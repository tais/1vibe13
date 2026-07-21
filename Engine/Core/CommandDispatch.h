#ifndef ENGINE_CORE_COMMAND_DISPATCH_H
#define ENGINE_CORE_COMMAND_DISPATCH_H

#include <cstddef>
#include <cstdint>
#include <utility>

#include <Engine/Core/DeterministicCommandQueue.h>

// Draining and delivery belong to the engine layer; concrete handlers remain
// in adapters until their legacy global dependencies are extracted.
template<typename Command, typename Handler>
std::size_t DispatchCommandsThrough(
	DeterministicCommandQueue<Command>& queue,
	std::uint64_t tick,
	Handler&& handler)
{
	auto ready = queue.drainThrough(tick);
	for (auto& entry : ready)
	{
		std::forward<Handler>(handler)(entry.command, entry.tick, entry.sequence);
	}
	return ready.size();
}

#endif
