#ifndef ENGINE_CORE_COMMAND_PROCESSOR_H
#define ENGINE_CORE_COMMAND_PROCESSOR_H

#include <cstddef>
#include <cstdint>
#include <utility>

#include <Engine/Core/DeterministicCommandQueue.h>

enum class CommandDisposition
{
	Applied,
	Retry,
	Discard
};

enum class CommandProcessStatus
{
	Completed,
	Blocked,
	QueueChanged
};

struct CommandProcessingResult
{
	CommandProcessStatus status = CommandProcessStatus::Completed;
	std::size_t scheduled = 0;
	std::size_t applied = 0;
	std::size_t discarded = 0;
	std::uint64_t blockedTick = 0;
	std::uint64_t blockedSequence = 0;

	explicit operator bool() const { return status == CommandProcessStatus::Completed; }
};

namespace engine_command_detail
{
template<typename Observer, typename Command>
void NotifyObserver(
	Observer& observer, const Command& command, std::uint64_t tick,
	std::uint64_t sequence, CommandDisposition disposition) noexcept
{
	try
	{
		observer(command, tick, sequence, disposition);
	}
	catch (...)
	{
		// Diagnostics and replay capture must not alter authoritative delivery.
	}
}
}

// Process only the commands that were ready when this pass began. Applied and
// explicitly discarded commands are acknowledged after the handler returns.
// Retry leaves that command and every later command queued. If a handler
// throws, the exception propagates while the failing and later commands remain
// queued; already applied commands stay acknowledged exactly once. The
// observer runs only for an acknowledged or retry-blocked attempt, and its
// exceptions are isolated from authoritative delivery.
template<typename Command, typename Handler, typename Observer>
CommandProcessingResult ProcessCommandsThrough(
	DeterministicCommandQueue<Command>& queue,
	std::uint64_t tick,
	Handler&& handler,
	Observer&& observer)
{
	const auto ready = queue.snapshotThrough(tick);
	CommandProcessingResult result;
	result.scheduled = ready.size();
	for (const auto& entry : ready)
	{
		const CommandDisposition disposition =
			handler(entry.command, entry.tick, entry.sequence);
		if (disposition == CommandDisposition::Retry)
		{
			engine_command_detail::NotifyObserver(
				observer, entry.command, entry.tick, entry.sequence, disposition);
			result.status = CommandProcessStatus::Blocked;
			result.blockedTick = entry.tick;
			result.blockedSequence = entry.sequence;
			return result;
		}
		if (!queue.acknowledge(entry.sequence))
		{
			result.status = CommandProcessStatus::QueueChanged;
			result.blockedTick = entry.tick;
			result.blockedSequence = entry.sequence;
			return result;
		}
		engine_command_detail::NotifyObserver(
			observer, entry.command, entry.tick, entry.sequence, disposition);
		if (disposition == CommandDisposition::Applied) ++result.applied;
		else ++result.discarded;
	}
	return result;
}

template<typename Command, typename Handler>
CommandProcessingResult ProcessCommandsThrough(
	DeterministicCommandQueue<Command>& queue,
	std::uint64_t tick,
	Handler&& handler)
{
	return ProcessCommandsThrough(
		queue, tick, std::forward<Handler>(handler),
		[](const Command&, std::uint64_t, std::uint64_t, CommandDisposition) {});
}

#endif
