#include "CoopCampaignBootstrapProtocol.h"
#include "CoopCampaignSyncProtocol.h"
#include "FullEngineCoopCampaignSyncClient.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int Failures = 0;

void Expect(bool condition, const char* message)
{
	if (condition) return;
	std::fprintf(stderr, "FAIL: %s\n", message);
	++Failures;
}

CoopCampaignBootstrapDescriptor MakeBootstrap()
{
	CoopCampaignBootstrapDescriptor bootstrap;
	bootstrap.protocolVersion = CurrentProtocolVersion;
	bootstrap.sessionEpoch = 91;
	bootstrap.campaignSeed = 0;
	Expect(ComputeCoopCampaignIdentitySha256(
		"campaign-alpha", bootstrap.campaignSeed,
		bootstrap.campaignIdentitySha256),
		"campaign identity should hash");
	bootstrap.runtimeFingerprint.schema = 2;
	bootstrap.runtimeFingerprint.high = 17;
	bootstrap.runtimeFingerprint.low = 23;
	bootstrap.contentManifestSha256[0] = 0xa5;
	return bootstrap;
}

PeerIdentity MakePeer(std::uint8_t marker = 7)
{
	PeerIdentity peer{};
	peer[0] = marker;
	peer[15] = static_cast<std::uint8_t>(marker + 1);
	return peer;
}

CoopCampaignSyncMetadata MakeMetadata(
	const CoopCampaignBootstrapDescriptor& bootstrap,
	std::uint64_t generation = 3,
	std::uint64_t totalSize =
		CoopCampaignSyncCanonicalChunkBytes + 3)
{
	CoopCampaignSyncMetadata metadata;
	metadata.transfer.protocolVersion = bootstrap.protocolVersion;
	metadata.transfer.sessionEpoch = bootstrap.sessionEpoch;
	metadata.transfer.transferId = generation + 100;
	metadata.transfer.campaignSeed = bootstrap.campaignSeed;
	metadata.transfer.campaignIdentitySha256 =
		bootstrap.campaignIdentitySha256;
	metadata.transfer.checkpointGeneration = generation;
	metadata.transfer.totalSize = totalSize;
	metadata.transfer.checkpointSha256[0] =
		static_cast<std::uint8_t>(generation + 1);
	metadata.worldMinutes = 1440 + generation;
	return metadata;
}

CoopCampaignSyncMetadataBytes EncodeMetadata(
	const CoopCampaignSyncMetadata& metadata)
{
	CoopCampaignSyncMetadataBytes bytes{};
	Expect(EncodeCoopCampaignSyncMetadata(metadata, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"metadata should encode");
	return bytes;
}

std::vector<std::uint8_t> EncodeChunk(
	const CoopCampaignSyncMetadata& metadata, std::uint64_t offset,
	std::size_t size, std::uint8_t marker)
{
	CoopCampaignSyncChunk chunk;
	chunk.transfer = metadata.transfer;
	chunk.offset = offset;
	chunk.payload.assign(size, marker);
	std::vector<std::uint8_t> bytes;
	Expect(EncodeCoopCampaignSyncChunk(chunk, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"chunk should encode");
	return bytes;
}

CoopCampaignSyncCompleteBytes EncodeComplete(
	const CoopCampaignSyncMetadata& metadata)
{
	CoopCampaignSyncComplete complete;
	complete.transfer = metadata.transfer;
	CoopCampaignSyncCompleteBytes bytes{};
	Expect(EncodeCoopCampaignSyncComplete(complete, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"complete should encode");
	return bytes;
}

CoopCampaignSyncRejectBytes EncodeReject(
	const CoopCampaignSyncMetadata& metadata,
	CoopCampaignSyncFailureReason reason)
{
	CoopCampaignSyncReject rejection;
	rejection.transfer = metadata.transfer;
	rejection.reason = reason;
	CoopCampaignSyncRejectBytes bytes{};
	Expect(EncodeCoopCampaignSyncReject(rejection, bytes) ==
		CoopCampaignSyncCodecResult::Success,
		"reject should encode");
	return bytes;
}

struct FakeScratch final : FullEngineCoopCampaignScratch
{
	FullEngineCoopCampaignScratchBeginResult beginResult =
		FullEngineCoopCampaignScratchBeginResult::Success;
	FullEngineCoopCampaignScratchWriteResult writeResult =
		FullEngineCoopCampaignScratchWriteResult::Success;
	FullEngineCoopCampaignScratchCommitResult commitResult =
		FullEngineCoopCampaignScratchCommitResult::Committed;
	CoopCampaignSyncMetadata metadata;
	std::vector<std::uint8_t> bytes;
	std::size_t beginCalls = 0;
	std::size_t writeCalls = 0;
	std::size_t commitCalls = 0;
	std::size_t abortCalls = 0;
	FullEngineCoopCampaignSyncClient* reentrantClient = nullptr;
	std::array<FullEngineCoopCampaignSyncClientResult, 16>
		reentrantResults{};
	std::size_t reentrantResultCount = 0;
	bool probeReentrancy = false;

	void probe() noexcept
	{
		if (!probeReentrancy || reentrantClient == nullptr ||
			reentrantResultCount >= reentrantResults.size())
			return;
		reentrantResults[reentrantResultCount++] =
			reentrantClient->flushOutbound();
	}

	FullEngineCoopCampaignScratchBeginResult begin(
		const CoopCampaignSyncMetadata& value) noexcept override
	{
		++beginCalls;
		probe();
		if (beginResult !=
			FullEngineCoopCampaignScratchBeginResult::Success)
			return beginResult;
		try
		{
			metadata = value;
			bytes.assign(static_cast<std::size_t>(
				value.transfer.totalSize), 0);
		}
		catch (...)
		{
			return FullEngineCoopCampaignScratchBeginResult::CapacityReached;
		}
		return beginResult;
	}

	FullEngineCoopCampaignScratchWriteResult writeExact(
		std::uint64_t offset, const std::uint8_t* source,
		std::size_t size) noexcept override
	{
		++writeCalls;
		probe();
		if (writeResult !=
			FullEngineCoopCampaignScratchWriteResult::Success)
			return writeResult;
		if (source == nullptr || offset > bytes.size() ||
			size > bytes.size() - static_cast<std::size_t>(offset))
			return FullEngineCoopCampaignScratchWriteResult::StorageFailure;
		std::copy(source, source + size,
			bytes.begin() + static_cast<std::size_t>(offset));
		return writeResult;
	}

	FullEngineCoopCampaignScratchCommitResult commitAndLoad(
		const CoopCampaignSyncMetadata& value) noexcept override
	{
		++commitCalls;
		probe();
		if (!SameCoopCampaignSyncTransfer(
			metadata.transfer, value.transfer))
			return FullEngineCoopCampaignScratchCommitResult::StorageFailure;
		return commitResult;
	}

	void abort() noexcept override
	{
		++abortCalls;
		probe();
	}
};

struct SentFrame
{
	std::string name;
	std::vector<std::uint8_t> bytes;
};

struct FakeWire final : FullEngineCoopCampaignSyncClientWire
{
	bool accepting = true;
	std::vector<SentFrame> sent;
	std::size_t closeCalls = 0;

	bool send(const char* messageName, const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		if (!accepting) return false;
		try
		{
			sent.push_back({messageName,
				std::vector<std::uint8_t>(bytes, bytes + size)});
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	void close() noexcept override { ++closeCalls; }
};

CoopCampaignSyncAck DecodeAck(const SentFrame& frame)
{
	CoopCampaignSyncAck acknowledgement;
	Expect(frame.name == CoopCampaignSyncAckMessageName,
		"frame should be campaign ACK");
	Expect(DecodeCoopCampaignSyncAck(frame.bytes.data(), frame.bytes.size(),
		acknowledgement) == CoopCampaignSyncCodecResult::Success,
		"ACK should decode");
	return acknowledgement;
}

CoopCampaignSyncResult DecodeResult(const SentFrame& frame)
{
	CoopCampaignSyncResult result;
	Expect(frame.name == CoopCampaignSyncResultMessageName,
		"frame should be campaign result");
	Expect(DecodeCoopCampaignSyncResult(frame.bytes.data(), frame.bytes.size(),
		result) == CoopCampaignSyncCodecResult::Success,
		"result should decode");
	return result;
}

void Begin(FullEngineCoopCampaignSyncClient& client,
	const CoopCampaignBootstrapDescriptor& bootstrap,
	const PeerIdentity& peer)
{
	Expect(client.beginSession(bootstrap, peer) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"session should begin");
	Expect(client.state() ==
		FullEngineCoopCampaignSyncClientState::AwaitingMetadata,
		"client should await metadata");
}

void TestHappyPathAndCommitGate()
{
	FakeScratch scratch;
	FakeWire wire;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	const auto bootstrap = MakeBootstrap();
	const auto peer = MakePeer();
	const auto metadata = MakeMetadata(bootstrap);
	Begin(client, bootstrap, peer);

	const auto metadataBytes = EncodeMetadata(metadata);
	Expect(client.receiveMetadata(metadataBytes.data(), metadataBytes.size()) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"metadata should start transfer");
	Expect(wire.sent.size() == 1, "metadata should emit ACK zero");
	const auto initialAck = DecodeAck(wire.sent.back());
	Expect(initialAck.nextExpectedOffset == 0 &&
		initialAck.precedingChunkChecksum == 0 &&
		initialAck.peerIdentity == peer,
		"initial ACK should bind peer and zero cursor");

	const auto first = EncodeChunk(metadata, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0x31);
	Expect(client.receiveChunk(first.data(), first.size()) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"first chunk should apply");
	const auto firstAck = DecodeAck(wire.sent.back());
	Expect(firstAck.nextExpectedOffset ==
		CoopCampaignSyncCanonicalChunkBytes,
		"first ACK should advance canonical cursor");

	const auto tail = EncodeChunk(metadata,
		CoopCampaignSyncCanonicalChunkBytes, 3, 0x77);
	Expect(client.receiveChunk(tail.data(), tail.size()) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"tail chunk should apply");
	Expect(client.nextExpectedOffset() == metadata.transfer.totalSize,
		"all checkpoint bytes should be staged");
	Expect(scratch.writeCalls == 2 && scratch.bytes.front() == 0x31 &&
		scratch.bytes.back() == 0x77,
		"scratch should receive exact ordered bytes");

	const auto complete = EncodeComplete(metadata);
	Expect(client.receiveComplete(complete.data(), complete.size()) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"complete should commit and deliver result");
	Expect(scratch.commitCalls == 1,
		"complete should commit exactly once");
	Expect(client.state() ==
		FullEngineCoopCampaignSyncClientState::Ready,
		"client should become ready only after result delivery");
	const auto result = DecodeResult(wire.sent.back());
	Expect(result.status == CoopCampaignSyncResultStatus::Committed &&
		result.reason == CoopCampaignSyncFailureReason::None &&
		result.peerIdentity == peer,
		"committed result should bind exact peer and transfer");
}

void TestBackpressureRetainsAndCoalescesAck()
{
	FakeScratch scratch;
	FakeWire wire;
	wire.accepting = false;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap);
	Begin(client, bootstrap, MakePeer());
	const auto metadataBytes = EncodeMetadata(metadata);
	Expect(client.receiveMetadata(metadataBytes.data(), metadataBytes.size()) ==
		FullEngineCoopCampaignSyncClientResult::Backpressured,
		"ACK zero should retain on backpressure");
	Expect(client.hasPendingOutbound() && wire.sent.empty(),
		"backpressured ACK should remain owned by client");

	const auto first = EncodeChunk(metadata, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0x42);
	Expect(client.receiveChunk(first.data(), first.size()) ==
		FullEngineCoopCampaignSyncClientResult::Backpressured,
		"cumulative ACK should remain backpressured");
	wire.accepting = true;
	Expect(client.flushOutbound() ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"retained cumulative ACK should retry");
	Expect(wire.sent.size() == 1,
		"ACK zero should coalesce into latest cumulative ACK");
	Expect(DecodeAck(wire.sent.back()).nextExpectedOffset ==
		CoopCampaignSyncCanonicalChunkBytes,
		"coalesced ACK should name latest staged cursor");

	const auto tail = EncodeChunk(metadata,
		CoopCampaignSyncCanonicalChunkBytes, 3, 0x43);
	Expect(client.receiveChunk(tail.data(), tail.size()) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"tail should stage after ACK retry");
	wire.accepting = false;
	const auto complete = EncodeComplete(metadata);
	Expect(client.receiveComplete(complete.data(), complete.size()) ==
		FullEngineCoopCampaignSyncClientResult::Backpressured,
		"committed result should retain on backpressure");
	Expect(client.state() ==
		FullEngineCoopCampaignSyncClientState::CommitPending,
		"client must not expose Ready before result delivery");
	wire.accepting = true;
	Expect(client.flushOutbound() ==
		FullEngineCoopCampaignSyncClientResult::Success &&
		client.state() == FullEngineCoopCampaignSyncClientState::Ready,
		"result retry should publish Ready");
}

void TestGapResyncAndExactReplay()
{
	FakeScratch scratch;
	FakeWire wire;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap);
	Begin(client, bootstrap, MakePeer());
	const auto metadataBytes = EncodeMetadata(metadata);
	(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());

	const auto tail = EncodeChunk(metadata,
		CoopCampaignSyncCanonicalChunkBytes, 3, 0x55);
	Expect(client.receiveChunk(tail.data(), tail.size()) ==
		FullEngineCoopCampaignSyncClientResult::ResyncRequested,
		"gap should request exact resync without applying bytes");
	Expect(scratch.writeCalls == 0,
		"gap must not mutate scratch");
	CoopCampaignSyncResync resync;
	Expect(DecodeCoopCampaignSyncResync(wire.sent.back().bytes.data(),
		wire.sent.back().bytes.size(), resync) ==
		CoopCampaignSyncCodecResult::Success &&
		resync.expectedOffset == 0 &&
		resync.precedingChunkChecksum == 0,
		"resync should echo exact zero cursor");

	const auto first = EncodeChunk(metadata, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0x54);
	(void)client.receiveChunk(first.data(), first.size());
	const std::size_t writes = scratch.writeCalls;
	Expect(client.receiveChunk(first.data(), first.size()) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"exact preceding chunk replay should be idempotent");
	Expect(scratch.writeCalls == writes,
		"exact replay must not rewrite scratch");
}

void TestSupersessionAndStaleFrames()
{
	FakeScratch scratch;
	FakeWire wire;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	const auto bootstrap = MakeBootstrap();
	const auto oldMetadata = MakeMetadata(bootstrap, 3);
	const auto newMetadata = MakeMetadata(bootstrap, 4);
	Begin(client, bootstrap, MakePeer());
	const auto oldBytes = EncodeMetadata(oldMetadata);
	const auto newBytes = EncodeMetadata(newMetadata);
	(void)client.receiveMetadata(oldBytes.data(), oldBytes.size());
	Expect(client.receiveMetadata(newBytes.data(), newBytes.size()) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"strictly newer metadata should supersede transaction");
	Expect(scratch.beginCalls == 2 && scratch.abortCalls >= 2,
		"supersession should abort old scratch before new begin");

	const auto staleChunk = EncodeChunk(oldMetadata, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0x61);
	Expect(client.receiveChunk(staleChunk.data(), staleChunk.size()) ==
		FullEngineCoopCampaignSyncClientResult::StaleMessage,
		"old in-flight chunk should be ignored after supersession");
	Expect(client.state() ==
		FullEngineCoopCampaignSyncClientState::Receiving,
		"stale frame must not disturb new transfer");
}

void TestStorageAndCommitFailuresAreTerminalResults()
{
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap);
	const auto metadataBytes = EncodeMetadata(metadata);

	{
		FakeScratch scratch;
		scratch.beginResult =
			FullEngineCoopCampaignScratchBeginResult::CapacityReached;
		FakeWire wire;
		FullEngineCoopCampaignSyncClient client(scratch, wire);
		Begin(client, bootstrap, MakePeer());
		Expect(client.receiveMetadata(metadataBytes.data(),
			metadataBytes.size()) ==
			FullEngineCoopCampaignSyncClientResult::StorageFailure,
			"scratch capacity failure should be reported");
		Expect(client.state() ==
			FullEngineCoopCampaignSyncClientState::Rejected,
			"delivered failure result should become terminal");
		const auto result = DecodeResult(wire.sent.back());
		Expect(result.status == CoopCampaignSyncResultStatus::Rejected &&
			result.reason == CoopCampaignSyncFailureReason::CapacityReached,
			"capacity rejection should cross wire exactly");
	}

	{
		FakeScratch scratch;
		scratch.commitResult =
			FullEngineCoopCampaignScratchCommitResult::HashMismatch;
		FakeWire wire;
		FullEngineCoopCampaignSyncClient client(scratch, wire);
		Begin(client, bootstrap, MakePeer(9));
		(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());
		const auto first = EncodeChunk(metadata, 0,
			CoopCampaignSyncCanonicalChunkBytes, 0x71);
		const auto tail = EncodeChunk(metadata,
			CoopCampaignSyncCanonicalChunkBytes, 3, 0x72);
		(void)client.receiveChunk(first.data(), first.size());
		(void)client.receiveChunk(tail.data(), tail.size());
		const auto complete = EncodeComplete(metadata);
		Expect(client.receiveComplete(complete.data(), complete.size()) ==
			FullEngineCoopCampaignSyncClientResult::CommitFailed,
			"hash mismatch should reject committed checkpoint");
		Expect(client.state() ==
			FullEngineCoopCampaignSyncClientState::Rejected &&
			client.failureReason() ==
				CoopCampaignSyncFailureReason::HashMismatch,
			"hash mismatch must never expose Ready");
		Expect(DecodeResult(wire.sent.back()).reason ==
			CoopCampaignSyncFailureReason::HashMismatch,
			"hash mismatch should be sent to server");
	}
}

void TestMalformedFutureTransferFailsClosed()
{
	FakeScratch scratch;
	FakeWire wire;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap, 3);
	Begin(client, bootstrap, MakePeer());
	const auto metadataBytes = EncodeMetadata(metadata);
	(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());
	const auto future = MakeMetadata(bootstrap, 9);
	const auto futureChunk = EncodeChunk(future, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0x81);
	Expect(client.receiveChunk(futureChunk.data(), futureChunk.size()) ==
		FullEngineCoopCampaignSyncClientResult::DescriptorMismatch,
		"future chunk without metadata should fail closed");
	Expect(client.state() ==
		FullEngineCoopCampaignSyncClientState::Failed &&
		wire.closeCalls == 1,
		"descriptor mismatch should close transport");
}

void TestActiveLifecycleCannotDropCommittedResult()
{
	FakeScratch scratch;
	FakeWire wire;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	const auto bootstrap = MakeBootstrap();
	const auto peer = MakePeer();
	const auto metadata = MakeMetadata(bootstrap, 20);
	Begin(client, bootstrap, peer);
	const auto metadataBytes = EncodeMetadata(metadata);
	(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());
	const auto first = EncodeChunk(metadata, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0x91);
	const auto tail = EncodeChunk(metadata,
		CoopCampaignSyncCanonicalChunkBytes, 3, 0x92);
	(void)client.receiveChunk(first.data(), first.size());
	(void)client.receiveChunk(tail.data(), tail.size());
	wire.accepting = false;
	const auto complete = EncodeComplete(metadata);
	Expect(client.receiveComplete(complete.data(), complete.size()) ==
		FullEngineCoopCampaignSyncClientResult::Backpressured,
		"committed result should remain pending for lifecycle test");
	const std::size_t aborts = scratch.abortCalls;
	Expect(client.beginSession(bootstrap, MakePeer(0x44)) ==
		FullEngineCoopCampaignSyncClientResult::InvalidState,
		"active session replacement should be rejected");
	Expect(client.state() ==
			FullEngineCoopCampaignSyncClientState::CommitPending &&
		client.hasPendingOutbound() && scratch.commitCalls == 1 &&
		scratch.abortCalls == aborts,
		"lifecycle rejection must preserve the loaded checkpoint result");
	wire.accepting = true;
	Expect(client.flushOutbound() ==
			FullEngineCoopCampaignSyncClientResult::Success &&
		client.state() == FullEngineCoopCampaignSyncClientState::Ready,
		"preserved committed result should still flush");
	const auto result = DecodeResult(wire.sent.back());
	Expect(result.peerIdentity == peer &&
		result.status == CoopCampaignSyncResultStatus::Committed,
		"lifecycle gate must retain the original admitted peer obligation");
	client.disconnect();
	Expect(wire.closeCalls == 1,
		"explicit disconnect should retire the old admitted wire");
	Expect(client.beginSession(bootstrap, MakePeer(0x44)) ==
		FullEngineCoopCampaignSyncClientResult::Success,
		"explicit disconnect should permit a fresh admitted session");
}

void TestScratchCallbacksAreUniformlyReentrancyGuarded()
{
	FakeScratch scratch;
	FakeWire wire;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	scratch.reentrantClient = &client;
	scratch.probeReentrancy = true;
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap, 21);
	Begin(client, bootstrap, MakePeer());
	const auto metadataBytes = EncodeMetadata(metadata);
	(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());
	const auto first = EncodeChunk(metadata, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0xa1);
	const auto tail = EncodeChunk(metadata,
		CoopCampaignSyncCanonicalChunkBytes, 3, 0xa2);
	(void)client.receiveChunk(first.data(), first.size());
	(void)client.receiveChunk(tail.data(), tail.size());
	const auto complete = EncodeComplete(metadata);
	(void)client.receiveComplete(complete.data(), complete.size());
	Expect(client.state() == FullEngineCoopCampaignSyncClientState::Ready &&
		scratch.reentrantResultCount == 5,
		"abort, begin, both writes, and commit should all be probed");
	for (std::size_t index = 0;
		index < scratch.reentrantResultCount; ++index)
		Expect(scratch.reentrantResults[index] ==
			FullEngineCoopCampaignSyncClientResult::ReentrantCall,
			"every scratch callback should reject coordinator reentry");

	const std::size_t beforeFailure = scratch.reentrantResultCount;
	Expect(client.receiveMetadata(nullptr, 0) ==
		FullEngineCoopCampaignSyncClientResult::InvalidMessage,
		"malformed metadata should exercise fail-path abort");
	Expect(scratch.reentrantResultCount == beforeFailure + 1 &&
		scratch.reentrantResults[beforeFailure] ==
			FullEngineCoopCampaignSyncClientResult::ReentrantCall &&
		client.state() == FullEngineCoopCampaignSyncClientState::Failed,
		"fail-path abort should use the same reentrancy guard");
}

void TestServerRejectIsRestrictedToRejectedTransfer()
{
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap, 22);
	const auto metadataBytes = EncodeMetadata(metadata);
	const auto rejection = EncodeReject(metadata,
		CoopCampaignSyncFailureReason::CapacityReached);
	{
		FakeScratch scratch;
		scratch.beginResult =
			FullEngineCoopCampaignScratchBeginResult::CapacityReached;
		FakeWire wire;
		FullEngineCoopCampaignSyncClient client(scratch, wire);
		Begin(client, bootstrap, MakePeer());
		(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());
		Expect(client.state() ==
			FullEngineCoopCampaignSyncClientState::Rejected,
			"local rejection result should be delivered first");
		Expect(client.receiveReject(rejection.data(), rejection.size()) ==
			FullEngineCoopCampaignSyncClientResult::ServerRejected,
			"server may acknowledge the exact already-rejected transfer");
		Expect(client.state() ==
				FullEngineCoopCampaignSyncClientState::Rejected &&
			wire.closeCalls == 0,
			"valid rejection acknowledgement remains terminal without close");
	}

	{
		FakeScratch scratch;
		FakeWire wire;
		FullEngineCoopCampaignSyncClient client(scratch, wire);
		Begin(client, bootstrap, MakePeer());
		(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());
		const auto first = EncodeChunk(metadata, 0,
			CoopCampaignSyncCanonicalChunkBytes, 0xb1);
		const auto tail = EncodeChunk(metadata,
			CoopCampaignSyncCanonicalChunkBytes, 3, 0xb2);
		(void)client.receiveChunk(first.data(), first.size());
		(void)client.receiveChunk(tail.data(), tail.size());
		const auto complete = EncodeComplete(metadata);
		(void)client.receiveComplete(complete.data(), complete.size());
		Expect(client.state() == FullEngineCoopCampaignSyncClientState::Ready,
			"client should load checkpoint before adversarial reject");
		Expect(client.receiveReject(rejection.data(), rejection.size()) ==
			FullEngineCoopCampaignSyncClientResult::InvalidState,
			"server reject after committed result should fail closed");
		Expect(client.state() ==
				FullEngineCoopCampaignSyncClientState::Failed &&
			wire.closeCalls == 1 &&
			client.failureReason() ==
				CoopCampaignSyncFailureReason::ProtocolViolation,
			"late reject cannot silently rewrite loaded state");
	}
}

void TestTerminalFlushDiagnosticsSurviveRetry()
{
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap, 23);
	const auto metadataBytes = EncodeMetadata(metadata);
	{
		FakeScratch scratch;
		scratch.beginResult =
			FullEngineCoopCampaignScratchBeginResult::CapacityReached;
		FakeWire wire;
		wire.accepting = false;
		FullEngineCoopCampaignSyncClient client(scratch, wire);
		Begin(client, bootstrap, MakePeer());
		Expect(client.receiveMetadata(metadataBytes.data(),
			metadataBytes.size()) ==
			FullEngineCoopCampaignSyncClientResult::Backpressured,
			"rejection result should retain through backpressure");
		wire.accepting = true;
		Expect(client.flushOutbound() ==
				FullEngineCoopCampaignSyncClientResult::Success &&
			client.state() ==
				FullEngineCoopCampaignSyncClientState::Rejected &&
			client.lastResult() ==
				FullEngineCoopCampaignSyncClientResult::StorageFailure,
			"successful result-frame retry should preserve terminal cause");
		Expect(client.flushOutbound() ==
				FullEngineCoopCampaignSyncClientResult::StorageFailure &&
			client.lastResult() ==
				FullEngineCoopCampaignSyncClientResult::StorageFailure,
			"idle terminal flush should not rewrite rejection diagnostics");
	}

	{
		FakeScratch scratch;
		FakeWire wire;
		FullEngineCoopCampaignSyncClient client(scratch, wire);
		Begin(client, bootstrap, MakePeer());
		Expect(client.receiveMetadata(nullptr, 0) ==
			FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			"malformed metadata should fail client");
		Expect(client.flushOutbound() ==
				FullEngineCoopCampaignSyncClientResult::InvalidMessage &&
			client.lastResult() ==
				FullEngineCoopCampaignSyncClientResult::InvalidMessage,
			"failed-state flush should preserve fatal diagnostics");
	}

	{
		FakeScratch scratch;
		FakeWire wire;
		FullEngineCoopCampaignSyncClient client(scratch, wire);
		Expect(client.flushOutbound() ==
			FullEngineCoopCampaignSyncClientResult::InvalidState,
			"disconnected client has no flushable wire obligation");
	}
}

void TestDuplicateCompleteIsIdempotent()
{
	FakeScratch scratch;
	FakeWire wire;
	FullEngineCoopCampaignSyncClient client(scratch, wire);
	const auto bootstrap = MakeBootstrap();
	const auto metadata = MakeMetadata(bootstrap, 24);
	Begin(client, bootstrap, MakePeer());
	const auto metadataBytes = EncodeMetadata(metadata);
	(void)client.receiveMetadata(metadataBytes.data(), metadataBytes.size());
	const auto first = EncodeChunk(metadata, 0,
		CoopCampaignSyncCanonicalChunkBytes, 0xc1);
	const auto tail = EncodeChunk(metadata,
		CoopCampaignSyncCanonicalChunkBytes, 3, 0xc2);
	(void)client.receiveChunk(first.data(), first.size());
	(void)client.receiveChunk(tail.data(), tail.size());
	const auto complete = EncodeComplete(metadata);
	wire.accepting = false;
	Expect(client.receiveComplete(complete.data(), complete.size()) ==
		FullEngineCoopCampaignSyncClientResult::Backpressured,
		"first complete should retain committed result");
	Expect(client.receiveComplete(complete.data(), complete.size()) ==
		FullEngineCoopCampaignSyncClientResult::Backpressured &&
		scratch.commitCalls == 1 &&
		client.state() ==
			FullEngineCoopCampaignSyncClientState::CommitPending,
		"duplicate complete while pending must not recommit");
	wire.accepting = true;
	Expect(client.flushOutbound() ==
			FullEngineCoopCampaignSyncClientResult::Success &&
		client.state() == FullEngineCoopCampaignSyncClientState::Ready,
		"retained committed result should reach Ready");
	const std::size_t sentBeforeReplay = wire.sent.size();
	Expect(client.receiveComplete(complete.data(), complete.size()) ==
			FullEngineCoopCampaignSyncClientResult::Success &&
		scratch.commitCalls == 1 &&
		wire.sent.size() == sentBeforeReplay + 1,
		"duplicate complete in Ready should resend result without reload");
	const auto replayed = DecodeResult(wire.sent.back());
	Expect(replayed.status == CoopCampaignSyncResultStatus::Committed &&
		replayed.reason == CoopCampaignSyncFailureReason::None,
		"idempotent complete should reproduce exact committed result");
	wire.accepting = false;
	Expect(client.receiveComplete(complete.data(), complete.size()) ==
			FullEngineCoopCampaignSyncClientResult::Backpressured &&
		client.state() ==
			FullEngineCoopCampaignSyncClientState::CommitPending,
		"backpressured Ready replay should restore the commit gate");
	const auto newer = MakeMetadata(bootstrap, 25);
	const auto newerBytes = EncodeMetadata(newer);
	Expect(client.receiveMetadata(newerBytes.data(), newerBytes.size()) ==
			FullEngineCoopCampaignSyncClientResult::InvalidState &&
		client.hasPendingOutbound(),
		"new metadata cannot erase a replayed committed result");
	wire.accepting = true;
	Expect(client.flushOutbound() ==
			FullEngineCoopCampaignSyncClientResult::Success &&
		client.state() == FullEngineCoopCampaignSyncClientState::Ready &&
		scratch.commitCalls == 1,
		"replayed result retry should return to Ready without recommit");
}
}

int main()
{
	TestHappyPathAndCommitGate();
	TestBackpressureRetainsAndCoalescesAck();
	TestGapResyncAndExactReplay();
	TestSupersessionAndStaleFrames();
	TestStorageAndCommitFailuresAreTerminalResults();
	TestMalformedFutureTransferFailsClosed();
	TestActiveLifecycleCannotDropCommittedResult();
	TestScratchCallbacksAreUniformlyReentrancyGuarded();
	TestServerRejectIsRestrictedToRejectedTransfer();
	TestTerminalFlushDiagnosticsSurviveRetry();
	TestDuplicateCompleteIsIdempotent();
	if (Failures != 0)
	{
		std::fprintf(stderr,
			"full-engine co-op campaign sync client: %d failure(s)\n",
			Failures);
		return 1;
	}
	std::puts("full-engine co-op campaign sync client tests passed");
	return 0;
}
