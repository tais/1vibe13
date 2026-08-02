#ifndef TACTICAL_GRID_DIRECTION_H
#define TACTICAL_GRID_DIRECTION_H

#include "types.h"

class TacticalActor;

UINT8 GetDirectionFromXY(INT16 x, INT16 y, TacticalActor* actor);
BOOLEAN GetDirectionChangeAmount(
	INT32 gridNo, TacticalActor* actor, UINT8 turnAmount);
UINT8 GetDirectionFromGridNo(INT32 gridNo, TacticalActor* actor);
UINT8 atan8(INT16 x, INT16 y, INT16 sourceX, INT16 sourceY);
UINT8 atan8FromAngle(DOUBLE angle);
INT16 GetDirectionToGridNoFromGridNo(INT32 destination, INT32 source);
INT16 GetDirectionFromCenterCellXYGridNo(INT32 destination, INT32 source);

#endif
