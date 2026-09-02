#include "FullEngineCoopCampaignSyncServer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); } } while (0)

static_assert(MaximumFullEngineCoopCampaignSyncPeers == 4);
static_assert(MaximumCoopCampaignSyncChunkWindow == 3);
static_assert(noexcept(std::declval<FullEngineCoopCampaignSyncServer&>().
	flushOutbound()));
static_assert(noexcept(std::declval<FullEngineCoopCampaignSyncServer&>().
	reconcilePeers(nullptr, 0)));

PeerIdentity Peer(std::uint8_t seed)
{
	PeerIdentity peer{};
	for (std::size_t index = 0; index < peer.size(); ++index)
		peer[index] = static_cast<std::uint8_t>(seed + index);
	return peer;
}

TransportPeer Transport(std::uint64_t value)
{
	return TransportPeer{value};
}

template<typename Digest>
Digest DigestFrom(std::uint8_t seed)
{
	Digest digest{};
	for (std::size_t index = 0; index < digest.size(); ++index)
		digest[index] = static_cast<std::uint8_t>(seed + index);
	return digest;
}

struct ReadCall
{
	CoopCampaignCheckpointSha256 expectedSha{};
	std::uint64_t offset = 0;
	std::size_t size = 0;
};

class MemoryCheckpointSource final :
	public FullEngineCoopCampaignCheckpointSource
{
public:
	explicit MemoryCheckpointSource(std::size_t size = 3,
		std::uint64_t generation = 1,
		std::uint8_t byteSeed = 0x20)
		: bytes(size)
	{
		metadataValue.campaignSeed = UINT64_C(0x8899aabbccddeeff);
		CHECK(ComputeCoopCampaignIdentitySha256("shared_01",
			metadataValue.campaignSeed,
			metadataValue.campaignIdentitySha256),
			"source campaign identity fixture hashes");
		metadataValue.checkpointGeneration = generation;
		metadataValue.totalSize = size;
		metadataValue.checkpointSha256 =
			DigestFrom<CoopCampaignCheckpointSha256>(
				static_cast<std::uint8_t>(0x40 + generation));
		metadataValue.worldMinutes = 1234 + generation;
		for (std::size_t index = 0; index < bytes.size(); ++index)
			bytes[index] = static_cast<std::uint8_t>(byteSeed + index * 17u);
	}

	bool metadata(
		FullEngineCoopCampaignCheckpointMetadata& output) const noexcept override
	{
		++metadataCalls;
		if (!metadataAvailable) return false;
		output = metadataValue;
		if (metadataCalls == descriptorMismatchMetadataCall)
			output.checkpointSha256[0] ^= 1;
		return true;
	}

	FullEngineCoopCampaignCheckpointReadResult readExact(
		const CoopCampaignCheckpointSha256& expectedCheckpointSha256,
		std::uint64_t offset,
		std::uint8_t* output,
		std::size_t size) noexcept override
	{
		if (readCallCount < readCalls.size())
			readCalls[readCallCount++] =
				ReadCall{expectedCheckpointSha256, offset, size};
		if (forcedReadResult !=
			FullEngineCoopCampaignCheckpointReadResult::Success)
			return forcedReadResult;
		if (expectedCheckpointSha256 !=
			metadataValue.checkpointSha256)
			return FullEngineCoopCampaignCheckpointReadResult::DescriptorMismatch;
		if (output == nullptr || size == 0 ||
			offset > bytes.size() || size > bytes.size() - offset)
			return FullEngineCoopCampaignCheckpointReadResult::Unavailable;
		std::copy(bytes.begin() + static_cast<std::size_t>(offset),
			bytes.begin() + static_cast<std::size_t>(offset) + size, output);
		return FullEngineCoopCampaignCheckpointReadResult::Success;
	}

	FullEngineCoopCampaignCheckpointMetadata metadataValue;
	std::vector<std::uint8_t> bytes;
	bool metadataAvailable = true;
	std::size_t descriptorMismatchMetadataCall =
		std::numeric_limits<std::size_t>::max();
	FullEngineCoopCampaignCheckpointReadResult forcedReadResult =
		FullEngineCoopCampaignCheckpointReadResult::Success;
	mutable std::size_t metadataCalls = 0;
	std::array<ReadCall, 64> readCalls{};
	std::size_t readCallCount = 0;
};

struct SendAttempt
{
	PeerIdentity peer{};
	TransportPeer transport;
	FullEngineCoopCampaignSyncOutboundKind kind =
		FullEngineCoopCampaignSyncOutboundKind::Metadata;
	std::string name;
	std::vector<std::uint8_t> bytes;
	bool accepted = false;
};

class RecordingWireSink final : public FullEngineCoopCampaignSyncWireSink
{
public:
	bool send(const PeerIdentity& peer,
		const TransportPeer& transport,
		FullEngineCoopCampaignSyncOutboundKind kind,
		const char* messageName,
		const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		const bool accepted = !failNext;
		failNext = false;
		try
		{
			attempts.push_back(SendAttempt{peer, transport, kind,
				messageName,
				std::vector<std::uint8_t>(bytes, bytes + size), accepted});
		}
		catch (...)
		{
			return false;
		}
		return accepted;
	}

	bool failNext = false;
	std::vector<SendAttempt> attempts;
};

struct Fixture
{
	explicit Fixture(std::size_t checkpointSize = 3,
		FullEngineCoopCampaignSyncServerConfiguration configuration = {})
		: source(checkpointSize), server(source, sink, configuration)
	{
	}

	MemoryCheckpointSource source;
	RecordingWireSink sink;
	FullEngineCoopCampaignSyncServer server;
};

FullEngineCoopCampaignSyncAuthenticatedPeer Authenticated(
	std::uint8_t peerSeed, std::uint64_t transport)
{
	return FullEngineCoopCampaignSyncAuthenticatedPeer{
		Peer(peerSeed), Transport(transport)};
}

CoopCampaignSyncMetadata DecodeMetadata(const SendAttempt& message)
{
	CoopCampaignSyncMetadata metadata;
	CHECK(message.accepted &&
		message.kind == FullEngineCoopCampaignSyncOutboundKind::Metadata &&
		message.name == CoopCampaignSyncMetadataMessageName,
		"captured message is accepted metadata");
	CHECK(DecodeCoopCampaignSyncMetadata(
		message.bytes.data(), message.bytes.size(), metadata) ==
		CoopCampaignSyncCodecResult::Success,
		"captured metadata decodes");
	return metadata;
}

CoopCampaignSyncChunk DecodeChunk(const SendAttempt& message)
{
	CoopCampaignSyncChunk chunk;
	CHECK(message.accepted &&
		message.kind == FullEngineCoopCampaignSyncOutboundKind::Chunk &&
		message.name == CoopCampaignSyncChunkMessageName,
		"captured message is accepted chunk");
	CHECK(DecodeCoopCampaignSyncChunk(
		message.bytes.data(), message.bytes.size(), chunk) ==
		CoopCampaignSyncCodecResult::Success,
		"captured chunk decodes");
	return chunk;
}

CoopCampaignSyncComplete DecodeComplete(const SendAttempt& message)
{
	CoopCampaignSyncComplete completion;
	CHECK(message.accepted &&
		message.kind == FullEngineCoopCampaignSyncOutboundKind::Complete &&
		message.name == CoopCampaignSyncCompleteMessageName,
		"captured message is accepted completion");
	CHECK(DecodeCoopCampaignSyncComplete(
		message.bytes.data(), message.bytes.size(), completion) ==
		CoopCampaignSyncCodecResult::Success,
		"captured completion decodes");
	return completion;
}

CoopCampaignSyncReject DecodeReject(const SendAttempt& message)
{
	CoopCampaignSyncReject rejection;
	CHECK(message.accepted &&
		message.kind == FullEngineCoopCampaignSyncOutboundKind::Reject &&
		message.name == CoopCampaignSyncRejectMessageName,
		"captured message is accepted reject");
	CHECK(DecodeCoopCampaignSyncReject(
		message.bytes.data(), message.bytes.size(), rejection) ==
		CoopCampaignSyncCodecResult::Success,
		"captured reject decodes");
	return rejection;
}

CoopCampaignSyncAck Ack(const CoopCampaignSyncTransferIdentity& transfer,
	const PeerIdentity& peer,
	std::uint64_t cursor,
	std::uint32_t checksum)
{
	CoopCampaignSyncAck acknowledgement;
	acknowledgement.transfer = transfer;
	acknowledgement.peerIdentity = peer;
	acknowledgement.nextExpectedOffset = cursor;
	acknowledgement.precedingChunkChecksum = checksum;
	return acknowledgement;
}

CoopCampaignSyncResult Result(
	const CoopCampaignSyncTransferIdentity& transfer,
	const PeerIdentity& peer,
	CoopCampaignSyncResultStatus status,
	CoopCampaignSyncFailureReason reason)
{
	CoopCampaignSyncResult result;
	result.transfer = transfer;
	result.peerIdentity = peer;
	result.status = status;
	result.reason = reason;
	return result;
}

CoopCampaignSyncResync Resync(
	const CoopCampaignSyncTransferIdentity& transfer,
	const PeerIdentity& peer,
	std::uint64_t cursor,
	std::uint32_t checksum)
{
	CoopCampaignSyncResync resync;
	resync.transfer = transfer;
	resync.peerIdentity = peer;
	resync.expectedOffset = cursor;
	resync.precedingChunkChecksum = checksum;
	resync.reason = CoopCampaignSyncFailureReason::SequenceMismatch;
	return resync;
}

CoopCampaignSyncMetadata BeginOne(Fixture& fixture,
	const FullEngineCoopCampaignSyncAuthenticatedPeer& authenticated,
	std::uint64_t epoch = 17)
{
	CHECK(fixture.server.beginSession(epoch) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"campaign sync session begins");
	CHECK(fixture.server.reconcilePeers(&authenticated, 1) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"authenticated peer reconciles");
	const FullEngineCoopCampaignSyncFlushResult flushed =
		fixture.server.flushOutbound();
	CHECK(flushed.result == FullEngineCoopCampaignSyncServerResult::Success &&
		flushed.messagesSent == 1,
		"initial metadata flush succeeds");
	return DecodeMetadata(fixture.sink.attempts.back());
}

void DriveTinyReady(Fixture& fixture,
	const FullEngineCoopCampaignSyncAuthenticatedPeer& authenticated,
	CoopCampaignSyncMetadata& metadata)
{
	metadata = BeginOne(fixture, authenticated);
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"initial ACK starts streaming");
	CHECK(fixture.server.flushOutbound().chunksSent == 1,
		"tiny checkpoint chunk sends");
	const CoopCampaignSyncChunk chunk =
		DecodeChunk(fixture.sink.attempts.back());
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity,
			chunk.offset + chunk.payload.size(), chunk.payloadChecksum)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"tiny checkpoint final ACK succeeds");
	CHECK(fixture.server.flushOutbound().messagesSent == 1,
		"completion sends");
	DecodeComplete(fixture.sink.attempts.back());
	CHECK(fixture.server.handleResult(authenticated.peerIdentity,
		authenticated.transport,
		Result(metadata.transfer, authenticated.peerIdentity,
			CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"exact committed result makes campaign ready");
}

void TestConfigurationAndPeerReconciliation()
{
	FullEngineCoopCampaignSyncServerConfiguration invalidConfiguration;
	invalidConfiguration.maximumTransferId = 0;
	Fixture invalid(3, invalidConfiguration);
	CHECK(invalid.server.beginSession(1) ==
		FullEngineCoopCampaignSyncServerResult::InvalidConfiguration,
		"zero transfer-id ceiling is invalid");

	MemoryCheckpointSource unavailable;
	unavailable.metadataAvailable = false;
	RecordingWireSink unavailableSink;
	FullEngineCoopCampaignSyncServer unavailableServer(
		unavailable, unavailableSink);
	CHECK(unavailableServer.beginSession(1) ==
		FullEngineCoopCampaignSyncServerResult::SourceUnavailable,
		"missing checkpoint metadata fails begin");

	MemoryCheckpointSource malformed;
	malformed.metadataValue.checkpointGeneration = 0;
	RecordingWireSink malformedSink;
	FullEngineCoopCampaignSyncServer malformedServer(malformed, malformedSink);
	CHECK(malformedServer.beginSession(1) ==
		FullEngineCoopCampaignSyncServerResult::InvalidCheckpoint,
		"invalid source descriptor fails begin");

	Fixture fixture;
	CHECK(fixture.server.beginSession(0) ==
		FullEngineCoopCampaignSyncServerResult::InvalidSessionEpoch,
		"zero admission epoch is rejected");
	CHECK(fixture.server.beginSession(9) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"valid session begins");

	std::array<FullEngineCoopCampaignSyncAuthenticatedPeer, 2> unsorted{{
		Authenticated(0x30, 2), Authenticated(0x10, 1)}};
	CHECK(fixture.server.reconcilePeers(unsorted.data(), unsorted.size()) ==
		FullEngineCoopCampaignSyncServerResult::InvalidPeerSet,
		"peer input must be strictly sorted");
	std::array<FullEngineCoopCampaignSyncAuthenticatedPeer, 2> duplicateTransport{{
		Authenticated(0x10, 1), Authenticated(0x30, 1)}};
	CHECK(fixture.server.reconcilePeers(
		duplicateTransport.data(), duplicateTransport.size()) ==
		FullEngineCoopCampaignSyncServerResult::InvalidPeerSet,
		"one connection cannot bind two identities");
	std::array<FullEngineCoopCampaignSyncAuthenticatedPeer, 5> tooMany{{
		Authenticated(0x10, 1), Authenticated(0x20, 2),
		Authenticated(0x30, 3), Authenticated(0x40, 4),
		Authenticated(0x50, 5)}};
	CHECK(fixture.server.reconcilePeers(tooMany.data(), tooMany.size()) ==
		FullEngineCoopCampaignSyncServerResult::PeerCapacityReached,
		"fifth authenticated peer is rejected");

	std::array<FullEngineCoopCampaignSyncAuthenticatedPeer, 4> peers{{
		Authenticated(0x10, 1), Authenticated(0x20, 2),
		Authenticated(0x30, 3), Authenticated(0x40, 4)}};
	CHECK(fixture.server.reconcilePeers(peers.data(), peers.size()) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"exact four-peer bound reconciles");
	for (std::size_t index = 0; index < peers.size(); ++index)
	{
		FullEngineCoopCampaignSyncPeerDiagnostics diagnostics;
		CHECK(fixture.server.peerDiagnostics(
			peers[index].peerIdentity, diagnostics) &&
			diagnostics.transferId == index + 1 &&
			diagnostics.phase ==
				FullEngineCoopCampaignSyncPeerPhase::MetadataPending,
			"new peers receive sorted monotonic transfers");
	}
	const FullEngineCoopCampaignSyncServerDiagnostics before =
		fixture.server.diagnostics();
	CHECK(fixture.server.reconcilePeers(peers.data(), peers.size()) ==
		FullEngineCoopCampaignSyncServerResult::Success &&
		fixture.server.diagnostics().nextTransferId == before.nextTransferId,
		"unchanged peer/transport set preserves transfers");
	CHECK(fixture.server.endSession() ==
		FullEngineCoopCampaignSyncServerResult::Success &&
		!fixture.server.active(), "session ends and clears state");
}

void TestWindowPartialAckAndReady()
{
	const std::size_t chunkBytes = CoopCampaignSyncCanonicalChunkBytes;
	Fixture fixture(chunkBytes * 3 + 7);
	const FullEngineCoopCampaignSyncAuthenticatedPeer authenticated =
		Authenticated(0x10, 101);
	const CoopCampaignSyncMetadata metadata = BeginOne(fixture, authenticated);
	CHECK(metadata.transfer.sessionEpoch == 17 &&
		metadata.transfer.transferId == 1 &&
		metadata.transfer.totalSize == fixture.source.bytes.size() &&
		metadata.transfer.campaignSeed ==
			fixture.source.metadataValue.campaignSeed &&
		metadata.worldMinutes == fixture.source.metadataValue.worldMinutes,
		"metadata binds exact session, campaign, and checkpoint");
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"exact ACK(0) starts streaming");

	const std::size_t firstChunkAttempt = fixture.sink.attempts.size();
	FullEngineCoopCampaignSyncFlushResult flushed =
		fixture.server.flushOutbound();
	CHECK(flushed.result == FullEngineCoopCampaignSyncServerResult::Success &&
		flushed.chunksSent == 3 && flushed.messagesSent == 3,
		"server pipelines exactly the three-chunk window");
	std::array<CoopCampaignSyncChunk, 3> chunks;
	for (std::size_t index = 0; index < chunks.size(); ++index)
	{
		chunks[index] = DecodeChunk(
			fixture.sink.attempts[firstChunkAttempt + index]);
		CHECK(chunks[index].offset == index * chunkBytes &&
			chunks[index].payload.size() == chunkBytes,
			"window chunks are sequential and canonical");
		CHECK(std::equal(chunks[index].payload.begin(),
			chunks[index].payload.end(),
			fixture.source.bytes.begin() + chunks[index].offset),
			"chunk bytes exactly match immutable source");
	}
	FullEngineCoopCampaignSyncPeerDiagnostics diagnostics;
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, diagnostics) &&
		diagnostics.inFlightChunks == 3 &&
		diagnostics.highestSentOffset == chunkBytes * 3,
		"diagnostics expose full in-flight window");
	CHECK(fixture.server.flushOutbound().messagesSent == 0,
		"full window backpressures additional source reads");

	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity, chunkBytes,
			chunks[0].payloadChecksum)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"partial cumulative ACK retires first chunk");
	flushed = fixture.server.flushOutbound();
	CHECK(flushed.chunksSent == 1 && flushed.messagesSent == 1,
		"partial ACK opens exactly one window slot");
	const CoopCampaignSyncChunk finalChunk =
		DecodeChunk(fixture.sink.attempts.back());
	CHECK(finalChunk.offset == chunkBytes * 3 &&
		finalChunk.payload.size() == 7,
		"final chunk has exact canonical remainder");
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity,
			metadata.transfer.totalSize, finalChunk.payloadChecksum)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"ACK(total) cumulatively retires the remaining window");
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, diagnostics) &&
		diagnostics.phase ==
			FullEngineCoopCampaignSyncPeerPhase::CompletePending &&
		diagnostics.inFlightChunks == 0 &&
		diagnostics.acknowledgedOffset == metadata.transfer.totalSize,
		"ACK(total) stages completion without early readiness");
	flushed = fixture.server.flushOutbound();
	CHECK(flushed.messagesSent == 1 && flushed.chunksSent == 0,
		"completion sends after all bytes are cumulatively ACKed");
	const CoopCampaignSyncComplete completion =
		DecodeComplete(fixture.sink.attempts.back());
	CHECK(SameCoopCampaignSyncTransfer(
		completion.transfer, metadata.transfer),
		"completion echoes exact immutable transfer");

	CHECK(fixture.server.handleResult(authenticated.peerIdentity,
		authenticated.transport,
		Result(metadata.transfer, authenticated.peerIdentity,
			CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"post-load committed result makes peer ready");
	std::array<PeerIdentity, MaximumFullEngineCoopCampaignSyncPeers> ready{};
	CHECK(fixture.server.readyPeers(ready) == 1 &&
		ready[0] == authenticated.peerIdentity,
		"ready-peer enumeration exposes exact sorted current peer");
	CHECK(fixture.server.handleResult(authenticated.peerIdentity,
		authenticated.transport,
		Result(metadata.transfer, authenticated.peerIdentity,
			CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"duplicate exact committed result is idempotent");
	CHECK(fixture.source.readCallCount == 4,
		"source is read exactly once per canonical chunk");
	for (std::size_t index = 0; index < fixture.source.readCallCount; ++index)
		CHECK(fixture.source.readCalls[index].size != 0 &&
			fixture.source.readCalls[index].size <=
				CoopCampaignSyncCanonicalChunkBytes &&
			fixture.source.readCalls[index].expectedSha ==
				fixture.source.metadataValue.checkpointSha256,
			"every source read is bounded and descriptor-bound");
}

void TestExactBackpressureRetention()
{
	FullEngineCoopCampaignSyncServerConfiguration configuration;
	configuration.maximumMessagesPerFlush = 1;
	Fixture fixture(CoopCampaignSyncCanonicalChunkBytes + 3, configuration);
	const auto authenticated = Authenticated(0x10, 201);
	CHECK(fixture.server.beginSession(19) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"backpressure session begins");
	CHECK(fixture.server.reconcilePeers(&authenticated, 1) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"backpressure peer reconciles");
	fixture.sink.failNext = true;
	FullEngineCoopCampaignSyncFlushResult flushed =
		fixture.server.flushOutbound();
	CHECK(flushed.result ==
			FullEngineCoopCampaignSyncServerResult::TransportBackpressured &&
		flushed.backpressured && flushed.messagesSent == 0,
		"metadata sink refusal applies backpressure");
	FullEngineCoopCampaignSyncPeerDiagnostics diagnostics;
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, diagnostics) &&
		diagnostics.phase ==
			FullEngineCoopCampaignSyncPeerPhase::MetadataPending &&
		diagnostics.pendingKind ==
			FullEngineCoopCampaignSyncPendingKind::Metadata,
		"metadata state does not advance on backpressure");
	flushed = fixture.server.flushOutbound();
	CHECK(flushed.result == FullEngineCoopCampaignSyncServerResult::Success &&
		flushed.messagesSent == 1 &&
		fixture.sink.attempts[0].bytes == fixture.sink.attempts[1].bytes,
		"metadata retry preserves exact encoded bytes");
	const CoopCampaignSyncMetadata metadata =
		DecodeMetadata(fixture.sink.attempts[1]);
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"backpressure fixture starts streaming");

	fixture.sink.failNext = true;
	const std::size_t attemptsBefore = fixture.sink.attempts.size();
	flushed = fixture.server.flushOutbound();
	CHECK(flushed.backpressured && fixture.source.readCallCount == 1,
		"chunk is read once before refused enqueue");
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, diagnostics) &&
		diagnostics.highestSentOffset == 0 &&
		diagnostics.inFlightChunks == 0 &&
		diagnostics.pendingKind ==
			FullEngineCoopCampaignSyncPendingKind::Chunk,
		"refused chunk remains pending and unsent");
	flushed = fixture.server.flushOutbound();
	CHECK(flushed.messagesSent == 1 && flushed.chunksSent == 1 &&
		fixture.source.readCallCount == 1 &&
		fixture.sink.attempts[attemptsBefore].bytes ==
			fixture.sink.attempts[attemptsBefore + 1].bytes,
		"chunk retry neither rereads nor re-encodes pending bytes");
}

void TestResyncRetransmission()
{
	const std::size_t chunkBytes = CoopCampaignSyncCanonicalChunkBytes;
	Fixture fixture(chunkBytes * 4 + 3);
	const auto authenticated = Authenticated(0x10, 301);
	const CoopCampaignSyncMetadata metadata = BeginOne(fixture, authenticated);
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"resync fixture starts streaming");
	const std::size_t originalStart = fixture.sink.attempts.size();
	CHECK(fixture.server.flushOutbound().chunksSent == 3,
		"resync fixture fills initial window");
	const CoopCampaignSyncChunk first =
		DecodeChunk(fixture.sink.attempts[originalStart]);
	const CoopCampaignSyncChunk second =
		DecodeChunk(fixture.sink.attempts[originalStart + 1]);
	const CoopCampaignSyncChunk third =
		DecodeChunk(fixture.sink.attempts[originalStart + 2]);

	CHECK(fixture.server.handleResync(authenticated.peerIdentity,
		authenticated.transport,
		Resync(metadata.transfer, authenticated.peerIdentity,
			first.offset + first.payload.size(), first.payloadChecksum)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"exact resync cursor cumulatively accepts its prefix");
	FullEngineCoopCampaignSyncPeerDiagnostics diagnostics;
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, diagnostics) &&
		diagnostics.acknowledgedOffset == chunkBytes &&
		diagnostics.highestSentOffset == chunkBytes &&
		diagnostics.inFlightChunks == 0,
		"resync discards all later in-flight state");
	const std::size_t replayStart = fixture.sink.attempts.size();
	CHECK(fixture.server.flushOutbound().chunksSent == 3,
		"resync retransmits a fresh bounded window");
	const CoopCampaignSyncChunk replaySecond =
		DecodeChunk(fixture.sink.attempts[replayStart]);
	const CoopCampaignSyncChunk replayThird =
		DecodeChunk(fixture.sink.attempts[replayStart + 1]);
	CHECK(replaySecond.offset == second.offset &&
		replaySecond.payload == second.payload &&
		replaySecond.payloadChecksum == second.payloadChecksum &&
		replayThird.offset == third.offset &&
		replayThird.payload == third.payload,
		"retransmission restarts at exact requested offset");

	const FullEngineCoopCampaignSyncPeerDiagnostics before = diagnostics;
	CoopCampaignSyncResync wrongChecksum = Resync(metadata.transfer,
		authenticated.peerIdentity, chunkBytes, first.payloadChecksum ^ 1u);
	CHECK(fixture.server.handleResync(authenticated.peerIdentity,
		authenticated.transport, wrongChecksum) ==
		FullEngineCoopCampaignSyncServerResult::IntegrityMismatch,
		"resync with wrong preceding checksum is rejected");
	CHECK(fixture.server.handleResync(authenticated.peerIdentity,
		authenticated.transport,
		Resync(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::SequenceMismatch,
		"resync cannot roll the server cursor backward");
	FullEngineCoopCampaignSyncPeerDiagnostics after;
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, after) &&
		after.acknowledgedOffset == before.acknowledgedOffset,
		"invalid resync never advances or rolls back ACK state");
}

void TestSpoofingStaleAndMalformedFrames()
{
	Fixture fixture(CoopCampaignSyncCanonicalChunkBytes * 2 + 3);
	const auto authenticated = Authenticated(0x10, 401);
	const CoopCampaignSyncMetadata metadata = BeginOne(fixture, authenticated);
	FullEngineCoopCampaignSyncPeerDiagnostics before;
	CHECK(fixture.server.peerDiagnostics(authenticated.peerIdentity, before),
		"spoof fixture diagnostics exist");

	CoopCampaignSyncAck forged = Ack(
		metadata.transfer, Peer(0x70), 0, 0);
	CoopCampaignSyncAckBytes forgedBytes{};
	CHECK(EncodeCoopCampaignSyncAck(forged, forgedBytes) ==
		CoopCampaignSyncCodecResult::Success,
		"forged claimed identity is structurally valid");
	CHECK(fixture.server.handleInbound(authenticated.peerIdentity,
		authenticated.transport,
		FullEngineCoopCampaignSyncInboundKind::Ack,
		forgedBytes.data(), forgedBytes.size()) ==
		FullEngineCoopCampaignSyncServerResult::ClaimedIdentityMismatch,
		"payload identity never replaces transport-resolved identity");
	CHECK(fixture.server.handleInbound(authenticated.peerIdentity,
		authenticated.transport,
		FullEngineCoopCampaignSyncInboundKind::Ack,
		forgedBytes.data(), forgedBytes.size() - 1) ==
		FullEngineCoopCampaignSyncServerResult::MalformedFrame,
		"truncated raw campaign frame is rejected");
	CHECK(fixture.server.handleAck(authenticated.peerIdentity, Transport(999),
		Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::StaleTransport,
		"old connection generation cannot control peer");
	CHECK(fixture.server.handleAck(Peer(0x60), authenticated.transport,
		Ack(metadata.transfer, Peer(0x60), 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::InvalidPeer,
		"unknown authenticated peer is rejected");
	CoopCampaignSyncAck stale = Ack(
		metadata.transfer, authenticated.peerIdentity, 0, 0);
	stale.transfer.transferId++;
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport, stale) ==
		FullEngineCoopCampaignSyncServerResult::StaleTransfer,
		"stale transfer frame is rejected");
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity,
			metadata.transfer.totalSize, 9)) ==
		FullEngineCoopCampaignSyncServerResult::SequenceMismatch,
		"future initial ACK cannot skip checkpoint bytes");

	FullEngineCoopCampaignSyncPeerDiagnostics after;
	CHECK(fixture.server.peerDiagnostics(authenticated.peerIdentity, after) &&
		after.phase == before.phase &&
		after.acknowledgedOffset == before.acknowledgedOffset &&
		after.transferId == before.transferId,
		"spoofed, malformed, stale, and future frames never advance state");

	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"valid initial ACK still works after adversarial frames");
	CHECK(fixture.server.flushOutbound().chunksSent == 3,
		"all three available chunks enter bounded flight");
	const CoopCampaignSyncChunk first =
		DecodeChunk(fixture.sink.attempts[fixture.sink.attempts.size() - 3]);
	const CoopCampaignSyncChunk finalChunk =
		DecodeChunk(fixture.sink.attempts.back());
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity,
			first.offset + first.payload.size(),
			first.payloadChecksum ^ 1u)) ==
		FullEngineCoopCampaignSyncServerResult::IntegrityMismatch,
		"wrong ACK checksum is rejected");
	CHECK(fixture.server.handleAck(authenticated.peerIdentity,
		authenticated.transport,
		Ack(metadata.transfer, authenticated.peerIdentity,
			finalChunk.offset + finalChunk.payload.size(),
			finalChunk.payloadChecksum)) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"later exact cumulative ACK remains usable");
}

void TestReconnectAndSupersession()
{
	Fixture fixture(3);
	auto authenticated = Authenticated(0x10, 501);
	CoopCampaignSyncMetadata firstMetadata;
	DriveTinyReady(fixture, authenticated, firstMetadata);
	std::array<PeerIdentity, MaximumFullEngineCoopCampaignSyncPeers> ready{};
	CHECK(fixture.server.readyPeers(ready) == 1,
		"ready fixture reached observable ready state");

	CHECK(fixture.server.reconcilePeers(&authenticated, 1) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"unchanged live connection reconciles");
	FullEngineCoopCampaignSyncPeerDiagnostics diagnostics;
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, diagnostics) &&
		diagnostics.transferId == firstMetadata.transfer.transferId &&
		diagnostics.campaignReady,
		"unchanged transport preserves ready transfer");

	const TransportPeer oldTransport = authenticated.transport;
	authenticated.transport = Transport(502);
	CHECK(fixture.server.reconcilePeers(&authenticated, 1) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"same identity on new transport is a reconnect");
	CHECK(fixture.server.peerDiagnostics(
		authenticated.peerIdentity, diagnostics) &&
		diagnostics.transferId == firstMetadata.transfer.transferId + 1 &&
		!diagnostics.campaignReady &&
		diagnostics.phase ==
			FullEngineCoopCampaignSyncPeerPhase::MetadataPending,
		"reconnect loses readiness and allocates fresh transfer");
	CHECK(fixture.server.handleResult(authenticated.peerIdentity, oldTransport,
		Result(firstMetadata.transfer, authenticated.peerIdentity,
			CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None)) ==
		FullEngineCoopCampaignSyncServerResult::StaleTransport,
		"displaced transport cannot replay committed result");
	CHECK(fixture.server.handleResult(authenticated.peerIdentity,
		authenticated.transport,
		Result(firstMetadata.transfer, authenticated.peerIdentity,
			CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None)) ==
		FullEngineCoopCampaignSyncServerResult::StaleTransfer,
		"new transport cannot reuse prior transfer");
	CHECK(fixture.server.flushOutbound().messagesSent == 1,
		"reconnect receives new metadata");
	const CoopCampaignSyncMetadata reconnectMetadata =
		DecodeMetadata(fixture.sink.attempts.back());
	CHECK(reconnectMetadata.transfer.transferId ==
		firstMetadata.transfer.transferId + 1,
		"reconnect metadata carries new monotonic transfer");

	MemoryCheckpointSource newer(5, 2, 0x70);
	CHECK(fixture.server.supersedeCheckpoint(newer) ==
		FullEngineCoopCampaignSyncServerResult::Success,
		"strictly newer same-campaign checkpoint supersedes");
	CHECK(fixture.server.diagnostics().checkpointGeneration == 2 &&
		fixture.server.readyPeers(ready) == 0 &&
		fixture.server.peerDiagnostics(
			authenticated.peerIdentity, diagnostics) &&
		diagnostics.transferId == reconnectMetadata.transfer.transferId + 1 &&
		diagnostics.phase ==
			FullEngineCoopCampaignSyncPeerPhase::MetadataPending,
		"supersession invalidates readiness and stages fresh transfer");
	CHECK(fixture.server.handleResult(authenticated.peerIdentity,
		authenticated.transport,
		Result(reconnectMetadata.transfer, authenticated.peerIdentity,
			CoopCampaignSyncResultStatus::Committed,
			CoopCampaignSyncFailureReason::None)) ==
		FullEngineCoopCampaignSyncServerResult::StaleTransfer,
		"superseded transfer cannot restore readiness");
	CHECK(fixture.server.supersedeCheckpoint(newer) ==
		FullEngineCoopCampaignSyncServerResult::CheckpointNotNewer,
		"identical checkpoint is not spuriously retransferred");
}

void TestReadySupersessionAndSourceRecheck()
{
	{
		Fixture fixture(3);
		const auto authenticated = Authenticated(0x10, 551);
		CoopCampaignSyncMetadata original;
		DriveTinyReady(fixture, authenticated, original);
		std::array<PeerIdentity,
			MaximumFullEngineCoopCampaignSyncPeers> ready{};
		CHECK(fixture.server.readyPeers(ready) == 1,
			"supersession fixture begins ready");
		MemoryCheckpointSource newer(5, 2, 0x71);
		CHECK(fixture.server.supersedeCheckpoint(newer) ==
			FullEngineCoopCampaignSyncServerResult::Success,
			"newer immutable checkpoint supersedes ready peer");
		FullEngineCoopCampaignSyncPeerDiagnostics diagnostics;
		CHECK(fixture.server.readyPeers(ready) == 0 &&
			fixture.server.peerDiagnostics(
				authenticated.peerIdentity, diagnostics) &&
			diagnostics.phase ==
				FullEngineCoopCampaignSyncPeerPhase::MetadataPending &&
			diagnostics.transferId == original.transfer.transferId + 1,
			"supersession atomically removes readiness and allocates transfer");
		CHECK(fixture.server.flushOutbound().messagesSent == 1,
			"superseded ready peer receives fresh metadata");
		const CoopCampaignSyncMetadata refreshed =
			DecodeMetadata(fixture.sink.attempts.back());
		CHECK(refreshed.transfer.checkpointGeneration == 2 &&
			refreshed.transfer.checkpointSha256 ==
				newer.metadataValue.checkpointSha256,
			"fresh metadata names the newer checkpoint exactly");
	}

	{
		Fixture fixture(3);
		const auto authenticated = Authenticated(0x20, 552);
		CoopCampaignSyncMetadata metadata;
		DriveTinyReady(fixture, authenticated, metadata);
		fixture.source.metadataAvailable = false;
		const FullEngineCoopCampaignSyncFlushResult flushed =
			fixture.server.flushOutbound();
		std::array<PeerIdentity,
			MaximumFullEngineCoopCampaignSyncPeers> ready{};
		CHECK(flushed.result ==
				FullEngineCoopCampaignSyncServerResult::SourceUnavailable &&
			fixture.server.terminal() &&
			fixture.server.readyPeers(ready) == 0,
			"source loss globally revokes already-published readiness");
	}

	{
		Fixture fixture(3);
		const auto authenticated = Authenticated(0x30, 553);
		const CoopCampaignSyncMetadata metadata =
			BeginOne(fixture, authenticated);
		CHECK(fixture.server.handleAck(authenticated.peerIdentity,
			authenticated.transport,
			Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
			FullEngineCoopCampaignSyncServerResult::Success,
			"adjacent source-recheck fixture starts streaming");
		fixture.source.descriptorMismatchMetadataCall =
			fixture.source.metadataCalls + 2;
		const FullEngineCoopCampaignSyncFlushResult flushed =
			fixture.server.flushOutbound();
		CHECK(flushed.result ==
				FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch &&
			fixture.source.readCallCount == 0 && fixture.server.terminal(),
			"metadata is rechecked immediately before read, after flush precheck");
	}
}

enum class RejectionPhase
{
	AwaitingInitialAck,
	Streaming,
	CompletePending,
	AwaitingResult
};

void CheckClientRejectionAtPhase(RejectionPhase target,
	CoopCampaignSyncFailureReason reason)
{
	FullEngineCoopCampaignSyncServerConfiguration configuration;
	configuration.maximumMessagesPerFlush = 1;
	Fixture fixture(3, configuration);
	const auto authenticated = Authenticated(
		static_cast<std::uint8_t>(0x10 + static_cast<int>(target) * 0x10),
		601 + static_cast<std::uint64_t>(target));
	const CoopCampaignSyncMetadata metadata = BeginOne(fixture, authenticated);
	if (target != RejectionPhase::AwaitingInitialAck)
	{
		CHECK(fixture.server.handleAck(authenticated.peerIdentity,
			authenticated.transport,
			Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
			FullEngineCoopCampaignSyncServerResult::Success,
			"rejection fixture starts streaming");
	}
	CoopCampaignSyncChunk chunk;
	if (target == RejectionPhase::CompletePending ||
		target == RejectionPhase::AwaitingResult)
	{
		CHECK(fixture.server.flushOutbound().chunksSent == 1,
			"rejection fixture sends its chunk");
		chunk = DecodeChunk(fixture.sink.attempts.back());
		CHECK(fixture.server.handleAck(authenticated.peerIdentity,
			authenticated.transport,
			Ack(metadata.transfer, authenticated.peerIdentity,
				chunk.offset + chunk.payload.size(), chunk.payloadChecksum)) ==
			FullEngineCoopCampaignSyncServerResult::Success,
			"rejection fixture reaches completion-pending");
	}
	if (target == RejectionPhase::AwaitingResult)
	{
		CHECK(fixture.server.flushOutbound().messagesSent == 1,
			"rejection fixture sends completion");
		DecodeComplete(fixture.sink.attempts.back());
	}

	if (target != RejectionPhase::AwaitingResult)
	{
		CHECK(fixture.server.handleResult(authenticated.peerIdentity,
			authenticated.transport,
			Result(metadata.transfer, authenticated.peerIdentity,
				CoopCampaignSyncResultStatus::Committed,
				CoopCampaignSyncFailureReason::None)) ==
			FullEngineCoopCampaignSyncServerResult::UnexpectedFrame,
			"committed result is illegal before completion was sent");
	}
	FullEngineCoopCampaignSyncPeerDiagnostics before;
	CHECK(fixture.server.peerDiagnostics(authenticated.peerIdentity, before),
		"rejection phase diagnostics exist");
	const CoopCampaignSyncResult rejected = Result(metadata.transfer,
		authenticated.peerIdentity,
		CoopCampaignSyncResultStatus::Rejected, reason);
	CHECK(fixture.server.handleResult(authenticated.peerIdentity,
		authenticated.transport, rejected) ==
		FullEngineCoopCampaignSyncServerResult::ClientRejected,
		"exact current client rejection is terminally accepted");
	FullEngineCoopCampaignSyncPeerDiagnostics after;
	CHECK(fixture.server.peerDiagnostics(authenticated.peerIdentity, after) &&
		after.phase == FullEngineCoopCampaignSyncPeerPhase::RejectPending &&
		after.pendingKind ==
			FullEngineCoopCampaignSyncPendingKind::Reject &&
		!after.campaignReady &&
		after.acknowledgedOffset == before.acknowledgedOffset,
		"client rejection stages reject without fabricating ACK progress");
	CHECK(fixture.server.flushOutbound().messagesSent == 1,
		"terminal server reject echo sends");
	const CoopCampaignSyncReject wireReject =
		DecodeReject(fixture.sink.attempts.back());
	CHECK(wireReject.reason == reason &&
		SameCoopCampaignSyncTransfer(
			wireReject.transfer, metadata.transfer),
		"server reject echoes exact transfer and client failure reason");
	CHECK(fixture.server.handleResult(authenticated.peerIdentity,
		authenticated.transport, rejected) ==
		FullEngineCoopCampaignSyncServerResult::ClientRejected,
		"duplicate exact client rejection is idempotent");
}

void TestEarlyClientRejections()
{
	CheckClientRejectionAtPhase(RejectionPhase::AwaitingInitialAck,
		CoopCampaignSyncFailureReason::StorageFailure);
	CheckClientRejectionAtPhase(RejectionPhase::Streaming,
		CoopCampaignSyncFailureReason::CapacityReached);
	CheckClientRejectionAtPhase(RejectionPhase::CompletePending,
		CoopCampaignSyncFailureReason::HashMismatch);
	CheckClientRejectionAtPhase(RejectionPhase::AwaitingResult,
		CoopCampaignSyncFailureReason::LoadFailed);
}

void TestSourceFailuresAndTransferExhaustion()
{
	{
		Fixture fixture;
		const auto authenticated = Authenticated(0x10, 701);
		CHECK(fixture.server.beginSession(23) ==
			FullEngineCoopCampaignSyncServerResult::Success &&
			fixture.server.reconcilePeers(&authenticated, 1) ==
				FullEngineCoopCampaignSyncServerResult::Success,
			"descriptor mismatch fixture begins");
		fixture.source.metadataValue.checkpointSha256[0] ^= 1;
		const FullEngineCoopCampaignSyncFlushResult flushed =
			fixture.server.flushOutbound();
		CHECK(flushed.result ==
				FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch &&
			fixture.server.terminal() &&
			fixture.server.diagnostics().terminalCause ==
				FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch,
			"changed immutable source descriptor fails globally closed");
	}
	for (FullEngineCoopCampaignCheckpointReadResult failure : {
		FullEngineCoopCampaignCheckpointReadResult::Unavailable,
		FullEngineCoopCampaignCheckpointReadResult::DescriptorMismatch})
	{
		Fixture fixture;
		const auto authenticated = Authenticated(0x20, 702);
		const CoopCampaignSyncMetadata metadata = BeginOne(fixture, authenticated);
		CHECK(fixture.server.handleAck(authenticated.peerIdentity,
			authenticated.transport,
			Ack(metadata.transfer, authenticated.peerIdentity, 0, 0)) ==
			FullEngineCoopCampaignSyncServerResult::Success,
			"source-read failure fixture starts streaming");
		fixture.source.forcedReadResult = failure;
		const FullEngineCoopCampaignSyncFlushResult flushed =
			fixture.server.flushOutbound();
		const FullEngineCoopCampaignSyncServerResult expected = failure ==
			FullEngineCoopCampaignCheckpointReadResult::DescriptorMismatch
			? FullEngineCoopCampaignSyncServerResult::SourceDescriptorMismatch
			: FullEngineCoopCampaignSyncServerResult::SourceUnavailable;
		std::array<PeerIdentity,
			MaximumFullEngineCoopCampaignSyncPeers> ready{};
		CHECK(flushed.result == expected && fixture.server.terminal() &&
			fixture.server.readyPeers(ready) == 0,
			"source read failure clears all readiness and becomes terminal");
	}

	{
		FullEngineCoopCampaignSyncServerConfiguration configuration;
		configuration.maximumTransferId = 1;
		Fixture fixture(3, configuration);
		auto authenticated = Authenticated(0x10, 703);
		CHECK(fixture.server.beginSession(25) ==
			FullEngineCoopCampaignSyncServerResult::Success &&
			fixture.server.reconcilePeers(&authenticated, 1) ==
				FullEngineCoopCampaignSyncServerResult::Success,
			"single-transfer ceiling admits first connection");
		authenticated.transport = Transport(704);
		CHECK(fixture.server.reconcilePeers(&authenticated, 1) ==
			FullEngineCoopCampaignSyncServerResult::TransferExhausted &&
			fixture.server.terminal() &&
			fixture.server.diagnostics().terminalCause ==
				FullEngineCoopCampaignSyncServerResult::TransferExhausted,
			"reconnect after transfer-id ceiling is terminal");
	}

	{
		FullEngineCoopCampaignSyncServerConfiguration configuration;
		configuration.maximumTransferId = 2;
		Fixture fixture(3, configuration);
		std::array<FullEngineCoopCampaignSyncAuthenticatedPeer, 2> peers{{
			Authenticated(0x10, 705), Authenticated(0x20, 706)}};
		CHECK(fixture.server.beginSession(27) ==
			FullEngineCoopCampaignSyncServerResult::Success &&
			fixture.server.reconcilePeers(peers.data(), peers.size()) ==
				FullEngineCoopCampaignSyncServerResult::Success,
			"two transfers consume configured ceiling");
		MemoryCheckpointSource newer(4, 2, 0x60);
		CHECK(fixture.server.supersedeCheckpoint(newer) ==
			FullEngineCoopCampaignSyncServerResult::TransferExhausted &&
			fixture.server.terminal() &&
			fixture.server.diagnostics().checkpointGeneration == 1,
			"supersession preflights all peer transfers without partial commit");
	}
}
}

int main()
{
	TestConfigurationAndPeerReconciliation();
	TestWindowPartialAckAndReady();
	TestExactBackpressureRetention();
	TestResyncRetransmission();
	TestSpoofingStaleAndMalformedFrames();
	TestReconnectAndSupersession();
	TestReadySupersessionAndSourceRecheck();
	TestEarlyClientRejections();
	TestSourceFailuresAndTransferExhaustion();
	if (failures != 0)
	{
		std::printf("%d full-engine co-op campaign sync server test(s) failed\n",
			failures);
		return 1;
	}
	std::puts("full-engine co-op campaign sync server tests passed");
	return 0;
}
