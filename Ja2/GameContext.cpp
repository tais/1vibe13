#include "GameContext.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignPackage.h"
#include "PackageHost.h"
#include "RulesPackage.h"
#include "Screens.h"
#include "TacticalCommandHost.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "TacticalWorldObserverHost.h"
#include <Engine/Adapters/JA2/CampaignClockService.h>
#include <Engine/Adapters/Legacy/PlatformAssets.h>
#include <Engine/Adapters/Legacy/PlatformLog.h>
#include <Engine/Adapters/Legacy/PlatformInput.h>
#include <Engine/Adapters/Legacy/PlatformAudio.h>
#include <Engine/Adapters/Legacy/PlatformFileSystem.h>
#include <Engine/Adapters/Legacy/LegacyFrameInvalidationGateway.h>
#include <Engine/Adapters/Legacy/LegacyFrameGateway.h>
#include <Engine/Adapters/Legacy/LegacyRenderCommandGateway.h>
#include <Engine/Adapters/Legacy/PlatformFrameInvalidator.h>
#include <Engine/Adapters/Legacy/PlatformFramePresenter.h>
#include <Engine/Adapters/Legacy/LegacyRenderSurfaceGateway.h>
#include <Engine/Adapters/Legacy/PlatformRenderCommands.h>
#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>
#include <Engine/Adapters/Legacy/PlatformTime.h>
#include "random.h"

GameContext& GetGameContext()
{
	// Construct the application-owned package graph before the registry and
	// external host that keep non-owning references into it.
	(void)GetCompiledRulesPackage();
	(void)GetCompiledCampaignPackage();
	(void)GetStartupPackageHost();
	// The command host receives lifecycle events from this context, so construct
	// its application owner first and destroy it after the runtime registry.
	PackageEventSink& packageEvents = GetJa2TacticalCommandPackageEventSink();
	static GameContext context(
		gGameSettings,
		gGameOptions,
		GetCompiledGameCapabilities(),
		EngineServices{GetPlatformTimeSource(), GetGameRandomSource(),
		               GetPlatformByteStorage(), GetPlatformLogSink(),
		               GetPlatformInputSource(), GetPlatformAudioOutput(),
		               GetPlatformFramePresenter(), GetPlatformAssetSource(),
		               GetPlatformFrameInvalidator(),
		               GetPlatformRenderSurfaceAccess(),
		               GetPlatformRenderCommands()},
		packageEvents);
	static const bool legacyFrameGatewayBound = [&] {
		BindLegacyFramePresenter(context.services().frames);
		return true;
	}();
	(void)legacyFrameGatewayBound;
	static const bool legacyFrameInvalidatorBound = [&] {
		BindLegacyFrameInvalidator(context.services().frameInvalidation);
		return true;
	}();
	(void)legacyFrameInvalidatorBound;
	static const bool legacyRenderSurfaceAccessBound = [&] {
		BindLegacyRenderSurfaceAccess(context.services().renderSurfaces);
		return true;
	}();
	(void)legacyRenderSurfaceAccessBound;
	static const bool legacyRenderCommandsBound = [&] {
		BindLegacyRenderCommands(context.services().renderCommands);
		return true;
	}();
	(void)legacyRenderCommandsBound;
	static const bool tacticalCommandHostBound = BindJa2TacticalCommandHost(context);
	(void)tacticalCommandHostBound;
	static const bool tacticalEntityDirectoryBound = [&] {
		BindJa2TacticalEntityDirectory(context.runtime().tacticalEntityDirectory());
		return true;
	}();
	(void)tacticalEntityDirectoryBound;
	static const bool campaignClockSessionBound = [&] {
		BindJa2CampaignClockSession(context.runtime().campaignClockSession());
		return true;
	}();
	(void)campaignClockSessionBound;
	static const SimulationTickSinkRegistrationError campaignSimulationRegistered =
		context.enableCampaignSimulation();
	static const bool campaignSimulationRegistrationReported = [&] {
		if (campaignSimulationRegistered !=
			SimulationTickSinkRegistrationError::None)
			context.log().write(LogRecord{
				LogSeverity::Error, "simulation",
				"Campaign fixed-step registration failed"});
		return true;
	}();
	(void)campaignSimulationRegistrationReported;
	static const EngineServiceRegistrationError campaignClockRegistered =
		RegisterCampaignClockService(
			context.serviceCatalog(), context.runtime().campaignClockService());
	static const bool campaignClockRegistrationReported = [&] {
		if (campaignClockRegistered != EngineServiceRegistrationError::None)
			context.log().write(LogRecord{
				LogSeverity::Error, "services",
				"Campaign clock service registration failed"});
		return true;
	}();
	(void)campaignClockRegistrationReported;
	static const EngineServiceRegistrationError campaignEventsRegistered =
		[&] {
			BindJa2CampaignEventQueue(context.runtime().campaignEventQueue());
			return RegisterCampaignEventService(
				context.serviceCatalog(), GetJa2CampaignEventAdapter());
		}();
	static const bool campaignEventsRegistrationReported = [&] {
		if (campaignEventsRegistered != EngineServiceRegistrationError::None)
			context.log().write(LogRecord{
				LogSeverity::Error, "services",
				"Campaign event service registration failed"});
		return true;
	}();
	(void)campaignEventsRegistrationReported;
	static const EngineServiceRegistrationError tacticalWorldRegistered =
		[&] {
			BindJa2TacticalWorldSession(context.runtime().tacticalWorldSession());
			return RegisterTacticalWorldService(
				context.serviceCatalog(), GetJa2TacticalWorldAdapter());
		}();
	static const bool tacticalWorldRegistrationReported = [&] {
		if (tacticalWorldRegistered != EngineServiceRegistrationError::None)
			context.log().write(LogRecord{
				LogSeverity::Error, "services", "Tactical world service registration failed"});
		return true;
	}();
	(void)tacticalWorldRegistrationReported;
	static const EngineServiceRegistrationError tacticalWorldObserverRegistered =
		RegisterTacticalWorldObserverService(
			context.serviceCatalog(), GetJa2TacticalWorldObserverService());
	static const bool tacticalWorldObserverRegistrationReported = [&] {
		if (tacticalWorldObserverRegistered != EngineServiceRegistrationError::None)
			context.log().write(LogRecord{
				LogSeverity::Error, "services",
				"Tactical world observer service registration failed"});
		return true;
	}();
	(void)tacticalWorldObserverRegistrationReported;
	static const EngineServiceRegistrationError tacticalCommandsRegistered =
		RegisterTacticalCommandService(
			context.serviceCatalog(), GetJa2TacticalCommandService());
	static const bool tacticalCommandsRegistrationReported = [&] {
		if (!tacticalCommandHostBound)
			context.log().write(LogRecord{
				LogSeverity::Error, "services",
				"Tactical command host binding failed"});
		if (tacticalCommandsRegistered != EngineServiceRegistrationError::None)
			context.log().write(LogRecord{
				LogSeverity::Error, "services",
				"Tactical command service registration failed"});
		return true;
	}();
	(void)tacticalCommandsRegistrationReported;
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
	static const bool packagesRegistered = [] {
		LegacyRulesPackage& rules = GetCompiledRulesPackage();
		LegacyCampaignPackage& package = GetCompiledCampaignPackage();
		GameContext& game = context;
		const PackageRegistrationError rulesError =
			game.packages().registerPackage(rules);
		if (rulesError != PackageRegistrationError::None)
		{
			game.log().write(LogRecord{LogSeverity::Error, "packages",
				"Compiled 1.13 rules registration failed with code " +
					std::to_string(static_cast<int>(rulesError))});
			return false;
		}
		const PackageRegistrationError campaignError =
			game.packages().registerPackage(package);
		if (campaignError != PackageRegistrationError::None)
		{
			game.log().write(LogRecord{LogSeverity::Error, "packages",
				"Compiled campaign registration failed with code " +
					std::to_string(static_cast<int>(campaignError))});
			return false;
		}
		return true;
	}();
	(void)packagesRegistered;
	return context;
}
