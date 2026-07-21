#ifndef ENGINE_CORE_COMMAND_STREAM_H
#define ENGINE_CORE_COMMAND_STREAM_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <Engine/Core/CommandJournal.h>
#include <Engine/Core/DeterministicCommandQueue.h>

// Game-agnostic command ingress. It keeps authoritative delivery and
// best-effort observability together so hosts cannot enqueue a command without
// assigning the same tick and sequence to its journal record.
template<typename Command>
class CommandStream
{
public:
	using Entry = ScheduledCommand<Command>;

	explicit CommandStream(std::size_t journalCapacity = 4096)
		: journal_(journalCapacity) {}

	DeterministicCommandQueue<Command>& queue() { return queue_; }
	const DeterministicCommandQueue<Command>& queue() const { return queue_; }
	CommandJournal<Command>& journal() { return journal_; }
	const CommandJournal<Command>& journal() const { return journal_; }

	std::uint64_t submit(std::uint64_t tick, Command command)
	{
		Command recorded = command;
		const std::uint64_t sequence = queue_.enqueue(tick, std::move(command));
		journal_.recordSubmission(tick, sequence, std::move(recorded));
		return sequence;
	}

	bool submitRecorded(
		std::uint64_t tick, std::uint64_t sequence, Command command)
	{
		Command recorded = command;
		if (!queue_.enqueueRecorded(tick, sequence, std::move(command))) return false;
		journal_.recordSubmission(tick, sequence, std::move(recorded));
		return true;
	}

	// The queue validates and commits the complete batch first. Journal writes
	// are deliberately best-effort and cannot roll back authoritative delivery.
	bool stageRecordedBatch(const std::vector<Entry>& batch)
	{
		if (!queue_.enqueueRecordedBatch(batch)) return false;
		for (const Entry& entry : batch)
			journal_.recordSubmission(entry.tick, entry.sequence, entry.command);
		return true;
	}

private:
	DeterministicCommandQueue<Command> queue_;
	CommandJournal<Command> journal_;
};

#endif
