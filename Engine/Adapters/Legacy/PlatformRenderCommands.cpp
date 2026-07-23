#include <Engine/Adapters/Legacy/PlatformRenderCommands.h>

#include <Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.h>
#include <Engine/Adapters/Legacy/PlatformVideoObjectBackend.h>

namespace
{
class PlatformRenderCommandSink final : public RenderCommandSink
{
public:
	PlatformRenderCommandSink()
		: mapped_(GetPlatformRenderSurfaceAccess())
	{
	}

	bool fillSurface(const RenderSurfaceFillCommand& command) override
	{
		return mapped_.fillSurface(command);
	}

	bool copySurface(const RenderSurfaceCopyCommand& command) override
	{
		return mapped_.copySurface(command);
	}

	bool stretchSurface(const RenderSurfaceStretchCommand& command) override
	{
		return mapped_.stretchSurface(command);
	}

	bool shadeSurface(const RenderSurfaceShadeCommand& command) override
	{
		return mapped_.shadeSurface(command);
	}

	bool drawImage(const RenderImageDrawCommand& command) override
	{
		try
		{
			return PlatformVideoObjectDraw(command);
		}
		catch (...)
		{
			return false;
		}
	}

private:
	MappedRenderCommandSink mapped_;
};
}

RenderCommandSink& GetPlatformRenderCommands() noexcept
{
	static PlatformRenderCommandSink commands;
	return commands;
}
