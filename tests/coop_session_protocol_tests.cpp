#include "CoopAdmission.h"
#include "CoopSessionProtocol.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); } } while (0)

static_assert(noexcept(std::declval<AdmissionTokenSource&>().issue(
	std::declval<PeerIdentity&>(), std::declval<ReconnectToken&>())));
static_assert(noexcept(std::declval<AdmissionRegistry&>().admit(
	std::declval<const TransportPeer&>(), std::declval<const AdmissionRequest&>())));
static_assert(noexcept(std::declval<const AdmissionRegistry&>().resolvePeerForIntent(
	std::declval<const TransportPeer&>(), std::uint64_t{},
	std::declval<PeerIdentity&>())));
static_assert(noexcept(EncodeAdmissionRequest(
	std::declval<const AdmissionRequest&>(), std::declval<AdmissionRequestBytes&>())));
static_assert(noexcept(EncodeAdmissionResponse(
	std::declval<const AdmissionResponse&>(), std::declval<AdmissionResponseBytes&>())));
static_assert(noexcept(EncodeAdmissionAck(
	std::declval<const AdmissionAck&>(), std::declval<AdmissionAckBytes&>())));
static_assert(noexcept(EncodeAdmissionCredentialAbandon(
	std::declval<const AdmissionCredentialAbandon&>(),
	std::declval<AdmissionCredentialAbandonBytes&>())));
static_assert(noexcept(EncodeAdmissionSelfRetirementRequest(
	std::declval<const AdmissionSelfRetirementRequest&>(),
	std::declval<AdmissionSelfRetirementRequestBytes&>())));
static_assert(noexcept(EncodeAdmissionSelfRetirementResult(
	std::declval<const AdmissionSelfRetirementResult&>(),
	std::declval<AdmissionSelfRetirementResultBytes&>())));

PeerIdentity Identity(std::uint8_t seed)
{
	PeerIdentity value{};
	for (std::size_t index = 0; index < value.size(); ++index)
		value[index] = static_cast<std::uint8_t>(seed + index);
	return value;
}

ReconnectToken Token(std::uint8_t seed)
{
	ReconnectToken value{};
	for (std::size_t index = 0; index < value.size(); ++index)
		value[index] = static_cast<std::uint8_t>(seed + index);
	return value;
}

ContentManifestSha256 ContentHash(std::uint8_t seed)
{
	ContentManifestSha256 value{};
	for (std::size_t index = 0; index < value.size(); ++index)
		value[index] = static_cast<std::uint8_t>(seed + index);
	return value;
}

struct Credential
{
	PeerIdentity identity{};
	ReconnectToken token{};
	bool succeeds = true;
};

class ScriptedTokenSource final : public AdmissionTokenSource
{
public:
	std::vector<Credential> script;
	std::size_t calls = 0;

	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override
	{
		if (calls >= script.size()) return false;
		const Credential& credential = script[calls++];
		if (!credential.succeeds) return false;
		identity = credential.identity;
		token = credential.token;
		return true;
	}
};

Credential MakeCredential(std::uint8_t identitySeed, std::uint8_t tokenSeed)
{
	Credential credential;
	credential.identity = Identity(identitySeed);
	credential.token = Token(tokenSeed);
	return credential;
}

RuntimeCompatibilityFingerprint RuntimeFingerprint()
{
	return RuntimeCompatibilityFingerprint{
		1, UINT64_C(0x1122334455667788), UINT64_C(0x99aabbccddeeff00)};
}

AuthorityConfiguration CompleteConfiguration(std::size_t maximumPeers = MaximumAuthorityPeers)
{
	AuthorityConfiguration configuration;
	configuration.enabled = true;
	configuration.sessionEpoch = UINT64_C(0x0102030405060708);
	configuration.runtimeFingerprintSupplied = true;
	configuration.runtimeFingerprint = RuntimeFingerprint();
	configuration.contentManifestSupplied = true;
	configuration.contentManifestSha256 = ContentHash(0x80);
	configuration.maximumPeers = maximumPeers;
	return configuration;
}

AdmissionRequest FirstJoinRequest(const AuthorityConfiguration& configuration)
{
	AdmissionRequest request;
	request.sessionEpoch = configuration.sessionEpoch;
	request.runtimeFingerprint = configuration.runtimeFingerprint;
	request.contentManifestSha256 = configuration.contentManifestSha256;
	return request;
}

AdmissionRequest ReconnectRequest(
	const AuthorityConfiguration& configuration, const AdmissionResponse& accepted)
{
	AdmissionRequest request = FirstJoinRequest(configuration);
	request.peerIdentity = accepted.peerIdentity;
	request.reconnectToken = accepted.reconnectToken;
	return request;
}

AdmissionAck AckFor(const AuthorityConfiguration& configuration,
	const AdmissionResponse& accepted)
{
	AdmissionAck acknowledgement;
	acknowledgement.sessionEpoch = configuration.sessionEpoch;
	acknowledgement.peerIdentity = accepted.peerIdentity;
	acknowledgement.reconnectToken = accepted.reconnectToken;
	return acknowledgement;
}

AdmissionCredentialAbandon AbandonFor(
	const AuthorityConfiguration& configuration,
	const AdmissionResponse& credential)
{
	AdmissionCredentialAbandon abandonment;
	abandonment.sessionEpoch = configuration.sessionEpoch;
	abandonment.runtimeFingerprint = configuration.runtimeFingerprint;
	abandonment.contentManifestSha256 =
		configuration.contentManifestSha256;
	abandonment.peerIdentity = credential.peerIdentity;
	abandonment.reconnectToken = credential.reconnectToken;
	return abandonment;
}

void CheckReason(
	const AdmissionResponse& response, AdmissionRejectReason expected, const char* message)
{
	CHECK(response.rejectReason == expected, message);
	CHECK(!response.admitted(), "rejection is not admitted");
	CHECK(IsZero(response.reconnectToken), "rejection never echoes reconnect token");
}

void TestRequestCodec()
{
	AdmissionRequest request;
	request.sessionEpoch = UINT64_C(0x0807060504030201);
	request.runtimeFingerprint = RuntimeCompatibilityFingerprint{
		UINT32_C(0x0c0b0a09), UINT64_C(0x14131211100f0e0d),
		UINT64_C(0x1c1b1a1918171615)};
	request.contentManifestSha256 = ContentHash(0x20);
	request.peerIdentity = Identity(0x40);
	request.reconnectToken = Token(0x50);

	AdmissionRequestBytes bytes{};
	CHECK(EncodeAdmissionRequest(request, bytes), "canonical request encodes");
	CHECK(bytes.size() == 116, "request wire size is exactly 116 bytes");
	CHECK(bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'C' && bytes[3] == 'A',
		"request magic is byte exact");
	CHECK(bytes[4] == 7 && bytes[5] == 0 && bytes[6] == 1 && bytes[7] == 0,
		"request version, kind, and reserved bytes are exact");
	for (std::size_t index = 0; index < 8; ++index)
		CHECK(bytes[8 + index] == index + 1, "request epoch is little endian");
	for (std::size_t index = 0; index < 20; ++index)
		CHECK(bytes[16 + index] == index + 9, "runtime fingerprint fields are little endian");
	for (std::size_t index = 0; index < 32; ++index)
		CHECK(bytes[36 + index] == index + 0x20, "content SHA occupies its exact range");
	for (std::size_t index = 0; index < 16; ++index)
		CHECK(bytes[68 + index] == index + 0x40, "peer identity occupies its exact range");
	for (std::size_t index = 0; index < 32; ++index)
		CHECK(bytes[84 + index] == index + 0x50, "reconnect token occupies its exact range");

	AdmissionRequest decoded;
	CHECK(DecodeAdmissionRequest(bytes.data(), bytes.size(), decoded) == DecodeResult::Ok,
		"request golden bytes decode");
	CHECK(decoded.protocolVersion == request.protocolVersion &&
		decoded.sessionEpoch == request.sessionEpoch &&
		decoded.runtimeFingerprint == request.runtimeFingerprint &&
		decoded.contentManifestSha256 == request.contentManifestSha256 &&
		decoded.peerIdentity == request.peerIdentity &&
		decoded.reconnectToken == request.reconnectToken,
		"request round trip preserves every field");

	AdmissionRequest unchanged = request;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		AdmissionRequest output = unchanged;
		CHECK(DecodeAdmissionRequest(bytes.data(), size, output) == DecodeResult::WrongSize,
			"every truncated request length is rejected");
		CHECK(output.sessionEpoch == unchanged.sessionEpoch,
			"failed request decode is transactional");
	}
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CHECK(DecodeAdmissionRequest(extended.data(), extended.size(), decoded) ==
		DecodeResult::WrongSize, "extended request is rejected");
	CHECK(DecodeAdmissionRequest(nullptr, bytes.size(), decoded) ==
		DecodeResult::WrongSize, "null request input is rejected");

	AdmissionRequestBytes malformed = bytes;
	malformed[0] ^= 1;
	CHECK(DecodeAdmissionRequest(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::WrongMagic, "wrong request magic is explicit");
	malformed = bytes;
	malformed[6] = 2;
	CHECK(DecodeAdmissionRequest(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::WrongMessageKind, "wrong request kind is explicit");
	malformed = bytes;
	malformed[7] = 1;
	CHECK(DecodeAdmissionRequest(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::NonZeroReserved, "nonzero request reserved byte is explicit");
	malformed = bytes;
	malformed[4] = static_cast<std::uint8_t>(CurrentProtocolVersion + 1);
	CHECK(DecodeAdmissionRequest(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::UnsupportedProtocol, "unsupported request version is explicit");

	AdmissionRequest halfBound = FirstJoinRequest(CompleteConfiguration());
	halfBound.peerIdentity = Identity(1);
	AdmissionRequestBytes halfBoundBytes = bytes;
	CHECK(!EncodeAdmissionRequest(halfBound, halfBoundBytes),
		"encoder refuses identity without token");
	AdmissionRequest fresh = FirstJoinRequest(CompleteConfiguration());
	CHECK(EncodeAdmissionRequest(fresh, halfBoundBytes), "fresh admission request encodes");
	halfBoundBytes[68] = 1;
	CHECK(DecodeAdmissionRequest(halfBoundBytes.data(), halfBoundBytes.size(), decoded) ==
		DecodeResult::InvalidSemanticValue, "identity without token is rejected by codec");
	halfBound = FirstJoinRequest(CompleteConfiguration());
	halfBound.reconnectToken = Token(1);
	AdmissionRequestBytes halfTokenBytes = bytes;
	CHECK(!EncodeAdmissionRequest(halfBound, halfTokenBytes),
		"encoder refuses token without identity");
	CHECK(EncodeAdmissionRequest(fresh, halfTokenBytes), "second fresh request encodes");
	halfTokenBytes[84] = 1;
	CHECK(DecodeAdmissionRequest(halfTokenBytes.data(), halfTokenBytes.size(), decoded) ==
		DecodeResult::InvalidSemanticValue, "token without identity is rejected by codec");
	AdmissionRequest unsupported = request;
	unsupported.protocolVersion++;
	CHECK(!EncodeAdmissionRequest(unsupported, halfTokenBytes),
		"encoder refuses unsupported request protocol");
	for (unsigned invalidField = 0; invalidField < 3; ++invalidField)
	{
		AdmissionRequest invalid = FirstJoinRequest(CompleteConfiguration());
		if (invalidField == 0) invalid.sessionEpoch = 0;
		if (invalidField == 1) invalid.runtimeFingerprint.schema = 0;
		if (invalidField == 2) invalid.contentManifestSha256.fill(0);
		AdmissionRequestBytes unchangedBytes = bytes;
		CHECK(!EncodeAdmissionRequest(invalid, unchangedBytes),
			"encoder refuses missing authoritative request value");
		CHECK(unchangedBytes == bytes, "failed request encoding is transactional");
		AdmissionRequestBytes invalidBytes{};
		CHECK(EncodeAdmissionRequest(
			FirstJoinRequest(CompleteConfiguration()), invalidBytes),
			"canonical first join encodes for semantic corruption test");
		if (invalidField == 0)
			for (std::size_t offset = 8; offset < 16; ++offset) invalidBytes[offset] = 0;
		if (invalidField == 1)
			for (std::size_t offset = 16; offset < 20; ++offset) invalidBytes[offset] = 0;
		if (invalidField == 2)
			for (std::size_t offset = 36; offset < 68; ++offset) invalidBytes[offset] = 0;
		CHECK(DecodeAdmissionRequest(invalidBytes.data(), invalidBytes.size(), decoded) ==
			DecodeResult::InvalidSemanticValue,
			"decoder refuses missing authoritative request value");
	}
}

void TestResponseCodec()
{
	AdmissionResponse response;
	response.sessionEpoch = UINT64_C(0x0807060504030201);
	response.peerIdentity = Identity(0x20);
	response.reconnectToken = Token(0x40);
	response.rejectReason = AdmissionRejectReason::None;

	AdmissionResponseBytes bytes{};
	CHECK(EncodeAdmissionResponse(response, bytes), "canonical response encodes");
	CHECK(bytes.size() == 68, "response wire size is exactly 68 bytes");
	CHECK(bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'C' && bytes[3] == 'A',
		"response magic is byte exact");
	CHECK(bytes[4] == 7 && bytes[5] == 0 && bytes[6] == 2 && bytes[7] == 0,
		"response version, kind, and reserved bytes are exact");
	for (std::size_t index = 0; index < 8; ++index)
		CHECK(bytes[8 + index] == index + 1, "response epoch is little endian");
	for (std::size_t index = 0; index < 16; ++index)
		CHECK(bytes[16 + index] == index + 0x20, "response identity occupies exact range");
	for (std::size_t index = 0; index < 32; ++index)
		CHECK(bytes[32 + index] == index + 0x40, "response token occupies exact range");
	CHECK(bytes[64] == 0 && bytes[65] == 0 && bytes[66] == 0 && bytes[67] == 0,
		"accepted reason and trailing reserved bytes are exact");

	AdmissionResponse decoded;
	CHECK(DecodeAdmissionResponse(bytes.data(), bytes.size(), decoded) == DecodeResult::Ok,
		"response golden bytes decode");
	CHECK(decoded.admitted() && decoded.sessionEpoch == response.sessionEpoch &&
		decoded.peerIdentity == response.peerIdentity &&
		decoded.reconnectToken == response.reconnectToken,
		"response round trip preserves every field");

	for (std::size_t size = 0; size < bytes.size(); ++size)
		CHECK(DecodeAdmissionResponse(bytes.data(), size, decoded) == DecodeResult::WrongSize,
			"every truncated response length is rejected");
	std::vector<std::uint8_t> extended(bytes.begin(), bytes.end());
	extended.push_back(0);
	CHECK(DecodeAdmissionResponse(extended.data(), extended.size(), decoded) ==
		DecodeResult::WrongSize, "extended response is rejected");
	CHECK(DecodeAdmissionResponse(nullptr, bytes.size(), decoded) ==
		DecodeResult::WrongSize, "null response input is rejected");

	AdmissionResponseBytes malformed = bytes;
	malformed[0] ^= 1;
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::WrongMagic, "wrong response magic is explicit");
	malformed = bytes;
	malformed[6] = 1;
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::WrongMessageKind, "wrong response kind is explicit");
	for (std::size_t reservedOffset : {std::size_t(7), std::size_t(66), std::size_t(67)})
	{
		malformed = bytes;
		malformed[reservedOffset] = 1;
		CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
			DecodeResult::NonZeroReserved, "every response reserved byte is canonical");
	}
	malformed = bytes;
	malformed[4] = static_cast<std::uint8_t>(CurrentProtocolVersion + 1);
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::UnsupportedProtocol, "unsupported response version is explicit");

	for (std::uint32_t raw = 0; raw <= UINT16_MAX; ++raw)
	{
		const bool expectedKnown = raw <=
			static_cast<std::uint16_t>(
				AdmissionRejectReason::CredentialRetirementPending);
		CHECK(IsKnownAdmissionRejectReason(static_cast<AdmissionRejectReason>(raw)) ==
			expectedKnown, "reject reason domain is exhaustive");
	}
	for (std::uint16_t raw = 0;
		raw <= static_cast<std::uint16_t>(
			AdmissionRejectReason::CredentialRetirementPending); ++raw)
	{
		AdmissionResponse enumerated;
		enumerated.sessionEpoch = 7;
		enumerated.peerIdentity = Identity(1);
		enumerated.rejectReason = static_cast<AdmissionRejectReason>(raw);
		if (raw == 0) enumerated.reconnectToken = Token(1);
		AdmissionResponseBytes encoded{};
		CHECK(EncodeAdmissionResponse(enumerated, encoded),
			"every canonical response reason encodes");
		CHECK(DecodeAdmissionResponse(encoded.data(), encoded.size(), decoded) ==
			DecodeResult::Ok, "every defined reject reason round trips");
		CHECK(decoded.rejectReason == enumerated.rejectReason,
			"reject reason numeric value is stable");
	}
	malformed = bytes;
	malformed[64] = 18;
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::InvalidRejectReason, "undefined reject reason is rejected");
	malformed = bytes;
	for (std::size_t index = 32; index < 64; ++index) malformed[index] = 0;
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::InvalidSemanticValue, "acceptance requires a nonzero token");
	AdmissionResponse rejected;
	rejected.peerIdentity = Identity(1);
	rejected.reconnectToken = Token(1);
	rejected.rejectReason = AdmissionRejectReason::CapacityReached;
	CHECK(!EncodeAdmissionResponse(rejected, malformed),
		"encoder refuses a rejection carrying bearer token");
	rejected.reconnectToken.fill(0);
	CHECK(EncodeAdmissionResponse(rejected, malformed), "canonical rejection encodes");
	malformed[32] = 1;
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::InvalidSemanticValue, "rejection cannot carry bearer token");
	AdmissionResponse invalidAccepted;
	invalidAccepted.peerIdentity = Identity(1);
	invalidAccepted.rejectReason = AdmissionRejectReason::None;
	CHECK(!EncodeAdmissionResponse(invalidAccepted, malformed),
		"encoder refuses acceptance without token");
	AdmissionResponse invalidReason;
	invalidReason.rejectReason = static_cast<AdmissionRejectReason>(18);
	CHECK(!EncodeAdmissionResponse(invalidReason, malformed),
		"encoder refuses undefined rejection reason");
	AdmissionResponse invalidVersion;
	invalidVersion.protocolVersion++;
	CHECK(!EncodeAdmissionResponse(invalidVersion, malformed),
		"encoder refuses unsupported response protocol");
	AdmissionResponse zeroEpoch = response;
	zeroEpoch.sessionEpoch = 0;
	AdmissionResponseBytes unchangedBytes = bytes;
	CHECK(!EncodeAdmissionResponse(zeroEpoch, unchangedBytes),
		"encoder refuses accepted response with zero epoch");
	CHECK(unchangedBytes == bytes, "failed response encoding is transactional");
	malformed = bytes;
	for (std::size_t offset = 8; offset < 16; ++offset) malformed[offset] = 0;
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::InvalidSemanticValue,
		"decoder refuses accepted response with zero epoch");
}

void TestAckCodec()
{
	AdmissionAck acknowledgement;
	acknowledgement.sessionEpoch = UINT64_C(0x0807060504030201);
	acknowledgement.peerIdentity = Identity(0x20);
	acknowledgement.reconnectToken = Token(0x40);
	AdmissionAckBytes bytes{};
	CHECK(EncodeAdmissionAck(acknowledgement, bytes),
		"canonical admission ACK encodes");
	CHECK(bytes.size() == 64, "admission ACK wire size is exactly 64 bytes");
	CHECK(bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'C' &&
		bytes[3] == 'A' && bytes[4] == 7 && bytes[5] == 0 &&
		bytes[6] == 3 && bytes[7] == 0,
		"admission ACK magic, version, kind and reserved bytes are exact");
	for (std::size_t index = 0; index < 8; ++index)
		CHECK(bytes[8 + index] == index + 1,
			"admission ACK epoch is little endian");
	for (std::size_t index = 0; index < 16; ++index)
		CHECK(bytes[16 + index] == index + 0x20,
			"admission ACK identity occupies its exact range");
	for (std::size_t index = 0; index < 32; ++index)
		CHECK(bytes[32 + index] == index + 0x40,
			"admission ACK token occupies its exact range");

	AdmissionAck decoded;
	CHECK(DecodeAdmissionAck(bytes.data(), bytes.size(), decoded) ==
		DecodeResult::Ok &&
		decoded.sessionEpoch == acknowledgement.sessionEpoch &&
		decoded.peerIdentity == acknowledgement.peerIdentity &&
		decoded.reconnectToken == acknowledgement.reconnectToken,
		"admission ACK round trips every credential field");
	for (std::size_t size = 0; size < bytes.size(); ++size)
		CHECK(DecodeAdmissionAck(bytes.data(), size, decoded) ==
			DecodeResult::WrongSize,
			"every truncated admission ACK is rejected");
	AdmissionAckBytes malformed = bytes;
	malformed[6] = 1;
	CHECK(DecodeAdmissionAck(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::WrongMessageKind,
		"admission ACK rejects a request message kind");
	malformed = bytes;
	malformed[7] = 1;
	CHECK(DecodeAdmissionAck(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::NonZeroReserved,
		"admission ACK reserved byte is canonical");
	malformed = bytes;
	malformed[4] = static_cast<std::uint8_t>(CurrentProtocolVersion + 1);
	CHECK(DecodeAdmissionAck(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::UnsupportedProtocol,
		"admission ACK protocol mismatch is explicit");
	AdmissionAck invalid = acknowledgement;
	invalid.reconnectToken.fill(0);
	AdmissionAckBytes unchanged = bytes;
	CHECK(!EncodeAdmissionAck(invalid, unchanged) && unchanged == bytes,
		"invalid admission ACK encoding is transactional");
	malformed = bytes;
	for (std::size_t index = 32; index < malformed.size(); ++index)
		malformed[index] = 0;
	CHECK(DecodeAdmissionAck(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::InvalidSemanticValue,
		"admission ACK requires a nonzero bearer token");
}

void TestCredentialAbandonCodec()
{
	const AuthorityConfiguration configuration = CompleteConfiguration();
	AdmissionResponse credential;
	credential.sessionEpoch = configuration.sessionEpoch;
	credential.peerIdentity = Identity(0x20);
	credential.reconnectToken = Token(0x40);
	const AdmissionCredentialAbandon abandonment =
		AbandonFor(configuration, credential);
	AdmissionCredentialAbandonBytes bytes{};
	CHECK(EncodeAdmissionCredentialAbandon(abandonment, bytes),
		"canonical credential abandonment encodes");
	CHECK(bytes.size() == AdmissionCredentialAbandonWireSize &&
		bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'C' &&
		bytes[3] == 'A' && bytes[4] == 7 && bytes[5] == 0 &&
		bytes[6] == 4 && bytes[7] == 0,
		"credential abandonment has exact fixed width, magic, and kind");
	AdmissionCredentialAbandon decoded;
	CHECK(DecodeAdmissionCredentialAbandon(
		bytes.data(), bytes.size(), decoded) == DecodeResult::Ok &&
		decoded.sessionEpoch == abandonment.sessionEpoch &&
		decoded.runtimeFingerprint == abandonment.runtimeFingerprint &&
		decoded.contentManifestSha256 ==
			abandonment.contentManifestSha256 &&
		decoded.peerIdentity == abandonment.peerIdentity &&
		decoded.reconnectToken == abandonment.reconnectToken,
		"credential abandonment round trips every offered field");
	for (std::size_t size = 0; size < bytes.size(); ++size)
		CHECK(DecodeAdmissionCredentialAbandon(
			bytes.data(), size, decoded) == DecodeResult::WrongSize,
			"every truncated credential abandonment is rejected");
	AdmissionCredentialAbandonBytes malformed = bytes;
	malformed[6] = 1;
	CHECK(DecodeAdmissionCredentialAbandon(
		malformed.data(), malformed.size(), decoded) ==
			DecodeResult::WrongMessageKind,
		"credential abandonment cannot alias a first-join request");
	malformed = bytes;
	malformed[7] = 1;
	CHECK(DecodeAdmissionCredentialAbandon(
		malformed.data(), malformed.size(), decoded) ==
			DecodeResult::NonZeroReserved,
		"credential abandonment rejects noncanonical reserved bytes");
	AdmissionCredentialAbandon invalid = abandonment;
	invalid.reconnectToken.fill(0);
	AdmissionCredentialAbandonBytes unchanged = bytes;
	CHECK(!EncodeAdmissionCredentialAbandon(invalid, unchanged) &&
		unchanged == bytes,
		"credential abandonment requires a complete stale credential");
}

void TestSelfRetirementCodecs()
{
	AdmissionSelfRetirementRequest request;
	request.sessionEpoch = UINT64_C(0x0807060504030201);
	request.requestId = UINT64_C(0x1817161514131211);
	AdmissionSelfRetirementRequestBytes requestBytes{};
	CHECK(EncodeAdmissionSelfRetirementRequest(request, requestBytes),
		"self-retirement request encodes");
	CHECK(requestBytes.size() == 24 && requestBytes[0] == 'J' &&
		requestBytes[1] == '2' && requestBytes[2] == 'C' &&
		requestBytes[3] == 'A' && requestBytes[4] == 7 &&
		requestBytes[5] == 0 && requestBytes[6] == 5 &&
		requestBytes[7] == 0,
		"self-retirement request has exact fixed header and no victim field");
	for (std::size_t index = 0; index < 8; ++index)
	{
		CHECK(requestBytes[8 + index] == index + 1,
			"self-retirement epoch is little endian");
		CHECK(requestBytes[16 + index] == index + 0x11,
			"self-retirement request ID is little endian");
	}
	AdmissionSelfRetirementRequest decodedRequest;
	CHECK(DecodeAdmissionSelfRetirementRequest(requestBytes.data(),
		requestBytes.size(), decodedRequest) == DecodeResult::Ok &&
		decodedRequest.sessionEpoch == request.sessionEpoch &&
		decodedRequest.requestId == request.requestId,
		"self-retirement request round trips exactly");
	for (std::size_t size = 0; size < requestBytes.size(); ++size)
		CHECK(DecodeAdmissionSelfRetirementRequest(requestBytes.data(), size,
			decodedRequest) == DecodeResult::WrongSize,
			"every truncated self-retirement request is rejected");
	AdmissionSelfRetirementRequestBytes malformedRequest = requestBytes;
	malformedRequest[6] = 6;
	CHECK(DecodeAdmissionSelfRetirementRequest(malformedRequest.data(),
		malformedRequest.size(), decodedRequest) ==
		DecodeResult::WrongMessageKind,
		"self-retirement request kind cannot alias its result");
	malformedRequest = requestBytes;
	malformedRequest[7] = 1;
	CHECK(DecodeAdmissionSelfRetirementRequest(malformedRequest.data(),
		malformedRequest.size(), decodedRequest) ==
		DecodeResult::NonZeroReserved,
		"self-retirement request reserved byte is canonical");
	malformedRequest = requestBytes;
	malformedRequest[4] = static_cast<std::uint8_t>(
		CurrentProtocolVersion + 1);
	CHECK(DecodeAdmissionSelfRetirementRequest(malformedRequest.data(),
		malformedRequest.size(), decodedRequest) ==
		DecodeResult::UnsupportedProtocol,
		"self-retirement request version mismatch is explicit");
	AdmissionSelfRetirementRequest invalidRequest = request;
	invalidRequest.requestId = 0;
	AdmissionSelfRetirementRequestBytes unchangedRequest = requestBytes;
	CHECK(!EncodeAdmissionSelfRetirementRequest(
		invalidRequest, unchangedRequest) && unchangedRequest == requestBytes,
		"invalid self-retirement request encoding is transactional");

	AdmissionSelfRetirementResult result;
	result.sessionEpoch = request.sessionEpoch;
	result.requestId = request.requestId;
	result.peerIdentity = Identity(0x20);
	result.result = AdmissionSelfRetirementResultCode::CredentialRetired;
	AdmissionSelfRetirementResultBytes resultBytes{};
	CHECK(EncodeAdmissionSelfRetirementResult(result, resultBytes),
		"self-retirement result encodes");
	CHECK(resultBytes.size() == 48 && resultBytes[0] == 'J' &&
		resultBytes[1] == '2' && resultBytes[2] == 'C' &&
		resultBytes[3] == 'A' && resultBytes[4] == 7 &&
		resultBytes[5] == 0 && resultBytes[6] == 6 &&
		resultBytes[7] == 0,
		"self-retirement result has exact fixed header");
	for (std::size_t index = 0; index < 16; ++index)
		CHECK(resultBytes[24 + index] == index + 0x20,
			"server-resolved retirement identity occupies its exact range");
	CHECK(resultBytes[40] == 1 && resultBytes[41] == 0,
		"CredentialRetired has stable little-endian result code");
	for (std::size_t index = 42; index < resultBytes.size(); ++index)
		CHECK(resultBytes[index] == 0,
			"self-retirement result trailing reserved bytes are zero");
	AdmissionSelfRetirementResult decodedResult;
	CHECK(DecodeAdmissionSelfRetirementResult(resultBytes.data(),
		resultBytes.size(), decodedResult) == DecodeResult::Ok &&
		decodedResult.sessionEpoch == result.sessionEpoch &&
		decodedResult.requestId == result.requestId &&
		decodedResult.peerIdentity == result.peerIdentity &&
		decodedResult.result == result.result,
		"self-retirement result round trips every server-owned field");
	for (std::size_t size = 0; size < resultBytes.size(); ++size)
		CHECK(DecodeAdmissionSelfRetirementResult(resultBytes.data(), size,
			decodedResult) == DecodeResult::WrongSize,
			"every truncated self-retirement result is rejected");
	AdmissionSelfRetirementResultBytes malformedResult = resultBytes;
	malformedResult[42] = 1;
	CHECK(DecodeAdmissionSelfRetirementResult(malformedResult.data(),
		malformedResult.size(), decodedResult) ==
		DecodeResult::NonZeroReserved,
		"self-retirement result reserved range is canonical");
	malformedResult = resultBytes;
	malformedResult[40] = 3;
	CHECK(DecodeAdmissionSelfRetirementResult(malformedResult.data(),
		malformedResult.size(), decodedResult) ==
		DecodeResult::InvalidSemanticValue,
		"unknown self-retirement result code is rejected");
	for (std::uint32_t raw = 0; raw <= UINT16_MAX; ++raw)
	{
		const bool known = raw == 1 || raw == 2;
		CHECK(IsKnownAdmissionSelfRetirementResultCode(
			static_cast<AdmissionSelfRetirementResultCode>(raw)) == known,
			"self-retirement result code domain is exhaustive");
	}
	AdmissionSelfRetirementResult refused = result;
	refused.result =
		AdmissionSelfRetirementResultCode::TombstoneCapacityReached;
	CHECK(EncodeAdmissionSelfRetirementResult(refused, resultBytes),
		"bounded tombstone refusal uses the same fixed result shape");
}

void TestHexParsers()
{
	RuntimeCompatibilityFingerprint fingerprint{9, 8, 7};
	CHECK(ParseRuntimeCompatibilityFingerprintHex(
		"00000001112233445566778899AABBCCDDEEFF00", fingerprint),
		"runtime fingerprint hex accepts canonical mixed case");
	CHECK(fingerprint == RuntimeFingerprint(),
		"runtime fingerprint hex matches production schema/high/low ordering");
	const RuntimeCompatibilityFingerprint unchanged = fingerprint;
	CHECK(!ParseRuntimeCompatibilityFingerprintHex("01", fingerprint),
		"short runtime fingerprint hex fails");
	CHECK(fingerprint == unchanged, "failed runtime parser is transactional");
	CHECK(!ParseRuntimeCompatibilityFingerprintHex(
		"00000001112233445566778899aabbccddeeff0z", fingerprint),
		"non-hex runtime fingerprint fails");

	ContentManifestSha256 content{};
	PeerIdentity identity{};
	ReconnectToken token{};
	CHECK(ParseContentManifestSha256Hex(
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
		content), "content manifest SHA parser consumes exactly 32 bytes");
	CHECK(ParsePeerIdentityHex("000102030405060708090a0b0c0d0e0f", identity),
		"peer identity parser consumes exactly 16 bytes");
	CHECK(ParseReconnectTokenHex(
		"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
		token), "reconnect token parser consumes exactly 32 bytes");
	CHECK(!ParseContentManifestSha256Hex(std::string(63, '0'), content) &&
		!ParsePeerIdentityHex(std::string(33, '0'), identity) &&
		!ParseReconnectTokenHex(std::string(65, '0'), token),
		"all fixed hex parsers reject non-exact lengths");
}

void TestFailClosedAdmission()
{
	const AuthorityConfiguration complete = CompleteConfiguration();
	const AdmissionRequest request = FirstJoinRequest(complete);
	const TransportPeer sender{5000};
	ScriptedTokenSource source;
	source.script.push_back(MakeCredential(1, 64));

	AdmissionRegistry registry(&source);
	AuthorityConfiguration disabled = complete;
	disabled.enabled = false;
	registry.beginSession(disabled);
	CheckReason(registry.admit(sender, request), AdmissionRejectReason::AuthorityDisabled,
		"authority defaults closed when disabled");

	std::vector<AuthorityConfiguration> incomplete;
	AuthorityConfiguration item = complete;
	item.sessionEpoch = 0; incomplete.push_back(item);
	item = complete; item.runtimeFingerprintSupplied = false; incomplete.push_back(item);
	item = complete; item.runtimeFingerprint.schema = 0; incomplete.push_back(item);
	item = complete; item.contentManifestSupplied = false; incomplete.push_back(item);
	item = complete; item.contentManifestSha256.fill(0); incomplete.push_back(item);
	item = complete; item.maximumPeers = 0; incomplete.push_back(item);
	item = complete; item.maximumPeers = MaximumAuthorityPeers + 1; incomplete.push_back(item);
	for (const AuthorityConfiguration& configuration : incomplete)
	{
		registry.beginSession(configuration);
		CheckReason(registry.admit(sender, request),
			AdmissionRejectReason::ConfigurationIncomplete,
			"every missing authoritative input fails closed");
		CHECK(!registry.authorizesIntent(sender, configuration.sessionEpoch),
			"incomplete configuration never authorizes intent");
	}

	registry.beginSession(complete);
	AdmissionRequest altered = request;
	altered.protocolVersion++;
	CheckReason(registry.admit(sender, altered), AdmissionRejectReason::UnsupportedProtocol,
		"unsupported protocol is explicit");
	CheckReason(registry.admit(TransportPeer{}, request),
		AdmissionRejectReason::InvalidTransport, "missing connection is rejected");
	CheckReason(registry.admit(TransportPeer{UINT64_MAX}, request),
		AdmissionRejectReason::InvalidTransport, "wildcard connection is rejected");
	altered = request; altered.sessionEpoch++;
	CheckReason(registry.admit(sender, altered), AdmissionRejectReason::SessionEpochMismatch,
		"session epoch mismatch is explicit");
	altered = request; altered.runtimeFingerprint.high ^= 1;
	CheckReason(registry.admit(sender, altered),
		AdmissionRejectReason::RuntimeCompatibilityMismatch,
		"runtime/package graph mismatch is explicit");
	altered = request; altered.contentManifestSha256[0] ^= 1;
	CheckReason(registry.admit(sender, altered), AdmissionRejectReason::ContentManifestMismatch,
		"separate content manifest mismatch is explicit");
	altered = request; altered.peerIdentity = Identity(1);
	CheckReason(registry.admit(sender, altered), AdmissionRejectReason::InvalidPeerBinding,
		"half-present peer binding is rejected");
	altered = request; altered.reconnectToken = Token(1);
	CheckReason(registry.admit(sender, altered), AdmissionRejectReason::InvalidPeerBinding,
		"half-present reconnect token is rejected");

	AdmissionRegistry noSource;
	noSource.beginSession(complete);
	CheckReason(noSource.admit(sender, request), AdmissionRejectReason::TokenSourceUnavailable,
		"first admission fails closed without a token source");

	ScriptedTokenSource failingSource;
	failingSource.script.push_back(Credential{{}, {}, false});
	AdmissionRegistry failing(&failingSource);
	failing.beginSession(complete);
	CheckReason(failing.admit(sender, request), AdmissionRejectReason::TokenIssuanceFailed,
		"token source failure is explicit");
}

void TestAdmissionLifecycleAndIntentGate()
{
	const AuthorityConfiguration configuration = CompleteConfiguration();
	const AdmissionRequest firstJoin = FirstJoinRequest(configuration);
	const TransportPeer firstTransport{5000};
	const TransportPeer secondTransport{5001};
	ScriptedTokenSource source;
	source.script.push_back(MakeCredential(1, 64));
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);

	const AdmissionResponse accepted = registry.admit(firstTransport, firstJoin);
	CHECK(accepted.admitted(), "all-zero identity and token allocate a seat");
	CHECK(!IsZero(accepted.peerIdentity) && !IsZero(accepted.reconnectToken),
		"first admission issues nonzero identity and token");
	CHECK(source.calls == 1 && registry.peerCount() == 1 && registry.boundPeerCount() == 1,
		"first admission consumes exactly one credential and one seat");

	const AdmissionResponse retry = registry.admit(firstTransport, firstJoin);
	CHECK(retry.admitted() && retry.peerIdentity == accepted.peerIdentity &&
		retry.reconnectToken == accepted.reconnectToken,
		"same-sender first-join retry is idempotent");
	CHECK(source.calls == 1 && registry.peerCount() == 1,
		"idempotent retry does not issue or consume another seat");

	PeerIdentity resolved = Identity(200);
	CHECK(!registry.resolvePeerForIntent(
		firstTransport, configuration.sessionEpoch, resolved) &&
		!registry.authorizesIntent(firstTransport, configuration.sessionEpoch),
		"pending credential cannot authorize intent before explicit ACK");
	AdmissionAck wrongAck = AckFor(configuration, accepted);
	wrongAck.reconnectToken[0] ^= 1;
	CHECK(registry.acknowledge(firstTransport, wrongAck) ==
		AdmissionRejectReason::InvalidReconnectToken &&
		registry.acknowledge(secondTransport, AckFor(configuration, accepted)) ==
			AdmissionRejectReason::InvalidPeerBinding,
		"ACK validation binds both bearer and current transport");
	CHECK(registry.acknowledge(firstTransport,
		AckFor(configuration, accepted)) == AdmissionRejectReason::None,
		"valid ACK atomically promotes the pending credential");
	CHECK(registry.resolvePeerForIntent(
		firstTransport, configuration.sessionEpoch, resolved) &&
		resolved == accepted.peerIdentity,
		"ACK-promoted intent gate resolves the server-bound identity");
	CHECK(!registry.authorizesIntent(firstTransport, configuration.sessionEpoch + 1) &&
		!registry.authorizesIntent(secondTransport, configuration.sessionEpoch),
		"wrong epoch and unbound sender fail intent authorization");

	AdmissionRequest reconnect = ReconnectRequest(configuration, accepted);
	const AdmissionRegistryResult rebound =
		registry.admitWithEffects(secondTransport, reconnect);
	CHECK(rebound.response.admitted() && rebound.displaced() &&
		rebound.displacedTransport == firstTransport,
		"valid reconnect reports the process-local displaced sender");
	CHECK(!registry.authorizesIntent(firstTransport, configuration.sessionEpoch) &&
		!registry.authorizesIntent(secondTransport, configuration.sessionEpoch),
		"reconnect atomically invalidates old sender but awaits binding ACK");
	CHECK(registry.acknowledge(secondTransport,
		AckFor(configuration, rebound.response)) == AdmissionRejectReason::None &&
		registry.authorizesIntent(secondTransport, configuration.sessionEpoch),
		"reconnect ACK promotes only the replacement transport");

	registry.disconnect(secondTransport);
	CHECK(!registry.authorizesIntent(secondTransport, configuration.sessionEpoch) &&
		registry.peerCount() == 1 && registry.boundPeerCount() == 0,
		"disconnect revokes transport authority but retains reconnect identity");
	const AdmissionResponse wrapperReconnect = registry.admit(firstTransport, reconnect);
	CHECK(wrapperReconnect.admitted() &&
		!registry.authorizesIntent(firstTransport, configuration.sessionEpoch),
		"source-compatible admit wrapper returns wire response and preserves pending binding semantics");
	CHECK(registry.acknowledge(firstTransport,
		AckFor(configuration, wrapperReconnect)) == AdmissionRejectReason::None,
		"wrapper-created reconnect binding is ACK-promoted normally");
	registry.clearTransportBindings();
	CHECK(!registry.authorizesIntent(firstTransport, configuration.sessionEpoch) &&
		registry.peerCount() == 1,
		"clearing bindings revokes transports without erasing credentials");
	const AdmissionResponse afterClear = registry.admit(secondTransport, reconnect);
	CHECK(afterClear.admitted() && registry.acknowledge(secondTransport,
		AckFor(configuration, afterClear)) == AdmissionRejectReason::None,
		"credential remains usable after clearing transport bindings");

	AuthorityConfiguration nextSession = configuration;
	nextSession.sessionEpoch++;
	registry.beginSession(nextSession);
	CHECK(registry.peerCount() == 0 &&
		!registry.authorizesIntent(secondTransport, configuration.sessionEpoch),
		"new session forgets all prior identities, tokens, and bindings");
	AdmissionRequest oldReconnect = reconnect;
	oldReconnect.sessionEpoch = nextSession.sessionEpoch;
	CheckReason(registry.admit(secondTransport, oldReconnect),
		AdmissionRejectReason::UnknownPeer,
		"old credential cannot cross a session boundary");
}

void TestAuthenticatedSelfRetirementLifecycle()
{
	const AuthorityConfiguration configuration = CompleteConfiguration(1);
	const AdmissionRequest firstJoin = FirstJoinRequest(configuration);
	const TransportPeer firstTransport{5101};
	const TransportPeer reconnectTransport{5102};
	const TransportPeer replacementTransport{5103};
	ScriptedTokenSource source;
	source.script.push_back(MakeCredential(1, 64));
	source.script.push_back(MakeCredential(33, 96));
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);
	const AdmissionResponse admitted = registry.admit(firstTransport, firstJoin);
	CHECK(admitted.admitted() && registry.acknowledge(firstTransport,
		AckFor(configuration, admitted)) == AdmissionRejectReason::None,
		"retirement fixture ACK-authenticates one exact transport");

	const AdmissionSelfRetirementRegistryBegin unauthenticated =
		registry.beginSelfRetirement(
			reconnectTransport, configuration.sessionEpoch, 7);
	CHECK(unauthenticated.result ==
		AdmissionSelfRetirementRegistryResult::NotAuthenticated &&
		IsZero(unauthenticated.peerIdentity),
		"an unbound transport cannot select or retire another identity");
	const AdmissionSelfRetirementRegistryBegin begun =
		registry.beginSelfRetirement(
			firstTransport, configuration.sessionEpoch, 7);
	CHECK(begun.result == AdmissionSelfRetirementRegistryResult::Success &&
		begun.peerIdentity == admitted.peerIdentity &&
		registry.pendingSelfRetirementCount() == 1,
		"server resolves and reserves the authenticated sender's own retirement");
	PeerIdentity resolved{};
	CHECK(!registry.resolvePeerForIntent(firstTransport,
		configuration.sessionEpoch, resolved) &&
		registry.resolveAuthenticatedPeer(firstTransport,
			configuration.sessionEpoch, resolved) &&
		resolved == admitted.peerIdentity,
		"Pending immediately closes gameplay while preserving transport identity");
	CHECK(registry.beginSelfRetirement(firstTransport,
		configuration.sessionEpoch, 7).result ==
		AdmissionSelfRetirementRegistryResult::AlreadyPending &&
		registry.beginSelfRetirement(firstTransport,
			configuration.sessionEpoch, 8).result ==
			AdmissionSelfRetirementRegistryResult::ConflictingRequest,
		"exact pending request is idempotent and conflicting request fails closed");

	registry.disconnect(firstTransport);
	const AdmissionResponse pendingReconnect = registry.admit(
		reconnectTransport, ReconnectRequest(configuration, admitted));
	CheckReason(pendingReconnect,
		AdmissionRejectReason::CredentialRetirementPending,
		"reconnect while Pending never regains gameplay authority");
	CHECK(registry.peerCount() == 1 &&
		registry.pendingSelfRetirementCount() == 1,
		"Pending reconnect neither releases nor duplicates the reserved seat");

	CHECK(registry.completeSelfRetirement(admitted.peerIdentity, 7) ==
		AdmissionSelfRetirementRegistryResult::Success &&
		registry.peerCount() == 0 && registry.retiredCredentialCount() == 1 &&
		registry.pendingSelfRetirementCount() == 0 &&
		registry.credentialRetired(admitted.peerIdentity),
		"tombstone commits before the active seat is released");
	CHECK(registry.completeSelfRetirement(admitted.peerIdentity, 7) ==
		AdmissionSelfRetirementRegistryResult::AlreadyCompleted &&
		registry.retiredCredentialCount() == 1,
		"exact completion is idempotent and never duplicates a tombstone");
	CheckReason(registry.admit(reconnectTransport,
		ReconnectRequest(configuration, admitted)),
		AdmissionRejectReason::CredentialRetired,
		"old bearer deterministically observes committed retirement");
	AdmissionRequest wrongToken = ReconnectRequest(configuration, admitted);
	wrongToken.reconnectToken[0] ^= 1;
	CheckReason(registry.admit(reconnectTransport, wrongToken),
		AdmissionRejectReason::InvalidReconnectToken,
		"tombstone remains bearer-authenticated and reveals nothing to a wrong token");

	const AdmissionResponse replacement =
		registry.admit(replacementTransport, firstJoin);
	CHECK(replacement.admitted() &&
		replacement.peerIdentity != admitted.peerIdentity &&
		registry.peerCount() == 1,
		"a distinct newly launched client can immediately take the freed seat");
	AuthorityConfiguration nextEpoch = configuration;
	++nextEpoch.sessionEpoch;
	registry.beginSession(nextEpoch);
	CHECK(registry.retiredCredentialCount() == 0 &&
		!registry.credentialRetired(admitted.peerIdentity),
		"retirement tombstones are same-epoch state");
}

void TestSelfRetirementTombstoneCapacityPreflight()
{
	const AuthorityConfiguration configuration = CompleteConfiguration(1);
	const AdmissionRequest firstJoin = FirstJoinRequest(configuration);
	ScriptedTokenSource source;
	for (std::size_t index = 0;
		index <= MaximumRetiredAdmissionCredentials; ++index)
	{
		Credential credential;
		credential.identity[0] = static_cast<std::uint8_t>(index + 1);
		credential.identity[1] = static_cast<std::uint8_t>((index + 1) >> 8);
		credential.token[0] = static_cast<std::uint8_t>(index + 17);
		credential.token[1] = static_cast<std::uint8_t>((index + 17) >> 8);
		source.script.push_back(credential);
	}
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);
	for (std::size_t index = 0;
		index < MaximumRetiredAdmissionCredentials; ++index)
	{
		const TransportPeer transport{
			static_cast<std::uint64_t>(6000 + index)};
		const AdmissionResponse admitted = registry.admit(transport, firstJoin);
		CHECK(admitted.admitted() && registry.acknowledge(transport,
			AckFor(configuration, admitted)) == AdmissionRejectReason::None,
			"bounded tombstone fixture admits and ACKs each replacement seat");
		const std::uint64_t requestId = index + 1;
		const AdmissionSelfRetirementRegistryBegin begun =
			registry.beginSelfRetirement(
				transport, configuration.sessionEpoch, requestId);
		CHECK(begun.result ==
			AdmissionSelfRetirementRegistryResult::Success &&
			registry.completeSelfRetirement(
				admitted.peerIdentity, requestId) ==
				AdmissionSelfRetirementRegistryResult::Success,
			"every reserved tombstone through the fixed boundary commits");
	}
	CHECK(registry.retiredCredentialCount() ==
		MaximumRetiredAdmissionCredentials && registry.peerCount() == 0,
		"same-epoch tombstone history reaches its exact bounded capacity");
	const TransportPeer finalTransport{7000};
	const AdmissionResponse finalPeer = registry.admit(finalTransport, firstJoin);
	CHECK(finalPeer.admitted() && registry.acknowledge(finalTransport,
		AckFor(configuration, finalPeer)) == AdmissionRejectReason::None,
		"a live seat may still be admitted at full tombstone capacity");
	const AdmissionSelfRetirementRegistryBegin refused =
		registry.beginSelfRetirement(
			finalTransport, configuration.sessionEpoch, 999);
	CHECK(refused.result == AdmissionSelfRetirementRegistryResult::
		TombstoneCapacityReached &&
		registry.pendingSelfRetirementCount() == 0 &&
		registry.peerCount() == 1 &&
		registry.authorizesIntent(finalTransport, configuration.sessionEpoch),
		"capacity is atomically preflighted before gameplay authority closes");
}

void TestReconnectRejectionsAndCapacity()
{
	const AuthorityConfiguration configuration = CompleteConfiguration(2);
	const AdmissionRequest firstJoin = FirstJoinRequest(configuration);
	const TransportPeer transportA{1001};
	const TransportPeer transportB{1002};
	const TransportPeer transportC{1003};
	ScriptedTokenSource source;
	source.script.push_back(MakeCredential(1, 64));
	source.script.push_back(MakeCredential(17, 96));
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);
	const AdmissionResponse peerA = registry.admit(transportA, firstJoin);
	const AdmissionResponse peerB = registry.admit(transportB, firstJoin);
	CHECK(peerA.admitted() && peerB.admitted(), "capacity seats are admitted");
	CHECK(registry.acknowledge(transportA, AckFor(configuration, peerA)) ==
		AdmissionRejectReason::None &&
		registry.acknowledge(transportB, AckFor(configuration, peerB)) ==
			AdmissionRejectReason::None,
		"capacity peers ACK before receiving intent authority");

	AdmissionRequest unknown = ReconnectRequest(configuration, peerA);
	unknown.peerIdentity = Identity(200);
	CheckReason(registry.admit(transportC, unknown), AdmissionRejectReason::UnknownPeer,
		"unknown reconnect identity is explicit");
	AdmissionRequest wrongToken = ReconnectRequest(configuration, peerA);
	wrongToken.reconnectToken[0] ^= 1;
	CheckReason(registry.admit(transportC, wrongToken),
		AdmissionRejectReason::InvalidReconnectToken,
		"wrong reconnect bearer is explicit");
	CheckReason(registry.admit(transportB, ReconnectRequest(configuration, peerA)),
		AdmissionRejectReason::TransportAlreadyBound,
		"one sender cannot bind a second peer");
	CHECK(registry.authorizesIntent(transportA, configuration.sessionEpoch) &&
		registry.authorizesIntent(transportB, configuration.sessionEpoch),
		"failed conflict leaves both existing bindings intact");
	CheckReason(registry.abandonUnknownCredential(
		transportC, AbandonFor(configuration, peerA)).response,
		AdmissionRejectReason::InvalidPeerBinding,
		"explicit abandonment can never replace a still-known identity");
	AdmissionResponse unknownCredential;
	unknownCredential.sessionEpoch = configuration.sessionEpoch;
	unknownCredential.peerIdentity = Identity(200);
	unknownCredential.reconnectToken = Token(220);
	CheckReason(registry.abandonUnknownCredential(
		transportC, AbandonFor(configuration, unknownCredential)).response,
		AdmissionRejectReason::CapacityReached,
		"credential abandonment preserves the fixed peer capacity");
	CheckReason(registry.abandonUnknownCredential(
		transportB, AbandonFor(configuration, unknownCredential)).response,
		AdmissionRejectReason::TransportAlreadyBound,
		"bound transport cannot use abandonment to acquire another identity");
	CheckReason(registry.admit(transportC, firstJoin), AdmissionRejectReason::CapacityReached,
		"issued identity capacity is bounded");
	registry.disconnect(transportA);
	CheckReason(registry.admit(transportC, firstJoin), AdmissionRejectReason::CapacityReached,
		"disconnect does not free reconnect identity capacity");
}

void TestPendingAdmissionReclamation()
{
	const AuthorityConfiguration configuration = CompleteConfiguration(1);
	const AdmissionRequest firstJoin = FirstJoinRequest(configuration);
	ScriptedTokenSource source;
	source.script.push_back(MakeCredential(1, 64));
	source.script.push_back(MakeCredential(1, 64));
	source.script.push_back(MakeCredential(17, 96));
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);
	const TransportPeer firstTransport{1201};
	const AdmissionResponse lostResponse = registry.admit(firstTransport, firstJoin);
	CHECK(lostResponse.admitted() && registry.peerCount() == 1 &&
		!registry.authorizesIntent(firstTransport, configuration.sessionEpoch),
		"new seat remains pending when its acceptance is not ACKed");
	registry.disconnect(firstTransport);
	CHECK(registry.peerCount() == 0 && registry.boundPeerCount() == 0,
		"disconnect reclaims an unacknowledged lost-response seat");

	const TransportPeer secondTransport{1202};
	const AdmissionRequest staleReconnect =
		ReconnectRequest(configuration, lostResponse);
	CheckReason(registry.admit(secondTransport, staleReconnect),
		AdmissionRejectReason::UnknownPeer,
		"lost-ACK client receives explicit UnknownPeer before reset");
	const AdmissionRegistryResult reset = registry.abandonUnknownCredential(
		secondTransport, AbandonFor(configuration, lostResponse));
	const AdmissionResponse replacement = reset.response;
	CHECK(replacement.admitted() &&
		replacement.peerIdentity != lostResponse.peerIdentity &&
		replacement.reconnectToken != lostResponse.reconnectToken &&
		source.calls == 3,
		"explicit abandonment rejects stale bearer reuse and issues fresh credentials");
	CHECK(registry.acknowledge(secondTransport,
		AckFor(configuration, replacement)) == AdmissionRejectReason::None,
		"replacement credential becomes durable only after ACK");
	registry.disconnect(secondTransport);
	CHECK(registry.peerCount() == 1 && registry.boundPeerCount() == 0,
		"disconnect retains an ACK-promoted reconnect credential");
}

void TestCredentialIssuanceUniqueness()
{
	const AuthorityConfiguration configuration = CompleteConfiguration(2);
	const AdmissionRequest request = FirstJoinRequest(configuration);
	ScriptedTokenSource source;
	source.script.push_back(Credential{});
	source.script.push_back(MakeCredential(1, 64));
	source.script.push_back(MakeCredential(1, 96));
	source.script.push_back(MakeCredential(17, 64));
	source.script.push_back(MakeCredential(17, 96));
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);
	const AdmissionResponse first = registry.admit(TransportPeer{1}, request);
	const AdmissionResponse second = registry.admit(TransportPeer{2}, request);
	CHECK(first.admitted() && second.admitted(),
		"issuer retries zero and duplicate credentials until unique");
	CHECK(source.calls == 5 && first.peerIdentity != second.peerIdentity &&
		first.reconnectToken != second.reconnectToken,
		"identity and token are independently unique");

	ScriptedTokenSource invalid;
	for (unsigned index = 0; index < 16; ++index) invalid.script.push_back(Credential{});
	AdmissionRegistry exhausted(&invalid);
	exhausted.beginSession(configuration);
	CheckReason(exhausted.admit(TransportPeer{3}, request),
		AdmissionRejectReason::TokenIssuanceFailed,
		"bounded issuance retries fail closed");
	CHECK(invalid.calls == 16 && exhausted.peerCount() == 0,
		"failed issuance is bounded and does not create a peer");
}

void TestMaximumCapacityBoundary()
{
	const AuthorityConfiguration configuration = CompleteConfiguration();
	const AdmissionRequest request = FirstJoinRequest(configuration);
	ScriptedTokenSource source;
	for (std::uint8_t index = 0; index < MaximumAuthorityPeers; ++index)
		source.script.push_back(MakeCredential(
			static_cast<std::uint8_t>(1 + index * 16),
			static_cast<std::uint8_t>(64 + index * 32)));
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);
	for (std::uint16_t index = 0; index < MaximumAuthorityPeers; ++index)
	{
		CHECK(registry.admit(TransportPeer{
			static_cast<std::uint64_t>(2000 + index)}, request).admitted(),
			"every fixed registry seat is usable");
	}
	CHECK(registry.peerCount() == MaximumAuthorityPeers &&
		registry.boundPeerCount() == MaximumAuthorityPeers,
		"fixed registry reaches its exact maximum without allocation");
	CheckReason(registry.admit(TransportPeer{2099}, request),
		AdmissionRejectReason::CapacityReached,
		"one request beyond the fixed registry is rejected");
	CHECK(source.calls == MaximumAuthorityPeers,
		"capacity rejection does not invoke credential source");
}
}

int main()
{
	TestRequestCodec();
	TestResponseCodec();
	TestAckCodec();
	TestCredentialAbandonCodec();
	TestSelfRetirementCodecs();
	TestHexParsers();
	TestFailClosedAdmission();
	TestAdmissionLifecycleAndIntentGate();
	TestAuthenticatedSelfRetirementLifecycle();
	TestSelfRetirementTombstoneCapacityPreflight();
	TestReconnectRejectionsAndCapacity();
	TestPendingAdmissionReclamation();
	TestCredentialIssuanceUniqueness();
	TestMaximumCapacityBoundary();

	if (failures != 0)
	{
		std::printf("%d co-op session protocol test(s) failed\n", failures);
		return 1;
	}
	std::printf("all co-op session protocol tests passed\n");
	return 0;
}
