#ifndef ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_EXECUTOR_H
#define ENGINE_ADAPTERS_JA2_SIMULATION_COMMAND_EXECUTOR_H

#include <cstdint>

#include <Engine/Adapters/JA2/SimulationCommand.h>
#include <Engine/Core/CommandProcessor.h>

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

#endif
