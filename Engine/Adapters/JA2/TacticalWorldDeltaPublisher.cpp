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

TacticalWorldDeltaPublishError MapTacticalWorldDeltaChunkEncodeError(
	TacticalWorldDeltaChunkEncodeError error) noexcept
{
	switch (error)
	{
		case TacticalWorldDeltaChunkEncodeError::None:
			return TacticalWorldDeltaPublishError::None;
		case TacticalWorldDeltaChunkEncodeError::InvalidTransfer:
			return TacticalWorldDeltaPublishError::InvalidTransfer;
		case TacticalWorldDeltaChunkEncodeError::PayloadLimitTooSmall:
			return TacticalWorldDeltaPublishError::PayloadTooLarge;
		case TacticalWorldDeltaChunkEncodeError::TransferTooLarge:
			return TacticalWorldDeltaPublishError::TransferTooLarge;
		case TacticalWorldDeltaChunkEncodeError::TooManyChunks:
			return TacticalWorldDeltaPublishError::TooManyChunks;
		case TacticalWorldDeltaChunkEncodeError::AllocationFailure:
			return TacticalWorldDeltaPublishError::MessageAllocationFailure;
	}
	return TacticalWorldDeltaPublishError::InvalidTransfer;
}

TacticalWorldDeltaPublisher::TacticalWorldDeltaPublisher(
	RuntimeMessageBus& messages,
	TacticalWorldDeltaPublishLimits limits) noexcept
	: messages_(messages),
	  maximumEvents_(std::min(limits.maximumEvents,
		  MaximumTacticalWorldDeltaEvents)),
	  maximumPayloadBytes_(std::min(limits.maximumPayloadBytes,
		  messages.maxPayloadBytes())),
	  maximumTransferBytes_(std::min(limits.maximumTransferBytes,
		  MaximumTacticalWorldDeltaTransferBytes)),
	  maximumChunks_(std::min(limits.maximumChunks,
		  MaximumTacticalWorldDeltaTransferChunks))
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

TacticalWorldDeltaPublishError TacticalWorldDeltaPublisher::prepareBatch(
	const TacticalWorldDelta& delta,
	std::uint64_t transferId,
	PreparedTacticalWorldDeltaBatch& output) const noexcept
{
	if (transferId == 0) return TacticalWorldDeltaPublishError::InvalidTransfer;
	std::vector<std::uint8_t> encoded;
	const TacticalWorldDeltaEncodeResult encodedResult =
		EncodeTacticalWorldDelta(delta, encoded, maximumEvents_);
	if (encodedResult != TacticalWorldDeltaEncodeResult::Success)
		return MapTacticalWorldDeltaEncodeError(encodedResult);
	if (encoded.size() > maximumTransferBytes_)
		return TacticalWorldDeltaPublishError::TransferTooLarge;

	try
	{
		PreparedTacticalWorldDeltaBatch prepared;
		prepared.transferId = transferId;
		prepared.eventCount = delta.events.size();
		prepared.totalPayloadBytes = encoded.size();
		if (encoded.size() <= maximumPayloadBytes_)
		{
			prepared.requests.reserve(1);
			prepared.requests.push_back(RuntimeMessageRequest{
				std::string(TacticalWorldDeltaMessageTopic),
				std::string(TacticalWorldDeltaMessageSource),
				std::move(encoded)});
		}
		else
		{
			EncodedTacticalWorldDeltaChunks chunks;
			const TacticalWorldDeltaChunkEncodeError chunkResult =
				EncodeTacticalWorldDeltaChunks(
					encoded, transferId, maximumPayloadBytes_, chunks,
					maximumTransferBytes_, maximumChunks_);
			if (chunkResult != TacticalWorldDeltaChunkEncodeError::None)
				return MapTacticalWorldDeltaChunkEncodeError(chunkResult);
			prepared.chunked = true;
			prepared.checksum = chunks.checksum;
			prepared.requests.reserve(chunks.payloads.size());
			for (std::vector<std::uint8_t>& payload : chunks.payloads)
				prepared.requests.push_back(RuntimeMessageRequest{
					std::string(TacticalWorldDeltaChunkMessageTopic),
					std::string(TacticalWorldDeltaMessageSource),
					std::move(payload)});
		}
		output = std::move(prepared);
		return TacticalWorldDeltaPublishError::None;
	}
	catch (...)
	{
		return TacticalWorldDeltaPublishError::MessageAllocationFailure;
	}
}

TacticalWorldDeltaBatchPublishResult
TacticalWorldDeltaPublisher::publishPreparedBatch(
	PreparedTacticalWorldDeltaBatch& prepared) const noexcept
{
	TacticalWorldDeltaBatchPublishResult result;
	result.payloadBytes = prepared.totalPayloadBytes;
	result.totalMessages = prepared.requests.size();
	if (prepared.totalPayloadBytes > maximumTransferBytes_)
	{
		result.error = TacticalWorldDeltaPublishError::TransferTooLarge;
		return result;
	}
	if (!prepared || prepared.eventCount > maximumEvents_ ||
		(prepared.chunked && prepared.requests.size() > maximumChunks_))
	{
		result.error = TacticalWorldDeltaPublishError::InvalidDelta;
		return result;
	}

	const char* expectedTopic = prepared.chunked
		? TacticalWorldDeltaChunkMessageTopic
		: TacticalWorldDeltaMessageTopic;
	if ((!prepared.chunked && prepared.requests.size() != 1) ||
		(prepared.chunked && prepared.requests.size() < 2) ||
		(!prepared.chunked && !prepared.complete() &&
			prepared.requests[0].payload.size() != prepared.totalPayloadBytes))
	{
		result.error = TacticalWorldDeltaPublishError::InvalidDelta;
		return result;
	}
	if (prepared.complete())
	{
		result.complete = true;
		return result;
	}

	// Validate the complete unsent suffix before enqueueing any of it. A caller
	// mutation can therefore never strand a newly published transfer prefix.
	for (std::size_t index = prepared.nextRequest;
		index < prepared.requests.size(); ++index)
	{
		const RuntimeMessageRequest& request = prepared.requests[index];
		if (request.topic != expectedTopic ||
			request.source != TacticalWorldDeltaMessageSource ||
			request.payload.empty())
		{
			result.error = TacticalWorldDeltaPublishError::InvalidDelta;
			return result;
		}
		if (request.payload.size() > maximumPayloadBytes_)
		{
			result.error = TacticalWorldDeltaPublishError::PayloadTooLarge;
			return result;
		}
	}

	while (prepared.nextRequest < prepared.requests.size())
	{
		RuntimeMessageRequest& request = prepared.requests[prepared.nextRequest];
		const RuntimeMessagePublishResult published =
			messages_.publishRetained(request);
		if (!published)
		{
			result.error = MapTacticalWorldDeltaMessageError(published.error);
			return result;
		}
		if (result.firstSequence == 0) result.firstSequence = published.sequence;
		result.lastSequence = published.sequence;
		++result.messagesPublished;
		++prepared.nextRequest;
	}
	result.complete = true;
	return result;
}
