#ifndef ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_OBJECT_BACKEND_H
#define ENGINE_ADAPTERS_LEGACY_PLATFORM_VIDEO_OBJECT_BACKEND_H

#include <Engine/Core/RenderCommands.h>

// Raw SGP video-object renderer. Only PlatformRenderCommands consumes this;
// game and compatibility callers submit RenderImageDrawCommand values through
// the engine-owned command boundary.
bool PlatformVideoObjectDraw(const RenderImageDrawCommand& command);

#endif
