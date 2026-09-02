#include "FullEngineCoopTacticalServer.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace CoopSession
{
namespace
{
bool IdentityLess(const PeerIdentity& left, const PeerIdentity& right) noexcept
{
	return std::lexicographical_compare(
		left.begin(), left.end(), right.begin(), right.end());
}

std::uint64_t CommandAfter(std::uint64_t commandId) noexcept
{
	return commandId == (std::numeric_limits<std::uint64_t>::max)()
		? 0 : commandId + 1;
}

CoopTacticalIntentReceiptReason ReceiptReasonFor(
	TacticalIntentAuthorizationReason reason) noexcept
{
	switch (reason)
	{
		case TacticalIntentAuthorizationReason::None:
			return CoopTacticalIntentReceiptReason::None;
		case TacticalIntentAuthorizationReason::NotConfigured:
			return CoopTacticalIntentReceiptReason::UnavailableContext;
		case TacticalIntentAuthorizationReason::NotAdmitted:
		case TacticalIntentAuthorizationReason::ClaimedIdentityMismatch:
			return CoopTacticalIntentReceiptReason::NotAdmitted;
		case TacticalIntentAuthorizationReason::WrongSessionEpoch:
			return CoopTacticalIntentReceiptReason::SessionMismatch;
		case TacticalIntentAuthorizationReason::InvalidIntent:
			return CoopTacticalIntentReceiptReason::MalformedIntent;
		case TacticalIntentAuthorizationReason::WrongGeneration:
			return CoopTacticalIntentReceiptReason::WorldMismatch;
		case TacticalIntentAuthorizationReason::StaleRevision:
		case TacticalIntentAuthorizationReason::FutureRevision:
			return CoopTacticalIntentReceiptReason::RevisionMismatch;
		case TacticalIntentAuthorizationReason::StaleTurn:
		case TacticalIntentAuthorizationReason::FutureTurn:
			return CoopTacticalIntentReceiptReason::TurnMismatch;
		case TacticalIntentAuthorizationReason::InvalidCommandId:
		case TacticalIntentAuthorizationReason::DuplicateCommand:
		case TacticalIntentAuthorizationReason::OutOfOrderCommand:
			return CoopTacticalIntentReceiptReason::InvalidCommandSequence;
		case TacticalIntentAuthorizationReason::SequenceExhausted:
			return CoopTacticalIntentReceiptReason::InboxSequenceExhausted;
		case TacticalIntentAuthorizationReason::ActorNotOwned:
			return CoopTacticalIntentReceiptReason::ActorNotOwned;
		case TacticalIntentAuthorizationReason::PeerCapacityReached:
			return CoopTacticalIntentReceiptReason::InboxCapacityReached;
	}
	return CoopTacticalIntentReceiptReason::UnavailableContext;
}

FullEngineCoopTacticalServerResult ResultForReplication(
	FullEngineCoopServerSessionResult result) noexcept
{
	switch (result)
	{
		case FullEngineCoopServerSessionResult::Success:
			return FullEngineCoopTacticalServerResult::Success;
		case FullEngineCoopServerSessionResult::InvalidConfiguration:
			return FullEngineCoopTacticalServerResult::InvalidConfiguration;
		case FullEngineCoopServerSessionResult::NotActive:
			return FullEngineCoopTacticalServerResult::NotActive;
		case FullEngineCoopServerSessionResult::NoWorld:
			return FullEngineCoopTacticalServerResult::NoWorld;
		case FullEngineCoopServerSessionResult::InvalidAssignment:
		case FullEngineCoopServerSessionResult::AssignmentCapacityReached:
			return FullEngineCoopTacticalServerResult::InvalidAssignment;
		case FullEngineCoopServerSessionResult::PeerCapacityReached:
			return FullEngineCoopTacticalServerResult::PeerCapacityReached;
		case FullEngineCoopServerSessionResult::ReceiptCapacityReached:
			return FullEngineCoopTacticalServerResult::ReceiptCapacityReached;
		case FullEngineCoopServerSessionResult::Busy:
			return FullEngineCoopTacticalServerResult::Busy;
		case FullEngineCoopServerSessionResult::InvalidContext:
		case FullEngineCoopServerSessionResult::StaleContext:
			return FullEngineCoopTacticalServerResult::InvalidContext;
		default:
			return FullEngineCoopTacticalServerResult::ReplicationFailure;
	}
}

bool TerminalStatus(CoopTacticalIntentReceiptStatus status) noexcept
{
	return status == CoopTacticalIntentReceiptStatus::Rejected ||
		status == CoopTacticalIntentReceiptStatus::Applied ||
		status == CoopTacticalIntentReceiptStatus::Discarded ||
		status == CoopTacticalIntentReceiptStatus::Cancelled;
}
}

bool FullEngineCoopTacticalServer::ListenerWireSink::send(
	const PeerIdentity& peer,
	CoopTacticalOutboundMessageKind,
	const char* messageName,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	return listener_.sendToPeer(peer, messageName, bytes, size);
}

FullEngineCoopTacticalServer::FullEngineCoopTacticalServer(
	FullEngineCoopIngress& ingress,
	FullEngineCoopAdmissionListener& listener,
	FullEngineCoopTacticalServerConfiguration configuration) noexcept
	: ingress_(ingress),
	  listener_(listener),
	  configuration_(configuration),
	  replication_(configuration.replication),
	  wireSink_(listener)
{
}

bool FullEngineCoopTacticalServer::configurationValid() const noexcept
{
	return configuration_.maximumInboundMessagesPerPump != 0 &&
		configuration_.maximumInboundMessagesPerPump <=
			MaximumCoopTacticalServerInboundPerPump &&
		configuration_.maximumTransientReceipts != 0 &&
		configuration_.maximumTransientReceipts <=
			MaximumCoopTacticalServerTransientReceipts &&
		configuration_.maximumAuthoritativeSequence != 0;
}

void FullEngineCoopTacticalServer::clearState() noexcept
{
	campaignReadyPeers_ = {};
	campaignReadyPeerCount_ = 0;
	peers_ = {};
	peerCount_ = 0;
	transientReceipts_ = {};
	transientReceiptCount_ = 0;
	baselineSnapshot_ = TacticalWorldSnapshot{};
	baselineSnapshotAvailable_ = false;
	nextAuthoritativeSequence_ = 1;
	deferredInbound_ = FullEngineCoopTacticalInboundMessage{};
	deferredInboundOccupied_ = false;
	failed_ = false;
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::beginEpoch(
	std::uint64_t sessionEpoch) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (active_) return FullEngineCoopTacticalServerResult::AlreadyActive;
	if (!configurationValid())
		return FullEngineCoopTacticalServerResult::InvalidConfiguration;
	if (!ingress_.admissionActive())
		return FullEngineCoopTacticalServerResult::AdmissionInactive;
	if (ingress_.tacticalActive())
		return FullEngineCoopTacticalServerResult::InvalidContext;
	if (sessionEpoch == 0 ||
		ingress_.admissionConfiguration().sessionEpoch != sessionEpoch)
		return FullEngineCoopTacticalServerResult::SessionEpochMismatch;
	const FullEngineCoopServerSessionResult started =
		replication_.beginSession(sessionEpoch);
	if (started != FullEngineCoopServerSessionResult::Success)
		return ResultForReplication(started);
	clearState();
	transportRestartRequired_ = false;
	active_ = true;
	return FullEngineCoopTacticalServerResult::Success;
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::endEpoch()
	noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_) return FullEngineCoopTacticalServerResult::NotActive;
	if (replication_.worldActive())
	{
		const FullEngineCoopTacticalServerResult ended = endWorld();
		if (ended != FullEngineCoopTacticalServerResult::Success) return ended;
	}
	ingress_.endSession();
	replication_.endSession();
	clearState();
	active_ = false;
	transportRestartRequired_ = true;
	return FullEngineCoopTacticalServerResult::Success;
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::beginWorld(
	std::uint64_t worldGeneration,
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (outboundPending())
		return FullEngineCoopTacticalServerResult::PendingReceipts;
	const bool replacing = replication_.worldActive();
	const FullEngineCoopServerSessionResult begun = replication_.beginWorld(
		worldGeneration, revision, turnSerial);
	if (begun != FullEngineCoopServerSessionResult::Success)
		return ResultForReplication(begun);
	const TacticalAuthorityConfigurationResult authorityResult = replacing
		? ingress_.beginGeneration(worldGeneration, revision, turnSerial)
		: (ingress_.beginTacticalSession(TacticalAuthorityContext{
			replication_.sessionEpoch(), worldGeneration, revision, turnSerial}) ==
				FullEngineCoopStartResult::Success
			? TacticalAuthorityConfigurationResult::Success
			: TacticalAuthorityConfigurationResult::AdmissionUnavailable);
	if (authorityResult != TacticalAuthorityConfigurationResult::Success)
	{
		failed_ = true;
		ingress_.endTacticalSession();
		replication_.endWorld();
		return FullEngineCoopTacticalServerResult::InternalFailure;
	}
	ingress_.clearActorBindings();
	baselineSnapshotAvailable_ = false;
	return FullEngineCoopTacticalServerResult::Success;
}

bool FullEngineCoopTacticalServer::outboundPending() const noexcept
{
	if (transientReceiptCount_ != 0) return true;
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].pendingCount != 0) return true;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		CoopTacticalPeerReplicationState state;
		if (replication_.peerState(peers_[index].identity, state) &&
			state.connected && state.pendingReceipts != 0)
			return true;
	}
	return false;
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::endWorld()
	noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (!replication_.worldActive())
		return FullEngineCoopTacticalServerResult::NoWorld;
	if (deferredInboundOccupied_ || listener_.pendingInboundCount() != 0)
		return FullEngineCoopTacticalServerResult::PendingInput;
	const FullEngineCoopTacticalServerPumpResult flushed =
		flushOutboundInternal();
	if (flushed.backpressured)
		return FullEngineCoopTacticalServerResult::TransportBackpressured;
	if (outboundPending())
		return FullEngineCoopTacticalServerResult::PendingReceipts;
	ingress_.clearActorBindings();
	ingress_.endTacticalSession();
	replication_.endWorld();
	baselineSnapshot_ = TacticalWorldSnapshot{};
	baselineSnapshotAvailable_ = false;
	transientReceipts_ = {};
	transientReceiptCount_ = 0;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		peers_[index].pending = {};
		peers_[index].pendingCount = 0;
	}
	transportRestartRequired_ = true;
	return FullEngineCoopTacticalServerResult::Success;
}

bool FullEngineCoopTacticalServer::takeTransportRestartRequired() noexcept
{
	const bool required = transportRestartRequired_;
	transportRestartRequired_ = false;
	return required;
}

FullEngineCoopTacticalServer::PeerRecord*
FullEngineCoopTacticalServer::findPeer(const PeerIdentity& identity) noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == identity) return &peers_[index];
	return nullptr;
}

const FullEngineCoopTacticalServer::PeerRecord*
FullEngineCoopTacticalServer::findPeer(const PeerIdentity& identity) const
	noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == identity) return &peers_[index];
	return nullptr;
}

FullEngineCoopTacticalServer::PeerRecord*
FullEngineCoopTacticalServer::findOrCreatePeer(
	const PeerIdentity& identity) noexcept
{
	if (PeerRecord* existing = findPeer(identity)) return existing;
	if (peerCount_ >= peers_.size()) return nullptr;
	std::size_t insertion = 0;
	while (insertion < peerCount_ &&
		IdentityLess(peers_[insertion].identity, identity))
		++insertion;
	for (std::size_t index = peerCount_; index > insertion; --index)
		peers_[index] = peers_[index - 1];
	peers_[insertion] = PeerRecord{};
	peers_[insertion].identity = identity;
	++peerCount_;
	return &peers_[insertion];
}

bool FullEngineCoopTacticalServer::campaignReady(
	const PeerIdentity& identity) const noexcept
{
	return std::find(campaignReadyPeers_.begin(),
		campaignReadyPeers_.begin() + campaignReadyPeerCount_, identity) !=
		campaignReadyPeers_.begin() + campaignReadyPeerCount_;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::setCampaignReadyPeers(
	const PeerIdentity* readyPeers,
	std::size_t count) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (count > campaignReadyPeers_.size() ||
		(count != 0 && readyPeers == nullptr))
		return FullEngineCoopTacticalServerResult::InvalidPeerSet;
	for (std::size_t index = 0; index < count; ++index)
	{
		if (IsZero(readyPeers[index]) ||
			(index != 0 && !IdentityLess(
				readyPeers[index - 1], readyPeers[index])))
			return FullEngineCoopTacticalServerResult::InvalidPeerSet;
	}

	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> captured{};
	if (count != 0)
		std::copy(readyPeers, readyPeers + count, captured.begin());
	if (count == campaignReadyPeerCount_ &&
		std::equal(captured.begin(), captured.begin() + count,
			campaignReadyPeers_.begin()))
		return reconcilePeers(nullptr);
	campaignReadyPeers_ = captured;
	campaignReadyPeerCount_ = count;
	return reconcilePeers(nullptr);
}

std::size_t FullEngineCoopTacticalServer::campaignReadyPeers(
	std::array<PeerIdentity,
		MaximumCoopTacticalSessionPeers>& output) const noexcept
{
	output = {};
	if (!active_ || failed_) return 0;
	std::copy(campaignReadyPeers_.begin(),
		campaignReadyPeers_.begin() + campaignReadyPeerCount_, output.begin());
	return campaignReadyPeerCount_;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::reconcilePeers() noexcept
{
	return reconcilePeers(nullptr);
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::reconcilePeers(
	FullEngineCoopTacticalServerPumpResult* diagnostics) noexcept
{
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	std::array<FullEngineCoopAuthenticatedPeer,
		MaximumAuthorityPeers> authenticated{};
	std::array<FullEngineCoopAuthenticatedPeer,
		MaximumAuthorityPeers> admitted{};
	const std::size_t admittedCount = listener_.authenticatedPeers(admitted);
	std::size_t authenticatedCount = 0;
	for (std::size_t index = 0; index < admittedCount; ++index)
	{
		if (!campaignReady(admitted[index].peerIdentity)) continue;
		authenticated[authenticatedCount++] = admitted[index];
	}

	bool changed = false;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		PeerRecord& peer = peers_[index];
		if (!peer.connected) continue;
		const auto found = std::find_if(authenticated.begin(),
			authenticated.begin() + authenticatedCount,
			[&peer](const FullEngineCoopAuthenticatedPeer& candidate) {
				return candidate.peerIdentity == peer.identity;
			});
		if (found != authenticated.begin() + authenticatedCount &&
			found->transport == peer.transport)
			continue;
		const FullEngineCoopServerSessionResult disconnected =
			replication_.disconnectPeer(peer.identity);
		if (disconnected != FullEngineCoopServerSessionResult::Success)
			return FullEngineCoopTacticalServerResult::PeerReconciliationFailed;
		peer.connected = false;
		peer.transport = {};
		discardTransientThrough(peer.identity,
			std::numeric_limits<std::uint64_t>::max());
		changed = true;
		if (diagnostics != nullptr) ++diagnostics->peersDisconnected;
	}

	for (std::size_t index = 0; index < authenticatedCount; ++index)
	{
		const FullEngineCoopAuthenticatedPeer& mapping = authenticated[index];
		PeerRecord* peer = findOrCreatePeer(mapping.peerIdentity);
		if (peer == nullptr)
			return FullEngineCoopTacticalServerResult::PeerCapacityReached;
		if (peer->connected) continue;
		const FullEngineCoopServerSessionResult connected =
			replication_.connectPeer(peer->identity);
		if (connected != FullEngineCoopServerSessionResult::Success)
			return ResultForReplication(connected);
		peer->connected = true;
		peer->transport = mapping.transport;
		changed = true;
		if (diagnostics != nullptr) ++diagnostics->peersConnected;
	}
	if (changed)
		return rebuildActorBindings();
	return FullEngineCoopTacticalServerResult::Success;
}

bool FullEngineCoopTacticalServer::peerCaughtUp(
	const PeerIdentity& peer) const noexcept
{
	CoopTacticalPeerReplicationState state;
	return replication_.peerState(peer, state) && state.connected &&
		state.phase == CoopTacticalPeerPhase::Active &&
		state.lastAcknowledgedRevision == replication_.revision() &&
		state.inFlightDeltas == 0;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::rebuildActorBindings() noexcept
{
	ingress_.clearActorBindings();
	if (!replication_.worldActive())
		return FullEngineCoopTacticalServerResult::Success;
	for (std::size_t index = 0; index < replication_.assignmentCount(); ++index)
	{
		const CoopTacticalActorAssignment* assignment =
			replication_.assignment(index);
		if (assignment == nullptr || !peerCaughtUp(assignment->peerIdentity))
			continue;
		const PeerRecord* peer = findPeer(assignment->peerIdentity);
		if (peer == nullptr || !peer->connected ||
			ingress_.bindActorForTransport(peer->transport, assignment->actor) !=
				TacticalActorBindingResult::Success)
		{
			ingress_.clearActorBindings();
			return FullEngineCoopTacticalServerResult::ActorBindingFailure;
		}
	}
	return FullEngineCoopTacticalServerResult::Success;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::replaceAssignments(
	const CoopTacticalActorAssignment* assignments,
	std::size_t count) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (count == replication_.assignmentCount())
	{
		bool identical = true;
		for (std::size_t index = 0; index < count; ++index)
		{
			const CoopTacticalActorAssignment* current =
				replication_.assignment(index);
			if (current == nullptr || assignments == nullptr ||
				current->peerIdentity != assignments[index].peerIdentity ||
				current->actor != assignments[index].actor)
			{
				identical = false;
				break;
			}
		}
		if (identical) return FullEngineCoopTacticalServerResult::Success;
	}
	const FullEngineCoopServerSessionResult replaced =
		replication_.replaceAssignments(assignments, count);
	if (replaced != FullEngineCoopServerSessionResult::Success)
		return ResultForReplication(replaced);
	// Any assignment publication not already behind a fresh-baseline gate
	// invalidates the peer's prior ACK.  Callers should invoke this only when
	// their deterministic assignment set actually changes.
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		PeerRecord& peer = peers_[index];
		if (!peer.connected) continue;
		const CoopTacticalPeerPhase phase =
			replication_.peerPhase(peer.identity);
		if (phase == CoopTacticalPeerPhase::NeedsBaseline ||
			phase == CoopTacticalPeerPhase::ResyncRequired)
			continue;
		const FullEngineCoopServerSessionResult disconnected =
			replication_.disconnectPeer(peer.identity);
		const FullEngineCoopServerSessionResult connected = disconnected ==
			FullEngineCoopServerSessionResult::Success
			? replication_.connectPeer(peer.identity) : disconnected;
		if (connected != FullEngineCoopServerSessionResult::Success)
			return ResultForReplication(connected);
	}
	return rebuildActorBindings();
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::stageBaseline(
	const PeerIdentity& identity,
	const TacticalWorldSnapshot& snapshot) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	PeerRecord* peer = findPeer(identity);
	if (peer == nullptr || !peer->connected)
		return FullEngineCoopTacticalServerResult::PeerReconciliationFailed;
	if (replication_.peerPhase(identity) == CoopTacticalPeerPhase::NeedsBaseline &&
		peer->pendingCount != 0)
		return FullEngineCoopTacticalServerResult::BaselineUnavailable;
	TacticalWorldSnapshot captured;
	if (!snapshot.copyTo(captured))
		return FullEngineCoopTacticalServerResult::BaselineUnavailable;
	const FullEngineCoopServerSessionResult staged = replication_.stageBaseline(
		identity, snapshot, peer->nextExpectedCommandId);
	if (staged != FullEngineCoopServerSessionResult::Success)
		return ResultForReplication(staged);
	discardTransientThrough(identity,
		(std::numeric_limits<std::uint64_t>::max)());
	baselineSnapshot_ = std::move(captured);
	baselineSnapshotAvailable_ = true;
	return rebuildActorBindings();
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::stageBaselines(
	const TacticalWorldSnapshot& snapshot) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	TacticalWorldSnapshot captured;
	if (!snapshot.copyTo(captured))
		return FullEngineCoopTacticalServerResult::BaselineUnavailable;
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> needed{};
	const std::size_t count = replication_.peersNeedingBaseline(needed);
	for (std::size_t index = 0; index < count; ++index)
	{
		PeerRecord* peer = findPeer(needed[index]);
		if (peer == nullptr)
			return FullEngineCoopTacticalServerResult::PeerReconciliationFailed;
		if (replication_.peerPhase(peer->identity) ==
				CoopTacticalPeerPhase::NeedsBaseline &&
			peer->pendingCount != 0)
			continue;
		const FullEngineCoopServerSessionResult staged =
			replication_.stageBaseline(
				peer->identity, snapshot, peer->nextExpectedCommandId);
		if (staged != FullEngineCoopServerSessionResult::Success)
		{
			(void)rebuildActorBindings();
			return ResultForReplication(staged);
		}
		discardTransientThrough(peer->identity,
			(std::numeric_limits<std::uint64_t>::max)());
	}
	baselineSnapshot_ = std::move(captured);
	baselineSnapshotAvailable_ = true;
	return rebuildActorBindings();
}

std::size_t FullEngineCoopTacticalServer::peersNeedingBaseline(
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers>& peers) const
	noexcept
{
	peers = {};
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> needed{};
	const std::size_t count = replication_.peersNeedingBaseline(needed);
	std::size_t published = 0;
	for (std::size_t index = 0; index < count; ++index)
	{
		const PeerRecord* peer = findPeer(needed[index]);
		if (peer == nullptr) continue;
		if (replication_.peerPhase(peer->identity) ==
				CoopTacticalPeerPhase::NeedsBaseline &&
			peer->pendingCount != 0)
			continue;
		peers[published++] = needed[index];
	}
	return published;
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::publishDelta(
	const TacticalWorldDelta& delta,
	std::uint64_t resultingRevision,
	std::uint64_t resultingTurnSerial) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	const FullEngineCoopServerSessionResult published =
		replication_.publishDelta(
			delta, resultingRevision, resultingTurnSerial);
	if (published != FullEngineCoopServerSessionResult::Success)
		return ResultForReplication(published);
	if (ingress_.advanceContext(resultingRevision, resultingTurnSerial) !=
		TacticalAuthorityConfigurationResult::Success)
	{
		failed_ = true;
		ingress_.clearActorBindings();
		return FullEngineCoopTacticalServerResult::InternalFailure;
	}
	baselineSnapshotAvailable_ = false;
	return rebuildActorBindings();
}

FullEngineCoopTacticalServer::PendingCommand*
FullEngineCoopTacticalServer::findPending(
	PeerRecord& peer, std::uint64_t commandId) noexcept
{
	for (PendingCommand& pending : peer.pending)
		if (pending.occupied && pending.commandId == commandId) return &pending;
	return nullptr;
}

bool FullEngineCoopTacticalServer::addPending(
	PeerRecord& peer,
	std::uint64_t commandId,
	std::uint64_t nextExpectedCommandId,
	std::uint64_t authoritativeSequence) noexcept
{
	if (findPending(peer, commandId) != nullptr) return true;
	if (peer.pendingCount >=
		configuration_.replication.maximumReceiptHistoryPerPeer)
		return false;
	for (PendingCommand& pending : peer.pending)
	{
		if (pending.occupied) continue;
		pending.commandId = commandId;
		pending.nextExpectedCommandId = nextExpectedCommandId;
		pending.authoritativeSequence = authoritativeSequence;
		pending.occupied = true;
		++peer.pendingCount;
		return true;
	}
	return false;
}

void FullEngineCoopTacticalServer::removePending(
	PeerRecord& peer, std::uint64_t commandId) noexcept
{
	PendingCommand* pending = findPending(peer, commandId);
	if (pending == nullptr) return;
	*pending = PendingCommand{};
	if (peer.pendingCount != 0) --peer.pendingCount;
}

bool FullEngineCoopTacticalServer::canConsumeInbound(
	const PeerRecord& peer) const noexcept
{
	if (!ingress_.tacticalExecutionReady())
		return false;
	if (peer.pendingCount >=
			configuration_.replication.maximumReceiptHistoryPerPeer)
		return false;
	CoopTacticalPeerReplicationState state;
	if (replication_.peerState(peer.identity, state) &&
		state.retainedReceipts >=
				configuration_.replication.maximumReceiptHistoryPerPeer &&
		state.pendingReceipts != 0)
		return false;
	return true;
}

bool FullEngineCoopTacticalServer::mappingCurrent(
	const FullEngineCoopTacticalInboundMessage& message,
	PeerRecord*& peer) noexcept
{
	if (!campaignReady(message.peerIdentity)) return false;
	peer = findPeer(message.peerIdentity);
	if (peer == nullptr || !peer->connected ||
		peer->transport != message.transport)
		return false;
	PeerIdentity resolved{};
	return listener_.authenticatedPeerForTransport(
		message.transport, resolved) && resolved == peer->identity;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::restagePeerAfterCursorAdvance(
	PeerRecord& peer) noexcept
{
	const CoopTacticalPeerPhase phase = replication_.peerPhase(peer.identity);
	if (phase == CoopTacticalPeerPhase::Active ||
		phase == CoopTacticalPeerPhase::Offline ||
		phase == CoopTacticalPeerPhase::AwaitingBaselineAck)
		return FullEngineCoopTacticalServerResult::Success;
	if (baselineSnapshotAvailable_)
	{
		const FullEngineCoopServerSessionResult staged =
			replication_.stageBaseline(peer.identity, baselineSnapshot_,
				peer.nextExpectedCommandId);
		if (staged == FullEngineCoopServerSessionResult::Success)
			discardTransientThrough(peer.identity,
				(std::numeric_limits<std::uint64_t>::max)());
		return ResultForReplication(staged);
	}
	const FullEngineCoopServerSessionResult disconnected =
		replication_.disconnectPeer(peer.identity);
	const FullEngineCoopServerSessionResult connected =
		disconnected == FullEngineCoopServerSessionResult::Success
		? replication_.connectPeer(peer.identity)
		: disconnected;
	return connected == FullEngineCoopServerSessionResult::Success
		? FullEngineCoopTacticalServerResult::BaselineUnavailable
		: ResultForReplication(connected);
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::recordGeneratedReceipt(
	PeerRecord& peer,
	std::uint64_t commandId,
	CoopTacticalIntentReceiptStatus status,
	CoopTacticalIntentReceiptReason reason,
	std::uint64_t authoritativeSequence,
	std::uint64_t simulationTick) noexcept
{
	CoopTacticalIntentReceipt receipt;
	receipt.state.sessionEpoch = replication_.sessionEpoch();
	receipt.state.worldGeneration = replication_.worldGeneration();
	receipt.state.revision = replication_.revision();
	receipt.state.turnSerial = replication_.turnSerial();
	receipt.peerIdentity = peer.identity;
	receipt.commandId = commandId;
	receipt.nextExpectedCommandId = peer.nextExpectedCommandId;
	receipt.authoritativeSequence = authoritativeSequence;
	receipt.simulationTick = simulationTick;
	receipt.status = status;
	receipt.reason = reason;
	const FullEngineCoopServerSessionResult recorded =
		replication_.recordReceipt(receipt);
	return recorded == FullEngineCoopServerSessionResult::Success
		? FullEngineCoopTacticalServerResult::Success
		: ResultForReplication(recorded);
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::queueTransientRejection(
	PeerRecord& peer,
	std::uint64_t commandId,
	CoopTacticalIntentReceiptReason reason,
	std::uint64_t simulationTick) noexcept
{
	for (std::size_t index = 0; index < transientReceiptCount_; ++index)
	{
		const TransientReceipt& existing = transientReceipts_[index];
		if (existing.peerIdentity == peer.identity &&
			existing.commandId == commandId &&
			existing.nextExpectedCommandId == peer.nextExpectedCommandId)
			return FullEngineCoopTacticalServerResult::Success;
	}
	if (transientReceiptCount_ >= configuration_.maximumTransientReceipts)
		return FullEngineCoopTacticalServerResult::InputRejected;
	CoopTacticalIntentReceipt receipt;
	receipt.state.sessionEpoch = replication_.sessionEpoch();
	receipt.state.worldGeneration = replication_.worldGeneration();
	receipt.state.revision = replication_.revision();
	receipt.state.turnSerial = replication_.turnSerial();
	receipt.peerIdentity = peer.identity;
	receipt.commandId = commandId;
	receipt.nextExpectedCommandId = peer.nextExpectedCommandId;
	receipt.simulationTick = simulationTick;
	receipt.status = CoopTacticalIntentReceiptStatus::Rejected;
	receipt.reason = reason;
	TransientReceipt accepted;
	accepted.peerIdentity = peer.identity;
	accepted.commandId = commandId;
	accepted.nextExpectedCommandId = peer.nextExpectedCommandId;
	accepted.revision = replication_.revision();
	if (EncodeCoopTacticalIntentReceipt(receipt, accepted.bytes) !=
		CoopTacticalCodecResult::Success)
		return FullEngineCoopTacticalServerResult::ReceiptRejected;
	accepted.occupied = true;
	transientReceipts_[transientReceiptCount_++] = accepted;
	return FullEngineCoopTacticalServerResult::Success;
}

void FullEngineCoopTacticalServer::discardTransientThrough(
	const PeerIdentity& peer, std::uint64_t commandId) noexcept
{
	std::size_t index = 0;
	while (index < transientReceiptCount_)
	{
		if (transientReceipts_[index].peerIdentity != peer ||
			transientReceipts_[index].commandId > commandId)
		{
			++index;
			continue;
		}
		for (std::size_t move = index + 1;
			move < transientReceiptCount_; ++move)
			transientReceipts_[move - 1] = transientReceipts_[move];
		transientReceipts_[--transientReceiptCount_] = TransientReceipt{};
	}
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::processIntent(
	const FullEngineCoopTacticalInboundMessage& message,
	std::uint64_t simulationTick,
	FullEngineCoopTacticalServerPumpResult& diagnostics) noexcept
{
	PeerRecord* peer = nullptr;
	if (!mappingCurrent(message, peer))
		return FullEngineCoopTacticalServerResult::InputRejected;
	TacticalIntent intent;
	const TacticalIntentCodecResult decoded = DecodeTacticalIntent(
		message.bytes.data(), message.size, intent);
	if (decoded != TacticalIntentCodecResult::Success ||
		intent.sessionEpoch != replication_.sessionEpoch())
		return FullEngineCoopTacticalServerResult::InputRejected;
	diagnostics.peerIdentity = peer->identity;
	diagnostics.commandId = intent.commandId;
	const CoopTacticalPeerPhase replicationPhase =
		replication_.peerPhase(peer->identity);
	if (replicationPhase == CoopTacticalPeerPhase::NeedsBaseline ||
		 replicationPhase == CoopTacticalPeerPhase::ResyncRequired ||
		 replicationPhase == CoopTacticalPeerPhase::AwaitingBaselineAck)
		return FullEngineCoopTacticalServerResult::InputRejected;
	if (peer->exhausted || peer->nextExpectedCommandId == 0)
		return queueTransientRejection(*peer, intent.commandId,
			CoopTacticalIntentReceiptReason::InboxSequenceExhausted,
			simulationTick);
	if (intent.commandId < peer->nextExpectedCommandId)
	{
		const FullEngineCoopServerSessionResult replayed =
			replication_.replayReceipt(peer->identity, intent.commandId);
		if (replayed == FullEngineCoopServerSessionResult::Success)
		{
			++diagnostics.duplicateReceiptsReplayed;
			return FullEngineCoopTacticalServerResult::Success;
		}
		return queueTransientRejection(*peer, intent.commandId,
			CoopTacticalIntentReceiptReason::InvalidCommandSequence,
			simulationTick);
	}
	if (intent.commandId > peer->nextExpectedCommandId)
		return queueTransientRejection(*peer, intent.commandId,
			CoopTacticalIntentReceiptReason::InvalidCommandSequence,
			simulationTick);
	// The passive client exposes a one-command causal lock, but this boundary is
	// untrusted. Do not let a custom peer authorize multiple commands against
	// one replica revision while its earlier command remains retained. This is
	// a transient, non-consuming sequence rejection: the exact command ID can
	// be retried after the prior terminal result releases the reservation.
	if (peer->pendingCount != 0)
		return queueTransientRejection(*peer, intent.commandId,
			CoopTacticalIntentReceiptReason::InvalidCommandSequence,
			simulationTick);
	if (!canConsumeInbound(*peer))
		return FullEngineCoopTacticalServerResult::ExecutionBackpressured;

	CoopTacticalPeerReplicationState replicationState;
	if (!replication_.peerState(peer->identity, replicationState))
		return FullEngineCoopTacticalServerResult::PeerReconciliationFailed;
	const bool caughtUp = peerCaughtUp(peer->identity);
	const bool policyReject = !caughtUp || nextAuthoritativeSequence_ == 0;
	const std::uint64_t expectedAfter = intent.commandId ==
		std::numeric_limits<std::uint64_t>::max()
		? 0 : intent.commandId + 1;
	PendingCommand* reservation = nullptr;
	if (!policyReject)
	{
		if (!addPending(*peer, intent.commandId, expectedAfter,
			nextAuthoritativeSequence_))
			return FullEngineCoopTacticalServerResult::ReceiptCapacityReached;
		reservation = findPending(*peer, intent.commandId);
		if (reservation == nullptr)
			return FullEngineCoopTacticalServerResult::InternalFailure;
		reservation->executing = true;
	}
	const TacticalIntentIngressResult ingressResult = policyReject
		? ingress_.rejectTacticalIntent(peer->transport,
			message.bytes.data(), message.size)
		: ingress_.handleTacticalIntent(peer->transport,
			message.bytes.data(), message.size);
	if (reservation != nullptr) reservation->executing = false;
	diagnostics.authorizationReason = ingressResult.authorization.reason;
	if (ingressResult.decodeResult != TacticalIntentCodecResult::Success)
	{
		if (reservation != nullptr) removePending(*peer, intent.commandId);
		return FullEngineCoopTacticalServerResult::InputRejected;
	}
	if (!ingressResult.authorization.commandConsumed ||
		ingressResult.authorization.peerIdentity != peer->identity ||
		ingressResult.authorization.commandId != intent.commandId)
	{
		if (reservation != nullptr) removePending(*peer, intent.commandId);
		if (!policyReject && !ingress_.tacticalExecutionReady())
			return FullEngineCoopTacticalServerResult::ExecutionBackpressured;
		return FullEngineCoopTacticalServerResult::SequenceDiverged;
	}
	if (ingressResult.authorization.nextExpectedCommandId != expectedAfter)
	{
		if (reservation != nullptr) removePending(*peer, intent.commandId);
		return FullEngineCoopTacticalServerResult::SequenceDiverged;
	}
	peer->nextExpectedCommandId =
		ingressResult.authorization.nextExpectedCommandId;
	peer->exhausted = peer->nextExpectedCommandId == 0;
	discardTransientThrough(peer->identity, intent.commandId);
	++diagnostics.intentsConsumed;

	FullEngineCoopTacticalServerResult receiptResult;
	if (policyReject)
	{
		receiptResult = recordGeneratedReceipt(*peer, intent.commandId,
			CoopTacticalIntentReceiptStatus::Rejected,
			nextAuthoritativeSequence_ == 0
				? CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted
				: CoopTacticalIntentReceiptReason::NotBaselineReady,
			0, simulationTick);
		++diagnostics.inputsRejected;
	}
	else if (!ingressResult.authorization)
	{
		removePending(*peer, intent.commandId);
		reservation = nullptr;
		receiptResult = recordGeneratedReceipt(*peer, intent.commandId,
			CoopTacticalIntentReceiptStatus::Rejected,
			ReceiptReasonFor(ingressResult.authorization.reason),
			0, simulationTick);
		++diagnostics.inputsRejected;
	}
	else if (!ingressResult.executionAttempted)
	{
		removePending(*peer, intent.commandId);
		return FullEngineCoopTacticalServerResult::ExecutionBackpressured;
	}
	else if (ingressResult.execution ==
		TacticalIntentExecutionDisposition::Rejected)
	{
		if (reservation == nullptr)
			return FullEngineCoopTacticalServerResult::InternalFailure;
		if (!reservation->terminalRecorded)
			return FullEngineCoopTacticalServerResult::ExecutionBackpressured;
		receiptResult = FullEngineCoopTacticalServerResult::Success;
		removePending(*peer, intent.commandId);
		++diagnostics.inputsRejected;
	}
	else
	{
		if (reservation == nullptr)
			return FullEngineCoopTacticalServerResult::InternalFailure;
		const std::uint64_t authoritativeSequence =
			reservation->authoritativeSequence;
		nextAuthoritativeSequence_ = authoritativeSequence ==
			configuration_.maximumAuthoritativeSequence
			? 0 : authoritativeSequence + 1;
		const bool retained = ingressResult.execution ==
			TacticalIntentExecutionDisposition::Retained;
		if (retained && reservation->terminalRecorded)
			return FullEngineCoopTacticalServerResult::ReceiptRejected;
		if (retained)
		{
			if (!reservation->queuedRecorded)
				return FullEngineCoopTacticalServerResult::ExecutionBackpressured;
			receiptResult = FullEngineCoopTacticalServerResult::Success;
		}
		else
		{
			if (!reservation->terminalRecorded)
				return FullEngineCoopTacticalServerResult::ExecutionBackpressured;
			receiptResult = FullEngineCoopTacticalServerResult::Success;
			removePending(*peer, intent.commandId);
		}
	}
	if (receiptResult != FullEngineCoopTacticalServerResult::Success)
		return receiptResult;
	const FullEngineCoopTacticalServerResult restaged =
		restagePeerAfterCursorAdvance(*peer);
	return restaged == FullEngineCoopTacticalServerResult::BaselineUnavailable
		? FullEngineCoopTacticalServerResult::Success : restaged;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::processBaselineAck(
	const FullEngineCoopTacticalInboundMessage& message,
	FullEngineCoopTacticalServerPumpResult& diagnostics) noexcept
{
	PeerRecord* peer = nullptr;
	if (!mappingCurrent(message, peer))
		return FullEngineCoopTacticalServerResult::InputRejected;
	const FullEngineCoopServerSessionResult acknowledged =
		replication_.acknowledgeBaseline(
			peer->identity, message.bytes.data(), message.size);
	diagnostics.replicationResult = acknowledged;
	if (acknowledged == FullEngineCoopServerSessionResult::ResyncRequired)
	{
		diagnostics.resyncRequired = true;
		(void)rebuildActorBindings();
		return FullEngineCoopTacticalServerResult::InputRejected;
	}
	if (acknowledged != FullEngineCoopServerSessionResult::Success)
		return FullEngineCoopTacticalServerResult::InputRejected;
	const FullEngineCoopTacticalServerResult rebound = rebuildActorBindings();
	if (rebound == FullEngineCoopTacticalServerResult::Success)
		++diagnostics.acknowledgementsAccepted;
	return rebound;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::processDeltaAck(
	const FullEngineCoopTacticalInboundMessage& message,
	FullEngineCoopTacticalServerPumpResult& diagnostics) noexcept
{
	PeerRecord* peer = nullptr;
	if (!mappingCurrent(message, peer))
		return FullEngineCoopTacticalServerResult::InputRejected;
	const FullEngineCoopServerSessionResult acknowledged =
		replication_.acknowledgeDelta(
			peer->identity, message.bytes.data(), message.size);
	diagnostics.replicationResult = acknowledged;
	if (acknowledged == FullEngineCoopServerSessionResult::ResyncRequired)
	{
		diagnostics.resyncRequired = true;
		(void)rebuildActorBindings();
		return FullEngineCoopTacticalServerResult::InputRejected;
	}
	if (acknowledged != FullEngineCoopServerSessionResult::Success)
		return FullEngineCoopTacticalServerResult::InputRejected;
	const FullEngineCoopTacticalServerResult rebound = rebuildActorBindings();
	if (rebound == FullEngineCoopTacticalServerResult::Success)
		++diagnostics.acknowledgementsAccepted;
	return rebound;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::processResyncRequest(
	const FullEngineCoopTacticalInboundMessage& message,
	FullEngineCoopTacticalServerPumpResult& diagnostics) noexcept
{
	PeerRecord* peer = nullptr;
	if (!mappingCurrent(message, peer))
		return FullEngineCoopTacticalServerResult::InputRejected;
	CoopTacticalResyncRequest request;
	if (DecodeCoopTacticalResyncRequest(
		message.bytes.data(), message.size, request) !=
		CoopTacticalCodecResult::Success)
		return FullEngineCoopTacticalServerResult::InputRejected;
	const bool exactCursor =
		request.nextExpectedCommandId == peer->nextExpectedCommandId;
	const bool retainedReceiptEvidence = replication_.hasRetainedReceipt(
		peer->identity, request.nextExpectedCommandId);
	const bool retainedConsumedCommand =
		CommandAfter(request.nextExpectedCommandId) ==
			peer->nextExpectedCommandId &&
		(findPending(*peer, request.nextExpectedCommandId) != nullptr ||
		 retainedReceiptEvidence);
	if (!exactCursor && !retainedConsumedCommand)
		return FullEngineCoopTacticalServerResult::InputRejected;
	bool replicationReset = false;
	const FullEngineCoopServerSessionResult requested =
		replication_.requestResync(
			peer->identity, message.bytes.data(), message.size,
			&replicationReset);
	diagnostics.replicationResult = requested;
	if (requested != FullEngineCoopServerSessionResult::Success)
		return FullEngineCoopTacticalServerResult::InputRejected;
	if (replicationReset)
	{
		discardTransientThrough(peer->identity,
			(std::numeric_limits<std::uint64_t>::max)());
		if (retainedConsumedCommand && retainedReceiptEvidence &&
			replication_.replayReceipt(
				peer->identity, request.nextExpectedCommandId) !=
				FullEngineCoopServerSessionResult::Success)
			return FullEngineCoopTacticalServerResult::InputRejected;
		diagnostics.resyncRequired = true;
	}
	return rebuildActorBindings();
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::recordReceipt(
	const CoopTacticalIntentReceipt& supplied) noexcept
{
	if (flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (!replication_.worldActive())
		return FullEngineCoopTacticalServerResult::NoWorld;
	if (supplied.status != CoopTacticalIntentReceiptStatus::Queued &&
		!TerminalStatus(supplied.status))
		return FullEngineCoopTacticalServerResult::ReceiptRejected;
	PeerRecord* peer = findPeer(supplied.peerIdentity);
	if (peer == nullptr)
		return FullEngineCoopTacticalServerResult::ReceiptRejected;
	PendingCommand* pending = findPending(*peer, supplied.commandId);
	if (pending == nullptr)
		return pumping_ ? FullEngineCoopTacticalServerResult::Busy
			: FullEngineCoopTacticalServerResult::ReceiptRejected;
	if (supplied.status == CoopTacticalIntentReceiptStatus::Rejected &&
		(supplied.reason ==
			CoopTacticalIntentReceiptReason::InvalidCommandSequence ||
		 supplied.reason ==
			CoopTacticalIntentReceiptReason::InboxSequenceExhausted))
		return FullEngineCoopTacticalServerResult::ReceiptRejected;
	if (pumping_ && !pending->executing)
		return FullEngineCoopTacticalServerResult::Busy;
	if ((supplied.status == CoopTacticalIntentReceiptStatus::Queued &&
		pending->terminalRecorded) ||
		(TerminalStatus(supplied.status) && pending->terminalRecorded))
		return FullEngineCoopTacticalServerResult::ReceiptRejected;
	CoopTacticalIntentReceipt normalized = supplied;
	normalized.state = {};
	normalized.state.sessionEpoch = replication_.sessionEpoch();
	normalized.state.worldGeneration = replication_.worldGeneration();
	normalized.state.revision = replication_.revision();
	normalized.state.turnSerial = replication_.turnSerial();
	normalized.peerIdentity = peer->identity;
	normalized.nextExpectedCommandId = pending->nextExpectedCommandId;
	normalized.authoritativeSequence =
		normalized.status == CoopTacticalIntentReceiptStatus::Rejected
		? 0 : pending->authoritativeSequence;
	const FullEngineCoopServerSessionResult recorded =
		replication_.recordReceipt(normalized);
	if (recorded != FullEngineCoopServerSessionResult::Success)
	{
		if (recorded == FullEngineCoopServerSessionResult::CodecFailure ||
			recorded == FullEngineCoopServerSessionResult::ConflictingReceipt ||
			recorded == FullEngineCoopServerSessionResult::InvalidContext)
			return FullEngineCoopTacticalServerResult::ReceiptRejected;
		return ResultForReplication(recorded);
	}
	if (normalized.status == CoopTacticalIntentReceiptStatus::Queued)
		pending->queuedRecorded = true;
	else
		pending->terminalRecorded = true;
	if (pending->terminalRecorded && !pending->executing)
		removePending(*peer, supplied.commandId);
	return FullEngineCoopTacticalServerResult::Success;
}

FullEngineCoopTacticalServerPumpResult
FullEngineCoopTacticalServer::flushOutboundInternal() noexcept
{
	FullEngineCoopTacticalServerPumpResult result;
	if (!active_ || failed_)
	{
		result.result = FullEngineCoopTacticalServerResult::NotActive;
		return result;
	}
	if (!replication_.worldActive())
	{
		result.result = FullEngineCoopTacticalServerResult::NoWorld;
		return result;
	}
	if (flushing_)
	{
		result.result = FullEngineCoopTacticalServerResult::Busy;
		return result;
	}
	struct Guard
	{
		explicit Guard(bool& value) noexcept : value_(value) { value_ = true; }
		~Guard() { value_ = false; }
		bool& value_;
	} guard(flushing_);
	const FullEngineCoopServerSessionFlushResult flushed =
		replication_.flush(wireSink_);
	result.replicationResult = flushed.result;
	result.messagesSent = flushed.messagesSent;
	if (flushed.backpressured)
	{
		result.result = FullEngineCoopTacticalServerResult::TransportBackpressured;
		result.backpressured = true;
		return result;
	}
	if (flushed.result == FullEngineCoopServerSessionResult::ResyncRequired)
	{
		result.resyncRequired = true;
		(void)rebuildActorBindings();
	}
	else if (flushed.result != FullEngineCoopServerSessionResult::Success)
	{
		result.result = ResultForReplication(flushed.result);
		return result;
	}

	std::size_t index = 0;
	while (index < transientReceiptCount_)
	{
		TransientReceipt& receipt = transientReceipts_[index];
		CoopTacticalPeerReplicationState state;
		if (!replication_.peerState(receipt.peerIdentity, state) ||
			!state.connected || state.phase != CoopTacticalPeerPhase::Active ||
			receipt.revision > state.lastSentRevision)
		{
			++index;
			continue;
		}
		if (!listener_.sendToPeer(receipt.peerIdentity,
			CoopTacticalIntentReceiptMessageName,
			receipt.bytes.data(), receipt.bytes.size()))
		{
			result.result =
				FullEngineCoopTacticalServerResult::TransportBackpressured;
			result.backpressured = true;
			return result;
		}
		++result.messagesSent;
		for (std::size_t move = index + 1;
			move < transientReceiptCount_; ++move)
			transientReceipts_[move - 1] = transientReceipts_[move];
		transientReceipts_[--transientReceiptCount_] = TransientReceipt{};
	}
	result.transportRestartRequired = transportRestartRequired_;
	return result;
}

FullEngineCoopTacticalServerPumpResult
FullEngineCoopTacticalServer::flushOutbound() noexcept
{
	if (pumping_)
	{
		FullEngineCoopTacticalServerPumpResult result;
		result.result = FullEngineCoopTacticalServerResult::Busy;
		return result;
	}
	return flushOutboundInternal();
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::discardInboundAfterTransportStop() noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (listener_.running() || listener_.authenticatedPeerCount() != 0 ||
		ingress_.boundPeerCount() != 0 || replication_.connectedPeerCount() != 0)
		return FullEngineCoopTacticalServerResult::InvalidContext;

	deferredInbound_ = FullEngineCoopTacticalInboundMessage{};
	deferredInboundOccupied_ = false;
	FullEngineCoopTacticalInboundMessage discarded;
	while (listener_.popInbound(discarded))
		discarded = FullEngineCoopTacticalInboundMessage{};
	return FullEngineCoopTacticalServerResult::Success;
}

FullEngineCoopTacticalServerResult
FullEngineCoopTacticalServer::discardInboundAfterSelfRetirementGate() noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (!listener_.running() || !listener_.selfRetirementInputFrozen() ||
		ingress_.pendingSelfRetirementCount() == 0)
		return FullEngineCoopTacticalServerResult::InvalidContext;

	deferredInbound_ = FullEngineCoopTacticalInboundMessage{};
	deferredInboundOccupied_ = false;
	if (!listener_.discardInboundForSelfRetirement())
		return FullEngineCoopTacticalServerResult::InvalidContext;
	return FullEngineCoopTacticalServerResult::Success;
}

FullEngineCoopTacticalServerResult FullEngineCoopTacticalServer::retirePeer(
	const PeerIdentity& identity) noexcept
{
	if (pumping_ || flushing_) return FullEngineCoopTacticalServerResult::Busy;
	if (!active_ || failed_) return FullEngineCoopTacticalServerResult::NotActive;
	if (!ingress_.credentialRetired(identity) ||
		ingress_.pendingSelfRetirementCount() != 0 || listener_.running() ||
		listener_.authenticatedPeerCount() != 0 ||
		ingress_.boundPeerCount() != 0 || deferredInboundOccupied_ ||
		listener_.pendingInboundCount() != 0 ||
		listener_.pendingCampaignInboundCount() != 0)
		return FullEngineCoopTacticalServerResult::InvalidContext;

	std::size_t peerIndex = 0;
	while (peerIndex < peerCount_ && peers_[peerIndex].identity != identity)
		++peerIndex;
	// Retirement compacts three independently bounded tables. Require the
	// listener-stop reconciliation to have made every retained coordinator and
	// replication record an exact offline pair before mutating any of them. This
	// makes the final actor-binding rebuild infallible and prevents a stale live
	// survivor from being left behind after partial compaction.
	if (replication_.peerCount() != peerCount_)
		return FullEngineCoopTacticalServerResult::InvalidContext;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		const PeerRecord& retained = peers_[index];
		CoopTacticalPeerReplicationState state;
		if (retained.connected || retained.pendingCount != 0 ||
			!replication_.peerState(retained.identity, state) ||
			state.connected || state.phase != CoopTacticalPeerPhase::Offline ||
			state.inFlightDeltas != 0)
			return FullEngineCoopTacticalServerResult::InvalidContext;
	}
	if (transientReceiptCount_ != 0)
		return FullEngineCoopTacticalServerResult::InvalidContext;
	if (!ingress_.canRetireTacticalAuthorityPeer(identity))
		return FullEngineCoopTacticalServerResult::InvalidContext;

	CoopTacticalPeerReplicationState replicationState;
	const bool replicationRetained =
		replication_.peerState(identity, replicationState);
	if (replicationRetained != (peerIndex != peerCount_) ||
		(replicationRetained && (replicationState.connected ||
			replicationState.phase != CoopTacticalPeerPhase::Offline ||
			replicationState.inFlightDeltas != 0)))
		return FullEngineCoopTacticalServerResult::InvalidContext;
	if (replicationRetained)
	{
		const FullEngineCoopServerSessionResult retired =
			replication_.retirePeer(identity);
		if (retired != FullEngineCoopServerSessionResult::Success)
			return ResultForReplication(retired);
	}
	// The preflight above is stable on this single-threaded boundary. Compact
	// the admission-epoch sequence only after replication's fallible atomic
	// compaction; survivor cursors are moved unchanged.
	if (!ingress_.retireTacticalAuthorityPeer(identity))
		return FullEngineCoopTacticalServerResult::InvalidContext;

	std::size_t readyOutput = 0;
	for (std::size_t index = 0; index < campaignReadyPeerCount_; ++index)
	{
		if (campaignReadyPeers_[index] == identity) continue;
		if (readyOutput != index)
			campaignReadyPeers_[readyOutput] = campaignReadyPeers_[index];
		++readyOutput;
	}
	for (std::size_t index = readyOutput; index < campaignReadyPeerCount_; ++index)
		campaignReadyPeers_[index] = PeerIdentity{};
	campaignReadyPeerCount_ = readyOutput;

	if (peerIndex != peerCount_)
	{
		for (std::size_t index = peerIndex + 1; index < peerCount_; ++index)
			peers_[index - 1] = std::move(peers_[index]);
		peers_[--peerCount_] = PeerRecord{};
	}
	return rebuildActorBindings();
}

FullEngineCoopTacticalServerPumpResult
FullEngineCoopTacticalServer::pumpInbound(
	std::uint64_t simulationTick) noexcept
{
	FullEngineCoopTacticalServerPumpResult diagnostics;
	if (!active_ || failed_)
	{
		diagnostics.result = FullEngineCoopTacticalServerResult::NotActive;
		return diagnostics;
	}
	if (pumping_ || flushing_)
	{
		diagnostics.result = FullEngineCoopTacticalServerResult::Busy;
		return diagnostics;
	}
	struct Guard
	{
		explicit Guard(bool& value) noexcept : value_(value) { value_ = true; }
		~Guard() { value_ = false; }
		bool& value_;
	} guard(pumping_);

	FullEngineCoopTacticalServerResult reconciled =
		reconcilePeers(&diagnostics);
	if (reconciled != FullEngineCoopTacticalServerResult::Success)
	{
		diagnostics.result = reconciled;
		return diagnostics;
	}
	if (!replication_.worldActive())
	{
		FullEngineCoopTacticalInboundMessage discarded;
		if (deferredInboundOccupied_)
		{
			deferredInbound_ = FullEngineCoopTacticalInboundMessage{};
			deferredInboundOccupied_ = false;
			++diagnostics.inputsRejected;
		}
		while (diagnostics.inboundConsumed <
			configuration_.maximumInboundMessagesPerPump &&
			listener_.popInbound(discarded))
		{
			++diagnostics.inboundConsumed;
			++diagnostics.inputsRejected;
		}
		diagnostics.result = diagnostics.inputsRejected == 0
			? FullEngineCoopTacticalServerResult::NoWorld
			: FullEngineCoopTacticalServerResult::InputRejected;
		diagnostics.transportRestartRequired = transportRestartRequired_;
		return diagnostics;
	}

	FullEngineCoopTacticalServerPumpResult before = flushOutboundInternal();
	diagnostics.messagesSent += before.messagesSent;
	diagnostics.replicationResult = before.replicationResult;
	diagnostics.resyncRequired = before.resyncRequired;
	if (before.backpressured)
	{
		diagnostics.result = before.result;
		diagnostics.backpressured = true;
		return diagnostics;
	}
	if (before.result != FullEngineCoopTacticalServerResult::Success)
	{
		diagnostics.result = before.result;
		failed_ = true;
		ingress_.clearActorBindings();
		return diagnostics;
	}
	FullEngineCoopTacticalInboundMessage message;
	std::size_t processedCount = 0;
	while (processedCount < configuration_.maximumInboundMessagesPerPump)
	{
		FullEngineCoopTacticalInboundKind headKind =
			FullEngineCoopTacticalInboundKind::Intent;
		const bool hasHead = deferredInboundOccupied_
			? (headKind = deferredInbound_.kind, true)
			: listener_.peekInboundKind(headKind);
		if (!hasHead) break;
		if (deferredInboundOccupied_)
		{
			message = deferredInbound_;
			deferredInbound_ = FullEngineCoopTacticalInboundMessage{};
			deferredInboundOccupied_ = false;
		}
		else
		{
			if (!listener_.popInbound(message)) break;
			++diagnostics.inboundConsumed;
		}
		++processedCount;
		const std::size_t intentsBefore = diagnostics.intentsConsumed;
		FullEngineCoopTacticalServerResult processed;
		switch (message.kind)
		{
			case FullEngineCoopTacticalInboundKind::Intent:
				processed = processIntent(
					message, simulationTick, diagnostics);
				break;
			case FullEngineCoopTacticalInboundKind::BaselineAck:
				processed = processBaselineAck(message, diagnostics);
				break;
			case FullEngineCoopTacticalInboundKind::DeltaAck:
				processed = processDeltaAck(message, diagnostics);
				break;
			case FullEngineCoopTacticalInboundKind::ResyncRequest:
				processed = processResyncRequest(message, diagnostics);
				break;
			default:
				processed = FullEngineCoopTacticalServerResult::InputRejected;
				break;
		}
		if (processed == FullEngineCoopTacticalServerResult::InputRejected)
		{
			++diagnostics.inputsRejected;
			diagnostics.result = FullEngineCoopTacticalServerResult::InputRejected;
			continue;
		}
		if (processed != FullEngineCoopTacticalServerResult::Success)
		{
			diagnostics.result = processed;
			if (processed ==
				FullEngineCoopTacticalServerResult::ExecutionBackpressured)
			{
				if (diagnostics.intentsConsumed == intentsBefore)
				{
					deferredInbound_ = message;
					deferredInboundOccupied_ = true;
				}
				diagnostics.backpressured = true;
			}
			else
			{
				failed_ = true;
				ingress_.clearActorBindings();
			}
			return diagnostics;
		}
	}
	FullEngineCoopTacticalServerPumpResult after = flushOutboundInternal();
	diagnostics.messagesSent += after.messagesSent;
	diagnostics.replicationResult = after.replicationResult;
	diagnostics.resyncRequired =
		diagnostics.resyncRequired || after.resyncRequired;
	if (after.backpressured)
	{
		diagnostics.result = after.result;
		diagnostics.backpressured = true;
	}
	else if (after.result != FullEngineCoopTacticalServerResult::Success)
	{
		diagnostics.result = after.result;
		failed_ = true;
		ingress_.clearActorBindings();
	}
	diagnostics.transportRestartRequired = transportRestartRequired_;
	return diagnostics;
}

bool FullEngineCoopTacticalServer::peerCommandState(
	const PeerIdentity& identity,
	FullEngineCoopTacticalPeerCommandState& state) const noexcept
{
	const PeerRecord* peer = findPeer(identity);
	if (peer == nullptr) return false;
	FullEngineCoopTacticalPeerCommandState captured;
	captured.peerIdentity = peer->identity;
	captured.transport = peer->transport;
	captured.nextExpectedCommandId = peer->nextExpectedCommandId;
	captured.connected = peer->connected;
	captured.exhausted = peer->exhausted;
	captured.pendingCommands = peer->pendingCount;
	state = captured;
	return true;
}

FullEngineCoopTacticalServerDrainState
FullEngineCoopTacticalServer::drainState() const noexcept
{
	FullEngineCoopTacticalServerDrainState state;
	state.active = active_;
	state.worldActive = replication_.worldActive();
	state.transportRunning = listener_.running();
	state.transportRestartRequired = transportRestartRequired_;
	state.authenticatedPeers = listener_.authenticatedPeerCount();
	state.inboundMessages = listener_.pendingInboundCount() +
		(deferredInboundOccupied_ ? 1U : 0U);
	state.transientReceipts = transientReceiptCount_;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		state.pendingCommands += peers_[index].pendingCount;
		CoopTacticalPeerReplicationState replicationState;
		if (!replication_.peerState(peers_[index].identity, replicationState))
			continue;
		if (replicationState.connected)
			state.pendingReplicationReceipts +=
				replicationState.pendingReceipts;
		state.inFlightDeltas += replicationState.inFlightDeltas;
		if (replication_.worldActive() && replicationState.connected &&
			(replicationState.phase != CoopTacticalPeerPhase::Active ||
			 replicationState.lastSentRevision < replication_.revision() ||
			 replicationState.lastAcknowledgedRevision <
				replication_.revision()))
			++state.peersAwaitingReplication;
	}
	return state;
}
}
