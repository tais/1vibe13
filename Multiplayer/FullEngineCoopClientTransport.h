#ifndef MULTIPLAYER_FULL_ENGINE_COOP_CLIENT_TRANSPORT_H
#define MULTIPLAYER_FULL_ENGINE_COOP_CLIENT_TRANSPORT_H

#include "CoopCampaignSyncProtocol.h"
#include "FullEngineCoopClient.h"
#include "SdlNetTransport.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <thread>

namespace CoopSession
{
inline constexpr std::size_t MaximumFullEngineCoopClientInboundMessages = 16;
inline constexpr std::size_t MaximumFullEngineCoopClientInboundWireSize =
	MaximumCoopTacticalWireSize > MaximumCoopCampaignSyncWireSize
		? MaximumCoopTacticalWireSize
		: MaximumCoopCampaignSyncWireSize;
static_assert(MaximumCoopTacticalWireSize <=
	MaximumFullEngineCoopClientInboundWireSize,
	"the client callback FIFO must retain every tactical frame");
static_assert(MaximumCoopCampaignSyncWireSize <=
	MaximumFullEngineCoopClientInboundWireSize,
	"the client callback FIFO must retain every campaign-sync frame");
static_assert(MaximumFullEngineCoopClientInboundWireSize < 64u * 1024u,
	"the client callback slot must remain below the public payload ceiling");
inline constexpr std::size_t MaximumFullEngineCoopClientPendingWriteBytes =
	4u * 1024u * 1024u;
inline constexpr std::size_t DefaultFullEngineCoopClientPendingWriteBytes =
	256u * 1024u;
inline constexpr unsigned MaximumFullEngineCoopClientTimeoutMilliseconds =
	600000;
inline constexpr std::size_t FullEngineCoopCampaignInboundRateBytesPerSecond =
	ja2::mp::net::MaximumSdlNetInboundMessageRateBytesPerSecond;
inline constexpr std::size_t FullEngineCoopCampaignInboundBurstBytes =
	ja2::mp::net::MaximumSdlNetInboundMessageBurstBytes;
// One protocol window may arrive per rendered frame. Cover the supported
// 144 FPS cap without making the generic SDL peer's strict default any larger.
static_assert(FullEngineCoopCampaignInboundRateBytesPerSecond >=
	MaximumCoopCampaignSyncWindowWireBytes * 144u,
	"campaign receive budget must cover one chunk window at 144 FPS");

struct FullEngineCoopClientTransportConfiguration
{
	// This is always a remote endpoint. The owned peer starts without a
	// listener, so it cannot accept a self-connection or legacy arena traffic.
	ja2::mp::net::SdlNetEndpoint serverEndpoint;
	unsigned timeoutMilliseconds = 120000;
	std::size_t maximumQueuedInboundMessages =
		MaximumFullEngineCoopClientInboundMessages;
	std::size_t maximumPendingWriteBytes =
		DefaultFullEngineCoopClientPendingWriteBytes;
};

enum class FullEngineCoopClientTransportConnectResult : std::uint8_t
{
	Success,
	InvalidConfiguration,
	WrongThread,
	LifecycleBusy,
	ClientRejected,
	TransportUnavailable,
	TransportStartFailed,
	TransportConnectFailed
};

enum class FullEngineCoopClientTransportFailure : std::uint8_t
{
	None,
	WrongThread,
	ConnectionAttemptFailed,
	ConnectionLost,
	InboundCapacityReached,
	InboundMessageTooLarge,
	UnexpectedConnection,
	UnexpectedTransportEvent,
	PendingWriteLimit,
	TransportFailure,
	ClientRejected
};

// Raw campaign frames cross this boundary only after the SDL callback pump has
// unwound. A future campaign-sync core owns decoding, storage, and checkpoint
// loading; this socket adapter only preserves bounded wire order.
class FullEngineCoopClientCampaignSyncSink
{
public:
	virtual ~FullEngineCoopClientCampaignSyncSink() = default;
	virtual bool receiveCampaignMetadata(
		const std::uint8_t* bytes, std::size_t size) noexcept = 0;
	virtual bool receiveCampaignChunk(
		const std::uint8_t* bytes, std::size_t size) noexcept = 0;
	virtual bool receiveCampaignComplete(
		const std::uint8_t* bytes, std::size_t size) noexcept = 0;
	virtual bool receiveCampaignReject(
		const std::uint8_t* bytes, std::size_t size) noexcept = 0;
};

// Socket-only adapter for the passive FullEngineCoopClient core. Construct it
// before the core, pass it as the core's FullEngineCoopClientWire, and then
// bind that core with connect(). SDL message callbacks only copy into the
// fixed FIFO; every core transition happens later on the constructing thread,
// after SdlNetPeer::Poll has returned.
class FullEngineCoopClientTransport final : public FullEngineCoopClientWire
{
public:
	FullEngineCoopClientTransport() noexcept;
	~FullEngineCoopClientTransport() override;
	FullEngineCoopClientTransport(
		const FullEngineCoopClientTransport&) = delete;
	FullEngineCoopClientTransport& operator=(
		const FullEngineCoopClientTransport&) = delete;

	FullEngineCoopClientTransportConnectResult connect(
		FullEngineCoopClient& client,
		const FullEngineCoopClientTransportConfiguration& configuration) noexcept;
	FullEngineCoopClientTransportConnectResult connect(
		FullEngineCoopClient& client,
		FullEngineCoopClientCampaignSyncSink& campaignSink,
		const FullEngineCoopClientTransportConfiguration& configuration) noexcept;
	void poll() noexcept;
	void stop(unsigned drainMilliseconds = 0) noexcept;

	bool running() const noexcept { return running_; }
	bool connected() const noexcept { return connected_; }
	std::size_t pendingInboundCount() const noexcept { return inboundCount_; }
	FullEngineCoopClientTransportFailure lastFailure() const noexcept
	{
		return lastFailure_;
	}

	bool send(const char* messageName, const std::uint8_t* bytes,
		std::size_t size) noexcept override;
	void close() noexcept override;

private:
	enum class InboundKind : std::uint8_t
	{
		ServerHello,
		AdmissionResponse,
		TacticalBaseline,
		TacticalDelta,
		TacticalReceipt,
		SelfRetirementResult,
		CampaignMetadata,
		CampaignChunk,
		CampaignComplete,
		CampaignReject
	};

	struct InboundMessage
	{
		InboundKind kind = InboundKind::ServerHello;
		ja2::mp::ConnectionId sender;
		std::size_t size = 0;
		std::array<std::uint8_t,
			MaximumFullEngineCoopClientInboundWireSize> bytes{};
	};

	static void HandleServerHello(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleAdmissionResponse(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleTacticalBaseline(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleTacticalDelta(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleTacticalReceipt(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleSelfRetirementResult(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignMetadata(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignChunk(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignComplete(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignReject(
		ja2::mp::net::SdlNetMessage* message, void* context);

	static void QueueFromCallback(ja2::mp::net::SdlNetMessage* message,
		void* context, InboundKind kind) noexcept;
	void queueInbound(const ja2::mp::net::SdlNetMessage& message,
		InboundKind kind) noexcept;
	void handleEvent(const ja2::mp::net::SdlNetEvent& event) noexcept;
	void deliverInbound() noexcept;
	FullEngineCoopClientResult deliver(
		const InboundMessage& message) noexcept;
	void clearInbound() noexcept;
	void failTransport(
		FullEngineCoopClientTransportFailure failure) noexcept;
	void requestClose(bool notifyClient,
		unsigned drainMilliseconds = 0) noexcept;
	void finishClose() noexcept;
	bool onMainThread() const noexcept;
	FullEngineCoopClientTransportConnectResult connectInternal(
		FullEngineCoopClient& client,
		FullEngineCoopClientCampaignSyncSink* campaignSink,
		const FullEngineCoopClientTransportConfiguration& configuration) noexcept;
	bool registerMessages() noexcept;
	bool validInbound(InboundKind kind, std::size_t size) const noexcept;
	bool validConfiguration(
		const FullEngineCoopClientTransportConfiguration& configuration)
		const noexcept;
	bool validOutbound(const char* messageName,
		std::size_t size) const noexcept;

	FullEngineCoopClient* client_ = nullptr;
	FullEngineCoopClientCampaignSyncSink* campaignSink_ = nullptr;
	ja2::mp::net::SdlNetPeer* transport_ = nullptr;
	ja2::mp::ConnectionId server_;
	ja2::mp::ConnectionId pendingAccepted_;
	std::array<InboundMessage,
		MaximumFullEngineCoopClientInboundMessages> inbound_{};
	std::thread::id mainThread_;
	std::size_t inboundHead_ = 0;
	std::size_t inboundCount_ = 0;
	std::size_t maximumQueuedInboundMessages_ = 0;
	std::size_t maximumPendingWriteBytes_ = 0;
	unsigned pollDepth_ = 0;
	unsigned handlerDepth_ = 0;
	unsigned pendingDrainMilliseconds_ = 0;
	FullEngineCoopClientTransportFailure lastFailure_ =
		FullEngineCoopClientTransportFailure::None;
	bool running_ = false;
	bool connected_ = false;
	bool closePending_ = false;
	bool notifyClientOnClose_ = false;
	bool preserveClientStateOnClose_ = false;
};
}

#endif
