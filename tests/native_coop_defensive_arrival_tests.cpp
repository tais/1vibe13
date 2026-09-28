// Actual native defensive pre-battle preparation. Installed qualification proves
// the enemy movement callback reaches DeferDedicatedCoopArrival; this isolated
// fixture checks that continuation without an installed map or local UI.
#include "DedicatedCoopArrival.h"
#include "GameContext.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignEventScheduling.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "Strategic Movement.h"
#include "StrategicGroupHost.h"
#include "Campaign Types.h"
#include "strategicmap.h"
#include "strategic.h"
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
#include "Overhead.h"
#include "screenids.h"
#include "gameloop.h"
#include "Dialogue Control.h"
#include "random.h"
#include "LaptopSave.h"
#include "Auto Resolve.h"
#include "Strategic AI.h"
#include "Quests.h"
#include "TacticalActorStateFlags.h"
#include "Tactical Placement GUI.h"
#include <Engine/Core/SimulationRandom.h>
#include <vfs/Core/vfs_init.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }
extern BOOLEAN fInMapMode;

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
bool SamePreparation(const NativePreBattlePreparation& a, const NativePreBattlePreparation& b)
{
	return a.encounterCode == b.encounterCode && a.involvedMercs == b.involvedMercs &&
		a.uninvolvedMercs == b.uninvolvedMercs && a.ambushRadiusModifier == b.ambushRadiusModifier &&
		a.actions.autoResolve == b.actions.autoResolve && a.actions.enterSector == b.actions.enterSector &&
		a.actions.retreat == b.actions.retreat && a.actions.tacticalPlacement == b.actions.tacticalPlacement;
}
}

int main(int argc, char** argv)
{
	using Prepared = DedicatedCoopArrivalPrepareResult;
	using Enter = DedicatedCoopArrivalEnterResult;
	using Retreat = DedicatedCoopArrivalRetreatResult;
	using Deployment = NativePreBattleDeployment;
	const bool invasion = argc == 2 && std::strcmp(argv[1], "--invasion") == 0;
	const bool retreat = argc == 2 && std::strcmp(argv[1], "--retreat") == 0;
	const bool pendingHire = invasion || retreat || (argc == 2 && std::strcmp(argv[1], "--pending-hire") == 0);
	if (argc > 2 || (argc == 2 && !pendingHire)) return 2;
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	CHECK(InstallGameSimulationRandom(20260922) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"actual native fixture starts");
	vfs_init::VfsConfig config;
	auto* profile = new vfs_init::Profile();
	profile->m_name = L"native-coop-defensive-arrival-fixture";
	profile->m_root = vfs::Path(NATIVE_COOP_TRAVEL_FIXTURE_ROOT); profile->m_writable = false;
	auto* location = new vfs_init::Location(); location->m_type = L"DIRECTORY";
	profile->addLocation(location, true); config.addProfile(profile, true);
	CHECK(vfs_init::initVirtualFileSystem(config, false), "test-owned Lua mounted without installed assets");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	[[maybe_unused]] auto screen = OverrideCurrentScreen(MAP_SCREEN);
	fInMapMode = TRUE; gTacticalStatus.fDidGameJustStart = FALSE; gTacticalStatus.fEnemyInSector = FALSE;
	gbPlayerNum = OUR_TEAM; gfAtLeastOneMercWasHired = TRUE;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{static_cast<UINT16>(pendingHire ? 2 : 1)};
	gGameOptions.fNewTraitSystem = FALSE; gGameExternalOptions.fDisease = FALSE;
	gGameExternalOptions.iStrengthToLiftHalfKilo = 2; gGameExternalOptions.gfAllowReinforcements = FALSE;
	RestoreJa2TacticalTurnState(0, OUR_TEAM, 0); RestoreJa2CampaignClock(112500, 112500);
	SetJa2TacticalWorldSector(0, 0, -1); SetPendingNewScreen(NO_PENDING_SCREEN);
	SetSelectedDestChar(-1); UnLockPauseState(); PauseGame(); SetEnemyEncounterCode(NO_ENCOUNTER_CODE);
	GROUP defenders{}, attacker{}; PLAYERGROUP members[2]{}; ENEMYGROUP enemies{};
	defenders.ubGroupID = 20; defenders.usGroupTeam = OUR_TEAM; defenders.ubGroupSize = 2;
	defenders.ubSectorX = 9; defenders.ubSectorY = 1; defenders.ubPrevX = 10; defenders.ubPrevY = 1;
	defenders.ubTransportationMask = FOOT; defenders.pPlayerList = members; defenders.next = &attacker;
	attacker.ubGroupID = 80; attacker.usGroupTeam = ENEMY_TEAM; attacker.ubGroupSize = 7;
	attacker.ubSectorX = 9; attacker.ubSectorY = 1; attacker.ubPrevX = 10; attacker.ubPrevY = 1;
	attacker.ubTransportationMask = FOOT; attacker.pEnemyGroup = &enemies; enemies.ubNumTroops = 7;
	CHECK(gpGroupList == nullptr, "isolated native strategic groups"); gpGroupList = &defenders;
	CHECK(AdoptJa2StrategicGroup(defenders) && AdoptJa2StrategicGroup(attacker), "exact friendly and hostile identities adopted");
	for (unsigned index = 0; index < (pendingHire ? 3u : 2u); ++index)
	{
		auto& actor = *repository.resolve(index);
		actor.identity().id() = SoldierID{static_cast<UINT16>(index)}; actor.identity().incarnation() = 101 + index;
		actor.identity().profile() = index == 2 ? 6 : 21 + index;
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM; actor.roster().inSector() = FALSE;
		actor.deployment().setSector(9, 1, 0); actor.deployment().groupId() = index == 2 ? 0 : 20;
		actor.deployment().previousSectorId() = SECTOR(10, 1);
		actor.assignment().current() = index == 2 ? IN_TRANSIT : 0;
		actor.vitals().health() = actor.vitals().maximumHealth() = 100;
		actor.vitals().breath() = actor.vitals().maximumBreath() = 100;
		actor.statistics().experienceLevel() = 6; actor.statistics().leadership() = 20; actor.statistics().strength() = 100;
		if (index == 2) { actor.deployment().arrivalTime() = 2250; actor.employment().endTime() = 13740; }
		CHECK(AdoptJa2TacticalEntity(actor), "native defender or pending hire adopted");
		if (index < 2) members[index].actor = GetJa2TacticalEntityId(actor);
	}
	members[0].next = &members[1];
	for (auto& sector : SectorInfo)
	{
		sector.ubGarrisonID = NO_GARRISON;
		for (auto& terrain : sector.ubTraversability) terrain = GROUNDBARRIER;
	}
	SectorInfo[SECTOR(9, 1)].ubTraversability[1] = ROAD;
	SectorInfo[SECTOR(10, 1)].ubTraversability[3] = ROAD;
	StrategicMap[CALCULATE_STRATEGIC_INDEX(9, 1)].bNameId = invasion ? 1 : BLANK_SECTOR;
	auto& queue = GetJa2CampaignEventQueue();
	CHECK(queue.empty(), "isolated event queue");
	const auto sentinel = queue.schedule({112501, 27, 0, ONETIME_EVENT, EVENT_GROUP_ABOUT_TO_ARRIVE, 0});
	CampaignEventQueueNode* hireEvent = nullptr;
	if (pendingHire)
	{
		CHECK(AddStrategicEventChecked(EVENT_DELAYED_HIRING_OF_MERC, 2250, 2), "one real delayed-hiring event remains scheduled");
		hireEvent = queue.head() ? queue.head()->next : nullptr;
	}
	std::vector<CampaignEventSnapshot> initialEvents;
	CHECK(sentinel && queue.capture(initialEvents), "native event snapshot captured before any decision");
	const auto balance = LaptopSaveInfo.iCurrentBalance;
	const auto hireIdentity = pendingHire ? GetJa2TacticalEntityId(*repository.resolve(2)) : TacticalEntityId{};
	const auto hireEventId = hireEvent ? hireEvent->id : CampaignEventId{};
	const auto hireEventSnapshot = hireEvent ? hireEvent->snapshot() : CampaignEventSnapshot{};
	const auto pendingUnchanged = [&]() {
		if (!pendingHire) return true;
		if (!queue.validate()) return false;
		unsigned hires = 0;
		for (const auto* event = queue.head(); event; event = event->next)
			if (event->callbackId == EVENT_DELAYED_HIRING_OF_MERC)
			{
				if (event != hireEvent || event->id != hireEventId || !(event->snapshot() == hireEventSnapshot)) return false;
				++hires;
			}
		const auto* hire = repository.resolve(2);
		return hires == 1 && hire && GetJa2TacticalEntityId(*hire) == hireIdentity &&
			hire->assignment().current() == IN_TRANSIT && hire->deployment().arrivalTime() == 2250 &&
			hire->employment().endTime() == 13740 && !hire->roster().inSector() &&
			!hire->deployment().isBetweenSectors() && !hire->deployment().groupId() &&
			hire->deployment().sectorX() == 9 && hire->deployment().sectorY() == 1 && !hire->deployment().sectorZ();
	};
	const auto unchanged = [&]() {
		std::vector<CampaignEventSnapshot> current;
		return queue.validate() && queue.capture(current) && current == initialEvents && queue.head() == sentinel.event &&
			GetWorldTotalSeconds() == 112500 && GamePaused() && !IsTimeBeingCompressed() &&
			!IsJa2TacticalWorldLoaded() && !gfPreBattleInterfaceActive && !fDisableMapInterfaceDueToBattle &&
			!gfTacticalPlacementGUIActive && !gfEnterTacticalPlacementGUI &&
			DialogueQueueIsEmpty() && !DialogueActive() && !PauseStateLocked() &&
			GetPendingNewScreen() != MSG_BOX_SCREEN && LaptopSaveInfo.iCurrentBalance == balance && pendingUnchanged();
	};
	DedicatedCoopArrivalState state;
	CHECK(BindDedicatedCoopArrivalState(state) &&
		DeferDedicatedCoopArrival(DedicatedCoopArrivalKind::Battle, &attacker, &defenders) && state.front() && !state.failure(),
		"native enemy arrival retains the attacker and distinct friendly dialog group");
	if (!state.front()) return 1;
	const auto pending = *state.front();
	CHECK(pending.group == GetJa2StrategicGroupId(80) && pending.dialogGroup == GetJa2StrategicGroupId(20) &&
		pending.x == 9 && pending.y == 1 && pending.worldSeconds == 112500 && unchanged(), "exact defensive context is deferred without native mutation");
	auto& defender = *repository.resolve(0); defender.assignment().fallAsleep();
	NativePreBattlePreparation report; report.encounterCode = 244;
	const auto untouched = report;
	const auto before = GetGameSimulationRandomSource()->checkpoint();
	CHECK(PrepareDedicatedCoopArrivalBattle(pending.id + 1, report) == Prepared::StaleDecision && SamePreparation(report, untouched),
		"stale defensive decision cannot prepare or overwrite output");
	const auto exactAttacker = attacker; const auto exactEnemies = enemies;
	for (unsigned fault = 0; fault < 12; ++fault)
	{
		if (fault == 0) attacker.pEnemyGroup = nullptr;
		if (fault == 1) attacker.usGroupTeam = MILITIA_TEAM;
		if (fault == 2) attacker.fVehicle = TRUE;
		if (fault == 3) attacker.ubTransportationMask = CAR;
		if (fault == 4) attacker.ubGroupSize = 0;
		if (fault == 5) attacker.ubGroupSize = 6;
		if (fault == 6) enemies.ubNumTroops = 0;
		if (fault == 7) enemies.ubTroopsInBattle = 1;
		if (fault == 8) attacker.fBetweenSectors = TRUE;
		if (fault == 9) attacker.ubSectorX = 10;
		if (fault == 10) defender.assignment().current() = IN_TRANSIT;
		if (fault == 11) members[1].actor = members[0].actor;
		CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) != Prepared::Prepared && SamePreparation(report, untouched) &&
			!state.failure() && !state.preparedBattle() && !IsHeadlessPreBattleActive() && defender.assignment().isAsleep() &&
			GetGameSimulationRandomSource()->checkpoint() == before && unchanged(),
			"invalid enemy union/team/vehicle/count/location or defender identity rejects before wake, RNG, events or UI");
		attacker = exactAttacker; enemies = exactEnemies; defender.assignment().current() = 0;
		members[1].actor = GetJa2TacticalEntityId(*repository.resolve(1));
	}
	CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::Prepared && state.preparedBattle() &&
		IsHeadlessPreBattleActive() && !defender.assignment().isAsleep(), "native defensive battle prepares exactly once and wakes its defenders");
	if (!state.preparedBattle()) return 1;
	CHECK(report.encounterCode == (invasion ? ENEMY_INVASION_CODE : ENEMY_ENCOUNTER_CODE) &&
		report.involvedMercs == 2 && report.uninvolvedMercs == (pendingHire ? 1u : 0u) &&
		report.actions.enterSector && report.actions.autoResolve && report.actions.retreat && !report.actions.tacticalPlacement,
		"native defensive encounter/invasion and permissions exclude the pending hire from involved mercs");
	CHECK(ResolvePreBattleGroup() == &attacker && PlayerMercInvolvedInThisCombat(&defender) &&
		(!pendingHire || !PlayerMercInvolvedInThisCombat(repository.resolve(2))) && unchanged(),
		"logical defensive battle uses the hostile group while preserving the unrelated pending hire and event");
	CoopSession::CoopCampaignArrival observation;
	CHECK(state.captureObservation(observation) && observation.stage == CoopSession::CoopCampaignArrivalStage::Prepared &&
		observation.involvedMercs == 2 && observation.uninvolvedMercs == (pendingHire ? 1u : 0u) && observation.nativeEnterSector &&
		!observation.nativePlacement, "published native preparation exposes a usable forced-entry decision");
	const auto preparedRandom = GetGameSimulationRandomSource()->checkpoint();
	for (unsigned retry = 0; retry < 32; ++retry)
	{
		NativePreBattlePreparation duplicate;
		CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, duplicate) == Prepared::Prepared && SamePreparation(duplicate, report) &&
			GetGameSimulationRandomSource()->checkpoint() == preparedRandom && unchanged(), "defensive retries return the exact native cache without another draw or event");
	}
	CHECK(EnterDedicatedCoopArrivalBattle(pending.id, Deployment::Spread) == Enter::UnsupportedDecision && unchanged(),
		"defending an occupied sector never substitutes player-arrival Spread deployment");
	for (unsigned retry = 0; retry < 4; ++retry)
		CHECK(EnterDedicatedCoopArrivalBattle(pending.id, Deployment::Forced) == Enter::MapUnavailable &&
			state.size() == 1 && !state.failure() && GetGameSimulationRandomSource()->checkpoint() == preparedRandom && unchanged(),
			"valid defensive entry reaches the actual missing-map boundary, not an IN_TRANSIT eligibility rejection");
	if (retreat)
	{
		const auto firstRecords = gMercProfiles[21].records.usBattlesRetreated;
		const auto secondRecords = gMercProfiles[22].records.usBattlesRetreated;
		const auto hireRecords = gMercProfiles[6].records.usBattlesRetreated;
		const auto cost = GetSectorMvtTimeForGroupWithoutUiCache(SECTOR(9, 1), EAST_STRATEGIC_MOVE, &defenders);
		CHECK(cost >= 0, "native defensive retreat destination is reachable");
		if (cost < 0) return 1;
		const auto minutes = cost ? static_cast<UINT32>(cost) : 5u;
		CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id + 1) == Retreat::StaleDecision && unchanged(),
			"stale retreat preserves the defensive decision and delayed hire");
		CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::Retreated &&
			!state.failure() && !state.size() && !IsHeadlessPreBattleActive(),
			"actual defensive retreat succeeds with an unrelated pending hire at the same landing sector");
		CHECK(defenders.fBetweenSectors && defenders.ubNextX == 10 && defenders.ubNextY == 1 &&
			defenders.uiTraverseTime == minutes && defenders.uiArrivalTime == 1875 + minutes &&
			(defenders.uiFlags & GROUPFLAG_JUST_RETREATED_FROM_BATTLE) && !defenders.pWaypoints,
			"native defenders depart for their previous sector with the exact native traversal and arrival time");
		for (unsigned index = 0; index < 2; ++index)
		{
			const auto* actor = repository.resolve(index);
			CHECK(actor && actor->deployment().isBetweenSectors() && !actor->roster().inSector() &&
				actor->deployment().groupId() == defenders.ubGroupID && actor->assignment().current() == 0 &&
				actor->strategicPath().empty(), "only ordinary defenders join native retreat movement");
		}
		CHECK(gMercProfiles[21].records.usBattlesRetreated == firstRecords + 1 &&
			gMercProfiles[22].records.usBattlesRetreated == secondRecords + 1 &&
			gMercProfiles[6].records.usBattlesRetreated == hireRecords && pendingUnchanged(),
			"retreat records apply once to defenders while the pending actor and exact delayed-hire event remain untouched");
		unsigned arrivals = 0, warnings = 0, pursuits = 0;
		CHECK(queue.validate(), "native defensive retreat leaves a valid event queue");
		for (const auto* event = queue.head(); event; event = event->next)
		{
			if (event == sentinel.event || event == hireEvent) continue;
			if (event->callbackId == EVENT_GROUP_ARRIVAL && event->parameter == defenders.ubGroupID)
			{
				++arrivals;
				CHECK(event->snapshot() == (CampaignEventSnapshot{defenders.uiArrivalTime * 60u, 20, 0,
					ONETIME_EVENT, EVENT_GROUP_ARRIVAL, 0}), "native retreat posts the exact ordinary-group arrival event");
			}
			else if (event->callbackId == EVENT_GROUP_ABOUT_TO_ARRIVE && event->parameter == defenders.ubGroupID)
			{
				++warnings;
				CHECK(event->snapshot() == (CampaignEventSnapshot{(defenders.uiArrivalTime - 30u) * 60u, 20, 0,
					ONETIME_EVENT, EVENT_GROUP_ABOUT_TO_ARRIVE, 0}), "native retreat posts the exact advance warning when required");
			}
			else if (event->callbackId == EVENT_GROUP_ARRIVAL && event->parameter == attacker.ubGroupID)
			{
				++pursuits;
				CHECK(event->snapshot() == (CampaignEventSnapshot{attacker.uiArrivalTime * 60u, 80, 0,
					ONETIME_EVENT, EVENT_GROUP_ARRIVAL, 0}), "native enemy pursuit retains its actual delayed arrival event");
			}
			else CHECK(false, "retreat must not manufacture another event or replay the delayed hire");
		}
		CHECK(arrivals == 1 && warnings == (minutes > 30u ? 1u : 0u) && pursuits == 1 &&
			attacker.fBetweenSectors && attacker.ubNextX == 10 && attacker.ubNextY == 1 &&
			enemies.ubIntention == PURSUIT && attacker.uiArrivalTime >= defenders.uiArrivalTime + 5u,
			"real native retreat schedules one departure and enemy pursuit without merging the pending hire");
		CHECK(queue.head() == sentinel.event && sentinel.event->snapshot() == initialEvents.front() &&
			GamePaused() && !IsTimeBeingCompressed() && GetWorldTotalSeconds() == pending.worldSeconds &&
			!IsJa2TacticalWorldLoaded() && !gfPreBattleInterfaceActive && !fDisableMapInterfaceDueToBattle &&
			!gfTacticalPlacementGUIActive && !gfEnterTacticalPlacementGUI && !PauseStateLocked() &&
			DialogueQueueIsEmpty() && !DialogueActive() && GetPendingNewScreen() != MSG_BOX_SCREEN &&
			LaptopSaveInfo.iCurrentBalance == balance && state.captureObservation(observation) && !observation.decision,
			"retreat clears only the prepared decision and preserves paused clock, funds, unrelated event and headless state");
		std::vector<CampaignEventSnapshot> committedEvents;
		CHECK(queue.capture(committedEvents), "committed native retreat queue captured");
		const auto committedRandom = GetGameSimulationRandomSource()->checkpoint();
		for (unsigned retry = 0; retry < 32; ++retry)
		{
			std::vector<CampaignEventSnapshot> current;
			CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::NotPending && queue.capture(current) &&
				current == committedEvents && GetGameSimulationRandomSource()->checkpoint() == committedRandom && pendingUnchanged() &&
				gMercProfiles[21].records.usBattlesRetreated == firstRecords + 1 &&
				gMercProfiles[22].records.usBattlesRetreated == secondRecords + 1 &&
				gMercProfiles[6].records.usBattlesRetreated == hireRecords,
				"duplicate defensive retreat cannot replay movement, records, enemy pursuit or delayed hire");
		}
	}
	else
	{
		const auto old = GetJa2TacticalEntityId(defender);
		CHECK(ReleaseJa2TacticalEntity(defender), "release defender identity for stale cache test");
		++defender.identity().incarnation();
		CHECK(AdoptJa2TacticalEntity(defender) && GetJa2TacticalEntityId(defender) != old, "defender slot reincarnated");
		members[0].actor = GetJa2TacticalEntityId(defender);
		CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::GroupChanged &&
			EnterDedicatedCoopArrivalBattle(pending.id, Deployment::Forced) == Enter::GroupChanged && unchanged(),
			"prepared defense cannot follow a replaced friendly actor");
		CHECK(ReleaseJa2StrategicGroup(attacker) && AdoptJa2StrategicGroup(attacker) &&
			PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::GroupChanged && unchanged(),
			"prepared defense cannot follow a reincarnated enemy group slot");
	}
	UnbindDedicatedCoopArrivalState(state);
	CHECK(!DedicatedCoopArrivalDecisionPending() && !IsHeadlessPreBattleActive(), "native logical battle tears down without a screen");
	RemovePGroupWaypoints(&attacker); RemovePGroupWaypoints(&defenders);
	gpGroupList = nullptr; ResetJa2StrategicGroupDirectory(); queue.clear();
	if (!failures) std::puts("all native defensive arrival tests passed");
	return failures ? 1 : 0;
}
