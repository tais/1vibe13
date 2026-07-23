#include <Engine/Core/AssetSource.h>
#include <Engine/Core/BinaryArchive.h>
#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/CommandStream.h>
#include <Engine/Core/CommandProcessor.h>
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
#include <Engine/Core/PinnedSlotCache.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/RuntimeReportJson.h>
#include <Engine/Core/SimulationTick.h>
#include <Engine/Core/StableResourceRegistry.h>
#include <Engine/Core/StateRegistry.h>

#include <cstdint>
#include <cstdio>
#include <functional>
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

class CallbackInputSink final : public InputEventSink
{
public:
	void receiveInput(const EngineInputEvent&) override { callback(); }
	std::function<void()> callback;
};

class CallbackRuntimeUpdateSink final : public RuntimeUpdateSink
{
public:
	void updateRuntime(const RuntimeUpdateContext&) override { callback(); }
	std::function<void()> callback;
};

class CallbackSimulationTickSink final : public SimulationTickSink
{
public:
	void simulate(const SimulationTickContext&) override { callback(); }
	std::function<void()> callback;
};

class CallbackMessageSink final : public RuntimeMessageSink
{
public:
	void receiveMessage(const RuntimeMessage&) override { callback(); }
	std::function<void()> callback;
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

class TransactionalLifecyclePackage final : public EnginePackage
{
public:
	explicit TransactionalLifecyclePackage(std::string id)
		: descriptor_{ContentManifest{
			std::move(id), "1", ContentApiVersion{1, 0}}, PackageKind::Rules}
	{
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override
	{
		active = true;
		return true;
	}
	void deactivate() noexcept override
	{
		active = false;
		++deactivateCalls;
	}
	bool bootstrap(PackageBootstrapContext&, PackageBootstrapPhase phase) override
	{
		bootstrapCalls.push_back(static_cast<int>(phase));
		if (cancelDuringBootstrap)
		{
			cancelDuringBootstrapResult =
				cancelDuringBootstrap->tryCancelInitialization();
			cancelDuringBootstrap = nullptr;
		}
		return static_cast<int>(phase) != failOnBootstrapPhase;
	}
	void shutdown(PackageBootstrapContext&, PackageBootstrapPhase phase) override
	{
		shutdownCalls.push_back(static_cast<int>(phase));
		if (shutdownDuringShutdown)
		{
			shutdownDuringShutdownResult = shutdownDuringShutdown->shutdownPackages();
			shutdownDuringShutdown = nullptr;
		}
		if (static_cast<int>(phase) == throwOnShutdownPhase)
			throw std::runtime_error("injected shutdown failure");
	}

	PackageDescriptor descriptor_;
	std::vector<int> bootstrapCalls;
	std::vector<int> shutdownCalls;
	RuntimeSession* cancelDuringBootstrap = nullptr;
	RuntimeSessionTransitionResult cancelDuringBootstrapResult;
	RuntimeSession* shutdownDuringShutdown = nullptr;
	RuntimeSessionShutdownResult shutdownDuringShutdownResult;
	int throwOnShutdownPhase = -1;
	int failOnBootstrapPhase = -1;
	int deactivateCalls = 0;
	bool active = false;
};

class RandomSavePackage final : public EnginePackage
{
public:
	explicit RandomSavePackage(std::string id, std::uint8_t state)
		: descriptor_{ContentManifest{
			std::move(id), "1", ContentApiVersion{1, 0}}, PackageKind::Rules,
			{}, {}, {}, {}, {}, {}, 1}, state_(state)
	{
	}

	const PackageDescriptor& descriptor() const override { return descriptor_; }
	bool activate() noexcept override { active_ = true; return true; }
	void deactivate() noexcept override { active_ = false; }
	void simulate(PackageBootstrapContext& context,
		const SimulationTickContext&) override
	{
		const PackageRandomResult value = context.random.next("simulation", 1000000);
		if (value) simulationValues.push_back(value.value);
	}
	bool saveState(PackageBootstrapContext& context,
		std::vector<std::uint8_t>& state) override
	{
		++saveCalls;
		context.random.next("save-callback", 1000);
		state = {state_};
		return true;
	}
	bool validateState(PackageBootstrapContext& context, std::uint32_t schema,
		const std::vector<std::uint8_t>& state) override
	{
		++validateCalls;
		context.random.next("validate-callback", 1000);
		return schema == 1 && state.size() == 1;
	}
	bool loadState(PackageBootstrapContext& context, std::uint32_t schema,
		const std::vector<std::uint8_t>& state) override
	{
		++loadCalls;
		context.random.next("load-callback", 1000);
		if (schema != 1 || state.size() != 1) return false;
		state_ = state[0];
		return true;
	}

	PackageDescriptor descriptor_;
	std::vector<std::uint32_t> simulationValues;
	int saveCalls = 0;
	int validateCalls = 0;
	int loadCalls = 0;

private:
	std::uint8_t state_;
	bool active_ = false;
};

class ContractTestService
{
public:
	virtual ~ContractTestService() = default;
	virtual unsigned value() const = 0;
};

class ContractTestPadding
{
public:
	virtual ~ContractTestPadding() = default;
	unsigned padding = 7;
};

class ContractTestImplementation final : public ContractTestPadding,
	public ContractTestService
{
public:
	unsigned value() const override { return 42; }
};

class RegistryResource
{
public:
	RegistryResource(int value, int& destructions)
		: value(value), destructions_(&destructions)
	{
	}
	~RegistryResource() { if (destructions_) ++*destructions_; }
	RegistryResource(const RegistryResource&) = delete;
	RegistryResource& operator=(const RegistryResource&) = delete;
	RegistryResource(RegistryResource&& other) noexcept
		: value(other.value), destructions_(other.destructions_)
	{
		other.destructions_ = nullptr;
	}
	RegistryResource& operator=(RegistryResource&& other) noexcept
	{
		if (this == &other) return *this;
		if (destructions_) ++*destructions_;
		value = other.value;
		destructions_ = other.destructions_;
		other.destructions_ = nullptr;
		return *this;
	}

	int value = 0;

private:
	int* destructions_ = nullptr;
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
	int registryDestructions = 0;
	StableResourceRegistry<RegistryResource> registry(
		StableResourceRegistry<RegistryResource>::Limits{2, 2, 6, 3});
	const auto registryFirst = registry.insert(RegistryResource(10, registryDestructions));
	const auto registrySecond = registry.insert(RegistryResource(20, registryDestructions));
	check(registryFirst == 2 && registrySecond == 4 && registry.size() == 2 &&
		registry.find(2) && registry.find(2)->value == 10 &&
		registry.erase(2) && !registry.find(2) && registryDestructions == 1,
		"stable resource registries own move-only values behind configured handles");
	const auto registryThird = registry.insert(RegistryResource(30, registryDestructions));
	const auto registryExhausted = registry.insert(RegistryResource(40, registryDestructions));
	check(registryThird == 6 && !registryExhausted && registry.exhausted() &&
		registry.size() == 2 && registryDestructions == 2,
		"stable resource registries reject exhausted IDs without leaking rejected values");
	registry.clear();
	const auto registryRestarted = registry.insert(RegistryResource(50, registryDestructions));
	check(registryRestarted == 2 && registry.size() == 1 && registryDestructions == 4,
		"clearing a stable registry destroys live values and starts a fresh handle lifetime");
	registry.clear();

	int slotDestructions = 0;
	PinnedSlotCache<RegistryResource, std::uint8_t> slots(3);
	const auto slotFirst = slots.insert(RegistryResource(10, slotDestructions));
	const auto slotSecond = slots.insert(RegistryResource(20, slotDestructions));
	check(slotFirst == 0 && slotSecond == 1 && slots.highWaterMark() == 2 &&
		slots.retain(0) && slots.pins(0) == 2 &&
		slots.release(0) == decltype(slots)::ReleaseResult::Retained &&
		slots.release(0) == decltype(slots)::ReleaseResult::Removed &&
		slotDestructions == 1,
		"pinned slot caches retain stable live IDs until their final release");
	const auto reusedSlot = slots.insert(RegistryResource(30, slotDestructions));
	const auto finalSlot = slots.insert(RegistryResource(40, slotDestructions));
	const auto fullSlot = slots.insert(RegistryResource(50, slotDestructions));
	check(reusedSlot == 0 && finalSlot == 2 && !fullSlot && slots.full() &&
		slots.find(1) && slots.find(1)->value == 20 && slotDestructions == 2,
		"pinned slot caches choose the lowest free slot and reject full insertion safely");
	bool retainedToLimit = true;
	for (unsigned count = 1; count < 255; ++count)
		retainedToLimit = slots.retain(1) && retainedToLimit;
	check(retainedToLimit && slots.pins(1) == 255 && !slots.retain(1),
		"pinned slot caches reject saturated reference counts without wrapping");
	slots.clear();
	check(slots.empty() && slots.highWaterMark() == 0 && slotDestructions == 5,
		"clearing a pinned slot cache releases each owned resource exactly once");

	std::string path;
	check(NormalizeAssetPath("TableData\\Items.XML", path) &&
		path == "tabledata/items.xml",
		"compiled core normalizes portable asset paths");
	check(!NormalizeAssetPath("../Data/secret", path),
		"compiled core rejects traversal paths");
	const std::string maximumIdentifier(MaximumEngineIdentifierBytes, 'a');
	const std::string oversizedIdentifier(MaximumEngineIdentifierBytes + 1, 'a');
	const std::string maximumLogicalPath(MaximumLogicalAssetPathBytes, 'a');
	const std::string oversizedLogicalPath(MaximumLogicalAssetPathBytes + 1, 'a');
	check(IsValidEngineIdentifier(maximumIdentifier) &&
		!IsValidEngineIdentifier(oversizedIdentifier) &&
		NormalizeAssetPath(maximumLogicalPath, path) &&
		path == maximumLogicalPath &&
		!NormalizeAssetPath(oversizedLogicalPath, path) && path.empty(),
		"public identifiers and logical paths enforce exact metadata bounds");
	ContentRegistry boundedMetadataContent(CurrentContentApiVersion);
	check(boundedMetadataContent.registerContent(ContentManifest{
			oversizedIdentifier, "1", CurrentContentApiVersion}) ==
			ContentRegistrationError::InvalidManifest &&
		boundedMetadataContent.registerContent(ContentManifest{
			"metadata.version", std::string(MaximumEngineVersionBytes + 1, '1'),
			CurrentContentApiVersion}) == ContentRegistrationError::InvalidManifest,
		"content manifests reject metadata that cannot round-trip through archives");
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
			LocalizationSetError::None &&
		localization.maximumTotalTextBytes() == 32,
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
	LocalizationCatalog boundedLocalization(4, 16, 8);
	check(boundedLocalization.set("campaign.base", "en", "ui.first", "Ready") ==
			LocalizationSetError::None && boundedLocalization.textBytes() == 5 &&
		boundedLocalization.set("campaign.base", "en", "ui.second", "More") ==
			LocalizationSetError::TotalCapacityReached &&
		boundedLocalization.set("campaign.base", "en", "ui.first", "Go") ==
			LocalizationSetError::None && boundedLocalization.textBytes() == 2 &&
		boundedLocalization.set("campaign.base", "en", "ui.second", "More") ==
			LocalizationSetError::None && boundedLocalization.textBytes() == 6,
		"localization catalog enforces aggregate text budgets transactionally");
	LocalizationCatalog saturatedLocalization(
		std::numeric_limits<std::size_t>::max(), 2);
	check(saturatedLocalization.maximumTotalTextBytes() ==
			std::numeric_limits<std::size_t>::max(),
		"legacy localization limits preserve capacity without overflowing");
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
			DefinitionSetError::None && definitions.maximumTotalPayloadBytes() == 8,
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
	DefinitionCatalog boundedDefinitions(4, 4, 5);
	check(boundedDefinitions.set("campaign.base", "item", "first", 1, {1, 2, 3, 4}) ==
			DefinitionSetError::None && boundedDefinitions.payloadBytes() == 4 &&
		boundedDefinitions.set("campaign.base", "item", "second", 1, {5, 6}) ==
			DefinitionSetError::TotalCapacityReached &&
		boundedDefinitions.set("campaign.base", "item", "first", 2, {1}) ==
			DefinitionSetError::None && boundedDefinitions.payloadBytes() == 1 &&
		boundedDefinitions.set("campaign.base", "item", "second", 1, {5, 6}) ==
			DefinitionSetError::None && boundedDefinitions.payloadBytes() == 3,
		"definition catalog enforces aggregate payload budgets transactionally");
	DefinitionCatalog saturatedDefinitions(std::numeric_limits<std::size_t>::max(), 2);
	check(saturatedDefinitions.maximumTotalPayloadBytes() ==
			std::numeric_limits<std::size_t>::max(),
		"legacy definition limits preserve capacity without overflowing");
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
	groupedAudioOutput.finish(firstGroupedPlayback.playback);
	const PackageAudioPruneResult prunedAudio = audioGroups.pruneFinished();
	const PackageAudioPlayResult replacementGroupedPlayback = audioGroups.play(
		"mod.audio", "ui", AudioPlaybackRequest{"audio/replacement.wav"});
	check(prunedAudio.checked == 2 && prunedAudio.retired == 1 &&
		prunedAudio.queryFailures == 0 && replacementGroupedPlayback &&
		audioGroups.size() == 2,
		"naturally completed package audio releases bounded playback capacity");
	const PackageAudioOperationResult releasedAudio = audioGroups.releasePackage("mod.audio");
	check(releasedAudio.matched == 2 && releasedAudio.succeeded == 2 &&
		audioGroups.size() == 0 &&
		!groupedAudioOutput.isPlaying(firstGroupedPlayback.playback) &&
		!groupedAudioOutput.isPlaying(secondGroupedPlayback.playback) &&
		!groupedAudioOutput.isPlaying(replacementGroupedPlayback.playback),
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
	const PackageRandomCheckpoint randomCheckpoint = packageRandom.checkpoint();
	const PackageRandomResult expectedThirdCombat = packageRandom.next("combat", 1000);
	const PackageRandomResult expectedSecondLoot = packageRandom.next("loot", 1000);
	const PackageRandomCheckpoint advancedRandomCheckpoint = packageRandom.checkpoint();
	PackageRandomCheckpoint duplicateRandomCheckpoint = randomCheckpoint;
	duplicateRandomCheckpoint.streams.push_back(
		duplicateRandomCheckpoint.streams.front());
	PackageRandomCheckpoint invalidRandomCheckpoint = randomCheckpoint;
	invalidRandomCheckpoint.schema = 99;
	PackageRandomSource duplicateCheckpointTarget("rules.ballistics", 0, 3);
	check(packageRandom.restoreCheckpoint(randomCheckpoint) ==
			PackageRandomCheckpointError::None &&
		packageRandom.next("combat", 1000).value == expectedThirdCombat.value &&
		packageRandom.next("loot", 1000).value == expectedSecondLoot.value &&
		packageRandom.checkpoint() == advancedRandomCheckpoint &&
		packageRandom.restoreCheckpoint(invalidRandomCheckpoint) ==
			PackageRandomCheckpointError::InvalidSchema &&
		duplicateCheckpointTarget.restoreCheckpoint(duplicateRandomCheckpoint) ==
			PackageRandomCheckpointError::DuplicateStream &&
		packageRandom.checkpoint() == advancedRandomCheckpoint,
		"package random checkpoints restore every stream transactionally");
	PackageRandomSource exhaustedRandom("rules.exhausted", 0, 1);
	const PackageRandomCheckpoint exhaustedCheckpoint{
		PackageRandomCheckpoint::CurrentSchema, "rules.exhausted",
		{{"stream", 42, std::numeric_limits<std::uint64_t>::max()}}};
	check(exhaustedRandom.restoreCheckpoint(exhaustedCheckpoint) ==
			PackageRandomCheckpointError::None &&
		exhaustedRandom.next("stream", 10).error ==
			PackageRandomError::SequenceExhausted &&
		exhaustedRandom.checkpoint() == exhaustedCheckpoint,
		"package random generation rejects counter exhaustion without wrapping state");
	PackageSaveStateSnapshot separatedEngineState;
	separatedEngineState.engineStatePresent = true;
	separatedEngineState.engineRecords.push_back(PackageEngineSaveStateRecord{
		"rules.ballistics", "1", randomCheckpoint});
	check(separatedEngineState.records.empty() &&
		separatedEngineState.findEngine("rules.ballistics") &&
		separatedEngineState.findEngine("rules.ballistics")->random == randomCheckpoint &&
		!separatedEngineState.findEngine("rules.missing"),
		"package saves represent engine-owned state separately from opaque mod payloads");
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
	EngineHost<unsigned> legacyBraceDefaultHost({});
	EngineHost<unsigned> namedDefaultHost(defaultHostOptions);
	EngineHost<unsigned> legacyCapacityHost(EngineServices::defaults(),
		CurrentContentApiVersion, NullPackageEventSink::instance(), RuntimeCapabilities{},
		0, 64, 16667, 4, 128, 64u * 1024u * 1024u, 256, 2, 3, 4, 5);
	check(defaultOptionsValidation &&
		legacyDefaultHost.serviceCatalog().size() == 14 &&
		legacyDefaultHost.configuration().size() == 23 &&
		namedDefaultHost.serviceCatalog().size() ==
			legacyDefaultHost.serviceCatalog().size() &&
		namedDefaultHost.configuration().size() ==
			legacyDefaultHost.configuration().size() &&
		namedDefaultHost.compatibilityFingerprint() ==
			legacyDefaultHost.compatibilityFingerprint() &&
		legacyBraceDefaultHost.compatibilityFingerprint() ==
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
			PackageRegistry::MaximumSaveStateRecords &&
		legacyCapacityHost.localization().maximumTotalTextBytes() == 6 &&
		legacyCapacityHost.definitions().maximumTotalPayloadBytes() == 20,
		"named host defaults preserve the positional host contract and fingerprint");

	constexpr EngineServiceContract<ContractTestService> derivedServiceContract{
		"test.derived-service", {1, 0}};
	EngineServiceContract<ContractTestService> invalidServiceContract;
	ServiceCatalog contractCatalog;
	ContractTestImplementation contractImplementation;
	const EngineServiceRegistrationError derivedServiceRegistration =
		contractCatalog.registerService(
			derivedServiceContract, contractImplementation);
	const EngineServiceLookupResult<ContractTestService> derivedService =
		contractCatalog.resolve(derivedServiceContract);
	const EngineServiceRegistrationError invalidServiceRegistration =
		contractCatalog.registerService(
			invalidServiceContract, contractImplementation);
	const EngineServiceLookupResult<ContractTestService> invalidService =
		contractCatalog.resolve(invalidServiceContract);
	check(derivedServiceRegistration == EngineServiceRegistrationError::None &&
		derivedService && derivedService.service->value() == 42 &&
		derivedService.service ==
			static_cast<ContractTestService*>(&contractImplementation) &&
		invalidServiceRegistration ==
			EngineServiceRegistrationError::InvalidDescriptor &&
		invalidService.error == EngineServiceLookupError::InvalidDescriptor &&
		contractCatalog.size() == 1,
		"typed service contracts bind derived implementations and reject invalid descriptors");

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
	customHostOptions.limits.maximumTotalLocalizationTextBytes = 104;
	customHostOptions.limits.maximumDefinitionEntries = 7;
	customHostOptions.limits.maximumDefinitionPayloadBytes = 103;
	customHostOptions.limits.maximumTotalDefinitionPayloadBytes = 105;
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
		customHost.localization().maximumTotalTextBytes() == 104 &&
		customHost.definitions().maximumEntries() == 7 &&
		customHost.definitions().maximumPayloadBytes() == 103 &&
		customHost.definitions().maximumTotalPayloadBytes() == 105 &&
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
		customHost.packageSaveArchives().maximumTotalBytes() == 108 &&
		customHost.packageSaveArchives().maximumRandomStreamsPerPackage() == 2,
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
	EngineHostOptions invalidCatalogOptions;
	invalidCatalogOptions.limits.maximumLocalizationTextBytes = 2;
	invalidCatalogOptions.limits.maximumTotalLocalizationTextBytes = 1;
	const EngineHostOptionsValidationResult invalidCatalogValidation =
		ValidateEngineHostOptions(invalidCatalogOptions);
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
		invalidCatalogValidation.error ==
			EngineHostOptionsValidationError::InvalidCatalogLimits &&
		invalidHostRejected,
		"invalid host options are diagnosed and rejected before host construction");

	EngineHostOptions zeroSaveBudgetOptions;
	zeroSaveBudgetOptions.limits.maximumPackageSaveStateRecords = 0;
	zeroSaveBudgetOptions.limits.maximumPackageSaveStateBytes = 0;
	zeroSaveBudgetOptions.limits.maximumTotalPackageSaveStateBytes = 0;
	EngineHost<unsigned> zeroSaveBudgetHost(zeroSaveBudgetOptions);
	const bool zeroSaveBudgetReady = zeroSaveBudgetHost.beginInitialization() &&
		zeroSaveBudgetHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::StartRuntime) &&
		zeroSaveBudgetHost.markRunning();
	const PackageSaveStateCaptureResult zeroSaveBudgetCapture =
		zeroSaveBudgetHost.capturePackageSaveState();
	check(zeroSaveBudgetReady && zeroSaveBudgetCapture &&
		zeroSaveBudgetCapture.snapshot.records.empty() &&
		zeroSaveBudgetCapture.snapshot.engineRecords.empty() &&
		!zeroSaveBudgetCapture.snapshot.engineStatePresent,
		"an empty runtime captures no package metadata even with a zero-byte save budget");

	RandomSavePackage firstRandomSavePackage("rules.random-save-a", 7);
	RandomSavePackage secondRandomSavePackage("rules.random-save-b", 9);
	EngineHost<unsigned> randomSaveHost;
	const bool randomSaveReady =
		randomSaveHost.packages().registerPackage(firstRandomSavePackage) ==
			PackageRegistrationError::None &&
		randomSaveHost.packages().registerPackage(secondRandomSavePackage) ==
			PackageRegistrationError::None &&
		randomSaveHost.packages().activate("rules.random-save-a") ==
			PackageActivationError::None &&
		randomSaveHost.packages().activate("rules.random-save-b") ==
			PackageActivationError::None &&
		randomSaveHost.beginInitialization() &&
		randomSaveHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::StartRuntime) &&
		randomSaveHost.markRunning();
	randomSaveHost.simulationTicks().advance(
		randomSaveHost.simulationTicks().stepMicroseconds());
	const PackageSaveStateCaptureResult capturedRandomState =
		randomSaveHost.capturePackageSaveState();
	randomSaveHost.simulationTicks().advance(
		randomSaveHost.simulationTicks().stepMicroseconds());
	const std::uint32_t expectedFirstRandom =
		firstRandomSavePackage.simulationValues.size() > 1
			? firstRandomSavePackage.simulationValues[1] : 0;
	const std::uint32_t expectedSecondRandom =
		secondRandomSavePackage.simulationValues.size() > 1
			? secondRandomSavePackage.simulationValues[1] : 0;
	const PackageSaveStateLoadResult restoredRandomState =
		randomSaveHost.restorePackageSaveState(capturedRandomState.snapshot);
	randomSaveHost.simulationTicks().advance(
		randomSaveHost.simulationTicks().stepMicroseconds());
	check(randomSaveReady && capturedRandomState &&
		capturedRandomState.snapshot.engineStatePresent &&
		capturedRandomState.snapshot.records.size() == 2 &&
		capturedRandomState.snapshot.engineRecords.size() == 2 &&
		capturedRandomState.snapshot.engineRecords[0].random.streams.size() == 1 &&
		restoredRandomState && restoredRandomState.restored == 2 &&
		restoredRandomState.engineRecordsRestored == 2 &&
		firstRandomSavePackage.simulationValues.size() == 3 &&
		secondRandomSavePackage.simulationValues.size() == 3 &&
		firstRandomSavePackage.simulationValues[2] == expectedFirstRandom &&
		secondRandomSavePackage.simulationValues[2] == expectedSecondRandom,
		"package save restore rewinds all deterministic random streams atomically");

	const PackageSaveStateCaptureResult beforeLegacyRandomRestore =
		randomSaveHost.capturePackageSaveState();
	PackageSaveStateSnapshot legacyRandomState = beforeLegacyRandomRestore.snapshot;
	legacyRandomState.engineRecords.clear();
	legacyRandomState.engineStatePresent = false;
	const PackageSaveStateLoadResult restoredLegacyRandomState =
		randomSaveHost.restorePackageSaveState(legacyRandomState);
	const PackageSaveStateCaptureResult afterLegacyRandomRestore =
		randomSaveHost.capturePackageSaveState();
	const bool legacyRandomUnchanged = beforeLegacyRandomRestore &&
		afterLegacyRandomRestore &&
		beforeLegacyRandomRestore.snapshot.engineRecords.size() == 2 &&
		afterLegacyRandomRestore.snapshot.engineRecords.size() == 2 &&
		beforeLegacyRandomRestore.snapshot.engineRecords[0].random ==
			afterLegacyRandomRestore.snapshot.engineRecords[0].random &&
		beforeLegacyRandomRestore.snapshot.engineRecords[1].random ==
			afterLegacyRandomRestore.snapshot.engineRecords[1].random;
	check(restoredLegacyRandomState &&
		restoredLegacyRandomState.engineRecordsRestored == 0 && legacyRandomUnchanged,
		"v1 package state callbacks cannot perturb live random streams");

	PackageSaveStateSnapshot invalidLaterRandomState =
		afterLegacyRandomRestore.snapshot;
	if (invalidLaterRandomState.engineRecords.size() == 2)
		invalidLaterRandomState.engineRecords[1].random.schema = 99;
	const PackageSaveStateLoadResult invalidLaterRandomRestore =
		randomSaveHost.restorePackageSaveState(invalidLaterRandomState);
	const PackageSaveStateCaptureResult afterInvalidRandomRestore =
		randomSaveHost.capturePackageSaveState();
	const bool invalidRandomUnchanged = afterLegacyRandomRestore &&
		afterInvalidRandomRestore &&
		afterLegacyRandomRestore.snapshot.engineRecords.size() == 2 &&
		afterInvalidRandomRestore.snapshot.engineRecords.size() == 2 &&
		afterLegacyRandomRestore.snapshot.engineRecords[0].random ==
			afterInvalidRandomRestore.snapshot.engineRecords[0].random &&
		afterLegacyRandomRestore.snapshot.engineRecords[1].random ==
			afterInvalidRandomRestore.snapshot.engineRecords[1].random;
	check(invalidLaterRandomRestore.error ==
			PackageSaveStateError::EngineStateMismatch &&
		invalidLaterRandomRestore.packageId == "rules.random-save-b" &&
		invalidRandomUnchanged,
		"a later invalid package checkpoint leaves every earlier RNG unchanged");

	TransactionalLifecyclePackage transactionalPackage("rules.transactional-session");
	EngineHost<unsigned> transactionalHost;
	const bool transactionalRegistered =
		transactionalHost.packages().registerPackage(transactionalPackage) ==
			PackageRegistrationError::None &&
		transactionalHost.packages().activate("rules.transactional-session") ==
			PackageActivationError::None;
	const RuntimeSessionTransitionResult initializationStarted =
		transactionalHost.tryBeginInitialization();
	const RuntimeSessionAdvanceResult contentLoaded =
		transactionalHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::LoadContent);
	const RuntimeSessionTransitionResult prematureRunning =
		transactionalHost.tryMarkRunning();
	const RuntimeSessionTransitionResult cancelledInitialization =
		transactionalHost.tryCancelInitialization();
	const RuntimeSessionTransitionResult repeatedCancellation =
		transactionalHost.tryCancelInitialization();
	check(transactionalRegistered && initializationStarted && contentLoaded &&
		prematureRunning.error == RuntimeSessionError::PackageBootstrapIncomplete &&
		prematureRunning.lifecycle == EngineLifecycle::Initializing &&
		cancelledInitialization &&
		cancelledInitialization.lifecycle == EngineLifecycle::Stopped &&
		cancelledInitialization.completedPackagePhases == 0 &&
		cancelledInitialization.rollback.packages.shutdownPhases == 2 &&
		cancelledInitialization.rollback.packages.callbacks == 2 &&
		cancelledInitialization.rollback.packages.callbackFailures == 0 &&
		repeatedCancellation.error == RuntimeSessionError::InvalidState &&
		transactionalPackage.active && transactionalPackage.deactivateCalls == 0 &&
		transactionalPackage.bootstrapCalls == std::vector<int>({0, 1}) &&
		transactionalPackage.shutdownCalls == std::vector<int>({1, 0}) &&
		transactionalHost.packages().completedBootstrapPhases() == 0,
		"initialization cancellation rolls back completed phases without deactivation");

	TransactionalLifecyclePackage reentrantPackage("rules.reentrant-cancel");
	EngineHost<unsigned> reentrantHost;
	reentrantPackage.cancelDuringBootstrap = &reentrantHost.runtimeSession();
	const bool reentrantReady =
		reentrantHost.packages().registerPackage(reentrantPackage) ==
			PackageRegistrationError::None &&
		reentrantHost.packages().activate("rules.reentrant-cancel") ==
			PackageActivationError::None &&
		reentrantHost.beginInitialization();
	const RuntimeSessionAdvanceResult reentrantAdvance =
		reentrantHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::Configure);
	const RuntimeSessionTransitionResult reentrantRetry =
		reentrantHost.tryCancelInitialization();
	check(reentrantReady && reentrantAdvance &&
		reentrantPackage.cancelDuringBootstrapResult.error ==
			RuntimeSessionError::PackageRollbackFailed &&
		reentrantPackage.cancelDuringBootstrapResult.rollback.packages.error ==
			PackageBootstrapShutdownError::OperationInProgress &&
		reentrantPackage.cancelDuringBootstrapResult.lifecycle ==
			EngineLifecycle::Initializing &&
		reentrantRetry && reentrantRetry.lifecycle == EngineLifecycle::Stopped &&
		reentrantPackage.shutdownCalls == std::vector<int>({0}),
		"reentrant cancellation stays initializing until rollback can be retried");

	TransactionalLifecyclePackage reentrantShutdownPackage("rules.reentrant-shutdown");
	EngineHost<unsigned> reentrantShutdownHost;
	const bool reentrantShutdownReady =
		reentrantShutdownHost.packages().registerPackage(reentrantShutdownPackage) ==
			PackageRegistrationError::None &&
		reentrantShutdownHost.packages().activate("rules.reentrant-shutdown") ==
			PackageActivationError::None &&
		reentrantShutdownHost.beginInitialization() &&
		reentrantShutdownHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::StartRuntime) &&
		reentrantShutdownHost.markRunning() &&
		reentrantShutdownHost.beginShutdown();
	reentrantShutdownPackage.shutdownDuringShutdown =
		&reentrantShutdownHost.runtimeSession();
	const RuntimeSessionShutdownResult reentrantShutdown =
		reentrantShutdownHost.runtimeSession().shutdownPackages();
	const RuntimeSessionShutdownResult repeatedReentrantShutdown =
		reentrantShutdownHost.runtimeSession().shutdownPackages();
	const RuntimeSessionTransitionResult reentrantShutdownStopped =
		reentrantShutdownHost.tryMarkStopped();
	check(reentrantShutdownReady &&
		reentrantShutdownPackage.shutdownDuringShutdownResult.error ==
			RuntimeSessionError::PackageShutdownFailed &&
		reentrantShutdownPackage.shutdownDuringShutdownResult.packages.bootstrap.packages.error ==
			PackageBootstrapShutdownError::OperationInProgress &&
		reentrantShutdownPackage.shutdownDuringShutdownResult.packages.deactivation.error ==
			PackageDeactivationError::OperationInProgress &&
		reentrantShutdown && repeatedReentrantShutdown && reentrantShutdownStopped &&
		reentrantShutdownPackage.shutdownCalls == std::vector<int>({2, 1, 0}) &&
		reentrantShutdownPackage.deactivateCalls == 1 &&
		!reentrantShutdownPackage.active &&
		reentrantShutdownHost.lifecycle() == EngineLifecycle::Stopped,
		"reentrant shutdown contention does not poison the completing outer transaction");

	const RuntimeSessionTransitionResult retryStarted =
		transactionalHost.tryBeginInitialization();
	const RuntimeSessionAdvanceResult retryBootstrapped =
		transactionalHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::StartRuntime);
	const RuntimeSessionTransitionResult retryRunning =
		transactionalHost.tryMarkRunning();
	const RuntimeSessionTransitionResult shutdownStarted =
		transactionalHost.tryBeginShutdown();
	const RuntimeSessionTransitionResult prematureStopped =
		transactionalHost.tryMarkStopped();
	const RuntimeSessionShutdownResult finalShutdown =
		transactionalHost.runtimeSession().shutdownPackages();
	const RuntimeSessionShutdownResult repeatedShutdown =
		transactionalHost.runtimeSession().shutdownPackages();
	const RuntimeSessionTransitionResult finalStopped =
		transactionalHost.tryMarkStopped();
	check(retryStarted && retryBootstrapped && retryRunning && shutdownStarted &&
		prematureStopped.error == RuntimeSessionError::PackageShutdownIncomplete &&
		prematureStopped.lifecycle == EngineLifecycle::ShuttingDown && finalShutdown &&
		finalShutdown.packages.shutdownPhases == 3 &&
		finalShutdown.packages.bootstrap.packages.callbacks == 3 && repeatedShutdown &&
		repeatedShutdown.packages.shutdownPhases == 0 &&
		repeatedShutdown.packages.bootstrap.packages.callbacks == 0 && finalStopped &&
		transactionalPackage.bootstrapCalls == std::vector<int>({0, 1, 0, 1, 2}) &&
		transactionalPackage.shutdownCalls == std::vector<int>({1, 0, 2, 1, 0}) &&
		transactionalPackage.deactivateCalls == 1 && !transactionalPackage.active &&
		transactionalHost.lifecycle() == EngineLifecycle::Stopped,
		"rolled-back packages retry cleanly and final shutdown callbacks run only once");

	TransactionalLifecyclePackage failingRollbackPackage("rules.rollback-failure");
	failingRollbackPackage.throwOnShutdownPhase = 1;
	EngineHost<unsigned> failingRollbackHost;
	const bool failingRollbackReady =
		failingRollbackHost.packages().registerPackage(failingRollbackPackage) ==
			PackageRegistrationError::None &&
		failingRollbackHost.packages().activate("rules.rollback-failure") ==
			PackageActivationError::None &&
		failingRollbackHost.beginInitialization() &&
		failingRollbackHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::LoadContent);
	const RuntimeSessionTransitionResult failedRollback =
		failingRollbackHost.tryCancelInitialization();
	check(failingRollbackReady &&
		failedRollback.error == RuntimeSessionError::PackageRollbackFailed &&
		failedRollback.rollback.packages.error ==
			PackageBootstrapShutdownError::CallbackFailed &&
		failedRollback.rollback.packages.shutdownPhases == 2 &&
		failedRollback.rollback.packages.callbacks == 2 &&
		failedRollback.rollback.packages.callbackFailures == 1 &&
		failingRollbackPackage.shutdownCalls == std::vector<int>({1, 0}) &&
		failingRollbackHost.packages().completedBootstrapPhases() == 0 &&
		failingRollbackHost.lifecycle() == EngineLifecycle::Stopped,
		"rollback callback failures remain structured after best-effort cleanup");

	TransactionalLifecyclePackage failedPhaseRollbackPackage(
		"rules.failed-phase-rollback");
	failedPhaseRollbackPackage.failOnBootstrapPhase =
		static_cast<int>(PackageBootstrapPhase::LoadContent);
	failedPhaseRollbackPackage.throwOnShutdownPhase =
		static_cast<int>(PackageBootstrapPhase::LoadContent);
	EngineHost<unsigned> failedPhaseRollbackHost;
	const bool failedPhaseRollbackReady =
		failedPhaseRollbackHost.packages().registerPackage(failedPhaseRollbackPackage) ==
			PackageRegistrationError::None &&
		failedPhaseRollbackHost.packages().activate("rules.failed-phase-rollback") ==
			PackageActivationError::None &&
		failedPhaseRollbackHost.beginInitialization();
	const RuntimeSessionAdvanceResult failedPhaseRollback =
		failedPhaseRollbackHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::LoadContent);
	const RuntimeSessionTransitionResult failedPhaseRollbackRecovery =
		failedPhaseRollbackHost.tryCancelInitialization();
	check(failedPhaseRollbackReady &&
		failedPhaseRollback.error == RuntimeSessionError::PackageBootstrapFailed &&
		failedPhaseRollback.packages.error == PackageBootstrapError::CallbackFailed &&
		failedPhaseRollback.packages.phase == PackageBootstrapPhase::LoadContent &&
		failedPhaseRollback.packages.rolledBack &&
		failedPhaseRollback.packages.completedPhases == 0 &&
		failedPhaseRollback.packages.rollback.packages.error ==
			PackageBootstrapShutdownError::CallbackFailed &&
		failedPhaseRollback.packages.rollback.packages.shutdownPhases == 2 &&
		failedPhaseRollback.packages.rollback.packages.callbacks == 2 &&
		failedPhaseRollback.packages.rollback.packages.callbackFailures == 1 &&
		failedPhaseRollbackPackage.shutdownCalls == std::vector<int>({1, 0}) &&
		failedPhaseRollbackRecovery &&
		failedPhaseRollbackHost.lifecycle() == EngineLifecycle::Stopped,
		"failed-phase rollback failures propagate through lifecycle and session diagnostics");
	const bool failingFinalShutdownReady =
		failingRollbackHost.beginInitialization() &&
		failingRollbackHost.runtimeSession().advancePackagesTo(
			PackageBootstrapPhase::StartRuntime) &&
		failingRollbackHost.markRunning() && failingRollbackHost.beginShutdown();
	const RuntimeSessionShutdownResult failedFinalShutdown =
		failingRollbackHost.runtimeSession().shutdownPackages();
	const RuntimeSessionShutdownResult repeatedFailedFinalShutdown =
		failingRollbackHost.runtimeSession().shutdownPackages();
	const RuntimeSessionTransitionResult rejectedStoppedState =
		failingRollbackHost.tryMarkStopped();
	check(failingFinalShutdownReady &&
		failedFinalShutdown.error == RuntimeSessionError::PackageShutdownFailed &&
		failedFinalShutdown.packages.bootstrap.packages.callbackFailures == 1 &&
		repeatedFailedFinalShutdown.error == RuntimeSessionError::PackageShutdownFailed &&
		repeatedFailedFinalShutdown.packages.bootstrap.packages.callbacks == 0 &&
		rejectedStoppedState.error == RuntimeSessionError::PackageShutdownIncomplete &&
		failingRollbackPackage.shutdownCalls == std::vector<int>({1, 0, 2, 1, 0}) &&
		failingRollbackPackage.deactivateCalls == 1 &&
		failingRollbackHost.lifecycle() == EngineLifecycle::ShuttingDown,
		"a failed final rollback cannot be hidden by a no-op repeated shutdown");

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
	DeclaredContentPackage dependencyBaseA(PackageDescriptor{
		ContentManifest{"rules.dependency-a", "1", ContentApiVersion{1, 3}},
		PackageKind::Rules});
	DeclaredContentPackage dependencyBaseB(PackageDescriptor{
		ContentManifest{"rules.dependency-b", "1", ContentApiVersion{1, 3}},
		PackageKind::Rules});
	DeclaredContentPackage dependencyConsumerSecond(PackageDescriptor{
		ContentManifest{"mod.consumer-second", "1", ContentApiVersion{1, 3},
			{{"rules.dependency-b", {}}}, {{"rules.dependency-a", {}}}},
		PackageKind::Extension});
	DeclaredContentPackage dependencyConsumerFirst(PackageDescriptor{
		ContentManifest{"mod.consumer-first", "1", ContentApiVersion{1, 3},
			{{"rules.dependency-a", {}}}, {{"rules.dependency-b", {}}}},
		PackageKind::Extension});
	DeclaredContentPackage duplicateDependencyConsumer(PackageDescriptor{
		ContentManifest{"mod.consumer-duplicate", "1", ContentApiVersion{1, 3},
			{{"rules.dependency-a", {}}}, {{"rules.dependency-a", {}}}},
		PackageKind::Extension});
	EngineHost<unsigned> dependencyCatalogHost;
	const bool dependenciesRegistered =
		dependencyCatalogHost.packages().registerPackage(dependencyBaseA) ==
			PackageRegistrationError::None &&
		dependencyCatalogHost.packages().registerPackage(dependencyBaseB) ==
			PackageRegistrationError::None &&
		dependencyCatalogHost.packages().registerPackage(dependencyConsumerSecond) ==
			PackageRegistrationError::None &&
		dependencyCatalogHost.packages().registerPackage(dependencyConsumerFirst) ==
			PackageRegistrationError::None;
	const PackageCatalogSnapshot dependencyCatalog =
		dependencyCatalogHost.packageCatalog();
	const PackageCatalogEntry* dependencyA = dependencyCatalog.find("rules.dependency-a");
	const PackageCatalogEntry* dependencyB = dependencyCatalog.find("rules.dependency-b");
	check(dependenciesRegistered && dependencyA && dependencyB &&
		dependencyA->dependents == std::vector<std::string>({
			"mod.consumer-second", "mod.consumer-first"}) &&
		dependencyB->dependents == std::vector<std::string>({
			"mod.consumer-second", "mod.consumer-first"}) &&
		dependencyCatalogHost.packages().registerPackage(duplicateDependencyConsumer) ==
			PackageRegistrationError::InvalidRelationship,
		"indexed package catalogs preserve consumer order and reject duplicate relationships");

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
		sessionHost.configuration().size() == 24,
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
		diagnostics.configuration.size() == 24 &&
		diagnostics.compatibility == sessionHost.compatibilityFingerprint() &&
		diagnostics.queuedMessages == 0 &&
		diagnostics.completedFrames == 0 && diagnostics.completedSimulationTicks == 0,
		"runtime diagnostics capture one pointer-free ordered host snapshot");
	check(runtimeReport.lifecycle == EngineLifecycle::Stopped && runtimeReport.healthy() &&
		runtimeReport.completedFrames == 0 && runtimeReport.completedSimulationTicks == 0 &&
		runtimeReport.registeredPackages == 0 && runtimeReport.activePackages == 0 &&
		runtimeReport.services.size() == 15 && runtimeReport.configuration.size() == 24 &&
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

	DeterministicCommandQueue<int> expectedNextCommands;
	expectedNextCommands.enqueue(20, 200);
	const std::uint64_t expectedNextSequence =
		expectedNextCommands.enqueue(10, 100);
	expectedNextCommands.enqueue(10, 101);
	std::vector<int> expectedNextDelivered;
	std::vector<std::uint64_t> expectedNextObserved;
	const ExpectedCommandProcessingResult expectedNextProcessed =
		ProcessExpectedNextCommandThrough(
			expectedNextCommands, 10, expectedNextSequence,
			[&expectedNextDelivered](int command, std::uint64_t, std::uint64_t) {
				expectedNextDelivered.push_back(command);
				return CommandDisposition::Applied;
			},
			[&expectedNextObserved](
				int, std::uint64_t, std::uint64_t sequence, CommandDisposition) {
				expectedNextObserved.push_back(sequence);
			});
	check(expectedNextProcessed &&
		expectedNextProcessed.processing.scheduled == 1 &&
		expectedNextProcessed.processing.applied == 1 &&
		expectedNextDelivered == std::vector<int>({100}) &&
		expectedNextObserved == std::vector<std::uint64_t>({expectedNextSequence}) &&
		expectedNextCommands.size() == 2,
		"expected-next processing normalizes order and invokes only its exact command");

	DeterministicCommandQueue<int> precededCommands;
	const std::uint64_t precededSequence = precededCommands.enqueue(10, 100);
	const std::uint64_t precedingSequence = precededCommands.enqueue(9, 90);
	bool precededInvoked = false;
	const ExpectedCommandProcessingResult preceded =
		ProcessExpectedNextCommandThrough(
			precededCommands, 10, precededSequence,
			[&precededInvoked](int, std::uint64_t, std::uint64_t) {
				precededInvoked = true;
				return CommandDisposition::Applied;
			});
	check(preceded.status ==
			ExpectedCommandProcessStatus::DifferentCommandReady &&
		preceded.observedTick == 9 &&
		preceded.observedSequence == precedingSequence && !precededInvoked &&
		precededCommands.size() == 2,
		"expected-next processing exposes authoritative backlog without consuming it");

	DeterministicCommandQueue<int> gatedExpectedCommands;
	const std::uint64_t gatedExpectedSequence =
		gatedExpectedCommands.enqueue(20, 200);
	bool gatedExpectedInvoked = false;
	const ExpectedCommandProcessingResult notReady =
		ProcessExpectedNextCommandThrough(
			gatedExpectedCommands, 10, gatedExpectedSequence,
			[&gatedExpectedInvoked](int, std::uint64_t, std::uint64_t) {
				gatedExpectedInvoked = true;
				return CommandDisposition::Applied;
			});
	const ExpectedCommandProcessingResult retryExpected =
		ProcessExpectedNextCommandThrough(
			gatedExpectedCommands, 20, gatedExpectedSequence,
			[](int, std::uint64_t, std::uint64_t) {
				return CommandDisposition::Retry;
			});
	check(notReady.status == ExpectedCommandProcessStatus::NoCommandReady &&
		!gatedExpectedInvoked &&
		retryExpected.status == ExpectedCommandProcessStatus::Retry &&
		retryExpected.processing.blockedSequence == gatedExpectedSequence &&
		gatedExpectedCommands.containsSequence(gatedExpectedSequence),
		"expected-next processing distinguishes tick gating and retains retryable work");

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

	StateRegistry<unsigned> reentrantStates;
	StateInitializationError nestedInitialization = StateInitializationError::None;
	StateRegistrationError nestedRegistration = StateRegistrationError::None;
	StateShutdownError nestedShutdown = StateShutdownError::None;
	StateHandleError nestedHandle = StateHandleError::None;
	check(reentrantStates.registerState(8, StateCallbacks<unsigned>{
		[&] {
			nestedInitialization = reentrantStates.initialize(8);
			nestedRegistration = reentrantStates.registerState(9,
				StateCallbacks<unsigned>{[] { return true; }, [] { return 9u; }, [] {}});
			return true;
		},
		[&] {
			nestedShutdown = reentrantStates.shutdown(8);
			return 8u;
		},
		[&] { nestedHandle = reentrantStates.handle(8).error; }}) ==
			StateRegistrationError::None &&
		reentrantStates.initialize(8) == StateInitializationError::None &&
		reentrantStates.handle(8) &&
		reentrantStates.shutdown(8) == StateShutdownError::None &&
		nestedInitialization == StateInitializationError::OperationInProgress &&
		nestedRegistration == StateRegistrationError::OperationInProgress &&
		nestedShutdown == StateShutdownError::OperationInProgress &&
		nestedHandle == StateHandleError::OperationInProgress &&
		reentrantStates.size() == 1,
		"state registry rejects callback reentrancy without invalidating entries");

	MemoryInputSource nestedInputSource;
	InputDispatcher nestedInput(nestedInputSource, 2);
	CallbackInputSink nestedInputSink;
	InputDispatchResult nestedInputResult;
	nestedInputSink.callback = [&] { nestedInputResult = nestedInput.dispatchPending(); };
	nestedInput.addSink(nestedInputSink);
	nestedInputSource.push(EngineInputEvent{});
	const InputDispatchResult outerInputResult = nestedInput.dispatchPending();

	RuntimeUpdateDispatcher nestedUpdates;
	CallbackRuntimeUpdateSink nestedUpdateSink;
	RuntimeUpdateDispatchResult nestedUpdateResult;
	nestedUpdateSink.callback = [&] {
		nestedUpdateResult = nestedUpdates.dispatch(RuntimeUpdateContext{});
	};
	nestedUpdates.addSink(nestedUpdateSink);
	const RuntimeUpdateDispatchResult outerUpdateResult =
		nestedUpdates.dispatch(RuntimeUpdateContext{});

	SimulationTickDispatcher nestedTicks(1, 1);
	CallbackSimulationTickSink nestedTickSink;
	SimulationTickDispatchResult nestedTickResult;
	nestedTickSink.callback = [&] { nestedTickResult = nestedTicks.advance(1); };
	nestedTicks.addSink(nestedTickSink);
	const SimulationTickDispatchResult outerTickResult = nestedTicks.advance(1);

	RuntimeMessageBus nestedMessages(2, 8);
	CallbackMessageSink nestedMessageSink;
	RuntimeMessageDispatchResult nestedMessageResult;
	nestedMessageSink.callback = [&] {
		nestedMessageResult = nestedMessages.dispatchPending();
	};
	nestedMessages.addSink(nestedMessageSink);
	nestedMessages.publish(RuntimeMessageRequest{"nested.message", "engine.test", {}});
	const RuntimeMessageDispatchResult outerMessageResult =
		nestedMessages.dispatchPending();
	check(outerInputResult.polled == 1 && nestedInputResult.operationInProgress &&
		outerUpdateResult.delivered == 1 && nestedUpdateResult.operationInProgress &&
		outerTickResult.executed == 1 && nestedTickResult.operationInProgress &&
		outerMessageResult.messages == 1 && nestedMessageResult.operationInProgress,
		"core fan-out dispatchers reject nested work while completing the outer pass");

	PackageTaskQueue nestedTasks(4, 2);
	PackageTaskDrainResult nestedTaskResult;
	std::size_t nestedRemoval = 1;
	unsigned nestedTaskRuns = 0;
	nestedTasks.schedule("nested.tasks", [&] {
		++nestedTaskRuns;
		nestedTaskResult = nestedTasks.drain();
		nestedRemoval = nestedTasks.removePackage("nested.tasks");
		nestedTasks.schedule("nested.tasks", [&] { ++nestedTaskRuns; });
	});
	nestedTasks.schedule("nested.tasks", [&] { ++nestedTaskRuns; });
	const PackageTaskDrainResult outerTaskResult = nestedTasks.drain();
	const PackageTaskDrainResult deferredTaskResult = nestedTasks.drain();
	check(outerTaskResult.executed == 2 && nestedTaskResult.operationInProgress &&
		nestedRemoval == 0 && deferredTaskResult.executed == 1 &&
		nestedTaskRuns == 3 && nestedTasks.size() == 0,
		"package task draining rejects recursion and defers newly scheduled work");

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

	ManualTimeSource recoveryTime;
	MemoryInputSource recoveryInput;
	EngineServices recoveryServices{
		recoveryTime, ZeroRandomSource::instance(), NullByteStorage::instance(),
		NullLogSink::instance(), recoveryInput,
		NullAudioOutput::instance(), NullFramePresenter::instance(),
		NullAssetSource::instance()};
	RuntimeMessageBus recoveryMessages;
	InputDispatcher recoveryInputDispatcher(recoveryInput);
	RuntimeUpdateDispatcher recoveryUpdates;
	TestRuntimeUpdateSink recoveryUpdateSink;
	recoveryUpdates.addSink(recoveryUpdateSink);
	FrameTelemetry recoveryTelemetry;
	SimulationTickDispatcher recoveryTicks(10, 8);
	TestSimulationTickSink recoveryTickSink;
	recoveryTicks.addSink(recoveryTickSink);
	FrameDriver recoveryDriver(
		recoveryServices, recoveryMessages, recoveryInputDispatcher,
		recoveryUpdates, recoveryTelemetry, recoveryTicks);
	const FrameRunResult initialRecoveryFrame = recoveryDriver.runFrame(
		[] { return FramePlan{false, FramePresentMode::Paced}; }, [] {});
	recoveryTime.advanceMicroseconds(10);
	bool prepareFailed = false;
	try
	{
		recoveryDriver.runFrame(
			[]() -> FramePlan { throw std::runtime_error("prepare failed"); }, [] {});
	}
	catch (const std::runtime_error&)
	{
		prepareFailed = true;
	}
	recoveryTime.advanceMicroseconds(10);
	const FrameRunResult afterPrepareFailure = recoveryDriver.runFrame(
		[] { return FramePlan{false, FramePresentMode::Paced}; }, [] {});
	recoveryTime.advanceMicroseconds(10);
	bool completionFailed = false;
	try
	{
		recoveryDriver.runFrame(
			[] { return FramePlan{false, FramePresentMode::Paced}; },
			[] { throw std::runtime_error("completion failed"); });
	}
	catch (const std::runtime_error&)
	{
		completionFailed = true;
	}
	recoveryTime.advanceMicroseconds(10);
	const FrameRunResult afterCompletionFailure = recoveryDriver.runFrame(
		[] { return FramePlan{false, FramePresentMode::Paced}; }, [] {});
	bool nestedRejected = false;
	const FrameRunResult afterNestedAttempt = recoveryDriver.runFrame(
		[&] {
			try
			{
				recoveryDriver.runFrame(
					[] { return FramePlan{false, FramePresentMode::Paced}; }, [] {});
			}
			catch (const std::logic_error&)
			{
				nestedRejected = true;
			}
			return FramePlan{false, FramePresentMode::Paced};
		}, [] {});
	const FrameTelemetrySnapshot recoverySnapshot = recoveryTelemetry.snapshot();
	check(prepareFailed && completionFailed && nestedRejected &&
		initialRecoveryFrame.sequence == 1 &&
		afterPrepareFailure.sequence == 3 &&
		afterCompletionFailure.sequence == 5 &&
		afterNestedAttempt.sequence == 6 &&
		recoveryDriver.nextFrameSequence() == 7 &&
		recoveryDriver.completedFrames() == 4 &&
		recoveryUpdateSink.updates.size() == 6 &&
		recoveryUpdateSink.updates[1].frameSequence == 2 &&
		recoveryUpdateSink.updates[3].frameSequence == 4 &&
		recoveryTickSink.ticks.size() == 4 &&
		recoverySnapshot.summary.completedFrames == 4,
		"failed frames consume identity and committed elapsed time exactly once");
	check(afterPrepareFailure.runtimeUpdates.delivered == 1 &&
		afterPrepareFailure.simulationTicks.executed == 1 &&
		afterCompletionFailure.runtimeUpdates.delivered == 1 &&
		afterCompletionFailure.simulationTicks.executed == 1 &&
		afterNestedAttempt.runtimeUpdates.delivered == 1 &&
		afterNestedAttempt.simulationTicks.executed == 0,
		"frame recovery resumes without replaying a failed frame's simulation interval");

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
	EngineServices checkpointHostServices{
		ZeroTimeSource::instance(), ZeroRandomSource::instance(), checkpointStorage};
	EngineHost<unsigned> checkpointHost(checkpointHostServices);
	const RuntimeCompatibilityFingerprint currentHostFingerprint =
		checkpointHost.compatibilityFingerprint();
	const RuntimeCompatibilityFingerprint preAggregateHostFingerprint =
		checkpointHost.preAggregateCatalogCompatibilityFingerprint();
	const RuntimeCheckpoint preAggregateCheckpoint{
		preAggregateHostFingerprint, 31, 47, {}};
	check(currentHostFingerprint != preAggregateHostFingerprint &&
		checkpointHost.runtimeCheckpoints().save(
			"runtime.checkpoint-pre-aggregate", preAggregateCheckpoint) ==
			RuntimeCheckpointSaveError::None,
		"the reconstructed pre-aggregate host configuration has its own real fingerprint");
	RuntimeCheckpoint loadedPreAggregateCheckpoint;
	const RuntimeCheckpointLoadResult loadedPreAggregateResult =
		checkpointHost.loadRuntimeCheckpoint(
			"runtime.checkpoint-pre-aggregate", loadedPreAggregateCheckpoint);
	check(loadedPreAggregateResult &&
		loadedPreAggregateCheckpoint.compatibility == preAggregateHostFingerprint &&
		loadedPreAggregateCheckpoint.completedFrames == 31 &&
		loadedPreAggregateCheckpoint.completedSimulationTicks == 47,
		"runtime checkpoints made with the pre-aggregate configuration remain loadable");
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
	PackageSaveArchiveService packageArchives(checkpointPersistence, 2, 8, 256);
	PackageSaveArchive savedPackageArchive{firstFingerprint,
		PackageSaveStateSnapshot{{
			PackageSaveStateRecord{"rules.fingerprint", "2.0", 1, {4, 2}},
			PackageSaveStateRecord{"extension.state", "1.0", 3, {1, 3, 3, 7}}}}};
	savedPackageArchive.state.engineStatePresent = true;
	savedPackageArchive.state.engineRecords.push_back(PackageEngineSaveStateRecord{
		"rules.fingerprint", "2.0",
		PackageRandomCheckpoint{PackageRandomCheckpoint::CurrentSchema,
			"rules.fingerprint", {{"combat", 123, 7}}}});
	check(packageArchives.save("package-state", savedPackageArchive) ==
			PackageSaveArchiveSaveError::None,
		"package save archive writes ordered bounded state through persistence envelopes");
	PackageSaveArchive missingEngineMarker = savedPackageArchive;
	missingEngineMarker.state.engineStatePresent = false;
	check(packageArchives.save("package-state-invalid", missingEngineMarker) ==
			PackageSaveArchiveSaveError::InvalidArchive &&
		!checkpointStorage.exists("package-state-invalid"),
		"package save archives reject hidden engine records transactionally");
	PackageSaveArchiveService tightPackageArchives(
		checkpointPersistence, 2, 8, 12, 64);
	check(tightPackageArchives.save("package-state-over-budget", savedPackageArchive) ==
			PackageSaveArchiveSaveError::TotalTooLarge &&
		!checkpointStorage.exists("package-state-over-budget"),
		"engine-owned save records share the aggregate package-state byte budget");
	std::vector<std::uint8_t> encodedPackageArchive;
	checkpointStorage.readAll("package-state", encodedPackageArchive);
	PackageSaveArchive loadedPackageArchive;
	const PackageSaveArchiveLoadResult loadedPackageState = packageArchives.load(
		"package-state", firstFingerprint, loadedPackageArchive);
	check(loadedPackageState && loadedPackageArchive.compatibility == firstFingerprint &&
		loadedPackageArchive.state.records.size() == 2 &&
		loadedPackageArchive.state.records[1].packageId == "extension.state" &&
		loadedPackageArchive.state.records[1].schemaVersion == 3 &&
		loadedPackageArchive.state.records[1].payload ==
			std::vector<std::uint8_t>({1, 3, 3, 7}) &&
		loadedPackageArchive.state.engineStatePresent &&
		loadedPackageArchive.state.engineRecords.size() == 1 &&
		loadedPackageArchive.state.engineRecords[0].random.streams.size() == 1 &&
		loadedPackageArchive.state.engineRecords[0].random.streams[0].state == 123 &&
		encodedPackageArchive.size() >= 6 && encodedPackageArchive[4] == 2 &&
		encodedPackageArchive[5] == 0,
		"package save archive v2 round-trips opaque and engine-owned state");
	PackageSaveArchive tightLoadOutput{firstFingerprint,
		PackageSaveStateSnapshot{{PackageSaveStateRecord{"unchanged", "1", 1, {9}}}}};
	const PackageSaveArchiveLoadResult tightLoad = tightPackageArchives.load(
		"package-state", firstFingerprint, tightLoadOutput);
	check(tightLoad.error == PackageSaveArchiveLoadError::TotalTooLarge &&
		tightLoadOutput.state.records.size() == 1 &&
		tightLoadOutput.state.records[0].packageId == "unchanged",
		"engine-state budget failures do not publish partially decoded archives");
	const std::vector<std::uint8_t> versionOneArchiveFixture{
		0x50, 0x47, 0x53, 0x54, 0x01, 0x00, 0x2f, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0xb5, 0x6d, 0x61, 0xed, 0x01, 0x00, 0x00, 0x00,
		0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
		0x00, 0x00, 0x70, 0x01, 0x00, 0x00, 0x00, 0x76, 0x01, 0x00, 0x00,
		0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f};
	checkpointStorage.writeAll("package-state-v1", versionOneArchiveFixture);
	PackageSaveArchive loadedVersionOneArchive;
	const PackageSaveArchiveLoadResult loadedVersionOneState = packageArchives.load(
		"package-state-v1", RuntimeCompatibilityFingerprint{1, 2, 3},
		loadedVersionOneArchive);
	check(loadedVersionOneState && loadedVersionOneArchive.state.records.size() == 1 &&
		loadedVersionOneArchive.state.records[0].packageId == "p" &&
		loadedVersionOneArchive.state.records[0].packageVersion == "v" &&
		loadedVersionOneArchive.state.records[0].payload ==
			std::vector<std::uint8_t>({0x7f}) &&
		!loadedVersionOneArchive.state.engineStatePresent &&
		loadedVersionOneArchive.state.engineRecords.empty(),
		"package save archive loads the exact pre-v2 byte fixture unchanged");
	BinaryWriter preAggregateVersionOnePayload;
	preAggregateVersionOnePayload.writeU32(preAggregateHostFingerprint.schema);
	preAggregateVersionOnePayload.writeU64(preAggregateHostFingerprint.high);
	preAggregateVersionOnePayload.writeU64(preAggregateHostFingerprint.low);
	preAggregateVersionOnePayload.writeU32(1);
	preAggregateVersionOnePayload.writeString("p");
	preAggregateVersionOnePayload.writeString("v");
	preAggregateVersionOnePayload.writeU32(1);
	preAggregateVersionOnePayload.writeU64(1);
	preAggregateVersionOnePayload.writeU8(0x7f);
	check(checkpointPersistence.saveEnvelope("package-state-pre-aggregate-v1",
			PersistenceHeader{0x54534750u, 1}, preAggregateVersionOnePayload.bytes()) ==
			PersistenceSaveResult::Success,
		"tests can reproduce the pre-v2 package archive with a real old host fingerprint");
	PackageSaveArchive loadedPreAggregateVersionOneArchive;
	const PackageSaveArchiveLoadResult loadedPreAggregateVersionOne =
		packageArchives.load("package-state-pre-aggregate-v1", currentHostFingerprint,
			preAggregateHostFingerprint, loadedPreAggregateVersionOneArchive);
	check(loadedPreAggregateVersionOne &&
		loadedPreAggregateVersionOneArchive.compatibility == preAggregateHostFingerprint &&
		loadedPreAggregateVersionOneArchive.state.records.size() == 1 &&
		loadedPreAggregateVersionOneArchive.state.records[0].payload ==
			std::vector<std::uint8_t>({0x7f}),
		"v1 package archives validate against the reconstructed pre-aggregate fingerprint");
	PackageSaveArchive rejectedPreAggregateVersionOne;
	const PackageSaveArchiveLoadResult rejectedWithoutPreAggregateFingerprint =
		packageArchives.load("package-state-pre-aggregate-v1", currentHostFingerprint,
			rejectedPreAggregateVersionOne);
	check(rejectedWithoutPreAggregateFingerprint.error ==
			PackageSaveArchiveLoadError::IncompatibleRuntime,
		"v1 package archive compatibility remains validated rather than bypassed");
	PackageSaveArchive preAggregateVersionTwoArchive{
		preAggregateHostFingerprint, PackageSaveStateSnapshot{}};
	preAggregateVersionTwoArchive.state.engineStatePresent = true;
	check(packageArchives.save(
			"package-state-pre-aggregate-v2", preAggregateVersionTwoArchive) ==
			PackageSaveArchiveSaveError::None,
		"tests can encode a structurally valid v2 archive with the legacy fingerprint");
	PackageSaveArchive rejectedPreAggregateVersionTwo;
	const PackageSaveArchiveLoadResult rejectedPreAggregateVersionTwoResult =
		packageArchives.load("package-state-pre-aggregate-v2", currentHostFingerprint,
			preAggregateHostFingerprint, rejectedPreAggregateVersionTwo);
	check(rejectedPreAggregateVersionTwoResult.error ==
			PackageSaveArchiveLoadError::IncompatibleRuntime,
		"v2 package archives cannot use the pre-aggregate compatibility alternative");
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
