#include "AimMemberProfileLayout.h"

#include <cstdlib>
#include <iostream>

namespace
{
using namespace AimMemberProfileLayoutModel;

void Require(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "FAILED: " << message << '\n';
	std::exit(EXIT_FAILURE);
}

bool Is(const Point& point, int x, int y)
{
	return point.x == x && point.y == y;
}

bool Is(const Rect& rect, int x, int y, int width, int height)
{
	return rect.x == x && rect.y == y &&
		rect.width == width && rect.height == height;
}
}

int main()
{
	using namespace AimMemberProfileLayoutModel;
	constexpr Anchors anchors{0, 0, 111, 46, 19};
	constexpr Layout legacy = MakeLayout(false, anchors);
	constexpr Layout expanded = MakeLayout(true, anchors);
	constexpr Anchors shiftedAnchors{160, 90, 271, 136, 19};
	constexpr Layout shifted = MakeLayout(true, shiftedAnchors);

	Require(Is(legacy.pageBounds, 111, 46, 502, 400) &&
		Is(expanded.pageBounds, 111, 46, 502, 400),
		"both variants occupy the same fixed Laptop web canvas");

	Require(Is(legacy.stats.panel, 232, 112) &&
		Is(expanded.stats.panel, 232, 80),
		"profile variants preserve their exact stats-panel origins");
	Require(Is(legacy.portrait, 119, 112, 110, 126) &&
		Is(legacy.face, 121, 114, 106, 122) &&
		Is(expanded.portrait, 119, 80, 110, 126) &&
		Is(expanded.face, 121, 82, 106, 122),
		"portrait visuals and face content retain the authored two-pixel inset");
	Require(legacy.faceStatusY == 221 && expanded.faceStatusY == 189,
		"face status text remains anchored to the portrait content");

	Require(legacy.stats.rows == std::array<int, 6>{146, 161, 176, 191, 206, 221} &&
		expanded.stats.rows == std::array<int, 6>{114, 129, 144, 159, 174, 189},
		"stat rows are derived from one consistent fifteen-pixel rhythm");
	Require(legacy.stats.firstLabelX == 241 && legacy.stats.secondLabelX == 370 &&
		legacy.stats.firstValueX == 343 && legacy.stats.secondValueX == 467 &&
		legacy.stats.firstDotX == 328 && legacy.stats.secondDotX == 451,
		"stat labels, dot leaders, and values preserve their authored columns");
	Require(Is(legacy.price.panel, 488, 112) &&
		Is(legacy.price.fee.origin, 495, 119) &&
		Is(legacy.price.contract.origin, 539, 119) &&
		Is(legacy.price.missionFeeValue.origin, 528, 140) &&
		Is(expanded.price.panel, 488, 80) &&
		Is(expanded.price.missionFeeValue.origin, 528, 80),
		"pricing and campaign fee fields retain variant-specific anchors");
	Require(Is(legacy.price.optionalGearLabel.origin, 124, 329) &&
		Is(expanded.price.optionalGearLabel.origin, 539, 159) &&
		Is(expanded.price.optionalGearCost.origin, 495, 159),
		"optional-gear text retains the layout-specific composition");
	Require(Is(legacy.activeMemberTitle.origin, 260, 99) &&
		Is(expanded.activeMemberTitle.origin, 260, 54),
		"member titles remain aligned with their corresponding artwork");

	Require(Is(legacy.inventory.firstCell, 117, 342, 61, 31) &&
		legacy.inventory.capacity() == 8 &&
		Is(expanded.inventory.firstCell, 135, 291, 64, 30) &&
		expanded.inventory.capacity() == kExpandedInventoryCapacity,
		"legacy and expanded equipment grids retain their exact geometry");
	Require(Is(expanded.inventory.cell(20), 519, 351, 64, 30) &&
		Is(expanded.inventory.contentOrigin(7), 138, 321),
		"equipment cells and item origins use the same row-major mapping");
	for (std::size_t i = 0; i < expanded.inventory.capacity(); ++i)
		Require(Contains(expanded.pageBounds, expanded.inventory.cell(i)),
			"every expanded equipment hitbox stays inside the web page");

	Require(Is(legacy.navigation.previousButton, 224, 401, 75, 18) &&
		Is(legacy.navigation.contactButton, 331, 401, 75, 18) &&
		Is(legacy.navigation.nextButton, 431, 401, 75, 18),
		"legacy navigation button rectangles preserve their authored positions");
	Require(Is(expanded.navigation.previousButton, 218, 406, 75, 18) &&
		Is(expanded.navigation.contactButton, 325, 406, 75, 18) &&
		Is(expanded.navigation.nextButton, 425, 406, 75, 18),
		"expanded navigation button rectangles preserve their authored positions");
	Require(Is(legacy.navigation.pageNumber, 582, 409) &&
		Is(expanded.navigation.pageNumber, 582, 409),
		"page numbering retains its shared footer anchor");
	Require(!Overlaps(expanded.navigation.previousButton,
		expanded.navigation.contactButton) &&
		!Overlaps(expanded.navigation.contactButton,
		expanded.navigation.nextButton),
		"navigation controls remain non-overlapping");

	Require(Is(legacy.biography.biography.origin, 124, 242) &&
		Is(legacy.biography.additionalLabel, 124, 288) &&
		Is(legacy.biography.additionalText.origin, 124, 303),
		"legacy biography sections preserve their exact vertical anchors");
	Require(Is(expanded.biography.biography.origin, 122, 209) &&
		Is(expanded.biography.additionalLabel, 122, 254) &&
		Is(expanded.biography.additionalText.origin, 122, 269),
		"expanded biography sections preserve their exact vertical anchors");

	Require(Is(legacy.help.leftClick.origin, 116, 54) &&
		Is(expanded.help.leftClick.origin, 200, 54) &&
		Is(legacy.help.rightClick.origin, 500, 54),
		"face-help text preserves variant-specific and shared anchors");
	Require(Is(expanded.kitButtons.button(0), 143, 382, 75, 18) &&
		Is(expanded.kitButtons.button(4), 503, 382, 75, 18),
		"gear-kit buttons derive from one tested stride");

	Require(Is(shifted.stats.panel, 392, 170) &&
		shifted.stats.firstDotX == 488 && shifted.stats.secondDotX == 611 &&
		Is(shifted.inventory.firstCell, 295, 381, 64, 30) &&
		Is(shifted.navigation.contactButton, 485, 496, 75, 18),
		"all geometry, including dot leaders, follows centered-screen offsets");

	Require(Contains(legacy.pageBounds, legacy.portrait) &&
		Contains(expanded.pageBounds, expanded.portrait) &&
		Contains(legacy.pageBounds, legacy.navigation.nextButton) &&
		Contains(expanded.pageBounds, expanded.navigation.nextButton),
		"profile visuals and controls remain inside the fixed Laptop web canvas");

	return EXIT_SUCCESS;
}
