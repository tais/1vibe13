#include "FullEngineCoopAdmissionListener.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <new>

namespace CoopSession
{
namespace
{
CoopServerHello HelloFor(
	const AuthorityConfiguration& configuration) noexcept
{
	CoopServerHello hello;
	hello.protocolVersion = CurrentProtocolVersion;
	hello.sessionEpoch = configuration.sessionEpoch;
	hello.runtimeFingerprint = configuration.runtimeFingerprint;
	hello.contentManifestSha256 = configuration.contentManifestSha256;
	return hello;
}

bool BootstrapMatchesAdmission(
	const CoopCampaignBootstrapDescriptor& bootstrap,
	const AuthorityConfiguration& admission) noexcept
{
	return IsValidCoopCampaignBootstrapDescriptor(bootstrap) &&
		bootstrap.protocolVersion == CurrentProtocolVersion &&
		bootstrap.sessionEpoch == admission.sessionEpoch &&
		bootstrap.runtimeFingerprint == admission.runtimeFingerprint &&
		bootstrap.contentManifestSha256 == admission.contentManifestSha256;
}

AdmissionCredentialAbandon AbandonmentFor(
	const AdmissionRequest& request) noexcept
{
	AdmissionCredentialAbandon abandonment;
	abandonment.protocolVersion = request.protocolVersion;
	abandonment.sessionEpoch = request.sessionEpoch;
	abandonment.runtimeFingerprint = request.runtimeFingerprint;
	abandonment.contentManifestSha256 = request.contentManifestSha256;
	abandonment.peerIdentity = request.peerIdentity;
	abandonment.reconnectToken = request.reconnectToken;
	return abandonment;
}

bool SameAbandonment(const AdmissionCredentialAbandon& left,
	const AdmissionCredentialAbandon& right) noexcept
{
	return left.protocolVersion == right.protocolVersion &&
		left.sessionEpoch == right.sessionEpoch &&
		left.runtimeFingerprint == right.runtimeFingerprint &&
		left.contentManifestSha256 == right.contentManifestSha256 &&
		left.peerIdentity == right.peerIdentity &&
		left.reconnectToken == right.reconnectToken;
}

bool ValidOutboundMessage(
	const char* name, std::size_t size) noexcept
{
	if (name == nullptr) return false;
	if (std::strcmp(name, CoopTacticalIntentReceiptMessageName) == 0)
		return size == CoopTacticalIntentReceiptWireSize;
	if (std::strcmp(name, CoopTacticalBaselineMessageName) == 0)
		return size >= CoopTacticalBaselineHeaderWireSize &&
			size <= MaximumCoopTacticalBaselineWireSize;
	if (std::strcmp(name, CoopTacticalDeltaMessageName) == 0)
		return size >= CoopTacticalDeltaHeaderWireSize &&
			size <= MaximumCoopTacticalDeltaWireSize;
	if (std::strcmp(name, CoopCampaignSyncMetadataMessageName) == 0)
		return size == CoopCampaignSyncMetadataWireSize;
	if (std::strcmp(name, CoopCampaignSyncChunkMessageName) == 0)
		return size >= CoopCampaignSyncChunkHeaderWireSize &&
			size <= MaximumCoopCampaignSyncWireSize;
	if (std::strcmp(name, CoopCampaignSyncCompleteMessageName) == 0)
		return size == CoopCampaignSyncCompleteWireSize;
	if (std::strcmp(name, CoopCampaignSyncRejectMessageName) == 0)
		return size == CoopCampaignSyncRejectWireSize;
	if (std::strcmp(name,
		CoopAdmissionSelfRetirementResultMessageName) == 0)
		return size == AdmissionSelfRetirementResultWireSize;
	return false;
}
}

FullEngineCoopAdmissionListener::FullEngineCoopAdmissionListener(
	FullEngineCoopIngress& ingress) noexcept
	: ingress_(ingress)
{
}

FullEngineCoopAdmissionListener::~FullEngineCoopAdmissionListener()
{
	stop(0);
}

FullEngineCoopAdmissionListenerStartResult
FullEngineCoopAdmissionListener::start(
	const FullEngineCoopAdmissionListenerConfiguration& configuration) noexcept
{
	if (pollDepth_ != 0 || handlerDepth_ != 0)
		return FullEngineCoopAdmissionListenerStartResult::LifecycleBusy;
	stop(0);
	if (!ingress_.admissionActive())
		return FullEngineCoopAdmissionListenerStartResult::AdmissionSessionInactive;
	CoopCampaignBootstrapBytes campaignBootstrapBytes{};
	if (!BootstrapMatchesAdmission(configuration.campaignBootstrap,
			ingress_.admissionConfiguration()) ||
		!EncodeCoopCampaignBootstrap(
			configuration.campaignBootstrap, campaignBootstrapBytes) ||
		configuration.endpoint.port == 0 ||
		configuration.maximumConnections == 0 ||
		configuration.maximumConnections >
			MaximumCoopAdmissionTransportConnections ||
		configuration.timeoutMilliseconds == 0 ||
		configuration.timeoutMilliseconds >
			MaximumCoopAdmissionTimeoutMilliseconds ||
		configuration.handshakeTimeoutMilliseconds == 0 ||
		configuration.handshakeTimeoutMilliseconds >
			MaximumCoopAdmissionHandshakeMilliseconds ||
		configuration.maximumRejectedAdmissionMessages == 0 ||
		configuration.maximumRejectedAdmissionMessages >
			MaximumCoopAdmissionRejectedMessages ||
		configuration.maximumQueuedTacticalMessages == 0 ||
		configuration.maximumQueuedTacticalMessages >
			MaximumCoopTacticalInboundMessages ||
		configuration.maximumQueuedCampaignMessages == 0 ||
		configuration.maximumQueuedCampaignMessages >
			MaximumCoopCampaignInboundMessages ||
		configuration.maximumPendingWriteBytesPerConnection == 0 ||
		configuration.maximumPendingWriteBytesPerConnection >
			MaximumCoopTransportPendingWriteBytes)
		return FullEngineCoopAdmissionListenerStartResult::InvalidConfiguration;

	try
	{
		transport_ = ja2::mp::net::CreateSdlNetPeer();
	}
	catch (const std::bad_alloc&)
	{
		return FullEngineCoopAdmissionListenerStartResult::TransportUnavailable;
	}
	if (transport_ == nullptr)
		return FullEngineCoopAdmissionListenerStartResult::TransportUnavailable;
	if (!transport_->RegisterMessage(CoopAdmissionRequestMessageName,
		&FullEngineCoopAdmissionListener::HandleAdmissionMessage, this) ||
		!transport_->RegisterMessage(CoopAdmissionAckMessageName,
			&FullEngineCoopAdmissionListener::HandleAdmissionAckMessage, this) ||
		!transport_->RegisterMessage(
			CoopAdmissionCredentialAbandonMessageName,
			&FullEngineCoopAdmissionListener::HandleCredentialAbandonMessage,
			this) ||
		!transport_->RegisterMessage(
			CoopAdmissionSelfRetirementRequestMessageName,
			&FullEngineCoopAdmissionListener::HandleSelfRetirementMessage,
			this) ||
		!transport_->RegisterMessage(CoopTacticalIntentMessageName,
			&FullEngineCoopAdmissionListener::HandleTacticalIntentMessage, this) ||
		!transport_->RegisterMessage(CoopTacticalBaselineAckMessageName,
			&FullEngineCoopAdmissionListener::HandleTacticalBaselineAckMessage,
			this) ||
		!transport_->RegisterMessage(CoopTacticalDeltaAckMessageName,
			&FullEngineCoopAdmissionListener::HandleTacticalDeltaAckMessage,
			this) ||
		!transport_->RegisterMessage(CoopTacticalResyncRequestMessageName,
			&FullEngineCoopAdmissionListener::HandleTacticalResyncRequestMessage,
			this) ||
		!transport_->RegisterMessage(CoopCampaignSyncAckMessageName,
			&FullEngineCoopAdmissionListener::HandleCampaignAckMessage, this) ||
		!transport_->RegisterMessage(CoopCampaignSyncResultMessageName,
			&FullEngineCoopAdmissionListener::HandleCampaignResultMessage,
			this) ||
		!transport_->RegisterMessage(CoopCampaignSyncResyncMessageName,
			&FullEngineCoopAdmissionListener::HandleCampaignResyncMessage,
			this))
	{
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		return FullEngineCoopAdmissionListenerStartResult::TransportUnavailable;
	}
	if (!transport_->Start(
		configuration.maximumConnections, configuration.endpoint))
	{
		ja2::mp::net::DestroySdlNetPeer(transport_);
		transport_ = nullptr;
		return FullEngineCoopAdmissionListenerStartResult::TransportStartFailed;
	}
	transport_->SetTimeout(configuration.timeoutMilliseconds);
	handshakeTimeoutMilliseconds_ = configuration.handshakeTimeoutMilliseconds;
	maximumRejectedAdmissionMessages_ =
		configuration.maximumRejectedAdmissionMessages;
	connections_ = {};
	clearInbound();
	maximumQueuedTacticalMessages_ =
		configuration.maximumQueuedTacticalMessages;
	maximumQueuedCampaignMessages_ =
		configuration.maximumQueuedCampaignMessages;
	maximumPendingWriteBytesPerConnection_ =
		configuration.maximumPendingWriteBytesPerConnection;
	campaignBootstrap_ = configuration.campaignBootstrap;
	campaignBootstrapBytes_ = campaignBootstrapBytes;
	stopPending_ = false;
	pendingStopDrainMilliseconds_ = 0;
	running_ = true;
	return FullEngineCoopAdmissionListenerStartResult::Success;
}

void FullEngineCoopAdmissionListener::poll() noexcept
{
	if (!running_ || transport_ == nullptr) return;
	++pollDepth_;
	expireHandshakes();
	while (running_ && transport_ != nullptr)
	{
		ja2::mp::net::SdlNetEvent* event = transport_->Poll();
		if (event == nullptr) break;
		if (running_) handleEvent(*event);
		transport_->Release(event);
	}
	expireHandshakes();
	--pollDepth_;
	if (pollDepth_ == 0 && handlerDepth_ == 0 && stopPending_)
		stopNow(pendingStopDrainMilliseconds_);
}

void FullEngineCoopAdmissionListener::stop(
	unsigned drainMilliseconds) noexcept
{
	running_ = false;
	if (pollDepth_ != 0 || handlerDepth_ != 0)
	{
		stopPending_ = true;
		if (drainMilliseconds > pendingStopDrainMilliseconds_)
			pendingStopDrainMilliseconds_ = drainMilliseconds;
		return;
	}
	stopNow(drainMilliseconds);
}

std::size_t FullEngineCoopAdmissionListener::authenticatedPeerCount()
	const noexcept
{
	std::array<FullEngineCoopAuthenticatedPeer,
		MaximumAuthorityPeers> ignored{};
	return authenticatedPeers(ignored);
}

std::size_t FullEngineCoopAdmissionListener::authenticatedPeers(
	std::array<FullEngineCoopAuthenticatedPeer,
		MaximumAuthorityPeers>& peers) const noexcept
{
	std::array<FullEngineCoopAuthenticatedPeer,
		MaximumAuthorityPeers> resolved{};
	std::size_t count = 0;
	for (const ConnectionAdmissionState& state : connections_)
	{
		if (count == resolved.size()) break;
		PeerIdentity peerIdentity{};
		if (!connectionAuthenticates(state, peerIdentity)) continue;
		resolved[count].peerIdentity = peerIdentity;
		resolved[count].transport = state.transport;
		++count;
	}
	std::sort(resolved.begin(), resolved.begin() + count,
		[](const FullEngineCoopAuthenticatedPeer& left,
			const FullEngineCoopAuthenticatedPeer& right) noexcept {
			if (left.peerIdentity != right.peerIdentity)
				return left.peerIdentity < right.peerIdentity;
			return left.transport < right.transport;
		});
	peers = resolved;
	return count;
}

bool FullEngineCoopAdmissionListener::authenticatedPeerForTransport(
	const TransportPeer& transport,
	PeerIdentity& peerIdentity) const noexcept
{
	for (const ConnectionAdmissionState& state : connections_)
	{
		if (state.transport != transport) continue;
		PeerIdentity resolved{};
		if (!connectionAuthenticates(state, resolved)) return false;
		peerIdentity = resolved;
		return true;
	}
	return false;
}

bool FullEngineCoopAdmissionListener::authenticatedTransportForPeer(
	const PeerIdentity& peerIdentity,
	TransportPeer& transport) const noexcept
{
	if (IsZero(peerIdentity)) return false;
	for (const ConnectionAdmissionState& state : connections_)
	{
		PeerIdentity resolved{};
		if (!connectionAuthenticates(state, resolved) ||
			resolved != peerIdentity)
			continue;
		transport = state.transport;
		return true;
	}
	return false;
}

bool FullEngineCoopAdmissionListener::popInbound(
	FullEngineCoopTacticalInboundMessage& message) noexcept
{
	if (inboundCount_ == 0) return false;
	message = inbound_[inboundHead_];
	inbound_[inboundHead_] = FullEngineCoopTacticalInboundMessage{};
	inboundHead_ = (inboundHead_ + 1) % MaximumCoopTacticalInboundMessages;
	--inboundCount_;
	if (inboundCount_ == 0) inboundHead_ = 0;
	return true;
}

bool FullEngineCoopAdmissionListener::popCampaignInbound(
	FullEngineCoopCampaignInboundMessage& message) noexcept
{
	if (campaignInboundCount_ == 0) return false;
	message = campaignInbound_[campaignInboundHead_];
	campaignInbound_[campaignInboundHead_] =
		FullEngineCoopCampaignInboundMessage{};
	campaignInboundHead_ = (campaignInboundHead_ + 1) %
		MaximumCoopCampaignInboundMessages;
	--campaignInboundCount_;
	if (campaignInboundCount_ == 0) campaignInboundHead_ = 0;
	return true;
}

bool FullEngineCoopAdmissionListener::popSelfRetirement(
	FullEngineCoopSelfRetirementInbound& message) noexcept
{
	if (!selfRetirementInboundOccupied_) return false;
	message = selfRetirementInbound_;
	selfRetirementInbound_ = FullEngineCoopSelfRetirementInbound{};
	selfRetirementInboundOccupied_ = false;
	return true;
}

bool FullEngineCoopAdmissionListener::discardInboundForSelfRetirement()
	noexcept
{
	if (!running_ || !selfRetirementInputFrozen_ ||
		selfRetirementInboundOccupied_)
		return false;
	inbound_ = {};
	inboundHead_ = 0;
	inboundCount_ = 0;
	campaignInbound_ = {};
	campaignInboundHead_ = 0;
	campaignInboundCount_ = 0;
	return true;
}

bool FullEngineCoopAdmissionListener::sendCommittedSelfRetirementResult(
	const FullEngineCoopSelfRetirementInbound& request,
	const AdmissionSelfRetirementResultBytes& bytes) noexcept
{
	if (!running_ || transport_ == nullptr ||
		!selfRetirementInputFrozen_ || !request.transport ||
		!ingress_.credentialRetired(request.peerIdentity))
		return false;
	AdmissionSelfRetirementResult result;
	if (DecodeAdmissionSelfRetirementResult(
		bytes.data(), bytes.size(), result) != DecodeResult::Ok ||
		result.result !=
			AdmissionSelfRetirementResultCode::CredentialRetired ||
		result.sessionEpoch != request.request.sessionEpoch ||
		result.requestId != request.request.requestId ||
		result.peerIdentity != request.peerIdentity)
		return false;
	ConnectionAdmissionState* state = findConnection(request.transport);
	if (state == nullptr || !state->selfRetirementReplyAuthorized ||
		state->selfRetirementRequestId != request.request.requestId ||
		state->selfRetirementPeer != request.peerIdentity)
		return false;
	state->selfRetirementReplyAuthorized = false;
	state->selfRetirementRequestId = 0;
	state->selfRetirementPeer = {};
	std::size_t pending = 0;
	const std::size_t frameBytes = 6 +
		std::strlen(CoopAdmissionSelfRetirementResultMessageName) +
		bytes.size();
	if (!transport_->PendingWriteBytes(request.transport, pending) ||
		pending > maximumPendingWriteBytesPerConnection_ ||
		frameBytes > maximumPendingWriteBytesPerConnection_ - pending)
		return false;
	return transport_->SendMessage(
		CoopAdmissionSelfRetirementResultMessageName,
		bytes.data(), bytes.size(), request.transport, false);
}

bool FullEngineCoopAdmissionListener::sendToPeer(
	const PeerIdentity& peerIdentity,
	const char* messageName,
	const std::uint8_t* bytes,
	std::size_t size) noexcept
{
	if (!running_ || transport_ == nullptr || bytes == nullptr ||
		!ValidOutboundMessage(messageName, size))
		return false;
	TransportPeer recipient;
	if (!authenticatedTransportForPeer(peerIdentity, recipient)) return false;
	std::size_t pending = 0;
	const std::size_t frameBytes = 6 + std::strlen(messageName) + size;
	if (!transport_->PendingWriteBytes(recipient, pending) ||
		pending > maximumPendingWriteBytesPerConnection_ ||
		frameBytes > maximumPendingWriteBytesPerConnection_ - pending)
	{
		closeConnection(recipient, false);
		return false;
	}
	if (transport_->SendMessage(
		messageName, bytes, size, recipient, false)) return true;
	closeConnection(recipient, false);
	return false;
}

void FullEngineCoopAdmissionListener::HandleAdmissionMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleAdmissionMessage(*message);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleAdmissionAckMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleAdmissionAckMessage(*message);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleCredentialAbandonMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleCredentialAbandonMessage(*message);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleSelfRetirementMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleSelfRetirementMessage(*message);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleTacticalIntentMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleTacticalMessage(
		*message, FullEngineCoopTacticalInboundKind::Intent);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleTacticalBaselineAckMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleTacticalMessage(
		*message, FullEngineCoopTacticalInboundKind::BaselineAck);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleTacticalDeltaAckMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleTacticalMessage(
		*message, FullEngineCoopTacticalInboundKind::DeltaAck);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleTacticalResyncRequestMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleTacticalMessage(
		*message, FullEngineCoopTacticalInboundKind::ResyncRequest);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleCampaignAckMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleCampaignMessage(
		*message, FullEngineCoopCampaignInboundKind::Ack);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleCampaignResultMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleCampaignMessage(
		*message, FullEngineCoopCampaignInboundKind::Result);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::HandleCampaignResyncMessage(
	ja2::mp::net::SdlNetMessage* message, void* context)
{
	if (message == nullptr || context == nullptr) return;
	FullEngineCoopAdmissionListener* listener =
		static_cast<FullEngineCoopAdmissionListener*>(context);
	++listener->handlerDepth_;
	listener->handleCampaignMessage(
		*message, FullEngineCoopCampaignInboundKind::Resync);
	listener->finishHandler();
}

void FullEngineCoopAdmissionListener::handleAdmissionMessage(
	ja2::mp::net::SdlNetMessage& message) noexcept
{
	if (!running_ || transport_ == nullptr) return;
	if (selfRetirementInputFrozen_)
	{
		closeConnection(message.sender, true);
		return;
	}
	ConnectionAdmissionState* state = ensureConnection(message.sender);
	if (state == nullptr) return;
	if (!state->helloSent)
	{
		if (!sendHandshakePrelude(message.sender))
		{
			closeConnection(message.sender, false);
			return;
		}
		state = findConnection(message.sender);
		if (state == nullptr) return;
		state->helloSent = true;
	}
	if (handshakeExpired(*state))
	{
		closeConnection(message.sender, true);
		return;
	}
	state->credentialAbandonOffered = false;
	state->credentialAbandonment = {};
	const AdmissionIngressResult result = ingress_.handleAdmission(
		message.sender, message.data, message.size);
	// Admission has already atomically moved a reconnect credential to the new
	// transport. Retire the displaced socket on every subsequent path, including
	// failure to queue the replacement response; otherwise an unauthoritative,
	// already-authenticated old connection could occupy a listener slot forever.
	if (result.displacedTransport)
		closeConnection(result.displacedTransport, true);
	state = findConnection(message.sender);
	if (state == nullptr) return;
	const bool offerCredentialAbandon =
		!state->authenticated && !state->credentialAbandonOfferUsed &&
		result.decodeResult == DecodeResult::Ok &&
		!IsZero(result.request.peerIdentity) &&
		!IsZero(result.request.reconnectToken) &&
		result.response.rejectReason == AdmissionRejectReason::UnknownPeer &&
		result.response.peerIdentity == result.request.peerIdentity;
	if (offerCredentialAbandon)
	{
		state->credentialAbandonOfferUsed = true;
		state->credentialAbandonOffered = true;
		state->credentialAbandonment = AbandonmentFor(result.request);
	}
	if (result.response.admitted())
	{
		PeerIdentity resolved{};
		const std::uint64_t epoch =
			ingress_.admissionConfiguration().sessionEpoch;
		if (ingress_.resolveAuthenticatedPeer(
			message.sender, epoch, resolved) && !IsZero(resolved))
		{
			state->authenticated = true;
			state->authenticatedSessionEpoch = epoch;
			state->resolvedPeer = resolved;
		}
		else
		{
			state->authenticated = false;
			state->authenticatedSessionEpoch = 0;
			state->resolvedPeer = {};
		}
	}
	if (!result.responseReady || !transport_->SendMessage(CoopAdmissionResponseMessageName,
		result.responseBytes.data(), result.responseBytes.size(),
		message.sender, false))
	{
		closeConnection(message.sender, false);
		return;
	}
	if (!result.response.admitted() && !offerCredentialAbandon)
		rejectMessage(message.sender);
}

void FullEngineCoopAdmissionListener::handleAdmissionAckMessage(
	ja2::mp::net::SdlNetMessage& message) noexcept
{
	if (!running_ || transport_ == nullptr) return;
	if (selfRetirementInputFrozen_)
	{
		closeConnection(message.sender, true);
		return;
	}
	ConnectionAdmissionState* state = ensureConnection(message.sender);
	if (state == nullptr) return;
	if (!state->helloSent)
	{
		if (!sendHandshakePrelude(message.sender))
		{
			closeConnection(message.sender, false);
			return;
		}
		state = findConnection(message.sender);
		if (state == nullptr) return;
		state->helloSent = true;
	}
	if (handshakeExpired(*state))
	{
		closeConnection(message.sender, true);
		return;
	}
	state->credentialAbandonOffered = false;
	state->credentialAbandonment = {};
	const AdmissionAckIngressResult result = ingress_.handleAdmissionAck(
		message.sender, message.data, message.size);
	if (result.acknowledged())
	{
		PeerIdentity resolved{};
		const std::uint64_t epoch =
			ingress_.admissionConfiguration().sessionEpoch;
		if (!ingress_.resolveAuthenticatedPeer(
			message.sender, epoch, resolved) || IsZero(resolved))
		{
			closeConnection(message.sender, true);
			return;
		}
		state->authenticated = true;
		state->authenticatedSessionEpoch = epoch;
		state->resolvedPeer = resolved;
	}
	else
		rejectMessage(message.sender);
}

void FullEngineCoopAdmissionListener::handleCredentialAbandonMessage(
	ja2::mp::net::SdlNetMessage& message) noexcept
{
	if (!running_ || transport_ == nullptr) return;
	if (selfRetirementInputFrozen_)
	{
		closeConnection(message.sender, true);
		return;
	}
	ConnectionAdmissionState* state = findConnection(message.sender);
	if (state == nullptr)
	{
		closeConnection(message.sender, true);
		return;
	}
	if (!state->credentialAbandonOffered || state->authenticated)
	{
		rejectMessage(message.sender);
		return;
	}
	if (handshakeExpired(*state))
	{
		closeConnection(message.sender, true);
		return;
	}

	const AdmissionCredentialAbandon expected =
		state->credentialAbandonment;
	state->credentialAbandonOffered = false;
	state->credentialAbandonment = {};
	AdmissionCredentialAbandon decoded;
	if (DecodeAdmissionCredentialAbandon(
		message.data, message.size, decoded) != DecodeResult::Ok ||
		!SameAbandonment(decoded, expected))
	{
		rejectMessage(message.sender);
		return;
	}

	const AdmissionCredentialAbandonIngressResult result =
		ingress_.handleCredentialAbandon(
			message.sender, message.data, message.size);
	if (!result.responseReady || !transport_->SendMessage(
		CoopAdmissionResponseMessageName,
		result.responseBytes.data(), result.responseBytes.size(),
		message.sender, false))
	{
		closeConnection(message.sender, false);
		return;
	}
	if (!result.response.admitted()) rejectMessage(message.sender);
}

void FullEngineCoopAdmissionListener::handleSelfRetirementMessage(
	ja2::mp::net::SdlNetMessage& message) noexcept
{
	if (!running_ || transport_ == nullptr) return;
	ConnectionAdmissionState* state = findConnection(message.sender);
	PeerIdentity resolved{};
	AdmissionSelfRetirementRequest request;
	if (state == nullptr ||
		!connectionAuthenticates(*state, resolved) ||
		DecodeAdmissionSelfRetirementRequest(
			message.data, message.size, request) != DecodeResult::Ok ||
		request.sessionEpoch !=
			ingress_.admissionConfiguration().sessionEpoch)
	{
		closeConnection(message.sender, true);
		return;
	}
	if (selfRetirementInputFrozen_)
	{
		// The first authenticated request is the global close boundary. Exact
		// retransmission is harmless; every other request waits for reconnect.
		return;
	}
	const AdmissionSelfRetirementRegistryBegin begun =
		ingress_.beginSelfRetirement(
			message.sender, request.sessionEpoch, request.requestId);
	if (begun.result == AdmissionSelfRetirementRegistryResult::
		TombstoneCapacityReached)
	{
		AdmissionSelfRetirementResult refusal;
		refusal.sessionEpoch = request.sessionEpoch;
		refusal.requestId = request.requestId;
		refusal.peerIdentity = begun.peerIdentity;
		refusal.result = AdmissionSelfRetirementResultCode::
			TombstoneCapacityReached;
		AdmissionSelfRetirementResultBytes bytes{};
		if (!EncodeAdmissionSelfRetirementResult(refusal, bytes) ||
			!sendToPeer(begun.peerIdentity,
				CoopAdmissionSelfRetirementResultMessageName,
				bytes.data(), bytes.size()))
			closeConnection(message.sender, false);
		return;
	}
	if (!begun || begun.peerIdentity != resolved ||
		selfRetirementInboundOccupied_)
	{
		closeConnection(message.sender, true);
		return;
	}
	FullEngineCoopSelfRetirementInbound captured;
	captured.request = request;
	captured.peerIdentity = resolved;
	captured.transport = message.sender;
	selfRetirementInbound_ = captured;
	selfRetirementInboundOccupied_ = true;
	selfRetirementInputFrozen_ = true;
	state->selfRetirementReplyAuthorized = true;
	state->selfRetirementRequestId = request.requestId;
	state->selfRetirementPeer = resolved;
}

void FullEngineCoopAdmissionListener::handleTacticalMessage(
	ja2::mp::net::SdlNetMessage& message,
	FullEngineCoopTacticalInboundKind kind) noexcept
{
	if (!running_ || transport_ == nullptr) return;
	if (selfRetirementInputFrozen_) return;
	ConnectionAdmissionState* state = findConnection(message.sender);
	PeerIdentity resolved{};
	if (state == nullptr || !connectionAuthenticates(*state, resolved) ||
		!validTacticalInboundSize(kind, message.size) ||
		message.data == nullptr ||
		!queueTacticalMessage(*state, resolved, message, kind))
	{
		// Tactical frames have no valid pre-authentication use. Malformed input
		// and capacity exhaustion close that transport immediately instead of
		// falling back to the admission rejection budget.
		closeConnection(message.sender, true);
	}
}

void FullEngineCoopAdmissionListener::handleCampaignMessage(
	ja2::mp::net::SdlNetMessage& message,
	FullEngineCoopCampaignInboundKind kind) noexcept
{
	if (!running_ || transport_ == nullptr) return;
	if (selfRetirementInputFrozen_) return;
	ConnectionAdmissionState* state = findConnection(message.sender);
	PeerIdentity resolved{};
	if (state == nullptr || !connectionAuthenticates(*state, resolved) ||
		!validCampaignInboundSize(kind, message.size) ||
		message.data == nullptr ||
		!queueCampaignMessage(*state, resolved, message, kind))
	{
		// Campaign control is meaningful only for the ACK-confirmed identity.
		// Callbacks copy bounded bytes; the main-thread campaign coordinator owns
		// decoding, transfer state, storage, and checkpoint loading.
		closeConnection(message.sender, true);
	}
}

void FullEngineCoopAdmissionListener::handleEvent(
	const ja2::mp::net::SdlNetEvent& event) noexcept
{
	if (event.size == 0 || event.data == nullptr) return;
	switch (event.data[0])
	{
		case ja2::mp::net::SDLNET_NEW_INCOMING_CONNECTION:
		{
			if (selfRetirementInputFrozen_)
			{
				closeConnection(event.connection, true);
				break;
			}
			ConnectionAdmissionState* state = ensureConnection(event.connection);
			if (state == nullptr)
			{
				closeConnection(event.connection, false);
				break;
			}
			if (!state->helloSent)
			{
				if (!sendHandshakePrelude(event.connection))
				{
					closeConnection(event.connection, false);
					break;
				}
				state->helloSent = true;
			}
			break;
		}
		case ja2::mp::net::SDLNET_DISCONNECTION_NOTIFICATION:
		case ja2::mp::net::SDLNET_CONNECTION_LOST:
			ingress_.disconnect(event.connection);
			removeConnection(event.connection);
			break;
		default:
			break;
	}
}

bool FullEngineCoopAdmissionListener::sendHandshakePrelude(
	const TransportPeer& recipient) noexcept
{
	if (!ingress_.admissionActive() || transport_ == nullptr ||
		!BootstrapMatchesAdmission(
			campaignBootstrap_, ingress_.admissionConfiguration()))
		return false;
	CoopServerHelloBytes bytes{};
	if (!EncodeCoopServerHello(
		HelloFor(ingress_.admissionConfiguration()), bytes))
		return false;
	if (!transport_->SendMessage(CoopServerHelloMessageName,
		bytes.data(), bytes.size(), recipient, false))
		return false;
	return transport_->SendMessage(CoopCampaignBootstrapMessageName,
		campaignBootstrapBytes_.data(), campaignBootstrapBytes_.size(),
		recipient, false);
}

FullEngineCoopAdmissionListener::ConnectionAdmissionState*
FullEngineCoopAdmissionListener::ensureConnection(
	const TransportPeer& transport) noexcept
{
	ConnectionAdmissionState* existing = findConnection(transport);
	if (existing != nullptr) return existing;
	for (ConnectionAdmissionState& state : connections_)
	{
		if (state.transport) continue;
		state.transport = transport;
		state.handshakeDeadlineMilliseconds =
			static_cast<std::uint64_t>(SDL_GetTicks()) +
			handshakeTimeoutMilliseconds_;
		state.remainingRejections = maximumRejectedAdmissionMessages_;
		return &state;
	}
	return nullptr;
}

FullEngineCoopAdmissionListener::ConnectionAdmissionState*
FullEngineCoopAdmissionListener::findConnection(
	const TransportPeer& transport) noexcept
{
	for (ConnectionAdmissionState& state : connections_)
		if (state.transport == transport) return &state;
	return nullptr;
}

bool FullEngineCoopAdmissionListener::connectionAuthenticates(
	const ConnectionAdmissionState& state,
	PeerIdentity& peerIdentity) const noexcept
{
	if (!running_ || transport_ == nullptr || !state.transport ||
		!state.authenticated || state.authenticatedSessionEpoch == 0 ||
		IsZero(state.resolvedPeer) ||
		state.authenticatedSessionEpoch !=
			ingress_.admissionConfiguration().sessionEpoch)
		return false;
	PeerIdentity resolved{};
	if (!ingress_.resolveAuthenticatedPeer(state.transport,
		state.authenticatedSessionEpoch, resolved) ||
		resolved != state.resolvedPeer)
		return false;
	peerIdentity = resolved;
	return true;
}

bool FullEngineCoopAdmissionListener::validTacticalInboundSize(
	FullEngineCoopTacticalInboundKind kind,
	std::size_t size) const noexcept
{
	switch (kind)
	{
		case FullEngineCoopTacticalInboundKind::Intent:
			return size >= TacticalIntentHeaderWireSize &&
				size <= MaximumTacticalIntentWireSize;
		case FullEngineCoopTacticalInboundKind::BaselineAck:
			return size == CoopTacticalBaselineAckWireSize;
		case FullEngineCoopTacticalInboundKind::DeltaAck:
			return size == CoopTacticalDeltaAckWireSize;
		case FullEngineCoopTacticalInboundKind::ResyncRequest:
			return size == CoopTacticalResyncRequestWireSize;
	}
	return false;
}

bool FullEngineCoopAdmissionListener::queueTacticalMessage(
	const ConnectionAdmissionState& state,
	const PeerIdentity& peerIdentity,
	const ja2::mp::net::SdlNetMessage& message,
	FullEngineCoopTacticalInboundKind kind) noexcept
{
	if (maximumQueuedTacticalMessages_ == 0 ||
		inboundCount_ >= maximumQueuedTacticalMessages_ ||
		message.size > MaximumCoopTacticalInboundWireSize)
		return false;
	const std::size_t tail = (inboundHead_ + inboundCount_) %
		MaximumCoopTacticalInboundMessages;
	FullEngineCoopTacticalInboundMessage queued;
	queued.kind = kind;
	queued.peerIdentity = peerIdentity;
	queued.transport = state.transport;
	queued.size = message.size;
	std::copy(message.data, message.data + message.size, queued.bytes.begin());
	inbound_[tail] = queued;
	++inboundCount_;
	return true;
}

bool FullEngineCoopAdmissionListener::validCampaignInboundSize(
	FullEngineCoopCampaignInboundKind kind,
	std::size_t size) const noexcept
{
	switch (kind)
	{
		case FullEngineCoopCampaignInboundKind::Ack:
			return size == CoopCampaignSyncAckWireSize;
		case FullEngineCoopCampaignInboundKind::Result:
			return size == CoopCampaignSyncResultWireSize;
		case FullEngineCoopCampaignInboundKind::Resync:
			return size == CoopCampaignSyncResyncWireSize;
	}
	return false;
}

bool FullEngineCoopAdmissionListener::queueCampaignMessage(
	const ConnectionAdmissionState& state,
	const PeerIdentity& peerIdentity,
	const ja2::mp::net::SdlNetMessage& message,
	FullEngineCoopCampaignInboundKind kind) noexcept
{
	if (maximumQueuedCampaignMessages_ == 0 ||
		campaignInboundCount_ >= maximumQueuedCampaignMessages_ ||
		message.size > MaximumCoopCampaignInboundWireSize)
		return false;
	const std::size_t tail =
		(campaignInboundHead_ + campaignInboundCount_) %
		MaximumCoopCampaignInboundMessages;
	FullEngineCoopCampaignInboundMessage queued;
	queued.kind = kind;
	queued.peerIdentity = peerIdentity;
	queued.transport = state.transport;
	queued.size = message.size;
	std::copy(message.data, message.data + message.size, queued.bytes.begin());
	campaignInbound_[tail] = queued;
	++campaignInboundCount_;
	return true;
}

void FullEngineCoopAdmissionListener::removeQueuedMessagesForTransport(
	const TransportPeer& transport) noexcept
{
	if (!transport) return;
	if (inboundCount_ != 0)
	{
		std::array<FullEngineCoopTacticalInboundMessage,
			MaximumCoopTacticalInboundMessages> retained{};
		std::size_t retainedCount = 0;
		for (std::size_t offset = 0; offset < inboundCount_; ++offset)
		{
			const std::size_t index = (inboundHead_ + offset) %
				MaximumCoopTacticalInboundMessages;
			if (inbound_[index].transport == transport) continue;
			retained[retainedCount++] = inbound_[index];
		}
		inbound_ = retained;
		inboundHead_ = 0;
		inboundCount_ = retainedCount;
	}
	if (campaignInboundCount_ != 0)
	{
		std::array<FullEngineCoopCampaignInboundMessage,
			MaximumCoopCampaignInboundMessages> retained{};
		std::size_t retainedCount = 0;
		for (std::size_t offset = 0; offset < campaignInboundCount_; ++offset)
		{
			const std::size_t index = (campaignInboundHead_ + offset) %
				MaximumCoopCampaignInboundMessages;
			if (campaignInbound_[index].transport == transport) continue;
			retained[retainedCount++] = campaignInbound_[index];
		}
		campaignInbound_ = retained;
		campaignInboundHead_ = 0;
		campaignInboundCount_ = retainedCount;
	}
}

void FullEngineCoopAdmissionListener::clearInbound() noexcept
{
	inbound_ = {};
	inboundHead_ = 0;
	inboundCount_ = 0;
	campaignInbound_ = {};
	campaignInboundHead_ = 0;
	campaignInboundCount_ = 0;
	selfRetirementInbound_ = FullEngineCoopSelfRetirementInbound{};
	selfRetirementInboundOccupied_ = false;
	selfRetirementInputFrozen_ = false;
}

bool FullEngineCoopAdmissionListener::handshakeExpired(
	const ConnectionAdmissionState& state) const noexcept
{
	return !state.authenticated &&
		static_cast<std::uint64_t>(SDL_GetTicks()) >=
			state.handshakeDeadlineMilliseconds;
}

void FullEngineCoopAdmissionListener::expireHandshakes() noexcept
{
	const std::uint64_t now = static_cast<std::uint64_t>(SDL_GetTicks());
	for (ConnectionAdmissionState& state : connections_)
	{
		if (!state.transport || state.authenticated ||
			now < state.handshakeDeadlineMilliseconds)
			continue;
		const TransportPeer expired = state.transport;
		closeConnection(expired, true);
	}
}

void FullEngineCoopAdmissionListener::rejectMessage(
	const TransportPeer& transport) noexcept
{
	ConnectionAdmissionState* state = findConnection(transport);
	if (state == nullptr) return;
	if (state->remainingRejections > 1)
	{
		--state->remainingRejections;
		return;
	}
	closeConnection(transport, true);
}

void FullEngineCoopAdmissionListener::closeConnection(
	const TransportPeer& transport, bool notifyPeer) noexcept
{
	ingress_.disconnect(transport);
	removeConnection(transport);
	if (transport_ != nullptr)
		transport_->CloseConnection(transport, notifyPeer);
}

void FullEngineCoopAdmissionListener::removeConnection(
	const TransportPeer& transport) noexcept
{
	removeQueuedMessagesForTransport(transport);
	ConnectionAdmissionState* state = findConnection(transport);
	if (state != nullptr) *state = ConnectionAdmissionState{};
}

void FullEngineCoopAdmissionListener::stopNow(
	unsigned drainMilliseconds) noexcept
{
	stopPending_ = false;
	pendingStopDrainMilliseconds_ = 0;
	connections_ = {};
	clearInbound();
	campaignBootstrap_ = {};
	campaignBootstrapBytes_ = {};
	maximumQueuedTacticalMessages_ = 0;
	maximumQueuedCampaignMessages_ = 0;
	if (transport_ == nullptr) return;
	ingress_.clearTransportBindings();
	transport_->Shutdown(drainMilliseconds);
	ja2::mp::net::DestroySdlNetPeer(transport_);
	transport_ = nullptr;
}

void FullEngineCoopAdmissionListener::finishHandler() noexcept
{
	if (handlerDepth_ != 0) --handlerDepth_;
	// A handler always runs inside SdlNetPeer::Poll. Destruction therefore waits
	// for the outer listener poll to return from the transport pump.
}
}
