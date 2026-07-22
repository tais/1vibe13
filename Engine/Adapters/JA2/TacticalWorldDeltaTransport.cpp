#include <Engine/Adapters/JA2/TacticalWorldDeltaTransport.h>

#include <algorithm>
#include <limits>
#include <utility>

#include <Engine/Core/BinaryArchive.h>

namespace
{
constexpr std::uint32_t TacticalWorldDeltaChunkMagic = 0x31435754u; // "TWC1"

struct DecodedChunk
{
	std::uint64_t transferId = 0;
	std::uint32_t index = 0;
	std::uint32_t count = 0;
	std::uint64_t totalPayloadBytes = 0;
	std::uint32_t checksum = 0;
	const std::uint8_t* payload = nullptr;
	std::size_t payloadBytes = 0;
};

bool DecodeChunkEnvelope(
	const std::vector<std::uint8_t>& bytes,
	DecodedChunk& chunk,
	bool& unsupportedVersion) noexcept
{
	unsupportedVersion = false;
	if (bytes.size() < TacticalWorldDeltaChunkHeaderBytes + 1) return false;
	BinaryReader reader(bytes);
	std::uint32_t magic = 0;
	std::uint16_t version = 0;
	std::uint32_t payloadBytes = 0;
	if (!reader.readU32(magic) || !reader.readU16(version) ||
		magic != TacticalWorldDeltaChunkMagic)
		return false;
	if (version != TacticalWorldDeltaChunkWireVersion)
	{
		unsupportedVersion = true;
		return false;
	}
	if (!reader.readU64(chunk.transferId) || !reader.readU32(chunk.index) ||
		!reader.readU32(chunk.count) || !reader.readU64(chunk.totalPayloadBytes) ||
		!reader.readU32(chunk.checksum) || !reader.readU32(payloadBytes) ||
		payloadBytes == 0 || payloadBytes != reader.remaining())
		return false;
	chunk.payload = bytes.data() + reader.position();
	chunk.payloadBytes = payloadBytes;
	return true;
}

TacticalWorldDeltaReassemblyResult MapDeltaDecodeResult(
	TacticalWorldDeltaDecodeResult result) noexcept
{
	switch (result)
	{
		case TacticalWorldDeltaDecodeResult::Success:
			return TacticalWorldDeltaReassemblyResult::Completed;
		case TacticalWorldDeltaDecodeResult::Invalid:
			return TacticalWorldDeltaReassemblyResult::InvalidDelta;
		case TacticalWorldDeltaDecodeResult::UnsupportedVersion:
			return TacticalWorldDeltaReassemblyResult::UnsupportedDeltaVersion;
		case TacticalWorldDeltaDecodeResult::TooManyEvents:
			return TacticalWorldDeltaReassemblyResult::TooManyEvents;
		case TacticalWorldDeltaDecodeResult::AllocationFailure:
			return TacticalWorldDeltaReassemblyResult::AllocationFailure;
	}
	return TacticalWorldDeltaReassemblyResult::InvalidDelta;
}
}

std::uint32_t TacticalWorldDeltaTransferChecksum(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (!bytes && size != 0) return 0;
	std::uint32_t checksum = 2166136261u;
	for (std::size_t index = 0; index < size; ++index)
	{
		checksum ^= bytes[index];
		checksum *= 16777619u;
	}
	return checksum;
}

TacticalWorldDeltaChunkEncodeError EncodeTacticalWorldDeltaChunks(
	const std::vector<std::uint8_t>& encodedDelta,
	std::uint64_t transferId,
	std::size_t maximumMessagePayloadBytes,
	EncodedTacticalWorldDeltaChunks& output,
	std::size_t maximumTransferBytes,
	std::size_t maximumChunks) noexcept
{
	if (transferId == 0 || encodedDelta.empty())
		return TacticalWorldDeltaChunkEncodeError::InvalidTransfer;
	const std::size_t effectiveTransferLimit = std::min(
		maximumTransferBytes, MaximumTacticalWorldDeltaTransferBytes);
	const std::size_t effectiveChunkLimit = std::min(
		maximumChunks, MaximumTacticalWorldDeltaTransferChunks);
	if (encodedDelta.size() > effectiveTransferLimit)
		return TacticalWorldDeltaChunkEncodeError::TransferTooLarge;
	if (maximumMessagePayloadBytes <= TacticalWorldDeltaChunkHeaderBytes)
		return TacticalWorldDeltaChunkEncodeError::PayloadLimitTooSmall;

	const std::size_t bytesPerChunk =
		maximumMessagePayloadBytes - TacticalWorldDeltaChunkHeaderBytes;
	const std::size_t chunkCount =
		1 + (encodedDelta.size() - 1) / bytesPerChunk;
	if (chunkCount < 2 || chunkCount > effectiveChunkLimit ||
		chunkCount > std::numeric_limits<std::uint32_t>::max())
		return TacticalWorldDeltaChunkEncodeError::TooManyChunks;

	try
	{
		EncodedTacticalWorldDeltaChunks encoded;
		encoded.transferId = transferId;
		encoded.totalPayloadBytes = encodedDelta.size();
		encoded.checksum = TacticalWorldDeltaTransferChecksum(
			encodedDelta.data(), encodedDelta.size());
		encoded.payloads.reserve(chunkCount);
		for (std::size_t index = 0, offset = 0;
			index < chunkCount; ++index)
		{
			const std::size_t payloadBytes = std::min(
				bytesPerChunk, encodedDelta.size() - offset);
			BinaryWriter writer;
			writer.writeU32(TacticalWorldDeltaChunkMagic);
			writer.writeU16(TacticalWorldDeltaChunkWireVersion);
			writer.writeU64(transferId);
			writer.writeU32(static_cast<std::uint32_t>(index));
			writer.writeU32(static_cast<std::uint32_t>(chunkCount));
			writer.writeU64(static_cast<std::uint64_t>(encodedDelta.size()));
			writer.writeU32(encoded.checksum);
			writer.writeU32(static_cast<std::uint32_t>(payloadBytes));
			if (writer.bytes().size() != TacticalWorldDeltaChunkHeaderBytes)
				return TacticalWorldDeltaChunkEncodeError::InvalidTransfer;
			writer.writeBytes(encodedDelta.data() + offset, payloadBytes);
			encoded.payloads.push_back(writer.take());
			offset += payloadBytes;
		}
		output = std::move(encoded);
		return TacticalWorldDeltaChunkEncodeError::None;
	}
	catch (...)
	{
		return TacticalWorldDeltaChunkEncodeError::AllocationFailure;
	}
}

TacticalWorldDeltaReassembler::TacticalWorldDeltaReassembler(
	TacticalWorldDeltaReassemblyLimits limits) noexcept
	: limits_{
		std::min(limits.maximumTransferBytes,
			MaximumTacticalWorldDeltaTransferBytes),
		std::min(limits.maximumChunks,
			MaximumTacticalWorldDeltaTransferChunks),
		std::min(limits.maximumEvents, MaximumTacticalWorldDeltaEvents)}
{
}

TacticalWorldDeltaReassemblyResult TacticalWorldDeltaReassembler::accept(
	const RuntimeMessage& message, TacticalWorldDelta& output) noexcept
{
	if (message.source != TacticalWorldDeltaMessageSource)
		return TacticalWorldDeltaReassemblyResult::InvalidMessage;
	if (message.topic == TacticalWorldDeltaMessageTopic)
	{
		if (message.payload.size() > limits_.maximumTransferBytes)
			return TacticalWorldDeltaReassemblyResult::TransferTooLarge;
		TacticalWorldDelta decoded;
		const TacticalWorldDeltaReassemblyResult decodedResult = MapDeltaDecodeResult(
			DecodeTacticalWorldDelta(message.payload, decoded, limits_.maximumEvents));
		if (decodedResult != TacticalWorldDeltaReassemblyResult::Completed)
			return decodedResult;
		reset();
		output = std::move(decoded);
		return TacticalWorldDeltaReassemblyResult::Completed;
	}
	if (message.topic != TacticalWorldDeltaChunkMessageTopic)
		return TacticalWorldDeltaReassemblyResult::InvalidMessage;

	DecodedChunk chunk;
	bool unsupportedVersion = false;
	if (!DecodeChunkEnvelope(message.payload, chunk, unsupportedVersion))
		return unsupportedVersion
			? TacticalWorldDeltaReassemblyResult::UnsupportedVersion
			: TacticalWorldDeltaReassemblyResult::InvalidMessage;
	if (chunk.transferId == 0 || chunk.count < 2 || chunk.index >= chunk.count ||
		chunk.totalPayloadBytes == 0 ||
		chunk.payloadBytes > chunk.totalPayloadBytes)
		return TacticalWorldDeltaReassemblyResult::InvalidTransfer;
	if (chunk.totalPayloadBytes > limits_.maximumTransferBytes)
		return TacticalWorldDeltaReassemblyResult::TransferTooLarge;
	if (chunk.count > limits_.maximumChunks)
		return TacticalWorldDeltaReassemblyResult::TooManyChunks;

	const bool replacingIncompleteTransfer =
		active() && chunk.transferId > transferId_ && chunk.index == 0;
	if (active() && !replacingIncompleteTransfer)
	{
		if (chunk.transferId != transferId_)
			return TacticalWorldDeltaReassemblyResult::InterleavedTransfer;
		if (chunk.count != chunkCount_ ||
			chunk.totalPayloadBytes != totalPayloadBytes_ ||
			chunk.checksum != checksum_)
			return TacticalWorldDeltaReassemblyResult::InvalidTransfer;
		if (chunk.index != nextChunkIndex_)
			return TacticalWorldDeltaReassemblyResult::UnexpectedChunk;
	}
	else if (!active() && chunk.index != 0)
	{
		return TacticalWorldDeltaReassemblyResult::UnexpectedChunk;
	}
	else if (!active() && chunk.transferId <= highestTransferId_)
	{
		return TacticalWorldDeltaReassemblyResult::InterleavedTransfer;
	}

	const std::size_t retainedBytes = replacingIncompleteTransfer
		? 0 : encodedDelta_.size();
	if (chunk.payloadBytes >
		static_cast<std::size_t>(chunk.totalPayloadBytes) - retainedBytes)
		return TacticalWorldDeltaReassemblyResult::InvalidTransfer;
	const std::size_t acceptedBytes = retainedBytes + chunk.payloadBytes;
	const bool finalChunk = chunk.index + 1 == chunk.count;
	if ((finalChunk && acceptedBytes != chunk.totalPayloadBytes) ||
		(!finalChunk && acceptedBytes >= chunk.totalPayloadBytes))
		return TacticalWorldDeltaReassemblyResult::InvalidTransfer;

	try
	{
		if (!active() || replacingIncompleteTransfer)
		{
			// Build the replacement prefix before touching an incomplete transfer.
			// Allocation failure therefore leaves the old transfer resumable.
			std::vector<std::uint8_t> replacement;
			replacement.reserve(static_cast<std::size_t>(chunk.totalPayloadBytes));
			replacement.insert(
				replacement.end(), chunk.payload, chunk.payload + chunk.payloadBytes);
			encodedDelta_ = std::move(replacement);
			transferId_ = chunk.transferId;
			chunkCount_ = chunk.count;
			totalPayloadBytes_ = static_cast<std::size_t>(chunk.totalPayloadBytes);
			checksum_ = chunk.checksum;
			nextChunkIndex_ = 1;
			highestTransferId_ = chunk.transferId;
		}
		else
		{
			encodedDelta_.insert(
				encodedDelta_.end(), chunk.payload, chunk.payload + chunk.payloadBytes);
			++nextChunkIndex_;
		}
	}
	catch (...)
	{
		return TacticalWorldDeltaReassemblyResult::AllocationFailure;
	}

	if (!finalChunk) return TacticalWorldDeltaReassemblyResult::AwaitingMore;
	if (TacticalWorldDeltaTransferChecksum(
			encodedDelta_.data(), encodedDelta_.size()) != checksum_)
	{
		reset();
		return TacticalWorldDeltaReassemblyResult::IntegrityMismatch;
	}

	TacticalWorldDelta decoded;
	const TacticalWorldDeltaReassemblyResult decodedResult = MapDeltaDecodeResult(
		DecodeTacticalWorldDelta(encodedDelta_, decoded, limits_.maximumEvents));
	reset();
	if (decodedResult != TacticalWorldDeltaReassemblyResult::Completed)
		return decodedResult;
	output = std::move(decoded);
	return TacticalWorldDeltaReassemblyResult::Completed;
}

void TacticalWorldDeltaReassembler::reset() noexcept
{
	transferId_ = 0;
	nextChunkIndex_ = 0;
	chunkCount_ = 0;
	totalPayloadBytes_ = 0;
	checksum_ = 0;
	encodedDelta_.clear();
}
