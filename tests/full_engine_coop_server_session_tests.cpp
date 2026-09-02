#include "FullEngineCoopServerSession.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); } } while (0)

PeerIdentity Identity(std::uint8_t seed)
{
	PeerIdentity identity{};
	for (std::size_t index = 0; index < identity.size(); ++index)
		identity[index] = static_cast<std::uint8_t>(seed + index);
	return identity;
}

TacticalActorSnapshot Actor(std::uint16_t slot)
{
	TacticalActorSnapshot actor;
	actor.id = TacticalEntityId{slot, 1};
	actor.team = 0;
	actor.profile = slot;
	actor.grid = 1000 + slot;
	actor.direction = 2;
	actor.stance = TacticalStance::Standing;
	actor.actionPoints = 20;
	actor.life = 80;
	actor.maximumLife = 90;
	actor.breath = 75;
	actor.maximumBreath = 100;
	actor.active = true;
	actor.inSector = true;
	return actor;
}

TacticalWorldSnapshot Snapshot(
	std::uint64_t generation,
	std::uint64_t turn,
	std::size_t actorCount = 2)
{
	std::vector<TacticalActorSnapshot> actors;
	for (std::size_t index = 0; index < actorCount; ++index)
		actors.push_back(Actor(static_cast<std::uint16_t>(index + 1)));
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(
		generation, TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true},
		TacticalTurnSnapshot{true, true, 0, turn},
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"session snapshot fixture is valid");
	return snapshot;
}

TacticalWorldDelta EmptyDelta(std::uint64_t generation)
{
	TacticalWorldDelta delta;
	delta.previousEpoch = generation;
	delta.currentEpoch = generation;
	return delta;
}

CoopTacticalIntentReceipt Receipt(
	const PeerIdentity& peer,
	std::uint64_t command,
	std::uint64_t session,
	std::uint64_t generation,
	std::uint64_t revision,
	std::uint64_t turn)
{
	CoopTacticalIntentReceipt receipt;
	receipt.state.sessionEpoch = session;
	receipt.state.worldGeneration = generation;
	receipt.state.revision = revision;
	receipt.state.turnSerial = turn;
	receipt.peerIdentity = peer;
	receipt.commandId = command;
	receipt.nextExpectedCommandId = command + 1;
	receipt.authoritativeSequence = command - 1;
	receipt.simulationTick = 100 + command;
	receipt.status = CoopTacticalIntentReceiptStatus::Applied;
	receipt.reason = CoopTacticalIntentReceiptReason::None;
	return receipt;
}

struct SentMessage
{
	PeerIdentity peer{};
	CoopTacticalOutboundMessageKind kind =
		CoopTacticalOutboundMessageKind::Baseline;
	std::string name;
	std::vector<std::uint8_t> bytes;
};

class RecordingSink : public FullEngineCoopServerSessionWireSink
{
public:
	bool send(const PeerIdentity& peer,
		CoopTacticalOutboundMessageKind kind,
		const char* messageName,
		const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		if (rejectNext)
		{
			rejectNext = false;
			return false;
		}
		try
		{
			messages.push_back(SentMessage{
				peer, kind, messageName,
				std::vector<std::uint8_t>(bytes, bytes + size)});
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool rejectNext = false;
	std::vector<SentMessage> messages;
};

CoopTacticalBaseline DecodeBaseline(const SentMessage& message)
{
	CoopTacticalBaseline baseline;
	CHECK(message.kind == CoopTacticalOutboundMessageKind::Baseline &&
		message.name == CoopTacticalBaselineMessageName,
		"captured message is a named baseline");
	CHECK(DecodeCoopTacticalBaseline(message.bytes, baseline) ==
		CoopTacticalCodecResult::Success,
		"captured baseline decodes");
	return baseline;
}

CoopTacticalDelta DecodeDelta(const SentMessage& message)
{
	CoopTacticalDelta delta;
	CHECK(message.kind == CoopTacticalOutboundMessageKind::Delta &&
		message.name == CoopTacticalDeltaMessageName,
		"captured message is a named delta");
	CHECK(DecodeCoopTacticalDelta(message.bytes, delta) ==
		CoopTacticalCodecResult::Success,
		"captured delta decodes");
	return delta;
}

CoopTacticalBaselineAckBytes BaselineAckBytes(
	const CoopTacticalBaseline& baseline,
	const PeerIdentity& peer)
{
	CoopTacticalBaselineAck acknowledgement;
	acknowledgement.state = baseline.state;
	acknowledgement.peerIdentity = peer;
	acknowledgement.baselineId = baseline.baselineId;
	acknowledgement.payloadChecksum = baseline.payloadChecksum;
	acknowledgement.nextExpectedCommandId =
		baseline.nextExpectedCommandId;
	CoopTacticalBaselineAckBytes bytes{};
	CHECK(EncodeCoopTacticalBaselineAck(acknowledgement, bytes) ==
		CoopTacticalCodecResult::Success, "test baseline ACK encodes");
	return bytes;
}

CoopTacticalDeltaAckBytes DeltaAckBytes(
	const CoopTacticalDelta& delta,
	const PeerIdentity& peer)
{
	CoopTacticalDeltaAck acknowledgement;
	acknowledgement.state = delta.state;
	acknowledgement.peerIdentity = peer;
	acknowledgement.deltaId = delta.deltaId;
	acknowledgement.payloadChecksum = delta.payloadChecksum;
	CoopTacticalDeltaAckBytes bytes{};
	CHECK(EncodeCoopTacticalDeltaAck(acknowledgement, bytes) ==
		CoopTacticalCodecResult::Success, "test delta ACK encodes");
	return bytes;
}

CoopTacticalResyncRequestBytes ResyncBytes(
	const CoopTacticalStateIdentity& state, std::uint64_t requestId,
	std::uint64_t baselineId, std::uint64_t deltaId,
	std::uint32_t checksum, CoopTacticalResyncReason reason)
{
	CoopTacticalResyncRequest request;
	request.acceptedState = state;
	request.requestId = requestId;
	request.acceptedBaselineId = baselineId;
	request.lastAppliedDeltaId = deltaId;
	request.lastPayloadChecksum = checksum;
	request.reason = reason;
	request.nextExpectedCommandId = UINT64_C(0xfedcba9876543210);
	CoopTacticalResyncRequestBytes bytes{};
	CHECK(EncodeCoopTacticalResyncRequest(request, bytes) ==
		CoopTacticalCodecResult::Success, "test resync request encodes");
	return bytes;
}

CoopTacticalBaseline MakePeerLive(
	FullEngineCoopServerSession& session,
	RecordingSink& sink,
	const PeerIdentity& peer,
	const TacticalWorldSnapshot& snapshot)
{
	CHECK(session.stageBaseline(peer, snapshot, 1) ==
		FullEngineCoopServerSessionResult::Success,
		"peer baseline stages");
	const std::size_t before = sink.messages.size();
	CHECK(session.flush(sink).messagesSent == 1,
		"peer baseline flushes");
	CoopTacticalBaseline baseline = DecodeBaseline(sink.messages[before]);
	const CoopTacticalBaselineAckBytes acknowledgement =
		BaselineAckBytes(baseline, peer);
	CHECK(session.acknowledgeBaseline(peer, acknowledgement.data(),
		acknowledgement.size()) == FullEngineCoopServerSessionResult::Success,
		"peer baseline ACK enables live phase");
	CHECK(session.peerPhase(peer) == CoopTacticalPeerPhase::Active,
		"peer is live after exact baseline ACK");
	return baseline;
}

bool SamePeerState(const CoopTacticalPeerReplicationState& left,
	const CoopTacticalPeerReplicationState& right)
{
	return left.peerIdentity == right.peerIdentity &&
		left.phase == right.phase && left.connected == right.connected &&
		left.baselineId == right.baselineId &&
		left.baselineRevision == right.baselineRevision &&
		left.baselineNextExpectedCommandId ==
			right.baselineNextExpectedCommandId &&
		left.nextDeltaToSend == right.nextDeltaToSend &&
		left.nextDeltaToAcknowledge == right.nextDeltaToAcknowledge &&
		left.lastSentRevision == right.lastSentRevision &&
		left.lastAcknowledgedDeltaId == right.lastAcknowledgedDeltaId &&
		left.lastAcknowledgedRevision == right.lastAcknowledgedRevision &&
		left.inFlightDeltas == right.inFlightDeltas &&
		left.retainedReceipts == right.retainedReceipts &&
		left.pendingReceipts == right.pendingReceipts;
}

void TestConfigurationLifecycleAndAssignments()
{
	FullEngineCoopServerSessionConfiguration invalidConfiguration;
	invalidConfiguration.maximumDeltaHistory = 0;
	FullEngineCoopServerSession invalid(invalidConfiguration);
	CHECK(invalid.beginSession(1) ==
		FullEngineCoopServerSessionResult::InvalidConfiguration &&
		!invalid.active(), "invalid bounded configuration fails closed");

	FullEngineCoopServerSessionConfiguration configuration;
	configuration.maximumPeers = 2;
	FullEngineCoopServerSession session(configuration);
	CHECK(session.beginSession(0) ==
		FullEngineCoopServerSessionResult::InvalidContext,
		"zero session epoch is rejected");
	CHECK(session.beginSession(101) ==
		FullEngineCoopServerSessionResult::Success,
		"valid session begins");
	const PeerIdentity high = Identity(0x40);
	const PeerIdentity low = Identity(0x10);
	CHECK(session.connectPeer(high) ==
		FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(low) ==
		FullEngineCoopServerSessionResult::Success,
		"peers connect in arbitrary order");
	CHECK(session.connectPeer(Identity(0x70)) ==
		FullEngineCoopServerSessionResult::PeerCapacityReached,
		"peer table is bounded");
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> needs{};
	CHECK(session.peersNeedingBaseline(needs) == 2 && needs[0] == low &&
		needs[1] == high,
		"baseline work is exposed in deterministic identity order");

	CHECK(session.beginWorld(7, 1, 3) ==
		FullEngineCoopServerSessionResult::Success,
		"first tactical world begins");
	const std::array<CoopTacticalActorAssignment, 2> assignments{{
		{TacticalEntityId{1, 1}, high},
		{TacticalEntityId{2, 1}, low}}};
	const TacticalEntityId firstActor{1, 1};
	CHECK(session.replaceAssignments(assignments.data(), assignments.size()) ==
		FullEngineCoopServerSessionResult::Success &&
		session.assignmentCount() == 2 &&
		session.assignment(0)->actor == firstActor &&
		session.assignment(1)->peerIdentity == low,
		"strict actor-ordered assignments publish transactionally");
	auto malformed = assignments;
	std::swap(malformed[0], malformed[1]);
	CHECK(session.replaceAssignments(malformed.data(), malformed.size()) ==
		FullEngineCoopServerSessionResult::InvalidAssignment &&
		session.assignment(0)->actor == firstActor,
		"unordered assignment input is rejected without mutation");
	malformed = assignments;
	malformed[1].actor = malformed[0].actor;
	CHECK(session.replaceAssignments(malformed.data(), malformed.size()) ==
		FullEngineCoopServerSessionResult::InvalidAssignment,
		"duplicate actor ownership is rejected");
	malformed = assignments;
	malformed[1].peerIdentity = Identity(0x70);
	CHECK(session.replaceAssignments(malformed.data(), malformed.size()) ==
		FullEngineCoopServerSessionResult::InvalidAssignment,
		"assignment to an unretained peer is rejected");
	CHECK(session.beginWorld(7, 2, 4) ==
		FullEngineCoopServerSessionResult::StaleContext,
		"world generation cannot rewind or restart in place");
	CHECK(session.beginWorld(8, 2, 4) ==
		FullEngineCoopServerSessionResult::Success &&
		session.assignmentCount() == 0,
		"new world generation clears old actor ownership");
	session.endSession();
	CHECK(!session.active() && session.peerCount() == 0,
		"session teardown clears all retained identities");
}

void TestBaselineAckGatingAndCatchup()
{
	FullEngineCoopServerSessionConfiguration configuration;
	configuration.maximumInFlightDeltasPerPeer = 2;
	FullEngineCoopServerSession session(configuration);
	const PeerIdentity peer = Identity(0x20);
	CHECK(session.beginSession(201) ==
		FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) ==
		FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(11, 20, 5) ==
		FullEngineCoopServerSessionResult::Success,
		"baseline gating fixture starts");
	const TacticalWorldSnapshot snapshot = Snapshot(11, 5);
	const CoopTacticalActorAssignment assignment{
		TacticalEntityId{1, 1}, peer};
	CHECK(session.replaceAssignments(&assignment, 1) ==
		FullEngineCoopServerSessionResult::Success,
		"server installs deterministic peer actor assignment");
	CHECK(session.stageBaseline(peer, snapshot, 7) ==
		FullEngineCoopServerSessionResult::Success,
		"baseline stages before deltas");
	RecordingSink sink;
	sink.rejectNext = true;
	const FullEngineCoopServerSessionFlushResult blocked = session.flush(sink);
	CHECK(blocked.backpressured && blocked.messagesSent == 0 &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::AwaitingBaselineAck,
		"transport backpressure does not advance baseline state");
	CHECK(session.flush(sink).messagesSent == 1 && sink.messages.size() == 1,
		"baseline retries exactly after backpressure");
	const CoopTacticalBaseline baseline = DecodeBaseline(sink.messages[0]);
	CHECK(baseline.assignedActors ==
		std::vector<TacticalEntityId>({TacticalEntityId{1, 1}}) &&
		baseline.nextExpectedCommandId == 7,
		"baseline carries the exact peer assignment and command cursor before activation");

	CHECK(session.publishDelta(EmptyDelta(11), 21, 5) ==
		FullEngineCoopServerSessionResult::Success &&
		session.flush(sink).messagesSent == 0,
		"delta cannot pass an unacknowledged baseline");
	CoopTacticalBaselineAckBytes acknowledgement =
		BaselineAckBytes(baseline, peer);
	auto corruptAck = acknowledgement;
	corruptAck[72] ^= 1;
	CHECK(session.acknowledgeBaseline(peer, corruptAck.data(), corruptAck.size()) ==
		FullEngineCoopServerSessionResult::IntegrityMismatch,
		"baseline ACK must echo exact payload checksum");
	corruptAck = acknowledgement;
	corruptAck[65] ^= 1;
	CHECK(session.acknowledgeBaseline(peer, corruptAck.data(), corruptAck.size()) ==
		FullEngineCoopServerSessionResult::UnexpectedAcknowledgement,
		"baseline ACK must echo exact baseline ID");
	corruptAck = acknowledgement;
	corruptAck[48] ^= 1;
	CHECK(session.acknowledgeBaseline(peer, corruptAck.data(), corruptAck.size()) ==
		FullEngineCoopServerSessionResult::UnexpectedAcknowledgement,
		"baseline ACK must echo transport-resolved peer identity");
	corruptAck = acknowledgement;
	corruptAck[80] ^= 1;
	CHECK(session.acknowledgeBaseline(peer, corruptAck.data(),
		corruptAck.size()) ==
		FullEngineCoopServerSessionResult::UnexpectedAcknowledgement,
		"baseline ACK must echo the exact authoritative command cursor");
	CHECK(session.acknowledgeBaseline(peer, acknowledgement.data(),
		acknowledgement.size()) == FullEngineCoopServerSessionResult::Success,
		"exact delayed baseline ACK opens catch-up");
	CHECK(session.flush(sink).messagesSent == 1 && sink.messages.size() == 2,
		"retained post-baseline delta flushes after ACK");
	const CoopTacticalDelta delta = DecodeDelta(sink.messages[1]);
	CHECK(delta.baseRevision == 20 && delta.state.revision == 21,
		"catch-up delta carries exact revision edge");
	const CoopTacticalDeltaAckBytes deltaAck = DeltaAckBytes(delta, peer);
	auto wrongDeltaAck = deltaAck;
	wrongDeltaAck[64] += 1;
	CHECK(session.acknowledgeDelta(peer, wrongDeltaAck.data(),
		wrongDeltaAck.size()) ==
		FullEngineCoopServerSessionResult::UnexpectedAcknowledgement,
		"out-of-order delta ACK is rejected");
	wrongDeltaAck = deltaAck;
	wrongDeltaAck[72] ^= 1;
	CHECK(session.acknowledgeDelta(peer, wrongDeltaAck.data(),
		wrongDeltaAck.size()) ==
		FullEngineCoopServerSessionResult::IntegrityMismatch,
		"delta ACK must echo exact checksum");
	CHECK(session.acknowledgeDelta(peer, deltaAck.data(), deltaAck.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"exact delta ACK advances peer window");
	CoopTacticalPeerReplicationState peerState;
	CHECK(session.peerState(peer, peerState) &&
		peerState.inFlightDeltas == 0,
		"delta ACK closes the in-flight slot");
}

void TestDeltaRingForcesResync()
{
	FullEngineCoopServerSessionConfiguration configuration;
	configuration.maximumDeltaHistory = 2;
	configuration.maximumInFlightDeltasPerPeer = 1;
	FullEngineCoopServerSession session(configuration);
	RecordingSink sink;
	const PeerIdentity peer = Identity(0x30);
	CHECK(session.beginSession(301) ==
		FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) ==
		FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(15, 1, 1) ==
		FullEngineCoopServerSessionResult::Success,
		"delta eviction fixture starts");
	const CoopTacticalBaseline original =
		MakePeerLive(session, sink, peer, Snapshot(15, 1));
	const std::size_t deltaMessage = sink.messages.size();
	CHECK(session.publishDelta(EmptyDelta(15), 2, 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.flush(sink).messagesSent == 1,
		"first delta is sent and remains unacknowledged");
	CHECK(session.publishDelta(EmptyDelta(15), 3, 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.publishDelta(EmptyDelta(15), 4, 1) ==
		FullEngineCoopServerSessionResult::Success,
		"ring accepts later authoritative deltas");
	CHECK(session.deltaHistorySize() == 2 &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::ResyncRequired,
		"evicting an unacknowledged delta forces resynchronization");
	const CoopTacticalDelta applied = DecodeDelta(sink.messages[deltaMessage]);
	const CoopTacticalDeltaAckBytes lateAck = DeltaAckBytes(applied, peer);
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> needs{};
	CHECK(session.peersNeedingBaseline(needs) == 1 && needs[0] == peer,
		"resync peer is exposed for caller-owned baseline capture");

	CHECK(session.stageBaseline(peer, Snapshot(15, 1), 1) ==
		FullEngineCoopServerSessionResult::Success,
		"current baseline replaces lost delta history");
	const std::size_t before = sink.messages.size();
	CHECK(session.flush(sink).messagesSent == 1,
		"replacement baseline flushes");
	const CoopTacticalBaseline current = DecodeBaseline(sink.messages[before]);
	CHECK(current.state.revision == 4,
		"replacement baseline identifies current revision");
	CHECK(session.acknowledgeDelta(peer, lateAck.data(), lateAck.size()) ==
		FullEngineCoopServerSessionResult::Success &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::AwaitingBaselineAck,
		"late exact ACK promotes its server-sent checkpoint without activating replacement B1");
	const CoopTacticalResyncRequestBytes rejected = ResyncBytes(
		applied.state, 1, original.baselineId, applied.deltaId,
		applied.payloadChecksum, CoopTacticalResyncReason::BaselineRejected);
	CHECK(session.requestResync(peer, rejected.data(), rejected.size()) ==
		FullEngineCoopServerSessionResult::Success &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::ResyncRequired,
		"rejected replacement validates against the promoted late-ACK checkpoint");
	CHECK(session.stageBaseline(peer, Snapshot(15, 1), 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.flush(sink).messagesSent == 1,
		"newer precise request permits a fresh replacement baseline");
	const CoopTacticalBaseline recovery = DecodeBaseline(sink.messages.back());
	const CoopTacticalBaselineAckBytes recoveryAck =
		BaselineAckBytes(recovery, peer);
	CHECK(session.acknowledgeBaseline(peer, recoveryAck.data(), recoveryAck.size()) ==
		FullEngineCoopServerSessionResult::Success &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::Active,
		"fresh replacement ACK completes recovery after delta eviction");
}

void TestCumulativeDeltaAckAndReceiptOrdering()
{
	FullEngineCoopServerSessionConfiguration configuration;
	configuration.maximumInFlightDeltasPerPeer = 4;
	FullEngineCoopServerSession session(configuration);
	RecordingSink sink;
	const PeerIdentity peer = Identity(0x38);
	CHECK(session.beginSession(351) ==
		FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) ==
		FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(17, 1, 1) ==
		FullEngineCoopServerSessionResult::Success,
		"cumulative ACK fixture starts");
	MakePeerLive(session, sink, peer, Snapshot(17, 1));
	sink.messages.clear();
	for (std::uint64_t revision = 2; revision <= 4; ++revision)
		CHECK(session.publishDelta(EmptyDelta(17), revision, 1) ==
			FullEngineCoopServerSessionResult::Success,
			"ordered delta publishes for cumulative ACK");
	CHECK(session.flush(sink).messagesSent == 3 && sink.messages.size() == 3,
		"all deltas enter the bounded in-flight window");
	const CoopTacticalDelta first = DecodeDelta(sink.messages[0]);
	const CoopTacticalDelta last = DecodeDelta(sink.messages[2]);
	const CoopTacticalDeltaAckBytes cumulative = DeltaAckBytes(last, peer);
	CHECK(session.acknowledgeDelta(peer, cumulative.data(), cumulative.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"latest exact ACK cumulatively confirms its sent prefix");
	CoopTacticalPeerReplicationState state;
	CHECK(session.peerState(peer, state) && state.inFlightDeltas == 0 &&
		state.lastAcknowledgedDeltaId == last.deltaId &&
		state.lastAcknowledgedRevision == 4,
		"cumulative ACK closes every earlier in-flight delta");
	CHECK(session.acknowledgeDelta(peer, cumulative.data(), cumulative.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"exact duplicate cumulative ACK is idempotent");
	CHECK(session.publishDelta(EmptyDelta(17), 5, 1) ==
		FullEngineCoopServerSessionResult::Success,
		"outcome revision delta publishes");
	CHECK(session.recordReceipt(Receipt(peer, 7, 351, 17, 5, 1)) ==
		FullEngineCoopServerSessionResult::Success,
		"terminal outcome is retained for its resulting revision");
	sink.messages.clear();
	CHECK(session.flush(sink).messagesSent == 2 &&
		sink.messages.size() == 2 &&
		sink.messages[0].kind == CoopTacticalOutboundMessageKind::Delta &&
		sink.messages[1].kind ==
			CoopTacticalOutboundMessageKind::IntentReceipt,
		"state delta is always queued before its terminal receipt");
	const CoopTacticalDeltaAckBytes stale = DeltaAckBytes(first, peer);
	CHECK(session.acknowledgeDelta(peer, stale.data(), stale.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"older exact sent ACK remains harmless with a newer delta in flight");
	CHECK(session.peerState(peer, state) && state.inFlightDeltas == 1 &&
		state.lastAcknowledgedDeltaId == last.deltaId &&
		state.lastAcknowledgedRevision == 4,
		"older exact sent ACK cannot rewind or consume the newer in-flight delta");
}

void TestReceiptReplayAndCapacity()
{
	FullEngineCoopServerSessionConfiguration configuration;
	configuration.maximumReceiptHistoryPerPeer = 2;
	FullEngineCoopServerSession session(configuration);
	RecordingSink sink;
	const PeerIdentity peer = Identity(0x50);
	CHECK(session.beginSession(401) ==
		FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) ==
		FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(21, 9, 2) ==
		FullEngineCoopServerSessionResult::Success,
		"receipt fixture starts");
	MakePeerLive(session, sink, peer, Snapshot(21, 2));
	sink.messages.clear();
	const CoopTacticalIntentReceipt first =
		Receipt(peer, 1, 401, 21, 9, 2);
	const CoopTacticalIntentReceipt second =
		Receipt(peer, 2, 401, 21, 9, 2);
	const CoopTacticalIntentReceipt third =
		Receipt(peer, 3, 401, 21, 9, 2);
	CoopTacticalIntentReceipt queuedFirst = first;
	queuedFirst.status = CoopTacticalIntentReceiptStatus::Queued;
	CHECK(session.recordReceipt(queuedFirst) ==
		FullEngineCoopServerSessionResult::Success &&
		session.recordReceipt(first) ==
		FullEngineCoopServerSessionResult::Success &&
		session.recordReceipt(second) ==
		FullEngineCoopServerSessionResult::Success,
		"queued receipt advances once to its terminal replay record");
	CHECK(session.recordReceipt(third) ==
		FullEngineCoopServerSessionResult::ReceiptCapacityReached,
		"pending receipt is never silently evicted");
	CHECK(session.flush(sink).messagesSent == 2 && sink.messages.size() == 2,
		"pending receipts flush in record order");
	CoopTacticalIntentReceipt decodedFirst;
	CHECK(sink.messages[0].kind ==
		CoopTacticalOutboundMessageKind::IntentReceipt &&
		sink.messages[0].name == CoopTacticalIntentReceiptMessageName &&
		DecodeCoopTacticalIntentReceipt(sink.messages[0].bytes.data(),
			sink.messages[0].bytes.size(), decodedFirst) ==
			CoopTacticalCodecResult::Success &&
		decodedFirst.commandId == 1,
		"first flushed receipt is exact and decodable");
	CHECK(session.recordReceipt(third) ==
		FullEngineCoopServerSessionResult::Success,
		"delivered oldest receipt may be deterministically evicted");
	CoopTacticalIntentReceipt conflict = second;
	conflict.simulationTick++;
	CHECK(session.recordReceipt(conflict) ==
		FullEngineCoopServerSessionResult::ConflictingReceipt,
		"same command cannot acquire a different terminal receipt");
	CHECK(session.replayReceipt(peer, 2) ==
		FullEngineCoopServerSessionResult::Success,
		"retained command receipt can be explicitly replayed");
	sink.messages.clear();
	CHECK(session.flush(sink).messagesSent == 2,
		"explicit replay and newly queued receipt both flush");
	CHECK(session.replayReceipt(peer, 1) ==
		FullEngineCoopServerSessionResult::NotFound,
		"evicted receipt cannot be ambiguously reconstructed");

	CHECK(session.disconnectPeer(peer) ==
		FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) ==
		FullEngineCoopServerSessionResult::Success &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::NeedsBaseline,
		"reconnect retains identity but requires a fresh baseline");
	CHECK(session.replayReceipt(peer, 2) ==
		FullEngineCoopServerSessionResult::Success,
		"receipt history survives transport reconnect");
}

void TestCommittedPeerRetirementFreesCapacityExactly()
{
	FullEngineCoopServerSessionConfiguration configuration;
	configuration.maximumPeers = MaximumCoopTacticalSessionPeers;
	FullEngineCoopServerSession session(configuration);
	CHECK(session.beginSession(451) ==
			FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(25, 9, 2) ==
			FullEngineCoopServerSessionResult::Success,
		"peer retirement replication fixture starts");
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> peers{};
	std::array<CoopTacticalActorAssignment,
		MaximumCoopTacticalSessionPeers> assignments{};
	for (std::size_t index = 0; index < peers.size(); ++index)
	{
		peers[index] = Identity(static_cast<std::uint8_t>(0x70 + index * 0x10));
		assignments[index] = CoopTacticalActorAssignment{
			TacticalEntityId{static_cast<std::uint16_t>(index + 1), 1},
			peers[index]};
		CHECK(session.connectPeer(peers[index]) ==
			FullEngineCoopServerSessionResult::Success,
			"all four fixed replication records connect");
	}
	CHECK(session.replaceAssignments(assignments.data(), assignments.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"retirement fixture publishes sorted assignments for every peer");
	const TacticalWorldSnapshot snapshot = Snapshot(25, 2, peers.size());
	for (std::size_t index = 0; index < peers.size(); ++index)
		CHECK(session.stageBaseline(peers[index], snapshot, 11 + index) ==
			FullEngineCoopServerSessionResult::Success,
			"each peer retains a distinct pending baseline cursor");
	CHECK(session.recordReceipt(Receipt(peers[2], 7, 451, 25, 9, 2)) ==
		FullEngineCoopServerSessionResult::Success,
		"survivor retains an unsent terminal receipt obligation");

	std::array<CoopTacticalPeerReplicationState,
		MaximumCoopTacticalSessionPeers> before{};
	for (std::size_t index = 1; index < peers.size(); ++index)
		CHECK(session.peerState(peers[index], before[index]),
			"survivor replication state is captured before compaction");
	CHECK(session.retirePeer(peers[0]) ==
		FullEngineCoopServerSessionResult::InvalidContext,
		"a connected record cannot be retired");
	CHECK(session.disconnectPeer(peers[0]) ==
			FullEngineCoopServerSessionResult::Success &&
		session.retirePeer(peers[0]) ==
			FullEngineCoopServerSessionResult::Success,
		"offline peer compacts only at the explicit committed boundary");
	CoopTacticalPeerReplicationState absent;
	CHECK(session.peerCount() == peers.size() - 1 &&
		!session.peerState(peers[0], absent) &&
		session.assignmentCount() == peers.size() - 1,
		"retired record and only its actor assignment are absent");
	for (std::size_t index = 1; index < peers.size(); ++index)
	{
		CoopTacticalPeerReplicationState after;
		CHECK(session.peerState(peers[index], after) &&
			SamePeerState(before[index], after),
			"stable compaction preserves every survivor cursor and receipt count exactly");
	}
	CHECK(session.replayReceipt(peers[2], 7) ==
		FullEngineCoopServerSessionResult::Success,
		"survivor retained receipt bytes remain replayable after compaction");

	const PeerIdentity replacement = Identity(0xc5);
	CHECK(session.connectPeer(replacement) ==
			FullEngineCoopServerSessionResult::Success &&
		session.peerCount() == peers.size() &&
		session.peerPhase(replacement) == CoopTacticalPeerPhase::NeedsBaseline &&
		session.stageBaseline(replacement, snapshot, 1) ==
			FullEngineCoopServerSessionResult::Success,
		"a distinct fifth identity reuses the slot and receives a fresh baseline cursor");
	CoopTacticalPeerReplicationState replacementState;
	CHECK(session.peerState(replacement, replacementState) &&
		replacementState.baselineNextExpectedCommandId == 1 &&
		replacementState.retainedReceipts == 0,
		"replacement starts with no retired history or inherited command cursor");
}

class ReentrantSink final : public FullEngineCoopServerSessionWireSink
{
public:
	explicit ReentrantSink(FullEngineCoopServerSession& session,
		PeerIdentity peer) : session_(session), peer_(peer) {}

	bool send(const PeerIdentity&,
		CoopTacticalOutboundMessageKind,
		const char*, const std::uint8_t*, std::size_t) noexcept override
	{
		reentrantResult = session_.disconnectPeer(peer_);
		return true;
	}

	FullEngineCoopServerSessionResult reentrantResult =
		FullEngineCoopServerSessionResult::Success;

private:
	FullEngineCoopServerSession& session_;
	PeerIdentity peer_{};
};

void TestFlushReentrancyGuardAndSequenceExhaustion()
{
	FullEngineCoopServerSessionConfiguration configuration;
	configuration.maximumBaselineId = 1;
	configuration.maximumDeltaId = 1;
	FullEngineCoopServerSession session(configuration);
	const PeerIdentity peer = Identity(0x60);
	CHECK(session.beginSession(501) ==
		FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) ==
		FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(31, 1, 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.stageBaseline(peer, Snapshot(31, 1), 0) ==
		FullEngineCoopServerSessionResult::Success,
		"reentrancy fixture stages its only baseline ID");
	ReentrantSink sink(session, peer);
	CHECK(session.flush(sink).messagesSent == 1 &&
		sink.reentrantResult == FullEngineCoopServerSessionResult::Busy &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::AwaitingBaselineAck,
		"wire callback cannot re-enter session mutation");
	CHECK(session.stageBaseline(peer, Snapshot(31, 1), 0) ==
		FullEngineCoopServerSessionResult::SequenceExhausted,
		"baseline identity never wraps");
	CHECK(session.publishDelta(EmptyDelta(31), 2, 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.publishDelta(EmptyDelta(31), 3, 1) ==
		FullEngineCoopServerSessionResult::SequenceExhausted,
		"delta identity never wraps");
}

void TestDeltaTurnIdentity()
{
	FullEngineCoopServerSession session;
	CHECK(session.beginSession(601) ==
		FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(41, 1, 10) ==
		FullEngineCoopServerSessionResult::Success,
		"turn identity fixture starts");
	CHECK(session.publishDelta(EmptyDelta(41), 2, 11) ==
		FullEngineCoopServerSessionResult::InvalidContext,
		"turn serial cannot advance without its authoritative turn event");
	TacticalWorldDelta turnDelta = EmptyDelta(41);
	turnDelta.events.push_back(TacticalTurnChangedEvent{
		TacticalTurnSnapshot{true, true, 0, 10},
		TacticalTurnSnapshot{true, true, 1, 11}});
	CHECK(session.publishDelta(turnDelta, 2, 11) ==
		FullEngineCoopServerSessionResult::Success &&
		session.turnSerial() == 11,
		"matching turn edge advances revision and turn atomically");
	turnDelta.events[0] = TacticalTurnChangedEvent{
		TacticalTurnSnapshot{true, true, 1, 10},
		TacticalTurnSnapshot{true, true, 0, 12}};
	CHECK(session.publishDelta(turnDelta, 3, 12) ==
		FullEngineCoopServerSessionResult::InvalidContext,
		"turn event cannot skip its exact previous serial");
}

void TestAuthenticatedResyncPreservesAuthorityState()
{
	FullEngineCoopServerSessionConfiguration configuration;
	FullEngineCoopServerSession session(configuration);
	const PeerIdentity peer = Identity(0x64);
	CHECK(session.beginSession(801) == FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) == FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(71, 4, 9) == FullEngineCoopServerSessionResult::Success,
		"resync fixture starts");
	const CoopTacticalActorAssignment assignment{TacticalEntityId{1, 1}, peer};
	CHECK(session.replaceAssignments(&assignment, 1) ==
		FullEngineCoopServerSessionResult::Success, "resync fixture assigns actor");
	RecordingSink sink;
	CHECK(session.stageBaseline(peer, Snapshot(71, 9), 1) ==
		FullEngineCoopServerSessionResult::Success, "resync baseline stages");
	CHECK(session.flush(sink).messagesSent == 1, "resync baseline flushes");
	if (sink.messages.empty()) return;
	const CoopTacticalBaseline baseline = DecodeBaseline(sink.messages.back());
	const CoopTacticalBaselineAckBytes baselineAck = BaselineAckBytes(baseline, peer);
	CHECK(session.acknowledgeBaseline(peer, baselineAck.data(), baselineAck.size()) ==
		FullEngineCoopServerSessionResult::Success, "resync baseline is active");
	for (std::uint64_t commandId = 1; commandId <= 32; ++commandId)
		CHECK(session.recordReceipt(Receipt(
			peer, commandId, 801, 71, 4, 9)) ==
			FullEngineCoopServerSessionResult::Success,
			"resync fixture fills authoritative receipt history");
	CHECK(session.flush(sink).messagesSent == 32,
		"resync fixture sends maximum receipt history before retaining it");
	const CoopTacticalResyncRequestBytes request = ResyncBytes(
		baseline.state, 1, baseline.baselineId, 0, baseline.payloadChecksum,
		CoopTacticalResyncReason::ReplicaRejected);
	CoopTacticalPeerReplicationState resyncState;
	CHECK(session.requestResync(peer, request.data(), request.size()) ==
		FullEngineCoopServerSessionResult::Success &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::ResyncRequired &&
		session.assignmentCount() == 1 && session.assignment(0)->actor == assignment.actor &&
		session.peerState(peer, resyncState) && resyncState.retainedReceipts == 32 &&
		resyncState.pendingReceipts == 0,
		"exact-cursor resync preserves maximum history without replaying it");
	CHECK(session.requestResync(peer, request.data(), request.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"exact resync duplicate is idempotent after replication reset");
	auto conflicting = request;
	conflicting[80] ^= 1;
	CHECK(session.requestResync(peer, conflicting.data(), conflicting.size()) !=
		FullEngineCoopServerSessionResult::Success,
		"same resync request ID with different bytes is rejected");
	CHECK(session.stageBaseline(peer, Snapshot(71, 9), 23) ==
		FullEngineCoopServerSessionResult::Success,
		"next safe frame may stage a fresh baseline on the same connection");
	const FullEngineCoopServerSessionFlushResult replacementFlush = session.flush(sink);
	CHECK(replacementFlush.messagesSent == 1, "replacement baseline sends");
	if (replacementFlush.messagesSent != 1) return;
	const CoopTacticalBaseline replacement = DecodeBaseline(sink.messages.back());
	const CoopTacticalResyncRequestBytes rejected = ResyncBytes(
		baseline.state, 2, baseline.baselineId, 0,
		baseline.payloadChecksum, CoopTacticalResyncReason::BaselineRejected);
	CHECK(session.requestResync(peer, rejected.data(), rejected.size()) ==
		FullEngineCoopServerSessionResult::Success &&
		session.peerPhase(peer) == CoopTacticalPeerPhase::ResyncRequired,
		"newer rejected-baseline request rotates the awaiting baseline");
	CHECK(session.stageBaseline(peer, Snapshot(71, 9), 23) ==
		FullEngineCoopServerSessionResult::Success,
		"state-mismatch retry stages another replacement baseline");
	const CoopTacticalResyncRequestBytes stateMismatch = ResyncBytes(
		baseline.state, 3, baseline.baselineId, 0, baseline.payloadChecksum,
		CoopTacticalResyncReason::StateMismatch);
	CHECK(session.requestResync(peer, stateMismatch.data(), stateMismatch.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"state-mismatch retry validates the retained committed checkpoint");
	CHECK(session.stageBaseline(peer, Snapshot(71, 9), 23) ==
		FullEngineCoopServerSessionResult::Success,
		"invalid-envelope retry stages another replacement baseline");
	const CoopTacticalResyncRequestBytes invalidEnvelope = ResyncBytes(
		baseline.state, 4, baseline.baselineId, 0, baseline.payloadChecksum,
		CoopTacticalResyncReason::InvalidEnvelope);
	CHECK(session.requestResync(peer, invalidEnvelope.data(), invalidEnvelope.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"invalid-envelope retry validates the retained committed checkpoint");
}

void TestEqualRevisionLateBaselineAckCannotRegressLineage()
{
	FullEngineCoopServerSession session;
	const PeerIdentity peer = Identity(0x72);
	CHECK(session.beginSession(901) == FullEngineCoopServerSessionResult::Success &&
		session.connectPeer(peer) == FullEngineCoopServerSessionResult::Success &&
		session.beginWorld(81, 7, 3) == FullEngineCoopServerSessionResult::Success,
		"equal-revision late ACK fixture starts");
	const CoopTacticalActorAssignment firstAssignment{
		TacticalEntityId{1, 1}, peer};
	const CoopTacticalActorAssignment secondAssignment{
		TacticalEntityId{2, 1}, peer};
	RecordingSink sink;
	CHECK(session.replaceAssignments(&firstAssignment, 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.stageBaseline(peer, Snapshot(81, 3), 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.flush(sink).messagesSent == 1,
		"first same-revision baseline is sent");
	const CoopTacticalBaseline first = DecodeBaseline(sink.messages.back());
	const CoopTacticalBaselineAckBytes firstAck = BaselineAckBytes(first, peer);
	CHECK(session.replaceAssignments(&secondAssignment, 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.stageBaseline(peer, Snapshot(81, 3), 1) ==
		FullEngineCoopServerSessionResult::Success &&
		session.flush(sink).messagesSent == 1,
		"changed assignment supersedes the first baseline at the same revision");
	const CoopTacticalBaseline second = DecodeBaseline(sink.messages.back());
	const CoopTacticalBaselineAckBytes secondAck = BaselineAckBytes(second, peer);
	CHECK(session.acknowledgeBaseline(peer, secondAck.data(), secondAck.size()) ==
		FullEngineCoopServerSessionResult::Success &&
		session.acknowledgeBaseline(peer, firstAck.data(), firstAck.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"older server-authored ACK is harmless after newer lineage commits");
	const CoopTacticalResyncRequestBytes current = ResyncBytes(second.state, 1,
		second.baselineId, 0, second.payloadChecksum,
		CoopTacticalResyncReason::BaselineRejected);
	CHECK(session.requestResync(peer, current.data(), current.size()) ==
		FullEngineCoopServerSessionResult::Success,
		"precise resync retains the newer equal-revision lineage");
	const CoopTacticalResyncRequestBytes rejectedOld = ResyncBytes(first.state, 2,
		first.baselineId, 0, first.payloadChecksum,
		CoopTacticalResyncReason::BaselineRejected);
	CHECK(session.requestResync(peer, rejectedOld.data(), rejectedOld.size()) ==
		FullEngineCoopServerSessionResult::UnexpectedAcknowledgement &&
		session.acknowledgeBaseline(peer, firstAck.data(), firstAck.size()) ==
		FullEngineCoopServerSessionResult::UnexpectedAcknowledgement,
		"accepted resync purges rejected same-revision sent proofs");

	FullEngineCoopServerSession ordered;
	RecordingSink orderedSink;
	CHECK(ordered.beginSession(902) == FullEngineCoopServerSessionResult::Success &&
		ordered.connectPeer(peer) == FullEngineCoopServerSessionResult::Success &&
		ordered.beginWorld(82, 7, 3) == FullEngineCoopServerSessionResult::Success &&
		ordered.replaceAssignments(&firstAssignment, 1) ==
			FullEngineCoopServerSessionResult::Success &&
		ordered.stageBaseline(peer, Snapshot(82, 3), 1) ==
			FullEngineCoopServerSessionResult::Success &&
		ordered.flush(orderedSink).messagesSent == 1,
		"assignment-race fixture sends B1");
	const CoopTacticalBaseline b1 = DecodeBaseline(orderedSink.messages.back());
	const CoopTacticalBaselineAckBytes b1Ack = BaselineAckBytes(b1, peer);
	CHECK(ordered.replaceAssignments(&secondAssignment, 1) ==
			FullEngineCoopServerSessionResult::Success &&
		ordered.stageBaseline(peer, Snapshot(82, 3), 1) ==
			FullEngineCoopServerSessionResult::Success &&
		ordered.flush(orderedSink).messagesSent == 1,
		"assignment replacement sends B2 before B1 ACK arrives");
	const CoopTacticalBaseline b2 = DecodeBaseline(orderedSink.messages.back());
	const CoopTacticalBaselineAckBytes b2Ack = BaselineAckBytes(b2, peer);
	CHECK(ordered.acknowledgeBaseline(peer, b1Ack.data(), b1Ack.size()) ==
			FullEngineCoopServerSessionResult::Success &&
		ordered.peerPhase(peer) == CoopTacticalPeerPhase::AwaitingBaselineAck,
		"late B1 ACK promotes committed evidence without activating B2");
	const CoopTacticalResyncRequestBytes b2Rejected = ResyncBytes(b1.state, 1,
		b1.baselineId, 0, b1.payloadChecksum,
		CoopTacticalResyncReason::BaselineRejected);
	CHECK(ordered.requestResync(peer, b2Rejected.data(), b2Rejected.size()) ==
			FullEngineCoopServerSessionResult::Success &&
		ordered.peerPhase(peer) == CoopTacticalPeerPhase::ResyncRequired &&
		ordered.acknowledgeBaseline(peer, b2Ack.data(), b2Ack.size()) ==
			FullEngineCoopServerSessionResult::UnexpectedAcknowledgement,
		"B2 rejection validates precisely against late-committed B1");
}
}

int main()
{
	TestConfigurationLifecycleAndAssignments();
	TestBaselineAckGatingAndCatchup();
	TestDeltaRingForcesResync();
	TestCumulativeDeltaAckAndReceiptOrdering();
	TestReceiptReplayAndCapacity();
	TestCommittedPeerRetirementFreesCapacityExactly();
	TestFlushReentrancyGuardAndSequenceExhaustion();
	TestDeltaTurnIdentity();
	TestAuthenticatedResyncPreservesAuthorityState();
	TestEqualRevisionLateBaselineAckCannotRegressLineage();
	if (failures == 0)
		std::printf("all full-engine coop server session tests passed\n");
	return failures == 0 ? 0 : 1;
}
