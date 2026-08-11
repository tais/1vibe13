#include "types.h"
#include "himage.h"
#include "Quantize.h"
#include "Quantize Wrap.h"
#include "ImageUtilityModel.h"

#include <array>
#include <cstring>
#include <vector>

BOOLEAN	QuantizeImage( UINT8 *pDest, const UINT8 *pSrc, INT16 sWidth, INT16 sHeight, SGPPaletteEntry *pPalette )
{
	std::size_t sourceSize = 0;
	std::size_t pixelCount = 0;
	if (!pDest || !pSrc || !pPalette ||
		!UtilsImageUtilityModel::CheckedImageByteCount(
			sWidth, sHeight, 3, sourceSize) ||
		!UtilsImageUtilityModel::CheckedImageByteCount(
			sWidth, sHeight, 1, pixelCount))
	{
		return FALSE;
	}

	try
	{
		// FIRST CREATE PALETTE
		CQuantizer q(255, 6);
		if (!q.ProcessImage(pSrc, sWidth, sHeight)) return FALSE;
		const UINT colorCount = q.GetColorCount();
		if (colorCount == 0 || colorCount > 255) return FALSE;

		std::array<RGBQUAD, 256> quantizedPalette{};
		std::array<SGPPaletteEntry, 256> stagedPalette{};
		q.GetColorTable(quantizedPalette.data());
		for (UINT index = 0; index < colorCount; ++index)
		{
			// CQuantizer consumes legacy source triples as B,G,R. Explicitly
			// reverse RGBQUAD instead of relying on incompatible struct layouts.
			stagedPalette[index].peRed = quantizedPalette[index].rgbBlue;
			stagedPalette[index].peGreen = quantizedPalette[index].rgbGreen;
			stagedPalette[index].peBlue = quantizedPalette[index].rgbRed;
			stagedPalette[index].peFlags = 0;
		}

		// THEN MAP IMAGE TO PALETTE
		std::vector<UINT8> stagedPixels(pixelCount);
		if (!TryMapPalette(stagedPixels.data(), pSrc, sWidth, sHeight,
			static_cast<INT16>(colorCount), stagedPalette.data()))
		{
			return FALSE;
		}
		std::memcpy(pDest, stagedPixels.data(), pixelCount);
		std::memcpy(pPalette, stagedPalette.data(), sizeof(stagedPalette));
		return TRUE;
	}
	catch (...)
	{
		return FALSE;
	}
}

BOOLEAN TryMapPalette( UINT8 *pDest, const UINT8 *pSrc, INT16 sWidth, INT16 sHeight, INT16 sNumColors, const SGPPaletteEntry *pTable )
{
	std::size_t sourceSize = 0;
	std::size_t pixelCount = 0;
	if (!pDest || !pSrc || !pTable || sNumColors <= 0 || sNumColors > 256 ||
		!UtilsImageUtilityModel::CheckedImageByteCount(
			sWidth, sHeight, 3, sourceSize) ||
		!UtilsImageUtilityModel::CheckedImageByteCount(
			sWidth, sHeight, 1, pixelCount))
	{
		return FALSE;
	}

	std::array<UINT8, 256 * 3> paletteRgb{};
	for (INT16 index = 0; index < sNumColors; ++index)
	{
		paletteRgb[static_cast<std::size_t>(index) * 3] = pTable[index].peRed;
		paletteRgb[static_cast<std::size_t>(index) * 3 + 1] = pTable[index].peGreen;
		paletteRgb[static_cast<std::size_t>(index) * 3 + 2] = pTable[index].peBlue;
	}
	for (std::size_t index = 0; index < pixelCount; ++index)
	{
		const std::size_t sourceOffset = index * 3;
		pDest[index] = UtilsImageUtilityModel::NearestPaletteIndex(
			pSrc[sourceOffset], pSrc[sourceOffset + 1],
			pSrc[sourceOffset + 2],
			paletteRgb.data(), static_cast<std::size_t>(sNumColors));
	}
	return TRUE;
}

void MapPalette( UINT8 *pDest, UINT8 *pSrc, INT16 sWidth, INT16 sHeight, INT16 sNumColors, SGPPaletteEntry *pTable )
{
	TryMapPalette(pDest, pSrc, sWidth, sHeight, sNumColors, pTable);
}
