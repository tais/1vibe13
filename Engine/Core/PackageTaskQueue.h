#ifndef ENGINE_CORE_PACKAGE_TASK_QUEUE_H
#define ENGINE_CORE_PACKAGE_TASK_QUEUE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

enum class PackageTaskScheduleError
{
	None,
	InvalidOwner,
	InvalidTask,
	CapacityReached,
	SequenceExhausted,
	AllocationFailure
};

struct PackageTaskScheduleResult
{
	PackageTaskScheduleError error = PackageTaskScheduleError::None;
	std::uint64_t sequence = 0;

	explicit operator bool() const
	{
		return error == PackageTaskScheduleError::None && sequence != 0;
	}
};

struct PackageTaskRecord
{
	std::uint64_t sequence = 0;
	std::string packageId;
};

struct PackageTaskSummary
{
	std::uint64_t scheduled = 0;
	std::uint64_t executed = 0;
	std::uint64_t failed = 0;
	std::uint64_t cancelled = 0;
	std::size_t queued = 0;
};

struct PackageTaskQueueSnapshot
{
	PackageTaskSummary summary;
	std::vector<PackageTaskRecord> queued;
};

struct PackageTaskDrainResult
{
	std::size_t attempted = 0;
	std::size_t executed = 0;
	std::size_t failed = 0;
	std::size_t deferred = 0;
	bool operationInProgress = false;
};

// Bounded main-thread work staged by package code. A drain only observes the
// tasks that existed at its start, so callbacks cannot create an unbounded
// same-frame loop. Package teardown cancels work before captured state dies.
class PackageTaskQueue
{
public:
	using Task = std::function<void()>;

	explicit PackageTaskQueue(
		std::size_t maximumQueued = 1024, std::size_t maximumPerDrain = 64)
		: maximumQueued_(maximumQueued), maximumPerDrain_(maximumPerDrain) {}

	PackageTaskScheduleResult schedule(
		const std::string& packageId, Task task) noexcept
	{
		if (!IsValidEngineIdentifier(packageId))
			return PackageTaskScheduleResult{PackageTaskScheduleError::InvalidOwner, 0};
		if (!task)
			return PackageTaskScheduleResult{PackageTaskScheduleError::InvalidTask, 0};
		if (tasks_.size() >= maximumQueued_)
			return PackageTaskScheduleResult{PackageTaskScheduleError::CapacityReached, 0};
		if (nextSequence_ == std::numeric_limits<std::uint64_t>::max())
			return PackageTaskScheduleResult{PackageTaskScheduleError::SequenceExhausted, 0};
		const std::uint64_t sequence = nextSequence_;
		try
		{
			tasks_.push_back(Entry{PackageTaskRecord{sequence, packageId}, std::move(task)});
		}
		catch (...)
		{
			return PackageTaskScheduleResult{PackageTaskScheduleError::AllocationFailure, 0};
		}
		++nextSequence_;
		++summary_.scheduled;
		summary_.queued = tasks_.size();
		return PackageTaskScheduleResult{PackageTaskScheduleError::None, sequence};
	}

	template<typename FailureSink>
	PackageTaskDrainResult drain(FailureSink&& failureSink) noexcept
	{
		PackageTaskDrainResult result;
		if (draining_)
		{
			result.deferred = tasks_.size();
			result.operationInProgress = true;
			return result;
		}
		DrainGuard guard(draining_);
		const std::size_t ready = std::min(tasks_.size(), maximumPerDrain_);
		for (std::size_t index = 0; index < ready; ++index)
		{
			Entry entry = std::move(tasks_.front());
			tasks_.pop_front();
			++result.attempted;
			try
			{
				entry.task();
				++result.executed;
				++summary_.executed;
			}
			catch (...)
			{
				++result.failed;
				++summary_.failed;
				try { failureSink(entry.record, summary_.failed); } catch (...) {}
			}
		}
		result.deferred = tasks_.size();
		summary_.queued = tasks_.size();
		return result;
	}

	PackageTaskDrainResult drain() noexcept
	{
		return drain([](const PackageTaskRecord&, std::uint64_t) {});
	}

	std::size_t removePackage(const std::string& packageId) noexcept
	{
		if (draining_) return 0;
		std::size_t removed = 0;
		for (auto task = tasks_.begin(); task != tasks_.end();)
		{
			if (task->record.packageId != packageId)
			{
				++task;
				continue;
			}
			task = tasks_.erase(task);
			++removed;
		}
		summary_.cancelled += removed;
		summary_.queued = tasks_.size();
		return removed;
	}

	PackageTaskQueueSnapshot snapshot() const
	{
		PackageTaskQueueSnapshot result;
		result.summary = summary_;
		result.queued.reserve(tasks_.size());
		for (const Entry& task : tasks_) result.queued.push_back(task.record);
		return result;
	}

	std::size_t size() const { return tasks_.size(); }
	std::size_t maximumQueued() const { return maximumQueued_; }
	std::size_t maximumPerDrain() const { return maximumPerDrain_; }

	static PackageTaskQueue& disabled()
	{
		static PackageTaskQueue queue(0, 0);
		return queue;
	}

private:
	class DrainGuard
	{
	public:
		explicit DrainGuard(bool& draining) : draining_(draining)
		{
			draining_ = true;
		}
		~DrainGuard() { draining_ = false; }
	private:
		bool& draining_;
	};

	struct Entry
	{
		PackageTaskRecord record;
		Task task;
	};

	std::size_t maximumQueued_;
	std::size_t maximumPerDrain_;
	std::deque<Entry> tasks_;
	PackageTaskSummary summary_;
	std::uint64_t nextSequence_ = 1;
	bool draining_ = false;
};

#endif
