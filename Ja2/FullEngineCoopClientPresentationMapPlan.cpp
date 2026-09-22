#include "FullEngineCoopClientPresentationMapPlan.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace
{
using Result = FullEngineCoopClientPresentationMapPlanResult;
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

constexpr std::uint32_t MaximumWorldItems = 65536u;
constexpr std::uint32_t MaximumRecursiveObjects = 1000000u;
constexpr std::uint32_t MaximumObjectNesting = 64u;
constexpr std::int32_t MaximumObjectListEntries = 511;

// Frozen map-wire widths. Native VFS integration must statically check these
// against engine structures. This parser remains a pure, data-free component
// suitable for synthetic fuzz/regression tests.
constexpr std::size_t LegacyWorldItemBytes = 52;
constexpr std::size_t WorldItemV6PodBytes = 12;
constexpr std::size_t WorldItemV7PodBytes = 16;
constexpr std::size_t WorldItemV8PodBytes = 18;
constexpr std::size_t LegacyObjectBytes = 36;
constexpr std::size_t ObjectPodBytes = 5;
constexpr std::size_t CurrentObjectDataBytes = 48;
constexpr std::size_t PreItsObjectDataBytes = 48;
constexpr std::size_t PreItsObjectDataPodBytes = 15;
constexpr std::size_t LbeNodePodBytes = 20;
constexpr std::size_t LbeMarkerOffset = 2;
constexpr std::size_t LegacyBasicPlacementBytes = 52;
constexpr std::size_t CurrentBasicPlacementBytes = 64;
constexpr std::size_t LegacyDetailedPlacementBytes = 1040;
constexpr std::size_t V6DetailedPlacementPodBytes = 239;
constexpr std::size_t CurrentDetailedPlacementPodBytes = 263;
constexpr std::size_t LegacyMapInformationBytes = 100;
constexpr std::size_t CurrentMapInformationBytes = 32;
constexpr std::size_t LightSpriteBytes = 24;
constexpr std::size_t PaletteEntryBytes = 4;
constexpr std::size_t CurrentExitGridBytes = 12;
constexpr std::size_t LegacyDoorBytes = 14;
constexpr std::size_t CurrentDoorBytes = 12;
constexpr std::size_t LegacyScheduleBytes = 36;
constexpr std::size_t V7ScheduleBytes = 52;
constexpr std::size_t V8ScheduleBytes = 56;
constexpr std::uint8_t CurrentMinorMapVersion = 31;
constexpr std::uint8_t RepairSystemMinorMapVersion = 30;
constexpr std::uint8_t OverheatingMinorMapVersion = 28;
constexpr std::uint16_t MaximumIndividuals = 1284;
constexpr std::int32_t InventorySlotCount = 55;
constexpr std::uint16_t MaximumItemCount = 16001;
constexpr std::uint16_t MaximumLightSprites = 4096;
constexpr std::int32_t MaximumTilesets = 255;

class Cursor
{
public:
	explicit Cursor(const std::vector<std::uint8_t>& bytes) noexcept
		: bytes_(bytes)
	{
	}

	std::size_t offset() const noexcept { return offset_; }
	std::size_t remaining() const noexcept { return bytes_.size() - offset_; }
	const std::uint8_t* at(std::size_t offset) const noexcept
	{
		return bytes_.data() + offset;
	}

	bool skip(std::size_t amount) noexcept
	{
		if (amount > remaining()) return false;
		offset_ += amount;
		return true;
	}

	template <typename Value>
	bool read(Value& output) noexcept
	{
		static_assert(std::is_trivially_copyable<Value>::value,
			"map cursor reads scalar POD only");
		if (sizeof(Value) > remaining()) return false;
		std::memcpy(&output, at(offset_), sizeof(Value));
		offset_ += sizeof(Value);
		return true;
	}

private:
	const std::vector<std::uint8_t>& bytes_;
	std::size_t offset_ = 0;
};

struct MapFormat
{
	std::uint8_t major = 0;
	std::uint8_t minor = 0;
	std::uint8_t effectiveTailMajor = 0;
	bool modernObjects = false;
	bool russianPadding = false;
};

struct ParseState
{
	Cursor cursor;
	MapFormat format;
	FullEngineCoopClientPresentationMapItemClassifier classifier;
	std::array<Span, static_cast<std::size_t>(Section::Count)> sections{};
	std::array<std::uint32_t, 6> layerCounts{};
	std::uint32_t flags = 0;
	std::int32_t tilesetId = -1;
	std::uint32_t legacySoldierSize = 0;
	std::uint32_t worldCells = 0;
	std::uint16_t placementCount = 0;
	std::uint8_t embeddedMapVersion = 0;
	std::uint32_t recursiveObjects = 0;

	ParseState(const std::vector<std::uint8_t>& bytes,
		const FullEngineCoopClientPresentationMapItemClassifier& value) noexcept
		: cursor(bytes), classifier(value)
	{
	}
};

bool CheckedProduct(std::size_t left, std::size_t right,
	std::size_t& output) noexcept
{
	if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left)
		return false;
	output = left * right;
	return true;
}

void SetSpan(ParseState& state, Section section, std::size_t start) noexcept
{
	state.sections[static_cast<std::size_t>(section)] =
		{start, state.cursor.offset() - start, true};
}

Result SkipSection(ParseState& state, Section section,
	std::size_t size) noexcept
{
	const std::size_t start = state.cursor.offset();
	if (!state.cursor.skip(size)) return Result::TruncatedData;
	SetSpan(state, section, start);
	return Result::Success;
}

bool SupportedMapFormat(float major, std::uint8_t minor,
	MapFormat& output) noexcept
{
	MapFormat candidate;
	if (major == 5.0f && minor >= 24 && minor <= 26)
	{
		candidate = {5, minor, 5, false, false};
	}
	else if (major == 6.0f && minor == 26)
	{
		// The engine consumes a 37-dword Russian-editor extension, then
		// deliberately treats every following record as a v5 record.
		candidate = {6, minor, 5, false, true};
	}
	else if (major == 6.0f && minor >= 27 && minor <= 31)
	{
		candidate = {6, minor, 6, true, false};
	}
	else if (major == 7.0f && minor >= 27 && minor <= 31)
	{
		candidate = {7, minor, 7, true, false};
	}
	else if (major == 8.0f && minor == 31)
	{
		candidate = {8, minor, 8, true, false};
	}
	else
	{
		return false;
	}
	output = candidate;
	return true;
}

std::size_t ObjectDataBytes(const MapFormat& format) noexcept
{
	if (format.major >= 8 && format.minor >= CurrentMinorMapVersion)
		return CurrentObjectDataBytes;
	if (format.major >= 7 && format.minor >= CurrentMinorMapVersion)
		return PreItsObjectDataBytes;
	if (format.major >= 7 && format.minor >= RepairSystemMinorMapVersion)
		return PreItsObjectDataBytes - sizeof(std::uint64_t);
	if (format.major >= 7 && format.minor >= OverheatingMinorMapVersion)
		return 32;
	return PreItsObjectDataPodBytes + 1;
}

Result ParseObject(ParseState& state, std::uint32_t depth) noexcept;

Result ParseLbeNode(ParseState& state, std::uint32_t depth) noexcept
{
	if (!state.cursor.skip(LbeNodePodBytes))
		return Result::TruncatedData;
	std::int32_t count = 0;
	if (!state.cursor.read(count)) return Result::TruncatedData;
	if (count < 0 || count > MaximumObjectListEntries)
		return Result::InvalidCount;
	for (std::int32_t index = 0; index < count; ++index)
	{
		const Result parsed = ParseObject(state, depth + 1);
		if (parsed != Result::Success) return parsed;
	}
	return Result::Success;
}

Result ParseStackedObject(ParseState& state, std::uint16_t item,
	bool lbeItem, std::uint32_t depth) noexcept
{
	const std::size_t dataStart = state.cursor.offset();
	const std::size_t dataBytes = ObjectDataBytes(state.format);
	if (!state.cursor.skip(dataBytes)) return Result::TruncatedData;
	const bool activeLbe = lbeItem &&
		static_cast<std::int8_t>(state.cursor.at(dataStart)[
			LbeMarkerOffset]) == -1;

	std::int32_t attachmentCount = 0;
	if (!state.cursor.read(attachmentCount)) return Result::TruncatedData;
	if (attachmentCount < 0 ||
		attachmentCount > MaximumObjectListEntries)
		return Result::InvalidCount;
	for (std::int32_t index = 0; index < attachmentCount; ++index)
	{
		const Result parsed = ParseObject(state, depth + 1);
		if (parsed != Result::Success) return parsed;
	}
	if (activeLbe) return ParseLbeNode(state, depth + 1);
	(void)item;
	return Result::Success;
}

Result ParseObject(ParseState& state, std::uint32_t depth) noexcept
{
	if (depth > MaximumObjectNesting) return Result::RecursionLimit;
	if (++state.recursiveObjects > MaximumRecursiveObjects)
		return Result::InvalidCount;
	if (!state.format.modernObjects)
		return state.cursor.skip(LegacyObjectBytes)
			? Result::Success : Result::TruncatedData;

	const std::size_t podStart = state.cursor.offset();
	if (!state.cursor.skip(ObjectPodBytes))
		return Result::TruncatedData;
	std::int32_t stackCount = 0;
	if (!state.cursor.read(stackCount)) return Result::TruncatedData;
	if (stackCount < 0 || stackCount > MaximumObjectListEntries)
		return Result::InvalidCount;
	if (stackCount == 0) return Result::Success;
	if (state.classifier.lookup == nullptr)
		return Result::ItemClassificationRequired;

	std::uint16_t item = 0;
	std::memcpy(&item, state.cursor.at(podStart), sizeof(item));
	if (item >= MaximumItemCount) return Result::InvalidMetadata;
	const bool lbeItem = state.classifier.lookup(item,
		state.classifier.context) ==
		FullEngineCoopClientPresentationMapItemKind::LoadBearingEquipment;
	for (std::int32_t index = 0; index < stackCount; ++index)
	{
		const Result parsed =
			ParseStackedObject(state, item, lbeItem, depth);
		if (parsed != Result::Success) return parsed;
	}
	return Result::Success;
}

Result ParseWorldItems(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	std::uint32_t count = 0;
	if (!state.cursor.read(count)) return Result::TruncatedData;
	if (count > MaximumWorldItems) return Result::InvalidCount;
	if (!state.format.modernObjects)
	{
		std::size_t bytes = 0;
		if (!CheckedProduct(count, LegacyWorldItemBytes, bytes) ||
			!state.cursor.skip(bytes))
			return Result::TruncatedData;
	}
	else
	{
		const std::size_t podBytes = state.format.major < 7
			? WorldItemV6PodBytes
			: (state.format.major < 8
				? WorldItemV7PodBytes
				: WorldItemV8PodBytes);
		for (std::uint32_t index = 0; index < count; ++index)
		{
			if (!state.cursor.skip(podBytes)) return Result::TruncatedData;
			const Result parsed = ParseObject(state, 1);
			if (parsed != Result::Success) return parsed;
		}
	}
	SetSpan(state, Section::WorldItems, start);
	return Result::Success;
}

Result ParseWorldLights(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	std::uint8_t colors = 0;
	if (!state.cursor.read(colors)) return Result::TruncatedData;
	if (colors > 3) return Result::InvalidCount;
	if (!state.cursor.skip(static_cast<std::size_t>(colors) *
		PaletteEntryBytes))
		return Result::TruncatedData;
	std::uint16_t lights = 0;
	if (!state.cursor.read(lights)) return Result::TruncatedData;
	if (lights > MaximumLightSprites) return Result::InvalidCount;
	for (std::uint16_t index = 0; index < lights; ++index)
	{
		if (!state.cursor.skip(LightSpriteBytes))
			return Result::TruncatedData;
		std::uint8_t length = 0;
		if (!state.cursor.read(length)) return Result::TruncatedData;
		const std::size_t stringStart = state.cursor.offset();
		if (!state.cursor.skip(length)) return Result::TruncatedData;
		if (length != 0 && state.cursor.at(stringStart)[length - 1] != 0)
			return Result::InvalidMetadata;
	}
	SetSpan(state, Section::WorldLights, start);
	return Result::Success;
}

bool ValidEntryGrid(std::int32_t grid, std::uint32_t cells) noexcept
{
	return grid == -1 || (grid >= 0 &&
		static_cast<std::uint32_t>(grid) < cells);
}

Result ParseMapInformation(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	if (state.format.effectiveTailMajor < 7)
	{
		if (LegacyMapInformationBytes > state.cursor.remaining())
			return Result::TruncatedData;
		std::array<std::int16_t, 6> grids{};
		for (std::size_t index = 0; index < 4; ++index)
			std::memcpy(&grids[index], state.cursor.at(start) + index * 2, 2);
		std::memcpy(&grids[4], state.cursor.at(start) + 12, 2);
		std::memcpy(&grids[5], state.cursor.at(start) + 14, 2);
		for (const std::int16_t grid : grids)
			if (!ValidEntryGrid(grid, state.worldCells))
				return Result::InvalidMetadata;
		state.placementCount = state.cursor.at(start)[8];
		state.embeddedMapVersion = state.cursor.at(start)[9];
		if (!state.cursor.skip(LegacyMapInformationBytes))
			return Result::TruncatedData;
	}
	else
	{
		if (CurrentMapInformationBytes > state.cursor.remaining())
			return Result::TruncatedData;
		for (std::size_t index = 0; index < 6; ++index)
		{
			std::int32_t grid = 0;
			std::memcpy(&grid, state.cursor.at(start) + index * 4, 4);
			if (!ValidEntryGrid(grid, state.worldCells))
				return Result::InvalidMetadata;
		}
		std::memcpy(&state.placementCount, state.cursor.at(start) + 24, 2);
		state.embeddedMapVersion = state.cursor.at(start)[26];
		if (!state.cursor.skip(CurrentMapInformationBytes))
			return Result::TruncatedData;
	}
	if (state.placementCount > MaximumIndividuals ||
		state.embeddedMapVersion < 15 ||
		state.embeddedMapVersion > CurrentMinorMapVersion)
		return Result::InvalidMetadata;
	SetSpan(state, Section::MapInformation, start);
	return Result::Success;
}

Result ParsePlacements(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	const std::size_t basicBytes = state.format.effectiveTailMajor < 7
		? LegacyBasicPlacementBytes
		: CurrentBasicPlacementBytes;
	for (std::uint16_t index = 0; index < state.placementCount; ++index)
	{
		if (basicBytes > state.cursor.remaining())
			return Result::TruncatedData;
		const bool detailed = state.cursor.at(state.cursor.offset())[0] != 0;
		if (!state.cursor.skip(basicBytes)) return Result::TruncatedData;
		if (!detailed) continue;
		if (!state.format.modernObjects)
		{
			if (!state.cursor.skip(LegacyDetailedPlacementBytes))
				return Result::TruncatedData;
			continue;
		}
		const std::size_t detailedBytes = state.format.major < 7
			? V6DetailedPlacementPodBytes
			: CurrentDetailedPlacementPodBytes;
		if (!state.cursor.skip(detailedBytes)) return Result::TruncatedData;
		std::int32_t inventoryCount = 0;
		if (!state.cursor.read(inventoryCount)) return Result::TruncatedData;
		if (inventoryCount < 0 || inventoryCount > InventorySlotCount)
			return Result::InvalidCount;
		for (std::int32_t slot = 0; slot < inventoryCount; ++slot)
		{
			const Result object = ParseObject(state, 1);
			if (object != Result::Success) return object;
			if (!state.cursor.skip(2 * sizeof(std::int32_t)))
				return Result::TruncatedData;
		}
	}
	SetSpan(state, Section::Placements, start);
	return Result::Success;
}

Result ParseExitGrids(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	std::uint16_t count = 0;
	if (!state.cursor.read(count)) return Result::TruncatedData;
	if (count > state.worldCells) return Result::InvalidCount;
	const std::size_t recordBytes =
		state.format.effectiveTailMajor < 7 ? 7 : CurrentExitGridBytes;
	std::size_t bytes = 0;
	if (!CheckedProduct(count, recordBytes, bytes) ||
		!state.cursor.skip(bytes))
		return Result::TruncatedData;
	SetSpan(state, Section::ExitGrids, start);
	return Result::Success;
}

Result ParseDoors(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	std::uint8_t count = 0;
	if (!state.cursor.read(count)) return Result::TruncatedData;
	const std::size_t recordBytes = state.format.effectiveTailMajor < 7
		? LegacyDoorBytes : CurrentDoorBytes;
	if (!state.cursor.skip(static_cast<std::size_t>(count) * recordBytes))
		return Result::TruncatedData;
	SetSpan(state, Section::Doors, start);
	return Result::Success;
}

Result ParseEdgepoints(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	const bool obsoleteLayout = state.embeddedMapVersion < 17;
	const std::size_t arrayCount = obsoleteLayout ? 4 : 8;
	const std::size_t elementBytes = obsoleteLayout
		? sizeof(std::int32_t)
		: (state.format.effectiveTailMajor < 7
			? sizeof(std::int16_t) : sizeof(std::int32_t));
	for (std::size_t index = 0; index < arrayCount; ++index)
	{
		std::uint16_t count = 0;
		std::uint16_t middle = 0;
		if (!state.cursor.read(count) || !state.cursor.read(middle))
			return Result::TruncatedData;
		// Legacy editors saved stale split indices, including middle > count.
		// LoadMapEdgepoints consumes them verbatim; passive geometry never uses
		// edgepoints. Keep the count/byte bounds without imposing a new legacy
		// gameplay invariant. Modern formats retain the canonical split check.
		if (count > state.worldCells ||
			(state.format.effectiveTailMajor >= 7 && middle > count))
			return Result::InvalidCount;
		std::size_t bytes = 0;
		if (!CheckedProduct(count, elementBytes, bytes) ||
			!state.cursor.skip(bytes))
			return Result::TruncatedData;
	}
	SetSpan(state, Section::Edgepoints, start);
	return Result::Success;
}

Result ParseSchedules(ParseState& state) noexcept
{
	const std::size_t start = state.cursor.offset();
	std::uint8_t count = 0;
	if (!state.cursor.read(count)) return Result::TruncatedData;
	if (count > 32) return Result::InvalidCount;
	const std::size_t recordBytes = state.format.effectiveTailMajor < 7
		? LegacyScheduleBytes
		: (state.format.effectiveTailMajor < 8
			? V7ScheduleBytes
			: V8ScheduleBytes);
	if (!state.cursor.skip(static_cast<std::size_t>(count) * recordBytes))
		return Result::TruncatedData;
	SetSpan(state, Section::Schedules, start);
	return Result::Success;
}

Result ParseBody(ParseState& state,
	const FullEngineCoopClientPresentationMapHeader& header) noexcept
{
	const std::size_t headerBytes = header.majorVersion >= 7.0f
		? FullEngineCoopClientSizedMapHeaderBytes
		: FullEngineCoopClientLegacyMapHeaderBytes;
	if (!state.cursor.skip(headerBytes)) return Result::TruncatedData;
	SetSpan(state, Section::Header, 0);

	const std::size_t metadataStart = state.cursor.offset();
	if (!state.cursor.read(state.flags) ||
		!state.cursor.read(state.tilesetId) ||
		!state.cursor.read(state.legacySoldierSize))
		return Result::TruncatedData;
	SetSpan(state, Section::Metadata, metadataStart);
	if ((state.flags & ~FullEngineCoopClientPresentationMapKnownFlags) != 0)
		return Result::UnknownFlags;
	if (state.tilesetId < 0 || state.tilesetId >= MaximumTilesets)
		return Result::InvalidMetadata;

	state.worldCells = static_cast<std::uint32_t>(header.dimensions.columns) *
		header.dimensions.rows;
	const std::size_t heightsStart = state.cursor.offset();
	const std::size_t heightBytes =
		static_cast<std::size_t>(state.worldCells) * 2;
	if (!state.cursor.skip(heightBytes)) return Result::TruncatedData;
	// Legacy IO writes two bytes starting at one-byte MAP_ELEMENT::sHeight.
	// The second byte is the adjacent ubAdjacentSoldierCnt field and can be
	// nonzero in saved maps.  It is transient simulation state, not terrain
	// geometry; Phase B consumes only the first byte and rebuilds actor
	// adjacency from the replicated presentation actors.
	SetSpan(state, Section::Heights, heightsStart);
	Result result = Result::Success;

	const std::size_t countsStart = state.cursor.offset();
	const std::size_t countBytes =
		static_cast<std::size_t>(state.worldCells) * 4;
	if (!state.cursor.skip(countBytes)) return Result::TruncatedData;
	SetSpan(state, Section::LayerCounts, countsStart);
	for (std::uint32_t cell = 0; cell < state.worldCells; ++cell)
	{
		const std::uint8_t* counts = state.cursor.at(countsStart + cell * 4);
		if ((counts[3] & 0xf0u) != 0) return Result::InvalidMetadata;
		const std::array<std::uint8_t, 6> values{{
			static_cast<std::uint8_t>(counts[0] & 0x0fu),
			static_cast<std::uint8_t>(counts[1] & 0x0fu),
			static_cast<std::uint8_t>(counts[1] >> 4),
			static_cast<std::uint8_t>(counts[2] & 0x0fu),
			static_cast<std::uint8_t>(counts[2] >> 4),
			static_cast<std::uint8_t>(counts[3] & 0x0fu)}};
		for (std::size_t layer = 0; layer < values.size(); ++layer)
			state.layerCounts[layer] += values[layer];
	}

	const std::array<Section, 6> layerSections{{Section::Land,
		Section::Objects, Section::Structures, Section::Shadows,
		Section::Roofs, Section::OnRoofs}};
	for (std::size_t layer = 0; layer < layerSections.size(); ++layer)
	{
		const std::size_t entryBytes = layer == 1 ? 3 : 2;
		std::size_t bytes = 0;
		if (!CheckedProduct(state.layerCounts[layer], entryBytes, bytes))
			return Result::InvalidCount;
		result = SkipSection(state, layerSections[layer], bytes);
		if (result != Result::Success) return result;
	}

	if (state.format.russianPadding)
	{
		result = SkipSection(state, Section::LegacyPadding, 37 * 4);
		if (result != Result::Success) return result;
	}
	const std::size_t roomBytes = state.format.minor < 29 ? 1 : 2;
	result = SkipSection(state, Section::Rooms,
		static_cast<std::size_t>(state.worldCells) * roomBytes);
	if (result != Result::Success) return result;

	if ((state.flags & MapWorldItemsSaved) != 0)
	{
		result = ParseWorldItems(state);
		if (result != Result::Success) return result;
	}
	if ((state.flags & MapAmbientLightSaved) != 0)
	{
		result = SkipSection(state, Section::AmbientLight, 3);
		if (result != Result::Success) return result;
	}
	if ((state.flags & MapWorldLightsSaved) != 0)
	{
		result = ParseWorldLights(state);
		if (result != Result::Success) return result;
	}
	result = ParseMapInformation(state);
	if (result != Result::Success) return result;
	if ((state.flags & MapFullSoldierSaved) != 0)
	{
		result = ParsePlacements(state);
		if (result != Result::Success) return result;
	}
	else if (state.placementCount != 0)
	{
		return Result::InvalidMetadata;
	}
	if ((state.flags & MapExitGridsSaved) != 0)
	{
		result = ParseExitGrids(state);
		if (result != Result::Success) return result;
	}
	if ((state.flags & MapDoorTableSaved) != 0)
	{
		result = ParseDoors(state);
		if (result != Result::Success) return result;
	}
	if ((state.flags & MapEdgepointsSaved) != 0)
	{
		result = ParseEdgepoints(state);
		if (result != Result::Success) return result;
	}
	if ((state.flags & MapNpcSchedulesSaved) != 0)
	{
		result = ParseSchedules(state);
		if (result != Result::Success) return result;
	}
	if (state.format.effectiveTailMajor < 7)
	{
		// LoadWorld stops after schedules. Old non-truncating editor saves
		// retain remnants of earlier edgepoint/schedule data after that body;
		// SaveWorld now explicitly deletes the old file before opening it.
		// Preserve these admitted asset bytes as one inert, bounded span only
		// after every declared native section has been completely validated.
		return SkipSection(state, Section::LegacyEditorRemainder,
			state.cursor.remaining());
	}
	return state.cursor.remaining() == 0
		? Result::Success : Result::TrailingData;
}
}

const char* FullEngineCoopClientPresentationMapPlanResultName(
	FullEngineCoopClientPresentationMapPlanResult result) noexcept
{
	switch (result)
	{
		case Result::Success: return "success";
		case Result::InvalidSnapshot: return "invalid authoritative snapshot";
		case Result::AssetOpenFailure: return "exact map asset could not be opened";
		case Result::AssetReadFailure: return "exact map asset could not be read";
		case Result::AssetTooLarge: return "map asset exceeds presentation bound";
		case Result::AllocationFailure: return "map asset allocation failed";
		case Result::TruncatedData: return "truncated map data";
		case Result::UnsupportedVersion: return "unsupported map version";
		case Result::DimensionMismatch: return "authority/map dimension mismatch";
		case Result::UnknownFlags: return "unknown map flags";
		case Result::InvalidMetadata: return "invalid map metadata";
		case Result::InvalidCount: return "invalid or oversized map count";
		case Result::ItemClassificationRequired:
			return "modern map item classification is required";
		case Result::RecursionLimit: return "map object nesting limit exceeded";
		case Result::TrailingData: return "unexpected trailing map data";
	}
	return "unknown presentation map plan result";
}

FullEngineCoopClientPresentationMapPlanResult
ParseFullEngineCoopClientPresentationMap(
	const TacticalSectorSnapshot& sector,
	const TacticalWorldDimensions& expectedDimensions,
	std::vector<std::uint8_t>&& bytes,
	const FullEngineCoopClientPresentationMapItemClassifier& itemClassifier,
	FullEngineCoopClientPresentationMapPlan& output) noexcept
{
	if (!sector.loaded || !IsValidTacticalSectorSnapshot(sector) ||
		!expectedDimensions.valid())
		return Result::InvalidSnapshot;
	if (bytes.empty()) return Result::TruncatedData;
	if (bytes.size() > FullEngineCoopClientPresentationMapMaximumAssetBytes)
		return Result::AssetTooLarge;

	FullEngineCoopClientPresentationMapHeader header;
	const FullEngineCoopClientPresentationMapResult inspected =
		InspectFullEngineCoopClientPresentationMapHeader(
			bytes.data(), bytes.size(), header);
	if (inspected == FullEngineCoopClientPresentationMapResult::TruncatedHeader)
		return Result::TruncatedData;
	if (inspected != FullEngineCoopClientPresentationMapResult::Success)
		return Result::InvalidMetadata;
	if (header.dimensions.columns != expectedDimensions.columns ||
		header.dimensions.rows != expectedDimensions.rows)
		return Result::DimensionMismatch;

	ParseState state(bytes, itemClassifier);
	if (!SupportedMapFormat(
		header.majorVersion, header.minorVersion, state.format))
		return Result::UnsupportedVersion;
	const Result parsed = ParseBody(state, header);
	if (parsed != Result::Success) return parsed;

	FullEngineCoopClientPresentationMapPlan candidate;
	candidate.sector_ = sector;
	candidate.dimensions_ = expectedDimensions;
	candidate.header_ = header;
	candidate.flags_ = state.flags;
	candidate.tilesetId_ = state.tilesetId;
	candidate.legacySoldierSize_ = state.legacySoldierSize;
	candidate.worldCellCount_ = state.worldCells;
	candidate.layerEntryCounts_ = state.layerCounts;
	candidate.placementCount_ = state.placementCount;
	candidate.embeddedMapVersion_ = state.embeddedMapVersion;
	candidate.sections_ = state.sections;
	candidate.bytes_ = std::move(bytes);
	output = std::move(candidate);
	return Result::Success;
}
