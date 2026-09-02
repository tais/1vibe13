#include "FullEngineCoopAdmissionListener.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace CoopSession;
using namespace ja2::mp;
using namespace ja2::mp::net;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++failures; \
	} \
} while (false)

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

class StopDuringIssueTokenSource final : public AdmissionTokenSource
{
public:
	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override
	{
		for (std::size_t index = 0; index < identity.size(); ++index)
			identity[index] = static_cast<std::uint8_t>(0x20 + index);
		for (std::size_t index = 0; index < token.size(); ++index)
			token[index] = static_cast<std::uint8_t>(0x60 + index);
		if (listener != nullptr) listener->stop(0);
		return true;
	}

	FullEngineCoopAdmissionListener* listener = nullptr;
};

class RejectingExecutionSink final : public TacticalIntentExecutionSink
{
public:
	TacticalIntentExecutionDisposition execute(
		const AuthorizedTacticalIntent&) noexcept override
	{
		++calls;
		return TacticalIntentExecutionDisposition::Rejected;
	}

	std::size_t calls = 0;
};

AuthorityConfiguration Authority(std::uint64_t epoch)
{
	AuthorityConfiguration configuration;
	configuration.enabled = true;
	configuration.sessionEpoch = epoch;
	configuration.runtimeFingerprintSupplied = true;
	configuration.runtimeFingerprint = {
		0x0a0b0c0du, 0x1122334455667788ull, 0x99aabbccddeeff00ull};
	configuration.contentManifestSupplied = true;
	for (std::size_t index = 0;
		index < configuration.contentManifestSha256.size(); ++index)
	{
		configuration.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0xa0 + index);
	}
	configuration.maximumPeers = 2;
	return configuration;
}

CoopCampaignBootstrapDescriptor Bootstrap(
	const AuthorityConfiguration& authority,
	std::uint64_t campaignSeed = UINT64_C(0x1020304050607080))
{
	CoopCampaignBootstrapDescriptor descriptor;
	descriptor.sessionEpoch = authority.sessionEpoch;
	descriptor.campaignSeed = campaignSeed;
	for (std::size_t index = 0;
		index < descriptor.campaignIdentitySha256.size(); ++index)
	{
		descriptor.campaignIdentitySha256[index] =
			static_cast<std::uint8_t>(0x40u + index +
				static_cast<std::uint8_t>(campaignSeed));
	}
	descriptor.runtimeFingerprint = authority.runtimeFingerprint;
	descriptor.contentManifestSha256 = authority.contentManifestSha256;
	return descriptor;
}

FullEngineCoopAdmissionListenerConfiguration ListenerConfiguration(
	const AuthorityConfiguration& authority,
	std::uint64_t campaignSeed = UINT64_C(0x1020304050607080))
{
	FullEngineCoopAdmissionListenerConfiguration configuration;
	configuration.campaignBootstrap = Bootstrap(authority, campaignSeed);
	return configuration;
}

AdmissionRequestBytes FirstJoinBytes(
	const AuthorityConfiguration& configuration)
{
	AdmissionRequest request;
	request.sessionEpoch = configuration.sessionEpoch;
	request.runtimeFingerprint = configuration.runtimeFingerprint;
	request.contentManifestSha256 = configuration.contentManifestSha256;
	AdmissionRequestBytes bytes{};
	CHECK(EncodeAdmissionRequest(request, bytes),
		"listener test request encodes");
	return bytes;
}

AdmissionRequestBytes ReconnectBytes(
	const AuthorityConfiguration& configuration,
	const AdmissionResponse& admitted)
{
	AdmissionRequest request;
	request.sessionEpoch = configuration.sessionEpoch;
	request.runtimeFingerprint = configuration.runtimeFingerprint;
	request.contentManifestSha256 = configuration.contentManifestSha256;
	request.peerIdentity = admitted.peerIdentity;
	request.reconnectToken = admitted.reconnectToken;
	AdmissionRequestBytes bytes{};
	CHECK(EncodeAdmissionRequest(request, bytes),
		"listener test reconnect request encodes");
	return bytes;
}

AdmissionAckBytes AckBytes(const AdmissionResponse& admitted)
{
	AdmissionAck acknowledgement;
	acknowledgement.sessionEpoch = admitted.sessionEpoch;
	acknowledgement.peerIdentity = admitted.peerIdentity;
	acknowledgement.reconnectToken = admitted.reconnectToken;
	AdmissionAckBytes bytes{};
	CHECK(EncodeAdmissionAck(acknowledgement, bytes),
		"listener test admission ACK encodes");
	return bytes;
}

AdmissionCredentialAbandonBytes AbandonBytes(
	const AuthorityConfiguration& configuration,
	const AdmissionResponse& abandoned)
{
	AdmissionCredentialAbandon request;
	request.sessionEpoch = configuration.sessionEpoch;
	request.runtimeFingerprint = configuration.runtimeFingerprint;
	request.contentManifestSha256 = configuration.contentManifestSha256;
	request.peerIdentity = abandoned.peerIdentity;
	request.reconnectToken = abandoned.reconnectToken;
	AdmissionCredentialAbandonBytes bytes{};
	CHECK(EncodeAdmissionCredentialAbandon(request, bytes),
		"listener credential-abandon fixture encodes");
	return bytes;
}

struct Capture
{
	std::size_t count = 0;
	std::vector<std::uint8_t> bytes;
	ConnectionId sender;
	std::vector<int>* order = nullptr;
	int orderValue = 0;
};

void CaptureMessage(SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	Capture& capture = *static_cast<Capture*>(context);
	++capture.count;
	capture.bytes.assign(message->data, message->data + message->size);
	capture.sender = message->sender;
	if (capture.order != nullptr) capture.order->push_back(capture.orderValue);
}

struct ClientEvents
{
	bool connected = false;
	bool disconnected = false;
	ConnectionId server;
};

void PumpClient(SdlNetPeer& client, ClientEvents& events)
{
	for (SdlNetEvent* event = client.Poll(); event; event = client.Poll())
	{
		if (event->size != 0 && event->data != nullptr)
		{
			if (event->data[0] == SDLNET_CONNECTION_ACCEPTED)
			{
				events.connected = true;
				events.server = event->connection;
			}
			if (event->data[0] == SDLNET_DISCONNECTION_NOTIFICATION ||
				event->data[0] == SDLNET_CONNECTION_LOST)
				events.disconnected = true;
		}
		client.Release(event);
	}
}

template <typename Predicate>
bool PumpUntil(FullEngineCoopAdmissionListener& listener,
	SdlNetPeer& client, ClientEvents& events, Predicate predicate,
	unsigned timeoutMilliseconds = 5000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		listener.poll();
		PumpClient(client, events);
		if (predicate()) return true;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		SDL_Delay(2);
	}
}

struct LiveClient
{
	SdlNetPeer* peer = nullptr;
	Capture hello;
	Capture campaignBootstrap;
	Capture response;
	Capture receipt;
	Capture baseline;
	Capture delta;
	Capture campaignMetadata;
	Capture campaignChunk;
	Capture campaignComplete;
	Capture campaignReject;
	ClientEvents events;
};

bool StartClient(LiveClient& client, std::uint16_t port)
{
	client.peer = CreateSdlNetPeer();
	SdlNetEndpoint endpoint;
	if (!client.peer->Start(1, endpoint)) return false;
	if (!client.peer->RegisterMessage(
		CoopServerHelloMessageName, CaptureMessage, &client.hello) ||
		!client.peer->RegisterMessage(
			CoopCampaignBootstrapMessageName,
			CaptureMessage, &client.campaignBootstrap) ||
		!client.peer->RegisterMessage(
			CoopAdmissionResponseMessageName, CaptureMessage, &client.response) ||
		!client.peer->RegisterMessage(
			CoopTacticalIntentReceiptMessageName,
			CaptureMessage, &client.receipt) ||
		!client.peer->RegisterMessage(
			CoopTacticalBaselineMessageName,
			CaptureMessage, &client.baseline) ||
		!client.peer->RegisterMessage(
			CoopTacticalDeltaMessageName,
			CaptureMessage, &client.delta) ||
		!client.peer->RegisterMessage(
			CoopCampaignSyncMetadataMessageName,
			CaptureMessage, &client.campaignMetadata) ||
		!client.peer->RegisterMessage(
			CoopCampaignSyncChunkMessageName,
			CaptureMessage, &client.campaignChunk) ||
		!client.peer->RegisterMessage(
			CoopCampaignSyncCompleteMessageName,
			CaptureMessage, &client.campaignComplete) ||
		!client.peer->RegisterMessage(
			CoopCampaignSyncRejectMessageName,
			CaptureMessage, &client.campaignReject))
		return false;
	return client.peer->Connect("127.0.0.1", port);
}

void DestroyClient(LiveClient& client)
{
	if (client.peer == nullptr) return;
	client.peer->Shutdown(20);
	DestroySdlNetPeer(client.peer);
	client.peer = nullptr;
}

template <typename Predicate>
bool PumpManyUntil(FullEngineCoopAdmissionListener& listener,
	std::vector<LiveClient*> clients, Predicate predicate,
	unsigned timeoutMilliseconds = 5000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		listener.poll();
		for (LiveClient* client : clients)
			if (client != nullptr && client->peer != nullptr)
				PumpClient(*client->peer, client->events);
		if (predicate()) return true;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		SDL_Delay(2);
	}
}

bool StartListenerOnLoopback(FullEngineCoopAdmissionListener& listener,
	FullEngineCoopAdmissionListenerConfiguration& configuration)
{
	static Uint64 sequence = static_cast<Uint64>(
		std::chrono::steady_clock::now().time_since_epoch().count());
	for (unsigned attempt = 0; attempt < 128; ++attempt)
	{
		const std::uint16_t port = static_cast<std::uint16_t>(
			40000 + (sequence++ % 20000));
		configuration.endpoint = SdlNetEndpoint(port, "127.0.0.1");
		if (listener.start(configuration) ==
			FullEngineCoopAdmissionListenerStartResult::Success)
			return true;
	}
	return false;
}

bool SendAck(LiveClient& client, const AdmissionResponse& admitted)
{
	const AdmissionAckBytes acknowledgement = AckBytes(admitted);
	return client.peer != nullptr && client.peer->SendMessage(
		CoopAdmissionAckMessageName, acknowledgement.data(),
		acknowledgement.size(), client.events.server, false);
}

bool DecodeLatestResponse(
	const LiveClient& client, AdmissionResponse& response)
{
	return DecodeAdmissionResponse(client.response.bytes.data(),
		client.response.bytes.size(), response) == DecodeResult::Ok;
}

bool AdmitAndAcknowledge(FullEngineCoopAdmissionListener& listener,
	LiveClient& client, const AuthorityConfiguration& authority,
	std::vector<LiveClient*> clients, AdmissionResponse& admitted)
{
	const std::size_t responseCount = client.response.count;
	const AdmissionRequestBytes request = FirstJoinBytes(authority);
	if (client.peer == nullptr || !client.peer->SendMessage(
		CoopAdmissionRequestMessageName, request.data(), request.size(),
		client.events.server, false))
		return false;
	if (!PumpManyUntil(listener, clients, [&] {
		return client.response.count == responseCount + 1;
	}) || !DecodeLatestResponse(client, admitted) || !admitted.admitted() ||
		!SendAck(client, admitted))
		return false;
	return PumpManyUntil(listener, clients, [&] {
		TransportPeer transport;
		return listener.authenticatedTransportForPeer(
			admitted.peerIdentity, transport);
	});
}

void TestServerHelloCodec()
{
	const AuthorityConfiguration configuration = Authority(0x0102030405060708ull);
	CoopServerHello hello;
	hello.protocolVersion = CurrentProtocolVersion;
	hello.sessionEpoch = configuration.sessionEpoch;
	hello.runtimeFingerprint = configuration.runtimeFingerprint;
	hello.contentManifestSha256 = configuration.contentManifestSha256;
	CoopServerHelloBytes bytes{};
	CHECK(EncodeCoopServerHello(hello, bytes),
		"complete server hello encodes");
	const CoopServerHelloBytes expected{{
		0x4a, 0x32, 0x43, 0x48, 0x01, 0x00, 0x01, 0x00,
		0x07, 0x00, 0x00, 0x00,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x0d, 0x0c, 0x0b, 0x0a,
		0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
		0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf
	}};
	CHECK(bytes == expected,
		"server hello is the pinned 72-byte little-endian wire image");

	CoopServerHello decoded;
	CHECK(DecodeCoopServerHello(bytes.data(), bytes.size(), decoded) ==
		CoopServerHelloDecodeResult::Success &&
		decoded.protocolVersion == hello.protocolVersion &&
		decoded.sessionEpoch == hello.sessionEpoch &&
		decoded.runtimeFingerprint == hello.runtimeFingerprint &&
		decoded.contentManifestSha256 == hello.contentManifestSha256,
		"server hello round-trips every compatibility field");
	CoopServerHelloBytes malformed = bytes;
	malformed[10] = 1;
	CHECK(DecodeCoopServerHello(malformed.data(), malformed.size(), decoded) ==
		CoopServerHelloDecodeResult::NonZeroReserved,
		"server hello rejects nonzero reserved bytes");
	CHECK(DecodeCoopServerHello(bytes.data(), bytes.size() - 1, decoded) ==
		CoopServerHelloDecodeResult::WrongSize,
		"server hello rejects every non-exact width");
}

void TestAdmissionOnlyListener()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority =
		Authority(0x5566778899aabbccull);
	FullEngineCoopAdmissionListenerConfiguration listenerConfiguration =
		ListenerConfiguration(authority);
	listenerConfiguration.endpoint = SdlNetEndpoint(1, "127.0.0.1");
	CHECK(listener.start(listenerConfiguration) ==
		FullEngineCoopAdmissionListenerStartResult::AdmissionSessionInactive,
		"listener refuses to bind before admission is configured");

	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"listener test starts admission without tactical authority");
	listenerConfiguration.maximumConnections =
		MaximumCoopAdmissionTransportConnections + 1;
	CHECK(listener.start(listenerConfiguration) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener enforces its transport connection bound");
	listenerConfiguration.maximumConnections = 4;
	listenerConfiguration.maximumQueuedTacticalMessages = 0;
	CHECK(listener.start(listenerConfiguration) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener rejects an unbounded zero-capacity tactical queue configuration");
	listenerConfiguration.maximumQueuedTacticalMessages =
		MaximumCoopTacticalInboundMessages;
	listenerConfiguration.maximumQueuedCampaignMessages = 0;
	CHECK(listener.start(listenerConfiguration) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener rejects an unbounded zero-capacity campaign queue configuration");
	listenerConfiguration.maximumQueuedCampaignMessages =
		MaximumCoopCampaignInboundMessages;
	FullEngineCoopAdmissionListenerConfiguration invalidBootstrap =
		listenerConfiguration;
	invalidBootstrap.campaignBootstrap = {};
	CHECK(listener.start(invalidBootstrap) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener rejects a semantically incomplete campaign bootstrap");
	FullEngineCoopAdmissionListenerConfiguration mismatchedBootstrap =
		listenerConfiguration;
	mismatchedBootstrap.campaignBootstrap.protocolVersion = 0;
	CHECK(listener.start(mismatchedBootstrap) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener rejects a bootstrap with the wrong admission protocol");
	mismatchedBootstrap = listenerConfiguration;
	++mismatchedBootstrap.campaignBootstrap.sessionEpoch;
	CHECK(listener.start(mismatchedBootstrap) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener rejects a valid bootstrap for another admission epoch");
	mismatchedBootstrap = listenerConfiguration;
	mismatchedBootstrap.campaignBootstrap.runtimeFingerprint.high ^= 1;
	CHECK(listener.start(mismatchedBootstrap) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener rejects a bootstrap for another runtime fingerprint");
	mismatchedBootstrap = listenerConfiguration;
	mismatchedBootstrap.campaignBootstrap.contentManifestSha256[0] ^= 1;
	CHECK(listener.start(mismatchedBootstrap) ==
		FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration,
		"listener rejects a bootstrap for another content manifest");

	const bool started = StartListenerOnLoopback(listener, listenerConfiguration);
	CHECK(started, "isolated admission listener binds a loopback endpoint");
	if (!started) return;
	CHECK(listener.running() && ingress.admissionActive() &&
		!ingress.tacticalActive(),
		"listener stays admission-only after bind");

	LiveClient client;
	CHECK(StartClient(client, listenerConfiguration.endpoint.port),
		"client initiates admission listener connection");
	std::vector<int> handshakeOrder;
	client.hello.order = &handshakeOrder;
	client.hello.orderValue = 1;
	client.campaignBootstrap.order = &handshakeOrder;
	client.campaignBootstrap.orderValue = 2;
	client.response.order = &handshakeOrder;
	client.response.orderValue = 3;
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "accepted client receives one ordered server handshake prelude");
	CHECK(client.hello.bytes.size() == CoopServerHelloWireSize,
		"live listener sends the exact fixed-width hello");
	CoopServerHello liveHello;
	CHECK(DecodeCoopServerHello(client.hello.bytes.data(),
		client.hello.bytes.size(), liveHello) ==
			CoopServerHelloDecodeResult::Success &&
		liveHello.sessionEpoch == authority.sessionEpoch &&
		liveHello.runtimeFingerprint == authority.runtimeFingerprint &&
		liveHello.contentManifestSha256 == authority.contentManifestSha256,
		"live hello advertises only the server-owned compatibility contract");
	CoopCampaignBootstrapDescriptor liveBootstrap;
	CHECK(client.campaignBootstrap.bytes.size() ==
			CoopCampaignBootstrapWireSize &&
		DecodeCoopCampaignBootstrap(client.campaignBootstrap.bytes.data(),
			client.campaignBootstrap.bytes.size(), liveBootstrap) ==
				CoopCampaignBootstrapDecodeResult::Success &&
		SameCoopCampaignBootstrapDescriptor(liveBootstrap,
			listenerConfiguration.campaignBootstrap) &&
		handshakeOrder.size() == 2 && handshakeOrder[0] == 1 &&
		handshakeOrder[1] == 2,
		"listener sends the exact retained 128-byte bootstrap after hello");

	LiveClient duplicate;
	CHECK(StartClient(duplicate, listenerConfiguration.endpoint.port),
		"second pre-admission client connects");
	CHECK(PumpManyUntil(listener, {&client, &duplicate}, [&] {
		return duplicate.events.connected && duplicate.hello.count == 1 &&
			duplicate.campaignBootstrap.count == 1;
	}) && duplicate.hello.bytes == client.hello.bytes &&
		duplicate.campaignBootstrap.bytes == client.campaignBootstrap.bytes,
		"each connection receives one identical retained handshake prelude");
	(void)PumpManyUntil(listener, {&client, &duplicate},
		[] { return false; }, 30);
	CHECK(duplicate.hello.count == 1 &&
		duplicate.campaignBootstrap.count == 1,
		"accept and idle polling never duplicate either prelude frame");
	DestroyClient(duplicate);

	const std::uint8_t malformed = 0xff;
	CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
		&malformed, 1, client.events.server, false),
		"malformed admission request reaches only the admission namespace");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.response.count == 1;
	}), "malformed admission receives a fixed response");
	AdmissionResponse response;
	CHECK(DecodeLatestResponse(client, response) &&
		response.rejectReason == AdmissionRejectReason::MalformedRequest &&
		handshakeOrder.size() == 3 && handshakeOrder[0] == 1 &&
		handshakeOrder[1] == 2 && handshakeOrder[2] == 3,
		"listener sanitizes malformed admission through full-engine ingress");

	const AdmissionRequestBytes firstJoin = FirstJoinBytes(authority);
	CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
		firstJoin.data(), firstJoin.size(), client.events.server, false),
		"valid fixed-width admission request sends");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.response.count == 2;
	}), "valid admission receives a targeted response");
	CHECK(DecodeLatestResponse(client, response) &&
		response.admitted() && ingress.admittedPeerCount() == 1 &&
		ingress.boundPeerCount() == 1,
		"listener binds issued identity to the sender-derived connection");
	const AdmissionResponse firstCredential = response;
	CHECK(SendAck(client, firstCredential),
		"client explicitly ACKs its issued credential");
	(void)PumpManyUntil(listener, {&client}, [] { return false; }, 30);

	std::array<std::uint8_t, TacticalIntentHeaderWireSize> fakeTactical{};
	for (std::size_t index = 0; index < fakeTactical.size(); ++index)
		fakeTactical[index] = static_cast<std::uint8_t>(0x30 + index);
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
		fakeTactical.data(), fakeTactical.size(), client.events.server, false),
		"authenticated tactical wire frame sends to the isolated namespace");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return listener.pendingInboundCount() == 1;
	}), "listener copies one authenticated tactical frame into its bounded queue");
	FullEngineCoopTacticalInboundMessage queued;
	CHECK(listener.popInbound(queued) &&
		queued.kind == FullEngineCoopTacticalInboundKind::Intent &&
		queued.peerIdentity == firstCredential.peerIdentity &&
		queued.size == fakeTactical.size() &&
		std::equal(fakeTactical.begin(), fakeTactical.end(), queued.bytes.begin()) &&
		sink.calls == 0 && !ingress.tacticalActive(),
		"transport callback attaches server identity and copies bytes without executing JA2");

	// Stop while the acknowledged transport is still live. The transport's next
	// instance restarts ConnectionId allocation from one, so stale bindings would
	// hand the old credential to the next socket's zero-credential request.
	listener.stop(20);
	CHECK(!listener.running() && ingress.boundPeerCount() == 0 &&
		ingress.admittedPeerCount() == 1,
		"listener stop clears bindings while retaining ACKed credentials");
	const std::vector<std::uint8_t> firstBootstrapBytes =
		client.campaignBootstrap.bytes;
	DestroyClient(client);
	listenerConfiguration.campaignBootstrap = Bootstrap(
		authority, UINT64_C(0x8877665544332211));
	CHECK(StartListenerOnLoopback(listener, listenerConfiguration),
		"admission listener restarts over the same live session");
	LiveClient afterRestart;
	CHECK(StartClient(afterRestart, listenerConfiguration.endpoint.port),
		"post-restart client connects");
	CHECK(PumpManyUntil(listener, {&afterRestart}, [&] {
		return afterRestart.events.connected && afterRestart.hello.count == 1 &&
			afterRestart.campaignBootstrap.count == 1;
	}), "post-restart transport receives a complete fresh prelude");
	CoopCampaignBootstrapDescriptor restartedBootstrap;
	CHECK(DecodeCoopCampaignBootstrap(
			afterRestart.campaignBootstrap.bytes.data(),
			afterRestart.campaignBootstrap.bytes.size(), restartedBootstrap) ==
			CoopCampaignBootstrapDecodeResult::Success &&
		SameCoopCampaignBootstrapDescriptor(restartedBootstrap,
			listenerConfiguration.campaignBootstrap) &&
		afterRestart.campaignBootstrap.bytes != firstBootstrapBytes,
		"listener restart replaces every retained bootstrap byte without stale reuse");
	CHECK(ingress.boundPeerCount() == 0,
		"restarted ConnectionId cannot inherit the old transport binding");
	CHECK(afterRestart.peer->SendMessage(CoopAdmissionRequestMessageName,
		firstJoin.data(), firstJoin.size(), afterRestart.events.server, false),
		"post-restart zero-credential join sends");
	CHECK(PumpManyUntil(listener, {&afterRestart}, [&] {
		return afterRestart.response.count == 1;
	}), "post-restart zero-credential join receives response");
	AdmissionResponse restartResponse;
	CHECK(DecodeLatestResponse(afterRestart, restartResponse) &&
		restartResponse.admitted() &&
		restartResponse.peerIdentity != firstCredential.peerIdentity,
		"ConnectionId reuse cannot take over the retained credential");
	afterRestart.peer->Shutdown(20);
	CHECK(PumpManyUntil(listener, {&afterRestart}, [&] {
		return ingress.admittedPeerCount() == 1;
	}), "unacknowledged post-restart seat is reclaimed on disconnect");
	DestroyClient(afterRestart);
	listener.stop(0);
}

void TestLostAckCredentialAbandonRetry()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	AuthorityConfiguration authority = Authority(0x5566778899000042ull);
	authority.maximumPeers = 1;
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"lost-ACK retry admission session starts");
	FullEngineCoopAdmissionListener listener(ingress);
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	configuration.maximumRejectedAdmissionMessages = 1;
	CHECK(StartListenerOnLoopback(listener, configuration),
		"lost-ACK retry listener starts");

	AdmissionResponse arbitrary;
	arbitrary.sessionEpoch = authority.sessionEpoch;
	arbitrary.peerIdentity.fill(0x31);
	arbitrary.reconnectToken.fill(0x71);
	const AdmissionCredentialAbandonBytes arbitraryAbandon =
		AbandonBytes(authority, arbitrary);
	LiveClient attacker;
	CHECK(StartClient(attacker, configuration.endpoint.port),
		"unsolicited-abandon client connects");
	CHECK(PumpManyUntil(listener, {&attacker}, [&] {
		return attacker.events.connected && attacker.hello.count == 1 &&
			attacker.campaignBootstrap.count == 1;
	}), "unsolicited-abandon client receives hello");
	CHECK(attacker.peer->SendMessage(
		CoopAdmissionCredentialAbandonMessageName,
		arbitraryAbandon.data(), arbitraryAbandon.size(),
		attacker.events.server, false),
		"unsolicited credential abandonment reaches isolated namespace");
	CHECK(PumpManyUntil(listener, {&attacker}, [&] {
		return attacker.events.disconnected;
	}) && attacker.response.count == 0 && ingress.admittedPeerCount() == 0,
		"abandonment without a transport-bound UnknownPeer offer fails closed");
	DestroyClient(attacker);

	LiveClient first;
	CHECK(StartClient(first, configuration.endpoint.port),
		"lost-ACK first client connects");
	CHECK(PumpManyUntil(listener, {&first}, [&] {
		return first.events.connected && first.hello.count == 1 &&
			first.campaignBootstrap.count == 1;
	}), "lost-ACK first client receives hello");
	const AdmissionRequestBytes firstJoin = FirstJoinBytes(authority);
	CHECK(first.peer->SendMessage(CoopAdmissionRequestMessageName,
		firstJoin.data(), firstJoin.size(), first.events.server, false),
		"lost-ACK first join sends");
	CHECK(PumpManyUntil(listener, {&first}, [&] {
		return first.response.count == 1;
	}), "lost-ACK first credential arrives");
	AdmissionResponse lostCredential;
	CHECK(DecodeLatestResponse(first, lostCredential) &&
		lostCredential.admitted() && ingress.admittedPeerCount() == 1,
		"unacknowledged credential occupies one pending seat");
	DestroyClient(first);
	CHECK(PumpManyUntil(listener, {}, [&] {
		return ingress.admittedPeerCount() == 0;
	}), "disconnect reclaims the unacknowledged credential");

	const AdmissionRequestBytes reconnect =
		ReconnectBytes(authority, lostCredential);
	const AdmissionCredentialAbandonBytes exactAbandon =
		AbandonBytes(authority, lostCredential);
	LiveClient repeated;
	CHECK(StartClient(repeated, configuration.endpoint.port),
		"repeat-offer adversary reconnects");
	CHECK(PumpManyUntil(listener, {&repeated}, [&] {
		return repeated.events.connected && repeated.hello.count == 1 &&
			repeated.campaignBootstrap.count == 1;
	}), "repeat-offer adversary receives hello");
	CHECK(repeated.peer->SendMessage(CoopAdmissionRequestMessageName,
		reconnect.data(), reconnect.size(), repeated.events.server, false),
		"repeat-offer first stale request sends");
	CHECK(PumpManyUntil(listener, {&repeated}, [&] {
		return repeated.response.count == 1;
	}), "repeat-offer adversary receives its one permit");
	CHECK(repeated.peer->SendMessage(CoopAdmissionRequestMessageName,
		reconnect.data(), reconnect.size(), repeated.events.server, false),
		"repeat-offer second stale request sends");
	CHECK(PumpManyUntil(listener, {&repeated}, [&] {
		return repeated.events.disconnected;
	}) && ingress.admittedPeerCount() == 0,
		"one transport cannot renew an unused abandonment offer");
	DestroyClient(repeated);

	LiveClient mismatched;
	CHECK(StartClient(mismatched, configuration.endpoint.port),
		"mismatched-abandon client reconnects");
	CHECK(PumpManyUntil(listener, {&mismatched}, [&] {
		return mismatched.events.connected && mismatched.hello.count == 1 &&
			mismatched.campaignBootstrap.count == 1;
	}), "mismatched-abandon client receives hello");
	CHECK(mismatched.peer->SendMessage(CoopAdmissionRequestMessageName,
		reconnect.data(), reconnect.size(), mismatched.events.server, false),
		"stale reconnect credential sends");
	CHECK(PumpManyUntil(listener, {&mismatched}, [&] {
		return mismatched.response.count == 1;
	}), "stale reconnect receives explicit response");
	AdmissionResponse unknown;
	CHECK(DecodeLatestResponse(mismatched, unknown) &&
		unknown.rejectReason == AdmissionRejectReason::UnknownPeer &&
		unknown.peerIdentity == lostCredential.peerIdentity &&
		!mismatched.events.disconnected,
		"exact UnknownPeer offer does not consume the rejection budget");
	AdmissionCredentialAbandonBytes wrongAbandon = exactAbandon;
	wrongAbandon.back() ^= 1;
	CHECK(mismatched.peer->SendMessage(
		CoopAdmissionCredentialAbandonMessageName,
		wrongAbandon.data(), wrongAbandon.size(),
		mismatched.events.server, false),
		"mismatched abandonment echo sends");
	CHECK(PumpManyUntil(listener, {&mismatched}, [&] {
		return mismatched.events.disconnected;
	}) && ingress.admittedPeerCount() == 0,
		"mismatched abandonment consumes its one-shot permit without issuing");
	DestroyClient(mismatched);

	LiveClient recovered;
	CHECK(StartClient(recovered, configuration.endpoint.port),
		"lost-ACK client reconnects for explicit recovery");
	CHECK(PumpManyUntil(listener, {&recovered}, [&] {
		return recovered.events.connected && recovered.hello.count == 1 &&
			recovered.campaignBootstrap.count == 1;
	}), "recovery transport receives hello");
	CHECK(recovered.peer->SendMessage(CoopAdmissionRequestMessageName,
		reconnect.data(), reconnect.size(), recovered.events.server, false),
		"recovery repeats the stale reconnect explicitly");
	CHECK(PumpManyUntil(listener, {&recovered}, [&] {
		return recovered.response.count == 1;
	}), "recovery receives a fresh UnknownPeer offer");
	CHECK(recovered.peer->SendMessage(
		CoopAdmissionCredentialAbandonMessageName,
		exactAbandon.data(), exactAbandon.size(),
		recovered.events.server, false),
		"exact offered credential abandonment sends");
	CHECK(PumpManyUntil(listener, {&recovered}, [&] {
		return recovered.response.count == 2;
	}), "explicit abandonment receives one fresh credential response");
	AdmissionResponse replacement;
	CHECK(DecodeLatestResponse(recovered, replacement) &&
		replacement.admitted() &&
		replacement.peerIdentity != lostCredential.peerIdentity &&
		ingress.admittedPeerCount() == 1 && ingress.boundPeerCount() == 1,
		"explicit retry allocates exactly one distinct pending seat");
	CHECK(SendAck(recovered, replacement),
		"replacement credential ACK sends");
	CHECK(PumpManyUntil(listener, {&recovered}, [&] {
		return listener.authenticatedPeerCount() == 1;
	}), "replacement ACK alone promotes the recovered transport");

	DestroyClient(recovered);
	listener.stop(0);
}

void TestUnauthenticatedTacticalTrafficFailsClosed()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4450);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"unauthenticated tactical rejection session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	CHECK(StartListenerOnLoopback(listener, configuration),
		"unauthenticated tactical rejection listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"unauthenticated tactical client connects");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "unauthenticated tactical client receives hello");

	std::array<std::uint8_t, TacticalIntentHeaderWireSize> intent{};
	intent.fill(0xa5);
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
		intent.data(), intent.size(), client.events.server, false),
		"pre-admission tactical frame reaches the transport boundary");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.disconnected;
	}), "pre-admission tactical traffic closes the unauthenticated transport");
	CHECK(listener.pendingInboundCount() == 0 &&
		listener.authenticatedPeerCount() == 0 &&
		ingress.boundPeerCount() == 0 && ingress.admittedPeerCount() == 0 &&
		sink.calls == 0,
		"unauthenticated tactical bytes neither queue nor execute nor retain authority");
	DestroyClient(client);
	listener.stop(0);
}

void TestUnauthenticatedCampaignTrafficFailsClosed()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4454);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"unauthenticated campaign rejection session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	CHECK(StartListenerOnLoopback(listener, configuration),
		"unauthenticated campaign rejection listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"unauthenticated campaign client connects");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "unauthenticated campaign client receives hello");

	CoopCampaignSyncAckBytes acknowledgement{};
	acknowledgement.fill(0xa6u);
	CHECK(client.peer->SendMessage(CoopCampaignSyncAckMessageName,
		acknowledgement.data(), acknowledgement.size(),
		client.events.server, false),
		"pre-admission campaign ACK reaches the transport boundary");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.disconnected;
	}), "pre-admission campaign control closes the unauthenticated transport");
	CHECK(listener.pendingInboundCount() == 0 &&
		listener.pendingCampaignInboundCount() == 0 &&
		listener.authenticatedPeerCount() == 0 &&
		ingress.boundPeerCount() == 0 && ingress.admittedPeerCount() == 0 &&
		sink.calls == 0,
		"unauthenticated campaign bytes enter neither bounded server FIFO");
	DestroyClient(client);
	listener.stop(0);
}

void TestAuthenticatedCampaignQueueAndTargetedDelivery()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4455);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"authenticated campaign queue session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	configuration.maximumQueuedCampaignMessages = 4;
	CHECK(StartListenerOnLoopback(listener, configuration),
		"authenticated campaign queue listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"campaign queue client connects");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "campaign queue client receives hello");
	AdmissionResponse admitted;
	CHECK(AdmitAndAcknowledge(listener, client, authority, {&client}, admitted),
		"campaign queue client admits and ACKs");
	TransportPeer authenticatedTransport;
	CHECK(listener.authenticatedTransportForPeer(
		admitted.peerIdentity, authenticatedTransport),
		"campaign queue resolves the authenticated transport");

	CoopCampaignSyncAckBytes acknowledgement{};
	CoopCampaignSyncResultBytes result{};
	CoopCampaignSyncResyncBytes resync{};
	acknowledgement.fill(0x31u);
	result.fill(0x52u);
	resync.fill(0x73u);
	CHECK(client.peer->SendMessage(CoopCampaignSyncAckMessageName,
			acknowledgement.data(), acknowledgement.size(),
			client.events.server, false) &&
		client.peer->SendMessage(CoopCampaignSyncResultMessageName,
			result.data(), result.size(), client.events.server, false) &&
		client.peer->SendMessage(CoopCampaignSyncResyncMessageName,
			resync.data(), resync.size(), client.events.server, false),
		"authenticated campaign controls send in one ordered burst");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return listener.pendingCampaignInboundCount() == 3;
	}) && listener.pendingInboundCount() == 0,
		"campaign controls occupy only their independent bounded FIFO");
	FullEngineCoopCampaignInboundMessage queued;
	CHECK(listener.popCampaignInbound(queued) &&
		queued.kind == FullEngineCoopCampaignInboundKind::Ack &&
		queued.peerIdentity == admitted.peerIdentity &&
		queued.transport == authenticatedTransport &&
		queued.size == acknowledgement.size() &&
		std::equal(acknowledgement.begin(), acknowledgement.end(),
			queued.bytes.begin()),
		"campaign ACK dequeue attaches the transport-resolved identity");
	CHECK(listener.popCampaignInbound(queued) &&
		queued.kind == FullEngineCoopCampaignInboundKind::Result &&
		queued.peerIdentity == admitted.peerIdentity &&
		queued.size == result.size() &&
		std::equal(result.begin(), result.end(), queued.bytes.begin()),
		"campaign result retains exact FIFO bytes");
	CHECK(listener.popCampaignInbound(queued) &&
		queued.kind == FullEngineCoopCampaignInboundKind::Resync &&
		queued.peerIdentity == admitted.peerIdentity &&
		queued.size == resync.size() &&
		std::equal(resync.begin(), resync.end(), queued.bytes.begin()) &&
		!listener.popCampaignInbound(queued) && sink.calls == 0,
		"campaign resync completes FIFO delivery without gameplay execution");

	CoopCampaignSyncMetadataBytes metadata{};
	std::vector<std::uint8_t> chunk(MaximumCoopCampaignSyncWireSize, 0x25u);
	CoopCampaignSyncCompleteBytes complete{};
	CoopCampaignSyncRejectBytes reject{};
	metadata.fill(0x14u);
	complete.fill(0x36u);
	reject.fill(0x47u);
	CHECK(listener.sendToPeer(admitted.peerIdentity,
			CoopCampaignSyncMetadataMessageName,
			metadata.data(), metadata.size()) &&
		PumpManyUntil(listener, {&client}, [&] {
			return client.campaignMetadata.count == 1;
		}), "campaign metadata unicasts to its authenticated identity");
	CHECK(listener.sendToPeer(admitted.peerIdentity,
			CoopCampaignSyncChunkMessageName, chunk.data(), chunk.size()) &&
		PumpManyUntil(listener, {&client}, [&] {
			return client.campaignChunk.count == 1;
		}), "the maximum campaign chunk passes the bounded unicast whitelist");
	CHECK(listener.sendToPeer(admitted.peerIdentity,
			CoopCampaignSyncCompleteMessageName,
			complete.data(), complete.size()) &&
		PumpManyUntil(listener, {&client}, [&] {
			return client.campaignComplete.count == 1;
		}), "campaign completion unicasts to its authenticated identity");
	CHECK(listener.sendToPeer(admitted.peerIdentity,
			CoopCampaignSyncRejectMessageName,
			reject.data(), reject.size()) &&
		PumpManyUntil(listener, {&client}, [&] {
			return client.campaignReject.count == 1;
		}), "campaign rejection unicasts to its authenticated identity");
	CHECK(client.campaignMetadata.bytes == std::vector<std::uint8_t>(
			metadata.begin(), metadata.end()) &&
		client.campaignChunk.bytes == chunk &&
		client.campaignComplete.bytes == std::vector<std::uint8_t>(
			complete.begin(), complete.end()) &&
		client.campaignReject.bytes == std::vector<std::uint8_t>(
			reject.begin(), reject.end()),
		"campaign unicasts preserve every bounded wire byte");
	CHECK(!listener.sendToPeer(admitted.peerIdentity,
			CoopCampaignSyncAckMessageName,
			acknowledgement.data(), acknowledgement.size()) &&
		!listener.sendToPeer(admitted.peerIdentity,
			CoopCampaignSyncChunkMessageName,
			chunk.data(), MaximumCoopCampaignSyncWireSize + 1) &&
		!listener.sendToPeer(admitted.peerIdentity,
			CoopCampaignSyncMetadataMessageName,
			metadata.data(), metadata.size() - 1),
		"campaign unicast rejects reversed names and every out-of-bound shape");

	std::array<std::uint8_t, TacticalIntentHeaderWireSize> tactical{};
	tactical.fill(0x88u);
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
			tactical.data(), tactical.size(), client.events.server, false) &&
		client.peer->SendMessage(CoopCampaignSyncAckMessageName,
			acknowledgement.data(), acknowledgement.size(),
			client.events.server, false) &&
		PumpManyUntil(listener, {&client}, [&] {
			return listener.pendingInboundCount() == 1 &&
				listener.pendingCampaignInboundCount() == 1;
		}), "one transport can queue independent tactical and campaign work");
	client.peer->CloseConnection(client.events.server, true);
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return listener.authenticatedPeerCount() == 0;
	}) && listener.pendingInboundCount() == 0 &&
		listener.pendingCampaignInboundCount() == 0,
		"disconnect removes that transport from both independent FIFOs");
	DestroyClient(client);
	listener.stop(0);
}

void TestCampaignQueueSaturationFailsClosed()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4456);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"campaign saturation session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	configuration.maximumQueuedCampaignMessages = 1;
	CHECK(StartListenerOnLoopback(listener, configuration),
		"one-entry campaign queue listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"campaign saturation client connects");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "campaign saturation client receives hello");
	AdmissionResponse admitted;
	CHECK(AdmitAndAcknowledge(listener, client, authority, {&client}, admitted),
		"campaign saturation client authenticates");

	std::array<std::uint8_t, TacticalIntentHeaderWireSize> tactical{};
	CoopCampaignSyncAckBytes acknowledgement{};
	CoopCampaignSyncResultBytes result{};
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
			tactical.data(), tactical.size(), client.events.server, false) &&
		client.peer->SendMessage(CoopCampaignSyncAckMessageName,
			acknowledgement.data(), acknowledgement.size(),
			client.events.server, false) &&
		PumpManyUntil(listener, {&client}, [&] {
			return listener.pendingInboundCount() == 1 &&
				listener.pendingCampaignInboundCount() == 1;
		}), "campaign saturation fixture fills both independent queues");
	CHECK(client.peer->SendMessage(CoopCampaignSyncResultMessageName,
		result.data(), result.size(), client.events.server, false),
		"one campaign control beyond the configured ceiling sends");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.disconnected;
	}), "campaign FIFO saturation closes the offending transport");
	CHECK(listener.pendingInboundCount() == 0 &&
		listener.pendingCampaignInboundCount() == 0 &&
		listener.authenticatedPeerCount() == 0 &&
		ingress.boundPeerCount() == 0 && ingress.admittedPeerCount() == 1 &&
		sink.calls == 0,
		"campaign saturation retires both stale queues for only that transport");
	DestroyClient(client);
	listener.stop(0);
}

void TestAuthenticatedTacticalQueueAndTargetedDelivery()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4451);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"authenticated tactical queue session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	configuration.maximumConnections = 2;
	configuration.maximumQueuedTacticalMessages = 8;
	CHECK(StartListenerOnLoopback(listener, configuration),
		"authenticated tactical queue listener starts");

	LiveClient first;
	LiveClient second;
	CHECK(StartClient(first, configuration.endpoint.port) &&
		StartClient(second, configuration.endpoint.port),
		"two tactical clients connect");
	CHECK(PumpManyUntil(listener, {&first, &second}, [&] {
		return first.events.connected && second.events.connected &&
			first.hello.count == 1 && second.hello.count == 1 &&
			first.campaignBootstrap.count == 1 &&
			second.campaignBootstrap.count == 1;
	}), "two tactical clients receive isolated server hellos");
	AdmissionResponse firstPeer;
	AdmissionResponse secondPeer;
	CHECK(AdmitAndAcknowledge(listener, first, authority,
		{&first, &second}, firstPeer),
		"first tactical client admits and ACKs");
	CHECK(AdmitAndAcknowledge(listener, second, authority,
		{&first, &second}, secondPeer),
		"second tactical client admits and ACKs");
	CHECK(firstPeer.peerIdentity != secondPeer.peerIdentity &&
		listener.authenticatedPeerCount() == 2,
		"listener exposes two distinct ACK-confirmed identities");

	std::array<FullEngineCoopAuthenticatedPeer, MaximumAuthorityPeers> peers{};
	const std::size_t peerCount = listener.authenticatedPeers(peers);
	CHECK(peerCount == 2 &&
		peers[0].peerIdentity < peers[1].peerIdentity,
		"authenticated enumeration is bounded and identity-sorted");
	TransportPeer firstTransport;
	TransportPeer secondTransport;
	PeerIdentity mapped{};
	CHECK(listener.authenticatedTransportForPeer(
		firstPeer.peerIdentity, firstTransport) &&
		listener.authenticatedPeerForTransport(firstTransport, mapped) &&
		mapped == firstPeer.peerIdentity &&
		listener.authenticatedTransportForPeer(
			secondPeer.peerIdentity, secondTransport) &&
		firstTransport != secondTransport,
		"authenticated identity and transport mappings round-trip without aliasing");

	TacticalIntent claimedAsSecond;
	claimedAsSecond.sessionEpoch = authority.sessionEpoch;
	claimedAsSecond.claimedPeerIdentity = secondPeer.peerIdentity;
	claimedAsSecond.commandId = 1;
	claimedAsSecond.worldGeneration = 4;
	claimedAsSecond.baseRevision = 9;
	claimedAsSecond.turnSerial = 2;
	claimedAsSecond.actor = TacticalEntityId{7, 3};
	claimedAsSecond.payload = MoveTacticalIntent{123, 5, false};
	std::vector<std::uint8_t> intent;
	CHECK(EncodeTacticalIntent(claimedAsSecond, intent) ==
		TacticalIntentCodecResult::Success,
		"sender-identity test intent encodes with another claimed identity");
	std::array<std::uint8_t, CoopTacticalBaselineAckWireSize> baselineAck{};
	std::array<std::uint8_t, CoopTacticalDeltaAckWireSize> deltaAck{};
	std::array<std::uint8_t, CoopTacticalResyncRequestWireSize> resync{};
	for (std::size_t index = 0; index < baselineAck.size(); ++index)
		baselineAck[index] = static_cast<std::uint8_t>(0x80 + index);
	for (std::size_t index = 0; index < deltaAck.size(); ++index)
		deltaAck[index] = static_cast<std::uint8_t>(0x40 + index);
	for (std::size_t index = 0; index < resync.size(); ++index)
		resync[index] = static_cast<std::uint8_t>(0x20 + index);
	CHECK(first.peer->SendMessage(CoopTacticalIntentMessageName,
		intent.data(), intent.size(), first.events.server, false) &&
		PumpManyUntil(listener, {&first, &second}, [&] {
			return listener.pendingInboundCount() == 1;
		}), "authenticated intent is copied into the queue");
	CHECK(second.peer->SendMessage(CoopTacticalBaselineAckMessageName,
		baselineAck.data(), baselineAck.size(), second.events.server, false) &&
		PumpManyUntil(listener, {&first, &second}, [&] {
			return listener.pendingInboundCount() == 2;
		}), "authenticated baseline ACK is copied into the queue");
	CHECK(first.peer->SendMessage(CoopTacticalDeltaAckMessageName,
		deltaAck.data(), deltaAck.size(), first.events.server, false) &&
		PumpManyUntil(listener, {&first, &second}, [&] {
			return listener.pendingInboundCount() == 3;
		}), "authenticated delta ACK is copied into the queue");
	CHECK(second.peer->SendMessage(CoopTacticalResyncRequestMessageName,
		resync.data(), resync.size(), second.events.server, false) &&
		PumpManyUntil(listener, {&first, &second}, [&] {
			return listener.pendingInboundCount() == 4;
		}), "authenticated tactical resync is copied into the queue");

	FullEngineCoopTacticalInboundMessage queued;
	CHECK(listener.popInbound(queued) &&
		queued.kind == FullEngineCoopTacticalInboundKind::Intent &&
		queued.peerIdentity == firstPeer.peerIdentity &&
		queued.transport == firstTransport && queued.size == intent.size() &&
		std::equal(intent.begin(), intent.end(), queued.bytes.begin()),
		"intent dequeue uses transport-derived sender identity despite the copied peer claim");
	CHECK(listener.popInbound(queued) &&
		queued.kind == FullEngineCoopTacticalInboundKind::BaselineAck &&
		queued.peerIdentity == secondPeer.peerIdentity &&
		queued.transport == secondTransport &&
		queued.size == baselineAck.size() &&
		std::equal(baselineAck.begin(), baselineAck.end(), queued.bytes.begin()),
		"baseline ACK dequeue preserves its authenticated sender and bytes");
	CHECK(listener.popInbound(queued) &&
		queued.kind == FullEngineCoopTacticalInboundKind::DeltaAck &&
		queued.peerIdentity == firstPeer.peerIdentity &&
		queued.size == deltaAck.size() &&
		std::equal(deltaAck.begin(), deltaAck.end(), queued.bytes.begin()) &&
		sink.calls == 0,
		"delta ACK dequeue preserves FIFO delivery without gameplay execution");
	CHECK(listener.popInbound(queued) &&
		queued.kind == FullEngineCoopTacticalInboundKind::ResyncRequest &&
		queued.peerIdentity == secondPeer.peerIdentity &&
		queued.transport == secondTransport && queued.size == resync.size() &&
		std::equal(resync.begin(), resync.end(), queued.bytes.begin()) &&
		!listener.popInbound(queued),
		"resync dequeue preserves its authenticated sender and exact bytes");

	CoopTacticalIntentReceiptBytes receipt{};
	receipt.fill(0x11);
	std::array<std::uint8_t, CoopTacticalBaselineHeaderWireSize> baseline{};
	baseline.fill(0x22);
	std::array<std::uint8_t, CoopTacticalDeltaHeaderWireSize> delta{};
	delta.fill(0x33);
	CHECK(listener.sendToPeer(firstPeer.peerIdentity,
		CoopTacticalIntentReceiptMessageName,
		receipt.data(), receipt.size()),
		"targeted receipt queues for the first identity");
	CHECK(PumpManyUntil(listener, {&first, &second}, [&] {
		return first.receipt.count == 1;
	}), "first client receives its targeted receipt");
	CHECK(listener.sendToPeer(secondPeer.peerIdentity,
		CoopTacticalBaselineMessageName,
		baseline.data(), baseline.size()),
		"targeted baseline queues for the second identity");
	CHECK(PumpManyUntil(listener, {&first, &second}, [&] {
		return second.baseline.count == 1;
	}), "second client receives its targeted baseline");
	CHECK(listener.sendToPeer(firstPeer.peerIdentity,
		CoopTacticalDeltaMessageName, delta.data(), delta.size()),
		"targeted delta queues for the first identity");
	CHECK(PumpManyUntil(listener, {&first, &second}, [&] {
		return first.delta.count == 1;
	}), "first client receives its targeted delta");
	(void)PumpManyUntil(listener, {&first, &second}, [] { return false; }, 30);
	CHECK(first.receipt.bytes == std::vector<std::uint8_t>(
		receipt.begin(), receipt.end()) && second.receipt.count == 0 &&
		second.baseline.bytes == std::vector<std::uint8_t>(
			baseline.begin(), baseline.end()) && first.baseline.count == 0 &&
		first.delta.bytes == std::vector<std::uint8_t>(
			delta.begin(), delta.end()) && second.delta.count == 0,
		"identity-targeted outbound frames never cross-deliver");

	PeerIdentity unknown{};
	unknown.fill(0xfe);
	CHECK(!listener.sendToPeer(unknown,
		CoopTacticalIntentReceiptMessageName,
		receipt.data(), receipt.size()) &&
		!listener.sendToPeer(firstPeer.peerIdentity,
			CoopTacticalBaselineAckMessageName,
			baselineAck.data(), baselineAck.size()) &&
		!listener.sendToPeer(firstPeer.peerIdentity,
			CoopTacticalDeltaMessageName, delta.data(),
			MaximumCoopTacticalWireSize + 1),
		"targeted send rejects unknown identities, client-to-server names and oversized bytes");

	CHECK(first.peer->SendMessage(CoopTacticalIntentMessageName,
		intent.data(), intent.size(), first.events.server, false) &&
		PumpManyUntil(listener, {&first, &second}, [&] {
			return listener.pendingInboundCount() == 1;
		}), "stop-clear test leaves one authenticated inbound frame queued");
	listener.stop(20);
	mapped.fill(0xdd);
	CHECK(!listener.running() && listener.pendingInboundCount() == 0 &&
		listener.authenticatedPeerCount() == 0 &&
		!listener.authenticatedPeerForTransport(firstTransport, mapped) &&
		ingress.boundPeerCount() == 0 &&
		!listener.sendToPeer(firstPeer.peerIdentity,
			CoopTacticalIntentReceiptMessageName,
			receipt.data(), receipt.size()),
		"listener stop clears inbound bytes and every transport/identity mapping");
	DestroyClient(first);
	DestroyClient(second);
}

void TestTacticalQueueSaturationFailsClosed()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4452);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"tactical saturation admission session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	configuration.maximumQueuedTacticalMessages = 2;
	CHECK(StartListenerOnLoopback(listener, configuration),
		"two-entry tactical queue listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"tactical saturation client connects");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "tactical saturation client receives hello");
	AdmissionResponse peer;
	CHECK(AdmitAndAcknowledge(listener, client, authority, {&client}, peer),
		"tactical saturation client authenticates");

	std::array<std::uint8_t, TacticalIntentHeaderWireSize> intent{};
	intent.fill(0x5a);
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
		intent.data(), intent.size(), client.events.server, false) &&
		PumpManyUntil(listener, {&client}, [&] {
			return listener.pendingInboundCount() == 1;
		}), "first tactical frame occupies the bounded queue");
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
		intent.data(), intent.size(), client.events.server, false) &&
		PumpManyUntil(listener, {&client}, [&] {
			return listener.pendingInboundCount() == 2;
		}), "second tactical frame reaches the configured queue ceiling");
	CHECK(client.peer->SendMessage(CoopTacticalIntentMessageName,
		intent.data(), intent.size(), client.events.server, false),
		"one frame beyond the tactical queue ceiling sends");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.disconnected;
	}), "queue saturation closes the offending transport");
	CHECK(listener.pendingInboundCount() == 0 &&
		listener.authenticatedPeerCount() == 0 &&
		ingress.boundPeerCount() == 0 && ingress.admittedPeerCount() == 1 &&
		sink.calls == 0,
		"saturation clears that transport's stale queue while retaining only its reconnect credential");
	DestroyClient(client);
	listener.stop(0);
}

void TestTacticalOutboundHighWaterFailsClosed()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4453);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"outbound high-water admission session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	// A valid receipt plus its transport/name envelope cannot fit this bound,
	// making the fail-closed branch deterministic without depending on OS socket
	// buffer sizes.
	configuration.maximumPendingWriteBytesPerConnection =
		CoopTacticalIntentReceiptWireSize;
	CHECK(StartListenerOnLoopback(listener, configuration),
		"bounded-output listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"bounded-output client connects");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "bounded-output client receives hello");
	AdmissionResponse peer;
	CHECK(AdmitAndAcknowledge(listener, client, authority, {&client}, peer),
		"bounded-output client authenticates");

	CoopTacticalIntentReceiptBytes receipt{};
	receipt.fill(0x7c);
	CHECK(!listener.sendToPeer(peer.peerIdentity,
		CoopTacticalIntentReceiptMessageName,
		receipt.data(), receipt.size()),
		"an outbound frame beyond the configured byte high-water is refused");
	CHECK(listener.authenticatedPeerCount() == 0 &&
		ingress.boundPeerCount() == 0 && ingress.admittedPeerCount() == 1,
		"high-water refusal immediately clears authority transport binding");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.disconnected;
	}), "high-water refusal closes the slow transport");
	DestroyClient(client);
	listener.stop(0);
}

void TestHelloPrecedesPresendRequest()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4401);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"presend test admission session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	CHECK(StartListenerOnLoopback(listener, configuration),
		"presend test listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"presend client starts connecting");
	const Uint64 connectStarted = SDL_GetTicks();
	while (!client.events.connected && SDL_GetTicks() - connectStarted < 5000)
	{
		// Deliberately do not poll the server. The completed TCP client can send
		// admission before the listener consumes its queued accept event.
		PumpClient(*client.peer, client.events);
		SDL_Delay(2);
	}
	CHECK(client.events.connected,
		"presend client connects before server application poll");
	std::vector<int> order;
	client.hello.order = &order;
	client.hello.orderValue = 1;
	client.campaignBootstrap.order = &order;
	client.campaignBootstrap.orderValue = 2;
	client.response.order = &order;
	client.response.orderValue = 3;
	const AdmissionRequestBytes request = FirstJoinBytes(authority);
	CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
		request.data(), request.size(), client.events.server, false),
		"client pre-sends admission before accept event handling");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.response.count == 1;
	}), "pre-sent admission receives response");
	CHECK(order.size() == 3 && order[0] == 1 && order[1] == 2 &&
		order[2] == 3 && client.hello.count == 1 &&
		client.campaignBootstrap.count == 1 && client.response.count == 1,
		"pre-sent admission receives exactly hello then bootstrap then response");
	client.peer->Shutdown(20);
	(void)PumpManyUntil(listener, {&client}, [&] {
		return ingress.admittedPeerCount() == 0;
	});
	DestroyClient(client);
	listener.stop(0);
}

void TestReconnectDisplacesLiveTransport()
{
	SequentialTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	const AuthorityConfiguration authority = Authority(0x4402);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"reconnect displacement session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	configuration.maximumConnections = 2;
	CHECK(StartListenerOnLoopback(listener, configuration),
		"two-socket reconnect listener starts");

	LiveClient first;
	CHECK(StartClient(first, configuration.endpoint.port),
		"first reconnect client connects");
	CHECK(PumpManyUntil(listener, {&first}, [&] {
		return first.events.connected && first.hello.count == 1 &&
			first.campaignBootstrap.count == 1;
	}), "first reconnect client receives hello");
	const AdmissionRequestBytes firstJoin = FirstJoinBytes(authority);
	CHECK(first.peer->SendMessage(CoopAdmissionRequestMessageName,
		firstJoin.data(), firstJoin.size(), first.events.server, false),
		"first reconnect client requests a seat");
	CHECK(PumpManyUntil(listener, {&first}, [&] {
		return first.response.count == 1;
	}), "first reconnect client is admitted");
	AdmissionResponse credential;
	CHECK(DecodeLatestResponse(first, credential) && credential.admitted(),
		"first reconnect credential decodes");
	CHECK(SendAck(first, credential), "first reconnect credential ACK sends");
	TransportPeer firstTransport;
	CHECK(PumpManyUntil(listener, {&first}, [&] {
		return listener.authenticatedTransportForPeer(
			credential.peerIdentity, firstTransport);
	}), "first ACK publishes the transport-derived peer mapping");
	std::array<std::uint8_t, TacticalIntentHeaderWireSize> staleIntent{};
	staleIntent.fill(0x6c);
	CHECK(first.peer->SendMessage(CoopTacticalIntentMessageName,
		staleIntent.data(), staleIntent.size(), first.events.server, false) &&
		PumpManyUntil(listener, {&first}, [&] {
			return listener.pendingInboundCount() == 1;
		}), "old reconnect transport leaves one tactical frame queued");

	LiveClient second;
	CHECK(StartClient(second, configuration.endpoint.port),
		"second reconnect client connects beside old transport");
	CHECK(PumpManyUntil(listener, {&first, &second}, [&] {
		return second.events.connected && second.hello.count == 1 &&
			second.campaignBootstrap.count == 1;
	}), "second reconnect client receives hello");
	const AdmissionRequestBytes reconnect = ReconnectBytes(authority, credential);
	CHECK(second.peer->SendMessage(CoopAdmissionRequestMessageName,
		reconnect.data(), reconnect.size(), second.events.server, false),
		"second transport presents reconnect credential");
	CHECK(PumpManyUntil(listener, {&first, &second}, [&] {
		return second.response.count == 1 && first.events.disconnected;
	}), "reconnect response closes and displaces the old live transport");
	AdmissionResponse secondResponse;
	CHECK(DecodeLatestResponse(second, secondResponse) &&
		secondResponse.admitted() &&
		secondResponse.peerIdentity == credential.peerIdentity,
		"replacement transport retains the server peer identity");
	TransportPeer unchangedTransport{0xfeed};
	CHECK(listener.pendingInboundCount() == 0 &&
		listener.authenticatedPeerCount() == 0 &&
		!listener.authenticatedTransportForPeer(
			credential.peerIdentity, unchangedTransport) &&
		unchangedTransport == TransportPeer{0xfeed},
		"reconnect displacement clears stale bytes and withholds mapping until fresh ACK");
	CHECK(SendAck(second, secondResponse),
		"replacement transport ACKs its binding");
	TransportPeer secondTransport;
	CHECK(PumpManyUntil(listener, {&first, &second}, [&] {
		return listener.authenticatedTransportForPeer(
			credential.peerIdentity, secondTransport);
	}) && secondTransport != firstTransport,
		"fresh reconnect ACK replaces rather than aliases the old transport mapping");

	LiveClient third;
	CHECK(StartClient(third, configuration.endpoint.port),
		"third reconnect client connects after displaced socket closes");
	CHECK(PumpManyUntil(listener, {&second, &third}, [&] {
		return third.events.connected && third.hello.count == 1 &&
			third.campaignBootstrap.count == 1;
	}), "third reconnect client proves socket capacity was reclaimed");
	CHECK(third.peer->SendMessage(CoopAdmissionRequestMessageName,
		reconnect.data(), reconnect.size(), third.events.server, false),
		"third transport repeats reconnect");
	CHECK(PumpManyUntil(listener, {&second, &third}, [&] {
		return third.response.count == 1 && second.events.disconnected;
	}), "repeated reconnect displaces the immediately previous transport");
	AdmissionResponse thirdResponse;
	CHECK(DecodeLatestResponse(third, thirdResponse) &&
		thirdResponse.admitted() && listener.authenticatedPeerCount() == 0 &&
		SendAck(third, thirdResponse),
		"third replacement receives the credential but has no mapping before ACK");
	TransportPeer thirdTransport;
	CHECK(PumpManyUntil(listener, {&third}, [&] {
		return listener.authenticatedTransportForPeer(
			credential.peerIdentity, thirdTransport);
	}) && thirdTransport != secondTransport,
		"third replacement ACK installs one fresh transport mapping");
	CHECK(ingress.admittedPeerCount() == 1 &&
		ingress.boundPeerCount() == 1,
		"repeated reconnect retains one credential and one live binding");

	DestroyClient(first);
	DestroyClient(second);
	third.peer->Shutdown(20);
	(void)PumpManyUntil(listener, {&third}, [&] {
		return ingress.boundPeerCount() == 0;
	});
	DestroyClient(third);
	listener.stop(0);
}

void TestHandshakeDeadlineAndRejectionBudget()
{
	{
		SequentialTokenSource tokens;
		RejectingExecutionSink sink;
		FullEngineCoopIngress ingress(tokens, sink);
		FullEngineCoopAdmissionListener listener(ingress);
		const AuthorityConfiguration authority = Authority(0x4403);
		CHECK(ingress.beginAdmissionSession(authority) ==
			FullEngineCoopStartResult::Success,
			"handshake deadline session starts");
		FullEngineCoopAdmissionListenerConfiguration configuration =
			ListenerConfiguration(authority);
		configuration.handshakeTimeoutMilliseconds = 500;
		configuration.timeoutMilliseconds = 120000;
		CHECK(StartListenerOnLoopback(listener, configuration),
			"handshake deadline listener starts");
		LiveClient client;
		CHECK(StartClient(client, configuration.endpoint.port),
			"deadline client connects");
		CHECK(PumpManyUntil(listener, {&client}, [&] {
			return client.events.connected && client.hello.count == 1 &&
				client.campaignBootstrap.count == 1;
		}), "deadline client receives hello");
		const AdmissionRequestBytes request = FirstJoinBytes(authority);
		CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
			request.data(), request.size(), client.events.server, false),
			"deadline client requests pending seat");
		CHECK(PumpManyUntil(listener, {&client}, [&] {
			return client.response.count == 1;
		}), "deadline client receives acceptance but sends no ACK");
		CHECK(ingress.admittedPeerCount() == 1,
			"unacknowledged accepted seat is pending");
		const std::uint8_t keepalive = 1;
		const Uint64 deadlineStarted = SDL_GetTicks();
		while (!client.events.disconnected &&
			SDL_GetTicks() - deadlineStarted < 1000)
		{
			(void)client.peer->SendMessage("coop.unregistered.keepalive",
				&keepalive, 1, client.events.server, false);
			listener.poll();
			PumpClient(*client.peer, client.events);
			SDL_Delay(5);
		}
		CHECK(client.events.disconnected && ingress.admittedPeerCount() == 0,
			"handshake deadline ignores liveness traffic, closes socket, and reclaims pending seat");
		DestroyClient(client);
		listener.stop(0);
	}

	{
		SequentialTokenSource tokens;
		RejectingExecutionSink sink;
		FullEngineCoopIngress ingress(tokens, sink);
		FullEngineCoopAdmissionListener listener(ingress);
		const AuthorityConfiguration authority = Authority(0x4404);
		CHECK(ingress.beginAdmissionSession(authority) ==
			FullEngineCoopStartResult::Success,
			"rejection budget session starts");
		FullEngineCoopAdmissionListenerConfiguration configuration =
			ListenerConfiguration(authority);
		configuration.maximumRejectedAdmissionMessages = 2;
		CHECK(StartListenerOnLoopback(listener, configuration),
			"rejection budget listener starts");
		LiveClient client;
		CHECK(StartClient(client, configuration.endpoint.port),
			"rejection budget client connects");
		CHECK(PumpManyUntil(listener, {&client}, [&] {
			return client.events.connected && client.hello.count == 1 &&
				client.campaignBootstrap.count == 1;
		}), "rejection budget client receives hello");
		const std::uint8_t malformed = 0xff;
		CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
			&malformed, 1, client.events.server, false),
			"first rejected request sends");
		CHECK(PumpManyUntil(listener, {&client}, [&] {
			return client.response.count == 1;
		}) && !client.events.disconnected,
			"first rejected request consumes budget without closing");
		CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
			&malformed, 1, client.events.server, false),
			"second rejected request sends");
		CHECK(PumpManyUntil(listener, {&client}, [&] {
			return client.events.disconnected;
		}), "rejection budget closes transport at its exact bound");
		CHECK(ingress.admittedPeerCount() == 0,
			"rejected unauthenticated transport retains no admission capacity");
		DestroyClient(client);
		listener.stop(0);
	}
}

void TestStopDefersAcrossActiveHandler()
{
	StopDuringIssueTokenSource tokens;
	RejectingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	FullEngineCoopAdmissionListener listener(ingress);
	tokens.listener = &listener;
	const AuthorityConfiguration authority = Authority(0x4405);
	CHECK(ingress.beginAdmissionSession(authority) ==
		FullEngineCoopStartResult::Success,
		"deferred-stop admission session starts");
	FullEngineCoopAdmissionListenerConfiguration configuration =
		ListenerConfiguration(authority);
	CHECK(StartListenerOnLoopback(listener, configuration),
		"deferred-stop listener starts");
	LiveClient client;
	CHECK(StartClient(client, configuration.endpoint.port),
		"deferred-stop client connects");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return client.events.connected && client.hello.count == 1 &&
			client.campaignBootstrap.count == 1;
	}), "deferred-stop client receives hello");
	const AdmissionRequestBytes request = FirstJoinBytes(authority);
	CHECK(client.peer->SendMessage(CoopAdmissionRequestMessageName,
		request.data(), request.size(), client.events.server, false),
		"request enters token source that stops listener reentrantly");
	CHECK(PumpManyUntil(listener, {&client}, [&] {
		return !listener.running();
	}), "listener defers transport destruction until active handler and poll unwind");
	CHECK(ingress.admittedPeerCount() == 0 &&
		ingress.boundPeerCount() == 0,
		"deferred stop reclaims the handler-created pending credential");
	DestroyClient(client);
}
}

int main()
{
	SDL_Init(0);
	TestServerHelloCodec();
	TestAdmissionOnlyListener();
	TestLostAckCredentialAbandonRetry();
	TestUnauthenticatedTacticalTrafficFailsClosed();
	TestUnauthenticatedCampaignTrafficFailsClosed();
	TestAuthenticatedCampaignQueueAndTargetedDelivery();
	TestCampaignQueueSaturationFailsClosed();
	TestAuthenticatedTacticalQueueAndTargetedDelivery();
	TestTacticalQueueSaturationFailsClosed();
	TestTacticalOutboundHighWaterFailsClosed();
	TestHelloPrecedesPresendRequest();
	TestReconnectDisplacesLiveTransport();
	TestHandshakeDeadlineAndRejectionBudget();
	TestStopDefersAcrossActiveHandler();
	SDL_Quit();
	if (failures != 0)
	{
		std::printf("%d full-engine admission listener test(s) failed\n", failures);
		return 1;
	}
	std::puts("all full-engine admission listener tests passed");
	return 0;
}
