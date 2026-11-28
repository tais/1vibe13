#include <Ja2/FullEngineCoopClientController.h>
#include <Ja2/FullEngineCoopClientCommandTranslator.h>

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
		TacticalSectorSnapshot{9, 2, 0, loaded,
			loaded ? TacticalMapAssetKey{{"A9.dat"}} : TacticalMapAssetKey{}},
		Turn(activeTeam, commandsBlocked, interruptPhase, interruptSerial),
		std::move(actors), std::move(doors), snapshot) ==
			TacticalSnapshotCreateError::None,
		"controller fixture snapshot is valid");
	return snapshot;
}

TacticalWorldSnapshot RelativeMoveSnapshot(TacticalActorSnapshot actor,
	TacticalWorldDimensions dimensions = TacticalWorldDimensions{5, 5},
	std::uint8_t activeTeam = 0, bool loaded = true,
	bool turnBased = true, bool inCombat = true)
{
	std::vector<TacticalActorSnapshot> actors{actor};
	TacticalTurnSnapshot turn = Turn(activeTeam);
	turn.turnBased = turnBased;
	turn.inCombat = inCombat;
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(11, dimensions,
		TacticalSectorSnapshot{9, 2, 0, loaded},
		turn,
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

void TestExactActorSelectionValidatesIdentityAndModalState()
{
	const TacticalWorldSnapshot snapshot = Snapshot(true, 0,
		{TacticalDoorSnapshot{1000, 41, false}});
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	CHECK(controller.beginDestinationEntry(view) &&
		controller.appendDestinationDigit(4),
		"exact-selection fixture enters destination mode");
	controller.toggleReverse();
	CHECK(controller.selectActor(view, TacticalEntityId{1, 1}) &&
		controller.enteringDestination() && controller.reverse() &&
		std::string(controller.destinationText()) == "4",
		"reselecting the exact actor retains its local modal state");
	CHECK(controller.selectActor(view, TacticalEntityId{2, 1}) &&
		controller.selectedActor() == (TacticalEntityId{2, 1}) &&
		!controller.enteringDestination(),
		"changing to an exact assigned actor cancels destination state");

	CHECK(controller.beginDestinationEntry(view) &&
		controller.appendDestinationDigit(7),
		"invalid exact-selection fixture re-enters destination mode");
	CHECK(!controller.selectActor(view, TacticalEntityId{4, 1}) &&
		!controller.selectActor(view, TacticalEntityId{1, 2}) &&
		!controller.selectActor(view, TacticalEntityId{}) &&
		controller.selectedActor() == (TacticalEntityId{2, 1}) &&
		controller.enteringDestination() &&
		std::string(controller.destinationText()) == "7",
		"unassigned, stale, and invalid identities fail without changing selection or modal state");
	controller.cancelDestinationEntry();

	CHECK(controller.selectActor(view, TacticalEntityId{1, 1}) &&
		controller.beginAttackTargeting(view) &&
		controller.selectActor(view, TacticalEntityId{1, 1}) &&
		controller.targetingAttack(),
		"reselecting an actor retains attack targeting");
	CHECK(controller.selectActor(view, TacticalEntityId{2, 1}) &&
		!controller.targetingAttack(),
		"changing actor cancels attack targeting");

	CHECK(controller.selectActor(view, TacticalEntityId{1, 1}) &&
		controller.beginDoorSelection(view) &&
		controller.selectActor(view, TacticalEntityId{1, 1}) &&
		controller.selectingDoor(),
		"reselecting an actor retains exact door selection");
	CHECK(controller.selectActor(view, TacticalEntityId{2, 1}) &&
		!controller.selectingDoor(),
		"changing actor cancels exact door selection");

	const FullEngineCoopClientControllerView locked =
		View(snapshot, assigned, 44, true);
	CHECK(controller.selectActor(locked, TacticalEntityId{1, 1}) &&
		controller.selectedActor() == (TacticalEntityId{1, 1}) &&
		!controller.actionsEnabled(locked),
		"presentation selection remains available while authoritative actions are locked");
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

void TestDirectGridMoveIsBoundedPureAndAuthorityGated()
{
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	const FullEngineCoopClientController& readOnlyController = controller;

	const FullEngineCoopClientIntentRequest forward =
		readOnlyController.moveToGrid(view, 0, 65535);
	const auto* const forwardMove =
		std::get_if<CoopSession::MoveTacticalIntent>(&forward.payload);
	CHECK(forward && forward.actor == (TacticalEntityId{1, 1}) &&
		forwardMove != nullptr && forwardMove->destinationGrid == 0 &&
		forwardMove->movementMode == 65535 && !forwardMove->reverse,
		"direct movement accepts the first authority grid and preserves the full movement mode");
	const FullEngineCoopClientIntentRequest reverse =
		readOnlyController.moveToGrid(view, 25599, 5, true);
	const auto* const reverseMove =
		std::get_if<CoopSession::MoveTacticalIntent>(&reverse.payload);
	CHECK(reverse && reverse.actor == (TacticalEntityId{1, 1}) &&
		reverseMove != nullptr && reverseMove->destinationGrid == 25599 &&
		reverseMove->movementMode == 5 && reverseMove->reverse,
		"direct movement accepts the inclusive last grid and explicit reverse flag");
	CHECK(!readOnlyController.moveToGrid(view, -1, 0) &&
		!readOnlyController.moveToGrid(view, 25600, 0),
		"direct movement rejects both sides of the authority dimension bounds");
	CHECK(snapshot.find(TacticalEntityId{1, 1})->grid == 1001 &&
		snapshot.find(TacticalEntityId{1, 1})->actionPoints == 20,
		"direct movement does not mutate the controller's committed replica");

	const FullEngineCoopClientControllerView outstanding =
		View(snapshot, assigned, 44);
	const FullEngineCoopClientControllerView resynchronizing =
		View(snapshot, assigned, 0, true);
	const TacticalWorldSnapshot enemyTurn = Snapshot(true, 1);
	const FullEngineCoopClientControllerView observing =
		View(enemyTurn, assigned);
	const TacticalWorldSnapshot commandsBlocked =
		Snapshot(true, 0, {}, 11, true);
	const FullEngineCoopClientControllerView blocked =
		View(commandsBlocked, assigned);
	CHECK(!readOnlyController.moveToGrid(outstanding, 1002, 0) &&
		!readOnlyController.moveToGrid(resynchronizing, 1002, 0) &&
		!readOnlyController.moveToGrid(observing, 1002, 0) &&
		!readOnlyController.moveToGrid(blocked, 1002, 0),
		"direct movement obeys outstanding, resync, turn, and replicated command gates");

	TacticalActorSnapshot actor = Actor(1, 12);
	actor.actionPoints = 0;
	const TacticalWorldSnapshot noActionPoints =
		RelativeMoveSnapshot(actor);
	const std::array<TacticalEntityId, 3> oneAssigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1},
		TacticalEntityId{9, 1}};
	const FullEngineCoopClientControllerView noActionPointView =
		View(noActionPoints, oneAssigned);
	FullEngineCoopClientController noActionPointController;
	noActionPointController.synchronize(noActionPointView);
	CHECK(!noActionPointController.moveToGrid(
		noActionPointView, 13, 0),
		"turn-based direct movement requires positive replicated action points");
	const TacticalWorldSnapshot realTime = RelativeMoveSnapshot(
		actor, TacticalWorldDimensions{5, 5}, 0, true, false, false);
	const FullEngineCoopClientControllerView realTimeView =
		View(realTime, oneAssigned);
	noActionPointController.synchronize(realTimeView);
	CHECK(noActionPointController.moveToGrid(realTimeView, 13, 0),
		"real-time direct movement does not add a turn-based action-point gate");

	CHECK(controller.beginDestinationEntry(view) &&
		controller.appendDestinationDigit(3) &&
		controller.appendDestinationDigit(0) &&
		controller.appendDestinationDigit(0) &&
		controller.appendDestinationDigit(0) &&
		controller.appendDestinationDigit(0) &&
		!controller.submitMove(view, 0) && controller.enteringDestination(),
		"numeric submission reuses direct dimension validation and retains invalid input for correction");
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

void TestRelativeMoveUsesRectangularColumnStride()
{
	const TacticalWorldSnapshot snapshot = RelativeMoveSnapshot(
		Actor(1, 10), TacticalWorldDimensions{7, 3});
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1},
		TacticalEntityId{9, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	struct DirectionCase
	{
		int deltaRow;
		int deltaColumn;
		std::int32_t destination;
	};
	const std::array<DirectionCase, 8> cases{{
		{-1, -1, 2}, {-1, 0, 3}, {-1, 1, 4}, {0, 1, 11},
		{1, 1, 18}, {1, 0, 17}, {1, -1, 16}, {0, -1, 9}}};
	for (const DirectionCase& direction : cases)
	{
		const FullEngineCoopClientIntentRequest request =
			controller.submitRelativeMove(view, direction.deltaRow,
				direction.deltaColumn, 0);
		const auto* const move =
			std::get_if<CoopSession::MoveTacticalIntent>(&request.payload);
		CHECK(request && move != nullptr &&
			move->destinationGrid == direction.destination,
			"rectangular relative movement uses columns as its row-major stride");
	}

	const TacticalWorldSnapshot topEdge = RelativeMoveSnapshot(
		Actor(1, 3), TacticalWorldDimensions{7, 3});
	const FullEngineCoopClientControllerView topView = View(topEdge, assigned);
	controller.synchronize(topView);
	CHECK(!controller.submitRelativeMove(topView, -1, 0, 0),
		"rectangular relative movement validates the destination edge");
	const TacticalWorldSnapshot invalidSource = RelativeMoveSnapshot(
		Actor(1, 21), TacticalWorldDimensions{7, 3});
	const FullEngineCoopClientControllerView invalidView =
		View(invalidSource, assigned);
	controller.synchronize(invalidView);
	CHECK(!controller.submitRelativeMove(invalidView, 0, -1, 0),
		"rectangular relative movement validates the source grid");
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

void TestTurnBasedMovementRequiresPositiveActionPoints()
{
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1},
		TacticalEntityId{9, 1}};
	TacticalActorSnapshot actor = Actor(1, 12);
	actor.actionPoints = 0;
	const TacticalWorldSnapshot noActionPoints =
		RelativeMoveSnapshot(actor);
	const FullEngineCoopClientControllerView turnBased =
		View(noActionPoints, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(turnBased);

	CHECK(controller.actionsEnabled(turnBased) &&
		controller.stop(turnBased) && controller.face(turnBased, 3) &&
		controller.stance(turnBased,
			CoopSession::TacticalIntentStance::Crouched) &&
		controller.reload(turnBased) && controller.endTurn(turnBased),
		"zero AP leaves non-movement turn controls available");
	CHECK(!controller.submitRelativeMove(turnBased, -1, -1, 0),
		"zero AP rejects an arrow movement intent in turn-based combat");
	CHECK(controller.beginDestinationEntry(turnBased) &&
		controller.appendDestinationDigit(1) &&
		controller.appendDestinationDigit(3) &&
		!controller.submitMove(turnBased, 0),
		"zero AP rejects an explicit-grid movement intent in turn-based combat");
	controller.cancelDestinationEntry();

	const TacticalWorldSnapshot realTime = RelativeMoveSnapshot(
		actor, TacticalWorldDimensions{5, 5}, 0, true, false, false);
	const FullEngineCoopClientControllerView realTimeView =
		View(realTime, assigned);
	controller.synchronize(realTimeView);
	CHECK(controller.submitRelativeMove(realTimeView, -1, -1, 0),
		"real-time movement does not inherit the turn-based AP guard");
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

void TestDestinationAndAttackModesAreWorldEpochBound()
{
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const TacticalWorldSnapshot firstWorld = Snapshot(true, 0, {}, 11);
	const TacticalWorldSnapshot nextWorld = Snapshot(true, 0, {}, 12);
	const FullEngineCoopClientControllerView first = View(firstWorld, assigned);
	const FullEngineCoopClientControllerView next = View(nextWorld, assigned);
	FullEngineCoopClientController epochQuery;
	epochQuery.synchronize(first);
	CHECK(!epochQuery.activeModalWorldMatches(0) &&
		epochQuery.activeModalWorldMatches(11) &&
		epochQuery.activeModalWorldMatches(12),
		"the modal epoch query rejects zero and accepts any nonzero world while idle");
	CHECK(epochQuery.beginDestinationEntry(first) &&
		epochQuery.activeModalWorldMatches(11) &&
		!epochQuery.activeModalWorldMatches(12),
		"the modal epoch query exposes destination ownership without mutation");
	epochQuery.cancelDestinationEntry();
	CHECK(epochQuery.beginAttackTargeting(first) &&
		epochQuery.activeModalWorldMatches(11) &&
		!epochQuery.activeModalWorldMatches(12),
		"the modal epoch query exposes attack ownership without mutation");
	epochQuery.cancelAttackTargeting();
	const TacticalWorldSnapshot doorWorld = Snapshot(true, 0,
		{TacticalDoorSnapshot{1000, 41, false}}, 11);
	const FullEngineCoopClientControllerView doorView =
		View(doorWorld, assigned);
	epochQuery.synchronize(doorView);
	CHECK(epochQuery.beginDoorSelection(doorView) &&
		epochQuery.activeModalWorldMatches(11) &&
		!epochQuery.activeModalWorldMatches(12),
		"the modal epoch query covers the existing door modal as well");

	FullEngineCoopClientController synchronizedDestination;
	synchronizedDestination.synchronize(first);
	CHECK(synchronizedDestination.beginDestinationEntry(first) &&
		synchronizedDestination.appendDestinationDigit(4),
		"destination epoch fixture enters numeric mode in the first world");
	synchronizedDestination.synchronize(next);
	CHECK(!synchronizedDestination.enteringDestination(),
		"a synchronized world-generation change cancels destination mode");

	FullEngineCoopClientController directDestination;
	directDestination.synchronize(first);
	CHECK(directDestination.beginDestinationEntry(first) &&
		directDestination.appendDestinationDigit(4) &&
		!directDestination.submitMove(next, 0) &&
		directDestination.enteringDestination(),
		"direct destination submission against another world fails transactionally");

	FullEngineCoopClientController synchronizedAttack;
	synchronizedAttack.synchronize(first);
	CHECK(synchronizedAttack.beginAttackTargeting(first),
		"attack epoch fixture enters targeting in the first world");
	synchronizedAttack.synchronize(next);
	CHECK(!synchronizedAttack.targetingAttack(),
		"a synchronized world-generation change cancels attack targeting");

	FullEngineCoopClientController directAttack;
	directAttack.synchronize(first);
	CHECK(directAttack.beginAttackTargeting(first) &&
		directAttack.adjustAttackAim(2),
		"direct stale-world attack fixture retains meaningful target state");
	const TacticalEntityId retainedTarget = directAttack.attackTarget();
	CHECK(!directAttack.selectNextTarget(next) &&
		directAttack.attackTarget() == retainedTarget &&
		!directAttack.selectAttackTarget(next, TacticalEntityId{4, 1}) &&
		directAttack.attackTarget() == retainedTarget &&
		directAttack.attackAimTime() == 3 &&
		!directAttack.submitAimedFirearmAttack(next) &&
		directAttack.targetingAttack(),
		"direct target selection and submission against another world fail transactionally");

	FullEngineCoopClientController exactSelection;
	exactSelection.synchronize(first);
	CHECK(exactSelection.beginDestinationEntry(first) &&
		exactSelection.appendDestinationDigit(7) &&
		exactSelection.selectActor(next, TacticalEntityId{1, 1}) &&
		!exactSelection.enteringDestination() &&
		exactSelection.activeModalWorldMatches(12),
		"same-actor exact selection in a new world cancels stale destination state");
	CHECK(exactSelection.beginAttackTargeting(next),
		"same-actor attack fixture enters the current world");
	CHECK(exactSelection.selectActor(first, TacticalEntityId{1, 1}) &&
		!exactSelection.targetingAttack() &&
		exactSelection.activeModalWorldMatches(11),
		"same-actor exact selection in another world cancels stale attack state");

	FullEngineCoopClientController rejectedCycle;
	rejectedCycle.synchronize(first);
	const FullEngineCoopClientControllerView emptyNext{
		&nextWorld, nullptr, 0, 0, false};
	CHECK(rejectedCycle.beginDestinationEntry(first) &&
		rejectedCycle.appendDestinationDigit(8) &&
		!rejectedCycle.selectNext(emptyNext) &&
		rejectedCycle.enteringDestination() &&
		std::string(rejectedCycle.destinationText()) == "8",
		"a failed empty-roster cycle preserves retained modal state transactionally");

	const std::array<TacticalEntityId, 1> oneAssigned{
		TacticalEntityId{1, 1}};
	const FullEngineCoopClientControllerView oneFirst{
		&firstWorld, oneAssigned.data(), oneAssigned.size(), 0, false};
	const FullEngineCoopClientControllerView oneNext{
		&nextWorld, oneAssigned.data(), oneAssigned.size(), 0, false};
	FullEngineCoopClientController oneActorCycle;
	oneActorCycle.synchronize(oneFirst);
	CHECK(oneActorCycle.beginDestinationEntry(oneFirst) &&
		oneActorCycle.selectNext(oneNext) &&
		oneActorCycle.selectedActor() == (TacticalEntityId{1, 1}) &&
		!oneActorCycle.enteringDestination(),
		"one-actor forward cycling cannot carry destination mode across worlds");
	CHECK(oneActorCycle.beginAttackTargeting(oneNext) &&
		oneActorCycle.selectPrevious(oneFirst) &&
		oneActorCycle.selectedActor() == (TacticalEntityId{1, 1}) &&
		!oneActorCycle.targetingAttack(),
		"one-actor reverse cycling cannot carry attack mode across worlds");
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

void TestExactAttackTargetSelectionFailsClosed()
{
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	CHECK(!controller.selectAttackTarget(view, TacticalEntityId{4, 1}) &&
		!controller.targetingAttack() && !controller.attackTarget().valid(),
		"an exact attack target cannot be selected outside attack mode");
	CHECK(controller.beginAttackTargeting(view) &&
		controller.attackTarget() == (TacticalEntityId{3, 1}) &&
		controller.adjustAttackAim(2),
		"exact-target fixture begins with a valid hostile and adjusted aim");
	CHECK(controller.selectAttackTarget(view, TacticalEntityId{4, 1}) &&
		controller.attackTarget() == (TacticalEntityId{4, 1}) &&
		controller.attackAimTime() == 3 &&
		controller.selectAttackTarget(view, TacticalEntityId{4, 1}),
		"exact target selection accepts and reselects a live authority identity without resetting aim");
	CHECK(!controller.selectAttackTarget(view, TacticalEntityId{2, 1}) &&
		!controller.selectAttackTarget(view, TacticalEntityId{1, 1}) &&
		!controller.selectAttackTarget(view, TacticalEntityId{4, 2}) &&
		!controller.selectAttackTarget(view, TacticalEntityId{}) &&
		controller.targetingAttack() &&
		controller.attackTarget() == (TacticalEntityId{4, 1}) &&
		controller.attackAimTime() == 3,
		"friendly, self, stale, and invalid targets fail without changing exact target or aim");
	controller.cancelAttackTargeting();
	CHECK(!controller.selectAttackTarget(view, TacticalEntityId{3, 1}),
		"cancelled attack mode rejects an otherwise valid exact hostile");
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
		TacticalSectorSnapshot{9, 2, 0, true, TacticalMapAssetKey{{"A9.dat"}}},
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

// Self-aid: the stock kit used on the merc's own tile.
void TestFirstAidOffersSelfAid()
{
	std::vector<TacticalActorSnapshot> actors{Actor(1, 1001), Actor(2, 1003)};
	actors[0].bleeding = 6;
	actors[0].handMedicalKitPoints = 40;
	actors[0].loadout.primaryHand.item = 201;
	actors[0].loadout.primaryHand.quantity = 1;
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(11,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true, TacticalMapAssetKey{{"A9.dat"}}},
		Turn(0),
		std::vector<TacticalActorSnapshot>(actors), snapshot) == TacticalSnapshotCreateError::None,
		"self-aid fixture is valid");
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1}, TacticalEntityId{9, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	const FullEngineCoopClientIntentRequest self = controller.beginFirstAid(view, TacticalEntityId{1, 1});
	const auto* aid = std::get_if<CoopSession::BeginFirstAidTacticalIntent>(&self.payload);
	CHECK(self && self.actor == (TacticalEntityId{1, 1}) && aid && aid->target == (TacticalEntityId{1, 1}),
		"a bleeding medic holding a kit may treat itself");
	CHECK(!controller.beginFirstAid(view, TacticalEntityId{2, 1}),
		"a teammate two tiles away is still not a patient");
	actors[0].bleeding = 0;
	TacticalWorldSnapshot healthy;
	CHECK(TacticalWorldSnapshot::create(11, TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true, TacticalMapAssetKey{{"A9.dat"}}}, Turn(0),
		std::move(actors), healthy) == TacticalSnapshotCreateError::None, "healthy fixture is valid");
	const FullEngineCoopClientControllerView healthyView = View(healthy, assigned);
	controller.synchronize(healthyView);
	CHECK(!controller.beginFirstAid(healthyView, TacticalEntityId{1, 1}),
		"a medic with nothing to treat does not treat itself");
}

// Melee targeting (F with a knife, punching item or empty hand): only hostiles
// on the eight neighbouring tiles of the merc's level, no aim, a melee intent.
void TestMeleeTargetingOffersOnlyAdjacentHostiles()
{
	// Grid 1001 on a 160-column map: 1162 is diagonal, 1002 beside, 1003 two away.
	std::vector<TacticalActorSnapshot> actors{
		Actor(1, 1001), Actor(3, 1003), Actor(4, 1162), Actor(5, 1002)};
	actors[3].level = 1; // beside, but on the roof
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(11,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true, TacticalMapAssetKey{{"A9.dat"}}},
		Turn(0),
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"melee targeting fixture is valid");
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{8, 1}, TacticalEntityId{9, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.beginAttackTargeting(view, true) && controller.meleeTargeting() &&
		controller.attackTarget() == (TacticalEntityId{4, 1}) && controller.attackAimTime() == 0,
		"melee targeting starts on the adjacent hostile, without aim");
	CHECK(!controller.selectNextTarget(view) || controller.attackTarget() == (TacticalEntityId{4, 1}),
		"the hostile two tiles away and the one on the roof are never offered");
	CHECK(!controller.selectAttackTarget(view, TacticalEntityId{3, 1}) &&
		!controller.selectAttackTarget(view, TacticalEntityId{5, 1}) && !controller.adjustAttackAim(1),
		"a distant or other-level hostile cannot be chosen and a stab has no aim");
	CHECK(!controller.submitAimedFirearmAttack(view) && controller.targetingAttack(),
		"melee targeting never submits an aimed shot");
	const FullEngineCoopClientIntentRequest strike = controller.submitMeleeAttack(view);
	const auto* melee = std::get_if<CoopSession::MeleeAttackTacticalIntent>(&strike.payload);
	CHECK(strike && melee != nullptr && melee->target == (TacticalEntityId{4, 1}) &&
		!controller.targetingAttack() && !controller.meleeTargeting(),
		"Enter sends the stab or punch at the exact adjacent hostile and ends targeting");
	CHECK(controller.beginAttackTargeting(view) && !controller.meleeTargeting() &&
		controller.attackTarget() == (TacticalEntityId{3, 1}) && !controller.submitMeleeAttack(view),
		"fire targeting still offers every hostile and never submits a melee intent");
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
	CHECK(controller.beginDoorSelection(view) && controller.selectedDoorBaseGrid() == 1000 &&
		!controller.submitDoorAction(view, 1) && controller.selectingDoor(),
		"open is not a locked-door menu choice");
	const FullEngineCoopClientIntentRequest pick = controller.submitDoorAction(view, 3);
	const auto* action = std::get_if<CoopSession::DoorActionTacticalIntent>(&pick.payload);
	CHECK(pick && pick.actor == (TacticalEntityId{1, 1}) && action && action->baseGrid == 1000 &&
		action->structureId == 41 && action->action == 3 && !controller.selectingDoor(),
		"a closed door's lockpick choice names the exact door and stock code");
	CHECK(controller.beginDoorSelection(view) && controller.selectNextDoor(view) && controller.selectedDoorOpen() &&
		!controller.submitDoorAction(view, 4),
		"an open door offers no locked-door menu");
}

void TestExactDoorSelectionIsTransactionalAndEpochBound()
{
	const std::vector<TacticalDoorSnapshot> doors{
		TacticalDoorSnapshot{1000, 41, false},
		TacticalDoorSnapshot{1001, 42, true},
		TacticalDoorSnapshot{1162, 43, false}};
	const TacticalWorldSnapshot snapshot = Snapshot(true, 0, doors);
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	CHECK(!controller.selectDoor(view, 1001),
		"an exact door grid cannot be selected outside door mode");
	CHECK(controller.beginDoorSelection(view) &&
		controller.selectedDoorBaseGrid() == 1000,
		"exact door selection fixture begins on the canonical candidate");
	CHECK(controller.selectDoor(view, 1001) &&
		controller.selectedDoorBaseGrid() == 1001 &&
		controller.selectedDoorStructureId() == 42 &&
		controller.selectedDoorOpen(),
		"a clicked published grid selects its exact structure identity and state");

	CHECK(!controller.selectDoor(view, 1002) &&
		!controller.selectDoor(view, 1162) &&
		controller.selectedDoorBaseGrid() == 1001 &&
		controller.selectedDoorStructureId() == 42 &&
		controller.selectedDoorOpen(),
		"missing and nonadjacent grids preserve the prior exact door selection");

	const TacticalWorldSnapshot nextWorld = Snapshot(true, 0, doors, 12);
	const FullEngineCoopClientControllerView nextWorldView =
		View(nextWorld, assigned);
	CHECK(!controller.selectDoor(nextWorldView, 1000) &&
		controller.selectedDoorBaseGrid() == 1001 &&
		controller.selectedDoorStructureId() == 42 &&
		controller.selectedDoorOpen(),
		"a click from another world epoch cannot replace retained door identity");

	const FullEngineCoopClientIntentRequest request =
		controller.submitDoorOpenClose(view);
	const auto* const door =
		std::get_if<CoopSession::DoorOpenCloseTacticalIntent>(&request.payload);
	CHECK(request && request.actor == (TacticalEntityId{1, 1}) &&
		door != nullptr && door->baseGrid == 1001 &&
		door->structureId == 42 && !door->desiredOpen &&
		!controller.selectingDoor(),
		"submission preserves the clicked identity and requests its inverse state");
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

void TestNormalTacticalUiCommandsTranslateWithoutLocalExecution()
{
	using Translation = FullEngineCoopClientCommandTranslationResult;
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	const TacticalEntityId selected{1, 1};
	FullEngineCoopClientIntentRequest output;

	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{MoveToGridCommand{
			selected, 1005, 5, true, false,
			SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::Success,
		"normal tactical move command translates to authoritative intent");
	const auto* move =
		std::get_if<CoopSession::MoveTacticalIntent>(&output.payload);
	CHECK(output && output.actor == selected && move != nullptr &&
		move->destinationGrid == 1005 && move->movementMode == 5 &&
		move->reverse,
		"translated move retains only server-validated intent fields");

	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{SetFacingCommand{
			selected, 7, SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::Success &&
		std::get<CoopSession::FaceTacticalIntent>(output.payload).direction == 7,
		"normal tactical facing command translates to authoritative intent");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{ChangeStanceCommand{
			selected, 1, SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::Success &&
		std::get<CoopSession::StanceTacticalIntent>(output.payload).stance ==
			CoopSession::TacticalIntentStance::Prone,
		"normal tactical stance vocabulary translates to the closed wire enum");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{StopMovementCommand{
			selected, SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::Success &&
		std::holds_alternative<CoopSession::StopTacticalIntent>(output.payload),
		"normal tactical stop command translates to authoritative intent");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{ReloadWeaponCommand{
			selected, true, SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::Success &&
		std::holds_alternative<CoopSession::ReloadTacticalIntent>(output.payload),
		"normal tactical reload command leaves weapon and ammunition selection to authority");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{EndTurnCommand{
			1, SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::Success &&
		output.actor == selected &&
		std::holds_alternative<CoopSession::EndTurnTacticalIntent>(output.payload),
		"normal tactical end-turn command uses the exact selected authority actor");

	const TacticalWorldSnapshot interruptSnapshot = Snapshot(
		true, 0, {}, 11, false, TacticalInterruptPhase::Active, 44,
		false, true);
	const FullEngineCoopClientControllerView interruptView =
		View(interruptSnapshot, assigned);
	const TacticalEntityId interruptActor{2, 1};
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{EndTurnCommand{
			1, SimulationCommandSource::LocalPlayer}},
		interruptView, interruptActor, output) == Translation::Success,
		"normal end-turn input becomes an interrupt pass during the exact active grant");
	const auto* pass =
		std::get_if<CoopSession::PassInterruptTacticalIntent>(&output.payload);
	CHECK(output.actor == interruptActor && pass != nullptr &&
		pass->interruptSerial == 44,
		"translated interrupt pass binds exact actor and serial");
	CHECK(snapshot.find(selected)->grid == 1001 &&
		snapshot.find(selected)->actionPoints == 20,
		"translation never predicts into the committed replica");
}

void TestNormalTacticalUiTranslationFailsClosed()
{
	using Translation = FullEngineCoopClientCommandTranslationResult;
	const TacticalWorldSnapshot snapshot = Snapshot();
	const std::array<TacticalEntityId, 3> assigned{
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1},
		TacticalEntityId{3, 1}};
	const TacticalEntityId selected{1, 1};
	const FullEngineCoopClientControllerView view = View(snapshot, assigned);
	FullEngineCoopClientIntentRequest output;
	output.actor = TacticalEntityId{9, 9};
	output.payload = CoopSession::StopTacticalIntent{};
	output.valid = true;
	const auto outputUnchanged = [&]() {
		return output.valid && output.actor == (TacticalEntityId{9, 9}) &&
			std::holds_alternative<CoopSession::StopTacticalIntent>(
				output.payload);
	};

	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{SetFacingCommand{
			selected, 3, SimulationCommandSource::NetworkPeer,
			TacticalEventPolicy::LocalOnly}},
		view, selected, output) == Translation::WrongSource &&
		outputUnchanged(),
		"network/system/replay commands cannot masquerade as local UI intents");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{MoveToGridCommand{
			TacticalEntityId{2, 1}, 1005, 0, false, false,
			SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::ActorNotSelected &&
		outputUnchanged(),
		"a UI command cannot act through a different assigned actor");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{MoveToGridCommand{
			selected, 30000, 0, false, false,
			SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::InvalidCommand &&
		outputUnchanged(),
		"a destination outside authority dimensions is rejected locally");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{BeginFireWeaponCommand{
			selected, 1200, 0, 0,
			SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::Unsupported &&
		outputUnchanged(),
		"grid-only legacy fire is not guessed into an exact-target co-op attack");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{EndTurnCommand{
			9, SimulationCommandSource::LocalPlayer}},
		view, selected, output) == Translation::InvalidCommand &&
		outputUnchanged(),
		"a legacy next-team value inconsistent with the selected actor is rejected");
	const FullEngineCoopClientControllerView outstanding =
		View(snapshot, assigned, 77);
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{StopMovementCommand{
			selected, SimulationCommandSource::LocalPlayer}},
		outstanding, selected, output) == Translation::ActionsDisabled &&
		outputUnchanged(),
		"the authoritative one-command lock applies before UI translation");
	CHECK(TranslateFullEngineCoopClientUiCommand(
		SimulationCommand{StopMovementCommand{
			selected, SimulationCommandSource::LocalPlayer}},
		view, TacticalEntityId{8, 1}, output) == Translation::InvalidView &&
		outputUnchanged(),
		"an unassigned presentation selection fails closed");
}

struct InventoryFixture
{
	TacticalWorldSnapshot snapshot = Snapshot(true, 0, {{1000,77,false}});
	std::array<TacticalEntityId,3> assigned{{{1,1},{2,1},{8,1}}};
	CoopSession::CoopOwnerInventorySnapshot first;
	CoopSession::CoopOwnerInventorySnapshot second;
	std::array<const CoopSession::CoopOwnerInventorySnapshot*,3> owners;
	InventoryFixture()
	{
		first.sessionEpoch=31;
		first.worldGeneration=11;
		first.baselineId=41;
		first.inventoryRevision=51;
		first.owner[0]=1;
		first.actor={1,1};
		first.usesNewInventory=true;
		for(std::uint16_t slot=0;slot<55;++slot)
			first.slots.push_back({slot,0,0,0,CoopSession::CoopInventorySlotSupport::Empty});
		first.slots[5]={5,24,1,91,CoopSession::CoopInventorySlotSupport::OrdinarySwappable};
		first.slots[14]={14,45,2,99,CoopSession::CoopInventorySlotSupport::OrdinarySwappable};
		first.slots[15]={15,66,1,88,CoopSession::CoopInventorySlotSupport::UnsupportedComplex};
		first.slots[0]={0,77,1,100,CoopSession::CoopInventorySlotSupport::OrdinarySwappable};
		first.slots[7]={7,78,1,100,CoopSession::CoopInventorySlotSupport::OrdinarySwappable};
		second=first;
		second.actor={2,1};
		owners={&first,&second,nullptr};
	}
	FullEngineCoopClientControllerView view(std::uint64_t outstanding=0,bool resync=false)
	{
		auto result=View(snapshot,assigned,outstanding,resync);
		result.ownerInventories=owners.data();
		return result;
	}
};

struct DropFixture : InventoryFixture
{
	DropFixture()
	{
		auto actors = snapshot.actors();
		for (auto& actor : actors)
		{
			actor.presentation.flags = TacticalActorRenderPosePresent;
			actor.presentation.animationDirection = actor.direction;
			actor.presentation.animationSurface = 1;
			actor.presentation.worldXQ8 = (actor.grid % 160 * TacticalWorldCellSize + 5) * TacticalWorldCoordinateScale;
			actor.presentation.worldYQ8 = (actor.grid / 160 * TacticalWorldCellSize + 5) * TacticalWorldCoordinateScale;
		}
		CHECK(TacticalWorldSnapshot::create(11,{160,160},{9,2,0,true},Turn(0),std::move(actors),snapshot) ==
			TacticalSnapshotCreateError::None,"drop fixture has a valid pinned render pose");
	}
};

void TestInventoryDropRequiresExplicitWholePocketConfirmation()
{
	DropFixture fixture; auto view = fixture.view();
	FullEngineCoopClientController controller; controller.synchronize(view);
	const auto before = fixture.first; const auto actorBefore = *fixture.snapshot.find({1,1});
	CHECK(!controller.inventoryDrop(view) && controller.openInventory(view) && !controller.beginInventoryDrop(view),
		"drop cannot occur without inventory source and explicit confirmation");
	CHECK(controller.selectInventorySource(view,5) && !controller.beginInventoryDrop(view) && !controller.inventoryDrop(view),
		"hand source may swap but is not part of the first drop slice");
	CHECK(controller.selectInventorySource(view,0) && !controller.beginInventoryDrop(view) &&
		!controller.selectInventorySource(view,15) && !controller.selectInventorySource(view,16),
		"equipment may swap but equipment, complex and empty sources cannot arm drop");
	CHECK(controller.selectInventorySource(view,14) && !controller.inventoryDrop(view) && controller.beginInventoryDrop(view) &&
		controller.droppingInventory() && !controller.beginInventoryDrop(view),
		"first explicit X arms exactly the chosen ordinary whole pocket stack and is not itself confirmation");
	CHECK(!controller.clickInventorySlot(view,16) && !controller.inventorySwap(view,16) &&
		!controller.selectInventorySource(view,5) && !controller.inspectInventorySlot(view,5) &&
		!controller.beginInventoryTransfer(view) && !controller.beginInventoryPickup(view) &&
		!controller.moveToGrid(view,1010,0) && controller.droppingInventory() && controller.inventorySourceSlot() == 14,
		"drop confirmation consumes inventory/other mode actions without swapping, moving, giving or rebinding source");
	const auto result = controller.inventoryDrop(view);
	const auto* drop = std::get_if<CoopSession::DropInventoryItemTacticalIntent>(&result.payload);
	CHECK(result && result.actor == (TacticalEntityId{1,1}) && drop && drop->sourceSlot == 14 &&
		drop->expectedInventoryRevision == before.inventoryRevision && fixture.first == before &&
		*fixture.snapshot.find({1,1}) == actorBefore && controller.inventoryOpen() && !controller.droppingInventory() &&
		controller.inventorySourceSlot() == 65535 && controller.inventoryInspectedSlot() == 65535,
		"confirmation emits exactly source slot/private token without destination, local item/AP mutation or inventory close");
	CHECK(!controller.inventoryDrop(view) && !controller.beginInventoryDrop(view),
		"submitted choice is spent; receipt loss cannot autonomously replay the drop");
	CHECK(controller.selectInventorySource(view,14) && controller.beginInventoryDrop(view),"new deliberate source can arm a new confirmation");
	controller.cancelInventoryDrop();
	CHECK(!controller.droppingInventory() && controller.inventoryOpen() && controller.inventorySourceSlot() == 65535 &&
		!controller.inventoryDrop(view) && fixture.first == before && controller.selectInventorySource(view,14) &&
		controller.inventorySwap(view,16),"Esc cancellation leaves all authority state unchanged and existing swaps usable");
	CHECK(controller.selectInventorySource(view,14) && controller.beginInventoryDrop(view),"drop can be explicitly armed before closing");
	controller.closeInventory();
	CHECK(!controller.droppingInventory() && !controller.inventoryDrop(view),"I close revokes the drop confirmation");

	FullEngineCoopClientInventoryDropKeyGate key;
	key.release(); CHECK(!key.confirm(),"release before arming cannot pre-confirm a later drop");
	key.arm();
	CHECK(!key.confirm() && !key.confirm(),"held key or repeated key-down cannot confirm the first X");
	key.release(); CHECK(key.confirm() && !key.confirm(),"physical release enables exactly one explicit second-X confirmation");
	key.arm(); key.release(); key.cancel(); CHECK(!key.confirm(),"modal cancellation also revokes the physical confirmation gate");
}

void TestInventoryDropCancelsStaleContextAndPose()
{
	for (unsigned change = 0; change < 23; ++change)
	{
		DropFixture fixture; auto view = fixture.view();
		FullEngineCoopClientController controller; controller.synchronize(view);
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14) && controller.beginInventoryDrop(view),
			"stale drop fixture is explicitly armed");
		switch (change)
		{
			case 0: ++fixture.first.inventoryRevision; break;
			case 1: ++fixture.first.baselineId; break;
			case 2: ++fixture.first.sessionEpoch; break;
			case 3: ++fixture.first.owner[0]; break;
			case 4: fixture.assigned[2] = {9,1}; break;
			case 5: view.outstandingCommandId = 1; break;
			case 6: view.resynchronizing = true; break;
			case 7: fixture.snapshot = Snapshot(true,0,{},12); break;
			case 8: fixture.owners[0] = nullptr; break;
			case 9: view.snapshot = nullptr; break;
			case 10: fixture.first.slots[14] = {14,0,0,0,CoopSession::CoopInventorySlotSupport::Empty}; break;
			default:
			{
				auto actors = fixture.snapshot.actors(); auto& actor = actors[0]; auto turn = Turn(0);
				switch (change)
				{
					case 11: ++actor.grid; actor.presentation.worldXQ8 += TacticalWorldCellSize*TacticalWorldCoordinateScale; break;
					case 12: actor.level = 1; break;
					case 13: ++actor.direction; actor.presentation.animationDirection = actor.direction; break;
					case 14: actor.stance = TacticalStance::Crouched; break;
					case 15: ++actor.animation; break;
					case 16: ++actor.presentation.worldXQ8; break;
					case 17: ++actor.presentation.worldYQ8; break;
					case 18: ++actor.presentation.bodyType; break;
					case 19: actor.life = 14; break;
					case 20: actor.actionPoints = 0; break;
					case 21: turn.activeTeam = 1; break;
					case 22: ++actor.presentation.heightAdjustment; break;
				}
				CHECK(TacticalWorldSnapshot::create(11,{160,160},{9,2,0,true},turn,std::move(actors),fixture.snapshot) ==
					TacticalSnapshotCreateError::None,"changed drop pose/readiness remains canonical public state");
			}
		}
		CHECK(!controller.inventoryDrop(view) && !controller.droppingInventory() &&
			controller.inventorySourceSlot() == 65535 && controller.inventoryInspectedSlot() == 65535,
			"direct confirmation rejects and cancels stale owner/token/context/pose before synchronization");
		controller.synchronize(view);
		CHECK(!controller.droppingInventory() && !controller.inventoryDrop(view),"later synchronization never re-arms or replays a cancelled drop");
	}
	DropFixture fixture; auto view = fixture.view();
	FullEngineCoopClientController controller; controller.synchronize(view);
	CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14) && controller.beginInventoryDrop(view),
		"passive cancellation fixture armed");
	++fixture.first.inventoryRevision;
	controller.synchronize(view);
	CHECK(!controller.droppingInventory() && controller.inventorySourceSlot() == 65535 &&
		controller.inventoryNotice()[0] != 0,"private updates cancel even without a confirmation attempt");
}

void TestInventoryOrdinarySwapsArePureAndSlotBounded()
{
	InventoryFixture fixture;
	auto view=fixture.view();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.openInventory(view) && controller.inventoryOpen(),
		"owned current-baseline inventory opens through normal controller state");
	const auto before=fixture.first;
	CHECK(!controller.selectInventorySource(view,6) &&
		!controller.selectInventorySource(view,15) && !controller.selectInventorySource(view,7) &&
		!controller.selectInventorySource(view,55) && !controller.selectInventorySource(view,65535),
		"empty, complex, worn LBE and out-of-range slots cannot become a movable item cursor");
	CHECK(controller.selectInventorySource(view,14) && controller.inventorySourceSlot()==14,
		"ordinary carried stack becomes only a local source-slot choice");
	for(std::uint16_t destination : {7,12,13,15,55,65535})
		CHECK(!controller.inventorySwap(view,destination) && controller.inventorySourceSlot()==14,
			"unsupported LBE/sling/knife/complex/out-of-range destination is rejected without losing source");
	CHECK(!controller.inventorySwap(view,14) && controller.inventorySourceSlot()==14,
		"same-slot direct swap cannot generate a command");
	const auto equip=controller.inventorySwap(view,6);
	CHECK(equip && equip.actor==(TacticalEntityId{1,1}) &&
		std::holds_alternative<CoopSession::SwapInventorySlotsTacticalIntent>(equip.payload) &&
		controller.inventorySourceSlot()==65535,
		"equip to empty off-hand emits one typed authority request and clears local selection");
	const auto payload=std::get<CoopSession::SwapInventorySlotsTacticalIntent>(equip.payload);
	CHECK(payload.sourceSlot==14 && payload.destinationSlot==6 && payload.expectedInventoryRevision==51 &&
		fixture.first==before && fixture.snapshot.find({1,1})->actionPoints==20,
		"request carries exact private token and both slots without moving objects or predicting AP");
	CHECK(!controller.inventorySwap(view,6),"no selected source means no automatic second submission");
	CHECK(controller.selectInventorySource(view,5),"held item can be selected for unequip");
	const auto unequip=controller.inventorySwap(view,16);
	CHECK(unequip && std::get<CoopSession::SwapInventorySlotsTacticalIntent>(unequip.payload).destinationSlot==16 &&
		fixture.first==before,"hand-to-empty-pocket unequip is supported without local mutation");
	CHECK(controller.selectInventorySource(view,14),"ordinary stack can be reselected");
	const auto pocket=controller.inventorySwap(view,17);
	CHECK(pocket && std::get<CoopSession::SwapInventorySlotsTacticalIntent>(pocket.payload).sourceSlot==14 &&
		fixture.first.slots[14].count==2,"pocket-to-pocket operation preserves the entire stack on the client");
	CHECK(controller.selectInventorySource(view,14) && controller.selectInventorySource(view,14) &&
		controller.inventorySourceSlot()==65535,"clicking the selected source again cancels its local highlight");
}

void TestOrdinaryArmorAndFaceSwapsKeepTransferAndDropBounded()
{
	for (const std::uint16_t equipment : {0,1,2,3,4})
	{
		InventoryFixture fixture;
		fixture.first.slots[equipment] = {equipment,77,1,73,CoopSession::CoopInventorySlotSupport::OrdinarySwappable};
		auto view = fixture.view();
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		const auto before = fixture.first;
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,equipment),
			"ordinary helmet, vest, legs and face equipment can be selected without local unequip");
		CHECK(!controller.beginInventoryTransfer(view) && !controller.selectInventoryRecipient(view,{2,1}) &&
			!controller.beginInventoryDrop(view) && controller.inventorySourceSlot()==equipment && fixture.first==before,
			"widening swaps does not implicitly give or drop worn equipment");
		const auto remove = controller.inventorySwap(view,16);
		const auto* request = std::get_if<CoopSession::SwapInventorySlotsTacticalIntent>(&remove.payload);
		CHECK(remove && request && request->sourceSlot==equipment && request->destinationSlot==16 &&
			request->expectedInventoryRevision==51 && fixture.first==before,
			"ordinary worn equipment removal emits one exact-revision native swap request");
		CHECK(controller.selectInventorySource(view,14),"ordinary carried source can request native equipment placement");
		const auto equip = controller.inventorySwap(view,equipment);
		CHECK(equip && std::get<CoopSession::SwapInventorySlotsTacticalIntent>(equip.payload).destinationSlot==equipment &&
			fixture.first==before,"server alone validates armor class, face compatibility and whole-stack capacity");
		fixture.first.slots[equipment].support = CoopSession::CoopInventorySlotSupport::UnsupportedComplex;
		CHECK(!controller.selectInventorySource(view,equipment) && controller.selectInventorySource(view,14) &&
			!controller.inventorySwap(view,equipment),"complex worn objects still cannot be moved or overwritten");
		controller.closeInventory();
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14) && controller.beginInventoryTransfer(view) &&
			!controller.inventoryTransfer(view,controller.inventoryRecipient(),equipment),
			"giving into armor or face equipment stays outside the hand/pocket transfer scope");
	}
}

void TestInventoryModalControlsAndBusyInspection()
{
	InventoryFixture fixture;
	auto view=fixture.view();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.beginDestinationEntry(view) && controller.appendDestinationDigit(9) &&
		controller.openInventory(view) && !controller.enteringDestination(),
		"inventory entry cancels pending numeric movement");
	CHECK(controller.beginAttackTargeting(view) && !controller.inventoryOpen() &&
		controller.openInventory(view) && !controller.targetingAttack(),
		"explicit modal changes cancel previous inventory/fire targeting rather than leak modes");
	CHECK(controller.beginDoorSelection(view) && !controller.inventoryOpen() &&
		controller.openInventory(view) && !controller.selectingDoor(),
		"inventory entry cancels door targeting and preserves modal exclusivity");
	CHECK(!controller.moveToGrid(view,1010,0) && !controller.submitRelativeMove(view,0,1,0) &&
		!controller.face(view,2) && !controller.stance(view,CoopSession::TacticalIntentStance::Prone) &&
		!controller.stop(view) && !controller.endTurn(view) && !controller.reload(view) &&
		!controller.beginFirstAid(view,{2,1}),
		"open inventory suppresses terrain/tactical intents rather than becoming a local simulation path");
	CHECK(controller.selectInventorySource(view,14),"source selected before outstanding request appears");
	auto pending=fixture.view(77);
	controller.synchronize(pending);
	CHECK(controller.inventoryOpen() && controller.inventorySourceSlot()==65535 &&
		!controller.selectInventorySource(pending,14) && !controller.inventorySwap(pending,6),
		"pending command retains inspectable panel but clears source and prevents another operation");
	controller.closeInventory();
	CHECK(controller.openInventory(pending),"inventory can be opened read-only while a receipt is outstanding");
	fixture.snapshot=Snapshot(true,1);
	view=fixture.view();
	controller.synchronize(view);
	CHECK(controller.inventoryOpen() && !controller.actionsEnabled(view) &&
		!controller.selectInventorySource(view,14),"enemy turn keeps inventory inspection, not equip authority");
	controller.closeInventory();
	CHECK(controller.openInventory(view),"enemy turn permits opening the owned inventory for inspection");
	fixture.snapshot=Snapshot(true,0,{},11,true);
	view=fixture.view();
	controller.synchronize(view);
	CHECK(controller.inventoryOpen() && !controller.selectInventorySource(view,14),
		"native command gate permits read-only panel but blocks item requests");
	fixture.snapshot=Snapshot();
	view=fixture.view();
	controller.synchronize(view);
	CHECK(controller.inventoryOpen() && controller.selectInventorySource(view,14),
		"new ordinary player phase recovers selection only after the real gate clears");
	controller.closeInventory();
	CHECK(!controller.inventoryOpen() && controller.inventorySourceSlot()==65535 && controller.moveToGrid(view,1010,0),
		"explicit close drops all inventory selection and restores ordinary typed movement");
}

void TestInventoryInspectionNeverBecomesASwapCursor()
{
	InventoryFixture fixture;
	auto view=fixture.view();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.openInventory(view) && controller.inventoryInspectedSlot()==65535,
		"fresh owner inventory starts with no retained inspected item");
	const auto before=fixture.first;
	CHECK(controller.inspectInventorySlot(view,15) && controller.inventoryInspectedSlot()==15 &&
		controller.inventorySourceSlot()==65535 && !controller.selectInventorySource(view,15) &&
		!controller.inventorySwap(view,6),
		"complex stack can be inspected without selecting or submitting a swap");
	CHECK(controller.inspectInventorySlot(view,7) && controller.inventoryInspectedSlot()==7 &&
		!controller.selectInventorySource(view,7) && controller.inventorySourceSlot()==65535,
		"unsupported worn LBE slot can expose its details without equipment authority");
	CHECK(!controller.inspectInventorySlot(view,6) && !controller.inspectInventorySlot(view,55) &&
		!controller.inspectInventorySlot(view,65535) && controller.inventoryInspectedSlot()==7,
		"empty and out-of-range clicks cannot invent or replace inspected item details");
	CHECK(controller.selectInventorySource(view,14) && controller.inventoryInspectedSlot()==14 &&
		controller.inventorySourceSlot()==14,"ordinary source selection also inspects the same item");
	CHECK(controller.inspectInventorySlot(view,15) && !controller.inventorySwap(view,15) &&
		controller.inventoryInspectedSlot()==15 && controller.inventorySourceSlot()==14,
		"clicked unsupported destination inspects its contents without silently forgetting a live source");
	CHECK(controller.inspectInventorySlot(view,7) && !controller.inventorySwap(view,7) &&
		controller.inventoryInspectedSlot()==7 && controller.inventorySourceSlot()==14 &&
		fixture.first==before && fixture.snapshot.find({1,1})->actionPoints==20,
		"inspection and invalid destination clicks never mutate private items or public action points");
	CHECK(controller.selectInventorySource(view,14) && controller.inventorySourceSlot()==65535 &&
		controller.inventoryInspectedSlot()==14,
		"canceling a swap source still permits reading that item's details");
	auto pending=fixture.view(77);
	CHECK(controller.inspectInventorySlot(pending,15) && !controller.selectInventorySource(pending,14),
		"pending command permits inspection but cannot open a second operation");
	controller.synchronize(pending);
	CHECK(controller.inventoryInspectedSlot()==15 && controller.inventorySourceSlot()==65535,
		"outstanding-command synchronization does not erase read-only inspection");
	fixture.snapshot=Snapshot(true,1);
	view=fixture.view();
	CHECK(controller.inspectInventorySlot(view,0) && !controller.inventorySwap(view,6),
		"enemy turn allows owned equipment inspection without command authority");
	controller.synchronize(view);
	CHECK(controller.inventoryInspectedSlot()==0,"enemy phase retains current inspected item details");
	fixture.snapshot=Snapshot(true,0,{},11,true);
	view=fixture.view();
	CHECK(controller.inspectInventorySlot(view,14) && !controller.selectInventorySource(view,14),
		"native busy gate also remains inspectable without permitting swaps");
	controller.closeInventory();
	CHECK(controller.inventoryInspectedSlot()==65535 && !controller.inspectInventorySlot(view,0),
		"closing the panel revokes inspection as well as the selected source");
	for(const bool remove : {false,true})
	{
		CHECK(controller.openInventory(view) && controller.inspectInventorySlot(view,15),
			"inspection fixture begins with a currently occupied complex slot");
		if(remove)fixture.first.slots.resize(15);
		else fixture.first.slots[15]={15,0,0,0,CoopSession::CoopInventorySlotSupport::Empty};
		controller.synchronize(view);
		CHECK(controller.inventoryOpen() && controller.inventoryInspectedSlot()==65535,
			"slot removal or empty replacement clears stale inspection even before a private token changes");
		fixture.first=before;
	}
}

void TestInventoryContextChangesRevokeRetainedChoices()
{
	for(unsigned change=0;change<11;++change)
	{
		InventoryFixture fixture;
		auto view=fixture.view();
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14),"context fixture opens selected item");
		switch(change)
		{
			case 0: ++fixture.first.inventoryRevision; break;
			case 1: ++fixture.first.baselineId; break;
			case 2: ++fixture.first.sessionEpoch; break;
			case 3: fixture.owners[0]=nullptr; break;
			case 4: view.resynchronizing=true; break;
			case 5: fixture.snapshot=Snapshot(true,0,{},12); fixture.first.worldGeneration=12; break;
			case 6: fixture.assigned[0]={9,1}; break;
			case 7: ++fixture.first.actor.incarnation; break;
			case 8: view.snapshot=nullptr; break;
			case 9: fixture.first.baselineId=0; break;
			case 10: fixture.first.slots[14].slot=15; break;
		}
		CHECK(!controller.inventorySwap(view,6),
			"source operation fails immediately on private token/context change, even before synchronize");
		if(change!=0)CHECK(!controller.inspectInventorySlot(view,15),
			"lost session/baseline/world/assignment or resync rejects a new inspection before synchronize");
		controller.synchronize(view);
		CHECK(controller.inventorySourceSlot()==65535 && controller.inventoryInspectedSlot()==65535 &&
			(change==0 ? controller.inventoryOpen() : !controller.inventoryOpen()),
			"private revision clears source; baseline/session/actor/world/assignment/resync replacements close the panel");
		if(change==0)
			CHECK(controller.selectInventorySource(view,14) && controller.inventorySwap(view,6),
				"a new explicit selection may use the refreshed private token; no prior request is replayed");
	}
	InventoryFixture fixture;
	auto view=fixture.view();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14) &&
		controller.selectActor(view,{2,1}) && !controller.inventoryOpen() && controller.inventorySourceSlot()==65535 &&
		controller.inventoryInspectedSlot()==65535,
		"selecting another exact owned actor closes previous owner's inventory rather than carrying its source over");
	CHECK(controller.openInventory(view) && controller.selectPrevious(view) && !controller.inventoryOpen(),
		"relative roster selection also revokes inventory modal context");
	fixture.owners={nullptr,nullptr,nullptr};
	CHECK(!controller.openInventory(view),"no admitted owner state cannot open a client-local inventory fallback");
}

void TestInventoryTransferUsesBothPrivateRevisionsWithoutMutation()
{
	InventoryFixture fixture;
	fixture.second.inventoryRevision = 63;
	auto view = fixture.view();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.openInventory(view) && !controller.beginInventoryTransfer(view) &&
		controller.inventoryNotice()[0], "give without a selected item provides a visible explanation");
	CHECK(controller.selectInventorySource(view,14) && controller.beginInventoryTransfer(view) &&
		controller.inventoryRecipient() == (TacticalEntityId{2,1}) &&
		controller.inventoryDisplayActor() == (TacticalEntityId{2,1}) &&
		controller.selectedActor() == (TacticalEntityId{1,1}),
		"give opens the owned recipient inventory while the source merc remains selected");
	const auto first = fixture.first;
	const auto second = fixture.second;
	CHECK(!controller.selectInventorySource(view,5) && !controller.inspectInventorySlot(view,5) &&
		!controller.inventorySwap(view,6), "recipient view cannot select or swap a source merc's slots");
	for (std::uint16_t slot : {0,5,7,14,15,55,65535})
		CHECK(!controller.clickInventorySlot(view,slot) && controller.transferringInventory() &&
			controller.inventorySourceSlot() == 14,
			"occupied, complex and unsupported destinations leave the same whole-stack transfer selected");
	CHECK(controller.selectNextInventoryRecipient(view) && controller.selectNextInventoryRecipient(view,false) &&
		controller.inventoryRecipient() == (TacticalEntityId{2,1}), "recipient cycling skips source and unavailable actors");
	const auto result = controller.clickInventorySlot(view,6);
	CHECK(result && result.actor == (TacticalEntityId{1,1}) &&
		std::holds_alternative<CoopSession::TransferInventoryItemTacticalIntent>(result.payload),
		"empty recipient off-hand click emits one exact typed give intent");
	if (result)
	{
		const auto& intent = std::get<CoopSession::TransferInventoryItemTacticalIntent>(result.payload);
		CHECK(intent.target == (TacticalEntityId{2,1}) && intent.sourceSlot == 14 && intent.destinationSlot == 6 &&
			intent.expectedInventoryRevision == 51 && intent.expectedTargetInventoryRevision == 63,
			"give binds both actor inventories independently, not just public world revision");
	}
	CHECK(fixture.first == first && fixture.second == second && fixture.snapshot.find({1,1})->actionPoints == 20 &&
		fixture.snapshot.find({2,1})->actionPoints == 20 && !controller.transferringInventory() &&
		controller.inventorySourceSlot() == 65535 && !controller.inventoryTransfer(view,{2,1},6),
		"submission clears presentation choices without changing either inventory/AP or automatically resubmitting");
	CHECK(controller.selectInventorySource(view,14) && controller.selectInventoryRecipient(view,{2,1}),
		"explicit recipient entry supports test-driver and mouse-independent input");
	controller.cancelInventoryTransfer();
	CHECK(controller.inventoryOpen() && !controller.transferringInventory() && controller.inventorySourceSlot() == 65535 &&
		fixture.first == first && fixture.second == second && !controller.moveToGrid(view,1010,0),
		"Esc/G cancellation returns to source inventory with no drop, transfer or movement");
}

void TestInventoryTransferAndDropFollowInterruptEligibility()
{
	// During an active interrupt the authority admits an inventory action only
	// when every AP-spending actor is interrupt-eligible; Resolving admits none.
	struct Case { TacticalInterruptPhase phase; bool sourceEligible, recipientEligible, transfer, drop; };
	const Case cases[] = {
		{TacticalInterruptPhase::Active, true, true, true, true},
		{TacticalInterruptPhase::Active, true, false, false, true},
		{TacticalInterruptPhase::Active, false, true, false, false},
		// Eligibility is only representable during an active interrupt.
		{TacticalInterruptPhase::Resolving, false, false, false, false},
	};
	for (const auto& test : cases)
	{
		DropFixture fixture;
		auto actors = fixture.snapshot.actors();
		for (auto& actor : actors)
			actor.interruptActionEligible = actor.id == TacticalEntityId{1,1} ? test.sourceEligible :
				actor.id == TacticalEntityId{2,1} ? test.recipientEligible : false;
		CHECK(TacticalWorldSnapshot::create(11, {160,160}, {9,2,0,true},
			Turn(0, false, test.phase, 5), std::move(actors), fixture.snapshot) == TacticalSnapshotCreateError::None,
			"interrupt inventory fixture snapshot is valid");
		auto view = fixture.view();
		FullEngineCoopClientController controller; controller.synchronize(view);
		const bool opened = controller.openInventory(view) && controller.selectInventorySource(view,14);
		const bool transfer = opened && controller.beginInventoryTransfer(view) &&
			controller.inventoryRecipient() == (TacticalEntityId{2,1});
		CHECK(transfer == test.transfer,
			"transfer during an interrupt requires both actors to be interrupt-eligible and never runs while resolving");
		if (transfer)
		{
			const auto result = controller.clickInventorySlot(view,6);
			CHECK(result && std::holds_alternative<CoopSession::TransferInventoryItemTacticalIntent>(result.payload),
				"eligible interrupt transfer emits one exact typed give intent");
		}
		controller.cancelInventoryTransfer();
		const bool drop = (controller.inventoryOpen() || controller.openInventory(view)) &&
			controller.selectInventorySource(view,14) && controller.beginInventoryDrop(view);
		CHECK(drop == test.drop,
			"drop during an interrupt requires the selected actor to be interrupt-eligible and never runs while resolving");
	}
}

void TestInventoryTransferInvalidatesBothContexts()
{
	for (unsigned change = 0; change < 15; ++change)
	{
		InventoryFixture fixture;
		auto view = fixture.view();
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14) &&
			controller.beginInventoryTransfer(view), "transfer context fixture starts with source and exact recipient");
		switch (change)
		{
			case 0: ++fixture.first.inventoryRevision; break;
			case 1: ++fixture.second.inventoryRevision; break;
			case 2: ++fixture.second.baselineId; break;
			case 3: ++fixture.second.sessionEpoch; break;
			case 4: fixture.second.owner[0] = 2; break;
			case 5: fixture.owners[1] = nullptr; break;
			case 6: fixture.assigned[1] = {2,2}; break;
			case 7: fixture.assigned[2] = {9,1}; break;
			case 8: view.outstandingCommandId = 7; break;
			case 9: view.resynchronizing = true; break;
			case 10: fixture.snapshot = Snapshot(true,0,{},12); break;
			case 11: ++fixture.first.baselineId; break;
			case 12: ++fixture.first.sessionEpoch; break;
			case 13: fixture.first.owner[0] = fixture.second.owner[0] = 2; break;
			case 14: fixture.snapshot = Snapshot(true,1); break;
		}
		CHECK(!controller.inventoryTransfer(view,{2,1},6) && !controller.selectNextInventoryRecipient(view),
			"stale source, target, session, world, assignment and busy states fail before synchronization");
		controller.synchronize(view);
		CHECK(!controller.transferringInventory() && controller.inventorySourceSlot() == 65535 &&
			controller.inventoryInspectedSlot() == 65535,
			"synchronization invalidates both retained choices without replaying the transfer");
	}
}

void TestInventoryTransferRecipientsAreOwnedReadyHumans()
{
	for (unsigned change = 0; change < 10; ++change)
	{
		InventoryFixture fixture;
		auto actors = fixture.snapshot.actors();
		switch (change)
		{
			case 0: actors[1].life = 14; break;
			case 1: actors[1].presentation.bodyType = 4; break;
			case 2: actors[1].grid += 5; break;
			case 3: actors[1].level = 1; break;
			case 4: actors[1].actionPoints = 2; break;
			case 5: actors[0].actionPoints = 2; break;
			case 6: actors[1].medicalService.receivingCount = 1; break;
			case 7: actors[0].medicalService.providing = true; break;
			case 8: fixture.owners[1] = nullptr; break;
			case 9: fixture.second.owner[0] = 2; break;
		}
		CHECK(TacticalWorldSnapshot::create(11,{160,160},{9,2,0,true},Turn(0),
			std::move(actors),fixture.snapshot) == TacticalSnapshotCreateError::None,
			"recipient rejection fixture is a valid public snapshot");
		auto view = fixture.view();
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14) &&
			!controller.selectInventoryRecipient(view,{1,1}) && !controller.selectInventoryRecipient(view,{3,1}) &&
			!controller.selectInventoryRecipient(view,{2,2}) && !controller.beginInventoryTransfer(view) &&
			!controller.transferringInventory() && controller.inventorySourceSlot() == 14 && controller.inventoryNotice()[0],
			"give rejects self, hostile/unassigned/reincarnated, unready, remote, nonhuman or differently owned recipients");
	}
}

struct PickupFixture : InventoryFixture
{
	PickupFixture()
	{
		first.groundGrid = 1001;
		first.groundLevel = 0;
		first.groundItems = {
			{{30,7},{0,45,2,99,CoopSession::CoopInventorySlotSupport::OrdinarySwappable,
				CoopSession::CoopInventoryStatusKind::MedicalKitPoints,180}},
			{{31,8},{1,77,2,6,CoopSession::CoopInventorySlotSupport::OrdinarySwappable,
				CoopSession::CoopInventoryStatusKind::AmmoRounds,12}}};
	}
};

void TestGroundPickupSelectsExactAuthorityIdAndEmptyPocket()
{
	PickupFixture fixture;
	auto view = fixture.view();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	const auto original = fixture.first;
	CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14) &&
		controller.beginInventoryPickup(view) && controller.pickingUpInventory() &&
		controller.inventorySourceSlot() == 65535 && controller.inventoryInspectedSlot() == 65535 &&
		controller.inventoryGroundItem() == (TacticalWorldItemId{30,7}),
		"P opens an exact-tile authority ground list and clears the former inventory swap source");
	CHECK(!controller.selectInventorySource(view,5) && !controller.inspectInventorySlot(view,14) &&
		!controller.inventorySwap(view,16) && !controller.beginInventoryTransfer(view) &&
		!controller.selectInventoryRecipient(view,{2,1}),
		"ground selection cannot become a held inventory source, swap or give action");
	CHECK(!controller.selectInventoryGroundItem(view,{30,8}) && !controller.selectInventoryGroundItem(view,{}) &&
		controller.inventoryGroundItem() == (TacticalWorldItemId{30,7}),
		"unknown/reused world-item incarnations cannot replace the selected server identity");
	CHECK(controller.selectNextInventoryGroundItem(view) && controller.inventoryGroundItem() == (TacticalWorldItemId{31,8}) &&
		controller.selectNextInventoryGroundItem(view) && controller.inventoryGroundItem() == (TacticalWorldItemId{30,7}) &&
		controller.selectNextInventoryGroundItem(view,false) && controller.inventoryGroundItem() == (TacticalWorldItemId{31,8}),
		"Tab/brackets cycle the bounded sorted authority list in both directions without changing actors");
	for (std::uint16_t slot : {0,5,6,7,13,14,15,55,65535})
		CHECK(!controller.clickInventorySlot(view,slot) && controller.pickingUpInventory() &&
			controller.inventoryGroundItem() == (TacticalWorldItemId{31,8}),
			"only exact empty pockets accept pickup; hands, occupied slots, equipment and invalid indices retain selection");
	CHECK(controller.inventoryPickupActionsEnabled(view),"pickup readiness is independent of inventory swap source");
	const auto request = controller.clickInventorySlot(view,16);
	CHECK(request && request.actor == (TacticalEntityId{1,1}) &&
		std::holds_alternative<CoopSession::PickupWorldItemTacticalIntent>(request.payload),
		"an empty pocket click emits one typed pickup intent");
	if (request)
	{
		const auto& item = std::get<CoopSession::PickupWorldItemTacticalIntent>(request.payload);
		CHECK(item.item == (TacticalWorldItemId{31,8}) && item.destinationSlot == 16 && item.expectedInventoryRevision == 51,
			"pickup command binds the stable world item, exact destination and private inventory revision");
	}
	CHECK(fixture.first == original && fixture.snapshot.find({1,1})->actionPoints == 20 &&
		controller.inventoryOpen() && !controller.pickingUpInventory() && !controller.inventoryGroundItem().valid() &&
		!controller.inventoryPickup(view,16),
		"submission clears only local mode; no pool item, pocket, AP or command replay is created locally");
	CHECK(controller.selectInventoryGroundItem(view,{30,7}) && controller.pickingUpInventory(),
		"explicit item selection can enter pickup mode for the installed controller driver");
	controller.cancelInventoryPickup();
	CHECK(controller.inventoryOpen() && !controller.pickingUpInventory() && fixture.first == original &&
		!controller.moveToGrid(view,1010,0) && controller.selectInventorySource(view,14) && controller.inventorySwap(view,16),
		"Esc/P cancels without dropping or moving, then ordinary inventory swaps still work");
	CHECK(controller.selectInventorySource(view,14) && controller.beginInventoryTransfer(view) &&
		!controller.beginInventoryPickup(view) && controller.transferringInventory(),
		"pickup cannot silently replace a live give recipient");
}

void TestGroundPickupContextChangesInvalidateBeforeSubmit()
{
	for (unsigned change = 0; change < 15; ++change)
	{
		PickupFixture fixture;
		auto view = fixture.view();
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		CHECK(controller.openInventory(view) && controller.beginInventoryPickup(view),"pickup context fixture starts selected");
		switch (change)
		{
			case 0: ++fixture.first.inventoryRevision; break;
			case 1: ++fixture.first.baselineId; break;
			case 2: ++fixture.first.sessionEpoch; break;
			case 3: fixture.first.owner[0] = 2; break;
			case 4: ++fixture.first.groundItems[0].id.incarnation; break;
			case 5: fixture.first.groundItems.clear(); break;
			case 6: ++fixture.first.groundGrid; break;
			case 7: fixture.first.groundLevel = 1; break;
			case 8: fixture.assigned[2] = {9,1}; break;
			case 9: view.resynchronizing = true; break;
			case 10: fixture.snapshot = Snapshot(true,0,{},12); break;
			case 11: fixture.owners[0] = nullptr; break;
			case 12: view.snapshot = nullptr; break;
			case 13: case 14:
			{
				auto actors = fixture.snapshot.actors();
				if (change == 13) ++actors[0].grid;
				else actors[0].level = 1;
				CHECK(TacticalWorldSnapshot::create(11,{160,160},{9,2,0,true},Turn(0),
					std::move(actors),fixture.snapshot) == TacticalSnapshotCreateError::None,
					"changed actor tile/floor remains valid public state");
				break;
			}
		}
		CHECK(!controller.inventoryPickup(view,16) && !controller.inventoryPickupActionsEnabled(view) &&
			!controller.selectNextInventoryGroundItem(view) && !controller.selectInventoryGroundItem(view,{31,8}),
			"stale list, tile/floor, ownership, assignment, session, world or private revision cannot submit or silently rebind");
		controller.synchronize(view);
		CHECK(!controller.pickingUpInventory() && !controller.inventoryGroundItem().valid() &&
			controller.inventorySourceSlot() == 65535 && controller.inventoryInspectedSlot() == 65535,
			"synchronization revokes the pending ground item without carrying a new object into its selection");
	}
}

void TestGroundPickupBusyInspectionAndEmptyScope()
{
	for (unsigned busy = 0; busy < 6; ++busy)
	{
		PickupFixture fixture;
		auto actors = fixture.snapshot.actors();
		if (busy == 3) actors[0].actionPoints = 0;
		if (busy == 4) actors[0].life = 14;
		if (busy == 5) actors[0].medicalService.providing = true;
		CHECK(TacticalWorldSnapshot::create(11,{160,160},{9,2,0,true},Turn(busy == 2 ? 1 : 0,busy == 1),
			std::move(actors),fixture.snapshot) == TacticalSnapshotCreateError::None,"busy pickup inspection fixture");
		auto view = fixture.view(busy == 0 ? 77 : 0);
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		CHECK(controller.openInventory(view) && controller.beginInventoryPickup(view) &&
			controller.selectNextInventoryGroundItem(view) && !controller.inventoryPickupActionsEnabled(view) &&
			!controller.inventoryPickup(view,16) && controller.inventoryNotice()[0],
			"outstanding, blocked, enemy phase, no AP, unconscious or servicing actors can inspect ground without pickup authority");
		controller.synchronize(view);
		CHECK(controller.pickingUpInventory() && controller.inventoryGroundItem() == (TacticalWorldItemId{31,8}),
			"busy-state synchronization keeps current read-only ground details, not a false empty list");
	}
	PickupFixture fixture;
	fixture.first.groundItems.clear();
	auto view = fixture.view();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.openInventory(view) && controller.beginInventoryPickup(view) && controller.pickingUpInventory() &&
		!controller.inventoryGroundItem().valid() && !controller.selectNextInventoryGroundItem(view) &&
		!controller.inventoryPickup(view,16),"an authoritative empty exact-tile list is inspectable without inventing a target");
	controller.cancelInventoryPickup();
	fixture.first.groundGrid = -1;
	fixture.first.groundLevel = -1;
	CHECK(!controller.beginInventoryPickup(view) && !controller.pickingUpInventory() && controller.inventoryNotice()[0],
		"unavailable ground projection is explicitly different from no supported items on a known tile");
	fixture.first.groundGrid = 1001;
	fixture.first.groundLevel = 0;
	for (std::uint16_t index = 0; index < 64; ++index)
		fixture.first.groundItems.push_back({{static_cast<std::uint32_t>(70000 + index),9},
			{index,45,1,100,CoopSession::CoopInventorySlotSupport::OrdinarySwappable,
				CoopSession::CoopInventoryStatusKind::Condition,0}});
	fixture.first.groundItemsTruncated = true;
	CHECK(controller.beginInventoryPickup(view),"bounded capped ground list can be browsed without guessing omitted items");
	for (unsigned index = 0; index < 64; ++index)
	{
		CHECK(controller.inventoryGroundItem() == (TacticalWorldItemId{70000 + index,9}),
			"every capped-list entry retains its exact world-item identity");
		CHECK(controller.selectNextInventoryGroundItem(view),"all64 listed entries can be reached through normal item cycling");
	}
	CHECK(controller.inventoryGroundItem() == (TacticalWorldItemId{70000,9}) &&
		!controller.selectInventoryGroundItem(view,{70064,9}),
		"capped list wraps after the64th entry and cannot select an item the server did not publish");
}
}

void TestInventoryReadinessLossRevokesSourceWithoutExternalSynchronization()
{
	for (unsigned change = 0; change < 5; ++change)
	{
		InventoryFixture fixture;
		auto view = fixture.view();
		FullEngineCoopClientController controller;
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,14),
			"direct readiness fixture selects a source");
		if (change == 0) view.outstandingCommandId = 77;
		else if (change == 1) fixture.snapshot = Snapshot(true,1);
		else if (change == 2) fixture.snapshot = Snapshot(true,0,{},11,true);
		else if (change == 3) fixture.snapshot = Snapshot(true,0,{},11,false,TacticalInterruptPhase::Resolving,9);
		else fixture.snapshot = Snapshot(true,0,{},11,false,TacticalInterruptPhase::Active,9);
		CHECK(!controller.inventorySwap(view,6) && controller.inventorySourceSlot()==65535 &&
			controller.inventoryOpen() && controller.inspectInventorySlot(view,15),
			"direct confirmation revokes blocked source while retaining read-only owned inspection");
		fixture.snapshot = Snapshot(); view.outstandingCommandId = 0;
		CHECK(!controller.inventorySwap(view,6) && controller.selectInventorySource(view,14) &&
			controller.inventorySwap(view,6), "readiness recovery requires a deliberate fresh source");
	}
}

void TestInventoryOrdinaryEquipmentRemainsAuthorityValidated()
{
	for (std::uint16_t equipment : {0,1,2,3,4})
	{
		InventoryFixture fixture;
		fixture.first.slots[equipment] = {equipment,77,1,73,CoopSession::CoopInventorySlotSupport::OrdinarySwappable};
		auto view = fixture.view();
		FullEngineCoopClientController controller;
		const auto before = fixture.first;
		CHECK(controller.openInventory(view) && controller.selectInventorySource(view,equipment),
			"ordinary armor and face equipment may become a retained source");
		const auto remove = controller.inventorySwap(view,16);
		const auto* request = std::get_if<CoopSession::SwapInventorySlotsTacticalIntent>(&remove.payload);
		CHECK(remove && request && request->sourceSlot==equipment && request->destinationSlot==16 &&
			request->expectedInventoryRevision==51 && fixture.first==before,
			"worn equipment emits exact slots and private revision without local unequip");
		CHECK(controller.selectInventorySource(view,14) && controller.inventorySwap(view,equipment) && fixture.first==before,
			"native authority alone validates equipment placement, face compatibility and capacity");
	}
}

int main()
{
	TestInventoryOrdinarySwapsArePureAndSlotBounded();
	TestInventoryModalControlsAndBusyInspection();
	TestInventoryInspectionNeverBecomesASwapCursor();
	TestInventoryContextChangesRevokeRetainedChoices();
	TestInventoryReadinessLossRevokesSourceWithoutExternalSynchronization();
	TestInventoryOrdinaryEquipmentRemainsAuthorityValidated();
	TestSelectionUsesOnlyAssignedPresentActors();
	TestExactActorSelectionValidatesIdentityAndModalState();
	TestNumericMoveEntryProducesOneTypedRequest();
	TestDirectGridMoveIsBoundedPureAndAuthorityGated();
	TestRelativeMoveProducesEveryAdjacentPayload();
	TestRelativeMoveUsesRectangularColumnStride();
	TestRelativeMoveRejectsEdgesAndRowWrapping();
	TestRelativeMoveFailsClosedForInvalidState();
	TestTurnBasedMovementRequiresPositiveActionPoints();
	TestInvalidAndOutstandingInputsFailClosed();
	TestResynchronizationRetainsSelectionAndFreezesActions();
	TestDestinationAndAttackModesAreWorldEpochBound();
	TestReplicatedCommandGateLocksAndRecovers();
	TestAuthoritativeInterruptGatesActionsAndPassesSelectedMerc();
	TestRemainingTypedActions();
	TestExactAttackTargetSelectionFailsClosed();
	TestAttackTargetingUsesCanonicalHostility();
	TestMeleeTargetingOffersOnlyAdjacentHostiles();
	TestFirstAidOffersSelfAid();
	TestDoorSelectionProducesExactInverseIntent();
	TestExactDoorSelectionIsTransactionalAndEpochBound();
	TestDoorSelectionFailsClosedAcrossReplicaChanges();
	TestNormalTacticalUiCommandsTranslateWithoutLocalExecution();
	TestNormalTacticalUiTranslationFailsClosed();
	TestInventoryOrdinarySwapsArePureAndSlotBounded();
	TestOrdinaryArmorAndFaceSwapsKeepTransferAndDropBounded();
	TestInventoryDropRequiresExplicitWholePocketConfirmation();
	TestInventoryDropCancelsStaleContextAndPose();
	TestInventoryModalControlsAndBusyInspection();
	TestInventoryInspectionNeverBecomesASwapCursor();
	TestInventoryContextChangesRevokeRetainedChoices();
	TestInventoryTransferUsesBothPrivateRevisionsWithoutMutation();
	TestInventoryTransferAndDropFollowInterruptEligibility();
	TestInventoryTransferInvalidatesBothContexts();
	TestInventoryTransferRecipientsAreOwnedReadyHumans();
	TestGroundPickupSelectsExactAuthorityIdAndEmptyPocket();
	TestGroundPickupContextChangesInvalidateBeforeSubmit();
	TestInventoryReadinessLossRevokesSourceWithoutExternalSynchronization();
	TestInventoryOrdinaryEquipmentRemainsAuthorityValidated();
	TestGroundPickupBusyInspectionAndEmptyScope();
	if (Failures == 0)
		std::printf("full-engine co-op client controller tests passed\n");
	return Failures == 0 ? 0 : 1;
}
