#ifndef SGP_WINFONT_H
#define SGP_WINFONT_H

#include "types.h"
#include "vobject.h"

// Portable description of the optional scalable-font path. Font bytes are
// selected by the backend from Ja2.ini or a platform font fallback; callers
// choose only the metrics/style needed by the legacy font catalogue.
// The subsystem is owned by the game/render thread; callers must not race its
// lifecycle, colour, measurement, or drawing operations.
struct WinFontDescriptor
{
	INT32 pixelHeight;
	BOOLEAN bold;
};

BOOLEAN InitWinFonts();
void ShutdownWinFonts();

BOOLEAN InitTooltipFonts();
void ShutdownTooltipFonts();

INT32 CreateWinFont(const WinFontDescriptor& descriptor);
void DeleteWinFont(INT32 iFont);

void SetWinFontBackColor(INT32 iFont, const COLORVAL* pColor);
void SetWinFontForeColor(INT32 iFont, const COLORVAL* pColor);

// Text is already formatted by the legacy Font.cpp boundary. Keeping this a
// plain-text API prevents localized '%' characters from becoming format input.
BOOLEAN PrintWinFont(
	UINT32 uiDestBuf, INT32 iFont, INT32 x, INT32 y, STR16 text);

INT16 WinFontStringPixLength(STR16 text, INT32 iFont);
INT16 GetWinFontHeight(INT32 iFont);
BOOLEAN IsWinFontReady(INT32 iFont);
BOOLEAN IsWinFontBackendAvailable();

enum
{
	WIN_LARGEFONT1 = 0,
	WIN_SMALLFONT1,
	WIN_TINYFONT1,
	WIN_12POINTFONT1,
	WIN_COMPFONT,
	WIN_SMALLCOMPFONT,
	WIN_10POINTROMAN,
	WIN_12POINTROMAN,
	WIN_14POINTSANSSERIF,
	WIN_10POINTARIAL,
	WIN_14POINTARIAL,
	WIN_12POINTARIAL,
	WIN_BLOCKYFONT,
	WIN_BLOCKYFONT2,
	WIN_10POINTARIALBOLD,
	WIN_12POINTARIALFIXEDFONT,
	WIN_16POINTARIAL,
	WIN_BLOCKFONTNARROW,
	WIN_14POINTHUMANIST,
	WIN_HUGEFONT,
	WIN_LASTFONT
};

#define MAX_WINFONTMAP 25
extern INT32 WinFontMap[MAX_WINFONTMAP];
extern INT32 TOOLTIP_IFONT;
extern INT32 TOOLTIP_IFONT_BOLD;

#endif
