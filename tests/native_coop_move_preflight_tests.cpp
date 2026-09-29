#include "move_diagnostic_capture.h"
#include "TacticalActorRouteExecution.h"
// Native pathfinding and MoveToGrid execution with inert animation pixels.
// Reproduce the exhausted standing tile-wait seen in the closed co-op trace.
#include "DedicatedCoopTacticalHost.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Weapons.h"
#include "Timer Control.h"
#include "MemMan.h"
#include "FileMan.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Points.h"
#include "renderworld.h"
#include "Soldier Profile Constants.h"
#include "SoldierRepository.h"
#include "Simulation Commands.h"
#include "TacticalActor.h"
#include "TacticalActorMovementState.h"
#include "TacticalActorWorldPlacement.h"
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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>

extern UINT8* gubGridNoMarkers;
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
	const auto fixtureRoot = std::filesystem::temp_directory_path() /
		("ja2-native-retained-movement-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(fixtureRoot / "scripts");
	{
		std::ofstream script(fixtureRoot / "scripts/Overhead.lua");
		script << "function HandleAtNewGridNo() end\n";
		CHECK(script.good(), "native arrival Lua callback fixture written");
	}
	CHECK(InitializeMemoryManager(), "native memory manager initialized");
	vfs_init::VfsConfig vfsConfig;
	auto* profile = new vfs_init::Profile();
	profile->m_name = L"native-retained-movement";
	profile->m_root = vfs::Path(fixtureRoot.generic_u8string()); profile->m_writable = false;
	auto* location = new vfs_init::Location(); location->m_type = L"DIRECTORY";
	profile->addLocation(location, true); vfsConfig.addProfile(profile, true);
	CHECK(vfs_init::initVirtualFileSystem(vfsConfig, false) && InitializeFileManager(nullptr),
		"native arrival callback VFS mounted");
	if (failures) return 1;
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
		return std::make_tuple(actor.position().gridNo(), actor.position().worldX(), actor.position().worldY(),
			actor.pathing().destinationGrid(), actor.pathing().destinationX(), actor.pathing().destinationY(),
			actor.movement().outOfActionPoints(), actor.actionPoints().current(), actor.vitals().breath(),
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
	auto rejected = [&](TacticalMoveFailure expected, const char* message) {
		const auto before = state();
		TacticalMoveDiagnostic diagnostic;
		CHECK(!TacticalActorRouteExecution::canBeginMoveToGrid(actor, newDestination, WALKING, false, &diagnostic) &&
			diagnostic.reason == expected && state() == before, message);
	};
	actor.actionPoints().current() = 0;
	rejected(TacticalMoveFailure::InsufficientPoints, "unaffordable stopped route fails preflight without spending or clearing state");
	actor = stopped; actor.identity().bodyType() = TOTALBODYTYPES;
	rejected(TacticalMoveFailure::LiveRouteContext, "invalid native body type is rejected before animation/path indexing");
	actor = stopped; actor.position().level() = SECOND_LEVEL + 1;
	rejected(TacticalMoveFailure::LiveRouteContext, "invalid native level is rejected before tile-cost indexing");
	actor = stopped; actor.position().gridNo() = WORLD_MAX;
	rejected(TacticalMoveFailure::LiveRouteContext, "invalid native origin is rejected before pathfinding");
	actor = stopped; actor.vitals().health() = 0;
	rejected(TacticalMoveFailure::Health, "dead actor cannot begin a native route");
	actor = stopped; actor.animationPlayback().state() = WALKING;
	rejected(TacticalMoveFailure::NonIdlePose, "live locomotion cannot be replaced using a delayed flag");
	actor = stopped; actor.pathing().pathIndex() = 29;
	rejected(TacticalMoveFailure::PathUnconsumed, "unconsumed route cannot be replaced even in standing animation");
	actor = stopped; actor.movement().clearDelay();
	rejected(TacticalMoveFailure::RetainedRoute, "unreached final destination without an ordinary tile wait stays rejected");
	actor = stopped; actor.animationPlayback().state() = HOPFENCE;
	rejected(TacticalMoveFailure::AnimationActivity, "active HOPFENCE remains excluded despite ANIM_STATIONARY");
	actor = stopped; actor.movement().clearDelay(); actor.pathing().finalDestinationGrid() = origin;
	actor.animationPlayback().state() = COWERING;
	rejected(TacticalMoveFailure::NonIdlePose, "cowering animation is not an ordinary idle boundary even on a completed route");
	actor.animationPlayback().state() = END_COWER;
	rejected(TacticalMoveFailure::NonIdlePose, "stance transition is not an ordinary idle boundary even on a completed route");
	actor = stopped; actor.status().flags() |= SOLDIER_COWERING;
	rejected(TacticalMoveFailure::ActionLockOrCowering, "retained cowering work cannot be cleared by replacement admission");
	for (UINT16 animation : {UINT16(READY_RIFLE_STAND), UINT16(END_RIFLE_STAND),
		UINT16(SHOOT_RIFLE_STAND), UINT16(CATCH_STANDING)})
	{
		actor = stopped; actor.animationPlayback().state() = animation;
		rejected((animation == SHOOT_RIFLE_STAND || animation == CATCH_STANDING) ? TacticalMoveFailure::AnimationActivity : TacticalMoveFailure::NonIdlePose, "raising, lowering, firing and catch animations are not steady ready poses");
	}
	actor = stopped; actor.animationIntent().queueAnimation(HOPFENCE);
	actor.animationActivity().turningUntilDone() = TRUE;
	actor.animationIntent().continueAfterStance(2);
	actor.status().flags() |= SOLDIER_LOCKPENDINGACTIONCOUNTER;
	rejected(TacticalMoveFailure::PendingAnimation, "pending standing HOPFENCE preserves its continuation and action lock");
	actor = stopped; actor.animationIntent().queueDirection(EAST);
	rejected(TacticalMoveFailure::PendingDirection, "queued facing work is not an ordinary tile wait");
	actor = stopped; actor.pathing().desiredDirection() = EAST; actor.animationActivity().turningIncrement() = 1;
	rejected(TacticalMoveFailure::DirectionMismatch, "native facing work remains pending without an intent direction or movement turn flag");
	actor = stopped; actor.pendingAction().begin(1);
	rejected(TacticalMoveFailure::PendingAction, "pending native action cannot be cleared by replacement admission");
	actor = stopped; actor.schedule().beginDoorContinuation(origin + 1);
	rejected(TacticalMoveFailure::ScheduleDoor, "schedule door work cannot be replaced");
	actor = stopped; actor.animationActivity().turningToShoot() = TRUE;
	rejected(TacticalMoveFailure::TurningToShoot, "attack work cannot be replaced");
	actor = stopped; actor.movement().setContinuedPath(oldDestination);
	rejected(TacticalMoveFailure::ContinuedPath, "native path continuation remains pending");
	actor = stopped; actor.movement().delayedFlags() = DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE;
	rejected(TacticalMoveFailure::RetainedRoute, "escalated through-people wait cannot lend its old policy to a new destination");
	actor = stopped; actor.movement().pauseMovement();
	rejected(TacticalMoveFailure::MovementPaused, "paused native movement is not an ordinary tile wait");
	actor = stopped;
	const auto before = state();
	UINT8 offThreadDirection = 255;
	bool offThreadAccepted = true;
	TacticalMoveFailure offThreadFailure = TacticalMoveFailure::None;
	std::thread other([&] {
		offThreadAccepted = FindBestPathForMoveAdmission(actor, newDestination, FIRST_LEVEL, WALKING, offThreadDirection, &offThreadFailure);
	});
	other.join();
	CHECK(!offThreadAccepted && offThreadFailure == TacticalMoveFailure::ProbeWrongThread && offThreadDirection == 255 && state() == before,
		"path admission refuses other threads without touching native globals or output");
	CHECK(!live.canBeginMoveToGrid(id, origin, WALKING, false) && state() == before,
		"same-tile request does not cancel the old route");
	std::memset(gubWorldMovementCosts, TRAVELCOST_BLOCKED, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
	rejected(TacticalMoveFailure::NoPath, "unreachable replacement preserves the old delayed route and path scratch");
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
		rejected(TacticalMoveFailure::InsufficientPoints, "both native pathfinders preserve campaign RNG and poisoned globals on low-AP rejection");
		actor = stopped;
		std::memset(gubWorldMovementCosts, TRAVELCOST_BLOCKED, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
		rejected(TacticalMoveFailure::NoPath, "both native pathfinders preserve campaign RNG and poisoned globals when no path exists");
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

	for (bool enabled : {false, true})
	{
		actor = stopped; actor.animationIntent().queueDirection(EAST);
		const auto beforeTrace = state();
		MoveDiagnosticCapture trace(enabled);
		CHECK(trace.valid(), "trace capture opens");
		const bool accepted = live.canBeginMoveToGrid(id, newDestination, WALKING, false);
		const auto output = trace.finish();
		CHECK(!accepted && state() == beforeTrace, "opt-in admission trace preserves native rejection, route, actor and RNG");
		CHECK(enabled ? output.find("stage=admission reason=pending-direction") != std::string::npos : output.empty(),
			"admission diagnostics are explicit opt-in and name the actual guard");
	}
	actor = stopped;
	ETRLEObject frames[8]{}; SGPVObject video{};
	video.usNumberOfObjects = 8; video.pETRLEObject = frames;
	const auto savedSurface = gAnimSurfaceDatabase[0];
	gAnimSurfaceDatabase[0].hVideoObject = &video;
	gAnimSurfaceDatabase[0].uiNumDirections = 8; gAnimSurfaceDatabase[0].uiNumFramesPerDir = 1;
	gAnimSurfaceDatabase[0].bStructDataType = NO_STRUCT; gAnimSurfaceDatabase[0].bProfile = -1;
	for (UINT16 animation : {UINT16(STANDING), UINT16(WALKING), UINT16(RUNNING)})
	{ gubAnimSurfaceIndex[REGMALE][animation] = 0; gubAnimSurfaceItemSubIndex[REGMALE][animation] = INVALID_ANIMATION; }
	CHECK(BindJa2SimulationCommandExecutor(game), "real native MoveToGrid executor binds");
	std::uint64_t frame = 0;
	auto dispatchTo = [&](INT32 destination, UINT16 mode) {
		BeginSimulationCommandFrameBudget(++frame, 1);
		return TryDispatchSimulationCommandNow(SimulationCommand{MoveToGridCommand{
			id, destination, mode, false, false, SimulationCommandSource::NetworkPeer,
			TacticalMoveOrigin::TeamAwareUi, TacticalPendingActionPolicy::Clear, TacticalCommandAuthorityPolicy::DedicatedCoop}});
	};
	auto dispatch = [&] { return dispatchTo(newDestination, WALKING); };
	actor.animationIntent().queueAnimation(HOPFENCE); actor.animationActivity().turningUntilDone() = TRUE;
	actor.animationIntent().continueAfterStance(2); actor.status().flags() |= SOLDIER_LOCKPENDINGACTIONCOUNTER;
	const auto pendingRace = state();
	{
		MoveDiagnosticCapture trace(true);
		const auto result = dispatch();
		const auto output = trace.finish();
		CHECK(result.status == SimulationCommandDispatchStatus::Discarded && state() == pendingRace,
			"traversal beginning after admission is rejected by actual Move execution before any route/pending mutation");
		CHECK(output.find("stage=execution-readiness reason=pending-animation") != std::string::npos,
			"actual execution labels its own single revalidation failure");
	}
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
	actor = stopped;
	gubNPCAPBudget = 1;
	{
		MoveDiagnosticCapture trace(true);
		const auto result = dispatch();
		const auto output = trace.finish();
		CHECK(result.status == SimulationCommandDispatchStatus::Discarded &&
			output.find("stage=execution-path-start reason=native-path-start-rejected") != std::string::npos &&
			output.find("firstStep=1") != std::string::npos,
			"actual legacy path start failure is distinct from successful fresh preflight");
	}
	gubNPCAPBudget = 0;
	actor = stopped;
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

	gubGridNoMarkers = static_cast<UINT8*>(MemAlloc(WORLD_MAX));
	CHECK(gubGridNoMarkers != nullptr, "native arrival sight markers allocated");
	if (!gubGridNoMarkers) return 1;
	std::memset(gubGridNoMarkers, 0, WORLD_MAX);
	// Drive the real Overhead final-destination branch: arrival cancels the
	// route before the option deliberately retains locomotion and pauses it.
	// Pixel frames are inert; native arrival, policy, admission and dispatch run.
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gGameExternalOptions.fNoStandingAnimAdjustInCombat = TRUE;
	APBPConstants[AP_START_RUN_COST] = 7;
	CHECK(AddJa2ActiveTacticalActor(id) >= 0, "arrival actor enters real overhead roster");
	for (UINT16 retainedAnimation : {UINT16(WALKING), UINT16(RUNNING)})
	{
		actor.animationCache().reset(); actor = stopped;
		actor.movement().clearDelay();
		actor.animationPlayback().state() = retainedAnimation;
		actor.animationPlayback().code() = 0;
		actor.movement().mode() = retainedAnimation;
		actor.pathing().pathIndex() = 0; actor.pathing().pathSize() = 1;
		actor.pathing().path()[0] = NORTH;
		actor.pathing().finalDestinationGrid() = origin;
		INT16 centerX = 0, centerY = 0;
		ConvertGridNoToCenterCellXY(origin, &centerX, &centerY);
		actor.position().setWorldCoordinates(centerX, centerY);
		actor.pathing().destinationX() = centerX; actor.pathing().destinationY() = centerY;
		actor.movement().markPastXDestination(); actor.movement().markPastYDestination();
		actor.renderBindings().faceIndex() = -1;
		actor.timing().start(SoldierTimingComponent::Timer::AnimationUpdate, 0);
		gusAnimInst[retainedAnimation][0] = 1;
		giTimerCounters[TOVERHEAD] = 0;
		CHECK(ExecuteOverhead(), "real native overhead arrival runs");
		CHECK(actor.animationPlayback().state() == retainedAnimation &&
			actor.pathing().pathIndex() == 0 && actor.pathing().pathSize() == 0 &&
			actor.pathing().finalDestinationGrid() == origin && actor.movement().outOfActionPoints() &&
			actor.position().worldX() == centerX && actor.position().worldY() == centerY &&
			!actor.animationIntent().hasPendingAnimation() && actor.actionPoints().current() == 82,
			"native arrival retains paused locomotion at tile center after clearing its route");
		const TacticalActor retained = actor;
		for (bool alternate : {false, true})
		{
			gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = alternate;
			const auto preserved = state();
			CHECK(live.canBeginMoveToGrid(id, newDestination, retainedAnimation, false) && state() == preserved,
				"both native pathfinders admit actual retained zero-route arrival without mutation");
		}
		gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = FALSE;
		actor.movement().setOutOfActionPoints(false);
		rejected(TacticalMoveFailure::NonIdlePose, "zero-route locomotion without native pause marker remains active");
		actor = retained; actor.pathing().finalDestinationGrid() = oldDestination;
		rejected(TacticalMoveFailure::NonIdlePose, "unreached zero-route destination is not retained arrival");
		actor = retained; actor.position().worldX() += 0.25f;
		rejected(TacticalMoveFailure::NonIdlePose, "subtile locomotion cannot be mistaken for centered arrival");
		actor = retained; actor.pathing().destinationGrid() = oldDestination;
		rejected(TacticalMoveFailure::NonIdlePose, "inconsistent next tile is not completed arrival");
		actor = retained; actor.pathing().destinationY() += 1;
		rejected(TacticalMoveFailure::NonIdlePose, "inconsistent destination center is not completed arrival");
		actor = retained; actor.pathing().pathSize() = 1;
		rejected(TacticalMoveFailure::PathUnconsumed, "paused partial route remains excluded");
		actor = retained; actor.animationIntent().queueAnimation(HOPFENCE);
		rejected(TacticalMoveFailure::PendingAnimation, "retained arrival cannot replace pending fence traversal");
		actor = retained; actor.animationPlayback().state() = HOPFENCE;
		rejected(TacticalMoveFailure::AnimationActivity, "active traversal cannot reuse retained arrival marker");
		actor = retained; actor.animationPlayback().state() = SWATTING;
		rejected(TacticalMoveFailure::NonIdlePose, "crouched movement excluded by native retention policy stays rejected");
		actor = retained; actor.movement().setNetworkDelayed(true);
		rejected(TacticalMoveFailure::NonIdlePose, "network-delayed locomotion is not the native retained boundary");
		actor = retained; actor.status().flags() |= SOLDIER_PAUSEANIMOVE;
		rejected(TacticalMoveFailure::NonIdlePose, "animation-move pause flag cannot reuse retained arrival marker");
		actor = retained; actor.status().flags() &= ~SOLDIER_PC;
		rejected(TacticalMoveFailure::NonIdlePose, "native retention is limited to player merc policy");
		actor = retained; gGameExternalOptions.fNoStandingAnimAdjustInCombat = FALSE;
		rejected(TacticalMoveFailure::NonIdlePose, "disabled retention policy cannot admit zero-route locomotion");
		gGameExternalOptions.fNoStandingAnimAdjustInCombat = TRUE;
		RestoreJa2TacticalTurnState(ACTIVE | TURNBASED, OUR_TEAM, 0);
		rejected(TacticalMoveFailure::NonIdlePose, "realtime zero-route locomotion cannot use combat retention policy");
		RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
		actor = retained; actor.actionPoints().current() = 0;
		rejected(TacticalMoveFailure::InsufficientPoints, "retained arrival still requires native first-step AP");
		actor = retained;
		if (retainedAnimation == RUNNING)
		{
			const INT32 nextGrid = NewGridNo(origin, DirectionInc(NORTH));
			const INT16 retainedCost = ActionPointCost(&actor, nextGrid, NORTH, RUNNING);
			actor.animationPlayback().state() = STANDING;
			const INT16 restartedCost = ActionPointCost(&actor, nextGrid, NORTH, RUNNING);
			CHECK(restartedCost == retainedCost + GetAPsStartRun(&actor) && restartedCost > retainedCost,
				"native cost proves forcing standing would add the run restart surcharge");
			actor = retained;
		}
		for (bool alternate : {false, true})
		{
			gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = alternate;
			actor.animationCache().reset(); actor = retained;
			actor.position().direction() = WEST; actor.pathing().desiredDirection() = WEST;
			actor.movement().animationDirection() = WEST;
			for (INT32 destination : {origin - 1, origin - 2})
			{
				INT16 targetX = 0, targetY = 0;
				ConvertGridNoToCenterCellXY(destination, &targetX, &targetY);
				const INT16 beforeAP = actor.actionPoints().current();
				const INT16 stepCost = ActionPointCost(&actor, destination, WEST, retainedAnimation);
				CHECK(stepCost == 4, "retained native step excludes the seven-AP run restart surcharge");
				CHECK(dispatchTo(destination, retainedAnimation).status == SimulationCommandDispatchStatus::Applied &&
					actor.animationPlayback().state() == retainedAnimation && !actor.movement().outOfActionPoints() &&
					actor.pathing().pathIndex() == 0 && actor.pathing().pathSize() == 1 &&
					actor.pathing().destinationGrid() == destination && actor.pathing().finalDestinationGrid() == destination &&
					actor.pathing().destinationX() == targetX && actor.pathing().destinationY() == targetY &&
					actor.actionPoints().current() == beforeAP - stepCost,
					"retained route restart initializes the first native tile and charges its exact AP once");
				for (unsigned tick = 0; tick < 64; ++tick)
				{
					// Pixel frames are inert; only scheduling is advanced by the fixture.
					// Overhead owns locomotion, grid updates, route cursor and arrival.
					actor.animationPlayback().code() = 0;
					actor.timing().start(SoldierTimingComponent::Timer::AnimationUpdate, 0);
					giTimerCounters[TOVERHEAD] = 0;
					CHECK(ExecuteOverhead(), "native retained replan movement tick runs");
					if (actor.movement().outOfActionPoints() || actor.animationPlayback().state() == STANDING ||
						(actor.pathing().pathSize() != 0 && actor.pathing().pathIndex() == actor.pathing().pathSize())) break;
				}
				CHECK(actor.position().gridNo() == destination && actor.position().worldX() == targetX && actor.position().worldY() == targetY &&
					actor.pathing().destinationGrid() == destination && actor.pathing().finalDestinationGrid() == destination &&
					actor.pathing().pathIndex() == 0 && actor.pathing().pathSize() == 0 && actor.movement().outOfActionPoints() &&
					actor.animationPlayback().state() == retainedAnimation,
					"consecutive native retained route reaches exact next tile center and native arrival boundary");
				CHECK(actor.actionPoints().current() == beforeAP - stepCost,
					"actual retained arrival preserves the one native step debit without a run restart charge");
			}
			CHECK(TacticalActorWorldPlacement::removeFromGrid(actor), "consecutive arrival fixture releases native world placement");
		}
		gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = FALSE;
		actor.animationCache().reset(); actor = retained;
	}

	// Use a real rifle: the earlier ready-pose admission case has empty hands
	// and therefore never enters WALKING_WEAPON_RDY. Pixels remain inert while
	// native Overhead owns every coordinate, route cursor, debit and arrival.
	gGameExternalOptions.fAllowWalkingWithWeaponRaised = TRUE;
	gGameExternalOptions.ubEnergyCostForWeaponWeight = 0;
	gGameExternalOptions.ubAllowAlternativeWeaponHolding = 0;
	APBPConstants[AP_MODIFIER_READY] = 1;
	gMAXITEMS_READ = 7;
	Item[5].usItemClass = IC_GUN;
	Weapon[5].ubWeaponType = GUN_RIFLE; Weapon[5].ubReadyTime = 6;
	Weapon[5].ubWeaponClass = RIFLECLASS; Weapon[5].ubCalibre = 1; Weapon[5].ubMagSize = 30;
	Item[6].usItemClass = IC_AMMO; Item[6].ubClassIndex = 0;
	Magazine[0].ubCalibre = 1; Magazine[0].ubMagSize = 30; Magazine[0].ubAmmoType = 0;
	Magazine[1].ubCalibre = NOAMMO; AmmoTypes[0].standardIssue = TRUE;
	for (UINT16 animation : {UINT16(AIM_RIFLE_STAND), UINT16(WALKING_WEAPON_RDY)})
	{
		gubAnimSurfaceIndex[REGMALE][animation] = 0;
		gubAnimSurfaceItemSubIndex[REGMALE][animation] = INVALID_ANIMATION;
		gusAnimInst[animation][0] = 1;
	}
	auto raisedTick = [&] {
		actor.animationPlayback().code() = 0;
		actor.timing().start(SoldierTimingComponent::Timer::AnimationUpdate, 0);
		giTimerCounters[TOVERHEAD] = 0;
		CHECK(ExecuteOverhead(), "native raised-rifle movement tick runs");
	};
	auto raisedTrace = [&](const char* stage, bool alternate) {
		std::printf("raised-walk stage=%s alternate=%u grid=%d xy=%.3f,%.3f next=%d dest=%d,%d final=%d path=%u/%u anim=%u pending=%u ap=%d noAP=%u reason=%u delay=%u paused=%u pauseAnim=%u\n",
			stage, alternate, actor.position().gridNo(), actor.position().worldX(), actor.position().worldY(),
			actor.pathing().destinationGrid(), actor.pathing().destinationX(), actor.pathing().destinationY(),
			actor.pathing().finalDestinationGrid(), actor.pathing().pathIndex(), actor.pathing().pathSize(),
			actor.animationPlayback().state(), actor.animationIntent().pendingAnimation(), actor.actionPoints().current(),
			actor.movement().outOfActionPoints(), actor.movement().stopReason(), actor.movement().delayCounter(),
			actor.movement().movementPaused(), !!(actor.status().flags() & SOLDIER_PAUSEANIMOVE));
	};
	for (bool alternate : {false, true})
	for (unsigned scenario : {0u, 1u, 2u})
	{
		const bool sighting = scenario != 0, traceEnabled = scenario == 2;
		gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = alternate;
		actor.animationCache().reset(); actor = stopped;
		actor.movement().clearDelay();
		actor.pathing().pathIndex() = actor.pathing().pathSize() = 0;
		actor.position().gridNo() = actor.pathing().destinationGrid() = actor.pathing().finalDestinationGrid() = 16255;
		INT16 startX = 0, startY = 0;
		ConvertGridNoToCenterCellXY(actor.position().gridNo(), &startX, &startY);
		actor.position().setWorldCoordinates(startX, startY);
		actor.pathing().destinationX() = startX; actor.pathing().destinationY() = startY;
		actor.renderBindings().faceIndex() = -1;
		actor.animationPlayback().state() = AIM_RIFLE_STAND;
		CHECK(CreateItem(5, 100, &actor.inventory()[HANDPOS]), "raised route carries a real native rifle");
		const INT16 initialAP = actor.actionPoints().current();
		CHECK(dispatchTo(15455, WALKING).status == SimulationCommandDispatchStatus::Applied &&
			actor.animationPlayback().state() == WALKING_WEAPON_RDY && actor.pathing().pathSize() == 5 &&
			actor.actionPoints().current() == initialAP - 5,
			"actual ready-rifle route starts native weapon-raised locomotion and charges its first tile");
		raisedTrace("start", alternate);
		INT32 observedGrid = actor.position().gridNo();
		for (unsigned tick = 0; tick < 512; ++tick)
		{
			raisedTick();
			if (actor.position().gridNo() != observedGrid)
			{
				observedGrid = actor.position().gridNo(); raisedTrace("tile", alternate);
				if (sighting && actor.pathing().pathIndex() >= 2) break;
			}
			if (actor.animationPlayback().state() != WALKING_WEAPON_RDY || actor.movement().outOfActionPoints()) break;
		}
		if (sighting)
		{
			INT16 sightX = 0, sightY = 0;
			ConvertGridNoToCenterCellXY(actor.position().gridNo(), &sightX, &sightY);
			CHECK(actor.animationPlayback().state() == WALKING_WEAPON_RDY &&
				actor.pathing().pathIndex() < actor.pathing().pathSize() &&
				(actor.position().worldX() != sightX || actor.position().worldY() != sightY),
				"sighting fixture reaches a real off-center unconsumed raised route");
			const auto beforeSighting = std::make_tuple(actor.position().gridNo(), actor.position().worldX(), actor.position().worldY(),
				actor.pathing().pathIndex(), actor.pathing().pathSize(), actor.pathing().finalDestinationGrid(), actor.actionPoints().current());
			const auto sightingRandom = std::make_pair(campaignRandom->checkpoint(), campaignRandom->consumptionEpoch());
			{
				MoveDiagnosticCapture trace(traceEnabled);
				const bool traceValid = trace.valid();
				const bool haltedForSighting = TacticalActorRouteExecution::haltForSighting(actor, true);
				const auto output = trace.finish();
				CHECK(traceValid && haltedForSighting, "actual native sighting halt accepts moving actor");
				char expected[512]{};
				std::snprintf(expected, sizeof(expected),
					"[coop-move] stage=sighting-halt actor=4:5 sightingEnemy=1 reason=1 noAP=1 "
					"animation=331 ap=67 path=2/5 grid=15775 final=15455 next=15775 "
					"world=%.6f,%.6f destination=955,985\n",
					double(actor.position().worldX()), double(actor.position().worldY()));
				CHECK(traceEnabled ? output == expected : output.empty(),
					"native sighting trace is default off and reports the exact actual halt only when enabled");
			}
			CHECK(sightingRandom == std::make_pair(campaignRandom->checkpoint(), campaignRandom->consumptionEpoch()),
				"enabled and disabled native sighting observations preserve RNG state and draw epoch");
			CHECK(beforeSighting == std::make_tuple(actor.position().gridNo(), actor.position().worldX(), actor.position().worldY(),
				actor.pathing().pathIndex(), actor.pathing().pathSize(), actor.pathing().finalDestinationGrid(), actor.actionPoints().current()) &&
				actor.movement().outOfActionPoints() && actor.movement().stopReason() == REASON_STOPPED_SIGHT &&
				actor.animationPlayback().state() == WALKING_WEAPON_RDY,
				"combat sighting deliberately pauses off-center with route and AP retained");
			raisedTrace("sighting-halt", alternate);
			const auto halted = state();
			for (unsigned tick = 0; tick < 8; ++tick) raisedTick();
			CHECK(state() == halted, "native overhead preserves the deliberate sighting halt");
			CHECK(!live.canBeginMoveToGrid(id, 15455, WALKING, false) && state() == halted,
				"fresh movement cannot replace an unconsumed sighting-paused route");
			const INT16 stoppedAP = actor.actionPoints().current();
			BeginSimulationCommandFrameBudget(++frame, 1);
			CHECK(TryDispatchSimulationCommandNow(SimulationCommand{StopMovementCommand{
				id, SimulationCommandSource::NetworkPeer, TacticalCommandAuthorityPolicy::DedicatedCoop}}).status == SimulationCommandDispatchStatus::Applied,
				"ordinary dedicated Stop command applies to an off-center sighting halt");
			CHECK(actor.position().gridNo() == observedGrid && actor.position().worldX() == sightX && actor.position().worldY() == sightY &&
				actor.pathing().finalDestinationGrid() == observedGrid && actor.pathing().pathIndex() == 0 && actor.pathing().pathSize() == 0 &&
				!actor.movement().delayed() && !actor.pendingAction().active() && !actor.animationIntent().hasPendingAnimation() &&
				actor.animationPlayback().state() == AIM_RIFLE_STAND && actor.actionPoints().current() == stoppedAP,
				"native Stop centers the current tile, cancels route and preserves rifle readiness without AP cost");
			raisedTrace("stop", alternate);
			CHECK(dispatchTo(15455, WALKING).status == SimulationCommandDispatchStatus::Applied &&
				actor.animationPlayback().state() == WALKING_WEAPON_RDY && !actor.movement().outOfActionPoints(),
				"ordinary fresh route resumes raised-rifle locomotion after explicit Stop");
			for (unsigned tick = 0; tick < 512; ++tick)
			{
				raisedTick();
				if (actor.animationPlayback().state() != WALKING_WEAPON_RDY || actor.movement().outOfActionPoints()) break;
			}
		}
		INT16 finishX = 0, finishY = 0;
		ConvertGridNoToCenterCellXY(15455, &finishX, &finishY);
		raisedTrace("finish", alternate);
		CHECK(actor.position().gridNo() == 15455 && actor.position().worldX() == finishX && actor.position().worldY() == finishY &&
			actor.pathing().finalDestinationGrid() == 15455 && actor.pathing().pathIndex() == 0 && actor.pathing().pathSize() == 0 &&
			actor.animationPlayback().state() == AIM_RIFLE_STAND && actor.actionPoints().current() == initialAP - 25,
			"both pathfinders complete a real five-tile raised route with exact native AP debit, including Stop recovery");
		CHECK(TacticalActorWorldPlacement::removeFromGrid(actor), "raised-rifle fixture releases native world placement");
	}
	gGameSettings.fOptions[TOPTION_ALT_PATHFINDING] = FALSE;

	CHECK(RemoveJa2ActiveTacticalActor(id), "arrival actor leaves overhead roster");
	MemFree(gubGridNoMarkers); gubGridNoMarkers = nullptr;
	actor.animationCache().reset(); gAnimSurfaceDatabase[0] = savedSurface;
	ShutDownPathAI(); MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr; ReleaseWorldTileMap();
	ShutdownFileManager(); vfs::CVirtualFileSystem::shutdownVFS();
	std::filesystem::remove_all(fixtureRoot);
	std::printf("native co-op move preflight: %d failures\n", failures);
	return failures ? 1 : 0;
}
