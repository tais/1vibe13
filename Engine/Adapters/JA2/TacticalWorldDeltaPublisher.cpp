#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

TacticalWorldDeltaPublishError MapTacticalWorldDeltaEncodeError(
	TacticalWorldDeltaEncodeResult result) noexcept
{
	switch (result)
	{
		case TacticalWorldDeltaEncodeResult::Success:
			return TacticalWorldDeltaPublishError::None;
		case TacticalWorldDeltaEncodeResult::Invalid:
			return TacticalWorldDeltaPublishError::InvalidDelta;
		case TacticalWorldDeltaEncodeResult::TooManyEvents:
			return TacticalWorldDeltaPublishError::TooManyEvents;
		case TacticalWorldDeltaEncodeResult::AllocationFailure:
			return TacticalWorldDeltaPublishError::CodecAllocationFailure;
	}
	return TacticalWorldDeltaPublishError::InvalidDelta;
}

TacticalWorldDeltaPublishError MapTacticalWorldDeltaMessageError(
	RuntimeMessagePublishError error) noexcept
{
	switch (error)
	{
		case RuntimeMessagePublishError::None:
			return TacticalWorldDeltaPublishError::None;
		case RuntimeMessagePublishError::InvalidTopic:
		case RuntimeMessagePublishError::InvalidSource:
			return TacticalWorldDeltaPublishError::InvalidMessageIdentifier;
		case RuntimeMessagePublishError::PayloadTooLarge:
			return TacticalWorldDeltaPublishError::PayloadTooLarge;
		case RuntimeMessagePublishError::QueueFull:
			return TacticalWorldDeltaPublishError::QueueFull;
		case RuntimeMessagePublishError::SequenceExhausted:
			return TacticalWorldDeltaPublishError::SequenceExhausted;
		case RuntimeMessagePublishError::AllocationFailure:
			return TacticalWorldDeltaPublishError::MessageAllocationFailure;
	}
	return TacticalWorldDeltaPublishError::InvalidMessageIdentifier;
}

TacticalWorldDeltaPublisher::TacticalWorldDeltaPublisher(
	RuntimeMessageBus& messages,
	TacticalWorldDeltaPublishLimits limits) noexcept
	: messages_(messages),
	  maximumEvents_(std::min(limits.maximumEvents,
		  MaximumTacticalWorldDeltaEvents)),
	  maximumPayloadBytes_(std::min(limits.maximumPayloadBytes,
		  messages.maxPayloadBytes()))
{
}

TacticalWorldDeltaPublishResult TacticalWorldDeltaPublisher::publish(
	const TacticalWorldDelta& delta) const noexcept
{
	std::vector<std::uint8_t> encoded;
	const TacticalWorldDeltaEncodeResult encodedResult =
		EncodeTacticalWorldDelta(delta, encoded, maximumEvents_);
	if (encodedResult != TacticalWorldDeltaEncodeResult::Success)
		return TacticalWorldDeltaPublishResult{
			MapTacticalWorldDeltaEncodeError(encodedResult), 0, 0};

	const std::size_t payloadBytes = encoded.size();
	if (payloadBytes > maximumPayloadBytes_)
		return TacticalWorldDeltaPublishResult{
			TacticalWorldDeltaPublishError::PayloadTooLarge, 0, payloadBytes};

	try
	{
		const RuntimeMessagePublishResult published = messages_.publish(
			RuntimeMessageRequest{
				std::string(TacticalWorldDeltaMessageTopic),
				std::string(TacticalWorldDeltaMessageSource),
				std::move(encoded)});
		return TacticalWorldDeltaPublishResult{
			MapTacticalWorldDeltaMessageError(published.error),
			published.sequence, payloadBytes};
	}
	catch (...)
	{
		return TacticalWorldDeltaPublishResult{
			TacticalWorldDeltaPublishError::MessageAllocationFailure,
			0, payloadBytes};
	}
}
