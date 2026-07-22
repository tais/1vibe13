#include "builddefines.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "DEBUG.H"
#include "Debug Control.h"
#include "FileMan.h"
#include "structure.h"
#include "Tile Surface.h"
#include "Tile Cache.h"
#ifdef JA2TESTVERSION
	#include "Sys Globals.h"
#endif

#include <Engine/Core/PinnedSlotCache.h>
#include <Engine/Core/UniqueResourcePtr.h>

UINT32 guiNumTileCacheStructs = 0;
UINT32 guiMaxTileCacheSize = 50;
UINT32 guiCurTileCacheSize = 0;
INT32 giDefaultStructIndex = -1;
TILE_CACHE_ELEMENT* gpTileCache = NULL;
TILE_CACHE_STRUCT* gpTileCacheStructInfo = NULL;

namespace
{
	constexpr std::size_t TileNameCapacity = 128;
	constexpr std::size_t TileRootCapacity = 30;
	constexpr std::size_t StructureFilenameCapacity = 150;
	constexpr std::size_t TileSlotCapacity = 50;

	struct TileSurfaceReleaser
	{
		void operator()(TILE_IMAGERY* imagery) const { DeleteTileSurface(imagery); }
	};

	struct StructureFileReleaser
	{
		void operator()(STRUCTURE_FILE_REF* structure) const
		{
			FreeStructureFile(structure);
		}
	};

	using OwnedTileSurface = UniqueResourcePtr<TILE_IMAGERY, TileSurfaceReleaser>;
	using OwnedStructureFile =
		UniqueResourcePtr<STRUCTURE_FILE_REF, StructureFileReleaser>;

	struct TileCacheResource
	{
		TileCacheResource(TILE_IMAGERY* tileImagery, const CHAR8* tileName,
			const CHAR8* rootName, UINT8 frameCount, INT16 structureIndex)
			: imagery(tileImagery), name(tileName), root(rootName),
			  frames(frameCount), structureRefID(structureIndex)
		{
		}

		OwnedTileSurface imagery;
		std::string name;
		std::string root;
		UINT8 frames = 1;
		INT16 structureRefID = -1;
	};

	struct TileCacheStructure
	{
		TileCacheStructure(const CHAR8* structureFilename, const CHAR8* rootName,
			OwnedStructureFile structure)
			: reference(std::move(structure)), filename(structureFilename), root(rootName)
		{
		}

		OwnedStructureFile reference;
		std::string filename;
		std::string root;
	};

	using TileSlots = PinnedSlotCache<TileCacheResource, INT16>;
	std::optional<TileSlots> gTileCache;
	std::vector<TileCacheStructure> gTileCacheStructures;
	std::array<TILE_CACHE_ELEMENT, TileSlotCapacity> gTileCacheCompatibility{};
	std::vector<TILE_CACHE_STRUCT> gTileCacheStructureCompatibility;

	void ResetCompatibilitySlot(std::size_t slot)
	{
		TILE_CACHE_ELEMENT& view = gTileCacheCompatibility[slot];
		std::memset(&view, 0, sizeof(view));
		view.sStructRefID = -1;
	}

	void SyncCompatibilitySlot(std::size_t slot)
	{
		ResetCompatibilitySlot(slot);
		if (!gTileCache) return;
		const TileCacheResource* const resource = gTileCache->find(slot);
		if (!resource) return;

		TILE_CACHE_ELEMENT& view = gTileCacheCompatibility[slot];
		std::snprintf(view.zName, sizeof(view.zName), "%s", resource->name.c_str());
		std::snprintf(view.zRootName, sizeof(view.zRootName), "%s",
			resource->root.c_str());
		view.pImagery = resource->imagery.get();
		view.sHits = gTileCache->pins(slot);
		view.ubNumFrames = resource->frames;
		view.sStructRefID = resource->structureRefID;
	}

	void PublishCompatibilityViews()
	{
		for (std::size_t slot = 0; slot < TileSlotCapacity; ++slot)
			ResetCompatibilitySlot(slot);

		gTileCacheStructureCompatibility.clear();
		gTileCacheStructureCompatibility.resize(gTileCacheStructures.size());
		for (std::size_t index = 0; index < gTileCacheStructures.size(); ++index)
		{
			TILE_CACHE_STRUCT& view = gTileCacheStructureCompatibility[index];
			std::snprintf(view.Filename, sizeof(view.Filename), "%s",
				gTileCacheStructures[index].filename.c_str());
			std::snprintf(view.zRootName, sizeof(view.zRootName), "%s",
				gTileCacheStructures[index].root.c_str());
			view.pStructureFileRef = gTileCacheStructures[index].reference.get();
		}

		gpTileCache = gTileCacheCompatibility.data();
		gpTileCacheStructInfo = gTileCacheStructureCompatibility.empty()
			? NULL : gTileCacheStructureCompatibility.data();
	}

	void SyncTileCacheDiagnostics()
	{
		guiCurTileCacheSize = gTileCache
			? static_cast<UINT32>(gTileCache->highWaterMark()) : 0;
		guiNumTileCacheStructs =
			static_cast<UINT32>(gTileCacheStructures.size());
	}

	BOOLEAN IsLiveTileCacheIndex(INT32 index)
	{
		return gTileCache && index >= 0 &&
			gTileCache->find(static_cast<std::size_t>(index)) != nullptr &&
			gTileCache->pins(static_cast<std::size_t>(index)) > 0;
	}

	void ReportTileCacheFailure(const CHAR8* reason, const STR8 filename = NULL)
	{
		std::fprintf(stderr, "[tile-cache] %s%s%s\n", reason,
			filename != NULL ? ": " : "", filename != NULL ? filename : "");
	}

	BOOLEAN CopyRootName(CHAR8* destination, size_t destinationSize,
		const STR8 source)
	{
		if (destination == NULL || destinationSize == 0 || source == NULL)
			return FALSE;

		const CHAR8* root = source;
		const CHAR8* backslash = std::strrchr(source, '\\');
		const CHAR8* slash = std::strrchr(source, '/');
		if (backslash != NULL && (slash == NULL || backslash > slash))
			root = backslash + 1;
		else if (slash != NULL)
			root = slash + 1;

		const CHAR8* extension = std::strchr(root, '.');
		const size_t length = extension != NULL
			? static_cast<size_t>(extension - root) : std::strlen(root);
		if (length >= destinationSize)
		{
			destination[0] = '\0';
			return FALSE;
		}

		std::memcpy(destination, root, length);
		destination[length] = '\0';
		return TRUE;
	}

	UINT32 CountStructureFiles()
	{
		GETFILESTRUCT fileInfo{};
		UINT32 count = 0;
		if (!GetFileFirst("TILECACHE\\*.jsd", &fileInfo)) return 0;
		do
		{
			++count;
		} while (GetFileNext(&fileInfo));
		GetFileClose(&fileInfo);
		return count;
	}

	void LoadStructureFiles(std::vector<TileCacheStructure>& structures,
		INT32& defaultStructure)
	{
		structures.reserve(CountStructureFiles());
		GETFILESTRUCT fileInfo{};
		if (!GetFileFirst("TILECACHE\\*.jsd", &fileInfo)) return;

		try
		{
			do
			{
				if (structures.size() >=
					static_cast<std::size_t>(std::numeric_limits<INT16>::max()))
				{
					ReportTileCacheFailure("too many structure files");
					break;
				}
				CHAR8 filename[StructureFilenameCapacity]{};
				const int filenameLength = std::snprintf(filename, sizeof(filename),
					"TILECACHE\\%s", fileInfo.zFileName);
				if (filenameLength < 0 ||
					static_cast<size_t>(filenameLength) >= sizeof(filename))
				{
					ReportTileCacheFailure("structure filename is too long",
						fileInfo.zFileName);
					continue;
				}

				CHAR8 root[TileRootCapacity]{};
				if (!CopyRootName(root, sizeof(root), filename))
				{
					ReportTileCacheFailure("structure root name is too long", filename);
					continue;
				}

				OwnedStructureFile loaded(LoadStructureFile(filename));
#ifdef JA2TESTVERSION
				if (!loaded)
					SET_ERROR("Cannot load tilecache JSD: %s", filename);
#endif
				structures.emplace_back(filename, root, std::move(loaded));
				if (_stricmp(root, "l_dead1") == 0)
					defaultStructure =
						static_cast<INT32>(structures.size() - 1);
			} while (GetFileNext(&fileInfo));
		}
		catch (...)
		{
			GetFileClose(&fileInfo);
			throw;
		}
		GetFileClose(&fileInfo);
	}

	INT16 FindCacheStructDataIndex(const STR8 filename)
	{
		if (filename == NULL) return -1;
		for (std::size_t index = 0; index < gTileCacheStructures.size(); ++index)
		{
			if (_stricmp(gTileCacheStructures[index].root.c_str(), filename) == 0)
				return static_cast<INT16>(index);
		}
		return -1;
	}

	STRUCTURE_FILE_REF* StructureAt(INT16 index)
	{
		if (index < 0 ||
			static_cast<std::size_t>(index) >= gTileCacheStructures.size())
			return NULL;
		return gTileCacheStructures[static_cast<std::size_t>(index)].reference.get();
	}
}

BOOLEAN InitTileCache()
{
	if (gTileCache) return TRUE;
	DeleteTileCache();

	try
	{
		TileSlots stagedCache(TileSlotCapacity);
		std::vector<TileCacheStructure> stagedStructures;
		INT32 stagedDefaultStructure = -1;
		LoadStructureFiles(stagedStructures, stagedDefaultStructure);

		gTileCache.emplace(std::move(stagedCache));
		gTileCacheStructures = std::move(stagedStructures);
		giDefaultStructIndex = stagedDefaultStructure;
		PublishCompatibilityViews();
		SyncTileCacheDiagnostics();
		return TRUE;
	}
	catch (...)
	{
		DeleteTileCache();
		ReportTileCacheFailure("initialization allocation failed");
		return FALSE;
	}
}

void DeleteTileCache()
{
	gTileCache.reset();
	gTileCacheStructures.clear();
	gTileCacheStructureCompatibility.clear();
	for (std::size_t slot = 0; slot < TileSlotCapacity; ++slot)
		ResetCompatibilitySlot(slot);
	gpTileCache = NULL;
	gpTileCacheStructInfo = NULL;
	guiCurTileCacheSize = 0;
	guiNumTileCacheStructs = 0;
	giDefaultStructIndex = -1;
}

BOOLEAN IsTileCacheInitialized()
{
	return gTileCache.has_value() ? TRUE : FALSE;
}

INT16 GetCachedTileReferenceCount(INT32 index)
{
	if (!gTileCache || index < 0) return 0;
	return gTileCache->pins(static_cast<std::size_t>(index));
}

INT32 GetCachedTile(const STR8 filename)
{
	if (!gTileCache)
	{
		ReportTileCacheFailure("request before initialization", filename);
		return -1;
	}
	if (filename == NULL || filename[0] == '\0')
	{
		ReportTileCacheFailure("empty tile filename");
		return -1;
	}
	if (std::strlen(filename) >= TileNameCapacity)
	{
		ReportTileCacheFailure("tile filename is too long", filename);
		return -1;
	}

	for (std::size_t slot = 0; slot < gTileCache->highWaterMark(); ++slot)
	{
		TileCacheResource* const resource = gTileCache->find(slot);
		if (!resource || _stricmp(resource->name.c_str(), filename) != 0) continue;
		if (!gTileCache->retain(slot))
		{
			ReportTileCacheFailure("invalid or saturated tile reference count", filename);
			return -1;
		}
		SyncCompatibilitySlot(slot);
		return static_cast<INT32>(slot);
	}

	if (gTileCache->full())
	{
		UINT32 references = 0;
		gTileCache->forEach([&references](std::size_t, const TileCacheResource&,
			INT16 pins) { references += static_cast<UINT32>(pins); });
		CHAR8 capacityFailure[128];
		std::snprintf(capacityFailure, sizeof(capacityFailure),
			"capacity %u exhausted by %u live reference%s", guiMaxTileCacheSize,
			references, references == 1 ? "" : "s");
		ReportTileCacheFailure(capacityFailure, filename);
		return -1;
	}

	CHAR8 root[TileRootCapacity]{};
	if (!CopyRootName(root, sizeof(root), filename))
	{
		ReportTileCacheFailure("tile root name is too long", filename);
		return -1;
	}

	TILE_IMAGERY* const loaded = LoadTileSurface(filename);
	if (loaded == NULL)
	{
		ReportTileCacheFailure("could not load tile surface", filename);
		return -1;
	}

	const INT16 structureIndex = FindCacheStructDataIndex(root);
	STRUCTURE_FILE_REF* const structure = StructureAt(structureIndex);
	if (structure != NULL)
		AddZStripInfoToVObject(loaded->vo, structure, TRUE, 0);
	const UINT8 frames = loaded->pAuxData != NULL
		? loaded->pAuxData->ubNumberOfFrames : 1;

	try
	{
		const std::optional<std::size_t> slot = gTileCache->insert(
			TileCacheResource(loaded, filename, root, frames, structureIndex));
		if (!slot) return -1;
		SyncCompatibilitySlot(*slot);
		SyncTileCacheDiagnostics();
		return static_cast<INT32>(*slot);
	}
	catch (...)
	{
		ReportTileCacheFailure("tile publication allocation failed", filename);
		return -1;
	}
}

BOOLEAN RemoveCachedTile(INT32 cachedTile)
{
	if (!gTileCache || cachedTile < 0) return FALSE;
	const TileSlots::ReleaseResult result =
		gTileCache->release(static_cast<std::size_t>(cachedTile));
	if (static_cast<std::size_t>(cachedTile) < TileSlotCapacity)
		SyncCompatibilitySlot(static_cast<std::size_t>(cachedTile));
	SyncTileCacheDiagnostics();
	return result == TileSlots::ReleaseResult::Removed ? TRUE : FALSE;
}

HVOBJECT GetCachedTileVideoObject(INT32 index)
{
	if (!IsLiveTileCacheIndex(index)) return NULL;
	return gTileCache->find(static_cast<std::size_t>(index))->imagery->vo;
}

UINT8 GetCachedTileFrameCount(INT32 index)
{
	if (!IsLiveTileCacheIndex(index)) return 0;
	return gTileCache->find(static_cast<std::size_t>(index))->frames;
}

STRUCTURE_FILE_REF* GetCachedTileStructureRef(INT32 index)
{
	if (!IsLiveTileCacheIndex(index)) return NULL;
	const TileCacheResource* const resource =
		gTileCache->find(static_cast<std::size_t>(index));
	return StructureAt(resource->structureRefID);
}

STRUCTURE_FILE_REF* GetCachedTileStructureRefFromFilename(const STR8 filename)
{
	return StructureAt(FindCacheStructDataIndex(filename));
}

void CheckForAndAddTileCacheStructInfo(LEVELNODE* node, INT32 gridNo,
	INT8 level, UINT16 index, UINT16 subIndex)
{
	STRUCTURE_FILE_REF* structure = GetCachedTileStructureRef(index);
	if (node == NULL || structure == NULL ||
		subIndex >= structure->usNumberOfStructures)
		return;

	if (AddStructureToWorld(gridNo, level,
		&(structure->pDBStructureRef[subIndex]), node))
		return;

	structure = StructureAt(static_cast<INT16>(giDefaultStructIndex));
	if (structure != NULL && subIndex < structure->usNumberOfStructures)
		AddStructureToWorld(gridNo, level,
			&(structure->pDBStructureRef[subIndex]), node);
}

void CheckForAndDeleteTileCacheStructInfo(LEVELNODE* node, UINT16 index)
{
	if (index < TILE_CACHE_START_INDEX) return;
	STRUCTURE_FILE_REF* const structure =
		GetCachedTileStructureRef(index - TILE_CACHE_START_INDEX);
	if (node != NULL && node->pStructureData != NULL && structure != NULL)
		DeleteStructureFromWorld(node->pStructureData);
}

void GetRootName(CHAR8* destination, const STR8 source)
{
	// Legacy pointer-only API retained for compatibility. Its existing callers
	// all pass buffers at least 128 bytes wide; bounded cache-owned buffers use
	// CopyRootName directly above.
	CopyRootName(destination, 128, source);
}
