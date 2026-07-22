#include "builddefines.h"

#include "DEBUG.H"
#include "Sys Globals.h"
#include "Tile Surface.h"
#include "XML.h"

#include <cstring>
#include <string>

namespace
{
INT32 gTileSurfaceAllocationCountdown = -1;

void* AllocateTileSurfaceMemory(UINT32 size)
{
	if (gTileSurfaceAllocationCountdown == 0) return nullptr;
	if (gTileSurfaceAllocationCountdown > 0)
		--gTileSurfaceAllocationCountdown;
	return MemAlloc(size);
}

std::string ReplaceExtension(const CHAR8* source, const CHAR8* extension)
{
	if (!source || !*source || !extension || !*extension) return {};
	std::string result(source);
	const std::string::size_type separator = result.find_last_of("\\/");
	const std::string::size_type dot = result.find_last_of('.');
	if (dot == std::string::npos ||
		(separator != std::string::npos && dot < separator + 1))
	{
		result.push_back('.');
	}
	else
	{
		result.erase(dot + 1);
	}
	result.append(extension);
	return result;
}

std::string CommonAdditionalPropertiesPath(const std::string& propertiesPath)
{
	const std::string::size_type separator = propertiesPath.find_last_of("\\/");
	const std::string basename = separator == std::string::npos
		? propertiesPath : propertiesPath.substr(separator + 1);
	if (basename.empty()) return {};
	return std::string("TILESETS\\ADDITIONALPROPERTIES\\") + basename;
}

class ScopedImage
{
public:
	explicit ScopedImage(HIMAGE image) : image_(image) {}
	~ScopedImage() { if (image_) DestroyImage(image_); }
	ScopedImage(const ScopedImage&) = delete;
	ScopedImage& operator=(const ScopedImage&) = delete;
	HIMAGE get() const { return image_; }

private:
	HIMAGE image_;
};

class ScopedVideoObject
{
public:
	explicit ScopedVideoObject(HVOBJECT object) : object_(object) {}
	~ScopedVideoObject() { if (object_) DeleteVideoObject(object_); }
	ScopedVideoObject(const ScopedVideoObject&) = delete;
	ScopedVideoObject& operator=(const ScopedVideoObject&) = delete;
	HVOBJECT get() const { return object_; }
	HVOBJECT release()
	{
		HVOBJECT const result = object_;
		object_ = nullptr;
		return result;
	}

private:
	HVOBJECT object_;
};

class ScopedStructureFile
{
public:
	explicit ScopedStructureFile(STRUCTURE_FILE_REF* structure = nullptr)
		: structure_(structure) {}
	~ScopedStructureFile() { if (structure_) FreeStructureFile(structure_); }
	ScopedStructureFile(const ScopedStructureFile&) = delete;
	ScopedStructureFile& operator=(const ScopedStructureFile&) = delete;
	STRUCTURE_FILE_REF* get() const { return structure_; }
	void reset(STRUCTURE_FILE_REF* structure)
	{
		if (structure_ && structure_ != structure) FreeStructureFile(structure_);
		structure_ = structure;
	}
	STRUCTURE_FILE_REF* release()
	{
		STRUCTURE_FILE_REF* const result = structure_;
		structure_ = nullptr;
		return result;
	}

private:
	STRUCTURE_FILE_REF* structure_;
};

class ScopedAuxData
{
public:
	explicit ScopedAuxData(AuxObjectData* data = nullptr) : data_(data) {}
	~ScopedAuxData() { if (data_) MemFree(data_); }
	ScopedAuxData(const ScopedAuxData&) = delete;
	ScopedAuxData& operator=(const ScopedAuxData&) = delete;
	AuxObjectData* get() const { return data_; }
	void reset(AuxObjectData* data)
	{
		if (data_ && data_ != data) MemFree(data_);
		data_ = data;
	}
	AuxObjectData* release()
	{
		AuxObjectData* const result = data_;
		data_ = nullptr;
		return result;
	}

private:
	AuxObjectData* data_;
};

void ApplyAdditionalProperties(TILE_IMAGERY& tile,
	const ADDITIONAL_TILE_PROPERTIES_VALUES& properties)
{
	tile.ubTerrainID = properties.ubTerrainID;
	tile.bWoodCamoAffinity = properties.bWoodCamoAffinity;
	tile.bDesertCamoAffinity = properties.bDesertCamoAffinity;
	tile.bUrbanCamoAffinity = properties.bUrbanCamoAffinity;
	tile.bSnowCamoAffinity = properties.bSnowCamoAffinity;
	tile.bSoundModifier = properties.bSoundModifier;
	tile.bCamoStanceModifer = properties.bCamoStanceModifer;
	tile.bStealthDifficultyModifer = properties.bStealthDifficultyModifer;
	tile.bTrapBonus = properties.bTrapBonus;
	tile.uiAdditionalFlags = properties.uiAdditionalFlags;
}
}

namespace TileSurfaceTestHooks
{
std::string CompanionPath(const CHAR8* source, const CHAR8* extension)
{
	return ReplaceExtension(source, extension);
}

std::string CommonPropertiesPath(const CHAR8* source)
{
	return CommonAdditionalPropertiesPath(ReplaceExtension(
		source, ADDITIONAL_TILE_PROPERTIES_EXTENSION));
}

void FailAllocationAfter(INT32 successfulAllocations)
{
	gTileSurfaceAllocationCountdown = successfulAllocations;
}

void ResetAllocationFailure()
{
	gTileSurfaceAllocationCountdown = -1;
}
}

TILE_IMAGERY* gTileSurfaceArray[NUMBEROFTILETYPES];
UINT8 gbDefaultSurfaceUsed[NUMBEROFTILETYPES];
UINT8 gbSameAsDefaultSurfaceUsed[NUMBEROFTILETYPES];

TILE_IMAGERY* LoadTileSurface(STR8 cFilename)
{
	if (!cFilename || !*cFilename)
	{
		SET_ERROR("Could not load tile file: invalid filename");
		return nullptr;
	}

	const std::string structureFilename =
		ReplaceExtension(cFilename, STRUCTURE_FILE_EXTENSION);
	const std::string additionalPropertiesFilename =
		ReplaceExtension(cFilename, ADDITIONAL_TILE_PROPERTIES_EXTENSION);
	const std::string commonPropertiesFilename =
		CommonAdditionalPropertiesPath(additionalPropertiesFilename);
	if (structureFilename.empty() || additionalPropertiesFilename.empty() ||
		commonPropertiesFilename.empty())
	{
		SET_ERROR("Could not derive companion paths for tile file: %s", cFilename);
		return nullptr;
	}

	ScopedImage image(CreateImage(cFilename, IMAGE_ALLDATA));
	if (!image.get())
	{
		SET_ERROR("Could not load tile file: %s", cFilename);
		return nullptr;
	}

	VOBJECT_DESC objectDescription{};
	objectDescription.fCreateFlags = VOBJECT_CREATE_FROMHIMAGE;
	objectDescription.hImage = image.get();
	ScopedVideoObject videoObject(CreateVideoObject(&objectDescription));
	if (!videoObject.get())
	{
		SET_ERROR("Could not load tile file: %s", cFilename);
		return nullptr;
	}

	ScopedStructureFile structure;
	if (FileExists(const_cast<CHAR8*>(structureFilename.c_str())))
	{
		structure.reset(LoadStructureFile(
			const_cast<CHAR8*>(structureFilename.c_str())));
		if (!structure.get() || videoObject.get()->usNumberOfObjects !=
			structure.get()->usNumberOfStructures)
		{
			SET_ERROR("Structure file error: %s", structureFilename.c_str());
			return nullptr;
		}

		DebugMsg(TOPIC_JA2, DBG_LEVEL_3, structureFilename.c_str());
		if (!AddZStripInfoToVObject(
			videoObject.get(), structure.get(), FALSE, 0))
		{
			SET_ERROR("ZStrip creation error: %s", structureFilename.c_str());
			return nullptr;
		}
	}

	memset(&zAdditionalTileProperties, 0, sizeof(zAdditionalTileProperties));
	if (FileExists(const_cast<CHAR8*>(additionalPropertiesFilename.c_str())))
	{
		DebugMsg(TOPIC_JA2, DBG_LEVEL_3,
			String("LoadExternalGameplayData, fileName = %s",
				additionalPropertiesFilename.c_str()));
		SGP_THROW_IFFALSE(ReadInAdditionalTileProperties(
			const_cast<CHAR8*>(additionalPropertiesFilename.c_str())),
			additionalPropertiesFilename.c_str());
	}
	else if (FileExists(const_cast<CHAR8*>(commonPropertiesFilename.c_str())))
	{
		DebugMsg(TOPIC_JA2, DBG_LEVEL_3,
			String("LoadExternalGameplayData, fileName = %s",
				commonPropertiesFilename.c_str()));
		SGP_THROW_IFFALSE(ReadInAdditionalTileProperties(
			const_cast<CHAR8*>(commonPropertiesFilename.c_str())),
			commonPropertiesFilename.c_str());
	}
	const ADDITIONAL_TILE_PROPERTIES_VALUES properties =
		zAdditionalTileProperties;

	ScopedAuxData ownedAuxData;
	AuxObjectData* auxData = nullptr;
	RelTileLoc* tileLocationData = nullptr;
	if (structure.get() && structure.get()->pAuxData)
	{
		auxData = structure.get()->pAuxData;
		tileLocationData = structure.get()->pTileLocData;
	}
	else
	{
		const std::size_t expectedAuxDataSize =
			static_cast<std::size_t>(videoObject.get()->usNumberOfObjects) *
			sizeof(AuxObjectData);
		if (image.get()->uiAppDataSize == expectedAuxDataSize &&
			expectedAuxDataSize != 0)
		{
			if (!image.get()->pAppData) return nullptr;
			ownedAuxData.reset(static_cast<AuxObjectData*>(
				AllocateTileSurfaceMemory(image.get()->uiAppDataSize)));
			if (!ownedAuxData.get()) return nullptr;
			memcpy(ownedAuxData.get(), image.get()->pAppData,
				image.get()->uiAppDataSize);
			auxData = ownedAuxData.get();
		}
	}

	PTILE_IMAGERY tile =
		static_cast<PTILE_IMAGERY>(
			AllocateTileSurfaceMemory(sizeof(TILE_IMAGERY)));
	if (!tile) return nullptr;
	memset(tile, 0, sizeof(*tile));
	tile->vo = videoObject.release();
	tile->pStructureFileRef = structure.release();
	if (tile->pStructureFileRef)
	{
		tile->pAuxData = auxData;
		tile->pTileLocData = tileLocationData;
	}
	else
	{
		tile->pAuxData = ownedAuxData.release();
	}
	ApplyAdditionalProperties(*tile, properties);
	return tile;
}

void DeleteTileSurface(PTILE_IMAGERY pTileSurf)
{
	if (!pTileSurf) return;
	if (pTileSurf->pStructureFileRef)
	{
		FreeStructureFile(pTileSurf->pStructureFileRef);
	}
	else if (pTileSurf->pAuxData)
	{
		MemFree(pTileSurf->pAuxData);
	}
	if (pTileSurf->vo) DeleteVideoObject(pTileSurf->vo);
	MemFree(pTileSurf);
}

extern void GetRootName(CHAR8* pDestStr, const STR8 pSrcStr);

void SetRaisedObjectFlag(const CHAR8* cFilename, TILE_IMAGERY* pTileSurf)
{
	if (!cFilename || !pTileSurf) return;
	INT32 cnt = 0;
	CHAR8 cRootFile[128];
	CHAR8 ubRaisedObjectFiles[][80] =
	{
		"bones",
		"bones2",
		"grass2",
		"grass3",
		"l_weed3",
		"litter",
		"miniweed",
		"sblast",
		"sweeds",
		"twigs",
		"wing",
		"1"
	};

	if ((pTileSurf->fType >= DEBRISWOOD &&
		pTileSurf->fType <= DEBRISWEEDS) ||
		pTileSurf->fType == DEBRIS2MISC ||
		pTileSurf->fType == ANOTHERDEBRIS)
	{
		GetRootName(cRootFile, cFilename);
		while (ubRaisedObjectFiles[cnt][0] != '1')
		{
			if (_stricmp(ubRaisedObjectFiles[cnt], cRootFile) == 0)
				pTileSurf->bRaisedObjectType = TRUE;
			++cnt;
		}
	}
}
