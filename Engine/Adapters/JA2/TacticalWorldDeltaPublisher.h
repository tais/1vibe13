#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_PUBLISHER_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_PUBLISHER_H

#include <cstddef>
#include <cstdint>
#include <limits>

#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Core/RuntimeMessageBus.h>

// Packages opt into this topic to receive TacticalWorldDeltaWireVersion
// payloads and decode them with DecodeTacticalWorldDelta. The stable source
// names the JA2 host adapter rather than coupling records to a package ID.
inline constexpr char TacticalWorldDeltaMessageTopic[] = "ja2.tactical-world.delta";
inline constexpr char TacticalWorldDeltaMessageSource[] = "ja2.runtime-adapter";

struct TacticalWorldDeltaPublishLimits
{
	std::size_t maximumEvents = MaximumTacticalWorldDeltaEvents;
	std::size_t maximumPayloadBytes = std::numeric_limits<std::size_t>::max();
};

enum class TacticalWorldDeltaPublishError
{
	None,
	InvalidDelta,
	TooManyEvents,
	CodecAllocationFailure,
	PayloadTooLarge,
	QueueFull,
	SequenceExhausted,
	MessageAllocationFailure,
	InvalidMessageIdentifier
};

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

// Stable translations are public so hosts that defer or wrap publication can
// preserve the same package-facing failure contract.
TacticalWorldDeltaPublishError MapTacticalWorldDeltaEncodeError(
	TacticalWorldDeltaEncodeResult result) noexcept;
TacticalWorldDeltaPublishError MapTacticalWorldDeltaMessageError(
	RuntimeMessagePublishError error) noexcept;

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

	// Validation, encoding, and queueing are transactional. Any failure leaves
	// the bus queue and sequence unchanged.
	TacticalWorldDeltaPublishResult publish(
		const TacticalWorldDelta& delta) const noexcept;

private:
	RuntimeMessageBus& messages_;
	std::size_t maximumEvents_;
	std::size_t maximumPayloadBytes_;
};

#endif
