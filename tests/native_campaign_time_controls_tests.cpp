// Real native clock + fixed-step scheduler, with a data-free established
// strategic roster. No fake native clock, installed campaign or GUI automation.
#include "GameContext.h"
#include "Animation Data.h"
#include "Game Clock.h"
#include "Map Screen Interface.h"
#include "Map Screen Interface Bottom.h"
#include "Map Screen Interface Map.h"
#include "Overhead.h"
#include "SoldierRepository.h"
#include "Soldier Profile Constants.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "CampaignClockAdapter.h"
#include "gameloop.h"
#include "screenids.h"
#include "connect.h"
#include <Engine/Adapters/JA2/CampaignClockScheduler.h>
#include <cstdio>
#include <cstdlib>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1);
}
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
}
int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native fixture starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	is_networked = is_client = is_server = false;
	gbPlayerNum = OUR_TEAM;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0}; gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	auto& actor = *repository.resolve(0);
	actor.identity().id() = SoldierID{0}; actor.identity().incarnation() = 101;
	actor.identity().profile() = NO_PROFILE; actor.identity().bodyType() = REGMALE;
	actor.roster().active() = TRUE; actor.roster().inSector() = FALSE; actor.roster().team() = OUR_TEAM;
	actor.assignment().current() = 0; actor.vitals().health() = actor.vitals().maximumHealth() = 80;
	CHECK(AdoptJa2TacticalEntity(actor), "established strategic actor adopted");
	gTacticalStatus.fDidGameJustStart = FALSE;
	gTacticalStatus.fEnemyInSector = FALSE; gfAtLeastOneMercWasHired = TRUE;
	SetSelectedDestChar(-1);
	RestoreJa2TacticalTurnState(0, OUR_TEAM, 0);
	RestoreJa2CampaignClock(111600, 111600);
	[[maybe_unused]] auto screen = OverrideCurrentScreen(MAP_SCREEN);
	UnLockPauseState(); PauseGame();
	CHECK(!IsJa2TacticalWorldLoaded() && AllowedToTimeCompress(), "real native guard permits this worldless established campaign");
	if (failures) return 1;
	const auto initial = GetWorldTotalSeconds();
	for (const UINT32 mode : {TIME_COMPRESS_5MINS, TIME_COMPRESS_30MINS, TIME_COMPRESS_60MINS})
	{
		ResetTimeCompressHasOccured(); gfTimeInterrupt = TRUE;
		CHECK(TrySetWorldlessStrategicTimeCompression(mode) && !GamePaused() && IsTimeBeingCompressed() &&
			giTimeCompressMode == static_cast<INT32>(mode) && HasTimeCompressOccured() && gfTimeInterrupt &&
			GetWorldTotalSeconds() == initial && !IsJa2TacticalWorldLoaded(), "all speeds preserve first-compression cleanup, do not warp time, clear interrupt or load a world");
		CHECK(TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_X0) && GamePaused() && !IsTimeCompressionOn() &&
			giTimeCompressMode == TIME_COMPRESS_X0, "absolute pause stops compression and game clock");
	}
	// A lock, legacy multiplayer, startup, battle, or unusable roster must deny
	// without opening the native GUI error path or changing the pause/rate.
	for (unsigned fault = 0; fault < 5; ++fault)
	{
		if (fault == 0) LockPauseState(4242);
		if (fault == 1) is_networked = true;
		if (fault == 2) gTacticalStatus.fDidGameJustStart = TRUE;
		if (fault == 3) gTacticalStatus.fEnemyInSector = TRUE;
		if (fault == 4) actor.vitals().health() = 0;
		CHECK(!TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_5MINS) && GamePaused() &&
			!IsTimeCompressionOn() && giTimeCompressMode == TIME_COMPRESS_X0 && GetWorldTotalSeconds() == initial,
			"native blocking condition is transactional, no GUI fallback");
		CHECK(TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_X0), "pause is allowed even with a native lock or obstacle");
		UnLockPauseState(); is_networked = false; gTacticalStatus.fDidGameJustStart = FALSE;
		gTacticalStatus.fEnemyInSector = FALSE; actor.vitals().health() = 80;
	}
	for (const UINT32 mode : {1u, 5u, 6u, UINT32_MAX})
		CHECK(!TrySetWorldlessStrategicTimeCompression(mode) && GamePaused(), "1x, super and arbitrary rates rejected");
	{
		[[maybe_unused]] auto tactical = OverrideCurrentScreen(GAME_SCREEN);
		CHECK(!TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_X0) && !TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_5MINS),
			"no tactical pause or compression entry");
	}
	NotifyJa2TacticalWorldLoaded(1);
	CHECK(!TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_X0) && !TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_5MINS),
		"loaded tactical world blocks even while map screen is selected");
	NotifyJa2TacticalWorldUnloaded();
	gfTimeInterrupt = FALSE;
	CHECK(TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_5MINS), "normal resume accepted");
	CampaignClockScheduler scheduler;
	for (unsigned i = 0; i < 100; ++i) (void)AdvanceClockFromFixedStep(scheduler, 10000);
	CHECK(GetWorldTotalSeconds() == initial + 300, "real fixed-step scheduler advances requested five minutes, not the command handler");
	for (const UINT32 mode : {TIME_COMPRESS_30MINS, TIME_COMPRESS_60MINS})
	{
		const auto before = GetWorldTotalSeconds();
		CHECK(TrySetWorldlessStrategicTimeCompression(mode), "higher speed accepted by the same native guard");
		for (unsigned i = 0; i < 100; ++i) (void)AdvanceClockFromFixedStep(scheduler, 10000);
		CHECK(GetWorldTotalSeconds() == before + (mode == TIME_COMPRESS_30MINS ? 1800 : 3600),
			"native fixed-step scheduler advances exact thirty/sixty-minute rates");
	}
	PauseGame(); // Native event pause must remain in force without another command.
	const auto paused = GetWorldTotalSeconds();
	for (unsigned i = 0; i < 100; ++i) (void)AdvanceClockFromFixedStep(scheduler, 10000);
	CHECK(GetWorldTotalSeconds() == paused && GamePaused(), "native pause is not automatically resumed on subsequent ticks");
	CHECK(TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_X0), "offline safety pause uses the same native path");
	for (unsigned i = 0; i < 100; ++i) (void)AdvanceClockFromFixedStep(scheduler, 10000);
	CHECK(GetWorldTotalSeconds() == paused && !IsTimeCompressionOn(), "disconnected clock has no accumulated catch-up time");
	return failures ? 1 : 0;
}
