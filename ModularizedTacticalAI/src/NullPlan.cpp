/** 
 * @file
 * @author feynman (bears-pit.com)
 */

#include "../include/NullPlan.h"

#include "../../TacticalAI/ai.h" // for EndAIGuysTurn
#include "../../Tactical/TacticalActor.h"

namespace AI
{
    namespace tactical
    {
        NullPlan::NullPlan(TacticalActor* npc)
            : Plan(npc)
        {
        }

        /// Simply set the action to be performed to AI_ACTION_NONE
        void NullPlan::execute(PlanInputData& environment)
        {
            get_npc()->aiPlanning().action() = AI_ACTION_NONE;
        }
    }
}

