#ifndef BOBBY_RAY_FULFILMENT_MODEL_H
#define BOBBY_RAY_FULFILMENT_MODEL_H

#include <cstddef>
#include <limits>

namespace BobbyRayFulfilmentModel
{
inline constexpr std::size_t NoSelection =
	std::numeric_limits<std::size_t>::max();

constexpr std::size_t VisibleCount(
	std::size_t itemCount, std::size_t capacity) noexcept
{
	return itemCount < capacity ? itemCount : capacity;
}

constexpr std::size_t NormalizeWindowStart(
	std::size_t currentStart, std::size_t itemCount,
	std::size_t visibleCount) noexcept
{
	if (itemCount <= visibleCount) return 0;
	const std::size_t lastStart = itemCount - visibleCount;
	return currentStart < lastStart ? currentStart : lastStart;
}

constexpr bool IsIndexInRange(
	std::size_t index, std::size_t itemCount) noexcept
{
	return index < itemCount;
}

constexpr std::size_t NormalizeSelection(
	std::size_t selection, std::size_t itemCount) noexcept
{
	return IsIndexInRange(selection, itemCount) ? selection : NoSelection;
}

constexpr std::size_t IndexForVisibleSlot(
	std::size_t slot, std::size_t windowStart,
	std::size_t visibleCount, std::size_t itemCount) noexcept
{
	if (slot >= visibleCount || windowStart >= itemCount ||
		slot >= itemCount - windowStart)
	{
		return NoSelection;
	}
	return windowStart + slot;
}

constexpr std::size_t NextSelection(
	std::size_t selection, std::size_t itemCount) noexcept
{
	if (itemCount == 0) return NoSelection;
	if (!IsIndexInRange(selection, itemCount)) return 0;
	return selection + 1 < itemCount ? selection + 1 : selection;
}

constexpr std::size_t PreviousSelection(
	std::size_t selection, std::size_t itemCount) noexcept
{
	if (itemCount == 0) return NoSelection;
	if (!IsIndexInRange(selection, itemCount)) return 0;
	return selection > 0 ? selection - 1 : 0;
}

template <typename Predicate>
std::size_t IndexForMatchingSlot(
	std::size_t itemCount, std::size_t slot,
	Predicate isMatch) noexcept
{
	std::size_t matchingSlot = 0;
	for (std::size_t index = 0; index < itemCount; ++index)
	{
		if (!isMatch(index)) continue;
		if (matchingSlot == slot) return index;
		++matchingSlot;
	}
	return NoSelection;
}
}

#endif
