#ifndef MULTIPLAYER_COOP_ADMISSION_H
#define MULTIPLAYER_COOP_ADMISSION_H

#include "CoopSessionProtocol.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace CoopSession
{
constexpr std::size_t MaximumAuthorityPeers = 4;

// The network adapter must construct this value from RPCParameters::sender (or
// its transport's equivalent). It must never deserialize an address supplied by
// the peer. A binding is scoped to one server-owned session epoch.
struct TransportPeer
{
	std::uint32_t binaryAddress = 0;
	std::uint16_t port = 0;
};

bool operator==(
	const TransportPeer& left, const TransportPeer& right) noexcept;
bool operator!=(
	const TransportPeer& left, const TransportPeer& right) noexcept;

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

class AdmissionRegistry
{
public:
	explicit AdmissionRegistry(AdmissionTokenSource* tokenSource = nullptr) noexcept;

	// Starts a distinct authoritative session and forgets every prior identity,
	// token, and transport binding. Operators must use a new nonzero epoch when
	// loading or creating a different campaign session.
	void beginSession(const AuthorityConfiguration& configuration) noexcept;
	const AuthorityConfiguration& configuration() const noexcept { return configuration_; }

	AdmissionResponse admit(const TransportPeer& sender,
		const AdmissionRequest& request) noexcept;

	// Future intent adapters call this only with a sender-derived TransportPeer
	// and the epoch decoded from the intent envelope. The resolved identity is
	// server-owned; no client-claimed identity participates in authorization.
	bool resolvePeerForIntent(const TransportPeer& sender,
		std::uint64_t intentEpoch, PeerIdentity& identity) const noexcept;
	bool authorizesIntent(const TransportPeer& sender,
		std::uint64_t intentEpoch) const noexcept;

	// A transport disconnect removes authority immediately but retains the peer's
	// issued credential for reconnect during the same session epoch.
	void disconnect(const TransportPeer& sender) noexcept;
	void clearTransportBindings() noexcept;

	std::size_t peerCount() const noexcept { return peerCount_; }
	std::size_t boundPeerCount() const noexcept;

private:
	struct PeerRecord
	{
		PeerIdentity identity{};
		ReconnectToken token{};
		bool bound = false;
		TransportPeer transport;
	};

	PeerRecord* findPeer(const PeerIdentity& identity) noexcept;
	const PeerRecord* findBoundPeer(const TransportPeer& transport) const noexcept;
	PeerRecord* findBoundPeer(const TransportPeer& transport) noexcept;
	AdmissionResponse reject(const AdmissionRequest& request,
		AdmissionRejectReason reason) const noexcept;
	AdmissionResponse accept(const PeerRecord& peer) const noexcept;
	bool issueUniqueCredential(
		PeerIdentity& identity, ReconnectToken& token) noexcept;

	AuthorityConfiguration configuration_;
	AdmissionTokenSource* tokenSource_ = nullptr;
	std::array<PeerRecord, MaximumAuthorityPeers> peers_{};
	std::size_t peerCount_ = 0;
};
}

#endif
