#define SDL_MAIN_HANDLED

#include <SDL3/SDL.h>

#include "types.h"
#include "Buildings.h"
#include "FileMan.h"
#include "FullEngineCoopClientPresentationStaticGeometry.h"
#include "MemMan.h"
#include "PATHAI.H"
#include "Render Fun.h"
#include "TacticalWorldAdapter.h"
#include "TileDat.h"
#include "Tile Surface.h"
#include "World Tile Map.h"
#include "WorldDat.h"
#include "ai.h"
#include "environment.h"
#include "lighting.h"
#include "renderworld.h"
#include "structure.h"
#include "tiledef.h"
#include "worlddef.h"

#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <vector>

// The application shell normally owns these. This focused executable links
// the production JA2 archives without sgp.cpp, matching the other headless
// engine tests.
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE;
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE;
BOOLEAN gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = { 0 };
extern BOOLEAN RandomSector[256];

extern UINT8 *gubGridNoMarkers;
extern INT32 dirDelta[8];
extern INT16 DirIncrementer[8];
extern INT8 gbNewTileSurfaceLoaded[NUMBEROFTILETYPES];

void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}

namespace
{
int failures = 0;
int movementCostCallbacks = 0;

void Check(bool condition, const char* message)
{
	if (condition)
		std::printf("ok    %s\n", message);
	else
	{
		++failures;
		std::printf("FAIL  %s\n", message);
	}
	std::fflush(stdout);
}

void CountMovementCostCallback()
{
	++movementCostCallbacks;
}

bool DirectionTablesUseColumnStride(INT32 columns)
{
	const std::array<INT32, 8> expected{
		-columns, 1 - columns, 1, 1 + columns,
		columns, columns - 1, -1, -columns - 1};
	for (std::size_t direction = 0; direction < expected.size(); ++direction)
		if (dirDelta[direction] != expected[direction] ||
			DirIncrementer[direction] != expected[direction])
			return false;
	return true;
}

LEVELNODE* AllocateNode()
{
	LEVELNODE* const node = static_cast<LEVELNODE*>(MemAlloc(sizeof(LEVELNODE)));
	if (node) std::memset(node, 0, sizeof(*node));
	return node;
}

template <typename Value>
void Append(std::vector<std::uint8_t>& bytes, const Value& value)
{
	const std::size_t offset = bytes.size();
	bytes.resize(offset + sizeof(value));
	std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void AppendZeroes(std::vector<std::uint8_t>& bytes, std::size_t count)
{
	bytes.resize(bytes.size() + count, 0);
}

TacticalSectorSnapshot PresentationSector()
{
	TacticalSectorSnapshot sector{9, 10, 0, true};
	Check(AssignTacticalMapAssetKey(
		sector.mapAssetKey, "A9.dat", sizeof("A9.dat")),
		"synthetic presentation plan has a valid exact map identity");
	return sector;
}

std::vector<std::uint8_t> CompleteStaticPresentationMap(
	bool invalidLandTile = false, bool legacyRooms = false)
{
	std::vector<std::uint8_t> bytes;
	const float major = legacyRooms ? 5.0f : 8.0f;
	const std::uint8_t minor = legacyRooms ? 25 : 31;
	const std::int32_t rows = legacyRooms ? OLD_WORLD_ROWS : 1;
	const std::int32_t columns = legacyRooms ? OLD_WORLD_COLS : 2;
	const std::size_t cells = static_cast<std::size_t>(rows) * columns;
	Append(bytes, major);
	Append(bytes, minor);
	if (!legacyRooms)
	{
		Append(bytes, rows);
		Append(bytes, columns);
	}
	const std::uint32_t flags = 0x00000084u; // ambient + map lights
	const std::int32_t tileset = 0;
	const std::uint32_t legacySoldierSize = 0;
	Append(bytes, flags);
	Append(bytes, tileset);
	Append(bytes, legacySoldierSize);
	const std::size_t heights = bytes.size();
	AppendZeroes(bytes, cells * 2);
	bytes[heights] = 7;
	bytes[heights + 1] = 27; // value present in installed A9.DAT
	bytes[heights + 2] = 11;
	// Cell 0: saved map flags 0x3 and one entry in every static layer.
	const std::size_t counts = bytes.size();
	AppendZeroes(bytes, cells * 4);
	bytes[counts] = 0x31;
	bytes[counts + 1] = 0x11;
	bytes[counts + 2] = 0x11;
	bytes[counts + 3] = 0x01;
	const std::uint8_t type = 0;
	const std::uint8_t subIndex = invalidLandTile ? 2 : 1;
	Append(bytes, type);
	Append(bytes, subIndex); // land
	Append(bytes, type);
	const std::uint16_t objectSubIndex = 1;
	Append(bytes, objectSubIndex);
	for (std::size_t layer = 0; layer < 4; ++layer)
	{
		Append(bytes, type);
		const std::uint8_t one = 1;
		Append(bytes, one);
	}
	if (legacyRooms)
	{
		const std::size_t rooms = bytes.size();
		AppendZeroes(bytes, cells);
		bytes[rooms] = 30;
		bytes[rooms + 1] = 40;
	}
	else
	{
		const std::size_t rooms = bytes.size();
		AppendZeroes(bytes, cells * 2);
		const std::uint16_t firstRoom = 300;
		const std::uint16_t secondRoom = 400;
		std::memcpy(bytes.data() + rooms, &firstRoom, sizeof(firstRoom));
		std::memcpy(bytes.data() + rooms + sizeof(firstRoom),
			&secondRoom, sizeof(secondRoom));
	}
	const std::uint8_t basement = 1;
	const std::uint8_t caves = 0;
	const std::uint8_t staleMapAmbient = 4;
	Append(bytes, basement);
	Append(bytes, caves);
	Append(bytes, staleMapAmbient);
	const std::uint8_t lightColors = 1;
	const SGPPaletteEntry mapLightColor{17, 9, 3, 0};
	const std::uint16_t mapLights = 0;
	Append(bytes, lightColors);
	Append(bytes, mapLightColor);
	Append(bytes, mapLights);
	const std::size_t mapInfo = bytes.size();
	AppendZeroes(bytes, legacyRooms ? 100 : 32);
	if (legacyRooms)
	{
		const std::int16_t legacyNowhere = -1;
		for (std::size_t index = 0; index < 4; ++index)
			std::memcpy(bytes.data() + mapInfo + index * 2,
				&legacyNowhere, sizeof(legacyNowhere));
		std::memcpy(bytes.data() + mapInfo + 12,
			&legacyNowhere, sizeof(legacyNowhere));
		std::memcpy(bytes.data() + mapInfo + 14,
			&legacyNowhere, sizeof(legacyNowhere));
		bytes[mapInfo + 9] = 25;
	}
	else
	{
		for (std::size_t index = 0; index < 6; ++index)
		{
			const std::int32_t nowhere = -1;
			std::memcpy(bytes.data() + mapInfo + index * sizeof(nowhere),
				&nowhere, sizeof(nowhere));
		}
		bytes[mapInfo + 26] = 31;
	}
	return bytes;
}

FullEngineCoopClientPresentationMapPlan PresentationPlan(
	bool invalidLandTile = false, bool legacyRooms = false)
{
	FullEngineCoopClientPresentationMapPlan plan;
	auto bytes = CompleteStaticPresentationMap(invalidLandTile, legacyRooms);
	Check(ParseFullEngineCoopClientPresentationMap(
		PresentationSector(), legacyRooms
			? TacticalWorldDimensions{OLD_WORLD_COLS, OLD_WORLD_ROWS}
			: TacticalWorldDimensions{2, 1},
		std::move(bytes), {}, plan) ==
		FullEngineCoopClientPresentationMapPlanResult::Success,
		"synthetic complete map parses into a retained replay plan");
	return plan;
}

bool NodeHasAuthorityShade(const LEVELNODE* node, std::uint8_t shade)
{
	return node != nullptr && node->ubNaturalShadeLevel == shade &&
		node->ubShadeLevel == shade;
}

std::vector<std::uint32_t> CaptureStaticGeometryCells()
{
	std::vector<std::uint32_t> values;
	values.push_back(WORLD_ROWS);
	values.push_back(WORLD_COLS);
	values.push_back(ubAmbientLightLevel);
	for (INT32 grid = 0; grid < WORLD_MAX; ++grid)
	{
		const auto& cell = GetMapElement(grid);
		values.push_back(static_cast<std::uint8_t>(cell.sHeight));
		values.push_back(cell.ubAdjacentSoldierCnt);
		values.push_back(cell.uiFlags);
		values.push_back(gusWorldRoomInfo[grid]);
		const std::array<const LEVELNODE*, 6> layers{{cell.pLandHead,
			cell.pObjectHead, cell.pStructHead, cell.pShadowHead,
			cell.pRoofHead, cell.pOnRoofHead}};
		for (const LEVELNODE* node : layers)
		{
			for (; node; node = node->pNext)
			{
				values.push_back(node->usIndex);
				values.push_back(node->ubNaturalShadeLevel);
				values.push_back(node->ubShadeLevel);
				values.push_back(node->uiFlags);
			}
			values.push_back(UINT32_MAX);
		}
	}
	return values;
}

void TestProductionStaticGeometryReplay()
{
	TrashStaticPresentationWorld();
	const UINT8 savedSetCount = gubNumSets;
	const INT32 savedTileTypes = giNumberOfTileTypes;
	const INT32 savedTileCount = giNumberOfTiles;
	const INT32 savedTileset = giCurrentTilesetID;
	const UINT16 savedDatabaseSize = gTileDatabaseSize;
	const UINT16 savedTypeStart = gTileTypeStartIndex[0];
	const UINT16 savedTypeCount = gNumTilesPerType[0];
	const TILE_ELEMENT savedTile = gTileDatabase[0];
	const UINT8 savedAmbient = ubAmbientLightLevel;
	SGPPaletteEntry savedLightColors[2]{};
	const UINT8 savedLightColorCount = LightGetColors(savedLightColors);
	const BOOLEAN savedBasement = gfBasement;
	const BOOLEAN savedCaves = gfCaves;
	const UINT32 savedRenderFlags = GetRenderFlags();
	const LIGHT_SPRITE savedFirstLight = LightSprites[0];

	gubNumSets = 1;
	giNumberOfTileTypes = 1;
	giNumberOfTiles = 1;
	gTileDatabaseSize = 1;
	gTileTypeStartIndex[0] = 0;
	gNumTilesPerType[0] = 1;
	gTileDatabase[0] = {};
	gTileDatabase[0].fType = 0;
	giCurrentTilesetID = 0; // keep this headless test independent of image data
	ubAmbientLightLevel = 6;
	gfBasement = FALSE;
	gfCaves = TRUE;
	RenderSetShadows(TRUE);
	LightSprites[0] = {};
	LightSprites[0].iX = 77;
	LightSprites[0].uiFlags = LIGHT_SPR_ACTIVE | LIGHT_SPR_ON;
	const UINT32 presentationRenderFlagsBefore = GetRenderFlags();

	UINT8 (*const movementCostsBefore)[MAXDIR][2] = gubWorldMovementCosts;
	UINT32* const pathingDataBefore = guiPathingData;
	UINT8* const gridMarkersBefore = gubGridNoMarkers;
	INT16* const coverBefore = gsCoverValue;
	UINT8* const buildingInfoBefore = gubBuildingInfo;
	const TacticalWorldSession::Snapshot sessionBefore =
		CaptureJa2TacticalWorld();

	FullEngineCoopClientPresentationMapPlan plan = PresentationPlan();
	FullEngineCoopClientPresentationStaticGeometry geometry;
	Check(gMapTrn.SetTrnPar(1, 1, 2, 2) &&
		geometry.load(plan, TacticalWorldLightingSnapshot{12}) ==
			FullEngineCoopClientPresentationStaticGeometryResult::
				MapTranslationActive &&
		!geometry.loaded() && !IsWorldPreparedForStaticPresentation(),
		"stale legacy map translation fails closed before direct-grid replay");
	gMapTrn.DisableTrn();
	Check(geometry.load(plan, TacticalWorldLightingSnapshot{0}) ==
		FullEngineCoopClientPresentationStaticGeometryResult::
			InvalidAuthorityLighting &&
		!IsWorldPreparedForStaticPresentation(),
		"invalid authority lighting fails before world mutation");

	const auto loaded = geometry.load(
		plan, TacticalWorldLightingSnapshot{12});
	const MAP_ELEMENT& first = GetMapElement(0);
	Check(loaded ==
		FullEngineCoopClientPresentationStaticGeometryResult::Success &&
		geometry.loaded() && geometry.omittedMapLightSprites() &&
		IsWorldPreparedForStaticPresentation() &&
		WORLD_ROWS == 1 && WORLD_COLS == 2,
		"production replay commits exact authority dimensions and records omitted map lights");
	Check(gTopLeftWorldLimitX == CELL_X_SIZE &&
		gTopLeftWorldLimitY == (WORLD_ROWS / 2) * CELL_X_SIZE &&
		gBottomRightWorldLimitX == WORLD_COLS * CELL_Y_SIZE &&
		gCenterWorldX == (WORLD_ROWS / 2) * CELL_X_SIZE &&
		gCenterWorldY == (WORLD_COLS / 2) * CELL_Y_SIZE,
		"renderer bounds and center refresh from the published presentation dimensions");
	Check(first.sHeight == 7 && first.ubAdjacentSoldierCnt == 0 &&
		GetMapElement(1).sHeight == 11 &&
		(first.uiFlags & 0x000f) == 0x0003 &&
		gusWorldRoomInfo[0] == 300 && gusWorldRoomInfo[1] == 400,
		"height low bytes and static metadata replay while saved actor adjacency is ignored");
	Check(first.pLandHead && first.pLandHead->usIndex == 0 &&
		first.pObjectHead && first.pObjectHead->usIndex == 0 &&
		first.pStructHead && first.pStructHead->usIndex == 0 &&
		first.pShadowHead && first.pShadowHead->usIndex == 0 &&
		first.pRoofHead && first.pRoofHead->usIndex == 0 &&
		first.pOnRoofHead && first.pOnRoofHead->usIndex == 0,
		"all six retained static layers replay through checked TileEngine insertions");
	Check(NodeHasAuthorityShade(first.pLandHead, 12) &&
		NodeHasAuthorityShade(first.pObjectHead, 12) &&
		NodeHasAuthorityShade(first.pStructHead, 12) &&
		NodeHasAuthorityShade(first.pRoofHead, 12) &&
		NodeHasAuthorityShade(first.pOnRoofHead, 12) &&
		ubAmbientLightLevel == 12 && gubNumLightColors == 1 &&
		gpLightColors[0].peRed == 17 && gpLightColors[0].peGreen == 9 &&
		gpLightColors[0].peBlue == 3 && gfBasement && !gfCaves,
		"map tile colour and replicated ambient initialize passive renderer shading");
	Check(geometry.synchronizeLighting(TacticalWorldLightingSnapshot{8}) ==
			FullEngineCoopClientPresentationStaticGeometryResult::Success &&
		NodeHasAuthorityShade(first.pLandHead, 8) &&
		NodeHasAuthorityShade(first.pStructHead, 8) &&
		ubAmbientLightLevel == 8 &&
		LightSprites[0].iX == 77 &&
		LightSprites[0].uiFlags == (LIGHT_SPR_ACTIVE | LIGHT_SPR_ON),
		"a later committed ambient refreshes natural tile shades without local light sprites");
	Check(geometry.synchronizeLighting(TacticalWorldLightingSnapshot{0}) ==
			FullEngineCoopClientPresentationStaticGeometryResult::
				InvalidAuthorityLighting &&
		ubAmbientLightLevel == 8 && NodeHasAuthorityShade(first.pLandHead, 8),
		"an invalid later ambient is rejected without changing the rendered world");
	Check(gubWorldRoomHidden[300] && gubWorldRoomHidden[400],
		"replayed rooms begin hidden for ordinary roof rendering");
	Check(gubWorldMovementCosts == movementCostsBefore &&
		guiPathingData == pathingDataBefore &&
		gubGridNoMarkers == gridMarkersBefore &&
		gsCoverValue == coverBefore && gubBuildingInfo == buildingInfoBefore &&
		!IsJa2TacticalWorldLoaded() &&
		CaptureJa2TacticalWorld().worldGeneration ==
			sessionBefore.worldGeneration &&
		CaptureJa2TacticalWorld().turnSerial == sessionBefore.turnSerial &&
		LightSprites[0].iX == 77 &&
		LightSprites[0].uiFlags == (LIGHT_SPR_ACTIVE | LIGHT_SPR_ON),
		"replay leaves navigation, simulation generation, and local light sprites untouched");
	LEVELNODE* const retainedLand = first.pLandHead;
	Check(geometry.load(plan, TacticalWorldLightingSnapshot{12}) ==
		FullEngineCoopClientPresentationStaticGeometryResult::AlreadyLoaded &&
		GetMapElement(0).pLandHead == retainedLand,
		"an accidental second load cannot replace committed geometry");

	geometry.teardown();
	Check(!geometry.loaded() && !geometry.omittedMapLightSprites() &&
		!IsWorldPreparedForStaticPresentation() &&
		GetMapElement(0).pLandHead == nullptr &&
		gusWorldRoomInfo[0] == 0 && gusWorldRoomInfo[1] == 0 &&
		ubAmbientLightLevel == 6 &&
		gubNumLightColors == savedLightColorCount &&
		gpLightColors[0].peRed == savedLightColors[0].peRed &&
		gpLightColors[0].peGreen == savedLightColors[0].peGreen &&
		gpLightColors[0].peBlue == savedLightColors[0].peBlue &&
		!gfBasement && gfCaves &&
		GetRenderFlags() == presentationRenderFlagsBefore,
		"teardown revokes geometry and restores the prior presentation environment");
	geometry.teardown();
	Check(!IsWorldPreparedForStaticPresentation(),
		"production static-geometry teardown is idempotent");

	FullEngineCoopClientPresentationMapPlan legacyRoomPlan =
		PresentationPlan(false, true);
	Check(geometry.load(legacyRoomPlan,
		TacticalWorldLightingSnapshot{9}) ==
		FullEngineCoopClientPresentationStaticGeometryResult::Success &&
		gusWorldRoomInfo[0] == 30 && gusWorldRoomInfo[1] == 40 &&
		GetMapElement(0).sHeight == 7 &&
		GetMapElement(1).sHeight == 11 && ubAmbientLightLevel == 9,
		"legacy UINT8 rooms replay alongside exact heights and authority ambient");
	const auto legacyGeometry = CaptureStaticGeometryCells();
	geometry.teardown();

	for (unsigned variant = 0; variant < 2; ++variant)
	{
		auto legacyBytes = CompleteStaticPresentationMap(false, true);
		const std::uint32_t flagsWithEdgepoints = 0x000000c4u;
		std::memcpy(legacyBytes.data() + FullEngineCoopClientLegacyMapHeaderBytes,
			&flagsWithEdgepoints, sizeof(flagsWithEdgepoints));
		for (unsigned array = 0; array < 8; ++array)
		{
			const std::uint16_t count = array == 0 ? 1 : 0;
			const std::uint16_t middle = UINT16_MAX;
			Append(legacyBytes, count);
			Append(legacyBytes, middle);
			if (count) Append(legacyBytes, static_cast<std::int16_t>(42));
		}
		if (variant != 0)
			for (unsigned byte = 0; byte < 498; ++byte)
				legacyBytes.push_back(static_cast<std::uint8_t>(byte * 37 + 1));
		FullEngineCoopClientPresentationMapPlan legacyPlan;
		Check(ParseFullEngineCoopClientPresentationMap(PresentationSector(),
			TacticalWorldDimensions{OLD_WORLD_COLS, OLD_WORLD_ROWS},
			std::move(legacyBytes), {}, legacyPlan) ==
			FullEngineCoopClientPresentationMapPlanResult::Success,
			"legacy native split metadata and nonzero editor remainder form a bounded plan");
		Check(geometry.load(legacyPlan, TacticalWorldLightingSnapshot{9}) ==
			FullEngineCoopClientPresentationStaticGeometryResult::Success &&
			CaptureStaticGeometryCells() == legacyGeometry,
			"unused legacy edgepoint splits and editor bytes produce identical geometry in every cell and layer");
		Check(gubWorldMovementCosts == movementCostsBefore &&
			guiPathingData == pathingDataBefore &&
			!IsJa2TacticalWorldLoaded() &&
			CaptureJa2TacticalWorld().worldGeneration == sessionBefore.worldGeneration &&
			CaptureJa2TacticalWorld().turnSerial == sessionBefore.turnSerial,
			"legacy compatibility does not import edgepoint gameplay state or create a tactical world");
		geometry.teardown();
	}

	FullEngineCoopClientPresentationMapPlan invalidTilePlan =
		PresentationPlan(true);
	Check(geometry.load(invalidTilePlan,
		TacticalWorldLightingSnapshot{10}) ==
		FullEngineCoopClientPresentationStaticGeometryResult::
			InvalidTileReference &&
		!geometry.loaded() && !IsWorldPreparedForStaticPresentation() &&
		GetMapElement(0).pLandHead == nullptr && ubAmbientLightLevel == 6 &&
		!gfBasement && gfCaves,
		"an unavailable type/subindex rolls back the whole presentation transaction");

	DB_STRUCTURE invalidDefinition{};
	DB_STRUCTURE_REF invalidReference{};
	DB_STRUCTURE_TILE invalidTiles[2]{};
	DB_STRUCTURE_TILE* invalidTilePointers[2] = {
		&invalidTiles[0], &invalidTiles[1]};
	invalidDefinition.ubNumberOfTiles = 2;
	invalidDefinition.fFlags = STRUCTURE_OBSTACLE;
	invalidDefinition.ubHitPoints = 100;
	invalidTiles[1].sPosRelToBase = 1;
	invalidTiles[1].bXPosRelToBase = 2;
	invalidReference.pDBStructure = &invalidDefinition;
	invalidReference.ppTile = invalidTilePointers;
	gTileDatabase[0].pDBStructureRef = &invalidReference;
	Check(geometry.load(plan, TacticalWorldLightingSnapshot{10}) ==
		FullEngineCoopClientPresentationStaticGeometryResult::
			LayerInsertionFailure &&
		!geometry.loaded() && !IsWorldPreparedForStaticPresentation() &&
		GetMapElement(0).pLandHead == nullptr &&
		GetMapElement(0).pObjectHead == nullptr &&
		GetMapElement(0).pStructureHead == nullptr,
		"a TileEngine structure insertion failure rolls back prior replayed layers");
	gTileDatabase[0].pDBStructureRef = nullptr;

	LightSprites[0] = savedFirstLight;
	ubAmbientLightLevel = savedAmbient;
	gfBasement = savedBasement;
	gfCaves = savedCaves;
	ClearRenderFlags(~static_cast<UINT32>(0));
	SetRenderFlags(savedRenderFlags);
	gTileDatabase[0] = savedTile;
	gTileTypeStartIndex[0] = savedTypeStart;
	gNumTilesPerType[0] = savedTypeCount;
	gTileDatabaseSize = savedDatabaseSize;
	giNumberOfTileTypes = savedTileTypes;
	giNumberOfTiles = savedTileCount;
	giCurrentTilesetID = savedTileset;
	gubNumSets = savedSetCount;
}

void TestTransactionalStaticShadeTables()
{
	TrashStaticPresentationWorld();
	const INT32 savedTileTypes = giNumberOfTileTypes;
	TILE_IMAGERY* const savedSurface = gTileSurfaceArray[0];
	std::array<INT8, NUMBEROFTILETYPES> savedNewSurfaceFlags{};
	std::memcpy(savedNewSurfaceFlags.data(), gbNewTileSurfaceLoaded,
		savedNewSurfaceFlags.size());
	const UINT32 savedRenderFlags = GetRenderFlags();
	SGPPaletteEntry savedOriginalColors[2]{};
	const UINT8 savedColorCount = LightGetColors(savedOriginalColors);
	SGPPaletteEntry savedDerivedColors[3]{};
	std::memcpy(savedDerivedColors, gpLightColors,
		sizeof(savedDerivedColors));

	std::array<SGPPaletteEntry, 256> palette{};
	for (std::size_t index = 0; index < palette.size(); ++index)
	{
		palette[index].peRed = static_cast<UINT8>(index);
		palette[index].peGreen = static_cast<UINT8>(255 - index);
		palette[index].peBlue = static_cast<UINT8>(index / 2);
	}
	SGPVObject object{};
	object.pPaletteEntry = palette.data();
	TILE_IMAGERY imagery{};
	imagery.vo = &object;
	giNumberOfTileTypes = 1;
	gTileSurfaceArray[0] = &imagery;

	const SGPPaletteEntry twoColors[2]{
		{13, 7, 3, 0}, {2, 11, 17, 0}};
	Check(PrepareWorldForStaticPresentation(1, 1) &&
		LightSetColorsForStaticPresentation(twoColors, 2),
		"two-colour static lighting builds a complete staged palette set");
	bool allTwoColorSlotsPresent = object.pShadeCurrent == object.pShades[4] &&
		object.pGlow == object.pShades[0];
	std::array<PIXEL*, HVOBJECT_SHADE_TABLES> committedShades{};
	for (std::size_t shade = 0; shade < committedShades.size(); ++shade)
	{
		committedShades[shade] = object.pShades[shade];
		allTwoColorSlotsPresent = allTwoColorSlotsPresent &&
			object.pShades[shade] != nullptr;
	}
	Check(allTwoColorSlotsPresent,
		"two-colour static lighting validates all 48 renderer shade slots");

	// A source-object failure is detected before any installed palette is
	// destroyed.  The same prepared world can then retry after its asset owner
	// becomes valid again.
	imagery.vo = nullptr;
	const SGPPaletteEntry retryColor{29, 19, 5, 0};
	const bool rejected = !LightSetColorsForStaticPresentation(&retryColor, 1);
	SGPPaletteEntry colorsAfterFailure[2]{};
	const UINT8 countAfterFailure = LightGetColors(colorsAfterFailure);
	bool installedTablesPreserved = object.pShadeCurrent == object.pShades[4] &&
		object.pGlow == object.pShades[0];
	for (std::size_t shade = 0; shade < committedShades.size(); ++shade)
		installedTablesPreserved = installedTablesPreserved &&
			object.pShades[shade] == committedShades[shade];
	Check(rejected && IsWorldPreparedForStaticPresentation() &&
		countAfterFailure == 2 &&
		std::memcmp(colorsAfterFailure, twoColors, sizeof(twoColors)) == 0 &&
		installedTablesPreserved,
		"failed static colour replacement preserves installed metadata and palettes");

	imagery.vo = &object;
	const bool retrySucceeded =
		LightSetColorsForStaticPresentation(&retryColor, 1) != FALSE;
	bool oneColorSlotsComplete = object.pShadeCurrent == object.pShades[4] &&
		object.pGlow == object.pShades[0];
	for (std::size_t shade = 0; shade < 16; ++shade)
		oneColorSlotsComplete = oneColorSlotsComplete &&
			object.pShades[shade] != nullptr;
	for (std::size_t shade = 16; shade < committedShades.size(); ++shade)
		oneColorSlotsComplete = oneColorSlotsComplete &&
			object.pShades[shade] == nullptr;
	Check(retrySucceeded && oneColorSlotsComplete,
		"static colour setup retries cleanly and replaces all prior colour slots");

	Check(LightSetColorsForStaticPresentation(
		savedOriginalColors, savedColorCount),
		"static shade-table test restores the prior lighting metadata");
	std::memcpy(gpLightColors, savedDerivedColors,
		sizeof(savedDerivedColors));
	(void)DestroyObjectPaletteTables(&object);
	TrashStaticPresentationWorld();
	gTileSurfaceArray[0] = savedSurface;
	giNumberOfTileTypes = savedTileTypes;
	std::memcpy(gbNewTileSurfaceLoaded, savedNewSurfaceFlags.data(),
		savedNewSurfaceFlags.size());
	ClearRenderFlags(~static_cast<UINT32>(0));
	SetRenderFlags(savedRenderFlags);
}

void TestFailedStaticTilesetLoadEntersRetryableColdState()
{
	TrashStaticPresentationWorld();
	const UINT8 savedSetCount = gubNumSets;
	const INT32 savedTileTypes = giNumberOfTileTypes;
	const INT32 savedTileCount = giNumberOfTiles;
	const TILESET_CALLBACK savedCallback = gTilesets[1].MovementCostFnc;
	std::array<CHAR8, 32> savedFilename{};
	std::memcpy(savedFilename.data(),
		gTilesets[1].TileSurfaceFilenames[0], savedFilename.size());

	gubNumSets = 2;
	// LoadTileSurfaces' legacy progress calculation requires at least two
	// configured types; the first missing asset still deterministically fails.
	giNumberOfTileTypes = 2;
	giNumberOfTiles = 1;
	giCurrentTilesetID = 0;
	gTileDatabaseSize = 1;
	gTileDatabase[0] = {};
	gTileDatabase[0].hTileSurface =
		reinterpret_cast<HVOBJECT>(static_cast<std::uintptr_t>(1));
	gTileTypeStartIndex[0] = 77;
	gbDefaultSurfaceUsed[0] = TRUE;
	gbSameAsDefaultSurfaceUsed[0] = TRUE;
	gbNewTileSurfaceLoaded[0] = TRUE;
	std::snprintf(gTilesets[1].TileSurfaceFilenames[0],
		sizeof(gTilesets[1].TileSurfaceFilenames[0]),
		"missing-static-tileset.sti");
	gTilesets[1].MovementCostFnc = &CountMovementCostCallback;
	movementCostCallbacks = 0;

	Check(PrepareWorldForStaticPresentation(1, 1) &&
		!LoadMapTilesetForStaticPresentation(1) &&
		IsWorldPreparedForStaticPresentation() &&
		giCurrentTilesetID == -1 && gTileDatabaseSize == 0 &&
		gTileDatabase[0].hTileSurface == nullptr &&
		gTileTypeStartIndex[0] == 0 && gTileSurfaceArray[0] == nullptr &&
		!gbDefaultSurfaceUsed[0] && !gbSameAsDefaultSurfaceUsed[0] &&
		!gbNewTileSurfaceLoaded[0] && movementCostCallbacks == 0,
		"failed passive tileset load publishes a pointer-free cold resource state");

	// A zero-type synthetic retry exercises the state transition without
	// requiring installed game assets; most importantly, the old id cannot
	// trigger InternalLoadMapTileset's same-id fast path.
	giNumberOfTileTypes = 0;
	giNumberOfTiles = 0;
	Check(LoadMapTilesetForStaticPresentation(1) &&
		giCurrentTilesetID == 1 && gTileDatabaseSize == 0 &&
		movementCostCallbacks == 0,
		"cold passive tileset state retries the same request without simulation callbacks");

	TrashStaticPresentationWorld();
	std::memcpy(gTilesets[1].TileSurfaceFilenames[0], savedFilename.data(),
		savedFilename.size());
	gTilesets[1].MovementCostFnc = savedCallback;
	gubNumSets = savedSetCount;
	giNumberOfTileTypes = savedTileTypes;
	giNumberOfTiles = savedTileCount;
	// Tile resources intentionally remain in the coherent cold state established
	// by the failure; restoring only an old id would recreate the bug under test.
	giCurrentTilesetID = -1;
}
}

int main()
{
	std::printf("== static_presentation_world_lifecycle_tests ==\n");
	const std::filesystem::path root = std::filesystem::temp_directory_path() /
		("ja2-static-presentation-" + std::to_string(
			static_cast<unsigned long long>(SDL_GetTicksNS())));
	std::error_code error;
	std::filesystem::create_directories(root, error);
	Check(!error, "temporary VFS root is available");

	vfs_init::VfsConfig config;
	vfs_init::Profile* profile = new vfs_init::Profile();
	profile->m_name = L"static-presentation-world-tests";
	profile->m_root = vfs::Path(root.generic_u8string());
	profile->m_writable = true;
	config.addProfile(profile, true);
	Check(vfs_init::initVirtualFileSystem(config),
		"writable VFS profile initializes");
	Check(InitializeMemoryManager(), "memory manager initializes");
	Check(InitializeFileManager(NULL), "FileMan initializes");

	TrashStaticPresentationWorld();
	ReleaseWorldTileMap();
	if (gusWorldRoomInfo)
	{
		MemFree(gusWorldRoomInfo);
		gusWorldRoomInfo = nullptr;
	}
	guiWorldRows = OLD_WORLD_ROWS;
	guiWorldCols = OLD_WORLD_COLS;
	ShutDownPathAI();

	MAP_ELEMENT* const initialWorld = gpWorldLevelData;
	UINT16* const initialRooms = gusWorldRoomInfo;
	const UINT32 initialStorageSize = GetWorldTileMapSize();
	Check(!PrepareWorldForStaticPresentation(0, 4) &&
		!PrepareWorldForStaticPresentation(4, 0) &&
		!PrepareWorldForStaticPresentation(WORLD_ROWS_MAX + 1, 4) &&
		!PrepareWorldForStaticPresentation(4, WORLD_COLS_MAX + 1) &&
		!IsWorldPreparedForStaticPresentation() &&
		gpWorldLevelData == initialWorld &&
		gusWorldRoomInfo == initialRooms &&
		GetWorldTileMapSize() == initialStorageSize &&
		WORLD_ROWS == OLD_WORLD_ROWS && WORLD_COLS == OLD_WORLD_COLS,
		"invalid authority dimensions fail without publishing partial storage");

	const TacticalWorldSession::Snapshot coldSession = CaptureJa2TacticalWorld();
	TacticalWorldSession::Snapshot loadedSession = coldSession;
	loadedSession.loaded = true;
	loadedSession.worldGeneration = 91;
	loadedSession.turnSerial = 1;
	RestoreJa2TacticalWorldSession(loadedSession);
	Check(!PrepareWorldForStaticPresentation(3, 4) &&
		!IsWorldPreparedForStaticPresentation() &&
		gpWorldLevelData == initialWorld && gusWorldRoomInfo == initialRooms,
		"a loaded gameplay world excludes passive presentation ownership");
	RestoreJa2TacticalWorldSession(coldSession);

	UINT8 (*const movementCostsBefore)[MAXDIR][2] = gubWorldMovementCosts;
	UINT32* const pathingDataBefore = guiPathingData;
	UINT8* const gridMarkersBefore = gubGridNoMarkers;
	INT16* const coverBefore = gsCoverValue;
	UINT8* const buildingInfoBefore = gubBuildingInfo;
	Check(PrepareWorldForStaticPresentation(3, 4) &&
		IsWorldPreparedForStaticPresentation() &&
		WORLD_ROWS == 3 && WORLD_COLS == 4 &&
		DirectionTablesUseColumnStride(4) &&
		GetWorldTileMapSize() == 12 && gpWorldLevelData != nullptr &&
		gusWorldRoomInfo != nullptr &&
		gubWorldMovementCosts == movementCostsBefore &&
		guiPathingData == pathingDataBefore &&
		gubGridNoMarkers == gridMarkersBefore &&
		gsCoverValue == coverBefore &&
		gubBuildingInfo == buildingInfoBefore &&
		!IsJa2TacticalWorldLoaded(),
		"presentation setup publishes rectangular renderer strides without navigation or gameplay arrays");

	MAP_ELEMENT* const preparedWorld = gpWorldLevelData;
	UINT16* const preparedRooms = gusWorldRoomInfo;
	preparedRooms[11] = 73;
	LEVELNODE* const retainedNode = AllocateNode();
	if (retainedNode)
	{
		GetMapElement(2).pObjectHead = retainedNode;
	}
	Check(retainedNode != nullptr &&
		PrepareWorldForStaticPresentation(3, 4) &&
		gpWorldLevelData == preparedWorld && gusWorldRoomInfo == preparedRooms &&
		GetMapElement(2).pObjectHead == retainedNode && preparedRooms[11] == 73,
		"repeating the exact prepare is idempotent and preserves replayed geometry");
	Check(!PrepareWorldForStaticPresentation(4, 3) &&
		gpWorldLevelData == preparedWorld && gusWorldRoomInfo == preparedRooms &&
		WORLD_ROWS == 3 && WORLD_COLS == 4 &&
		GetMapElement(2).pObjectHead == retainedNode && preparedRooms[11] == 73,
		"an active presentation rejects replacement dimensions transactionally");

	const UINT8 savedSetCount = gubNumSets;
	const INT32 savedTileTypes = giNumberOfTileTypes;
	const INT32 savedTileCount = giNumberOfTiles;
	const INT32 savedTileset = giCurrentTilesetID;
	const TILESET_CALLBACK savedCallback = gTilesets[0].MovementCostFnc;
	gubNumSets = 1;
	giNumberOfTileTypes = 0;
	giNumberOfTiles = 0;
	gTilesets[0].MovementCostFnc = &CountMovementCostCallback;
	giCurrentTilesetID = -1;
	movementCostCallbacks = 0;
	const bool presentationTilesetLoaded =
		LoadMapTilesetForStaticPresentation(0) != FALSE;
	const int presentationCallbackCount = movementCostCallbacks;
	giCurrentTilesetID = -1;
	const bool ordinaryTilesetLoaded = LoadMapTileset(0) != FALSE;
	Check(presentationTilesetLoaded && presentationCallbackCount == 0 &&
		ordinaryTilesetLoaded && movementCostCallbacks == 1,
		"presentation tilesets skip movement callbacks while ordinary loads retain them");
	gTilesets[0].MovementCostFnc = savedCallback;
	gubNumSets = savedSetCount;
	giNumberOfTileTypes = savedTileTypes;
	giNumberOfTiles = savedTileCount;
	giCurrentTilesetID = savedTileset;

	DB_STRUCTURE definition{};
	DB_STRUCTURE_REF reference{};
	DB_STRUCTURE_TILE tile{};
	DB_STRUCTURE_TILE* tilePointers[1] = { &tile };
	definition.ubHitPoints = 100;
	definition.ubNumberOfTiles = 1;
	definition.fFlags = STRUCTURE_OBSTACLE;
	reference.pDBStructure = &definition;
	reference.ppTile = tilePointers;
	LEVELNODE* const structureNode = AllocateNode();
	bool structureInstalled = false;
	if (structureNode)
	{
		GetMapElement(5).pStructHead = structureNode;
		structureInstalled = AddStructureToWorld(
			5, 0, &reference, structureNode) != FALSE;
	}
	gsRecompileAreaTop = 7;
	gsRecompileAreaLeft = 8;
	gsRecompileAreaRight = 9;
	gsRecompileAreaBottom = 10;
	const std::uint64_t generationBeforeTrash =
		CaptureJa2TacticalWorld().worldGeneration;
	TrashStaticPresentationWorld();
	Check(structureInstalled && !IsWorldPreparedForStaticPresentation() &&
		gpWorldLevelData == preparedWorld && gusWorldRoomInfo == preparedRooms &&
		GetWorldTileMapSize() == 12 && WORLD_ROWS == 3 && WORLD_COLS == 4 &&
		DirectionTablesUseColumnStride(4) &&
		GetMapElement(2).pObjectHead == nullptr &&
		GetMapElement(5).pStructHead == nullptr &&
		GetMapElement(5).pStructureHead == nullptr &&
		preparedRooms[11] == 0 &&
		gsRecompileAreaTop == 7 && gsRecompileAreaLeft == 8 &&
		gsRecompileAreaRight == 9 && gsRecompileAreaBottom == 10 &&
		!IsJa2TacticalWorldLoaded() &&
		CaptureJa2TacticalWorld().worldGeneration == generationBeforeTrash,
		"static teardown preserves rectangular renderer strides while freeing renderer/structure state");
	TrashStaticPresentationWorld();
	Check(gpWorldLevelData == preparedWorld && gusWorldRoomInfo == preparedRooms &&
		GetWorldTileMapSize() == 12 && GetMapElement(5).pStructureHead == nullptr,
		"static presentation teardown is idempotent and retains safe zeroed fallback storage");
	Check(!LoadMapTilesetForStaticPresentation(0),
		"presentation tileset loading is unavailable outside its explicit lifecycle");

	TestProductionStaticGeometryReplay();
	TestTransactionalStaticShadeTables();
	TestFailedStaticTilesetLoadEntersRetryableColdState();

	gsRecompileAreaTop = 0;
	gsRecompileAreaLeft = 0;
	gsRecompileAreaRight = 0;
	gsRecompileAreaBottom = 0;
	ReleaseWorldTileMap();
	if (gusWorldRoomInfo)
	{
		MemFree(gusWorldRoomInfo);
		gusWorldRoomInfo = nullptr;
	}
	guiWorldRows = OLD_WORLD_ROWS;
	guiWorldCols = OLD_WORLD_COLS;
	ShutdownFileManager();
	ShutdownMemoryManager();
	std::filesystem::remove_all(root, error);
	std::printf("\n%s (%d failure%s)\n",
		failures == 0 ? "STATIC PRESENTATION WORLD TESTS PASSED" :
			"STATIC PRESENTATION WORLD TESTS FAILED",
		failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
