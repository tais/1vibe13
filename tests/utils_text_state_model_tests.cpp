#include "TextInfrastructureModel.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

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
	using namespace TextInfrastructureModel;

	Require(!IsValidIndex(5, -1) && IsValidIndex(5, 4) &&
		!IsValidIndex(5, 5) &&
		!IsValidIndex(5, std::numeric_limits<std::uint64_t>::max()),
		"text indices reject negative, exact-end, and oversized values");

	wchar_t copied[5]{};
	Require(CopyBounded(copied, L"four") &&
		std::wstring_view(copied) == L"four",
		"bounded text copy accepts an exact-capacity payload");
	Require(!CopyBounded(copied, L"overflow") &&
		std::wstring_view(copied) == L"over",
		"bounded text copy truncates and terminates oversized payloads");
	Require(!CopyBounded(copied, static_cast<const wchar_t*>(nullptr)) &&
		copied[0] == L'\0',
		"bounded text copy clears its destination for null input");

	wchar_t appended[8] = L"abc";
	Require(AppendBounded(appended, L"defg") &&
		std::wstring_view(appended) == L"abcdefg",
		"bounded append accepts an exact-capacity payload");
	Require(!AppendBounded(appended, L"x") &&
		std::wstring_view(appended) == L"abcdefg",
		"bounded append preserves termination when full");

	Require(CanReadSerializedText(512u * sizeof(wchar_t),
			sizeof(wchar_t), 512) &&
		!CanReadSerializedText(512u * sizeof(wchar_t) + 1,
			sizeof(wchar_t), 512) &&
		!CanReadSerializedText(0, sizeof(wchar_t), 512),
		"serialized message sizes accept full buffers and reject oversized payloads");

	LazyLoadState state;
	Require(!NeedsLoad(state),
		"unassociated localization sections do not load");
	Associate(state);
	Require(NeedsLoad(state),
		"new localization associations load once");
	RecordLoadResult(state, false);
	Require(NeedsLoad(state),
		"failed localization loads remain retryable");
	RecordLoadResult(state, true);
	Require(!NeedsLoad(state),
		"successful localization loads are cached");
	Reset(state);
	Require(NeedsLoad(state),
		"localization cache reset schedules an associated section again");

	std::cout << "Utils text state model tests passed\n";
	return 0;
}
