#include "PersonnelRosterModel.h"

#include <iostream>
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
	using namespace PersonnelRosterModel;

	Check(IsValidProfileId(0, 255) && IsValidProfileId(254, 255) &&
		!IsValidProfileId(-1, 255) && !IsValidProfileId(255, 255),
		"Profile validation rejects negative and exact-end IDs");
	Check(IsValidIndex(16000, 16001) &&
		!IsValidIndex(-1, 16001) && !IsValidIndex(16001, 16001),
		"Generic index validation rejects corrupt item and table IDs");
	Check(ClampCurrency(42) == 42 &&
		ClampCurrency(static_cast<std::uint64_t>(
			std::numeric_limits<std::int32_t>::max()) + 1) ==
			std::numeric_limits<std::int32_t>::max(),
		"Currency totals clamp instead of overflowing signed UI values");
	wchar_t text[6]{};
	Check(CopyText(text, L"abc") && AppendText(text, L"de") &&
		std::wstring(text) == L"abcde" && !AppendText(text, L"f") &&
		std::wstring(text) == L"abcde" && !CopyText(text, L"123456") &&
		std::wstring(text) == L"12345",
		"Personnel text helpers always terminate and report truncation");

	RosterCursor cursor(20);
	cursor.reset(0);
	Check(!cursor.hasSelection() && !cursor.next(0) && !cursor.previous(0),
		"Empty rosters have no selectable or navigable entry");
	cursor.reset(41);
	Check(cursor.selected() == 0 && cursor.first() == 0 &&
		cursor.selectVisible(19, 41) && cursor.selected() == 19 &&
		!cursor.selectVisible(20, 41),
		"Visible selection rejects the exact page end");
	Check(cursor.next(41) && cursor.selected() == 20 && cursor.first() == 20 &&
		cursor.previous(41) && cursor.selected() == 19 && cursor.first() == 0,
		"Single-entry navigation crosses page boundaries safely");
	cursor.reset(41);
	Check(cursor.previous(41) && cursor.selected() == 40 && cursor.first() == 40 &&
		cursor.next(41) && cursor.selected() == 0 && cursor.first() == 0,
		"Previous and next navigation wrap around the complete roster");
	Check(cursor.nextPage(41) && cursor.selected() == 20 && cursor.first() == 20 &&
		cursor.nextPage(41) && cursor.selected() == 40 && cursor.first() == 40 &&
		cursor.nextPage(41) && cursor.selected() == 0 && cursor.first() == 0,
		"Page navigation reaches a one-entry final page and wraps");
	Check(cursor.previousPage(41) && cursor.selected() == 40 &&
		cursor.pageUp(41) && cursor.selected() == 20 && cursor.first() == 20 &&
		cursor.pageDown(41) && cursor.selected() == 40 && cursor.first() == 40 &&
		!cursor.pageDown(41),
		"Page scrolling preserves a bounded selection on partial pages");
	cursor.normalize(1);
	Check(cursor.selected() == 0 && cursor.first() == 0 &&
		!cursor.nextPage(1) && !cursor.previousPage(1),
		"A stale selection normalizes after roster shrinkage");

	short dead[5] = {4, -1, 999, 4, -1};
	short fired[5] = {7, 4, -1, -1, -1};
	short other[5] = {-2, 8, -1, -1, -1};
	auto roster = BuildDepartedRoster(dead, fired, other, short{-1}, 255);
	Check(roster.size() == 3 && roster[0].profileId == 4 &&
		roster[0].state == DepartedState::Dead && roster[1].profileId == 7 &&
		roster[1].state == DepartedState::Fired && roster[2].profileId == 8 &&
		roster[2].state == DepartedState::Other,
		"Departed views filter corrupt IDs and cross-list duplicates");
	Check(FindDepartedState(dead, fired, other, 4, short{-1}, 255) ==
			DepartedState::Dead &&
		FindDepartedState(dead, fired, other, 7, short{-1}, 255) ==
			DepartedState::Fired &&
		!FindDepartedState(dead, fired, other, 999, short{-1}, 255),
		"Departed-state queries are bounded and preserve category priority");

	Check(MoveDepartedProfile(dead, fired, other, short{7},
		DepartedState::Dead, short{-1}, 255),
		"Moving an existing profile between departed categories succeeds");
	roster = BuildDepartedRoster(dead, fired, other, short{-1}, 255);
	Check(roster.size() == 3 && roster[1].profileId == 7 &&
		roster[1].state == DepartedState::Dead,
		"A moved profile occurs exactly once in its new category");
	Check(RemoveDepartedProfile(dead, fired, other, short{4}, short{-1}) &&
		!RemoveDepartedProfile(dead, fired, other, short{42}, short{-1}),
		"Rehiring removes every stale occurrence and reports missing profiles");

	short fullDead[2] = {1, 2};
	short emptyFired[2] = {-1, -1};
	short emptyOther[2] = {-1, -1};
	Check(!MoveDepartedProfile(fullDead, emptyFired, emptyOther, short{3},
		DepartedState::Dead, short{-1}, 255) &&
		fullDead[0] == 1 && fullDead[1] == 2,
		"A full destination rejects additions without partial mutation");
	Check(!MoveDepartedProfile(fullDead, emptyFired, emptyOther, short{255},
		DepartedState::Other, short{-1}, 255),
		"Invalid profile IDs are never persisted");

	Check(NormalizeWindowStart(9, 0, 8) == 0 &&
		NormalizeWindowStart(9, 8, 8) == 0 &&
		NormalizeWindowStart(9, 9, 8) == 1 &&
		NormalizeWindowStart(1, 20, 8) == 1 &&
		CanScrollWindowDown(11, 20, 8) &&
		!CanScrollWindowDown(12, 20, 8),
		"Inventory windows normalize empty, exact, and stale offsets");
	Check(SliderPosition(7, 8, 8, 184) == 0 &&
		SliderPosition(1, 9, 8, 184) == 184 &&
		SliderPosition(6, 20, 8, 184) == 92 &&
		WindowStartFromSlider(999, 20, 8, 184) == 12 &&
		WindowStartFromSlider(92, 20, 8, 184) == 6 &&
		WindowStartFromSlider(92, 8, 8, 184) == 0,
		"Inventory slider mapping handles exact-page, midpoint, and bounds cases");

	return failures == 0 ? 0 : 1;
}
