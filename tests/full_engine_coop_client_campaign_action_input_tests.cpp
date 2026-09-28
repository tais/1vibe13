#include <Ja2/FullEngineCoopClientCampaignActionInput.h>
#include <Ja2/FullEngineCoopClientSurrenderInput.h>
#include <Ja2/FullEngineCoopClientBattleNoticeInput.h>
#include <cstdio>

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
using Key = FullEngineCoopClientCampaignActionKey;
struct Fixture
{
	FullEngineCoopClientCampaignActionInput input;
	CoopCampaignStatus status;
	CoopCampaignGroups groups;
	bool enabled = true, pending = false;
	Fixture()
	{
		status.sessionEpoch = 7; status.revision = 1; status.phase = CoopCampaignPhase::Strategic;
		groups.sessionEpoch = 7; groups.revision = 1; groups.available = true;
		groups.groupCount = groups.memberCount = 2;
		groups.groups[0].id = {1, 10}; groups.groups[0].x = 9; groups.groups[0].y = 1;
		groups.groups[0].memberCount = 1;
		groups.groups[1].id = {2, 20}; groups.groups[1].x = 4; groups.groups[1].y = 8;
		groups.groups[1].firstMember = groups.groups[1].memberCount = 1;
		groups.members[0].actor = {1, 1}; groups.members[1].actor = {2, 1};
	}
	auto key(Key key, bool down = true, bool up = false)
	{ return input.handle(key, down, up, &status, &groups, enabled, pending); }
	auto press(Key key)
	{ (void)this->key(key, false, true); return this->key(key); }
	void battle(bool placement = false)
	{
		status.arrival.decision = 3; status.arrival.kind = CoopCampaignArrivalKind::Battle;
		status.arrival.stage = CoopCampaignArrivalStage::Prepared;
		status.arrival.x = 10; status.arrival.y = 1; status.arrival.pendingCount = 1;
		status.arrival.involvedMercs = 2; status.arrival.nativeEnterSector = true;
		status.arrival.nativeRetreat = true; status.arrival.nativePlacement = placement;
	}
};

void Travel()
{
	Fixture f;
	CHECK(!f.press(Key::Confirm), "confirmation without an explicit destination sends nothing");
	CHECK(!f.press(Key::East) && f.input.destinationX() == 10 && f.input.destinationY() == 1,
		"east selects only one adjacent sector from the observed group");
	auto request = f.press(Key::Confirm);
	CHECK(request && request->action == CoopCampaignAction::Travel && request->group == (StrategicGroupId{1, 10}) &&
		request->destinationX == 10 && request->destinationY == 1 && !request->decision && !request->sessionEpoch &&
		!request->requestId && !request->controlRevision && !request->groupsRevision && !f.input.destinationX(),
		"travel carries exact selected identity/destination; core owns authority fields");
	CHECK(!f.key(Key::Confirm) && !f.press(Key::Confirm), "held or fresh confirm cannot reuse consumed destination");
	(void)f.press(Key::North);
	CHECK(!f.input.destinationX(), "north map boundary cannot wrap to another sector");
	(void)f.press(Key::South); (void)f.press(Key::Cancel);
	CHECK(!f.press(Key::Confirm), "cancelled destination cannot be submitted");
	(void)f.press(Key::East); (void)f.press(Key::NextGroup);
	CHECK(f.input.selected() == (StrategicGroupId{2, 20}) && !f.input.destinationX(), "cycling exact groups clears destination");
	(void)f.press(Key::West); request = f.press(Key::Confirm);
	CHECK(request && request->group == (StrategicGroupId{2, 20}) && request->destinationX == 3 && request->destinationY == 8,
		"travel follows the newly selected group's source");
	(void)f.press(Key::NextGroup);
	CHECK(f.input.selected() == (StrategicGroupId{1, 10}), "forward selection wraps");
	(void)f.press(Key::PreviousGroup);
	CHECK(f.input.selected() == (StrategicGroupId{2, 20}), "backward selection wraps");
	for (unsigned restriction = 0; restriction < 4; ++restriction)
	{
		Fixture blocked;
		auto& group = blocked.groups.groups[0];
		if (restriction == 0) { group.betweenSectors = true; group.nextX = 10; group.nextY = 1; }
		if (restriction == 1) group.vehicle = true;
		if (restriction == 2) group.z = 1;
		if (restriction == 3) { group.destinationX = 10; group.destinationY = 1; }
		(void)blocked.press(Key::East);
		CHECK(!blocked.press(Key::Confirm), "transit, vehicle, underground and retained-route groups have no new travel action");
	}
}

void StaleAndPending()
{
	for (unsigned change = 0; change < 8; ++change)
	{
		Fixture f; (void)f.press(Key::East);
		if (change == 0) ++f.groups.revision;
		if (change == 1) ++f.status.timeControlRevision;
		if (change == 2) ++f.status.sessionEpoch;
		if (change == 3) { f.groups.available = false; f.groups.groupCount = f.groups.memberCount = 0; }
		if (change == 4) f.groups.groups[0].id.incarnation = 11;
		if (change == 5) f.pending = true;
		if (change == 6) f.enabled = false;
		if (change == 7) f.status.phase = CoopCampaignPhase::Tactical;
		CHECK(!f.press(Key::Confirm) && !f.input.destinationX(), "changed context or outstanding request invalidates confirmation");
	}
	Fixture clock;
	(void)clock.press(Key::East); ++clock.status.worldSeconds; ++clock.status.revision;
	CHECK(clock.press(Key::Confirm).has_value(), "ordinary authoritative clock tick does not change travel selection");
	Fixture held;
	held.pending = true; (void)held.key(Key::East);
	held.pending = false; (void)held.key(Key::East);
	CHECK(!held.input.destinationX(), "key held while request pending cannot arm travel after receipt");
	(void)held.press(Key::East);
	CHECK(held.input.destinationX() == 10, "fresh released press can choose again");
	held.input.synchronize(nullptr, nullptr, false, false);
	(void)held.key(Key::East);
	CHECK(!held.input.destinationX(), "disconnect clears selection but retains held-key suppression");
	(void)held.press(Key::East);
	CHECK(held.press(Key::Confirm).has_value(), "fresh choice after reconnect binds current observation");
	Fixture malformed;
	malformed.groups.groupCount = 256;
	(void)malformed.press(Key::East);
	CHECK(!malformed.press(Key::Confirm), "invalid copied groups fail closed without indexing past capacity");
}

void Arrival()
{
	Fixture f;
	f.status.arrival.decision = 1; f.status.arrival.kind = CoopCampaignArrivalKind::WildernessNpc;
	f.status.arrival.stage = CoopCampaignArrivalStage::Pending;
	f.status.arrival.x = 10; f.status.arrival.y = 1; f.status.arrival.pendingCount = 1;
	CHECK(!f.press(Key::Acknowledge), "interrupted route requires stop, not destination acknowledgment");
	auto request = f.press(Key::Stop);
	CHECK(request && request->action == CoopCampaignAction::StopArrival && request->decision == 1 && !request->group.valid(),
		"NPC stop is scoped to exact observed decision");
	f.status.arrival.finalDestination = true;
	CHECK(!f.press(Key::Stop), "destination notice does not offer route stop");
	request = f.press(Key::Acknowledge);
	CHECK(request && request->action == CoopCampaignAction::AcknowledgeArrival, "final NPC arrival can be acknowledged");
	for (bool placement : {false, true})
	{
		Fixture battle; battle.battle(placement);
		request = battle.press(Key::EnterBattle);
		CHECK(request && request->decision == 3 && request->action == (placement ? CoopCampaignAction::EnterArrivalSpread
			: CoopCampaignAction::EnterArrivalForced), "battle entry chooses observed native insertion policy");
		CHECK(!battle.key(Key::EnterBattle), "held entry key cannot resubmit after a result");
		(void)battle.press(Key::Retreat);
		CHECK(battle.input.retreatArmed(), "retreat requires an explicit confirm");
		request = battle.press(Key::Confirm);
		CHECK(request && request->action == CoopCampaignAction::RetreatArrival && !battle.input.retreatArmed(),
			"retreat confirmation consumes its exact decision");
		(void)battle.press(Key::Retreat); ++battle.status.arrival.decision;
		CHECK(!battle.press(Key::Confirm), "retreat cannot apply to a replacement encounter");
		battle.status.arrival.nativeRetreat = false;
		(void)battle.press(Key::Retreat);
		CHECK(!battle.press(Key::Confirm), "native retreat prohibition is respected");
		battle.status.arrival.nativePlacement = false; battle.status.arrival.nativeEnterSector = false;
		CHECK(!battle.press(Key::EnterBattle), "native entry prohibition is respected");
	}
	Fixture unsupported; unsupported.battle();
	unsupported.status.arrival.stage = CoopCampaignArrivalStage::Unsupported;
	unsupported.status.arrival.involvedMercs = 0; unsupported.status.arrival.nativeEnterSector = false;
	unsupported.status.arrival.nativeRetreat = false;
	CHECK(!unsupported.press(Key::EnterBattle) && !unsupported.press(Key::Acknowledge) && !unsupported.press(Key::Stop),
		"unsupported native arrival has no invented fallback decision");
	Fixture noGroups; noGroups.battle();
	noGroups.groups = {};
	CHECK(!noGroups.press(Key::EnterBattle), "arrival submission waits for a current groups revision to bind its wire request");
	noGroups.groups.sessionEpoch = noGroups.status.sessionEpoch; noGroups.groups.revision = 1;
	CHECK(noGroups.press(Key::EnterBattle).has_value(), "arrival needs a groups revision but does not require an available roster");
	Fixture multiple; multiple.battle(); multiple.status.arrival.pendingCount = 2;
	CHECK(!multiple.press(Key::EnterBattle), "native battle actions cannot bypass multiple pending encounters");
}
void BattleNoticeConfirmation()
{
	using K = FullEngineCoopClientBattleNoticeInput::Key;
	Fixture f; FullEngineCoopClientBattleNoticeInput input;
	f.status.phase = CoopCampaignPhase::Tactical;
	f.status.battleNotice = {11, CoopCampaignBattleNoticeKind::Defeated, 9, 1, 0};
	auto key = [&](K k, bool down = true, bool up = false) { return input.handle(k, down, up, &f.status, &f.groups, f.enabled, f.pending); };
	CHECK(!key(K::Confirm), "Enter alone cannot acknowledge an unselected native outcome");
	CHECK(!key(K::Continue) && input.armed() == 9, "explicit Continue still requires confirmation");
	CHECK(!key(K::Confirm), "held Enter cannot confirm a later Continue choice");
	(void)key(K::Confirm, false, true);
	const auto answer = key(K::Confirm);
	CHECK(answer && answer->action == CoopCampaignAction::AcknowledgeBattleNotice && answer->decision == 11 &&
		!answer->requestId && !answer->sessionEpoch && !answer->controlRevision && !input.armed(),
		"new Enter submits only the displayed native notice; core supplies authenticated request lineage");
	CHECK(!key(K::Confirm), "held key cannot replay the acknowledgement");
	for (unsigned change = 0; change != 8; ++change)
	{
		Fixture changed; FullEngineCoopClientBattleNoticeInput confirm;
		changed.status.phase = CoopCampaignPhase::Tactical; changed.status.battleNotice = f.status.battleNotice;
		(void)confirm.handle(K::Continue, true, false, &changed.status, &changed.groups, true, false);
		if (change == 0) ++changed.status.timeControlRevision;
		if (change == 1) ++changed.groups.revision;
		if (change == 2) ++changed.status.battleNotice.id;
		if (change == 3) ++changed.status.sessionEpoch;
		if (change == 4) changed.status.battleNotice = {};
		if (change == 5) changed.enabled = false;
		if (change == 6) changed.pending = true;
		if (change == 7) confirm.reset();
		CHECK(!confirm.handle(K::Confirm, true, false, &changed.status, &changed.groups, changed.enabled, changed.pending) && !confirm.armed(),
			"stale status, reconnect or blocked input cannot acknowledge the retained choice");
	}
	input.reset(); (void)key(K::Continue); (void)key(K::Cancel);
	CHECK(!key(K::Confirm), "cancel removes the acknowledgement choice");
}

void SurrenderConfirmation()
{
	using K = FullEngineCoopClientSurrenderInput::Key;
	for (const auto choice : {K::Fight, K::Surrender})
	{
		Fixture f; FullEngineCoopClientSurrenderInput input;
		f.status.phase = CoopCampaignPhase::Tactical; f.status.surrenderOffer = 11;
		auto key = [&](K k, bool down = true, bool up = false) { return input.handle(k, down, up, &f.status, &f.groups, f.enabled, f.pending); };
		CHECK(!key(K::Confirm), "unselected confirmation cannot answer surrender");
		CHECK(!key(choice) && input.armed(), "both choices require explicit confirmation");
		CHECK(!key(K::Confirm), "held Enter cannot confirm a later choice");
		(void)key(K::Confirm, false, true);
		const auto answer = key(K::Confirm);
		CHECK(answer && answer->decision == 11 && answer->action == (choice == K::Fight ?
			CoopCampaignAction::DeclineSurrender : CoopCampaignAction::AcceptSurrender) &&
			!answer->sessionEpoch && !answer->controlRevision && !answer->requestId && !input.armed(),
			"released Enter emits only the exact displayed choice; Client owns request lineage");
		CHECK(!key(K::Confirm), "held confirmation cannot replay the consumed choice");
	}
	for (unsigned change = 0; change != 7; ++change)
	{
		Fixture f; FullEngineCoopClientSurrenderInput input;
		f.status.phase = CoopCampaignPhase::Tactical; f.status.surrenderOffer = 11;
		(void)input.handle(K::Fight, true, false, &f.status, &f.groups, true, false);
		if (change == 0) ++f.status.timeControlRevision;
		if (change == 1) ++f.groups.revision;
		if (change == 2) ++f.status.surrenderOffer;
		if (change == 3) ++f.status.sessionEpoch;
		if (change == 4) f.status.surrenderOffer = 0;
		if (change == 5) f.enabled = false;
		if (change == 6) f.pending = true;
		CHECK(!input.handle(K::Confirm, true, false, &f.status, &f.groups, f.enabled, f.pending) && !input.armed(),
			"changed authority, offer, readiness or outstanding request cancels armed answers");
	}
}

}

int main() { BattleNoticeConfirmation(); SurrenderConfirmation(); Travel(); StaleAndPending(); Arrival(); return failures ? 1 : 0; }
