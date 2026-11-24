#include "FullEngineCoopClientPresentationPassiveUiRosterTextPlan.h"

#include "FullEngineCoopClientPresentationPassiveUiRosterRaster.h"

#include <Multiplayer/CoopTacticalProtocol.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace
{
bool Contains(const FullEngineCoopClientPassiveUiRect& outer,
	const FullEngineCoopClientPassiveUiRect& inner) noexcept
{
	return outer.valid() && inner.valid() && inner.left >= outer.left &&
		inner.top >= outer.top && inner.right <= outer.right &&
		inner.bottom <= outer.bottom;
}

bool AdjustRect(const FullEngineCoopClientPassiveUiRect& panel,
	const FullEngineCoopClientPassiveUiRect& source,
	std::int32_t leftAdjustment, std::int32_t rightAdjustment,
	FullEngineCoopClientPassiveUiRect& output) noexcept
{
	const std::int64_t left =
		static_cast<std::int64_t>(source.left) + leftAdjustment;
	const std::int64_t right =
		static_cast<std::int64_t>(source.right) + rightAdjustment;
	if (left < (std::numeric_limits<std::int32_t>::min)() ||
		right > (std::numeric_limits<std::int32_t>::max)())
		return false;
	const FullEngineCoopClientPassiveUiRect candidate{
		static_cast<std::int32_t>(left), source.top,
		static_cast<std::int32_t>(right), source.bottom};
	if (!Contains(panel, candidate)) return false;
	output = candidate;
	return true;
}

bool DecodeName(const std::array<std::uint16_t,
		TacticalActorDisplayNameCodeUnits>& source,
	std::array<std::uint16_t,
		FullEngineCoopClientPassiveRosterNameTextCapacity>& output,
	std::size_t& length) noexcept
{
	if (!IsCanonicalTacticalDisplayName(source)) return false;
	output.fill(0);
	length = 0;
	for (std::size_t input = 0; input < source.size(); ++input)
	{
		const std::uint16_t first = source[input];
		if (first == 0) return true;
		if (first >= 0xd800u && first <= 0xdbffu)
		{
			// Canonical validation guarantees the following low surrogate.
			++input;
			output[length++] = static_cast<std::uint16_t>('?');
		}
		else
		{
			output[length++] = first;
		}
	}
	return false;
}

bool FormatActionPoints(std::int16_t value,
	std::array<std::uint16_t,
		FullEngineCoopClientPassiveRosterActionPointTextCapacity>& output,
	std::size_t& length) noexcept
{
	output.fill(0);
	const std::int32_t signedValue = value;
	const bool negative = signedValue < 0;
	std::uint32_t magnitude = negative
		? static_cast<std::uint32_t>(-signedValue)
		: static_cast<std::uint32_t>(signedValue);
	std::array<std::uint16_t, 5> reverse{};
	std::size_t digits = 0;
	do
	{
		if (digits == reverse.size()) return false;
		reverse[digits++] = static_cast<std::uint16_t>(
			static_cast<std::uint16_t>('0') + magnitude % 10u);
		magnitude /= 10u;
	}
	while (magnitude != 0);

	length = 0;
	if (negative) output[length++] = static_cast<std::uint16_t>('-');
	while (digits != 0) output[length++] = reverse[--digits];
	return length < output.size();
}
}

bool PositionFullEngineCoopClientPassiveRosterTextRun(
	const FullEngineCoopClientPassiveUiRect& alignment,
	std::int32_t textWidth, std::int32_t bitmapFontHeight,
	bool topAligned, FullEngineCoopClientPassiveUiPoint& output) noexcept
{
	if (!alignment.valid() || textWidth < 0 || bitmapFontHeight <= 0)
		return false;
	const std::int64_t x = static_cast<std::int64_t>(alignment.left) +
		(static_cast<std::int64_t>(alignment.right - alignment.left) -
			textWidth + 1) / 2;
	const std::int64_t y = topAligned
		? alignment.top
		: static_cast<std::int64_t>(alignment.top) +
			(static_cast<std::int64_t>(
				alignment.bottom - alignment.top) - bitmapFontHeight) / 2;
	if (x < (std::numeric_limits<std::int32_t>::min)() ||
		x > (std::numeric_limits<std::int32_t>::max)() ||
		y < (std::numeric_limits<std::int32_t>::min)() ||
		y > (std::numeric_limits<std::int32_t>::max)())
		return false;
	output = {static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
	return true;
}

bool BuildFullEngineCoopClientPassiveRosterTextPlan(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	FullEngineCoopClientPassiveRosterTextPlan& output) noexcept
{
	if (!panel.valid() ||
		model.roster.size() >
			CoopSession::MaximumCoopTacticalAssignedActors ||
		model.rosterFirstVisible > model.roster.size() ||
		model.rosterFirstVisible %
			FullEngineCoopClientPassiveUiRosterVisibleSlots != 0)
		return false;

	std::array<FullEngineCoopClientPassiveRosterSlotGeometry,
		FullEngineCoopClientPassiveUiRosterVisibleSlots> geometry{};
	for (std::size_t slot = 0; slot < geometry.size(); ++slot)
	{
		if (!BuildFullEngineCoopClientPassiveRosterSlotGeometry(
				panel, slot, geometry[slot]))
			return false;
	}

	FullEngineCoopClientPassiveRosterTextPlan candidate;
	const std::size_t remaining =
		model.roster.size() - model.rosterFirstVisible;
	candidate.visibleSlotCount = (std::min)(remaining,
		FullEngineCoopClientPassiveUiRosterVisibleSlots);
	for (std::size_t slot = 0; slot < candidate.visibleSlotCount; ++slot)
	{
		const std::size_t rosterIndex = model.rosterFirstVisible + slot;
		const FullEngineCoopClientPassiveRosterEntry& entry =
			model.roster[rosterIndex];
		FullEngineCoopClientPassiveRosterTextPlanSlot& planned =
			candidate.slots[slot];
		planned.visibleSlot = slot;
		planned.rosterIndex = rosterIndex;
		planned.selected = entry.selected;
		if (!DecodeName(entry.displayNameUtf16,
				planned.name, planned.nameLength) ||
			!FormatActionPoints(entry.actionPoints,
				planned.actionPoints, planned.actionPointLength) ||
			!AdjustRect(panel, geometry[slot].name, 2, 2,
				planned.nameAlignment) ||
			!AdjustRect(panel, geometry[slot].name, 2, 2,
				planned.nameClip) ||
			!AdjustRect(panel, geometry[slot].actionPoints, 0, 0,
				planned.actionPointAlignment) ||
			!AdjustRect(panel, geometry[slot].actionPoints, -2, 2,
				planned.actionPointClip))
			return false;
	}
	output = candidate;
	return true;
}
