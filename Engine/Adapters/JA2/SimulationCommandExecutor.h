#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_EXECUTOR_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_EXECUTOR_H

#include <cstdint>

#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Core/CommandProcessor.h>

// Best-effort observation after an execution attempt. The runtime updates its
// own command journal before invoking this sink, and contains callback
// failures so diagnostics cannot alter authoritative delivery.
class SimulationCommandExecutionSink
{
public:
	virtual ~SimulationCommandExecutionSink() = default;
	virtual void commandProcessed(
		const SimulationCommand& command,
		std::uint64_t tick,
		std::uint64_t sequence,
		CommandDisposition disposition) = 0;
};

// Host-owned execution boundary for the pointer-free tactical command stream.
// The queue and replay layers provide deterministic tick/sequence metadata;
// concrete executors decide how a valid command affects their world.
class SimulationCommandExecutor
{
public:
	virtual ~SimulationCommandExecutor() = default;

	virtual CommandDisposition execute(
		const SimulationCommand& command,
		std::uint64_t tick,
		std::uint64_t sequence) = 0;
};

// Safe default for runtimes that stage or inspect commands before selecting a
// world implementation. Retry retains the first command and every later one;
// an unbound host cannot accidentally acknowledge authoritative work.
class NullSimulationCommandExecutor final : public SimulationCommandExecutor
{
public:
	static NullSimulationCommandExecutor& instance() noexcept
	{
		static NullSimulationCommandExecutor executor;
		return executor;
	}

	CommandDisposition execute(
		const SimulationCommand&,
		std::uint64_t,
		std::uint64_t) noexcept override
	{
		return CommandDisposition::Retry;
	}

private:
	NullSimulationCommandExecutor() = default;
};

#endif
