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

// soundman.cpp stubs were here during the Phase 6 strangler-fig period
// (FMOD-only Windows path + silent macOS/Linux stubs). They moved into
// the real SDL3_mixer-backed soundman.cpp in Phase 6r, so this block
// is gone.

// ---- KeyMap (Utils/KeyMap.cpp) --------------------------------------------
BOOLEAN IsKeyPressed(INT32)      { return FALSE; }
INT32   ParseKeyString(STR8)     { return 0; }

// ---- Intro (Ja2/Intro.cpp) -------------------------------------------------
// Phase 8 will land real cinematic playback. Until then init/shutdown
// just need to return TRUE (the screen registration loop treats FALSE
// as a fatal error) and Handle has to:
//   1. flip gfDoneWithSplashScreen TRUE -- otherwise InitScreenHandle
//      bounces straight back to INTRO_SCREEN every tick and nothing
//      ever draws
//   2. return a next-screen id that gets us out of the intro --
//      INIT_SCREEN per the real INTRO_SPLASH path in Ja2/Intro.cpp
#include "screenids.h"
extern BOOLEAN gfDoneWithSplashScreen;
UINT32 IntroScreenInit()         { return 1; }
UINT32 IntroScreenHandle()       {
	gfDoneWithSplashScreen = TRUE;
	return INIT_SCREEN;
}
UINT32 IntroScreenShutdown()     { return 1; }
void   SetIntroType(INT8)        {}
void   StopIntroVideo()          {}
void   DisplaySirtechSplashScreen() {}

// ---- connect.h (Multiplayer) -----------------------------------------------
bool can_edgechange()            { return false; }
