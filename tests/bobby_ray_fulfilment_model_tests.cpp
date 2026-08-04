#include "BobbyRayFulfilmentModel.h"

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
	using namespace BobbyRayFulfilmentModel;

	Check(VisibleCount(0, 10) == 0 && VisibleCount(7, 10) == 7 &&
		VisibleCount(17, 10) == 10,
		"Fulfilment lists cap visible rows without inventing empty rows");
	Check(NormalizeWindowStart(9, 0, 0) == 0 &&
		NormalizeWindowStart(9, 7, 7) == 0 &&
		NormalizeWindowStart(9, 17, 10) == 7 &&
		NormalizeWindowStart(4, 17, 10) == 4,
		"Fulfilment list windows normalize empty, short, and stale starts");

	Check(NormalizeSelection(0, 0) == NoSelection &&
		NormalizeSelection(6, 7) == 6 &&
		NormalizeSelection(7, 7) == NoSelection,
		"Fulfilment selections reject empty and exact-end indices");
	Check(IndexForVisibleSlot(0, 0, 0, 0) == NoSelection &&
		IndexForVisibleSlot(6, 4, 7, 11) == 10 &&
		IndexForVisibleSlot(7, 4, 7, 11) == NoSelection &&
		IndexForVisibleSlot(6, 5, 7, 11) == NoSelection,
		"Fulfilment callbacks reject hidden and stale visible slots");

	Check(NextSelection(NoSelection, 0) == NoSelection &&
		NextSelection(NoSelection, 3) == 0 &&
		NextSelection(2, 3) == 2 &&
		PreviousSelection(NoSelection, 3) == 0 &&
		PreviousSelection(0, 3) == 0 &&
		PreviousSelection(2, 3) == 1,
		"Fulfilment keyboard navigation stays within empty and list bounds");

	const bool active[] = {false, true, false, true, true};
	Check(IndexForMatchingSlot(5, 0,
		[&](std::size_t index) { return active[index]; }) == 1 &&
		IndexForMatchingSlot(5, 2,
		[&](std::size_t index) { return active[index]; }) == 4 &&
		IndexForMatchingSlot(5, 3,
		[&](std::size_t index) { return active[index]; }) == NoSelection,
		"Shipment rows map visible slots to sparse live records safely");

	return failures == 0 ? 0 : 1;
}
