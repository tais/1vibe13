#ifndef LAPTOP_FLORIST_SITE_MODEL_H
#define LAPTOP_FLORIST_SITE_MODEL_H

#include "LaptopLayout.h"

#include <cstddef>

constexpr std::size_t kFloristGalleryFlowerCount = 10;
constexpr std::size_t kFloristGalleryPageSize = 3;

constexpr std::size_t FloristGalleryPageCount(
	std::size_t itemCount, std::size_t pageSize)
{
	return pageSize == 0 ? 0 :
		itemCount / pageSize + (itemCount % pageSize != 0 ? 1 : 0);
}

constexpr std::size_t kFloristGalleryPageCount = FloristGalleryPageCount(
	kFloristGalleryFlowerCount, kFloristGalleryPageSize);

// Dependency-free bounds for the legacy Florist adapters. Gallery and
// destination selections may survive page transitions while their backing
// content changes, so every consumer normalizes them before indexing.
constexpr std::size_t ClampFloristIndex(
	std::size_t index, std::size_t itemCount)
{
	return itemCount == 0 ? 0 :
		(index < itemCount ? index : itemCount - 1);
}

constexpr std::size_t FloristGalleryPageStart(
	std::size_t index, std::size_t itemCount, std::size_t pageSize)
{
	if (itemCount == 0 || pageSize == 0) return 0;
	const std::size_t clamped = ClampFloristIndex(index, itemCount);
	return clamped - clamped % pageSize;
}

constexpr std::size_t NextFloristGalleryPageStart(
	std::size_t index, std::size_t itemCount, std::size_t pageSize)
{
	const std::size_t current = FloristGalleryPageStart(
		index, itemCount, pageSize);
	return itemCount != 0 && pageSize != 0 &&
		itemCount - 1 - current >= pageSize
		? current + pageSize
		: current;
}

constexpr std::size_t PreviousFloristGalleryPageStart(
	std::size_t index, std::size_t itemCount, std::size_t pageSize)
{
	const std::size_t current = FloristGalleryPageStart(
		index, itemCount, pageSize);
	return pageSize != 0 && current >= pageSize
		? current - pageSize
		: 0;
}

constexpr std::size_t FloristGalleryPageNumber(
	std::size_t index, std::size_t itemCount, std::size_t pageSize)
{
	return pageSize == 0 ? 0 :
		FloristGalleryPageStart(index, itemCount, pageSize) / pageSize;
}

constexpr std::size_t CenteredFloristTextOffset(
	std::size_t containerHeight, std::size_t textHeight)
{
	return textHeight < containerHeight
		? (containerHeight - textHeight) / 2
		: 0;
}

struct FloristLayoutAnchors
{
	int webX = 0;
	int webY = 0;
};

struct FloristCardGrid
{
	LaptopLayoutModel::Rect firstCard;
	int columns = 0;
	int rows = 0;
	int columnStep = 0;
	int rowStep = 0;

	constexpr std::size_t capacity() const noexcept
	{
		return static_cast<std::size_t>(columns * rows);
	}

	constexpr LaptopLayoutModel::Rect card(std::size_t index) const noexcept
	{
		return {
			firstCard.x + static_cast<int>(index % columns) * columnStep,
			firstCard.y + static_cast<int>(index / columns) * rowStep,
			firstCard.width,
			firstCard.height};
	}
};

struct FloristCardsLayout
{
	LaptopLayoutModel::Rect pageBounds;
	FloristCardGrid cards;
	LaptopLayoutModel::TextArea title;
	LaptopLayoutModel::Point backButton;
	int cardTextInsetX = 0;
	int cardTextInsetY = 0;
	int cardTextWidth = 0;
	int cardTextHeight = 0;
};

constexpr std::size_t kFloristCardCount = 9;

constexpr FloristCardsLayout MakeFloristCardsLayout(
	FloristLayoutAnchors anchors) noexcept
{
	return {
		{anchors.webX, anchors.webY, 502, 400},
		{{anchors.webX + 7, anchors.webY + 72, 135, 100},
		 3, 3, 174, 109},
		{{anchors.webX, anchors.webY + 53}, 502},
		{anchors.webX + 8, anchors.webY + 12},
		7,
		10,
		121,
		90};
}

struct FloristGalleryRow
{
	LaptopLayoutModel::Point button;
	LaptopLayoutModel::Point title;
	LaptopLayoutModel::Point price;
	LaptopLayoutModel::TextArea description;
};

struct FloristGalleryLayout
{
	LaptopLayoutModel::Rect pageBounds;
	LaptopLayoutModel::Point backButton;
	LaptopLayoutModel::Point nextButton;
	LaptopLayoutModel::TextArea title;
	LaptopLayoutModel::Point firstFlowerButton;
	int rowStep = 0;

	constexpr FloristGalleryRow row(std::size_t index) const noexcept
	{
		const int y = firstFlowerButton.y +
			static_cast<int>(index) * rowStep;
		return {
			{firstFlowerButton.x, y},
			{firstFlowerButton.x + 88, y + 9},
			{firstFlowerButton.x + 88, y + 26},
			{{firstFlowerButton.x + 88, y + 41}, 390}};
	}
};

constexpr FloristGalleryLayout MakeFloristGalleryLayout(
	FloristLayoutAnchors anchors) noexcept
{
	return {
		{anchors.webX, anchors.webY, 502, 400},
		{anchors.webX + 8, anchors.webY + 12},
		{anchors.webX + 420, anchors.webY + 12},
		{{anchors.webX, anchors.webY + 48}, 502},
		{anchors.webX + 7, anchors.webY + 74},
		112};
}

#endif
