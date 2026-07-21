#ifndef ENGINE_CORE_ENGINE_HOST_H
#define ENGINE_CORE_ENGINE_HOST_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <Engine/Core/ContentApi.h>
#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/AudioGroupService.h>
#include <Engine/Core/DefinitionCatalog.h>
#include <Engine/Core/EntityRegistry.h>
#include <Engine/Core/EngineServices.h>
#include <Engine/Core/FrameDriver.h>
#include <Engine/Core/InputDispatcher.h>
#include <Engine/Core/LocalizationCatalog.h>
#include <Engine/Core/PackageApi.h>
#include <Engine/Core/PackageEventSink.h>
#include <Engine/Core/PackageLifecycle.h>
#include <Engine/Core/PackageTaskQueue.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/RuntimeCheckpoint.h>
#include <Engine/Core/RuntimeDiagnostics.h>
#include <Engine/Core/RuntimeFaultJournal.h>
#include <Engine/Core/RuntimeReport.h>
#include <Engine/Core/RuntimeReportService.h>
#include <Engine/Core/RuntimeSession.h>
#include <Engine/Core/RuntimeUpdate.h>
#include <Engine/Core/SimulationTick.h>
#include <Engine/Core/StateController.h>
#include <Engine/Core/StateRegistry.h>
#include <Engine/Core/StateStack.h>

// Command- and game-agnostic composition root for engine applications, tools,
// package hosts, and tests. The application owns platform adapters and package
// objects; both must outlive this host and its non-owning references.
template<typename ScreenId = std::uint32_t>
class EngineHost
{
public:
	explicit EngineHost(
		EngineServices services = EngineServices::defaults(),
		ContentApiVersion supportedContentApi = CurrentContentApiVersion,
		PackageEventSink& packageEvents = NullPackageEventSink::instance(),
		RuntimeCapabilities hostCapabilities = {},
		std::uint64_t packageRandomSeed = 0,
		std::size_t packageRandomStreamLimit = 64,
		std::uint64_t simulationStepMicroseconds = 16667,
		std::size_t maximumSimulationCatchUpTicks = 4,
		std::size_t assetCacheEntries = 128,
		std::size_t assetCacheBytes = 64u * 1024u * 1024u,
		std::size_t runtimeFaultCapacity = 256,
		std::size_t localizationEntries = 65536,
		std::size_t localizationTextBytes = 16u * 1024u,
		std::size_t definitionEntries = 65536,
		std::size_t definitionPayloadBytes = 1024u * 1024u,
		std::size_t maximumEntities = 65536,
		std::size_t maximumPackageAudioPlaybacks = 1024,
		std::size_t maximumQueuedPackageTasks = 1024,
		std::size_t maximumPackageTasksPerFrame = 64,
		std::size_t maximumCheckpointPackages = 4096,
		std::size_t maximumRuntimeReportBytes = 4u * 1024u * 1024u)
		: content_(supportedContentApi),
		  audioGroups_(services.audio, maximumPackageAudioPlaybacks),
		  faultJournal_(runtimeFaultCapacity),
		  localization_(localizationEntries, localizationTextBytes),
		  definitions_(definitionEntries, definitionPayloadBytes),
		  entities_(maximumEntities),
		  hostCapabilities_(std::move(hostCapabilities)),
		  packageTasks_(maximumQueuedPackageTasks, maximumPackageTasksPerFrame),
		  packages_(content_, services, packageEvents, runtimeMessages_, serviceCatalog_,
		            runtimeConfiguration_, packageRandomSeed, packageRandomStreamLimit,
		            assetCacheEntries, assetCacheBytes, faultJournal_, localization_,
		            definitions_, entities_, audioGroups_, &hostCapabilities_, packageTasks_),
		  packageLifecycle_(packages_),
		  runtimeSession_(packageLifecycle_, serviceCatalog_, runtimeConfiguration_),
		  inputDispatcher_(packages_.services().input),
		  simulationTicks_(simulationStepMicroseconds, maximumSimulationCatchUpTicks),
		  frameDriver_(packages_.services(), runtimeMessages_, inputDispatcher_,
		               runtimeUpdates_, frameTelemetry_, simulationTicks_),
		  persistence_(packages_.services().storage),
		  runtimeCheckpoints_(persistence_, maximumCheckpointPackages),
		  runtimeReports_(persistence_, maximumRuntimeReportBytes)
	{
		serviceCatalog_.registerService(
			"engine.frame-telemetry", EngineServiceVersion{1, 0}, frameTelemetry_);
		serviceCatalog_.registerService(
			"engine.runtime-messages", EngineServiceVersion{1, 0}, runtimeMessages_);
		serviceCatalog_.registerService(
			"engine.persistence", EngineServiceVersion{1, 0}, persistence_);
		serviceCatalog_.registerService(
			"engine.simulation-ticks", EngineServiceVersion{1, 0}, simulationTicks_);
		serviceCatalog_.registerService(
			"engine.asset-cache", EngineServiceVersion{1, 0}, packages_.assetCache());
		serviceCatalog_.registerService(
			"engine.runtime-faults", EngineServiceVersion{1, 0}, faultJournal_);
		serviceCatalog_.registerService(
			"engine.localization", EngineServiceVersion{1, 0}, localization_);
		serviceCatalog_.registerService(
			"engine.definitions", EngineServiceVersion{1, 0}, definitions_);
		serviceCatalog_.registerService(
			"engine.entities", EngineServiceVersion{1, 0}, entities_);
		serviceCatalog_.registerService(
			"engine.package-audio", EngineServiceVersion{1, 0}, audioGroups_);
		serviceCatalog_.registerService(
			"engine.package-tasks", EngineServiceVersion{1, 0}, packageTasks_);
		serviceCatalog_.registerService(
			"engine.runtime-checkpoints", EngineServiceVersion{1, 0}, runtimeCheckpoints_);
		serviceCatalog_.registerService(
			"engine.runtime-reports", EngineServiceVersion{1, 0}, runtimeReports_);
		runtimeConfiguration_.set("engine.telemetry.history-capacity",
			static_cast<std::int64_t>(frameTelemetry_.capacity()));
		runtimeConfiguration_.set("engine.messages.queue-capacity",
			static_cast<std::int64_t>(runtimeMessages_.maxQueuedMessages()));
		runtimeConfiguration_.set("engine.messages.payload-limit",
			static_cast<std::int64_t>(runtimeMessages_.maxPayloadBytes()));
		runtimeConfiguration_.set("engine.simulation.step-microseconds",
			static_cast<std::int64_t>(simulationTicks_.stepMicroseconds()));
		runtimeConfiguration_.set("engine.simulation.maximum-catch-up-ticks",
			static_cast<std::int64_t>(simulationTicks_.maxCatchUpTicks()));
		runtimeConfiguration_.set("engine.assets.cache-entries",
			static_cast<std::int64_t>(packages_.assetCache().maximumEntries()));
		runtimeConfiguration_.set("engine.assets.cache-bytes",
			static_cast<std::int64_t>(packages_.assetCache().maximumBytes()));
		runtimeConfiguration_.set("engine.faults.history-capacity",
			static_cast<std::int64_t>(faultJournal_.capacity()));
		runtimeConfiguration_.set("engine.localization.entry-capacity",
			static_cast<std::int64_t>(localization_.maximumEntries()));
		runtimeConfiguration_.set("engine.localization.text-byte-limit",
			static_cast<std::int64_t>(localization_.maximumTextBytes()));
		runtimeConfiguration_.set("engine.definitions.entry-capacity",
			static_cast<std::int64_t>(definitions_.maximumEntries()));
		runtimeConfiguration_.set("engine.definitions.payload-byte-limit",
			static_cast<std::int64_t>(definitions_.maximumPayloadBytes()));
		runtimeConfiguration_.set("engine.entities.capacity",
			static_cast<std::int64_t>(entities_.maximumEntities()));
		runtimeConfiguration_.set("engine.package-audio.playback-capacity",
			static_cast<std::int64_t>(audioGroups_.maximumPlaybacks()));
		runtimeConfiguration_.set("engine.package-tasks.queue-capacity",
			static_cast<std::int64_t>(packageTasks_.maximumQueued()));
		runtimeConfiguration_.set("engine.package-tasks.per-frame-limit",
			static_cast<std::int64_t>(packageTasks_.maximumPerDrain()));
		runtimeConfiguration_.set("engine.checkpoints.package-limit",
			static_cast<std::int64_t>(runtimeCheckpoints_.maximumPackages()));
		runtimeConfiguration_.set("engine.reports.maximum-bytes",
			static_cast<std::int64_t>(runtimeReports_.maximumBytes()));
		inputDispatcher_.addSink(packages_);
		runtimeUpdates_.addSink(packages_);
		simulationTicks_.addSink(packages_);
		runtimeMessages_.addSink(packages_);
	}

	// PackageRegistry keeps references to this host's ContentRegistry. Stable
	// host identity is therefore an ownership invariant, not a convenience.
	EngineHost(const EngineHost&) = delete;
	EngineHost& operator=(const EngineHost&) = delete;
	EngineHost(EngineHost&&) = delete;
	EngineHost& operator=(EngineHost&&) = delete;

	EngineServices& services() { return packages_.services(); }
	const EngineServices& services() const { return packages_.services(); }
	LogSink& log() { return services().log; }
	StateStack<ScreenId>& screens() { return screenController_.stack(); }
	const StateStack<ScreenId>& screens() const { return screenController_.stack(); }
	StateController<ScreenId>& screenController() { return screenController_; }
	const StateController<ScreenId>& screenController() const { return screenController_; }
	StateRegistry<ScreenId>& stateRegistry() { return stateRegistry_; }
	const StateRegistry<ScreenId>& stateRegistry() const { return stateRegistry_; }
	FrameDriver& frameDriver() { return frameDriver_; }
	const FrameDriver& frameDriver() const { return frameDriver_; }
	InputDispatcher& inputDispatcher() { return inputDispatcher_; }
	const InputDispatcher& inputDispatcher() const { return inputDispatcher_; }
	RuntimeUpdateDispatcher& runtimeUpdates() { return runtimeUpdates_; }
	const RuntimeUpdateDispatcher& runtimeUpdates() const { return runtimeUpdates_; }
	SimulationTickDispatcher& simulationTicks() { return simulationTicks_; }
	const SimulationTickDispatcher& simulationTicks() const { return simulationTicks_; }
	FrameTelemetry& frameTelemetry() { return frameTelemetry_; }
	const FrameTelemetry& frameTelemetry() const { return frameTelemetry_; }
	RuntimeMessageBus& runtimeMessages() { return runtimeMessages_; }
	const RuntimeMessageBus& runtimeMessages() const { return runtimeMessages_; }
	CachingAssetSource& assetCache() { return packages_.assetCache(); }
	const CachingAssetSource& assetCache() const { return packages_.assetCache(); }
	RuntimeFaultJournal& runtimeFaults() { return faultJournal_; }
	const RuntimeFaultJournal& runtimeFaults() const { return faultJournal_; }
	LocalizationCatalog& localization() { return localization_; }
	const LocalizationCatalog& localization() const { return localization_; }
	DefinitionCatalog& definitions() { return definitions_; }
	const DefinitionCatalog& definitions() const { return definitions_; }
	EntityRegistry& entities() { return entities_; }
	const EntityRegistry& entities() const { return entities_; }
	AudioGroupService& packageAudio() { return audioGroups_; }
	const AudioGroupService& packageAudio() const { return audioGroups_; }
	PackageTaskQueue& packageTasks() { return packageTasks_; }
	const PackageTaskQueue& packageTasks() const { return packageTasks_; }
	PackageResourceUsageSnapshot packageResourceUsage() const
	{
		return BuildPackageResourceUsage(packages_.catalog(), localization_.snapshot(),
			definitions_.snapshot(), entities_.snapshot(), audioGroups_.snapshot(),
			packageTasks_.snapshot(), packages_.randomUsageSnapshot());
	}
	ServiceCatalog& serviceCatalog() { return serviceCatalog_; }
	const ServiceCatalog& serviceCatalog() const { return serviceCatalog_; }
	RuntimeConfiguration& configuration() { return runtimeConfiguration_; }
	const RuntimeConfiguration& configuration() const { return runtimeConfiguration_; }
	ContentRegistry& content() { return content_; }
	const ContentRegistry& content() const { return content_; }
	PackageRegistry& packages() { return packages_; }
	const PackageRegistry& packages() const { return packages_; }
	PackageLifecycle& packageLifecycle() { return packageLifecycle_; }
	const PackageLifecycle& packageLifecycle() const { return packageLifecycle_; }
	RuntimeSession& runtimeSession() { return runtimeSession_; }
	const RuntimeSession& runtimeSession() const { return runtimeSession_; }
	PackageCatalogSnapshot packageCatalog() const { return packages_.catalog(); }
	PackageSaveStateCaptureResult capturePackageSaveState() noexcept
	{
		return packages_.captureSaveState();
	}
	PackageSaveStateLoadResult validatePackageSaveState(
		const PackageSaveStateSnapshot& snapshot) const noexcept
	{
		return packages_.validateSaveState(snapshot);
	}
	PackageSaveStateLoadResult restorePackageSaveState(
		const PackageSaveStateSnapshot& snapshot) noexcept
	{
		return packages_.restoreSaveState(snapshot);
	}
	bool hasCapability(const std::string& capability) const
	{
		return hostCapabilities_.contains(capability) ||
			packages_.hasCapability(capability);
	}
	RuntimeCapabilities runtimeCapabilities() const
	{
		RuntimeCapabilities capabilities = hostCapabilities_;
		capabilities.addAll(packages_.activeCapabilities().ids());
		return capabilities;
	}
	RuntimeCompatibilityFingerprint compatibilityFingerprint() const
	{
		return BuildRuntimeCompatibilityFingerprint(
			packages_.catalog(), serviceCatalog_.snapshot(), runtimeConfiguration_.snapshot(),
			runtimeCapabilities(), definitions_.snapshot());
	}
	RuntimeDiagnosticsSnapshot diagnostics() const
	{
		return RuntimeDiagnosticsSnapshot{
			lifecycle(), frameTelemetry_.snapshot(), packages_.catalog(),
			packages_.assetCache().statistics(), faultJournal_.snapshot(),
			localization_.snapshot(), definitions_.snapshot(), entities_.snapshot(),
			audioGroups_.snapshot(), packageTasks_.snapshot(), packageResourceUsage(),
			serviceCatalog_.snapshot(),
			runtimeConfiguration_.snapshot(), runtimeCapabilities(),
			compatibilityFingerprint(),
			runtimeMessages_.queued(), frameDriver_.completedFrames(),
			simulationTicks_.completedTickSequence()};
	}
	RuntimeReport runtimeReport() const
	{
		return BuildRuntimeReport(diagnostics());
	}
	RuntimeReportService& runtimeReports() { return runtimeReports_; }
	const RuntimeReportService& runtimeReports() const { return runtimeReports_; }
	RuntimeReportSaveError saveRuntimeReport(const std::string& path) const noexcept
	{
		try { return runtimeReports_.save(path, runtimeReport()); }
		catch (...) { return RuntimeReportSaveError::AllocationFailure; }
	}
	bool setHostCapabilities(RuntimeCapabilities capabilities)
	{
		if (lifecycle() != EngineLifecycle::Stopped) return false;
		hostCapabilities_ = std::move(capabilities);
		return true;
	}
	PersistenceService& persistence() { return persistence_; }
	const PersistenceService& persistence() const { return persistence_; }
	RuntimeCheckpointService& runtimeCheckpoints() { return runtimeCheckpoints_; }
	const RuntimeCheckpointService& runtimeCheckpoints() const { return runtimeCheckpoints_; }
	RuntimeCheckpoint makeRuntimeCheckpoint() const
	{
		RuntimeCheckpoint checkpoint;
		checkpoint.compatibility = compatibilityFingerprint();
		checkpoint.completedFrames = frameDriver_.completedFrames();
		checkpoint.completedSimulationTicks = simulationTicks_.completedTickSequence();
		const PackageCatalogSnapshot catalog = packages_.catalog();
		checkpoint.activePackages.reserve(catalog.activationOrder.size());
		for (const std::string& packageId : catalog.activationOrder)
		{
			const PackageCatalogEntry* package = catalog.find(packageId);
			checkpoint.activePackages.push_back(RuntimeCheckpointPackage{
				packageId, package ? package->descriptor.content.version : std::string{}});
		}
		return checkpoint;
	}
	RuntimeCheckpointSaveError saveRuntimeCheckpoint(const std::string& path) const noexcept
	{
		try { return runtimeCheckpoints_.save(path, makeRuntimeCheckpoint()); }
		catch (...) { return RuntimeCheckpointSaveError::StorageError; }
	}
	RuntimeCheckpointLoadResult loadRuntimeCheckpoint(
		const std::string& path, RuntimeCheckpoint& checkpoint) const noexcept
	{
		try { return runtimeCheckpoints_.load(path, compatibilityFingerprint(), checkpoint); }
		catch (...) { return RuntimeCheckpointLoadResult{
			RuntimeCheckpointLoadError::StorageError, {}}; }
	}

	EngineLifecycle lifecycle() const { return runtimeSession_.lifecycle(); }
	bool beginInitialization() { return runtimeSession_.beginInitialization(); }
	bool cancelInitialization() { return runtimeSession_.cancelInitialization(); }
	bool markRunning() { return runtimeSession_.markRunning(); }
	bool beginShutdown() { return runtimeSession_.beginShutdown(); }
	bool markStopped() { return runtimeSession_.markStopped(); }

private:
	StateController<ScreenId> screenController_;
	StateRegistry<ScreenId> stateRegistry_;
	ContentRegistry content_;
	RuntimeMessageBus runtimeMessages_;
	ServiceCatalog serviceCatalog_;
	RuntimeConfiguration runtimeConfiguration_;
	AudioGroupService audioGroups_;
	RuntimeFaultJournal faultJournal_;
	LocalizationCatalog localization_;
	DefinitionCatalog definitions_;
	EntityRegistry entities_;
	RuntimeCapabilities hostCapabilities_;
	PackageTaskQueue packageTasks_;
	PackageRegistry packages_;
	PackageLifecycle packageLifecycle_;
	RuntimeSession runtimeSession_;
	InputDispatcher inputDispatcher_;
	RuntimeUpdateDispatcher runtimeUpdates_;
	SimulationTickDispatcher simulationTicks_;
	FrameTelemetry frameTelemetry_;
	FrameDriver frameDriver_;
	PersistenceService persistence_;
	RuntimeCheckpointService runtimeCheckpoints_;
	RuntimeReportService runtimeReports_;
};

#endif
