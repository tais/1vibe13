#include "CampaignHistoryModel.h"

#include <iostream>
#include <limits>
#include <string>

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
	using namespace CampaignHistoryModel;

	Check(!IsValidIndex(-1, 4) && IsValidIndex(0, 4) &&
		IsValidIndex(3, 4) && !IsValidIndex(4, 4),
		"Index validation rejects negative and exact-end values");
	Check(NormalizePage(9, 0) == 0 && NormalizePage(4, 4) == 0 &&
		NormalizePage(2, 4) == 2 && NextPage(0, 0) == 0 &&
		PreviousPage(0, 0) == 0,
		"Empty, stale, and exact-end incident pages normalize safely");
	Check(NextPage(2, 3) == 0 && PreviousPage(0, 3) == 2 &&
		NextPage(99, 3) == 1 && PreviousPage(99, 3) == 2,
		"Incident navigation wraps after normalizing stale state");

	Check(RetainedIncidentCount(5, -1) == 5 &&
		RetainedIncidentCount(5, 0) == 0 &&
		RetainedIncidentCount(5, 20) == 5 &&
		RetainedIncidentCount(5, 2) == 2 &&
		!ShouldRetainIncident(0, 5, 0) &&
		ShouldRetainIncident(0, 5, 20) &&
		!ShouldRetainIncident(2, 5, 2) &&
		ShouldRetainIncident(3, 5, 2) &&
		!ShouldRetainIncident(5, 5, -1),
		"Report limits retain all short histories and only the requested tail");

	Check(SaturatingAddUnsigned<std::uint16_t>(65534, 1) == 65535 &&
		SaturatingAddUnsigned<std::uint16_t>(65535, 1) == 65535 &&
		SaturatingAddUnsigned<std::uint32_t>(
			std::numeric_limits<std::uint32_t>::max() - 2, 9) ==
			std::numeric_limits<std::uint32_t>::max(),
		"Incident and aggregate counters saturate instead of wrapping");
	Check(SaturatingAddSigned<std::int32_t>(
			std::numeric_limits<std::int32_t>::max(), 1) ==
			std::numeric_limits<std::int32_t>::max() &&
		SaturatingAddSigned<std::int32_t>(
			std::numeric_limits<std::int32_t>::min(), -1) ==
			std::numeric_limits<std::int32_t>::min(),
		"Campaign money totals saturate at both signed limits");
	Check(SaturatingAddFinite(std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max()) ==
			std::numeric_limits<float>::max() &&
		SaturatingAddFinite(7.0f,
			std::numeric_limits<float>::quiet_NaN()) == 7.0f,
		"Consumption totals stay finite when inputs overflow or are NaN");

	Check(IsExactTransfer(true, 64, 64) &&
		!IsExactTransfer(true, 63, 64) && !IsExactTransfer(false, 64, 64),
		"Persistence accepts only successful exact-size transfers");
	Check(!FrameIndex(9, 0) && FrameIndex(9, 4) == 1,
		"Picture selection rejects empty libraries before modulo");

	constexpr std::array<std::uint64_t, 4> masks = {1, 2, 4, 8};
	const std::array<const wchar_t*, 4> names =
		{L"north", L"west", L"south", L"east"};
	Check(JoinDirections(0, masks, names, L"and", L"unknown") == L"unknown" &&
		JoinDirections(1, masks, names, L"and", L"unknown") == L"north" &&
		JoinDirections(3, masks, names, L"and", L"unknown") ==
			L"north and west" &&
		JoinDirections(7, masks, names, L"and", L"unknown") ==
			L"north, west and south" &&
		JoinDirections(15, masks, names, L"and", L"unknown") ==
			L"north, west, south and east",
		"Direction text is deterministic for zero through four directions");

	wchar_t destination[6]{};
	wchar_t unterminated[3] = {L'a', L'b', L'c'};
	Check(CopyText(destination, L"abcd") &&
		std::wstring(destination) == L"abcd" &&
		!CopyText(destination, L"123456") &&
		std::wstring(destination) == L"12345" &&
		!CopyText(destination, unterminated) &&
		std::wstring(destination) == L"abc",
		"Campaign History text copies terminate and bound fixed sources");
	Check(!CopyTextFromPointer(destination,
		static_cast<const wchar_t*>(nullptr)) && destination[0] == L'\0',
		"Campaign History text copies reject null dynamic sources safely");

	return failures == 0 ? 0 : 1;
}
