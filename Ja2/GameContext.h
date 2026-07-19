#ifndef JA2_GAME_CONTEXT_H
#define JA2_GAME_CONTEXT_H

#include "GameSettings.h"
#include "GameCapabilities.h"
#include "../Engine/Core/StateStack.h"
#include "../Engine/Core/ContentApi.h"
#include "../Engine/Core/PackageApi.h"
#include "../Engine/Core/LogSink.h"

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
	GameContext(GAME_SETTINGS& settings, GAME_OPTIONS& options, GameCapabilities capabilities = {},
	            LogSink& log = NullLogSink::instance())
		: settings_(settings), options_(options), capabilities_(capabilities), log_(log), packages_(content_, log_)
	{
	}

	GAME_SETTINGS& settings() { return settings_; }
	const GAME_SETTINGS& settings() const { return settings_; }
	GAME_OPTIONS& options() { return options_; }
	const GAME_OPTIONS& options() const { return options_; }
	const GameCapabilities& capabilities() const { return capabilities_; }
	LogSink& log() { return log_; }
	StateStack<UINT32>& screens() { return screens_; }
	const StateStack<UINT32>& screens() const { return screens_; }
	ContentRegistry& content() { return content_; }
	const ContentRegistry& content() const { return content_; }
	PackageRegistry& packages() { return packages_; }
	const PackageRegistry& packages() const { return packages_; }
	bool setCapabilities(GameCapabilities capabilities)
	{
		if (lifecycle_ != GameLifecycle::Stopped) return false;
		capabilities_ = capabilities;
		return true;
	}

	GameLifecycle lifecycle() const { return lifecycle_; }
	bool beginInitialization();
	bool cancelInitialization();
	bool markRunning();
	bool beginShutdown();
	bool markStopped();

private:
	GAME_SETTINGS& settings_;
	GAME_OPTIONS& options_;
	GameCapabilities capabilities_;
	LogSink& log_;
	StateStack<UINT32> screens_;
	ContentRegistry content_{ContentApiVersion{1, 0}};
	PackageRegistry packages_;
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
