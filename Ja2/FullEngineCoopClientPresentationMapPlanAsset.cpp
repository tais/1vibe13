#include "FullEngineCoopClientPresentationMapPlan.h"

#include "FileMan.h"
#include "Exit Grids.h"
#include "Item Types.h"
#include "Keys.h"
#include "Map Information.h"
#include "Scheduling.h"
#include "Soldier Create.h"
#include "World Items.h"
#include "WorldDat.h"
#include "lighting.h"
#include "types.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

namespace
{
static_assert(sizeof(FLOAT) == 4 && sizeof(INT32) == 4 &&
	sizeof(INT16) == 2 && sizeof(UINT16) == 2 && sizeof(UINT8) == 1,
	"presentation map parsing requires the established JA2 scalar widths");
static_assert(sizeof(OLD_WORLDITEM_101) == 52 &&
	SIZEOF_WORLDITEM_POD == 18 && _OLD_SIZEOF_WORLDITEM_POD == 12 &&
	_SIZEOF_WORLDITEM_INT8_ID_POD == 16,
	"world-item map record changed");
static_assert(SIZEOF_OBJECTTYPE_POD == 5 &&
	sizeof(OLD_OBJECTTYPE_101) == 36 && sizeof(ObjectData) == 48 &&
	sizeof(ObjectData_PRE_ITS) == 48 &&
	SIZEOF_OBJECTDATA_POD_PRE_ITS == 15,
	"object map record changed");
static_assert(SIZEOF_LBENODE_POD == 20 &&
	offsetof(ObjectDataStructs::OBJECT_LBE, bLBE) == 2,
	"load-bearing-equipment map record changed");
static_assert(sizeof(_OLD_BASIC_SOLDIERCREATE_STRUCT) == 52 &&
	sizeof(BASIC_SOLDIERCREATE_STRUCT) == 64 &&
	SIZEOF_OLD_SOLDIERCREATE_STRUCT_101_POD == 1040 &&
	_OLD_SIZEOF_SOLDIERCREATE_STRUCT_POD == 239 &&
	SIZEOF_MAPDISK_SOLDIERCREATE_STRUCT_POD == 263,
	"placement map record changed");
static_assert(sizeof(_OLD_MAPCREATE_STRUCT) == 100 &&
	sizeof(MAPCREATE_STRUCT) == 32,
	"map-information record changed");
static_assert(sizeof(LIGHT_SPRITE) == 24 && sizeof(SGPPaletteEntry) == 4 &&
	sizeof(EXITGRID) == 12 && sizeof(_OLD_DOOR) == 14 &&
	sizeof(DOOR) == 12,
	"map tail record changed");
static_assert(sizeof(_OLD_SCHEDULENODE) == 36 &&
	sizeof(_OLD_SCHEDULENODE_PRE_ITS) == 52 &&
	sizeof(MAPDISK_SCHEDULENODE) == 56,
	"schedule map record changed");
static_assert(NUM_INV_SLOTS == 55 && MAXITEMS == 16001 &&
	MAX_LIGHT_SPRITES == 4096 && MAX_INDIVIDUALS == 1284 &&
	MAX_TILESETS == 255 && MINOR_MAP_VERSION == 31 &&
	MINOR_MAP_REPAIR_SYSTEM == 30 && MINOR_MAP_OVERHEATING == 28,
	"bounded presentation map parser constants changed");

FullEngineCoopClientPresentationMapItemKind ClassifyMapItem(
	std::uint16_t item, const void*) noexcept
{
	return item < MAXITEMS && Item[item].usItemClass == IC_LBEGEAR
		? FullEngineCoopClientPresentationMapItemKind::LoadBearingEquipment
		: FullEngineCoopClientPresentationMapItemKind::Ordinary;
}
}

FullEngineCoopClientPresentationMapPlanResult
ReadFullEngineCoopClientPresentationMapAsset(
	const TacticalSectorSnapshot& sector,
	const TacticalWorldDimensions& expectedDimensions,
	FullEngineCoopClientPresentationMapPlan& output) noexcept
{
	using Result = FullEngineCoopClientPresentationMapPlanResult;
	if (!sector.loaded || !IsValidTacticalSectorSnapshot(sector) ||
		!expectedDimensions.valid())
		return Result::InvalidSnapshot;

	constexpr char prefix[] = "MAPS\\";
	std::array<char,
		(sizeof(prefix) - 1) + TacticalMapAssetKeyStorageBytes> path{};
	std::memcpy(path.data(), prefix, sizeof(prefix) - 1);
	const std::size_t keyLength = std::strlen(sector.mapAssetKey.c_str());
	std::memcpy(path.data() + sizeof(prefix) - 1,
		sector.mapAssetKey.c_str(), keyLength + 1);

	const HWFILE file = FileOpen(path.data(), FILE_ACCESS_READ);
	if (!file) return Result::AssetOpenFailure;
	const UINT32 fileSize = FileGetSize(file);
	if (fileSize == 0)
	{
		FileClose(file);
		return Result::AssetReadFailure;
	}
	if (fileSize > FullEngineCoopClientPresentationMapMaximumAssetBytes)
	{
		FileClose(file);
		return Result::AssetTooLarge;
	}

	std::vector<std::uint8_t> bytes;
	try
	{
		bytes.resize(fileSize);
	}
	catch (const std::bad_alloc&)
	{
		FileClose(file);
		return Result::AllocationFailure;
	}
	catch (...)
	{
		FileClose(file);
		return Result::AllocationFailure;
	}

	UINT32 received = 0;
	const bool read = FileRead(file, bytes.data(), fileSize, &received) != FALSE &&
		received == fileSize;
	FileClose(file);
	if (!read) return Result::AssetReadFailure;

	const FullEngineCoopClientPresentationMapItemClassifier classifier{
		&ClassifyMapItem, nullptr};
	return ParseFullEngineCoopClientPresentationMap(sector,
		expectedDimensions, std::move(bytes), classifier, output);
}
