#include "MercFilesLayout.h"

#include <cstdlib>
#include <iostream>

namespace
{
using namespace MercFilesLayoutModel;

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
	using namespace MercFilesLayoutModel;
	constexpr Layout layout = MakeLayout({0, 0, 111, 46, 19});

	Require(Is(layout.pageBounds, 111, 46, 502, 400),
		"M.E.R.C. files retain the fixed Laptop web canvas");
	Require(layout.inventory.capacity() == kInventoryCapacity &&
		Is(layout.inventory.cell(0), 133, 211, 64, 30) &&
		Is(layout.inventory.cell(6), 517, 211, 64, 30) &&
		Is(layout.inventory.cell(7), 133, 241, 64, 30) &&
		Is(layout.inventory.cell(20), 517, 271, 64, 30),
		"inventory drawing, backgrounds, and hitboxes share one row-major grid");
	Require(Is(layout.inventory.contentOrigin(0), 136, 211) &&
		Is(layout.inventory.countTextOrigin(0), 135, 231) &&
		layout.inventory.countTextWidth == 59,
		"item artwork and count text preserve their exact cell-relative offsets");
	for (std::size_t slot = 0; slot < layout.inventory.capacity(); ++slot)
		Require(LaptopLayoutModel::Contains(
			layout.pageBounds, layout.inventory.cell(slot)),
			"every M.E.R.C. inventory hitbox stays inside the page bounds");

	Require(Is(layout.kitButtons.button(0), 130, 345, 75, 30) &&
		Is(layout.kitButtons.button(4), 510, 345, 75, 30),
		"all five M.E.R.C. loadout buttons derive from one tested stride");
	for (std::size_t button = 0; button < kKitButtonCount; ++button)
		Require(LaptopLayoutModel::Contains(
			layout.pageBounds, layout.kitButtons.button(button)),
			"every M.E.R.C. loadout control stays inside the page bounds");

	constexpr Layout shifted = MakeLayout({160, 90, 271, 136, 19});
	Require(Is(shifted.inventory.firstCell, 293, 301, 64, 30) &&
		Is(shifted.kitButtons.button(0), 290, 435, 75, 30),
		"M.E.R.C. inventory and loadout geometry follows centered-screen offsets");

	return EXIT_SUCCESS;
}
