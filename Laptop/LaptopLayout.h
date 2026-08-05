#ifndef LAPTOP_LAPTOP_LAYOUT_H
#define LAPTOP_LAPTOP_LAYOUT_H

namespace LaptopLayoutModel
{
	struct Point
	{
		int x = 0;
		int y = 0;
	};

	struct Rect
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;

		constexpr int right() const noexcept { return x + width; }
		constexpr int bottom() const noexcept { return y + height; }
	};

	struct TextArea
	{
		Point origin;
		int width = 0;
	};

	constexpr bool Contains(const Rect& outer, const Rect& inner) noexcept
	{
		return inner.x >= outer.x && inner.y >= outer.y &&
			inner.right() <= outer.right() &&
			inner.bottom() <= outer.bottom();
	}

	constexpr bool Overlaps(const Rect& first, const Rect& second) noexcept
	{
		return first.x < second.right() && second.x < first.right() &&
			first.y < second.bottom() && second.y < first.bottom();
	}
}

#endif
