#include "Soldier Palette.h"

#include "DEBUG.H"
#include "FileMan.h"
#include "MemMan.h"
#include "WCheck.h"

#define PALETTEFILENAME "BINARYDATA\\ja2pal.dat"

UINT8* gubpNumReplacementsPerRange;
PaletteSubRangeType* gpPaletteSubRanges;
PaletteReplacementType* gpPalRep;
UINT32 guiNumPaletteSubRanges;
UINT32 guiNumReplacements;

BOOLEAN LoadPaletteData()
{
	HWFILE hFile;
	UINT32 cnt, cnt2;

	hFile = FileOpen(PALETTEFILENAME, FILE_ACCESS_READ, FALSE);

	if (!FileRead(hFile, &guiNumPaletteSubRanges, sizeof(guiNumPaletteSubRanges), (UINT32*)NULL))
	{
		return FALSE;
	}

	gpPaletteSubRanges = (PaletteSubRangeType*)MemAlloc(sizeof(PaletteSubRangeType) * guiNumPaletteSubRanges);
	gubpNumReplacementsPerRange = (UINT8*)MemAlloc(sizeof(UINT8) * guiNumPaletteSubRanges);

	for (cnt = 0; cnt < guiNumPaletteSubRanges; ++cnt)
	{
		if (!FileRead(hFile, &gubpNumReplacementsPerRange[cnt], sizeof(UINT8), (UINT32*)NULL))
		{
			return FALSE;
		}
	}

	for (cnt = 0; cnt < guiNumPaletteSubRanges; ++cnt)
	{
		if (!FileRead(hFile, &gpPaletteSubRanges[cnt].ubStart, sizeof(UINT8), (UINT32*)NULL))
		{
			return FALSE;
		}
		if (!FileRead(hFile, &gpPaletteSubRanges[cnt].ubEnd, sizeof(UINT8), (UINT32*)NULL))
		{
			return FALSE;
		}
	}

	if (!FileRead(hFile, &guiNumReplacements, sizeof(guiNumReplacements), (UINT32*)NULL))
	{
		return FALSE;
	}

	gpPalRep = (PaletteReplacementType*)MemAlloc(sizeof(PaletteReplacementType) * guiNumReplacements);

	for (cnt = 0; cnt < guiNumReplacements; ++cnt)
	{
		if (!FileRead(hFile, &gpPalRep[cnt].ubType, sizeof(gpPalRep[cnt].ubType), (UINT32*)NULL))
		{
			return FALSE;
		}

		if (!FileRead(hFile, &gpPalRep[cnt].ID, sizeof(gpPalRep[cnt].ID), (UINT32*)NULL))
		{
			return FALSE;
		}

		if (!FileRead(hFile, &gpPalRep[cnt].ubPaletteSize, sizeof(gpPalRep[cnt].ubPaletteSize), (UINT32*)NULL))
		{
			return FALSE;
		}

		gpPalRep[cnt].r = (UINT8*)MemAlloc(gpPalRep[cnt].ubPaletteSize);
		CHECKF(gpPalRep[cnt].r != NULL);
		gpPalRep[cnt].g = (UINT8*)MemAlloc(gpPalRep[cnt].ubPaletteSize);
		CHECKF(gpPalRep[cnt].g != NULL);
		gpPalRep[cnt].b = (UINT8*)MemAlloc(gpPalRep[cnt].ubPaletteSize);
		CHECKF(gpPalRep[cnt].b != NULL);

		for (cnt2 = 0; cnt2 < gpPalRep[cnt].ubPaletteSize; ++cnt2)
		{
			if (!FileRead(hFile, &gpPalRep[cnt].r[cnt2], sizeof(UINT8), (UINT32*)NULL))
			{
				return FALSE;
			}
			if (!FileRead(hFile, &gpPalRep[cnt].g[cnt2], sizeof(UINT8), (UINT32*)NULL))
			{
				return FALSE;
			}
			if (!FileRead(hFile, &gpPalRep[cnt].b[cnt2], sizeof(UINT8), (UINT32*)NULL))
			{
				return FALSE;
			}
		}
	}

	FileClose(hFile);

	return TRUE;
}

BOOLEAN SetPaletteReplacement(SGPPaletteEntry* p8BPPPalette, PaletteRepID aPalRep)
{
	UINT32 cnt2;
	UINT8 ubType;
	UINT8 ubPalIndex;

	CHECKF(GetPaletteRepIndexFromID(aPalRep, &ubPalIndex));

	ubType = gpPalRep[ubPalIndex].ubType;

	for (cnt2 = gpPaletteSubRanges[ubType].ubStart; cnt2 <= gpPaletteSubRanges[ubType].ubEnd; ++cnt2)
	{
		p8BPPPalette[cnt2].peRed = gpPalRep[ubPalIndex].r[cnt2 - gpPaletteSubRanges[ubType].ubStart];
		p8BPPPalette[cnt2].peGreen = gpPalRep[ubPalIndex].g[cnt2 - gpPaletteSubRanges[ubType].ubStart];
		p8BPPPalette[cnt2].peBlue = gpPalRep[ubPalIndex].b[cnt2 - gpPaletteSubRanges[ubType].ubStart];
	}

	return TRUE;
}

BOOLEAN DeletePaletteData()
{
	UINT32 cnt;

	if (gpPaletteSubRanges != NULL)
	{
		MemFree(gpPaletteSubRanges);
		gpPaletteSubRanges = NULL;
	}

	if (gubpNumReplacementsPerRange != NULL)
	{
		MemFree(gubpNumReplacementsPerRange);
		gubpNumReplacementsPerRange = NULL;
	}

	for (cnt = 0; cnt < guiNumReplacements; ++cnt)
	{
		if (gpPalRep[cnt].r != NULL)
		{
			MemFree(gpPalRep[cnt].r);
			gpPalRep[cnt].r = NULL;
		}
		if (gpPalRep[cnt].g != NULL)
		{
			MemFree(gpPalRep[cnt].g);
			gpPalRep[cnt].g = NULL;
		}
		if (gpPalRep[cnt].b != NULL)
		{
			MemFree(gpPalRep[cnt].b);
			gpPalRep[cnt].b = NULL;
		}
	}

	if (gpPalRep != NULL)
	{
		MemFree(gpPalRep);
		gpPalRep = NULL;
	}

	return TRUE;
}

BOOLEAN GetPaletteRepIndexFromID(const CHAR8* aPalRep, UINT8* pubPalIndex)
{
	for (UINT32 cnt = 0; cnt < guiNumReplacements; ++cnt)
	{
		if (COMPARE_PALETTEREP_ID(aPalRep, gpPalRep[cnt].ID))
		{
			*pubPalIndex = (UINT8)cnt;
			return TRUE;
		}
	}

	DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "Invalid Palette Replacement ID given");
	return FALSE;
}
