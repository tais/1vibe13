	#include "DirectDraw Calls.h"
	#include <stdio.h>
	#include "DEBUG.H"
		#include "video.h"
	#include "himage.h"
	#include "vobject.h"
	#include "vobject_blitters.h"
	#include "shading.h"

BOOLEAN ShadesCalculateTables(SGPPaletteEntry *p8BPPPalette);
BOOLEAN ShadesCalculatePalette(SGPPaletteEntry *pSrcPalette, SGPPaletteEntry *pDestPalette, UINT16 usRed, UINT16 usGreen, UINT16 usBlue, BOOLEAN fMono);
void FindIndecies(SGPPaletteEntry *pSrcPalette, SGPPaletteEntry *pMapPalette, UINT8 *pTable);
void FindMaskIndecies(UINT8 *, UINT8 *, UINT8 *);

SGPPaletteEntry Shaded8BPPPalettes[HVOBJECT_SHADE_TABLES+3][256];
UINT8 ubColorTables[HVOBJECT_SHADE_TABLES+3][256];

UINT16	IntensityTable[65536];
UINT16	ShadeTable[65536];
UINT16	White16BPPPalette[ 256 ];
FLOAT	guiShadePercent = (FLOAT)0.48;
FLOAT	guiBrightPercent = (FLOAT)1.1;

BOOLEAN ShadesCalculateTables(SGPPaletteEntry *p8BPPPalette)
{
UINT32	uiCount;

		// Green palette
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[0], 0, 255, 0, TRUE);
		// Blue palette
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[HVOBJECT_SHADE_TABLES],	0, 0, 255, TRUE);
		// Yellow palette
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[HVOBJECT_SHADE_TABLES+1], 255, 255, 0, TRUE);
		// Red palette
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[HVOBJECT_SHADE_TABLES+2], 255, 0, 0, TRUE);

		// these are the brightening tables, 115%-150% brighter than original
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[1], 293, 293, 293, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[2], 281, 281, 281, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[3], 268, 268, 268, FALSE);

		// palette 4 is the non-modified palette.
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[4], 255, 255, 255, FALSE);

		// the rest are darkening tables, right down to all-black.
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[5], 195, 195, 195, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[6], 165, 165, 165, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[7], 135, 135, 135, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[8], 105, 105, 105, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[9], 75, 75, 75, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[10], 45, 45, 45, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[11], 36, 36, 36, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[12], 27, 27, 27, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[13], 18, 18, 18, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[14], 9, 9, 9, FALSE);
		ShadesCalculatePalette(p8BPPPalette, Shaded8BPPPalettes[15], 0, 0, 0, FALSE);


	// Remap the shade colors to the original palette
	for(uiCount=0; uiCount < (HVOBJECT_SHADE_TABLES+3); uiCount++)
	{
		FindIndecies(Shaded8BPPPalettes[uiCount], p8BPPPalette, ubColorTables[uiCount]);
		ubColorTables[uiCount][0]=0;
	}

	return(TRUE);
}

BOOLEAN ShadesCalculatePalette(SGPPaletteEntry *pSrcPalette, SGPPaletteEntry *pDestPalette, UINT16 usRed, UINT16 usGreen, UINT16 usBlue, BOOLEAN fMono)
{
UINT32 cnt, lumin;
UINT32 rmod, gmod, bmod;

	Assert( pSrcPalette != NULL );
	Assert( pDestPalette != NULL );

	for ( cnt = 0; cnt < 256; cnt++ )
	{
		if(fMono)
		{
			lumin=(pSrcPalette[ cnt ].peRed*299/1000)+ (pSrcPalette[ cnt ].peGreen*587/1000)+(pSrcPalette[ cnt ].peBlue*114/1000);
			rmod=usRed*lumin/255;
			gmod=usGreen*lumin/255;
			bmod=usBlue*lumin/255;
		}
		else
		{
			rmod = (usRed*pSrcPalette[ cnt ].peRed/255);
			gmod = (usGreen*pSrcPalette[ cnt ].peGreen/255);
			bmod = (usBlue*pSrcPalette[ cnt ].peBlue/255);
		}

		pDestPalette[ cnt ].peRed = (UINT8)__min(rmod, 255);
		pDestPalette[ cnt ].peGreen = (UINT8)__min(gmod, 255);
		pDestPalette[ cnt ].peBlue = (UINT8)__min(bmod, 255);
	}

	return(TRUE);
}




void FindIndecies(SGPPaletteEntry *pSrcPalette, SGPPaletteEntry *pMapPalette, UINT8 *pTable)
{
	// Nearest-color lookup, brute-force: for each shaded color in
	// pSrcPalette[1..255], find the index in pMapPalette[1..255] whose
	// RGB value minimizes sum-of-abs-deltas across the three channels.
	// Index 0 is reserved for the transparent color.
	pTable[0] = 0;
	for (UINT32 si = 1; si < 256; ++si) {
		const SGPPaletteEntry& s = pSrcPalette[si];
		UINT32 bestIdx = 0;
		UINT32 bestDelta = 0xFFFFFFFFu;
		for (UINT32 mi = 1; mi < 256; ++mi) {
			const SGPPaletteEntry& m = pMapPalette[mi];
			const INT32 dr = (INT32)m.peRed   - (INT32)s.peRed;
			const INT32 dg = (INT32)m.peGreen - (INT32)s.peGreen;
			const INT32 db = (INT32)m.peBlue  - (INT32)s.peBlue;
			const UINT32 d =
				(UINT32)(dr < 0 ? -dr : dr) +
				(UINT32)(dg < 0 ? -dg : dg) +
				(UINT32)(db < 0 ? -db : db);
			if (d < bestDelta) {
				bestDelta = d;
				bestIdx   = mi;
			}
		}
		pTable[si] = (UINT8)bestIdx;
	}
}

/**********************************************************************************************
 BuildShadeTable

	Builds a 16-bit color shading table. This function should be called only after the current
	video adapter's pixel format is known (IE: GetRgbDistribution() has been called, and the
	globals for masks and shifts have been initialized by that function), and before any
	blitting is done.

	Using the table is a straight lookup. The pixel to be shaded down is used as the index into
	the table and the entry at that point will be a pixel that is 25% darker.

**********************************************************************************************/
void BuildShadeTable(void)
{
	UINT16 red, green, blue;
	UINT16 index = 0;


	for(red=0; red < 256; red+=4)
		for(green=0; green < 256; green+=4)
			for(blue=0; blue < 256; blue+=4)
			{
				index=Get16BPPColor(FROMRGB(red, green, blue));
				ShadeTable[index]=Get16BPPColor(FROMRGB(red*guiShadePercent, green*guiShadePercent, blue*guiShadePercent));
			}

	memset( White16BPPPalette, 65535, sizeof( White16BPPPalette ) );
}


/**********************************************************************************************
 BuildIntensityTable

	Builds a 16-bit color shading table. This function should be called only after the current
	video adapter's pixel format is known (IE: GetRgbDistribution() has been called, and the
	globals for masks and shifts have been initialized by that function), and before any
	blitting is done.



**********************************************************************************************/
void BuildIntensityTable(void)
{
	UINT16 red, green, blue = 0;
	UINT16 index = 0;
	FLOAT	dShadedPercent = (FLOAT)0.80;






	for(red=0; red < 256; red+=4)
		for(green=0; green < 256; green+=4)
			for(blue=0; blue < 256; blue+=4)
			{
				index=Get16BPPColor(FROMRGB(red, green, blue));
				IntensityTable[index]=Get16BPPColor(FROMRGB(red*dShadedPercent, green*dShadedPercent, blue*dShadedPercent));
			}

}



void SetShadeTablePercent( FLOAT uiShadePercent )
{
	guiShadePercent = uiShadePercent;
	BuildShadeTable( );
}


void Init8BitTables(void)
{
SGPPaletteEntry Pal[256];
UINT32 uiCount;

	// calculate a grey-scale table for the default palette
	for(uiCount=0; uiCount < 256; uiCount++)
	{
		Pal[uiCount].peRed=(UINT8)(uiCount%128)+128;
		Pal[uiCount].peGreen=(UINT8)(uiCount%128)+128;
		Pal[uiCount].peBlue=(UINT8)(uiCount%128)+128;
	}

	Pal[0].peRed=0;
	Pal[0].peGreen=0;
	Pal[0].peBlue=0;

	Set8BPPPalette(Shaded8BPPPalettes[4]);
}

BOOLEAN Set8BitModePalette(SGPPaletteEntry *pPal)
{
	ShadesCalculateTables(pPal);
	Set8BPPPalette(pPal);
	return(TRUE);
}

