// Definitions for globals that the JA2 codebase reads by extern but
// whose owning subsystem hasn't been ported off DirectDraw/Win32 yet.
// As each Phase replaces a subsystem with an SDL3-backed
// implementation, the corresponding globals move out of this file
// into the new implementation TUs.

#include "types.h"

// ---- sgp.cpp (Phase 3) -----------------------------------------------------
BOOLEAN gfProgramIsRunning   = TRUE;
BOOLEAN gfDontUseDDBlits     = FALSE;
BOOLEAN gfApplicationActive  = TRUE;
BOOLEAN gfGameInitialized    = FALSE;
BOOLEAN gfIgnoreMessages     = FALSE;
CHAR8   gzCommandLine[100]   = {0};
CHAR8   gzErrorMsg[2048]     = {0};
int     iWindowedMode        = 0;
UINT32  guiMouseWheelMsg     = 0;
bool    g_bUseXML_Structures = false;

// ---- video.cpp (Phase 5) ---------------------------------------------------
UINT32  CurrentSurface          = 0;
INT32   giNumFrames             = 0;
BOOLEAN gfNextRefreshFullScreen = FALSE;
// gpFrameData (the framebuffer) -- not defined here; Phase 5 will
// allocate it when the SDL_Texture-backed renderer lands.

// (giMemUsedInSurfaces + ghFrameBuffer are now owned by sdl_vsurface.cpp.)

// ---- Intro.cpp (Phase 8) ---------------------------------------------------
UINT32  gbIntroScreenMode     = 0;
BOOLEAN gfIntroScreenExit     = FALSE;
UINT32  guiIntroExitScreen    = 0;

// ---- WinFont.cpp (Phase 9) -------------------------------------------------
INT32   TOOLTIP_IFONT         = -1;
INT32   TOOLTIP_IFONT_BOLD    = -1;
#ifndef MAX_WINFONTMAP
#define MAX_WINFONTMAP 25
#endif
INT32   WinFontMap[MAX_WINFONTMAP] = {0};
