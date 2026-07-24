#include "vobject_blitters.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
bool IsValidDepthSource(HVOBJECT source, UINT16 frame)
{
	return source &&
		source->ubBitDepth == 8 &&
		frame < source->usNumberOfObjects &&
		source->pETRLEObject &&
		source->pPixData;
}

bool RowInsideClip(INT32 y, const SGPRect* clipping)
{
	return y >= 0 &&
		(!clipping ||
		 (y >= clipping->iTop && y < clipping->iBottom));
}

bool ColumnInsideClip(INT32 x, const SGPRect* clipping)
{
	return x >= 0 &&
		(!clipping ||
		 (x >= clipping->iLeft && x < clipping->iRight));
}
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransInvZ(
	PIXEL* pBuffer,
	UINT32 uiDestPitchBYTES,
	UINT16* pZBuffer,
	UINT16 usZValue,
	HVOBJECT hSrcVObject,
	INT32 iX,
	INT32 iY,
	UINT16 usIndex)
{
	if (!pBuffer || !pZBuffer ||
		!IsValidDepthSource(hSrcVObject, usIndex) ||
		!hSrcVObject->pShadeCurrent)
		return FALSE;
	const ETRLEObject& image =
		hSrcVObject->pETRLEObject[usIndex];
	if (image.usWidth == 0 || image.usHeight == 0 ||
		image.uiDataOffset >= hSrcVObject->uiSizePixData)
		return FALSE;

	const std::int64_t imageLeft =
		static_cast<std::int64_t>(iX) + image.sOffsetX;
	const std::int64_t imageTop =
		static_cast<std::int64_t>(iY) + image.sOffsetY;
	const std::int64_t imageRight = imageLeft + image.usWidth;
	const std::int64_t imageBottom = imageTop + image.usHeight;
	if (imageLeft < 0 || imageTop < 0 ||
		imageRight > std::numeric_limits<INT32>::max() ||
		imageBottom > std::numeric_limits<INT32>::max())
		return FALSE;

	const UINT8* const sourceBase =
		static_cast<const UINT8*>(hSrcVObject->pPixData);
	const UINT8* source =
		sourceBase + image.uiDataOffset;
	const UINT8* const sourceEnd =
		sourceBase + hSrcVObject->uiSizePixData;
	for (UINT32 row = 0; row < image.usHeight; ++row)
	{
		const INT32 destinationY =
			static_cast<INT32>(imageTop + row);
		PIXEL* const destinationRow =
			reinterpret_cast<PIXEL*>(
				reinterpret_cast<UINT8*>(pBuffer) +
				static_cast<std::size_t>(destinationY) *
					uiDestPitchBYTES);
		const UINT16* const depthRow =
			reinterpret_cast<const UINT16*>(
				reinterpret_cast<const UINT8*>(pZBuffer) +
				static_cast<std::size_t>(destinationY) *
					uiDestPitchBYTES);
		UINT32 column = 0;
		while (true)
		{
			if (source >= sourceEnd) return FALSE;
			const UINT8 command = *source++;
			if (command == 0) break;
			const UINT32 runLength = command & 0x7fu;
			if (runLength == 0 ||
				runLength > image.usWidth - column)
				return FALSE;
			if ((command & 0x80u) != 0)
			{
				column += runLength;
				continue;
			}
			if (static_cast<std::size_t>(sourceEnd - source) <
				runLength)
				return FALSE;
			for (UINT32 run = 0; run < runLength; ++run)
			{
				const INT32 destinationX =
					static_cast<INT32>(
						imageLeft + column + run);
				if (depthRow[destinationX] == usZValue)
					destinationRow[destinationX] =
						hSrcVObject->pShadeCurrent[
							source[run]];
			}
			source += runLength;
			column += runLength;
		}
		if (column != image.usWidth) return FALSE;
	}
	return TRUE;
}

BOOLEAN Query8BPPDataToDepthBufferOcclusion(
	UINT32 uiDestPitchBYTES,
	UINT16* pZBuffer,
	INT16 sZValue,
	HVOBJECT hSrcVObject,
	INT32 iX,
	INT32 iY,
	UINT16 usIndex,
	const SGPRect* clipregion,
	BOOLEAN* pFullyOccluded)
{
	if (!pZBuffer || !pFullyOccluded ||
		!IsValidDepthSource(hSrcVObject, usIndex))
		return FALSE;
	const ETRLEObject& image =
		hSrcVObject->pETRLEObject[usIndex];
	if (image.usWidth == 0 || image.usHeight == 0 ||
		image.uiDataOffset >= hSrcVObject->uiSizePixData)
		return FALSE;

	const std::int64_t imageLeft =
		static_cast<std::int64_t>(iX) + image.sOffsetX;
	const std::int64_t imageTop =
		static_cast<std::int64_t>(iY) + image.sOffsetY;
	const std::int64_t imageRight = imageLeft + image.usWidth;
	const std::int64_t imageBottom = imageTop + image.usHeight;
	if (imageLeft < std::numeric_limits<INT32>::min() ||
		imageTop < std::numeric_limits<INT32>::min() ||
		imageRight > std::numeric_limits<INT32>::max() ||
		imageBottom > std::numeric_limits<INT32>::max())
		return FALSE;
	if (!clipregion && (imageLeft < 0 || imageTop < 0))
		return FALSE;

	const UINT8* const sourceBase =
		static_cast<const UINT8*>(hSrcVObject->pPixData);
	const UINT8* source =
		sourceBase + image.uiDataOffset;
	const UINT8* const sourceEnd =
		sourceBase + hSrcVObject->uiSizePixData;
	*pFullyOccluded = TRUE;
	for (UINT32 row = 0; row < image.usHeight; ++row)
	{
		const INT32 destinationY =
			static_cast<INT32>(imageTop + row);
		const UINT16* const depthRow =
			RowInsideClip(destinationY, clipregion) ?
				reinterpret_cast<const UINT16*>(
					reinterpret_cast<const UINT8*>(pZBuffer) +
					static_cast<std::size_t>(destinationY) *
						uiDestPitchBYTES) :
				nullptr;
		UINT32 column = 0;
		while (true)
		{
			if (source >= sourceEnd) return FALSE;
			const UINT8 command = *source++;
			if (command == 0) break;
			const UINT32 runLength = command & 0x7fu;
			if (runLength == 0 ||
				runLength > image.usWidth - column)
				return FALSE;
			if ((command & 0x80u) != 0)
			{
				column += runLength;
				continue;
			}
			if (static_cast<std::size_t>(sourceEnd - source) <
				runLength)
				return FALSE;
			for (UINT32 run = 0; run < runLength; ++run)
			{
				const INT32 destinationX =
					static_cast<INT32>(
						imageLeft + column + run);
				if (!depthRow ||
					!ColumnInsideClip(
						destinationX, clipregion))
					continue;
				if (sZValue >
					static_cast<INT16>(
						depthRow[destinationX]))
				{
					*pFullyOccluded = FALSE;
					return TRUE;
				}
			}
			source += runLength;
			column += runLength;
		}
		if (column != image.usWidth) return FALSE;
	}
	return TRUE;
}

// Retained as a source/ABI compatibility entry point while the legacy tile
// renderer migrates to the explicit engine visibility query.
BOOLEAN IsTileRedundent(
	UINT32 uiDestPitchBYTES,
	UINT16* pZBuffer,
	UINT16 usZValue,
	HVOBJECT hSrcVObject,
	INT32 iX,
	INT32 iY,
	UINT16 usIndex)
{
	BOOLEAN fullyOccluded = FALSE;
	if (!Query8BPPDataToDepthBufferOcclusion(
			uiDestPitchBYTES, pZBuffer,
			static_cast<INT16>(usZValue), hSrcVObject,
			iX, iY, usIndex, nullptr, &fullyOccluded))
		return FALSE;
	return fullyOccluded;
}
