#ifndef _SOLDIER_FUNCTIONS_H
#define _SOLDIER_FUNCTIONS_H

#include "Soldier Control.h"


void ContinueMercMovement( TacticalActor *pSoldier );

BOOLEAN IsValidStance( TacticalActor *pSoldier, INT8 bNewStance );
void SelectMoveAnimationFromStance( TacticalActor *pSoldier );
BOOLEAN IsValidMovementMode( TacticalActor *pSoldier, INT16 usMovementMode );
FLOAT CalcSoldierNextBleed( TacticalActor *pSoldier );
FLOAT CalcSoldierNextUnmovingBleed( TacticalActor *pSoldier );
void SoldierCollapse( TacticalActor *pSoldier );

BOOLEAN ReevaluateEnemyStance( TacticalActor *pSoldier, UINT16 usAnimState );

void HandlePlacingRoofMarker( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fSet, BOOLEAN fForce );

void PickPickupAnimation( TacticalActor *pSoldier, INT32 iItemIndex, INT32 sGridNo, INT8 bZLevel );

BOOLEAN MercStealFromMerc( TacticalActor *pSoldier, TacticalActor *pTarget );

void HandleCrowShadowVisibility( TacticalActor *pSoldier );

BOOLEAN DoesSoldierWearGasMask(TacticalActor *pSoldier);//dnl ch40 200909

#endif
