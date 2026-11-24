#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_NATIVE_SHADE_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_PASSIVE_UI_NATIVE_SHADE_H

#include "vobject.h"

#include <cstdint>

struct FullEngineCoopClientPassiveRadarShadeServices
{
	using Create = PIXEL* (*)(const SGPPaletteEntry* palette,
		void* context) noexcept;
	using Release = void (*)(PIXEL* palette, void* context) noexcept;
	Create create = nullptr;
	Release release = nullptr;
	void* context = nullptr;
};

enum class FullEngineCoopClientPassiveRadarShadeResult : std::uint8_t
{
	Success,
	InvalidInput,
	SharedPaletteState,
	NonFreshPaletteState,
	AllocationFailure,
	AliasedAllocation
};

// Installs one neutral day palette into a freshly loaded radar VObject.  Every
// validation precedes publication; after success DeleteVideoObject owns the
// new pShades[0] allocation and releases it exactly once with the base palette.
FullEngineCoopClientPassiveRadarShadeResult
InstallFullEngineCoopClientPassiveRadarDayShade(
	HVOBJECT object,
	const FullEngineCoopClientPassiveRadarShadeServices& services) noexcept;

#endif
