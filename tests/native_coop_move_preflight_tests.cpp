// Native pathfinding and MoveToGrid execution with inert animation pixels.
// Reproduce the exhausted standing tile-wait seen in the closed co-op trace.
#include "DedicatedCoopTacticalHost.h"
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
#include "Simulation Commands.h"
#include "TacticalActor.h"
#include "TacticalActorMovementState.h"
#include "TacticalActorStateFlags.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "World Tile Map.h"
#include "structure.h"
#include "worldman.h"
#include "connect.h"
#include "random.h"
#include "sgp.h"
#include "worlddef.h"
#include <Engine/Core/SimulationRandom.h>
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <vector>
#include <thread>

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
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (0)
	CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native runtime starts");
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots(); ResetJa2TacticalActorRosters(); ResetJa2TacticalInterruptForNewWorld();
	CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "native logical world allocated");
	gubWorldMovementCosts = static_cast<UINT8 (*)[MAXDIR][2]>(
		MemAlloc(static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2));
	if (!gubWorldMovementCosts || !GetWorldTileMapSize() || failures) return 1;
	std::memset(gubWorldMovementCosts, TRAVELCOST_FLAT, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
	InitRenderParams(0); NotifyJa2TacticalWorldLoaded(1);
	CHECK(InitPathAI(), "real native pathfinder initializes");
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM; gusSelectedSoldier = NOBODY;
	is_networked = is_client = is_server = false;
	gGameOptions.fNewTraitSystem = FALSE;
	gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = FALSE;
	APBPConstants[AP_MAXIMUM] = 100;
	APBPConstants[AP_MOVEMENT_FLAT] = 4;
	APBPConstants[AP_MODIFIER_WALK] = 0;
	APBPConstants[BP_RATIO_RED_PTS_TO_NORMAL] = 100;
	for (auto& team : gTacticalStatus.Team)
	{ team.bFirstID = SoldierID{1}; team.bLastID = SoldierID{0}; }
	gTacticalStatus.Team[OUR_TEAM].bFirstID = gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{4};
	auto& actor = *repository.resolve(4);
	actor.identity().id() = SoldierID{4}; actor.identity().incarnation() = 5;
	actor.identity().profile() = NO_PROFILE; actor.identity().bodyType() = REGMALE;
	actor.roster().active() = actor.roster().inSector() = TRUE;
	actor.roster().team() = OUR_TEAM; actor.status().flags() = SOLDIER_PC;
	actor.assignment().current() = 0;
	actor.vitals().health() = actor.vitals().maximumHealth() = 80;
	actor.vitals().breath() = actor.vitals().maximumBreath() = 76;
	actor.actionPoints().current() = 82;
	const INT32 origin = 20546, oldDestination = 20555, newDestination = 18965;
	actor.position().gridNo() = origin; actor.position().level() = FIRST_LEVEL;
	actor.position().direction() = NORTH; actor.pathing().desiredDirection() = NORTH;
	actor.movement().mode() = WALKING;
	actor.animationPlayback().state() = STANDING;
	actor.pathing().destinationGrid() = origin;
	actor.pathing().finalDestinationGrid() = oldDestination;
	actor.pathing().pathIndex() = actor.pathing().pathSize() = 30;
	actor.movement().waitForGrid(origin + 1, 1);
	actor.runtime().pendingAction.pathSearchSourceGrid = 731;
	CHECK(AdoptJa2TacticalEntity(actor), "exact native actor adopted");
	const auto id = GetJa2TacticalEntityId(actor);
	DedicatedCoopTacticalJa2LiveState live(game);
	const TacticalActor stopped = actor;
	auto* campaignRandom = GetGameSimulationRandomSource();
	CHECK(campaignRandom != nullptr, "campaign random stream available");
	gubGlobalPathFlags = PATH_THROUGH_PEOPLE | PATH_IGNORE_PERSON_AT_DEST;
	gubNPCAPBudget = 1; gubNPCDistLimit = 1; gfEstimatePath = TRUE;
	gfPathAroundObstacles = FALSE; gfPlotPathToExitGrid = TRUE; gfNPCCircularDistLimit = TRUE;
	gfPlotPathEndDirection = 7;
	std::fill_n(guiPathingData, MAX_PATH_DATA_LENGTH, UINT32(77));
	auto state = [&] {
		std::array<UINT16, MAX_PATH_LIST_SIZE> path{};
		std::copy_n(actor.pathing().path(), path.size(), path.begin());
		return std::make_tuple(actor.position().gridNo(), actor.actionPoints().current(), actor.vitals().breath(),
			actor.pathing().pathIndex(), actor.pathing().pathSize(), actor.pathing().finalDestinationGrid(), path,
			actor.movement().delayCounter(), actor.movement().delayedFlags(), actor.movement().continuedPathValid(),
			actor.animationPlayback().state(), actor.animationIntent().pendingAnimation(),
			actor.animationIntent().pendingDirection(), actor.animationIntent().continuationMode(),
			actor.position().direction(), actor.pathing().desiredDirection(), actor.animationActivity().turningIncrement(),
			actor.pendingAction().action(), actor.status().flags(), actor.runtime().pendingAction.pathSearchSourceGrid,
			campaignRandom->checkpoint(), campaignRandom->consumptionEpoch(),
			gubGlobalPathFlags, gubNPCAPBudget, gubNPCDistLimit, gfEstimatePath, gfPathAroundObstacles,
			gfPlotPathToExitGrid, gfNPCCircularDistLimit, gfPlotPathEndDirection, guiPathingData,
			std::vector<UINT32>(guiPathingData, guiPathingData + MAX_PATH_DATA_LENGTH));
	};
	auto rejected = [&](const char* message) {
		const auto before = state();
		CHECK(!live.canBeginMoveToGrid(id, newDestination, WALKING, false) && state() == before, message);
	};
	actor.actionPoints().current() = 0;
	rejected("unaffordable stopped route fails preflight without spending or clearing state");
	actor = stopped; actor.identity().bodyType() = TOTALBODYTYPES;
	rejected("invalid native body type is rejected before animation/path indexing");
	actor = stopped; actor.position().level() = SECOND_LEVEL + 1;
	rejected("invalid native level is rejected before tile-cost indexing");
	actor = stopped; actor.position().gridNo() = WORLD_MAX;
	rejected("invalid native origin is rejected before pathfinding");
	actor = stopped; actor.vitals().health() = 0;
	rejected("dead actor cannot begin a native route");
	actor = stopped; actor.animationPlayback().state() = WALKING;
	rejected("live locomotion cannot be replaced using a delayed flag");
	actor = stopped; actor.pathing().pathIndex() = 29;
	rejected("unconsumed route cannot be replaced even in standing animation");
	actor = stopped; actor.movement().clearDelay();
	rejected("unreached final destination without an ordinary tile wait stays rejected");
	actor = stopped; actor.animationPlayback().state() = HOPFENCE;
	rejected("active HOPFENCE remains excluded despite ANIM_STATIONARY");
	actor = stopped; actor.movement().clearDelay(); actor.pathing().finalDestinationGrid() = origin;
	actor.animationPlayback().state() = COWERING;
	rejected("cowering animation is not an ordinary idle boundary even on a completed route");
	actor.animationPlayback().state() = END_COWER;
	rejected("stance transition is not an ordinary idle boundary even on a completed route");
	actor = stopped; actor.status().flags() |= SOLDIER_COWERING;
	rejected("retained cowering work cannot be cleared by replacement admission");
	for (UINT16 animation : {UINT16(READY_RIFLE_STAND), UINT16(END_RIFLE_STAND),
		UINT16(SHOOT_RIFLE_STAND), UINT16(CATCH_STANDING)})
	{
		actor = stopped; actor.animationPlayback().state() = animation;
		rejected("raising, lowering, firing and catch animations are not steady ready poses");
	}
	actor = stopped; actor.animationIntent().queueAnimation(HOPFENCE);
	actor.animationActivity().turningUntilDone() = TRUE;
	actor.animationIntent().continueAfterStance(2);
	actor.status().flags() |= SOLDIER_LOCKPENDINGACTIONCOUNTER;
	rejected("pending standing HOPFENCE preserves its continuation and action lock");
	actor = stopped; actor.animationIntent().queueDirection(EAST);
	rejected("queued facing work is not an ordinary tile wait");
	actor = stopped; actor.pathing().desiredDirection() = EAST; actor.animationActivity().turningIncrement() = 1;
	rejected("native facing work remains pending without an intent direction or movement turn flag");
	actor = stopped; actor.pendingAction().begin(1);
	rejected("pending native action cannot be cleared by replacement admission");
	actor = stopped; actor.schedule().beginDoorContinuation(origin + 1);
	rejected("schedule door work cannot be replaced");
	actor = stopped; actor.animationActivity().turningToShoot() = TRUE;
	rejected("attack work cannot be replaced");
	actor = stopped; actor.movement().setContinuedPath(oldDestination);
	rejected("native path continuation remains pending");
	actor = stopped; actor.movement().delayedFlags() = DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE;
	rejected("escalated through-people wait cannot lend its old policy to a new destination");
	actor = stopped; actor.movement().pauseMovement();
	rejected("paused native movement is not an ordinary tile wait");
	actor = stopped;
	const auto before = state();
	UINT8 offThreadDirection = 255;
	bool offThreadAccepted = true;
	std::thread other([&] {
		offThreadAccepted = FindBestPathForMoveAdmission(actor, newDestination, FIRST_LEVEL, WALKING, offThreadDirection);
	});
	other.join();
	CHECK(!offThreadAccepted && offThreadDirection == 255 && state() == before,
		"path admission refuses other threads without touching native globals or output");
	CHECK(!live.canBeginMoveToGrid(id, origin, WALKING, false) && state() == before,
		"same-tile request does not cancel the old route");
	std::memset(gubWorldMovementCosts, TRAVELCOST_BLOCKED, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
	rejected("unreachable replacement preserves the old delayed route and path scratch");
	std::memset(gubWorldMovementCosts, TRAVELCOST_FLAT, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
	for (bool alternate : {false, true})
	{
		gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = alternate;
		actor = stopped;
		const auto preserved = state();
		const bool ready = live.canBeginMoveToGrid(id, newDestination, WALKING, false);
		CHECK(ready && state() == preserved,
			"both native pathfinders isolate poisoned policy, output scratch and campaign RNG including monotonic work");
		actor.actionPoints().current() = 0;
		rejected("both native pathfinders preserve campaign RNG and poisoned globals on low-AP rejection");
		actor = stopped;
		std::memset(gubWorldMovementCosts, TRAVELCOST_BLOCKED, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
		rejected("both native pathfinders preserve campaign RNG and poisoned globals when no path exists");
		std::memset(gubWorldMovementCosts, TRAVELCOST_FLAT, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
	}
	gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = FALSE;
	actor = stopped;
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED, OUR_TEAM, 0);
	for (UINT16 animation : {UINT16(AIM_RIFLE_STAND), UINT16(AIM_RIFLE_CROUCH), UINT16(AIM_RIFLE_PRONE),
		UINT16(AIM_DUAL_STAND), UINT16(AIM_DUAL_CROUCH), UINT16(AIM_DUAL_PRONE),
		UINT16(AIM_ALTERNATIVE_STAND), UINT16(PUNCH_BREATH), UINT16(NINJA_BREATH), UINT16(KNIFE_BREATH)})
	{
		actor = stopped; actor.animationPlayback().state() = animation;
		const auto readyBefore = state();
		CHECK(live.canBeginMoveToGrid(id, newDestination, WALKING, false) && state() == readyBefore,
			"quiescent native weapon-ready poses preserve fresh-move eligibility");
	}
	actor = stopped;
	actor.movement().clearDelay(); actor.pathing().finalDestinationGrid() = origin;
	actor.animationPlayback().state() = WALKING;
	const auto completedBefore = state();
	CHECK(live.canBeginMoveToGrid(id, newDestination, WALKING, false) && state() == completedBefore,
		"completed native locomotion presentation remains eligible for a fresh move");
	actor = stopped;
	const auto noncombatBefore = state();
	const bool admitted = live.canBeginMoveToGrid(id, newDestination, WALKING, false);
	CHECK(admitted && state() == noncombatBefore, "installed noncombat tile wait admits a fresh route without side effects");
	gubGlobalPathFlags = 0; gubNPCAPBudget = 0; gubNPCDistLimit = 0;
	gfEstimatePath = FALSE; gfPathAroundObstacles = TRUE; gfPlotPathToExitGrid = FALSE; gfNPCCircularDistLimit = FALSE;

	ETRLEObject frames[8]{}; SGPVObject video{};
	video.usNumberOfObjects = 8; video.pETRLEObject = frames;
	const auto savedSurface = gAnimSurfaceDatabase[0];
	gAnimSurfaceDatabase[0].hVideoObject = &video;
	gAnimSurfaceDatabase[0].uiNumDirections = 8; gAnimSurfaceDatabase[0].uiNumFramesPerDir = 1;
	gAnimSurfaceDatabase[0].bStructDataType = NO_STRUCT; gAnimSurfaceDatabase[0].bProfile = -1;
	for (UINT16 animation : {UINT16(STANDING), UINT16(WALKING)})
	{ gubAnimSurfaceIndex[REGMALE][animation] = 0; gubAnimSurfaceItemSubIndex[REGMALE][animation] = INVALID_ANIMATION; }
	CHECK(BindJa2SimulationCommandExecutor(game), "real native MoveToGrid executor binds");
	std::uint64_t frame = 0;
	auto dispatch = [&] {
		BeginSimulationCommandFrameBudget(++frame, 1);
		return TryDispatchSimulationCommandNow(SimulationCommand{MoveToGridCommand{
			id, newDestination, WALKING, false, false, SimulationCommandSource::NetworkPeer,
			TacticalMoveOrigin::TeamAwareUi, TacticalPendingActionPolicy::Clear, TacticalCommandAuthorityPolicy::DedicatedCoop}});
	};
	actor.animationIntent().queueAnimation(HOPFENCE); actor.animationActivity().turningUntilDone() = TRUE;
	actor.animationIntent().continueAfterStance(2); actor.status().flags() |= SOLDIER_LOCKPENDINGACTIONCOUNTER;
	const auto pendingRace = state();
	CHECK(dispatch().status == SimulationCommandDispatchStatus::Discarded && state() == pendingRace,
		"traversal beginning after admission is rejected by actual Move execution before any route/pending mutation");
	actor = stopped;
	auto& blocker = *repository.resolve(5);
	blocker.position().gridNo() = newDestination; blocker.position().level() = FIRST_LEVEL;
	blocker.roster().active() = blocker.roster().inSector() = TRUE; blocker.vitals().health() = 80;
	STRUCTURE occupied{}; occupied.usStructureID = 5; occupied.fFlags = STRUCTURE_PERSON;
	GetMapElement(newDestination).pStructureHead = GetMapElement(newDestination).pStructureTail = &occupied;
	CHECK(WhoIsThere2(newDestination, FIRST_LEVEL) == SoldierID{5}, "native target occupancy is real");
	const auto occupiedRace = state();
	CHECK(dispatch().status == SimulationCommandDispatchStatus::Discarded && state() == occupiedRace,
		"target occupied after admission rejects execution without inheriting delayed ignore-person policy or losing old route");
	GetMapElement(newDestination).pStructureHead = GetMapElement(newDestination).pStructureTail = nullptr;
	if (admitted)
	{
		const auto nativeDrawsBefore = campaignRandom->consumptionEpoch();
		const auto result = dispatch();
		CHECK(result.status == SimulationCommandDispatchStatus::Applied && !actor.movement().delayed() &&
			!actor.movement().continuedPathValid() && actor.pathing().finalDestinationGrid() == newDestination &&
			actor.pathing().pathIndex() < actor.pathing().pathSize() && actor.animationPlayback().state() == WALKING,
			"actual accepted MoveToGrid releases old wait and starts a fresh native route");
		CHECK(actor.position().gridNo() == origin && actor.actionPoints().current() == 82,
			"route acceptance does not claim arrival or spend later movement AP");
		CHECK(campaignRandom->consumptionEpoch() > nativeDrawsBefore,
			"accepted ordinary native path execution retains its existing campaign RNG behavior");
	}
	actor.animationCache().reset(); actor = stopped;
	actor.animationPlayback().state() = AIM_RIFLE_STAND;
	actor.movement().clearDelay(); actor.pathing().pathIndex() = actor.pathing().pathSize() = 0;
	actor.pathing().finalDestinationGrid() = origin;
	CHECK(dispatch().status == SimulationCommandDispatchStatus::Applied &&
		actor.animationPlayback().state() == WALKING && actor.pathing().pathIndex() < actor.pathing().pathSize() &&
		actor.pathing().finalDestinationGrid() == newDestination && actor.position().gridNo() == origin &&
		actor.actionPoints().current() == 82,
		"actual dedicated move preserves native ready-to-walking transition without claiming arrival");
	actor.animationCache().reset(); gAnimSurfaceDatabase[0] = savedSurface;
	ShutDownPathAI(); MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr; ReleaseWorldTileMap();
	std::printf("native co-op move preflight: %d failures\n", failures);
	return failures ? 1 : 0;
}
