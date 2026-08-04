#include "LaptopEmailListModel.h"

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
	using namespace LaptopEmailListModel;

	Check(PageCount(0, 18) == 0 && PageCount(1, 18) == 1 &&
		PageCount(18, 18) == 1 && PageCount(19, 18) == 2 &&
		PageCount(100, 0) == 0,
		"Inbox page counts handle empty and exact-page message sets");
	Check(NormalizeInboxPage(9, 0, 18) == 0 &&
		NormalizeInboxPage(9, 19, 18) == 1 &&
		NormalizeInboxPage(0, 19, 18) == 0,
		"Saved inbox pages normalize empty and stale values");

	Check(IsIndexInRange(254, 255) && !IsIndexInRange(255, 255) &&
		!IsIndexInRange(-1, 255),
		"Profile and message indices reject negative and exact-end values");
	Check(CanStoreUnsigned16(0) && CanStoreUnsigned16(65535) &&
		!CanStoreUnsigned16(-1) && !CanStoreUnsigned16(65536),
		"Persisted email offsets reject narrowing and negative values");
	Check(CanAppendMessage(0, -1, 4000) &&
		CanAppendMessage(3999, 3998, 4000) &&
		!CanAppendMessage(0, -1, 0) &&
		!CanAppendMessage(4000, 3999, 4000) &&
		!CanAppendMessage(1, 3999, 4000) &&
		!CanAppendMessage(1, std::numeric_limits<std::int32_t>::max(), 4000) &&
		NextMessageId(-1) == 0 && NextMessageId(41) == 42,
		"Message ID generation rejects capacity and signed overflow");
	Check(IsMoreRecent(11, 1, true, 10, 9) &&
		IsMoreRecent(10, 10, true, 10, 9) &&
		!IsMoreRecent(9, 99, true, 10, 9) &&
		IsMoreRecent(0, 0, false, 100, 100),
		"Most-recent unread selection uses date then message ID");

	Check(WildfireSubjectLine(170) == 0 &&
		WildfireSubjectLine(177) == 14 &&
		WildfireSubjectLine(178) == 16 &&
		WildfireSubjectLine(169) == NoIndex &&
		WildfireSubjectLine(-1) == NoIndex,
		"Wildfire subjects reject underflow and preserve paired indices");

	Check(BodyPageCount(0, 100) == 0 && BodyPageCount(2, 100) == 1 &&
		BodyPageCount(500, 100) == 99 &&
		NormalizeBodyPage(500, 500, 100) == 98 &&
		HasNextBodyPage(97, 500, 100) &&
		!HasNextBodyPage(98, 500, 100),
		"Message body pages reserve a bounded sentinel entry");

	wchar_t shortText[5]{};
	Check(CopyText(shortText, L"mail") && shortText[4] == L'\0' &&
		!CopyText(shortText, L"longer") && shortText[4] == L'\0' &&
		!CopyText(shortText, static_cast<const wchar_t*>(nullptr)) &&
		shortText[0] == L'\0',
		"Fixed email text copies terminate exact, long, and null inputs");
	wchar_t appended[6] = L"ab";
	Check(AppendText(appended, L"cde") && appended[5] == L'\0' &&
		!AppendText(appended, L"f") && appended[5] == L'\0',
		"Fixed email text appends preserve the terminator at capacity");

	std::size_t length = 0;
	Check(BoundedLength(L"subject", 7, length) && length == 7 &&
		!BoundedLength(L"subject", 6, length) &&
		IsSerializedSubjectSizeValid(8 * sizeof(wchar_t), sizeof(wchar_t), 7) &&
		IsSerializedSubjectSizeValid(sizeof(wchar_t), sizeof(wchar_t), 7) &&
		!IsSerializedSubjectSizeValid(0, sizeof(wchar_t), 7) &&
		!IsSerializedSubjectSizeValid(3, sizeof(wchar_t), 7) &&
		!IsSerializedSubjectSizeValid(9 * sizeof(wchar_t), sizeof(wchar_t), 7),
		"Subject bounds reject unterminated and malformed serialized text");

	return failures == 0 ? 0 : 1;
}
