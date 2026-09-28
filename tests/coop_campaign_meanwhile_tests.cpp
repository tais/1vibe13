#include "CoopCampaignActionAuthority.h"
#include "CoopCampaignTime.h"
#include <Ja2/FullEngineCoopClientMeanwhileInput.h>
#include <Ja2/FullEngineCoopClientCampaignActionInput.h>
#include <cstdio>
#include <cwchar>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
PeerIdentity Peer(unsigned n) { PeerIdentity p{}; p[0] = n; return p; }
CoopCampaignStatus Status()
{
	CoopCampaignStatus s; s.sessionEpoch = 7; s.revision = 1; s.phase = CoopCampaignPhase::Strategic;
	s.pauseLocked = true; s.meanwhile = {0x0807060504030201ull, CoopCampaignMeanwhileScene::FirstBattle};
	return s;
}
CoopCampaignGroups Groups()
{ CoopCampaignGroups g; g.sessionEpoch = 7; g.revision = 1; return g; }
void Observation()
{
	auto s = Status(); CoopCampaignStatusBytes bytes; CoopCampaignStatus decoded;
	CHECK(EncodeCoopCampaignStatus(s, bytes) && bytes.size() == 128 && bytes[112] == 1 && bytes[119] == 8 && bytes[120] == 1 &&
		DecodeCoopCampaignStatus(bytes.data(), bytes.size(), decoded) && SameCoopCampaignStatus(s, decoded),
		"scene and exact runtime identity roundtrip in protocol 19's 128-byte observation");
	for (unsigned scene = 1; scene <= 17; ++scene)
	{
		s.meanwhile.scene = static_cast<CoopCampaignMeanwhileScene>(scene);
		CHECK(EncodeCoopCampaignStatus(s, bytes) && DecodeCoopCampaignStatus(bytes.data(), bytes.size(), decoded) &&
			SameCoopCampaignStatus(s, decoded) && std::wcscmp(FullEngineCoopClientMeanwhileTitle(s.meanwhile.scene), L"Campaign scene"),
			"every native scene has a bounded explicit wire tag and player-facing title");
	}
	for (unsigned at : {109u,110u,111u,121u,122u,123u,124u,125u,126u,127u})
	{
		auto bad = bytes; bad[at] = 1; decoded = s;
		CHECK(!DecodeCoopCampaignStatus(bad.data(),bad.size(),decoded) && SameCoopCampaignStatus(s,decoded),
			"reserved bytes reject transactionally");
	}
	for (unsigned fault = 0; fault < 9; ++fault)
	{
		auto bad = s;
		if (fault == 0) bad.meanwhile.id = 0;
		if (fault == 1) bad.meanwhile.scene = CoopCampaignMeanwhileScene::None;
		if (fault == 2) bad.meanwhile.scene = static_cast<CoopCampaignMeanwhileScene>(18);
		if (fault == 3) bad.gamePaused = false;
		if (fault == 4) bad.pauseLocked = false;
		if (fault == 5) bad.phase = CoopCampaignPhase::Transition;
		if (fault == 6) { bad.phase = CoopCampaignPhase::Tactical; bad.surrenderOffer = 4; }
		if (fault == 7) { bad.phase = CoopCampaignPhase::Tactical; bad.battleNotice = {4,CoopCampaignBattleNoticeKind::Defeated,9,1,0}; }
		if (fault == 8) { bad.arrival.decision = 4; bad.arrival.kind = CoopCampaignArrivalKind::WildernessNpc; bad.arrival.stage = CoopCampaignArrivalStage::Pending; bad.arrival.x = 9; bad.arrival.y = 1; bad.arrival.pendingCount = 1; }
		auto unchanged = bytes;
		CHECK(!EncodeCoopCampaignStatus(bad, bytes) && bytes == unchanged, "scene owns an exclusive locked, paused decision");
	}
	CoopCampaignStatusLedger ledger; const PeerIdentity ready[] = {Peer(1),Peer(2)};
	CHECK(ledger.beginSession(7) && ledger.observe(s,ready,2), "held scene observed");
	const auto original = s.meanwhile;
	s.meanwhile.scene = CoopCampaignMeanwhileScene::FirstBattle;
	CHECK(!ledger.observe(s,ready,2), "same identity cannot change scene");
	s.meanwhile = {}; const auto revision = ledger.value().timeControlRevision;
	CHECK(ledger.observe(s,ready,2) && ledger.value().timeControlRevision > revision, "consumption invalidates previously prepared controls");
	s.meanwhile = original;
	CHECK(!ledger.observe(s,ready,2), "consumed scene cannot resurrect");
	++s.meanwhile.id; CHECK(ledger.observe(s,ready,2), "later native scene requires increasing identity");
}
void Serialization()
{
	const PeerIdentity ready[] = {Peer(1),Peer(2)};
	const CoopCampaignActionAuthority::Peer peers[] = {{ready[0],{10}}, {ready[1],{20}}};
	CoopCampaignStatusLedger ledger; auto clock = Status(); const auto groups = Groups();
	CHECK(ledger.beginSession(7) && ledger.observe(clock,ready,2), "scene ledger ready");
	CoopCampaignActionRequest request; request.sessionEpoch = 7; request.controlRevision = ledger.value().timeControlRevision;
	request.groupsRevision = 1; request.requestId = 1; request.decision = clock.meanwhile.id; request.action = CoopCampaignAction::SkipMeanwhile;
	CHECK(ValidateCoopCampaignActionRequest(request,ledger.value(),groups,true,true,true) == CoopCampaignActionOutcome::Applied &&
		ValidateCoopCampaignActionRequest(request,ledger.value(),groups,false,true,true) == CoopCampaignActionOutcome::NotReady &&
		ValidateCoopCampaignActionRequest(request,ledger.value(),groups,true,false,true) == CoopCampaignActionOutcome::Unauthorized,
		"scene skipping requires ready shared authority independent of leadership");
	auto unrelated = request; unrelated.action = CoopCampaignAction::AcknowledgeArrival;
	CHECK(ValidateCoopCampaignActionRequest(unrelated,ledger.value(),groups,true,true,true) == CoopCampaignActionOutcome::Unavailable,
		"other campaign choices cannot bypass a held scene");
	CoopCampaignTimeRequest time{7,request.controlRevision,1,CoopCampaignTimeAction::SixtyMinutes};
	CHECK(ValidateCoopCampaignTimeRequest(time,ledger.value(),ready[0],true,true) == CoopCampaignTimeOutcome::NativeBlocked,
		"even the designated time leader cannot resume across a scene");
	CoopCampaignActionAuthority authority; unsigned executions = 0;
	CHECK(authority.reconcile(peers,2), "both transports authenticated");
	auto apply = [&](const auto&) { ++executions; return CoopCampaignActionNativeResult{CoopCampaignActionOutcome::Applied,0}; };
	CHECK(authority.submit(request,peers[1],true,true,true,ledger,groups,apply) && executions == 1,
		"nonleader skip consumes the common revision before native acknowledgement");
	CHECK(authority.submit(request,peers[0],true,true,true,ledger,groups,apply) && executions == 1 &&
		authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale, "competing ready player cannot skip twice");
	authority.delivered(1);
	CHECK(authority.submit(request,peers[1],true,true,true,ledger,groups,apply) && executions == 1 &&
		authority.deliveries()[1].result.outcome == CoopCampaignActionOutcome::Applied,
		"identical retry resends its retained receipt without repeating native effects");
	clock = ledger.value(); clock.meanwhile = {}; clock.phase = CoopCampaignPhase::Transition;
	CHECK(ledger.observe(clock,ready,2), "acknowledged scene stays in transition until native completion");
	authority.delivered(0); ++request.requestId; request.controlRevision = ledger.value().timeControlRevision;
	CHECK(authority.submit(request,peers[0],true,true,true,ledger,groups,apply) && executions == 1 &&
		authority.deliveries()[0].result.outcome == CoopCampaignActionOutcome::Stale, "consumed native ID cannot be skipped again with fresh request lineage");
}
void Input()
{
	using Key = FullEngineCoopClientMeanwhileInput::Key;
	auto s = Status(); auto g = Groups(); FullEngineCoopClientMeanwhileInput input;
	auto key = [&](Key k, bool down = true, bool up = false) { return input.handle(k,down,up,&s,&g,true,false); };
	CHECK(!key(Key::Confirm) && !key(Key::Skip) && input.armed() == 10 && !key(Key::Confirm),
		"Skip requires a separately pressed Enter; held confirmation cannot approve a later selection");
	(void)key(Key::Confirm,false,true); const auto answer = key(Key::Confirm);
	CHECK(answer && answer->action == CoopCampaignAction::SkipMeanwhile && answer->decision == s.meanwhile.id &&
		!answer->sessionEpoch && !answer->requestId && !key(Key::Confirm), "presentation submits only the explicit scene; core binds identity and lineage");
	for (unsigned fault = 0; fault != 9; ++fault)
	{
		auto changed = s; auto groups = g; FullEngineCoopClientMeanwhileInput held;
		(void)held.handle(Key::Skip,true,false,&changed,&groups,true,false);
		if (fault == 0) ++changed.timeControlRevision;
		if (fault == 1) ++groups.revision;
		if (fault == 2) ++changed.meanwhile.id;
		if (fault == 3) ++changed.sessionEpoch;
		if (fault == 4) changed.meanwhile = {};
		if (fault == 5) held.reset();
		if (fault == 6) (void)held.handle(Key::Cancel,true,false,&changed,&groups,true,false);
		CHECK(!held.handle(Key::Confirm,true,false,&changed,&groups,fault != 7,fault == 8) && !held.armed(),
			"stale scene/control/groups, reconnect, cancellation and outstanding requests discard armed skips");
	}
	FullEngineCoopClientCampaignActionInput travel;
	CHECK(!travel.handle(FullEngineCoopClientCampaignActionKey::EnterBattle,true,false,&s,&g,true,false),
		"ordinary campaign input stays disabled behind the scene");
}
}
int main() { Observation(); Serialization(); Input(); return failures ? 1 : 0; }
