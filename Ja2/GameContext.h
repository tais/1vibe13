#ifndef JA2_GAME_CONTEXT_H
#define JA2_GAME_CONTEXT_H

#include "CampaignSimulationHost.h"
#include "GameSettings.h"
#include "GameCapabilities.h"
#include <string>
#include <utility>
#include <Engine/Adapters/JA2/EngineRuntime.h>

// Compatibility name retained while callers migrate to EngineRuntime.
using GameLifecycle = EngineLifecycle;

// Incremental composition root for engine-wide services. References initially
// point at the legacy globals so systems can migrate without changing save data
// layout, ABI, or initialization order.
class GameContext
{
public:
	GameContext(GAME_SETTINGS& settings, GAME_OPTIONS& options, GameCapabilities capabilities = {},
	            EngineServices services = EngineServices::defaults(),
	            PackageEventSink& packageEvents = NullPackageEventSink::instance())
		: settings_(settings), options_(options), capabilities_(capabilities),
		  runtime_(makeRuntimeOptions(capabilities), services, packageEvents),
		  campaignSimulation_(runtime_.campaignClockScheduler())
	{
	}

	~GameContext()
	{
		if (campaignSimulationRegistered_)
			(void)runtime_.simulationTicks().removeSink(campaignSimulation_);
	}

	GAME_SETTINGS& settings() { return settings_; }
	const GAME_SETTINGS& settings() const { return settings_; }
	GAME_OPTIONS& options() { return options_; }
	const GAME_OPTIONS& options() const { return options_; }
	const GameCapabilities& capabilities() const { return capabilities_; }
	EngineRuntime<UINT32>& runtime() { return runtime_; }
	const EngineRuntime<UINT32>& runtime() const { return runtime_; }
	EngineServices& services() { return runtime_.services(); }
	const EngineServices& services() const { return runtime_.services(); }
	LogSink& log() { return runtime_.log(); }
	StateStack<UINT32>& screens() { return runtime_.screens(); }
	const StateStack<UINT32>& screens() const { return runtime_.screens(); }
	StateController<UINT32>& screenController() { return runtime_.screenController(); }
	const StateController<UINT32>& screenController() const { return runtime_.screenController(); }
	StateRegistry<UINT32>& stateRegistry() { return runtime_.stateRegistry(); }
	const StateRegistry<UINT32>& stateRegistry() const { return runtime_.stateRegistry(); }
	FrameDriver& frameDriver() { return runtime_.frameDriver(); }
	const FrameDriver& frameDriver() const { return runtime_.frameDriver(); }
	InputDispatcher& inputDispatcher() { return runtime_.inputDispatcher(); }
	const InputDispatcher& inputDispatcher() const { return runtime_.inputDispatcher(); }
	RuntimeUpdateDispatcher& runtimeUpdates() { return runtime_.runtimeUpdates(); }
	const RuntimeUpdateDispatcher& runtimeUpdates() const { return runtime_.runtimeUpdates(); }
	CampaignSimulationHost& campaignSimulation() { return campaignSimulation_; }
	const CampaignSimulationHost& campaignSimulation() const
	{
		return campaignSimulation_;
	}
	SimulationTickSinkRegistrationError enableCampaignSimulation()
	{
		if (campaignSimulationRegistered_)
			return SimulationTickSinkRegistrationError::None;
		const SimulationTickSinkRegistrationError result =
			runtime_.simulationTicks().addSinkBefore(
				campaignSimulation_, runtime_.packages());
		if (result == SimulationTickSinkRegistrationError::None)
			campaignSimulationRegistered_ = true;
		return result;
	}
	bool campaignSimulationEnabled() const
	{
		return campaignSimulationRegistered_;
	}
	FrameTelemetry& frameTelemetry() { return runtime_.frameTelemetry(); }
	const FrameTelemetry& frameTelemetry() const { return runtime_.frameTelemetry(); }
	RuntimeMessageBus& runtimeMessages() { return runtime_.runtimeMessages(); }
	const RuntimeMessageBus& runtimeMessages() const { return runtime_.runtimeMessages(); }
	ServiceCatalog& serviceCatalog() { return runtime_.serviceCatalog(); }
	const ServiceCatalog& serviceCatalog() const { return runtime_.serviceCatalog(); }
	RuntimeConfiguration& configuration() { return runtime_.configuration(); }
	const RuntimeConfiguration& configuration() const { return runtime_.configuration(); }
	ContentRegistry& content() { return runtime_.content(); }
	const ContentRegistry& content() const { return runtime_.content(); }
	PackageRegistry& packages() { return runtime_.packages(); }
	const PackageRegistry& packages() const { return runtime_.packages(); }
	PackageLifecycle& packageLifecycle() { return runtime_.packageLifecycle(); }
	const PackageLifecycle& packageLifecycle() const { return runtime_.packageLifecycle(); }
	RuntimeSession& runtimeSession() { return runtime_.runtimeSession(); }
	const RuntimeSession& runtimeSession() const { return runtime_.runtimeSession(); }
	RuntimeReport runtimeReport() const { return runtime_.runtimeReport(); }
	RuntimeReportSaveError saveRuntimeReport(const std::string& path) const noexcept
	{
		return runtime_.saveRuntimeReport(path);
	}
	RuntimeSessionAdvanceResult advancePackagesTo(PackageBootstrapPhase phase)
	{
		return runtime_.runtimeSession().advancePackagesTo(phase);
	}
	RuntimeSessionShutdownResult shutdownPackages()
	{
		return runtime_.runtimeSession().shutdownPackages();
	}
	PackageCatalogSnapshot packageCatalog() const { return runtime_.packageCatalog(); }
	PackageSaveStateCaptureResult capturePackageSaveState() noexcept
	{
		return runtime_.capturePackageSaveState();
	}
	PackageSaveStateLoadResult validatePackageSaveState(
		const PackageSaveStateSnapshot& snapshot) const noexcept
	{
		return runtime_.validatePackageSaveState(snapshot);
	}
	PackageSaveStateLoadResult restorePackageSaveState(
		const PackageSaveStateSnapshot& snapshot) noexcept
	{
		return runtime_.restorePackageSaveState(snapshot);
	}
	PackageSaveArchiveService& packageSaveArchives()
	{
		return runtime_.packageSaveArchives();
	}
	const PackageSaveArchiveService& packageSaveArchives() const
	{
		return runtime_.packageSaveArchives();
	}
	RuntimeSaveContainerService& runtimeSaveContainers()
	{
		return runtime_.runtimeSaveContainers();
	}
	const RuntimeSaveContainerService& runtimeSaveContainers() const
	{
		return runtime_.runtimeSaveContainers();
	}
	bool hasCapability(const std::string& capability) const
	{
		return runtime_.hasCapability(capability);
	}
	RuntimeCapabilities runtimeCapabilities() const
	{
		return runtime_.runtimeCapabilities();
	}
	PersistenceService& persistence() { return runtime_.persistence(); }
	const PersistenceService& persistence() const { return runtime_.persistence(); }
	DeterministicCommandQueue<SimulationCommand>& commands() { return runtime_.commands(); }
	const DeterministicCommandQueue<SimulationCommand>& commands() const { return runtime_.commands(); }
	CommandJournal<SimulationCommand>& commandJournal() { return runtime_.commandJournal(); }
	const CommandJournal<SimulationCommand>& commandJournal() const { return runtime_.commandJournal(); }
	CommandReplayService& commandReplay() { return runtime_.commandReplay(); }
	const CommandReplayService& commandReplay() const { return runtime_.commandReplay(); }
	CommandReplaySaveResult saveCommandReplay(const std::string& path) const noexcept
	{
		return runtime_.saveCommandReplay(path);
	}
	CommandReplayLoadResult loadCommandReplay(
		const std::string& path, SimulationCommandReplay& replay) const noexcept
	{
		return runtime_.loadCommandReplay(path, replay);
	}
	CommandReplayStageResult stageCommandReplay(const SimulationCommandReplay& replay)
	{
		return runtime_.stageCommandReplay(replay);
	}
	std::uint64_t submitCommand(std::uint64_t tick, SimulationCommand command)
	{
		return runtime_.submitCommand(tick, std::move(command));
	}
	bool submitRecordedCommand(
		std::uint64_t tick, std::uint64_t sequence, SimulationCommand command)
	{
		return runtime_.submitRecordedCommand(tick, sequence, std::move(command));
	}
	bool setCapabilities(GameCapabilities capabilities)
	{
		if (runtime_.lifecycle() != EngineLifecycle::Stopped) return false;
		if (!runtime_.setHostCapabilities(makeHostCapabilities(capabilities))) return false;
		capabilities_ = capabilities;
		return true;
	}

	GameLifecycle lifecycle() const { return runtime_.lifecycle(); }
	RuntimeSessionTransitionResult tryBeginInitialization()
	{
		return runtime_.tryBeginInitialization();
	}
	RuntimeSessionTransitionResult tryCancelInitialization()
	{
		return runtime_.tryCancelInitialization();
	}
	RuntimeSessionTransitionResult tryMarkRunning()
	{
		return runtime_.tryMarkRunning();
	}
	RuntimeSessionTransitionResult tryBeginShutdown()
	{
		return runtime_.tryBeginShutdown();
	}
	RuntimeSessionTransitionResult tryMarkStopped()
	{
		return runtime_.tryMarkStopped();
	}
	bool beginInitialization() { return runtime_.beginInitialization(); }
	bool cancelInitialization() { return runtime_.cancelInitialization(); }
	bool markRunning() { return runtime_.markRunning(); }
	bool beginShutdown() { return runtime_.beginShutdown(); }
	bool markStopped() { return runtime_.markStopped(); }

private:
	static RuntimeCapabilities makeHostCapabilities(GameCapabilities capabilities)
	{
		RuntimeCapabilities result;
		result.add(capabilities.isUnfinishedBusiness()
			? GameCapability::HostCampaignUnfinishedBusiness
			: GameCapability::HostCampaignArulco);
		if (capabilities.isEditor()) result.add(GameCapability::ApplicationMapEditor);
		return result;
	}
	static EngineHostOptions makeRuntimeOptions(GameCapabilities capabilities)
	{
		EngineHostOptions options;
		options.hostCapabilities = makeHostCapabilities(capabilities);
		return options;
	}

	GAME_SETTINGS& settings_;
	GAME_OPTIONS& options_;
	GameCapabilities capabilities_;
	EngineRuntime<UINT32> runtime_;
	CampaignSimulationHost campaignSimulation_;
	bool campaignSimulationRegistered_ = false;
};

class GameInitializationGuard
{
public:
	explicit GameInitializationGuard(GameContext& context)
		: context_(context), begin_(context_.tryBeginInitialization()),
		  active_(static_cast<bool>(begin_))
	{
	}
	GameInitializationGuard(const GameInitializationGuard&) = delete;
	GameInitializationGuard& operator=(const GameInitializationGuard&) = delete;
	GameInitializationGuard(GameInitializationGuard&&) = delete;
	GameInitializationGuard& operator=(GameInitializationGuard&&) = delete;

	~GameInitializationGuard() noexcept
	{
		if (!active_) return;
		try
		{
			const RuntimeSessionTransitionResult result = cancel();
			if (result) return;
			context_.log().write(LogRecord{
				LogSeverity::Error, "lifecycle",
				"Initialization rollback failed with code " +
					std::to_string(static_cast<int>(result.error))});
		}
		catch (...)
		{
			try
			{
				context_.log().write(LogRecord{
					LogSeverity::Error, "lifecycle",
					"Initialization rollback raised an unexpected exception"});
			}
			catch (...) {}
		}
	}

	explicit operator bool() const { return active_; }
	const RuntimeSessionTransitionResult& beginResult() const { return begin_; }
	RuntimeSessionTransitionResult cancel()
	{
		if (!active_) return inactiveResult();
		RuntimeSessionTransitionResult result = context_.tryCancelInitialization();
		// Reentrant cancellation can observe the package registry's active
		// bootstrap transaction. Retain ownership while the session is still
		// initializing so an explicit retry or this guard's destructor can
		// perform the rollback after that transaction returns.
		if (result.lifecycle != EngineLifecycle::Initializing) active_ = false;
		return result;
	}
	RuntimeSessionTransitionResult tryMarkRunning()
	{
		if (!active_) return inactiveResult();
		RuntimeSessionTransitionResult result = context_.tryMarkRunning();
		if (result) active_ = false;
		return result;
	}
	bool markRunning()
	{
		return static_cast<bool>(tryMarkRunning());
	}

private:
	RuntimeSessionTransitionResult inactiveResult() const
	{
		return RuntimeSessionTransitionResult{
			RuntimeSessionError::InvalidState, context_.lifecycle(),
			context_.packages().completedBootstrapPhases(), {}};
	}

	GameContext& context_;
	RuntimeSessionTransitionResult begin_;
	bool active_;
};

GameContext& GetGameContext();

#endif
