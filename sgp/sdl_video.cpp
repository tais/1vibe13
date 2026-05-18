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
#include "himage.h"
#include "input.h"   // gusMouseXPos / gusMouseYPos for cursor composite
#include "sysutil.h" // guiSAVEBUFFER
#include "DEBUG.H"

// RenderStaticWorldRect: tile-engine entry point we use to paint the
// new strip that scrolled into view (lives in TileEngine/renderworld.cpp).
// Declared here instead of including the header to keep the video <-
// tile-engine dependency loose.
extern void RenderStaticWorldRect(INT16 sLeft, INT16 sTop, INT16 sRight, INT16 sBottom, BOOLEAN fDynamicsToo);

// Scroll-shift integration: these globals live in TileEngine and let
// us know how far the camera moved between the previous frame and the
// upcoming render. JA2 1.13's ScrollBackground accumulates the scroll
// magnitude (always positive) into gsScrollX/YIncrement and records
// the direction(s) bitwise in guiScrollDirection (SCROLL_LEFT bit
// 0x08, SCROLL_RIGHT 0x04, SCROLL_UP 0x01, SCROLL_DOWN 0x02). We
// reconstruct the signed delta from those, then shift the previous
// frame's pixels accordingly so JA2's static-world render can paint
// over them cleanly with no trail at the old screen positions.
extern INT16 gsScrollXIncrement;
extern INT16 gsScrollYIncrement;
extern INT32 guiScrollDirection;
extern BOOLEAN gfRenderScroll;
extern INT16 gsVIEWPORT_START_X;
extern INT16 gsVIEWPORT_END_X;
extern INT16 gsVIEWPORT_WINDOW_START_Y;
extern INT16 gsVIEWPORT_WINDOW_END_Y;

// Pulled from renderworld.h's SCROLL_* defines (we don't include the
// header to keep video <- tile-engine dependencies one-way).
#define SCROLL_UP    0x00000001
#define SCROLL_DOWN  0x00000002
#define SCROLL_RIGHT 0x00000004
#define SCROLL_LEFT  0x00000008

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
// Hotspot offset within gMouseBuf -- the "click point" of the cursor
// sprite relative to its top-left. Captured by SetMouseCursorProperties
// from the cursor database; composited at (mouseX - hotX, mouseY - hotY).
static INT16   gMouseCursorHotX = 0;
static INT16   gMouseCursorHotY = 0;

// Magenta-ish sentinel in RGB565 marking "transparent" pixels in
// gMouseBuf. ETRLE blits only write opaque pixels; surrounding area
// keeps this value and the composite below skips it. Chosen to be a
// colour real cursor sprites don't contain (full red + full blue, no
// green) so the cursor's black outline pixels are preserved -- using
// 0x0000 as the key ate them and left the cursor looking fragmented
// against dark backgrounds.
static constexpr UINT16 kCursorTransparent = 0xF81F;

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

	// JA2 renders its own cursor (crosshairs, walk, look, etc.) into
	// gMouseBuf and we composite it onto the framebuffer below; hide
	// the OS arrow so it doesn't double up on top.
	if (!SDL_HideCursor()) {
		std::fprintf(stderr, "[video] SDL_HideCursor() failed: %s\n", SDL_GetError());
	}

	const size_t fbBytes = (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(UINT16);
	gFrameBuffer = (UINT16*)std::calloc(1, fbBytes);
	gBackBuffer  = (UINT16*)std::calloc(1, fbBytes);
	gMouseBuf    = (UINT16*)std::calloc(1, (size_t)gMouseBufW * gMouseBufH * sizeof(UINT16));
	if (!gFrameBuffer || !gBackBuffer || !gMouseBuf) {
		std::fprintf(stderr, "video: heap buffer alloc failed\n");
		return FALSE;
	}

	// Initialize the RGB565 mask + shift globals from himage.cpp. The
	// legacy code queried DirectDraw for these and JA2's color /
	// palette / shade-table code reads them in Get16BPPColor to
	// convert 8-bit-per-channel RGB triples into RGB565 pixels:
	//     r16 = (r << gusRedShift)   & gusRedMask
	//     g16 = (g << gusGreenShift) & gusGreenMask
	//     b16 = (b << gusBlueShift)  & gusBlueMask
	// For RGB565 (R in bits 15-11, G in 10-5, B in 4-0), an 8-bit r
	// becomes 5-bit r by shifting left 8 (then masking with 0xF800);
	// 8-bit g becomes 6-bit g shifted left 3 (masked 0x07E0); 8-bit
	// b becomes 5-bit b shifted right 3 (masked 0x001F).
	// Without these, every Get16BPPColor() returned 0 (or close to
	// 0), making the rendered screen nearly all black or, with masks
	// alone, blue-tinted.
	gusRedMask   = 0xF800;
	gusGreenMask = 0x07E0;
	gusBlueMask  = 0x001F;
	gusRedShift   = 8;
	gusGreenShift = 3;
	gusBlueShift  = -3;

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

// Fixed pitch -- gMouseBuf is always allocated at MAX_CURSOR_WIDTH so
// every cursor (regardless of its declared width via
// SetMouseCursorProperties) lives in the same backing layout. Returning
// a pitch that tracks gMouseBufW would have ETRLE write small-stride
// data into a 64-wide allocation, causing the next row to overlap the
// previous row's tail and producing sheared/"mirrored"-looking cursors.
PTR  LockMouseBuffer(UINT32* pitch)    { if (pitch) *pitch = MAX_CURSOR_WIDTH * sizeof(UINT16); return gMouseBuf; }
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

// "Scroll happened" hook. When the camera is moving (scroll inertia)
// we promote that into a RENDER_FLAG_FULL on the engine's render-flag
// bitmask so RenderWorld this frame does a full repaint -- the
// streaming-texture pipeline has no persistent back-buffer for the
// engine's incremental render to rely on across camera shifts.
//
// We check ALL three scroll-active conditions because they fire on
// different frames within a single user-held-arrow-key:
//   - gfRenderScroll: set by ScrollBackground on the frames where
//     NEXTSCROLL counter actually commits a scroll step (1 per N
//     frames -- the camera advances in discrete steps).
//   - gfScrollInertia: TRUE throughout the entire scroll, including
//     the in-between frames where the camera hasn't advanced yet
//     (we still need FULL on those frames because the engine's
//     RenderDynamicWorld will compute merc screen positions from the
//     CURRENT camera, and if RenderStaticWorld isn't keeping pace the
//     merc visibly drifts over a stale background -- exactly the
//     "merc warps with a trail" symptom).
//   - gfScrollPending: TRUE on the very first scroll-input frame
//     before inertia kicks in.
//
// We deliberately do NOT memmove the framebuffer pixels in lockstep
// with the camera the way stracciatella's ScrollJA2Background does --
// screen-fixed overlays drawn into FRAME_BUFFER outside of the world
// render (ScrollString status messages, merc dialogue face + text,
// etc.) get shifted along with everything else and leave ghosts. A
// full re-render every scroll frame is heavier but visually clean.
//
// All scroll-delta globals get reset here so they don't accumulate
// between frames.
extern BOOLEAN gfScrollInertia;
extern BOOLEAN gfScrollPending;

static void ShiftFrameBufferForScroll()
{
	const bool scrolled = (gfRenderScroll
		|| gfScrollInertia
		|| gfScrollPending
		|| gsScrollXIncrement != 0
		|| gsScrollYIncrement != 0);
	gsScrollXIncrement = 0;
	gsScrollYIncrement = 0;
	gfRenderScroll = FALSE;
	guiScrollDirection = 0;
	if (scrolled) {
		// RENDER_FLAG_FULL = 0x1 (renderworld.h). NOTE: an earlier draft
		// of this hook hard-coded the value as 0x4, which is actually
		// RENDER_FLAG_MARKED -- the symptom was "merc warps with a trail,
		// map only updates when scroll stops" because RenderWorld took
		// the MARKED branch (RenderMarkedWorld renders nothing unless
		// MAPELEMENT_REDRAW is set on a tile) instead of the FULL branch.
		extern void SetRenderFlags(UINT32 uiFlags);
		SetRenderFlags(0x00000001 /*RENDER_FLAG_FULL*/);
	}
}

void StartFrameBufferRender(void) {}
void EndFrameBufferRender(void)   {}

// Explicit hook called from gamescreen.cpp's HandleTacticalScreen
// between ScrollWorld() (which COMMITS the scroll and sets
// gsScrollX/YIncrement + guiScrollDirection) and RenderWorld() (which
// paints the new camera position into gFrameBuffer). Doing it here
// instead of StartFrameBufferRender ensures the shift matches the
// SAME frame's scroll delta -- not one frame stale.
void Sgp_ShiftFrameBufferForScroll(void) {
	ShiftFrameBufferForScroll();
}

// Cursor save buffer for the pixels we're about to overdraw, so we can
// restore them after the SDL upload and keep gFrameBuffer clean across
// frames. The main menu / laptop / map screens don't repaint their
// background every frame, so without save/restore the cursor would
// leave a trail wherever the mouse has been.
static UINT16 gCursorSavePixels[MAX_CURSOR_WIDTH * MAX_CURSOR_HEIGHT];
static INT32  gCursorSaveX0 = 0, gCursorSaveY0 = 0;
static INT32  gCursorSaveX1 = 0, gCursorSaveY1 = 0;
static INT32  gCursorStampDstX = 0, gCursorStampDstY = 0;

// Stamp gMouseBuf onto gFrameBuffer at the current mouse position using
// 0x0000 as the transparency key. Records the affected region so
// RestoreCursorPixels() can undo the write right after the SDL upload.
static void CompositeCursorOntoFramebuffer()
{
	gCursorSaveX0 = gCursorSaveX1 = 0;
	if (!gMouseBuf || !gFrameBuffer) return;
	if (gMouseBufW <= 0 || gMouseBufH <= 0) return;

	const INT32 dstX = (INT32)gusMouseXPos - (INT32)gMouseCursorHotX;
	const INT32 dstY = (INT32)gusMouseYPos - (INT32)gMouseCursorHotY;

	const INT32 srcX0 = (dstX < 0) ? -dstX : 0;
	const INT32 srcY0 = (dstY < 0) ? -dstY : 0;
	const INT32 srcX1 = (dstX + gMouseBufW > (INT32)SCREEN_WIDTH)
	                    ? ((INT32)SCREEN_WIDTH - dstX) : gMouseBufW;
	const INT32 srcY1 = (dstY + gMouseBufH > (INT32)SCREEN_HEIGHT)
	                    ? ((INT32)SCREEN_HEIGHT - dstY) : gMouseBufH;
	if (srcX0 >= srcX1 || srcY0 >= srcY1) return;

	// Save the framebuffer rect first so we can restore after upload.
	gCursorSaveX0 = srcX0; gCursorSaveY0 = srcY0;
	gCursorSaveX1 = srcX1; gCursorSaveY1 = srcY1;
	gCursorStampDstX = dstX; gCursorStampDstY = dstY;
	for (INT32 sy = srcY0; sy < srcY1; ++sy) {
		const UINT16* fbRow = gFrameBuffer + (size_t)(dstY + sy) * SCREEN_WIDTH + dstX;
		UINT16* saveRow = gCursorSavePixels + (size_t)sy * MAX_CURSOR_WIDTH;
		for (INT32 sx = srcX0; sx < srcX1; ++sx) saveRow[sx] = fbRow[sx];
	}

	// Stamp with color-key transparency. gMouseBuf is always laid out at
	// MAX_CURSOR_WIDTH stride (matches the LockMouseBuffer pitch ETRLE
	// blits use); the actual cursor sprite occupies only the top-left
	// gMouseBufW x gMouseBufH region.
	for (INT32 sy = srcY0; sy < srcY1; ++sy) {
		const UINT16* srcRow = gMouseBuf + (size_t)sy * MAX_CURSOR_WIDTH;
		UINT16* dstRow = gFrameBuffer + (size_t)(dstY + sy) * SCREEN_WIDTH + dstX;
		for (INT32 sx = srcX0; sx < srcX1; ++sx) {
			const UINT16 px = srcRow[sx];
			if (px != kCursorTransparent) dstRow[sx] = px;
		}
	}
}

// Reverse of CompositeCursorOntoFramebuffer: put the saved pixels back
// so gFrameBuffer matches whatever the game wrote, with no cursor
// artifact. Called right after SDL_UpdateTexture so the texture (and
// thus the on-screen frame) does include the cursor.
static void RestoreCursorPixels()
{
	if (gCursorSaveX0 >= gCursorSaveX1 || gCursorSaveY0 >= gCursorSaveY1) return;
	for (INT32 sy = gCursorSaveY0; sy < gCursorSaveY1; ++sy) {
		const UINT16* saveRow = gCursorSavePixels + (size_t)sy * MAX_CURSOR_WIDTH;
		UINT16* fbRow = gFrameBuffer + (size_t)(gCursorStampDstY + sy) * SCREEN_WIDTH + gCursorStampDstX;
		for (INT32 sx = gCursorSaveX0; sx < gCursorSaveX1; ++sx) fbRow[sx] = saveRow[sx];
	}
}

// Push gFrameBuffer to the streaming texture and present. If the
// override callback is set (the game uses this for in-flight overlays
// / fades), call it first.
void RefreshScreen(void* /*dummy*/)
{
	if (!gRenderer || !gFrameTex || !gFrameBuffer) return;

	// macOS sometimes lets the OS arrow reappear (after window focus
	// changes, etc.). Re-hide each frame so it stays consistently off
	// over our SDL-rendered surface.
	if (SDL_CursorVisible()) SDL_HideCursor();

	if (gRefreshOverride) gRefreshOverride();

	// Stamp -> upload -> un-stamp. The texture sees the cursor; the
	// framebuffer ends up unchanged so non-tactical screens don't get
	// a cursor trail across frames.
	CompositeCursorOntoFramebuffer();

	SDL_UpdateTexture(gFrameTex, nullptr, gFrameBuffer,
	                  SCREEN_WIDTH * sizeof(UINT16));
	gDirtyL = gDirtyT = gDirtyR = gDirtyB = 0;

	RestoreCursorPixels();

	SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
	SDL_RenderClear(gRenderer);
	SDL_RenderTexture(gRenderer, gFrameTex, nullptr, nullptr);
	SDL_RenderPresent(gRenderer);

	guiFrameBufferState = BUFFER_READY;
}

// ---- Mouse cursor & misc (minimal so the stubs go away) -------------------

BOOLEAN EraseMouseCursor(void) {
	if (!gMouseBuf) return TRUE;
	const size_t n = (size_t)MAX_CURSOR_WIDTH * MAX_CURSOR_HEIGHT;
	for (size_t i = 0; i < n; ++i) gMouseBuf[i] = kCursorTransparent;
	return TRUE;
}
BOOLEAN SetMouseCursorProperties(INT16 hotX, INT16 hotY, UINT16 usCursorHeight, UINT16 usCursorWidth) {
	// NB: the legacy header argument order is (hotX, hotY, HEIGHT, WIDTH)
	// -- height comes first. Earlier we had the last two reversed and
	// the resulting stride mismatch made ETRLE-decoded cursors wrap
	// every gMouseBufW pixels, producing fragmented/skewed sprites.
	gMouseCursorHotX = hotX;
	gMouseCursorHotY = hotY;
	if (usCursorWidth  > 0 && usCursorWidth  <= MAX_CURSOR_WIDTH)  gMouseBufW = usCursorWidth;
	if (usCursorHeight > 0 && usCursorHeight <= MAX_CURSOR_HEIGHT) gMouseBufH = usCursorHeight;
	return TRUE;
}
BOOLEAN SetMouseCursorFromObject(UINT32, UINT16, UINT16, UINT16) { return TRUE; }
BOOLEAN HideMouseCursor(void) {
	// Cursor visibility is implicit -- gMouseBuf with all-transparent
	// pixels composites to nothing. HideMouseCursor() callers also call
	// EraseMouseCursor() first, which already wipes the buffer.
	return TRUE;
}
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

