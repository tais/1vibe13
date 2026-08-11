#include "WinFont.h"

#include "FileMan.h"
#include "Font Control.h"
#include "GameSettings.h"
#include "PortableFontModel.h"
#include "himage.h"
#include "pixfmt.h"
#include "vsurface.h"

#include <SDL3/SDL_log.h>
#include <language.hpp>
#include <vfs/Tools/vfs_property_container.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

extern float fTooltipScaleFactor;

namespace
{
constexpr std::size_t MaximumFontBytes = 64U * 1024U * 1024U;
constexpr std::size_t MaximumGlyphPixels = 4U * 1024U * 1024U;
constexpr std::size_t MaximumCachedGlyphPixels = 1U * 1024U * 1024U;
constexpr std::size_t MaximumCachedGlyphs = 512;
constexpr std::size_t MaximumWinFontHandles = 32;

class LegacyFileOwner
{
public:
	explicit LegacyFileOwner(HWFILE file) noexcept : file_(file) {}
	~LegacyFileOwner() noexcept
	{
		if (!file_) return;
		try { FileClose(file_); } catch (...) {}
	}
	LegacyFileOwner(const LegacyFileOwner&) = delete;
	LegacyFileOwner& operator=(const LegacyFileOwner&) = delete;
	HWFILE get() const noexcept { return file_; }
private:
	HWFILE file_;
};

struct FontBytes
{
	std::vector<std::uint8_t> bytes;
	int offset = -1;
	std::string source;
};

struct RasterizedGlyph
{
	int x0 = 0;
	int y0 = 0;
	int width = 0;
	int height = 0;
	std::vector<std::uint8_t> coverage;
};

struct PortableWinFont
{
	std::shared_ptr<const FontBytes> storage;
	stbtt_fontinfo info{};
	float scale = 0.0F;
	COLORVAL foreground = FROMRGB(255, 255, 255);
	COLORVAL background = FROMRGB(0, 0, 0);
	INT16 height = 0;
	INT16 lineHeight = 0;
	INT16 ascent = 0;
	std::unordered_map<std::uint32_t, RasterizedGlyph> glyphs;
	std::size_t cachedGlyphPixels = 0;
	bool embolden = false;
	bool active = false;
};

struct CatalogueFont
{
	const wchar_t* section;
	INT32 defaultHeight;
	INT32 defaultWeight;
	COLORVAL color;
};

constexpr std::array<CatalogueFont, WIN_LASTFONT> Catalogue = {{
	{L"LargeFont1", -12, 700, FROMRGB(98, 98, 98)},
	{L"SmallFont1", -11, 700, FROMRGB(98, 98, 98)},
	{L"TinyFont1", -12, 400, FROMRGB(98, 98, 98)},
	{L"12PointFont1", -14, 400, 0},
	{L"CompFont", -12, 400, FROMRGB(0, 255, 0)},
	{L"SmallCompFont", -10, 400, FROMRGB(98, 98, 98)},
	{L"10PointRoman", -12, 400, FROMRGB(0, 255, 0)},
	{L"12PointRoman", -14, 400, FROMRGB(0, 255, 0)},
	{L"14PointSansSerif", -16, 400, FROMRGB(0, 255, 0)},
	{L"10PointArial", -11, 400, 0},
	{L"14PointArial", -16, 400, FROMRGB(0, 255, 0)},
	{L"12PointArial", -13, 400, FROMRGB(222, 222, 222)},
	{L"BlockyFont", -11, 700, FROMRGB(217, 217, 217)},
	{L"BlockyFont2", -12, 400, FROMRGB(217, 217, 217)},
	{L"10PointArialBold", -11, 700, 0},
	{L"12PointArialFixedFont", -12, 400, 0},
	{L"16PointArial", -20, 400, FROMRGB(222, 222, 222)},
	{L"BlockFontNarrow", -11, 700, FROMRGB(128, 0, 0)},
	{L"14PointHumanist", -15, 700, FROMRGB(255, 0, 0)},
	{L"HugeFont", -19, 700, FROMRGB(0, 255, 0)}
}};

std::array<PortableWinFont, MaximumWinFontHandles> Fonts;
std::array<INT32, WIN_LASTFONT> MainHandles = [] {
	std::array<INT32, WIN_LASTFONT> handles{};
	handles.fill(-1);
	return handles;
}();
std::shared_ptr<const FontBytes> RegularBytes;
std::shared_ptr<const FontBytes> BoldBytes;
INT32 FontAdjustment = 0;
bool MainInitialized = false;

bool ReadVfsBytes(const std::string& path, std::vector<std::uint8_t>& output)
{
	if (path.empty()) return false;
	try
	{
		LegacyFileOwner file(FileOpen(const_cast<char*>(path.c_str()),
			FILE_ACCESS_READ | FILE_OPEN_EXISTING));
		if (!file.get()) return false;
		const UINT32 size = FileGetSize(file.get());
		if (size < 12 || static_cast<std::size_t>(size) > MaximumFontBytes)
			return false;
		std::vector<std::uint8_t> staged(size);
		UINT32 read = 0;
		if (!FileRead(file.get(), staged.data(), size, &read) || read != size)
			return false;
		output = std::move(staged);
		return true;
	}
	catch (...) { return false; }
}

bool ReadOsBytes(const std::string& path, std::vector<std::uint8_t>& output)
{
	try
	{
		std::ifstream stream(std::filesystem::u8path(path), std::ios::binary);
		if (!stream) return false;
		stream.seekg(0, std::ios::end);
		const std::streamoff end = stream.tellg();
		if (end < 12 || end > static_cast<std::streamoff>(MaximumFontBytes))
			return false;
		stream.seekg(0, std::ios::beg);
		std::vector<std::uint8_t> staged(static_cast<std::size_t>(end));
		const std::streamsize readSize = static_cast<std::streamsize>(end);
		stream.read(reinterpret_cast<char*>(staged.data()), readSize);
		if (!stream || stream.gcount() != readSize) return false;
		output = std::move(staged);
		return true;
	}
	catch (...) { return false; }
}

std::shared_ptr<const FontBytes> LoadFontBytes(
	const std::vector<std::string>& candidates)
{
	for (const std::string& path : candidates)
	{
		if (path.empty()) continue;
		std::vector<std::uint8_t> bytes;
		if (!ReadVfsBytes(path, bytes) && !ReadOsBytes(path, bytes)) continue;
		const int offset = stbtt_GetFontOffsetForIndex(bytes.data(), 0);
		if (offset < 0 ||
			static_cast<std::size_t>(offset) > bytes.size() - 12U)
			continue;
		stbtt_fontinfo probe{};
		if (!stbtt_InitFont(&probe, bytes.data(), offset)) continue;
		// GDI used platform font linking for Chinese. stb intentionally does
		// not, so reject a face that cannot cover even the baseline CJK block
		// and keep searching; if no capable face exists the bitmap path wins.
		if (g_lang == i18n::Lang::zh &&
			stbtt_FindGlyphIndex(&probe, 0x4E00) == 0)
			continue;
		try
		{
			auto loaded = std::make_shared<FontBytes>();
			loaded->bytes = std::move(bytes);
			loaded->offset = offset;
			loaded->source = path;
			return loaded;
		}
		catch (...) { return {}; }
	}
	return {};
}

std::vector<std::string> RegularFontCandidates(const std::string& configured)
{
	std::vector<std::string> result;
	if (!configured.empty()) result.push_back(configured);
	result.emplace_back("FONTS\\ja2font3.ttf");
	result.emplace_back("FONTS\\ja2font3.otf");
#if defined(_WIN32)
	if (g_lang == i18n::Lang::zh)
	{
		result.emplace_back("C:\\Windows\\Fonts\\msyh.ttc");
		result.emplace_back("C:\\Windows\\Fonts\\simsun.ttc");
	}
	result.emplace_back("C:\\Windows\\Fonts\\arial.ttf");
	result.emplace_back("C:\\Windows\\Fonts\\segoeui.ttf");
#elif defined(__APPLE__)
	if (g_lang == i18n::Lang::zh)
		result.emplace_back("/System/Library/Fonts/PingFang.ttc");
	result.emplace_back("/System/Library/Fonts/Supplemental/Arial.ttf");
	result.emplace_back("/System/Library/Fonts/Supplemental/Arial Unicode.ttf");
	if (g_lang != i18n::Lang::zh)
		result.emplace_back("/System/Library/Fonts/PingFang.ttc");
#else
	if (g_lang == i18n::Lang::zh)
	{
		result.emplace_back(
			"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
		result.emplace_back(
			"/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc");
	}
	result.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
	result.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
	if (g_lang != i18n::Lang::zh)
		result.emplace_back(
			"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc");
#endif
	return result;
}

std::vector<std::string> BoldFontCandidates(
	const std::string& configured, const std::string& regularSource)
{
	std::vector<std::string> result;
	if (!configured.empty())
	{
		result.push_back(configured);
		return result;
	}
	if (regularSource == "FONTS\\ja2font3.ttf" ||
		regularSource == "FONTS\\ja2font3.otf")
	{
		result.emplace_back("FONTS\\ja2font3-bold.ttf");
		result.emplace_back("FONTS\\ja2font3-bold.otf");
		return result;
	}
#if defined(_WIN32)
	if (regularSource == "C:\\Windows\\Fonts\\msyh.ttc")
		result.emplace_back("C:\\Windows\\Fonts\\msyhbd.ttc");
	else if (regularSource == "C:\\Windows\\Fonts\\arial.ttf")
		result.emplace_back("C:\\Windows\\Fonts\\arialbd.ttf");
	else if (regularSource == "C:\\Windows\\Fonts\\segoeui.ttf")
		result.emplace_back("C:\\Windows\\Fonts\\segoeuib.ttf");
#elif defined(__APPLE__)
	if (regularSource == "/System/Library/Fonts/Supplemental/Arial.ttf")
		result.emplace_back("/System/Library/Fonts/Supplemental/Arial Bold.ttf");
#else
	if (regularSource ==
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf")
		result.emplace_back(
			"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
	else if (regularSource ==
		"/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf")
		result.emplace_back(
			"/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf");
	else if (regularSource ==
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc")
		result.emplace_back(
			"/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc");
#endif
	return result;
}

bool EnsureFontBytes()
{
	if (RegularBytes) return true;
	try
	{
		vfs::PropertyContainer properties;
		properties.initFromIniFile(GAME_INI_FILE);
		const std::string configuredRegular =
			properties.getStringProperty(
				L"Ja2 Settings", L"WIN_FONT_FILE", L"").utf8();
		const std::string configuredBold =
			properties.getStringProperty(
				L"Ja2 Settings", L"WIN_FONT_BOLD_FILE", L"").utf8();
		FontAdjustment = static_cast<INT32>(properties.getIntProperty(
			L"Ja2 Settings", L"WIN_FONT_ADJUST", 0, -128, 128));
		RegularBytes = LoadFontBytes(RegularFontCandidates(configuredRegular));
		if (!RegularBytes)
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
				"Scalable fonts disabled: WIN_FONT_FILE and platform fallbacks could not be loaded");
			return false;
		}
		// An explicit/custom regular face without an explicit bold companion
		// stays in-family via synthetic bold instead of silently mixing in an
		// unrelated system face.
		const bool customRegular = !configuredRegular.empty();
		BoldBytes = customRegular && configuredBold.empty()
			? std::shared_ptr<const FontBytes>{}
			: LoadFontBytes(BoldFontCandidates(
				configuredBold, RegularBytes->source));
		if (!BoldBytes) BoldBytes = RegularBytes;
		SDL_Log("Scalable font backend: %s%s", RegularBytes->source.c_str(),
			BoldBytes == RegularBytes ? " (synthetic bold)" : "");
		return true;
	}
	catch (...)
	{
		RegularBytes.reset();
		BoldBytes.reset();
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
			"Scalable fonts disabled: font configuration failed");
		return false;
	}
}

PortableWinFont* GetFont(INT32 handle)
{
	if (handle < 0 ||
		static_cast<std::size_t>(handle) >= Fonts.size() ||
		!Fonts[static_cast<std::size_t>(handle)].active)
		return nullptr;
	return &Fonts[static_cast<std::size_t>(handle)];
}

INT32 FindFreeFont() noexcept
{
	for (std::size_t index = 0; index < Fonts.size(); ++index)
		if (!Fonts[index].active) return static_cast<INT32>(index);
	return -1;
}

std::uint32_t GlyphCodePoint(
	const PortableWinFont& font, std::uint32_t requested) noexcept
{
	if (stbtt_FindGlyphIndex(&font.info, static_cast<int>(requested)) != 0)
		return requested;
	if (stbtt_FindGlyphIndex(&font.info,
		static_cast<int>(portable_font::ReplacementCodePoint)) != 0)
		return portable_font::ReplacementCodePoint;
	return static_cast<std::uint32_t>('?');
}

int GlyphAdvance(const PortableWinFont& font, std::uint32_t current,
	std::uint32_t next) noexcept
{
	int advance = 0;
	int leftBearing = 0;
	stbtt_GetCodepointHMetrics(
		&font.info, static_cast<int>(current), &advance, &leftBearing);
	(void)leftBearing;
	double pixels = static_cast<double>(advance) * font.scale;
	if (next != 0)
		pixels += static_cast<double>(stbtt_GetCodepointKernAdvance(
			&font.info, static_cast<int>(current), static_cast<int>(next))) *
			font.scale;
	if (font.embolden) pixels += 1.0;
	if (!std::isfinite(pixels) || pixels <= 0.0) return 0;
	if (pixels >= std::numeric_limits<INT16>::max())
		return std::numeric_limits<INT16>::max();
	return static_cast<int>(std::lround(pixels));
}

const RasterizedGlyph* RasterizeGlyph(
	PortableWinFont& font, std::uint32_t codePoint, RasterizedGlyph& scratch)
{
	const auto found = font.glyphs.find(codePoint);
	if (found != font.glyphs.end()) return &found->second;

	RasterizedGlyph staged;
	int x1 = 0;
	int y1 = 0;
	stbtt_GetCodepointBitmapBox(&font.info, static_cast<int>(codePoint),
		font.scale, font.scale, &staged.x0, &staged.y0, &x1, &y1);
	const std::int64_t width =
		static_cast<std::int64_t>(x1) - staged.x0;
	const std::int64_t height =
		static_cast<std::int64_t>(y1) - staged.y0;
	if (width < 0 || height < 0 ||
		width > std::numeric_limits<int>::max() ||
		height > std::numeric_limits<int>::max())
		return nullptr;
	staged.width = static_cast<int>(width);
	staged.height = static_cast<int>(height);
	std::size_t area = 0;
	if (!portable_font::CheckedBitmapArea(
		staged.width, staged.height, MaximumGlyphPixels, area))
		return nullptr;
	staged.coverage.resize(area);
	if (area != 0)
		stbtt_MakeCodepointBitmap(&font.info, staged.coverage.data(),
			staged.width, staged.height, staged.width, font.scale, font.scale,
			static_cast<int>(codePoint));

	if (font.glyphs.size() < MaximumCachedGlyphs &&
		area <= MaximumCachedGlyphPixels - std::min(
			font.cachedGlyphPixels, MaximumCachedGlyphPixels))
	{
		try
		{
			const auto inserted = font.glyphs.emplace(codePoint, staged);
			if (inserted.second) font.cachedGlyphPixels += area;
			return &inserted.first->second;
		}
		catch (...) {}
	}
	scratch = std::move(staged);
	return &scratch;
}

template <typename Consumer>
void ForEachCodePoint(STR16 text, Consumer&& consumer)
{
	if (!text) return;
	const std::wstring_view view(text);
	std::size_t offset = 0;
	while (offset < view.size())
	{
		const portable_font::DecodedCodePoint decoded =
			portable_font::DecodeNext(view, offset);
		if (decoded.units == 0) break;
		offset += decoded.units;
		consumer(decoded.value, offset < view.size()
			? portable_font::DecodeNext(view, offset).value : 0U);
	}
}

PIXEL BlendPixel(PIXEL destination, PIXEL source, std::uint8_t coverage) noexcept
{
#if SGP_PIXEL_DEPTH == 32
	const std::uint8_t da = static_cast<std::uint8_t>(destination >> 24U);
	const std::uint8_t dr = static_cast<std::uint8_t>(destination >> 16U);
	const std::uint8_t dg = static_cast<std::uint8_t>(destination >> 8U);
	const std::uint8_t db = static_cast<std::uint8_t>(destination);
	const std::uint8_t sr = static_cast<std::uint8_t>(source >> 16U);
	const std::uint8_t sg = static_cast<std::uint8_t>(source >> 8U);
	const std::uint8_t sb = static_cast<std::uint8_t>(source);
	const std::uint8_t oa = static_cast<std::uint8_t>(
		coverage + (static_cast<std::uint32_t>(da) * (255U - coverage) + 127U) / 255U);
	return (static_cast<PIXEL>(oa) << 24U) |
		(static_cast<PIXEL>(portable_font::BlendChannel(dr, sr, coverage)) << 16U) |
		(static_cast<PIXEL>(portable_font::BlendChannel(dg, sg, coverage)) << 8U) |
		static_cast<PIXEL>(portable_font::BlendChannel(db, sb, coverage));
#else
	const std::uint8_t dr = static_cast<std::uint8_t>(
		((destination >> 11U) & 0x1FU) * 255U / 31U);
	const std::uint8_t dg = static_cast<std::uint8_t>(
		((destination >> 5U) & 0x3FU) * 255U / 63U);
	const std::uint8_t db = static_cast<std::uint8_t>(
		(destination & 0x1FU) * 255U / 31U);
	const std::uint8_t sr = static_cast<std::uint8_t>(
		((source >> 11U) & 0x1FU) * 255U / 31U);
	const std::uint8_t sg = static_cast<std::uint8_t>(
		((source >> 5U) & 0x3FU) * 255U / 63U);
	const std::uint8_t sb = static_cast<std::uint8_t>(
		(source & 0x1FU) * 255U / 31U);
	const std::uint16_t r = portable_font::BlendChannel(dr, sr, coverage);
	const std::uint16_t g = portable_font::BlendChannel(dg, sg, coverage);
	const std::uint16_t b = portable_font::BlendChannel(db, sb, coverage);
	return static_cast<PIXEL>(((r >> 3U) << 11U) |
		((g >> 2U) << 5U) | (b >> 3U));
#endif
}

void BlendGlyph(PIXEL* pixels, UINT32 pitchPixels, UINT16 surfaceWidth,
	UINT16 surfaceHeight, std::int64_t targetX, std::int64_t targetY,
	int bitmapWidth,
	int bitmapHeight, const std::vector<std::uint8_t>& bitmap, PIXEL color)
{
	for (int row = 0; row < bitmapHeight; ++row)
	{
		const std::int64_t y = static_cast<std::int64_t>(targetY) + row;
		if (y < 0 || y >= surfaceHeight) continue;
		for (int column = 0; column < bitmapWidth; ++column)
		{
			const std::int64_t x = static_cast<std::int64_t>(targetX) + column;
			if (x < 0 || x >= surfaceWidth) continue;
			const std::uint8_t coverage = bitmap[
				static_cast<std::size_t>(row) * bitmapWidth + column];
			if (coverage == 0) continue;
			PIXEL& destination = pixels[
				static_cast<std::size_t>(y) * pitchPixels +
				static_cast<std::size_t>(x)];
			destination = BlendPixel(destination, color, coverage);
		}
	}
}

bool IsLineBreak(std::uint32_t codePoint) noexcept
{
	return codePoint == static_cast<std::uint32_t>('\r') ||
		codePoint == static_cast<std::uint32_t>('\n');
}

INT16 ClampFontMetric(double metric) noexcept
{
	if (!std::isfinite(metric) ||
		metric >= static_cast<double>(std::numeric_limits<INT16>::max()))
		return std::numeric_limits<INT16>::max();
	if (metric <= 1.0) return 1;
	return static_cast<INT16>(std::ceil(metric));
}

void ReleaseFontBytesIfUnused()
{
	for (const PortableWinFont& font : Fonts)
		if (font.active) return;
	RegularBytes.reset();
	BoldBytes.reset();
}

void AssignMap(INT32 legacyFont, INT32 portableFont)
{
	if (legacyFont >= 0 && legacyFont < MAX_WINFONTMAP)
		WinFontMap[legacyFont] = portableFont;
}
}

INT32 WinFontMap[MAX_WINFONTMAP] = {
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1};
INT32 TOOLTIP_IFONT = -1;
INT32 TOOLTIP_IFONT_BOLD = -1;

INT32 CreateWinFont(const WinFontDescriptor& descriptor)
{
	if (!EnsureFontBytes()) return -1;
	const INT32 handle = FindFreeFont();
	if (handle < 0) return -1;
	try
	{
		const bool bold = descriptor.bold != FALSE;
		const std::shared_ptr<const FontBytes> storage =
			bold && BoldBytes ? BoldBytes : RegularBytes;
		if (!storage) return -1;
		PortableWinFont staged;
		staged.storage = storage;
		if (!stbtt_InitFont(&staged.info, storage->bytes.data(), storage->offset))
			return -1;
		const int pixelHeight = portable_font::ClampPixelHeight(
			descriptor.pixelHeight, 0);
		staged.scale = stbtt_ScaleForPixelHeight(
			&staged.info, static_cast<float>(pixelHeight));
		if (!(staged.scale > 0.0F) || !std::isfinite(staged.scale)) return -1;
		int ascent = 0;
		int descent = 0;
		int lineGap = 0;
		stbtt_GetFontVMetrics(&staged.info, &ascent, &descent, &lineGap);
		staged.ascent = ClampFontMetric(
			static_cast<double>(ascent) * staged.scale);
		staged.height = staged.ascent;
		const std::int64_t lineUnits = static_cast<std::int64_t>(ascent) -
			descent + lineGap;
		staged.lineHeight = ClampFontMetric(
			static_cast<double>(lineUnits) * staged.scale);
		staged.embolden = bold &&
			(BoldBytes == RegularBytes ||
			 BoldBytes->source == RegularBytes->source);
		staged.glyphs.reserve(128);
		staged.active = true;
		Fonts[static_cast<std::size_t>(handle)] = std::move(staged);
		return handle;
	}
	catch (...) { return -1; }
}

void DeleteWinFont(INT32 handle)
{
	PortableWinFont* font = GetFont(handle);
	if (!font) return;
	*font = PortableWinFont{};
	ReleaseFontBytesIfUnused();
}

void SetWinFontForeColor(INT32 handle, const COLORVAL* color)
{
	PortableWinFont* font = GetFont(handle);
	if (font && color) font->foreground = *color;
}

void SetWinFontBackColor(INT32 handle, const COLORVAL* color)
{
	PortableWinFont* font = GetFont(handle);
	if (font && color) font->background = *color;
}

BOOLEAN IsWinFontReady(INT32 handle)
{
	return GetFont(handle) ? TRUE : FALSE;
}

BOOLEAN IsWinFontBackendAvailable()
{
	return RegularBytes ? TRUE : FALSE;
}

BOOLEAN InitWinFonts()
{
	if (MainInitialized) return TRUE;
	MainHandles.fill(-1);
	std::fill(std::begin(WinFontMap), std::end(WinFontMap), -1);
	try
	{
		vfs::PropertyContainer properties;
		properties.initFromIniFile(GAME_INI_FILE);
		if (!EnsureFontBytes()) return FALSE;
		for (std::size_t index = 0; index < Catalogue.size(); ++index)
		{
			const CatalogueFont& source = Catalogue[index];
			const INT32 configuredHeight = static_cast<INT32>(
				properties.getIntProperty(
					source.section, L"Height", source.defaultHeight, -512, 512));
			const INT32 configuredWeight = static_cast<INT32>(
				properties.getIntProperty(
					source.section, L"Weight", source.defaultWeight, 1, 1000));
			const WinFontDescriptor descriptor{
				portable_font::ClampPixelHeight(configuredHeight, FontAdjustment),
				static_cast<BOOLEAN>(
					configuredWeight >= 600 ? TRUE : FALSE)};
			const INT32 handle = CreateWinFont(descriptor);
			if (handle < 0)
			{
				for (INT32 created : MainHandles) DeleteWinFont(created);
				MainHandles.fill(-1);
				ReleaseFontBytesIfUnused();
				return FALSE;
			}
			MainHandles[index] = handle;
			SetWinFontForeColor(handle, &source.color);
		}

		AssignMap(FONT12ARIAL, MainHandles[WIN_12POINTARIAL]);
		AssignMap(LARGEFONT1, MainHandles[WIN_LARGEFONT1]);
		AssignMap(SMALLFONT1, MainHandles[WIN_SMALLFONT1]);
		AssignMap(TINYFONT1, MainHandles[WIN_TINYFONT1]);
		AssignMap(FONT12POINT1, MainHandles[WIN_12POINTFONT1]);
		AssignMap(COMPFONT, MainHandles[WIN_COMPFONT]);
		AssignMap(SMALLCOMPFONT, MainHandles[WIN_SMALLCOMPFONT]);
		AssignMap(FONT10ROMAN, MainHandles[WIN_10POINTROMAN]);
		AssignMap(FONT12ROMAN, MainHandles[WIN_12POINTROMAN]);
		AssignMap(FONT14SANSERIF, MainHandles[WIN_14POINTSANSSERIF]);
		AssignMap(MILITARYFONT1, MainHandles[WIN_BLOCKYFONT]);
		AssignMap(FONT10ARIAL, MainHandles[WIN_10POINTARIAL]);
		AssignMap(FONT14ARIAL, MainHandles[WIN_14POINTARIAL]);
		AssignMap(FONT10ARIALBOLD, MainHandles[WIN_10POINTARIALBOLD]);
		AssignMap(BLOCKFONT, MainHandles[WIN_BLOCKYFONT]);
		AssignMap(BLOCKFONT2, MainHandles[WIN_BLOCKYFONT2]);
		AssignMap(FONT12ARIALFIXEDWIDTH,
			MainHandles[WIN_12POINTARIALFIXEDFONT]);
		AssignMap(FONT16ARIAL, MainHandles[WIN_16POINTARIAL]);
		AssignMap(BLOCKFONTNARROW, MainHandles[WIN_BLOCKFONTNARROW]);
		AssignMap(FONT14HUMANIST, MainHandles[WIN_14POINTHUMANIST]);
		MainInitialized = true;
		return TRUE;
	}
	catch (...)
	{
		for (INT32 handle : MainHandles) DeleteWinFont(handle);
		MainHandles.fill(-1);
		std::fill(std::begin(WinFontMap), std::end(WinFontMap), -1);
		ReleaseFontBytesIfUnused();
		return FALSE;
	}
}

void ShutdownWinFonts()
{
	for (INT32 handle : MainHandles) DeleteWinFont(handle);
	MainHandles.fill(-1);
	std::fill(std::begin(WinFontMap), std::end(WinFontMap), -1);
	MainInitialized = false;
	ReleaseFontBytesIfUnused();
}

BOOLEAN InitTooltipFonts()
{
	if (IsWinFontReady(TOOLTIP_IFONT) &&
		IsWinFontReady(TOOLTIP_IFONT_BOLD))
		return TRUE;
	ShutdownTooltipFonts();
	const INT32 height = portable_font::ClampScaledPixelHeight(
		11, static_cast<double>(fTooltipScaleFactor));
	TOOLTIP_IFONT = CreateWinFont({height, FALSE});
	TOOLTIP_IFONT_BOLD = CreateWinFont({height, TRUE});
	if (!IsWinFontReady(TOOLTIP_IFONT) ||
		!IsWinFontReady(TOOLTIP_IFONT_BOLD))
	{
		ShutdownTooltipFonts();
		return FALSE;
	}
	const COLORVAL regularColor = FROMRGB(201, 197, 143);
	const COLORVAL boldColor = FROMRGB(223, 176, 1);
	SetWinFontForeColor(TOOLTIP_IFONT, &regularColor);
	SetWinFontForeColor(TOOLTIP_IFONT_BOLD, &boldColor);
	return TRUE;
}

void ShutdownTooltipFonts()
{
	DeleteWinFont(TOOLTIP_IFONT);
	DeleteWinFont(TOOLTIP_IFONT_BOLD);
	TOOLTIP_IFONT = -1;
	TOOLTIP_IFONT_BOLD = -1;
	ReleaseFontBytesIfUnused();
}

INT16 WinFontStringPixLength(STR16 text, INT32 handle)
{
	PortableWinFont* font = GetFont(handle);
	if (!font || !text) return 0;
	int currentLine = 0;
	int maximumLine = 0;
	ForEachCodePoint(text, [&](std::uint32_t requested, std::uint32_t requestedNext)
	{
		if (requested == static_cast<std::uint32_t>('\r') &&
			requestedNext == static_cast<std::uint32_t>('\n'))
			return;
		if (IsLineBreak(requested))
		{
			maximumLine = std::max(maximumLine, currentLine);
			currentLine = 0;
			return;
		}
		const std::uint32_t current = GlyphCodePoint(*font, requested);
		const std::uint32_t next = requestedNext == 0 ||
			IsLineBreak(requestedNext) ? 0 :
			GlyphCodePoint(*font, requestedNext);
		currentLine = portable_font::SaturatingPixelAdd(
			currentLine, GlyphAdvance(*font, current, next));
	});
	maximumLine = std::max(maximumLine, currentLine);
	return static_cast<INT16>(maximumLine);
}

INT16 GetWinFontHeight(INT32 handle)
{
	const PortableWinFont* font = GetFont(handle);
	if (!font) return 0;
	if (g_lang == i18n::Lang::zh &&
		(handle == MainHandles[WIN_TINYFONT1] ||
		 handle == MainHandles[WIN_SMALLFONT1] ||
		 handle == MainHandles[WIN_14POINTARIAL]))
	{
		const int adjusted = static_cast<int>(font->height) + 2;
		return adjusted > std::numeric_limits<INT16>::max()
			? std::numeric_limits<INT16>::max()
			: static_cast<INT16>(adjusted);
	}
	return font->height;
}

BOOLEAN PrintWinFont(
	UINT32 destinationSurface, INT32 handle, INT32 x, INT32 y, STR16 text)
{
	PortableWinFont* font = GetFont(handle);
	if (!font || !text) return FALSE;
	HVSURFACE surface = nullptr;
	if (!GetVideoSurface(&surface, destinationSurface) || !surface ||
		surface->ubBitDepth <= 8)
		return FALSE;
	// VSurface retains the legacy 16-bit *content* tag for every renderable
	// surface even when its backing storage is the shipped 32-bit PIXEL type.
	// The pitch contract below is the authoritative native-storage check;
	// only genuinely indexed surfaces are ineligible for direct composition.
	UINT32 pitchBytes = 0;
	BYTE* mapped = LockVideoSurface(destinationSurface, &pitchBytes);
	if (!mapped || pitchBytes < static_cast<UINT32>(surface->usWidth) * sizeof(PIXEL) ||
		pitchBytes % sizeof(PIXEL) != 0)
	{
		if (mapped) UnLockVideoSurface(destinationSurface);
		return FALSE;
	}

	bool success = true;
	try
	{
		PIXEL* pixels = reinterpret_cast<PIXEL*>(mapped);
		const UINT32 pitchPixels = pitchBytes / sizeof(PIXEL);
		const PIXEL color = Get16BPPColor(font->foreground);
		std::int64_t penX = x;
		std::int64_t baseline =
			static_cast<std::int64_t>(y) + font->ascent;
		ForEachCodePoint(text,
			[&](std::uint32_t requested, std::uint32_t requestedNext)
		{
			if (!success) return;
			if (requested == static_cast<std::uint32_t>('\r') &&
				requestedNext == static_cast<std::uint32_t>('\n'))
				return;
			if (IsLineBreak(requested))
			{
				penX = x;
				baseline += font->lineHeight;
				return;
			}
			const std::uint32_t current = GlyphCodePoint(*font, requested);
			const std::uint32_t next = requestedNext == 0 ||
				IsLineBreak(requestedNext) ? 0 :
				GlyphCodePoint(*font, requestedNext);
			RasterizedGlyph scratch;
			const RasterizedGlyph* glyph =
				RasterizeGlyph(*font, current, scratch);
			if (!glyph)
			{
				success = false;
				return;
			}
			if (!glyph->coverage.empty())
			{
				BlendGlyph(pixels, pitchPixels, surface->usWidth, surface->usHeight,
					penX + glyph->x0, baseline + glyph->y0, glyph->width,
					glyph->height, glyph->coverage, color);
				if (font->embolden)
					BlendGlyph(pixels, pitchPixels, surface->usWidth,
						surface->usHeight, penX + glyph->x0 + 1,
						baseline + glyph->y0, glyph->width, glyph->height,
						glyph->coverage, color);
			}
			penX += GlyphAdvance(*font, current, next);
		});
	}
	catch (...) { success = false; }
	UnLockVideoSurface(destinationSurface);
	return success ? TRUE : FALSE;
}
