#include "FullEngineCoopIngress.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <variant>
#include <vector>

using namespace CoopSession;

namespace
{
int failures = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL: %s\n", message); \
		++failures; \
	} \
} while (false)

class SequentialTokenSource final : public AdmissionTokenSource
{
public:
	bool issue(PeerIdentity& identity, ReconnectToken& token) noexcept override
	{
		++sequence_;
		for (std::size_t index = 0; index < identity.size(); ++index)
			identity[index] = static_cast<std::uint8_t>(sequence_ + index);
		for (std::size_t index = 0; index < token.size(); ++index)
			token[index] = static_cast<std::uint8_t>(0x80 + sequence_ + index);
		return true;
	}

private:
	std::uint8_t sequence_ = 0;
};

class RecordingExecutionSink final : public TacticalIntentExecutionSink
{
public:
	TacticalIntentExecutionDisposition execute(
		const AuthorizedTacticalIntent& intent) noexcept override
	{
		if (count < received.size()) received[count] = intent;
		++count;
		return nextDisposition;
	}

	std::array<AuthorizedTacticalIntent, 16> received{};
	std::size_t count = 0;
	TacticalIntentExecutionDisposition nextDisposition =
		TacticalIntentExecutionDisposition::Applied;
};

AuthorityConfiguration Authority(std::uint64_t epoch)
{
	AuthorityConfiguration configuration;
	configuration.enabled = true;
	configuration.sessionEpoch = epoch;
	configuration.runtimeFingerprintSupplied = true;
	configuration.runtimeFingerprint = {3, 0x1122334455667788ull,
		0x99aabbccddeeff00ull};
	configuration.contentManifestSupplied = true;
	for (std::size_t index = 0;
		index < configuration.contentManifestSha256.size(); ++index)
	{
		configuration.contentManifestSha256[index] =
			static_cast<std::uint8_t>(index + 1);
	}
	configuration.maximumPeers = 2;
	return configuration;
}

FullEngineCoopSessionConfiguration Session(std::uint64_t epoch)
{
	FullEngineCoopSessionConfiguration configuration;
	configuration.admission = Authority(epoch);
	configuration.tactical = TacticalAuthorityContext{epoch, 7, 20, 3};
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
	const AdmissionResponse& admitted)
{
	AdmissionRequest request = FirstJoin(configuration);
	request.peerIdentity = admitted.peerIdentity;
	request.reconnectToken = admitted.reconnectToken;
	return request;
}

AdmissionRequestBytes RequestBytes(const AdmissionRequest& request)
{
	AdmissionRequestBytes bytes{};
	CHECK(EncodeAdmissionRequest(request, bytes),
		"test admission request encodes");
	return bytes;
}

AdmissionResponse DecodeResponse(const AdmissionIngressResult& result)
{
	CHECK(result.responseReady,
		"admission ingress returns one complete response buffer");
	AdmissionResponse response;
	CHECK(DecodeAdmissionResponse(result.responseBytes.data(),
		result.responseBytes.size(), response) == DecodeResult::Ok,
		"admission ingress response is an exact decodable frame");
	return response;
}

TacticalIntent Intent(
	const AdmissionResponse& peer,
	std::uint64_t commandId,
	TacticalEntityId actor,
	TacticalAuthorityContext context = TacticalAuthorityContext{0x501, 7, 20, 3})
{
	TacticalIntent intent;
	intent.sessionEpoch = context.sessionEpoch;
	intent.claimedPeerIdentity = peer.peerIdentity;
	intent.commandId = commandId;
	intent.worldGeneration = context.worldGeneration;
	intent.baseRevision = context.revision;
	intent.turnSerial = context.turnSerial;
	intent.actor = actor;
	intent.payload = MoveTacticalIntent{1234, 17, true};
	return intent;
}

std::vector<std::uint8_t> IntentBytes(const TacticalIntent& intent)
{
	std::vector<std::uint8_t> bytes;
	CHECK(EncodeTacticalIntent(intent, bytes) ==
		TacticalIntentCodecResult::Success, "test tactical intent encodes");
	return bytes;
}

void TestLifecycleFailsClosed()
{
	SequentialTokenSource tokens;
	RecordingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	const FullEngineCoopSessionConfiguration valid = Session(0x501);

	FullEngineCoopSessionConfiguration disabled = valid;
	disabled.admission.enabled = false;
	CHECK(ingress.beginSession(disabled) ==
		FullEngineCoopStartResult::AuthorityDisabled,
		"disabled authority cannot start full-engine ingress");
	CHECK(!ingress.active(), "disabled start leaves ingress inactive");

	FullEngineCoopSessionConfiguration incomplete = valid;
	incomplete.admission.contentManifestSupplied = false;
	CHECK(ingress.beginSession(incomplete) ==
		FullEngineCoopStartResult::ConfigurationIncomplete,
		"missing canonical content identity cannot start ingress");
	CHECK(!ingress.active(), "incomplete start leaves ingress inactive");

	FullEngineCoopSessionConfiguration invalidContext = valid;
	invalidContext.tactical.revision = 0;
	CHECK(ingress.beginSession(invalidContext) ==
		FullEngineCoopStartResult::InvalidTacticalContext,
		"zero tactical revision cannot start ingress");
	CHECK(!ingress.active(), "invalid context leaves ingress inactive");

	FullEngineCoopSessionConfiguration wrongEpoch = valid;
	wrongEpoch.tactical.sessionEpoch = 0x502;
	CHECK(ingress.beginSession(wrongEpoch) ==
		FullEngineCoopStartResult::SessionEpochMismatch,
		"admission and tactical epochs must match");
	CHECK(!ingress.active(), "epoch mismatch leaves ingress inactive");

	CHECK(ingress.beginSession(valid) == FullEngineCoopStartResult::Success,
		"complete explicit configuration starts ingress");
	CHECK(ingress.active(), "successful start marks ingress active");

	// A failed replacement is a fail-closed lifecycle event, not a request to
	// keep serving the old campaign epoch.
	CHECK(ingress.beginSession(disabled) ==
		FullEngineCoopStartResult::AuthorityDisabled,
		"invalid replacement is rejected");
	CHECK(!ingress.active() && ingress.admittedPeerCount() == 0 &&
		ingress.actorBindingCount() == 0,
		"invalid replacement clears and deactivates the previous session");

	const AdmissionRequestBytes inactiveRequest =
		RequestBytes(FirstJoin(valid.admission));
	const AdmissionResponse inactiveResponse = DecodeResponse(
		ingress.handleAdmission(TransportPeer{6001},
			inactiveRequest.data(), inactiveRequest.size()));
	CHECK(inactiveResponse.rejectReason ==
		AdmissionRejectReason::AuthorityDisabled,
		"inactive ingress rejects even a structurally valid admission");

	const TacticalIntentIngressResult inactiveTactical =
		ingress.handleTacticalIntent(TransportPeer{6001},
			nullptr, 0);
	CHECK(inactiveTactical.authorization.reason ==
		TacticalIntentAuthorizationReason::NotConfigured &&
		!inactiveTactical.executionAttempted,
		"inactive ingress never decodes or executes tactical input");
}

void TestAdmissionDecodeAndExactResponses()
{
	SequentialTokenSource tokens;
	RecordingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	const FullEngineCoopSessionConfiguration configuration = Session(0x501);
	CHECK(ingress.beginSession(configuration) ==
		FullEngineCoopStartResult::Success, "admission test session starts");

	const AdmissionIngressResult truncated =
		ingress.handleAdmission(TransportPeer{6101}, nullptr, 0);
	const AdmissionResponse truncatedResponse = DecodeResponse(truncated);
	CHECK(truncated.decodeResult == DecodeResult::WrongSize &&
		truncatedResponse.rejectReason == AdmissionRejectReason::MalformedRequest &&
		truncatedResponse.sessionEpoch == configuration.admission.sessionEpoch &&
		IsZero(truncatedResponse.peerIdentity) &&
		IsZero(truncatedResponse.reconnectToken),
		"truncated admission produces a complete sanitized malformed response");

	AdmissionRequestBytes unsupported =
		RequestBytes(FirstJoin(configuration.admission));
	unsupported[4] = static_cast<std::uint8_t>(CurrentProtocolVersion + 1);
	unsupported[5] = 0;
	const AdmissionIngressResult unsupportedResult = ingress.handleAdmission(
		TransportPeer{6101}, unsupported.data(), unsupported.size());
	const AdmissionResponse unsupportedResponse = DecodeResponse(unsupportedResult);
	CHECK(unsupportedResult.decodeResult == DecodeResult::UnsupportedProtocol &&
		unsupportedResponse.rejectReason ==
			AdmissionRejectReason::UnsupportedProtocol &&
		IsZero(unsupportedResponse.peerIdentity),
		"unsupported admission version receives its explicit rejection");

	AdmissionRequestBytes badMagic =
		RequestBytes(FirstJoin(configuration.admission));
	badMagic[0] ^= 0xff;
	const AdmissionResponse badMagicResponse = DecodeResponse(
		ingress.handleAdmission(TransportPeer{6101},
			badMagic.data(), badMagic.size()));
	CHECK(badMagicResponse.rejectReason == AdmissionRejectReason::MalformedRequest,
		"bad admission magic cannot leak partially decoded identity");

	const TransportPeer sender{6101};
	const AdmissionRequestBytes valid =
		RequestBytes(FirstJoin(configuration.admission));
	const AdmissionIngressResult admittedResult = ingress.handleAdmission(
		sender, valid.data(), valid.size());
	const AdmissionResponse admitted = DecodeResponse(admittedResult);
	CHECK(admittedResult.decodeResult == DecodeResult::Ok && admitted.admitted() &&
		!IsZero(admitted.peerIdentity) && !IsZero(admitted.reconnectToken) &&
		ingress.admittedPeerCount() == 1 && ingress.boundPeerCount() == 1,
		"valid sender-derived admission returns one issued credential");
}

void TestIntentRoutingAndLifecycle()
{
	SequentialTokenSource tokens;
	RecordingExecutionSink sink;
	FullEngineCoopIngress ingress(tokens, sink);
	const FullEngineCoopSessionConfiguration configuration = Session(0x501);
	CHECK(ingress.beginSession(configuration) ==
		FullEngineCoopStartResult::Success, "intent routing session starts");

	const TransportPeer senderA{6201};
	const TransportPeer senderB{6202};
	const TransportPeer senderC{6203};
	const AdmissionRequestBytes firstJoin =
		RequestBytes(FirstJoin(configuration.admission));
	const AdmissionResponse peerA = DecodeResponse(ingress.handleAdmission(
		senderA, firstJoin.data(), firstJoin.size()));
	const AdmissionResponse peerB = DecodeResponse(ingress.handleAdmission(
		senderB, firstJoin.data(), firstJoin.size()));
	CHECK(peerA.admitted() && peerB.admitted() &&
		peerA.peerIdentity != peerB.peerIdentity,
		"two transports receive distinct admitted identities");

	const TacticalEntityId actorA{10, 4};
	const TacticalEntityId actorB{11, 9};
	CHECK(ingress.bindActorForTransport(senderA, actorA) ==
		TacticalActorBindingResult::Success,
		"admitted sender A receives actor A ownership");
	CHECK(ingress.bindActorForTransport(senderB, actorB) ==
		TacticalActorBindingResult::Success,
		"admitted sender B receives actor B ownership");
	CHECK(ingress.bindActorForTransport(senderC, TacticalEntityId{12, 1}) ==
		TacticalActorBindingResult::InvalidPeer,
		"unadmitted transport cannot create an actor binding");

	TacticalIntent commandA1 = Intent(peerA, 1, actorA);
	std::vector<std::uint8_t> commandA1Bytes = IntentBytes(commandA1);
	const TacticalIntentIngressResult truncated = ingress.handleTacticalIntent(
		senderA, commandA1Bytes.data(), commandA1Bytes.size() - 1);
	CHECK(truncated.decodeResult == TacticalIntentCodecResult::Invalid &&
		!truncated.executionAttempted && sink.count == 0,
		"truncated tactical bytes never reach authority or execution");

	TacticalIntent spoof = Intent(peerB, 1, actorB);
	const std::vector<std::uint8_t> spoofBytes = IntentBytes(spoof);
	const TacticalIntentIngressResult spoofed = ingress.handleTacticalIntent(
		senderA, spoofBytes.data(), spoofBytes.size());
	CHECK(spoofed.authorization.reason ==
		TacticalIntentAuthorizationReason::ClaimedIdentityMismatch &&
		!spoofed.executionAttempted && sink.count == 0,
		"transport A cannot route an envelope claiming peer B");

	sink.nextDisposition = TacticalIntentExecutionDisposition::Rejected;
	const TacticalIntentIngressResult rejected = ingress.handleTacticalIntent(
		senderA, commandA1Bytes.data(), commandA1Bytes.size());
	CHECK(rejected.authorization.reason ==
		TacticalIntentAuthorizationReason::None &&
		rejected.executionAttempted &&
		rejected.execution == TacticalIntentExecutionDisposition::Rejected &&
		sink.count == 1,
		"authorized command reaches the sink and reports gameplay rejection");
	CHECK(sink.received[0].peerIdentity == peerA.peerIdentity &&
		sink.received[0].commandId == 1 && sink.received[0].actor == actorA &&
		sink.received[0].context.sessionEpoch == 0x501 &&
		std::holds_alternative<MoveTacticalIntent>(sink.received[0].payload),
		"sink receives only resolved identity, authoritative context and payload");

	const TacticalIntentIngressResult replayRejected =
		ingress.handleTacticalIntent(
			senderA, commandA1Bytes.data(), commandA1Bytes.size());
	CHECK(replayRejected.authorization.reason ==
		TacticalIntentAuthorizationReason::DuplicateCommand &&
		!replayRejected.executionAttempted && sink.count == 1,
		"sink rejection is terminal and cannot replay the same command ID");

	sink.nextDisposition = TacticalIntentExecutionDisposition::Applied;
	TacticalIntent commandB1 = Intent(peerB, 1, actorB);
	commandB1.payload = FaceTacticalIntent{6};
	const std::vector<std::uint8_t> commandB1Bytes = IntentBytes(commandB1);
	const TacticalIntentIngressResult applied = ingress.handleTacticalIntent(
		senderB, commandB1Bytes.data(), commandB1Bytes.size());
	CHECK(applied.execution == TacticalIntentExecutionDisposition::Applied &&
		applied.executionAttempted && sink.count == 2 &&
		std::holds_alternative<FaceTacticalIntent>(sink.received[1].payload),
		"independent peer command is sanitized and applied");

	sink.nextDisposition = TacticalIntentExecutionDisposition::Retained;
	TacticalIntent commandA2 = Intent(peerA, 2, actorA);
	commandA2.payload = StanceTacticalIntent{TacticalIntentStance::Crouched};
	const std::vector<std::uint8_t> commandA2Bytes = IntentBytes(commandA2);
	const TacticalIntentIngressResult retained = ingress.handleTacticalIntent(
		senderA, commandA2Bytes.data(), commandA2Bytes.size());
	CHECK(retained.execution == TacticalIntentExecutionDisposition::Retained &&
		retained.executionAttempted && sink.count == 3,
		"bounded executor retention is surfaced as terminal acceptance");

	ingress.disconnect(senderA);
	TacticalIntent commandA3 = Intent(peerA, 3, actorA);
	commandA3.payload = StopTacticalIntent{};
	const std::vector<std::uint8_t> commandA3Bytes = IntentBytes(commandA3);
	const TacticalIntentIngressResult disconnected = ingress.handleTacticalIntent(
		senderA, commandA3Bytes.data(), commandA3Bytes.size());
	CHECK(disconnected.authorization.reason ==
		TacticalIntentAuthorizationReason::NotAdmitted &&
		!disconnected.executionAttempted && sink.count == 3,
		"disconnect immediately removes sender authority");

	const AdmissionRequestBytes reconnectBytes =
		RequestBytes(Reconnect(configuration.admission, peerA));
	const AdmissionResponse reconnected = DecodeResponse(ingress.handleAdmission(
		senderC, reconnectBytes.data(), reconnectBytes.size()));
	CHECK(reconnected.admitted() &&
		reconnected.peerIdentity == peerA.peerIdentity,
		"credential reconnects the same peer on a new transport");
	sink.nextDisposition = TacticalIntentExecutionDisposition::Applied;
	const TacticalIntentIngressResult afterReconnect =
		ingress.handleTacticalIntent(
			senderC, commandA3Bytes.data(), commandA3Bytes.size());
	CHECK(afterReconnect.executionAttempted && sink.count == 4 &&
		sink.received[3].peerIdentity == peerA.peerIdentity,
		"reconnect preserves actor ownership and command sequence by identity");

	CHECK(ingress.beginGeneration(8, 1, 1) ==
		TacticalAuthorityConfigurationResult::Success,
		"new tactical generation advances through ingress lifecycle");
	CHECK(ingress.actorBindingCount() == 0,
		"generation barrier clears every actor binding");
	TacticalIntent commandA4 = Intent(peerA, 4, actorA,
		TacticalAuthorityContext{0x501, 8, 1, 1});
	commandA4.payload = EndTurnTacticalIntent{};
	const std::vector<std::uint8_t> commandA4Bytes = IntentBytes(commandA4);
	const TacticalIntentIngressResult unbound = ingress.handleTacticalIntent(
		senderC, commandA4Bytes.data(), commandA4Bytes.size());
	CHECK(unbound.authorization.reason ==
		TacticalIntentAuthorizationReason::ActorNotOwned && sink.count == 4,
		"new generation rejects commands until server rebinds actors");
	CHECK(ingress.bindActorForTransport(senderC, actorA) ==
		TacticalActorBindingResult::Success,
		"reconnected transport receives a fresh generation binding");
	const TacticalIntentIngressResult generationAccepted =
		ingress.handleTacticalIntent(
			senderC, commandA4Bytes.data(), commandA4Bytes.size());
	CHECK(generationAccepted.executionAttempted && sink.count == 5 &&
		std::holds_alternative<EndTurnTacticalIntent>(sink.received[4].payload),
		"generation rebind preserves the session-monotonic command sequence");

	ingress.endSession();
	CHECK(!ingress.active() && ingress.admittedPeerCount() == 0 &&
		ingress.boundPeerCount() == 0 && ingress.actorBindingCount() == 0,
		"endSession clears all authority and ingress state");
	const TacticalIntentIngressResult afterEnd = ingress.handleTacticalIntent(
		senderC, commandA4Bytes.data(), commandA4Bytes.size());
	CHECK(!afterEnd.executionAttempted && sink.count == 5,
		"ended session cannot execute a previously valid frame");
}
}

int main()
{
	static_assert(std::is_nothrow_copy_assignable<TacticalIntentPayload>::value,
		"authorized payload handoff must remain noexcept");
	TestLifecycleFailsClosed();
	TestAdmissionDecodeAndExactResponses();
	TestIntentRoutingAndLifecycle();
	if (failures != 0)
	{
		std::printf("%d full-engine co-op ingress test(s) failed\n", failures);
		return 1;
	}
	std::puts("all full-engine co-op ingress tests passed");
	return 0;
}
