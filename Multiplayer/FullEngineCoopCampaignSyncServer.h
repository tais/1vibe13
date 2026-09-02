#ifndef MULTIPLAYER_FULL_ENGINE_COOP_CAMPAIGN_SYNC_SERVER_H
#define MULTIPLAYER_FULL_ENGINE_COOP_CAMPAIGN_SYNC_SERVER_H

#include "CoopAdmission.h"
#include "CoopCampaignSyncProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace CoopSession
{
inline constexpr std::size_t MaximumFullEngineCoopCampaignSyncPeers = 4;
inline constexpr std::size_t
	MaximumFullEngineCoopCampaignSyncMessagesPerFlush = 16;
static_assert(MaximumFullEngineCoopCampaignSyncPeers == MaximumAuthorityPeers,
	"campaign synchronization must retain the admission authority peer bound");

struct FullEngineCoopCampaignCheckpointMetadata
{
	std::uint64_t campaignSeed = 0;
	CoopCampaignIdentitySha256 campaignIdentitySha256{};
	std::uint64_t checkpointGeneration = 0;
	std::uint64_t totalSize = 0;
	CoopCampaignCheckpointSha256 checkpointSha256{};
	std::uint64_t worldMinutes = 0;
};

enum class FullEngineCoopCampaignCheckpointReadResult : std::uint8_t
{
	Success,
	Unavailable,
	DescriptorMismatch
};

// One source instance names one immutable cold checkpoint. metadata() must
// publish its output only on success and must return the same descriptor for
// the lifetime of the instance. readExact() receives the captured digest so a
// versioned backend can fail closed rather than reading a replaced object. It
// is called only with a non-null destination, an in-range offset, and at most
// CoopCampaignSyncCanonicalChunkBytes. On failure the server discards the
// scratch output and never advances peer state.
class FullEngineCoopCampaignCheckpointSource
{
public:
	virtual ~FullEngineCoopCampaignCheckpointSource() = default;
	virtual bool metadata(
		FullEngineCoopCampaignCheckpointMetadata& output) const noexcept = 0;
	virtual FullEngineCoopCampaignCheckpointReadResult readExact(
		const CoopCampaignCheckpointSha256& expectedCheckpointSha256,
		std::uint64_t offset,
		std::uint8_t* output,
		std::size_t size) noexcept = 0;
};

struct FullEngineCoopCampaignSyncAuthenticatedPeer
{
	PeerIdentity peerIdentity{};
	// Process-local connection generation. A changed value for the same peer is
	// a reconnect and must receive a new checkpoint transfer.
	TransportPeer transport;
};

enum class FullEngineCoopCampaignSyncOutboundKind : std::uint8_t
{
	Metadata,
	Chunk,
	Complete,
	Reject
};

const char* FullEngineCoopCampaignSyncOutboundMessageName(
	FullEngineCoopCampaignSyncOutboundKind kind) noexcept;

// A transport-enqueue seam only. Returning false retains the exact encoded
// frame for retry. Implementations must target this exact peer/connection pair
// and must not re-enter or mutate the server.
class FullEngineCoopCampaignSyncWireSink
{
public:
	virtual ~FullEngineCoopCampaignSyncWireSink() = default;
	virtual bool send(const PeerIdentity& peer,
		const TransportPeer& transport,
		FullEngineCoopCampaignSyncOutboundKind kind,
		const char* messageName,
		const std::uint8_t* bytes,
		std::size_t size) noexcept = 0;
};

struct FullEngineCoopCampaignSyncServerConfiguration
{
	std::uint64_t maximumTransferId =
		std::numeric_limits<std::uint64_t>::max();
	std::size_t maximumMessagesPerFlush =
		MaximumFullEngineCoopCampaignSyncMessagesPerFlush;
};

enum class FullEngineCoopCampaignSyncServerResult : std::uint8_t
{
	Success,
	InvalidConfiguration,
	AlreadyActive,
	NotActive,
	InvalidSessionEpoch,
	InvalidCheckpoint,
	CheckpointNotNewer,
	SourceUnavailable,
	SourceDescriptorMismatch,
	InvalidPeerSet,
	PeerCapacityReached,
	InvalidPeer,
	StaleTransport,
	MalformedFrame,
	ClaimedIdentityMismatch,
	StaleTransfer,
	UnexpectedFrame,
	SequenceMismatch,
	IntegrityMismatch,
	ClientRejected,
	TransferExhausted,
	CodecFailure,
	AllocationFailure,
	TransportBackpressured,
	Busy,
	TerminalFailure
};

enum class FullEngineCoopCampaignSyncPeerPhase : std::uint8_t
{
	Vacant,
	MetadataPending,
	AwaitingInitialAck,
	Streaming,
	CompletePending,
	AwaitingResult,
	Ready,
	RejectPending,
	Rejected,
	Failed
};

enum class FullEngineCoopCampaignSyncPendingKind : std::uint8_t
{
	None,
	Metadata,
	Chunk,
	Complete,
	Reject
};

enum class FullEngineCoopCampaignSyncInboundKind : std::uint8_t
{
	Ack,
	Result,
	Resync
};

struct FullEngineCoopCampaignSyncPeerDiagnostics
{
	PeerIdentity peerIdentity{};
	TransportPeer transport;
	FullEngineCoopCampaignSyncPeerPhase phase =
		FullEngineCoopCampaignSyncPeerPhase::Vacant;
	std::uint64_t transferId = 0;
	std::uint64_t acknowledgedOffset = 0;
	std::uint64_t highestSentOffset = 0;
	std::uint64_t nextSendOffset = 0;
	std::uint32_t precedingAcknowledgedChecksum = 0;
	std::size_t inFlightChunks = 0;
	FullEngineCoopCampaignSyncPendingKind pendingKind =
		FullEngineCoopCampaignSyncPendingKind::None;
	std::size_t pendingBytes = 0;
	CoopCampaignSyncFailureReason rejectionReason =
		CoopCampaignSyncFailureReason::None;
	bool campaignReady = false;
};

struct FullEngineCoopCampaignSyncServerDiagnostics
{
	bool active = false;
	bool terminal = false;
	std::uint64_t sessionEpoch = 0;
	std::uint64_t checkpointGeneration = 0;
	std::uint64_t checkpointSize = 0;
	std::uint64_t nextTransferId = 0;
	std::size_t connectedPeers = 0;
	std::size_t readyPeers = 0;
	std::size_t pendingMessages = 0;
	std::size_t inFlightChunks = 0;
	FullEngineCoopCampaignSyncServerResult terminalCause =
		FullEngineCoopCampaignSyncServerResult::Success;
};

struct FullEngineCoopCampaignSyncFlushResult
{
	FullEngineCoopCampaignSyncServerResult result =
		FullEngineCoopCampaignSyncServerResult::Success;
	PeerIdentity peerIdentity{};
	std::size_t messagesSent = 0;
	std::size_t chunksSent = 0;
	bool backpressured = false;
};

// Pure, bounded server-side checkpoint replication. The caller supplies only
// sorted transport-authenticated peers and frames already attributed to one
// such peer/connection. Client-echoed identities are compared after that
// lookup and are never authorization inputs. The sink must outlive this
// server. The currently installed checkpoint source must remain immutable and
// alive until a successful supersedeCheckpoint() or server destruction; a
// successfully superseded source is no longer referenced.
class FullEngineCoopCampaignSyncServer
{
public:
	FullEngineCoopCampaignSyncServer(
		FullEngineCoopCampaignCheckpointSource& source,
		FullEngineCoopCampaignSyncWireSink& sink,
		FullEngineCoopCampaignSyncServerConfiguration configuration = {})
		noexcept;

	FullEngineCoopCampaignSyncServerResult beginSession(
		std::uint64_t sessionEpoch) noexcept;
	FullEngineCoopCampaignSyncServerResult endSession() noexcept;
	bool active() const noexcept { return active_; }
	bool terminal() const noexcept { return terminal_; }
	std::uint64_t sessionEpoch() const noexcept { return sessionEpoch_; }

	// Input is strict PeerIdentity order with unique nonzero identities and
	// unique nonzero connection generations. The update is atomic, including
	// transfer-id allocation and metadata staging for new/reconnected peers.
	FullEngineCoopCampaignSyncServerResult reconcilePeers(
		const FullEngineCoopCampaignSyncAuthenticatedPeer* peers,
		std::size_t count) noexcept;

	// A superseding source must retain campaign seed/identity and publish a
	// strictly greater checkpoint generation. Every connected peer, including a
	// previously Ready peer, receives a preflighted fresh transfer atomically.
	FullEngineCoopCampaignSyncServerResult supersedeCheckpoint(
		FullEngineCoopCampaignCheckpointSource& source) noexcept;

	FullEngineCoopCampaignSyncServerResult handleInbound(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		FullEngineCoopCampaignSyncInboundKind kind,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;
	FullEngineCoopCampaignSyncServerResult handleAck(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		const CoopCampaignSyncAck& acknowledgement) noexcept;
	FullEngineCoopCampaignSyncServerResult handleResult(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		const CoopCampaignSyncResult& result) noexcept;
	FullEngineCoopCampaignSyncServerResult handleResync(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		const CoopCampaignSyncResync& resync) noexcept;

	FullEngineCoopCampaignSyncFlushResult flushOutbound() noexcept;
	std::size_t readyPeers(std::array<PeerIdentity,
		MaximumFullEngineCoopCampaignSyncPeers>& peers) const noexcept;
	bool peerDiagnostics(const PeerIdentity& peer,
		FullEngineCoopCampaignSyncPeerDiagnostics& diagnostics) const noexcept;
	FullEngineCoopCampaignSyncServerDiagnostics diagnostics() const noexcept;

private:
	struct SentChunk
	{
		std::uint64_t offset = 0;
		std::uint64_t endOffset = 0;
		std::uint32_t checksum = 0;
	};

	struct PendingMessage
	{
		std::array<std::uint8_t, MaximumCoopCampaignSyncWireSize> bytes{};
		std::size_t size = 0;
		FullEngineCoopCampaignSyncPendingKind kind =
			FullEngineCoopCampaignSyncPendingKind::None;
		std::uint64_t chunkOffset = 0;
		std::uint64_t chunkEndOffset = 0;
		std::uint32_t chunkChecksum = 0;

		bool occupied() const noexcept
		{
			return kind != FullEngineCoopCampaignSyncPendingKind::None;
		}
		void clear() noexcept;
	};

	struct PeerRecord
	{
		PeerIdentity identity{};
		TransportPeer transport;
		FullEngineCoopCampaignSyncPeerPhase phase =
			FullEngineCoopCampaignSyncPeerPhase::Vacant;
		CoopCampaignSyncTransferIdentity transfer;
		std::uint64_t acknowledgedOffset = 0;
		std::uint64_t highestSentOffset = 0;
		std::uint64_t nextSendOffset = 0;
		std::uint32_t precedingAcknowledgedChecksum = 0;
		std::array<SentChunk, MaximumCoopCampaignSyncChunkWindow> inFlight{};
		std::size_t inFlightCount = 0;
		PendingMessage pending;
		CoopCampaignSyncFailureReason rejectionReason =
			CoopCampaignSyncFailureReason::None;
	};

	bool configurationValid() const noexcept;
	bool checkpointMetadataValid(
		const FullEngineCoopCampaignCheckpointMetadata& metadata) const noexcept;
	bool sameCheckpointMetadata(
		const FullEngineCoopCampaignCheckpointMetadata& left,
		const FullEngineCoopCampaignCheckpointMetadata& right) const noexcept;
	bool canAllocateTransfers(std::size_t count) const noexcept;
	std::uint64_t allocateTransferId(std::uint64_t& cursor) const noexcept;
	CoopCampaignSyncTransferIdentity transferIdentity(
		std::uint64_t transferId,
		const FullEngineCoopCampaignCheckpointMetadata& metadata) const noexcept;
	PeerRecord* findPeer(const PeerIdentity& peer) noexcept;
	const PeerRecord* findPeer(const PeerIdentity& peer) const noexcept;
	FullEngineCoopCampaignSyncServerResult resolvePeer(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		PeerRecord*& peer) noexcept;
	FullEngineCoopCampaignSyncServerResult verifySource() noexcept;
	void failTerminal(FullEngineCoopCampaignSyncServerResult cause) noexcept;
	FullEngineCoopCampaignSyncServerResult stageFreshPeer(
		PeerRecord& peer,
		const FullEngineCoopCampaignSyncAuthenticatedPeer& authenticated,
		std::uint64_t transferId,
		const FullEngineCoopCampaignCheckpointMetadata& metadata) noexcept;
	FullEngineCoopCampaignSyncServerResult stageMetadata(
		PeerRecord& peer,
		const FullEngineCoopCampaignCheckpointMetadata& metadata) noexcept;
	FullEngineCoopCampaignSyncServerResult stageComplete(
		PeerRecord& peer) noexcept;
	FullEngineCoopCampaignSyncServerResult stageReject(PeerRecord& peer,
		CoopCampaignSyncFailureReason reason) noexcept;
	FullEngineCoopCampaignSyncServerResult stageNextChunk(
		PeerRecord& peer) noexcept;
	void commitPendingSend(PeerRecord& peer,
		FullEngineCoopCampaignSyncFlushResult& result) noexcept;
	const char* pendingMessageName(
		FullEngineCoopCampaignSyncPendingKind kind) const noexcept;
	FullEngineCoopCampaignSyncOutboundKind pendingOutboundKind(
		FullEngineCoopCampaignSyncPendingKind kind) const noexcept;
	FullEngineCoopCampaignSyncServerResult validateEchoedTransfer(
		const PeerRecord& peer,
		const CoopCampaignSyncTransferIdentity& transfer) const noexcept;
	const SentChunk* findInFlightEnd(
		const PeerRecord& peer, std::uint64_t cursor) const noexcept;
	void acknowledgeThrough(PeerRecord& peer,
		std::uint64_t cursor, std::uint32_t checksum) noexcept;
	FullEngineCoopCampaignSyncServerResult handleAckInternal(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		const CoopCampaignSyncAck& acknowledgement) noexcept;
	FullEngineCoopCampaignSyncServerResult handleResultInternal(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		const CoopCampaignSyncResult& result) noexcept;
	FullEngineCoopCampaignSyncServerResult handleResyncInternal(
		const PeerIdentity& authenticatedPeer,
		const TransportPeer& transport,
		const CoopCampaignSyncResync& resync) noexcept;

	FullEngineCoopCampaignCheckpointSource* source_ = nullptr;
	FullEngineCoopCampaignSyncWireSink& sink_;
	FullEngineCoopCampaignSyncServerConfiguration configuration_;
	FullEngineCoopCampaignCheckpointMetadata checkpoint_;
	std::uint64_t sessionEpoch_ = 0;
	std::uint64_t nextTransferId_ = 1;
	std::array<PeerRecord, MaximumFullEngineCoopCampaignSyncPeers> peers_{};
	std::size_t peerCount_ = 0;
	bool active_ = false;
	bool terminal_ = false;
	bool busy_ = false;
	FullEngineCoopCampaignSyncServerResult terminalCause_ =
		FullEngineCoopCampaignSyncServerResult::Success;
};
}

#endif
