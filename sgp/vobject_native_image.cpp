#include "vobject_native_image.h"

#include "MemMan.h"

#include <cstddef>
#include <cstring>
#include <limits>

namespace
{
INT32 gAllocationsBeforeFailure = -1;

void* Allocate(UINT32 size)
{
	if (gAllocationsBeforeFailure == 0) return nullptr;
	if (gAllocationsBeforeFailure > 0) --gAllocationsBeforeFailure;
	return MemAlloc(size);
}

void ReleaseImport(
	NativePixelObjectInfo* image,
	PIXEL* pixels,
	UINT8* opacity)
{
	if (opacity) MemFree(opacity);
	if (pixels) MemFree(pixels);
	if (image) MemFree(image);
}

PIXEL NativePixelFromRgbaBytes(const UINT8* rgba)
{
	// PngLoader preserves the decoder's RGBA byte sequence in p32BPPData.
	// Read bytes rather than relying on host word order, then normalize once
	// instead of swizzling every draw.
	const UINT8 red = rgba[0];
	const UINT8 green = rgba[1];
	const UINT8 blue = rgba[2];
	const UINT8 alpha = rgba[3];
#if SGP_PIXEL_DEPTH == 32
	return
		(static_cast<UINT32>(alpha) << 24) |
		(static_cast<UINT32>(red) << 16) |
		(static_cast<UINT32>(green) << 8) |
		blue;
#else
	const UINT16 token = static_cast<UINT16>(
		(static_cast<UINT16>(red >> 3) << 11) |
		(static_cast<UINT16>(green >> 2) << 5) |
		static_cast<UINT16>(blue >> 3));
	return PixFromColor16(token);
#endif
}
}

namespace NativeImageTestHooks
{
void FailAllocationAfter(INT32 successfulAllocations)
{
	gAllocationsBeforeFailure = successfulAllocations;
}

void ResetAllocationFailure()
{
	gAllocationsBeforeFailure = -1;
}
}

BOOLEAN ImportNativeVideoObjectImage(HVOBJECT object, HIMAGE source)
{
	if (!object || !source ||
		(source->ubBitDepth != 16 && source->ubBitDepth != 32) ||
		source->usNumberOfObjects == 0 || !source->pETRLEObject ||
		object->pNativePixelObject ||
		object->usNumberOfNativePixelObjects != 0)
		return FALSE;

	const ETRLEObject& region = source->pETRLEObject[0];
	if (region.usWidth == 0 || region.usHeight == 0)
		return FALSE;
	const std::size_t pixelCount =
		static_cast<std::size_t>(region.usWidth) * region.usHeight;
	if (pixelCount >
		std::numeric_limits<UINT32>::max() / sizeof(PIXEL))
		return FALSE;
	if ((source->usWidth != 0 && source->usHeight != 0) &&
		pixelCount >
			static_cast<std::size_t>(source->usWidth) * source->usHeight)
		return FALSE;
	if ((source->ubBitDepth == 16 && !source->p16BPPData) ||
		(source->ubBitDepth == 32 && !source->p32BPPData))
		return FALSE;

	NativePixelObjectInfo* const imported =
		static_cast<NativePixelObjectInfo*>(
			Allocate(sizeof(NativePixelObjectInfo)));
	PIXEL* pixels = nullptr;
	UINT8* opacity = nullptr;
	if (imported)
	{
		pixels = static_cast<PIXEL*>(
			Allocate(static_cast<UINT32>(
				pixelCount * sizeof(PIXEL))));
	}
	if (imported && pixels && source->ubBitDepth == 32)
	{
		opacity = static_cast<UINT8*>(
			Allocate(static_cast<UINT32>(pixelCount)));
	}
	if (!imported || !pixels ||
		(source->ubBitDepth == 32 && !opacity))
	{
		ReleaseImport(imported, pixels, opacity);
		return FALSE;
	}

	std::memset(imported, 0, sizeof(*imported));
	if (source->ubBitDepth == 16)
	{
		for (std::size_t index = 0; index < pixelCount; ++index)
			pixels[index] =
				PixFromColor16(source->p16BPPData[index]);
	}
	else
	{
		const UINT8* const sourceBytes =
			reinterpret_cast<const UINT8*>(
				source->p32BPPData);
		for (std::size_t index = 0; index < pixelCount; ++index)
		{
			const UINT8* const sourcePixel =
				sourceBytes + index * sizeof(UINT32);
			pixels[index] =
				NativePixelFromRgbaBytes(sourcePixel);
			opacity[index] = sourcePixel[3];
		}
	}

	imported->pNativePixels = pixels;
	imported->pNativeOpacity = opacity;
	imported->usRegionIndex = 0;
	imported->ubShadeLevel = 0;
	imported->usWidth = region.usWidth;
	imported->usHeight = region.usHeight;
	imported->sOffsetX = region.sOffsetX;
	imported->sOffsetY = region.sOffsetY;
	imported->storage = NativePixelObjectStorage::LinearPixels;

	object->pNativePixelObject = imported;
	object->usNumberOfNativePixelObjects = 1;
	object->ubBitDepth = source->ubBitDepth;
	std::strncpy(
		object->ImageFile, source->ImageFile,
		sizeof(object->ImageFile) - 1);
	object->ImageFile[sizeof(object->ImageFile) - 1] = '\0';
	return TRUE;
}
