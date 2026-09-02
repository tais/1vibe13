#ifndef MULTIPLAYER_FULL_ENGINE_COOP_ADMISSION_LISTENER_H
#define MULTIPLAYER_FULL_ENGINE_COOP_ADMISSION_LISTENER_H

#include "CoopCampaignBootstrapProtocol.h"
#include "CoopCampaignSyncProtocol.h"
#include "CoopHandshakeProtocol.h"
#include "CoopTacticalIntent.h"
#include "CoopTacticalProtocol.h"
#include "FullEngineCoopIngress.h"
#include "SdlNetTransport.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
constexpr std::uint16_t MaximumCoopAdmissionTransportConnections = 16;
constexpr unsigned MaximumCoopAdmissionTimeoutMilliseconds = 600000;
constexpr unsigned MaximumCoopAdmissionHandshakeMilliseconds = 60000;
constexpr unsigned MaximumCoopAdmissionRejectedMessages = 16;
constexpr std::size_t MaximumCoopTransportPendingWriteBytes = 4u * 1024u * 1024u;
constexpr std::size_t DefaultCoopTransportPendingWriteBytes = 256u * 1024u;
constexpr std::size_t MaximumCoopTacticalInboundMessages = 64;
constexpr std::size_t MaximumCoopCampaignInboundMessages = 64;
constexpr std::size_t MaximumCoopTacticalInboundWireSize =
	CoopTacticalBaselineAckWireSize > MaximumTacticalIntentWireSize
		? CoopTacticalBaselineAckWireSize
		: MaximumTacticalIntentWireSize;
static_assert(CoopTacticalDeltaAckWireSize <=
	MaximumCoopTacticalInboundWireSize,
	"the bounded listener inbox must hold every tactical ACK");
static_assert(CoopTacticalResyncRequestWireSize <=
	MaximumCoopTacticalInboundWireSize,
	"the bounded listener inbox must hold a tactical resync request");
constexpr std::size_t MaximumCoopCampaignInboundWireSize =
	CoopCampaignSyncAckWireSize;
static_assert(CoopCampaignSyncResultWireSize <=
	MaximumCoopCampaignInboundWireSize,
	"the bounded campaign inbox must hold a sync result");
static_assert(CoopCampaignSyncResyncWireSize <=
	MaximumCoopCampaignInboundWireSize,
	"the bounded campaign inbox must hold a resync request");

enum class FullEngineCoopTacticalInboundKind : std::uint8_t
{
	Intent = 1,
	BaselineAck = 2,
	DeltaAck = 3,
	ResyncRequest = 4
};

// A transport callback copies only bounded wire bytes and attaches the
// server-resolved sender. Decoding and all tactical state transitions happen
// later, outside the SDL callback.
struct FullEngineCoopTacticalInboundMessage
{
	FullEngineCoopTacticalInboundKind kind =
		FullEngineCoopTacticalInboundKind::Intent;
	PeerIdentity peerIdentity{};
	TransportPeer transport;
	std::size_t size = 0;
	std::array<std::uint8_t, MaximumCoopTacticalInboundWireSize> bytes{};
};

enum class FullEngineCoopCampaignInboundKind : std::uint8_t
{
	Ack = 1,
	Result = 2,
	Resync = 3
};

// Campaign-control callbacks use the same transport-authenticated identity as
// tactical callbacks but copy into a disjoint FIFO. The tactical server remains
// the sole consumer of its existing inbox.
struct FullEngineCoopCampaignInboundMessage
{
	FullEngineCoopCampaignInboundKind kind =
		FullEngineCoopCampaignInboundKind::Ack;
	PeerIdentity peerIdentity{};
	TransportPeer transport;
	std::size_t size = 0;
	std::array<std::uint8_t, MaximumCoopCampaignInboundWireSize> bytes{};
};

struct FullEngineCoopAuthenticatedPeer
{
	PeerIdentity peerIdentity{};
	TransportPeer transport;
};

// Captured only after the request's transport has resolved to an
// ACK-authenticated identity. The wire request itself contains no identity.
struct FullEngineCoopSelfRetirementInbound
{
	AdmissionSelfRetirementRequest request;
	PeerIdentity peerIdentity{};
	TransportPeer transport;
};

struct FullEngineCoopAdmissionListenerConfiguration
{
	ja2::mp::net::SdlNetEndpoint endpoint;
	CoopCampaignBootstrapDescriptor campaignBootstrap;
	std::uint16_t maximumConnections = MaximumAuthorityPeers * 2;
	unsigned timeoutMilliseconds = 120000;
	unsigned handshakeTimeoutMilliseconds = 10000;
	unsigned maximumRejectedAdmissionMessages = 3;
	std::size_t maximumQueuedTacticalMessages =
		MaximumCoopTacticalInboundMessages;
	std::size_t maximumQueuedCampaignMessages =
		MaximumCoopCampaignInboundMessages;
	std::size_t maximumPendingWriteBytesPerConnection =
		DefaultCoopTransportPendingWriteBytes;
};

enum class FullEngineCoopAdmissionListenerStartResult
{
	Success,
	AdmissionSessionInactive,
	InvalidConfiguration,
	TransportUnavailable,
	TransportStartFailed,
	LifecycleBusy
};

// Owns a transport dedicated to the new co-op admission, tactical, and campaign
// wire namespaces. Every connection receives the retained hello/bootstrap pair
// before admission can respond. The credential-abandon handler is a one-shot
// handshake recovery gate; tactical and campaign callbacks only authenticate
// and copy into disjoint bounded queues and never decode gameplay, mutate JA2,
// or import legacy arena state.
class FullEngineCoopAdmissionListener
{
public:
	explicit FullEngineCoopAdmissionListener(
		FullEngineCoopIngress& ingress) noexcept;
	~FullEngineCoopAdmissionListener();
	FullEngineCoopAdmissionListener(
		const FullEngineCoopAdmissionListener&) = delete;
	FullEngineCoopAdmissionListener& operator=(
		const FullEngineCoopAdmissionListener&) = delete;

	FullEngineCoopAdmissionListenerStartResult start(
		const FullEngineCoopAdmissionListenerConfiguration& configuration) noexcept;
	void poll() noexcept;
	void stop(unsigned drainMilliseconds = 0) noexcept;
	bool running() const noexcept { return running_; }

	std::size_t authenticatedPeerCount() const noexcept;
	std::size_t authenticatedPeers(
		std::array<FullEngineCoopAuthenticatedPeer,
			MaximumAuthorityPeers>& peers) const noexcept;
	bool authenticatedPeerForTransport(
		const TransportPeer& transport,
		PeerIdentity& peerIdentity) const noexcept;
	bool authenticatedTransportForPeer(
		const PeerIdentity& peerIdentity,
		TransportPeer& transport) const noexcept;

	std::size_t pendingInboundCount() const noexcept
	{
		return inboundCount_;
	}
	// Read-only FIFO head inspection lets the main-thread coordinator apply
	// execution readiness only to intents while still accepting a head ACK.
	// It never scans past or reorders a blocked intent.
	bool peekInboundKind(
		FullEngineCoopTacticalInboundKind& kind) const noexcept
	{
		if (inboundCount_ == 0) return false;
		kind = inbound_[inboundHead_].kind;
		return true;
	}
	bool popInbound(
		FullEngineCoopTacticalInboundMessage& message) noexcept;
	std::size_t pendingCampaignInboundCount() const noexcept
	{
		return campaignInboundCount_;
	}
	bool popCampaignInbound(
		FullEngineCoopCampaignInboundMessage& message) noexcept;
	bool selfRetirementInputFrozen() const noexcept
	{
		return selfRetirementInputFrozen_;
	}
	bool popSelfRetirement(
		FullEngineCoopSelfRetirementInbound& message) noexcept;
	// Once the authenticated request has been captured, all unconsumed raw
	// tactical/campaign bytes are unauthoritative and restartable. This explicit
	// gate discard never advances a command cursor or removes a retained receipt.
	bool discardInboundForSelfRetirement() noexcept;
	// Narrow post-commit reply seam. It targets only the exact transport-local
	// ticket captured for this request and intentionally does not require the
	// now-tombstoned identity to remain admission-authorized.
	bool sendCommittedSelfRetirementResult(
		const FullEngineCoopSelfRetirementInbound& request,
		const AdmissionSelfRetirementResultBytes& bytes) noexcept;
	bool sendToPeer(
		const PeerIdentity& peerIdentity,
		const char* messageName,
		const std::uint8_t* bytes,
		std::size_t size) noexcept;

private:
	struct ConnectionAdmissionState
	{
		TransportPeer transport;
		std::uint64_t handshakeDeadlineMilliseconds = 0;
		unsigned remainingRejections = 0;
		bool helloSent = false;
		bool authenticated = false;
		std::uint64_t authenticatedSessionEpoch = 0;
		PeerIdentity resolvedPeer{};
		bool credentialAbandonOfferUsed = false;
		bool credentialAbandonOffered = false;
		AdmissionCredentialAbandon credentialAbandonment{};
		bool selfRetirementReplyAuthorized = false;
		std::uint64_t selfRetirementRequestId = 0;
		PeerIdentity selfRetirementPeer{};
	};

	static void HandleAdmissionMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleAdmissionAckMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCredentialAbandonMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleSelfRetirementMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleTacticalIntentMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleTacticalBaselineAckMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleTacticalDeltaAckMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleTacticalResyncRequestMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignAckMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignResultMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	static void HandleCampaignResyncMessage(
		ja2::mp::net::SdlNetMessage* message, void* context);
	void handleAdmissionMessage(
		ja2::mp::net::SdlNetMessage& message) noexcept;
	void handleAdmissionAckMessage(
		ja2::mp::net::SdlNetMessage& message) noexcept;
	void handleCredentialAbandonMessage(
		ja2::mp::net::SdlNetMessage& message) noexcept;
	void handleSelfRetirementMessage(
		ja2::mp::net::SdlNetMessage& message) noexcept;
	void handleTacticalMessage(
		ja2::mp::net::SdlNetMessage& message,
		FullEngineCoopTacticalInboundKind kind) noexcept;
	void handleCampaignMessage(
		ja2::mp::net::SdlNetMessage& message,
		FullEngineCoopCampaignInboundKind kind) noexcept;
	void handleEvent(const ja2::mp::net::SdlNetEvent& event) noexcept;
	bool sendHandshakePrelude(const TransportPeer& recipient) noexcept;
	ConnectionAdmissionState* ensureConnection(
		const TransportPeer& transport) noexcept;
	ConnectionAdmissionState* findConnection(
		const TransportPeer& transport) noexcept;
	bool connectionAuthenticates(
		const ConnectionAdmissionState& state,
		PeerIdentity& peerIdentity) const noexcept;
	bool validTacticalInboundSize(
		FullEngineCoopTacticalInboundKind kind,
		std::size_t size) const noexcept;
	bool queueTacticalMessage(
		const ConnectionAdmissionState& state,
		const PeerIdentity& peerIdentity,
		const ja2::mp::net::SdlNetMessage& message,
		FullEngineCoopTacticalInboundKind kind) noexcept;
	bool validCampaignInboundSize(
		FullEngineCoopCampaignInboundKind kind,
		std::size_t size) const noexcept;
	bool queueCampaignMessage(
		const ConnectionAdmissionState& state,
		const PeerIdentity& peerIdentity,
		const ja2::mp::net::SdlNetMessage& message,
		FullEngineCoopCampaignInboundKind kind) noexcept;
	void removeQueuedMessagesForTransport(
		const TransportPeer& transport) noexcept;
	void clearInbound() noexcept;
	bool handshakeExpired(const ConnectionAdmissionState& state) const noexcept;
	void expireHandshakes() noexcept;
	void rejectMessage(const TransportPeer& transport) noexcept;
	void closeConnection(
		const TransportPeer& transport, bool notifyPeer) noexcept;
	void removeConnection(const TransportPeer& transport) noexcept;
	void stopNow(unsigned drainMilliseconds) noexcept;
	void finishHandler() noexcept;

	FullEngineCoopIngress& ingress_;
	ja2::mp::net::SdlNetPeer* transport_ = nullptr;
	CoopCampaignBootstrapDescriptor campaignBootstrap_{};
	CoopCampaignBootstrapBytes campaignBootstrapBytes_{};
	std::array<ConnectionAdmissionState,
		MaximumCoopAdmissionTransportConnections> connections_{};
	std::array<FullEngineCoopTacticalInboundMessage,
		MaximumCoopTacticalInboundMessages> inbound_{};
	std::array<FullEngineCoopCampaignInboundMessage,
		MaximumCoopCampaignInboundMessages> campaignInbound_{};
	FullEngineCoopSelfRetirementInbound selfRetirementInbound_;
	std::size_t inboundHead_ = 0;
	std::size_t inboundCount_ = 0;
	std::size_t campaignInboundHead_ = 0;
	std::size_t campaignInboundCount_ = 0;
	std::size_t maximumQueuedTacticalMessages_ = 0;
	std::size_t maximumQueuedCampaignMessages_ = 0;
	std::size_t maximumPendingWriteBytesPerConnection_ = 0;
	unsigned handshakeTimeoutMilliseconds_ = 0;
	unsigned maximumRejectedAdmissionMessages_ = 0;
	unsigned pollDepth_ = 0;
	unsigned handlerDepth_ = 0;
	unsigned pendingStopDrainMilliseconds_ = 0;
	bool stopPending_ = false;
	bool running_ = false;
	bool selfRetirementInboundOccupied_ = false;
	bool selfRetirementInputFrozen_ = false;
};
}

#endif
