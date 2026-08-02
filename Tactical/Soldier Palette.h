#ifndef TACTICAL_SOLDIER_PALETTE_H
#define TACTICAL_SOLDIER_PALETTE_H

#include "Overhead Types.h"
#include "vobject.h"

enum
{
	UNIFORM_ENEMY_ADMIN = 0,
	UNIFORM_ENEMY_TROOP,
	UNIFORM_ENEMY_ELITE,
	UNIFORM_MILITIA_ROOKIE,
	UNIFORM_MILITIA_REGULAR,
	UNIFORM_MILITIA_ELITE,
	NUM_UNIFORMS,
};

struct UNIFORMCOLORS
{
	PaletteRepID vest;
	PaletteRepID pants;
};

extern UNIFORMCOLORS gUniformColors[NUM_UNIFORMS];

extern UINT32 guiNumPaletteSubRanges;
extern UINT8* gubpNumReplacementsPerRange;
extern PaletteSubRangeType* gpPaletteSubRanges;
extern UINT32 guiNumReplacements;
extern PaletteReplacementType* gpPalRep;

BOOLEAN GetPaletteRepIndexFromID(const CHAR8* paletteId, UINT8* paletteIndex);
BOOLEAN SetPaletteReplacement(
	SGPPaletteEntry* palette, PaletteRepID paletteReplacement);
BOOLEAN LoadPaletteData();
BOOLEAN DeletePaletteData();

#endif
