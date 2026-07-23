#ifndef ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_BACKEND_H
#define ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_BACKEND_H

// Raw SDL presentation entry points. Only PlatformFramePresenter may consume
// these; game and compatibility callers use FramePresenter or the established
// RefreshScreen/PresentNow gateway.
void PlatformVideoPresentPaced();
void PlatformVideoPresentImmediate();

#endif
