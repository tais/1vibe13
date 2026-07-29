/** 
 * @file
 * @author feynman (bears-pit.com)
 */

#include "../include/LegacyCreaturePlan.h"
#include "../../Tactical/Soldier Control.h"

INT8 CreatureDecideAction( TacticalActor *pSoldier );

namespace AI
{
    namespace tactical
    {
        LegacyCreaturePlan::LegacyCreaturePlan(TacticalActor* npc)
            : Plan(npc)
        {
        }

        void LegacyCreaturePlan::execute(PlanInputData& environment)
        {
            get_npc()->aiPlanning().action() = CreatureDecideAction( get_npc() );
        }
    }
}

