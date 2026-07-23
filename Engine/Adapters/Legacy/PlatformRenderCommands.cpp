#include <Engine/Adapters/Legacy/PlatformRenderCommands.h>

#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>

RenderCommandSink& GetPlatformRenderCommands() noexcept
{
	static MappedRenderCommandSink commands(GetPlatformRenderSurfaceAccess());
	return commands;
}
