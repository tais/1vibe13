#ifndef __HANDLEUIPLAN_H
#define __HANDLEUIPLAN_H

#include <Engine/Adapters/JA2/TacticalEntity.h>

#include "types.h"

#define		UIPLAN_ACTION_MOVETO			1
#define		UIPLAN_ACTION_FIRE				2


BOOLEAN BeginUIPlan( TacticalEntityId actor );
BOOLEAN AddUIPlan( INT32 sGridNo, UINT8 ubPlanID );
void EndUIPlan(	);
BOOLEAN InUIPlanMode( );


#endif
