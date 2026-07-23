#include <Engine/Adapters/Legacy/LegacyRenderCommandGateway.h>

#include <Engine/Adapters/Legacy/LegacyRenderSurfaceGateway.h>
#include <Engine/Adapters/Legacy/PlatformRenderCommands.h>

#include "vsurface.h"

#include <algorithm>
#include <atomic>
#include <cmath>

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

RenderColor DecodeLegacyColor(std::uint32_t color) noexcept
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

bool LegacyShadeFraction(
	bool lowPercent,
	std::uint16_t& numerator,
	std::uint16_t& denominator) noexcept
{
	const float fraction = lowPercent ? 0.80f : guiShadePercent;
	if (!std::isfinite(fraction)) return false;
	const float clipped = std::max(0.0f, std::min(fraction, 1.0f));
	denominator = 1000;
	numerator = static_cast<std::uint16_t>(
		std::lround(clipped * denominator));
	return true;
}
}

RenderColor DecodeLegacyRenderColor(std::uint32_t color) noexcept
{
	return DecodeLegacyColor(color);
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

bool CopyLegacyRenderSurface(
	const RenderSurfaceCopyCommand& command) noexcept
{
	RenderCommandGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		return GetLegacyRenderCommands().copySurface(command);
	}
	catch (...)
	{
		return false;
	}
}

bool StretchLegacyRenderSurface(
	const RenderSurfaceStretchCommand& command) noexcept
{
	RenderCommandGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		return GetLegacyRenderCommands().stretchSurface(command);
	}
	catch (...)
	{
		return false;
	}
}

bool ShadeLegacyRenderSurface(
	const RenderSurfaceShadeCommand& command) noexcept
{
	RenderCommandGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		return GetLegacyRenderCommands().shadeSurface(command);
	}
	catch (...)
	{
		return false;
	}
}

bool DrawLegacyRenderImage(
	const RenderImageDrawCommand& command) noexcept
{
	RenderCommandGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		return GetLegacyRenderCommands().drawImage(command);
	}
	catch (...)
	{
		return false;
	}
}

bool DrawLegacyRenderImageOutline(
	const RenderImageOutlineCommand& command) noexcept
{
	RenderCommandGuard guard;
	if (!guard.acquired()) return false;
	try
	{
		return GetLegacyRenderCommands().drawImageOutline(command);
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
		DecodeLegacyRenderColor(color)}) ? TRUE : FALSE;
}

BOOLEAN BltVideoSurface(
	UINT32 destination,
	UINT32 source,
	UINT16 sourceRegionIndex,
	INT32 destinationX,
	INT32 destinationY,
	UINT32 flags,
	blt_vs_fx* effects)
{
	HVSURFACE destinationSurface = nullptr;
	HVSURFACE sourceSurface = nullptr;
	if (!GetVideoSurface(&destinationSurface, destination) ||
		!destinationSurface ||
		!GetVideoSurface(&sourceSurface, source) ||
		!sourceSurface ||
		destinationSurface->ubBitDepth != sourceSurface->ubBitDepth)
		return FALSE;

	if (flags & VS_BLT_COLORFILLRECT)
	{
		if (!effects || destinationSurface->ubBitDepth != 16) return FALSE;
		return FillLegacyRenderSurface(RenderSurfaceFillCommand{
			destination,
			RenderSurfaceRegion{
				effects->FillRect.iLeft,
				effects->FillRect.iTop,
				effects->FillRect.iRight,
				effects->FillRect.iBottom},
			DecodeLegacyRenderColor(effects->ColorFill)}) ? TRUE : FALSE;
	}
	if (flags & VS_BLT_COLORFILL)
	{
		if (!effects || destinationSurface->ubBitDepth != 16) return FALSE;
		return FillLegacyRenderSurface(RenderSurfaceFillCommand{
			destination,
			RenderSurfaceRegion{
				0, 0,
				static_cast<std::int32_t>(destinationSurface->usWidth),
				static_cast<std::int32_t>(destinationSurface->usHeight)},
			DecodeLegacyRenderColor(effects->ColorFill)}) ? TRUE : FALSE;
	}

	RenderSurfaceRegion sourceRegion;
	if (flags & VS_BLT_SRCREGION)
	{
		VSURFACE_REGION legacyRegion{};
		if (!GetVSurfaceRegion(
				sourceSurface, sourceRegionIndex, &legacyRegion))
			return FALSE;
		sourceRegion = RenderSurfaceRegion{
			legacyRegion.RegionCoords.iLeft,
			legacyRegion.RegionCoords.iTop,
			legacyRegion.RegionCoords.iRight,
			legacyRegion.RegionCoords.iBottom};
	}
	else if (flags & VS_BLT_SRCSUBRECT)
	{
		if (!effects) return FALSE;
		sourceRegion = RenderSurfaceRegion{
			effects->SrcRect.iLeft,
			effects->SrcRect.iTop,
			effects->SrcRect.iRight,
			effects->SrcRect.iBottom};
	}
	else
	{
		sourceRegion = RenderSurfaceRegion{
			0, 0,
			static_cast<std::int32_t>(sourceSurface->usWidth),
			static_cast<std::int32_t>(sourceSurface->usHeight)};
	}

	RenderSurfaceCopyMode mode = RenderSurfaceCopyMode::Opaque;
	RenderColor colorKey;
	if ((flags & VS_BLT_USECOLORKEY) &&
		sourceSurface->ubBitDepth == 16)
	{
		mode = RenderSurfaceCopyMode::SourceColorKeyRgb;
		// This is deliberately the historical conversion. TransparentColor is
		// a COLORVAL, but the blitter has always treated its low 16 bits as an
		// RGB565 token before widening it to the physical pixel format.
		colorKey = DecodeLegacyRenderColor(static_cast<PIXEL>(
			static_cast<UINT16>(sourceSurface->TransparentColor)));
	}

	return CopyLegacyRenderSurface(RenderSurfaceCopyCommand{
		source,
		destination,
		sourceRegion,
		RenderSurfacePoint{destinationX, destinationY},
		mode,
		colorKey}) ? TRUE : FALSE;
}

BOOLEAN BltStretchVideoSurface(
	UINT32 destination,
	UINT32 source,
	INT32 destinationX,
	INT32 destinationY,
	UINT32 flags,
	SGPRect* sourceRegion,
	SGPRect* destinationRegion)
{
	// The DirectDraw-era API carried an extra point, but both the original
	// implementation and the SDL port use the explicit destination rectangle.
	(void)destinationX;
	(void)destinationY;
	if (!sourceRegion || !destinationRegion) return FALSE;

	HVSURFACE destinationSurface = nullptr;
	HVSURFACE sourceSurface = nullptr;
	if (!GetVideoSurface(&destinationSurface, destination) ||
		!destinationSurface ||
		!GetVideoSurface(&sourceSurface, source) ||
		!sourceSurface ||
		destinationSurface->ubBitDepth != 16 ||
		sourceSurface->ubBitDepth != 16)
		return FALSE;

	RenderSurfaceCopyMode mode = RenderSurfaceCopyMode::Opaque;
	RenderColor colorKey;
	if (flags & VS_BLT_USECOLORKEY)
	{
		mode = RenderSurfaceCopyMode::SourceColorKeyRgb;
		colorKey = DecodeLegacyRenderColor(static_cast<PIXEL>(
			static_cast<UINT16>(sourceSurface->TransparentColor)));
	}

	return StretchLegacyRenderSurface(RenderSurfaceStretchCommand{
		source,
		destination,
		RenderSurfaceRegion{
			sourceRegion->iLeft,
			sourceRegion->iTop,
			sourceRegion->iRight,
			sourceRegion->iBottom},
		RenderSurfaceRegion{
			destinationRegion->iLeft,
			destinationRegion->iTop,
			destinationRegion->iRight,
			destinationRegion->iBottom},
		mode,
		colorKey}) ? TRUE : FALSE;
}

namespace
{
BOOLEAN ShadeLegacySurfaceRegion(
	UINT32 surface,
	INT32 left,
	INT32 top,
	INT32 right,
	INT32 bottom,
	bool lowPercent)
{
	if (right <= left || bottom <= top) return FALSE;
	RenderSurfaceDescription description;
	if (!DescribeLegacyRenderSurface(surface, description) ||
		description.contentBitDepth != 16)
		return FALSE;

	std::uint16_t numerator = 0;
	std::uint16_t denominator = 0;
	if (!LegacyShadeFraction(lowPercent, numerator, denominator))
		return FALSE;
	return ShadeLegacyRenderSurface(RenderSurfaceShadeCommand{
		surface,
		RenderSurfaceRegion{left, top, right, bottom},
		numerator,
		denominator}) ? TRUE : FALSE;
}
}

BOOLEAN ShadowVideoSurfaceRect(
	UINT32 surface,
	INT32 left,
	INT32 top,
	INT32 right,
	INT32 bottom)
{
	return ShadeLegacySurfaceRegion(
		surface, left, top, right, bottom, false);
}

BOOLEAN ShadowVideoSurfaceRectUsingLowPercentTable(
	UINT32 surface,
	INT32 left,
	INT32 top,
	INT32 right,
	INT32 bottom)
{
	return ShadeLegacySurfaceRegion(
		surface, left, top, right, bottom, true);
}
