#include "CoopCampaignTimeAuthority.h"
#include <cstdio>
using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
PeerIdentity Identity(unsigned n) { PeerIdentity p{}; p[0] = static_cast<std::uint8_t>(n); return p; }
void Codec()
{
	CoopCampaignTimeRequest r{0x0102030405060708ull, 9, 10, CoopCampaignTimeAction::ThirtyMinutes};
	CoopCampaignTimeRequestBytes bytes{}, expected{{'J','2','T','Q',
		static_cast<std::uint8_t>(CurrentProtocolVersion),static_cast<std::uint8_t>(CurrentProtocolVersion >> 8),3,0,
		8,7,6,5,4,3,2,1, 9,0,0,0,0,0,0,0, 10,0,0,0,0,0,0,0}};
	CHECK(EncodeCoopCampaignTimeRequest(r, bytes) && bytes == expected, "exact 32-byte request vector, no claimed peer identity");
	for (unsigned action = 1; action <= 4; ++action)
	{
		r.action = static_cast<CoopCampaignTimeAction>(action);
		CoopCampaignTimeRequest out;
		CHECK(EncodeCoopCampaignTimeRequest(r, bytes) && DecodeCoopCampaignTimeRequest(bytes.data(), bytes.size(), out) &&
			SameCoopCampaignTimeRequest(r, out), "every absolute action roundtrips");
		for (unsigned outcome = 1; outcome <= 6; ++outcome)
		{
			CoopCampaignTimeResult result{r, 11, static_cast<CoopCampaignTimeOutcome>(outcome)}, decoded;
			CoopCampaignTimeResultBytes encoded;
			CHECK(EncodeCoopCampaignTimeResult(result, encoded) && encoded[3] == 'R' && encoded[7] == outcome && encoded[32] == 11 &&
				DecodeCoopCampaignTimeResult(encoded.data(), encoded.size(), decoded) && SameCoopCampaignTimeRequest(r, decoded.request) &&
				decoded.outcome == result.outcome && decoded.controlRevision == 11, "exact result envelope roundtrips");
			for (std::size_t size = 0; size <= encoded.size() + 1; ++size)
				if (size != encoded.size()) CHECK(!DecodeCoopCampaignTimeResult(encoded.data(), size, decoded), "all nonexact result widths rejected");
			for (unsigned at : {0u,1u,2u,3u,4u,5u,6u,7u})
			{
				auto bad = encoded; bad[at] ^= 0xff;
				CHECK(!DecodeCoopCampaignTimeResult(bad.data(), bad.size(), decoded), "result header corruption rejected");
			}
			CHECK(!DecodeCoopCampaignTimeResult(nullptr, 40, decoded), "null result rejected");
		}
	}
	CoopCampaignTimeRequest output = r;
	for (std::size_t size = 0; size <= 33; ++size)
		if (size != 32) CHECK(!DecodeCoopCampaignTimeRequest(bytes.data(), size, output) && SameCoopCampaignTimeRequest(r, output), "nonexact requests leave output untouched");
	for (unsigned at : {0u,1u,2u,3u,4u,5u,6u,7u})
	{
		auto bad = bytes; bad[at] ^= 0xff;
		CHECK(!DecodeCoopCampaignTimeRequest(bad.data(), bad.size(), output) && SameCoopCampaignTimeRequest(r, output), "bad request header transactional");
	}
	for (unsigned at : {8u,16u,24u})
	{
		auto bad = bytes; std::fill(bad.begin() + at, bad.begin() + at + 8, 0);
		CHECK(!DecodeCoopCampaignTimeRequest(bad.data(), bad.size(), output), "zero session/revision/request rejected");
	}
	CHECK(!DecodeCoopCampaignTimeRequest(nullptr, 32, output), "null request rejected");
	r.action = static_cast<CoopCampaignTimeAction>(5);
	CHECK(!EncodeCoopCampaignTimeRequest(r, bytes), "super compression excluded");
	r.action = CoopCampaignTimeAction::Pause;
	CoopCampaignTimeResult bad{r, r.controlRevision, CoopCampaignTimeOutcome::Applied};
	CoopCampaignTimeResultBytes saved{}; saved.fill(0xee); auto encoded = saved;
	CHECK(!EncodeCoopCampaignTimeResult(bad, encoded) && encoded == saved, "applied requires consumed revision; invalid encoder transactional");
}
void Authority()
{
	CoopCampaignStatusLedger ledger; CHECK(ledger.beginSession(1), "begin session");
	CoopCampaignStatus clock; clock.phase = CoopCampaignPhase::Strategic;
	PeerIdentity identities[] = {Identity(1), Identity(2)};
	CoopCampaignTimeAuthority::Peer peers[] = {{identities[0], TransportPeer{1}}, {identities[1], TransportPeer{2}}};
	CHECK(ledger.observe(clock, identities, 2), "ready leader established");
	CoopCampaignTimeAuthority authority; CHECK(authority.reconcile(peers, 2), "bounded authenticated peers");
	unsigned executions = 0; bool nativeAllowed = true;
	auto apply = [&](CoopCampaignTimeAction) { ++executions; return nativeAllowed; };
	auto request = [&] { return CoopCampaignTimeRequest{1, ledger.value().timeControlRevision, 1, CoopCampaignTimeAction::FiveMinutes}; };
	auto r = request();
	CHECK(authority.submit(r, peers[1], true, true, ledger, apply) && executions == 0 &&
		authority.deliveries()[1].result.outcome == CoopCampaignTimeOutcome::NotLeader, "nonleader never invokes native clock");
	authority.delivered(1);
	CHECK(authority.submit(r, peers[0], true, true, ledger, apply) && executions == 1 &&
		authority.deliveries()[0].pending && ledger.value().timeControlRevision > r.controlRevision, "reserve receipt and consume revision before native application");
	auto newer = request(); newer.requestId = 2;
	CHECK(!authority.submit(newer, peers[0], true, true, ledger, apply) && executions == 1, "receipt backpressure prevents extra execution");
	CHECK(authority.submit(r, peers[0], true, true, ledger, apply) && executions == 1, "duplicate during backpressure does not execute");
	authority.delivered(0);
	CHECK(authority.submit(r, peers[0], true, true, ledger, apply) && executions == 1 &&
		authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::Applied, "duplicate after delivery returns historical result");
	authority.delivered(0);
	r.requestId = 3;
	CHECK(authority.submit(r, peers[0], true, true, ledger, apply) && executions == 1 &&
		authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::Stale, "consumed revision cannot undo a later native pause");
	authority.delivered(0);
	nativeAllowed = false; r = request(); r.requestId = 4;
	CHECK(authority.submit(r, peers[0], true, true, ledger, apply) && executions == 2 &&
		authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::NativeBlocked, "native refusal is an explicit consumed terminal outcome");
	authority.delivered(0); nativeAllowed = true;
	CHECK(authority.submit(r, peers[0], true, true, ledger, apply) && executions == 2 &&
		authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::NativeBlocked, "rejected resume never springs to life when native obstacle clears");
	authority.delivered(0);
	for (unsigned fault = 0; fault < 5; ++fault)
	{
		r = request(); r.requestId = 10 + fault;
		if (fault == 0) ++r.sessionEpoch;
		if (fault == 1) ++r.controlRevision;
		if (fault == 4) { clock.phase = CoopCampaignPhase::Tactical; CHECK(ledger.observe(clock, identities, 2), "tactical phase"); r.controlRevision = ledger.value().timeControlRevision; }
		CHECK(authority.submit(r, peers[0], fault != 2, fault != 3, ledger, apply) && executions == 2 &&
			authority.deliveries()[0].result.outcome != CoopCampaignTimeOutcome::Applied, "wrong epoch/revision, unready peer, loaded world and tactical phase cannot execute");
		authority.delivered(0);
	}
	clock.phase = CoopCampaignPhase::Strategic; CHECK(ledger.observe(clock, identities, 2), "return to strategic");
	r = request(); r.requestId = 20;
	CHECK(authority.submit(r, peers[0], true, true, ledger, apply), "old transport has a pending result");
	peers[0].transport = TransportPeer{3};
	CHECK(authority.reconcile(peers, 2) && !authority.deliveries()[0].pending &&
		!authority.submit(request(), {identities[0], TransportPeer{1}}, true, true, ledger, apply), "transport replacement drops old outcomes and rejects queued old transport commands");
	const auto revision = ledger.value().timeControlRevision;
	++clock.worldSeconds; CHECK(ledger.observe(clock, identities, 2) && ledger.value().timeControlRevision == revision, "ordinary tick does not stale UI request");
	clock.gamePaused = false; CHECK(ledger.observe(clock, identities, 2) && ledger.value().timeControlRevision > revision, "native pause transition invalidates old resumes");
	const auto before = ledger.value();
	CHECK(!ledger.consumeTimeControlRevision(before.timeControlRevision - 1) && SameCoopCampaignStatus(before, ledger.value()), "stale consume transactional");
	CHECK(ledger.observe(clock, identities + 1, 1) && ledger.value().timeLeader == identities[0] && !ledger.value().timeLeaderReady &&
		ledger.value().timeControlRevision > before.timeControlRevision, "offline leader invalidates resume without reassigning authority");
	CHECK(authority.reconcile(nullptr, 0) && !authority.deliveries()[0].pending && !authority.reconcile(peers, 5), "drain clears bounded results, oversized peers rejected");
	CHECK(authority.reconcile(peers, 2), "authenticated peers return");
	r = request(); r.requestId = 21;
	const auto beforeUnready = executions;
	CHECK(authority.submit(r, peers[0], false, true, ledger, apply) && authority.deliveries()[0].pending &&
		authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::NotReady && executions == beforeUnready &&
		authority.reconcile(peers, 2) && authority.deliveries()[0].pending,
		"same-transport campaign resync retains a rejected request outcome for delivery after Ready");
	const auto retained = authority.deliveries()[0].result;
	CoopCampaignTimeAuthority::Peer duplicated[] = {peers[0], peers[0]};
	CHECK(!authority.reconcile(duplicated, 2) && authority.deliveries()[0].pending &&
		SameCoopCampaignTimeRequest(retained.request, authority.deliveries()[0].result.request), "invalid peer reconciliation cannot erase a queued outcome");
}
void ArrivalHold()
{
	CoopCampaignStatusLedger ledger;
	CoopCampaignStatus clock; clock.phase = CoopCampaignPhase::Strategic;
	PeerIdentity identities[]{Identity(1), Identity(2)};
	CoopCampaignTimeAuthority::Peer peers[]{{identities[0], TransportPeer{1}}, {identities[1], TransportPeer{2}}};
	CoopCampaignTimeAuthority authority;
	CHECK(ledger.beginSession(3) && ledger.observe(clock, identities, 2) && authority.reconcile(peers, 2), "arrival time fixture ready");
	const CoopCampaignTimeRequest delayed{3, ledger.value().timeControlRevision, 1, CoopCampaignTimeAction::FiveMinutes};
	clock.arrival.decision = 1; clock.arrival.kind = CoopCampaignArrivalKind::WildernessNpc;
	clock.arrival.stage = CoopCampaignArrivalStage::Pending; clock.arrival.x = 10; clock.arrival.y = 1; clock.arrival.pendingCount = 1;
	CHECK(ledger.observe(clock, identities, 2), "native arrival pauses an already paused campaign");
	unsigned executions = 0; auto apply = [&](CoopCampaignTimeAction) { ++executions; return true; };
	CHECK(authority.submit(delayed, peers[0], true, true, ledger, apply) && !executions &&
		authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::Stale, "pre-arrival resume cannot undo the native decision hold");
	authority.delivered(0);
	for (unsigned action = 1; action <= 4; ++action)
	{
		const CoopCampaignTimeRequest request{3, ledger.value().timeControlRevision, action + 1, static_cast<CoopCampaignTimeAction>(action)};
		CHECK(authority.submit(request, peers[0], true, true, ledger, apply) && !executions &&
			authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::NativeBlocked,
			"even the time leader cannot use time controls to answer an arrival");
		authority.delivered(0);
	}
	const auto rejected = authority.deliveries()[0].result.request;
	clock.arrival = {};
	CHECK(ledger.observe(clock, identities, 2) && authority.submit(rejected, peers[0], true, true, ledger, apply) && !executions &&
		authority.deliveries()[0].result.outcome == CoopCampaignTimeOutcome::NativeBlocked, "resolved arrival cannot turn a duplicate rejected request into a resume");
	authority.delivered(0);
	const CoopCampaignTimeRequest fresh{3, ledger.value().timeControlRevision, 10, CoopCampaignTimeAction::FiveMinutes};
	CHECK(authority.submit(fresh, peers[0], true, true, ledger, apply) && executions == 1, "only a fresh authorized time request can resume after resolution");
}
}
int main() { Codec(); Authority(); ArrivalHold(); return failures ? 1 : 0; }
