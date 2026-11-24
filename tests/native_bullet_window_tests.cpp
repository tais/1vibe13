// A native window partner swap frees the current collision structure. The
// projectile must keep moving without reusing that pointer for shield checks.
#include "types.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "SoldierRepository.h"
#include "MemMan.h"
#include "World Tile Map.h"
#include "worlddef.h"
#include "worldman.h"
#include "renderworld.h"
#include "structure.h"
#include "Isometric Utils.h"
#include "Bullets.h"
#include "TacticalWorldAdapter.h"
#include "Overhead.h"
#include "strategicmap.h"
#include "PATHAI.H"
#include <vfs/Core/vfs.h>
#include <vfs/Core/Location/vfs_directory_tree.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <initializer_list>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (false)
struct Files
{
    std::filesystem::path root = std::filesystem::temp_directory_path() /
        ("ja2-bullet-window-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Files()
    {
        std::filesystem::create_directories(root / "TEMP");
        auto* profile = new vfs::CVirtualProfile(L"_BULLET_WINDOW_TEST", vfs::Path(root.c_str()), true);
        getVFS()->getProfileStack()->pushProfile(profile);
        auto* tree = new vfs::CDirectoryTree(vfs::Path(""), vfs::Path(root.c_str()));
        CHECK(tree->init(), "private native directory initialized");
        profile->addLocation(tree);
        CHECK(getVFS()->addLocation(tree, profile), "private native VFS mounted");
    }
    ~Files()
    {
        vfs::CVirtualFileSystem::shutdownVFS();
        std::error_code ignored; std::filesystem::remove_all(root, ignored);
    }
};
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    Files files;
    auto& game = GetGameContext();
    CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
        "native runtime starts");
    GetJa2SoldierRepository().initializeSlots();
    CHECK(InitializeMemoryManager() && AllocateWorldTileMap(WORLD_MAX), "native world allocates");
    gubWorldMovementCosts = static_cast<UINT8 (*)[MAXDIR][2]>(MemAlloc(static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2));
    if (failures || !gubWorldMovementCosts) return 1;
    std::memset(gubWorldMovementCosts, TRAVELCOST_BLOCKED, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
    InitRenderParams(0);
    SetJa2TacticalWorldSector(9, 1, 0);
    const INT32 grid = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS / 2;
    CHECK(GridNoOnVisibleWorldTile(grid), "collision tile is inside the native visible world");
    if (failures) return 1;
    for (auto orientation : {INSIDE_TOP_RIGHT, OUTSIDE_TOP_RIGHT, INSIDE_TOP_LEFT, OUTSIDE_TOP_LEFT})
    for (int direction : {-1, 1})
    {
        DB_STRUCTURE definitions[2]{};
        DB_STRUCTURE_TILE tile{};
        DB_STRUCTURE_TILE* tiles[] = {&tile};
        DB_STRUCTURE_REF refs[2]{};
        std::memset(tile.Shape, 0xff, sizeof(tile.Shape));
        for (unsigned i = 0; i < 2; ++i)
        {
            definitions[i].ubNumberOfTiles = 1;
            definitions[i].fFlags = STRUCTURE_WALLNWINDOW;
            definitions[i].ubWallOrientation = orientation;
            definitions[i].ubHitPoints = 100;
            // Screen windows take the real partner-swap path without needing
            // installed shatter animation or audio assets.
            definitions[i].ubArmour = MATERIAL_THICKER_METAL_WITH_SCREEN_WINDOWS;
            refs[i].pDBStructure = &definitions[i]; refs[i].ppTile = tiles;
        }
        definitions[0].bPartnerDelta = 1;
        LEVELNODE node{}; node.usIndex = 1;
        gpWorldLevelData[grid].pStructHead = &node;
        CHECK(AddStructureToWorld(grid, 0, &refs[0], &node), "real window structure inserts");
        if (!node.pStructureData) return 1;
        const UINT16 oldId = node.pStructureData->usStructureID;
        const INT32 index = CreateBullet(NOBODY, FALSE, 0, 0);
        CHECK(index >= 0, "native projectile allocated");
        if (index < 0) return 1;
        BULLET& bullet = *GetBulletPtr(index);
        bullet.iCurrTileX = grid % WORLD_COLS; bullet.iCurrTileY = grid / WORLD_COLS;
        bullet.sGridNo = bullet.sOrigGridNo = bullet.sTargetGridNo = grid;
        bullet.qCurrX = INT32_TO_FIXEDPT(bullet.iCurrTileX * CELL_X_SIZE + CELL_X_SIZE / 2);
        bullet.qCurrY = INT32_TO_FIXEDPT(bullet.iCurrTileY * CELL_Y_SIZE + CELL_Y_SIZE / 2);
        bullet.qCurrZ = INT32_TO_FIXEDPT((WINDOW_BOTTOM_HEIGHT_UNITS + WINDOW_TOP_HEIGHT_UNITS) / 2);
        bullet.iCurrCubesZ = CONVERT_HEIGHTUNITS_TO_INDEX(FIXEDPT_TO_INT32(bullet.qCurrZ));
        bullet.bLOSIndexX = FIXEDPT_TO_LOS_INDEX(bullet.qCurrX);
        bullet.bLOSIndexY = FIXEDPT_TO_LOS_INDEX(bullet.qCurrY);
        const bool horizontal = orientation == INSIDE_TOP_RIGHT || orientation == OUTSIDE_TOP_RIGHT;
        bullet.qIncrX = horizontal ? INT32_TO_FIXEDPT(direction) : 0;
        bullet.qIncrY = horizontal ? 0 : INT32_TO_FIXEDPT(direction);
        bullet.ubTilesPerUpdate = 1; bullet.iRange = bullet.iDistanceLimit = 100;
        bullet.iImpact = 50;
        MoveBullet(index);
        CHECK(node.pStructureData && node.pStructureData->pDBStructureRef == &refs[1] &&
            node.usIndex == 2 && FindStructureByID(grid, oldId) == nullptr,
            "native collision replaces and frees the original window");
        CHECK(bullet.fAllocated && !bullet.fToDelete && !(bullet.usFlags & BULLET_STOPPED) &&
            bullet.sGridNo == grid + direction * (horizontal ? 1 : WORLD_COLS) && bullet.iLoop > 0,
            "projectile continues through the window into the next tile");
        RemoveBullet(index);
        bullet.fAllocated = FALSE;
        CHECK(DeleteStructureFromWorld(node.pStructureData), "replacement window removed");
        gpWorldLevelData[grid].pStructHead = nullptr;
    }
    MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr;
    ReleaseWorldTileMap();
    std::printf("Native bullet window lifetime: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
