#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_RASTER_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_RASTER_H

#include "FullEngineCoopClientPresentationPassiveUi.h"

#include "line.h"

#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr std::size_t FullEngineCoopClientPassiveUiColorCount =
	static_cast<std::size_t>(FullEngineCoopClientPassiveUiColor::Count);

using FullEngineCoopClientPassiveUiPixelColors =
	std::array<PIXEL, FullEngineCoopClientPassiveUiColorCount>;

// Rasterizes the already-projected camera/marker overlay into a caller-owned
// surface. The byte pitch and radar clip are installed only for this call and
// the exact prior line-rasterizer state is restored on every return path.
bool RasterFullEngineCoopClientPassiveUiOverlay(
	const FullEngineCoopClientPassiveUiModel& model,
	const FullEngineCoopClientPassiveUiProjection& projection,
	UINT8* pixels,
	std::uint32_t pitchBytes,
	std::int32_t surfaceWidth,
	std::int32_t surfaceHeight,
	const FullEngineCoopClientPassiveUiPixelColors& colors) noexcept;

#endif
