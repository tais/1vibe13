#ifndef LAPTOP_AIM_WEBSITE_LAYOUT_H
#define LAPTOP_AIM_WEBSITE_LAYOUT_H

#include "LaptopLayout.h"

#include <cstddef>

namespace AimWebsiteLayoutModel
{
	using LaptopLayoutModel::Point;
	using LaptopLayoutModel::Rect;
	using LaptopLayoutModel::TextArea;

	struct ScreenAnchors
	{
		int screenX = 0;
		int screenY = 0;
		int webDeltaY = 0;
	};

	struct PageAnchors
	{
		int screenX = 0;
		int screenY = 0;
		int imageX = 0;
		int imageY = 0;
		int webDeltaY = 0;
	};

	struct PointSequence
	{
		Point first;
		Point step;

		constexpr Point at(std::size_t index) const noexcept
		{
			return {
				first.x + static_cast<int>(index) * step.x,
				first.y + static_cast<int>(index) * step.y};
		}
	};

	struct RectSequence
	{
		Rect first;
		Point step;

		constexpr Rect at(std::size_t index) const noexcept
		{
			return {
				first.x + static_cast<int>(index) * step.x,
				first.y + static_cast<int>(index) * step.y,
				first.width,
				first.height};
		}
	};

	struct AimDefaultsLayout
	{
		bool expandedGear = false;
		Rect pageBounds;
		Rect logo;
		Rect firstBackgroundTile;
		int backgroundColumns = 0;
		int backgroundRows = 0;

		constexpr std::size_t backgroundTileCount() const noexcept
		{
			return static_cast<std::size_t>(
				backgroundColumns * backgroundRows);
		}

		constexpr Rect backgroundTile(std::size_t index) const noexcept
		{
			return {
				firstBackgroundTile.x + static_cast<int>(index % backgroundColumns) *
					firstBackgroundTile.width,
				firstBackgroundTile.y + static_cast<int>(index / backgroundColumns) *
					firstBackgroundTile.height,
				firstBackgroundTile.width,
				firstBackgroundTile.height};
		}
	};

	constexpr AimDefaultsLayout MakeAimDefaultsLayout(
		bool expandedGear, PageAnchors anchors) noexcept
	{
		AimDefaultsLayout result;
		result.expandedGear = expandedGear;
		result.pageBounds = {anchors.imageX, anchors.imageY, 502, 400};
		result.logo = expandedGear
			? Rect{anchors.imageX + 3, anchors.imageY + 3, 102, 26}
			: Rect{anchors.imageX + 149, anchors.imageY + 3, 203, 51};
		result.firstBackgroundTile = {
			anchors.imageX, anchors.imageY, 125, 100};
		result.backgroundColumns = 4;
		result.backgroundRows = 4;
		return result;
	}

	struct VideoConferenceLayout
	{
		static constexpr std::size_t kTitleAnimationIterations = 18;

		ScreenAnchors anchors;
		Rect terminal;
		Rect titleBarSource;
		Point contractImage;
		Point closeButton;
		Point name;
		Point contractChargeLabel;
		Rect contractChargeAmount;
		TextArea oneTimeFeeOffer;
		PointSequence contractButtons;
		PointSequence equipmentButtons;
		PointSequence contactButtons;
		PointSequence authorizationButtons;
		Point unavailableHangUp;
		Rect face;
		Rect popup;
		Point popupButton;
		int popupFirstTextY = 0;
		int talkingTextPopupY = 0;

		constexpr Point selectionLight(
			Point button, bool pressed) const noexcept
		{
			return {button.x + 105, button.y + (pressed ? 8 : 7)};
		}

		constexpr Rect titleFrame(std::size_t frame) const noexcept
		{
			const int boundedFrame = frame > kTitleAnimationIterations
				? static_cast<int>(kTitleAnimationIterations)
				: static_cast<int>(frame);
			const int iterations = static_cast<int>(kTitleAnimationIterations);
			const int startY = 382 + anchors.webDeltaY;
			const int left =
				(331 * iterations - (331 - 125) * boundedFrame) /
				iterations;
			const int right =
				(405 * iterations + (490 - 405) * boundedFrame) /
				iterations;
			const int top =
				(startY * iterations - (startY - 96) * boundedFrame) /
				iterations;
			return {
				anchors.screenX + left,
				anchors.screenY + top,
				right - left,
				titleBarSource.height};
		}
	};

	constexpr VideoConferenceLayout MakeVideoConferenceLayout(
		ScreenAnchors anchors) noexcept
	{
		VideoConferenceLayout result;
		result.anchors = anchors;
		result.terminal = {
			anchors.screenX + 125,
			anchors.screenY + 97 + anchors.webDeltaY,
			368,
			150};
		result.titleBarSource = {0, 0, 368, 21};
		result.contractImage = {
			result.terminal.x + 6, result.terminal.y + 130};
		result.closeButton = {
			result.terminal.x + 348, result.terminal.y + 3};
		result.name = {result.terminal.x + 7, result.terminal.y + 5};
		result.contractChargeLabel = {
			result.terminal.x + 7, result.terminal.y + 118};
		result.contractChargeAmount = {
			result.terminal.x + 7, result.terminal.y + 131, 98, 12};
		result.oneTimeFeeOffer = {
			{result.terminal.x + 115, result.terminal.y + 45}, 245};
		result.contractButtons = {
			{result.terminal.x + 113, result.terminal.y + 35}, {0, 23}};
		result.equipmentButtons = {
			{result.terminal.x + 235, result.terminal.y + 35}, {0, 23}};
		result.contactButtons = {
			{result.terminal.x + 113, result.terminal.y + 62}, {122, 0}};
		result.authorizationButtons = {
			{result.terminal.x + 113, result.terminal.y + 112}, {122, 0}};
		result.unavailableHangUp = {
			anchors.screenX + 290, result.terminal.y + 62};
		result.face = {
			result.terminal.x + 8, result.terminal.y + 27, 96, 86};
		result.popup = {
			anchors.screenX + 260,
			anchors.screenY + 140 + anchors.webDeltaY,
			162,
			100};
		result.popupButton = {
			result.popup.x + 20, result.popup.y + 62};
		result.popupFirstTextY = result.popup.y + 6;
		result.talkingTextPopupY =
			anchors.screenY + 255 + anchors.webDeltaY;
		return result;
	}

	struct FacialIndexGrid
	{
		Rect firstCell;
		int columns = 0;
		int rows = 0;
		int columnGap = 0;
		int rowGap = 0;

		constexpr std::size_t capacity() const noexcept
		{
			return static_cast<std::size_t>(columns * rows);
		}

		constexpr Rect cell(std::size_t index) const noexcept
		{
			return {
				firstCell.x + static_cast<int>(index % columns) *
					(firstCell.width + columnGap),
				firstCell.y + static_cast<int>(index / columns) *
					(firstCell.height + rowGap),
				firstCell.width,
				firstCell.height};
		}

		constexpr Rect face(std::size_t index) const noexcept
		{
			const Rect portrait = cell(index);
			return {portrait.x + 2, portrait.y + 2, 48, 43};
		}

		constexpr TextArea nickname(std::size_t index) const noexcept
		{
			const Rect portrait = cell(index);
			return {{portrait.x - 2, portrait.y + 49}, 56};
		}

		constexpr TextArea status(std::size_t index) const noexcept
		{
			const Rect portrait = cell(index);
			return {{portrait.x + 3, portrait.y + 23}, 48};
		}
	};

	struct HelpLayout
	{
		TextArea leftClick;
		TextArea rightClick;
		int descriptionOffsetY = 0;
	};

	struct FacialIndexLayout
	{
		bool expandedGear = false;
		Rect pageBounds;
		Rect pageButton;
		TextArea memberTitle;
		FacialIndexGrid grid;
		HelpLayout help;
	};

	constexpr FacialIndexLayout MakeFacialIndexLayout(
		bool expandedGear, PageAnchors anchors) noexcept
	{
		FacialIndexLayout result;
		result.expandedGear = expandedGear;
		result.pageBounds = {anchors.imageX, anchors.imageY, 502, 400};
		result.pageButton = {anchors.imageX + 6, anchors.imageY + 35, 75, 18};
		result.memberTitle = {
			{anchors.imageX + 155,
			 anchors.imageY + (expandedGear ? 38 : 55)},
			190};
		result.grid = {
			{anchors.imageX + 6, anchors.imageY + 69, 52, 48},
			8,
			5,
			10,
			13};
		result.help.leftClick = {
			{anchors.screenX + (expandedGear ? 200 : 116),
			 anchors.screenY + 35 + anchors.webDeltaY},
			110};
		result.help.rightClick = {
			{anchors.screenX + 500,
			 anchors.screenY + 35 + anchors.webDeltaY},
			110};
		result.help.descriptionOffsetY = 16;
		return result;
	}

	constexpr std::size_t kFacialIndexPageCapacity = 40;
	constexpr std::size_t kFacialIndexMaximumPages = 3;

	constexpr std::size_t FacialIndexPageCount(
		std::size_t profileCount) noexcept
	{
		if (profileCount == 0)
			return 1;
		const std::size_t count =
			(profileCount + kFacialIndexPageCapacity - 1) /
			kFacialIndexPageCapacity;
		return count > kFacialIndexMaximumPages
			? kFacialIndexMaximumPages : count;
	}

	constexpr std::size_t NormalizeFacialIndexPageStart(
		std::size_t profileCount, std::size_t pageStart) noexcept
	{
		const std::size_t page = pageStart / kFacialIndexPageCapacity;
		return page < FacialIndexPageCount(profileCount) &&
			pageStart % kFacialIndexPageCapacity == 0
			? pageStart : 0;
	}

	constexpr std::size_t NextFacialIndexPageStart(
		std::size_t profileCount, std::size_t pageStart) noexcept
	{
		const std::size_t pageCount = FacialIndexPageCount(profileCount);
		const std::size_t normalized =
			NormalizeFacialIndexPageStart(profileCount, pageStart);
		return ((normalized / kFacialIndexPageCapacity + 1) % pageCount) *
			kFacialIndexPageCapacity;
	}

	constexpr std::size_t PreviousFacialIndexPageStart(
		std::size_t profileCount, std::size_t pageStart) noexcept
	{
		const std::size_t pageCount = FacialIndexPageCount(profileCount);
		const std::size_t normalized =
			NormalizeFacialIndexPageStart(profileCount, pageStart);
		const std::size_t page = normalized / kFacialIndexPageCapacity;
		return ((page + pageCount - 1) % pageCount) *
			kFacialIndexPageCapacity;
	}

	constexpr std::size_t FacialIndexVisibleSlotCount(
		std::size_t profileCount, std::size_t pageStart) noexcept
	{
		if (pageStart >= profileCount)
			return 0;
		const std::size_t remaining = profileCount - pageStart;
		return remaining < kFacialIndexPageCapacity
			? remaining : kFacialIndexPageCapacity;
	}

	constexpr int FacialIndexPageTextIndex(
		std::size_t profileCount, std::size_t pageStart) noexcept
	{
		const std::size_t normalized =
			NormalizeFacialIndexPageStart(profileCount, pageStart);
		const int page = static_cast<int>(
			normalized / kFacialIndexPageCapacity);
		if (profileCount > kFacialIndexPageCapacity * 2)
			return 2 + page;
		if (profileCount > kFacialIndexPageCapacity)
			return page;
		return 5;
	}

	struct PolicyLayout
	{
		TextArea title;
		RectSequence tocButtons;
		Point tocTextInset;
		PointSequence menuButtons;
		PointSequence agreementButtons;
	};

	constexpr PolicyLayout MakePolicyLayout(
		bool statementPage, bool expandedGear, PageAnchors anchors) noexcept
	{
		PolicyLayout result;
		result.title = {{
			anchors.imageX + 149,
			anchors.imageY +
				(statementPage ? 104 : (expandedGear ? 38 : 65))},
			203};
		result.tocButtons = {
			{anchors.screenX + 259,
			 anchors.screenY + 115 + anchors.webDeltaY,
			 205,
			 19},
			{0, 25}};
		result.tocTextInset = {5, 5};
		result.menuButtons = {
			{anchors.imageX + 40,
			 anchors.screenY + 390 + anchors.webDeltaY},
			{115, 0}};
		result.agreementButtons = {
			{anchors.imageX + 150,
			 anchors.screenY + 350 + anchors.webDeltaY},
			{125, 0}};
		return result;
	}

	constexpr std::size_t kSortCriterionCount = 13;
	constexpr std::size_t kSortControlCount = 15;
	constexpr std::size_t kSortNavigationCount = 3;

	struct SortLayout
	{
		Rect pageBounds;
		Rect sortPanel;
		TextArea memberTitle;
		Point sortTitle;
		RectSequence navigationArtwork;
		ScreenAnchors anchors;

		constexpr Point criterionText(std::size_t criterion) const noexcept
		{
			if (criterion == kSortCriterionCount - 1)
				return {sortPanel.x + 22, sortPanel.y + 23};
			return {
				sortPanel.x + 22 + static_cast<int>(criterion / 3) * 96,
				sortPanel.y + 36 + static_cast<int>(criterion % 3) * 13};
		}

		constexpr Rect control(std::size_t mode) const noexcept
		{
			if (mode < 12)
				return {
					sortPanel.x + 9 + static_cast<int>(mode / 3) * 96,
					sortPanel.y + 34 + static_cast<int>(mode % 3) * 13,
					10,
					10};
			if (mode == 12)
				return {sortPanel.x + 9, sortPanel.y + 21, 10, 10};
			if (mode == 13)
				return {sortPanel.x + 372, sortPanel.y + 4, 10, 10};
			if (mode == 14)
				return {sortPanel.x + 372, sortPanel.y + 17, 10, 10};
			return {};
		}

		constexpr bool hasControl(std::size_t mode) const noexcept
		{
			return mode < kSortControlCount;
		}

		constexpr Rect criterionHitbox(
			std::size_t criterion, int textWidth) const noexcept
		{
			if (criterion >= kSortCriterionCount)
				return {};
			const Rect checkbox = control(criterion);
			const Point text = criterionText(criterion);
			int right = text.x + textWidth - 3;
			if (right < checkbox.right()) right = checkbox.right();
			if (right > sortPanel.right()) right = sortPanel.right();
			return {
				checkbox.x, checkbox.y, right - checkbox.x, checkbox.height};
		}

		constexpr TextArea orderText(std::size_t mode) const noexcept
		{
			return {{
				sortPanel.x + 268,
				anchors.screenY + (mode == 14 ? 141 : 128) +
					anchors.webDeltaY},
				100};
		}

		constexpr Rect orderHitbox(
			std::size_t mode, int textWidth) const noexcept
		{
			if (mode != 13 && mode != 14)
				return {};
			const Rect checkbox = control(mode);
			int left = checkbox.x - textWidth - 6;
			if (left < sortPanel.x) left = sortPanel.x;
			return {
				left, checkbox.y, checkbox.right() - left, checkbox.height};
		}

		constexpr Point navigationText(std::size_t index) const noexcept
		{
			return {
				anchors.screenX + 266,
				anchors.screenY +
					(index == 0 ? 230 : (index == 1 ? 293 : 351)) +
					anchors.webDeltaY};
		}
	};

	constexpr SortLayout MakeSortLayout(PageAnchors anchors) noexcept
	{
		SortLayout result;
		result.pageBounds = {anchors.imageX, anchors.imageY, 502, 400};
		result.sortPanel = {
			anchors.imageX + 53, anchors.imageY + 96, 394, 81};
		result.memberTitle = {{
			result.sortPanel.x,
			anchors.screenY + 105 + anchors.webDeltaY},
			result.sortPanel.width};
		result.sortTitle = {
			result.sortPanel.x + 9, result.sortPanel.y + 8};
		result.navigationArtwork = {
			{anchors.imageX + 89, anchors.imageY + 184, 54, 54},
			{0, 60}};
		result.anchors = {
			anchors.screenX, anchors.screenY, anchors.webDeltaY};
		return result;
	}

	constexpr std::size_t kArchiveColumns = 5;
	constexpr std::size_t kArchiveRows = 4;
	constexpr std::size_t kArchivePageCapacity =
		kArchiveColumns * kArchiveRows;
	constexpr std::size_t kArchivePageCount = 4;

	struct ArchiveGrid
	{
		Rect firstFrame;
		Point step;

		constexpr std::size_t capacity() const noexcept
		{
			return kArchivePageCapacity;
		}

		constexpr Rect frame(std::size_t slot) const noexcept
		{
			return {
				firstFrame.x + static_cast<int>(slot % kArchiveColumns) * step.x,
				firstFrame.y + static_cast<int>(slot / kArchiveColumns) * step.y,
				firstFrame.width,
				firstFrame.height};
		}

		constexpr Point face(std::size_t slot) const noexcept
		{
			const Rect bounds = frame(slot);
			return {bounds.x + 4, bounds.y + 4};
		}

		constexpr Rect hitbox(std::size_t slot) const noexcept
		{
			const Rect bounds = frame(slot);
			return {bounds.x, bounds.y, 56, 50};
		}

		constexpr TextArea nickname(std::size_t slot) const noexcept
		{
			const Rect bounds = frame(slot);
			return {{bounds.x + 5, bounds.y + 55}, 56};
		}
	};

	struct ArchivePopupLayout
	{
		Point origin;
		int width = 0;
		int textWidth = 0;
		int sectionHeight = 0;
		int shadowGap = 0;
		Rect facePanel;
		Point name;
		TextArea description;
		int doneX = 0;

		constexpr Point section(std::size_t index) const noexcept
		{
			return {
				origin.x,
				origin.y + static_cast<int>(index) * sectionHeight};
		}

		constexpr Rect shadow(std::size_t index) const noexcept
		{
			const Point position = section(index);
			return {
				position.x + shadowGap,
				position.y + shadowGap,
				width,
				sectionHeight - 1};
		}

		constexpr Rect doneButton(std::size_t middleSections) const noexcept
		{
			const Point bottom = section(middleSections + 1);
			return {doneX, bottom.y - 16, 36, 16};
		}

		constexpr Rect doneHitbox(std::size_t middleSections) const noexcept
		{
			const Rect button = doneButton(middleSections);
			return {button.x - 2, button.y, button.width, button.height};
		}
	};

	struct ArchiveLayout
	{
		Rect pageBounds;
		TextArea title;
		ArchiveGrid grid;
		Rect pageButton;
		Rect pageControlsInvalidation;
		ArchivePopupLayout popup;
	};

	constexpr ArchiveLayout MakeArchiveLayout(PageAnchors anchors) noexcept
	{
		ArchiveLayout result;
		result.pageBounds = {anchors.imageX, anchors.imageY, 502, 400};
		result.title = {{anchors.imageX + 149, anchors.imageY + 54}, 203};
		result.grid = {
			{anchors.imageX + 37, anchors.imageY + 68, 66, 64},
			{90, 72}};
		result.pageButton = {
			anchors.imageX + 200, anchors.imageY + 357, 75, 18};
		result.pageControlsInvalidation = {
			anchors.imageX + 100, anchors.imageY + 357, 450, 18};
		result.popup.origin = {
			anchors.imageX + (500 - 309) / 2,
			anchors.screenY + 120 + anchors.webDeltaY};
		result.popup.width = 309;
		result.popup.textWidth = 296;
		result.popup.sectionHeight = 9;
		result.popup.shadowGap = 4;
		result.popup.facePanel = {
			result.popup.origin.x + 6,
			result.popup.origin.y + 6,
			58,
			52};
		result.popup.name = {
			result.popup.facePanel.right() + 10,
			result.popup.facePanel.y + 20};
		result.popup.description = {{
			result.popup.origin.x + 8,
			result.popup.facePanel.bottom() + 5},
			result.popup.textWidth};
		result.popup.doneX =
			result.popup.origin.x + result.popup.width - 36 - 7;
		return result;
	}

	constexpr std::size_t ArchiveProfileIndex(
		std::size_t page, std::size_t slot,
		std::size_t profileCount) noexcept
	{
		if (page >= kArchivePageCount || slot >= kArchivePageCapacity)
			return profileCount;
		const std::size_t index = page * kArchivePageCapacity + slot;
		return index < profileCount ? index : profileCount;
	}

	template<typename Visibility>
	constexpr bool ArchivePageHasVisible(
		const Visibility* visible, std::size_t profileCount,
		std::size_t page) noexcept
	{
		if (!visible || page >= kArchivePageCount)
			return false;
		for (std::size_t slot = 0; slot < kArchivePageCapacity; ++slot)
		{
			const std::size_t index =
				ArchiveProfileIndex(page, slot, profileCount);
			if (index == profileCount)
				break;
			if (visible[index])
				return true;
		}
		return false;
	}

	template<typename Visibility>
	constexpr std::size_t NextArchivePage(
		std::size_t currentPage, const Visibility* pageEnabled,
		std::size_t pageCount = kArchivePageCount) noexcept
	{
		if (!pageEnabled || pageCount == 0 ||
			pageCount > kArchivePageCount)
			return 0;
		const std::size_t normalized =
			currentPage < pageCount ? currentPage : 0;
		for (std::size_t offset = 1; offset <= pageCount; ++offset)
		{
			const std::size_t page = (normalized + offset) % pageCount;
			if (pageEnabled[page])
				return page;
		}
		return normalized;
	}
}

#endif
