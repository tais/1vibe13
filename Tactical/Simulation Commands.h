#ifndef TACTICAL_SIMULATION_COMMANDS_H
#define TACTICAL_SIMULATION_COMMANDS_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandExecutor.h>
#include <Engine/Core/CommandProcessor.h>

class SOLDIERTYPE;
class GameContext;

// Installs the application-owned completion observer used by every execution
// entry point, including synchronous compatibility dispatch. Binding the same
// sink again is harmless; replacing a live binding is rejected. The sink must
// outlive all command execution.
bool BindSimulationCommandExecutionSink(
	SimulationCommandExecutionSink& sink) noexcept;

// Composition-time binding of the live JA2 compatibility executor. Runtime
// ownership prevents command drains from selecting a different world adapter.
bool BindJa2SimulationCommandExecutor(GameContext& game) noexcept;

enum class SimulationCommandDomainError
{
	None,
	ValuelessCommand,
	InvalidSource,
	InvalidEventPolicy,
	InvalidTeam,
	InvalidActor,
	InvalidStance,
	InvalidTargetGrid,
	InvalidTargetLevel,
	InvalidTargetCubeLevel,
	InvalidActorGrid,
	InvalidActorLevel,
	InvalidReportedGrid,
	InvalidReplicatedPath,
	InvalidReplicatedPosition,
	InvalidAttackingHand,
	InvalidAttackingWeapon,
	InvalidDestinationGrid,
	InvalidMovementMode,
	InvalidMoveOrigin,
	InvalidPendingActionPolicy,
	InvalidDirection,
	InvalidTraversalKind,
	InvalidObjectGrid,
	InvalidTargetActor,
	InvalidVehicleSeat,
	InvalidWorldItem,
	InvalidWorldItemRenderHeight,
	InvalidWorldItemPickupKind
};

// Complete value-domain validation shared by package admission and every
// execution entry point. Live actor/context checks remain executor policy.
SimulationCommandDomainError ValidateSimulationCommandDomain(
	const SimulationCommand& command) noexcept;

enum class SimulationCommandDispatchStatus
{
	Applied,
	Discarded,
	InvalidActor,
	InvalidDomain,
	AuthoritativeBackpressure,
	FrameBudgetExhausted,
	SequenceExhausted,
	SubmissionFailure,
	RetryDeferred,
	QueueChanged
};

struct SimulationCommandDispatchResult
{
	SimulationCommandDispatchStatus status =
		SimulationCommandDispatchStatus::SubmissionFailure;
	std::uint64_t tick = 0;
	std::uint64_t sequence = 0;
	bool submitted = false;

	explicit operator bool() const
	{
		return status == SimulationCommandDispatchStatus::Applied;
	}

	bool processed() const
	{
		return status == SimulationCommandDispatchStatus::Applied ||
			status == SimulationCommandDispatchStatus::Discarded;
	}

	// Applied commands and retained commands both represent accepted ingress.
	// A retained command may finish at the next safe frame; a processed discard
	// is deliberately not accepted.
	bool accepted() const
	{
		return status == SimulationCommandDispatchStatus::Applied ||
			(submitted && !processed());
	}
};

// Starts the one bounded authoritative budget shared by synchronous player
// commands and the end-of-frame package drain. The composition root calls this
// once before each application frame.
void BeginSimulationCommandFrameBudget(
	std::uint64_t frameSequence, std::size_t maximumCommands) noexcept;
std::size_t RemainingSimulationCommandFrameBudget(
	std::size_t requestedMaximum) noexcept;
void ConsumeSimulationCommandFrameBudget(std::size_t commands) noexcept;

// Submit at the completed simulation-tick boundary and synchronously execute
// only that sequence. Existing authoritative backlog and an exhausted frame
// budget are reported without mutating the queue.
SimulationCommandDispatchResult TryDispatchSimulationCommandNow(
	SimulationCommand command) noexcept;

// Reliable network ingress first attempts the established synchronous boundary.
// If an earlier authoritative producer or the per-frame budget prevents
// immediate execution, the validated packet is retained in sequence for the
// safe-frame drain rather than being silently dropped.
SimulationCommandDispatchResult TryDispatchNetworkSimulationCommand(
	SimulationCommand command) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkChangeStanceCommand(
	SOLDIERTYPE& soldier, std::uint8_t stance) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkSetFacingCommand(
	SOLDIERTYPE& soldier, std::uint8_t direction) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkActorPathCommand(
	SOLDIERTYPE& soldier,
	std::int32_t reportedGrid,
	std::int32_t destinationGrid,
	std::uint16_t movementState,
	std::uint16_t currentPathIndex,
	const std::uint16_t* path,
	std::uint16_t pathSize) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkActorFireCommand(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint32_t attackingWeapon) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkActorStopCommand(
	SOLDIERTYPE& soldier,
	std::int32_t reportedGrid,
	std::int16_t positionX,
	std::int16_t positionY,
	std::uint8_t direction,
	bool stop) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkTurnCommand(
	std::uint8_t nextTeam,
	bool enterCombat,
	bool endClientTurn) noexcept;

// AI and script actions have the same reliability requirement as received
// packets: a full frame or earlier authoritative work must defer, not erase,
// an action that the state machine already considers started.
SimulationCommandDispatchResult TryDispatchSystemSimulationCommand(
	SimulationCommand command) noexcept;

SimulationCommandDispatchResult TryDispatchSystemChangeStanceCommand(
	SOLDIERTYPE& soldier,
	std::uint8_t stance,
	TacticalEventPolicy eventPolicy =
		TacticalEventPolicy::Replicated) noexcept;

SimulationCommandDispatchResult TryDispatchSystemSetFacingCommand(
	SOLDIERTYPE& soldier,
	std::uint8_t direction,
	TacticalEventPolicy eventPolicy =
		TacticalEventPolicy::Replicated) noexcept;

SimulationCommandDispatchResult TryDispatchSystemMoveToGridCommand(
	SOLDIERTYPE& soldier,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemBeginSelectedFireWeaponCommand(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint8_t attackingHand,
	std::uint32_t attackingWeapon) noexcept;

SimulationCommandDispatchResult TryDispatchEndTurnCommandNow(
	std::uint8_t nextTeam,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

// Player/UI ingress captures a complete identity from the exact live actor
// reference. This is the production-facing seam: callers cannot accidentally
// combine one pool slot with another actor's incarnation. Value-only overloads
// below remain available for replay, network, tests, and legacy compatibility.
SimulationCommandDispatchResult TryDispatchChangeStanceCommandNow(
	SOLDIERTYPE& soldier,
	std::uint8_t stance,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchBeginFireWeaponCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchMoveToGridCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetFacingCommandNow(
	SOLDIERTYPE& soldier,
	std::uint8_t direction,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetStealthModeCommandNow(
	SOLDIERTYPE& soldier,
	bool enabled,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStopMovementCommandNow(
	SOLDIERTYPE& soldier,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCancelDragCommandNow(
	SOLDIERTYPE& soldier,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCycleWeaponModeCommandNow(
	SOLDIERTYPE& soldier,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCycleScopeModeCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchReloadWeaponCommandNow(
	SOLDIERTYPE& soldier,
	bool reloadEvenIfNotEmpty,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetWeaponReadyCommandNow(
	SOLDIERTYPE& soldier,
	std::uint8_t direction,
	bool ready,
	bool alternativeHold,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchTraverseObstacleCommandNow(
	SOLDIERTYPE& soldier,
	TacticalTraversalKind kind,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchActivateWorldObjectCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachWorldObjectCommandNow(
	SOLDIERTYPE& soldier,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStartConversationCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachConversationCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchEnterVehicleCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachVehicleCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchPickupWorldItemCommandNow(
	SOLDIERTYPE& soldier,
	TacticalWorldItemId item,
	std::int32_t grid,
	std::int8_t renderHeight,
	TacticalWorldItemPickupKind kind,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStealFromActorCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchExchangePositionsCommandNow(
	SOLDIERTYPE& soldier,
	SOLDIERTYPE& target,
	std::int32_t soldierGrid,
	std::int32_t targetGrid,
	std::int8_t level,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchChangeStanceCommandNow(
	std::uint16_t soldierId,
	std::uint8_t stance,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchBeginFireWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchMoveToGridCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetFacingCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint8_t direction,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetStealthModeCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	bool enabled,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStopMovementCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCancelDragCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCycleWeaponModeCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCycleScopeModeCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchReloadWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	bool reloadEvenIfNotEmpty,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetWeaponReadyCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint8_t direction,
	bool ready,
	bool alternativeHold,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchTraverseObstacleCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	TacticalTraversalKind kind,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchActivateWorldObjectCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachWorldObjectCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStartConversationCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachConversationCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchEnterVehicleCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t vehicleId,
	std::uint32_t vehicleUniqueSoldierId,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachVehicleCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t vehicleId,
	std::uint32_t vehicleUniqueSoldierId,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchPickupWorldItemCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	TacticalWorldItemId item,
	std::int32_t grid,
	std::int8_t renderHeight,
	TacticalWorldItemPickupKind kind,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStealFromActorCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchExchangePositionsCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::uint16_t targetId,
	std::uint32_t targetUniqueSoldierId,
	std::int32_t soldierGrid,
	std::int32_t targetGrid,
	std::int8_t level,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

// Delayed JA2 movement still stores its pending action on SOLDIERTYPE. These
// compatibility completion seams reconstruct the stable target identity and
// reject a despawned/reused target instead of acting on the new slot occupant.
bool TryCompletePendingConversationCommand(SOLDIERTYPE& soldier) noexcept;
bool TryCompletePendingVehicleCommand(SOLDIERTYPE& soldier) noexcept;
bool TryCompletePendingStealCommand(SOLDIERTYPE& soldier) noexcept;
SOLDIERTYPE* ResolveAndConsumePendingStealTarget(
	SOLDIERTYPE& soldier,
	std::int32_t targetGrid,
	std::int8_t targetLevel) noexcept;
bool TryValidatePendingWorldItemPickup(SOLDIERTYPE& soldier) noexcept;
bool TryConsumePendingWorldItemPickup(
	SOLDIERTYPE& soldier,
	std::int32_t itemIndex,
	std::int32_t grid,
	std::int8_t level) noexcept;

// Source-compatible wrappers for legacy callers. New production migrations use
// the structured Try variants so backpressure never triggers UI follow-up.
std::uint64_t DispatchEndTurnCommandNow(
	std::uint8_t nextTeam,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

std::uint64_t DispatchChangeStanceCommandNow(
	std::uint16_t soldierId,
	std::uint8_t stance,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

std::uint64_t DispatchBeginFireWeaponCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

std::uint64_t DispatchMoveToGridCommandNow(
	std::uint16_t soldierId,
	std::uint32_t uniqueSoldierId,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

CommandProcessingResult ExecuteSimulationCommandsThrough(std::uint64_t tick);
CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands);
CommandProcessingResult ExecuteSimulationCommandsThrough(
	std::uint64_t tick, std::size_t maximumCommands,
	SimulationCommandExecutionSink& sink);

#endif
