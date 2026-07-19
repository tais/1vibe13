#include "GameContext.h"

bool GameContext::beginInitialization()
{
	if (lifecycle_ != GameLifecycle::Stopped) return false;
	lifecycle_ = GameLifecycle::Initializing;
	return true;
}

bool GameContext::markRunning()
{
	if (lifecycle_ != GameLifecycle::Initializing) return false;
	lifecycle_ = GameLifecycle::Running;
	return true;
}

bool GameContext::cancelInitialization()
{
	if (lifecycle_ != GameLifecycle::Initializing) return false;
	lifecycle_ = GameLifecycle::Stopped;
	return true;
}

bool GameContext::beginShutdown()
{
	if (lifecycle_ != GameLifecycle::Running && lifecycle_ != GameLifecycle::Initializing) return false;
	lifecycle_ = GameLifecycle::ShuttingDown;
	return true;
}

bool GameContext::markStopped()
{
	if (lifecycle_ != GameLifecycle::ShuttingDown) return false;
	lifecycle_ = GameLifecycle::Stopped;
	return true;
}

GameContext& GetGameContext()
{
	static GameContext context(gGameSettings, gGameOptions);
	return context;
}
