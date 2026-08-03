#ifndef BOBBY_RAY_COMMERCE_MODEL_H
#define BOBBY_RAY_COMMERCE_MODEL_H

#include <cstddef>
#include <cstdint>

namespace BobbyRayCommerceModel
{
inline constexpr std::size_t PurchaseCapacity = 100;

// The retired pre-PostalService format stored a signed slot count followed by
// fixed-size shipment records. No shipped game could create more than a small
// number of these, so keep corrupt saves from turning that count into an
// effectively unbounded allocation while retaining a generous compatibility
// ceiling.
inline constexpr std::size_t MaximumLegacyShipmentSlots = 4096;

constexpr std::size_t PurchaseLimit(std::size_t configured) noexcept
{
	return configured < PurchaseCapacity ? configured : PurchaseCapacity;
}

constexpr std::size_t BoundedLength(
	std::size_t persistedLength, std::size_t capacity) noexcept
{
	return persistedLength < capacity ? persistedLength : capacity;
}

constexpr bool IsIndexInBoundedList(
	std::size_t index, std::size_t persistedLength,
	std::size_t capacity) noexcept
{
	return index < BoundedLength(persistedLength, capacity);
}

constexpr bool LegacyOrderCountsAreConsistent(
	std::size_t slotCount, std::size_t activeCount) noexcept
{
	return activeCount <= slotCount &&
		(activeCount == 0 || slotCount != 0);
}

constexpr bool LegacyShipmentCountFits(
	std::int32_t slotCount, std::size_t remainingBytes,
	std::size_t recordSize) noexcept
{
	return slotCount >= 0 && recordSize != 0 &&
		static_cast<std::size_t>(slotCount) <= MaximumLegacyShipmentSlots &&
		static_cast<std::size_t>(slotCount) <= remainingBytes / recordSize;
}

constexpr std::size_t VisibleShipmentCount(
	std::size_t liveCount, std::size_t snapshotCount,
	std::size_t regionCapacity) noexcept
{
	const std::size_t available =
		liveCount < snapshotCount ? liveCount : snapshotCount;
	return available < regionCapacity ? available : regionCapacity;
}

constexpr std::uint8_t RemoveStock(
	std::uint8_t onHand, std::uint8_t purchased) noexcept
{
	return purchased < onHand
		? static_cast<std::uint8_t>(onHand - purchased)
		: 0;
}

constexpr std::uint8_t AddStock(
	std::uint8_t onHand, std::uint8_t arriving) noexcept
{
	const auto total = static_cast<unsigned>(onHand) + arriving;
	return total < 256u ? static_cast<std::uint8_t>(total) : 255u;
}

constexpr std::uint8_t PurchaseRecordCount(std::size_t persistedCount) noexcept
{
	return static_cast<std::uint8_t>(PurchaseLimit(persistedCount));
}

template <typename Order>
std::size_t CountActiveOrders(
	const Order* orders, std::size_t slotCount) noexcept
{
	if (!orders) return 0;

	std::size_t activeCount = 0;
	for (std::size_t index = 0; index < slotCount; ++index)
	{
		if (orders[index].fActive) ++activeCount;
	}
	return activeCount;
}
}

#endif
