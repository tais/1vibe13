#include "PaletteTable.h"

#include "FileMan.h"
#include "MemMan.h"
#include "Render Palette Effects.h"

#include <cstring>
#include <utility>

namespace LogicalBodyTypes
{

PaletteTable::PaletteTable() = default;

PaletteTable::~PaletteTable() = default;

bool PaletteTable::CreateSGPPaletteFromActFile(
	SGPPaletteEntry* palette, std::string fileName)
{
	char* colFileName = new char[fileName.size() + 1];
	std::strcpy(colFileName, fileName.c_str());
	if (!FileExists(colFileName))
	{
		DebugMsg(
			TOPIC_JA2, DBG_LEVEL_3,
			"Cannot find COL file");
		delete[] colFileName;
		return false;
	}

	HWFILE file = FileOpen(
		colFileName, FILE_ACCESS_READ, FALSE);
	if (file == 0)
	{
		DebugMsg(
			TOPIC_JA2, DBG_LEVEL_3,
			"Cannot open COL file");
		delete[] colFileName;
		return false;
	}

	std::memset(
		palette, 0,
		sizeof(SGPPaletteEntry) *
		RenderPaletteBank::EntryCount);
	for (std::size_t index = 0;
	     index < RenderPaletteBank::EntryCount;
	     ++index)
	{
		if (!FileRead(
				file, &palette[index].peRed,
				sizeof(UINT8), nullptr) ||
			!FileRead(
				file, &palette[index].peGreen,
				sizeof(UINT8), nullptr) ||
			!FileRead(
				file, &palette[index].peBlue,
				sizeof(UINT8), nullptr))
		{
			DebugMsg(
				TOPIC_JA2, DBG_LEVEL_3,
				"Short read on COL file");
			FileClose(file);
			delete[] colFileName;
			return false;
		}
	}

	FileClose(file);
	delete[] colFileName;
	return true;
}

bool PaletteTable::Load(std::string fileName)
{
	SGPPaletteEntry sourcePalette[
		RenderPaletteBank::EntryCount]{};
	if (!CreateSGPPaletteFromActFile(
			sourcePalette, std::move(fileName)))
	{
		return false;
	}

	RenderPaletteBank loadedPalette;
	SGPPaletteEntry* base8 =
		static_cast<SGPPaletteEntry*>(
			MemAlloc(
				sizeof(SGPPaletteEntry) *
				RenderPaletteBank::EntryCount));
	if (base8 == nullptr)
	{
		return false;
	}
	loadedPalette.adoptBase8(base8);
	std::memcpy(
		loadedPalette.base8(), sourcePalette,
		sizeof(SGPPaletteEntry) *
		RenderPaletteBank::EntryCount);
	loadedPalette.adoptBase16(
		Create16BPPPalette(loadedPalette.base8()));

	if (!RenderPaletteEffects::populateActorShades(loadedPalette))
		return false;

	palette_.swapStorage(loadedPalette);
	return true;
}

}
