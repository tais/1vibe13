#ifndef ENGINE_ADAPTERS_LEGACY_PLATFORM_DEPTH_BUFFER_BACKEND_H
#define ENGINE_ADAPTERS_LEGACY_PLATFORM_DEPTH_BUFFER_BACKEND_H

#include <cstdint>

// Raw SGP tactical-depth storage. Only PlatformRenderSurfaceAccess consumes
// this interface; game and package code address the buffer through the
// RenderSurfaceRole::DepthBuffer identity.
bool PlatformDepthBufferDescribe(
	std::uint32_t& width,
	std::uint32_t& height,
	std::uint32_t& pitchBytes);
std::uint8_t* PlatformDepthBufferMap(
	std::uint32_t& pitchBytes);
void PlatformDepthBufferUnmap();

#endif
