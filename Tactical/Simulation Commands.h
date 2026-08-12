#ifndef TACTICAL_SIMULATION_COMMANDS_H
#define TACTICAL_SIMULATION_COMMANDS_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Adapters/JA2/SimulationCommandExecutor.h>
#include <Engine/Core/CommandProcessor.h>

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
	InvalidTraversalOrigin,
	InvalidTraversalContinuation,
	InvalidTraversalPrecondition,
	InvalidObjectGrid,
	InvalidWorldObjectOperation,
	InvalidWorldObjectOrigin,
	InvalidWorldObjectContinuation,
	InvalidWorldObjectPrecondition,
	InvalidTargetActor,
	InvalidVehicleSeat,
	InvalidWorldItem,
	InvalidWorldItemRenderHeight,
	InvalidWorldItemPickupKind,
	InvalidBulkReloadMode,
	InvalidBulkReloadSquad,
	InvalidBulkReloadRoster,
	InvalidWeaponConfigurationResult,
	InvalidWeaponConfigurationCause,
	InvalidWeaponConfigurationPostApplyPolicy,
	InvalidWeaponConfigurationContinuation
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
	TacticalEntityId actor, std::uint8_t stance) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkSetFacingCommand(
	TacticalEntityId actor, std::uint8_t direction) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkActorPathCommand(
	TacticalEntityId actor,
	std::int32_t reportedGrid,
	std::int32_t destinationGrid,
	std::uint16_t movementState,
	std::uint16_t currentPathIndex,
	const std::uint16_t* path,
	std::uint16_t pathSize) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkActorFireCommand(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint32_t attackingWeapon) noexcept;

SimulationCommandDispatchResult TryDispatchNetworkActorStopCommand(
	TacticalEntityId actor,
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

// Path completion can run recursively while another command is being
// executed. Playback already contains the recorded continuation, so that
// recursive producer must not synthesize a second System command.
bool IsReplaySimulationCommandExecutionActive() noexcept;
bool HasPendingReplayPathTraversalCommand(
	TacticalEntityId actor) noexcept;
bool HasPendingReplayWorldObjectInteractionCommand(
	TacticalEntityId actor,
	TacticalWorldObjectOrigin origin) noexcept;

SimulationCommandDispatchResult TryDispatchSystemChangeStanceCommand(
	TacticalEntityId actor,
	std::uint8_t stance,
	TacticalEventPolicy eventPolicy =
		TacticalEventPolicy::Replicated) noexcept;

SimulationCommandDispatchResult TryDispatchSystemSetFacingCommand(
	TacticalEntityId actor,
	std::uint8_t direction,
	TacticalEventPolicy eventPolicy =
		TacticalEventPolicy::Replicated) noexcept;

SimulationCommandDispatchResult TryDispatchSystemMoveToGridCommand(
	TacticalEntityId actor,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemBeginSelectedFireWeaponCommand(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	std::uint8_t attackingHand,
	std::uint32_t attackingWeapon) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemApplyWeaponConfigurationCommand(
	TacticalEntityId actor,
	TacticalWeaponConfigurationResult result,
	TacticalWeaponConfigurationCause cause,
	TacticalWeaponConfigurationPostApplyPolicy postApplyPolicy,
	TacticalWeaponConfigurationContinuation continuation =
		TacticalWeaponConfigurationContinuation::None,
	TacticalEntityId target = {},
	std::int32_t targetGrid = TacticalNoTargetGrid,
	std::int8_t targetLevel = 0,
	std::uint32_t handItem = 0,
	std::uint32_t previousItem = 0,
	std::uint32_t changedItem = 0,
	std::uint32_t inventoryPosition = TacticalNoInventoryPosition,
	TacticalEventPolicy eventPolicy =
		TacticalEventPolicy::LocalOnly) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemAiTraverseObstacleCommandNow(
	TacticalEntityId actor,
	TacticalTraversalKind kind) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemPathTraverseObstacleCommandNow(
	TacticalEntityId actor,
	std::uint16_t movementAnimationState) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemAiWorldObjectInteractionCommandNow(
	TacticalEntityId actor,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	TacticalWorldObjectOperation operation) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemPathWorldObjectInteractionCommandNow(
	TacticalEntityId actor,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemPendingWorldObjectInteractionCommandNow(
	TacticalEntityId actor) noexcept;

SimulationCommandDispatchResult
TryDispatchSystemDialogueWorldObjectInteractionCommandNow(
	TacticalEntityId actor,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	std::int32_t actionGrid,
	std::uint16_t movementMode) noexcept;

SimulationCommandDispatchResult TryDispatchEndTurnCommandNow(
	std::uint8_t nextTeam,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

// Every producer captures one complete, generation-aware identity before
// crossing this boundary. The executor is the only command layer that resolves
// the value back to JA2's compatibility record.
SimulationCommandDispatchResult TryDispatchChangeStanceCommandNow(
	TacticalEntityId actor,
	std::uint8_t stance,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchBeginFireWeaponCommandNow(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchMoveToGridCommandNow(
	TacticalEntityId actor,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetFacingCommandNow(
	TacticalEntityId actor,
	std::uint8_t direction,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetStealthModeCommandNow(
	TacticalEntityId actor,
	bool enabled,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStopMovementCommandNow(
	TacticalEntityId actor,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCancelDragCommandNow(
	TacticalEntityId actor,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCycleWeaponModeCommandNow(
	TacticalEntityId actor,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchCycleScopeModeCommandNow(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchReloadWeaponCommandNow(
	TacticalEntityId actor,
	bool reloadEvenIfNotEmpty,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchBulkReloadWeaponsCommandNow(
	const TacticalEntityId* soldiers,
	std::uint16_t soldierCount,
	std::uint8_t squad,
	TacticalBulkReloadMode mode,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchSetWeaponReadyCommandNow(
	TacticalEntityId actor,
	std::uint8_t direction,
	bool ready,
	bool alternativeHold,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchTraverseObstacleCommandNow(
	TacticalEntityId actor,
	TacticalTraversalKind kind,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchActivateWorldObjectCommandNow(
	TacticalEntityId actor,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachWorldObjectCommandNow(
	TacticalEntityId actor,
	std::int32_t objectGrid,
	std::uint16_t structureId,
	std::uint8_t direction,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool reverse,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStartConversationCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachConversationCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchEnterVehicleCommandNow(
	TacticalEntityId actor,
	TacticalEntityId vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchApproachVehicleCommandNow(
	TacticalEntityId actor,
	TacticalEntityId vehicle,
	std::uint8_t direction,
	std::uint8_t seatIndex,
	std::int32_t destinationGrid,
	std::uint16_t movementMode,
	bool forceRestart,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchPickupWorldItemCommandNow(
	TacticalEntityId actor,
	TacticalWorldItemId item,
	std::int32_t grid,
	std::int8_t renderHeight,
	TacticalWorldItemPickupKind kind,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchStealFromActorCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

SimulationCommandDispatchResult TryDispatchExchangePositionsCommandNow(
	TacticalEntityId actor,
	TacticalEntityId target,
	std::int32_t soldierGrid,
	std::int32_t targetGrid,
	std::int8_t level,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer) noexcept;

// Sequence-only wrappers for compatibility code. New production migrations use
// the structured Try variants so backpressure never triggers UI follow-up.
std::uint64_t DispatchEndTurnCommandNow(
	std::uint8_t nextTeam,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

std::uint64_t DispatchChangeStanceCommandNow(
	TacticalEntityId actor,
	std::uint8_t stance,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

std::uint64_t DispatchBeginFireWeaponCommandNow(
	TacticalEntityId actor,
	std::int32_t targetGrid,
	std::int8_t targetLevel,
	std::int8_t targetCubeLevel,
	SimulationCommandSource source = SimulationCommandSource::LocalPlayer);

std::uint64_t DispatchMoveToGridCommandNow(
	TacticalEntityId actor,
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
