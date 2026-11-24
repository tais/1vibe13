#include "Ja2/FullEngineCoopClientPresentationPassiveUiRosterTextPlan.h"

#include <array>
#include <cstdio>
#include <limits>

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, m); } } while (0)

FullEngineCoopClientPassiveRosterEntry Entry(
	std::uint16_t slot, std::int16_t actionPoints,
	const char* name, bool selected = false)
{
	FullEngineCoopClientPassiveRosterEntry entry;
	entry.actor = {slot, 1};
	entry.actionPoints = actionPoints;
	entry.selected = selected;
	for (std::size_t index = 0;
		name[index] != '\0' && index + 1 < entry.displayNameUtf16.size();
		++index)
		entry.displayNameUtf16[index] =
			static_cast<std::uint8_t>(name[index]);
	return entry;
}

template <std::size_t Capacity>
bool EqualsAscii(const std::array<std::uint16_t, Capacity>& text,
	std::size_t length, const char* expected)
{
	std::size_t index = 0;
	for (; expected[index] != '\0'; ++index)
	{
		if (index >= length || text[index] !=
			static_cast<std::uint8_t>(expected[index]))
			return false;
	}
	return index == length && index < text.size() && text[index] == 0;
}

void TestWindowAlignmentAndThreeDigitActionPoints()
{
	FullEngineCoopClientPassiveUiModel model;
	for (std::uint16_t index = 0; index < 8; ++index)
		model.roster.push_back(Entry(static_cast<std::uint16_t>(index + 1),
			index == 6 ? 100 : (index == 7 ? 250 : 0), "Merc",
			index == 7));
	model.rosterFirstVisible = 6;
	const FullEngineCoopClientPassiveUiRect panel{5, 3, 645, 123};
	FullEngineCoopClientPassiveRosterTextPlan plan;
	CHECK(BuildFullEngineCoopClientPassiveRosterTextPlan(
		model, panel, plan), "a second authority roster window builds");
	CHECK(plan.visibleSlotCount == 2 &&
		plan.slots[0].visibleSlot == 0 &&
		plan.slots[0].rosterIndex == 6 &&
		plan.slots[1].visibleSlot == 1 &&
		plan.slots[1].rosterIndex == 7 && plan.slots[1].selected,
		"the plan retains exact visible-slot and source-roster identities");
	CHECK(EqualsAscii(plan.slots[0].name,
			plan.slots[0].nameLength, "Merc") &&
		EqualsAscii(plan.slots[0].actionPoints,
			plan.slots[0].actionPointLength, "100") &&
		EqualsAscii(plan.slots[1].actionPoints,
			plan.slots[1].actionPointLength, "250"),
		"names and zero-to-250 action points are prepared without libc formatting");
	CHECK(plan.slots[0].nameAlignment.left == 14 &&
		plan.slots[0].nameAlignment.top == 58 &&
		plan.slots[0].nameAlignment.right == 74 &&
		plan.slots[0].nameAlignment.bottom == 67 &&
		plan.slots[0].nameClip.left == 14 &&
		plan.slots[0].nameClip.right == 74,
		"name alignment and clipping retain the stock +2 horizontal offset");
	CHECK(plan.slots[0].actionPointAlignment.left == 73 &&
		plan.slots[0].actionPointAlignment.right == 88 &&
		plan.slots[0].actionPointAlignment.top == 55 &&
		plan.slots[0].actionPointAlignment.bottom == 65 &&
		plan.slots[0].actionPointClip.left == 71 &&
		plan.slots[0].actionPointClip.right == 90 &&
		plan.slots[0].actionPointClip.right -
			plan.slots[0].actionPointClip.left == 19,
		"AP centers in the stock 15-pixel box but has a 19-pixel three-digit clip");
	CHECK(plan.slots[1].nameAlignment.left == 97 &&
		plan.slots[1].actionPointAlignment.left == 156 &&
		plan.slots[1].actionPointClip.left == 154 &&
		plan.slots[1].actionPointClip.right == 173,
		"the next text slot preserves the stock 83-pixel stride");
}

void TestCanonicalUnicodeAndActionPointExtremes()
{
	FullEngineCoopClientPassiveUiModel model;
	auto unicode = Entry(1, 0, "A");
	unicode.displayNameUtf16.fill(0);
	unicode.displayNameUtf16[0] = static_cast<std::uint16_t>('A');
	unicode.displayNameUtf16[1] = 0xd83d;
	unicode.displayNameUtf16[2] = 0xde00;
	unicode.displayNameUtf16[3] = static_cast<std::uint16_t>('B');
	model.roster.push_back(unicode);
	model.roster.push_back(Entry(2,
		(std::numeric_limits<std::int16_t>::max)(), "Max"));
	model.roster.push_back(Entry(3,
		(std::numeric_limits<std::int16_t>::min)(), "Min"));

	FullEngineCoopClientPassiveRosterTextPlan plan;
	CHECK(BuildFullEngineCoopClientPassiveRosterTextPlan(
		model, {0, 0, 640, 120}, plan) &&
		EqualsAscii(plan.slots[0].name,
			plan.slots[0].nameLength, "A?B") &&
		EqualsAscii(plan.slots[0].actionPoints,
			plan.slots[0].actionPointLength, "0") &&
		EqualsAscii(plan.slots[1].actionPoints,
			plan.slots[1].actionPointLength, "32767") &&
		EqualsAscii(plan.slots[2].actionPoints,
			plan.slots[2].actionPointLength, "-32768"),
		"canonical surrogate pairs fall back once and every int16 AP formats safely");
}

void TestExactBitmapTextOrigin()
{
	FullEngineCoopClientPassiveUiPoint origin{91, 92};
	CHECK(PositionFullEngineCoopClientPassiveRosterTextRun(
			{14, 58, 74, 67}, 17, 8, false, origin) &&
		origin.x == 36 && origin.y == 58,
		"name origin retains legacy plus-one centering and bitmap-font height");
	CHECK(PositionFullEngineCoopClientPassiveRosterTextRun(
			{73, 55, 88, 65}, 18, 9, true, origin) &&
		origin.x == 72 && origin.y == 55,
		"three-digit AP starts one pixel left and remains stock top-aligned");
	const auto retained = origin;
	CHECK(!PositionFullEngineCoopClientPassiveRosterTextRun(
			{73, 55, 88, 65}, -1, 9, true, origin) &&
		origin.x == retained.x && origin.y == retained.y,
		"invalid bitmap metrics cannot publish a partial text origin");
}

void TestMalformedNamesAndInvalidWindowsAreTransactional()
{
	FullEngineCoopClientPassiveUiModel model;
	auto malformed = Entry(1, 14, "Bad");
	malformed.displayNameUtf16.fill(0);
	malformed.displayNameUtf16[0] = 0xd83d;
	malformed.displayNameUtf16[1] = static_cast<std::uint16_t>('X');
	model.roster.push_back(malformed);

	FullEngineCoopClientPassiveRosterTextPlan output;
	output.visibleSlotCount = 4;
	output.slots[0].rosterIndex = 99;
	CHECK(!BuildFullEngineCoopClientPassiveRosterTextPlan(
		model, {0, 0, 640, 120}, output) &&
		output.visibleSlotCount == 4 && output.slots[0].rosterIndex == 99,
		"a malformed surrogate pair fails without publishing a partial plan");

	malformed.displayNameUtf16.fill(0);
	malformed.displayNameUtf16[0] = static_cast<std::uint16_t>('A');
	malformed.displayNameUtf16[2] = static_cast<std::uint16_t>('B');
	model.roster[0] = malformed;
	CHECK(!BuildFullEngineCoopClientPassiveRosterTextPlan(
		model, {0, 0, 640, 120}, output) &&
		output.visibleSlotCount == 4,
		"nonzero data after the name terminator is rejected transactionally");

	model.roster[0] = Entry(1, 14, "Good");
	model.rosterFirstVisible = 1;
	CHECK(!BuildFullEngineCoopClientPassiveRosterTextPlan(
		model, {0, 0, 640, 120}, output) &&
		output.visibleSlotCount == 4,
		"a non-page-aligned roster window cannot publish ambiguous slot text");
}
}

int main()
{
	TestWindowAlignmentAndThreeDigitActionPoints();
	TestCanonicalUnicodeAndActionPointExtremes();
	TestExactBitmapTextOrigin();
	TestMalformedNamesAndInvalidWindowsAreTransactional();
	if (failures == 0)
		std::printf("full-engine co-op passive roster text-plan tests passed\n");
	return failures == 0 ? 0 : 1;
}
