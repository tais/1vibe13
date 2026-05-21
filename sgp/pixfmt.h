#ifndef SGP_PIXFMT_H
#define SGP_PIXFMT_H

// Internal framebuffer pixel-format selector for the Phase 6b RGBA8888
// experiment.
//
//   SGP_PIXEL_DEPTH == 16  ->  transitional RGB565 (original JA2 format)
//   SGP_PIXEL_DEPTH == 32  ->  RGBA8888 (0xAARRGGBB, SDL_PIXELFORMAT_ARGB8888)
//
// All framebuffers, surfaces and blitters store `PIXEL` and route the
// format-specific per-pixel operations through the inlines below, so the
// rest of the engine is depth-agnostic and flipping the depth is contained
// to this header plus the palette-LUT and SDL-texture setup.
//
// While the migration is in progress the default stays 16 so the build is
// byte-for-byte the current RGB565 pipeline.

#include "types.h"

#ifndef SGP_PIXEL_DEPTH
#define SGP_PIXEL_DEPTH 32
#endif

#if SGP_PIXEL_DEPTH == 32
typedef UINT32 PIXEL;
#else
typedef UINT16 PIXEL;
#endif

#ifdef __cplusplus

// Self-contained externs (defined in shading.cpp, declared extern "C"
// there) so this header stays lightweight and free of ordering
// dependencies -- it gets pulled into low-level headers before
// SGPPaletteEntry/COLORVAL exist. Linkage must match shading.h.
extern "C" {
	extern FLOAT  guiShadePercent;        // shading.cpp
	extern UINT16 ShadeTable[65536];      // shading.cpp (16bpp dest-shade LUT)
	extern UINT16 IntensityTable[65536];  // shading.cpp (16bpp intensity LUT)
}

// Drop-shadow / fade darkening of a destination pixel. At 16bpp this is the
// precomputed 64K ShadeTable lookup; at 32bpp a 2^32 table is impossible so
// we darken each channel by the same factor the table was built from
// (guiShadePercent, see shading.cpp BuildShadeTable). IntensityTable uses a
// fixed 0.80 factor (BuildIntensityTable).

inline PIXEL PixShade(PIXEL p)
{
#if SGP_PIXEL_DEPTH == 32
	const UINT32 a =  p & 0xFF000000u;
	const UINT32 r = (UINT32)(((p >> 16) & 0xFFu) * guiShadePercent);
	const UINT32 g = (UINT32)(((p >>  8) & 0xFFu) * guiShadePercent);
	const UINT32 b = (UINT32)(( p        & 0xFFu) * guiShadePercent);
	return a | (r << 16) | (g << 8) | b;
#else
	return ShadeTable[p];
#endif
}

inline PIXEL PixIntensity(PIXEL p)
{
#if SGP_PIXEL_DEPTH == 32
	const float k = 0.80f;
	const UINT32 a =  p & 0xFF000000u;
	const UINT32 r = (UINT32)(((p >> 16) & 0xFFu) * k);
	const UINT32 g = (UINT32)(((p >>  8) & 0xFFu) * k);
	const UINT32 b = (UINT32)(( p        & 0xFFu) * k);
	return a | (r << 16) | (g << 8) | b;
#else
	return IntensityTable[p];
#endif
}

// Expand a "logical" RGB565 colour (what the engine computes everywhere via
// Get16BPPColor and stores in UINT16s) into a screen-format PIXEL. At 16bpp
// this is the identity; at 32bpp it widens RGB565 -> opaque ARGB8888 so the
// hundreds of UINT16-typed UI colours (fills, lines, boxes, hatch/pixelate)
// land as real colours instead of near-black low bits. 0x0000 stays
// 0x00000000 so colour-key transparency still matches.
inline PIXEL PixFromColor16(UINT32 c)
{
#if SGP_PIXEL_DEPTH == 32
	// A colour may already be full ARGB8888 (its alpha/high bits set) now that
	// Get16BPPColor returns true colour. RGB565 tokens are always 0..0xFFFF, so
	// a non-zero top half means "already expanded" -> pass through unchanged.
	if (c & 0xFFFF0000u) return c;
	if (c == 0) return 0; // preserve transparent-black key
	const UINT32 r5 = (c >> 11) & 0x1Fu;
	const UINT32 g6 = (c >>  5) & 0x3Fu;
	const UINT32 b5 =  c        & 0x1Fu;
	const UINT32 r8 = (r5 << 3) | (r5 >> 2);
	const UINT32 g8 = (g6 << 2) | (g6 >> 4);
	const UINT32 b8 = (b5 << 3) | (b5 >> 2);
	return 0xFF000000u | (r8 << 16) | (g8 << 8) | b8;
#else
	return c;
#endif
}

// 50% blend of a source pixel over a destination pixel (the translucency
// blitters' lo-bit-strip trick). At 16bpp this is the classic
// "(s>>1)+(d>>1)" with the 5/6/5 carry mask (guiTranslucentMask, 0x3DEF);
// at 32bpp it is the per-channel equivalent with an 8/8/8 carry mask.
inline PIXEL PixBlend50(PIXEL src, PIXEL dst)
{
#if SGP_PIXEL_DEPTH == 32
	return 0xFF000000u | ((((UINT32)src >> 1) & 0x7F7F7Fu) + (((UINT32)dst >> 1) & 0x7F7F7Fu));
#else
	return (PIXEL)((((UINT32)src >> 1) & 0x3DEFu) + (((UINT32)dst >> 1) & 0x3DEFu));
#endif
}

#endif // __cplusplus

#endif // SGP_PIXFMT_H
