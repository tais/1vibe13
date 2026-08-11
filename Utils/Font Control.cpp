	#include <stdio.h>
	#include <time.h>
	#include "sgp.h"
	#include "himage.h"
	#include "vsurface.h"
	#include "WCheck.h"
	#include "Font Control.h"
#include "render_palette_registry.h"
#include <language.hpp>

INT32		  giCurWinFont = 0;
//BOOLEAN		gfUseWinFonts = FALSE;


// Global variables for video objects
INT32						gpLargeFontType1 = -1;
HVOBJECT				gvoLargeFontType1 = nullptr;

INT32						gpSmallFontType1 = -1;
HVOBJECT				gvoSmallFontType1 = nullptr;

INT32						gpTinyFontType1 = -1;
HVOBJECT				gvoTinyFontType1 = nullptr;

INT32						gp12PointFont1 = -1;
HVOBJECT				gvo12PointFont1 = nullptr;

INT32			gpClockFont = -1;
HVOBJECT		gvoClockFont = nullptr;

INT32			gpCompFont = -1;
HVOBJECT		gvoCompFont = nullptr;

INT32			gpSmallCompFont = -1;
HVOBJECT		gvoSmallCompFont = nullptr;

INT32						gp10PointRoman = -1;
HVOBJECT				gvo10PointRoman = nullptr;

INT32						gp12PointRoman = -1;
HVOBJECT				gvo12PointRoman = nullptr;

INT32						gp14PointSansSerif = -1;
HVOBJECT				gvo14PointSansSerif = nullptr;

//INT32						gpMilitaryFont1;
//HVOBJECT				gvoMilitaryFont1;

INT32						gp10PointArial = -1;
HVOBJECT				gvo10PointArial = nullptr;

INT32						gp10PointArialBold = -1;
HVOBJECT				gvo10PointArialBold = nullptr;

INT32						gp14PointArial = -1;
HVOBJECT				gvo14PointArial = nullptr;

INT32						gp12PointArial = -1;
HVOBJECT				gvo12PointArial = nullptr;

INT32			gpBlockyFont = -1;
HVOBJECT				gvoBlockyFont = nullptr;

INT32			gpBlockyFont2 = -1;
HVOBJECT				gvoBlockyFont2 = nullptr;

INT32			gpBlockyFont3 = -1;
HVOBJECT				gvoBlockyFont3 = nullptr;

INT32						gp12PointArialFixedFont = -1;
HVOBJECT				gvo12PointArialFixedFont = nullptr;

INT32						gp16PointArial = -1;
HVOBJECT				gvo16PointArial = nullptr;

INT32						gpBlockFontNarrow = -1;
HVOBJECT				gvoBlockFontNarrow = nullptr;

INT32						gp14PointHumanist = -1;
HVOBJECT				gvo14PointHumanist = nullptr;

#if defined( JA2EDITOR )
	INT32				gpHugeFont = -1;
	HVOBJECT			gvoHugeFont = nullptr;
#endif

//INT32		  giSubTitleWinFont;



BOOLEAN					gfFontsInit = FALSE;

UINT16 CreateFontPaletteTables(HVOBJECT pObj );


extern UINT16 gzFontName[32];

auto GetHugeFont() -> INT32 {
#if defined(JA2EDITOR)
	return g_lang == i18n::Lang::en ? gpHugeFont : gp16PointArial;
#else
	return gp16PointArial;
#endif
}

static BOOLEAN LoadManagedFont(
	const STR8 filename, INT32& index, HVOBJECT& object)
{
	index = LoadFontFile(filename);
	if (index < 0)
	{
		object = nullptr;
		return FALSE;
	}
	object = GetFontObject(index);
	if (object && CreateFontPaletteTables(object)) return TRUE;
	if (IsFontLoaded(index)) UnloadFont(index);
	index = -1;
	object = nullptr;
	return FALSE;
}

static void UnloadManagedFont(INT32& index, HVOBJECT& object)
{
	if (index >= 0 && index < MAX_FONTS && IsFontLoaded(index))
		UnloadFont(index);
	index = -1;
	object = nullptr;
}

BOOLEAN	InitializeFonts( )
{
	if (gfFontsInit) return TRUE;
	if (!LoadManagedFont("FONTS\\LARGEFONT1.sti", gpLargeFontType1, gvoLargeFontType1) ||
		!LoadManagedFont("FONTS\\SMALLFONT1.sti", gpSmallFontType1, gvoSmallFontType1) ||
		!LoadManagedFont("FONTS\\TINYFONT1.sti", gpTinyFontType1, gvoTinyFontType1) ||
		!LoadManagedFont("FONTS\\FONT12POINT1.sti", gp12PointFont1, gvo12PointFont1) ||
		!LoadManagedFont("FONTS\\CLOCKFONT.sti", gpClockFont, gvoClockFont) ||
		!LoadManagedFont("FONTS\\COMPFONT.sti", gpCompFont, gvoCompFont) ||
		!LoadManagedFont("FONTS\\SMALLCOMPFONT.sti", gpSmallCompFont, gvoSmallCompFont) ||
		!LoadManagedFont("FONTS\\FONT10ROMAN.sti", gp10PointRoman, gvo10PointRoman) ||
		!LoadManagedFont("FONTS\\FONT12ROMAN.sti", gp12PointRoman, gvo12PointRoman) ||
		!LoadManagedFont("FONTS\\FONT14SANSERIF.sti", gp14PointSansSerif, gvo14PointSansSerif) ||
		!LoadManagedFont("FONTS\\FONT10ARIAL.sti", gp10PointArial, gvo10PointArial) ||
		!LoadManagedFont("FONTS\\FONT14ARIAL.sti", gp14PointArial, gvo14PointArial) ||
		!LoadManagedFont("FONTS\\FONT10ARIALBOLD.sti", gp10PointArialBold, gvo10PointArialBold) ||
		!LoadManagedFont("FONTS\\FONT12ARIAL.sti", gp12PointArial, gvo12PointArial) ||
		!LoadManagedFont("FONTS\\BLOCKFONT.sti", gpBlockyFont, gvoBlockyFont) ||
		!LoadManagedFont("FONTS\\BLOCKFONT2.sti", gpBlockyFont2, gvoBlockyFont2) ||
		!LoadManagedFont("FONTS\\BLOCKFONT2.sti", gpBlockyFont3, gvoBlockyFont3) ||
		!LoadManagedFont("FONTS\\FONT12ARIALFIXEDWIDTH.sti",
			gp12PointArialFixedFont, gvo12PointArialFixedFont) ||
		!LoadManagedFont("FONTS\\FONT16ARIAL.sti", gp16PointArial, gvo16PointArial) ||
		!LoadManagedFont("FONTS\\BLOCKFONTNARROW.sti", gpBlockFontNarrow, gvoBlockFontNarrow) ||
		!LoadManagedFont("FONTS\\FONT14HUMANIST.sti", gp14PointHumanist, gvo14PointHumanist))
	{
		ShutdownFonts();
		return FALSE;
	}

	#if defined( JA2EDITOR )
	if(g_lang == i18n::Lang::en) {
		if (!LoadManagedFont("FONTS\\HUGEFONT.sti", gpHugeFont, gvoHugeFont))
		{
			ShutdownFonts();
			return FALSE;
		}
	}
	#endif

	// Set default for font system
	SetFontDestBuffer( FRAME_BUFFER, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, FALSE );

	gfFontsInit = TRUE;

	// The optional scalable path is portable now. A missing/invalid configured
	// font disables it transactionally and leaves the shipped bitmap catalogue
	// authoritative. Tooltip scaling independently uses the same backend.
	if (iUseWinFonts && !InitWinFonts()) iUseWinFonts = 0;
	if (fTooltipScaleFactor > 1.0F) InitTooltipFonts();

	return( TRUE );
}

void ShutdownFonts( )
{
	const bool wasInitialized = gfFontsInit;
	gfFontsInit = FALSE;
	UnloadManagedFont(gp14PointHumanist, gvo14PointHumanist);
	UnloadManagedFont(gpBlockFontNarrow, gvoBlockFontNarrow);
	UnloadManagedFont(gp16PointArial, gvo16PointArial);
	UnloadManagedFont(gp12PointArialFixedFont, gvo12PointArialFixedFont);
	UnloadManagedFont(gpBlockyFont3, gvoBlockyFont3);
	UnloadManagedFont(gpBlockyFont2, gvoBlockyFont2);
	UnloadManagedFont(gpBlockyFont, gvoBlockyFont);
	UnloadManagedFont(gp12PointArial, gvo12PointArial);
	UnloadManagedFont(gp10PointArialBold, gvo10PointArialBold);
	UnloadManagedFont(gp14PointArial, gvo14PointArial);
	UnloadManagedFont(gp10PointArial, gvo10PointArial);
	UnloadManagedFont(gp14PointSansSerif, gvo14PointSansSerif);
	UnloadManagedFont(gp12PointRoman, gvo12PointRoman);
	UnloadManagedFont(gp10PointRoman, gvo10PointRoman);
	UnloadManagedFont(gpSmallCompFont, gvoSmallCompFont);
	UnloadManagedFont(gpCompFont, gvoCompFont);
	UnloadManagedFont(gpClockFont, gvoClockFont);
	UnloadManagedFont(gp12PointFont1, gvo12PointFont1);
	UnloadManagedFont(gpTinyFontType1, gvoTinyFontType1);
	UnloadManagedFont(gpSmallFontType1, gvoSmallFontType1);
	UnloadManagedFont(gpLargeFontType1, gvoLargeFontType1);
	#if defined( JA2EDITOR )
	UnloadManagedFont(gpHugeFont, gvoHugeFont);
	#endif

	if (wasInitialized)
	{
		ShutdownTooltipFonts();
		ShutdownWinFonts();
	}
}

// Set shades for fonts
BOOLEAN SetFontShade( UINT32 uiFontID, INT8 bColorID )
{
	if (uiFontID >= MAX_FONTS || bColorID <= 0 || bColorID >= 16)
		return FALSE;
	if (!IsFontLoaded(static_cast<INT32>(uiFontID))) return FALSE;
	HVOBJECT pFont = GetFontObject(static_cast<INT32>(uiFontID));
	if (!pFont || !pFont->pShades[bColorID]) return FALSE;
	pFont->pShadeCurrent = pFont->pShades[bColorID];

	return( TRUE );
}

UINT16 CreateFontPaletteTables(HVOBJECT pObj )
{
	if (!pObj || !pObj->pPaletteEntry) return FALSE;
	UINT32 count;
	SGPPaletteEntry Pal[256];

	for( count = 0; count < 16; count++ )
	{
		if ( (count == 4) && (pObj->p16BPPPalette == pObj->pShades[ count ]) )
			pObj->pShades[ count ] = NULL;
		else if ( pObj->pShades[ count ] != NULL )
		{
			UnregisterLegacyRenderPalette(pObj->pShades[count]);
			MemFree( pObj->pShades[ count ] );
			pObj->pShades[ count ] = NULL;
		}
	}

	// Build white palette
	for(count=0; count < 256; count++)
	{
		Pal[count].peRed=(UINT8)255;
		Pal[count].peGreen=(UINT8)255;
		Pal[count].peBlue=(UINT8)255;
	}

	pObj->pShades[ FONT_SHADE_RED ]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 255, 0, 0, TRUE);
	pObj->pShades[ FONT_SHADE_BLUE ]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 0, 0, 255, TRUE);
	pObj->pShades[ FONT_SHADE_GREEN ]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 0, 255, 0, TRUE);
	pObj->pShades[ FONT_SHADE_YELLOW ]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 255, 255, 0, TRUE);
	pObj->pShades[ FONT_SHADE_NEUTRAL ]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 255, 255, 255, FALSE);

	pObj->pShades[ FONT_SHADE_WHITE ]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 255, 255, 255, TRUE);


	// the rest are darkening tables, right down to all-black.
	pObj->pShades[0]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 165, 165, 165, FALSE);
	pObj->pShades[7]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 135, 135, 135, FALSE);
	pObj->pShades[8]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 105, 105, 105, FALSE);
	pObj->pShades[9]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 75, 75, 75, FALSE);
	pObj->pShades[10]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 45, 45, 45, FALSE);
	pObj->pShades[11]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 36, 36, 36, FALSE);
	pObj->pShades[12]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 27, 27, 27, FALSE);
	pObj->pShades[13]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 18, 18, 18, FALSE);
	pObj->pShades[14]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 9, 9, 9, FALSE);
	pObj->pShades[15]=Create16BPPPaletteShaded( pObj->pPaletteEntry, 0, 0, 0, FALSE);

	// Set current shade table to neutral color
	pObj->pShadeCurrent=pObj->pShades[4];

	// check to make sure every table got a palette
	//for(count=0; (count < HVOBJECT_SHADE_TABLES) && (pObj->pShades[count]!=NULL); count++);

	// return the result of the check
	//return(count==HVOBJECT_SHADE_TABLES);
	return(TRUE);
}

UINT16	WFGetFontHeight( INT32 FontNum )
{
	if (FontNum < 0 || FontNum >= MAX_FONTS || !IsFontLoaded(FontNum)) return 0;
	return( GetFontHeight( FontNum ) );
}


INT16 WFStringPixLength( STR16 string,INT32 UseFont )
{
	if (!string || UseFont < 0 || UseFont >= MAX_FONTS ||
		!IsFontLoaded(UseFont)) return 0;
	return( StringPixLength( string, UseFont ) );
}



