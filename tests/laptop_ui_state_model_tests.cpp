#include "LaptopUiStateModel.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace
{
	void Require(bool condition, const char* message)
	{
		if (condition) return;
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace LaptopUiStateModel;

	Require(!IsValidIndex(3, -1),
		"signed negative indices are rejected");
	Require(IsValidIndex(3, 2) && !IsValidIndex(3, 3),
		"exact-end indices are rejected");
	Require(!NormalizeIndex(0, 4).has_value(),
		"empty lists have no normalized selection");
	Require(NormalizeIndex(4, 9).value() == 0,
		"stale selections normalize to the first item");
	Require(AdjacentIndex(4, -1, true).value() == 0 &&
		!AdjacentIndex(4, -1, false).has_value(),
		"no-selection sentinels enter only at the first forward item");
	Require(AdjacentIndex(4, 2, true).value() == 3 &&
		AdjacentIndex(4, 2, false).value() == 1,
		"adjacent selection moves in either direction");
	Require(!AdjacentIndex(4, 3, true).has_value() &&
		!AdjacentIndex(4, 0, false).has_value() &&
		!AdjacentIndex(4, 4, true).has_value(),
		"adjacent selection rejects both ends and stale indices");

	Require(PageCount(0, 4) == 0 && PageCount(5, 4) == 2,
		"page counts cover empty and partial final pages");
	Require(PageCount(5, 0) == 0,
		"zero-sized pages cannot divide by zero");
	Require(NormalizePageStart(9, 4, 99) == 8,
		"stale page starts normalize to the final page");
	Require(VisibleCount(9, 4, 99) == 1,
		"final-page visibility uses the normalized start");
	Require(!VisibleIndex(9, 4, 8, 1).has_value(),
		"hidden and exact-end page slots are rejected");
	Require(VisibleIndex(9, 4, 8, 0).value() == 8,
		"visible slots map to live indices");
	Require(NextPageStart(9, 4, 0) == 4 &&
		NextPageStart(9, 4, 8) == 8,
		"next-page movement stops at the final page");
	Require(PreviousPageStart(9, 4, 8) == 4 &&
		PreviousPageStart(9, 4, 0) == 0,
		"previous-page movement cannot underflow");
	Require(NextPageStart(std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(), 0) == 0,
		"page movement cannot overflow");
	Require(NormalizeWindowStart(3, 8, 99) == 0 &&
		NormalizeWindowEnd(3, 8, 99) == 3,
		"short scrolling windows never expose rows past exact end");
	Require(NormalizeWindowStart(20, 5, 99) == 15 &&
		NormalizeWindowEnd(20, 5, 2) == 5,
		"scrolling windows clamp both stale and undersized end positions");
	Require(NormalizeWindowStart(20, 0, 9) == 0 &&
		NormalizeWindowEnd(20, 0, 9) == 0,
		"zero-capacity scrolling windows remain empty");

	wchar_t copied[5] = {};
	Require(!CopyText(copied, L"abcdef") && copied[4] == L'\0' &&
		std::wstring(copied) == L"abcd",
		"bounded copies truncate and terminate");
	Require(!CopyText(copied, static_cast<const wchar_t*>(nullptr)) &&
		copied[0] == L'\0',
		"null text sources clear the destination");

	wchar_t appended[8] = L"abc";
	Require(AppendText(appended, L"def") &&
		std::wstring(appended) == L"abcdef",
		"bounded appends preserve complete text");
	Require(!AppendText(appended, L"ghijkl") && appended[7] == L'\0' &&
		std::wstring(appended) == L"abcdefg",
		"bounded appends truncate and terminate");

	Require(IsExactTransfer(16, 16) && !IsExactTransfer(16, 15),
		"binary transfers must be exact");

	int bookmarks[5] = {2, -9, 2, 4, -1};
	Require(NormalizeSentinelList(bookmarks, -1,
		[](int value) { return value >= 0 && value < 5; }) == 2 &&
		bookmarks[0] == 2 && bookmarks[1] == 4 && bookmarks[2] == -1,
		"sentinel lists reject invalid entries, duplicates, and holes");
	Require(AppendUniqueSentinel(bookmarks, 3, -1) &&
		AppendUniqueSentinel(bookmarks, 3, -1) && bookmarks[2] == 3,
		"sentinel insertion is bounded and idempotent");
	Require(RemoveSentinelValue(bookmarks, 4, -1) &&
		bookmarks[0] == 2 && bookmarks[1] == 3 && bookmarks[2] == -1,
		"sentinel removal compacts the live list");
	int fullList[2] = {1, 2};
	Require(!AppendUniqueSentinel(fullList, 3, -1),
		"full sentinel lists reject insertion without an exact-end write");

	std::cout << "Laptop UI state model tests passed\n";
	return 0;
}
