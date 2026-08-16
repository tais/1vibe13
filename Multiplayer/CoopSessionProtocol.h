#ifndef MULTIPLAYER_COOP_SESSION_PROTOCOL_H
#define MULTIPLAYER_COOP_SESSION_PROTOCOL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace CoopSession
{
constexpr std::uint16_t CurrentProtocolVersion = 1;
constexpr std::size_t AdmissionRequestWireSize = 116;
constexpr std::size_t AdmissionResponseWireSize = 68;

struct RuntimeCompatibilityFingerprint
{
	std::uint32_t schema = 0;
	std::uint64_t high = 0;
	std::uint64_t low = 0;
};

using ContentManifestSha256 = std::array<std::uint8_t, 32>;
using PeerIdentity = std::array<std::uint8_t, 16>;
using ReconnectToken = std::array<std::uint8_t, 32>;

struct AdmissionRequest
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	RuntimeCompatibilityFingerprint runtimeFingerprint;
	ContentManifestSha256 contentManifestSha256{};
	PeerIdentity peerIdentity{};
	ReconnectToken reconnectToken{};
};

enum class AdmissionRejectReason : std::uint16_t
{
	None = 0,
	MalformedRequest = 1,
	AuthorityDisabled = 2,
	ConfigurationIncomplete = 3,
	UnsupportedProtocol = 4,
	SessionEpochMismatch = 5,
	RuntimeCompatibilityMismatch = 6,
	ContentManifestMismatch = 7,
	InvalidTransport = 8,
	InvalidPeerBinding = 9,
	UnknownPeer = 10,
	InvalidReconnectToken = 11,
	TransportAlreadyBound = 12,
	CapacityReached = 13,
	TokenSourceUnavailable = 14,
	TokenIssuanceFailed = 15
};

struct AdmissionResponse
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	PeerIdentity peerIdentity{};
	ReconnectToken reconnectToken{};
	AdmissionRejectReason rejectReason = AdmissionRejectReason::MalformedRequest;

	bool admitted() const noexcept { return rejectReason == AdmissionRejectReason::None; }
};

using AdmissionRequestBytes = std::array<std::uint8_t, AdmissionRequestWireSize>;
using AdmissionResponseBytes = std::array<std::uint8_t, AdmissionResponseWireSize>;

enum class DecodeResult
{
	Ok,
	WrongSize,
	WrongMagic,
	WrongMessageKind,
	NonZeroReserved,
	UnsupportedProtocol,
	InvalidRejectReason,
	InvalidSemanticValue
};

bool EncodeAdmissionRequest(
	const AdmissionRequest& request, AdmissionRequestBytes& bytes) noexcept;
DecodeResult DecodeAdmissionRequest(
	const std::uint8_t* bytes, std::size_t size, AdmissionRequest& request) noexcept;

bool EncodeAdmissionResponse(
	const AdmissionResponse& response, AdmissionResponseBytes& bytes) noexcept;
DecodeResult DecodeAdmissionResponse(
	const std::uint8_t* bytes, std::size_t size, AdmissionResponse& response) noexcept;

bool IsKnownAdmissionRejectReason(AdmissionRejectReason reason) noexcept;
bool IsZero(const PeerIdentity& identity) noexcept;
bool IsZero(const ReconnectToken& token) noexcept;

// RuntimeCompatibilityFingerprint::hex() is exactly 40 hexadecimal characters:
// eight for schema, then sixteen each for high and low. This parser intentionally
// does not claim that the runtime fingerprint covers arbitrary installed Data.
bool ParseRuntimeCompatibilityFingerprintHex(
	const std::string& text, RuntimeCompatibilityFingerprint& fingerprint) noexcept;
bool ParseContentManifestSha256Hex(
	const std::string& text, ContentManifestSha256& hash) noexcept;
bool ParsePeerIdentityHex(
	const std::string& text, PeerIdentity& identity) noexcept;
bool ParseReconnectTokenHex(
	const std::string& text, ReconnectToken& token) noexcept;

bool operator==(const RuntimeCompatibilityFingerprint& left,
	const RuntimeCompatibilityFingerprint& right) noexcept;
bool operator!=(const RuntimeCompatibilityFingerprint& left,
	const RuntimeCompatibilityFingerprint& right) noexcept;
}

#endif
