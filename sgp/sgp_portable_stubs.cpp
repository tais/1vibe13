// Stub bodies for symbols whose real implementations still live
// inside DirectDraw / Win32-specific subsystems that haven't been
// rewritten on SDL3 yet (the SGPVSurface manager + blitters in
// vsurface.cpp, intro video, audio sample loaders, KeyMap, the
// multiplayer connect surface). The bodies are intentionally empty /
// return defaults -- nothing in the current path actually drives them
// yet, so these symbols are referenced (via input.cpp pulling
// transitively) but never called.
//
// As each Phase replaces its subsystem with a portable SDL3-backed
// implementation, the corresponding stub block here gets deleted and
// the real implementation provides the symbol.

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

// ---- vsurface.cpp blitters (manager moved into sdl_vsurface.cpp) ----------
BOOLEAN BltVideoSurface(UINT32, UINT32, UINT16, INT32, INT32, UINT32, blt_vs_fx*) { return FALSE; }
BOOLEAN BltVideoSurfaceToVideoSurface(HVSURFACE, HVSURFACE, UINT16, INT32, INT32, INT32, blt_vs_fx*) { return FALSE; }
BOOLEAN BltStretchVideoSurface(UINT32, UINT32, INT32, INT32, UINT32, SGPRect*, SGPRect*) { return FALSE; }
BOOLEAN BltVSurfaceUsingDD(HVSURFACE, HVSURFACE, UINT32, INT32, INT32, RECT*) { return FALSE; }
BOOLEAN ColorFillVideoSurfaceArea(UINT32, INT32, INT32, INT32, INT32, UINT16) { return FALSE; }
BOOLEAN ImageFillVideoSurfaceArea(UINT32, INT32, INT32, INT32, INT32, HVOBJECT, UINT16, INT16, INT16) { return FALSE; }
BOOLEAN ShadowVideoSurfaceRect(UINT32, INT32, INT32, INT32, INT32) { return FALSE; }
BOOLEAN ShadowVideoSurfaceImage(UINT32, HVOBJECT, INT32, INT32) { return FALSE; }
BOOLEAN ShadowVideoSurfaceRectUsingLowPercentTable(UINT32, INT32, INT32, INT32, INT32) { return FALSE; }

// (video.cpp stubs moved into sdl_video.cpp -- it now provides real
// SDL3-backed implementations of FatalError, DirtyCursor, PrintScreen,
// RefreshScreen, InvalidateRegion*, StartFrameBufferRender,
// EndFrameBufferRender, VideoCaptureToggle, GetCurrentVideoSettings,
// GetPrimaryRGBDistributionMasks, Set8BPPPalette, EraseMouseCursor,
// SetMouseCursorProperties, SetCurrentCursor, LockMouseBuffer,
// UnlockMouseBuffer, and the rest of video.h's public surface.)

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
