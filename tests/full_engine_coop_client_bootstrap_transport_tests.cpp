#include "Multiplayer/FullEngineCoopClientBootstrapTransport.h"

#include "Multiplayer/CoopHandshakeProtocol.h"

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

using namespace CoopSession;
using namespace ja2::mp;
using namespace ja2::mp::net;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
		++failures; \
	} \
} while (false)

struct Capture
{
	std::size_t count = 0;
};

void CaptureMessage(SdlNetMessage*, void* context)
{
	if (context == nullptr) return;
	++static_cast<Capture*>(context)->count;
}

struct LoopbackServer
{
	SdlNetPeer* peer = nullptr;
	ConnectionId client;
	std::uint16_t port = 0;
	std::size_t disconnects = 0;
	Capture admissionRequests;

	~LoopbackServer()
	{
		stop();
	}

	bool start()
	{
		peer = CreateSdlNetPeer();
		if (peer == nullptr) return false;
		static std::uint64_t sequence = static_cast<std::uint64_t>(
			std::chrono::steady_clock::now().time_since_epoch().count());
		bool started = false;
		for (unsigned attempt = 0; attempt < 128 && !started; ++attempt)
		{
			port = static_cast<std::uint16_t>(
				40000 + sequence++ % 20000);
			started = peer->Start(
				2, SdlNetEndpoint(port, "127.0.0.1"));
		}
		if (!started)
		{
			DestroySdlNetPeer(peer);
			peer = nullptr;
			return false;
		}
		peer->SetMaximumIncomingConnections(1);
		peer->SetTimeout(120000);
		return peer->RegisterMessage(CoopAdmissionRequestMessageName,
			CaptureMessage, &admissionRequests);
	}

	void pump()
	{
		if (peer == nullptr) return;
		for (SdlNetEvent* event = peer->Poll(); event;
			event = peer->Poll())
		{
			if (event->size == 1 && event->data != nullptr)
			{
				if (event->data[0] == SDLNET_NEW_INCOMING_CONNECTION)
					client = event->connection;
				else if (event->data[0] ==
						SDLNET_DISCONNECTION_NOTIFICATION ||
					event->data[0] == SDLNET_CONNECTION_LOST)
					++disconnects;
			}
			peer->Release(event);
		}
	}

	bool send(const char* name, const std::uint8_t* bytes,
		std::size_t size)
	{
		return peer != nullptr && client &&
			peer->SendMessage(name, bytes, size, client, false);
	}

	void disconnect(bool notifyClient = true)
	{
		if (peer != nullptr && client)
			peer->CloseConnection(client, notifyClient);
	}

	void stop()
	{
		if (peer == nullptr) return;
		peer->Shutdown(0);
		DestroySdlNetPeer(peer);
		peer = nullptr;
		client = NoConnection;
	}
};

FullEngineCoopClientBootstrapTransportConfiguration Configuration(
	std::uint16_t port, unsigned timeoutMilliseconds = 5000)
{
	FullEngineCoopClientBootstrapTransportConfiguration configuration;
	configuration.serverEndpoint =
		SdlNetEndpoint(port, "127.0.0.1");
	configuration.timeoutMilliseconds = timeoutMilliseconds;
	return configuration;
}

CoopCampaignBootstrapDescriptor Descriptor(std::uint64_t epoch = 700,
	std::uint64_t seed = 0)
{
	CoopCampaignBootstrapDescriptor descriptor;
	descriptor.protocolVersion = CurrentProtocolVersion;
	descriptor.sessionEpoch = epoch;
	descriptor.campaignSeed = seed;
	for (std::size_t index = 0;
		index < descriptor.campaignIdentitySha256.size(); ++index)
		descriptor.campaignIdentitySha256[index] =
			static_cast<std::uint8_t>(0x20u + index);
	descriptor.runtimeFingerprint.schema = 9;
	descriptor.runtimeFingerprint.high = UINT64_C(0x1020304050607080);
	descriptor.runtimeFingerprint.low = UINT64_C(0x90a0b0c0d0e0f001);
	for (std::size_t index = 0;
		index < descriptor.contentManifestSha256.size(); ++index)
		descriptor.contentManifestSha256[index] =
			static_cast<std::uint8_t>(0x80u + index);
	return descriptor;
}

CoopServerHello Hello(
	const CoopCampaignBootstrapDescriptor& descriptor)
{
	CoopServerHello hello;
	hello.protocolVersion = descriptor.protocolVersion;
	hello.sessionEpoch = descriptor.sessionEpoch;
	hello.runtimeFingerprint = descriptor.runtimeFingerprint;
	hello.contentManifestSha256 = descriptor.contentManifestSha256;
	return hello;
}

CoopServerHelloBytes EncodeHello(const CoopServerHello& hello)
{
	CoopServerHelloBytes bytes{};
	CHECK(EncodeCoopServerHello(hello, bytes),
		"server hello test fixture encodes");
	return bytes;
}

CoopCampaignBootstrapBytes EncodeBootstrap(
	const CoopCampaignBootstrapDescriptor& descriptor)
{
	CoopCampaignBootstrapBytes bytes{};
	CHECK(EncodeCoopCampaignBootstrap(descriptor, bytes),
		"campaign bootstrap test fixture encodes");
	return bytes;
}

CoopCampaignBootstrapDescriptor SentinelDescriptor()
{
	CoopCampaignBootstrapDescriptor sentinel;
	sentinel.protocolVersion = 77;
	sentinel.sessionEpoch = UINT64_C(0xaaaaaaaaaaaaaaaa);
	sentinel.campaignSeed = UINT64_C(0xbbbbbbbbbbbbbbbb);
	sentinel.campaignIdentitySha256.fill(0xcc);
	sentinel.runtimeFingerprint = RuntimeCompatibilityFingerprint{
		55, UINT64_C(0xdddddddddddddddd),
		UINT64_C(0xeeeeeeeeeeeeeeee)};
	sentinel.contentManifestSha256.fill(0xff);
	return sentinel;
}

bool DescriptorUnavailableAndPreserved(
	const FullEngineCoopClientBootstrapTransport& transport)
{
	const CoopCampaignBootstrapDescriptor sentinel = SentinelDescriptor();
	CoopCampaignBootstrapDescriptor output = sentinel;
	return !transport.descriptor(output) &&
		SameCoopCampaignBootstrapDescriptor(output, sentinel);
}

template<typename Predicate>
bool PumpUntil(LoopbackServer& server,
	FullEngineCoopClientBootstrapTransport& transport,
	Predicate predicate, unsigned timeoutMilliseconds = 5000)
{
	const Uint64 started = SDL_GetTicks();
	for (;;)
	{
		server.pump();
		transport.poll();
		if (predicate()) return true;
		if (SDL_GetTicks() - started >= timeoutMilliseconds) return false;
		SDL_Delay(2);
	}
}

bool ConnectAndAwaitHello(LoopbackServer& server,
	FullEngineCoopClientBootstrapTransport& transport,
	unsigned timeoutMilliseconds = 5000)
{
	if (transport.connect(Configuration(server.port, timeoutMilliseconds)) !=
		FullEngineCoopClientBootstrapTransportConnectResult::Success)
		return false;
	return PumpUntil(server, transport, [&] {
		return server.client && transport.state() ==
			FullEngineCoopClientBootstrapTransportState::AwaitingHello;
	});
}

bool WaitForFailure(LoopbackServer& server,
	FullEngineCoopClientBootstrapTransport& transport,
	FullEngineCoopClientBootstrapTransportResult expected)
{
	const bool terminal = PumpUntil(server, transport, [&] {
		return transport.state() ==
			FullEngineCoopClientBootstrapTransportState::Failed;
	});
	return terminal && transport.result() == expected &&
		transport.pendingInboundCount() == 0 &&
		DescriptorUnavailableAndPreserved(transport);
}

void TestHappyPathZeroSeedAndOneShotClose()
{
	LoopbackServer server;
	CHECK(server.start(), "happy-path loopback server starts");
	FullEngineCoopClientBootstrapTransport transport;
	CHECK(ConnectAndAwaitHello(server, transport),
		"outbound-only bootstrap client connects");
	CHECK(DescriptorUnavailableAndPreserved(transport),
		"descriptor is unavailable before both frames and preserves output");

	const CoopCampaignBootstrapDescriptor expected = Descriptor(701, 0);
	const CoopServerHelloBytes hello = EncodeHello(Hello(expected));
	const CoopCampaignBootstrapBytes bootstrap = EncodeBootstrap(expected);
	CHECK(server.send(CoopServerHelloMessageName,
			hello.data(), hello.size()) &&
		server.send(CoopCampaignBootstrapMessageName,
			bootstrap.data(), bootstrap.size()),
		"server sends exact ordered hello and zero-seed bootstrap burst");
	CHECK(DescriptorUnavailableAndPreserved(transport),
		"network writes alone cannot publish before caller poll");
	CHECK(PumpUntil(server, transport, [&] {
		return transport.state() ==
			FullEngineCoopClientBootstrapTransportState::Complete;
	}), "ordered preflight completes over real SDL3_net loopback");
	CHECK(transport.result() ==
			FullEngineCoopClientBootstrapTransportResult::Success &&
		transport.pendingInboundCount() == 0,
		"completion is terminal and drains the fixed callback FIFO");
	CoopCampaignBootstrapDescriptor actual = SentinelDescriptor();
	CHECK(transport.descriptor(actual) &&
		SameCoopCampaignBootstrapDescriptor(actual, expected) &&
		actual.campaignSeed == 0,
		"descriptor publishes exactly once after socket teardown with zero seed");
	CHECK(PumpUntil(server, transport, [&] {
		return server.disconnects != 0;
	}), "Complete has already closed the one-shot peer");
	CHECK(server.admissionRequests.count == 0,
		"early preflight sends no admission request");
	CHECK(transport.connect(Configuration(server.port)) ==
		FullEngineCoopClientBootstrapTransportConnectResult::LifecycleBusy,
		"completed preflight cannot reconnect or replace its descriptor");
	transport.stop();
	CHECK(transport.state() ==
			FullEngineCoopClientBootstrapTransportState::Complete,
		"stop cannot rewrite an immutable successful result");
}

void TestExactNamespacesAndSeparatedDelivery()
{
	LoopbackServer server;
	CHECK(server.start(), "namespace loopback server starts");
	FullEngineCoopClientBootstrapTransport transport;
	CHECK(ConnectAndAwaitHello(server, transport),
		"namespace bootstrap client connects");
	const std::uint8_t legacy = 0x91;
	CHECK(server.send("legacy.v3.2.rpc", &legacy, 1),
		"server can send an unregistered legacy namespace");
	for (unsigned iteration = 0; iteration < 8; ++iteration)
	{
		server.pump();
		transport.poll();
		SDL_Delay(1);
	}
	CHECK(transport.state() ==
			FullEngineCoopClientBootstrapTransportState::AwaitingHello &&
		DescriptorUnavailableAndPreserved(transport),
		"legacy namespace has no handler and cannot affect preflight");

	const CoopCampaignBootstrapDescriptor expected =
		Descriptor(702, UINT64_C(0xfedcba9876543210));
	const CoopServerHelloBytes hello = EncodeHello(Hello(expected));
	CHECK(server.send(CoopServerHelloMessageName,
		hello.data(), hello.size()), "server sends hello separately");
	CHECK(PumpUntil(server, transport, [&] {
		return transport.state() ==
			FullEngineCoopClientBootstrapTransportState::AwaitingBootstrap;
	}), "hello decodes only after poll unwinds");
	CHECK(DescriptorUnavailableAndPreserved(transport),
		"hello alone never publishes campaign identity");
	const CoopCampaignBootstrapBytes bootstrap = EncodeBootstrap(expected);
	CHECK(server.send(CoopCampaignBootstrapMessageName,
		bootstrap.data(), bootstrap.size()),
		"server sends bootstrap separately");
	CHECK(PumpUntil(server, transport, [&] {
		return transport.state() ==
			FullEngineCoopClientBootstrapTransportState::Complete;
	}), "separated exact pair completes");
	CHECK(server.admissionRequests.count == 0,
		"separated preflight still performs no admission write");
}

void TestOutOfOrderAndDuplicateFrames()
{
	{
		LoopbackServer server;
		CHECK(server.start(), "out-of-order server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"out-of-order client connects");
		const CoopCampaignBootstrapBytes bootstrap =
			EncodeBootstrap(Descriptor(710));
		CHECK(server.send(CoopCampaignBootstrapMessageName,
			bootstrap.data(), bootstrap.size()),
			"server sends bootstrap before hello");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::WrongMessageOrder),
			"bootstrap-before-hello fails closed without output");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "duplicate-hello server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"duplicate-hello client connects");
		const CoopServerHelloBytes hello =
			EncodeHello(Hello(Descriptor(711)));
		CHECK(server.send(CoopServerHelloMessageName,
				hello.data(), hello.size()) &&
			server.send(CoopServerHelloMessageName,
				hello.data(), hello.size()),
			"server sends duplicate hello frames");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::WrongMessageOrder),
			"duplicate hello fails exact-order transaction");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "duplicate-bootstrap server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"duplicate-bootstrap client connects");
		const CoopCampaignBootstrapDescriptor descriptor = Descriptor(712);
		const CoopServerHelloBytes hello = EncodeHello(Hello(descriptor));
		CHECK(server.send(CoopServerHelloMessageName,
			hello.data(), hello.size()), "server sends first exact hello");
		CHECK(PumpUntil(server, transport, [&] {
			return transport.state() ==
				FullEngineCoopClientBootstrapTransportState::AwaitingBootstrap;
		}), "client awaits bootstrap before duplicate test");
		const CoopCampaignBootstrapBytes bootstrap =
			EncodeBootstrap(descriptor);
		CHECK(server.send(CoopCampaignBootstrapMessageName,
				bootstrap.data(), bootstrap.size()) &&
			server.send(CoopCampaignBootstrapMessageName,
				bootstrap.data(), bootstrap.size()),
			"server sends duplicate bootstrap frames in one burst");
		SDL_Delay(10);
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::WrongMessageOrder),
			"already-copied duplicate bootstrap prevents publication");
	}
}

void TestWrongSizesAndCorruptPayloads()
{
	{
		LoopbackServer server;
		CHECK(server.start(), "short-hello server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"short-hello client connects");
		const CoopServerHelloBytes hello =
			EncodeHello(Hello(Descriptor(720)));
		CHECK(server.send(CoopServerHelloMessageName,
			hello.data(), hello.size() - 1),
			"server sends truncated hello");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::InboundWrongSize),
			"truncated hello fails before copy/decode");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "long-hello server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"long-hello client connects");
		std::array<std::uint8_t, CoopServerHelloWireSize + 1> bytes{};
		CHECK(server.send(CoopServerHelloMessageName,
			bytes.data(), bytes.size()), "server sends oversized hello");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::InboundWrongSize),
			"oversized hello fails before fixed copy");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "corrupt-hello server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"corrupt-hello client connects");
		CoopServerHelloBytes hello =
			EncodeHello(Hello(Descriptor(721)));
		hello[0] ^= 0xff;
		CHECK(server.send(CoopServerHelloMessageName,
			hello.data(), hello.size()), "server sends corrupt exact-size hello");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::HelloDecodeFailed),
			"hello codec rejection fails transport closed");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "short-bootstrap server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"short-bootstrap client connects");
		const CoopCampaignBootstrapDescriptor descriptor = Descriptor(722);
		const CoopServerHelloBytes hello = EncodeHello(Hello(descriptor));
		CHECK(server.send(CoopServerHelloMessageName,
			hello.data(), hello.size()), "short-bootstrap server sends hello");
		CHECK(PumpUntil(server, transport, [&] {
			return transport.state() ==
				FullEngineCoopClientBootstrapTransportState::AwaitingBootstrap;
		}), "short-bootstrap client accepts hello");
		const CoopCampaignBootstrapBytes bootstrap =
			EncodeBootstrap(descriptor);
		CHECK(server.send(CoopCampaignBootstrapMessageName,
			bootstrap.data(), bootstrap.size() - 1),
			"server sends truncated bootstrap");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::InboundWrongSize),
			"truncated bootstrap fails before copy/decode");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "long-bootstrap server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"long-bootstrap client connects");
		const CoopCampaignBootstrapDescriptor descriptor = Descriptor(723);
		const CoopServerHelloBytes hello = EncodeHello(Hello(descriptor));
		CHECK(server.send(CoopServerHelloMessageName,
			hello.data(), hello.size()), "long-bootstrap server sends hello");
		CHECK(PumpUntil(server, transport, [&] {
			return transport.state() ==
				FullEngineCoopClientBootstrapTransportState::AwaitingBootstrap;
		}), "long-bootstrap client accepts hello");
		std::array<std::uint8_t,
			CoopCampaignBootstrapWireSize + 1> bytes{};
		CHECK(server.send(CoopCampaignBootstrapMessageName,
			bytes.data(), bytes.size()), "server sends oversized bootstrap");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::InboundWrongSize),
			"oversized bootstrap fails before fixed copy");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "corrupt-bootstrap server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"corrupt-bootstrap client connects");
		const CoopCampaignBootstrapDescriptor descriptor = Descriptor(724);
		const CoopServerHelloBytes hello = EncodeHello(Hello(descriptor));
		CHECK(server.send(CoopServerHelloMessageName,
			hello.data(), hello.size()), "corrupt-bootstrap server sends hello");
		CHECK(PumpUntil(server, transport, [&] {
			return transport.state() ==
				FullEngineCoopClientBootstrapTransportState::AwaitingBootstrap;
		}), "corrupt-bootstrap client accepts hello");
		CoopCampaignBootstrapBytes bootstrap = EncodeBootstrap(descriptor);
		bootstrap[112] ^= 0x01;
		CHECK(server.send(CoopCampaignBootstrapMessageName,
			bootstrap.data(), bootstrap.size()),
			"server sends corrupt exact-size bootstrap");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::
				BootstrapDecodeFailed),
			"bootstrap codec rejection fails transport closed");
	}
}

enum class RepeatedMismatch
{
	Protocol,
	SessionEpoch,
	RuntimeSchema,
	RuntimeHigh,
	RuntimeLow,
	ContentManifest
};

void TestRepeatedDescriptorMismatch(RepeatedMismatch mismatch,
	std::uint64_t epoch)
{
	LoopbackServer server;
	CHECK(server.start(), "descriptor-mismatch server starts");
	FullEngineCoopClientBootstrapTransport transport;
	CHECK(ConnectAndAwaitHello(server, transport),
		"descriptor-mismatch client connects");
	const CoopCampaignBootstrapDescriptor descriptor = Descriptor(epoch);
	CoopServerHello hello = Hello(descriptor);
	switch (mismatch)
	{
		case RepeatedMismatch::Protocol:
			hello.protocolVersion =
				static_cast<std::uint16_t>(descriptor.protocolVersion + 1);
			break;
		case RepeatedMismatch::SessionEpoch:
			++hello.sessionEpoch;
			break;
		case RepeatedMismatch::RuntimeSchema:
			++hello.runtimeFingerprint.schema;
			break;
		case RepeatedMismatch::RuntimeHigh:
			++hello.runtimeFingerprint.high;
			break;
		case RepeatedMismatch::RuntimeLow:
			++hello.runtimeFingerprint.low;
			break;
		case RepeatedMismatch::ContentManifest:
			hello.contentManifestSha256[0] ^= 0x01;
			break;
	}
	const CoopServerHelloBytes helloBytes = EncodeHello(hello);
	const CoopCampaignBootstrapBytes bootstrap =
		EncodeBootstrap(descriptor);
	CHECK(server.send(CoopServerHelloMessageName,
			helloBytes.data(), helloBytes.size()) &&
		server.send(CoopCampaignBootstrapMessageName,
			bootstrap.data(), bootstrap.size()),
		"server sends individually mismatched repeated descriptors");
	CHECK(WaitForFailure(server, transport,
		FullEngineCoopClientBootstrapTransportResult::DescriptorMismatch),
		"every repeated hello/bootstrap field is exact-matched");
}

void TestEveryRepeatedDescriptorField()
{
	TestRepeatedDescriptorMismatch(RepeatedMismatch::Protocol, 730);
	TestRepeatedDescriptorMismatch(RepeatedMismatch::SessionEpoch, 731);
	TestRepeatedDescriptorMismatch(RepeatedMismatch::RuntimeSchema, 732);
	TestRepeatedDescriptorMismatch(RepeatedMismatch::RuntimeHigh, 733);
	TestRepeatedDescriptorMismatch(RepeatedMismatch::RuntimeLow, 734);
	TestRepeatedDescriptorMismatch(RepeatedMismatch::ContentManifest, 735);
}

void TestCapacityTimeoutAndDisconnect()
{
	{
		LoopbackServer server;
		CHECK(server.start(), "capacity server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"capacity client connects");
		const CoopServerHelloBytes hello =
			EncodeHello(Hello(Descriptor(740)));
		bool sent = true;
		for (unsigned index = 0; index < 8; ++index)
			sent = sent && server.send(CoopServerHelloMessageName,
				hello.data(), hello.size());
		CHECK(sent, "server queues an adversarial registered-frame burst");
		SDL_Delay(10);
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::
				InboundCapacityReached),
			"third callback frame saturates the fixed two-entry FIFO");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "timeout server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport, 30),
			"timeout client reaches hello wait within deadline");
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::Timeout),
			"monotonic wall deadline covers a silent handshake");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "disconnect server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"disconnect client connects");
		server.disconnect(true);
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::ConnectionLost),
			"disconnect before bootstrap fails closed");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "disconnect-after-frames server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(ConnectAndAwaitHello(server, transport),
			"disconnect-after-frames client connects");
		const CoopCampaignBootstrapDescriptor descriptor = Descriptor(741);
		const CoopServerHelloBytes hello = EncodeHello(Hello(descriptor));
		const CoopCampaignBootstrapBytes bootstrap =
			EncodeBootstrap(descriptor);
		CHECK(server.send(CoopServerHelloMessageName,
				hello.data(), hello.size()) &&
			server.send(CoopCampaignBootstrapMessageName,
				bootstrap.data(), bootstrap.size()),
			"server queues the pair before disconnecting");
		server.disconnect(true);
		SDL_Delay(10);
		CHECK(WaitForFailure(server, transport,
			FullEngineCoopClientBootstrapTransportResult::ConnectionLost),
			"same-poll disconnect prevents staged descriptor publication");
	}
}

void TestConfigurationThreadingAndConnectionFailure()
{
	{
		FullEngineCoopClientBootstrapTransport transport;
		FullEngineCoopClientBootstrapTransportConfiguration invalid;
		CHECK(transport.connect(invalid) ==
			FullEngineCoopClientBootstrapTransportConnectResult::
				InvalidConfiguration,
			"zero endpoint is rejected without opening a listener");
		invalid.serverEndpoint = SdlNetEndpoint(1, "127.0.0.1");
		invalid.timeoutMilliseconds = 0;
		CHECK(transport.connect(invalid) ==
			FullEngineCoopClientBootstrapTransportConnectResult::
				InvalidConfiguration,
			"zero timeout is invalid");
		invalid.timeoutMilliseconds =
			MaximumFullEngineCoopClientBootstrapTimeoutMilliseconds + 1;
		CHECK(transport.connect(invalid) ==
			FullEngineCoopClientBootstrapTransportConnectResult::
				InvalidConfiguration,
			"unbounded timeout is invalid");
		invalid.timeoutMilliseconds = 100;
		invalid.maximumPendingWriteBytes = 0;
		CHECK(transport.connect(invalid) ==
			FullEngineCoopClientBootstrapTransportConnectResult::
				InvalidConfiguration,
			"zero pending-write bound is invalid");
		invalid.maximumPendingWriteBytes =
			MaximumFullEngineCoopClientBootstrapPendingWriteBytes + 1;
		CHECK(transport.connect(invalid) ==
			FullEngineCoopClientBootstrapTransportConnectResult::
				InvalidConfiguration,
			"oversized pending-write bound is invalid");
		invalid.maximumPendingWriteBytes = 1;
		std::memset(invalid.serverEndpoint.host, 'x',
			sizeof(invalid.serverEndpoint.host));
		CHECK(transport.connect(invalid) ==
			FullEngineCoopClientBootstrapTransportConnectResult::
				InvalidConfiguration,
			"unterminated host is invalid");
		CHECK(transport.state() ==
				FullEngineCoopClientBootstrapTransportState::Idle &&
			transport.result() ==
				FullEngineCoopClientBootstrapTransportResult::None &&
			DescriptorUnavailableAndPreserved(transport),
			"configuration rejection leaves the one-shot object retryable");
	}

	{
		LoopbackServer reservation;
		CHECK(reservation.start(), "closed-port reservation starts");
		const std::uint16_t closedPort = reservation.port;
		reservation.stop();
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(transport.connect(Configuration(closedPort)) ==
			FullEngineCoopClientBootstrapTransportConnectResult::Success,
			"connection refusal begins asynchronously");
		LoopbackServer inert;
		CHECK(WaitForFailure(inert, transport,
			FullEngineCoopClientBootstrapTransportResult::
				ConnectionAttemptFailed),
			"connection refusal destroys transport and preserves output");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "wrong-thread server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(transport.connect(Configuration(server.port)) ==
			FullEngineCoopClientBootstrapTransportConnectResult::Success,
			"wrong-thread test begins an active connection");
		std::thread wrongThread([&] { transport.poll(); });
		wrongThread.join();
		transport.poll();
		CHECK(transport.state() ==
				FullEngineCoopClientBootstrapTransportState::Failed &&
			transport.result() ==
				FullEngineCoopClientBootstrapTransportResult::WrongThread &&
			DescriptorUnavailableAndPreserved(transport),
			"off-thread poll records a deferred fail-closed teardown");
	}

	{
		LoopbackServer server;
		CHECK(server.start(), "stop-lifecycle server starts");
		FullEngineCoopClientBootstrapTransport transport;
		CHECK(transport.connect(Configuration(server.port)) ==
			FullEngineCoopClientBootstrapTransportConnectResult::Success,
			"stop-lifecycle connection starts");
		CHECK(transport.connect(Configuration(server.port)) ==
			FullEngineCoopClientBootstrapTransportConnectResult::LifecycleBusy,
			"active one-shot connection cannot be replaced");
		transport.stop();
		CHECK(transport.state() ==
				FullEngineCoopClientBootstrapTransportState::Stopped &&
			transport.result() ==
				FullEngineCoopClientBootstrapTransportResult::Stopped &&
			DescriptorUnavailableAndPreserved(transport),
			"explicit stop synchronously destroys an unpumped peer");
	}
}
}

int main()
{
	CHECK(SDL_Init(0),
		"SDL initializes before early bootstrap transport construction");
	TestHappyPathZeroSeedAndOneShotClose();
	TestExactNamespacesAndSeparatedDelivery();
	TestOutOfOrderAndDuplicateFrames();
	TestWrongSizesAndCorruptPayloads();
	TestEveryRepeatedDescriptorField();
	TestCapacityTimeoutAndDisconnect();
	TestConfigurationThreadingAndConnectionFailure();
	SDL_Quit();
	if (failures != 0)
	{
		std::printf("%d full-engine co-op client bootstrap transport test(s) failed\n",
			failures);
		return 1;
	}
	std::puts("full-engine co-op client bootstrap transport tests passed");
	return 0;
}
