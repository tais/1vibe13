#include "vobject_blitters.h"

#include <cstddef>
#include <cstdint>
#include <limits>

extern INT32 gLeftSkip;
extern INT32 gRightSkip;
extern INT32 gTopSkip;
extern INT32 gBottomSkip;
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
	if (!buffer || !sourceObject ||
		cacheIndex >= sourceObject->usNumberOfNativePixelObjects ||
		!sourceObject->pNativePixelObject ||
		destinationPitchBytes == 0 ||
		destinationPitchBytes % sizeof(PIXEL) != 0)
		return FALSE;

	const NativePixelObjectInfo& image =
		sourceObject->pNativePixelObject[cacheIndex];
	if (image.storage != NativePixelObjectStorage::MaskedSprite ||
		!image.pNativePixels || !image.pNativeTransparencyMask ||
		image.usWidth == 0 || image.usHeight == 0)
		return FALSE;

	const std::int64_t originX64 =
		static_cast<std::int64_t>(destinationX) + image.sOffsetX;
	const std::int64_t originY64 =
		static_cast<std::int64_t>(destinationY) + image.sOffsetY;
	if (originX64 < std::numeric_limits<INT32>::min() ||
		originX64 > std::numeric_limits<INT32>::max() ||
		originY64 < std::numeric_limits<INT32>::min() ||
		originY64 > std::numeric_limits<INT32>::max())
		return FALSE;
	const INT32 originX = static_cast<INT32>(originX64);
	const INT32 originY = static_cast<INT32>(originY64);

	INT32 left = 0;
	INT32 right = 0;
	INT32 top = 0;
	INT32 bottom = 0;
	if (gfUsePreCalcSkips)
	{
		left = gLeftSkip;
		right = gRightSkip;
		top = gTopSkip;
		bottom = gBottomSkip;
		gfUsePreCalcSkips = FALSE;
	}
	else
	{
		const SGPRect& clipping =
			clippingRegion ? *clippingRegion : ClippingRect;
		if (!CalculateSkips(
				originX, originY,
				image.usWidth, image.usHeight,
				clipping, left, right, top, bottom))
			return FALSE;
	}

	if (!ValidSkips(
			left, right, top, bottom,
			image.usWidth, image.usHeight))
		return FALSE;
	if (left >= image.usWidth || right >= image.usWidth ||
		top >= image.usHeight || bottom >= image.usHeight)
		return TRUE;

	const INT32 blitWidth =
		static_cast<INT32>(image.usWidth) - left - right;
	const INT32 blitHeight =
		static_cast<INT32>(image.usHeight) - top - bottom;
	if (blitWidth <= 0 || blitHeight <= 0)
		return TRUE;

	const std::int64_t firstDestinationX64 =
		static_cast<std::int64_t>(originX) + left;
	const std::int64_t firstDestinationY64 =
		static_cast<std::int64_t>(originY) + top;
	if (firstDestinationX64 < 0 || firstDestinationY64 < 0)
		return FALSE;

	const std::size_t firstDestinationX =
		static_cast<std::size_t>(firstDestinationX64);
	const std::size_t firstDestinationY =
		static_cast<std::size_t>(firstDestinationY64);
	const std::size_t destinationPitchPixels =
		destinationPitchBytes / sizeof(PIXEL);
	if (static_cast<std::size_t>(blitHeight - 1) >
		std::numeric_limits<std::size_t>::max() - firstDestinationY)
		return FALSE;
	const std::size_t lastDestinationY =
		firstDestinationY + static_cast<std::size_t>(blitHeight - 1);
	if (firstDestinationX > destinationPitchPixels ||
		static_cast<std::size_t>(blitWidth) >
			destinationPitchPixels - firstDestinationX ||
		lastDestinationY >
			std::numeric_limits<std::size_t>::max() /
				destinationPitchBytes)
		return FALSE;

	UINT8* destinationRowBytes =
		reinterpret_cast<UINT8*>(buffer) +
		firstDestinationY * destinationPitchBytes +
		firstDestinationX * sizeof(PIXEL);
	for (INT32 row = 0; row < blitHeight; ++row)
	{
		const std::size_t sourceOffset =
			(static_cast<std::size_t>(top + row) * image.usWidth) +
			left;
		const PIXEL* const source =
			image.pNativePixels + sourceOffset;
		const UINT8* const mask =
			image.pNativeTransparencyMask + sourceOffset;
		PIXEL* const destination =
			reinterpret_cast<PIXEL*>(destinationRowBytes);
		for (INT32 column = 0; column < blitWidth; ++column)
		{
			if (mask[column]) destination[column] = source[column];
		}
		destinationRowBytes += destinationPitchBytes;
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
