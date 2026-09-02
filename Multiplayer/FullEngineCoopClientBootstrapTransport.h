#ifndef MULTIPLAYER_FULL_ENGINE_COOP_CLIENT_BOOTSTRAP_TRANSPORT_H
#define MULTIPLAYER_FULL_ENGINE_COOP_CLIENT_BOOTSTRAP_TRANSPORT_H

#include "CoopCampaignBootstrapProtocol.h"
#include "CoopHandshakeProtocol.h"
#include "SdlNetTransport.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace CoopSession
{
inline constexpr std::size_t
	MaximumFullEngineCoopClientBootstrapInboundMessages = 2;
inline constexpr std::size_t
	MaximumFullEngineCoopClientBootstrapPendingWriteBytes =
		4u * 1024u * 1024u;
inline constexpr std::size_t
	DefaultFullEngineCoopClientBootstrapPendingWriteBytes =
		256u * 1024u;
inline constexpr unsigned
	MaximumFullEngineCoopClientBootstrapTimeoutMilliseconds = 600000;
static_assert(CoopServerHelloWireSize <= CoopCampaignBootstrapWireSize,
	"bootstrap callback storage must retain the complete server hello");

struct FullEngineCoopClientBootstrapTransportConfiguration
{
	// This is always a remote endpoint. The peer starts on port zero and
	// therefore cannot listen, accept a self-connection, or serve legacy arena
	// traffic.
	ja2::mp::net::SdlNetEndpoint serverEndpoint;
	unsigned timeoutMilliseconds = 120000;
	std::size_t maximumPendingWriteBytes =
		DefaultFullEngineCoopClientBootstrapPendingWriteBytes;
};

enum class FullEngineCoopClientBootstrapTransportState : std::uint8_t
{
	Idle,
	Connecting,
	AwaitingHello,
	AwaitingBootstrap,
	Complete,
	Failed,
	Stopped
};

enum class FullEngineCoopClientBootstrapTransportConnectResult : std::uint8_t
{
	Success,
	InvalidConfiguration,
	WrongThread,
	LifecycleBusy,
	TransportUnavailable,
	TransportStartFailed,
	TransportConnectFailed
};

enum class FullEngineCoopClientBootstrapTransportResult : std::uint8_t
{
	None,
	Success,
	Stopped,
	WrongThread,
	ConnectionAttemptFailed,
	ConnectionLost,
	Timeout,
	InboundCapacityReached,
	InboundWrongSize,
	WrongMessageOrder,
	HelloDecodeFailed,
	BootstrapDecodeFailed,
	DescriptorMismatch,
	UnexpectedConnection,
	UnexpectedTransportEvent,
	PendingWriteLimit,
	TransportFailure
};

// One-shot, outbound-only preflight used after SDL_Init and before installing
// SimulationRandom or constructing GameContext. SDL message callbacks perform
// fixed copies only. Decoding and state transitions happen on the constructing
// thread after SdlNetPeer::Poll has completely unwound.
class FullEngineCoopClientBootstrapTransport final
{
public:
	FullEngineCoopClientBootstrapTransport() noexcept;
	~FullEngineCoopClientBootstrapTransport();
	FullEngineCoopClientBootstrapTransport(
		const FullEngineCoopClientBootstrapTransport&) = delete;
	FullEngineCoopClientBootstrapTransport& operator=(
		const FullEngineCoopClientBootstrapTransport&) = delete;

	FullEngineCoopClientBootstrapTransportConnectResult connect(
		const FullEngineCoopClientBootstrapTransportConfiguration&
			configuration) noexcept;
	void poll() noexcept;
	void stop() noexcept;

	FullEngineCoopClientBootstrapTransportState state() const noexcept
	{
		return state_;
	}
	FullEngineCoopClientBootstrapTransportResult result() const noexcept
	{
		return result_;
	}
	std::size_t pendingInboundCount() const noexcept
	{
		return inboundCount_;
	}

	// Output is replaced only after the exact hello/bootstrap pair has been
	// validated and the owned socket has been shut down and destroyed.
	bool descriptor(
		CoopCampaignBootstrapDescriptor& descriptor) const noexcept;

private:
	enum class InboundKind : std::uint8_t
	{
		ServerHello,
		CampaignBootstrap
	};

	struct InboundMessage
	{
		InboundKind kind = InboundKind::ServerHello;
		ja2::mp::ConnectionId sender;
		std::size_t size = 0;
		std::array<std::uint8_t, CoopCampaignBootstrapWireSize> bytes{};
	};

	static void HandleServerHello(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignBootstrap(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void QueueFromCallback(ja2::mp::net::SdlNetMessage* message,
		void* context, InboundKind kind) noexcept;

	void queueInbound(const ja2::mp::net::SdlNetMessage& message,
		InboundKind kind) noexcept;
	void handleEvent(const ja2::mp::net::SdlNetEvent& event) noexcept;
	void deliverInbound() noexcept;
	void fail(
		FullEngineCoopClientBootstrapTransportResult result) noexcept;
	void requestCompletion(
		const CoopCampaignBootstrapDescriptor& descriptor) noexcept;
	void requestStop() noexcept;
	void finishClose() noexcept;
	void clearInbound() noexcept;
	bool registerMessages() noexcept;
	bool validConfiguration(
		const FullEngineCoopClientBootstrapTransportConfiguration&
			configuration) const noexcept;
	bool timeoutExpired() const noexcept;
	bool onMainThread() const noexcept;
	bool active() const noexcept;

	ja2::mp::net::SdlNetPeer* transport_ = nullptr;
	ja2::mp::ConnectionId server_;
	ja2::mp::ConnectionId pendingAccepted_;
	ja2::mp::ConnectionId callbackSender_;
	std::array<InboundMessage,
		MaximumFullEngineCoopClientBootstrapInboundMessages> inbound_{};
	CoopServerHello hello_;
	CoopCampaignBootstrapDescriptor stagedDescriptor_;
	CoopCampaignBootstrapDescriptor descriptor_;
	std::thread::id mainThread_;
	std::atomic<bool> wrongThreadObserved_{false};
	std::uint64_t startedAtMilliseconds_ = 0;
	std::size_t inboundHead_ = 0;
	std::size_t inboundCount_ = 0;
	std::size_t maximumPendingWriteBytes_ = 0;
	unsigned timeoutMilliseconds_ = 0;
	unsigned pollDepth_ = 0;
	unsigned handlerDepth_ = 0;
	FullEngineCoopClientBootstrapTransportState state_ =
		FullEngineCoopClientBootstrapTransportState::Idle;
	FullEngineCoopClientBootstrapTransportResult result_ =
		FullEngineCoopClientBootstrapTransportResult::None;
	FullEngineCoopClientBootstrapTransportResult pendingResult_ =
		FullEngineCoopClientBootstrapTransportResult::None;
	bool closePending_ = false;
	bool descriptorAvailable_ = false;
};
}

#endif
