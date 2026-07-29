#ifndef TACTICAL_RENDER_PALETTE_BANK_H
#define TACTICAL_RENDER_PALETTE_BANK_H

#include "Overhead Types.h"
#include "himage.h"

#include <cstddef>

// Owns the legacy palette storage used to render one tactical actor or one
// logical-body layer. The renderer still consumes established 256-entry host
// palettes, but their allocation, render-registry lifetime, copy semantics,
// and active aliases now move together.
class RenderPaletteBank
{
public:
	static constexpr std::size_t EntryCount = 256;
	static constexpr std::size_t ShadeCount = NUM_SOLDIER_SHADES;
	static constexpr std::size_t GlowShadeCount = 20;
	static constexpr std::size_t EffectShadeCount =
		NUM_SOLDIER_EFFECTSHADES;

	RenderPaletteBank() noexcept = default;
	~RenderPaletteBank();
	RenderPaletteBank(const RenderPaletteBank& source);
	RenderPaletteBank& operator=(const RenderPaletteBank& source);
	RenderPaletteBank(RenderPaletteBank&& source) noexcept;
	RenderPaletteBank& operator=(RenderPaletteBank&& source) noexcept;

	SGPPaletteEntry* base8() noexcept { return base8_; }
	const SGPPaletteEntry* base8() const noexcept { return base8_; }
	PIXEL* base16() noexcept { return base16_; }
	const PIXEL* base16() const noexcept { return base16_; }
	PIXEL* shade(std::size_t index) noexcept;
	const PIXEL* shade(std::size_t index) const noexcept;
	PIXEL* glowShade(std::size_t index) noexcept;
	const PIXEL* glowShade(std::size_t index) const noexcept;
	PIXEL* effectShade(std::size_t index) noexcept;
	const PIXEL* effectShade(std::size_t index) const noexcept;
	PIXEL* currentShade() noexcept { return currentShade_; }
	const PIXEL* currentShade() const noexcept { return currentShade_; }
	PIXEL* forcedShade() noexcept { return forcedShade_; }
	const PIXEL* forcedShade() const noexcept { return forcedShade_; }

	bool empty() const noexcept;
	void adoptBase8(SGPPaletteEntry* palette) noexcept;
	void adoptBase16(PIXEL* palette) noexcept;
	void adoptShade(std::size_t index, PIXEL* palette) noexcept;
	void adoptGlowShade(std::size_t index, PIXEL* palette) noexcept;
	void adoptEffectShade(std::size_t index, PIXEL* palette) noexcept;
	void setCurrentShade(PIXEL* palette) noexcept { currentShade_ = palette; }
	void setForcedShade(PIXEL* palette) noexcept { forcedShade_ = palette; }
	void clearBase8() noexcept;
	void clearBase16() noexcept;
	void clearShade(std::size_t index) noexcept;
	void clearGlowShade(std::size_t index) noexcept;
	void clearEffectShade(std::size_t index) noexcept;
	void swapStorage(RenderPaletteBank& other) noexcept;
	void reset() noexcept;

private:
	static void releaseBase8(SGPPaletteEntry* palette) noexcept;
	void releaseOwned16(PIXEL* palette) noexcept;
	void clearOwnedAliases(const PIXEL* palette) noexcept;
	void cloneFrom(const RenderPaletteBank& source);

	SGPPaletteEntry* base8_ = nullptr;
	PIXEL* base16_ = nullptr;
	PIXEL* shades_[ShadeCount]{};
	PIXEL* glowShades_[GlowShadeCount]{};
	PIXEL* effectShades_[EffectShadeCount]{};
	PIXEL* currentShade_ = nullptr;
	PIXEL* forcedShade_ = nullptr;
};

#endif
