// Engine-linked exit-grid traversal: read-only planning against a loaded world's
// native exit-grid table, then worldless group relocation with the stock
// exit-grid insertion values. No client command, walk-off animation or dialog.
#include "DedicatedCoopExitGrid.h"
#include "GameContext.h"
#include "Strategic Movement.h"
#include "StrategicGroupHost.h"
#include "Exit Grids.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Isometric Utils.h"
#include "WorldDat.h"
#include "worlddef.h"
#include "strategic.h"
#include "Overhead.h"
#include "screenids.h"
#include "gameloop.h"
#include "random.h"
#include "World Tile Map.h"
#include "strategicmap.h"
#include "Soldier Profile Constants.h"
#include <Engine/Core/SimulationRandom.h>
#include <cstdio>
#include <cstdlib>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }
extern BOOLEAN gfTacticalTraversal;
namespace { int failures = 0; }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

int main()
{
	using Code = DedicatedCoopExitGridCode;
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	CHECK(InstallGameSimulationRandom(20261125) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native fixture starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	gbPlayerNum = OUR_TEAM;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0}; gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	RestoreJa2TacticalTurnState(0, OUR_TEAM, 0);
	CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "logical map exists");
	NotifyJa2TacticalWorldLoaded(1);
	SetJa2TacticalWorldSector(10, 1, 0);
	[[maybe_unused]] auto screen = OverrideCurrentScreen(GAME_SCREEN);
	GROUP group{}; PLAYERGROUP members[2]{};
	group.ubGroupID = 20; group.usGroupTeam = OUR_TEAM; group.ubGroupSize = 2;
	group.ubSectorX = 10; group.ubSectorY = 1; group.ubSectorZ = 0; group.ubTransportationMask = FOOT;
	CHECK(gpGroupList == nullptr, "movement fixture is isolated");
	gpGroupList = &group; CHECK(AdoptJa2StrategicGroup(group), "native group adopted");
	const auto identity = GetJa2StrategicGroupId(20);
	constexpr INT32 stairs = 12010, otherGrid = 12011, landing = 12722;
	for (unsigned i = 0; i < 2; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)}; actor.identity().incarnation() = 201 + i;
		actor.identity().profile() = NO_PROFILE; actor.roster().active() = TRUE; actor.roster().inSector() = TRUE;
		actor.roster().team() = OUR_TEAM; actor.deployment().groupId() = 20; actor.deployment().setSector(10, 1, 0);
		actor.assignment().current() = 0;
		actor.vitals().health() = actor.vitals().maximumHealth() = 100;
		actor.position().gridNo() = i == 0 ? stairs : stairs + 200;
		actor.deployment().strategicInsertionCode() = INSERTION_CODE_NORTH;
		CHECK(AdoptJa2TacticalEntity(actor), "native actor adopted"); members[i].actor = GetJa2TacticalEntityId(actor);
	}
	members[0].next = &members[1]; group.pPlayerList = &members[0];
	auto& first = *repository.resolve(0); auto& second = *repository.resolve(1);

	DedicatedCoopExitGridPlan plan, untouched;
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, plan) == Code::NotOnExitGrid && SameDedicatedCoopExitGridPlan(plan, untouched),
		"no exit grid under any member is refused without output");
	EXITGRID basement; basement.ubGotoSectorX = 10; basement.ubGotoSectorY = 1; basement.ubGotoSectorZ = 1; basement.usGridNo = landing;
	AddExitGridToWorld(stairs, &basement);
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, plan) == Code::Planned && plan.group == identity &&
		plan.sourceGrid == stairs && plan.destinationGrid == landing && plan.x == 10 && plan.y == 1 && plan.z == 1 &&
		plan.originX == 10 && plan.originY == 1 && plan.originZ == 0 && plan.memberCount == 2 &&
		plan.members[0] == members[0].actor && plan.members[1] == members[1].actor,
		"one member on a native exit grid plans the whole group into the exit grid's destination");
	EXITGRID tunnel = basement; tunnel.ubGotoSectorZ = 2;
	AddExitGridToWorld(second.position().gridNo(), &tunnel);
	DedicatedCoopExitGridPlan ambiguous;
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, ambiguous) == Code::AmbiguousExitGrid,
		"members on exit grids to different destinations are refused");
	second.position().gridNo() = otherGrid;
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, ambiguous) == Code::Planned, "a single destination plans again");
	second.vitals().health() = 1;
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, ambiguous) == Code::MemberNotReady,
		"an unconscious member must be handled before the whole group leaves");
	second.vitals().health() = 100;
	EXITGRID same = basement; same.ubGotoSectorZ = 0;
	AddExitGridToWorld(stairs, &same);
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, ambiguous) == Code::InvalidDestination, "an exit grid back to this sector is refused");
	AddExitGridToWorld(stairs, &basement);
	gfTacticalTraversal = TRUE;
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, ambiguous) == Code::WorldUnavailable, "native traversal in progress is refused");
	gfTacticalTraversal = FALSE;
	group.fVehicle = TRUE;
	CHECK(PlanDedicatedCoopExitGridTraversal(identity, ambiguous) == Code::UnsupportedGroup, "vehicle groups are refused");
	group.fVehicle = FALSE;

	// Relocation is worldless only: a live world would be torn down by the
	// native SetGroupSectorValue unload check outside the co-op drain path.
	StrategicGroupId moved{};
	CHECK(ApplyDedicatedCoopExitGridRelocation(plan, moved) == Code::WorldUnavailable && !moved.valid() &&
		group.ubSectorZ == 0 && first.deployment().strategicInsertionCode() == INSERTION_CODE_NORTH,
		"relocation refuses a loaded world without mutation");
	NotifyJa2TacticalWorldUnloaded();
	for (auto* actor : {&first, &second}) actor->roster().inSector() = FALSE;
	auto changed = plan; changed.memberCount = 1;
	CHECK(ApplyDedicatedCoopExitGridRelocation(changed, moved) == Code::Changed && group.ubSectorZ == 0,
		"a group whose membership changed since planning is not relocated");
	auto elsewhere = plan; elsewhere.originX = 9;
	CHECK(ApplyDedicatedCoopExitGridRelocation(elsewhere, moved) == Code::Changed && group.ubSectorZ == 0,
		"a group that already left its origin is not relocated");
	CHECK(ApplyDedicatedCoopExitGridRelocation(plan, moved) == Code::Planned && moved == identity &&
		group.ubSectorX == 10 && group.ubSectorY == 1 && group.ubSectorZ == 1 && !group.fBetweenSectors,
		"the planned group moves to the exit grid's destination sector");
	for (auto* actor : {&first, &second})
		CHECK(actor->deployment().sectorZ() == 1 && actor->deployment().strategicInsertionCode() == INSERTION_CODE_GRIDNO &&
			actor->deployment().strategicInsertionData() == landing && actor->deployment().useExitGridForReentryDirection(),
			"every member gets the stock exit-grid insertion at the destination grid");
	CHECK(ApplyDedicatedCoopExitGridRelocation(plan, moved) == Code::Changed && group.ubSectorZ == 1,
		"the same plan cannot be applied twice");

	TrashExitGridTable();
	gpGroupList = nullptr; ResetJa2StrategicGroupDirectory();
	ReleaseWorldTileMap();
	std::printf("native co-op exit grid: %s\n", failures ? "FAIL" : "PASS");
	return failures ? 1 : 0;
}
