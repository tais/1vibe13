// Actual stationary transitions must not dereference the consumed route sentinel.
#include "Animation Control.h"
#include "Animation Data.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "MemMan.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Points.h"
#include "renderworld.h"
#include "Soldier Profile Constants.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorStateFlags.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "World Tile Map.h"
#include "connect.h"
#include "random.h"
#include "sgp.h"
#include "worlddef.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>

extern UINT16 gubAnimSurfaceIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
extern UINT16 gubAnimSurfaceItemSubIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1); }

int main()
{
    int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (false)
    CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "native RNG installed");
    auto& game = GetGameContext();
    CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
        "native runtime starts");
    auto& repository = GetJa2SoldierRepository();
    repository.initializeSlots(); ResetJa2TacticalActorRosters();
    CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "native logical world allocated");
    gubWorldMovementCosts = static_cast<UINT8 (*)[MAXDIR][2]>(
        MemAlloc(static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2));
    if (failures || !gubWorldMovementCosts) return 1;
    std::memset(gubWorldMovementCosts, TRAVELCOST_FLAT, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
    InitRenderParams(0); NotifyJa2TacticalWorldLoaded(1);
    RestoreJa2TacticalTurnState(ACTIVE | TURNBASED, OUR_TEAM, 0);
    gbPlayerNum = OUR_TEAM; gusSelectedSoldier = NOBODY;
    is_networked = is_client = is_server = false;
    gGameOptions.fNewTraitSystem = FALSE;
    APBPConstants[AP_MAXIMUM] = 100; APBPConstants[BP_RATIO_RED_PTS_TO_NORMAL] = 100;
    auto& actor = *repository.resolve(4);
    actor.identity().id() = SoldierID{4}; actor.identity().incarnation() = 5;
    actor.identity().profile() = NO_PROFILE; actor.identity().bodyType() = REGMALE;
    actor.roster().active() = actor.roster().inSector() = TRUE;
    actor.roster().team() = OUR_TEAM; actor.status().flags() = SOLDIER_PC;
    actor.vitals().health() = actor.vitals().maximumHealth() = 80;
    actor.vitals().breath() = actor.vitals().maximumBreath() = 76;
    actor.actionPoints().current() = 82;
    actor.position().gridNo() = 20546; actor.position().level() = FIRST_LEVEL;
    actor.position().direction() = EAST; actor.pathing().desiredDirection() = EAST;
    actor.pathing().destinationGrid() = 20546; actor.pathing().finalDestinationGrid() = 20555;
    actor.pathing().pathIndex() = actor.pathing().pathSize() = MAX_PATH_LIST_SIZE;
    std::fill_n(actor.pathing().path(), MAX_PATH_LIST_SIZE, UINT16(EAST));
    actor.movement().mode() = WALKING; actor.movement().waitForGrid(20547, 1);
    CHECK(AdoptJa2TacticalEntity(actor), "exact native actor adopted");
    ETRLEObject frames[8]{}; SGPVObject video{};
    video.usNumberOfObjects = 8; video.pETRLEObject = frames;
    // Native unarmed standing selects this surface directly instead of the index table.
    constexpr UINT16 surface = RGMNOTHING_STD;
    const auto savedSurface = gAnimSurfaceDatabase[surface];
    gAnimSurfaceDatabase[surface].hVideoObject = &video;
    gAnimSurfaceDatabase[surface].uiNumDirections = 8; gAnimSurfaceDatabase[surface].uiNumFramesPerDir = 1;
    gAnimSurfaceDatabase[surface].bStructDataType = NO_STRUCT; gAnimSurfaceDatabase[surface].bProfile = -1;
    for (UINT16 animation : {UINT16(WALKING), UINT16(STANDING), UINT16(START_COWER), UINT16(COWERING),
            UINT16(END_COWER), UINT16(FALLBACKHIT_STOP), UINT16(ROLLOVER)})
    { gubAnimSurfaceIndex[REGMALE][animation] = surface; gubAnimSurfaceItemSubIndex[REGMALE][animation] = INVALID_ANIMATION; }
    const auto preserved = [&] {
        std::array<UINT16, MAX_PATH_LIST_SIZE> path{};
        std::copy_n(actor.pathing().path(), path.size(), path.begin());
        return std::make_tuple(actor.position().gridNo(), actor.position().direction(), actor.actionPoints().current(),
            actor.vitals().breath(), actor.pathing().pathIndex(), actor.pathing().pathSize(),
            actor.pathing().finalDestinationGrid(), path, actor.movement().delayCounter(), actor.movement().delayedCauseGrid());
    }();
    actor.animationPlayback().state() = WALKING;
    CHECK(TacticalActorAnimationTransitions::initializeAnimation(actor, STANDING, 0, true) &&
        actor.animationPlayback().state() == STANDING, "full-buffer route can settle into native standing animation");
    actor.animationPlayback().state() = START_COWER;
    CHECK(TacticalActorAnimationTransitions::initializeAnimation(actor, COWERING, 0, true) &&
        actor.animationPlayback().state() == COWERING, "full-buffer route can enter the native cowering animation");
    CHECK(TacticalActorAnimationTransitions::initializeAnimation(actor, END_COWER, 0, true) &&
        actor.animationPlayback().state() == END_COWER, "full-buffer route can enter the native cowering exit animation");
    actor.animationPlayback().state() = FALLBACKHIT_STOP;
    CHECK(TacticalActorAnimationTransitions::initializeAnimation(actor, ROLLOVER, 0, false) &&
        actor.animationPlayback().state() == ROLLOVER, "full-buffer route can enter the native recovery rollover animation");
    std::array<UINT16, MAX_PATH_LIST_SIZE> afterPath{};
    std::copy_n(actor.pathing().path(), afterPath.size(), afterPath.begin());
    CHECK(preserved == std::make_tuple(actor.position().gridNo(), actor.position().direction(), actor.actionPoints().current(),
        actor.vitals().breath(), actor.pathing().pathIndex(), actor.pathing().pathSize(),
        actor.pathing().finalDestinationGrid(), afterPath, actor.movement().delayCounter(), actor.movement().delayedCauseGrid()),
        "stationary animation changes preserve the consumed native route, wait, position and movement points");
    actor.animationCache().reset(); gAnimSurfaceDatabase[surface] = savedSurface;
    MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr; ReleaseWorldTileMap();
    std::printf("native stationary animation cursor: %d failures\n", failures);
    return failures ? 1 : 0;
}
