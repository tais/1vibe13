/** 
 * @file
 * @author feynman (bears-pit.com)
 */

#include "../include/CrowPlan.h"
#include "../../Tactical/TacticalActor.h"     // defines TacticalActor
#include "../../Tactical/TacticalActorCrowBehavior.h"
#include "../../Tactical/Grid Direction.h"
#include "../../Tactical/Animation Control.h"   // defines CROW_FLY
#include "../../Tactical/Soldier Add.h"         // FindGridNoFromSweetSpot()
#include "../../TacticalAI/ai.h"                // AI_ACTION_...
#include "../../TacticalAI/AIInternals.h"       // AIDEBUGMSG
#include "../../Tactical/Rotting Corpses.h"     // FindNearestRottingCorpse()
#include "../../TileEngine/Isometric Utils.h"   // TileIsOutOfBounds()

namespace AI
{
    namespace tactical
    {
        CrowFlyAwayPlan::CrowFlyAwayPlan(TacticalActor* npc)
            : Plan(npc)
        {
        }

        void CrowFlyAwayPlan::execute(PlanInputData& environment)
        {
            CrowsFlyAway( get_npc()->roster().team() );
        }

        CrowSeekCorpsePlan::CrowSeekCorpsePlan(TacticalActor* npc)
            : Plan(npc),
              corpse_grid_(FindNearestRottingCorpse(npc))
        {
        }

        void CrowSeekCorpsePlan::execute(PlanInputData& environment)
        {
            get_npc()->aiPlanning().action() =  AI_ACTION_NONE;
            if(TileIsOutOfBounds(corpse_grid_))
                return;
            // Walk to nearest corpse
            UINT8 unused;
            get_npc()->aiPlanning().actionData() = FindGridNoFromSweetSpot( get_npc(), corpse_grid_, 4, &unused);
            if(TileIsOutOfBounds(get_npc()->aiPlanning().actionData()))
                return;
            get_npc()->aiPlanning().action() = AI_ACTION_GET_CLOSER;
        }

        bool CrowSeekCorpsePlan::done() const
        {
            return SpacesAway( get_npc()->position().gridNo(), corpse_grid_ ) < 2;
        }

        int CrowSeekCorpsePlan::get_corpse_grid() const
        {
            return corpse_grid_;
        }

        CrowPeckPlan::CrowPeckPlan(TacticalActor* npc, int corpse_grid)
            : Plan(npc), corpse_grid_(corpse_grid)
        {
        }

        void CrowPeckPlan::execute(PlanInputData& environment)
        {
            get_npc()->aiPlanning().action() =  AI_ACTION_NONE;
            INT16 sFacingDir;
            if (TileIsOutOfBounds(corpse_grid_))
                return;

            // Change facing
            sFacingDir = GetDirectionFromGridNo( corpse_grid_, get_npc() );
            if ( sFacingDir != get_npc()->position().direction() )
            {
                get_npc()->aiPlanning().actionData() = sFacingDir;
                get_npc()->aiPlanning().action() = AI_ACTION_CHANGE_FACING;
                return;
            }
            if (!environment.turn_based())
            {
                get_npc()->aiPlanning().actionData() = 30000;
                get_npc()->aiPlanning().action() =  AI_ACTION_WAIT;
            }
        }

    } // namespace tactical
} // namespace AI

