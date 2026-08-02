#ifndef STRATEGIC_PATH_TYPES_H
#define STRATEGIC_PATH_TYPES_H

#include "types.h"

// Stable strategic-route node shared by map, vehicle, militia, and movement
// APIs. Keep this layout unchanged: legacy save and route code allocate it by
// its concrete size.
struct path
{
	UINT32 uiSectorId;
	UINT32 uiEta;
	BOOLEAN fSpeed;
	path* pNext;
	path* pPrev;
};

using PathSt = path;
using PathStPtr = PathSt*;

#endif
