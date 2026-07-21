#ifndef ENGINE_CORE_DETERMINISTIC_COMMAND_QUEUE_H
#define ENGINE_CORE_DETERMINISTIC_COMMAND_QUEUE_H

#include <algorithm>
#include <cstdint>
#include <deque>
#include <iterator>
#include <unordered_set>
#include <utility>
#include <vector>

template <typename Command>
struct ScheduledCommand
{
	std::uint64_t tick;
	std::uint64_t sequence;
	Command command;
};

// Commands are ordered solely by simulation tick and insertion sequence. Wall
// clock time, render cadence, and container iteration order cannot affect them.
template <typename Command>
class DeterministicCommandQueue
{
public:
	using Entry = ScheduledCommand<Command>;

	std::uint64_t enqueue(std::uint64_t tick, Command command)
	{
		std::uint64_t sequence = nextSequence_;
		while (usedSequences_.find(sequence) != usedSequences_.end()) ++sequence;
		const auto inserted = usedSequences_.insert(sequence);
		if (!inserted.second) return sequence;
		try
		{
			entries_.push_back(Entry{tick, sequence, std::move(command)});
		}
		catch (...)
		{
			usedSequences_.erase(inserted.first);
			throw;
		}
		nextSequence_ = sequence + 1;
		ordered_ = false;
		return sequence;
	}

	bool enqueueRecorded(std::uint64_t tick, std::uint64_t sequence, Command command)
	{
		const auto inserted = usedSequences_.insert(sequence);
		if (!inserted.second) return false;
		try
		{
			entries_.push_back(Entry{tick, sequence, std::move(command)});
		}
		catch (...)
		{
			usedSequences_.erase(inserted.first);
			throw;
		}
		if (sequence >= nextSequence_) nextSequence_ = sequence + 1;
		ordered_ = false;
		return true;
	}

	// Validate and append a complete replay batch without partially mutating the
	// authoritative queue. Existing and within-batch sequence conflicts reject
	// the whole batch; allocation failures also leave this queue untouched.
	bool enqueueRecordedBatch(const std::vector<Entry>& batch)
	{
		DeterministicCommandQueue staged(*this);
		for (const Entry& entry : batch)
		{
			if (!staged.enqueueRecorded(entry.tick, entry.sequence, entry.command))
				return false;
		}
		entries_.swap(staged.entries_);
		usedSequences_.swap(staged.usedSequences_);
		std::swap(nextSequence_, staged.nextSequence_);
		std::swap(ordered_, staged.ordered_);
		return true;
	}

	std::vector<Entry> drainThrough(std::uint64_t tick)
	{
		ensureOrdered();
		const auto end = std::upper_bound(entries_.begin(), entries_.end(), tick,
			[](std::uint64_t value, const Entry& entry) { return value < entry.tick; });
		std::vector<Entry> ready;
		ready.reserve(static_cast<std::size_t>(end - entries_.begin()));
		std::move(entries_.begin(), end, std::back_inserter(ready));
		entries_.erase(entries_.begin(), end);
		return ready;
	}

	// Copy a stable, bounded delivery view without removing commands. A
	// processor acknowledges each sequence only after its handler succeeds, so
	// exceptions or retries cannot lose the failed command or later work.
	std::vector<Entry> snapshotThrough(std::uint64_t tick)
	{
		ensureOrdered();
		const auto end = std::upper_bound(entries_.begin(), entries_.end(), tick,
			[](std::uint64_t value, const Entry& entry) { return value < entry.tick; });
		return std::vector<Entry>(entries_.begin(), end);
	}

	bool acknowledge(std::uint64_t sequence)
	{
		if (!entries_.empty() && entries_.front().sequence == sequence)
		{
			entries_.pop_front();
			return true;
		}
		const auto entry = std::find_if(entries_.begin(), entries_.end(),
			[sequence](const Entry& candidate) { return candidate.sequence == sequence; });
		if (entry == entries_.end()) return false;
		entries_.erase(entry);
		return true;
	}

	std::size_t size() const { return entries_.size(); }
	bool empty() const { return entries_.empty(); }

private:
	void ensureOrdered()
	{
		if (ordered_) return;
		std::stable_sort(entries_.begin(), entries_.end(), less);
		ordered_ = true;
	}

	static bool less(const Entry& left, const Entry& right)
	{
		if (left.tick != right.tick) return left.tick < right.tick;
		return left.sequence < right.sequence;
	}

	// Normal ready acknowledgements remove from the front in constant time. The
	// fallback supports a handler that enqueues and explicitly re-snapshots an
	// earlier tick before returning.
	std::deque<Entry> entries_;
	std::unordered_set<std::uint64_t> usedSequences_;
	std::uint64_t nextSequence_ = 0;
	bool ordered_ = true;
};

#endif
