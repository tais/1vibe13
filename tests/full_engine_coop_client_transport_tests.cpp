#include "Multiplayer/FullEngineCoopClientTransport.h"

#include "Multiplayer/CoopHandshakeProtocol.h"
#include "Multiplayer/FullEngineCoopServerSession.h"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

using namespace CoopSession;
using namespace ja2::mp;
using namespace ja2::mp::net;

namespace
{
int failures = 0;

static_assert(MaximumFullEngineCoopClientInboundWireSize == 61584);
static_assert(MaximumFullEngineCoopClientInboundWireSize >
	MaximumCoopTacticalWireSize);
static_assert(MaximumFullEngineCoopClientInboundWireSize ==
	MaximumCoopCampaignSyncWireSize);
static_assert(sizeof(FullEngineCoopClientTransport) <= 1024,
	"the heap-backed inbound FIFO must not consume the Windows thread stack");
static_assert(std::is_nothrow_default_constructible<
	FullEngineCoopClientTransport>::value,
	"allocating the bounded FIFO must leave transport construction noexcept");

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
		++failures; \
	} \
} while (false)

RuntimeCompatibilityFingerprint Fingerprint(std::uint64_t seed = 1)
{
	return RuntimeCompatibilityFingerprint{
		1, UINT64_C(0x1000000000000000) + seed,
		UINT64_C(0x2000000000000000) + seed};
}

ContentManifestSha256 Manifest(std::uint8_t seed = 1)
{
	ContentManifestSha256 digest{};
	for (std::size_t index = 0; index < digest.size(); ++index)
		digest[index] = static_cast<std::uint8_t>(seed + index);
	return digest;
}

PeerIdentity Identity(std::uint8_t seed)
{
	PeerIdentity identity{};
	for (std::size_t index = 0; index < identity.size(); ++index)
		identity[index] = static_cast<std::uint8_t>(seed + index);
	return identity;
}

ReconnectToken Token(std::uint8_t seed)
{
	ReconnectToken token{};
	for (std::size_t index = 0; index < token.size(); ++index)
		token[index] = static_cast<std::uint8_t>(seed + index);
	return token;
}

FullEngineCoopClientConfiguration ClientConfiguration()
{
	FullEngineCoopClientConfiguration configuration;
	configuration.runtimeFingerprint = Fingerprint();
	configuration.contentManifestSha256 = Manifest();
	return configuration;
}

CoopServerHelloBytes HelloBytes(
	const FullEngineCoopClientConfiguration& configuration,
	std::uint64_t epoch)
{
	CoopServerHello hello;
	hello.protocolVersion = configuration.protocolVersion;
	hello.sessionEpoch = epoch;
	hello.runtimeFingerprint = configuration.runtimeFingerprint;
	hello.contentManifestSha256 = configuration.contentManifestSha256;
	CoopServerHelloBytes bytes{};
	CHECK(EncodeCoopServerHello(hello, bytes),
		"server hello fixture encodes");
	return bytes;
}

AdmissionResponseBytes ResponseBytes(std::uint64_t epoch,
	const PeerIdentity& identity, const ReconnectToken& token)
{
	AdmissionResponse response;
	response.sessionEpoch = epoch;
	response.peerIdentity = identity;
	response.reconnectToken = token;
	response.rejectReason = AdmissionRejectReason::None;
	AdmissionResponseBytes bytes{};
	CHECK(EncodeAdmissionResponse(response, bytes),
		"admission response fixture encodes");
	return bytes;
}

AdmissionSelfRetirementResultBytes RetirementResultBytes(
	std::uint64_t epoch, std::uint64_t requestId,
	const PeerIdentity& identity)
{
	AdmissionSelfRetirementResult result;
	result.sessionEpoch = epoch;
	result.requestId = requestId;
	result.peerIdentity = identity;
	result.result = AdmissionSelfRetirementResultCode::CredentialRetired;
	AdmissionSelfRetirementResultBytes bytes{};
	CHECK(EncodeAdmissionSelfRetirementResult(result, bytes),
		"transport retirement result fixture encodes");
	return bytes;
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

TacticalWorldSnapshot Snapshot(std::uint64_t generation,
	std::uint64_t turn)
{
	std::vector<TacticalActorSnapshot> actors{Actor(1), Actor(2)};
	TacticalWorldSnapshot snapshot;
	CHECK(TacticalWorldSnapshot::create(generation,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true, TacticalMapAssetKey{{"A9.dat"}}},
		TacticalTurnSnapshot{true, true, 0, turn},
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"transport snapshot fixture is valid");
	return snapshot;
}

std::vector<std::uint8_t> BaselineBytes(std::uint64_t epoch,
	std::uint64_t generation, std::uint64_t revision,
	std::uint64_t turn, std::uint64_t cursor)
{
	CoopTacticalBaseline baseline;
	baseline.state.sessionEpoch = epoch;
	baseline.state.worldGeneration = generation;
	baseline.state.revision = revision;
	baseline.state.turnSerial = turn;
	baseline.baselineId = 1;
	baseline.nextExpectedCommandId = cursor;
	baseline.snapshot = Snapshot(generation, turn);
	for (const TacticalActorSnapshot& actor : baseline.snapshot.actors())
		baseline.assignedActors.push_back(actor.id);
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalBaseline(baseline, bytes) ==
		CoopTacticalCodecResult::Success,
		"transport baseline fixture encodes");
	return bytes;
}

std::vector<std::uint8_t> DeltaBytes(std::uint64_t epoch,
	std::uint64_t generation, std::uint64_t baseRevision,
	std::uint64_t revision, std::uint64_t turn,
	std::uint64_t deltaId = 1)
{
	CoopTacticalDelta envelope;
	envelope.state.sessionEpoch = epoch;
	envelope.state.worldGeneration = generation;
	envelope.state.revision = revision;
	envelope.state.turnSerial = turn;
	envelope.deltaId = deltaId;
	envelope.baseRevision = baseRevision;
	envelope.delta.previousEpoch = generation;
	envelope.delta.currentEpoch = generation;
	TacticalActorSnapshot actor = Actor(1);
	actor.grid = 1002;
	actor.direction = 3;
	envelope.delta.events.push_back(TacticalActorUpdatedEvent{actor});
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeCoopTacticalDelta(envelope, bytes) ==
		CoopTacticalCodecResult::Success,
		"transport delta fixture encodes");
	return bytes;
}

struct CaptureSlot
{
	const char* name = nullptr;
	std::vector<std::vector<std::uint8_t>> messages;
};

void CaptureMessage(SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	CaptureSlot& slot = *static_cast<CaptureSlot*>(context);
	try
	{
		slot.messages.emplace_back(
			message->data, message->data + message->size);
	}
	catch (...)
	{
	}
}

struct LoopbackServer
{
	SdlNetPeer* peer = nullptr;
	ConnectionId client;
	std::uint16_t port = 0;
	std::array<CaptureSlot, 13> captures{{
		{CoopAdmissionRequestMessageName, {}},
		{CoopAdmissionAckMessageName, {}},
		{CoopAdmissionSelfRetirementRequestMessageName, {}},
		{CoopTacticalBaselineAckMessageName, {}},
		{CoopTacticalDeltaAckMessageName, {}},
		{CoopTacticalResyncRequestMessageName, {}},
		{CoopTacticalIntentMessageName, {}},
		{CoopCampaignSyncAckMessageName, {}},
		{CoopCampaignSyncResultMessageName, {}},
		{CoopCampaignSyncResyncMessageName, {}},
		{CoopCampaignTimeRequestMessageName, {}},
		{CoopCampaignActionRequestMessageName, {}},
		{CoopCampaignHireRequestMessageName, {}}
	}};

	std::size_t count(const char* name) const noexcept
	{
		for (const CaptureSlot& capture : captures)
			if (std::strcmp(capture.name, name) == 0)
				return capture.messages.size();
		return 0;
	}
};

bool StartServer(LoopbackServer& server)
{
	server.peer = CreateSdlNetPeer();
	if (server.peer == nullptr) return false;
	static std::uint64_t sequence = static_cast<std::uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count());
	bool started = false;
	for (unsigned attempt = 0; attempt < 128 && !started; ++attempt)
	{
		server.port = static_cast<std::uint16_t>(
			40000 + sequence++ % 20000);
		started = server.peer->Start(
			2, SdlNetEndpoint(server.port, "127.0.0.1"));
	}
	if (!started) return false;
	server.peer->SetMaximumIncomingConnections(1);
	server.peer->SetTimeout(120000);
	for (CaptureSlot& capture : server.captures)
		if (!server.peer->RegisterMessage(
			capture.name, CaptureMessage, &capture)) return false;
	return true;
}

void PumpServer(LoopbackServer& server)
{
	if (server.peer == nullptr) return;
	for (SdlNetEvent* event = server.peer->Poll(); event;
		event = server.peer->Poll())
	{
		if (event->size != 0 && event->data != nullptr &&
			event->data[0] == SDLNET_NEW_INCOMING_CONNECTION)
			server.client = event->connection;
		server.peer->Release(event);
	}
}

void StopServer(LoopbackServer& server)
{
	if (server.peer == nullptr) return;
	server.peer->Shutdown(0);
	DestroySdlNetPeer(server.peer);
	server.peer = nullptr;
	server.client = NoConnection;
}

template <typename Predicate>
bool PumpUntil(LoopbackServer& server,
	FullEngineCoopClientTransport& transport,
	Predicate predicate, unsigned timeoutMilliseconds = 5000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		PumpServer(server);
		transport.poll();
		if (predicate()) return true;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		SDL_Delay(2);
	}
}

bool Send(LoopbackServer& server, const char* name,
	const std::uint8_t* bytes, std::size_t size)
{
	return server.peer != nullptr && server.client &&
		server.peer->SendMessage(
			name, bytes, size, server.client, false);
}

class RecordingReplica final : public FullEngineCoopPassiveReplicaSink
{
public:
	FullEngineCoopReplicaApplyResult applyBaseline(
		const CoopTacticalBaseline&) noexcept override
	{
		order[orderCount++] = 1;
		++baselineCalls;
		return FullEngineCoopReplicaApplyResult::Committed;
	}

	FullEngineCoopReplicaApplyResult applyDelta(
		const CoopTacticalDelta&) noexcept override
	{
		order[orderCount++] = 2;
		++deltaCalls;
		return FullEngineCoopReplicaApplyResult::Committed;
	}

	std::array<int, MaximumCoopTacticalDeltaHistory + 2> order{};
	std::size_t orderCount = 0;
	unsigned baselineCalls = 0;
	unsigned deltaCalls = 0;
};

class RecordingCampaignSink final
	: public FullEngineCoopClientCampaignSyncSink
{
public:
	bool receiveCampaignMetadata(
		const std::uint8_t* bytes, std::size_t size) noexcept override
	{
		return record(1, bytes, size);
	}

	bool receiveCampaignChunk(
		const std::uint8_t* bytes, std::size_t size) noexcept override
	{
		return record(2, bytes, size);
	}

	bool receiveCampaignComplete(
		const std::uint8_t* bytes, std::size_t size) noexcept override
	{
		return record(3, bytes, size);
	}

	bool receiveCampaignReject(
		const std::uint8_t* bytes, std::size_t size) noexcept override
	{
		return record(4, bytes, size);
	}

	bool record(int kind, const std::uint8_t* bytes,
		std::size_t size) noexcept
	{
		if (!accept || count >= kinds.size() || bytes == nullptr) return false;
		try
		{
			kinds[count] = kind;
			threads[count] = std::this_thread::get_id();
			states[count] = observedClient != nullptr
				? observedClient->state()
				: FullEngineCoopClientState::Disconnected;
			payloads[count].assign(bytes, bytes + size);
			++count;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	FullEngineCoopClient* observedClient = nullptr;
	std::array<int, 8> kinds{};
	std::array<std::thread::id, 8> threads{};
	std::array<FullEngineCoopClientState, 8> states{};
	std::array<std::vector<std::uint8_t>, 8> payloads{};
	std::size_t count = 0;
	bool accept = true;
};

FullEngineCoopClientTransportConfiguration TransportConfiguration(
	std::uint16_t port)
{
	FullEngineCoopClientTransportConfiguration configuration;
	configuration.serverEndpoint =
		SdlNetEndpoint(port, "127.0.0.1");
	return configuration;
}

bool ConnectClient(LoopbackServer& server,
	FullEngineCoopClientTransport& transport,
	FullEngineCoopClient& client,
	FullEngineCoopClientTransportConfiguration configuration)
{
	if (client.configure(ClientConfiguration()) !=
		FullEngineCoopClientResult::Success) return false;
	if (transport.connect(client, configuration) !=
		FullEngineCoopClientTransportConnectResult::Success) return false;
	return PumpUntil(server, transport, [&] {
		return server.client && transport.connected() &&
			client.state() == FullEngineCoopClientState::Hello;
	});
}

bool ConnectClient(LoopbackServer& server,
	FullEngineCoopClientTransport& transport,
	FullEngineCoopClient& client,
	FullEngineCoopClientCampaignSyncSink& campaignSink,
	FullEngineCoopClientTransportConfiguration configuration)
{
	if (client.configure(ClientConfiguration()) !=
		FullEngineCoopClientResult::Success) return false;
	if (transport.connect(client, campaignSink, configuration) !=
		FullEngineCoopClientTransportConnectResult::Success) return false;
	return PumpUntil(server, transport, [&] {
		return server.client && transport.connected() &&
			client.state() == FullEngineCoopClientState::Hello;
	});
}

void TestCallbackBatchConfigurationInvariant()
{
	FullEngineCoopClientTransportConfiguration configuration;
	CHECK(configuration.maximumQueuedInboundMessages ==
			MaximumFullEngineCoopClientInboundMessages &&
		MaximumFullEngineCoopClientInboundMessages == 16,
		"the immutable parser batch defaults to the fixed callback FIFO bound");

	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	configuration.serverEndpoint = SdlNetEndpoint(1, "127.0.0.1");
	configuration.maximumQueuedInboundMessages =
		MaximumFullEngineCoopClientInboundMessages + 1;
	CHECK(transport.connect(client, configuration) ==
			FullEngineCoopClientTransportConnectResult::InvalidConfiguration &&
		!transport.running() &&
		client.state() == FullEngineCoopClientState::Disconnected,
		"configuration cannot expand the fixed callback-batch bound");
}

void TestMaximumInFlightDeltaWindowSurvivesBoundedCallbackBatches()
{
	LoopbackServer server;
	CHECK(StartServer(server), "tactical-window server starts");
	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	CHECK(ConnectClient(server, transport, client,
		TransportConfiguration(server.port)),
		"tactical-window client connects");

	const FullEngineCoopClientConfiguration core = ClientConfiguration();
	const CoopServerHelloBytes hello = HelloBytes(core, 699);
	CHECK(Send(server, CoopServerHelloMessageName,
		hello.data(), hello.size()),
		"tactical-window server sends hello");
	CHECK(PumpUntil(server, transport, [&] {
		return client.state() == FullEngineCoopClientState::Admission;
	}), "tactical-window client reaches admission");

	const AdmissionResponseBytes response =
		ResponseBytes(699, Identity(0x18), Token(0x38));
	CHECK(Send(server, CoopAdmissionResponseMessageName,
		response.data(), response.size()),
		"tactical-window server admits client");
	CHECK(PumpUntil(server, transport, [&] {
		return client.state() == FullEngineCoopClientState::AwaitingBaseline;
	}), "tactical-window client awaits baseline");

	const std::vector<std::uint8_t> baseline =
		BaselineBytes(699, 10, 2, 3, 1);
	CHECK(Send(server, CoopTacticalBaselineMessageName,
		baseline.data(), baseline.size()),
		"tactical-window server sends baseline");
	CHECK(PumpUntil(server, transport, [&] {
		return client.state() == FullEngineCoopClientState::Active &&
			client.acceptedState().revision == 2;
	}), "tactical-window baseline commits before the burst");

	std::array<std::uint32_t, MaximumCoopTacticalDeltaHistory> checksums{};
	bool queued = true;
	for (std::size_t index = 0;
		index < MaximumCoopTacticalDeltaHistory; ++index)
	{
		const std::uint64_t baseRevision = 2 + index;
		const std::vector<std::uint8_t> delta = DeltaBytes(
			699, 10, baseRevision, baseRevision + 1, 3, index + 1);
		CoopTacticalDelta decoded;
		CHECK(DecodeCoopTacticalDelta(delta, decoded) ==
			CoopTacticalCodecResult::Success,
			"queued delta exposes its exact payload checksum");
		checksums[index] = decoded.payloadChecksum;
		queued = queued && Send(server, CoopTacticalDeltaMessageName,
			delta.data(), delta.size());
	}
	CHECK(queued,
		"server queues one maximum legal in-flight delta window");

	std::size_t pendingWrites = 1;
	const Uint64 deadline = SDL_GetTicks() + 2000;
	while (pendingWrites != 0 && SDL_GetTicks() < deadline)
	{
		PumpServer(server);
		if (!server.peer->PendingWriteBytes(server.client, pendingWrites))
			break;
		if (pendingWrites != 0) SDL_Delay(1);
	}
	CHECK(pendingWrites == 0,
		"complete tactical window reaches the loopback socket before one poll");
	SDL_Delay(20);

	// One parser pump may fill, but cannot exceed, the fixed callback FIFO.
	// Core/replica delivery begins only after that parser has unwound.
	transport.poll();
	const unsigned firstBatchDeltas = replica.deltaCalls;
	CHECK(transport.running() &&
		transport.lastFailure() == FullEngineCoopClientTransportFailure::None,
		"one capped parser pump does not fail the live transport");
	CHECK(transport.pendingInboundCount() == 0,
		"the callback FIFO drains after its parser pump unwinds");
	CHECK(client.state() == FullEngineCoopClientState::Active,
		"bounded callback delivery preserves the active client state");
	CHECK(firstBatchDeltas != 0 && firstBatchDeltas <=
			MaximumFullEngineCoopClientInboundMessages &&
		client.acceptedState().revision == 2 + firstBatchDeltas,
		"one no-event poll commits no more than one capped delta batch");
	CHECK(replica.baselineCalls == 1,
		"replica mutation begins only after the capped parser pump unwinds");

	CHECK(PumpUntil(server, transport, [&] {
		return client.acceptedState().revision ==
			2 + MaximumCoopTacticalDeltaHistory;
	}) && transport.running() &&
		transport.lastFailure() == FullEngineCoopClientTransportFailure::None &&
		replica.deltaCalls == MaximumCoopTacticalDeltaHistory,
		"retained excess deltas drain in order across later bounded parser pumps");

	CHECK(PumpUntil(server, transport, [&] {
		return server.count(CoopTacticalDeltaAckMessageName) ==
			MaximumCoopTacticalDeltaHistory;
	}), "every retained delta produces its exact ordered acknowledgement");
	const auto& acknowledgements = server.captures[4].messages;
	for (std::size_t index = 0; index < acknowledgements.size(); ++index)
	{
		CoopTacticalDeltaAck ack;
		const auto& bytes = acknowledgements[index];
		CHECK(index < checksums.size() &&
			DecodeCoopTacticalDeltaAck(bytes.data(), bytes.size(), ack) ==
				CoopTacticalCodecResult::Success &&
			ack.state.sessionEpoch == 699 && ack.state.worldGeneration == 10 &&
			ack.state.revision == 3 + index && ack.state.turnSerial == 3 &&
			ack.peerIdentity == Identity(0x18) && ack.deltaId == index + 1 &&
			ack.payloadChecksum == checksums[index],
			"acknowledgements retain each delta identity, state and payload checksum");
	}
	transport.stop();
	StopServer(server);
}

void TestLoopbackHandshakeFifoAndExactNamespaces()
{
	LoopbackServer server;
	CHECK(StartServer(server), "loopback server starts");
	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	CHECK(ConnectClient(server, transport, client,
		TransportConfiguration(server.port)),
		"isolated client transport connects over SDL3_net loopback");

	const std::uint8_t legacyPayload = 7;
	CHECK(Send(server, "legacy.v3.2.rpc", &legacyPayload, 1),
		"server can place an unknown legacy name on the framed socket");
	for (unsigned iteration = 0; iteration < 8; ++iteration)
	{
		PumpServer(server);
		transport.poll();
		SDL_Delay(1);
	}
	CHECK(client.state() == FullEngineCoopClientState::Hello &&
		transport.running(),
		"unregistered legacy namespace is ignored without touching the core");

	const FullEngineCoopClientConfiguration core = ClientConfiguration();
	const CoopServerHelloBytes hello = HelloBytes(core, 700);
	CHECK(Send(server, CoopServerHelloMessageName,
		hello.data(), hello.size()), "server sends exact co-op hello");
	CHECK(PumpUntil(server, transport, [&] {
		return client.state() == FullEngineCoopClientState::Admission &&
			server.count(CoopAdmissionRequestMessageName) == 1;
	}), "hello is delivered after polling and emits exact admission request");

	const PeerIdentity peer = Identity(0x20);
	const ReconnectToken token = Token(0x40);
	const AdmissionResponseBytes response = ResponseBytes(700, peer, token);
	CHECK(Send(server, CoopAdmissionResponseMessageName,
		response.data(), response.size()),
		"server sends exact admission response");
	CHECK(PumpUntil(server, transport, [&] {
		return client.state() ==
				FullEngineCoopClientState::AwaitingBaseline &&
			server.count(CoopAdmissionAckMessageName) == 1;
	}), "admission response is delivered and ACKed on exact namespace");

	const std::vector<std::uint8_t> baseline =
		BaselineBytes(700, 11, 2, 3, 5);
	const std::vector<std::uint8_t> delta =
		DeltaBytes(700, 11, 2, 3, 3);
	CHECK(Send(server, CoopTacticalBaselineMessageName,
		baseline.data(), baseline.size()) &&
		Send(server, CoopTacticalDeltaMessageName,
			delta.data(), delta.size()),
		"server queues baseline and delta in one ordered burst");
	CHECK(PumpUntil(server, transport, [&] {
		return client.state() == FullEngineCoopClientState::Active &&
			client.acceptedState().revision == 3 &&
			server.count(CoopTacticalBaselineAckMessageName) == 1 &&
			server.count(CoopTacticalDeltaAckMessageName) == 1;
	}), "fixed callback FIFO delivers baseline before dependent delta");
	CHECK(replica.baselineCalls == 1 && replica.deltaCalls == 1 &&
		replica.orderCount == 2 && replica.order[0] == 1 &&
		replica.order[1] == 2,
		"replica observes exact callback FIFO order on the caller thread");

	CHECK(client.sendIntent(TacticalEntityId{1, 1}, StopTacticalIntent{}) ==
		FullEngineCoopClientResult::Success,
		"active client sends through bounded socket adapter");
	CHECK(PumpUntil(server, transport, [&] {
		return server.count(CoopTacticalIntentMessageName) == 1;
	}), "only the exact co-op tactical intent name reaches the server");

	const std::vector<std::uint8_t> gap =
		DeltaBytes(700, 11, 3, 4, 3);
	CHECK(Send(server, CoopTacticalDeltaMessageName,
		gap.data(), gap.size()), "server sends a gap-inducing delta");
	CHECK(PumpUntil(server, transport, [&] {
		return server.count(CoopTacticalResyncRequestMessageName) == 1;
	}) && transport.running() && client.resyncPending() &&
		server.captures[5].messages.back().size() ==
			CoopTacticalResyncRequestWireSize,
		"transport sends the exact 88-byte resync namespace without closing");

	transport.stop();
	CHECK(!transport.running() &&
		client.state() == FullEngineCoopClientState::Disconnected,
		"explicit transport stop reports disconnection to the core");
	StopServer(server);
}

void TestCampaignBridgeFifoAndExactNamespaces()
{
	LoopbackServer server;
	CHECK(StartServer(server), "campaign bridge loopback server starts");
	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	RecordingCampaignSink campaign;
	campaign.observedClient = &client;
	CHECK(ConnectClient(server, transport, client, campaign,
		TransportConfiguration(server.port)),
		"campaign bridge client connects with its raw main-thread sink");

	CoopCampaignSyncMetadataBytes metadata{};
	std::vector<std::uint8_t> chunk(MaximumCoopCampaignSyncWireSize, 0x42u);
	CoopCampaignSyncCompleteBytes complete{};
	CoopCampaignSyncRejectBytes reject{};
	metadata.fill(0x11u);
	complete.fill(0x63u);
	reject.fill(0x84u);
	const CoopServerHelloBytes hello =
		HelloBytes(ClientConfiguration(), 705);
	CHECK(Send(server, CoopCampaignSyncMetadataMessageName,
			metadata.data(), metadata.size()) &&
		Send(server, CoopServerHelloMessageName,
			hello.data(), hello.size()) &&
		Send(server, CoopCampaignSyncChunkMessageName,
			chunk.data(), chunk.size()) &&
		Send(server, CoopCampaignSyncCompleteMessageName,
			complete.data(), complete.size()) &&
		Send(server, CoopCampaignSyncRejectMessageName,
			reject.data(), reject.size()),
		"server queues campaign frames around one core hello in a single burst");
	CHECK(PumpUntil(server, transport, [&] {
		return campaign.count == 4 &&
			server.count(CoopAdmissionRequestMessageName) == 1;
	}), "one fixed callback FIFO delivers all campaign and core frames in order");
	const std::thread::id caller = std::this_thread::get_id();
	CHECK(campaign.kinds[0] == 1 && campaign.kinds[1] == 2 &&
		campaign.kinds[2] == 3 && campaign.kinds[3] == 4 &&
		campaign.states[0] == FullEngineCoopClientState::Hello &&
		campaign.states[1] == FullEngineCoopClientState::Admission &&
		campaign.threads[0] == caller && campaign.threads[1] == caller &&
		campaign.threads[2] == caller && campaign.threads[3] == caller,
		"campaign delivery occurs after polling on the caller thread and preserves cross-domain order");
	CHECK(campaign.payloads[0] == std::vector<std::uint8_t>(
			metadata.begin(), metadata.end()) &&
		campaign.payloads[1] == chunk &&
		campaign.payloads[2] == std::vector<std::uint8_t>(
			complete.begin(), complete.end()) &&
		campaign.payloads[3] == std::vector<std::uint8_t>(
			reject.begin(), reject.end()),
		"client campaign sink receives every bounded byte including a maximum chunk");

	CoopCampaignSyncAckBytes acknowledgement{};
	CoopCampaignSyncResultBytes result{};
	CoopCampaignSyncResyncBytes resync{};
	acknowledgement.fill(0x19u);
	result.fill(0x2au);
	resync.fill(0x3bu);
	CHECK(transport.send(CoopCampaignSyncAckMessageName,
			acknowledgement.data(), acknowledgement.size()) &&
		transport.send(CoopCampaignSyncResultMessageName,
			result.data(), result.size()) &&
		transport.send(CoopCampaignSyncResyncMessageName,
			resync.data(), resync.size()),
		"client transport permits exactly the three campaign control directions");
	CHECK(PumpUntil(server, transport, [&] {
		return server.count(CoopCampaignSyncAckMessageName) == 1 &&
			server.count(CoopCampaignSyncResultMessageName) == 1 &&
			server.count(CoopCampaignSyncResyncMessageName) == 1;
	}), "loopback server receives all three exact campaign control names");
	CHECK(server.captures[7].messages[0] == std::vector<std::uint8_t>(
			acknowledgement.begin(), acknowledgement.end()) &&
		server.captures[8].messages[0] == std::vector<std::uint8_t>(
			result.begin(), result.end()) &&
		server.captures[9].messages[0] == std::vector<std::uint8_t>(
			resync.begin(), resync.end()),
		"campaign control sends preserve their exact fixed-width bytes");
	CHECK(!transport.send(CoopCampaignSyncMetadataMessageName,
		metadata.data(), metadata.size()),
		"client transport rejects a reversed server-to-client campaign name");
	transport.poll();
	CHECK(!transport.running() && transport.lastFailure() ==
			FullEngineCoopClientTransportFailure::TransportFailure,
		"reversed campaign direction fails the socket adapter closed");
	StopServer(server);
}

void TestVoluntaryRetirementExactTransportLifecycle()
{
	LoopbackServer server;
	CHECK(StartServer(server), "retirement loopback server starts");
	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	CHECK(ConnectClient(server, transport, client,
		TransportConfiguration(server.port)),
		"retirement client connects over the production socket adapter");
	const std::uint64_t epoch = 707;
	const CoopServerHelloBytes hello =
		HelloBytes(ClientConfiguration(), epoch);
	CHECK(Send(server, CoopServerHelloMessageName, hello.data(), hello.size()) &&
		PumpUntil(server, transport, [&] {
			return client.state() == FullEngineCoopClientState::Admission &&
				server.count(CoopAdmissionRequestMessageName) == 1;
		}), "retirement client completes hello and requests admission");
	const PeerIdentity peer = Identity(0x27);
	const ReconnectToken token = Token(0x47);
	const AdmissionResponseBytes admitted = ResponseBytes(epoch, peer, token);
	CHECK(Send(server, CoopAdmissionResponseMessageName,
			admitted.data(), admitted.size()) &&
		PumpUntil(server, transport, [&] {
			return client.state() ==
					FullEngineCoopClientState::AwaitingBaseline &&
				server.count(CoopAdmissionAckMessageName) == 1;
		}), "retirement client durably enters the ACK-authenticated waiting state");
	CHECK(client.requestSelfRetirement() ==
			FullEngineCoopClientResult::Success &&
		PumpUntil(server, transport, [&] {
			return server.count(
				CoopAdmissionSelfRetirementRequestMessageName) == 1;
		}), "client sends one exact bounded self-retirement frame");
	const CaptureSlot* requestCapture = nullptr;
	for (const CaptureSlot& capture : server.captures)
		if (std::strcmp(capture.name,
			CoopAdmissionSelfRetirementRequestMessageName) == 0)
			requestCapture = &capture;
	AdmissionSelfRetirementRequest request;
	CHECK(requestCapture != nullptr &&
		DecodeAdmissionSelfRetirementRequest(
			requestCapture->messages[0].data(),
			requestCapture->messages[0].size(), request) == DecodeResult::Ok &&
		request.sessionEpoch == epoch && request.requestId != 0,
		"wire request contains only protocol, epoch, and client correlation ID");
	const AdmissionSelfRetirementResultBytes completed =
		RetirementResultBytes(epoch, request.requestId, peer);
	CHECK(Send(server, CoopAdmissionSelfRetirementResultMessageName,
			completed.data(), completed.size()) &&
		PumpUntil(server, transport, [&] {
			return !transport.running();
		}) && client.state() == FullEngineCoopClientState::Retired &&
		client.lastResult() == FullEngineCoopClientResult::CredentialRetired &&
		transport.lastFailure() == FullEngineCoopClientTransportFailure::None,
		"truthful post-commit result cleanly retires core and socket without failure");
	StopServer(server);
}

void TestCampaignSinkFailureFailsClosed()
{
	{
		LoopbackServer server;
		CHECK(StartServer(server), "unbound campaign sink server starts");
		FullEngineCoopClientTransport transport;
		RecordingReplica replica;
		FullEngineCoopClient client(transport, replica);
		CHECK(ConnectClient(server, transport, client,
			TransportConfiguration(server.port)),
			"client without campaign sink connects for compatibility");
		CoopCampaignSyncMetadataBytes metadata{};
		CHECK(Send(server, CoopCampaignSyncMetadataMessageName,
			metadata.data(), metadata.size()),
			"server sends campaign metadata to an unbound adapter");
		CHECK(PumpUntil(server, transport, [&] {
			return !transport.running();
		}) && transport.lastFailure() ==
				FullEngineCoopClientTransportFailure::ClientRejected &&
			client.state() == FullEngineCoopClientState::Disconnected,
			"campaign input without a sink fails closed instead of disappearing");
		StopServer(server);
	}
	{
		LoopbackServer server;
		CHECK(StartServer(server), "rejecting campaign sink server starts");
		FullEngineCoopClientTransport transport;
		RecordingReplica replica;
		FullEngineCoopClient client(transport, replica);
		RecordingCampaignSink campaign;
		campaign.accept = false;
		CHECK(ConnectClient(server, transport, client, campaign,
			TransportConfiguration(server.port)),
			"client with rejecting campaign sink connects");
		CoopCampaignSyncRejectBytes reject{};
		CHECK(Send(server, CoopCampaignSyncRejectMessageName,
			reject.data(), reject.size()),
			"server sends a campaign frame rejected by its raw sink");
		CHECK(PumpUntil(server, transport, [&] {
			return !transport.running();
		}) && transport.lastFailure() ==
				FullEngineCoopClientTransportFailure::ClientRejected &&
			client.state() == FullEngineCoopClientState::Disconnected,
			"campaign sink rejection fails the transport closed");
		StopServer(server);
	}
}

void TestPendingWriteFailurePreservesCoreFailure()
{
	LoopbackServer server;
	CHECK(StartServer(server), "pending-bound server starts");
	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	FullEngineCoopClientTransportConfiguration configuration =
		TransportConfiguration(server.port);
	configuration.maximumPendingWriteBytes = 16;
	CHECK(ConnectClient(server, transport, client, configuration),
		"pending-bound client connects before its first write");

	const CoopServerHelloBytes hello =
		HelloBytes(ClientConfiguration(), 710);
	CHECK(Send(server, CoopServerHelloMessageName,
		hello.data(), hello.size()),
		"pending-bound server sends hello");
	CHECK(PumpUntil(server, transport, [&] {
		return !transport.running();
	}), "oversized pending-write obligation closes the adapter");
	CHECK(transport.lastFailure() ==
			FullEngineCoopClientTransportFailure::PendingWriteLimit &&
		client.state() == FullEngineCoopClientState::Failed &&
		client.lastResult() == FullEngineCoopClientResult::WireFailure &&
		server.count(CoopAdmissionRequestMessageName) == 0,
		"wire.close during delivery defers teardown and preserves core failure");
	StopServer(server);
}

void TestInboundCapacityAndSizeFailClosed()
{
	{
		LoopbackServer server;
		CHECK(StartServer(server), "capacity server starts");
		FullEngineCoopClientTransport transport;
		RecordingReplica replica;
		FullEngineCoopClient client(transport, replica);
		FullEngineCoopClientTransportConfiguration configuration =
			TransportConfiguration(server.port);
		configuration.maximumQueuedInboundMessages = 1;
		CHECK(ConnectClient(server, transport, client, configuration),
			"capacity client connects");
		const CoopServerHelloBytes hello =
			HelloBytes(ClientConfiguration(), 720);
		bool queued = true;
		for (unsigned index = 0; index < 8; ++index)
			queued = queued && Send(server, CoopServerHelloMessageName,
				hello.data(), hello.size());
		CHECK(queued, "server queues an adversarial hello burst");
		for (unsigned iteration = 0; iteration < 8; ++iteration)
		{
			PumpServer(server);
			SDL_Delay(1);
		}
		transport.poll();
		CHECK(!transport.running() && transport.lastFailure() ==
				FullEngineCoopClientTransportFailure::InboundCapacityReached &&
			client.state() == FullEngineCoopClientState::Disconnected,
			"callback FIFO saturation drops all frames and disconnects closed");
		StopServer(server);
	}

	{
		LoopbackServer server;
		CHECK(StartServer(server), "oversize server starts");
		FullEngineCoopClientTransport transport;
		RecordingReplica replica;
		FullEngineCoopClient client(transport, replica);
		CHECK(ConnectClient(server, transport, client,
			TransportConfiguration(server.port)),
			"oversize client connects");
		std::vector<std::uint8_t> oversized(
			MaximumCoopTacticalWireSize + 1, 0x5a);
		CHECK(Send(server, CoopTacticalBaselineMessageName,
			oversized.data(), oversized.size()),
			"SDL transport accepts frame just beyond co-op ceiling");
		CHECK(PumpUntil(server, transport, [&] {
			return !transport.running();
		}), "oversized co-op callback closes adapter");
		CHECK(transport.lastFailure() ==
				FullEngineCoopClientTransportFailure::InboundMessageTooLarge &&
			client.state() == FullEngineCoopClientState::Disconnected,
			"oversized registered message is never delivered to the core");
		StopServer(server);
	}

	{
		LoopbackServer server;
		CHECK(StartServer(server), "oversize campaign server starts");
		FullEngineCoopClientTransport transport;
		RecordingReplica replica;
		FullEngineCoopClient client(transport, replica);
		RecordingCampaignSink campaign;
		CHECK(ConnectClient(server, transport, client, campaign,
			TransportConfiguration(server.port)),
			"oversize campaign client connects");
		std::vector<std::uint8_t> oversized(
			MaximumFullEngineCoopClientInboundWireSize + 1, 0x6bu);
		CHECK(Send(server, CoopCampaignSyncChunkMessageName,
			oversized.data(), oversized.size()),
			"SDL transport accepts one byte beyond the campaign callback bound");
		CHECK(PumpUntil(server, transport, [&] {
			return !transport.running();
		}), "oversized campaign callback closes adapter");
		CHECK(transport.lastFailure() ==
				FullEngineCoopClientTransportFailure::InboundMessageTooLarge &&
			client.state() == FullEngineCoopClientState::Disconnected &&
			campaign.count == 0 && transport.pendingInboundCount() == 0,
			"61,585 campaign bytes fail before copy or sink delivery");
		StopServer(server);
	}
}

void TestConfigurationAndConnectionFailure()
{
	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	CHECK(client.configure(ClientConfiguration()) ==
		FullEngineCoopClientResult::Success,
		"failure-path client configures");
	FullEngineCoopClientTransportConfiguration invalid;
	CHECK(transport.connect(client, invalid) ==
			FullEngineCoopClientTransportConnectResult::InvalidConfiguration &&
		client.state() == FullEngineCoopClientState::Disconnected,
		"zero remote endpoint cannot create a listener or self-connect");

	LoopbackServer reservation;
	CHECK(StartServer(reservation), "closed-port reservation starts");
	const std::uint16_t closedPort = reservation.port;
	StopServer(reservation);
	CHECK(transport.connect(client, TransportConfiguration(closedPort)) ==
		FullEngineCoopClientTransportConnectResult::Success,
		"connection refusal begins asynchronously");
	const Uint64 started = SDL_GetTicks();
	while (transport.running() && SDL_GetTicks() - started < 5000)
	{
		transport.poll();
		SDL_Delay(2);
	}
	CHECK(!transport.running() && transport.lastFailure() ==
			FullEngineCoopClientTransportFailure::ConnectionAttemptFailed &&
		client.state() == FullEngineCoopClientState::Disconnected,
		"connection refusal retires the socket and restores disconnected core");
}

void TestLocalAndRemoteDisconnectLifecycle()
{
	{
		LoopbackServer server;
		CHECK(StartServer(server), "local-close server starts");
		FullEngineCoopClientTransport transport;
		RecordingReplica replica;
		FullEngineCoopClient client(transport, replica);
		CHECK(ConnectClient(server, transport, client,
			TransportConfiguration(server.port)),
			"local-close client connects");
		client.disconnect();
		CHECK(client.state() == FullEngineCoopClientState::Disconnected,
			"core applies its explicit disconnect synchronously");
		transport.poll();
		CHECK(!transport.running() &&
			client.state() == FullEngineCoopClientState::Disconnected,
			"wire close destroys its peer only after the next outer poll");
		StopServer(server);
	}

	{
		LoopbackServer server;
		CHECK(StartServer(server), "remote-close server starts");
		FullEngineCoopClientTransport transport;
		RecordingReplica replica;
		FullEngineCoopClient client(transport, replica);
		CHECK(ConnectClient(server, transport, client,
			TransportConfiguration(server.port)),
			"remote-close client connects");
		server.peer->CloseConnection(server.client, true);
		CHECK(PumpUntil(server, transport, [&] {
			return !transport.running();
		}), "remote transport close reaches isolated client adapter");
		CHECK(transport.lastFailure() ==
				FullEngineCoopClientTransportFailure::ConnectionLost &&
			client.state() == FullEngineCoopClientState::Disconnected,
			"remote close retires queued input and notifies the core once");
		StopServer(server);
	}
}
}

namespace
{
void TestCampaignActionWireDelivery(bool skipScene = false)
{
	LoopbackServer server;
	CHECK(StartServer(server), "campaign action socket server starts");
	FullEngineCoopClientTransport transport;
	RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	CHECK(ConnectClient(server, transport, client, TransportConfiguration(server.port)),
		"campaign action client connects");
	const auto hello = HelloBytes(ClientConfiguration(), 913);
	const auto admission = ResponseBytes(913, Identity(10), Token(30));
	CoopCampaignStatus status;
	status.sessionEpoch = 913;
	status.revision = 1;
	status.phase = CoopCampaignPhase::Strategic;
	status.timeLeader = Identity(20);
	status.leadershipRevision = 1;
	status.timeLeaderReady = true;
	status.readyPeers = 2;
	status.worldSeconds = 112800;
	if (skipScene)
	{
		status.pauseLocked = true;
		status.meanwhile = {7, CoopCampaignMeanwhileScene::FirstBattle};
	}
	CoopCampaignGroups groups;
	groups.sessionEpoch = 913;
	groups.revision = 1;
	groups.available = true;
	groups.groupCount = 1;
	groups.memberCount = 1;
	groups.groups[0].id = {2, 44};
	groups.groups[0].x = 9;
	groups.groups[0].y = 1;
	groups.groups[0].memberCount = 1;
	groups.members[0] = {{263, 55}, 246, 0};
	CoopCampaignStatusBytes statusBytes{};
	CoopCampaignGroupsBytes groupBytes{};
	std::size_t groupSize = 0;
	CHECK(EncodeCoopCampaignStatus(status, statusBytes) &&
		EncodeCoopCampaignGroups(groups, groupBytes, groupSize) &&
		Send(server, CoopServerHelloMessageName, hello.data(), hello.size()) &&
		Send(server, CoopAdmissionResponseMessageName, admission.data(), admission.size()) &&
		Send(server, CoopCampaignStatusMessageName, statusBytes.data(), statusBytes.size()) &&
		Send(server, CoopCampaignGroupsMessageName, groupBytes.data(), groupSize) &&
		PumpUntil(server, transport, [&] { return client.campaignStatus() && client.campaignGroups(); }),
		"ordered admission, status and roster enable a campaign action without tactical baseline");
	CoopCampaignActionRequest selection;
	selection.action = CoopCampaignAction::Travel;
	selection.group = groups.groups[0].id;
	selection.destinationX = 10;
	selection.destinationY = 1;
	if (skipScene)
	{
		selection = {};
		selection.action = CoopCampaignAction::SkipMeanwhile;
		selection.decision = status.meanwhile.id;
	}
	CHECK(client.requestCampaignAction(selection) == FullEngineCoopClientResult::Success &&
		client.campaignActionPending() &&
		PumpUntil(server, transport, [&] { return server.count(CoopCampaignActionRequestMessageName) == 1; }),
		"campaign action request crosses exact outbound namespace and width");
	CoopCampaignActionRequest request;
	const auto& captured = server.captures[11].messages;
	CHECK(captured.size() == 1 && captured.front().size() == CoopCampaignActionRequestWireSize &&
		DecodeCoopCampaignActionRequest(captured.front().data(), captured.front().size(), request) &&
		request.sessionEpoch == 913 && request.controlRevision == status.timeControlRevision &&
		request.groupsRevision == groups.revision && request.group == selection.group &&
		request.action == selection.action && request.decision == selection.decision &&
		request.destinationX == selection.destinationX && request.destinationY == selection.destinationY,
		"server receives exact core-attributed campaign action with current observation revisions");
	++status.revision;
	++status.timeControlRevision;
	if (skipScene)
	{
		status.meanwhile = {};
		status.pauseLocked = false;
	}
	else
	{
		++groups.revision;
		groups.groups[0].x = 10;
	}
	CoopCampaignActionResult result{request, status.timeControlRevision, groups.revision,
		CoopCampaignActionOutcome::Applied, 0};
	CoopCampaignActionResultBytes resultBytes{};
	CHECK(EncodeCoopCampaignStatus(status, statusBytes) &&
		EncodeCoopCampaignGroups(groups, groupBytes, groupSize) &&
		EncodeCoopCampaignActionResult(result, resultBytes) &&
		Send(server, CoopCampaignStatusMessageName, statusBytes.data(), statusBytes.size()) &&
		Send(server, CoopCampaignGroupsMessageName, groupBytes.data(), groupSize) &&
		Send(server, CoopCampaignActionResultMessageName, resultBytes.data(), resultBytes.size()) &&
		PumpUntil(server, transport, [&] { return client.lastCampaignActionResult() != nullptr; }),
		"terminal action receipt follows its status and roster through the shared callback FIFO");
	const auto* received = client.lastCampaignActionResult();
	CHECK(received && SameCoopCampaignActionRequest(received->request, request) &&
		received->outcome == result.outcome && received->controlRevision == result.controlRevision &&
		received->groupsRevision == result.groupsRevision && !client.campaignActionPending() &&
		client.campaignGroups() && client.campaignGroups()->groups[0].x == (skipScene ? 9 : 10) &&
		client.campaignStatus() && !client.campaignStatus()->meanwhile.id &&
		client.campaignStatus()->gamePaused && client.campaignStatus()->worldSeconds == 112800 &&
		transport.running() && replica.baselineCalls == 0 && replica.deltaCalls == 0,
		"receipt completes only the matching campaign action without tactical execution");
	if (skipScene)
	{
		++status.revision;
		CHECK(EncodeCoopCampaignStatus(status, statusBytes) &&
			Send(server, CoopCampaignActionResultMessageName, resultBytes.data(), resultBytes.size()) &&
			Send(server, CoopCampaignStatusMessageName, statusBytes.data(), statusBytes.size()) &&
			PumpUntil(server, transport, [&] {
				return client.campaignStatus() && client.campaignStatus()->revision == status.revision;
			}) && transport.running() && !client.campaignActionPending() &&
			server.count(CoopCampaignActionRequestMessageName) == 1,
			"duplicate Skip receipt is harmless and never replays the decision");
		CHECK(client.requestCampaignTime(CoopCampaignTimeAction::ThirtyMinutes) !=
				FullEngineCoopClientResult::Success && !client.campaignTimePending() &&
			server.count(CoopCampaignTimeRequestMessageName) == 0,
			"nonleader may skip the shared scene but cannot resume campaign time");
	}
	CHECK(!transport.send(CoopCampaignActionResultMessageName, resultBytes.data(), resultBytes.size()),
		"client cannot send a server-only campaign action result");
	transport.poll();
	CHECK(!transport.running() && transport.lastFailure() == FullEngineCoopClientTransportFailure::TransportFailure,
		"reversed campaign action direction closes the client transport");
	StopServer(server);
}

void TestCampaignHireWireDelivery()
{
	for (unsigned fault = 0; fault < 4; ++fault)
	{
		LoopbackServer server; CHECK(StartServer(server), "hire socket server starts");
		FullEngineCoopClientTransport transport; RecordingReplica replica; FullEngineCoopClient client(transport,replica);
		CHECK(ConnectClient(server,transport,client,TransportConfiguration(server.port)), "hire socket client connects");
		const auto hello = HelloBytes(ClientConfiguration(),914); const auto admission = ResponseBytes(914,Identity(10),Token(30));
		CoopCampaignStatus status; status.sessionEpoch = 914; status.revision = 1; status.phase = CoopCampaignPhase::Strategic;
		status.timeLeader = Identity(20); status.leadershipRevision = 1; status.timeLeaderReady = true; status.readyPeers = 2;
		CoopCampaignEconomy economy; economy.sessionEpoch = 914; economy.revision = 1; economy.available = true;
		economy.balance = 10000; economy.mercenaryLimit = 32; economy.mercenaryCount = economy.rosterCount = 1;
		economy.roster[0].actor = {0,1}; economy.roster[0].profile = 238; economy.roster[0].x = 9; economy.roster[0].y = 1;
		CoopCampaignAimQuotes quotes; quotes.sessionEpoch = 914; quotes.revision = quotes.economyRevision = 1; quotes.available = true;
		quotes.arrivalMinutes = 1900; quotes.landingX = 9; quotes.landingY = 1; quotes.quoteCount = 255;
		for (std::size_t i = 0; i < quotes.quoteCount; ++i) quotes.quotes[i].profile = static_cast<std::uint16_t>(i);
		auto& offer = quotes.quotes[0]; offer.status = CoopCampaignAimQuoteStatus::Available; offer.gearAvailable = true;
		offer.salary = {100,600,1100}; offer.medicalDeposit = 300; offer.gearCost = 50; offer.total = {400,450,900,950,1400,1450};
		CoopCampaignStatusBytes statusBytes; CoopCampaignEconomyBytes economyBytes; CoopCampaignAimQuotesBytes quoteBytes;
		std::size_t economySize = 0, quoteSize = 0;
		CHECK(EncodeCoopCampaignStatus(status,statusBytes) && EncodeCoopCampaignEconomy(economy,economyBytes,economySize) &&
			EncodeCoopCampaignAimQuotes(quotes,quoteBytes,quoteSize) && quoteSize == MaximumCoopCampaignAimQuotesWireSize &&
			Send(server,CoopServerHelloMessageName,hello.data(),hello.size()) && Send(server,CoopAdmissionResponseMessageName,admission.data(),admission.size()) &&
			Send(server,CoopCampaignStatusMessageName,statusBytes.data(),statusBytes.size()) && Send(server,CoopCampaignEconomyMessageName,economyBytes.data(),economySize) &&
			Send(server,CoopCampaignAimQuotesMessageName,quoteBytes.data(),quoteSize) && PumpUntil(server,transport,[&] { return client.campaignAimQuotes() != nullptr; }),
			"ordered admission and maximum quote replacement cross bounded callback FIFO without tactical state");
		CHECK(client.requestCampaignHire(0,7,true) == FullEngineCoopClientResult::Success && client.campaignHirePending() &&
			PumpUntil(server,transport,[&] { return server.count(CoopCampaignHireRequestMessageName) == 1; }), "nonleader hire reaches server through request-only namespace");
		CoopCampaignHireRequest request; const auto& captured = server.captures.back().messages;
		CHECK(captured.size() == 1 && DecodeCoopCampaignHireRequest(captured.front().data(),captured.front().size(),request) &&
			request.sessionEpoch == 914 && request.controlRevision == 1 && request.economyRevision == 1 && request.quoteRevision == 1 &&
			request.requestId == 1 && request.profile == 0 && request.days == 7 && request.buyGear, "socket request carries exact core-selected terms and current revision stamps");
		CoopCampaignHireResult result; result.request = request; result.controlRevision = 2; result.economyRevision = result.quoteRevision = 1;
		result.actor = {5,7}; result.chargedTotal = 950; result.outcome = CoopCampaignHireOutcome::Applied; result.nativeAttempted = true;
		++status.revision; ++status.timeControlRevision; ++economy.revision; economy.balance -= 950;
		economy.rosterCount = economy.mercenaryCount = 2; economy.roster[1] = economy.roster[0]; economy.roster[1].actor = result.actor;
		economy.roster[1].profile = 0; economy.roster[1].pendingHire = true; economy.roster[1].arrivalMinutes = 1900;
		if (fault == 1) ++economy.roster[1].actor.incarnation;
		++quotes.revision; quotes.economyRevision = economy.revision;
		CoopCampaignHireResultBytes resultBytes; CHECK(EncodeCoopCampaignHireResult(result,resultBytes), "socket native hire receipt encodes");
		if (fault == 2) resultBytes[94] = 1;
		CHECK(EncodeCoopCampaignStatus(status,statusBytes) && EncodeCoopCampaignEconomy(economy,economyBytes,economySize) &&
			EncodeCoopCampaignAimQuotes(quotes,quoteBytes,quoteSize), "socket post-hire observations encode");
		if (fault != 3) CHECK(Send(server,CoopCampaignStatusMessageName,statusBytes.data(),statusBytes.size()) &&
			Send(server,CoopCampaignEconomyMessageName,economyBytes.data(),economySize) && Send(server,CoopCampaignAimQuotesMessageName,quoteBytes.data(),quoteSize),
			"server queues committed observations before terminal receipt");
		CHECK(Send(server,CoopCampaignHireResultMessageName,resultBytes.data(),resultBytes.size()), "server queues exact bounded terminal hire frame");
		if (fault)
		{
			CHECK(PumpUntil(server,transport,[&] { return !transport.running(); }) && !client.campaignHirePending() && !client.campaignEconomy() &&
				transport.lastFailure() == FullEngineCoopClientTransportFailure::ClientRejected && !replica.baselineCalls && !replica.deltaCalls,
				"wrong actor, malformed authenticated receipt or observation ordering closes without tactical execution");
		}
		else
		{
			CHECK(PumpUntil(server,transport,[&] { return client.lastCampaignHireResult() != nullptr; }) && !client.campaignHirePending() &&
				client.lastCampaignHireResult()->actor == result.actor && client.campaignEconomy()->balance == 9050 &&
				client.campaignEconomy()->roster[1].pendingHire && client.campaignAimQuotes()->economyRevision == 2 &&
				!replica.baselineCalls && !replica.deltaCalls, "hire receipt releases input only after exact native pending actor is published");
			++status.revision;
			CHECK(EncodeCoopCampaignStatus(status,statusBytes) && Send(server,CoopCampaignHireResultMessageName,resultBytes.data(),resultBytes.size()) &&
				Send(server,CoopCampaignStatusMessageName,statusBytes.data(),statusBytes.size()) &&
				PumpUntil(server,transport,[&] { return client.campaignStatus() && client.campaignStatus()->revision == status.revision; }) && transport.running(),
				"duplicate socket receipt is harmless before a later ordered observation marker");
			CHECK(!transport.send(CoopCampaignHireResultMessageName,resultBytes.data(),resultBytes.size()), "client cannot send server-only hire result");
			transport.poll(); CHECK(!transport.running(), "reversed hire namespace closes transport");
		}
		StopServer(server);
	}
}

void TestCampaignActionWireWidthsFailClosed(bool hire = false)
{
	const auto requestName = hire ? CoopCampaignHireRequestMessageName : CoopCampaignActionRequestMessageName;
	const auto resultName = hire ? CoopCampaignHireResultMessageName : CoopCampaignActionResultMessageName;
	for (const bool inbound : {false, true})
	{
		const std::size_t exactSize = inbound ? (hire ? CoopCampaignHireResultWireSize : CoopCampaignActionResultWireSize) : (hire ? CoopCampaignHireRequestWireSize : CoopCampaignActionRequestWireSize);
		for (const std::size_t size : {std::size_t{0}, exactSize - 1, exactSize + 1})
		{
			LoopbackServer server;
			CHECK(StartServer(server), "action width socket server starts");
			FullEngineCoopClientTransport transport;
			RecordingReplica replica;
			FullEngineCoopClient client(transport, replica);
			CHECK(ConnectClient(server, transport, client, TransportConfiguration(server.port)),
				"action width socket client connects");
			std::array<std::uint8_t, CoopCampaignHireResultWireSize + 1> bytes{};
			if (inbound)
				CHECK(Send(server, resultName, bytes.data(), size),
					"noncanonical result width reaches the client callback");
			else
				CHECK(!transport.send(requestName, bytes.data(), size),
					"noncanonical request width is rejected before send");
			CHECK(PumpUntil(server, transport, [&] { return !transport.running(); }) &&
				transport.lastFailure() == (inbound ? FullEngineCoopClientTransportFailure::InboundMessageTooLarge :
					FullEngineCoopClientTransportFailure::TransportFailure) &&
				server.count(requestName) == 0 &&
				!client.lastCampaignActionResult() && transport.pendingInboundCount() == 0 &&
				replica.baselineCalls == 0 && replica.deltaCalls == 0,
				"wrong action width fails closed with no receipt or gameplay delivery");
			StopServer(server);
		}
	}
}

void TestCampaignGroupsWireDelivery()
{
	LoopbackServer server; CHECK(StartServer(server), "group socket server starts");
	FullEngineCoopClientTransport transport; RecordingReplica replica; FullEngineCoopClient client(transport, replica);
	CHECK(ConnectClient(server, transport, client, TransportConfiguration(server.port)), "group client connects");
	const auto hello = HelloBytes(ClientConfiguration(), 911); const auto admission = ResponseBytes(911, Identity(10), Token(30));
	CoopCampaignGroups groups; groups.sessionEpoch = 911; groups.revision = 1; groups.available = true;
	groups.groupCount = 1; groups.memberCount = 1;
	groups.groups[0].id = {2, 44}; groups.groups[0].x = 9; groups.groups[0].y = 1; groups.groups[0].memberCount = 1;
	groups.members[0] = {{263, 55}, 246, 0};
	CoopCampaignGroupsBytes bytes; std::size_t size = 0;
	CHECK(EncodeCoopCampaignGroups(groups, bytes, size) && Send(server, CoopServerHelloMessageName, hello.data(), hello.size()) &&
		Send(server, CoopAdmissionResponseMessageName, admission.data(), admission.size()) &&
		Send(server, CoopCampaignGroupsMessageName, bytes.data(), size) &&
		PumpUntil(server, transport, [&] { return client.campaignGroups() != nullptr; }) &&
		SameCoopCampaignGroups(*client.campaignGroups(), groups) && replica.baselineCalls == 0,
		"friendly groups cross copy-only socket FIFO after admission without local simulation");
	groups = {}; groups.sessionEpoch = 911; groups.revision = 2;
	CHECK(EncodeCoopCampaignGroups(groups, bytes, size) && Send(server, CoopCampaignGroupsMessageName, bytes.data(), size) &&
		PumpUntil(server, transport, [&] { return client.campaignGroups() && client.campaignGroups()->revision == 2; }) &&
		!client.campaignGroups()->available && !client.campaignGroups()->memberCount, "smaller unavailable replacement clears roster over actual transport");
	CHECK(Send(server, CoopCampaignGroupsMessageName, bytes.data(), size + 1) &&
		PumpUntil(server, transport, [&] { return !transport.running(); }) && !client.campaignGroups(), "count/width mismatch fails closed and clears groups");
	StopServer(server);
}
void TestCampaignStatusWireDelivery()
{
	LoopbackServer server; CHECK(StartServer(server), "campaign status socket server starts");
	FullEngineCoopClientTransport transport; RecordingReplica replica;
	FullEngineCoopClient client(transport, replica);
	CHECK(ConnectClient(server, transport, client, TransportConfiguration(server.port)), "status client connects");
	const auto hello = HelloBytes(ClientConfiguration(), 911);
	const auto admission = ResponseBytes(911, Identity(10), Token(30));
	CoopCampaignStatus status;
	status.sessionEpoch = 911; status.revision = 1; status.worldSeconds = 111600;
	status.timeLeader = Identity(10); status.leadershipRevision = 1; status.timeLeaderReady = true; status.readyPeers = 1;
	CoopCampaignStatusBytes bytes;
	CHECK(EncodeCoopCampaignStatus(status, bytes), "socket status fixture encodes");
	CHECK(Send(server, CoopServerHelloMessageName, hello.data(), hello.size()) &&
		Send(server, CoopAdmissionResponseMessageName, admission.data(), admission.size()) &&
		Send(server, CoopCampaignStatusMessageName, bytes.data(), bytes.size()), "hello, admission and status queued in order");
	CHECK(PumpUntil(server, transport, [&] { return client.campaignStatus() != nullptr; }) &&
		client.state() == FullEngineCoopClientState::AwaitingBaseline && replica.baselineCalls == 0 &&
		SameCoopCampaignStatus(*client.campaignStatus(), status), "fixed FIFO delivers status after admission, before any tactical baseline");
	++status.revision; ++status.worldSeconds;
	CHECK(EncodeCoopCampaignStatus(status, bytes) &&
		Send(server, CoopCampaignStatusMessageName, bytes.data(), bytes.size()) &&
		PumpUntil(server, transport, [&] { return client.campaignStatus() && client.campaignStatus()->revision == 2; }),
		"later server-only observation arrives through ordinary transport");
	status.phase = CoopCampaignPhase::Strategic; ++status.revision; ++status.timeControlRevision;
	CHECK(EncodeCoopCampaignStatus(status, bytes) && Send(server, CoopCampaignStatusMessageName, bytes.data(), bytes.size()) &&
		PumpUntil(server, transport, [&] { return client.campaignStatus() && client.campaignStatus()->revision == status.revision; }), "strategic status arrives");
	CHECK(client.requestCampaignTime(CoopCampaignTimeAction::ThirtyMinutes) == FullEngineCoopClientResult::Success &&
		PumpUntil(server, transport, [&] { return server.count(CoopCampaignTimeRequestMessageName) == 1; }), "time request crosses ordinary bounded outbound transport");
	CoopCampaignTimeResult result{{911, status.timeControlRevision, 1, CoopCampaignTimeAction::ThirtyMinutes},
		status.timeControlRevision + 1, CoopCampaignTimeOutcome::Applied};
	++status.revision; ++status.timeControlRevision;
	CoopCampaignTimeResultBytes resultBytes;
	CHECK(EncodeCoopCampaignStatus(status, bytes) && EncodeCoopCampaignTimeResult(result, resultBytes) &&
		Send(server, CoopCampaignStatusMessageName, bytes.data(), bytes.size()) &&
		Send(server, CoopCampaignTimeResultMessageName, resultBytes.data(), resultBytes.size()) &&
		PumpUntil(server, transport, [&] { return client.campaignTimeResult() != nullptr; }) &&
		!client.campaignTimePending() && replica.baselineCalls == 0, "status then terminal time result cross copy-only FIFO without tactical simulation");
	++status.revision; ++status.timeControlRevision;
	status.arrival.decision = 1; status.arrival.kind = CoopCampaignArrivalKind::Battle;
	status.arrival.stage = CoopCampaignArrivalStage::ReinforcementsRequired;
	status.arrival.x = 10; status.arrival.y = 1; status.arrival.pendingCount = 1;
	CHECK(EncodeCoopCampaignStatus(status, bytes) && Send(server, CoopCampaignStatusMessageName, bytes.data(), bytes.size()) &&
		PumpUntil(server, transport, [&] { return client.campaignStatus() && client.campaignStatus()->arrival.decision == 1; }) &&
		SameCoopCampaignStatus(*client.campaignStatus(), status) && !replica.baselineCalls,
		"native militia decision reaches the client through ordinary authenticated FIFO without a map");
	CHECK(client.requestCampaignTime(CoopCampaignTimeAction::SixtyMinutes) != FullEngineCoopClientResult::Success &&
		!client.campaignTimePending(), "pending native decision disables the client time request path");
	status.arrival = {}; ++status.revision; ++status.timeControlRevision;
	CHECK(EncodeCoopCampaignStatus(status, bytes) && Send(server, CoopCampaignStatusMessageName, bytes.data(), bytes.size()) &&
		PumpUntil(server, transport, [&] { return client.campaignStatus() && !client.campaignStatus()->arrival.decision; }),
		"same transport replaces the old arrival with an explicit empty notice");
	CHECK(Send(server, CoopCampaignStatusMessageName, bytes.data(), bytes.size() - 1) &&
		PumpUntil(server, transport, [&] { return !transport.running(); }) && !client.campaignStatus(),
		"non-exact status width closes transport and clears previously displayed clock");
	StopServer(server);
}
}

int main()
{
	CHECK(SDL_Init(0), "SDL initializes for production loopback transport");
	TestCallbackBatchConfigurationInvariant();
	TestMaximumInFlightDeltaWindowSurvivesBoundedCallbackBatches();
	TestCampaignGroupsWireDelivery();
	TestCampaignActionWireDelivery();
	TestCampaignActionWireDelivery(true);
	TestCampaignHireWireDelivery();
	TestCampaignActionWireWidthsFailClosed();
	TestCampaignActionWireWidthsFailClosed(true);
	TestCampaignStatusWireDelivery();
	TestLoopbackHandshakeFifoAndExactNamespaces();
	TestCampaignBridgeFifoAndExactNamespaces();
	TestVoluntaryRetirementExactTransportLifecycle();
	TestCampaignSinkFailureFailsClosed();
	TestPendingWriteFailurePreservesCoreFailure();
	TestInboundCapacityAndSizeFailClosed();
	TestConfigurationAndConnectionFailure();
	TestLocalAndRemoteDisconnectLifecycle();
	SDL_Quit();
	if (failures != 0)
	{
		std::printf("%d full-engine co-op client transport test(s) failed\n",
			failures);
		return 1;
	}
	std::puts("full-engine co-op client transport tests passed");
	return 0;
}
