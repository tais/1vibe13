#ifndef __SOLDIER_ANI_H
#define __SOLDIER_ANI_H

BOOLEAN AdjustToNextAnimationFrame( TacticalActor *pSoldier );

BOOLEAN CheckForAndHandleSoldierDeath( TacticalActor *pSoldier, BOOLEAN *pfMadeCorpse );

BOOLEAN CheckForAndHandleSoldierDyingNotFromHit( TacticalActor *pSoldier );

BOOLEAN HandleSoldierDeath( TacticalActor *pSoldier , BOOLEAN *pfMadeCorpse );

BOOLEAN OKFallDirection( TacticalActor *pSoldier, INT32 sGridNo, INT8 bLevel, UINT8 ubTestDirection, UINT16 usAnimState );

BOOLEAN HandleCheckForDeathCommonCode( TacticalActor *pSoldier );

void KickOutWheelchair( TacticalActor *pSoldier );

#endif
