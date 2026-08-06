#include "LaptopRecordPageModel.h"

#include <iostream>
#include <limits>

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
	using namespace LaptopRecordPageModel;
	constexpr FileLayout finance{4, 14};
	constexpr FileLayout history{0, 12};

	Check(IsWellFormedFile(4, finance) &&
		IsWellFormedFile(32, finance) &&
		!IsWellFormedFile(3, finance) &&
		!IsWellFormedFile(5, finance) &&
		!IsWellFormedFile(12, FileLayout{0, 0}),
		"Record layouts reject short, partial, and zero-width files");
	Check(IsAppendableFile(0, finance) &&
		IsAppendableFile(4, finance) &&
		IsAppendableFile(18, finance) &&
		!IsAppendableFile(3, finance) &&
		!IsAppendableFile(5, finance) &&
		!IsAppendableFile(0, FileLayout{0, 0}),
		"Fresh ledgers accept their first header and record append");
	Check(RecordCount(4, finance) == 0 &&
		RecordCount(18, finance) == 1 &&
		RecordCount(32, finance) == 2 &&
		RecordCount(13, history) == 0,
		"Record counts exclude headers and reject truncated tails");

	Check(PageCount(0, 17) == 0 && PageCount(1, 17) == 1 &&
		PageCount(17, 17) == 1 && PageCount(18, 17) == 2 &&
		PageCount(100, 0) == 0,
		"Ledger page counts handle empty and exact-page record sets");
	Check(NormalizeZeroBasedPage(9, 0) == 0 &&
		NormalizeZeroBasedPage(9, 3) == 2 &&
		NormalizeOneBasedPage(0, 3) == 1 &&
		NormalizeOneBasedPage(9, 3) == 3 &&
		NormalizeOneBasedPage(9, 0) == 1,
		"Saved ledger pages normalize empty, zero, and stale values");

	Check(PageByteOffset(0, 17, finance) == 4 &&
		PageByteOffset(2, 17, finance) == 480 &&
		PageByteOffset(0, 22, history) == 0 &&
		PageByteOffset(1, 22, history) == 264 &&
		PageByteOffset(0, 0, finance) == NoOffset &&
		PageByteOffset(std::numeric_limits<std::size_t>::max(), 17,
			finance) == NoOffset,
		"Ledger offsets include headers and reject overflowing pages");
	Check(RecordsOnPage(0, 0, 17) == 0 &&
		RecordsOnPage(17, 0, 17) == 17 &&
		RecordsOnPage(18, 1, 17) == 1 &&
		RecordsOnPage(18, 2, 17) == 0,
		"Ledger views never expose empty or exact-end pages");
	Check(BoundedIndex(4, 5) == 4 &&
		BoundedIndex(5, 5) == 0 &&
		BoundedIndex(std::numeric_limits<std::size_t>::max(), 5) == 0 &&
		BoundedIndex(0, 0) == 0,
		"Record-driven indices reject exact-end and oversized values");
	Check(CanApplyBalanceChange(100, -50) &&
		CanApplyBalanceChange(std::numeric_limits<std::int32_t>::max(), 0) &&
		!CanApplyBalanceChange(std::numeric_limits<std::int32_t>::max(), 1) &&
		!CanApplyBalanceChange(std::numeric_limits<std::int32_t>::min(), -1),
		"Finance balance publication rejects signed overflow");
	Check(SaturatingAdd(100, -50) == 50 &&
		SaturatingAdd(std::numeric_limits<std::int32_t>::max(), 1) ==
			std::numeric_limits<std::int32_t>::max() &&
		SaturatingAdd(std::numeric_limits<std::int32_t>::min(), -1) ==
			std::numeric_limits<std::int32_t>::min() &&
		SaturatingSubtract(std::numeric_limits<std::int32_t>::max(), -1) ==
			std::numeric_limits<std::int32_t>::max() &&
		SaturatingSubtract(std::numeric_limits<std::int32_t>::min(), 1) ==
			std::numeric_limits<std::int32_t>::min() &&
		SaturatingMultiply(std::numeric_limits<std::int32_t>::max(), 2) ==
			std::numeric_limits<std::int32_t>::max() &&
		SaturatingMultiply(std::numeric_limits<std::int32_t>::min(), 2) ==
			std::numeric_limits<std::int32_t>::min() &&
		SaturatingAddUnsigned(-100, 50) == -50 &&
		SaturatingAddUnsigned(0,
			std::numeric_limits<std::uint32_t>::max()) ==
			std::numeric_limits<std::int32_t>::max() &&
		Magnitude(std::numeric_limits<std::int32_t>::min()) == 2147483648ULL,
		"Ledger summaries saturate instead of overflowing signed totals");
	Check(IsRecordOnDayOffset(2820, 2880, 0, 60) &&
		IsRecordOnDayOffset(0, 2880, 1, 1500) &&
		!IsRecordOnDayOffset(0, 60, 1, 1500) &&
		!IsRecordOnDayOffset(std::numeric_limits<std::uint32_t>::max(),
			std::numeric_limits<std::uint32_t>::max(), 0, 1500),
		"Ledger day buckets handle legacy adjustment, early days, and overflow");

	return failures == 0 ? 0 : 1;
}
