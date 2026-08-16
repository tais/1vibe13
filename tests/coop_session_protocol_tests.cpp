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
	CHECK(bytes[4] == 1 && bytes[5] == 0 && bytes[6] == 1 && bytes[7] == 0,
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
	malformed[4] = 2;
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
	CHECK(bytes[4] == 1 && bytes[5] == 0 && bytes[6] == 2 && bytes[7] == 0,
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
	malformed[4] = 2;
	CHECK(DecodeAdmissionResponse(malformed.data(), malformed.size(), decoded) ==
		DecodeResult::UnsupportedProtocol, "unsupported response version is explicit");

	for (std::uint32_t raw = 0; raw <= UINT16_MAX; ++raw)
	{
		const bool expectedKnown = raw <=
			static_cast<std::uint16_t>(AdmissionRejectReason::TokenIssuanceFailed);
		CHECK(IsKnownAdmissionRejectReason(static_cast<AdmissionRejectReason>(raw)) ==
			expectedKnown, "reject reason domain is exhaustive");
	}
	for (std::uint16_t raw = 0;
		raw <= static_cast<std::uint16_t>(AdmissionRejectReason::TokenIssuanceFailed); ++raw)
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
	malformed[64] = 16;
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
	invalidReason.rejectReason = static_cast<AdmissionRejectReason>(16);
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
	const TransportPeer sender{UINT32_C(0x01020304), UINT16_C(5000)};
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
	CheckReason(registry.admit(TransportPeer{0, 5000}, request),
		AdmissionRejectReason::InvalidTransport, "zero sender address is rejected");
	CheckReason(registry.admit(TransportPeer{1, 0}, request),
		AdmissionRejectReason::InvalidTransport, "zero sender port is rejected");
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
	const TransportPeer firstTransport{UINT32_C(0x01020304), UINT16_C(5000)};
	const TransportPeer secondTransport{UINT32_C(0x01020304), UINT16_C(5001)};
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
	CHECK(registry.resolvePeerForIntent(
		firstTransport, configuration.sessionEpoch, resolved) &&
		resolved == accepted.peerIdentity,
		"intent gate resolves only the server-bound peer identity");
	CHECK(registry.authorizesIntent(firstTransport, configuration.sessionEpoch),
		"bound sender and epoch authorize intent");
	CHECK(!registry.authorizesIntent(firstTransport, configuration.sessionEpoch + 1) &&
		!registry.authorizesIntent(secondTransport, configuration.sessionEpoch),
		"wrong epoch and unbound sender fail intent authorization");

	AdmissionRequest reconnect = ReconnectRequest(configuration, accepted);
	const AdmissionResponse rebound = registry.admit(secondTransport, reconnect);
	CHECK(rebound.admitted(), "valid reconnect credential is admitted on new sender");
	CHECK(!registry.authorizesIntent(firstTransport, configuration.sessionEpoch) &&
		registry.authorizesIntent(secondTransport, configuration.sessionEpoch),
		"reconnect atomically invalidates old sender and binds new sender");

	registry.disconnect(secondTransport);
	CHECK(!registry.authorizesIntent(secondTransport, configuration.sessionEpoch) &&
		registry.peerCount() == 1 && registry.boundPeerCount() == 0,
		"disconnect revokes transport authority but retains reconnect identity");
	CHECK(registry.admit(firstTransport, reconnect).admitted(),
		"retained credential reconnects within the same epoch");
	registry.clearTransportBindings();
	CHECK(!registry.authorizesIntent(firstTransport, configuration.sessionEpoch) &&
		registry.peerCount() == 1,
		"clearing bindings revokes transports without erasing credentials");
	CHECK(registry.admit(secondTransport, reconnect).admitted(),
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

void TestReconnectRejectionsAndCapacity()
{
	const AuthorityConfiguration configuration = CompleteConfiguration(2);
	const AdmissionRequest firstJoin = FirstJoinRequest(configuration);
	const TransportPeer transportA{1, 1001};
	const TransportPeer transportB{2, 1002};
	const TransportPeer transportC{3, 1003};
	ScriptedTokenSource source;
	source.script.push_back(MakeCredential(1, 64));
	source.script.push_back(MakeCredential(17, 96));
	AdmissionRegistry registry(&source);
	registry.beginSession(configuration);
	const AdmissionResponse peerA = registry.admit(transportA, firstJoin);
	const AdmissionResponse peerB = registry.admit(transportB, firstJoin);
	CHECK(peerA.admitted() && peerB.admitted(), "capacity seats are admitted");

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
	CheckReason(registry.admit(transportC, firstJoin), AdmissionRejectReason::CapacityReached,
		"issued identity capacity is bounded");
	registry.disconnect(transportA);
	CheckReason(registry.admit(transportC, firstJoin), AdmissionRejectReason::CapacityReached,
		"disconnect does not free reconnect identity capacity");
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
	const AdmissionResponse first = registry.admit(TransportPeer{1, 1}, request);
	const AdmissionResponse second = registry.admit(TransportPeer{2, 2}, request);
	CHECK(first.admitted() && second.admitted(),
		"issuer retries zero and duplicate credentials until unique");
	CHECK(source.calls == 5 && first.peerIdentity != second.peerIdentity &&
		first.reconnectToken != second.reconnectToken,
		"identity and token are independently unique");

	ScriptedTokenSource invalid;
	for (unsigned index = 0; index < 16; ++index) invalid.script.push_back(Credential{});
	AdmissionRegistry exhausted(&invalid);
	exhausted.beginSession(configuration);
	CheckReason(exhausted.admit(TransportPeer{3, 3}, request),
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
			static_cast<std::uint32_t>(index + 1),
			static_cast<std::uint16_t>(2000 + index)}, request).admitted(),
			"every fixed registry seat is usable");
	}
	CHECK(registry.peerCount() == MaximumAuthorityPeers &&
		registry.boundPeerCount() == MaximumAuthorityPeers,
		"fixed registry reaches its exact maximum without allocation");
	CheckReason(registry.admit(TransportPeer{99, 2099}, request),
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
	TestHexParsers();
	TestFailClosedAdmission();
	TestAdmissionLifecycleAndIntentGate();
	TestReconnectRejectionsAndCapacity();
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
