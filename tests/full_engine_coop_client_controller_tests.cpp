#include <Ja2/FullEngineCoopClientController.h>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace
{
int Failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++Failures; \
	} \
} while (false)

TacticalActorSnapshot Actor(std::uint16_t slot, std::int32_t grid,
	TacticalStance stance = TacticalStance::Standing)
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{slot, 1};
	actor.team = slot >= 3 ? 1 : 0;
	actor.profile = slot;
	actor.grid = grid;
	actor.direction = 2;
	actor.stance = stance;
	actor.actionPoints = 20;
	actor.life = 80;
	actor.maximumLife = 90;
	actor.breath = 70;
	actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	actor.hostileToPlayerTeam = slot >= 3;
	return actor;
}

TacticalTurnSnapshot Turn(std::uint8_t activeTeam,
	bool commandsBlocked = false,
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None,
	std::uint64_t interruptSerial = 0)
{
	TacticalTurnSnapshot turn;
	turn.turnBased = true;
	turn.inCombat = true;
	turn.activeTeam = activeTeam;
	turn.serial = 7;
	turn.interruptPhase = interruptPhase;
	turn.interruptSerial = interruptSerial;
	turn.commandsBlocked = commandsBlocked;
	return turn;
}

TacticalWorldSnapshot Snapshot(bool loaded = true,
	std::uint8_t activeTeam = 0,
	std::vector<TacticalDoorSnapshot> doors = {},
	std::uint64_t epoch = 11,
	bool commandsBlocked = false,
	TacticalInterruptPhase interruptPhase = TacticalInterruptPhase::None,
	std::uint64_t interruptSerial = 0,
	bool firstInterruptEligible = false,
	bool secondInterruptEligible = false)
{
	std::vector<TacticalActorSnapshot> actors{
		Actor(1, 1001), Actor(2, 1002, TacticalStance::Crouched),
		Actor(3, 1200), Actor(4, 1201)};
	actors[0].interruptActionEligible = firstInterruptEligible;
	actors[1].interruptActionEligible = secondInterruptEligible;
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(epoch,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, loaded},
		Turn(activeTeam, commandsBlocked, interruptPhase, interruptSerial),
		std::move(actors), std::move(doors), snapshot) ==
			TacticalSnapshotCreateError::None,
		"controller fixture snapshot is valid");
	return snapshot;
}

TacticalWorldSnapshot RelativeMoveSnapshot(TacticalActorSnapshot actor,
	TacticalWorldDimensions dimensions = TacticalWorldDimensions{5, 5},
	std::uint8_t activeTeam = 0, bool loaded = true)
{
	std::vector<TacticalActorSnapshot> actors{actor};
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(11, dimensions,
		TacticalSectorSnapshot{9, 2, 0, loaded},
		Turn(activeTeam),
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"relative-move fixture snapshot is valid");
	return snapshot;
}

FullEngineCoopClientControllerView View(
	const TacticalWorldSnapshot& snapshot,
	const std::array<TacticalEntityId, 3>& assigned,
	std::uint64_t outstanding = 0, bool resynchronizing = false)
{
	return FullEngineCoopClientControllerView{
		&snapshot, assigned.data(), assigned.size(), outstanding,
		resynchronizing};
}

void TestSelectionUsesOnlyAssignedPresentActors()
{
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{5, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.ready(view) &&
		controller.selectedActor() == (TacticalEntityId{1, 1}),
		"first assigned present actor is selected");
	CHECK(controller.selectNext(view) &&
		controller.selectedActor() == (TacticalEntityId{2, 1}),
		"next selection advances across assigned present actors");
	CHECK(controller.selectNext(view) &&
		controller.selectedActor() == (TacticalEntityId{1, 1}),
		"selection skips an assigned actor absent from the replica");
	CHECK(controller.selectPrevious(view) &&
		controller.selectedActor() == (TacticalEntityId{2, 1}),
		"previous selection wraps over absent assignments");
}

void TestNumericMoveEntryProducesOneTypedRequest()
{
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.beginDestinationEntry(view),
		"ready controller begins explicit destination entry");
	CHECK(controller.appendDestinationDigit(1) &&
		controller.appendDestinationDigit(2) &&
		controller.appendDestinationDigit(3) &&
		controller.appendDestinationDigit(4),
		"destination accepts bounded decimal digits");
	controller.toggleReverse();
	const FullEngineCoopClientIntentRequest request =
		controller.submitMove(view, 5);
	const auto* move = std::get_if<CoopSession::MoveTacticalIntent>(
		&request.payload);
	CHECK(request && request.actor == (TacticalEntityId{1, 1}) &&
		move != nullptr && move->destinationGrid == 1234 &&
		move->movementMode == 5 && move->reverse,
		"move entry emits actor-owned typed payload without local mutation");
	CHECK(!controller.enteringDestination(),
		"successful submission closes destination entry");
	CHECK(snapshot.find(TacticalEntityId{1, 1})->grid == 1001,
		"intent construction never speculates into the replica");
}

void TestRelativeMoveProducesEveryAdjacentPayload()
{
	const TacticalWorldSnapshot snapshot = RelativeMoveSnapshot(
		Actor(1, 12, TacticalStance::Crouched));
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1},
		TacticalEntityId{9, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	struct AdjacentCase
	{
		int deltaRow;
		int deltaColumn;
		std::int32_t destination;
		const char* message;
	};
	const std::array<AdjacentCase, 8> cases{{
		{-1, -1, 6, "up-arrow delta maps to the isometric upper tile"},
		{1, 1, 18, "down-arrow delta maps to the isometric lower tile"},
		{1, -1, 16, "left-arrow delta maps to the isometric left tile"},
		{-1, 1, 8, "right-arrow delta maps to the isometric right tile"},
		{-1, 0, 7, "north cardinal delta remains adjacent"},
		{1, 0, 17, "south cardinal delta remains adjacent"},
		{0, -1, 11, "west cardinal delta remains adjacent"},
		{0, 1, 13, "east cardinal delta remains adjacent"}}};
	for (const AdjacentCase& adjacent : cases)
	{
		const FullEngineCoopClientIntentRequest request =
			controller.submitRelativeMove(view, adjacent.deltaRow,
				adjacent.deltaColumn, 5);
		const auto* const move =
			std::get_if<CoopSession::MoveTacticalIntent>(&request.payload);
		CHECK(request && request.actor == (TacticalEntityId{1, 1}) &&
			move != nullptr && move->destinationGrid == adjacent.destination &&
			move->movementMode == 5 && !move->reverse, adjacent.message);
	}
	CHECK(snapshot.find(TacticalEntityId{1, 1})->grid == 12,
		"relative move requests never predict movement into the replica");
}

void TestRelativeMoveRejectsEdgesAndRowWrapping()
{
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1},
		TacticalEntityId{9, 1}};
	struct EdgeCase
	{
		std::int32_t grid;
		int deltaRow;
		int deltaColumn;
		const char* message;
	};
	const std::array<EdgeCase, 10> cases{{
		{2, -1, 0, "top edge rejects an upward cardinal step"},
		{22, 1, 0, "bottom edge rejects a downward cardinal step"},
		{10, 0, -1, "left edge rejects a horizontal row wrap"},
		{14, 0, 1, "right edge rejects a horizontal row wrap"},
		{0, -1, -1, "top-left corner rejects its outward diagonal"},
		{4, -1, 1, "top-right corner rejects its outward diagonal"},
		{20, 1, -1, "bottom-left corner rejects its outward diagonal"},
		{24, 1, 1, "bottom-right corner rejects its outward diagonal"},
		{5, 0, -1, "row-start movement cannot wrap to the previous row"},
		{9, 0, 1, "row-end movement cannot wrap to the next row"}}};
	for (const EdgeCase& edge : cases)
	{
		const TacticalWorldSnapshot snapshot =
			RelativeMoveSnapshot(Actor(1, edge.grid));
		const FullEngineCoopClientControllerView view = View(snapshot, assigned);
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		CHECK(!controller.submitRelativeMove(
			view, edge.deltaRow, edge.deltaColumn, 0), edge.message);
	}
}

void TestRelativeMoveFailsClosedForInvalidState()
{
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1},
		TacticalEntityId{9, 1}};
	const TacticalWorldSnapshot snapshot =
		RelativeMoveSnapshot(Actor(1, 12));
	const FullEngineCoopClientControllerView ready = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(ready);
	CHECK(!controller.submitRelativeMove(ready, 0, 0, 0) &&
		!controller.submitRelativeMove(ready, -2, 0, 0) &&
		!controller.submitRelativeMove(ready, 2, 0, 0) &&
		!controller.submitRelativeMove(ready, 0, -2, 0) &&
		!controller.submitRelativeMove(ready, 0, 2, 0),
		"zero and out-of-range relative deltas are rejected");

	const FullEngineCoopClientControllerView outstanding =
		View(snapshot, assigned, 44);
	CHECK(!controller.submitRelativeMove(outstanding, -1, -1, 0),
		"outstanding command lock rejects relative movement");

	const TacticalWorldSnapshot enemyTurn =
		RelativeMoveSnapshot(Actor(1, 12), {5, 5}, 1);
	const FullEngineCoopClientControllerView observing =
		View(enemyTurn, assigned);
	controller.synchronize(observing);
	CHECK(!controller.submitRelativeMove(observing, -1, -1, 0),
		"another team's active turn rejects relative movement");

	TacticalWorldSnapshot invalidDimensions;
	const FullEngineCoopClientControllerView invalidView =
		View(invalidDimensions, assigned);
	controller.synchronize(invalidView);
	CHECK(!controller.submitRelativeMove(invalidView, -1, -1, 0),
		"a default snapshot with zero dimensions rejects relative movement");

	struct ActorStateCase
	{
		TacticalActorSnapshot actor;
		bool unavailable;
		const char* message;
	};
	TacticalActorSnapshot dead = Actor(1, 12);
	dead.life = 0;
	TacticalActorSnapshot inactive = Actor(1, 12);
	inactive.active = false;
	TacticalActorSnapshot absent = Actor(1, 12);
	absent.inSector = false;
	const std::array<ActorStateCase, 5> actorStates{{
		{Actor(1, -1), false,
			"negative current grid rejects relative movement"},
		{Actor(1, 25), false,
			"current grid outside dimensions rejects relative movement"},
		{dead, true, "dead selected actor rejects every action"},
		{inactive, true, "inactive selected actor rejects every action"},
		{absent, true, "out-of-sector selected actor rejects every action"}}};
	for (const ActorStateCase& actorState : actorStates)
	{
		const TacticalWorldSnapshot stateSnapshot =
			RelativeMoveSnapshot(actorState.actor);
		const FullEngineCoopClientControllerView stateView =
			View(stateSnapshot, assigned);
		controller.synchronize(stateView);
		CHECK((!actorState.unavailable ||
			(!controller.actionsEnabled(stateView) &&
			 !controller.stop(stateView))) &&
			!controller.submitRelativeMove(stateView, -1, -1, 0),
			actorState.message);
	}
}

void TestInvalidAndOutstandingInputsFailClosed()
{
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	FullEngineCoopClientController controller;
	const FullEngineCoopClientControllerView ready = View(snapshot, assigned);
	controller.synchronize(ready);
	CHECK(controller.beginDestinationEntry(ready),
		"overflow fixture enters move mode");
	for (char digit : std::string("2147483648"))
		CHECK(controller.appendDestinationDigit(
			static_cast<unsigned>(digit - '0')),
			"ten digit destination remains bounded");
	CHECK(!controller.submitMove(ready, 0),
		"destination above signed grid range is rejected");

	const FullEngineCoopClientControllerView outstanding =
		View(snapshot, assigned, 44);
	controller.synchronize(outstanding);
	CHECK(!controller.actionsEnabled(outstanding) &&
		!controller.stop(outstanding) &&
		!controller.reload(outstanding) &&
		!controller.beginDestinationEntry(outstanding),
		"one outstanding command disables every action");

	const TacticalWorldSnapshot unloaded = Snapshot(false);
	const FullEngineCoopClientControllerView waiting = View(unloaded, assigned);
	controller.synchronize(waiting);
	CHECK(!controller.ready(waiting) &&
		!controller.selectedActor().valid() && !controller.endTurn(waiting),
		"unloaded strategic snapshot remains a non-actionable waiting view");

	const TacticalWorldSnapshot enemyTurn = Snapshot(true, 1);
	const FullEngineCoopClientControllerView observing =
		View(enemyTurn, assigned);
	controller.synchronize(observing);
	CHECK(controller.ready(observing) &&
		!controller.actionsEnabled(observing) && !controller.stop(observing),
		"turn-based controls remain passive during another team's turn");
}

void TestResynchronizationRetainsSelectionAndFreezesActions()
{
	const TacticalWorldSnapshot snapshot = Snapshot(true, 0,
		{TacticalDoorSnapshot{1000, 41, false}});
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView ready = View(snapshot, assigned);
	const FullEngineCoopClientControllerView resynchronizing =
		View(snapshot, assigned, 0, true);
	FullEngineCoopClientController controller;
	controller.synchronize(ready);
	CHECK(controller.beginDestinationEntry(ready),
		"resynchronization fixture enters a local modal");
	controller.synchronize(resynchronizing);
	CHECK(controller.ready(resynchronizing) &&
		controller.selectedActor() == (TacticalEntityId{1, 1}),
		"resynchronization keeps the last committed replica selection visible");
	CHECK(!controller.actionsEnabled(resynchronizing) &&
		!controller.enteringDestination() &&
		!controller.stop(resynchronizing) &&
		!controller.endTurn(resynchronizing),
		"resynchronization cancels local modals and freezes every action");

	controller.synchronize(ready);
	CHECK(controller.actionsEnabled(ready),
		"a replacement committed baseline restores tactical controls");
}

void TestReplicatedCommandGateLocksAndRecovers()
{
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const std::vector<TacticalDoorSnapshot> doors{
		TacticalDoorSnapshot{1000, 41, false}};
	const TacticalWorldSnapshot readySnapshot =
		Snapshot(true, 0, doors);
	const TacticalWorldSnapshot blockedSnapshot =
		Snapshot(true, 0, doors, 11, true);
	const FullEngineCoopClientControllerView ready =
		View(readySnapshot, assigned);
	const FullEngineCoopClientControllerView blocked =
		View(blockedSnapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(ready);

	CHECK(controller.beginDestinationEntry(ready),
		"busy-gate fixture enters numeric movement mode");
	controller.synchronize(blocked);
	CHECK(!controller.actionsEnabled(blocked) &&
		!controller.enteringDestination() && !controller.stop(blocked),
		"replicated command-busy gate closes movement mode and blocks actions");

	controller.synchronize(ready);
	CHECK(controller.actionsEnabled(ready) &&
		controller.beginAttackTargeting(ready),
		"cleared command-busy gate restores actions and attack targeting");
	controller.synchronize(blocked);
	CHECK(!controller.targetingAttack(),
		"replicated command-busy gate closes attack targeting");

	controller.synchronize(ready);
	CHECK(controller.beginDoorSelection(ready),
		"cleared command-busy gate restores door selection");
	controller.synchronize(blocked);
	CHECK(!controller.selectingDoor(),
		"replicated command-busy gate closes door selection");

	controller.synchronize(ready);
	CHECK(controller.beginDestinationEntry(ready),
		"turn-lock fixture enters a modal on the actor's own turn");
	const TacticalWorldSnapshot enemyTurn = Snapshot(true, 1, doors);
	const FullEngineCoopClientControllerView observing =
		View(enemyTurn, assigned);
	controller.synchronize(observing);
	CHECK(!controller.actionsEnabled(observing) &&
		!controller.enteringDestination(),
		"own-turn to other-team transition closes stale modal state");
}

void TestAuthoritativeInterruptGatesActionsAndPassesSelectedMerc()
{
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const TacticalWorldSnapshot activeSnapshot = Snapshot(
		true, 0, {}, 11, false, TacticalInterruptPhase::Active, 44,
		false, true);
	const FullEngineCoopClientControllerView active =
		View(activeSnapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(active);
	CHECK(controller.ready(active) &&
		controller.selectedActor() == (TacticalEntityId{1, 1}) &&
		!controller.actionsEnabled(active) && !controller.stop(active) &&
		!controller.endTurn(active),
		"an ineligible selected merc cannot act or pass an active interrupt");

	CHECK(controller.selectNext(active) &&
		controller.selectedActor() == (TacticalEntityId{2, 1}) &&
		controller.actionsEnabled(active) && controller.stop(active),
		"an eligible selected merc may act while an ineligible teammate may not");
	const FullEngineCoopClientIntentRequest pass = controller.endTurn(active);
	const auto* const passPayload =
		std::get_if<CoopSession::PassInterruptTacticalIntent>(&pass.payload);
	CHECK(pass && pass.actor == (TacticalEntityId{2, 1}) &&
		passPayload != nullptr && passPayload->interruptSerial == 44,
		"T emits an exact interrupt pass for the selected eligible merc");

	CHECK(controller.beginDestinationEntry(active),
		"eligible interrupt action can enter a local modal");
	const TacticalWorldSnapshot resolvingSnapshot = Snapshot(
		true, 0, {}, 11, false, TacticalInterruptPhase::Resolving, 45);
	const FullEngineCoopClientControllerView resolving =
		View(resolvingSnapshot, assigned);
	controller.synchronize(resolving);
	CHECK(!controller.actionsEnabled(resolving) &&
		!controller.enteringDestination() && !controller.stop(resolving) &&
		!controller.endTurn(resolving),
		"authoritative interrupt resolution cancels modals and disables every action");

	const TacticalWorldSnapshot blockedSnapshot = Snapshot(
		true, 0, {}, 11, true, TacticalInterruptPhase::Active, 46,
		false, true);
	const FullEngineCoopClientControllerView blocked =
		View(blockedSnapshot, assigned);
	controller.synchronize(blocked);
	CHECK(!controller.actionsEnabled(blocked) && !controller.stop(blocked) &&
		!controller.endTurn(blocked),
		"the replicated command gate overrides active interrupt eligibility");

	const TacticalWorldSnapshot normalSnapshot = Snapshot();
	const FullEngineCoopClientControllerView normal =
		View(normalSnapshot, assigned);
	controller.synchronize(normal);
	const FullEngineCoopClientIntentRequest end = controller.endTurn(normal);
	CHECK(end && end.actor == (TacticalEntityId{2, 1}) &&
		std::holds_alternative<CoopSession::EndTurnTacticalIntent>(end.payload),
		"T remains the ordinary end-turn intent outside an active interrupt");
}

void TestRemainingTypedActions()
{
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	const FullEngineCoopClientIntentRequest face = controller.face(view, 7);
	const auto* facing = std::get_if<CoopSession::FaceTacticalIntent>(
		&face.payload);
	CHECK(face && facing != nullptr && facing->direction == 7,
		"face action carries an absolute bounded direction");

	const FullEngineCoopClientIntentRequest stance = controller.stance(
		view, CoopSession::TacticalIntentStance::Prone);
	const auto* changed = std::get_if<CoopSession::StanceTacticalIntent>(
		&stance.payload);
	CHECK(stance && changed != nullptr &&
		changed->stance == CoopSession::TacticalIntentStance::Prone,
		"stance action carries only the closed protocol enum");
	CHECK(controller.stop(view) &&
		std::holds_alternative<CoopSession::StopTacticalIntent>(
			controller.stop(view).payload),
		"stop action is typed and actor-owned");
	CHECK(controller.endTurn(view) &&
		std::holds_alternative<CoopSession::EndTurnTacticalIntent>(
			controller.endTurn(view).payload),
		"combat end-turn action is typed and actor-owned");
	const FullEngineCoopClientIntentRequest reload = controller.reload(view);
	CHECK(reload && reload.actor == (TacticalEntityId{1, 1}) &&
		std::holds_alternative<CoopSession::ReloadTacticalIntent>(
			reload.payload),
		"reload is a zero-payload selected-actor request without local mutation");
	CHECK(!controller.face(view, 8),
		"out-of-range face direction is rejected locally");

	CHECK(controller.beginAttackTargeting(view) &&
		controller.targetingAttack() &&
		controller.attackTarget() == (TacticalEntityId{3, 1}) &&
		controller.attackAimTime() == 1,
		"aimed fire begins on the first live different-team replica actor");
	CHECK(controller.selectNextTarget(view) &&
		controller.attackTarget() == (TacticalEntityId{4, 1}) &&
		controller.selectPreviousTarget(view) &&
		controller.attackTarget() == (TacticalEntityId{3, 1}),
		"target selection cycles only replicated opposing actors");
	CHECK(controller.adjustAttackAim(1) &&
		controller.attackAimTime() == 2,
		"targeting adjusts the bounded authoritative aim request");
	const FullEngineCoopClientIntentRequest attack =
		controller.submitAimedFirearmAttack(view);
	const auto* aimed =
		std::get_if<CoopSession::AimedFirearmAttackTacticalIntent>(
			&attack.payload);
	CHECK(attack && attack.actor == (TacticalEntityId{1, 1}) &&
		aimed != nullptr && aimed->target == (TacticalEntityId{3, 1}) &&
		aimed->aimTime == 2 && !controller.targetingAttack(),
		"aimed fire emits exact actor/target identities without local mutation");
	CHECK(snapshot.find(TacticalEntityId{3, 1})->life == 80,
		"targeting never speculates damage into the passive replica");
}

void TestAttackTargetingUsesCanonicalHostility()
{
	std::vector<TacticalActorSnapshot> actors{
		Actor(1, 1001), Actor(3, 1200), Actor(4, 1201), Actor(5, 1202)};
	actors[2].hostileToPlayerTeam = false;
	actors[3].team = actors[0].team;
	actors[3].hostileToPlayerTeam = true;
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(11,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true},
		Turn(0),
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"hostility targeting fixture is valid");
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1},
		TacticalEntityId{9, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.beginAttackTargeting(view) &&
		controller.attackTarget() == (TacticalEntityId{3, 1}),
		"targeting offers a canonically hostile actor");
	CHECK(controller.selectNextTarget(view) &&
		controller.attackTarget() == (TacticalEntityId{5, 1}),
		"targeting skips neutral militia and trusts hostility over team number");
}

void TestDoorSelectionProducesExactInverseIntent()
{
	const TacticalWorldSnapshot snapshot = Snapshot(true, 0, {
		TacticalDoorSnapshot{1000, 41, false},
		TacticalDoorSnapshot{1001, 42, true},
		TacticalDoorSnapshot{1003, 43, false}});
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.beginDoorSelection(view) && controller.selectingDoor() &&
		controller.selectedDoorBaseGrid() == 1000 &&
		controller.selectedDoorStructureId() == 41 &&
		!controller.selectedDoorOpen(),
		"door mode selects the first visible same-or-cardinal adjacent door");
	CHECK(controller.selectNextDoor(view) &&
		controller.selectedDoorBaseGrid() == 1001 &&
		controller.selectedDoorOpen() &&
		controller.selectPreviousDoor(view) &&
		controller.selectedDoorBaseGrid() == 1000,
		"door mode cycles only coarse adjacent projected doors");
	const FullEngineCoopClientIntentRequest request =
		controller.submitDoorOpenClose(view);
	const auto* door = std::get_if<CoopSession::DoorOpenCloseTacticalIntent>(
		&request.payload);
	CHECK(request && request.actor == (TacticalEntityId{1, 1}) &&
		door != nullptr && door->baseGrid == 1000 &&
		door->structureId == 41 && door->desiredOpen &&
		!controller.selectingDoor(),
		"door submit carries exact public identity and requests inverse state");
	CHECK(!snapshot.findDoor(1000)->open,
		"door selection never speculates into the passive replica");
}

void TestDoorSelectionFailsClosedAcrossReplicaChanges()
{
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const TacticalWorldSnapshot closed = Snapshot(true, 0,
		{TacticalDoorSnapshot{1000, 41, false}});
	FullEngineCoopClientController controller;
	auto view = View(closed, assigned);
	controller.synchronize(view);
	CHECK(controller.beginDoorSelection(view),
		"door invalidation fixture enters door mode");
	const TacticalWorldSnapshot swapped = Snapshot(true, 0,
		{TacticalDoorSnapshot{1000, 42, true}});
	auto swappedView = View(swapped, assigned);
	controller.synchronize(swappedView);
	CHECK(!controller.selectingDoor() &&
		!controller.submitDoorOpenClose(swappedView),
		"partner ID and open-state change cancels stale door selection");

	view = View(closed, assigned);
	controller.synchronize(view);
	CHECK(controller.beginDoorSelection(view),
		"door mode can restart after a committed change");
	const TacticalWorldSnapshot nextWorld = Snapshot(true, 0,
		{TacticalDoorSnapshot{1000, 41, false}}, 12);
	auto nextWorldView = View(nextWorld, assigned);
	controller.synchronize(nextWorldView);
	CHECK(!controller.selectingDoor(),
		"world-generation epoch change cancels door selection");

	controller.synchronize(view);
	CHECK(controller.beginDoorSelection(view),
		"outstanding-command fixture enters door mode");
	auto outstanding = View(closed, assigned, 9);
	controller.synchronize(outstanding);
	CHECK(!controller.selectingDoor(),
		"an outstanding command cancels door selection");

	controller.synchronize(view);
	CHECK(controller.beginDoorSelection(view) &&
		controller.selectNext(view) && !controller.selectingDoor(),
		"changing selected actor cancels door selection");
}
}

int main()
{
	TestSelectionUsesOnlyAssignedPresentActors();
	TestNumericMoveEntryProducesOneTypedRequest();
	TestRelativeMoveProducesEveryAdjacentPayload();
	TestRelativeMoveRejectsEdgesAndRowWrapping();
	TestRelativeMoveFailsClosedForInvalidState();
	TestInvalidAndOutstandingInputsFailClosed();
	TestResynchronizationRetainsSelectionAndFreezesActions();
	TestReplicatedCommandGateLocksAndRecovers();
	TestAuthoritativeInterruptGatesActionsAndPassesSelectedMerc();
	TestRemainingTypedActions();
	TestAttackTargetingUsesCanonicalHostility();
	TestDoorSelectionProducesExactInverseIntent();
	TestDoorSelectionFailsClosedAcrossReplicaChanges();
	if (Failures == 0)
		std::printf("full-engine co-op client controller tests passed\n");
	return Failures == 0 ? 0 : 1;
}
