#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_MAP_PLAN_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_MAP_PLAN_H

#include "FullEngineCoopClientPresentationMap.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Phase A of passive tactical presentation.  The parser owns and validates an
// exact authority-selected map asset without invoking any JA2 world loader.
// The resulting immutable bytes and section boundaries are the only input a
// later, explicitly mutating presentation-world phase is expected to consume.
enum class FullEngineCoopClientPresentationMapPlanResult : std::uint8_t
{
	Success,
	InvalidSnapshot,
	AssetOpenFailure,
	AssetReadFailure,
	AssetTooLarge,
	AllocationFailure,
	TruncatedData,
	UnsupportedVersion,
	DimensionMismatch,
	UnknownFlags,
	InvalidMetadata,
	InvalidCount,
	ItemClassificationRequired,
	RecursionLimit,
	TrailingData
};

const char* FullEngineCoopClientPresentationMapPlanResultName(
	FullEngineCoopClientPresentationMapPlanResult result) noexcept;

enum class FullEngineCoopClientPresentationMapSection : std::uint8_t
{
	Header,
	Metadata,
	Heights,
	LayerCounts,
	Land,
	Objects,
	Structures,
	Shadows,
	Roofs,
	OnRoofs,
	LegacyPadding,
	Rooms,
	WorldItems,
	AmbientLight,
	WorldLights,
	MapInformation,
	Placements,
	ExitGrids,
	Doors,
	Edgepoints,
	Schedules,
	// Old non-truncating editor saves can retain bytes beyond the complete
	// native body. They belong to the admitted asset, never to replayed state.
	LegacyEditorRemainder,
	Count
};

struct FullEngineCoopClientPresentationMapByteSpan
{
	std::size_t offset = 0;
	std::size_t size = 0;
	bool present = false;

	bool validFor(std::size_t byteCount) const noexcept
	{
		return !present || (offset <= byteCount && size <= byteCount - offset);
	}
};

enum class FullEngineCoopClientPresentationMapItemKind : std::uint8_t
{
	Ordinary,
	LoadBearingEquipment
};

// Modern map objects conditionally append an LBENODE according to admitted
// Items.xml data.  Injecting this read-only lookup keeps the byte parser pure
// and makes the content dependency explicit instead of consulting globals.
using FullEngineCoopClientPresentationMapItemKindLookup =
	FullEngineCoopClientPresentationMapItemKind (*)(
		std::uint16_t item, const void* context) noexcept;

struct FullEngineCoopClientPresentationMapItemClassifier
{
	FullEngineCoopClientPresentationMapItemKindLookup lookup = nullptr;
	const void* context = nullptr;
};

inline constexpr std::uint32_t
	FullEngineCoopClientPresentationMapKnownFlags = 0x000001ffu;
inline constexpr std::size_t
	FullEngineCoopClientPresentationMapMaximumAssetBytes =
		256u * 1024u * 1024u;

class FullEngineCoopClientPresentationMapPlan
{
public:
	FullEngineCoopClientPresentationMapPlan() = default;
	FullEngineCoopClientPresentationMapPlan(
		const FullEngineCoopClientPresentationMapPlan&) = delete;
	FullEngineCoopClientPresentationMapPlan& operator=(
		const FullEngineCoopClientPresentationMapPlan&) = delete;
	FullEngineCoopClientPresentationMapPlan(
		FullEngineCoopClientPresentationMapPlan&&) noexcept = default;
	FullEngineCoopClientPresentationMapPlan& operator=(
		FullEngineCoopClientPresentationMapPlan&&) noexcept = default;

	const TacticalSectorSnapshot& sector() const noexcept { return sector_; }
	const TacticalWorldDimensions& dimensions() const noexcept
	{
		return dimensions_;
	}
	const FullEngineCoopClientPresentationMapHeader& header() const noexcept
	{
		return header_;
	}
	std::uint32_t flags() const noexcept { return flags_; }
	std::int32_t tilesetId() const noexcept { return tilesetId_; }
	std::uint32_t legacySoldierSize() const noexcept
	{
		return legacySoldierSize_;
	}
	std::uint32_t worldCellCount() const noexcept { return worldCellCount_; }
	const std::array<std::uint32_t, 6>& layerEntryCounts() const noexcept
	{
		return layerEntryCounts_;
	}
	std::uint16_t placementCount() const noexcept { return placementCount_; }
	std::uint8_t embeddedMapVersion() const noexcept
	{
		return embeddedMapVersion_;
	}
	const std::vector<std::uint8_t>& bytes() const noexcept { return bytes_; }
	const FullEngineCoopClientPresentationMapByteSpan& section(
		FullEngineCoopClientPresentationMapSection value) const noexcept
	{
		return sections_[static_cast<std::size_t>(value)];
	}

private:
	friend FullEngineCoopClientPresentationMapPlanResult
	ParseFullEngineCoopClientPresentationMap(
		const TacticalSectorSnapshot&, const TacticalWorldDimensions&,
		std::vector<std::uint8_t>&&,
		const FullEngineCoopClientPresentationMapItemClassifier&,
		FullEngineCoopClientPresentationMapPlan&) noexcept;

	TacticalSectorSnapshot sector_;
	TacticalWorldDimensions dimensions_;
	FullEngineCoopClientPresentationMapHeader header_;
	std::uint32_t flags_ = 0;
	std::int32_t tilesetId_ = -1;
	std::uint32_t legacySoldierSize_ = 0;
	std::uint32_t worldCellCount_ = 0;
	std::array<std::uint32_t, 6> layerEntryCounts_{};
	std::uint16_t placementCount_ = 0;
	std::uint8_t embeddedMapVersion_ = 0;
	std::array<FullEngineCoopClientPresentationMapByteSpan,
		static_cast<std::size_t>(
			FullEngineCoopClientPresentationMapSection::Count)> sections_{};
	std::vector<std::uint8_t> bytes_;
};

// Transactional pure parser.  It consumes an already-read exact asset on
// success and leaves output untouched on every failure.
FullEngineCoopClientPresentationMapPlanResult
ParseFullEngineCoopClientPresentationMap(
	const TacticalSectorSnapshot& sector,
	const TacticalWorldDimensions& expectedDimensions,
	std::vector<std::uint8_t>&& bytes,
	const FullEngineCoopClientPresentationMapItemClassifier& itemClassifier,
	FullEngineCoopClientPresentationMapPlan& output) noexcept;

// Thin production adapter: validates the authority key, opens exactly
// MAPS\\<key>, obtains a bounded size, performs one full FileRead, closes the
// file, and invokes the pure parser with a read-only admitted item classifier.
FullEngineCoopClientPresentationMapPlanResult
ReadFullEngineCoopClientPresentationMapAsset(
	const TacticalSectorSnapshot& sector,
	const TacticalWorldDimensions& expectedDimensions,
	FullEngineCoopClientPresentationMapPlan& output) noexcept;

#endif
