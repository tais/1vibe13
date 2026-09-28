#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_MOVE_DIAGNOSTIC_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_MOVE_DIAGNOSTIC_H
#include <cstdint>

// Server-local observations only; these are not protocol or command outcomes.
enum class TacticalMoveFailure : std::uint8_t
{
	None,
	LiveRouteContext,
	Health,
	Collapsed,
	ThroughPeoplePolicy,
	DestinationBounds,
	SameDestination,
	MovementMode,
	MovementAnimation,
	AnimationBounds,
	AnimationActivity,
	PendingAnimation,
	SecondaryAnimation,
	PendingStance,
	PendingDirection,
	StanceContinuation,
	StopNextTile,
	TurningUntilDone,
	TurningFromProne,
	TurningToShoot,
	TurningToFall,
	NonInterruptible,
	RealtimeNonInterruptible,
	AnimationPaused,
	GettingHit,
	HeldAttacker,
	SuppressionStance,
	PendingAction,
	ActionLockOrCowering,
	Cowering,
	RemainingShots,
	DelayedDamage,
	WaitingAction,
	MovementTurn,
	DirectionMismatch,
	MovementPaused,
	ContinuedPath,
	ScheduleAssigned,
	ScheduleDoor,
	PathSize,
	PathIndex,
	PathIndexAfterEnd,
	PathUnconsumed,
	NonIdlePose,
	RetainedRoute,
	ProbeWrongThread,
	ProbeReentry,
	ProbeStorage,
	NoPath,
	ProbeDirection,
	ProbeRandom,
	ProbeException,
	NextGridBounds,
	NextGridUnchanged,
	NegativeStepCost,
	InsufficientPoints,
	WrongThread,
	ActorUnavailable,
	NativePathStart,
	ContextUnavailable,
	ActorCapture,
	ActorIdentity,
	WrongTeam,
	NotControllable,
	CombatOrInterruptPending,
	WrongTurn,
	InterruptResolving,
	InterruptEligibility,
	InterruptPhase,
	LegacyNetworking,
};

inline const char* TacticalMoveFailureName(TacticalMoveFailure value) noexcept
{
	switch (value)
	{
	case TacticalMoveFailure::None: return "accepted";
	case TacticalMoveFailure::LiveRouteContext: return "invalid-live-route-context";
	case TacticalMoveFailure::Health: return "insufficient-health";
	case TacticalMoveFailure::Collapsed: return "collapsed";
	case TacticalMoveFailure::ThroughPeoplePolicy: return "through-people-policy";
	case TacticalMoveFailure::DestinationBounds: return "destination-out-of-bounds";
	case TacticalMoveFailure::SameDestination: return "same-grid";
	case TacticalMoveFailure::MovementMode: return "invalid-movement-mode";
	case TacticalMoveFailure::MovementAnimation: return "movement-mode-not-moving";
	case TacticalMoveFailure::AnimationBounds: return "invalid-animation";
	case TacticalMoveFailure::AnimationActivity: return "active-animation-flags";
	case TacticalMoveFailure::PendingAnimation: return "pending-animation";
	case TacticalMoveFailure::SecondaryAnimation: return "secondary-pending-animation";
	case TacticalMoveFailure::PendingStance: return "pending-stance";
	case TacticalMoveFailure::PendingDirection: return "pending-direction";
	case TacticalMoveFailure::StanceContinuation: return "stance-continuation";
	case TacticalMoveFailure::StopNextTile: return "stop-next-tile";
	case TacticalMoveFailure::TurningUntilDone: return "turning-until-done";
	case TacticalMoveFailure::TurningFromProne: return "turning-from-prone";
	case TacticalMoveFailure::TurningToShoot: return "turning-to-shoot";
	case TacticalMoveFailure::TurningToFall: return "turning-to-fall";
	case TacticalMoveFailure::NonInterruptible: return "noninterruptible";
	case TacticalMoveFailure::RealtimeNonInterruptible: return "realtime-noninterruptible";
	case TacticalMoveFailure::AnimationPaused: return "animation-paused";
	case TacticalMoveFailure::GettingHit: return "getting-hit";
	case TacticalMoveFailure::HeldAttacker: return "held-attacker";
	case TacticalMoveFailure::SuppressionStance: return "suppression-stance";
	case TacticalMoveFailure::PendingAction: return "pending-action";
	case TacticalMoveFailure::ActionLockOrCowering: return "pending-action-lock-or-cowering";
	case TacticalMoveFailure::Cowering: return "cowering";
	case TacticalMoveFailure::RemainingShots: return "remaining-shots";
	case TacticalMoveFailure::DelayedDamage: return "delayed-damage-callback";
	case TacticalMoveFailure::WaitingAction: return "waiting-for-action";
	case TacticalMoveFailure::MovementTurn: return "movement-turn-active";
	case TacticalMoveFailure::DirectionMismatch: return "unfinished-direction";
	case TacticalMoveFailure::MovementPaused: return "movement-paused";
	case TacticalMoveFailure::ContinuedPath: return "continued-path";
	case TacticalMoveFailure::ScheduleAssigned: return "assigned-schedule";
	case TacticalMoveFailure::ScheduleDoor: return "schedule-door-continuation";
	case TacticalMoveFailure::PathSize: return "path-size-out-of-range";
	case TacticalMoveFailure::PathIndex: return "path-index-out-of-range";
	case TacticalMoveFailure::PathIndexAfterEnd: return "path-index-after-end";
	case TacticalMoveFailure::PathUnconsumed: return "unconsumed-path";
	case TacticalMoveFailure::NonIdlePose: return "non-idle-pose";
	case TacticalMoveFailure::RetainedRoute: return "retained-route-continuation";
	case TacticalMoveFailure::ProbeWrongThread: return "path-probe-wrong-thread";
	case TacticalMoveFailure::ProbeReentry: return "path-probe-reentry";
	case TacticalMoveFailure::ProbeStorage: return "path-probe-storage-unavailable";
	case TacticalMoveFailure::NoPath: return "path-not-found";
	case TacticalMoveFailure::ProbeDirection: return "path-probe-invalid-direction";
	case TacticalMoveFailure::ProbeRandom: return "path-probe-random-failure";
	case TacticalMoveFailure::ProbeException: return "path-probe-exception";
	case TacticalMoveFailure::NextGridBounds: return "first-step-out-of-bounds";
	case TacticalMoveFailure::NextGridUnchanged: return "first-step-same-grid";
	case TacticalMoveFailure::NegativeStepCost: return "negative-first-step-cost";
	case TacticalMoveFailure::InsufficientPoints: return "insufficient-first-step-points";
	case TacticalMoveFailure::WrongThread: return "wrong-thread";
	case TacticalMoveFailure::ActorUnavailable: return "actor-unavailable";
	case TacticalMoveFailure::NativePathStart: return "native-path-start-rejected";
	case TacticalMoveFailure::ContextUnavailable: return "context-unavailable";
	case TacticalMoveFailure::ActorCapture: return "actor-capture-failed";
	case TacticalMoveFailure::ActorIdentity: return "actor-identity-or-roster";
	case TacticalMoveFailure::WrongTeam: return "wrong-team";
	case TacticalMoveFailure::NotControllable: return "actor-not-controllable";
	case TacticalMoveFailure::CombatOrInterruptPending: return "combat-action-or-interrupt-pending";
	case TacticalMoveFailure::WrongTurn: return "wrong-player-turn";
	case TacticalMoveFailure::InterruptResolving: return "interrupt-resolving";
	case TacticalMoveFailure::InterruptEligibility: return "interrupt-not-eligible";
	case TacticalMoveFailure::InterruptPhase: return "invalid-interrupt-phase";
	case TacticalMoveFailure::LegacyNetworking: return "legacy-networking-active";
	}
	return "unknown";
}

// No pointers to native state or route/animation array contents escape.
// Scalars describe preflight; path-start rejection retains that preceding snapshot.
struct TacticalMoveDiagnostic
{
	TacticalMoveFailure reason = TacticalMoveFailure::None;
	std::uint16_t actorSlot = UINT16_MAX;
	std::uint32_t incarnation = 0;
	std::uint64_t commandId = 0;
	std::int32_t origin = -1, destination = -1, finalDestination = -1;
	std::int32_t delayedGrid = -1, nextGrid = -1;
	std::uint16_t movementMode = UINT16_MAX, animation = UINT16_MAX;
	std::uint16_t pathIndex = 0, pathSize = 0;
	std::int16_t actionPoints = 0, firstStepCost = -1;
	std::int16_t breath = 0;
	std::uint8_t direction = UINT8_MAX, desiredDirection = UINT8_MAX;
	std::uint8_t firstDirection = UINT8_MAX;
	std::uint32_t delayedFlags = 0, pendingCombatActions = 0;
	std::uint8_t interruptPhase = UINT8_MAX;
	bool reverse = false, actorObserved = false, firstStepObserved = false;
};
#endif
