#include "vobject_blitters.h"

#include <cstddef>
#include <cstdint>
#include <limits>

extern BOOLEAN gfUsePreCalcSkips;

namespace
{
INT32 SaturatedSkip(std::int64_t requested, INT32 extent)
{
	if (requested <= 0) return 0;
	if (requested >= extent) return extent;
	return static_cast<INT32>(requested);
}

bool CalculateSkips(
	INT32 originX,
	INT32 originY,
	INT32 width,
	INT32 height,
	const SGPRect& clipping,
	INT32& left,
	INT32& right,
	INT32& top,
	INT32& bottom)
{
	if (width <= 0 || height <= 0)
		return false;

	const std::int64_t imageRight =
		static_cast<std::int64_t>(originX) + width;
	const std::int64_t imageBottom =
		static_cast<std::int64_t>(originY) + height;
	left = SaturatedSkip(
		static_cast<std::int64_t>(clipping.iLeft) - originX,
		width);
	right = SaturatedSkip(
		imageRight - clipping.iRight,
		width);
	top = SaturatedSkip(
		static_cast<std::int64_t>(clipping.iTop) - originY,
		height);
	bottom = SaturatedSkip(
		imageBottom - clipping.iBottom,
		height);
	return true;
}

bool ValidSkips(
	INT32 left,
	INT32 right,
	INT32 top,
	INT32 bottom,
	INT32 width,
	INT32 height)
{
	return left >= 0 && right >= 0 && top >= 0 && bottom >= 0 &&
		left <= width && right <= width &&
		top <= height && bottom <= height;
}

enum class PrepareResult
{
	Invalid,
	Empty,
	Ready
};

struct NativeBlitRegion
{
	const NativePixelObjectInfo* image = nullptr;
	std::size_t sourceX = 0;
	std::size_t sourceY = 0;
	std::size_t width = 0;
	std::size_t height = 0;
	UINT8* destinationRowBytes = nullptr;
};

PrepareResult PrepareNativeBlit(
	PIXEL* buffer,
	UINT32 destinationPitchBytes,
	HVOBJECT sourceObject,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 imageIndex,
	NativePixelObjectStorage storage,
	const SGPRect* clippingRegion,
	NativeBlitRegion& prepared)
{
	if (!buffer || !sourceObject ||
		imageIndex >= sourceObject->usNumberOfNativePixelObjects ||
		!sourceObject->pNativePixelObject ||
		destinationPitchBytes == 0 ||
		destinationPitchBytes % sizeof(PIXEL) != 0)
		return PrepareResult::Invalid;

	const NativePixelObjectInfo& image =
		sourceObject->pNativePixelObject[imageIndex];
	if (image.storage != storage || !image.pNativePixels ||
		image.usWidth == 0 || image.usHeight == 0)
		return PrepareResult::Invalid;

	const std::int64_t originX64 =
		static_cast<std::int64_t>(destinationX) + image.sOffsetX;
	const std::int64_t originY64 =
		static_cast<std::int64_t>(destinationY) + image.sOffsetY;
	if (originX64 < std::numeric_limits<INT32>::min() ||
		originX64 > std::numeric_limits<INT32>::max() ||
		originY64 < std::numeric_limits<INT32>::min() ||
		originY64 > std::numeric_limits<INT32>::max())
		return PrepareResult::Invalid;
	const INT32 originX = static_cast<INT32>(originX64);
	const INT32 originY = static_cast<INT32>(originY64);

	INT32 left = 0;
	INT32 right = 0;
	INT32 top = 0;
	INT32 bottom = 0;
	// Older ETRLE callers exposed one process-global set of precomputed skips.
	// Recompute from this image and clip instead: a rejected/off-screen draw can
	// otherwise leak stale geometry into the next unrelated native image.
	gfUsePreCalcSkips = FALSE;
	const SGPRect& clipping =
		clippingRegion ? *clippingRegion : ClippingRect;
	if (!CalculateSkips(
			originX, originY,
			image.usWidth, image.usHeight,
			clipping, left, right, top, bottom))
		return PrepareResult::Invalid;

	if (!ValidSkips(
			left, right, top, bottom,
			image.usWidth, image.usHeight))
		return PrepareResult::Invalid;
	const INT32 blitWidth =
		static_cast<INT32>(image.usWidth) - left - right;
	const INT32 blitHeight =
		static_cast<INT32>(image.usHeight) - top - bottom;
	if (blitWidth <= 0 || blitHeight <= 0)
		return PrepareResult::Empty;

	const std::int64_t firstDestinationX64 =
		static_cast<std::int64_t>(originX) + left;
	const std::int64_t firstDestinationY64 =
		static_cast<std::int64_t>(originY) + top;
	if (firstDestinationX64 < 0 || firstDestinationY64 < 0)
		return PrepareResult::Invalid;

	const std::size_t firstDestinationX =
		static_cast<std::size_t>(firstDestinationX64);
	const std::size_t firstDestinationY =
		static_cast<std::size_t>(firstDestinationY64);
	const std::size_t destinationPitchPixels =
		destinationPitchBytes / sizeof(PIXEL);
	if (firstDestinationX >= destinationPitchPixels ||
		static_cast<std::size_t>(blitWidth) >
			destinationPitchPixels - firstDestinationX)
		return PrepareResult::Invalid;

	const std::size_t additionalRows =
		static_cast<std::size_t>(blitHeight - 1);
	if (additionalRows >
		std::numeric_limits<std::size_t>::max() - firstDestinationY)
		return PrepareResult::Invalid;
	const std::size_t lastDestinationY =
		firstDestinationY + additionalRows;
	if (lastDestinationY >
		std::numeric_limits<std::size_t>::max() /
			destinationPitchBytes)
		return PrepareResult::Invalid;
	const std::size_t lastRowOffset =
		lastDestinationY * destinationPitchBytes;
	if (firstDestinationX >
		(std::numeric_limits<std::size_t>::max() - lastRowOffset) /
			sizeof(PIXEL))
		return PrepareResult::Invalid;

	prepared.image = &image;
	prepared.sourceX = static_cast<std::size_t>(left);
	prepared.sourceY = static_cast<std::size_t>(top);
	prepared.width = static_cast<std::size_t>(blitWidth);
	prepared.height = static_cast<std::size_t>(blitHeight);
	prepared.destinationRowBytes =
		reinterpret_cast<UINT8*>(buffer) +
		firstDestinationY * destinationPitchBytes +
		firstDestinationX * sizeof(PIXEL);
	return PrepareResult::Ready;
}

UINT8 BlendChannel(UINT8 source, UINT8 destination, UINT8 opacity)
{
	return static_cast<UINT8>(
		(static_cast<UINT32>(255 - opacity) * destination) / 255 +
		(static_cast<UINT32>(opacity) * source) / 255);
}

PIXEL OpaqueNativePixel(PIXEL pixel)
{
#if SGP_PIXEL_DEPTH == 32
	return pixel | 0xFF000000u;
#else
	return pixel;
#endif
}

PIXEL BlendNativePixel(PIXEL source, PIXEL destination, UINT8 opacity)
{
	if (opacity == 0) return destination;
	if (opacity == 255) return OpaqueNativePixel(source);

#if SGP_PIXEL_DEPTH == 32
	const UINT8 red = BlendChannel(
		static_cast<UINT8>(source >> 16),
		static_cast<UINT8>(destination >> 16), opacity);
	const UINT8 green = BlendChannel(
		static_cast<UINT8>(source >> 8),
		static_cast<UINT8>(destination >> 8), opacity);
	const UINT8 blue = BlendChannel(
		static_cast<UINT8>(source),
		static_cast<UINT8>(destination), opacity);
	return 0xFF000000u |
		(static_cast<UINT32>(red) << 16) |
		(static_cast<UINT32>(green) << 8) |
		blue;
#else
	const UINT16 sourceToken = PixToColor16(source);
	const UINT16 destinationToken = PixToColor16(destination);
	const UINT8 sourceRed = static_cast<UINT8>(
		((sourceToken >> 11) & 0x1Fu) * 255 / 31);
	const UINT8 sourceGreen = static_cast<UINT8>(
		((sourceToken >> 5) & 0x3Fu) * 255 / 63);
	const UINT8 sourceBlue = static_cast<UINT8>(
		(sourceToken & 0x1Fu) * 255 / 31);
	const UINT8 destinationRed = static_cast<UINT8>(
		((destinationToken >> 11) & 0x1Fu) * 255 / 31);
	const UINT8 destinationGreen = static_cast<UINT8>(
		((destinationToken >> 5) & 0x3Fu) * 255 / 63);
	const UINT8 destinationBlue = static_cast<UINT8>(
		(destinationToken & 0x1Fu) * 255 / 31);
	const UINT16 blended = static_cast<UINT16>(
		(static_cast<UINT16>(
			BlendChannel(sourceRed, destinationRed, opacity) >> 3) << 11) |
		(static_cast<UINT16>(
			BlendChannel(sourceGreen, destinationGreen, opacity) >> 2) << 5) |
		static_cast<UINT16>(
			BlendChannel(sourceBlue, destinationBlue, opacity) >> 3));
	return PixFromColor16(blended);
#endif
}
}

BOOLEAN BltNativePixelDataToBufferTransparentClip(
	PIXEL* buffer,
	UINT32 destinationPitchBytes,
	HVOBJECT sourceObject,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 cacheIndex,
	SGPRect* clippingRegion)
{
	NativeBlitRegion prepared;
	const PrepareResult result = PrepareNativeBlit(
		buffer, destinationPitchBytes, sourceObject,
		destinationX, destinationY, cacheIndex,
		NativePixelObjectStorage::MaskedSprite,
		clippingRegion, prepared);
	if (result == PrepareResult::Invalid) return FALSE;
	if (result == PrepareResult::Empty) return TRUE;
	if (!prepared.image->pNativeOpacity) return FALSE;

	for (std::size_t row = 0; row < prepared.height; ++row)
	{
		const std::size_t sourceOffset =
			(prepared.sourceY + row) * prepared.image->usWidth +
			prepared.sourceX;
		const PIXEL* const source =
			prepared.image->pNativePixels + sourceOffset;
		const UINT8* const opacity =
			prepared.image->pNativeOpacity + sourceOffset;
		PIXEL* const destination =
			reinterpret_cast<PIXEL*>(prepared.destinationRowBytes);
		for (std::size_t column = 0;
			column < prepared.width; ++column)
		{
			if (opacity[column]) destination[column] = source[column];
		}
		prepared.destinationRowBytes += destinationPitchBytes;
	}
	return TRUE;
}

BOOLEAN Blt16BPPDataTo16BPPBufferTransparentClip(
	PIXEL* buffer,
	UINT32 destinationPitchBytes,
	HVOBJECT sourceObject,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 cacheIndex,
	SGPRect* clippingRegion)
{
	return BltNativePixelDataToBufferTransparentClip(
		buffer, destinationPitchBytes, sourceObject,
		destinationX, destinationY, cacheIndex, clippingRegion);
}

BOOLEAN BltNativePixelImageToBufferClip(
	PIXEL* buffer,
	UINT32 destinationPitchBytes,
	HVOBJECT sourceObject,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 imageIndex,
	BOOLEAN sourceTransparency,
	BOOLEAN shadow,
	const SGPRect* clippingRegion)
{
	if (!sourceObject ||
		(sourceObject->ubBitDepth != 16 &&
			sourceObject->ubBitDepth != 32))
		return FALSE;

	NativeBlitRegion prepared;
	const PrepareResult result = PrepareNativeBlit(
		buffer, destinationPitchBytes, sourceObject,
		destinationX, destinationY, imageIndex,
		NativePixelObjectStorage::LinearPixels,
		clippingRegion, prepared);
	if (result == PrepareResult::Invalid) return FALSE;
	if (result == PrepareResult::Empty) return TRUE;
	if (sourceObject->ubBitDepth == 32 &&
		!prepared.image->pNativeOpacity)
		return FALSE;

	const PIXEL transparencyKey = PixFromColor16(0x001Fu);
	for (std::size_t row = 0; row < prepared.height; ++row)
	{
		const std::size_t sourceOffset =
			(prepared.sourceY + row) * prepared.image->usWidth +
			prepared.sourceX;
		const PIXEL* const source =
			prepared.image->pNativePixels + sourceOffset;
		const UINT8* const opacity =
			prepared.image->pNativeOpacity ?
				prepared.image->pNativeOpacity + sourceOffset :
				nullptr;
		PIXEL* const destination =
			reinterpret_cast<PIXEL*>(prepared.destinationRowBytes);
		for (std::size_t column = 0;
			column < prepared.width; ++column)
		{
			UINT8 pixelOpacity = 255;
			if (sourceObject->ubBitDepth == 32)
			{
				pixelOpacity = opacity[column];
			}
			else if ((sourceTransparency || shadow) &&
				source[column] == transparencyKey)
			{
				pixelOpacity = 0;
			}
			if (pixelOpacity == 0) continue;

			const PIXEL sourcePixel = shadow ?
				PixShade(destination[column]) : source[column];
			destination[column] = BlendNativePixel(
				sourcePixel, destination[column], pixelOpacity);
		}
		prepared.destinationRowBytes += destinationPitchBytes;
	}
	return TRUE;
}
