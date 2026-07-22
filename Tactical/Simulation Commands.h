#ifndef TACTICAL_SIMULATION_COMMANDS_H
#define TACTICAL_SIMULATION_COMMANDS_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Core/CommandProcessor.h>

// Authoritative completion seam for hosts that must correlate package/network
// requests with actual simulation disposition. This is independent of the
// best-effort command journal and is invoked only after queue acknowledgement
// (or for an explicit retry observation).
class SimulationCommandExecutionSink
{
public:
	virtual ~SimulationCommandExecutionSink() = default;
	virtual void commandProcessed(
		const SimulationCommand& command,
		std::uint64_t tick,
		std::uint64_t sequence,
		CommandDisposition disposition) noexcept = 0;
};

// Installs the application-owned completion observer used by every execution
// entry point, including synchronous compatibility dispatch. Binding the same
// sink again is harmless; replacing a live binding is rejected. The sink must
// outlive all command execution.
bool BindSimulationCommandExecutionSink(
	SimulationCommandExecutionSink& sink) noexcept;

enum class SimulationCommandDomainError
{
	None,
	ValuelessCommand,
	InvalidSource,
	InvalidTeam,
	InvalidActor,
	InvalidStance,
	InvalidTargetGrid,
	InvalidTargetLevel,
	InvalidTargetCubeLevel,
	InvalidDestinationGrid,
	InvalidMovementMode,
	InvalidMoveOrigin,
	InvalidPendingActionPolicy
};

// Complete value-domain validation shared by package admission and every
// execution entry point. Live actor/context checks remain executor policy.
SimulationCommandDomainError ValidateSimulationCommandDomain(
	const SimulationCommand& command) noexcept;

enum class SimulationCommandDispatchStatus
{
	Applied,
	Discarded,
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

SimulationCommandDispatchResult TryDispatchEndTurnCommandNow(
	std::uint8_t nextTeam,
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
