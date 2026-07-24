#include "vobject_blitters.h"

#include "DEBUG.H"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace
{
constexpr UINT16 kMultiZSubLayers = 8;
constexpr UINT16 kStructureStripDepthDelta = kMultiZSubLayers * 10;
constexpr UINT16 kStripWidth = 20;

template <typename Core>
inline void BlitMultiZStripRun(
	const UINT8* sourcePixels,
	UINT8* destinationPixels,
	UINT8* depthPixels,
	INT32 blitWidth,
	INT32 blitHeight,
	INT32 leftSkip,
	INT32 topSkip,
	UINT32 lineSkip,
	UINT16 stripDepthDelta,
	UINT16 startDepth,
	UINT16 startColumns,
	UINT16 startIndex,
	const INT8* depthChanges,
	UINT32 initialLineParity,
	Core core)
{
	const UINT8* source = sourcePixels;
	for (INT32 row = 0; row < topSkip; ++row)
	{
		for (;;)
		{
			const UINT8 command = *source++;
			if (command == 0) break;
			if ((command & 0x80) == 0) source += command;
		}
	}

	UINT8* destination = destinationPixels;
	UINT8* depth = depthPixels;
	UINT32 lineParity = initialLineParity;
	const UINT32 depthLineSkip = lineSkip;
	const UINT32 destinationLineSkip =
		lineSkip + static_cast<UINT32>(blitWidth) * 2u -
		static_cast<UINT32>(blitWidth) *
			static_cast<UINT32>(sizeof(PIXEL));

	for (INT32 row = 0; row < blitHeight; ++row)
	{
		UINT16 stripDepth = startDepth;
		UINT16 stripIndex = startIndex;
		UINT16 stripColumns = startColumns;
		INT32 remaining;
		INT32 runLength;

		for (remaining = leftSkip; remaining > 0;
			remaining -= runLength)
		{
			runLength = *source++;
			if (runLength & 0x80)
			{
				runLength &= 0x7f;
				if (runLength > remaining)
				{
					runLength -= remaining;
					remaining = blitWidth;
					goto transparent_run;
				}
			}
			else
			{
				if (runLength > remaining)
				{
					source += remaining;
					runLength -= remaining;
					remaining = blitWidth;
					goto opaque_run;
				}
				source += runLength;
			}
		}

		remaining = blitWidth;
		while (remaining > 0)
		{
			runLength = *source++;
			if (runLength & 0x80)
			{
transparent_run:
				runLength &= 0x7f;
				if (runLength > remaining) runLength = remaining;
				remaining -= runLength;
				destination += sizeof(PIXEL) * runLength;
				depth += sizeof(UINT16) * runLength;
				for (;;)
				{
					if (runLength >= static_cast<INT32>(stripColumns))
					{
						runLength -= stripColumns;
						stripColumns = kStripWidth;
						const INT8 change =
							depthChanges[stripIndex++];
						if (change < 0)
							stripDepth -= stripDepthDelta;
						else if (change > 0)
							stripDepth += stripDepthDelta;
					}
					else
					{
						stripColumns -= runLength;
						break;
					}
				}
			}
			else
			{
opaque_run:
				INT32 unblitted;
				if (runLength > remaining)
				{
					unblitted = runLength - remaining;
					runLength = remaining;
				}
				else
				{
					unblitted = 0;
				}
				remaining -= runLength;
				do
				{
					core(
						source,
						reinterpret_cast<PIXEL*>(destination),
						reinterpret_cast<UINT16*>(depth),
						stripDepth,
						lineParity);
					++source;
					destination += sizeof(PIXEL);
					depth += sizeof(UINT16);
					if (--stripColumns == 0)
					{
						stripColumns = kStripWidth;
						const INT8 change =
							depthChanges[stripIndex++];
						if (change < 0)
							stripDepth -= stripDepthDelta;
						else if (change > 0)
							stripDepth += stripDepthDelta;
					}
				}
				while (--runLength > 0);
				source += unblitted;
			}
		}

		while (*source++ != 0) {}
		lineParity ^= 1;
		destination += destinationLineSkip;
		depth += depthLineSkip;
	}
}

template <
	bool InclusiveDepth,
	bool PixelateObscured,
	bool ShadowMarker,
	bool AlphaBlend>
BOOLEAN BlitMultiZStrip(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 baseDepth,
	HVOBJECT source,
	HVOBJECT alphaSource,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	const SGPRect* clipping,
	INT16 depthProfileFrame,
	PIXEL* externalPalette,
	BOOLEAN ignoreShadows,
	UINT16 stripDepthDelta,
	bool paletteLeftClipRule)
{
	if (!destination || !depthBuffer || !source ||
		source->ubBitDepth != 8 ||
		frame >= source->usNumberOfObjects ||
		depthProfileFrame < 0 ||
		static_cast<UINT16>(depthProfileFrame) >=
			source->usNumberOfObjects ||
		!source->pETRLEObject || !source->pPixData ||
		(ShadowMarker && !externalPalette) ||
		(!ShadowMarker && !source->pShadeCurrent))
		return FALSE;
	if constexpr (AlphaBlend)
	{
		if (!alphaSource || alphaSource->ubBitDepth != 8 ||
			frame >= alphaSource->usNumberOfObjects ||
			!alphaSource->pETRLEObject || !alphaSource->pPixData)
			return FALSE;
	}

	const ETRLEObject& image = source->pETRLEObject[frame];
	if (image.usWidth == 0 || image.usHeight == 0 ||
		image.uiDataOffset >= source->uiSizePixData)
		return FALSE;
	if constexpr (AlphaBlend)
	{
		const ETRLEObject& alphaImage =
			alphaSource->pETRLEObject[frame];
		if (alphaImage.usWidth != image.usWidth ||
			alphaImage.usHeight != image.usHeight ||
			alphaImage.uiDataOffset >= alphaSource->uiSizePixData)
			return FALSE;
	}

	const INT32 imageX = destinationX + image.sOffsetX;
	const INT32 imageY = destinationY + image.sOffsetY;
	const SGPRect& clip = clipping ? *clipping : ClippingRect;
	const INT32 width = static_cast<INT32>(image.usWidth);
	const INT32 height = static_cast<INT32>(image.usHeight);
	const INT32 leftSkip = std::min(
		clip.iLeft - std::min(clip.iLeft, imageX), width);
	const INT32 rightSkip = std::min(
		std::max(clip.iRight, imageX + width) - clip.iRight,
		width);
	const INT32 topSkip = std::min(
		clip.iTop - std::min(clip.iTop, imageY), height);
	const INT32 bottomSkip = std::min(
		std::max(clip.iBottom, imageY + height) - clip.iBottom,
		height);
	const INT32 blitWidth = width - leftSkip - rightSkip;
	const INT32 blitHeight = height - topSkip - bottomSkip;
	if (leftSkip >= width || rightSkip >= width ||
		topSkip >= height || bottomSkip >= height)
		return TRUE;

	if (!source->ppZStripInfo)
	{
		DebugMsg(
			TOPIC_VIDEOOBJECT, DBG_LEVEL_0,
			"Missing Z-Strip info on multi-Z object");
		return FALSE;
	}
	const ZStripInfo* const profile =
		source->ppZStripInfo[depthProfileFrame];
	if (!profile || !profile->pbZChange)
	{
		DebugMsg(
			TOPIC_VIDEOOBJECT, DBG_LEVEL_0,
			"Missing Z-Strip info on multi-Z object");
		return FALSE;
	}

	UINT16 startDepth = static_cast<UINT16>(
		static_cast<INT16>(baseDepth) +
		static_cast<INT16>(profile->bInitialZChange) *
			static_cast<INT16>(kStructureStripDepthDelta));
	UINT16 startColumns;
	if (leftSkip > profile->ubFirstZStripWidth)
	{
		startColumns = static_cast<UINT16>(
			leftSkip - profile->ubFirstZStripWidth);
		startColumns = static_cast<UINT16>(
			kStripWidth - (startColumns % kStripWidth));
	}
	else if (leftSkip < profile->ubFirstZStripWidth)
	{
		startColumns = static_cast<UINT16>(
			profile->ubFirstZStripWidth - leftSkip);
	}
	else
	{
		startColumns = kStripWidth;
	}

	UINT16 startIndex = 0;
	const bool startsPastFirstStrip =
		paletteLeftClipRule ?
			leftSkip >= static_cast<INT32>(startColumns) :
			leftSkip >= profile->ubFirstZStripWidth;
	if (startsPastFirstStrip)
	{
		startIndex = static_cast<UINT16>(
			1 + ((leftSkip - profile->ubFirstZStripWidth) /
				kStripWidth));
		for (UINT16 index = 0; index < startIndex; ++index)
		{
			const INT8 change = profile->pbZChange[index];
			if (change < 0)
				startDepth -= stripDepthDelta;
			else if (change > 0)
				startDepth += stripDepthDelta;
		}
	}

	const UINT8* const sourcePixels =
		static_cast<const UINT8*>(source->pPixData) +
		image.uiDataOffset;
	const UINT8* alphaPixels = nullptr;
	if constexpr (AlphaBlend)
	{
		alphaPixels =
			static_cast<const UINT8*>(alphaSource->pPixData) +
			alphaSource->pETRLEObject[frame].uiDataOffset;
	}
	UINT8* const destinationPixels =
		reinterpret_cast<UINT8*>(destination) +
		destinationPitchBytes * (imageY + topSkip) +
		(imageX + leftSkip) * sizeof(PIXEL);
	UINT8* const depthPixels =
		reinterpret_cast<UINT8*>(depthBuffer) +
		destinationPitchBytes * (imageY + topSkip) +
		(imageX + leftSkip) * sizeof(UINT16);
	PIXEL* const palette =
		ShadowMarker ? externalPalette : source->pShadeCurrent;
	const UINT32 lineSkip =
		destinationPitchBytes -
		static_cast<UINT32>(blitWidth) * sizeof(UINT16);
	const UINT32 lineParity =
		PixelateObscured ?
			static_cast<UINT32>(imageY & 1) : 0;

	BlitMultiZStripRun(
		sourcePixels,
		destinationPixels,
		depthPixels,
		blitWidth,
		blitHeight,
		leftSkip,
		topSkip,
		lineSkip,
		stripDepthDelta,
		startDepth,
		startColumns,
		startIndex,
		profile->pbZChange,
		lineParity,
		[&](
			const UINT8* sourcePixel,
			PIXEL* destinationPixel,
			UINT16* destinationDepth,
			UINT16 stripDepth,
			UINT32 rowParity)
		{
			const bool passesDepth = InclusiveDepth ?
				*destinationDepth <= stripDepth :
				*destinationDepth < stripDepth;
			bool draw = passesDepth;
			if constexpr (PixelateObscured)
			{
				draw = draw ||
					(((rowParity & 1u) != 0) ==
					 ((reinterpret_cast<std::uintptr_t>(
						 destinationPixel) &
						sizeof(PIXEL)) != 0));
			}
			if (!draw) return;

			*destinationDepth = stripDepth;
			const UINT8 index = *sourcePixel;
			if constexpr (ShadowMarker)
			{
				if (index == 254)
				{
					if (!ignoreShadows)
						*destinationPixel =
							PixShade(*destinationPixel);
					return;
				}
			}
			if constexpr (AlphaBlend)
			{
				*destinationPixel = blendWithAlpha(
					palette[index],
					*destinationPixel,
					alphaPixels[sourcePixel - sourcePixels]);
			}
			else
			{
				*destinationPixel = palette[index];
			}
		});
	return TRUE;
}
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncClip(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping)
{
	return Blt8BPPDataTo16BPPBufferTransZIncClipProfile(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, destinationX, destinationY, frame, clipping,
		static_cast<UINT16>(frame), FALSE);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncClipProfile(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping,
	UINT16 depthProfileFrame,
	BOOLEAN sameDepthPasses)
{
	if (depthProfileFrame >
		static_cast<UINT16>(
			std::numeric_limits<INT16>::max()))
		return FALSE;
	if (sameDepthPasses)
	{
		return BlitMultiZStrip<true, false, false, false>(
			destination, destinationPitchBytes, depthBuffer, depth,
			source, nullptr, destinationX, destinationY, frame,
			clipping, static_cast<INT16>(depthProfileFrame),
			nullptr, FALSE, kStructureStripDepthDelta, false);
	}
	return BlitMultiZStrip<false, false, false, false>(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, nullptr, destinationX, destinationY, frame,
		clipping, static_cast<INT16>(depthProfileFrame), nullptr, FALSE,
		kStructureStripDepthDelta, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncClipZSameZBurnsThrough(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping,
	INT16 depthProfileFrame)
{
	if (depthProfileFrame < 0) return FALSE;
	return Blt8BPPDataTo16BPPBufferTransZIncClipProfile(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, destinationX, destinationY, frame, clipping,
		static_cast<UINT16>(depthProfileFrame), TRUE);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncObscureClip(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping)
{
	return Blt8BPPDataTo16BPPBufferTransZIncObscureClipProfile(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, destinationX, destinationY, frame, clipping,
		static_cast<UINT16>(frame));
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZIncObscureClipProfile(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping,
	UINT16 depthProfileFrame)
{
	if (depthProfileFrame >
		static_cast<UINT16>(
			std::numeric_limits<INT16>::max()))
		return FALSE;
	return BlitMultiZStrip<false, true, false, false>(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, nullptr, destinationX, destinationY, frame,
		clipping, static_cast<INT16>(depthProfileFrame), nullptr, FALSE,
		kStructureStripDepthDelta, false);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncClip(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping,
	INT16 depthProfileFrame,
	PIXEL* palette,
	BOOLEAN ignoreShadows)
{
	return BlitMultiZStrip<true, false, true, false>(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, nullptr, destinationX, destinationY, frame,
		clipping, depthProfileFrame, palette, ignoreShadows,
		kMultiZSubLayers, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncClipAlpha(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	HVOBJECT alphaSource,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping,
	INT16 depthProfileFrame,
	PIXEL* palette,
	BOOLEAN ignoreShadows)
{
	return BlitMultiZStrip<true, false, true, true>(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, alphaSource, destinationX, destinationY, frame,
		clipping, depthProfileFrame, palette, ignoreShadows,
		kMultiZSubLayers, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncObscureClip(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping,
	INT16 depthProfileFrame,
	PIXEL* palette,
	BOOLEAN ignoreShadows)
{
	return BlitMultiZStrip<true, true, true, false>(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, nullptr, destinationX, destinationY, frame,
		clipping, depthProfileFrame, palette, ignoreShadows,
		kMultiZSubLayers, true);
}

BOOLEAN Blt8BPPDataTo16BPPBufferTransZTransShadowIncObscureClipAlpha(
	PIXEL* destination,
	UINT32 destinationPitchBytes,
	UINT16* depthBuffer,
	UINT16 depth,
	HVOBJECT source,
	HVOBJECT alphaSource,
	INT32 destinationX,
	INT32 destinationY,
	UINT16 frame,
	SGPRect* clipping,
	INT16 depthProfileFrame,
	PIXEL* palette,
	BOOLEAN ignoreShadows)
{
	return BlitMultiZStrip<false, true, true, true>(
		destination, destinationPitchBytes, depthBuffer, depth,
		source, alphaSource, destinationX, destinationY, frame,
		clipping, depthProfileFrame, palette, ignoreShadows,
		kMultiZSubLayers, true);
}
