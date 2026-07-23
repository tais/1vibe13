#ifndef ENGINE_ADAPTERS_LEGACY_LEGACY_RENDER_COMMAND_GATEWAY_H
#define ENGINE_ADAPTERS_LEGACY_LEGACY_RENDER_COMMAND_GATEWAY_H

#include <Engine/Core/RenderCommands.h>

// Bind/reset affect only compatibility entry points. Engine and package code
// should retain the RenderCommandSink reference supplied in EngineServices.
void BindLegacyRenderCommands(RenderCommandSink& commands) noexcept;
void ResetLegacyRenderCommands() noexcept;
RenderCommandSink& GetLegacyRenderCommands() noexcept;

bool FillLegacyRenderSurface(
	const RenderSurfaceFillCommand& command) noexcept;
bool CopyLegacyRenderSurface(
	const RenderSurfaceCopyCommand& command) noexcept;
bool StretchLegacyRenderSurface(
	const RenderSurfaceStretchCommand& command) noexcept;
bool ShadeLegacyRenderSurface(
	const RenderSurfaceShadeCommand& command) noexcept;
bool DrawLegacyRenderImage(
	const RenderImageDrawCommand& command) noexcept;

#endif
