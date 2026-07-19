#include "GameContext.h"
#include "CampaignPackage.h"
#include "PlatformLog.h"
#include "PlatformFileSystem.h"
#include "PlatformTime.h"
#include "random.h"

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
	static GameContext context(
		gGameSettings,
		gGameOptions,
		GetCompiledGameCapabilities(),
		EngineServices{GetPlatformTimeSource(), GetGameRandomSource(),
		               GetPlatformByteStorage(), GetPlatformLogSink()});
	static const bool packageActivated = [] {
		LegacyCampaignPackage& package = GetCompiledCampaignPackage();
		GameContext& game = context;
		return game.packages().registerPackage(package) == PackageRegistrationError::None &&
			game.packages().activate(package.descriptor().content.id) == PackageActivationError::None;
	}();
	(void)packageActivated;
	return context;
}
