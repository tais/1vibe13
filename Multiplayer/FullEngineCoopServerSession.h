#ifndef MULTIPLAYER_FULL_ENGINE_COOP_SERVER_SESSION_H
#define MULTIPLAYER_FULL_ENGINE_COOP_SERVER_SESSION_H

#include "CoopAdmission.h"
#include "CoopTacticalProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace CoopSession
{
inline constexpr std::size_t MaximumCoopTacticalSessionPeers =
	MaximumAuthorityPeers;
inline constexpr std::size_t MaximumCoopTacticalAssignments =
	MaximumCoopTacticalAssignedActors;
inline constexpr std::size_t MaximumCoopTacticalDeltaHistory = 64;
inline constexpr std::size_t MaximumCoopTacticalReceiptHistoryPerPeer = 64;
inline constexpr std::size_t MaximumCoopTacticalMessagesPerFlush = 256;

struct FullEngineCoopServerSessionConfiguration
{
	std::size_t maximumPeers = MaximumCoopTacticalSessionPeers;
	std::size_t maximumAssignments = MaximumCoopTacticalAssignments;
	std::size_t maximumDeltaHistory = 32;
	std::size_t maximumReceiptHistoryPerPeer = 32;
	std::size_t maximumInFlightDeltasPerPeer = 8;
	std::size_t maximumMessagesPerFlush = 64;
	std::uint64_t maximumBaselineId =
		std::numeric_limits<std::uint64_t>::max();
	std::uint64_t maximumDeltaId =
		std::numeric_limits<std::uint64_t>::max();
};

enum class CoopTacticalPeerPhase : std::uint8_t
{
	Vacant,
	Offline,
	NeedsBaseline,
	AwaitingBaselineAck,
	Active,
	ResyncRequired
};

struct CoopTacticalActorAssignment
{
	TacticalEntityId actor;
	PeerIdentity peerIdentity{};
};

enum class FullEngineCoopServerSessionResult
{
	Success,
	InvalidConfiguration,
	NotActive,
	NoWorld,
	InvalidPeer,
	PeerCapacityReached,
	InvalidContext,
	StaleContext,
	InvalidAssignment,
	AssignmentCapacityReached,
	SequenceExhausted,
	CodecFailure,
	ReceiptCapacityReached,
	ConflictingReceipt,
	UnexpectedAcknowledgement,
	IntegrityMismatch,
	ResyncRequired,
	NotFound,
	AllocationFailure,
	Busy
};

enum class CoopTacticalOutboundMessageKind : std::uint8_t
{
	IntentReceipt,
	Baseline,
	Delta
};

const char* CoopTacticalOutboundMessageName(
	CoopTacticalOutboundMessageKind kind) noexcept;

// This callback is a transport-enqueue seam only. It is called while flush()
// is guarded against re-entry and must not invoke JA2 or mutate the session.
// Returning false applies bounded backpressure without advancing send state.
class FullEngineCoopServerSessionWireSink
{
public:
	virtual ~FullEngineCoopServerSessionWireSink() = default;
	virtual bool send(
		const PeerIdentity& peer,
		CoopTacticalOutboundMessageKind kind,
		const char* messageName,
		const std::uint8_t* bytes,
		std::size_t size) noexcept = 0;
};

struct FullEngineCoopServerSessionFlushResult
{
	FullEngineCoopServerSessionResult result =
		FullEngineCoopServerSessionResult::Success;
	std::size_t messagesSent = 0;
	bool backpressured = false;
};

struct CoopTacticalPeerReplicationState
{
	PeerIdentity peerIdentity{};
	CoopTacticalPeerPhase phase = CoopTacticalPeerPhase::Vacant;
	bool connected = false;
	std::uint64_t baselineId = 0;
	std::uint64_t baselineRevision = 0;
	std::uint64_t baselineNextExpectedCommandId = 1;
	std::uint64_t nextDeltaToSend = 0;
	std::uint64_t nextDeltaToAcknowledge = 0;
	std::uint64_t lastSentRevision = 0;
	std::uint64_t lastAcknowledgedDeltaId = 0;
	std::uint64_t lastAcknowledgedRevision = 0;
	std::size_t inFlightDeltas = 0;
	std::size_t retainedReceipts = 0;
	std::size_t pendingReceipts = 0;
};

// Pure, bounded replication bookkeeping for the full-engine server. The
// caller captures snapshots/deltas, applies assignment ACLs, handles intents,
// and records terminal receipts at explicit committed-frame boundaries. This
// object owns no JA2 pointers and never executes gameplay.
class FullEngineCoopServerSession
{
public:
	explicit FullEngineCoopServerSession(
		FullEngineCoopServerSessionConfiguration configuration = {}) noexcept;

	FullEngineCoopServerSessionResult beginSession(
		std::uint64_t sessionEpoch) noexcept;
	void endSession() noexcept;
	bool active() const noexcept { return active_; }
	bool worldActive() const noexcept { return worldActive_; }
	std::uint64_t sessionEpoch() const noexcept { return sessionEpoch_; }
	std::uint64_t worldGeneration() const noexcept { return worldGeneration_; }
	std::uint64_t revision() const noexcept { return revision_; }
	std::uint64_t turnSerial() const noexcept { return turnSerial_; }

	FullEngineCoopServerSessionResult beginWorld(
		std::uint64_t worldGeneration,
		std::uint64_t revision,
		std::uint64_t turnSerial) noexcept;
	void endWorld() noexcept;

	FullEngineCoopServerSessionResult connectPeer(
		const PeerIdentity& peer) noexcept;
	FullEngineCoopServerSessionResult disconnectPeer(
		const PeerIdentity& peer) noexcept;
	// Permanently frees an offline identity slot at a committed retirement
	// boundary. Any assignments owned by that identity are removed; all
	// surviving peer records, receipt histories, and replication cursors are
	// preserved exactly. Pending wire sends retained only for the retired peer
	// are intentionally waived after transport stop.
	FullEngineCoopServerSessionResult retirePeer(
		const PeerIdentity& peer) noexcept;
	std::size_t peerCount() const noexcept { return peerCount_; }
	std::size_t connectedPeerCount() const noexcept;
	CoopTacticalPeerPhase peerPhase(const PeerIdentity& peer) const noexcept;
	bool peerState(const PeerIdentity& peer,
		CoopTacticalPeerReplicationState& output) const noexcept;

	// Input must already be in strict TacticalEntityId order. Every actor is
	// unique and every referenced peer must be retained by this session.
	FullEngineCoopServerSessionResult replaceAssignments(
		const CoopTacticalActorAssignment* assignments,
		std::size_t count) noexcept;
	std::size_t assignmentCount() const noexcept { return assignmentCount_; }
	const CoopTacticalActorAssignment* assignment(
		std::size_t index) const noexcept;

	FullEngineCoopServerSessionResult stageBaseline(
		const PeerIdentity& peer,
		const TacticalWorldSnapshot& snapshot,
		std::uint64_t nextExpectedCommandId) noexcept;
	FullEngineCoopServerSessionResult acknowledgeBaseline(
		const PeerIdentity& peer,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;

	FullEngineCoopServerSessionResult publishDelta(
		const TacticalWorldDelta& delta,
		std::uint64_t resultingRevision,
		std::uint64_t resultingTurnSerial) noexcept;
	FullEngineCoopServerSessionResult acknowledgeDelta(
		const PeerIdentity& peer,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;
	FullEngineCoopServerSessionResult requestResync(
		const PeerIdentity& peer, const std::uint8_t* bytes,
		std::size_t size, bool* replicationReset = nullptr) noexcept;
	std::size_t deltaHistorySize() const noexcept { return deltaCount_; }

	FullEngineCoopServerSessionResult recordReceipt(
		const CoopTacticalIntentReceipt& receipt) noexcept;
	FullEngineCoopServerSessionResult replayReceipt(
		const PeerIdentity& peer,
		std::uint64_t commandId) noexcept;
	bool hasRetainedReceipt(
		const PeerIdentity& peer, std::uint64_t commandId) const noexcept;

	FullEngineCoopServerSessionFlushResult flush(
		FullEngineCoopServerSessionWireSink& sink) noexcept;

	// Resynchronization is caller-driven: capture the current committed world and
	// call stageBaseline for each returned identity outside all callbacks.
	std::size_t peersNeedingBaseline(
		std::array<PeerIdentity, MaximumCoopTacticalSessionPeers>& peers) const
		noexcept;

private:
	struct ReceiptRecord
	{
		std::uint64_t commandId = 0;
		std::uint64_t revision = 0;
		CoopTacticalIntentReceiptStatus status =
			CoopTacticalIntentReceiptStatus::Rejected;
		CoopTacticalIntentReceiptBytes bytes{};
		bool pending = false;
	};
	struct SentCheckpoint
	{
		std::uint64_t sendOrdinal = 0;
		bool baseline = false;
		std::uint64_t baselineId = 0;
		std::uint64_t deltaId = 0;
		std::uint64_t revision = 0;
		std::uint64_t turnSerial = 0;
		std::uint64_t nextExpectedCommandId = 1;
		std::uint32_t checksum = 0;
	};

	struct PeerRecord
	{
		PeerIdentity identity{};
		CoopTacticalPeerPhase phase = CoopTacticalPeerPhase::Vacant;
		bool connected = false;
		std::uint64_t baselineId = 0;
		std::uint64_t baselineRevision = 0;
		std::uint64_t baselineTurnSerial = 0;
		std::uint64_t baselineNextExpectedCommandId = 1;
		std::uint64_t baselineDeltaFloor = 0;
		std::uint32_t baselineChecksum = 0;
		bool baselineSent = false;
		std::vector<std::uint8_t> baselineBytes;
		std::uint64_t nextDeltaToSend = 0;
		std::uint64_t nextDeltaToAcknowledge = 0;
		std::uint64_t lastSentRevision = 0;
		std::uint64_t lastAcknowledgedDeltaId = 0;
		std::uint64_t lastAcknowledgedRevision = 0;
		std::uint64_t lastAcknowledgedTurnSerial = 0;
		std::uint32_t lastAcknowledgedChecksum = 0;
		std::size_t inFlightDeltas = 0;
		bool committedCheckpointValid = false;
		std::uint64_t committedBaselineId = 0;
		std::uint64_t committedRevision = 0;
		std::uint64_t committedTurnSerial = 0;
		std::uint64_t committedDeltaId = 0;
		std::uint32_t committedChecksum = 0;
		std::uint64_t committedOrdinal = 0;
		std::uint64_t nextSentCheckpointOrdinal = 1;
		bool hasResyncRequest = false;
		std::uint64_t lastResyncRequestId = 0;
		CoopTacticalResyncRequestBytes lastResyncRequestBytes{};
		std::array<ReceiptRecord,
			MaximumCoopTacticalReceiptHistoryPerPeer> receipts{};
		std::size_t receiptHead = 0;
		std::size_t receiptCount = 0;
		std::array<SentCheckpoint, MaximumCoopTacticalDeltaHistory + 2>
			sentCheckpoints{};
		std::size_t sentCheckpointHead = 0;
		std::size_t sentCheckpointCount = 0;
	};

	struct DeltaRecord
	{
		std::uint64_t id = 0;
		std::uint64_t baseRevision = 0;
		std::uint64_t revision = 0;
		std::uint64_t turnSerial = 0;
		std::uint32_t checksum = 0;
		std::vector<std::uint8_t> bytes;
	};
	bool configurationValid() const noexcept;
	PeerRecord* findPeer(const PeerIdentity& peer) noexcept;
	const PeerRecord* findPeer(const PeerIdentity& peer) const noexcept;
	DeltaRecord* findDelta(std::uint64_t id) noexcept;
	const DeltaRecord* findDelta(std::uint64_t id) const noexcept;
	DeltaRecord& deltaAt(std::size_t offset) noexcept;
	const DeltaRecord& deltaAt(std::size_t offset) const noexcept;
	ReceiptRecord& receiptAt(PeerRecord& peer, std::size_t offset) noexcept;
	const ReceiptRecord& receiptAt(
		const PeerRecord& peer, std::size_t offset) const noexcept;
	void recordSentCheckpoint(PeerRecord& peer,
		const SentCheckpoint& checkpoint) noexcept;
	const SentCheckpoint* findSentBaseline(const PeerRecord& peer,
		std::uint64_t baselineId) const noexcept;
	const SentCheckpoint* findSentDelta(const PeerRecord& peer,
		std::uint64_t deltaId) const noexcept;
	void resetPeerReplication(PeerRecord& peer,
		CoopTacticalPeerPhase phase) noexcept;
	void requireResync(PeerRecord& peer) noexcept;
	bool baselineCanCatchUp(const PeerRecord& peer) const noexcept;
	std::uint64_t nextBaselineId(std::uint64_t value) const noexcept;
	std::uint64_t nextDeltaId(std::uint64_t value) const noexcept;
	void evictOldestDelta() noexcept;
	bool sameState(const CoopTacticalStateIdentity& left,
		const CoopTacticalStateIdentity& right) const noexcept;

	FullEngineCoopServerSessionConfiguration configuration_;
	std::uint64_t sessionEpoch_ = 0;
	std::uint64_t worldGeneration_ = 0;
	std::uint64_t revision_ = 0;
	std::uint64_t turnSerial_ = 0;
	std::uint64_t nextBaselineId_ = 1;
	std::uint64_t nextDeltaId_ = 1;
	std::array<PeerRecord, MaximumCoopTacticalSessionPeers> peers_{};
	std::size_t peerCount_ = 0;
	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments> assignments_{};
	std::size_t assignmentCount_ = 0;
	std::array<DeltaRecord, MaximumCoopTacticalDeltaHistory> deltas_{};
	std::size_t deltaHead_ = 0;
	std::size_t deltaCount_ = 0;
	bool active_ = false;
	bool worldActive_ = false;
	bool flushing_ = false;
};
}

#endif
