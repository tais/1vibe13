#include "vobject.h"

#include "MemMan.h"

#include <cstddef>
#include <cstring>
#include <limits>

namespace
{
bool DecodeIndexedEtrleRegion(
	const UINT8* source,
	std::size_t sourceLength,
	UINT16 width,
	UINT16 height,
	const PIXEL* palette,
	PIXEL* pixels,
	UINT8* transparencyMask)
{
	if (!source || sourceLength == 0 || width == 0 || height == 0 ||
		!palette || !pixels || !transparencyMask)
		return false;

	const std::size_t pixelCount =
		static_cast<std::size_t>(width) * height;
	std::memset(pixels, 0, pixelCount * sizeof(PIXEL));
	std::memset(transparencyMask, 0, pixelCount);

	std::size_t sourceOffset = 0;
	for (std::size_t y = 0; y < height; ++y)
	{
		std::size_t x = 0;
		bool reachedEndOfRow = false;
		while (sourceOffset < sourceLength)
		{
			const UINT8 command = source[sourceOffset++];
			if (command == 0)
			{
				reachedEndOfRow = true;
				break;
			}

			const std::size_t runLength = command & 0x7Fu;
			if (runLength == 0 || runLength > width - x)
				return false;

			if (command & 0x80u)
			{
				x += runLength;
				continue;
			}

			if (runLength > sourceLength - sourceOffset)
				return false;
			for (std::size_t run = 0; run < runLength; ++run, ++x)
			{
				const std::size_t destination =
					y * static_cast<std::size_t>(width) + x;
				pixels[destination] = palette[source[sourceOffset++]];
				transparencyMask[destination] = 1;
			}
		}
		if (!reachedEndOfRow || x != width)
			return false;
	}

	// uiDataLength describes exactly one ETRLE region. Rejecting trailing bytes
	// prevents a malformed cache from hiding a second row stream past the
	// dimensions that bound its native allocations.
	return sourceOffset == sourceLength;
}
}

BOOLEAN FindCachedVObjectNativePixelRegion(
	HVOBJECT object,
	UINT16 regionIndex,
	UINT8 shadeLevel,
	UINT16* cacheIndex)
{
	if (!object || !object->pNativePixelObject)
		return FALSE;

	for (UINT16 index = 0;
		index < object->usNumberOfNativePixelObjects; ++index)
	{
		const NativePixelObjectInfo& cached =
			object->pNativePixelObject[index];
		if (cached.storage == NativePixelObjectStorage::MaskedSprite &&
			cached.usRegionIndex == regionIndex &&
			cached.ubShadeLevel == shadeLevel)
		{
			if (cacheIndex) *cacheIndex = index;
			return TRUE;
		}
	}
	return FALSE;
}

BOOLEAN CacheVObjectRegionNativePixels(
	HVOBJECT object,
	UINT16 regionIndex,
	UINT8 shadeLevel)
{
	if (!object || object->ubBitDepth != 8 || !object->pETRLEObject ||
		!object->pPixData || regionIndex >= object->usNumberOfObjects ||
		shadeLevel >= HVOBJECT_SHADE_TABLES || !object->pShades[shadeLevel] ||
		object->usNumberOfNativePixelObjects ==
			std::numeric_limits<UINT16>::max())
		return FALSE;

	if (FindCachedVObjectNativePixelRegion(
			object, regionIndex, shadeLevel, nullptr))
		return TRUE;

	const ETRLEObject& region = object->pETRLEObject[regionIndex];
	if (region.usWidth == 0 || region.usHeight == 0 ||
		region.uiDataOffset > object->uiSizePixData ||
		region.uiDataLength >
			object->uiSizePixData - region.uiDataOffset)
		return FALSE;

	const std::size_t pixelCount =
		static_cast<std::size_t>(region.usWidth) * region.usHeight;
	if (pixelCount >
			std::numeric_limits<UINT32>::max() / sizeof(PIXEL))
		return FALSE;

	PIXEL* const pixels = static_cast<PIXEL*>(
		MemAlloc(static_cast<UINT32>(pixelCount * sizeof(PIXEL))));
	UINT8* const transparencyMask = static_cast<UINT8*>(
		MemAlloc(static_cast<UINT32>(pixelCount)));
	if (!pixels || !transparencyMask)
	{
		if (pixels) MemFree(pixels);
		if (transparencyMask) MemFree(transparencyMask);
		return FALSE;
	}

	const UINT8* const source =
		static_cast<const UINT8*>(object->pPixData) + region.uiDataOffset;
	if (!DecodeIndexedEtrleRegion(
			source, region.uiDataLength,
			region.usWidth, region.usHeight,
			object->pShades[shadeLevel], pixels, transparencyMask))
	{
		MemFree(transparencyMask);
		MemFree(pixels);
		return FALSE;
	}

	const std::size_t newCount =
		static_cast<std::size_t>(
			object->usNumberOfNativePixelObjects) + 1;
	const std::size_t allocationSize =
		newCount * sizeof(NativePixelObjectInfo);
	if (allocationSize > std::numeric_limits<UINT32>::max())
	{
		MemFree(transparencyMask);
		MemFree(pixels);
		return FALSE;
	}

	NativePixelObjectInfo* expanded = nullptr;
	if (object->pNativePixelObject)
	{
		expanded = static_cast<NativePixelObjectInfo*>(
			MemRealloc(
				object->pNativePixelObject,
				static_cast<UINT32>(allocationSize)));
	}
	else
	{
		expanded = static_cast<NativePixelObjectInfo*>(
			MemAlloc(static_cast<UINT32>(allocationSize)));
	}
	if (!expanded)
	{
		MemFree(transparencyMask);
		MemFree(pixels);
		return FALSE;
	}

	object->pNativePixelObject = expanded;
	NativePixelObjectInfo cached{};
	cached.pNativePixels = pixels;
	cached.pNativeTransparencyMask = transparencyMask;
	cached.usRegionIndex = regionIndex;
	cached.ubShadeLevel = shadeLevel;
	cached.usWidth = region.usWidth;
	cached.usHeight = region.usHeight;
	cached.sOffsetX = region.sOffsetX;
	cached.sOffsetY = region.sOffsetY;
	cached.storage = NativePixelObjectStorage::MaskedSprite;
	object->pNativePixelObject[
		object->usNumberOfNativePixelObjects] = cached;
	++object->usNumberOfNativePixelObjects;
	return TRUE;
}

BOOLEAN CheckFor16BPPRegion(
	HVOBJECT object,
	UINT16 regionIndex,
	UINT8 shadeLevel,
	UINT16* cacheIndex)
{
	return FindCachedVObjectNativePixelRegion(
		object, regionIndex, shadeLevel, cacheIndex);
}

BOOLEAN ConvertVObjectRegionTo16BPP(
	HVOBJECT object,
	UINT16 regionIndex,
	UINT8 shadeLevel)
{
	return CacheVObjectRegionNativePixels(
		object, regionIndex, shadeLevel);
}
