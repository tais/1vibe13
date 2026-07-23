#ifndef SGP_RENDER_PALETTE_REGISTRY_H
#define SGP_RENDER_PALETTE_REGISTRY_H

#include "pixfmt.h"

#include <Engine/Core/RenderCommands.h>

#include <cstddef>

// Registers a borrowed, immutable 256-entry host palette behind a stable
// engine identity. Registration is idempotent for the same live pointer.
bool RegisterLegacyRenderPalette(
	const PIXEL* palette, RenderPaletteId* identity = nullptr) noexcept;

// Retires a palette identity before its owner releases or replaces storage.
void UnregisterLegacyRenderPalette(const PIXEL* palette) noexcept;

bool FindLegacyRenderPalette(
	const PIXEL* palette, RenderPaletteId& identity) noexcept;
const PIXEL* ResolveLegacyRenderPalette(RenderPaletteId identity) noexcept;
std::size_t LegacyRenderPaletteCount() noexcept;

#endif
