#include "LibraryDataBase.h"

// This table is retained for the legacy LibraryDataBase ABI only. SLF mounting
// is owned by bfVFS, and LibraryDataBase.cpp no longer consumes this metadata.
// Language archives therefore belong to the runtime language/package catalog,
// not to a preprocessor-shaped legacy enum and array.
LibraryInitHeader gGameLibaries[ ] = 
{ 
		//Library Name					Can be	Init at start
//													on cd
	{ "Data.slf",							FALSE, TRUE },
	{ "Editor.slf",                     FALSE, FALSE },
	{ "Ambient.slf",					FALSE, TRUE },
	{ "Anims.slf",						FALSE, TRUE },
	{ "BattleSnds.slf",				FALSE, TRUE },
	{ "BigItems.slf",					FALSE, TRUE },
	{ "BinaryData.slf",				FALSE, TRUE },
	{ "Cursors.slf",					FALSE, TRUE },
	{ "Faces.slf",						FALSE, TRUE },
	{ "Fonts.slf",						FALSE, TRUE },
	{ "Interface.slf",				FALSE, TRUE },
	{ "Laptop.slf",						FALSE, TRUE },
	{ "Maps.slf",							TRUE,	TRUE },
	{ "MercEdt.slf",					FALSE, TRUE },
	{ "Music.slf",						TRUE,	TRUE },
	{ "Npc_Speech.slf",				TRUE,	TRUE },
	{ "NpcData.slf",					FALSE, TRUE },
	{ "RadarMaps.slf",				FALSE, TRUE },
	{ "Sounds.slf",						FALSE, TRUE },
	{ "Speech.slf",						TRUE,	TRUE },
//	{ "TileCache.slf",				FALSE, TRUE },
	{ "TileSets.slf",					TRUE,	TRUE },
	{ "LoadScreens.slf",			TRUE,	TRUE },
	{ "Intro.slf",						TRUE,	TRUE },
};

static_assert(sizeof(gGameLibaries) / sizeof(gGameLibaries[0]) ==
	NUMBER_OF_LIBRARIES,
	"legacy library metadata and IDs must stay aligned");
