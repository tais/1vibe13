#include "FullEngineCoopServerSession.h"

#include <algorithm>
#include <utility>

namespace CoopSession
{
namespace
{
std::uint32_t ReadU32At(
	const std::vector<std::uint8_t>& bytes,
	std::size_t offset) noexcept
{
	if (bytes.size() - std::min(bytes.size(), offset) < 4) return 0;
	return static_cast<std::uint32_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
		(static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
		(static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

bool IdentityLess(
	const PeerIdentity& left,
	const PeerIdentity& right) noexcept
{
	return std::lexicographical_compare(
		left.begin(), left.end(), right.begin(), right.end());
}

bool DeltaTurnEdgeValid(
	const TacticalWorldDelta& delta,
	std::uint64_t previousTurnSerial,
	std::uint64_t currentTurnSerial) noexcept
{
	const TacticalTurnChangedEvent* turn = nullptr;
	for (const TacticalWorldEvent& event : delta.events)
	{
		if (!std::holds_alternative<TacticalTurnChangedEvent>(event)) continue;
		if (turn != nullptr) return false;
		turn = &std::get<TacticalTurnChangedEvent>(event);
	}
	if (!turn) return previousTurnSerial == currentTurnSerial;
	return turn->previous.serial == previousTurnSerial &&
		turn->current.serial == currentTurnSerial;
}

}

const char* CoopTacticalOutboundMessageName(
	CoopTacticalOutboundMessageKind kind) noexcept
{
	switch (kind)
	{
		case CoopTacticalOutboundMessageKind::IntentReceipt:
			return CoopTacticalIntentReceiptMessageName;
		case CoopTacticalOutboundMessageKind::Baseline:
			return CoopTacticalBaselineMessageName;
		case CoopTacticalOutboundMessageKind::Delta:
			return CoopTacticalDeltaMessageName;
	}
	return "";
}

FullEngineCoopServerSession::FullEngineCoopServerSession(
	FullEngineCoopServerSessionConfiguration configuration) noexcept
	: configuration_(configuration)
{
}

bool FullEngineCoopServerSession::configurationValid() const noexcept
{
	return configuration_.maximumPeers != 0 &&
		configuration_.maximumPeers <= MaximumCoopTacticalSessionPeers &&
		configuration_.maximumAssignments != 0 &&
		configuration_.maximumAssignments <= MaximumCoopTacticalAssignments &&
		configuration_.maximumDeltaHistory != 0 &&
		configuration_.maximumDeltaHistory <= MaximumCoopTacticalDeltaHistory &&
		configuration_.maximumReceiptHistoryPerPeer != 0 &&
		configuration_.maximumReceiptHistoryPerPeer <=
			MaximumCoopTacticalReceiptHistoryPerPeer &&
		configuration_.maximumInFlightDeltasPerPeer != 0 &&
		configuration_.maximumInFlightDeltasPerPeer <=
			configuration_.maximumDeltaHistory &&
		configuration_.maximumMessagesPerFlush != 0 &&
		configuration_.maximumMessagesPerFlush <=
			MaximumCoopTacticalMessagesPerFlush &&
		configuration_.maximumBaselineId != 0 &&
		configuration_.maximumDeltaId != 0;
}

FullEngineCoopServerSessionResult
FullEngineCoopServerSession::beginSession(
	std::uint64_t sessionEpoch) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	endSession();
	if (!configurationValid())
		return FullEngineCoopServerSessionResult::InvalidConfiguration;
	if (sessionEpoch == 0)
		return FullEngineCoopServerSessionResult::InvalidContext;
	sessionEpoch_ = sessionEpoch;
	nextBaselineId_ = 1;
	nextDeltaId_ = 1;
	active_ = true;
	return FullEngineCoopServerSessionResult::Success;
}

void FullEngineCoopServerSession::endSession() noexcept
{
	if (flushing_) return;
	active_ = false;
	worldActive_ = false;
	sessionEpoch_ = 0;
	worldGeneration_ = 0;
	revision_ = 0;
	turnSerial_ = 0;
	nextBaselineId_ = 1;
	nextDeltaId_ = 1;
	peers_ = {};
	peerCount_ = 0;
	assignments_ = {};
	assignmentCount_ = 0;
	deltas_ = {};
	deltaHead_ = 0;
	deltaCount_ = 0;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::beginWorld(
	std::uint64_t worldGeneration,
	std::uint64_t revision,
	std::uint64_t turnSerial) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (worldGeneration == 0 || revision == 0 || turnSerial == 0)
		return FullEngineCoopServerSessionResult::InvalidContext;
	if (worldActive_ && worldGeneration <= worldGeneration_)
		return FullEngineCoopServerSessionResult::StaleContext;

	worldGeneration_ = worldGeneration;
	revision_ = revision;
	turnSerial_ = turnSerial;
	worldActive_ = true;
	assignments_ = {};
	assignmentCount_ = 0;
	deltas_ = {};
	deltaHead_ = 0;
	deltaCount_ = 0;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		PeerRecord& peer = peers_[index];
		peer.receipts = {};
		peer.receiptHead = 0;
		peer.receiptCount = 0;
		peer.hasResyncRequest = false;
		peer.committedCheckpointValid = false;
		peer.committedOrdinal = 0;
		peer.nextSentCheckpointOrdinal = 1;
		peer.sentCheckpointHead = 0;
		peer.sentCheckpointCount = 0;
		resetPeerReplication(peer, peer.connected
			? CoopTacticalPeerPhase::NeedsBaseline
			: CoopTacticalPeerPhase::Offline);
	}
	return FullEngineCoopServerSessionResult::Success;
}

void FullEngineCoopServerSession::endWorld() noexcept
{
	if (flushing_) return;
	worldActive_ = false;
	worldGeneration_ = 0;
	revision_ = 0;
	turnSerial_ = 0;
	assignments_ = {};
	assignmentCount_ = 0;
	deltas_ = {};
	deltaHead_ = 0;
	deltaCount_ = 0;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		PeerRecord& peer = peers_[index];
		peer.receipts = {};
		peer.receiptHead = 0;
		peer.receiptCount = 0;
		peer.hasResyncRequest = false;
		peer.committedCheckpointValid = false;
		peer.committedOrdinal = 0;
		peer.nextSentCheckpointOrdinal = 1;
		peer.sentCheckpointHead = 0;
		peer.sentCheckpointCount = 0;
		resetPeerReplication(peer, peer.connected
			? CoopTacticalPeerPhase::NeedsBaseline
			: CoopTacticalPeerPhase::Offline);
	}
}

FullEngineCoopServerSession::PeerRecord*
FullEngineCoopServerSession::findPeer(const PeerIdentity& peer) noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == peer) return &peers_[index];
	return nullptr;
}

const FullEngineCoopServerSession::PeerRecord*
FullEngineCoopServerSession::findPeer(const PeerIdentity& peer) const noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == peer) return &peers_[index];
	return nullptr;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::connectPeer(
	const PeerIdentity& identity) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (IsZero(identity)) return FullEngineCoopServerSessionResult::InvalidPeer;
	if (PeerRecord* existing = findPeer(identity))
	{
		if (existing->connected)
			return FullEngineCoopServerSessionResult::Success;
		existing->connected = true;
		existing->committedCheckpointValid = false;
		existing->committedOrdinal = 0;
		existing->nextSentCheckpointOrdinal = 1;
		existing->hasResyncRequest = false;
		existing->sentCheckpointHead = 0;
		existing->sentCheckpointCount = 0;
		resetPeerReplication(*existing,
			CoopTacticalPeerPhase::NeedsBaseline);
		return FullEngineCoopServerSessionResult::Success;
	}
	if (peerCount_ >= configuration_.maximumPeers)
		return FullEngineCoopServerSessionResult::PeerCapacityReached;

	std::size_t insertion = 0;
	while (insertion < peerCount_ &&
		IdentityLess(peers_[insertion].identity, identity))
		++insertion;
	for (std::size_t index = peerCount_; index > insertion; --index)
		peers_[index] = std::move(peers_[index - 1]);
	peers_[insertion] = PeerRecord{};
	peers_[insertion].identity = identity;
	peers_[insertion].connected = true;
	peers_[insertion].phase = CoopTacticalPeerPhase::NeedsBaseline;
	++peerCount_;
	return FullEngineCoopServerSessionResult::Success;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::disconnectPeer(
	const PeerIdentity& identity) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	PeerRecord* peer = findPeer(identity);
	if (!peer) return FullEngineCoopServerSessionResult::InvalidPeer;
	peer->connected = false;
	peer->committedCheckpointValid = false;
	peer->committedOrdinal = 0;
	peer->nextSentCheckpointOrdinal = 1;
	peer->hasResyncRequest = false;
	peer->sentCheckpointHead = 0;
	peer->sentCheckpointCount = 0;
	resetPeerReplication(*peer, CoopTacticalPeerPhase::Offline);
	return FullEngineCoopServerSessionResult::Success;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::retirePeer(
	const PeerIdentity& identity) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;

	std::size_t peerIndex = 0;
	while (peerIndex < peerCount_ && peers_[peerIndex].identity != identity)
		++peerIndex;
	if (peerIndex == peerCount_)
		return FullEngineCoopServerSessionResult::InvalidPeer;
	const PeerRecord& retiring = peers_[peerIndex];
	if (retiring.connected || retiring.phase != CoopTacticalPeerPhase::Offline ||
		!retiring.baselineBytes.empty() || retiring.inFlightDeltas != 0)
		return FullEngineCoopServerSessionResult::InvalidContext;

	// Assignment order is by actor, so stable compaction preserves the ordering
	// contract without perturbing any surviving assignment.
	std::size_t assignmentOutput = 0;
	for (std::size_t index = 0; index < assignmentCount_; ++index)
	{
		if (assignments_[index].peerIdentity == identity) continue;
		if (assignmentOutput != index)
			assignments_[assignmentOutput] = assignments_[index];
		++assignmentOutput;
	}
	for (std::size_t index = assignmentOutput; index < assignmentCount_; ++index)
		assignments_[index] = CoopTacticalActorAssignment{};
	assignmentCount_ = assignmentOutput;

	for (std::size_t index = peerIndex + 1; index < peerCount_; ++index)
		peers_[index - 1] = std::move(peers_[index]);
	peers_[--peerCount_] = PeerRecord{};
	return FullEngineCoopServerSessionResult::Success;
}

std::size_t FullEngineCoopServerSession::connectedPeerCount() const noexcept
{
	std::size_t count = 0;
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].connected) ++count;
	return count;
}

CoopTacticalPeerPhase FullEngineCoopServerSession::peerPhase(
	const PeerIdentity& identity) const noexcept
{
	const PeerRecord* peer = findPeer(identity);
	return peer ? peer->phase : CoopTacticalPeerPhase::Vacant;
}

bool FullEngineCoopServerSession::peerState(
	const PeerIdentity& identity,
	CoopTacticalPeerReplicationState& output) const noexcept
{
	const PeerRecord* peer = findPeer(identity);
	if (!peer) return false;
	CoopTacticalPeerReplicationState captured;
	captured.peerIdentity = peer->identity;
	captured.phase = peer->phase;
	captured.connected = peer->connected;
	captured.baselineId = peer->baselineId;
	captured.baselineRevision = peer->baselineRevision;
	captured.baselineNextExpectedCommandId =
		peer->baselineNextExpectedCommandId;
	captured.nextDeltaToSend = peer->nextDeltaToSend;
	captured.nextDeltaToAcknowledge = peer->nextDeltaToAcknowledge;
	captured.lastSentRevision = peer->lastSentRevision;
	captured.lastAcknowledgedDeltaId = peer->lastAcknowledgedDeltaId;
	captured.lastAcknowledgedRevision = peer->lastAcknowledgedRevision;
	captured.inFlightDeltas = peer->inFlightDeltas;
	captured.retainedReceipts = peer->receiptCount;
	for (std::size_t index = 0; index < peer->receiptCount; ++index)
		if (receiptAt(*peer, index).pending) ++captured.pendingReceipts;
	output = captured;
	return true;
}

FullEngineCoopServerSessionResult
FullEngineCoopServerSession::replaceAssignments(
	const CoopTacticalActorAssignment* assignments,
	std::size_t count) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (!worldActive_) return FullEngineCoopServerSessionResult::NoWorld;
	if (count > configuration_.maximumAssignments)
		return FullEngineCoopServerSessionResult::AssignmentCapacityReached;
	if (count != 0 && assignments == nullptr)
		return FullEngineCoopServerSessionResult::InvalidAssignment;

	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalAssignments> accepted{};
	for (std::size_t index = 0; index < count; ++index)
	{
		const CoopTacticalActorAssignment& candidate = assignments[index];
		if (!candidate.actor.valid() || IsZero(candidate.peerIdentity) ||
			findPeer(candidate.peerIdentity) == nullptr ||
			(index != 0 &&
				!(assignments[index - 1].actor < candidate.actor)))
			return FullEngineCoopServerSessionResult::InvalidAssignment;
		accepted[index] = candidate;
	}
	bool changed = count != assignmentCount_;
	for (std::size_t index = 0; !changed && index < count; ++index)
		changed = assignments_[index].actor != accepted[index].actor ||
			assignments_[index].peerIdentity != accepted[index].peerIdentity;
	assignments_ = accepted;
	assignmentCount_ = count;
	if (changed)
	{
		for (std::size_t index = 0; index < peerCount_; ++index)
			if (peers_[index].connected)
				resetPeerReplication(peers_[index],
					CoopTacticalPeerPhase::NeedsBaseline);
	}
	return FullEngineCoopServerSessionResult::Success;
}

const CoopTacticalActorAssignment* FullEngineCoopServerSession::assignment(
	std::size_t index) const noexcept
{
	return index < assignmentCount_ ? &assignments_[index] : nullptr;
}

void FullEngineCoopServerSession::recordSentCheckpoint(
	PeerRecord& peer, const SentCheckpoint& checkpoint) noexcept
{
	for (std::size_t index = 0; index < peer.sentCheckpointCount; ++index)
	{
		SentCheckpoint& existing = peer.sentCheckpoints[
			(peer.sentCheckpointHead + index) % peer.sentCheckpoints.size()];
		if (existing.baseline == checkpoint.baseline &&
			(existing.baseline ? existing.baselineId == checkpoint.baselineId
			 : existing.deltaId == checkpoint.deltaId))
		{
			return;
		}
	}
	if (peer.sentCheckpointCount == peer.sentCheckpoints.size())
	{
		peer.sentCheckpointHead =
			(peer.sentCheckpointHead + 1) % peer.sentCheckpoints.size();
		--peer.sentCheckpointCount;
	}
	SentCheckpoint recorded = checkpoint;
	recorded.sendOrdinal = peer.nextSentCheckpointOrdinal++;
	peer.sentCheckpoints[(peer.sentCheckpointHead + peer.sentCheckpointCount) %
		peer.sentCheckpoints.size()] = recorded;
	++peer.sentCheckpointCount;
}

const FullEngineCoopServerSession::SentCheckpoint*
FullEngineCoopServerSession::findSentBaseline(
	const PeerRecord& peer, std::uint64_t baselineId) const noexcept
{
	for (std::size_t index = 0; index < peer.sentCheckpointCount; ++index)
	{
		const SentCheckpoint& checkpoint = peer.sentCheckpoints[
			(peer.sentCheckpointHead + index) % peer.sentCheckpoints.size()];
		if (checkpoint.baseline && checkpoint.baselineId == baselineId)
			return &checkpoint;
	}
	return nullptr;
}

const FullEngineCoopServerSession::SentCheckpoint*
FullEngineCoopServerSession::findSentDelta(
	const PeerRecord& peer, std::uint64_t deltaId) const noexcept
{
	for (std::size_t index = 0; index < peer.sentCheckpointCount; ++index)
	{
		const SentCheckpoint& checkpoint = peer.sentCheckpoints[
			(peer.sentCheckpointHead + index) % peer.sentCheckpoints.size()];
		if (!checkpoint.baseline && checkpoint.deltaId == deltaId)
			return &checkpoint;
	}
	return nullptr;
}

void FullEngineCoopServerSession::resetPeerReplication(
	PeerRecord& peer,
	CoopTacticalPeerPhase phase) noexcept
{
	peer.phase = phase;
	peer.baselineId = 0;
	peer.baselineRevision = 0;
	peer.baselineTurnSerial = 0;
	peer.baselineNextExpectedCommandId = 1;
	peer.baselineDeltaFloor = 0;
	peer.baselineChecksum = 0;
	peer.baselineSent = false;
	peer.baselineBytes.clear();
	peer.nextDeltaToSend = 0;
	peer.nextDeltaToAcknowledge = 0;
	peer.lastSentRevision = 0;
	peer.lastAcknowledgedDeltaId = 0;
	peer.lastAcknowledgedRevision = 0;
	peer.lastAcknowledgedTurnSerial = 0;
	peer.lastAcknowledgedChecksum = 0;
	peer.inFlightDeltas = 0;
}

void FullEngineCoopServerSession::requireResync(PeerRecord& peer) noexcept
{
	resetPeerReplication(peer, peer.connected
		? CoopTacticalPeerPhase::ResyncRequired
		: CoopTacticalPeerPhase::Offline);
}

std::uint64_t FullEngineCoopServerSession::nextBaselineId(
	std::uint64_t value) const noexcept
{
	return value == configuration_.maximumBaselineId ? 0 : value + 1;
}

std::uint64_t FullEngineCoopServerSession::nextDeltaId(
	std::uint64_t value) const noexcept
{
	return value == configuration_.maximumDeltaId ? 0 : value + 1;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::stageBaseline(
	const PeerIdentity& identity,
	const TacticalWorldSnapshot& snapshot,
	std::uint64_t nextExpectedCommandId) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (!worldActive_) return FullEngineCoopServerSessionResult::NoWorld;
	PeerRecord* peer = findPeer(identity);
	if (!peer || !peer->connected)
		return FullEngineCoopServerSessionResult::InvalidPeer;
	if (nextBaselineId_ == 0)
		return FullEngineCoopServerSessionResult::SequenceExhausted;
	if (peer->phase == CoopTacticalPeerPhase::AwaitingBaselineAck)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	if (snapshot.epoch() != worldGeneration_ ||
		snapshot.turn().serial != turnSerial_)
		return FullEngineCoopServerSessionResult::InvalidContext;

	CoopTacticalBaseline baseline;
	baseline.state.sessionEpoch = sessionEpoch_;
	baseline.state.worldGeneration = worldGeneration_;
	baseline.state.revision = revision_;
	baseline.state.turnSerial = turnSerial_;
	baseline.baselineId = nextBaselineId_;
	baseline.nextExpectedCommandId = nextExpectedCommandId;
	try
	{
		baseline.assignedActors.reserve(assignmentCount_);
		for (std::size_t index = 0; index < assignmentCount_; ++index)
			if (assignments_[index].peerIdentity == identity)
				baseline.assignedActors.push_back(assignments_[index].actor);
	}
	catch (...)
	{
		return FullEngineCoopServerSessionResult::AllocationFailure;
	}
	if (!snapshot.copyTo(baseline.snapshot))
		return FullEngineCoopServerSessionResult::AllocationFailure;
	std::vector<std::uint8_t> encoded;
	const CoopTacticalCodecResult encodedResult =
		EncodeCoopTacticalBaseline(baseline, encoded);
	if (encodedResult == CoopTacticalCodecResult::AllocationFailure)
		return FullEngineCoopServerSessionResult::AllocationFailure;
	if (encodedResult != CoopTacticalCodecResult::Success)
		return FullEngineCoopServerSessionResult::CodecFailure;

	peer->phase = CoopTacticalPeerPhase::AwaitingBaselineAck;
	peer->baselineId = nextBaselineId_;
	peer->baselineRevision = revision_;
	peer->baselineTurnSerial = turnSerial_;
	peer->baselineNextExpectedCommandId = nextExpectedCommandId;
	peer->baselineDeltaFloor = nextDeltaId_;
	peer->baselineChecksum = ReadU32At(encoded, 64);
	peer->baselineSent = false;
	peer->baselineBytes = std::move(encoded);
	peer->nextDeltaToSend = 0;
	peer->nextDeltaToAcknowledge = 0;
	peer->inFlightDeltas = 0;
	nextBaselineId_ = nextBaselineId(nextBaselineId_);
	return FullEngineCoopServerSessionResult::Success;
}

bool FullEngineCoopServerSession::sameState(
	const CoopTacticalStateIdentity& left,
	const CoopTacticalStateIdentity& right) const noexcept
{
	return left.wireVersion == right.wireVersion &&
		left.protocolVersion == right.protocolVersion &&
		left.sessionEpoch == right.sessionEpoch &&
		left.worldGeneration == right.worldGeneration &&
		left.revision == right.revision &&
		left.turnSerial == right.turnSerial;
}

bool FullEngineCoopServerSession::baselineCanCatchUp(
	const PeerRecord& peer) const noexcept
{
	if (peer.baselineRevision == revision_) return true;
	if (peer.baselineDeltaFloor == 0) return false;
	std::uint64_t requiredId = peer.baselineDeltaFloor;
	std::uint64_t expectedRevision = peer.baselineRevision;
	for (std::size_t index = 0; index < deltaCount_; ++index)
	{
		const DeltaRecord& delta = deltaAt(index);
		if (delta.id < requiredId) continue;
		if (delta.id != requiredId || delta.baseRevision != expectedRevision)
			return false;
		expectedRevision = delta.revision;
		if (expectedRevision == revision_) return true;
		requiredId = nextDeltaId(requiredId);
		if (requiredId == 0) return false;
	}
	return false;
}

FullEngineCoopServerSessionResult
FullEngineCoopServerSession::acknowledgeBaseline(
	const PeerIdentity& identity,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	PeerRecord* peer = findPeer(identity);
	if (!peer || !peer->connected)
		return FullEngineCoopServerSessionResult::InvalidPeer;
	CoopTacticalBaselineAck acknowledgement;
	const CoopTacticalCodecResult decoded = DecodeCoopTacticalBaselineAck(
		bytes, size, acknowledgement);
	if (decoded != CoopTacticalCodecResult::Success)
		return decoded == CoopTacticalCodecResult::ChecksumMismatch
			? FullEngineCoopServerSessionResult::IntegrityMismatch
			: FullEngineCoopServerSessionResult::CodecFailure;
	const bool currentBaseline =
		peer->phase == CoopTacticalPeerPhase::AwaitingBaselineAck &&
		peer->baselineSent && acknowledgement.baselineId == peer->baselineId;
	const SentCheckpoint* sent = currentBaseline ? nullptr :
		findSentBaseline(*peer, acknowledgement.baselineId);
	if (!currentBaseline && sent == nullptr)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	CoopTacticalStateIdentity expected;
	expected.sessionEpoch = sessionEpoch_;
	expected.worldGeneration = worldGeneration_;
	expected.revision = currentBaseline ? peer->baselineRevision : sent->revision;
	expected.turnSerial = currentBaseline ? peer->baselineTurnSerial : sent->turnSerial;
	if (!sameState(acknowledgement.state, expected) ||
		acknowledgement.peerIdentity != identity ||
		acknowledgement.nextExpectedCommandId !=
			(currentBaseline ? peer->baselineNextExpectedCommandId :
			 sent->nextExpectedCommandId))
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	const std::uint32_t expectedChecksum = currentBaseline ?
		peer->baselineChecksum : sent->checksum;
	if (acknowledgement.payloadChecksum != expectedChecksum)
		return FullEngineCoopServerSessionResult::IntegrityMismatch;
	if (!currentBaseline)
	{
		if (peer->committedCheckpointValid &&
			sent->sendOrdinal <= peer->committedOrdinal)
			return FullEngineCoopServerSessionResult::Success;
		peer->committedCheckpointValid = true;
		peer->committedBaselineId = acknowledgement.baselineId;
		peer->committedRevision = sent->revision;
		peer->committedTurnSerial = sent->turnSerial;
		peer->committedDeltaId = 0;
		peer->committedChecksum = sent->checksum;
		peer->committedOrdinal = sent->sendOrdinal;
		return FullEngineCoopServerSessionResult::Success;
	}

	peer->baselineBytes.clear();
	peer->baselineSent = false;
	peer->nextDeltaToSend = peer->baselineDeltaFloor;
	peer->nextDeltaToAcknowledge = peer->baselineDeltaFloor;
	peer->inFlightDeltas = 0;
	peer->lastSentRevision = peer->baselineRevision;
	peer->lastAcknowledgedRevision = peer->baselineRevision;
	peer->lastAcknowledgedTurnSerial = peer->baselineTurnSerial;
	peer->lastAcknowledgedChecksum = peer->baselineChecksum;
	peer->committedCheckpointValid = true;
	peer->committedBaselineId = peer->baselineId;
	peer->committedRevision = peer->baselineRevision;
	peer->committedTurnSerial = peer->baselineTurnSerial;
	peer->committedDeltaId = 0;
	peer->committedChecksum = peer->baselineChecksum;
	if (const SentCheckpoint* sent = findSentBaseline(*peer, peer->baselineId))
		peer->committedOrdinal = sent->sendOrdinal;
	peer->phase = CoopTacticalPeerPhase::Active;
	if (!baselineCanCatchUp(*peer))
	{
		requireResync(*peer);
		return FullEngineCoopServerSessionResult::ResyncRequired;
	}
	return FullEngineCoopServerSessionResult::Success;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::requestResync(
	const PeerIdentity& identity, const std::uint8_t* bytes,
	std::size_t size, bool* replicationReset) noexcept
{
	if (replicationReset) *replicationReset = false;
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (!worldActive_) return FullEngineCoopServerSessionResult::NoWorld;
	PeerRecord* peer = findPeer(identity);
	if (!peer || !peer->connected)
		return FullEngineCoopServerSessionResult::InvalidPeer;
	CoopTacticalResyncRequest request;
	if (DecodeCoopTacticalResyncRequest(bytes, size, request) !=
		CoopTacticalCodecResult::Success)
		return FullEngineCoopServerSessionResult::CodecFailure;
	if (peer->hasResyncRequest &&
		request.requestId == peer->lastResyncRequestId)
	{
		return std::equal(bytes, bytes + size,
			peer->lastResyncRequestBytes.begin())
			? FullEngineCoopServerSessionResult::Success
			: FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	}
	if (peer->hasResyncRequest &&
		request.requestId < peer->lastResyncRequestId)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;

	if (!peer->committedCheckpointValid)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	CoopTacticalStateIdentity expected;
	expected.sessionEpoch = sessionEpoch_;
	expected.worldGeneration = worldGeneration_;
	expected.revision = peer->committedRevision;
	expected.turnSerial = peer->committedTurnSerial;
	if (peer->phase != CoopTacticalPeerPhase::Active &&
		peer->phase != CoopTacticalPeerPhase::NeedsBaseline &&
		peer->phase != CoopTacticalPeerPhase::AwaitingBaselineAck &&
		peer->phase != CoopTacticalPeerPhase::ResyncRequired)
	{
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	}
	if (!sameState(request.acceptedState, expected) ||
		request.acceptedBaselineId != peer->committedBaselineId ||
		request.lastAppliedDeltaId != peer->committedDeltaId ||
		request.lastPayloadChecksum != peer->committedChecksum)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;

	peer->hasResyncRequest = true;
	peer->lastResyncRequestId = request.requestId;
	std::copy(bytes, bytes + size, peer->lastResyncRequestBytes.begin());
	peer->sentCheckpoints = {};
	peer->sentCheckpointHead = 0;
	peer->sentCheckpointCount = 0;
	requireResync(*peer);
	if (replicationReset) *replicationReset = true;
	return FullEngineCoopServerSessionResult::Success;
}

FullEngineCoopServerSession::DeltaRecord&
FullEngineCoopServerSession::deltaAt(std::size_t offset) noexcept
{
	return deltas_[(deltaHead_ + offset) % deltas_.size()];
}

const FullEngineCoopServerSession::DeltaRecord&
FullEngineCoopServerSession::deltaAt(std::size_t offset) const noexcept
{
	return deltas_[(deltaHead_ + offset) % deltas_.size()];
}

FullEngineCoopServerSession::DeltaRecord*
FullEngineCoopServerSession::findDelta(std::uint64_t id) noexcept
{
	for (std::size_t index = 0; index < deltaCount_; ++index)
		if (deltaAt(index).id == id) return &deltaAt(index);
	return nullptr;
}

const FullEngineCoopServerSession::DeltaRecord*
FullEngineCoopServerSession::findDelta(std::uint64_t id) const noexcept
{
	for (std::size_t index = 0; index < deltaCount_; ++index)
		if (deltaAt(index).id == id) return &deltaAt(index);
	return nullptr;
}

void FullEngineCoopServerSession::evictOldestDelta() noexcept
{
	if (deltaCount_ == 0) return;
	const std::uint64_t evictedId = deltaAt(0).id;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		PeerRecord& peer = peers_[index];
		if (!peer.connected) continue;
		if (peer.phase == CoopTacticalPeerPhase::AwaitingBaselineAck &&
			peer.baselineDeltaFloor != 0 &&
			peer.baselineDeltaFloor <= evictedId)
			requireResync(peer);
		else if (peer.phase == CoopTacticalPeerPhase::Active &&
			peer.nextDeltaToAcknowledge != 0 &&
			peer.nextDeltaToAcknowledge <= evictedId)
			requireResync(peer);
	}
	deltaAt(0) = DeltaRecord{};
	deltaHead_ = (deltaHead_ + 1) % deltas_.size();
	--deltaCount_;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::publishDelta(
	const TacticalWorldDelta& delta,
	std::uint64_t resultingRevision,
	std::uint64_t resultingTurnSerial) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (!worldActive_) return FullEngineCoopServerSessionResult::NoWorld;
	if (nextDeltaId_ == 0)
		return FullEngineCoopServerSessionResult::SequenceExhausted;
	if (resultingRevision <= revision_ ||
		resultingTurnSerial < turnSerial_ || resultingTurnSerial == 0 ||
		delta.previousEpoch != worldGeneration_ ||
		delta.currentEpoch != worldGeneration_ ||
		!DeltaTurnEdgeValid(delta, turnSerial_, resultingTurnSerial))
		return FullEngineCoopServerSessionResult::InvalidContext;

	CoopTacticalDelta envelope;
	envelope.state.sessionEpoch = sessionEpoch_;
	envelope.state.worldGeneration = worldGeneration_;
	envelope.state.revision = resultingRevision;
	envelope.state.turnSerial = resultingTurnSerial;
	envelope.deltaId = nextDeltaId_;
	envelope.baseRevision = revision_;
	try
	{
		envelope.delta = delta;
	}
	catch (...)
	{
		return FullEngineCoopServerSessionResult::AllocationFailure;
	}
	std::vector<std::uint8_t> encoded;
	const CoopTacticalCodecResult encodedResult =
		EncodeCoopTacticalDelta(envelope, encoded);
	if (encodedResult == CoopTacticalCodecResult::AllocationFailure)
		return FullEngineCoopServerSessionResult::AllocationFailure;
	if (encodedResult != CoopTacticalCodecResult::Success)
		return FullEngineCoopServerSessionResult::CodecFailure;

	DeltaRecord accepted;
	accepted.id = nextDeltaId_;
	accepted.baseRevision = revision_;
	accepted.revision = resultingRevision;
	accepted.turnSerial = resultingTurnSerial;
	accepted.checksum = ReadU32At(encoded, 68);
	accepted.bytes = std::move(encoded);
	if (deltaCount_ == configuration_.maximumDeltaHistory)
		evictOldestDelta();
	deltaAt(deltaCount_) = std::move(accepted);
	++deltaCount_;
	revision_ = resultingRevision;
	turnSerial_ = resultingTurnSerial;
	nextDeltaId_ = nextDeltaId(nextDeltaId_);
	return FullEngineCoopServerSessionResult::Success;
}

FullEngineCoopServerSessionResult
FullEngineCoopServerSession::acknowledgeDelta(
	const PeerIdentity& identity,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	PeerRecord* peer = findPeer(identity);
	if (!peer || !peer->connected)
		return FullEngineCoopServerSessionResult::InvalidPeer;
	CoopTacticalDeltaAck acknowledgement;
	const CoopTacticalCodecResult decoded = DecodeCoopTacticalDeltaAck(
		bytes, size, acknowledgement);
	if (decoded != CoopTacticalCodecResult::Success)
		return FullEngineCoopServerSessionResult::CodecFailure;
	if (acknowledgement.peerIdentity != identity)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	if (acknowledgement.deltaId == peer->lastAcknowledgedDeltaId &&
		acknowledgement.payloadChecksum == peer->lastAcknowledgedChecksum &&
		acknowledgement.state.sessionEpoch == sessionEpoch_ &&
		acknowledgement.state.worldGeneration == worldGeneration_ &&
		acknowledgement.state.revision == peer->lastAcknowledgedRevision &&
		acknowledgement.state.turnSerial ==
			peer->lastAcknowledgedTurnSerial)
		return FullEngineCoopServerSessionResult::Success;
	const SentCheckpoint* sentCheckpoint =
		findSentDelta(*peer, acknowledgement.deltaId);
	if (sentCheckpoint != nullptr && peer->committedCheckpointValid &&
		sentCheckpoint->sendOrdinal <= peer->committedOrdinal)
	{
		CoopTacticalStateIdentity sentState;
		sentState.sessionEpoch = sessionEpoch_;
		sentState.worldGeneration = worldGeneration_;
		sentState.revision = sentCheckpoint->revision;
		sentState.turnSerial = sentCheckpoint->turnSerial;
		if (!sameState(acknowledgement.state, sentState))
			return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
		return acknowledgement.payloadChecksum == sentCheckpoint->checksum
			? FullEngineCoopServerSessionResult::Success
			: FullEngineCoopServerSessionResult::IntegrityMismatch;
	}
	if (peer->phase != CoopTacticalPeerPhase::Active ||
		peer->inFlightDeltas == 0 ||
		findDelta(acknowledgement.deltaId) == nullptr)
	{
		CoopTacticalStateIdentity expected;
		expected.sessionEpoch = sessionEpoch_;
		expected.worldGeneration = worldGeneration_;
		if (sentCheckpoint == nullptr)
			return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
		expected.revision = sentCheckpoint->revision;
		expected.turnSerial = sentCheckpoint->turnSerial;
		if (!sameState(acknowledgement.state, expected))
			return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
		if (acknowledgement.payloadChecksum != sentCheckpoint->checksum)
			return FullEngineCoopServerSessionResult::IntegrityMismatch;
		if (peer->committedCheckpointValid &&
			sentCheckpoint->sendOrdinal <= peer->committedOrdinal)
			return FullEngineCoopServerSessionResult::Success;
		peer->committedCheckpointValid = true;
		peer->committedBaselineId = sentCheckpoint->baselineId;
		peer->committedRevision = sentCheckpoint->revision;
		peer->committedTurnSerial = sentCheckpoint->turnSerial;
		peer->committedDeltaId = sentCheckpoint->deltaId;
		peer->committedChecksum = sentCheckpoint->checksum;
		peer->committedOrdinal = sentCheckpoint->sendOrdinal;
		return FullEngineCoopServerSessionResult::Success;
	}
	if (peer->inFlightDeltas == 0)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;

	std::uint64_t requiredId = peer->nextDeltaToAcknowledge;
	const DeltaRecord* acknowledgedDelta = nullptr;
	std::size_t acknowledgedCount = 0;
	while (acknowledgedCount < peer->inFlightDeltas)
	{
		const DeltaRecord* candidate = findDelta(requiredId);
		if (!candidate)
		{
			requireResync(*peer);
			return FullEngineCoopServerSessionResult::ResyncRequired;
		}
		++acknowledgedCount;
		if (candidate->id == acknowledgement.deltaId)
		{
			acknowledgedDelta = candidate;
			break;
		}
		requiredId = nextDeltaId(candidate->id);
		if (requiredId == 0) break;
	}
	if (!acknowledgedDelta)
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	CoopTacticalStateIdentity expected;
	expected.sessionEpoch = sessionEpoch_;
	expected.worldGeneration = worldGeneration_;
	expected.revision = acknowledgedDelta->revision;
	expected.turnSerial = acknowledgedDelta->turnSerial;
	if (!sameState(acknowledgement.state, expected))
		return FullEngineCoopServerSessionResult::UnexpectedAcknowledgement;
	if (acknowledgement.payloadChecksum != acknowledgedDelta->checksum)
		return FullEngineCoopServerSessionResult::IntegrityMismatch;

	peer->nextDeltaToAcknowledge = nextDeltaId(acknowledgedDelta->id);
	peer->inFlightDeltas -= acknowledgedCount;
	peer->lastAcknowledgedDeltaId = acknowledgedDelta->id;
	peer->lastAcknowledgedRevision = acknowledgedDelta->revision;
	peer->lastAcknowledgedTurnSerial = acknowledgedDelta->turnSerial;
	peer->lastAcknowledgedChecksum = acknowledgedDelta->checksum;
	peer->committedRevision = acknowledgedDelta->revision;
	peer->committedTurnSerial = acknowledgedDelta->turnSerial;
	peer->committedDeltaId = acknowledgedDelta->id;
	peer->committedChecksum = acknowledgedDelta->checksum;
	if (sentCheckpoint != nullptr)
		peer->committedOrdinal = sentCheckpoint->sendOrdinal;
	return FullEngineCoopServerSessionResult::Success;
}

FullEngineCoopServerSession::ReceiptRecord&
FullEngineCoopServerSession::receiptAt(
	PeerRecord& peer,
	std::size_t offset) noexcept
{
	return peer.receipts[(peer.receiptHead + offset) % peer.receipts.size()];
}

const FullEngineCoopServerSession::ReceiptRecord&
FullEngineCoopServerSession::receiptAt(
	const PeerRecord& peer,
	std::size_t offset) const noexcept
{
	return peer.receipts[(peer.receiptHead + offset) % peer.receipts.size()];
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::recordReceipt(
	const CoopTacticalIntentReceipt& receipt) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (!worldActive_) return FullEngineCoopServerSessionResult::NoWorld;
	PeerRecord* peer = findPeer(receipt.peerIdentity);
	if (!peer) return FullEngineCoopServerSessionResult::InvalidPeer;
	if (receipt.state.sessionEpoch != sessionEpoch_ ||
		receipt.state.worldGeneration != worldGeneration_ ||
		receipt.state.revision > revision_ ||
		receipt.state.turnSerial > turnSerial_)
		return FullEngineCoopServerSessionResult::InvalidContext;

	CoopTacticalIntentReceiptBytes encoded{};
	const CoopTacticalCodecResult encodedResult =
		EncodeCoopTacticalIntentReceipt(receipt, encoded);
	if (encodedResult != CoopTacticalCodecResult::Success)
		return FullEngineCoopServerSessionResult::CodecFailure;
	for (std::size_t index = 0; index < peer->receiptCount; ++index)
	{
		ReceiptRecord& existing = receiptAt(*peer, index);
		if (existing.commandId != receipt.commandId) continue;
		if (existing.bytes == encoded)
			return FullEngineCoopServerSessionResult::Success;
		if (existing.status != CoopTacticalIntentReceiptStatus::Queued ||
			receipt.status == CoopTacticalIntentReceiptStatus::Queued)
			return FullEngineCoopServerSessionResult::ConflictingReceipt;
		existing.revision = receipt.state.revision;
		existing.status = receipt.status;
		existing.bytes = encoded;
		existing.pending = true;
		return FullEngineCoopServerSessionResult::Success;
	}

	std::size_t insertion = peer->receiptCount;
	if (peer->receiptCount ==
		configuration_.maximumReceiptHistoryPerPeer)
	{
		ReceiptRecord& oldest = receiptAt(*peer, 0);
		if (oldest.pending)
			return FullEngineCoopServerSessionResult::ReceiptCapacityReached;
		oldest = ReceiptRecord{};
		peer->receiptHead = (peer->receiptHead + 1) % peer->receipts.size();
		--peer->receiptCount;
		insertion = peer->receiptCount;
	}
	ReceiptRecord& accepted = receiptAt(*peer, insertion);
	accepted.commandId = receipt.commandId;
	accepted.revision = receipt.state.revision;
	accepted.status = receipt.status;
	accepted.bytes = encoded;
	accepted.pending = true;
	++peer->receiptCount;
	return FullEngineCoopServerSessionResult::Success;
}

FullEngineCoopServerSessionResult FullEngineCoopServerSession::replayReceipt(
	const PeerIdentity& identity,
	std::uint64_t commandId) noexcept
{
	if (flushing_) return FullEngineCoopServerSessionResult::Busy;
	if (!active_) return FullEngineCoopServerSessionResult::NotActive;
	if (commandId == 0)
		return FullEngineCoopServerSessionResult::NotFound;
	PeerRecord* peer = findPeer(identity);
	if (!peer) return FullEngineCoopServerSessionResult::InvalidPeer;
	for (std::size_t index = 0; index < peer->receiptCount; ++index)
	{
		ReceiptRecord& receipt = receiptAt(*peer, index);
		if (receipt.commandId != commandId) continue;
		receipt.pending = true;
		return FullEngineCoopServerSessionResult::Success;
	}
	return FullEngineCoopServerSessionResult::NotFound;
}

bool FullEngineCoopServerSession::hasRetainedReceipt(
	const PeerIdentity& identity, std::uint64_t commandId) const noexcept
{
	const PeerRecord* peer = findPeer(identity);
	if (!peer) return false;
	for (std::size_t index = 0; index < peer->receiptCount; ++index)
		if (receiptAt(*peer, index).commandId == commandId) return true;
	return false;
}

FullEngineCoopServerSessionFlushResult FullEngineCoopServerSession::flush(
	FullEngineCoopServerSessionWireSink& sink) noexcept
{
	FullEngineCoopServerSessionFlushResult result;
	if (!active_)
	{
		result.result = FullEngineCoopServerSessionResult::NotActive;
		return result;
	}
	if (!worldActive_)
	{
		result.result = FullEngineCoopServerSessionResult::NoWorld;
		return result;
	}
	if (flushing_)
	{
		result.result = FullEngineCoopServerSessionResult::Busy;
		return result;
	}
	struct FlushGuard
	{
		explicit FlushGuard(bool& active) noexcept : active_(active)
		{
			active_ = true;
		}
		~FlushGuard() { active_ = false; }
		bool& active_;
	} guard(flushing_);

	for (std::size_t peerIndex = 0; peerIndex < peerCount_; ++peerIndex)
	{
		PeerRecord& peer = peers_[peerIndex];
		if (!peer.connected) continue;
		if (result.messagesSent >= configuration_.maximumMessagesPerFlush)
			return result;

		if (peer.phase == CoopTacticalPeerPhase::AwaitingBaselineAck &&
			!peer.baselineSent)
		{
			if (peer.nextSentCheckpointOrdinal == 0)
			{
				result.result =
					FullEngineCoopServerSessionResult::SequenceExhausted;
				return result;
			}
			if (peer.sentCheckpointCount == peer.sentCheckpoints.size() &&
				(!peer.committedCheckpointValid ||
				 peer.sentCheckpoints[peer.sentCheckpointHead].sendOrdinal >
					peer.committedOrdinal))
				continue;
			if (!sink.send(peer.identity,
					CoopTacticalOutboundMessageKind::Baseline,
					CoopTacticalBaselineMessageName,
					peer.baselineBytes.data(), peer.baselineBytes.size()))
			{
				result.backpressured = true;
				return result;
			}
			peer.baselineSent = true;
			recordSentCheckpoint(peer, SentCheckpoint{0, true, peer.baselineId, 0,
				peer.baselineRevision, peer.baselineTurnSerial,
				peer.baselineNextExpectedCommandId, peer.baselineChecksum});
			++result.messagesSent;
		}
		if (peer.phase != CoopTacticalPeerPhase::Active) continue;

		while (peer.inFlightDeltas <
			configuration_.maximumInFlightDeltasPerPeer)
		{
			if (result.messagesSent >=
				configuration_.maximumMessagesPerFlush)
				return result;
			if (peer.nextDeltaToSend == 0 ||
				peer.nextDeltaToSend == nextDeltaId_)
				break;
			DeltaRecord* delta = findDelta(peer.nextDeltaToSend);
			if (!delta)
			{
				requireResync(peer);
				result.result =
					FullEngineCoopServerSessionResult::ResyncRequired;
				break;
			}
			if (peer.nextSentCheckpointOrdinal == 0)
			{
				result.result =
					FullEngineCoopServerSessionResult::SequenceExhausted;
				return result;
			}
			if (peer.sentCheckpointCount == peer.sentCheckpoints.size() &&
				(!peer.committedCheckpointValid ||
				 peer.sentCheckpoints[peer.sentCheckpointHead].sendOrdinal >
					peer.committedOrdinal))
				break;
			if (!sink.send(peer.identity,
					CoopTacticalOutboundMessageKind::Delta,
					CoopTacticalDeltaMessageName,
					delta->bytes.data(), delta->bytes.size()))
			{
				result.backpressured = true;
				return result;
			}
			peer.nextDeltaToSend = nextDeltaId(delta->id);
			recordSentCheckpoint(peer, SentCheckpoint{0, false, peer.baselineId,
				delta->id, delta->revision, delta->turnSerial,
				peer.baselineNextExpectedCommandId, delta->checksum});
			peer.lastSentRevision = delta->revision;
			++peer.inFlightDeltas;
			++result.messagesSent;
		}

		// An outcome naming revision N is never emitted before every delta through
		// N has entered this peer's reliable transport queue.
		for (std::size_t receiptIndex = 0;
			receiptIndex < peer.receiptCount; ++receiptIndex)
		{
			ReceiptRecord& receipt = receiptAt(peer, receiptIndex);
			if (!receipt.pending || receipt.revision > peer.lastSentRevision)
				continue;
			if (result.messagesSent >=
				configuration_.maximumMessagesPerFlush)
				return result;
			if (!sink.send(peer.identity,
					CoopTacticalOutboundMessageKind::IntentReceipt,
					CoopTacticalIntentReceiptMessageName,
					receipt.bytes.data(), receipt.bytes.size()))
			{
				result.backpressured = true;
				return result;
			}
			receipt.pending = false;
			++result.messagesSent;
		}
	}
	return result;
}

std::size_t FullEngineCoopServerSession::peersNeedingBaseline(
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers>& output) const
	noexcept
{
	output = {};
	std::size_t count = 0;
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		const PeerRecord& peer = peers_[index];
		if (!peer.connected ||
			(peer.phase != CoopTacticalPeerPhase::NeedsBaseline &&
				peer.phase != CoopTacticalPeerPhase::ResyncRequired))
			continue;
		output[count++] = peer.identity;
	}
	return count;
}
}
