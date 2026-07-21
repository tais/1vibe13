#include <Engine/Core/AssetSource.h>
#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/CommandStream.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/EngineHost.h>
#include <Engine/Core/FrameDriver.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/StateRegistry.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
int failures = 0;

class TestInputSink final : public InputEventSink
{
public:
	void receiveInput(const EngineInputEvent& event) override
	{
		events.push_back(event);
		if (throws) throw 1;
	}

	std::vector<EngineInputEvent> events;
	bool throws = false;
};

class TestRuntimeUpdateSink final : public RuntimeUpdateSink
{
public:
	void updateRuntime(const RuntimeUpdateContext& context) override
	{
		updates.push_back(context);
		if (throws) throw 1;
	}

	std::vector<RuntimeUpdateContext> updates;
	bool throws = false;
};

class TestMessageSink final : public RuntimeMessageSink
{
public:
	void receiveMessage(const RuntimeMessage& message) override
	{
		messages.push_back(message);
		if (publishReply && bus)
		{
			publishReply = false;
			bus->publish(RuntimeMessageRequest{"engine.reply", "engine.test", {2}});
		}
		if (throws) throw 1;
	}

	RuntimeMessageBus* bus = nullptr;
	std::vector<RuntimeMessage> messages;
	bool publishReply = false;
	bool throws = false;
};

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
}

int main()
{
	std::string path;
	check(NormalizeAssetPath("TableData\\Items.XML", path) &&
		path == "tabledata/items.xml",
		"compiled core normalizes portable asset paths");
	check(!NormalizeAssetPath("../Data/secret", path),
		"compiled core rejects traversal paths");
	RuntimeCapabilities capabilities;
	check(capabilities.add("engine.rendering") &&
		capabilities.add("tool.map-editor") &&
		!capabilities.add("engine.rendering") &&
		!capabilities.add("invalid/capability") &&
		capabilities.ids() == std::vector<std::string>({
			"engine.rendering", "tool.map-editor"}),
		"runtime capabilities are portable, unique, and deterministically ordered");

	EngineHost<unsigned> sessionHost;
	unsigned externalService = 42;
	const EngineServiceRegistrationError registeredService =
		sessionHost.serviceCatalog().registerService(
			"host.test-service", EngineServiceVersion{2, 3}, externalService);
	const EngineServiceLookupResult<unsigned> resolvedService =
		sessionHost.serviceCatalog().resolve<unsigned>(
			"host.test-service", EngineServiceVersion{2, 1});
	const RuntimeSessionShutdownResult prematureSessionShutdown =
		sessionHost.runtimeSession().shutdownPackages();
	const RuntimeSessionAdvanceResult configuredSession =
		sessionHost.runtimeSession().advancePackagesTo(PackageBootstrapPhase::Configure);
	check(!prematureSessionShutdown &&
		prematureSessionShutdown.error == RuntimeSessionError::InvalidState &&
		configuredSession && configuredSession.packages.completedPhases == 1 &&
		sessionHost.beginInitialization() &&
		sessionHost.runtimeSession().advancePackagesTo(PackageBootstrapPhase::StartRuntime) &&
		sessionHost.markRunning() && sessionHost.beginShutdown() &&
		sessionHost.runtimeSession().shutdownPackages() && sessionHost.markStopped(),
		"runtime session coordinates package phases with orderly host transitions");
	check(registeredService == EngineServiceRegistrationError::None &&
		resolvedService && resolvedService.service == &externalService &&
		resolvedService.availableVersion.minor == 3 &&
		sessionHost.serviceCatalog().size() == 4 &&
		sessionHost.serviceCatalog().sealed() &&
		sessionHost.serviceCatalog().registerService(
			"host.too-late", EngineServiceVersion{1, 0}, externalService) ==
			EngineServiceRegistrationError::Sealed &&
		sessionHost.serviceCatalog().resolve<std::string>(
			"host.test-service", EngineServiceVersion{2, 0}).error ==
			EngineServiceLookupError::TypeMismatch &&
		sessionHost.serviceCatalog().resolve<unsigned>(
			"host.test-service", EngineServiceVersion{3, 0}).error ==
			EngineServiceLookupError::IncompatibleVersion,
		"service catalog seals versioned type-checked host extensions before bootstrap");

	CommandStream<std::string> commandStream(8);
	check(commandStream.submit(4, "live") == 0 &&
		commandStream.submitRecorded(3, 7, "recorded") &&
		!commandStream.submitRecorded(5, 7, "duplicate"),
		"generic command stream assigns and protects deterministic sequences");
	const std::vector<ScheduledCommand<std::string>> stagedCommands{
		{6, 8, "batch-a"}, {6, 9, "batch-b"}};
	check(commandStream.stageRecordedBatch(stagedCommands) &&
		commandStream.journal().size() == 4 &&
		commandStream.queue().size() == 4,
		"generic command stream stages a complete batch with matching journal records");
	const std::vector<ScheduledCommand<std::string>> conflictingCommands{
		{8, 10, "would-stage"}, {8, 7, "conflict"}};
	check(!commandStream.stageRecordedBatch(conflictingCommands) &&
		commandStream.journal().size() == 4 &&
		commandStream.queue().size() == 4,
		"generic command stream rejects a conflicting batch transactionally");

	StateRegistry<unsigned> states;
	unsigned initialized = 0;
	unsigned handled = 0;
	unsigned shutDown = 0;
	check(states.registerState(4, StateCallbacks<unsigned>{
		[&initialized] { ++initialized; return true; },
		[&handled] { ++handled; return 7u; },
		[&shutDown] { ++shutDown; }}) == StateRegistrationError::None &&
		states.registerState(4, StateCallbacks<unsigned>{
			[] { return true; }, [] { return 0u; }, [] {}}) ==
			StateRegistrationError::DuplicateId &&
		states.registerState(5, StateCallbacks<unsigned>{}) ==
			StateRegistrationError::InvalidCallbacks,
		"state registry validates complete unique state contracts");
	check(states.handle(4).error == StateHandleError::NotInitialized &&
		states.initialize(4) == StateInitializationError::None &&
		states.initialize(4) == StateInitializationError::AlreadyInitialized &&
		states.initializedCount() == 1,
		"state registry enforces initialization before deterministic dispatch");
	const StateHandleResult<unsigned> handledState = states.handle(4);
	check(handledState && *handledState.nextState == 7 &&
		initialized == 1 && handled == 1,
		"state registry dispatches through its application-owned callback adapter");
	check(states.shutdown(4) == StateShutdownError::None &&
		states.shutdown(4) == StateShutdownError::NotInitialized &&
		shutDown == 1 && states.initializedCount() == 0,
		"state registry tracks orderly shutdown without owning captured resources");
	check(states.registerState(6, StateCallbacks<unsigned>{
		[]() -> bool { throw 1; }, [] { return 6u; }, [] {}}) ==
			StateRegistrationError::None &&
		states.initialize(6) == StateInitializationError::CallbackException,
		"state registry contains initialization exceptions at the host boundary");
	check(states.registerState(7, StateCallbacks<unsigned>{
		[] { return true; }, []() -> unsigned { throw 1; }, [] { throw 1; }}) ==
			StateRegistrationError::None &&
		states.initialize(7) == StateInitializationError::None &&
		states.handle(7).error == StateHandleError::CallbackException &&
		states.shutdown(7) == StateShutdownError::CallbackException &&
		!states.isInitialized(7),
		"state registry reports callback exceptions and releases lifecycle state");

	ManualTimeSource frameTime;
	MemoryInputSource frameInput;
	RecordingFramePresenter framePresenter;
	EngineServices frameServices{
		frameTime, ZeroRandomSource::instance(), NullByteStorage::instance(),
		NullLogSink::instance(), frameInput,
		NullAudioOutput::instance(), framePresenter, NullAssetSource::instance()};
	InputDispatcher inputDispatcher(frameInput, 1);
	TestInputSink receivingInput;
	TestInputSink throwingInput;
	throwingInput.throws = true;
	check(inputDispatcher.addSink(receivingInput) == InputSinkRegistrationError::None &&
		inputDispatcher.addSink(throwingInput) == InputSinkRegistrationError::None &&
		inputDispatcher.addSink(receivingInput) == InputSinkRegistrationError::Duplicate,
		"input dispatcher retains deterministic unique subscribers");
	frameInput.push(EngineInputEvent{10, 0, 1, 65, 0, 1, 3});
	frameInput.push(EngineInputEvent{20, 0, 2, 65, 0, 2, 0});
	RuntimeUpdateDispatcher runtimeUpdates;
	FrameTelemetry frameTelemetry(1);
	RuntimeMessageBus runtimeMessages(4, 8);
	TestMessageSink receivingMessages;
	receivingMessages.bus = &runtimeMessages;
	receivingMessages.publishReply = true;
	TestMessageSink throwingMessages;
	throwingMessages.throws = true;
	check(runtimeMessages.addSink(receivingMessages) ==
		RuntimeMessageSinkRegistrationError::None &&
		runtimeMessages.addSink(throwingMessages) ==
		RuntimeMessageSinkRegistrationError::None &&
		runtimeMessages.publish(RuntimeMessageRequest{
			"engine.ready", "engine.test", {1}}).sequence == 1 &&
		!runtimeMessages.publish(RuntimeMessageRequest{
			"invalid/topic", "engine.test", {}}),
		"runtime message bus validates publishers and retains deterministic sinks");
	TestRuntimeUpdateSink receivingUpdates;
	TestRuntimeUpdateSink throwingUpdates;
	throwingUpdates.throws = true;
	check(runtimeUpdates.addSink(receivingUpdates) ==
		RuntimeUpdateSinkRegistrationError::None &&
		runtimeUpdates.addSink(throwingUpdates) ==
		RuntimeUpdateSinkRegistrationError::None &&
		runtimeUpdates.addSink(receivingUpdates) ==
		RuntimeUpdateSinkRegistrationError::Duplicate,
		"runtime update dispatcher retains deterministic unique subscribers");
	FrameDriver frameDriver(
		frameServices, runtimeMessages, inputDispatcher, runtimeUpdates, frameTelemetry);
	unsigned frameOrder = 0;
	const FrameRunResult presentedFrame = frameDriver.runFrame(
		[&] {
			check(frameOrder++ == 0, "frame driver begins with application update");
			frameTime.advanceMicroseconds(25);
			return FramePlan{true, FramePresentMode::Immediate};
		},
		[&] {
			check(framePresenter.presentations().size() == 1 && frameOrder++ == 1,
				"frame driver presents before application completion");
			frameTime.advanceMicroseconds(15);
		});
	check(presentedFrame.sequence == 1 && presentedFrame.presented &&
		presentedFrame.presentationMode == FramePresentMode::Immediate &&
		presentedFrame.startedAtMicroseconds == 0 &&
		presentedFrame.finishedAtMicroseconds == 40 && frameOrder == 2,
		"frame driver reports deterministic frame identity and timing");
	check(presentedFrame.messages.messages == 1 &&
		presentedFrame.messages.delivered == 1 &&
		presentedFrame.messages.callbackFailures == 1 &&
		presentedFrame.messages.queuedForNextDispatch == 1 &&
		receivingMessages.messages[0].sequence == 1,
		"frame driver defers messages published during dispatch to the next frame");
	check(presentedFrame.input.polled == 1 && presentedFrame.input.delivered == 1 &&
		presentedFrame.input.callbackFailures == 1 &&
		presentedFrame.input.sourceDrops == 3 && presentedFrame.input.limitReached &&
		receivingInput.events.size() == 1 && throwingInput.events.size() == 1,
		"frame driver dispatches bounded input before update and isolates subscriber failures");
	check(presentedFrame.runtimeUpdates.delivered == 1 &&
		presentedFrame.runtimeUpdates.callbackFailures == 1 &&
		receivingUpdates.updates.size() == 1 &&
		receivingUpdates.updates[0].frameSequence == 1 &&
		receivingUpdates.updates[0].startedAtMicroseconds == 0 &&
		receivingUpdates.updates[0].elapsedSincePreviousFrameMicroseconds == 0,
		"frame driver dispatches deterministic runtime updates before application work");
	const FrameRunResult skippedFrame = frameDriver.runFrame(
		[] { return FramePlan{false, FramePresentMode::Paced}; }, [] {});
	check(skippedFrame.sequence == 2 && !skippedFrame.presented &&
		framePresenter.presentations().size() == 1 &&
		frameDriver.completedFrames() == 2 && skippedFrame.input.polled == 1 &&
		receivingInput.events.size() == 2 && receivingInput.events.back().type == 2 &&
		receivingUpdates.updates.size() == 2 &&
		receivingUpdates.updates.back().frameSequence == 2 &&
		receivingUpdates.updates.back().elapsedSincePreviousFrameMicroseconds == 40,
		"frame driver preserves skipped-frame policy without presenting");
	check(skippedFrame.messages.messages == 1 &&
		skippedFrame.messages.queuedForNextDispatch == 0 &&
		receivingMessages.messages.size() == 2 &&
		receivingMessages.messages.back().sequence == 2,
		"runtime message delivery preserves publication sequence across frames");
	const FrameTelemetrySnapshot telemetrySnapshot = frameTelemetry.snapshot();
	check(telemetrySnapshot.summary.completedFrames == 2 &&
		telemetrySnapshot.summary.presentedFrames == 1 &&
		telemetrySnapshot.summary.maximumFrameMicroseconds == 40 &&
		telemetrySnapshot.summary.inputCallbackFailures == 2 &&
		telemetrySnapshot.summary.runtimeUpdateCallbackFailures == 2 &&
		telemetrySnapshot.summary.messageCallbackFailures == 2 &&
		telemetrySnapshot.summary.messagesDelivered == 2 &&
		telemetrySnapshot.summary.evictedSamples == 1 &&
		telemetrySnapshot.samples.size() == 1 &&
		telemetrySnapshot.samples[0].sequence == 2,
		"frame telemetry retains bounded live timings and aggregate failures");

	BinaryWriter writer;
	WritePersistenceHeader(writer, PersistenceHeader{0x4A413243u, 7});
	writer.writeU32(0x10203040u);
	BinaryReader reader(writer.bytes());
	PersistenceHeader header{};
	std::uint32_t payload = 0;
	check(ReadPersistenceHeader(reader, 0x4A413243u, 6, 7, header) &&
		reader.readU32(payload) && payload == 0x10203040u && reader.remaining() == 0,
		"compiled core preserves versioned little-endian archives");

	MemoryByteStorage persistenceStorage;
	PersistenceService persistence(persistenceStorage, 8);
	const std::vector<std::uint8_t> persisted{1, 3, 3, 7};
	check(persistence.saveEnvelope(
		"engine.record", PersistenceHeader{0x454E4750u, 2}, persisted) ==
		PersistenceSaveResult::Success,
		"compiled persistence writes bounded checksummed envelopes");
	PersistenceHeader persistedHeader{};
	std::vector<std::uint8_t> loadedPersisted;
	check(persistence.loadEnvelope("engine.record", 0x454E4750u, 1, 2,
		persistedHeader, loadedPersisted) == PersistenceLoadResult::Success &&
		persistedHeader.version == 2 && loadedPersisted == persisted,
		"compiled persistence validates and publishes complete envelopes");
	std::vector<std::uint8_t> corrupted;
	persistenceStorage.readAll("engine.record", corrupted);
	corrupted.back() ^= 0xffu;
	persistenceStorage.writeAll("engine.corrupt", corrupted);
	PersistenceHeader unchangedHeader{99, 99};
	std::vector<std::uint8_t> unchangedPayload{9};
	check(persistence.loadEnvelope("engine.corrupt", 0x454E4750u, 1, 2,
		unchangedHeader, unchangedPayload) == PersistenceLoadResult::IntegrityFailure &&
		unchangedHeader.magic == 99 && unchangedHeader.version == 99 &&
		unchangedPayload == std::vector<std::uint8_t>({9}),
		"failed envelope loads leave caller state unchanged");
	check(persistence.saveEnvelope("too-large", PersistenceHeader{1, 1},
		std::vector<std::uint8_t>(9, 0)) == PersistenceSaveResult::TooLarge,
		"compiled persistence rejects payloads above the configured bound");

	ContentRegistry content(ContentApiVersion{1, 2});
	check(content.registerContent(ContentManifest{
		"engine.test", "1", ContentApiVersion{1, 0}, {}}) ==
		ContentRegistrationError::None,
		"compiled core registry links without game or platform libraries");

	return failures == 0 ? 0 : 1;
}
