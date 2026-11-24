#include <Ja2/FullEngineCoopClientPresentationInteraction.h>

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <array>
#include <cstdio>
#include <utility>
#include <variant>
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

using InteractionKind =
	FullEngineCoopClientPresentationInteractionKind;
using ClickKind = FullEngineCoopClientPresentationClickKind;

TacticalEntityId Id(std::uint16_t slot, std::uint32_t incarnation)
{
	return {slot, incarnation};
}

TacticalActorSnapshot Actor(TacticalEntityId id, std::uint8_t team,
	std::int32_t grid, TacticalStance stance = TacticalStance::Standing,
	bool hostile = false)
{
	TacticalActorSnapshot actor;
	actor.id = id;
	actor.team = team;
	actor.profile = id.slot;
	actor.grid = grid;
	actor.level = 0;
	actor.direction = 2;
	actor.animation = 1;
	actor.stance = stance;
	actor.actionPoints = 20;
	actor.life = actor.maximumLife = 80;
	actor.breath = actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	actor.hostileToPlayerTeam = hostile;
	actor.presentation.flags = TacticalActorRenderPosePresent;
	actor.presentation.animationDirection = actor.direction;
	actor.presentation.worldXQ8 =
		(grid % 20) * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	actor.presentation.worldYQ8 =
		(grid / 20) * TacticalWorldCellSize * TacticalWorldCoordinateScale;
	actor.presentation.animationSurface = 1;
	return actor;
}

TacticalWorldSnapshot Snapshot(std::vector<TacticalActorSnapshot> actors,
	std::vector<TacticalDoorSnapshot> doors = {},
	bool commandsBlocked = false)
{
	TacticalTurnSnapshot turn;
	turn.turnBased = true;
	turn.inCombat = true;
	turn.activeTeam = 0;
	turn.serial = 7;
	turn.commandsBlocked = commandsBlocked;
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(11,
		TacticalWorldDimensions{20, 20},
		TacticalSectorSnapshot{9, 2, 0, true}, turn,
		std::move(actors), std::move(doors), snapshot) ==
			TacticalSnapshotCreateError::None,
		"interaction fixture snapshot is canonical");
	return snapshot;
}

FullEngineCoopClientControllerView View(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assigned, std::size_t assignedCount,
	std::uint64_t outstanding = 0, bool resynchronizing = false)
{
	return {&snapshot, assigned, assignedCount, outstanding, resynchronizing};
}

FullEngineCoopClientPresentationClick ActorClick(TacticalEntityId actor)
{
	FullEngineCoopClientPresentationClick click;
	click.kind = ClickKind::RosterActor;
	click.actor = actor;
	return click;
}

FullEngineCoopClientPresentationClick GridClick(std::int32_t grid)
{
	FullEngineCoopClientPresentationClick click;
	click.kind = ClickKind::WorldGrid;
	click.grid = grid;
	return click;
}

FullEngineCoopClientPresentationClick RadarClick(std::int32_t grid)
{
	FullEngineCoopClientPresentationClick click;
	click.kind = ClickKind::RadarGrid;
	click.grid = grid;
	return click;
}

void TestRadarClicksNeverMutateControllerOrProduceIntent()
{
	const TacticalEntityId selected = Id(1, 101);
	const TacticalEntityId hostile = Id(10, 1001);
	const std::array<TacticalEntityId, 1> assigned{selected};
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(selected, 0, 21),
		Actor(hostile, 1, 30, TacticalStance::Standing, true)},
		{TacticalDoorSnapshot{22, 41, false}});
	const auto view = View(snapshot, assigned.data(), assigned.size());
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	auto result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, RadarClick(30));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.selectedActor() == selected,
		"a radar camera click cannot become movement or actor selection");

	CHECK(controller.beginAttackTargeting(view) &&
		controller.adjustAttackAim(2),
		"radar fixture enters aimed targeting");
	const TacticalEntityId retainedTarget = controller.attackTarget();
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, RadarClick(22));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.targetingAttack() &&
		controller.attackTarget() == retainedTarget &&
		controller.attackAimTime() == 3,
		"radar navigation preserves exact target identity and aim");

	controller.cancelAttackTargeting();
	CHECK(controller.beginDestinationEntry(view) &&
		controller.appendDestinationDigit(4),
		"radar fixture enters destination mode");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, RadarClick(22));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.enteringDestination() &&
		controller.destinationText()[0] == '4',
		"radar navigation preserves numeric destination input");

	controller.cancelDestinationEntry();
	CHECK(controller.beginDoorSelection(view),
		"radar fixture enters door mode");
	const std::int32_t retainedDoor = controller.selectedDoorBaseGrid();
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, RadarClick(30));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.selectingDoor() &&
		controller.selectedDoorBaseGrid() == retainedDoor,
		"radar navigation preserves exact door selection");
}

void TestRosterActorClicksUseExactIdentityAndNeverSubmit()
{
	const TacticalEntityId first = Id(1, 101);
	const TacticalEntityId second = Id(2, 202);
	const TacticalEntityId hostile = Id(10, 1001);
	const std::array<TacticalEntityId, 2> assigned{first, second};
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(first, 0, 21), Actor(second, 0, 25),
		Actor(hostile, 1, 30, TacticalStance::Standing, true)},
		{TacticalDoorSnapshot{22, 41, false}});
	const auto view = View(snapshot, assigned.data(), assigned.size());
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	CHECK(controller.beginDestinationEntry(view) &&
		controller.appendDestinationDigit(7),
		"roster fixture enters destination mode");
	auto result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(first));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent &&
		controller.selectedActor() == first &&
		controller.enteringDestination() &&
		controller.destinationText()[0] == '7',
		"reselecting the roster actor retains destination state without an intent");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(second));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent &&
		controller.selectedActor() == second &&
		!controller.enteringDestination(),
		"a different exact roster actor selects locally and cancels destination mode");

	CHECK(controller.beginAttackTargeting(view) &&
		controller.adjustAttackAim(2),
		"roster fixture enters aimed targeting");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(second));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent &&
		controller.targetingAttack() && controller.attackAimTime() == 3,
		"reselecting the roster actor retains target mode and aim");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(first));
	CHECK(result.kind == InteractionKind::ActorSelected &&
		!controller.targetingAttack(),
		"changing roster actor cancels target mode without firing");

	CHECK(controller.beginDoorSelection(view),
		"roster fixture enters door mode");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(first));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent &&
		controller.selectingDoor(),
		"reselecting the roster actor retains exact door mode");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(second));
	CHECK(result.kind == InteractionKind::ActorSelected &&
		!controller.selectingDoor(),
		"changing roster actor cancels door mode without opening it");

	const auto locked = View(
		snapshot, assigned.data(), assigned.size(), 77);
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, locked, ActorClick(first));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent &&
		controller.selectedActor() == first,
		"presentation-only roster selection remains available under the command lock");
	const TacticalEntityId retained = controller.selectedActor();
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(Id(first.slot, first.incarnation + 1)));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.selectedActor() == retained,
		"a stale roster incarnation is ignored transactionally");
}

void TestNormalGridClicksSelectBeforeMoving()
{
	const TacticalEntityId first = Id(1, 101);
	const TacticalEntityId second = Id(2, 202);
	const std::array<TacticalEntityId, 2> assigned{first, second};
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(first, 0, 21), Actor(second, 0, 22)});
	const auto view = View(snapshot, assigned.data(), assigned.size());
	FullEngineCoopClientController controller;
	controller.synchronize(view);

	auto result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(21));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent &&
		controller.selectedActor() == first,
		"the selected actor's grid reselects instead of generating movement");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(22));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent &&
		controller.selectedActor() == second,
		"another assigned actor's grid selects it without movement fallthrough");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, FullEngineCoopClientPresentationClick{});
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.selectedActor() == second,
		"a non-click semantic value is ignored transactionally");
}

void TestEmptyGridProducesStanceMovementModesWithoutMutation()
{
	struct Case
	{
		TacticalStance stance;
		std::uint16_t movementMode;
	};
	const Case cases[]{
		{TacticalStance::Standing, 0},
		{TacticalStance::Crouched, 5},
		{TacticalStance::Prone, 8}};
	for (const Case expected : cases)
	{
		const TacticalEntityId selected = Id(1, 101);
		const std::array<TacticalEntityId, 1> assigned{selected};
		const TacticalWorldSnapshot snapshot = Snapshot({
			Actor(selected, 0, 21, expected.stance)});
		const auto view = View(snapshot, assigned.data(), assigned.size());
		FullEngineCoopClientController controller;
		controller.synchronize(view);
		const auto result = DispatchFullEngineCoopClientPresentationClick(
			controller, view, GridClick(42));
		const auto* const move =
			std::get_if<CoopSession::MoveTacticalIntent>(&result.intent.payload);
		CHECK(result.kind == InteractionKind::IntentReady && result.intent &&
			result.intent.actor == selected && move != nullptr &&
			move->destinationGrid == 42 &&
			move->movementMode == expected.movementMode && !move->reverse &&
			snapshot.find(selected)->grid == 21,
			"an empty grid returns one stance-correct move intent without prediction");
	}

	const TacticalEntityId selected = Id(1, 101);
	const std::array<TacticalEntityId, 1> assigned{selected};
	const TacticalWorldSnapshot unknown = Snapshot({
		Actor(selected, 0, 21, TacticalStance::Unknown)});
	const auto unknownView = View(unknown, assigned.data(), assigned.size());
	FullEngineCoopClientController unknownController;
	unknownController.synchronize(unknownView);
	const auto unknownResult = DispatchFullEngineCoopClientPresentationClick(
		unknownController, unknownView, GridClick(42));
	CHECK(unknownResult.kind == InteractionKind::Ignored &&
		!unknownResult.intent,
		"unknown replicated stance cannot guess a JA2 movement mode");
}

void TestTargetModeSelectsOnlyHostilesAndNeverFallsThrough()
{
	const TacticalEntityId selected = Id(1, 101);
	const TacticalEntityId friendly = Id(2, 202);
	const TacticalEntityId firstHostile = Id(10, 1001);
	const TacticalEntityId clickedHostile = Id(11, 1101);
	const std::array<TacticalEntityId, 2> assigned{selected, friendly};
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(selected, 0, 21), Actor(friendly, 0, 22),
		Actor(firstHostile, 1, 30, TacticalStance::Standing, true),
		Actor(clickedHostile, 1, 31, TacticalStance::Standing, true)});
	const auto view = View(snapshot, assigned.data(), assigned.size());
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.beginAttackTargeting(view) &&
		controller.attackTarget() == firstHostile &&
		controller.adjustAttackAim(2),
		"target interaction fixture begins with aim state");

	auto result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(31));
	CHECK(result.kind == InteractionKind::TargetSelected && !result.intent &&
		controller.attackTarget() == clickedHostile &&
		controller.attackAimTime() == 3,
		"a hostile map click selects the exact target without firing");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(22));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.selectedActor() == selected &&
		controller.attackTarget() == clickedHostile &&
		controller.attackAimTime() == 3,
		"a friendly actor click in target mode cannot select or move by fallthrough");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(42));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.attackTarget() == clickedHostile,
		"an empty target-mode grid cannot become a move intent");
}

void TestDestinationAndDoorModesConsumeWorldClicks()
{
	const TacticalEntityId selected = Id(1, 101);
	const TacticalEntityId other = Id(2, 202);
	const std::array<TacticalEntityId, 2> assigned{selected, other};
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(selected, 0, 21), Actor(other, 0, 25)}, {
		TacticalDoorSnapshot{20, 40, false},
		TacticalDoorSnapshot{22, 42, true}});
	const auto view = View(snapshot, assigned.data(), assigned.size());
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.beginDestinationEntry(view) &&
		controller.appendDestinationDigit(9),
		"destination interaction fixture enters numeric mode");
	controller.toggleReverse();
	auto result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(25));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.selectedActor() == selected &&
		controller.enteringDestination() && controller.reverse() &&
		controller.destinationText()[0] == '9',
		"destination mode consumes an assigned-actor world click without fallthrough");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(42));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.enteringDestination(),
		"destination mode consumes an empty world click without movement");
	controller.cancelDestinationEntry();

	CHECK(controller.beginDoorSelection(view) &&
		controller.selectedDoorBaseGrid() == 20,
		"door interaction fixture selects the canonical adjacent door");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(22));
	CHECK(result.kind == InteractionKind::DoorSelected && !result.intent &&
		controller.selectingDoor() &&
		controller.selectedDoorBaseGrid() == 22 &&
		controller.selectedDoorStructureId() == 42,
		"door mode selects the exact clicked published door without submitting");
	result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(25));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
		controller.selectingDoor() && controller.selectedActor() == selected &&
		controller.selectedDoorBaseGrid() == 22,
		"a non-door click in door mode cannot select an actor or move by fallthrough");
}

void TestEquippedMedicalKitUsesExactAdjacentPatient()
{
	const TacticalEntityId medicId = Id(1, 101);
	const TacticalEntityId patientId = Id(2, 202);
	const std::array<TacticalEntityId, 2> assigned{medicId, patientId};
	auto medic = Actor(medicId, 0, 21);
	medic.loadout.primaryHand = {99, 1, 80};
	medic.handMedicalKitPoints = 80;
	auto patient = Actor(patientId, 0, 22);
	patient.bleeding = 12;
	const auto snapshot = Snapshot({medic, patient});
	const auto view = View(snapshot, assigned.data(), assigned.size());
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	const auto result = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(patient.grid));
	const auto* aid = std::get_if<CoopSession::BeginFirstAidTacticalIntent>(
		&result.intent.payload);
	CHECK(result.kind == InteractionKind::IntentReady && result.intent && aid &&
		aid->target == patientId && result.intent.actor == medicId &&
		controller.selectedActor() == medicId && snapshot.find(patientId)->bleeding == 12,
		"held-kit world click requests exact patient without selection or local treatment");
	const auto otherPeerView = View(snapshot, assigned.data(), 1);
	CHECK(controller.beginFirstAid(otherPeerView, patientId),
		"a medic may request treatment of another peer's adjacent teammate");
	CHECK(!controller.beginFirstAid(view, medicId) &&
		!controller.beginFirstAid(view, Id(patientId.slot, patientId.incarnation + 1)),
		"self treatment and stale patient identities are not silently retargeted");
	const auto selection = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(patientId));
	CHECK(selection.kind == InteractionKind::ActorSelected && !selection.intent &&
		controller.selectedActor() == patientId,
		"the wounded actor's roster portrait always remains a selection action");

	auto rejects = [&](TacticalActorSnapshot source, TacticalActorSnapshot target,
		const char* message) {
		for (auto* actor : {&source, &target})
		{
			actor->presentation.worldXQ8 = (actor->grid % 20) *
				TacticalWorldCellSize * TacticalWorldCoordinateScale;
			actor->presentation.worldYQ8 = (actor->grid / 20) *
				TacticalWorldCellSize * TacticalWorldCoordinateScale;
		}
		const auto rejectedSnapshot = Snapshot({source, target});
		const auto rejectedView = View(rejectedSnapshot, assigned.data(), 1);
		FullEngineCoopClientController rejectedController;
		rejectedController.synchronize(rejectedView);
		CHECK(!rejectedController.beginFirstAid(rejectedView, patientId), message);
	};
	auto changed = patient;
	changed.bleeding = 0;
	rejects(medic, changed, "a stable non-bleeding patient does not need first aid");
	changed.life = 10;
	const auto critical = Snapshot({medic, changed});
	const auto criticalView = View(critical, assigned.data(), 1);
	controller.synchronize(criticalView);
	CHECK(controller.beginFirstAid(criticalView, patientId),
		"a living critically wounded teammate may need stabilizing without bleeding");
	changed = patient; changed.life = 0;
	rejects(medic, changed, "first aid does not resurrect a dead patient");
	changed = patient; changed.level = 1;
	rejects(medic, changed, "first aid requires the same elevation");
	changed = patient; changed.team = 1;
	rejects(medic, changed, "first aid does not cross team ownership");
	changed = patient; changed.hostileToPlayerTeam = true;
	rejects(medic, changed, "a hostile actor cannot be a teammate patient");
	changed = patient; changed.grid = 25;
	rejects(medic, changed, "distant patients do not create an implicit approach route");
	changed = patient; changed.grid = 42;
	rejects(medic, changed, "diagonal neighbors are not native first-aid action tiles");
	changed = medic; changed.grid = 19;
	auto wrapped = patient; wrapped.grid = 20;
	rejects(changed, wrapped, "linear grid neighbors across row edges are not adjacent");
	changed = medic; changed.handMedicalKitPoints = 0;
	rejects(changed, patient, "an exhausted or absent held kit cannot start aid");
	changed = medic; changed.life = 10;
	rejects(changed, patient, "a critically wounded medic cannot begin aid");
	changed = medic; changed.presentation.bodyType = 4;
	rejects(changed, patient, "a body without stock first-aid animations cannot begin aid");
	changed = medic; changed.medicalService = {true, patientId, 0};
	rejects(changed, patient, "a providing medic cannot queue a redundant aid start");
	changed = medic; changed.medicalService.receivingCount = 1;
	rejects(changed, patient, "a receiving medic cannot begin a conflicting service");
	changed = patient; changed.medicalService.providing = true;
	rejects(medic, changed, "a providing patient is not advertised as a new aid target");
	changed = patient; changed.medicalService.receivingCount = 1;
	rejects(medic, changed, "a patient already receiving aid cannot queue a conflicting service");
	changed = medic; changed.medicalService = {true, patientId, 0};
	const auto treating = Snapshot({changed, patient});
	const auto treatingView = View(treating, assigned.data(), assigned.size());
	controller.synchronize(treatingView);
	const auto duplicate = DispatchFullEngineCoopClientPresentationClick(
		controller, treatingView, GridClick(patient.grid));
	CHECK(duplicate.kind == InteractionKind::Ignored && !duplicate.intent &&
		controller.selectedActor() == medicId && controller.stop(treatingView),
		"reclicking the current patient neither moves nor cancels aid; explicit Stop remains available");
	const auto selectReceiving = DispatchFullEngineCoopClientPresentationClick(
		controller, treatingView, ActorClick(patientId));
	CHECK(selectReceiving.kind == InteractionKind::ActorSelected && !selectReceiving.intent,
		"roster selection remains available while an actor is treating");
	CHECK(controller.selectActor(view, medicId) && controller.beginFirstAid(view, patientId),
		"a later cleared service snapshot permits a new aid request without stale UI locks");

	controller.synchronize(view);
	CHECK(!controller.beginFirstAid(View(snapshot, assigned.data(), 1, 88), patientId) &&
		!controller.beginFirstAid(View(snapshot, assigned.data(), 1, 0, true), patientId),
		"outstanding-command and resynchronization locks also cover treatment");
	const auto blocked = Snapshot({medic, patient}, {}, true);
	CHECK(!controller.beginFirstAid(View(blocked, assigned.data(), 1), patientId),
		"the authoritative combat busy gate also covers treatment");
	CHECK(controller.beginDestinationEntry(view) &&
		!controller.beginFirstAid(view, patientId),
		"a medical click cannot bypass an active destination editor");
	controller.cancelDestinationEntry();
	auto secondPatient = patient; secondPatient.id = Id(3, 303);
	const auto ambiguous = Snapshot({medic, patient, secondPatient});
	const auto ambiguousResult = DispatchFullEngineCoopClientPresentationClick(
		controller, View(ambiguous, assigned.data(), 1), GridClick(patient.grid));
	CHECK(ambiguousResult.kind == InteractionKind::Ignored && !ambiguousResult.intent,
		"multiple eligible patients on one tile never pick an arbitrary identity");
}

void TestOutstandingLockBlocksMovesButNotRosterSelection()
{
	const TacticalEntityId first = Id(1, 101);
	const TacticalEntityId second = Id(2, 202);
	const std::array<TacticalEntityId, 2> assigned{first, second};
	const TacticalWorldSnapshot snapshot = Snapshot({
		Actor(first, 0, 21), Actor(second, 0, 22)});
	auto view = View(snapshot, assigned.data(), assigned.size());
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	const auto firstMove = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(42));
	CHECK(firstMove.kind == InteractionKind::IntentReady && firstMove.intent,
		"unlocked world click produces the first move request");

	// Mirrors the screen's same-frame causal lock after a successful send.
	view.outstandingCommandId = 77;
	const auto secondMove = DispatchFullEngineCoopClientPresentationClick(
		controller, view, GridClick(43));
	CHECK(secondMove.kind == InteractionKind::Ignored && !secondMove.intent,
		"a second queued map click cannot bypass the outstanding-command lock");
	const auto selection = DispatchFullEngineCoopClientPresentationClick(
		controller, view, ActorClick(second));
	CHECK(selection.kind == InteractionKind::ActorSelected &&
		!selection.intent && controller.selectedActor() == second,
		"the command lock blocks gameplay but not presentation-only roster selection");
}

void TestInventoryAndTransferConsumeAllWorldAndRosterClicks()
{
	const std::array<TacticalEntityId,2> assigned{Id(1,101),Id(2,202)};
	const auto snapshot = Snapshot({Actor(assigned[0],0,21),Actor(assigned[1],0,22)});
	CoopSession::CoopOwnerInventorySnapshot first;
	first.actor = assigned[0];
	first.owner[0] = 1;
	first.sessionEpoch = 1;
	first.baselineId = 1;
	first.worldGeneration = 11;
	first.inventoryRevision = 1;
	first.usesNewInventory = true;
	for (std::uint16_t i = 0; i < 55; ++i)
		first.slots.push_back({i,0,0,0,CoopSession::CoopInventorySlotSupport::Empty});
	first.slots[14] = {14,45,1,100,CoopSession::CoopInventorySlotSupport::OrdinarySwappable};
	first.groundGrid = 21;
	first.groundLevel = 0;
	first.groundItems = {{{70000,8},{0,45,1,100,CoopSession::CoopInventorySlotSupport::OrdinarySwappable,
		CoopSession::CoopInventoryStatusKind::Condition,0}}};
	auto second = first;
	second.actor = assigned[1];
	const std::array<const CoopSession::CoopOwnerInventorySnapshot*,2> owners{&first,&second};
	auto view = View(snapshot,assigned.data(),assigned.size());
	view.ownerInventories = owners.data();
	FullEngineCoopClientController controller;
	controller.synchronize(view);
	CHECK(controller.openInventory(view),"inventory modal opens before raw semantic clicks");
	for (const bool transfer : {false,true})
	{
		if (transfer) CHECK(controller.selectInventorySource(view,14) && controller.beginInventoryTransfer(view),
			"owned recipient selection is active before missed-slot clicks");
		for (const auto click : {GridClick(42),GridClick(22),ActorClick(assigned[1]),RadarClick(42)})
		{
			const auto result = DispatchFullEngineCoopClientPresentationClick(controller,view,click);
			CHECK(result.kind == InteractionKind::Ignored && !result.intent &&
				controller.selectedActor() == assigned[0] && controller.inventoryOpen() &&
				controller.transferringInventory() == transfer,
				"world, occupied tile, roster and radar clicks cannot escape inventory modal or emit accidental movement");
		}
	}
	controller.cancelInventoryTransfer();
	CHECK(controller.beginInventoryPickup(view),"exact ground selection becomes the active inventory modal");
	for (const auto click : {GridClick(42),GridClick(22),ActorClick(assigned[1]),RadarClick(42)})
	{
		const auto result = DispatchFullEngineCoopClientPresentationClick(controller,view,click);
		CHECK(result.kind == InteractionKind::Ignored && !result.intent && controller.pickingUpInventory() &&
			controller.selectedActor() == assigned[0] && controller.inventoryGroundItem() == (TacticalWorldItemId{70000,8}),
			"ground pickup mode consumes terrain/roster/radar clicks without moving, switching mercs or dropping items");
	}
	controller.cancelInventoryPickup();
	CHECK(controller.selectInventorySource(view,14) && controller.beginInventoryDrop(view),
		"explicit drop confirmation becomes the active inventory modal");
	for (const auto click : {GridClick(42),GridClick(22),ActorClick(assigned[1]),RadarClick(42)})
	{
		const auto result = DispatchFullEngineCoopClientPresentationClick(controller,view,click);
		CHECK(result.kind == InteractionKind::Ignored && !result.intent && controller.droppingInventory() &&
			controller.selectedActor() == assigned[0] && controller.inventorySourceSlot() == 14,
			"drop confirmation consumes terrain/roster/radar clicks without confirming or dispatching movement");
	}
}

void TestLootClicksMoveOrOpenPickupWithoutChangingPriority()
{
	const std::array<TacticalEntityId,2> assigned{Id(1,101),Id(2,202)};
	auto selected = Actor(assigned[0],0,21);
	auto other = Actor(assigned[1],0,22);
	const auto hostile = Actor(Id(10,1001),1,30,TacticalStance::Standing,true);
	const auto snapshot = Snapshot({selected,other,hostile},{{22,42,false}});
	CoopSession::CoopOwnerInventorySnapshot owner;
	owner.actor = assigned[0]; owner.owner[0] = 1;
	owner.sessionEpoch = 1; owner.baselineId = 2; owner.worldGeneration = 11;
	owner.inventoryRevision = 3; owner.groundGrid = 21; owner.groundLevel = 0;
	owner.usesNewInventory = true;
	for (std::uint16_t i = 0; i < 55; ++i)
		owner.slots.push_back({i,0,0,0,CoopSession::CoopInventorySlotSupport::Empty});
	owner.nearbyLoot = {{21,0,false},{22,0,true},{43,0,true}};
	owner.groundItems = {{{70000,8},{0,45,1,100,CoopSession::CoopInventorySlotSupport::OrdinarySwappable,
		CoopSession::CoopInventoryStatusKind::Condition,0}}};
	const std::array<const CoopSession::CoopOwnerInventorySnapshot*,2> owners{&owner,nullptr};
	auto view = View(snapshot,assigned.data(),assigned.size()); view.ownerInventories = owners.data();
	const auto lootClick = [&](std::int32_t grid) {
		auto click = GridClick(44); // A raised marker need not agree with the raw terrain inverse.
		click.hasLoot = true;
		click.loot.context = {assigned[0],owner.owner,owner.sessionEpoch,owner.worldGeneration,
			owner.baselineId,owner.inventoryRevision,owner.groundGrid,owner.groundLevel};
		click.loot.context.assignmentCount = assigned.size();
		click.loot.context.assignments[0] = assigned[0]; click.loot.context.assignments[1] = assigned[1];
		click.loot.grid = grid; click.loot.level = 0; click.loot.hasMedicalKit = grid != 21;
		return click;
	};
	FullEngineCoopClientController controller; controller.synchronize(view);
	auto result = DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(43));
	const auto* move = std::get_if<CoopSession::MoveTacticalIntent>(&result.intent.payload);
	CHECK(result.kind == InteractionKind::IntentReady && result.intent && move && move->destinationGrid == 43 &&
		!controller.inventoryOpen() && owner.groundItems.size() == 1 && snapshot.find(assigned[0])->grid == 21,
		"nearby marker emits only the existing exact-grid move, with no pickup or prediction");
	view.outstandingCommandId = 99;
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(43));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent,"queued marker click cannot bypass outstanding lock");
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(21));
	CHECK(result.kind == InteractionKind::InventoryOpened && !result.intent && controller.pickingUpInventory() &&
		controller.inventoryGroundItem() == (TacticalWorldItemId{70000,8}),
		"current selected tile opens read-only I/P even while busy, without issuing any command");
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(43));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent && controller.pickingUpInventory(),
		"inventory modal consumes marker clicks without moving or auto-picking up");
	controller.cancelInventoryPickup();
	CHECK(controller.inventoryOpen() && !controller.pickingUpInventory() && owner.groundItems.size() == 1,
		"pickup cancellation does not remove ground items");
	controller.closeInventory(); view.outstandingCommandId = 0;
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(22));
	CHECK(result.kind == InteractionKind::ActorSelected && !result.intent && controller.selectedActor() == assigned[1],
		"other owned actor keeps priority over a loot marker on its tile");
	controller.selectActor(view,assigned[0]);
	CHECK(controller.beginAttackTargeting(view),"loot fixture enters fire targeting");
	auto fireClick = lootClick(43); fireClick.grid = 30;
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,fireClick);
	CHECK(result.kind == InteractionKind::TargetSelected && !result.intent && controller.attackTarget() == hostile.id,
		"fire mode uses original tactical target instead of marker movement");
	controller.cancelAttackTargeting();
	CHECK(controller.beginDestinationEntry(view),"loot fixture enters destination mode");
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(43));
	CHECK(result.kind == InteractionKind::Ignored && !result.intent && controller.enteringDestination(),
		"numeric destination modal retains priority");
	controller.cancelDestinationEntry();
	CHECK(controller.beginDoorSelection(view),"loot fixture enters door mode");
	auto doorClick = lootClick(43); doorClick.grid = 22;
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,doorClick);
	CHECK(result.kind == InteractionKind::DoorSelected && !result.intent && controller.selectedDoorBaseGrid() == 22,
		"door mode preserves underlying published door selection");
	controller.cancelDoorSelection();
	const auto oldClick = lootClick(43);
	++owner.inventoryRevision;
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,oldClick);
	CHECK(result.kind == InteractionKind::Ignored && !result.intent && !controller.inventoryOpen(),
		"stale rendered private revision is consumed, never reinterpreted as terrain movement");
	--owner.inventoryRevision;
	view.resynchronizing = true;
	CHECK(!DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(43)).intent,
		"resynchronizing marker clicks cannot emit movement");
	view.resynchronizing = false;
	selected.loadout.primaryHand = {99,1,80}; selected.handMedicalKitPoints = 80;
	other.bleeding = 12;
	const auto medicalSnapshot = Snapshot({selected,other});
	view.snapshot = &medicalSnapshot;
	result = DispatchFullEngineCoopClientPresentationClick(controller,view,lootClick(22));
	const auto* aid = std::get_if<CoopSession::BeginFirstAidTacticalIntent>(&result.intent.payload);
	CHECK(result.kind == InteractionKind::IntentReady && aid && aid->target == other.id &&
		!controller.inventoryOpen(),"adjacent wounded teammate with held kit retains aid priority over loot");
	view.snapshot = &snapshot;
	auto wrongOwner = lootClick(43); ++wrongOwner.loot.context.owner[0];
	CHECK(!DispatchFullEngineCoopClientPresentationClick(controller,view,wrongOwner).intent,
		"marker from a different private owner is rejected");
	auto reassigned = view; reassigned.assignedActorCount = 1;
	CHECK(!DispatchFullEngineCoopClientPresentationClick(controller,reassigned,lootClick(43)).intent,
		"assignment-set changes cancel old marker activation");
}
}

int main()
{
	TestRadarClicksNeverMutateControllerOrProduceIntent();
	TestRosterActorClicksUseExactIdentityAndNeverSubmit();
	TestNormalGridClicksSelectBeforeMoving();
	TestEmptyGridProducesStanceMovementModesWithoutMutation();
	TestTargetModeSelectsOnlyHostilesAndNeverFallsThrough();
	TestDestinationAndDoorModesConsumeWorldClicks();
	TestEquippedMedicalKitUsesExactAdjacentPatient();
	TestOutstandingLockBlocksMovesButNotRosterSelection();
	TestInventoryAndTransferConsumeAllWorldAndRosterClicks();
	TestLootClicksMoveOrOpenPickupWithoutChangingPriority();
	if (Failures != 0)
	{
		std::printf("%d full-engine co-op presentation interaction test(s) failed\n",
			Failures);
		return 1;
	}
	std::printf("full-engine co-op presentation interaction tests passed\n");
	return 0;
}
