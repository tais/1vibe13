#include "Render Palette Effects.h"

#include "MemMan.h"
#include "Render Palette Bank.h"
#include "lighting.h"
#include "render_palette_registry.h"
#include "shading.h"
#include "video.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
constexpr std::array<std::uint8_t, 20> RedGlowScale{
	0, 25, 50, 75, 100, 125, 150, 175, 200, 225,
	0, 25, 50, 75, 100, 125, 150, 175, 200, 225};
constexpr std::array<std::uint8_t, 20> OrangeGlowRedScale{
	0, 25, 50, 75, 100, 125, 150, 175, 200, 225,
	0, 25, 50, 75, 100, 125, 150, 175, 200, 225};
constexpr std::array<std::uint8_t, 20> OrangeGlowGreenScale{
	0, 20, 40, 60, 80, 100, 120, 140, 160, 180,
	0, 20, 40, 60, 80, 100, 120, 140, 160, 180};

PIXEL packColour(
	std::uint8_t red,
	std::uint8_t green,
	std::uint8_t blue) noexcept
{
#if SGP_PIXEL_DEPTH == 32
	PIXEL colour = 0xFF000000u |
		(static_cast<std::uint32_t>(red) << 16) |
		(static_cast<std::uint32_t>(green) << 8) |
		static_cast<std::uint32_t>(blue);
	if ((colour & 0x00FFFFFFu) == 0 &&
		(red + green + blue) != 0)
	{
		colour = 0xFF000001u;
	}
	return colour;
#else
	const PIXEL packedRed = gusRedShift < 0
		? static_cast<PIXEL>(red) >> (-gusRedShift)
		: static_cast<PIXEL>(red) << gusRedShift;
	const PIXEL packedGreen = gusGreenShift < 0
		? static_cast<PIXEL>(green) >> (-gusGreenShift)
		: static_cast<PIXEL>(green) << gusGreenShift;
	const PIXEL packedBlue = gusBlueShift < 0
		? static_cast<PIXEL>(blue) >> (-gusBlueShift)
		: static_cast<PIXEL>(blue) << gusBlueShift;
	PIXEL colour = (packedRed & gusRedMask) |
		(packedGreen & gusGreenMask) |
		(packedBlue & gusBlueMask);
	if (colour == 0 && (red + green + blue) != 0)
		colour = 0x0001;
	return colour;
#endif
}

PIXEL* createGlow(
	const SGPPaletteEntry* palette,
	std::uint32_t redScale,
	std::uint32_t greenScale,
	bool adjustGreen,
	bool greyscale)
{
	if (palette == nullptr)
		return nullptr;

	PIXEL* converted = static_cast<PIXEL*>(
		MemAlloc(sizeof(PIXEL) * RenderPaletteBank::EntryCount));
	if (converted == nullptr)
		return nullptr;

	for (std::size_t index = 0;
		 index < RenderPaletteBank::EntryCount;
		 ++index)
	{
		std::uint32_t red = palette[index].peRed;
		std::uint32_t green = palette[index].peGreen;
		std::uint32_t blue = palette[index].peBlue;
		if (greyscale)
		{
			const std::uint32_t luminance =
				(red * 299 / 1000) +
				(green * 587 / 1000) +
				(blue * 114 / 1000);
			red = green = blue = (100 * luminance) / 256;
		}

		red = std::max(redScale, red);
		if (adjustGreen)
			green = std::max(greenScale, green);

		converted[index] = packColour(
			static_cast<std::uint8_t>(std::min(red, 255u)),
			static_cast<std::uint8_t>(std::min(green, 255u)),
			static_cast<std::uint8_t>(std::min(blue, 255u)));
	}

	if (!RegisterLegacyRenderPalette(converted))
	{
		MemFree(converted);
		return nullptr;
	}
	return converted;
}

bool adoptEffectShade(
	RenderPaletteBank& palette,
	std::size_t index,
	PIXEL* shade)
{
	if (shade == nullptr)
		return false;
	palette.adoptEffectShade(index, shade);
	return true;
}

bool adoptGlowShade(
	RenderPaletteBank& palette,
	std::size_t index,
	PIXEL* shade)
{
	if (shade == nullptr)
		return false;
	palette.adoptGlowShade(index, shade);
	return true;
}

bool adoptShade(
	RenderPaletteBank& palette,
	std::size_t index,
	PIXEL* shade)
{
	if (shade == nullptr)
		return false;
	palette.adoptShade(index, shade);
	return true;
}
}

bool RenderPaletteEffects::populateActorShades(
	RenderPaletteBank& palette)
{
	SGPPaletteEntry* const basePalette = palette.base8();
	if (basePalette == nullptr)
		return false;

	CreateRenderPaletteTables(palette, HVOBJECT_GLOW_GREEN);
	if (!adoptEffectShade(
			palette,
			0,
			Create16BPPPaletteShaded(
				basePalette, 100, 100, 100, TRUE)) ||
		!adoptEffectShade(
			palette,
			1,
			Create16BPPPaletteShaded(
				basePalette, 100, 150, 100, TRUE)) ||
		!adoptGlowShade(
			palette,
			0,
			Create16BPPPaletteShaded(
				basePalette, 255, 255, 255, FALSE)))
	{
		return false;
	}

	for (std::size_t index = 1; index < 10; ++index)
	{
		if (!adoptGlowShade(
				palette,
				index,
				createGlow(
					basePalette,
					RedGlowScale[index],
					255,
					false,
					false)))
		{
			return false;
		}
	}
	if (!adoptGlowShade(
			palette,
			10,
			Create16BPPPaletteShaded(
				basePalette, 100, 100, 100, TRUE)))
	{
		return false;
	}
	for (std::size_t index = 11; index < 19; ++index)
	{
		if (!adoptGlowShade(
				palette,
				index,
				createGlow(
					basePalette,
					RedGlowScale[index],
					0,
					false,
					true)))
		{
			return false;
		}
	}
	if (!adoptGlowShade(
			palette,
			19,
			createGlow(
				basePalette,
				RedGlowScale[18],
				0,
				false,
				true)) ||
		!adoptShade(
			palette,
			20,
			Create16BPPPaletteShaded(
				basePalette, 255, 255, 255, FALSE)))
	{
		return false;
	}

	for (std::size_t index = 21; index < 30; ++index)
	{
		if (!adoptShade(
				palette,
				index,
				createGlow(
					basePalette,
					OrangeGlowRedScale[index - 20],
					OrangeGlowGreenScale[index - 20],
					true,
					false)))
		{
			return false;
		}
	}
	if (!adoptShade(
			palette,
			30,
			Create16BPPPaletteShaded(
				basePalette, 100, 100, 100, TRUE)))
	{
		return false;
	}
	for (std::size_t index = 31; index < 39; ++index)
	{
		if (!adoptShade(
				palette,
				index,
				createGlow(
					basePalette,
					OrangeGlowRedScale[index - 20],
					OrangeGlowGreenScale[index - 20],
					true,
					true)))
		{
			return false;
		}
	}
	return adoptShade(
		palette,
		39,
		createGlow(
			basePalette,
			OrangeGlowRedScale[18],
			OrangeGlowGreenScale[18],
			true,
			true));
}
