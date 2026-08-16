#include "CoopAdmission.h"

#include <algorithm>

namespace CoopSession
{
namespace
{
constexpr unsigned CredentialIssueAttempts = 16;

bool ConstantTimeTokenEqual(
	const ReconnectToken& left, const ReconnectToken& right) noexcept
{
	std::uint8_t difference = 0;
	for (std::size_t index = 0; index < left.size(); ++index)
		difference |= left[index] ^ right[index];
	return difference == 0;
}

bool ValidTransport(const TransportPeer& transport) noexcept
{
	return static_cast<bool>(transport) && transport != ja2::mp::AnyConnection;
}
}

bool AuthorityConfiguration::complete() const noexcept
{
	return sessionEpoch != 0 &&
		runtimeFingerprintSupplied && runtimeFingerprint.schema != 0 &&
		contentManifestSupplied && !IsZero(contentManifestSha256) &&
		maximumPeers != 0 && maximumPeers <= MaximumAuthorityPeers;
}

AdmissionRegistry::AdmissionRegistry(AdmissionTokenSource* tokenSource) noexcept
	: tokenSource_(tokenSource)
{
}

void AdmissionRegistry::beginSession(
	const AuthorityConfiguration& configuration) noexcept
{
	configuration_ = configuration;
	peers_ = {};
	peerCount_ = 0;
}

AdmissionResponse AdmissionRegistry::admit(
	const TransportPeer& sender, const AdmissionRequest& request) noexcept
{
	if (!configuration_.enabled)
		return reject(request, AdmissionRejectReason::AuthorityDisabled);
	if (!configuration_.complete())
		return reject(request, AdmissionRejectReason::ConfigurationIncomplete);
	if (request.protocolVersion != CurrentProtocolVersion)
		return reject(request, AdmissionRejectReason::UnsupportedProtocol);
	if (!ValidTransport(sender))
		return reject(request, AdmissionRejectReason::InvalidTransport);
	if (request.sessionEpoch != configuration_.sessionEpoch)
		return reject(request, AdmissionRejectReason::SessionEpochMismatch);
	if (request.runtimeFingerprint != configuration_.runtimeFingerprint)
		return reject(request, AdmissionRejectReason::RuntimeCompatibilityMismatch);
	if (request.contentManifestSha256 != configuration_.contentManifestSha256)
		return reject(request, AdmissionRejectReason::ContentManifestMismatch);

	const bool identityIsZero = IsZero(request.peerIdentity);
	const bool tokenIsZero = IsZero(request.reconnectToken);
	if (identityIsZero != tokenIsZero)
		return reject(request, AdmissionRejectReason::InvalidPeerBinding);

	PeerRecord* transportPeer = findBoundPeer(sender);
	if (identityIsZero)
	{
		// A lost acceptance response may cause a first-join retry. The same sender
		// receives its already-issued credential instead of consuming another slot.
		if (transportPeer != nullptr) return accept(*transportPeer);
		if (peerCount_ >= configuration_.maximumPeers)
			return reject(request, AdmissionRejectReason::CapacityReached);
		if (tokenSource_ == nullptr)
			return reject(request, AdmissionRejectReason::TokenSourceUnavailable);

		PeerIdentity identity{};
		ReconnectToken token{};
		if (!issueUniqueCredential(identity, token))
			return reject(request, AdmissionRejectReason::TokenIssuanceFailed);
		PeerRecord peer;
		peer.identity = identity;
		peer.token = token;
		peer.bound = true;
		peer.transport = sender;
		peers_[peerCount_++] = peer;
		return accept(peers_[peerCount_ - 1]);
	}

	PeerRecord* peer = findPeer(request.peerIdentity);
	if (peer == nullptr) return reject(request, AdmissionRejectReason::UnknownPeer);
	if (!ConstantTimeTokenEqual(peer->token, request.reconnectToken))
		return reject(request, AdmissionRejectReason::InvalidReconnectToken);
	if (transportPeer != nullptr && transportPeer != peer)
		return reject(request, AdmissionRejectReason::TransportAlreadyBound);

	// A valid reconnect credential atomically moves the binding. The old sender
	// ceases to authorize intents before this function returns.
	peer->bound = true;
	peer->transport = sender;
	return accept(*peer);
}

bool AdmissionRegistry::resolvePeerForIntent(const TransportPeer& sender,
	std::uint64_t intentEpoch, PeerIdentity& identity) const noexcept
{
	if (!configuration_.enabled || !configuration_.complete() ||
		intentEpoch != configuration_.sessionEpoch || !ValidTransport(sender))
		return false;
	const PeerRecord* peer = findBoundPeer(sender);
	if (peer == nullptr) return false;
	identity = peer->identity;
	return true;
}

bool AdmissionRegistry::authorizesIntent(
	const TransportPeer& sender, std::uint64_t intentEpoch) const noexcept
{
	PeerIdentity ignored{};
	return resolvePeerForIntent(sender, intentEpoch, ignored);
}

void AdmissionRegistry::disconnect(const TransportPeer& sender) noexcept
{
	PeerRecord* peer = findBoundPeer(sender);
	if (peer == nullptr) return;
	peer->bound = false;
	peer->transport = TransportPeer{};
}

void AdmissionRegistry::clearTransportBindings() noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
	{
		PeerRecord& peer = peers_[index];
		peer.bound = false;
		peer.transport = TransportPeer{};
	}
}

std::size_t AdmissionRegistry::boundPeerCount() const noexcept
{
	return static_cast<std::size_t>(std::count_if(
		peers_.begin(), peers_.begin() + peerCount_,
		[](const PeerRecord& peer) { return peer.bound; }));
}

AdmissionRegistry::PeerRecord* AdmissionRegistry::findPeer(
	const PeerIdentity& identity) noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == identity) return &peers_[index];
	return nullptr;
}

const AdmissionRegistry::PeerRecord* AdmissionRegistry::findBoundPeer(
	const TransportPeer& transport) const noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].bound && peers_[index].transport == transport)
			return &peers_[index];
	return nullptr;
}

AdmissionRegistry::PeerRecord* AdmissionRegistry::findBoundPeer(
	const TransportPeer& transport) noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].bound && peers_[index].transport == transport)
			return &peers_[index];
	return nullptr;
}

AdmissionResponse AdmissionRegistry::reject(
	const AdmissionRequest& request, AdmissionRejectReason reason) const noexcept
{
	AdmissionResponse response;
	response.sessionEpoch = configuration_.sessionEpoch;
	response.peerIdentity = request.peerIdentity;
	response.reconnectToken.fill(0);
	response.rejectReason = reason;
	return response;
}

AdmissionResponse AdmissionRegistry::accept(const PeerRecord& peer) const noexcept
{
	AdmissionResponse response;
	response.sessionEpoch = configuration_.sessionEpoch;
	response.peerIdentity = peer.identity;
	response.reconnectToken = peer.token;
	response.rejectReason = AdmissionRejectReason::None;
	return response;
}

bool AdmissionRegistry::issueUniqueCredential(
	PeerIdentity& identity, ReconnectToken& token) noexcept
{
	for (unsigned attempt = 0; attempt < CredentialIssueAttempts; ++attempt)
	{
		PeerIdentity candidateIdentity{};
		ReconnectToken candidateToken{};
		if (!tokenSource_->issue(candidateIdentity, candidateToken)) return false;
		if (IsZero(candidateIdentity) || IsZero(candidateToken)) continue;
		bool duplicate = false;
		for (std::size_t index = 0; index < peerCount_; ++index)
		{
			const PeerRecord& peer = peers_[index];
			if (peer.identity == candidateIdentity ||
				ConstantTimeTokenEqual(peer.token, candidateToken))
			{
				duplicate = true;
				break;
			}
		}
		if (duplicate) continue;
		identity = candidateIdentity;
		token = candidateToken;
		return true;
	}
	return false;
}
}
