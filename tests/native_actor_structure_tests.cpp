// Data-free regression for the native structure lookup reached during ambush
// enemy insertion. Invalid directions/short structure tables must not index
// outside their arrays or remove an actor's existing footprint.
#include "types.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "SoldierRepository.h"
#include "Animation Data.h"
#include "Animation Control.h"
#include "Isometric Utils.h"
#include "worlddef.h"
#include "worldman.h"
#include "World Tile Map.h"
#include "structure.h"
#include "MemMan.h"
#include <cstdio>
#include <cstdlib>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }

int main()
{
	int failures = 0;
	const auto check = [&](bool condition, const char* message)
	{ if (!condition) { ++failures; std::printf("FAIL: %s\n", message); } };
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	auto& actor = *repository.resolve(0);
	actor.identity().id() = SoldierID{0};
	actor.identity().bodyType() = REGMALE;
	LEVELNODE node{};
	STRUCTURE existing{};
	node.pStructureData = &existing;
	actor.renderBindings().levelNode() = &node;
	actor.status().flags() = SOLDIER_MULTITILE;
	check(!AddMercStructureInfoFromAnimSurface(0, nullptr, 0, STANDING), "null actor rejects");
	for (unsigned direction = NUM_WORLD_DIRECTIONS; direction <= 255; ++direction)
	{
		actor.position().direction() = static_cast<UINT8>(direction);
		check(!AddMercStructureInfoFromAnimSurface(0, &actor, 0, STANDING), "invalid live direction rejects");
		check(node.pStructureData == &existing && actor.status().flags() == SOLDIER_MULTITILE,
			"invalid direction preserves existing structure and flags");
	}

	STRUCTURE_FILE_REF file{};
	DB_STRUCTURE_REF refs[NUM_WORLD_DIRECTIONS]{};
	gAnimSurfaceDatabase[0].bStructDataType = S_STRUCT;
	gAnimStructureDatabase[REGMALE][S_STRUCT].pStructureFileRef = &file;
	for (unsigned direction = 0; direction < NUM_WORLD_DIRECTIONS; ++direction)
	{
		actor.position().direction() = static_cast<UINT8>(direction);
		file.pDBStructureRef = refs;
		file.usNumberOfStructures = gOneCDirection[direction]; // selected index is one past end
		check(!AddMercStructureInfoFromAnimSurface(0, &actor, 0, STANDING), "short directional table rejects");
		check(node.pStructureData == &existing && actor.status().flags() == SOLDIER_MULTITILE,
			"short table does not change existing footprint or flags");
		file.usNumberOfStructures = NUM_WORLD_DIRECTIONS;
		file.pDBStructureRef = nullptr;
		check(!AddMercStructureInfoFromAnimSurface(0, &actor, 0, STANDING), "missing directional table rejects");
		check(node.pStructureData == &existing && actor.status().flags() == SOLDIER_MULTITILE,
			"missing table does not change existing footprint or flags");
	}
	actor.identity().bodyType() = QUEENMONSTER;
	gAnimStructureDatabase[QUEENMONSTER][S_STRUCT].pStructureFileRef = &file;
	file.pDBStructureRef = refs;
	file.usNumberOfStructures = 0;
	check(!AddMercStructureInfoFromAnimSurface(0, &actor, 0, STANDING), "queen single-direction table must exist");
	check(node.pStructureData == &existing, "missing queen structure preserves footprint");

	// Positive controls use the real structure allocator/world links, without
	// assets. Every octant must select the native rotated variant; the queen
	// must continue using variant zero even when its live facing is not zero.
	check(InitializeMemoryManager() && AllocateWorldTileMap(WORLD_MAX), "logical structure world allocates");
	if (!gpWorldLevelData) return 1;
	DB_STRUCTURE definition{};
	DB_STRUCTURE_TILE tile{};
	DB_STRUCTURE_TILE* tiles[] = {&tile};
	definition.ubNumberOfTiles = 1;
	definition.fFlags = STRUCTURE_PERSON;
	for (auto& ref : refs) { ref.pDBStructure = &definition; ref.ppTile = tiles; }
	node.pStructureData = nullptr;
	node.uiFlags = LEVELNODE_SOLDIER;
	node.pSoldier = &actor;
	actor.identity().bodyType() = REGMALE;
	file.usNumberOfStructures = NUM_WORLD_DIRECTIONS;
	const INT32 grid = WORLD_MAX / 2;
	const auto place = [&](UINT16 index)
	{
		check(AddMercStructureInfoFromAnimSurface(grid, &actor, 0, STANDING), "valid native structure inserts");
		check(node.pStructureData && node.pStructureData->pDBStructureRef == &refs[index] &&
			gpWorldLevelData[grid].pStructureHead == node.pStructureData, "native structure variant and world link match");
		DeleteStructureFromWorld(node.pStructureData);
		node.pStructureData = nullptr;
		check(gpWorldLevelData[grid].pStructureHead == nullptr, "native footprint removal leaves no world link");
	};
	for (unsigned direction = 0; direction < NUM_WORLD_DIRECTIONS; ++direction)
	{
		actor.position().direction() = static_cast<UINT8>(direction);
		place(gOneCDirection[direction]);
	}
	actor.identity().bodyType() = QUEENMONSTER;
	file.usNumberOfStructures = 1;
	place(0);
	ReleaseWorldTileMap();
	actor.renderBindings().levelNode() = nullptr;
	gAnimStructureDatabase[REGMALE][S_STRUCT].pStructureFileRef = nullptr;
	gAnimStructureDatabase[QUEENMONSTER][S_STRUCT].pStructureFileRef = nullptr;
	std::printf("Native actor structure bounds: %s\n", failures ? "FAIL" : "PASS");
	return failures ? 1 : 0;
}
