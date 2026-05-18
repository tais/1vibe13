#ifndef __VIDEO_
#define __VIDEO_

#ifdef _WIN32
#include <windows.h>
#include <ddraw.h>
#include <process.h>
#include "DirectDraw Calls.h"
#endif

#include "local.h"
#include "DEBUG.H"
#include "types.h"
#include "vsurface.h"

#define BUFFER_READY			0x00
#define BUFFER_BUSY			0x01
#define BUFFER_DIRTY			0x02
#define BUFFER_DISABLED		0x03

#define MAX_CURSOR_WIDTH		64
#define MAX_CURSOR_HEIGHT	 64
#define VIDEO_NO_CURSOR				0xFFFF


extern UINT32				 guiMouseBufferState;	// BUFFER_READY, BUFFER_DIRTY, BUFFER_DISABLED
//#ifdef WINFONTS
extern UINT32 CurrentSurface;
//#endif

#ifdef _WIN32
extern HWND										ghWindow;
extern BOOLEAN				InitializeVideoManager(HINSTANCE hInstance, UINT16 usCommandShow, void *WindowProc);
extern LPDIRECTDRAW2		GetDirectDraw2Object(void);
extern LPDIRECTDRAWSURFACE2 GetPrimarySurfaceObject(void);
extern LPDIRECTDRAWSURFACE2 GetBackBufferObject(void);
extern LPDIRECTDRAWSURFACE2 GetFrameBufferObject(void);
extern LPDIRECTDRAWSURFACE2 GetMouseBufferObject(void);
#else
extern BOOLEAN				InitializeVideoManager(void);
#endif

extern void				 ShutdownVideoManager(void);
extern void				 SuspendVideoManager(void);
extern BOOLEAN				RestoreVideoManager(void);
extern void				 GetCurrentVideoSettings(UINT16 *usWidth, UINT16 *usHeight, UINT8 *ubBitDepth);
extern BOOLEAN				CanBlitToFrameBuffer(void);
extern BOOLEAN				CanBlitToMouseBuffer(void);
extern void				 InvalidateRegion(INT32 iLeft, INT32 iTop, INT32 iRight, INT32 iBottom);
extern void				 InvalidateRegions(SGPRect *pArrayOfRegions, UINT32 uiRegionCount);
extern void				 InvalidateScreen(void);
extern void				 InvalidateFrameBuffer(void);
extern void				 SetFrameBufferRefreshOverride(PTR pFrameBufferRefreshOverride);

// SDL3 port: SDL-side shift of the framebuffer that mirrors stracciatella's
// ScrollJA2Background. Must be called AFTER ScrollWorld committed the camera
// move (which sets gsScrollX/YIncrement + guiScrollDirection) and BEFORE
// RenderWorld paints the new view -- so the shifted previous-frame pixels
// serve as a clean fallback for any iso tile gap.
extern void				 Sgp_ShiftFrameBufferForScroll(void);
extern PTR					LockPrimarySurface(UINT32 *uiPitch);
extern void				 UnlockPrimarySurface(void);
extern PTR					LockBackBuffer(UINT32 *uiPitch);
extern void				 UnlockBackBuffer(void);
extern PTR					LockFrameBuffer(UINT32 *uiPitch);
extern void				 UnlockFrameBuffer(void);
extern PTR					LockMouseBuffer(UINT32 *uiPitch);
extern void				 UnlockMouseBuffer(void);
extern BOOLEAN				GetRGBDistribution(void);
extern BOOLEAN				GetPrimaryRGBDistributionMasks(UINT32 *RedBitMask, UINT32 *GreenBitMask, UINT32 *BblueBitMask);
extern BOOLEAN							SetMouseCursorFromObject(UINT32 uiVideoObjectHandle, UINT16 usVideoObjectSubIndex, UINT16 usOffsetX, UINT16 usOffsetY );
extern BOOLEAN				HideMouseCursor(void);
extern BOOLEAN				LoadCursorFile(STR8 pFilename);
extern BOOLEAN							SetCurrentCursor(UINT16 usVideoObjectSubIndex,	UINT16 usOffsetX, UINT16 usOffsetY );
extern void				 StartFrameBufferRender(void);
extern void				 EndFrameBufferRender(void);
extern void				 PrintScreen(void);



extern BOOLEAN							EraseMouseCursor( );
extern BOOLEAN							SetMouseCursorProperties( INT16 sOffsetX, INT16 sOffsetY, UINT16 usCursorHeight, UINT16 usCursorWidth );
extern BOOLEAN							BltToMouseCursor(UINT32 uiVideoObjectHandle, UINT16 usVideoObjectSubIndex, UINT16 usXPos, UINT16 usYPos );
void												DirtyCursor( );
void												EnableCursor( BOOLEAN fEnable );

BOOLEAN											Set8BPPPalette(SGPPaletteEntry *pPalette);
// 8-bit palette globals

void												VideoCaptureToggle( void );


void InvalidateRegionEx(INT32 iLeft, INT32 iTop, INT32 iRight, INT32 iBottom, UINT32 uiFlags );

void RefreshScreen(void *DummyVariable);


void FatalError( const STR8 pError, ...);


extern SGPPaletteEntry			gSgpPalette[256];
#ifdef _WIN32
extern LPDIRECTDRAWPALETTE	gpDirectDrawPalette;
#endif

/*
#ifdef __cplusplus
}
#endif
*/

#endif
