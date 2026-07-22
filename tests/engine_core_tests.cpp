#include <Engine/Core/AssetSource.h>
#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/CommandStream.h>
#include <Engine/Core/ContentApi.h>
#include <Engine/Core/DefinitionCatalog.h>
#include <Engine/Core/EngineHost.h>
#include <Engine/Core/EngineHostOptions.h>
#include <Engine/Core/EntityRegistry.h>
#include <Engine/Core/FrameDriver.h>
#include <Engine/Core/LocalizationCatalog.h>
#include <Engine/Core/LocalizationDocument.h>
#include <Engine/Core/PackageEntities.h>
#include <Engine/Core/PackageRandomSource.h>
#include <Engine/Core/PackageContentLoader.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/RuntimeReportJson.h>
#include <Engine/Core/SimulationTick.h>
#include <Engine/Core/StateRegistry.h>

#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
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

// Models a third-party ByteStorage implementation compiled against the legacy
// readAll contract. It deliberately does not override readAllBounded so this
// test protects the extension's source-compatible fallback behavior.
class LegacyOnlyByteStorage final : public ByteStorage
{
public:
	explicit LegacyOnlyByteStorage(std::vector<std::uint8_t> bytes)
		: bytes_(std::move(bytes)) {}

	bool exists(const std::string& path) const override
	{
		return path == "legacy.record";
	}
	bool readAll(const std::string& path,
		std::vector<std::uint8_t>& bytes) const override
	{
		++readAllCalls;
		if (!exists(path)) return false;
		bytes = bytes_;
		return true;
	}
	bool writeAll(const std::string&,
		const std::vector<std::uint8_t>&) override
	{
		return false;
	}

	mutable std::size_t readAllCalls = 0;

private:
	std::vector<std::uint8_t> bytes_;
};

class BoundedProbeByteStorage final : public ByteStorage
{
public:
	explicit BoundedProbeByteStorage(std::vector<std::uint8_t> bytes)
		: bytes_(std::move(bytes)) {}

	bool exists(const std::string& path) const override
	{
		++existsCalls;
		return path == "bounded.record" || path == "storage.error";
	}
	bool readAll(const std::string&,
		std::vector<std::uint8_t>&) const override
	{
		++readAllCalls;
		return false;
	}
	ByteStorageReadResult readAllBounded(const std::string& path,
		std::size_t maximumBytes,
		std::vector<std::uint8_t>& bytes) const override
	{
		++boundedReadCalls;
		if (path == "storage.error") return ByteStorageReadResult::StorageError;
		if (path != "bounded.record") return ByteStorageReadResult::NotFound;
		if (bytes_.size() > maximumBytes) return ByteStorageReadResult::TooLarge;
		bytes = bytes_;
		return ByteStorageReadResult::Success;
	}
	bool writeAll(const std::string&,
		const std::vector<std::uint8_t>&) override
	{
		return false;
	}

	mutable std::size_t existsCalls = 0;
	mutable std::size_t readAllCalls = 0;
	mutable std::size_t boundedReadCalls = 0;

private:
	std::vector<std::uint8_t> bytes_;
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
	// Use real UTF-8 in the format; backslash-u is deliberately not a second
	// competing Unicode escape language.
	const std::string utf8LocalizationText =
		"JA2-LOCALIZATION 1\nui.ready = R\xc3\xa9" "ady\\nNow\nui.eq = A\\=B\n";
	std::vector<LocalizationDocumentEntry> localizationDocument;
	const LocalizationDocumentResult parsedLocalization = ParseLocalizationDocument(
		std::vector<std::uint8_t>(utf8LocalizationText.begin(), utf8LocalizationText.end()),
		localizationDocument);
	check(parsedLocalization && localizationDocument.size() == 2 &&
		localizationDocument[0].key == "ui.ready" &&
		localizationDocument[0].text == "R\xc3\xa9" "ady\nNow" &&
		localizationDocument[1].text == "A=B",
		"localization documents decode bounded UTF-8 package strings and escapes");
	const std::vector<LocalizationDocumentEntry> retainedLocalization = localizationDocument;
	const std::string duplicateLocalizationText =
		"JA2-LOCALIZATION 1\nui.same = First\nui.same = Second\n";
	const LocalizationDocumentResult rejectedLocalization = ParseLocalizationDocument(
		std::vector<std::uint8_t>(duplicateLocalizationText.begin(), duplicateLocalizationText.end()),
		localizationDocument);
	check(rejectedLocalization.error == LocalizationDocumentError::DuplicateKey &&
		rejectedLocalization.line == 3 &&
		localizationDocument.size() == retainedLocalization.size() &&
		localizationDocument[0].text == retainedLocalization[0].text,
		"localization document failures report their line and preserve caller state");
	MemoryAssetSource declaredAssets("mod.content-loader");
	declaredAssets.put("localization/en.lang", std::vector<std::uint8_t>{
		'J','A','2','-','L','O','C','A','L','I','Z','A','T','I','O','N',' ','1','\n',
		'u','i','.','r','e','a','d','y',' ','=',' ','R','e','a','d','y','\n'});
	declaredAssets.put("definitions/item.json", {'{','}','\n'});
	LocalizationCatalog loadedLocalization;
	DefinitionCatalog loadedDefinitions;
	PackageLocalization packageLocalization("mod.content-loader", loadedLocalization);
	PackageDefinitions packageDefinitions("mod.content-loader", loadedDefinitions);
	const PackageDescriptor loadableDescriptor{
		ContentManifest{"mod.content-loader", "1", ContentApiVersion{1, 4}},
		PackageKind::Extension, {}, {}, {}, {},
		{{"en", "localization/en.lang"}},
		{{"item", "field-kit", 1, "definitions/item.json"}}};
	const PackageContentLoadResult loadedContent = LoadDeclaredPackageContent(
		loadableDescriptor, declaredAssets, packageLocalization, packageDefinitions);
	check(loadedContent && loadedLocalization.resolve("en", "ui.ready") &&
		loadedDefinitions.resolve("item", "field-kit", 1, 1),
		"core declared-content loader imports localization and opaque definitions");
	MemoryAssetSource failingDeclaredAssets("mod.content-loader");
	failingDeclaredAssets.put("localization/good.lang", std::vector<std::uint8_t>{
		'J','A','2','-','L','O','C','A','L','I','Z','A','T','I','O','N',' ','1','\n',
		'u','i','.','o','n','e',' ','=',' ','O','n','e','\n'});
	failingDeclaredAssets.put("localization/bad.lang", std::vector<std::uint8_t>{
		'J','A','2','-','L','O','C','A','L','I','Z','A','T','I','O','N',' ','1','\n',
		'u','i','.','x',' ','=',' ','X','\n','u','i','.','x',' ','=',' ','Y','\n'});
	const PackageDescriptor failingDescriptor{
		ContentManifest{"mod.content-loader", "1", ContentApiVersion{1, 4}},
		PackageKind::Extension, {}, {}, {}, {},
		{{"en", "localization/good.lang"}, {"en", "localization/bad.lang"}}, {}};
	const PackageContentLoadResult failedContent = LoadDeclaredPackageContent(
		failingDescriptor, failingDeclaredAssets, packageLocalization, packageDefinitions);
	check(failedContent.error == PackageContentLoadError::LocalizationDocumentInvalid &&
		failedContent.assetPath == "localization/bad.lang" && failedContent.line == 3 &&
		loadedLocalization.size() == 0 && loadedDefinitions.size() == 0,
		"core declared-content import failure rolls back the complete package layer");
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
	EntityRegistry isolatedEntities(2);
	PackageEntities firstPackageEntities("campaign.base", isolatedEntities);
	PackageEntities secondPackageEntities("mod.observer", isolatedEntities);
	const EntityCreateResult ownedEntity = firstPackageEntities.create("mercenary");
	check(ownedEntity &&
		secondPackageEntities.destroy(ownedEntity.id) == EntityDestroyError::NotOwner &&
		isolatedEntities.alive(ownedEntity.id) &&
		firstPackageEntities.destroy(ownedEntity.id) == EntityDestroyError::None &&
		!isolatedEntities.alive(ownedEntity.id),
		"package entity handles cannot destroy another package's owned identity");
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
	auto stateSchemaPackages = fingerprintPackages;
	stateSchemaPackages.packages[0].descriptor.saveStateSchemaVersion = 2;
	const RuntimeCompatibilityFingerprint stateSchemaFingerprint =
		BuildRuntimeCompatibilityFingerprint(stateSchemaPackages, fingerprintServices,
			fingerprintConfiguration, capabilities, fingerprintDefinitions);
	check(firstFingerprint == repeatedFingerprint &&
		firstFingerprint != changedFingerprint && firstFingerprint != stateSchemaFingerprint &&
		firstFingerprint.hex().size() == 40,
		"runtime fingerprints include versioned definitions and package save schemas");
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

	EngineHostOptions defaultHostOptions;
	const EngineHostOptionsValidationResult defaultOptionsValidation =
		ValidateEngineHostOptions(defaultHostOptions);
	EngineHost<unsigned> legacyDefaultHost;
	EngineHost<unsigned> namedDefaultHost(defaultHostOptions);
	check(defaultOptionsValidation &&
		legacyDefaultHost.serviceCatalog().size() == 14 &&
		legacyDefaultHost.configuration().size() == 21 &&
		namedDefaultHost.serviceCatalog().size() ==
			legacyDefaultHost.serviceCatalog().size() &&
		namedDefaultHost.configuration().size() ==
			legacyDefaultHost.configuration().size() &&
		namedDefaultHost.compatibilityFingerprint() ==
			legacyDefaultHost.compatibilityFingerprint() &&
		namedDefaultHost.runtimeMessages().maxQueuedMessages() == 1024 &&
		namedDefaultHost.runtimeMessages().maxPayloadBytes() == 64u * 1024u &&
		namedDefaultHost.inputDispatcher().maxEventsPerDispatch() == 256 &&
		namedDefaultHost.frameTelemetry().capacity() == 240 &&
		namedDefaultHost.persistence().maximumPayloadBytes() ==
			PersistenceService::DefaultMaximumPayloadBytes &&
		namedDefaultHost.packages().maximumPersistencePayloadBytes() ==
			PersistenceService::DefaultMaximumPayloadBytes &&
		namedDefaultHost.packages().maximumSaveStateRecords() ==
			PackageRegistry::MaximumSaveStateRecords &&
		namedDefaultHost.packageSaveArchives().maximumRecords() ==
			PackageRegistry::MaximumSaveStateRecords,
		"named host defaults preserve the positional host contract and fingerprint");

	MemoryByteStorage optionStorage;
	MemoryInputSource optionInput;
	EngineServices optionServices{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), optionStorage,
		NullLogSink::instance(), optionInput};
	EngineHostOptions customHostOptions;
	customHostOptions.supportedContentApi = ContentApiVersion{7, 9};
	customHostOptions.hostCapabilities.add("host.named-options");
	customHostOptions.packageRandomSeed = 41;
	customHostOptions.simulationStepMicroseconds = 5000;
	customHostOptions.limits.maximumPackageRandomStreams = 2;
	customHostOptions.limits.maximumSimulationCatchUpTicks = 3;
	customHostOptions.limits.maximumAssetCacheEntries = 4;
	customHostOptions.limits.maximumAssetCacheBytes = 101;
	customHostOptions.limits.runtimeFaultHistoryCapacity = 5;
	customHostOptions.limits.maximumLocalizationEntries = 6;
	customHostOptions.limits.maximumLocalizationTextBytes = 102;
	customHostOptions.limits.maximumDefinitionEntries = 7;
	customHostOptions.limits.maximumDefinitionPayloadBytes = 103;
	customHostOptions.limits.maximumEntities = 8;
	customHostOptions.limits.maximumPackageAudioPlaybacks = 9;
	customHostOptions.limits.maximumQueuedPackageTasks = 10;
	customHostOptions.limits.maximumPackageTasksPerFrame = 3;
	customHostOptions.limits.maximumCheckpointPackages = 11;
	customHostOptions.limits.maximumRuntimeReportBytes = 104;
	customHostOptions.limits.maximumQueuedRuntimeMessages = 12;
	customHostOptions.limits.maximumRuntimeMessagePayloadBytes = 105;
	customHostOptions.limits.maximumInputEventsPerDispatch = 13;
	customHostOptions.limits.frameTelemetryHistoryCapacity = 14;
	customHostOptions.limits.maximumPersistencePayloadBytes = 256;
	customHostOptions.limits.maximumPackageSaveStateRecords = 15;
	customHostOptions.limits.maximumPackageSaveStateBytes = 107;
	customHostOptions.limits.maximumTotalPackageSaveStateBytes = 108;
	EngineHost<unsigned> customHost(customHostOptions, optionServices);
	const PackageCatalogSnapshot customCatalog = customHost.packageCatalog();
	check(customHost.hasCapability("host.named-options") &&
		customCatalog.supportedApi.major == 7 && customCatalog.supportedApi.minor == 9 &&
		customHost.simulationTicks().stepMicroseconds() == 5000 &&
		customHost.simulationTicks().maxCatchUpTicks() == 3 &&
		customHost.assetCache().maximumEntries() == 4 &&
		customHost.assetCache().maximumBytes() == 101 &&
		customHost.runtimeFaults().capacity() == 5 &&
		customHost.localization().maximumEntries() == 6 &&
		customHost.localization().maximumTextBytes() == 102 &&
		customHost.definitions().maximumEntries() == 7 &&
		customHost.definitions().maximumPayloadBytes() == 103 &&
		customHost.entities().maximumEntities() == 8 &&
		customHost.packageAudio().maximumPlaybacks() == 9 &&
		customHost.packageTasks().maximumQueued() == 10 &&
		customHost.packageTasks().maximumPerDrain() == 3 &&
		customHost.runtimeCheckpoints().maximumPackages() == 11 &&
		customHost.runtimeReports().maximumBytes() == 104 &&
		customHost.runtimeMessages().maxQueuedMessages() == 12 &&
		customHost.runtimeMessages().maxPayloadBytes() == 105 &&
		customHost.inputDispatcher().maxEventsPerDispatch() == 13 &&
		customHost.frameTelemetry().capacity() == 14 &&
		customHost.persistence().maximumPayloadBytes() == 256 &&
		customHost.packages().maximumPersistencePayloadBytes() == 256 &&
		customHost.packages().maximumSaveStateRecords() == 15 &&
		customHost.packages().maximumPackageSaveStateBytes() == 107 &&
		customHost.packages().maximumTotalSaveStateBytes() == 108 &&
		customHost.packageSaveArchives().maximumRecords() == 15 &&
		customHost.packageSaveArchives().maximumPackageBytes() == 107 &&
		customHost.packageSaveArchives().maximumTotalBytes() == 108,
		"named host options configure every owned bounded subsystem coherently");

	EngineHostOptions invalidHostOptions;
	invalidHostOptions.simulationStepMicroseconds =
		std::numeric_limits<std::uint64_t>::max();
	const EngineHostOptionsValidationResult invalidOptionsValidation =
		ValidateEngineHostOptions(invalidHostOptions);
	EngineHostOptions invalidSaveStateOptions;
	invalidSaveStateOptions.limits.maximumPackageSaveStateBytes = 2;
	invalidSaveStateOptions.limits.maximumTotalPackageSaveStateBytes = 1;
	const EngineHostOptionsValidationResult invalidSaveStateValidation =
		ValidateEngineHostOptions(invalidSaveStateOptions);
	bool invalidHostRejected = false;
	try
	{
		EngineHost<unsigned> invalidHost(invalidHostOptions);
	}
	catch (const std::invalid_argument& error)
	{
		invalidHostRejected =
			std::string(error.what()).find("simulationStepMicroseconds") !=
			std::string::npos;
	}
	check(invalidOptionsValidation.error ==
			EngineHostOptionsValidationError::RuntimeConfigurationRange &&
		std::string(invalidOptionsValidation.option) == "simulationStepMicroseconds" &&
		invalidSaveStateValidation.error ==
			EngineHostOptionsValidationError::InvalidPackageSaveStateLimits &&
		invalidHostRejected,
		"invalid host options are diagnosed and rejected before host construction");

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
	const RuntimeReport declaredContentReport = declaredContentHost.runtimeReport();
	check(declaredContentRegistration == PackageRegistrationError::None &&
		declaredContentCatalog &&
		declaredContentCatalog->descriptor.localizationSources.size() == 2 &&
		declaredContentCatalog->descriptor.localizationSources[1].locale == "nl" &&
		declaredContentCatalog->descriptor.definitionSources.size() == 1 &&
		declaredContentCatalog->descriptor.definitionSources[0].schemaVersion == 2 &&
		declaredContentReport.schema == RuntimeReport::CurrentSchema &&
		declaredContentReport.healthy() && declaredContentReport.registeredPackages == 1 &&
		declaredContentReport.activePackages == 0 &&
		declaredContentReport.packages[0].descriptor.content.id == "mod.declared-content" &&
		declaredContentReport.packages[0].descriptor.localizationSources.size() == 2,
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
	constexpr EngineServiceContract<unsigned> externalServiceContract{
		"host.test-service", {2, 1}};
	const EngineServiceRegistrationError registeredService =
		sessionHost.serviceCatalog().registerService(
			"host.test-service", EngineServiceVersion{2, 3}, externalService);
	const EngineServiceLookupResult<unsigned> resolvedService =
		sessionHost.serviceCatalog().resolve(externalServiceContract);
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
		sessionHost.serviceCatalog().resolve(FrameTelemetryServiceContract).service ==
			&sessionHost.frameTelemetry() &&
		sessionHost.serviceCatalog().size() == 15 &&
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
		sessionHost.configuration().size() == 22,
		"runtime configuration publishes typed stable values and seals before bootstrap");
	const RuntimeDiagnosticsSnapshot diagnostics = sessionHost.diagnostics();
	const RuntimeReport runtimeReport = sessionHost.runtimeReport();
	check(diagnostics.lifecycle == EngineLifecycle::Stopped &&
		diagnostics.frames.summary.completedFrames == 0 &&
		diagnostics.packages.packages.empty() && diagnostics.faults.records.empty() &&
		diagnostics.localization.empty() && diagnostics.definitions.empty() &&
		diagnostics.entities.empty() && diagnostics.packageAudio.empty() &&
		diagnostics.packageTasks.queued.empty() &&
		diagnostics.packageResources.packages.empty() && diagnostics.services.size() == 15 &&
		diagnostics.configuration.size() == 22 &&
		diagnostics.compatibility == sessionHost.compatibilityFingerprint() &&
		diagnostics.queuedMessages == 0 &&
		diagnostics.completedFrames == 0 && diagnostics.completedSimulationTicks == 0,
		"runtime diagnostics capture one pointer-free ordered host snapshot");
	check(runtimeReport.lifecycle == EngineLifecycle::Stopped && runtimeReport.healthy() &&
		runtimeReport.completedFrames == 0 && runtimeReport.completedSimulationTicks == 0 &&
		runtimeReport.registeredPackages == 0 && runtimeReport.activePackages == 0 &&
		runtimeReport.services.size() == 15 && runtimeReport.configuration.size() == 22 &&
		runtimeReport.compatibility == diagnostics.compatibility &&
		runtimeReport.frames.completedFrames == diagnostics.frames.summary.completedFrames,
		"runtime report condenses diagnostics without retaining sensitive content payloads");
	RuntimeReport serializableReport = runtimeReport;
	serializableReport.configuration.push_back(RuntimeConfigurationEntry{
		"diagnostics.label", std::string("Quoted \"line\"\nR\xc3\xa9" "ady")});
	const RuntimeReportJsonResult reportJson =
		SerializeRuntimeReportJson(serializableReport);
	const RuntimeReportJsonResult repeatedReportJson =
		SerializeRuntimeReportJson(serializableReport);
	const RuntimeReportJsonResult boundedReportJson =
		SerializeRuntimeReportJson(serializableReport, 32);
	check(reportJson && reportJson.json == repeatedReportJson.json &&
		reportJson.json.find("\"schema\":1") != std::string::npos &&
		reportJson.json.find("Quoted \\\"line\\\"\\nR\xc3\xa9" "ady") != std::string::npos &&
		reportJson.json.find("\"packages\":[]") != std::string::npos &&
		boundedReportJson.error == RuntimeReportJsonError::TooLarge &&
		boundedReportJson.json.empty(),
		"runtime report JSON is deterministic, escaped, UTF-8, and bounded transactionally");
	MemoryByteStorage reportStorage;
	EngineServices reportServices{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), reportStorage};
	EngineHost<unsigned> reportingHost(reportServices);
	const RuntimeReportSaveError savedRuntimeReport =
		reportingHost.saveRuntimeReport("diagnostics/runtime-report.json");
	std::vector<std::uint8_t> savedRuntimeReportBytes;
	const bool readRuntimeReport = reportStorage.readAll(
		"diagnostics/runtime-report.json", savedRuntimeReportBytes);
	RuntimeReportService tinyReportService(reportingHost.persistence(), 32);
	check(savedRuntimeReport == RuntimeReportSaveError::None && readRuntimeReport &&
		!savedRuntimeReportBytes.empty() && savedRuntimeReportBytes.front() == '{' &&
		savedRuntimeReportBytes.back() == '\n' &&
		tinyReportService.save("diagnostics/tiny.json", reportingHost.runtimeReport()) ==
			RuntimeReportSaveError::TooLarge &&
		!reportStorage.exists("diagnostics/tiny.json") &&
		reportingHost.runtimeReports().maximumBytes() == 4u * 1024u * 1024u,
		"runtime report service persists readable JSON without partial bounded writes");

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
	DeterministicCommandQueue<int> reclaimedSequences;
	for (int command = 0; command < 100000; ++command)
	{
		reclaimedSequences.enqueue(0, command);
		reclaimedSequences.drainThrough(0);
	}
	check(reclaimedSequences.empty() && reclaimedSequences.liveSequenceCount() == 0 &&
		!reclaimedSequences.enqueueRecorded(0, 99999, 1),
		"completed command identities are retired without unbounded live storage");
	DeterministicCommandQueue<int> exhaustedSequences;
	const std::uint64_t maximumSequence = std::numeric_limits<std::uint64_t>::max();
	const bool acceptedMaximum = exhaustedSequences.enqueueRecorded(0, maximumSequence, 1);
	bool exhaustionReported = false;
	try
	{
		exhaustedSequences.enqueue(0, 2);
	}
	catch (const std::overflow_error&)
	{
		exhaustionReported = true;
	}
	exhaustedSequences.drainThrough(0);
	check(acceptedMaximum && exhaustionReported && exhaustedSequences.sequenceExhausted() &&
		exhaustedSequences.liveSequenceCount() == 0 &&
		!exhaustedSequences.enqueueRecorded(0, maximumSequence, 3),
		"command sequence exhaustion fails explicitly without wrapping or reusing IDs");

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
	RuntimeMessageBus retainedMessages(1, 8);
	retainedMessages.publish(RuntimeMessageRequest{
		"engine.first", "engine.test", {1}});
	RuntimeMessageRequest retainedRequest{
		"engine.retry", "engine.test", {2, 3}};
	const RuntimeMessagePublishResult retainedPressure =
		retainedMessages.publishRetained(retainedRequest);
	const bool retainedUnchanged =
		retainedRequest.topic == "engine.retry" &&
		retainedRequest.source == "engine.test" &&
		retainedRequest.payload == std::vector<std::uint8_t>({2, 3});
	retainedMessages.dispatchPending();
	const RuntimeMessagePublishResult retainedPublished =
		retainedMessages.publishRetained(retainedRequest);
	check(retainedPressure.error == RuntimeMessagePublishError::QueueFull &&
		retainedUnchanged &&
		retainedRequest.topic.empty() && retainedRequest.source.empty() &&
		retainedRequest.payload.empty() && retainedPublished &&
		retainedPublished.sequence == 2,
		"runtime message ownership stays retained under pressure and transfers on success");
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
	persistenceStorage.writeAll(
		"memory.oversized", std::vector<std::uint8_t>{1, 2, 3, 4, 5});
	std::vector<std::uint8_t> boundedOutput{9};
	check(persistenceStorage.readAllBounded(
			"memory.oversized", 4, boundedOutput) == ByteStorageReadResult::TooLarge &&
		boundedOutput == std::vector<std::uint8_t>({9}) &&
		persistenceStorage.readAllBounded(
			"memory.missing", 4, boundedOutput) == ByteStorageReadResult::NotFound &&
		boundedOutput == std::vector<std::uint8_t>({9}),
		"memory byte storage rejects oversized records before copying and preserves output");
	LegacyOnlyByteStorage legacyOnlyStorage({1, 2, 3, 4, 5});
	check(legacyOnlyStorage.readAllBounded(
			"legacy.record", 4, boundedOutput) == ByteStorageReadResult::TooLarge &&
		legacyOnlyStorage.readAllCalls == 1 &&
		boundedOutput == std::vector<std::uint8_t>({9}),
		"bounded byte-storage reads retain a transactional fallback for legacy adapters");
	BoundedProbeByteStorage boundedProbeStorage(
		std::vector<std::uint8_t>(9, 0));
	PersistenceService boundedProbePersistence(boundedProbeStorage, 2);
	PersistenceHeader boundedProbeHeader{77, 88};
	std::vector<std::uint8_t> boundedProbePayload{7};
	check(boundedProbePersistence.load(
			"bounded.record", 1, 1, 1,
			boundedProbeHeader, boundedProbePayload) == PersistenceLoadResult::TooLarge &&
		boundedProbeStorage.boundedReadCalls == 1 &&
		boundedProbeStorage.readAllCalls == 0 && boundedProbeStorage.existsCalls == 0 &&
		boundedProbeHeader.magic == 77 && boundedProbeHeader.version == 88 &&
		boundedProbePayload == std::vector<std::uint8_t>({7}),
		"persistence enforces encoded-size bounds through the storage adapter before reading");
	std::vector<std::uint8_t> boundedRawOutput{6};
	check(!boundedProbePersistence.loadRawBounded(
			"bounded.record", 8, boundedRawOutput) &&
		boundedProbeStorage.boundedReadCalls == 2 &&
		boundedProbeStorage.readAllCalls == 0 && boundedProbeStorage.existsCalls == 0 &&
		boundedRawOutput == std::vector<std::uint8_t>({6}),
		"raw persistence uses bounded storage reads without publishing oversized data");
	check(boundedProbePersistence.loadEnvelope(
			"missing.record", 1, 1, 1,
			boundedProbeHeader, boundedProbePayload) == PersistenceLoadResult::NotFound &&
		boundedProbePersistence.loadEnvelope(
			"storage.error", 1, 1, 1,
			boundedProbeHeader, boundedProbePayload) == PersistenceLoadResult::StorageError &&
		boundedProbeHeader.magic == 77 && boundedProbeHeader.version == 88 &&
		boundedProbePayload == std::vector<std::uint8_t>({7}),
		"bounded persistence distinguishes missing and failed storage without publishing output");
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
	check(persistenceStorage.remove("engine.record") &&
		!persistenceStorage.exists("engine.record") &&
		persistenceStorage.remove("engine.record") &&
		!persistenceStorage.remove(""),
		"byte storage removes records idempotently and rejects empty paths");
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
	PackageSaveArchiveService packageArchives(checkpointPersistence, 2, 8, 12);
	const PackageSaveArchive savedPackageArchive{firstFingerprint,
		PackageSaveStateSnapshot{{
			PackageSaveStateRecord{"rules.fingerprint", "2.0", 1, {4, 2}},
			PackageSaveStateRecord{"extension.state", "1.0", 3, {1, 3, 3, 7}}}}};
	check(packageArchives.save("package-state", savedPackageArchive) ==
			PackageSaveArchiveSaveError::None,
		"package save archive writes ordered bounded state through persistence envelopes");
	PackageSaveArchive loadedPackageArchive;
	const PackageSaveArchiveLoadResult loadedPackageState = packageArchives.load(
		"package-state", firstFingerprint, loadedPackageArchive);
	check(loadedPackageState && loadedPackageArchive.compatibility == firstFingerprint &&
		loadedPackageArchive.state.records.size() == 2 &&
		loadedPackageArchive.state.records[1].packageId == "extension.state" &&
		loadedPackageArchive.state.records[1].schemaVersion == 3 &&
		loadedPackageArchive.state.records[1].payload ==
			std::vector<std::uint8_t>({1, 3, 3, 7}),
		"package save archive round-trips identity, schemas, order, and opaque bytes");
	PackageSaveArchive unchangedPackageArchive{firstFingerprint,
		PackageSaveStateSnapshot{{PackageSaveStateRecord{"unchanged", "1", 1, {9}}}}};
	const PackageSaveArchiveLoadResult incompatiblePackageState = packageArchives.load(
		"package-state", changedFingerprint, unchangedPackageArchive);
	check(incompatiblePackageState.error ==
			PackageSaveArchiveLoadError::IncompatibleRuntime &&
		incompatiblePackageState.storedCompatibility == firstFingerprint &&
		unchangedPackageArchive.state.records[0].packageId == "unchanged",
		"package save archive rejects another runtime before publishing package state");

	ContentRegistry content(ContentApiVersion{1, 2});
	check(content.registerContent(ContentManifest{
		"engine.test", "1", ContentApiVersion{1, 0}, {}}) ==
		ContentRegistrationError::None,
		"compiled core registry links without game or platform libraries");

	return failures == 0 ? 0 : 1;
}
