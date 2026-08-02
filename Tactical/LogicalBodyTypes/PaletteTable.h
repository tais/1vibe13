#pragma once

#ifndef _LBT_PALETTETABLE__H_
#define _LBT_PALETTETABLE__H_

#include "Render Palette Bank.h"
#include "Utilities.h"
#include "lighting.h"

namespace LogicalBodyTypes {

class PaletteTable {

public:
	PaletteTable();
	~PaletteTable(void);
	bool Load(std::string fileName);
	RenderPaletteBank& palette() noexcept { return palette_; }
	const RenderPaletteBank& palette() const noexcept { return palette_; }

private:
	bool CreateSGPPaletteFromActFile(SGPPaletteEntry *pPalette, std::string fileName);
	RenderPaletteBank palette_;

};

}

#endif
