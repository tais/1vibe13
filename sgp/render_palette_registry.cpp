#include "render_palette_registry.h"

#include <Engine/Core/StableResourceRegistry.h>

#include <limits>
#include <optional>
#include <unordered_map>

namespace
{
struct RenderPaletteResource
{
	const PIXEL* palette = nullptr;
};

using RenderPaletteRegistry =
	StableResourceRegistry<RenderPaletteResource, RenderPaletteId>;

RenderPaletteRegistry gRenderPalettes(RenderPaletteRegistry::Limits{
	RenderPaletteId{1} << 32, 1,
	std::numeric_limits<RenderPaletteId>::max()});
std::unordered_map<const PIXEL*, RenderPaletteId> gRenderPaletteHandles;
}

bool RegisterLegacyRenderPalette(
	const PIXEL* palette, RenderPaletteId* identity) noexcept
{
	if (!palette) return false;
	const auto existing = gRenderPaletteHandles.find(palette);
	if (existing != gRenderPaletteHandles.end())
	{
		if (identity) *identity = existing->second;
		return true;
	}

	std::optional<RenderPaletteId> registered;
	try
	{
		registered =
			gRenderPalettes.insert(RenderPaletteResource{palette});
	}
	catch (...)
	{
		return false;
	}
	if (!registered) return false;

	try
	{
		const auto inserted =
			gRenderPaletteHandles.emplace(palette, *registered);
		if (inserted.second)
		{
			if (identity) *identity = *registered;
			return true;
		}
	}
	catch (...)
	{
	}
	(void)gRenderPalettes.erase(*registered);
	return false;
}

void UnregisterLegacyRenderPalette(const PIXEL* palette) noexcept
{
	const auto found = gRenderPaletteHandles.find(palette);
	if (found == gRenderPaletteHandles.end()) return;
	const RenderPaletteId identity = found->second;
	gRenderPaletteHandles.erase(found);
	(void)gRenderPalettes.erase(identity);
}

bool FindLegacyRenderPalette(
	const PIXEL* palette, RenderPaletteId& identity) noexcept
{
	const auto found = gRenderPaletteHandles.find(palette);
	if (found == gRenderPaletteHandles.end()) return false;
	identity = found->second;
	return true;
}

const PIXEL* ResolveLegacyRenderPalette(
	RenderPaletteId identity) noexcept
{
	RenderPaletteResource* const palette =
		gRenderPalettes.find(identity);
	return palette ? palette->palette : nullptr;
}

std::size_t LegacyRenderPaletteCount() noexcept
{
	return gRenderPalettes.size();
}
