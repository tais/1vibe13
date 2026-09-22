#include "CoopCampaignStatus.h"
#include <cstdio>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
PeerIdentity Peer(unsigned n) { PeerIdentity p{}; p[0] = static_cast<std::uint8_t>(n); return p; }
CoopCampaignStatus Sample()
{
	CoopCampaignStatus s;
	s.sessionEpoch = 0x0102030405060708ull; s.revision = 9; s.worldSeconds = 111661;
	s.phase = CoopCampaignPhase::Tactical; s.timeLeader = Peer(3);
	s.leadershipRevision = 1; s.timeLeaderReady = true; s.readyPeers = 2;
	return s;
}
void TestCodec()
{
	auto s = Sample();
	CoopCampaignStatusBytes bytes{};
	CHECK(EncodeCoopCampaignStatus(s, bytes), "valid status encodes");
	CoopCampaignStatusBytes expected{{
		'J','2','C','T',
		static_cast<std::uint8_t>(CurrentProtocolVersion),static_cast<std::uint8_t>(CurrentProtocolVersion >> 8),1,0, 8,7,6,5,4,3,2,1,
		9,0,0,0,0,0,0,0, 0x2d,0xb4,1,0, 3,1,17,2,
		3,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
		1,0,0,0,0,0,0,0, 1,0,0,0,0,0,0,0}};
	CHECK(bytes == expected, "exact 96-byte little endian wire layout, canonical empty arrival tail");
	CoopCampaignStatus out;
	CHECK(DecodeCoopCampaignStatus(bytes.data(), bytes.size(), out) && SameCoopCampaignStatus(s, out),
		"status roundtrips exactly");
	for (unsigned phase = 1; phase <= 4; ++phase)
		for (int mode = -1; mode <= 5; ++mode)
			for (unsigned flags = 0; flags != 16; ++flags)
			{
				auto candidate = s; candidate.phase = static_cast<CoopCampaignPhase>(phase);
				candidate.compressionMode = static_cast<std::int8_t>(mode);
				candidate.gamePaused = flags & 1; candidate.pauseLocked = flags & 2;
				candidate.compressionActive = flags & 4; candidate.timeInterrupted = flags & 8;
				const bool valid = !candidate.compressionActive || (!candidate.gamePaused && mode != 0);
				auto encoded = expected;
				CHECK(EncodeCoopCampaignStatus(candidate, encoded) == valid, "native flags validated");
				if (valid) CHECK(DecodeCoopCampaignStatus(encoded.data(), encoded.size(), out) &&
					SameCoopCampaignStatus(candidate, out), "all valid modes and flags roundtrip");
				else CHECK(encoded == expected, "invalid encoder leaves output untouched");
			}
	for (std::size_t size = 0; size <= bytes.size() + 1; ++size)
	{
		if (size == bytes.size()) continue;
		out = s;
		CHECK(!DecodeCoopCampaignStatus(bytes.data(), size, out) && SameCoopCampaignStatus(s, out),
			"every non-exact width rejected transactionally without reading beyond buffer");
	}
	CHECK(!DecodeCoopCampaignStatus(nullptr, bytes.size(), out), "null buffer rejected");
	for (unsigned at : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u})
	{
		auto bad = bytes; bad[at] ^= 0xff; out = s;
		CHECK(!DecodeCoopCampaignStatus(bad.data(), bad.size(), out) && SameCoopCampaignStatus(s, out),
			"wrong magic/version/kind/reserved rejected transactionally");
	}
	for (unsigned kind = 0; kind != 11; ++kind)
	{
		auto bad = bytes; out = s;
		switch (kind)
		{
			case 0: std::fill(bad.begin() + 8, bad.begin() + 16, 0); break;
			case 1: std::fill(bad.begin() + 16, bad.begin() + 24, 0); break;
			case 2: bad[28] = 0; break;
			case 3: bad[28] = 5; break;
			case 4: bad[29] = 7; break;
			case 5: bad[30] |= 32; break;
			case 6: bad[31] = 5; break;
			case 7: bad[31] = 0; break;
			case 8: bad[32] = 0; break;
			case 9: bad[48] = 0; break;
			case 10: bad[56] = 0; break;
		}
		CHECK(!DecodeCoopCampaignStatus(bad.data(), bad.size(), out) && SameCoopCampaignStatus(s, out),
			"semantic corruption rejected without replacing committed observation");
	}
}
void TestLedger()
{
	CoopCampaignStatusLedger ledger;
	auto clock = Sample(); const PeerIdentity first[] = {Peer(3), Peer(8)};
	CHECK(!ledger.observe(clock, first, 2) && !ledger.beginSession(0), "epoch required");
	CHECK(ledger.beginSession(30) && !ledger.beginSession(31), "session cannot silently change");
	CHECK(ledger.observe(clock, nullptr, 0) && ledger.value().revision == 1 &&
		IsZero(ledger.value().timeLeader), "no leader before any campaign-ready peer");
	CHECK(ledger.observe(clock, first, 2) && ledger.value().timeLeader == Peer(3) &&
		ledger.value().leadershipRevision == 1 && ledger.value().revision == 2,
		"first ready cohort selects deterministic lowest identity, not caller-supplied leadership");
	const auto established = ledger.value();
	CHECK(ledger.observe(clock, first, 2) && SameCoopCampaignStatus(established, ledger.value()),
		"unchanged state coalesces without revision churn");
	const PeerIdentity later[] = {Peer(1), Peer(8)};
	CHECK(ledger.observe(clock, later, 2) && ledger.value().timeLeader == Peer(3) &&
		!ledger.value().timeLeaderReady && ledger.value().leadershipRevision == 1,
		"later lower identity cannot steal an offline or retired leader's role");
	CHECK(ledger.observe(clock, nullptr, 0) && !ledger.value().timeLeaderReady &&
		ledger.value().timeLeader == Peer(3), "complete transport/world drain retains session leader");
	clock.phase = CoopCampaignPhase::Strategic; clock.worldSeconds += 60;
	CHECK(ledger.observe(clock, first, 2) && ledger.value().timeLeaderReady &&
		ledger.value().timeLeader == Peer(3) && ledger.value().leadershipRevision == 1,
		"same identity regains readiness after reconnect and tactical-to-strategic transition");
	const auto before = ledger.value();
	const PeerIdentity reversed[] = {Peer(8), Peer(3)}, duplicate[] = {Peer(3), Peer(3)}, zero[] = {{}};
	CHECK(!ledger.observe(clock, reversed, 2) && !ledger.observe(clock, duplicate, 2) &&
		!ledger.observe(clock, zero, 1) && !ledger.observe(clock, nullptr, 1) &&
		!ledger.observe(clock, first, 5), "bounded sorted nonzero ready identities required");
	--clock.worldSeconds;
	CHECK(!ledger.observe(clock, first, 2) && SameCoopCampaignStatus(before, ledger.value()),
		"clock rollback and rejected observations leave ledger unchanged");
	ledger.clear();
	CHECK(ledger.beginSession(31) && ledger.observe(clock, later, 2) && ledger.value().timeLeader == Peer(1) &&
		ledger.value().revision == 1, "new server session starts new leadership and clock lineage");
}
CoopCampaignArrival Battle()
{
	CoopCampaignArrival a;
	a.decision = 0x0102030405060708ull; a.kind = CoopCampaignArrivalKind::Battle;
	a.stage = CoopCampaignArrivalStage::Prepared; a.x = 10; a.y = 1; a.pendingCount = 2;
	a.involvedMercs = 4; a.uninvolvedMercs = 6; a.encounterCode = 2;
	a.nativeEnterSector = a.nativeRetreat = a.nativePlacement = true;
	return a;
}
void TestArrivalCodec()
{
	using Kind = CoopCampaignArrivalKind; using Stage = CoopCampaignArrivalStage;
	auto s = Sample(); s.phase = CoopCampaignPhase::Strategic; s.arrival = Battle();
	CoopCampaignStatusBytes bytes;
	CHECK(EncodeCoopCampaignStatus(s, bytes), "prepared native encounter encodes");
	const std::array<std::uint8_t, 32> expected{{8,7,6,5,4,3,2,1, 3,2,28,10,1,0,2,0, 2,0,4,0,6,0}};
	CHECK(std::equal(expected.begin(), expected.end(), bytes.begin() + 64), "exact arrival tail excludes native pointers, opponents and command authority");
	CoopCampaignStatus out;
	CHECK(DecodeCoopCampaignStatus(bytes.data(), bytes.size(), out) && SameCoopCampaignStatus(s, out), "arrival metadata roundtrips with the coherent clock");
	for (unsigned at : {74u,79u,86u,87u,88u,89u,90u,91u,92u,93u,94u,95u})
	{
		auto bad = bytes; bad[at] |= 32; out = s;
		CHECK(!DecodeCoopCampaignStatus(bad.data(), bad.size(), out) && SameCoopCampaignStatus(s, out), "reserved arrival bits/bytes rejected transactionally");
	}
	for (unsigned fault = 0; fault < 18; ++fault)
	{
		auto bad = s;
		switch (fault)
		{
			case 0: bad.arrival.decision = 0; break;
			case 1: bad.arrival.kind = Kind::None; break;
			case 2: bad.arrival.kind = static_cast<Kind>(4); break;
			case 3: bad.arrival.stage = Stage::None; break;
			case 4: bad.arrival.stage = static_cast<Stage>(6); break;
			case 5: bad.arrival.x = 0; break;
			case 6: bad.arrival.y = 17; break;
			case 7: bad.arrival.z = 4; break;
			case 8: bad.arrival.pendingCount = 0; break;
			case 9: bad.arrival.pendingCount = 257; break;
			case 10: bad.arrival.involvedMercs = 0; break;
			case 11: bad.arrival.uninvolvedMercs = 256; break;
			case 12: bad.arrival.nativeEnterSector = false; break;
			case 13: bad.arrival.finalDestination = true; break;
			case 14: bad.arrival.stage = Stage::Pending; break;
			case 15: bad.phase = CoopCampaignPhase::Tactical; break;
			case 16: bad.phase = CoopCampaignPhase::Transition; break;
			case 17: bad.gamePaused = false; break;
		}
		auto encoded = bytes;
		CHECK(!EncodeCoopCampaignStatus(bad, encoded) && encoded == bytes, "invalid native decision/clock combination cannot replace the observation");
	}
	for (unsigned kind = 1; kind <= 3; ++kind)
		for (unsigned stage = 1; stage <= 5; ++stage)
		{
			auto candidate = s; auto& a = candidate.arrival;
			a = {}; a.decision = UINT64_MAX; a.kind = static_cast<Kind>(kind); a.stage = static_cast<Stage>(stage);
			a.x = a.y = 16; a.z = 3; a.pendingCount = 256;
			if (stage == 2) { a.involvedMercs = 256; a.nativeEnterSector = true; }
			const bool valid = kind == 3 || (stage != 2 && stage != 3);
			CHECK(EncodeCoopCampaignStatus(candidate, bytes) == valid, "only battles can be prepared or need militia reinforcement decisions");
			if (!valid) continue;
			CHECK(DecodeCoopCampaignStatus(bytes.data(), bytes.size(), out) && SameCoopCampaignStatus(candidate, out), "all supported decision kinds/stages roundtrip");
		}
}
void TestArrivalLedger()
{
	CoopCampaignStatusLedger ledger; const PeerIdentity ready[]{Peer(3)};
	auto clock = Sample(); clock.phase = CoopCampaignPhase::Strategic;
	CHECK(ledger.beginSession(1) && ledger.observe(clock, ready, 1), "arrival ledger begins with a paused campaign");
	const auto idle = ledger.value(); clock.arrival = Battle();
	CHECK(ledger.observe(clock, ready, 1) && ledger.value().revision > idle.revision &&
		ledger.value().timeControlRevision > idle.timeControlRevision, "arrival invalidates a delayed resume even when the clock was already paused");
	const auto prepared = ledger.value();
	CHECK(ledger.observe(clock, ready, 1) && SameCoopCampaignStatus(prepared, ledger.value()), "read-only repeated arrival coalesces");
	++clock.arrival.pendingCount;
	CHECK(ledger.observe(clock, ready, 1) && ledger.value().revision > prepared.revision, "additional queued decisions publish without pretending to be the front choice");
	clock.arrival.x = 11; const auto before = ledger.value();
	CHECK(!ledger.observe(clock, ready, 1) && SameCoopCampaignStatus(before, ledger.value()), "same decision cannot silently change sector");
	clock.arrival = {};
	CHECK(ledger.observe(clock, ready, 1) && ledger.value().timeControlRevision > before.timeControlRevision && !ledger.value().arrival.decision,
		"resolution clears the notice and invalidates all requests made during that decision");
	clock.arrival = Battle();
	CHECK(!ledger.observe(clock, ready, 1), "resolved decision cannot resurrect under a later status revision");
	++clock.arrival.decision;
	CHECK(ledger.observe(clock, ready, 1), "next native queue identity can be published");
	ledger.clear();
	CHECK(ledger.beginSession(2) && ledger.observe(clock, ready, 1), "new server session owns a new decision lineage");
}
}
int main() { TestCodec(); TestLedger(); TestArrivalCodec(); TestArrivalLedger(); return failures ? 1 : 0; }
