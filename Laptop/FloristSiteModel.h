#ifndef LAPTOP_FLORIST_SITE_MODEL_H
#define LAPTOP_FLORIST_SITE_MODEL_H

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

#endif
