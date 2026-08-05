#include "UtilsUiStateModel.h"

#include <cstdlib>
#include <iostream>
#include <limits>

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
	using namespace UtilsUiStateModel;

	Require(!IsValidIndex(4, -1) && IsValidIndex(4, 3) &&
		!IsValidIndex(4, 4),
		"UI indices reject negative and exact-end values");
	Require(!IsValidIndex(0, 0u) &&
		!IsValidIndex(4, std::numeric_limits<std::uint64_t>::max()),
		"empty and oversized UI indices are rejected");

	Require(ClampIncrement(99, 8) == 8 && ClampIncrement(3, 8) == 3,
		"slider values clamp to their configured increment count");
	Require(SliderIncrementFromPosition(0, 0, 8) == 0 &&
		SliderIncrementFromPosition(5, 10, 8) == 4 &&
		SliderIncrementFromPosition(20, 10, 8) == 8,
		"slider input handles zero extent, rounding, and positions past the end");
	Require(SliderPositionFromIncrement(100, 4, 8) == 50 &&
		SliderPositionFromIncrement(100, 99, 8) == 100 &&
		SliderPositionFromIncrement(100, 4, 0) == 0,
		"slider rendering handles clamped and zero increment counts");

	BoundedIdDirectory<unsigned, unsigned> directory(2);
	Require(directory.insert(10, 100) && directory.insert(11, 110),
		"bounded callback directories accept live unique mappings");
	Require(!directory.insert(10, 999) && !directory.insert(12, 120),
		"callback directories reject duplicate IDs and full-capacity insertion");
	Require(directory.find(10).value() == 100 &&
		!directory.find(99).has_value(),
		"callback lookup distinguishes live and stale IDs");
	Require(directory.erase(10) && !directory.erase(10) &&
		directory.insert(12, 120) && directory.size() == 2,
		"callback removal is idempotent and releases capacity");
	directory.clear();
	Require(directory.size() == 0 && !directory.find(11).has_value(),
		"callback teardown clears every stale mapping");

	std::cout << "Utils UI state model tests passed\n";
	return 0;
}
