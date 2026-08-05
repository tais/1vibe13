#ifndef LAPTOP_AIM_MEMBER_PROFILE_LAYOUT_H
#define LAPTOP_AIM_MEMBER_PROFILE_LAYOUT_H

#include <array>
#include <cstddef>

namespace AimMemberProfileLayoutModel
{
	struct Point
	{
		int x = 0;
		int y = 0;
	};

	struct Rect
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;

		constexpr int right() const noexcept { return x + width; }
		constexpr int bottom() const noexcept { return y + height; }
	};

	struct TextArea
	{
		Point origin;
		int width = 0;
	};

	struct Anchors
	{
		int screenX = 0;
		int screenY = 0;
		int imageX = 0;
		int imageY = 0;
		int webDeltaY = 0;
	};

	struct StatsLayout
	{
		Point panel;
		Point name;
		int firstLabelX = 0;
		int secondLabelX = 0;
		int firstValueX = 0;
		int secondValueX = 0;
		int firstDotX = 0;
		int secondDotX = 0;
		std::array<int, 6> rows{};
	};

	struct PriceLayout
	{
		Point panel;
		int panelWidth = 0;
		TextArea fee;
		TextArea contract;
		TextArea missionFeeLabel;
		TextArea missionFeeValue;
		TextArea missionFeeOffer;
		TextArea optionalGearLabel;
		TextArea optionalGearCost;
		TextArea medicalDeposit;
	};

	struct BiographyLayout
	{
		TextArea biography;
		Point additionalLabel;
		TextArea additionalText;
	};

	struct InventoryLayout
	{
		Rect firstCell;
		int columns = 0;
		int rows = 0;
		int contentInsetX = 0;
		int countTextWidth = 0;
		int itemNameY = 0;

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
	};

	struct NavigationLayout
	{
		Rect previousButton;
		Rect contactButton;
		Rect nextButton;
		Point pageNumber;
	};

	struct HelpLayout
	{
		TextArea leftClick;
		TextArea rightClick;
		int descriptionOffsetY = 0;
	};

	struct KitButtonLayout
	{
		Rect firstButton;
		int gap = 0;

		constexpr Rect button(std::size_t index) const noexcept
		{
			return {
				firstButton.x + static_cast<int>(index) * (firstButton.width + gap),
				firstButton.y,
				firstButton.width,
				firstButton.height};
		}
	};

	struct Layout
	{
		bool expandedGear = false;
		Rect pageBounds;
		Rect portrait;
		Rect face;
		int faceStatusY = 0;
		TextArea activeMemberTitle;
		StatsLayout stats;
		PriceLayout price;
		BiographyLayout biography;
		InventoryLayout inventory;
		NavigationLayout navigation;
		HelpLayout help;
		KitButtonLayout kitButtons;
	};

	constexpr std::size_t kExpandedInventoryCapacity = 21;

	constexpr bool Contains(const Rect& outer, const Rect& inner) noexcept
	{
		return inner.x >= outer.x && inner.y >= outer.y &&
			inner.right() <= outer.right() &&
			inner.bottom() <= outer.bottom();
	}

	constexpr bool Overlaps(const Rect& first, const Rect& second) noexcept
	{
		return first.x < second.right() && second.x < first.right() &&
			first.y < second.bottom() && second.y < first.bottom();
	}

	constexpr Layout MakeLayout(bool expandedGear, Anchors anchors) noexcept
	{
		Layout result;
		result.expandedGear = expandedGear;
		result.pageBounds = {anchors.imageX, anchors.imageY, 502, 400};

		const int statsY = anchors.imageY + (expandedGear ? 34 : 66);
		result.stats.panel = {anchors.imageX + 121, statsY};
		result.stats.firstLabelX = result.stats.panel.x + 9;
		result.stats.secondLabelX = result.stats.firstLabelX + 129;
		result.stats.firstValueX = result.stats.panel.x + 111;
		result.stats.secondValueX = result.stats.panel.x + 235;
		result.stats.firstDotX = anchors.screenX + 328;
		result.stats.secondDotX = anchors.screenX + 451;
		result.stats.name = {result.stats.firstLabelX, statsY + 7};
		for (std::size_t i = 0; i < result.stats.rows.size(); ++i)
			result.stats.rows[i] = statsY + 34 + static_cast<int>(i) * 15;

		result.portrait = {anchors.imageX + 8, statsY, 110, 126};
		result.face = {
			result.portrait.x + 2, result.portrait.y + 2, 106, 122};
		result.faceStatusY = result.face.y + 107;

		result.price.panel = {anchors.imageX + 377, statsY};
		result.price.panelWidth = 116;
		result.price.fee = {
			{result.price.panel.x + 7, result.stats.name.y}, 37};
		result.price.contract = {
			{result.price.panel.x + 51, result.stats.name.y}, 59};
		result.price.missionFeeLabel = {
			{result.price.panel.x + 19, result.stats.name.y}, 0};
		result.price.missionFeeValue = {
			{result.price.panel.x + 40, expandedGear ? statsY : statsY + 28}, 37};
		result.price.missionFeeOffer = {
			{result.price.panel.x + 3, result.stats.rows[1]}, 110};
		result.price.medicalDeposit = {
			{result.price.panel.x + 5, result.stats.rows[4]},
			result.price.panelWidth - 6};

		if (expandedGear)
		{
			result.activeMemberTitle = {
				{anchors.imageX + 149, anchors.imageY + 8}, 203};
			result.biography.biography = {
				{anchors.screenX + 122,
				 anchors.screenY + 190 + anchors.webDeltaY}, 470};
			result.biography.additionalLabel = {
				anchors.screenX + 122,
				anchors.screenY + 235 + anchors.webDeltaY};
			result.inventory = {
				{anchors.imageX + 24, anchors.imageY + 245, 64, 30},
				7, 3, 3, 59, 0};
			result.navigation.previousButton = {
				anchors.screenX + 218,
				anchors.screenY + 391 + anchors.webDeltaY - 4, 75, 18};
			result.navigation.contactButton = {
				anchors.screenX + 325,
				anchors.screenY + 391 + anchors.webDeltaY - 4, 75, 18};
			result.navigation.nextButton = {
				anchors.screenX + 425,
				anchors.screenY + 391 + anchors.webDeltaY - 4, 75, 18};
			result.help.leftClick = {
				{anchors.screenX + 200,
				 anchors.screenY + 35 + anchors.webDeltaY}, 110};
			result.price.optionalGearLabel = {
				{result.price.contract.origin.x, result.stats.rows[3]},
				result.price.contract.width};
			result.price.optionalGearCost = {
				{result.price.fee.origin.x, result.stats.rows[3]},
				result.price.fee.width};
		}
		else
		{
			result.activeMemberTitle = {
				{anchors.imageX + 149, anchors.imageY + 53}, 203};
			result.biography.biography = {
				{anchors.screenX + 124,
				 anchors.screenY + 223 + anchors.webDeltaY}, 470};
			result.biography.additionalLabel = {
				anchors.screenX + 124,
				anchors.screenY + 269 + anchors.webDeltaY};
			result.inventory = {
				{anchors.imageX + 6, anchors.imageY + 296, 61, 31},
				8, 1, 3, 59, anchors.imageY + 328};
			result.navigation.previousButton = {
				anchors.screenX + 224,
				anchors.screenY + 386 + anchors.webDeltaY - 4, 75, 18};
			result.navigation.contactButton = {
				anchors.screenX + 331,
				anchors.screenY + 386 + anchors.webDeltaY - 4, 75, 18};
			result.navigation.nextButton = {
				anchors.screenX + 431,
				anchors.screenY + 386 + anchors.webDeltaY - 4, 75, 18};
			result.help.leftClick = {
				{anchors.screenX + 116,
				 anchors.screenY + 35 + anchors.webDeltaY}, 110};
			result.price.optionalGearLabel = {
				{result.biography.biography.origin.x,
				 result.inventory.firstCell.y - 13}, 0};
			result.price.optionalGearCost = result.price.optionalGearLabel;
		}

		result.biography.additionalText = {
			{result.biography.additionalLabel.x,
			 result.biography.additionalLabel.y + 15},
			result.biography.biography.width};
		result.navigation.pageNumber = {
			anchors.screenX + 582,
			anchors.screenY + 386 + anchors.webDeltaY + 4};
		result.help.rightClick = {
			{anchors.screenX + 500,
			 anchors.screenY + 35 + anchors.webDeltaY}, 110};
		result.help.descriptionOffsetY = 16;
		result.kitButtons = {
			{anchors.screenX + 143,
			 anchors.screenY + 367 + anchors.webDeltaY - 4, 75, 18}, 15};

		return result;
	}
}

#endif
