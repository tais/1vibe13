#include <Engine/Adapters/JA2/EngineRuntime.h>
#include <Engine/Adapters/JA2/CampaignClockScheduler.h>
#include <Engine/Adapters/JA2/CampaignClockService.h>
#include <Engine/Adapters/JA2/CampaignClockSession.h>
#include <Engine/Adapters/JA2/CampaignEventQueue.h>
#include <Engine/Adapters/JA2/CampaignEventService.h>
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

class ReassemblingTacticalDeltaSink final : public RuntimeMessageSink
{
public:
	explicit ReassemblingTacticalDeltaSink(
		TacticalWorldDeltaReassemblyLimits limits = {})
		: reassembler(limits) {}

	void receiveMessage(const RuntimeMessage& message) override
	{
		results.push_back(reassembler.accept(message, delta));
		if (results.back() == TacticalWorldDeltaReassemblyResult::Completed)
			++completed;
	}

	TacticalWorldDeltaReassembler reassembler;
	TacticalWorldDelta delta;
	std::vector<TacticalWorldDeltaReassemblyResult> results;
	std::size_t completed = 0;
};

void WriteTestU32(
	std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
	for (std::size_t index = 0; index < 4; ++index)
		bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
}

void WriteTestU64(
	std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value)
{
	for (std::size_t index = 0; index < 8; ++index)
		bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
}

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

	TacticalWorldSession worldSession;
	check(!worldSession.snapshot().loaded &&
		worldSession.snapshot().sector == TacticalWorldSession::Sector{} &&
		worldSession.snapshot().worldGeneration == 0 &&
		worldSession.snapshot().turnSerial == 0,
		"tactical world sessions start unloaded without an identity");
	worldSession.setSector({9, 1, 0});
	worldSession.setTurnState({true, true, 2});
	const std::uint64_t firstWorldGeneration = worldSession.commitLoad();
	worldSession.beginTeamTurn();
	check(firstWorldGeneration == 1 && worldSession.snapshot().loaded &&
		worldSession.snapshot().sector == TacticalWorldSession::Sector{9, 1, 0} &&
		worldSession.snapshot().turnSerial == 2 &&
		worldSession.snapshot().turn ==
			TacticalWorldSession::Snapshot::Turn{true, true, 2},
		"committed tactical worlds own sector, combat mode, team, and turn identity");
	worldSession.unload();
	check(!worldSession.snapshot().loaded &&
		worldSession.snapshot().sector == TacticalWorldSession::Sector{9, 1, 0} &&
		worldSession.snapshot().worldGeneration == 1 &&
		worldSession.snapshot().turnSerial == 0 &&
		worldSession.snapshot().turn ==
			TacticalWorldSession::Snapshot::Turn{true, true, 2},
		"world unload preserves selected sector, generation, and tactical mode");
	worldSession.restore({
		{2, 3, -1}, true, std::numeric_limits<std::uint64_t>::max(),
		std::numeric_limits<std::uint64_t>::max()});
	worldSession.beginTeamTurn();
	const std::uint64_t wrappedWorldGeneration = worldSession.commitLoad();
	check(wrappedWorldGeneration == 1 && worldSession.snapshot().turnSerial == 1,
		"world generation retains legacy nonzero wrap while turn serial saturates");
	worldSession.setTurnBased(false);
	worldSession.setCombatActive(false);
	worldSession.setCurrentTeam(4);
	check(worldSession.snapshot().turn ==
			TacticalWorldSession::Snapshot::Turn{false, false, 4},
		"tactical turn transitions update one runtime-owned value state");
	check(&legacyBraceRuntime.tacticalWorldSession() ==
		&legacyBraceRuntime.tacticalWorldSession(),
		"EngineRuntime owns one stable tactical world session");

	CampaignClockSession campaignClock;
	campaignClock.initialize(24 * 60 * 60 + 2 * 60 * 60 + 3 * 60 + 5);
	check(campaignClock.snapshot() == CampaignClockSession::Snapshot{
			93785, 93785, 1, 2, 3},
		"campaign clock initializes total, checkpoint, and calendar atomically");
	campaignClock.advanceUncommitted(61);
	check(campaignClock.snapshot() == CampaignClockSession::Snapshot{
			93846, 93785, 1, 2, 3},
		"campaign event slices defer checkpoint and calendar publication");
	const CampaignClockSession::AdvanceCommit campaignCommit =
		campaignClock.commitAdvance();
	check(!campaignCommit.movedBackward &&
		campaignCommit.attemptedTotalSeconds == 93846 &&
		campaignClock.snapshot() == CampaignClockSession::Snapshot{
			93846, 93846, 1, 2, 4},
		"campaign tick commit advances its monotonic checkpoint and calendar");
	campaignClock.restoreSaved(
		3 * 24 * 60 * 60 + 12 * 60 * 60 + 34 * 60 + 56, 12345);
	check(campaignClock.snapshot() == CampaignClockSession::Snapshot{
			304496, 12345, 3, 12, 34},
		"campaign save restoration derives calendar fields from serialized time");
	campaignClock.restore({
		std::numeric_limits<std::uint32_t>::max() - 10,
		std::numeric_limits<std::uint32_t>::max() - 10, 0, 0, 0});
	campaignClock.advanceUncommitted(20);
	const CampaignClockSession::AdvanceCommit wrappedCampaignCommit =
		campaignClock.commitAdvance();
	check(wrappedCampaignCommit.movedBackward &&
		wrappedCampaignCommit.attemptedTotalSeconds == 9 &&
		campaignClock.snapshot().totalSeconds ==
			std::numeric_limits<std::uint32_t>::max() - 10 &&
		campaignClock.snapshot().previousTotalSeconds ==
			std::numeric_limits<std::uint32_t>::max() - 10,
		"campaign clock preserves the legacy backwards-time wrap guard");
	campaignClock.setEventTime(2 * 24 * 60 * 60 + 7 * 60 * 60 + 59 * 60);
	campaignClock.overrideCalendar(8, 0, 0);
	check(campaignClock.snapshot().totalSeconds == 201540 &&
		campaignClock.snapshot().day == 8 &&
		campaignClock.snapshot().hour == 0 &&
		campaignClock.snapshot().minute == 0,
		"campaign clock retains the legacy calendar-only compatibility override");
	check(&legacyBraceRuntime.campaignClockSession() ==
		&legacyBraceRuntime.campaignClockSession(),
		"EngineRuntime owns one stable campaign clock session");
	CampaignClockScheduler campaignScheduler;
	const CampaignClockScheduleResult inactiveCampaignSchedule =
		campaignScheduler.schedule(123, 0, 1);
	const CampaignClockScheduleResult invalidCampaignResolution =
		campaignScheduler.schedule(
			123, 1, CampaignClockScheduler::MaximumResolution + 1);
	check(inactiveCampaignSchedule.error ==
			CampaignClockScheduleError::Inactive &&
		inactiveCampaignSchedule.droppedElapsedMicroseconds == 123 &&
		invalidCampaignResolution.error ==
			CampaignClockScheduleError::InvalidResolution &&
		invalidCampaignResolution.droppedElapsedMicroseconds == 123 &&
		campaignScheduler.elapsedWithinSecondMicroseconds() == 0,
		"campaign scheduling rejects inactive and invalid controls transactionally");
	const CampaignClockScheduleResult almostOneSecond =
		campaignScheduler.schedule(999999, 1, 1);
	const CampaignClockScheduleResult oneSecondBoundary =
		campaignScheduler.schedule(1, 1, 1);
	check(almostOneSecond && almostOneSecond.advanceSeconds == 0 &&
		oneSecondBoundary && oneSecondBoundary.advanceSeconds == 1 &&
		oneSecondBoundary.completedRealSeconds == 1 &&
		campaignScheduler.elapsedWithinSecondMicroseconds() == 0,
		"campaign scheduling advances one-times time at an exact fixed-step boundary");
	campaignScheduler.reset();
	const CampaignClockScheduleResult beforeCompressedSlice =
		campaignScheduler.schedule(199999, 300, 5);
	const CampaignClockScheduleResult firstCompressedSlice =
		campaignScheduler.schedule(1, 300, 5);
	const CampaignClockScheduleResult middleCompressedSlices =
		campaignScheduler.schedule(600000, 300, 5);
	const CampaignClockScheduleResult finalCompressedSlice =
		campaignScheduler.schedule(200000, 300, 5);
	check(beforeCompressedSlice.advanceSeconds == 0 &&
		firstCompressedSlice.advanceSeconds == 60 &&
		middleCompressedSlices.advanceSeconds == 180 &&
		finalCompressedSlice.advanceSeconds == 60 &&
		finalCompressedSlice.completedRealSeconds == 1,
		"campaign scheduling preserves established speed and resolution slices");
	campaignScheduler.reset();
	std::uint64_t fixedStepCampaignSeconds = 0;
	std::uint32_t completedFixedStepSeconds = 0;
	for (std::size_t tick = 0; tick < 60; ++tick)
	{
		const CampaignClockScheduleResult scheduled =
			campaignScheduler.schedule(16667, 3600, 60);
		fixedStepCampaignSeconds += scheduled.advanceSeconds;
		completedFixedStepSeconds += scheduled.completedRealSeconds;
	}
	check(fixedStepCampaignSeconds == 3600 &&
		completedFixedStepSeconds == 1 &&
		campaignScheduler.elapsedWithinSecondMicroseconds() == 20,
		"sixty engine ticks deterministically schedule one compressed real second");
	campaignScheduler.reset();
	const CampaignClockScheduleResult coarseCampaignPhase =
		campaignScheduler.schedule(500000, 300, 5);
	const CampaignClockScheduleResult refinedCampaignPhase =
		campaignScheduler.schedule(16667, 300, 30);
	const CampaignClockScheduleResult loweredCampaignRate =
		campaignScheduler.schedule(16667, 60, 30);
	check(coarseCampaignPhase.advanceSeconds == 120 &&
		refinedCampaignPhase.advanceSeconds == 30 &&
		loweredCampaignRate.advanceSeconds == 0 &&
		campaignScheduler.emittedGameSecondsWithinSecond() == 150,
		"mid-second control changes preserve monotonic campaign progress");
	campaignScheduler.reset();
	const CampaignClockScheduleResult boundedCampaignStep =
		campaignScheduler.schedule(2500000, 300, 5);
	check(boundedCampaignStep.advanceSeconds == 300 &&
		boundedCampaignStep.acceptedElapsedMicroseconds == 1000000 &&
		boundedCampaignStep.droppedElapsedMicroseconds == 1500000 &&
		boundedCampaignStep.completedRealSeconds == 1,
		"campaign scheduling bounds a misconfigured fixed step explicitly");
	check(&legacyBraceRuntime.campaignClockScheduler() ==
			&legacyBraceRuntime.campaignClockScheduler(),
		"EngineRuntime owns one stable campaign pacing scheduler");
	CampaignClockSessionService campaignClockService(campaignClock);
	ServiceCatalog campaignClockServices;
	check(RegisterCampaignClockService(
			campaignClockServices, campaignClockService) ==
			EngineServiceRegistrationError::None,
		"campaign clock service registers as an explicit versioned host extension");
	const auto resolvedCampaignClock =
		campaignClockServices.resolve(CampaignClockServiceContract);
	const auto futureCampaignClock =
		campaignClockServices.resolve<CampaignClockService>(
			CampaignClockServiceId, EngineServiceVersion{1, 1});
	const auto wrongCampaignClockType =
		campaignClockServices.resolve<TacticalWorldService>(
			CampaignClockServiceId, CampaignClockServiceVersion);
	CampaignClockSession::Snapshot capturedCampaignClock;
	check(resolvedCampaignClock &&
		resolvedCampaignClock.service->capture(capturedCampaignClock) ==
			CampaignClockCaptureResult::Success &&
		capturedCampaignClock == campaignClock.snapshot() &&
		futureCampaignClock.error ==
			EngineServiceLookupError::IncompatibleVersion &&
		wrongCampaignClockType.error == EngineServiceLookupError::TypeMismatch,
		"packages capture campaign time by value with type and version checks");
	check(RegisterCampaignClockService(
			campaignClockServices, campaignClockService) ==
			EngineServiceRegistrationError::DuplicateId,
		"campaign clock service IDs cannot be registered twice");
	MemoryCampaignClockService memoryCampaignClock;
	CampaignClockSession::Snapshot retainedCampaignClock{
		7, 6, 5, 4, 3};
	check(memoryCampaignClock.capture(retainedCampaignClock) ==
			CampaignClockCaptureResult::Unavailable &&
		retainedCampaignClock ==
			CampaignClockSession::Snapshot{7, 6, 5, 4, 3},
		"unavailable memory campaign clocks preserve the caller's last capture");
	const CampaignClockSession::Snapshot publishedCampaignClock{
		190861, 190800, 2, 5, 1};
	memoryCampaignClock.publish(publishedCampaignClock);
	check(memoryCampaignClock.capture(retainedCampaignClock) ==
			CampaignClockCaptureResult::Success &&
		retainedCampaignClock == publishedCampaignClock,
		"memory campaign clocks publish deterministic package and replay fixtures");
	memoryCampaignClock.clear();
	check(memoryCampaignClock.capture(retainedCampaignClock) ==
			CampaignClockCaptureResult::Unavailable &&
		retainedCampaignClock == publishedCampaignClock &&
		NullCampaignClockService::instance().capture(retainedCampaignClock) ==
			CampaignClockCaptureResult::Unavailable &&
		retainedCampaignClock == publishedCampaignClock,
		"cleared and null campaign clocks retain the last complete capture");
	CampaignClockSession::Snapshot runtimeCampaignClock;
	legacyBraceRuntime.campaignClockSession().initialize(90061);
	check(&legacyBraceRuntime.campaignClockService() ==
			&legacyBraceRuntime.campaignClockService() &&
		legacyBraceRuntime.campaignClockService().capture(runtimeCampaignClock) ==
			CampaignClockCaptureResult::Success &&
		runtimeCampaignClock ==
			legacyBraceRuntime.campaignClockSession().snapshot(),
		"EngineRuntime owns one stable read-only view of its campaign clock");

	CampaignEventQueueSnapshot campaignEvents;
	const std::vector<CampaignEventSnapshot> orderedCampaignEvents{
		{190800, 11, 0, 0, 21, 0},
		{190800, 12, 3600, 4, 22, 1},
		{190861, std::numeric_limits<std::uint32_t>::max(), 60, 255, 255, 255}};
	check(CampaignEventQueueSnapshot::create(
			orderedCampaignEvents, campaignEvents) ==
			CampaignEventSnapshotCreateError::None &&
		campaignEvents.size() == 3 &&
		campaignEvents.events()[0] == orderedCampaignEvents[0] &&
		campaignEvents.events()[1] == orderedCampaignEvents[1] &&
		campaignEvents.events()[2] == orderedCampaignEvents[2],
		"campaign event snapshots preserve FIFO order and opaque legacy values");
	const std::size_t campaignEventCapacity = campaignEvents.events().capacity();
	std::vector<CampaignEventSnapshot> repeatedCampaignEvents =
		orderedCampaignEvents;
	check(CampaignEventQueueSnapshot::createReusableOrdered(
			repeatedCampaignEvents, campaignEvents) ==
			CampaignEventSnapshotCreateError::None &&
		campaignEvents.events().capacity() == campaignEventCapacity,
		"reusable campaign event captures retain caller-owned output storage");
	check(CampaignEventQueueSnapshot::create(
			{{190862, 1, 0, 0, 1, 0}, {190800, 2, 0, 0, 2, 0}},
			campaignEvents) == CampaignEventSnapshotCreateError::UnorderedEvent &&
		campaignEvents.size() == 3 &&
		campaignEvents.events()[0] == orderedCampaignEvents[0] &&
		CampaignEventQueueSnapshot::create(
			{{190800, 1, 0, 0, 1, 0}, {190801, 2, 0, 0, 2, 0}},
			campaignEvents, 1) ==
			CampaignEventSnapshotCreateError::TooManyEvents &&
		campaignEvents.size() == 3,
		"rejected campaign event input preserves the last complete snapshot");

	CampaignEventQueue ownedCampaignEvents(3);
	const CampaignEventScheduleResult laterCampaignEvent =
		ownedCampaignEvents.schedule({190861, 13, 0, 0, 23, 0});
	const CampaignEventScheduleResult equalTimeCampaignEvent =
		ownedCampaignEvents.schedule({190861, 14, 60, 4, 24, 1});
	const CampaignEventScheduleResult earlierCampaignEvent =
		ownedCampaignEvents.schedule({190800, 12, 0, 0, 22, 0});
	const CampaignEventScheduleResult excessCampaignEvent =
		ownedCampaignEvents.schedule({190900, 15, 0, 0, 25, 0});
	std::vector<CampaignEventSnapshot> ownedCampaignEventSnapshot;
	check(laterCampaignEvent && equalTimeCampaignEvent && earlierCampaignEvent &&
		!excessCampaignEvent &&
		excessCampaignEvent.error == CampaignEventQueueError::CapacityReached &&
		ownedCampaignEvents.validate() &&
		ownedCampaignEvents.capture(ownedCampaignEventSnapshot) &&
		ownedCampaignEventSnapshot ==
			std::vector<CampaignEventSnapshot>({
				{190800, 12, 0, 0, 22, 0},
				{190861, 13, 0, 0, 23, 0},
				{190861, 14, 60, 4, 24, 1}}) &&
		earlierCampaignEvent.event->id != laterCampaignEvent.event->id &&
		laterCampaignEvent.event->id != equalTimeCampaignEvent.event->id,
		"engine-owned campaign event queues are bounded, ordered, stable, and FIFO");
	const CampaignEventId identityBeforeClear = equalTimeCampaignEvent.event->id;
	const std::vector<CampaignEventSnapshot> retainedOwnedCampaignEvents =
		ownedCampaignEventSnapshot;
	check(ownedCampaignEvents.replace(
			{{190900, 1, 0, 0, 1, 0}, {190800, 2, 0, 0, 2, 0}}) ==
			CampaignEventQueueError::UnorderedInput &&
		ownedCampaignEvents.capture(ownedCampaignEventSnapshot) &&
		ownedCampaignEventSnapshot == retainedOwnedCampaignEvents,
		"campaign event replacement rejects unordered state transactionally");
	ownedCampaignEvents.clear();
	const CampaignEventScheduleResult eventAfterClear =
		ownedCampaignEvents.schedule({200000, 16, 0, 0, 26, 0});
	check(eventAfterClear &&
		eventAfterClear.event->id.value > identityBeforeClear.value &&
		ownedCampaignEvents.size() == 1 &&
		ownedCampaignEvents.validate(),
		"campaign event identities do not repeat when a runtime queue is cleared");
	const CampaignEventId identityBeforeDamagedClear =
		eventAfterClear.event->id;
	eventAfterClear.event->next = eventAfterClear.event;
	ownedCampaignEvents.clear();
	const CampaignEventScheduleResult eventAfterDamagedClear =
		ownedCampaignEvents.schedule({200001, 17, 0, 0, 27, 0});
	check(eventAfterDamagedClear &&
		eventAfterDamagedClear.event->id.value >
			identityBeforeDamagedClear.value &&
		ownedCampaignEvents.size() == 1 &&
		ownedCampaignEvents.validate(),
		"campaign event teardown breaks damaged cycles without repeating identity");
	check(&legacyBraceRuntime.campaignEventQueue() ==
			&legacyBraceRuntime.campaignEventQueue() &&
		legacyBraceRuntime.campaignEventQueue().empty(),
		"EngineRuntime owns one stable strategic event queue");

	MemoryCampaignEventService memoryCampaignEvents;
	ServiceCatalog campaignEventServices;
	check(RegisterCampaignEventService(
			campaignEventServices, memoryCampaignEvents) ==
			EngineServiceRegistrationError::None,
		"campaign event service registers as an explicit versioned host extension");
	const auto resolvedCampaignEvents =
		campaignEventServices.resolve(CampaignEventServiceContract);
	const auto futureCampaignEvents =
		campaignEventServices.resolve<CampaignEventService>(
			CampaignEventServiceId, EngineServiceVersion{1, 1});
	const auto wrongCampaignEventType =
		campaignEventServices.resolve<CampaignClockService>(
			CampaignEventServiceId, CampaignEventServiceVersion);
	CampaignEventQueueSnapshot retainedCampaignEvents = campaignEvents;
	check(resolvedCampaignEvents &&
		resolvedCampaignEvents.service->capture(retainedCampaignEvents) ==
			CampaignEventCaptureResult::Unavailable &&
		retainedCampaignEvents.size() == 3 &&
		futureCampaignEvents.error ==
			EngineServiceLookupError::IncompatibleVersion &&
		wrongCampaignEventType.error == EngineServiceLookupError::TypeMismatch,
		"campaign event lookup enforces type and version without erasing retained state");
	check(RegisterCampaignEventService(
			campaignEventServices, memoryCampaignEvents) ==
			EngineServiceRegistrationError::DuplicateId,
		"campaign event service IDs cannot be registered twice");
	memoryCampaignEvents.publish(campaignEvents);
	CampaignEventQueueSnapshot capturedCampaignEvents;
	check(memoryCampaignEvents.capture(capturedCampaignEvents) ==
			CampaignEventCaptureResult::Success &&
		capturedCampaignEvents.events() == campaignEvents.events(),
		"memory campaign event services publish isolated deterministic fixtures");
	memoryCampaignEvents.clear();
	check(memoryCampaignEvents.capture(capturedCampaignEvents) ==
			CampaignEventCaptureResult::Unavailable &&
		capturedCampaignEvents.events() == campaignEvents.events() &&
		NullCampaignEventService::instance().capture(capturedCampaignEvents) ==
			CampaignEventCaptureResult::Unavailable &&
		capturedCampaignEvents.events() == campaignEvents.events(),
		"cleared and null campaign event services retain the last complete capture");

	TacticalEntityDirectory entityDirectory(2);
	check(entityDirectory.maximumSlots() == 2 &&
		entityDirectory.activeCount() == 0 &&
		entityDirectory.nextIncarnation() == 1,
		"tactical entity directories are bounded and start empty");
	const std::uint32_t failedCreateIncarnation =
		entityDirectory.issueIncarnation();
	check(failedCreateIncarnation == 1 && entityDirectory.activeCount() == 0 &&
		entityDirectory.nextIncarnation() == 2,
		"failed creation consumes its legacy incarnation without publishing liveness");
	const TacticalEntityId firstDirectoryEntity{
		0, entityDirectory.issueIncarnation()};
	check(entityDirectory.activate(firstDirectoryEntity) &&
		entityDirectory.contains(firstDirectoryEntity) &&
		entityDirectory.identity(0) == firstDirectoryEntity &&
		entityDirectory.activeCount() == 1,
		"successful creation publishes one exact slot incarnation");
	const TacticalEntityId replacementDirectoryEntity{
		0, entityDirectory.issueIncarnation()};
	check(entityDirectory.activate(replacementDirectoryEntity) &&
		!entityDirectory.contains(firstDirectoryEntity) &&
		!entityDirectory.release(firstDirectoryEntity) &&
		entityDirectory.contains(replacementDirectoryEntity) &&
		entityDirectory.activeCount() == 1,
		"slot reuse rejects stale resolution and stale deletion");
	check(entityDirectory.release(replacementDirectoryEntity) &&
		!entityDirectory.identity(0).valid() && entityDirectory.activeCount() == 0,
		"exact deletion retires the live incarnation");
	entityDirectory.restoreNextIncarnation(700);
	const std::uint32_t loadTemporaryIncarnation =
		entityDirectory.issueIncarnation();
	const TacticalEntityId savedEntity{1, 77};
	check(loadTemporaryIncarnation == 700 &&
		entityDirectory.activate(savedEntity) &&
		entityDirectory.identity(1) == savedEntity &&
		entityDirectory.nextIncarnation() == 701,
		"save restoration preserves the serialized identity after consuming its temporary creation ID");
	entityDirectory.reset();
	check(entityDirectory.activeCount() == 0 &&
		entityDirectory.nextIncarnation() == 701,
		"pool reset clears liveness without rewinding the save-compatible sequence");
	check(&legacyBraceRuntime.tacticalEntityDirectory() ==
		&legacyBraceRuntime.tacticalEntityDirectory() &&
		legacyBraceRuntime.tacticalEntityDirectory().maximumSlots() >= 2048,
		"EngineRuntime owns one stable bounded tactical entity directory");

	TacticalWorldItemDirectory worldItemDirectory(3);
	check(worldItemDirectory.maximumSlots() == 3 &&
		worldItemDirectory.trackedSlots() == 0 &&
		worldItemDirectory.activeCount() == 0 &&
		worldItemDirectory.nextIncarnation() == 1,
		"world-item directories are bounded without eagerly allocating their maximum");
	const TacticalWorldItemId firstWorldItem{
		2, worldItemDirectory.issueIncarnation()};
	check(worldItemDirectory.activate(firstWorldItem) &&
		worldItemDirectory.trackedSlots() == 3 &&
		worldItemDirectory.identity(2) == firstWorldItem &&
		worldItemDirectory.activeCount() == 1,
		"world-item activation grows only through the exact live slot");
	const TacticalWorldItemId replacementWorldItem{
		2, worldItemDirectory.issueIncarnation()};
	check(worldItemDirectory.activate(replacementWorldItem) &&
		!worldItemDirectory.contains(firstWorldItem) &&
		!worldItemDirectory.release(firstWorldItem) &&
		worldItemDirectory.contains(replacementWorldItem) &&
		worldItemDirectory.activeCount() == 1,
		"world-item slot reuse replaces the incarnation without duplicating liveness");
	check(!worldItemDirectory.activate(TacticalWorldItemId{3, 99}) &&
		worldItemDirectory.trackedSlots() == 3 &&
		worldItemDirectory.release(replacementWorldItem) &&
		worldItemDirectory.activeCount() == 0,
		"out-of-range world-item identities cannot allocate or release storage");
	worldItemDirectory.reset();
	check(worldItemDirectory.trackedSlots() == 3 &&
		worldItemDirectory.nextIncarnation() == 3 &&
		!worldItemDirectory.identity(2).valid(),
		"world-item reset retires liveness without shrinking or rewinding identity");
	TacticalWorldItemDirectory adoptedWorldItemDirectory(1);
	const TacticalWorldItemId adoptedWorldItem{0, 77};
	check(adoptedWorldItemDirectory.activate(adoptedWorldItem) &&
		adoptedWorldItemDirectory.release(adoptedWorldItem) &&
		adoptedWorldItemDirectory.issueIncarnation() == 78,
		"adopted world-item identities advance the allocator beyond stale incarnations");
	TacticalWorldItemDirectory exhaustedWorldItemDirectory(1);
	exhaustedWorldItemDirectory.mergeNextIncarnation(
		std::numeric_limits<std::uint32_t>::max());
	check(exhaustedWorldItemDirectory.issueIncarnation() ==
			std::numeric_limits<std::uint32_t>::max() &&
		exhaustedWorldItemDirectory.issueIncarnation() == 0 &&
		exhaustedWorldItemDirectory.issueIncarnation() == 0 &&
		exhaustedWorldItemDirectory.nextIncarnation() == 0,
		"world-item incarnation exhaustion fails closed instead of wrapping into stale identities");
	check(&legacyBraceRuntime.tacticalWorldItemDirectory() ==
		&legacyBraceRuntime.tacticalWorldItemDirectory() &&
		legacyBraceRuntime.tacticalWorldItemDirectory().maximumSlots() >=
			TacticalWorldItemDirectory::DefaultMaximumSlots,
		"EngineRuntime owns one stable bounded tactical world-item directory");

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
		TacticalCommandInboxLimits{16, 16, 16, 8, 20});
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
	TraverseObstacleCommand invalidTraversalKind{
		TacticalEntityId{3, 301},
		static_cast<TacticalTraversalKind>(0xff),
		SimulationCommandSource::LocalPlayer};
	ActivateWorldObjectCommand invalidObjectDirection{
		TacticalEntityId{3, 301}, TacticalWorldObjectId{102, 7},
		TacticalDirectionCount, SimulationCommandSource::LocalPlayer};
	const SimulationCommand invalidConversationTarget{
		StartConversationCommand{
			TacticalEntityId{3, 301}, TacticalEntityId{},
			SimulationCommandSource::LocalPlayer}};
	const SimulationCommand invalidVehicleSeat{
		EnterVehicleCommand{
			TacticalEntityId{3, 301}, TacticalEntityId{4, 401}, 2,
			TacticalMaximumVehicleSeats,
			SimulationCommandSource::LocalPlayer}};
	const SimulationCommand invalidWorldItem{
		PickupWorldItemCommand{
			TacticalEntityId{3, 301}, TacticalWorldItemId{},
			102, 0, TacticalWorldItemPickupKind::SpecificItem,
			SimulationCommandSource::LocalPlayer}};
	const SimulationCommand invalidStealTarget{
		StealFromActorCommand{
			TacticalEntityId{3, 301}, TacticalEntityId{}, 102, 0,
			SimulationCommandSource::LocalPlayer}};
	const SimulationCommand invalidExchangeTarget{
		ExchangePositionsCommand{
			TacticalEntityId{3, 301}, TacticalEntityId{3, 301},
			101, 102, 0, SimulationCommandSource::LocalPlayer}};
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
	const TacticalCommandSubmissionResult invalidTraversalResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{invalidTraversalKind});
	const TacticalCommandSubmissionResult invalidObjectDirectionResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{invalidObjectDirection});
	const TacticalCommandSubmissionResult invalidConversationTargetResult =
		validationInbox.submit("pkg.ok", invalidConversationTarget);
	const TacticalCommandSubmissionResult invalidVehicleSeatResult =
		validationInbox.submit("pkg.ok", invalidVehicleSeat);
	const TacticalCommandSubmissionResult invalidWorldItemResult =
		validationInbox.submit("pkg.ok", invalidWorldItem);
	const TacticalCommandSubmissionResult invalidStealTargetResult =
		validationInbox.submit("pkg.ok", invalidStealTarget);
	const TacticalCommandSubmissionResult invalidExchangeTargetResult =
		validationInbox.submit("pkg.ok", invalidExchangeTarget);
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
	const TacticalCommandSubmissionResult validWeaponModeResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{CycleWeaponModeCommand{
				TacticalEntityId{3, 301},
				SimulationCommandSource::LocalPlayer}});
	const TacticalCommandSubmissionResult validScopeModeResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{CycleScopeModeCommand{
				TacticalEntityId{3, 301}, TacticalNoTargetGrid,
				SimulationCommandSource::Replay}});
	const TacticalCommandSubmissionResult validReloadResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{ReloadWeaponCommand{
				TacticalEntityId{3, 301}, false,
				SimulationCommandSource::NetworkPeer}});
	const TacticalCommandSubmissionResult validTraversalResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{TraverseObstacleCommand{
				TacticalEntityId{3, 301}, TacticalTraversalKind::ClimbWall,
				SimulationCommandSource::LocalPlayer}});
	const TacticalCommandSubmissionResult validActivationResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{ActivateWorldObjectCommand{
				TacticalEntityId{3, 301}, TacticalWorldObjectId{102, 7}, 3,
				SimulationCommandSource::NetworkPeer}});
	const TacticalCommandSubmissionResult validApproachResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{ApproachWorldObjectCommand{
				TacticalEntityId{3, 301}, TacticalWorldObjectId{102, 7}, 3,
				101, 6, true, false, SimulationCommandSource::Replay}});
	const TacticalCommandSubmissionResult validConversationResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{StartConversationCommand{
				TacticalEntityId{3, 301}, TacticalEntityId{4, 401},
				SimulationCommandSource::LocalPlayer}});
	const TacticalCommandSubmissionResult validConversationApproachResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{ApproachConversationCommand{
				TacticalEntityId{3, 301}, TacticalEntityId{4, 401},
				101, 6, true, SimulationCommandSource::Replay}});
	const TacticalCommandSubmissionResult validVehicleResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{EnterVehicleCommand{
				TacticalEntityId{3, 301}, TacticalEntityId{4, 401},
				2, 3, SimulationCommandSource::NetworkPeer}});
	const TacticalCommandSubmissionResult validVehicleApproachResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{ApproachVehicleCommand{
				TacticalEntityId{3, 301}, TacticalEntityId{4, 401},
				2, 3, 101, 6, false, SimulationCommandSource::Replay}});
	const TacticalCommandSubmissionResult validWorldItemResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{PickupWorldItemCommand{
				TacticalEntityId{3, 301}, TacticalWorldItemId{9, 901},
				102, 0, TacticalWorldItemPickupKind::SpecificItem,
				SimulationCommandSource::NetworkPeer}});
	const TacticalCommandSubmissionResult validStealResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{StealFromActorCommand{
				TacticalEntityId{3, 301}, TacticalEntityId{4, 401},
				102, 0, SimulationCommandSource::NetworkPeer}});
	const TacticalCommandSubmissionResult validExchangeResult =
		validationInbox.submit(
			"pkg.ok", SimulationCommand{ExchangePositionsCommand{
				TacticalEntityId{3, 301}, TacticalEntityId{4, 401},
				101, 102, 0, SimulationCommandSource::Replay}});
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
		invalidTraversalResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidObjectDirectionResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidConversationTargetResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidVehicleSeatResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidWorldItemResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidStealTargetResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidExchangeTargetResult.error ==
			TacticalCommandSubmissionError::InvalidCommand &&
		invalidOwner.requestId == 0 && invalidSourceResult.requestId == 0 &&
		validStanceResult.requestId == 1 && validFireResult.requestId == 2 &&
		validMoveResult.requestId == 3 && validWeaponModeResult.requestId == 4 &&
		validScopeModeResult.requestId == 5 && validReloadResult.requestId == 6 &&
		validTraversalResult.requestId == 7 &&
		validActivationResult.requestId == 8 &&
		validApproachResult.requestId == 9 &&
		validConversationResult.requestId == 10 &&
		validConversationApproachResult.requestId == 11 &&
		validVehicleResult.requestId == 12 &&
		validVehicleApproachResult.requestId == 13 &&
		validWorldItemResult.requestId == 14 &&
		validStealResult.requestId == 15 &&
		validExchangeResult.requestId == 16 &&
		validationInbox.summary().submitted == 16 &&
		validationInbox.summary().nextRequestId == 17,
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

	PreparedTacticalWorldDeltaBatch directBatch;
	check(tacticalPublisher.prepareBatch(codecFixture, 90, directBatch) ==
			TacticalWorldDeltaPublishError::None && directBatch && !directBatch.chunked &&
		directBatch.requests.size() == 1 &&
		directBatch.requests[0].topic == TacticalWorldDeltaMessageTopic &&
		directBatch.requests[0].source == TacticalWorldDeltaMessageSource &&
		directBatch.requests[0].payload == encodedDelta,
		"deltas within the bus limit retain their exact version-1 topic and bytes");
	RuntimeMessageBus noChunkMessages(1, encodedDelta.size());
	TacticalWorldDeltaPublisher noChunkPublisher(
		noChunkMessages,
		TacticalWorldDeltaPublishLimits{
			8, encodedDelta.size(), encodedDelta.size() + 1, 0});
	PreparedTacticalWorldDeltaBatch noChunkBatch;
	const TacticalWorldDeltaPublishError noChunkPrepared =
		noChunkPublisher.prepareBatch(codecFixture, 91, noChunkBatch);
	PreparedTacticalWorldDeltaBatch mutatedDirectBatch = noChunkBatch;
	++mutatedDirectBatch.totalPayloadBytes;
	const TacticalWorldDeltaBatchPublishResult mutatedDirectRejected =
		noChunkPublisher.publishPreparedBatch(mutatedDirectBatch);
	const TacticalWorldDeltaBatchPublishResult noChunkPublished =
		noChunkPublisher.publishPreparedBatch(noChunkBatch);
	check(noChunkPrepared == TacticalWorldDeltaPublishError::None &&
		!noChunkBatch.chunked && mutatedDirectRejected.error ==
			TacticalWorldDeltaPublishError::InvalidDelta &&
		noChunkPublished && noChunkMessages.queued() == 1,
		"disabling chunk batches preserves legacy messages and validates prepared byte counts");

	constexpr std::size_t ChunkPayloadLimit = TacticalWorldDeltaChunkHeaderBytes + 8;
	RuntimeMessageBus chunkMessages(2, ChunkPayloadLimit);
	ReassemblingTacticalDeltaSink chunkSink(
		TacticalWorldDeltaReassemblyLimits{encodedDelta.size(), 128, 8});
	chunkMessages.addSink(chunkSink);
	TacticalWorldDeltaPublisher chunkPublisher(
		chunkMessages,
		TacticalWorldDeltaPublishLimits{
			8, ChunkPayloadLimit, encodedDelta.size(), 128});
	PreparedTacticalWorldDeltaBatch chunkBatch;
	const TacticalWorldDeltaPublishError chunkPrepared =
		chunkPublisher.prepareBatch(codecFixture, 100, chunkBatch);
	const std::size_t preparedChunkCount = chunkBatch.requests.size();
	const TacticalWorldDeltaBatchPublishResult firstChunkPass =
		chunkPublisher.publishPreparedBatch(chunkBatch);
	const std::size_t cursorAfterPressure = chunkBatch.nextRequest;
	chunkMessages.dispatchPending();
	bool chunkRetryValid = true;
	std::size_t chunkPublishPasses = 1;
	while (!chunkBatch.complete() && chunkPublishPasses < preparedChunkCount + 2)
	{
		const TacticalWorldDeltaBatchPublishResult pass =
			chunkPublisher.publishPreparedBatch(chunkBatch);
		if (pass.error != TacticalWorldDeltaPublishError::None &&
			pass.error != TacticalWorldDeltaPublishError::QueueFull)
			chunkRetryValid = false;
		chunkMessages.dispatchPending();
		++chunkPublishPasses;
	}
	const bool chunkResultsOrdered =
		!chunkSink.results.empty() &&
		std::all_of(chunkSink.results.begin(), chunkSink.results.end() - 1,
			[](TacticalWorldDeltaReassemblyResult result) {
				return result == TacticalWorldDeltaReassemblyResult::AwaitingMore;
			}) &&
		chunkSink.results.back() == TacticalWorldDeltaReassemblyResult::Completed;
	check(chunkPrepared == TacticalWorldDeltaPublishError::None && chunkBatch.chunked &&
		preparedChunkCount > 2 &&
		firstChunkPass.error == TacticalWorldDeltaPublishError::QueueFull &&
		firstChunkPass.messagesPublished == 2 && cursorAfterPressure == 2 &&
		chunkRetryValid && chunkBatch.complete() &&
		chunkSink.results.size() == preparedChunkCount && chunkResultsOrdered &&
		chunkSink.completed == 1 && chunkSink.delta.events.size() == 8 &&
		chunkSink.delta.previousEpoch == codecFixture.previousEpoch &&
		chunkSink.delta.currentEpoch == codecFixture.currentEpoch,
		"chunk publication resumes its retained cursor without duplicates across frames");

	PreparedTacticalWorldDeltaBatch malformedBatch;
	const bool malformedBatchPrepared =
		chunkPublisher.prepareBatch(codecFixture, 101, malformedBatch) ==
			TacticalWorldDeltaPublishError::None &&
		malformedBatch.requests.size() > 1;
	if (malformedBatchPrepared) malformedBatch.requests[1].topic = "invalid topic";
	const TacticalWorldDeltaBatchPublishResult malformedBatchRejected =
		chunkPublisher.publishPreparedBatch(malformedBatch);
	PreparedTacticalWorldDeltaBatch malformedCompleteBatch;
	malformedCompleteBatch.transferId = 102;
	malformedCompleteBatch.totalPayloadBytes = 1;
	malformedCompleteBatch.chunked = true;
	malformedCompleteBatch.requests.resize(1);
	malformedCompleteBatch.nextRequest = 1;
	const TacticalWorldDeltaBatchPublishResult malformedCompleteRejected =
		chunkPublisher.publishPreparedBatch(malformedCompleteBatch);
	check(malformedBatchPrepared &&
		malformedBatchRejected.error == TacticalWorldDeltaPublishError::InvalidDelta &&
		malformedBatch.nextRequest == 0 && chunkMessages.queued() == 0 &&
		malformedCompleteRejected.error ==
			TacticalWorldDeltaPublishError::InvalidDelta &&
		!malformedCompleteRejected.complete,
		"batch validation rejects every unsent request before publishing a prefix");

	EncodedTacticalWorldDeltaChunks transferA;
	EncodedTacticalWorldDeltaChunks transferB;
	EncodedTacticalWorldDeltaChunks retainedChunkOutput;
	retainedChunkOutput.transferId = 77;
	retainedChunkOutput.totalPayloadBytes = 1;
	retainedChunkOutput.payloads = {{7}};
	const bool transferFixturesValid =
		EncodeTacticalWorldDeltaChunks(
			encodedDelta, 200, ChunkPayloadLimit, transferA,
			encodedDelta.size(), 128) == TacticalWorldDeltaChunkEncodeError::None &&
		EncodeTacticalWorldDeltaChunks(
			encodedDelta, 201, ChunkPayloadLimit, transferB,
			encodedDelta.size(), 128) == TacticalWorldDeltaChunkEncodeError::None;
	auto chunkMessage = [](const std::vector<std::uint8_t>& payload) {
		return RuntimeMessage{
			1, TacticalWorldDeltaChunkMessageTopic,
			TacticalWorldDeltaMessageSource, payload};
	};
	TacticalWorldDelta retainedReassembly;
	retainedReassembly.previousEpoch = 777;
	retainedReassembly.currentEpoch = 888;
	TacticalWorldDeltaReassembler supersedingReassembler(
		TacticalWorldDeltaReassemblyLimits{encodedDelta.size(), 128, 8});
	const TacticalWorldDeltaReassemblyResult firstTransferAccepted =
		supersedingReassembler.accept(
			chunkMessage(transferA.payloads[0]), retainedReassembly);
	const TacticalWorldDeltaReassemblyResult duplicateRejected =
		supersedingReassembler.accept(
			chunkMessage(transferA.payloads[0]), retainedReassembly);
	const TacticalWorldDeltaReassemblyResult foreignNonzeroRejected =
		supersedingReassembler.accept(
			chunkMessage(transferB.payloads[1]), retainedReassembly);
	const TacticalWorldDeltaReassemblyResult replacementAccepted =
		supersedingReassembler.accept(
			chunkMessage(transferB.payloads[0]), retainedReassembly);
	const TacticalWorldDeltaReassemblyResult olderZeroRejected =
		supersedingReassembler.accept(
			chunkMessage(transferA.payloads[0]), retainedReassembly);
	bool replacementCompleted = true;
	for (std::size_t index = 1; index < transferB.payloads.size(); ++index)
	{
		const TacticalWorldDeltaReassemblyResult accepted =
			supersedingReassembler.accept(
				chunkMessage(transferB.payloads[index]), retainedReassembly);
		if (accepted != (index + 1 == transferB.payloads.size()
				? TacticalWorldDeltaReassemblyResult::Completed
				: TacticalWorldDeltaReassemblyResult::AwaitingMore))
			replacementCompleted = false;
	}
	const TacticalWorldDeltaReassemblyResult completedReplayRejected =
		supersedingReassembler.accept(
			chunkMessage(transferA.payloads[0]), retainedReassembly);
	check(transferFixturesValid &&
		firstTransferAccepted == TacticalWorldDeltaReassemblyResult::AwaitingMore &&
		duplicateRejected == TacticalWorldDeltaReassemblyResult::UnexpectedChunk &&
		foreignNonzeroRejected ==
			TacticalWorldDeltaReassemblyResult::InterleavedTransfer &&
		replacementAccepted == TacticalWorldDeltaReassemblyResult::AwaitingMore &&
		olderZeroRejected == TacticalWorldDeltaReassemblyResult::InterleavedTransfer &&
		replacementCompleted && !supersedingReassembler.active() &&
		supersedingReassembler.highestTransferId() == 201 &&
		completedReplayRejected == TacticalWorldDeltaReassemblyResult::InterleavedTransfer &&
		retainedReassembly.previousEpoch == codecFixture.previousEpoch &&
		retainedReassembly.events.size() == codecFixture.events.size(),
		"a new index-zero transfer supersedes abandoned world data while duplicates and foreign continuations fail");

	TacticalWorldDeltaReassembler directBoundaryReassembler(
		TacticalWorldDeltaReassemblyLimits{encodedDelta.size(), 128, 8});
	TacticalWorldDelta directBoundaryOutput;
	directBoundaryOutput.previousEpoch = 777;
	const TacticalWorldDeltaReassemblyResult directPrefixAccepted =
		directBoundaryReassembler.accept(
			chunkMessage(transferA.payloads[0]), directBoundaryOutput);
	const std::size_t retainedDirectPrefix =
		directBoundaryReassembler.retainedBytes();
	const TacticalWorldDeltaReassemblyResult malformedDirectRejected =
		directBoundaryReassembler.accept(
			RuntimeMessage{1, TacticalWorldDeltaMessageTopic,
				TacticalWorldDeltaMessageSource, {0}},
			directBoundaryOutput);
	const bool malformedDirectPreservedPrefix =
		directBoundaryReassembler.active() &&
		directBoundaryReassembler.transferId() == 200 &&
		directBoundaryReassembler.retainedBytes() == retainedDirectPrefix &&
		directBoundaryOutput.previousEpoch == 777;
	const TacticalWorldDeltaReassemblyResult directBoundaryCompleted =
		directBoundaryReassembler.accept(
			RuntimeMessage{2, TacticalWorldDeltaMessageTopic,
				TacticalWorldDeltaMessageSource, encodedDelta},
			directBoundaryOutput);
	const TacticalWorldDeltaReassemblyResult delayedContinuationRejected =
		directBoundaryReassembler.accept(
			chunkMessage(transferA.payloads[1]), directBoundaryOutput);
	const TacticalWorldDeltaReassemblyResult delayedRestartRejected =
		directBoundaryReassembler.accept(
			chunkMessage(transferA.payloads[0]), directBoundaryOutput);
	check(directPrefixAccepted == TacticalWorldDeltaReassemblyResult::AwaitingMore &&
		retainedDirectPrefix != 0 &&
		malformedDirectRejected == TacticalWorldDeltaReassemblyResult::InvalidDelta &&
		malformedDirectPreservedPrefix &&
		directBoundaryReassembler.highestTransferId() == 200 &&
		directBoundaryCompleted == TacticalWorldDeltaReassemblyResult::Completed &&
		!directBoundaryReassembler.active() &&
		delayedContinuationRejected ==
			TacticalWorldDeltaReassemblyResult::UnexpectedChunk &&
		delayedRestartRejected ==
			TacticalWorldDeltaReassemblyResult::InterleavedTransfer &&
		directBoundaryOutput.previousEpoch == codecFixture.previousEpoch &&
		directBoundaryOutput.events.size() == codecFixture.events.size(),
		"a valid legacy delta retires stale chunk state while malformed direct input remains atomic");

	std::vector<std::uint8_t> unsupportedChunk = transferA.payloads[0];
	unsupportedChunk[4] = 2;
	std::vector<std::uint8_t> malformedLengthChunk = transferA.payloads[0];
	WriteTestU32(malformedLengthChunk, 34, 0);
	std::vector<std::uint8_t> oversizedChunk = transferA.payloads[0];
	WriteTestU64(oversizedChunk, 22, encodedDelta.size() + 1);
	std::vector<std::uint8_t> invalidCountChunk = transferA.payloads[0];
	WriteTestU32(invalidCountChunk, 18, 0);
	std::vector<std::uint8_t> excessiveCountChunk = transferA.payloads[0];
	WriteTestU32(excessiveCountChunk, 18, 129);
	TacticalWorldDeltaReassembler malformedReassembler(
		TacticalWorldDeltaReassemblyLimits{encodedDelta.size(), 128, 8});
	const bool rejectsMalformedEnvelopes =
		malformedReassembler.accept(
			chunkMessage(unsupportedChunk), retainedReassembly) ==
				TacticalWorldDeltaReassemblyResult::UnsupportedVersion &&
		malformedReassembler.accept(
			chunkMessage(malformedLengthChunk), retainedReassembly) ==
				TacticalWorldDeltaReassemblyResult::InvalidMessage &&
		malformedReassembler.accept(
			chunkMessage(oversizedChunk), retainedReassembly) ==
				TacticalWorldDeltaReassemblyResult::TransferTooLarge &&
		malformedReassembler.accept(
			chunkMessage(invalidCountChunk), retainedReassembly) ==
				TacticalWorldDeltaReassemblyResult::InvalidTransfer &&
		malformedReassembler.accept(
			chunkMessage(excessiveCountChunk), retainedReassembly) ==
				TacticalWorldDeltaReassemblyResult::TooManyChunks;
	bool everyTruncatedChunkRejected = true;
	for (std::size_t size = 0; size < transferA.payloads[0].size(); ++size)
	{
		std::vector<std::uint8_t> truncated(
			transferA.payloads[0].begin(), transferA.payloads[0].begin() + size);
		TacticalWorldDeltaReassembler truncatedReassembler;
		TacticalWorldDelta retainedTruncated;
		retainedTruncated.previousEpoch = 777;
		if (truncatedReassembler.accept(
				chunkMessage(truncated), retainedTruncated) !=
					TacticalWorldDeltaReassemblyResult::InvalidMessage ||
			retainedTruncated.previousEpoch != 777)
		{
			everyTruncatedChunkRejected = false;
			break;
		}
	}
	std::vector<std::vector<std::uint8_t>> corruptTransfer = transferA.payloads;
	corruptTransfer.back().back() ^= 0x80u;
	TacticalWorldDeltaReassembler integrityReassembler(
		TacticalWorldDeltaReassemblyLimits{encodedDelta.size(), 128, 8});
	retainedReassembly.previousEpoch = 777;
	retainedReassembly.currentEpoch = 888;
	retainedReassembly.events.clear();
	TacticalWorldDeltaReassemblyResult integrityResult =
		TacticalWorldDeltaReassemblyResult::InvalidMessage;
	for (const std::vector<std::uint8_t>& payload : corruptTransfer)
		integrityResult = integrityReassembler.accept(
			chunkMessage(payload), retainedReassembly);
	check(rejectsMalformedEnvelopes && everyTruncatedChunkRejected &&
		integrityResult == TacticalWorldDeltaReassemblyResult::IntegrityMismatch &&
		!integrityReassembler.active() && retainedReassembly.previousEpoch == 777 &&
		retainedReassembly.currentEpoch == 888 && retainedReassembly.events.empty(),
		"bounded reassembly rejects truncation, size, count, version, and integrity faults atomically");

	check(EncodeTacticalWorldDeltaChunks(
			encodedDelta, 0, ChunkPayloadLimit, retainedChunkOutput) ==
				TacticalWorldDeltaChunkEncodeError::InvalidTransfer &&
		EncodeTacticalWorldDeltaChunks(
			encodedDelta, 1, TacticalWorldDeltaChunkHeaderBytes,
			retainedChunkOutput) ==
				TacticalWorldDeltaChunkEncodeError::PayloadLimitTooSmall &&
		EncodeTacticalWorldDeltaChunks(
			encodedDelta, 1, ChunkPayloadLimit, retainedChunkOutput,
			encodedDelta.size() - 1, 128) ==
				TacticalWorldDeltaChunkEncodeError::TransferTooLarge &&
		EncodeTacticalWorldDeltaChunks(
			encodedDelta, 1, ChunkPayloadLimit, retainedChunkOutput,
			encodedDelta.size(), 1) ==
				TacticalWorldDeltaChunkEncodeError::TooManyChunks &&
		EncodeTacticalWorldDeltaChunks(
			encodedDelta, 1, std::numeric_limits<std::size_t>::max(),
			retainedChunkOutput) == TacticalWorldDeltaChunkEncodeError::TooManyChunks &&
		retainedChunkOutput.transferId == 77 &&
		retainedChunkOutput.payloads ==
			std::vector<std::vector<std::uint8_t>>({{7}}),
		"chunk preparation is bounded, overflow-safe, and transactional on rejection");

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
			TacticalWorldDeltaPublishError::InvalidMessageIdentifier &&
		MapTacticalWorldDeltaChunkEncodeError(
			TacticalWorldDeltaChunkEncodeError::InvalidTransfer) ==
				TacticalWorldDeltaPublishError::InvalidTransfer &&
		MapTacticalWorldDeltaChunkEncodeError(
			TacticalWorldDeltaChunkEncodeError::TransferTooLarge) ==
				TacticalWorldDeltaPublishError::TransferTooLarge &&
		MapTacticalWorldDeltaChunkEncodeError(
			TacticalWorldDeltaChunkEncodeError::TooManyChunks) ==
				TacticalWorldDeltaPublishError::TooManyChunks,
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
				firstIncarnation, SimulationCommandSource::System}}},
		RecordedSimulationCommand{
			25, 49, CommandJournalStatus::Applied,
			SimulationCommand{CycleWeaponModeCommand{
				reusedSlot, SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			26, 50, CommandJournalStatus::Applied,
			SimulationCommand{CycleScopeModeCommand{
				firstIncarnation, 3456, SimulationCommandSource::NetworkPeer}}},
		RecordedSimulationCommand{
			27, 51, CommandJournalStatus::Queued,
			SimulationCommand{ReloadWeaponCommand{
				reusedSlot, false, SimulationCommandSource::Replay}}},
		RecordedSimulationCommand{
			28, 52, CommandJournalStatus::Applied,
			SimulationCommand{TraverseObstacleCommand{
				firstIncarnation, TacticalTraversalKind::JumpWindow,
				SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			29, 53, CommandJournalStatus::Applied,
			SimulationCommand{ActivateWorldObjectCommand{
				reusedSlot, TacticalWorldObjectId{4567, 0x1234}, 6,
				SimulationCommandSource::NetworkPeer}}},
		RecordedSimulationCommand{
			30, 54, CommandJournalStatus::Queued,
			SimulationCommand{ApproachWorldObjectCommand{
				firstIncarnation, TacticalWorldObjectId{5678, 0x2345}, 4,
				5600, 6, true, false,
				SimulationCommandSource::Replay}}},
		RecordedSimulationCommand{
			31, 55, CommandJournalStatus::Applied,
			SimulationCommand{StartConversationCommand{
				firstIncarnation, reusedSlot,
				SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			32, 56, CommandJournalStatus::Queued,
			SimulationCommand{ApproachConversationCommand{
				reusedSlot, firstIncarnation, 6000, 6, true,
				SimulationCommandSource::Replay}}},
		RecordedSimulationCommand{
			33, 57, CommandJournalStatus::Applied,
			SimulationCommand{EnterVehicleCommand{
				firstIncarnation, reusedSlot, 2, 3,
				SimulationCommandSource::NetworkPeer}}},
		RecordedSimulationCommand{
			34, 58, CommandJournalStatus::Queued,
			SimulationCommand{ApproachVehicleCommand{
				reusedSlot, firstIncarnation, 5, 8, 6200, 6, false,
				SimulationCommandSource::System}}},
		RecordedSimulationCommand{
			35, 59, CommandJournalStatus::Applied,
			SimulationCommand{PickupWorldItemCommand{
				firstIncarnation, TacticalWorldItemId{123456, 9876},
				6400, 1, TacticalWorldItemPickupKind::SpecificItem,
				SimulationCommandSource::LocalPlayer}}},
		RecordedSimulationCommand{
			36, 60, CommandJournalStatus::Queued,
			SimulationCommand{StealFromActorCommand{
				firstIncarnation, reusedSlot, 6500, 1,
				SimulationCommandSource::NetworkPeer}}},
		RecordedSimulationCommand{
			37, 61, CommandJournalStatus::Applied,
			SimulationCommand{ExchangePositionsCommand{
				reusedSlot, firstIncarnation, 6600, 6601, 0,
				SimulationCommandSource::System}}}};
	std::vector<std::uint8_t> encoded;
	check(EncodeSimulationCommandJournal(recorded, 3, encoded) &&
		encoded.size() > 5 && encoded[4] == SimulationCommandJournalWireVersion &&
		encoded[5] == 0,
		"JA2 adapter emits the single current simulation command format");
	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 0;
	const SimulationCommandJournalDecodeResult decodeResult =
		DecodeSimulationCommandJournal(encoded, decoded, dropped);
	bool decodedFields = false;
	if (decodeResult == SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 21)
	{
		const auto& oldOccupant = std::get<ChangeStanceCommand>(decoded[0].command);
		const auto& newOccupant = std::get<ChangeStanceCommand>(decoded[1].command);
		const auto& fire = std::get<BeginFireWeaponCommand>(decoded[2].command);
		const auto& move = std::get<MoveToGridCommand>(decoded[3].command);
		const auto& turn = std::get<EndTurnCommand>(decoded[4].command);
		const auto& facing = std::get<SetFacingCommand>(decoded[5].command);
		const auto& stealth = std::get<SetStealthModeCommand>(decoded[6].command);
		const auto& stop = std::get<StopMovementCommand>(decoded[7].command);
		const auto& weaponMode =
			std::get<CycleWeaponModeCommand>(decoded[8].command);
		const auto& scopeMode =
			std::get<CycleScopeModeCommand>(decoded[9].command);
		const auto& reload =
			std::get<ReloadWeaponCommand>(decoded[10].command);
		const auto& traversal =
			std::get<TraverseObstacleCommand>(decoded[11].command);
		const auto& activation =
			std::get<ActivateWorldObjectCommand>(decoded[12].command);
		const auto& approach =
			std::get<ApproachWorldObjectCommand>(decoded[13].command);
		const auto& conversation =
			std::get<StartConversationCommand>(decoded[14].command);
		const auto& conversationApproach =
			std::get<ApproachConversationCommand>(decoded[15].command);
		const auto& vehicle =
			std::get<EnterVehicleCommand>(decoded[16].command);
		const auto& vehicleApproach =
			std::get<ApproachVehicleCommand>(decoded[17].command);
		const auto& worldItem =
			std::get<PickupWorldItemCommand>(decoded[18].command);
		const auto& steal =
			std::get<StealFromActorCommand>(decoded[19].command);
		const auto& exchange =
			std::get<ExchangePositionsCommand>(decoded[20].command);
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
			stop.source == SimulationCommandSource::System &&
			weaponMode.soldier == reusedSlot &&
			weaponMode.source == SimulationCommandSource::LocalPlayer &&
			scopeMode.soldier == firstIncarnation &&
			scopeMode.targetGrid == 3456 &&
			scopeMode.source == SimulationCommandSource::NetworkPeer &&
			reload.soldier == reusedSlot && !reload.reloadEvenIfNotEmpty &&
			reload.source == SimulationCommandSource::Replay &&
			traversal.soldier == firstIncarnation &&
			traversal.kind == TacticalTraversalKind::JumpWindow &&
			traversal.source == SimulationCommandSource::LocalPlayer &&
			activation.soldier == reusedSlot &&
			activation.object.grid == 4567 &&
			activation.object.structureId == 0x1234 &&
			activation.direction == 6 &&
			activation.source == SimulationCommandSource::NetworkPeer &&
			approach.soldier == firstIncarnation &&
			approach.object.grid == 5678 &&
			approach.object.structureId == 0x2345 &&
			approach.direction == 4 &&
			approach.destinationGrid == 5600 &&
			approach.movementMode == 6 && approach.reverse &&
			!approach.forceRestart &&
			approach.source == SimulationCommandSource::Replay &&
			conversation.soldier == firstIncarnation &&
			conversation.target == reusedSlot &&
			conversation.source == SimulationCommandSource::LocalPlayer &&
			conversationApproach.soldier == reusedSlot &&
			conversationApproach.target == firstIncarnation &&
			conversationApproach.destinationGrid == 6000 &&
			conversationApproach.movementMode == 6 &&
			conversationApproach.forceRestart &&
			conversationApproach.source == SimulationCommandSource::Replay &&
			vehicle.soldier == firstIncarnation &&
			vehicle.vehicle == reusedSlot && vehicle.direction == 2 &&
			vehicle.seatIndex == 3 &&
			vehicle.source == SimulationCommandSource::NetworkPeer &&
			vehicleApproach.soldier == reusedSlot &&
			vehicleApproach.vehicle == firstIncarnation &&
			vehicleApproach.direction == 5 &&
			vehicleApproach.seatIndex == 8 &&
			vehicleApproach.destinationGrid == 6200 &&
			vehicleApproach.movementMode == 6 &&
			!vehicleApproach.forceRestart &&
			vehicleApproach.source == SimulationCommandSource::System &&
			worldItem.soldier == firstIncarnation &&
			worldItem.item == TacticalWorldItemId{123456, 9876} &&
			worldItem.grid == 6400 && worldItem.renderHeight == 1 &&
			worldItem.kind ==
				TacticalWorldItemPickupKind::SpecificItem &&
			worldItem.source == SimulationCommandSource::LocalPlayer &&
			steal.soldier == firstIncarnation &&
			steal.target == reusedSlot &&
			steal.targetGrid == 6500 && steal.targetLevel == 1 &&
			steal.source == SimulationCommandSource::NetworkPeer &&
			exchange.soldier == reusedSlot &&
			exchange.target == firstIncarnation &&
			exchange.soldierGrid == 6600 &&
			exchange.targetGrid == 6601 && exchange.level == 0 &&
			exchange.source == SimulationCommandSource::System;
	}
	check(decodedFields,
		"current commands preserve movement, weapon, traversal, world-item, and peer-interaction intent");

	std::vector<RecordedSimulationCommand> unresolved = recorded;
	std::get<ChangeStanceCommand>(unresolved[0].command).soldier.incarnation = 0;
	std::vector<std::uint8_t> preservedEncoding{0xa5, 0x5a};
	check(!EncodeSimulationCommandJournal(unresolved, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects unresolved actor identities transactionally");
	std::vector<RecordedSimulationCommand> invalidFire = recorded;
	std::get<BeginFireWeaponCommand>(invalidFire[2].command).soldier =
		TacticalEntityId{};
	check(!EncodeSimulationCommandJournal(invalidFire, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding validates fire-command actors");
	std::vector<RecordedSimulationCommand> invalidMove = recorded;
	std::get<MoveToGridCommand>(invalidMove[3].command).soldier =
		TacticalEntityId{};
	check(!EncodeSimulationCommandJournal(invalidMove, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding validates move-command actors");
	invalidMove = recorded;
	std::get<MoveToGridCommand>(invalidMove[3].command).origin =
		static_cast<TacticalMoveOrigin>(0xff);
	check(!EncodeSimulationCommandJournal(invalidMove, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects unknown movement origins transactionally");
	invalidMove = recorded;
	std::get<MoveToGridCommand>(invalidMove[3].command).pendingAction =
		static_cast<TacticalPendingActionPolicy>(0xff);
	check(!EncodeSimulationCommandJournal(invalidMove, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects unknown pending-action policy transactionally");
	std::vector<RecordedSimulationCommand> invalidFacing = recorded;
	std::get<SetFacingCommand>(invalidFacing[5].command).direction =
		TacticalDirectionCount;
	check(!EncodeSimulationCommandJournal(invalidFacing, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects invalid tactical directions transactionally");
	std::vector<RecordedSimulationCommand> invalidTraversal = recorded;
	std::get<TraverseObstacleCommand>(invalidTraversal[11].command).kind =
		static_cast<TacticalTraversalKind>(0xff);
	check(!EncodeSimulationCommandJournal(
			invalidTraversal, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects unknown traversal kinds transactionally");
	std::vector<RecordedSimulationCommand> invalidActivation = recorded;
	std::get<ActivateWorldObjectCommand>(
		invalidActivation[12].command).direction = TacticalDirectionCount;
	check(!EncodeSimulationCommandJournal(
			invalidActivation, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects invalid world-object directions transactionally");
	std::vector<RecordedSimulationCommand> invalidApproach = recorded;
	std::get<ApproachWorldObjectCommand>(
		invalidApproach[13].command).direction = TacticalDirectionCount;
	check(!EncodeSimulationCommandJournal(
			invalidApproach, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects invalid approach directions transactionally");
	std::vector<RecordedSimulationCommand> invalidConversation = recorded;
	std::get<StartConversationCommand>(
		invalidConversation[14].command).target.incarnation = 0;
	check(!EncodeSimulationCommandJournal(
			invalidConversation, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects unresolved conversation targets transactionally");
	std::vector<RecordedSimulationCommand> invalidVehicle = recorded;
	std::get<EnterVehicleCommand>(
		invalidVehicle[16].command).seatIndex =
			TacticalMaximumVehicleSeats;
	check(!EncodeSimulationCommandJournal(
			invalidVehicle, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects out-of-range vehicle seats transactionally");
	std::vector<RecordedSimulationCommand> invalidWorldItemJournal = recorded;
	std::get<PickupWorldItemCommand>(
		invalidWorldItemJournal[18].command).item.incarnation = 0;
	check(!EncodeSimulationCommandJournal(
			invalidWorldItemJournal, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects unresolved world-item identities transactionally");
	std::vector<RecordedSimulationCommand> invalidWorldItemSearch = recorded;
	auto& invalidWorldItemSearchCommand =
		std::get<PickupWorldItemCommand>(
			invalidWorldItemSearch[18].command);
	invalidWorldItemSearchCommand.kind =
		TacticalWorldItemPickupKind::SearchGrid;
	check(!EncodeSimulationCommandJournal(
			invalidWorldItemSearch, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects search intent carrying a specific world-item identity");
	std::vector<RecordedSimulationCommand> invalidSteal = recorded;
	std::get<StealFromActorCommand>(
		invalidSteal[19].command).target.incarnation = 0;
	check(!EncodeSimulationCommandJournal(
			invalidSteal, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects unresolved steal targets transactionally");
	std::vector<RecordedSimulationCommand> invalidExchange = recorded;
	std::get<ExchangePositionsCommand>(
		invalidExchange[20].command).target =
			std::get<ExchangePositionsCommand>(
				invalidExchange[20].command).soldier;
	check(!EncodeSimulationCommandJournal(
			invalidExchange, 0, preservedEncoding) &&
		preservedEncoding == std::vector<std::uint8_t>{0xa5, 0x5a},
		"command encoding rejects self-exchange targets transactionally");

	std::vector<std::uint8_t> trailing = encoded;
	trailing.push_back(0xff);
	check(RejectsJournalWithoutPublishing(
		trailing, SimulationCommandJournalDecodeResult::Invalid),
		"JA2 command codec rejects trailing data without publishing partial output");

	std::vector<std::uint8_t> unsupportedVersion = encoded;
	unsupportedVersion[4] = 2;
	check(RejectsJournalWithoutPublishing(
		unsupportedVersion,
		SimulationCommandJournalDecodeResult::UnsupportedVersion),
		"command journals reject unsupported future wire versions");

	std::vector<RecordedSimulationCommand> oneStance{recorded[0]};
	std::vector<std::uint8_t> malformedStance;
	const bool encodedStanceFixture =
		EncodeSimulationCommandJournal(oneStance, 0, malformedStance) &&
		malformedStance.size() == 44;
	std::vector<std::uint8_t> invalidStatus = malformedStance;
	std::vector<std::uint8_t> invalidTag = malformedStance;
	std::vector<std::uint8_t> invalidSourceBytes = malformedStance;
	std::vector<std::uint8_t> truncatedStance = malformedStance;
	if (encodedStanceFixture)
	{
		malformedStance[38] = 0;
		malformedStance[39] = 0;
		malformedStance[40] = 0;
		malformedStance[41] = 0;
		invalidStatus[34] = 0xff;
		invalidTag[35] = 0xff;
		invalidSourceBytes[43] = 0xff;
		truncatedStance.pop_back();
	}
	check(encodedStanceFixture &&
		RejectsJournalWithoutPublishing(
			malformedStance, SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			invalidStatus, SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			invalidTag, SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			invalidSourceBytes, SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			truncatedStance, SimulationCommandJournalDecodeResult::Invalid),
		"the current command format rejects malformed record identity, status, tag, source, and length");

	std::vector<RecordedSimulationCommand> oneMove{recorded[3]};
	std::vector<std::uint8_t> malformedMove;
	const bool encodedMoveFixture =
		EncodeSimulationCommandJournal(oneMove, 0, malformedMove) &&
		malformedMove.size() == 52;
	check(encodedMoveFixture,
		"the current move fixture encodes all stable value fields");
	if (encodedMoveFixture)
	{
		malformedMove[48] = 0x80;
	}
	check(RejectsJournalWithoutPublishing(
		malformedMove, SimulationCommandJournalDecodeResult::Invalid),
		"move decoding rejects unknown packed flags transactionally");
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
		"move decoding rejects unknown movement origins transactionally");
	check(encodedMoveFixture && RejectsJournalWithoutPublishing(
		malformedPendingAction, SimulationCommandJournalDecodeResult::Invalid),
		"move decoding rejects unknown pending-action policy transactionally");

	std::vector<RecordedSimulationCommand> oneFacing{recorded[5]};
	std::vector<RecordedSimulationCommand> oneStealth{recorded[6]};
	std::vector<std::uint8_t> malformedFacing;
	std::vector<std::uint8_t> malformedStealth;
	const bool encodedNewCommands =
		EncodeSimulationCommandJournal(oneFacing, 0, malformedFacing) &&
		EncodeSimulationCommandJournal(oneStealth, 0, malformedStealth) &&
		malformedFacing.size() == 44 && malformedStealth.size() == 44;
	if (encodedNewCommands)
	{
		malformedFacing[42] = TacticalDirectionCount;
		malformedStealth[42] = 2;
	}
	check(encodedNewCommands && RejectsJournalWithoutPublishing(
		malformedFacing, SimulationCommandJournalDecodeResult::Invalid),
		"facing decoding rejects invalid directions transactionally");
	check(encodedNewCommands && RejectsJournalWithoutPublishing(
		malformedStealth, SimulationCommandJournalDecodeResult::Invalid),
		"stealth decoding rejects malformed booleans transactionally");

	std::vector<RecordedSimulationCommand> oneReload{recorded[10]};
	std::vector<std::uint8_t> malformedReload;
	const bool encodedWeaponControlCommands =
		EncodeSimulationCommandJournal(oneReload, 0, malformedReload) &&
		malformedReload.size() == 44;
	if (encodedWeaponControlCommands)
	{
		malformedReload[42] = 2;
	}
	check(encodedWeaponControlCommands && RejectsJournalWithoutPublishing(
		malformedReload, SimulationCommandJournalDecodeResult::Invalid),
		"reload decoding rejects malformed booleans transactionally");

	std::vector<RecordedSimulationCommand> oneTraversal{recorded[11]};
	std::vector<std::uint8_t> encodedTraversal;
	std::vector<std::uint8_t> malformedTraversalKind;
	const bool encodedTraversalCommand =
		EncodeSimulationCommandJournal(
			oneTraversal, 0, encodedTraversal) &&
		encodedTraversal.size() == 44;
	if (encodedTraversalCommand)
	{
		malformedTraversalKind = encodedTraversal;
		malformedTraversalKind[42] = 0xff;
	}
	check(encodedTraversalCommand && RejectsJournalWithoutPublishing(
		malformedTraversalKind, SimulationCommandJournalDecodeResult::Invalid),
		"traversal decoding rejects unknown kinds transactionally");

	std::vector<RecordedSimulationCommand> oneActivation{recorded[12]};
	std::vector<RecordedSimulationCommand> oneApproach{recorded[13]};
	std::vector<std::uint8_t> encodedActivation;
	std::vector<std::uint8_t> malformedActivationDirection;
	std::vector<std::uint8_t> encodedApproach;
	std::vector<std::uint8_t> malformedApproachDirection;
	std::vector<std::uint8_t> malformedApproachFlags;
	const bool encodedWorldObjectCommands =
		EncodeSimulationCommandJournal(
			oneActivation, 0, encodedActivation) &&
		EncodeSimulationCommandJournal(
			oneApproach, 0, encodedApproach) &&
		encodedActivation.size() == 50 &&
		encodedApproach.size() == 57;
	if (encodedWorldObjectCommands)
	{
		malformedActivationDirection = encodedActivation;
		malformedApproachDirection = encodedApproach;
		malformedApproachFlags = encodedApproach;
		malformedActivationDirection[48] = TacticalDirectionCount;
		malformedApproachDirection[48] = TacticalDirectionCount;
		malformedApproachFlags[55] = 0x80;
	}
	check(encodedWorldObjectCommands &&
		RejectsJournalWithoutPublishing(
			malformedActivationDirection,
			SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			malformedApproachDirection,
			SimulationCommandJournalDecodeResult::Invalid),
		"world-object decoding rejects invalid directions transactionally");
	check(encodedWorldObjectCommands &&
		RejectsJournalWithoutPublishing(
			malformedApproachFlags,
			SimulationCommandJournalDecodeResult::Invalid),
		"approach decoding rejects unknown movement flags transactionally");

	std::vector<RecordedSimulationCommand> oneConversation{recorded[14]};
	std::vector<RecordedSimulationCommand> oneConversationApproach{recorded[15]};
	std::vector<RecordedSimulationCommand> oneVehicle{recorded[16]};
	std::vector<RecordedSimulationCommand> oneVehicleApproach{recorded[17]};
	std::vector<std::uint8_t> encodedConversation;
	std::vector<std::uint8_t> encodedConversationApproach;
	std::vector<std::uint8_t> encodedVehicle;
	std::vector<std::uint8_t> encodedVehicleApproach;
	const bool encodedEntityInteractionCommands =
		EncodeSimulationCommandJournal(
			oneConversation, 0, encodedConversation) &&
		EncodeSimulationCommandJournal(
			oneConversationApproach, 0, encodedConversationApproach) &&
		EncodeSimulationCommandJournal(
			oneVehicle, 0, encodedVehicle) &&
		EncodeSimulationCommandJournal(
			oneVehicleApproach, 0, encodedVehicleApproach) &&
		encodedConversation.size() == 49 &&
		encodedConversationApproach.size() == 56 &&
		encodedVehicle.size() == 51 &&
		encodedVehicleApproach.size() == 58;
	std::vector<std::uint8_t> unresolvedConversationTarget =
		encodedConversation;
	std::vector<std::uint8_t> malformedConversationApproach =
		encodedConversationApproach;
	std::vector<std::uint8_t> malformedVehicleDirection = encodedVehicle;
	std::vector<std::uint8_t> malformedVehicleSeat = encodedVehicle;
	std::vector<std::uint8_t> malformedVehicleApproach =
		encodedVehicleApproach;
	if (encodedEntityInteractionCommands)
	{
		for (std::size_t offset = 44; offset <= 47; ++offset)
			unresolvedConversationTarget[offset] = 0;
		malformedConversationApproach[54] = 2;
		malformedVehicleDirection[48] = TacticalDirectionCount;
		malformedVehicleSeat[49] = TacticalMaximumVehicleSeats;
		malformedVehicleApproach[56] = 2;
	}
	check(encodedEntityInteractionCommands &&
		RejectsJournalWithoutPublishing(
			unresolvedConversationTarget,
			SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			malformedConversationApproach,
			SimulationCommandJournalDecodeResult::Invalid),
		"conversation decoding rejects unresolved targets and malformed approach flags transactionally");
	check(encodedEntityInteractionCommands &&
		RejectsJournalWithoutPublishing(
			malformedVehicleDirection,
			SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			malformedVehicleSeat,
			SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			malformedVehicleApproach,
			SimulationCommandJournalDecodeResult::Invalid),
		"vehicle decoding rejects invalid directions, seats, and approach flags transactionally");

	std::vector<RecordedSimulationCommand> oneWorldItem{recorded[18]};
	std::vector<std::uint8_t> encodedWorldItem;
	const bool encodedWorldItemCommand =
		EncodeSimulationCommandJournal(
			oneWorldItem, 0, encodedWorldItem) &&
		encodedWorldItem.size() == 57;
	std::vector<std::uint8_t> unresolvedWorldItem = encodedWorldItem;
	std::vector<std::uint8_t> oversizedWorldItemSlot = encodedWorldItem;
	std::vector<std::uint8_t> malformedWorldItemKind = encodedWorldItem;
	if (encodedWorldItemCommand)
	{
		for (std::size_t offset = 46; offset <= 49; ++offset)
			unresolvedWorldItem[offset] = 0;
		oversizedWorldItemSlot[42] = 0;
		oversizedWorldItemSlot[43] = 0;
		oversizedWorldItemSlot[44] = 0;
		oversizedWorldItemSlot[45] = 0x80;
		malformedWorldItemKind[55] = 0xff;
	}
	check(encodedWorldItemCommand &&
		RejectsJournalWithoutPublishing(
			unresolvedWorldItem,
			SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			oversizedWorldItemSlot,
			SimulationCommandJournalDecodeResult::Invalid) &&
		RejectsJournalWithoutPublishing(
			malformedWorldItemKind,
			SimulationCommandJournalDecodeResult::Invalid),
		"world-item decoding rejects unresolved, oversized, and unknown pickup identities transactionally");
	std::vector<RecordedSimulationCommand> searchWorldItem = oneWorldItem;
	auto& searchWorldItemCommand =
		std::get<PickupWorldItemCommand>(searchWorldItem[0].command);
	searchWorldItemCommand.item = {};
	searchWorldItemCommand.kind =
		TacticalWorldItemPickupKind::SearchGrid;
	std::vector<std::uint8_t> encodedWorldItemSearch;
	std::vector<RecordedSimulationCommand> decodedWorldItemSearch;
	std::uint64_t droppedWorldItemSearch = 0;
	check(EncodeSimulationCommandJournal(
			searchWorldItem, 0, encodedWorldItemSearch) &&
		DecodeSimulationCommandJournal(
			encodedWorldItemSearch,
			decodedWorldItemSearch,
			droppedWorldItemSearch) ==
			SimulationCommandJournalDecodeResult::Success &&
		decodedWorldItemSearch.size() == 1 &&
		std::get<PickupWorldItemCommand>(
			decodedWorldItemSearch[0].command).item ==
			TacticalWorldItemId{} &&
		std::get<PickupWorldItemCommand>(
			decodedWorldItemSearch[0].command).kind ==
			TacticalWorldItemPickupKind::SearchGrid,
		"world-item search intent round-trips without inventing a specific identity");

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
