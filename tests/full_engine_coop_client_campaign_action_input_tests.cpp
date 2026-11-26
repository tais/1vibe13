#include <Ja2/FullEngineCoopClientCampaignActionInput.h>
#include <Ja2/FullEngineCoopClientSurrenderInput.h>
#include <Ja2/FullEngineCoopClientBattleNoticeInput.h>
#include <Ja2/FullEngineCoopClientReturnToMapInput.h>
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

void AssignmentMode()
{
	Fixture f;
	f.groups.groups[0].memberCount = 2; f.groups.groups[1].firstMember = 2;
	f.groups.memberCount = 3; f.groups.members[1].actor = {3, 7}; f.groups.members[2].actor = {2, 1};
	CHECK(!f.press(Key::Assign) && f.input.assigning() && f.input.assignmentCode() == 1,
		"O opens assignment mode on the selected group's first merc with Squad offered first");
	const auto* member = f.input.assignedMember(f.groups);
	CHECK(member && member->actor == (TacticalEntityId{1, 1}), "the first member of the selected group is addressed");
	(void)f.press(Key::NextGroup);
	member = f.input.assignedMember(f.groups);
	CHECK(f.input.assigning() && member && member->actor == (TacticalEntityId{3, 7}) &&
		f.input.selected() == (StrategicGroupId{1, 10}), "in assignment mode the group keys pick the merc, not the group");
	(void)f.press(Key::South);
	auto doctor = f.press(Key::Confirm);
	CHECK(doctor && doctor->action == CoopCampaignAction::SetAssignment && doctor->destinationX == 2 && !doctor->destinationY &&
		doctor->decision == (3u | (std::uint64_t(7) << 16)) && !doctor->group.valid() && !f.input.assigning(),
		"Down then Enter submits Doctor for exactly the addressed merc and leaves the mode");
	(void)f.press(Key::Assign);
	for (int i = 0; i < 4; ++i) (void)f.press(Key::South);
	CHECK(f.input.assignmentCode() == 5, "the fifth choice is self-training");
	(void)f.press(Key::East); (void)f.press(Key::East);
	auto train = f.press(Key::Confirm);
	CHECK(train && train->destinationX == 5 && train->destinationY == 3, "Right picks the stat to train (1-based)");
	(void)f.press(Key::Assign); (void)f.press(Key::North);
	CHECK(f.input.assignmentCode() == 8, "Up wraps to the last choice (wake up)");
	(void)f.press(Key::North); (void)f.press(Key::North);
	CHECK(f.input.assignmentCode() == 6, "militia training sits between self-training and rest");
	(void)f.press(Key::West);
	CHECK(f.input.assignmentStat() == 9, "Left wraps the stat to the last trainable one");
	(void)f.press(Key::Cancel);
	CHECK(!f.input.assigning() && !f.press(Key::Confirm), "Esc leaves the mode without a request");
	(void)f.press(Key::Assign); f.groups.revision = 2;
	CHECK(!f.press(Key::Confirm) && !f.input.assigning(), "a new group observation cancels a pending assignment");
	f.groups.groups[0].betweenSectors = true; f.groups.groups[0].nextX = 10; f.groups.groups[0].nextY = 1;
	(void)f.press(Key::Assign);
	CHECK(!f.input.assigning(), "a travelling group cannot be reassigned");
}

void ContractMode()
{
	Fixture f;
	CHECK(!f.press(Key::Contract) && f.input.contracting() && f.input.contractChoice() == 0,
		"C opens contract mode with a one-day extension offered first");
	(void)f.press(Key::South);
	auto week = f.press(Key::Confirm);
	CHECK(week && week->action == CoopCampaignAction::ExtendContract && week->destinationX == 2 && !week->destinationY &&
		week->decision == (1u | (std::uint64_t(1) << 16)) && !f.input.contracting(),
		"Down then Enter extends the addressed merc by one week");
	(void)f.press(Key::Contract); (void)f.press(Key::North);
	CHECK(f.input.contractChoice() == 3 && !f.press(Key::Confirm) && f.input.dismissArmed() && f.input.contracting(),
		"Up wraps to dismiss, and the first Enter only arms it");
	auto fire = f.press(Key::Confirm);
	CHECK(fire && fire->action == CoopCampaignAction::DismissMerc && !fire->destinationX && !f.input.contracting(),
		"the second Enter dismisses exactly the addressed merc");
	(void)f.press(Key::Contract); (void)f.press(Key::North); (void)f.press(Key::Confirm); (void)f.press(Key::South);
	CHECK(!f.input.dismissArmed() && f.input.contractChoice() == 0, "changing the choice disarms a dismissal");
	(void)f.press(Key::Cancel);
	CHECK(!f.input.contracting() && !f.press(Key::Confirm), "Esc leaves contract mode without a request");
}

void TimeLeadConfirmation()
{
	Fixture f;
	CHECK(!f.press(Key::Confirm), "Enter alone moves no time controls");
	CHECK(!f.press(Key::TimeLead) && f.input.timeLeadArmed(), "T arms a time-lead transfer");
	auto request = f.press(Key::Confirm);
	CHECK(request && request->action == CoopCampaignAction::TransferTimeLead && !request->decision &&
		!request->group.valid() && !f.input.timeLeadArmed(), "Enter confirms the armed transfer with no fields");
	(void)f.press(Key::TimeLead); (void)f.press(Key::TimeLead);
	CHECK(!f.input.timeLeadArmed() && !f.press(Key::Confirm), "a second T disarms it");
	(void)f.press(Key::TimeLead); (void)f.press(Key::Cancel);
	CHECK(!f.input.timeLeadArmed(), "Esc disarms it");
}

void EnterSectorConfirmation()
{
	Fixture f;
	CHECK(!f.press(Key::Confirm), "Enter alone cannot load a sector");
	CHECK(!f.press(Key::EnterBattle) && f.input.enterArmed() && !f.input.destinationX(),
		"E arms entry into the selected group's own sector without choosing travel");
	const auto request = f.press(Key::Confirm);
	CHECK(request && request->action == CoopCampaignAction::EnterSector && request->group == (StrategicGroupId{1, 10}) &&
		request->destinationX == 9 && request->destinationY == 1 && !request->decision && !f.input.enterArmed(),
		"Enter submits the exact selected group and its displayed sector");
	(void)f.press(Key::EnterBattle); (void)f.press(Key::Cancel);
	CHECK(!f.press(Key::Confirm), "Esc cancels the armed entry");
	(void)f.press(Key::EnterBattle); (void)f.press(Key::East);
	CHECK(!f.input.enterArmed() && f.input.destinationX() == 10, "choosing travel replaces an armed entry");
	f.input.reset(); f.groups.groups[0].z = 1;
	(void)f.press(Key::EnterBattle);
	const auto underground = f.press(Key::Confirm);
	CHECK(underground && underground->action == CoopCampaignAction::EnterSector,
		"an underground group may request peaceful entry of its level");
	f.input.reset(); f.groups.groups[0].z = 0; f.groups.groups[0].vehicle = true;
	CHECK(!f.press(Key::EnterBattle) && !f.input.enterArmed(), "vehicle groups cannot arm peaceful entry");
	f.input.reset(); f.groups.groups[0].vehicle = false; f.battle();
	(void)f.press(Key::EnterBattle);
	CHECK(!f.input.enterArmed(), "with a pending battle, E answers the arrival decision instead");
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
	Fixture asking; asking.battle();
	asking.status.arrival.stage = CoopCampaignArrivalStage::ReinforcementsRequired;
	asking.status.arrival.involvedMercs = 0; asking.status.arrival.nativeEnterSector = asking.status.arrival.nativeRetreat = false;
	CHECK(!asking.press(Key::Confirm) && !asking.press(Key::EnterBattle), "nothing is answered without Y or N");
	CHECK(!asking.press(Key::Yes) && asking.input.reinforcementArmed() == 1, "Y arms calling the militia in");
	request = asking.press(Key::Confirm);
	CHECK(request && request->action == CoopCampaignAction::AcceptReinforcements && request->decision == 3 &&
		!asking.input.reinforcementArmed(), "Enter confirms the call");
	(void)asking.press(Key::No);
	CHECK(asking.input.reinforcementArmed() == 2, "N arms fighting without them");
	(void)asking.press(Key::Cancel);
	CHECK(!asking.input.reinforcementArmed() && !asking.press(Key::Confirm), "Esc disarms the answer");
	(void)asking.press(Key::No);
	request = asking.press(Key::Confirm);
	CHECK(request && request->action == CoopCampaignAction::DeclineReinforcements, "Enter confirms the refusal");
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

void ReturnToMapConfirmation()
{
	using K = FullEngineCoopClientReturnToMapInput::Key;
	Fixture f; FullEngineCoopClientReturnToMapInput input;
	f.status.phase = CoopCampaignPhase::Tactical;
	f.status.securedSector = 4;
	auto key = [&](K k, bool down = true, bool up = false) { return input.handle(k, down, up, &f.status, &f.groups, f.enabled, f.pending); };
	CHECK(!key(K::Confirm), "Enter alone cannot leave a secured sector");
	CHECK(!key(K::Arm) && input.armed(), "W arms the return but does not submit it");
	CHECK(!key(K::Arm) && input.armed(), "held W cannot submit or toggle the choice");
	(void)key(K::Confirm, false, true);
	const auto answer = key(K::Confirm);
	CHECK(answer && answer->action == CoopCampaignAction::ReturnToMap && answer->decision == 4 &&
		!answer->requestId && !answer->sessionEpoch && !answer->controlRevision && !input.armed(),
		"separate Enter submits only the displayed secured sector; core supplies request lineage");
	CHECK(!key(K::Confirm), "held Enter cannot replay the return");
	for (unsigned change = 0; change != 11; ++change)
	{
		Fixture changed; FullEngineCoopClientReturnToMapInput confirm;
		changed.status.phase = CoopCampaignPhase::Tactical; changed.status.securedSector = 4;
		(void)confirm.handle(K::Arm, true, false, &changed.status, &changed.groups, true, false);
		if (change == 0) ++changed.status.timeControlRevision;
		if (change == 1) ++changed.groups.revision;
		if (change == 2) ++changed.status.securedSector;
		if (change == 3) ++changed.status.sessionEpoch;
		if (change == 4) changed.status.securedSector = 0;
		if (change == 5) changed.enabled = false;
		if (change == 6) changed.pending = true;
		if (change == 7) confirm.reset();
		if (change == 8) changed.status.surrenderOffer = 9;
		if (change == 9) changed.status.battleNotice = {11, CoopCampaignBattleNoticeKind::Defeated, 9, 1, 0};
		if (change == 10) { changed.status.phase = CoopCampaignPhase::Strategic; changed.status.securedSector = 0; }
		CHECK(!confirm.handle(K::Confirm, true, false, &changed.status, &changed.groups, changed.enabled, changed.pending) && !confirm.armed(),
			"stale sector, reconnect, pending request or another decision cannot confirm the retained return");
	}
	input.reset(); (void)key(K::Arm); (void)key(K::Cancel);
	CHECK(!key(K::Confirm), "Esc removes the return choice");
	f.status.securedSector = 0; input.reset();
	CHECK(!key(K::Arm) && !input.armed() && !input.enabled(), "an unsecured encounter offers no return");
}

void ExitGridConfirmation()
{
	using K = FullEngineCoopClientReturnToMapInput::Key;
	Fixture f; FullEngineCoopClientReturnToMapInput input;
	f.status.phase = CoopCampaignPhase::Tactical; f.status.securedSector = 4;
	const StrategicGroupId squad{1, 10};
	auto key = [&](K k, StrategicGroupId group = {}, bool down = true, bool up = false) {
		return input.handle(k, down, up, &f.status, &f.groups, f.enabled, f.pending, group); };
	CHECK(!key(K::ArmExit, StrategicGroupId{7, 7}) && !input.exitArmed(), "an unknown group cannot arm the exit");
	(void)key(K::ArmExit, {}, false, true);
	CHECK(!key(K::ArmExit, squad) && input.exitArmed() && !input.armed() && input.exitGroup() == squad,
		"U arms the exit for the selected merc's group without returning to the map");
	const auto answer = key(K::Confirm);
	CHECK(answer && answer->action == CoopCampaignAction::TakeExitGrid && answer->decision == 4 &&
		answer->group == squad && !input.exitArmed(), "Enter takes the exit for exactly that group and sector");
	(void)key(K::Confirm, {}, false, true);
	(void)key(K::ArmExit, {}, false, true); (void)key(K::ArmExit, squad); (void)key(K::Arm);
	CHECK(input.armed() && !input.exitArmed(), "W replaces an armed exit with the map return");
	const auto returned = key(K::Confirm);
	CHECK(returned && returned->action == CoopCampaignAction::ReturnToMap, "the later choice is the one confirmed");
	(void)key(K::Confirm, {}, false, true); (void)key(K::ArmExit, {}, false, true);
	(void)key(K::ArmExit, squad); f.groups.groups[0].betweenSectors = true;
	CHECK(!key(K::Confirm) && !input.exitArmed(), "a group that starts travelling cannot take the retained exit");
	f.groups.groups[0].betweenSectors = false; ++f.groups.revision;
	(void)key(K::Confirm, {}, false, true);
	CHECK(!key(K::Confirm) && !input.exitArmed(), "a new group observation disarms the exit");
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

int main() { BattleNoticeConfirmation(); ReturnToMapConfirmation(); ExitGridConfirmation(); SurrenderConfirmation(); Travel(); AssignmentMode(); ContractMode(); TimeLeadConfirmation(); EnterSectorConfirmation(); StaleAndPending(); Arrival(); return failures ? 1 : 0; }
