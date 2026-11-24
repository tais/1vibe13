#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_ROSTER_RASTER_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_ROSTER_RASTER_H

#include "FullEngineCoopClientPresentationPassiveUi.h"

#include "pixfmt.h"

#include <array>
#include <cstddef>
#include <cstdint>

// Exact rectangles used by one of the stock TEAM panel's six visible slots.
// Text is deliberately left to the native font renderer; this raster owns only
// bounded, data-driven shapes inside these rectangles.
struct FullEngineCoopClientPassiveRosterSlotGeometry
{
	FullEngineCoopClientPassiveUiRect face;
	FullEngineCoopClientPassiveUiRect name;
	FullEngineCoopClientPassiveUiRect life;
	FullEngineCoopClientPassiveUiRect breath;
	FullEngineCoopClientPassiveUiRect actionPoints;
	FullEngineCoopClientPassiveUiRect primaryHand;
	FullEngineCoopClientPassiveUiRect secondaryHand;
	FullEngineCoopClientPassiveUiRect highlight;
};

// Resolves the legacy TEAM slot offsets against the supplied panel origin.
// Every output rectangle must fit inside panel; output is untouched on failure.
bool BuildFullEngineCoopClientPassiveRosterSlotGeometry(
	const FullEngineCoopClientPassiveUiRect& panel,
	std::size_t visibleSlot,
	FullEngineCoopClientPassiveRosterSlotGeometry& output) noexcept;

// Resolves one logical point against the exact, exclusive TEAM-slot highlight
// rectangles for the model's current roster page. The returned identity is
// the complete authority actor identity; output is untouched on failure.
bool HitTestFullEngineCoopClientPassiveRoster(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	std::int32_t x,
	std::int32_t y,
	TacticalEntityId& output) noexcept;

enum class FullEngineCoopClientPassiveRosterColor : std::uint8_t
{
	PortraitBackground,
	PortraitSilhouette,
	BodyBadge,
	SelectedBorder,
	MeterBackground,
	Life,
	Breath,
	HandBorder,
	HandOccupied,
	Condition,
	Jammed
};

inline constexpr std::size_t FullEngineCoopClientPassiveRosterColorCount = 11;
static_assert(static_cast<std::size_t>(
	FullEngineCoopClientPassiveRosterColor::Jammed) + 1 ==
	FullEngineCoopClientPassiveRosterColorCount,
	"passive roster color table must cover every raster color");

using FullEngineCoopClientPassiveRosterPixelColors =
	std::array<PIXEL, FullEngineCoopClientPassiveRosterColorCount>;

// Draws only non-text roster state into a caller-owned packed-pixel surface.
// The surface may have padded rows. No legacy UI, font, item, or line-raster
// state is read or changed, and all writes remain inside exact TEAM geometry.
bool RasterFullEngineCoopClientPassiveUiRoster(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	std::uint8_t* pixels,
	std::uint32_t pitchBytes,
	std::int32_t surfaceWidth,
	std::int32_t surfaceHeight,
	const FullEngineCoopClientPassiveRosterPixelColors& colors) noexcept;

#endif
