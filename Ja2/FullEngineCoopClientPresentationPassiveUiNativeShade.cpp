#include "FullEngineCoopClientPresentationPassiveUiNativeShade.h"

FullEngineCoopClientPassiveRadarShadeResult
InstallFullEngineCoopClientPassiveRadarDayShade(
	HVOBJECT object,
	const FullEngineCoopClientPassiveRadarShadeServices& services) noexcept
{
	using Result = FullEngineCoopClientPassiveRadarShadeResult;
	if (object == nullptr || services.create == nullptr ||
		services.release == nullptr || object->pPaletteEntry == nullptr ||
		object->p16BPPPalette == nullptr)
		return Result::InvalidInput;
	if ((object->fFlags & VOBJECT_FLAG_SHADETABLE_SHARED) != 0)
		return Result::SharedPaletteState;
	if (object->pShadeCurrent != object->p16BPPPalette)
		return Result::NonFreshPaletteState;
	for (PIXEL* shade : object->pShades)
		if (shade != nullptr) return Result::NonFreshPaletteState;

	PIXEL* const day = services.create(
		object->pPaletteEntry, services.context);
	if (day == nullptr) return Result::AllocationFailure;
	if (day == object->p16BPPPalette)
	{
		// The creator contract requires a fresh allocation. Equality means it
		// returned this object's borrowed base palette; releasing it here would
		// corrupt the VObject and cause a later double-free.
		return Result::AliasedAllocation;
	}

	// No fallible operation follows publication.  Normal VObject destruction
	// now owns both this slot and the distinct p16BPPPalette allocation.
	object->pShades[0] = day;
	object->pShadeCurrent = day;
	return Result::Success;
}
