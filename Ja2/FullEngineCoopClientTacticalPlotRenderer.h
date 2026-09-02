#ifndef JA2_FULL_ENGINE_COOP_CLIENT_TACTICAL_PLOT_RENDERER_H
#define JA2_FULL_ENGINE_COOP_CLIENT_TACTICAL_PLOT_RENDERER_H

#include "FullEngineCoopClientTacticalPresentation.h"

#include <cstdint>

// Render-only seam for INIT_SCREEN. The caller supplies a valid video surface
// and bounds wholly inside it. This function reads only the immutable model and
// writes pixels; it never loads a JA2 world or resolves live tactical actors.
bool RenderFullEngineCoopClientTacticalPlot(
	const FullEngineCoopClientTacticalPresentation& presentation,
	std::uint32_t destinationSurface) noexcept;

#endif
