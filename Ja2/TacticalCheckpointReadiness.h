#ifndef JA2_TACTICAL_CHECKPOINT_READINESS_H
#define JA2_TACTICAL_CHECKPOINT_READINESS_H

#include <cstdint>

// Diagnostic evidence only. This does not authorize a checkpoint or certify
// native save/load fidelity. Capture on the simulation thread at a stopped
// boundary; it never ticks, drains, cancels, or reconciles native state.
enum class Ja2TacticalCheckpointActorHazard : std::uint32_t
{
	InvalidBinding = 1u << 0,
	InvalidAnimation = 1u << 1,
	NonIdleAnimation = 1u << 2,
	CoweringOrCollapsed = 1u << 3,
	InvalidRoute = 1u << 4,
	RouteContinuation = 1u << 5,
	Turning = 1u << 6,
	AnimationContinuation = 1u << 7,
	PendingAction = 1u << 8,
	PendingItem = 1u << 9,
	MedicalService = 1u << 10,
	DamageRecovery = 1u << 11,
	AiContinuation = 1u << 12,
	ScheduleContinuation = 1u << 13,
	DeferredCallback = 1u << 14,
	Interaction = 1u << 15,
	FireContinuation = 1u << 16,
	WorldObjectContinuation = 1u << 17
};

struct Ja2TacticalCheckpointActorEvidence
{
	// Validity of this bounded actor scan, not completeness of checkpoint safety.
	bool complete = false;
	std::uint32_t scannedSlots = 0;
	std::uint32_t activeActors = 0;
	std::uint32_t inSectorActors = 0;
	std::uint32_t awayActors = 0;
	std::uint32_t busyActors = 0;
	std::uint32_t hazards = 0;
	std::uint16_t firstBusySlot = UINT16_MAX;
	std::uint32_t firstBusyIncarnation = 0;
};

struct Ja2TacticalCheckpointTurnEvidence
{
	bool worldLoaded = false;
	bool worldIntegrityValid = false;
	bool sectorValid = false;
	bool turnBased = false;
	bool inCombat = false;
	std::uint8_t currentTeam = UINT8_MAX;
	std::uint8_t playerTeam = UINT8_MAX;
	std::uint32_t pendingCombatActions = 0;
	std::uint8_t pendingInterrupt = 0;
	std::uint16_t queuedInterrupters = 0;
	bool interruptResolving = false;
	bool interruptActive = false;
	bool hiddenInterrupt = false;
	bool playerInterruptsDisabled = false;
};

struct Ja2TacticalCheckpointEventEvidence
{
	std::uint32_t primary = 0;
	std::uint32_t delayed = 0;
	std::uint32_t demand = 0;
	std::uint32_t payloadBytes = 0;
};

struct Ja2TacticalCheckpointEffectEvidence
{
	// Validity of the advertised effect scans, not all tactical continuations.
	bool complete = false;
	std::uint32_t bullets = 0;
	std::uint32_t physicsObjects = 0;
	std::uint32_t explosions = 0;
	std::uint32_t smokeEffects = 0;
	std::uint32_t lightEffects = 0;
	std::uint32_t timedWorldBombs = 0;
	std::uint8_t queuedExplosions = 0;
	bool explosionQueueActive = false;
	bool explosionSightUpdatePending = false;
	// Any live transient tile is conservative evidence, including paused tiles
	// whose completion may release an attacker or run a damage/door callback.
	bool animationTilesPresent = false;
};

struct Ja2TacticalCheckpointContinuationEvidence
{
	bool dialogueActive = false;
	bool dialogueQueued = false;
	bool triggerTimer = false;
	bool customTimer = false;
	bool arrivalDecision = false;
	bool surrenderDecision = false;
	bool battleNotice = false;
	bool meanwhileDecision = false;
	bool meanwhile = false;
	bool autoResolve = false;
	bool traversal = false;
	bool autoBandage = false;
	bool boxing = false;
	bool loading = false;
	bool uiTransition = false;
	bool conversation = false;
	bool sightSuppressed = false;
	bool enemySighting = false;
};

struct Ja2TacticalCheckpointReadiness
{
	bool observed = false;
	Ja2TacticalCheckpointTurnEvidence turn;
	Ja2TacticalCheckpointActorEvidence actors;
	Ja2TacticalCheckpointEventEvidence events;
	Ja2TacticalCheckpointEffectEvidence effects;
	Ja2TacticalCheckpointContinuationEvidence continuations;
};

// Scans every active repository actor, including active away actors serialized
// in the save, and fixed native effect pools. No native pointers escape.
// Separate responsibilities remain: committed frame and package/transport/
// receipt drains; reinforcement/schedule association and extension validation;
// inventory-contained timed effects and callback registries other than the
// observed custom/trigger timers and actor delayed-damage callbacks; and a real
// full save/load roundtrip. None of the evidence enables tactical saving.
Ja2TacticalCheckpointReadiness CaptureJa2TacticalCheckpointReadiness() noexcept;

#endif
