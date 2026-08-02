/** 
 * @file
 * @author feynman (bears-pit.com)
 */

#include "../include/NullPlanFactory.h"
#include "../include/NullPlan.h"
#include "../Tactical/TacticalActor.h"

namespace AI
{
    namespace tactical
    {
        Plan* NullPlanFactory::create_plan(TacticalActor* npc, const AIInputData& input)
        {
            return new NullPlan(npc);
        }

        void NullPlanFactory::update_plan(TacticalActor* npc, const AIInputData& input)
        {
            // the idea is to do nothing, so let's do it...
        }
    }
}

