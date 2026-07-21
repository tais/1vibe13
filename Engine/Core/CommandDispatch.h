#ifndef ENGINE_CORE_COMMAND_DISPATCH_H
#define ENGINE_CORE_COMMAND_DISPATCH_H

#include <cstddef>
#include <cstdint>
#include <Engine/Core/CommandProcessor.h>

// Draining and delivery belong to the engine layer; concrete handlers remain
// in adapters until their legacy global dependencies are extracted.
template<typename Command, typename Handler>
std::size_t DispatchCommandsThrough(
	DeterministicCommandQueue<Command>& queue,
	std::uint64_t tick,
	Handler&& handler)
{
	const CommandProcessingResult result = ProcessCommandsThrough(
		queue, tick,
		[&handler](const Command& command, std::uint64_t commandTick,
			std::uint64_t sequence)
		{
			handler(command, commandTick, sequence);
			return CommandDisposition::Applied;
		});
	return result.applied;
}

#endif
