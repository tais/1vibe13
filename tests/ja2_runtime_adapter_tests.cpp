#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalWorldDelta.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>
#include <Engine/Adapters/JA2/TacticalWorldService.h>
#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <cstdint>
#include <cstdio>
#include <limits>
#include <utility>
#include <variant>
#include <vector>

namespace
{
int failures = 0;

void check(bool condition, const char* message)
{
	if (!condition)
	{
		++failures;
		std::printf("FAIL  %s\n", message);
		return;
	}
	std::printf("ok    %s\n", message);
}

TacticalWorldDelta CodecFixture()
{
	TacticalWorldDelta delta;
	delta.previousEpoch = 0x0102030405060708ull;
	delta.currentEpoch = 0x1112131415161718ull;
	delta.events = {
		TacticalWorldResetEvent{
			0x0102030405060708ull, 0x1112131415161718ull},
		TacticalSectorChangedEvent{
			TacticalSectorSnapshot{
				std::numeric_limits<std::int16_t>::min(),
				std::numeric_limits<std::int16_t>::max(),
				std::numeric_limits<std::int8_t>::min(), false},
			TacticalSectorSnapshot{
				-1, 258, std::numeric_limits<std::int8_t>::max(), true}},
		TacticalTurnChangedEvent{
			TacticalTurnSnapshot{false, true, 0x7fu, 0x2122232425262728ull},
			TacticalTurnSnapshot{true, false, 0xffu, 0x3132333435363738ull}},
		TacticalActorEnteredEvent{TacticalActorSnapshot{
			TacticalEntityId{0x1234u, 0x89abcdefu}, 0xfeu, 0x4567u,
			std::numeric_limits<std::int32_t>::min(), -127, 0xffu, 0xbeefu,
			TacticalStance::Prone, std::numeric_limits<std::int16_t>::min(),
			-1, std::numeric_limits<std::int16_t>::max(), -12345, 23456,
			true, true}},
		TacticalActorLeftEvent{TacticalEntityId{0, 1}},
		TacticalActorMovedEvent{
			TacticalEntityId{1, 2},
			std::numeric_limits<std::int32_t>::min(),
			std::numeric_limits<std::int32_t>::max(),
			std::numeric_limits<std::int8_t>::min(),
			std::numeric_limits<std::int8_t>::max(), 0, 0xffu},
		TacticalActorStanceChangedEvent{
			TacticalEntityId{2, 3}, TacticalStance::Unknown,
			TacticalStance::Prone, 0, 0xffffu},
		TacticalActorVitalsChangedEvent{
			TacticalEntityId{3, 4},
			std::numeric_limits<std::int16_t>::min(),
			std::numeric_limits<std::int16_t>::max(),
			-1, 0, 1, 2, -300, 400, -500, 600}};
	return delta;
}

std::vector<std::uint8_t> EncodeSingleCodecEvent(TacticalWorldEvent event)
{
	TacticalWorldDelta delta;
	delta.previousEpoch = 1;
	delta.currentEpoch = 1;
	delta.events.push_back(std::move(event));
	std::vector<std::uint8_t> bytes;
	if (EncodeTacticalWorldDelta(delta, bytes) !=
		TacticalWorldDeltaEncodeResult::Success) bytes.clear();
	return bytes;
}

class RecordingRuntimeMessageSink final : public RuntimeMessageSink
{
public:
	void receiveMessage(const RuntimeMessage& message) override
	{
		messages.push_back(message);
	}

	std::vector<RuntimeMessage> messages;
};
}

int main()
{
	constexpr TacticalEntityId invalidEntity;
	constexpr TacticalEntityId firstIncarnation{7, 9001};
	constexpr TacticalEntityId reusedSlot{7, 9002};
	static_assert(!invalidEntity.valid(), "default tactical identity must be invalid");
	static_assert(firstIncarnation.valid(), "slot and incarnation form a valid identity");
	static_assert(firstIncarnation != reusedSlot,
		"slot reuse must not preserve tactical identity");
	check(firstIncarnation < reusedSlot,
		"tactical identities have deterministic slot and incarnation ordering");

	TacticalWorldSnapshot tacticalSnapshot;
	std::vector<TacticalActorSnapshot> unorderedActors{
		TacticalActorSnapshot{reusedSlot, 1, 12, 220, 0, 3, 18,
			TacticalStance::Crouched, 65, 77, 80, 54, 90, true, true},
		TacticalActorSnapshot{TacticalEntityId{2, 51}, 0, 4, 100, 0, 1, 4,
			TacticalStance::Standing, 90, 95, 95, 100, 100, true, true},
		TacticalActorSnapshot{firstIncarnation, 1, 12, 219, 0, 2, 17,
			TacticalStance::Crouched, 70, 78, 80, 55, 90, true, true}};
	check(TacticalWorldSnapshot::create(
			44, TacticalSectorSnapshot{9, 1, 0, true},
			TacticalTurnSnapshot{true, true, 0, 8},
			unorderedActors, tacticalSnapshot) == TacticalSnapshotCreateError::None &&
		tacticalSnapshot.epoch() == 44 && tacticalSnapshot.actors().size() == 3 &&
		tacticalSnapshot.actors()[0].id == TacticalEntityId{2, 51} &&
		tacticalSnapshot.actors()[1].id == firstIncarnation &&
		tacticalSnapshot.find(reusedSlot) != nullptr &&
		tacticalSnapshot.find(TacticalEntityId{7, 9003}) == nullptr,
		"tactical snapshots own pointer-free actors in deterministic identity order");
	const std::uint64_t acceptedEpoch = tacticalSnapshot.epoch();
	std::vector<TacticalActorSnapshot> duplicateActors{
		unorderedActors[0], unorderedActors[0]};
	check(TacticalWorldSnapshot::create(
			45, TacticalSectorSnapshot{}, TacticalTurnSnapshot{},
			duplicateActors, tacticalSnapshot) == TacticalSnapshotCreateError::DuplicateEntity &&
		tacticalSnapshot.epoch() == acceptedEpoch,
		"invalid tactical captures cannot partially replace the last good snapshot");
	MemoryTacticalWorldService memoryWorld;
	memoryWorld.publish(tacticalSnapshot);
	ServiceCatalog tacticalServices;
	check(RegisterTacticalWorldService(tacticalServices, memoryWorld) ==
			EngineServiceRegistrationError::None,
		"tactical world service registers as an explicit versioned host extension");
	const auto resolvedWorld = tacticalServices.resolve<TacticalWorldService>(
		TacticalWorldServiceId, EngineServiceVersion{1, 0});
	TacticalWorldSnapshot capturedWorld;
	check(resolvedWorld &&
		resolvedWorld.service->capture(capturedWorld) == TacticalWorldCaptureResult::Success &&
		capturedWorld.epoch() == tacticalSnapshot.epoch() &&
		capturedWorld.find(firstIncarnation) != nullptr,
		"packages can transactionally capture a pointer-free tactical world view");
	memoryWorld.clear();
	check(memoryWorld.capture(capturedWorld) == TacticalWorldCaptureResult::Unavailable &&
		capturedWorld.epoch() == tacticalSnapshot.epoch() &&
		!tacticalServices.resolve<TacticalWorldService>(
			TacticalWorldServiceId, EngineServiceVersion{2, 0}),
		"unavailable and incompatible tactical services preserve the last good capture");
	std::vector<TacticalActorSnapshot> changedActors = tacticalSnapshot.actors();
	changedActors.erase(changedActors.begin());
	changedActors[0].grid = 221;
	changedActors[0].direction = 4;
	changedActors[0].animation = 30;
	changedActors[0].stance = TacticalStance::Prone;
	changedActors[0].life = 70;
	changedActors.push_back(TacticalActorSnapshot{
		TacticalEntityId{9, 1}, 2, 18, 330, 0, 6, 4,
		TacticalStance::Standing, 80, 90, 90, 70, 80, true, true});
	TacticalWorldSnapshot changedWorld;
	check(TacticalWorldSnapshot::create(
			44, tacticalSnapshot.sector(), TacticalTurnSnapshot{true, true, 1, 9},
			changedActors, changedWorld) == TacticalSnapshotCreateError::None,
		"changed tactical fixture remains a valid immutable snapshot");
	TacticalWorldDelta worldDelta;
	check(DiffTacticalWorldSnapshots(tacticalSnapshot, changedWorld, 6, worldDelta) ==
			TacticalWorldDiffResult::Success && worldDelta.events.size() == 6 &&
		std::holds_alternative<TacticalTurnChangedEvent>(worldDelta.events[0]) &&
		std::holds_alternative<TacticalActorLeftEvent>(worldDelta.events[1]) &&
		std::holds_alternative<TacticalActorMovedEvent>(worldDelta.events[2]) &&
		std::holds_alternative<TacticalActorStanceChangedEvent>(worldDelta.events[3]) &&
		std::holds_alternative<TacticalActorVitalsChangedEvent>(worldDelta.events[4]) &&
		std::holds_alternative<TacticalActorEnteredEvent>(worldDelta.events[5]),
		"tactical world diffs emit bounded deterministic turn and actor events");
	TacticalWorldDelta undersizedDelta;
	check(DiffTacticalWorldSnapshots(tacticalSnapshot, changedWorld, 5, undersizedDelta) ==
			TacticalWorldDiffResult::CapacityReached && undersizedDelta.events.empty(),
		"tactical world diff capacity failure cannot publish a partial event stream");
	TacticalWorldSnapshot reloadedWorld;
	TacticalWorldSnapshot::create(
		45, tacticalSnapshot.sector(), tacticalSnapshot.turn(),
		tacticalSnapshot.actors(), reloadedWorld);
	check(DiffTacticalWorldSnapshots(tacticalSnapshot, reloadedWorld, 1, worldDelta) ==
			TacticalWorldDiffResult::Success && worldDelta.events.size() == 1 &&
			std::holds_alternative<TacticalWorldResetEvent>(worldDelta.events[0]),
		"tactical epoch changes collapse unrelated worlds into one reset event");

	const TacticalWorldDelta codecFixture = CodecFixture();
	std::vector<std::uint8_t> encodedDelta;
	check(EncodeTacticalWorldDelta(codecFixture, encodedDelta) ==
			TacticalWorldDeltaEncodeResult::Success,
		"tactical delta codec encodes every current event kind");
	TacticalWorldDelta decodedDelta;
	const TacticalWorldDeltaDecodeResult deltaDecodeResult =
		DecodeTacticalWorldDelta(encodedDelta, decodedDelta);
	const bool decodedEventTypes =
		deltaDecodeResult == TacticalWorldDeltaDecodeResult::Success &&
		decodedDelta.events.size() == 8 &&
		std::holds_alternative<TacticalWorldResetEvent>(decodedDelta.events[0]) &&
		std::holds_alternative<TacticalSectorChangedEvent>(decodedDelta.events[1]) &&
		std::holds_alternative<TacticalTurnChangedEvent>(decodedDelta.events[2]) &&
		std::holds_alternative<TacticalActorEnteredEvent>(decodedDelta.events[3]) &&
		std::holds_alternative<TacticalActorLeftEvent>(decodedDelta.events[4]) &&
		std::holds_alternative<TacticalActorMovedEvent>(decodedDelta.events[5]) &&
		std::holds_alternative<TacticalActorStanceChangedEvent>(decodedDelta.events[6]) &&
		std::holds_alternative<TacticalActorVitalsChangedEvent>(decodedDelta.events[7]);
	bool decodedEventFields = false;
	if (decodedEventTypes)
	{
		const auto& sector = std::get<TacticalSectorChangedEvent>(decodedDelta.events[1]);
		const auto& turn = std::get<TacticalTurnChangedEvent>(decodedDelta.events[2]);
		const auto& actor = std::get<TacticalActorEnteredEvent>(decodedDelta.events[3]).actor;
		const auto& moved = std::get<TacticalActorMovedEvent>(decodedDelta.events[5]);
		const auto& vitals = std::get<TacticalActorVitalsChangedEvent>(decodedDelta.events[7]);
		decodedEventFields =
			decodedDelta.previousEpoch == codecFixture.previousEpoch &&
			decodedDelta.currentEpoch == codecFixture.currentEpoch &&
			sector.previous.x == std::numeric_limits<std::int16_t>::min() &&
			sector.previous.y == std::numeric_limits<std::int16_t>::max() &&
			sector.previous.z == std::numeric_limits<std::int8_t>::min() &&
			!sector.previous.loaded && sector.current.loaded &&
			!turn.previous.turnBased && turn.previous.inCombat &&
			turn.current.turnBased && !turn.current.inCombat &&
			turn.current.serial == 0x3132333435363738ull &&
			actor.id == TacticalEntityId{0x1234u, 0x89abcdefu} &&
			actor.grid == std::numeric_limits<std::int32_t>::min() &&
			actor.level == -127 && actor.stance == TacticalStance::Prone &&
			actor.actionPoints == std::numeric_limits<std::int16_t>::min() &&
			actor.maximumLife == std::numeric_limits<std::int16_t>::max() &&
			actor.breath == -12345 && actor.maximumBreath == 23456 &&
			moved.previousLevel == std::numeric_limits<std::int8_t>::min() &&
			moved.currentGrid == std::numeric_limits<std::int32_t>::max() &&
			vitals.previousActionPoints == std::numeric_limits<std::int16_t>::min() &&
			vitals.currentActionPoints == std::numeric_limits<std::int16_t>::max() &&
			vitals.previousBreath == -300 && vitals.currentMaximumBreath == 600;
	}
	std::vector<std::uint8_t> reencodedDelta;
	check(decodedEventFields &&
		EncodeTacticalWorldDelta(decodedDelta, reencodedDelta) ==
			TacticalWorldDeltaEncodeResult::Success &&
		reencodedDelta == encodedDelta,
		"tactical delta codec deterministically round-trips all fields and signed limits");

	TacticalWorldDelta resetDelta;
	resetDelta.previousEpoch = 0x0102030405060708ull;
	resetDelta.currentEpoch = 0x1112131415161718ull;
	resetDelta.events.push_back(TacticalWorldResetEvent{
		resetDelta.previousEpoch, resetDelta.currentEpoch});
	std::vector<std::uint8_t> resetBytes;
	const std::vector<std::uint8_t> expectedResetBytes{
		0x54, 0x57, 0x44, 0x31, 0x01, 0x00,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
		0x01, 0x00, 0x00, 0x00, 0x01,
		0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
		0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11};
	check(EncodeTacticalWorldDelta(resetDelta, resetBytes) ==
			TacticalWorldDeltaEncodeResult::Success &&
		resetBytes == expectedResetBytes,
		"tactical delta version 1 has a fixed little-endian golden representation");

	bool rejectedEveryTruncation = true;
	for (std::size_t length = 0; length < encodedDelta.size(); ++length)
	{
		const std::vector<std::uint8_t> truncated(
			encodedDelta.begin(), encodedDelta.begin() + length);
		TacticalWorldDelta ignored;
		if (DecodeTacticalWorldDelta(truncated, ignored) !=
			TacticalWorldDeltaDecodeResult::Invalid)
		{
			rejectedEveryTruncation = false;
			break;
		}
	}
	check(rejectedEveryTruncation,
		"tactical delta codec rejects every truncated prefix");

	std::vector<std::uint8_t> malformed = encodedDelta;
	malformed.push_back(0);
	TacticalWorldDelta unchangedDelta;
	unchangedDelta.previousEpoch = 777;
	unchangedDelta.currentEpoch = 888;
	unchangedDelta.events.push_back(TacticalActorLeftEvent{TacticalEntityId{5, 6}});
	check(DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
			TacticalWorldDeltaDecodeResult::Invalid &&
		unchangedDelta.previousEpoch == 777 && unchangedDelta.currentEpoch == 888 &&
		unchangedDelta.events.size() == 1 &&
		std::get<TacticalActorLeftEvent>(unchangedDelta.events[0]).actor ==
			TacticalEntityId{5, 6},
		"trailing tactical delta bytes are rejected without replacing prior state");

	malformed = encodedDelta;
	malformed[4] = 2;
	check(DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
			TacticalWorldDeltaDecodeResult::UnsupportedVersion,
		"tactical delta codec distinguishes unsupported format versions");
	malformed = encodedDelta;
	malformed[0] ^= 0xffu;
	check(DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
			TacticalWorldDeltaDecodeResult::Invalid,
		"tactical delta codec rejects unknown wire magic");
	malformed = encodedDelta;
	malformed[22] = 0xffu;
	malformed[23] = 0xffu;
	malformed[24] = 0xffu;
	malformed[25] = 0x7fu;
	check(DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
			TacticalWorldDeltaDecodeResult::TooManyEvents &&
		DecodeTacticalWorldDelta(encodedDelta, unchangedDelta, 7) ==
			TacticalWorldDeltaDecodeResult::TooManyEvents,
		"tactical delta codec enforces fixed and caller-selected event bounds");

	malformed = EncodeSingleCodecEvent(TacticalWorldResetEvent{1, 2});
	malformed[26] = 0xffu;
	const bool rejectsUnknownTag = DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
		TacticalWorldDeltaDecodeResult::Invalid;
	malformed = EncodeSingleCodecEvent(TacticalSectorChangedEvent{
		TacticalSectorSnapshot{1, 2, 0, true},
		TacticalSectorSnapshot{2, 3, 1, false}});
	malformed[32] = 2;
	const bool rejectsBoolean = DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
		TacticalWorldDeltaDecodeResult::Invalid;
	malformed = EncodeSingleCodecEvent(TacticalActorEnteredEvent{
		TacticalActorSnapshot{TacticalEntityId{8, 9}, 0, 0, -1, 0, 0, 0,
			TacticalStance::Standing, 0, 1, 1, 2, 2, true, true}});
	malformed[44] = 0xffu;
	const bool rejectsActorStance = DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
		TacticalWorldDeltaDecodeResult::Invalid;
	malformed = EncodeSingleCodecEvent(TacticalActorStanceChangedEvent{
		TacticalEntityId{8, 9}, TacticalStance::Standing,
		TacticalStance::Crouched, 1, 2});
	const bool usesFixedStanceCodes = malformed[33] == 1 && malformed[34] == 2;
	malformed[33] = 0xffu;
	const bool rejectsChangedStance = DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
		TacticalWorldDeltaDecodeResult::Invalid;
	malformed = EncodeSingleCodecEvent(TacticalActorLeftEvent{TacticalEntityId{8, 9}});
	malformed[27] = 0xffu;
	malformed[28] = 0xffu;
	const bool rejectsInvalidEntity = DecodeTacticalWorldDelta(malformed, unchangedDelta) ==
		TacticalWorldDeltaDecodeResult::Invalid;
	check(usesFixedStanceCodes && rejectsUnknownTag && rejectsBoolean && rejectsActorStance &&
		rejectsChangedStance && rejectsInvalidEntity,
		"tactical delta codec rejects malformed tags, booleans, stances, and identities");

	std::vector<std::uint8_t> unchangedBytes{0xaa, 0x55};
	TacticalWorldDelta invalidDelta = codecFixture;
	invalidDelta.previousEpoch = 0;
	const bool rejectsInvalidEncode =
		EncodeTacticalWorldDelta(invalidDelta, unchangedBytes) ==
			TacticalWorldDeltaEncodeResult::Invalid;
	invalidDelta = codecFixture;
	invalidDelta.events[3] = TacticalActorEnteredEvent{TacticalActorSnapshot{
		TacticalEntityId{}, 0, 0, 0, 0, 0, 0, TacticalStance::Unknown,
		0, 0, 0, 0, 0, true, true}};
	const bool rejectsInvalidActor =
		EncodeTacticalWorldDelta(invalidDelta, unchangedBytes) ==
			TacticalWorldDeltaEncodeResult::Invalid;
	check(rejectsInvalidEncode && rejectsInvalidActor &&
		unchangedBytes == std::vector<std::uint8_t>({0xaa, 0x55}) &&
		EncodeTacticalWorldDelta(codecFixture, unchangedBytes, 7) ==
			TacticalWorldDeltaEncodeResult::TooManyEvents &&
		unchangedBytes == std::vector<std::uint8_t>({0xaa, 0x55}),
		"invalid tactical deltas fail transactionally before publication");

	check(IsValidEngineIdentifier(TacticalWorldDeltaMessageTopic) &&
		IsValidEngineIdentifier(TacticalWorldDeltaMessageSource),
		"tactical delta messages use stable package-safe topic and source identifiers");
	RuntimeMessageBus tacticalMessages(2, encodedDelta.size());
	RecordingRuntimeMessageSink tacticalMessageSink;
	TacticalWorldDeltaPublisher tacticalPublisher(
		tacticalMessages,
		TacticalWorldDeltaPublishLimits{8, encodedDelta.size()});
	const TacticalWorldDeltaPublishResult tacticalPublished =
		tacticalPublisher.publish(codecFixture);
	const RuntimeMessageDispatchResult tacticalDispatch =
		tacticalMessages.addSink(tacticalMessageSink) ==
			RuntimeMessageSinkRegistrationError::None
			? tacticalMessages.dispatchPending()
			: RuntimeMessageDispatchResult{};
	TacticalWorldDelta deliveredDelta;
	const bool deliveredPayloadDecodes = tacticalMessageSink.messages.size() == 1 &&
		DecodeTacticalWorldDelta(
			tacticalMessageSink.messages[0].payload, deliveredDelta) ==
			TacticalWorldDeltaDecodeResult::Success;
	check(tacticalPublished && tacticalPublished.sequence == 1 &&
		tacticalPublished.payloadBytes == encodedDelta.size() &&
		tacticalDispatch.messages == 1 && tacticalDispatch.delivered == 1 &&
		deliveredPayloadDecodes && deliveredDelta.events.size() == 8 &&
		tacticalMessageSink.messages[0].topic == TacticalWorldDeltaMessageTopic &&
		tacticalMessageSink.messages[0].source == TacticalWorldDeltaMessageSource &&
		tacticalMessageSink.messages[0].payload == encodedDelta,
		"tactical delta publisher delivers unchanged deterministic codec bytes to package sinks");

	check(
		MapTacticalWorldDeltaEncodeError(TacticalWorldDeltaEncodeResult::Success) ==
			TacticalWorldDeltaPublishError::None &&
		MapTacticalWorldDeltaEncodeError(TacticalWorldDeltaEncodeResult::Invalid) ==
			TacticalWorldDeltaPublishError::InvalidDelta &&
		MapTacticalWorldDeltaEncodeError(TacticalWorldDeltaEncodeResult::TooManyEvents) ==
			TacticalWorldDeltaPublishError::TooManyEvents &&
		MapTacticalWorldDeltaEncodeError(
			TacticalWorldDeltaEncodeResult::AllocationFailure) ==
			TacticalWorldDeltaPublishError::CodecAllocationFailure &&
		MapTacticalWorldDeltaMessageError(RuntimeMessagePublishError::PayloadTooLarge) ==
			TacticalWorldDeltaPublishError::PayloadTooLarge &&
		MapTacticalWorldDeltaMessageError(RuntimeMessagePublishError::QueueFull) ==
			TacticalWorldDeltaPublishError::QueueFull &&
		MapTacticalWorldDeltaMessageError(RuntimeMessagePublishError::SequenceExhausted) ==
			TacticalWorldDeltaPublishError::SequenceExhausted &&
		MapTacticalWorldDeltaMessageError(RuntimeMessagePublishError::AllocationFailure) ==
			TacticalWorldDeltaPublishError::MessageAllocationFailure &&
		MapTacticalWorldDeltaMessageError(RuntimeMessagePublishError::InvalidTopic) ==
			TacticalWorldDeltaPublishError::InvalidMessageIdentifier &&
		MapTacticalWorldDeltaMessageError(RuntimeMessagePublishError::InvalidSource) ==
			TacticalWorldDeltaPublishError::InvalidMessageIdentifier,
		"tactical delta publication explicitly maps every codec and bus failure family");

	RuntimeMessageBus validationMessages(2, encodedDelta.size());
	TacticalWorldDeltaPublisher validationPublisher(
		validationMessages, TacticalWorldDeltaPublishLimits{7, encodedDelta.size()});
	invalidDelta = codecFixture;
	invalidDelta.previousEpoch = 0;
	const TacticalWorldDeltaPublishResult invalidPublication =
		validationPublisher.publish(invalidDelta);
	const TacticalWorldDeltaPublishResult eventLimitedPublication =
		validationPublisher.publish(codecFixture);
	TacticalWorldDeltaPublisher validationSuccessPublisher(
		validationMessages, TacticalWorldDeltaPublishLimits{8, encodedDelta.size()});
	const TacticalWorldDeltaPublishResult afterValidationFailures =
		validationSuccessPublisher.publish(codecFixture);
	check(invalidPublication.error == TacticalWorldDeltaPublishError::InvalidDelta &&
		invalidPublication.sequence == 0 && invalidPublication.payloadBytes == 0 &&
		eventLimitedPublication.error == TacticalWorldDeltaPublishError::TooManyEvents &&
		eventLimitedPublication.sequence == 0 &&
		afterValidationFailures.sequence == 1 && validationMessages.queued() == 1,
		"codec and event-limit rejection cannot enqueue or consume a message sequence");

	RuntimeMessageBus busPayloadMessages(2, encodedDelta.size() - 1);
	TacticalWorldDeltaPublisher busPayloadPublisher(
		busPayloadMessages,
		TacticalWorldDeltaPublishLimits{8, encodedDelta.size() + 100});
	const TacticalWorldDeltaPublishResult busPayloadRejected =
		busPayloadPublisher.publish(codecFixture);
	const TacticalWorldDeltaPublishResult smallerPayloadAccepted =
		busPayloadPublisher.publish(resetDelta);
	RuntimeMessageBus callerPayloadMessages(2, encodedDelta.size());
	TacticalWorldDeltaPublisher callerPayloadPublisher(
		callerPayloadMessages,
		TacticalWorldDeltaPublishLimits{8, encodedDelta.size() - 1});
	const TacticalWorldDeltaPublishResult callerPayloadRejected =
		callerPayloadPublisher.publish(codecFixture);
	TacticalWorldDeltaPublisher callerPayloadSuccessPublisher(
		callerPayloadMessages,
		TacticalWorldDeltaPublishLimits{8, encodedDelta.size()});
	const TacticalWorldDeltaPublishResult afterCallerPayloadFailure =
		callerPayloadSuccessPublisher.publish(codecFixture);
	check(busPayloadPublisher.maximumPayloadBytes() == encodedDelta.size() - 1 &&
		busPayloadRejected.error == TacticalWorldDeltaPublishError::PayloadTooLarge &&
		busPayloadRejected.payloadBytes == encodedDelta.size() &&
		smallerPayloadAccepted.sequence == 1 &&
		callerPayloadPublisher.maximumPayloadBytes() == encodedDelta.size() - 1 &&
		callerPayloadRejected.error == TacticalWorldDeltaPublishError::PayloadTooLarge &&
		afterCallerPayloadFailure.sequence == 1,
		"caller payload limits are clamped by the bus and reject without advancing sequences");

	RuntimeMessageBus fullMessages(1, encodedDelta.size());
	TacticalWorldDeltaPublisher fullPublisher(
		fullMessages, TacticalWorldDeltaPublishLimits{8, encodedDelta.size()});
	const TacticalWorldDeltaPublishResult firstQueued = fullPublisher.publish(resetDelta);
	const TacticalWorldDeltaPublishResult queueRejected = fullPublisher.publish(resetDelta);
	const RuntimeMessageDispatchResult drainedFullQueue = fullMessages.dispatchPending();
	const TacticalWorldDeltaPublishResult afterQueueFailure = fullPublisher.publish(resetDelta);
	RuntimeMessageBus boundedMessages(1, encodedDelta.size());
	TacticalWorldDeltaPublisher boundedPublisher(
		boundedMessages,
		TacticalWorldDeltaPublishLimits{
			MaximumTacticalWorldDeltaEvents + 100, encodedDelta.size() + 100});
	check(firstQueued.sequence == 1 &&
		queueRejected.error == TacticalWorldDeltaPublishError::QueueFull &&
		queueRejected.sequence == 0 && queueRejected.payloadBytes == resetBytes.size() &&
		drainedFullQueue.messages == 1 && afterQueueFailure.sequence == 2 &&
		boundedPublisher.maximumEvents() == MaximumTacticalWorldDeltaEvents &&
		boundedPublisher.maximumPayloadBytes() == encodedDelta.size(),
		"queue-full publication is atomic and configured limits cannot exceed codec or bus bounds");

	std::vector<RecordedSimulationCommand> recorded{
		RecordedSimulationCommand{
			17, 41, CommandJournalStatus::Applied,
			SimulationCommand{BeginFireWeaponCommand{
				firstIncarnation, -123, -1, 4, SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			18, 42, CommandJournalStatus::Blocked,
			SimulationCommand{EndTurnCommand{2, SimulationCommandSource::NetworkPeer}}}};
	std::vector<std::uint8_t> encoded;
	check(EncodeSimulationCommandJournal(recorded, 3, encoded),
		"JA2 adapter encodes versioned simulation command journals");
	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	const SimulationCommandJournalDecodeResult decodeResult =
		DecodeSimulationCommandJournal(encoded, decoded, dropped);
	bool decodedFields = false;
	if (decodeResult == SimulationCommandJournalDecodeResult::Success && decoded.size() == 2)
	{
		const auto& fire = std::get<BeginFireWeaponCommand>(decoded[0].command);
		const auto& turn = std::get<EndTurnCommand>(decoded[1].command);
		decodedFields = dropped == 3 && decoded[0].tick == 17 &&
			decoded[0].sequence == 41 &&
			decoded[0].status == CommandJournalStatus::Applied &&
			fire.soldier == firstIncarnation &&
			fire.targetGrid == -123 && fire.targetLevel == -1 &&
			fire.targetCubeLevel == 4 &&
			fire.source == SimulationCommandSource::LocalPlayer &&
			decoded[1].status == CommandJournalStatus::Blocked && turn.nextTeam == 2 &&
			turn.source == SimulationCommandSource::NetworkPeer;
	}
	check(decodedFields,
		"JA2 command codec round-trips explicit tags and signed tactical values");
	encoded.push_back(0xff);
	check(DecodeSimulationCommandJournal(encoded, decoded, dropped) ==
		SimulationCommandJournalDecodeResult::Invalid,
		"JA2 command codec rejects trailing ambiguous data");

	CommandJournal<SimulationCommand> journal(1);
	journal.recordSubmission(
		1, 10, SimulationCommand{EndTurnCommand{1, SimulationCommandSource::System}});
	journal.recordSubmission(
		2, 11, SimulationCommand{ChangeStanceCommand{
			3, 2, SimulationCommandSource::LocalPlayer}});
	journal.recordDisposition(11, CommandDisposition::Applied);
	const auto bounded = journal.snapshot();
	check(bounded.size() == 1 && bounded[0].sequence == 11 &&
		bounded[0].status == CommandJournalStatus::Applied &&
		journal.droppedCount() == 1,
		"JA2 adapter uses the bounded generic command journal");

	MemoryByteStorage replayStorage;
	EngineServices replayServices{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), replayStorage};
	EngineRuntime<> captureRuntime(replayServices);
	captureRuntime.submitCommand(
		12, SimulationCommand{ChangeStanceCommand{
			5, 1, SimulationCommandSource::LocalPlayer}});
	captureRuntime.submitCommand(
		11, SimulationCommand{EndTurnCommand{2, SimulationCommandSource::NetworkPeer}});
	check(captureRuntime.saveCommandReplay("capture.replay") ==
		CommandReplaySaveResult::Success,
		"JA2 runtime persists its bounded command journal as a durable replay");
	SimulationCommandReplay replay;
	EngineRuntime<> playbackRuntime(replayServices);
	check(playbackRuntime.loadCommandReplay("capture.replay", replay) ==
		CommandReplayLoadResult::Success && replay.records.size() == 2 &&
		replay.droppedCount == 0,
		"JA2 runtime loads a complete integrity-checked replay capture");
	check(playbackRuntime.stageCommandReplay(replay) ==
		CommandReplayStageResult::Success,
		"JA2 runtime transactionally stages a complete replay");
	const auto replayed = playbackRuntime.commands().drainThrough(12);
	check(replayed.size() == 2 && replayed[0].tick == 11 &&
		replayed[0].sequence == 1 && replayed[1].tick == 12 &&
		replayed[1].sequence == 0 &&
		std::get<EndTurnCommand>(replayed[0].command).nextTeam == 2 &&
		std::get<ChangeStanceCommand>(replayed[1].command).soldierId == 5,
		"staged replay retains deterministic tick and sequence order");
	check(playbackRuntime.stageCommandReplay(replay) ==
		CommandReplayStageResult::SequenceConflict &&
		playbackRuntime.commands().empty(),
		"replay sequence conflicts reject the whole batch without partial queuing");
	SimulationCommandReplay incomplete = replay;
	incomplete.droppedCount = 1;
	check(playbackRuntime.stageCommandReplay(incomplete) ==
		CommandReplayStageResult::IncompleteCapture,
		"JA2 runtime refuses playback of a truncated bounded journal");
	std::vector<std::uint8_t> corruptReplayBytes;
	replayStorage.readAll("capture.replay", corruptReplayBytes);
	corruptReplayBytes.back() ^= 0x80u;
	replayStorage.writeAll("corrupt.replay", corruptReplayBytes);
	SimulationCommandReplay unchangedReplay;
	unchangedReplay.droppedCount = 77;
	check(playbackRuntime.loadCommandReplay("corrupt.replay", unchangedReplay) ==
		CommandReplayLoadResult::IntegrityFailure &&
		unchangedReplay.droppedCount == 77 && unchangedReplay.records.empty(),
		"corrupt replay loads leave the caller's capture untouched");

	return failures == 0 ? 0 : 1;
}
