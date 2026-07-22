#ifndef ENGINE_CORE_RUNTIME_MESSAGE_BUS_H
#define ENGINE_CORE_RUNTIME_MESSAGE_BUS_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <Engine/Core/Identifier.h>

struct RuntimeMessageRequest
{
	std::string topic;
	std::string source;
	std::vector<std::uint8_t> payload;
};

struct RuntimeMessage
{
	std::uint64_t sequence = 0;
	std::string topic;
	std::string source;
	std::vector<std::uint8_t> payload;
};

enum class RuntimeMessagePublishError
{
	None,
	InvalidTopic,
	InvalidSource,
	PayloadTooLarge,
	QueueFull,
	SequenceExhausted,
	AllocationFailure
};

struct RuntimeMessagePublishResult
{
	RuntimeMessagePublishError error = RuntimeMessagePublishError::None;
	std::uint64_t sequence = 0;

	explicit operator bool() const { return error == RuntimeMessagePublishError::None; }
};

class RuntimeMessageSink
{
public:
	virtual ~RuntimeMessageSink() = default;
	virtual void receiveMessage(const RuntimeMessage& message) = 0;
};

enum class RuntimeMessageSinkRegistrationError
{
	None,
	Duplicate,
	NotFound,
	DispatchInProgress
};

struct RuntimeMessageDispatchResult
{
	std::size_t messages = 0;
	std::size_t delivered = 0;
	std::size_t callbackFailures = 0;
	std::size_t queuedForNextDispatch = 0;
	bool operationInProgress = false;
};

// Deterministic, bounded, non-owning message fan-out for communication between
// packages and hosts without campaign headers. Dispatch snapshots the ready
// queue: messages published by a callback are deferred until the next frame,
// preventing reentrant or unbounded same-frame work.
class RuntimeMessageBus
{
public:
	explicit RuntimeMessageBus(
		std::size_t maxQueuedMessages = 1024,
		std::size_t maxPayloadBytes = 64 * 1024)
		: maxQueuedMessages_(maxQueuedMessages), maxPayloadBytes_(maxPayloadBytes) {}

	RuntimeMessagePublishResult publish(RuntimeMessageRequest request) noexcept
	{
		return publishRetained(request);
	}

	// Ownership-transfer variant for hosts that must retry a prepared payload.
	// Every failure leaves request byte-for-byte owned by the caller; success
	// moves its strings and payload into the queue without another allocation.
	RuntimeMessagePublishResult publishRetained(RuntimeMessageRequest& request) noexcept
	{
		if (!IsValidEngineIdentifier(request.topic))
			return RuntimeMessagePublishResult{RuntimeMessagePublishError::InvalidTopic, 0};
		if (!IsValidEngineIdentifier(request.source))
			return RuntimeMessagePublishResult{RuntimeMessagePublishError::InvalidSource, 0};
		if (request.payload.size() > maxPayloadBytes_)
			return RuntimeMessagePublishResult{RuntimeMessagePublishError::PayloadTooLarge, 0};
		if (queue_.size() >= maxQueuedMessages_)
			return RuntimeMessagePublishResult{RuntimeMessagePublishError::QueueFull, 0};
		if (nextSequence_ == std::numeric_limits<std::uint64_t>::max())
			return RuntimeMessagePublishResult{RuntimeMessagePublishError::SequenceExhausted, 0};
		const std::uint64_t sequence = nextSequence_;
		try
		{
			// Allocate the deque slot before consuming caller ownership. Standard
			// allocator move-assignment for strings/vectors below is non-throwing.
			queue_.emplace_back();
		}
		catch (...)
		{
			return RuntimeMessagePublishResult{RuntimeMessagePublishError::AllocationFailure, 0};
		}
		RuntimeMessage& queued = queue_.back();
		queued.sequence = sequence;
		queued.topic = std::move(request.topic);
		queued.source = std::move(request.source);
		queued.payload = std::move(request.payload);
		++nextSequence_;
		return RuntimeMessagePublishResult{RuntimeMessagePublishError::None, sequence};
	}

	RuntimeMessageSinkRegistrationError addSink(RuntimeMessageSink& sink)
	{
		if (dispatching_) return RuntimeMessageSinkRegistrationError::DispatchInProgress;
		if (std::find(sinks_.begin(), sinks_.end(), &sink) != sinks_.end())
			return RuntimeMessageSinkRegistrationError::Duplicate;
		sinks_.push_back(&sink);
		return RuntimeMessageSinkRegistrationError::None;
	}

	RuntimeMessageSinkRegistrationError removeSink(RuntimeMessageSink& sink)
	{
		if (dispatching_) return RuntimeMessageSinkRegistrationError::DispatchInProgress;
		const auto found = std::find(sinks_.begin(), sinks_.end(), &sink);
		if (found == sinks_.end()) return RuntimeMessageSinkRegistrationError::NotFound;
		sinks_.erase(found);
		return RuntimeMessageSinkRegistrationError::None;
	}

	RuntimeMessageDispatchResult dispatchPending()
	{
		RuntimeMessageDispatchResult result;
		if (dispatching_)
		{
			result.operationInProgress = true;
			result.queuedForNextDispatch = queue_.size();
			return result;
		}
		DispatchGuard guard(dispatching_);
		const std::size_t ready = queue_.size();
		for (std::size_t index = 0; index < ready; ++index)
		{
			RuntimeMessage message = std::move(queue_.front());
			queue_.pop_front();
			++result.messages;
			for (RuntimeMessageSink* sink : sinks_)
			{
				try
				{
					sink->receiveMessage(message);
					++result.delivered;
				}
				catch (...)
				{
					++result.callbackFailures;
				}
			}
		}
		result.queuedForNextDispatch = queue_.size();
		return result;
	}

	std::size_t queued() const { return queue_.size(); }
	std::size_t sinkCount() const { return sinks_.size(); }
	std::size_t maxQueuedMessages() const { return maxQueuedMessages_; }
	std::size_t maxPayloadBytes() const { return maxPayloadBytes_; }

	static RuntimeMessageBus& disabled()
	{
		static RuntimeMessageBus bus(0, 0);
		return bus;
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

	std::size_t maxQueuedMessages_;
	std::size_t maxPayloadBytes_;
	std::deque<RuntimeMessage> queue_;
	std::vector<RuntimeMessageSink*> sinks_;
	std::uint64_t nextSequence_ = 1;
	bool dispatching_ = false;
};

#endif
