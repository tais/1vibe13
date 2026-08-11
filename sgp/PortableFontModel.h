#ifndef SGP_PORTABLE_FONT_MODEL_H
#define SGP_PORTABLE_FONT_MODEL_H

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <string_view>
#include <type_traits>

namespace portable_font
{
constexpr std::uint32_t ReplacementCodePoint = 0xFFFDU;

struct DecodedCodePoint
{
	std::uint32_t value = ReplacementCodePoint;
	std::size_t units = 0;
	bool valid = false;
};

template <typename Unit>
DecodedCodePoint DecodeNext(
	std::basic_string_view<Unit> text, std::size_t offset) noexcept
{
	static_assert(sizeof(Unit) == 2 || sizeof(Unit) == 4,
		"portable font input must use UTF-16 or UTF-32 code units");
	using UnsignedUnit = typename std::make_unsigned<Unit>::type;
	if (offset >= text.size()) return {};

	const std::uint32_t first =
		static_cast<std::uint32_t>(static_cast<UnsignedUnit>(text[offset]));
	if constexpr (sizeof(Unit) == 2)
	{
		if (first >= 0xD800U && first <= 0xDBFFU)
		{
			if (offset + 1 < text.size())
			{
				const std::uint32_t second = static_cast<std::uint32_t>(
					static_cast<UnsignedUnit>(text[offset + 1]));
				if (second >= 0xDC00U && second <= 0xDFFFU)
				{
					return {0x10000U + ((first - 0xD800U) << 10U) +
						(second - 0xDC00U), 2, true};
				}
			}
			return {ReplacementCodePoint, 1, false};
		}
		if (first >= 0xDC00U && first <= 0xDFFFU)
			return {ReplacementCodePoint, 1, false};
		return {first, 1, true};
	}
	else
	{
		if (first > 0x10FFFFU || (first >= 0xD800U && first <= 0xDFFFU))
			return {ReplacementCodePoint, 1, false};
		return {first, 1, true};
	}
}

inline int ClampPixelHeight(std::int64_t configuredHeight,
	std::int64_t adjustment) noexcept
{
	std::int64_t adjusted = configuredHeight;
	if (adjustment > 0 &&
		configuredHeight > std::numeric_limits<std::int64_t>::max() - adjustment)
		adjusted = std::numeric_limits<std::int64_t>::max();
	else if (adjustment < 0 &&
		configuredHeight < std::numeric_limits<std::int64_t>::min() - adjustment)
		adjusted = std::numeric_limits<std::int64_t>::min();
	else
		adjusted += adjustment;
	if (adjusted < 0)
	{
		if (adjusted == std::numeric_limits<std::int64_t>::min())
			adjusted = std::numeric_limits<std::int64_t>::max();
		else
			adjusted = -adjusted;
	}
	if (adjusted < 1) return 1;
	if (adjusted > 256) return 256;
	return static_cast<int>(adjusted);
}

inline int ClampScaledPixelHeight(int nominalHeight, double scale) noexcept
{
	const int nominal = ClampPixelHeight(nominalHeight, 0);
	if (std::isnan(scale) || scale <= 0.0) return nominal;
	if (!std::isfinite(scale)) return 256;
	const double maximumScale = 256.0 / nominal;
	const double boundedScale = scale > maximumScale ? maximumScale : scale;
	const double scaled = static_cast<double>(nominal) * boundedScale;
	return ClampPixelHeight(static_cast<std::int64_t>(scaled), 0);
}

inline bool CheckedBitmapArea(
	int width, int height, std::size_t maximum, std::size_t& area) noexcept
{
	area = 0;
	if (width < 0 || height < 0) return false;
	if (width == 0 || height == 0) return true;
	const std::size_t w = static_cast<std::size_t>(width);
	const std::size_t h = static_cast<std::size_t>(height);
	if (w > maximum || h > maximum / w) return false;
	area = w * h;
	return area <= maximum;
}

inline std::uint8_t BlendChannel(
	std::uint8_t destination, std::uint8_t source, std::uint8_t coverage) noexcept
{
	const std::uint32_t inverse = 255U - coverage;
	return static_cast<std::uint8_t>(
		(static_cast<std::uint32_t>(source) * coverage +
		 static_cast<std::uint32_t>(destination) * inverse + 127U) / 255U);
}

inline int SaturatingPixelAdd(int total, int amount) noexcept
{
	const std::int64_t result =
		static_cast<std::int64_t>(total) + static_cast<std::int64_t>(amount);
	if (result > std::numeric_limits<std::int16_t>::max())
		return std::numeric_limits<std::int16_t>::max();
	if (result < 0) return 0;
	return static_cast<int>(result);
}
}

#endif
