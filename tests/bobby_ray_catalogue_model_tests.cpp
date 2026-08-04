#include "BobbyRayCatalogueModel.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Check(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}
}

int main()
{
	using namespace BobbyRayCatalogueModel;

	Check(PageCount(0) == 0 && PageCount(1) == 1 &&
		PageCount(4) == 1 && PageCount(5) == 2 &&
		PageCount(1024) == 256 && PageCount(16001) == 4001 &&
		PageCount(8, 0) == 0,
		"Catalogue page counts cover empty, full, and partial pages");
	Check(NormalizePage(9, 0) == 0 && NormalizePage(9, 3) == 2 &&
		NextPage(0, 0) == 0 && NextPage(0, 3, 10) == 2 &&
		PreviousPage(2, 3, 10) == 0,
		"Catalogue navigation clamps empty and stale page selections");
	Check(VisibleItemCount(0, 0) == 0 &&
		VisibleItemCount(8, 1) == 4 &&
		VisibleItemCount(9, 2) == 1 &&
		VisibleItemCount(8, 0, 0) == 0 &&
		DisplayPageNumber(4, 0) == 0 &&
		DisplayPageNumber(4, 2) == 2,
		"Catalogue presentation normalizes empty and final partial pages");
	Check(IsVisibleItemSlot(0, 1) &&
		!IsVisibleItemSlot(1, 1) &&
		!IsVisibleItemSlot(4, 4),
		"Catalogue hotkeys and callbacks reject hidden item slots");
	Check(!IsCatalogueIndexInBounds(0, 0) &&
		IsCatalogueIndexInBounds(3, 4) &&
		!IsCatalogueIndexInBounds(4, 4),
		"Catalogue data indices reject the exact-end value");

	std::cout << "Bobby Ray catalogue model tests passed\n";
	return 0;
}
