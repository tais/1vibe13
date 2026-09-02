#ifndef MULTIPLAYER_COOP_SESSION_PROTOCOL_H
#define MULTIPLAYER_COOP_SESSION_PROTOCOL_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace CoopSession
{
constexpr std::uint16_t CurrentProtocolVersion = 7;
constexpr std::size_t AdmissionRequestWireSize = 116;
constexpr std::size_t AdmissionResponseWireSize = 68;
constexpr std::size_t AdmissionAckWireSize = 64;
constexpr std::size_t AdmissionCredentialAbandonWireSize = 116;
constexpr std::size_t AdmissionSelfRetirementRequestWireSize = 24;
constexpr std::size_t AdmissionSelfRetirementResultWireSize = 48;

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
	TokenIssuanceFailed = 15,
	CredentialRetired = 16,
	CredentialRetirementPending = 17
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

// Confirms that the client received the issued credential on its currently
// bound transport. Until this exact value is acknowledged, a new identity is
// pending and may not authorize gameplay.
struct AdmissionAck
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	PeerIdentity peerIdentity{};
	ReconnectToken reconnectToken{};
};

// Explicitly abandons a reconnect credential that the server has already
// reported UnknownPeer for on this same transport. This is never an implicit
// downgrade from a failed reconnect: the listener grants a one-shot,
// transport-bound retry permit before accepting this exact echo.
struct AdmissionCredentialAbandon
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	RuntimeCompatibilityFingerprint runtimeFingerprint;
	ContentManifestSha256 contentManifestSha256{};
	PeerIdentity peerIdentity{};
	ReconnectToken reconnectToken{};
};

// Voluntary retirement is deliberately self-only. The authenticated transport
// resolves the affected identity; no peer identity or reconnect bearer is
// accepted from this request as a selectable target.
struct AdmissionSelfRetirementRequest
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	std::uint64_t requestId = 0;
};

enum class AdmissionSelfRetirementResultCode : std::uint16_t
{
	CredentialRetired = 1,
	TombstoneCapacityReached = 2
};

// The server supplies the transport-resolved identity only in its result. A
// CredentialRetired result is truthful only after the same-epoch tombstone has
// committed and the active seat has been released.
struct AdmissionSelfRetirementResult
{
	std::uint16_t protocolVersion = CurrentProtocolVersion;
	std::uint64_t sessionEpoch = 0;
	std::uint64_t requestId = 0;
	PeerIdentity peerIdentity{};
	AdmissionSelfRetirementResultCode result =
		AdmissionSelfRetirementResultCode::CredentialRetired;
};

using AdmissionRequestBytes = std::array<std::uint8_t, AdmissionRequestWireSize>;
using AdmissionResponseBytes = std::array<std::uint8_t, AdmissionResponseWireSize>;
using AdmissionAckBytes = std::array<std::uint8_t, AdmissionAckWireSize>;
using AdmissionCredentialAbandonBytes =
	std::array<std::uint8_t, AdmissionCredentialAbandonWireSize>;
using AdmissionSelfRetirementRequestBytes =
	std::array<std::uint8_t, AdmissionSelfRetirementRequestWireSize>;
using AdmissionSelfRetirementResultBytes =
	std::array<std::uint8_t, AdmissionSelfRetirementResultWireSize>;

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

bool EncodeAdmissionAck(
	const AdmissionAck& acknowledgement, AdmissionAckBytes& bytes) noexcept;
DecodeResult DecodeAdmissionAck(
	const std::uint8_t* bytes, std::size_t size,
	AdmissionAck& acknowledgement) noexcept;

bool EncodeAdmissionCredentialAbandon(
	const AdmissionCredentialAbandon& abandonment,
	AdmissionCredentialAbandonBytes& bytes) noexcept;
DecodeResult DecodeAdmissionCredentialAbandon(
	const std::uint8_t* bytes, std::size_t size,
	AdmissionCredentialAbandon& abandonment) noexcept;

bool EncodeAdmissionSelfRetirementRequest(
	const AdmissionSelfRetirementRequest& request,
	AdmissionSelfRetirementRequestBytes& bytes) noexcept;
DecodeResult DecodeAdmissionSelfRetirementRequest(
	const std::uint8_t* bytes, std::size_t size,
	AdmissionSelfRetirementRequest& request) noexcept;

bool EncodeAdmissionSelfRetirementResult(
	const AdmissionSelfRetirementResult& result,
	AdmissionSelfRetirementResultBytes& bytes) noexcept;
DecodeResult DecodeAdmissionSelfRetirementResult(
	const std::uint8_t* bytes, std::size_t size,
	AdmissionSelfRetirementResult& result) noexcept;

bool IsKnownAdmissionRejectReason(AdmissionRejectReason reason) noexcept;
bool IsKnownAdmissionSelfRetirementResultCode(
	AdmissionSelfRetirementResultCode result) noexcept;
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
