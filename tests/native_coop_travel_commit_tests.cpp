// Engine-linked single-leg departure and native scheduled arrival; no client
// command, fake clock callback, installed campaign or alternate movement engine.
#include "DedicatedCoopTravel.h"
#include "DedicatedCoopCampaignGroups.h"
#include "GameContext.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "Strategic Movement.h"
#include "Strategic Path Types.h"
#include "StrategicGroupHost.h"
#include "Campaign Types.h"
#include "Map Screen Interface.h"
#include "Map Screen Interface Map.h"
#include "PreBattle Interface.h"
#include "Assignments.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "strategic.h"
#include "Overhead.h"
#include "screenids.h"
#include "gameloop.h"
#include "random.h"
#include <Engine/Core/SimulationRandom.h>
#include "LaptopSave.h"
#include <vfs/Core/vfs_init.h>
#include <cstdio>
#include <cstdlib>
#include <new>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }
extern BOOLEAN fInMapMode, gfProcessingGameEvents, gfRandomizingPatrolGroup;
namespace { int failures = 0; long failAfter = -1; }
void* operator new(std::size_t size)
{
	if (failAfter == 0) { failAfter = -1; throw std::bad_alloc(); }
	if (failAfter > 0) --failAfter;
	if (void* p = std::malloc(size ? size : 1)) return p;
	throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{ try { return ::operator new(n); } catch (...) { return nullptr; } }
void operator delete(void* p, const std::nothrow_t&) noexcept { ::operator delete(p); }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

int main()
{
	using Code = DedicatedCoopTravelStartCode;
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	CHECK(InstallGameSimulationRandom(20260907) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native fixture starts");
	if (failures) return 1;
	// Minimal test-owned campaign script, mounted read-only. The real native
	// arrival must still invoke Lua sector-liberation rules; do not stub them out.
	vfs_init::VfsConfig config;
	auto* profile = new vfs_init::Profile();
	profile->m_name = L"native-coop-travel-fixture";
	profile->m_root = vfs::Path(NATIVE_COOP_TRAVEL_FIXTURE_ROOT);
	profile->m_writable = false;
	auto* location = new vfs_init::Location();
	location->m_type = L"DIRECTORY";
	profile->addLocation(location, true); config.addProfile(profile, true);
	CHECK(vfs_init::initVirtualFileSystem(config, false), "test-owned Lua content mounted");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	[[maybe_unused]] auto screen = OverrideCurrentScreen(MAP_SCREEN);
	fInMapMode = TRUE; gTacticalStatus.fDidGameJustStart = FALSE; gTacticalStatus.fEnemyInSector = FALSE;
	gbPlayerNum = OUR_TEAM; gfAtLeastOneMercWasHired = TRUE;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0}; gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	gGameOptions.fNewTraitSystem = FALSE; gGameExternalOptions.fDisease = FALSE; gGameExternalOptions.iStrengthToLiftHalfKilo = 2;
	RestoreJa2TacticalTurnState(0, OUR_TEAM, 0); RestoreJa2CampaignClock(111600, 111600);
	SetSelectedDestChar(-1); UnLockPauseState(); PauseGame();
	GROUP group{}; PLAYERGROUP members[2]{};
	group.ubGroupID = 20; group.usGroupTeam = OUR_TEAM; group.ubGroupSize = 2;
	group.ubSectorX = 9; group.ubSectorY = 1; group.ubTransportationMask = FOOT;
	CHECK(gpGroupList == nullptr, "movement fixture is isolated");
	gpGroupList = &group; CHECK(AdoptJa2StrategicGroup(group), "native group adopted");
	const auto identity = GetJa2StrategicGroupId(20);
	for (unsigned i = 0; i < 2; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)}; actor.identity().incarnation() = 101 + i;
		actor.identity().profile() = NO_PROFILE; actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		actor.deployment().groupId() = 20; actor.deployment().setSector(9, 1, 0); actor.assignment().current() = 0;
		actor.vitals().health() = actor.vitals().maximumHealth() = 100;
		actor.vitals().breath() = actor.vitals().maximumBreath() = 100; actor.statistics().strength() = 100;
		actor.deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO;
		CHECK(AdoptJa2TacticalEntity(actor), "native actor adopted"); members[i].actor = GetJa2TacticalEntityId(actor);
	}
	members[0].next = &members[1]; group.pPlayerList = &members[0];
	auto& first = *repository.resolve(0); auto& second = *repository.resolve(1);
	for (auto& sector : SectorInfo) for (auto& terrain : sector.ubTraversability) terrain = GROUNDBARRIER;
	SectorInfo[SECTOR(9, 1)].ubTraversability[1] = ROAD; SectorInfo[SECTOR(10, 1)].ubTraversability[3] = ROAD;
	SectorInfo[SECTOR(10, 1)].ubTraversability[1] = ROAD; SectorInfo[SECTOR(11, 1)].ubTraversability[3] = ROAD;
	auto& queue = GetJa2CampaignEventQueue();
	CHECK(queue.empty(), "native event queue initially empty");
	const auto sentinel = queue.schedule({180000, 27, 0, ONETIME_EVENT, EVENT_GROUP_ARRIVAL, 0});
	CHECK(sentinel, "unrelated native event sentinel");
	const auto nextIdentity = queue.nextIdentity();
	const auto rng = GetGameSimulationRandomSource()->checkpoint();
	const auto unchanged = [&] {
		return queue.validate() && queue.size() == 1 && queue.head() == sentinel.event && queue.nextIdentity() == nextIdentity &&
			!group.pWaypoints && !group.fBetweenSectors && !group.uiArrivalTime && !group.uiTraverseTime && !group.ubNextX && !group.ubNextY &&
			first.strategicPath().empty() && second.strategicPath().empty() && !first.deployment().isBetweenSectors() &&
			!second.deployment().isBetweenSectors() && first.deployment().strategicInsertionCode() == INSERTION_CODE_GRIDNO &&
			second.deployment().strategicInsertionCode() == INSERTION_CODE_GRIDNO && GamePaused() && GetWorldTotalSeconds() == 111600 &&
			GetGameSimulationRandomSource()->checkpoint() == rng;
	};
	for (unsigned fault = 0; fault < 9; ++fault)
	{
		if (fault == 0) gfProcessingGameEvents = TRUE;
		if (fault == 1) gfPreBattleInterfaceActive = TRUE;
		if (fault == 2) gfTacticalTraversal = TRUE;
		if (fault == 3) gfRandomizingPatrolGroup = TRUE;
		if (fault == 4) second.roster().inSector() = TRUE;
		if (fault == 5) second.vitals().health() = 0;
		if (fault == 6) ++members[1].actor.incarnation;
		if (fault == 7) NotifyJa2TacticalWorldLoaded(1);
		if (fault == 8) group.next = &group;
		CHECK(StartDedicatedCoopTravel(identity, 10, 1).code != Code::Started && unchanged(), "native context/identity/health failure leaves departure untouched");
		gfProcessingGameEvents = gfPreBattleInterfaceActive = gfTacticalTraversal = gfRandomizingPatrolGroup = FALSE;
		second.roster().inSector() = FALSE; second.vitals().health() = 100; members[1].actor = GetJa2TacticalEntityId(second);
		NotifyJa2TacticalWorldUnloaded(); group.next = nullptr;
	}
	CHECK(StartDedicatedCoopTravel(identity, 11, 1).code == Code::AdjacentSectorRequired && unchanged(), "multi-leg route is not silently truncated");
	GROUP crossing{}; crossing.ubGroupID = 21; crossing.usGroupTeam = ENEMY_TEAM; crossing.ubSectorX = 10; crossing.ubSectorY = 1;
	crossing.ubNextX = 9; crossing.ubNextY = 1; crossing.uiArrivalTime = GetWorldTotalMin() + 5;
	group.next = &crossing;
	CHECK(StartDedicatedCoopTravel(identity, 10, 1).code == Code::NativeInteractionRequired && unchanged(), "crossing cannot change enemy events or consume RNG through the simple commit");
	group.next = nullptr;
	const auto conflict = queue.schedule({160000, 20, 0, ONETIME_EVENT, EVENT_GROUP_ABOUT_TO_ARRIVE, 0});
	CHECK(StartDedicatedCoopTravel(identity, 10, 1).code == Code::EventConflict && queue.size() == 2 && !group.pWaypoints,
		"stale arrival/warning is not erased or duplicated by a new departure");
	queue.erase(conflict.event);
	// A tiny live queue must reject the pair before any route/group mutation.
	CampaignEventQueue tiny(1); queue.swap(tiny);
	const auto tinyIdentity = queue.nextIdentity();
	CHECK(StartDedicatedCoopTravel(identity, 10, 1).code == Code::EventQueueFailure && queue.empty() && queue.nextIdentity() == tinyIdentity &&
		!group.pWaypoints && first.strategicPath().empty() && second.strategicPath().empty(), "insufficient batch capacity rolls back prepared native paths");
	queue.swap(tiny);
	for (long allocation = 0; allocation < 2; ++allocation)
	{
		const auto beforeIdentity = queue.nextIdentity();
		failAfter = allocation; const auto result = StartDedicatedCoopTravel(identity, 10, 1); failAfter = -1;
		CHECK(result.code == Code::EventQueueFailure && queue.head() == sentinel.event && queue.size() == 1 && queue.nextIdentity() == beforeIdentity &&
			!group.pWaypoints && !group.fBetweenSectors && !group.uiArrivalTime && first.strategicPath().empty() && second.strategicPath().empty() &&
			GetWorldTotalSeconds() == 111600 && GetGameSimulationRandomSource()->checkpoint() == rng, "each native event allocation failure rolls back the complete departure");
	}
	const auto start = StartDedicatedCoopTravel(identity, 10, 1);
	CHECK(start.code == Code::Started && start.arrivalMinutes == 1949 && group.fBetweenSectors && group.ubSectorX == 9 && group.ubSectorY == 1 &&
		group.ubNextX == 10 && group.ubNextY == 1 && group.uiTraverseTime == 89 && group.uiArrivalTime == 1949 &&
		group.pWaypoints && group.pWaypoints->x == 10 && group.pWaypoints->y == 1 && !group.pWaypoints->next,
		"native single-leg departure schedules travel, without teleporting the group");
	for (auto* actor : {&first, &second})
	{
		const auto* path = actor->strategicPath().head();
		CHECK(actor->deployment().isBetweenSectors() && actor->deployment().sectorX() == 9 && !actor->roster().inSector() &&
			actor->deployment().strategicInsertionCode() == 0 && path && path->uiSectorId == 27 && path->uiEta == 1860 && !path->pPrev &&
			path->pNext && path->pNext->pPrev == path && path->pNext->uiSectorId == 28 && path->pNext->uiEta == 1949 && !path->pNext->pNext,
			"every exact member owns a separate native two-node route and matching transit state");
	}
	CHECK(first.strategicPath().head() != second.strategicPath().head() && GamePaused() && GetWorldTotalSeconds() == 111600 &&
		GetGameSimulationRandomSource()->checkpoint() == rng && queue.size() == 3 && queue.head()->callbackId == EVENT_GROUP_ABOUT_TO_ARRIVE &&
		queue.head()->scheduledSeconds == (1949 - 30) * 60 && queue.head()->next->callbackId == EVENT_GROUP_ARRIVAL &&
		queue.head()->next->scheduledSeconds == 1949 * 60 && queue.head()->next->next == sentinel.event, "commit preserves clocks/RNG and existing stable events");
	CoopSession::CoopCampaignGroups observed;
	CHECK(!CaptureDedicatedCoopCampaignGroups(observed) && observed.groupCount == 1 && observed.groups[0].betweenSectors &&
		observed.groups[0].arrivalMinutes == 1949 && observed.groups[0].destinationX == 10, "existing live observation sees committed native travel");
	CHECK(StartDedicatedCoopTravel(identity, 10, 1).code == Code::PreflightRejected && queue.size() == 3, "repeated start never reposts movement");
	if (failures) return 1;
	// Use the production fixed-step clock and event callback, not direct arrival
	// state changes. This test-owned-content case contains no NPC/hostile encounter.
	const auto volunteers = LaptopSaveInfo.dMilitiaVolunteerPool;
	CHECK(TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_60MINS), "native clock can advance this established trip");
	CampaignClockScheduler scheduler;
	for (unsigned frame = 0; frame < 300 && group.fBetweenSectors; ++frame)
		(void)AdvanceClockFromFixedStep(scheduler, 10000);
	CHECK(!group.fBetweenSectors && group.ubSectorX == 10 && group.ubSectorY == 1 && !group.pWaypoints && !group.uiArrivalTime &&
		first.deployment().sectorX() == 10 && second.deployment().sectorX() == 10 && !first.deployment().isBetweenSectors() &&
		!second.deployment().isBetweenSectors() && first.strategicPath().empty() && second.strategicPath().empty(),
		"native scheduled arrival moves both members and clears the completed route");
	CHECK(!IsJa2TacticalWorldLoaded() && queue.validate(), "peaceful native arrival keeps a coherent worldless campaign");
	CHECK(LaptopSaveInfo.dMilitiaVolunteerPool == volunteers + 7 && SectorInfo[SECTOR(10, 1)].fSurfaceWasEverPlayerControlled,
		"native arrival executes the test campaign's Lua liberation effect exactly once");
	// A second, five-minute town leg must post no 30-minute warning. Merely
	// submitting it must not advance time or run the next liberation hook.
	PauseGame();
	SectorInfo[SECTOR(10, 1)].ubTraversability[1] = TOWN;
	SectorInfo[SECTOR(11, 1)].ubTraversability[3] = TOWN;
	const auto shortStartSeconds = GetWorldTotalSeconds();
	const auto eventsBeforeShortTrip = queue.size();
	const auto shortTrip = StartDedicatedCoopTravel(identity, 11, 1);
	CHECK(shortTrip.code == Code::Started && shortTrip.arrivalMinutes == shortStartSeconds / 60 + 5 && group.uiTraverseTime == 5 &&
		queue.size() == eventsBeforeShortTrip + 1 && GetWorldTotalSeconds() == shortStartSeconds && GamePaused() &&
		LaptopSaveInfo.dMilitiaVolunteerPool == volunteers + 7, "short trip posts one arrival without a warning or immediate scripted effects");
	for (const auto* event = queue.head(); event; event = event->next)
		CHECK(event->parameter != identity.slot || event->callbackId != EVENT_GROUP_ABOUT_TO_ARRIVE, "no underflowed/past warning for a five-minute leg");
	RemovePGroupWaypoints(&group); first.strategicPath().reset(); second.strategicPath().reset();
	gpGroupList = nullptr; ResetJa2StrategicGroupDirectory(); queue.clear();
	return failures ? 1 : 0;
}
