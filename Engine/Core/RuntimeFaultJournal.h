#ifndef ENGINE_CORE_RUNTIME_FAULT_JOURNAL_H
#define ENGINE_CORE_RUNTIME_FAULT_JOURNAL_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <vector>

enum class RuntimeFaultKind
{
	ServiceContract,
	CapabilityContract,
	DeferredTask,
	Bootstrap,
	Shutdown,
	Input,
	RuntimeUpdate,
	SimulationTick,
	Message,
	SaveState,
	LoadState
};

struct RuntimeFaultRecord
{
	std::uint64_t sequence = 0;
	RuntimeFaultKind kind = RuntimeFaultKind::Bootstrap;
	std::string packageId;
	std::string callback;
	std::uint64_t occurrence = 0;
};

struct RuntimeFaultSummary
{
	std::uint64_t observed = 0;
	std::uint64_t retained = 0;
	std::uint64_t evicted = 0;
	std::uint64_t storageFailures = 0;
	bool sequenceExhausted = false;
};

struct RuntimeFaultSnapshot
{
	RuntimeFaultSummary summary;
	std::vector<RuntimeFaultRecord> records;
};

// Bounded fault observation path. Recording never throws into gameplay and is
// independent from rate-limited logging, so repeated package failures remain
// countable and inspectable even when duplicate log lines are suppressed.
class RuntimeFaultJournal
{
public:
	explicit RuntimeFaultJournal(std::size_t capacity = 256) : capacity_(capacity) {}

	void record(RuntimeFaultKind kind, const std::string& packageId,
		const std::string& callback, std::uint64_t occurrence) noexcept
	{
		++summary_.observed;
		if (nextSequence_ == std::numeric_limits<std::uint64_t>::max())
		{
			summary_.sequenceExhausted = true;
			return;
		}
		const std::uint64_t sequence = nextSequence_++;
		if (capacity_ == 0) return;
		try
		{
			if (records_.size() == capacity_)
			{
				records_.pop_front();
				++summary_.evicted;
			}
			records_.push_back(RuntimeFaultRecord{
				sequence, kind, packageId, callback, occurrence});
			summary_.retained = records_.size();
		}
		catch (...)
		{
			++summary_.storageFailures;
		}
	}

	RuntimeFaultSnapshot snapshot() const
	{
		return RuntimeFaultSnapshot{
			summary_, std::vector<RuntimeFaultRecord>(records_.begin(), records_.end())};
	}

	const RuntimeFaultSummary& summary() const { return summary_; }
	std::size_t capacity() const { return capacity_; }
	std::size_t size() const { return records_.size(); }

	void clear()
	{
		records_.clear();
		summary_ = RuntimeFaultSummary{};
		nextSequence_ = 1;
	}

	static RuntimeFaultJournal& disabled()
	{
		static RuntimeFaultJournal journal(0);
		return journal;
	}

private:
	std::size_t capacity_;
	std::deque<RuntimeFaultRecord> records_;
	RuntimeFaultSummary summary_;
	std::uint64_t nextSequence_ = 1;
};

#endif
