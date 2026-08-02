/** 
 * @file
 * @author feynman (bears-pit.com)
 */

#include "../include/LegacyZombiePlan.h"
#include "../../Tactical/TacticalActor.h" // definition of TacticalActor

INT8 ZombieDecideAction( TacticalActor *pSoldier ); // defined in DecideAction.cpp

namespace AI
{
    namespace tactical
    {
        LegacyZombiePlan::LegacyZombiePlan(TacticalActor* npc)
            : Plan(npc)
        {
        }

        void LegacyZombiePlan::execute(PlanInputData& environment)
        {
            get_npc()->aiPlanning().action() = ZombieDecideAction(get_npc());
        }
    }
}

