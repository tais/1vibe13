// Real native pathing, eligibility, directories and movement costs; no mounted
// campaign, GUI selection emulation as authority, network commands or fake route.
#include "DedicatedCoopTravel.h"
#include "GameContext.h"
#include "CampaignClockAdapter.h"
#include "Game Clock.h"
#include "Game Event Hook.h"
#include "GameSettings.h"
#include "StrategicGroupHost.h"
#include "Strategic Movement.h"
#include "Strategic Pathing.h"
#include "StrategicPathQuery.h"
#include "Strategic Path Types.h"
#include "Map Screen Interface.h"
#include "Map Screen Interface Map.h"
#include "mapscreen.h"
#include "Campaign Types.h"
#include "Assignments.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Soldier Profile.h"
#include "Overhead.h"
#include "Items.h"
#include "screenids.h"
#include "input.h"
#include "english.h"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <algorithm>
#include <limits>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1);
}
extern CHAR16 gsCustomErrorString[128];
extern BOOLEAN fInMapMode;
extern BOOLEAN fSelectedListOfMercsForMapScreen[CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS];
extern BOOLEAN gfPlotToAvoidPlayerInfuencedSectors;
extern UINT16 gusMapPathingData[256], gusPathDataSize;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
using Code = DedicatedCoopTravelPlanCode;
bool Same(const DedicatedCoopTravelPlan& a, const DedicatedCoopTravelPlan& b)
{
	if (a.group != b.group || a.observedWorldSeconds != b.observedWorldSeconds || a.totalMinutes != b.totalMinutes ||
		a.sectorCount != b.sectorCount || a.memberCount != b.memberCount || a.members != b.members) return false;
	for (std::size_t i = 0; i < a.sectors.size(); ++i)
		if (a.sectors[i].x != b.sectors[i].x || a.sectors[i].y != b.sectors[i].y || a.sectors[i].minutesFromPrevious != b.sectors[i].minutesFromPrevious) return false;
	return true;
}
void BlockTerrain()
{
	for (auto& sector : SectorInfo)
		for (unsigned direction = 0; direction < 4; ++direction) sector.ubTraversability[direction] = GROUNDBARRIER;
}
void Edge(unsigned x, unsigned y, unsigned direction, UINT8 terrain)
{
	SectorInfo[SECTOR(x, y)].ubTraversability[direction] = terrain;
	if (direction == 0) --y; else if (direction == 1) ++x; else if (direction == 2) ++y; else --x;
	SectorInfo[SECTOR(x, y)].ubTraversability[(direction + 2) % 4] = terrain;
}
void TwoRoutes()
{
	BlockTerrain();
	Edge(9, 1, 1, WATER); Edge(10, 1, 1, WATER);
	Edge(9, 1, 2, ROAD); Edge(9, 2, 1, ROAD); Edge(10, 2, 1, ROAD); Edge(11, 2, 0, ROAD);
}
void Weight(TacticalActor& actor)
{
	Item[1001].usItemClass = IC_MISC; Item[1001].ubWeight = 200;
	auto& item = actor.inventory()[0]; item.initialize(); item.usItem = 1001; item.ubNumberOfObjects = 40;
	item.objectStack.resize(40);
	for (auto& object : item.objectStack) object.data.objectStatus = 100;
}
}
int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native fixture starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	CHECK(gpGroupList == nullptr, "native movement list initially empty");
	if (failures) return 1;
	[[maybe_unused]] auto screen = OverrideCurrentScreen(MAP_SCREEN);
	fInMapMode = TRUE; gTacticalStatus.fDidGameJustStart = FALSE; gTacticalStatus.fEnemyInSector = FALSE;
	RestoreJa2CampaignClock(111600, 111600);
	gbPlayerNum = OUR_TEAM; gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0}; gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{2};
	gGameOptions.fNewTraitSystem = FALSE; gGameExternalOptions.fDisease = FALSE; gGameExternalOptions.iStrengthToLiftHalfKilo = 2;
	GROUP group{}, other{}; PLAYERGROUP members[3]{};
	group.ubGroupID = 20; group.usGroupTeam = OUR_TEAM; group.ubGroupSize = 2; group.ubSectorX = 9; group.ubSectorY = 1; group.ubTransportationMask = FOOT;
	other = group; other.ubGroupID = 21; other.ubGroupSize = 1; group.next = &other; gpGroupList = &group;
	CHECK(AdoptJa2StrategicGroup(group) && AdoptJa2StrategicGroup(other), "native groups adopted");
	const auto identity = GetJa2StrategicGroupId(20);
	for (unsigned i = 0; i < 3; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)}; actor.identity().incarnation() = 101 + i; actor.identity().profile() = static_cast<UINT8>(238 + i);
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		actor.deployment().groupId() = i < 2 ? 20 : 21; actor.deployment().sectorX() = 9; actor.deployment().sectorY() = 1;
		actor.assignment().current() = 0; actor.vitals().health() = actor.vitals().maximumHealth() = 80;
		actor.vitals().breath() = actor.vitals().maximumBreath() = 100; actor.statistics().strength() = 80;
		CHECK(AdoptJa2TacticalEntity(actor), "native actor adopted"); members[i].actor = GetJa2TacticalEntityId(actor);
		gCharactersList[i].fValid = TRUE; gCharactersList[i].usSolID = SoldierID{static_cast<UINT16>(i)};
	}
	members[0].next = &members[1]; group.pPlayerList = &members[0]; other.pPlayerList = &members[2];
	auto& first = *repository.resolve(0); auto& second = *repository.resolve(1); auto& outsider = *repository.resolve(2);
	giMAXIMUM_NUMBER_OF_PLAYER_SLOTS = 3; fSelectedListOfMercsForMapScreen[2] = TRUE; outsider.assignment().current() = ASSIGNMENT_POW;
	TwoRoutes();
	// Seed the legacy cache with another, heavily encumbered group and leave an
	// unrelated POW selected in the native map list. Neither may affect this plan.
	Weight(outsider); SetSelectedDestChar(-1);
	CHECK(GetSectorMvtTimeForGroup(SECTOR(9, 2), 1, &other) > 89, "other group's legacy weight cache seeded");
	SetSelectedDestChar(0); gSquadEncumbranceCheckNecessary = false;
	std::fill(std::begin(gusMapPathingData), std::end(gusMapPathingData), 0xbeef); gusPathDataSize = 17;
	gfKeyState[SHIFT] = TRUE; gfPlotToAvoidPlayerInfuencedSectors = TRUE; fMapPanelDirty = FALSE;
	std::wcscpy(gsCustomErrorString, L"unchanged UI error");
	CHECK(AddStrategicEvent(EVENT_GROUP_ARRIVAL, GetWorldTotalMin() + 700, 21), "unrelated native event retained as sentinel");
	const auto events = GetAllStrategicEventsOfType(EVENT_GROUP_ARRIVAL);
	DedicatedCoopTravelPlan plan;
	CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::Planned && plan.group == identity && plan.memberCount == 2 &&
		plan.members[0] == members[0].actor && plan.members[1] == members[1].actor && plan.sectorCount == 5 && plan.totalMinutes == 356 &&
		plan.sectors[1].x == 9 && plan.sectors[1].y == 2 && plan.sectors[4].x == 11 && plan.sectors[4].y == 1,
		"exact native group gets fastest terrain route, independent of Shift, another squad cache and selected POW");
	CHECK(!fMapPanelDirty && !gSquadEncumbranceCheckNecessary && GetSelectedDestChar() == 0 && gfKeyState[SHIFT] &&
		gfPlotToAvoidPlayerInfuencedSectors && gusPathDataSize == 17 &&
		std::all_of(std::begin(gusMapPathingData), std::end(gusMapPathingData), [](UINT16 x) { return x == 0xbeef; }) &&
		std::wcscmp(gsCustomErrorString, L"unchanged UI error") == 0 && GetWorldTotalSeconds() == 111600 &&
		!group.fBetweenSectors && !group.pWaypoints && first.strategicPath().empty() && second.strategicPath().empty() &&
		GetAllStrategicEventsOfType(EVENT_GROUP_ARRIVAL) == events, "preview leaves UI, clock, native events, actors and routes untouched");
	INT8 nativeError = 0;
	CHECK(!CanEntireMovementGroupMercIsInMove(&first, &nativeError) && nativeError == 5,
		"legacy UI still checks the selected POW; server planner does not borrow that selection rule");
	gfPlotToAvoidPlayerInfuencedSectors = FALSE; SetSelectedDestChar(-1);
	CHECK(FindStratPath(1 * 18 + 9, 1 * 18 + 11, 20, FALSE) == 2 && gusMapPathingData[0] == EAST && gusMapPathingData[1] == EAST,
		"legacy Shift plotting still chooses its shorter, slower route");
	gfKeyState[SHIFT] = FALSE;
	CHECK(FindStratPath(1 * 18 + 9, 1 * 18 + 11, 20, FALSE) == 4, "legacy normal path still chooses native fastest route");
	const auto successful = plan;
	for (unsigned fault = 0; fault < 24; ++fault)
	{
		WAYPOINT waypoint{10, 1, nullptr};
		path existingPath{};
		Code expected = Code::GroupUnavailable; StrategicGroupId requested = identity;
		if (fault == 0) { ++requested.incarnation; }
		if (fault == 1) { other.next = &group; }
		if (fault == 2) { ++members[1].actor.incarnation; }
		if (fault == 3) { second.deployment().sectorY() = 2; }
		if (fault == 4) { group.fVehicle = TRUE; expected = Code::UnsupportedGroup; }
		if (fault == 5) { group.pWaypoints = &waypoint; expected = Code::Busy; }
		if (fault == 6) { group.fBetweenSectors = TRUE; group.ubNextX = 10; group.ubNextY = 1; expected = Code::Busy; }
		if (fault == 7) { second.assignment().current() = IN_TRANSIT; expected = Code::NativeRejected; }
		if (fault == 8) { second.vitals().health() = 0; expected = Code::NativeRejected; }
		if (fault == 9) { second.assignment().fallAsleep(); second.vitals().maximumBreath() = BREATHMAX_ABSOLUTE_MINIMUM; expected = Code::NativeRejected; }
		if (fault == 10) { second.vitals().maximumBreath() = BREATHMAX_GOTTA_STOP_MOVING - 1; expected = Code::NativeRejected; }
		if (fault == 11) { BlockTerrain(); expected = Code::NoRoute; }
		if (fault == 12) { group.ubTransportationMask = AIR; expected = Code::UnsupportedGroup; }
		if (fault == 13) { second.assignment().current() = ASSIGNMENT_POW; expected = Code::NativeRejected; }
		if (fault == 14) { second.vitals().health() = OKLIFE - 1; expected = Code::NativeRejected; }
		if (fault == 15) { second.assignment().current() = DOCTOR; expected = Code::NativeRejected; }
		if (fault == 16) { second.strategicPath().rebind(&existingPath); expected = Code::Busy; }
		if (fault == 17) { group.ubGroupSize = 1; }
		if (fault == 18) { waypoint.next = &waypoint; group.pWaypoints = &waypoint; }
		if (fault == 19) { group.ubSectorZ = 1; expected = Code::UnsupportedGroup; }
		if (fault == 20) { second.deployment().beginStrategicTransit(); }
		if (fault == 21) { members[1].next = &members[0]; }
		if (fault == 22) { second.assignment().current() = VEHICLE; expected = Code::UnsupportedGroup; }
		if (fault == 23) { second.assignment().current() = ASSIGNMENT_MINIEVENT; expected = Code::NativeRejected; }
		const auto result = PlanDedicatedCoopTravel(requested, 11, 1, plan);
		CHECK(result.code == expected && Same(plan, successful), "rejected travel preflight preserves previous plan");
		if (expected == Code::NativeRejected) CHECK(result.blockedActor == members[1].actor, "native refusal identifies the exact actual group member");
		CHECK(!second.collapseState().fatigueCollapsed() && std::wcscmp(gsCustomErrorString, L"unchanged UI error") == 0,
			"even exhausted/asleep/dead member checks never mutate collapse state or UI error text");
		other.next = nullptr; members[1].actor = GetJa2TacticalEntityId(second); second.deployment().sectorY() = 1;
		members[1].next = nullptr; group.ubGroupSize = 2; group.ubSectorZ = 0;
		second.deployment().completeStrategicTransit(); second.strategicPath().release();
		group.fVehicle = FALSE; group.pWaypoints = nullptr; group.fBetweenSectors = FALSE; group.ubTransportationMask = FOOT;
		second.assignment().current() = 0; second.assignment().wakeUp(); second.vitals().health() = 80; second.vitals().maximumBreath() = 100;
		TwoRoutes();
	}
	const auto moveOrigin = [&](UINT8 x, UINT8 y) {
		group.ubSectorX = first.deployment().sectorX() = second.deployment().sectorX() = x;
		group.ubSectorY = first.deployment().sectorY() = second.deployment().sectorY() = y;
	};
	SectorInfo[SECTOR(9, 1)].ubNumTroops = 1;
	const auto hostile = PlanDedicatedCoopTravel(identity, 11, 1, plan);
	CHECK(hostile.code == Code::NativeRejected && hostile.nativeError == 2 && Same(plan, successful), "native hostile origin rejects travel");
	SectorInfo[SECTOR(9, 1)].ubNumTroops = 0;
	moveOrigin(5, 3); second.identity().profile() = MARIA;
	const auto quest = PlanDedicatedCoopTravel(identity, 11, 1, plan);
	CHECK(quest.code == Code::NativeRejected && quest.blockedActor == members[1].actor && quest.nativeError == -99 && Same(plan, successful),
		"native Maria quest restriction rejects the actual group member without UI text mutation");
	second.identity().profile() = 239; moveOrigin(12, 12);
	const auto museum = PlanDedicatedCoopTravel(identity, 11, 1, plan);
	CHECK(museum.code == Code::NativeContextUnavailable && museum.nativeError == STRATEGIC_MOVE_REQUIRES_TACTICAL_CONTEXT && Same(plan, successful),
		"room-dependent museum rule requires exact native map context instead of reading an absent room table");
	moveOrigin(9, 1);
	RestoreJa2CampaignClock(std::numeric_limits<UINT32>::max() - 30, std::numeric_limits<UINT32>::max() - 30);
	CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::InvalidRoute && Same(plan, successful), "route cannot overflow native event timestamps");
	RestoreJa2CampaignClock(111600, 111600);
	for (const auto destination : {std::pair<UINT8, UINT8>{0, 1}, {17, 1}, {9, 1}})
		CHECK(PlanDedicatedCoopTravel(identity, destination.first, destination.second, plan).code == Code::InvalidDestination && Same(plan, successful), "invalid or unchanged destination rejected");
	NotifyJa2TacticalWorldLoaded(1);
	CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::HostUnavailable, "loaded tactical world cannot use strategic planner");
	NotifyJa2TacticalWorldUnloaded(); gTacticalStatus.fDidGameJustStart = TRUE;
	CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::HostUnavailable, "startup cannot plan travel");
	gTacticalStatus.fDidGameJustStart = FALSE;
	{
		[[maybe_unused]] auto tactical = OverrideCurrentScreen(GAME_SCREEN);
		CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::HostUnavailable, "native tactical screen cannot plan worldless travel");
	}
	second.assignment().fallAsleep(); second.vitals().maximumBreath() = BREATHMAX_ABSOLUTE_MINIMUM;
	CHECK(!CanCharacterBeAwakenedWithoutSideEffects(&second) && !second.collapseState().fatigueCollapsed(), "pure wake guard refuses without mutation");
	CHECK(!CanCharacterBeAwakened(&second, FALSE) && second.collapseState().fatigueCollapsed(), "legacy wake action keeps its fatigue bookkeeping");
	second.collapseState().clearFatigueCollapse(); second.assignment().wakeUp(); second.vitals().maximumBreath() = 100;
	Weight(first);
	CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::Planned && plan.totalMinutes > successful.totalMinutes, "real carried weight changes this group's route timing");
	first.inventory()[0].initialize();
	gGameOptions.fNewTraitSystem = TRUE; gSkillTraitValues.ubMaxNumberOfTraits = 3; gSkillTraitValues.ubNumberOfMajorTraitsAllowed = 2;
	gSkillTraitValues.ubSVMaxBonusesToTravelSpeed = 3; gSkillTraitValues.ubSVGroupTimeSpentForTravellingFoot = 20;
	first.statistics().skillTrait(0) = second.statistics().skillTrait(0) = SURVIVAL_NT;
	CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::Planned && plan.totalMinutes == 264,
		"real survival traits reduce each road leg to 66 minutes without repeated cache decay");
	const auto skilled = plan;
	CHECK(PlanDedicatedCoopTravel(identity, 11, 1, plan).code == Code::Planned && Same(plan, skilled), "repeated query has identical native costs and route");
	gGameOptions.fNewTraitSystem = FALSE;
	// A forced 255-leg native route traverses the complete playable grid. The
	// old tactical path limit of 30 must not silently truncate strategic plans.
	BlockTerrain();
	for (unsigned y = 1; y <= 16; ++y)
	{
		for (unsigned x = 1; x < 16; ++x) Edge(x, y, 1, ROAD);
		if (y < 16) Edge(y % 2 ? 16 : 1, y, 2, ROAD);
	}
	group.ubSectorX = group.ubSectorY = 1;
	first.deployment().sectorX() = second.deployment().sectorX() = 1;
	CHECK(PlanDedicatedCoopTravel(identity, 1, 16, plan).code == Code::Planned && plan.sectorCount == 256 &&
		plan.totalMinutes == 255 * 89 && plan.sectors[255].x == 1 && plan.sectors[255].y == 16,
		"native route beyond 30 legs reaches exact destination with every sector and duration");
	CHECK(GetWorldTotalSeconds() == 111600 && GetAllStrategicEventsOfType(EVENT_GROUP_ARRIVAL) == events &&
		!group.fBetweenSectors && !group.pWaypoints && first.strategicPath().empty(), "all preflight cases leave strategic simulation unadvanced");
	gpGroupList = nullptr; ResetJa2StrategicGroupDirectory(); DeleteAllStrategicEventsOfType(EVENT_GROUP_ARRIVAL);
	return failures ? 1 : 0;
}
