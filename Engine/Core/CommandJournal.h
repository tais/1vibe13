#ifndef ENGINE_CORE_COMMAND_JOURNAL_H
#define ENGINE_CORE_COMMAND_JOURNAL_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>

#include <Engine/Core/CommandProcessor.h>

enum class CommandJournalStatus : std::uint8_t
{
	Queued,
	Applied,
	Discarded,
	Blocked
};

template<typename Command>
struct CommandJournalRecord
{
	std::uint64_t tick;
	std::uint64_t sequence;
	CommandJournalStatus status;
	Command command;
};

// Bounded, best-effort observability for deterministic command submission and
// delivery. Journal allocation failure must never alter simulation behavior;
// missing records are reported through droppedCount() instead.
template<typename Command>
class CommandJournal
{
public:
	using Record = CommandJournalRecord<Command>;

	explicit CommandJournal(std::size_t capacity = 4096) : capacity_(capacity) {}

	void recordSubmission(
		std::uint64_t tick, std::uint64_t sequence, Command command) noexcept
	{
		if (capacity_ == 0)
		{
			++dropped_;
			return;
		}
		try
		{
			if (records_.size() == capacity_)
			{
				records_.pop_front();
				++dropped_;
			}
			records_.push_back(Record{
				tick, sequence, CommandJournalStatus::Queued, std::move(command)});
		}
		catch (...)
		{
			++dropped_;
		}
	}

	void recordDisposition(
		std::uint64_t sequence, CommandDisposition disposition) noexcept
	{
		for (auto record = records_.rbegin(); record != records_.rend(); ++record)
		{
			if (record->sequence != sequence) continue;
			switch (disposition)
			{
				case CommandDisposition::Applied:
					record->status = CommandJournalStatus::Applied;
					break;
				case CommandDisposition::Discard:
					record->status = CommandJournalStatus::Discarded;
					break;
				case CommandDisposition::Retry:
					record->status = CommandJournalStatus::Blocked;
					break;
			}
			return;
		}
	}

	std::vector<Record> snapshot() const
	{
		return std::vector<Record>(records_.begin(), records_.end());
	}

	void clear()
	{
		records_.clear();
		dropped_ = 0;
	}

	std::size_t size() const { return records_.size(); }
	std::size_t capacity() const { return capacity_; }
	std::uint64_t droppedCount() const { return dropped_; }

private:
	std::size_t capacity_;
	std::deque<Record> records_;
	std::uint64_t dropped_ = 0;
};

#endif
