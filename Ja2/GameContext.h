#ifndef JA2_GAME_CONTEXT_H
#define JA2_GAME_CONTEXT_H

#include "GameSettings.h"
#include "GameCapabilities.h"
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
		  runtime_(services, CurrentContentApiVersion, packageEvents,
		           makeHostCapabilities(capabilities))
	{
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
	ContentRegistry& content() { return runtime_.content(); }
	const ContentRegistry& content() const { return runtime_.content(); }
	PackageRegistry& packages() { return runtime_.packages(); }
	const PackageRegistry& packages() const { return runtime_.packages(); }
	PackageCatalogSnapshot packageCatalog() const { return runtime_.packageCatalog(); }
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
	bool beginInitialization() { return runtime_.beginInitialization(); }
	bool cancelInitialization() { return runtime_.cancelInitialization(); }
	bool markRunning() { return runtime_.markRunning(); }
	bool beginShutdown() { return runtime_.beginShutdown(); }
	bool markStopped() { return runtime_.markStopped(); }

private:
	static RuntimeCapabilities makeHostCapabilities(GameCapabilities capabilities)
	{
		RuntimeCapabilities result;
		if (capabilities.isEditor()) result.add(GameCapability::ApplicationMapEditor);
		return result;
	}

	GAME_SETTINGS& settings_;
	GAME_OPTIONS& options_;
	GameCapabilities capabilities_;
	EngineRuntime<UINT32> runtime_;
};

class GameInitializationGuard
{
public:
	explicit GameInitializationGuard(GameContext& context)
		: context_(context), active_(context_.beginInitialization())
	{
	}

	~GameInitializationGuard()
	{
		if (active_) context_.cancelInitialization();
	}

	explicit operator bool() const { return active_; }
	bool markRunning()
	{
		if (!active_ || !context_.markRunning()) return false;
		active_ = false;
		return true;
	}

private:
	GameContext& context_;
	bool active_;
};

GameContext& GetGameContext();

#endif
