// Committed-frame carried capture through the production runtime helper and SDL sockets.
#include "DedicatedCoopInventoryPublication.h"
#include "DedicatedCoopTacticalHost.h"
#include "FullEngineCoopTacticalServer.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Items.h"
#include "Overhead.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "connect.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <memory>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE;
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE;
BOOLEAN gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}

using namespace CoopSession;
using namespace ja2::mp;
using namespace ja2::mp::net;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { \
	++failures; std::printf("FAIL %s:%d  %s\n", \
		__FILE__, __LINE__, message); } } while (false)

constexpr std::uint64_t SessionEpoch = 0x1122334455667788ull;
constexpr std::uint64_t WorldGeneration = 1;
constexpr std::uint64_t InitialRevision = 20;
constexpr std::uint64_t TurnSerial = 3;
constexpr TacticalEntityId ActorId{1, 1};

class SequentialTokenSource final : public AdmissionTokenSource
{
public:
	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override
	{
		++sequence_;
		for (std::size_t index = 0; index < identity.size(); ++index)
			identity[index] = static_cast<std::uint8_t>(sequence_ + index);
		for (std::size_t index = 0; index < token.size(); ++index)
			token[index] = static_cast<std::uint8_t>(0x80 + sequence_ + index);
		return true;
	}

private:
	std::uint8_t sequence_ = 0;
};

class RetainedExecution final : public TacticalIntentExecutionSink
{
public:
	bool ready() const noexcept override { return true; }
	TacticalIntentExecutionDisposition execute(const AuthorizedTacticalIntent& intent) noexcept override
	{
		++calls;
		CoopTacticalIntentReceipt receipt;
		receipt.peerIdentity = intent.peerIdentity;
		receipt.commandId = intent.commandId;
		receipt.status = CoopTacticalIntentReceiptStatus::Queued;
		receipt.reason = CoopTacticalIntentReceiptReason::None;
		CHECK(server->recordReceipt(receipt) == FullEngineCoopTacticalServerResult::Success,
			"accepted command retains a queued receipt");
		return TacticalIntentExecutionDisposition::Retained;
	}
	FullEngineCoopTacticalServer* server = nullptr;
	unsigned calls = 0;
};

AuthorityConfiguration Authority(
	std::size_t maximumPeers = 2)
{
	AuthorityConfiguration configuration;
	configuration.enabled = true;
	configuration.sessionEpoch = SessionEpoch;
	configuration.runtimeFingerprintSupplied = true;
	configuration.runtimeFingerprint = {
		0x01020304u, 0x1122334455667788ull, 0x99aabbccddeeff00ull};
	configuration.contentManifestSupplied = true;
	for (std::size_t index = 0;
		index < configuration.contentManifestSha256.size(); ++index)
		configuration.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0xa0 + index);
	configuration.maximumPeers = maximumPeers;
	return configuration;
}

CoopCampaignBootstrapDescriptor Bootstrap(
	const AuthorityConfiguration& authority)
{
	CoopCampaignBootstrapDescriptor bootstrap;
	bootstrap.protocolVersion = CurrentProtocolVersion;
	bootstrap.sessionEpoch = authority.sessionEpoch;
	bootstrap.campaignSeed = 17;
	CHECK(ComputeCoopCampaignIdentitySha256(
		"tactical-server-test", bootstrap.campaignSeed,
		bootstrap.campaignIdentitySha256),
		"coordinator campaign identity hashes");
	bootstrap.runtimeFingerprint = authority.runtimeFingerprint;
	bootstrap.contentManifestSha256 = authority.contentManifestSha256;
	return bootstrap;
}

TacticalWorldSnapshot Snapshot(
	std::uint64_t generation = WorldGeneration,
	std::uint64_t turnSerial = TurnSerial,
	std::size_t actorCount = 1)
{
	TacticalWorldSnapshot snapshot;
	std::vector<TacticalActorSnapshot> actors;
	for (std::size_t index = 0; index < actorCount; ++index)
	{
		TacticalActorSnapshot actor;
		actor.id = TacticalEntityId{
			static_cast<std::uint16_t>(index + 1), 1};
		actor.team = 0;
		actor.profile = static_cast<std::uint16_t>(index + 1);
		actor.grid = static_cast<std::int32_t>(1001 + index);
		actor.direction = 2;
		actor.stance = TacticalStance::Standing;
		actor.actionPoints = 20;
		actor.life = 80;
		actor.maximumLife = 90;
		actor.breath = 75;
		actor.maximumBreath = 100;
		actor.active = true;
		actor.inSector = true;
		actors.push_back(actor);
	}
	CHECK(TacticalWorldSnapshot::create(generation,
		TacticalWorldDimensions{160, 160},
		TacticalSectorSnapshot{9, 2, 0, true, TacticalMapAssetKey{{"A9.dat"}}},
		TacticalTurnSnapshot{true, true, 0, turnSerial},
		std::move(actors), snapshot) == TacticalSnapshotCreateError::None,
		"coordinator snapshot fixture is valid");
	return snapshot;
}

TacticalWorldDelta EmptyDelta()
{
	TacticalWorldDelta delta;
	delta.previousEpoch = WorldGeneration;
	delta.currentEpoch = WorldGeneration;
	return delta;
}

AdmissionRequestBytes AdmissionBytes(const AuthorityConfiguration& authority,
	const AdmissionResponse* reconnect = nullptr)
{
	AdmissionRequest request;
	request.sessionEpoch = authority.sessionEpoch;
	request.runtimeFingerprint = authority.runtimeFingerprint;
	request.contentManifestSha256 = authority.contentManifestSha256;
	if (reconnect != nullptr)
	{
		request.peerIdentity = reconnect->peerIdentity;
		request.reconnectToken = reconnect->reconnectToken;
	}
	AdmissionRequestBytes bytes{};
	CHECK(EncodeAdmissionRequest(request, bytes),
		"coordinator admission request encodes");
	return bytes;
}

AdmissionAckBytes AdmissionAckFor(const AdmissionResponse& admitted)
{
	AdmissionAck acknowledgement;
	acknowledgement.sessionEpoch = admitted.sessionEpoch;
	acknowledgement.peerIdentity = admitted.peerIdentity;
	acknowledgement.reconnectToken = admitted.reconnectToken;
	AdmissionAckBytes bytes{};
	CHECK(EncodeAdmissionAck(acknowledgement, bytes),
		"coordinator admission ACK encodes");
	return bytes;
}

struct Capture
{
	std::vector<std::vector<std::uint8_t>> messages;
	std::vector<std::string>* order = nullptr;
	const char* label = nullptr;
};

void CaptureMessage(SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	Capture& capture = *static_cast<Capture*>(context);
	try
	{
		capture.messages.emplace_back(
			message->data, message->data + message->size);
		if (capture.order != nullptr && capture.label != nullptr)
			capture.order->emplace_back(capture.label);
	}
	catch (...)
	{
	}
}

struct Client
{
	SdlNetPeer* peer = nullptr;
	ConnectionId server;
	bool connected = false;
	bool disconnected = false;
	Capture hello;
	Capture admission;
	Capture baseline;
	Capture delta;
	Capture inventory;
	Capture receipt;
	std::vector<std::string> order;
};

void PumpClient(Client& client)
{
	if (client.peer == nullptr) return;
	for (SdlNetEvent* event = client.peer->Poll(); event;
		event = client.peer->Poll())
	{
		if (event->size != 0 && event->data != nullptr)
		{
			if (event->data[0] == SDLNET_CONNECTION_ACCEPTED)
			{
				client.connected = true;
				client.server = event->connection;
			}
			if (event->data[0] == SDLNET_DISCONNECTION_NOTIFICATION ||
				event->data[0] == SDLNET_CONNECTION_LOST)
				client.disconnected = true;
		}
		client.peer->Release(event);
	}
}

bool StartClient(Client& client, std::uint16_t port)
{
	client.baseline.order = &client.order;
	client.baseline.label = "baseline";
	client.delta.order = &client.order;
	client.delta.label = "delta";
	client.inventory.order = &client.order;
	client.inventory.label = "inventory";
	client.receipt.order = &client.order;
	client.receipt.label = "receipt";
	client.peer = CreateSdlNetPeer();
	if (client.peer == nullptr ||
		!client.peer->Start(1, SdlNetEndpoint()) ||
		!client.peer->RegisterMessage(
			CoopServerHelloMessageName, CaptureMessage, &client.hello) ||
		!client.peer->RegisterMessage(CoopAdmissionResponseMessageName,
			CaptureMessage, &client.admission) ||
		!client.peer->RegisterMessage(CoopTacticalBaselineMessageName,
			CaptureMessage, &client.baseline) ||
		!client.peer->RegisterMessage(CoopTacticalDeltaMessageName,
			CaptureMessage, &client.delta) ||
		!client.peer->RegisterMessage(CoopOwnerInventoryMessageName,
			CaptureMessage, &client.inventory) ||
		!client.peer->RegisterMessage(CoopTacticalIntentReceiptMessageName,
			CaptureMessage, &client.receipt))
		return false;
	return client.peer->Connect("127.0.0.1", port);
}

void DestroyClient(Client& client)
{
	if (client.peer == nullptr) return;
	client.peer->Shutdown(20);
	DestroySdlNetPeer(client.peer);
	client.peer = nullptr;
}

template <typename Predicate>
bool WaitUntil(FullEngineCoopAdmissionListener& listener,
	std::vector<Client*> clients, Predicate predicate,
	unsigned timeoutMilliseconds = 5000)
{
	const Uint64 start = SDL_GetTicks();
	for (;;)
	{
		listener.poll();
		for (Client* client : clients)
			if (client != nullptr) PumpClient(*client);
		if (predicate()) return true;
		if (SDL_GetTicks() - start >= timeoutMilliseconds) return false;
		SDL_Delay(2);
	}
}

bool StartListener(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopAdmissionListenerConfiguration& configuration)
{
	static Uint64 sequence = static_cast<Uint64>(
		std::chrono::steady_clock::now().time_since_epoch().count());
	for (unsigned attempt = 0; attempt < 128; ++attempt)
	{
		configuration.endpoint = SdlNetEndpoint(static_cast<std::uint16_t>(
			40000 + sequence++ % 20000), "127.0.0.1");
		if (listener.start(configuration) ==
			FullEngineCoopAdmissionListenerStartResult::Success)
			return true;
	}
	return false;
}

bool Admit(FullEngineCoopAdmissionListener& listener,
	Client& client, const AuthorityConfiguration& authority,
	AdmissionResponse& admitted, const AdmissionResponse* reconnect = nullptr)
{
	if (!WaitUntil(listener, {&client}, [&] {
		return client.connected && client.hello.messages.size() == 1;
	})) return false;
	const AdmissionRequestBytes request = AdmissionBytes(authority, reconnect);
	if (!client.peer->SendMessage(CoopAdmissionRequestMessageName,
		request.data(), request.size(), client.server, false) ||
		!WaitUntil(listener, {&client}, [&] {
			return client.admission.messages.size() == 1;
		})) return false;
	if (DecodeAdmissionResponse(client.admission.messages.back().data(),
		client.admission.messages.back().size(), admitted) != DecodeResult::Ok ||
		!admitted.admitted()) return false;
	const AdmissionAckBytes acknowledgement = AdmissionAckFor(admitted);
	if (!client.peer->SendMessage(CoopAdmissionAckMessageName,
		acknowledgement.data(), acknowledgement.size(), client.server, false))
		return false;
	return WaitUntil(listener, {&client}, [&] {
		TransportPeer transport;
		return listener.authenticatedTransportForPeer(
			admitted.peerIdentity, transport);
	});
}

bool SendIntent(Client& client, const PeerIdentity& claimedPeer,
	std::uint64_t commandId, std::uint64_t revision,
	TacticalEntityId actor = ActorId)
{
	TacticalIntent intent;
	intent.sessionEpoch = SessionEpoch;
	intent.claimedPeerIdentity = claimedPeer;
	intent.commandId = commandId;
	intent.worldGeneration = WorldGeneration;
	intent.baseRevision = revision;
	intent.turnSerial = TurnSerial;
	intent.actor = actor;
	intent.payload = StopTacticalIntent{};
	std::vector<std::uint8_t> bytes;
	if (EncodeTacticalIntent(intent, bytes) !=
		TacticalIntentCodecResult::Success) return false;
	return client.peer->SendMessage(CoopTacticalIntentMessageName,
		bytes.data(), bytes.size(), client.server, false);
}

CoopTacticalBaseline DecodeLastBaseline(const Client& client)
{
	CoopTacticalBaseline baseline;
	CHECK(!client.baseline.messages.empty() &&
		DecodeCoopTacticalBaseline(client.baseline.messages.back(), baseline) ==
			CoopTacticalCodecResult::Success,
		"client baseline decodes");
	return baseline;
}

CoopTacticalIntentReceipt DecodeLastReceipt(const Client& client)
{
	CoopTacticalIntentReceipt receipt;
	CHECK(!client.receipt.messages.empty() &&
		DecodeCoopTacticalIntentReceipt(client.receipt.messages.back().data(),
			client.receipt.messages.back().size(), receipt) ==
			CoopTacticalCodecResult::Success,
		"client receipt decodes");
	return receipt;
}

bool SendBaselineAck(Client& client, const CoopTacticalBaseline& baseline,
	const PeerIdentity& claimedPeer)
{
	CoopTacticalBaselineAck acknowledgement;
	acknowledgement.state = baseline.state;
	acknowledgement.peerIdentity = claimedPeer;
	acknowledgement.baselineId = baseline.baselineId;
	acknowledgement.payloadChecksum = baseline.payloadChecksum;
	acknowledgement.nextExpectedCommandId =
		baseline.nextExpectedCommandId;
	CoopTacticalBaselineAckBytes bytes{};
	if (EncodeCoopTacticalBaselineAck(acknowledgement, bytes) !=
		CoopTacticalCodecResult::Success) return false;
	return client.peer->SendMessage(CoopTacticalBaselineAckMessageName,
		bytes.data(), bytes.size(), client.server, false);
}

bool QueueArrived(FullEngineCoopAdmissionListener& listener,
	Client& client, std::size_t count = 1)
{
	return WaitUntil(listener, {&client}, [&] {
		return listener.pendingInboundCount() >= count;
	});
}

CoopOwnerInventorySnapshot DecodeLastInventory(const Client& client)
{
	CoopOwnerInventorySnapshot inventory;
	CHECK(!client.inventory.messages.empty() &&
		DecodeCoopOwnerInventorySnapshot(client.inventory.messages.back().data(),
			client.inventory.messages.back().size(), inventory) == CoopInventoryCodecResult::Success,
		"owner inventory decodes from the reliable socket");
	return inventory;
}

void MakeObject(OBJECTTYPE& object, UINT16 item, INT16 points)
{
	object.initialize();
	object.usItem = item;
	object.ubNumberOfObjects = 1;
	object.objectStack.resize(1);
	object[0]->data.objectStatus = points;
}

void TestCommittedOwnerPublication(GameContext& game)
{
	SequentialTokenSource tokens;
	RetainedExecution execution;
	auto ingressStorage = std::make_unique<FullEngineCoopIngress>(tokens, execution);
	auto& ingress = *ingressStorage;
	const auto authority = Authority();
	CHECK(ingress.beginAdmissionSession(authority) == FullEngineCoopStartResult::Success,
		"native publication admission starts");
	auto listenerStorage = std::make_unique<FullEngineCoopAdmissionListener>(ingress);
	auto& listener = *listenerStorage;
	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration;
	listenerConfiguration.campaignBootstrap = Bootstrap(authority);
	CHECK(StartListener(listener, listenerConfiguration), "native publication loopback listener starts");
	std::array<Client, 2> clients;
	std::array<AdmissionResponse, 2> admitted;
	std::array<PeerIdentity, 2> peers;
	for (std::size_t index = 0; index < clients.size(); ++index)
	{
		CHECK(StartClient(clients[index], listenerConfiguration.endpoint.port) &&
			Admit(listener, clients[index], authority, admitted[index]), "both native inventory owners authenticate");
		peers[index] = admitted[index].peerIdentity;
	}
	auto serverStorage = std::make_unique<FullEngineCoopTacticalServer>(ingress, listener);
	auto& server = *serverStorage;
	execution.server = &server;
	const std::array<CoopTacticalActorAssignment, 2> assignments{{
		{TacticalEntityId{1, 1}, peers[0]}, {TacticalEntityId{2, 1}, peers[1]}}};
	std::sort(peers.begin(), peers.end());
	CHECK(server.beginEpoch(SessionEpoch) == FullEngineCoopTacticalServerResult::Success &&
		server.beginWorld(WorldGeneration, InitialRevision, TurnSerial) == FullEngineCoopTacticalServerResult::Success &&
		server.setCampaignReadyPeers(peers.data(), peers.size()) == FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() == FullEngineCoopTacticalServerResult::Success &&
		server.replaceAssignments(assignments.data(), assignments.size()) == FullEngineCoopTacticalServerResult::Success,
		"runtime has an active world and exact actor owners");
	auto liveStorage = std::make_unique<DedicatedCoopTacticalJa2LiveState>(game);
	auto& live = *liveStorage;
	auto& actor = *GetJa2SoldierRepository().resolve(1);
	OBJECTTYPE& pocket = actor.inventory()[BIGPOCK1POS];
	// Peers without a baseline must not cause native reads or partial frames.
	++pocket.ubNumberOfObjects;
	CHECK(StageDedicatedCoopOwnerInventories(live, server, WorldGeneration),
		"pre-baseline owners are skipped even when native storage is not capturable");
	--pocket.ubNumberOfObjects;
	CHECK(server.stageBaselines(Snapshot(WorldGeneration, TurnSerial, 2)) == FullEngineCoopTacticalServerResult::Success &&
		StageDedicatedCoopOwnerInventories(live, server, WorldGeneration) &&
		server.flushOutbound().result == FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&clients[0], &clients[1]}, [&] {
			return clients[0].baseline.messages.size() == 1 && clients[1].baseline.messages.size() == 1;
		}) && clients[0].inventory.messages.empty() && clients[1].inventory.messages.empty(),
		"native owner contents stage alongside fresh baselines but cannot pass their ACK barrier");
	CHECK(SendBaselineAck(clients[0], DecodeLastBaseline(clients[0]), admitted[0].peerIdentity) &&
		SendBaselineAck(clients[1], DecodeLastBaseline(clients[1]), admitted[1].peerIdentity) &&
		WaitUntil(listener, {&clients[0], &clients[1]}, [&] { return listener.pendingInboundCount() == 2; }) &&
		server.pumpInbound().acknowledgementsAccepted == 2 &&
		WaitUntil(listener, {&clients[0], &clients[1]}, [&] {
			return clients[0].inventory.messages.size() == 1 && clients[1].inventory.messages.size() == 1;
		}), "baseline ACKs publish both private native frames");
	for (std::size_t index = 0; index < clients.size(); ++index)
	{
		const auto inventory = DecodeLastInventory(clients[index]);
		CHECK(inventory.owner == admitted[index].peerIdentity && inventory.actor == assignments[index].actor &&
			inventory.worldGeneration == WorldGeneration && inventory.sessionEpoch == SessionEpoch &&
			inventory.baselineId == DecodeLastBaseline(clients[index]).baselineId &&
			inventory.inventoryRevision == 1 && inventory.slots[BIGPOCK1POS].resourceTotal == 73 - index * 20 &&
			inventory.groundGrid == -1 && inventory.groundItems.empty(),
			"socket frame has only its authenticated owner's dense native carried resources");
	}
	CHECK(StageDedicatedCoopOwnerInventories(live, server, WorldGeneration) &&
		server.flushOutbound().messagesSent == 0, "unchanged native captures do not resend or advance tokens");
	bool offThreadCapture = true, offThreadStage = true;
	CoopOwnerInventorySnapshot untouched;
	std::thread other([&] {
		offThreadCapture = live.captureInventory(ActorId, WorldGeneration, untouched);
		offThreadStage = StageDedicatedCoopOwnerInventories(live, server, WorldGeneration);
	});
	other.join();
	CHECK(!offThreadCapture && !offThreadStage && untouched.inventoryRevision == 0 &&
		!StageDedicatedCoopOwnerInventories(live, server, WorldGeneration + 1),
		"native capture/staging rejects foreign threads and mismatched world scope");

	CHECK(SendIntent(clients[0], admitted[0].peerIdentity, 1, InitialRevision) && QueueArrived(listener, clients[0]) &&
		server.pumpInbound(500).intentsConsumed == 1 &&
		WaitUntil(listener, {&clients[0]}, [&] { return clients[0].receipt.messages.size() == 1; }),
		"existing Stop vocabulary establishes a retained command before committed native work");
	// Model completed native work at the committed-frame boundary. No new wire
	// verb or client-side simulation is needed to observe changed carried data.
	pocket[0]->data.objectStatus = 61;
	CHECK(server.publishDelta(EmptyDelta(), InitialRevision + 1, TurnSerial) == FullEngineCoopTacticalServerResult::Success,
		"runtime stages the resulting public frame first");
	CoopTacticalIntentReceipt terminal;
	terminal.peerIdentity = admitted[0].peerIdentity;
	terminal.commandId = 1;
	terminal.status = CoopTacticalIntentReceiptStatus::Applied;
	terminal.reason = CoopTacticalIntentReceiptReason::None;
	terminal.simulationTick = 501;
	CHECK(server.recordReceipt(terminal) == FullEngineCoopTacticalServerResult::Success &&
		StageDedicatedCoopOwnerInventories(live, server, WorldGeneration) &&
		!server.hasInventoryRevision(admitted[0].peerIdentity, ActorId, 1) &&
		server.pumpInbound(501).result == FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&clients[0], &clients[1]}, [&] {
			return clients[0].inventory.messages.size() == 2 && clients[0].receipt.messages.size() == 2;
		}), "runtime captures native replacement before the tactical pump's implicit flush");
	CHECK(clients[0].order.size() >= 3 && clients[0].order[clients[0].order.size()-3] == "delta" &&
		clients[0].order[clients[0].order.size()-2] == "inventory" && clients[0].order.back() == "receipt" &&
		DecodeLastInventory(clients[0]).slots[BIGPOCK1POS].resourceTotal == 61 &&
		DecodeLastInventory(clients[0]).inventoryRevision == 2 &&
		DecodeLastReceipt(clients[0]).status == CoopTacticalIntentReceiptStatus::Applied &&
		clients[1].inventory.messages.size() == 1 && execution.calls == 1,
		"public frame, new native owner inventory, then terminal receipt arrive in causal socket order");
	// No public frame changes for an ordinary object's private metadata.
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(StageDedicatedCoopOwnerInventories(live, server, WorldGeneration) &&
		server.flushOutbound().result == FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&clients[0]}, [&] { return clients[0].inventory.messages.size() == 3; }) &&
		DecodeLastInventory(clients[0]).inventoryRevision == 3 && clients[0].delta.messages.size() == 1,
		"private metadata reaches owner even when the observer's public frame is unchanged");
	++pocket.ubNumberOfObjects;
	CHECK(!StageDedicatedCoopOwnerInventories(live, server, WorldGeneration),
		"malformed native storage fails the runtime publication barrier instead of acknowledging stale state");
	--pocket.ubNumberOfObjects;
	CHECK(StageDedicatedCoopOwnerInventories(live, server, WorldGeneration) &&
		server.flushOutbound().messagesSent == 0 && server.hasInventoryRevision(admitted[0].peerIdentity, ActorId, 3),
		"failed native capture did not advance or publish a private revision");

	const auto oldBaseline = DecodeLastBaseline(clients[1]);
	CoopTacticalResyncRequest resync;
	resync.acceptedState = oldBaseline.state;
	resync.requestId = 1;
	resync.acceptedBaselineId = oldBaseline.baselineId;
	resync.lastPayloadChecksum = oldBaseline.payloadChecksum;
	resync.nextExpectedCommandId = 1;
	resync.reason = CoopTacticalResyncReason::ReplicaRejected;
	CoopTacticalResyncRequestBytes resyncBytes{};
	CHECK(EncodeCoopTacticalResyncRequest(resync, resyncBytes) == CoopTacticalCodecResult::Success &&
		clients[1].peer->SendMessage(CoopTacticalResyncRequestMessageName, resyncBytes.data(), resyncBytes.size(),
			clients[1].server, false) && QueueArrived(listener, clients[1]) &&
		server.pumpInbound().result == FullEngineCoopTacticalServerResult::Success &&
		server.replication().peerPhase(admitted[1].peerIdentity) == CoopTacticalPeerPhase::ResyncRequired &&
		!server.hasInventoryRevision(admitted[1].peerIdentity, assignments[1].actor, 1),
		"same-connection resync revokes the prior owner's private token");
	auto& otherActor = *GetJa2SoldierRepository().resolve(2);
	otherActor.roster().inSector() = FALSE;
	CHECK(StageDedicatedCoopOwnerInventories(live, server, WorldGeneration),
		"quarantined peers cannot capture a stale native assignment");
	otherActor.roster().inSector() = TRUE;
	CHECK(server.stageBaseline(admitted[1].peerIdentity, Snapshot(WorldGeneration, TurnSerial, 2)) ==
		FullEngineCoopTacticalServerResult::Success && StageDedicatedCoopOwnerInventories(live, server, WorldGeneration) &&
		server.flushOutbound().result == FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&clients[1]}, [&] { return clients[1].baseline.messages.size() == 2; }) &&
		clients[1].inventory.messages.size() == 1 &&
		SendBaselineAck(clients[1], DecodeLastBaseline(clients[1]), admitted[1].peerIdentity) &&
		QueueArrived(listener, clients[1]) && server.pumpInbound().acknowledgementsAccepted == 1 &&
		WaitUntil(listener, {&clients[1]}, [&] { return clients[1].inventory.messages.size() == 2; }) &&
		DecodeLastInventory(clients[1]).baselineId != oldBaseline.baselineId &&
		DecodeLastInventory(clients[1]).inventoryRevision == 1,
		"fresh baseline ACK restores unchanged native contents under a newly bound transport token");

	listener.stop(20);
	CHECK(server.reconcilePeers() == FullEngineCoopTacticalServerResult::Success &&
		!server.hasInventoryRevision(admitted[0].peerIdentity, ActorId, 3),
		"disconnect revokes sent native tokens");
	actor.roster().inSector() = FALSE;
	CHECK(StageDedicatedCoopOwnerInventories(live, server, WorldGeneration),
		"disconnected owners never recapture actors that left the world");
	actor.roster().inSector() = TRUE;
	CHECK(server.endWorld() == FullEngineCoopTacticalServerResult::Success &&
		!StageDedicatedCoopOwnerInventories(live, server, WorldGeneration),
		"ended world disables runtime owner staging");
	for (auto& client : clients) DestroyClient(client);
	CoopOwnerInventorySnapshot reset;
	CHECK(ReleaseJa2TacticalEntity(actor), "retire captured native actor");
	actor.identity().incarnation() = 2;
	CHECK(AdoptJa2TacticalEntity(actor) && !live.captureInventory(ActorId, WorldGeneration, reset) &&
		live.captureInventory(GetJa2TacticalEntityId(actor), WorldGeneration, reset) && reset.inventoryRevision == 1,
		"runtime-owned capture ledger resets for actor reincarnation without reviving the old identity");
	NotifyJa2TacticalWorldLoaded(2);
	CHECK(live.captureInventory(GetJa2TacticalEntityId(actor), 2, reset) && reset.inventoryRevision == 1 &&
		reset.worldGeneration == 2, "runtime-owned ledger starts fresh for the next native world");
}
}

int main()
{
	GameContext& game = GetGameContext();
	if (!game.beginInitialization() || !game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) || !game.markRunning())
		return 1;
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	ResetJa2TacticalActorRosters();
	ResetJa2TacticalInterruptForNewWorld();
	NotifyJa2TacticalWorldLoaded(WorldGeneration);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM;
	is_networked = is_client = is_server = false;
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	gMAXITEMS_READ = FIRSTAIDKIT + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
	for (unsigned index = 1; index <= 2; ++index)
	{
		TacticalActor& actor = *repository.resolve(index);
		actor.identity().id() = SoldierID{static_cast<UINT16>(index)};
		actor.identity().incarnation() = 1;
		actor.roster().active() = actor.roster().inSector() = TRUE;
		actor.roster().team() = OUR_TEAM;
		CHECK(AdoptJa2TacticalEntity(actor), "native owner actor is adopted");
		MakeObject(actor.inventory()[BIGPOCK1POS], FIRSTAIDKIT, 73 - (index - 1) * 20);
	}
	TestCommittedOwnerPublication(game);
	CHECK(game.commands().empty() && game.commandJournal().size() == 0,
		"owner publication issues no native command and performs no client simulation");
	NotifyJa2TacticalWorldUnloaded();
	std::printf("native owner inventory runtime: %s\n", failures == 0 ? "PASS" : "FAIL");
	return failures ? 1 : 0;
}
