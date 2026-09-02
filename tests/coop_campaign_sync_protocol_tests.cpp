#include "CoopCampaignSyncProtocol.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); } } while (0)

static_assert(CoopCampaignSyncCommonHeaderWireSize == 128);
static_assert(CoopCampaignSyncMetadataWireSize == 136);
static_assert(CoopCampaignSyncChunkHeaderWireSize == 144);
static_assert(CoopCampaignSyncAckWireSize == 160);
static_assert(CoopCampaignSyncCompleteWireSize == 128);
static_assert(CoopCampaignSyncResultWireSize == 152);
static_assert(CoopCampaignSyncResyncWireSize == 160);
static_assert(CoopCampaignSyncRejectWireSize == 136);
static_assert(CoopCampaignSyncCanonicalChunkBytes == 61440);
static_assert(MaximumCoopCampaignSyncChunkWindow == 3);
static_assert(MaximumCoopCampaignIdentityIdBytes == 48);
static_assert(sizeof(CoopCampaignIdentitySha256Domain) - 1 == 29);
static_assert(MaximumCoopCampaignSyncWireSize == 61584);
static_assert(MaximumCoopCampaignCheckpointBytes == UINT64_C(268435456));
static_assert(MaximumCoopCampaignSyncWireSize < 64u * 1024u);
static_assert(MaximumCoopCampaignSyncWindowWireBytes < 256u * 1024u);

static_assert(noexcept(EncodeCoopCampaignSyncMetadata(
	std::declval<const CoopCampaignSyncMetadata&>(),
	std::declval<CoopCampaignSyncMetadataBytes&>())));
static_assert(noexcept(EncodeCoopCampaignSyncChunk(
	std::declval<const CoopCampaignSyncChunk&>(),
	std::declval<std::vector<std::uint8_t>&>())));
static_assert(noexcept(ValidateCoopCampaignSyncChunkAtOffset(
	std::declval<const CoopCampaignSyncMetadata&>(),
	std::declval<const CoopCampaignSyncChunk&>(), std::uint64_t{})));

PeerIdentity Peer(std::uint8_t seed)
{
	PeerIdentity peer{};
	for (std::size_t index = 0; index < peer.size(); ++index)
		peer[index] = static_cast<std::uint8_t>(seed + index);
	return peer;
}

CoopCampaignCheckpointSha256 Hash(std::uint8_t seed)
{
	CoopCampaignCheckpointSha256 hash{};
	for (std::size_t index = 0; index < hash.size(); ++index)
		hash[index] = static_cast<std::uint8_t>(seed + index);
	return hash;
}

CoopCampaignSyncTransferIdentity Transfer(std::uint64_t totalSize = 3)
{
	CoopCampaignSyncTransferIdentity transfer;
	transfer.sessionEpoch = UINT64_C(0x0807060504030201);
	transfer.transferId = UINT64_C(0x1817161514131211);
	transfer.campaignSeed = UINT64_C(0x3837363534333231);
	CHECK(ComputeCoopCampaignIdentitySha256("shared_01",
		transfer.campaignSeed, transfer.campaignIdentitySha256),
		"transfer fixture campaign identity hashes");
	transfer.checkpointGeneration = UINT64_C(0x2827262524232221);
	transfer.totalSize = totalSize;
	transfer.checkpointSha256 = Hash(0x30);
	return transfer;
}

bool SameTransfer(const CoopCampaignSyncTransferIdentity& left,
	const CoopCampaignSyncTransferIdentity& right)
{
	return SameCoopCampaignSyncTransfer(left, right);
}

void WriteU32(std::uint8_t* bytes, std::size_t offset,
	std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(std::uint8_t* bytes, std::size_t offset,
	std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint32_t ReadU32(const std::uint8_t* bytes, std::size_t offset)
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
	return value;
}

std::uint64_t ReadU64(const std::uint8_t* bytes, std::size_t offset)
{
	std::uint64_t value = 0;
	for (unsigned shift = 0; shift < 64; shift += 8)
		value |= static_cast<std::uint64_t>(bytes[offset++]) << shift;
	return value;
}

void CheckCommonGolden(const std::uint8_t* bytes,
	CoopCampaignSyncMessageKind kind,
	const CoopCampaignSyncTransferIdentity& transfer)
{
	CHECK(bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'C' &&
		bytes[3] == 'S', "campaign sync magic is exact");
	CHECK(bytes[4] == 1 && bytes[5] == 0 && bytes[6] == 7 &&
		bytes[7] == 0 && bytes[8] == static_cast<std::uint8_t>(kind),
		"campaign sync versions and kind are exact");
	for (std::size_t index = 9; index < 16; ++index)
		CHECK(bytes[index] == 0, "common leading reserve is zero");
	CHECK(ReadU64(bytes, 16) == transfer.sessionEpoch,
		"session epoch is little endian");
	CHECK(ReadU64(bytes, 24) == transfer.transferId,
		"transfer id is little endian");
	CHECK(ReadU64(bytes, 32) == transfer.campaignSeed,
		"campaign seed is little endian");
	for (std::size_t index = 0;
		index < transfer.campaignIdentitySha256.size(); ++index)
		CHECK(bytes[40 + index] == transfer.campaignIdentitySha256[index],
			"campaign identity SHA occupies exact bytes");
	CHECK(ReadU64(bytes, 72) == transfer.checkpointGeneration,
		"checkpoint generation is little endian");
	CHECK(ReadU64(bytes, 80) == transfer.totalSize,
		"checkpoint size is little endian");
	for (std::size_t index = 0; index < transfer.checkpointSha256.size(); ++index)
		CHECK(bytes[88 + index] == transfer.checkpointSha256[index],
			"checkpoint SHA occupies exact bytes");
	CHECK(ReadU32(bytes, 120) == CoopCampaignSyncCanonicalChunkBytes,
		"canonical chunk size is little endian");
	for (std::size_t index = 124; index < 128; ++index)
		CHECK(bytes[index] == 0, "common trailing reserve is zero");
}

void TestNamesAndChecksum()
{
	CHECK(std::string(CoopCampaignSyncMetadataMessageName) ==
		"coop.campaign.sync.metadata", "metadata message name is frozen");
	CHECK(std::string(CoopCampaignSyncRejectMessageName) ==
		"coop.campaign.sync.reject", "reject message name is frozen");
	const std::array<std::uint8_t, 3> abc{{'a', 'b', 'c'}};
	CHECK(CoopCampaignSyncPayloadChecksum(abc.data(), abc.size()) ==
		UINT32_C(0x1a47e90b), "chunk checksum pins FNV-1a bytes");
	CHECK(CoopCampaignSyncPayloadChecksum(nullptr, 0) ==
		UINT32_C(2166136261), "empty checksum uses FNV offset basis");
	CHECK(CoopCampaignSyncPayloadChecksum(nullptr, 1) == 0,
		"nonempty null checksum input is rejected");
}

void TestCampaignIdentityDigest()
{
	const CoopCampaignIdentitySha256 golden{{
		0x44, 0x2f, 0x73, 0xac, 0x09, 0x1d, 0x0d, 0x20,
		0x10, 0x67, 0x08, 0x56, 0xda, 0xa7, 0x3c, 0x16,
		0x7d, 0xe2, 0xf0, 0x75, 0x5d, 0xc8, 0x5b, 0x03,
		0xe3, 0x5e, 0x9d, 0x41, 0xdf, 0x29, 0x95, 0x66}};
	CoopCampaignIdentitySha256 digest{};
	CHECK(ComputeCoopCampaignIdentitySha256("shared_01",
		UINT64_C(0x3837363534333231), digest),
		"canonical campaign identity hashes");
	CHECK(digest == golden,
		"campaign identity SHA-256 pins domain, length, id, and LE seed");

	CoopCampaignIdentitySha256 seedChanged{};
	CHECK(ComputeCoopCampaignIdentitySha256("shared_01",
		UINT64_C(0x3837363534333232), seedChanged) &&
		seedChanged != digest,
		"nearby campaign seeds cannot collide by framing");
	CoopCampaignIdentitySha256 idChanged{};
	CHECK(ComputeCoopCampaignIdentitySha256("shared_02",
		UINT64_C(0x3837363534333231), idChanged) &&
		idChanged != digest && idChanged != seedChanged,
		"nearby campaign ids produce distinct identities");
	CoopCampaignIdentitySha256 zeroSeed{};
	CHECK(ComputeCoopCampaignIdentitySha256("shared_01", 0, zeroSeed) &&
		zeroSeed != digest, "zero is a valid and bound campaign seed");

	CHECK(IsCanonicalCoopCampaignIdentityId("a") &&
		IsCanonicalCoopCampaignIdentityId("shared_01") &&
		IsCanonicalCoopCampaignIdentityId("pvp-2026") &&
		IsCanonicalCoopCampaignIdentityId(std::string(48, 'z')),
		"portable lowercase campaign ids through 48 bytes are canonical");
	for (const std::string& invalid : {std::string(), std::string(49, 'a'),
		std::string("Shared_01"), std::string("../escape"),
		std::string("two words"), std::string("caf\xC3\xA9")})
	{
		CHECK(!IsCanonicalCoopCampaignIdentityId(invalid),
			"noncanonical campaign id is rejected");
		CoopCampaignIdentitySha256 unchanged{};
		unchanged.fill(0xa5);
		CHECK(!ComputeCoopCampaignIdentitySha256(invalid, 7, unchanged),
			"noncanonical campaign id cannot be hashed");
		CHECK(std::all_of(unchanged.begin(), unchanged.end(),
			[](std::uint8_t byte) { return byte == 0xa5; }),
			"failed identity computation preserves output transactionally");
	}
}

void TestTransferValidation()
{
	CoopCampaignSyncTransferIdentity transfer = Transfer();
	CHECK(IsValidCoopCampaignSyncTransferIdentity(transfer),
		"complete immutable transfer identity is valid");

	CoopCampaignSyncTransferIdentity invalid = transfer;
	invalid.sessionEpoch = 0;
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"zero session epoch is invalid");
	invalid = transfer;
	invalid.transferId = 0;
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"zero transfer id is invalid");
	invalid = transfer;
	invalid.checkpointGeneration = 0;
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"zero checkpoint generation is invalid");
	invalid = transfer;
	invalid.totalSize = 0;
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"empty checkpoint is invalid");
	invalid = transfer;
	invalid.totalSize = MaximumCoopCampaignCheckpointBytes + 1;
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"checkpoint over 256 MiB is invalid");
	invalid = transfer;
	invalid.campaignIdentitySha256.fill(0);
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"missing campaign identity SHA is invalid");
	invalid = transfer;
	invalid.checkpointSha256.fill(0);
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"missing checkpoint SHA is invalid");
	invalid = transfer;
	invalid.canonicalChunkBytes--;
	CHECK(!IsValidCoopCampaignSyncTransferIdentity(invalid),
		"noncanonical chunk size is invalid in wire v1");

	transfer.totalSize = MaximumCoopCampaignCheckpointBytes;
	CHECK(IsValidCoopCampaignSyncTransferIdentity(transfer),
		"exact 256 MiB checkpoint is valid");
	CHECK(IsCanonicalCoopCampaignSyncCursor(transfer, 0),
		"zero cumulative cursor is canonical");
	CHECK(IsCanonicalCoopCampaignSyncCursor(transfer,
		CoopCampaignSyncCanonicalChunkBytes),
		"chunk boundary cumulative cursor is canonical");
	CHECK(IsCanonicalCoopCampaignSyncCursor(transfer, transfer.totalSize),
		"total-size cumulative cursor is canonical");
	CHECK(!IsCanonicalCoopCampaignSyncCursor(transfer, 1),
		"mid-chunk cumulative cursor is invalid");
	CHECK(!IsCanonicalCoopCampaignSyncCursor(
		transfer, transfer.totalSize + 1), "cursor beyond total is invalid");

	invalid = transfer;
	invalid.transferId++;
	CHECK(!SameCoopCampaignSyncTransfer(transfer, invalid),
		"transfer ids prevent checkpoint splicing");
	invalid = transfer;
	invalid.campaignSeed++;
	CHECK(!SameCoopCampaignSyncTransfer(transfer, invalid),
		"campaign seed participates in exact identity");
	invalid = transfer;
	invalid.campaignIdentitySha256[0] ^= 1;
	CHECK(!SameCoopCampaignSyncTransfer(transfer, invalid),
		"campaign identity digest participates in exact transfer identity");
	invalid = transfer;
	invalid.checkpointSha256[0] ^= 1;
	CHECK(!SameCoopCampaignSyncTransfer(transfer, invalid),
		"checkpoint digest participates in exact identity");
}

void TestMetadataCodec()
{
	CoopCampaignSyncMetadata metadata;
	metadata.transfer = Transfer();
	metadata.worldMinutes = UINT64_C(0x4847464544434241);
	CoopCampaignSyncMetadataBytes bytes{};
	CHECK(EncodeCoopCampaignSyncMetadata(metadata, bytes) ==
		CoopCampaignSyncCodecResult::Success, "metadata encodes");
	CheckCommonGolden(bytes.data(), CoopCampaignSyncMessageKind::Metadata,
		metadata.transfer);
	CHECK(ReadU64(bytes.data(), 128) == metadata.worldMinutes,
		"world minutes is little endian");

	CoopCampaignSyncMetadata decoded;
	CHECK(DecodeCoopCampaignSyncMetadata(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success, "metadata golden decodes");
	CHECK(SameTransfer(decoded.transfer, metadata.transfer) &&
		decoded.worldMinutes == metadata.worldMinutes,
		"metadata round trip preserves all fields");

	CoopCampaignSyncMetadata sentinel;
	sentinel.transfer.transferId = 91;
	sentinel.worldMinutes = 92;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignSyncMetadata output = sentinel;
		CHECK(DecodeCoopCampaignSyncMetadata(bytes.data(), size, output) !=
			CoopCampaignSyncCodecResult::Success,
			"every truncated metadata length is rejected");
		CHECK(output.transfer.transferId == 91 && output.worldMinutes == 92,
			"failed metadata decode is transactional");
	}
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CoopCampaignSyncMetadata output = sentinel;
	CHECK(DecodeCoopCampaignSyncMetadata(
		extended.data(), extended.size(), output) ==
		CoopCampaignSyncCodecResult::Invalid,
		"extended metadata is rejected");
	CHECK(output.worldMinutes == 92,
		"extended metadata does not publish output");

	auto malformed = bytes;
	malformed[4] = 2;
	CHECK(DecodeCoopCampaignSyncMetadata(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::UnsupportedVersion,
		"unsupported campaign wire version is explicit");
	malformed = bytes;
	malformed[6] = static_cast<std::uint8_t>(CurrentProtocolVersion + 1);
	CHECK(DecodeCoopCampaignSyncMetadata(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::UnsupportedVersion,
		"unsupported session protocol is explicit");
	malformed = bytes;
	malformed[8] = static_cast<std::uint8_t>(
		CoopCampaignSyncMessageKind::Chunk);
	CHECK(DecodeCoopCampaignSyncMetadata(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::WrongMessageKind,
		"wrong metadata message kind is rejected");
	constexpr std::size_t commonReserved[] = {
		9, 10, 11, 12, 13, 14, 15, 124, 125, 126, 127};
	for (std::size_t reserved : commonReserved)
	{
		malformed = bytes;
		malformed[reserved] = 1;
		CHECK(DecodeCoopCampaignSyncMetadata(
			malformed.data(), malformed.size(), decoded) ==
			CoopCampaignSyncCodecResult::Invalid,
			"every common reserved byte must be zero");
	}
	malformed = bytes;
	std::fill(malformed.begin() + 40, malformed.begin() + 72, 0);
	CHECK(DecodeCoopCampaignSyncMetadata(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"zero campaign identity SHA is rejected on decode");
	malformed = bytes;
	WriteU64(malformed.data(), 72, 0);
	CHECK(DecodeCoopCampaignSyncMetadata(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"zero checkpoint generation is rejected on decode");
	malformed = bytes;
	WriteU64(malformed.data(), 80, MaximumCoopCampaignCheckpointBytes + 1);
	CHECK(DecodeCoopCampaignSyncMetadata(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"oversized checkpoint is rejected on decode");
	malformed = bytes;
	std::fill(malformed.begin() + 88, malformed.begin() + 120, 0);
	CHECK(DecodeCoopCampaignSyncMetadata(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"zero checkpoint SHA is rejected on decode");

	CoopCampaignSyncMetadata invalid = metadata;
	invalid.transfer.checkpointGeneration = 0;
	CoopCampaignSyncMetadataBytes unchanged{};
	unchanged.fill(0xa5);
	CHECK(EncodeCoopCampaignSyncMetadata(invalid, unchanged) ==
		CoopCampaignSyncCodecResult::Invalid,
		"invalid metadata does not encode");
	CHECK(std::all_of(unchanged.begin(), unchanged.end(),
		[](std::uint8_t byte) { return byte == 0xa5; }),
		"failed metadata encode preserves output");
}

void TestChunkCodecAndBounds()
{
	CoopCampaignSyncChunk chunk;
	chunk.transfer = Transfer(3);
	chunk.payload = {'a', 'b', 'c'};
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopCampaignSyncChunk(chunk, bytes) ==
		CoopCampaignSyncCodecResult::Success, "small final chunk encodes");
	CHECK(bytes.size() == CoopCampaignSyncChunkHeaderWireSize + 3,
		"chunk wire size is exact");
	CheckCommonGolden(bytes.data(), CoopCampaignSyncMessageKind::Chunk,
		chunk.transfer);
	CHECK(ReadU64(bytes.data(), 128) == 0,
		"chunk offset is little endian");
	CHECK(ReadU32(bytes.data(), 136) == 3,
		"chunk payload length is little endian");
	CHECK(ReadU32(bytes.data(), 140) == UINT32_C(0x1a47e90b),
		"chunk payload checksum is golden");
	CHECK(std::equal(bytes.begin() + CoopCampaignSyncChunkHeaderWireSize,
		bytes.end(), chunk.payload.begin()), "chunk payload is byte exact");

	CoopCampaignSyncChunk decoded;
	CHECK(DecodeCoopCampaignSyncChunk(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success, "chunk golden decodes");
	CHECK(SameTransfer(decoded.transfer, chunk.transfer) &&
		decoded.offset == 0 && decoded.payload == chunk.payload &&
		decoded.payloadChecksum == UINT32_C(0x1a47e90b),
		"chunk round trip preserves descriptor and payload");

	CoopCampaignSyncChunk sentinel;
	sentinel.transfer.transferId = 77;
	sentinel.offset = 78;
	sentinel.payload = {79};
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignSyncChunk output = sentinel;
		CHECK(DecodeCoopCampaignSyncChunk(bytes.data(), size, output) !=
			CoopCampaignSyncCodecResult::Success,
			"every truncated chunk length is rejected");
		CHECK(output.transfer.transferId == 77 && output.offset == 78 &&
			output.payload == sentinel.payload,
			"failed chunk decode is transactional");
	}

	auto malformed = bytes;
	malformed.push_back(0);
	CoopCampaignSyncChunk output = sentinel;
	CHECK(DecodeCoopCampaignSyncChunk(
		malformed.data(), malformed.size(), output) ==
		CoopCampaignSyncCodecResult::Invalid,
		"chunk trailing bytes are rejected");
	CHECK(output.offset == 78, "extended chunk preserves output");
	malformed = bytes;
	malformed.back() ^= 0x80;
	CHECK(DecodeCoopCampaignSyncChunk(
		malformed.data(), malformed.size(), output) ==
		CoopCampaignSyncCodecResult::ChecksumMismatch,
		"corrupt chunk payload is rejected by checksum");
	CHECK(output.offset == 78, "checksum failure preserves chunk output");
	malformed = bytes;
	malformed[140] ^= 1;
	CHECK(DecodeCoopCampaignSyncChunk(
		malformed.data(), malformed.size(), output) ==
		CoopCampaignSyncCodecResult::ChecksumMismatch,
		"corrupt carried checksum is rejected");
	malformed = bytes;
	WriteU32(malformed.data(), 136, 2);
	CHECK(DecodeCoopCampaignSyncChunk(
		malformed.data(), malformed.size(), output) ==
		CoopCampaignSyncCodecResult::Invalid,
		"declared chunk length must exactly match frame");
	malformed = bytes;
	WriteU64(malformed.data(), 128, 1);
	CHECK(DecodeCoopCampaignSyncChunk(
		malformed.data(), malformed.size(), output) ==
		CoopCampaignSyncCodecResult::Invalid,
		"unaligned chunk offset is noncanonical");

	CoopCampaignSyncChunk invalid = chunk;
	invalid.payloadChecksum = 1;
	std::vector<std::uint8_t> unchanged{0xa5};
	CHECK(EncodeCoopCampaignSyncChunk(invalid, unchanged) ==
		CoopCampaignSyncCodecResult::ChecksumMismatch,
		"supplied wrong checksum is rejected by encoder");
	CHECK(unchanged == std::vector<std::uint8_t>{0xa5},
		"failed chunk encode preserves vector output");
	invalid = chunk;
	invalid.transfer.totalSize =
		static_cast<std::uint64_t>(CoopCampaignSyncCanonicalChunkBytes) + 1;
	CHECK(EncodeCoopCampaignSyncChunk(invalid, unchanged) ==
		CoopCampaignSyncCodecResult::Invalid,
		"short non-final chunk is rejected");
	invalid = chunk;
	invalid.transfer.totalSize = MaximumCoopCampaignCheckpointBytes;
	invalid.payload.assign(CoopCampaignSyncCanonicalChunkBytes + 1, 0x44);
	CHECK(EncodeCoopCampaignSyncChunk(invalid, unchanged) ==
		CoopCampaignSyncCodecResult::PayloadTooLarge,
		"chunk payload over 60 KiB is rejected");

	CoopCampaignSyncChunk maximum;
	maximum.transfer = Transfer(MaximumCoopCampaignCheckpointBytes);
	maximum.payload.assign(CoopCampaignSyncCanonicalChunkBytes, 0x5a);
	CHECK(EncodeCoopCampaignSyncChunk(maximum, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"maximum non-final payload encodes");
	CHECK(bytes.size() == MaximumCoopCampaignSyncWireSize,
		"maximum wire message remains below 64 KiB");
	CHECK(DecodeCoopCampaignSyncChunk(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success &&
		decoded.payload.size() == CoopCampaignSyncCanonicalChunkBytes,
		"maximum payload decodes");

	const std::uint64_t finalOffset =
		((MaximumCoopCampaignCheckpointBytes - 1) /
			CoopCampaignSyncCanonicalChunkBytes) *
		CoopCampaignSyncCanonicalChunkBytes;
	maximum.offset = finalOffset;
	maximum.payload.assign(static_cast<std::size_t>(
		maximum.transfer.totalSize - finalOffset), 0x6b);
	CHECK(!maximum.payload.empty() &&
		maximum.payload.size() <= CoopCampaignSyncCanonicalChunkBytes,
		"256 MiB final chunk fixture has bounded remainder");
	CHECK(EncodeCoopCampaignSyncChunk(maximum, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"exact checkpoint ceiling final chunk encodes");
	CHECK(DecodeCoopCampaignSyncChunk(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success &&
		decoded.offset == finalOffset && decoded.payload == maximum.payload,
		"exact checkpoint ceiling final chunk decodes");
}

void TestChunkSequenceValidation()
{
	CoopCampaignSyncMetadata metadata;
	metadata.transfer = Transfer(
		static_cast<std::uint64_t>(CoopCampaignSyncCanonicalChunkBytes) + 3);
	CoopCampaignSyncChunk first;
	first.transfer = metadata.transfer;
	first.payload.assign(CoopCampaignSyncCanonicalChunkBytes, 0x31);
	first.payloadChecksum = CoopCampaignSyncPayloadChecksum(
		first.payload.data(), first.payload.size());
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, first, 0) ==
		CoopCampaignSyncChunkSequenceResult::Success,
		"exact next chunk validates");
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, first,
		CoopCampaignSyncCanonicalChunkBytes) ==
		CoopCampaignSyncChunkSequenceResult::Overlap,
		"already-covered chunk is overlap and is never reapplied");

	CoopCampaignSyncChunk final;
	final.transfer = metadata.transfer;
	final.offset = CoopCampaignSyncCanonicalChunkBytes;
	final.payload = {1, 2, 3};
	final.payloadChecksum = CoopCampaignSyncPayloadChecksum(
		final.payload.data(), final.payload.size());
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, final, 0) ==
		CoopCampaignSyncChunkSequenceResult::Gap,
		"future chunk is a gap and is never applied");
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, final,
		CoopCampaignSyncCanonicalChunkBytes) ==
		CoopCampaignSyncChunkSequenceResult::Success,
		"canonical final chunk validates at exact cursor");

	CoopCampaignSyncChunk mismatch = first;
	mismatch.transfer.transferId++;
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, mismatch, 0) ==
		CoopCampaignSyncChunkSequenceResult::TransferMismatch,
		"a different transfer cannot splice chunks");
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, first, 1) ==
		CoopCampaignSyncChunkSequenceResult::InvalidExpectedOffset,
		"noncanonical expected cursor is rejected");
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, final,
		metadata.transfer.totalSize) ==
		CoopCampaignSyncChunkSequenceResult::InvalidExpectedOffset,
		"no chunk may apply after total-size cursor");

	CoopCampaignSyncChunk malformed = first;
	malformed.payload.pop_back();
	malformed.payloadChecksum = CoopCampaignSyncPayloadChecksum(
		malformed.payload.data(), malformed.payload.size());
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, malformed, 0) ==
		CoopCampaignSyncChunkSequenceResult::InvalidChunk,
		"short non-final chunk is invalid");
	malformed = first;
	malformed.payloadChecksum ^= 1;
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(metadata, malformed, 0) ==
		CoopCampaignSyncChunkSequenceResult::ChecksumMismatch,
		"sequence validation independently checks payload checksum");

	CoopCampaignSyncMetadata invalidMetadata = metadata;
	invalidMetadata.transfer.checkpointGeneration = 0;
	CHECK(ValidateCoopCampaignSyncChunkAtOffset(
		invalidMetadata, first, 0) ==
		CoopCampaignSyncChunkSequenceResult::InvalidMetadata,
		"invalid metadata cannot seed a chunk sequence");
}

void TestAckCodec()
{
	CoopCampaignSyncAck acknowledgement;
	acknowledgement.transfer = Transfer(3);
	acknowledgement.peerIdentity = Peer(0x50);
	acknowledgement.nextExpectedOffset = 3;
	acknowledgement.precedingChunkChecksum = UINT32_C(0x1a47e90b);
	CoopCampaignSyncAckBytes bytes{};
	CHECK(EncodeCoopCampaignSyncAck(acknowledgement, bytes) ==
		CoopCampaignSyncCodecResult::Success, "cumulative ACK encodes");
	CheckCommonGolden(bytes.data(), CoopCampaignSyncMessageKind::Ack,
		acknowledgement.transfer);
	for (std::size_t index = 0; index < acknowledgement.peerIdentity.size();
		++index)
		CHECK(bytes[128 + index] == acknowledgement.peerIdentity[index],
			"ACK peer identity occupies exact bytes");
	CHECK(ReadU64(bytes.data(), 144) == 3,
		"ACK cursor is little endian");
	CHECK(ReadU32(bytes.data(), 152) == UINT32_C(0x1a47e90b),
		"ACK preceding checksum is little endian");
	for (std::size_t index = 156; index < 160; ++index)
		CHECK(bytes[index] == 0, "ACK reserve is zero");

	CoopCampaignSyncAck decoded;
	CHECK(DecodeCoopCampaignSyncAck(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success, "ACK golden decodes");
	CHECK(SameTransfer(decoded.transfer, acknowledgement.transfer) &&
		decoded.peerIdentity == acknowledgement.peerIdentity &&
		decoded.nextExpectedOffset == 3 &&
		decoded.precedingChunkChecksum == UINT32_C(0x1a47e90b),
		"ACK round trip preserves cumulative state");

	CoopCampaignSyncAck sentinel;
	sentinel.transfer.transferId = 71;
	sentinel.nextExpectedOffset = 72;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignSyncAck output = sentinel;
		CHECK(DecodeCoopCampaignSyncAck(bytes.data(), size, output) !=
			CoopCampaignSyncCodecResult::Success,
			"every truncated ACK length is rejected");
		CHECK(output.transfer.transferId == 71 &&
			output.nextExpectedOffset == 72,
			"failed ACK decode is transactional");
	}

	CoopCampaignSyncAck zero = acknowledgement;
	zero.nextExpectedOffset = 0;
	zero.precedingChunkChecksum = 0;
	CHECK(EncodeCoopCampaignSyncAck(zero, bytes) ==
		CoopCampaignSyncCodecResult::Success, "initial ACK(0) encodes");
	zero.precedingChunkChecksum = 1;
	CHECK(EncodeCoopCampaignSyncAck(zero, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"ACK(0) requires zero preceding checksum");
	CoopCampaignSyncAck invalid = acknowledgement;
	invalid.peerIdentity.fill(0);
	CHECK(EncodeCoopCampaignSyncAck(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"anonymous ACK is rejected");
	invalid = acknowledgement;
	invalid.transfer.totalSize =
		static_cast<std::uint64_t>(CoopCampaignSyncCanonicalChunkBytes) * 4 + 3;
	invalid.nextExpectedOffset = 1;
	CHECK(EncodeCoopCampaignSyncAck(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"mid-chunk cumulative ACK is rejected");
	invalid.nextExpectedOffset =
		static_cast<std::uint64_t>(CoopCampaignSyncCanonicalChunkBytes) * 3;
	CHECK(EncodeCoopCampaignSyncAck(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"one ACK may cumulatively cover the fixed three-chunk window");
	for (std::size_t reserved = 156; reserved < 160; ++reserved)
	{
		auto malformed = bytes;
		malformed[reserved] = 1;
		CHECK(DecodeCoopCampaignSyncAck(
			malformed.data(), malformed.size(), decoded) ==
			CoopCampaignSyncCodecResult::Invalid,
			"every ACK reserved byte must be zero");
	}
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CHECK(DecodeCoopCampaignSyncAck(
		extended.data(), extended.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"extended ACK is rejected");
}

void TestCompletionCodec()
{
	CoopCampaignSyncComplete completion;
	completion.transfer = Transfer(3);
	CoopCampaignSyncCompleteBytes bytes{};
	CHECK(EncodeCoopCampaignSyncComplete(completion, bytes) ==
		CoopCampaignSyncCodecResult::Success, "completion encodes");
	CheckCommonGolden(bytes.data(), CoopCampaignSyncMessageKind::Complete,
		completion.transfer);
	CoopCampaignSyncComplete decoded;
	CHECK(DecodeCoopCampaignSyncComplete(
		bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success &&
		SameTransfer(decoded.transfer, completion.transfer),
		"completion round trip preserves exact checkpoint identity");

	CoopCampaignSyncComplete sentinel;
	sentinel.transfer.transferId = 81;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignSyncComplete output = sentinel;
		CHECK(DecodeCoopCampaignSyncComplete(bytes.data(), size, output) !=
			CoopCampaignSyncCodecResult::Success,
			"every truncated completion length is rejected");
		CHECK(output.transfer.transferId == 81,
			"failed completion decode is transactional");
	}
	auto malformed = bytes;
	malformed[9] = 1;
	CHECK(DecodeCoopCampaignSyncComplete(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"completion common reserve must be zero");
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CHECK(DecodeCoopCampaignSyncComplete(
		extended.data(), extended.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"extended completion is rejected");
}

void TestResultCodec()
{
	CoopCampaignSyncResult result;
	result.transfer = Transfer(3);
	result.peerIdentity = Peer(0x60);
	result.status = CoopCampaignSyncResultStatus::Committed;
	result.reason = CoopCampaignSyncFailureReason::None;
	CoopCampaignSyncResultBytes bytes{};
	CHECK(EncodeCoopCampaignSyncResult(result, bytes) ==
		CoopCampaignSyncCodecResult::Success, "committed result encodes");
	CheckCommonGolden(bytes.data(), CoopCampaignSyncMessageKind::Result,
		result.transfer);
	CHECK(bytes[144] == 1 && bytes[145] == 0,
		"committed status and reason bytes are exact");
	for (std::size_t index = 146; index < 152; ++index)
		CHECK(bytes[index] == 0, "result reserve is zero");

	CoopCampaignSyncResult decoded;
	CHECK(DecodeCoopCampaignSyncResult(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success, "committed result decodes");
	CHECK(SameTransfer(decoded.transfer, result.transfer) &&
		decoded.peerIdentity == result.peerIdentity &&
		decoded.status == CoopCampaignSyncResultStatus::Committed &&
		decoded.reason == CoopCampaignSyncFailureReason::None,
		"committed result round trip is exact");

	CoopCampaignSyncResult rejected = result;
	rejected.status = CoopCampaignSyncResultStatus::Rejected;
	rejected.reason = CoopCampaignSyncFailureReason::LoadFailed;
	CHECK(EncodeCoopCampaignSyncResult(rejected, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"cold-checkpoint load failure result encodes");
	CHECK(DecodeCoopCampaignSyncResult(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success &&
		decoded.reason == CoopCampaignSyncFailureReason::LoadFailed,
		"cold-checkpoint load failure result decodes");

	CoopCampaignSyncResult invalid = result;
	invalid.reason = CoopCampaignSyncFailureReason::HashMismatch;
	CHECK(EncodeCoopCampaignSyncResult(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"committed result cannot carry a failure reason");
	invalid = rejected;
	invalid.reason = CoopCampaignSyncFailureReason::None;
	CHECK(EncodeCoopCampaignSyncResult(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"rejected result requires a known failure reason");
	invalid = result;
	invalid.peerIdentity.fill(0);
	CHECK(EncodeCoopCampaignSyncResult(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"anonymous result is rejected");

	CHECK(EncodeCoopCampaignSyncResult(rejected, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"rejected fixture re-encodes");
	CoopCampaignSyncResult sentinel;
	sentinel.transfer.transferId = 91;
	sentinel.reason = CoopCampaignSyncFailureReason::StorageFailure;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignSyncResult output = sentinel;
		CHECK(DecodeCoopCampaignSyncResult(bytes.data(), size, output) !=
			CoopCampaignSyncCodecResult::Success,
			"every truncated result length is rejected");
		CHECK(output.transfer.transferId == 91 &&
			output.reason == CoopCampaignSyncFailureReason::StorageFailure,
			"failed result decode is transactional");
	}
	auto malformed = bytes;
	malformed[144] = 0xff;
	CHECK(DecodeCoopCampaignSyncResult(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"unknown result status is rejected");
	malformed = bytes;
	malformed[145] = 0xff;
	CHECK(DecodeCoopCampaignSyncResult(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"unknown result reason is rejected");
	for (std::size_t reserved = 146; reserved < 152; ++reserved)
	{
		malformed = bytes;
		malformed[reserved] = 1;
		CHECK(DecodeCoopCampaignSyncResult(
			malformed.data(), malformed.size(), decoded) ==
			CoopCampaignSyncCodecResult::Invalid,
			"every result reserved byte must be zero");
	}
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CHECK(DecodeCoopCampaignSyncResult(
		extended.data(), extended.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"extended result is rejected");
}

void TestResyncCodec()
{
	CoopCampaignSyncResync resync;
	resync.transfer = Transfer(
		static_cast<std::uint64_t>(CoopCampaignSyncCanonicalChunkBytes) + 3);
	resync.peerIdentity = Peer(0x70);
	resync.expectedOffset = CoopCampaignSyncCanonicalChunkBytes;
	resync.precedingChunkChecksum = UINT32_C(0x44332211);
	resync.reason = CoopCampaignSyncFailureReason::SequenceMismatch;
	CoopCampaignSyncResyncBytes bytes{};
	CHECK(EncodeCoopCampaignSyncResync(resync, bytes) ==
		CoopCampaignSyncCodecResult::Success, "resync request encodes");
	CheckCommonGolden(bytes.data(), CoopCampaignSyncMessageKind::Resync,
		resync.transfer);
	CHECK(ReadU64(bytes.data(), 144) == resync.expectedOffset &&
		ReadU32(bytes.data(), 152) == resync.precedingChunkChecksum &&
		bytes[156] == static_cast<std::uint8_t>(resync.reason),
		"resync cursor, checksum, and reason bytes are exact");
	for (std::size_t index = 157; index < 160; ++index)
		CHECK(bytes[index] == 0, "resync reserve is zero");

	CoopCampaignSyncResync decoded;
	CHECK(DecodeCoopCampaignSyncResync(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success, "resync golden decodes");
	CHECK(SameTransfer(decoded.transfer, resync.transfer) &&
		decoded.peerIdentity == resync.peerIdentity &&
		decoded.expectedOffset == resync.expectedOffset &&
		decoded.precedingChunkChecksum == resync.precedingChunkChecksum &&
		decoded.reason == resync.reason,
		"resync round trip preserves exact cursor");

	CoopCampaignSyncResync initial = resync;
	initial.expectedOffset = 0;
	initial.precedingChunkChecksum = 0;
	initial.reason = CoopCampaignSyncFailureReason::ChunkChecksumMismatch;
	CHECK(EncodeCoopCampaignSyncResync(initial, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"resync to the beginning is canonical");
	initial.precedingChunkChecksum = 1;
	CHECK(EncodeCoopCampaignSyncResync(initial, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"resync cursor zero requires zero preceding checksum");
	CoopCampaignSyncResync invalid = resync;
	invalid.reason = CoopCampaignSyncFailureReason::None;
	CHECK(EncodeCoopCampaignSyncResync(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"resync requires a failure reason");
	invalid = resync;
	invalid.expectedOffset = 1;
	CHECK(EncodeCoopCampaignSyncResync(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"resync cursor must be canonical");

	CHECK(EncodeCoopCampaignSyncResync(resync, bytes) ==
		CoopCampaignSyncCodecResult::Success, "resync fixture re-encodes");
	CoopCampaignSyncResync sentinel;
	sentinel.transfer.transferId = 101;
	sentinel.expectedOffset = 102;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignSyncResync output = sentinel;
		CHECK(DecodeCoopCampaignSyncResync(bytes.data(), size, output) !=
			CoopCampaignSyncCodecResult::Success,
			"every truncated resync length is rejected");
		CHECK(output.transfer.transferId == 101 &&
			output.expectedOffset == 102,
			"failed resync decode is transactional");
	}
	for (std::size_t reserved = 157; reserved < 160; ++reserved)
	{
		auto malformed = bytes;
		malformed[reserved] = 1;
		CHECK(DecodeCoopCampaignSyncResync(
			malformed.data(), malformed.size(), decoded) ==
			CoopCampaignSyncCodecResult::Invalid,
			"every resync reserved byte must be zero");
	}
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CHECK(DecodeCoopCampaignSyncResync(
		extended.data(), extended.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"extended resync is rejected");
}

void TestRejectCodec()
{
	CoopCampaignSyncReject rejection;
	rejection.transfer = Transfer(3);
	rejection.reason = CoopCampaignSyncFailureReason::Superseded;
	CoopCampaignSyncRejectBytes bytes{};
	CHECK(EncodeCoopCampaignSyncReject(rejection, bytes) ==
		CoopCampaignSyncCodecResult::Success, "terminal reject encodes");
	CheckCommonGolden(bytes.data(), CoopCampaignSyncMessageKind::Reject,
		rejection.transfer);
	CHECK(bytes[128] == static_cast<std::uint8_t>(
		CoopCampaignSyncFailureReason::Superseded),
		"reject reason byte is exact");
	for (std::size_t index = 129; index < 136; ++index)
		CHECK(bytes[index] == 0, "reject reserve is zero");

	CoopCampaignSyncReject decoded;
	CHECK(DecodeCoopCampaignSyncReject(bytes.data(), bytes.size(), decoded) ==
		CoopCampaignSyncCodecResult::Success &&
		SameTransfer(decoded.transfer, rejection.transfer) &&
		decoded.reason == rejection.reason,
		"terminal reject round trip is exact");

	CoopCampaignSyncReject invalid = rejection;
	invalid.reason = CoopCampaignSyncFailureReason::None;
	CHECK(EncodeCoopCampaignSyncReject(invalid, bytes) ==
		CoopCampaignSyncCodecResult::Invalid,
		"terminal reject requires a reason");
	CHECK(EncodeCoopCampaignSyncReject(rejection, bytes) ==
		CoopCampaignSyncCodecResult::Success, "reject fixture re-encodes");
	CoopCampaignSyncReject sentinel;
	sentinel.transfer.transferId = 111;
	sentinel.reason = CoopCampaignSyncFailureReason::LoadFailed;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		CoopCampaignSyncReject output = sentinel;
		CHECK(DecodeCoopCampaignSyncReject(bytes.data(), size, output) !=
			CoopCampaignSyncCodecResult::Success,
			"every truncated reject length is rejected");
		CHECK(output.transfer.transferId == 111 &&
			output.reason == CoopCampaignSyncFailureReason::LoadFailed,
			"failed reject decode is transactional");
	}
	auto malformed = bytes;
	malformed[128] = 0xff;
	CHECK(DecodeCoopCampaignSyncReject(
		malformed.data(), malformed.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"unknown reject reason is rejected");
	for (std::size_t reserved = 129; reserved < 136; ++reserved)
	{
		malformed = bytes;
		malformed[reserved] = 1;
		CHECK(DecodeCoopCampaignSyncReject(
			malformed.data(), malformed.size(), decoded) ==
			CoopCampaignSyncCodecResult::Invalid,
			"every reject reserved byte must be zero");
	}
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CHECK(DecodeCoopCampaignSyncReject(
		extended.data(), extended.size(), decoded) ==
		CoopCampaignSyncCodecResult::Invalid,
		"extended reject is rejected");
}

std::uint32_t NextRandom(std::uint32_t& state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return state;
}

void TestDeterministicMalformedFuzz()
{
	std::uint32_t random = UINT32_C(0x6d2b79f5);
	const std::array<std::size_t, 10> interestingSizes{{
		0, 1, CoopCampaignSyncCommonHeaderWireSize - 1,
		CoopCampaignSyncCommonHeaderWireSize,
		CoopCampaignSyncMetadataWireSize,
		CoopCampaignSyncChunkHeaderWireSize,
		CoopCampaignSyncResultWireSize, CoopCampaignSyncAckWireSize,
		255, 511}};
	for (std::size_t iteration = 0; iteration < 2048; ++iteration)
	{
		const std::size_t size = iteration < interestingSizes.size()
			? interestingSizes[iteration]
			: static_cast<std::size_t>(NextRandom(random) % 512);
		std::vector<std::uint8_t> bytes(size);
		for (std::uint8_t& byte : bytes)
			byte = static_cast<std::uint8_t>(NextRandom(random));

		CoopCampaignSyncMetadata metadata;
		metadata.transfer.transferId = 201;
		metadata.worldMinutes = 202;
		const CoopCampaignSyncCodecResult metadataResult =
			DecodeCoopCampaignSyncMetadata(bytes.data(), bytes.size(), metadata);
		if (metadataResult != CoopCampaignSyncCodecResult::Success)
			CHECK(metadata.transfer.transferId == 201 &&
				metadata.worldMinutes == 202,
				"fuzzed metadata failure preserves output");

		CoopCampaignSyncChunk chunk;
		chunk.transfer.transferId = 203;
		chunk.offset = 204;
		chunk.payload = {205};
		const CoopCampaignSyncCodecResult chunkResult =
			DecodeCoopCampaignSyncChunk(bytes.data(), bytes.size(), chunk);
		if (chunkResult != CoopCampaignSyncCodecResult::Success)
			CHECK(chunk.transfer.transferId == 203 && chunk.offset == 204 &&
				chunk.payload == std::vector<std::uint8_t>{205},
				"fuzzed chunk failure preserves output");

		CoopCampaignSyncAck acknowledgement;
		acknowledgement.transfer.transferId = 206;
		acknowledgement.nextExpectedOffset = 207;
		const CoopCampaignSyncCodecResult ackResult = DecodeCoopCampaignSyncAck(
			bytes.data(), bytes.size(), acknowledgement);
		if (ackResult != CoopCampaignSyncCodecResult::Success)
			CHECK(acknowledgement.transfer.transferId == 206 &&
				acknowledgement.nextExpectedOffset == 207,
				"fuzzed ACK failure preserves output");

		CoopCampaignSyncComplete completion;
		completion.transfer.transferId = 208;
		const CoopCampaignSyncCodecResult completeResult =
			DecodeCoopCampaignSyncComplete(
				bytes.data(), bytes.size(), completion);
		if (completeResult != CoopCampaignSyncCodecResult::Success)
			CHECK(completion.transfer.transferId == 208,
				"fuzzed completion failure preserves output");

		CoopCampaignSyncResult result;
		result.transfer.transferId = 209;
		result.reason = CoopCampaignSyncFailureReason::StorageFailure;
		const CoopCampaignSyncCodecResult resultResult =
			DecodeCoopCampaignSyncResult(bytes.data(), bytes.size(), result);
		if (resultResult != CoopCampaignSyncCodecResult::Success)
			CHECK(result.transfer.transferId == 209 &&
				result.reason == CoopCampaignSyncFailureReason::StorageFailure,
				"fuzzed result failure preserves output");

		CoopCampaignSyncResync resync;
		resync.transfer.transferId = 210;
		resync.expectedOffset = 211;
		const CoopCampaignSyncCodecResult resyncResult =
			DecodeCoopCampaignSyncResync(bytes.data(), bytes.size(), resync);
		if (resyncResult != CoopCampaignSyncCodecResult::Success)
			CHECK(resync.transfer.transferId == 210 &&
				resync.expectedOffset == 211,
				"fuzzed resync failure preserves output");

		CoopCampaignSyncReject rejection;
		rejection.transfer.transferId = 212;
		rejection.reason = CoopCampaignSyncFailureReason::LoadFailed;
		const CoopCampaignSyncCodecResult rejectResult =
			DecodeCoopCampaignSyncReject(bytes.data(), bytes.size(), rejection);
		if (rejectResult != CoopCampaignSyncCodecResult::Success)
			CHECK(rejection.transfer.transferId == 212 &&
				rejection.reason == CoopCampaignSyncFailureReason::LoadFailed,
				"fuzzed reject failure preserves output");
	}
}
}

int main()
{
	TestNamesAndChecksum();
	TestCampaignIdentityDigest();
	TestTransferValidation();
	TestMetadataCodec();
	TestChunkCodecAndBounds();
	TestChunkSequenceValidation();
	TestAckCodec();
	TestCompletionCodec();
	TestResultCodec();
	TestResyncCodec();
	TestRejectCodec();
	TestDeterministicMalformedFuzz();
	if (failures != 0)
	{
		std::printf("%d coop campaign sync protocol test(s) failed\n", failures);
		return 1;
	}
	std::puts("coop campaign sync protocol tests passed");
	return 0;
}
