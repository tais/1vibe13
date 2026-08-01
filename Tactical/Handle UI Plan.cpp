#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorOrientation.h"
#include "TacticalActorWorldPlacement.h"
	#include "Handle UI Plan.h"
	#include "Overhead.h"
	#include "Isometric Utils.h"
	#include "PATHAI.H"
	#include "Handle UI.h"
	#include "Points.h"
	#include "renderworld.h"
	#include "Animation Control.h"
	#include "message.h"
	#include "Soldier Create.h"
	#include "SoldierRepository.h"
	#include "TacticalEntityHost.h"
	#include "Interface.h"

UINT8						gubNumUIPlannedMoves			= 0;
BOOLEAN					gfInUIPlanMode					= FALSE;

namespace
{
Ja2TacticalEntityReference gUiPlannedSoldier;
Ja2TacticalEntityReference gUiStartPlannedSoldier;

void ResetUiPlanActors() noexcept
{
	gUiPlannedSoldier.reset();
	gUiStartPlannedSoldier.reset();
}
}

void SelectPausedFireAnimation( TacticalActor *pSoldier );


BOOLEAN BeginUIPlan( TacticalEntityId actor )
{
	Ja2TacticalEntityReference plannedSoldier;
	Ja2TacticalEntityReference startSoldier;
	if (!plannedSoldier.capture(actor) ||
		!startSoldier.capture(actor))
	{
		ResetUiPlanActors();
		gfInUIPlanMode = FALSE;
		return FALSE;
	}

	gubNumUIPlannedMoves = 0;
	gUiPlannedSoldier = plannedSoldier;
	gUiStartPlannedSoldier = startSoldier;
	gfInUIPlanMode			= TRUE;

	gfPlotNewMovement	= TRUE;

	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Entering Planning Mode" );

	return( TRUE );
}

BOOLEAN AddUIPlan( INT32 sGridNo, UINT8 ubPlanID )
{
	TacticalActor				*pPlanSoldier = NULL;
	TacticalActor				*pUiPlannedSoldier =
		gUiPlannedSoldier.resolve();
	INT16					sXPos, sYPos;
	INT16					sAPCost = 0;
	INT8						bDirection;
	INT32					iLoop;
	SOLDIERCREATE_STRUCT		MercCreateStruct;
	SoldierID				ubNewIndex;

	if (!gfInUIPlanMode || !pUiPlannedSoldier)
		return FALSE;

	// Depeding on stance and direction facing, add guy!

	// If we have a planned action here, ignore!


	// If not OK Dest, ignore!
	if ( !NewOKDestination( pUiPlannedSoldier, sGridNo, FALSE, (INT8)gsInterfaceLevel ) )
	{
		return( FALSE );
	}


	if ( ubPlanID == UIPLAN_ACTION_MOVETO )
	{
		// Calculate cost to move here
		sAPCost = PlotPath( pUiPlannedSoldier, sGridNo, COPYROUTE, NO_PLOT, TEMPORARY, (UINT16) pUiPlannedSoldier->movement().mode(), NOT_STEALTH, FORWARD,	pUiPlannedSoldier->actionPoints().current() );
		// Adjust for running if we are not already running
		if (	pUiPlannedSoldier->movement().mode() == RUNNING )
		{
			sAPCost += GetAPsStartRun( pUiPlannedSoldier ); // changed by SANDRO
		}

		if ( EnoughPoints( pUiPlannedSoldier, sAPCost, 0, FALSE ) )
		{
			MercCreateStruct.initialize();
			MercCreateStruct.bTeam				= SOLDIER_CREATE_AUTO_TEAM;
			MercCreateStruct.ubProfile		= NO_PROFILE;
			MercCreateStruct.fPlayerPlan	= TRUE;
			MercCreateStruct.ubBodyType		= pUiPlannedSoldier->identity().bodyType();
			MercCreateStruct.sInsertionGridNo		= sGridNo;

			// Get Grid Corrdinates of mouse
			if ( TacticalCreateSoldier( &MercCreateStruct, &ubNewIndex ) )
			{
				// Get pointer to soldier
				if (!GetSoldier( &pPlanSoldier, ubNewIndex ) ||
					!pPlanSoldier)
				{
					TacticalRemoveSoldier(ubNewIndex);
					return FALSE;
				}

				pPlanSoldier->uiPresentation().clearPlannedTarget();

				// Compare OPPLISTS!
				// Set ones we don't know about but do now back to old ( ie no new guys )
				for (iLoop = 0; iLoop < MAX_NUM_SOLDIERS; iLoop++ )
				{
					if ( pUiPlannedSoldier->awareness().opponentKnowledge()[ iLoop ] < 0 )
					{
							pPlanSoldier->awareness().opponentKnowledge()[ iLoop ] = pUiPlannedSoldier->awareness().opponentKnowledge()[ iLoop ];
					}
				}

				// Get XY from Gridno
				ConvertGridNoToCenterCellXY( sGridNo, &sXPos, &sYPos );

				(void)TacticalActorWorldPlacement::setPosition(*pPlanSoldier, sXPos, sYPos );
				(void)TacticalActorOrientation::setMovementDestination(*pPlanSoldier, (UINT8) sGridNo ); // Hopefully this code is never used anymore because the second param is now direction, not grid
				pPlanSoldier->awareness().markVisible();
				pPlanSoldier->movement().mode() = pUiPlannedSoldier->movement().mode();


				pPlanSoldier->actionPoints().current() = pUiPlannedSoldier->actionPoints().current() - sAPCost;

				pPlanSoldier->uiPresentation().plannedActionPointCost() = (UINT8)pPlanSoldier->actionPoints().current();

				// Get direction
				bDirection = (INT8)pUiPlannedSoldier->pathing().path()[ pUiPlannedSoldier->pathing().pathSize() - 1 ];

				// Set direction
				pPlanSoldier->position().direction() = bDirection;
				pPlanSoldier->pathing().desiredDirection() = bDirection;

				// Set walking animation
				TacticalActorAnimationTransitions::changeState(*pPlanSoldier,  pPlanSoldier->movement().mode(), 0, FALSE );

				if (!gUiPlannedSoldier.capture(
						GetJa2TacticalEntityId(*pPlanSoldier)))
				{
					TacticalRemoveSoldier(
						pPlanSoldier->identity().id());
					return FALSE;
				}
				pUiPlannedSoldier = pPlanSoldier;

				// Change selected soldier
				gusSelectedSoldier = pPlanSoldier->identity().id();

				gubNumUIPlannedMoves++;

				gfPlotNewMovement	= TRUE;

				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Adding Merc Move to Plan" );

			}
		}
		else
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Merc will not have enough action points" );
		}
	}
	else if ( ubPlanID == UIPLAN_ACTION_FIRE )
	{
	sAPCost = CalcTotalAPsToAttack( pUiPlannedSoldier, sGridNo, TRUE, (INT16)(pUiPlannedSoldier->aiPlanning().shownAimTime() ) );

		// Get XY from Gridno
		ConvertGridNoToCenterCellXY( sGridNo, &sXPos, &sYPos );


		// If this is a player guy, show message about no APS
		if ( EnoughPoints( pUiPlannedSoldier, sAPCost, 0, FALSE ) )
		{
			// CHECK IF WE ARE A PLANNED SOLDIER OR NOT< IF SO< CREATE!
			if ( pUiPlannedSoldier->identity().id() < MAX_NUM_SOLDIERS )
			{
				MercCreateStruct.initialize();
				MercCreateStruct.bTeam				= SOLDIER_CREATE_AUTO_TEAM;
				MercCreateStruct.ubProfile		= NO_PROFILE;
				MercCreateStruct.fPlayerPlan	= TRUE;
				MercCreateStruct.ubBodyType		= pUiPlannedSoldier->identity().bodyType();
				MercCreateStruct.sInsertionGridNo		= sGridNo;

				// Get Grid Corrdinates of mouse
				if ( TacticalCreateSoldier( &MercCreateStruct, &ubNewIndex ) )
				{
					// Get pointer to soldier
					if (!GetSoldier( &pPlanSoldier, ubNewIndex ) ||
						!pPlanSoldier)
					{
						TacticalRemoveSoldier(ubNewIndex);
						return FALSE;
					}

					pPlanSoldier->uiPresentation().clearPlannedTarget();

					// Compare OPPLISTS!
					// Set ones we don't know about but do now back to old ( ie no new guys )
					for (iLoop = 0; iLoop < MAX_NUM_SOLDIERS; iLoop++ )
					{
						if ( pUiPlannedSoldier->awareness().opponentKnowledge()[ iLoop ] < 0 )
						{
								pPlanSoldier->awareness().opponentKnowledge()[ iLoop ] = pUiPlannedSoldier->awareness().opponentKnowledge()[ iLoop ];
						}
					}

					(void)TacticalActorWorldPlacement::setPosition(*pPlanSoldier, pUiPlannedSoldier->position().worldX(), pUiPlannedSoldier->position().worldY() );
					(void)TacticalActorOrientation::setMovementDestination(*pPlanSoldier, (UINT8) pUiPlannedSoldier->position().gridNo() );
					pPlanSoldier->awareness().markVisible();
					pPlanSoldier->movement().mode() = pUiPlannedSoldier->movement().mode();


					pPlanSoldier->actionPoints().current() = pUiPlannedSoldier->actionPoints().current() - sAPCost;

					pPlanSoldier->uiPresentation().plannedActionPointCost() = (UINT8)pPlanSoldier->actionPoints().current();

					// Get direction
					bDirection = (INT8)pUiPlannedSoldier->pathing().path()[ pUiPlannedSoldier->pathing().pathSize() - 1 ];

					// Set direction
					pPlanSoldier->position().direction() = bDirection;
					pPlanSoldier->pathing().desiredDirection() = bDirection;

					// Set walking animation
					TacticalActorAnimationTransitions::changeState(*pPlanSoldier,  pPlanSoldier->movement().mode(), 0, FALSE );

					if (!gUiPlannedSoldier.capture(
							GetJa2TacticalEntityId(*pPlanSoldier)))
					{
						TacticalRemoveSoldier(
							pPlanSoldier->identity().id());
						return FALSE;
					}
					pUiPlannedSoldier = pPlanSoldier;

					// Change selected soldier
					gusSelectedSoldier = pPlanSoldier->identity().id();

					gubNumUIPlannedMoves++;
				}


			}

			pUiPlannedSoldier->actionPoints().current() = pUiPlannedSoldier->actionPoints().current() - sAPCost;

			pUiPlannedSoldier->uiPresentation().plannedActionPointCost() = (UINT8)pUiPlannedSoldier->actionPoints().current();

			// Get direction from gridno
			bDirection = GetDirectionFromGridNo( sGridNo, pUiPlannedSoldier );

			// Set direction
			pUiPlannedSoldier->position().direction() = bDirection;
			pUiPlannedSoldier->pathing().desiredDirection() = bDirection;

			// Set to shooting animation
			SelectPausedFireAnimation( pUiPlannedSoldier );

			pUiPlannedSoldier->uiPresentation().setPlannedTarget(
				sXPos, sYPos,
				pUiPlannedSoldier->uiPresentation().plannedActionPointCost());

			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Adding Merc Shoot to Plan" );

		}
		else
		{
			ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Merc will not have enough action points" );
		}
	}
	return( TRUE );
}


void EndUIPlan(	)
{
	int				cnt;
	TacticalActor *pSoldier;
	TacticalActor *pStartSoldier =
		gUiStartPlannedSoldier.resolve();

	// Zero out any planned soldiers
	//FIXME
	for( cnt = MAX_NUM_SOLDIERS; cnt < TOTAL_SOLDIERS; cnt++ )
	{
		pSoldier = GetJa2SoldierRepository().resolve(
			static_cast<UINT32>(cnt));

		if ( pSoldier && pSoldier->roster().active() )
		{
			if ( pSoldier->uiPresentation().hasPlannedTarget() )
			{
				SetRenderFlags(RENDER_FLAG_FULL );
			}
			TacticalRemoveSoldier( pSoldier->identity().id() );
		}


	}
	gfInUIPlanMode			= FALSE;
	gubNumUIPlannedMoves = 0;
	ResetUiPlanActors();
	gusSelectedSoldier = pStartSoldier
		? pStartSoldier->identity().id()
		: NOBODY;

	gfPlotNewMovement	= TRUE;

	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Leaving Planning Mode" );

}

BOOLEAN InUIPlanMode( )
{
	return( gfInUIPlanMode );
}


void SelectPausedFireAnimation( TacticalActor *pSoldier )
{
	// Determine which animation to do...depending on stance and gun in hand...

	switch ( gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight )
	{
		case ANIM_STAND:

			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_ALT_WEAPON_HOLDING ) )
				{
					TacticalActorAnimationTransitions::changeState(*pSoldier,  BURST_ALTERNATIVE_STAND, 2 , FALSE );
				}
				else
				{
					TacticalActorAnimationTransitions::changeState(*pSoldier,  STANDING_BURST, 2 , FALSE );
				}
			}
			else
			{
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_ALT_WEAPON_HOLDING ) )
				{
					TacticalActorAnimationTransitions::changeState(*pSoldier,  SHOOT_ALTERNATIVE_STAND, 2 , FALSE );
				}
				else
				{
					TacticalActorAnimationTransitions::changeState(*pSoldier,  SHOOT_RIFLE_STAND, 2 , FALSE );
				}
			}
			break;

		case ANIM_PRONE:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  SHOOT_RIFLE_PRONE, 2 , FALSE );
			break;

		case ANIM_CROUCH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  SHOOT_RIFLE_CROUCH, 2 , FALSE );
			break;

	}

}
