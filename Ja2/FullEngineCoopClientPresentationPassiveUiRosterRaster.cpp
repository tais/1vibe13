#include "FullEngineCoopClientPresentationPassiveUiRosterRaster.h"

#include <Multiplayer/CoopTacticalProtocol.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace
{
constexpr std::int64_t SlotStep = 83;

bool Contains(const FullEngineCoopClientPassiveUiRect& outer,
	const FullEngineCoopClientPassiveUiRect& inner) noexcept
{
	return outer.valid() && inner.valid() && inner.left >= outer.left &&
		inner.top >= outer.top && inner.right <= outer.right &&
		inner.bottom <= outer.bottom;
}

bool MakePanelRect(const FullEngineCoopClientPassiveUiRect& panel,
	std::int64_t offsetX, std::int64_t offsetY,
	std::int64_t width, std::int64_t height,
	FullEngineCoopClientPassiveUiRect& output) noexcept
{
	if (!panel.valid() || width <= 0 || height <= 0) return false;
	const std::int64_t left = static_cast<std::int64_t>(panel.left) + offsetX;
	const std::int64_t top = static_cast<std::int64_t>(panel.top) + offsetY;
	const std::int64_t right = left + width;
	const std::int64_t bottom = top + height;
	if (left < (std::numeric_limits<std::int32_t>::min)() ||
		top < (std::numeric_limits<std::int32_t>::min)() ||
		right > (std::numeric_limits<std::int32_t>::max)() ||
		bottom > (std::numeric_limits<std::int32_t>::max)())
		return false;
	const FullEngineCoopClientPassiveUiRect candidate{
		static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
		static_cast<std::int32_t>(right), static_cast<std::int32_t>(bottom)};
	if (!Contains(panel, candidate)) return false;
	output = candidate;
	return true;
}

PIXEL Color(const FullEngineCoopClientPassiveRosterPixelColors& colors,
	FullEngineCoopClientPassiveRosterColor color) noexcept
{
	return colors[static_cast<std::size_t>(color)];
}

struct PixelSurface
{
	PIXEL* pixels = nullptr;
	std::size_t stride = 0;

	void fill(const FullEngineCoopClientPassiveUiRect& rectangle,
		PIXEL color) const noexcept
	{
		for (std::int32_t y = rectangle.top; y < rectangle.bottom; ++y)
		{
			PIXEL* const row = pixels + static_cast<std::size_t>(y) * stride;
			std::fill(row + rectangle.left, row + rectangle.right, color);
		}
	}

	void border(const FullEngineCoopClientPassiveUiRect& rectangle,
		PIXEL color) const noexcept
	{
		const FullEngineCoopClientPassiveUiRect top{
			rectangle.left, rectangle.top, rectangle.right, rectangle.top + 1};
		const FullEngineCoopClientPassiveUiRect bottom{
			rectangle.left, rectangle.bottom - 1,
			rectangle.right, rectangle.bottom};
		const FullEngineCoopClientPassiveUiRect left{
			rectangle.left, rectangle.top + 1,
			rectangle.left + 1, rectangle.bottom - 1};
		const FullEngineCoopClientPassiveUiRect right{
			rectangle.right - 1, rectangle.top + 1,
			rectangle.right, rectangle.bottom - 1};
		fill(top, color);
		fill(bottom, color);
		if (left.valid()) fill(left, color);
		if (right.valid()) fill(right, color);
	}
};

std::int32_t RatioExtent(std::int16_t value, std::int16_t maximum,
	std::int32_t extent) noexcept
{
	if (maximum <= 0 || value <= 0 || extent <= 0) return 0;
	const std::int64_t clamped = (std::min)(
		static_cast<std::int64_t>(value),
		static_cast<std::int64_t>(maximum));
	return static_cast<std::int32_t>(clamped * extent / maximum);
}

void DrawVerticalMeter(const PixelSurface& surface,
	const FullEngineCoopClientPassiveUiRect& rectangle,
	std::int16_t value, std::int16_t maximum,
	PIXEL background, PIXEL foreground) noexcept
{
	surface.fill(rectangle, background);
	const std::int32_t height = rectangle.bottom - rectangle.top;
	const std::int32_t fillHeight = RatioExtent(value, maximum, height);
	if (fillHeight == 0) return;
	surface.fill({rectangle.left, rectangle.bottom - fillHeight,
		rectangle.right, rectangle.bottom}, foreground);
}

void DrawPortrait(const PixelSurface& surface,
	const FullEngineCoopClientPassiveRosterEntry& entry,
	const FullEngineCoopClientPassiveRosterSlotGeometry& geometry,
	const FullEngineCoopClientPassiveRosterPixelColors& colors) noexcept
{
	const auto& face = geometry.face;
	surface.fill(face, Color(colors,
		FullEngineCoopClientPassiveRosterColor::PortraitBackground));

	// A deliberately neutral head-and-shoulders proxy. Portrait identity is not
	// inferred from cold local profile tables; the authority has not published
	// a face asset key yet.
	const std::int32_t centerX = face.left + (face.right - face.left) / 2;
	surface.fill({centerX - 6, face.top + 7,
		centerX + 6, face.top + 17},
		Color(colors,
			FullEngineCoopClientPassiveRosterColor::PortraitSilhouette));
	surface.fill({centerX - 14, face.top + 19,
		centerX + 14, face.bottom - 3},
		Color(colors,
			FullEngineCoopClientPassiveRosterColor::PortraitSilhouette));

	// The small authority-driven body badge varies by stance. A one-pixel notch
	// derived from body type prevents otherwise identical proxy portraits while
	// remaining independent of JA2 sprite/profile tables.
	std::int32_t badgeWidth = 7;
	std::int32_t badgeHeight = 7;
	switch (entry.stance)
	{
		case TacticalStance::Standing:
			badgeWidth = 3;
			badgeHeight = 10;
			break;
		case TacticalStance::Crouched:
			badgeWidth = 8;
			badgeHeight = 6;
			break;
		case TacticalStance::Prone:
			badgeWidth = 12;
			badgeHeight = 3;
			break;
		case TacticalStance::Unknown:
			break;
	}
	const FullEngineCoopClientPassiveUiRect badge{
		face.left + 3, face.bottom - 3 - badgeHeight,
		face.left + 3 + badgeWidth, face.bottom - 3};
	surface.fill(badge,
		Color(colors, FullEngineCoopClientPassiveRosterColor::BodyBadge));
	if ((entry.bodyType & 1u) != 0 && badgeWidth > 2 && badgeHeight > 2)
	{
		surface.fill({badge.left + 1, badge.top + 1,
			badge.left + 2, badge.bottom - 1},
			Color(colors,
				FullEngineCoopClientPassiveRosterColor::PortraitBackground));
	}
}

void DrawHand(const PixelSurface& surface,
	const TacticalHandItemSnapshot& hand,
	const FullEngineCoopClientPassiveUiRect& rectangle,
	const FullEngineCoopClientPassiveRosterPixelColors& colors) noexcept
{
	surface.border(rectangle,
		Color(colors, FullEngineCoopClientPassiveRosterColor::HandBorder));
	if (hand.item == 0) return;

	const FullEngineCoopClientPassiveUiRect contents{
		rectangle.left + 2, rectangle.top + 2,
		rectangle.right - 2, rectangle.bottom - 5};
	surface.fill(contents,
		Color(colors, FullEngineCoopClientPassiveRosterColor::HandOccupied));

	const FullEngineCoopClientPassiveUiRect conditionBackground{
		rectangle.left + 2, rectangle.bottom - 4,
		rectangle.right - 2, rectangle.bottom - 2};
	surface.fill(conditionBackground,
		Color(colors, FullEngineCoopClientPassiveRosterColor::MeterBackground));
	const std::int32_t conditionWidth = RatioExtent(hand.condition, 100,
		conditionBackground.right - conditionBackground.left);
	if (conditionWidth != 0)
	{
		surface.fill({conditionBackground.left, conditionBackground.top,
			conditionBackground.left + conditionWidth,
			conditionBackground.bottom},
			Color(colors, FullEngineCoopClientPassiveRosterColor::Condition));
	}

	if (hand.ammunitionState && hand.ammunitionCondition < 0)
	{
		// A compact solid warning survives both 16- and 32-bit pixel formats.
		surface.fill({rectangle.right - 7, rectangle.top + 2,
			rectangle.right - 2, rectangle.top + 7},
			Color(colors, FullEngineCoopClientPassiveRosterColor::Jammed));
	}
}

bool ValidEntry(const FullEngineCoopClientPassiveRosterEntry& entry) noexcept
{
	return entry.actor.valid() && entry.bodyType < TacticalActorBodyTypeCount &&
		(entry.stance == TacticalStance::Unknown ||
		 entry.stance == TacticalStance::Standing ||
		 entry.stance == TacticalStance::Crouched ||
		 entry.stance == TacticalStance::Prone) &&
		entry.primaryHand.valid() && entry.secondaryHand.valid();
}

bool ValidRosterModel(
	const FullEngineCoopClientPassiveUiModel& model) noexcept
{
	if (model.roster.size() >
			CoopSession::MaximumCoopTacticalAssignedActors ||
		model.rosterFirstVisible > model.roster.size() ||
		model.rosterFirstVisible %
			FullEngineCoopClientPassiveUiRosterVisibleSlots != 0)
		return false;
	for (std::size_t index = 0; index < model.roster.size(); ++index)
	{
		if (!ValidEntry(model.roster[index]) ||
			(index != 0 &&
			 !(model.roster[index - 1].actor < model.roster[index].actor)))
			return false;
	}
	return true;
}

bool ContainsPoint(const FullEngineCoopClientPassiveUiRect& rectangle,
	std::int32_t x, std::int32_t y) noexcept
{
	return rectangle.valid() && x >= rectangle.left && x < rectangle.right &&
		y >= rectangle.top && y < rectangle.bottom;
}
}

bool BuildFullEngineCoopClientPassiveRosterSlotGeometry(
	const FullEngineCoopClientPassiveUiRect& panel,
	std::size_t visibleSlot,
	FullEngineCoopClientPassiveRosterSlotGeometry& output) noexcept
{
	if (visibleSlot >= FullEngineCoopClientPassiveUiRosterVisibleSlots)
		return false;
	const std::int64_t slot = static_cast<std::int64_t>(visibleSlot);
	const std::int64_t x = slot * SlotStep;
	FullEngineCoopClientPassiveRosterSlotGeometry candidate;
	if (!MakePanelRect(panel, 13 + x + (slot > 0 ? 1 : 0), 6,
			48, 43, candidate.face) ||
		!MakePanelRect(panel, 7 + x, 55, 60, 9, candidate.name) ||
		!MakePanelRect(panel, 69 + x, 5, 3, 42, candidate.life) ||
		!MakePanelRect(panel, 75 + x, 5, 3, 42, candidate.breath) ||
		!MakePanelRect(panel, 68 + x, 52, 15, 10,
			candidate.actionPoints) ||
		!MakePanelRect(panel, 8 + x, 67, 58, 23,
			candidate.primaryHand) ||
		!MakePanelRect(panel, 8 + x, 91, 58, 23,
			candidate.secondaryHand) ||
		!MakePanelRect(panel, 4 + x, 2, 84, 114,
			candidate.highlight))
		return false;
	output = candidate;
	return true;
}

bool HitTestFullEngineCoopClientPassiveRoster(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	std::int32_t x,
	std::int32_t y,
	TacticalEntityId& output) noexcept
{
	if (!panel.valid() || !ValidRosterModel(model)) return false;
	std::array<FullEngineCoopClientPassiveRosterSlotGeometry,
		FullEngineCoopClientPassiveUiRosterVisibleSlots> geometry{};
	for (std::size_t slot = 0; slot < geometry.size(); ++slot)
	{
		if (!BuildFullEngineCoopClientPassiveRosterSlotGeometry(
				panel, slot, geometry[slot]))
			return false;
	}
	const std::size_t remaining =
		model.roster.size() - model.rosterFirstVisible;
	const std::size_t visible = (std::min)(remaining,
		FullEngineCoopClientPassiveUiRosterVisibleSlots);
	for (std::size_t slot = 0; slot < visible; ++slot)
	{
		if (!ContainsPoint(geometry[slot].highlight, x, y)) continue;
		const TacticalEntityId actor =
			model.roster[model.rosterFirstVisible + slot].actor;
		if (!actor.valid()) return false;
		output = actor;
		return true;
	}
	return false;
}

bool RasterFullEngineCoopClientPassiveUiRoster(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiRect& panel,
	std::uint8_t* pixels,
	std::uint32_t pitchBytes,
	std::int32_t surfaceWidth,
	std::int32_t surfaceHeight,
	const FullEngineCoopClientPassiveRosterPixelColors& colors) noexcept
{
	if (pixels == nullptr || !panel.valid() || surfaceWidth <= 0 ||
		surfaceHeight <= 0 || panel.right > surfaceWidth ||
		panel.bottom > surfaceHeight || pitchBytes % sizeof(PIXEL) != 0 ||
		reinterpret_cast<std::uintptr_t>(pixels) % alignof(PIXEL) != 0 ||
		static_cast<std::uint64_t>(surfaceWidth) * sizeof(PIXEL) > pitchBytes ||
		model.roster.size() >
			CoopSession::MaximumCoopTacticalAssignedActors ||
		model.rosterFirstVisible > model.roster.size())
		return false;
	const std::uint64_t byteExtent =
		static_cast<std::uint64_t>(pitchBytes) *
		static_cast<std::uint64_t>(surfaceHeight);
	if (byteExtent > (std::numeric_limits<std::size_t>::max)() ||
		byteExtent > static_cast<std::uint64_t>(
			(std::numeric_limits<std::ptrdiff_t>::max)()))
		return false;

	std::array<FullEngineCoopClientPassiveRosterSlotGeometry,
		FullEngineCoopClientPassiveUiRosterVisibleSlots> geometry{};
	for (std::size_t slot = 0; slot < geometry.size(); ++slot)
	{
		if (!BuildFullEngineCoopClientPassiveRosterSlotGeometry(
				panel, slot, geometry[slot]))
			return false;
	}
	const std::size_t remaining =
		model.roster.size() - model.rosterFirstVisible;
	const std::size_t visible = (std::min)(remaining,
		FullEngineCoopClientPassiveUiRosterVisibleSlots);
	for (std::size_t slot = 0; slot < visible; ++slot)
	{
		if (!ValidEntry(model.roster[model.rosterFirstVisible + slot]))
			return false;
	}

	const PixelSurface surface{
		reinterpret_cast<PIXEL*>(pixels), pitchBytes / sizeof(PIXEL)};
	for (std::size_t slot = 0; slot < visible; ++slot)
	{
		const FullEngineCoopClientPassiveRosterEntry& entry =
			model.roster[model.rosterFirstVisible + slot];
		DrawPortrait(surface, entry, geometry[slot], colors);
		DrawVerticalMeter(surface, geometry[slot].life,
			entry.life, entry.maximumLife,
			Color(colors,
				FullEngineCoopClientPassiveRosterColor::MeterBackground),
			Color(colors, FullEngineCoopClientPassiveRosterColor::Life));
		DrawVerticalMeter(surface, geometry[slot].breath,
			entry.breath, entry.maximumBreath,
			Color(colors,
				FullEngineCoopClientPassiveRosterColor::MeterBackground),
			Color(colors, FullEngineCoopClientPassiveRosterColor::Breath));
		DrawHand(surface, entry.primaryHand,
			geometry[slot].primaryHand, colors);
		DrawHand(surface, entry.secondaryHand,
			geometry[slot].secondaryHand, colors);
	}
	// Selection is applied after every slot body so the legacy one-pixel overlap
	// between adjacent highlight rectangles cannot hide the selected outline.
	for (std::size_t slot = 0; slot < visible; ++slot)
	{
		if (model.roster[model.rosterFirstVisible + slot].selected)
			surface.border(geometry[slot].highlight,
				Color(colors,
					FullEngineCoopClientPassiveRosterColor::SelectedBorder));
	}
	return true;
}
