#ifndef ENGINE_CORE_DEDICATED_CHECKPOINT_ELIGIBILITY_H
#define ENGINE_CORE_DEDICATED_CHECKPOINT_ELIGIBILITY_H

#include <cstdint>

// These state declarations are one-hot. Unspecified, combined (contradictory),
// and otherwise unknown values are rejected before any eligibility decision.
enum class DedicatedCheckpointResumeMode : std::uint8_t
{
	Unspecified = 0,
	Cold = 1,
	Warm = 2
};

enum class DedicatedCheckpointDrainState : std::uint8_t
{
	Unspecified = 0,
	Drained = 1,
	Pending = 2
};

enum class DedicatedCheckpointRunState : std::uint8_t
{
	Unspecified = 0,
	Paused = 1,
	Running = 2
};

enum class DedicatedCheckpointFrameBoundary : std::uint8_t
{
	Unspecified = 0,
	Committed = 1,
	InProgress = 2
};

// A bounded value snapshot supplied by the host at one instant. The policy
// owns no state, reads no globals, and performs no checkpoint I/O.
struct DedicatedCheckpointEligibilitySnapshot
{
	DedicatedCheckpointResumeMode resumeMode =
		DedicatedCheckpointResumeMode::Unspecified;
	DedicatedCheckpointDrainState commandQueue =
		DedicatedCheckpointDrainState::Unspecified;
	DedicatedCheckpointDrainState networkQueue =
		DedicatedCheckpointDrainState::Unspecified;
	DedicatedCheckpointDrainState packageQueue =
		DedicatedCheckpointDrainState::Unspecified;
	// Covers queued dialogue events and the pending trigger timer, not only a
	// face that is actively speaking.
	DedicatedCheckpointDrainState dialogueQueue =
		DedicatedCheckpointDrainState::Unspecified;
	DedicatedCheckpointRunState simulation =
		DedicatedCheckpointRunState::Unspecified;
	DedicatedCheckpointFrameBoundary frameBoundary =
		DedicatedCheckpointFrameBoundary::Unspecified;

	// Every hazard defaults unsafe. A collector must explicitly observe and
	// clear every field before this snapshot can authorize a checkpoint.
	bool tacticalWorldLoaded = true;
	bool worldItemsLoaded = true;
	bool combatActive = true;
	bool autoResolveActive = true;
	bool meanwhileActive = true;
	bool projectileActive = true;
	bool explosionActive = true;
	bool dialogueActive = true;
	bool realtimeAiActive = true;
	bool customizableCallbackPending = true;

	// These five values correspond to the transient reinforcement state that
	// legacy save/load currently reconstructs instead of restoring exactly.
	std::uint32_t reinforcementTurnCounter = 1;
	std::uint32_t enemyReinforcementTurn = 1;
	std::uint32_t enemyReinforcementsArrived = 1;
	std::uint32_t militiaReinforcementTurn = 1;
	std::uint32_t militiaReinforcementsArrived = 1;

	bool temporarySchedulesPresent = true;
	bool miniEventsEnabled = true;
	bool unrestrictedLuaRandomnessEnabled = true;
};

enum class DedicatedCheckpointEligibilityReason : std::uint8_t
{
	None,
	InvalidResumeMode,
	InvalidCommandQueueState,
	InvalidNetworkQueueState,
	InvalidPackageQueueState,
	InvalidDialogueQueueState,
	InvalidSimulationState,
	InvalidFrameBoundaryState,
	WarmResumeRequested,
	TacticalWorldLoaded,
	WorldItemsLoaded,
	CombatActive,
	AutoResolveActive,
	MeanwhileActive,
	ProjectileActive,
	ExplosionActive,
	DialogueActive,
	DialogueQueueNotDrained,
	RealtimeAiActive,
	CommandQueueNotDrained,
	NetworkQueueNotDrained,
	PackageQueueNotDrained,
	SimulationNotPaused,
	NotAtCommittedFrameBoundary,
	CustomizableCallbackPending,
	ReinforcementTurnCounterNonzero,
	EnemyReinforcementTurnNonzero,
	EnemyReinforcementsArrivedNonzero,
	MilitiaReinforcementTurnNonzero,
	MilitiaReinforcementsArrivedNonzero,
	TemporarySchedulesPresent,
	MiniEventsEnabled,
	UnrestrictedLuaRandomnessEnabled
};

// Returns None only for a strategic, quiescent, cold-resume boundary. Invalid
// declarations fail closed and take precedence over operational conditions.
DedicatedCheckpointEligibilityReason EvaluateDedicatedCheckpointEligibility(
	const DedicatedCheckpointEligibilitySnapshot& snapshot) noexcept;

// Stable diagnostic text suitable for logs and tests.
const char* DedicatedCheckpointEligibilityReasonName(
	DedicatedCheckpointEligibilityReason reason) noexcept;

#endif
