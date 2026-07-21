#ifndef ENGINE_CORE_ENGINE_HOST_H
#define ENGINE_CORE_ENGINE_HOST_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include <Engine/Core/ContentApi.h>
#include <Engine/Core/CachingAssetSource.h>
#include <Engine/Core/EngineServices.h>
#include <Engine/Core/FrameDriver.h>
#include <Engine/Core/InputDispatcher.h>
#include <Engine/Core/PackageApi.h>
#include <Engine/Core/PackageEventSink.h>
#include <Engine/Core/PackageLifecycle.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCapabilities.h>
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
		std::size_t assetCacheBytes = 64u * 1024u * 1024u)
		: content_(supportedContentApi),
		  packages_(content_, services, packageEvents, runtimeMessages_, serviceCatalog_,
		            runtimeConfiguration_, packageRandomSeed, packageRandomStreamLimit,
		            assetCacheEntries, assetCacheBytes),
		  packageLifecycle_(packages_),
		  runtimeSession_(packageLifecycle_, serviceCatalog_, runtimeConfiguration_),
		  inputDispatcher_(packages_.services().input),
		  simulationTicks_(simulationStepMicroseconds, maximumSimulationCatchUpTicks),
		  frameDriver_(packages_.services(), runtimeMessages_, inputDispatcher_,
		               runtimeUpdates_, frameTelemetry_, simulationTicks_),
		  persistence_(packages_.services().storage),
		  hostCapabilities_(std::move(hostCapabilities))
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
	bool setHostCapabilities(RuntimeCapabilities capabilities)
	{
		if (lifecycle() != EngineLifecycle::Stopped) return false;
		hostCapabilities_ = std::move(capabilities);
		return true;
	}
	PersistenceService& persistence() { return persistence_; }
	const PersistenceService& persistence() const { return persistence_; }

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
	PackageRegistry packages_;
	PackageLifecycle packageLifecycle_;
	RuntimeSession runtimeSession_;
	InputDispatcher inputDispatcher_;
	RuntimeUpdateDispatcher runtimeUpdates_;
	SimulationTickDispatcher simulationTicks_;
	FrameTelemetry frameTelemetry_;
	FrameDriver frameDriver_;
	PersistenceService persistence_;
	RuntimeCapabilities hostCapabilities_;
};

#endif
