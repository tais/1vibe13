#ifndef MULTIPLAYER_COOP_ADMISSION_H
#define MULTIPLAYER_COOP_ADMISSION_H

#include "ConnectionId.h"
#include "CoopSessionProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
constexpr std::size_t MaximumAuthorityPeers = 4;
constexpr std::size_t MaximumRetiredAdmissionCredentials = 64;

// The network adapter supplies this opaque, process-local connection identity.
// It must never deserialize an address or identifier claimed by the peer. A
// binding is scoped to one server-owned session epoch.
using TransportPeer = ja2::mp::ConnectionId;

struct AuthorityConfiguration
{
	bool enabled = false;
	std::uint64_t sessionEpoch = 0;
	bool runtimeFingerprintSupplied = false;
	RuntimeCompatibilityFingerprint runtimeFingerprint;
	bool contentManifestSupplied = false;
	ContentManifestSha256 contentManifestSha256{};
	std::size_t maximumPeers = MaximumAuthorityPeers;

	bool complete() const noexcept;
};

// Production must provide an OS-backed implementation. Values issued here are
// opaque bearer material for reconnect identity binding over the existing
// plaintext transport; they are not transport authentication or encryption.
class AdmissionTokenSource
{
public:
	virtual ~AdmissionTokenSource() = default;
	virtual bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept = 0;
};

struct AdmissionRegistryResult
{
	AdmissionResponse response;
	// Process-local effect only. This opaque connection identity is never
	// serialized into the admission response.
	TransportPeer displacedTransport;

	bool displaced() const noexcept
	{
		return static_cast<bool>(displacedTransport);
	}
};

enum class AdmissionSelfRetirementRegistryResult : std::uint8_t
{
	Success,
	AlreadyPending,
	AlreadyCompleted,
	InvalidContext,
	NotAuthenticated,
	ConflictingRequest,
	TombstoneCapacityReached
};

struct AdmissionSelfRetirementRegistryBegin
{
	AdmissionSelfRetirementRegistryResult result =
		AdmissionSelfRetirementRegistryResult::InvalidContext;
	PeerIdentity peerIdentity{};

	explicit operator bool() const noexcept
	{
		return result == AdmissionSelfRetirementRegistryResult::Success ||
			result ==
				AdmissionSelfRetirementRegistryResult::AlreadyPending;
	}
};

class AdmissionRegistry
{
public:
	explicit AdmissionRegistry(AdmissionTokenSource* tokenSource = nullptr) noexcept;

	// Starts a distinct authoritative session and forgets every prior identity,
	// token, and transport binding. Operators must use a new nonzero epoch when
	// loading or creating a different campaign session.
	void beginSession(const AuthorityConfiguration& configuration) noexcept;
	const AuthorityConfiguration& configuration() const noexcept { return configuration_; }

	// Source-compatible wire-only convenience. It applies the same registry
	// transition but intentionally discards any displaced socket effect; live
	// transports must call admitWithEffects so they can close that old socket.
	AdmissionResponse admit(const TransportPeer& sender,
		const AdmissionRequest& request) noexcept;
	AdmissionRegistryResult admitWithEffects(const TransportPeer& sender,
		const AdmissionRequest& request) noexcept;
	// This explicit reset is safe only after the transport layer has offered a
	// one-shot retry for the exact UnknownPeer reconnect request. It never
	// replaces or displaces a credential that is still known to this registry.
	AdmissionRegistryResult abandonUnknownCredential(
		const TransportPeer& sender,
		const AdmissionCredentialAbandon& abandonment) noexcept;
	AdmissionRejectReason acknowledge(const TransportPeer& sender,
		const AdmissionAck& acknowledgement) noexcept;

	// Reserves one bounded tombstone for the ACK-authenticated sender and closes
	// only that peer's gameplay authority immediately. The request contains no
	// client-selected identity. Completion is a later committed-boundary action;
	// exact repeats are idempotent in both phases.
	AdmissionSelfRetirementRegistryBegin beginSelfRetirement(
		const TransportPeer& sender,
		std::uint64_t sessionEpoch,
		std::uint64_t requestId) noexcept;
	AdmissionSelfRetirementRegistryResult completeSelfRetirement(
		const PeerIdentity& peer,
		std::uint64_t requestId) noexcept;

	// Future intent adapters call this only with a sender-derived TransportPeer
	// and the epoch decoded from the intent envelope. The resolved identity is
	// server-owned; no client-claimed identity participates in authorization.
	bool resolvePeerForIntent(const TransportPeer& sender,
		std::uint64_t intentEpoch, PeerIdentity& identity) const noexcept;
	// Transport/session authentication remains resolvable while retirement is
	// pending so the listener can drain ACKs and target a truthful post-commit
	// result. Gameplay authorization above is already revoked.
	bool resolveAuthenticatedPeer(const TransportPeer& sender,
		std::uint64_t sessionEpoch, PeerIdentity& identity) const noexcept;
	bool authorizesIntent(const TransportPeer& sender,
		std::uint64_t intentEpoch) const noexcept;

	// A transport disconnect removes authority immediately. ACK-confirmed
	// credentials remain reconnectable in this epoch; an unacknowledged seat is
	// reclaimed and can recover only through the explicit abandonment exchange.
	void disconnect(const TransportPeer& sender) noexcept;
	void clearTransportBindings() noexcept;

	std::size_t peerCount() const noexcept { return peerCount_; }
	std::size_t boundPeerCount() const noexcept;
	std::size_t retiredCredentialCount() const noexcept
	{
		return retiredCredentialCount_;
	}
	bool credentialRetired(const PeerIdentity& peer) const noexcept;
	std::size_t pendingSelfRetirementCount() const noexcept;

private:
	struct PeerRecord
	{
		PeerIdentity identity{};
		ReconnectToken token{};
		bool credentialAcknowledged = false;
		bool bindingAcknowledged = false;
		bool bound = false;
		std::uint64_t selfRetirementRequestId = 0;
		TransportPeer transport;
	};

	struct RetiredCredential
	{
		PeerIdentity identity{};
		ReconnectToken token{};
		std::uint64_t requestId = 0;
	};

	PeerRecord* findPeer(const PeerIdentity& identity) noexcept;
	const PeerRecord* findPeer(const PeerIdentity& identity) const noexcept;
	const RetiredCredential* findRetiredCredential(
		const PeerIdentity& identity) const noexcept;
	const PeerRecord* findBoundPeer(const TransportPeer& transport) const noexcept;
	PeerRecord* findBoundPeer(const TransportPeer& transport) noexcept;
	void removePeer(PeerRecord* peer) noexcept;
	AdmissionResponse reject(const AdmissionRequest& request,
		AdmissionRejectReason reason) const noexcept;
	AdmissionResponse accept(const PeerRecord& peer) const noexcept;
	bool issueUniqueCredential(
		PeerIdentity& identity, ReconnectToken& token,
		const PeerIdentity* excludedIdentity = nullptr,
		const ReconnectToken* excludedToken = nullptr) noexcept;

	AuthorityConfiguration configuration_;
	AdmissionTokenSource* tokenSource_ = nullptr;
	std::array<PeerRecord, MaximumAuthorityPeers> peers_{};
	std::size_t peerCount_ = 0;
	std::array<RetiredCredential,
		MaximumRetiredAdmissionCredentials> retiredCredentials_{};
	std::size_t retiredCredentialCount_ = 0;
};
}

#endif
