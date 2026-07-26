#ifndef __WORLD_TILE_MAP_H
#define __WORLD_TILE_MAP_H

#include "types.h"

// Own the process-local MAP_ELEMENT array without changing the legacy map
// representation or any on-disk map format. gpWorldLevelData remains a
// compatibility projection published by this owner.
BOOLEAN AllocateWorldTileMap(UINT32 tileCount);
void ResetWorldTileMap();
void ReleaseWorldTileMap();
UINT32 GetWorldTileMapSize();

#endif
