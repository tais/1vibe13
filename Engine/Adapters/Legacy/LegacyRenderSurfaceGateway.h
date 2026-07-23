#ifndef ENGINE_ADAPTERS_LEGACY_LEGACY_RENDER_SURFACE_GATEWAY_H
#define ENGINE_ADAPTERS_LEGACY_LEGACY_RENDER_SURFACE_GATEWAY_H

#include <Engine/Core/RenderSurfaceAccess.h>

// Bind/reset affect only compatibility entry points. Engine and package code
// should retain the RenderSurfaceAccess reference supplied in EngineServices.
void BindLegacyRenderSurfaceAccess(RenderSurfaceAccess& access) noexcept;
void ResetLegacyRenderSurfaceAccess() noexcept;
RenderSurfaceAccess& GetLegacyRenderSurfaceAccess() noexcept;

bool DescribeLegacyRenderSurface(
	RenderSurfaceId surface, RenderSurfaceDescription& description) noexcept;
bool MapLegacyRenderSurface(
	RenderSurfaceId surface, MutableRenderSurface& mapping) noexcept;
bool UnmapLegacyRenderSurface(RenderSurfaceId surface) noexcept;

#endif
