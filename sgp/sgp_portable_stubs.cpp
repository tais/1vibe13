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

// (Blitter stubs moved into sdl_vsurface.cpp: BltVideoSurface,
// BltVideoSurfaceToVideoSurface, BltStretchVideoSurface,
// BltVSurfaceUsingDD, ColorFillVideoSurfaceArea got CPU
// implementations; ImageFillVideoSurfaceArea / Shadow* still return
// FALSE pending the Phase 6 RGB565 blender retirement.)

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
// Phase 8 will land real cinematic playback. Until then these need to
// return TRUE on init/shutdown (the screen registration loop treats
// FALSE as a fatal error) and a benign screen id on handle.
UINT32 IntroScreenInit()         { return 1; }
UINT32 IntroScreenHandle()       { return 0; }
UINT32 IntroScreenShutdown()     { return 1; }
void   SetIntroType(INT8)        {}
void   StopIntroVideo()          {}
void   DisplaySirtechSplashScreen() {}

// ---- connect.h (Multiplayer) -----------------------------------------------
bool can_edgechange()            { return false; }
