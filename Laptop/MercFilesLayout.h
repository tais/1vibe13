#ifndef LAPTOP_MERC_FILES_LAYOUT_H
#define LAPTOP_MERC_FILES_LAYOUT_H

#include "LaptopLayout.h"

#include <cstddef>

namespace MercFilesLayoutModel
{
	using LaptopLayoutModel::Point;
	using LaptopLayoutModel::Rect;

	struct Anchors
	{
		int screenX = 0;
		int screenY = 0;
		int webX = 0;
		int webY = 0;
		int webDeltaY = 0;
	};

	struct InventoryLayout
	{
		Rect firstCell;
		int columns = 0;
		int rows = 0;
		int contentInsetX = 0;
		int countTextOffsetX = 0;
		int countTextOffsetY = 0;
		int countTextWidth = 0;

		constexpr std::size_t capacity() const noexcept
		{
			return static_cast<std::size_t>(columns * rows);
		}

		constexpr Rect cell(std::size_t index) const noexcept
		{
			return {
				firstCell.x + static_cast<int>(index % columns) * firstCell.width,
				firstCell.y + static_cast<int>(index / columns) * firstCell.height,
				firstCell.width,
				firstCell.height};
		}

		constexpr Point contentOrigin(std::size_t index) const noexcept
		{
			const Rect slot = cell(index);
			return {slot.x + contentInsetX, slot.y};
		}

		constexpr Point countTextOrigin(std::size_t index) const noexcept
		{
			const Rect slot = cell(index);
			return {
				slot.x + countTextOffsetX,
				slot.y + countTextOffsetY};
		}
	};

	struct KitButtonLayout
	{
		Rect firstButton;
		int gap = 0;

		constexpr Rect button(std::size_t index) const noexcept
		{
			return {
				firstButton.x + static_cast<int>(index) *
					(firstButton.width + gap),
				firstButton.y,
				firstButton.width,
				firstButton.height};
		}
	};

	struct Layout
	{
		Rect pageBounds;
		InventoryLayout inventory;
		KitButtonLayout kitButtons;
	};

	constexpr std::size_t kInventoryCapacity = 21;
	constexpr std::size_t kKitButtonCount = 5;

	constexpr Layout MakeLayout(Anchors anchors) noexcept
	{
		Layout result;
		result.pageBounds = {anchors.webX, anchors.webY, 502, 400};
		result.inventory = {
			{anchors.webX + 22, anchors.webY + 165, 64, 30},
			7,
			3,
			3,
			2,
			20,
			59};
		result.kitButtons = {
			{anchors.webX + 19,
			 anchors.screenY + 326 + anchors.webDeltaY,
			 75,
			 30},
			20};
		return result;
	}
}

#endif
