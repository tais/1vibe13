#ifndef ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_SURFACE_BACKEND_H
#define ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_SURFACE_BACKEND_H

#include <cstdint>

// Raw SGP surface-manager entry points. Only PlatformRenderSurfaceAccess
// consumes these; game and compatibility callers use RenderSurfaceAccess or
// the established Get/Lock/UnLockVideoSurface functions.
bool PlatformVideoSurfaceDescribe(
	std::uint32_t surface,
	std::uint32_t& width,
	std::uint32_t& height,
	std::uint8_t& contentBitDepth,
	std::uint8_t& pixelBytes);
std::uint8_t* PlatformVideoSurfaceMap(
	std::uint32_t surface, std::uint32_t& pitchBytes);
void PlatformVideoSurfaceUnmap(std::uint32_t surface);

#endif
