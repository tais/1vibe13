#include "CursorStateModel.h"

#include <array>
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
	using namespace CursorStateModel;

	Require(!IsValidIndex(4, -1) && IsValidIndex(4, 3) &&
		!IsValidIndex(4, 4) &&
		!IsValidIndex(4, std::numeric_limits<std::uint64_t>::max()),
		"cursor indices reject negative, exact-end, and oversized values");

	constexpr std::array<SurfacePair, 2> surfacePairs{{{2, 20}, {3, 30}}};
	Require(ResolveSurface(2, true, surfacePairs) == 20 &&
		ResolveSurface(20, false, surfacePairs) == 2 &&
		ResolveSurface(3, false, surfacePairs) == 3 &&
		ResolveSurface(99, true, surfacePairs) == 99,
		"cursor surface selection round-trips legacy and NCTH families");

	Require(NextAnimationFrame(0, 3, true) == 1 &&
		NextAnimationFrame(2, 3, true) == 0 &&
		NextAnimationFrame(99, 3, true) == 0 &&
		NextAnimationFrame(4, 0, true) == 0 &&
		NextAnimationFrame(4, 0, false) == 4,
		"animation frames wrap stale and zero-frame metadata safely");

	constexpr std::array<int, 2> levelOffsets{{0, 50}};
	int selectedOffset = 17;
	Require(!TrySelectMouseLevelOffset(-1, levelOffsets, selectedOffset) &&
		selectedOffset == 17 &&
		!TrySelectMouseLevelOffset(2, levelOffsets, selectedOffset) &&
		selectedOffset == 17 &&
		TrySelectMouseLevelOffset(1, levelOffsets, selectedOffset) &&
		selectedOffset == 50,
		"mouse levels reject negative and exact-end values without publication");

	const ChanceBarGeometry normal =
		ComputeChanceBarGeometry(32, 32, 64, 64, false, 0);
	Require(normal.left == 15 && normal.right == 49 && normal.top == 15 &&
		normal.interiorPixels == 33 && ChanceBarFillPixels(33, 99) == 33 &&
		ChanceBarFillPixels(33, 50) == 16,
		"chance-bar geometry preserves the legacy valid-input layout");
	const ChanceBarGeometry edge =
		ComputeChanceBarGeometry(0, 0, 1, 1, true, 0);
	Require(!edge.drawable() && edge.left == 0 && edge.top == 0 &&
		!PixelOffset(-1, 0, 64, 64, 64).has_value() &&
		!PixelOffset(0, -1, 64, 64, 64).has_value() &&
		!PixelOffset(64, 0, 64, 64, 64).has_value() &&
		!PixelOffset(0, 64, 64, 64, 64).has_value() &&
		!PixelOffset(0, 0, 63, 64, 64).has_value() &&
		!PixelOffset(
			0, 2, std::numeric_limits<std::size_t>::max(), 1, 3).has_value() &&
		!PixelOffset(
			1, 1, std::numeric_limits<std::size_t>::max(), 2, 2).has_value() &&
		PixelOffset(63, 63, 64, 64, 64).value() == 4095,
			"chance-bar geometry stays signed and clips every fixed-buffer write");
	const ChanceBarGeometry extreme = ComputeChanceBarGeometry(
		std::numeric_limits<int>::max(),
		std::numeric_limits<int>::max(),
		std::numeric_limits<std::size_t>::max(),
		std::numeric_limits<std::size_t>::max(),
		true,
		std::numeric_limits<std::size_t>::max());
	Require(extreme.left < extreme.right &&
		extreme.right == std::numeric_limits<int>::max() &&
		extreme.top == std::numeric_limits<int>::max() &&
		ChanceBarFillPixels(std::numeric_limits<std::size_t>::max(), 99) ==
			std::numeric_limits<std::size_t>::max(),
		"chance-bar arithmetic saturates coordinates and avoids size overflow");

	Require(BoundedCount(255, 10) == 10 && BoundedCount(2, 10) == 2,
		"chance-bar shot counts clamp to the CTH sample capacity");

	const FlashTransition lit = AdvanceFlash(true, true, 0);
	const FlashTransition dark = AdvanceFlash(true, true, lit.frame);
	Require(lit.changed && lit.frame == 1 && lit.playSound &&
		dark.changed && dark.frame == 0 && !dark.playSound &&
		!AdvanceFlash(false, true, 7).changed,
		"flash transitions preserve sound-on-visible-frame behavior");

	Require(!OneBasedIndex(0, 5).has_value() &&
		OneBasedIndex(1, 5).value() == 0 &&
		OneBasedIndex(5, 5).value() == 4 &&
		OneBasedIndex(255, 5).value() == 4,
		"one-based AP shades clamp oversized requests");

	std::cout << "Utils cursor state model tests passed\n";
	return 0;
}
