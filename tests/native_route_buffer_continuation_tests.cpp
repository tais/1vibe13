// Real pathfinding/route execution at the full-buffer continuation boundary.
#include "Animation Control.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "MemMan.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Points.h"
#include "renderworld.h"
#include "Soldier Profile Constants.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorRouteExecution.h"
#include "TacticalActorStateFlags.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "World Tile Map.h"
#include "connect.h"
#include "random.h"
#include "sgp.h"
#include "soldier tile.h"
#include "worlddef.h"
#include <Engine/Core/SimulationRandom.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }

int main()
{
    int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (false)
    CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "native RNG installed");
    auto& game = GetGameContext();
    CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native runtime started");
    auto& repository = GetJa2SoldierRepository();
    repository.initializeSlots(); ResetJa2TacticalActorRosters();
    CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "logical world allocated");
    gubWorldMovementCosts = static_cast<UINT8 (*)[MAXDIR][2]>(MemAlloc(static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2));
    if (failures || !gubWorldMovementCosts) return 1;
    std::memset(gubWorldMovementCosts, TRAVELCOST_FLAT, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
    InitRenderParams(0); NotifyJa2TacticalWorldLoaded(1);
    CHECK(InitPathAI(), "native pathfinder initialized");
    RestoreJa2TacticalTurnState(ACTIVE | TURNBASED, OUR_TEAM, 0);
    gbPlayerNum = OUR_TEAM; is_networked = is_client = is_server = false;
    gGameOptions.fNewTraitSystem = FALSE;
    APBPConstants[AP_MAXIMUM] = 100; APBPConstants[AP_MOVEMENT_FLAT] = 4;
    APBPConstants[BP_RATIO_RED_PTS_TO_NORMAL] = 100;
    for (auto& team : gTacticalStatus.Team)
    { team.bFirstID = SoldierID{1}; team.bLastID = SoldierID{0}; }
    gTacticalStatus.Team[OUR_TEAM].bFirstID = gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{4};
    TacticalActor& actor = *repository.resolve(4);
    actor.identity().id() = SoldierID{4}; actor.identity().incarnation() = 5;
    actor.identity().profile() = NO_PROFILE; actor.identity().bodyType() = REGMALE;
    actor.roster().active() = actor.roster().inSector() = TRUE; actor.roster().team() = OUR_TEAM;
    actor.status().flags() = SOLDIER_PC;
    actor.vitals().health() = actor.vitals().maximumHealth() = 81;
    actor.vitals().breath() = actor.vitals().maximumBreath() = 76;
    actor.actionPoints().current() = 82;
    const INT32 origin = 20546, goal = 20555;
    actor.position().gridNo() = origin; actor.position().level() = FIRST_LEVEL;
    actor.position().direction() = EAST; actor.pathing().desiredDirection() = EAST;
    actor.pathing().destinationGrid() = origin; actor.pathing().finalDestinationGrid() = goal;
    actor.pathing().pathIndex() = actor.pathing().pathSize() = MAX_PATH_LIST_SIZE;
    std::fill_n(actor.pathing().path(), MAX_PATH_LIST_SIZE, UINT16(EAST));
    actor.animationPlayback().state() = WALKING; actor.movement().mode() = WALKING;
    CHECK(AdoptJa2TacticalEntity(actor), "native actor identity adopted");
    const TacticalActor exhausted = actor;
    for (const bool alternate : {false, true})
    {
        actor = exhausted;
        gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = alternate;
        // Match Overhead's noncombat exhausted-route branch: find the next
        // tile, mark its native wait, then request ContinueMovement.
        CHECK(FindBestPath(&actor, goal, FIRST_LEVEL, WALKING, NO_COPYROUTE, PATH_THROUGH_PEOPLE) == 9,
            "native search finds the remaining nine steps after the first full buffer");
        const INT32 next = NewGridNo(origin, DirectionInc(static_cast<UINT8>(guiPathingData[0])));
        SetDelayedTileWaiting(&actor, next, 1);
        CHECK(actor.pathing().pathIndex() == MAX_PATH_LIST_SIZE && actor.movement().delayCounter() == 1,
            "fixture reaches the exact full-buffer cursor and native wait");
        const bool resumed = TacticalActorRouteExecution::requestPath(actor, goal, WALKING,
            TacticalActorRouteExecution::PathOrigin::ContinueMovement, false, false);
        std::printf("algorithm=%u resumed=%u path=%u/%u delay=%u grid=%d final=%d\n",
            unsigned(alternate), unsigned(resumed), unsigned(actor.pathing().pathIndex()),
            unsigned(actor.pathing().pathSize()), unsigned(actor.movement().delayCounter()),
            actor.position().gridNo(), actor.pathing().finalDestinationGrid());
        CHECK(resumed && actor.pathing().pathIndex() == 0 && actor.pathing().pathSize() == 10 &&
            actor.pathing().path()[1] == EAST && !actor.movement().delayed() &&
            actor.pathing().finalDestinationGrid() == goal && actor.animationPlayback().state() == WALKING,
            "full native buffer continues into the remaining route instead of settling into a permanent wait");
        CHECK(actor.position().gridNo() == origin && actor.actionPoints().current() == 82 && actor.vitals().breath() == 76,
            "native continuation setup neither teleports nor spends later movement points");
    }
    actor = exhausted;
    CHECK(!TacticalActorRouteExecution::requestPath(actor, goal, WALKING,
        TacticalActorRouteExecution::PathOrigin::TeamAwareUi, false, false) &&
        actor.pathing().pathIndex() == MAX_PATH_LIST_SIZE,
        "full-buffer exception belongs only to native continuation, not an unvalidated replacement order");
    actor.collapseState().tactical() = TRUE;
    CHECK(!TacticalActorRouteExecution::requestPath(actor, goal, WALKING,
        TacticalActorRouteExecution::PathOrigin::ContinueMovement, false, false) &&
        actor.collapseState().tactical() && actor.pathing().pathIndex() == MAX_PATH_LIST_SIZE,
        "exhausted continuation cannot enter collapse recovery before a fresh route exists");
    actor = exhausted;
    actor.status().flags() |= SOLDIER_COWERING;
    CHECK(!TacticalActorRouteExecution::requestPath(actor, goal, WALKING,
        TacticalActorRouteExecution::PathOrigin::ContinueMovement, false, false) &&
        (actor.status().flags() & SOLDIER_COWERING) && actor.pathing().pathIndex() == MAX_PATH_LIST_SIZE,
        "exhausted continuation cannot enter EPC cowering recovery before replacing the route");
    for (const UINT16 animation : {UINT16(COWERING), UINT16(COWERING_PRONE)})
    {
        actor = exhausted; actor.animationPlayback().state() = animation;
        CHECK(!TacticalActorRouteExecution::requestPath(actor, goal, WALKING,
            TacticalActorRouteExecution::PathOrigin::ContinueMovement, false, false) &&
            actor.animationPlayback().state() == animation && actor.pathing().pathIndex() == MAX_PATH_LIST_SIZE,
            "cowering animations preserve their unconsumed recovery and exhausted cursor");
    }
    actor = exhausted;
    actor.pathing().pathIndex() = MAX_PATH_LIST_SIZE + 1;
    CHECK(!TacticalActorRouteExecution::requestPath(actor, goal, WALKING,
        TacticalActorRouteExecution::PathOrigin::ContinueMovement, false, false) &&
        actor.pathing().pathIndex() == MAX_PATH_LIST_SIZE + 1 && actor.pathing().pathSize() == MAX_PATH_LIST_SIZE,
        "cursor beyond the allocation remains rejected without normalization");
    actor = exhausted;
    actor.pathing().pathSize() = MAX_PATH_LIST_SIZE - 1;
    CHECK(!TacticalActorRouteExecution::requestPath(actor, goal, WALKING,
        TacticalActorRouteExecution::PathOrigin::ContinueMovement, false, false) &&
        actor.pathing().pathIndex() == MAX_PATH_LIST_SIZE && actor.pathing().pathSize() == MAX_PATH_LIST_SIZE - 1,
        "exact-capacity cursor is accepted only for an actually exhausted full buffer");
    ShutDownPathAI(); MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr;
    ReleaseWorldTileMap();
    std::printf("native full-buffer route continuation: %d failures\n", failures);
    return failures ? 1 : 0;
}
