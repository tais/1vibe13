// Definitions for globals that the JA2 codebase reads by extern but
// whose owning subsystem hasn't been ported off DirectDraw/Win32 yet.
// As each Phase replaces a subsystem with an SDL3-backed
// implementation, the corresponding globals move out of this file
// into the new implementation TUs.

#include "types.h"

// ghWindow: the Win32 HWND. The legacy WinMain used to own it; the
// SDL3 portable main() doesn't create a Win32 window directly, but
// several Win32-gated paths still reference ghWindow (ScreenToClient
// in mapscreen / tactical placement, OpenClipboard in Text Input,
// DSEnable in Win Util). Define it here as null so those TUs link;
// the SDL3 build doesn't drive them. (HWND only exists on Windows.)
#ifdef _WIN32
HWND ghWindow = nullptr;
#endif

// (sgp.cpp's globals -- gfProgramIsRunning, gfApplicationActive,
// gfGameInitialized, gzCommandLine, gzErrorMsg, iWindowedMode,
// guiMouseWheelMsg, g_bUseXML_Structures, gfDontUseDDBlits,
// gfIgnoreMessages -- now live in sgp.cpp directly; the WinMain
// rewrite made that TU unconditional.)

// ---- video.cpp (Phase 5) ---------------------------------------------------
UINT32  CurrentSurface          = 0;
INT32   giNumFrames             = 0;
BOOLEAN gfNextRefreshFullScreen = FALSE;
// gpFrameData (the framebuffer) -- not defined here; Phase 5 will
// allocate it when the SDL_Texture-backed renderer lands.

// (giMemUsedInSurfaces + ghFrameBuffer are now owned by sdl_vsurface.cpp.)

// Intro-screen globals moved back to Ja2/Intro.cpp in Phase 6u, when
// libsmacker took over for binkw32 and the Intro module stopped being
// Win32-only.

// ---- WinFont (retired) -----------------------------------------------------
// WinFont.cpp (the GDI + DirectDraw font rasterizer) is no longer
// compiled on any platform, so define its exported globals here. The
// iUseWinFonts path stays off; Phase 9 brings cross-platform text.
INT32   TOOLTIP_IFONT         = -1;
INT32   TOOLTIP_IFONT_BOLD    = -1;
#ifndef MAX_WINFONTMAP
#define MAX_WINFONTMAP 25
#endif
INT32   WinFontMap[MAX_WINFONTMAP] = {0};
