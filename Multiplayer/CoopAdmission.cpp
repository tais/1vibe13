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
	retiredCredentials_ = {};
	retiredCredentialCount_ = 0;
}

AdmissionResponse AdmissionRegistry::admit(
	const TransportPeer& sender, const AdmissionRequest& request) noexcept
{
	return admitWithEffects(sender, request).response;
}

AdmissionRegistryResult AdmissionRegistry::admitWithEffects(
	const TransportPeer& sender, const AdmissionRequest& request) noexcept
{
	AdmissionRegistryResult result;
	auto rejectResult = [&](AdmissionRejectReason reason) noexcept {
		result.response = reject(request, reason);
		return result;
	};
	if (!configuration_.enabled)
		return rejectResult(AdmissionRejectReason::AuthorityDisabled);
	if (!configuration_.complete())
		return rejectResult(AdmissionRejectReason::ConfigurationIncomplete);
	if (request.protocolVersion != CurrentProtocolVersion)
		return rejectResult(AdmissionRejectReason::UnsupportedProtocol);
	if (!ValidTransport(sender))
		return rejectResult(AdmissionRejectReason::InvalidTransport);
	if (request.sessionEpoch != configuration_.sessionEpoch)
		return rejectResult(AdmissionRejectReason::SessionEpochMismatch);
	if (request.runtimeFingerprint != configuration_.runtimeFingerprint)
		return rejectResult(AdmissionRejectReason::RuntimeCompatibilityMismatch);
	if (request.contentManifestSha256 != configuration_.contentManifestSha256)
		return rejectResult(AdmissionRejectReason::ContentManifestMismatch);

	const bool identityIsZero = IsZero(request.peerIdentity);
	const bool tokenIsZero = IsZero(request.reconnectToken);
	if (identityIsZero != tokenIsZero)
		return rejectResult(AdmissionRejectReason::InvalidPeerBinding);

	PeerRecord* transportPeer = findBoundPeer(sender);
	if (identityIsZero)
	{
		// A lost acceptance response may cause a first-join retry. The same sender
		// receives its already-issued credential instead of consuming another slot.
		if (transportPeer != nullptr)
		{
			result.response = accept(*transportPeer);
			return result;
		}
		if (peerCount_ >= configuration_.maximumPeers)
			return rejectResult(AdmissionRejectReason::CapacityReached);
		if (tokenSource_ == nullptr)
			return rejectResult(AdmissionRejectReason::TokenSourceUnavailable);

		PeerIdentity identity{};
		ReconnectToken token{};
		if (!issueUniqueCredential(identity, token))
			return rejectResult(AdmissionRejectReason::TokenIssuanceFailed);
		PeerRecord peer;
		peer.identity = identity;
		peer.token = token;
		peer.bound = true;
		peer.transport = sender;
		peers_[peerCount_++] = peer;
		result.response = accept(peers_[peerCount_ - 1]);
		return result;
	}

	PeerRecord* peer = findPeer(request.peerIdentity);
	if (peer == nullptr)
	{
		const RetiredCredential* retired =
			findRetiredCredential(request.peerIdentity);
		if (retired == nullptr)
			return rejectResult(AdmissionRejectReason::UnknownPeer);
		if (!ConstantTimeTokenEqual(
			retired->token, request.reconnectToken))
			return rejectResult(
				AdmissionRejectReason::InvalidReconnectToken);
		return rejectResult(AdmissionRejectReason::CredentialRetired);
	}
	if (!ConstantTimeTokenEqual(peer->token, request.reconnectToken))
		return rejectResult(AdmissionRejectReason::InvalidReconnectToken);
	if (peer->selfRetirementRequestId != 0)
		return rejectResult(
			AdmissionRejectReason::CredentialRetirementPending);
	if (transportPeer != nullptr && transportPeer != peer)
		return rejectResult(AdmissionRejectReason::TransportAlreadyBound);

	// A valid reconnect credential atomically moves the binding. The old sender
	// ceases to authorize intents before this function returns.
	if (peer->bound && peer->transport != sender)
		result.displacedTransport = peer->transport;
	const bool sameBinding = peer->bound && peer->transport == sender;
	peer->bound = true;
	peer->transport = sender;
	if (!sameBinding) peer->bindingAcknowledged = false;
	result.response = accept(*peer);
	return result;
}

AdmissionRegistryResult AdmissionRegistry::abandonUnknownCredential(
	const TransportPeer& sender,
	const AdmissionCredentialAbandon& abandonment) noexcept
{
	AdmissionRequest abandoned;
	abandoned.protocolVersion = abandonment.protocolVersion;
	abandoned.sessionEpoch = abandonment.sessionEpoch;
	abandoned.runtimeFingerprint = abandonment.runtimeFingerprint;
	abandoned.contentManifestSha256 = abandonment.contentManifestSha256;
	abandoned.peerIdentity = abandonment.peerIdentity;
	abandoned.reconnectToken = abandonment.reconnectToken;
	auto rejectResult = [&](AdmissionRejectReason reason) noexcept {
		AdmissionRegistryResult result;
		result.response = reject(abandoned, reason);
		return result;
	};

	if (!configuration_.enabled)
		return rejectResult(AdmissionRejectReason::AuthorityDisabled);
	if (!configuration_.complete())
		return rejectResult(AdmissionRejectReason::ConfigurationIncomplete);
	if (abandonment.protocolVersion != CurrentProtocolVersion)
		return rejectResult(AdmissionRejectReason::UnsupportedProtocol);
	if (!ValidTransport(sender))
		return rejectResult(AdmissionRejectReason::InvalidTransport);
	if (abandonment.sessionEpoch != configuration_.sessionEpoch)
		return rejectResult(AdmissionRejectReason::SessionEpochMismatch);
	if (abandonment.runtimeFingerprint != configuration_.runtimeFingerprint)
		return rejectResult(
			AdmissionRejectReason::RuntimeCompatibilityMismatch);
	if (abandonment.contentManifestSha256 !=
		configuration_.contentManifestSha256)
		return rejectResult(AdmissionRejectReason::ContentManifestMismatch);
	if (IsZero(abandonment.peerIdentity) ||
		IsZero(abandonment.reconnectToken))
		return rejectResult(AdmissionRejectReason::InvalidPeerBinding);
	if (findBoundPeer(sender) != nullptr)
		return rejectResult(AdmissionRejectReason::TransportAlreadyBound);
	// Never turn a wrong token for a live identity into a fresh seat. The reset
	// path exists only for a credential already absent from this epoch.
	if (findPeer(abandonment.peerIdentity) != nullptr ||
		findRetiredCredential(abandonment.peerIdentity) != nullptr)
		return rejectResult(AdmissionRejectReason::InvalidPeerBinding);
	if (peerCount_ >= configuration_.maximumPeers)
		return rejectResult(AdmissionRejectReason::CapacityReached);
	if (tokenSource_ == nullptr)
		return rejectResult(AdmissionRejectReason::TokenSourceUnavailable);

	PeerIdentity identity{};
	ReconnectToken token{};
	if (!issueUniqueCredential(identity, token,
		&abandonment.peerIdentity, &abandonment.reconnectToken))
		return rejectResult(AdmissionRejectReason::TokenIssuanceFailed);
	PeerRecord peer;
	peer.identity = identity;
	peer.token = token;
	peer.bound = true;
	peer.transport = sender;
	peers_[peerCount_++] = peer;
	AdmissionRegistryResult result;
	result.response = accept(peers_[peerCount_ - 1]);
	return result;
}

AdmissionRejectReason AdmissionRegistry::acknowledge(
	const TransportPeer& sender, const AdmissionAck& acknowledgement) noexcept
{
	if (!configuration_.enabled)
		return AdmissionRejectReason::AuthorityDisabled;
	if (!configuration_.complete())
		return AdmissionRejectReason::ConfigurationIncomplete;
	if (acknowledgement.protocolVersion != CurrentProtocolVersion)
		return AdmissionRejectReason::UnsupportedProtocol;
	if (!ValidTransport(sender))
		return AdmissionRejectReason::InvalidTransport;
	if (acknowledgement.sessionEpoch != configuration_.sessionEpoch)
		return AdmissionRejectReason::SessionEpochMismatch;
	PeerRecord* peer = findPeer(acknowledgement.peerIdentity);
	if (peer == nullptr) return AdmissionRejectReason::UnknownPeer;
	if (!ConstantTimeTokenEqual(peer->token, acknowledgement.reconnectToken))
		return AdmissionRejectReason::InvalidReconnectToken;
	if (!peer->bound || peer->transport != sender)
		return AdmissionRejectReason::InvalidPeerBinding;
	peer->credentialAcknowledged = true;
	peer->bindingAcknowledged = true;
	return AdmissionRejectReason::None;
}

AdmissionSelfRetirementRegistryBegin
AdmissionRegistry::beginSelfRetirement(
	const TransportPeer& sender,
	std::uint64_t sessionEpoch,
	std::uint64_t requestId) noexcept
{
	AdmissionSelfRetirementRegistryBegin result;
	if (!configuration_.enabled || !configuration_.complete() ||
		sessionEpoch == 0 || sessionEpoch != configuration_.sessionEpoch ||
		requestId == 0 || !ValidTransport(sender))
		return result;
	PeerRecord* peer = findBoundPeer(sender);
	if (peer == nullptr || !peer->bindingAcknowledged)
	{
		result.result =
			AdmissionSelfRetirementRegistryResult::NotAuthenticated;
		return result;
	}
	result.peerIdentity = peer->identity;
	if (peer->selfRetirementRequestId != 0)
	{
		result.result = peer->selfRetirementRequestId == requestId
			? AdmissionSelfRetirementRegistryResult::AlreadyPending
			: AdmissionSelfRetirementRegistryResult::ConflictingRequest;
		return result;
	}
	if (retiredCredentialCount_ + pendingSelfRetirementCount() >=
		retiredCredentials_.size())
	{
		result.result = AdmissionSelfRetirementRegistryResult::
			TombstoneCapacityReached;
		return result;
	}
	peer->selfRetirementRequestId = requestId;
	result.result = AdmissionSelfRetirementRegistryResult::Success;
	return result;
}

AdmissionSelfRetirementRegistryResult
AdmissionRegistry::completeSelfRetirement(
	const PeerIdentity& identity,
	std::uint64_t requestId) noexcept
{
	if (!configuration_.enabled || !configuration_.complete() ||
		IsZero(identity) || requestId == 0)
		return AdmissionSelfRetirementRegistryResult::InvalidContext;
	PeerRecord* peer = findPeer(identity);
	if (peer == nullptr)
	{
		const RetiredCredential* retired = findRetiredCredential(identity);
		return retired != nullptr && retired->requestId == requestId
			? AdmissionSelfRetirementRegistryResult::AlreadyCompleted
			: AdmissionSelfRetirementRegistryResult::NotAuthenticated;
	}
	if (peer->selfRetirementRequestId != requestId)
		return peer->selfRetirementRequestId == 0
			? AdmissionSelfRetirementRegistryResult::NotAuthenticated
			: AdmissionSelfRetirementRegistryResult::ConflictingRequest;
	// beginSelfRetirement reserved this slot atomically. No allocation or
	// fallible operation exists between publishing the tombstone and freeing the
	// active seat.
	if (retiredCredentialCount_ >= retiredCredentials_.size())
		return AdmissionSelfRetirementRegistryResult::
			TombstoneCapacityReached;
	RetiredCredential tombstone;
	tombstone.identity = peer->identity;
	tombstone.token = peer->token;
	tombstone.requestId = requestId;
	retiredCredentials_[retiredCredentialCount_++] = tombstone;
	removePeer(peer);
	return AdmissionSelfRetirementRegistryResult::Success;
}

bool AdmissionRegistry::resolvePeerForIntent(const TransportPeer& sender,
	std::uint64_t intentEpoch, PeerIdentity& identity) const noexcept
{
	PeerIdentity resolved{};
	if (!resolveAuthenticatedPeer(sender, intentEpoch, resolved)) return false;
	const PeerRecord* peer = findPeer(resolved);
	if (peer == nullptr || peer->selfRetirementRequestId != 0) return false;
	identity = resolved;
	return true;
}

bool AdmissionRegistry::resolveAuthenticatedPeer(
	const TransportPeer& sender,
	std::uint64_t sessionEpoch,
	PeerIdentity& identity) const noexcept
{
	if (!configuration_.enabled || !configuration_.complete() ||
		sessionEpoch != configuration_.sessionEpoch || !ValidTransport(sender))
		return false;
	const PeerRecord* peer = findBoundPeer(sender);
	if (peer == nullptr || !peer->bindingAcknowledged) return false;
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
	if (!peer->credentialAcknowledged)
	{
		removePeer(peer);
		return;
	}
	peer->bound = false;
	peer->bindingAcknowledged = false;
	peer->transport = TransportPeer{};
}

void AdmissionRegistry::clearTransportBindings() noexcept
{
	std::size_t index = 0;
	while (index < peerCount_)
	{
		PeerRecord& peer = peers_[index];
		if (!peer.credentialAcknowledged)
		{
			removePeer(&peer);
			continue;
		}
		peer.bound = false;
		peer.bindingAcknowledged = false;
		peer.transport = TransportPeer{};
		++index;
	}
}

std::size_t AdmissionRegistry::boundPeerCount() const noexcept
{
	return static_cast<std::size_t>(std::count_if(
		peers_.begin(), peers_.begin() + peerCount_,
		[](const PeerRecord& peer) { return peer.bound; }));
}

std::size_t AdmissionRegistry::pendingSelfRetirementCount() const noexcept
{
	return static_cast<std::size_t>(std::count_if(
		peers_.begin(), peers_.begin() + peerCount_,
		[](const PeerRecord& peer) {
			return peer.selfRetirementRequestId != 0;
		}));
}

bool AdmissionRegistry::credentialRetired(
	const PeerIdentity& identity) const noexcept
{
	return findRetiredCredential(identity) != nullptr;
}

AdmissionRegistry::PeerRecord* AdmissionRegistry::findPeer(
	const PeerIdentity& identity) noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == identity) return &peers_[index];
	return nullptr;
}

const AdmissionRegistry::PeerRecord* AdmissionRegistry::findPeer(
	const PeerIdentity& identity) const noexcept
{
	for (std::size_t index = 0; index < peerCount_; ++index)
		if (peers_[index].identity == identity) return &peers_[index];
	return nullptr;
}

const AdmissionRegistry::RetiredCredential*
AdmissionRegistry::findRetiredCredential(
	const PeerIdentity& identity) const noexcept
{
	for (std::size_t index = 0; index < retiredCredentialCount_; ++index)
		if (retiredCredentials_[index].identity == identity)
			return &retiredCredentials_[index];
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

void AdmissionRegistry::removePeer(PeerRecord* peer) noexcept
{
	if (peer == nullptr || peer < peers_.data() ||
		peer >= peers_.data() + peerCount_)
		return;
	const std::size_t index = static_cast<std::size_t>(peer - peers_.data());
	for (std::size_t move = index + 1; move < peerCount_; ++move)
		peers_[move - 1] = peers_[move];
	--peerCount_;
	peers_[peerCount_] = PeerRecord{};
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
	PeerIdentity& identity, ReconnectToken& token,
	const PeerIdentity* excludedIdentity,
	const ReconnectToken* excludedToken) noexcept
{
	for (unsigned attempt = 0; attempt < CredentialIssueAttempts; ++attempt)
	{
		PeerIdentity candidateIdentity{};
		ReconnectToken candidateToken{};
		if (!tokenSource_->issue(candidateIdentity, candidateToken)) return false;
		if (IsZero(candidateIdentity) || IsZero(candidateToken)) continue;
		bool duplicate =
			(excludedIdentity != nullptr &&
				candidateIdentity == *excludedIdentity) ||
			(excludedToken != nullptr &&
				ConstantTimeTokenEqual(candidateToken, *excludedToken));
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
		for (std::size_t index = 0;
			!duplicate && index < retiredCredentialCount_; ++index)
		{
			const RetiredCredential& retired = retiredCredentials_[index];
			if (retired.identity == candidateIdentity ||
				ConstantTimeTokenEqual(retired.token, candidateToken))
				duplicate = true;
		}
		if (duplicate) continue;
		identity = candidateIdentity;
		token = candidateToken;
		return true;
	}
	return false;
}
}
