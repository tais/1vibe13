#include "FullEngineCoopClientTacticalPlotRenderer.h"

#include "vsurface.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace
{
constexpr int MarkerDirectionX[8] = {0, 2, 3, 2, 0, -2, -3, -2};
constexpr int MarkerDirectionY[8] = {-3, -2, 0, 2, 3, 2, 0, -2};

int MarkerRadius(const FullEngineCoopClientTacticalPlotMarker& marker) noexcept
{
	return marker.selected ? 3 : (marker.assigned ? 2 : 1);
}

bool ResolveBounds(const FullEngineCoopClientTacticalPlotBounds& bounds,
	int& left, int& top, int& right, int& bottom) noexcept
{
	const std::int64_t resolvedLeft = bounds.x;
	const std::int64_t resolvedTop = bounds.y;
	const std::int64_t resolvedWidth = bounds.width;
	const std::int64_t resolvedHeight = bounds.height;
	const std::int64_t rightExclusive = resolvedLeft + resolvedWidth;
	const std::int64_t bottomExclusive = resolvedTop + resolvedHeight;
	if (resolvedLeft < 0 || resolvedTop < 0 ||
		resolvedWidth < 3 || resolvedHeight < 3 ||
		rightExclusive > std::numeric_limits<int>::max() ||
		bottomExclusive > std::numeric_limits<int>::max())
		return false;
	left = static_cast<int>(resolvedLeft);
	top = static_cast<int>(resolvedTop);
	right = static_cast<int>(rightExclusive - 1);
	bottom = static_cast<int>(bottomExclusive - 1);
	return true;
}

bool OffsetFitsInt(int value, int offset) noexcept
{
	const std::int64_t adjusted =
		static_cast<std::int64_t>(value) + offset;
	return adjusted >= std::numeric_limits<int>::min() &&
		adjusted <= std::numeric_limits<int>::max();
}

int OffsetAndClamp(int value, int offset, int low, int high) noexcept
{
	const std::int64_t adjusted =
		static_cast<std::int64_t>(value) + offset;
	return static_cast<int>(std::max(static_cast<std::int64_t>(low),
		std::min(adjusted, static_cast<std::int64_t>(high))));
}

bool MarkerOffsetsFit(const FullEngineCoopClientTacticalPlotMarker& marker)
	noexcept
{
	const int radius = MarkerRadius(marker);
	return marker.direction < 8 &&
		OffsetFitsInt(marker.screenX, -radius) &&
		OffsetFitsInt(marker.screenX, radius) &&
		OffsetFitsInt(marker.screenY, -radius) &&
		OffsetFitsInt(marker.screenY, radius) &&
		OffsetFitsInt(marker.screenX, MarkerDirectionX[marker.direction]) &&
		OffsetFitsInt(marker.screenY, MarkerDirectionY[marker.direction]);
}

void PutPixel(std::uint8_t* surface, std::uint32_t pitch,
	int x, int y, PIXEL color) noexcept
{
	auto* const row = reinterpret_cast<PIXEL*>(
		surface + static_cast<std::size_t>(y) * pitch);
	row[x] = color;
}

void DrawLine(std::uint8_t* surface, std::uint32_t pitch,
	int x0, int y0, int x1, int y1, PIXEL color) noexcept
{
	const std::int64_t dx = x0 <= x1
		? static_cast<std::int64_t>(x1) - x0
		: static_cast<std::int64_t>(x0) - x1;
	const std::int64_t dy = y0 <= y1
		? static_cast<std::int64_t>(y1) - y0
		: static_cast<std::int64_t>(y0) - y1;
	const int stepX = x0 < x1 ? 1 : -1;
	const int stepY = y0 < y1 ? 1 : -1;
	std::int64_t error = dx - dy;
	for (;;)
	{
		PutPixel(surface, pitch, x0, y0, color);
		if (x0 == x1 && y0 == y1) return;
		const std::int64_t twiceError = error * 2;
		if (twiceError > -dy)
		{
			error -= dy;
			x0 += stepX;
		}
		if (twiceError < dx)
		{
			error += dx;
			y0 += stepY;
		}
	}
}

void DrawRectangle(std::uint8_t* surface, std::uint32_t pitch,
	int left, int top, int right, int bottom, PIXEL color) noexcept
{
	DrawLine(surface, pitch, left, top, right, top, color);
	DrawLine(surface, pitch, left, bottom, right, bottom, color);
	DrawLine(surface, pitch, left, top, left, bottom, color);
	DrawLine(surface, pitch, right, top, right, bottom, color);
}

void DrawMarker(const FullEngineCoopClientTacticalPresentation& presentation,
	const FullEngineCoopClientTacticalPlotMarker& marker,
	std::uint8_t* surface, std::uint32_t pitch) noexcept
{
	const auto& bounds = presentation.bounds;
	const int left = bounds.x;
	const int top = bounds.y;
	const int right = static_cast<int>(
		static_cast<std::int64_t>(bounds.x) + bounds.width - 1);
	const int bottom = static_cast<int>(
		static_cast<std::int64_t>(bounds.y) + bounds.height - 1);
	const PIXEL color = Get16BPPColor(marker.selected
		? FROMRGB(255, 225, 64)
		: (marker.assigned
			? FROMRGB(72, 255, 112)
			: FROMRGB(80, 190, 255)));
	const int radius = MarkerRadius(marker);
	DrawRectangle(surface, pitch,
		OffsetAndClamp(marker.screenX, -radius, left, right),
		OffsetAndClamp(marker.screenY, -radius, top, bottom),
		OffsetAndClamp(marker.screenX, radius, left, right),
		OffsetAndClamp(marker.screenY, radius, top, bottom), color);

	DrawLine(surface, pitch, marker.screenX, marker.screenY,
		OffsetAndClamp(marker.screenX,
			MarkerDirectionX[marker.direction], left, right),
		OffsetAndClamp(marker.screenY,
			MarkerDirectionY[marker.direction], top, bottom),
		color);
}
}

bool RenderFullEngineCoopClientTacticalPlot(
	const FullEngineCoopClientTacticalPresentation& presentation,
	std::uint32_t destinationSurface) noexcept
{
	const auto& bounds = presentation.bounds;
	int left = 0;
	int top = 0;
	int right = 0;
	int bottom = 0;
	if (!presentation.dimensions.valid() ||
		!ResolveBounds(bounds, left, top, right, bottom) ||
		presentation.markerCount > presentation.markers.size())
		return false;

	const int centerX = left + (bounds.width - 1) / 2;
	const int centerY = top + (bounds.height - 1) / 2;
	if (!presentation.hasFriendlyTeam && presentation.markerCount != 0)
		return false;
	for (std::size_t index = 0; index < presentation.markerCount; ++index)
	{
		const FullEngineCoopClientTacticalPlotMarker& marker =
			presentation.markers[index];
		if (!marker.actor.valid() ||
			marker.column >= presentation.dimensions.columns ||
			marker.row >= presentation.dimensions.rows ||
			marker.screenX < left || marker.screenX > right ||
			marker.screenY < top || marker.screenY > bottom ||
			!MarkerOffsetsFit(marker))
			return false;
	}
	if (!ColorFillVideoSurfaceArea(destinationSurface,
		left, top, right + 1, bottom + 1,
		Get16BPPColor(FROMRGB(10, 20, 28))))
		return false;

	UINT32 pitch = 0;
	UINT8* const surface = LockVideoSurface(destinationSurface, &pitch);
	if (surface == nullptr) return false;
	const PIXEL border = Get16BPPColor(FROMRGB(92, 122, 138));
	const PIXEL axes = Get16BPPColor(FROMRGB(40, 66, 78));
	DrawLine(surface, pitch, centerX, top, right, centerY, border);
	DrawLine(surface, pitch, right, centerY, centerX, bottom, border);
	DrawLine(surface, pitch, centerX, bottom, left, centerY, border);
	DrawLine(surface, pitch, left, centerY, centerX, top, border);
	DrawLine(surface, pitch, centerX, top, centerX, bottom, axes);
	DrawLine(surface, pitch, left, centerY, right, centerY, axes);
	for (std::size_t index = 0; index < presentation.markerCount; ++index)
		DrawMarker(presentation, presentation.markers[index], surface, pitch);
	UnLockVideoSurface(destinationSurface);
	return true;
}
