// Real native scene publication, skip consequences, strategic groups/events
// and a partially applied quest failure. No callback or effect is substituted.
#include "DedicatedCoopMeanwhile.h"
#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "GameInitOptionsScreen.h"
#include "CampaignClockAdapter.h"
#include "CampaignEventAdapter.h"
#include "Game Clock.h"
#include "Game Event Hook.h"
#include "Meanwhile.h"
#include "Strategic Movement.h"
#include "Strategic AI.h"
#include "StrategicGroupHost.h"
#include "strategicmap.h"
#include "strategic.h"
#include "Quests.h"
#include "SoldierRepository.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "TacticalEntityHost.h"
#include "TacticalActor.h"
#include "TacticalWorldAdapter.h"
#include "Overhead.h"
#include "connect.h"
#include "gameloop.h"
#include "screenids.h"
#include "FileMan.h"
#include "MemMan.h"
#include "random.h"
#include <Engine/Core/SimulationRandom.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1); }
extern BOOLEAN gfQueenAIAwake, gfUnlimitedTroops, gfMassFortificationOrdered;
extern INT32 giReinforcementPool;
extern UINT8 gubMinEnemyGroupSize;
extern INT16 sWorldSectorLocationOfFirstBattle;

int main(int argc, char** argv)
{
	const bool stale = argc == 2 && std::strcmp(argv[1], "--stale") == 0;
	const bool staleAfter = argc == 2 && std::strcmp(argv[1], "--stale-after-ack") == 0;
	const bool nativeFailure = argc == 2 && std::strcmp(argv[1], "--native-failure") == 0;
	if (argc > 2 || (argc == 2 && !stale && !staleAfter && !nativeFailure)) return 2;
	int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (0)
	std::setbuf(stdout, nullptr);
	using Result = DedicatedCoopMeanwhileResult;
	DedicatedServerOptions options; options.enabled = true; options.mode = DedicatedServerMode::Coop;
	InstallDedicatedServerOptions(options);
	CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native runtime starts");
	CHECK(InitializeMemoryManager(), "native allocator starts");
	if (failures) return 1;
	const auto root = std::filesystem::temp_directory_path() /
		("ja2-meanwhile-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root / "TEMP");
	std::filesystem::create_directories(root / "scripts");
	{
		std::ofstream script(root / "scripts" / "Quests.lua");
		script << "function InternalStartQuest(q,x,y,history) SetQuest(q,1) end\n";
		CHECK(script.good(), "minimal quest content written");
	}
	auto* profile = new vfs::CVirtualProfile(L"_MEANWHILE_TEST", vfs::Path(root.c_str()), true);
	getVFS()->getProfileStack()->pushProfile(profile);
	auto* tree = new vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str()));
	CHECK(tree->init(), "private native VFS initialized"); profile->addLocation(tree);
	CHECK(getVFS()->addLocation(tree, profile) && InitializeFileManager(nullptr), "private native file service mounted");
	if (failures) return 1;
	GetJa2SoldierRepository().initializeSlots(); ResetJa2TacticalActorRosters();
	gbPlayerNum = OUR_TEAM;
	for (auto& team : gTacticalStatus.Team) team.bFirstID = team.bLastID = SoldierID{0};
	GetJa2SoldierRepository().resolve(0)->roster().team() = OUR_TEAM;
	NotifyJa2TacticalWorldUnloaded(); ClearJa2TacticalWorldSector();
	RestoreJa2TacticalTurnState(0, OUR_TEAM, 0);
	InitializeJa2CampaignClock(112560);
	[[maybe_unused]] auto screen = OverrideCurrentScreen(MAP_SCREEN);
	gGameOptions.ubDifficultyLevel = DIF_LEVEL_EASY;
	gGameExternalOptions.iMaxEnemyGroupSize = 20;
	zDiffSetting[DIF_LEVEL_EASY].bStrategicAiActionWakeQueen = TRUE;
	gGameExternalOptions.fASDAssignsTanks = gGameExternalOptions.fASDAssignsJeeps = gGameExternalOptions.fASDAssignsRobots = FALSE;
	gModSettings.ubSAISpawnSectorX = 9; gModSettings.ubSAISpawnSectorY = 2;
	sWorldSectorLocationOfFirstBattle = CALCULATE_STRATEGIC_INDEX(9, 1);
	gubMinEnemyGroupSize = 4; giReinforcementPool = 100; gfUnlimitedTroops = FALSE;
	// This minimal campaign has already ordered its town fortifications; the
	// first-battle response still wakes the Queen and sends an actual new group.
	gfQueenAIAwake = FALSE; gfMassFortificationOrdered = TRUE;
	for (auto& sector : SectorInfo)
	{
		sector.ubGarrisonID = NO_GARRISON;
		for (auto& terrain : sector.ubTraversability) terrain = ROAD;
	}
	const UINT8 scene = nativeFailure ? AWOL_SCIENTIST : END_OF_PLAYERS_FIRST_BATTLE;
	gMeanwhileDef[scene] = {3, 16, 0, scene, QUEEN};
	CHECK(BeginMeanwhile(scene) && gfMeanwhileTryingToStart && GamePaused() && PauseStateLocked(), "real BeginMeanwhile retains its pause lock");
	DedicatedCoopMeanwhileState state, other;
	CHECK(!DeferDedicatedCoopMeanwhile() && !DedicatedCoopMeanwhilePending(), "unbound native presentation remains ordinary");
	CHECK(BindDedicatedCoopMeanwhileState(state) && BindDedicatedCoopMeanwhileState(state) && !BindDedicatedCoopMeanwhileState(other),
		"only one runtime owns scene continuations");
	options.enabled = false; InstallDedicatedServerOptions(options);
	CHECK(!DeferDedicatedCoopMeanwhile() && !state.pending(), "single player retains its local presentation");
	options.enabled = true; options.mode = DedicatedServerMode::Pvp; InstallDedicatedServerOptions(options);
	CHECK(!DeferDedicatedCoopMeanwhile() && !state.pending(), "dedicated PvP does not use co-op decisions");
	options.mode = DedicatedServerMode::Coop; InstallDedicatedServerOptions(options);
	is_networked = true;
	CHECK(!DeferDedicatedCoopMeanwhile() && !state.pending(), "legacy networking remains separate");
	is_networked = false;
	const auto randomBefore = GetGameSimulationRandomSource()->checkpoint();
	CHECK(gpGroupList == nullptr && GetJa2CampaignEventQueue().empty(), "native fixture starts without movement groups or events");
	CheckForMeanwhileOKStart();
	CHECK(state.pending() && !state.failure() && DedicatedCoopMeanwhilePending() && gfMeanwhileTryingToStart && !gfInMeanwhile,
		"actual native pre-cinematic hook holds the scene without opening a local dialog");
	if (!state.pending()) return 1;
	const auto notice = *state.pending();
	CHECK(notice.scene == scene && notice.worldSeconds == 112560 && !notice.worldLoaded &&
		notice.sceneX == 3 && notice.sceneY == 16 && notice.npc == QUEEN,
		"notice retains the native scene definition, clock and world context");
	CheckForMeanwhileOKStart();
	CHECK(state.pending()->id == notice.id && !state.failure(), "repeated pre-cinematic checks retain the same decision");
	CHECK(!gpGroupList && !gfQueenAIAwake && giReinforcementPool == 100 &&
		GetGameSimulationRandomSource()->checkpoint() == randomBefore, "holding a scene does not apply or reroll its consequences");
	CHECK(CompleteDedicatedCoopMeanwhile(notice.id) == Result::NotPending &&
		AcknowledgeDedicatedCoopMeanwhile(notice.id + 1) == Result::StaleNotice,
		"effects require the exact pending explicit acknowledgement");
	if (stale)
	{
		++gCurrentMeanwhileDef.usTriggerEvent;
		CHECK(AcknowledgeDedicatedCoopMeanwhile(notice.id) == Result::NativeContextChanged && !state.acknowledged(),
			"changed scene metadata cannot be skipped by an earlier request");
		CHECK(DeferDedicatedCoopMeanwhile() && state.failure() && state.pending()->id == notice.id,
			"overlapping scenes fail without replacing the retained context");
	}
	else
	{
		CHECK(AcknowledgeDedicatedCoopMeanwhile(notice.id) == Result::Applied && state.acknowledged() &&
			gfMeanwhileTryingToStart && !gpGroupList && GetGameSimulationRandomSource()->checkpoint() == randomBefore,
			"acknowledgement leaves effects held until receipt delivery");
		CHECK(AcknowledgeDedicatedCoopMeanwhile(notice.id) == Result::NotPending &&
			CompleteDedicatedCoopMeanwhile(notice.id + 1) == Result::StaleNotice,
			"duplicate acknowledgement and foreign completion cannot apply effects");
		if (staleAfter)
		{
			NotifyJa2TacticalWorldLoaded(99);
			CHECK(CompleteDedicatedCoopMeanwhile(notice.id) == Result::Failed && state.failure() && !gpGroupList,
				"a replacement native world after acknowledgement prevents completion");
		}
		else if (nativeFailure)
		{
			// The native AWOL branch begins the quest, then asserts if content
			// defines no alternate scientist map. Keep that genuine partial failure.
			gModSettings.ubMeanwhileAddMadlabSector1X = gModSettings.ubMeanwhileAddMadlabSector2X =
				gModSettings.ubMeanwhileAddMadlabSector3X = gModSettings.ubMeanwhileAddMadlabSector4X = 1;
			gModSettings.ubMeanwhileAddMadlabSector1Y = gModSettings.ubMeanwhileAddMadlabSector2Y =
				gModSettings.ubMeanwhileAddMadlabSector3Y = gModSettings.ubMeanwhileAddMadlabSector4Y = 1;
			CHECK(CompleteDedicatedCoopMeanwhile(notice.id) == Result::Failed && state.failure() && !state.pending() &&
				gubQuest[QUEST_FIND_SCIENTIST] == QUESTINPROGRESS && GamePaused(),
				"partial native quest consequence consumes the decision and latches failure");
		}
		else
		{
			CHECK(CompleteDedicatedCoopMeanwhile(notice.id) == Result::Applied && !state.pending() && !state.failure() &&
				!gfMeanwhileTryingToStart && !gfInMeanwhile && GamePaused() && !PauseStateLocked() && !IsTimeBeingCompressed(),
				"native Skip finishes and releases only its scene lock, leaving campaign time paused");
			CHECK(gfQueenAIAwake && giReinforcementPool == 92 && gpGroupList && !gpGroupList->next &&
				gpGroupList->usGroupTeam == ENEMY_TEAM && gpGroupList->ubGroupSize == 8 && gpGroupList->fBetweenSectors &&
				gpGroupList->ubSectorX == 9 && gpGroupList->ubSectorY == 2 && gpGroupList->ubNextX == 9 && gpGroupList->ubNextY == 1,
				"real first-battle consequences wake the Queen and pay for one eight-soldier response to A9");
			const auto arrivals = GetAllStrategicEventsOfType(EVENT_GROUP_ARRIVAL);
			CHECK(arrivals.size() == 1 && gpGroupList && arrivals.front().second == gpGroupList->ubGroupID,
				"response travels through a real native group-arrival event");
			const auto randomAfter = GetGameSimulationRandomSource()->checkpoint();
			CHECK(randomAfter != randomBefore && GetWorldTotalSeconds() == notice.worldSeconds,
				"consequences use gameplay RNG without advancing campaign time");
			CHECK(AcknowledgeDedicatedCoopMeanwhile(notice.id) == Result::NotPending &&
				CompleteDedicatedCoopMeanwhile(notice.id) == Result::NotPending && giReinforcementPool == 92 &&
				GetAllStrategicEventsOfType(EVENT_GROUP_ARRIVAL) == arrivals && GetGameSimulationRandomSource()->checkpoint() == randomAfter,
				"replayed acknowledgement/completion cannot dispatch troops or reroll effects");
			gMeanwhileDef[FLOWERS] = {3, 16, 1, FLOWERS, QUEEN};
			CHECK(BeginMeanwhile(FLOWERS), "later independent scene begins"); CheckForMeanwhileOKStart();
			CHECK(state.pending() && state.pending()->id > notice.id && !state.failure(), "later scene gets a fresh identity");
			if (state.pending())
			{
				const auto later = state.pending()->id;
				CHECK(AcknowledgeDedicatedCoopMeanwhile(later) == Result::Applied && CompleteDedicatedCoopMeanwhile(later) == Result::Applied &&
					giReinforcementPool == 92 && GetGameSimulationRandomSource()->checkpoint() == randomAfter,
					"later native scene completes without repeating first-battle consequences");
			}
		}
	}
	if (stale || staleAfter || nativeFailure)
	{
		CHECK(AcknowledgeDedicatedCoopMeanwhile(notice.id) == Result::Failed &&
			CompleteDedicatedCoopMeanwhile(notice.id) == Result::Failed && DedicatedCoopMeanwhilePending(),
			"failed continuation stays held and cannot be retried");
		UnbindDedicatedCoopMeanwhileState(other);
		CHECK(DedicatedCoopMeanwhilePending(), "foreign teardown cannot release a failed continuation");
	}
	UnbindDedicatedCoopMeanwhileState(state);
	CHECK(!DedicatedCoopMeanwhilePending(), "runtime teardown releases only its binding");
	RemoveAllGroups(); GetJa2CampaignEventQueue().clear();
	ShutdownFileManager(); vfs::CVirtualFileSystem::shutdownVFS();
	std::error_code ignored; std::filesystem::remove_all(root, ignored);
	std::printf("native co-op meanwhile: %d failures\n", failures);
	return failures ? 1 : 0;
}
