#include <Engine/Adapters/JA2/TacticalCommandResultPublisher.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

TacticalCommandResultPublishError MapTacticalCommandResultEncodeError(
	TacticalCommandResultEncodeError error) noexcept
{
	switch (error)
	{
		case TacticalCommandResultEncodeError::None:
			return TacticalCommandResultPublishError::None;
		case TacticalCommandResultEncodeError::Invalid:
			return TacticalCommandResultPublishError::InvalidResult;
		case TacticalCommandResultEncodeError::AllocationFailure:
			return TacticalCommandResultPublishError::CodecAllocationFailure;
	}
	return TacticalCommandResultPublishError::InvalidResult;
}

TacticalCommandResultPublishError MapTacticalCommandResultMessageError(
	RuntimeMessagePublishError error) noexcept
{
	switch (error)
	{
		case RuntimeMessagePublishError::None:
			return TacticalCommandResultPublishError::None;
		case RuntimeMessagePublishError::InvalidTopic:
		case RuntimeMessagePublishError::InvalidSource:
			return TacticalCommandResultPublishError::InvalidMessageIdentifier;
		case RuntimeMessagePublishError::PayloadTooLarge:
			return TacticalCommandResultPublishError::PayloadTooLarge;
		case RuntimeMessagePublishError::QueueFull:
			return TacticalCommandResultPublishError::QueueFull;
		case RuntimeMessagePublishError::SequenceExhausted:
			return TacticalCommandResultPublishError::SequenceExhausted;
		case RuntimeMessagePublishError::AllocationFailure:
			return TacticalCommandResultPublishError::MessageAllocationFailure;
	}
	return TacticalCommandResultPublishError::InvalidMessageIdentifier;
}

TacticalCommandResultPublisher::TacticalCommandResultPublisher(
	RuntimeMessageBus& messages, std::size_t maximumPayloadBytes) noexcept
	: messages_(messages),
	  maximumPayloadBytes_(std::min(
		  maximumPayloadBytes, messages.maxPayloadBytes()))
{
}

TacticalCommandResultPublishError TacticalCommandResultPublisher::prepare(
	const TacticalCommandResult& result,
	PreparedTacticalCommandResultMessage& output) const noexcept
{
	std::vector<std::uint8_t> payload;
	const TacticalCommandResultEncodeError encoded =
		EncodeTacticalCommandResult(result, payload);
	if (encoded != TacticalCommandResultEncodeError::None)
		return MapTacticalCommandResultEncodeError(encoded);
	if (payload.size() > maximumPayloadBytes_)
		return TacticalCommandResultPublishError::PayloadTooLarge;
	try
	{
		PreparedTacticalCommandResultMessage prepared;
		prepared.payloadBytes = payload.size();
		prepared.request = RuntimeMessageRequest{
			std::string(TacticalCommandResultMessageTopic),
			std::string(TacticalCommandResultMessageSource),
			std::move(payload)};
		output = std::move(prepared);
		return TacticalCommandResultPublishError::None;
	}
	catch (...)
	{
		return TacticalCommandResultPublishError::MessageAllocationFailure;
	}
}

TacticalCommandResultPublishResult
TacticalCommandResultPublisher::publishPrepared(
	PreparedTacticalCommandResultMessage& prepared) const noexcept
{
	const std::size_t payloadBytes = prepared.request.payload.size();
	if (!prepared || prepared.payloadBytes != payloadBytes ||
		prepared.request.topic != TacticalCommandResultMessageTopic ||
		prepared.request.source != TacticalCommandResultMessageSource)
		return TacticalCommandResultPublishResult{
			TacticalCommandResultPublishError::InvalidResult, 0, payloadBytes};
	if (payloadBytes > maximumPayloadBytes_)
		return TacticalCommandResultPublishResult{
			TacticalCommandResultPublishError::PayloadTooLarge, 0, payloadBytes};
	const RuntimeMessagePublishResult published =
		messages_.publishRetained(prepared.request);
	return TacticalCommandResultPublishResult{
		MapTacticalCommandResultMessageError(published.error),
		published.sequence, payloadBytes};
}

TacticalCommandResultPublishResult TacticalCommandResultPublisher::publish(
	const TacticalCommandResult& result) const noexcept
{
	PreparedTacticalCommandResultMessage prepared;
	const TacticalCommandResultPublishError preparedResult =
		prepare(result, prepared);
	if (preparedResult != TacticalCommandResultPublishError::None)
		return TacticalCommandResultPublishResult{preparedResult};
	return publishPrepared(prepared);
}
