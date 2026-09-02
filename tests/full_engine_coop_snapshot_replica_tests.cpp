#include <Multiplayer/FullEngineCoopSnapshotReplica.h>

#include <cstdio>
#include <cstdint>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

TacticalActorSnapshot Actor(std::uint16_t slot, std::int32_t grid,
	std::uint32_t incarnation = 1, bool active = true,
	bool inSector = true)
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{slot, incarnation};
	actor.team = 0;
	actor.profile = slot;
	actor.grid = grid;
	actor.level = 0;
	actor.direction = 2;
	actor.animation = 10;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 20;
	actor.life = actor.maximumLife = 80;
	actor.breath = actor.maximumBreath = 90;
	actor.active = active;
	actor.inSector = inSector;
	return actor;
}

TacticalActorLoadoutSnapshot CombatLoadout(std::uint16_t rounds = 7,
	bool chambered = true, std::int16_t ammunitionCondition = 100)
{
	TacticalActorLoadoutSnapshot loadout;
	loadout.helmet = TacticalHandItemSnapshot{
		40, 1, 96, 0, 0, 0, false, false};
	loadout.vest = TacticalHandItemSnapshot{
		41, 1, 87, 0, 0, 0, false, false};
	loadout.legs = TacticalHandItemSnapshot{
		42, 1, 78, 0, 0, 0, false, false};
	loadout.primaryHand = TacticalHandItemSnapshot{
		10, 1, 85, 20, rounds, ammunitionCondition, true, chambered};
	loadout.secondaryHand = TacticalHandItemSnapshot{
		30, 1, 72, 0, 0, 0, false, false};
	return loadout;
}

bool InstallSnapshot(CoopTacticalBaseline& baseline,
	std::vector<TacticalActorSnapshot> actors,
	TacticalSectorSnapshot sector = TacticalSectorSnapshot{3, 4, 0, true},
	std::vector<TacticalDoorSnapshot> doors = {})
{
	return TacticalWorldSnapshot::create(
		baseline.state.worldGeneration,
		TacticalWorldDimensions{160, 160}, sector,
		TacticalTurnSnapshot{true, true, 0, baseline.state.turnSerial},
		std::move(actors), std::move(doors), baseline.snapshot, 1024,
		MaximumCoopTacticalSnapshotDoors) ==
		TacticalSnapshotCreateError::None;
}

CoopTacticalBaseline Baseline(std::uint64_t generation = 11,
	std::uint64_t revision = 20, std::uint64_t turnSerial = 30)
{
	CoopTacticalBaseline baseline;
	baseline.state.sessionEpoch = 7;
	baseline.state.worldGeneration = generation;
	baseline.state.revision = revision;
	baseline.state.turnSerial = turnSerial;
	baseline.baselineId = 1;
	baseline.nextExpectedCommandId = 1;
	// Actor 3 is deliberately absent from the tactical-present projection.
	// Version 1 deltas publish it only if it later enters the sector.
	TacticalActorSnapshot first = Actor(1, 100);
	first.loadout = CombatLoadout();
	CHECK(InstallSnapshot(baseline, {first, Actor(2, 200),
		Actor(3, -1, 1, true, false)}),
		"baseline fixture creates");
	return baseline;
}

CoopTacticalDelta DeltaFrom(const CoopTacticalBaseline& baseline,
	std::uint64_t resultingRevision = 21,
	std::uint64_t resultingTurnSerial = 30)
{
	CoopTacticalDelta delta;
	delta.state = baseline.state;
	delta.state.revision = resultingRevision;
	delta.state.turnSerial = resultingTurnSerial;
	delta.deltaId = 1;
	delta.baseRevision = baseline.state.revision;
	delta.delta.previousEpoch = baseline.state.worldGeneration;
	delta.delta.currentEpoch = baseline.state.worldGeneration;
	return delta;
}

void CheckOriginalView(const FullEngineCoopSnapshotReplica& replica,
	const char* message)
{
	const TacticalActorSnapshot* first =
		replica.snapshot().find(TacticalEntityId{1, 1});
	CHECK(replica.hasSnapshot() && replica.state().sessionEpoch == 7 &&
		replica.state().worldGeneration == 11 &&
		replica.state().revision == 20 &&
		replica.state().turnSerial == 30 &&
		replica.snapshot().dimensions().columns == 160 &&
		replica.snapshot().dimensions().rows == 160 &&
		replica.snapshot().actors().size() == 2 && first != nullptr &&
		first->grid == 100 && first->loadout == CombatLoadout() &&
		replica.snapshot().find(TacticalEntityId{3, 1}) == nullptr,
		message);
}

void TestPresentProjectionAndFullDelta()
{
	FullEngineCoopSnapshotReplica replica;
	CoopTacticalBaseline baseline = Baseline();
	baseline.assignedActors = {
		TacticalEntityId{1, 1}, TacticalEntityId{2, 1}};
	CHECK(replica.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"valid baseline commits");
	CheckOriginalView(replica,
		"baseline retains only the delta-covered present projection");

	CoopTacticalDelta delta = DeltaFrom(baseline, 21, 31);
	delta.delta.events.push_back(TacticalSectorChangedEvent{
		baseline.snapshot.sector(), TacticalSectorSnapshot{4, 4, 0, true}});
	TacticalTurnSnapshot interruptTurn{true, true, 0, 31};
	interruptTurn.interruptPhase = TacticalInterruptPhase::Active;
	interruptTurn.interruptSerial = 9;
	delta.delta.events.push_back(TacticalTurnChangedEvent{
		baseline.snapshot.turn(), interruptTurn});
	delta.delta.events.push_back(
		TacticalActorEnteredEvent{Actor(3, 300)});
	delta.delta.events.push_back(
		TacticalActorLeftEvent{TacticalEntityId{2, 1}});
	delta.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{1, 1}, 100, 105, 0, 0, 2, 3});
	delta.delta.events.push_back(TacticalActorStanceChangedEvent{
		TacticalEntityId{1, 1}, TacticalStance::Standing,
		TacticalStance::Crouched, 10, 11});
	delta.delta.events.push_back(TacticalActorVitalsChangedEvent{
		TacticalEntityId{1, 1}, 20, 15, 80, 70, 80, 80, 90, 75, 90, 90,
		false, false, false, true});
	TacticalActorLoadoutSnapshot changedEquipment =
		CombatLoadout(6, false, -31);
	changedEquipment.vest.condition = 79;
	delta.delta.events.push_back(TacticalActorLoadoutChangedEvent{
		TacticalEntityId{1, 1}, CombatLoadout(), changedEquipment});
	CHECK(replica.applyDelta(delta) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"the complete canonical delta vocabulary commits");
	const TacticalActorSnapshot* first =
		replica.snapshot().find(TacticalEntityId{1, 1});
	const TacticalActorSnapshot* entered =
		replica.snapshot().find(TacticalEntityId{3, 1});
	CHECK(replica.state().revision == 21 &&
		replica.state().turnSerial == 31 &&
		replica.snapshot().turn().interruptPhase ==
			TacticalInterruptPhase::Active &&
		replica.snapshot().turn().interruptSerial == 9 &&
		first != nullptr && first->interruptActionEligible &&
		replica.snapshot().sector().x == 4 &&
		replica.snapshot().turn().activeTeam == 0 &&
		replica.snapshot().actors().size() == 2 && first != nullptr &&
		first->grid == 105 && first->direction == 3 &&
		first->stance == TacticalStance::Crouched &&
		first->animation == 11 && first->life == 70 &&
		first->actionPoints == 15 &&
		first->loadout == changedEquipment &&
		first->loadout.vest.condition == 79 &&
		entered != nullptr &&
		replica.snapshot().find(TacticalEntityId{2, 1}) == nullptr,
		"all covered values become visible at one committed boundary");
}

void TestCommandBusyOnlyDeltaIsTransactional()
{
	FullEngineCoopSnapshotReplica replica;
	const CoopTacticalBaseline baseline = Baseline();
	CHECK(replica.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"command-busy replica fixture baseline commits");

	TacticalTurnSnapshot blocked = baseline.snapshot.turn();
	blocked.commandsBlocked = true;
	CoopTacticalDelta lock = DeltaFrom(baseline);
	lock.delta.events.push_back(TacticalTurnChangedEvent{
		baseline.snapshot.turn(), blocked});
	CHECK(replica.applyDelta(lock) ==
			FullEngineCoopReplicaApplyResult::Committed &&
		replica.state().revision == 21 &&
		replica.state().turnSerial == baseline.state.turnSerial &&
		replica.snapshot().turn().commandsBlocked,
		"busy-only delta applies without advancing the replicated turn serial");

	CoopTacticalDelta stalePrevious;
	stalePrevious.state = replica.state();
	stalePrevious.state.revision = 22;
	stalePrevious.deltaId = 2;
	stalePrevious.baseRevision = replica.state().revision;
	stalePrevious.delta.previousEpoch = baseline.state.worldGeneration;
	stalePrevious.delta.currentEpoch = baseline.state.worldGeneration;
	stalePrevious.delta.events.push_back(TacticalTurnChangedEvent{
		baseline.snapshot.turn(), blocked});
	CHECK(replica.applyDelta(stalePrevious) ==
			FullEngineCoopReplicaApplyResult::Rejected &&
		replica.state().revision == 21 &&
		replica.snapshot().turn().commandsBlocked,
		"stale command-busy previous value rejects without rolling back replica state");

	CoopTacticalDelta unlock = std::move(stalePrevious);
	unlock.delta.events.clear();
	unlock.delta.events.push_back(TacticalTurnChangedEvent{
		blocked, baseline.snapshot.turn()});
	CHECK(replica.applyDelta(unlock) ==
			FullEngineCoopReplicaApplyResult::Committed &&
		replica.state().revision == 22 &&
		replica.state().turnSerial == baseline.state.turnSerial &&
		!replica.snapshot().turn().commandsBlocked,
		"exact busy-gate recovery commits at the same turn serial");
}

void TestGenerationRequiresFreshBaseline()
{
	FullEngineCoopSnapshotReplica replica;
	CoopTacticalBaseline baseline = Baseline();
	CHECK(replica.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"generation fixture baseline commits");

	CoopTacticalDelta reset = DeltaFrom(baseline);
	reset.state.worldGeneration = 12;
	reset.delta.currentEpoch = 12;
	reset.delta.events.push_back(TacticalWorldResetEvent{11, 12});
	CHECK(replica.applyDelta(reset) ==
		FullEngineCoopReplicaApplyResult::Rejected,
		"a reset delta cannot invent a new generation");
	CheckOriginalView(replica,
		"rejected generation reset preserves the committed generation");

	CoopTacticalBaseline fresh = Baseline(12, 1, 1);
	fresh.baselineId = 2;
	CHECK(replica.applyBaseline(fresh) ==
		FullEngineCoopReplicaApplyResult::Committed &&
		replica.state().worldGeneration == 12 &&
		replica.state().revision == 1 && replica.snapshot().epoch() == 12,
		"a fresh baseline may replace the generation transactionally");

	CoopTacticalDelta stale = DeltaFrom(baseline);
	CHECK(replica.applyDelta(stale) ==
		FullEngineCoopReplicaApplyResult::Rejected &&
		replica.state().worldGeneration == 12,
		"an old-generation delta cannot roll back a fresh baseline");

	replica.clear();
	CHECK(!replica.hasSnapshot() && replica.snapshot().epoch() == 0 &&
		replica.snapshot().actors().empty() &&
		replica.snapshot().doors().empty() &&
		replica.state().sessionEpoch == 0,
		"clear removes every observable replica value");
}

void TestBaselineValidationPreservesState()
{
	FullEngineCoopSnapshotReplica replica;
	const CoopTacticalBaseline baseline = Baseline();
	CHECK(replica.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"baseline-validation fixture commits");
	auto rejects = [&](CoopTacticalBaseline candidate, const char* message) {
		CHECK(replica.applyBaseline(candidate) ==
			FullEngineCoopReplicaApplyResult::Rejected, message);
		CheckOriginalView(replica,
			"rejected baseline preserves the prior committed snapshot");
	};

	CoopTacticalBaseline bad = baseline;
	bad.state.turnSerial = 99;
	rejects(std::move(bad), "state/snapshot turn mismatch is rejected");

	bad = baseline;
	bad.baselineId = 0;
	rejects(std::move(bad), "zero baseline identity is rejected");

	bad = baseline;
	CHECK(InstallSnapshot(bad,
		{Actor(1, 100), Actor(2, 200)},
		TacticalSectorSnapshot{3, 4, 0, false}),
		"unloaded baseline fixture creates");
	rejects(std::move(bad), "an unloaded tactical baseline is rejected");

	bad = baseline;
	std::vector<TacticalActorSnapshot> excessive;
	for (std::size_t index = 0;
		index <= MaximumCoopTacticalSnapshotActors; ++index)
		excessive.push_back(Actor(static_cast<std::uint16_t>(index),
			static_cast<std::int32_t>(index)));
	CHECK(InstallSnapshot(bad, std::move(excessive)),
		"oversized baseline fixture creates");
	rejects(std::move(bad), "baseline actor capacity is enforced locally");

	bad = baseline;
	TacticalActorSnapshot invalidStance = Actor(1, 100);
	invalidStance.stance = static_cast<TacticalStance>(99);
	CHECK(InstallSnapshot(bad,
		{invalidStance, Actor(2, 200)}),
		"invalid-stance baseline fixture creates");
	rejects(std::move(bad), "non-enumerated baseline stance is rejected");

	bad = baseline;
	CHECK(InstallSnapshot(bad,
		{Actor(1, 100, 1), Actor(1, 101, 2)}),
		"duplicate-slot baseline fixture creates");
	rejects(std::move(bad),
		"two live incarnations of one actor slot are rejected");

	bad = baseline;
	CHECK(InstallSnapshot(bad,
		{Actor(1, 100, 1, false, true)}),
		"impossible-presence baseline fixture creates");
	rejects(std::move(bad),
		"in-sector but inactive baseline actor is rejected");

	bad = baseline;
	bad.assignedActors = {TacticalEntityId{3, 1}};
	rejects(std::move(bad),
		"an absent actor cannot be assigned by the accepted baseline");

	bad = baseline;
	bad.assignedActors = {
		TacticalEntityId{2, 1}, TacticalEntityId{1, 1}};
	rejects(std::move(bad),
		"noncanonical baseline actor assignments are rejected");
}

void TestDeltaEnvelopeAndPreviousValueValidation()
{
	FullEngineCoopSnapshotReplica replica;
	const CoopTacticalBaseline baseline = Baseline();
	CHECK(replica.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"delta-validation fixture commits");
	auto rejects = [&](CoopTacticalDelta candidate, const char* message) {
		CHECK(replica.applyDelta(candidate) ==
			FullEngineCoopReplicaApplyResult::Rejected, message);
		CheckOriginalView(replica,
			"rejected delta preserves every committed value");
	};

	CoopTacticalDelta bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{1, 1}, 999, 105, 0, 0, 2, 3});
	rejects(std::move(bad), "mismatched previous actor value is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorLoadoutChangedEvent{
		TacticalEntityId{1, 1}, CombatLoadout(8), CombatLoadout(6)});
	rejects(std::move(bad),
		"mismatched previous combat loadout is rejected");

	bad = DeltaFrom(baseline);
	bad.state.sessionEpoch = 8;
	rejects(std::move(bad), "foreign session delta is rejected");

	bad = DeltaFrom(baseline);
	bad.baseRevision = 19;
	rejects(std::move(bad), "noncurrent base revision is rejected");

	bad = DeltaFrom(baseline, 20);
	rejects(std::move(bad), "nonadvancing revision is rejected");

	bad = DeltaFrom(baseline);
	bad.deltaId = 0;
	rejects(std::move(bad), "zero delta identity is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.currentEpoch = 12;
	rejects(std::move(bad), "mismatched inner generation is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.assign(MaximumCoopTacticalDeltaEvents + 1,
		TacticalActorLeftEvent{TacticalEntityId{1, 1}});
	rejects(std::move(bad), "delta event capacity is enforced locally");
}

void TestNoncanonicalAndAmbiguousDeltasAreRejected()
{
	FullEngineCoopSnapshotReplica replica;
	const CoopTacticalBaseline baseline = Baseline();
	CHECK(replica.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"canonical-validation fixture commits");
	auto rejects = [&](CoopTacticalDelta candidate, const char* message) {
		CHECK(replica.applyDelta(candidate) ==
			FullEngineCoopReplicaApplyResult::Rejected, message);
		CheckOriginalView(replica,
			"noncanonical delta rejection is transactional");
	};

	CoopTacticalDelta bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorVitalsChangedEvent{
		TacticalEntityId{1, 1}, 20, 19, 80, 79, 80, 80, 90, 89, 90, 90});
	bad.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{2, 1}, 200, 201, 0, 0, 2, 3});
	rejects(std::move(bad), "decreasing event-kind order is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{1, 1}, 100, 101, 0, 0, 2, 3});
	bad.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{1, 1}, 101, 102, 0, 0, 3, 4});
	rejects(std::move(bad),
		"duplicate actor event in one canonical category is rejected");

	bad = DeltaFrom(baseline, 21, 31);
	bad.delta.events.push_back(TacticalTurnChangedEvent{
		baseline.snapshot.turn(), TacticalTurnSnapshot{true, true, 1, 30}});
	bad.delta.events.push_back(TacticalTurnChangedEvent{
		TacticalTurnSnapshot{true, true, 1, 30},
		TacticalTurnSnapshot{true, true, 2, 31}});
	rejects(std::move(bad), "multiple chained turn edges are rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorEnteredEvent{Actor(3, 300)});
	bad.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{3, 1}, 300, 301, 0, 0, 2, 3});
	rejects(std::move(bad),
		"an entering actor cannot also carry a partial update");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorEnteredEvent{Actor(3, 300)});
	bad.delta.events.push_back(
		TacticalActorLeftEvent{TacticalEntityId{3, 1}});
	rejects(std::move(bad),
		"one delta cannot enter and leave the same incarnation");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorMovedEvent{
		TacticalEntityId{1, 1}, 100, 100, 0, 0, 2, 2});
	rejects(std::move(bad), "a redundant no-op actor event is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorLoadoutChangedEvent{
		TacticalEntityId{1, 1}, CombatLoadout(), CombatLoadout()});
	rejects(std::move(bad), "a redundant no-op loadout event is rejected");

	bad = DeltaFrom(baseline);
	TacticalActorLoadoutSnapshot invalidLoadout = CombatLoadout(6);
	invalidLoadout.primaryHand.quantity = 0;
	bad.delta.events.push_back(TacticalActorLoadoutChangedEvent{
		TacticalEntityId{1, 1}, CombatLoadout(), invalidLoadout});
	rejects(std::move(bad),
		"a noncanonical current combat loadout is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorStanceChangedEvent{
		TacticalEntityId{1, 1}, TacticalStance::Standing,
		static_cast<TacticalStance>(99), 10, 11});
	rejects(std::move(bad), "non-enumerated delta stance is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorEnteredEvent{
		Actor(4, 400, 1, false, false)});
	rejects(std::move(bad), "non-present entered actor is rejected");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalActorEnteredEvent{Actor(1, 101, 2)});
	rejects(std::move(bad),
		"a second incarnation cannot remain in one live actor slot");

	bad = DeltaFrom(baseline);
	bad.delta.events.push_back(TacticalSectorChangedEvent{
		baseline.snapshot.sector(), TacticalSectorSnapshot{3, 4, 0, false}});
	bad.delta.events.push_back(
		TacticalActorLeftEvent{TacticalEntityId{1, 1}});
	bad.delta.events.push_back(
		TacticalActorLeftEvent{TacticalEntityId{2, 1}});
	rejects(std::move(bad),
		"world unload cannot masquerade as an in-generation delta");
}

void TestIncarnationReplacementCapacityAndEmptyDelta()
{
	CoopTacticalBaseline baseline = Baseline();
	FullEngineCoopSnapshotReplica replacement;
	CHECK(replacement.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"incarnation fixture baseline commits");
	TacticalWorldSnapshot reincarnatedWorld;
	CHECK(TacticalWorldSnapshot::create(
		baseline.state.worldGeneration, baseline.snapshot.dimensions(),
		baseline.snapshot.sector(),
		baseline.snapshot.turn(),
		{Actor(1, 101, 2), Actor(2, 200),
			Actor(3, -1, 1, true, false)},
		reincarnatedWorld, MaximumCoopTacticalSnapshotActors) ==
		TacticalSnapshotCreateError::None,
		"reincarnated world fixture creates");
	CoopTacticalDelta replace = DeltaFrom(baseline);
	const TacticalEntityId newIncarnation{1, 2};
	const TacticalEntityId oldIncarnation{1, 1};
	CHECK(DiffTacticalWorldSnapshots(baseline.snapshot, reincarnatedWorld,
		2, replace.delta) == TacticalWorldDiffResult::Success &&
		replace.delta.events.size() == 2 &&
		std::holds_alternative<TacticalActorEnteredEvent>(
			replace.delta.events[0]) &&
		std::get<TacticalActorEnteredEvent>(replace.delta.events[0]).actor.id ==
			newIncarnation &&
		std::holds_alternative<TacticalActorLeftEvent>(
			replace.delta.events[1]) &&
		std::get<TacticalActorLeftEvent>(replace.delta.events[1]).actor ==
			oldIncarnation,
		"diff emits incarnation replacement in frozen Enter-then-Left order");
	std::vector<std::uint8_t> encodedReplacement;
	CHECK(EncodeCoopTacticalDelta(replace, encodedReplacement) ==
		CoopTacticalCodecResult::Success,
		"incarnation replacement is accepted by the frozen co-op codec");
	CHECK(replacement.applyDelta(replace) ==
		FullEngineCoopReplicaApplyResult::Committed &&
		replacement.snapshot().find(TacticalEntityId{1, 1}) == nullptr &&
		replacement.snapshot().find(TacticalEntityId{1, 2}) != nullptr &&
		replacement.snapshot().actors().size() == 2,
		"enter-then-leave replaces one slot incarnation atomically");

	FullEngineCoopSnapshotReplica emptyChange;
	CHECK(emptyChange.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed,
		"empty-delta fixture baseline commits");
	CoopTacticalDelta empty = DeltaFrom(baseline, 25, 30);
	CHECK(emptyChange.applyDelta(empty) ==
		FullEngineCoopReplicaApplyResult::Committed &&
		emptyChange.state().revision == 25 &&
		emptyChange.snapshot().actors().size() == 2,
		"an advancing authority revision may carry an empty world delta");

	CoopTacticalBaseline full = baseline;
	std::vector<TacticalActorSnapshot> actors;
	actors.reserve(MaximumCoopTacticalSnapshotActors);
	for (std::size_t index = 0;
		index < MaximumCoopTacticalSnapshotActors; ++index)
		actors.push_back(Actor(static_cast<std::uint16_t>(index),
			static_cast<std::int32_t>(index)));
	CHECK(InstallSnapshot(full, std::move(actors)),
		"full-capacity baseline fixture creates");
	FullEngineCoopSnapshotReplica capacity;
	CHECK(capacity.applyBaseline(full) ==
		FullEngineCoopReplicaApplyResult::Committed &&
		capacity.snapshot().actors().size() ==
			MaximumCoopTacticalSnapshotActors,
		"maximum actor baseline commits");
	CoopTacticalDelta overflow = DeltaFrom(full);
	overflow.delta.events.push_back(TacticalActorEnteredEvent{
		Actor(static_cast<std::uint16_t>(MaximumCoopTacticalSnapshotActors),
			500)});
	CHECK(capacity.applyDelta(overflow) ==
		FullEngineCoopReplicaApplyResult::Rejected &&
		capacity.state().revision == full.state.revision &&
		capacity.snapshot().actors().size() ==
			MaximumCoopTacticalSnapshotActors,
		"actor-capacity failure preserves a full committed baseline");

	std::vector<TacticalActorSnapshot> reincarnatedActors;
	reincarnatedActors.reserve(MaximumCoopTacticalSnapshotActors);
	for (std::size_t index = 0;
		index < MaximumCoopTacticalSnapshotActors; ++index)
		reincarnatedActors.push_back(Actor(
			static_cast<std::uint16_t>(index),
			static_cast<std::int32_t>(index + 1000), 2));
	TacticalWorldSnapshot allReincarnated;
	CHECK(TacticalWorldSnapshot::create(full.state.worldGeneration,
		full.snapshot.dimensions(), full.snapshot.sector(),
		full.snapshot.turn(),
		std::move(reincarnatedActors), allReincarnated,
		MaximumCoopTacticalSnapshotActors) ==
		TacticalSnapshotCreateError::None,
		"maximum reincarnation fixture creates");
	CoopTacticalDelta replaceAll = DeltaFrom(full);
	CHECK(DiffTacticalWorldSnapshots(full.snapshot, allReincarnated,
		MaximumCoopTacticalSnapshotActors * 2, replaceAll.delta) ==
		TacticalWorldDiffResult::Success &&
		replaceAll.delta.events.size() ==
			MaximumCoopTacticalSnapshotActors * 2 &&
		std::holds_alternative<TacticalActorEnteredEvent>(
			replaceAll.delta.events[0]) &&
		std::holds_alternative<TacticalActorEnteredEvent>(
			replaceAll.delta.events[
				MaximumCoopTacticalSnapshotActors - 1]) &&
		std::holds_alternative<TacticalActorLeftEvent>(
			replaceAll.delta.events[MaximumCoopTacticalSnapshotActors]) &&
		std::holds_alternative<TacticalActorLeftEvent>(
			replaceAll.delta.events.back()),
		"maximum reincarnation diff retains category-major ordering");
	std::vector<std::uint8_t> encodedReplaceAll;
	CHECK(EncodeCoopTacticalDelta(replaceAll, encodedReplaceAll) ==
		CoopTacticalCodecResult::Success,
		"maximum reincarnation diff stays within the frozen wire bounds");
	CHECK(capacity.applyDelta(replaceAll) ==
		FullEngineCoopReplicaApplyResult::Committed &&
		capacity.snapshot().actors().size() ==
			MaximumCoopTacticalSnapshotActors &&
		capacity.snapshot().find(TacticalEntityId{0, 1}) == nullptr &&
		capacity.snapshot().find(TacticalEntityId{0, 2}) != nullptr &&
		capacity.snapshot().find(TacticalEntityId{
			static_cast<std::uint16_t>(
				MaximumCoopTacticalSnapshotActors - 1), 2}) != nullptr,
		"the fixed membership table handles its maximum replacement transaction");
}

void TestDoorAndHostilityProjectionIsTransactional()
{
	CoopTacticalBaseline baseline = Baseline();
	CHECK(InstallSnapshot(baseline,
		{Actor(1, 100), Actor(2, 200), Actor(3, -1, 1, true, false)},
		TacticalSectorSnapshot{3, 4, 0, true},
		{TacticalDoorSnapshot{100, 41, false},
		 TacticalDoorSnapshot{101, 42, true}}),
		"door replica baseline fixture creates");
	FullEngineCoopSnapshotReplica replica;
	CHECK(replica.applyBaseline(baseline) ==
		FullEngineCoopReplicaApplyResult::Committed &&
		replica.snapshot().doors().size() == 2 &&
		replica.snapshot().findDoor(100)->structureId == 41,
		"baseline retains the complete public door projection");

	CoopTacticalDelta delta = DeltaFrom(baseline);
	delta.delta.events.push_back(TacticalActorVitalsChangedEvent{
		TacticalEntityId{1, 1}, 20, 20, 80, 80, 80, 80, 90, 90, 90, 90,
		false, true});
	delta.delta.events.push_back(TacticalDoorEnteredEvent{
		TacticalDoorSnapshot{102, 43, false}});
	delta.delta.events.push_back(TacticalDoorLeftEvent{101});
	delta.delta.events.push_back(TacticalDoorChangedEvent{
		TacticalDoorSnapshot{100, 41, false},
		TacticalDoorSnapshot{100, 44, true}});
	CHECK(replica.applyDelta(delta) ==
		FullEngineCoopReplicaApplyResult::Committed &&
		replica.snapshot().find(TacticalEntityId{1, 1})->
			hostileToPlayerTeam &&
		replica.snapshot().doors().size() == 2 &&
		replica.snapshot().findDoor(100)->structureId == 44 &&
		replica.snapshot().findDoor(100)->open &&
		replica.snapshot().findDoor(101) == nullptr &&
		replica.snapshot().findDoor(102)->structureId == 43,
		"hostility and Enter/Left/Changed doors commit in one transaction");

	const std::uint64_t committedRevision = replica.state().revision;
	CoopTacticalDelta stale;
	stale.state = replica.state();
	stale.state.revision++;
	stale.deltaId = 2;
	stale.baseRevision = committedRevision;
	stale.delta.previousEpoch = baseline.state.worldGeneration;
	stale.delta.currentEpoch = baseline.state.worldGeneration;
	stale.delta.events.push_back(TacticalDoorChangedEvent{
		TacticalDoorSnapshot{100, 41, false},
		TacticalDoorSnapshot{100, 45, false}});
	CHECK(replica.applyDelta(stale) ==
		FullEngineCoopReplicaApplyResult::Rejected &&
		replica.state().revision == committedRevision &&
		replica.snapshot().findDoor(100)->structureId == 44 &&
		replica.snapshot().findDoor(102) != nullptr,
		"stale previous door identity rejects without partial mutation");

	CoopTacticalDelta ambiguous = stale;
	ambiguous.delta.events.clear();
	ambiguous.delta.events.push_back(TacticalDoorEnteredEvent{
		TacticalDoorSnapshot{103, 46, false}});
	ambiguous.delta.events.push_back(TacticalDoorChangedEvent{
		TacticalDoorSnapshot{103, 46, false},
		TacticalDoorSnapshot{103, 47, true}});
	CHECK(replica.applyDelta(ambiguous) ==
		FullEngineCoopReplicaApplyResult::Rejected &&
		replica.snapshot().findDoor(103) == nullptr,
		"one delta cannot enter and change the same logical door");

	CoopTacticalDelta noncanonical = stale;
	noncanonical.delta.events.clear();
	noncanonical.delta.events.push_back(TacticalDoorChangedEvent{
		TacticalDoorSnapshot{100, 44, true},
		TacticalDoorSnapshot{100, 45, false}});
	noncanonical.delta.events.push_back(TacticalDoorEnteredEvent{
		TacticalDoorSnapshot{103, 46, false}});
	CHECK(replica.applyDelta(noncanonical) ==
		FullEngineCoopReplicaApplyResult::Rejected &&
		replica.snapshot().findDoor(100)->structureId == 44,
		"decreasing door event category order is rejected transactionally");
}
}

int main()
{
	TestPresentProjectionAndFullDelta();
	TestCommandBusyOnlyDeltaIsTransactional();
	TestGenerationRequiresFreshBaseline();
	TestBaselineValidationPreservesState();
	TestDeltaEnvelopeAndPreviousValueValidation();
	TestNoncanonicalAndAmbiguousDeltasAreRejected();
	TestIncarnationReplacementCapacityAndEmptyDelta();
	TestDoorAndHostilityProjectionIsTransactional();
	if (failures != 0)
	{
		std::printf("%d full-engine co-op snapshot replica test(s) failed\n",
			failures);
		return 1;
	}
	std::puts("full-engine co-op snapshot replica tests passed");
	return 0;
}
