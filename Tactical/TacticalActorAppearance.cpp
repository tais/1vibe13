#include "TacticalActorAppearance.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "LogicalBodyTypes/PaletteTable.h"
#include "MemMan.h"
#include "Render Palette Bank.h"
#include "Render Palette Effects.h"
#include "Soldier Palette.h"
#include "TacticalActor.h"
#include "shading.h"

#include <cstring>

bool TacticalActorAppearance::rebuildPalettes(TacticalActor& actor)
{
	if (actor.identity().bodyType() >= TOTALBODYTYPES ||
		actor.animationPlayback().state() >= NUMANIMATIONSTATES)
	{
		return false;
	}

	CHAR8 colourFilename[100];
	const UINT16 animationSurface = GetSoldierAnimationSurface(
		&actor,
		actor.animationPlayback().state());
	const INT8 bodyTypePalette = GetBodyTypePaletteSubstitutionCode(
		&actor,
		actor.identity().bodyType(),
		colourFilename);
	if (bodyTypePalette >= 0 &&
		animationSurface >= NUMANIMATIONSURFACETYPES)
	{
		return false;
	}
	if (bodyTypePalette >= 0 &&
		(gAnimSurfaceDatabase[animationSurface].hVideoObject == nullptr ||
		 gAnimSurfaceDatabase[animationSurface].hVideoObject->pPaletteEntry ==
			nullptr))
	{
		return false;
	}

	SGPPaletteEntry temporaryPalette[RenderPaletteBank::EntryCount];
	RenderPaletteBank rebuiltPalette;
	SGPPaletteEntry* base8 = static_cast<SGPPaletteEntry*>(
		MemAlloc(
			sizeof(SGPPaletteEntry) *
			RenderPaletteBank::EntryCount));
	if (base8 == nullptr)
		return false;
	rebuiltPalette.adoptBase8(base8);
	std::memset(
		rebuiltPalette.base8(),
		0,
		sizeof(SGPPaletteEntry) * RenderPaletteBank::EntryCount);

	if (bodyTypePalette == -1)
	{
		const UINT16 paletteAnimationSurface =
			LoadSoldierAnimationSurface(&actor, STANDING);
		if (paletteAnimationSurface != INVALID_ANIMATION_SURFACE)
		{
			if (paletteAnimationSurface >= NUMANIMATIONSURFACETYPES ||
				gAnimSurfaceDatabase[paletteAnimationSurface]
					.hVideoObject == nullptr ||
				gAnimSurfaceDatabase[paletteAnimationSurface]
					.hVideoObject->pPaletteEntry == nullptr)
			{
				return false;
			}
			std::memcpy(
				rebuiltPalette.base8(),
				gAnimSurfaceDatabase[paletteAnimationSurface]
					.hVideoObject->pPaletteEntry,
				sizeof(SGPPaletteEntry) *
					RenderPaletteBank::EntryCount);
			SetPaletteReplacement(
				rebuiltPalette.base8(),
				actor.renderState().headPalette());
			SetPaletteReplacement(
				rebuiltPalette.base8(),
				actor.renderState().vestPalette());
			SetPaletteReplacement(
				rebuiltPalette.base8(),
				actor.renderState().pantsPalette());
			SetPaletteReplacement(
				rebuiltPalette.base8(),
				actor.renderState().skinPalette());
		}
	}
	else if (bodyTypePalette == 0)
	{
		std::memcpy(
			rebuiltPalette.base8(),
			gAnimSurfaceDatabase[animationSurface]
				.hVideoObject->pPaletteEntry,
			sizeof(SGPPaletteEntry) * RenderPaletteBank::EntryCount);
	}
	else if (CreateSGPPaletteFromCOLFile(
		temporaryPalette,
		colourFilename))
	{
		std::memcpy(
			rebuiltPalette.base8(),
			temporaryPalette,
			sizeof(SGPPaletteEntry) * RenderPaletteBank::EntryCount);
	}
	else
	{
		std::memcpy(
			rebuiltPalette.base8(),
			gAnimSurfaceDatabase[animationSurface]
				.hVideoObject->pPaletteEntry,
			sizeof(SGPPaletteEntry) * RenderPaletteBank::EntryCount);
	}

	rebuiltPalette.adoptBase16(
		Create16BPPPalette(rebuiltPalette.base8()));
	if (!RenderPaletteEffects::populateActorShades(rebuiltPalette))
		return false;

	actor.palette().swapStorage(rebuiltPalette);
	return true;
}
