// Real native path command and fence structures; animation pixels are inert.
#include "Animation Control.h"
#include "Animation Data.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "MemMan.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Points.h"
#include "renderworld.h"
#include "Simulation Commands.h"
#include "SoldierRepository.h"
#include "Soldier Profile Constants.h"
#include "Structure Wrap.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorTraversal.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "World Tile Map.h"
#include "connect.h"
#include "random.h"
#include "sgp.h"
#include "structure.h"
#include "soldier tile.h"
#include "worlddef.h"
#include <Engine/Core/SimulationRandom.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern UINT16 gubAnimSurfaceIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
extern UINT16 gubAnimSurfaceItemSubIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }
namespace {
int failures = 0;
#define CHECK(condition, message) do { if (!(condition)) { \
    ++failures; std::printf("FAIL %d: %s\n", __LINE__, message); } } while (false)
}
int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "RNG installed");
    GameContext& game = GetGameContext();
    CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "runtime started");
    CHECK(BindJa2SimulationCommandExecutor(game), "native executor bound");
    auto& repository = GetJa2SoldierRepository();
    repository.initializeSlots(); ResetJa2TacticalActorRosters();
    CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "logical world allocated");
    gubWorldMovementCosts = static_cast<UINT8 (*)[MAXDIR][2]>(MemAlloc(static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2));
    if (failures || !gubWorldMovementCosts) return 1;
    std::memset(gubWorldMovementCosts, TRAVELCOST_FLAT, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
    InitRenderParams(0); NotifyJa2TacticalWorldLoaded(1);
    RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
    gbPlayerNum = OUR_TEAM; is_networked = is_client = is_server = false;
    gGameOptions.fNewTraitSystem = FALSE;
    APBPConstants[AP_MAXIMUM] = 100; APBPConstants[AP_MIN_LIMIT] = -100;
    APBPConstants[BP_RATIO_RED_PTS_TO_NORMAL] = 100;
    APBPConstants[AP_JUMPFENCE] = 6;

    TacticalActor& actor = *repository.resolve(0);
    actor.identity().id() = SoldierID{0}; actor.identity().incarnation() = 1;
    actor.identity().profile() = NO_PROFILE; actor.identity().bodyType() = REGMALE;
    actor.roster().active() = actor.roster().inSector() = TRUE;
    actor.roster().team() = OUR_TEAM;
    actor.position().gridNo() = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS / 2;
    actor.position().level() = FIRST_LEVEL; actor.position().direction() = EAST;
    actor.pathing().desiredDirection() = EAST;
    actor.vitals().health() = actor.vitals().maximumHealth() = 100;
    actor.vitals().breath() = actor.vitals().maximumBreath() = 100;
    actor.animationPlayback().state() = STANDING; actor.animationPlayback().surface() = 0;
    actor.animationIntent().clearPendingAnimations(); actor.pendingAction().clearAction();
    actor.movement().mode() = WALKING;
    CHECK(AdoptJa2TacticalEntity(actor), "native identity adopted");
    const auto id = GetJa2TacticalEntityId(actor);
    const INT32 fenceGrid = NewGridNo(actor.position().gridNo(), DirectionInc(EAST));
    const INT32 landingGrid = NewGridNo(fenceGrid, DirectionInc(EAST));
    gubWorldMovementCosts[fenceGrid][EAST][FIRST_LEVEL] = TRAVELCOST_FENCE;

    ETRLEObject frames[8]{}; SGPVObject video{};
    video.usNumberOfObjects = 8; video.pETRLEObject = frames;
    const AnimationSurfaceType retainedSurface = gAnimSurfaceDatabase[0];
    gAnimSurfaceDatabase[0].hVideoObject = &video;
    gAnimSurfaceDatabase[0].uiNumDirections = 8; gAnimSurfaceDatabase[0].uiNumFramesPerDir = 1;
    gAnimSurfaceDatabase[0].bStructDataType = NO_STRUCT; gAnimSurfaceDatabase[0].bProfile = -1;
    gubAnimSurfaceIndex[REGMALE][STANDING] = 0;
    gubAnimSurfaceItemSubIndex[REGMALE][STANDING] = INVALID_ANIMATION;
    DB_STRUCTURE definition{}; DB_STRUCTURE_REF reference{}; DB_STRUCTURE_TILE tile{};
    DB_STRUCTURE_TILE* tiles[] = {&tile}; LEVELNODE node{};
    definition.ubHitPoints = 100; definition.ubNumberOfTiles = 1;
    definition.fFlags = STRUCTURE_FENCE;
    reference.pDBStructure = &definition; reference.ppTile = tiles;
    gpWorldLevelData[fenceGrid].pStructHead = &node;
    CHECK(AddStructureToWorld(fenceGrid, 0, &reference, &node), "native fence installed");
    CHECK(IsJumpableFencePresentAtGridNo(fenceGrid), "native fence is jumpable");
    const auto resetRoute = [&] {
        actor.pathing().pathIndex() = 0; actor.pathing().pathSize() = 2;
        actor.pathing().path()[0] = EAST; actor.pathing().path()[1] = EAST;
        actor.pathing().finalDestinationGrid() = landingGrid;
        actor.animationPlayback().state() = STANDING;
        actor.animationIntent().clearPendingAnimations(); actor.animationIntent().clearContinuation();
        actor.animationActivity().turningUntilDone() = FALSE;
        actor.status().flags() &= ~SOLDIER_LOCKPENDINGACTIONCOUNTER;
        actor.movement().clearDelay(); actor.movement().setOutOfActionPoints(false);
        actor.actionPoints().current() = 100;
    };
    std::uint64_t frame = 0;
    const auto dispatch = [&] {
        BeginSimulationCommandFrameBudget(++frame, 1);
        return TryDispatchSystemPathTraverseObstacleCommandNow(id, WALKING);
    };
    resetRoute();
    actor.pathing().pathSize() = 3; actor.pathing().path()[2] = EAST;
    actor.pathing().finalDestinationGrid() = NewGridNo(landingGrid, DirectionInc(EAST));
    const auto started = dispatch();
    CHECK(started.status == SimulationCommandDispatchStatus::Applied &&
        actor.pathing().pathIndex() == 1 &&
        actor.animationIntent().pendingAnimation() == HOPFENCE &&
        actor.animationActivity().turningUntilDone() &&
        actor.position().temporaryGrid() == landingGrid &&
        actor.animationIntent().continuationMode() == 2 &&
        (actor.status().flags() & SOLDIER_LOCKPENDINGACTIONCOUNTER),
        "real fence schedules HOPFENCE and one path continuation");
    const auto firstCommand = game.commandJournal().snapshot().back().command;
    BeginSimulationCommandFrameBudget(++frame, 1);
    const auto duplicate = TryDispatchSystemSimulationCommand(firstCommand);
    CHECK(duplicate.status == SimulationCommandDispatchStatus::Discarded &&
        actor.pathing().pathIndex() == 1 && actor.animationIntent().pendingAnimation() == HOPFENCE &&
        actor.animationActivity().turningUntilDone() && actor.animationIntent().continuationMode() == 2 &&
        (actor.status().flags() & SOLDIER_LOCKPENDINGACTIONCOUNTER),
        "stale traversal cannot cancel the valid pending HOPFENCE");

    CHECK(dispatch().status == SimulationCommandDispatchStatus::Discarded &&
        actor.pathing().pathIndex() == 1 && actor.pathing().pathSize() == 3 &&
        actor.animationIntent().pendingAnimation() == HOPFENCE && actor.animationActivity().turningUntilDone() &&
        actor.animationIntent().continuationMode() == 2,
        "a fresh path producer cannot overwrite an already queued native jump");

    resetRoute();
    actor.pathing().path()[1] = NORTH;
    CHECK(dispatch().status == SimulationCommandDispatchStatus::Discarded &&
        actor.pathing().pathIndex() == 0 && actor.animationIntent().pendingAnimation() == NO_PENDING_ANIMATION,
        "a route that turns on the fence cannot queue an unrelated jump");

    resetRoute();
    actor.pathing().finalDestinationGrid() = NewGridNo(landingGrid, DirectionInc(EAST));
    gpWorldLevelData[landingGrid].uiFlags |= MAPELEMENT_MOVEMENT_RESERVED;
    gpWorldLevelData[landingGrid].ubReservedSoldierID = SoldierID{1};
    const auto waiting = dispatch();
    CHECK(waiting.status == SimulationCommandDispatchStatus::Applied && actor.movement().delayCounter() == 100 &&
        actor.movement().delayedCauseGrid() == landingGrid &&
        actor.animationIntent().pendingAnimation() == NO_PENDING_ANIMATION &&
        actor.animationIntent().continuationMode() == 0 &&
        !(actor.status().flags() & SOLDIER_LOCKPENDINGACTIONCOUNTER),
        "a reserved landing retains native waiting without a phantom jump");
    gpWorldLevelData[landingGrid].uiFlags &= ~MAPELEMENT_MOVEMENT_RESERVED;

    CHECK(DeleteStructureFromWorld(node.pStructureData), "fence removed while cached path cost remains");
    node.pStructureData = nullptr;
    CHECK(!IsJumpableFencePresentAtGridNo(fenceGrid), "native fence lookup sees removal");
    resetRoute();
    CHECK(!TacticalActorTraversal::beginFenceJump(actor), "native jump start refuses the missing fence");
    const auto rejected = dispatch();
    std::printf("Missing fence: status=%u path=%u/%u pending=%u continuation=%u lock=%u\n",
        unsigned(rejected.status), unsigned(actor.pathing().pathIndex()), unsigned(actor.pathing().pathSize()),
        unsigned(actor.animationIntent().pendingAnimation()), unsigned(actor.animationIntent().continuationMode()),
        unsigned((actor.status().flags() & SOLDIER_LOCKPENDINGACTIONCOUNTER) != 0));
    CHECK(rejected.status == SimulationCommandDispatchStatus::Discarded,
        "path command rejects a native fence start failure");
    CHECK(actor.pathing().pathIndex() == 0 && actor.pathing().pathSize() == 2 &&
        actor.animationIntent().pendingAnimation() == NO_PENDING_ANIMATION &&
        actor.animationIntent().continuationMode() == 0 && !actor.animationActivity().turningUntilDone() &&
        !(actor.status().flags() & SOLDIER_LOCKPENDINGACTIONCOUNTER) && actor.actionPoints().current() == 100,
        "failed start preserves cursor, points and continuation ownership");
    actor.animationCache().reset(); gAnimSurfaceDatabase[0] = retainedSurface;
    gpWorldLevelData[fenceGrid].pStructHead = nullptr;
    MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr; ReleaseWorldTileMap();
    std::printf("Native path fence start: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
