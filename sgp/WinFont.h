#ifndef __WINFONT_
#define __WINFONT_

// The GDI-backed TrueType font path (WinFont.cpp) was retired in the
// SDL3 port: it rasterized via Win32 GDI and blitted onto a DirectDraw
// surface, both of which are gone now that SDL3 owns video on every
// platform. The functions below are stubbed to no-ops so Font.cpp's
// WinFont call sites still compile; the iUseWinFonts flag stays off and
// Phase 9 brings real cross-platform text via stb_truetype.
// COLORVAL is typedef'd to UINT32 in vobject.h; we re-use it.
#include "vobject.h"
inline void InitWinFonts() {}
inline void ShutdownWinFonts() {}
inline void InitTooltipFonts() {}
inline void ShutdownTooltipFonts() {}
inline INT32 CreateWinFont(void*) { return -1; }
inline void DeleteWinFont(INT32) {}
inline void SetWinFontBackColor(INT32, COLORVAL*) {}
inline void SetWinFontForeColor(INT32, COLORVAL*) {}
inline void PrintWinFont(UINT32, INT32, INT32, INT32, STR16, ...) {}
inline INT16 WinFontStringPixLength(STR16, INT32) { return 0; }
inline INT16 GetWinFontHeight(INT32) { return 12; }
#define MAX_WINFONTMAP 25
extern INT32 WinFontMap[MAX_WINFONTMAP];
extern INT32 TOOLTIP_IFONT;
extern INT32 TOOLTIP_IFONT_BOLD;

#endif
