#include <Engine/Core/DedicatedCheckpointEligibility.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <type_traits>

namespace
{
using Reason = DedicatedCheckpointEligibilityReason;
using Snapshot = DedicatedCheckpointEligibilitySnapshot;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	std::exit(1);
}

Snapshot AllClear()
{
	Snapshot snapshot;
	snapshot.resumeMode = DedicatedCheckpointResumeMode::Cold;
	snapshot.commandQueue = DedicatedCheckpointDrainState::Drained;
	snapshot.networkQueue = DedicatedCheckpointDrainState::Drained;
	snapshot.packageQueue = DedicatedCheckpointDrainState::Drained;
	snapshot.dialogueQueue = DedicatedCheckpointDrainState::Drained;
	snapshot.simulation = DedicatedCheckpointRunState::Paused;
	snapshot.frameBoundary = DedicatedCheckpointFrameBoundary::Committed;
	snapshot.tacticalWorldLoaded = false;
	snapshot.worldItemsLoaded = false;
	snapshot.combatActive = false;
	snapshot.autoResolveActive = false;
	snapshot.meanwhileActive = false;
	snapshot.projectileActive = false;
	snapshot.explosionActive = false;
	snapshot.dialogueActive = false;
	snapshot.realtimeAiActive = false;
	snapshot.customizableCallbackPending = false;
	snapshot.reinforcementTurnCounter = 0;
	snapshot.enemyReinforcementTurn = 0;
	snapshot.enemyReinforcementsArrived = 0;
	snapshot.militiaReinforcementTurn = 0;
	snapshot.militiaReinforcementsArrived = 0;
	snapshot.temporarySchedulesPresent = false;
	snapshot.miniEventsEnabled = false;
	snapshot.unrestrictedLuaRandomnessEnabled = false;
	return snapshot;
}

template <typename Mutator>
void ExpectRejected(Reason expected, const char* message, Mutator mutate)
{
	Snapshot snapshot = AllClear();
	mutate(snapshot);
	Check(EvaluateDedicatedCheckpointEligibility(snapshot) == expected, message);
}

void TestAllClearAndDefaults()
{
	const Snapshot allClear = AllClear();
	Check(EvaluateDedicatedCheckpointEligibility(allClear) == Reason::None,
		"the fully quiescent strategic cold-resume snapshot is eligible");
	Check(std::string_view(DedicatedCheckpointEligibilityReasonName(
		EvaluateDedicatedCheckpointEligibility(allClear))) == "none",
		"the eligible result has stable diagnostic text");

	const Snapshot unspecified;
	Check(EvaluateDedicatedCheckpointEligibility(unspecified) ==
		Reason::InvalidResumeMode,
		"a default snapshot fails closed instead of assuming safe declarations");

	Snapshot partial;
	partial.resumeMode = DedicatedCheckpointResumeMode::Cold;
	partial.commandQueue = DedicatedCheckpointDrainState::Drained;
	partial.networkQueue = DedicatedCheckpointDrainState::Drained;
	partial.packageQueue = DedicatedCheckpointDrainState::Drained;
	partial.dialogueQueue = DedicatedCheckpointDrainState::Drained;
	partial.simulation = DedicatedCheckpointRunState::Paused;
	partial.frameBoundary = DedicatedCheckpointFrameBoundary::Committed;
	Check(EvaluateDedicatedCheckpointEligibility(partial) ==
		Reason::TacticalWorldLoaded,
		"a partial collector snapshot retains unsafe hazard defaults");
}

void TestEveryUnobservedHazardDefaultsUnsafe()
{
	const Snapshot defaults;
	Check(defaults.tacticalWorldLoaded && defaults.worldItemsLoaded &&
		defaults.combatActive && defaults.autoResolveActive &&
		defaults.meanwhileActive && defaults.projectileActive &&
		defaults.explosionActive && defaults.dialogueActive &&
		defaults.realtimeAiActive && defaults.customizableCallbackPending &&
		defaults.reinforcementTurnCounter != 0 &&
		defaults.enemyReinforcementTurn != 0 &&
		defaults.enemyReinforcementsArrived != 0 &&
		defaults.militiaReinforcementTurn != 0 &&
		defaults.militiaReinforcementsArrived != 0 &&
		defaults.temporarySchedulesPresent && defaults.miniEventsEnabled &&
		defaults.unrestrictedLuaRandomnessEnabled,
		"every unobserved hazard defaults unsafe");
}

void TestOneConditionAtATime()
{
	ExpectRejected(Reason::WarmResumeRequested, "warm resume is rejected",
		[](Snapshot& value) {
			value.resumeMode = DedicatedCheckpointResumeMode::Warm;
		});
	ExpectRejected(Reason::TacticalWorldLoaded, "a loaded tactical world is rejected",
		[](Snapshot& value) { value.tacticalWorldLoaded = true; });
	ExpectRejected(Reason::WorldItemsLoaded, "loaded world items are rejected",
		[](Snapshot& value) { value.worldItemsLoaded = true; });
	ExpectRejected(Reason::CombatActive, "active combat is rejected",
		[](Snapshot& value) { value.combatActive = true; });
	ExpectRejected(Reason::AutoResolveActive, "active auto resolve is rejected",
		[](Snapshot& value) { value.autoResolveActive = true; });
	ExpectRejected(Reason::MeanwhileActive, "an active meanwhile is rejected",
		[](Snapshot& value) { value.meanwhileActive = true; });
	ExpectRejected(Reason::ProjectileActive, "active projectiles are rejected",
		[](Snapshot& value) { value.projectileActive = true; });
	ExpectRejected(Reason::ExplosionActive, "active explosions are rejected",
		[](Snapshot& value) { value.explosionActive = true; });
	ExpectRejected(Reason::DialogueActive, "active dialogue is rejected",
		[](Snapshot& value) { value.dialogueActive = true; });
	ExpectRejected(Reason::DialogueQueueNotDrained,
		"queued dialogue effects or trigger timers are rejected",
		[](Snapshot& value) {
			value.dialogueQueue = DedicatedCheckpointDrainState::Pending;
		});
	ExpectRejected(Reason::RealtimeAiActive, "active realtime AI is rejected",
		[](Snapshot& value) { value.realtimeAiActive = true; });
	ExpectRejected(Reason::CommandQueueNotDrained,
		"a pending command queue is rejected", [](Snapshot& value) {
			value.commandQueue = DedicatedCheckpointDrainState::Pending;
		});
	ExpectRejected(Reason::NetworkQueueNotDrained,
		"a pending network queue is rejected", [](Snapshot& value) {
			value.networkQueue = DedicatedCheckpointDrainState::Pending;
		});
	ExpectRejected(Reason::PackageQueueNotDrained,
		"a pending package queue is rejected", [](Snapshot& value) {
			value.packageQueue = DedicatedCheckpointDrainState::Pending;
		});
	ExpectRejected(Reason::SimulationNotPaused,
		"a running simulation is rejected", [](Snapshot& value) {
			value.simulation = DedicatedCheckpointRunState::Running;
		});
	ExpectRejected(Reason::NotAtCommittedFrameBoundary,
		"an in-progress frame is rejected", [](Snapshot& value) {
			value.frameBoundary = DedicatedCheckpointFrameBoundary::InProgress;
		});
	ExpectRejected(Reason::CustomizableCallbackPending,
		"a pending customizable callback is rejected",
		[](Snapshot& value) { value.customizableCallbackPending = true; });
	ExpectRejected(Reason::ReinforcementTurnCounterNonzero,
		"the reinforcement turn counter is rejected",
		[](Snapshot& value) { value.reinforcementTurnCounter = 1; });
	ExpectRejected(Reason::EnemyReinforcementTurnNonzero,
		"the enemy reinforcement turn is rejected",
		[](Snapshot& value) { value.enemyReinforcementTurn = 1; });
	ExpectRejected(Reason::EnemyReinforcementsArrivedNonzero,
		"the enemy reinforcement arrival count is rejected",
		[](Snapshot& value) { value.enemyReinforcementsArrived = 1; });
	ExpectRejected(Reason::MilitiaReinforcementTurnNonzero,
		"the militia reinforcement turn is rejected",
		[](Snapshot& value) { value.militiaReinforcementTurn = 1; });
	ExpectRejected(Reason::MilitiaReinforcementsArrivedNonzero,
		"the militia reinforcement arrival count is rejected",
		[](Snapshot& value) { value.militiaReinforcementsArrived = 1; });
	ExpectRejected(Reason::TemporarySchedulesPresent,
		"temporary schedules are rejected",
		[](Snapshot& value) { value.temporarySchedulesPresent = true; });
	ExpectRejected(Reason::MiniEventsEnabled, "MiniEvents are rejected",
		[](Snapshot& value) { value.miniEventsEnabled = true; });
	ExpectRejected(Reason::UnrestrictedLuaRandomnessEnabled,
		"unrestricted Lua randomness is rejected",
		[](Snapshot& value) { value.unrestrictedLuaRandomnessEnabled = true; });
}

void TestInvalidAndContradictoryDeclarations()
{
	for (const std::uint8_t raw : {std::uint8_t{0}, std::uint8_t{3},
		std::uint8_t{255}})
	{
		ExpectRejected(Reason::InvalidResumeMode,
			"unspecified, contradictory, and unknown resume declarations fail closed",
			[raw](Snapshot& value) {
				value.resumeMode =
					static_cast<DedicatedCheckpointResumeMode>(raw);
			});
		ExpectRejected(Reason::InvalidCommandQueueState,
			"invalid command queue declarations fail closed",
			[raw](Snapshot& value) {
				value.commandQueue =
					static_cast<DedicatedCheckpointDrainState>(raw);
			});
		ExpectRejected(Reason::InvalidNetworkQueueState,
			"invalid network queue declarations fail closed",
			[raw](Snapshot& value) {
				value.networkQueue =
					static_cast<DedicatedCheckpointDrainState>(raw);
			});
		ExpectRejected(Reason::InvalidPackageQueueState,
			"invalid package queue declarations fail closed",
			[raw](Snapshot& value) {
				value.packageQueue =
					static_cast<DedicatedCheckpointDrainState>(raw);
			});
		ExpectRejected(Reason::InvalidDialogueQueueState,
			"invalid dialogue queue declarations fail closed",
			[raw](Snapshot& value) {
				value.dialogueQueue =
					static_cast<DedicatedCheckpointDrainState>(raw);
			});
		ExpectRejected(Reason::InvalidSimulationState,
			"invalid simulation declarations fail closed",
			[raw](Snapshot& value) {
				value.simulation =
					static_cast<DedicatedCheckpointRunState>(raw);
			});
		ExpectRejected(Reason::InvalidFrameBoundaryState,
			"invalid frame boundary declarations fail closed",
			[raw](Snapshot& value) {
				value.frameBoundary =
					static_cast<DedicatedCheckpointFrameBoundary>(raw);
			});
	}
}

void TestPrecedence()
{
	Snapshot snapshot = AllClear();
	snapshot.resumeMode = DedicatedCheckpointResumeMode::Warm;
	snapshot.tacticalWorldLoaded = true;
	snapshot.worldItemsLoaded = true;
	snapshot.combatActive = true;
	snapshot.autoResolveActive = true;
	snapshot.meanwhileActive = true;
	snapshot.projectileActive = true;
	snapshot.explosionActive = true;
	snapshot.dialogueActive = true;
	snapshot.realtimeAiActive = true;
	snapshot.commandQueue = DedicatedCheckpointDrainState::Pending;
	snapshot.networkQueue = DedicatedCheckpointDrainState::Pending;
	snapshot.packageQueue = DedicatedCheckpointDrainState::Pending;
	snapshot.dialogueQueue = DedicatedCheckpointDrainState::Pending;
	snapshot.simulation = DedicatedCheckpointRunState::Running;
	snapshot.frameBoundary = DedicatedCheckpointFrameBoundary::InProgress;
	snapshot.customizableCallbackPending = true;
	snapshot.reinforcementTurnCounter = 1;
	snapshot.enemyReinforcementTurn = 1;
	snapshot.enemyReinforcementsArrived = 1;
	snapshot.militiaReinforcementTurn = 1;
	snapshot.militiaReinforcementsArrived = 1;
	snapshot.temporarySchedulesPresent = true;
	snapshot.miniEventsEnabled = true;
	snapshot.unrestrictedLuaRandomnessEnabled = true;
	Check(EvaluateDedicatedCheckpointEligibility(snapshot) ==
		Reason::WarmResumeRequested,
		"operational rejection precedence starts with the resume contract");

	snapshot.resumeMode = DedicatedCheckpointResumeMode::Cold;
	Check(EvaluateDedicatedCheckpointEligibility(snapshot) ==
		Reason::TacticalWorldLoaded,
		"tactical residency precedes later operational conditions");

	snapshot.commandQueue =
		static_cast<DedicatedCheckpointDrainState>(std::uint8_t{255});
	Check(EvaluateDedicatedCheckpointEligibility(snapshot) ==
		Reason::InvalidCommandQueueState,
		"invalid declarations precede every operational rejection");
}

void TestGoldenReasonNames()
{
	struct Golden
	{
		Reason reason;
		const char* name;
	};
	constexpr Golden golden[] = {
		{Reason::None, "none"},
		{Reason::InvalidResumeMode, "invalid resume mode"},
		{Reason::InvalidCommandQueueState, "invalid command queue state"},
		{Reason::InvalidNetworkQueueState, "invalid network queue state"},
		{Reason::InvalidPackageQueueState, "invalid package queue state"},
		{Reason::InvalidDialogueQueueState, "invalid dialogue queue state"},
		{Reason::InvalidSimulationState, "invalid simulation state"},
		{Reason::InvalidFrameBoundaryState, "invalid frame boundary state"},
		{Reason::WarmResumeRequested, "warm resume requested"},
		{Reason::TacticalWorldLoaded, "tactical world loaded"},
		{Reason::WorldItemsLoaded, "world items loaded"},
		{Reason::CombatActive, "combat active"},
		{Reason::AutoResolveActive, "auto resolve active"},
		{Reason::MeanwhileActive, "meanwhile active"},
		{Reason::ProjectileActive, "projectile active"},
		{Reason::ExplosionActive, "explosion active"},
		{Reason::DialogueActive, "dialogue active"},
		{Reason::DialogueQueueNotDrained, "dialogue queue not drained"},
		{Reason::RealtimeAiActive, "realtime AI active"},
		{Reason::CommandQueueNotDrained, "command queue not drained"},
		{Reason::NetworkQueueNotDrained, "network queue not drained"},
		{Reason::PackageQueueNotDrained, "package queue not drained"},
		{Reason::SimulationNotPaused, "simulation not paused"},
		{Reason::NotAtCommittedFrameBoundary,
			"not at committed frame boundary"},
		{Reason::CustomizableCallbackPending,
			"customizable callback pending"},
		{Reason::ReinforcementTurnCounterNonzero,
			"reinforcement turn counter nonzero"},
		{Reason::EnemyReinforcementTurnNonzero,
			"enemy reinforcement turn nonzero"},
		{Reason::EnemyReinforcementsArrivedNonzero,
			"enemy reinforcements arrived nonzero"},
		{Reason::MilitiaReinforcementTurnNonzero,
			"militia reinforcement turn nonzero"},
		{Reason::MilitiaReinforcementsArrivedNonzero,
			"militia reinforcements arrived nonzero"},
		{Reason::TemporarySchedulesPresent, "temporary schedules present"},
		{Reason::MiniEventsEnabled, "MiniEvents enabled"},
		{Reason::UnrestrictedLuaRandomnessEnabled,
			"unrestricted Lua randomness enabled"}
	};
	constexpr std::size_t reasonCount =
		static_cast<std::size_t>(Reason::UnrestrictedLuaRandomnessEnabled) + 1;
	static_assert(sizeof(golden) / sizeof(golden[0]) == reasonCount,
		"the golden table must cover every eligibility reason");
	for (const Golden& entry : golden)
	{
		Check(std::string_view(DedicatedCheckpointEligibilityReasonName(
			entry.reason)) == entry.name,
			"an eligibility reason name changed");
	}
	Check(std::string_view(DedicatedCheckpointEligibilityReasonName(
		static_cast<Reason>(std::uint8_t{255}))) ==
		"unknown dedicated checkpoint eligibility reason",
		"unknown eligibility reasons have fail-closed diagnostic text");
}
}

int main()
{
	static_assert(std::is_standard_layout<Snapshot>::value,
		"the eligibility snapshot remains a plain value contract");
	static_assert(std::is_trivially_copyable<Snapshot>::value,
		"the eligibility snapshot remains bounded and data-free");

	TestAllClearAndDefaults();
	TestEveryUnobservedHazardDefaultsUnsafe();
	TestOneConditionAtATime();
	TestInvalidAndContradictoryDeclarations();
	TestPrecedence();
	TestGoldenReasonNames();
	std::puts("dedicated checkpoint eligibility tests passed");
	return 0;
}
