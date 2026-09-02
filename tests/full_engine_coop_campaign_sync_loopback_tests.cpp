#include <Multiplayer/FullEngineCoopCampaignSyncClient.h>
#include <Multiplayer/FullEngineCoopCampaignSyncServer.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

PeerIdentity Peer()
{
	PeerIdentity peer{};
	for (std::size_t index = 0; index < peer.size(); ++index)
		peer[index] = static_cast<std::uint8_t>(0x20 + index);
	return peer;
}

class MemorySource final : public FullEngineCoopCampaignCheckpointSource
{
public:
	MemorySource()
	{
		bytes.resize(CoopCampaignSyncCanonicalChunkBytes * 2 + 17);
		for (std::size_t index = 0; index < bytes.size(); ++index)
			bytes[index] = static_cast<std::uint8_t>(index * 37u + 11u);
		value.campaignSeed = UINT64_C(0x1020304050607080);
		CHECK(ComputeCoopCampaignIdentitySha256("shared_01",
			value.campaignSeed, value.campaignIdentitySha256),
			"loopback campaign identity hashes");
		value.checkpointGeneration = 9;
		value.totalSize = bytes.size();
		for (std::size_t index = 0; index < value.checkpointSha256.size(); ++index)
			value.checkpointSha256[index] =
				static_cast<std::uint8_t>(0x60 + index);
		value.worldMinutes = 777;
	}

	bool metadata(FullEngineCoopCampaignCheckpointMetadata& output)
		const noexcept override
	{
		output = value;
		return true;
	}

	FullEngineCoopCampaignCheckpointReadResult readExact(
		const CoopCampaignCheckpointSha256& expected,
		std::uint64_t offset, std::uint8_t* output,
		std::size_t size) noexcept override
	{
		if (expected != value.checkpointSha256)
			return FullEngineCoopCampaignCheckpointReadResult::DescriptorMismatch;
		if (output == nullptr || offset > bytes.size() ||
			size > bytes.size() - static_cast<std::size_t>(offset))
			return FullEngineCoopCampaignCheckpointReadResult::Unavailable;
		std::copy(bytes.begin() + static_cast<std::size_t>(offset),
			bytes.begin() + static_cast<std::size_t>(offset) + size, output);
		return FullEngineCoopCampaignCheckpointReadResult::Success;
	}

	FullEngineCoopCampaignCheckpointMetadata value;
	std::vector<std::uint8_t> bytes;
};

struct Frame
{
	std::string name;
	std::vector<std::uint8_t> bytes;
};

class ServerWire final : public FullEngineCoopCampaignSyncWireSink
{
public:
	bool send(const PeerIdentity& sentPeer, const TransportPeer& sentTransport,
		FullEngineCoopCampaignSyncOutboundKind, const char* name,
		const std::uint8_t* bytes, std::size_t size) noexcept override
	{
		if (sentPeer != peer || sentTransport != transport || name == nullptr)
			return false;
		try { frames.push_back(Frame{name, {bytes, bytes + size}}); }
		catch (...) { return false; }
		return true;
	}

	PeerIdentity peer = Peer();
	TransportPeer transport{55};
	std::vector<Frame> frames;
};

class ClientWire final : public FullEngineCoopCampaignSyncClientWire
{
public:
	bool send(const char* name, const std::uint8_t* bytes,
		std::size_t size) noexcept override
	{
		try { frames.push_back(Frame{name, {bytes, bytes + size}}); }
		catch (...) { return false; }
		return true;
	}
	void close() noexcept override { closed = true; }
	std::vector<Frame> frames;
	bool closed = false;
};

class MemoryScratch final : public FullEngineCoopCampaignScratch
{
public:
	FullEngineCoopCampaignScratchBeginResult begin(
		const CoopCampaignSyncMetadata& metadata) noexcept override
	{
		try
		{
			bytes.assign(static_cast<std::size_t>(metadata.transfer.totalSize), 0);
			written.assign(bytes.size(), false);
		}
		catch (...)
		{
			return FullEngineCoopCampaignScratchBeginResult::StorageFailure;
		}
		return FullEngineCoopCampaignScratchBeginResult::Success;
	}
	FullEngineCoopCampaignScratchWriteResult writeExact(
		std::uint64_t offset, const std::uint8_t* input,
		std::size_t size) noexcept override
	{
		if (input == nullptr || offset > bytes.size() ||
			size > bytes.size() - static_cast<std::size_t>(offset))
			return FullEngineCoopCampaignScratchWriteResult::StorageFailure;
		std::copy(input, input + size,
			bytes.begin() + static_cast<std::size_t>(offset));
		std::fill(written.begin() + static_cast<std::size_t>(offset),
			written.begin() + static_cast<std::size_t>(offset) + size, true);
		return FullEngineCoopCampaignScratchWriteResult::Success;
	}
	FullEngineCoopCampaignScratchCommitResult commitAndLoad(
		const CoopCampaignSyncMetadata&) noexcept override
	{
		++commits;
		return std::all_of(written.begin(), written.end(),
			[](bool value) { return value; })
			? FullEngineCoopCampaignScratchCommitResult::Committed
			: FullEngineCoopCampaignScratchCommitResult::StorageFailure;
	}
	void abort() noexcept override { ++aborts; }
	std::vector<std::uint8_t> bytes;
	std::vector<bool> written;
	unsigned commits = 0;
	unsigned aborts = 0;
};

bool DeliverServerFrames(ServerWire& wire,
	FullEngineCoopCampaignSyncClient& client)
{
	std::vector<Frame> frames;
	frames.swap(wire.frames);
	for (const Frame& frame : frames)
	{
		FullEngineCoopCampaignSyncClientResult result =
			FullEngineCoopCampaignSyncClientResult::InvalidMessage;
		if (frame.name == CoopCampaignSyncMetadataMessageName)
			result = client.receiveMetadata(frame.bytes.data(), frame.bytes.size());
		else if (frame.name == CoopCampaignSyncChunkMessageName)
			result = client.receiveChunk(frame.bytes.data(), frame.bytes.size());
		else if (frame.name == CoopCampaignSyncCompleteMessageName)
			result = client.receiveComplete(frame.bytes.data(), frame.bytes.size());
		else if (frame.name == CoopCampaignSyncRejectMessageName)
			result = client.receiveReject(frame.bytes.data(), frame.bytes.size());
		if (result != FullEngineCoopCampaignSyncClientResult::Success &&
			result != FullEngineCoopCampaignSyncClientResult::Backpressured &&
			result != FullEngineCoopCampaignSyncClientResult::StaleMessage)
			return false;
	}
	return true;
}

bool DeliverClientFrames(ClientWire& wire,
	FullEngineCoopCampaignSyncServer& server,
	const PeerIdentity& peer, const TransportPeer& transport)
{
	std::vector<Frame> frames;
	frames.swap(wire.frames);
	for (const Frame& frame : frames)
	{
		FullEngineCoopCampaignSyncInboundKind kind;
		if (frame.name == CoopCampaignSyncAckMessageName)
			kind = FullEngineCoopCampaignSyncInboundKind::Ack;
		else if (frame.name == CoopCampaignSyncResultMessageName)
			kind = FullEngineCoopCampaignSyncInboundKind::Result;
		else if (frame.name == CoopCampaignSyncResyncMessageName)
			kind = FullEngineCoopCampaignSyncInboundKind::Resync;
		else return false;
		if (server.handleInbound(peer, transport, kind,
			frame.bytes.data(), frame.bytes.size()) !=
			FullEngineCoopCampaignSyncServerResult::Success)
			return false;
	}
	return true;
}

void TestExactEndToEndTransfer()
{
	MemorySource source;
	ServerWire serverWire;
	FullEngineCoopCampaignSyncServer server(source, serverWire);
	MemoryScratch scratch;
	ClientWire clientWire;
	FullEngineCoopCampaignSyncClient client(scratch, clientWire);

	CoopCampaignBootstrapDescriptor bootstrap;
	bootstrap.protocolVersion = CurrentProtocolVersion;
	bootstrap.sessionEpoch = 99;
	bootstrap.campaignSeed = source.value.campaignSeed;
	bootstrap.campaignIdentitySha256 = source.value.campaignIdentitySha256;
	bootstrap.runtimeFingerprint = RuntimeCompatibilityFingerprint{1, 2, 3};
	bootstrap.contentManifestSha256.fill(7);
	const PeerIdentity peer = serverWire.peer;
	const FullEngineCoopCampaignSyncAuthenticatedPeer authenticated{
		peer, serverWire.transport};
	CHECK(client.beginSession(bootstrap, peer) ==
		FullEngineCoopCampaignSyncClientResult::Success &&
		server.beginSession(bootstrap.sessionEpoch) ==
			FullEngineCoopCampaignSyncServerResult::Success &&
		server.reconcilePeers(&authenticated, 1) ==
			FullEngineCoopCampaignSyncServerResult::Success,
		"loopback coordinators begin exact session");

	for (unsigned iteration = 0; iteration < 32; ++iteration)
	{
		const FullEngineCoopCampaignSyncFlushResult flushed =
			server.flushOutbound();
		CHECK(flushed.result ==
			FullEngineCoopCampaignSyncServerResult::Success,
			"server loopback flush succeeds");
		CHECK(DeliverServerFrames(serverWire, client),
			"client accepts exact server frames");
		const FullEngineCoopCampaignSyncClientResult clientFlushed =
			client.flushOutbound();
		CHECK(clientFlushed == FullEngineCoopCampaignSyncClientResult::Success ||
			clientFlushed == FullEngineCoopCampaignSyncClientResult::InvalidState,
			"client loopback flush succeeds or is idle");
		CHECK(DeliverClientFrames(clientWire, server,
			peer, serverWire.transport),
			"server accepts exact client controls");
		std::array<PeerIdentity,
			MaximumFullEngineCoopCampaignSyncPeers> ready{};
		if (server.readyPeers(ready) == 1 &&
			client.state() == FullEngineCoopCampaignSyncClientState::Ready)
			break;
	}

	std::array<PeerIdentity,
		MaximumFullEngineCoopCampaignSyncPeers> ready{};
	CHECK(server.readyPeers(ready) == 1 && ready[0] == peer &&
		client.state() == FullEngineCoopCampaignSyncClientState::Ready &&
		scratch.commits == 1 && scratch.bytes == source.bytes &&
		!clientWire.closed,
		"only verified loaded client becomes campaign-ready");
}
}

int main()
{
	TestExactEndToEndTransfer();
	if (failures != 0)
	{
		std::printf("%d campaign-sync loopback test(s) failed\n", failures);
		return 1;
	}
	std::puts("full-engine co-op campaign-sync loopback tests passed");
	return 0;
}
