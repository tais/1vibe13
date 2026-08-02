#ifndef _SOLDIER_FUNCTIONS_H
#define _SOLDIER_FUNCTIONS_H

#include "TacticalActorCrowBehavior.h"
#include "types.h"

class TacticalActor;


FLOAT CalcSoldierNextBleed( TacticalActor *pSoldier );
FLOAT CalcSoldierNextUnmovingBleed( TacticalActor *pSoldier );
BOOLEAN ReevaluateEnemyStance( TacticalActor *pSoldier, UINT16 usAnimState );

void HandlePlacingRoofMarker( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fSet, BOOLEAN fForce );

void PickPickupAnimation( TacticalActor *pSoldier, INT32 iItemIndex, INT32 sGridNo, INT8 bZLevel );

BOOLEAN MercStealFromMerc( TacticalActor *pSoldier, TacticalActor *pTarget );

BOOLEAN DoesSoldierWearGasMask(TacticalActor *pSoldier);//dnl ch40 200909

#endif
