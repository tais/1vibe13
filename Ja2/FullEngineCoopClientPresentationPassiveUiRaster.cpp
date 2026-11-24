#include "FullEngineCoopClientPresentationPassiveUiRaster.h"

#include <algorithm>
#include <limits>

namespace
{
class LineStateLease final
{
public:
	LineStateLease(std::uint32_t pitch,
		const FullEngineCoopClientPassiveUiRect& clip) noexcept
	{
		GetLineDrawState(&previous_);
		SetClippingRegionAndImageWidth(static_cast<int>(pitch),
			clip.left, clip.top, clip.right - clip.left,
			clip.bottom - clip.top);
	}
	~LineStateLease() { SetLineDrawState(&previous_); }
	LineStateLease(const LineStateLease&) = delete;
	LineStateLease& operator=(const LineStateLease&) = delete;
private:
	SGPLineDrawState previous_{};
};

PIXEL Color(const FullEngineCoopClientPassiveUiPixelColors& colors,
	FullEngineCoopClientPassiveUiColor color) noexcept
{
	return colors[static_cast<std::size_t>(color)];
}
}

bool RasterFullEngineCoopClientPassiveUiOverlay(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiProjection& projection,
	UINT8* pixels, std::uint32_t pitchBytes,
	std::int32_t surfaceWidth, std::int32_t surfaceHeight,
	const FullEngineCoopClientPassiveUiPixelColors& colors) noexcept
{
	if (pixels == nullptr || !model.dimensions.valid() ||
		!projection.valid() || surfaceWidth <= 0 || surfaceHeight <= 0 ||
		projection.radar.right > surfaceWidth ||
		projection.radar.bottom > surfaceHeight ||
		static_cast<std::uint64_t>(surfaceWidth) * sizeof(PIXEL) > pitchBytes ||
		pitchBytes > static_cast<std::uint32_t>(
			(std::numeric_limits<int>::max)()))
		return false;

	FullEngineCoopClientPassiveUiRect camera;
	if (!ProjectFullEngineCoopClientPassiveCamera(projection, camera))
		return false;
	// Preflight every authority-derived pin before touching the destination.
	// Projection is pure, so the second pass below cannot discover a late
	// malformed grid or token after the camera has already been drawn.
	for (const FullEngineCoopClientPassiveMarker& marker : model.markers)
	{
		FullEngineCoopClientPassiveUiPoint point;
		if (!ProjectFullEngineCoopClientPassiveMarker(
				marker, model.dimensions, projection, point))
			return false;
	}
	for (const FullEngineCoopClientPassiveDoorMarker& door : model.doors)
	{
		FullEngineCoopClientPassiveUiPoint point;
		if (!ProjectFullEngineCoopClientPassiveDoorMarker(
				door, model.dimensions, projection, point))
			return false;
	}

	LineStateLease lineState(pitchBytes, projection.radar);
	RectangleDraw(TRUE, camera.left, camera.top,
		camera.right - 1, camera.bottom - 1,
		Color(colors, FullEngineCoopClientPassiveUiColor::Camera), pixels);

	// Door state is background map context. Draw it before actor/target pins so
	// a coarse three-pixel closed-door glyph cannot erase a co-located actor.
	for (const FullEngineCoopClientPassiveDoorMarker& door : model.doors)
	{
		FullEngineCoopClientPassiveUiPoint point;
		(void)ProjectFullEngineCoopClientPassiveDoorMarker(
			door, model.dimensions, projection, point);
		const PIXEL stateColor = Color(colors, door.open
			? FullEngineCoopClientPassiveUiColor::DoorOpen
			: FullEngineCoopClientPassiveUiColor::DoorClosed);
		RectangleDraw(TRUE, point.x, point.y,
			point.x + 2, point.y + 2, stateColor, pixels);
		if (!door.open)
			LineDraw(TRUE, point.x, point.y + 1,
				point.x + 2, point.y + 1, stateColor, pixels);
	}

	for (const FullEngineCoopClientPassiveMarker& marker : model.markers)
	{
		FullEngineCoopClientPassiveUiPoint point;
		if (!ProjectFullEngineCoopClientPassiveMarker(
				marker, model.dimensions, projection, point))
			return false;
		const PIXEL color = Color(
			colors, FullEngineCoopClientPassiveMarkerColor(marker));
		if (marker.targeted)
		{
			// A clipped outline remains recognizable even when the authority pin
			// lands on the edge of the radar. It is presentation-only; no target
			// flag is ever written into the renderer's proxy actor repository.
			const std::int32_t left = static_cast<std::int32_t>((std::max)(
				static_cast<std::int64_t>(projection.radar.left),
				static_cast<std::int64_t>(point.x) - 2));
			const std::int32_t top = static_cast<std::int32_t>((std::max)(
				static_cast<std::int64_t>(projection.radar.top),
				static_cast<std::int64_t>(point.y) - 2));
			const std::int32_t right = static_cast<std::int32_t>((std::min)(
				static_cast<std::int64_t>(projection.radar.right) - 1,
				static_cast<std::int64_t>(point.x) + 3));
			const std::int32_t bottom = static_cast<std::int32_t>((std::min)(
				static_cast<std::int64_t>(projection.radar.bottom) - 1,
				static_cast<std::int64_t>(point.y) + 3));
			RectangleDraw(FALSE, left, top, right, bottom, color, pixels);
		}
		RectangleDraw(TRUE, point.x, point.y,
			(std::min)(point.x + 1, projection.radar.right - 1),
			(std::min)(point.y + 1, projection.radar.bottom - 1),
			color, pixels);
	}
	for (const FullEngineCoopClientPassiveDoorMarker& door : model.doors)
	{
		if (!door.selected) continue;
		FullEngineCoopClientPassiveUiPoint point;
		(void)ProjectFullEngineCoopClientPassiveDoorMarker(
			door, model.dimensions, projection, point);
		const std::int32_t left = (std::max)(
			projection.radar.left, point.x - 1);
		const std::int32_t top = (std::max)(
			projection.radar.top, point.y - 1);
		const std::int32_t right = (std::min)(
			projection.radar.right - 1, point.x + 3);
		const std::int32_t bottom = (std::min)(
			projection.radar.bottom - 1, point.y + 3);
		RectangleDraw(FALSE, left, top, right, bottom,
			Color(colors,
				FullEngineCoopClientPassiveUiColor::DoorSelected), pixels);
	}
	return true;
}
