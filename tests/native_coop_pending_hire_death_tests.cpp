// Actual skull-completion/strategic death and heap-owned squad removal, followed
// by read-only cold Resume classification. No death callback is substituted.
#include "DedicatedCoopMissionBootstrap.h"
#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "CampaignEventScheduling.h"
#include "CampaignLedger.h"
#include "CampaignLedgerRecord.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "Game Event Hook.h"
#include "GameSettings.h"
#include "GameInitOptionsScreen.h"
#include "strategicmap.h"
#include "TacticalWorldAdapter.h"
#include "TacticalEntityHost.h"
#include "StrategicGroupHost.h"
#include "StrategicSquadHost.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorEmploymentTypes.h"
#include "Tactical Save.h"
#include "Interface Control.h"
#include "strategic.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "Animation Data.h"
#include "Assignments.h"
#include "Squads.h"
#include "Strategic Movement.h"
#include "Strategic Status.h"
#include "Queen Command.h"
#include "Quests.h"
#include "Interface Panels.h"
#include "Interface.h"
#include "Overhead.h"
#include "Merc Hiring.h"
#include "Merc Contract.h"
#include "LaptopSave.h"
#include "finances.h"
#include "history.h"
#include "screenids.h"
#include "gameloop.h"
#include "FileMan.h"
#include "MemMan.h"
#include "random.h"
#include <Engine/Core/SimulationRandom.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1); }
extern BOOLEAN IsDeadGuyOnAnySquad(TacticalActor* actor);

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
struct Files
{
	std::filesystem::path root = std::filesystem::temp_directory_path() /
		("ja2-pending-hire-death-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	Files(bool capture)
	{
		std::filesystem::create_directories(root / "TEMP");
		if (capture)
		{
			std::filesystem::create_directories(root / "scripts");
			std::ofstream script(root / "scripts" / "Quests.lua");
			script << "function InternalStartQuest(q,x,y,history)\n"
				" if CheckQuest(q) == 0 then\n"
				"  SetQuest(q,1)\n"
				"  if history then SetHistoryFact(" << unsigned(HISTORY_QUEST_STARTED) <<
				",q,GetWorldTotalMin(),x,y) end\n"
				" else SetQuest(q,1) end\nend\n";
			script.close();
			CHECK(script.good(), "native first-capture Lua fixture written");
		}
		auto* profile = new vfs::CVirtualProfile(L"_PENDING_DEATH_TEST", vfs::Path(root.c_str()), true);
		getVFS()->getProfileStack()->pushProfile(profile);
		auto* tree = new vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str()));
		CHECK(tree->init(), "private native VFS directory initialized");
		profile->addLocation(tree);
		CHECK(getVFS()->addLocation(tree, profile), "private native VFS mounted");
	}
	~Files()
	{
		vfs::CVirtualFileSystem::shutdownVFS();
		std::error_code ignored; std::filesystem::remove_all(root, ignored);
	}
	std::string bytes(const char* name) const
	{
		std::ifstream file(root / "TEMP" / name, std::ios::binary);
		CHECK(file.is_open(), "native ledger opens with its actual on-disk filename");
		std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
		CHECK(!file.bad(), "complete native ledger bytes read without an I/O error");
		return bytes;
	}
};
template<class T> std::array<unsigned char, sizeof(T)> Representation(const T& value)
{
	std::array<unsigned char, sizeof(T)> bytes{};
	std::memcpy(bytes.data(), &value, sizeof(T)); return bytes;
}
}

int main(int argc, char** argv)
{
	const bool capture = argc == 2 && std::strcmp(argv[1], "--capture") == 0;
	if (argc > 2 || (argc == 2 && !capture)) return 2;
	std::setbuf(stdout, nullptr);
	DedicatedServerOptions options; options.enabled = true; options.mode = DedicatedServerMode::Coop;
	InstallDedicatedServerOptions(options);
	CHECK(InstallGameSimulationRandom(20260922) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"actual native campaign runtime starts");
	CHECK(InitializeMemoryManager(), "native memory manager initialized");
	Files files(capture);
	CHECK(InitializeFileManager(nullptr), "native file manager initialized");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	gbPlayerNum = OUR_TEAM;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{2};
	gTacticalStatus.fDidGameJustStart = FALSE;
	gTacticalStatus.fEnemyInSector = FALSE;
	gGameExternalOptions.iGameStartingTime = 90000;
	gGameOptions.ubSquadSize = 6; gGameOptions.fNewTraitSystem = FALSE;
	gGameExternalOptions.fDynamicOpinions = FALSE;
	// Native death refreshes morale and campaign progress. Initialize the
	// difficulty divisor and a real important sector normally loaded from XML;
	// x86 traps on zero division while ARM can silently produce zero.
	gGameOptions.ubDifficultyLevel = DIF_LEVEL_EASY;
	zDiffSetting[DIF_LEVEL_EASY].iNumKillsPerProgressPoint = 10;
	NUMBER_OF_SAMS = 1; gpSamSectorX[0] = 2; gpSamSectorY[0] = 4;
	RestoreJa2TacticalTurnState(0, OUR_TEAM, 0);
	InitializeJa2CampaignClock(112200);
	gsMercArriveSectorX = 9; gsMercArriveSectorY = 1;
	NotifyJa2TacticalWorldUnloaded(); ClearJa2TacticalWorldSector();
	CHECK(SetSectorFlag(9, 1, 0, SF_ALREADY_VISITED), "native death sector previously visited");
	[[maybe_unused]] auto screen = OverrideCurrentScreen(GAME_SCREEN);
	// The real exit-screen helper can run before the screen changes. Suppress
	// only the ordinary panel selection refresh while native squad removal runs.
	SetPendingNewScreen(MAINMENU_SCREEN); gusSelectedSoldier = NOBODY;
	guiTacticalInterfaceFlags = 0;
	InitSquads(); // Allocates the actual native GROUP and PLAYERGROUP owners.
	for (unsigned slot = 0; slot < 3; ++slot)
	{
		auto& actor = *repository.resolve(slot);
		const UINT8 profileId = slot == 0 ? 21 : slot == 1 ? 238 : 6;
		actor.identity().id() = SoldierID{static_cast<UINT16>(slot)};
		actor.identity().incarnation() = 101 + slot; actor.identity().profile() = profileId;
		actor.identity().bodyType() = REGMALE;
		actor.roster().active() = TRUE; actor.roster().team() = OUR_TEAM;
		actor.roster().inSector() = slot == 2 ? FALSE : TRUE;
		actor.deployment().setSector(9, 1, 0); actor.deployment().groupId() = 0;
		actor.assignment().current() = slot == 2 ? IN_TRANSIT : FIRST_SQUAD;
		actor.vitals().health() = actor.vitals().maximumHealth() = 80;
		actor.vitals().breath() = actor.vitals().maximumBreath() = 100;
		actor.employment().mercenaryType() = MERC_TYPE__AIM_MERC;
		actor.employment().totalLength() = 7; actor.employment().lastContractType() = CONTRACT_EXTEND_1_WEEK;
		actor.employment().endTime() = 13740; actor.employment().timeCanSignElsewhere() = GetWorldTotalMin();
		actor.combatResult().currentAttacker() = NOBODY;
		actor.morale().morale() = 80;
		auto& profile = gMercProfiles[profileId];
		profile.Type = PROFILETYPE_AIM; profile.ubBodyType = REGMALE;
		profile.bLife = profile.bLifeMax = 80;
		profile.bMercStatus = slot == 2 ? MERC_HIRED_BUT_NOT_ARRIVED_YET : 7;
		CHECK(AdoptJa2TacticalEntity(actor), "real actor identity adopted");
		if (slot != 2) CHECK(AddCharacterToSquad(&actor, FIRST_SQUAD), "ordinary actor enters actual native squad and heap-owned group");
	}
	auto& alive = *repository.resolve(0);
	auto& dead = *repository.resolve(1);
	auto& pending = *repository.resolve(2);
	pending.deployment().setUseLandingZoneForArrival(true);
	pending.deployment().strategicInsertionCode() = INSERTION_CODE_ARRIVING_GAME;
	pending.deployment().strategicInsertionData() = 0;
	pending.deployment().arrivalTime() = 2250;
	pending.employment().medicalDeposit() = gMercProfiles[6].sMedicalDepositAmount = 321;
	pending.employment().insuranceStartDay() = pending.employment().insuranceLengthDays() = 0;
	auto& queue = GetJa2CampaignEventQueue(); queue.clear();
	const auto flight = AddStrategicEventUsingSecondsChecked(EVENT_DELAYED_HIRING_OF_MERC, 2250 * 60, 2);
	CHECK(flight && queue.size() == 1, "Grunty has the exact native pending arrival event");
	LaptopSaveInfo.iCurrentBalance = 0;
	CHECK(AddTransactionToPlayersBookChecked(ANONYMOUS_DEPOSIT, 0, GetWorldTotalMin(), 13640).succeeded(), "private finance ledger seeded");
	if (failures) return 1;
	GROUP* const group = GetGroup(alive.deployment().groupId());
	CHECK(group && group->ubGroupSize == 2 && group->pPlayerList && group->pPlayerList->next,
		"fixture uses actual heap-owned native membership for both ordinary actors");
	if (!group) return 1;
	const auto groupId = GetJa2StrategicGroupId(group->ubGroupID);
	const auto deadId = GetJa2TacticalEntityId(dead);
	const auto pendingBytes = Representation(pending); const auto pendingProfile = Representation(gMercProfiles[6]);
	const auto clock = CaptureJa2CampaignClock();
	const auto flightId = flight.event->id; const auto flightSnapshot = flight.event->snapshot();
	const auto nextEvent = queue.nextIdentity();
	const auto finance = files.bytes("finances.dat");
	const auto deaths = gStrategicStatus.ubMercDeaths;
	const auto contract = dead.employment().endTime();
	CHECK(NumberOfMercsOnPlayerTeam() == 3, "native merc count includes all three retained roster records");
	dead.vitals().health() = 0;
	dead.status().flags() |= SOLDIER_DEAD; // Ordinary tactical death precedes skull completion.
	dead.uiPresentation().queueDeadMercUi();
	FinishAnySkullPanelAnimations();
	CHECK(dead.roster().active() && GetJa2TacticalEntityId(dead) == deadId &&
		dead.assignment().current() == ASSIGNMENT_DEAD && (dead.status().flags() & SOLDIER_DEAD) &&
		!dead.vitals().breath() && !dead.vitals().maximumBreath() &&
		!dead.uiPresentation().deadMercUiPending() && !dead.uiPresentation().panelClosingForDeath() &&
		gMercProfiles[238].bMercStatus == MERC_IS_DEAD && gMercProfiles[238].bLife == 80 &&
		dead.employment().endTime() == contract && NumberOfMercsOnPlayerTeam() == 3,
		"real skull completion retains active actor, native AIM contract and unchanged historical profile life");
	CHECK(!dead.deployment().groupId() && group->ubGroupSize == 1 && group->pPlayerList && !group->pPlayerList->next &&
		GetPlayerGroupMemberActor(group->pPlayerList) == GetJa2TacticalEntityId(alive) &&
		Ja2StrategicSquadSize(FIRST_SQUAD) == 1 && GetJa2StrategicSquadActor(FIRST_SQUAD, 0) == GetJa2TacticalEntityId(alive) && IsDeadGuyOnAnySquad(&dead),
		"native cleanup removes live group/squad membership and preserves the historical dead-squad profile record");
	bool exactHistoryName = false;
	for (const auto& entry : std::filesystem::directory_iterator(files.root / "TEMP"))
		exactHistoryName = exactHistoryName || entry.path().filename().string() == "History.dat";
	CHECK(exactHistoryName, "native history filename has the exact portable casing even on a case-insensitive host");
	const auto history = files.bytes("History.dat");
	CampaignLedgerRecord::History deathRecord;
	std::size_t offset = 0;
	const bool decoded = CampaignLedgerRecord::HistoryFields(deathRecord, [&](void* destination, std::size_t size) {
		if (offset + size > history.size()) return false;
		std::memcpy(destination, history.data() + offset, size); offset += size; return true;
	});
	CHECK(decoded && history.size() == CampaignLedgerRecord::HistoryRecordBytes &&
		deathRecord.code == HISTORY_MERC_KILLED && deathRecord.secondCode == 238 &&
		deathRecord.date == 1870 && deathRecord.sectorX == 9 && deathRecord.sectorY == 1 &&
		gStrategicStatus.ubMercDeaths == deaths + 1, "actual strategic death appends exactly one correct native history row");
	const auto unchangedPending = [&] {
		return Representation(pending) == pendingBytes && Representation(gMercProfiles[6]) == pendingProfile &&
			queue.size() == 1 && queue.head() == flight.event && flight.event->id == flightId &&
			flight.event->snapshot() == flightSnapshot && queue.nextIdentity() == nextEvent &&
			CaptureJa2CampaignClock() == clock && LaptopSaveInfo.iCurrentBalance == 13640 &&
			files.bytes("finances.dat") == finance;
	};
	CHECK(unchangedPending(), "native death leaves Grunty, exact delayed event, clock, funds and contracts unchanged");
	for (unsigned attempt = 0; attempt < 8; ++attempt) FinishAnySkullPanelAnimations();
	CHECK(unchangedPending() && files.bytes("History.dat") == history && gStrategicStatus.ubMercDeaths == deaths + 1,
		"completed native death cannot repeat history, native death count or pending-hire effects");
	SetPendingNewScreen(NO_PENDING_SCREEN);
	NotifyJa2TacticalWorldUnloaded(); ClearJa2TacticalWorldSector();
	UnLockPauseState(); PauseGame(); StopTimeCompression();
	using State = DedicatedCoopStarterCampaignState;
	const auto inspect = [&](State expected, const char* reason) {
		const auto deadBefore = Representation(dead); const auto profileBefore = Representation(gMercProfiles[238]);
		const auto aliveBefore = Representation(alive); const auto groupBefore = Representation(*group);
		CHECK(InspectDedicatedCoopStarterCampaign() == expected, reason);
		CHECK(Representation(dead) == deadBefore && Representation(gMercProfiles[238]) == profileBefore &&
			Representation(alive) == aliveBefore && Representation(*group) == groupBefore &&
			GetJa2StrategicGroupId(group->ubGroupID) == groupId && unchangedPending() &&
			files.bytes("History.dat") == history && !IsJa2TacticalWorldLoaded(),
			"Resume inspection is read-only across living/dead/pending native state and all ledgers");
	};
	inspect(State::EstablishedStrategicCold, "real completed death plus healthy ordinary member and pending Grunty resume paused");
	for (int change = 0; change < 12; ++change)
	{
		switch (change)
		{
		case 0: dead.status().flags() &= ~SOLDIER_DEAD; break;
		case 1: dead.assignment().current() = FIRST_SQUAD; break;
		case 2: gMercProfiles[238].bMercStatus = 7; break;
		case 3: dead.vitals().health() = 1; break;
		case 4: dead.deployment().groupId() = group->ubGroupID; break;
		case 5: dead.deployment().betweenSectors() = TRUE; break;
		case 6: dead.uiPresentation().queueDeadMercUi(); break;
		case 7: dead.identity().incarnation()++; break;
		case 8: dead.identity().profile() = NO_PROFILE; break;
		case 9: dead.vitals().maximumBreath() = 100; break;
		case 10: dead.deployment().sectorX() = 0; break;
		case 11: dead.employment().endTime() = -1; break;
		}
		inspect(State::Ineligible, "incomplete death, stale identity, unsafe deployment and retained UI cannot hide in dead cohort");
		dead.status().flags() |= SOLDIER_DEAD; dead.assignment().current() = ASSIGNMENT_DEAD;
		gMercProfiles[238].bMercStatus = MERC_IS_DEAD; dead.vitals().health() = 0;
		dead.deployment().groupId() = 0; dead.deployment().betweenSectors() = FALSE;
		dead.uiPresentation().finishDeathUi(); dead.identity().incarnation() = deadId.incarnation;
		dead.identity().profile() = 238; dead.vitals().maximumBreath() = 0;
		dead.deployment().sectorX() = 9; dead.employment().endTime() = contract;
	}
	CHECK(AddJa2StrategicSquadActor(3, deadId) == 0, "insert contradictory real live squad membership");
	inspect(State::Ineligible, "completed dead actor cannot retain live squad membership");
	CHECK(RemoveJa2StrategicSquadActor(3, deadId), "remove contradictory squad membership");
	GROUP hidden{}; PLAYERGROUP hiddenMember{};
	hidden.ubGroupID = 250; hidden.usGroupTeam = OUR_TEAM; hidden.ubGroupSize = 1;
	hidden.pPlayerList = &hiddenMember; hiddenMember.actor = deadId; ++hiddenMember.actor.incarnation;
	hiddenMember.ubProfileID = 238; hidden.next = gpGroupList; gpGroupList = &hidden;
	inspect(State::Ineligible, "groupId zero cannot conceal a stale same-slot player group membership");
	gpGroupList = hidden.next;
	inspect(State::EstablishedStrategicCold, "removing contradictory metadata restores the unchanged valid cold campaign");
	if (capture)
	{
		// The real capture performs assignment, squad removal, placement, healing
		// and quest start. Only the installed quest script has a minimal fixture.
		SetPendingNewScreen(MAINMENU_SCREEN);
		SetJa2TacticalWorldSector(9, 1, 0);
		gModSettings.ubInitialPOWSectorX = 13; gModSettings.ubInitialPOWSectorY = 9;
		for (unsigned index = 0; index < 3; ++index)
		{
			gModSettings.iInitialPOWGridNo[index] = 1000 + index;
			gModSettings.iInitialPOWItemGridNo[index] = 2000 + index;
		}
		gStrategicStatus.uiFlags &= ~(STRATEGIC_PLAYER_CAPTURED_FOR_RESCUE | STRATEGIC_PLAYER_CAPTURED_FOR_ESCAPE);
		gubQuest[QUEST_HELD_IN_ALMA] = gubQuest[QUEST_HELD_IN_TIXA] = gubQuest[QUEST_INTERROGATION] = QUESTNOTSTARTED;
		// A second independently identified native actor exercises distinct prison
		// positions in one completed capture sequence.
		gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{3};
		auto& otherPrisoner = *repository.resolve(3);
		otherPrisoner.identity().id() = SoldierID{3}; otherPrisoner.identity().incarnation() = 104;
		otherPrisoner.identity().profile() = 22; otherPrisoner.identity().bodyType() = REGMALE;
		otherPrisoner.roster().active() = TRUE; otherPrisoner.roster().team() = OUR_TEAM;
		otherPrisoner.roster().inSector() = TRUE; otherPrisoner.deployment().setSector(9, 1, 0);
		otherPrisoner.assignment().current() = FIRST_SQUAD;
		otherPrisoner.vitals().health() = otherPrisoner.vitals().maximumHealth() = 80;
		otherPrisoner.vitals().breath() = otherPrisoner.vitals().maximumBreath() = 100;
		otherPrisoner.employment().mercenaryType() = MERC_TYPE__AIM_MERC;
		otherPrisoner.employment().totalLength() = 7; otherPrisoner.employment().endTime() = 13740;
		otherPrisoner.combatResult().currentAttacker() = NOBODY;
		gMercProfiles[22].Type = PROFILETYPE_AIM; gMercProfiles[22].ubBodyType = REGMALE;
		gMercProfiles[22].bLife = gMercProfiles[22].bLifeMax = 80; gMercProfiles[22].bMercStatus = 7;
		CHECK(AdoptJa2TacticalEntity(otherPrisoner) && AddCharacterToSquad(&otherPrisoner, FIRST_SQUAD),
			"second capture participant has a real independent identity and heap-owned native membership");
		BeginCaptureSquence(); EnemyCapturesPlayerSoldier(&alive); EnemyCapturesPlayerSoldier(&otherPrisoner); EndCaptureSequence();
		SetPendingNewScreen(NO_PENDING_SCREEN); NotifyJa2TacticalWorldUnloaded(); ClearJa2TacticalWorldSector();
		UnLockPauseState(); PauseGame(); StopTimeCompression();
		CHECK(alive.assignment().current() == ASSIGNMENT_POW && !alive.deployment().groupId() &&
			!alive.roster().inSector() && alive.deployment().sectorX() == 13 && alive.deployment().sectorY() == 9 &&
			alive.vitals().health() == 40 && alive.vitals().breath() == 50 &&
			gStrategicStatus.ubNumCapturedForRescue == 2 && gubQuest[QUEST_HELD_IN_ALMA] == QUESTINPROGRESS &&
			otherPrisoner.assignment().current() == ASSIGNMENT_POW &&
			otherPrisoner.deployment().strategicInsertionData() == 1001 &&
			alive.deployment().strategicInsertionData() == 1000,
			"actual native capture removes live membership, places and heals the prisoner, and starts rescue");
		const auto captureHistory = files.bytes("History.dat");
		CHECK(captureHistory.size() == history.size() + CampaignLedgerRecord::HistoryRecordBytes &&
			captureHistory.compare(0, history.size(), history) == 0 && unchangedPending(),
			"native capture adds one quest history row and preserves the exact paid hire and ledgers");
		const auto inspectPrisoner = [&](State expected) {
			const auto actorBytes = Representation(alive); const auto profileBytes = Representation(gMercProfiles[21]);
			const auto otherBytes = Representation(otherPrisoner); const auto otherProfile = Representation(gMercProfiles[22]);
			const auto deadBytes = Representation(dead); const auto strategicBytes = Representation(gStrategicStatus);
			const auto randomState = GetGameSimulationRandomSource()->checkpoint();
			const auto questBytes = Representation(gubQuest);
			CHECK(InspectDedicatedCoopStarterCampaign() == expected, "cold capture classifier recognizes only the exact supported native cohort");
			CHECK(Representation(alive) == actorBytes && Representation(gMercProfiles[21]) == profileBytes &&
				Representation(dead) == deadBytes && Representation(gStrategicStatus) == strategicBytes &&
				Representation(otherPrisoner) == otherBytes && Representation(gMercProfiles[22]) == otherProfile &&
				GetGameSimulationRandomSource()->checkpoint() == randomState && Representation(gubQuest) == questBytes && unchangedPending() &&
				files.bytes("History.dat") == captureHistory && !IsJa2TacticalWorldLoaded(),
				"capture inspection cannot replay capture, alter a prisoner, redraw RNG or touch pending arrival and history");
		};
		for (unsigned attempt = 0; attempt < 4; ++attempt) inspectPrisoner(State::EstablishedStrategicCold);
		const auto prisoner = Representation(alive);
		const auto flags = gStrategicStatus.uiFlags;
		for (unsigned change = 0; change < 17; ++change)
		{
			switch (change)
			{
			case 0: alive.assignment().current() = FIRST_SQUAD; break;
			case 1: alive.deployment().sectorX() = 12; break;
			case 2: alive.deployment().strategicInsertionData() = 999; break;
			case 3: alive.roster().inSector() = TRUE; break;
			case 4: alive.aiBehavior().neutral() = FALSE; break;
			case 5: alive.status().flags() |= SOLDIER_DEAD; break;
			case 6: alive.uiPresentation().queueDeadMercUi(); break;
			case 7: alive.identity().incarnation()++; break;
			case 8: gStrategicStatus.ubNumCapturedForRescue = 3; break;
			case 9: gStrategicStatus.uiFlags |= STRATEGIC_PLAYER_CAPTURED_FOR_ESCAPE; break;
			case 10: gubQuest[QUEST_HELD_IN_ALMA] = QUESTNOTSTARTED; break;
			case 11: gubQuest[QUEST_HELD_IN_TIXA] = QUESTINPROGRESS; break;
			case 12: alive.vitals().bleeding() = 1; break;
			case 13: alive.deployment().groupId() = 250; break;
			case 14: gModSettings.iInitialPOWGridNo[1] = 1000; break;
			case 15: gMercProfiles[21].ubMiscFlags |= PROFILE_MISC_FLAG_EPCACTIVE; break;
			case 16: gMercProfiles[21].bMercStatus = MERC_IS_DEAD; break;
			}
			inspectPrisoner(State::Ineligible);
			// Restore fields changed by this negative case, without cloning an actor.
			alive.assignment().current() = ASSIGNMENT_POW; alive.deployment().sectorX() = 13;
			alive.deployment().strategicInsertionData() = 1000; alive.roster().inSector() = FALSE;
			alive.aiBehavior().neutral() = TRUE; alive.status().flags() &= ~SOLDIER_DEAD;
			alive.uiPresentation().finishDeathUi(); alive.identity().incarnation() = 101;
			gStrategicStatus.ubNumCapturedForRescue = 2; gStrategicStatus.uiFlags = flags;
			gubQuest[QUEST_HELD_IN_ALMA] = QUESTINPROGRESS; gubQuest[QUEST_HELD_IN_TIXA] = QUESTNOTSTARTED;
			alive.vitals().bleeding() = 0; alive.deployment().groupId() = 0;
			gModSettings.iInitialPOWGridNo[1] = 1001;
			gMercProfiles[21].ubMiscFlags &= ~PROFILE_MISC_FLAG_EPCACTIVE;
			gMercProfiles[21].bMercStatus = 7;
		}
		CHECK(Representation(alive) == prisoner, "negative checks restore the actual native prisoner without reconstruction");
		CHECK(AddJa2StrategicSquadActor(3, GetJa2TacticalEntityId(alive)) == 0, "insert contradictory prisoner squad membership");
		inspectPrisoner(State::Ineligible);
		CHECK(RemoveJa2StrategicSquadActor(3, GetJa2TacticalEntityId(alive)), "remove contradictory prisoner membership");
		inspectPrisoner(State::EstablishedStrategicCold);
		otherPrisoner.deployment().strategicInsertionData() = 1000;
		inspectPrisoner(State::Ineligible);
		otherPrisoner.deployment().strategicInsertionData() = 1001;
		inspectPrisoner(State::EstablishedStrategicCold);
		(void)ReleaseJa2TacticalEntity(otherPrisoner);
		otherPrisoner.roster().active() = FALSE;
	}
	else
	{
		// A second real skull completion leaves no living squad member, but the
		// already-paid delayed arrival still belongs to this exact cold campaign.
		SetPendingNewScreen(MAINMENU_SCREEN);
		alive.vitals().health() = 0;
		alive.status().flags() |= SOLDIER_DEAD;
		alive.uiPresentation().queueDeadMercUi();
		FinishAnySkullPanelAnimations();
		SetPendingNewScreen(NO_PENDING_SCREEN);
		CHECK(alive.assignment().current() == ASSIGNMENT_DEAD && alive.deployment().groupId() == 0 &&
			!alive.uiPresentation().deadMercUiPending() && !alive.uiPresentation().panelClosingForDeath() &&
			gStrategicStatus.ubMercDeaths == deaths + 2 && unchangedPending(),
			"last ordinary merc completes native death while paid Grunty and his exact event remain intact");
		const auto allDeadHistory = files.bytes("History.dat");
		CHECK(allDeadHistory.size() == 2 * CampaignLedgerRecord::HistoryRecordBytes &&
			allDeadHistory.compare(0, history.size(), history) == 0,
			"last native death appends one additional history record");
		const auto allDeadActor = Representation(alive);
		const auto allDeadProfile = Representation(gMercProfiles[21]);
		for (unsigned attempt = 0; attempt < 4; ++attempt)
		{
			CHECK(InspectDedicatedCoopStarterCampaign() == State::EstablishedStrategicCold,
				"completed dead roster plus exact paid pending hire resumes paused without a living squad");
			CHECK(Representation(alive) == allDeadActor && Representation(gMercProfiles[21]) == allDeadProfile &&
				unchangedPending() && files.bytes("History.dat") == allDeadHistory &&
				gStrategicStatus.ubMercDeaths == deaths + 2 && !IsJa2TacticalWorldLoaded(),
				"dead-only pending-hire inspection neither repairs gameplay nor repeats native death effects");
		}
	}
	RemoveAllGroups(); ResetJa2StrategicSquadRosters();
	queue.clear();
	for (unsigned slot = 0; slot < 3; ++slot) (void)ReleaseJa2TacticalEntity(*repository.resolve(slot));
	ResetJa2TacticalActorRosters();
	ShutdownFileManager(); ShutdownMemoryManager();
	std::printf("native pending hire with completed death: %d failures\n", failures);
	return failures ? 1 : 0;
}
