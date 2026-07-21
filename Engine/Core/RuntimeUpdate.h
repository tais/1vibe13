#ifndef ENGINE_CORE_RUNTIME_UPDATE_H
#define ENGINE_CORE_RUNTIME_UPDATE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

struct RuntimeUpdateContext
{
	std::uint64_t frameSequence = 0;
	std::uint64_t startedAtMicroseconds = 0;
	std::uint64_t elapsedSincePreviousFrameMicroseconds = 0;
};

class RuntimeUpdateSink
{
public:
	virtual ~RuntimeUpdateSink() = default;
	virtual void updateRuntime(const RuntimeUpdateContext& context) = 0;
};

enum class RuntimeUpdateSinkRegistrationError
{
	None,
	Duplicate,
	NotFound,
	DispatchInProgress
};

struct RuntimeUpdateDispatchResult
{
	std::size_t delivered = 0;
	std::size_t callbackFailures = 0;
};

// Deterministic non-owning fan-out for per-frame runtime work. Subscriber
// mutation is rejected during a dispatch and one failure cannot starve later
// systems. Timing comes from the host's injected monotonic clock.
class RuntimeUpdateDispatcher
{
public:
	RuntimeUpdateSinkRegistrationError addSink(RuntimeUpdateSink& sink)
	{
		if (dispatching_) return RuntimeUpdateSinkRegistrationError::DispatchInProgress;
		if (std::find(sinks_.begin(), sinks_.end(), &sink) != sinks_.end())
			return RuntimeUpdateSinkRegistrationError::Duplicate;
		sinks_.push_back(&sink);
		return RuntimeUpdateSinkRegistrationError::None;
	}

	RuntimeUpdateSinkRegistrationError removeSink(RuntimeUpdateSink& sink)
	{
		if (dispatching_) return RuntimeUpdateSinkRegistrationError::DispatchInProgress;
		const auto found = std::find(sinks_.begin(), sinks_.end(), &sink);
		if (found == sinks_.end()) return RuntimeUpdateSinkRegistrationError::NotFound;
		sinks_.erase(found);
		return RuntimeUpdateSinkRegistrationError::None;
	}

	RuntimeUpdateDispatchResult dispatch(const RuntimeUpdateContext& context)
	{
		RuntimeUpdateDispatchResult result;
		DispatchGuard guard(dispatching_);
		for (RuntimeUpdateSink* sink : sinks_)
		{
			try
			{
				sink->updateRuntime(context);
				++result.delivered;
			}
			catch (...)
			{
				++result.callbackFailures;
			}
		}
		return result;
	}

	std::size_t sinkCount() const { return sinks_.size(); }

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

	std::vector<RuntimeUpdateSink*> sinks_;
	bool dispatching_ = false;
};

#endif
