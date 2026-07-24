#ifndef ENGINE_CORE_SIMULATION_TICK_H
#define ENGINE_CORE_SIMULATION_TICK_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

struct SimulationTickContext
{
	std::uint64_t sequence = 0;
	std::uint64_t stepMicroseconds = 0;
	std::uint64_t simulatedTimeMicroseconds = 0;
};

class SimulationTickSink
{
public:
	virtual ~SimulationTickSink() = default;
	virtual void simulate(const SimulationTickContext& tick) = 0;
};

enum class SimulationTickSinkRegistrationError
{
	None,
	Duplicate,
	NotFound,
	DispatchInProgress
};

struct SimulationTickDispatchResult
{
	std::uint64_t scheduled = 0;
	std::uint64_t executed = 0;
	std::uint64_t dropped = 0;
	std::size_t delivered = 0;
	std::size_t callbackFailures = 0;
	std::uint64_t accumulatedMicroseconds = 0;
	bool sequenceExhausted = false;
	bool operationInProgress = false;
};

// Bounded fixed-step scheduler driven by monotonic frame elapsed time. Long
// frames execute at most maxCatchUpTicks and explicitly report discarded work,
// preventing a hitch from becoming an unbounded spiral of catch-up updates.
class SimulationTickDispatcher
{
public:
	explicit SimulationTickDispatcher(
		std::uint64_t stepMicroseconds = 16667, std::size_t maxCatchUpTicks = 4)
		: stepMicroseconds_(stepMicroseconds == 0 ? 1 : stepMicroseconds),
		  maxCatchUpTicks_(maxCatchUpTicks) {}

	SimulationTickSinkRegistrationError addSink(SimulationTickSink& sink)
	{
		if (dispatching_) return SimulationTickSinkRegistrationError::DispatchInProgress;
		if (std::find(sinks_.begin(), sinks_.end(), &sink) != sinks_.end())
			return SimulationTickSinkRegistrationError::Duplicate;
		sinks_.push_back(&sink);
		return SimulationTickSinkRegistrationError::None;
	}

	// Inserts a system adapter at an explicit point in the deterministic stream.
	// This lets an application commit authoritative domain state before package
	// callbacks observe the same tick without assigning global sink priorities.
	SimulationTickSinkRegistrationError addSinkBefore(
		SimulationTickSink& sink, SimulationTickSink& before)
	{
		if (dispatching_) return SimulationTickSinkRegistrationError::DispatchInProgress;
		if (std::find(sinks_.begin(), sinks_.end(), &sink) != sinks_.end())
			return SimulationTickSinkRegistrationError::Duplicate;
		const auto found = std::find(sinks_.begin(), sinks_.end(), &before);
		if (found == sinks_.end()) return SimulationTickSinkRegistrationError::NotFound;
		sinks_.insert(found, &sink);
		return SimulationTickSinkRegistrationError::None;
	}

	SimulationTickSinkRegistrationError removeSink(SimulationTickSink& sink)
	{
		if (dispatching_) return SimulationTickSinkRegistrationError::DispatchInProgress;
		const auto found = std::find(sinks_.begin(), sinks_.end(), &sink);
		if (found == sinks_.end()) return SimulationTickSinkRegistrationError::NotFound;
		sinks_.erase(found);
		return SimulationTickSinkRegistrationError::None;
	}

	SimulationTickDispatchResult advance(std::uint64_t elapsedMicroseconds)
	{
		SimulationTickDispatchResult result;
		if (dispatching_)
		{
			result.operationInProgress = true;
			result.accumulatedMicroseconds = accumulator_;
			return result;
		}
		if (elapsedMicroseconds > std::numeric_limits<std::uint64_t>::max() - accumulator_)
			accumulator_ = std::numeric_limits<std::uint64_t>::max();
		else
			accumulator_ += elapsedMicroseconds;
		result.scheduled = accumulator_ / stepMicroseconds_;
		accumulator_ %= stepMicroseconds_;
		const std::uint64_t catchUpLimit = static_cast<std::uint64_t>(maxCatchUpTicks_);
		const std::uint64_t requested = std::min(result.scheduled, catchUpLimit);
		result.dropped = result.scheduled - requested;

		DispatchGuard guard(dispatching_);
		for (std::uint64_t index = 0; index < requested; ++index)
		{
			if (tickSequence_ == std::numeric_limits<std::uint64_t>::max())
			{
				result.sequenceExhausted = true;
				result.dropped += requested - index;
				break;
			}
			++tickSequence_;
			simulatedTime_ = saturatingAdd(simulatedTime_, stepMicroseconds_);
			const SimulationTickContext tick{
				tickSequence_, stepMicroseconds_, simulatedTime_};
			for (SimulationTickSink* sink : sinks_)
			{
				try
				{
					sink->simulate(tick);
					++result.delivered;
				}
				catch (...)
				{
					++result.callbackFailures;
				}
			}
			++result.executed;
		}

		const std::uint64_t skippedSequence = std::min(
			result.dropped,
			std::numeric_limits<std::uint64_t>::max() - tickSequence_);
		tickSequence_ += skippedSequence;
		const std::uint64_t skippedTime = result.dropped >
			std::numeric_limits<std::uint64_t>::max() / stepMicroseconds_
			? std::numeric_limits<std::uint64_t>::max()
			: result.dropped * stepMicroseconds_;
		simulatedTime_ = saturatingAdd(simulatedTime_, skippedTime);
		result.accumulatedMicroseconds = accumulator_;
		return result;
	}

	std::uint64_t stepMicroseconds() const { return stepMicroseconds_; }
	std::size_t maxCatchUpTicks() const { return maxCatchUpTicks_; }
	std::uint64_t completedTickSequence() const { return tickSequence_; }
	std::size_t sinkCount() const { return sinks_.size(); }

	void reset()
	{
		if (dispatching_) return;
		accumulator_ = 0;
		tickSequence_ = 0;
		simulatedTime_ = 0;
	}

	static SimulationTickDispatcher& disabled()
	{
		static SimulationTickDispatcher dispatcher(1, 0);
		return dispatcher;
	}

private:
	class DispatchGuard
	{
	public:
		explicit DispatchGuard(bool& dispatching) : dispatching_(dispatching)
		{
			dispatching_ = true;
		}
		~DispatchGuard() { dispatching_ = false; }
	private:
		bool& dispatching_;
	};

	static std::uint64_t saturatingAdd(std::uint64_t left, std::uint64_t right)
	{
		return right > std::numeric_limits<std::uint64_t>::max() - left
			? std::numeric_limits<std::uint64_t>::max() : left + right;
	}

	std::uint64_t stepMicroseconds_;
	std::size_t maxCatchUpTicks_;
	std::vector<SimulationTickSink*> sinks_;
	std::uint64_t accumulator_ = 0;
	std::uint64_t tickSequence_ = 0;
	std::uint64_t simulatedTime_ = 0;
	bool dispatching_ = false;
};

#endif
