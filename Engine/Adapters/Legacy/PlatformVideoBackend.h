#ifndef ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_BACKEND_H
#define ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_BACKEND_H

#include <cstdint>

// Raw SDL frame-output entry points. Only the platform frame adapters consume
// these; game and compatibility callers use the engine contracts or their
// established RefreshScreen/PresentNow/Invalidate* gateways.
void PlatformVideoPresentPaced();
void PlatformVideoPresentImmediate();
void PlatformVideoInvalidateRegion(
	std::int32_t left, std::int32_t top,
	std::int32_t right, std::int32_t bottom);
void PlatformVideoInvalidateAll();
void PlatformVideoMarkFrameChanged();

#endif
