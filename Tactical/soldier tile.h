#ifndef __SOLDIER_TILE_H
#define __SOLDIER_TILE_H

#include "types.h"

class TacticalActor;

#define			MOVE_TILE_CLEAR										1
#define			MOVE_TILE_TEMP_BLOCKED					-1
#define			MOVE_TILE_STATIONARY_BLOCKED		-2



INT8 TileIsClear( TacticalActor *pSoldier, INT8 bDirection, INT32 sGridNo, INT8 bLevel );

void MarkMovementReserved( TacticalActor *pSoldier, INT32 sGridNo );

void UnMarkMovementReserved( TacticalActor *pSoldier );

BOOLEAN HandleNextTile(
	TacticalActor *pSoldier,
	INT8 bDirection,
	INT32 sGridNo,
	INT32 sFinalDestTile,
	BOOLEAN fReplicateStance = TRUE );//dnl ch53 111009

BOOLEAN HandleNextTileWaiting( TacticalActor *pSoldier );

BOOLEAN TeleportSoldier( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fForce );

BOOLEAN SwapMercPositions( TacticalActor *pSoldier1, TacticalActor *pSoldier2 );

void SetDelayedTileWaiting( TacticalActor *pSoldier, INT32 sCauseGridNo, UINT8 bValue );

BOOLEAN CanExchangePlaces( TacticalActor *pSoldier1, TacticalActor *pSoldier2, BOOLEAN fShow );

#endif
