#ifndef UTILS_IMAGE_UTILITY_MODEL_H
#define UTILS_IMAGE_UTILITY_MODEL_H

#include <cstddef>
#include <cstdint>
#include <limits>

namespace UtilsImageUtilityModel
{
	inline bool CheckedProduct(
		std::size_t left, std::size_t right, std::size_t& result) noexcept
	{
		if (left != 0 &&
			right > (std::numeric_limits<std::size_t>::max)() / left)
			return false;
		result = left * right;
		return true;
	}

	inline bool CheckedImageByteCount(
		std::int64_t width, std::int64_t height, std::size_t bytesPerPixel,
		std::size_t& result) noexcept
	{
		if (width <= 0 || height <= 0 || bytesPerPixel == 0) return false;
		if (static_cast<std::uint64_t>(width) >
				(std::numeric_limits<std::size_t>::max)() ||
			static_cast<std::uint64_t>(height) >
				(std::numeric_limits<std::size_t>::max)())
		{
			return false;
		}
		std::size_t pixels = 0;
		std::size_t staged = 0;
		if (!CheckedProduct(static_cast<std::size_t>(width),
			static_cast<std::size_t>(height), pixels) ||
			!CheckedProduct(pixels, bytesPerPixel, staged) ||
			staged > (std::numeric_limits<std::uint32_t>::max)())
		{
			return false;
		}
		result = staged;
		return true;
	}

	inline bool CheckedEtrleCapacity(
		std::uint16_t width, std::uint16_t height, std::uint32_t& result) noexcept
	{
		std::size_t capacity = 0;
		if (!CheckedImageByteCount(width, height, 3, capacity)) return false;
		result = static_cast<std::uint32_t>(capacity);
		return true;
	}

	inline bool ContainsRectangle(
		std::uint16_t imageWidth, std::uint16_t imageHeight,
		std::int32_t x, std::int32_t y,
		std::uint16_t width, std::uint16_t height) noexcept
	{
		if (imageWidth == 0 || imageHeight == 0 || x < 0 || y < 0 ||
			width == 0 || height == 0)
		{
			return false;
		}
		return static_cast<std::uint32_t>(x) < imageWidth &&
			static_cast<std::uint32_t>(y) < imageHeight &&
			width <= static_cast<std::uint32_t>(imageWidth) -
				static_cast<std::uint32_t>(x) &&
			height <= static_cast<std::uint32_t>(imageHeight) -
				static_cast<std::uint32_t>(y);
	}

	inline bool PixelOffset(
		std::uint16_t width, std::uint16_t height,
		std::int32_t x, std::int32_t y, std::uint32_t& result) noexcept
	{
		if (!ContainsRectangle(width, height, x, y, 1, 1)) return false;
		result = static_cast<std::uint32_t>(y) * width +
			static_cast<std::uint32_t>(x);
		return true;
	}

	inline bool CanAppendSerializedBytes(
		std::size_t current, std::size_t addition) noexcept
	{
		return addition <= (std::numeric_limits<std::uint32_t>::max)() &&
			current <= (std::numeric_limits<std::uint32_t>::max)() - addition;
	}

	inline std::uint8_t NearestPaletteIndex(
		std::uint8_t red, std::uint8_t green, std::uint8_t blue,
		const std::uint8_t* paletteRgb, std::size_t colorCount) noexcept
	{
		if (!paletteRgb || colorCount == 0) return 0;
		if (colorCount > 256) colorCount = 256;
		std::uint32_t lowestDistance =
			(std::numeric_limits<std::uint32_t>::max)();
		std::uint8_t best = 0;
		for (std::size_t index = 0; index < colorCount; ++index)
		{
			const std::int32_t redDifference =
				static_cast<std::int32_t>(red) - paletteRgb[index * 3];
			const std::int32_t greenDifference =
				static_cast<std::int32_t>(green) - paletteRgb[index * 3 + 1];
			const std::int32_t blueDifference =
				static_cast<std::int32_t>(blue) - paletteRgb[index * 3 + 2];
			const std::uint32_t distance =
				static_cast<std::uint32_t>(redDifference * redDifference +
				greenDifference * greenDifference + blueDifference * blueDifference);
			if (distance < lowestDistance)
			{
				lowestDistance = distance;
				best = static_cast<std::uint8_t>(index);
			}
		}
		return best;
	}
}

#endif
