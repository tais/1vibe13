// Exercise native blood-loss death callbacks while finishing a won battle.
// History is read from the actual private VFS ledger, without installed assets.
#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "GameInitOptionsScreen.h"
#include "Game Clock.h"
#include "CampaignClockAdapter.h"
#include "CampaignLedgerRecord.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "SoldierRepository.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "Overhead.h"
#include "Animation Data.h"
#include "Animation Control.h"
#include "Assignments.h"
#include "Squads.h"
#include "Interface.h"
#include "Interface Control.h"
#include "strategicmap.h"
#include "strategic.h"
#include "Strategic Status.h"
#include "Queen Command.h"
#include "PreBattle Interface.h"
#include "Quests.h"
#include "history.h"
#include "screenids.h"
#include "gameloop.h"
#include "World Tile Map.h"
#include "worlddef.h"
#include "MemMan.h"
#include "FileMan.h"
#include "random.h"
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

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
	std::setbuf(stdout, nullptr);
	DedicatedServerOptions options; options.enabled = true; options.mode = DedicatedServerMode::Coop;
	InstallDedicatedServerOptions(options);
	CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native campaign runtime starts");
	CHECK(InitializeMemoryManager() && AllocateWorldTileMap(WORLD_MAX), "logical tactical world allocated");
	if (failures) return 1;
	const auto root = std::filesystem::temp_directory_path() /
		("ja2-battle-resolution-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root / "TEMP");
	std::filesystem::create_directories(root / "scripts");
	// No campaign-specific liberation rewards in this minimal content set.
	// The native sector-control and history paths still execute normally.
	{
		std::ofstream script(root / "scripts" / "strategicmap.lua");
		script << "function HandleSectorLiberation(x,y,z,first) end\n";
		CHECK(script.good(), "minimal campaign liberation content written");
	}
	auto* profile = new vfs::CVirtualProfile(L"_BATTLE_RESOLUTION_TEST", vfs::Path(root.c_str()), true);
	getVFS()->getProfileStack()->pushProfile(profile);
	auto* tree = new vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str()));
	CHECK(tree->init(), "private native VFS initialized"); profile->addLocation(tree);
	CHECK(getVFS()->addLocation(tree, profile) && InitializeFileManager(nullptr), "private native file service mounted");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	gbPlayerNum = OUR_TEAM;
	for (auto& team : gTacticalStatus.Team) { team.bFirstID = SoldierID{3}; team.bLastID = SoldierID{3}; }
	gTacticalStatus.Team[OUR_TEAM].bFirstID = gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{0};
	gTacticalStatus.Team[ENEMY_TEAM].bFirstID = SoldierID{1}; gTacticalStatus.Team[ENEMY_TEAM].bLastID = SoldierID{2};
	gTacticalStatus.fDidGameJustStart = FALSE;
	gGameOptions.fNewTraitSystem = FALSE; gGameOptions.ubSquadSize = 6;
	gGameOptions.ubDifficultyLevel = DIF_LEVEL_EASY;
	zDiffSetting[DIF_LEVEL_EASY].iNumKillsPerProgressPoint = 10;
	NUMBER_OF_SAMS = 1; gpSamSectorX[0] = 2; gpSamSectorY[0] = 4;
	gGameExternalOptions.fDynamicOpinions = FALSE;
	gGameExternalOptions.fStandUpAfterBattle = FALSE;
	gGameSettings.fOptions[TOPTION_BLOOD_N_GORE] = FALSE;
	InitializeJa2CampaignClock(112500);
	SetJa2TacticalWorldSector(5, 5, 0); NotifyJa2TacticalWorldLoaded(1);
	// San Mona uses its native loyalty exemption; this fixture needs no Lua
	// content to exercise enemy cleanup and the actual battle history writer.
	StrategicMap[CALCULATE_STRATEGIC_INDEX(5, 5)].bNameId = SAN_MONA;
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	[[maybe_unused]] auto screen = OverrideCurrentScreen(GAME_SCREEN);
	gusSelectedSoldier = NOBODY; guiTacticalInterfaceFlags = 0;
	for (unsigned slot = 0; slot < 3; ++slot)
	{
		auto& actor = *repository.resolve(slot);
		actor.identity().id() = SoldierID{static_cast<UINT16>(slot)};
		actor.identity().incarnation() = slot + 1;
		actor.identity().profile() = slot ? NO_PROFILE : 21;
		actor.identity().bodyType() = REGMALE;
		actor.roster().active() = actor.roster().inSector() = TRUE;
		actor.roster().team() = slot ? ENEMY_TEAM : OUR_TEAM;
		actor.roster().side() = slot ? 1 : 0;
		actor.aiBehavior().neutral() = FALSE;
		actor.deployment().setSector(5, 5, 0);
		actor.assignment().current() = FIRST_SQUAD;
		actor.vitals().health() = 80;
		actor.vitals().maximumHealth() = 80;
		actor.vitals().breath() = actor.vitals().maximumBreath() = 100;
		actor.position().gridNo() = WORLD_MAX / 2 + slot;
		actor.collapseState().tactical() = slot != 0;
		actor.animationPlayback().state() = STANDING;
		actor.combatResult().currentAttacker() = actor.combatResult().previousAttacker() = actor.combatResult().earlierAttacker() = NOBODY;
		CHECK(AdoptJa2TacticalEntity(actor), "native actor identity adopted");
	}
	gMercProfiles[21].bLife = gMercProfiles[21].bLifeMax = 80;
	gTacticalStatus.fEnemyInSector = TRUE;
	gTacticalStatus.bNumFoughtInBattle[ENEMY_TEAM] = 2;
	SetEnemyEncounterCode(ENEMY_INVASION_CODE);
	CHECK(SetTacticalTeamPopulation(OUR_TEAM, 1, TRUE) && SetTacticalTeamPopulation(ENEMY_TEAM, 2, TRUE),
		"native team populations set");
	SectorInfo[SECTOR(5, 5)].ubNumTroops = SectorInfo[SECTOR(5, 5)].ubTroopsInBattle = 2;
	CHECK(!CheckForEndOfBattle(FALSE) && IsJa2TacticalCombatActive(), "capable enemies keep the battle open");
	for (unsigned slot = 1; slot <= 2; ++slot) repository.resolve(slot)->vitals().health() = 1;
	CHECK(NumCapableEnemyInSector() == 0, "both enemies are incapacitated before native completion");
	CHECK(CheckForEndOfBattle(FALSE), "real native battle completion succeeds");
	CHECK(!IsJa2TacticalCombatActive() && !gTacticalStatus.fEnemyInSector && gTacticalStatus.fLastBattleWon,
		"native victory exits combat");
	CHECK(!CheckForEndOfBattle(FALSE), "later ordinary check cannot repeat a completed battle");
	std::ifstream file(root / "TEMP" / "History.dat", std::ios::binary);
	CHECK(file.is_open(), "native history ledger exists");
	const std::string bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	CHECK(bytes.size() == CampaignLedgerRecord::HistoryRecordBytes,
		"exactly one native history record is committed for the battle");
	CampaignLedgerRecord::History record; std::size_t position = 0;
	CHECK(CampaignLedgerRecord::HistoryFields(record, [&](void* field, std::size_t count) {
		if (position + count > bytes.size()) return false;
		std::memcpy(field, bytes.data() + position, count); position += count; return true;
	}) && record.code == HISTORY_DEFENDEDTOWNSECTOR && record.date == 1875 &&
		record.sectorX == 5 && record.sectorY == 5 && record.sectorZ == 0 && !record.secondCode && !record.color,
		"native history records this exact successful town defense");
	CHECK(repository.resolve(1)->vitals().health() == 0 && repository.resolve(2)->vitals().health() == 0 &&
		!NumCapableEnemyInSector() && !SectorInfo[SECTOR(5, 5)].ubNumTroops && !SectorInfo[SECTOR(5, 5)].ubTroopsInBattle,
		"all incapacitated enemies finish native death and strategic cleanup");
	CHECK(repository.resolve(0)->vitals().health() == 80, "surviving player remains healthy");
	// A later encounter in the same process must still resolve. No defeated
	// enemy is resurrected, and the first battle's ledger remains intact.
	gTacticalStatus.fEnemyInSector = TRUE;
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	CHECK(CheckForEndOfBattle(TRUE), "resolution ownership is released for a later enemy retreat");
	std::ifstream laterFile(root / "TEMP" / "History.dat", std::ios::binary);
	const std::string later{std::istreambuf_iterator<char>(laterFile), std::istreambuf_iterator<char>()};
	CHECK(later.size() == 2 * CampaignLedgerRecord::HistoryRecordBytes && later.substr(0, bytes.size()) == bytes,
		"later encounter appends exactly one result without rewriting the first");
	ShutdownFileManager(); vfs::CVirtualFileSystem::shutdownVFS(); ReleaseWorldTileMap();
	std::error_code ignored; std::filesystem::remove_all(root, ignored);
	std::printf("native battle resolution: %d failures\n", failures);
	return failures ? 1 : 0;
}
