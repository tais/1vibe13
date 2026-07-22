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
	PreparedTacticalWorldDeltaMessage prepared;
	std::size_t encodedPayloadBytes = 0;
	const TacticalWorldDeltaPublishError preparedResult =
		prepareImpl(delta, prepared, &encodedPayloadBytes);
	if (preparedResult != TacticalWorldDeltaPublishError::None)
		return TacticalWorldDeltaPublishResult{
			preparedResult, 0,
			preparedResult == TacticalWorldDeltaPublishError::PayloadTooLarge
				? encodedPayloadBytes : 0};
	return publishPrepared(prepared);
}

TacticalWorldDeltaPublishError TacticalWorldDeltaPublisher::prepare(
	const TacticalWorldDelta& delta,
	PreparedTacticalWorldDeltaMessage& output) const noexcept
{
	return prepareImpl(delta, output, nullptr);
}

TacticalWorldDeltaPublishError TacticalWorldDeltaPublisher::prepareImpl(
	const TacticalWorldDelta& delta,
	PreparedTacticalWorldDeltaMessage& output,
	std::size_t* encodedPayloadBytes) const noexcept
{
	std::vector<std::uint8_t> payload;
	const TacticalWorldDeltaEncodeResult encodedResult =
		EncodeTacticalWorldDelta(delta, payload, maximumEvents_);
	if (encodedResult != TacticalWorldDeltaEncodeResult::Success)
		return MapTacticalWorldDeltaEncodeError(encodedResult);

	const std::size_t payloadBytes = payload.size();
	if (encodedPayloadBytes) *encodedPayloadBytes = payloadBytes;
	if (payloadBytes > maximumPayloadBytes_)
		return TacticalWorldDeltaPublishError::PayloadTooLarge;

	try
	{
		PreparedTacticalWorldDeltaMessage prepared;
		prepared.eventCount = delta.events.size();
		prepared.payloadBytes = payloadBytes;
		prepared.request = RuntimeMessageRequest{
				std::string(TacticalWorldDeltaMessageTopic),
				std::string(TacticalWorldDeltaMessageSource),
				std::move(payload)};
		output = std::move(prepared);
		return TacticalWorldDeltaPublishError::None;
	}
	catch (...)
	{
		return TacticalWorldDeltaPublishError::MessageAllocationFailure;
	}
}

TacticalWorldDeltaPublishResult TacticalWorldDeltaPublisher::publishPrepared(
	PreparedTacticalWorldDeltaMessage& prepared) const noexcept
{
	const std::size_t payloadBytes = prepared.request.payload.size();
	if (!prepared || prepared.payloadBytes != payloadBytes ||
		prepared.eventCount > maximumEvents_ ||
		prepared.request.topic != TacticalWorldDeltaMessageTopic ||
		prepared.request.source != TacticalWorldDeltaMessageSource)
		return TacticalWorldDeltaPublishResult{
			TacticalWorldDeltaPublishError::InvalidDelta, 0, payloadBytes};
	if (payloadBytes > maximumPayloadBytes_)
		return TacticalWorldDeltaPublishResult{
			TacticalWorldDeltaPublishError::PayloadTooLarge, 0, payloadBytes};
	const RuntimeMessagePublishResult published =
		messages_.publishRetained(prepared.request);
	return TacticalWorldDeltaPublishResult{
		MapTacticalWorldDeltaMessageError(published.error),
		published.sequence, payloadBytes};
}
