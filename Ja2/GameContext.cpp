#include "GameContext.h"
#include "CampaignPackage.h"
#include "PackageHost.h"
#include "Screens.h"
#include <Engine/Adapters/Legacy/PlatformAssets.h>
#include <Engine/Adapters/Legacy/PlatformLog.h>
#include <Engine/Adapters/Legacy/PlatformInput.h>
#include <Engine/Adapters/Legacy/PlatformAudio.h>
#include <Engine/Adapters/Legacy/PlatformFileSystem.h>
#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>
#include <Engine/Adapters/Legacy/PlatformTime.h>
#include "random.h"

GameContext& GetGameContext()
{
	// External package objects are non-owningly referenced by GameContext's
	// registry. Construct their application owner first so it is destroyed last.
	(void)GetStartupPackageHost();
	// Construct the application-owned package first so it also outlives the
	// registry's non-owning package and asset references during static teardown.
	(void)GetCompiledCampaignPackage();
	static GameContext context(
		gGameSettings,
		gGameOptions,
		GetCompiledGameCapabilities(),
		EngineServices{GetPlatformTimeSource(), GetGameRandomSource(),
		               GetPlatformByteStorage(), GetPlatformLogSink(),
		               GetPlatformInputSource(), GetPlatformAudioOutput(),
		               GetPlatformFramePresenter(), GetPlatformAssetSource()});
	static const bool screensRegistered = [] {
		for (UINT32 screenId = 0; screenId < MAX_SCREENS; ++screenId)
		{
			Screens* screen = &GameScreens[screenId];
			if (!screen->InitializeScreen || !screen->HandleScreen || !screen->ShutdownScreen)
				return false;
			if (context.stateRegistry().registerState(screenId, StateCallbacks<UINT32>{
				[screen] { return screen->InitializeScreen() != FALSE; },
				[screen] { return screen->HandleScreen(); },
				[screen] { (void)screen->ShutdownScreen(); }}) !=
				StateRegistrationError::None) return false;
		}
		return true;
	}();
	static const bool screenRegistrationReported = [&] {
		if (!screensRegistered)
			context.log().write(LogRecord{
				LogSeverity::Error, "states", "Legacy screen registration failed"});
		return true;
	}();
	(void)screenRegistrationReported;
	static const bool packageActivated = [] {
		LegacyCampaignPackage& package = GetCompiledCampaignPackage();
		GameContext& game = context;
		return game.packages().registerPackage(package) == PackageRegistrationError::None &&
			game.packages().activate(package.descriptor().content.id) == PackageActivationError::None;
	}();
	(void)packageActivated;
	return context;
}
