#ifndef JA2_GAME_CONTEXT_H
#define JA2_GAME_CONTEXT_H

#include "GameSettings.h"
#include "GameCapabilities.h"
#include <Engine/Core/EngineRuntime.h>

// Compatibility name retained while callers migrate to EngineRuntime.
using GameLifecycle = EngineLifecycle;

// Incremental composition root for engine-wide services. References initially
// point at the legacy globals so systems can migrate without changing save data
// layout, ABI, or initialization order.
class GameContext
{
public:
	GameContext(GAME_SETTINGS& settings, GAME_OPTIONS& options, GameCapabilities capabilities = {},
	            EngineServices services = EngineServices::defaults())
		: settings_(settings), options_(options), capabilities_(capabilities), runtime_(services)
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
	ContentRegistry& content() { return runtime_.content(); }
	const ContentRegistry& content() const { return runtime_.content(); }
	PackageRegistry& packages() { return runtime_.packages(); }
	const PackageRegistry& packages() const { return runtime_.packages(); }
	PackageCatalogSnapshot packageCatalog() const { return runtime_.packageCatalog(); }
	DeterministicCommandQueue<SimulationCommand>& commands() { return runtime_.commands(); }
	const DeterministicCommandQueue<SimulationCommand>& commands() const { return runtime_.commands(); }
	bool setCapabilities(GameCapabilities capabilities)
	{
		if (runtime_.lifecycle() != EngineLifecycle::Stopped) return false;
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
