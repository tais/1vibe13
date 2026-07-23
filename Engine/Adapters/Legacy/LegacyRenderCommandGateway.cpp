#include <Engine/Adapters/Legacy/LegacyRenderCommandGateway.h>

#include <Engine/Adapters/Legacy/LegacyRenderSurfaceGateway.h>
#include <Engine/Adapters/Legacy/PlatformRenderCommands.h>

#include "vsurface.h"

#include <atomic>

namespace
{
std::atomic<RenderCommandSink*> gBoundCommands{nullptr};
thread_local bool gSubmittingRenderCommand = false;

class RenderCommandGuard
{
public:
	RenderCommandGuard() noexcept
		: acquired_(!gSubmittingRenderCommand)
	{
		if (acquired_) gSubmittingRenderCommand = true;
	}

	~RenderCommandGuard()
	{
		if (acquired_) gSubmittingRenderCommand = false;
	}

	bool acquired() const noexcept { return acquired_; }

private:
	bool acquired_;
};

RenderColor DecodeLegacyColor(PIXEL color) noexcept
{
	const PIXEL normalized = PixFromColor16(color);
	if constexpr (sizeof(PIXEL) == sizeof(std::uint32_t))
	{
		const std::uint32_t packed =
			static_cast<std::uint32_t>(normalized);
		return RenderColor{
			static_cast<std::uint8_t>(packed >> 16),
			static_cast<std::uint8_t>(packed >> 8),
			static_cast<std::uint8_t>(packed),
			static_cast<std::uint8_t>(packed >> 24)};
	}
	else
	{
		const std::uint16_t packed =
			static_cast<std::uint16_t>(normalized);
		const std::uint8_t red5 =
			static_cast<std::uint8_t>((packed >> 11) & 0x1fu);
		const std::uint8_t green6 =
			static_cast<std::uint8_t>((packed >> 5) & 0x3fu);
		const std::uint8_t blue5 =
			static_cast<std::uint8_t>(packed & 0x1fu);
		return RenderColor{
			static_cast<std::uint8_t>((red5 << 3) | (red5 >> 2)),
			static_cast<std::uint8_t>((green6 << 2) | (green6 >> 4)),
			static_cast<std::uint8_t>((blue5 << 3) | (blue5 >> 2)),
			255};
	}
}
}

void BindLegacyRenderCommands(RenderCommandSink& commands) noexcept
{
	gBoundCommands.store(&commands, std::memory_order_release);
}

void ResetLegacyRenderCommands() noexcept
{
	gBoundCommands.store(nullptr, std::memory_order_release);
}

RenderCommandSink& GetLegacyRenderCommands() noexcept
{
	RenderCommandSink* const commands =
		gBoundCommands.load(std::memory_order_acquire);
	return commands ? *commands : GetPlatformRenderCommands();
}

bool FillLegacyRenderSurface(
	const RenderSurfaceFillCommand& command) noexcept
{
	RenderCommandGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		return GetLegacyRenderCommands().fillSurface(command);
	}
	catch (...)
	{
		return false;
	}
}

BOOLEAN ColorFillVideoSurfaceArea(
	UINT32 surface,
	INT32 left,
	INT32 top,
	INT32 right,
	INT32 bottom,
	PIXEL color)
{
	RenderSurfaceDescription description;
	if (!DescribeLegacyRenderSurface(surface, description) ||
		description.contentBitDepth != 16)
		return FALSE;
	return FillLegacyRenderSurface(RenderSurfaceFillCommand{
		surface,
		RenderSurfaceRegion{left, top, right, bottom},
		DecodeLegacyColor(color)}) ? TRUE : FALSE;
}
