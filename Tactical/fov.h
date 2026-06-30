#ifndef __FOV_H
#define __FOV_H

// fov.h only uses SOLDIERTYPE by pointer, so it does not need the full (heavy)
// Overhead.h -- a forward declaration plus the base integer types is enough.
#include "types.h"
#include "fwd_decl.h"

void RevealRoofsAndItems(SOLDIERTYPE *pSoldier, UINT32 itemsToo, BOOLEAN fShowLocators, UINT8 ubLevel, BOOLEAN fForce );

INT32 GetFreeSlantRoof( void );
void RecountSlantRoofs( void );
void ClearSlantRoofs( void );
BOOLEAN FindSlantRoofSlot( INT32 sGridNo );
void AddSlantRoofFOVSlot( INT32 sGridNo );
void ExamineSlantRoofFOVSlots( );









#endif
