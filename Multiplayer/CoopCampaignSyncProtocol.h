#ifndef MULTIPLAYER_COOP_CAMPAIGN_SYNC_PROTOCOL_H
#define MULTIPLAYER_COOP_CAMPAIGN_SYNC_PROTOCOL_H

#include "CoopSessionProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace CoopSession
{
inline constexpr char CoopCampaignSyncMetadataMessageName[] =
	"coop.campaign.sync.metadata";
inline constexpr char CoopCampaignSyncChunkMessageName[] =
	"coop.campaign.sync.chunk";
inline constexpr char CoopCampaignSyncAckMessageName[] =
	"coop.campaign.sync.ack";
inline constexpr char CoopCampaignSyncCompleteMessageName[] =
	"coop.campaign.sync.complete";
inline constexpr char CoopCampaignSyncResultMessageName[] =
	"coop.campaign.sync.result";
inline constexpr char CoopCampaignSyncResyncMessageName[] =
	"coop.campaign.sync.resync";
inline constexpr char CoopCampaignSyncRejectMessageName[] =
	"coop.campaign.sync.reject";

inline constexpr std::uint16_t CoopCampaignSyncWireVersion = 1;
inline constexpr std::uint64_t MaximumCoopCampaignCheckpointBytes =
	UINT64_C(256) * 1024u * 1024u;
inline constexpr std::uint32_t CoopCampaignSyncCanonicalChunkBytes =
	60u * 1024u;
inline constexpr std::size_t MaximumCoopCampaignSyncChunkWindow = 3;
inline constexpr std::size_t MaximumCoopCampaignIdentityIdBytes = 48;
inline constexpr char CoopCampaignIdentitySha256Domain[] =
	"JA2-COOP-CAMPAIGN-IDENTITY-V1";
inline constexpr std::size_t CoopCampaignSyncCommonHeaderWireSize = 128;
inline constexpr std::size_t CoopCampaignSyncMetadataWireSize = 136;
inline constexpr std::size_t CoopCampaignSyncChunkHeaderWireSize = 144;
inline constexpr std::size_t CoopCampaignSyncAckWireSize = 160;
inline constexpr std::size_t CoopCampaignSyncCompleteWireSize = 128;
inline constexpr std::size_t CoopCampaignSyncResultWireSize = 152;
inline constexpr std::size_t CoopCampaignSyncResyncWireSize = 160;
inline constexpr std::size_t CoopCampaignSyncRejectWireSize = 136;
inline constexpr std::size_t MaximumCoopCampaignSyncWireSize =
	CoopCampaignSyncChunkHeaderWireSize +
	CoopCampaignSyncCanonicalChunkBytes;
inline constexpr std::size_t CoopCampaignSyncTransportCeiling =
	64u * 1024u;
inline constexpr std::size_t MaximumCoopCampaignSyncWindowWireBytes =
	MaximumCoopCampaignSyncChunkWindow * MaximumCoopCampaignSyncWireSize;
static_assert(MaximumCoopCampaignSyncWireSize <
	CoopCampaignSyncTransportCeiling,
	"campaign checkpoint messages must remain below 64 KiB");
static_assert(MaximumCoopCampaignSyncWindowWireBytes < 256u * 1024u,
	"three maximum checkpoint chunks must fit the listener write budget");

using CoopCampaignCheckpointSha256 = std::array<std::uint8_t, 32>;
using CoopCampaignIdentitySha256 = std::array<std::uint8_t, 32>;

enum class CoopCampaignSyncMessageKind : std::uint8_t
{
	Metadata = 1,
	Chunk = 2,
	Ack = 3,
	Complete = 4,
	Result = 5,
	Resync = 6,
	Reject = 7
};

// This immutable identity is echoed by every message in one checkpoint
// transfer. A new transferId is required after reconnect or supersession.
struct CoopCampaignSyncTransferIdentity
{
	std::uint16_t wireVersion = CoopCampaignSyncWireVersion;
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	std::uint64_t transferId = 0;
	// Zero is a valid immutable campaign seed.
	std::uint64_t campaignSeed = 0;
	CoopCampaignIdentitySha256 campaignIdentitySha256{};
	std::uint64_t checkpointGeneration = 0;
	std::uint64_t totalSize = 0;
	CoopCampaignCheckpointSha256 checkpointSha256{};
	std::uint32_t canonicalChunkBytes =
		CoopCampaignSyncCanonicalChunkBytes;
};

// Describes a cold, immutable campaign checkpoint. The protocol deliberately
// does not depend on campaign-store or JA2 types.
struct CoopCampaignSyncMetadata
{
	CoopCampaignSyncTransferIdentity transfer;
	std::uint64_t worldMinutes = 0;
};

struct CoopCampaignSyncChunk
{
	CoopCampaignSyncTransferIdentity transfer;
	std::uint64_t offset = 0;
	// Zero asks the encoder to calculate the checksum. A decoded chunk always
	// contains the checksum carried on the wire.
	std::uint32_t payloadChecksum = 0;
	std::vector<std::uint8_t> payload;
};

// ACKs are cumulative. A sender may pipeline no more than
// MaximumCoopCampaignSyncChunkWindow sequential chunks beyond the last ACK;
// stop-and-wait is also valid. The checksum names the canonical chunk ending
// at nextExpectedOffset (and must be zero when that cursor is zero).
struct CoopCampaignSyncAck
{
	CoopCampaignSyncTransferIdentity transfer;
	PeerIdentity peerIdentity{};
	std::uint64_t nextExpectedOffset = 0;
	std::uint32_t precedingChunkChecksum = 0;
};

struct CoopCampaignSyncComplete
{
	CoopCampaignSyncTransferIdentity transfer;
};

enum class CoopCampaignSyncResultStatus : std::uint8_t
{
	Committed = 1,
	Rejected = 2
};

enum class CoopCampaignSyncFailureReason : std::uint8_t
{
	None = 0,
	MalformedMessage = 1,
	SessionMismatch = 2,
	TransferMismatch = 3,
	CheckpointMismatch = 4,
	SequenceMismatch = 5,
	ChunkChecksumMismatch = 6,
	HashMismatch = 7,
	StorageFailure = 8,
	LoadFailed = 9,
	CompatibilityMismatch = 10,
	CapacityReached = 11,
	Superseded = 12,
	ProtocolViolation = 13
};

// Committed is the client-side main-thread commit boundary: scratch bytes have
// been SHA-256 verified, atomically published, and the cold checkpoint has
// successfully loaded into the passive JA2 context. Only then may the server
// advance this peer to a tactical baseline.
struct CoopCampaignSyncResult
{
	CoopCampaignSyncTransferIdentity transfer;
	PeerIdentity peerIdentity{};
	CoopCampaignSyncResultStatus status =
		CoopCampaignSyncResultStatus::Rejected;
	CoopCampaignSyncFailureReason reason =
		CoopCampaignSyncFailureReason::ProtocolViolation;
};

struct CoopCampaignSyncResync
{
	CoopCampaignSyncTransferIdentity transfer;
	PeerIdentity peerIdentity{};
	std::uint64_t expectedOffset = 0;
	std::uint32_t precedingChunkChecksum = 0;
	CoopCampaignSyncFailureReason reason =
		CoopCampaignSyncFailureReason::SequenceMismatch;
};

struct CoopCampaignSyncReject
{
	CoopCampaignSyncTransferIdentity transfer;
	CoopCampaignSyncFailureReason reason =
		CoopCampaignSyncFailureReason::ProtocolViolation;
};

using CoopCampaignSyncMetadataBytes =
	std::array<std::uint8_t, CoopCampaignSyncMetadataWireSize>;
using CoopCampaignSyncAckBytes =
	std::array<std::uint8_t, CoopCampaignSyncAckWireSize>;
using CoopCampaignSyncCompleteBytes =
	std::array<std::uint8_t, CoopCampaignSyncCompleteWireSize>;
using CoopCampaignSyncResultBytes =
	std::array<std::uint8_t, CoopCampaignSyncResultWireSize>;
using CoopCampaignSyncResyncBytes =
	std::array<std::uint8_t, CoopCampaignSyncResyncWireSize>;
using CoopCampaignSyncRejectBytes =
	std::array<std::uint8_t, CoopCampaignSyncRejectWireSize>;

enum class CoopCampaignSyncCodecResult : std::uint8_t
{
	Success,
	Invalid,
	UnsupportedVersion,
	WrongMessageKind,
	PayloadTooLarge,
	ChecksumMismatch,
	AllocationFailure
};

enum class CoopCampaignSyncChunkSequenceResult : std::uint8_t
{
	Success,
	InvalidMetadata,
	TransferMismatch,
	InvalidExpectedOffset,
	Overlap,
	Gap,
	InvalidChunk,
	ChecksumMismatch
};

bool IsKnownCoopCampaignSyncMessageKind(
	CoopCampaignSyncMessageKind kind) noexcept;
bool IsKnownCoopCampaignSyncResultStatus(
	CoopCampaignSyncResultStatus status) noexcept;
bool IsKnownCoopCampaignSyncFailureReason(
	CoopCampaignSyncFailureReason reason) noexcept;
bool IsCanonicalCoopCampaignIdentityId(
	const std::string& campaignId) noexcept;
// SHA-256 over the exact domain bytes (excluding the C terminator), one u8
// campaign-id length, the exact lowercase-ASCII id bytes, then LE u64 seed.
// Invalid ids leave digest unchanged.
bool ComputeCoopCampaignIdentitySha256(const std::string& campaignId,
	std::uint64_t campaignSeed,
	CoopCampaignIdentitySha256& digest) noexcept;
bool IsValidCoopCampaignSyncTransferIdentity(
	const CoopCampaignSyncTransferIdentity& transfer) noexcept;
bool SameCoopCampaignSyncTransfer(
	const CoopCampaignSyncTransferIdentity& left,
	const CoopCampaignSyncTransferIdentity& right) noexcept;
bool IsCanonicalCoopCampaignSyncCursor(
	const CoopCampaignSyncTransferIdentity& transfer,
	std::uint64_t cursor) noexcept;

// FNV-1a over exactly size bytes. Null is accepted only for an empty input.
std::uint32_t CoopCampaignSyncPayloadChecksum(
	const std::uint8_t* bytes, std::size_t size) noexcept;

CoopCampaignSyncChunkSequenceResult ValidateCoopCampaignSyncChunkAtOffset(
	const CoopCampaignSyncMetadata& metadata,
	const CoopCampaignSyncChunk& chunk,
	std::uint64_t expectedOffset) noexcept;

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncMetadata(
	const CoopCampaignSyncMetadata& metadata,
	CoopCampaignSyncMetadataBytes& bytes) noexcept;
CoopCampaignSyncCodecResult DecodeCoopCampaignSyncMetadata(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncMetadata& metadata) noexcept;

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncChunk(
	const CoopCampaignSyncChunk& chunk,
	std::vector<std::uint8_t>& bytes) noexcept;
CoopCampaignSyncCodecResult DecodeCoopCampaignSyncChunk(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncChunk& chunk) noexcept;

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncAck(
	const CoopCampaignSyncAck& acknowledgement,
	CoopCampaignSyncAckBytes& bytes) noexcept;
CoopCampaignSyncCodecResult DecodeCoopCampaignSyncAck(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncAck& acknowledgement) noexcept;

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncComplete(
	const CoopCampaignSyncComplete& completion,
	CoopCampaignSyncCompleteBytes& bytes) noexcept;
CoopCampaignSyncCodecResult DecodeCoopCampaignSyncComplete(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncComplete& completion) noexcept;

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncResult(
	const CoopCampaignSyncResult& result,
	CoopCampaignSyncResultBytes& bytes) noexcept;
CoopCampaignSyncCodecResult DecodeCoopCampaignSyncResult(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncResult& result) noexcept;

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncResync(
	const CoopCampaignSyncResync& resync,
	CoopCampaignSyncResyncBytes& bytes) noexcept;
CoopCampaignSyncCodecResult DecodeCoopCampaignSyncResync(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncResync& resync) noexcept;

CoopCampaignSyncCodecResult EncodeCoopCampaignSyncReject(
	const CoopCampaignSyncReject& rejection,
	CoopCampaignSyncRejectBytes& bytes) noexcept;
CoopCampaignSyncCodecResult DecodeCoopCampaignSyncReject(
	const std::uint8_t* bytes, std::size_t size,
	CoopCampaignSyncReject& rejection) noexcept;
}

#endif
