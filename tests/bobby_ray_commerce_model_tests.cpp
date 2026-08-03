#include "BobbyRayCommerceModel.h"

#include <cstdint>
#include <iostream>

namespace
{
int failures = 0;

void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}
}

int main()
{
	using namespace BobbyRayCommerceModel;

	Check(PurchaseLimit(0) == 0 && PurchaseLimit(99) == 99 &&
		PurchaseLimit(100) == PurchaseCapacity &&
		PurchaseLimit(255) == PurchaseCapacity,
		"configured purchase limits never exceed physical storage");

	Check(BoundedLength(7, 10) == 7 && BoundedLength(10, 10) == 10 &&
		BoundedLength(11, 10) == 10,
		"persisted list lengths clamp to their physical capacity");
	Check(IsIndexInBoundedList(9, 10, 10) &&
		!IsIndexInBoundedList(10, 10, 10) &&
		!IsIndexInBoundedList(9, 4, 10),
		"bounded list indices reject the exact end and persisted tail");

	Check(LegacyOrderCountsAreConsistent(4, 4) &&
		LegacyOrderCountsAreConsistent(0, 0) &&
		!LegacyOrderCountsAreConsistent(0, 1) &&
		!LegacyOrderCountsAreConsistent(3, 4),
		"legacy active-order counts cannot exceed allocated slots");

	constexpr std::size_t recordSize = 800;
	Check(LegacyShipmentCountFits(2, 2 * recordSize, recordSize) &&
		LegacyShipmentCountFits(0, 0, recordSize) &&
		!LegacyShipmentCountFits(-1, 2 * recordSize, recordSize) &&
		!LegacyShipmentCountFits(2, 2 * recordSize - 1, recordSize) &&
		!LegacyShipmentCountFits(1, recordSize, 0) &&
		!LegacyShipmentCountFits(
			static_cast<std::int32_t>(MaximumLegacyShipmentSlots + 1),
			(MaximumLegacyShipmentSlots + 1) * recordSize, recordSize),
		"legacy shipment allocation is bounded by count, bytes, and a ceiling");

	Check(VisibleShipmentCount(20, 18, 13) == 13 &&
		VisibleShipmentCount(8, 18, 13) == 8 &&
		VisibleShipmentCount(18, 6, 13) == 6,
		"shipment regions stay within live, snapshot, and physical bounds");

	Check(RemoveStock(5, 3) == 2 && RemoveStock(5, 5) == 0 &&
		RemoveStock(5, 8) == 0,
		"stock removal saturates at zero");
	Check(AddStock(5, 3) == 8 && AddStock(250, 5) == 255 &&
		AddStock(250, 8) == 255,
		"stock arrival saturates at byte capacity");
	Check(PurchaseRecordCount(255) == PurchaseCapacity,
		"persisted purchase counts clamp to record capacity");

	struct Order { bool fActive; };
	const Order orders[]{{true}, {false}, {true}};
	Check(CountActiveOrders(orders, 3) == 2 &&
		CountActiveOrders<Order>(nullptr, 3) == 0,
		"active legacy orders are derived from owned records");

	return failures == 0 ? 0 : 1;
}
