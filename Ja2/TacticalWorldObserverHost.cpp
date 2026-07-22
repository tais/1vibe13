#include "TacticalWorldObserverHost.h"

#include <limits>

#include "Overhead.h"
#include "TacticalWorldAdapter.h"

namespace
{
constexpr std::size_t Ja2TacticalMaximumEvents = TOTAL_SOLDIERS * 3 + 2;

void IncrementSaturated(std::uint64_t& value) noexcept
{
	if (value != std::numeric_limits<std::uint64_t>::max()) ++value;
}

void AddSaturated(std::uint64_t& value, std::size_t amount) noexcept
{
	const std::uint64_t bounded = amount > std::numeric_limits<std::uint64_t>::max()
		? std::numeric_limits<std::uint64_t>::max()
		: static_cast<std::uint64_t>(amount);
	if (bounded > std::numeric_limits<std::uint64_t>::max() - value)
		value = std::numeric_limits<std::uint64_t>::max();
	else
		value += bounded;
}

bool IsRetryablePublishFailure(TacticalWorldDeltaPublishError error) noexcept
{
	switch (error)
	{
		case TacticalWorldDeltaPublishError::CodecAllocationFailure:
		case TacticalWorldDeltaPublishError::QueueFull:
		case TacticalWorldDeltaPublishError::MessageAllocationFailure:
		case TacticalWorldDeltaPublishError::MessageBusChanged:
			return true;
		default:
			return false;
	}
}

class Ja2TacticalWorldObserverHost
{
public:
	Ja2TacticalWorldObserverHost() noexcept
		: observer_(GetJa2TacticalWorldAdapter(), TacticalWorldObserverLimits{
			TOTAL_SOLDIERS, Ja2TacticalMaximumEvents})
	{
	}

	TacticalWorldObserverService& service() noexcept
	{
		return observer_;
	}

	void updateAtSafeFrame(RuntimeMessageBus& messages) noexcept
	{
		IncrementSaturated(safeFrameUpdates_);
		if (synchronizeWorldLifecycle()) return;

		// A failed transient publication pins the observer's bounded latest
		// delta. Once encoded, the retained request is retried byte-for-byte, so
		// queue pressure neither re-encodes nor reallocates the payload.
		if (pendingDelta_)
		{
			publishDelta(*pendingDelta_, pendingDeltaSerial_, messages);
			return;
		}

		lastUpdate_ = observer_.update();
		if (lastUpdate_ == TacticalWorldObserverUpdateResult::PublishedBaseline)
		{
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::BaselineSuppressed;
			return;
		}
		if (lastUpdate_ == TacticalWorldObserverUpdateResult::Unchanged)
		{
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::EmptyDeltaSuppressed;
			return;
		}
		if (lastUpdate_ != TacticalWorldObserverUpdateResult::PublishedDelta)
		{
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::ObservationSuppressed;
			return;
		}

		const TacticalWorldPublicationView publication = observer_.latest();
		if (!publication || publication.status != TacticalWorldPublicationStatus::Delta)
		{
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::ObservationSuppressed;
			return;
		}
		if (publication.serial <= handledDeltaSerial_)
		{
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::DuplicateSerialSuppressed;
			return;
		}
		handledDeltaSerial_ = publication.serial;
		if (publication.delta->events.empty())
		{
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::EmptyDeltaSuppressed;
			return;
		}

		publishDelta(*publication.delta, publication.serial, messages);
	}

	Ja2TacticalWorldObserverDiagnostics diagnostics() const noexcept
	{
		const TacticalWorldPublicationView publication = observer_.latest();
		return Ja2TacticalWorldObserverDiagnostics{
			lastUpdate_, publication ? publication.serial : 0, safeFrameUpdates_,
			bridgeResult_, lastPublishError_, handledDeltaSerial_,
			pendingDeltaSerial_, publishedDeltaSerial_, messageSequence_, payloadBytes_,
			publishAttempts_, publishedMessages_, publicationFailures_,
			worldGeneration_, turnSerial_, worldTransitions_, observerResets_,
			discardedPendingDeltas_, preparationAttempts_,
			pendingBatch_.requests.size(), pendingBatch_.nextRequest,
			physicalMessagesPublished_, chunkedDeltasPrepared_,
			transferIdExhaustions_, pendingTransferId_, nextTransferId_};
	}

private:
	bool synchronizeWorldLifecycle() noexcept
	{
		const Ja2TacticalTurnIdentity identity =
			GetJa2TacticalWorldAdapter().liveTurnIdentity();
		worldGeneration_ = identity.worldGeneration;
		turnSerial_ = identity.serial;
		if (worldGeneration_ == observedWorldGeneration_) return false;

		const std::uint64_t previousGeneration = observedWorldGeneration_;
		observedWorldGeneration_ = worldGeneration_;
		IncrementSaturated(worldTransitions_);
		if (pendingDelta_)
		{
			clearPendingDelta();
			IncrementSaturated(discardedPendingDeltas_);
			lastPublishError_ = TacticalWorldDeltaPublishError::None;
			messageSequence_ = 0;
			payloadBytes_ = 0;
		}

		// A nonzero-to-nonzero transition retains the old accepted snapshot so
		// DiffTacticalWorldSnapshots can publish its existing reset event. An
		// observed unload instead makes the package service unavailable now.
		if (worldGeneration_ != 0 || previousGeneration == 0) return false;

		observer_.reset();
		handledDeltaSerial_ = 0;
		publishedDeltaSerial_ = 0;
		lastPublishError_ = TacticalWorldDeltaPublishError::None;
		messageSequence_ = 0;
		payloadBytes_ = 0;
		lastUpdate_ = TacticalWorldObserverUpdateResult::SourceUnavailable;
		bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::WorldUnavailableReset;
		IncrementSaturated(observerResets_);
		return true;
	}

	void publishDelta(
		const TacticalWorldDelta& delta,
		std::uint64_t serial,
		RuntimeMessageBus& messages) noexcept
	{
		// A host-lifetime ID never reuses observer serials after world resets. A
		// retained batch owns every exact request plus its next unsent cursor. Once
		// any request is queued, the rest stay pinned to that physical bus.
		if (!pendingDelta_)
		{
			if (nextTransferId_ == 0)
			{
				lastPublishError_ = TacticalWorldDeltaPublishError::InvalidTransfer;
				bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::PublishFailed;
				IncrementSaturated(publicationFailures_);
				IncrementSaturated(transferIdExhaustions_);
				return;
			}
			pendingDelta_ = &delta;
			pendingDeltaSerial_ = serial;
			pendingTransferId_ = nextTransferId_;
			nextTransferId_ = nextTransferId_ ==
					std::numeric_limits<std::uint64_t>::max()
				? 0 : nextTransferId_ + 1;
		}
		IncrementSaturated(publishAttempts_);
		if (pendingMessageBus_ && pendingMessageBus_ != &messages)
		{
			lastPublishError_ = TacticalWorldDeltaPublishError::MessageBusChanged;
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::PublishFailed;
			IncrementSaturated(publicationFailures_);
			return;
		}
		const TacticalWorldDeltaPublisher publisher(
			messages, TacticalWorldDeltaPublishLimits{
				Ja2TacticalMaximumEvents, messages.maxPayloadBytes()});
		if (!pendingBatch_)
		{
			IncrementSaturated(preparationAttempts_);
			const TacticalWorldDeltaPublishError prepared =
				publisher.prepareBatch(delta, pendingTransferId_, pendingBatch_);
			lastPublishError_ = prepared;
			messageSequence_ = 0;
			payloadBytes_ = pendingBatch_.totalPayloadBytes;
			if (prepared != TacticalWorldDeltaPublishError::None)
			{
				bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::PublishFailed;
				IncrementSaturated(publicationFailures_);
				if (!IsRetryablePublishFailure(prepared)) clearPendingDelta();
				return;
			}
			if (pendingBatch_.chunked)
				IncrementSaturated(chunkedDeltasPrepared_);
		}

		const TacticalWorldDeltaBatchPublishResult published =
			publisher.publishPreparedBatch(pendingBatch_);
		if (!pendingMessageBus_ && pendingBatch_.nextRequest != 0)
			pendingMessageBus_ = &messages;
		lastPublishError_ = published.error;
		if (published.lastSequence != 0)
			messageSequence_ = published.lastSequence;
		payloadBytes_ = published.payloadBytes;
		AddSaturated(physicalMessagesPublished_, published.messagesPublished);
		if (!published)
		{
			bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::PublishFailed;
			IncrementSaturated(publicationFailures_);
			if (!IsRetryablePublishFailure(published.error)) clearPendingDelta();
			return;
		}

		bridgeResult_ = Ja2TacticalWorldDeltaBridgeResult::Published;
		publishedDeltaSerial_ = serial;
		IncrementSaturated(publishedMessages_);
		clearPendingDelta();
	}

	void clearPendingDelta() noexcept
	{
		pendingDelta_ = nullptr;
		pendingDeltaSerial_ = 0;
		pendingTransferId_ = 0;
		pendingMessageBus_ = nullptr;
		pendingBatch_ = PreparedTacticalWorldDeltaBatch{};
	}

	TacticalWorldObserver observer_;
	TacticalWorldObserverUpdateResult lastUpdate_ =
		TacticalWorldObserverUpdateResult::SourceUnavailable;
	std::uint64_t safeFrameUpdates_ = 0;
	Ja2TacticalWorldDeltaBridgeResult bridgeResult_ =
		Ja2TacticalWorldDeltaBridgeResult::AwaitingDelta;
	TacticalWorldDeltaPublishError lastPublishError_ =
		TacticalWorldDeltaPublishError::None;
	std::uint64_t handledDeltaSerial_ = 0;
	const TacticalWorldDelta* pendingDelta_ = nullptr;
	std::uint64_t pendingDeltaSerial_ = 0;
	std::uint64_t pendingTransferId_ = 0;
	RuntimeMessageBus* pendingMessageBus_ = nullptr;
	std::uint64_t nextTransferId_ = 1;
	PreparedTacticalWorldDeltaBatch pendingBatch_;
	std::uint64_t publishedDeltaSerial_ = 0;
	std::uint64_t messageSequence_ = 0;
	std::size_t payloadBytes_ = 0;
	std::uint64_t publishAttempts_ = 0;
	std::uint64_t publishedMessages_ = 0;
	std::uint64_t publicationFailures_ = 0;
	std::uint64_t worldGeneration_ = 0;
	std::uint64_t turnSerial_ = 0;
	std::uint64_t observedWorldGeneration_ = 0;
	std::uint64_t worldTransitions_ = 0;
	std::uint64_t observerResets_ = 0;
	std::uint64_t discardedPendingDeltas_ = 0;
	std::uint64_t preparationAttempts_ = 0;
	std::uint64_t physicalMessagesPublished_ = 0;
	std::uint64_t chunkedDeltasPrepared_ = 0;
	std::uint64_t transferIdExhaustions_ = 0;
};

Ja2TacticalWorldObserverHost& GetObserverHost() noexcept
{
	static Ja2TacticalWorldObserverHost host;
	return host;
}
}

TacticalWorldObserverService& GetJa2TacticalWorldObserverService() noexcept
{
	return GetObserverHost().service();
}

void UpdateJa2TacticalWorldObserverAtSafeFrame(RuntimeMessageBus& messages) noexcept
{
	GetObserverHost().updateAtSafeFrame(messages);
}

Ja2TacticalWorldObserverDiagnostics GetJa2TacticalWorldObserverDiagnostics() noexcept
{
	return GetObserverHost().diagnostics();
}
