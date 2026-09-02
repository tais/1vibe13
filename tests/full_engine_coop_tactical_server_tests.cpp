#include "FullEngineCoopTacticalServer.h"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <utility>
#include <vector>

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
constexpr std::uint64_t WorldGeneration = 7;
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

class PublishingExecutionSink final : public TacticalIntentExecutionSink
{
public:
	enum class Mode
	{
		RetainedQueued,
		RejectedTerminal,
		AppliedTerminal,
		NoReceipt
	};

	bool ready() const noexcept override
	{
		++readyCalls;
		return readyToExecute &&
			(blockReadyAtCall == 0 || readyCalls < blockReadyAtCall);
	}

	TacticalIntentExecutionDisposition execute(
		const AuthorizedTacticalIntent& intent) noexcept override
	{
		++calls;
		lastIntent = intent;
		if (server != nullptr && mode != Mode::NoReceipt)
		{
			CoopTacticalIntentReceipt receipt;
			receipt.peerIdentity = intent.peerIdentity;
			receipt.commandId = intent.commandId;
			receipt.simulationTick = 900 + intent.commandId;
			if (mode == Mode::RetainedQueued)
			{
				receipt.status = CoopTacticalIntentReceiptStatus::Queued;
				receipt.reason = CoopTacticalIntentReceiptReason::None;
			}
			else if (mode == Mode::RejectedTerminal)
			{
				receipt.status = CoopTacticalIntentReceiptStatus::Rejected;
				receipt.reason =
					CoopTacticalIntentReceiptReason::ActorUnavailable;
			}
			else
			{
				receipt.status = CoopTacticalIntentReceiptStatus::Applied;
				receipt.reason = CoopTacticalIntentReceiptReason::None;
			}
			recordResult = server->recordReceipt(receipt);
			reentrantFlushResult = server->flushOutbound().result;
		}
		switch (mode)
		{
			case Mode::RetainedQueued:
				return TacticalIntentExecutionDisposition::Retained;
			case Mode::RejectedTerminal:
				return TacticalIntentExecutionDisposition::Rejected;
			case Mode::AppliedTerminal:
				return TacticalIntentExecutionDisposition::Applied;
			case Mode::NoReceipt:
				return TacticalIntentExecutionDisposition::Applied;
		}
		return TacticalIntentExecutionDisposition::Rejected;
	}

	FullEngineCoopTacticalServer* server = nullptr;
	Mode mode = Mode::RetainedQueued;
	bool readyToExecute = true;
	mutable std::size_t readyCalls = 0;
	std::size_t blockReadyAtCall = 0;
	std::size_t calls = 0;
	AuthorizedTacticalIntent lastIntent;
	FullEngineCoopTacticalServerResult recordResult =
		FullEngineCoopTacticalServerResult::InternalFailure;
	FullEngineCoopTacticalServerResult reentrantFlushResult =
		FullEngineCoopTacticalServerResult::InternalFailure;
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
		TacticalSectorSnapshot{9, 2, 0, true},
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
	Capture receipt;
	Capture retirement;
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
		!client.peer->RegisterMessage(CoopTacticalIntentReceiptMessageName,
			CaptureMessage, &client.receipt) ||
		!client.peer->RegisterMessage(
			CoopAdmissionSelfRetirementResultMessageName,
			CaptureMessage, &client.retirement))
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

bool SendSelfRetirement(Client& client, std::uint64_t requestId)
{
	AdmissionSelfRetirementRequest request;
	request.sessionEpoch = SessionEpoch;
	request.requestId = requestId;
	AdmissionSelfRetirementRequestBytes bytes{};
	return EncodeAdmissionSelfRetirementRequest(request, bytes) &&
		client.peer->SendMessage(
			CoopAdmissionSelfRetirementRequestMessageName,
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

bool SendDeltaAck(Client& client, const CoopTacticalDelta& delta,
	const PeerIdentity& peer)
{
	CoopTacticalDeltaAck acknowledgement;
	acknowledgement.state = delta.state;
	acknowledgement.peerIdentity = peer;
	acknowledgement.deltaId = delta.deltaId;
	acknowledgement.payloadChecksum = delta.payloadChecksum;
	CoopTacticalDeltaAckBytes bytes{};
	if (EncodeCoopTacticalDeltaAck(acknowledgement, bytes) !=
		CoopTacticalCodecResult::Success) return false;
	return client.peer->SendMessage(CoopTacticalDeltaAckMessageName,
		bytes.data(), bytes.size(), client.server, false);
}

bool QueueArrived(FullEngineCoopAdmissionListener& listener,
	Client& client, std::size_t count = 1)
{
	return WaitUntil(listener, {&client}, [&] {
		return listener.pendingInboundCount() >= count;
	});
}

bool CompleteCapturedSelfRetirement(FullEngineCoopIngress& ingress,
	FullEngineCoopAdmissionListener& listener,
	FullEngineCoopTacticalServer& server, Client& client,
	std::uint64_t requestId,
	FullEngineCoopSelfRetirementInbound& captured)
{
	if (!listener.popSelfRetirement(captured) ||
		server.discardInboundAfterSelfRetirementGate() !=
			FullEngineCoopTacticalServerResult::Success)
		return false;
	AdmissionSelfRetirementResult result;
	result.sessionEpoch = captured.request.sessionEpoch;
	result.requestId = captured.request.requestId;
	result.peerIdentity = captured.peerIdentity;
	result.result = AdmissionSelfRetirementResultCode::CredentialRetired;
	AdmissionSelfRetirementResultBytes bytes{};
	if (!EncodeAdmissionSelfRetirementResult(result, bytes) ||
		listener.sendCommittedSelfRetirementResult(captured, bytes) ||
		ingress.completeSelfRetirement(captured.peerIdentity, requestId) !=
			AdmissionSelfRetirementRegistryResult::Success ||
		!listener.sendCommittedSelfRetirementResult(captured, bytes) ||
		!WaitUntil(listener, {&client}, [&] {
			return client.retirement.messages.size() == 1;
		}))
		return false;
	AdmissionSelfRetirementResult decoded;
	return DecodeAdmissionSelfRetirementResult(
		client.retirement.messages[0].data(),
		client.retirement.messages[0].size(), decoded) == DecodeResult::Ok &&
		decoded.result == AdmissionSelfRetirementResultCode::CredentialRetired &&
		decoded.requestId == requestId &&
		decoded.peerIdentity == captured.peerIdentity;
}

bool CommitSelfRetirement(FullEngineCoopIngress& ingress,
	FullEngineCoopAdmissionListener& listener,
	FullEngineCoopTacticalServer& server, Client& client,
	std::uint64_t requestId,
	FullEngineCoopSelfRetirementInbound& captured)
{
	if (!SendSelfRetirement(client, requestId) ||
		!WaitUntil(listener, {&client}, [&] {
			return listener.selfRetirementInputFrozen() &&
				ingress.pendingSelfRetirementCount() == 1;
		}))
		return false;
	return CompleteCapturedSelfRetirement(ingress, listener, server, client,
		requestId, captured);
}

bool SameCommandState(const FullEngineCoopTacticalPeerCommandState& left,
	const FullEngineCoopTacticalPeerCommandState& right)
{
	return left.peerIdentity == right.peerIdentity &&
		left.transport == right.transport &&
		left.nextExpectedCommandId == right.nextExpectedCommandId &&
		left.connected == right.connected && left.exhausted == right.exhausted &&
		left.pendingCommands == right.pendingCommands;
}

bool SameReplicationState(const CoopTacticalPeerReplicationState& left,
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

void TestRetirementBeforeCampaignReadyHasNoTacticalSlot()
{
	SequentialTokenSource tokens;
	PublishingExecutionSink execution;
	FullEngineCoopIngress ingress(tokens, execution);
	const AuthorityConfiguration authority = Authority(4);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"pre-campaign retirement admission epoch begins");
	FullEngineCoopAdmissionListener listener(ingress);
	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration;
	listenerConfiguration.campaignBootstrap = Bootstrap(authority);
	CHECK(StartListener(listener, listenerConfiguration),
		"pre-campaign retirement listener starts");
	Client client;
	AdmissionResponse admitted;
	CHECK(StartClient(client, listenerConfiguration.endpoint.port) &&
		WaitUntil(listener, {&client}, [&] {
			return client.connected && client.hello.messages.size() == 1;
		}), "pre-campaign retirement client receives server hello");
	const AdmissionRequestBytes admission = AdmissionBytes(authority);
	CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
			admission.data(), admission.size(), client.server, false) &&
		WaitUntil(listener, {&client}, [&] {
			return client.admission.messages.size() == 1;
		}) && DecodeAdmissionResponse(client.admission.messages.back().data(),
			client.admission.messages.back().size(), admitted) == DecodeResult::Ok &&
		admitted.admitted(),
		"client receives a seat but has not ACK-authenticated it yet");
	FullEngineCoopTacticalServer server(ingress, listener);
	execution.server = &server;
	CHECK(server.beginEpoch(SessionEpoch) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.beginWorld(WorldGeneration, InitialRevision, TurnSerial) ==
			FullEngineCoopTacticalServerResult::Success,
		"pre-campaign tactical coordinator is active with an empty peer table");
	FullEngineCoopTacticalPeerCommandState absent;
	CHECK(!server.peerCommandState(admitted.peerIdentity, absent) &&
		server.replication().peerCount() == 0,
		"admission ACK alone creates no tactical/coordinator record");
	const AdmissionAckBytes acknowledgement = AdmissionAckFor(admitted);
	constexpr std::uint64_t requestId = UINT64_C(0x7001);
	CHECK(client.peer->SendMessage(CoopAdmissionAckMessageName,
			acknowledgement.data(), acknowledgement.size(), client.server, false) &&
		SendSelfRetirement(client, requestId),
		"ACK and retirement are queued FIFO before the listener drains either");
	CHECK(WaitUntil(listener, {&client}, [&] {
		return listener.selfRetirementInputFrozen() &&
			ingress.pendingSelfRetirementCount() == 1;
	}), "one listener drain may authenticate ACK then capture retirement");
	FullEngineCoopSelfRetirementInbound captured;
	CHECK(CompleteCapturedSelfRetirement(ingress, listener, server, client,
			requestId, captured),
		"ACK-authenticated retirement commits before campaign readiness");
	listener.stop(20);
	CHECK(server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.retirePeer(admitted.peerIdentity) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replication().peerCount() == 0 &&
		!server.peerCommandState(admitted.peerIdentity, absent),
		"absent tactical state is an idempotent committed-boundary no-op");
	DestroyClient(client);

	CHECK(StartListener(listener, listenerConfiguration),
		"listener restarts after pre-campaign retirement");
	Client replacement;
	AdmissionResponse replacementAdmission;
	CHECK(StartClient(replacement, listenerConfiguration.endpoint.port) &&
		Admit(listener, replacement, authority, replacementAdmission) &&
		replacementAdmission.peerIdentity != admitted.peerIdentity &&
		server.setCampaignReadyPeers(
			&replacementAdmission.peerIdentity, 1) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.stageBaseline(replacementAdmission.peerIdentity, Snapshot()) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&replacement}, [&] {
			return replacement.baseline.messages.size() == 1;
		}) && DecodeLastBaseline(replacement).nextExpectedCommandId == 1,
		"distinct replacement takes the freed seat through a fresh baseline");
	listener.stop(20);
	CHECK(server.reconcilePeers() ==
		FullEngineCoopTacticalServerResult::Success,
		"pre-campaign replacement disconnects cleanly");
	DestroyClient(replacement);
}

void TestFullRosterRetirementCompactsAllPeerState()
{
	SequentialTokenSource tokens;
	PublishingExecutionSink execution;
	FullEngineCoopIngress ingress(tokens, execution);
	const AuthorityConfiguration authority = Authority(4);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"full-roster retirement admission epoch begins");
	FullEngineCoopAdmissionListener listener(ingress);
	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration;
	listenerConfiguration.campaignBootstrap = Bootstrap(authority);
	CHECK(StartListener(listener, listenerConfiguration),
		"full-roster retirement listener starts");
	std::array<Client, MaximumAuthorityPeers> clients{};
	std::array<AdmissionResponse, MaximumAuthorityPeers> admitted{};
	for (std::size_t index = 0; index < clients.size(); ++index)
		CHECK(StartClient(clients[index], listenerConfiguration.endpoint.port) &&
			Admit(listener, clients[index], authority, admitted[index]),
			"each fixed roster seat authenticates");

	FullEngineCoopTacticalServer server(ingress, listener);
	execution.server = &server;
	execution.mode = PublishingExecutionSink::Mode::AppliedTerminal;
	std::array<PeerIdentity, MaximumAuthorityPeers> ready{};
	std::array<CoopTacticalActorAssignment, MaximumAuthorityPeers> assignments{};
	for (std::size_t index = 0; index < ready.size(); ++index)
	{
		ready[index] = admitted[index].peerIdentity;
		assignments[index] = CoopTacticalActorAssignment{
			TacticalEntityId{static_cast<std::uint16_t>(index + 1), 1},
			admitted[index].peerIdentity};
	}
	std::sort(ready.begin(), ready.end());
	const TacticalWorldSnapshot snapshot = Snapshot(
		WorldGeneration, TurnSerial, MaximumAuthorityPeers);
	CHECK(server.beginEpoch(SessionEpoch) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.beginWorld(WorldGeneration, InitialRevision, TurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.setCampaignReadyPeers(ready.data(), ready.size()) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replaceAssignments(assignments.data(), assignments.size()) ==
			FullEngineCoopTacticalServerResult::Success,
		"four-peer roster publishes deterministic assignments");
	for (std::size_t index = 0; index < clients.size(); ++index)
		CHECK(server.stageBaseline(admitted[index].peerIdentity, snapshot) ==
			FullEngineCoopTacticalServerResult::Success,
			"each full-roster peer stages a baseline");
	CHECK(server.flushOutbound().result ==
		FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener,
			{&clients[0], &clients[1], &clients[2], &clients[3]}, [&] {
				return std::all_of(clients.begin(), clients.end(),
					[](const Client& client) {
						return client.baseline.messages.size() == 1;
					});
			}), "all full-roster baselines flush");
	for (std::size_t index = 0; index < clients.size(); ++index)
		CHECK(SendBaselineAck(clients[index],
			DecodeLastBaseline(clients[index]), admitted[index].peerIdentity),
			"each full-roster baseline ACK queues");
	CHECK(WaitUntil(listener,
			{&clients[0], &clients[1], &clients[2], &clients[3]}, [&] {
				return listener.pendingInboundCount() == clients.size();
			}) && server.pumpInbound().acknowledgementsAccepted == clients.size(),
		"full roster becomes tactically authoritative");
	for (std::size_t index = 0; index < clients.size(); ++index)
		CHECK(SendIntent(clients[index], admitted[index].peerIdentity, 1,
			InitialRevision,
			TacticalEntityId{static_cast<std::uint16_t>(index + 1), 1}),
			"every identity consumes an authority sequence slot");
	CHECK(WaitUntil(listener,
			{&clients[0], &clients[1], &clients[2], &clients[3]}, [&] {
				return listener.pendingInboundCount() == clients.size();
			}) && server.pumpInbound().intentsConsumed == clients.size() &&
		execution.calls == clients.size(),
		"all four command-issuing identities settle terminal receipts");

	FullEngineCoopSelfRetirementInbound captured;
	CHECK(CommitSelfRetirement(ingress, listener, server, clients[0],
			UINT64_C(0x7002), captured),
		"one peer commits self-retirement from the full roster");
	CHECK(server.reconcilePeers() ==
		FullEngineCoopTacticalServerResult::Success,
		"pending retirement revokes only the target while survivors remain live");
	FullEngineCoopTacticalPeerCommandState targetBeforeRejectedRetirement;
	FullEngineCoopTacticalPeerCommandState survivorBeforeRejectedRetirement;
	CoopTacticalPeerReplicationState targetReplicationBeforeRejectedRetirement;
	CoopTacticalPeerReplicationState survivorReplicationBeforeRejectedRetirement;
	CHECK(server.peerCommandState(admitted[0].peerIdentity,
			targetBeforeRejectedRetirement) &&
		server.peerCommandState(admitted[1].peerIdentity,
			survivorBeforeRejectedRetirement) &&
		server.replication().peerState(admitted[0].peerIdentity,
			targetReplicationBeforeRejectedRetirement) &&
		server.replication().peerState(admitted[1].peerIdentity,
			survivorReplicationBeforeRejectedRetirement) &&
		!targetBeforeRejectedRetirement.connected &&
		!targetReplicationBeforeRejectedRetirement.connected &&
		survivorBeforeRejectedRetirement.connected &&
		survivorReplicationBeforeRejectedRetirement.connected,
		"retirement atomicity fixture has an offline target and live survivor");
	listener.stop(20);
	CHECK(server.retirePeer(admitted[0].peerIdentity) ==
		FullEngineCoopTacticalServerResult::InvalidContext,
		"compaction rejects until every survivor mapping is reconciled offline");
	FullEngineCoopTacticalPeerCommandState targetAfterRejectedRetirement;
	FullEngineCoopTacticalPeerCommandState survivorAfterRejectedRetirement;
	CoopTacticalPeerReplicationState targetReplicationAfterRejectedRetirement;
	CoopTacticalPeerReplicationState survivorReplicationAfterRejectedRetirement;
	CHECK(server.peerCommandState(admitted[0].peerIdentity,
			targetAfterRejectedRetirement) &&
		server.peerCommandState(admitted[1].peerIdentity,
			survivorAfterRejectedRetirement) &&
		server.replication().peerState(admitted[0].peerIdentity,
			targetReplicationAfterRejectedRetirement) &&
		server.replication().peerState(admitted[1].peerIdentity,
			survivorReplicationAfterRejectedRetirement) &&
		SameCommandState(targetBeforeRejectedRetirement,
			targetAfterRejectedRetirement) &&
		SameCommandState(survivorBeforeRejectedRetirement,
			survivorAfterRejectedRetirement) &&
		SameReplicationState(targetReplicationBeforeRejectedRetirement,
			targetReplicationAfterRejectedRetirement) &&
		SameReplicationState(survivorReplicationBeforeRejectedRetirement,
			survivorReplicationAfterRejectedRetirement),
		"rejected compaction leaves target and survivor state byte-exact");
	CHECK(server.reconcilePeers() ==
		FullEngineCoopTacticalServerResult::Success,
		"listener stop reconciles all restartable wire obligations");
	FullEngineCoopTacticalPeerCommandState survivorCommandBefore;
	CoopTacticalPeerReplicationState survivorReplicationBefore;
	CHECK(server.peerCommandState(admitted[1].peerIdentity,
			survivorCommandBefore) &&
		server.replication().peerState(admitted[1].peerIdentity,
			survivorReplicationBefore),
		"survivor state is captured immediately before compaction");
	CHECK(server.retirePeer(admitted[0].peerIdentity) ==
		FullEngineCoopTacticalServerResult::Success,
		"coordinator compacts replication, command, ACL, and authority state");
	FullEngineCoopTacticalPeerCommandState survivorCommandAfter;
	CoopTacticalPeerReplicationState survivorReplicationAfter;
	FullEngineCoopTacticalPeerCommandState retiredCommand;
	CoopTacticalPeerReplicationState retiredReplication;
	CHECK(!server.peerCommandState(admitted[0].peerIdentity, retiredCommand) &&
		!server.replication().peerState(
			admitted[0].peerIdentity, retiredReplication) &&
		server.peerCommandState(admitted[1].peerIdentity,
			survivorCommandAfter) &&
		server.replication().peerState(admitted[1].peerIdentity,
			survivorReplicationAfter) &&
		SameCommandState(survivorCommandBefore, survivorCommandAfter) &&
		SameReplicationState(
			survivorReplicationBefore, survivorReplicationAfter),
		"retiree disappears while survivor cursor and receipt history remain exact");
	for (Client& client : clients) DestroyClient(client);

	CHECK(StartListener(listener, listenerConfiguration),
		"listener restarts after full-roster compaction");
	Client fifth;
	Client survivor;
	AdmissionResponse fifthAdmission;
	AdmissionResponse survivorAdmission;
	CHECK(StartClient(fifth, listenerConfiguration.endpoint.port) &&
		Admit(listener, fifth, authority, fifthAdmission) &&
		fifthAdmission.peerIdentity != admitted[0].peerIdentity &&
		StartClient(survivor, listenerConfiguration.endpoint.port) &&
		Admit(listener, survivor, authority, survivorAdmission, &admitted[1]) &&
		survivorAdmission.peerIdentity == admitted[1].peerIdentity,
		"distinct fifth identity and retained survivor authenticate after restart");
	ready = {admitted[1].peerIdentity, admitted[2].peerIdentity,
		admitted[3].peerIdentity, fifthAdmission.peerIdentity};
	std::sort(ready.begin(), ready.end());
	assignments = {{
		{TacticalEntityId{1, 1}, fifthAdmission.peerIdentity},
		{TacticalEntityId{2, 1}, admitted[1].peerIdentity},
		{TacticalEntityId{3, 1}, admitted[2].peerIdentity},
		{TacticalEntityId{4, 1}, admitted[3].peerIdentity}}};
	CHECK(server.setCampaignReadyPeers(ready.data(), ready.size()) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replaceAssignments(assignments.data(), assignments.size()) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.stageBaseline(fifthAdmission.peerIdentity, snapshot) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.stageBaseline(admitted[1].peerIdentity, snapshot) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&fifth, &survivor}, [&] {
			return fifth.baseline.messages.size() == 1 &&
				survivor.baseline.messages.size() == 1;
		}), "replacement and survivor receive fresh baselines");
	const CoopTacticalBaseline fifthBaseline = DecodeLastBaseline(fifth);
	const CoopTacticalBaseline survivorBaseline = DecodeLastBaseline(survivor);
	CHECK(fifthBaseline.nextExpectedCommandId == 1 &&
		survivorBaseline.nextExpectedCommandId == 2 &&
		SendBaselineAck(fifth, fifthBaseline, fifthAdmission.peerIdentity) &&
		SendBaselineAck(survivor, survivorBaseline, admitted[1].peerIdentity) &&
		WaitUntil(listener, {&fifth, &survivor}, [&] {
			return listener.pendingInboundCount() == 2;
		}) && server.pumpInbound().acknowledgementsAccepted == 2,
		"fifth starts fresh while survivor resumes its exact command cursor");
	const std::size_t callsBefore = execution.calls;
	CHECK(SendIntent(fifth, fifthAdmission.peerIdentity, 1, InitialRevision,
			TacticalEntityId{1, 1}) &&
		SendIntent(survivor, admitted[1].peerIdentity, 2, InitialRevision,
			TacticalEntityId{2, 1}) &&
		WaitUntil(listener, {&fifth, &survivor}, [&] {
			return listener.pendingInboundCount() == 2;
		}) && server.pumpInbound().intentsConsumed == 2 &&
		execution.calls == callsBefore + 2,
		"freed authority slot admits the fifth command and preserves survivor sequencing");
	listener.stop(20);
	CHECK(server.reconcilePeers() ==
		FullEngineCoopTacticalServerResult::Success,
		"full-roster replacement fixture disconnects cleanly");
	DestroyClient(fifth);
	DestroyClient(survivor);
}

void TestCoordinatorEndToEnd()
{
	SequentialTokenSource tokens;
	PublishingExecutionSink execution;
	FullEngineCoopIngress ingress(tokens, execution);
	const AuthorityConfiguration authority = Authority();
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"coordinator admission epoch begins");
	FullEngineCoopAdmissionListener listener(ingress);
	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration;
	listenerConfiguration.campaignBootstrap = Bootstrap(authority);
	listenerConfiguration.maximumQueuedTacticalMessages = 4;
	CHECK(StartListener(listener, listenerConfiguration),
		"coordinator listener starts on loopback");

	Client client;
	CHECK(StartClient(client, listenerConfiguration.endpoint.port),
		"coordinator client connects");
	AdmissionResponse admitted;
	CHECK(Admit(listener, client, authority, admitted),
		"coordinator client authenticates and ACKs admission");

	FullEngineCoopTacticalServerConfiguration serverConfiguration;
	serverConfiguration.replication.maximumDeltaHistory = 2;
	serverConfiguration.replication.maximumInFlightDeltasPerPeer = 1;
	serverConfiguration.replication.maximumMessagesPerFlush = 16;
	serverConfiguration.replication.maximumReceiptHistoryPerPeer = 8;
	serverConfiguration.maximumInboundMessagesPerPump = 4;
	serverConfiguration.maximumTransientReceipts = 4;
	FullEngineCoopTacticalServer server(
		ingress, listener, serverConfiguration);
	execution.server = &server;
	CHECK(server.beginEpoch(SessionEpoch) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.beginWorld(WorldGeneration, InitialRevision, TurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success,
		"coordinator begins one admission epoch and tactical world");
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> readyPeers{};
	CHECK(server.campaignReadyPeers(readyPeers) == 0 &&
		server.replication().connectedPeerCount() == 0,
		"campaign-ready tactical allowlist is fail-closed by default");
	CHECK(SendIntent(client, admitted.peerIdentity, 1, InitialRevision) &&
		QueueArrived(listener, client),
		"admitted but campaign-unready intent reaches isolated FIFO");
	const FullEngineCoopTacticalServerPumpResult campaignBlocked =
		server.pumpInbound(99);
	FullEngineCoopTacticalPeerCommandState commandState;
	CHECK(campaignBlocked.inputsRejected == 1 &&
		campaignBlocked.intentsConsumed == 0 && execution.calls == 0 &&
		!server.peerCommandState(admitted.peerIdentity, commandState),
		"campaign-unready intent retires without peer activation or cursor consumption");
	PeerIdentity zeroPeer{};
	CHECK(server.setCampaignReadyPeers(&zeroPeer, 1) ==
			FullEngineCoopTacticalServerResult::InvalidPeerSet &&
		server.campaignReadyPeers(readyPeers) == 0,
		"invalid ready set preserves the fail-closed allowlist");
	CHECK(server.setCampaignReadyPeers(&admitted.peerIdentity, 1) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.campaignReadyPeers(readyPeers) == 1 &&
		readyPeers[0] == admitted.peerIdentity &&
		server.replication().connectedPeerCount() == 1,
		"committed campaign peer enters tactical replication through the allowlist");
	const CoopTacticalActorAssignment assignment{ActorId,
		admitted.peerIdentity};
	CHECK(server.replaceAssignments(&assignment, 1) ==
		FullEngineCoopTacticalServerResult::Success &&
		ingress.actorBindingCount() == 0,
		"assignment stays outside ingress ACL before exact baseline ACK");

	CHECK(SendIntent(client, admitted.peerIdentity, 1, InitialRevision) &&
		QueueArrived(listener, client),
		"pre-baseline exact-expected intent reaches bounded queue");
	const FullEngineCoopTacticalServerPumpResult early =
		server.pumpInbound(100);
	CHECK(early.intentsConsumed == 0 && early.inputsRejected == 1 &&
		execution.calls == 0 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 1,
		"pre-baseline intent is dropped without consuming its command cursor");

	const TacticalWorldSnapshot snapshot = Snapshot();
	CHECK(server.stageBaseline(admitted.peerIdentity, snapshot) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&client}, [&] {
			return client.baseline.messages.size() == 1;
		}), "current baseline flushes without advancing the rejected cursor");
	CoopTacticalBaseline baseline = DecodeLastBaseline(client);
	CHECK(baseline.nextExpectedCommandId == 1 &&
		baseline.assignedActors == std::vector<TacticalEntityId>{ActorId},
		"baseline publishes exact cursor and deterministic assignment");
	CHECK(SendIntent(client, admitted.peerIdentity, 1, InitialRevision) &&
		QueueArrived(listener, client),
		"exact-cursor intent can arrive while baseline ACK is outstanding");
	execution.readyToExecute = false;
	const FullEngineCoopTacticalServerPumpResult overlappingBaseline =
		server.pumpInbound(100);
	CHECK(overlappingBaseline.inputsRejected == 1 && execution.calls == 0 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 1 &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::AwaitingBaselineAck &&
		client.baseline.messages.size() == 1,
		"awaiting-baseline intent bypasses unavailable execution and cannot replace the sent baseline");
	execution.readyToExecute = true;
	PeerIdentity forged = admitted.peerIdentity;
	forged[0] ^= 0x40;
	CHECK(SendBaselineAck(client, baseline, forged) &&
		QueueArrived(listener, client),
		"forged payload identity reaches authenticated ACK queue");
	const FullEngineCoopTacticalServerPumpResult forgedResult =
		server.pumpInbound();
	CHECK(forgedResult.inputsRejected == 1 &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::AwaitingBaselineAck &&
		ingress.actorBindingCount() == 0,
		"transport identity defeats a forged baseline ACK identity");
	CHECK(server.setCampaignReadyPeers(nullptr, 0) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::Offline &&
		ingress.actorBindingCount() == 0,
		"campaign readiness loss immediately disconnects tactical replication");
	CHECK(SendBaselineAck(client, baseline, admitted.peerIdentity) &&
		QueueArrived(listener, client),
		"old tactical ACK queues while campaign readiness is absent");
	const FullEngineCoopTacticalServerPumpResult blockedAck =
		server.pumpInbound();
	CHECK(blockedAck.inputsRejected == 1 &&
		blockedAck.acknowledgementsAccepted == 0 &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::Offline &&
		ingress.actorBindingCount() == 0,
		"campaign-unready ACK cannot reactivate replication or ACL");
	CHECK(server.setCampaignReadyPeers(&admitted.peerIdentity, 1) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::NeedsBaseline &&
		server.stageBaseline(admitted.peerIdentity, snapshot) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&client}, [&] {
			return client.baseline.messages.size() == 2;
		}), "readiness restoration requires a newly staged baseline");
	baseline = DecodeLastBaseline(client);
	CHECK(SendBaselineAck(client, baseline, admitted.peerIdentity) &&
		QueueArrived(listener, client),
		"fresh exact baseline ACK returns through ready transport");
	CHECK(server.pumpInbound().acknowledgementsAccepted == 1 &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::Active &&
		ingress.actorBindingCount() == 1,
		"exact baseline ACK activates ACL without a stale pre-baseline receipt");
	execution.mode = PublishingExecutionSink::Mode::AppliedTerminal;
	CHECK(SendIntent(client, admitted.peerIdentity, 1, InitialRevision) &&
		QueueArrived(listener, client) && server.pumpInbound(100).intentsConsumed == 1 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 2 &&
		WaitUntil(listener, {&client}, [&] {
			return !client.receipt.messages.empty();
		}),
		"client retries the same cursor after baseline ACK and advances exactly once");
	CoopTacticalIntentReceipt receipt = DecodeLastReceipt(client);
	execution.calls = 0;
	CHECK(server.replaceAssignments(&assignment, 1) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::Active &&
		ingress.actorBindingCount() == 1,
		"identical assignment publication preserves the committed baseline and ACL");

	execution.mode = PublishingExecutionSink::Mode::RetainedQueued;
	const std::size_t beforeQueued = client.receipt.messages.size();
	CHECK(SendIntent(client, admitted.peerIdentity, 2, InitialRevision) &&
		QueueArrived(listener, client),
		"active exact-expected intent queues");
	const FullEngineCoopTacticalServerPumpResult accepted =
		server.pumpInbound(101);
	CHECK(accepted.intentsConsumed == 1 && execution.calls == 1 &&
		execution.recordResult == FullEngineCoopTacticalServerResult::Success &&
		execution.reentrantFlushResult ==
			FullEngineCoopTacticalServerResult::Busy &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 3 &&
		commandState.pendingCommands == 1,
		"reserved slot accepts synchronous host Queued receipt during execute");
	CHECK(WaitUntil(listener, {&client}, [&] {
		return client.receipt.messages.size() == beforeQueued + 1;
	}), "host-owned Queued receipt reaches the client");
	const std::size_t afterQueued = client.receipt.messages.size();
	CHECK(SendIntent(client, admitted.peerIdentity, 3, InitialRevision) &&
		QueueArrived(listener, client),
		"pipelined exact-next command reaches the untrusted boundary");
	const FullEngineCoopTacticalServerPumpResult pipelineRejected =
		server.pumpInbound(102);
	CHECK(pipelineRejected.intentsConsumed == 0 && execution.calls == 1 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 3 &&
		commandState.pendingCommands == 1 &&
		WaitUntil(listener, {&client}, [&] {
			return client.receipt.messages.size() == afterQueued + 1;
		}),
		"one retained command rejects same-peer pipelining without consuming "
		"the cursor or entering gameplay");
	receipt = DecodeLastReceipt(client);
	CHECK(receipt.commandId == 3 &&
		receipt.nextExpectedCommandId == 3 &&
		receipt.status == CoopTacticalIntentReceiptStatus::Rejected &&
		receipt.reason ==
			CoopTacticalIntentReceiptReason::InvalidCommandSequence,
		"pipelining rejection reports the unchanged authoritative cursor");
	const std::size_t afterPipelineRejection = client.receipt.messages.size();
	CHECK(SendIntent(client, admitted.peerIdentity, 2, InitialRevision) &&
		QueueArrived(listener, client),
		"duplicate retained command arrives");
	CHECK(server.pumpInbound().duplicateReceiptsReplayed == 1 &&
		execution.calls == 1 &&
		WaitUntil(listener, {&client}, [&] {
			return client.receipt.messages.size() == afterPipelineRejection + 1;
		}), "duplicate replays cached receipt without a second execution");

	CHECK(server.publishDelta(EmptyDelta(), 21, TurnSerial) ==
		FullEngineCoopTacticalServerResult::Success,
		"committed state delta advances authority and replication together");
	CoopTacticalIntentReceipt terminal;
	terminal.peerIdentity = admitted.peerIdentity;
	terminal.commandId = 2;
	terminal.status = CoopTacticalIntentReceiptStatus::Applied;
	terminal.reason = CoopTacticalIntentReceiptReason::None;
	terminal.simulationTick = 1234;
	CHECK(server.recordReceipt(terminal) ==
		FullEngineCoopTacticalServerResult::Success,
		"host terminal receipt replaces retained Queued receipt");
	const std::size_t receiptBeforeTerminal = client.receipt.messages.size();
	const std::size_t orderStart = client.order.size();
	CHECK(server.flushOutbound().result ==
		FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&client}, [&] {
			return client.delta.messages.size() == 1 &&
			client.receipt.messages.size() == receiptBeforeTerminal + 1;
		}), "delta and terminal receipt reach the client");
	CHECK(client.order.size() >= orderStart + 2 &&
		client.order[orderStart] == "delta" &&
		client.order[orderStart + 1] == "receipt",
		"committed delta is queued before its terminal receipt");
	receipt = DecodeLastReceipt(client);
	CHECK(receipt.commandId == 2 &&
		receipt.status == CoopTacticalIntentReceiptStatus::Applied &&
		receipt.authoritativeSequence == 2 &&
		receipt.nextExpectedCommandId == 3,
		"terminal receipt retains server-owned sequence and command cursor");
	CoopTacticalDelta delta;
	CHECK(DecodeCoopTacticalDelta(client.delta.messages.back(), delta) ==
		CoopTacticalCodecResult::Success &&
		SendDeltaAck(client, delta, admitted.peerIdentity) &&
		QueueArrived(listener, client) &&
		server.pumpInbound().acknowledgementsAccepted == 1,
		"exact cumulative delta ACK restores input readiness");

	execution.mode = PublishingExecutionSink::Mode::RejectedTerminal;
	CHECK(SendIntent(client, admitted.peerIdentity, 3, 21) &&
		QueueArrived(listener, client),
		"host-rejected exact command queues");
	CHECK(server.pumpInbound().intentsConsumed == 1 &&
		execution.calls == 2 && execution.recordResult ==
			FullEngineCoopTacticalServerResult::Success &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 4 &&
		commandState.pendingCommands == 0,
		"retry after the prior terminal consumes once and leaves no obligation");
	CHECK(WaitUntil(listener, {&client}, [&] {
		return DecodeLastReceipt(client).commandId == 3;
	}) && DecodeLastReceipt(client).reason ==
		CoopTacticalIntentReceiptReason::ActorUnavailable,
		"host rejection reason is preserved instead of coordinator duplication");

	execution.mode = PublishingExecutionSink::Mode::AppliedTerminal;
	execution.readyToExecute = false;
	CHECK(SendIntent(client, admitted.peerIdentity, 4, 21) &&
		QueueArrived(listener, client),
		"readiness fixture queues exact intent");
	const FullEngineCoopTacticalServerPumpResult blocked =
		server.pumpInbound();
	CHECK(blocked.backpressured &&
		blocked.result ==
			FullEngineCoopTacticalServerResult::ExecutionBackpressured &&
		listener.pendingInboundCount() == 0 && execution.calls == 2 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 4,
		"not-ready host leaves all inbound bytes and command cursor untouched");
	execution.readyToExecute = true;
	CHECK(server.pumpInbound().intentsConsumed == 1 && execution.calls == 3 &&
		listener.pendingInboundCount() == 0,
		"readiness recovery executes the retained transport frame once");

	std::array<std::uint8_t, TacticalIntentHeaderWireSize> malformed{};
	malformed.fill(0xff);
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
		malformed.data(), malformed.size(), client.server, false) &&
		QueueArrived(listener, client),
		"malformed fixed-bound intent queues for main-thread decode");
	CHECK(server.pumpInbound().inputsRejected == 1 && execution.calls == 3 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 5,
		"malformed ingress never consumes authority sequence");
	CHECK(SendIntent(client, admitted.peerIdentity, 6, 21) &&
		QueueArrived(listener, client),
		"future command ID queues");
	CHECK(server.pumpInbound().result ==
		FullEngineCoopTacticalServerResult::Success &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 5 && execution.calls == 3,
		"future command receives non-consuming rejection without execution");
	execution.mode = PublishingExecutionSink::Mode::NoReceipt;
	CHECK(SendIntent(client, admitted.peerIdentity, 5, 21) &&
		QueueArrived(listener, client),
		"exact cursor remains usable after future-ID rejection");
	const FullEngineCoopTacticalServerPumpResult receiptBlocked =
		server.pumpInbound();
	CHECK(receiptBlocked.result ==
			FullEngineCoopTacticalServerResult::ExecutionBackpressured &&
		receiptBlocked.intentsConsumed == 1 && execution.calls == 4 &&
		listener.pendingInboundCount() == 0 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 6 &&
		commandState.pendingCommands == 1,
		"missing host receipt keeps its reservation without replaying consumed input");
	CoopTacticalIntentReceipt retriedReceipt;
	retriedReceipt.peerIdentity = admitted.peerIdentity;
	retriedReceipt.commandId = 5;
	retriedReceipt.status = CoopTacticalIntentReceiptStatus::Applied;
	retriedReceipt.reason = CoopTacticalIntentReceiptReason::None;
	CHECK(server.recordReceipt(retriedReceipt) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.pendingCommands == 0,
		"host can retry its reserved terminal receipt after pump returns");

	client.peer->Shutdown(20);
	CHECK(WaitUntil(listener, {&client}, [&] {
		return listener.authenticatedPeerCount() == 0;
	}) && server.reconcilePeers() ==
		FullEngineCoopTacticalServerResult::Success &&
		ingress.actorBindingCount() == 0,
		"disconnect removes transport mapping and actor ACL");
	DestroyClient(client);

	Client reconnect;
	CHECK(StartClient(reconnect, listenerConfiguration.endpoint.port),
		"reconnect transport starts");
	AdmissionResponse reconnected;
	CHECK(Admit(listener, reconnect, authority, reconnected, &admitted) &&
		reconnected.peerIdentity == admitted.peerIdentity &&
		server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replication().peerPhase(admitted.peerIdentity) ==
			CoopTacticalPeerPhase::NeedsBaseline,
		"same credential reconnects only into fresh-baseline phase");
	CHECK(server.stageBaseline(admitted.peerIdentity, snapshot) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&reconnect}, [&] {
			return reconnect.baseline.messages.size() == 1;
		}), "reconnect baseline flushes");
	baseline = DecodeLastBaseline(reconnect);
	CHECK(baseline.nextExpectedCommandId == 6,
		"reconnect baseline resynchronizes admission-epoch command cursor");
	CHECK(SendBaselineAck(reconnect, baseline, admitted.peerIdentity) &&
		QueueArrived(listener, reconnect) &&
		server.pumpInbound().acknowledgementsAccepted == 1 &&
		ingress.actorBindingCount() == 1,
		"reconnect exact baseline ACK restores its assignment ACL");

	CHECK(server.publishDelta(EmptyDelta(), 22, TurnSerial) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		server.publishDelta(EmptyDelta(), 23, TurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.publishDelta(EmptyDelta(), 24, TurnSerial) ==
			FullEngineCoopTacticalServerResult::Success,
		"bounded delta history advances while first delta stays unacknowledged");
	std::array<PeerIdentity, MaximumCoopTacticalSessionPeers> needs{};
	CHECK(server.peersNeedingBaseline(needs) == 1 &&
		needs[0] == admitted.peerIdentity && ingress.actorBindingCount() == 0,
		"delta eviction forces deterministic resync and clears actor ACL");
	const FullEngineCoopTacticalServerDrainState beforeEnd =
		server.drainState();
	CHECK(beforeEnd.worldActive && beforeEnd.peersAwaitingReplication == 1 &&
		!beforeEnd.drained(),
		"drain diagnostics expose unfinished world replication");
	CHECK(server.stageBaseline(admitted.peerIdentity, snapshot) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&reconnect}, [&] {
			return reconnect.baseline.messages.size() == 2;
		}), "resync baseline replaces evicted delta history");
	baseline = DecodeLastBaseline(reconnect);
	CHECK(SendBaselineAck(reconnect, baseline, admitted.peerIdentity) &&
		QueueArrived(listener, reconnect) &&
		server.pumpInbound().acknowledgementsAccepted == 1,
		"resync baseline ACK returns peer to active phase");
	execution.mode = PublishingExecutionSink::Mode::RetainedQueued;
	CHECK(SendIntent(reconnect, admitted.peerIdentity, 6, 24) &&
		QueueArrived(listener, reconnect) &&
		server.pumpInbound().intentsConsumed == 1 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.pendingCommands == 1,
		"offline-receipt fixture retains one accepted command");
	reconnect.peer->Shutdown(20);
	CHECK(WaitUntil(listener, {&reconnect}, [&] {
		return listener.authenticatedPeerCount() == 0;
	}) && server.reconcilePeers() ==
		FullEngineCoopTacticalServerResult::Success,
		"offline-receipt peer disconnects before terminal publication");
	terminal = CoopTacticalIntentReceipt{};
	terminal.peerIdentity = admitted.peerIdentity;
	terminal.commandId = 6;
	terminal.status = CoopTacticalIntentReceiptStatus::Applied;
	terminal.reason = CoopTacticalIntentReceiptReason::None;
	CHECK(server.recordReceipt(terminal) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.drainState().pendingCommands == 0 &&
		server.drainState().pendingReplicationReceipts == 0,
		"offline terminal retires host obligation without blocking on delivery");
	CHECK(server.endWorld() == FullEngineCoopTacticalServerResult::Success &&
		!server.worldActive() && !server.drained() &&
		server.transportRestartRequired() &&
		server.takeTransportRestartRequired() &&
		!server.takeTransportRestartRequired() && !server.drained(),
		"offline pending wire receipt does not block world teardown restart");

	listener.stop(20);
	CHECK(server.drained(),
		"checkpoint drain waits for the admission transport to stop");
	DestroyClient(reconnect);
	CHECK(StartListener(listener, listenerConfiguration),
		"transport restarts after the explicit world-unload signal");
	Client nextWorldClient;
	CHECK(StartClient(nextWorldClient, listenerConfiguration.endpoint.port),
		"same peer opens a transport for the next world");
	AdmissionResponse nextWorldAdmission;
	CHECK(Admit(listener, nextWorldClient, authority,
		nextWorldAdmission, &admitted) &&
		nextWorldAdmission.peerIdentity == admitted.peerIdentity,
		"retained admission credential reconnects after world unload");
	CHECK(server.beginWorld(8, 1, 4) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replaceAssignments(&assignment, 1) ==
			FullEngineCoopTacticalServerResult::Success,
		"next tactical generation begins with the retained peer identity");
	const TacticalWorldSnapshot nextSnapshot = Snapshot(8, 4);
	CHECK(server.stageBaseline(admitted.peerIdentity, nextSnapshot) ==
		FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&nextWorldClient}, [&] {
			return nextWorldClient.baseline.messages.size() == 1;
		}), "next-world reconnect receives a fresh baseline");
	CHECK(DecodeLastBaseline(nextWorldClient).nextExpectedCommandId == 7,
		"next-world baseline preserves admission-epoch cursor across offline receipt retirement");
	baseline = DecodeLastBaseline(nextWorldClient);
	CHECK(SendBaselineAck(nextWorldClient, baseline, admitted.peerIdentity) &&
		QueueArrived(listener, nextWorldClient) &&
		server.pumpInbound().acknowledgementsAccepted == 1,
		"teardown fixture activates the next-world peer");
	execution.blockReadyAtCall = execution.readyCalls + 2;
	CHECK(SendIntent(nextWorldClient, admitted.peerIdentity, 7, 1) &&
		QueueArrived(listener, nextWorldClient),
		"teardown fixture queues an exact unconsumed intent");
	const FullEngineCoopTacticalServerPumpResult deferred =
		server.pumpInbound();
	CHECK(deferred.result ==
			FullEngineCoopTacticalServerResult::ExecutionBackpressured &&
		deferred.intentsConsumed == 0 &&
		server.drainState().inboundMessages == 1 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 7,
		"readiness loss between probe and authorization retains one deferred intent without consuming its cursor");
	CHECK(server.discardInboundAfterTransportStop() ==
		FullEngineCoopTacticalServerResult::InvalidContext &&
		server.drainState().inboundMessages == 1,
		"live authenticated transport cannot discard deferred tactical input");
	listener.stop(20);
	CHECK(server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.discardInboundAfterTransportStop() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.drainState().inboundMessages == 0 &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 7,
		"stopped and reconciled transport retires deferred bytes without advancing authority cursor");
	CHECK(server.endWorld() == FullEngineCoopTacticalServerResult::Success &&
		server.takeTransportRestartRequired(),
		"retired deferred input permits the next world to end through the restart contract");
	CHECK(server.drained(),
		"checkpoint drain waits until authenticated transport state is cleared");
	DestroyClient(nextWorldClient);
}

void TestAuthoritySequenceExhaustionFlushesTerminalReceipt()
{
	SequentialTokenSource tokens;
	PublishingExecutionSink execution;
	FullEngineCoopIngress ingress(tokens, execution);
	const AuthorityConfiguration authority = Authority();
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"authority-exhaustion admission epoch begins");
	FullEngineCoopAdmissionListener listener(ingress);
	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration;
	listenerConfiguration.campaignBootstrap = Bootstrap(authority);
	CHECK(StartListener(listener, listenerConfiguration),
		"authority-exhaustion listener starts");

	Client client;
	CHECK(StartClient(client, listenerConfiguration.endpoint.port),
		"authority-exhaustion client connects");
	AdmissionResponse admitted;
	CHECK(Admit(listener, client, authority, admitted),
		"authority-exhaustion client authenticates");

	FullEngineCoopTacticalServerConfiguration configuration;
	configuration.maximumAuthoritativeSequence = 1;
	FullEngineCoopTacticalServer server(ingress, listener, configuration);
	execution.server = &server;
	execution.mode = PublishingExecutionSink::Mode::AppliedTerminal;
	const CoopTacticalActorAssignment assignment{
		ActorId, admitted.peerIdentity};
	CHECK(server.beginEpoch(SessionEpoch) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.beginWorld(WorldGeneration, InitialRevision, TurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.setCampaignReadyPeers(&admitted.peerIdentity, 1) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() ==
			FullEngineCoopTacticalServerResult::Success &&
		server.replaceAssignments(&assignment, 1) ==
			FullEngineCoopTacticalServerResult::Success,
		"authority-exhaustion world publishes its peer and assignment");
	const TacticalWorldSnapshot snapshot = Snapshot();
	CHECK(server.stageBaseline(admitted.peerIdentity, snapshot) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result ==
			FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&client}, [&] {
			return client.baseline.messages.size() == 1;
		}), "authority-exhaustion baseline reaches the client");
	const CoopTacticalBaseline baseline = DecodeLastBaseline(client);
	CHECK(SendBaselineAck(client, baseline, admitted.peerIdentity) &&
		QueueArrived(listener, client) &&
		server.pumpInbound().acknowledgementsAccepted == 1,
		"authority-exhaustion baseline ACK activates gameplay");

	CHECK(SendIntent(client, admitted.peerIdentity, 1, InitialRevision) &&
		QueueArrived(listener, client),
		"last available authoritative sequence receives one exact command");
	const FullEngineCoopTacticalServerPumpResult accepted =
		server.pumpInbound(200);
	CHECK(accepted.result == FullEngineCoopTacticalServerResult::Success &&
		accepted.intentsConsumed == 1 && execution.calls == 1 &&
		WaitUntil(listener, {&client}, [&] {
			return client.receipt.messages.size() == 1;
		}), "last authoritative command and terminal receipt complete normally");
	CoopTacticalIntentReceipt receipt = DecodeLastReceipt(client);
	CHECK(receipt.commandId == 1 && receipt.authoritativeSequence == 1 &&
		receipt.status == CoopTacticalIntentReceiptStatus::Applied,
		"configured ceiling is consumed by the accepted command");

	CHECK(SendIntent(client, admitted.peerIdentity, 2, InitialRevision) &&
		QueueArrived(listener, client),
		"post-exhaustion exact command reaches the authority boundary");
	const FullEngineCoopTacticalServerPumpResult exhausted =
		server.pumpInbound(201);
	FullEngineCoopTacticalPeerCommandState commandState;
	CHECK(exhausted.result == FullEngineCoopTacticalServerResult::Success &&
		exhausted.intentsConsumed == 1 && exhausted.inputsRejected == 1 &&
		execution.calls == 1 && server.active() &&
		server.peerCommandState(admitted.peerIdentity, commandState) &&
		commandState.nextExpectedCommandId == 3 &&
		commandState.pendingCommands == 0 &&
		WaitUntil(listener, {&client}, [&] {
			return client.receipt.messages.size() == 2;
		}),
		"authority exhaustion consumes the peer cursor and flushes before any "
		"server failure latch");
	receipt = DecodeLastReceipt(client);
	CHECK(receipt.commandId == 2 && receipt.nextExpectedCommandId == 3 &&
		receipt.authoritativeSequence == 0 &&
		receipt.status == CoopTacticalIntentReceiptStatus::Rejected &&
		receipt.reason ==
			CoopTacticalIntentReceiptReason::AuthoritySequenceExhausted,
		"terminal authority exhaustion is distinct from inbox exhaustion");

	client.peer->Shutdown(20);
	CHECK(WaitUntil(listener, {&client}, [&] {
		return listener.authenticatedPeerCount() == 0;
	}) && server.reconcilePeers() ==
		FullEngineCoopTacticalServerResult::Success,
		"authority-exhaustion fixture disconnects cleanly");
	DestroyClient(client);
	CHECK(server.endWorld() == FullEngineCoopTacticalServerResult::Success,
		"authority-exhaustion fixture ends its world");
	listener.stop(20);
}

void TestPeerLocalReceiptCapacityDoesNotStrandBaselineAck()
{
	SequentialTokenSource tokens;
	PublishingExecutionSink execution;
	FullEngineCoopIngress ingress(tokens, execution);
	const AuthorityConfiguration authority = Authority(2);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success, "two-peer FIFO epoch begins");
	FullEngineCoopAdmissionListener listener(ingress);
	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration;
	listenerConfiguration.campaignBootstrap = Bootstrap(authority);
	CHECK(StartListener(listener, listenerConfiguration),
		"two-peer FIFO listener starts");
	Client clients[2];
	AdmissionResponse admitted[2];
	CHECK(StartClient(clients[0], listenerConfiguration.endpoint.port) &&
		Admit(listener, clients[0], authority, admitted[0]) &&
		StartClient(clients[1], listenerConfiguration.endpoint.port) &&
		Admit(listener, clients[1], authority, admitted[1]),
		"two peers authenticate");
	FullEngineCoopTacticalServerConfiguration configuration;
	configuration.replication.maximumReceiptHistoryPerPeer = 1;
	FullEngineCoopTacticalServer server(ingress, listener, configuration);
	execution.server = &server;
	execution.mode = PublishingExecutionSink::Mode::AppliedTerminal;
	std::array<PeerIdentity, 2> ready{
		admitted[0].peerIdentity, admitted[1].peerIdentity};
	std::sort(ready.begin(), ready.end());
	const CoopTacticalActorAssignment assignments[2]{
		{TacticalEntityId{1, 1}, admitted[0].peerIdentity},
		{TacticalEntityId{2, 1}, admitted[1].peerIdentity}};
	const TacticalWorldSnapshot snapshot = Snapshot(WorldGeneration, TurnSerial, 2);
	CHECK(server.beginEpoch(SessionEpoch) == FullEngineCoopTacticalServerResult::Success &&
		server.beginWorld(WorldGeneration, InitialRevision, TurnSerial) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.setCampaignReadyPeers(ready.data(), ready.size()) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.reconcilePeers() == FullEngineCoopTacticalServerResult::Success &&
		server.replaceAssignments(assignments, 2) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.stageBaselines(snapshot) == FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result == FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&clients[0], &clients[1]}, [&] {
			return clients[0].baseline.messages.size() == 1 &&
				clients[1].baseline.messages.size() == 1;
		}), "both peers receive initial baselines");
	CoopTacticalBaseline baselines[2]{
		DecodeLastBaseline(clients[0]), DecodeLastBaseline(clients[1])};
	CHECK(SendBaselineAck(clients[0], baselines[0], admitted[0].peerIdentity) &&
		SendBaselineAck(clients[1], baselines[1], admitted[1].peerIdentity) &&
		WaitUntil(listener, {&clients[0], &clients[1]}, [&] {
			return listener.pendingInboundCount() == 2;
		}) && server.pumpInbound().acknowledgementsAccepted == 2,
		"both peers activate");
	CoopTacticalResyncRequest request;
	request.acceptedState = baselines[0].state;
	request.requestId = 1;
	request.acceptedBaselineId = baselines[0].baselineId;
	request.lastPayloadChecksum = baselines[0].payloadChecksum;
	request.nextExpectedCommandId = 1;
	request.reason = CoopTacticalResyncReason::ReplicaRejected;
	CoopTacticalResyncRequestBytes requestBytes{};
	CHECK(SendIntent(clients[0], admitted[0].peerIdentity, 1, InitialRevision,
			TacticalEntityId{1, 1}) &&
		EncodeCoopTacticalResyncRequest(request, requestBytes) ==
			CoopTacticalCodecResult::Success &&
		clients[0].peer->SendMessage(CoopTacticalResyncRequestMessageName,
			requestBytes.data(), requestBytes.size(), clients[0].server, false) &&
		WaitUntil(listener, {&clients[0]}, [&] {
			return listener.pendingInboundCount() == 2;
		}) &&
		server.pumpInbound(499).intentsConsumed == 1 &&
		server.replication().peerPhase(admitted[0].peerIdentity) ==
			CoopTacticalPeerPhase::ResyncRequired,
		"peer A fills its one-entry history and requests same-connection resync");
	CHECK(
		server.stageBaseline(admitted[0].peerIdentity, snapshot) ==
			FullEngineCoopTacticalServerResult::Success &&
		server.flushOutbound().result == FullEngineCoopTacticalServerResult::Success &&
		WaitUntil(listener, {&clients[0]}, [&] {
			return clients[0].baseline.messages.size() == 2;
		}), "peer A awaits a replacement ACK with full receipt history");
	const CoopTacticalBaseline replacement = DecodeLastBaseline(clients[0]);
	CHECK(SendIntent(clients[1], admitted[1].peerIdentity, 1, InitialRevision,
			TacticalEntityId{2, 1}) &&
		WaitUntil(listener, {&clients[1]}, [&] {
			return listener.pendingInboundCount() == 1;
		}) &&
		SendBaselineAck(clients[0], replacement, admitted[0].peerIdentity) &&
		WaitUntil(listener, {&clients[0], &clients[1]}, [&] {
			return listener.pendingInboundCount() == 2;
		}), "peer B intent is queued ahead of peer A baseline ACK");
	const FullEngineCoopTacticalServerPumpResult pumped = server.pumpInbound(500);
	FullEngineCoopTacticalPeerCommandState stateB;
	CHECK(pumped.intentsConsumed == 1 && pumped.acknowledgementsAccepted == 1 &&
		server.peerCommandState(admitted[1].peerIdentity, stateB) &&
		stateB.nextExpectedCommandId == 2 &&
		server.replication().peerPhase(admitted[0].peerIdentity) ==
			CoopTacticalPeerPhase::Active,
		"peer-local capacity lets B consume and then releases A's queued ACK");
	listener.stop(20);
	DestroyClient(clients[0]);
	DestroyClient(clients[1]);
}
}

int main()
{
	TestRetirementBeforeCampaignReadyHasNoTacticalSlot();
	TestFullRosterRetirementCompactsAllPeerState();
	TestCoordinatorEndToEnd();
	TestAuthoritySequenceExhaustionFlushesTerminalReceipt();
	TestPeerLocalReceiptCapacityDoesNotStrandBaselineAck();
	if (failures == 0)
		std::printf("all full-engine coop tactical server tests passed\n");
	return failures == 0 ? 0 : 1;
}
