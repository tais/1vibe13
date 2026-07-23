#ifndef ENGINE_ADAPTERS_LEGACY_LEGACY_RENDER_COMMAND_GATEWAY_H
#define ENGINE_ADAPTERS_LEGACY_LEGACY_RENDER_COMMAND_GATEWAY_H

#include <Engine/Core/RenderCommands.h>

// Bind/reset affect only compatibility entry points. Engine and package code
// should retain the RenderCommandSink reference supplied in EngineServices.
void BindLegacyRenderCommands(RenderCommandSink& commands) noexcept;
void ResetLegacyRenderCommands() noexcept;
RenderCommandSink& GetLegacyRenderCommands() noexcept;

// Packed SGP colours are normalized at the compatibility edge so command
// streams remain independent of the host framebuffer's physical format.
RenderColor DecodeLegacyRenderColor(std::uint32_t color) noexcept;

bool FillLegacyRenderSurface(
	const RenderSurfaceFillCommand& command) noexcept;
bool CopyLegacyRenderSurface(
	const RenderSurfaceCopyCommand& command) noexcept;
bool StretchLegacyRenderSurface(
	const RenderSurfaceStretchCommand& command) noexcept;
bool ShadeLegacyRenderSurface(
	const RenderSurfaceShadeCommand& command) noexcept;
bool FillLegacyRenderDepth(
	const RenderDepthFillCommand& command) noexcept;
bool DrawLegacyRenderImage(
	const RenderImageDrawCommand& command) noexcept;
bool DrawLegacyRenderImageOutline(
	const RenderImageOutlineCommand& command) noexcept;

#endif
