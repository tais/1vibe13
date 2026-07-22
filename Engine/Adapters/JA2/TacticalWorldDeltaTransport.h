#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_TRANSPORT_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_DELTA_TRANSPORT_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Core/RuntimeMessageBus.h>

// Small deltas retain the original topic and codec bytes. Packages that want
// a lossless stream for deltas of every size subscribe to both topics and pass
// both through TacticalWorldDeltaReassembler.
inline constexpr char TacticalWorldDeltaMessageTopic[] =
	"ja2.tactical-world.delta";
inline constexpr char TacticalWorldDeltaChunkMessageTopic[] =
	"ja2.tactical-world.delta.chunk";
inline constexpr char TacticalWorldDeltaMessageSource[] =
	"ja2.runtime-adapter";
inline constexpr std::uint16_t TacticalWorldDeltaChunkWireVersion = 1;
inline constexpr std::size_t TacticalWorldDeltaChunkHeaderBytes = 38;
inline constexpr std::size_t MaximumTacticalWorldDeltaTransferBytes =
	16u * 1024u * 1024u;
inline constexpr std::size_t MaximumTacticalWorldDeltaTransferChunks = 4096;

enum class TacticalWorldDeltaChunkEncodeError
{
	None = 0,
	InvalidTransfer = 1,
	PayloadLimitTooSmall = 2,
	TransferTooLarge = 3,
	TooManyChunks = 4,
	AllocationFailure = 5
};

struct EncodedTacticalWorldDeltaChunks
{
	std::uint64_t transferId = 0;
	std::size_t totalPayloadBytes = 0;
	std::uint32_t checksum = 0;
	std::vector<std::vector<std::uint8_t>> payloads;

	explicit operator bool() const noexcept
	{
		return transferId != 0 && totalPayloadBytes != 0 && payloads.size() > 1;
	}
};

// Creates versioned message payloads from one already-encoded delta. Output is
// replaced only after the complete bounded batch has been constructed.
TacticalWorldDeltaChunkEncodeError EncodeTacticalWorldDeltaChunks(
	const std::vector<std::uint8_t>& encodedDelta,
	std::uint64_t transferId,
	std::size_t maximumMessagePayloadBytes,
	EncodedTacticalWorldDeltaChunks& output,
	std::size_t maximumTransferBytes = MaximumTacticalWorldDeltaTransferBytes,
	std::size_t maximumChunks = MaximumTacticalWorldDeltaTransferChunks) noexcept;

std::uint32_t TacticalWorldDeltaTransferChecksum(
	const std::uint8_t* bytes, std::size_t size) noexcept;

struct TacticalWorldDeltaReassemblyLimits
{
	std::size_t maximumTransferBytes = MaximumTacticalWorldDeltaTransferBytes;
	std::size_t maximumChunks = MaximumTacticalWorldDeltaTransferChunks;
	std::size_t maximumEvents = MaximumTacticalWorldDeltaEvents;
};

enum class TacticalWorldDeltaReassemblyResult
{
	AwaitingMore = 0,
	Completed = 1,
	InvalidMessage = 2,
	UnsupportedVersion = 3,
	InvalidTransfer = 4,
	TransferTooLarge = 5,
	TooManyChunks = 6,
	InterleavedTransfer = 7,
	UnexpectedChunk = 8,
	IntegrityMismatch = 9,
	InvalidDelta = 10,
	UnsupportedDeltaVersion = 11,
	TooManyEvents = 12,
	AllocationFailure = 13
};

// One bounded, strictly ordered transfer may be active at a time. Duplicate,
// out-of-order, and foreign nonzero chunks are rejected without damaging the
// accepted prefix. A valid index-zero chunk with a new host-lifetime transfer
// identity atomically supersedes an abandoned transfer after a world reset.
// A valid legacy single-message delta also retires an abandoned chunk prefix.
// The caller's delta is replaced only after size, checksum, and the complete
// inner delta codec all validate.
class TacticalWorldDeltaReassembler
{
public:
	explicit TacticalWorldDeltaReassembler(
		TacticalWorldDeltaReassemblyLimits limits = {}) noexcept;

	TacticalWorldDeltaReassemblyResult accept(
		const RuntimeMessage& message, TacticalWorldDelta& output) noexcept;
	// Abandons retained bytes but preserves the host-lifetime transfer-ID high
	// water mark so delayed pre-reset chunks cannot replay an older delta.
	void reset() noexcept;

	bool active() const noexcept { return transferId_ != 0; }
	std::uint64_t transferId() const noexcept { return transferId_; }
	std::uint32_t nextChunkIndex() const noexcept { return nextChunkIndex_; }
	std::uint32_t chunkCount() const noexcept { return chunkCount_; }
	std::size_t retainedBytes() const noexcept { return encodedDelta_.size(); }
	std::uint64_t highestTransferId() const noexcept { return highestTransferId_; }
	const TacticalWorldDeltaReassemblyLimits& limits() const noexcept
	{
		return limits_;
	}

private:
	TacticalWorldDeltaReassemblyLimits limits_;
	std::uint64_t transferId_ = 0;
	std::uint32_t nextChunkIndex_ = 0;
	std::uint32_t chunkCount_ = 0;
	std::size_t totalPayloadBytes_ = 0;
	std::uint32_t checksum_ = 0;
	std::uint64_t highestTransferId_ = 0;
	std::vector<std::uint8_t> encodedDelta_;
};

#endif
