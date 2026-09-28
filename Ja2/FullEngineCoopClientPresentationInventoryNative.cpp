#include "FullEngineCoopClientPresentationInventory.h"

#include "Font Control.h"
#include "Font.h"
#include "Interface Items.h"
#include "InterfaceItemImages.h"
#include "Items.h"
#include "Utilities.h"
#include "himage.h"
#include "local.h"
#include "vobject.h"
#include "vobject_blitters.h"
#include "vsurface.h"

#include <algorithm>
#include <array>
#include <limits>

namespace
{
using Rect = FullEngineCoopClientInventoryRect;
using Paint = FullEngineCoopClientInventoryPaint;

bool OnScreen(const Rect& rect) noexcept
{
	return rect.valid() && rect.right <= SCREEN_WIDTH && rect.bottom <= SCREEN_HEIGHT;
}
PIXEL Pixel(Paint paint) noexcept
{
	switch (paint)
	{
		case Paint::Background: return Get16BPPColor(FROMRGB(9, 12, 10));
		case Paint::SlotBackground: return Get16BPPColor(FROMRGB(28, 31, 26));
		case Paint::Border: return Get16BPPColor(FROMRGB(99, 92, 72));
		case Paint::Selected: return Get16BPPColor(FROMRGB(60, 255, 60));
		case Paint::Inspected: return Get16BPPColor(FROMRGB(75, 205, 255));
		case Paint::Destination: return Get16BPPColor(FROMRGB(255, 220, 55));
		case Paint::Unsupported: return Get16BPPColor(FROMRGB(240, 135, 70));
		case Paint::Text: return Get16BPPColor(FROMRGB(230, 230, 211));
		case Paint::Muted: return Get16BPPColor(FROMRGB(140, 145, 132));
	}
	return 0;
}
struct SurfaceLock
{
	UINT32 pitch = 0;
	PIXEL* pixels = reinterpret_cast<PIXEL*>(LockVideoSurface(FRAME_BUFFER, &pitch));
	~SurfaceLock() { if (pixels) UnLockVideoSurface(FRAME_BUFFER); }
	bool valid() const noexcept
	{
		return pixels && pitch % sizeof(PIXEL) == 0 &&
			static_cast<std::uint64_t>(SCREEN_WIDTH) * sizeof(PIXEL) <= pitch;
	}
};
bool Fill(const Rect& rect, Paint paint, void*) noexcept
{
	if (!OnScreen(rect)) return false;
	SurfaceLock surface;
	if (!surface.valid()) return false;
	const auto stride = surface.pitch / sizeof(PIXEL);
	const PIXEL pixel = Pixel(paint);
	for (std::int32_t y = rect.top; y < rect.bottom; ++y)
		std::fill(surface.pixels + std::size_t(y) * stride + rect.left,
			surface.pixels + std::size_t(y) * stride + rect.right, pixel);
	return true;
}
bool Border(const Rect& rect, Paint paint, void* context) noexcept
{
	if (!OnScreen(rect) || rect.right - rect.left < 2 || rect.bottom - rect.top < 2)
		return false;
	return Fill({rect.left, rect.top, rect.right, rect.top + 1}, paint, context) &&
		Fill({rect.left, rect.bottom - 1, rect.right, rect.bottom}, paint, context) &&
		Fill({rect.left, rect.top + 1, rect.left + 1, rect.bottom - 1}, paint, context) &&
		Fill({rect.right - 1, rect.top + 1, rect.right, rect.bottom - 1}, paint, context);
}
bool LoadPanel(const char* path, std::uint32_t& handle, void*) noexcept
{
	if (!path) return false;
	try
	{
		VOBJECT_DESC description{};
		description.fCreateFlags = VOBJECT_CREATE_FROMFILE;
		if (!FilenameForBPP(path, description.ImageFile)) return false;
		UINT32 candidate = 0;
		if (!AddVideoObject(&description, &candidate)) return false;
		handle = candidate;
		return true;
	}
	catch (...) { return false; }
}
void ReleasePanel(std::uint32_t handle, void*) noexcept
{
	(void)DeleteVideoObjectFromIndex(handle);
}
bool BlitPanel(std::uint32_t handle, std::uint16_t frame, const Rect& panel, void*) noexcept
{
	HVOBJECT object = nullptr;
	if (!OnScreen(panel) || !GetVideoObject(&object, handle) || !object ||
		!object->pETRLEObject || frame >= object->usNumberOfObjects) return false;
	const ETRLEObject& graphic = object->pETRLEObject[frame];
	if (!graphic.usWidth || !graphic.usHeight ||
		graphic.usWidth > panel.right - panel.left ||
		graphic.usHeight > panel.bottom - panel.top) return false;
	const SGPRect clip{panel.left, panel.top, panel.right, panel.bottom};
	return BltVideoObjectEffectToSurface(FRAME_BUFFER, object, frame,
		panel.left, panel.top, VOBJECT_DRAW_SOURCE_TRANSPARENCY, &clip) != FALSE;
}
bool Text(const std::uint16_t* text, const Rect& rect, Paint paint, void*) noexcept
{
	if (!text || !OnScreen(rect) || !gfFontsInit || TINYFONT1 < 0 ||
		TINYFONT1 >= MAX_FONTS || !IsFontLoaded(TINYFONT1)) return false;
	HVOBJECT font = GetFontObject(TINYFONT1);
	if (!font || !font->pETRLEObject || !font->usNumberOfObjects) return false;
	std::array<std::uint16_t, 160> glyphs{};
	std::array<UINT32, 160> widths{};
	std::size_t count = 0;
	for (; count < glyphs.size() && text[count]; ++count)
	{
		INT16 glyph = -1;
		if (!TryGetIndex(static_cast<CHAR16>(text[count]), &glyph) || glyph < 0 ||
			static_cast<UINT16>(glyph) >= font->usNumberOfObjects)
		{
			if (!TryGetIndex(L'?', &glyph) || glyph < 0 ||
				static_cast<UINT16>(glyph) >= font->usNumberOfObjects) return false;
		}
		glyphs[count] = static_cast<std::uint16_t>(glyph);
		widths[count] = GetWidth(font, glyph);
	}
	if (count == glyphs.size()) return false;
	SurfaceLock surface;
	if (!surface.valid()) return false;
	SGPRect clip{rect.left, rect.top, rect.right, rect.bottom};
	std::int64_t x = rect.left;
	for (std::size_t i = 0; i < count && x < rect.right; ++i)
	{
		if (!Blt8BPPDataTo16BPPBufferMonoShadowClip(surface.pixels, surface.pitch, font,
			static_cast<INT32>(x), rect.top, glyphs[i], &clip, Pixel(paint), 0, 0)) return false;
		x += widths[i];
	}
	return true;
}
bool KnownItem(std::uint16_t item) noexcept
{
	return item != 0 && item < MAXITEMS && item < gMAXITEMS_READ &&
		Item[item].ubGraphicType <= MAX_PITEMS;
}
bool ItemName(std::uint16_t item, std::uint16_t* output,
	std::size_t capacity, void*) noexcept
{
	if (!KnownItem(item) || !output || !capacity ||
		capacity > FullEngineCoopClientInventoryNameCapacity) return false;
	std::array<std::uint16_t, FullEngineCoopClientInventoryNameCapacity> bounded{};
	static_assert(sizeof(Item[0].szItemName) / sizeof(Item[0].szItemName[0]) ==
		FullEngineCoopClientInventoryNameCapacity, "native item-name capacity changed");
	for (std::size_t index = 0; index < bounded.size(); ++index)
	{
		const auto unit = static_cast<std::uint32_t>(Item[item].szItemName[index]);
		if (unit == 0) break;
		if (unit < 32 || unit == 127 || unit > 0xffff || (unit >= 0xd800 && unit <= 0xdfff))
			return false;
		bounded[index] = static_cast<std::uint16_t>(unit);
	}
	if (bounded.front() == 0) return false;
	return CopyFullEngineCoopClientInventoryDisplayName(
		bounded.data(), bounded.size(), output, capacity);
}
bool ItemGraphic(std::uint16_t item, const Rect& slot, void*) noexcept
{
	if (!KnownItem(item) || !OnScreen(slot)) return false;
	try
	{
		const UINT32 handle = GetInterfaceGraphicForItem(&Item[item]);
		HVOBJECT object = nullptr;
		if (!GetVideoObject(&object, handle) || !object || !object->pETRLEObject) return false;
		const UINT16 frame = g_bUsePngItemImages ? 0 : Item[item].ubGraphicNum;
		if (frame >= object->usNumberOfObjects) return false;
		const ETRLEObject& graphic = object->pETRLEObject[frame];
		if (!graphic.usWidth || !graphic.usHeight) return false;
		const std::int32_t x = slot.left + (slot.right - slot.left - graphic.usWidth) / 2 - graphic.sOffsetX;
		const std::int32_t y = slot.top + (slot.bottom - slot.top - graphic.usHeight) / 2 - graphic.sOffsetY;
		const SGPRect clip{slot.left, slot.top, slot.right, slot.bottom};
		return BltVideoObjectEffectToSurface(FRAME_BUFFER, object, frame,
			x, y, VOBJECT_DRAW_SOURCE_TRANSPARENCY, &clip) != FALSE;
	}
	catch (...) { return false; }
}
}

FullEngineCoopClientPresentationInventoryServices
MakeFullEngineCoopClientPresentationInventoryNativeServices() noexcept
{
	return {LoadPanel, ReleasePanel, BlitPanel, Fill, Border, Text,
		ItemGraphic, ItemName, nullptr};
}
