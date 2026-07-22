#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_PUBLISHER_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_PUBLISHER_H

#include <cstddef>
#include <cstdint>
#include <limits>

#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaTransport.h>
#include <Engine/Core/RuntimeMessageBus.h>

struct TacticalWorldDeltaPublishLimits
{
	std::size_t maximumEvents = MaximumTacticalWorldDeltaEvents;
	std::size_t maximumPayloadBytes = std::numeric_limits<std::size_t>::max();
	std::size_t maximumTransferBytes = MaximumTacticalWorldDeltaTransferBytes;
	std::size_t maximumChunks = MaximumTacticalWorldDeltaTransferChunks;
};

enum class TacticalWorldDeltaPublishError
{
	None = 0,
	InvalidDelta = 1,
	TooManyEvents = 2,
	CodecAllocationFailure = 3,
	PayloadTooLarge = 4,
	QueueFull = 5,
	SequenceExhausted = 6,
	MessageAllocationFailure = 7,
	InvalidMessageIdentifier = 8,
	TransferTooLarge = 9,
	TooManyChunks = 10,
	InvalidTransfer = 11,
	MessageBusChanged = 12
};

static_assert(static_cast<int>(TacticalWorldDeltaPublishError::None) == 0 &&
	static_cast<int>(TacticalWorldDeltaPublishError::InvalidDelta) == 1 &&
	static_cast<int>(TacticalWorldDeltaPublishError::TooManyEvents) == 2 &&
	static_cast<int>(TacticalWorldDeltaPublishError::CodecAllocationFailure) == 3 &&
	static_cast<int>(TacticalWorldDeltaPublishError::PayloadTooLarge) == 4 &&
	static_cast<int>(TacticalWorldDeltaPublishError::QueueFull) == 5 &&
	static_cast<int>(TacticalWorldDeltaPublishError::SequenceExhausted) == 6 &&
	static_cast<int>(TacticalWorldDeltaPublishError::MessageAllocationFailure) == 7 &&
	static_cast<int>(TacticalWorldDeltaPublishError::InvalidMessageIdentifier) == 8,
	"tactical delta publisher error values are a stable SDK contract");

struct TacticalWorldDeltaPublishResult
{
	TacticalWorldDeltaPublishError error = TacticalWorldDeltaPublishError::None;
	std::uint64_t sequence = 0;
	std::size_t payloadBytes = 0;

	explicit operator bool() const noexcept
	{
		return error == TacticalWorldDeltaPublishError::None;
	}
};

// Encoded once and retained by the caller until the runtime bus accepts it.
// publishPrepared consumes request ownership only on success.
struct PreparedTacticalWorldDeltaMessage
{
	RuntimeMessageRequest request;
	std::size_t eventCount = 0;
	std::size_t payloadBytes = 0;

	explicit operator bool() const noexcept
	{
		return payloadBytes != 0 && !request.payload.empty();
	}
};

struct PreparedTacticalWorldDeltaBatch
{
	std::uint64_t transferId = 0;
	std::size_t eventCount = 0;
	std::size_t totalPayloadBytes = 0;
	std::uint32_t checksum = 0;
	bool chunked = false;
	std::vector<RuntimeMessageRequest> requests;
	std::size_t nextRequest = 0;

	explicit operator bool() const noexcept
	{
		return transferId != 0 && totalPayloadBytes != 0 && !requests.empty() &&
			nextRequest <= requests.size();
	}

	bool complete() const noexcept
	{
		return static_cast<bool>(*this) && nextRequest == requests.size();
	}

	std::size_t remaining() const noexcept
	{
		return nextRequest <= requests.size() ? requests.size() - nextRequest : 0;
	}
};

struct TacticalWorldDeltaBatchPublishResult
{
	TacticalWorldDeltaPublishError error = TacticalWorldDeltaPublishError::None;
	std::uint64_t firstSequence = 0;
	std::uint64_t lastSequence = 0;
	std::size_t payloadBytes = 0;
	std::size_t messagesPublished = 0;
	std::size_t totalMessages = 0;
	bool complete = false;

	explicit operator bool() const noexcept
	{
		return error == TacticalWorldDeltaPublishError::None && complete;
	}
};

// Stable translations are public so hosts that defer or wrap publication can
// preserve the same package-facing failure contract.
TacticalWorldDeltaPublishError MapTacticalWorldDeltaEncodeError(
	TacticalWorldDeltaEncodeResult result) noexcept;
TacticalWorldDeltaPublishError MapTacticalWorldDeltaMessageError(
	RuntimeMessagePublishError error) noexcept;
TacticalWorldDeltaPublishError MapTacticalWorldDeltaChunkEncodeError(
	TacticalWorldDeltaChunkEncodeError error) noexcept;

// Bounded bridge from the JA2 tactical adapter to package-visible runtime
// messages. The caller may lower codec and payload limits. The effective
// payload limit can never exceed RuntimeMessageBus::maxPayloadBytes().
class TacticalWorldDeltaPublisher
{
public:
	explicit TacticalWorldDeltaPublisher(
		RuntimeMessageBus& messages,
		TacticalWorldDeltaPublishLimits limits = {}) noexcept;

	std::size_t maximumEvents() const noexcept { return maximumEvents_; }
	std::size_t maximumPayloadBytes() const noexcept { return maximumPayloadBytes_; }
	std::size_t maximumTransferBytes() const noexcept { return maximumTransferBytes_; }
	std::size_t maximumChunks() const noexcept { return maximumChunks_; }

	// The original one-message path is transactional. Any failure leaves the
	// bus queue and sequence unchanged.
	TacticalWorldDeltaPublishError prepare(
		const TacticalWorldDelta& delta,
		PreparedTacticalWorldDeltaMessage& output) const noexcept;
	TacticalWorldDeltaPublishResult publishPrepared(
		PreparedTacticalWorldDeltaMessage& prepared) const noexcept;
	TacticalWorldDeltaPublishError prepareBatch(
		const TacticalWorldDelta& delta,
		std::uint64_t transferId,
		PreparedTacticalWorldDeltaBatch& output) const noexcept;
	// Batch preparation is transactional. Publication advances its retained
	// cursor only after each successful enqueue; QueueFull leaves already queued
	// chunks intact and the first unsent request ready for a later frame.
	TacticalWorldDeltaBatchPublishResult publishPreparedBatch(
		PreparedTacticalWorldDeltaBatch& prepared) const noexcept;
	TacticalWorldDeltaPublishResult publish(
		const TacticalWorldDelta& delta) const noexcept;

private:
	TacticalWorldDeltaPublishError prepareImpl(
		const TacticalWorldDelta& delta,
		PreparedTacticalWorldDeltaMessage& output,
		std::size_t* encodedPayloadBytes) const noexcept;

	RuntimeMessageBus& messages_;
	std::size_t maximumEvents_;
	std::size_t maximumPayloadBytes_;
	std::size_t maximumTransferBytes_;
	std::size_t maximumChunks_;
};

#endif
