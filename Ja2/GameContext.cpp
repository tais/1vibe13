#include "GameContext.h"
#include "CampaignPackage.h"
#include "PlatformLog.h"
#include "PlatformInput.h"
#include "PlatformAudio.h"
#include "PlatformFileSystem.h"
#include "PlatformFramePresenter.h"
#include "PlatformTime.h"
#include "random.h"

GameContext& GetGameContext()
{
	static GameContext context(
		gGameSettings,
		gGameOptions,
		GetCompiledGameCapabilities(),
		EngineServices{GetPlatformTimeSource(), GetGameRandomSource(),
		               GetPlatformByteStorage(), GetPlatformLogSink(),
		               GetPlatformInputSource(), GetPlatformAudioOutput(),
		               GetPlatformFramePresenter()});
	static const bool packageActivated = [] {
		LegacyCampaignPackage& package = GetCompiledCampaignPackage();
		GameContext& game = context;
		return game.packages().registerPackage(package) == PackageRegistrationError::None &&
			game.packages().activate(package.descriptor().content.id) == PackageActivationError::None;
	}();
	(void)packageActivated;
	return context;
}
