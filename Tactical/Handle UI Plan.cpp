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
	#include "Interface.h"

UINT8						gubNumUIPlannedMoves			= 0;
SOLDIERTYPE			*gpUIPlannedSoldier			= NULL;
SOLDIERTYPE			*gpUIStartPlannedSoldier = NULL;
BOOLEAN					gfInUIPlanMode					= FALSE;


void SelectPausedFireAnimation( SOLDIERTYPE *pSoldier );


BOOLEAN BeginUIPlan( SOLDIERTYPE *pSoldier )
{
	gubNumUIPlannedMoves = 0;
	gpUIPlannedSoldier				= pSoldier;
	gpUIStartPlannedSoldier		= pSoldier;
	gfInUIPlanMode			= TRUE;

	gfPlotNewMovement	= TRUE;

	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Entering Planning Mode" );

	return( TRUE );
}

BOOLEAN AddUIPlan( INT32 sGridNo, UINT8 ubPlanID )
{
	SOLDIERTYPE				*pPlanSoldier;
	INT16					sXPos, sYPos;
	INT16					sAPCost = 0;
	INT8						bDirection;
	INT32					iLoop;
	SOLDIERCREATE_STRUCT		MercCreateStruct;
	SoldierID				ubNewIndex;

	// Depeding on stance and direction facing, add guy!

	// If we have a planned action here, ignore!


	// If not OK Dest, ignore!
	if ( !NewOKDestination( gpUIPlannedSoldier, sGridNo, FALSE, (INT8)gsInterfaceLevel ) )
	{
		return( FALSE );
	}


	if ( ubPlanID == UIPLAN_ACTION_MOVETO )
	{
		// Calculate cost to move here
		sAPCost = PlotPath( gpUIPlannedSoldier, sGridNo, COPYROUTE, NO_PLOT, TEMPORARY, (UINT16) gpUIPlannedSoldier->usUIMovementMode, NOT_STEALTH, FORWARD,	gpUIPlannedSoldier->actionPoints().current() );
		// Adjust for running if we are not already running
		if (	gpUIPlannedSoldier->usUIMovementMode == RUNNING )
		{
			sAPCost += GetAPsStartRun( gpUIPlannedSoldier ); // changed by SANDRO
		}

		if ( EnoughPoints( gpUIPlannedSoldier, sAPCost, 0, FALSE ) )
		{
			MercCreateStruct.initialize();
			MercCreateStruct.bTeam				= SOLDIER_CREATE_AUTO_TEAM;
			MercCreateStruct.ubProfile		= NO_PROFILE;
			MercCreateStruct.fPlayerPlan	= TRUE;
			MercCreateStruct.ubBodyType		= gpUIPlannedSoldier->ubBodyType;
			MercCreateStruct.sInsertionGridNo		= sGridNo;

			// Get Grid Corrdinates of mouse
			if ( TacticalCreateSoldier( &MercCreateStruct, &ubNewIndex ) )
			{
				// Get pointer to soldier
				GetSoldier( &pPlanSoldier, ubNewIndex );

				pPlanSoldier->sPlannedTargetX = -1;
				pPlanSoldier->sPlannedTargetY = -1;

				// Compare OPPLISTS!
				// Set ones we don't know about but do now back to old ( ie no new guys )
				for (iLoop = 0; iLoop < MAX_NUM_SOLDIERS; iLoop++ )
				{
					if ( gpUIPlannedSoldier->aiData.bOppList[ iLoop ] < 0 )
					{
							pPlanSoldier->aiData.bOppList[ iLoop ] = gpUIPlannedSoldier->aiData.bOppList[ iLoop ];
					}
				}

				// Get XY from Gridno
				ConvertGridNoToCenterCellXY( sGridNo, &sXPos, &sYPos );

				pPlanSoldier->EVENT_SetSoldierPosition( sXPos, sYPos );
				pPlanSoldier->EVENT_SetSoldierDestination( (UINT8) sGridNo ); // Hopefully this code is never used anymore because the second param is now direction, not grid
				pPlanSoldier->awareness().markVisible();
				pPlanSoldier->usUIMovementMode = gpUIPlannedSoldier->usUIMovementMode;


				pPlanSoldier->actionPoints().current() = gpUIPlannedSoldier->actionPoints().current() - sAPCost;

				pPlanSoldier->ubPlannedUIAPCost = (UINT8)pPlanSoldier->actionPoints().current();

				// Get direction
				bDirection = (INT8)gpUIPlannedSoldier->pathing().path()[ gpUIPlannedSoldier->pathing().pathSize() - 1 ];

				// Set direction
				pPlanSoldier->position().direction() = bDirection;
				pPlanSoldier->pathing().desiredDirection() = bDirection;

				// Set walking animation
				pPlanSoldier->ChangeSoldierState( pPlanSoldier->usUIMovementMode, 0, FALSE );

				// Change selected soldier
				gusSelectedSoldier = pPlanSoldier->ubID;

				// Change global planned mode to this guy!
				gpUIPlannedSoldier = pPlanSoldier;

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
 	sAPCost = CalcTotalAPsToAttack( gpUIPlannedSoldier, sGridNo, TRUE, (INT16)(gpUIPlannedSoldier->aiData.bShownAimTime ) );

		// Get XY from Gridno
		ConvertGridNoToCenterCellXY( sGridNo, &sXPos, &sYPos );


		// If this is a player guy, show message about no APS
		if ( EnoughPoints( gpUIPlannedSoldier, sAPCost, 0, FALSE ) )
		{
			// CHECK IF WE ARE A PLANNED SOLDIER OR NOT< IF SO< CREATE!
			if ( gpUIPlannedSoldier->ubID < MAX_NUM_SOLDIERS )
			{
				MercCreateStruct.initialize();
				MercCreateStruct.bTeam				= SOLDIER_CREATE_AUTO_TEAM;
				MercCreateStruct.ubProfile		= NO_PROFILE;
				MercCreateStruct.fPlayerPlan	= TRUE;
				MercCreateStruct.ubBodyType		= gpUIPlannedSoldier->ubBodyType;
				MercCreateStruct.sInsertionGridNo		= sGridNo;

				// Get Grid Corrdinates of mouse
				if ( TacticalCreateSoldier( &MercCreateStruct, &ubNewIndex ) )
				{
					// Get pointer to soldier
					GetSoldier( &pPlanSoldier, ubNewIndex );

					pPlanSoldier->sPlannedTargetX = -1;
					pPlanSoldier->sPlannedTargetY = -1;

					// Compare OPPLISTS!
					// Set ones we don't know about but do now back to old ( ie no new guys )
					for (iLoop = 0; iLoop < MAX_NUM_SOLDIERS; iLoop++ )
					{
						if ( gpUIPlannedSoldier->aiData.bOppList[ iLoop ] < 0 )
						{
								pPlanSoldier->aiData.bOppList[ iLoop ] = gpUIPlannedSoldier->aiData.bOppList[ iLoop ];
						}
					}

					pPlanSoldier->EVENT_SetSoldierPosition( gpUIPlannedSoldier->position().worldX(), gpUIPlannedSoldier->position().worldY() );
					pPlanSoldier->EVENT_SetSoldierDestination( (UINT8) gpUIPlannedSoldier->position().gridNo() );
					pPlanSoldier->awareness().markVisible();
					pPlanSoldier->usUIMovementMode = gpUIPlannedSoldier->usUIMovementMode;


					pPlanSoldier->actionPoints().current() = gpUIPlannedSoldier->actionPoints().current() - sAPCost;

					pPlanSoldier->ubPlannedUIAPCost = (UINT8)pPlanSoldier->actionPoints().current();

					// Get direction
					bDirection = (INT8)gpUIPlannedSoldier->pathing().path()[ gpUIPlannedSoldier->pathing().pathSize() - 1 ];

					// Set direction
					pPlanSoldier->position().direction() = bDirection;
					pPlanSoldier->pathing().desiredDirection() = bDirection;

					// Set walking animation
					pPlanSoldier->ChangeSoldierState( pPlanSoldier->usUIMovementMode, 0, FALSE );

					// Change selected soldier
					gusSelectedSoldier = pPlanSoldier->ubID;

					// Change global planned mode to this guy!
					gpUIPlannedSoldier = pPlanSoldier;

					gubNumUIPlannedMoves++;
				}


			}

			gpUIPlannedSoldier->actionPoints().current() = gpUIPlannedSoldier->actionPoints().current() - sAPCost;

			gpUIPlannedSoldier->ubPlannedUIAPCost = (UINT8)gpUIPlannedSoldier->actionPoints().current();

			// Get direction from gridno
			bDirection = GetDirectionFromGridNo( sGridNo, gpUIPlannedSoldier );

			// Set direction
			gpUIPlannedSoldier->position().direction() = bDirection;
			gpUIPlannedSoldier->pathing().desiredDirection() = bDirection;

			// Set to shooting animation
			SelectPausedFireAnimation( gpUIPlannedSoldier );

			gpUIPlannedSoldier->sPlannedTargetX = sXPos;
			gpUIPlannedSoldier->sPlannedTargetY = sYPos;

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
	SOLDIERTYPE *pSoldier;

	// Zero out any planned soldiers
	//FIXME
	for( cnt = MAX_NUM_SOLDIERS; cnt < TOTAL_SOLDIERS; cnt++ )
	{
		pSoldier = GetJa2SoldierRepository().resolve(
			static_cast<UINT32>(cnt));

		if ( pSoldier && pSoldier->bActive )
		{
			if ( pSoldier->sPlannedTargetX != -1 )
			{
				SetRenderFlags(RENDER_FLAG_FULL );
			}
			TacticalRemoveSoldier( pSoldier->ubID );
		}


	}
	gfInUIPlanMode			= FALSE;
	gusSelectedSoldier	= gpUIStartPlannedSoldier->ubID;

	gfPlotNewMovement	= TRUE;

	ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Leaving Planning Mode" );

}

BOOLEAN InUIPlanMode( )
{
	return( gfInUIPlanMode );
}


void SelectPausedFireAnimation( SOLDIERTYPE *pSoldier )
{
	// Determine which animation to do...depending on stance and gun in hand...

	switch ( gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight )
	{
		case ANIM_STAND:

			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_ALT_WEAPON_HOLDING ) )
				{
					pSoldier->ChangeSoldierState( BURST_ALTERNATIVE_STAND, 2 , FALSE );
				}
				else
				{
					pSoldier->ChangeSoldierState( STANDING_BURST, 2 , FALSE );
				}
			}
			else
			{
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_ALT_WEAPON_HOLDING ) )
				{
					pSoldier->ChangeSoldierState( SHOOT_ALTERNATIVE_STAND, 2 , FALSE );
				}
				else
				{
					pSoldier->ChangeSoldierState( SHOOT_RIFLE_STAND, 2 , FALSE );
				}
			}
			break;

		case ANIM_PRONE:
			pSoldier->ChangeSoldierState( SHOOT_RIFLE_PRONE, 2 , FALSE );
			break;

		case ANIM_CROUCH:
			pSoldier->ChangeSoldierState( SHOOT_RIFLE_CROUCH, 2 , FALSE );
			break;

	}

}
