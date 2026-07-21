#include <Engine/Core/AssetSource.h>
#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/CommandStream.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/DefinitionCatalog.h>
#include <Engine/Core/EngineHost.h>
#include <Engine/Core/EntityRegistry.h>
#include <Engine/Core/FrameDriver.h>
#include <Engine/Core/LocalizationCatalog.h>
#include <Engine/Core/PackageRandomSource.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/SimulationTick.h>
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

class TestSimulationTickSink final : public SimulationTickSink
{
public:
	void simulate(const SimulationTickContext& tick) override
	{
		ticks.push_back(tick);
		if (throws) throw 1;
	}

	std::vector<SimulationTickContext> ticks;
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

class DeclaredContentPackage final : public EnginePackage
{
public:
	explicit DeclaredContentPackage(PackageDescriptor descriptor)
		: descriptor_(std::move(descriptor)) {}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		if (active_) return false;
		active_ = true;
		return true;
	}
	void deactivate() noexcept override { active_ = false; }

private:
	PackageDescriptor descriptor_;
	bool active_ = false;
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
	MemoryAssetSource metadataAssets("test.assets");
	metadataAssets.put("Data/Metadata.bin", {1, 2, 3});
	AssetMetadata metadata;
	check(metadataAssets.metadata("DATA\\METADATA.BIN", metadata) ==
			AssetMetadataResult::Success &&
		metadata.logicalPath == "data/metadata.bin" &&
		metadata.provenance == "test.assets" && metadata.byteSize == 3 &&
		metadataAssets.metadata("../invalid", metadata) ==
			AssetMetadataResult::InvalidPath && metadata.logicalPath.empty(),
		"asset metadata queries normalize paths without copying asset payloads");
	MemoryAssetSource cacheUpstream("test.cache");
	cacheUpstream.put("data/a.bin", {1, 2});
	cacheUpstream.put("data/b.bin", {3, 4});
	CachingAssetSource assetCache(cacheUpstream, 1, 4);
	AssetData cachedAsset;
	const AssetReadResult cacheMiss = assetCache.read("data/a.bin", cachedAsset);
	cacheUpstream.put("data/a.bin", {9});
	const AssetReadResult cacheHit = assetCache.read("DATA/A.BIN", cachedAsset);
	const std::vector<std::uint8_t> retainedBytes = cachedAsset.bytes;
	assetCache.read("data/b.bin", cachedAsset);
	const AssetCacheStatistics cacheStatistics = assetCache.statistics();
	check(cacheMiss == AssetReadResult::Success && cacheHit == AssetReadResult::Success &&
		retainedBytes == std::vector<std::uint8_t>({1, 2}) &&
		cacheStatistics.hits == 1 && cacheStatistics.misses == 2 &&
		cacheStatistics.insertions == 2 && cacheStatistics.evictions == 1 &&
		cacheStatistics.entries == 1 && cacheStatistics.bytes == 2,
		"bounded asset cache serves normalized hits and evicts least-recently-used payloads");
	LocalizationCatalog localization(2, 16);
	check(localization.set("campaign.base", "en", "ui.ready", "Ready") ==
			LocalizationSetError::None &&
		localization.set("mod.override", "en", "ui.ready", "Prepared") ==
			LocalizationSetError::None,
		"localization catalog accepts bounded ordered package layers");
	const LocalizedTextView localizedOverride = localization.resolve("nl", "ui.ready");
	check(localizedOverride && localizedOverride.usedFallback &&
		*localizedOverride.text == "Prepared" &&
		*localizedOverride.packageId == "mod.override" &&
		localization.set("mod.third", "en", "ui.other", "Other") ==
			LocalizationSetError::CapacityReached &&
		localization.removePackage("mod.override") == 1 &&
		*localization.resolve("en", "ui.ready").text == "Ready",
		"localization lookup uses explicit fallback and restores lower package layers");
	DefinitionCatalog definitions(2, 4);
	check(definitions.set("campaign.base", "item", "medkit", 1, {1}) ==
			DefinitionSetError::None &&
		definitions.set("mod.override", "item", "medkit", 2, {2, 3}) ==
			DefinitionSetError::None,
		"definition catalog accepts bounded versioned package data layers");
	const DefinitionView incompatibleDefinition =
		definitions.resolve("item", "medkit", 1, 1);
	const DefinitionView overriddenDefinition =
		definitions.resolve("item", "medkit", 1, 2);
	check(incompatibleDefinition.error == DefinitionLookupError::IncompatibleSchema &&
		incompatibleDefinition.schemaVersion == 2 && overriddenDefinition &&
		*overriddenDefinition.packageId == "mod.override" &&
		*overriddenDefinition.payload == std::vector<std::uint8_t>({2, 3}) &&
		definitions.removePackage("mod.override") == 1 &&
		*definitions.resolve("item", "medkit", 1, 1).payload ==
			std::vector<std::uint8_t>({1}),
		"definition lookup rejects incompatible top overrides and restores lower layers");
	EntityRegistry entities(1);
	const EntityCreateResult firstEntity = entities.create("campaign.base", "mercenary");
	const EntityDestroyError destroyedEntity = entities.destroy(firstEntity.id);
	const EntityCreateResult replacementEntity = entities.create("campaign.base", "mercenary");
	const std::vector<EntityRecordSnapshot> entitySnapshot = entities.snapshot();
	check(firstEntity && destroyedEntity == EntityDestroyError::None && replacementEntity &&
		replacementEntity.id.index == firstEntity.id.index &&
		replacementEntity.id.generation > firstEntity.id.generation &&
		!entities.alive(firstEntity.id) && entities.alive(replacementEntity.id) &&
		entitySnapshot.size() == 1 && entitySnapshot[0].kind == "mercenary" &&
		entities.create("campaign.base", "second").error ==
			EntityCreateError::CapacityReached,
		"entity registry reuses bounded slots without reviving stale generational handles");
	RecordingAudioOutput groupedAudioOutput;
	AudioGroupService audioGroups(groupedAudioOutput, 2);
	const PackageAudioPlayResult firstGroupedPlayback = audioGroups.play(
		"mod.audio", "ui",
		AudioPlaybackRequest{"Audio\\Clicks//Select.wav", 22050, 100, 64, 1, false});
	const PackageAudioPlayResult secondGroupedPlayback = audioGroups.play(
		"mod.audio", "ui",
		AudioPlaybackRequest{"audio/clicks/confirm.wav", 22050, 90, 64, 1, false});
	const PackageAudioOperationResult changedGroup =
		audioGroups.setGroupVolume("mod.audio", "ui", 80);
	check(firstGroupedPlayback && secondGroupedPlayback &&
		groupedAudioOutput.requests().size() == 2 &&
		groupedAudioOutput.requests()[0].asset == "audio/clicks/select.wav" &&
		!audioGroups.stop("other.package", firstGroupedPlayback.playback) &&
		changedGroup.matched == 2 && changedGroup.succeeded == 2 &&
		audioGroups.play("mod.audio", "ui",
			AudioPlaybackRequest{"audio/third.wav"}).error ==
			PackageAudioPlayError::CapacityReached,
		"package audio groups normalize assets, isolate owners, and enforce a live bound");
	const PackageAudioOperationResult releasedAudio = audioGroups.releasePackage("mod.audio");
	check(releasedAudio.matched == 2 && releasedAudio.succeeded == 2 &&
		audioGroups.size() == 0 &&
		!groupedAudioOutput.isPlaying(firstGroupedPlayback.playback) &&
		!groupedAudioOutput.isPlaying(secondGroupedPlayback.playback),
		"package audio teardown stops every playback owned by the package");
	PackageTaskQueue deferredTasks(2, 1);
	PackageTasks boundTasks("mod.tasks", deferredTasks);
	unsigned deferredRuns = 0;
	const PackageTaskScheduleResult firstTask = boundTasks.defer([&]
	{
		++deferredRuns;
		boundTasks.defer([&] { ++deferredRuns; });
	});
	const PackageTaskScheduleResult secondTask = boundTasks.defer([] { throw 1; });
	const PackageTaskDrainResult firstDrain = deferredTasks.drain();
	check(firstTask && secondTask && firstTask.sequence < secondTask.sequence &&
		firstDrain.attempted == 1 && firstDrain.executed == 1 &&
		firstDrain.deferred == 2 && deferredRuns == 1,
		"package task drains are bounded and defer recursively scheduled work");
	check(deferredTasks.removePackage("mod.tasks") == 2 && deferredTasks.size() == 0 &&
		deferredTasks.snapshot().summary.cancelled == 2,
		"package task teardown cancels every queued callback owned by the package");
	PackageRandomSource packageRandom("rules.ballistics", 12345, 2);
	PackageRandomSource replayRandom("rules.ballistics", 12345, 2);
	const PackageRandomResult firstCombat = packageRandom.next("combat", 1000);
	const PackageRandomResult unrelatedLoot = packageRandom.next("loot", 1000);
	const PackageRandomResult secondCombat = packageRandom.next("combat", 1000);
	const PackageRandomResult replayFirstCombat = replayRandom.next("combat", 1000);
	const PackageRandomResult replaySecondCombat = replayRandom.next("combat", 1000);
	const std::vector<PackageRandomStreamSnapshot> randomSnapshot = packageRandom.snapshot();
	check(firstCombat && unrelatedLoot && secondCombat && replayFirstCombat &&
		replaySecondCombat && firstCombat.value == replayFirstCombat.value &&
		secondCombat.value == replaySecondCombat.value &&
		!packageRandom.next("invalid/stream", 10) &&
		packageRandom.next("third", 10).error == PackageRandomError::StreamLimitReached &&
		randomSnapshot.size() == 2 && randomSnapshot[0].id == "combat" &&
		randomSnapshot[0].valuesGenerated == 2 && randomSnapshot[1].id == "loot",
		"package random streams are deterministic, isolated, bounded, and inspectable");
	RuntimeCapabilities capabilities;
	check(capabilities.add("engine.rendering") &&
		capabilities.add("tool.map-editor") &&
		!capabilities.add("engine.rendering") &&
		!capabilities.add("invalid/capability") &&
		capabilities.ids() == std::vector<std::string>({
			"engine.rendering", "tool.map-editor"}),
		"runtime capabilities are portable, unique, and deterministically ordered");
	PackageCatalogSnapshot fingerprintPackages;
	fingerprintPackages.supportedApi = ContentApiVersion{1, 3};
	fingerprintPackages.activationOrder = {"rules.fingerprint"};
	fingerprintPackages.packages.push_back(PackageCatalogEntry{
		PackageDescriptor{
			ContentManifest{"rules.fingerprint", "2.0", ContentApiVersion{1, 3}},
			PackageKind::Rules, {"rules.fingerprint"}},
		PackageLifecycleState::Active, false, 0, {}, {}});
	const std::vector<EngineServiceDescriptor> fingerprintServices{
		{"engine.test", EngineServiceVersion{1, 2}}};
	const std::vector<RuntimeConfigurationEntry> fingerprintConfiguration{
		{"engine.test-value", std::int64_t{42}}};
	const std::vector<DefinitionRecord> fingerprintDefinitions{
		{"rules.fingerprint", "item", "medkit", 1, {4, 2}}};
	const RuntimeCompatibilityFingerprint firstFingerprint =
		BuildRuntimeCompatibilityFingerprint(fingerprintPackages, fingerprintServices,
			fingerprintConfiguration, capabilities, fingerprintDefinitions);
	const RuntimeCompatibilityFingerprint repeatedFingerprint =
		BuildRuntimeCompatibilityFingerprint(fingerprintPackages, fingerprintServices,
			fingerprintConfiguration, capabilities, fingerprintDefinitions);
	auto changedDefinitions = fingerprintDefinitions;
	changedDefinitions[0].payload[1] = 3;
	const RuntimeCompatibilityFingerprint changedFingerprint =
		BuildRuntimeCompatibilityFingerprint(fingerprintPackages, fingerprintServices,
			fingerprintConfiguration, capabilities, changedDefinitions);
	check(firstFingerprint == repeatedFingerprint &&
		firstFingerprint != changedFingerprint && firstFingerprint.hex().size() == 40,
		"runtime fingerprints are deterministic and include versioned definition bytes");
	PackageTaskQueueSnapshot resourceTasks;
	resourceTasks.queued.push_back(PackageTaskRecord{7, "rules.fingerprint"});
	const PackageResourceUsageSnapshot resourceUsage = BuildPackageResourceUsage(
		fingerprintPackages,
		std::vector<LocalizationEntry>{{"rules.fingerprint", "en", "ui.ready", "Ready"}},
		fingerprintDefinitions,
		std::vector<EntityRecordSnapshot>{{EntityId{1, 1}, "rules.fingerprint", "unit"}},
		std::vector<PackageAudioPlaybackSnapshot>{{3, "rules.fingerprint", "ui", "a.wav", 90}},
		resourceTasks,
		std::vector<PackageRandomUsageSnapshot>{{"rules.fingerprint", 2, 11}});
	const PackageResourceUsage* packageUsage = resourceUsage.find("rules.fingerprint");
	check(packageUsage && packageUsage->active &&
		packageUsage->localizationEntries == 1 &&
		packageUsage->localizationTextBytes == 5 &&
		packageUsage->definitionEntries == 1 &&
		packageUsage->definitionPayloadBytes == 2 && packageUsage->entities == 1 &&
		packageUsage->audioPlaybacks == 1 && packageUsage->deferredTasks == 1 &&
		packageUsage->randomStreams == 2 && packageUsage->randomValuesGenerated == 11 &&
		resourceUsage.total.definitionEntries == 1 &&
		resourceUsage.unattributedRecords == 0,
		"package resource accounting attributes owned framework state and totals");

	DeclaredContentPackage declaredContent(PackageDescriptor{
		ContentManifest{"mod.declared-content", "1", ContentApiVersion{1, 4}},
		PackageKind::Extension, {}, {}, {}, {},
		{{"en", "localization/en.lang"}, {"nl", "localization/nl.lang"}},
		{{"item", "field-kit", 2, "definitions/items/field-kit.json"}}});
	EngineHost<unsigned> declaredContentHost;
	const PackageRegistrationError declaredContentRegistration =
		declaredContentHost.packages().registerPackage(declaredContent);
	const PackageCatalogSnapshot declaredContentSnapshot =
		declaredContentHost.packageCatalog();
	const PackageCatalogEntry* declaredContentCatalog =
		declaredContentSnapshot.find("mod.declared-content");
	check(declaredContentRegistration == PackageRegistrationError::None &&
		declaredContentCatalog &&
		declaredContentCatalog->descriptor.localizationSources.size() == 2 &&
		declaredContentCatalog->descriptor.localizationSources[1].locale == "nl" &&
		declaredContentCatalog->descriptor.definitionSources.size() == 1 &&
		declaredContentCatalog->descriptor.definitionSources[0].schemaVersion == 2,
		"content API 1.4 preserves declared package sources in catalog snapshots");
	DeclaredContentPackage oldDeclaredContent(PackageDescriptor{
		ContentManifest{"mod.old-content", "1", ContentApiVersion{1, 3}},
		PackageKind::Extension, {}, {}, {}, {},
		{{"en", "localization/en.lang"}}, {}});
	DeclaredContentPackage duplicateDeclaredContent(PackageDescriptor{
		ContentManifest{"mod.duplicate-content", "1", ContentApiVersion{1, 4}},
		PackageKind::Extension, {}, {}, {}, {}, {},
		{{"item", "same", 1, "definitions/one.bin"},
		 {"item", "same", 2, "definitions/two.bin"}}});
	EngineHost<unsigned> invalidDeclaredContentHost;
	check(invalidDeclaredContentHost.packages().registerPackage(oldDeclaredContent) ==
			PackageRegistrationError::InvalidManifest &&
		invalidDeclaredContentHost.packages().registerPackage(duplicateDeclaredContent) ==
			PackageRegistrationError::InvalidManifest,
		"declared sources require content API 1.4 and unique definition identities");

	EngineHost<unsigned> sessionHost;
	unsigned externalService = 42;
	const EngineServiceRegistrationError registeredService =
		sessionHost.serviceCatalog().registerService(
			"host.test-service", EngineServiceVersion{2, 3}, externalService);
	const EngineServiceLookupResult<unsigned> resolvedService =
		sessionHost.serviceCatalog().resolve<unsigned>(
			"host.test-service", EngineServiceVersion{2, 1});
	const RuntimeConfigurationSetError configuredValue =
		sessionHost.configuration().set("host.test-value", std::int64_t{42});
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
		sessionHost.serviceCatalog().size() == 13 &&
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
	const std::int64_t* resolvedConfiguration =
		sessionHost.configuration().find<std::int64_t>("host.test-value");
	check(configuredValue == RuntimeConfigurationSetError::None &&
		resolvedConfiguration && *resolvedConfiguration == 42 &&
		sessionHost.configuration().sealed() &&
		sessionHost.configuration().set("host.test-value", std::int64_t{43}) ==
			RuntimeConfigurationSetError::Sealed &&
		sessionHost.configuration().size() == 18,
		"runtime configuration publishes typed stable values and seals before bootstrap");
	const RuntimeDiagnosticsSnapshot diagnostics = sessionHost.diagnostics();
	check(diagnostics.lifecycle == EngineLifecycle::Stopped &&
		diagnostics.frames.summary.completedFrames == 0 &&
		diagnostics.packages.packages.empty() && diagnostics.faults.records.empty() &&
		diagnostics.localization.empty() && diagnostics.definitions.empty() &&
		diagnostics.entities.empty() && diagnostics.packageAudio.empty() &&
		diagnostics.packageTasks.queued.empty() &&
		diagnostics.packageResources.packages.empty() && diagnostics.services.size() == 13 &&
		diagnostics.configuration.size() == 18 &&
		diagnostics.compatibility == sessionHost.compatibilityFingerprint() &&
		diagnostics.queuedMessages == 0 &&
		diagnostics.completedFrames == 0 && diagnostics.completedSimulationTicks == 0,
		"runtime diagnostics capture one pointer-free ordered host snapshot");

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
	SimulationTickDispatcher simulationTicks(10, 2);
	TestSimulationTickSink receivingTicks;
	check(simulationTicks.addSink(receivingTicks) ==
		SimulationTickSinkRegistrationError::None,
		"fixed-step simulation accepts deterministic non-owning subscribers");
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
		frameServices, runtimeMessages, inputDispatcher, runtimeUpdates, frameTelemetry,
		simulationTicks);
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
	check(skippedFrame.simulationTicks.scheduled == 4 &&
		skippedFrame.simulationTicks.executed == 2 &&
		skippedFrame.simulationTicks.dropped == 2 &&
		skippedFrame.simulationTicks.delivered == 2 &&
		receivingTicks.ticks.size() == 2 &&
		receivingTicks.ticks[0].sequence == 1 &&
		receivingTicks.ticks[1].simulatedTimeMicroseconds == 20 &&
		simulationTicks.completedTickSequence() == 4,
		"frame driver bounds fixed-step catch-up and reports discarded simulation work");
	const FrameTelemetrySnapshot telemetrySnapshot = frameTelemetry.snapshot();
	check(telemetrySnapshot.summary.completedFrames == 2 &&
		telemetrySnapshot.summary.presentedFrames == 1 &&
		telemetrySnapshot.summary.maximumFrameMicroseconds == 40 &&
		telemetrySnapshot.summary.inputCallbackFailures == 2 &&
		telemetrySnapshot.summary.runtimeUpdateCallbackFailures == 2 &&
		telemetrySnapshot.summary.simulationTicksDropped == 2 &&
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
	MemoryByteStorage checkpointStorage;
	PersistenceService checkpointPersistence(checkpointStorage, 4096);
	RuntimeCheckpointService checkpoints(checkpointPersistence, 1);
	const RuntimeCheckpoint savedCheckpoint{
		firstFingerprint, 17, 23, {{"rules.fingerprint", "2.0"}}};
	check(checkpoints.save("runtime.checkpoint", savedCheckpoint) ==
			RuntimeCheckpointSaveError::None,
		"runtime checkpoint service writes a bounded integrity-checked manifest");
	RuntimeCheckpoint loadedCheckpoint;
	const RuntimeCheckpointLoadResult loadedCheckpointResult = checkpoints.load(
		"runtime.checkpoint", firstFingerprint, loadedCheckpoint);
	check(loadedCheckpointResult &&
		loadedCheckpoint.compatibility == firstFingerprint &&
		loadedCheckpoint.completedFrames == 17 &&
		loadedCheckpoint.completedSimulationTicks == 23 &&
		loadedCheckpoint.activePackages.size() == 1 &&
		loadedCheckpoint.activePackages[0].id == "rules.fingerprint" &&
		loadedCheckpoint.activePackages[0].version == "2.0",
		"runtime checkpoint loads publish complete portable session metadata");
	RuntimeCheckpoint unchangedCheckpoint{
		firstFingerprint, 99, 99, {{"unchanged.package", "1"}}};
	const RuntimeCheckpointLoadResult incompatibleCheckpoint = checkpoints.load(
		"runtime.checkpoint", changedFingerprint, unchangedCheckpoint);
	check(incompatibleCheckpoint.error == RuntimeCheckpointLoadError::IncompatibleRuntime &&
		incompatibleCheckpoint.storedCompatibility == firstFingerprint &&
		unchangedCheckpoint.completedFrames == 99 &&
		unchangedCheckpoint.activePackages[0].id == "unchanged.package",
		"runtime checkpoint rejects incompatible engines before publishing metadata");

	ContentRegistry content(ContentApiVersion{1, 2});
	check(content.registerContent(ContentManifest{
		"engine.test", "1", ContentApiVersion{1, 0}, {}}) ==
		ContentRegistrationError::None,
		"compiled core registry links without game or platform libraries");

	return failures == 0 ? 0 : 1;
}
