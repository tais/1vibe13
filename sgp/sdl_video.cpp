// SDL3-backed video manager. Phase 5 first slice. Replaces the
// DirectDraw-driven video.cpp (which compiles to an empty TU on
// non-Windows). The legacy public API (Lock*Buffer / Unlock*Buffer /
// InvalidateRegion* / RefreshScreen / etc.) is preserved so callers
// don't need to change; under the hood every "surface" is just a
// heap RGB565 buffer (UINT16*) and the framebuffer also has a
// matching SDL_Texture(SDL_PIXELFORMAT_RGB565, STREAMING) so the GPU
// gets to do the present.
//
// What's NOT here yet (still stubbed in sgp_non_win32_stubs.cpp):
//   - vsurface.cpp (the surface manager) -- Phase 5 second slice.
//   - mouse cursor compositing (we treat the OS cursor as authoritative
//     and let SDL render it).
//   - 8bpp palette upload (no 8bpp renderer in this slice).
//   - video capture (gpFrameData[]) -- the replay feature.
//
// As subsequent slices land, the stubs in sgp_non_win32_stubs.cpp for
// their respective subsystems get deleted.

#include "types.h"
#include "video.h"
#include "vsurface.h"
#include "DEBUG.H"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <algorithm>

extern UINT16 SCREEN_WIDTH;
extern UINT16 SCREEN_HEIGHT;

// ---- State -----------------------------------------------------------------

static SDL_Window*   gWindow    = nullptr;
static SDL_Renderer* gRenderer  = nullptr;
static SDL_Texture*  gFrameTex  = nullptr;

// Heap RGB565 buffers, one per logical surface. Pitch is always
// SCREEN_WIDTH * sizeof(UINT16) (no row padding).
static UINT16* gFrameBuffer = nullptr;
static UINT16* gBackBuffer  = nullptr;
static UINT16* gMouseBuf    = nullptr;
static int     gMouseBufW   = MAX_CURSOR_WIDTH;
static int     gMouseBufH   = MAX_CURSOR_HEIGHT;

// Accumulated dirty rect (in framebuffer pixels). Empty when L>=R.
static INT32 gDirtyL = 0, gDirtyT = 0, gDirtyR = 0, gDirtyB = 0;

// Optional frame-end callback the game uses for overlays.
static void (*gRefreshOverride)() = nullptr;

// ---- Globals declared in video.h ------------------------------------------
SGPPaletteEntry gSgpPalette[256] = {};
UINT32 guiFrameBufferState = BUFFER_READY;
UINT32 guiMouseBufferState = BUFFER_DISABLED;

// ---- Helpers --------------------------------------------------------------

// msvc_compat.h defines `min` and `max` as preprocessor macros for
// Win32-API parity; using std::min / std::max would get eaten. Parens
// suppress the macro at the call site.
static void ExpandDirtyRect(INT32 L, INT32 T, INT32 R, INT32 B)
{
	if (L < 0) L = 0;
	if (T < 0) T = 0;
	if (R > (INT32)SCREEN_WIDTH)  R = SCREEN_WIDTH;
	if (B > (INT32)SCREEN_HEIGHT) B = SCREEN_HEIGHT;
	if (L >= R || T >= B) return;
	if (gDirtyL >= gDirtyR || gDirtyT >= gDirtyB) {
		gDirtyL = L; gDirtyT = T; gDirtyR = R; gDirtyB = B;
	} else {
		if (L < gDirtyL) gDirtyL = L;
		if (T < gDirtyT) gDirtyT = T;
		if (R > gDirtyR) gDirtyR = R;
		if (B > gDirtyB) gDirtyB = B;
	}
}

static void DirtyFullScreen()
{
	gDirtyL = 0; gDirtyT = 0;
	gDirtyR = SCREEN_WIDTH; gDirtyB = SCREEN_HEIGHT;
}

// ---- Public API -----------------------------------------------------------

BOOLEAN InitializeVideoManager(void)
{
	if (SCREEN_WIDTH == 0)  SCREEN_WIDTH  = 640;
	if (SCREEN_HEIGHT == 0) SCREEN_HEIGHT = 480;

	if (!SDL_WasInit(SDL_INIT_VIDEO) && !SDL_Init(SDL_INIT_VIDEO)) {
		std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return FALSE;
	}

	gWindow = SDL_CreateWindow(
		"Jagged Alliance 2 1.13 (SDL3 port)",
		SCREEN_WIDTH, SCREEN_HEIGHT,
		SDL_WINDOW_RESIZABLE);
	if (!gWindow) {
		std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return FALSE;
	}

	gRenderer = SDL_CreateRenderer(gWindow, nullptr);
	if (!gRenderer) {
		std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
		return FALSE;
	}

	gFrameTex = SDL_CreateTexture(gRenderer,
		SDL_PIXELFORMAT_RGB565,
		SDL_TEXTUREACCESS_STREAMING,
		SCREEN_WIDTH, SCREEN_HEIGHT);
	if (!gFrameTex) {
		std::fprintf(stderr, "SDL_CreateTexture(RGB565) failed: %s\n",
		             SDL_GetError());
		return FALSE;
	}
	SDL_SetTextureScaleMode(gFrameTex, SDL_SCALEMODE_NEAREST);

	const size_t fbBytes = (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UINT16);
	gFrameBuffer = (UINT16*)std::calloc(1, fbBytes);
	gBackBuffer  = (UINT16*)std::calloc(1, fbBytes);
	gMouseBuf    = (UINT16*)std::calloc(1, (size_t)gMouseBufW * gMouseBufH * sizeof(UINT16));
	if (!gFrameBuffer || !gBackBuffer || !gMouseBuf) {
		std::fprintf(stderr, "video: heap buffer alloc failed\n");
		return FALSE;
	}

	DirtyFullScreen();
	guiFrameBufferState = BUFFER_READY;
	guiMouseBufferState = BUFFER_DISABLED;
	return TRUE;
}

void ShutdownVideoManager(void)
{
	std::free(gFrameBuffer); gFrameBuffer = nullptr;
	std::free(gBackBuffer);  gBackBuffer  = nullptr;
	std::free(gMouseBuf);    gMouseBuf    = nullptr;
	if (gFrameTex) { SDL_DestroyTexture(gFrameTex); gFrameTex = nullptr; }
	if (gRenderer) { SDL_DestroyRenderer(gRenderer); gRenderer = nullptr; }
	if (gWindow)   { SDL_DestroyWindow(gWindow);   gWindow   = nullptr; }
}

void    SuspendVideoManager(void) {}
BOOLEAN RestoreVideoManager(void) { return TRUE; }

void GetCurrentVideoSettings(UINT16* w, UINT16* h, UINT8* depth)
{
	if (w)     *w     = SCREEN_WIDTH;
	if (h)     *h     = SCREEN_HEIGHT;
	if (depth) *depth = 16;
}

BOOLEAN CanBlitToFrameBuffer(void) { return TRUE; }
BOOLEAN CanBlitToMouseBuffer(void) { return TRUE; }

void InvalidateRegion(INT32 L, INT32 T, INT32 R, INT32 B)
{
	ExpandDirtyRect(L, T, R, B);
}

void InvalidateRegions(SGPRect* rects, UINT32 n)
{
	if (!rects) return;
	for (UINT32 i = 0; i < n; ++i) {
		ExpandDirtyRect(rects[i].iLeft, rects[i].iTop,
		                rects[i].iRight, rects[i].iBottom);
	}
}

void InvalidateScreen(void)       { DirtyFullScreen(); }
void InvalidateFrameBuffer(void)  { DirtyFullScreen(); guiFrameBufferState = BUFFER_DIRTY; }

void InvalidateRegionEx(INT32 L, INT32 T, INT32 R, INT32 B, UINT32 /*flags*/)
{
	ExpandDirtyRect(L, T, R, B);
}

void SetFrameBufferRefreshOverride(PTR cb)
{
	gRefreshOverride = (void(*)())cb;
}

// All four "surface" locks return their corresponding heap buffer +
// the width-in-bytes pitch. JA2 doesn't actually exercise the
// primary/back distinction on the SDL3 path because the renderer
// owns presentation; the buffers are kept separate to preserve the
// game's expectations and to leave room for a later split if needed.
PTR  LockPrimarySurface(UINT32* pitch) { if (pitch) *pitch = SCREEN_WIDTH * sizeof(UINT16); return gFrameBuffer; }
void UnlockPrimarySurface(void)        {}

PTR  LockBackBuffer(UINT32* pitch)     { if (pitch) *pitch = SCREEN_WIDTH * sizeof(UINT16); return gBackBuffer; }
void UnlockBackBuffer(void)            {}

PTR  LockFrameBuffer(UINT32* pitch)    { if (pitch) *pitch = SCREEN_WIDTH * sizeof(UINT16); return gFrameBuffer; }
void UnlockFrameBuffer(void)           {}

PTR  LockMouseBuffer(UINT32* pitch)    { if (pitch) *pitch = gMouseBufW * sizeof(UINT16); return gMouseBuf; }
void UnlockMouseBuffer(void)           {}

BOOLEAN GetRGBDistribution(void) { return TRUE; }

BOOLEAN GetPrimaryRGBDistributionMasks(UINT32* r, UINT32* g, UINT32* b)
{
	if (r) *r = 0xF800;
	if (g) *g = 0x07E0;
	if (b) *b = 0x001F;
	return TRUE;
}

BOOLEAN Set8BPPPalette(SGPPaletteEntry* pal)
{
	if (pal) std::memcpy(gSgpPalette, pal, sizeof(gSgpPalette));
	return TRUE;
}

void StartFrameBufferRender(void) {}
void EndFrameBufferRender(void)   {}

// Push the dirty region of gFrameBuffer to the streaming texture and
// present. If the override callback is set (the game uses this for
// in-flight overlays / fades), call it first.
void RefreshScreen(void* /*dummy*/)
{
	if (!gRenderer || !gFrameTex || !gFrameBuffer) return;

	if (gRefreshOverride) gRefreshOverride();

	if (gDirtyL < gDirtyR && gDirtyT < gDirtyB) {
		SDL_Rect rc { gDirtyL, gDirtyT, gDirtyR - gDirtyL, gDirtyB - gDirtyT };
		const UINT16* src = gFrameBuffer + gDirtyT * SCREEN_WIDTH + gDirtyL;
		SDL_UpdateTexture(gFrameTex, &rc, src,
		                  SCREEN_WIDTH * sizeof(UINT16));
		gDirtyL = gDirtyT = gDirtyR = gDirtyB = 0;
	}

	SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
	SDL_RenderClear(gRenderer);
	SDL_RenderTexture(gRenderer, gFrameTex, nullptr, nullptr);
	SDL_RenderPresent(gRenderer);

	guiFrameBufferState = BUFFER_READY;
}

// ---- Mouse cursor & misc (minimal so the stubs go away) -------------------

BOOLEAN EraseMouseCursor(void) { return TRUE; }
BOOLEAN SetMouseCursorProperties(INT16, INT16, UINT16 w, UINT16 h) {
	if (w > 0 && w <= MAX_CURSOR_WIDTH)  gMouseBufW = w;
	if (h > 0 && h <= MAX_CURSOR_HEIGHT) gMouseBufH = h;
	return TRUE;
}
BOOLEAN SetMouseCursorFromObject(UINT32, UINT16, UINT16, UINT16) { return TRUE; }
BOOLEAN HideMouseCursor(void) { return TRUE; }
BOOLEAN LoadCursorFile(STR8) { return TRUE; }
BOOLEAN SetCurrentCursor(UINT16, UINT16, UINT16) { return TRUE; }
BOOLEAN BltToMouseCursor(UINT32, UINT16, UINT16, UINT16) { return TRUE; }
void    DirtyCursor(void) {}
void    EnableCursor(BOOLEAN) {}

void VideoCaptureToggle(void) {}

// PrintScreen: SDL3 has SDL_RenderReadPixels + SDL_SaveBMP_IO which
// could give a real screenshot path here, but JA2 only invokes this
// from a JA2BETAVERSION-only hotkey. Stub for now.
void PrintScreen(void) {}

// FatalError: route through DEBUG.cpp's logger and abort. JA2 uses
// this from a handful of asset-load failures; on non-Windows builds
// the game loop isn't even reached yet so this is largely belt-and-
// braces.
void FatalError(const STR8 fmt, ...)
{
	if (fmt) {
		va_list ap;
		va_start(ap, fmt);
		std::vfprintf(stderr, fmt, ap);
		va_end(ap);
		std::fputc('\n', stderr);
	}
	// std::exit instead of std::abort so the OS doesn't pop a crash
	// report dialog -- this path is the normal "we hit something we
	// can't recover from" route, not an unexpected segfault.
	std::exit(1);
}

