#ifndef MULTIPLAYER_COOP_TACTICAL_PROTOCOL_H
#define MULTIPLAYER_COOP_TACTICAL_PROTOCOL_H

#include "CoopSessionProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Adapters/JA2/TacticalWorldSnapshotCodec.h>

namespace CoopSession
{
inline constexpr char CoopTacticalIntentMessageName[] =
	"coop.tactical.intent";
inline constexpr char CoopTacticalIntentReceiptMessageName[] =
	"coop.tactical.receipt";
inline constexpr char CoopTacticalBaselineMessageName[] =
	"coop.tactical.baseline";
inline constexpr char CoopTacticalBaselineAckMessageName[] =
	"coop.tactical.baseline.ack";
inline constexpr char CoopTacticalDeltaMessageName[] =
	"coop.tactical.delta";
inline constexpr char CoopTacticalDeltaAckMessageName[] =
	"coop.tactical.delta.ack";
inline constexpr char CoopTacticalResyncRequestMessageName[] =
	"coop.tactical.resync";

inline constexpr std::uint16_t CoopTacticalWireVersion = 3;
inline constexpr std::size_t CoopTacticalCommonHeaderWireSize = 48;
inline constexpr std::size_t CoopTacticalIntentReceiptWireSize = 96;
inline constexpr std::size_t CoopTacticalBaselineHeaderWireSize = 76;
inline constexpr std::size_t CoopTacticalBaselineAckWireSize = 88;
inline constexpr std::size_t CoopTacticalDeltaHeaderWireSize = 72;
inline constexpr std::size_t CoopTacticalDeltaAckWireSize = 80;
inline constexpr std::size_t CoopTacticalResyncRequestWireSize = 88;

// The live JA2 authority exposes at most 256 stable actor slots. Keeping that
// narrower limit on the wire places both inner payloads well below the 64 KiB
// application ceiling while the reusable SDK codecs retain their wider bounds.
inline constexpr std::size_t MaximumCoopTacticalSnapshotActors = 256;
inline constexpr std::size_t MaximumCoopTacticalSnapshotDoors = 1024;
inline constexpr std::size_t MaximumCoopTacticalAssignedActors = 256;
inline constexpr std::size_t MaximumCoopTacticalDeltaEvents =
	MaximumCoopTacticalSnapshotActors * 4 +
	MaximumCoopTacticalSnapshotDoors * 2 + 2;
inline constexpr std::size_t MaximumCoopTacticalPayloadWireSize = 64u * 1024u;
inline constexpr std::size_t MaximumCoopTacticalBaselinePayloadWireSize =
	EncodedTacticalWorldSnapshotHeaderBytes +
	MaximumCoopTacticalSnapshotActors * EncodedTacticalActorSnapshotBytes +
	MaximumCoopTacticalSnapshotDoors * EncodedTacticalDoorSnapshotBytes;
inline constexpr std::size_t MaximumCoopTacticalDeltaPayloadWireSize = 62034;
inline constexpr std::size_t MaximumCoopTacticalBaselineWireSize =
	CoopTacticalBaselineHeaderWireSize +
	MaximumCoopTacticalAssignedActors * 6 +
	MaximumCoopTacticalBaselinePayloadWireSize;
inline constexpr std::size_t MaximumCoopTacticalDeltaWireSize =
	CoopTacticalDeltaHeaderWireSize +
	MaximumCoopTacticalDeltaPayloadWireSize;
inline constexpr std::size_t MaximumCoopTacticalWireSize =
	MaximumCoopTacticalDeltaWireSize;
static_assert(MaximumCoopTacticalBaselineWireSize <=
	MaximumCoopTacticalWireSize,
	"the public tactical wire ceiling must hold a maximum baseline");
static_assert(MaximumCoopTacticalWireSize <=
	MaximumCoopTacticalPayloadWireSize,
	"every tactical envelope must remain below the public SDL ceiling");
static_assert(MaximumCoopTacticalBaselinePayloadWireSize <=
	MaximumCoopTacticalPayloadWireSize,
	"co-op tactical baselines must remain below the public payload ceiling");
static_assert(MaximumCoopTacticalDeltaPayloadWireSize <=
	MaximumCoopTacticalPayloadWireSize,
	"co-op tactical deltas must remain below the public payload ceiling");

using CoopTacticalIntentReceiptBytes =
	std::array<std::uint8_t, CoopTacticalIntentReceiptWireSize>;
using CoopTacticalBaselineAckBytes =
	std::array<std::uint8_t, CoopTacticalBaselineAckWireSize>;
using CoopTacticalDeltaAckBytes =
	std::array<std::uint8_t, CoopTacticalDeltaAckWireSize>;
using CoopTacticalResyncRequestBytes =
	std::array<std::uint8_t, CoopTacticalResyncRequestWireSize>;

enum class CoopTacticalWireMessageKind : std::uint8_t
{
	IntentReceipt = 1,
	Baseline = 2,
	BaselineAck = 3,
	Delta = 4,
	DeltaAck = 5,
	ResyncRequest = 6
};

struct CoopTacticalStateIdentity
{
	std::uint16_t wireVersion = CoopTacticalWireVersion;
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	std::uint64_t worldGeneration = 0;
	std::uint64_t revision = 0;
	std::uint64_t turnSerial = 0;
};

enum class CoopTacticalIntentReceiptStatus : std::uint8_t
{
	Queued = 1,
	Rejected = 2,
	Applied = 3,
	Discarded = 4,
	Cancelled = 5
};

// These reasons are deliberately protocol-level. They do not serialize JA2,
// UI, animation, or package-service enums.
enum class CoopTacticalIntentReceiptReason : std::uint8_t
{
	None = 0,
	MalformedIntent = 1,
	NotAdmitted = 2,
	SessionMismatch = 3,
	WorldMismatch = 4,
	RevisionMismatch = 5,
	TurnMismatch = 6,
	InvalidCommandSequence = 7,
	ActorNotOwned = 8,
	NotBaselineReady = 9,
	ActorUnavailable = 10,
	WrongTeam = 11,
	GameplayRejected = 12,
	InboxCapacityReached = 13,
	InboxSequenceExhausted = 14,
	AllocationFailure = 15,
	QueueUnavailable = 16,
	UnavailableContext = 17,
	AuthoritativeDiscard = 18,
	SessionEnded = 19,
	// The server or retained command service consumed this intent but could
	// not allocate another global authoritative command sequence. This is
	// distinct from the peer-inbox exhausted sentinel, which is non-consuming.
	AuthoritySequenceExhausted = 20
};

struct CoopTacticalIntentReceipt
{
	CoopTacticalStateIdentity state;
	PeerIdentity peerIdentity{};
	std::uint64_t commandId = 0;
	// Cursor immediately after a consumed command. A non-consuming sequence
	// rejection instead reports the server's current expected cursor.
	std::uint64_t nextExpectedCommandId = 0;
	std::uint64_t authoritativeSequence = 0;
	std::uint64_t simulationTick = 0;
	CoopTacticalIntentReceiptStatus status =
		CoopTacticalIntentReceiptStatus::Rejected;
	CoopTacticalIntentReceiptReason reason =
		CoopTacticalIntentReceiptReason::MalformedIntent;
};

struct CoopTacticalBaseline
{
	CoopTacticalStateIdentity state;
	std::uint64_t baselineId = 0;
	std::uint32_t payloadChecksum = 0;
	// Admission-epoch-wide command cursor. Zero is the exhausted sentinel.
	std::uint64_t nextExpectedCommandId = 1;
	std::vector<TacticalEntityId> assignedActors;
	TacticalWorldSnapshot snapshot;
};

struct CoopTacticalBaselineAck
{
	CoopTacticalStateIdentity state;
	PeerIdentity peerIdentity{};
	std::uint64_t baselineId = 0;
	std::uint32_t payloadChecksum = 0;
	// Echoed only after the client transactionally adopts the baseline cursor.
	std::uint64_t nextExpectedCommandId = 1;
};

struct CoopTacticalDelta
{
	CoopTacticalStateIdentity state;
	std::uint64_t deltaId = 0;
	std::uint64_t baseRevision = 0;
	std::uint32_t payloadChecksum = 0;
	TacticalWorldDelta delta;
};

struct CoopTacticalDeltaAck
{
	CoopTacticalStateIdentity state;
	PeerIdentity peerIdentity{};
	std::uint64_t deltaId = 0;
	std::uint32_t payloadChecksum = 0;
};

enum class CoopTacticalResyncReason : std::uint8_t
{
	DeltaSequenceGap = 1,
	PayloadChecksumMismatch = 2,
	StateMismatch = 3,
	ReplicaRejected = 4,
	InvalidEnvelope = 5,
	BaselineRejected = 6
};

// Self-only recovery request. The authenticated transport resolves the peer;
// none of these echoed client fields grants identity, cursor, or state authority.
struct CoopTacticalResyncRequest
{
	CoopTacticalStateIdentity acceptedState;
	std::uint64_t requestId = 0;
	std::uint64_t acceptedBaselineId = 0;
	std::uint64_t lastAppliedDeltaId = 0;
	std::uint32_t lastPayloadChecksum = 0;
	CoopTacticalResyncReason reason =
		CoopTacticalResyncReason::DeltaSequenceGap;
	std::uint64_t nextExpectedCommandId = 1;
};

enum class CoopTacticalCodecResult
{
	Success,
	Invalid,
	UnsupportedVersion,
	WrongMessageKind,
	PayloadTooLarge,
	ChecksumMismatch,
	InvalidPayload,
	AllocationFailure
};

bool IsKnownCoopTacticalWireMessageKind(
	CoopTacticalWireMessageKind kind) noexcept;
bool IsKnownCoopTacticalIntentReceiptStatus(
	CoopTacticalIntentReceiptStatus status) noexcept;
bool IsKnownCoopTacticalIntentReceiptReason(
	CoopTacticalIntentReceiptReason reason) noexcept;
bool IsKnownCoopTacticalResyncReason(
	CoopTacticalResyncReason reason) noexcept;
bool IsValidCoopTacticalStateIdentity(
	const CoopTacticalStateIdentity& identity) noexcept;

// FNV-1a over exactly size bytes. Null is accepted only for an empty input.
std::uint32_t CoopTacticalPayloadChecksum(
	const std::uint8_t* bytes, std::size_t size) noexcept;

CoopTacticalCodecResult EncodeCoopTacticalIntentReceipt(
	const CoopTacticalIntentReceipt& receipt,
	CoopTacticalIntentReceiptBytes& bytes) noexcept;
CoopTacticalCodecResult DecodeCoopTacticalIntentReceipt(
	const std::uint8_t* bytes, std::size_t size,
	CoopTacticalIntentReceipt& receipt) noexcept;

CoopTacticalCodecResult EncodeCoopTacticalBaseline(
	const CoopTacticalBaseline& baseline,
	std::vector<std::uint8_t>& bytes) noexcept;
CoopTacticalCodecResult DecodeCoopTacticalBaseline(
	const std::uint8_t* bytes, std::size_t size,
	CoopTacticalBaseline& baseline) noexcept;

CoopTacticalCodecResult EncodeCoopTacticalBaselineAck(
	const CoopTacticalBaselineAck& acknowledgement,
	CoopTacticalBaselineAckBytes& bytes) noexcept;
CoopTacticalCodecResult DecodeCoopTacticalBaselineAck(
	const std::uint8_t* bytes, std::size_t size,
	CoopTacticalBaselineAck& acknowledgement) noexcept;

CoopTacticalCodecResult EncodeCoopTacticalDelta(
	const CoopTacticalDelta& delta,
	std::vector<std::uint8_t>& bytes) noexcept;
CoopTacticalCodecResult DecodeCoopTacticalDelta(
	const std::uint8_t* bytes, std::size_t size,
	CoopTacticalDelta& delta) noexcept;

CoopTacticalCodecResult EncodeCoopTacticalDeltaAck(
	const CoopTacticalDeltaAck& acknowledgement,
	CoopTacticalDeltaAckBytes& bytes) noexcept;
CoopTacticalCodecResult DecodeCoopTacticalDeltaAck(
	const std::uint8_t* bytes, std::size_t size,
	CoopTacticalDeltaAck& acknowledgement) noexcept;

CoopTacticalCodecResult EncodeCoopTacticalResyncRequest(
	const CoopTacticalResyncRequest& request,
	CoopTacticalResyncRequestBytes& bytes) noexcept;
CoopTacticalCodecResult DecodeCoopTacticalResyncRequest(
	const std::uint8_t* bytes, std::size_t size,
	CoopTacticalResyncRequest& request) noexcept;

inline CoopTacticalCodecResult DecodeCoopTacticalBaseline(
	const std::vector<std::uint8_t>& bytes,
	CoopTacticalBaseline& baseline) noexcept
{
	return DecodeCoopTacticalBaseline(bytes.data(), bytes.size(), baseline);
}

inline CoopTacticalCodecResult DecodeCoopTacticalDelta(
	const std::vector<std::uint8_t>& bytes,
	CoopTacticalDelta& delta) noexcept
{
	return DecodeCoopTacticalDelta(bytes.data(), bytes.size(), delta);
}
}

#endif
