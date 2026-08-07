#include "IndexedXmlModel.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

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
	using namespace IndexedXmlModel;

	const auto languageLast = ParseBoundedIndex(
		"999", 1000, IndexSyntax::CStyleUnsigned);
	Require(languageLast && languageLast.value == 999 &&
		!ParseBoundedIndex("1000", 1000, IndexSyntax::CStyleUnsigned),
		"language indices accept 999 and reject exact-end 1000");

	const auto senderLast = ParseBoundedIndex(
		"  +499\r\n", 500, IndexSyntax::Decimal);
	Require(senderLast && senderLast.value == 499 &&
		!ParseBoundedIndex("500", 500, IndexSyntax::Decimal),
		"sender indices accept 499 and reject exact-end 500");

	Require(!ParseBoundedIndex("-1", 1000, IndexSyntax::Decimal) &&
		!ParseBoundedIndex("18446744073709551616", 1000,
			IndexSyntax::Decimal) &&
		!ParseBoundedIndex("12garbage", 1000, IndexSyntax::Decimal),
		"indexed XML rejects negative, overflowing, and partial numeric text");

	const auto hexadecimal = ParseBoundedIndex(
		"0x3e7", 1000, IndexSyntax::CStyleUnsigned);
	const auto octal = ParseBoundedIndex(
		"01747", 1000, IndexSyntax::CStyleUnsigned);
	const auto leadingZeroDecimal = ParseBoundedIndex(
		"0499", 500, IndexSyntax::Decimal);
	Require(hexadecimal && hexadecimal.value == 999 &&
		octal && octal.value == 999 &&
		leadingZeroDecimal && leadingZeroDecimal.value == 499 &&
		!ParseBoundedIndex("09", 1000, IndexSyntax::CStyleUnsigned),
		"language keeps C-style indices while sender indices remain decimal");

	StagedIndexedText<std::wstring> staged(5);
	Require(staged.stage(4, L"four", 5) == StageResult::Accepted &&
		staged.stage(1, L"first", 8) == StageResult::Accepted &&
		staged.stage(1, L"last", 8) == StageResult::Accepted,
		"indexed XML staging accepts sparse, duplicate, and exact-capacity records");

	std::vector<std::wstring> live(5, L"unchanged");
	Require(live[1] == L"unchanged" && live[4] == L"unchanged",
		"staged indexed XML does not publish partial records");
	staged.publish([&](std::size_t index, const std::wstring& text)
	{
		live[index] = text;
	});
	Require(live[0] == L"unchanged" && live[1] == L"last" &&
		live[2] == L"unchanged" && live[4] == L"four",
		"indexed XML publication preserves sparse entries and duplicate-last-wins order");

	StagedIndexedText<std::wstring> rejected(5);
	Require(rejected.stage(5, L"end", 8) == StageResult::IndexOutOfRange &&
		rejected.stage(2, L"five!", 5) == StageResult::TextTooLong &&
		rejected.stage(2, L"", 0) == StageResult::TextTooLong &&
		rejected.empty(),
		"indexed XML staging rejects exact-end, oversized, and zero-capacity records");

	std::cout << "Utils indexed XML model tests passed\n";
	return 0;
}
