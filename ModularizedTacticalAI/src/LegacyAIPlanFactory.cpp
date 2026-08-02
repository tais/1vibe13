/** 
 * @file
 * @author feynman (bears-pit.com)
 */
#include "../include/LegacyAIPlanFactory.h"
#include "TacticalActorConditions.h"
#include "../include/LegacyAIPlan.h"
#include "../include/LegacyCreaturePlan.h"
#include "../include/LegacyZombiePlan.h"
#include "../include/LegacyArmedVehiclePlan.h"
#include "../include/CrowPlan.h"
#include "../include/PlanList.h"

#include "../../TacticalAI/AIInternals.h"      // DEBUGAIMSG
#include "../../Tactical/TacticalActor.h" // For TacticalActor definition
#include "../../Tactical/TacticalActorStateFlags.h"
#include "../../Tactical/Animation Data.h"  // For the definition of, wait for it... BLOODCAT!
#include "Soldier macros.h"

#include <stdio.h>


namespace AI
{
    namespace tactical
    {
        Plan* LegacyAIPlanFactory::create_plan(TacticalActor* npc, const AIInputData& input)
        {
            DEBUGAIMSG("Planning for "<<(int)npc->identity().id());
            if((npc->status().flags() & SOLDIER_MONSTER) || npc->identity().bodyType() == BLOODCAT )
                return new LegacyCreaturePlan(npc);
            if(npc->identity().bodyType() == CROW)
            {
                PlanList* find_supper = new PlanList(npc);
                CrowSeekCorpsePlan* seek_corpse = new CrowSeekCorpsePlan(npc);
                CrowPeckPlan* peck = new CrowPeckPlan(npc, seek_corpse->get_corpse_grid());
                find_supper->add_subplan(seek_corpse);
                find_supper->add_subplan(peck);
                return find_supper;
            }

			if ( ARMED_VEHICLE( npc ) )
				return new LegacyArmedVehiclePlan( npc );

            if(TacticalActorConditions::isZombie(*npc))
                return new LegacyZombiePlan(npc);

            return new LegacyAIPlan(npc);               // no special plan for other cases yet, return default legacy AI wrapper
        }


        void LegacyAIPlanFactory::update_plan(TacticalActor* npc, const AIInputData& input)
        {
            DEBUGAIMSG("Update called for "<<(int)npc->identity().id()<<" event: "<<input);
            if(!npc->aiPlan().hasPlan())
                npc->aiPlan().adopt(create_plan(npc, input));
        }
    }
}

