#ifndef JA2_TACTICAL_WORLD_OBSERVER_HOST_H
#define JA2_TACTICAL_WORLD_OBSERVER_HOST_H

#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/TacticalWorldObserver.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>

enum class Ja2TacticalWorldDeltaBridgeResult
{
	AwaitingDelta,
	ObservationSuppressed,
	BaselineSuppressed,
	EmptyDeltaSuppressed,
	DuplicateSerialSuppressed,
	Published,
	PublishFailed,
	WorldUnavailableReset
};

struct Ja2TacticalWorldObserverDiagnostics
{
	TacticalWorldObserverUpdateResult lastUpdate =
		TacticalWorldObserverUpdateResult::SourceUnavailable;
	std::uint64_t publicationSerial = 0;
	std::uint64_t safeFrameUpdates = 0;
	Ja2TacticalWorldDeltaBridgeResult bridgeResult =
		Ja2TacticalWorldDeltaBridgeResult::AwaitingDelta;
	TacticalWorldDeltaPublishError lastPublishError =
		TacticalWorldDeltaPublishError::None;
	std::uint64_t handledDeltaSerial = 0;
	std::uint64_t pendingDeltaSerial = 0;
	std::uint64_t publishedDeltaSerial = 0;
	std::uint64_t messageSequence = 0;
	std::size_t payloadBytes = 0;
	std::uint64_t publishAttempts = 0;
	std::uint64_t publishedMessages = 0;
	std::uint64_t publicationFailures = 0;
	std::uint64_t worldGeneration = 0;
	std::uint64_t turnSerial = 0;
	std::uint64_t worldTransitions = 0;
	std::uint64_t observerResets = 0;
	std::uint64_t discardedPendingDeltas = 0;
	std::uint64_t preparationAttempts = 0;
	std::size_t pendingBatchMessages = 0;
	std::size_t pendingBatchCursor = 0;
	std::uint64_t physicalMessagesPublished = 0;
	std::uint64_t chunkedDeltasPrepared = 0;
	std::uint64_t transferIdExhaustions = 0;
	std::uint64_t pendingTransferId = 0;
	std::uint64_t nextTransferId = 1;
};

// Application-owned read-only service registered with the production engine
// composition root before packages activate.
TacticalWorldObserverService& GetJa2TacticalWorldObserverService() noexcept;

// The production GameLoop invokes this exactly once after all legacy work at
// its completed-frame boundary. A new non-empty delta is queued for the next
// frame's message dispatch. Transient queue/allocation failures retain one
// bounded observer delta and backpressure observation until a later safe-frame
// retry. World unload invalidates the observer publication and any retained
// delta before it can be retried against another world; suppression and
// failure paths never log.
void UpdateJa2TacticalWorldObserverAtSafeFrame(RuntimeMessageBus& messages) noexcept;

Ja2TacticalWorldObserverDiagnostics GetJa2TacticalWorldObserverDiagnostics() noexcept;

#endif
