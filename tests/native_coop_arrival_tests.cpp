// Real native event arrivals with no renderer, dialogs, installed campaign or
// alternate movement/encounter rules. Each scenario has isolated process state.
#include "DedicatedCoopArrival.h"
#include "DedicatedCoopTravel.h"
#include "GameContext.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "Strategic Movement.h"
#include "Strategic Pathing.h"
#include "Strategic Path Types.h"
#include "StrategicGroupHost.h"
#include "Campaign Types.h"
#include "strategicmap.h"
#include "Map Screen Interface.h"
#include "Map Screen Interface Map.h"
#include "Map Screen Interface Bottom.h"
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
#include "Dialogue Control.h"
#include "random.h"
#include "LaptopSave.h"
#include "Auto Resolve.h"
#include "Strategic AI.h"
#include "Quests.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorQuoteFlags.h"
#include "Morale.h"
#include "TacticalDeployment.h"
#include "Tactical Placement GUI.h"
#include "Soldier Add.h"
#include <Engine/Core/SimulationRandom.h>
#include <vfs/Core/vfs_init.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }
extern BOOLEAN fInMapMode, gfProcessingGameEvents, gfWaitingForInput;
extern UINT32 guiGameSecondsPerRealSecond;
extern BOOLEAN HandlePlayerGroupEnteringSectorToCheckForNPCsOfNote(GROUP*);
extern GARRISON_GROUP* gGarrisonGroup;
extern INT32 giGarrisonArraySize;
extern FLOAT gAmbushRadiusModifier;
extern void HandlePreBattleInterfaceStates();
extern UINT8 CalcTotalImportantSectors();
extern UINT16 TotalVisitableSurfaceSectors();
namespace
{
int failures = 0;
bool SamePreparation(const NativePreBattlePreparation& a, const NativePreBattlePreparation& b)
{
	return a.encounterCode == b.encounterCode && a.involvedMercs == b.involvedMercs &&
		a.uninvolvedMercs == b.uninvolvedMercs && a.ambushRadiusModifier == b.ambushRadiusModifier &&
		a.actions.autoResolve == b.actions.autoResolve && a.actions.enterSector == b.actions.enterSector &&
		a.actions.retreat == b.actions.retreat && a.actions.tacticalPlacement == b.actions.tacticalPlacement;
}
}
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

int main(int argc, char** argv)
{
	using Kind = DedicatedCoopArrivalKind;
	using Reply = DedicatedCoopArrivalReply;
	using Result = DedicatedCoopArrivalReplyResult;
	using Enter = DedicatedCoopArrivalEnterResult;
	using Retreat = DedicatedCoopArrivalRetreatResult;
	using Deploy = NativePreBattleDeployment;
	const auto option = [=](const char* arg) { return argc == 2 && std::strcmp(argv[1], arg) == 0; };
	const bool ambush = option("--ambush"), deployment = option("--deployment"), scout = option("--scout");
	const bool retreated = option("--retreat"), concealed = option("--concealed"), reinforcements = option("--reinforcements");
	const bool randomFailure = option("--prepare-failure");
	const bool retreatAction = option("--retreat-action");
	const bool retreatWarningFailure = option("--retreat-warning-failure");
	const bool retreatFailure = option("--retreat-failure") || retreatWarningFailure;
	const bool battle = option("--battle") || ambush || deployment || scout || retreated || concealed || reinforcements || randomFailure || retreatAction || retreatFailure;
	const bool coordinate = argc == 2 && std::strcmp(argv[1], "--coordinate") == 0;
	const bool nonfinal = argc == 2 && std::strcmp(argv[1], "--nonfinal") == 0;
	const bool stale = argc == 2 && std::strcmp(argv[1], "--stale") == 0;
	const bool capacity = argc == 2 && std::strcmp(argv[1], "--capacity") == 0;
	const bool multiple = argc == 2 && std::strcmp(argv[1], "--multiple") == 0;
	const bool invalid = argc == 2 && std::strcmp(argv[1], "--invalid") == 0;
	const bool bloodcats = argc == 2 && std::strcmp(argv[1], "--bloodcats") == 0;
	const bool hostile = battle || coordinate || bloodcats;
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	CHECK(InstallGameSimulationRandom(20260907) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(), "native fixture starts");
	if (failures) return 1;
	vfs_init::VfsConfig config;
	auto* profile = new vfs_init::Profile();
	profile->m_name = L"native-coop-arrival-fixture";
	profile->m_root = vfs::Path(NATIVE_COOP_TRAVEL_FIXTURE_ROOT); profile->m_writable = false;
	auto* location = new vfs_init::Location(); location->m_type = L"DIRECTORY";
	profile->addLocation(location, true); config.addProfile(profile, true);
	CHECK(vfs_init::initVirtualFileSystem(config, false), "test-owned native campaign Lua mounted");
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	[[maybe_unused]] auto screen = OverrideCurrentScreen(MAP_SCREEN);
	fInMapMode = TRUE; gTacticalStatus.fDidGameJustStart = FALSE; gTacticalStatus.fEnemyInSector = FALSE;
	gbPlayerNum = OUR_TEAM; gfAtLeastOneMercWasHired = TRUE;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0}; gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	gGameOptions.fNewTraitSystem = FALSE; gGameExternalOptions.fDisease = FALSE; gGameExternalOptions.iStrengthToLiftHalfKilo = 2;
	RestoreJa2TacticalTurnState(0, OUR_TEAM, 0); RestoreJa2CampaignClock(111600, 111600);
	SetSelectedDestChar(-1); UnLockPauseState(); PauseGame();
	GROUP group{}, later{}; PLAYERGROUP members[2]{}; ENEMYGROUP enemies{};
	GARRISON_GROUP garrison{}; garrison.ubComposition = QUEEN_DEFENCE;
	gGarrisonGroup = &garrison; giGarrisonArraySize = 1;
	group.ubGroupID = 20; group.usGroupTeam = OUR_TEAM; group.ubGroupSize = 2;
	group.ubSectorX = 9; group.ubSectorY = 1; group.ubTransportationMask = FOOT;
	CHECK(gpGroupList == nullptr, "isolated group fixture"); gpGroupList = &group;
	CHECK(AdoptJa2StrategicGroup(group), "native group adopted");
	const auto identity = GetJa2StrategicGroupId(20);
	for (unsigned i = 0; i < 2; ++i)
	{
		auto& actor = *repository.resolve(i);
		actor.identity().id() = SoldierID{static_cast<UINT16>(i)}; actor.identity().incarnation() = 101 + i;
		actor.identity().profile() = NO_PROFILE; actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		if (hostile) actor.identity().profile() = 21 + i;
		actor.statistics().experienceLevel() = 6; actor.statistics().leadership() = deployment ? 95 : 20;
		actor.deployment().groupId() = 20; actor.deployment().setSector(9, 1, 0); actor.assignment().current() = 0;
		actor.vitals().health() = actor.vitals().maximumHealth() = 100;
		actor.vitals().breath() = actor.vitals().maximumBreath() = 100; actor.statistics().strength() = 100;
		CHECK(AdoptJa2TacticalEntity(actor), "native actor adopted"); members[i].actor = GetJa2TacticalEntityId(actor);
	}
	members[0].next = &members[1]; group.pPlayerList = &members[0];
	for (auto& sector : SectorInfo) for (auto& terrain : sector.ubTraversability) terrain = GROUNDBARRIER;
	SectorInfo[SECTOR(9, 1)].ubTraversability[1] = ROAD; SectorInfo[SECTOR(10, 1)].ubTraversability[3] = ROAD;
	SectorInfo[SECTOR(10, 1)].ubTraversability[1] = ROAD; SectorInfo[SECTOR(11, 1)].ubTraversability[3] = ROAD;
	// Scout morale refreshes native campaign progress. Supply the difficulty and
	// sector data normally loaded from XML: x86 traps on missing divisors while
	// ARM can silently produce zero. Keep the real morale/progress path active.
	gGameOptions.ubDifficultyLevel = DIF_LEVEL_EASY;
	zDiffSetting[DIF_LEVEL_EASY].iNumKillsPerProgressPoint = 10;
	NUMBER_OF_SAMS = 1; gpSamSectorX[0] = 2; gpSamSectorY[0] = 4;
	SectorInfo[SECTOR(2, 4)].ubTraversability[THROUGH_STRATEGIC_MOVE] = ROAD;
	for (INT16 x = 9; x <= 11; ++x)
		SectorInfo[SECTOR(x, 1)].ubTraversability[THROUGH_STRATEGIC_MOVE] = ROAD;
	CHECK(zDiffSetting[gGameOptions.ubDifficultyLevel].iNumKillsPerProgressPoint > 0,
		"native campaign progress has a kills divisor");
	CHECK(CalcTotalImportantSectors() > 0, "native campaign progress has an important sector");
	CHECK(TotalVisitableSurfaceSectors() > 0, "native campaign progress has visitable surface sectors");
	if (failures) return 1;
	auto& npc = gMercProfiles[191]; npc.Type = PROFILETYPE_NPC; npc.bLife = 100;
	npc.sSectorX = 10; npc.sSectorY = 1; npc.bSectorZ = 0;
	if (battle || coordinate) SectorInfo[SECTOR(10, 1)].ubNumTroops = 1;
	if (bloodcats)
	{
		gBloodcatPlacements[SECTOR(10, 1)][0].PlacementType = BLOODCAT_PLACEMENT_LAIR;
		SectorInfo[SECTOR(10, 1)].bBloodCats = SectorInfo[SECTOR(10, 1)].bBloodCatPlacements = 2;
	}
	DedicatedCoopArrivalState state, other;
	CoopSession::CoopCampaignArrival observation;
	using ObservationStage = CoopSession::CoopCampaignArrivalStage;
	CHECK(state.captureObservation(observation) && !observation.decision, "new runtime observes an explicit empty arrival queue");
	CHECK(!DedicatedCoopArrivalDecisionPending() && !DeferDedicatedCoopArrival(Kind::WildernessNpc, &group), "unbound legacy UI is not intercepted");
	CHECK(BindDedicatedCoopArrivalState(state) && !BindDedicatedCoopArrivalState(other), "exclusive runtime binding cannot be stolen");
	UnbindDedicatedCoopArrivalState(other);
	CHECK(ReplyToDedicatedCoopArrival(1, Reply::Acknowledge) == Result::NotPending, "empty replies do nothing");
	CHECK(RetreatFromDedicatedCoopArrivalBattle(1) == Retreat::NotPending, "empty retreat cannot mutate a campaign");
	CHECK(EnterDedicatedCoopArrivalBattle(1, Deploy::Spread) == Enter::NotPending &&
		!ApplyHeadlessPreBattleDeployment() && !SpreadHeadlessPreBattleMercs(), "entry/deployment cannot run without an explicit prepared battle");
	// The hook belongs after the native predicate. No notice for another sector,
	// a dead/recruited/already-spoken-to NPC, or a marked town.
	CHECK(!HandlePlayerGroupEnteringSectorToCheckForNPCsOfNote(&group) && !state.size(), "NPC in another sector does not cause a notice");
	npc.sSectorX = 9;
	for (unsigned rule = 0; rule < 4; ++rule)
	{
		if (rule == 0) npc.bLife = 0;
		if (rule == 1) npc.ubMiscFlags = PROFILE_MISC_FLAG_RECRUITED;
		if (rule == 2) npc.ubLastDateSpokenTo = 1;
		if (rule == 3) StrategicMap[CALCULATE_STRATEGIC_INDEX(9, 1)].bNameId = 1;
		CHECK(!HandlePlayerGroupEnteringSectorToCheckForNPCsOfNote(&group) && !state.size(), "native NPC exclusions cannot be bypassed by headless capture");
		npc.bLife = 100; npc.ubMiscFlags = 0; npc.ubLastDateSpokenTo = 0;
		StrategicMap[CALCULATE_STRATEGIC_INDEX(9, 1)].bNameId = BLANK_SECTOR;
	}
	npc.sSectorX = 10;
	const auto start = StartDedicatedCoopTravel(identity, 10, 1);
	CHECK(start.code == DedicatedCoopTravelStartCode::Started, "native adjacent leg starts");
	if (coordinate)
	{
		// A native militia reinforcement is still en route to the same sector.
		later.ubGroupID = 21; later.usGroupTeam = MILITIA_TEAM; later.ubGroupSize = 1;
		later.ubSectorX = 11; later.ubSectorY = 1; later.ubNextX = 10; later.ubNextY = 1;
		later.fBetweenSectors = TRUE; later.uiTraverseTime = 89; later.uiArrivalTime = start.arrivalMinutes + 10;
		group.next = &later; CHECK(AdoptJa2StrategicGroup(later), "reinforcement group adopted");
	}
	if (retreated) group.uiFlags |= GROUPFLAG_JUST_RETREATED_FROM_BATTLE;
	if (ambush || deployment || scout)
	{
		SectorInfo[SECTOR(10, 1)].ubNumTroops = 0; SectorInfo[SECTOR(10, 1)].ubGarrisonID = NO_GARRISON;
		later.ubGroupID = 21; later.usGroupTeam = ENEMY_TEAM; later.ubGroupSize = 7;
		later.ubSectorX = 10; later.ubSectorY = 1; enemies.ubNumTroops = 7;
		later.pEnemyGroup = &enemies; group.next = &later;
		CHECK(AdoptJa2StrategicGroup(later), "native mobile enemy group adopted");
		gfAutoAmbush = TRUE;
		if (scout)
		{
			gGameOptions.fNewTraitSystem = TRUE; gSkillTraitValues.ubMaxNumberOfTraits = 3;
			gSkillTraitValues.ubNumberOfMajorTraitsAllowed = 2; gSkillTraitValues.fSCPreventsTheEnemyToAmbushMercs = TRUE;
			repository.resolve(0)->statistics().skillTrait(0) = SCOUTING_NT;
			gMercProfiles[21].bCharacterTrait = gMercProfiles[22].bCharacterTrait = CHAR_TRAIT_COWARD;
			gMoraleSettings.bValues[MORALE_ENEMYGROUP_COWARD] = -7;
			gMoraleSettings.bModifiers[MORALE_MOD_MAX] = 50;
		}
	}
	auto& queue = GetJa2CampaignEventQueue();
	const auto finishFixture = [&]() {
		UnbindDedicatedCoopArrivalState(state);
		CHECK(!DedicatedCoopArrivalDecisionPending() && !IsHeadlessPreBattleActive(), "composition teardown unbinds only its own state and logical battle");
		RemovePGroupWaypoints(&group);
		for (unsigned i = 0; i < 2; ++i) repository.resolve(i)->strategicPath().reset();
		gpGroupList = nullptr; ResetJa2StrategicGroupDirectory(); queue.clear();
		gGarrisonGroup = nullptr; giGarrisonArraySize = 0;
		return failures ? 1 : 0;
	};
	const auto sentinel = queue.schedule({start.arrivalMinutes * 60 + 1, 27, 0, ONETIME_EVENT, EVENT_GROUP_ABOUT_TO_ARRIVE, 0});
	const auto volunteers = LaptopSaveInfo.dMilitiaVolunteerPool;
	CHECK(TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_60MINS), "native strategic time starts");
	auto& scheduler = game.runtime().campaignClockScheduler();
	for (unsigned frame = 0; frame < 300 && group.fBetweenSectors; ++frame)
		(void)AdvanceClockFromFixedStep(scheduler, 10000);
	CHECK(!group.fBetweenSectors && group.ubSectorX == 10 && !group.pWaypoints &&
		!repository.resolve(0)->deployment().isBetweenSectors() && !repository.resolve(1)->deployment().isBetweenSectors(), "native callback completed the arrival before asking");
	CHECK(!state.failure() && state.size() == 1 && state.front(), "native rule records one pending decision, not a local dialog");
	if (!state.front()) return 1;
	const auto pending = *state.front();
	CHECK(state.captureObservation(observation) && observation.decision == pending.id && observation.pendingCount == 1 &&
		observation.x == 10 && observation.y == 1 && observation.stage == ObservationStage::Pending && !observation.nativeEnterSector &&
		!observation.involvedMercs && observation.finalDestination == pending.finalDestination,
		"real native arrival publishes its exact notice without preparing a battle or granting commands");
	CHECK(pending.group == identity && pending.x == 10 && pending.y == 1 && pending.z == 0 && pending.worldSeconds == start.arrivalMinutes * 60 &&
		pending.kind == (coordinate ? Kind::CoordinateAttack : hostile ? Kind::Battle : Kind::WildernessNpc), "typed decision identifies exact native context");
	CHECK(pending.finalDestination == !hostile, "only NPC decisions query friendly route completion");
	if (bloodcats) CHECK(pending.encounterCode == BLOODCAT_AMBUSH_CODE && SectorInfo[SECTOR(10, 1)].bBloodCats == 2,
		"native bloodcat ambush detection and population survive without the notification dialog");
	CHECK(GamePaused() && !IsTimeBeingCompressed() && gfTimeInterrupt && GetWorldTotalSeconds() == pending.worldSeconds &&
		queue.validate() && queue.size() == 1 && queue.head() == sentinel.event, "arrival interrupts at the exact timestamp, retaining the next stable event");
	CHECK(!IsJa2TacticalWorldLoaded() && !gfPreBattleInterfaceActive && !fDisableMapInterfaceDueToBattle &&
		DialogueQueueIsEmpty() && !DialogueActive() && !PauseStateLocked(), "no world load, GUI/dialogue continuation or stolen pause lock");
	CHECK(LaptopSaveInfo.dMilitiaVolunteerPool == volunteers + (hostile ? 0 : 7), "peaceful NPC arrival still executes native Lua; hostile arrival does not liberate the sector");
	CHECK(!AllowedToTimeCompress() && !TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_60MINS), "pending decision rejects time-leader resume");
	CHECK(StartDedicatedCoopTravel(identity, 11, 1).code != DedicatedCoopTravelStartCode::Started && !group.pWaypoints, "new travel cannot bypass the decision");
	const auto rng = GetGameSimulationRandomSource()->checkpoint();
	const auto notice = observation;
	for (unsigned read = 0; read < 100; ++read)
		CHECK(state.captureObservation(observation) && CoopSession::SameCoopCampaignArrival(notice, observation) &&
			GetGameSimulationRandomSource()->checkpoint() == rng && queue.head() == sentinel.event && GetWorldTotalSeconds() == pending.worldSeconds,
			"observation cannot roll RNG, advance events, choose an action or traverse native mutable state");
	for (unsigned frame = 0; frame < 100; ++frame) (void)AdvanceClockFromFixedStep(scheduler, 10000);
	CHECK(GetWorldTotalSeconds() == pending.worldSeconds && queue.head() == sentinel.event && GetGameSimulationRandomSource()->checkpoint() == rng, "idle frames cannot advance events or RNG");
	// Even a legacy caller restoring clock scalars cannot bypass the decision at
	// the actual scheduler boundary. Restore them through native pause afterward.
	UnPauseGame(); guiGameSecondsPerRealSecond = 3600; SetClockResolutionPerSecond(60);
	CHECK(AdvanceClockFromFixedStep(scheduler, 1000000).error == CampaignClockScheduleError::Inactive &&
		GetWorldTotalSeconds() == pending.worldSeconds && queue.head() == sentinel.event, "scheduler independently blocks pending decisions");
	guiGameSecondsPerRealSecond = 0; SetClockResolutionPerSecond(0); PauseGame();
	CHECK(ReplyToDedicatedCoopArrival(pending.id + 1, Reply::Stop) == Result::StaleDecision && state.size() == 1, "wrong ID cannot consume a decision");
	CHECK(DeferDedicatedCoopArrival(pending.kind, &group, hostile && !coordinate ? &group : nullptr) && state.size() == 1, "identical native notice is coalesced without a new ID");
	if (hostile)
	{
		CHECK(EnterDedicatedCoopArrivalBattle(pending.id, Deploy::Spread) == (coordinate ? Enter::UnsupportedDecision : Enter::NotPrepared),
			"entry cannot implicitly prepare/reroll or answer another native interaction");
		CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == (coordinate ? Retreat::UnsupportedDecision : Retreat::NotPrepared),
			"retreat cannot prepare or answer another native interaction");
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::UnsupportedDecision &&
			ReplyToDedicatedCoopArrival(pending.id, Reply::Stop) == Result::UnsupportedDecision && state.size() == 1,
			"battle/coordination stays pending, never treated as an NPC acknowledgment or automatic result");
		if (coordinate) CHECK(!gfWaitingForInput && (later.uiFlags & GROUPFLAG_SIMULTANEOUSARRIVAL_CHECKED) &&
			later.uiArrivalTime == start.arrivalMinutes + 10 && later.fBetweenSectors, "native coordination detection retained; no forced synchronized arrival");
		using Prepared = DedicatedCoopArrivalPrepareResult;
		NativePreBattlePreparation report; report.encounterCode = 244;
		const auto untouched = report;
		CHECK(PrepareDedicatedCoopArrivalBattle(pending.id + 1, report) == Prepared::StaleDecision && SamePreparation(report, untouched),
			"stale preparation cannot change native state or output");
		if (coordinate)
			CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::UnsupportedDecision && !IsHeadlessPreBattleActive(),
				"coordination is not silently answered by battle preparation");
		else
		{
			auto& actor = *repository.resolve(0);
			actor.assignment().fallAsleep();
			if (concealed) actor.featureFlags().secondaryFlags() |= SOLDIER_CONCEALINSERTION;
			const auto beforePrepare = GetGameSimulationRandomSource()->checkpoint();
			for (unsigned fault = 0; fault < 7; ++fault)
			{
				if (fault == 0) gfProcessingGameEvents = TRUE;
				if (fault == 1) group.next = &group;
				if (fault == 2) SectorInfo[SECTOR(10, 1)].ubTroopsInBattle = 1;
				if (fault == 3) gfPreBattleInterfaceActive = TRUE;
				if (fault == 4) RestoreJa2CampaignClock(pending.worldSeconds + 1, pending.worldSeconds + 1);
				if (fault == 5) group.ubSectorX = 11;
				if (fault == 6) SetPendingNewScreen(MSG_BOX_SCREEN);
				CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) != Prepared::Prepared && SamePreparation(report, untouched) &&
					!state.preparedBattle() && !IsHeadlessPreBattleActive() && actor.assignment().isAsleep() &&
					GetGameSimulationRandomSource()->checkpoint() == beforePrepare,
					"invalid preparation is side-effect-free before native wake/RNG/publication");
				CHECK(state.captureObservation(observation) && observation.stage == ObservationStage::Unsupported &&
					!observation.nativeEnterSector, "a refused preparation is not presented as a battle still loading");
				gfProcessingGameEvents = FALSE; group.next = (ambush || deployment || scout) ? &later : nullptr;
				SectorInfo[SECTOR(10, 1)].ubTroopsInBattle = 0; gfPreBattleInterfaceActive = FALSE;
				RestoreJa2CampaignClock(pending.worldSeconds, pending.worldSeconds); group.ubSectorX = 10;
				SetPendingNewScreen(NO_PENDING_SCREEN);
			}
			if (reinforcements)
			{
				gGameExternalOptions.gfAllowReinforcements = TRUE;
				SectorInfo[SECTOR(9, 1)].ubNumberOfCivsAtLevel[0] = 3;
				gTacticalStatus.uiFlags |= WANT_MILITIA_REINFORCEMENTS;
				CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::ReinforcementDecisionRequired &&
					SamePreparation(report, untouched) && !IsHeadlessPreBattleActive() && actor.assignment().isAsleep() &&
					GetGameSimulationRandomSource()->checkpoint() == beforePrepare && (gTacticalStatus.uiFlags & WANT_MILITIA_REINFORCEMENTS),
					"native adjacent militia question remains unanswered, without clearing flags or preparing battle");
				CHECK(state.captureObservation(observation) && observation.stage == ObservationStage::ReinforcementsRequired &&
					!observation.involvedMercs && !observation.nativeEnterSector, "clients can distinguish the unsatisfied native reinforcement question");
				// Remove the fixture reinforcements, not a player answer. The same
				// native preflight can now progress because no question is required.
				SectorInfo[SECTOR(9, 1)].ubNumberOfCivsAtLevel[0] = 0;
			}
			CHECK(pending.justRetreated == retreated && !(group.uiFlags & GROUPFLAG_JUST_RETREATED_FROM_BATTLE),
				"native arrival flag cleanup cannot lose the deferred retreat restriction");
			if (randomFailure)
			{
				auto exhausted = GetGameSimulationRandomSource()->checkpoint();
				exhausted.rawValuesGenerated = UINT64_MAX;
				CHECK(GetGameSimulationRandomSource()->restoreCheckpoint(exhausted) == SimulationRandomCheckpointError::None,
					"native RNG exhaustion injected without a production test hook");
				CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::Failed && state.failure() &&
					!state.preparedBattle() && IsHeadlessPreBattleActive() && SamePreparation(report, untouched) &&
					!GetGameSimulationRandomSource()->healthy(), "failure after native preparation begins latches without publishing a partially prepared result");
				CHECK(state.captureObservation(observation) && observation.stage == ObservationStage::Failed &&
					!observation.nativeEnterSector && !observation.involvedMercs, "failed native preparation cannot advertise partial battle choices");
				for (unsigned retry = 0; retry < 100; ++retry)
					CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::Failed && SamePreparation(report, untouched),
						"partial native failure cannot be retried or acknowledged away");
				CHECK(GamePaused() && !AllowedToTimeCompress() && queue.head() == sentinel.event &&
					GetGameSimulationRandomSource()->checkpoint() == exhausted, "failed preparation retains native pause/event/RNG boundary");
			}
			else
			{
				CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::Prepared && state.preparedBattle() &&
					IsHeadlessPreBattleActive() && !actor.assignment().isAsleep(), "one native preparation wakes mercs without screen widgets");
				if (!state.preparedBattle()) return 1;
				CHECK(state.captureObservation(observation) && observation.stage == ObservationStage::Prepared &&
					observation.encounterCode == report.encounterCode && observation.involvedMercs == report.involvedMercs &&
					observation.uninvolvedMercs == report.uninvolvedMercs && observation.nativeAutoResolve == report.actions.autoResolve &&
					observation.nativeEnterSector == report.actions.enterSector && observation.nativeRetreat == report.actions.retreat &&
					observation.nativePlacement == report.actions.tacticalPlacement,
					"prepared observation is the cached native result, including ambush/scout/deployment restrictions");
				const auto encounter = bloodcats ? BLOODCAT_AMBUSH_CODE : ambush ? ENEMY_AMBUSH_CODE :
					deployment ? ENEMY_AMBUSH_DEPLOYMENT_CODE : concealed ? CONCEALINSERTION_CODE : scout ? ENEMY_ENCOUNTER_CODE : ENTERING_ENEMY_SECTOR_CODE;
				CHECK(report.encounterCode == encounter && report.involvedMercs == 2 && report.uninvolvedMercs == 0 && report.ambushRadiusModifier > 0 &&
					report.actions.enterSector && report.actions.autoResolve == scout && report.actions.retreat == !(ambush || deployment || bloodcats || concealed || retreated) &&
					report.actions.tacticalPlacement == !(ambush || bloodcats || concealed), "native encounter, ambush, scout, deployment and retreat permissions preserved");
				INT16 x = 0, y = 0, z = -1;
				CHECK(GetCurrentBattleSectorXYZ(&x, &y, &z) && x == 10 && y == 1 && z == 0 &&
					GetCurrentBattleSectorXYZAndReturnTRUEIfThereIsABattle(&x, &y, &z) &&
					PlayerMercInvolvedInThisCombat(&actor) && ResolvePreBattleGroup() == &group,
					"native participants see the logical battle without pretending a PBI screen exists");
				CHECK(!gfPreBattleInterfaceActive && !IsJa2TacticalWorldLoaded() && !fDisableMapInterfaceDueToBattle &&
					DialogueQueueIsEmpty() && !DialogueActive() && !PauseStateLocked() && GamePaused() && state.size() == 1 &&
					!AllowedToTimeCompress() && queue.head() == sentinel.event && GetWorldTotalSeconds() == pending.worldSeconds,
					"preparation does not choose enter/retreat/autoresolve, open GUI/dialogue, consume pending event or resume time");
				CHECK(bloodcats || CheckFact(FACT_FIRST_BATTLE_BEING_FOUGHT, 0), "native first-battle fact prepared");
				if (reinforcements) CHECK(!(gTacticalStatus.uiFlags & WANT_MILITIA_REINFORCEMENTS), "native no-reinforcements path clears prior flag after successful preflight");
				const auto records = gMercProfiles[actor.identity().profile()].records.usAmbushesExperienced;
				CHECK(records == ((ambush || deployment || bloodcats || scout) ? 1 : 0), "native ambush/scout records applied once");
				const auto morale = actor.morale().tacticalModifier();
				if (scout) CHECK(morale == -7, "native coward morale rule runs once before the scout prevents the ambush");
				if (scout) CHECK(repository.resolve(1)->dialogue().hasSaid(SOLDIER_QUOTE_SAID_LOW_MORAL) && DialogueQueueIsEmpty(),
					"awake low-morale merc keeps native quote bookkeeping without queuing a headless GUI continuation");
				const auto preparedRng = GetGameSimulationRandomSource()->checkpoint();
				if (!report.actions.retreat)
					CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::NotPermitted &&
						GetGameSimulationRandomSource()->checkpoint() == preparedRng && !group.fBetweenSectors && state.size() == 1,
						"native ambush/bloodcat/concealed/just-retreated restrictions cannot be bypassed");
				if (retreatAction || retreatFailure)
				{
					CHECK(report.actions.retreat, "ordinary encounter permits retreat");
					const auto retreatRecords = gMercProfiles[21].records.usBattlesRetreated;
					CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id + 1) == Retreat::StaleDecision, "stale retreat cannot choose another battle");
					const auto prevX = group.ubPrevX, prevY = group.ubPrevY;
					for (unsigned fault = 0; fault < 8; ++fault)
					{
						if (fault == 0) SetPendingNewScreen(MSG_BOX_SCREEN);
						if (fault == 1) LockPauseState(99);
						if (fault == 2) actor.assignment().current() = ASSIGNMENT_POW;
						if (fault == 3) actor.featureFlags().primaryFlags() |= SOLDIER_AIRDROP;
						if (fault == 4) group.ubPrevX = group.ubSectorX;
						if (fault == 5) group.ubPrevY = 255;
						if (fault == 6) SectorInfo[SECTOR(10, 1)].ubTraversability[3] = GROUNDBARRIER;
						if (fault == 7) gfProcessingGameEvents = TRUE;
						CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) != Retreat::Retreated && !state.failure() &&
							!group.fBetweenSectors && state.size() == 1 && queue.head() == sentinel.event &&
							gMercProfiles[21].records.usBattlesRetreated == retreatRecords && GetWorldTotalSeconds() == pending.worldSeconds &&
							GetGameSimulationRandomSource()->checkpoint() == preparedRng, "unsafe retreat is rejected before native records/routes/events");
						SetPendingNewScreen(NO_PENDING_SCREEN); UnLockPauseState(); actor.assignment().current() = 0;
						actor.featureFlags().primaryFlags() &= ~SOLDIER_AIRDROP; group.ubPrevX = prevX; group.ubPrevY = prevY;
						SectorInfo[SECTOR(10, 1)].ubTraversability[3] = ROAD; gfProcessingGameEvents = FALSE;
					}
					SectorInfo[SECTOR(10, 1)].ubNumberOfCivsAtLevel[0] = 1;
					CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::AutoResolveRequired &&
						!group.fBetweenSectors && state.size() == 1, "militia cannot be abandoned without the native autoresolve continuation");
					SectorInfo[SECTOR(10, 1)].ubNumberOfCivsAtLevel[0] = 0;
					auto conflict = queue.schedule({pending.worldSeconds + 600, group.ubGroupID, 0, ONETIME_EVENT, EVENT_GROUP_ARRIVAL, 0});
					CHECK(conflict && RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::NativeContextUnavailable &&
						!group.fBetweenSectors, "retreat never posts a duplicate native group arrival");
					(void)queue.erase(conflict.event);
					auto* route = static_cast<PathSt*>(MemAlloc(sizeof(PathSt))); *route = {};
					actor.strategicPath().adopt(route); route->pNext = route;
					CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::GroupChanged &&
						actor.strategicPath().head() == route, "cyclic retreat route rejects before any native cleanup");
					route->pNext = nullptr;
					repository.resolve(1)->strategicPath().rebind(route);
					CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::GroupChanged &&
						actor.strategicPath().head() == route, "aliased retreat routes cannot be double freed");
					(void)repository.resolve(1)->strategicPath().release(); actor.strategicPath().reset();
					CampaignEventQueue originalQueue;
					if (retreatFailure)
					{
						CampaignEventQueue full(retreatWarningFailure ? 2 : 1);
						CHECK(full.schedule(sentinel.event->snapshot()), "bounded queue fault retains unrelated pending event");
						queue.swap(originalQueue); queue.swap(full);
					}
					const auto result = RetreatFromDedicatedCoopArrivalBattle(pending.id);
					if (retreatFailure)
					{
						CHECK(result == Retreat::Failed && state.failure() && state.size() == 1 && GamePaused(), "partial native retreat failure latches without decision consumption");
						CHECK(state.captureObservation(observation) && observation.stage == ObservationStage::Failed &&
							!observation.nativeRetreat, "failed retreat cannot continue advertising native battle choices");
						const auto failedRng = GetGameSimulationRandomSource()->checkpoint();
						const auto failedRecords = gMercProfiles[21].records.usBattlesRetreated;
						for (unsigned retry = 0; retry < 100; ++retry)
							CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::Failed &&
								gMercProfiles[21].records.usBattlesRetreated == failedRecords && GetGameSimulationRandomSource()->checkpoint() == failedRng,
								"partial retreat cannot replay native side effects");
					}
					else
					{
						CHECK(result == Retreat::Retreated && !state.failure() && !state.size() && !IsHeadlessPreBattleActive() &&
							group.fBetweenSectors && group.ubNextX == 9 && group.ubNextY == 1 &&
							(group.uiFlags & GROUPFLAG_JUST_RETREATED_FROM_BATTLE) &&
							actor.deployment().isBetweenSectors() && repository.resolve(1)->deployment().isBetweenSectors() &&
							gMercProfiles[21].records.usBattlesRetreated == retreatRecords + 1 && queue.head() == sentinel.event &&
							GamePaused() && !IsJa2TacticalWorldLoaded() && GetWorldTotalSeconds() == pending.worldSeconds,
							"native retreat departs toward previous sector and retains the exact clock and unrelated event");
						CHECK(state.captureObservation(observation) && !observation.decision, "successful retreat explicitly clears the pending arrival observation");
						const auto committed = GetGameSimulationRandomSource()->checkpoint();
						const auto events = queue.size();
						for (unsigned retry = 0; retry < 100; ++retry)
							CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::NotPending && queue.size() == events &&
								GetGameSimulationRandomSource()->checkpoint() == committed && gMercProfiles[21].records.usBattlesRetreated == retreatRecords + 1,
								"duplicate retreat does not repeat native movement, records or enemy response");
						// Remove only the test's unrelated sentinel, then let actual
						// native movement events complete the return leg to empty A9.
						(void)queue.erase(sentinel.event);
						CHECK(TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_60MINS), "retreat leaves time available for a later explicit resume");
						for (unsigned frame = 0; frame < 300 && group.fBetweenSectors; ++frame)
							(void)AdvanceClockFromFixedStep(scheduler, 10000);
						CHECK(!group.fBetweenSectors && group.ubSectorX == 9 && group.ubSectorY == 1 &&
							!actor.deployment().isBetweenSectors() && !repository.resolve(1)->deployment().isBetweenSectors() &&
							!DedicatedCoopArrivalDecisionPending() && !IsJa2TacticalWorldLoaded() &&
							LaptopSaveInfo.dMilitiaVolunteerPool == volunteers + 11, "native event processing completes the retreat and its actual return-sector Lua hook, without teleporting or loading a world");
						StopTimeCompression(); PauseGame();
					}
					return finishFixture();
				}
				const auto choice = report.actions.tacticalPlacement ? Deploy::Spread : Deploy::Forced;
				CHECK(EnterDedicatedCoopArrivalBattle(pending.id + 1, choice) == Enter::StaleDecision, "stale entry cannot act on a later battle");
				CHECK(EnterDedicatedCoopArrivalBattle(pending.id, report.actions.tacticalPlacement ? Deploy::Forced : Deploy::Spread) ==
					(report.actions.tacticalPlacement ? Enter::DeploymentRequired : Enter::UnsupportedDecision),
					"native forced insertion and explicit Spread are not interchangeable; deployment is never skipped");
				for (unsigned fault = 0; fault < 8; ++fault)
				{
					if (fault == 0) gfProcessingGameEvents = TRUE;
					if (fault == 1) gfEnterTacticalPlacementGUI = TRUE;
					if (fault == 2) gfTacticalPlacementGUIActive = TRUE;
					if (fault == 3) actor.vitals().health() = 0;
					if (fault == 4) actor.featureFlags().primaryFlags() |= SOLDIER_AIRDROP;
					if (fault == 5) actor.assignment().current() = ASSIGNMENT_POW;
					if (fault == 6) SetJa2TacticalWorldSector(10, 1, 0);
					if (fault == 7) SetPendingNewScreen(MSG_BOX_SCREEN);
					CHECK(EnterDedicatedCoopArrivalBattle(pending.id, choice) != Enter::Entered && !state.failure() &&
						!IsJa2TacticalWorldLoaded() && state.size() == 1 && GetGameSimulationRandomSource()->checkpoint() == preparedRng,
						"unsafe native entry rejects before loading, RNG, decision consumption or route cleanup");
					gfProcessingGameEvents = gfEnterTacticalPlacementGUI = gfTacticalPlacementGUIActive = FALSE;
					actor.vitals().health() = 100; actor.featureFlags().primaryFlags() &= ~SOLDIER_AIRDROP;
					actor.assignment().current() = 0; SetJa2TacticalWorldSector(0, 0, -1);
					SetPendingNewScreen(NO_PENDING_SCREEN);
				}
				gfUseAlternateMap = TRUE;
				for (unsigned retry = 0; retry < 100; ++retry)
					CHECK(EnterDedicatedCoopArrivalBattle(pending.id, choice) == Enter::MapUnavailable && gfUseAlternateMap &&
						GetGameSimulationRandomSource()->checkpoint() == preparedRng && state.size() == 1 && !state.failure() &&
						IsHeadlessPreBattleActive() && !IsHeadlessPreBattleEntryInProgress() && !IsJa2TacticalWorldLoaded(),
						"missing exact map rejects without placeholder load, alternate-map consumption, RNG or losing the battle");
				gfUseAlternateMap = FALSE;
				auto* path = static_cast<PathSt*>(MemAlloc(sizeof(PathSt)));
				*path = {}; path->uiSectorId = CALCULATE_STRATEGIC_INDEX(10, 1);
				actor.strategicPath().adopt(path); path->pNext = path;
				CHECK(EnterDedicatedCoopArrivalBattle(pending.id, choice) == Enter::GroupChanged &&
					actor.strategicPath().head() == path && state.size() == 1, "cyclic route rejected before native entry frees any paths");
				path->pNext = nullptr;
				repository.resolve(1)->strategicPath().rebind(path);
				CHECK(EnterDedicatedCoopArrivalBattle(pending.id, choice) == Enter::GroupChanged &&
					actor.strategicPath().head() == path, "aliased member paths rejected before double-free native cleanup");
				(void)repository.resolve(1)->strategicPath().release();
				actor.strategicPath().reset();
				for (unsigned retry = 0; retry < 100; ++retry)
				{
					NativePreBattlePreparation duplicate;
					CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, duplicate) == Prepared::Prepared && SamePreparation(duplicate, report), "valid retry returns cached preparation");
					(void)AdvanceClockFromFixedStep(scheduler, 10000);
					InitPreBattleInterface(&group, TRUE); HandlePreBattleInterfaceStates();
				}
				CHECK(GetGameSimulationRandomSource()->checkpoint() == preparedRng && gAmbushRadiusModifier == report.ambushRadiusModifier &&
					actor.morale().tacticalModifier() == morale &&
					gMercProfiles[actor.identity().profile()].records.usAmbushesExperienced == records && !gfPreBattleInterfaceActive &&
					queue.head() == sentinel.event && GetWorldTotalSeconds() == pending.worldSeconds, "retries and accidental GUI continuations cannot reroll or repeat native side effects");
				const auto oldActor = GetJa2TacticalEntityId(actor);
				CHECK(ReleaseJa2TacticalEntity(actor), "native actor slot released after preparation");
				++actor.identity().incarnation();
				CHECK(AdoptJa2TacticalEntity(actor) && GetJa2TacticalEntityId(actor) != oldActor, "native actor slot reincarnated after preparation");
				members[0].actor = GetJa2TacticalEntityId(actor);
				CHECK(PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::GroupChanged,
					"cached preparation cannot follow a replaced member inside the same group identity");
				CHECK(state.captureObservation(observation) && observation.stage == ObservationStage::Unsupported &&
					!observation.involvedMercs && !observation.nativeEnterSector,
					"failed revalidation hides stale native choices without discarding or replaying their preparation");
				CHECK(EnterDedicatedCoopArrivalBattle(pending.id, choice) == Enter::GroupChanged,
					"entry cannot follow a replaced actor even inside the same group");
				CHECK(RetreatFromDedicatedCoopArrivalBattle(pending.id) == Retreat::GroupChanged,
					"retreat cannot follow a replaced actor even inside the same group");
				CHECK(ReleaseJa2StrategicGroup(group) && AdoptJa2StrategicGroup(group) &&
					PrepareDedicatedCoopArrivalBattle(pending.id, report) == Prepared::GroupChanged, "prepared reply cannot follow a reincarnated group slot");
				if (option("--battle"))
				{
					UnbindDedicatedCoopArrivalState(state);
					CHECK(BindDedicatedCoopArrivalState(other) && DeferDedicatedCoopArrival(Kind::Battle, &group, &group) && other.front(),
						"replacement runtime owns a separate pending encounter");
					if (!other.front()) return 1;
					CHECK(PrepareDedicatedCoopArrivalBattle(other.front()->id, report) == Prepared::Prepared, "replacement runtime prepares its own native context");
					state.reset(); UnbindDedicatedCoopArrivalState(state);
					CHECK(IsHeadlessPreBattleActive() && other.preparedBattle() && DedicatedCoopArrivalDecisionPending(),
						"late reset/teardown of an unbound runtime cannot clear its replacement's logical battle");
					CHECK(DeferDedicatedCoopArrival(Kind::WildernessNpc, &group) && other.size() == 2 &&
						EnterDedicatedCoopArrivalBattle(other.front()->id, Deploy::Spread) == Enter::OtherDecisionsPending &&
						RetreatFromDedicatedCoopArrivalBattle(other.front()->id) == Retreat::OtherDecisionsPending &&
						other.size() == 2 && !IsJa2TacticalWorldLoaded(), "entry cannot invalidate another queued worldless continuation");
					UnbindDedicatedCoopArrivalState(other);
				}
			}
		}
	}
	else if (stale)
	{
		CHECK(ReleaseJa2StrategicGroup(group) && AdoptJa2StrategicGroup(group) && GetJa2StrategicGroupId(20) != identity, "same raw slot reincarnated");
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Stop) == Result::GroupChanged && state.size() == 1, "stale reply cannot operate on a replacement group");
	}
	else if (invalid)
	{
		group.next = &group;
		CHECK(DeferDedicatedCoopArrival(Kind::WildernessNpc, &group) && state.failure() && state.size() == 1 &&
			state.front()->id == pending.id, "damaged native graph fails closed without replacing the accepted decision");
		group.next = nullptr;
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::NativeContextUnavailable &&
			!TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_60MINS), "capture failure cannot be acknowledged away or resumed");
	}
	else if (multiple)
	{
		// Two valid groups at the same timestamp must not share the GUI's single
		// prompting-group slot or lose the second notice.
		later.ubGroupID = 21; later.usGroupTeam = OUR_TEAM; later.ubGroupSize = 1;
		later.ubSectorX = 10; later.ubSectorY = 1; later.ubTransportationMask = FOOT;
		group.ubGroupSize = 1; members[0].next = nullptr; later.pPlayerList = &members[1];
		repository.resolve(1)->deployment().groupId() = 21;
		group.next = &later; CHECK(AdoptJa2StrategicGroup(later), "second exact NPC group adopted");
		CHECK(HandlePlayerGroupEnteringSectorToCheckForNPCsOfNote(&later) && state.size() == 2 && state.front()->id == pending.id,
			"second native NPC notice retained in FIFO order");
		CHECK(state.captureObservation(observation) && observation.decision == pending.id && observation.pendingCount == 2,
			"another native notice updates the count without replacing the pending front");
		CHECK(ReplyToDedicatedCoopArrival(pending.id + 1, Reply::Acknowledge) == Result::StaleDecision && state.size() == 2,
			"out-of-order reply cannot consume either pending group");
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::Applied && state.size() == 1 &&
			state.front()->group == GetJa2StrategicGroupId(21), "acknowledging first group reveals exact second decision");
		CHECK(state.captureObservation(observation) && observation.decision == state.front()->id && observation.pendingCount == 1,
			"native acknowledgment publishes the next exact FIFO decision");
		CHECK(ReplyToDedicatedCoopArrival(state.front()->id, Reply::Acknowledge) == Result::Applied && !state.size() && GamePaused(),
			"second group can be acknowledged separately without resuming time");
	}
	else if (capacity)
	{
		// Different native observation times create distinct requests. Capacity
		// failure retains every accepted decision and keeps the clock blocked.
		for (std::size_t i = 1; i <= DedicatedCoopArrivalState::Capacity; ++i)
		{
			RestoreJa2CampaignClock(pending.worldSeconds + static_cast<UINT32>(i), pending.worldSeconds + static_cast<UINT32>(i));
			CHECK(DeferDedicatedCoopArrival(Kind::WildernessNpc, &group), "bounded capture handles every notice");
		}
		CHECK(state.size() == DedicatedCoopArrivalState::Capacity && state.failure() && state.front()->id == pending.id &&
			DedicatedCoopArrivalDecisionPending() && !TrySetWorldlessStrategicTimeCompression(TIME_COMPRESS_60MINS), "overflow fails closed without dropping or answering an older decision");
	}
	else
	{
		gfProcessingGameEvents = TRUE;
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::NativeContextUnavailable, "reentrant event reply rejected");
		gfProcessingGameEvents = FALSE;
		SetPendingNewScreen(MSG_BOX_SCREEN);
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::NativeContextUnavailable && state.size() == 1,
			"NPC acknowledgment cannot bypass a different queued native dialog");
		SetPendingNewScreen(NO_PENDING_SCREEN);
		RestoreJa2CampaignClock(pending.worldSeconds + 1, pending.worldSeconds + 1);
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::NativeContextUnavailable, "changed native clock invalidates the old reply context");
		RestoreJa2CampaignClock(pending.worldSeconds, pending.worldSeconds);
		group.next = &group;
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::GroupChanged, "cyclic native graph bounded before identity gateway");
		group.next = nullptr;
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::Applied && !DedicatedCoopArrivalDecisionPending() &&
			GamePaused() && GetWorldTotalSeconds() == pending.worldSeconds && queue.head() == sentinel.event, "exact final acknowledgment consumes once and leaves time paused");
		CHECK(ReplyToDedicatedCoopArrival(pending.id, Reply::Acknowledge) == Result::NotPending, "duplicate acknowledgment has no second effect");
		CHECK(state.captureObservation(observation) && CoopSession::SameCoopCampaignArrival(observation, {}),
			"native final acknowledgment publishes a canonical empty replacement, not stale choices");
		if (nonfinal)
		{
			// A retained native route (e.g. from a save) stops at an NPC too. Do
			// not exercise the unsupported legacy multi-leg continuation path.
			auto* waypoint = static_cast<WAYPOINT*>(MemAlloc(sizeof(WAYPOINT)));
			*waypoint = {}; waypoint->x = 11; waypoint->y = 1; group.pWaypoints = waypoint;
			for (unsigned i = 0; i < 2; ++i)
			{
				auto* path = static_cast<PathSt*>(MemAlloc(sizeof(PathSt))); *path = {}; path->uiSectorId = 28;
				auto* tail = static_cast<PathSt*>(MemAlloc(sizeof(PathSt))); *tail = {}; tail->uiSectorId = 29;
				path->pNext = tail; tail->pPrev = path;
				repository.resolve(i)->strategicPath().adopt(path);
			}
			CHECK(HandlePlayerGroupEnteringSectorToCheckForNPCsOfNote(&group) && state.front() && !state.front()->finalDestination, "native wilderness rule defers retained route");
			if (!state.front()) return 1;
			const auto next = state.front()->id;
			CHECK(next > pending.id && ReplyToDedicatedCoopArrival(pending.id, Reply::Stop) == Result::StaleDecision && group.pWaypoints,
				"old acknowledgment cannot stop a later route");
			CHECK(ReplyToDedicatedCoopArrival(next, Reply::Acknowledge) == Result::UnsupportedDecision && group.pWaypoints,
				"continue is not silently implemented with unsafe route rebuilding");
			auto* path = const_cast<PathSt*>(repository.resolve(1)->strategicPath().head());
			auto* tail = path->pNext; path->pNext = path;
			CHECK(ReplyToDedicatedCoopArrival(next, Reply::Stop) == Result::GroupChanged && group.pWaypoints && !repository.resolve(0)->strategicPath().empty(),
				"cyclic actor path rejected before freeing any member's route");
			path->pNext = tail;
			CHECK(ReplyToDedicatedCoopArrival(next, Reply::Stop) == Result::Applied && !group.pWaypoints && GamePaused() &&
				repository.resolve(0)->strategicPath().empty() && repository.resolve(1)->strategicPath().empty(), "stop clears every ordinary native route, never resumes time");
		}
	}
	return finishFixture();
}
