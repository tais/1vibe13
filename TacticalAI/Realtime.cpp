	#include "ai.h"
	#include "AIInternals.h"
	#include "Isometric Utils.h"
	#include "Overhead.h"
	#include "Soldier Add.h"
	#include "soldier profile type.h"
	#include "Items.h"
	#include "Weapons.h"
	#include "Soldier macros.h"
	#include "Soldier Profile.h"
	#include "NPC.h"
	#include "Render Fun.h"
	#include "Quests.h"
	#include "GameSettings.h"
// needed to use the modularized tactical AI:
#include "Plan.h"
#include "PlanFactoryLibrary.h"
#include "AbstractPlanFactory.h"


UINT16 RealtimeDelay( TacticalActor * pSoldier )
{
	if ( PTR_CIV_OR_MILITIA && !(pSoldier->roster().civilianGroup() == KINGPIN_CIV_GROUP ) )
	{
		return( (UINT16) REALTIME_CIV_AI_DELAY );
	}
	else if ( CREATURE_OR_BLOODCAT( pSoldier ) && !( pSoldier->aiBehavior().hunting() ) )
	{
		return( (UINT16) REALTIME_CREATURE_AI_DELAY );
	}
	else
	{

		if ( pSoldier->roster().civilianGroup() == KINGPIN_CIV_GROUP )
		{
			//DBrot: More Rooms
			//UINT8		ubRoom;
			UINT16 usRoom;

			if ( InARoom( pSoldier->position().gridNo(), &usRoom ) && IN_BROTHEL( usRoom ) )
			{
				return( (UINT16) (REALTIME_AI_DELAY / 3) );
			}
		}

		return( (UINT16) REALTIME_AI_DELAY );
	}

}


void RTHandleAI( TacticalActor * pSoldier )
{
#ifdef AI_PROFILING
	INT32 iLoop;
#endif

	if ((pSoldier->aiPlanning().action() != AI_ACTION_NONE) && pSoldier->aiPlanning().actionInProgress())
	{
		// if action should remain in progress
		if (ActionInProgress(pSoldier))
		{
			#ifdef DEBUGBUSY
				AINumMessage("Busy with action, skipping guy#",pSoldier->identity().id());
			#endif
			// let it continue
			return;
		}
	}

	// Flugente: prisoners of war don't do anything
	if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_POW || pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_CRYO) )
		return;

	// Flugente: if we are distracted by chatting and not alert, do nothing
	if ( pSoldier->interaction().chatting() )
	{
		if ( pSoldier->aiBehavior().alertStatus() < STATUS_RED )
		{
			EndAIGuysTurn( pSoldier );
			return;
		}
		else
		{
			pSoldier->StopChatting();
		}
	}

	// if man has nothing to do
	if (pSoldier->aiPlanning().action() == AI_ACTION_NONE)
	{
		if (pSoldier->aiPlanning().nextAction() == AI_ACTION_NONE)
		{
			// make sure this flag is turned off (it already should be!)
			pSoldier->aiPlanning().actionInProgress() = FALSE;

			// truly nothing to do!
			RefreshAI( pSoldier );
		}

		// Since we're NEVER going to "continue" along an old path at this point,
		// then it would be nice place to reinitialize "pathStored" flag for
		// insurance purposes.
		//
		// The "pathStored" variable controls whether it's necessary to call
		// findNewPath() after you've called NewDest(). Since the AI calls
		// findNewPath() itself, a speed gain can be obtained by avoiding
		// redundancy.
		//
		// The "normal" way for pathStored to be reset is inside
		// SetNewCourse() [which gets called after NewDest()].
		//
		// The only reason we would NEED to reinitialize it here is if I've
		// incorrectly set pathStored to TRUE in a process that doesn't end up
		// calling NewDest()
		pSoldier->pathing().stored() = FALSE;

		// decide on the next action
#ifdef AI_PROFILING
		for (iLoop = 0; iLoop < 1000; iLoop++)
#endif
		{
			if (pSoldier->aiPlanning().nextAction() != AI_ACTION_NONE)
			{
				if ( pSoldier->aiPlanning().nextAction() == AI_ACTION_END_COWER_AND_MOVE )
				{
					if ( pSoldier->status().flags() & SOLDIER_COWERING )
					{
						pSoldier->aiPlanning().action() = AI_ACTION_STOP_COWERING;
						pSoldier->aiPlanning().actionData() = ANIM_STAND;
					}
					else if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight < ANIM_STAND )
					{
						// stand up!
						pSoldier->aiPlanning().action() = AI_ACTION_CHANGE_STANCE;
						pSoldier->aiPlanning().actionData() = ANIM_STAND;
					}
					else
					{
						pSoldier->aiPlanning().action() = AI_ACTION_NONE;
					}
					if ( pSoldier->position().gridNo() == pSoldier->aiPlanning().nextActionData() )
					{
						// no need to walk after this
						pSoldier->aiPlanning().nextAction() = AI_ACTION_NONE;
						pSoldier->aiPlanning().nextActionData() = NOWHERE;
					}
					else
					{
						pSoldier->aiPlanning().nextAction() = AI_ACTION_WALK;
						// leave next-action-data as is since that's where we want to go
					}
				}
				else
				{
					// do the next thing we have to do...
					pSoldier->aiPlanning().action() = pSoldier->aiPlanning().nextAction();
					pSoldier->aiPlanning().actionData() = pSoldier->aiPlanning().nextActionData();
					pSoldier->targeting().level() = pSoldier->aiPlanning().nextTargetLevel();
					pSoldier->aiPlanning().nextAction() = AI_ACTION_NONE;
					pSoldier->aiPlanning().nextActionData() = 0;
					pSoldier->aiPlanning().nextTargetLevel() = 0;
				}
				if (pSoldier->aiPlanning().action() == AI_ACTION_PICKUP_ITEM)
				{
					// the item pool index was stored in the special data field
					pSoldier->pendingAction().primaryData() = pSoldier->pendingAction().nextSpecialData();
				}
			}			
			else if (!TileIsOutOfBounds(pSoldier->movement().absoluteDestination()))
			{
				if ( ACTING_ON_SCHEDULE( pSoldier ) )
				{
					pSoldier->aiPlanning().action() = AI_ACTION_SCHEDULE_MOVE;
				}
				else
				{
					pSoldier->aiPlanning().action() = AI_ACTION_WALK;
				}
				pSoldier->aiPlanning().actionData() = pSoldier->movement().absoluteDestination();
			}
			else
			{
				// sevenfm: clear next action data before making decision
				pSoldier->aiPlanning().nextAction() = AI_ACTION_NONE;
				pSoldier->aiPlanning().nextActionData() = 0;
				pSoldier->aiPlanning().nextTargetLevel() = 0;
				pSoldier->pendingAction().nextSpecialData() = 0;

				if (!(gTacticalStatus.uiFlags & ENGAGED_IN_CONV))
				{
                    if(!pSoldier->aiPlan().hasPlan()) // if the Soldier has no plan, create one
                    {
                        AI::tactical::AIInputData ai_input;
                        AI::tactical::PlanFactoryLibrary* plan_lib(AI::tactical::PlanFactoryLibrary::instance());
                        const INT16 planIndex =
                            pSoldier->aiPlanning().ensurePlanIndex(pSoldier->roster().team() + 1);
                        pSoldier->aiPlan().adopt(
                            plan_lib->create_plan(planIndex, pSoldier, ai_input));
                    }
                    AI::tactical::PlanInputData plan_input(false, gTacticalStatus);
                    pSoldier->aiPlan().execute(plan_input);
				}
			}
		}
		// if he chose to continue doing nothing
		if (pSoldier->aiPlanning().action() == AI_ACTION_NONE)
		{
			#ifdef RECORDNET
				fprintf(NetDebugFile,"\tMOVED BECOMING TRUE: Chose to do nothing, guynum %d\n",pSoldier->identity().id());
			#endif

			// do a standard wait before doing anything else!
			pSoldier->aiPlanning().action() = AI_ACTION_WAIT;
			//if (PTR_CIVILIAN && pSoldier->aiBehavior().alertStatus() != STATUS_BLACK)
			if ( PTR_CIV_OR_MILITIA && !(pSoldier->roster().civilianGroup() == KINGPIN_CIV_GROUP ) )
			{
				pSoldier->aiPlanning().actionData() = (UINT16) REALTIME_CIV_AI_DELAY;
			}
			else if ( CREATURE_OR_BLOODCAT( pSoldier ) && !( pSoldier->aiBehavior().hunting() ) )
			{
				pSoldier->aiPlanning().actionData() = (UINT16) REALTIME_CREATURE_AI_DELAY;
			}
			else
			{
				pSoldier->aiPlanning().actionData() = (UINT16) REALTIME_AI_DELAY;
				if ( pSoldier->roster().civilianGroup() == KINGPIN_CIV_GROUP )
				{
					//DBrot: More Rooms
					//UINT8		ubRoom;
					UINT16 usRoom;

					if ( InARoom( pSoldier->position().gridNo(), &usRoom ) && IN_BROTHEL( usRoom ) )
					{
						pSoldier->aiPlanning().actionData() /= 3;
					}

				}
			}
		}
		else if (pSoldier->aiPlanning().action() == AI_ACTION_ABSOLUTELY_NONE)
		{
			pSoldier->aiPlanning().action() = AI_ACTION_NONE;
		}

	}

	// to get here, we MUST have an action selected, but not in progress...
	NPCDoesAct(pSoldier);

	// perform the chosen action
	pSoldier->aiPlanning().actionInProgress() = ExecuteAction(pSoldier); // if started, mark us as busy
}
