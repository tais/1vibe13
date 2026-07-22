#ifndef ENGINE_CORE_COMMAND_PROCESSOR_H
#define ENGINE_CORE_COMMAND_PROCESSOR_H

#include <cstddef>
#include <cstdint>
#include <type_traits>
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
	QueueChanged,
	BudgetExhausted
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

template<typename Command, typename Handler, typename Observer>
CommandProcessingResult ProcessSnapshot(
	DeterministicCommandQueue<Command>& queue,
	const std::vector<ScheduledCommand<Command>>& ready,
	bool moreReady,
	Handler&& handler,
	Observer&& observer)
{
	CommandProcessingResult result;
	result.scheduled = ready.size();
	for (const auto& entry : ready)
	{
		const CommandDisposition disposition =
			handler(entry.command, entry.tick, entry.sequence);
		if (disposition == CommandDisposition::Retry)
		{
			NotifyObserver(
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
		NotifyObserver(
			observer, entry.command, entry.tick, entry.sequence, disposition);
		if (disposition == CommandDisposition::Applied) ++result.applied;
		else ++result.discarded;
	}
	if (moreReady) result.status = CommandProcessStatus::BudgetExhausted;
	return result;
}
}

// Process only the commands that were ready when this pass began. Applied and
// explicitly discarded commands are acknowledged after the handler returns.
// Retry leaves that command and every later command queued. If a handler
// throws, the exception propagates while the failing and later commands remain
// queued; already applied commands stay acknowledged exactly once. The
// observer runs only for an acknowledged or retry-blocked attempt, and its
// exceptions are isolated from authoritative delivery.
template<
	typename Command, typename Handler, typename Observer,
	std::enable_if_t<std::is_invocable_r<
		CommandDisposition, Handler&, const Command&,
		std::uint64_t, std::uint64_t>::value, int> = 0>
CommandProcessingResult ProcessCommandsThrough(
	DeterministicCommandQueue<Command>& queue,
	std::uint64_t tick,
	Handler&& handler,
	Observer&& observer)
{
	const auto ready = queue.snapshotThrough(tick);
	return engine_command_detail::ProcessSnapshot(
		queue, ready, false, std::forward<Handler>(handler),
		std::forward<Observer>(observer));
}

// Bounded counterpart to the compatibility overload above. At most maximum
// commands are copied and invoked. A completely handled prefix reports
// BudgetExhausted when more commands from the original ready set remain.
template<typename Command, typename Handler, typename Observer>
CommandProcessingResult ProcessCommandsThrough(
	DeterministicCommandQueue<Command>& queue,
	std::uint64_t tick,
	std::size_t maximum,
	Handler&& handler,
	Observer&& observer)
{
	bool moreReady = false;
	const auto ready = queue.snapshotThrough(tick, maximum, moreReady);
	return engine_command_detail::ProcessSnapshot(
		queue, ready, moreReady, std::forward<Handler>(handler),
		std::forward<Observer>(observer));
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

template<typename Command, typename Handler>
CommandProcessingResult ProcessCommandsThrough(
	DeterministicCommandQueue<Command>& queue,
	std::uint64_t tick,
	std::size_t maximum,
	Handler&& handler)
{
	return ProcessCommandsThrough(
		queue, tick, maximum, std::forward<Handler>(handler),
		[](const Command&, std::uint64_t, std::uint64_t, CommandDisposition) {});
}

#endif
