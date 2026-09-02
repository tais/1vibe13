#include "CoopCampaignSyncProtocol.h"

#include <algorithm>
#include <cstring>
#include <utility>

namespace CoopSession
{
namespace
{
constexpr std::uint8_t CampaignSyncMagic[4] = {'J', '2', 'C', 'S'};
constexpr std::size_t Sha256BlockBytes = 64;

std::uint32_t RotateRight(std::uint32_t value, unsigned count) noexcept
{
	return (value >> count) | (value << (32u - count));
}

class Sha256
{
public:
	Sha256() noexcept
		: state_{0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u,
			0xa54ff53au, 0x510e527fu, 0x9b05688cu,
			0x1f83d9abu, 0x5be0cd19u}
	{
	}

	void update(const std::uint8_t* bytes, std::size_t size) noexcept
	{
		totalBytes_ += size;
		while (size)
		{
			const std::size_t copied = std::min(
				size, Sha256BlockBytes - bufferedBytes_);
			std::memcpy(buffer_.data() + bufferedBytes_, bytes, copied);
			bufferedBytes_ += copied;
			bytes += copied;
			size -= copied;
			if (bufferedBytes_ == Sha256BlockBytes)
			{
				transform(buffer_.data());
				bufferedBytes_ = 0;
			}
		}
	}

	CoopCampaignIdentitySha256 finish() noexcept
	{
		const std::uint64_t bitLength = totalBytes_ * 8u;
		buffer_[bufferedBytes_++] = 0x80u;
		if (bufferedBytes_ > 56)
		{
			std::fill(buffer_.begin() + bufferedBytes_, buffer_.end(), 0);
			transform(buffer_.data());
			bufferedBytes_ = 0;
		}
		std::fill(buffer_.begin() + bufferedBytes_, buffer_.begin() + 56, 0);
		for (unsigned index = 0; index < 8; ++index)
			buffer_[56 + index] = static_cast<std::uint8_t>(
				bitLength >> (56u - index * 8u));
		transform(buffer_.data());

		CoopCampaignIdentitySha256 digest{};
		for (std::size_t word = 0; word < state_.size(); ++word)
			for (unsigned byte = 0; byte < 4; ++byte)
				digest[word * 4 + byte] = static_cast<std::uint8_t>(
					state_[word] >> (24u - byte * 8u));
		return digest;
	}

private:
	void transform(const std::uint8_t* block) noexcept
	{
		static constexpr std::uint32_t constants[64] = {
			0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
			0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
			0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
			0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
			0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
			0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
			0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
			0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
			0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
			0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
			0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
			0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
			0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
			0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
			0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
			0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

		std::uint32_t words[64]{};
		for (std::size_t index = 0; index < 16; ++index)
		{
			const std::size_t offset = index * 4;
			words[index] =
				(static_cast<std::uint32_t>(block[offset]) << 24) |
				(static_cast<std::uint32_t>(block[offset + 1]) << 16) |
				(static_cast<std::uint32_t>(block[offset + 2]) << 8) |
				static_cast<std::uint32_t>(block[offset + 3]);
		}
		for (std::size_t index = 16; index < 64; ++index)
		{
			const std::uint32_t first = RotateRight(words[index - 15], 7) ^
				RotateRight(words[index - 15], 18) ^
				(words[index - 15] >> 3);
			const std::uint32_t second = RotateRight(words[index - 2], 17) ^
				RotateRight(words[index - 2], 19) ^
				(words[index - 2] >> 10);
			words[index] = words[index - 16] + first +
				words[index - 7] + second;
		}

		std::uint32_t a = state_[0];
		std::uint32_t b = state_[1];
		std::uint32_t c = state_[2];
		std::uint32_t d = state_[3];
		std::uint32_t e = state_[4];
		std::uint32_t f = state_[5];
		std::uint32_t g = state_[6];
		std::uint32_t h = state_[7];
		for (std::size_t index = 0; index < 64; ++index)
		{
			const std::uint32_t sum1 = RotateRight(e, 6) ^
				RotateRight(e, 11) ^ RotateRight(e, 25);
			const std::uint32_t choose = (e & f) ^ (~e & g);
			const std::uint32_t temporary1 = h + sum1 + choose +
				constants[index] + words[index];
			const std::uint32_t sum0 = RotateRight(a, 2) ^
				RotateRight(a, 13) ^ RotateRight(a, 22);
			const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
			const std::uint32_t temporary2 = sum0 + majority;
			h = g;
			g = f;
			f = e;
			e = d + temporary1;
			d = c;
			c = b;
			b = a;
			a = temporary1 + temporary2;
		}
		state_[0] += a;
		state_[1] += b;
		state_[2] += c;
		state_[3] += d;
		state_[4] += e;
		state_[5] += f;
		state_[6] += g;
		state_[7] += h;
	}

	std::array<std::uint32_t, 8> state_;
	std::array<std::uint8_t, Sha256BlockBytes> buffer_{};
	std::uint64_t totalBytes_ = 0;
	std::size_t bufferedBytes_ = 0;
};

void WriteU16(std::uint8_t*& output, std::uint16_t value) noexcept
{
	*output++ = static_cast<std::uint8_t>(value);
	*output++ = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32(std::uint8_t*& output, std::uint32_t value) noexcept
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		*output++ = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(std::uint8_t*& output, std::uint64_t value) noexcept
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		*output++ = static_cast<std::uint8_t>(value >> shift);
}

std::uint16_t ReadU16(const std::uint8_t*& input) noexcept
{
	const std::uint16_t value = static_cast<std::uint16_t>(input[0]) |
		(static_cast<std::uint16_t>(input[1]) << 8);
	input += 2;
	return value;
}

std::uint32_t ReadU32(const std::uint8_t*& input) noexcept
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(*input++) << shift;
	return value;
}

std::uint64_t ReadU64(const std::uint8_t*& input) noexcept
{
	std::uint64_t value = 0;
	for (unsigned shift = 0; shift < 64; shift += 8)
		value |= static_cast<std::uint64_t>(*input++) << shift;
	return value;
}

template<typename Hash>
bool IsZeroHash(const Hash& hash) noexcept
{
	for (std::uint8_t byte : hash)
		if (byte != 0) return false;
	return true;
}

void WriteCommonHeader(std::uint8_t*& output,
	CoopCampaignSyncMessageKind kind,
	const CoopCampaignSyncTransferIdentity& transfer) noexcept
{
	std::copy(CampaignSyncMagic, CampaignSyncMagic + 4, output);
	output += 4;
	WriteU16(output, transfer.wireVersion);
	WriteU16(output, transfer.protocolVersion);
	*output++ = static_cast<std::uint8_t>(kind);
	for (unsigned index = 0; index < 7; ++index) *output++ = 0;
	WriteU64(output, transfer.sessionEpoch);
	WriteU64(output, transfer.transferId);
	WriteU64(output, transfer.campaignSeed);
	std::copy(transfer.campaignIdentitySha256.begin(),
		transfer.campaignIdentitySha256.end(), output);
	output += transfer.campaignIdentitySha256.size();
	WriteU64(output, transfer.checkpointGeneration);
	WriteU64(output, transfer.totalSize);
	std::copy(transfer.checkpointSha256.begin(),
		transfer.checkpointSha256.end(), output);
	output += transfer.checkpointSha256.size();
	WriteU32(output, transfer.canonicalChunkBytes);
	WriteU32(output, 0);
}

CoopCampaignSyncCodecResult ReadCommonHeader(const std::uint8_t* bytes,
	std::size_t size, CoopCampaignSyncMessageKind expectedKind,
	CoopCampaignSyncTransferIdentity& transfer,
	const std::uint8_t*& input) noexcept
{
	if (bytes == nullptr || size < CoopCampaignSyncCommonHeaderWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	if (!std::equal(CampaignSyncMagic, CampaignSyncMagic + 4, bytes))
		return CoopCampaignSyncCodecResult::Invalid;

	input = bytes + 4;
	transfer.wireVersion = ReadU16(input);
	transfer.protocolVersion = ReadU16(input);
	if (transfer.wireVersion != CoopCampaignSyncWireVersion ||
		transfer.protocolVersion != CurrentProtocolVersion)
		return CoopCampaignSyncCodecResult::UnsupportedVersion;

	const CoopCampaignSyncMessageKind kind =
		static_cast<CoopCampaignSyncMessageKind>(*input++);
	if (!IsKnownCoopCampaignSyncMessageKind(kind) || kind != expectedKind)
		return CoopCampaignSyncCodecResult::WrongMessageKind;
	for (unsigned index = 0; index < 7; ++index)
		if (*input++ != 0) return CoopCampaignSyncCodecResult::Invalid;

	transfer.sessionEpoch = ReadU64(input);
	transfer.transferId = ReadU64(input);
	transfer.campaignSeed = ReadU64(input);
	std::copy(input, input + transfer.campaignIdentitySha256.size(),
		transfer.campaignIdentitySha256.begin());
	input += transfer.campaignIdentitySha256.size();
	transfer.checkpointGeneration = ReadU64(input);
	transfer.totalSize = ReadU64(input);
	std::copy(input, input + transfer.checkpointSha256.size(),
		transfer.checkpointSha256.begin());
	input += transfer.checkpointSha256.size();
	transfer.canonicalChunkBytes = ReadU32(input);
	if (ReadU32(input) != 0)
		return CoopCampaignSyncCodecResult::Invalid;
	return IsValidCoopCampaignSyncTransferIdentity(transfer)
		? CoopCampaignSyncCodecResult::Success
		: CoopCampaignSyncCodecResult::Invalid;
}

bool ExpectedChunkSize(const CoopCampaignSyncTransferIdentity& transfer,
	std::uint64_t offset, std::size_t& expected) noexcept
{
	if (!IsValidCoopCampaignSyncTransferIdentity(transfer) ||
		offset >= transfer.totalSize ||
		offset % transfer.canonicalChunkBytes != 0)
		return false;
	const std::uint64_t remaining = transfer.totalSize - offset;
	const std::uint64_t length = std::min<std::uint64_t>(
		remaining, transfer.canonicalChunkBytes);
	expected = static_cast<std::size_t>(length);
	return expected != 0;
}

bool IsValidChunkShape(const CoopCampaignSyncChunk& chunk) noexcept
{
	std::size_t expected = 0;
	return ExpectedChunkSize(chunk.transfer, chunk.offset, expected) &&
		chunk.payload.size() == expected;
}

bool IsValidAck(const CoopCampaignSyncAck& acknowledgement) noexcept
{
	return IsValidCoopCampaignSyncTransferIdentity(
			acknowledgement.transfer) &&
		!IsZero(acknowledgement.peerIdentity) &&
		IsCanonicalCoopCampaignSyncCursor(acknowledgement.transfer,
			acknowledgement.nextExpectedOffset) &&
		(acknowledgement.nextExpectedOffset != 0 ||
			acknowledgement.precedingChunkChecksum == 0);
}

bool IsValidResult(const CoopCampaignSyncResult& result) noexcept
{
	if (!IsValidCoopCampaignSyncTransferIdentity(result.transfer) ||
		IsZero(result.peerIdentity) ||
		!IsKnownCoopCampaignSyncResultStatus(result.status) ||
		!IsKnownCoopCampaignSyncFailureReason(result.reason))
		return false;
	return result.status == CoopCampaignSyncResultStatus::Committed
		? result.reason == CoopCampaignSyncFailureReason::None
		: result.reason != CoopCampaignSyncFailureReason::None;
}

bool IsValidResync(const CoopCampaignSyncResync& resync) noexcept
{
	return IsValidCoopCampaignSyncTransferIdentity(resync.transfer) &&
		!IsZero(resync.peerIdentity) &&
		IsCanonicalCoopCampaignSyncCursor(
			resync.transfer, resync.expectedOffset) &&
		(resync.expectedOffset != 0 ||
			resync.precedingChunkChecksum == 0) &&
		IsKnownCoopCampaignSyncFailureReason(resync.reason) &&
		resync.reason != CoopCampaignSyncFailureReason::None;
}

bool IsValidReject(const CoopCampaignSyncReject& rejection) noexcept
{
	return IsValidCoopCampaignSyncTransferIdentity(rejection.transfer) &&
		IsKnownCoopCampaignSyncFailureReason(rejection.reason) &&
		rejection.reason != CoopCampaignSyncFailureReason::None;
}
}

bool IsKnownCoopCampaignSyncMessageKind(
	CoopCampaignSyncMessageKind kind) noexcept
{
	switch (kind)
	{
		case CoopCampaignSyncMessageKind::Metadata:
		case CoopCampaignSyncMessageKind::Chunk:
		case CoopCampaignSyncMessageKind::Ack:
		case CoopCampaignSyncMessageKind::Complete:
		case CoopCampaignSyncMessageKind::Result:
		case CoopCampaignSyncMessageKind::Resync:
		case CoopCampaignSyncMessageKind::Reject:
			return true;
	}
	return false;
}

bool IsKnownCoopCampaignSyncResultStatus(
	CoopCampaignSyncResultStatus status) noexcept
{
	return status == CoopCampaignSyncResultStatus::Committed ||
		status == CoopCampaignSyncResultStatus::Rejected;
}

bool IsKnownCoopCampaignSyncFailureReason(
	CoopCampaignSyncFailureReason reason) noexcept
{
	switch (reason)
	{
		case CoopCampaignSyncFailureReason::None:
		case CoopCampaignSyncFailureReason::MalformedMessage:
		case CoopCampaignSyncFailureReason::SessionMismatch:
		case CoopCampaignSyncFailureReason::TransferMismatch:
		case CoopCampaignSyncFailureReason::CheckpointMismatch:
		case CoopCampaignSyncFailureReason::SequenceMismatch:
		case CoopCampaignSyncFailureReason::ChunkChecksumMismatch:
		case CoopCampaignSyncFailureReason::HashMismatch:
		case CoopCampaignSyncFailureReason::StorageFailure:
		case CoopCampaignSyncFailureReason::LoadFailed:
		case CoopCampaignSyncFailureReason::CompatibilityMismatch:
		case CoopCampaignSyncFailureReason::CapacityReached:
		case CoopCampaignSyncFailureReason::Superseded:
		case CoopCampaignSyncFailureReason::ProtocolViolation:
			return true;
	}
	return false;
}

bool IsCanonicalCoopCampaignIdentityId(
	const std::string& campaignId) noexcept
{
	if (campaignId.empty() ||
		campaignId.size() > MaximumCoopCampaignIdentityIdBytes)
		return false;
	for (unsigned char value : campaignId)
	{
		if ((value >= 'a' && value <= 'z') ||
			(value >= '0' && value <= '9') || value == '-' || value == '_')
			continue;
		return false;
	}
	return true;
}

bool ComputeCoopCampaignIdentitySha256(const std::string& campaignId,
	std::uint64_t campaignSeed,
	CoopCampaignIdentitySha256& digest) noexcept
{
	if (!IsCanonicalCoopCampaignIdentityId(campaignId)) return false;
	Sha256 hash;
	hash.update(reinterpret_cast<const std::uint8_t*>(
		CoopCampaignIdentitySha256Domain),
		sizeof(CoopCampaignIdentitySha256Domain) - 1);
	const std::uint8_t length =
		static_cast<std::uint8_t>(campaignId.size());
	hash.update(&length, 1);
	hash.update(reinterpret_cast<const std::uint8_t*>(campaignId.data()),
		campaignId.size());
	std::uint8_t seedBytes[8]{};
	for (unsigned index = 0; index < 8; ++index)
		seedBytes[index] = static_cast<std::uint8_t>(
			campaignSeed >> (index * 8u));
	hash.update(seedBytes, sizeof(seedBytes));
	const CoopCampaignIdentitySha256 computed = hash.finish();
	digest = computed;
	return true;
}

bool IsValidCoopCampaignSyncTransferIdentity(
	const CoopCampaignSyncTransferIdentity& transfer) noexcept
{
	return transfer.wireVersion == CoopCampaignSyncWireVersion &&
		transfer.protocolVersion == CurrentProtocolVersion &&
		transfer.sessionEpoch != 0 && transfer.transferId != 0 &&
		!IsZeroHash(transfer.campaignIdentitySha256) &&
		transfer.checkpointGeneration != 0 && transfer.totalSize != 0 &&
		transfer.totalSize <= MaximumCoopCampaignCheckpointBytes &&
		!IsZeroHash(transfer.checkpointSha256) &&
		transfer.canonicalChunkBytes ==
			CoopCampaignSyncCanonicalChunkBytes;
}

bool SameCoopCampaignSyncTransfer(
	const CoopCampaignSyncTransferIdentity& left,
	const CoopCampaignSyncTransferIdentity& right) noexcept
{
	return left.wireVersion == right.wireVersion &&
		left.protocolVersion == right.protocolVersion &&
		left.sessionEpoch == right.sessionEpoch &&
		left.transferId == right.transferId &&
		left.campaignSeed == right.campaignSeed &&
		left.campaignIdentitySha256 == right.campaignIdentitySha256 &&
		left.checkpointGeneration == right.checkpointGeneration &&
		left.totalSize == right.totalSize &&
		left.checkpointSha256 == right.checkpointSha256 &&
		left.canonicalChunkBytes == right.canonicalChunkBytes;
}

bool IsCanonicalCoopCampaignSyncCursor(
	const CoopCampaignSyncTransferIdentity& transfer,
	std::uint64_t cursor) noexcept
{
	return IsValidCoopCampaignSyncTransferIdentity(transfer) &&
		cursor <= transfer.totalSize &&
		(cursor == transfer.totalSize ||
			cursor % transfer.canonicalChunkBytes == 0);
}

std::uint32_t CoopCampaignSyncPayloadChecksum(
	const std::uint8_t* bytes, std::size_t size) noexcept
{
	if (bytes == nullptr && size != 0) return 0;
	std::uint32_t checksum = UINT32_C(2166136261);
	for (std::size_t index = 0; index < size; ++index)
	{
		checksum ^= bytes[index];
		checksum *= UINT32_C(16777619);
	}
	return checksum;
}

CoopCampaignSyncChunkSequenceResult ValidateCoopCampaignSyncChunkAtOffset(
	const CoopCampaignSyncMetadata& metadata,
	const CoopCampaignSyncChunk& chunk,
	std::uint64_t expectedOffset) noexcept
{
	if (!IsValidCoopCampaignSyncTransferIdentity(metadata.transfer))
		return CoopCampaignSyncChunkSequenceResult::InvalidMetadata;
	if (!SameCoopCampaignSyncTransfer(metadata.transfer, chunk.transfer))
		return CoopCampaignSyncChunkSequenceResult::TransferMismatch;
	if (!IsCanonicalCoopCampaignSyncCursor(
			metadata.transfer, expectedOffset) ||
		expectedOffset == metadata.transfer.totalSize)
		return CoopCampaignSyncChunkSequenceResult::InvalidExpectedOffset;
	if (chunk.offset < expectedOffset)
		return CoopCampaignSyncChunkSequenceResult::Overlap;
	if (chunk.offset > expectedOffset)
		return CoopCampaignSyncChunkSequenceResult::Gap;
	if (!IsValidChunkShape(chunk))
		return CoopCampaignSyncChunkSequenceResult::InvalidChunk;
	if (CoopCampaignSyncPayloadChecksum(
			chunk.payload.data(), chunk.payload.size()) !=
		chunk.payloadChecksum)
		return CoopCampaignSyncChunkSequenceResult::ChecksumMismatch;
	return CoopCampaignSyncChunkSequenceResult::Success;
}

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncMetadata(
	const CoopCampaignSyncMetadata& metadata,
	CoopCampaignSyncMetadataBytes& bytes) noexcept
{
	if (!IsValidCoopCampaignSyncTransferIdentity(metadata.transfer))
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncMetadataBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopCampaignSyncMessageKind::Metadata,
		metadata.transfer);
	WriteU64(output, metadata.worldMinutes);
	bytes = encoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult DecodeCoopCampaignSyncMetadata(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncMetadata& metadata) noexcept
{
	if (bytes == nullptr || size != CoopCampaignSyncMetadataWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncMetadata decoded;
	const std::uint8_t* input = nullptr;
	const CoopCampaignSyncCodecResult header = ReadCommonHeader(bytes, size,
		CoopCampaignSyncMessageKind::Metadata, decoded.transfer, input);
	if (header != CoopCampaignSyncCodecResult::Success) return header;
	decoded.worldMinutes = ReadU64(input);
	metadata = decoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncChunk(
	const CoopCampaignSyncChunk& chunk,
	std::vector<std::uint8_t>& bytes) noexcept
{
	if (chunk.payload.size() > CoopCampaignSyncCanonicalChunkBytes)
		return CoopCampaignSyncCodecResult::PayloadTooLarge;
	if (!IsValidChunkShape(chunk))
		return CoopCampaignSyncCodecResult::Invalid;
	const std::uint32_t checksum = CoopCampaignSyncPayloadChecksum(
		chunk.payload.data(), chunk.payload.size());
	if (chunk.payloadChecksum != 0 && chunk.payloadChecksum != checksum)
		return CoopCampaignSyncCodecResult::ChecksumMismatch;

	try
	{
		std::vector<std::uint8_t> encoded(
			CoopCampaignSyncChunkHeaderWireSize + chunk.payload.size());
		std::uint8_t* output = encoded.data();
		WriteCommonHeader(output, CoopCampaignSyncMessageKind::Chunk,
			chunk.transfer);
		WriteU64(output, chunk.offset);
		WriteU32(output, static_cast<std::uint32_t>(chunk.payload.size()));
		WriteU32(output, checksum);
		std::copy(chunk.payload.begin(), chunk.payload.end(), output);
		bytes.swap(encoded);
		return CoopCampaignSyncCodecResult::Success;
	}
	catch (...)
	{
		return CoopCampaignSyncCodecResult::AllocationFailure;
	}
}

CoopCampaignSyncCodecResult DecodeCoopCampaignSyncChunk(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncChunk& chunk) noexcept
{
	if (bytes == nullptr || size < CoopCampaignSyncChunkHeaderWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	if (size > MaximumCoopCampaignSyncWireSize)
		return CoopCampaignSyncCodecResult::PayloadTooLarge;

	CoopCampaignSyncChunk decoded;
	const std::uint8_t* input = nullptr;
	const CoopCampaignSyncCodecResult header = ReadCommonHeader(bytes, size,
		CoopCampaignSyncMessageKind::Chunk, decoded.transfer, input);
	if (header != CoopCampaignSyncCodecResult::Success) return header;
	decoded.offset = ReadU64(input);
	const std::uint32_t payloadSize = ReadU32(input);
	decoded.payloadChecksum = ReadU32(input);
	if (payloadSize > CoopCampaignSyncCanonicalChunkBytes)
		return CoopCampaignSyncCodecResult::PayloadTooLarge;
	if (size - CoopCampaignSyncChunkHeaderWireSize != payloadSize)
		return CoopCampaignSyncCodecResult::Invalid;

	std::size_t expected = 0;
	if (!ExpectedChunkSize(decoded.transfer, decoded.offset, expected) ||
		expected != payloadSize)
		return CoopCampaignSyncCodecResult::Invalid;
	if (CoopCampaignSyncPayloadChecksum(input, payloadSize) !=
		decoded.payloadChecksum)
		return CoopCampaignSyncCodecResult::ChecksumMismatch;

	try
	{
		decoded.payload.assign(input, input + payloadSize);
		chunk = std::move(decoded);
		return CoopCampaignSyncCodecResult::Success;
	}
	catch (...)
	{
		return CoopCampaignSyncCodecResult::AllocationFailure;
	}
}

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncAck(
	const CoopCampaignSyncAck& acknowledgement,
	CoopCampaignSyncAckBytes& bytes) noexcept
{
	if (!IsValidAck(acknowledgement))
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncAckBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopCampaignSyncMessageKind::Ack,
		acknowledgement.transfer);
	std::copy(acknowledgement.peerIdentity.begin(),
		acknowledgement.peerIdentity.end(), output);
	output += acknowledgement.peerIdentity.size();
	WriteU64(output, acknowledgement.nextExpectedOffset);
	WriteU32(output, acknowledgement.precedingChunkChecksum);
	WriteU32(output, 0);
	bytes = encoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult DecodeCoopCampaignSyncAck(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncAck& acknowledgement) noexcept
{
	if (bytes == nullptr || size != CoopCampaignSyncAckWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncAck decoded;
	const std::uint8_t* input = nullptr;
	const CoopCampaignSyncCodecResult header = ReadCommonHeader(bytes, size,
		CoopCampaignSyncMessageKind::Ack, decoded.transfer, input);
	if (header != CoopCampaignSyncCodecResult::Success) return header;
	std::copy(input, input + decoded.peerIdentity.size(),
		decoded.peerIdentity.begin());
	input += decoded.peerIdentity.size();
	decoded.nextExpectedOffset = ReadU64(input);
	decoded.precedingChunkChecksum = ReadU32(input);
	if (ReadU32(input) != 0 || !IsValidAck(decoded))
		return CoopCampaignSyncCodecResult::Invalid;
	acknowledgement = decoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncComplete(
	const CoopCampaignSyncComplete& completion,
	CoopCampaignSyncCompleteBytes& bytes) noexcept
{
	if (!IsValidCoopCampaignSyncTransferIdentity(completion.transfer))
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncCompleteBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopCampaignSyncMessageKind::Complete,
		completion.transfer);
	bytes = encoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult DecodeCoopCampaignSyncComplete(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncComplete& completion) noexcept
{
	if (bytes == nullptr || size != CoopCampaignSyncCompleteWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncComplete decoded;
	const std::uint8_t* input = nullptr;
	const CoopCampaignSyncCodecResult header = ReadCommonHeader(bytes, size,
		CoopCampaignSyncMessageKind::Complete, decoded.transfer, input);
	if (header != CoopCampaignSyncCodecResult::Success) return header;
	completion = decoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncResult(
	const CoopCampaignSyncResult& result,
	CoopCampaignSyncResultBytes& bytes) noexcept
{
	if (!IsValidResult(result)) return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncResultBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopCampaignSyncMessageKind::Result,
		result.transfer);
	std::copy(result.peerIdentity.begin(), result.peerIdentity.end(), output);
	output += result.peerIdentity.size();
	*output++ = static_cast<std::uint8_t>(result.status);
	*output++ = static_cast<std::uint8_t>(result.reason);
	for (unsigned index = 0; index < 6; ++index) *output++ = 0;
	bytes = encoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult DecodeCoopCampaignSyncResult(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncResult& result) noexcept
{
	if (bytes == nullptr || size != CoopCampaignSyncResultWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncResult decoded;
	const std::uint8_t* input = nullptr;
	const CoopCampaignSyncCodecResult header = ReadCommonHeader(bytes, size,
		CoopCampaignSyncMessageKind::Result, decoded.transfer, input);
	if (header != CoopCampaignSyncCodecResult::Success) return header;
	std::copy(input, input + decoded.peerIdentity.size(),
		decoded.peerIdentity.begin());
	input += decoded.peerIdentity.size();
	decoded.status = static_cast<CoopCampaignSyncResultStatus>(*input++);
	decoded.reason = static_cast<CoopCampaignSyncFailureReason>(*input++);
	for (unsigned index = 0; index < 6; ++index)
		if (*input++ != 0) return CoopCampaignSyncCodecResult::Invalid;
	if (!IsValidResult(decoded)) return CoopCampaignSyncCodecResult::Invalid;
	result = decoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncResync(
	const CoopCampaignSyncResync& resync,
	CoopCampaignSyncResyncBytes& bytes) noexcept
{
	if (!IsValidResync(resync)) return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncResyncBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopCampaignSyncMessageKind::Resync,
		resync.transfer);
	std::copy(resync.peerIdentity.begin(), resync.peerIdentity.end(), output);
	output += resync.peerIdentity.size();
	WriteU64(output, resync.expectedOffset);
	WriteU32(output, resync.precedingChunkChecksum);
	*output++ = static_cast<std::uint8_t>(resync.reason);
	for (unsigned index = 0; index < 3; ++index) *output++ = 0;
	bytes = encoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult DecodeCoopCampaignSyncResync(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncResync& resync) noexcept
{
	if (bytes == nullptr || size != CoopCampaignSyncResyncWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncResync decoded;
	const std::uint8_t* input = nullptr;
	const CoopCampaignSyncCodecResult header = ReadCommonHeader(bytes, size,
		CoopCampaignSyncMessageKind::Resync, decoded.transfer, input);
	if (header != CoopCampaignSyncCodecResult::Success) return header;
	std::copy(input, input + decoded.peerIdentity.size(),
		decoded.peerIdentity.begin());
	input += decoded.peerIdentity.size();
	decoded.expectedOffset = ReadU64(input);
	decoded.precedingChunkChecksum = ReadU32(input);
	decoded.reason = static_cast<CoopCampaignSyncFailureReason>(*input++);
	for (unsigned index = 0; index < 3; ++index)
		if (*input++ != 0) return CoopCampaignSyncCodecResult::Invalid;
	if (!IsValidResync(decoded)) return CoopCampaignSyncCodecResult::Invalid;
	resync = decoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncReject(
	const CoopCampaignSyncReject& rejection,
	CoopCampaignSyncRejectBytes& bytes) noexcept
{
	if (!IsValidReject(rejection)) return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncRejectBytes encoded{};
	std::uint8_t* output = encoded.data();
	WriteCommonHeader(output, CoopCampaignSyncMessageKind::Reject,
		rejection.transfer);
	*output++ = static_cast<std::uint8_t>(rejection.reason);
	for (unsigned index = 0; index < 7; ++index) *output++ = 0;
	bytes = encoded;
	return CoopCampaignSyncCodecResult::Success;
}

CoopCampaignSyncCodecResult DecodeCoopCampaignSyncReject(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncReject& rejection) noexcept
{
	if (bytes == nullptr || size != CoopCampaignSyncRejectWireSize)
		return CoopCampaignSyncCodecResult::Invalid;
	CoopCampaignSyncReject decoded;
	const std::uint8_t* input = nullptr;
	const CoopCampaignSyncCodecResult header = ReadCommonHeader(bytes, size,
		CoopCampaignSyncMessageKind::Reject, decoded.transfer, input);
	if (header != CoopCampaignSyncCodecResult::Success) return header;
	decoded.reason = static_cast<CoopCampaignSyncFailureReason>(*input++);
	for (unsigned index = 0; index < 7; ++index)
		if (*input++ != 0) return CoopCampaignSyncCodecResult::Invalid;
	if (!IsValidReject(decoded)) return CoopCampaignSyncCodecResult::Invalid;
	rejection = decoded;
	return CoopCampaignSyncCodecResult::Success;
}
}
