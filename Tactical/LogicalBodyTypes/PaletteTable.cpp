#include "PaletteTable.h"

#include "FileMan.h"
#include "MemMan.h"

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

	CreateRenderPaletteTables(
		loadedPalette, HVOBJECT_GLOW_GREEN);

	loadedPalette.adoptEffectShade(
		0, Create16BPPPaletteShaded(
			loadedPalette.base8(), 100, 100, 100, TRUE));
	loadedPalette.adoptEffectShade(
		1, Create16BPPPaletteShaded(
			loadedPalette.base8(), 100, 150, 100, TRUE));

	loadedPalette.adoptGlowShade(
		0, Create16BPPPaletteShaded(
			loadedPalette.base8(), 255, 255, 255, FALSE));
	for (std::size_t index = 1; index < 10; ++index)
	{
		loadedPalette.adoptGlowShade(
			index, CreateEnemyGlow16BPPPalette(
				loadedPalette.base8(), gRedGlowR[index],
				255, FALSE));
	}
	loadedPalette.adoptGlowShade(
		10, Create16BPPPaletteShaded(
			loadedPalette.base8(), 100, 100, 100, TRUE));
	for (std::size_t index = 11; index < 19; ++index)
	{
		loadedPalette.adoptGlowShade(
			index, CreateEnemyGreyGlow16BPPPalette(
				loadedPalette.base8(), gRedGlowR[index],
				0, FALSE));
	}
	loadedPalette.adoptGlowShade(
		19, CreateEnemyGreyGlow16BPPPalette(
			loadedPalette.base8(), gRedGlowR[18], 0, FALSE));

	loadedPalette.adoptShade(
		20, Create16BPPPaletteShaded(
			loadedPalette.base8(), 255, 255, 255, FALSE));
	for (std::size_t index = 21; index < 30; ++index)
	{
		loadedPalette.adoptShade(
			index, CreateEnemyGlow16BPPPalette(
				loadedPalette.base8(), gOrangeGlowR[index - 20],
				gOrangeGlowG[index - 20], TRUE));
	}
	loadedPalette.adoptShade(
		30, Create16BPPPaletteShaded(
			loadedPalette.base8(), 100, 100, 100, TRUE));
	for (std::size_t index = 31; index < 39; ++index)
	{
		loadedPalette.adoptShade(
			index, CreateEnemyGreyGlow16BPPPalette(
				loadedPalette.base8(), gOrangeGlowR[index - 20],
				gOrangeGlowG[index - 20], TRUE));
	}
	loadedPalette.adoptShade(
		39, CreateEnemyGreyGlow16BPPPalette(
			loadedPalette.base8(), gOrangeGlowR[18],
			gOrangeGlowG[18], TRUE));

	palette_.swapStorage(loadedPalette);
	return true;
}

}
