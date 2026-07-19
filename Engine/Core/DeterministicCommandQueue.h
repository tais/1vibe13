#ifndef ENGINE_CORE_DETERMINISTIC_COMMAND_QUEUE_H
#define ENGINE_CORE_DETERMINISTIC_COMMAND_QUEUE_H

#include <algorithm>
#include <cstdint>
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
		const std::uint64_t sequence = nextSequence_++;
		usedSequences_.insert(sequence);
		entries_.push_back(Entry{tick, sequence, std::move(command)});
		return sequence;
	}

	bool enqueueRecorded(std::uint64_t tick, std::uint64_t sequence, Command command)
	{
		if (!usedSequences_.insert(sequence).second) return false;
		entries_.push_back(Entry{tick, sequence, std::move(command)});
		if (sequence >= nextSequence_) nextSequence_ = sequence + 1;
		return true;
	}

	std::vector<Entry> drainThrough(std::uint64_t tick)
	{
		std::stable_sort(entries_.begin(), entries_.end(), less);
		const auto end = std::upper_bound(entries_.begin(), entries_.end(), tick,
			[](std::uint64_t value, const Entry& entry) { return value < entry.tick; });
		std::vector<Entry> ready;
		ready.reserve(static_cast<std::size_t>(end - entries_.begin()));
		std::move(entries_.begin(), end, std::back_inserter(ready));
		entries_.erase(entries_.begin(), end);
		return ready;
	}

	std::size_t size() const { return entries_.size(); }
	bool empty() const { return entries_.empty(); }

private:
	static bool less(const Entry& left, const Entry& right)
	{
		if (left.tick != right.tick) return left.tick < right.tick;
		return left.sequence < right.sequence;
	}

	std::vector<Entry> entries_;
	std::unordered_set<std::uint64_t> usedSequences_;
	std::uint64_t nextSequence_ = 0;
};

#endif
