#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandService.h>
#include <Engine/Adapters/JA2/TacticalCommandResultCodec.h>
#include <Engine/Adapters/JA2/TacticalCommandResultPublisher.h>
#include <Engine/Adapters/JA2/TacticalEntity.h>
#include <Engine/Adapters/JA2/TacticalWorldDelta.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaCodec.h>
#include <Engine/Adapters/JA2/TacticalWorldDeltaPublisher.h>
#include <Engine/Adapters/JA2/TacticalWorldObserver.h>
#include <Engine/Adapters/JA2/TacticalWorldService.h>
#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <type_traits>
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

class RecordingTacticalCommandCancellationSink final
	: public TacticalCommandCancellationSink
{
public:
	void commandCancelled(
		const TacticalCommandRequest& request) noexcept override
	{
		if (count < requestIds.size()) requestIds[count++] = request.requestId;
	}

	std::array<std::uint64_t, 4> requestIds{};
	std::size_t count = 0;
};

class ReentrantTacticalCommandCancellationSink final
	: public TacticalCommandCancellationSink
{
public:
	explicit ReentrantTacticalCommandCancellationSink(TacticalCommandInbox& inbox)
		: inbox_(inbox) {}

	void commandCancelled(
		const TacticalCommandRequest& request) noexcept override
	{
		if (count < requestIds.size()) requestIds[count++] = request.requestId;
		if (reentered) return;
		reentered = true;
		submitted = inbox_.submit(
			"pkg.alpha", SimulationCommand{EndTurnCommand{
				4, SimulationCommandSource::System}});
		nestedCancellation = inbox_.cancelPackage("pkg.beta", this);
		nestedDrain = inbox_.drain([](const TacticalCommandRequest&) {
			return TacticalCommandDisposition::Accept;
		});
	}

	TacticalCommandInbox& inbox_;
	std::array<std::uint64_t, 4> requestIds{};
	std::size_t count = 0;
	bool reentered = false;
	TacticalCommandSubmissionResult submitted;
	TacticalCommandCancellationResult nestedCancellation;
	TacticalCommandDrainResult nestedDrain;
};

class ControlledTacticalWorldService final : public TacticalWorldService
{
public:
	void publish(TacticalWorldSnapshot snapshot)
	{
		snapshot_ = std::move(snapshot);
		result_ = TacticalWorldCaptureResult::Success;
	}

	void fail(TacticalWorldCaptureResult result)
	{
		result_ = result;
	}

	TacticalWorldCaptureResult capture(TacticalWorldSnapshot& output) noexcept override
	{
		if (result_ != TacticalWorldCaptureResult::Success) return result_;
		try
		{
			TacticalWorldSnapshot captured = snapshot_;
			output = std::move(captured);
			return TacticalWorldCaptureResult::Success;
		}
		catch (...)
		{
			return TacticalWorldCaptureResult::AllocationFailure;
		}
	}

private:
	TacticalWorldSnapshot snapshot_;
	TacticalWorldCaptureResult result_ = TacticalWorldCaptureResult::Unavailable;
};

class BoundCommandPackage final : public EnginePackage
{
public:
	BoundCommandPackage()
		: descriptor_{
			ContentManifest{
				"fixture.bound-commands", "1", CurrentContentApiVersion},
			PackageKind::Extension}
	{
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		active_ = true;
		return true;
	}
	void deactivate() noexcept override { active_ = false; }
	bool bootstrap(
		PackageBootstrapContext& context, PackageBootstrapPhase phase) override
	{
		if (phase == PackageBootstrapPhase::Configure)
			binding = BindTacticalCommandClient(
				context.extensionServices, context.identity);
		return phase != PackageBootstrapPhase::Configure || binding;
	}

	TacticalCommandClientBindingResult binding;

private:
	PackageDescriptor descriptor_;
	bool active_ = false;
};

bool RejectsJournalWithoutPublishing(
	const std::vector<std::uint8_t>& bytes,
	SimulationCommandJournalDecodeResult expected)
{
	std::vector<RecordedSimulationCommand> records{
		RecordedSimulationCommand{
			99, 88, CommandJournalStatus::Blocked,
			SimulationCommand{EndTurnCommand{
				4, SimulationCommandSource::System}}}};
	std::uint64_t dropped = 77;
	const SimulationCommandJournalDecodeResult result =
		DecodeSimulationCommandJournal(bytes, records, dropped);
	return result == expected && dropped == 77 && records.size() == 1 &&
		records[0].tick == 99 && records[0].sequence == 88 &&
		records[0].status == CommandJournalStatus::Blocked &&
		std::holds_alternative<EndTurnCommand>(records[0].command) &&
		std::get<EndTurnCommand>(records[0].command).nextTeam == 4;
}

SimulationCommand MakeTurnCommand(
	std::uint8_t nextTeam,
	SimulationCommandSource source = SimulationCommandSource::System)
{
	return SimulationCommand{EndTurnCommand{nextTeam, source}};
}
}

int main()
{
	EngineRuntime<> legacyBraceRuntime({});
	check(legacyBraceRuntime.serviceCatalog().size() == 14 &&
		legacyBraceRuntime.runtimeMessages().maxQueuedMessages() == 1024,
		"empty-brace runtime construction retains default EngineServices semantics");

	constexpr TacticalEntityId invalidEntity;
	constexpr TacticalEntityId firstIncarnation{7, 9001};
	constexpr TacticalEntityId reusedSlot{7, 9002};
	static_assert(!invalidEntity.valid(), "default tactical identity must be invalid");
	static_assert(firstIncarnation.valid(), "slot and incarnation form a valid identity");
	static_assert(firstIncarnation != reusedSlot,
		"slot reuse must not preserve tactical identity");
	check(firstIncarnation < reusedSlot,
		"tactical identities have deterministic slot and incarnation ordering");

	TacticalCommandInbox defaultCommandInbox;
	const TacticalCommandInboxLimits defaultCommandLimits =
		defaultCommandInbox.limits();
	check(defaultCommandLimits.maximumPending > 0 &&
		defaultCommandLimits.maximumPerDrain > 0 &&
		defaultCommandLimits.maximumDiagnosticEntries > 0 &&
		defaultCommandLimits.maximumOwnerBytes > 0 &&
		defaultCommandLimits.maximumRequestId > 0,
		"tactical command inbox defaults are finite and usable");

	TacticalCommandInbox registeredCommandInbox(
		TacticalCommandInboxLimits{8, 3, 2, 32, 100});
	ServiceCatalog commandServices;
	check(RegisterTacticalCommandService(
			commandServices, registeredCommandInbox) ==
			EngineServiceRegistrationError::None,
		"tactical command ingress registers as a versioned package service");
	const auto tacticalCommands = commandServices.resolve(TacticalCommandServiceContract);
	const auto futureTacticalCommands =
		commandServices.resolve<TacticalCommandService>(
			TacticalCommandServiceId, EngineServiceVersion{1, 1});
	const auto wrongTacticalCommandType =
		commandServices.resolve<TacticalWorldService>(
			TacticalCommandServiceId, TacticalCommandServiceVersion);
	check(tacticalCommands && tacticalCommands.service == &registeredCommandInbox &&
		tacticalCommands.availableVersion.major == 1 &&
		futureTacticalCommands.error ==
			EngineServiceLookupError::IncompatibleVersion &&
		wrongTacticalCommandType.error == EngineServiceLookupError::TypeMismatch,
		"tactical command lookup enforces service version and concrete type");
	check(RegisterTacticalCommandService(
			commandServices, registeredCommandInbox) ==
			EngineServiceRegistrationError::DuplicateId,
		"tactical command service IDs cannot be registered twice");
	check(BindTacticalCommandClient(commandServices, PackageIdentity{}).error ==
			TacticalCommandClientBindingError::InvalidIdentity,
		"tactical command clients require a registry-issued package identity");

	EngineRuntime<unsigned> boundRuntime;
	TacticalCommandInbox boundCommandInbox(
		TacticalCommandInboxLimits{4, 4, 4, 64, 10});
	BoundCommandPackage boundPackage;
	const bool boundPackageStarted =
		RegisterTacticalCommandService(
			boundRuntime.serviceCatalog(), boundCommandInbox) ==
				EngineServiceRegistrationError::None &&
		boundRuntime.packages().registerPackage(boundPackage) ==
			PackageRegistrationError::None &&
		boundRuntime.packages().activate("fixture.bound-commands") ==
			PackageActivationError::None &&
		boundRuntime.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::StartRuntime);
	const TacticalCommandSubmissionResult boundSubmission = boundPackage.binding
		? boundPackage.binding.client.submit(MakeTurnCommand(1))
		: TacticalCommandSubmissionResult{
			TacticalCommandSubmissionError::InvalidOwner, 0};
	TacticalCommandInboxSnapshot boundSnapshot;
	check(boundPackageStarted && boundPackage.binding &&
		boundPackage.binding.client.packageId() == "fixture.bound-commands" &&
		boundSubmission &&
		boundCommandInbox.snapshot(boundSnapshot) == TacticalCommandSnapshotError::None &&
		boundSnapshot.pending.size() == 1 &&
		boundSnapshot.pending[0].packageId == "fixture.bound-commands",
		"registry-issued tactical command clients bind ownership without caller strings");
	boundRuntime.packageLifecycle().shutdown();

	const TacticalCommandResult appliedResult{
		"p", 1, 2, 3, TacticalCommandTerminalStatus::Applied,
		TacticalCommandTerminalReason::None};
	std::vector<std::uint8_t> encodedResult;
	const std::vector<std::uint8_t> expectedResultBytes{
		0x54, 0x43, 0x52, 0x31, 0x01, 0x00,
		0x01, 0x00, 0x00, 0x00, 0x70,
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x02, 0x00};
	TacticalCommandResult decodedResult;
	check(EncodeTacticalCommandResult(appliedResult, encodedResult) ==
			TacticalCommandResultEncodeError::None &&
		encodedResult == expectedResultBytes &&
		DecodeTacticalCommandResult(encodedResult, decodedResult) ==
			TacticalCommandResultDecodeError::None &&
		decodedResult.packageId == "p" && decodedResult.requestId == 1 &&
		decodedResult.authoritativeSequence == 2 &&
		decodedResult.simulationTick == 3 &&
		decodedResult.status == TacticalCommandTerminalStatus::Applied &&
		decodedResult.reason == TacticalCommandTerminalReason::None,
		"tactical command result version 1 has a deterministic round-trip wire format");
	bool everyTruncatedResultRejected = true;
	for (std::size_t size = 0; size < encodedResult.size(); ++size)
	{
		std::vector<std::uint8_t> truncated(encodedResult.begin(),
			encodedResult.begin() + size);
		TacticalCommandResult retained{
			"retained", 9, 0, 7, TacticalCommandTerminalStatus::Rejected,
			TacticalCommandTerminalReason::InvalidDomain};
		if (DecodeTacticalCommandResult(truncated, retained) ==
				TacticalCommandResultDecodeError::None ||
			retained.packageId != "retained" || retained.requestId != 9)
		{
			everyTruncatedResultRejected = false;
			break;
		}
	}
	check(everyTruncatedResultRejected,
		"tactical command result decoding rejects every truncated prefix transactionally");
	std::vector<std::uint8_t> futureResult = encodedResult;
	futureResult[4] = 2;
	std::vector<std::uint8_t> invalidResultBytes = encodedResult;
	invalidResultBytes.back() = 6;
	std::vector<std::uint8_t> zeroSequenceBytes;
	TacticalCommandResult zeroSequenceResult;
	check(DecodeTacticalCommandResult(futureResult, decodedResult) ==
			TacticalCommandResultDecodeError::UnsupportedVersion &&
		DecodeTacticalCommandResult(invalidResultBytes, decodedResult) ==
			TacticalCommandResultDecodeError::Invalid &&
		EncodeTacticalCommandResult(TacticalCommandResult{
			"p", 1, 0, 3, TacticalCommandTerminalStatus::Applied,
			TacticalCommandTerminalReason::None}, zeroSequenceBytes) ==
			TacticalCommandResultEncodeError::None &&
		DecodeTacticalCommandResult(zeroSequenceBytes, zeroSequenceResult) ==
			TacticalCommandResultDecodeError::None &&
		zeroSequenceResult.authoritativeSequence == 0 &&
		EncodeTacticalCommandResult(TacticalCommandResult{
			"p", 1, 2, 3, TacticalCommandTerminalStatus::Applied,
			TacticalCommandTerminalReason::InvalidDomain}, zeroSequenceBytes) ==
			TacticalCommandResultEncodeError::Invalid,
		"tactical command results accept the first stream sequence and reject inconsistent records");
	RuntimeMessageBus resultMessages(1, 1024);
	RecordingRuntimeMessageSink resultSink;
	resultMessages.addSink(resultSink);
	resultMessages.publish(RuntimeMessageRequest{
		"fixture.blocker", "fixture.host", {9}});
	TacticalCommandResultPublisher resultPublisher(resultMessages);
	PreparedTacticalCommandResultMessage preparedResult;
	const TacticalCommandResultPublishError resultPrepared =
		resultPublisher.prepare(appliedResult, preparedResult);
	const std::vector<std::uint8_t> retainedResultPayload =
		preparedResult.request.payload;
	const TacticalCommandResultPublishResult resultPressure =
		resultPublisher.publishPrepared(preparedResult);
	const bool resultRetained =
		preparedResult.request.payload == retainedResultPayload &&
		preparedResult.request.topic == TacticalCommandResultMessageTopic &&
		preparedResult.request.source == TacticalCommandResultMessageSource;
	resultMessages.dispatchPending();
	const TacticalCommandResultPublishResult resultPublished =
		resultPublisher.publishPrepared(preparedResult);
	resultMessages.dispatchPending();
	TacticalCommandResult deliveredResult;
	check(resultPrepared == TacticalCommandResultPublishError::None &&
		resultPressure.error == TacticalCommandResultPublishError::QueueFull &&
		resultRetained && resultPublished && resultPublished.sequence == 2 &&
		preparedResult.request.payload.empty() && resultSink.messages.size() == 2 &&
		resultSink.messages[1].topic == TacticalCommandResultMessageTopic &&
		resultSink.messages[1].source == TacticalCommandResultMessageSource &&
		DecodeTacticalCommandResult(
			resultSink.messages[1].payload, deliveredResult) ==
				TacticalCommandResultDecodeError::None &&
		deliveredResult.requestId == appliedResult.requestId &&
		deliveredResult.status == TacticalCommandTerminalStatus::Applied,
		"prepared tactical command results encode once and retain ownership across bus pressure");
	PreparedTacticalCommandResultMessage retainedPrepared;
	retainedPrepared.request.payload = {7};
	retainedPrepared.payloadBytes = 1;
	TacticalCommandResultPublisher tinyResultPublisher(resultMessages, 1);
	check(tinyResultPublisher.prepare(appliedResult, retainedPrepared) ==
			TacticalCommandResultPublishError::PayloadTooLarge &&
		retainedPrepared.request.payload == std::vector<std::uint8_t>({7}),
		"tactical command result preparation enforces payload limits transactionally");

	TacticalCommandInbox validationInbox(
		TacticalCommandInboxLimits{8, 8, 8, 8, 10});
	const SimulationCommand validTurn = MakeTurnCommand(1);
	const SimulationCommand invalidSource = MakeTurnCommand(
		1, static_cast<SimulationCommandSource>(0xff));
	const SimulationCommand unresolvedStance{ChangeStanceCommand{
		TacticalEntityId{3, 0}, 2, SimulationCommandSource::Replay}};
	const SimulationCommand invalidFireCommand{BeginFireWeaponCommand{
		TacticalEntityId{}, 100, 0, 0, SimulationCommandSource::LocalPlayer}};
	const SimulationCommand invalidMoveCommand{MoveToGridCommand{
		TacticalEntityId{}, 100, 0, false, false,
		SimulationCommandSource::LocalPlayer}};
	MoveToGridCommand invalidMoveOrigin{
		TacticalEntityId{3, 301}, 100, 6, false, false,
		SimulationCommandSource::LocalPlayer};
	invalidMoveOrigin.origin = static_cast<TacticalMoveOrigin>(0xff);
	MoveToGridCommand invalidPendingAction{
		TacticalEntityId{3, 301}, 100, 6, false, false,
		SimulationCommandSource::LocalPlayer};
	invalidPendingAction.pendingAction =
		static_cast<TacticalPendingActionPolicy>(0xff);
	const TacticalCommandSubmissionResult invalidOwner =
		validationInbox.submit("bad/owner", validTurn);
	const TacticalCommandSubmissionResult oversizedOwner =
		validationInbox.submit(std::string(9, 'a'), validTurn);
	const TacticalCommandSubmissionResult invalidSourceResult =
		validationInbox.submit("pkg.ok", invalidSource);
	const TacticalCommandSubmissionResult unresolvedStanceResult =
		validationInbox.submit("pkg.ok", unresolvedStance);
	const TacticalCommandSubmissionResult invalidFireResult =
		validationInbox.submit("pkg.ok", invalidFireCommand);
	const TacticalCommandSubmissionResult invalidMoveResult =
		validationInbox.submit("pkg.ok", invalidMoveCommand);
	const TacticalCommandSubmissionResult invalidMoveOriginResult =
		validationInbox.submit("pkg.ok", SimulationCommand{invalidMoveOrigin});
	const TacticalCommandSubmissionResult invalidPendingActionResult =
		validationInbox.submit("pkg.ok", SimulationCommand{invalidPendingAction});
	const TacticalCommandSubmissionResult invalidFacingResult =
		validationInbox.submit("pkg.ok", SimulationCommand{SetFacingCommand{
			TacticalEntityId{3, 301}, TacticalDirectionCount,
			SimulationCommandSource::LocalPlayer}});
	const TacticalCommandSubmissionResult validStanceResult =
		validationInbox.submit("pkg.ok", SimulationCommand{ChangeStanceCommand{
			TacticalEntityId{3, 301}, 2, SimulationCommandSource::Replay}});
	const TacticalCommandSubmissionResult validFireResult =
		validationInbox.submit("pkg.ok", SimulationCommand{BeginFireWeaponCommand{
			TacticalEntityId{3, 301}, 100, 0, 0,
			SimulationCommandSource::LocalPlayer}});
	const TacticalCommandSubmissionResult validMoveResult =
		validationInbox.submit("pkg.ok", SimulationCommand{MoveToGridCommand{
			TacticalEntityId{3, 301}, 101, 6, true, false,
			SimulationCommandSource::LocalPlayer}});
	check(invalidOwner.error == TacticalCommandSubmissionError::InvalidOwner &&
		oversizedOwner.error == TacticalCommandSubmissionError::InvalidOwner &&
		invalidSourceResult.error == TacticalCommandSubmissionError::InvalidCommand &&
		unresolvedStanceResult.error == TacticalCommandSubmissionError::InvalidCommand &&
		invalidFireResult.error == TacticalCommandSubmissionError::InvalidCommand &&
		invalidMoveResult.error == TacticalCommandSubmissionError::InvalidCommand &&
		invalidMoveOriginResult.error == TacticalCommandSubmissionError::InvalidCommand &&
		invalidPendingActionResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidFacingResult.error == TacticalCommandSubmissionError::InvalidCommand &&
		invalidOwner.requestId == 0 && invalidSourceResult.requestId == 0 &&
		validStanceResult.requestId == 1 && validFireResult.requestId == 2 &&
		validMoveResult.requestId == 3 && validationInbox.summary().submitted == 3 &&
		validationInbox.summary().nextRequestId == 4,
		"package command validation rejects malformed ownership and unresolved actors without consuming IDs");

	TacticalCommandInbox capacityInbox(
		TacticalCommandInboxLimits{2, 1, 2, 32, 10});
	const auto capacityFirst = capacityInbox.submit("pkg.a", MakeTurnCommand(1));
	const auto capacitySecond = capacityInbox.submit("pkg.b", MakeTurnCommand(2));
	const auto capacityFailure = capacityInbox.submit("pkg.c", MakeTurnCommand(3));
	const TacticalCommandDrainResult capacityRelease = capacityInbox.drain(
		[](const TacticalCommandRequest&) { return TacticalCommandDisposition::Accept; });
	const auto capacityThird = capacityInbox.submit("pkg.c", MakeTurnCommand(3));
	check(capacityFirst.requestId == 1 && capacitySecond.requestId == 2 &&
		capacityFailure.error == TacticalCommandSubmissionError::CapacityReached &&
		capacityFailure.requestId == 0 && capacityRelease.accepted == 1 &&
		capacityThird.requestId == 3 && capacityInbox.size() == 2,
		"capacity failures are transactional and request IDs remain monotonic");

	TacticalCommandInbox shortLivedInbox(
		TacticalCommandInboxLimits{1, 1, 1, 32, 2});
	const auto lifetimeFirst = shortLivedInbox.submit("pkg.life", validTurn);
	shortLivedInbox.drain(
		[](const TacticalCommandRequest&) { return TacticalCommandDisposition::Accept; });
	const auto lifetimeSecond = shortLivedInbox.submit("pkg.life", validTurn);
	shortLivedInbox.drain(
		[](const TacticalCommandRequest&) { return TacticalCommandDisposition::Accept; });
	const TacticalCommandInboxSummary exhaustedBefore = shortLivedInbox.summary();
	const auto lifetimeExhausted = shortLivedInbox.submit("pkg.life", validTurn);
	const TacticalCommandInboxSummary exhaustedAfter = shortLivedInbox.summary();
	check(lifetimeFirst.requestId == 1 && lifetimeSecond.requestId == 2 &&
		lifetimeExhausted.error ==
			TacticalCommandSubmissionError::SequenceExhausted &&
		lifetimeExhausted.requestId == 0 && exhaustedBefore.sequenceExhausted &&
		exhaustedBefore.nextRequestId == 0 &&
		exhaustedAfter.submitted == exhaustedBefore.submitted &&
		exhaustedAfter.pending == exhaustedBefore.pending,
		"configured request lifetimes report sequence exhaustion transactionally");

	TacticalCommandInbox flowInbox(
		TacticalCommandInboxLimits{8, 2, 2, 32, 20});
	const auto flowFirst = flowInbox.submit("pkg.a", MakeTurnCommand(1));
	const auto flowSecond = flowInbox.submit("pkg.b", MakeTurnCommand(2));
	const auto flowThird = flowInbox.submit("pkg.a", MakeTurnCommand(3));
	TacticalCommandInboxSnapshot boundedCommands;
	check(flowInbox.snapshot(boundedCommands) == TacticalCommandSnapshotError::None &&
		boundedCommands.pending.size() == 2 && boundedCommands.omitted == 1 &&
		boundedCommands.pending[0].requestId == flowFirst.requestId &&
		boundedCommands.pending[1].requestId == flowSecond.requestId &&
		boundedCommands.summary.pending == 3 &&
		boundedCommands.limits.maximumDiagnosticEntries == 2,
		"command diagnostics publish only their configured oldest bounded prefix");
	std::vector<std::uint64_t> flowOrder;
	TacticalCommandSubmissionResult callbackSubmission;
	const TacticalCommandDrainResult firstFlowDrain = flowInbox.drain(
		[&](auto& request) {
			static_assert(std::is_const<typename std::remove_reference<
				decltype(request)>::type>::value,
				"host handlers must receive an immutable request");
			flowOrder.push_back(request.requestId);
			if (request.requestId == flowFirst.requestId)
				callbackSubmission =
					flowInbox.submit("pkg.callback", MakeTurnCommand(4));
			return TacticalCommandDisposition::Accept;
		});
	check(firstFlowDrain.initialPending == 3 && firstFlowDrain.eligible == 2 &&
		firstFlowDrain.attempted == 2 && firstFlowDrain.accepted == 2 &&
		callbackSubmission.requestId == 4 && flowOrder.size() == 2 &&
		flowOrder[0] == flowFirst.requestId && flowOrder[1] == flowSecond.requestId &&
		firstFlowDrain.queuedForNextDrain == 2,
		"draining preserves FIFO, enforces its cap, and defers callback submissions");
	const TacticalCommandDrainResult secondFlowDrain = flowInbox.drain(
		[&](const TacticalCommandRequest& request) {
			flowOrder.push_back(request.requestId);
			return TacticalCommandDisposition::Accept;
		});
	check(secondFlowDrain.accepted == 2 && flowOrder.size() == 4 &&
		flowOrder[2] == flowThird.requestId &&
		flowOrder[3] == callbackSubmission.requestId && flowInbox.empty(),
		"callback-enqueued commands become eligible on the following drain only");

	TacticalCommandInbox saturatedCallbackInbox(
		TacticalCommandInboxLimits{1, 1, 1, 32, 4});
	const auto saturatedFront =
		saturatedCallbackInbox.submit("pkg.front", MakeTurnCommand(1));
	TacticalCommandSubmissionResult saturatedCallbackSubmission;
	const TacticalCommandDrainResult saturatedCallbackDrain =
		saturatedCallbackInbox.drain(
			[&](const TacticalCommandRequest&) {
				saturatedCallbackSubmission = saturatedCallbackInbox.submit(
					"pkg.later", MakeTurnCommand(2));
				return TacticalCommandDisposition::Defer;
			});
	TacticalCommandInboxSnapshot saturatedCallbackSnapshot;
	saturatedCallbackInbox.snapshot(saturatedCallbackSnapshot);
	check(saturatedCallbackSubmission.error ==
			TacticalCommandSubmissionError::CapacityReached &&
		saturatedCallbackSubmission.requestId == 0 &&
		saturatedCallbackDrain.deferred == 1 &&
		saturatedCallbackSnapshot.pending.size() == 1 &&
		saturatedCallbackSnapshot.pending[0].requestId == saturatedFront.requestId &&
		saturatedCallbackSnapshot.summary.nextRequestId == 2,
		"the retained in-flight front counts toward capacity during callbacks");
	saturatedCallbackInbox.cancelPackage("pkg.front");

	TacticalCommandInbox retryInbox(
		TacticalCommandInboxLimits{5, 5, 5, 32, 20});
	const auto retryFirst = retryInbox.submit("pkg.retry", MakeTurnCommand(1));
	const auto retrySecond = retryInbox.submit("pkg.retry", MakeTurnCommand(2));
	std::uint64_t deferredFront = 0;
	const TacticalCommandDrainResult explicitDeferral = retryInbox.drain(
		[&](const TacticalCommandRequest& request) {
			deferredFront = request.requestId;
			return TacticalCommandDisposition::Defer;
		});
	std::uint64_t failedFront = 0;
	TacticalCommandSubmissionResult submissionBeforeThrow;
	const TacticalCommandDrainResult failedCallback = retryInbox.drain(
		[&](const TacticalCommandRequest& request) -> TacticalCommandDisposition {
			failedFront = request.requestId;
			submissionBeforeThrow =
				retryInbox.submit("pkg.later", MakeTurnCommand(3));
			throw 7;
		});
	TacticalCommandInboxSnapshot retainedAfterFailure;
	retryInbox.snapshot(retainedAfterFailure);
	std::vector<std::uint64_t> retryOrder;
	const TacticalCommandDrainResult completedRetry = retryInbox.drain(
		[&](const TacticalCommandRequest& request) {
			retryOrder.push_back(request.requestId);
			return request.requestId == retryFirst.requestId
				? TacticalCommandDisposition::Reject
				: TacticalCommandDisposition::Accept;
		});
	const TacticalCommandInboxSummary retrySummary = retryInbox.summary();
	check(explicitDeferral.attempted == 1 && explicitDeferral.deferred == 1 &&
		deferredFront == retryFirst.requestId && failedCallback.attempted == 1 &&
		failedCallback.callbackFailures == 1 && failedCallback.deferred == 1 &&
		failedFront == retryFirst.requestId && submissionBeforeThrow.requestId == 3 &&
		retainedAfterFailure.pending.size() == 3 &&
		retainedAfterFailure.pending[0].requestId == retryFirst.requestId &&
		retainedAfterFailure.pending[1].requestId == retrySecond.requestId &&
		retainedAfterFailure.pending[2].requestId == submissionBeforeThrow.requestId &&
		completedRetry.rejected == 1 && completedRetry.accepted == 2 &&
		retryOrder.size() == 3 && retrySummary.deferred == 2 &&
		retrySummary.callbackFailures == 1 && retryInbox.empty(),
		"defer and exceptions retain the FIFO front while rejection removes it safely");

	TacticalCommandInbox guardedInbox(
		TacticalCommandInboxLimits{3, 3, 3, 32, 10});
	guardedInbox.submit("pkg.guard", MakeTurnCommand(1));
	TacticalCommandDrainResult nestedDrain;
	TacticalCommandCancellationResult cancellationDuringDrain;
	bool observedDrainingSummary = false;
	const TacticalCommandDrainResult guardedDrain = guardedInbox.drain(
		[&](const TacticalCommandRequest&) {
			nestedDrain = guardedInbox.drain(
				[](const TacticalCommandRequest&) {
					return TacticalCommandDisposition::Accept;
				});
			cancellationDuringDrain = guardedInbox.cancelPackage("pkg.guard");
			observedDrainingSummary = guardedInbox.summary().draining;
			return TacticalCommandDisposition::Defer;
		});
	const TacticalCommandCancellationResult invalidCancellation =
		guardedInbox.cancelPackage("bad/owner");
	const TacticalCommandCancellationResult completedCancellation =
		guardedInbox.cancelPackage("pkg.guard");
	check(guardedDrain.deferred == 1 &&
		nestedDrain.error == TacticalCommandDrainError::AlreadyDraining &&
		nestedDrain.attempted == 0 &&
		cancellationDuringDrain.error ==
			TacticalCommandCancellationError::DrainInProgress &&
		cancellationDuringDrain.cancelled == 0 && observedDrainingSummary &&
		!guardedInbox.summary().draining &&
		invalidCancellation.error == TacticalCommandCancellationError::InvalidOwner &&
		completedCancellation.cancelled == 1 && guardedInbox.empty(),
		"nested drains and cancellation during callbacks fail without invalidating the front");

	TacticalCommandInbox malformedDispositionInbox(
		TacticalCommandInboxLimits{1, 1, 1, 32, 2});
	const auto malformedDispositionRequest =
		malformedDispositionInbox.submit("pkg.malformed", validTurn);
	const TacticalCommandDrainResult malformedDispositionDrain =
		malformedDispositionInbox.drain(
			[](const TacticalCommandRequest&) {
				return static_cast<TacticalCommandDisposition>(0xff);
			});
	TacticalCommandInboxSnapshot malformedDispositionSnapshot;
	malformedDispositionInbox.snapshot(malformedDispositionSnapshot);
	check(malformedDispositionDrain.attempted == 1 &&
		malformedDispositionDrain.callbackFailures == 1 &&
		malformedDispositionDrain.deferred == 1 &&
		malformedDispositionSnapshot.pending.size() == 1 &&
		malformedDispositionSnapshot.pending[0].requestId ==
			malformedDispositionRequest.requestId,
		"unknown handler dispositions fail closed and retain the front request");
	malformedDispositionInbox.cancelPackage("pkg.malformed");

	TacticalCommandInbox cancellationInbox(
		TacticalCommandInboxLimits{5, 5, 5, 32, 20});
	const auto cancelFirst = cancellationInbox.submit("pkg.alpha", MakeTurnCommand(1));
	const auto cancelSecond = cancellationInbox.submit("pkg.beta", MakeTurnCommand(2));
	cancellationInbox.submit("pkg.alpha", MakeTurnCommand(3));
	const auto cancelFourth = cancellationInbox.submit("pkg.beta", MakeTurnCommand(4));
	RecordingTacticalCommandCancellationSink cancellationSink;
	const TacticalCommandCancellationResult cancelledAlpha =
		cancellationInbox.cancelPackage("pkg.alpha", &cancellationSink);
	TacticalCommandInboxSnapshot cancellationSnapshot;
	cancellationInbox.snapshot(cancellationSnapshot);
	std::vector<std::uint64_t> survivorOrder;
	cancellationInbox.drain([&](const TacticalCommandRequest& request) {
		survivorOrder.push_back(request.requestId);
		return TacticalCommandDisposition::Accept;
	});
	check(cancelFirst.requestId == 1 && cancelledAlpha.cancelled == 2 &&
		cancellationSink.count == 2 && cancellationSink.requestIds[0] == 1 &&
		cancellationSink.requestIds[1] == 3 &&
		cancellationSnapshot.pending.size() == 2 &&
		cancellationSnapshot.pending[0].requestId == cancelSecond.requestId &&
		cancellationSnapshot.pending[1].requestId == cancelFourth.requestId &&
		survivorOrder.size() == 2 && survivorOrder[0] == cancelSecond.requestId &&
		survivorOrder[1] == cancelFourth.requestId &&
		cancellationInbox.summary().cancelled == 2,
		"package cancellation removes only owned work and preserves survivor FIFO");

	TacticalCommandInbox reentrantCancellationInbox(
		TacticalCommandInboxLimits{6, 6, 6, 32, 20});
	const auto reentrantAlphaFirst = reentrantCancellationInbox.submit(
		"pkg.alpha", MakeTurnCommand(1));
	const auto reentrantBeta = reentrantCancellationInbox.submit(
		"pkg.beta", MakeTurnCommand(2));
	const auto reentrantAlphaSecond = reentrantCancellationInbox.submit(
		"pkg.alpha", MakeTurnCommand(3));
	ReentrantTacticalCommandCancellationSink reentrantCancellationSink(
		reentrantCancellationInbox);
	const TacticalCommandCancellationResult reentrantCancelled =
		reentrantCancellationInbox.cancelPackage(
			"pkg.alpha", &reentrantCancellationSink);
	TacticalCommandInboxSnapshot reentrantCancellationSnapshot;
	reentrantCancellationInbox.snapshot(reentrantCancellationSnapshot);
	const TacticalCommandCancellationResult submittedDuringCancellation =
		reentrantCancellationInbox.cancelPackage("pkg.alpha");
	check(reentrantAlphaFirst && reentrantBeta && reentrantAlphaSecond &&
		reentrantCancelled.cancelled == 2 &&
		reentrantCancellationSink.count == 2 &&
		reentrantCancellationSink.requestIds[0] == reentrantAlphaFirst.requestId &&
		reentrantCancellationSink.requestIds[1] == reentrantAlphaSecond.requestId &&
		reentrantCancellationSink.submitted.requestId == 4 &&
		reentrantCancellationSink.nestedCancellation.error ==
			TacticalCommandCancellationError::CancellationInProgress &&
		reentrantCancellationSink.nestedDrain.error ==
			TacticalCommandDrainError::AlreadyDraining &&
		reentrantCancellationSnapshot.pending.size() == 2 &&
		reentrantCancellationSnapshot.pending[0].requestId == reentrantBeta.requestId &&
		reentrantCancellationSnapshot.pending[1].requestId ==
			reentrantCancellationSink.submitted.requestId &&
		submittedDuringCancellation.cancelled == 1 &&
		reentrantCancellationInbox.size() == 1,
		"cancellation tolerates callback submission and rejects recursive mutation without iterator invalidation");
	reentrantCancellationInbox.cancelPackage("pkg.beta");

	NullTacticalCommandService& nullCommands =
		NullTacticalCommandService::instance();
	const auto nullSubmission = nullCommands.submit("pkg.null", validTurn);
	const auto nullInvalidOwner = nullCommands.submit("bad/owner", validTurn);
	const auto nullInvalidCommand = nullCommands.submit("pkg.null", unresolvedStance);
	const auto nullCancellation = nullCommands.cancelPackage("pkg.null");
	TacticalCommandInboxSnapshot nullSnapshot;
	nullSnapshot.pending.push_back(TacticalCommandRequest{
		99, "sentinel", MakeTurnCommand(9)});
	const TacticalCommandSnapshotError nullSnapshotResult =
		nullCommands.snapshot(nullSnapshot);
	TacticalCommandInbox& disabledCommands = TacticalCommandInbox::disabled();
	bool disabledHandlerCalled = false;
	const TacticalCommandDrainResult disabledDrain = disabledCommands.drain(
		[&](const TacticalCommandRequest&) {
			disabledHandlerCalled = true;
			return TacticalCommandDisposition::Accept;
		});
	check(nullSubmission.error == TacticalCommandSubmissionError::CapacityReached &&
		nullSubmission.requestId == 0 &&
		nullInvalidOwner.error == TacticalCommandSubmissionError::InvalidOwner &&
		nullInvalidCommand.error == TacticalCommandSubmissionError::InvalidCommand &&
		nullCancellation && nullCancellation.cancelled == 0 &&
		nullSnapshotResult == TacticalCommandSnapshotError::None &&
		nullSnapshot.pending.empty() && nullSnapshot.summary.pending == 0 &&
		nullCommands.limits().maximumPending == 0 &&
		disabledCommands.submit("pkg.null", validTurn).error ==
			TacticalCommandSubmissionError::CapacityReached &&
		disabledDrain.eligible == 0 && !disabledHandlerCalled,
		"null and disabled command services validate input but never retain work");

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
	std::vector<TacticalActorSnapshot> orderedScratch = tacticalSnapshot.actors();
	const TacticalActorSnapshot* orderedScratchStorage = orderedScratch.data();
	TacticalWorldSnapshot orderedSnapshot;
	check(TacticalWorldSnapshot::createReusableOrdered(
			44, tacticalSnapshot.sector(), tacticalSnapshot.turn(),
			orderedScratch, orderedSnapshot, 3) == TacticalSnapshotCreateError::None &&
		orderedScratch.empty() && orderedSnapshot.actors().data() == orderedScratchStorage &&
		orderedSnapshot.find(firstIncarnation) != nullptr,
		"ordered snapshot capture transfers validated adapter storage without sorting or copying");
	std::vector<TacticalActorSnapshot> descendingScratch = orderedSnapshot.actors();
	std::reverse(descendingScratch.begin(), descendingScratch.end());
	check(TacticalWorldSnapshot::createReusableOrdered(
			45, TacticalSectorSnapshot{}, TacticalTurnSnapshot{},
			descendingScratch, orderedSnapshot, 3) ==
				TacticalSnapshotCreateError::UnorderedEntity &&
		orderedSnapshot.epoch() == 44 && !descendingScratch.empty(),
		"ordered capture rejects descending adapter input without consuming scratch or output");
	std::vector<TacticalActorSnapshot> orderedDuplicates = orderedSnapshot.actors();
	orderedDuplicates[1] = orderedDuplicates[0];
	check(TacticalWorldSnapshot::createReusableOrdered(
			45, TacticalSectorSnapshot{}, TacticalTurnSnapshot{},
			orderedDuplicates, orderedSnapshot, 3) ==
				TacticalSnapshotCreateError::DuplicateEntity &&
		orderedSnapshot.epoch() == 44 && !orderedDuplicates.empty(),
		"ordered capture distinguishes duplicate identities transactionally");
	MemoryTacticalWorldService memoryWorld;
	memoryWorld.publish(tacticalSnapshot);
	ServiceCatalog tacticalServices;
	check(RegisterTacticalWorldService(tacticalServices, memoryWorld) ==
			EngineServiceRegistrationError::None,
		"tactical world service registers as an explicit versioned host extension");
	const auto resolvedWorld = tacticalServices.resolve(TacticalWorldServiceContract);
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

	RuntimeMessageBus preparedDeltaMessages(1, encodedDelta.size());
	RecordingRuntimeMessageSink preparedDeltaSink;
	preparedDeltaMessages.addSink(preparedDeltaSink);
	preparedDeltaMessages.publish(RuntimeMessageRequest{
		"fixture.blocker", "fixture.host", {9}});
	TacticalWorldDeltaPublisher preparedDeltaPublisher(
		preparedDeltaMessages,
		TacticalWorldDeltaPublishLimits{8, encodedDelta.size()});
	PreparedTacticalWorldDeltaMessage preparedDelta;
	const TacticalWorldDeltaPublishError deltaPrepared =
		preparedDeltaPublisher.prepare(codecFixture, preparedDelta);
	const std::vector<std::uint8_t> retainedDeltaPayload =
		preparedDelta.request.payload;
	const TacticalWorldDeltaPublishResult preparedDeltaPressure =
		preparedDeltaPublisher.publishPrepared(preparedDelta);
	const bool preparedDeltaRetained =
		preparedDelta.eventCount == codecFixture.events.size() &&
		preparedDelta.payloadBytes == encodedDelta.size() &&
		preparedDelta.request.payload == retainedDeltaPayload &&
		preparedDelta.request.topic == TacticalWorldDeltaMessageTopic &&
		preparedDelta.request.source == TacticalWorldDeltaMessageSource;
	preparedDeltaMessages.dispatchPending();
	const TacticalWorldDeltaPublishResult preparedDeltaPublished =
		preparedDeltaPublisher.publishPrepared(preparedDelta);
	preparedDeltaMessages.dispatchPending();
	TacticalWorldDelta deliveredPreparedDelta;
	const bool preparedDeltaDecoded = preparedDeltaSink.messages.size() == 2 &&
		DecodeTacticalWorldDelta(
			preparedDeltaSink.messages[1].payload, deliveredPreparedDelta) ==
				TacticalWorldDeltaDecodeResult::Success;
	check(deltaPrepared == TacticalWorldDeltaPublishError::None &&
		preparedDeltaPressure.error == TacticalWorldDeltaPublishError::QueueFull &&
		preparedDeltaRetained && preparedDeltaPublished &&
		preparedDeltaPublished.sequence == 2 && preparedDelta.request.payload.empty() &&
		preparedDeltaDecoded && deliveredPreparedDelta.events.size() == 8,
		"prepared tactical deltas encode once and retain exact bytes across bus pressure");
	PreparedTacticalWorldDeltaMessage retainedPreparedDelta;
	retainedPreparedDelta.request.payload = {7};
	retainedPreparedDelta.eventCount = 1;
	retainedPreparedDelta.payloadBytes = 1;
	TacticalWorldDeltaPublisher tinyPreparedDeltaPublisher(
		preparedDeltaMessages,
		TacticalWorldDeltaPublishLimits{8, encodedDelta.size() - 1});
	check(tinyPreparedDeltaPublisher.prepare(codecFixture, retainedPreparedDelta) ==
			TacticalWorldDeltaPublishError::PayloadTooLarge &&
		retainedPreparedDelta.request.payload == std::vector<std::uint8_t>({7}) &&
		retainedPreparedDelta.eventCount == 1 && retainedPreparedDelta.payloadBytes == 1,
		"tactical delta preparation enforces payload limits transactionally");

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
	check(!NullTacticalWorldObserverService::instance().latest(),
		"the null tactical observer is an allocation-free unavailable package service");
	TacticalWorldObserver observedWorld(memoryWorld, TacticalWorldObserverLimits{3, 6});
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::SourceUnavailable &&
		!observedWorld.latest(),
		"an unavailable source cannot fabricate an observer publication");
	memoryWorld.publish(tacticalSnapshot);
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::PublishedBaseline,
		"the first successful safe-frame capture publishes a tactical baseline");
	TacticalWorldPublicationView publication = observedWorld.latest();
	check(publication && publication.status == TacticalWorldPublicationStatus::Baseline &&
		publication.serial == 1 && publication.snapshot->epoch() == 44 &&
		publication.delta->previousEpoch == 44 &&
		publication.delta->currentEpoch == 44 && publication.delta->events.empty(),
		"a baseline owns the latest immutable snapshot without fabricated events");
	check(RegisterTacticalWorldObserverService(tacticalServices, observedWorld) ==
			EngineServiceRegistrationError::None,
		"the read-only tactical observer registers as a versioned package service");
	const auto resolvedObserver = tacticalServices.resolve(TacticalWorldObserverServiceContract);
	check(resolvedObserver && resolvedObserver.service->latest().serial == 1 &&
		!tacticalServices.resolve<TacticalWorldObserverService>(
			TacticalWorldObserverServiceId, EngineServiceVersion{2, 0}),
		"packages resolve only compatible tactical observer service versions");

	memoryWorld.publish(changedWorld);
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::PublishedDelta,
		"a later successful safe-frame capture publishes a tactical delta");
	publication = observedWorld.latest();
	check(publication && publication.status == TacticalWorldPublicationStatus::Delta &&
		publication.serial == 2 && publication.snapshot->find(TacticalEntityId{9, 1}) &&
		publication.delta->events.size() == 6 &&
		std::holds_alternative<TacticalTurnChangedEvent>(publication.delta->events[0]) &&
		std::holds_alternative<TacticalActorLeftEvent>(publication.delta->events[1]) &&
		std::holds_alternative<TacticalActorMovedEvent>(publication.delta->events[2]) &&
		std::holds_alternative<TacticalActorStanceChangedEvent>(publication.delta->events[3]) &&
		std::holds_alternative<TacticalActorVitalsChangedEvent>(publication.delta->events[4]) &&
		std::holds_alternative<TacticalActorEnteredEvent>(publication.delta->events[5]),
		"observer publications retain deterministic delta category and entity order");
	const TacticalWorldSnapshot* changedPublicationSnapshot = publication.snapshot;
	const TacticalWorldDelta* changedPublicationDelta = publication.delta;
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::Unchanged,
		"an unchanged successful capture is suppressed before observer publication");
	publication = observedWorld.latest();
	check(publication.serial == 2 && publication.snapshot == changedPublicationSnapshot &&
		publication.delta == changedPublicationDelta && publication.delta->events.size() == 6,
		"unchanged capture preserves the last meaningful snapshot, delta, and serial");
	memoryWorld.clear();
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::SourceUnavailable &&
		!observedWorld.latest(),
		"source unavailability invalidates the observer's unloaded world publication");

	memoryWorld.publish(reloadedWorld);
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::PublishedBaseline,
		"the first world after an unavailable boundary establishes a fresh baseline");
	publication = observedWorld.latest();
	check(publication.serial == 1 && publication.status == TacticalWorldPublicationStatus::Baseline &&
		publication.delta->events.empty(),
		"observer serials restart only after the old publication becomes unavailable");
	TacticalWorldSnapshot replacedWorld;
	check(TacticalWorldSnapshot::create(
			46, reloadedWorld.sector(), reloadedWorld.turn(),
			reloadedWorld.actors(), replacedWorld) == TacticalSnapshotCreateError::None,
		"direct epoch replacement fixture remains valid");
	memoryWorld.publish(replacedWorld);
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::PublishedDelta,
		"a direct tactical epoch replacement remains publishable through the observer");
	publication = observedWorld.latest();
	check(publication.serial == 2 && publication.delta->events.size() == 1 &&
		std::holds_alternative<TacticalWorldResetEvent>(publication.delta->events[0]),
		"direct observer epoch changes reuse the existing bounded tactical reset event");
	std::vector<TacticalActorSnapshot> excessiveActors = reloadedWorld.actors();
	excessiveActors.push_back(TacticalActorSnapshot{
		TacticalEntityId{11, 1}, 2, 21, 440, 0, 0, 4,
		TacticalStance::Standing, 80, 80, 80, 80, 80, true, true});
	TacticalWorldSnapshot excessiveWorld;
	check(TacticalWorldSnapshot::create(
			45, reloadedWorld.sector(), reloadedWorld.turn(), excessiveActors,
			excessiveWorld) == TacticalSnapshotCreateError::None,
		"observer actor-capacity fixture is a valid tactical snapshot");
	memoryWorld.publish(excessiveWorld);
	check(observedWorld.update() == TacticalWorldObserverUpdateResult::ActorCapacityReached &&
		observedWorld.latest().serial == 2 &&
		observedWorld.latest().snapshot->actors().size() == 3 &&
		observedWorld.latest().delta->events.size() == 1,
		"observer actor capacity failure preserves snapshot, delta, and serial atomically");

	MemoryTacticalWorldService eventLimitedMemory;
	eventLimitedMemory.publish(tacticalSnapshot);
	TacticalWorldObserver eventLimitedWorld(
		eventLimitedMemory, TacticalWorldObserverLimits{3, 5});
	check(eventLimitedWorld.update() == TacticalWorldObserverUpdateResult::PublishedBaseline,
		"an event-limited observer can establish its baseline");
	eventLimitedMemory.publish(changedWorld);
	check(eventLimitedWorld.update() ==
			TacticalWorldObserverUpdateResult::EventCapacityReached &&
		eventLimitedWorld.latest().status == TacticalWorldPublicationStatus::Baseline &&
		eventLimitedWorld.latest().serial == 1 &&
		eventLimitedWorld.latest().delta->events.empty(),
		"observer event capacity failure cannot expose a partial delta publication");

	ControlledTacticalWorldService controlledWorld;
	controlledWorld.publish(tacticalSnapshot);
	TacticalWorldObserver failureObservedWorld(controlledWorld);
	check(failureObservedWorld.update() ==
			TacticalWorldObserverUpdateResult::PublishedBaseline,
		"a controlled tactical provider establishes a reusable observer fixture");
	controlledWorld.fail(TacticalWorldCaptureResult::CapacityReached);
	const bool sourceCapacityPreserved =
		failureObservedWorld.update() ==
			TacticalWorldObserverUpdateResult::SourceCapacityReached &&
		failureObservedWorld.latest().serial == 1;
	controlledWorld.fail(TacticalWorldCaptureResult::AllocationFailure);
	const bool sourceAllocationPreserved =
		failureObservedWorld.update() ==
			TacticalWorldObserverUpdateResult::SourceAllocationFailure &&
		failureObservedWorld.latest().serial == 1;
	controlledWorld.fail(TacticalWorldCaptureResult::AdapterFailure);
	const bool sourceAdapterPreserved =
		failureObservedWorld.update() ==
			TacticalWorldObserverUpdateResult::SourceAdapterFailure &&
		failureObservedWorld.latest().serial == 1;
	check(sourceCapacityPreserved && sourceAllocationPreserved && sourceAdapterPreserved &&
		failureObservedWorld.latest().status == TacticalWorldPublicationStatus::Baseline &&
		failureObservedWorld.latest().delta->events.empty(),
		"all explicit source failures preserve the observer's last good publication");
	controlledWorld.publish(TacticalWorldSnapshot{});
	check(failureObservedWorld.update() == TacticalWorldObserverUpdateResult::InvalidSnapshot &&
		failureObservedWorld.latest().serial == 1,
		"a source claiming success with invalid state cannot replace the baseline");

	std::vector<RecordedSimulationCommand> recorded{
		RecordedSimulationCommand{
			17, 41, CommandJournalStatus::Applied,
			SimulationCommand{ChangeStanceCommand{
				firstIncarnation, 2, SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			18, 42, CommandJournalStatus::Queued,
			SimulationCommand{ChangeStanceCommand{
				reusedSlot, 1, SimulationCommandSource::Replay}}},
		RecordedSimulationCommand{
			19, 43, CommandJournalStatus::Applied,
			SimulationCommand{BeginFireWeaponCommand{
				firstIncarnation, -123, -1, 4, SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			20, 44, CommandJournalStatus::Applied,
			SimulationCommand{MoveToGridCommand{
				reusedSlot, 2345, 6, true, false,
				SimulationCommandSource::Replay,
				TacticalMoveOrigin::TeamAwareUi,
				TacticalPendingActionPolicy::Preserve}}},
		RecordedSimulationCommand{
			21, 45, CommandJournalStatus::Blocked,
			SimulationCommand{EndTurnCommand{2, SimulationCommandSource::NetworkPeer}}},
		RecordedSimulationCommand{
			22, 46, CommandJournalStatus::Applied,
			SimulationCommand{SetFacingCommand{
				firstIncarnation, 7, SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			23, 47, CommandJournalStatus::Applied,
			SimulationCommand{SetStealthModeCommand{
				reusedSlot, true, SimulationCommandSource::Replay}}},
		RecordedSimulationCommand{
			24, 48, CommandJournalStatus::Queued,
			SimulationCommand{StopMovementCommand{
				firstIncarnation, SimulationCommandSource::System}}}};
	std::vector<std::uint8_t> encoded;
	check(EncodeSimulationCommandJournal(recorded, 3, encoded) &&
		encoded.size() > 5 && encoded[4] == SimulationCommandJournalWireVersion &&
		encoded[5] == 0,
		"JA2 adapter emits version-5 simulation command journals");
	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	const SimulationCommandJournalDecodeResult decodeResult =
		DecodeSimulationCommandJournal(encoded, decoded, dropped);
	bool decodedFields = false;
	if (decodeResult == SimulationCommandJournalDecodeResult::Success && decoded.size() == 8)
	{
		const auto& oldOccupant = std::get<ChangeStanceCommand>(decoded[0].command);
		const auto& newOccupant = std::get<ChangeStanceCommand>(decoded[1].command);
		const auto& fire = std::get<BeginFireWeaponCommand>(decoded[2].command);
		const auto& move = std::get<MoveToGridCommand>(decoded[3].command);
		const auto& turn = std::get<EndTurnCommand>(decoded[4].command);
		const auto& facing = std::get<SetFacingCommand>(decoded[5].command);
		const auto& stealth = std::get<SetStealthModeCommand>(decoded[6].command);
		const auto& stop = std::get<StopMovementCommand>(decoded[7].command);
		decodedFields = dropped == 3 && decoded[0].tick == 17 &&
			decoded[0].sequence == 41 &&
			decoded[0].status == CommandJournalStatus::Applied &&
			oldOccupant.soldier == firstIncarnation && oldOccupant.stance == 2 &&
			newOccupant.soldier == reusedSlot && newOccupant.stance == 1 &&
			oldOccupant.soldier != newOccupant.soldier &&
			fire.soldier == firstIncarnation &&
			fire.targetGrid == -123 && fire.targetLevel == -1 &&
			fire.targetCubeLevel == 4 &&
			fire.source == SimulationCommandSource::LocalPlayer &&
			move.soldier == reusedSlot && move.destinationGrid == 2345 &&
			move.movementMode == 6 && move.reverse && !move.forceRestart &&
			move.source == SimulationCommandSource::Replay &&
			move.origin == TacticalMoveOrigin::TeamAwareUi &&
			move.pendingAction == TacticalPendingActionPolicy::Preserve &&
			decoded[4].status == CommandJournalStatus::Blocked && turn.nextTeam == 2 &&
			turn.source == SimulationCommandSource::NetworkPeer &&
			facing.soldier == firstIncarnation && facing.direction == 7 &&
			facing.source == SimulationCommandSource::LocalPlayer &&
			stealth.soldier == reusedSlot && stealth.enabled &&
			stealth.source == SimulationCommandSource::Replay &&
			stop.soldier == firstIncarnation &&
			stop.source == SimulationCommandSource::System;
	}
	check(decodedFields,
		"version-5 commands preserve movement, facing, stealth, and stop intent");

	std::vector<RecordedSimulationCommand> unresolved = recorded;
	std::get<ChangeStanceCommand>(unresolved[0].command).soldier.incarnation = 0;
	std::vector<std::uint8_t> preservedEncoding{0xa5, 0x5a};
	check(!EncodeSimulationCommandJournal(unresolved, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"version-5 encoding rejects unresolved actor identities transactionally");
	std::vector<RecordedSimulationCommand> invalidFire = recorded;
	std::get<BeginFireWeaponCommand>(invalidFire[2].command).soldier =
		TacticalEntityId{};
	check(!EncodeSimulationCommandJournal(invalidFire, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"version-5 encoding validates fire-command actors");
	std::vector<RecordedSimulationCommand> invalidMove = recorded;
	std::get<MoveToGridCommand>(invalidMove[3].command).soldier =
		TacticalEntityId{};
	check(!EncodeSimulationCommandJournal(invalidMove, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"version-5 encoding validates move-command actors");
	invalidMove = recorded;
	std::get<MoveToGridCommand>(invalidMove[3].command).origin =
		static_cast<TacticalMoveOrigin>(0xff);
	check(!EncodeSimulationCommandJournal(invalidMove, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"version-5 encoding rejects unknown movement origins transactionally");
	invalidMove = recorded;
	std::get<MoveToGridCommand>(invalidMove[3].command).pendingAction =
		static_cast<TacticalPendingActionPolicy>(0xff);
	check(!EncodeSimulationCommandJournal(invalidMove, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"version-5 encoding rejects unknown pending-action policy transactionally");
	std::vector<RecordedSimulationCommand> invalidFacing = recorded;
	std::get<SetFacingCommand>(invalidFacing[5].command).direction =
		TacticalDirectionCount;
	check(!EncodeSimulationCommandJournal(invalidFacing, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"version-5 encoding rejects invalid tactical directions transactionally");

	std::vector<std::uint8_t> trailing = encoded;
	trailing.push_back(0xff);
	check(RejectsJournalWithoutPublishing(
		trailing, SimulationCommandJournalDecodeResult::Invalid),
		"JA2 command codec rejects trailing data without publishing partial output");

	// Literal version-1 bytes guard the historical slot-only stance layout.
	const std::vector<std::uint8_t> versionOneStance{
		0x53, 0x4d, 0x43, 0x31, 0x01, 0x00,
		0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x00, 0x00, 0x00,
		0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x29, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x01, 0x02, 0x07, 0x00, 0x02, 0x03};
	std::vector<RecordedSimulationCommand> legacyDecoded;
	std::uint64_t legacyDropped = 0;
	const SimulationCommandJournalDecodeResult legacyResult =
		DecodeSimulationCommandJournal(
			versionOneStance, legacyDecoded, legacyDropped);
	bool legacyFields = false;
	if (legacyResult == SimulationCommandJournalDecodeResult::Success &&
		legacyDecoded.size() == 1)
	{
		const auto& stance =
			std::get<ChangeStanceCommand>(legacyDecoded[0].command);
		legacyFields = legacyDropped == 3 && legacyDecoded[0].tick == 17 &&
			legacyDecoded[0].sequence == 41 &&
			legacyDecoded[0].status == CommandJournalStatus::Applied &&
			stance.soldier == TacticalEntityId{7, 0} &&
			stance.soldier.legacyUnresolved() && !stance.soldier.valid() &&
			stance.stance == 2 && stance.source == SimulationCommandSource::Replay;
	}
	check(legacyFields,
		"version-1 stance slots decode explicitly as legacy-unresolved identities");
	std::vector<std::uint8_t> refusedUpgrade{0xcc};
	check(!EncodeSimulationCommandJournal(
		legacyDecoded, legacyDropped, refusedUpgrade) &&
		refusedUpgrade == std::vector<std::uint8_t>{0xcc},
		"legacy-unresolved stance identities cannot be emitted as version 5");

	std::vector<std::uint8_t> malformedV1 = versionOneStance;
	malformedV1[36] = 0xff;
	malformedV1[37] = 0xff;
	check(RejectsJournalWithoutPublishing(
		malformedV1, SimulationCommandJournalDecodeResult::Invalid),
		"version-1 decoding rejects an invalid sentinel actor slot");
	malformedV1 = versionOneStance;
	malformedV1[34] = 0xff;
	check(RejectsJournalWithoutPublishing(
		malformedV1, SimulationCommandJournalDecodeResult::Invalid),
		"version-1 decoding rejects unknown journal statuses");
	malformedV1 = versionOneStance;
	malformedV1[35] = 0xff;
	check(RejectsJournalWithoutPublishing(
		malformedV1, SimulationCommandJournalDecodeResult::Invalid),
		"version-1 decoding rejects unknown command tags");
	malformedV1 = versionOneStance;
	malformedV1[39] = 0xff;
	check(RejectsJournalWithoutPublishing(
		malformedV1, SimulationCommandJournalDecodeResult::Invalid),
		"version-1 decoding rejects unknown command sources");
	malformedV1 = versionOneStance;
	malformedV1.pop_back();
	check(RejectsJournalWithoutPublishing(
		malformedV1, SimulationCommandJournalDecodeResult::Invalid),
		"version-1 decoding rejects truncated records transactionally");
	malformedV1 = versionOneStance;
	malformedV1[4] = 6;
	check(RejectsJournalWithoutPublishing(
		malformedV1, SimulationCommandJournalDecodeResult::UnsupportedVersion),
		"command journals reject unsupported future wire versions");

	std::vector<RecordedSimulationCommand> oneStance{recorded[0]};
	std::vector<std::uint8_t> malformedV5;
	const bool encodedV5Fixture =
		EncodeSimulationCommandJournal(oneStance, 0, malformedV5) &&
		malformedV5.size() == 44;
	check(encodedV5Fixture,
		"version-5 stance fixture encodes for corruption checks");
	std::vector<std::uint8_t> compatibleV2 = malformedV5;
	if (encodedV5Fixture)
	{
		compatibleV2[4] = 2;
		malformedV5[38] = 0;
		malformedV5[39] = 0;
		malformedV5[40] = 0;
		malformedV5[41] = 0;
	}
	std::vector<RecordedSimulationCommand> compatibleV2Decoded;
	std::uint64_t compatibleV2Dropped = 1;
	check(encodedV5Fixture &&
		DecodeSimulationCommandJournal(
			compatibleV2, compatibleV2Decoded, compatibleV2Dropped) ==
				SimulationCommandJournalDecodeResult::Success &&
		compatibleV2Dropped == 0 && compatibleV2Decoded.size() == 1 &&
		std::get<ChangeStanceCommand>(compatibleV2Decoded[0].command).soldier ==
			firstIncarnation,
		"version-5 decoder retains version-2 generational command compatibility");
	check(RejectsJournalWithoutPublishing(
		malformedV5, SimulationCommandJournalDecodeResult::Invalid),
		"version-5 decoding rejects zero-incarnation actor identities");

	std::vector<RecordedSimulationCommand> oneMove{recorded[3]};
	std::vector<std::uint8_t> malformedMove;
	const bool encodedMoveFixture =
		EncodeSimulationCommandJournal(oneMove, 0, malformedMove) &&
		malformedMove.size() == 52;
	check(encodedMoveFixture,
		"version-5 move fixture encodes its stable value fields");
	std::vector<std::uint8_t> compatibleV3Move = malformedMove;
	if (encodedMoveFixture)
	{
		compatibleV3Move[4] = 3;
		compatibleV3Move.resize(compatibleV3Move.size() - 2);
		malformedMove[48] = 0x80;
	}
	std::vector<RecordedSimulationCommand> compatibleV3MoveDecoded;
	std::uint64_t compatibleV3MoveDropped = 1;
	const bool decodedCompatibleV3Move = encodedMoveFixture &&
		DecodeSimulationCommandJournal(
			compatibleV3Move, compatibleV3MoveDecoded,
			compatibleV3MoveDropped) ==
			SimulationCommandJournalDecodeResult::Success &&
		compatibleV3MoveDecoded.size() == 1;
	bool compatibleV3MoveDefaults = false;
	if (decodedCompatibleV3Move)
	{
		const MoveToGridCommand& move =
			std::get<MoveToGridCommand>(compatibleV3MoveDecoded[0].command);
		compatibleV3MoveDefaults =
			move.origin == TacticalMoveOrigin::PlayerUi &&
			move.pendingAction == TacticalPendingActionPolicy::Clear;
	}
	check(compatibleV3MoveDefaults,
		"version-3 moves decode with legacy UI and clear-pending defaults");
	std::vector<std::uint8_t> moveWithOldVersion = compatibleV3Move;
	if (encodedMoveFixture) moveWithOldVersion[4] = 2;
	check(RejectsJournalWithoutPublishing(
		moveWithOldVersion, SimulationCommandJournalDecodeResult::Invalid),
		"version-2 journals cannot smuggle the version-3 move tag");
	check(RejectsJournalWithoutPublishing(
		malformedMove, SimulationCommandJournalDecodeResult::Invalid),
		"version-5 move decoding rejects unknown packed flags transactionally");
	std::vector<std::uint8_t> malformedMoveOrigin;
	std::vector<std::uint8_t> malformedPendingAction;
	if (encodedMoveFixture)
	{
		EncodeSimulationCommandJournal(oneMove, 0, malformedMoveOrigin);
		malformedPendingAction = malformedMoveOrigin;
		malformedMoveOrigin[50] = 0xff;
		malformedPendingAction[51] = 0xff;
	}
	check(encodedMoveFixture && RejectsJournalWithoutPublishing(
		malformedMoveOrigin, SimulationCommandJournalDecodeResult::Invalid),
		"version-5 move decoding rejects unknown movement origins transactionally");
	check(encodedMoveFixture && RejectsJournalWithoutPublishing(
		malformedPendingAction, SimulationCommandJournalDecodeResult::Invalid),
		"version-5 move decoding rejects unknown pending-action policy transactionally");

	std::vector<RecordedSimulationCommand> oneFacing{recorded[5]};
	std::vector<RecordedSimulationCommand> oneStealth{recorded[6]};
	std::vector<RecordedSimulationCommand> oneStop{recorded[7]};
	std::vector<std::uint8_t> malformedFacing;
	std::vector<std::uint8_t> malformedStealth;
	std::vector<std::uint8_t> stopWithOldVersion;
	const bool encodedNewCommands =
		EncodeSimulationCommandJournal(oneFacing, 0, malformedFacing) &&
		EncodeSimulationCommandJournal(oneStealth, 0, malformedStealth) &&
		EncodeSimulationCommandJournal(oneStop, 0, stopWithOldVersion) &&
		malformedFacing.size() == 44 && malformedStealth.size() == 44 &&
		stopWithOldVersion.size() == 43;
	if (encodedNewCommands)
	{
		malformedFacing[42] = TacticalDirectionCount;
		malformedStealth[42] = 2;
		stopWithOldVersion[4] = 4;
	}
	check(encodedNewCommands && RejectsJournalWithoutPublishing(
		malformedFacing, SimulationCommandJournalDecodeResult::Invalid),
		"version-5 facing decoding rejects invalid directions transactionally");
	check(encodedNewCommands && RejectsJournalWithoutPublishing(
		malformedStealth, SimulationCommandJournalDecodeResult::Invalid),
		"version-5 stealth decoding rejects malformed booleans transactionally");
	check(encodedNewCommands && RejectsJournalWithoutPublishing(
		stopWithOldVersion, SimulationCommandJournalDecodeResult::Invalid),
		"version-4 journals cannot smuggle version-5 command tags");

	CommandJournal<SimulationCommand> journal(1);
	journal.recordSubmission(
		1, 10, SimulationCommand{EndTurnCommand{1, SimulationCommandSource::System}});
	journal.recordSubmission(
		2, 11, SimulationCommand{ChangeStanceCommand{
			TacticalEntityId{3, 301}, 2, SimulationCommandSource::LocalPlayer}});
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
			TacticalEntityId{5, 501}, 1, SimulationCommandSource::LocalPlayer}});
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
		std::get<ChangeStanceCommand>(replayed[1].command).soldier ==
			TacticalEntityId{5, 501},
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
