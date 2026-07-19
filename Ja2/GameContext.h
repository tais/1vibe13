#ifndef JA2_GAME_CONTEXT_H
#define JA2_GAME_CONTEXT_H

#include "GameSettings.h"

enum class GameLifecycle
{
	Stopped,
	Initializing,
	Running,
	ShuttingDown
};

// Incremental composition root for engine-wide services. References initially
// point at the legacy globals so systems can migrate without changing save data
// layout, ABI, or initialization order.
class GameContext
{
public:
	GameContext(GAME_SETTINGS& settings, GAME_OPTIONS& options)
		: settings_(settings), options_(options)
	{
	}

	GAME_SETTINGS& settings() { return settings_; }
	const GAME_SETTINGS& settings() const { return settings_; }
	GAME_OPTIONS& options() { return options_; }
	const GAME_OPTIONS& options() const { return options_; }

	GameLifecycle lifecycle() const { return lifecycle_; }
	bool beginInitialization();
	bool cancelInitialization();
	bool markRunning();
	bool beginShutdown();
	bool markStopped();

private:
	GAME_SETTINGS& settings_;
	GAME_OPTIONS& options_;
	GameLifecycle lifecycle_ = GameLifecycle::Stopped;
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
