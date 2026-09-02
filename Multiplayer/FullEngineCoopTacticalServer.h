#ifndef MULTIPLAYER_FULL_ENGINE_COOP_TACTICAL_SERVER_H
#define MULTIPLAYER_FULL_ENGINE_COOP_TACTICAL_SERVER_H

#include "FullEngineCoopAdmissionListener.h"
#include "FullEngineCoopServerSession.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace CoopSession
{
inline constexpr std::size_t MaximumCoopTacticalServerInboundPerPump =
	MaximumCoopTacticalInboundMessages;
inline constexpr std::size_t MaximumCoopTacticalServerTransientReceipts = 64;
inline constexpr std::size_t MaximumCoopTacticalServerPendingCommandsPerPeer =
	MaximumCoopTacticalReceiptHistoryPerPeer;

struct FullEngineCoopTacticalServerConfiguration
{
	FullEngineCoopServerSessionConfiguration replication;
	std::size_t maximumInboundMessagesPerPump = 32;
	std::size_t maximumTransientReceipts = 32;
	// A configurable ceiling keeps the terminal exhaustion path testable while
	// production retains the complete uint64_t authoritative sequence domain.
	std::uint64_t maximumAuthoritativeSequence =
		(std::numeric_limits<std::uint64_t>::max)();
};

enum class FullEngineCoopTacticalServerResult : std::uint8_t
{
	Success,
	InvalidConfiguration,
	NotActive,
	AlreadyActive,
	AdmissionInactive,
	SessionEpochMismatch,
	NoWorld,
	InvalidContext,
	InvalidAssignment,
	InvalidPeerSet,
	PeerCapacityReached,
	PeerReconciliationFailed,
	BaselineUnavailable,
	ReplicationFailure,
	ActorBindingFailure,
	ReceiptCapacityReached,
	ReceiptRejected,
	SequenceDiverged,
	ExecutionBackpressured,
	TransportBackpressured,
	PendingReceipts,
	PendingInput,
	InputRejected,
	Busy,
	InternalFailure
};

struct FullEngineCoopTacticalServerPumpResult
{
	FullEngineCoopTacticalServerResult result =
		FullEngineCoopTacticalServerResult::Success;
	FullEngineCoopServerSessionResult replicationResult =
		FullEngineCoopServerSessionResult::Success;
	TacticalIntentAuthorizationReason authorizationReason =
		TacticalIntentAuthorizationReason::None;
	PeerIdentity peerIdentity{};
	std::uint64_t commandId = 0;
	std::size_t peersConnected = 0;
	std::size_t peersDisconnected = 0;
	std::size_t inboundConsumed = 0;
	std::size_t intentsConsumed = 0;
	std::size_t acknowledgementsAccepted = 0;
	std::size_t inputsRejected = 0;
	std::size_t duplicateReceiptsReplayed = 0;
	std::size_t messagesSent = 0;
	bool backpressured = false;
	bool resyncRequired = false;
	bool transportRestartRequired = false;
};

struct FullEngineCoopTacticalPeerCommandState
{
	PeerIdentity peerIdentity{};
	TransportPeer transport;
	std::uint64_t nextExpectedCommandId = 1;
	bool connected = false;
	bool exhausted = false;
	std::size_t pendingCommands = 0;
};

// A bounded, read-only checkpoint/drain view.  Sent receipt history is not an
// obligation and is therefore deliberately excluded; every count below names
// work that can still affect a client or consume an at-most-once command.
struct FullEngineCoopTacticalServerDrainState
{
	bool active = false;
	bool worldActive = false;
	bool transportRunning = false;
	bool transportRestartRequired = false;
	std::size_t authenticatedPeers = 0;
	std::size_t inboundMessages = 0;
	std::size_t transientReceipts = 0;
	std::size_t pendingCommands = 0;
	std::size_t pendingReplicationReceipts = 0;
	std::size_t peersAwaitingReplication = 0;
	std::size_t inFlightDeltas = 0;

	bool receiptObligationsCleared() const noexcept
	{
		return transientReceipts == 0 && pendingCommands == 0 &&
			pendingReplicationReceipts == 0;
	}
	// Checkpointing is allowed only after the world has ended and no queued
	// input, receipt obligation, or replication send remains.
	bool drained() const noexcept
	{
		return !worldActive && !transportRunning &&
			!transportRestartRequired &&
			authenticatedPeers == 0 && inboundMessages == 0 &&
			receiptObligationsCleared() &&
			peersAwaitingReplication == 0 && inFlightDeltas == 0;
	}
};

// Pure main-thread coordinator. While active it is the sole caller of tactical
// intent entry points on ingress; this ownership keeps its fixed command cursor
// mirror identical to the authority's admission-epoch-wide sequence table.
// SDL callbacks only feed the listener's bounded byte queue, and this class has
// no JA2 pointers or callbacks of its own.
class FullEngineCoopTacticalServer
{
public:
	FullEngineCoopTacticalServer(
		FullEngineCoopIngress& ingress,
		FullEngineCoopAdmissionListener& listener,
		FullEngineCoopTacticalServerConfiguration configuration = {}) noexcept;

	FullEngineCoopTacticalServerResult beginEpoch(
		std::uint64_t sessionEpoch) noexcept;
	FullEngineCoopTacticalServerResult endEpoch() noexcept;
	bool active() const noexcept { return active_; }
	bool worldActive() const noexcept { return replication_.worldActive(); }
	std::uint64_t sessionEpoch() const noexcept
	{
		return replication_.sessionEpoch();
	}

	FullEngineCoopTacticalServerResult beginWorld(
		std::uint64_t worldGeneration,
		std::uint64_t revision,
		std::uint64_t turnSerial) noexcept;
	// Drains queued terminal receipts, ends tactical authority/replication, and
	// raises a one-shot transport restart signal because v1 has no unload frame.
	FullEngineCoopTacticalServerResult endWorld() noexcept;
	bool transportRestartRequired() const noexcept
	{
		return transportRestartRequired_;
	}
	bool takeTransportRestartRequired() noexcept;

	// Tactical admission is a second gate after transport authentication. The
	// caller publishes the strict sorted set of peers that have committed the
	// current campaign checkpoint. The default set is empty. Removing a peer
	// disconnects its replication session and ACL immediately; adding it always
	// enters through the normal fresh-baseline path. Invalid input preserves the
	// current set.
	FullEngineCoopTacticalServerResult setCampaignReadyPeers(
		const PeerIdentity* peers, std::size_t count) noexcept;
	std::size_t campaignReadyPeers(
		std::array<PeerIdentity,
			MaximumCoopTacticalSessionPeers>& peers) const noexcept;
	FullEngineCoopTacticalServerResult reconcilePeers() noexcept;
	FullEngineCoopTacticalServerResult replaceAssignments(
		const CoopTacticalActorAssignment* assignments,
		std::size_t count) noexcept;
	FullEngineCoopTacticalServerResult stageBaseline(
		const PeerIdentity& peer,
		const TacticalWorldSnapshot& snapshot) noexcept;
	FullEngineCoopTacticalServerResult stageBaselines(
		const TacticalWorldSnapshot& snapshot) noexcept;
	std::size_t peersNeedingBaseline(
		std::array<PeerIdentity,
			MaximumCoopTacticalSessionPeers>& peers) const noexcept;
	FullEngineCoopTacticalServerResult publishDelta(
		const TacticalWorldDelta& delta,
		std::uint64_t resultingRevision,
		std::uint64_t resultingTurnSerial) noexcept;

	// Accepts the host-owned initial Queued/immediate-terminal receipt and later
	// terminal outcome for a reserved command. Identity, state, cursor, and
	// authoritative sequence are normalized from server-owned tables. A narrow
	// synchronous call from the execution sink is permitted while pumpInbound()
	// is executing that exact command; unrelated reentrant calls and all calls
	// during flush return Busy. Backpressured callers retain and retry.
	FullEngineCoopTacticalServerResult recordReceipt(
		const CoopTacticalIntentReceipt& receipt) noexcept;

	FullEngineCoopTacticalServerPumpResult pumpInbound(
		std::uint64_t simulationTick = 0) noexcept;
	FullEngineCoopTacticalServerPumpResult flushOutbound() noexcept;
	// World teardown may strand one unconsumed intent in the coordinator's
	// deferred slot after the live execution sink becomes unavailable.  Once the
	// transport is fully stopped, all authenticated transport/session mappings
	// have been reconciled away, and no pump/flush is active, the caller may
	// retire those bytes without advancing authority or the mirrored command
	// cursor.  A reconnect learns that unchanged cursor from its fresh baseline.
	FullEngineCoopTacticalServerResult discardInboundAfterTransportStop() noexcept;
	// A credential-authenticated self-retirement request closes the live input
	// gate before the runtime consumes it. At that exact gate only, discard raw
	// bytes which have not crossed tactical authority (including the one deferred
	// coordinator slot). Command cursors, pending commands, receipt history, and
	// authoritative sequence are deliberately untouched.
	FullEngineCoopTacticalServerResult
		discardInboundAfterSelfRetirementGate() noexcept;
	// Finalizes only an identity already tombstoned by authenticated
	// self-retirement. The transport must be stopped/reconciled and all local
	// command work for that peer settled. This frees the fixed coordinator and
	// replication slots without changing survivor command/receipt cursors.
	FullEngineCoopTacticalServerResult retirePeer(
		const PeerIdentity& peer) noexcept;

	bool peerCommandState(
		const PeerIdentity& peer,
		FullEngineCoopTacticalPeerCommandState& state) const noexcept;
	FullEngineCoopTacticalServerDrainState drainState() const noexcept;
	bool drained() const noexcept { return drainState().drained(); }
	const FullEngineCoopServerSession& replication() const noexcept
	{
		return replication_;
	}

private:
	struct PendingCommand
	{
		std::uint64_t commandId = 0;
		// Frozen receipt contract: cursor immediately after this command, not
		// the possibly newer peer cursor when a late terminal result arrives.
		std::uint64_t nextExpectedCommandId = 0;
		std::uint64_t authoritativeSequence = 0;
		bool executing = false;
		bool queuedRecorded = false;
		bool terminalRecorded = false;
		bool occupied = false;
	};

	struct PeerRecord
	{
		PeerIdentity identity{};
		TransportPeer transport;
		std::uint64_t nextExpectedCommandId = 1;
		bool connected = false;
		bool exhausted = false;
		std::array<PendingCommand,
			MaximumCoopTacticalServerPendingCommandsPerPeer> pending{};
		std::size_t pendingCount = 0;
	};

	struct TransientReceipt
	{
		PeerIdentity peerIdentity{};
		std::uint64_t commandId = 0;
		std::uint64_t nextExpectedCommandId = 0;
		std::uint64_t revision = 0;
		CoopTacticalIntentReceiptBytes bytes{};
		bool occupied = false;
	};

	class ListenerWireSink final : public FullEngineCoopServerSessionWireSink
	{
	public:
		explicit ListenerWireSink(
			FullEngineCoopAdmissionListener& listener) noexcept
			: listener_(listener) {}
		bool send(const PeerIdentity& peer,
			CoopTacticalOutboundMessageKind kind,
			const char* messageName,
			const std::uint8_t* bytes,
			std::size_t size) noexcept override;

	private:
		FullEngineCoopAdmissionListener& listener_;
	};

	bool configurationValid() const noexcept;
	bool campaignReady(const PeerIdentity& peer) const noexcept;
	FullEngineCoopTacticalServerResult reconcilePeers(
		FullEngineCoopTacticalServerPumpResult* diagnostics) noexcept;
	PeerRecord* findPeer(const PeerIdentity& peer) noexcept;
	const PeerRecord* findPeer(const PeerIdentity& peer) const noexcept;
	PeerRecord* findOrCreatePeer(const PeerIdentity& peer) noexcept;
	PendingCommand* findPending(
		PeerRecord& peer, std::uint64_t commandId) noexcept;
	bool addPending(PeerRecord& peer,
		std::uint64_t commandId,
		std::uint64_t nextExpectedCommandId,
		std::uint64_t authoritativeSequence) noexcept;
	void removePending(PeerRecord& peer, std::uint64_t commandId) noexcept;
	bool canConsumeInbound(const PeerRecord& peer) const noexcept;
	bool mappingCurrent(const FullEngineCoopTacticalInboundMessage& message,
		PeerRecord*& peer) noexcept;
	FullEngineCoopTacticalServerResult rebuildActorBindings() noexcept;
	bool peerCaughtUp(const PeerIdentity& peer) const noexcept;
	FullEngineCoopTacticalServerResult restagePeerAfterCursorAdvance(
		PeerRecord& peer) noexcept;
	FullEngineCoopTacticalServerResult recordGeneratedReceipt(
		PeerRecord& peer,
		std::uint64_t commandId,
		CoopTacticalIntentReceiptStatus status,
		CoopTacticalIntentReceiptReason reason,
		std::uint64_t authoritativeSequence,
		std::uint64_t simulationTick) noexcept;
	FullEngineCoopTacticalServerResult queueTransientRejection(
		PeerRecord& peer,
		std::uint64_t commandId,
		CoopTacticalIntentReceiptReason reason,
		std::uint64_t simulationTick) noexcept;
	void discardTransientThrough(
		const PeerIdentity& peer, std::uint64_t commandId) noexcept;
	FullEngineCoopTacticalServerResult processIntent(
		const FullEngineCoopTacticalInboundMessage& message,
		std::uint64_t simulationTick,
		FullEngineCoopTacticalServerPumpResult& result) noexcept;
	FullEngineCoopTacticalServerResult processBaselineAck(
		const FullEngineCoopTacticalInboundMessage& message,
		FullEngineCoopTacticalServerPumpResult& result) noexcept;
	FullEngineCoopTacticalServerResult processDeltaAck(
		const FullEngineCoopTacticalInboundMessage& message,
		FullEngineCoopTacticalServerPumpResult& result) noexcept;
	FullEngineCoopTacticalServerResult processResyncRequest(
		const FullEngineCoopTacticalInboundMessage& message,
		FullEngineCoopTacticalServerPumpResult& result) noexcept;
	FullEngineCoopTacticalServerPumpResult flushOutboundInternal() noexcept;
	bool outboundPending() const noexcept;
	void clearState() noexcept;

	FullEngineCoopIngress& ingress_;
	FullEngineCoopAdmissionListener& listener_;
	FullEngineCoopTacticalServerConfiguration configuration_;
	FullEngineCoopServerSession replication_;
	ListenerWireSink wireSink_;
	std::array<PeerIdentity,
		MaximumCoopTacticalSessionPeers> campaignReadyPeers_{};
	std::size_t campaignReadyPeerCount_ = 0;
	std::array<PeerRecord, MaximumCoopTacticalSessionPeers> peers_{};
	std::size_t peerCount_ = 0;
	std::array<TransientReceipt,
		MaximumCoopTacticalServerTransientReceipts> transientReceipts_{};
	std::size_t transientReceiptCount_ = 0;
	TacticalWorldSnapshot baselineSnapshot_;
	bool baselineSnapshotAvailable_ = false;
	std::uint64_t nextAuthoritativeSequence_ = 1;
	FullEngineCoopTacticalInboundMessage deferredInbound_;
	bool deferredInboundOccupied_ = false;
	bool active_ = false;
	bool pumping_ = false;
	bool flushing_ = false;
	bool failed_ = false;
	bool transportRestartRequired_ = false;
};
}

#endif
