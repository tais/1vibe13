#include "FullEngineCoopClientPresentationStaticGeometry.h"

#include "Render Fun.h"
#include "TileDat.h"
#include "environment.h"
#include "lighting.h"
#include "renderworld.h"
#include "tiledef.h"
#include "worlddef.h"
#include "worldman.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace
{
using Result = FullEngineCoopClientPresentationStaticGeometryResult;
using Section = FullEngineCoopClientPresentationMapSection;
using Span = FullEngineCoopClientPresentationMapByteSpan;

constexpr std::uint32_t MapFullSoldierSaved = 0x00000001u;
constexpr std::uint32_t MapWorldLightsSaved = 0x00000004u;
constexpr std::uint32_t MapWorldItemsSaved = 0x00000008u;
constexpr std::uint32_t MapExitGridsSaved = 0x00000010u;
constexpr std::uint32_t MapDoorTableSaved = 0x00000020u;
constexpr std::uint32_t MapEdgepointsSaved = 0x00000040u;
constexpr std::uint32_t MapAmbientLightSaved = 0x00000080u;
constexpr std::uint32_t MapNpcSchedulesSaved = 0x00000100u;

constexpr std::array<Section, 6> LayerSections{{
	Section::Land,
	Section::Objects,
	Section::Structures,
	Section::Shadows,
	Section::Roofs,
	Section::OnRoofs}};

bool SameDimensions(const TacticalWorldDimensions& left,
	const TacticalWorldDimensions& right) noexcept
{
	return left.columns == right.columns && left.rows == right.rows;
}

bool SupportedFormat(float major, std::uint8_t minor) noexcept
{
	return (major == 5.0f && minor >= 24 && minor <= 26) ||
		(major == 6.0f && minor >= 26 && minor <= 31) ||
		(major == 7.0f && minor >= 27 && minor <= 31) ||
		(major == 8.0f && minor == 31);
}

bool ExpectedSection(const FullEngineCoopClientPresentationMapPlan& plan,
	Section section) noexcept
{
	const std::uint32_t flags = plan.flags();
	switch (section)
	{
		case Section::Header:
		case Section::Metadata:
		case Section::Heights:
		case Section::LayerCounts:
		case Section::Land:
		case Section::Objects:
		case Section::Structures:
		case Section::Shadows:
		case Section::Roofs:
		case Section::OnRoofs:
		case Section::Rooms:
		case Section::MapInformation:
			return true;
		case Section::LegacyPadding:
			return plan.header().majorVersion == 6.0f &&
				plan.header().minorVersion < 27;
		case Section::WorldItems:
			return (flags & MapWorldItemsSaved) != 0;
		case Section::AmbientLight:
			return (flags & MapAmbientLightSaved) != 0;
		case Section::WorldLights:
			return (flags & MapWorldLightsSaved) != 0;
		case Section::Placements:
			return (flags & MapFullSoldierSaved) != 0;
		case Section::ExitGrids:
			return (flags & MapExitGridsSaved) != 0;
		case Section::Doors:
			return (flags & MapDoorTableSaved) != 0;
		case Section::Edgepoints:
			return (flags & MapEdgepointsSaved) != 0;
		case Section::Schedules:
			return (flags & MapNpcSchedulesSaved) != 0;
		case Section::LegacyEditorRemainder:
			// The parser retains even an empty remainder for supported legacy
			// layouts. It owns bytes after the native body, never replay input.
			return plan.header().majorVersion < 7.0f;
		case Section::Count:
			break;
	}
	return false;
}

bool ExactSectionSize(const FullEngineCoopClientPresentationMapPlan& plan,
	Section section, std::size_t expected) noexcept
{
	const Span& span = plan.section(section);
	return span.present && span.size == expected;
}

bool ReadU16(const std::uint8_t* input, std::uint16_t& value) noexcept
{
	std::memcpy(&value, input, sizeof(value));
	return true;
}

bool ValidatePlan(const FullEngineCoopClientPresentationMapPlan& plan) noexcept
{
	const auto& bytes = plan.bytes();
	const auto& dimensions = plan.dimensions();
	const auto& header = plan.header();
	if (bytes.empty() ||
		bytes.size() > FullEngineCoopClientPresentationMapMaximumAssetBytes ||
		!plan.sector().loaded ||
		!IsValidTacticalSectorSnapshot(plan.sector()) ||
		!dimensions.valid() || !SameDimensions(dimensions, header.dimensions) ||
		!SupportedFormat(header.majorVersion, header.minorVersion) ||
		(plan.flags() & ~FullEngineCoopClientPresentationMapKnownFlags) != 0 ||
		plan.tilesetId() < 0)
	{
		return false;
	}

	const std::uint64_t cells64 =
		static_cast<std::uint64_t>(dimensions.columns) * dimensions.rows;
	if (cells64 > std::numeric_limits<std::uint32_t>::max() ||
		plan.worldCellCount() != cells64)
	{
		return false;
	}
	const std::size_t cells = static_cast<std::size_t>(cells64);

	// Every byte of the immutable plan must still be owned by exactly one
	// parser-established span, in map-wire order. This catches stale/default
	// plans without interpreting ignored gameplay sections in Phase B.
	std::size_t nextOffset = 0;
	for (std::uint8_t raw = 0;
		raw < static_cast<std::uint8_t>(Section::Count); ++raw)
	{
		const Section section = static_cast<Section>(raw);
		const Span& span = plan.section(section);
		const bool expected = ExpectedSection(plan, section);
		if (span.present != expected || !span.validFor(bytes.size())) return false;
		if (!span.present)
		{
			if (span.offset != 0 || span.size != 0) return false;
			continue;
		}
		if (span.offset != nextOffset) return false;
		nextOffset += span.size;
	}
	if (nextOffset != bytes.size()) return false;

	const std::size_t headerSize = header.majorVersion >= 7.0f
		? FullEngineCoopClientSizedMapHeaderBytes
		: FullEngineCoopClientLegacyMapHeaderBytes;
	if (!ExactSectionSize(plan, Section::Header, headerSize) ||
		!ExactSectionSize(plan, Section::Metadata, 12) ||
		!ExactSectionSize(plan, Section::Heights, cells * 2) ||
		!ExactSectionSize(plan, Section::LayerCounts, cells * 4) ||
		!ExactSectionSize(plan, Section::Rooms,
			cells * (header.minorVersion < 29 ? 1 : 2)) ||
		!ExactSectionSize(plan, Section::MapInformation,
			header.majorVersion < 7.0f ? 100 : 32))
	{
		return false;
	}
	for (std::size_t layer = 0; layer < LayerSections.size(); ++layer)
	{
		const std::uint64_t size =
			static_cast<std::uint64_t>(plan.layerEntryCounts()[layer]) *
			(layer == 1 ? 3u : 2u);
		if (size > std::numeric_limits<std::size_t>::max() ||
			!ExactSectionSize(plan, LayerSections[layer],
				static_cast<std::size_t>(size)))
		{
			return false;
		}
	}
	if (ExpectedSection(plan, Section::LegacyPadding) &&
		!ExactSectionSize(plan, Section::LegacyPadding, 37 * 4))
	{
		return false;
	}
	if (ExpectedSection(plan, Section::AmbientLight) &&
		!ExactSectionSize(plan, Section::AmbientLight, 3))
	{
		return false;
	}

	// Reconfirm that the identity/metadata bytes still describe the public plan.
	float encodedMajor = 0.0f;
	std::uint8_t encodedMinor = 0;
	std::memcpy(&encodedMajor, bytes.data(), sizeof(encodedMajor));
	std::memcpy(&encodedMinor, bytes.data() + sizeof(encodedMajor),
		sizeof(encodedMinor));
	if (encodedMajor != header.majorVersion ||
		encodedMinor != header.minorVersion)
	{
		return false;
	}
	if (header.majorVersion >= 7.0f)
	{
		std::int32_t rows = 0;
		std::int32_t columns = 0;
		std::memcpy(&rows,
			bytes.data() + FullEngineCoopClientLegacyMapHeaderBytes,
			sizeof(rows));
		std::memcpy(&columns,
			bytes.data() + FullEngineCoopClientLegacyMapHeaderBytes +
				sizeof(rows), sizeof(columns));
		if (rows != dimensions.rows || columns != dimensions.columns)
			return false;
	}
	const Span& metadata = plan.section(Section::Metadata);
	std::uint32_t encodedFlags = 0;
	std::int32_t encodedTileset = -1;
	std::uint32_t encodedSoldierSize = 0;
	std::memcpy(&encodedFlags, bytes.data() + metadata.offset,
		sizeof(encodedFlags));
	std::memcpy(&encodedTileset,
		bytes.data() + metadata.offset + sizeof(encodedFlags),
		sizeof(encodedTileset));
	std::memcpy(&encodedSoldierSize,
		bytes.data() + metadata.offset + sizeof(encodedFlags) +
			sizeof(encodedTileset), sizeof(encodedSoldierSize));
	if (encodedFlags != plan.flags() || encodedTileset != plan.tilesetId() ||
		encodedSoldierSize != plan.legacySoldierSize())
	{
		return false;
	}

	std::array<std::uint32_t, 6> layerCounts{};
	const Span& counts = plan.section(Section::LayerCounts);
	for (std::size_t cell = 0; cell < cells; ++cell)
	{
		const std::uint8_t* value = bytes.data() + counts.offset + cell * 4;
		if ((value[3] & 0xf0u) != 0) return false;
		layerCounts[0] += value[0] & 0x0fu;
		layerCounts[1] += value[1] & 0x0fu;
		layerCounts[2] += value[1] >> 4;
		layerCounts[3] += value[2] & 0x0fu;
		layerCounts[4] += value[2] >> 4;
		layerCounts[5] += value[3] & 0x0fu;
	}
	if (layerCounts != plan.layerEntryCounts()) return false;

	const Span& rooms = plan.section(Section::Rooms);
	for (std::size_t cell = 0; cell < cells; ++cell)
	{
		std::uint16_t room = bytes[rooms.offset + cell];
		if (header.minorVersion >= 29)
			ReadU16(bytes.data() + rooms.offset + cell * 2, room);
		if (room >= MAX_ROOMS) return false;
	}
	if (ExpectedSection(plan, Section::AmbientLight))
	{
		const Span& ambient = plan.section(Section::AmbientLight);
		const std::uint8_t* value = bytes.data() + ambient.offset;
		if (value[0] > 1 || value[1] > 1 ||
			value[2] < TacticalWorldLightingSnapshot::Brightest ||
			value[2] > TacticalWorldLightingSnapshot::Darkest)
		{
			return false;
		}
	}
	return true;
}

std::uint8_t CountForLayer(const std::uint8_t* counts,
	std::size_t layer) noexcept
{
	switch (layer)
	{
		case 0: return counts[0] & 0x0fu;
		case 1: return counts[1] & 0x0fu;
		case 2: return counts[1] >> 4;
		case 3: return counts[2] & 0x0fu;
		case 4: return counts[2] >> 4;
		case 5: return counts[3] & 0x0fu;
	}
	return 0;
}

bool ResolveTile(std::uint8_t type, std::uint16_t subIndex,
	std::uint16_t& tileIndex) noexcept
{
	if (giNumberOfTileTypes <= 0 || giNumberOfTiles <= 0 ||
		gTileDatabaseSize == 0 || subIndex == 0 ||
		type >= static_cast<std::uint32_t>(giNumberOfTileTypes) ||
		type >= NUMBEROFTILETYPES || subIndex > gNumTilesPerType[type])
	{
		return false;
	}
	std::uint16_t resolved = NO_TILE;
	if (!GetTileIndexFromTypeSubIndex(type, subIndex, &resolved) ||
		resolved == NO_TILE || resolved >= giNumberOfTiles ||
		resolved >= gTileDatabaseSize)
	{
		return false;
	}
	tileIndex = resolved;
	return true;
}

Result AddLayerEntry(std::size_t layer, std::uint32_t cell,
	std::uint8_t type, std::uint16_t subIndex,
	std::uint8_t mapMinorVersion) noexcept
{
	// Pointer tiles are editor/UI markers, not static map imagery. Preserve the
	// established map-loader omission while still consuming their exact bytes.
	if (layer == 1 && type >= FIRSTPOINTERS) return Result::Success;

	std::uint16_t tile = NO_TILE;
	if (!ResolveTile(type, subIndex, tile)) return Result::InvalidTileReference;
	if (layer == 2 && mapMinorVersion <= 25 &&
		(gTileDatabase[tile].uiFlags & UNDERFLOW_FILLER) != 0)
	{
		if (!ResolveTile(type, 1, tile)) return Result::InvalidTileReference;
	}

	bool added = false;
	switch (layer)
	{
		case 0: added = AddLandToHead(cell, tile) != FALSE; break;
		case 1: added = AddObjectToTail(cell, tile) != nullptr; break;
		case 2:
			added = AddStructToTailForStaticPresentation(cell, tile) != nullptr;
			break;
		case 3: added = AddShadowToTail(cell, tile) != FALSE; break;
		case 4: added = AddRoofToTail(cell, tile) != nullptr; break;
		case 5: added = AddOnRoofToTail(cell, tile) != nullptr; break;
	}
	return added ? Result::Success : Result::LayerInsertionFailure;
}

Result ReplayGeometry(const FullEngineCoopClientPresentationMapPlan& plan)
	noexcept
{
	const auto& bytes = plan.bytes();
	const std::size_t cells = plan.worldCellCount();
	const Span& heights = plan.section(Section::Heights);
	const Span& counts = plan.section(Section::LayerCounts);
	for (std::size_t cell = 0; cell < cells; ++cell)
	{
		GetMapElement(static_cast<UINT32>(cell)).sHeight =
			bytes[heights.offset + cell * 2];
		// The following serialized byte is legacy ubAdjacentSoldierCnt state,
		// not part of the height.  Presentation actors own their own fresh
		// adjacency state, so intentionally do not replay it.
		GetMapElement(static_cast<UINT32>(cell)).uiFlags |=
			static_cast<UINT16>(
				(bytes[counts.offset + cell * 4] & 0xf0u) >> 4);
	}

	for (std::size_t layer = 0; layer < LayerSections.size(); ++layer)
	{
		const Span& entries = plan.section(LayerSections[layer]);
		std::size_t cursor = entries.offset;
		for (std::size_t cell = 0; cell < cells; ++cell)
		{
			const std::uint8_t* cellCounts =
				bytes.data() + counts.offset + cell * 4;
			for (std::uint8_t entry = 0;
				entry < CountForLayer(cellCounts, layer); ++entry)
			{
				const std::uint8_t type = bytes[cursor++];
				std::uint16_t subIndex = bytes[cursor++];
				if (layer == 1)
				{
					ReadU16(bytes.data() + cursor - 1, subIndex);
					++cursor;
				}
				const Result result = AddLayerEntry(layer,
					static_cast<std::uint32_t>(cell), type, subIndex,
					plan.header().minorVersion);
				if (result != Result::Success) return result;
			}
		}
		if (cursor != entries.offset + entries.size)
			return Result::InvalidPlan;
	}

	const Span& rooms = plan.section(Section::Rooms);
	for (std::size_t cell = 0; cell < cells; ++cell)
	{
		std::uint16_t room = bytes[rooms.offset + cell];
		if (plan.header().minorVersion >= 29)
			ReadU16(bytes.data() + rooms.offset + cell * 2, room);
		gusWorldRoomInfo[cell] = room;
	}
	std::memset(gubWorldRoomHidden, TRUE, sizeof(gubWorldRoomHidden));
	return Result::Success;
}
}

const char* FullEngineCoopClientPresentationStaticGeometryResultName(
	FullEngineCoopClientPresentationStaticGeometryResult result) noexcept
{
	switch (result)
	{
		case Result::Success: return "success";
		case Result::AlreadyLoaded: return "static geometry already loaded";
		case Result::NotLoaded: return "static geometry is not loaded";
		case Result::InvalidPlan: return "invalid presentation map plan";
		case Result::InvalidAuthorityLighting:
			return "invalid authoritative lighting";
		case Result::MapTranslationActive:
			return "stale map translation is active";
		case Result::WorldOwnershipConflict:
			return "static presentation world already owned";
		case Result::WorldPreparationFailure:
			return "static presentation world preparation failed";
		case Result::TilesetLoadFailure:
			return "presentation tileset load failed";
		case Result::InvalidTileReference:
			return "map layer references an unavailable tile";
		case Result::LayerInsertionFailure:
			return "static map layer insertion failed";
		case Result::RendererInitializationFailure:
			return "static presentation renderer initialization failed";
	}
	return "unknown static presentation geometry result";
}

FullEngineCoopClientPresentationStaticGeometry::~FullEngineCoopClientPresentationStaticGeometry()
{
	teardown();
}

void FullEngineCoopClientPresentationStaticGeometry::rollback() noexcept
{
	if (previousEnvironmentCaptured_ && environmentMutated_)
	{
		if (worldPrepared_ && IsWorldPreparedForStaticPresentation() &&
			lightColorsMutated_ && previousLightColorCount_ >= 1 &&
			previousLightColorCount_ <= 2)
		{
			SGPPaletteEntry previousColors[2]{};
			static_assert(sizeof(previousColors) ==
				sizeof(previousLightColors_));
			std::memcpy(previousColors, previousLightColors_.data(),
				sizeof(previousColors));
			(void)LightSetColorsForStaticPresentation(
				previousColors, previousLightColorCount_);
		}
		if (worldPrepared_ && IsWorldPreparedForStaticPresentation() &&
			previousAmbientLightLevel_ >= TacticalWorldLightingSnapshot::Brightest &&
			previousAmbientLightLevel_ <= TacticalWorldLightingSnapshot::Darkest)
		{
			(void)LightSetBaseLevelForStaticPresentation(
				previousAmbientLightLevel_);
		}
		else
		{
			ubAmbientLightLevel = previousAmbientLightLevel_;
		}
		gfBasement = previousBasement_ ? TRUE : FALSE;
		gfCaves = previousCaves_ ? TRUE : FALSE;
		ClearRenderFlags(~static_cast<UINT32>(0));
		SetRenderFlags(previousRenderFlags_);
	}
	if (worldPrepared_ && IsWorldPreparedForStaticPresentation())
		TrashStaticPresentationWorld();

	previousAmbientLightLevel_ = 0;
	previousLightColorCount_ = 0;
	previousLightColors_.fill(0);
	previousRenderFlags_ = 0;
	previousBasement_ = false;
	previousCaves_ = false;
	previousEnvironmentCaptured_ = false;
	lightColorsMutated_ = false;
	worldPrepared_ = false;
	environmentMutated_ = false;
	loaded_ = false;
	omittedMapLightSprites_ = false;
}

void FullEngineCoopClientPresentationStaticGeometry::teardown() noexcept
{
	rollback();
}

FullEngineCoopClientPresentationStaticGeometryResult
FullEngineCoopClientPresentationStaticGeometry::load(
	const FullEngineCoopClientPresentationMapPlan& plan,
	const TacticalWorldLightingSnapshot& authorityLighting) noexcept
{
	if (loaded_ || worldPrepared_) return Result::AlreadyLoaded;
	if (!ValidatePlan(plan)) return Result::InvalidPlan;
	if (!authorityLighting.valid()) return Result::InvalidAuthorityLighting;
	// Retained plan grids already use the authority's exact published
	// dimensions. Legacy editor resize translation must never be applied or
	// silently disabled on behalf of this transaction.
	if (gMapTrn.IsTrn()) return Result::MapTranslationActive;
	if (IsWorldPreparedForStaticPresentation())
		return Result::WorldOwnershipConflict;

	previousAmbientLightLevel_ = ubAmbientLightLevel;
	SGPPaletteEntry previousColors[2]{};
	previousLightColorCount_ = LightGetColors(previousColors);
	static_assert(sizeof(previousColors) == sizeof(previousLightColors_));
	std::memcpy(previousLightColors_.data(), previousColors,
		sizeof(previousColors));
	previousRenderFlags_ = GetRenderFlags();
	previousBasement_ = gfBasement != FALSE;
	previousCaves_ = gfCaves != FALSE;
	previousEnvironmentCaptured_ = true;

	const TacticalWorldDimensions& dimensions = plan.dimensions();
	if (!PrepareWorldForStaticPresentation(dimensions.rows,
		dimensions.columns))
	{
		rollback();
		return Result::WorldPreparationFailure;
	}
	worldPrepared_ = true;

	LoadShadeTablesFromTextFile();
	if (!LoadMapTilesetForStaticPresentation(plan.tilesetId()))
	{
		rollback();
		return Result::TilesetLoadFailure;
	}
	// World-light sprites remain authority-dependent and are omitted below, but
	// the first saved light colour is static map rendering metadata. The legacy
	// loader likewise normalizes map files to one active colour.
	SGPPaletteEntry mapLightColor{};
	const Span& worldLights = plan.section(Section::WorldLights);
	if (worldLights.present && plan.bytes()[worldLights.offset] != 0)
	{
		std::memcpy(&mapLightColor,
			plan.bytes().data() + worldLights.offset + 1,
			sizeof(mapLightColor));
	}
	if (!LightSetColorsForStaticPresentation(&mapLightColor, 1))
	{
		rollback();
		return Result::RendererInitializationFailure;
	}
	lightColorsMutated_ = true;

	bool basement = false;
	bool caves = false;
	const Span& ambient = plan.section(Section::AmbientLight);
	if (ambient.present)
	{
		basement = plan.bytes()[ambient.offset] != 0;
		caves = plan.bytes()[ambient.offset + 1] != 0;
	}
	gfBasement = basement ? TRUE : FALSE;
	gfCaves = caves ? TRUE : FALSE;
	environmentMutated_ = true;

	// Structure insertion consults GridNoIndoors for shadow-buddy behavior, so
	// the validated static underground identity must be visible before replay.
	const Result replay = ReplayGeometry(plan);
	if (replay != Result::Success)
	{
		rollback();
		return replay;
	}

	if (!LightSetBaseLevelForStaticPresentation(
		authorityLighting.ambientLightLevel))
	{
		rollback();
		return Result::RendererInitializationFailure;
	}
	if (!InitializeWorldForStaticPresentationRendering())
	{
		rollback();
		return Result::RendererInitializationFailure;
	}

	omittedMapLightSprites_ =
		plan.section(Section::WorldLights).present;
	loaded_ = true;
	return Result::Success;
}

FullEngineCoopClientPresentationStaticGeometryResult
FullEngineCoopClientPresentationStaticGeometry::synchronizeLighting(
	const TacticalWorldLightingSnapshot& authorityLighting) noexcept
{
	if (!loaded_ || !worldPrepared_ ||
		!IsWorldPreparedForStaticPresentation())
		return Result::NotLoaded;
	if (!authorityLighting.valid()) return Result::InvalidAuthorityLighting;
	if (!LightSetBaseLevelForStaticPresentation(
			authorityLighting.ambientLightLevel))
		return Result::RendererInitializationFailure;
	SetRenderFlags(RENDER_FLAG_FULL);
	return Result::Success;
}
