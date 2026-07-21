#ifndef ENGINE_CORE_ENGINE_HOST_H
#define ENGINE_CORE_ENGINE_HOST_H

#include <cstdint>
#include <string>
#include <utility>

#include <Engine/Core/ContentApi.h>
#include <Engine/Core/EngineServices.h>
#include <Engine/Core/FrameDriver.h>
#include <Engine/Core/PackageApi.h>
#include <Engine/Core/PackageEventSink.h>
#include <Engine/Core/PackageLifecycle.h>
#include <Engine/Core/PersistenceService.h>
#include <Engine/Core/RuntimeCapabilities.h>
#include <Engine/Core/StateController.h>
#include <Engine/Core/StateRegistry.h>
#include <Engine/Core/StateStack.h>

enum class EngineLifecycle
{
	Stopped,
	Initializing,
	Running,
	ShuttingDown
};

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
		RuntimeCapabilities hostCapabilities = {})
		: content_(supportedContentApi), packages_(content_, services, packageEvents),
		  packageLifecycle_(packages_), frameDriver_(packages_.services()),
		  persistence_(packages_.services().storage),
		  hostCapabilities_(std::move(hostCapabilities))
	{
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
	ContentRegistry& content() { return content_; }
	const ContentRegistry& content() const { return content_; }
	PackageRegistry& packages() { return packages_; }
	const PackageRegistry& packages() const { return packages_; }
	PackageLifecycle& packageLifecycle() { return packageLifecycle_; }
	const PackageLifecycle& packageLifecycle() const { return packageLifecycle_; }
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
		if (lifecycle_ != EngineLifecycle::Stopped) return false;
		hostCapabilities_ = std::move(capabilities);
		return true;
	}
	PersistenceService& persistence() { return persistence_; }
	const PersistenceService& persistence() const { return persistence_; }

	EngineLifecycle lifecycle() const { return lifecycle_; }
	bool beginInitialization()
	{
		if (lifecycle_ != EngineLifecycle::Stopped) return false;
		lifecycle_ = EngineLifecycle::Initializing;
		return true;
	}
	bool cancelInitialization()
	{
		if (lifecycle_ != EngineLifecycle::Initializing) return false;
		lifecycle_ = EngineLifecycle::Stopped;
		return true;
	}
	bool markRunning()
	{
		if (lifecycle_ != EngineLifecycle::Initializing) return false;
		lifecycle_ = EngineLifecycle::Running;
		return true;
	}
	bool beginShutdown()
	{
		if (lifecycle_ != EngineLifecycle::Running &&
			lifecycle_ != EngineLifecycle::Initializing) return false;
		lifecycle_ = EngineLifecycle::ShuttingDown;
		return true;
	}
	bool markStopped()
	{
		if (lifecycle_ != EngineLifecycle::ShuttingDown) return false;
		lifecycle_ = EngineLifecycle::Stopped;
		return true;
	}

private:
	StateController<ScreenId> screenController_;
	StateRegistry<ScreenId> stateRegistry_;
	ContentRegistry content_;
	PackageRegistry packages_;
	PackageLifecycle packageLifecycle_;
	FrameDriver frameDriver_;
	PersistenceService persistence_;
	RuntimeCapabilities hostCapabilities_;
	EngineLifecycle lifecycle_ = EngineLifecycle::Stopped;
};

#endif
