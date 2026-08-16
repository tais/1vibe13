#include "CoopAdmission.h"
#include "CoopTacticalAuthority.h"
#include "CoopTacticalIntent.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); } } while (0)

static_assert(noexcept(std::declval<TacticalIntentAuthority&>().authorize(
	std::declval<const TransportPeer&>(), std::declval<const TacticalIntent&>())));
static_assert(noexcept(std::declval<TacticalIntentAuthority&>().bindActor(
	std::declval<const PeerIdentity&>(), TacticalEntityId{})));
static_assert(TacticalIntentHeaderWireSize == 72);
static_assert(MaximumTacticalIntentWireSize == 79);

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

class SequentialTokenSource final : public AdmissionTokenSource
{
public:
	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override
	{
		if (next_ == 0 || next_ > 15) return false;
		identity = Identity(static_cast<std::uint8_t>(0x10 * next_));
		token = Token(static_cast<std::uint8_t>(0x80 + 0x10 * next_));
		++next_;
		return true;
	}

private:
	std::size_t next_ = 1;
};

AuthorityConfiguration Configuration(std::uint64_t epoch = 0x101)
{
	AuthorityConfiguration configuration;
	configuration.enabled = true;
	configuration.sessionEpoch = epoch;
	configuration.runtimeFingerprintSupplied = true;
	configuration.runtimeFingerprint = RuntimeCompatibilityFingerprint{
		1, UINT64_C(0x1122334455667788), UINT64_C(0x99aabbccddeeff00)};
	configuration.contentManifestSupplied = true;
	configuration.contentManifestSha256 = ContentHash(0x40);
	configuration.maximumPeers = MaximumAuthorityPeers;
	return configuration;
}

AdmissionRequest FirstJoin(const AuthorityConfiguration& configuration)
{
	AdmissionRequest request;
	request.sessionEpoch = configuration.sessionEpoch;
	request.runtimeFingerprint = configuration.runtimeFingerprint;
	request.contentManifestSha256 = configuration.contentManifestSha256;
	return request;
}

AdmissionRequest Reconnect(
	const AuthorityConfiguration& configuration,
	const AdmissionResponse& accepted)
{
	AdmissionRequest request = FirstJoin(configuration);
	request.peerIdentity = accepted.peerIdentity;
	request.reconnectToken = accepted.reconnectToken;
	return request;
}

TacticalAuthorityContext Context(std::uint64_t epoch = 0x101)
{
	return TacticalAuthorityContext{epoch, 7, 20, 3};
}

TacticalIntent Intent(
	const PeerIdentity& peer,
	std::uint64_t commandId,
	TacticalEntityId actor,
	TacticalAuthorityContext context = Context())
{
	TacticalIntent intent;
	intent.sessionEpoch = context.sessionEpoch;
	intent.claimedPeerIdentity = peer;
	intent.commandId = commandId;
	intent.worldGeneration = context.worldGeneration;
	intent.baseRevision = context.revision;
	intent.turnSerial = context.turnSerial;
	intent.actor = actor;
	intent.payload = StopTacticalIntent{};
	return intent;
}

bool SameIntent(const TacticalIntent& left, const TacticalIntent& right)
{
	if (left.protocolVersion != right.protocolVersion ||
		left.sessionEpoch != right.sessionEpoch ||
		left.claimedPeerIdentity != right.claimedPeerIdentity ||
		left.commandId != right.commandId ||
		left.worldGeneration != right.worldGeneration ||
		left.baseRevision != right.baseRevision ||
		left.turnSerial != right.turnSerial || left.actor != right.actor ||
		KindOf(left.payload) != KindOf(right.payload))
		return false;
	if (const auto* move = std::get_if<MoveTacticalIntent>(&left.payload))
	{
		const auto& other = std::get<MoveTacticalIntent>(right.payload);
		return move->destinationGrid == other.destinationGrid &&
			move->movementMode == other.movementMode &&
			move->reverse == other.reverse;
	}
	if (const auto* face = std::get_if<FaceTacticalIntent>(&left.payload))
		return face->direction == std::get<FaceTacticalIntent>(right.payload).direction;
	if (const auto* stance = std::get_if<StanceTacticalIntent>(&left.payload))
		return stance->stance == std::get<StanceTacticalIntent>(right.payload).stance;
	return true;
}

void CheckReason(
	const TacticalIntentAuthorizationResult& result,
	TacticalIntentAuthorizationReason expected,
	const char* message)
{
	CHECK(result.reason == expected, message);
	CHECK(static_cast<bool>(result) ==
		(expected == TacticalIntentAuthorizationReason::None),
		"authorization boolean agrees with reason");
}

void TestIntentCodec()
{
	TacticalIntent intent;
	intent.sessionEpoch = UINT64_C(0x0807060504030201);
	intent.claimedPeerIdentity = Identity(0x20);
	intent.commandId = UINT64_C(0x1817161514131211);
	intent.worldGeneration = UINT64_C(0x2827262524232221);
	intent.baseRevision = UINT64_C(0x3837363534333231);
	intent.turnSerial = UINT64_C(0x4847464544434241);
	intent.actor = TacticalEntityId{UINT16_C(0x5251), UINT32_C(0x56555453)};
	intent.payload = MoveTacticalIntent{
		INT32_C(0x04030201), UINT16_C(0x0605), true};

	std::vector<std::uint8_t> bytes;
	CHECK(EncodeTacticalIntent(intent, bytes) == TacticalIntentCodecResult::Success,
		"canonical move intent encodes");
	CHECK(bytes.size() == 79, "move intent is exact maximum wire size");
	CHECK(bytes[0] == 'J' && bytes[1] == '2' && bytes[2] == 'C' && bytes[3] == 'I',
		"intent magic is byte exact");
	CHECK(bytes[4] == 1 && bytes[5] == 0 && bytes[6] == 1 && bytes[7] == 0,
		"intent version, kind, and reserved byte are exact");
	for (std::size_t index = 0; index < 8; ++index)
	{
		CHECK(bytes[8 + index] == index + 1, "epoch is little endian");
		CHECK(bytes[32 + index] == index + 0x11, "command ID is little endian");
		CHECK(bytes[40 + index] == index + 0x21, "generation is little endian");
		CHECK(bytes[48 + index] == index + 0x31, "revision is little endian");
		CHECK(bytes[56 + index] == index + 0x41, "turn serial is little endian");
	}
	for (std::size_t index = 0; index < 16; ++index)
		CHECK(bytes[16 + index] == index + 0x20, "identity occupies exact range");
	CHECK(bytes[64] == 0x51 && bytes[65] == 0x52 &&
		bytes[66] == 0x53 && bytes[67] == 0x54 &&
		bytes[68] == 0x55 && bytes[69] == 0x56,
		"actor identity is explicitly little endian");
	CHECK(bytes[70] == 7 && bytes[71] == 0,
		"payload length is exact and little endian");
	CHECK(bytes[72] == 1 && bytes[73] == 2 && bytes[74] == 3 && bytes[75] == 4 &&
		bytes[76] == 5 && bytes[77] == 6 && bytes[78] == 1,
		"move payload is canonical");

	TacticalIntent decoded;
	CHECK(DecodeTacticalIntent(bytes, decoded) == TacticalIntentCodecResult::Success,
		"golden move intent decodes");
	CHECK(SameIntent(intent, decoded), "move intent round trip preserves every field");

	TacticalIntent sentinel = intent;
	sentinel.commandId = 99;
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		TacticalIntent output = sentinel;
		CHECK(DecodeTacticalIntent(bytes.data(), size, output) !=
			TacticalIntentCodecResult::Success,
			"every truncated intent is rejected");
		CHECK(output.commandId == sentinel.commandId,
			"failed truncated decode is transactional");
	}
	std::vector<std::uint8_t> extended = bytes;
	extended.push_back(0);
	CHECK(DecodeTacticalIntent(extended, decoded) == TacticalIntentCodecResult::Invalid,
		"extended intent is rejected");
	CHECK(DecodeTacticalIntent(nullptr, bytes.size(), decoded) ==
		TacticalIntentCodecResult::Invalid, "null intent input is rejected");

	auto malformed = bytes;
	malformed[0] ^= 1;
	CHECK(DecodeTacticalIntent(malformed, decoded) == TacticalIntentCodecResult::Invalid,
		"wrong intent magic is rejected");
	malformed = bytes;
	malformed[4] = 2;
	CHECK(DecodeTacticalIntent(malformed, decoded) ==
		TacticalIntentCodecResult::UnsupportedVersion,
		"unsupported intent version is explicit");
	malformed = bytes;
	malformed[6] = 0xff;
	CHECK(DecodeTacticalIntent(malformed, decoded) == TacticalIntentCodecResult::Invalid,
		"unknown intent kind is rejected");
	malformed = bytes;
	malformed[7] = 1;
	CHECK(DecodeTacticalIntent(malformed, decoded) == TacticalIntentCodecResult::Invalid,
		"nonzero reserved byte is rejected");
	malformed = bytes;
	malformed[70] = 6;
	CHECK(DecodeTacticalIntent(malformed, decoded) == TacticalIntentCodecResult::Invalid,
		"wrong payload length is rejected");
	malformed = bytes;
	malformed[78] = 2;
	CHECK(DecodeTacticalIntent(malformed, decoded) == TacticalIntentCodecResult::Invalid,
		"noncanonical move boolean is rejected");

	struct PayloadCase
	{
		TacticalIntentPayload payload;
		std::size_t wireSize;
	};
	const std::array<PayloadCase, 5> payloads{{
		{MoveTacticalIntent{12, 3, false}, 79},
		{FaceTacticalIntent{7}, 73},
		{StanceTacticalIntent{TacticalIntentStance::Prone}, 73},
		{StopTacticalIntent{}, 72},
		{EndTurnTacticalIntent{}, 72}}};
	for (const PayloadCase& payloadCase : payloads)
	{
		TacticalIntent candidate = intent;
		candidate.payload = payloadCase.payload;
		std::vector<std::uint8_t> candidateBytes;
		CHECK(EncodeTacticalIntent(candidate, candidateBytes) ==
			TacticalIntentCodecResult::Success,
			"every closed intent payload encodes");
		CHECK(candidateBytes.size() == payloadCase.wireSize,
			"every intent payload has its exact wire size");
		TacticalIntent output;
		CHECK(DecodeTacticalIntent(candidateBytes, output) ==
			TacticalIntentCodecResult::Success && SameIntent(candidate, output),
			"every closed intent payload round trips");
	}

	std::vector<std::uint8_t> unchanged{0xaa, 0xbb};
	TacticalIntent invalid = intent;
	invalid.commandId = 0;
	CHECK(EncodeTacticalIntent(invalid, unchanged) == TacticalIntentCodecResult::Invalid &&
		unchanged == std::vector<std::uint8_t>({0xaa, 0xbb}),
		"failed encode preserves output");
	invalid = intent;
	invalid.actor = {};
	CHECK(!IsStructurallyValidTacticalIntent(invalid), "invalid actor is structural failure");
	invalid = intent;
	invalid.payload = MoveTacticalIntent{-1, 0, false};
	CHECK(!IsStructurallyValidTacticalIntent(invalid), "negative move grid is rejected");
	invalid = intent;
	invalid.payload = FaceTacticalIntent{8};
	CHECK(!IsStructurallyValidTacticalIntent(invalid), "direction eight is rejected");
	invalid = intent;
	invalid.payload = StanceTacticalIntent{static_cast<TacticalIntentStance>(9)};
	CHECK(!IsStructurallyValidTacticalIntent(invalid), "unknown stance is rejected");
}

void TestAuthorityConfigurationAndBindings()
{
	SequentialTokenSource tokens;
	AdmissionRegistry admission(&tokens);
	TacticalIntentAuthority authority(admission);
	CHECK(authority.beginSession(Context()) ==
		TacticalAuthorityConfigurationResult::AdmissionUnavailable,
		"authority fails closed before admission configuration");

	AuthorityConfiguration configuration = Configuration();
	admission.beginSession(configuration);
	TacticalAuthorityContext invalid = Context();
	invalid.revision = 0;
	CHECK(authority.beginSession(invalid) ==
		TacticalAuthorityConfigurationResult::InvalidContext,
		"zero authority context is rejected");
	CHECK(authority.beginSession(Context(0x102)) ==
		TacticalAuthorityConfigurationResult::AdmissionUnavailable,
		"authority epoch must match admission epoch");
	CHECK(authority.beginSession(Context()) ==
		TacticalAuthorityConfigurationResult::Success && authority.configured(),
		"complete matching authority session starts");

	PeerIdentity zero{};
	CHECK(authority.bindActor(zero, TacticalEntityId{1, 1}) ==
		TacticalActorBindingResult::InvalidPeer, "zero actor owner is rejected");
	CHECK(authority.bindActor(Identity(1), TacticalEntityId{}) ==
		TacticalActorBindingResult::InvalidActor, "invalid actor identity is rejected");
	const PeerIdentity owner = Identity(1);
	CHECK(authority.bindActor(owner, TacticalEntityId{1, 1}) ==
		TacticalActorBindingResult::Success, "actor binds to owner");
	CHECK(authority.bindActor(owner, TacticalEntityId{1, 1}) ==
		TacticalActorBindingResult::Success, "same actor binding is idempotent");
	CHECK(authority.actorBindingCount() == 1, "idempotent bind does not consume capacity");
	CHECK(authority.bindActor(Identity(2), TacticalEntityId{1, 1}) ==
		TacticalActorBindingResult::ActorAlreadyOwned,
		"actor cannot be rebound to another peer");
	CHECK(authority.unbindActor(TacticalEntityId{1, 1}), "bound actor can be removed");
	CHECK(!authority.unbindActor(TacticalEntityId{1, 1}),
		"removing absent actor is a no-op");

	for (std::size_t index = 0; index < MaximumAuthoritativeActors; ++index)
	{
		CHECK(authority.bindActor(owner, TacticalEntityId{
			static_cast<std::uint16_t>(index), 1}) ==
			TacticalActorBindingResult::Success,
			"bounded actor table accepts every declared slot");
	}
	CHECK(authority.bindActor(owner, TacticalEntityId{300, 1}) ==
		TacticalActorBindingResult::CapacityReached,
		"actor table refuses overflow");
	CHECK(authority.beginGeneration(7, 1, 1) ==
		TacticalAuthorityConfigurationResult::StaleContext,
		"generation cannot repeat");
	CHECK(authority.beginGeneration(8, 0, 1) ==
		TacticalAuthorityConfigurationResult::InvalidContext,
		"new generation requires nonzero context");
	CHECK(authority.beginGeneration(8, 1, 1) ==
		TacticalAuthorityConfigurationResult::Success,
		"strictly newer generation starts");
	CHECK(authority.actorBindingCount() == 0,
		"generation barrier clears actor bindings");
	CHECK(authority.advanceContext(0, 1) ==
		TacticalAuthorityConfigurationResult::InvalidContext,
		"zero context advance is rejected");
	CHECK(authority.advanceContext(1, 0) ==
		TacticalAuthorityConfigurationResult::InvalidContext,
		"zero turn advance is rejected");
	CHECK(authority.advanceContext(1, 1) ==
		TacticalAuthorityConfigurationResult::Success,
		"unchanged context is idempotent");
	CHECK(authority.advanceContext(2, 2) ==
		TacticalAuthorityConfigurationResult::Success,
		"revision and turn advance monotonically");
	CHECK(authority.advanceContext(1, 2) ==
		TacticalAuthorityConfigurationResult::StaleContext,
		"revision cannot move backwards");
}

void TestAuthorityAuthorization()
{
	SequentialTokenSource tokens;
	AdmissionRegistry admission(&tokens);
	AuthorityConfiguration configuration = Configuration();
	admission.beginSession(configuration);
	const TransportPeer senderA{5001};
	const TransportPeer senderB{5002};
	const TransportPeer senderC{5003};
	const AdmissionResponse peerA = admission.admit(senderA, FirstJoin(configuration));
	const AdmissionResponse peerB = admission.admit(senderB, FirstJoin(configuration));
	CHECK(peerA.admitted() && peerB.admitted(), "two peers are admitted");

	TacticalIntentAuthority authority(admission);
	CHECK(authority.beginSession(Context()) ==
		TacticalAuthorityConfigurationResult::Success,
		"authority starts over admitted session");
	const TacticalEntityId actorA{10, 1};
	const TacticalEntityId actorB{11, 1};
	CHECK(authority.bindActor(peerA.peerIdentity, actorA) ==
		TacticalActorBindingResult::Success, "peer A owns actor A");
	CHECK(authority.bindActor(peerB.peerIdentity, actorB) ==
		TacticalActorBindingResult::Success, "peer B owns actor B");

	TacticalIntent command = Intent(peerA.peerIdentity, 1, actorA);
	TacticalIntentAuthorizationResult result = authority.authorize(senderA, command);
	CheckReason(result, TacticalIntentAuthorizationReason::None,
		"first owned command is authorized");
	CHECK(result.peerIdentity == peerA.peerIdentity && result.commandId == 1,
		"authorization returns server-resolved peer and command ID");
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::DuplicateCommand,
		"accepted command cannot be replayed");
	command.commandId = 3;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::OutOfOrderCommand,
		"command gaps fail closed");

	command.commandId = 2;
	command.actor = actorB;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::ActorNotOwned,
		"peer cannot command another peer's actor");
	command.actor = actorA;
	command.claimedPeerIdentity = peerB.peerIdentity;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::ClaimedIdentityMismatch,
		"payload identity cannot spoof transport-bound peer");
	command.claimedPeerIdentity = peerA.peerIdentity;
	command.commandId = 0;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::InvalidCommandId,
		"zero command identifier is rejected explicitly");
	command.commandId = 2;
	command.payload = FaceTacticalIntent{8};
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::InvalidIntent,
		"direct callers cannot bypass structural intent validation");
	command.payload = StopTacticalIntent{};

	command.worldGeneration = 8;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::WrongGeneration,
		"wrong world generation is rejected");
	command.worldGeneration = 7;
	command.baseRevision = 19;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::StaleRevision,
		"stale base revision is rejected");
	command.baseRevision = 21;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::FutureRevision,
		"future base revision is rejected");
	command.baseRevision = 20;
	command.turnSerial = 2;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::StaleTurn,
		"stale turn serial is rejected");
	command.turnSerial = 4;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::FutureTurn,
		"future turn serial is rejected");
	command.turnSerial = 3;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::None,
		"rejected attempts do not consume the command identifier");

	TacticalIntent peerBCommand = Intent(peerB.peerIdentity, 1, actorB);
	CheckReason(authority.authorize(senderA, peerBCommand),
		TacticalIntentAuthorizationReason::ClaimedIdentityMismatch,
		"sender A cannot submit peer B envelope");
	CheckReason(authority.authorize(senderC, peerBCommand),
		TacticalIntentAuthorizationReason::NotAdmitted,
		"unknown transport cannot submit an intent");
	CheckReason(authority.authorize(senderB, peerBCommand),
		TacticalIntentAuthorizationReason::None,
		"peer command sequences are independent");

	admission.disconnect(senderA);
	command.commandId = 3;
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::NotAdmitted,
		"disconnect immediately removes command authority");
	const AdmissionResponse rebound = admission.admit(
		senderC, Reconnect(configuration, peerA));
	CHECK(rebound.admitted() && rebound.peerIdentity == peerA.peerIdentity,
		"credential reconnects peer on a new transport");
	CheckReason(authority.authorize(senderA, command),
		TacticalIntentAuthorizationReason::NotAdmitted,
		"reconnect invalidates old transport");
	CheckReason(authority.authorize(senderC, command),
		TacticalIntentAuthorizationReason::None,
		"reconnect preserves peer command sequence");

	CHECK(authority.beginGeneration(8, 1, 1) ==
		TacticalAuthorityConfigurationResult::Success,
		"generation barrier starts after reconnect");
	TacticalIntent generationCommand = Intent(
		peerA.peerIdentity, 4, actorA, TacticalAuthorityContext{0x101, 8, 1, 1});
	CheckReason(authority.authorize(senderC, generationCommand),
		TacticalIntentAuthorizationReason::ActorNotOwned,
		"generation barrier requires fresh actor binding");
	CHECK(authority.bindActor(peerA.peerIdentity, actorA) ==
		TacticalActorBindingResult::Success, "actor rebinds in new generation");
	CheckReason(authority.authorize(senderC, generationCommand),
		TacticalIntentAuthorizationReason::None,
		"command sequence remains session-monotonic across generation");
}

void TestAuthoritySessionResetAndPeerBound()
{
	SequentialTokenSource tokens;
	AdmissionRegistry admission(&tokens);
	AuthorityConfiguration first = Configuration(0x201);
	admission.beginSession(first);
	TacticalIntentAuthority authority(admission);
	CHECK(authority.beginSession(TacticalAuthorityContext{0x201, 1, 1, 1}) ==
		TacticalAuthorityConfigurationResult::Success, "first session starts");

	std::array<TransportPeer, MaximumAuthorityPeers> senders{};
	std::array<AdmissionResponse, MaximumAuthorityPeers> peers{};
	for (std::size_t index = 0; index < MaximumAuthorityPeers; ++index)
	{
		senders[index] = TransportPeer{
			static_cast<std::uint64_t>(6000 + index)};
		peers[index] = admission.admit(senders[index], FirstJoin(first));
		CHECK(peers[index].admitted(), "fixed peer registry admits declared capacity");
		const TacticalEntityId actor{static_cast<std::uint16_t>(index), 1};
		CHECK(authority.bindActor(peers[index].peerIdentity, actor) ==
			TacticalActorBindingResult::Success, "each admitted peer binds one actor");
		TacticalIntent command = Intent(peers[index].peerIdentity, 1, actor,
			TacticalAuthorityContext{0x201, 1, 1, 1});
		CheckReason(authority.authorize(senders[index], command),
			TacticalIntentAuthorizationReason::None,
			"authority sequence table covers admission capacity exactly");
	}

	AuthorityConfiguration second = Configuration(0x202);
	admission.beginSession(second);
	CHECK(authority.beginSession(TacticalAuthorityContext{0x202, 2, 1, 1}) ==
		TacticalAuthorityConfigurationResult::Success,
		"new admission epoch starts new authority session");
	CHECK(authority.actorBindingCount() == 0,
		"new session clears every prior actor binding");
	const TransportPeer newSender{7000};
	const AdmissionResponse newPeer = admission.admit(newSender, FirstJoin(second));
	CHECK(newPeer.admitted(), "new session issues a fresh peer identity");
	const TacticalEntityId actor{20, 1};
	CHECK(authority.bindActor(newPeer.peerIdentity, actor) ==
		TacticalActorBindingResult::Success, "new session binds new actor");
	TacticalIntent command = Intent(newPeer.peerIdentity, 1, actor,
		TacticalAuthorityContext{0x202, 2, 1, 1});
	CheckReason(authority.authorize(newSender, command),
		TacticalIntentAuthorizationReason::None,
		"new session resets command sequence to one");
}

void TestAdmissionEpochCannotOutrunAuthority()
{
	SequentialTokenSource tokens;
	AdmissionRegistry admission(&tokens);
	AuthorityConfiguration first = Configuration(0x301);
	admission.beginSession(first);
	const TransportPeer firstSender{8001};
	const AdmissionResponse firstPeer = admission.admit(firstSender, FirstJoin(first));
	CHECK(firstPeer.admitted(), "first epoch peer is admitted");

	TacticalIntentAuthority authority(admission);
	const TacticalAuthorityContext staleContext{0x301, 5, 6, 7};
	CHECK(authority.beginSession(staleContext) ==
		TacticalAuthorityConfigurationResult::Success,
		"first authority epoch starts");

	// Deliberately omit authority.beginSession after rotating admission. This is
	// the integration failure mode the explicit context epoch check must contain.
	AuthorityConfiguration second = Configuration(0x302);
	admission.beginSession(second);
	const TransportPeer secondSender{8002};
	const AdmissionResponse secondPeer = admission.admit(secondSender, FirstJoin(second));
	CHECK(secondPeer.admitted(), "second epoch peer is admitted");
	const TacticalEntityId actor{30, 1};
	CHECK(authority.bindActor(secondPeer.peerIdentity, actor) ==
		TacticalActorBindingResult::Success,
		"stale authority can be misconfigured with a new peer for the negative test");
	TacticalIntent command = Intent(secondPeer.peerIdentity, 1, actor,
		TacticalAuthorityContext{0x302, staleContext.worldGeneration,
			staleContext.revision, staleContext.turnSerial});
	CheckReason(authority.authorize(secondSender, command),
		TacticalIntentAuthorizationReason::WrongSessionEpoch,
		"new admission epoch cannot authorize against stale authority context");
}
}

int main()
{
	TestIntentCodec();
	TestAuthorityConfigurationAndBindings();
	TestAuthorityAuthorization();
	TestAuthoritySessionResetAndPeerBound();
	TestAdmissionEpochCannotOutrunAuthority();
	if (failures != 0)
	{
		std::printf("%d co-op tactical authority test(s) failed\n", failures);
		return 1;
	}
	std::puts("all co-op tactical authority tests passed");
	return 0;
}
