/** 
 * @file
 * @author feynman (bears-pit.com)
 */

#include "../include/LegacyZombiePlan.h"
#include "../../Tactical/Soldier Control.h" // definition of SOLDIERTYPE

INT8 ZombieDecideAction( SOLDIERTYPE *pSoldier ); // defined in DecideAction.cpp

namespace AI
{
    namespace tactical
    {
        LegacyZombiePlan::LegacyZombiePlan(SOLDIERTYPE* npc)
            : Plan(npc)
        {
        }

        void LegacyZombiePlan::execute(PlanInputData& environment)
        {
            get_npc()->aiPlanning().action() = ZombieDecideAction(get_npc());
        }
    }
}

