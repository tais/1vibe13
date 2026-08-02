#ifndef _SOLDIER_FUNCTIONS_H
#define _SOLDIER_FUNCTIONS_H

#include "TacticalActorCrowBehavior.h"
#include "TacticalActorBleeding.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorInteractions.h"
#include "TacticalActorWorldPlacement.h"
#include "types.h"

class TacticalActor;


BOOLEAN ReevaluateEnemyStance( TacticalActor *pSoldier, UINT16 usAnimState );

#endif
