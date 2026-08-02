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

struct CLOTHES_STRUCT
{
	UINT16 uiIndex;
	CHAR16 szName[80];
	PaletteRepID vest;
	PaletteRepID pants;
};

#define CLOTHES_MAX 50

extern CLOTHES_STRUCT Clothes[CLOTHES_MAX];

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
