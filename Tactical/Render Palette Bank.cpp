#include "Render Palette Bank.h"

#include "MemMan.h"
#include "render_palette_registry.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>
#include <new>
#include <utility>

namespace
{
constexpr std::size_t Owned16PaletteCapacity =
	1 + RenderPaletteBank::ShadeCount +
	RenderPaletteBank::GlowShadeCount +
	RenderPaletteBank::EffectShadeCount;

struct PaletteCloneMap
{
	const PIXEL* sources[Owned16PaletteCapacity]{};
	PIXEL* clones[Owned16PaletteCapacity]{};
	std::size_t size = 0;

	PIXEL* clone(const PIXEL* source)
	{
		if (source == nullptr)
		{
			return nullptr;
		}
		for (std::size_t index = 0; index < size; ++index)
		{
			if (sources[index] == source)
			{
				return clones[index];
			}
		}

		PIXEL* copy = static_cast<PIXEL*>(
			MemAlloc(sizeof(PIXEL) * RenderPaletteBank::EntryCount));
		if (copy == nullptr)
		{
			throw std::bad_alloc();
		}
		std::memcpy(
			copy, source,
			sizeof(PIXEL) * RenderPaletteBank::EntryCount);
		if (!RegisterLegacyRenderPalette(copy))
		{
			MemFree(copy);
			throw std::bad_alloc();
		}

		assert(size < Owned16PaletteCapacity);
		sources[size] = source;
		clones[size] = copy;
		++size;
		return copy;
	}

	PIXEL* remapAlias(PIXEL* source) const noexcept
	{
		for (std::size_t index = 0; index < size; ++index)
		{
			if (sources[index] == source)
			{
				return clones[index];
			}
		}
		return source;
	}
};
}

RenderPaletteBank::~RenderPaletteBank()
{
	reset();
}

RenderPaletteBank::RenderPaletteBank(const RenderPaletteBank& source)
{
	try
	{
		cloneFrom(source);
	}
	catch (...)
	{
		reset();
		throw;
	}
}

RenderPaletteBank& RenderPaletteBank::operator=(
	const RenderPaletteBank& source)
{
	if (this != &source)
	{
		RenderPaletteBank copy(source);
		swapStorage(copy);
	}
	return *this;
}

RenderPaletteBank::RenderPaletteBank(RenderPaletteBank&& source) noexcept
{
	swapStorage(source);
}

RenderPaletteBank& RenderPaletteBank::operator=(
	RenderPaletteBank&& source) noexcept
{
	if (this != &source)
	{
		reset();
		swapStorage(source);
	}
	return *this;
}

PIXEL* RenderPaletteBank::shade(std::size_t index) noexcept
{
	assert(index < ShadeCount);
	return shades_[index];
}

const PIXEL* RenderPaletteBank::shade(std::size_t index) const noexcept
{
	assert(index < ShadeCount);
	return shades_[index];
}

PIXEL* RenderPaletteBank::glowShade(std::size_t index) noexcept
{
	assert(index < GlowShadeCount);
	return glowShades_[index];
}

const PIXEL* RenderPaletteBank::glowShade(
	std::size_t index) const noexcept
{
	assert(index < GlowShadeCount);
	return glowShades_[index];
}

PIXEL* RenderPaletteBank::effectShade(std::size_t index) noexcept
{
	assert(index < EffectShadeCount);
	return effectShades_[index];
}

const PIXEL* RenderPaletteBank::effectShade(
	std::size_t index) const noexcept
{
	assert(index < EffectShadeCount);
	return effectShades_[index];
}

bool RenderPaletteBank::empty() const noexcept
{
	if (base8_ != nullptr || base16_ != nullptr ||
		currentShade_ != nullptr || forcedShade_ != nullptr)
	{
		return false;
	}
	for (const PIXEL* palette : shades_)
	{
		if (palette != nullptr) return false;
	}
	for (const PIXEL* palette : glowShades_)
	{
		if (palette != nullptr) return false;
	}
	for (const PIXEL* palette : effectShades_)
	{
		if (palette != nullptr) return false;
	}
	return true;
}

void RenderPaletteBank::adoptBase8(SGPPaletteEntry* palette) noexcept
{
	if (base8_ == palette) return;
	clearBase8();
	base8_ = palette;
}

void RenderPaletteBank::adoptBase16(PIXEL* palette) noexcept
{
	if (base16_ == palette) return;
	clearBase16();
	base16_ = palette;
	if (palette != nullptr)
	{
		(void)RegisterLegacyRenderPalette(palette);
	}
}

void RenderPaletteBank::adoptShade(
	std::size_t index, PIXEL* palette) noexcept
{
	assert(index < ShadeCount);
	if (shades_[index] == palette) return;
	clearShade(index);
	shades_[index] = palette;
	if (palette != nullptr)
	{
		(void)RegisterLegacyRenderPalette(palette);
	}
}

void RenderPaletteBank::adoptGlowShade(
	std::size_t index, PIXEL* palette) noexcept
{
	assert(index < GlowShadeCount);
	if (glowShades_[index] == palette) return;
	clearGlowShade(index);
	glowShades_[index] = palette;
	if (palette != nullptr)
	{
		(void)RegisterLegacyRenderPalette(palette);
	}
}

void RenderPaletteBank::adoptEffectShade(
	std::size_t index, PIXEL* palette) noexcept
{
	assert(index < EffectShadeCount);
	if (effectShades_[index] == palette) return;
	clearEffectShade(index);
	effectShades_[index] = palette;
	if (palette != nullptr)
	{
		(void)RegisterLegacyRenderPalette(palette);
	}
}

void RenderPaletteBank::clearBase8() noexcept
{
	releaseBase8(std::exchange(base8_, nullptr));
}

void RenderPaletteBank::clearBase16() noexcept
{
	releaseOwned16(base16_);
}

void RenderPaletteBank::clearShade(std::size_t index) noexcept
{
	assert(index < ShadeCount);
	releaseOwned16(shades_[index]);
}

void RenderPaletteBank::clearGlowShade(std::size_t index) noexcept
{
	assert(index < GlowShadeCount);
	releaseOwned16(glowShades_[index]);
}

void RenderPaletteBank::clearEffectShade(std::size_t index) noexcept
{
	assert(index < EffectShadeCount);
	releaseOwned16(effectShades_[index]);
}

void RenderPaletteBank::swapStorage(RenderPaletteBank& other) noexcept
{
	std::swap(base8_, other.base8_);
	std::swap(base16_, other.base16_);
	std::swap_ranges(
		std::begin(shades_), std::end(shades_),
		std::begin(other.shades_));
	std::swap_ranges(
		std::begin(glowShades_), std::end(glowShades_),
		std::begin(other.glowShades_));
	std::swap_ranges(
		std::begin(effectShades_), std::end(effectShades_),
		std::begin(other.effectShades_));
	std::swap(currentShade_, other.currentShade_);
	std::swap(forcedShade_, other.forcedShade_);
}

void RenderPaletteBank::reset() noexcept
{
	clearBase8();
	while (base16_ != nullptr)
	{
		releaseOwned16(base16_);
	}
	for (std::size_t index = 0; index < ShadeCount; ++index)
	{
		while (shades_[index] != nullptr)
		{
			releaseOwned16(shades_[index]);
		}
	}
	for (std::size_t index = 0; index < GlowShadeCount; ++index)
	{
		while (glowShades_[index] != nullptr)
		{
			releaseOwned16(glowShades_[index]);
		}
	}
	for (std::size_t index = 0; index < EffectShadeCount; ++index)
	{
		while (effectShades_[index] != nullptr)
		{
			releaseOwned16(effectShades_[index]);
		}
	}
	currentShade_ = nullptr;
	forcedShade_ = nullptr;
}

void RenderPaletteBank::releaseBase8(
	SGPPaletteEntry* palette) noexcept
{
	if (palette != nullptr)
	{
		MemFree(palette);
	}
}

void RenderPaletteBank::releaseOwned16(PIXEL* palette) noexcept
{
	if (palette == nullptr) return;
	clearOwnedAliases(palette);
	UnregisterLegacyRenderPalette(palette);
	MemFree(palette);
}

void RenderPaletteBank::clearOwnedAliases(
	const PIXEL* palette) noexcept
{
	if (base16_ == palette) base16_ = nullptr;
	for (PIXEL*& owned : shades_)
	{
		if (owned == palette) owned = nullptr;
	}
	for (PIXEL*& owned : glowShades_)
	{
		if (owned == palette) owned = nullptr;
	}
	for (PIXEL*& owned : effectShades_)
	{
		if (owned == palette) owned = nullptr;
	}
	if (currentShade_ == palette) currentShade_ = nullptr;
	if (forcedShade_ == palette) forcedShade_ = nullptr;
}

void RenderPaletteBank::cloneFrom(const RenderPaletteBank& source)
{
	if (source.base8_ != nullptr)
	{
		base8_ = static_cast<SGPPaletteEntry*>(
			MemAlloc(sizeof(SGPPaletteEntry) * EntryCount));
		if (base8_ == nullptr)
		{
			throw std::bad_alloc();
		}
		std::memcpy(
			base8_, source.base8_,
			sizeof(SGPPaletteEntry) * EntryCount);
	}

	PaletteCloneMap clones;
	base16_ = clones.clone(source.base16_);
	for (std::size_t index = 0; index < ShadeCount; ++index)
	{
		shades_[index] = clones.clone(source.shades_[index]);
	}
	for (std::size_t index = 0; index < GlowShadeCount; ++index)
	{
		glowShades_[index] =
			clones.clone(source.glowShades_[index]);
	}
	for (std::size_t index = 0; index < EffectShadeCount; ++index)
	{
		effectShades_[index] =
			clones.clone(source.effectShades_[index]);
	}
	currentShade_ = clones.remapAlias(source.currentShade_);
	forcedShade_ = clones.remapAlias(source.forcedShade_);
}
