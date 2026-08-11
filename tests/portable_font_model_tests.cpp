#include "sgp/PortableFontModel.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

namespace
{
void Check(bool condition, const char* message)
{
	if (condition) return;
	std::cerr << "portable font model failure: " << message << '\n';
	std::exit(1);
}
}

int main()
{
	using portable_font::DecodeNext;

	const std::u16string_view utf16 = u"A\U0001F642Z";
	auto decoded = DecodeNext(utf16, 0);
	Check(decoded.valid && decoded.value == 'A' && decoded.units == 1,
		"UTF-16 ASCII decoding");
	decoded = DecodeNext(utf16, 1);
	Check(decoded.valid && decoded.value == 0x1F642 && decoded.units == 2,
		"UTF-16 surrogate-pair decoding");
	decoded = DecodeNext(utf16, 3);
	Check(decoded.valid && decoded.value == 'Z' && decoded.units == 1,
		"UTF-16 resumes after a surrogate pair");

	const char16_t malformedUnits[] = {
		static_cast<char16_t>(0xD800), u'x', static_cast<char16_t>(0xDC00)};
	const std::u16string_view malformed(malformedUnits, 3);
	decoded = DecodeNext(malformed, 0);
	Check(!decoded.valid &&
		decoded.value == portable_font::ReplacementCodePoint &&
		decoded.units == 1, "lone high surrogate replacement");
	decoded = DecodeNext(malformed, 2);
	Check(!decoded.valid &&
		decoded.value == portable_font::ReplacementCodePoint &&
		decoded.units == 1, "lone low surrogate replacement");

	const char32_t utf32Units[] = {
		U'A', static_cast<char32_t>(0x10FFFF), static_cast<char32_t>(0x110000),
		static_cast<char32_t>(0xDFFF)};
	const std::u32string_view utf32(utf32Units, 4);
	Check(DecodeNext(utf32, 1).valid &&
		DecodeNext(utf32, 1).value == 0x10FFFF,
		"highest Unicode scalar is accepted");
	Check(!DecodeNext(utf32, 2).valid &&
		DecodeNext(utf32, 2).value == portable_font::ReplacementCodePoint,
		"out-of-range UTF-32 is replaced");
	Check(!DecodeNext(utf32, 3).valid,
		"UTF-32 surrogate values are replaced");
	Check(DecodeNext(utf32, utf32.size()).units == 0,
		"exact-end decode is empty");

	Check(portable_font::ClampPixelHeight(-12, 0) == 12,
		"legacy negative font height");
	Check(portable_font::ClampPixelHeight(-12, 2) == 10,
		"legacy adjustment direction");
	Check(portable_font::ClampPixelHeight(0, 0) == 1,
		"zero height lower bound");
	Check(portable_font::ClampPixelHeight(1000, 0) == 256,
		"font height upper bound");
	Check(portable_font::ClampPixelHeight(
		std::numeric_limits<std::int64_t>::min(), 0) == 256,
		"signed-min height cannot overflow");
	Check(portable_font::ClampPixelHeight(
		std::numeric_limits<std::int64_t>::max(), 1) == 256,
		"positive height adjustment cannot overflow");
	Check(portable_font::ClampPixelHeight(
		std::numeric_limits<std::int64_t>::min(), -1) == 256,
		"negative height adjustment cannot overflow");
	Check(portable_font::ClampScaledPixelHeight(11, 1.5) == 16,
		"scaled legacy height truncates compatibly");
	Check(portable_font::ClampScaledPixelHeight(
		11, std::numeric_limits<double>::infinity()) == 256,
		"infinite scale is bounded");
	Check(portable_font::ClampScaledPixelHeight(
		11, std::numeric_limits<double>::quiet_NaN()) == 11,
		"invalid scale returns the nominal height");

	std::size_t area = 123;
	Check(portable_font::CheckedBitmapArea(64, 32, 2048, area) &&
		area == 2048, "exact-capacity glyph bitmap");
	Check(!portable_font::CheckedBitmapArea(65, 32, 2048, area) &&
		area == 0, "oversized glyph bitmap");
	Check(portable_font::CheckedBitmapArea(0, 32, 2048, area) &&
		area == 0, "zero-width glyph");
	Check(!portable_font::CheckedBitmapArea(-1, 32, 2048, area),
		"negative glyph geometry");

	Check(portable_font::BlendChannel(17, 240, 0) == 17,
		"zero coverage preserves destination");
	Check(portable_font::BlendChannel(17, 240, 255) == 240,
		"full coverage publishes source");
	Check(portable_font::BlendChannel(0, 255, 128) == 128,
		"half coverage rounds deterministically");

	Check(portable_font::SaturatingPixelAdd(32000, 1000) ==
		std::numeric_limits<std::int16_t>::max(),
		"width accumulation saturates");
	Check(portable_font::SaturatingPixelAdd(8, -20) == 0,
		"negative advance cannot underflow");

	std::cout << "portable font model tests passed\n";
	return 0;
}
