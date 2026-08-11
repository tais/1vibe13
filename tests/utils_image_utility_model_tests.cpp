#include "ImageUtilityModel.h"

#include <cstdint>
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
	using namespace UtilsImageUtilityModel;

	std::size_t product = 17;
	Require(CheckedProduct(88, 44, product) && product == 3872 &&
		!CheckedProduct((std::numeric_limits<std::size_t>::max)(), 2,
			product) && product == 3872,
		"checked products reject host-size overflow without publishing a partial result");

	std::size_t bytes = 77;
	Require(CheckedImageByteCount(88, 44, 3, bytes) && bytes == 11616 &&
		!CheckedImageByteCount(0, 44, 3, bytes) && bytes == 11616 &&
		!CheckedImageByteCount(-1, 44, 3, bytes) && bytes == 11616 &&
		!CheckedImageByteCount((std::numeric_limits<std::int64_t>::max)(),
			2, 3, bytes) && bytes == 11616,
		"image dimensions reject empty, negative, and overflowing byte counts without publishing a partial result");

	std::uint32_t capacity = 99;
	Require(CheckedEtrleCapacity(1, 1, capacity) && capacity == 3 &&
		CheckedEtrleCapacity(32767, 32767, capacity) &&
		capacity == 3221028867U &&
		!CheckedEtrleCapacity(65535, 65535, capacity) &&
		capacity == 3221028867U,
		"ETRLE scratch capacity remains representable in the serialized 32-bit format");

	Require(ContainsRectangle(88, 44, 0, 0, 88, 44) &&
		ContainsRectangle(88, 44, 87, 43, 1, 1) &&
		!ContainsRectangle(88, 44, -1, 0, 1, 1) &&
		!ContainsRectangle(88, 44, 88, 43, 1, 1) &&
		!ContainsRectangle(88, 44, 87, 43, 2, 1) &&
		!ContainsRectangle(88, 44, 0, 0, 0, 1),
		"subimages reject negative, empty, exact-end, and overflowing geometry");

	std::uint32_t offset = 55;
	Require(PixelOffset(88, 44, 87, 43, offset) && offset == 3871 &&
		!PixelOffset(88, 44, 88, 43, offset) && offset == 3871,
		"pixel offsets validate both axes before publishing an offset");

	Require(CanAppendSerializedBytes(100, 200) &&
		CanAppendSerializedBytes(
			(std::numeric_limits<std::uint32_t>::max)(), 0) &&
		!CanAppendSerializedBytes(
			(std::numeric_limits<std::uint32_t>::max)(), 1),
		"serialized STI staging rejects 32-bit size overflow");

	const std::uint8_t palette[] = {
		255, 0, 0,
		0, 255, 0,
		0, 0, 255
	};
	Require(NearestPaletteIndex(250, 2, 3, palette, 3) == 0 &&
		NearestPaletteIndex(1, 240, 2, palette, 3) == 1 &&
		NearestPaletteIndex(4, 3, 249, palette, 3) == 2,
		"palette mapping uses bounded integer distance with deterministic first-match ties");

	std::cout << "Utils image utility model tests passed\n";
	return 0;
}
