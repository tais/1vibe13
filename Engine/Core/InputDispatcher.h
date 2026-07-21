#ifndef ENGINE_CORE_INPUT_DISPATCHER_H
#define ENGINE_CORE_INPUT_DISPATCHER_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Core/InputSource.h>

class InputEventSink
{
public:
	virtual ~InputEventSink() = default;
	virtual void receiveInput(const EngineInputEvent& event) = 0;
};

enum class InputSinkRegistrationError
{
	None,
	Duplicate,
	NotFound,
	DispatchInProgress
};

struct InputDispatchResult
{
	std::size_t polled = 0;
	std::size_t delivered = 0;
	std::size_t callbackFailures = 0;
	std::uint64_t sourceDrops = 0;
	bool limitReached = false;
};

// Bounded fan-out from an injected input source to deterministic, non-owning
// subscribers. A callback failure cannot prevent later subscribers or events
// from running, and registration is frozen for the duration of dispatch.
class InputDispatcher
{
public:
	explicit InputDispatcher(InputSource& source, std::size_t maxEventsPerDispatch = 256)
		: source_(source), maxEventsPerDispatch_(maxEventsPerDispatch) {}

	InputSinkRegistrationError addSink(InputEventSink& sink)
	{
		if (dispatching_) return InputSinkRegistrationError::DispatchInProgress;
		if (std::find(sinks_.begin(), sinks_.end(), &sink) != sinks_.end())
			return InputSinkRegistrationError::Duplicate;
		sinks_.push_back(&sink);
		return InputSinkRegistrationError::None;
	}

	InputSinkRegistrationError removeSink(InputEventSink& sink)
	{
		if (dispatching_) return InputSinkRegistrationError::DispatchInProgress;
		const auto found = std::find(sinks_.begin(), sinks_.end(), &sink);
		if (found == sinks_.end()) return InputSinkRegistrationError::NotFound;
		sinks_.erase(found);
		return InputSinkRegistrationError::None;
	}

	InputDispatchResult dispatchPending()
	{
		InputDispatchResult result;
		DispatchGuard guard(dispatching_);
		EngineInputEvent event;
		while (result.polled < maxEventsPerDispatch_ && source_.poll(event))
		{
			++result.polled;
			result.sourceDrops += event.droppedBefore;
			for (InputEventSink* sink : sinks_)
			{
				try
				{
					sink->receiveInput(event);
					++result.delivered;
				}
				catch (...)
				{
					++result.callbackFailures;
				}
			}
		}
		result.limitReached = result.polled == maxEventsPerDispatch_;
		return result;
	}

	std::size_t sinkCount() const { return sinks_.size(); }
	std::size_t maxEventsPerDispatch() const { return maxEventsPerDispatch_; }

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

	InputSource& source_;
	std::vector<InputEventSink*> sinks_;
	std::size_t maxEventsPerDispatch_;
	bool dispatching_ = false;
};

#endif
