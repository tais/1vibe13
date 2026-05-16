// Non-Windows stub bodies for symbols whose real implementations live
// inside _WIN32-gated subsystems (vsurface.cpp, video.cpp, Intro.cpp,
// soundman.cpp, mousesystem). The bodies are intentionally empty /
// return defaults -- nothing on the non-Windows path drives the game
// loop yet, so these symbols are referenced (via input.cpp pulling
// transitively) but never actually called.
//
// As each Phase replaces its gated subsystem with an SDL3-backed
// implementation, the corresponding stub block below gets deleted and
// the real implementation provides the symbol.

#ifndef _WIN32

#include "types.h"
#include "vsurface.h"
#include "vobject.h"
#include "vobject_blitters.h"
#include "video.h"
#include "soundman.h"
#include "KeyMap.h"
#include "Intro.h"
#include "connect.h"

#include <cstdarg>

// ---- vsurface.cpp ----------------------------------------------------------
BOOLEAN AddStandardVideoSurface(VSURFACE_DESC*, UINT32*) { return FALSE; }
BOOLEAN GetVideoSurface(HVSURFACE*, UINT32) { return FALSE; }
BOOLEAN DeleteVideoSurfaceFromIndex(UINT32) { return FALSE; }
BOOLEAN BltVideoSurface(UINT32, UINT32, UINT16, INT32, INT32, UINT32, blt_vs_fx*) { return FALSE; }
BOOLEAN BltVideoSurfaceToVideoSurface(HVSURFACE, HVSURFACE, UINT16, INT32, INT32, INT32, blt_vs_fx*) { return FALSE; }
BOOLEAN BltStretchVideoSurface(UINT32, UINT32, INT32, INT32, UINT32, SGPRect*, SGPRect*) { return FALSE; }
BOOLEAN BltVSurfaceUsingDD(HVSURFACE, HVSURFACE, UINT32, INT32, INT32, RECT*) { return FALSE; }
BOOLEAN ColorFillVideoSurfaceArea(UINT32, INT32, INT32, INT32, INT32, UINT16) { return FALSE; }
BOOLEAN ImageFillVideoSurfaceArea(UINT32, INT32, INT32, INT32, INT32, HVOBJECT, UINT16, INT16, INT16) { return FALSE; }
BOOLEAN ShadowVideoSurfaceRect(UINT32, INT32, INT32, INT32, INT32) { return FALSE; }
BOOLEAN ShadowVideoSurfaceImage(UINT32, HVOBJECT, INT32, INT32) { return FALSE; }
BOOLEAN ShadowVideoSurfaceRectUsingLowPercentTable(UINT32, INT32, INT32, INT32, INT32) { return FALSE; }
BOOLEAN SetVideoSurfaceTransparency(UINT32, COLORVAL) { return FALSE; }
BOOLEAN GetVSurfacePaletteEntries(HVSURFACE, SGPPaletteEntry*) { return FALSE; }
BYTE*   LockVideoSurface(UINT32, UINT32*) { return nullptr; }
void    UnLockVideoSurface(UINT32) {}

namespace SurfaceData {
	BYTE* SetApplicationData(BYTE*)      { return nullptr; }
	void  ReleaseApplicationData(BYTE*)  {}
	tID   GetSurfaceID(BYTE*)            { return 0; }
}

ClipRectangle::ClipRectangle() {}
void ClipRectangle::SetRect(SGPRect const&) {}
void ClipRectangle::SetRect(unsigned int, unsigned int, int, int) {}
ClipRectangle::ClipType ClipRectangle::Clip(int&, int&, unsigned int&, unsigned int&) { return NoClip; }
ClipRectangle::ClipType ClipRectangle::Clip(int&, int&, int&, int&) { return NoClip; }
void ClipRectangle::Set(int, int, int, int) {}

// ---- video.cpp -------------------------------------------------------------
void FatalError(const STR8, ...) {}
void DirtyCursor() {}
void PrintScreen() {}
void RefreshScreen(void*) {}
void InvalidateRegion(INT32, INT32, INT32, INT32) {}
void InvalidateScreen() {}
void InvalidateRegionEx(INT32, INT32, INT32, INT32, UINT32) {}
void StartFrameBufferRender() {}
void EndFrameBufferRender() {}
void VideoCaptureToggle() {}
void GetCurrentVideoSettings(UINT16*, UINT16*, UINT8*) {}
BOOLEAN GetPrimaryRGBDistributionMasks(UINT32*, UINT32*, UINT32*) { return FALSE; }
BOOLEAN Set8BPPPalette(SGPPaletteEntry*) { return FALSE; }
BOOLEAN EraseMouseCursor() { return FALSE; }
BOOLEAN SetMouseCursorProperties(INT16, INT16, UINT16, UINT16) { return FALSE; }
BOOLEAN SetCurrentCursor(UINT16, UINT16, UINT16) { return FALSE; }
PTR  LockMouseBuffer(UINT32*) { return nullptr; }
void UnlockMouseBuffer() {}

// ---- soundman.cpp (extends the existing non-Win32 stubs there) -------------
UINT32 SoundLoadSample(STR)      { return 0xFFFFFFFFu; }
UINT32 SoundLockSample(STR)      { return 0xFFFFFFFFu; }
UINT32 SoundUnlockSample(STR)    { return 0xFFFFFFFFu; }
void   SoundRemoveSampleFlags(UINT32, UINT32) {}
void   ResetSoundMap()           {}

// ---- KeyMap (Utils/KeyMap.cpp) --------------------------------------------
BOOLEAN IsKeyPressed(INT32)      { return FALSE; }
INT32   ParseKeyString(STR8)     { return 0; }

// ---- Intro (Ja2/Intro.cpp) -------------------------------------------------
UINT32 IntroScreenInit()         { return 0; }
UINT32 IntroScreenHandle()       { return 0; }
UINT32 IntroScreenShutdown()     { return 0; }
void   SetIntroType(INT8)        {}
void   StopIntroVideo()          {}
void   DisplaySirtechSplashScreen() {}

// ---- connect.h (Multiplayer) -----------------------------------------------
bool can_edgechange()            { return false; }

#endif // !_WIN32
