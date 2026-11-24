#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_ROSTER_TEXT_PLAN_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_ROSTER_TEXT_PLAN_H

#include "FullEngineCoopClientPresentationPassiveUi.h"

#include <array>
#include <cstddef>
#include <cstdint>

// Text preparation stays independent of the global JA2 font state.  The
// native renderer can validate glyph availability for this complete plan
// before locking or changing the frame buffer.
inline constexpr std::size_t
	FullEngineCoopClientPassiveRosterNameTextCapacity =
		TacticalActorDisplayNameCodeUnits + 1;
inline constexpr std::size_t
	FullEngineCoopClientPassiveRosterActionPointTextCapacity = 7;

struct FullEngineCoopClientPassiveRosterTextPlanSlot
{
	std::size_t visibleSlot = 0;
	std::size_t rosterIndex = 0;
	bool selected = false;

	std::array<std::uint16_t,
		FullEngineCoopClientPassiveRosterNameTextCapacity> name{};
	std::size_t nameLength = 0;
	FullEngineCoopClientPassiveUiRect nameAlignment;
	FullEngineCoopClientPassiveUiRect nameClip;

	std::array<std::uint16_t,
		FullEngineCoopClientPassiveRosterActionPointTextCapacity>
		actionPoints{};
	std::size_t actionPointLength = 0;
	// Center against the exact stock 15-pixel AP box.  The clip is two pixels
	// wider on each side so a centered 18-pixel three-digit run is not cut.
	FullEngineCoopClientPassiveUiRect actionPointAlignment;
	FullEngineCoopClientPassiveUiRect actionPointClip;
};

struct FullEngineCoopClientPassiveRosterTextPlan
{
	std::array<FullEngineCoopClientPassiveRosterTextPlanSlot,
		FullEngineCoopClientPassiveUiRosterVisibleSlots> slots{};
	std::size_t visibleSlotCount = 0;
};

// Applies the stock TEAM text-origin rule without consulting global font
// state. Names use the legacy +1 centering bias; AP text is top-aligned even
// though the legacy code computes and then intentionally ignores centered Y.
bool PositionFullEngineCoopClientPassiveRosterTextRun(
	const FullEngineCoopClientPassiveUiRect& alignment,
	std::int32_t textWidth,
	std::int32_t bitmapFontHeight,
	bool topAligned,
	FullEngineCoopClientPassiveUiPoint& output) noexcept;

// Builds one fixed-capacity, pointer-free text plan transactionally.  Input
// names must use the snapshot's canonical terminated UTF-16 form.  A valid
// surrogate pair becomes one '?' because JA2's stock glyph table is BMP-only.
bool BuildFullEngineCoopClientPassiveRosterTextPlan(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	FullEngineCoopClientPassiveRosterTextPlan& output) noexcept;

#endif
