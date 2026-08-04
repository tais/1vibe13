#ifndef BOBBY_RAY_CATALOGUE_MODEL_H
#define BOBBY_RAY_CATALOGUE_MODEL_H

#include <algorithm>
#include <cstddef>

namespace BobbyRayCatalogueModel
{
inline constexpr std::size_t ItemsPerPage = 4;

constexpr std::size_t PageCount(
	std::size_t itemCount,
	std::size_t pageSize = ItemsPerPage) noexcept
{
	return pageSize == 0 ? 0 : itemCount / pageSize +
		(itemCount % pageSize != 0 ? 1 : 0);
}

constexpr std::size_t NormalizePage(
	std::size_t page, std::size_t pageCount) noexcept
{
	return pageCount == 0 ? 0 : std::min(page, pageCount - 1);
}

constexpr std::size_t NextPage(
	std::size_t page, std::size_t pageCount,
	std::size_t jump = 1) noexcept
{
	const std::size_t current = NormalizePage(page, pageCount);
	if (pageCount == 0) return 0;
	return current + std::min(jump, pageCount - 1 - current);
}

constexpr std::size_t PreviousPage(
	std::size_t page, std::size_t pageCount,
	std::size_t jump = 1) noexcept
{
	const std::size_t current = NormalizePage(page, pageCount);
	return current - std::min(jump, current);
}

constexpr std::size_t VisibleItemCount(
	std::size_t itemCount, std::size_t page,
	std::size_t pageSize = ItemsPerPage) noexcept
{
	if (itemCount == 0 || pageSize == 0) return 0;
	const std::size_t current = NormalizePage(
		page, PageCount(itemCount, pageSize));
	return std::min(pageSize, itemCount - current * pageSize);
}

constexpr std::size_t DisplayPageNumber(
	std::size_t page, std::size_t pageCount) noexcept
{
	return pageCount == 0 ? 0 : NormalizePage(page, pageCount) + 1;
}

constexpr bool IsVisibleItemSlot(
	std::size_t slot, std::size_t visibleItemCount,
	std::size_t pageSize = ItemsPerPage) noexcept
{
	return slot < pageSize && slot < visibleItemCount;
}

constexpr bool IsCatalogueIndexInBounds(
	std::size_t index, std::size_t count) noexcept
{
	return index < count;
}
}

#endif
