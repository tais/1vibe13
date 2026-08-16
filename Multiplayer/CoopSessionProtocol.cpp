#include "CoopSessionProtocol.h"

#include <algorithm>

namespace CoopSession
{
namespace
{
constexpr std::uint8_t WireMagic[4] = {'J', '2', 'C', 'A'};
constexpr std::uint8_t RequestMessageKind = 1;
constexpr std::uint8_t ResponseMessageKind = 2;

void WriteU16(std::uint8_t*& output, std::uint16_t value)
{
	*output++ = static_cast<std::uint8_t>(value);
	*output++ = static_cast<std::uint8_t>(value >> 8);
}

void WriteU32(std::uint8_t*& output, std::uint32_t value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		*output++ = static_cast<std::uint8_t>(value >> shift);
}

void WriteU64(std::uint8_t*& output, std::uint64_t value)
{
	for (unsigned shift = 0; shift < 64; shift += 8)
		*output++ = static_cast<std::uint8_t>(value >> shift);
}

std::uint16_t ReadU16(const std::uint8_t*& input)
{
	const std::uint16_t value = static_cast<std::uint16_t>(input[0]) |
		(static_cast<std::uint16_t>(input[1]) << 8);
	input += 2;
	return value;
}

std::uint32_t ReadU32(const std::uint8_t*& input)
{
	std::uint32_t value = 0;
	for (unsigned shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(*input++) << shift;
	return value;
}

std::uint64_t ReadU64(const std::uint8_t*& input)
{
	std::uint64_t value = 0;
	for (unsigned shift = 0; shift < 64; shift += 8)
		value |= static_cast<std::uint64_t>(*input++) << shift;
	return value;
}

template <std::size_t Size>
void WriteBytes(std::uint8_t*& output, const std::array<std::uint8_t, Size>& value)
{
	std::copy(value.begin(), value.end(), output);
	output += Size;
}

template <std::size_t Size>
void ReadBytes(const std::uint8_t*& input, std::array<std::uint8_t, Size>& value)
{
	std::copy(input, input + Size, value.begin());
	input += Size;
}

bool HasMagic(const std::uint8_t* bytes)
{
	return std::equal(WireMagic, WireMagic + 4, bytes);
}

int HexDigit(char value)
{
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

template <std::size_t Size>
bool ParseHex(
	const std::string& text, std::array<std::uint8_t, Size>& output) noexcept
{
	if (text.size() != Size * 2) return false;
	std::array<std::uint8_t, Size> parsed{};
	for (std::size_t index = 0; index < Size; ++index)
	{
		const int high = HexDigit(text[index * 2]);
		const int low = HexDigit(text[index * 2 + 1]);
		if (high < 0 || low < 0) return false;
		parsed[index] = static_cast<std::uint8_t>((high << 4) | low);
	}
	output = parsed;
	return true;
}

std::uint32_t ReadHexU32(const std::uint8_t* bytes)
{
	std::uint32_t value = 0;
	for (std::size_t index = 0; index < 4; ++index)
		value = (value << 8) | bytes[index];
	return value;
}

std::uint64_t ReadHexU64(const std::uint8_t* bytes)
{
	std::uint64_t value = 0;
	for (std::size_t index = 0; index < 8; ++index)
		value = (value << 8) | bytes[index];
	return value;
}
}

bool EncodeAdmissionRequest(
	const AdmissionRequest& request, AdmissionRequestBytes& bytes) noexcept
{
	if (request.protocolVersion != CurrentProtocolVersion ||
		request.sessionEpoch == 0 || request.runtimeFingerprint.schema == 0 ||
		IsZero(request.contentManifestSha256) ||
		IsZero(request.peerIdentity) != IsZero(request.reconnectToken))
		return false;
	AdmissionRequestBytes encoded{};
	std::uint8_t* output = encoded.data();
	std::copy(WireMagic, WireMagic + 4, output);
	output += 4;
	WriteU16(output, request.protocolVersion);
	*output++ = RequestMessageKind;
	*output++ = 0;
	WriteU64(output, request.sessionEpoch);
	WriteU32(output, request.runtimeFingerprint.schema);
	WriteU64(output, request.runtimeFingerprint.high);
	WriteU64(output, request.runtimeFingerprint.low);
	WriteBytes(output, request.contentManifestSha256);
	WriteBytes(output, request.peerIdentity);
	WriteBytes(output, request.reconnectToken);
	bytes = encoded;
	return true;
}

DecodeResult DecodeAdmissionRequest(
	const std::uint8_t* bytes, std::size_t size, AdmissionRequest& request) noexcept
{
	if (size != AdmissionRequestWireSize || bytes == nullptr) return DecodeResult::WrongSize;
	if (!HasMagic(bytes)) return DecodeResult::WrongMagic;
	if (bytes[6] != RequestMessageKind) return DecodeResult::WrongMessageKind;
	if (bytes[7] != 0) return DecodeResult::NonZeroReserved;

	AdmissionRequest decoded;
	const std::uint8_t* input = bytes + 4;
	decoded.protocolVersion = ReadU16(input);
	input += 2;
	decoded.sessionEpoch = ReadU64(input);
	decoded.runtimeFingerprint.schema = ReadU32(input);
	decoded.runtimeFingerprint.high = ReadU64(input);
	decoded.runtimeFingerprint.low = ReadU64(input);
	ReadBytes(input, decoded.contentManifestSha256);
	ReadBytes(input, decoded.peerIdentity);
	ReadBytes(input, decoded.reconnectToken);

	if (decoded.protocolVersion != CurrentProtocolVersion)
		return DecodeResult::UnsupportedProtocol;
	if (decoded.sessionEpoch == 0 || decoded.runtimeFingerprint.schema == 0 ||
		IsZero(decoded.contentManifestSha256))
		return DecodeResult::InvalidSemanticValue;
	if (IsZero(decoded.peerIdentity) != IsZero(decoded.reconnectToken))
		return DecodeResult::InvalidSemanticValue;
	request = decoded;
	return DecodeResult::Ok;
}

bool EncodeAdmissionResponse(
	const AdmissionResponse& response, AdmissionResponseBytes& bytes) noexcept
{
	if (response.protocolVersion != CurrentProtocolVersion ||
		!IsKnownAdmissionRejectReason(response.rejectReason))
		return false;
	if (response.admitted())
	{
		if (response.sessionEpoch == 0 ||
			IsZero(response.peerIdentity) || IsZero(response.reconnectToken))
			return false;
	}
	else if (!IsZero(response.reconnectToken))
	{
		return false;
	}
	AdmissionResponseBytes encoded{};
	std::uint8_t* output = encoded.data();
	std::copy(WireMagic, WireMagic + 4, output);
	output += 4;
	WriteU16(output, response.protocolVersion);
	*output++ = ResponseMessageKind;
	*output++ = 0;
	WriteU64(output, response.sessionEpoch);
	WriteBytes(output, response.peerIdentity);
	WriteBytes(output, response.reconnectToken);
	WriteU16(output, static_cast<std::uint16_t>(response.rejectReason));
	WriteU16(output, 0);
	bytes = encoded;
	return true;
}

DecodeResult DecodeAdmissionResponse(
	const std::uint8_t* bytes, std::size_t size, AdmissionResponse& response) noexcept
{
	if (size != AdmissionResponseWireSize || bytes == nullptr) return DecodeResult::WrongSize;
	if (!HasMagic(bytes)) return DecodeResult::WrongMagic;
	if (bytes[6] != ResponseMessageKind) return DecodeResult::WrongMessageKind;
	if (bytes[7] != 0 || bytes[66] != 0 || bytes[67] != 0)
		return DecodeResult::NonZeroReserved;

	AdmissionResponse decoded;
	const std::uint8_t* input = bytes + 4;
	decoded.protocolVersion = ReadU16(input);
	input += 2;
	decoded.sessionEpoch = ReadU64(input);
	ReadBytes(input, decoded.peerIdentity);
	ReadBytes(input, decoded.reconnectToken);
	decoded.rejectReason = static_cast<AdmissionRejectReason>(ReadU16(input));

	if (decoded.protocolVersion != CurrentProtocolVersion)
		return DecodeResult::UnsupportedProtocol;
	if (!IsKnownAdmissionRejectReason(decoded.rejectReason))
		return DecodeResult::InvalidRejectReason;
	if (decoded.admitted())
	{
		if (decoded.sessionEpoch == 0 ||
			IsZero(decoded.peerIdentity) || IsZero(decoded.reconnectToken))
			return DecodeResult::InvalidSemanticValue;
	}
	else if (!IsZero(decoded.reconnectToken))
	{
		return DecodeResult::InvalidSemanticValue;
	}
	response = decoded;
	return DecodeResult::Ok;
}

bool IsKnownAdmissionRejectReason(AdmissionRejectReason reason) noexcept
{
	switch (reason)
	{
		case AdmissionRejectReason::None:
		case AdmissionRejectReason::MalformedRequest:
		case AdmissionRejectReason::AuthorityDisabled:
		case AdmissionRejectReason::ConfigurationIncomplete:
		case AdmissionRejectReason::UnsupportedProtocol:
		case AdmissionRejectReason::SessionEpochMismatch:
		case AdmissionRejectReason::RuntimeCompatibilityMismatch:
		case AdmissionRejectReason::ContentManifestMismatch:
		case AdmissionRejectReason::InvalidTransport:
		case AdmissionRejectReason::InvalidPeerBinding:
		case AdmissionRejectReason::UnknownPeer:
		case AdmissionRejectReason::InvalidReconnectToken:
		case AdmissionRejectReason::TransportAlreadyBound:
		case AdmissionRejectReason::CapacityReached:
		case AdmissionRejectReason::TokenSourceUnavailable:
		case AdmissionRejectReason::TokenIssuanceFailed:
			return true;
	}
	return false;
}

bool IsZero(const PeerIdentity& identity) noexcept
{
	for (std::uint8_t byte : identity) if (byte != 0) return false;
	return true;
}

bool IsZero(const ReconnectToken& token) noexcept
{
	for (std::uint8_t byte : token) if (byte != 0) return false;
	return true;
}

bool ParseRuntimeCompatibilityFingerprintHex(
	const std::string& text, RuntimeCompatibilityFingerprint& fingerprint) noexcept
{
	std::array<std::uint8_t, 20> bytes{};
	if (!ParseHex(text, bytes)) return false;
	RuntimeCompatibilityFingerprint parsed;
	parsed.schema = ReadHexU32(bytes.data());
	parsed.high = ReadHexU64(bytes.data() + 4);
	parsed.low = ReadHexU64(bytes.data() + 12);
	fingerprint = parsed;
	return true;
}

bool ParseContentManifestSha256Hex(
	const std::string& text, ContentManifestSha256& hash) noexcept
{
	return ParseHex(text, hash);
}

bool ParsePeerIdentityHex(
	const std::string& text, PeerIdentity& identity) noexcept
{
	return ParseHex(text, identity);
}

bool ParseReconnectTokenHex(
	const std::string& text, ReconnectToken& token) noexcept
{
	return ParseHex(text, token);
}

bool operator==(const RuntimeCompatibilityFingerprint& left,
	const RuntimeCompatibilityFingerprint& right) noexcept
{
	return left.schema == right.schema && left.high == right.high && left.low == right.low;
}

bool operator!=(const RuntimeCompatibilityFingerprint& left,
	const RuntimeCompatibilityFingerprint& right) noexcept
{
	return !(left == right);
}
}
