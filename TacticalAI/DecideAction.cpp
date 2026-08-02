#include "TacticalActorWorldPlacement.h"
#include "TacticalActorAnimationSelection.h"
	#include "TacticalActor.h"
	#include "TacticalActorPredicates.h"
	#include "TacticalActorBloodState.h"
	#include "TacticalActorStateFlags.h"
	#include "Grid Direction.h"
	#include "Soldier Profile Constants.h"
#include "TacticalActorAiBehavior.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalActorVisibility.h"
#include "TacticalActorEquipment.h"
#include "ai.h"
#include "TacticalActorConditions.h"
#include "TacticalWorldAdapter.h"
#include "AIInternals.h"
#include "Isometric Utils.h"
#include "Points.h"
#include "Overhead.h"
#include "opplist.h"
#include "Items.h"
#include "Weapons.h"
#include "NPC.h"
#include "Soldier Functions.h"
#include "worldman.h"
#include "Scheduling.h"
#include "message.h"
#include "Structure Wrap.h"
#include "Keys.h"
#include "PATHAI.H"
#include "Render Fun.h"
#include "Boxing.h"
//	#include "Air Raid.h"
#include "Soldier Profile.h"
#include "soldier profile type.h"
#include "Soldier macros.h"
#include "LOS.h"
#include "Buildings.h"
#include "strategicmap.h"
#include "Quests.h"
#include "Map Screen Interface Map.h"
#include "Soldier Ani.h"
#include "Rotting Corpses.h"
#include "GameSettings.h"
#include "Dialogue Control.h"
#include "connect.h"
#include "Text.h"
#include "Exit Grids.h"		// added by Flugente
#include "Game Clock.h"		// sevenfm
#include "SkillCheck.h"		// sevenfm
#include "SoldierRepository.h"
#include "TacticalActorRadio.h"
#include "TacticalActorSkills.h"

//////////////////////////////////////////////////////////////////////////////
// SANDRO - In this file, all APBPConstants[AP_CROUCH] and APBPConstants[AP_PRONE] were changed to GetAPsCrouch() and GetAPsProne()
//			On the bottom here, there are these functions made
//////////////////////////////////////////////////////////////////////

extern BOOLEAN gfHiddenInterrupt;
extern BOOLEAN gfUseAlternateQueenPosition;
extern void IncrementWatchedLoc(UINT16 ubID, INT32 sGridNo, INT8 bLevel);
void LogDecideInfo(TacticalActor *pSoldier);
void LogKnowledgeInfo(TacticalActor *pSoldier);

// global status time counters to determine what takes the most time

#ifdef AI_TIMING_TESTS
UINT32 guiGreenTimeTotal = 0, guiYellowTimeTotal = 0, guiRedTimeTotal = 0, guiBlackTimeTotal = 0;
UINT32 guiGreenCounter = 0, guiYellowCounter = 0, guiRedCounter = 0, guiBlackCounter = 0;
UINT32 guiRedSeekTimeTotal = 0, guiRedHelpTimeTotal = 0, guiRedHideTimeTotal = 0;
UINT32 guiRedSeekCounter = 0, guiRedHelpCounter = 0; guiRedHideCounter = 0;
#endif

#define CENTER_OF_RING 11237//dnl!!!

INT8 ArmedVehicleDecideActionGreen(TacticalActor *pSoldier);
INT8 ArmedVehicleDecideActionYellow(TacticalActor *pSoldier);
INT8 ArmedVehicleDecideActionRed(TacticalActor *pSoldier);
INT8 ArmedVehicleDecideActionBlack(TacticalActor *pSoldier);

STR8 gStr8AlertStatus[] = { "Green", "Yellow", "Red", "Black" };
STR8 gStr8Attitude[] = { "DEFENSIVE", "BRAVESOLO", "BRAVEAID", "CUNNINGSOLO", "CUNNINGAID", "AGGRESSIVE", "MAXATTITUDES", "ATTACKSLAYONLY" };
STR8 gStr8Orders[] = { "STATIONARY", "ONGUARD", "CLOSEPATROL", "FARPATROL", "POINTPATROL", "ONCALL", "SEEKENEMY", "RNDPTPATROL", "SNIPER" };
STR8 gStr8Team[] = { "OUR_TEAM", "ENEMY_TEAM", "CREATURE_TEAM", "MILITIA_TEAM", "CIV_TEAM", "LAST_TEAM", "PLAYER_PLAN", "LAN_TEAM_ONE", "LAN_TEAM_TWO", "LAN_TEAM_THREE", "LAN_TEAM_FOUR" };
STR8 gStr8Class[] = { "SOLDIER_CLASS_NONE", "SOLDIER_CLASS_ADMINISTRATOR", "SOLDIER_CLASS_ELITE", "SOLDIER_CLASS_ARMY", "SOLDIER_CLASS_GREEN_MILITIA", "SOLDIER_CLASS_REG_MILITIA", "SOLDIER_CLASS_ELITE_MILITIA", "SOLDIER_CLASS_CREATURE", "SOLDIER_CLASS_MINER", "SOLDIER_CLASS_ZOMBIE", "SOLDIER_CLASS_TANK", "SOLDIER_CLASS_JEEP", "SOLDIER_CLASS_BANDIT", "SOLDIER_CLASS_ROBOT" };
STR8 gStr8Knowledge[] = { "HEARD_3_TURNS_AGO", "HEARD_2_TURNS_AGO", "HEARD_LAST_TURN", "HEARD_THIS_TURN", "NOT_HEARD_OR_SEEN", "SEEN_CURRENTLY", "SEEN_THIS_TURN", "SEEN_LAST_TURN", "SEEN_2_TURNS_AGO", "SEEN_3_TURNS_AGO" };

void DoneScheduleAction( TacticalActor * pSoldier )
{
	pSoldier->aiBehavior().flags() &= (~AI_CHECK_SCHEDULE);
	pSoldier->schedule().resetProgress();
	PostNextSchedule( pSoldier );
}

INT8 DecideActionSchedule( TacticalActor * pSoldier )
{
	SCHEDULENODE *		pSchedule;
	INT32							iScheduleIndex;
	UINT8							ubScheduleAction;
	INT32 usGridNo1, usGridNo2;
	INT16							sX, sY;
	INT8							bDirection;
	STRUCTURE *				pStructure;
	BOOLEAN						fDoUseDoor;
	DOOR_STATUS	*			pDoorStatus;

	pSchedule = GetSchedule( pSoldier->schedule().id() );
	if (!pSchedule)
	{
		return( AI_ACTION_NONE );
	}

	if (pSchedule->usFlags & SCHEDULE_FLAGS_ACTIVE1)
	{
		iScheduleIndex = 0;
	}
	else if (pSchedule->usFlags & SCHEDULE_FLAGS_ACTIVE2)
	{
		iScheduleIndex = 1;
	}
	else if (pSchedule->usFlags & SCHEDULE_FLAGS_ACTIVE3)
	{
		iScheduleIndex = 2;
	}
	else if (pSchedule->usFlags & SCHEDULE_FLAGS_ACTIVE4)
	{
		iScheduleIndex = 3;
	}
	else
	{
		// error!
		return( AI_ACTION_NONE );
	}

	ubScheduleAction = pSchedule->ubAction[ iScheduleIndex ];
	usGridNo1 = pSchedule->usData1[ iScheduleIndex ];
	usGridNo2 = pSchedule->usData2[ iScheduleIndex ];

	// assume soldier is awake unless the action is a sleep
	pSoldier->aiBehavior().flags() &= ~(AI_ASLEEP);

	switch( ubScheduleAction )
	{
	case SCHEDULE_ACTION_LOCKDOOR:
		//Uses first gridno for locking door, then second to move to after door is locked.
		//It is possible that the second gridno will border the edge of the map, meaning that
		//the individual will walk off of the map.
		//If this is a "merchant", make sure that nobody occupies the building/room.

		switch( pSoldier->schedule().progress() )
		{
		case 0: // move to gridno specified
			if (pSoldier->position().gridNo() == usGridNo1)
			{
				pSoldier->schedule().advanceProgress();
				// fall through
			}
			else
			{
				pSoldier->aiPlanning().actionData() = usGridNo1;
				pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
				return( AI_ACTION_SCHEDULE_MOVE );
			}
			// fall through
		case 1:
			// start the door open: find the door...
			usGridNo1 = FindDoorAtGridNoOrAdjacent( usGridNo1 );
			
			if (TileIsOutOfBounds(usGridNo1))
			{
				// do nothing right now!
				return( AI_ACTION_NONE );
			}

			pDoorStatus = GetDoorStatus( usGridNo1 );
			if (pDoorStatus && pDoorStatus->ubFlags & DOOR_BUSY)
			{
				// do nothing right now!
				return( AI_ACTION_NONE );
			}

			pStructure = FindStructure( usGridNo1, STRUCTURE_ANYDOOR );
			if (pStructure == NULL)
			{
				fDoUseDoor = FALSE;
			}
			else
			{
				// action-specific tests to not handle the door
				fDoUseDoor = TRUE;

				if (pStructure->fFlags & STRUCTURE_OPEN)
				{
					// not only do we have to lock the door but
					// close it too!
					pSoldier->aiBehavior().flags() |= AI_LOCK_DOOR_INCLUDES_CLOSE;
				}
				else
				{
					DOOR * pDoor;

					pDoor = FindDoorInfoAtGridNo( usGridNo1 );
					if (pDoor)
					{
						if (pDoor->fLocked)
						{
							// door already locked!
							fDoUseDoor = FALSE;
						}
						else
						{
							pDoor->fLocked = TRUE;
						}
					}
					else
					{
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_BETAVERSION, L"Schedule involved locked door at %d but there's no lock there!", usGridNo1 );
						fDoUseDoor = FALSE;
					}
				}
			}

			if (fDoUseDoor)
			{
				pSoldier->aiPlanning().actionData() = usGridNo1;
				return( AI_ACTION_LOCK_DOOR );
			}

			// the door is already in the desired state, or it doesn't exist!
			pSoldier->schedule().advanceProgress();
			// fall through

		case 2:			
			if (pSoldier->position().gridNo() == usGridNo2 || TileIsOutOfBounds(pSoldier->position().gridNo()))
			{
				// NOWHERE indicates we were supposed to go off map and have done so
				DoneScheduleAction( pSoldier );
				
				if (!TileIsOutOfBounds(pSoldier->position().gridNo()))
				{
					pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
				}
			}
			else
			{
				if ( GridNoOnEdgeOfMap( usGridNo2, &bDirection ) )
				{
					// told to go to edge of map, so go off at that point!
					pSoldier->dialogue().quoteActionId() = GetTraversalQuoteActionID( bDirection );
				}
				pSoldier->aiPlanning().actionData() = usGridNo2;
				pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
				return( AI_ACTION_SCHEDULE_MOVE );
			}
			break;
		}
		break;


	case SCHEDULE_ACTION_UNLOCKDOOR:
	case SCHEDULE_ACTION_OPENDOOR:
	case SCHEDULE_ACTION_CLOSEDOOR:
		//Uses first gridno for opening/closing/unlocking door, then second to move to after door is opened.
		//It is possible that the second gridno will border the edge of the map, meaning that
		//the individual will walk off of the map.
		switch( pSoldier->schedule().progress() )
		{
		case 0: // move to gridno specified
			if (pSoldier->position().gridNo() == usGridNo1)
			{
				pSoldier->schedule().advanceProgress();
				// fall through
			}
			else
			{
				pSoldier->aiPlanning().actionData() = usGridNo1;
				pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
				return( AI_ACTION_SCHEDULE_MOVE );
			}
			// fall through
		case 1:
			// start the door open: find the door...
			usGridNo1 = FindDoorAtGridNoOrAdjacent( usGridNo1 );
			
			if (TileIsOutOfBounds(usGridNo1))
			{
				// do nothing right now!
				return( AI_ACTION_NONE );
			}

			pDoorStatus = GetDoorStatus( usGridNo1 );
			if (pDoorStatus && pDoorStatus->ubFlags & DOOR_BUSY)
			{
				// do nothing right now!
				return( AI_ACTION_NONE );
			}

			pStructure = FindStructure( usGridNo1, STRUCTURE_ANYDOOR );
			if (pStructure == NULL)
			{
				fDoUseDoor = FALSE;
			}
			else
			{
				fDoUseDoor = TRUE;

				// action-specific tests to not handle the door
				switch( ubScheduleAction )
				{
				case SCHEDULE_ACTION_UNLOCKDOOR:
					if (pStructure->fFlags & STRUCTURE_OPEN)
					{
						// door is already open!
						fDoUseDoor = FALSE;
					}
					else
					{
						// set the door to unlocked
						DOOR * pDoor;

						pDoor = FindDoorInfoAtGridNo( usGridNo1 );
						if (pDoor)
						{
							if (pDoor->fLocked)
							{
								pDoor->fLocked = FALSE;
							}
							else
							{
								// door already unlocked!
								fDoUseDoor = FALSE;
							}
						}
						else
						{
							// WTF?  Warning time!
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_BETAVERSION, L"Schedule involved locked door at %d but there's no lock there!", usGridNo1 );
							fDoUseDoor = FALSE;
						}
					}
					break;
				case SCHEDULE_ACTION_OPENDOOR:
					if (pStructure->fFlags & STRUCTURE_OPEN)
					{
						// door is already open!
						fDoUseDoor = FALSE;
					}
					break;
				case SCHEDULE_ACTION_CLOSEDOOR:
					if ( !(pStructure->fFlags & STRUCTURE_OPEN) )
					{
						// door is already closed!
						fDoUseDoor = FALSE;
					}
					break;
				default:
					break;
				}
			}

			if (fDoUseDoor)
			{
				pSoldier->aiPlanning().actionData() = usGridNo1;
				if (ubScheduleAction == SCHEDULE_ACTION_UNLOCKDOOR)
				{
					return( AI_ACTION_UNLOCK_DOOR );
				}
				else
				{
					return( AI_ACTION_OPEN_OR_CLOSE_DOOR );
				}
			}

			// the door is already in the desired state, or it doesn't exist!
			pSoldier->schedule().advanceProgress();
			// fall through

		case 2:			
			if (pSoldier->position().gridNo() == usGridNo2 || TileIsOutOfBounds(pSoldier->position().gridNo()))
			{
				// NOWHERE indicates we were supposed to go off map and have done so
				DoneScheduleAction( pSoldier );				
				if (!TileIsOutOfBounds(pSoldier->position().gridNo()))
				{
					pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
				}
			}
			else
			{
				if ( GridNoOnEdgeOfMap( usGridNo2, &bDirection ) )
				{
					// told to go to edge of map, so go off at that point!
					pSoldier->dialogue().quoteActionId() = GetTraversalQuoteActionID( bDirection );
				}
				pSoldier->aiPlanning().actionData() = usGridNo2;
				pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
				return( AI_ACTION_SCHEDULE_MOVE );
			}
			break;
		}
		break;

	case SCHEDULE_ACTION_GRIDNO:
		// Only uses the first gridno
		if ( pSoldier->position().gridNo() == usGridNo1 )
		{
			// done!
			DoneScheduleAction( pSoldier );			
			if (!TileIsOutOfBounds(pSoldier->position().gridNo()))
			{
				pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
			}
		}
		else
		{
			// move!
			pSoldier->aiPlanning().actionData() = usGridNo1;
			pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
			return( AI_ACTION_SCHEDULE_MOVE );
		}
		break;
	case SCHEDULE_ACTION_LEAVESECTOR:
		//Doesn't use any gridno data
		switch( pSoldier->schedule().progress() )
		{
		case 0: // start the action

			pSoldier->aiPlanning().actionData() = FindNearestEdgePoint( pSoldier->position().gridNo() );
			
			if (TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
			{
#ifdef JA2BETAVERSION
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_BETAVERSION, L"Civilian could not find path to map edge!" );
#endif
				DoneScheduleAction( pSoldier );
				return( AI_ACTION_NONE );
			}

			if ( pSoldier->position().gridNo() == pSoldier->aiPlanning().actionData() )
			{
				// time to go off the map
				pSoldier->schedule().advanceProgress();
			}
			else
			{
				// move!
				pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
				return( AI_ACTION_SCHEDULE_MOVE );
			}

			// fall through

		case 1: // near edge

			pSoldier->aiPlanning().actionData() = FindNearbyPointOnEdgeOfMap( pSoldier, &bDirection );
			
			if (TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
			{
				// what the heck??
				// ABORT!
				DoneScheduleAction( pSoldier );
			}
			else
			{
				pSoldier->dialogue().quoteActionId() = GetTraversalQuoteActionID( bDirection );
				pSoldier->schedule().advanceProgress();
				pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
				return( AI_ACTION_SCHEDULE_MOVE );
			}
			break;

		case 2: // should now be done!
			DoneScheduleAction( pSoldier );
			break;

		default:
			break;
		}
		break;

	case SCHEDULE_ACTION_ENTERSECTOR:
		if ( pSoldier->identity().profile() != NO_PROFILE && gMercProfiles[ pSoldier->identity().profile() ].ubMiscFlags2 & PROFILE_MISC_FLAG2_DONT_ADD_TO_SECTOR )//Moa: changed 'ubMiscFlags' to 'ubMiscFlags2'
		{
			// ignore.
			DoneScheduleAction( pSoldier );
			break;
		}
		switch( pSoldier->schedule().progress() )
		{
		case 0:
			ConvertGridNoToCenterCellXY(pSoldier->deployment().offWorldGrid(), &sX, &sY);
			(void)TacticalActorWorldPlacement::setPosition(*pSoldier, sX, sY );
			pSoldier->roster().inSector() = TRUE;
			MoveSoldierFromAwayToMercSlot(
				GetJa2TacticalEntityId(*pSoldier));
			pSoldier->aiPlanning().actionData() = usGridNo1;
			pSoldier->schedule().advanceProgress();
			pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
			return( AI_ACTION_SCHEDULE_MOVE );
		case 1:
			if (pSoldier->position().gridNo() == usGridNo1)
			{
				DoneScheduleAction( pSoldier );
				
				if (!TileIsOutOfBounds(pSoldier->position().gridNo()))
				{
					pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
				}
			}
			else
			{
				pSoldier->aiPlanning().actionData() = usGridNo1;
				pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
				return( AI_ACTION_SCHEDULE_MOVE );
			}
			break;
		}
		break;

	case SCHEDULE_ACTION_WAKE:
		// Go to this position
		if (pSoldier->position().gridNo() == pSoldier->position().initialGrid())
		{
			// th-th-th-that's it!
			DoneScheduleAction( pSoldier );
			pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
		}
		else
		{
			pSoldier->aiPlanning().actionData() = pSoldier->position().initialGrid();
			pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
			return( AI_ACTION_SCHEDULE_MOVE );
		}
		break;

	case SCHEDULE_ACTION_SLEEP:
		// Go to this position
		if (pSoldier->position().gridNo() == usGridNo1)
		{
			// Sleep
			pSoldier->aiBehavior().flags() |= AI_ASLEEP;
			DoneScheduleAction( pSoldier );
			
			if (!TileIsOutOfBounds(pSoldier->position().gridNo()))
			{
				pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
			}
		}
		else
		{
			pSoldier->aiPlanning().actionData() = usGridNo1;
			pSoldier->movement().absoluteDestination() = pSoldier->aiPlanning().actionData();
			return( AI_ACTION_SCHEDULE_MOVE );
		}
		break;
	}


	return( AI_ACTION_NONE );
}

INT8 DecideActionBoxerEnteringRing(TacticalActor *pSoldier)
{
	//DBrot: More Rooms
	//UINT8 ubRoom;
	UINT16 usRoom;
	INT32	sDesiredMercLoc;
	UINT8 ubDesiredMercDir;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif
	// boxer, should move into ring!
	if ( InARoom( pSoldier->position().gridNo(), &usRoom ))
	{
		if (usRoom == BOXING_RING)
		{
			// look towards nearest player
			sDesiredMercLoc = ClosestPC( pSoldier, NULL );
			
			if (!TileIsOutOfBounds(sDesiredMercLoc))
			{
				// see if we are facing this person
				ubDesiredMercDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sDesiredMercLoc);

				// if not already facing in that direction,
				if ( pSoldier->position().direction() != ubDesiredMercDir && TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubDesiredMercDir) )
				{

					pSoldier->aiPlanning().actionData() = ubDesiredMercDir;

#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - TURNS TOWARDS CLOSEST PC to face direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					return( AI_ACTION_CHANGE_FACING );
				}
			}
			return( AI_ACTION_ABSOLUTELY_NONE );
		}
		else
		{
			// move to starting spot
			INT32 sRingSpot = FindClosestBoxingRingSpot( pSoldier, TRUE );
			if ( TileIsOutOfBounds( sRingSpot ) )
			{
				// No legal spot in the ring this pass: end the turn rather than
				// issue a GET_CLOSER toward NOWHERE, which the AI would otherwise
				// re-decide every tick and hang the match.
				return( AI_ACTION_ABSOLUTELY_NONE );
			}
			pSoldier->aiPlanning().actionData() = sRingSpot;
			return( AI_ACTION_GET_CLOSER );
		}
	}

	return( AI_ACTION_ABSOLUTELY_NONE );
}

INT8 DecideActionNamedNPC( TacticalActor * pSoldier )
{
	INT32		sDesiredMercLoc;
	UINT8		ubDesiredMercDir;
	SoldierID	ubDesiredMerc;
	INT32		sDesiredMercDist;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif

	// if a quote record has been set and we're not doing movement, then
	// it means we have to wait until someone is nearby and then see
	// to do...

	// is this person close enough to trigger event?
	if (pSoldier->dialogue().hasQuoteRecord() && pSoldier->dialogue().quoteActionId() == QUOTE_ACTION_ID_TURNTOWARDSPLAYER )
	{
		sDesiredMercLoc = ClosestPC( pSoldier, &sDesiredMercDist );
		
		if (!TileIsOutOfBounds(sDesiredMercLoc))
		{
			if ( sDesiredMercDist <= NPC_TALK_RADIUS * 2)
			{
				pSoldier->dialogue().quoteRecord() = 0;
				// see if this triggers a conversation/NPC record
				PCsNearNPC( pSoldier->identity().profile() );
				// clear "handle every frame" flag
				pSoldier->aiBehavior().flags() &= (~AI_HANDLE_EVERY_FRAME);
				return( AI_ACTION_ABSOLUTELY_NONE );
			}

			// see if we are facing this person
			ubDesiredMercDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sDesiredMercLoc);

			// if not already facing in that direction,
			if (pSoldier->position().direction() != ubDesiredMercDir && TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubDesiredMercDir) )
			{

				pSoldier->aiPlanning().actionData() = ubDesiredMercDir;

#ifdef DEBUGDECISIONS
				sprintf(tempstr,"%s - TURNS TOWARDS CLOSEST PC to face direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
				AIPopMessage(tempstr);
#endif

				return( AI_ACTION_CHANGE_FACING );
			}
		}

		// do nothing; we're looking at the PC or the NPC is far away
		return( AI_ACTION_ABSOLUTELY_NONE );

	}
	else
	{
		///////////////
		// CHECK TO SEE IF WE WANT TO GO UP TO PERSON AND SAY SOMETHING
		///////////////
		pSoldier->aiPlanning().actionData() = NPCConsiderInitiatingConv( pSoldier, &ubDesiredMerc );
		
		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
			return( AI_ACTION_APPROACH_MERC );
		}
	}

	if ( TacticalActorConditions::isAssassin(*pSoldier) )
	{
		sDesiredMercLoc = ClosestPC( pSoldier, &sDesiredMercDist );
		
		if (!TileIsOutOfBounds(sDesiredMercLoc))
		{
			if ( sDesiredMercDist <= NPC_TALK_RADIUS * 2 )
			{
				AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
				// now wait a bit!
				pSoldier->aiPlanning().actionData() = 5000;
				return( AI_ACTION_WAIT );
			}
			else
			{
				pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sDesiredMercLoc, AI_ACTION_APPROACH_MERC );
				
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
				{
					return( AI_ACTION_APPROACH_MERC );
				}
			}
		}
	}

	return( AI_ACTION_NONE );
}


INT8 DecideActionGreen(TacticalActor *pSoldier)
{
	DOUBLE iChance, iSneaky = 10;
	INT8  bInWater, bInDeepWater, bInGas;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif

	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen, orders = %d",pSoldier->aiBehavior().orders()));

	DebugAI(AI_MSG_START, pSoldier, String("[Green]"));
	LogDecideInfo(pSoldier);

	// sevenfm: disable stealth mode
	pSoldier->movement().setStealth(false);
	// disable reverse movement mode
	pSoldier->movement().setReverse(false);
	// sevenfm: initialize data
	pSoldier->attackSelection().weaponMode() = WM_NORMAL;

	BOOLEAN fCivilian = (PTR_CIVILIAN && (pSoldier->roster().civilianGroup() == NON_CIV_GROUP || pSoldier->aiBehavior().neutral() || (pSoldier->identity().bodyType() >= FATCIV && pSoldier->identity().bodyType() <= CRIPPLECIV) ) );
	BOOLEAN fCivilianOrMilitia = PTR_CIV_OR_MILITIA;

	gubNPCPathCount = 0;

	if ( gTacticalStatus.bBoxingState != NOT_BOXING )
	{
		if (pSoldier->status().flags() & SOLDIER_BOXER)
		{
			if ( gTacticalStatus.bBoxingState == PRE_BOXING )
			{
				return( DecideActionBoxerEnteringRing( pSoldier ) );
			}
			else
			{
				//DBrot: More Rooms
				//UINT8	ubRoom;
				UINT16 usRoom;
				UINT8 ubLoop;

				// boxer... but since in status green, it's time to leave the ring!
				if ( InARoom( pSoldier->position().gridNo(), &usRoom ))
				{
					if (usRoom == BOXING_RING)
					{
						for ( ubLoop = 0; ubLoop < NUM_BOXERS; ++ubLoop )
						{
							if (pSoldier->identity().id() == gubBoxerID[ ubLoop ])
							{
								// we should go back where we started
								pSoldier->aiPlanning().actionData() = gsBoxerGridNo[ ubLoop ];
								return( AI_ACTION_GET_CLOSER );
							}
						}
						pSoldier->aiPlanning().actionData() = FindClosestBoxingRingSpot( pSoldier, FALSE );
						return( AI_ACTION_GET_CLOSER );
					}
					else
					{
						// done!

						// Flugente: only do this if we are not boxing. Otherwise this might interfere with boxing scripts, as they temporariyl  set a PC under AI control (when leaaving the ring)
						if ( gTacticalStatus.bBoxingState == NOT_BOXING )
						{
							// WANNE: This should fix the bug if any merc are still under PC control. This could happen after boxing in SAN MONA.
							TacticalActor	*pTeamSoldier;
							for ( SoldierID bLoop=gTacticalStatus.Team[gbPlayerNum].bFirstID; bLoop <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++bLoop )
							{
								pTeamSoldier =
									GetJa2SoldierRepository().resolve(bLoop.i);
								if (!pTeamSoldier)
								{
									continue;
								}

								if (pTeamSoldier->status().flags() & SOLDIER_PCUNDERAICONTROL)
									pTeamSoldier->status().flags() &= (~SOLDIER_PCUNDERAICONTROL);

								TacticalActorAiBehavior::clearBoxerFlag(*pTeamSoldier);
							}
						}

						if (pSoldier->roster().team() == gbPlayerNum || CountPeopleInBoxingRing() == 0)
						{
							TriggerEndOfBoxingRecord( pSoldier );
						}
					}
				}

				return( AI_ACTION_ABSOLUTELY_NONE );
			}
		}
		//else if ( (gTacticalStatus.bBoxingState == PRE_BOXING || gTacticalStatus.bBoxingState == BOXING) && ( PythSpacesAway( pSoldier->sGridNo, CENTER_OF_RING ) <= TacticalActorVisibility::normalMaximumDistance() ) )
		else if ( PythSpacesAway( pSoldier->position().gridNo(), CENTER_OF_RING ) <= TacticalActorVisibility::normalMaximumDistance() )
		{
			UINT8 ubRingDir;
			// face ring!

			ubRingDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), CENTER_OF_RING);
			if ( gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
			{
				if ( pSoldier->position().direction() != ubRingDir )
				{
					pSoldier->aiPlanning().actionData() = ubRingDir;
					return( AI_ACTION_CHANGE_FACING );
				}
			}
			return( AI_ACTION_NONE );
		}
	}

	if ( !gGameExternalOptions.fEnemyTanksCanMoveInTactical && ARMED_VEHICLE( pSoldier ) )
	{
		return( AI_ACTION_NONE );
	}


	bInWater = Water(pSoldier->position().gridNo(), pSoldier->position().level());
	bInDeepWater = DeepWater(pSoldier->position().gridNo(), pSoldier->position().level());

	// check if standing in tear gas without a gas mask on, or in smoke
	bInGas = InGasOrSmoke( pSoldier, pSoldier->position().gridNo() );

	// Flugente: tanks do not care about gas
	if ( ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier ) )
	{
		bInGas = FALSE;
	}

	// if real-time, and not in the way, do nothing 90% of the time (for GUARDS!)
	// unless in water (could've started there), then we better swim to shore!

	if (fCivilian || (gGameExternalOptions.fAllNamedNpcsDecideAction && pSoldier->identity().profile() != NO_PROFILE))
	{
		// special stuff for civs

		if (pSoldier->status().flags() & SOLDIER_COWERING)
		{
			// everything's peaceful again, stop cowering!!
			pSoldier->aiPlanning().actionData() = ANIM_STAND;
			return( AI_ACTION_STOP_COWERING );
		}

		if (!gfTurnBasedAI)
		{
			// ******************
			// REAL TIME NPC CODE
			// ******************
			if (pSoldier->aiBehavior().flags() & AI_CHECK_SCHEDULE)
			{
				pSoldier->aiPlanning().action() = DecideActionSchedule( pSoldier );
				if (pSoldier->aiPlanning().action() != AI_ACTION_NONE)
				{
					return( pSoldier->aiPlanning().action() );
				}
			}

			if ( pSoldier->identity().profile() != NO_PROFILE || TacticalActorConditions::isAssassin(*pSoldier) )
			{
				if ( pSoldier->identity().profile() != NO_PROFILE )
					pSoldier->aiPlanning().action() = DecideActionNamedNPC( pSoldier );
				else
				{
					INT32 sDesiredMercDist;
					INT32 sDesiredMercLoc = ClosestUnDisguisedPC( pSoldier, &sDesiredMercDist );
		
					if (!TileIsOutOfBounds(sDesiredMercLoc))
					{
						if ( sDesiredMercDist <= NPC_TALK_RADIUS * 2 )
						{
							AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
							// now wait a bit!
							pSoldier->aiPlanning().actionData() = 5000;
							pSoldier->aiPlanning().action() = AI_ACTION_WAIT;
						}
						else
						{
							pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sDesiredMercLoc, AI_ACTION_APPROACH_MERC );
				
							if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
							{
								pSoldier->aiPlanning().action() = AI_ACTION_APPROACH_MERC;
							}
						}
					}
				}

				if ( pSoldier->aiPlanning().action() != AI_ACTION_NONE )
				{
					return( pSoldier->aiPlanning().action() );
				}
				// can we act again? not for a minute since we were last spoken to/triggered a record
				if ( pSoldier->dialogue().lastSpokeAt() && (GetJA2Clock() < pSoldier->dialogue().lastSpokeAt() + 60000) )
				{
					return( AI_ACTION_NONE );
				}
				// turn off counter so we don't check it again
				pSoldier->dialogue().lastSpokeAt() = 0;
			}
		}

		// if not in the way, do nothing most of the time
		// unless in water (could've started there), then we better swim to shore!

		if (!(bInDeepWater) && PreRandom( 5 ) )
		{
			// don't do nuttin!
			return( AI_ACTION_NONE );
		}

	}

//ddd{
	if( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && gGameExternalOptions.bNewTacticalAIBehavior && pSoldier->roster().team() == ENEMY_TEAM )
	{
		if ( !IsJa2TacticalTurnBased() && IsJa2TacticalCombatActive() )
		{
			INT32				cnt;
			ROTTING_CORPSE *	pCorpse;

			for ( cnt = 0; cnt < giNumRottingCorpse; ++cnt )
			{
				pCorpse = &(gRottingCorpse[ cnt ] );
			
				if ( pCorpse->fActivated && pCorpse->def.ubAIWarningValue > 0 )
				{
					if ( PythSpacesAway( pSoldier->position().gridNo(), pCorpse->def.sGridNo ) <= 5 )//add check(comparison) of sight range variable (smaxvid ?)
					{
						//check if the corpse is in the enemny/militia field of view?
						//CHRISL: Shouldn't we be using the corpse's bLevel?  Otherwise a soldier inside a building can see a corpse on the roof of that building
						//if ( SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pSoldier->position().level(), 3, TRUE, CALC_FROM_WANTED_DIR ) )
						if ( SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pCorpse->def.bLevel, 3, TRUE, CALC_FROM_WANTED_DIR ) )
						{
							ScreenMsg( MSG_FONT_YELLOW, MSG_INTERFACE, New113Message[MSG113_ENEMY_FOUND_DEAD_BODY]);
							//pCorpse->def.ubAIWarningValue=0;
							gRottingCorpse[ cnt ].def.ubAIWarningValue=0;
							return( AI_ACTION_RED_ALERT );
						}
					}
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////
		// IF YOU SEE CAPTURED FRIENDS, FREE THEM!
		////////////////////////////////////////////////////////////////////////////

		// Flugente: if we see one of our buddies in handcuffs, its a clear sign of enemy activity!
		if ( gGameExternalOptions.fAllowPrisonerSystem && pSoldier->roster().team() == ENEMY_TEAM && !gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition )
		{
			SoldierID ubPerson = GetClosestFlaggedSoldierID( pSoldier, 20, ENEMY_TEAM, SOLDIER_POW, TRUE );

			if ( ubPerson != NOBODY )
			{	
				// raise alarm!
				return( AI_ACTION_RED_ALERT );
			}
		}

		// if we are a doctor with medical gear, we might be able to help a wounded ally
		if (TacticalActorMedicalServices::
				canTreatForAi(*pSoldier))
		{
			SoldierID ubPerson = GetClosestWoundedSoldierID( pSoldier, gGameExternalOptions.sEnemyMedicsSearchRadius, pSoldier->roster().team());
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			// are we ourselves the patient?
			if ( ubPerson == pSoldier->identity().id() )
			{
				// if not already crouched, crouch down first
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_CROUCH && IsValidStance( pSoldier, ANIM_CROUCH ) && GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current() )
				{
					pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

					return(AI_ACTION_CHANGE_STANCE);
				}

				return(AI_ACTION_DOCTOR_SELF);
			}
			else if ( person )
			{
				if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) < 2 )
				{
					// see if we are facing this person
					UINT8 ubDesiredMercDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), person->position().gridNo());

					// if not already facing in that direction,
					if ( pSoldier->position().direction() != ubDesiredMercDir )
					{
						pSoldier->aiPlanning().actionData() = ubDesiredMercDir;

						return( AI_ACTION_CHANGE_FACING );
					}

					// if not already crouched, crouch down first
					if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_CROUCH && IsValidStance( pSoldier, ANIM_CROUCH ) && GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current() )
					{
						pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

						return(AI_ACTION_CHANGE_STANCE);
					}

					return(AI_ACTION_DOCTOR);
				}
				else
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
		// if we are not a medic, but are wounded, seek a medic
		else if ( pSoldier->vitals().healableInjury() >= gGameExternalOptions.sEnemyMedicsWoundMinAmount )
		{
			SoldierID ubPerson = GetClosestMedicSoldierID( pSoldier, gGameExternalOptions.sEnemyMedicsSearchRadius / 2, pSoldier->roster().team());
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) > 1 )
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
		
		// are we a bodyguard?
		if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_BODYGUARD )
		{
			// is VIP still alive?
			SoldierID ubPerson = GetClosestFlaggedSoldierID( pSoldier, 100, pSoldier->roster().team(), SOLDIER_VIP, FALSE );
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				// we want to stay close to him, but still be able to function properly... stay withing a 7-tile radius
				if ( SpacesAway( pSoldier->position().gridNo(), person->position().gridNo() ) > 7 )
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0 );

					if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
	}
//ddd}

	////////////////////////////////////////////////////////////////////////////
	// POINT PATROL: move towards next point unless getting a bit winded
	////////////////////////////////////////////////////////////////////////////

	// this takes priority over water/gas checks, so that point patrol WILL work
	// from island to island, and through gas covered areas, too
	if ((pSoldier->aiBehavior().orders() == POINTPATROL) && (pSoldier->vitals().breath() >= 75))
	{
		if (PointPatrolAI(pSoldier))
		{
			if (!gfTurnBasedAI)
			{
				// wait after this...
				pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
				pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
			}
			return(AI_ACTION_POINT_PATROL);
		}
		else
		{
			// Reset path count to avoid dedlok
			gubNPCPathCount = 0;
		}
	}

	if ((pSoldier->aiBehavior().orders() == RNDPTPATROL) && (pSoldier->vitals().breath() >=75))
	{
		if (RandomPointPatrolAI(pSoldier))
		{
			if (!gfTurnBasedAI)
			{
				// wait after this...
				pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
				pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
			}
			return(AI_ACTION_POINT_PATROL);
		}
		else
		{
			// Reset path count to avoid dedlok
			gubNPCPathCount = 0;
		}

	}

	////////////////////////////////////////////////////////////////////////////
	// WHEN LEFT IN WATER OR GAS, GO TO NEAREST REACHABLE SPOT OF UNGASSED LAND
	////////////////////////////////////////////////////////////////////////////

	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: get out of water and gas"));

	if (bInDeepWater || bInGas || FindBombNearby(pSoldier, pSoldier->position().gridNo(), BOMB_DETECTION_RANGE) || RedSmokeDanger(pSoldier->position().gridNo(), pSoldier->position().level()))
	{
		pSoldier->aiPlanning().actionData() = FindNearestUngassedLand(pSoldier);
		
		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
#ifdef DEBUGDECISIONS
			sprintf(tempstr,"%s - SEEKING NEAREST UNGASSED LAND at grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
			AIPopMessage(tempstr);
#endif

			return(AI_ACTION_LEAVE_WATER_GAS);
		}
	}



	////////////////////////////////////////////////////////////////////////
	// REST IF RUNNING OUT OF BREATH
	////////////////////////////////////////////////////////////////////////

	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: rest if running out of breath"));
	// if our breath is running a bit low, and we're not in the way or in water
	if ((pSoldier->vitals().breath() < 75) && !bInWater)
	{
		// take a breather for gods sake!
		// for realtime, AI will use a standard wait set outside of here
		pSoldier->aiPlanning().actionData() = NOWHERE;
		return(AI_ACTION_NONE);
	}


	////////////////////////////////////////////////////////////////////////////
	// CLIMB A BUILDING
	////////////////////////////////////////////////////////////////////////////

	if (!fCivilian && 
		TacticalActorAiBehavior::hasInitialActionPoints(*pSoldier) &&
		pSoldier->aiPlanning().lastAction() != AI_ACTION_CLIMB_ROOF &&
		pSoldier->aiBehavior().orders() != STATIONARY &&
		pSoldier->position().level() == 0 &&
		!ENEMYROBOT(pSoldier) &&
		!is_networked)
	{
		iChance = 10 + pSoldier->aiBehavior().bypassToGreen();

		// set base chance and maximum seeking distance according to orders
		switch (pSoldier->aiBehavior().orders())
		{
		case STATIONARY:     iChance *= 0; break;
		case ONGUARD:        iChance += 10; break;
		case ONCALL:                         break;
		case CLOSEPATROL:    iChance += -20; break;
		case RNDPTPATROL:
		case POINTPATROL:    iChance  = -30; break;
		case FARPATROL:      iChance += -40; break;
		case SEEKENEMY:      iChance += -30; break;
		case SNIPER:		 iChance += 70; break;
		}

		// modify for attitude
		switch (pSoldier->aiBehavior().attitude())
		{
		case DEFENSIVE:      iChance *= 1.5;  break;
		case BRAVESOLO:      iChance /= 2;    break;
		case BRAVEAID:       iChance /= 2;   break;
		case CUNNINGSOLO:    iChance *= 1;    break;
		case CUNNINGAID:     iChance /= 1;   break;
		case AGGRESSIVE:     iChance /= 3;    break;
		case ATTACKSLAYONLY:									 break;
		}


		//hide those suicidal militia on the roofs for better defensive positions
		// 0verhaul:  If they are allowed at all to move
		if ( pSoldier->roster().team() == MILITIA_TEAM && iChance != 0)
			iChance += 20;

		// reduce chance for any injury, less likely to hop up if hurt
		iChance -= (pSoldier->vitals().maximumHealth() - pSoldier->vitals().health());

		// reduce chance if breath is down
		//iChance -= (100 - pSoldier->vitals().breath());         // don't care

		// This is the chance that we want to be on the roof.  If already there, invert the chance to see if we want back
		// down
		if (pSoldier->position().level() > 0)
		{
			iChance = 100 - iChance;
		}

		if ((INT16) PreRandom(100) < iChance)
		{
			BOOLEAN fUp = FALSE;
			if ( pSoldier->position().level() == 0 )
			{
				fUp = TRUE;
			}
			else if (pSoldier->position().level() > 0 )
			{
				fUp = FALSE;
			}

			if ( CanClimbFromHere ( pSoldier, fUp ) )
			{
				DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("Soldier %d is climbing roof",pSoldier->identity().id()) );
				return( AI_ACTION_CLIMB_ROOF );
			}
			else
			{
				pSoldier->aiPlanning().actionData() = FindClosestClimbPoint(pSoldier, fUp );
				// Added the check here because sniper militia who are locked inside of a building without keys
				// will still have a >100% chance to want to climb, which means an infinite loop.  In fact, any
				// time a move is desired, there probably also will be a need to check for a path.				
				if ( !TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) &&
					LegalNPCDestination(pSoldier,pSoldier->aiPlanning().actionData(),ENSURE_PATH,WATEROK, 0 ))
				{
					return( AI_ACTION_MOVE_TO_CLIMB  );
				}
			}
		}
	}


	////////////////////////////////////////////////////////////////////////////
	// RANDOM PATROL:  determine % chance to start a new patrol route
	////////////////////////////////////////////////////////////////////////////
	if (!gubNPCPathCount) // try to limit pathing in Green AI
	{

		iChance = 25 + pSoldier->aiBehavior().bypassToGreen();

		// set base chance according to orders
		switch (pSoldier->aiBehavior().orders())
		{
		case STATIONARY:     iChance += -20;  break;
		case ONGUARD:        iChance += -15;  break;
		case ONCALL:                          break;
		case CLOSEPATROL:    iChance += +15;  break;
		case RNDPTPATROL:
		case POINTPATROL:		iChance = 0; break;
			/*
			if ( !gfTurnBasedAI )
			{
			// realtime deadlock... increase chance!
			iChance = 110;// more than 100 in case person is defensive
			}
			else if ( pSoldier->actionPoints().initial() < pSoldier->actionPoints().current() ) // could be less because of carried-over points
			{
			// CJC: allow pt patrol guys to do a random move in case
			// of a deadlock provided they haven't done anything yet this turn
			iChance=   0;
			}
			break;
			*/
		case FARPATROL:      iChance += +25;  break;
		case SEEKENEMY:      iChance += -10;  break;
		case SNIPER:		iChance += -10;  break;
		}

		// modify chance of patrol (and whether it's a sneaky one) by attitude
		switch (pSoldier->aiBehavior().attitude())
		{
		case DEFENSIVE:      iChance += -10;                 break;
		case BRAVESOLO:      iChance +=   5;                 break;
		case BRAVEAID:                                       break;
		case CUNNINGSOLO:    iChance +=   5;  iSneaky += 10; break;
		case CUNNINGAID:                      iSneaky +=  5; break;
		case AGGRESSIVE:     iChance +=  10;  iSneaky += -5; break;
		case ATTACKSLAYONLY: iChance +=  10;  iSneaky += -5; break;
		}

		// reduce chance for any injury, less likely to wander around when hurt
		iChance -= (pSoldier->vitals().maximumHealth() - pSoldier->vitals().health());

		// reduce chance if breath is down, less likely to wander around when tired
		iChance -= (100 - pSoldier->vitals().breath());


		// if we're in water with land miles (> 25 tiles) away,
		// OR if we roll under the chance calculated
		if (bInWater || ((INT16) PreRandom(100) < iChance))
		{
			pSoldier->aiPlanning().actionData() = RandDestWithinRange(pSoldier);
			
			if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
			{
				pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, pSoldier->aiPlanning().actionData(), AI_ACTION_RANDOM_PATROL );
			}
			
			if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
			{
#ifdef DEBUGDECISIONS
				sprintf(tempstr,"%s - RANDOM PATROL to grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
				AIPopMessage(tempstr);
#endif

				if (!gfTurnBasedAI)
				{
					// wait after this...
					pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
					pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
				}
				return(AI_ACTION_RANDOM_PATROL);
			}
		}
	}

	if (!gubNPCPathCount) // try to limit pathing in Green AI
	{
		////////////////////////////////////////////////////////////////////////////
		// SEEK FRIEND: determine %chance for man to pay a friendly visit
		////////////////////////////////////////////////////////////////////////////

		iChance = 25 + pSoldier->aiBehavior().bypassToGreen();

		// set base chance and maximum seeking distance according to orders
		switch (pSoldier->aiBehavior().orders())
		{
		case STATIONARY:     iChance += -20; break;
		case ONGUARD:        iChance += -15; break;
		case ONCALL:                         break;
		case CLOSEPATROL:    iChance += +10; break;
		case RNDPTPATROL:
		case POINTPATROL:    iChance  = -10; break;
		case FARPATROL:      iChance += +20; break;
		case SEEKENEMY:      iChance += -10; break;
		case SNIPER:		  iChance += -10; break;
		}

		// modify for attitude
		switch (pSoldier->aiBehavior().attitude())
		{
		case DEFENSIVE:                       break;
		case BRAVESOLO:      iChance /= 2;    break;  // loners
		case BRAVEAID:       iChance += 10;   break;  // friendly
		case CUNNINGSOLO:    iChance /= 2;    break;  // loners
		case CUNNINGAID:     iChance += 10;   break;  // friendly
		case AGGRESSIVE:                      break;
		case ATTACKSLAYONLY:									 break;
		}

		// reduce chance for any injury, less likely to wander around when hurt
		iChance -= (pSoldier->vitals().maximumHealth() - pSoldier->vitals().health());

		// reduce chance if breath is down
		iChance -= (100 - pSoldier->vitals().breath());         // very likely to wait when exhausted


		if ((INT16) PreRandom(100) < iChance)
		{
			if (RandomFriendWithin(pSoldier))
			{
				if ( pSoldier->aiPlanning().actionData() == GoAsFarAsPossibleTowards( pSoldier, pSoldier->aiPlanning().actionData(), AI_ACTION_SEEK_FRIEND ) )
				{

#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - SEEK FRIEND at grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					if (fCivilianOrMilitia && !gfTurnBasedAI)
					{
						// pause at the end of the walk!
						pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
						pSoldier->aiPlanning().nextActionData() = (UINT16) REALTIME_CIV_AI_DELAY;
					}

					return(AI_ACTION_SEEK_FRIEND);
				}
			}
		}
	}


	////////////////////////////////////////////////////////////////////////////
	// SNIPERS LIKE TO CROUCH (on roofs)
	////////////////////////////////////////////////////////////////////////////

	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Snipers like to crouch, sniper = %d",pSoldier->aiPlanning().sniperPosture()));
	// if not in water and not already crouched, try to crouch down first
	if (pSoldier->aiBehavior().orders() == SNIPER && !PTR_CROUCHED && IsValidStance( pSoldier, ANIM_CROUCH ) && pSoldier->position().level() == 1 )
	{
		if (!gfTurnBasedAI || (GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current()))
		{
			DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Sniper is crouching"));
			pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
			pSoldier->aiPlanning().lowerSniperPosture();
			return(AI_ACTION_CHANGE_STANCE);
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// SNIPER - RAISE WEAPON TO SCAN AREA
	////////////////////////////////////////////////////////////////////////////

	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Snipers like to raise weapons, sniper = %d",pSoldier->aiPlanning().sniperPosture()));
	if ( pSoldier->aiBehavior().orders() == SNIPER && pSoldier->aiPlanning().sniperPosture() == 0 && ( pSoldier->position().level() == 1 || Random(100) < 40 ) && (pSoldier->vitals().breath() > 30 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 20) )
	{
		if (!WeaponReady(pSoldier) && 
			TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION)
		{
			if (!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, READY_RIFLE_CROUCH ) <= pSoldier->actionPoints().current())
			{
				DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Sniper is raising weapon, soldier = %d, sniper = %d",pSoldier->identity().id(),pSoldier->aiPlanning().sniperPosture()));
				pSoldier->aiPlanning().raiseSniperPosture();
				DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Sniper = %d",pSoldier->aiPlanning().sniperPosture()));
				return(AI_ACTION_RAISE_GUN);
			}
		}
	}
	//else if ( pSoldier->aiPlanning().sniperPosture() == 1 )
	//{
	//	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Sniper is lowering weapon, sniper = %d",pSoldier->aiPlanning().sniperPosture()));
	//	pSoldier->aiPlanning().lowerSniperPosture();
	//	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Sniper = %d",pSoldier->aiPlanning().sniperPosture()));
	//	return(AI_ACTION_LOWER_GUN);
	//}

	////////////////////////////////////////////////////////////////////////////
	// SANDRO - occasionally, allow regular soldiers to scan around too
	if (IsScoped(&pSoldier->inventory()[HANDPOS]))
	{
		if (!WeaponReady(pSoldier) && 
			TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION)
		{
			if ((!gfTurnBasedAI || ((GetAPsToReadyWeapon( pSoldier, TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) ) ) <= pSoldier->actionPoints().current())) &&
				 (pSoldier->vitals().breath() > 30 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 20) )
			{
				iChance = 25;
				if ( pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA || pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE )
					iChance += 15;
				else if ( pSoldier->roster().soldierClass() == SOLDIER_CLASS_GREEN_MILITIA || pSoldier->roster().soldierClass() == SOLDIER_CLASS_ADMINISTRATOR || pSoldier->roster().soldierClass() == SOLDIER_CLASS_BANDIT )
					iChance -= 15;
				if ( Random(100) < iChance ) 
				{
					DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Soldier deciding to raise weapon with scope"));
					return(AI_ACTION_RAISE_GUN);
				}
			}
		}
		else // if the weapon is ready already, maybe unready it
		{
			iChance = 30;
			// is it a heavy gun? And we have energy cost for shooting enabled? 
			iChance += GetBPCostPer10APsForGunHolding( pSoldier ); // don't overexagerate yourself
			if ( Random(100) < iChance ) 
			{
				DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Soldier deciding to lower weapon"));
				return(AI_ACTION_LOWER_GUN);
			}
		}
	}
	////////////////////////////////////////////////////////////////////////////


	////////////////////////////////////////////////////////////////////////////
	// LOOK AROUND: determine %chance for man to turn in place
	////////////////////////////////////////////////////////////////////////////

	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Soldier deciding to turn"));
	if (!gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current())
	{
		// avoid 2 consecutive random turns in a row
		if (pSoldier->aiPlanning().lastAction() != AI_ACTION_CHANGE_FACING)
		{
			iChance = 25 + pSoldier->aiBehavior().bypassToGreen();

			// set base chance according to orders
			if (pSoldier->aiBehavior().orders() == STATIONARY || pSoldier->aiBehavior().orders() == SNIPER)
				iChance += 25;

			if (pSoldier->aiBehavior().orders() == ONGUARD)
				iChance += 20;

			if (pSoldier->aiBehavior().attitude() == DEFENSIVE)
				iChance += 25;

			if ( pSoldier->aiBehavior().orders() == SNIPER && pSoldier->position().level() == 1)
				iChance += 35;

			if ( WeaponReady(pSoldier) ) // SANDRO - if readied weapon, make him more likely to turn around
				iChance += 30;

			if ((INT16)PreRandom(100) < iChance)
			{
				// roll random directions (stored in actionData) until different from current
				do
				{
					// if man has a LEGAL dominant facing, and isn't facing it, he will turn
					// back towards that facing 50% of the time here (normally just enemies)
					if ((pSoldier->aiPlanning().dominantDirection() >= 0) && (pSoldier->aiPlanning().dominantDirection() <= 8) &&
						(pSoldier->position().direction() != pSoldier->aiPlanning().dominantDirection()) && PreRandom(2) && pSoldier->aiBehavior().orders() != SNIPER )
					{
						pSoldier->aiPlanning().actionData() = pSoldier->aiPlanning().dominantDirection();
					}
					else
					{
						INT32 iNoiseValue;
						BOOLEAN fClimb;
						BOOLEAN fReachable;
						INT32 sNoiseGridNo = MostImportantNoiseHeard(pSoldier,&iNoiseValue, &fClimb, &fReachable);
						UINT8 ubNoiseDir;
						
						if (TileIsOutOfBounds(sNoiseGridNo) || 
							( ubNoiseDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sNoiseGridNo) ) == pSoldier->position().direction() )
						
						{
							pSoldier->aiPlanning().actionData() = PreRandom(8);
						}
						else
						{
							pSoldier->aiPlanning().actionData() = ubNoiseDir;
						}
					}
				} while (pSoldier->aiPlanning().actionData() == pSoldier->position().direction());


#ifdef DEBUGDECISIONS
				sprintf(tempstr,"%s - TURNS to face direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
				AIPopMessage(tempstr);
#endif

				DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Trying to turn - checking stance validity, sniper = %d",pSoldier->aiPlanning().sniperPosture()));
				if ( TacticalActorMobility::isCurrentStanceValid(*pSoldier, (INT8) pSoldier->aiPlanning().actionData()) )
				{

					if ( !gfTurnBasedAI )
					{
						// wait after this...
						pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
						pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
					}

					DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionGreen: Soldier is turning"));
					return(AI_ACTION_CHANGE_FACING);
				}
			}
		}
	}


	////////////////////////////////////////////////////////////////////////////
	// NONE:
	////////////////////////////////////////////////////////////////////////////

	// by default, if everything else fails, just stands in place without turning
	// for realtime, regular AI guys will use a standard wait set outside of here
	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);
}

INT8 DecideActionYellow(TacticalActor *pSoldier)
{
	INT32 iDummy;
	UINT8 ubNoiseDir;
	INT32 sNoiseGridNo;
	INT32 iNoiseValue;
	INT32 iChance, iSneaky;
	INT32 sClosestFriend;
	BOOLEAN fCivilian = (PTR_CIVILIAN && (pSoldier->roster().civilianGroup() == NON_CIV_GROUP || pSoldier->aiBehavior().neutral() || (pSoldier->identity().bodyType() >= FATCIV && pSoldier->identity().bodyType() <= CRIPPLECIV) ) );
	BOOLEAN fClimb;
	BOOLEAN fReachable;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif	

	DebugAI(AI_MSG_START, pSoldier, String("[Yellow]"));
	LogDecideInfo(pSoldier);

	// sevenfm: disable stealth mode
	pSoldier->movement().setStealth(false);
	// disable reverse movement mode
	pSoldier->movement().setReverse(false);
	// sevenfm: initialize data
	pSoldier->attackSelection().weaponMode() = WM_NORMAL;

	if (fCivilian || (gGameExternalOptions.fAllNamedNpcsDecideAction && pSoldier->identity().profile() != NO_PROFILE))
	{
		if (pSoldier->status().flags() & SOLDIER_COWERING)
		{
			// everything's peaceful again, stop cowering!!
			pSoldier->aiPlanning().actionData() = ANIM_STAND;
			return( AI_ACTION_STOP_COWERING );
		}
		if (!gfTurnBasedAI)
		{
			// ******************
			// REAL TIME NPC CODE
			// ******************
			if (pSoldier->identity().profile() != NO_PROFILE || TacticalActorConditions::isAssassin(*pSoldier) )
			{
				if ( pSoldier->identity().profile() != NO_PROFILE )
					pSoldier->aiPlanning().action() = DecideActionNamedNPC( pSoldier );
				else
				{
					INT32 sDesiredMercDist;
					INT32 sDesiredMercLoc = ClosestUnDisguisedPC( pSoldier, &sDesiredMercDist );

					// Flugente: if this guy is disguised, do not consider him
		
					if (!TileIsOutOfBounds(sDesiredMercLoc))
					{
						if ( sDesiredMercDist <= NPC_TALK_RADIUS * 2 )
						{
							AddToShouldBecomeHostileOrSayQuoteList( pSoldier->identity().id() );
							// now wait a bit!
							pSoldier->aiPlanning().actionData() = 5000;
							pSoldier->aiPlanning().action() = AI_ACTION_WAIT;
						}
						else
						{
							pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sDesiredMercLoc, AI_ACTION_APPROACH_MERC );
				
							if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
							{
								pSoldier->aiPlanning().action() = AI_ACTION_APPROACH_MERC;
							}
						}
					}
				}

				if ( pSoldier->aiPlanning().action() != AI_ACTION_NONE )
				{
					return( pSoldier->aiPlanning().action() );
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// WHEN IN GAS, GO TO NEAREST REACHABLE SPOT OF UNGASSED LAND
	////////////////////////////////////////////////////////////////////////////

	if (InGas(pSoldier, pSoldier->position().gridNo()) || DeepWater(pSoldier->position().gridNo(), pSoldier->position().level()) || FindBombNearby(pSoldier, pSoldier->position().gridNo(), BOMB_DETECTION_RANGE))
	{
		pSoldier->aiPlanning().actionData() = FindNearestUngassedLand(pSoldier);

		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
			return(AI_ACTION_LEAVE_WATER_GAS);
		}
	}

	// determine the most important noise heard, and its relative value
	sNoiseGridNo = MostImportantNoiseHeard(pSoldier,&iNoiseValue, &fClimb, &fReachable);
	//NumMessage("iNoiseValue = ",iNoiseValue);
	
	if (TileIsOutOfBounds(sNoiseGridNo))
	{
		// then we have no business being under YELLOW status any more!
#ifdef BETAVERSION
		NumMessage("DecideActionYellow: ERROR - No important noise known by guynum ",pSoldier->identity().id());
#endif
		return(AI_ACTION_NONE);
	}

	if( gGameExternalOptions.bNewTacticalAIBehavior )
	{
		////////////////////////////////////////////////////////////////////////////
		// IF YOU SEE CAPTURED FRIENDS, FREE THEM!
		////////////////////////////////////////////////////////////////////////////

		// Flugente: if we see one of our buddies captured, it is a clear sign of enemy activity!
		if ( gGameExternalOptions.fAllowPrisonerSystem && pSoldier->roster().team() == ENEMY_TEAM )
		{
			SoldierID ubPerson = GetClosestFlaggedSoldierID( pSoldier, 20, ENEMY_TEAM, SOLDIER_POW, TRUE );
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				// if we are close, we can release this guy
				// possible only if not handcuffed (binders can be opened, handcuffs not)
				if ( !HasItemFlag( (&(person->inventory()[HANDPOS]))->usItem, HANDCUFFS ) )
				{
					if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) < 2 )
					{
						// see if we are facing this person
						UINT8 ubDesiredMercDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), person->position().gridNo());

						// if not already facing in that direction,
						if ( pSoldier->position().direction() != ubDesiredMercDir )
						{
							pSoldier->aiPlanning().actionData() = ubDesiredMercDir;

							return( AI_ACTION_CHANGE_FACING );
						}

						return(AI_ACTION_FREE_PRISONER);
					}
					else
					{
						pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
						if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
						{
							return(AI_ACTION_SEEK_FRIEND);
						}
					}
				}
				else if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && !gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition )
				{
					// raise alarm!
					return( AI_ACTION_RED_ALERT );
				}
			}
		}

		// if we are a doctor with medical gear, we might be able to help a wounded ally
		if (TacticalActorMedicalServices::
				canTreatForAi(*pSoldier))
		{
			SoldierID ubPerson = GetClosestWoundedSoldierID( pSoldier, gGameExternalOptions.sEnemyMedicsSearchRadius, pSoldier->roster().team());
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			// are we ourselves the patient?
			if ( ubPerson == pSoldier->identity().id() )
			{
				// if not already crouched, crouch down first
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_CROUCH && IsValidStance( pSoldier, ANIM_CROUCH ) && GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current() )
				{
					pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

					return(AI_ACTION_CHANGE_STANCE);
				}

				return(AI_ACTION_DOCTOR_SELF);
			}
			else if ( person )
			{
				if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) < 2 )
				{
					// see if we are facing this person
					UINT8 ubDesiredMercDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), person->position().gridNo());

					// if not already facing in that direction,
					if ( pSoldier->position().direction() != ubDesiredMercDir )
					{
						pSoldier->aiPlanning().actionData() = ubDesiredMercDir;

						return( AI_ACTION_CHANGE_FACING );
					}

					// if not already crouched, crouch down first
					if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_CROUCH && IsValidStance( pSoldier, ANIM_CROUCH ) && GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current() )
					{
						pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

						return(AI_ACTION_CHANGE_STANCE);
					}

					return(AI_ACTION_DOCTOR);
				}
				else
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
		// if we are not a medic, but are wounded, seek a medic
		else if ( pSoldier->vitals().healableInjury() >= gGameExternalOptions.sEnemyMedicsWoundMinAmount )
		{
			SoldierID ubPerson = GetClosestMedicSoldierID( pSoldier, gGameExternalOptions.sEnemyMedicsSearchRadius / 2, pSoldier->roster().team());
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) > 1 )
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
		
		// are we a bodyguard?
		if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_BODYGUARD )
		{
			// is VIP still alive?
			SoldierID ubPerson = GetClosestFlaggedSoldierID( pSoldier, 100, pSoldier->roster().team(), SOLDIER_VIP, FALSE );
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				// we want to stay close to him, but still be able to function properly... stay withing a 7-tile radius
				if ( SpacesAway( pSoldier->position().gridNo(), person->position().gridNo() ) > 7 )
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0 );

					if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// LOOK AROUND TOWARD NOISE: determine %chance for man to turn towards noise
	////////////////////////////////////////////////////////////////////////////

	// determine direction from this soldier in which the noise lies
	ubNoiseDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sNoiseGridNo);

	// if soldier is not already facing in that direction,
	// and the noise source is close enough that it could possibly be seen
	if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
	{
		if ((pSoldier->position().direction() != ubNoiseDir) && PythSpacesAway(pSoldier->position().gridNo(),sNoiseGridNo) <= TacticalActorVisibility::maximumDistance(*pSoldier, sNoiseGridNo) )
		{
			// set base chance according to orders
			if ((pSoldier->aiBehavior().orders() == STATIONARY) || (pSoldier->aiBehavior().orders() == ONGUARD) )
				iChance = 50;
			else           // all other orders
				iChance = 25;

			if (pSoldier->aiBehavior().attitude() == DEFENSIVE)
				iChance += 15;


			if ((INT16)PreRandom(100) < iChance && TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubNoiseDir) )
			{
				pSoldier->aiPlanning().actionData() = ubNoiseDir;
#ifdef DEBUGDECISIONS
				sprintf(tempstr,"%s - TURNS TOWARDS NOISE to face direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
				AIPopMessage(tempstr);
#endif
				if ( pSoldier->aiBehavior().orders() == SNIPER &&
					(pSoldier->vitals().breath() > 25 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 30) &&
					!WeaponReady(pSoldier) &&
					TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION)
				{
					if (!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, READY_RIFLE_CROUCH ) <= pSoldier->actionPoints().current())
					{
						pSoldier->aiPlanning().nextAction() = AI_ACTION_RAISE_GUN;
					}
				}
				////////////////////////////////////////////////////////////////////////////
				// SANDRO - allow regular soldiers to raise scoped weapons to see farther away too
				if (IsScoped(&pSoldier->inventory()[HANDPOS]))
				{
					if (!WeaponReady(pSoldier) && 
						TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION &&
						(pSoldier->vitals().breath() > 25 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 30))
					{
						if (!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) ) <= pSoldier->actionPoints().current())
						{
							if ( Random(100) < 35 ) 
							{
								pSoldier->aiPlanning().nextAction() = AI_ACTION_RAISE_GUN;
							}
						}
					}
				}
				////////////////////////////////////////////////////////////////////////////
				
				return(AI_ACTION_CHANGE_FACING);
			}
		}
	}


	////////////////////////////////////////////////////////////////////////////
	// RADIO YELLOW ALERT: determine %chance to call others and report noise
	////////////////////////////////////////////////////////////////////////////

	// if we have the action points remaining to RADIO
	// (we never want NPCs to choose to radio if they would have to wait a turn)
	if ( !fCivilian && (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) &&
		(gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) )
	{
		// base chance depends on how much new info we have to radio to the others
		iChance = 5 * WhatIKnowThatPublicDont(pSoldier,FALSE);   // use 5 * for YELLOW alert

		// if I actually know something they don't and I ain't swimming (deep water)
		if (iChance && !DeepWater( pSoldier->position().gridNo(), pSoldier->position().level() ))
		{

			// CJC: this addition allows for varying difficulty levels for soldier types
			iChance += gbDiff[ DIFF_RADIO_RED_ALERT ][ SoldierDifficultyLevel( pSoldier ) ] / 2;

			// Alex: this addition replaces the sectorValue/2 in original JA
			//iChance += gsDiff[DIFF_RADIO_RED_ALERT][GameOption[ENEMYDIFFICULTY]] / 2;

			// modify base chance according to orders
			switch (pSoldier->aiBehavior().orders())
			{
			case STATIONARY: iChance +=  20;  break;
			case ONGUARD:    iChance +=  15;  break;
			case ONCALL:     iChance +=  10;  break;
			case CLOSEPATROL:                 break;
			case RNDPTPATROL:
			case POINTPATROL:                 break;
			case FARPATROL:  iChance += -10;  break;
			case SEEKENEMY:  iChance += -20;  break;
			case SNIPER:		iChance += -10; break; //Madd: sniper contacts are supposed to be automatically reported
			}

			// modify base chance according to attitude
			switch (pSoldier->aiBehavior().attitude())
			{
			case DEFENSIVE:  iChance +=  20;  break;
			case BRAVESOLO:  iChance += -10;  break;
			case BRAVEAID:                    break;
			case CUNNINGSOLO:iChance +=  -5;  break;
			case CUNNINGAID:                  break;
			case AGGRESSIVE: iChance += -20;  break;
			case ATTACKSLAYONLY: iChance = 0; break;
			}

#ifdef DEBUGDECISIONS
			AINumMessage("Chance to radio yellow alert = ",iChance);
#endif

			if ((INT16)PreRandom(100) < iChance)
			{
#ifdef DEBUGDECISIONS
				AINameMessage(pSoldier,"decides to radio a YELLOW alert!",1000);
#endif

				return(AI_ACTION_YELLOW_ALERT);
			}
		}
	}

	if ( !gGameExternalOptions.fEnemyTanksCanMoveInTactical && ARMED_VEHICLE( pSoldier ) )
	{
		return( AI_ACTION_NONE );
	}

	////////////////////////////////////////////////////////////////////////
	// REST IF RUNNING OUT OF BREATH
	////////////////////////////////////////////////////////////////////////

	// if our breath is running a bit low, and we're not in water
	if ((pSoldier->vitals().breath() < 25) && !TacticalActorMobility::inWater(*pSoldier))
	{
		// take a breather for gods sake!
		pSoldier->aiPlanning().actionData() = NOWHERE;
		
		// is it a heavy gun? And we have energy cost for shooting enabled? 
		if ( WeaponReady(pSoldier) && GetBPCostPer10APsForGunHolding( pSoldier ) > 0 )
		{
			// unready
			return(AI_ACTION_LOWER_GUN); 
		}

		return(AI_ACTION_NONE);
	}

	//continue flanking
	INT32 sFlankGridNo;
	
	if (TileIsOutOfBounds(sNoiseGridNo))
		sFlankGridNo = pSoldier->aiPlanning().flankAnchorGrid();
	else
		sFlankGridNo = sNoiseGridNo;

	if ( pSoldier->aiPlanning().flankCount() > 0 && pSoldier->aiPlanning().flankCount() < MAX_FLANKS_YELLOW )
	{
		INT16 currDir = GetDirectionFromGridNo ( sFlankGridNo, pSoldier );
		INT16 origDir = pSoldier->aiPlanning().flankOriginDirection();
		pSoldier->aiPlanning().advanceFlank();
		if ( pSoldier->aiPlanning().lastFlankLeft() )
		{
			if ( origDir > currDir )
				origDir -= NUM_WORLD_DIRECTIONS;

			// stop flanking if reached desired direction
			if ( (currDir - origDir) >= MinFlankDirections(pSoldier) )
			{
				pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
			else
			{
				pSoldier->aiPlanning().actionData() = FindFlankingSpot (pSoldier, sFlankGridNo , AI_ACTION_FLANK_LEFT);
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) ) //&& (currDir - origDir) < 2 )
					return AI_ACTION_FLANK_LEFT ;
				else
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
		}
		else
		{
			if ( origDir < currDir )
				origDir += NUM_WORLD_DIRECTIONS;

			// stop flanking if reached desired direction
			if ( (origDir - currDir) >= MinFlankDirections(pSoldier) )
			{
				pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
			else
			{
				pSoldier->aiPlanning().actionData() = FindFlankingSpot (pSoldier, sFlankGridNo , AI_ACTION_FLANK_RIGHT);
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))//&& (origDir - currDir) < 2 )
					return AI_ACTION_FLANK_RIGHT ;
				else
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
		}
	}

	if ( pSoldier->aiPlanning().flankCount() == MAX_FLANKS_YELLOW )
	{
		pSoldier->aiPlanning().advanceFlank();
		pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards(pSoldier,sFlankGridNo,AI_ACTION_SEEK_NOISE);
		return AI_ACTION_SEEK_NOISE ;
	}

	// Hmmm, I don't think this check is doing what is intended.  But then I see no comment about what is intended.
	// However, civilians with no profile (and likely no weapons) do not need to be seeking out noises.  Most don't
	// even have the body type for it (can't climb or jump).
	//if ( !( pSoldier->roster().team() == CIV_TEAM && pSoldier->identity().profile() != NO_PROFILE && pSoldier->identity().profile() != ELDIN ) )
	//if ( pSoldier->roster().team() != CIV_TEAM || ( !pSoldier->aiBehavior().neutral() && pSoldier->identity().profile() != ELDIN ) )
	// ADB: Eldin is the only neutral civilian who should be seeking out noises.  As the museum curator, he can be
	// available to talk to.  As the night watchman, he needs to look for thieves.
	bool onCivTeam = (pSoldier->roster().team() == CIV_TEAM);
	bool isNamedCiv = (pSoldier->identity().profile() != NO_PROFILE);
	bool isEldin = (pSoldier->identity().profile() == ELDIN);//logically flipped from the original, isNotEldin == false is confusing
	// For purpose of seeking noise, cowardly civs are neutral, even if attacked by your thugs
	bool isNeutral = pSoldier->aiBehavior().neutral() || pSoldier->status().flags() & SOLDIER_COWERING;
	if (
		(onCivTeam == false) || //true #1
		(onCivTeam == true && isNamedCiv == true && isNeutral == false) || //true #2
		(onCivTeam == true && isEldin == true)//true #3
		)
	{
		// IF WE ARE MILITIA/CIV IN REALTIME, CLOSE TO NOISE, AND CAN SEE THE SPOT WHERE THE NOISE CAME FROM, FORGET IT
		if ( fReachable && !fClimb && !gfTurnBasedAI && (pSoldier->roster().team() == MILITIA_TEAM || pSoldier->roster().team() == CIV_TEAM )&& PythSpacesAway( pSoldier->position().gridNo(), sNoiseGridNo ) < 5 )
		{
			if ( SoldierTo3DLocationLineOfSightTest( pSoldier, sNoiseGridNo, pSoldier->position().level(), 0, TRUE, 6 )	)
			{
				// set reachable to false so we don't investigate
				fReachable = FALSE;
				// forget about noise
				pSoldier->perception().noiseGrid() = NOWHERE;
				pSoldier->perception().noiseVolume() = 0;
			}
		}

		////////////////////////////////////////////////////////////////////////////
		// SEEK NOISE
		////////////////////////////////////////////////////////////////////////////

		if ( fReachable )
		{
			// remember that noise value is negative, and closer to 0 => more important!
			iChance = 95 + (iNoiseValue / 3);
			iSneaky = 30;

			// increase

			// set base chance according to orders
			switch (pSoldier->aiBehavior().orders())
			{
			case STATIONARY:     iChance += -20;  break;
			case ONGUARD:        iChance += -15;  break;
			case ONCALL:                          break;
			case CLOSEPATROL:    iChance += -10;  break;
			case RNDPTPATROL:
			case POINTPATROL:                     break;
			case FARPATROL:      iChance +=  10;  break;
			case SEEKENEMY:      iChance +=  25;  break;
			case SNIPER:		  iChance += -10; break;
			}

			// modify chance of patrol (and whether it's a sneaky one) by attitude
			switch (pSoldier->aiBehavior().attitude())
			{
			case DEFENSIVE:      iChance += -10;  iSneaky +=  15;  break;
			case BRAVESOLO:      iChance +=  10;                   break;
			case BRAVEAID:       iChance +=   5;                   break;
			case CUNNINGSOLO:    iChance +=   5;  iSneaky +=  30;  break;
			case CUNNINGAID:                      iSneaky +=  30;  break;
			case AGGRESSIVE:     iChance +=  20;  iSneaky += -10;  break;
			case ATTACKSLAYONLY:	iChance +=  20;  iSneaky += -10;  break;
			}


			// reduce chance if breath is down, less likely to wander around when tired
			iChance -= (100 - pSoldier->vitals().breath());

			//Madd: make militia less likely to go running headlong into trouble
			if ( pSoldier->roster().team() == MILITIA_TEAM )
				iChance -= 30;

			if ((INT16) PreRandom(100) < iChance  )
			{

				pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards(pSoldier,sNoiseGridNo,AI_ACTION_SEEK_NOISE);
				
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
				{
#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - INVESTIGATING NOISE at grid %d, moving to %d",
						pSoldier->identity().name(),sNoiseGridNo,pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					if ( !ENEMYROBOT(pSoldier) && fClimb )//&& pSoldier->aiPlanning().actionData() == sNoiseGridNo)
					{
						// need to climb AND have enough APs to get there this turn
						BOOLEAN fUp = TRUE;
						if (pSoldier->position().level() > 0 )
							fUp = FALSE;

						if (!fUp)
							DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("Soldier %d, is climbing down",pSoldier->identity().id()) );

						// 0verhaul:  the Closest Noise call returns the location of a climb.  So 1) it's not necessary to
						// ask if we can climb from here.  And 2) It's not necessary to look for the climb point.  We already
						// have it.
//						if ( CanClimbFromHere ( pSoldier, fUp ) )
						if ( pSoldier->position().gridNo() == sNoiseGridNo)
						{
							if (IsActionAffordable(pSoldier) && pSoldier->actionPoints().current() >= ( APBPConstants[AP_CLIMBROOF] + MinAPsToAttack( pSoldier, sNoiseGridNo, ADDTURNCOST,0)))
							{
								return( AI_ACTION_CLIMB_ROOF );
							}
						}
						else
						{
//							pSoldier->aiPlanning().actionData() = FindClosestClimbPoint(pSoldier, pSoldier->sGridNo , sNoiseGridNo , fUp );
							pSoldier->aiPlanning().actionData() = sNoiseGridNo;
							//if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
							{
								return( AI_ACTION_MOVE_TO_CLIMB  );
							}
						}
					}

					// possibly start YELLOW flanking
					if( gGameExternalOptions.fAIYellowFlanking && 
						( pSoldier->aiBehavior().attitude() == CUNNINGAID || pSoldier->aiBehavior().attitude() == CUNNINGSOLO ) &&
						pSoldier->roster().team() == ENEMY_TEAM &&
						( CountFriendsInDirection( pSoldier, sNoiseGridNo ) > 0 || NightTime() ) &&
						( pSoldier->aiBehavior().orders() == SEEKENEMY ||
						pSoldier->aiBehavior().orders() == FARPATROL ||
						pSoldier->aiBehavior().orders() == CLOSEPATROL && NightTime() ))
					{
						INT8 action = AI_ACTION_SEEK_NOISE;
						INT16 dist = PythSpacesAway ( pSoldier->position().gridNo(), sNoiseGridNo );
						if ( dist > MIN_FLANK_DIST_YELLOW && dist < MAX_FLANK_DIST_YELLOW  )
						{
							INT16 rdm = Random(6);

							switch (rdm)
							{
							case 1:
							case 2:
							case 3:
								if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
									action = AI_ACTION_FLANK_LEFT ;
								break;
							default:
								if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
									action = AI_ACTION_FLANK_RIGHT ;
								break;
							}
						}
						else
							return AI_ACTION_SEEK_NOISE ;

						pSoldier->aiPlanning().actionData() = FindFlankingSpot (pSoldier, sNoiseGridNo, action );
						
						if (TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) || pSoldier->aiPlanning().flankCount() >= MAX_FLANKS_YELLOW  )
						{
							pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards(pSoldier,sNoiseGridNo,AI_ACTION_SEEK_NOISE);
							//pSoldier->aiPlanning().clearFlank();
							return(AI_ACTION_SEEK_NOISE);
						}
						else
						{
							if ( action == AI_ACTION_FLANK_LEFT )
								pSoldier->aiPlanning().lastFlankLeft() = TRUE;
							else
								pSoldier->aiPlanning().lastFlankLeft() = FALSE;

							pSoldier->aiPlanning().recordFlankStep(
								sNoiseGridNo,
								GetDirectionFromGridNo( sNoiseGridNo, pSoldier ) );

							// sevenfm: change orders CLOSEPATROL -> FARPATROL
							if( pSoldier->aiBehavior().orders() == CLOSEPATROL )
							{
								pSoldier->aiBehavior().orders() = FARPATROL;
							}

							return(action);
						}
					}
					else
					{
						return(AI_ACTION_SEEK_NOISE);
					}

				}
			}
		}


		////////////////////////////////////////////////////////////////////////////
		// SEEK FRIEND WHO LAST RADIOED IN TO REPORT NOISE
		////////////////////////////////////////////////////////////////////////////

		sClosestFriend = ClosestReachableFriendInTrouble(pSoldier, &fClimb);

		// if there is a friend alive & reachable who last radioed in		
		if (!TileIsOutOfBounds(sClosestFriend))
		{
			// there a chance enemy soldier choose to go "help" his friend
			iChance = 50 - SpacesAway(pSoldier->position().gridNo(),sClosestFriend);
			iSneaky = 10;

			// set base chance according to orders
			switch (pSoldier->aiBehavior().orders())
			{
			case STATIONARY:     iChance += -20;  break;
			case ONGUARD:        iChance += -15;  break;
			case ONCALL:         iChance +=  20;  break;
			case CLOSEPATROL:    iChance += -10;  break;
			case RNDPTPATROL:
			case POINTPATROL:    iChance += -10;  break;
			case FARPATROL:                       break;
			case SEEKENEMY:      iChance +=  10;  break;
			case SNIPER:		  iChance += -10; break;
			}

			// modify chance of patrol (and whether it's a sneaky one) by attitude
			switch (pSoldier->aiBehavior().attitude())
			{
			case DEFENSIVE:      iChance += -10;  iSneaky +=  15;        break;
			case BRAVESOLO:                                              break;
			case BRAVEAID:       iChance +=  20;  iSneaky += -10;        break;
			case CUNNINGSOLO:					   iSneaky +=  30;		  break;
			case CUNNINGAID:     iChance +=  20;  iSneaky +=  20;        break;
			case AGGRESSIVE:     iChance += -20;  iSneaky += -20;        break;
			case ATTACKSLAYONLY: iChance += -20;  iSneaky += -20;        break;
			}

			// reduce chance if breath is down, less likely to wander around when tired
			iChance -= (100 - pSoldier->vitals().breath());

			if ((INT16)PreRandom(100) < iChance)
			{
				pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards(pSoldier,sClosestFriend,AI_ACTION_SEEK_FRIEND);
				
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
				{
#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - SEEKING FRIEND at %d, MOVING to %d",
						pSoldier->identity().name(),sClosestFriend,pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					if ( !ENEMYROBOT(pSoldier) && fClimb )//&& pSoldier->aiPlanning().actionData() == sClosestFriend)
					{
						// need to climb AND have enough APs to get there this turn
						BOOLEAN fUp = TRUE;
						if (pSoldier->position().level() > 0 )
							fUp = FALSE;

						if (!fUp)
							DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("Soldier %d is climbing down",pSoldier->identity().id()) );

						// 0verhaul:  Closest Friend call also returns the climb point if climbing is necessary.  So don't
						// climb the wrong building and don't search again
						//if ( CanClimbFromHere ( pSoldier, fUp ) )
						if (pSoldier->position().gridNo() == sClosestFriend)
						{
							if (IsActionAffordable(pSoldier) )
							{
								return( AI_ACTION_CLIMB_ROOF );
							}
						}
						else
						{
							//pSoldier->aiPlanning().actionData() = FindClosestClimbPoint(pSoldier, pSoldier->sGridNo , sClosestFriend , fUp );
							pSoldier->aiPlanning().actionData() = sClosestFriend;
							//if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
							{
								return( AI_ACTION_MOVE_TO_CLIMB  );
							}
						}
					}

					//if (fClimb && pSoldier->aiPlanning().actionData() == sClosestFriend)
					//{
					//// need to climb AND have enough APs to get there this turn
					//return( AI_ACTION_CLIMB_ROOF );
					//}

					return(AI_ACTION_SEEK_FRIEND);
				}
			}
		}


		////////////////////////////////////////////////////////////////////////////
		// TAKE BEST NEARBY COVER FROM THE NOISE GENERATING GRIDNO
		////////////////////////////////////////////////////////////////////////////

		if (!SkipCoverCheck ) // && gfTurnBasedAI) // only do in turnbased
		{
			// remember that noise value is negative, and closer to 0 => more important!
			iChance = 25;
			iSneaky = 30;

			// set base chance according to orders
			switch (pSoldier->aiBehavior().orders())
			{
			case STATIONARY:     iChance +=  20;  break;
			case ONGUARD:        iChance +=  15;  break;
			case ONCALL:                          break;
			case CLOSEPATROL:    iChance +=  10;  break;
			case RNDPTPATROL:
			case POINTPATROL:                     break;
			case FARPATROL:      iChance +=  -5;  break;
			case SEEKENEMY:      iChance += -20;  break;
			case SNIPER:		  iChance +=  20; break;
			}

			// modify chance (and whether it's sneaky) by attitude
			switch (pSoldier->aiBehavior().attitude())
			{
			case DEFENSIVE:      iChance +=  10;  iSneaky +=  15;  break;
			case BRAVESOLO:      iChance += -15;  iSneaky += -20;  break;
			case BRAVEAID:       iChance += -20;  iSneaky += -20;  break;
			case CUNNINGSOLO:    iChance +=  20;  iSneaky +=  30;  break;
			case CUNNINGAID:     iChance +=  15;  iSneaky +=  30;  break;
			case AGGRESSIVE:     iChance += -10;  iSneaky += -10;  break;
			case ATTACKSLAYONLY: iChance += -10;  iSneaky += -10;  break;
			}


			//Madd: make militia more likely to take cover
			if ( pSoldier->roster().team() == MILITIA_TEAM )
				iChance += 20;

			// reduce chance if breath is down, less likely to wander around when tired
			iChance -= (100 - pSoldier->vitals().breath());

			if ((INT16)PreRandom(100) < iChance)
			{
				pSoldier->morale().aiMorale() = CalcMorale( pSoldier );
				pSoldier->aiPlanning().actionData() = FindBestNearbyCover(pSoldier,pSoldier->morale().aiMorale(),&iDummy);
				
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
				{
#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - TAKING COVER at grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					return(AI_ACTION_TAKE_COVER);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// SWITCH TO GREEN: determine if soldier acts as if nothing at all was wrong
	////////////////////////////////////////////////////////////////////////////
	if ((INT16)PreRandom(100) < 50 )
	{
#ifdef DEBUGDECISIONS
		AINameMessage(pSoldier,"ignores noise completely and BYPASSES to GREEN!",1000);
#endif
		// Skip YELLOW until new situation, 15% extra chance to do GREEN actions
		pSoldier->aiBehavior().bypassToGreen() = 15;
		return(DecideActionGreen(pSoldier));
	}


	////////////////////////////////////////////////////////////////////////////
	// CROUCH IF NOT CROUCHING ALREADY
	////////////////////////////////////////////////////////////////////////////

	// if not in water and not already crouched, try to crouch down first
	if (!fCivilian && !PTR_CROUCHED && IsValidStance( pSoldier, ANIM_CROUCH ) )
	{
#ifdef DEBUGDECISIONS
		sprintf(tempstr,"%s CROUCHES (STATUS YELLOW)",pSoldier->identity().name());
		AIPopMessage(tempstr);
#endif

		if (!gfTurnBasedAI || GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current())
		{
			////////////////////////////////////////////////////////////////////////////
			// SANDRO - raise weapon maybe
			if (!WeaponReady(pSoldier) && 
				TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION &&
				pSoldier->position().direction() == ubNoiseDir &&	// if we are facing the direction of where the noise came from
				(pSoldier->vitals().breath() > 25 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 30))
			{
				if (!gfTurnBasedAI || (((GetAPsToReadyWeapon( pSoldier, TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) ) ) + GetAPsToChangeStance( pSoldier, ANIM_CROUCH )) <= pSoldier->actionPoints().current()))
				{
					if (IsScoped(&pSoldier->inventory()[HANDPOS]))
					{
						pSoldier->aiPlanning().nextAction() = AI_ACTION_RAISE_GUN;
					}
				}
			}
			////////////////////////////////////////////////////////////////////////////

			pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
			return(AI_ACTION_CHANGE_STANCE);
		}
	}
	else if (!fCivilian)
	{
		////////////////////////////////////////////////////////////////////////////
		// SANDRO - raise weapon maybe
		if (!WeaponReady(pSoldier) && 
			TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION &&
			pSoldier->position().direction() == ubNoiseDir && // if we are facing the direction of where the noise came from
			(pSoldier->vitals().breath() > 25 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 30))
		{
			if (!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, pSoldier->animationPlayback().state() ) <= pSoldier->actionPoints().current())
			{
				if (IsScoped(&pSoldier->inventory()[HANDPOS]))
				{
					if ( Random(100) < 35 ) 
					{
						return( AI_ACTION_RAISE_GUN );
					}
				}
			}
		}
		////////////////////////////////////////////////////////////////////////////	
	}


	////////////////////////////////////////////////////////////////////////////
	// DO NOTHING: Not enough points left to move, so save them for next turn
	////////////////////////////////////////////////////////////////////////////

#ifdef DEBUGDECISIONS
	AINameMessage(pSoldier,"- DOES NOTHING (YELLOW)",1000);
#endif

	// by default, if everything else fails, just stands in place without turning
	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);
}


INT8 DecideActionRed(TacticalActor *pSoldier)
{
	INT8	bActionReturned;
	INT32	iDummy;
	INT32	iChance;
	INT32	sClosestOpponent = NOWHERE, sClosestFriend = NOWHERE;
	INT32	sClosestDisturbance = NOWHERE, sCheckGridNo;
	INT32	sDistVisible;
	UINT8	ubCanMove,ubOpponentDir;
	INT8	bInWater, bInDeepWater, bInGas;
	INT8	bSeekPts = 0, bHelpPts = 0, bHidePts = 0, bWatchPts = 0;
	INT8	bHighestWatchLoc;
	ATTACKTYPE BestThrow, BestShot;

	// sevenfm:
	BOOLEAN fProneSightCover = FALSE;
	BOOLEAN fDangerousSpot = FALSE;	
	BOOLEAN fAnyCover = FALSE;
	BOOLEAN fCanBeSeen = FALSE;
	INT32	sOpponentGridNo;
	INT8	bOpponentLevel;

#ifdef AI_TIMING_TEST
	UINT32	uiStartTime, uiEndTime;
#endif
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif
	BOOLEAN fClimb;
	BOOLEAN fCivilian = (PTR_CIVILIAN && (pSoldier->roster().civilianGroup() == NON_CIV_GROUP ||
		(pSoldier->aiBehavior().neutral() && gTacticalStatus.fCivGroupHostile[pSoldier->roster().civilianGroup()] == CIV_GROUP_NEUTRAL) ||
		(pSoldier->identity().bodyType() >= FATCIV && pSoldier->identity().bodyType() <= CRIPPLECIV) ) );

	// WANNE: Headrock informed me that I should remove that because it needs a lot of CPU!
	// HEADROCK HAM B2.7: Calculate the overall tactical situation
	//INT16 ubOverallTacticalSituation = AssessTacticalSituation(pSoldier->roster().side());

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("DecideActionRed: soldier orders = %d",pSoldier->aiBehavior().orders()));

	DebugAI(AI_MSG_START, pSoldier, String("[Red]"));
	LogDecideInfo(pSoldier);

	// sevenfm: disable stealth mode
	pSoldier->movement().setStealth(false);
	// disable reverse movement mode
	pSoldier->movement().setReverse(false);
	// sevenfm: initialize data
	pSoldier->attackSelection().weaponMode() = WM_NORMAL;

	// if we have absolutely no action points, we can't do a thing under RED!
	if ( pSoldier->actionPoints().current() <= 0 ) //Action points can be negative
	{
		pSoldier->aiPlanning().actionData() = NOWHERE;
		return(AI_ACTION_NONE);
	}

	// sevenfm: find closest opponent
	sClosestOpponent = ClosestKnownOpponent(pSoldier, &sOpponentGridNo, &bOpponentLevel);
	DebugAI(AI_MSG_INFO, pSoldier, String("sClosestOpponent %d", sClosestOpponent));

	if (!SightCoverAtSpot(pSoldier, pSoldier->position().gridNo(), FALSE))
	{
		fCanBeSeen = TRUE;
		DebugAI(AI_MSG_INFO, pSoldier, String("can be seen"));
	}

	fProneSightCover = ProneSightCoverAtSpot(pSoldier, pSoldier->position().gridNo(), FALSE);
	DebugAI(AI_MSG_INFO, pSoldier, String("prone sight cover %d", fProneSightCover));
	fAnyCover = AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo());
	DebugAI(AI_MSG_INFO, pSoldier, String("any cover %d", fAnyCover));

	if (!fProneSightCover || pSoldier->suppression().underFire())
	{
		fDangerousSpot = TRUE;
	}

	// can this guy move to any of the neighbouring squares ? (sets TRUE/FALSE)
	ubCanMove = (pSoldier->actionPoints().current() >= MinPtsToMove(pSoldier));

	// sevenfm: before deciding anything, stop cowering
	if (SoldierAI(pSoldier) &&
		!fCivilian &&
		ubCanMove &&
		pSoldier->vitals().health() >= OKLIFE &&
		!pSoldier->collapseState().tactical() &&
		!pSoldier->collapseState().breathTriggered() &&
		TacticalActorConditions::isCowering(*pSoldier))
	{
		return AI_ACTION_STOP_COWERING;
	}

	// sevenfm: stop giving aid
	if (SoldierAI(pSoldier) &&
		pSoldier->actionPoints().current() > 0 &&
		pSoldier->vitals().health() >= OKLIFE &&
		!pSoldier->collapseState().tactical() &&
		!pSoldier->collapseState().breathTriggered() &&
		TacticalActorConditions::isGivingAid(*pSoldier))
	{
		return AI_ACTION_STOP_MEDIC;
	}

	// if we're an alerted enemy, and there are panic bombs or a trigger around
	if ( !ENEMYROBOT(pSoldier) )
	{
		if ( (!PTR_CIVILIAN || pSoldier->identity().profile() == WARDEN) && ( ( gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition || (pSoldier->identity().id() == gTacticalStatus.ubTheChosenOne) || (pSoldier->identity().profile() == WARDEN) ) &&
			(gTacticalStatus.fPanicFlags & (PANIC_BOMBS_HERE | PANIC_TRIGGERS_HERE ) ) ) )
		{
			if ( pSoldier->identity().profile() == WARDEN && gTacticalStatus.ubTheChosenOne == NOBODY )
			{
				PossiblyMakeThisEnemyChosenOne( pSoldier );
			}

			// do some special panic AI decision making
			bActionReturned = PanicAI(pSoldier,ubCanMove);

			// if we decided on an action while in there, we're done
			if (bActionReturned != -1)
				return(bActionReturned);
		}

		if ( pSoldier->identity().profile() != NO_PROFILE )
		{
			if ( (pSoldier->identity().profile() == QUEEN || pSoldier->identity().profile() == JOE) && ubCanMove )
			{
				if ( gWorldSectorX == 3 && gWorldSectorY == MAP_ROW_P && gbWorldSectorZ == 0 && !gfUseAlternateQueenPosition )
				{
					bActionReturned = HeadForTheStairCase( pSoldier );
					if ( bActionReturned != AI_ACTION_NONE )
					{
						return( bActionReturned );
					}
				}
			}
		}
	}


	// determine if we happen to be in water (in which case we're in BIG trouble!)
	bInWater = Water( pSoldier->position().gridNo(), pSoldier->position().level() );
	bInDeepWater = DeepWater( pSoldier->position().gridNo(), pSoldier->position().level() );

	// check if standing in tear gas without a gas mask on
	bInGas = InGasOrSmoke( pSoldier, pSoldier->position().gridNo() );

	// Flugente: tanks do not care about gas
	if ( ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier ) )
	{
		bInGas = FALSE;
	}

	////////////////////////////////////////////////////////////////////////////
	// WHEN LEFT IN GAS, WEAR GAS MASK IF AVAILABLE AND NOT WORN
	////////////////////////////////////////////////////////////////////////////

	if ( !bInGas && (gWorldSectorX == TIXA_SECTOR_X && gWorldSectorY == TIXA_SECTOR_Y) )
	{
		// only chance if we happen to be caught with our gas mask off
		if ( PreRandom( 10 ) == 0 && WearGasMaskIfAvailable( pSoldier ) )
		{
			// reevaluate
			bInGas = InGasOrSmoke( pSoldier, pSoldier->position().gridNo() );
		}
	}

	//Only put mask on in gas
	if(bInGas && WearGasMaskIfAvailable(pSoldier))//dnl ch40 200909
		bInGas = InGasOrSmoke(pSoldier, pSoldier->position().gridNo());

	////////////////////////////////////////////////////////////////////////////
	// WHEN IN GAS, GO TO NEAREST REACHABLE SPOT OF UNGASSED LAND
	////////////////////////////////////////////////////////////////////////////

	// when in deep water, move to closest opponent
	if (ubCanMove && bInDeepWater && !pSoldier->aiBehavior().neutral() && pSoldier->aiBehavior().orders() == SEEKENEMY)
	{
		// find closest reachable opponent, excluding opponents in deep water
		pSoldier->aiPlanning().actionData() = ClosestReachableDisturbance(pSoldier, &fClimb);

		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
			return(AI_ACTION_LEAVE_WATER_GAS);
		}
	}

	if (ubCanMove && (bInGas || bInDeepWater || FindBombNearby(pSoldier, pSoldier->position().gridNo(), BOMB_DETECTION_RANGE) || RedSmokeDanger(pSoldier->position().gridNo(), pSoldier->position().level())))
	{
		pSoldier->aiPlanning().actionData() = FindNearestUngassedLand(pSoldier);
		
		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
#ifdef DEBUGDECISIONS
			sprintf(tempstr,"%s - SEEKING NEAREST UNGASSED LAND at grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
			AIPopMessage(tempstr);
#endif

			return(AI_ACTION_LEAVE_WATER_GAS);
		}
	}

	//if (fCivilian && !(pSoldier->identity().bodyType() == COW || pSoldier->identity().bodyType() == CRIPPLECIV || pSoldier->status().flags() & SOLDIER_VEHICLE) && gTacticalStatus.bBoxingState == NOT_BOXING)
	if (fCivilian && !(pSoldier->identity().bodyType() == COW || pSoldier->identity().bodyType() == CRIPPLECIV || pSoldier->status().flags() & SOLDIER_VEHICLE))
	{
		if (FindAIUsableObjClass(pSoldier, IC_WEAPON) == NO_SLOT)
		{
			// cower in fear!!
			if ( pSoldier->status().flags() & SOLDIER_COWERING )
			{
				if ( gfTurnBasedAI || gTacticalStatus.fEnemyInSector ) // battle!
				{
					// in battle!
					if ( pSoldier->aiPlanning().lastAction() == AI_ACTION_COWER )
					{
						// do nothing
						pSoldier->aiPlanning().actionData() = NOWHERE;
						return( AI_ACTION_NONE );
					}
					else
					{
						// set up next action to run away
						pSoldier->aiPlanning().nextActionData() = FindSpotMaxDistFromOpponents( pSoldier );
						if (!TileIsOutOfBounds(pSoldier->aiPlanning().nextActionData()))
						{
							pSoldier->aiPlanning().nextAction() = AI_ACTION_RUN_AWAY;
							pSoldier->aiPlanning().actionData() = ANIM_STAND;
							return( AI_ACTION_STOP_COWERING );
						}
						else
						{
							return( AI_ACTION_NONE );
						}
					}
				}
				else
				{
					if ( pSoldier->aiBehavior().newSituation() == NOT_NEW_SITUATION )
					{
						// stop cowering, not in battle, timer expired
						// we have to turn off whatever is necessary to stop status red...
						pSoldier->aiBehavior().alertStatus() = STATUS_GREEN;
						return( AI_ACTION_STOP_COWERING );
					}
					else
					{
						return( AI_ACTION_NONE );
					}
				}
			}
			else
			{
				if ( gfTurnBasedAI || gTacticalStatus.fEnemyInSector )
				{
					// battle - cower!!!
					pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
					return( AI_ACTION_COWER );
				}
				else // not in battle, cower for a certain length of time
				{
					pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
					pSoldier->aiPlanning().nextActionData() = (UINT16) REALTIME_CIV_AI_DELAY;
					pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
					return( AI_ACTION_COWER );
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////
	// IF POSSIBLE, FIRE LONG RANGE WEAPONS AT TARGETS REPORTED BY RADIO
	////////////////////////////////////////////////////////////////////////

	// can't do this in realtime, because the player could be shooting a gun or whatever at the same time!
	if (gfTurnBasedAI && 
		!fCivilian && 
		!bInWater && 
		!bInGas && 
		TacticalActorAiBehavior::hasInitialActionPoints(*pSoldier) &&
		!TacticalActorAiBehavior::isFlanking(*pSoldier) &&
		!(pSoldier->status().flags() & SOLDIER_BOXER) &&
		(CanNPCAttack(pSoldier) == TRUE))
	{
		BestThrow.ubPossible = FALSE;    // by default, assume Throwing isn't possible
		DebugAI(AI_MSG_TOPIC, pSoldier, String("[CheckIfTossPossible]"));
		CheckIfTossPossible(pSoldier,&BestThrow);

		if (BestThrow.ubPossible)
			DebugAI(AI_MSG_INFO, pSoldier, String("throw possible"));
		else
			DebugAI(AI_MSG_INFO, pSoldier, String("throw not possible"));

		if (BestThrow.ubPossible)
		{
			// sevenfm: allow using mortars, grenade launchers, flares and grenades in RED state
			UINT16 usItem = pSoldier->inventory()[BestThrow.bWeaponIn].usItem;
			if (ItemIsMortar(usItem) ||
				//Item[usItem].cannon ||
				ItemIsRocketLauncher(usItem) ||
				ItemIsGrenadeLauncher(usItem) ||
				ItemIsFlare(usItem) ||
				Item[usItem].usItemClass & IC_GRENADE)
			{
				// if firing mortar make sure we have room
				if (ItemIsMortar(usItem))
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("using mortar, check room to deploy"));
					ubOpponentDir = AIDirection(pSoldier->position().gridNo(), BestThrow.sTarget);

					// Get new gridno!
					sCheckGridNo = NewGridNo(pSoldier->position().gridNo(), DirectionInc(ubOpponentDir));

					if (!OKFallDirection(pSoldier, sCheckGridNo, pSoldier->position().level(), ubOpponentDir, pSoldier->animationPlayback().state()))
					{
						DebugAI(AI_MSG_INFO, pSoldier, String("no room to deploy mortar, check if we can move behind"));

						// can't fire!
						BestThrow.ubPossible = FALSE;

						// try behind us, see if there's room to move back
						sCheckGridNo = NewGridNo(pSoldier->position().gridNo(), DirectionInc(gOppositeDirection[ubOpponentDir]));
						if (OKFallDirection(pSoldier, sCheckGridNo, pSoldier->position().level(), gOppositeDirection[ubOpponentDir], pSoldier->animationPlayback().state()))
						{
							// sevenfm: check if we can reach this gridno
							INT32 iPathCost = EstimatePlotPath(pSoldier, sCheckGridNo, FALSE, FALSE, FALSE, DetermineMovementMode(pSoldier, AI_ACTION_GET_CLOSER), pSoldier->movement().stealthMode(), FALSE, 0);
							if (iPathCost != 0 && iPathCost + BestThrow.ubAPCost + GetAPsToLook(pSoldier) + GetAPsCrouch(pSoldier, FALSE) <= pSoldier->actionPoints().current())
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("moving backwards to have more room to deploy mortar"));
								pSoldier->aiPlanning().actionData() = sCheckGridNo;

								DebugAI(AI_MSG_INFO, pSoldier, String("prepare next action throw at spot %d level %d aimtime %d", BestThrow.sTarget, BestThrow.bTargetLevel, BestThrow.ubAimTime));

								// if necessary, swap the usItem
								if (BestThrow.bWeaponIn != HANDPOS)
								{
									DebugAI(AI_MSG_INFO, pSoldier, String("rearrange pocket"));
									RearrangePocket(pSoldier, HANDPOS, BestThrow.bWeaponIn, FOREVER);
								}

								pSoldier->aiPlanning().nextActionData() = BestThrow.sTarget;
								pSoldier->aiPlanning().nextTargetLevel() = BestThrow.bTargetLevel;
								pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;

								pSoldier->aiPlanning().nextAction() = AI_ACTION_TOSS_PROJECTILE;

								return AI_ACTION_GET_CLOSER;
							}
						}

						// can't fire!
						BestThrow.ubPossible = FALSE;
					}
				}

				// if still possible
				if (BestThrow.ubPossible)
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("prepare throw at spot %d level %d aimtime %d", BestThrow.sTarget, BestThrow.bTargetLevel, BestThrow.ubAimTime));

					// if necessary, swap the usItem
					if (BestThrow.bWeaponIn != HANDPOS)
					{
						DebugAI(AI_MSG_INFO, pSoldier, String("rearrange pocket"));
						RearrangePocket(pSoldier, HANDPOS, BestThrow.bWeaponIn, FOREVER);
					}

					// sevenfm: correctly set weapon mode for attached GL
					if (IsGrenadeLauncherAttached(&pSoldier->inventory()[HANDPOS]))
					{
						DebugAI(AI_MSG_INFO, pSoldier, String("set attached GL mode"));
						pSoldier->attackSelection().weaponMode() = WM_ATTACHED_GL;
					}

					// stand up before throwing if needed
					if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight < BestThrow.ubStance &&
						TacticalActorMobility::isValidStance(*pSoldier, AIDirection(pSoldier->position().gridNo(), BestThrow.sTarget), BestThrow.ubStance))
					{
						pSoldier->aiPlanning().actionData() = BestThrow.ubStance;
						pSoldier->aiPlanning().nextAction() = AI_ACTION_TOSS_PROJECTILE;
						pSoldier->aiPlanning().nextActionData() = BestThrow.sTarget;
						pSoldier->aiPlanning().nextTargetLevel() = BestThrow.bTargetLevel;
						pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
						return AI_ACTION_CHANGE_STANCE;
					}
					else
					{
						pSoldier->aiPlanning().actionData() = BestThrow.sTarget;
						pSoldier->targeting().level() = BestThrow.bTargetLevel;
						pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
					}

					return(AI_ACTION_TOSS_PROJECTILE);
				}
			}
		}
		else		// toss/throw/launch not possible
		{
			// WDS - Fix problem when there is no "best thrown" weapon (i.e., BestThrow.bWeaponIn == NO_SLOT)
			// if this dude has a longe-range weapon on him (longer than normal
			// sight range), and there's at least one other team-mate around, and
			// spotters haven't already been called for, then DO SO!

			if ((BestThrow.bWeaponIn != NO_SLOT) &&
				(CalcMaxTossRange(pSoldier, pSoldier->inventory()[BestThrow.bWeaponIn].usItem, TRUE) > TacticalActorVisibility::normalMaximumDistance()) &&
				(gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) &&
				(gTacticalStatus.ubSpottersCalledForBy == NOBODY))
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("throw not possible, call for spotters!"));

				// then call for spotters!  Uses up the rest of his turn (whatever
				// that may be), but from now on, BLACK AI NPC may radio sightings!
				gTacticalStatus.ubSpottersCalledForBy = pSoldier->identity().id();

				pSoldier->aiPlanning().actionData() = NOWHERE;
				return(AI_ACTION_NONE);
			}
		}

		// use smoke to cover friend
		DebugAI(AI_MSG_TOPIC, pSoldier, String("[use smoke to cover friend]"));
		if (gfTurnBasedAI &&
			SoldierAI(pSoldier) &&
			!bInWater &&
			!bInGas &&
			!TacticalActorAiBehavior::isFlanking(*pSoldier) &&
			TacticalActorAiBehavior::hasInitialActionPoints(*pSoldier) &&
			!pSoldier->suppression().underFire() &&
			SightCoverAtSpot(pSoldier, pSoldier->position().gridNo(), FALSE) &&
			!AICheckIsSniper(pSoldier) &&
			!AICheckIsMachinegunner(pSoldier) &&
			!AICheckIsMortarOperator(pSoldier) &&
			Chance(100 - min(100, 10 * CountPublicKnownEnemies(pSoldier, pSoldier->position().gridNo(), TACTICAL_RANGE))) &&
			!GuySawEnemy(pSoldier, SEEN_LAST_TURN))
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("check if we can cover friend with smoke"));

			CheckTossFriendSmoke(pSoldier, &BestThrow);

			if (BestThrow.ubPossible)
			{
				TacticalActor* bestThrowOpponent =
					GetJa2SoldierRepository().resolve(
						BestThrow.ubOpponent.i);
				DebugAI(AI_MSG_INFO, pSoldier, String("prepare throw at spot %d level %d aimtime %d", BestThrow.sTarget, BestThrow.bTargetLevel, BestThrow.ubAimTime));

				// start retreating for several turns
				if (bestThrowOpponent && !TacticalActorAiBehavior::isFlanking(*bestThrowOpponent))
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("start retreat counter for %d", BestThrow.ubOpponent));
					TacticalActorAiBehavior::startRetreat(*bestThrowOpponent, 2);
				}

				// if necessary, swap the usItem from holster into the hand position
				if (BestThrow.bWeaponIn != HANDPOS)
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("rearrange pocket"));
					RearrangePocket(pSoldier, HANDPOS, BestThrow.bWeaponIn, FOREVER);
				}

				// stand up before throwing if needed
				if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight < BestThrow.ubStance &&
					TacticalActorMobility::isValidStance(*pSoldier, AIDirection(pSoldier->position().gridNo(), BestThrow.sTarget), BestThrow.ubStance))
				{
					pSoldier->aiPlanning().actionData() = BestThrow.ubStance;
					pSoldier->aiPlanning().nextAction() = AI_ACTION_TOSS_PROJECTILE;
					pSoldier->aiPlanning().nextActionData() = BestThrow.sTarget;
					pSoldier->aiPlanning().nextTargetLevel() = BestThrow.bTargetLevel;
					pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
					return AI_ACTION_CHANGE_STANCE;
				}
				else
				{
					pSoldier->aiPlanning().actionData() = BestThrow.sTarget;
					pSoldier->targeting().level() = BestThrow.bTargetLevel;
					pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
				}

				DebugAI(AI_MSG_INFO, pSoldier, String("throw smoke grenade to cover friend %d at spot %d level %d", BestThrow.ubOpponent, BestThrow.sTarget, BestThrow.bTargetLevel));

				return(AI_ACTION_TOSS_PROJECTILE);
			}
		}

		// sevenfm: moved can attack check here as only sniper/suppression code needs usable gun
		if(CanNPCAttack(pSoldier) == TRUE)
		{
			// SNIPER!
			// sevenfm: set bAimShotLocation
			pSoldier->attackSelection().shotLocation() = AIM_SHOT_RANDOM;
			CheckIfShotPossible(pSoldier, &BestShot);
			TacticalActor* bestShotOpponent =
				GetJa2SoldierRepository().resolve(BestShot.ubOpponent.i);
			DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("decideactionred: is sniper shot possible? = %d, CTH = %d", BestShot.ubPossible, BestShot.ubChanceToReallyHit));

			if (BestShot.ubPossible && BestShot.ubChanceToReallyHit > 50)
			{
				// then do it!  The functions have already made sure that we have a
				// pair of worthy opponents, etc., so we're not just wasting our time

				// if necessary, swap the usItem from holster into the hand position
				DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "decideactionred: sniper shot possible!");
				if (BestShot.bWeaponIn != HANDPOS)
					RearrangePocket(pSoldier, HANDPOS, BestShot.bWeaponIn, FOREVER);

				pSoldier->aiPlanning().actionData() = BestShot.sTarget;
				//POSSIBLE STRUCTURE CHANGE PROBLEM. GOTTHARD 7/14/08
				pSoldier->aiPlanning().aimTime() = BestShot.ubAimTime;
				pSoldier->attackSelection().scopeMode() = BestShot.bScopeMode;
				// check if using sniper rifle
				if (Weapon[Item[pSoldier->inventory()[HANDPOS].usItem].ubClassIndex].ubWeaponType == GUN_SN_RIFLE)
					ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_SNIPER]);
				return(AI_ACTION_FIRE_GUN);
			}
			else		// snipe not possible
			{
				// if this dude has a long-range weapon on him (longer than normal
				// sight range), and there's at least one other team-mate around, and
				// spotters haven't already been called for, then DO SO!

				DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "decideactionred: sniper shot not possible");
				DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("decideactionred: weapon in slot #%d", BestShot.bWeaponIn));
				// WDS - Fix problem when there is no "best shot" weapon (i.e., BestShot.bWeaponIn == NO_SLOT)
				if (BestShot.bWeaponIn != NO_SLOT) {
					OBJECTTYPE * gun = &pSoldier->inventory()[BestShot.bWeaponIn];
					DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("decideactionred: men in sector %d, ubspotters called by %d, nobody %d", gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector, gTacticalStatus.ubSpottersCalledForBy, NOBODY));
					if (((IsScoped(gun) && GunRange(gun, pSoldier) > TacticalActorVisibility::normalMaximumDistance()) || pSoldier->aiBehavior().orders() == SNIPER) && // SANDRO - added argument
						(gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) &&
						(gTacticalStatus.ubSpottersCalledForBy == NOBODY))

					{
						// then call for spotters!  Uses up the rest of his turn (whatever
						// that may be), but from now on, BLACK AI NPC may radio sightings!
						gTacticalStatus.ubSpottersCalledForBy = pSoldier->identity().id();
						// HEADROCK HAM 3.1: This may be causing problems with HAM's lowered AP limit. From now on, we'll check
						// whether the soldier has more than 0 APs to begin with.
						if (pSoldier->actionPoints().current() > 0)
							pSoldier->actionPoints().current() = 0;

						DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "decideactionred: calling for sniper spotters");

						pSoldier->aiPlanning().actionData() = NOWHERE;
						return(AI_ACTION_NONE);
					}
				}
			}

			//SUPPRESSION FIRE			
			//CheckIfShotPossible(pSoldier, &BestShot);		//WarmSteel - No longer returns 0 when there IS actually a chance to hit.

			//RELOADING
			// WarmSteel - Because of suppression fire, we need enough ammo to even consider suppressing
			// This means we need to reload. Also reload if we're just plainly low on bullets.
			if (BestShot.bWeaponIn != NO_SLOT &&
				pSoldier->actionPoints().current() > APBPConstants[AP_MINIMUM] &&
				IsGunAutofireCapable(&pSoldier->inventory()[BestShot.bWeaponIn]) &&
				Weapon[pSoldier->inventory()[BestShot.bWeaponIn].usItem].swapClips &&
				(!pSoldier->suppression().underFire() && !GuySawEnemy(pSoldier, SEEN_LAST_TURN) && (TileIsOutOfBounds(sClosestOpponent) || PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent) > TACTICAL_RANGE / 2) || AICheckIsMachinegunner(pSoldier) && Chance(25) || Chance(10)) &&
				pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft < gGameExternalOptions.ubAISuppressionMinimumAmmo &&
				GetMagSize(&pSoldier->inventory()[BestShot.bWeaponIn]) >= gGameExternalOptions.ubAISuppressionMinimumMagSize)
				// || pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft < (UINT8)(GetMagSize(&pSoldier->inventory()[BestShot.bWeaponIn]) / 4)))
			{
				// HEADROCK HAM 5: Fixed an issue where no ammo was found, leading to a crash when overloading the
				// inventory vector (bAmmoSlot = -1...)
				INT8 bAmmoSlot = FindAmmoToReload(pSoldier, BestShot.bWeaponIn, NO_SLOT);
				if (bAmmoSlot > -1)
				{
					OBJECTTYPE * pAmmo = &(pSoldier->inventory()[bAmmoSlot]);
					if ((*pAmmo)[0]->data.ubShotsLeft > pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft && GetAPsToReloadGunWithAmmo(pSoldier, &(pSoldier->inventory()[BestShot.bWeaponIn]), pAmmo) <= (INT16)pSoldier->actionPoints().current())
					{
						pSoldier->aiPlanning().actionData() = BestShot.bWeaponIn;
						return AI_ACTION_RELOAD_GUN;
					}
				}
			}			

			// sevenfm: check that we have a clip to reload
			BOOLEAN fExtraClip = FALSE;
			if (BestShot.bWeaponIn != NO_SLOT)
			{
				INT8 bAmmoSlot = FindAmmoToReload(pSoldier, BestShot.bWeaponIn, NO_SLOT);
				if (bAmmoSlot != NO_SLOT)
				{
					fExtraClip = TRUE;
				}
			}

			// CHRISL: Changed from a simple flag to two externalized values for more modder control over AI suppression
			// WarmSteel - Don't *always* try to suppress when under 50 CTH
			if (BestShot.ubPossible &&
				BestShot.bWeaponIn != -1 &&
				// check valid target
				!TileIsOutOfBounds(BestShot.sTarget) &&
				bestShotOpponent &&
				Chance(100 - TacticalActorConditions::suppressionShockPercent(*bestShotOpponent) / 2) &&
				// check weapon/ammo requirements
				IsGunAutofireCapable(&pSoldier->inventory()[BestShot.bWeaponIn]) &&
				GetMagSize(&pSoldier->inventory()[BestShot.bWeaponIn]) >= gGameExternalOptions.ubAISuppressionMinimumMagSize &&
				pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft >= gGameExternalOptions.ubAISuppressionMinimumAmmo &&
				// check soldier and weapon
				pSoldier->aiBehavior().orders() != SNIPER &&
				BestShot.ubFriendlyFireChance <= MIN_CHANCE_TO_ACCIDENTALLY_HIT_SOMEONE &&
				!AICheckIsFlanking(pSoldier) &&
				(Chance(BestShot.ubChanceToReallyHit) || Chance(gGameExternalOptions.sSuppressionEffectiveness)) &&
				(!gGameExternalOptions.fAISafeSuppression || CheckSuppressionDirection(pSoldier, BestShot.sTarget, BestShot.bTargetLevel)) &&
				!TacticalActorAiBehavior::retreatCounter(*pSoldier) &&
				// check cover
				(fAnyCover ||																				// safe position
				!fCanBeSeen && NightLight() && CountFriendsFlankSameSpot(pSoldier, sClosestOpponent) && Chance(50) ||
				ARMED_VEHICLE(pSoldier) ||																		// tanks don't need cover
				ENEMYROBOT(pSoldier) || // robots don't try to be in cover
				pSoldier->suppression().underFire() && (pSoldier->combatResult().previousAttacker() == BestShot.ubOpponent || pSoldier->combatResult().earlierAttacker() == BestShot.ubOpponent || bestShotOpponent->targeting().lastGridNo() == pSoldier->position().gridNo()) ||	// return fire
				Chance((BestShot.ubChanceToReallyHit + 100) / 2) ||											// 50% chance to fire without cover
				//SoldierToSoldierLineOfSightTest(pSoldier, BestShot.ubOpponent, TRUE, CALC_FROM_ALL_DIRS)) &&		// can see target after turning
				LOS_Raised(pSoldier, bestShotOpponent, CALC_FROM_ALL_DIRS)) &&		// can see target after turning
				// reduce chance to shoot if target is beyond weapon range
				(AICheckIsMachinegunner(pSoldier) ||
				ARMED_VEHICLE(pSoldier) ||
				ENEMYROBOT(pSoldier) ||
				AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo()) ||
				pSoldier->suppression().underFire() && (pSoldier->combatResult().previousAttacker() == BestShot.ubOpponent || pSoldier->combatResult().earlierAttacker() == BestShot.ubOpponent || bestShotOpponent->targeting().lastGridNo() == pSoldier->position().gridNo()) ||	// return fire
				(PythSpacesAway(pSoldier->position().gridNo(), BestShot.sTarget) <= 0 || Chance(100 * (GunRange(&pSoldier->inventory()[BestShot.bWeaponIn], pSoldier) / CELL_X_SIZE) / PythSpacesAway(pSoldier->position().gridNo(), BestShot.sTarget)))) &&
				// check that we have spare ammo
				(fExtraClip || pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft >= gGameExternalOptions.ubAISuppressionMinimumMagSize))
			{
				// then do it!

				// if necessary, swap the usItem from holster into the hand position
				DebugAI(AI_MSG_INFO, pSoldier, String("suppression fire possible! target %d level %d aim %d", BestShot.sTarget, BestShot.bTargetLevel, BestShot.ubAimTime));

				if (BestShot.bWeaponIn != HANDPOS)
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("rearrange pocket"));
					RearrangePocket(pSoldier, HANDPOS, BestShot.bWeaponIn, FOREVER);
				}

				pSoldier->targeting().level() = BestShot.bTargetLevel;
				pSoldier->aiPlanning().aimTime() = BestShot.ubAimTime;
				pSoldier->fireControl().selectBurst();
				pSoldier->attackSelection().scopeMode() = BestShot.bScopeMode;

				INT16 ubBurstAPs = 0;
				FLOAT dTotalRecoil = 0;
				INT32 sActualAimAP;
				UINT8 ubAutoPenalty;
				INT16 sReserveAP = GetAPsProne(pSoldier, TRUE);
				UINT8 ubMinAuto = 5;

				if (BestShot.ubAimTime > 0 &&
					!UsingNewCTHSystem() &&
					Chance((100 - BestShot.ubChanceToReallyHit) * (100 - BestShot.ubChanceToReallyHit) / 100))
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("set ubAimTime = 0 for OCTH suppression"));
					BestShot.ubAimTime = 0;
				}

				// reserve APs to hide if no cover or enemy is close
				if (!AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo()) || PythSpacesAway(pSoldier->position().gridNo(), BestShot.sTarget) < TACTICAL_RANGE / 2)
				{
					sReserveAP = APBPConstants[AP_MINIMUM] / 2;
				}
				if (PythSpacesAway(pSoldier->position().gridNo(), BestShot.sTarget) > TACTICAL_RANGE || AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo()) || pSoldier->suppression().underFire())
				{
					ubMinAuto *= 2;
				}

				sActualAimAP = CalcAPCostForAiming(pSoldier, BestShot.sTarget, (INT8)pSoldier->aiPlanning().aimTime());

				if (UsingNewCTHSystem() == true)
				{
					do
					{
						pSoldier->fireControl().autofireShots()++;
						dTotalRecoil += AICalcRecoilForShot(pSoldier, &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots());
						ubBurstAPs = CalcAPsToAutofire(TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier);
					} while (pSoldier->actionPoints().current() >= BestShot.ubAPCost + sActualAimAP + ubBurstAPs + sReserveAP &&
						pSoldier->inventory()[pSoldier->attackSelection().hand()][0]->data.gun.ubGunShotsLeft >= pSoldier->fireControl().autofireShots() &&
						pSoldier->fireControl().autofireShots() <= 30 &&
						(dTotalRecoil <= 20.0f || pSoldier->fireControl().autofireShots() < ubMinAuto));
				}
				else
				{
					ubAutoPenalty = GetAutoPenalty(&pSoldier->inventory()[pSoldier->attackSelection().hand()], gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE);
					do
					{
						pSoldier->fireControl().autofireShots()++;
						ubBurstAPs = CalcAPsToAutofire(TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier);
					} while (pSoldier->actionPoints().current() >= BestShot.ubAPCost + sActualAimAP + ubBurstAPs + sReserveAP &&
						pSoldier->inventory()[pSoldier->attackSelection().hand()][0]->data.gun.ubGunShotsLeft >= pSoldier->fireControl().autofireShots() &&
						pSoldier->fireControl().autofireShots() <= 30 &&
						(ubAutoPenalty * pSoldier->fireControl().autofireShots() <= 80 || pSoldier->fireControl().autofireShots() < ubMinAuto));
				}

				pSoldier->fireControl().autofireShots()--;

				// Make sure we decided to fire at least one shot!
				ubBurstAPs = CalcAPsToAutofire(TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier);
				DebugAI(AI_MSG_INFO, pSoldier, String("autofire shots %d APcost %d burst AP %d aimtime %d reserve AP %d", pSoldier->fireControl().autofireShots(), BestShot.ubAPCost, ubBurstAPs, sActualAimAP, sReserveAP));

				// minimum 3 bullets
				if (pSoldier->fireControl().autofireShots() >= 3 && pSoldier->actionPoints().current() >= BestShot.ubAPCost + sActualAimAP + ubBurstAPs + sReserveAP)
				{
					if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight != BestShot.ubStance &&
						IsValidStance(pSoldier, BestShot.ubStance))
					{
						pSoldier->aiPlanning().nextAction() = AI_ACTION_FIRE_GUN;
						pSoldier->aiPlanning().nextActionData() = BestShot.sTarget;
						pSoldier->aiPlanning().nextTargetLevel() = BestShot.bTargetLevel;
						pSoldier->aiPlanning().actionData() = BestShot.ubStance;

						DebugAI(AI_MSG_INFO, pSoldier, String("Change stance before shooting"));

						// show "suppression fire" message only if opponent cannot be seen after turning
						if (!LOS_Raised(pSoldier, bestShotOpponent, CALC_FROM_ALL_DIRS))
							ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_SUPPRESSIONFIRE]);
						
						return(AI_ACTION_CHANGE_STANCE);
					}
					else
					{
						pSoldier->aiPlanning().actionData() = BestShot.sTarget;

						// show "suppression fire" message only if opponent cannot be seen after turning
						if (!LOS_Raised(pSoldier, bestShotOpponent, CALC_FROM_ALL_DIRS))
							ScreenMsg(FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_SUPPRESSIONFIRE]);

						return(AI_ACTION_FIRE_GUN);
					}
				}
				else
				{
					pSoldier->fireControl().selectSingleShot();
				}
			}
		}
		// suppression not possible, do something else

		// Flugente: trait skills
		// if we are a radio operator
		if (HAS_SKILL_TRAIT(pSoldier, RADIO_OPERATOR_NT) > 0 &&
			TacticalActorSkills::canUse(
				*pSoldier,
				SKILLS_RADIO_ARTILLERY,
				true))
		{
			UINT32 tmp;
			INT32 skilltargetgridno = 0;

			// call reinforcements if we haven't yet done so
			if (!gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition && MoreFriendsThanEnemiesinNearbysectors(pSoldier->roster().team(), pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ()))
			{
				// if frequencies are jammed...
				if (TacticalActorRadio::sectorJammed())
				{
					// if we are jamming, turn it off, otherwise, bad luck...
					if (TacticalActorRadio::isJamming(*pSoldier))
					{
						pSoldier->skillState().selectedAiSkill() = SKILLS_RADIO_TURNOFF;
						pSoldier->aiPlanning().actionData() = skilltargetgridno;
						return(AI_ACTION_USE_SKILL);
					}
				}
				// frequencies are clear, lets call for help
				else if (!(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT))
				{
					// raise alarm!
					return(AI_ACTION_RED_ALERT);
				}
			}
			// if we can't call in artillery, jam frequencies, so that the palyer can't use radio skills
			else if (!TacticalActorRadio::isJamming(*pSoldier) &&
					 !TacticalActorRadio::canOrderAnyArtilleryStrike(
						 *pSoldier,
						 &tmp))
			{
				pSoldier->skillState().selectedAiSkill() = SKILLS_RADIO_JAM;
				pSoldier->aiPlanning().actionData() = skilltargetgridno;
				return(AI_ACTION_USE_SKILL);
			}
		}
	}	

	/*
	// CALL IN AIR STRIKE & RADIO RED ALERT
	if ( !fCivilian && pSoldier->roster().team() != MILITIA_TEAM && gGameOptions.fAirStrikes && airstrikeavailable && (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) && !WillAirRaidBeStopped(pSoldier->deployment().sectorX(),pSoldier->deployment().sectorY()))
	{

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: checking to call in an air strike");

	iChance = Random(50);
	// if I ain't swimming (deep water)
	if ( !bInDeepWater )
	{
	// modify base chance according to orders
	switch (pSoldier->aiBehavior().orders())
	{
	case STATIONARY:       iChance +=  20;  break;
	case ONGUARD:          iChance +=  15;  break;
	case ONCALL:           iChance +=  10;  break;
	case CLOSEPATROL:                       break;
	case RNDPTPATROL:
	case POINTPATROL:      iChance +=  -5;  break;
	case FARPATROL:        iChance += -10;  break;
	case SEEKENEMY:        iChance += -20;  break;
	}

	// modify base chance according to attitude
	switch (pSoldier->aiBehavior().attitude())
	{
	case DEFENSIVE:        iChance +=  20;  break;
	case BRAVESOLO:        iChance += -10;  break;
	case BRAVEAID:                          break;
	case CUNNINGSOLO:      iChance +=  -5;  break;
	case CUNNINGAID:                        break;
	case AGGRESSIVE:       iChance += -20;  break;
	case ATTACKSLAYONLY:		iChance = 0;
	}

	// modify base chance according to morale
	switch (pSoldier->morale().aiMorale())
	{
	case MORALE_HOPELESS:  iChance *= 3;    break;
	case MORALE_WORRIED:   iChance *= 2;    break;
	case MORALE_NORMAL:                     break;
	case MORALE_CONFIDENT: iChance /= 2;    break;
	case MORALE_FEARLESS:  iChance /= 3;    break;
	}

	if ((INT16) Random(100) < iChance)
	{
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: decided to call in an air strike!");
	SayQuoteFromAnyBodyInSector( QUOTE_WEARY_SLASH_SUSPUCIOUS );
	EnemyCallInAirStrike ( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
	airstrikeavailable = FALSE;

	return(AI_ACTION_RED_ALERT);
	}
	}
	}
	*/

	if( gGameExternalOptions.bNewTacticalAIBehavior )
	{
		////////////////////////////////////////////////////////////////////////////
		// IF YOU SEE CAPTURED FRIENDS, FREE THEM!
		////////////////////////////////////////////////////////////////////////////

		// Flugente: if we see one of our buddies captured, it is a clear sign of enemy activity!
		if ( gGameExternalOptions.fAllowPrisonerSystem && pSoldier->roster().team() == ENEMY_TEAM )
		{
			SoldierID ubPerson = GetClosestFlaggedSoldierID( pSoldier, 20, ENEMY_TEAM, SOLDIER_POW, TRUE );
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				// if we are close, we can release this guy
				// possible only if not handcuffed (binders can be opened, handcuffs not)
				if ( !HasItemFlag( (&(person->inventory()[HANDPOS]))->usItem, HANDCUFFS ) )
				{
					if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) < 2 )
					{
						// see if we are facing this person
						UINT8 ubDesiredMercDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), person->position().gridNo());

						// if not already facing in that direction,
						if ( pSoldier->position().direction() != ubDesiredMercDir )
						{
							pSoldier->aiPlanning().actionData() = ubDesiredMercDir;

							return( AI_ACTION_CHANGE_FACING );
						}

						return(AI_ACTION_FREE_PRISONER);
					}
					else
					{
						pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
						if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
						{
							return(AI_ACTION_SEEK_FRIEND);
						}
					}
				}
			}
		}

		// if we are a doctor with medical gear, we might be able to help a wounded ally
		if (TacticalActorMedicalServices::
				canTreatForAi(*pSoldier))
		{
			SoldierID ubPerson = GetClosestWoundedSoldierID( pSoldier, gGameExternalOptions.sEnemyMedicsSearchRadius, pSoldier->roster().team());
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			// are we ourselves the patient?
			if ( ubPerson == pSoldier->identity().id() )
			{
				// if not already crouched, crouch down first
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_CROUCH && IsValidStance( pSoldier, ANIM_CROUCH ) && GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current() )
				{
					pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

					return(AI_ACTION_CHANGE_STANCE);
				}

				return(AI_ACTION_DOCTOR_SELF);
			}
			else if ( person )
			{
				if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) < 2 )
				{
					// see if we are facing this person
					UINT8 ubDesiredMercDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), person->position().gridNo());

					// if not already facing in that direction,
					if ( pSoldier->position().direction() != ubDesiredMercDir )
					{
						pSoldier->aiPlanning().actionData() = ubDesiredMercDir;

						return( AI_ACTION_CHANGE_FACING );
					}

					// if not already crouched, crouch down first
					if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_CROUCH && IsValidStance( pSoldier, ANIM_CROUCH ) && GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current() )
					{
						pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

						return(AI_ACTION_CHANGE_STANCE);
					}

					return(AI_ACTION_DOCTOR);
				}
				else
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
		// if we are not a medic, but are wounded, seek a medic
		else if ( pSoldier->vitals().healableInjury() >= gGameExternalOptions.sEnemyMedicsWoundMinAmount )
		{
			SoldierID ubPerson = GetClosestMedicSoldierID( pSoldier, gGameExternalOptions.sEnemyMedicsSearchRadius / 2, pSoldier->roster().team());
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				if ( PythSpacesAway(pSoldier->position().gridNo(), person->position().gridNo()) > 1 )
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0);
				
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}

		// VIPs run away (but not the GENERAL)
		if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_VIP && pSoldier->identity().profile() != GENERAL )
		{
			// this is in red AI state - a firefight is going on, we try to escape
			pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents( pSoldier );

			// if we don't know where our opponents are, we cannot run away from them...
			if ( TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
				// search for the closest map edge
				pSoldier->aiPlanning().actionData() = FindClosestExitGrid( pSoldier, pSoldier->position().gridNo(), 200 );
			}

			if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
				return AI_ACTION_RUN_AWAY;
			}
		}

		// are we a bodyguard?
		if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_BODYGUARD )
		{
			// is VIP still alive?
			SoldierID ubPerson = GetClosestFlaggedSoldierID( pSoldier, 100, pSoldier->roster().team(), SOLDIER_VIP, FALSE );
			TacticalActor* person =
				GetJa2SoldierRepository().resolve(ubPerson.i);

			if ( person )
			{
				// we want to stay close to him, but still be able to function properly... stay withing a 7-tile radius
				if ( SpacesAway( pSoldier->position().gridNo(), person->position().gridNo() ) > 7 )
				{
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, person->position().gridNo(), 20, AI_ACTION_SEEK_FRIEND, 0 );

					if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
					{
						return(AI_ACTION_SEEK_FRIEND);
					}
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////
	// RED RETREAT
	////////////////////////////////////////////////////////////////////////
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[retreat]"));
	if (gfTurnBasedAI &&
		!fCivilian &&
		!bInWater &&
		ubCanMove &&
		SoldierAI(pSoldier) &&
		pSoldier->aiBehavior().orders() != STATIONARY &&
		pSoldier->aiBehavior().orders() != SNIPER &&
		TacticalActorAiBehavior::retreatCounter(*pSoldier) > 0 &&
		(TacticalActorAiBehavior::hasInitialActionPoints(*pSoldier) || !fAnyCover || pSoldier->suppression().underFire()))
	{
		DebugAI(AI_MSG_TOPIC, pSoldier, String("search for retreat spot"));
		INT32 sRetreatSpot = FindRetreatSpot(pSoldier);

		if (!TileIsOutOfBounds(sRetreatSpot))
		{
			DebugAI(AI_MSG_TOPIC, pSoldier, String("found retreat spot %d", sRetreatSpot));

			//BeginMultiPurposeLocator(sRetreatSpot, pSoldier->position().level(), FALSE);

			pSoldier->aiPlanning().actionData() = sRetreatSpot;
			return(AI_ACTION_TAKE_COVER);
		}
	}

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: crouch and rest if running out of breath");
	////////////////////////////////////////////////////////////////////////
	// CROUCH & REST IF RUNNING OUT OF BREATH
	////////////////////////////////////////////////////////////////////////

	// if our breath is running a bit low, and we're not in water or under fire
	if ((pSoldier->vitals().breath() < 25) && !bInWater && !pSoldier->suppression().underFire())
	{
		// if not already crouched, try to crouch down first
		if (!fCivilian && !PTR_CROUCHED && IsValidStance( pSoldier, ANIM_CROUCH ) && gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_PRONE)
		{
#ifdef DEBUGDECISIONS
			sprintf(tempstr,"%s CROUCHES, NEEDING REST (STATUS RED), breath = %d",pSoldier->identity().name(),pSoldier->vitals().breath());
			AIPopMessage(tempstr);
#endif

			if (!gfTurnBasedAI || GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current())
			{
				pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

				return(AI_ACTION_CHANGE_STANCE);
			}
		}

#ifdef DEBUGDECISIONS
		sprintf(tempstr,"%s RESTS (STATUS RED), breath = %d",pSoldier->identity().name(),pSoldier->vitals().breath());
		AIPopMessage(tempstr);
#endif

		pSoldier->aiPlanning().actionData() = NOWHERE;

		// is it a heavy gun? And we have energy cost for shooting enabled? 
		if ( WeaponReady(pSoldier) && GetBPCostPer10APsForGunHolding( pSoldier ) > 0 )
		{
			// unready
			return(AI_ACTION_LOWER_GUN); 
		}
		return(AI_ACTION_NONE);
	}


	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: calculate morale");
	// calculate our morale
	pSoldier->morale().aiMorale() = CalcMorale(pSoldier);
// WDS DEBUG - this will make all enemies run away (to test retreating into occupied sector bugs)
//	pSoldier->morale().aiMorale() = MORALE_HOPELESS;

	// if a guy is feeling REALLY discouraged, he may continue to run like hell
	if ((pSoldier->morale().aiMorale() == MORALE_HOPELESS) && ubCanMove)
	{
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: run away");
		////////////////////////////////////////////////////////////////////////
		// RUN AWAY TO SPOT FARTHEST FROM KNOWN THREATS (ONLY IF MORALE HOPELESS)
		////////////////////////////////////////////////////////////////////////

		// look for best place to RUN AWAY to (farthest from the closest threat)
		pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents(pSoldier);
		
		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
#ifdef DEBUGDECISIONS
			sprintf(tempstr,"%s RUNNING AWAY to grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
			AIPopMessage(tempstr);
#endif

			return(AI_ACTION_RUN_AWAY);
		}
	}


	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: radio red alert?");
	////////////////////////////////////////////////////////////////////////////
	// RADIO RED ALERT: determine %chance to call others and report contact
	////////////////////////////////////////////////////////////////////////////

	// if we're a computer merc, and we have the action points remaining to RADIO
	// (we never want NPCs to choose to radio if they would have to wait a turn)
	if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && !fCivilian && (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) && (gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) )
	{

		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: checking to radio red alert");

		// if there hasn't been an initial RED ALERT yet in this sector
		if ( !(gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition) || NeedToRadioAboutPanicTrigger() )
			// since I'm at STATUS RED, I obviously know we're being invaded!
			iChance = gbDiff[DIFF_RADIO_RED_ALERT][ SoldierDifficultyLevel( pSoldier ) ];
		else // subsequent radioing (only to update enemy positions, request help)
			// base chance depends on how much new info we have to radio to the others
			iChance = 10 * WhatIKnowThatPublicDont(pSoldier,FALSE);  // use 10 * for RED alert

		// if I actually know something they don't and I ain't swimming (deep water)
		if (iChance && !bInDeepWater)
		{
			// modify base chance according to orders
			switch (pSoldier->aiBehavior().orders())
			{
			case STATIONARY:       iChance +=  20;  break;
			case ONGUARD:          iChance +=  15;  break;
			case ONCALL:           iChance +=  10;  break;
			case CLOSEPATROL:                       break;
			case RNDPTPATROL:
			case POINTPATROL:      iChance +=  -5;  break;
			case FARPATROL:        iChance += -10;  break;
			case SEEKENEMY:        iChance += -20;  break;
			case SNIPER:			  iChance += -10;  break; // Sniper contacts should be reported automatically
			}

			// modify base chance according to attitude
			switch (pSoldier->aiBehavior().attitude())
			{
			case DEFENSIVE:        iChance +=  20;  break;
			case BRAVESOLO:        iChance += -10;  break;
			case BRAVEAID:                          break;
			case CUNNINGSOLO:      iChance +=  -5;  break;
			case CUNNINGAID:                        break;
			case AGGRESSIVE:       iChance += -20;  break;
			case ATTACKSLAYONLY:		iChance = 0;
			}

			if ( (gTacticalStatus.fPanicFlags & PANIC_TRIGGERS_HERE) && !gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition)
			{
				// ignore morale (which could be really high
			}
			else
			{
				// modify base chance according to morale
				switch (pSoldier->morale().aiMorale())
				{
				case MORALE_HOPELESS:  iChance *= 3;    break;
				case MORALE_WORRIED:   iChance *= 2;    break;
				case MORALE_NORMAL:                     break;
				case MORALE_CONFIDENT: iChance /= 2;    break;
				case MORALE_FEARLESS:  iChance /= 3;    break;
				}
			}

#ifdef DEBUGDECISIONS
			AINumMessage("Chance to radio RED alert = ",iChance);
#endif

			if ((INT16) PreRandom(100) < iChance)
			{
#ifdef DEBUGDECISIONS
				AINameMessage(pSoldier,"decides to radio a RED alert!",1000);
#endif

				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: decided to radio red alert");
				return(AI_ACTION_RED_ALERT);
			}
		}
	}

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Self smoke when under fire]"));
	if (gfTurnBasedAI &&
		pSoldier->actionPoints().current() == pSoldier->actionPoints().initial() &&
		pSoldier->suppression().underFire() &&
		!InARoom(pSoldier->position().gridNo(), NULL) &&
		!InSmoke(pSoldier->position().gridNo(), pSoldier->position().level()) &&
		RangeChangeDesire(pSoldier) <= 2 &&
		(!NightLight() || InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level())) &&
		!TileIsOutOfBounds(sClosestOpponent) &&
		PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent) > TACTICAL_RANGE / 4 &&
		(!fProneSightCover && !AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo()) || TacticalActorConditions::hasTakenLargeHit(*pSoldier)) &&
		(TacticalActorConditions::hasTakenLargeHit(*pSoldier) || TacticalActorConditions::suppressionShockPercent(*pSoldier) > 20 + Random(80)))
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("check if soldier can cover himself with smoke"));

		CheckTossSelfSmoke(pSoldier, &BestThrow);

		if (BestThrow.ubPossible)
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("prepare throw at spot %d level %d aimtime %d", BestThrow.sTarget, BestThrow.bTargetLevel, BestThrow.ubAimTime));

			// start retreating for several turns
			TacticalActorAiBehavior::startRetreat(*pSoldier, 2);

			// if necessary, swap the usItem from holster into the hand position
			if (BestThrow.bWeaponIn != HANDPOS)
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("rearrange pocket"));
				RearrangePocket(pSoldier, HANDPOS, BestThrow.bWeaponIn, FOREVER);
			}

			// stand up before throwing if needed
			if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight < BestThrow.ubStance &&
				TacticalActorMobility::isValidStance(*pSoldier, AIDirection(pSoldier->position().gridNo(), BestThrow.sTarget), BestThrow.ubStance))
			{
				pSoldier->aiPlanning().actionData() = BestThrow.ubStance;
				pSoldier->aiPlanning().nextAction() = AI_ACTION_TOSS_PROJECTILE;
				pSoldier->aiPlanning().nextActionData() = BestThrow.sTarget;
				pSoldier->aiPlanning().nextTargetLevel() = BestThrow.bTargetLevel;
				pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
				return AI_ACTION_CHANGE_STANCE;
			}
			else
			{
				pSoldier->aiPlanning().actionData() = BestThrow.sTarget;
				pSoldier->targeting().level() = BestThrow.bTargetLevel;
				pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
			}

			return(AI_ACTION_TOSS_PROJECTILE);
		}
	}

	// sevenfm: no Main Red AI for civilians
	if ( (gGameExternalOptions.fEnemyTanksCanMoveInTactical || !ARMED_VEHICLE( pSoldier )) && 
		!(pSoldier->status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) &&
		!fCivilian)
	{
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: main red ai");

		// sevenfm: avoid light if spot is dangerous and no friends see my closest enemy
		if (ubCanMove &&
			InLightAtNight( pSoldier->position().gridNo(), pSoldier->position().level() ) &&
			pSoldier->aiBehavior().orders() != STATIONARY &&
			pSoldier->aiBehavior().orders() != SNIPER &&
			CountFriendsBlack(pSoldier) == 0 )
		{
			pSoldier->aiPlanning().actionData() = FindNearbyDarkerSpot( pSoldier );

			if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
			{
				// move as if leaving water or gas
				return( AI_ACTION_LEAVE_WATER_GAS );
			}
		}

		////////////////////////////////////////////////////////////////////////////
		// MAIN RED AI: Decide soldier's preference between SEEKING,HELPING & HIDING
		////////////////////////////////////////////////////////////////////////////

		// get the location of the closest reachable opponent
		sClosestDisturbance = ClosestReachableDisturbance(pSoldier, &fClimb);

		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: check to continue flanking");
		// continue flanking
		INT32 sFlankGridNo;
		
		if (TileIsOutOfBounds(sClosestDisturbance))
			sFlankGridNo = pSoldier->aiPlanning().flankAnchorGrid();
		else
			sFlankGridNo = sClosestDisturbance;

		// continue flanking
		// sevenfm: dont' flank when under fire
		if ( pSoldier->aiPlanning().flankCount() > 0 &&
			pSoldier->aiPlanning().flankCount() < MAX_FLANKS_RED  &&
			gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_PRONE &&
			!pSoldier->suppression().underFire() )
		{
			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: continue flanking");
			INT16 currDir = GetDirectionFromGridNo ( sFlankGridNo, pSoldier );
			INT16 origDir = pSoldier->aiPlanning().flankOriginDirection();
			pSoldier->aiPlanning().advanceFlank();
			if ( pSoldier->aiPlanning().lastFlankLeft() )
			{
				if ( origDir > currDir )
					origDir -= NUM_WORLD_DIRECTIONS;

				// stop flanking condition
				if ( (currDir - origDir) >= MinFlankDirections(pSoldier) )
				{
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
				else
				{
					pSoldier->aiPlanning().actionData() = FindFlankingSpot (pSoldier, sFlankGridNo , AI_ACTION_FLANK_LEFT);
					
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) ) //&& (currDir - origDir) < 2 )
						return AI_ACTION_FLANK_LEFT ;
					else
						pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
			}
			else
			{
				if ( origDir < currDir )
					origDir += NUM_WORLD_DIRECTIONS;

				// stop flanking condition
				if ( (origDir - currDir) >= MinFlankDirections(pSoldier) )
				{
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
				else
				{
					pSoldier->aiPlanning().actionData() = FindFlankingSpot (pSoldier, sFlankGridNo , AI_ACTION_FLANK_RIGHT);
					
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) )//&& (origDir - currDir) < 2 )
						return AI_ACTION_FLANK_RIGHT ;
					else
						pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
			}
		}

		// sevenfm: when we finished flanking, try to reach the flank anchor position
		// seek until we are close (DistanceVisible/2) and have line of sight to the flank anchor position
		// don't seek if we have seen enemy recently or under fire or have shock
		// don't seek if we have low AP (tired, wounded)
		if ( pSoldier->aiPlanning().flankCount() == MAX_FLANKS_RED )
		{
			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: stop flanking");

			// start end flank approach with full APs
			if( gfTurnBasedAI && pSoldier->actionPoints().current() < pSoldier->actionPoints().initial() )
			{
				return(AI_ACTION_END_TURN);
			}

			if( !TileIsOutOfBounds(sFlankGridNo) &&
				!GuySawEnemy(pSoldier) &&
				!pSoldier->suppression().underFire() &&
				!Water(pSoldier->position().gridNo(), pSoldier->position().level()) &&
				pSoldier->actionPoints().initial() >= APBPConstants[AP_MINIMUM] &&
				( PythSpacesAway( pSoldier->position().gridNo(), sFlankGridNo ) > MIN_FLANK_DIST_RED ||
				!LocationToLocationLineOfSightTest( pSoldier->position().gridNo(), pSoldier->position().level(), sFlankGridNo, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS) ) )
			{				
				pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier,sFlankGridNo,GetAPsCrouch( pSoldier, TRUE),AI_ACTION_SEEK_OPPONENT,0);

				// sevenfm: avoid going into water, gas or light
				if( !TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) &&
					!Water(pSoldier->aiPlanning().actionData(), pSoldier->position().level()) &&
					!InGas( pSoldier, pSoldier->aiPlanning().actionData() ) &&
					!InLightAtNight( pSoldier->aiPlanning().actionData(), pSoldier->position().level() ) )
				{
					// if soldier can be seen at new position and he cannot be seen at his current position
					if ( LocationToLocationLineOfSightTest( pSoldier->aiPlanning().actionData(), pSoldier->position().level(), sFlankGridNo, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS) &&
						!LocationToLocationLineOfSightTest( pSoldier->position().gridNo(), pSoldier->position().level(), sFlankGridNo, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS) )
					{
						// reserve APs for a possible crouch plus a shot
						INT32 sCautiousGridNo = InternalGoAsFarAsPossibleTowards(pSoldier, sFlankGridNo, (INT8) (MinAPsToAttack( pSoldier, sFlankGridNo, ADDTURNCOST,0) + GetAPsCrouch( pSoldier, TRUE) + GetAPsToLook(pSoldier)), AI_ACTION_SEEK_OPPONENT, FLAG_CAUTIOUS );

						if (!TileIsOutOfBounds(sCautiousGridNo))
						{
							pSoldier->aiPlanning().actionData() = sCautiousGridNo;
							pSoldier->aiBehavior().flags() |= AI_CAUTIOUS;
							pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
							return(AI_ACTION_SEEK_OPPONENT);
						}
						return(AI_ACTION_SEEK_OPPONENT);
					}
					else
					{
						return(AI_ACTION_SEEK_OPPONENT);
					}
				}
				else
				{
					// if we cannot advance to spot, stop trying
					pSoldier->aiPlanning().advanceFlank();
				}
			}
			else
			{	
				// stop
				pSoldier->aiPlanning().advanceFlank();
			}
		}

		DebugAI(AI_MSG_TOPIC, pSoldier, String("[Set watched location]"));
		if (TacticalActorAiBehavior::hasInitialActionPoints(*pSoldier) &&
			pSoldier->actionPoints().current() >= APBPConstants[AP_MINIMUM] &&
			gfTurnBasedAI &&
			pSoldier->position().level() == 0 &&
			!pSoldier->suppression().underFire() &&
			!InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level()) &&
			SightCoverAtSpot(pSoldier, pSoldier->position().gridNo(), TRUE) &&
			!GuySawEnemy(pSoldier) &&
			!TileIsOutOfBounds(sClosestDisturbance) &&
			//!fSeekClimb &&
			PythSpacesAway(pSoldier->position().gridNo(), sClosestDisturbance) < TACTICAL_RANGE &&
			(pSoldier->aiBehavior().orders() == STATIONARY || pSoldier->aiBehavior().orders() == SNIPER || RangeChangeDesire(pSoldier) < 4) &&
			!SoldierToVirtualSoldierLineOfSightTest(pSoldier, sClosestDisturbance, pSoldier->position().level(), ANIM_STAND, TRUE, CALC_FROM_ALL_DIRS) &&
			CountFriendsBlack(pSoldier, sClosestDisturbance) == 0)
		{
			gubNPCAPBudget = 0;
			gubNPCDistLimit = 0;

			// check path to closest disturbance and find the point where enemy will appear in sight						
			if (FindBestPath(pSoldier, sClosestDisturbance, pSoldier->position().level(), RUNNING, COPYROUTE, PATH_IGNORE_PERSON_AT_DEST | PATH_THROUGH_PEOPLE))
			{
				INT16 sLoop;
				INT32 sLastSeenSpot = NOWHERE;

				DebugAI(AI_MSG_INFO, pSoldier, String("found path to %d, path size %d ", sClosestDisturbance, pSoldier->pathing().pathSize()));
				DebugAI(AI_MSG_INFO, pSoldier, String("check path for seen spots"));

				sCheckGridNo = pSoldier->position().gridNo();

				for (sLoop = pSoldier->pathing().pathIndex(); sLoop < pSoldier->pathing().pathSize(); sLoop++)
				{
					sCheckGridNo = NewGridNo(sCheckGridNo, DirectionInc((UINT8)(pSoldier->pathing().path()[sLoop])));

					if (SoldierToVirtualSoldierLineOfSightTest(pSoldier, sCheckGridNo, pSoldier->position().level(), ANIM_STAND, TRUE, CALC_FROM_ALL_DIRS))
					{
						sLastSeenSpot = sCheckGridNo;
					}
				}

				// if found last seen spot
				if (!TileIsOutOfBounds(sLastSeenSpot))
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("last seen spot %d level %d", sLastSeenSpot, pSoldier->position().level()));
					IncrementWatchedLoc(pSoldier->identity().id(), sLastSeenSpot, pSoldier->position().level());
				}
			}
			gubNPCAPBudget = 0;
		}

		// if we can move at least 1 square's worth
		// and have more APs than we want to reserve
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("decideactionred: can we move? = %d, APs = %d",ubCanMove,pSoldier->actionPoints().current()));

		if (ubCanMove && pSoldier->actionPoints().current() > APBPConstants[MAX_AP_CARRIED])
		{
			DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("decideactionred: checking hide/seek/help/watch points... orders = %d, attitude = %d", pSoldier->aiBehavior().orders(), pSoldier->aiBehavior().attitude()));
			// calculate initial points for watch based on highest watch loc

			bWatchPts = GetHighestWatchedLocPoints(pSoldier->identity().id());
			if (bWatchPts <= 0)
			{
				// no watching
				bWatchPts = -99;
			}

			// modify RED movement tendencies according to morale
			switch (pSoldier->morale().aiMorale())
			{
			case MORALE_HOPELESS:  bSeekPts = -99; bHelpPts = -99; bHidePts += +2; bWatchPts = -99; break;
			case MORALE_WORRIED:   bSeekPts += -2; bHelpPts += 0; bHidePts += +2; bWatchPts += 1; break;
			case MORALE_NORMAL:    bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
			case MORALE_CONFIDENT: bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += 0; break;
			case MORALE_FEARLESS:  bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += 0; break;
			}

			// modify tendencies according to orders
			switch (pSoldier->aiBehavior().orders())
			{
			case STATIONARY:   bSeekPts += -1; bHelpPts += -1; bHidePts += +1; bWatchPts += +1; break;
			case ONGUARD:      bSeekPts += -1; bHelpPts += 0; bHidePts += +1; bWatchPts += +1; break;
			case CLOSEPATROL:  bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
			case RNDPTPATROL:  bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
			case POINTPATROL:  bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
			case FARPATROL:    bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
			case ONCALL:       bSeekPts += 0; bHelpPts += +1; bHidePts += -1; bWatchPts += 0; break;
			case SEEKENEMY:    bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += -1; break;
			case SNIPER:		bSeekPts += -1; bHelpPts += 0; bHidePts += +1; bWatchPts += +1; break;
			}

			// modify tendencies according to attitude
			switch (pSoldier->aiBehavior().attitude())
			{
			case DEFENSIVE:     bSeekPts += -1; bHelpPts += 0; bHidePts += +2; bWatchPts += +1; break;
			case BRAVESOLO:     bSeekPts += +1; bHelpPts += -1; bHidePts += -1; bWatchPts += -1; break;
			case BRAVEAID:      bSeekPts += +1; bHelpPts += +1; bHidePts += -1; bWatchPts += -1; break;
			case CUNNINGSOLO:   bSeekPts += 1; bHelpPts += -1; bHidePts += +1; bWatchPts += 0; break;
			case CUNNINGAID:    bSeekPts += 1; bHelpPts += +1; bHidePts += +1; bWatchPts += 0; break;
			case AGGRESSIVE:    bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += 0; break;
			case ATTACKSLAYONLY:bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += 0; break;
			}

			// sevenfm: snipers and soldiers with scoped guns should decide watch more often
			if (AIGunScoped(pSoldier) || AICheckIsSniper(pSoldier))
			{
				bWatchPts++;
			}

			// sevenfm: disable watching if soldier is under fire or in dangerous place
			// don't watch if some friends can see my closest opponent
			if (fDangerousSpot ||
				InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level()) ||
				CountFriendsBlack(pSoldier) > 0)
			{
				// prefer hiding when in dangerous place
				if (bHidePts > -90)
					bWatchPts = min(bWatchPts, bHidePts - 1);
				else
					bWatchPts--;
			}

			// sevenfm: don't watch when overcrowded and not in a building
			if (!InARoom(pSoldier->position().gridNo(), NULL))
			{
				bWatchPts -= CountNearbyFriends(pSoldier, pSoldier->position().gridNo(), TACTICAL_RANGE / 8);
			}

			// sevenfm: don't help if seen enemy recently or under fire
			if (GuySawEnemy(pSoldier) || pSoldier->suppression().underFire())
			{
				bHelpPts -= 10;
			}

			if (TacticalActorAiBehavior::retreatCounter(*pSoldier) > 0)
			{
				// no seeking when retreating
				bSeekPts = -99;
				// no helping when retreating
				bHelpPts = -99;

				if (bHidePts > -90)
				{
					bWatchPts = min(bWatchPts, bHidePts - 1);
				}
			}

			if (!gfTurnBasedAI)
			{
				// don't search for cover
				bHidePts = -99;
			}

			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("decideactionred: hide = %d, seek = %d, watch = %d, help = %d",bHidePts,bSeekPts,bWatchPts,bHelpPts));
			// while one of the three main RED REACTIONS remains viable
			while ((bSeekPts > -90) || (bHelpPts > -90) || (bHidePts > -90) )
			{
				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: checking to seek");
				// if SEEKING is possible and at least as desirable as helping or hiding
				if ( ((bSeekPts > -90) && (bSeekPts >= bHelpPts) && (bSeekPts >= bHidePts) && (bSeekPts >= bWatchPts )) )
				{
#ifdef AI_TIMING_TESTS
					uiStartTime = GetJA2Clock();
#endif

#ifdef AI_TIMING_TESTS
					uiEndTime = GetJA2Clock();
					guiRedSeekTimeTotal += (uiEndTime - uiStartTime);
					guiRedSeekCounter++;
#endif
					// if there is an opponent reachable					
					// sevenfm: allow seeking in prone stance if we haven't seen enemy for several turns
					if (!TileIsOutOfBounds(sClosestDisturbance) &&
						 (gAnimControl[pSoldier->animationPlayback().state()].ubHeight != ANIM_PRONE || !GuySawEnemy( pSoldier )) )
					{
						DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: seek opponent");
						//////////////////////////////////////////////////////////////////////
						// SEEK CLOSEST DISTURBANCE: GO DIRECTLY TOWARDS CLOSEST KNOWN OPPONENT
						//////////////////////////////////////////////////////////////////////

						// try to move towards him
						pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier,sClosestDisturbance,GetAPsCrouch( pSoldier, TRUE),AI_ACTION_SEEK_OPPONENT,0);
						
						if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
						{
							// Check for a trap
							if ( !ArmySeesOpponents() )
							{
								if ( GetNearestRottingCorpseAIWarning( pSoldier->aiPlanning().actionData() ) > 0 )
								{
									// abort! abort!
									pSoldier->aiPlanning().actionData() = NOWHERE;
								}
							}
						}

						// if it's possible						
						if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
						{
#ifdef DEBUGDECISIONS
							// do it!
							sprintf(tempstr,"%s - SEEKING OPPONENT at grid %d, MOVING to %d",
								pSoldier->identity().name(),sClosestDisturbance,pSoldier->aiPlanning().actionData());
							AIPopMessage(tempstr);
#endif

							if (!ENEMYROBOT(pSoldier) && fClimb)//&& pSoldier->aiPlanning().actionData() == sClosestDisturbance)
							{
								// need to climb AND have enough APs to get there this turn
								BOOLEAN fUp = TRUE;
								if (pSoldier->position().level() > 0 )
									fUp = FALSE;

								if (!fUp)
									DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("Soldier %d is climbing down",pSoldier->identity().id()) );

								// As mentioned in the next part, the sClosestDisturbance IS the climb point desired.  So the
								// check here should be "Am I aready there?"  If so, THEN possibly climb.  This previous check
								// would have a soldier climbing any building, even if it was not the desired building.  So
								// WRONG WRONG WRONG
								//if ( CanClimbFromHere ( pSoldier, fUp ) )
								if (pSoldier->position().gridNo() == sClosestDisturbance)
								{
									if (IsActionAffordable(pSoldier) && pSoldier->actionPoints().current() >= ( APBPConstants[AP_CLIMBROOF] + MinAPsToAttack( pSoldier, sClosestDisturbance, ADDTURNCOST,0)))
									{
										return( AI_ACTION_CLIMB_ROOF );
									}
								}
								else
								{
									// Do not overwrite the usActionData here.  If there's no nearby climb point, the action data
									// would become NOWHERE, and then the SEEK_ENEMY fallback would also fail.
									// In fact, sClosestDisturbance has ALREADY calculated the closest climb point when climbing is
									// necessary.  The returned grid # in sClosestDisturbance is that climb point.  So if climb is 
									// set, then use sClosestDisturbance as is.
									//INT16 usClimbPoint = FindClosestClimbPoint(pSoldier, pSoldier->sGridNo , sClosestDisturbance , fUp );
									INT32 usClimbPoint = sClosestDisturbance;									
									if (!TileIsOutOfBounds(usClimbPoint))
									{
										pSoldier->aiPlanning().actionData() = usClimbPoint;
										return( AI_ACTION_MOVE_TO_CLIMB  );
									}
								}
							}
							//if ( fClimb && pSoldier->aiPlanning().actionData() == sClosestDisturbance)
							//{
							//	return( AI_ACTION_CLIMB_ROOF );
							//}

							BOOLEAN fOvercrowded = FALSE;
							if( CountNearbyFriends(pSoldier, pSoldier->position().gridNo(), TACTICAL_RANGE / 4) > 2 )
							{
								fOvercrowded = TRUE;
							}
							
							// sevenfm: possibly start RED flanking
							if (( pSoldier->aiBehavior().attitude() == CUNNINGAID || pSoldier->aiBehavior().attitude() == CUNNINGSOLO ||
								( pSoldier->aiBehavior().attitude() == BRAVESOLO || pSoldier->aiBehavior().attitude() == BRAVEAID ) && fOvercrowded ) &&
								pSoldier->roster().team() == ENEMY_TEAM &&
								gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight != ANIM_PRONE &&
								!pSoldier->suppression().underFire() &&
								pSoldier->position().level() == 0 &&
								( pSoldier->aiBehavior().orders() == SEEKENEMY ||
								pSoldier->aiBehavior().orders() == FARPATROL ||
								pSoldier->aiBehavior().orders() == CLOSEPATROL && NightTime() ) &&
								(!GuySawEnemy( pSoldier ) || fOvercrowded ) &&
								!Water(pSoldier->position().gridNo(), pSoldier->position().level()) &&
								pSoldier->actionPoints().current() >= APBPConstants[AP_MINIMUM] &&
								( CountFriendsInDirection( pSoldier, sClosestDisturbance ) > 1 || NightTime() || fOvercrowded) )
							{
								INT8 action = AI_ACTION_SEEK_OPPONENT;
								INT16 dist = PythSpacesAway ( pSoldier->position().gridNo(), sClosestDisturbance );
								if ( dist > MIN_FLANK_DIST_RED  && dist < MAX_FLANK_DIST_RED )
								{
									INT16 rdm = Random(6);

									switch (rdm)
									{
									case 1:
									case 2:
									case 3:
										if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
											action = AI_ACTION_FLANK_LEFT ;
										break;
									default:
										if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
											action = AI_ACTION_FLANK_RIGHT ;
										break;
									}

									if (action == AI_ACTION_SEEK_OPPONENT) {
										return action;
									}
								}
								else
									return AI_ACTION_SEEK_OPPONENT ;

								pSoldier->aiPlanning().actionData() = FindFlankingSpot (pSoldier, sClosestDisturbance, action );
								
								if (TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) || pSoldier->aiPlanning().flankCount() >= MAX_FLANKS_RED )
								{
									pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier,sClosestDisturbance,GetAPsCrouch( pSoldier, TRUE), AI_ACTION_SEEK_OPPONENT,0);
									//pSoldier->aiPlanning().clearFlank();
									if ( PythSpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) < 5 || LocationToLocationLineOfSightTest( pSoldier->aiPlanning().actionData(), pSoldier->position().level(), sClosestDisturbance, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS ) )
									{
										// reserve APs for a possible crouch plus a shot
										pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, sClosestDisturbance, (INT8) (MinAPsToAttack( pSoldier, sClosestDisturbance, ADDTURNCOST,0) + GetAPsCrouch( pSoldier, TRUE)), AI_ACTION_SEEK_OPPONENT, FLAG_CAUTIOUS );
										
										if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
										{
											pSoldier->aiBehavior().flags() |= AI_CAUTIOUS;
											pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
											return(AI_ACTION_SEEK_OPPONENT);
										}
									}

									else
									{
										return(AI_ACTION_SEEK_OPPONENT);
									}
								}
								else
								{
									if ( action == AI_ACTION_FLANK_LEFT )
										pSoldier->aiPlanning().lastFlankLeft() = TRUE;
									else
										pSoldier->aiPlanning().lastFlankLeft() = FALSE;

									pSoldier->aiPlanning().recordFlankStep(
										sClosestDisturbance,
										GetDirectionFromGridNo( sClosestDisturbance, pSoldier ) );

									// sevenfm: change orders when starting to flank
									if( pSoldier->aiBehavior().orders() == CLOSEPATROL )
									{
										pSoldier->aiBehavior().orders() = FARPATROL;
									}

									return(action);
								}
							}
							else
							{
								// let's be a bit cautious about going right up to a location without enough APs to shoot
								if ( PythSpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) < 5 || LocationToLocationLineOfSightTest( pSoldier->aiPlanning().actionData(), pSoldier->position().level(), sClosestDisturbance, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS ) )
								{
									// reserve APs for a possible crouch plus a shot
									pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, sClosestDisturbance, (INT8) (MinAPsToAttack( pSoldier, sClosestDisturbance, ADDTURNCOST,0) + GetAPsCrouch( pSoldier, TRUE)), AI_ACTION_SEEK_OPPONENT, FLAG_CAUTIOUS );
									
									if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
									{
										pSoldier->aiBehavior().flags() |= AI_CAUTIOUS;
										pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
										return(AI_ACTION_SEEK_OPPONENT);
									}
								}
								else
								{
									return(AI_ACTION_SEEK_OPPONENT);
								}
								break;
							}
						}
					}

					// mark SEEKING as impossible for next time through while loop
#ifdef DEBUGDECISIONS
					AINameMessage(pSoldier,"couldn't SEEK...",1000);
#endif
					bSeekPts = -99;
					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: couldn't seek");
				}

				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: checking to watch");
				// if WATCHING is possible and at least as desirable as anything else
				if ((bWatchPts > -90) && (bWatchPts >= bSeekPts) && (bWatchPts >= bHelpPts) && (bWatchPts >= bHidePts ))
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("[watch]"));
					// take a look at our highest watch point... if it's still visible, turn to face it and then wait
					bHighestWatchLoc = GetHighestVisibleWatchedLoc( pSoldier->identity().id() );

					if ( bHighestWatchLoc != -1 )
					{
						// see if we need turn to face that location
						ubOpponentDir = AIDirection(pSoldier->position().gridNo(), gsWatchedLoc[pSoldier->identity().id()][bHighestWatchLoc]);
						DebugAI(AI_MSG_INFO, pSoldier, String("Highest watch location: [%d] %d %d watch dir: %d", bHighestWatchLoc, gsWatchedLoc[pSoldier->identity().id()][bHighestWatchLoc], gbWatchedLocLevel[pSoldier->identity().id()][bHighestWatchLoc], ubOpponentDir));

						// consider at least crouching
						if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND &&
							IsValidStance(pSoldier, ANIM_CROUCH) &&
							pSoldier->actionPoints().current() >= GetAPsCrouch(pSoldier, TRUE))
						{
							pSoldier->aiPlanning().actionData() = ANIM_CROUCH;

							DebugAI(AI_MSG_INFO, pSoldier, String("crouch to watch"));
							return(AI_ACTION_CHANGE_STANCE);
						}

						// raise weapon if not raised
						if (TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION &&
							!WeaponReady(pSoldier) &&
							(pSoldier->vitals().breath() > OKBREATH * 2 || GetBPCostPer10APsForGunHolding(pSoldier, TRUE) < 50) &&
							pSoldier->actionPoints().current() >= GetAPsToReadyWeapon(pSoldier, TacticalActorAnimationSelection::pickReady(*pSoldier, false, false)))
						{
							DebugAI(AI_MSG_INFO, pSoldier, String("raise weapon"));
							return AI_ACTION_RAISE_GUN;
						}

						// if soldier is not already facing in that direction
						if (pSoldier->position().direction() != ubOpponentDir &&
							TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubOpponentDir) &&
							pSoldier->actionPoints().current() >= GetAPsToLook(pSoldier))
						{
							// turn
							pSoldier->aiPlanning().actionData() = ubOpponentDir;
							DebugAI(AI_MSG_INFO, pSoldier, String("turn to watched location"));
							return(AI_ACTION_CHANGE_FACING);
						}

						// possibly go prone, check that we'll have line of sight to standing enemy at watched location
						if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_CROUCH &&
							IsValidStance(pSoldier, ANIM_PRONE) &&
							pSoldier->actionPoints().current() >= GetAPsProne(pSoldier, TRUE) &&
							(!InARoom(pSoldier->position().gridNo(), NULL) || pSoldier->position().level() > 0 || pSoldier->suppression().underFire()) &&
							gfTurnBasedAI &&
							LocationToLocationLineOfSightTest(pSoldier->position().gridNo(), pSoldier->position().level(), gsWatchedLoc[pSoldier->identity().id()][bHighestWatchLoc], gbWatchedLocLevel[pSoldier->identity().id()][bHighestWatchLoc], TRUE, TacticalActorVisibility::maximumDistance(*pSoldier, gsWatchedLoc[pSoldier->identity().id()][bHighestWatchLoc], gbWatchedLocLevel[pSoldier->identity().id()][bHighestWatchLoc], CALC_FROM_ALL_DIRS), PRONE_LOS_POS, STANDING_LOS_POS))
						{
							pSoldier->aiPlanning().actionData() = ANIM_PRONE;
							pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
							DebugAI(AI_MSG_INFO, pSoldier, String("go prone, end turn"));
							return(AI_ACTION_CHANGE_STANCE);
						}

						DebugAI(AI_MSG_INFO, pSoldier, String("watch at %d level %d", gsWatchedLoc[pSoldier->identity().id()][bHighestWatchLoc], gbWatchedLocLevel[pSoldier->identity().id()][bHighestWatchLoc]));
						return(AI_ACTION_NONE);
					}

					bWatchPts = -99;
					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: couldn't watch");
				}


				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: checking to help");
				// if HELPING is possible and at least as desirable as seeking or hiding
				if ((bHelpPts > -90) && (bHelpPts >= bSeekPts) && (bHelpPts >= bHidePts) && (bHelpPts >= bWatchPts ))
				{
#ifdef AI_TIMING_TESTS
					uiStartTime = GetJA2Clock();
#endif
					sClosestFriend = ClosestReachableFriendInTrouble(pSoldier, &fClimb );
#ifdef AI_TIMING_TESTS
					uiEndTime = GetJA2Clock();

					guiRedHelpTimeTotal += (uiEndTime - uiStartTime);
					guiRedHelpCounter++;
#endif
					//WarmSteel - Dont try if we're already quite close to our friend
					// sevenfm: reverted to vanilla helping
					//if (!TileIsOutOfBounds(sClosestFriend) && PythSpacesAway(pSoldier->sGridNo, sClosestFriend) > TacticalActorVisibility::maximumDistance(*pSoldier, sClosestFriend, 0, CALC_FROM_ALL_DIRS ))
					if (!TileIsOutOfBounds(sClosestFriend))
					{
						//////////////////////////////////////////////////////////////////////
						// GO DIRECTLY TOWARDS CLOSEST FRIEND UNDER FIRE OR WHO LAST RADIOED
						//////////////////////////////////////////////////////////////////////
						pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier,sClosestFriend,GetAPsCrouch( pSoldier, TRUE), AI_ACTION_SEEK_OPPONENT,0);
						
						if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
						{
#ifdef DEBUGDECISIONS
							sprintf(tempstr,"%s - SEEKING FRIEND at %d, MOVING to %d",
								pSoldier->identity().name(),sClosestFriend,pSoldier->aiPlanning().actionData());
							AIPopMessage(tempstr);
#endif

							if ( !ENEMYROBOT(pSoldier) && fClimb )//&& pSoldier->aiPlanning().actionData() == sClosestFriend)
							{
								// need to climb AND have enough APs to get there this turn
								BOOLEAN fUp = TRUE;
								if (pSoldier->position().level() > 0 )
									fUp = FALSE;

								if (!fUp)
									DebugMsg ( TOPIC_JA2AI , DBG_LEVEL_3 , String("Soldier %d is climbing down",pSoldier->identity().id()) );

								// 0verhaul:  Yet another chance to climb the wrong building and otherwise waste CPU power.
								// We already know the climb point we want, which may not be here even if climbing is possible.
								//if ( CanClimbFromHere ( pSoldier, fUp ) )
								if (pSoldier->position().gridNo() == sClosestFriend)
								{
									if (IsActionAffordable(pSoldier) && pSoldier->actionPoints().current() >= ( APBPConstants[AP_CLIMBROOF] + MinAPsToAttack( pSoldier, sClosestFriend, ADDTURNCOST,0)))
									{
										return( AI_ACTION_CLIMB_ROOF );
									}
								}
								else
								{
									pSoldier->aiPlanning().actionData() = sClosestFriend;
									//INT32 sClimbPoint = FindClosestClimbPoint(pSoldier, pSoldier->sGridNo , sClosestFriend , fUp );									
									//if (!TileIsOutOfBounds(sClimbPoint))
									{
										//pSoldier->aiPlanning().actionData() = sClimbPoint;
										return( AI_ACTION_MOVE_TO_CLIMB  );
									}
								}
							}
							//if (fClimb && pSoldier->aiPlanning().actionData() == sClosestFriend)
							//{
							// return( AI_ACTION_CLIMB_ROOF );
							//}
							return(AI_ACTION_SEEK_FRIEND);
						}
					}

					// mark SEEKING as impossible for next time through while loop
#ifdef DEBUGDECISIONS
					AINameMessage(pSoldier,"couldn't HELP...",1000);
#endif

					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: couldn't help");
					bHelpPts = -99;
				}


				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: checking to hide");
				// if HIDING is possible and at least as desirable as seeking or helping
				if ((bHidePts > -90) && (bHidePts >= bSeekPts) && (bHidePts >= bHelpPts) && (bHidePts >= bWatchPts ))
				{
					//sClosestOpponent = ClosestKnownOpponent( pSoldier, NULL, NULL );
					// if an opponent is known (not necessarily reachable or conscious)					
					if (!SkipCoverCheck && !TileIsOutOfBounds(sClosestOpponent))
					{
						//////////////////////////////////////////////////////////////////////
						// TAKE BEST NEARBY COVER FROM ALL KNOWN OPPONENTS
						//////////////////////////////////////////////////////////////////////
#ifdef AI_TIMING_TESTS
						uiStartTime = GetJA2Clock();
#endif

						pSoldier->aiPlanning().actionData() = FindBestNearbyCover(pSoldier,pSoldier->morale().aiMorale(),&iDummy);
#ifdef AI_TIMING_TESTS
						uiEndTime = GetJA2Clock();

						guiRedHideTimeTotal += (uiEndTime - uiStartTime);
						guiRedHideCounter++;
#endif

						// let's be a bit cautious about going right up to a location without enough APs to shoot						
						if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
						{
							sClosestDisturbance = ClosestReachableDisturbance(pSoldier, &fClimb);
							if (!TileIsOutOfBounds(sClosestDisturbance) && ( SpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) < 5 || SpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) + 5 < SpacesAway( pSoldier->position().gridNo(), sClosestDisturbance ) ) )
							{
								// either moving significantly closer or into very close range
								// ensure will we have enough APs for a possible crouch plus a shot
								if ( InternalGoAsFarAsPossibleTowards( pSoldier, pSoldier->aiPlanning().actionData(), (INT8) (MinAPsToAttack( pSoldier, sClosestOpponent, ADDTURNCOST,0) + GetAPsCrouch( pSoldier, TRUE)), AI_ACTION_TAKE_COVER, 0 ) == pSoldier->aiPlanning().actionData() )
								{
									return(AI_ACTION_TAKE_COVER);
								}
							}
							else
							{
								return(AI_ACTION_TAKE_COVER);
							}
						}

					}

					// mark HIDING as impossible for next time through while loop
#ifdef DEBUGDECISIONS
					AINameMessage(pSoldier,"couldn't HIDE...",1000);
#endif

					bHidePts = -99;
					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: couldn't hide");
				}
			}
		}
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: nothing to do!");
		////////////////////////////////////////////////////////////////////////////
		// NOTHING USEFUL POSSIBLE!  IF NPC IS CURRENTLY UNDER FIRE, TRY TO RUN AWAY
		////////////////////////////////////////////////////////////////////////////

		// if we're currently under fire (presumably, attacker is hidden)
		if (pSoldier->suppression().underFire())
		{
			// only try to run if we've actually been hit recently & noticably so
			// otherwise, presumably our current cover is pretty good & sufficient
			// HEADROCK HAM B2.6: New value here helps us change the ratio of running away due to shock. This
			// is terribly important if Suppression Shock is enabled.
			UINT16 bShock = 0;

			if (gGameExternalOptions.usSuppressionShockEffect > 0 )
			{
				// If bShock value is greater than (2*ExpLevel + MoraleModifier)*1.5, the target will flee.
				bShock = pSoldier->suppression().shock();
				if (bShock <= ((float)CalcSuppressionTolerance(pSoldier)*(float)1.5))
					bShock = 0;
			}
			else
			{			
				bShock = pSoldier->suppression().shock();
			}
			
			if (bShock > 0)
			{
				// look for best place to RUN AWAY to (farthest from the closest threat)
				pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents(pSoldier);
				
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
				{
#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s RUNNING AWAY to grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: run away!");
					return(AI_ACTION_RUN_AWAY);
				}
			}

			////////////////////////////////////////////////////////////////////////////
			// UNDER FIRE, DON'T WANNA/CAN'T RUN AWAY, SO CROUCH
			////////////////////////////////////////////////////////////////////////////

			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: crouch or go prone");
			// if not in water and not already crouched
			if (gAnimControl[pSoldier->animationPlayback().state()].ubHeight == ANIM_STAND && IsValidStance(pSoldier, ANIM_CROUCH))
			{
				if (!gfTurnBasedAI || GetAPsToChangeStance(pSoldier, ANIM_CROUCH) <= pSoldier->actionPoints().current())
				{

#ifdef DEBUGDECISIONS
					sprintf(tempstr, "%s CROUCHES (STATUS RED)", pSoldier->identity().name());
					AIPopMessage(tempstr);
#endif

					pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
					return(AI_ACTION_CHANGE_STANCE);
				}
			}
			else if (gAnimControl[pSoldier->animationPlayback().state()].ubHeight != ANIM_PRONE)
			{
				// maybe go prone
				if (PreRandom(2) == 0 && IsValidStance(pSoldier, ANIM_PRONE))
				{
					pSoldier->aiPlanning().actionData() = ANIM_PRONE;
					return(AI_ACTION_CHANGE_STANCE);
				}
			}
		}
	}

	// civilians are only interested in running away
	if (fCivilian &&
		//TacticalActorAiBehavior::hasInitialActionPoints(*pSoldier) &&
		(pSoldier->suppression().underFire() ||
		!TileIsOutOfBounds(sClosestOpponent) && PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent) < TACTICAL_RANGE / 2))
		//!TileIsOutOfBounds( sClosestNoise ) && PythSpacesAway(pSoldier->sGridNo, sClosestNoise) < TACTICAL_RANGE / 2) )
		//CorpseWarning(pSoldier, pSoldier->sGridNo, pSoldier->position().level())
	{
		DebugAI(AI_MSG_TOPIC, pSoldier, String("[civilians run away]"));

		// look for best place to RUN AWAY to (farthest from the closest threat)
		pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents(pSoldier);
		DebugAI(AI_MSG_INFO, pSoldier, String("found run away spot %d", pSoldier->aiPlanning().actionData()));
		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
			return(AI_ACTION_RUN_AWAY);
		}
		//else if (!pSoldier->SkipCoverCheck() && gfTurnBasedAI) // only do in turnbased
		else if (!SkipCoverCheck && gfTurnBasedAI) // only do in turnbased
		{
			// try to take cover
			pSoldier->morale().aiMorale() = MORALE_WORRIED;
			pSoldier->aiPlanning().actionData() = FindBestNearbyCover(pSoldier, MORALE_WORRIED, &iDummy);

			if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
			{
				return(AI_ACTION_TAKE_COVER);
			}
		}
	}

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionred: look around towards opponent");
	////////////////////////////////////////////////////////////////////////////
	// LOOK AROUND TOWARD CLOSEST KNOWN OPPONENT, IF KNOWN
	////////////////////////////////////////////////////////////////////////////

	if (!gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current())
	{
		// determine the location of the known closest opponent
		// (don't care if he's conscious, don't care if he's reachable at all)
		//sClosestOpponent = ClosestKnownOpponent(pSoldier, NULL, NULL);
		
		if (!TileIsOutOfBounds(sClosestOpponent))
		{
			// determine direction from this soldier to the closest opponent
			ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestOpponent);

			// if soldier is not already facing in that direction,
			// and the opponent is close enough that he could possibly be seen
			// note, have to change this to use the level returned from ClosestKnownOpponent
			sDistVisible = TacticalActorVisibility::maximumDistance(*pSoldier, sClosestOpponent, 0, CALC_FROM_ALL_DIRS );

			if ((pSoldier->position().direction() != ubOpponentDir) && (PythSpacesAway(pSoldier->position().gridNo(),sClosestOpponent) <= sDistVisible))
			{
				// set base chance according to orders
				if ((pSoldier->aiBehavior().orders() == STATIONARY) || (pSoldier->aiBehavior().orders() == ONGUARD))
					iChance = 50;
				else           // all other orders
					iChance = 25;

				if (pSoldier->aiBehavior().attitude() == DEFENSIVE)
					iChance += 25;

				if ( ARMED_VEHICLE( pSoldier ) )
				{
					iChance += 50;
				}

				if ((INT16)PreRandom(100) < iChance && TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubOpponentDir) )
				{
					pSoldier->aiPlanning().actionData() = ubOpponentDir;

#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - TURNS TOWARDS CLOSEST ENEMY to face direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif
					if ( pSoldier->aiBehavior().orders() == SNIPER &&
						!WeaponReady(pSoldier) && 
						TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION &&
						(pSoldier->vitals().breath() > 15 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 50) )
					{
						if (!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, READY_RIFLE_CROUCH ) <= pSoldier->actionPoints().current())
						{
							pSoldier->aiPlanning().nextAction() = AI_ACTION_RAISE_GUN;
						}
					}
					////////////////////////////////////////////////////////////////////////////
					// SANDRO - allow regular soldiers to raise scoped weapons to see rather away too
					else if (IsScoped(&pSoldier->inventory()[HANDPOS]))
					{
						if (!WeaponReady(pSoldier) && 
							TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION &&
							(pSoldier->vitals().breath() > 15 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 50))
						{
							if (!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, READY_RIFLE_CROUCH ) <= pSoldier->actionPoints().current())
							{
								if ( Random(100) < 35 ) 
								{
									pSoldier->aiPlanning().nextAction() = AI_ACTION_RAISE_GUN;
								}
							}
						}
					}
					////////////////////////////////////////////////////////////////////////////

					return(AI_ACTION_CHANGE_FACING);
				}
			}
			////////////////////////////////////////////////////////////////////////////
			// SANDRO - allow regular soldiers to raise scoped weapons to see farther away too
			else if ( pSoldier->position().direction() == ubOpponentDir &&
					!WeaponReady(pSoldier) &&
					TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION)
			{
				if ((!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, pSoldier->animationPlayback().state() ) <= pSoldier->actionPoints().current()) && (pSoldier->vitals().breath() > 15 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 50))
				{
					if ( pSoldier->aiBehavior().orders() == SNIPER )
					{
						return AI_ACTION_RAISE_GUN;
					}
					else if (IsScoped(&pSoldier->inventory()[HANDPOS]))
					{
						if ( Random(100) < 40 ) 
						{
							return AI_ACTION_RAISE_GUN;
						}
					}
					else
					{
						if ( Random(100) < 20 ) 
						{
							return AI_ACTION_RAISE_GUN;
						}
					}
				}
			}
			////////////////////////////////////////////////////////////////////////////
		}
	}

	if ( ARMED_VEHICLE( pSoldier ) )
	{
		// try turning in a random direction as we still can't see anyone.
		if (!gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current())
		{
			sClosestDisturbance = MostImportantNoiseHeard( pSoldier, NULL, NULL, NULL );
			
			if (!TileIsOutOfBounds(sClosestDisturbance))
			{
				ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestDisturbance);
				if ( pSoldier->position().direction() == ubOpponentDir )
				{
					ubOpponentDir = (UINT8) PreRandom( NUM_WORLD_DIRECTIONS );
				}
			}
			else
			{
				ubOpponentDir = (UINT8) PreRandom( NUM_WORLD_DIRECTIONS );
			}

			if ( (pSoldier->position().direction() != ubOpponentDir) )
			{
				if ( (pSoldier->actionPoints().current() == pSoldier->actionPoints().initial() || (INT16)PreRandom(100) < 60) && TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubOpponentDir) )
				{
					pSoldier->aiPlanning().actionData() = ubOpponentDir;

#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - TURNS TOWARDS CLOSEST ENEMY to face direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					// limit turning a bit... if the last thing we did was also a turn, add a 60% chance of this being our last turn
					if ( pSoldier->aiPlanning().lastAction() == AI_ACTION_CHANGE_FACING && PreRandom( 100 ) < 60 )
					{
						if ( gfTurnBasedAI )
						{
							pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
						}
						else
						{
							pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
							pSoldier->aiPlanning().nextActionData() = (UINT16) REALTIME_AI_DELAY;
						}
					}

					return(AI_ACTION_CHANGE_FACING);
				}
			}
		}

		// that's it for tanks
		return( AI_ACTION_NONE );
	}

	////////////////////////////////////////////////////////////////////////////
	// LEAVE THE SECTOR
	////////////////////////////////////////////////////////////////////////////

	// NOT IMPLEMENTED


	////////////////////////////////////////////////////////////////////////////
	// PICKUP A NEARBY ITEM THAT'S USEFUL
	////////////////////////////////////////////////////////////////////////////

	if ( ubCanMove && !pSoldier->aiBehavior().neutral() && (gfTurnBasedAI || pSoldier->roster().team() == ENEMY_TEAM ) )
	{
		pSoldier->aiPlanning().action() = SearchForItems( pSoldier, SEARCH_GENERAL_ITEMS, pSoldier->inventory()[HANDPOS].usItem );

		// sevenfm: check that location is safe
		if( pSoldier->aiPlanning().action() != AI_ACTION_NONE &&
			!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) &&
			(GetNearestRottingCorpseAIWarning( pSoldier->aiPlanning().actionData() ) > 0 ||
			InLightAtNight( pSoldier->aiPlanning().actionData(), pSoldier->position().level() ) && !InLightAtNight(pSoldier->aiPlanning().actionData(), pSoldier->position().level())) &&
			!fDangerousSpot &&
			CountFriendsBlack(pSoldier) == 0 )
		{
			// abort! abort!
			pSoldier->aiPlanning().action() = AI_ACTION_NONE;
		}

		if (pSoldier->aiPlanning().action() != AI_ACTION_NONE)
		{
			return( pSoldier->aiPlanning().action() );
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// SEEK CLOSEST FRIENDLY MEDIC
	////////////////////////////////////////////////////////////////////////////

	// NOT IMPLEMENTED


	////////////////////////////////////////////////////////////////////////////
	// GIVE FIRST AID TO A NEARBY INJURED/DYING FRIEND
	////////////////////////////////////////////////////////////////////////////
	// - must be BRAVEAID or CUNNINGAID (medic) ?

	// NOT IMPLEMENTED

	/* JULY 29, 1996 - Decided that this was a bad idea, after watching a civilian
	start a random patrol while 2 steps away from a hidden armed opponent...*/

	////////////////////////////////////////////////////////////////////////////
	// SWITCH TO GREEN: soldier does ordinary regular patrol, seeks friends
	////////////////////////////////////////////////////////////////////////////

	// if not in combat or under fire, and we COULD have moved, just chose not to	
	if ( (pSoldier->aiBehavior().alertStatus() != STATUS_BLACK) && !pSoldier->suppression().underFire() && ubCanMove && (!gfTurnBasedAI || pSoldier->actionPoints().current() >= pSoldier->actionPoints().initial()) && ( TileIsOutOfBounds(ClosestReachableDisturbance(pSoldier, &fClimb))) )
	{
		// addition:  if soldier is bleeding then reduce bleeding and do nothing
		if ( pSoldier->vitals().bleeding() > MIN_BLEEDING_THRESHOLD )
		{
			// reduce bleeding by 1 point per AP (in RT, APs will get recalculated so it's okay)
			pSoldier->vitals().bleeding() = __max( 0, pSoldier->vitals().bleeding() - (pSoldier->actionPoints().current()/2) );
			return( AI_ACTION_NONE ); // will end-turn/wait depending on whether we're in TB or realtime
		}
#ifdef DEBUGDECISIONS
		AINameMessage(pSoldier,"- chose to SKIP all RED actions, BYPASSES to GREEN!",1000);
#endif
		// Skip RED until new situation/next turn, 30% extra chance to do GREEN actions
		pSoldier->aiBehavior().bypassToGreen() = 30;
		return(DecideActionGreen(pSoldier));
	}


	////////////////////////////////////////////////////////////////////////////
	// CROUCH IF NOT CROUCHING ALREADY
	////////////////////////////////////////////////////////////////////////////

	// if not in water and not already crouched, try to crouch down first
	if (!fCivilian && !bInWater && (gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_STAND) && IsValidStance( pSoldier, ANIM_CROUCH ) )
	{
		//sClosestOpponent = ClosestKnownOpponent(pSoldier, NULL, NULL);

		//if ( ( !TileIsOutOfBounds(sClosestOpponent) && PythSpacesAway( pSoldier->sGridNo, sClosestOpponent ) < (TacticalActorVisibility::normalMaximumDistance() * 3) / 2 ) || PreRandom( 4 ) == 0 )
		if ( (!TileIsOutOfBounds(sClosestOpponent) && PythSpacesAway( pSoldier->position().gridNo(), sClosestOpponent ) < (TacticalActorVisibility::maximumDistance(*pSoldier, sClosestOpponent) * 3) / 2 ) || PreRandom( 4 ) == 0 )
		{
			if (!gfTurnBasedAI || GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current())
			{

#ifdef DEBUGDECISIONS
				sprintf(tempstr,"%s CROUCHES (STATUS RED)",pSoldier->identity().name() );
				AIPopMessage(tempstr);
#endif
				
					////////////////////////////////////////////////////////////////////////////
					// SANDRO - allow regular soldiers to raise scoped weapons to see farther away too
					if (!gfTurnBasedAI || (GetAPsToReadyWeapon( pSoldier, READY_RIFLE_CROUCH ) + GetAPsToChangeStance( pSoldier, ANIM_CROUCH )) <= pSoldier->actionPoints().current())
					{
						// determine direction from this soldier to the closest opponent
						ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestOpponent);

						if (!WeaponReady(pSoldier) && 
							pSoldier->position().direction() == ubOpponentDir &&
							TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION)
						{
							if (IsScoped(&pSoldier->inventory()[HANDPOS]))
							{
								if ( Random(100) < 40 ) 
								{
									pSoldier->aiPlanning().nextAction() = AI_ACTION_RAISE_GUN;
								}
							}
						}
					}
					////////////////////////////////////////////////////////////////////////////


				pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
				return(AI_ACTION_CHANGE_STANCE);
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// IF UNDER FIRE, FACE THE MOST IMPORTANT NOISE WE KNOW AND GO PRONE
	////////////////////////////////////////////////////////////////////////////

	if ( !fCivilian && pSoldier->suppression().underFire() && pSoldier->actionPoints().current() >= (pSoldier->actionPoints().initial() - GetAPsToLook( pSoldier ) ) && IsValidStance( pSoldier, ANIM_PRONE ) )
	{
		sClosestDisturbance = MostImportantNoiseHeard( pSoldier, NULL, NULL, NULL );
		
		if (!TileIsOutOfBounds(sClosestDisturbance))
		{
			ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestDisturbance);
			if ( pSoldier->position().direction() != ubOpponentDir )
			{
				if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
				{
					pSoldier->aiPlanning().actionData() = ubOpponentDir;
					return( AI_ACTION_CHANGE_FACING );
				}
			}
			else if ( (!gfTurnBasedAI || GetAPsToChangeStance( pSoldier, ANIM_PRONE ) <= pSoldier->actionPoints().current() ) && TacticalActorMobility::isValidStance(*pSoldier,  ubOpponentDir, ANIM_PRONE ) )
			{
				// go prone, end turn
				pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
				pSoldier->aiPlanning().actionData() = ANIM_PRONE;
				return( AI_ACTION_CHANGE_STANCE );
			}
		}
	}


	////////////////////////////////////////////////////////////////////////////
	// If sniper and nothing else to do then raise gun, and if that doesn't find somebody then goto yellow
	////////////////////////////////////////////////////////////////////////////
	if ( pSoldier->aiBehavior().orders() == SNIPER )
	{
		if ( pSoldier->aiPlanning().sniperPosture() == 0 )
		{
			DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionRed: sniper raising gun..."));
			if ((!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, READY_RIFLE_CROUCH ) <= pSoldier->actionPoints().current()) && (pSoldier->vitals().breath() > 15 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 50))
			{
				if (!WeaponReady(pSoldier) &&
					TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION)
				{
					pSoldier->aiPlanning().raiseSniperPosture();
					return AI_ACTION_RAISE_GUN;
				}
			}
		}
		else
		{
			pSoldier->aiPlanning().lowerSniperPosture();
			return(DecideActionYellow(pSoldier));
		}
	}
	else if (!fCivilian)
	{
		////////////////////////////////////////////////////////////////////////////
		// SANDRO - raise weapon maybe
		if (!WeaponReady(pSoldier) && 
			TacticalActorAnimationSelection::pickReady(*pSoldier, false, false) != INVALID_ANIMATION &&
			(pSoldier->vitals().breath() > 15 || GetBPCostPer10APsForGunHolding( pSoldier, TRUE ) < 50))
		{
			if (!gfTurnBasedAI || GetAPsToReadyWeapon( pSoldier, pSoldier->animationPlayback().state() ) <= pSoldier->actionPoints().current())
			{
				if (IsScoped(&pSoldier->inventory()[HANDPOS]))
				{
					if ( Random(100) < 35 ) 
					{
						return( AI_ACTION_RAISE_GUN );
					}
				}
			}
		}
		////////////////////////////////////////////////////////////////////////////
	
	}

	////////////////////////////////////////////////////////////////////////////
	// DO NOTHING: Not enough points left to move, so save them for next turn
	////////////////////////////////////////////////////////////////////////////
	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionRed: do nothing at all..."));
#ifdef DEBUGDECISIONS
	AINameMessage(pSoldier,"- DOES NOTHING (RED)",1000);
#endif

	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);
}

// Flugente: dummies if we do not want to check for any conditions or taboos
BOOLEAN SoldierCondTrue(TacticalActor *pSoldier)			{ return TRUE; }
BOOLEAN SoldierCondFalse(TacticalActor *pSoldier)			{ return FALSE; }

INT8 DecideActionBlack(TacticalActor *pSoldier)
{
	INT32	iCoverPercentBetter, iOffense, iDefense, iChance;
	INT32	sClosestOpponent = NOWHERE,sBestCover = NOWHERE;//dnl ch58 160813
 INT32	sClosestDisturbance;
INT16 ubMinAPCost;
	UINT8	ubCanMove;
	INT8		bInWater,bInDeepWater,bInGas;
	INT8		bDirection;
	UINT8	ubBestAttackAction = AI_ACTION_NONE;
	INT8		bCanAttack,bActionReturned;
	INT8		bWeaponIn;
	BOOLEAN	fTryPunching = FALSE;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif
	DebugMsg(TOPIC_JA2,DBG_LEVEL_3,String("DecideActionBlack: soldier = %d, orders = %d, attitude = %d",pSoldier->identity().id(),pSoldier->aiBehavior().orders(),pSoldier->aiBehavior().attitude()));

	DebugAI(AI_MSG_START, pSoldier, String("[Black]"));
	LogDecideInfo(pSoldier);

	ATTACKTYPE BestShot, BestThrow, BestStab ,BestAttack;//dnl ch69 150913
	TacticalActor* bestShotOpponent = nullptr;
	BOOLEAN fCivilian = (PTR_CIVILIAN && (pSoldier->roster().civilianGroup() == NON_CIV_GROUP || pSoldier->aiBehavior().neutral() || (pSoldier->identity().bodyType() >= FATCIV && pSoldier->identity().bodyType() <= CRIPPLECIV) ) );
	BOOLEAN fClimb;
	INT16	ubBurstAPs;
	UINT8	ubOpponentDir;
 INT32	sCheckGridNo;

	BOOLEAN fAllowCoverCheck = FALSE;

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"DecideActionBlack");

	// sevenfm: disable stealth mode
	pSoldier->movement().setStealth(false);
	// disable reverse movement mode
	pSoldier->movement().setReverse(false);
	// sevenfm: initialize data
	pSoldier->attackSelection().weaponMode() = WM_NORMAL;

	// sevenfm: stop flanking when we see enemy
	if( AICheckIsFlanking(pSoldier) )
	{
		pSoldier->aiPlanning().clearFlank();
	}
	
	// if we have absolutely no action points, we can't do a thing under BLACK!
	if (!pSoldier->actionPoints().current())
	{
		pSoldier->aiPlanning().actionData() = NOWHERE;
		return(AI_ACTION_NONE);
	}

	// can this guy move to any of the neighbouring squares ? (sets TRUE/FALSE)
	ubCanMove = (pSoldier->actionPoints().current() >= MinPtsToMove(pSoldier));

	if( pSoldier->status().flags() & ( SOLDIER_DRIVER | SOLDIER_PASSENGER ) )
	{
		ubCanMove = 0;
	}

	// sevenfm: before deciding anything, stop cowering
	if (SoldierAI(pSoldier) &&
		!fCivilian &&
		ubCanMove &&
		pSoldier->vitals().health() >= OKLIFE &&
		!pSoldier->collapseState().tactical() &&
		!pSoldier->collapseState().breathTriggered() &&
		TacticalActorConditions::isCowering(*pSoldier))
	{
		return AI_ACTION_STOP_COWERING;
	}

	// sevenfm: stop giving aid
	if (SoldierAI(pSoldier) &&
		pSoldier->actionPoints().current() > 0 &&
		pSoldier->vitals().health() >= OKLIFE &&
		!pSoldier->collapseState().tactical() &&
		!pSoldier->collapseState().breathTriggered() &&
		TacticalActorConditions::isGivingAid(*pSoldier))
	{
		return AI_ACTION_STOP_MEDIC;
	}

	if ( (pSoldier->roster().team() == ENEMY_TEAM || pSoldier->identity().profile() == WARDEN) && (gTacticalStatus.fPanicFlags & PANIC_TRIGGERS_HERE) && (gTacticalStatus.ubTheChosenOne == NOBODY) )
	{
		INT8 bPanicTrigger;

		bPanicTrigger = ClosestPanicTrigger( pSoldier );
		// if it's an alarm trigger and team is alerted, ignore it
		if ( bPanicTrigger != -1 && !(gTacticalStatus.bPanicTriggerIsAlarm[ bPanicTrigger ] && gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition) && PythSpacesAway( pSoldier->position().gridNo(), gTacticalStatus.sPanicTriggerGridNo[ bPanicTrigger ] ) < 10)
		{
			PossiblyMakeThisEnemyChosenOne( pSoldier );
		}
	}

	// if this soldier is the "Chosen One" (enemies only)
	if (pSoldier->identity().id() == gTacticalStatus.ubTheChosenOne)
	{
		// do some special panic AI decision making
		bActionReturned = PanicAI(pSoldier,ubCanMove);

		// if we decided on an action while in there, we're done
		if (bActionReturned != -1)
			return(bActionReturned);
	}

	if ( pSoldier->identity().profile() != NO_PROFILE )
	{
		// if they see enemies, the Queen will keep going to the staircase, but Joe will fight
		if ( (pSoldier->identity().profile() == QUEEN) && ubCanMove )
		{
			if ( gWorldSectorX == 3 && gWorldSectorY == MAP_ROW_P && gbWorldSectorZ == 0 && !gfUseAlternateQueenPosition )
			{
				bActionReturned = HeadForTheStairCase( pSoldier );
				if ( bActionReturned != AI_ACTION_NONE )
				{
					return( bActionReturned );
				}
			}
		}
	}

	if ( pSoldier->status().flags() & SOLDIER_BOXER )
	{
		if ( gTacticalStatus.bBoxingState == PRE_BOXING )
		{
			return( DecideActionBoxerEnteringRing( pSoldier ) );
		}
		else if ( gTacticalStatus.bBoxingState == BOXING )
		{
			bInWater = FALSE;
			bInDeepWater = FALSE;
			bInGas = FALSE;

			// calculate our morale
			// sevenfm: for boxer, always use high morale
			//pSoldier->morale().aiMorale() = CalcMorale(pSoldier);
			pSoldier->morale().aiMorale() = MORALE_FEARLESS;
			// and continue on...
		}
		else //????
		{
			return( AI_ACTION_NONE );
		}
	}
	else
	{
		// determine if we happen to be in water (in which case we're in BIG trouble!)
		bInWater = Water( pSoldier->position().gridNo(), pSoldier->position().level() );
		bInDeepWater = WaterTooDeepForAttacks( pSoldier->position().gridNo(), pSoldier->position().level() );

		// check if standing in tear gas without a gas mask on
		bInGas = InGasOrSmoke( pSoldier, pSoldier->position().gridNo() );

		// Flugente: tanks do not care about gas
		if ( ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier ) )
		{
			bInGas = FALSE;
		}

		// calculate our morale
		pSoldier->morale().aiMorale() = CalcMorale(pSoldier);

		////////////////////////////////////////////////////////////////////////////
		// WHEN LEFT IN GAS, WEAR GAS MASK IF AVAILABLE AND NOT WORN
		////////////////////////////////////////////////////////////////////////////

		if ( !bInGas && (gWorldSectorX == TIXA_SECTOR_X && gWorldSectorY == TIXA_SECTOR_Y) )
		{
			// only chance if we happen to be caught with our gas mask off
			if ( PreRandom( 10 ) == 0 && WearGasMaskIfAvailable( pSoldier ) )
			{
				bInGas = FALSE;
			}
		}

	//Only put mask on in gas
	if(bInGas && WearGasMaskIfAvailable(pSoldier))//dnl ch40 200909
		bInGas = InGasOrSmoke(pSoldier, pSoldier->position().gridNo());

		////////////////////////////////////////////////////////////////////////////
		// IF GASSED, OR REALLY TIRED (ON THE VERGE OF COLLAPSING), TRY TO RUN AWAY
		////////////////////////////////////////////////////////////////////////////

		// if we're desperately short on breath (it's OK if we're in water, though!)
		if (bInGas || (pSoldier->vitals().breath() < 5))
		{
			// if soldier has enough APs left to move at least 1 square's worth
			if (ubCanMove)
			{
				// look for best place to RUN AWAY to (farthest from the closest threat)
				pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents(pSoldier);
				
				if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
				{
#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - GASSED or LOW ON BREATH (%d), RUNNING AWAY to grid %d",pSoldier->identity().name(),pSoldier->vitals().breath(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					return(AI_ACTION_RUN_AWAY);
				}
			}

			// REALLY tired, can't get away, force soldier's morale to hopeless state
			if ( gGameOptions.ubDifficultyLevel == DIF_LEVEL_INSANE )
			{
				pSoldier->vitals().breath() = pSoldier->vitals().maximumBreath();  //Madd: backed into a corner, so go crazy like a wild animal...
				pSoldier->morale().aiMorale() = MORALE_FEARLESS;
			}
			else
				pSoldier->morale().aiMorale() = MORALE_HOPELESS;
		}

	}



	////////////////////////////////////////////////////////////////////////////
	// STUCK IN WATER OR GAS, NO COVER, GO TO NEAREST SPOT OF UNGASSED LAND
	////////////////////////////////////////////////////////////////////////////

	// when in deep water, move to closest opponent
	if (ubCanMove && bInDeepWater && !pSoldier->aiBehavior().neutral() && pSoldier->aiBehavior().orders() == SEEKENEMY)
	{
		// find closest reachable opponent, excluding opponents in deep water
		pSoldier->aiPlanning().actionData() = ClosestReachableDisturbance(pSoldier, &fClimb);

		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
			return(AI_ACTION_LEAVE_WATER_GAS);
		}
	}

	// if soldier in water/gas has enough APs left to move at least 1 square
	if (ubCanMove && (bInGas || bInDeepWater || FindBombNearby(pSoldier, pSoldier->position().gridNo(), BOMB_DETECTION_RANGE) || RedSmokeDanger(pSoldier->position().gridNo(), pSoldier->position().level())))
	{
		pSoldier->aiPlanning().actionData() = FindNearestUngassedLand(pSoldier);
		
		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
#ifdef DEBUGDECISIONS
			sprintf(tempstr,"%s - SEEKING NEAREST UNGASSED LAND at grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
			AIPopMessage(tempstr);
#endif

			return(AI_ACTION_LEAVE_WATER_GAS);
		}

		// couldn't find ANY land within 25 tiles(!), this should never happen...

		// look for best place to RUN AWAY to (farthest from the closest threat)
		pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents(pSoldier);
		
		if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
		{
#ifdef DEBUGDECISIONS
			sprintf(tempstr,"%s - NO LAND NEAR, RUNNING AWAY to grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
			AIPopMessage(tempstr);
#endif

			return(AI_ACTION_RUN_AWAY);
		}

		// GIVE UP ON LIFE!  MERCS MUST HAVE JUST CORNERED A HELPLESS ENEMY IN A
		// GAS FILLED ROOM (OR IN WATER MORE THAN 25 TILES FROM NEAREST LAND...)
		if ( bInGas && gGameOptions.ubDifficultyLevel == DIF_LEVEL_INSANE )
		{
			pSoldier->vitals().breath() = pSoldier->vitals().maximumBreath();
			pSoldier->morale().aiMorale() = MORALE_FEARLESS;  // Can't move, can't get away, go nuts instead...
		}
		else
			pSoldier->morale().aiMorale() = MORALE_HOPELESS;
	}

	// offer surrender?
#ifndef JA2UB
	if ( !is_networked ) // No surrender in multiplayer
	{
		if ( pSoldier->roster().team() == ENEMY_TEAM && pSoldier->awareness().visibility() == TRUE && !(gTacticalStatus.fEnemyFlags & ENEMY_OFFERED_SURRENDER) && pSoldier->vitals().health() >= pSoldier->vitals().maximumHealth() / 2 && !ARMED_VEHICLE( pSoldier ) && !ENEMYROBOT( pSoldier ) )
		{
			if ( gTacticalStatus.Team[ MILITIA_TEAM ].bMenInSector == 0 && gTacticalStatus.Team[ CREATURE_TEAM ].bMenInSector == 0 && NumPCsInSector() < 4 && gTacticalStatus.Team[ ENEMY_TEAM ].bMenInSector >= NumPCsInSector() * 3 )
			{
				if (gubQuest[QUEST_HELD_IN_ALMA] == QUESTNOTSTARTED || gubQuest[QUEST_HELD_IN_TIXA] == QUESTNOTSTARTED || gubQuest[QUEST_INTERROGATION] == QUESTNOTSTARTED)
				{
					return( AI_ACTION_OFFER_SURRENDER );
				}
			}
		}
	}
#endif

	////////////////////////////////////////////////////////////////////////////
	// SOLDIER CAN ATTACK IF NOT IN WATER/GAS AND NOT DOING SOMETHING TOO FUNKY
	////////////////////////////////////////////////////////////////////////////

	// NPCs in water/tear gas without masks are not permitted to shoot/stab/throw
	if ((pSoldier->actionPoints().current() < 2) || bInDeepWater || bInGas || pSoldier->aiBehavior().realtimeCombat() == RTP_COMBAT_REFRAIN)
	{
		bCanAttack = FALSE;
	}
	else if (pSoldier->status().flags() & SOLDIER_BOXER)
	{
		bCanAttack = TRUE;
		fTryPunching = TRUE;
	}
	else
	{
		do
		{
			bCanAttack = CanNPCAttack(pSoldier);
			if (bCanAttack != TRUE)
			{
				if (fCivilian)
				{
					if ( ( bCanAttack == NOSHOOT_NOWEAPON) && !(pSoldier->status().flags() & SOLDIER_BOXER) && pSoldier->identity().bodyType() != COW && pSoldier->identity().bodyType() != CRIPPLECIV && !(pSoldier->status().flags() & SOLDIER_VEHICLE) )
					{
						// cower in fear!!
						if ( pSoldier->status().flags() & SOLDIER_COWERING )
						{
							if ( pSoldier->aiPlanning().lastAction() == AI_ACTION_COWER )
							{
								// do nothing
								pSoldier->aiPlanning().actionData() = NOWHERE;
								return( AI_ACTION_NONE );
							}
							else
							{
								// set up next action to run away
								pSoldier->aiPlanning().nextActionData() = FindSpotMaxDistFromOpponents( pSoldier );
								
								if (!TileIsOutOfBounds(pSoldier->aiPlanning().nextActionData()))
								{
									pSoldier->aiPlanning().nextAction() = AI_ACTION_RUN_AWAY;
									pSoldier->aiPlanning().actionData() = ANIM_STAND;
									return( AI_ACTION_STOP_COWERING );
								}
								else
								{
									return( AI_ACTION_NONE );
								}
							}
						}
						else
						{
							// cower!!!
							pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
							return( AI_ACTION_COWER );
						}
					}
				}
				else if (bCanAttack == NOSHOOT_NOAMMO && ubCanMove && !pSoldier->aiBehavior().neutral())
				{
					int handPOS;
					//CHRISL: We need to know which weapon has no ammo in case the soldier is holding a weapoin in SECONDHANDPOS
					if(pSoldier->inventory()[SECONDHANDPOS].exists() == true && pSoldier->inventory()[SECONDHANDPOS][0]->data.gun.ubGunShotsLeft == 0)
						handPOS = SECONDHANDPOS;
					else
						handPOS = HANDPOS;

					// try to find more ammo
					pSoldier->aiPlanning().action() = SearchForItems( pSoldier, SEARCH_AMMO, pSoldier->inventory()[handPOS].usItem );

					if (pSoldier->aiPlanning().action() == AI_ACTION_NONE)
					{
						// the current weapon appears is useless right now!
						// (since we got a return code of noammo, we know the hand usItem
						// is our gun)
						pSoldier->inventory()[handPOS].fFlags |= OBJECT_AI_UNUSABLE;
						// move the gun into another pocket...
						if (!AutoPlaceObject( pSoldier, &(pSoldier->inventory()[handPOS]), FALSE ) )
						{
							// If there's no room in his pockets for the useless gun, just throw it away
							return AI_ACTION_DROP_ITEM;
						}
					}
					else
					{
						return( pSoldier->aiPlanning().action() );
					}
				}
				else
				{
					bCanAttack = FALSE;
				}
			}
		} while( bCanAttack != TRUE && bCanAttack != FALSE );

#ifdef RETREAT_TESTING
		bCanAttack = FALSE;
#endif

		if (!bCanAttack)
		{
			if (pSoldier->morale().aiMorale() > MORALE_WORRIED)
			{
				pSoldier->morale().aiMorale() = MORALE_WORRIED;
			}

			if (!fCivilian)
			{
				// can always attack with HTH as a last resort
				bCanAttack = TRUE;
				fTryPunching = TRUE;
			}
		}
	}

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Self smoke when under fire]"));
	if (SoldierAI(pSoldier) &&
		gfTurnBasedAI &&
		pSoldier->actionPoints().current() == pSoldier->actionPoints().initial() &&
		pSoldier->suppression().underFire() &&
		!InARoom(pSoldier->position().gridNo(), NULL) &&
		!InSmoke(pSoldier->position().gridNo(), pSoldier->position().level()) &&
		RangeChangeDesire(pSoldier) <= 2 &&
		(!NightLight() || InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level())) &&
		!TileIsOutOfBounds(sClosestOpponent) &&
		PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent) > TACTICAL_RANGE / 4 &&
		(!ProneSightCoverAtSpot(pSoldier, pSoldier->position().gridNo(), FALSE) && !AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo()) || TacticalActorConditions::hasTakenLargeHit(*pSoldier)) &&
		(TacticalActorConditions::hasTakenLargeHit(*pSoldier) || TacticalActorConditions::suppressionShockPercent(*pSoldier) > 20 + Random(80)))
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("check if soldier can cover himself with smoke"));

		CheckTossSelfSmoke(pSoldier, &BestThrow);

		if (BestThrow.ubPossible)
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("prepare throw at spot %d level %d aimtime %d", BestThrow.sTarget, BestThrow.bTargetLevel, BestThrow.ubAimTime));

			// start retreating for several turns
			TacticalActorAiBehavior::startRetreat(*pSoldier, 3);

			// if necessary, swap the usItem from holster into the hand position
			if (BestThrow.bWeaponIn != HANDPOS)
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("rearrange pocket"));
				RearrangePocket(pSoldier, HANDPOS, BestThrow.bWeaponIn, FOREVER);
			}

			// stand up before throwing if needed
			if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight < BestThrow.ubStance &&
				TacticalActorMobility::isValidStance(*pSoldier, AIDirection(pSoldier->position().gridNo(), BestThrow.sTarget), BestThrow.ubStance))
			{
				pSoldier->aiPlanning().actionData() = BestThrow.ubStance;
				pSoldier->aiPlanning().nextAction() = AI_ACTION_TOSS_PROJECTILE;
				pSoldier->aiPlanning().nextActionData() = BestThrow.sTarget;
				pSoldier->aiPlanning().nextTargetLevel() = BestThrow.bTargetLevel;
				pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
				return AI_ACTION_CHANGE_STANCE;
			}
			else
			{
				pSoldier->aiPlanning().actionData() = BestThrow.sTarget;
				pSoldier->targeting().level() = BestThrow.bTargetLevel;
				pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
			}

			return(AI_ACTION_TOSS_PROJECTILE);
		}
	}

	// if we don't have a gun, look around for a weapon!
	if (FindAIUsableObjClass( pSoldier, IC_GUN ) == ITEM_NOT_FOUND && ubCanMove && !pSoldier->aiBehavior().neutral())
	{
		// look around for a gun...
		pSoldier->aiPlanning().action() = SearchForItems( pSoldier, SEARCH_WEAPONS, pSoldier->inventory()[HANDPOS].usItem );
		if (pSoldier->aiPlanning().action() != AI_ACTION_NONE )
		{
			return( pSoldier->aiPlanning().action() );
		}
	}

	// Flugente: trait skills
	// if we are a radio operator
	if (HAS_SKILL_TRAIT(pSoldier, RADIO_OPERATOR_NT) > 0 &&
		TacticalActorSkills::canUse(
			*pSoldier,
			SKILLS_RADIO_ARTILLERY,
			true))
	{
		// check: would it be possible to call in artillery from neighbouring sectors?
		UINT32 tmp;
		INT32 skilltargetgridno = 0;
		// can we call in artillery?
		if (TacticalActorRadio::canOrderAnyArtilleryStrike(
				*pSoldier,
				&tmp))
		{
			// if frequencies are jammed...
			if (TacticalActorRadio::sectorJammed())
			{
				// if we are jamming, turn it off, otherwise, bad luck...
				if (TacticalActorRadio::isJamming(*pSoldier))
				{
					pSoldier->skillState().selectedAiSkill() = SKILLS_RADIO_TURNOFF;
					pSoldier->aiPlanning().actionData() = skilltargetgridno;
					return(AI_ACTION_USE_SKILL);
				}
			}
			// frequencies are clear, order a strike
			else if ( GetBestAoEGridNo(pSoldier, &skilltargetgridno, max(1, gSkillTraitValues.usVOMortarRadius - 2), 1, 2, SoldierCondTrue, SoldierCondFalse) )
			{
				pSoldier->skillState().selectedAiSkill() = SKILLS_RADIO_ARTILLERY;
				pSoldier->aiPlanning().actionData() = skilltargetgridno;
				return(AI_ACTION_USE_SKILL);
			}
		}
		// no access to artillery... we can still call reinforcements if we haven't yet done so
		else if ( !gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition && MoreFriendsThanEnemiesinNearbysectors(pSoldier->roster().team(), pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ()) )
		{
			// if frequencies are jammed...
			if (TacticalActorRadio::sectorJammed())
			{
				// if we are jamming, turn it off, otherwise, bad luck...
				if (TacticalActorRadio::isJamming(*pSoldier))
				{
					pSoldier->skillState().selectedAiSkill() = SKILLS_RADIO_TURNOFF;
					pSoldier->aiPlanning().actionData() = skilltargetgridno;
					return(AI_ACTION_USE_SKILL);
				}
			}
			// frequencies are clear, lets call for help
			else if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) )
			{
				// raise alarm!
				return( AI_ACTION_RED_ALERT );
			}
		}
		// if we can't call in artillery or reinforcements, then nobody else from our team can. So we better jam communications, so that the player cannot use these skills either
		else if (!TacticalActorRadio::isJamming(*pSoldier))
		{
			pSoldier->skillState().selectedAiSkill() = SKILLS_RADIO_JAM;
			pSoldier->aiPlanning().actionData() = skilltargetgridno;
			return(AI_ACTION_USE_SKILL);
		}
	}

	// VIPs run away (but not the GENERAL)
	if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_VIP && pSoldier->identity().profile() != GENERAL )
	{
		// this is in red AI state - a firefight is going on, we try to escape
		pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents( pSoldier );

		// if we don't know where our opponents are, we cannot run away from them...
		if ( TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
		{
			// search for the closest map edge
			pSoldier->aiPlanning().actionData() = FindClosestExitGrid( pSoldier, pSoldier->position().gridNo(), 200 );
		}

		if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
		{
			return AI_ACTION_RUN_AWAY;
		}
	}

	BestShot.ubPossible  = FALSE;	// by default, assume Shooting isn't possible
	BestThrow.ubPossible = FALSE;	// by default, assume Throwing isn't possible
	BestStab.ubPossible  = FALSE;	// by default, assume Stabbing isn't possible

	BestAttack.ubChanceToReallyHit = 0;

	// if we are able attack
	if (bCanAttack)
	{
		pSoldier->attackSelection().shotLocation() = AIM_SHOT_RANDOM;

		//////////////////////////////////////////////////////////////////////////
		// FIRE A GUN AT AN OPPONENT
		//////////////////////////////////////////////////////////////////////////
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"FIRE A GUN AT AN OPPONENT");

		CheckIfShotPossible(pSoldier, &BestShot);
		bestShotOpponent =
			GetJa2SoldierRepository().resolve(BestShot.ubOpponent.i);
		if (BestShot.ubPossible && !bestShotOpponent)
		{
			BestShot.ubPossible = FALSE;
		}

		if (BestShot.ubFriendlyFireChance)	//dnl ch61 180813
		{
			// determine chance to shoot
			INT32 iChanceToShoot;

			iChanceToShoot = 100 - BestShot.ubFriendlyFireChance;
			iChanceToShoot = iChanceToShoot * iChanceToShoot / 100;

			DebugAI(AI_MSG_INFO, pSoldier, String("Friendly fire chance %d, chance to shoot %d", BestShot.ubFriendlyFireChance, iChanceToShoot));

			if (Chance(100 - iChanceToShoot))
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("Friendly fire check failed, skip shooting!"));
				BestShot.ubPossible = FALSE;
			}
		}

		if (BestShot.ubPossible)
		{
			// if the selected opponent is not a threat (unconscious & !serviced)
			// (usually, this means all the guys we see are unconscious, but, on
			//  rare occasions, we may not be able to shoot a healthy guy, too)
			if ((bestShotOpponent->vitals().health() < OKLIFE) &&
				!bestShotOpponent->service().active() &&
				(pSoldier->aiBehavior().attitude() != AGGRESSIVE || Chance((100 - BestShot.ubChanceToReallyHit) / 2)))
			{
				// get the location of the closest CONSCIOUS reachable opponent
				sClosestDisturbance = ClosestReachableDisturbance(pSoldier, &fClimb);

				// if we found one								
				if (!TileIsOutOfBounds(sClosestDisturbance))
				{
					// then make decision as if at alert status RED
					return DecideActionRed(pSoldier);
				}
				// else kill the guy, he could be the last opponent alive in this sector
			}

			// now we KNOW FOR SURE that we will do something (shoot, at least)
			NPCDoesAct(pSoldier);
			DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "NPC decided to shoot (or something)");
		}

		//////////////////////////////////////////////////////////////////////////
		// THROW A TOSSABLE ITEM AT OPPONENT(S)
		// 	- HTH: THIS NOW INCLUDES FIRING THE GRENADE LAUNCHAR AND MORTAR!
		//////////////////////////////////////////////////////////////////////////

		// this looks for throwables, and sets BestThrow.ubPossible if it can be done
		CheckIfTossPossible(pSoldier,&BestThrow);

		if (BestThrow.ubPossible)
		{
			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"good throw possible");
			if (ItemIsMortar(pSoldier->inventory()[ BestThrow.bWeaponIn ].usItem))
			{
				ubOpponentDir = AIDirection(pSoldier->position().gridNo(), BestThrow.sTarget);

				// Get new gridno!
				sCheckGridNo = NewGridNo( pSoldier->position().gridNo(), (UINT16)DirectionInc( ubOpponentDir ) );

				if ( !OKFallDirection( pSoldier, sCheckGridNo, pSoldier->position().level(), ubOpponentDir, pSoldier->animationPlayback().state() ) )
				{
					// can't fire!
					BestThrow.ubPossible = FALSE;

					// try behind us, see if there's room to move back
					sCheckGridNo = NewGridNo( pSoldier->position().gridNo(), (UINT16)DirectionInc( gOppositeDirection[ ubOpponentDir ] ) );
					if ( OKFallDirection( pSoldier, sCheckGridNo, pSoldier->position().level(), gOppositeDirection[ ubOpponentDir ], pSoldier->animationPlayback().state() ) )
					{
						// sevenfm: check if we can reach this gridno
					INT32 iPathCost = EstimatePlotPath(pSoldier, sCheckGridNo, FALSE, FALSE, FALSE, DetermineMovementMode(pSoldier, AI_ACTION_GET_CLOSER), pSoldier->movement().stealthMode(), FALSE, 0);
					if (iPathCost != 0 && iPathCost <= pSoldier->actionPoints().current())
					{
							pSoldier->aiPlanning().actionData() = sCheckGridNo;
							return AI_ACTION_GET_CLOSER;
					}
				}
			}
			}

			if ( BestThrow.ubPossible )
			{
				// now we KNOW FOR SURE that we will do something (throw, at least)
				NPCDoesAct(pSoldier);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// GO STAB AN OPPONENT WITH A KNIFE
		//////////////////////////////////////////////////////////////////////////

		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"GO STAB AN OPPONENT WITH A KNIFE");
		// if soldier has a knife in his hand
		bWeaponIn = FindAIUsableObjClass( pSoldier, (IC_BLADE | IC_THROWING_KNIFE) );

		// if the soldier does have a usable knife somewhere
		// 0verhaul:  And is not a tank!
		if ( bWeaponIn != NO_SLOT && !ARMED_VEHICLE( pSoldier ) && !ENEMYROBOT( pSoldier ) && !(pSoldier->status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
		{
			DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"try to stab");
			BestStab.bWeaponIn = bWeaponIn;
			// if it's in his holster, swap it into his hand temporarily
			if (bWeaponIn != HANDPOS)
			{
				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionblack: about to rearrange pocket before stab check");
				RearrangePocket(pSoldier,HANDPOS,bWeaponIn,TEMPORARILY);
			}

			// get the minimum cost to attack with this knife
			ubMinAPCost = MinAPsToAttack(pSoldier,pSoldier->targeting().lastGridNo(),DONTADDTURNCOST,0,0);

			// if we can afford the minimum AP cost to stab with/throw this knife weapon
			if (pSoldier->actionPoints().current() >= ubMinAPCost)
			{
				// NB throwing knife in hand now
				if ( Item[ pSoldier->inventory()[HANDPOS].usItem ].usItemClass & IC_THROWING_KNIFE )
				{
					// throwing knife code works like shooting

					// look around for a worthy target (which sets BestStab.ubPossible)
					CalcBestShot(pSoldier,&BestStab);
					TacticalActor* throwingKnifeOpponent =
						GetJa2SoldierRepository().resolve(
							BestStab.ubOpponent.i);
					if (BestStab.ubPossible && !throwingKnifeOpponent)
					{
						BestStab.ubPossible = FALSE;
					}

					if (BestStab.ubPossible)
					{
						// if the selected opponent is not a threat (unconscious & !serviced)
						// (usually, this means all the guys we see are unconscious, but, on
						//  rare occasions, we may not be able to shoot a healthy guy, too)
						if ((throwingKnifeOpponent->vitals().health() < OKLIFE) &&
							!throwingKnifeOpponent->service().active())
						{
							// don't throw a knife at him.
							BestStab.ubPossible = FALSE;
						}

						// now we KNOW FOR SURE that we will do something (shoot, at least)
						NPCDoesAct(pSoldier);
						DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"NPC decided to shoot (2)");
					}
				}
				else
				{
					//sprintf((CHAR *)tempstr,"%s - ubMinAPCost = %d",pSoldier->identity().name(),ubMinAPCost);
					//PopMessage(tempstr);
					// then look around for a worthy target (which sets BestStab.ubPossible)
					CalcBestStab(pSoldier,&BestStab, TRUE);

					if (BestStab.ubPossible)
					{
						INT32 sAttackDist = PythSpacesAway(pSoldier->position().gridNo(), BestStab.sTarget);
						INT32 sMaxStabAttackDist = TACTICAL_RANGE / 8;
						// sevenfm: limit stab attacks when target is not very close
						if (sAttackDist > sMaxStabAttackDist)
						{
							BestStab.iAttackValue = BestStab.iAttackValue * sMaxStabAttackDist / sAttackDist;
						}

						if (!(gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT(pSoldier, MELEE_NT)) &&
							!(!gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT(pSoldier, KNIFING_OT)))
						{
							BestStab.iAttackValue /= 4;
						}

						// sevenfm: reduce stab attack attractiveness depending on number of seen opponents
						if (pSoldier->awareness().opponentCount() > 1)
						{
							BestStab.iAttackValue /= pSoldier->awareness().opponentCount();
						}

						// now we KNOW FOR SURE that we will do something (stab, at least)
						NPCDoesAct(pSoldier);
						DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"NPC decided to stab");
					}
				}

			}

			// if it was in his holster, swap it back into his holster for now
			if (bWeaponIn != HANDPOS)
			{
				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"about to rearrange pocket after stab check");
				RearrangePocket(pSoldier,HANDPOS,bWeaponIn,TEMPORARILY);
			}
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////
		// SANDRO - even if we don't have any blade, calculate how much damage we could do unarmed
		else if ( !ARMED_VEHICLE( pSoldier ) && !ENEMYROBOT( pSoldier ) && !(pSoldier->status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
		{
			bWeaponIn = FindAIUsableObjClass( pSoldier, IC_PUNCH );
			if (bWeaponIn == NO_SLOT) // if no punch-type weapon found, just calculate it with empty hands
			{
				bWeaponIn = FindEmptySlotWithin( pSoldier, HANDPOS, NUM_INV_SLOTS );
			}
			if (bWeaponIn != NO_SLOT)
			{
				DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"try to punch");
				BestStab.bWeaponIn = bWeaponIn;
				// if it's in his holster, swap it into his hand temporarily
				if (bWeaponIn != HANDPOS)
				{
					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"decideactionblack: about to rearrange pocket before punch check");
					RearrangePocket(pSoldier,HANDPOS,bWeaponIn,TEMPORARILY);
				}

				// get the minimum cost to attack with punch
				ubMinAPCost = MinAPsToAttack(pSoldier,pSoldier->targeting().lastGridNo(),DONTADDTURNCOST,0,0);
				// if we can afford the minimum AP cost to punch
				if (pSoldier->actionPoints().current() >= ubMinAPCost)
				{
					// then look around for a worthy target (which sets BestStab.ubPossible)
					CalcBestStab(pSoldier,&BestStab, FALSE);

					if (BestStab.ubPossible)
					{
						if (!(pSoldier->status().flags() & SOLDIER_BOXER))
						{
							// if we have not enough APs to deal at least two or three punches, 
							// reduce the attack value as one punch ain't much
							if (gGameOptions.fNewTraitSystem)
							{
								// if we are not specialized, reduce the attack attractiveness generally
								if (!HAS_SKILL_TRAIT(pSoldier, MARTIAL_ARTS_NT))
								{
									BestStab.iAttackValue /= 4;
									// if too far and not having APs for at least 3 hits no way to attack
									if (((CalcTotalAPsToAttack(pSoldier, BestStab.sTarget, ADDTURNCOST, 0) + (2 * (ApsToPunch(pSoldier)))) > pSoldier->actionPoints().current()) && !(PythSpacesAway(pSoldier->position().gridNo(), BestStab.sTarget) <= 1))
									{
										BestStab.ubPossible = 0;
										BestStab.iAttackValue = 0;
									}
								}
								else
								{
									if (PythSpacesAway(pSoldier->position().gridNo(), BestStab.sTarget) <= 1)
									{
										BestStab.iAttackValue = (BestStab.iAttackValue * 2);
									}
									// if too far and not having APs for at least 2 hits
									else if (((CalcTotalAPsToAttack(pSoldier, BestStab.sTarget, ADDTURNCOST, 0) + ApsToPunch(pSoldier)) > pSoldier->actionPoints().current()) && !(PythSpacesAway(pSoldier->position().gridNo(), BestStab.sTarget) <= 1))
									{
										BestStab.iAttackValue /= 3;
									}
								}
							}
							else
							{
								if (!HAS_SKILL_TRAIT(pSoldier, MARTIALARTS_OT) && !HAS_SKILL_TRAIT(pSoldier, HANDTOHAND_OT))
								{
									// if we are not specialized, reduce the attack attractiveness generally
									BestStab.iAttackValue /= 4;
									// if too far and not having APs for at least 3 hits
									if (((CalcTotalAPsToAttack(pSoldier, BestStab.sTarget, ADDTURNCOST, 0) + (2 * (ApsToPunch(pSoldier)))) > pSoldier->actionPoints().current()) && !(PythSpacesAway(pSoldier->position().gridNo(), BestStab.sTarget <= 1)))
									{
										BestStab.ubPossible = 0;
										BestStab.iAttackValue = 0;
									}
								}
								else
								{
									BestStab.iAttackValue = ((BestStab.iAttackValue * 3) / 2);

									if (PythSpacesAway(pSoldier->position().gridNo(), BestStab.sTarget) <= 1)
									{
										BestStab.iAttackValue = ((BestStab.iAttackValue * 3) / 2);
									}
									// if too far and not having APs for at least 2 hits
									else if (((CalcTotalAPsToAttack(pSoldier, BestStab.sTarget, ADDTURNCOST, 0) + ApsToPunch(pSoldier)) > pSoldier->actionPoints().current()) && !(PythSpacesAway(pSoldier->position().gridNo(), BestStab.sTarget <= 1)))
									{
										BestStab.iAttackValue /= 3;
									}
								}
							}

							// sevenfm: reduce HTH attack attractiveness depending on number of seen opponents
							if (pSoldier->awareness().opponentCount() > 1)
							{
								BestStab.iAttackValue /= pSoldier->awareness().opponentCount();
							}
						}						

						// now we KNOW FOR SURE that we will do something (stab, at least)
						NPCDoesAct(pSoldier);
						DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"NPC decided to punch");
					}

				}	
				// if it was in his holster, swap it back into his holster for now
				if (bWeaponIn != HANDPOS)
				{
					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"about to rearrange pocket after punch check");
					RearrangePocket(pSoldier,HANDPOS,bWeaponIn,TEMPORARILY);
				}
			}
		}
		/////////////////////////////////////////////////////////////////////////////////////////////////////

		//////////////////////////////////////////////////////////////////////////
		// CHOOSE THE BEST TYPE OF ATTACK OUT OF THOSE FOUND TO BE POSSIBLE
		//////////////////////////////////////////////////////////////////////////
		DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"CHOOSE THE BEST TYPE OF ATTACK OUT OF THOSE FOUND TO BE POSSIBLE");
		BestAttack.iAttackValue = 0;
		TacticalActor* bestStabOpponent =
			GetJa2SoldierRepository().resolve(BestStab.ubOpponent.i);
		if (BestStab.ubPossible && !bestStabOpponent)
		{
			BestStab.ubPossible = FALSE;
		}

		if (BestShot.ubPossible)
		{
			BestAttack.iAttackValue = BestShot.iAttackValue;
			ubBestAttackAction = AI_ACTION_FIRE_GUN;
			DebugAI(AI_MSG_INFO, pSoldier, String("best action = fire gun, iAttackValue = %d", BestAttack.iAttackValue));
		}

		// cautious boxer approach, reserve AP for two attacks (only if not attacking from the back)
		if (BestStab.ubPossible &&
			(pSoldier->status().flags() & SOLDIER_BOXER) &&
			SpacesAway(pSoldier->position().gridNo(), BestStab.sTarget) > 2 &&
			bestStabOpponent &&
			AIDirection(pSoldier->position().gridNo(), bestStabOpponent->position().gridNo()) != bestStabOpponent->position().direction() &&
			AIDirection(pSoldier->position().gridNo(), bestStabOpponent->position().gridNo()) != gOneCDirection[bestStabOpponent->position().direction()] &&
			AIDirection(pSoldier->position().gridNo(), bestStabOpponent->position().gridNo()) != gOneCCDirection[bestStabOpponent->position().direction()] &&
			pSoldier->actionPoints().initial() >= 2 * MinAPsToAttack(pSoldier, pSoldier->targeting().lastGridNo(), FALSE, 0, 0) + APBPConstants[AP_MOVEMENT_FLAT] + APBPConstants[AP_MODIFIER_WALK] &&
			pSoldier->actionPoints().current() < BestStab.ubAPCost + MinAPsToAttack(pSoldier, pSoldier->targeting().lastGridNo(), FALSE, 0, 0))
		{
			BestStab.ubPossible = FALSE;
			fTryPunching = FALSE;
			DebugAI(AI_MSG_INFO, pSoldier, String("boxer cannot reserve APs for second attack - disable stab attack"));
		}

		// try to avoid frontal attack
		if (BestStab.ubPossible &&
			(pSoldier->status().flags() & SOLDIER_BOXER) &&
			SpacesAway(pSoldier->position().gridNo(), BestStab.sTarget) > 1 &&
			bestStabOpponent &&
			gAnimControl[bestStabOpponent->animationPlayback().state()].ubEndHeight == ANIM_STAND &&
			bestStabOpponent->actionPoints().current() > 0 &&
			Chance(EffectiveAgility(bestStabOpponent, FALSE) * (100 + bestStabOpponent->vitals().breath()) * EffectiveWisdom(pSoldier) / (100 * 200)))
		{
			// find closest spot around opponent, avoid front direction
			UINT8	ubMovementCost;
			INT32	sTempGridNo;
			UINT8	ubDirection;
			INT32	sPathCost;
			INT32	sBestSpot = NOWHERE;
			INT32	sBestPathCost = 0;

			for (ubDirection = 0; ubDirection < NUM_WORLD_DIRECTIONS; ubDirection++)
			{
				sTempGridNo = NewGridNo(BestStab.sTarget, DirectionInc(ubDirection));

				if (sTempGridNo != BestStab.sTarget)
				{
					ubMovementCost = gubWorldMovementCosts[sTempGridNo][ubDirection][pSoldier->position().level()];

					if (ubMovementCost < TRAVELCOST_BLOCKED &&
						NewOKDestination(pSoldier, sTempGridNo, FALSE, pSoldier->position().level()) &&
						AIDirection(BestStab.sTarget, sTempGridNo) != bestStabOpponent->position().direction())
					{
						sPathCost = PlotPath(pSoldier, sTempGridNo, FALSE, FALSE, FALSE, DetermineMovementMode(pSoldier, AI_ACTION_GET_CLOSER), pSoldier->movement().stealthMode(), pSoldier->movement().reverse(), 0);
						if (TileIsOutOfBounds(sBestSpot) || sPathCost < sBestPathCost)
						{
							sBestSpot = sTempGridNo;
							sBestPathCost = sPathCost;
						}
					}
				}
			}

			if (!TileIsOutOfBounds(sBestSpot) &&
				pSoldier->actionPoints().current() >= sPathCost + GetAPsToLook(pSoldier) + MinAPsToAttack(pSoldier, pSoldier->targeting().lastGridNo(), FALSE, 0, 0))
			{
				pSoldier->aiPlanning().actionData() = sBestSpot;
				DebugAI(AI_MSG_INFO, pSoldier, String("boxer: get closer to opponent, avoid front direction"));
				return(AI_ACTION_GET_CLOSER);
			}
		}

		if (BestStab.ubPossible && ((BestStab.iAttackValue > BestAttack.iAttackValue) || (ubBestAttackAction == AI_ACTION_NONE)))
		{
			BestAttack.iAttackValue = BestStab.iAttackValue;
			if ( Item[ pSoldier->inventory()[BestStab.bWeaponIn].usItem ].usItemClass & IC_THROWING_KNIFE )
			{
				ubBestAttackAction = AI_ACTION_THROW_KNIFE;
				DebugAI(AI_MSG_INFO, pSoldier, String("best action = throw knife, iAttackValue = %d", BestAttack.iAttackValue));
			}
			else if ( Item[ pSoldier->inventory()[BestStab.bWeaponIn].usItem ].usItemClass & IC_BLADE ) // SANDRO - check specifically for blade attack
			{
				ubBestAttackAction = AI_ACTION_KNIFE_MOVE;
				DebugAI(AI_MSG_INFO, pSoldier, String("best action = move to stab, iAttackValue = %d", BestAttack.iAttackValue));
			}
			////////////////////////////////////////////////////////////////////////////////////
			// SANDRO - added a chance to try to steal merc's gun from hands
			else
			{
				if (AIDetermineStealingWeaponAttempt( pSoldier, bestStabOpponent ) == TRUE)
				{
					ubBestAttackAction = AI_ACTION_STEAL_MOVE;
					DebugAI(AI_MSG_INFO, pSoldier, String("best action = move to steal weapon, iAttackValue = %d", BestStab.iAttackValue));
				}
				else
				{
					ubBestAttackAction = AI_ACTION_KNIFE_MOVE;
					DebugAI(AI_MSG_INFO, pSoldier, String("best action = knife move, iAttackValue = %d", BestStab.iAttackValue));
				}
			}
			////////////////////////////////////////////////////////////////////////////////////
		}
		if ( BestThrow.ubPossible && ((BestThrow.iAttackValue > BestAttack.iAttackValue) || (ubBestAttackAction == AI_ACTION_NONE)) && !((ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier )) && ubBestAttackAction == AI_ACTION_FIRE_GUN && BestShot.ubChanceToReallyHit > 20 && Random( 2 )) )//dnl ch64 290813 tank always had better chance to fire from cannon so this will increase probabilty to use machinegun too
		{
			ubBestAttackAction = AI_ACTION_TOSS_PROJECTILE;
			DebugAI(AI_MSG_INFO, pSoldier, String("best action = throw something, iAttackValue = %d", BestThrow.iAttackValue));
		}

		if ( ( ubBestAttackAction == AI_ACTION_NONE ) && fTryPunching )
		{
			// nothing (else) to attack with so let's try hand-to-hand
			bWeaponIn = FindObj( pSoldier, NOTHING, HANDPOS, NUM_INV_SLOTS );

			if (bWeaponIn != NO_SLOT)
			{
				BestStab.bWeaponIn = bWeaponIn;
				// if it's in his holster, swap it into his hand temporarily
				if (bWeaponIn != HANDPOS)
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("swap knife into hand"));
					RearrangePocket(pSoldier,HANDPOS,bWeaponIn,TEMPORARILY);
				}

				// get the minimum cost to attack by HTH
				ubMinAPCost = MinAPsToAttack(pSoldier,pSoldier->targeting().lastGridNo(),DONTADDTURNCOST,0,0);

				// if we can afford the minimum AP cost to use HTH combat
				if (pSoldier->actionPoints().current() >= ubMinAPCost)
				{
					// then look around for a worthy target (which sets BestStab.ubPossible)
					CalcBestStab(pSoldier,&BestStab, FALSE);

					if (BestStab.ubPossible)
					{
						// now we KNOW FOR SURE that we will do something (stab, at least)
						NPCDoesAct(pSoldier);
						ubBestAttackAction = AI_ACTION_KNIFE_MOVE;
						DebugAI(AI_MSG_INFO, pSoldier, String("best action = move to stab, iAttackValue = %d", BestStab.iAttackValue));
					}
				}

				// if it was in his holster, swap it back into his holster for now
				if (bWeaponIn != HANDPOS)
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("put knife away"));
					RearrangePocket(pSoldier,HANDPOS,bWeaponIn,TEMPORARILY);
				}
			}
		}

		// copy the information on the best action selected into BestAttack struct
		DebugAI(AI_MSG_INFO, pSoldier, String("copy the information on the best action selected into BestAttack struct"));
		switch (ubBestAttackAction)
		{
		case AI_ACTION_FIRE_GUN:
			memcpy(&BestAttack,&BestShot,sizeof(BestAttack));
			DebugAI(AI_MSG_INFO, pSoldier, String("Best attack - shooting"));
			break;

		case AI_ACTION_TOSS_PROJECTILE:
			memcpy(&BestAttack,&BestThrow,sizeof(BestAttack));
			DebugAI(AI_MSG_INFO, pSoldier, String("Best attack - throwing grenade"));
			break;

		case AI_ACTION_THROW_KNIFE:
		case AI_ACTION_KNIFE_MOVE:
			DebugAI(AI_MSG_INFO, pSoldier, String("Best attack - stab"));
			memcpy(&BestAttack,&BestStab,sizeof(BestAttack));
			break;
		case AI_ACTION_STEAL_MOVE: // added by SANDRO
			DebugAI(AI_MSG_INFO, pSoldier, String("Best attack - steal weapon"));
			memcpy(&BestAttack,&BestStab,sizeof(BestAttack));
			break;

		default:
			// set to empty
			DebugAI(AI_MSG_INFO, pSoldier, String("Best attack - no good attack"));
			memset( &BestAttack, 0, sizeof( BestAttack ) );
			break;
		}
	}	

	UINT16 usRange = BestAttack.bWeaponIn==NO_SLOT ? 0 : GetModifiedGunRange(pSoldier->inventory()[BestAttack.bWeaponIn].usItem);//dnl ch69 150913
	INT32 sClosestThreat = ClosestKnownOpponent(pSoldier, NULL, NULL);

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Black Retreat]"));
	if (gfTurnBasedAI &&
		!bInWater &&
		ubCanMove &&
		!gfHiddenInterrupt &&
		!gTacticalStatus.fInterruptOccurred &&
		SoldierAI(pSoldier) &&
		pSoldier->aiBehavior().orders() != STATIONARY &&
		pSoldier->aiBehavior().orders() != SNIPER &&
		TacticalActorAiBehavior::retreatCounter(*pSoldier) > 0 &&
		(ubBestAttackAction == AI_ACTION_NONE || ubBestAttackAction == AI_ACTION_FIRE_GUN && (UINT8)BestAttack.ubChanceToReallyHit < Random(10 + TacticalActorConditions::suppressionShockPercent(*pSoldier) / 4)) &&
		(TacticalActorAiBehavior::hasInitialActionPoints(*pSoldier) || !AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo()) || pSoldier->suppression().underFire()))
	{
		DebugAI(AI_MSG_TOPIC, pSoldier, String("search for retreat spot"));
		INT32 sRetreatSpot = FindRetreatSpot(pSoldier);

		if (!TileIsOutOfBounds(sRetreatSpot))
		{
			DebugAI(AI_MSG_TOPIC, pSoldier, String("found retreat spot %d", sRetreatSpot));

			//BeginMultiPurposeLocator(sRetreatSpot, pSoldier->position().level(), FALSE);

			pSoldier->aiPlanning().actionData() = sRetreatSpot;
			return(AI_ACTION_TAKE_COVER);
		}
	}

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Black cover advance]"));
	// Black cover advance
	if (SoldierAI(pSoldier) &&
		gfTurnBasedAI &&
		!pSoldier->actionPoints().current() == pSoldier->actionPoints().initial() &&
		pSoldier->actionPoints().initial() > APBPConstants[AP_MINIMUM] &&
		!gfHiddenInterrupt &&
		!gTacticalStatus.fInterruptOccurred &&
		!InARoom(pSoldier->position().gridNo(), NULL) &&
		!TileIsOutOfBounds(sClosestOpponent) &&
		//!InARoom(pSoldier->sGridNo, NULL) &&
		pSoldier->aiBehavior().orders() != STATIONARY &&
		pSoldier->aiBehavior().orders() != ONGUARD &&
		!AICheckSpecialRole(pSoldier) &&
		(ubBestAttackAction == AI_ACTION_NONE || ubBestAttackAction == AI_ACTION_FIRE_GUN && BestAttack.ubChanceToReallyHit < 5 * RangeChangeDesire(pSoldier)) &&
		AIGunRange(pSoldier) < DAY_VISION_RANGE &&
		pSoldier->morale().aiMorale() >= MORALE_CONFIDENT &&
		(!AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo()) || !ProneSightCoverAtSpot(pSoldier, pSoldier->position().gridNo(), FALSE) || AIGunRange(pSoldier) < PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent)) &&
		PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent) < MAX_VISION_RANGE &&
		DetermineMovementMode(pSoldier, AI_ACTION_GET_CLOSER) != CRAWLING &&
		pSoldier->suppression().shock() < RangeChangeDesire(pSoldier) * 2 &&
		(AIGunRange(pSoldier) < PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent) ||
		pSoldier->combatResult().lastAttackHit() && pSoldier->targeting().lastGridNo() != NOWHERE ||
		pSoldier->morale().aiMorale() == MORALE_FEARLESS ||
		ubBestAttackAction == AI_ACTION_NONE ||
		ubBestAttackAction == AI_ACTION_FIRE_GUN && BestAttack.ubChanceToReallyHit == 1 ||
		!AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo())))
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("find cover advance spot"));

		INT32 sClosestDisturbance = ClosestReachableDisturbance(pSoldier, &fClimb);

		if (!TileIsOutOfBounds(sClosestDisturbance))
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("closest reachable disturbance %d", sClosestDisturbance));

			INT32 sAdvanceSpot = NOWHERE;

			DebugAI(AI_MSG_INFO, pSoldier, String("search for any cover advance spot"));
			sAdvanceSpot = FindAdvanceSpot(pSoldier, sClosestDisturbance, AI_ACTION_GET_CLOSER, ADVANCE_SPOT_ANY_COVER, FALSE);

			if (!TileIsOutOfBounds(sAdvanceSpot))
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("found cover advance spot %d", sAdvanceSpot));

				// check that we can reach desired location
				pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, sAdvanceSpot, 0, AI_ACTION_GET_CLOSER, 0);
				if (pSoldier->aiPlanning().actionData() == sAdvanceSpot)
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("cover advance spot ok"));
					pSoldier->aiPlanning().actionData() = sAdvanceSpot;

					//ScreenMsg(FONT_MCOLOR_LTGREEN, MSG_INTERFACE, L"[%d] found cover advance spot %d", pSoldier->identity().id(), sAdvanceSpot);
					//BeginMultiPurposeLocator(sAdvanceSpot, pSoldier->position().level(), FALSE);

					return AI_ACTION_GET_CLOSER;
				}
				else
				{
					DebugAI(AI_MSG_INFO, pSoldier, String("cannot reach cover advance spot!"));
				}
			}
			else
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("cannot find cover advance spot"));

				// try to use smoke to cover advance movement
				//gubNPCAPBudget = pSoldier->actionPoints().current();
				gubNPCAPBudget = 0;
				gubNPCDistLimit = 0;

				// check path to closest disturbance
				if (gfTurnBasedAI &&
					pSoldier->actionPoints().current() >= APBPConstants[AP_MINIMUM] &&
					pSoldier->actionPoints().current() == pSoldier->actionPoints().initial() &&
					!TileIsOutOfBounds(sClosestDisturbance) &&
					RangeChangeDesire(pSoldier) > 3 &&
					!AICheckIsSniper(pSoldier) &&
					!AICheckIsMachinegunner(pSoldier) &&
					pSoldier->aiBehavior().orders() != STATIONARY &&
					(pSoldier->suppression().underFire() ||
					pSoldier->suppression().shock() > 0 ||
					pSoldier->vitals().health() < pSoldier->vitals().maximumHealth() * 3 / 4 ||
					CountTeamUnderAttack(pSoldier->roster().team(), pSoldier->position().gridNo(), DAY_VISION_RANGE / 2) > CountNearbyFriends(pSoldier, pSoldier->position().gridNo(), DAY_VISION_RANGE / 2) / 2 ||
					CountSeenEnemiesLastTurn(pSoldier) > CountNearbyFriends(pSoldier, pSoldier->position().gridNo(), DAY_VISION_RANGE / 2)) &&
					(Chance(SoldierDifficultyLevel(pSoldier) * 10) || Chance(TeamPercentKilled(pSoldier->roster().team())) || Chance(CountTeamUnderAttack(pSoldier->roster().team(), pSoldier->position().gridNo(), DAY_VISION_RANGE / 2))) &&
					FindBestPath(pSoldier, sClosestDisturbance, pSoldier->position().level(), RUNNING, COPYROUTE, 0))
				{
					INT16 sLoop;
					INT32 sCoverSpot = NOWHERE;

					DebugAI(AI_MSG_INFO, pSoldier, String("found path to %d, path size %d ", sClosestDisturbance, pSoldier->pathing().pathSize()));

					sCheckGridNo = pSoldier->position().gridNo();

					for (sLoop = pSoldier->pathing().pathIndex(); sLoop < pSoldier->pathing().pathSize(); sLoop++)
					{
						sCheckGridNo = NewGridNo(sCheckGridNo, DirectionInc((UINT8)(pSoldier->pathing().path()[sLoop])));

						if (!TileIsOutOfBounds(sCheckGridNo) &&
							PythSpacesAway(pSoldier->position().gridNo(), sCheckGridNo) < TACTICAL_RANGE / 2 &&
							PythSpacesAway(pSoldier->position().gridNo(), sCheckGridNo) > TACTICAL_RANGE / 4 &&
							!Water(sCheckGridNo, pSoldier->position().level()) &&
							!InSmokeNearby(sCheckGridNo, pSoldier->position().level()) &&
							!InSmoke(sCheckGridNo, pSoldier->position().level()) &&
							(CorpseWarning(pSoldier, sCheckGridNo, pSoldier->position().level()) ||	InLightAtNight(sCheckGridNo, pSoldier->position().level())) &&
							SightCoverAtSpot(pSoldier, sCheckGridNo, FALSE))
						{
							CheckTossGrenadeAt(pSoldier, &BestThrow, sCheckGridNo, pSoldier->position().level(), EXPLOSV_SMOKE);

							if (BestThrow.ubPossible)
							{
								sCoverSpot = sCheckGridNo;
							}
						}
					}

					if (!TileIsOutOfBounds(sCoverSpot))
					{
						CheckTossGrenadeAt(pSoldier, &BestThrow, sCoverSpot, pSoldier->position().level(), EXPLOSV_SMOKE);

						if (BestThrow.ubPossible)
						{
							DebugAI(AI_MSG_INFO, pSoldier, String("prepare throw at spot %d level %d aimtime %d", BestThrow.sTarget, BestThrow.bTargetLevel, BestThrow.ubAimTime));

							// if necessary, swap the usItem from holster into the hand position
							if (BestThrow.bWeaponIn != HANDPOS)
							{
								DebugAI(AI_MSG_INFO, pSoldier, String("rearrange pocket"));
								RearrangePocket(pSoldier, HANDPOS, BestThrow.bWeaponIn, FOREVER);
							}

							// stand up before throwing if needed
							if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight < BestThrow.ubStance &&
								TacticalActorMobility::isValidStance(*pSoldier, AIDirection(pSoldier->position().gridNo(), BestThrow.sTarget), BestThrow.ubStance))
							{
								pSoldier->aiPlanning().actionData() = BestThrow.ubStance;
								pSoldier->aiPlanning().nextAction() = AI_ACTION_TOSS_PROJECTILE;
								pSoldier->aiPlanning().nextActionData() = BestThrow.sTarget;
								pSoldier->aiPlanning().nextTargetLevel() = BestThrow.bTargetLevel;
								pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;
								return AI_ACTION_CHANGE_STANCE;
							}

							pSoldier->aiPlanning().actionData() = BestThrow.sTarget;
							pSoldier->targeting().level() = BestThrow.bTargetLevel;
							pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;

							return(AI_ACTION_TOSS_PROJECTILE);
						}
					}
				}
				gubNPCAPBudget = 0;
			}
		}
	}

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Allow taking cover]"));
	if ( (pSoldier->actionPoints().current() == pSoldier->actionPoints().initial()) &&
		 (ubBestAttackAction == AI_ACTION_FIRE_GUN) && 
		 (pSoldier->suppression().shock() == 0) &&
		 (pSoldier->vitals().health() >= pSoldier->vitals().maximumHealth() / 2) &&
		 (BestAttack.ubChanceToReallyHit < 30) && 
		 (PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) > usRange / CELL_X_SIZE ) &&
		 (RangeChangeDesire( pSoldier ) >= 4) )
	{
		// okay, really got to wonder about this... could taking cover be an option?
		if (ubCanMove && pSoldier->aiBehavior().orders() != STATIONARY && !gfHiddenInterrupt &&
			!(pSoldier->status().flags() & SOLDIER_BOXER) )
		{
			// make militia a bit more cautious
			// 3 (UINT16) CONVERSIONS HERE TO AVOID ERRORS.  GOTTHARD 7/15/08
			if (pSoldier->roster().team() == MILITIA_TEAM && (INT16)(PreRandom(20)) > BestAttack.ubChanceToReallyHit ||
				pSoldier->roster().team() != MILITIA_TEAM && (INT16)(PreRandom(40)) > BestAttack.ubChanceToReallyHit)
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("Allow cover check"));
				// maybe taking cover would be better!
				fAllowCoverCheck = TRUE;

				sBestCover = FindBestNearbyCover(pSoldier, pSoldier->morale().aiMorale(), &iCoverPercentBetter);
				if ( (INT16)(PreRandom( 10 )) > BestAttack.ubChanceToReallyHit &&
					!TileIsOutOfBounds(sBestCover) &&
					(iCoverPercentBetter > 10 || !AnyCoverAtSpot(pSoldier, pSoldier->position().gridNo())))
				{
					DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"DecideActionBlack: can't hit so screw the attack");
					DebugAI(AI_MSG_INFO, pSoldier, String("can't hit, screw the attack"));
					// screw the attack!
					ubBestAttackAction = AI_ACTION_NONE;
				}
			}
		}

	}

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"LOOK FOR SOME KIND OF COVER BETTER THAN WHAT WE HAVE NOW");
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Find cover]"));
	////////////////////////////////////////////////////////////////////////////
	// LOOK FOR SOME KIND OF COVER BETTER THAN WHAT WE HAVE NOW
	////////////////////////////////////////////////////////////////////////////

	// if soldier has enough APs left to move at least 1 square's worth,
	// and either he can't attack any more, or his attack did wound someone
	iCoverPercentBetter = 0;
	
	if ( (ubCanMove && !SkipCoverCheck && !gfHiddenInterrupt &&
		((ubBestAttackAction == AI_ACTION_NONE) || pSoldier->combatResult().lastAttackHit()) &&
		(pSoldier->roster().team() != gbPlayerNum || pSoldier->aiBehavior().flags() & AI_RTP_OPTION_CAN_SEEK_COVER) &&
		!(pSoldier->status().flags() & SOLDIER_BOXER) )
		|| fAllowCoverCheck )
	{
		// sevenfm: if not found yet
		if(TileIsOutOfBounds(sBestCover))
		{
			sBestCover = FindBestNearbyCover(pSoldier, pSoldier->morale().aiMorale(), &iCoverPercentBetter);
		}		
		// DetermineMovementMode can consume the deterministic RNG (Random) for some
		// bodytypes, so evaluate it unconditionally into a local: the DebugAI macro skips
		// its argument evaluation when logging is off, which must not drop the RNG draw.
		UINT16 usDbgMoveMode = DetermineMovementMode(pSoldier, AI_ACTION_TAKE_COVER);
		DebugAI(AI_MSG_INFO, pSoldier, String("Found cover spot %d percent better %d movement mode %d", sBestCover, iCoverPercentBetter, usDbgMoveMode));
	}


#ifdef RETREAT_TESTING
	sBestCover = NOWHERE;
#endif

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"DecideActionBlack: DECIDE BETWEEN ATTACKING AND DEFENDING (TAKING COVER)");
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Decide attack/cover]"));
	//////////////////////////////////////////////////////////////////////////
	// IF NECESSARY, DECIDE BETWEEN ATTACKING AND DEFENDING (TAKING COVER)
	//////////////////////////////////////////////////////////////////////////

	// if both are possible	
	if ((ubBestAttackAction != AI_ACTION_NONE) && ( !TileIsOutOfBounds(sBestCover)))
	{
		// gotta compare their merits and select the more desirable option
		iOffense = BestAttack.ubChanceToReallyHit;
		iDefense = iCoverPercentBetter;

		// based on how we feel about the situation, decide whether to attack first
		switch (pSoldier->morale().aiMorale())
		{
		case MORALE_FEARLESS:
			iOffense += iOffense / 2;	// increase 50%
			break;

		case MORALE_CONFIDENT:
			iOffense += iOffense / 4;	// increase 25%
			break;

		case MORALE_NORMAL:
			break;

		case MORALE_WORRIED:
			iDefense += iDefense / 4;	// increase 25%
			break;

		case MORALE_HOPELESS:
			iDefense += iDefense / 2;	// increase 50%
			break;
		}


		// smart guys more likely to try to stay alive, dolts more likely to shoot!
		if (pSoldier->statistics().wisdom() >= 50) //Madd: reduced the wisdom required to want to live...
			iDefense += 10;
		else if (pSoldier->statistics().wisdom() < 30)
			iDefense -= 10;

		// some orders are more offensive, others more defensive
		if (pSoldier->aiBehavior().orders() == SEEKENEMY)
			iOffense += 10;
		else if ((pSoldier->aiBehavior().orders() == STATIONARY) || (pSoldier->aiBehavior().orders() == ONGUARD) || pSoldier->aiBehavior().orders() == SNIPER )
			iDefense += 10;

		switch (pSoldier->aiBehavior().attitude())
		{
		case DEFENSIVE:		iDefense += 30; break;
		case BRAVESOLO:		iDefense -= 0; break;
		case BRAVEAID:			iDefense -= 0; break;
		case CUNNINGSOLO:	iDefense += 20; break;
		case CUNNINGAID:		iDefense += 20; break;
		case AGGRESSIVE:		iOffense += 10; break;
		case ATTACKSLAYONLY:iOffense += 30; break;
		}

#ifdef DEBUGDECISIONS
		STR tempstr="";
		sprintf( tempstr, "%s - CHOICE: iOffense = %d, iDefense = %d\n",
			pSoldier->identity().name(),iOffense,iDefense);
		DebugAI( tempstr );
#endif

		DebugAI(AI_MSG_INFO, pSoldier, String("iOffense %d iDefense %d", iOffense, iDefense));

		// if his defensive instincts win out, forget all about the attack
		if (iDefense > iOffense)
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("[decided taking cover, disable attack]"));
			ubBestAttackAction = AI_ACTION_NONE;
		}
	}

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,String("DecideActionBlack: is attack still desirable?  ubBestAttackAction = %d",ubBestAttackAction));
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Attack]"));

	// if attack is still desirable (meaning it's also preferred to taking cover)
	if (ubBestAttackAction != AI_ACTION_NONE)
	{
		DebugAI(AI_MSG_TOPIC, pSoldier, String("[Prepare attack]"));
		// if we wanted to be REALLY mean, we could look at chance to hit and decide whether
		// to shoot at the head...

		// default settings
		//POSSIBLE STRUCTURE CHANGE PROBLEM, NOT CURRENTLY CHANGED. GOTTHARD 7/14/08		
		pSoldier->aiPlanning().aimTime() = BestAttack.ubAimTime;
		pSoldier->attackSelection().scopeMode() = BestAttack.bScopeMode;
		pSoldier->fireControl().burstCounter()			= 0;

		// HEADROCK HAM 3.6: bAimTime represents how MANY aiming levels are used, not how much APs they cost necessarily.
		INT16 sActualAimAP = CalcAPCostForAiming( pSoldier, BestAttack.sTarget, (INT8)pSoldier->aiPlanning().aimTime() );

		if (ubBestAttackAction == AI_ACTION_FIRE_GUN)
		{
			DebugAI(AI_MSG_TOPIC, pSoldier, String("[Prepare shooting]"));

			//////////////////////////////////////////////////////////////////////////
			// IF ENOUGH APs TO BURST, RANDOM CHANCE OF DOING SO
			//////////////////////////////////////////////////////////////////////////

			if (IsGunBurstCapable( &pSoldier->inventory()[BestAttack.bWeaponIn], FALSE, pSoldier ) &&
				bestShotOpponent &&
				!(bestShotOpponent->vitals().health() < OKLIFE) && // don't burst at downed targets
				pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft > 1 &&
				(pSoldier->roster().team() != gbPlayerNum || pSoldier->aiBehavior().realtimeCombat() == RTP_COMBAT_AGGRESSIVE) )
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("enough APs to burst, random chance of doing so"));

				ubBurstAPs = CalcAPsToBurst( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestAttack.bWeaponIn]), pSoldier );

				// HEADROCK HAM 3.6: Use Actual Aiming Time.
				if (pSoldier->actionPoints().current() >= BestAttack.ubAPCost + sActualAimAP + ubBurstAPs )
				{
					// Base chance of bursting is 25% if best shot was +0 aim, down to 8% at +4
					if ( ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier ) )
					{
						iChance = 100;
					}
					else
					{
						iChance = (25 / max((BestAttack.ubAimTime + 1),1));
						switch (pSoldier->aiBehavior().attitude())
						{
							case DEFENSIVE:		iChance += -5; break;
							case BRAVESOLO:		iChance +=  5; break;
							case BRAVEAID:		iChance +=  5; break;
							case CUNNINGSOLO:	iChance +=  0; break;
							case CUNNINGAID:	iChance +=  0; break;
							case AGGRESSIVE:	iChance += 10; break;
							case ATTACKSLAYONLY:iChance += 30; break;
						}

						// SANDRO: more likely to burst when firing from hip
						if ( BestAttack.bScopeMode == USE_ALT_WEAPON_HOLD && ItemIsTwoHanded(pSoldier->inventory()[BestAttack.bWeaponIn].usItem) )
							iChance += 40;

						// CHRISL: Changed from a simple flag to two externalized values for more modder control over AI suppression
						if ( GetMagSize(&pSoldier->inventory()[BestAttack.bWeaponIn], 0) >= gGameExternalOptions.ubAISuppressionMinimumMagSize &&
							pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft >= gGameExternalOptions.ubAISuppressionMinimumAmmo )
							iChance += 20;

						// increase chance based on proximity and difficulty of enemy
						if ( PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) < 15 )
						{
							DebugMsg(TOPIC_JA2AI,DBG_LEVEL_3,String("DecideActionBlack: check chance to burst"));
							iChance += ( 15 - PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) ) * ( 1 + SoldierDifficultyLevel( pSoldier ) );
							if ( pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY )
							{
								// increase it more!
								iChance += 5 * ( 15 - PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) );
							}
						}

						// HEADROCK HAM 3.6: due to the "else", this part of the formula is NEVER hit. Removing.
						//else if (PythSpacesAway( pSoldier->sGridNo, BestAttack.sTarget ) < 10 && gGameOptions.ubDifficultyLevel > DIF_LEVEL_EASY )
						if (PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) < 10 && gGameOptions.ubDifficultyLevel > DIF_LEVEL_EASY )
						{
							iChance += 100;
						}
					}

					if ( (INT32) PreRandom( 100 ) < iChance)
					{
						BestAttack.ubAPCost += ubBurstAPs + sActualAimAP;//dnl ch58 130913
						// check for spread burst possibilities
						if (pSoldier->aiBehavior().attitude() != ATTACKSLAYONLY)
						{
							CalcSpreadBurst( pSoldier, BestAttack.sTarget, BestAttack.bTargetLevel );
						}
						//dnl ch58 130913 return aiming for burst
						pSoldier->fireControl().selectBurst();
					}
				}
			}

			if (IsGunAutofireCapable( &pSoldier->inventory()[BestAttack.bWeaponIn] ) &&
				bestShotOpponent &&
				!(bestShotOpponent->vitals().health() < OKLIFE) && // don't burst at downed targets
				(( pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft > 1 &&
				!pSoldier->fireControl().burstCounter() ) || Weapon[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].NoSemiAuto) )
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("enough APs to autofire, random chance of doing so"));
L_NEWAIM:
				FLOAT dTotalRecoil = 0.0f;
				pSoldier->fireControl().autofireShots() = 0;
				if(UsingNewCTHSystem() == true)
				{
					do
					{
						pSoldier->fireControl().autofireShots()++;
						dTotalRecoil += AICalcRecoilForShot( pSoldier, &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots() );
						ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier );
					}
					while(	pSoldier->actionPoints().current() >= BestShot.ubAPCost + ubBurstAPs + sActualAimAP && pSoldier->inventory()[ BestAttack.bWeaponIn ][0]->data.gun.ubGunShotsLeft >= pSoldier->fireControl().autofireShots() && dTotalRecoil <= 10.0f );//dnl ch64 260813 pSoldier->attackSelection().hand() is wrong because decision is to use BestAttack.bWeaponIn
				} 
				else 
				{
					do
					{
						pSoldier->fireControl().autofireShots()++;
						ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestAttack.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier );
					}
					while(	pSoldier->actionPoints().current() >= BestAttack.ubAPCost + ubBurstAPs + sActualAimAP && pSoldier->inventory()[ BestAttack.bWeaponIn ][0]->data.gun.ubGunShotsLeft >= pSoldier->fireControl().autofireShots() && GetAutoPenalty(&pSoldier->inventory()[ BestAttack.bWeaponIn ], gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_PRONE)*pSoldier->fireControl().autofireShots() <= 80);//dnl ch64 130913 pSoldier->attackSelection().hand() is wrong because decision is to use BestAttack.bWeaponIn, also missing sActualAimTime
				}

				pSoldier->fireControl().autofireShots()--;

				DebugAI(AI_MSG_INFO, pSoldier, String("autofire %d", pSoldier->fireControl().autofireShots()));

				//dnl ch69 130913 let try increase autofire rate for aim cost
				// sevenfm: LIMIT_MAX_DEVIATION option increases effectiveness of suppression
				if ((!UsingNewCTHSystem() || gGameCTHConstants.LIMIT_MAX_DEVIATION) &&
					pSoldier->fireControl().autofireShots() < 3 &&
					pSoldier->aiPlanning().aimTime() > 0 &&
					pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft >= 3 &&
					Chance(gGameExternalOptions.sSuppressionEffectiveness) &&
					(!gGameExternalOptions.fAISafeSuppression || CheckSuppressionDirection(pSoldier, BestShot.sTarget, BestShot.bTargetLevel)))
				{
					pSoldier->aiPlanning().aimTime()--;
					sActualAimAP = CalcAPCostForAiming(pSoldier, BestAttack.sTarget, (INT8)pSoldier->aiPlanning().aimTime());
					DebugAI(AI_MSG_INFO, pSoldier, String("reduce aim to %d, recalc autofire, aim AP %d", pSoldier->aiPlanning().aimTime(), sActualAimAP));
					goto L_NEWAIM;
				}

				if (pSoldier->fireControl().autofireShots() > 0)
				{
					ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestAttack.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier );

					if (pSoldier->actionPoints().current() >= BestAttack.ubAPCost + sActualAimAP + ubBurstAPs )
					{
						// Base chance of bursting is 25% if best shot was +0 aim, down to 8% at +4
						if ( ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier ) )
						{
							iChance = 100;
						}
						else
						{
							iChance = (100 / max((BestAttack.ubAimTime + 1),1));
							switch (pSoldier->aiBehavior().attitude())
							{
							case DEFENSIVE:		iChance += -5; break;
							case BRAVESOLO:		iChance +=  5; break;
							case BRAVEAID:		iChance +=  5; break;
							case CUNNINGSOLO:	iChance +=  0; break;
							case CUNNINGAID:	iChance +=  0; break;
							case AGGRESSIVE:	iChance += 10; break;
							case ATTACKSLAYONLY:iChance += 30; break;
							}

							// SANDRO: more likely to burst when firing from hip
							if ( BestAttack.bScopeMode == USE_ALT_WEAPON_HOLD && ItemIsTwoHanded(pSoldier->inventory()[BestAttack.bWeaponIn].usItem) )
								iChance += 40;

							// CHRISL: Changed from a simple flag to two externalized values for more modder control over AI suppression
							if ( GetMagSize(&pSoldier->inventory()[BestAttack.bWeaponIn], 0) >= gGameExternalOptions.ubAISuppressionMinimumMagSize &&
								pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft >= gGameExternalOptions.ubAISuppressionMinimumAmmo )
								iChance += 30;

							if ( bInGas )
								iChance += 50; //Madd: extra chance of going nuts and autofiring if stuck in gas

							// increase chance based on proximity and difficulty of enemy
							if ( PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) < 15 )
							{
								DebugMsg(TOPIC_JA2AI,DBG_LEVEL_3,String("DecideActionBlack: check chance to autofire"));
								iChance += ( 15 - PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) ) * ( 1 + SoldierDifficultyLevel( pSoldier ) );
								if ( pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY )
								{
									// increase it more!
									iChance += 5 * ( 15 - PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) );
								}
							}
							// HEADROCK HAM 3.6: Forcing enemies to autofire at close range if possible, similar to forced burst (see above)
							if (PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) < 10 && gGameOptions.ubDifficultyLevel > DIF_LEVEL_EASY )
							{
								iChance += 100;
							}
						}

						DebugAI(AI_MSG_INFO, pSoldier, String("chance for autofire %d", iChance));

						if ((INT32) PreRandom( 100 ) < iChance || Weapon[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].NoSemiAuto)
						{
							//dnl ch69 140913 return aiming for autofire with halfautofire fix
							pSoldier->fireControl().burstCounter() = 1;
							INT16 ubHalfBurstAPs = 256;
							if (pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft < 4)
							{
								iChance = 0;
							}
							else
							{
								ubHalfBurstAPs = CalcAPsToAutofire(TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &pSoldier->inventory()[BestAttack.bWeaponIn], 2, pSoldier);
								
								if (!CheckSuppressionDirection(pSoldier, BestAttack.sTarget, BestAttack.bTargetLevel))
									iChance = 100;
								else
									iChance = BestAttack.ubChanceToReallyHit / 2;

								if (Weapon[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].NoSemiAuto || pSoldier->awareness().opponentCount() > 1)
									iChance += (100 - iChance) / 2;
							}

							if(Chance(iChance) && pSoldier->actionPoints().current() >= (2 * BestAttack.ubAPCost + ubHalfBurstAPs + sActualAimAP))
							{
								// Try short autofire to enhance chance of hitting
								pSoldier->fireControl().autofireShots() = 2;
								BestAttack.ubAPCost += ubHalfBurstAPs + sActualAimAP;
							}
							else
							{
								BestAttack.ubAPCost += ubBurstAPs + sActualAimAP;
							}
						}
						else
						{
							pSoldier->fireControl().selectSingleShot();
						}
					}
				}
			}

			if (!pSoldier->fireControl().burstCounter())
			{
				pSoldier->aiPlanning().aimTime()	= BestAttack.ubAimTime;
				pSoldier->fireControl().burstCounter()			= 0;
				pSoldier->fireControl().autofireShots()		= 0;
			}

			// IF WAY OUT OF EFFECTIVE RANGE TRY TO ADVANCE RESERVING ENOUGH AP FOR A SHOT IF NOT ACTED YET
			if ((pSoldier->actionPoints().current() > BestAttack.ubAPCost) &&
				bestShotOpponent &&
				(pSoldier->suppression().shock() == 0) &&
				(pSoldier->vitals().health() >= pSoldier->vitals().maximumHealth() / 2) &&
				(BestAttack.ubChanceToReallyHit < 8) &&
				(PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) > usRange / CELL_X_SIZE ) &&
				(RangeChangeDesire( pSoldier ) >= 3) ) // Cunning and above
			{
				sClosestOpponent = bestShotOpponent->position().gridNo();

				DebugAI(AI_MSG_INFO, pSoldier, String("check if can advance to closest opponent %d", sClosestOpponent));

				if (!TileIsOutOfBounds(sClosestOpponent))
				{
					// temporarily make merc get closer reserving enough for expected cost of shot
					USHORT tgrd = pSoldier->aiPlanning().patrolGrid()[0];
					INT8 oldOrders = pSoldier->aiBehavior().orders();
					pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
					pSoldier->aiBehavior().orders() = CLOSEPATROL;
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sClosestOpponent, BestAttack.ubAPCost, AI_ACTION_GET_CLOSER, 0 );
					pSoldier->aiPlanning().patrolGrid()[0] = tgrd;
					pSoldier->aiBehavior().orders() = oldOrders;

					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						pSoldier->aiPlanning().nextAction() = AI_ACTION_FIRE_GUN;
						pSoldier->aiPlanning().nextActionData() = BestAttack.sTarget;
						pSoldier->aiPlanning().nextTargetLevel() = BestAttack.bTargetLevel;

						DebugAI(AI_MSG_INFO, pSoldier, String("try to get closer before shooting, move to %d", pSoldier->aiPlanning().actionData()));
						return( AI_ACTION_GET_CLOSER );
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// IF NOT CROUCHED & WILL STILL HAVE ENOUGH APs TO DO THIS SAME BEST
			// ATTACK AFTER A STANCE CHANGE, CONSIDER CHANGING STANCE
			//////////////////////////////////////////////////////////////////////////



			// HEADROCK HAM 4: No longer necessary to do here. The conditions above already handle this, specifically
			// WITHOUT messing with the BestAttack.ubAimTime variable, since that can apply now even when bursting
			// or autofiring!!
			/*
			if (BestAttack.ubAimTime == BURSTING)
			{
				pSoldier->aiPlanning().aimTime()			= 0;
				pSoldier->fireControl().burstCounter()			= 1;
				pSoldier->fireControl().autofireShots()		= 0;
			}
			else if(BestAttack.ubAimTime >= AUTOFIRING)
			{
				pSoldier->aiPlanning().aimTime()			= 0;
				pSoldier->fireControl().burstCounter()			= 1;
				pSoldier->fireControl().autofireShots()		= BestAttack.ubAimTime-AUTOFIRING;

				BestAttack.ubAimTime = AUTOFIRING;
			}*/

			/*
			else // defaults already set
			{
			pSoldier->aiPlanning().aimTime()			= BestAttack.ubAimTime;
			pSoldier->fireControl().burstCounter()			= 0;
			}
			*/

		}
		else if (ubBestAttackAction == AI_ACTION_THROW_KNIFE)
		{
			DebugAI(AI_MSG_TOPIC, pSoldier, String("[Prepare throwing knife]"));

			if (BestAttack.bWeaponIn != HANDPOS && gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_STAND )
			{
				// we had better make sure we lower our gun first!

				pSoldier->aiPlanning().action() = AI_ACTION_LOWER_GUN;
				pSoldier->aiPlanning().actionData() = 0;

				// queue up attack for after we lower weapon if any
				pSoldier->aiPlanning().nextAction() = AI_ACTION_THROW_KNIFE;
				pSoldier->aiPlanning().nextActionData() = BestAttack.sTarget;
				pSoldier->aiPlanning().nextTargetLevel() = BestAttack.bTargetLevel;
			}

		}
		// SANDRO - chance to make aimed punch/stab for martial arts/melee 
		else if (ubBestAttackAction == AI_ACTION_KNIFE_MOVE && gGameOptions.fNewTraitSystem)
		{
			DebugAI(AI_MSG_TOPIC, pSoldier, String("[Prepare knife attack]"));

			pSoldier->aiPlanning().aimTime() = 0;
			iChance = 0;

			if (Item[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].usItemClass == IC_PUNCH)
			{
				if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
					iChance += 30;
				if (HAS_SKILL_TRAIT( pSoldier, MARTIAL_ARTS_NT) )
					iChance += 30 * NUM_SKILL_TRAITS( pSoldier, MARTIAL_ARTS_NT);

				if( (INT32)PreRandom( 100 ) <= iChance )
				{
					pSoldier->aiPlanning().aimTime() = (gGameExternalOptions.fEnhancedCloseCombatSystem ? gSkillTraitValues.ubModifierForAPsAddedOnAimedPunches : 6);
				}
			}
			else
			{
				if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
					iChance += 30;
				if (HAS_SKILL_TRAIT( pSoldier, MELEE_NT))
					iChance += 30;

				if( (INT32)PreRandom( 100 ) <= iChance )
				{
					pSoldier->aiPlanning().aimTime() = (gGameExternalOptions.fEnhancedCloseCombatSystem ? gSkillTraitValues.ubModifierForAPsAddedOnAimedBladedAttackes : 6);
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// OTHERWISE, JUST GO AHEAD & ATTACK!
		//////////////////////////////////////////////////////////////////////////
		DebugAI(AI_MSG_TOPIC, pSoldier, String("Attack!"));

		//dnl ch64 270813 must be as below RearrangePocket with FOREVER will screw already decided BURST or AUTOFIRE
		INT8 bDoBurst = pSoldier->fireControl().burstCounter();
		UINT8 bDoAutofire = pSoldier->fireControl().autofireShots();
		// swap weapon to hand if necessary
		if (BestAttack.bWeaponIn != HANDPOS)
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("swap weapon into hand from %d", BestAttack.bWeaponIn));
			RearrangePocket(pSoldier,HANDPOS,BestAttack.bWeaponIn,FOREVER);
		}

		if(ubBestAttackAction == AI_ACTION_FIRE_GUN && bDoBurst == 1)//dnl ch64 270813
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("using burst/autofire"));

			pSoldier->fireControl().autofireShots() = bDoAutofire;
			pSoldier->fireControl().burstCounter() = bDoBurst;
			if(bDoAutofire > 1)
				pSoldier->attackSelection().weaponMode() = WM_AUTOFIRE;
			else
				pSoldier->attackSelection().weaponMode() = WM_BURST;
		}

		DebugAI(AI_MSG_INFO, pSoldier, String("prepare attack at target %d level %d aim %d ap %d cth %d opponent %d", BestAttack.sTarget, BestAttack.bTargetLevel, BestAttack.ubAimTime, BestAttack.ubAPCost, BestAttack.ubChanceToReallyHit, BestAttack.ubOpponent));

		if (ubBestAttackAction == AI_ACTION_FIRE_GUN)
		{
			if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight != BestAttack.ubStance  &&
				IsValidStance(pSoldier, BestAttack.ubStance))
			{
				pSoldier->aiPlanning().nextAction() = AI_ACTION_FIRE_GUN;
				pSoldier->aiPlanning().nextActionData() = BestAttack.sTarget;
				pSoldier->aiPlanning().nextTargetLevel() = BestAttack.bTargetLevel;
				pSoldier->aiPlanning().actionData() = BestAttack.ubStance;

				DebugAI(AI_MSG_INFO, pSoldier, String("Change stance before shooting"));
				return(AI_ACTION_CHANGE_STANCE);
			}
			else
			{
				pSoldier->aiPlanning().actionData() = BestAttack.sTarget;
				pSoldier->targeting().level() = BestAttack.bTargetLevel;
				return(AI_ACTION_FIRE_GUN);
			}
		}
		else if (ubBestAttackAction == AI_ACTION_TOSS_PROJECTILE)
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("toss attack, disable burst/autofire"));
			pSoldier->fireControl().selectSingleShot();

			if (IsGrenadeLauncherAttached(&pSoldier->inventory()[HANDPOS]))	//dnl ch63 240813
			{
				DebugAI(AI_MSG_INFO, pSoldier, String("using attached GL"));
					pSoldier->attackSelection().weaponMode() = WM_ATTACHED_GL;
				}

				// stand up before throwing if needed
				if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight < BestAttack.ubStance &&
					TacticalActorMobility::isValidStance(*pSoldier, AIDirection(pSoldier->position().gridNo(), BestAttack.sTarget), BestAttack.ubStance))
				{
					pSoldier->aiPlanning().actionData() = BestAttack.ubStance;
					pSoldier->aiPlanning().nextAction() = AI_ACTION_TOSS_PROJECTILE;
					pSoldier->aiPlanning().nextActionData() = BestAttack.sTarget;
					pSoldier->aiPlanning().nextTargetLevel() = BestAttack.bTargetLevel;
					return AI_ACTION_CHANGE_STANCE;
				}
				else
				{
					pSoldier->aiPlanning().actionData() = BestAttack.sTarget;
					pSoldier->targeting().level() = BestAttack.bTargetLevel;
				return(AI_ACTION_TOSS_PROJECTILE);
			}
		}
		// other attacks
		else
		{
			pSoldier->aiPlanning().actionData() = BestAttack.sTarget;
			pSoldier->targeting().level() = BestAttack.bTargetLevel;
			return(ubBestAttackAction);
		}
	}

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[End of Tank AI]"));
	// end of tank AI
	if ( !gGameExternalOptions.fEnemyTanksCanMoveInTactical && ARMED_VEHICLE( pSoldier ) )
	{
		return( AI_ACTION_NONE );
	}

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Window jump]"));
	// get the location of the closest reachable opponent
	/*	Flugente 22.02.2012 - A few clarifications: I changed ClosestSeenOpponent so that for zombies, this function also returns an opponent if he is on the
	*	roof of a building, we are not, but our GridNo belongs to that same building. 
	*	If that is the case, it is clear that we have to get on that roof. However, we cannot do that in BlackState. If, by pure chance, we can still see our
	*	enemy, we cannot climb (there is  no climbing option in BlackState sofar).
	*	So, I changed the code so that now we will climb the roof.
	*/
	INT32	targetGridNo = -1;
	INT8	targetbLevel =  0;
	sClosestOpponent = ClosestSeenOpponentWithRoof(pSoldier, &targetGridNo, &targetbLevel);
	if ( !TileIsOutOfBounds(sClosestOpponent) && !TileIsOutOfBounds(targetGridNo) && SameBuilding( pSoldier->position().gridNo(), targetGridNo ) )
	{
		if ( targetbLevel == pSoldier->position().level() && targetbLevel == 0 )
		{
			//////////////////////////////////////////////////////////////////////
			// GO DIRECTLY TOWARDS CLOSEST KNOWN OPPONENT
			//////////////////////////////////////////////////////////////////////

			// try to move towards him
			pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards(pSoldier,sClosestOpponent,AI_ACTION_GET_CLOSER);
				
			// Flugente: if on the same level and there is a jumpable window here, jump through it
			if ( gGameExternalOptions.fCanJumpThroughWindows && !ENEMYROBOT(pSoldier) )
			{
				// determine if there is a jumpable window in the direction to our target
				// if yes, and we are not facing it, face it now
				// if yes, and we are facing it, jump
				// if no, go on, nothing to see here
				// determine direction of our target
				INT8 targetdirection = (INT8)GetDirectionToGridNoFromGridNo(pSoldier->position().gridNo(), sClosestOpponent);

				// determine if there is a jumpable window here, in the direction of our target
				// store old direction for this check
				UINT8 tmpdirection = pSoldier->position().direction();
				pSoldier->position().direction() = targetdirection;

				INT8 windowdirection = DIRECTION_IRRELEVANT;
				if ( FindWindowJumpDirection(pSoldier, pSoldier->position().gridNo(), pSoldier->position().direction(), &windowdirection) && targetdirection == windowdirection )
				{
					pSoldier->position().direction() = tmpdirection;

					// are we already looking in that direction?
					if ( pSoldier->position().direction() == targetdirection )
					{
						// jump through the window
						return(AI_ACTION_JUMP_WINDOW);
					}
					else
					{
						// look into that direction
						if ( TacticalActorMobility::isCurrentStanceValid(*pSoldier, targetdirection) )
						{
							pSoldier->aiPlanning().actionData() = targetdirection;
							return(AI_ACTION_CHANGE_FACING);
						}

					}
				}

				pSoldier->position().direction() = tmpdirection;
			}
		}
		// The situation mentioned above happens...
		else
		{
			// need to climb AND have enough APs to get there this turn
			BOOLEAN fUp = TRUE;
			if (pSoldier->position().level() > 0 )
				fUp = FALSE;

			if ( !ENEMYROBOT(pSoldier) && (pSoldier->actionPoints().current() > GetAPsToClimbRoof ( pSoldier, fUp )) )
			{
				pSoldier->aiPlanning().actionData() = targetGridNo;//FindClosestClimbPoint(pSoldier, fUp );

				// Necessary test: can we climb up at this position? It might happen that our target is directly above us, then we'll have to move
				INT8 newdirection;
				if ( ( fUp && FindHeigherLevel( pSoldier, pSoldier->position().gridNo(), pSoldier->position().direction(), &newdirection ) ) || ( !fUp && FindLowerLevel( pSoldier, pSoldier->position().gridNo(), pSoldier->position().direction(), &newdirection ) ) )
				{
					return( AI_ACTION_CLIMB_ROOF );
				}
				else
				{
					return(AI_ACTION_SEEK_OPPONENT);
				}
			}
		}
	}

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Make boxer close if possible]"));
	// try to make boxer close if possible
	if (pSoldier->status().flags() & SOLDIER_BOXER )
	{
		DebugAI(AI_MSG_TOPIC, pSoldier, String("[Make boxer close if possible]"));

		SoldierID ubOpponentID;
		sClosestOpponent = ClosestKnownOpponent(pSoldier, NULL, NULL, &ubOpponentID);
		TacticalActor* boxerOpponent =
			GetJa2SoldierRepository().resolve(ubOpponentID.i);
		DebugAI(AI_MSG_INFO, pSoldier, String("boxer: found closest opponent [%d] at %d", ubOpponentID, sClosestOpponent));

		if ( !TileIsOutOfBounds(sClosestOpponent) && boxerOpponent )
		{
			if (pSoldier->actionPoints().current() > 0)
			{
				if (SpacesAway(pSoldier->position().gridNo(), sClosestOpponent) > 1)
				{
					INT16 sReserveAP = GetAPsToLook(pSoldier) + 2 * MinAPsToAttack(pSoldier, pSoldier->targeting().lastGridNo(), FALSE, 0, 0);
					BOOLEAN fLimitOneStep = FALSE;

					if (pSoldier->actionPoints().initial() < sReserveAP + APBPConstants[AP_MOVEMENT_FLAT] + APBPConstants[AP_MODIFIER_WALK])
					{
						sReserveAP = GetAPsToLook(pSoldier) + MinAPsToAttack(pSoldier, pSoldier->targeting().lastGridNo(), FALSE, 0, 0);
					}

					if (pSoldier->actionPoints().initial() < sReserveAP + APBPConstants[AP_MOVEMENT_FLAT] + APBPConstants[AP_MODIFIER_WALK] &&
						pSoldier->actionPoints().initial() >= GetAPsToLook(pSoldier) + MinAPsToAttack(pSoldier, pSoldier->targeting().lastGridNo(), FALSE, 0, 0) &&
						SpacesAway(pSoldier->position().gridNo(), sClosestOpponent) > 2)
					{
						sReserveAP = GetAPsToLook(pSoldier) + 1;
						fLimitOneStep = TRUE;
					}

					// temporarily make boxer have orders of CLOSEPATROL rather than STATIONARY
					// And make him patrol the ring, not his usual place
					// so he has a good roaming range
					INT32 tgrd = pSoldier->aiPlanning().patrolGrid()[0];
					pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
					pSoldier->aiBehavior().orders() = CLOSEPATROL;
					//pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sClosestOpponent, AI_ACTION_GET_CLOSER );
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards(pSoldier, sClosestOpponent, sReserveAP, AI_ACTION_GET_CLOSER, 0);
					pSoldier->aiPlanning().patrolGrid()[0] = tgrd;
					pSoldier->aiBehavior().orders() = STATIONARY;

					// decide to restore breath
					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()) &&
						(pSoldier->vitals().breath() < OKBREATH ||
						pSoldier->vitals().breath() < pSoldier->vitals().maximumBreath() &&
						pSoldier->vitals().breath() < boxerOpponent->vitals().breath() &&
						Chance((100 - pSoldier->vitals().breath()) * (100 - pSoldier->vitals().breath()) / (2 * 100 * 100))))
					{
						DebugAI(AI_MSG_INFO, pSoldier, String("boxer: restore breath"));
						pSoldier->aiPlanning().actionData() = NOWHERE;
					}

					if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
					{
						// truncate path to 1 step
						if (fLimitOneStep)
						{
							DebugAI(AI_MSG_INFO, pSoldier, String("boxer: limit movement to one step"));
							pSoldier->aiPlanning().actionData() = pSoldier->position().gridNo() + DirectionInc((UINT8)pSoldier->pathing().path()[0]);
							pSoldier->pathing().finalDestinationGrid() = pSoldier->aiPlanning().actionData();
						}

						//pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
						DebugAI(AI_MSG_INFO, pSoldier, String("boxer: get closer to opponent"));
						return(AI_ACTION_GET_CLOSER);
					}
				}
				else if (pSoldier->vitals().breath() < OKBREATH ||
					pSoldier->vitals().breath() < pSoldier->vitals().maximumBreath() &&
					(pSoldier->vitals().breath() < boxerOpponent->vitals().breath() || !pSoldier->combatResult().lastAttackHit() && TacticalActorConditions::hasTakenLargeHit(*pSoldier)))
				{
					// maybe move away from opponent
					UINT8 ubOpponentDir = AIDirection(pSoldier->position().gridNo(), sClosestOpponent);
					INT32 sCheckGridNo = NewGridNo(pSoldier->position().gridNo(), DirectionInc(gOppositeDirection[ubOpponentDir]));

					// only use reverse movement if we are facing opponent
					if (pSoldier->position().direction() == ubOpponentDir ||
						pSoldier->position().direction() == gOneCDirection[ubOpponentDir] ||
						pSoldier->position().direction() == gOneCCDirection[ubOpponentDir])
					{
						pSoldier->movement().setReverse(true);
					}

					if (!NewOKDestination(pSoldier, sCheckGridNo, FALSE, pSoldier->position().level()) ||
						PlotPath(pSoldier, sCheckGridNo, FALSE, FALSE, FALSE, DetermineMovementMode(pSoldier, AI_ACTION_TAKE_COVER), pSoldier->movement().stealthMode(), pSoldier->movement().reverse(), 0) > pSoldier->actionPoints().current() - 1)
					{
						//if (sPathcost > pSoldier->actionPoints().current() - (GetAPsToLook(pSoldier) + 1))
						DebugAI(AI_MSG_INFO, pSoldier, String("boxer: bad destination or high path cost, cannot move away"));
						sCheckGridNo = NOWHERE;
					}

					// maybe try diagonal movement
					if (TileIsOutOfBounds(sCheckGridNo) && Chance(50))
					{
						sCheckGridNo = NewGridNo(pSoldier->position().gridNo(), DirectionInc(gOneCDirection[gOppositeDirection[ubOpponentDir]]));
						if (!NewOKDestination(pSoldier, sCheckGridNo, FALSE, pSoldier->position().level()) ||
							PlotPath(pSoldier, sCheckGridNo, FALSE, FALSE, FALSE, DetermineMovementMode(pSoldier, AI_ACTION_TAKE_COVER), pSoldier->movement().stealthMode(), pSoldier->movement().reverse(), 0) > pSoldier->actionPoints().current() - 1)
						{
							//if (sPathcost > pSoldier->actionPoints().current() - (GetAPsToLook(pSoldier) + 1))
							DebugAI(AI_MSG_INFO, pSoldier, String("boxer: bad destination or high path cost, cannot move away"));
							sCheckGridNo = NOWHERE;
						}
					}
					if (TileIsOutOfBounds(sCheckGridNo) && Chance(50))
					{
						sCheckGridNo = NewGridNo(pSoldier->position().gridNo(), DirectionInc(gOneCCDirection[gOppositeDirection[ubOpponentDir]]));
						if (!NewOKDestination(pSoldier, sCheckGridNo, FALSE, pSoldier->position().level()) ||
							PlotPath(pSoldier, sCheckGridNo, FALSE, FALSE, FALSE, DetermineMovementMode(pSoldier, AI_ACTION_TAKE_COVER), pSoldier->movement().stealthMode(), pSoldier->movement().reverse(), 0) > pSoldier->actionPoints().current() - 1)
						{
							//if (sPathcost > pSoldier->actionPoints().current() - (GetAPsToLook(pSoldier) + 1))
							DebugAI(AI_MSG_INFO, pSoldier, String("boxer: bad destination or high path cost, cannot move away"));
							sCheckGridNo = NOWHERE;
						}
					}

					if (!TileIsOutOfBounds(sCheckGridNo))
					{
						pSoldier->aiPlanning().actionData() = sCheckGridNo;
						DebugAI(AI_MSG_INFO, pSoldier, String("boxer: get away from opponent"));
						return(AI_ACTION_TAKE_COVER);
					}
					pSoldier->movement().setReverse(false);
				}
			}

			UINT8 ubOpponentDir = AIDirection(pSoldier->position().gridNo(), sClosestOpponent);

			// possibly turn to closest opponent
			if (pSoldier->position().direction() != ubOpponentDir &&
				TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubOpponentDir) &&
				pSoldier->actionPoints().current() >= GetAPsToLook(pSoldier))
			{
				pSoldier->aiPlanning().actionData() = ubOpponentDir;
				DebugAI(AI_MSG_INFO, pSoldier, String("boxer: turn to closest opponent"));
				return(AI_ACTION_CHANGE_FACING);
			}
		}

		// otherwise do nothing
		DebugAI(AI_MSG_INFO, pSoldier, String("boxer: nothing to do"));
		return(AI_ACTION_NONE);
	}

	////////////////////////////////////////////////////////////////////////////
	// IF A LOCATION WITH BETTER COVER IS AVAILABLE & REACHABLE, GO FOR IT!
	////////////////////////////////////////////////////////////////////////////
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Take cover]"));

	if (!TileIsOutOfBounds(sBestCover))
	{
#ifdef DEBUGDECISIONS
		STR tempstr="";
		sprintf ( tempstr,"%s - TAKING COVER at gridno %d (%d%% better)\n",
			pSoldier->identity().name(),sBestCover,iCoverPercentBetter);
		DebugAI( tempstr ) ;
#endif
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_TESTVERSION, L"AI %d taking cover, morale %d, from %d to %d", pSoldier->identity().id(), pSoldier->morale().aiMorale(), pSoldier->sGridNo, sBestCover );
		pSoldier->aiPlanning().actionData() = sBestCover;
		if(!TileIsOutOfBounds(sClosestOpponent))//dnl ch58 150913 After taking cover change facing toward recent target or closest enemy, currently such turn not charge APs and seems because AI is still in moving animation from take cover action
		{
			if(!TileIsOutOfBounds(pSoldier->targeting().lastGridNo()))
				sClosestOpponent = pSoldier->targeting().lastGridNo();
			pSoldier->aiPlanning().nextAction() = AI_ACTION_CHANGE_FACING;
			pSoldier->aiPlanning().nextActionData() = GetDirectionFromCenterCellXYGridNo(sBestCover, sClosestOpponent);
		}
		return(AI_ACTION_TAKE_COVER);
	}
	
	////////////////////////////////////////////////////////////////////////////
	// IF THINGS ARE REALLY HOPELESS, OR UNARMED, TRY TO RUN AWAY
	////////////////////////////////////////////////////////////////////////////

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Run away]"));
	// if soldier has enough APs left to move at least 1 square's worth
	if ( ubCanMove && (pSoldier->roster().team() != gbPlayerNum || pSoldier->aiBehavior().flags() & AI_RTP_OPTION_CAN_RETREAT) )
	{
		if ((pSoldier->morale().aiMorale() == MORALE_HOPELESS) || !bCanAttack)
		{
			// look for best place to RUN AWAY to (farthest from the closest threat)
			//pSoldier->aiPlanning().actionData() = RunAway( pSoldier );
			pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents(pSoldier);
			
			if (!TileIsOutOfBounds(pSoldier->aiPlanning().actionData()))
			{
#ifdef DEBUGDECISIONS
				sprintf(tempstr,"%s - RUNNING AWAY to grid %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
				AIPopMessage(tempstr);
#endif

				return(AI_ACTION_RUN_AWAY);
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// IF SPOTTERS HAVE BEEN CALLED FOR, AND WE HAVE SOME NEW SIGHTINGS, RADIO!
	////////////////////////////////////////////////////////////////////////////

	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Radio sightings]"));
	// if we're a computer merc, and we have the action points remaining to RADIO
	// (we never want NPCs to choose to radio if they would have to wait a turn)
	// and we're not swimming in deep water, and somebody has called for spotters
	// and we see the location of at least 2 opponents
	if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && (gTacticalStatus.ubSpottersCalledForBy != NOBODY) && (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) &&
		(pSoldier->awareness().opponentCount() > 1) && !fCivilian &&
		(gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) && !bInDeepWater)
	{
		// base chance depends on how much new info we have to radio to the others
		iChance = 25 * WhatIKnowThatPublicDont(pSoldier,TRUE);	// just count them

		// if I actually know something they don't
		if (iChance)
		{
#ifdef DEBUGDECISIONS
			AINumMessage("Chance to radio for SPOTTING = ",iChance);
#endif

			if ((INT16)PreRandom(100) < iChance)
			{
#ifdef DEBUGDECISIONS
				AINameMessage(pSoldier,"decides to radio a RED for SPOTTING!",1000);
#endif

				return(AI_ACTION_RED_ALERT);
			}
		}
	}


	////////////////////////////////////////////////////////////////////////////
	// CROUCH IF NOT CROUCHING ALREADY
	////////////////////////////////////////////////////////////////////////////
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Crouch if not crouching already]"));
	// if not in water and not already crouched, try to crouch down first
	if (!gfTurnBasedAI || GetAPsToChangeStance( pSoldier, ANIM_CROUCH ) <= pSoldier->actionPoints().current())
	{
		if ( !fCivilian && !gfHiddenInterrupt && IsValidStance( pSoldier, ANIM_CROUCH ) && ubBestAttackAction != AI_ACTION_KNIFE_MOVE && ubBestAttackAction != AI_ACTION_KNIFE_STAB && ubBestAttackAction != AI_ACTION_STEAL_MOVE) // SANDRO - if knife attack don't crouch
		{
			// determine the location of the known closest opponent
			// (don't care if he's conscious, don't care if he's reachable at all)

			sClosestOpponent = ClosestSeenOpponent(pSoldier, NULL, NULL);
			// SANDRO - don't crouch if in close combat distance (we got penalties that way)
			if (PythSpacesAway(pSoldier->position().gridNo(), sClosestOpponent) > 1 )
			{
				pSoldier->aiPlanning().actionData() = StanceChange( pSoldier, BestAttack.ubAPCost );
				if (pSoldier->aiPlanning().actionData() != 0)
				{
					if (pSoldier->aiPlanning().actionData() == ANIM_PRONE)
					{
						// we might want to turn before lying down!
						if ( (!gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() - GetAPsToChangeStance( pSoldier, (INT8) pSoldier->aiPlanning().actionData() )) &&
							(((pSoldier->morale().aiMorale() > MORALE_HOPELESS) || ubCanMove) && !AimingGun(pSoldier)) )
						{
							// if we have a closest seen opponent						
							if (!TileIsOutOfBounds(sClosestOpponent))
							{
								bDirection = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestOpponent);

								// if we're not facing towards him
								if (pSoldier->position().direction() != bDirection)
								{
									if ( TacticalActorMobility::isValidStance(*pSoldier,  bDirection, (INT8) pSoldier->aiPlanning().actionData()) )
									{
										// change direction, THEN change stance!
										pSoldier->aiPlanning().nextAction() = AI_ACTION_CHANGE_STANCE;
										pSoldier->aiPlanning().nextActionData() = pSoldier->aiPlanning().actionData();
										pSoldier->aiPlanning().actionData() = bDirection;
#ifdef DEBUGDECISIONS
										sprintf(tempstr,"%s - TURNS to face CLOSEST OPPONENT in direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
										AIPopMessage(tempstr);
#endif
										return(AI_ACTION_CHANGE_FACING);
									}
									else if ( (pSoldier->aiPlanning().actionData() == ANIM_PRONE) && (TacticalActorMobility::isValidStance(*pSoldier,  bDirection, ANIM_CROUCH) ) )
									{
										// we shouldn't go prone, since we can't turn to shoot
										pSoldier->aiPlanning().actionData() = ANIM_CROUCH;
										pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
										return( AI_ACTION_CHANGE_STANCE );
									}
								}
								// else we are facing in the right direction

							}
							// else we don't know any enemies
						}

						// we don't want to turn
					}
					pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
					return( AI_ACTION_CHANGE_STANCE );
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// TURN TO FACE CLOSEST KNOWN OPPONENT (IF NOT FACING THERE ALREADY)
	////////////////////////////////////////////////////////////////////////////
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Turn to closest known opponent]"));
	if (!gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current())
	{
		// hopeless guys shouldn't waste their time this way, UNLESS they CAN move
		// but chose not to to get this far (which probably means they're cornered)
		// ALSO, don't bother turning if we're already aiming a gun
		if ( !gfHiddenInterrupt && ((pSoldier->morale().aiMorale() > MORALE_HOPELESS) || ubCanMove) && !AimingGun(pSoldier))
		{
			// determine the location of the known closest opponent
			// (don't care if he's conscious, don't care if he's reachable at all)


			sClosestOpponent = ClosestSeenOpponent(pSoldier, NULL, NULL);
			// if we have a closest reachable opponent			
			if (!TileIsOutOfBounds(sClosestOpponent))
			{
				if(!TileIsOutOfBounds(pSoldier->targeting().lastGridNo()))//dnl ch58 150913
					sClosestOpponent = pSoldier->targeting().lastGridNo();
				bDirection = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestOpponent);

				// if we're not facing towards him
				if ( pSoldier->position().direction() != bDirection && TacticalActorMobility::isCurrentStanceValid(*pSoldier, bDirection) )
				{
					pSoldier->aiPlanning().actionData() = bDirection;

#ifdef DEBUGDECISIONS
					sprintf(tempstr,"%s - TURNS to face CLOSEST OPPONENT in direction %d",pSoldier->identity().name(),pSoldier->aiPlanning().actionData());
					AIPopMessage(tempstr);
#endif

					return(AI_ACTION_CHANGE_FACING);
				}
			}
		}
	}

	// if a militia has absofreaking nothing else to do, maybe they should radio in a report!

	////////////////////////////////////////////////////////////////////////////
	// RADIO RED ALERT: determine %chance to call others and report contact
	////////////////////////////////////////////////////////////////////////////
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Report contacts]"));
	if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && pSoldier->roster().team() == MILITIA_TEAM && (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) && (gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) )
	{

		// if there hasn't been an initial RED ALERT yet in this sector
		if ( !(gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition) || NeedToRadioAboutPanicTrigger() )
		{     // since I'm at STATUS RED, I obviously know we're being invaded!
			DebugMsg(TOPIC_JA2AI,DBG_LEVEL_3,String("DecideActionBlack: check chance to radio contact"));
			iChance = gbDiff[DIFF_RADIO_RED_ALERT][ SoldierDifficultyLevel( pSoldier ) ];
		}
		else // subsequent radioing (only to update enemy positions, request help)
			// base chance depends on how much new info we have to radio to the others
			iChance = 10 * WhatIKnowThatPublicDont(pSoldier,FALSE);  // use 10 * for RED alert

		// if I actually know something they don't and I ain't swimming (deep water)
		if (iChance && !bInDeepWater)
		{
			// modify base chance according to orders
			switch (pSoldier->aiBehavior().orders())
			{
			case STATIONARY:       iChance +=  20;  break;
			case ONGUARD:          iChance +=  15;  break;
			case ONCALL:           iChance +=  10;  break;
			case CLOSEPATROL:                       break;
			case RNDPTPATROL:
			case POINTPATROL:      iChance +=  -5;  break;
			case FARPATROL:        iChance += -10;  break;
			case SEEKENEMY:        iChance += -20;  break;
			case SNIPER:			  iChance += -10;  break;
			}

			// modify base chance according to attitude
			switch (pSoldier->aiBehavior().attitude())
			{
			case DEFENSIVE:        iChance +=  20;  break;
			case BRAVESOLO:        iChance += -10;  break;
			case BRAVEAID:                          break;
			case CUNNINGSOLO:      iChance +=  -5;  break;
			case CUNNINGAID:                        break;
			case AGGRESSIVE:       iChance += -20;  break;
			case ATTACKSLAYONLY:		iChance = 0;
			}

			if (gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition)
			{
				// ignore morale (which could be really high)
			}
			else
			{
				// modify base chance according to morale
				switch (pSoldier->morale().aiMorale())
				{
				case MORALE_HOPELESS:  iChance *= 3;    break;
				case MORALE_WORRIED:   iChance *= 2;    break;
				case MORALE_NORMAL:                     break;
				case MORALE_CONFIDENT: iChance /= 2;    break;
				case MORALE_FEARLESS:  iChance /= 3;    break;
				}
			}

			// reduce chance because we're in combat
			iChance /= 2;

#ifdef DEBUGDECISIONS
			AINumMessage("Chance to radio RED alert = ",iChance);
#endif

			if ((INT16) PreRandom(100) < iChance)
			{
#ifdef DEBUGDECISIONS
				AINameMessage(pSoldier,"decides to radio a RED alert!",1000);
#endif

				return(AI_ACTION_RED_ALERT);
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// LEAVE THE SECTOR
	////////////////////////////////////////////////////////////////////////////

	// NOT IMPLEMENTED

	////////////////////////////////////////////////////////////////////////////
	// DO NOTHING: Not enough points left to move, so save them for next turn
	////////////////////////////////////////////////////////////////////////////

#ifdef DEBUGDECISIONS
	AINameMessage(pSoldier,"- DOES NOTHING (BLACK)",1000);
#endif
	DebugAI(AI_MSG_TOPIC, pSoldier, String("[Nothing to do]"));

	// by default, if everything else fails, just stand in place and wait
	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);

}

void DecideAlertStatus( TacticalActor *pSoldier )
{
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif
	INT8	bOldStatus;
	INT32	iDummy;
	BOOLEAN fClimbDummy,fReachableDummy;

	// THE FOUR (4) POSSIBLE ALERT STATUSES ARE:
	// GREEN - No one seen, no suspicious noise heard, go about regular duties
	// YELLOW - Suspicious noise was heard personally or radioed in by buddy
	// RED - Either saw opponents in person, or definite contact had been radioed
	// BLACK - Currently has one or more opponents in sight

	// save the man's previous status

	if (pSoldier->status().flags() & SOLDIER_MONSTER)
	{
		CreatureDecideAlertStatus( pSoldier );
		return;
	}

	bOldStatus = pSoldier->aiBehavior().alertStatus();

	// determine the current alert status for this category of man
	//if (!(pSoldier->status().flags() & SOLDIER_PC))
	{
		if (pSoldier->awareness().opponentCount() > 0)        // opponent(s) in sight
		{
			pSoldier->aiBehavior().alertStatus() = STATUS_BLACK;
			CheckForChangingOrders( pSoldier );
		}
		else                        // no opponents are in sight
		{
			switch (bOldStatus)
			{
			case STATUS_BLACK:
				// then drop back to RED status
				pSoldier->aiBehavior().alertStatus() = STATUS_RED;
				break;

			case STATUS_RED:
				// RED can never go back down below RED, only up to BLACK
				break;

			case STATUS_YELLOW:
				// if all enemies have been RED alerted, or we're under fire
				if (!PTR_CIVILIAN && (gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition || pSoldier->suppression().underFire()))
				{
					pSoldier->aiBehavior().alertStatus() = STATUS_RED;
				}
				else
				{
					// if we are NOT aware of any uninvestigated noises right now
					// and we are not currently in the middle of an action
					// (could still be on his way heading to investigate a noise!)					
					if (( TileIsOutOfBounds(MostImportantNoiseHeard(pSoldier,&iDummy,&fClimbDummy,&fReachableDummy))) && !pSoldier->aiPlanning().actionInProgress())
					{
						// then drop back to GREEN status
						pSoldier->aiBehavior().alertStatus() = STATUS_GREEN;
						CheckForChangingOrders( pSoldier );
					}
				}
				break;

			case STATUS_GREEN:
				// if all enemies have been RED alerted, or we're under fire
				if (!PTR_CIVILIAN && (gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition || pSoldier->suppression().underFire()))
				{
					pSoldier->aiBehavior().alertStatus() = STATUS_RED;
				}
				else
				{
					// if we ARE aware of any uninvestigated noises right now					
					if (!TileIsOutOfBounds(MostImportantNoiseHeard(pSoldier,&iDummy,&fClimbDummy,&fReachableDummy)))
					{
						// then move up to YELLOW status
						pSoldier->aiBehavior().alertStatus() = STATUS_YELLOW;
					}
				}
				break;
			}
			// otherwise, RED stays RED, YELLOW stays YELLOW, GREEN stays GREEN
		}
	}

	if ( gTacticalStatus.bBoxingState == NOT_BOXING )
	{

		// if the man's alert status has changed in any way
		if (pSoldier->aiBehavior().alertStatus() != bOldStatus)
		{
			// HERE ARE TRYING TO AVOID NPCs SHUFFLING BACK & FORTH BETWEEN RED & BLACK
			// if either status is < RED (ie. anything but RED->BLACK && BLACK->RED)
			if ((bOldStatus < STATUS_RED) || (pSoldier->aiBehavior().alertStatus() < STATUS_RED))
			{
				// force a NEW action decision on next pass through HandleManAI()
				SetNewSituation( pSoldier );
			}

			// if this guy JUST discovered that there were opponents here for sure...
			if ((bOldStatus < STATUS_RED) && (pSoldier->aiBehavior().alertStatus() >= STATUS_RED))
			{
				CheckForChangingOrders(pSoldier);
			}

#ifdef DEBUGDECISIONS
			// don't report status changes for human-controlled mercs
			sprintf(tempstr,"%s's Alert Status changed from %d to %d",
				pSoldier->identity().name(),bOldStatus,pSoldier->aiBehavior().alertStatus());
			AIPopMessage(tempstr);
#endif

		}
		else   // status didn't change
		{
			// only do this stuff in TB
			// if a guy on status GREEN or YELLOW is running low on breath
			if (((pSoldier->aiBehavior().alertStatus() == STATUS_GREEN)  && (pSoldier->vitals().breath() < 75)) ||
				((pSoldier->aiBehavior().alertStatus() == STATUS_YELLOW) && (pSoldier->vitals().breath() < 50)))
			{
				// as long as he's not in water (standing on a bridge is OK)
				if (!TacticalActorMobility::inWater(*pSoldier))
				{
					// force a NEW decision so that he can get some rest
					SetNewSituation( pSoldier );

					// current action will be canceled. if noise is no longer important					
					if ((pSoldier->aiBehavior().alertStatus() == STATUS_YELLOW) &&
						( TileIsOutOfBounds(MostImportantNoiseHeard(pSoldier,&iDummy,&fClimbDummy,&fReachableDummy))))
					{
						// then drop back to GREEN status
						pSoldier->aiBehavior().alertStatus() = STATUS_GREEN;
						CheckForChangingOrders( pSoldier );
					}
				}
			}
		}
	}
	else if (gTacticalStatus.bBoxingState == DISQUALIFIED ||
		gTacticalStatus.bBoxingState == WON_ROUND ||
		gTacticalStatus.bBoxingState == LOST_ROUND)
	{
		pSoldier->aiBehavior().alertStatus() = STATUS_GREEN;
	}

}

INT8 ArmedVehicleDecideAction( TacticalActor *pSoldier )
{
	INT8 bAction = AI_ACTION_NONE;

	// sevenfm: disable stealth mode
	pSoldier->movement().setStealth(false);
	// disable reverse movement mode
	pSoldier->movement().setReverse(false);
	// sevenfm: initialize data
	pSoldier->attackSelection().weaponMode() = WM_NORMAL;

	switch ( pSoldier->aiBehavior().alertStatus() )
	{
	case STATUS_GREEN:
#ifdef DEBUGDECISIONS
		AIPopMessage( "AlertStatus = GREEN" );
#endif
		bAction = ArmedVehicleDecideActionGreen( pSoldier );
		break;

	case STATUS_YELLOW:
#ifdef DEBUGDECISIONS
		AIPopMessage( "AlertStatus = YELLOW" );
#endif
		bAction = ArmedVehicleDecideActionYellow( pSoldier );
		break;

	case STATUS_RED:
#ifdef DEBUGDECISIONS
		AIPopMessage( "AlertStatus = RED" );
#endif
		bAction = ArmedVehicleDecideActionRed(pSoldier);
		break;

	case STATUS_BLACK:
#ifdef DEBUGDECISIONS
		AIPopMessage( "AlertStatus = BLACK" );
#endif
		bAction = ArmedVehicleDecideActionBlack( pSoldier );
		break;
	}

#ifdef DEBUGDECISIONS
	STR tempstr;
	sprintf( tempstr, "ArmedVehicleDecideAction: selected action %d, actionData %d\n\n", bAction, pSoldier->aiPlanning().actionData() );
	DebugAI( tempstr );
#endif

	return(bAction);
}

INT8 ArmedVehicleDecideActionGreen( TacticalActor *pSoldier )
{
	DOUBLE iChance, iSneaky = 10;
	INT8  bInWater;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif

	// Flugente: to prevent an accidental call
	if ( !ARMED_VEHICLE( pSoldier ) )
		return DecideActionGreen( pSoldier );

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionGreen, orders = %d", pSoldier->aiBehavior().orders() ) );
	
	gubNPCPathCount = 0;

	if ( gTacticalStatus.bBoxingState != NOT_BOXING )
	{
		// do nothing during boxing match
		return(AI_ACTION_ABSOLUTELY_NONE);
	}

	if ( !gGameExternalOptions.fEnemyTanksCanMoveInTactical )
	{
		return(AI_ACTION_NONE);
	}
	
	bInWater = Water( pSoldier->position().gridNo(), pSoldier->position().level() );
	
	//ddd{
	if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && gGameExternalOptions.bNewTacticalAIBehavior && pSoldier->roster().team() == ENEMY_TEAM )
	{
		if ( !IsJa2TacticalTurnBased() && IsJa2TacticalCombatActive() )
		{
			INT32				cnt;
			ROTTING_CORPSE *	pCorpse;

			for ( cnt = 0; cnt < giNumRottingCorpse; ++cnt )
			{
				pCorpse = &(gRottingCorpse[cnt]);

				if ( pCorpse->fActivated && pCorpse->def.ubAIWarningValue > 0 )
				{
					if ( PythSpacesAway( pSoldier->position().gridNo(), pCorpse->def.sGridNo ) <= 5 )//add check(comparison) of sight range variable (smaxvid ?)
					{
						//check if the corpse is in the enemy/militia field of view?
						//CHRISL: Shouldn't we be using the corpse's bLevel?  Otherwise a soldier inside a building can see a corpse on the roof of that building
						//if ( SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pSoldier->position().level(), 3, TRUE, CALC_FROM_WANTED_DIR ) )
						if ( SoldierTo3DLocationLineOfSightTest( pSoldier, pCorpse->def.sGridNo, pCorpse->def.bLevel, 3, TRUE, CALC_FROM_WANTED_DIR ) )
						{
							ScreenMsg( MSG_FONT_YELLOW, MSG_INTERFACE, New113Message[MSG113_ENEMY_FOUND_DEAD_BODY] );
							//pCorpse->def.ubAIWarningValue=0;
							gRottingCorpse[cnt].def.ubAIWarningValue = 0;
							return(AI_ACTION_RED_ALERT);
						}
					}
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////
		// IF YOU SEE CAPTURED FRIENDS, FREE THEM!
		////////////////////////////////////////////////////////////////////////////

		// Flugente: if we see one of our buddies in handcuffs, its a clear sign of enemy activity!
		if ( gGameExternalOptions.fAllowPrisonerSystem && pSoldier->roster().team() == ENEMY_TEAM && !gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition )
		{
			if ( GetClosestFlaggedSoldierID( pSoldier, 20, ENEMY_TEAM, SOLDIER_POW, TRUE ) != NOBODY )
			{
				// raise alarm!
				return(AI_ACTION_RED_ALERT);
			}
		}
	}
	//ddd}

	////////////////////////////////////////////////////////////////////////////
	// POINT PATROL: move towards next point unless getting a bit winded
	////////////////////////////////////////////////////////////////////////////

	// this takes priority over water/gas checks, so that point patrol WILL work
	// from island to island, and through gas covered areas, too
	if ( (pSoldier->aiBehavior().orders() == POINTPATROL) && (pSoldier->vitals().breath() >= 75) )
	{
		if ( PointPatrolAI( pSoldier ) )
		{
			if ( !gfTurnBasedAI )
			{
				// wait after this...
				pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
				pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
			}
			return(AI_ACTION_POINT_PATROL);
		}
		else
		{
			// Reset path count to avoid dedlok
			gubNPCPathCount = 0;
		}
	}

	if ( (pSoldier->aiBehavior().orders() == RNDPTPATROL) && (pSoldier->vitals().breath() >= 75) )
	{
		if ( RandomPointPatrolAI( pSoldier ) )
		{
			if ( !gfTurnBasedAI )
			{
				// wait after this...
				pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
				pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
			}
			return(AI_ACTION_POINT_PATROL);
		}
		else
		{
			// Reset path count to avoid dedlok
			gubNPCPathCount = 0;
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// WHEN LEFT IN WATER OR GAS, GO TO NEAREST REACHABLE SPOT OF UNGASSED LAND
	////////////////////////////////////////////////////////////////////////////

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionGreen: get out of water and gas" ) );

	if ( bInWater )
	{
		pSoldier->aiPlanning().actionData() = FindNearestUngassedLand( pSoldier );

		if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
		{
#ifdef DEBUGDECISIONS
			sprintf( tempstr, "%s - SEEKING NEAREST UNGASSED LAND at grid %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
			AIPopMessage( tempstr );
#endif

			return(AI_ACTION_LEAVE_WATER_GAS);
		}
	}
	
	////////////////////////////////////////////////////////////////////////
	// REST IF RUNNING OUT OF BREATH
	////////////////////////////////////////////////////////////////////////

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "DecideActionGreen: rest if running out of breath" ) );
	// if our breath is running a bit low, and we're not in the way or in water
	if ( (pSoldier->vitals().breath() < 75) && !bInWater )
	{
		// take a breather for gods sake!
		// for realtime, AI will use a standard wait set outside of here
		pSoldier->aiPlanning().actionData() = NOWHERE;
		return(AI_ACTION_NONE);
	}
	
	////////////////////////////////////////////////////////////////////////////
	// RANDOM PATROL:  determine % chance to start a new patrol route
	////////////////////////////////////////////////////////////////////////////
	if ( !gubNPCPathCount ) // try to limit pathing in Green AI
	{
		iChance = 25 + pSoldier->aiBehavior().bypassToGreen();

		// set base chance according to orders
		switch ( pSoldier->aiBehavior().orders() )
		{
		case STATIONARY:     iChance += -20;  break;
		case ONGUARD:        iChance += -15;  break;
		case ONCALL:                          break;
		case CLOSEPATROL:    iChance += +15;  break;
		case RNDPTPATROL:
		case POINTPATROL:		iChance = 0; break;	
		case FARPATROL:      iChance += +25;  break;
		case SEEKENEMY:      iChance += -10;  break;
		case SNIPER:		iChance += -10;  break;
		}

		// modify chance of patrol (and whether it's a sneaky one) by attitude
		switch ( pSoldier->aiBehavior().attitude() )
		{
		case DEFENSIVE:      iChance += -10;                 break;
		case BRAVESOLO:      iChance += 5;                 break;
		case BRAVEAID:                                       break;
		case CUNNINGSOLO:    iChance += 5;  iSneaky += 10; break;
		case CUNNINGAID:                      iSneaky += 5; break;
		case AGGRESSIVE:     iChance += 10;  iSneaky += -5; break;
		case ATTACKSLAYONLY: iChance += 10;  iSneaky += -5; break;
		}

		// reduce chance for any injury, less likely to wander around when hurt
		iChance -= (pSoldier->vitals().maximumHealth() - pSoldier->vitals().health());

		// reduce chance if breath is down, less likely to wander around when tired
		iChance -= (100 - pSoldier->vitals().breath());

		// if we're in water with land miles (> 25 tiles) away,
		// OR if we roll under the chance calculated
		if ( bInWater || ((INT16)PreRandom( 100 ) < iChance) )
		{
			pSoldier->aiPlanning().actionData() = RandDestWithinRange( pSoldier );

			if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
				pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, pSoldier->aiPlanning().actionData(), AI_ACTION_RANDOM_PATROL );
			}

			if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
#ifdef DEBUGDECISIONS
				sprintf( tempstr, "%s - RANDOM PATROL to grid %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
				AIPopMessage( tempstr );
#endif

				if ( !gfTurnBasedAI )
				{
					// wait after this...
					pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
					pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
				}
				return(AI_ACTION_RANDOM_PATROL);
			}
		}
	}

	if ( !gubNPCPathCount ) // try to limit pathing in Green AI
	{
		////////////////////////////////////////////////////////////////////////////
		// SEEK FRIEND: determine %chance for man to pay a friendly visit
		////////////////////////////////////////////////////////////////////////////

		iChance = 25 + pSoldier->aiBehavior().bypassToGreen();

		// set base chance and maximum seeking distance according to orders
		switch ( pSoldier->aiBehavior().orders() )
		{
		case STATIONARY:     iChance += -20; break;
		case ONGUARD:        iChance += -15; break;
		case ONCALL:                         break;
		case CLOSEPATROL:    iChance += +10; break;
		case RNDPTPATROL:
		case POINTPATROL:    iChance = -10; break;
		case FARPATROL:      iChance += +20; break;
		case SEEKENEMY:      iChance += -10; break;
		case SNIPER:		  iChance += -10; break;
		}

		// modify for attitude
		switch ( pSoldier->aiBehavior().attitude() )
		{
		case DEFENSIVE:                       break;
		case BRAVESOLO:      iChance /= 2;    break;  // loners
		case BRAVEAID:       iChance += 10;   break;  // friendly
		case CUNNINGSOLO:    iChance /= 2;    break;  // loners
		case CUNNINGAID:     iChance += 10;   break;  // friendly
		case AGGRESSIVE:                      break;
		case ATTACKSLAYONLY:									 break;
		}

		// reduce chance for any injury, less likely to wander around when hurt
		iChance -= (pSoldier->vitals().maximumHealth() - pSoldier->vitals().health());

		// reduce chance if breath is down
		iChance -= (100 - pSoldier->vitals().breath());         // very likely to wait when exhausted
		
		if ( (INT16)PreRandom( 100 ) < iChance )
		{
			if ( RandomFriendWithin( pSoldier ) )
			{
				if ( pSoldier->aiPlanning().actionData() == GoAsFarAsPossibleTowards( pSoldier, pSoldier->aiPlanning().actionData(), AI_ACTION_SEEK_FRIEND ) )
				{

#ifdef DEBUGDECISIONS
					sprintf( tempstr, "%s - SEEK FRIEND at grid %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
					AIPopMessage( tempstr );
#endif

					return(AI_ACTION_SEEK_FRIEND);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// LOOK AROUND: determine %chance for man to turn in place
	////////////////////////////////////////////////////////////////////////////

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "DecideActionGreen: Soldier deciding to turn" ) );
	if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
	{
		// avoid 2 consecutive random turns in a row
		if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_CHANGE_FACING )
		{
			iChance = 25 + pSoldier->aiBehavior().bypassToGreen();

			// set base chance according to orders
			if ( pSoldier->aiBehavior().orders() == STATIONARY || pSoldier->aiBehavior().orders() == SNIPER )
				iChance += 25;

			if ( pSoldier->aiBehavior().orders() == ONGUARD )
				iChance += 20;

			if ( pSoldier->aiBehavior().attitude() == DEFENSIVE )
				iChance += 25;

			if ( pSoldier->aiBehavior().orders() == SNIPER && pSoldier->position().level() == 1 )
				iChance += 35;

			if ( WeaponReady( pSoldier ) ) // SANDRO - if readied weapon, make him more likely to turn around
				iChance += 30;

			if ( (INT16)PreRandom( 100 ) < iChance )
			{
				// roll random directions (stored in actionData) until different from current
				do
				{
					// if man has a LEGAL dominant facing, and isn't facing it, he will turn
					// back towards that facing 50% of the time here (normally just enemies)
					if ( (pSoldier->aiPlanning().dominantDirection() >= 0) && (pSoldier->aiPlanning().dominantDirection() <= 8) &&
						 (pSoldier->position().direction() != pSoldier->aiPlanning().dominantDirection()) && PreRandom( 2 ) && pSoldier->aiBehavior().orders() != SNIPER )
					{
						pSoldier->aiPlanning().actionData() = pSoldier->aiPlanning().dominantDirection();
					}
					else
					{
						INT32 iNoiseValue;
						BOOLEAN fClimb;
						BOOLEAN fReachable;
						INT32 sNoiseGridNo = MostImportantNoiseHeard( pSoldier, &iNoiseValue, &fClimb, &fReachable );
						UINT8 ubNoiseDir;

						if ( TileIsOutOfBounds( sNoiseGridNo ) ||
							 (ubNoiseDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sNoiseGridNo)
							 ) == pSoldier->position().direction() )

						{
							pSoldier->aiPlanning().actionData() = PreRandom( 8 );
						}
						else
						{
							pSoldier->aiPlanning().actionData() = ubNoiseDir;
						}
					}
				} while ( pSoldier->aiPlanning().actionData() == pSoldier->position().direction() );
				
#ifdef DEBUGDECISIONS
				sprintf( tempstr, "%s - TURNS to face direction %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
				AIPopMessage( tempstr );
#endif

				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "DecideActionGreen: Trying to turn - checking stance validity, sniper = %d", pSoldier->aiPlanning().sniperPosture() ) );
				if ( TacticalActorMobility::isCurrentStanceValid(*pSoldier, (INT8)pSoldier->aiPlanning().actionData()) )
				{

					if ( !gfTurnBasedAI )
					{
						// wait after this...
						pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
						pSoldier->aiPlanning().nextActionData() = RealtimeDelay( pSoldier );
					}

					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "DecideActionGreen: Soldier is turning" ) );
					return(AI_ACTION_CHANGE_FACING);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// NONE:
	////////////////////////////////////////////////////////////////////////////

	// by default, if everything else fails, just stands in place without turning
	// for realtime, regular AI guys will use a standard wait set outside of here
	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);
}

INT8 ArmedVehicleDecideActionYellow( TacticalActor *pSoldier )
{
	INT32 iDummy;
	UINT8 ubNoiseDir;
	INT32 sNoiseGridNo;
	INT32 iNoiseValue;
	INT32 iChance, iSneaky;
	INT32 sClosestFriend;
	BOOLEAN fClimb;
	BOOLEAN fReachable;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif

	// Flugente: to prevent an accidental call
	if ( !ARMED_VEHICLE(pSoldier) )
		return DecideActionYellow( pSoldier );
	
	// determine the most important noise heard, and its relative value
	sNoiseGridNo = MostImportantNoiseHeard( pSoldier, &iNoiseValue, &fClimb, &fReachable );

	if ( TileIsOutOfBounds( sNoiseGridNo ) )
	{
		// then we have no business being under YELLOW status any more!
#ifdef BETAVERSION
		NumMessage( "ArmedVehicleDecideActionYellow: ERROR - No important noise known by guynum ", pSoldier->identity().id() );
#endif
		return(AI_ACTION_NONE);
	}

	if ( gGameExternalOptions.bNewTacticalAIBehavior )
	{
		////////////////////////////////////////////////////////////////////////////
		// IF YOU SEE CAPTURED FRIENDS, FREE THEM!
		////////////////////////////////////////////////////////////////////////////

		// Flugente: if we see one of our buddies captured, it is a clear sign of enemy activity!
		if ( gGameExternalOptions.fAllowPrisonerSystem && pSoldier->roster().team() == ENEMY_TEAM )
		{
			if ( GetClosestFlaggedSoldierID( pSoldier, 20, ENEMY_TEAM, SOLDIER_POW, TRUE ) != NOBODY )
			{
				// if we are close, we can release this guy
				// possible only if not handcuffed (binders can be opened, handcuffs not)
				if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && !gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition )
				{
					// raise alarm!
					return(AI_ACTION_RED_ALERT);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// LOOK AROUND TOWARD NOISE: determine %chance for man to turn towards noise
	////////////////////////////////////////////////////////////////////////////

	// determine direction from this soldier in which the noise lies
	ubNoiseDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sNoiseGridNo);

	// if soldier is not already facing in that direction,
	// and the noise source is close enough that it could possibly be seen
	if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
	{
		if ( (pSoldier->position().direction() != ubNoiseDir) && PythSpacesAway( pSoldier->position().gridNo(), sNoiseGridNo ) <= TacticalActorVisibility::maximumDistance(*pSoldier,  sNoiseGridNo ) )
		{
			// set base chance according to orders
			if ( (pSoldier->aiBehavior().orders() == STATIONARY) || (pSoldier->aiBehavior().orders() == ONGUARD) )
				iChance = 50;
			else           // all other orders
				iChance = 25;

			if ( pSoldier->aiBehavior().attitude() == DEFENSIVE )
				iChance += 15;

			if ( (INT16)PreRandom( 100 ) < iChance && TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubNoiseDir) )
			{
				pSoldier->aiPlanning().actionData() = ubNoiseDir;
#ifdef DEBUGDECISIONS
				sprintf( tempstr, "%s - TURNS TOWARDS NOISE to face direction %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
				AIPopMessage( tempstr );
#endif
				
				return(AI_ACTION_CHANGE_FACING);
			}
		}
	}
	
	////////////////////////////////////////////////////////////////////////////
	// RADIO YELLOW ALERT: determine %chance to call others and report noise
	////////////////////////////////////////////////////////////////////////////

	// if we have the action points remaining to RADIO
	// (we never want NPCs to choose to radio if they would have to wait a turn)
	if ( (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) &&
		 (gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) )
	{
		// base chance depends on how much new info we have to radio to the others
		iChance = 5 * WhatIKnowThatPublicDont( pSoldier, FALSE );   // use 5 * for YELLOW alert

		// if I actually know something they don't and I ain't swimming (deep water)
		if ( iChance && !DeepWater( pSoldier->position().gridNo(), pSoldier->position().level() ) )
		{
			// CJC: this addition allows for varying difficulty levels for soldier types
			iChance += gbDiff[DIFF_RADIO_RED_ALERT][SoldierDifficultyLevel( pSoldier )] / 2;
			
			// modify base chance according to orders
			switch ( pSoldier->aiBehavior().orders() )
			{
			case STATIONARY: iChance += 20;  break;
			case ONGUARD:    iChance += 15;  break;
			case ONCALL:     iChance += 10;  break;
			case CLOSEPATROL:                 break;
			case RNDPTPATROL:
			case POINTPATROL:                 break;
			case FARPATROL:  iChance += -10;  break;
			case SEEKENEMY:  iChance += -20;  break;
			case SNIPER:		iChance += -10; break; //Madd: sniper contacts are supposed to be automatically reported
			}

			// modify base chance according to attitude
			switch ( pSoldier->aiBehavior().attitude() )
			{
			case DEFENSIVE:  iChance += 20;  break;
			case BRAVESOLO:  iChance += -10;  break;
			case BRAVEAID:                    break;
			case CUNNINGSOLO:iChance += -5;  break;
			case CUNNINGAID:                  break;
			case AGGRESSIVE: iChance += -20;  break;
			case ATTACKSLAYONLY: iChance = 0; break;
			}

#ifdef DEBUGDECISIONS
			AINumMessage( "Chance to radio yellow alert = ", iChance );
#endif

			if ( (INT16)PreRandom( 100 ) < iChance )
			{
#ifdef DEBUGDECISIONS
				AINameMessage( pSoldier, "decides to radio a YELLOW alert!", 1000 );
#endif

				return(AI_ACTION_YELLOW_ALERT);
			}
		}
	}

	if ( !gGameExternalOptions.fEnemyTanksCanMoveInTactical )
	{
		return(AI_ACTION_NONE);
	}

	////////////////////////////////////////////////////////////////////////
	// REST IF RUNNING OUT OF BREATH
	////////////////////////////////////////////////////////////////////////

	// if our breath is running a bit low, and we're not in water
	if ( (pSoldier->vitals().breath() < 25) && !TacticalActorMobility::inWater(*pSoldier) )
	{
		// take a breather for gods sake!
		pSoldier->aiPlanning().actionData() = NOWHERE;

		return(AI_ACTION_NONE);
	}

	//continue flanking
	INT32 sFlankGridNo;

	if ( TileIsOutOfBounds( sNoiseGridNo ) )
		sFlankGridNo = pSoldier->aiPlanning().flankAnchorGrid();
	else
		sFlankGridNo = sNoiseGridNo;

	if ( pSoldier->aiPlanning().flankCount() > 0 && pSoldier->aiPlanning().flankCount() < MAX_FLANKS_YELLOW )
	{
		INT16 currDir = GetDirectionFromGridNo( sFlankGridNo, pSoldier );
		INT16 origDir = pSoldier->aiPlanning().flankOriginDirection();
		pSoldier->aiPlanning().advanceFlank();
		if ( pSoldier->aiPlanning().lastFlankLeft() )
		{
			if ( origDir > currDir )
				origDir -= NUM_WORLD_DIRECTIONS;

			// stop flanking if reached desired direction
			if ( (currDir - origDir) >= MinFlankDirections( pSoldier ) )
			{
				pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
			else
			{
				pSoldier->aiPlanning().actionData() = FindFlankingSpot( pSoldier, sFlankGridNo, AI_ACTION_FLANK_LEFT );
				if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) ) //&& (currDir - origDir) < 2 )
					return AI_ACTION_FLANK_LEFT;
				else
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
		}
		else
		{
			if ( origDir < currDir )
				origDir += NUM_WORLD_DIRECTIONS;

			// stop flanking if reached desired direction
			if ( (origDir - currDir) >= MinFlankDirections( pSoldier ) )
			{
				pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
			else
			{
				pSoldier->aiPlanning().actionData() = FindFlankingSpot( pSoldier, sFlankGridNo, AI_ACTION_FLANK_RIGHT );
				if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )//&& (origDir - currDir) < 2 )
					return AI_ACTION_FLANK_RIGHT;
				else
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_YELLOW);
			}
		}
	}

	if ( pSoldier->aiPlanning().flankCount() == MAX_FLANKS_YELLOW )
	{
		pSoldier->aiPlanning().advanceFlank();
		pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sFlankGridNo, AI_ACTION_SEEK_NOISE );
		return AI_ACTION_SEEK_NOISE;
	}
		
	////////////////////////////////////////////////////////////////////////////
	// SEEK NOISE
	////////////////////////////////////////////////////////////////////////////

	if ( fReachable )
	{
		// remember that noise value is negative, and closer to 0 => more important!
		iChance = 95 + (iNoiseValue / 3);
		iSneaky = 30;

		// increase

		// set base chance according to orders
		switch ( pSoldier->aiBehavior().orders() )
		{
		case STATIONARY:     iChance += -20;  break;
		case ONGUARD:        iChance += -15;  break;
		case ONCALL:                          break;
		case CLOSEPATROL:    iChance += -10;  break;
		case RNDPTPATROL:
		case POINTPATROL:                     break;
		case FARPATROL:      iChance += 10;  break;
		case SEEKENEMY:      iChance += 25;  break;
		case SNIPER:		  iChance += -10; break;
		}

		// modify chance of patrol (and whether it's a sneaky one) by attitude
		switch ( pSoldier->aiBehavior().attitude() )
		{
		case DEFENSIVE:      iChance += -10;  iSneaky += 15;  break;
		case BRAVESOLO:      iChance += 10;                   break;
		case BRAVEAID:       iChance += 5;                   break;
		case CUNNINGSOLO:    iChance += 5;  iSneaky += 30;  break;
		case CUNNINGAID:                      iSneaky += 30;  break;
		case AGGRESSIVE:     iChance += 20;  iSneaky += -10;  break;
		case ATTACKSLAYONLY:	iChance += 20;  iSneaky += -10;  break;
		}
		
		// reduce chance if breath is down, less likely to wander around when tired
		iChance -= (100 - pSoldier->vitals().breath());

		//Madd: make militia less likely to go running headlong into trouble
		if ( pSoldier->roster().team() == MILITIA_TEAM )
			iChance -= 30;

		if ( (INT16)PreRandom( 100 ) < iChance )
		{
			pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sNoiseGridNo, AI_ACTION_SEEK_NOISE );

			if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
#ifdef DEBUGDECISIONS
				sprintf( tempstr, "%s - INVESTIGATING NOISE at grid %d, moving to %d",
							pSoldier->identity().name(), sNoiseGridNo, pSoldier->aiPlanning().actionData() );
				AIPopMessage( tempstr );
#endif			

				// possibly start YELLOW flanking
				if( gGameExternalOptions.fAIYellowFlanking &&  
					(pSoldier->aiBehavior().attitude() == CUNNINGAID || pSoldier->aiBehavior().attitude() == CUNNINGSOLO) &&
					pSoldier->roster().team() == ENEMY_TEAM &&
					(CountFriendsInDirection( pSoldier, sNoiseGridNo ) > 0 || NightTime( )) &&
					(pSoldier->aiBehavior().orders() == SEEKENEMY ||
					pSoldier->aiBehavior().orders() == FARPATROL ||
					pSoldier->aiBehavior().orders() == CLOSEPATROL && NightTime( )) )
				{
					INT8 action = AI_ACTION_SEEK_NOISE;
					INT16 dist = PythSpacesAway( pSoldier->position().gridNo(), sNoiseGridNo );
					if ( dist > MIN_FLANK_DIST_YELLOW && dist < MAX_FLANK_DIST_YELLOW )
					{
						INT16 rdm = Random( 6 );

						switch ( rdm )
						{
						case 1:
						case 2:
						case 3:
							if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
								action = AI_ACTION_FLANK_LEFT;
							break;
						default:
							if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
								action = AI_ACTION_FLANK_RIGHT;
							break;
						}
					}
					else
						return AI_ACTION_SEEK_NOISE;

					pSoldier->aiPlanning().actionData() = FindFlankingSpot( pSoldier, sNoiseGridNo, action );

					if ( TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) || pSoldier->aiPlanning().flankCount() >= MAX_FLANKS_YELLOW )
					{
						pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sNoiseGridNo, AI_ACTION_SEEK_NOISE );
						//pSoldier->aiPlanning().clearFlank();
						return(AI_ACTION_SEEK_NOISE);
					}
					else
					{
						if ( action == AI_ACTION_FLANK_LEFT )
							pSoldier->aiPlanning().lastFlankLeft() = TRUE;
						else
							pSoldier->aiPlanning().lastFlankLeft() = FALSE;

						pSoldier->aiPlanning().recordFlankStep(
							sNoiseGridNo,
							GetDirectionFromGridNo( sNoiseGridNo, pSoldier ) );

						// sevenfm: change orders CLOSEPATROL -> FARPATROL
						if ( pSoldier->aiBehavior().orders() == CLOSEPATROL )
						{
							pSoldier->aiBehavior().orders() = FARPATROL;
						}

						return(action);
					}
				}
				else
				{
					return(AI_ACTION_SEEK_NOISE);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// SEEK FRIEND WHO LAST RADIOED IN TO REPORT NOISE
	////////////////////////////////////////////////////////////////////////////

	sClosestFriend = ClosestReachableFriendInTrouble( pSoldier, &fClimb );

	// if there is a friend alive & reachable who last radioed in		
	if ( !TileIsOutOfBounds( sClosestFriend ) )
	{
		// there a chance enemy soldier choose to go "help" his friend
		iChance = 50 - SpacesAway( pSoldier->position().gridNo(), sClosestFriend );
		iSneaky = 10;

		// set base chance according to orders
		switch ( pSoldier->aiBehavior().orders() )
		{
		case STATIONARY:     iChance += -20;  break;
		case ONGUARD:        iChance += -15;  break;
		case ONCALL:         iChance += 20;  break;
		case CLOSEPATROL:    iChance += -10;  break;
		case RNDPTPATROL:
		case POINTPATROL:    iChance += -10;  break;
		case FARPATROL:                       break;
		case SEEKENEMY:      iChance += 10;  break;
		case SNIPER:		  iChance += -10; break;
		}

		// modify chance of patrol (and whether it's a sneaky one) by attitude
		switch ( pSoldier->aiBehavior().attitude() )
		{
		case DEFENSIVE:      iChance += -10;  iSneaky += 15;        break;
		case BRAVESOLO:                                              break;
		case BRAVEAID:       iChance += 20;  iSneaky += -10;        break;
		case CUNNINGSOLO:					   iSneaky += 30;		  break;
		case CUNNINGAID:     iChance += 20;  iSneaky += 20;        break;
		case AGGRESSIVE:     iChance += -20;  iSneaky += -20;        break;
		case ATTACKSLAYONLY: iChance += -20;  iSneaky += -20;        break;
		}

		// reduce chance if breath is down, less likely to wander around when tired
		iChance -= (100 - pSoldier->vitals().breath());

		if ( (INT16)PreRandom( 100 ) < iChance )
		{
			pSoldier->aiPlanning().actionData() = GoAsFarAsPossibleTowards( pSoldier, sClosestFriend, AI_ACTION_SEEK_FRIEND );

			if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
#ifdef DEBUGDECISIONS
				sprintf( tempstr, "%s - SEEKING FRIEND at %d, MOVING to %d",
							pSoldier->identity().name(), sClosestFriend, pSoldier->aiPlanning().actionData() );
				AIPopMessage( tempstr );
#endif								

				return(AI_ACTION_SEEK_FRIEND);
			}
		}
	}
		
	////////////////////////////////////////////////////////////////////////////
	// TAKE BEST NEARBY COVER FROM THE NOISE GENERATING GRIDNO
	////////////////////////////////////////////////////////////////////////////

	if ( !SkipCoverCheck ) // && gfTurnBasedAI) // only do in turnbased
	{
		// remember that noise value is negative, and closer to 0 => more important!
		iChance = 25;
		iSneaky = 30;

		// set base chance according to orders
		switch ( pSoldier->aiBehavior().orders() )
		{
		case STATIONARY:     iChance += 20;  break;
		case ONGUARD:        iChance += 15;  break;
		case ONCALL:                          break;
		case CLOSEPATROL:    iChance += 10;  break;
		case RNDPTPATROL:
		case POINTPATROL:                     break;
		case FARPATROL:      iChance += -5;  break;
		case SEEKENEMY:      iChance += -20;  break;
		case SNIPER:		  iChance += 20; break;
		}

		// modify chance (and whether it's sneaky) by attitude
		switch ( pSoldier->aiBehavior().attitude() )
		{
		case DEFENSIVE:      iChance += 10;  iSneaky += 15;  break;
		case BRAVESOLO:      iChance += -15;  iSneaky += -20;  break;
		case BRAVEAID:       iChance += -20;  iSneaky += -20;  break;
		case CUNNINGSOLO:    iChance += 20;  iSneaky += 30;  break;
		case CUNNINGAID:     iChance += 15;  iSneaky += 30;  break;
		case AGGRESSIVE:     iChance += -10;  iSneaky += -10;  break;
		case ATTACKSLAYONLY: iChance += -10;  iSneaky += -10;  break;
		}

		// reduce chance if breath is down, less likely to wander around when tired
		iChance -= (100 - pSoldier->vitals().breath());

		if ( (INT16)PreRandom( 100 ) < iChance )
		{
			pSoldier->morale().aiMorale() = CalcMorale( pSoldier );
			pSoldier->aiPlanning().actionData() = FindBestNearbyCover( pSoldier, pSoldier->morale().aiMorale(), &iDummy );

			if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
#ifdef DEBUGDECISIONS
				sprintf( tempstr, "%s - TAKING COVER at grid %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
				AIPopMessage( tempstr );
#endif

				return(AI_ACTION_TAKE_COVER);
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// SWITCH TO GREEN: determine if soldier acts as if nothing at all was wrong
	////////////////////////////////////////////////////////////////////////////
	if ( (INT16)PreRandom( 100 ) < 50 )
	{
#ifdef DEBUGDECISIONS
		AINameMessage( pSoldier, "ignores noise completely and BYPASSES to GREEN!", 1000 );
#endif
		// Skip YELLOW until new situation, 15% extra chance to do GREEN actions
		pSoldier->aiBehavior().bypassToGreen() = 15;
		return(ArmedVehicleDecideActionGreen( pSoldier ));
	}
		
	////////////////////////////////////////////////////////////////////////////
	// DO NOTHING: Not enough points left to move, so save them for next turn
	////////////////////////////////////////////////////////////////////////////

#ifdef DEBUGDECISIONS
	AINameMessage( pSoldier, "- DOES NOTHING (YELLOW)", 1000 );
#endif

	// by default, if everything else fails, just stands in place without turning
	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);
}


INT8 ArmedVehicleDecideActionRed( TacticalActor *pSoldier)
{
	INT32 iDummy;
	INT32 iChance, sClosestOpponent, sClosestFriend;
	INT32 sClosestDisturbance, sDistVisible, sCheckGridNo;
	UINT8 ubCanMove, ubOpponentDir;
	INT8 bInWater, bInDeepWater;
	INT8 bSeekPts = 0, bHelpPts = 0, bHidePts = 0, bWatchPts = 0;
	INT8	bHighestWatchLoc;
	ATTACKTYPE BestThrow, BestShot;
	BOOLEAN fClimb = FALSE;

	// sevenfm:
	BOOLEAN fProneSightCover = FALSE;
	BOOLEAN fDangerousSpot = FALSE;

#ifdef AI_TIMING_TEST
	UINT32	uiStartTime, uiEndTime;
#endif
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif

	// Flugente: to prevent an accidental call
	if ( !ARMED_VEHICLE(pSoldier) )
		return DecideActionRed( pSoldier);
	
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: soldier orders = %d", pSoldier->aiBehavior().orders() ) );

	// if we have absolutely no action points, we can't do a thing under RED!
	if ( pSoldier->actionPoints().current() <= 0 ) //Action points can be negative
	{
		pSoldier->aiPlanning().actionData() = NOWHERE;
		return(AI_ACTION_NONE);
	}

	fProneSightCover = ProneSightCoverAtSpot(pSoldier, pSoldier->position().gridNo(), FALSE);
	if ( !fProneSightCover || pSoldier->suppression().underFire() )
	{
		fDangerousSpot = TRUE;
	}

	// can this guy move to any of the neighbouring squares ? (sets TRUE/FALSE)
	ubCanMove = (pSoldier->actionPoints().current() >= MinPtsToMove( pSoldier ));
		
	// determine if we happen to be in water (in which case we're in BIG trouble!)
	bInWater = Water( pSoldier->position().gridNo(), pSoldier->position().level() );
	bInDeepWater = DeepWater( pSoldier->position().gridNo(), pSoldier->position().level() );
				
	////////////////////////////////////////////////////////////////////////
	// IF POSSIBLE, FIRE LONG RANGE WEAPONS AT TARGETS REPORTED BY RADIO
	////////////////////////////////////////////////////////////////////////
	//if(!is_networked)//hayden
	//{
	// can't do this in realtime, because the player could be shooting a gun or whatever at the same time!
	if ( gfTurnBasedAI && !bInWater && (CanNPCAttack( pSoldier ) == TRUE) )
	{
		BestThrow.ubPossible = FALSE;    // by default, assume Throwing isn't possible

		CheckIfTossPossible( pSoldier, &BestThrow );

		if ( BestThrow.ubPossible )
		{
			// if firing mortar make sure we have room
			UINT16 usItem = pSoldier->inventory()[BestThrow.bWeaponIn].usItem;
			if (ItemIsMortar(usItem)
				 || ItemIsGrenadeLauncher(usItem)
				 || ItemIsFlare(usItem) )
			{
				ubOpponentDir = GetDirectionFromGridNo( BestThrow.sTarget, pSoldier );

				// Get new gridno!
				sCheckGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( ubOpponentDir ) );

				if ( OKFallDirection( pSoldier, sCheckGridNo, pSoldier->position().level(), ubOpponentDir, pSoldier->animationPlayback().state() ) )
				{
					// then do it!  The functions have already made sure that we have a
					// pair of worthy opponents, etc., so we're not just wasting our time

					// if necessary, swap the usItem from holster into the hand position
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "decideactionred: if necessary, swap the usItem from holster into the hand position" );
					if ( BestThrow.bWeaponIn != HANDPOS )
						RearrangePocket( pSoldier, HANDPOS, BestThrow.bWeaponIn, FOREVER );

					pSoldier->aiPlanning().actionData() = BestThrow.sTarget;
					//POSSIBLE STRUCTURE CHANGE PROBLEM, NOT CURRENTLY CHANGED. GOTTHARD 7/14/08
					pSoldier->aiPlanning().aimTime() = BestThrow.ubAimTime;

					return(AI_ACTION_TOSS_PROJECTILE);
				}
				else
				{
					// can't fire!
					BestThrow.ubPossible = FALSE;

					// try behind us, see if there's room to move back
					sCheckGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[ubOpponentDir] ) );
					if ( OKFallDirection( pSoldier, sCheckGridNo, pSoldier->position().level(), gOppositeDirection[ubOpponentDir], pSoldier->animationPlayback().state() ) )
					{
						pSoldier->aiPlanning().actionData() = sCheckGridNo;

						return(AI_ACTION_GET_CLOSER);
					}
				}
			}
		}
		else		// toss/throw/launch not possible
		{
			// WDS - Fix problem when there is no "best thrown" weapon (i.e., BestThrow.bWeaponIn == NO_SLOT)
			// if this dude has a longe-range weapon on him (longer than normal
			// sight range), and there's at least one other team-mate around, and
			// spotters haven't already been called for, then DO SO!

			if ( (BestThrow.bWeaponIn != NO_SLOT) &&
				 (CalcMaxTossRange( pSoldier, pSoldier->inventory()[BestThrow.bWeaponIn].usItem, TRUE ) > TacticalActorVisibility::normalMaximumDistance()) &&
				 (gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) &&
				 (gTacticalStatus.ubSpottersCalledForBy == NOBODY) )
			{
				// then call for spotters!  Uses up the rest of his turn (whatever
				// that may be), but from now on, BLACK AI NPC may radio sightings!
				gTacticalStatus.ubSpottersCalledForBy = pSoldier->identity().id();
				// HEADROCK HAM 3.1: This may be causing problems with HAM's lowered AP limit. From now on, we'll check
				// whether the soldier has more than 0 APs to begin with.
				if ( pSoldier->actionPoints().current() > 0 )
					pSoldier->actionPoints().current() = 0;

#ifdef DEBUGDECISIONS
				AINameMessage( pSoldier, "calls for spotters!", 1000 );
#endif

				pSoldier->aiPlanning().actionData() = NOWHERE;
				return(AI_ACTION_NONE);
			}
		}
		//}//hayden

		// SNIPER!
		CheckIfShotPossible(pSoldier, &BestShot);
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: is sniper shot possible? = %d, CTH = %d", BestShot.ubPossible, BestShot.ubChanceToReallyHit ) );

		if ( BestShot.ubPossible && BestShot.ubChanceToReallyHit > 50 )
		{
			// then do it!  The functions have already made sure that we have a
			// pair of worthy opponents, etc., so we're not just wasting our time

			// if necessary, swap the usItem from holster into the hand position
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: sniper shot possible!" );
			if ( BestShot.bWeaponIn != HANDPOS )
				RearrangePocket( pSoldier, HANDPOS, BestShot.bWeaponIn, FOREVER );

			pSoldier->aiPlanning().actionData() = BestShot.sTarget;
			//POSSIBLE STRUCTURE CHANGE PROBLEM. GOTTHARD 7/14/08
			pSoldier->aiPlanning().aimTime() = BestShot.ubAimTime;
			pSoldier->attackSelection().scopeMode() = BestShot.bScopeMode;
			// sevenfm: disabled for vehicles
			//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_SNIPER] );
			return(AI_ACTION_FIRE_GUN);
		}
		else		// snipe not possible
		{
			// if this dude has a longe-range weapon on him (longer than normal
			// sight range), and there's at least one other team-mate around, and
			// spotters haven't already been called for, then DO SO!

			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: sniper shot not possible" );
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: weapon in slot #%d", BestShot.bWeaponIn ) );
			// WDS - Fix problem when there is no "best shot" weapon (i.e., BestShot.bWeaponIn == NO_SLOT)
			if ( BestShot.bWeaponIn != NO_SLOT ) {
				OBJECTTYPE * gun = &pSoldier->inventory()[BestShot.bWeaponIn];
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: men in sector %d, ubspotters called by %d, nobody %d", gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector, gTacticalStatus.ubSpottersCalledForBy, NOBODY ) );
				if ( ((IsScoped( gun ) && GunRange( gun, pSoldier ) > TacticalActorVisibility::normalMaximumDistance()) || pSoldier->aiBehavior().orders() == SNIPER) && // SANDRO - added argument
					 (gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) &&
					 (gTacticalStatus.ubSpottersCalledForBy == NOBODY) )
				{
					// then call for spotters!  Uses up the rest of his turn (whatever
					// that may be), but from now on, BLACK AI NPC may radio sightings!
					gTacticalStatus.ubSpottersCalledForBy = pSoldier->identity().id();
					// HEADROCK HAM 3.1: This may be causing problems with HAM's lowered AP limit. From now on, we'll check
					// whether the soldier has more than 0 APs to begin with.
					if ( pSoldier->actionPoints().current() > 0 )
						pSoldier->actionPoints().current() = 0;

					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: calling for sniper spotters" );

					pSoldier->aiPlanning().actionData() = NOWHERE;
					return(AI_ACTION_NONE);
				}
			}
		}

		//RELOADING
		// WarmSteel - Because of suppression fire, we need enough ammo to even consider suppressing
		// This means we need to reload. Also reload if we're just plainly low on bullets.
		if ( BestShot.bWeaponIn != NO_SLOT
			 && ((pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft < gGameExternalOptions.ubAISuppressionMinimumAmmo && GetMagSize( &pSoldier->inventory()[BestShot.bWeaponIn] ) >= gGameExternalOptions.ubAISuppressionMinimumMagSize)
			 || pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft < (UINT8)(GetMagSize( &pSoldier->inventory()[BestShot.bWeaponIn] ) / 4)) )
		{
			// HEADROCK HAM 5: Fixed an issue where no ammo was found, leading to a crash when overloading the
			// inventory vector (bAmmoSlot = -1...)
			INT8 bAmmoSlot = FindAmmoToReload( pSoldier, BestShot.bWeaponIn, NO_SLOT );
			if ( bAmmoSlot > -1 )
			{
				OBJECTTYPE * pAmmo = &(pSoldier->inventory()[bAmmoSlot]);
				if ( (*pAmmo)[0]->data.ubShotsLeft > pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft && GetAPsToReloadGunWithAmmo( pSoldier, &(pSoldier->inventory()[BestShot.bWeaponIn]), pAmmo ) <= (INT16)pSoldier->actionPoints().current() )
				{
					pSoldier->aiPlanning().actionData() = BestShot.bWeaponIn;
					return AI_ACTION_RELOAD_GUN;
				}
			}
		}

		//SUPPRESSION FIRE
		CheckIfShotPossible(pSoldier, &BestShot); //WarmSteel - No longer returns 0 when there IS actually a chance to hit.
		TacticalActor* bestShotOpponent =
			GetJa2SoldierRepository().resolve(BestShot.ubOpponent.i);
		if (BestShot.ubPossible && !bestShotOpponent)
		{
			BestShot.ubPossible = FALSE;
		}

		// sevenfm: check that we have a clip to reload
		BOOLEAN fExtraClip = FALSE;
		if ( BestShot.bWeaponIn != NO_SLOT )
		{
			INT8 bAmmoSlot = FindAmmoToReload( pSoldier, BestShot.bWeaponIn, NO_SLOT );
			if ( bAmmoSlot != NO_SLOT )
			{
				fExtraClip = TRUE;
			}
		}

		//must have a small chance to hit and the opponent must be on the ground (can't suppress guys on the roof)
		// HEADROCK HAM BETA2.4: Adjusted this for a random chance to suppress regardless of chance. This augments
		// current revamp of suppression fire.

		// CHRISL: Changed from a simple flag to two externalized values for more modder control over AI suppression
		// WarmSteel - Don't *always* try to suppress when under 50 CTH
		if ( BestShot.bWeaponIn != -1
			 && BestShot.ubPossible
			 && GetMagSize( &pSoldier->inventory()[BestShot.bWeaponIn] ) >= gGameExternalOptions.ubAISuppressionMinimumMagSize
			 && pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft >= gGameExternalOptions.ubAISuppressionMinimumAmmo
			 //&& BestShot.ubChanceToReallyHit < (INT16)(PreRandom(50))
			 && pSoldier->aiBehavior().orders() != SNIPER &&
			 BestShot.ubFriendlyFireChance < 5 &&
			 bestShotOpponent &&
			 !TacticalActorConditions::isCowering(*bestShotOpponent) &&
			 !AICheckIsFlanking( pSoldier ) &&
			 LocationToLocationLineOfSightTest( pSoldier->position().gridNo(), pSoldier->position().level(), bestShotOpponent->position().gridNo(), bestShotOpponent->position().level(), TRUE, NO_DISTANCE_LIMIT ) &&
			 //Weapon[pSoldier->inventory()[BestShot.bWeaponIn].usItem].ubWeaponType == GUN_LMG ) &&	//Weapon[usInHand].ubWeaponClass == MGCLASS
			 (fExtraClip || pSoldier->inventory()[BestShot.bWeaponIn][0]->data.gun.ubGunShotsLeft > gGameExternalOptions.ubAISuppressionMinimumMagSize) )
		{
			// then do it!
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: suppression fire possible!" );

			pSoldier->aiPlanning().actionData() = BestShot.sTarget;
			pSoldier->targeting().level() = BestShot.bTargetLevel;
			pSoldier->aiPlanning().aimTime() = 0;
			pSoldier->fireControl().selectBurst();

			INT16 ubBurstAPs = 0;
			INT16 totalUsedAPs = 0;
			FLOAT dTotalRecoil = 0;
			auto& weapon = pSoldier->inventory()[BestShot.bWeaponIn];
			const auto remainingAmmo = weapon[0]->data.gun.ubGunShotsLeft;

			if ( UsingNewCTHSystem( ) )
			{
				do
				{
					pSoldier->fireControl().autofireShots()++;
					dTotalRecoil += AICalcRecoilForShot( pSoldier, &weapon, pSoldier->fireControl().autofireShots() );
					ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &weapon, pSoldier->fireControl().autofireShots(), pSoldier );
					totalUsedAPs = BestShot.ubAPCost + ubBurstAPs;
				} while ( pSoldier->actionPoints().current() >= totalUsedAPs && remainingAmmo >= pSoldier->fireControl().autofireShots() && dTotalRecoil <= 10.0f );
			}
			else
			{
				do
				{
					pSoldier->fireControl().autofireShots()++;
					ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &weapon, pSoldier->fireControl().autofireShots(), pSoldier );
					totalUsedAPs = BestShot.ubAPCost + ubBurstAPs;
				} while ( pSoldier->actionPoints().current() >= totalUsedAPs && remainingAmmo >= pSoldier->fireControl().autofireShots() &&
						 GetAutoPenalty( &weapon, gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE )*pSoldier->fireControl().autofireShots() <= 80 );
			}

			pSoldier->fireControl().autofireShots()--;

			// Make sure we decided to fire at least one shot!
			ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &weapon, pSoldier->fireControl().autofireShots(), pSoldier );

			// if necessary, swap the usItem from holster into the hand position
			if ( BestShot.bWeaponIn != HANDPOS )
			{
				RearrangePocket( pSoldier, HANDPOS, BestShot.bWeaponIn, FOREVER );
			}

			// minimum 5 bullets
			if ( pSoldier->fireControl().autofireShots() >= 5 && pSoldier->actionPoints().current() >= BestShot.ubAPCost + ubBurstAPs )
			{
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, New113Message[MSG113_SUPPRESSIONFIRE] );
				return(AI_ACTION_FIRE_GUN);
			}
			else
			{
				pSoldier->fireControl().selectSingleShot();
			}
		}
		// suppression not possible, do something else
	}
		
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: radio red alert?" );
	////////////////////////////////////////////////////////////////////////////
	// RADIO RED ALERT: determine %chance to call others and report contact
	////////////////////////////////////////////////////////////////////////////

	// if we're a computer merc, and we have the action points remaining to RADIO
	// (we never want NPCs to choose to radio if they would have to wait a turn)
	if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) && (gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: checking to radio red alert" );

		// if there hasn't been an initial RED ALERT yet in this sector
		if ( !(gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition) || NeedToRadioAboutPanicTrigger( ) )
			// since I'm at STATUS RED, I obviously know we're being invaded!
			iChance = gbDiff[DIFF_RADIO_RED_ALERT][SoldierDifficultyLevel( pSoldier )];
		else // subsequent radioing (only to update enemy positions, request help)
			// base chance depends on how much new info we have to radio to the others
			iChance = 10 * WhatIKnowThatPublicDont( pSoldier, FALSE );  // use 10 * for RED alert

		// if I actually know something they don't and I ain't swimming (deep water)
		if ( iChance && !bInDeepWater )
		{
			// modify base chance according to orders
			switch ( pSoldier->aiBehavior().orders() )
			{
			case STATIONARY:       iChance += 20;  break;
			case ONGUARD:          iChance += 15;  break;
			case ONCALL:           iChance += 10;  break;
			case CLOSEPATROL:                       break;
			case RNDPTPATROL:
			case POINTPATROL:      iChance += -5;  break;
			case FARPATROL:        iChance += -10;  break;
			case SEEKENEMY:        iChance += -20;  break;
			case SNIPER:			  iChance += -10;  break; // Sniper contacts should be reported automatically
			}

			// modify base chance according to attitude
			switch ( pSoldier->aiBehavior().attitude() )
			{
			case DEFENSIVE:        iChance += 20;  break;
			case BRAVESOLO:        iChance += -10;  break;
			case BRAVEAID:                          break;
			case CUNNINGSOLO:      iChance += -5;  break;
			case CUNNINGAID:                        break;
			case AGGRESSIVE:       iChance += -20;  break;
			case ATTACKSLAYONLY:		iChance = 0;
			}

			if ( (gTacticalStatus.fPanicFlags & PANIC_TRIGGERS_HERE) && !gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition )
			{
				// ignore morale (which could be really high
			}
			else
			{
				// modify base chance according to morale
				switch ( pSoldier->morale().aiMorale() )
				{
				case MORALE_HOPELESS:  iChance *= 3;    break;
				case MORALE_WORRIED:   iChance *= 2;    break;
				case MORALE_NORMAL:                     break;
				case MORALE_CONFIDENT: iChance /= 2;    break;
				case MORALE_FEARLESS:  iChance /= 3;    break;
				}
			}

#ifdef DEBUGDECISIONS
			AINumMessage( "Chance to radio RED alert = ", iChance );
#endif

			if ( (INT16)PreRandom( 100 ) < iChance )
			{
#ifdef DEBUGDECISIONS
				AINameMessage( pSoldier, "decides to radio a RED alert!", 1000 );
#endif

				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: decided to radio red alert" );
				return(AI_ACTION_RED_ALERT);
			}
		}
	}

	if ( gGameExternalOptions.fEnemyTanksCanMoveInTactical )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: main red ai" );

		// sevenfm: avoid light if spot is dangerous and no friends see my closest enemy
		if ( ubCanMove &&
			 InLightAtNight( pSoldier->position().gridNo(), pSoldier->position().level() ) &&
			 pSoldier->aiBehavior().orders() != STATIONARY &&
			 pSoldier->aiBehavior().orders() != SNIPER &&
			 CountFriendsBlack( pSoldier ) == 0 )
		{
			pSoldier->aiPlanning().actionData() = FindNearbyDarkerSpot( pSoldier );

			if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
			{
				// move as if leaving water or gas
				return(AI_ACTION_LEAVE_WATER_GAS);
			}
		}

		////////////////////////////////////////////////////////////////////////////
		// MAIN RED AI: Decide soldier's preference between SEEKING,HELPING & HIDING
		////////////////////////////////////////////////////////////////////////////

		// get the location of the closest reachable opponent
		sClosestDisturbance = ClosestReachableDisturbance( pSoldier, &fClimb );

		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: check to continue flanking" );
		// continue flanking
		INT32 sFlankGridNo;

		if ( !TileIsOutOfBounds( sClosestDisturbance ) )
			sFlankGridNo = sClosestDisturbance;
		else
			sFlankGridNo = pSoldier->aiPlanning().flankAnchorGrid();
			
		// continue flanking
		// sevenfm: dont' flank when under fire
		if ( pSoldier->aiPlanning().flankCount() > 0 &&
			 pSoldier->aiPlanning().flankCount() < MAX_FLANKS_RED  &&
			 gAnimControl[pSoldier->animationPlayback().state()].ubHeight != ANIM_PRONE &&
			 !pSoldier->suppression().underFire() )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: continue flanking" );
			INT16 currDir = GetDirectionFromGridNo( sFlankGridNo, pSoldier );
			INT16 origDir = pSoldier->aiPlanning().flankOriginDirection();
			pSoldier->aiPlanning().advanceFlank();
			if ( pSoldier->aiPlanning().lastFlankLeft() )
			{
				if ( origDir > currDir )
					origDir -= NUM_WORLD_DIRECTIONS;

				// stop flanking condition
				if ( (currDir - origDir) >= MinFlankDirections( pSoldier ) )
				{
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
				else
				{
					pSoldier->aiPlanning().actionData() = FindFlankingSpot( pSoldier, sFlankGridNo, AI_ACTION_FLANK_LEFT );

					if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) ) //&& (currDir - origDir) < 2 )
						return AI_ACTION_FLANK_LEFT;
					else
						pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
			}
			else
			{
				if ( origDir < currDir )
					origDir += NUM_WORLD_DIRECTIONS;

				// stop flanking condition
				if ( (origDir - currDir) >= MinFlankDirections( pSoldier ) )
				{
					pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
				else
				{
					pSoldier->aiPlanning().actionData() = FindFlankingSpot( pSoldier, sFlankGridNo, AI_ACTION_FLANK_RIGHT );

					if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )//&& (origDir - currDir) < 2 )
						return AI_ACTION_FLANK_RIGHT;
					else
						pSoldier->aiPlanning().finishFlank(MAX_FLANKS_RED);
				}
			}
		}

		// sevenfm: when we finished flanking, try to reach the flank anchor position
		// seek until we are close (DistanceVisible/2) and have line of sight to the flank anchor position
		// don't seek if we have seen enemy recently or under fire or have shock
		// don't seek if we have low AP (tired, wounded)
		if ( pSoldier->aiPlanning().flankCount() == MAX_FLANKS_RED )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "decideactionred: stop flanking" );

			// start end flank approach with full APs
			if ( gfTurnBasedAI && pSoldier->actionPoints().current() < pSoldier->actionPoints().initial() )
			{
				return(AI_ACTION_END_TURN);
			}

			if ( !TileIsOutOfBounds( sFlankGridNo ) &&
				 !GuySawEnemy( pSoldier ) &&
				 !pSoldier->suppression().underFire() &&
				 !Water( pSoldier->position().gridNo(), pSoldier->position().level() ) &&
				 pSoldier->actionPoints().initial() >= APBPConstants[AP_MINIMUM] &&
				 (PythSpacesAway( pSoldier->position().gridNo(), sFlankGridNo ) > MIN_FLANK_DIST_RED ||
				 !LocationToLocationLineOfSightTest( pSoldier->position().gridNo(), pSoldier->position().level(), sFlankGridNo, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS )) )
			{
				pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sFlankGridNo, GetAPsCrouch( pSoldier, TRUE ), AI_ACTION_SEEK_OPPONENT, 0 );

				// sevenfm: avoid going into water, gas or light
				if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) &&
					 !Water( pSoldier->aiPlanning().actionData(), pSoldier->position().level() ) &&
					 !InGas( pSoldier, pSoldier->aiPlanning().actionData() ) &&
					 !InLightAtNight( pSoldier->aiPlanning().actionData(), pSoldier->position().level() ) )
				{
					// if soldier can be seen at new position and he cannot be seen at his current position
					if ( LocationToLocationLineOfSightTest( pSoldier->aiPlanning().actionData(), pSoldier->position().level(), sFlankGridNo, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS ) &&
						 !LocationToLocationLineOfSightTest( pSoldier->position().gridNo(), pSoldier->position().level(), sFlankGridNo, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS ) )
					{
						// reserve APs for a possible crouch plus a shot
						INT32 sCautiousGridNo = InternalGoAsFarAsPossibleTowards( pSoldier, sFlankGridNo, (INT8)(MinAPsToAttack( pSoldier, sFlankGridNo, ADDTURNCOST, 0 ) + GetAPsCrouch( pSoldier, TRUE ) + GetAPsToLook( pSoldier )), AI_ACTION_SEEK_OPPONENT, FLAG_CAUTIOUS );

						if ( !TileIsOutOfBounds( sCautiousGridNo ) )
						{
							pSoldier->aiPlanning().actionData() = sCautiousGridNo;
							pSoldier->aiBehavior().flags() |= AI_CAUTIOUS;
							pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
							return(AI_ACTION_SEEK_OPPONENT);
						}
						return(AI_ACTION_SEEK_OPPONENT);
					}
					else
					{
						return(AI_ACTION_SEEK_OPPONENT);
					}
				}
				else
				{
					// if we cannot advance to spot, stop trying
					pSoldier->aiPlanning().advanceFlank();
				}
			}
			else
			{
				// stop
				pSoldier->aiPlanning().advanceFlank();
			}
		}
		
		// if we can move at least 1 square's worth
		// and have more APs than we want to reserve
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: can we move? = %d, APs = %d", ubCanMove, pSoldier->actionPoints().current() ) );

		if ( ubCanMove && pSoldier->actionPoints().current() > APBPConstants[MAX_AP_CARRIED] )
		{	
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: checking hide/seek/help/watch points... orders = %d, attitude = %d", pSoldier->aiBehavior().orders(), pSoldier->aiBehavior().attitude() ) );
				// calculate initial points for watch based on highest watch loc

				bWatchPts = GetHighestWatchedLocPoints( pSoldier->identity().id() );
				if ( bWatchPts <= 0 )
				{
					// no watching
					bWatchPts = -99;
				}

				// modify RED movement tendencies according to morale
				switch ( pSoldier->morale().aiMorale() )
				{
				case MORALE_HOPELESS:  bSeekPts = -99; bHelpPts = -99; bHidePts = +2; bWatchPts = -99; break;
				case MORALE_WORRIED:   bSeekPts += -2; bHelpPts += 0; bHidePts += +2; bWatchPts += 1; break;
				case MORALE_NORMAL:    bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
				case MORALE_CONFIDENT: bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += 0; break;
				case MORALE_FEARLESS:  bSeekPts += +1; bHelpPts += 0; bHidePts = -1; bWatchPts += 0; break;
				}

				// modify tendencies according to orders
				switch ( pSoldier->aiBehavior().orders() )
				{
				case STATIONARY:   bSeekPts += -1; bHelpPts += -1; bHidePts += +1; bWatchPts += +1; break;
				case ONGUARD:      bSeekPts += -1; bHelpPts += 0; bHidePts += +1; bWatchPts += +1; break;
				case CLOSEPATROL:  bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
				case RNDPTPATROL:  bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
				case POINTPATROL:  bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
				case FARPATROL:    bSeekPts += 0; bHelpPts += 0; bHidePts += 0; bWatchPts += 0; break;
				case ONCALL:       bSeekPts += 0; bHelpPts += +1; bHidePts += -1; bWatchPts += 0; break;
				case SEEKENEMY:    bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += -1; break;
				case SNIPER:		bSeekPts += -1; bHelpPts += 0; bHidePts += +1; bWatchPts += +1; break;
				}

				// modify tendencies according to attitude
				switch ( pSoldier->aiBehavior().attitude() )
				{
				case DEFENSIVE:     bSeekPts += -1; bHelpPts += 0; bHidePts += +2; bWatchPts += +1; break;
				case BRAVESOLO:     bSeekPts += +1; bHelpPts += -1; bHidePts += -1; bWatchPts += -1; break;
				case BRAVEAID:      bSeekPts += +1; bHelpPts += +1; bHidePts += -1; bWatchPts += -1; break;
				case CUNNINGSOLO:   bSeekPts += 1; bHelpPts += -1; bHidePts += +1; bWatchPts += 0; break;
				case CUNNINGAID:    bSeekPts += 1; bHelpPts += +1; bHidePts += +1; bWatchPts += 0; break;
				case AGGRESSIVE:    bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += 0; break;
				case ATTACKSLAYONLY:bSeekPts += +1; bHelpPts += 0; bHidePts += -1; bWatchPts += 0; break;
				}
								
				// sevenfm: disable watching if soldier is under fire or in dangerous place
				// don't watch if some friends can see my closest opponent
				if ( fDangerousSpot ||
					 InLightAtNight( pSoldier->position().gridNo(), pSoldier->position().level() ) ||
					 CountFriendsBlack( pSoldier ) > 0 )
				{
					bWatchPts -= 10;
				}

				// sevenfm: don't watch when overcrowded and not in a building
				if ( !InARoom( pSoldier->position().gridNo(), NULL ) )
				{
					bWatchPts -= CountNearbyFriends( pSoldier, pSoldier->position().gridNo(), TACTICAL_RANGE / 8 );
				}

				// sevenfm: don't help if seen enemy recently or under fire
				if ( GuySawEnemy( pSoldier ) || pSoldier->suppression().underFire() )
				{
					bHelpPts -= 10;
				}
			}

			if ( !gfTurnBasedAI )
			{
				// don't search for cover
				bHidePts = -99;
			}

			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: hide = %d, seek = %d, watch = %d, help = %d", bHidePts, bSeekPts, bWatchPts, bHelpPts ) );
			// while one of the three main RED REACTIONS remains viable
			while ( (bSeekPts > -90) || (bHelpPts > -90) || (bHidePts > -90) )
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: checking to seek" );
				// if SEEKING is possible and at least as desirable as helping or hiding
				if ( ((bSeekPts > -90) && (bSeekPts >= bHelpPts) && (bSeekPts >= bHidePts) && (bSeekPts >= bWatchPts)) )
				{
#ifdef AI_TIMING_TESTS
					uiStartTime = GetJA2Clock( );
#endif

#ifdef AI_TIMING_TESTS
					uiEndTime = GetJA2Clock( );
					guiRedSeekTimeTotal += (uiEndTime - uiStartTime);
					guiRedSeekCounter++;
#endif
					// if there is an opponent reachable					
					// sevenfm: allow seeking in prone stance if we haven't seen enemy for several turns
					if ( !TileIsOutOfBounds( sClosestDisturbance ) &&
						 (gAnimControl[pSoldier->animationPlayback().state()].ubHeight != ANIM_PRONE || !GuySawEnemy( pSoldier )) )
					{
						DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: seek opponent" );
						//////////////////////////////////////////////////////////////////////
						// SEEK CLOSEST DISTURBANCE: GO DIRECTLY TOWARDS CLOSEST KNOWN OPPONENT
						//////////////////////////////////////////////////////////////////////

						// try to move towards him
						pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sClosestDisturbance, GetAPsCrouch( pSoldier, TRUE ), AI_ACTION_SEEK_OPPONENT, 0 );

						if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
						{
							// Check for a trap
							if ( !ArmySeesOpponents( ) )
							{
								if ( GetNearestRottingCorpseAIWarning( pSoldier->aiPlanning().actionData() ) > 0 )
								{
									// abort! abort!
									pSoldier->aiPlanning().actionData() = NOWHERE;
								}
							}
						}

						// if it's possible						
						if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
						{
#ifdef DEBUGDECISIONS
							// do it!
							sprintf( tempstr, "%s - SEEKING OPPONENT at grid %d, MOVING to %d",
									 pSoldier->identity().name(), sClosestDisturbance, pSoldier->aiPlanning().actionData() );
							AIPopMessage( tempstr );
#endif

							BOOLEAN fOvercrowded = FALSE;
							if ( CountNearbyFriends( pSoldier, pSoldier->position().gridNo(), TACTICAL_RANGE / 4 ) > 2 )
							{
								fOvercrowded = TRUE;
							}

							// sevenfm: possibly start RED flanking
							if ( (pSoldier->aiBehavior().attitude() == CUNNINGAID || pSoldier->aiBehavior().attitude() == CUNNINGSOLO ||
								(pSoldier->aiBehavior().attitude() == BRAVESOLO || pSoldier->aiBehavior().attitude() == BRAVEAID) && fOvercrowded) &&
								pSoldier->roster().team() == ENEMY_TEAM &&
								gAnimControl[pSoldier->animationPlayback().state()].ubHeight != ANIM_PRONE &&
								!pSoldier->suppression().underFire() &&
								pSoldier->position().level() == 0 &&
								(pSoldier->aiBehavior().orders() == SEEKENEMY ||
								pSoldier->aiBehavior().orders() == FARPATROL ||
								pSoldier->aiBehavior().orders() == CLOSEPATROL && NightTime( )) &&
								(!GuySawEnemy( pSoldier ) || fOvercrowded) &&
								!Water( pSoldier->position().gridNo(), pSoldier->position().level() ) &&
								pSoldier->actionPoints().current() >= APBPConstants[AP_MINIMUM] &&
								(CountFriendsInDirection( pSoldier, sClosestDisturbance ) > 1 || NightTime( ) || fOvercrowded) )
							{
								INT8 action = AI_ACTION_SEEK_OPPONENT;
								INT16 dist = PythSpacesAway( pSoldier->position().gridNo(), sClosestDisturbance );
								if ( dist > MIN_FLANK_DIST_RED  && dist < MAX_FLANK_DIST_RED )
								{
									INT16 rdm = Random( 6 );

									switch ( rdm )
									{
									case 1:
									case 2:
									case 3:
										if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
											action = AI_ACTION_FLANK_LEFT;
										break;
									default:
										if ( pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_LEFT && pSoldier->aiPlanning().lastAction() != AI_ACTION_FLANK_RIGHT )
											action = AI_ACTION_FLANK_RIGHT;
										break;
									}

									if ( action == AI_ACTION_SEEK_OPPONENT ) {
										return action;
									}
								}
								else
									return AI_ACTION_SEEK_OPPONENT;

								pSoldier->aiPlanning().actionData() = FindFlankingSpot( pSoldier, sClosestDisturbance, action );

								if ( TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) || pSoldier->aiPlanning().flankCount() >= MAX_FLANKS_RED )
								{
									pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sClosestDisturbance, GetAPsCrouch( pSoldier, TRUE ), AI_ACTION_SEEK_OPPONENT, 0 );
									//pSoldier->aiPlanning().clearFlank();
									if ( PythSpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) < 5 || LocationToLocationLineOfSightTest( pSoldier->aiPlanning().actionData(), pSoldier->position().level(), sClosestDisturbance, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS ) )
									{
										// reserve APs for a possible crouch plus a shot
										pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sClosestDisturbance, (INT8)(MinAPsToAttack( pSoldier, sClosestDisturbance, ADDTURNCOST, 0 ) + GetAPsCrouch( pSoldier, TRUE )), AI_ACTION_SEEK_OPPONENT, FLAG_CAUTIOUS );

										if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
										{
											pSoldier->aiBehavior().flags() |= AI_CAUTIOUS;
											pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
											return(AI_ACTION_SEEK_OPPONENT);
										}
									}

									else
									{
										return(AI_ACTION_SEEK_OPPONENT);
									}
								}
								else
								{
									if ( action == AI_ACTION_FLANK_LEFT )
										pSoldier->aiPlanning().lastFlankLeft() = TRUE;
									else
										pSoldier->aiPlanning().lastFlankLeft() = FALSE;

									pSoldier->aiPlanning().recordFlankStep(
										sClosestDisturbance,
										GetDirectionFromGridNo( sClosestDisturbance, pSoldier ) );

									// sevenfm: change orders when starting to flank
									if ( pSoldier->aiBehavior().orders() == CLOSEPATROL )
									{
										pSoldier->aiBehavior().orders() = FARPATROL;
									}

									return(action);
								}
							}
							else
							{
								// let's be a bit cautious about going right up to a location without enough APs to shoot
								if ( PythSpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) < 5 || LocationToLocationLineOfSightTest( pSoldier->aiPlanning().actionData(), pSoldier->position().level(), sClosestDisturbance, pSoldier->position().level(), TRUE, CALC_FROM_ALL_DIRS ) )
								{
									// reserve APs for a possible crouch plus a shot
									pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sClosestDisturbance, (INT8)(MinAPsToAttack( pSoldier, sClosestDisturbance, ADDTURNCOST, 0 ) + GetAPsCrouch( pSoldier, TRUE )), AI_ACTION_SEEK_OPPONENT, FLAG_CAUTIOUS );

									if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
									{
										pSoldier->aiBehavior().flags() |= AI_CAUTIOUS;
										pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
										return(AI_ACTION_SEEK_OPPONENT);
									}
								}
								else
								{
									return(AI_ACTION_SEEK_OPPONENT);
								}
								break;
							}
						}
					}

					// mark SEEKING as impossible for next time through while loop
#ifdef DEBUGDECISIONS
					AINameMessage( pSoldier, "couldn't SEEK...", 1000 );
#endif
					bSeekPts = -99;
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: couldn't seek" );
				}

				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: checking to watch" );
				// if WATCHING is possible and at least as desirable as anything else
				if ( (bWatchPts > -90) && (bWatchPts >= bSeekPts) && (bWatchPts >= bHelpPts) && (bWatchPts >= bHidePts) )
				{
					// take a look at our highest watch point... if it's still visible, turn to face it and then wait
					bHighestWatchLoc = GetHighestVisibleWatchedLoc( pSoldier->identity().id() );
					//sDistVisible =  TacticalActorVisibility::distance(*pSoldier, DIRECTION_IRRELEVANT, DIRECTION_IRRELEVANT, gsWatchedLoc[ pSoldier->identity().id() ][ bHighestWatchLoc ] );

					if ( bHighestWatchLoc != -1 )
					{
						// see if we need turn to face that location
						ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), gsWatchedLoc[pSoldier->identity().id()][bHighestWatchLoc]);

						// if soldier is not already facing in that direction,
						// and the opponent is close enough that he could possibly be seen
						if ( pSoldier->position().direction() != ubOpponentDir &&
							 TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubOpponentDir) &&
							 pSoldier->actionPoints().current() >= GetAPsToLook( pSoldier ) )
						{
							// turn
							pSoldier->aiPlanning().actionData() = ubOpponentDir;

							return(AI_ACTION_CHANGE_FACING);
						}												

						return(AI_ACTION_NONE);
					}

					bWatchPts = -99;
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: couldn't watch" );
				}
				
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: checking to help" );
				// if HELPING is possible and at least as desirable as seeking or hiding
				if ( (bHelpPts > -90) && (bHelpPts >= bSeekPts) && (bHelpPts >= bHidePts) && (bHelpPts >= bWatchPts) )
				{
#ifdef AI_TIMING_TESTS
					uiStartTime = GetJA2Clock( );
#endif
					sClosestFriend = ClosestReachableFriendInTrouble( pSoldier, &fClimb );
#ifdef AI_TIMING_TESTS
					uiEndTime = GetJA2Clock( );

					guiRedHelpTimeTotal += (uiEndTime - uiStartTime);
					guiRedHelpCounter++;
#endif
					//WarmSteel - Dont try if we're already quite close to our friend
					// sevenfm: reverted to vanilla helping
					//if (!TileIsOutOfBounds(sClosestFriend) && PythSpacesAway(pSoldier->sGridNo, sClosestFriend) > TacticalActorVisibility::maximumDistance(*pSoldier, sClosestFriend, 0, CALC_FROM_ALL_DIRS ))
					if ( !TileIsOutOfBounds( sClosestFriend ) )
					{
						//////////////////////////////////////////////////////////////////////
						// GO DIRECTLY TOWARDS CLOSEST FRIEND UNDER FIRE OR WHO LAST RADIOED
						//////////////////////////////////////////////////////////////////////
						pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sClosestFriend, GetAPsCrouch( pSoldier, TRUE ), AI_ACTION_SEEK_OPPONENT, 0 );

						if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
						{
#ifdef DEBUGDECISIONS
							sprintf( tempstr, "%s - SEEKING FRIEND at %d, MOVING to %d",
									 pSoldier->identity().name(), sClosestFriend, pSoldier->aiPlanning().actionData() );
							AIPopMessage( tempstr );
#endif

							return(AI_ACTION_SEEK_FRIEND);
						}
					}

					// mark SEEKING as impossible for next time through while loop
#ifdef DEBUGDECISIONS
					AINameMessage( pSoldier, "couldn't HELP...", 1000 );
#endif

					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: couldn't help" );
					bHelpPts = -99;
				}

				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: checking to hide" );
				// if HIDING is possible and at least as desirable as seeking or helping
				if ( (bHidePts > -90) && (bHidePts >= bSeekPts) && (bHidePts >= bHelpPts) && (bHidePts >= bWatchPts) )
				{
					sClosestOpponent = ClosestKnownOpponent( pSoldier, NULL, NULL );
					// if an opponent is known (not necessarily reachable or conscious)					
					if ( !SkipCoverCheck && !TileIsOutOfBounds( sClosestOpponent ) )
					{
						//////////////////////////////////////////////////////////////////////
						// TAKE BEST NEARBY COVER FROM ALL KNOWN OPPONENTS
						//////////////////////////////////////////////////////////////////////
#ifdef AI_TIMING_TESTS
						uiStartTime = GetJA2Clock( );
#endif

						pSoldier->aiPlanning().actionData() = FindBestNearbyCover( pSoldier, pSoldier->morale().aiMorale(), &iDummy );
#ifdef AI_TIMING_TESTS
						uiEndTime = GetJA2Clock( );

						guiRedHideTimeTotal += (uiEndTime - uiStartTime);
						guiRedHideCounter++;
#endif

						// let's be a bit cautious about going right up to a location without enough APs to shoot						
						if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
						{
							sClosestDisturbance = ClosestReachableDisturbance( pSoldier, &fClimb );
							if ( !TileIsOutOfBounds( sClosestDisturbance ) && (SpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) < 5 || SpacesAway( pSoldier->aiPlanning().actionData(), sClosestDisturbance ) + 5 < SpacesAway( pSoldier->position().gridNo(), sClosestDisturbance )) )
							{
								// either moving significantly closer or into very close range
								// ensure will we have enough APs for a possible crouch plus a shot
								if ( InternalGoAsFarAsPossibleTowards( pSoldier, pSoldier->aiPlanning().actionData(), (INT8)(MinAPsToAttack( pSoldier, sClosestOpponent, ADDTURNCOST, 0 ) + GetAPsCrouch( pSoldier, TRUE )), AI_ACTION_TAKE_COVER, 0 ) == pSoldier->aiPlanning().actionData() )
								{
									return(AI_ACTION_TAKE_COVER);
								}
							}
							else
							{
								return(AI_ACTION_TAKE_COVER);
							}
						}
					}

					// mark HIDING as impossible for next time through while loop
#ifdef DEBUGDECISIONS
					AINameMessage( pSoldier, "couldn't HIDE...", 1000 );
#endif

					bHidePts = -99;
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: couldn't hide" );
				}
			}
		}
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: nothing to do!" );
		////////////////////////////////////////////////////////////////////////////
		// NOTHING USEFUL POSSIBLE!  IF NPC IS CURRENTLY UNDER FIRE, TRY TO RUN AWAY
		////////////////////////////////////////////////////////////////////////////

		// if we're currently under fire (presumably, attacker is hidden)
		if ( pSoldier->suppression().underFire() )
		{
			// Flugente: see if we are equipped with a smoke screen. If so, use it do hide us
			if (TacticalActorConditions::hasTakenLargeHit(*pSoldier) && TacticalActorEquipment::hasItem(*pSoldier, SMOKE_GRENADE) && IsActionAffordable(pSoldier, AI_ACTION_SELFDETONATE))
			{
				pSoldier->aiPlanning().actionData() = SMOKE_GRENADE;

				return AI_ACTION_SELFDETONATE;
			}

			// only try to run if we've actually been hit recently & noticably so
			// otherwise, presumably our current cover is pretty good & sufficient
			// HEADROCK HAM B2.6: New value here helps us change the ratio of running away due to shock. This
			// is terribly important if Suppression Shock is enabled.
			UINT16 bShock = 0;

			if ( gGameExternalOptions.usSuppressionShockEffect > 0 )
			{
				// If bShock value is greater than (2*ExpLevel + MoraleModifier)*1.5, the target will flee.
				bShock = pSoldier->suppression().shock();
				if ( bShock <= ((float)CalcSuppressionTolerance( pSoldier )*(float)1.5) )
					bShock = 0;
			}
			else
			{
				bShock = pSoldier->suppression().shock();
			}

			if ( bShock > 0 )
			{
				// look for best place to RUN AWAY to (farthest from the closest threat)
				pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents( pSoldier );

				if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
				{
#ifdef DEBUGDECISIONS
					sprintf( tempstr, "%s RUNNING AWAY to grid %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
					AIPopMessage( tempstr );
#endif

					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: run away!" );
					return(AI_ACTION_RUN_AWAY);
				}
			}				
		}
	}

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionRed: look around towards opponent" );
	////////////////////////////////////////////////////////////////////////////
	// LOOK AROUND TOWARD CLOSEST KNOWN OPPONENT, IF KNOWN
	////////////////////////////////////////////////////////////////////////////

	if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
	{
		// determine the location of the known closest opponent
		// (don't care if he's conscious, don't care if he's reachable at all)
		sClosestOpponent = ClosestKnownOpponent( pSoldier, NULL, NULL );

		if ( !TileIsOutOfBounds( sClosestOpponent ) )
		{
			// determine direction from this soldier to the closest opponent
			ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestOpponent);

			// if soldier is not already facing in that direction,
			// and the opponent is close enough that he could possibly be seen
			// note, have to change this to use the level returned from ClosestKnownOpponent
			sDistVisible = TacticalActorVisibility::maximumDistance(*pSoldier,  sClosestOpponent, 0, CALC_FROM_ALL_DIRS );

			if ( (pSoldier->position().direction() != ubOpponentDir) && (PythSpacesAway( pSoldier->position().gridNo(), sClosestOpponent ) <= sDistVisible) )
			{
				// set base chance according to orders
				if ( (pSoldier->aiBehavior().orders() == STATIONARY) || (pSoldier->aiBehavior().orders() == ONGUARD) )
					iChance = 50;
				else           // all other orders
					iChance = 25;

				if ( pSoldier->aiBehavior().attitude() == DEFENSIVE )
					iChance += 25;

				if ( ARMED_VEHICLE( pSoldier ) )
				{
					iChance += 50;
				}

				if ( (INT16)PreRandom( 100 ) < iChance && TacticalActorMobility::isCurrentStanceValid(*pSoldier, ubOpponentDir) )
				{
					pSoldier->aiPlanning().actionData() = ubOpponentDir;

#ifdef DEBUGDECISIONS
					sprintf( tempstr, "%s - TURNS TOWARDS CLOSEST ENEMY to face direction %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
					AIPopMessage( tempstr );
#endif				

					return(AI_ACTION_CHANGE_FACING);
				}
			}
		}
	}

	// try turning in a random direction as we still can't see anyone.
	if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
	{
		sClosestDisturbance = MostImportantNoiseHeard( pSoldier, NULL, NULL, NULL );

		if ( !TileIsOutOfBounds( sClosestDisturbance ) )
		{
			ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestDisturbance);
			if ( pSoldier->position().direction() == ubOpponentDir )
			{
				ubOpponentDir = (UINT8)PreRandom( NUM_WORLD_DIRECTIONS );
			}
		}
		else
		{
			ubOpponentDir = (UINT8)PreRandom( NUM_WORLD_DIRECTIONS );
		}

		if ( (pSoldier->position().direction() != ubOpponentDir) )
		{
			if ( (pSoldier->actionPoints().current() == pSoldier->actionPoints().initial() || (INT16)PreRandom( 100 ) < 60) )
			{
				pSoldier->aiPlanning().actionData() = ubOpponentDir;

#ifdef DEBUGDECISIONS
				sprintf( tempstr, "%s - TURNS TOWARDS CLOSEST ENEMY to face direction %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
				AIPopMessage( tempstr );
#endif

				// limit turning a bit... if the last thing we did was also a turn, add a 60% chance of this being our last turn
				if ( pSoldier->aiPlanning().lastAction() == AI_ACTION_CHANGE_FACING && PreRandom( 100 ) < 60 )
				{
					if ( gfTurnBasedAI )
					{
						pSoldier->aiPlanning().nextAction() = AI_ACTION_END_TURN;
					}
					else
					{
						pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
						pSoldier->aiPlanning().nextActionData() = (UINT16)REALTIME_AI_DELAY;
					}
				}

				return(AI_ACTION_CHANGE_FACING);
			}
		}
	}
	
	////////////////////////////////////////////////////////////////////////////
	// IF UNDER FIRE, FACE THE MOST IMPORTANT NOISE WE KNOW AND GO PRONE
	////////////////////////////////////////////////////////////////////////////

	if ( pSoldier->suppression().underFire() && pSoldier->actionPoints().current() >= (pSoldier->actionPoints().initial() - GetAPsToLook( pSoldier )) )
	{
		sClosestDisturbance = MostImportantNoiseHeard( pSoldier, NULL, NULL, NULL );

		if ( !TileIsOutOfBounds( sClosestDisturbance ) )
		{
			ubOpponentDir = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestDisturbance);
			if ( pSoldier->position().direction() != ubOpponentDir )
			{
				if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
				{
					pSoldier->aiPlanning().actionData() = ubOpponentDir;
					return(AI_ACTION_CHANGE_FACING);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// If sniper and nothing else to do then raise gun, and if that doesn't find somebody then goto yellow
	////////////////////////////////////////////////////////////////////////////
	if ( pSoldier->aiBehavior().orders() == SNIPER )
	{
		if ( pSoldier->aiPlanning().sniperPosture() != 0 )
		{
			pSoldier->aiPlanning().lowerSniperPosture();
			return(ArmedVehicleDecideActionYellow( pSoldier ));
		}
	}
	
	////////////////////////////////////////////////////////////////////////////
	// SWITCH TO GREEN: soldier does ordinary regular patrol, seeks friends
	////////////////////////////////////////////////////////////////////////////

	// if not in combat or under fire, and we COULD have moved, just chose not to	
	if ( (pSoldier->aiBehavior().alertStatus() != STATUS_BLACK) && !pSoldier->suppression().underFire() && ubCanMove && (!gfTurnBasedAI || pSoldier->actionPoints().current() >= pSoldier->actionPoints().initial()) && (TileIsOutOfBounds( ClosestReachableDisturbance( pSoldier, &fClimb ) )) )
	{
#ifdef DEBUGDECISIONS
		AINameMessage( pSoldier, "- chose to SKIP all RED actions, BYPASSES to GREEN!", 1000 );
#endif
		// Skip RED until new situation/next turn, 30% extra chance to do GREEN actions
		pSoldier->aiBehavior().bypassToGreen() = 30;
		return(ArmedVehicleDecideActionGreen( pSoldier ));
	}

	////////////////////////////////////////////////////////////////////////////
	// DO NOTHING: Not enough points left to move, so save them for next turn
	////////////////////////////////////////////////////////////////////////////
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionRed: do nothing at all..." ) );
#ifdef DEBUGDECISIONS
	AINameMessage( pSoldier, "- DOES NOTHING (RED)", 1000 );
#endif

	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);
}

INT8 ArmedVehicleDecideActionBlack( TacticalActor *pSoldier )
{
	INT32	iCoverPercentBetter, iOffense, iDefense, iChance;
	INT32	sClosestOpponent = NOWHERE, sBestCover = NOWHERE;//dnl ch58 160813
	INT32	sClosestDisturbance;
	INT16 ubMinAPCost;
	UINT8	ubCanMove;
	INT8		bInWater, bInDeepWater;
	INT8		bDirection;
	UINT8	ubBestAttackAction = AI_ACTION_NONE;
	INT8		bCanAttack;
	INT8		bWeaponIn;
#ifdef DEBUGDECISIONS
	STR16 tempstr;
#endif
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionBlack: soldier = %d, orders = %d, attitude = %d", pSoldier->identity().id(), pSoldier->aiBehavior().orders(), pSoldier->aiBehavior().attitude() ) );

	// Flugente: to prevent an accidental call
	if ( !ARMED_VEHICLE(pSoldier) )
		return DecideActionBlack( pSoldier );

	// sevenfm: stop flanking when we see enemy
	if ( AICheckIsFlanking( pSoldier ) )
	{
		pSoldier->aiPlanning().clearFlank();
	}

	// if we have absolutely no action points, we can't do a thing under BLACK!
	if ( !pSoldier->actionPoints().current() )
	{
		pSoldier->aiPlanning().actionData() = NOWHERE;
		return(AI_ACTION_NONE);
	}

	if ( gTacticalStatus.bBoxingState != NOT_BOXING )
	{
		return(AI_ACTION_NONE);
	}

	ATTACKTYPE BestShot, BestThrow, BestAttack;//dnl ch69 150913
	TacticalActor* bestShotOpponent = nullptr;
	BOOLEAN fClimb;
	INT16	ubBurstAPs;
	UINT8	ubOpponentDir;
	INT32	sCheckGridNo;

	BOOLEAN fAllowCoverCheck = FALSE;

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack" );
			
	// can this guy move to any of the neighbouring squares ? (sets TRUE/FALSE)
	ubCanMove = (pSoldier->actionPoints().current() >= MinPtsToMove( pSoldier ));
		
	// determine if we happen to be in water (in which case we're in BIG trouble!)
	bInWater = Water( pSoldier->position().gridNo(), pSoldier->position().level() );
	bInDeepWater = WaterTooDeepForAttacks( pSoldier->position().gridNo(), pSoldier->position().level() );
		
	// calculate our morale
	pSoldier->morale().aiMorale() = CalcMorale( pSoldier );
	
	////////////////////////////////////////////////////////////////////////////
	// STUCK IN WATER OR GAS, NO COVER, GO TO NEAREST SPOT OF UNGASSED LAND
	////////////////////////////////////////////////////////////////////////////

	// if soldier in water/gas has enough APs left to move at least 1 square
	if ( bInDeepWater && ubCanMove )
	{
		pSoldier->aiPlanning().actionData() = FindNearestUngassedLand( pSoldier );

		if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
		{
#ifdef DEBUGDECISIONS
			sprintf( tempstr, "%s - SEEKING NEAREST UNGASSED LAND at grid %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
			AIPopMessage( tempstr );
#endif

			return(AI_ACTION_LEAVE_WATER_GAS);
		}

		// couldn't find ANY land within 25 tiles(!), this should never happen...

		// look for best place to RUN AWAY to (farthest from the closest threat)
		pSoldier->aiPlanning().actionData() = FindSpotMaxDistFromOpponents( pSoldier );

		if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
		{
#ifdef DEBUGDECISIONS
			sprintf( tempstr, "%s - NO LAND NEAR, RUNNING AWAY to grid %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
			AIPopMessage( tempstr );
#endif

			return(AI_ACTION_RUN_AWAY);
		}
	}

	// offer surrender?
#ifndef JA2UB
	if ( !is_networked ) // No surrender in multiplayer
	{
		if ( pSoldier->roster().team() == ENEMY_TEAM && pSoldier->awareness().visibility() == TRUE && !(gTacticalStatus.fEnemyFlags & ENEMY_OFFERED_SURRENDER) && pSoldier->vitals().health() >= pSoldier->vitals().maximumHealth() / 2 && !ARMED_VEHICLE( pSoldier ) && !ENEMYROBOT( pSoldier ) )
		{
			if ( gTacticalStatus.Team[MILITIA_TEAM].bMenInSector == 0 && gTacticalStatus.Team[CREATURE_TEAM].bMenInSector == 0 && NumPCsInSector() < 4 && gTacticalStatus.Team[ENEMY_TEAM].bMenInSector >= NumPCsInSector() * 3 )
			{
				if ( gubQuest[QUEST_HELD_IN_ALMA] == QUESTNOTSTARTED || gubQuest[QUEST_HELD_IN_TIXA] == QUESTNOTSTARTED || gubQuest[QUEST_INTERROGATION] == QUESTNOTSTARTED )
				{
					return(AI_ACTION_OFFER_SURRENDER);
				}
			}
		}
	}
#endif

	////////////////////////////////////////////////////////////////////////////
	// SOLDIER CAN ATTACK IF NOT IN WATER/GAS AND NOT DOING SOMETHING TOO FUNKY
	////////////////////////////////////////////////////////////////////////////

	// NPCs in water/tear gas without masks are not permitted to shoot/stab/throw
	if ( (pSoldier->actionPoints().current() < 2) || bInDeepWater || pSoldier->aiBehavior().realtimeCombat() == RTP_COMBAT_REFRAIN )
	{
		bCanAttack = FALSE;
	}
	else
	{
		do
		{
			bCanAttack = CanNPCAttack( pSoldier );
			if ( bCanAttack != TRUE )
			{
				if ( bCanAttack == NOSHOOT_NOAMMO && ubCanMove && !pSoldier->aiBehavior().neutral() )
				{
					int handPOS;
					//CHRISL: We need to know which weapon has no ammo in case the soldier is holding a weapoin in SECONDHANDPOS
					if ( pSoldier->inventory()[SECONDHANDPOS].exists( ) == true && pSoldier->inventory()[SECONDHANDPOS][0]->data.gun.ubGunShotsLeft == 0 )
						handPOS = SECONDHANDPOS;
					else
						handPOS = HANDPOS;

					// try to find more ammo
					pSoldier->aiPlanning().action() = SearchForItems( pSoldier, SEARCH_AMMO, pSoldier->inventory()[handPOS].usItem );

					if ( pSoldier->aiPlanning().action() == AI_ACTION_NONE )
					{
						// the current weapon appears is useless right now!
						// (since we got a return code of noammo, we know the hand usItem
						// is our gun)
						pSoldier->inventory()[handPOS].fFlags |= OBJECT_AI_UNUSABLE;
						// move the gun into another pocket...
						if ( !AutoPlaceObject( pSoldier, &(pSoldier->inventory()[handPOS]), FALSE ) )
						{
							// If there's no room in his pockets for the useless gun, just throw it away
							return AI_ACTION_DROP_ITEM;
						}
					}
					else
					{
						return(pSoldier->aiPlanning().action());
					}
				}
				else
				{
					bCanAttack = FALSE;
				}
			}
		} while ( bCanAttack != TRUE && bCanAttack != FALSE );

#ifdef RETREAT_TESTING
		bCanAttack = FALSE;
#endif

		if ( !bCanAttack )
		{
			if ( pSoldier->morale().aiMorale() > MORALE_WORRIED )
			{
				pSoldier->morale().aiMorale() = MORALE_WORRIED;
			}
		}
	}

	// if we don't have a gun, look around for a weapon!
	if ( FindAIUsableObjClass( pSoldier, IC_GUN ) == ITEM_NOT_FOUND && ubCanMove && !pSoldier->aiBehavior().neutral() )
	{
		// look around for a gun...
		pSoldier->aiPlanning().action() = SearchForItems( pSoldier, SEARCH_WEAPONS, pSoldier->inventory()[HANDPOS].usItem );
		if ( pSoldier->aiPlanning().action() != AI_ACTION_NONE )
		{
			return(pSoldier->aiPlanning().action());
		}
	}
		
	BestShot.ubPossible = FALSE;	// by default, assume Shooting isn't possible
	BestThrow.ubPossible = FALSE;	// by default, assume Throwing isn't possible

	BestAttack.ubChanceToReallyHit = 0;
	
	// if we are able attack
	if ( bCanAttack )
	{
		pSoldier->attackSelection().shotLocation() = AIM_SHOT_RANDOM;

		//////////////////////////////////////////////////////////////////////////
		// FIRE A GUN AT AN OPPONENT
		//////////////////////////////////////////////////////////////////////////
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "FIRE A GUN AT AN OPPONENT" );

		bWeaponIn = FindAIUsableObjClass( pSoldier, IC_GUN );

		if ( bWeaponIn != NO_SLOT )
		{
			BestShot.bWeaponIn = bWeaponIn;
			// if it's in another pocket, swap it into his hand temporarily
			if ( bWeaponIn != HANDPOS )
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: swap gun into hand" );
				RearrangePocket( pSoldier, HANDPOS, bWeaponIn, TEMPORARILY );
			}

			// now it better be a gun, or the guy can't shoot (but has other attack(s))
			if ( Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass == IC_GUN && pSoldier->inventory()[HANDPOS][0]->data.gun.bGunStatus >= USABLE )
			{
				// get the minimum cost to attack the same target with this gun
				ubMinAPCost = MinAPsToAttack( pSoldier, pSoldier->targeting().lastGridNo(), ADDTURNCOST, 0 );

				// if we have enough action points to shoot with this gun
				if ( pSoldier->actionPoints().current() >= ubMinAPCost )
				{
					// look around for a worthy target (which sets BestShot.ubPossible)
					CalcBestShot(pSoldier, &BestShot);
					bestShotOpponent =
						GetJa2SoldierRepository().resolve(
							BestShot.ubOpponent.i);
					if (BestShot.ubPossible && !bestShotOpponent)
					{
						BestShot.ubPossible = FALSE;
					}

					if ( pSoldier->roster().team() == gbPlayerNum && pSoldier->aiBehavior().realtimeCombat() == RTP_COMBAT_CONSERVE )
					{
						if ( BestShot.ubChanceToReallyHit < 30 )
						{
							// skip firing, our chance isn't good enough
							BestShot.ubPossible = FALSE;
						}
					}

					if ( BestShot.ubFriendlyFireChance )//dnl ch61 180813
					{
						iChance = 0;
						if ( BestShot.ubFriendlyFireChance == 100 )
						{
							if ( pSoldier->aiBehavior().attitude() == AGGRESSIVE )
								iChance = 5;
						}
						else
						{
							switch ( pSoldier->aiBehavior().attitude() )
							{
							case DEFENSIVE:iChance = 15; break;
							case BRAVESOLO:iChance = 25; break;
							case BRAVEAID:iChance = 20; break;
							case CUNNINGSOLO:iChance = 35; break;
							case CUNNINGAID:iChance = 30; break;
							case AGGRESSIVE:iChance = 45; break;
							case ATTACKSLAYONLY:iChance = 40; break;
							default:iChance = 10; break;
							}
						}
						if ( !((INT32)Random( 100 ) < iChance) )
							BestShot.ubPossible = FALSE;
					}

					if ( BestShot.ubPossible )
					{
						// if the selected opponent is not a threat (unconscious & !serviced)
						// (usually, this means all the guys we see are unconscious, but, on
						//  rare occasions, we may not be able to shoot a healthy guy, too)
						if ( (bestShotOpponent->vitals().health() < OKLIFE) &&
							 !bestShotOpponent->service().active() )
						{
							// if our attitude is NOT aggressive
							if ( pSoldier->aiBehavior().attitude() != AGGRESSIVE || BestShot.ubChanceToReallyHit < 60 )
							{
								// get the location of the closest CONSCIOUS reachable opponent
								sClosestDisturbance = ClosestReachableDisturbance( pSoldier, &fClimb );

								// if we found one								
								if ( !TileIsOutOfBounds( sClosestDisturbance ) )
								{
									// then make decision as if at alert status RED, but make sure
									// we don't try to SEEK OPPONENT the unconscious guy!
									return DecideActionRed(pSoldier);
								}
								// else kill the guy, he could be the last opponent alive in this sector
							}
							// else aggressive guys will ALWAYS finish off unconscious opponents
						}

						// now we KNOW FOR SURE that we will do something (shoot, at least)
						NPCDoesAct( pSoldier );
						DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "NPC decided to shoot (or something)" );
					}
				}

				// if it was in his holster, swap it back into his holster for now
				if ( bWeaponIn != HANDPOS )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: swap gun into holster" );
					RearrangePocket( pSoldier, HANDPOS, bWeaponIn, TEMPORARILY );
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// THROW A TOSSABLE ITEM AT OPPONENT(S)
		// 	- HTH: THIS NOW INCLUDES FIRING THE GRENADE LAUNCHAR AND MORTAR!
		//////////////////////////////////////////////////////////////////////////

		// this looks for throwables, and sets BestThrow.ubPossible if it can be done
		//if ( !gfHiddenInterrupt )
		// {
		//if(!is_networked) //disable for mp ai
		//{
		CheckIfTossPossible( pSoldier, &BestThrow );

		if ( BestThrow.ubPossible )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "good throw possible" );
			if (ItemIsMortar(pSoldier->inventory()[BestThrow.bWeaponIn].usItem))
			{
				ubOpponentDir = (UINT8)GetDirectionFromGridNo( BestThrow.sTarget, pSoldier );

				// Get new gridno!
				sCheckGridNo = NewGridNo( pSoldier->position().gridNo(), (UINT16)DirectionInc( ubOpponentDir ) );

				if ( !OKFallDirection( pSoldier, sCheckGridNo, pSoldier->position().level(), ubOpponentDir, pSoldier->animationPlayback().state() ) )
				{
					// can't fire!
					BestThrow.ubPossible = FALSE;

					// try behind us, see if there's room to move back
					sCheckGridNo = NewGridNo( pSoldier->position().gridNo(), (UINT16)DirectionInc( gOppositeDirection[ubOpponentDir] ) );
					if ( OKFallDirection( pSoldier, sCheckGridNo, pSoldier->position().level(), gOppositeDirection[ubOpponentDir], pSoldier->animationPlayback().state() ) )
					{
						pSoldier->aiPlanning().actionData() = sCheckGridNo;

						return(AI_ACTION_GET_CLOSER);
					}
				}
			}

			if ( BestThrow.ubPossible )
			{
				// now we KNOW FOR SURE that we will do something (throw, at least)
				NPCDoesAct( pSoldier );
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// GO STAB AN OPPONENT WITH A KNIFE
		//////////////////////////////////////////////////////////////////////////

		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "GO STAB AN OPPONENT WITH A KNIFE" );
		// if soldier has a knife in his hand
		bWeaponIn = FindAIUsableObjClass( pSoldier, (IC_BLADE | IC_THROWING_KNIFE) );
		
		//////////////////////////////////////////////////////////////////////////
		// CHOOSE THE BEST TYPE OF ATTACK OUT OF THOSE FOUND TO BE POSSIBLE
		//////////////////////////////////////////////////////////////////////////
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "CHOOSE THE BEST TYPE OF ATTACK OUT OF THOSE FOUND TO BE POSSIBLE" );
		if ( BestShot.ubPossible )
		{
			BestAttack.iAttackValue = BestShot.iAttackValue;
			ubBestAttackAction = AI_ACTION_FIRE_GUN;
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "best action = fire gun" );
		}
		else
		{
			BestAttack.iAttackValue = 0;
		}
		
		if ( BestThrow.ubPossible && ((BestThrow.iAttackValue > BestAttack.iAttackValue) || (ubBestAttackAction == AI_ACTION_NONE)) && !((ARMED_VEHICLE( pSoldier ) || ENEMYROBOT( pSoldier )) && ubBestAttackAction == AI_ACTION_FIRE_GUN && BestShot.ubChanceToReallyHit > 20 && Random( 2 )) )//dnl ch64 290813 tank always had better chance to fire from cannon so this will increase probabilty to use machinegun too
		{
			ubBestAttackAction = AI_ACTION_TOSS_PROJECTILE;
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "best action = throw something" );
		}
		
		// copy the information on the best action selected into BestAttack struct
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "copy the information on the best action selected into BestAttack struct" );
		switch ( ubBestAttackAction )
		{
		case AI_ACTION_FIRE_GUN:
			memcpy( &BestAttack, &BestShot, sizeof(BestAttack) );
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: best attack = firing a gun" );
			break;

		case AI_ACTION_TOSS_PROJECTILE:
			memcpy( &BestAttack, &BestThrow, sizeof(BestAttack) );
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: best attack = tossing a grenade" );
			break;

		default:
			// set to empty
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: best attack = no good attack" );
			memset( &BestAttack, 0, sizeof(BestAttack) );
			break;
		}
	}

	// NB a desire of 4 or more is only achievable by brave/aggressive guys with high morale
	UINT16 usRange = BestAttack.bWeaponIn == NO_SLOT ? 0 : GetModifiedGunRange( pSoldier->inventory()[BestAttack.bWeaponIn].usItem );//dnl ch69 150913

	if ( (pSoldier->actionPoints().current() == pSoldier->actionPoints().initial()) &&
		 (ubBestAttackAction == AI_ACTION_FIRE_GUN) &&
		 (pSoldier->suppression().shock() == 0) &&
		 (pSoldier->vitals().health() >= pSoldier->vitals().maximumHealth() / 2) &&
		 (BestAttack.ubChanceToReallyHit < 30) &&
		 (PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) > usRange / CELL_X_SIZE) &&
		 (RangeChangeDesire( pSoldier ) >= 4) )
	{
		// okay, really got to wonder about this... could taking cover be an option?
		if ( ubCanMove && pSoldier->aiBehavior().orders() != STATIONARY && !gfHiddenInterrupt &&
			 !(pSoldier->status().flags() & SOLDIER_BOXER) )
		{
			// make militia a bit more cautious
			// 3 (UINT16) CONVERSIONS HERE TO AVOID ERRORS.  GOTTHARD 7/15/08
			if ( ((pSoldier->roster().team() == MILITIA_TEAM) && ((INT16)(PreRandom( 20 )) > BestAttack.ubChanceToReallyHit))
				 || ((pSoldier->roster().team() != MILITIA_TEAM) && ((INT16)(PreRandom( 40 )) > BestAttack.ubChanceToReallyHit)) )
			{
				//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_TESTVERSION, L"AI %d allowing cover check, chance to hit is only %d, at range %d", BestAttack.ubChanceToReallyHit, PythSpacesAway( pSoldier->sGridNo, BestAttack.sTarget ) );
				// maybe taking cover would be better!
				fAllowCoverCheck = TRUE;
				if ( (INT16)(PreRandom( 10 )) > BestAttack.ubChanceToReallyHit )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: can't hit so screw the attack" );
					// screw the attack!
					ubBestAttackAction = AI_ACTION_NONE;
				}
			}
		}
	}

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "LOOK FOR SOME KIND OF COVER BETTER THAN WHAT WE HAVE NOW" );
	////////////////////////////////////////////////////////////////////////////
	// LOOK FOR SOME KIND OF COVER BETTER THAN WHAT WE HAVE NOW
	////////////////////////////////////////////////////////////////////////////

	// if soldier has enough APs left to move at least 1 square's worth,
	// and either he can't attack any more, or his attack did wound someone
	iCoverPercentBetter = 0;

	if ( (ubCanMove && !SkipCoverCheck && !gfHiddenInterrupt &&
		((ubBestAttackAction == AI_ACTION_NONE) || pSoldier->combatResult().lastAttackHit()) &&
		(pSoldier->roster().team() != gbPlayerNum || pSoldier->aiBehavior().flags() & AI_RTP_OPTION_CAN_SEEK_COVER) &&
		!(pSoldier->status().flags() & SOLDIER_BOXER))
		|| fAllowCoverCheck )
	{
		sBestCover = FindBestNearbyCover( pSoldier, pSoldier->morale().aiMorale(), &iCoverPercentBetter );
	}


#ifdef RETREAT_TESTING
	sBestCover = NOWHERE;
#endif

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "DecideActionBlack: DECIDE BETWEEN ATTACKING AND DEFENDING (TAKING COVER)" );
	//////////////////////////////////////////////////////////////////////////
	// IF NECESSARY, DECIDE BETWEEN ATTACKING AND DEFENDING (TAKING COVER)
	//////////////////////////////////////////////////////////////////////////

	// if both are possible	
	if ( (ubBestAttackAction != AI_ACTION_NONE) && (!TileIsOutOfBounds( sBestCover )) )
	{
		// gotta compare their merits and select the more desirable option
		iOffense = BestAttack.ubChanceToReallyHit;
		iDefense = iCoverPercentBetter;

		// based on how we feel about the situation, decide whether to attack first
		switch ( pSoldier->morale().aiMorale() )
		{
		case MORALE_FEARLESS:			iOffense += iOffense / 2;	break;
		case MORALE_CONFIDENT:			iOffense += iOffense / 4;	break;
		case MORALE_NORMAL:				break;
		case MORALE_WORRIED:			iDefense += iDefense / 4;	break;
		case MORALE_HOPELESS:			iDefense += iDefense / 2;	break;
		}

		// smart guys more likely to try to stay alive, dolts more likely to shoot!
		if ( pSoldier->statistics().wisdom() >= 50 ) //Madd: reduced the wisdom required to want to live...
			iDefense += 10;
		else if ( pSoldier->statistics().wisdom() < 30 )
			iDefense -= 10;

		// some orders are more offensive, others more defensive
		if ( pSoldier->aiBehavior().orders() == SEEKENEMY )
			iOffense += 10;
		else if ( (pSoldier->aiBehavior().orders() == STATIONARY) || (pSoldier->aiBehavior().orders() == ONGUARD) || pSoldier->aiBehavior().orders() == SNIPER )
			iDefense += 10;

		switch ( pSoldier->aiBehavior().attitude() )
		{
		case DEFENSIVE:		iDefense += 30; break;
		case BRAVESOLO:		iDefense -= 0; break;
		case BRAVEAID:			iDefense -= 0; break;
		case CUNNINGSOLO:	iDefense += 20; break;
		case CUNNINGAID:		iDefense += 20; break;
		case AGGRESSIVE:		iOffense += 10; break;
		case ATTACKSLAYONLY:iOffense += 30; break;
		}

#ifdef DEBUGDECISIONS
		STR tempstr = "";
		sprintf( tempstr, "%s - CHOICE: iOffense = %d, iDefense = %d\n",
				 pSoldier->identity().name(), iOffense, iDefense );
		DebugAI( tempstr );
#endif

		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: if his defensive instincts win out, forget all about the attack" );
		// if his defensive instincts win out, forget all about the attack
		if ( iDefense > iOffense )
			ubBestAttackAction = AI_ACTION_NONE;
	}
	
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionBlack: is attack still desirable?  ubBestAttackAction = %d", ubBestAttackAction ) );

	// if attack is still desirable (meaning it's also preferred to taking cover)
	if ( ubBestAttackAction != AI_ACTION_NONE )
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: attack is still desirable (meaning it's also preferred to taking cover)" );
		// if we wanted to be REALLY mean, we could look at chance to hit and decide whether
		// to shoot at the head...
		
		// default settings
		//POSSIBLE STRUCTURE CHANGE PROBLEM, NOT CURRENTLY CHANGED. GOTTHARD 7/14/08		
		pSoldier->aiPlanning().aimTime() = BestAttack.ubAimTime;
		pSoldier->attackSelection().scopeMode() = BestAttack.bScopeMode;
		pSoldier->fireControl().burstCounter() = 0;

		// HEADROCK HAM 3.6: bAimTime represents how MANY aiming levels are used, not how much APs they cost necessarily.
		INT16 sActualAimAP = CalcAPCostForAiming( pSoldier, BestAttack.sTarget, (INT8)pSoldier->aiPlanning().aimTime() );

		if ( ubBestAttackAction == AI_ACTION_FIRE_GUN )
		{
			if ( gGameExternalOptions.fEnemyTanksCanMoveInTactical )
			{
				// first get the direction, as we will need to pass that in to ShootingStanceChange
				bDirection = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), BestAttack.sTarget);
				
				// Change facing
				if ( pSoldier->position().direction() != bDirection && TacticalActorMobility::isCurrentStanceValid(*pSoldier, bDirection) )
				{
					// we're not facing towards him, so turn first!
					pSoldier->aiPlanning().actionData() = bDirection;
					return(AI_ACTION_CHANGE_FACING);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// IF ENOUGH APs TO BURST, RANDOM CHANCE OF DOING SO
			//////////////////////////////////////////////////////////////////////////

			if ( IsGunBurstCapable( &pSoldier->inventory()[BestAttack.bWeaponIn], FALSE, pSoldier ) &&
				 bestShotOpponent &&
				 !(bestShotOpponent->vitals().health() < OKLIFE) && // don't burst at downed targets
				 pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft > 1 &&
				 (pSoldier->roster().team() != gbPlayerNum || pSoldier->aiBehavior().realtimeCombat() == RTP_COMBAT_AGGRESSIVE) )
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "DecideActionBlack: ENOUGH APs TO BURST, RANDOM CHANCE OF DOING SO" );

				ubBurstAPs = CalcAPsToBurst( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestAttack.bWeaponIn]), pSoldier );

				// HEADROCK HAM 3.6: Use Actual Aiming Time.
				if ( pSoldier->actionPoints().current() >= BestAttack.ubAPCost + sActualAimAP + ubBurstAPs )
				{
					iChance = 100;

					if ( (INT32)PreRandom( 100 ) < iChance )
					{
						BestAttack.ubAPCost += ubBurstAPs + sActualAimAP;//dnl ch58 130913
						// check for spread burst possibilities
						if ( pSoldier->aiBehavior().attitude() != ATTACKSLAYONLY )
						{
							CalcSpreadBurst( pSoldier, BestAttack.sTarget, BestAttack.bTargetLevel );
						}
						//dnl ch58 130913 return aiming for burst
						pSoldier->fireControl().selectBurst();
					}
				}
			}

			if ( IsGunAutofireCapable( &pSoldier->inventory()[BestAttack.bWeaponIn] ) &&
				 bestShotOpponent &&
				 !(bestShotOpponent->vitals().health() < OKLIFE) && // don't burst at downed targets
				 ((pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft > 1 &&
				 !pSoldier->fireControl().burstCounter()) || Weapon[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].NoSemiAuto) )
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "DecideActionBlack: ENOUGH APs TO AUTOFIRE, RANDOM CHANCE OF DOING SO" );
			L_NEWAIM:
				FLOAT dTotalRecoil = 0.0f;
				pSoldier->fireControl().autofireShots() = 0;
				if ( UsingNewCTHSystem( ) == true ){
					do
					{
						pSoldier->fireControl().autofireShots()++;
						dTotalRecoil += AICalcRecoilForShot( pSoldier, &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots() );
						ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestShot.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier );
					} while ( pSoldier->actionPoints().current() >= BestShot.ubAPCost + ubBurstAPs + sActualAimAP && pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft >= pSoldier->fireControl().autofireShots() && dTotalRecoil <= 10.0f );//dnl ch64 260813 pSoldier->attackSelection().hand() is wrong because decision is to use BestAttack.bWeaponIn
				}
				else {
					do
					{
						pSoldier->fireControl().autofireShots()++;
						ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestAttack.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier );
					} while ( pSoldier->actionPoints().current() >= BestAttack.ubAPCost + ubBurstAPs + sActualAimAP && pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft >= pSoldier->fireControl().autofireShots() && GetAutoPenalty( &pSoldier->inventory()[BestAttack.bWeaponIn], gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE )*pSoldier->fireControl().autofireShots() <= 80 );//dnl ch64 130913 pSoldier->attackSelection().hand() is wrong because decision is to use BestAttack.bWeaponIn, also missing sActualAimTime
				}

				pSoldier->fireControl().autofireShots()--;
				if ( !UsingNewCTHSystem( ) && pSoldier->fireControl().autofireShots() < 3 && pSoldier->aiPlanning().aimTime() > 0 && pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft >= 3 )//dnl ch69 130913 let try increase autofire rate for aim cost
				{
					pSoldier->aiPlanning().aimTime()--;
					sActualAimAP = CalcAPCostForAiming( pSoldier, BestAttack.sTarget, (INT8)pSoldier->aiPlanning().aimTime() );
					goto L_NEWAIM;
				}
				if ( pSoldier->fireControl().autofireShots() > 0 )
				{
					ubBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestAttack.bWeaponIn]), pSoldier->fireControl().autofireShots(), pSoldier );

					if ( pSoldier->actionPoints().current() >= BestAttack.ubAPCost + sActualAimAP + ubBurstAPs )
					{
						iChance = 100;

						if ( (INT32)PreRandom( 100 ) < iChance || Weapon[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].NoSemiAuto )
						{
							//dnl ch69 140913 return aiming for autofire with halfautofire fix
							pSoldier->fireControl().burstCounter() = 1;
							INT16 ubHalfBurstAPs = 256;
							if ( pSoldier->inventory()[BestAttack.bWeaponIn][0]->data.gun.ubGunShotsLeft < 4 )
								iChance = 0;
							else
							{
								ubHalfBurstAPs = CalcAPsToAutofire( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &pSoldier->inventory()[BestAttack.bWeaponIn], 4, pSoldier );
								if ( Weapon[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].NoSemiAuto )
									iChance = 35;
							}
							if ( (INT32)PreRandom( 100 ) < iChance && pSoldier->actionPoints().current() > (2 * BestAttack.ubAPCost + ubHalfBurstAPs + sActualAimAP) )
							{
								// Try short autofire to enhance chance of hitting
								pSoldier->fireControl().autofireShots() = 4;
								BestAttack.ubAPCost += ubHalfBurstAPs + sActualAimAP;
							}
							else
							{
								BestAttack.ubAPCost += ubBurstAPs + sActualAimAP;
							}
						}
						else
						{
							pSoldier->fireControl().selectSingleShot();
						}
					}
				}
			}

			if ( !pSoldier->fireControl().burstCounter() )
			{
				pSoldier->aiPlanning().aimTime() = BestAttack.ubAimTime;
				pSoldier->fireControl().selectSingleShot();
			}

			// IF WAY OUT OF EFFECTIVE RANGE TRY TO ADVANCE RESERVING ENOUGH AP FOR A SHOT IF NOT ACTED YET
			if ( (pSoldier->actionPoints().current() > BestAttack.ubAPCost) &&
				 bestShotOpponent &&
				 (pSoldier->suppression().shock() == 0) &&
				 (pSoldier->vitals().health() >= pSoldier->vitals().maximumHealth() / 2) &&
				 (BestAttack.ubChanceToReallyHit < 8) &&
				 (PythSpacesAway( pSoldier->position().gridNo(), BestAttack.sTarget ) > usRange / CELL_X_SIZE) &&
				 (RangeChangeDesire( pSoldier ) >= 3) ) // Cunning and above
			{
				sClosestOpponent = bestShotOpponent->position().gridNo();
				if ( !TileIsOutOfBounds( sClosestOpponent ) )
				{
					// temporarily make merc get closer reserving enough for expected cost of shot
					USHORT tgrd = pSoldier->aiPlanning().patrolGrid()[0];
					INT8 oldOrders = pSoldier->aiBehavior().orders();
					pSoldier->aiPlanning().patrolGrid()[0] = pSoldier->position().gridNo();
					pSoldier->aiBehavior().orders() = CLOSEPATROL;
					pSoldier->aiPlanning().actionData() = InternalGoAsFarAsPossibleTowards( pSoldier, sClosestOpponent, BestAttack.ubAPCost, AI_ACTION_GET_CLOSER, 0 );
					pSoldier->aiPlanning().patrolGrid()[0] = tgrd;
					pSoldier->aiBehavior().orders() = oldOrders;

					if ( !TileIsOutOfBounds( pSoldier->aiPlanning().actionData() ) )
					{
						pSoldier->aiPlanning().actionData() = pSoldier->position().gridNo();
						pSoldier->pathing().finalDestinationGrid() = pSoldier->aiPlanning().actionData();

						pSoldier->aiPlanning().nextAction() = AI_ACTION_FIRE_GUN;
						pSoldier->aiPlanning().nextActionData() = BestAttack.sTarget;
						pSoldier->aiPlanning().nextTargetLevel() = BestAttack.bTargetLevel;
						return(AI_ACTION_GET_CLOSER);
					}
				}
			}				
		}

		//////////////////////////////////////////////////////////////////////////
		// OTHERWISE, JUST GO AHEAD & ATTACK!
		//////////////////////////////////////////////////////////////////////////
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "OTHERWISE, JUST GO AHEAD & ATTACK!" );

		//dnl ch64 270813 must be as below RearrangePocket with FOREVER will screw already decided BURST or AUTOFIRE
		INT8 bDoBurst = pSoldier->fireControl().burstCounter();
		UINT8 bDoAutofire = pSoldier->fireControl().autofireShots();
		// swap weapon to hand if necessary
		if ( BestAttack.bWeaponIn != HANDPOS )
		{
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: swap weapon into hand" );
			RearrangePocket( pSoldier, HANDPOS, BestAttack.bWeaponIn, FOREVER );
		}
		if ( ubBestAttackAction == AI_ACTION_FIRE_GUN && bDoBurst == 1 )//dnl ch64 270813
		{
			pSoldier->fireControl().autofireShots() = bDoAutofire;
			pSoldier->fireControl().burstCounter() = bDoBurst;
			if ( bDoAutofire > 1 )
				pSoldier->attackSelection().weaponMode() = WM_AUTOFIRE;
			else
				pSoldier->attackSelection().weaponMode() = WM_BURST;
		}
				
		{
			pSoldier->aiPlanning().actionData() = BestAttack.sTarget;
			pSoldier->targeting().level() = BestAttack.bTargetLevel;

#ifdef DEBUGDECISIONS
			STR tempstr = "";
			sprintf( tempstr,
					 "%d(%s) %s %d(%s) at gridno %d (%d APs aim)\n",
					 pSoldier->identity().id(), pSoldier->identity().name(),
					 (ubBestAttackAction == AI_ACTION_FIRE_GUN) ? "SHOOTS" : ((ubBestAttackAction == AI_ACTION_TOSS_PROJECTILE) ? "TOSSES AT" : "STABS"),
					 BestAttack.ubOpponent, pSoldier->identity().name(),
					 BestAttack.sTarget, BestAttack.ubAimTime );
			DebugAI( tempstr );
#endif

			//DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("DecideActionBlack: Check for GL Bursts, is launcher capable? = %d, rtpcombat? = %d, bestattackaction = %d",IsGunBurstCapable( pSoldier, BestAttack.bWeaponIn, FALSE ),pSoldier->aiBehavior().realtimeCombat(),ubBestAttackAction ));
			//should be a bug
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "ArmedVehicleDecideActionBlack: Check for GL Bursts, is launcher capable? = %d, rtpcombat? = %d, bestattackaction = %d", IsGunBurstCapable( &pSoldier->inventory()[BestAttack.bWeaponIn], FALSE, pSoldier ), pSoldier->aiBehavior().realtimeCombat(), ubBestAttackAction ) );
			if ( ubBestAttackAction == AI_ACTION_TOSS_PROJECTILE && (Item[pSoldier->inventory()[BestAttack.bWeaponIn].usItem].usItemClass == IC_LAUNCHER && IsGunBurstCapable( &pSoldier->inventory()[BestAttack.bWeaponIn], FALSE, pSoldier )) &&
				 (pSoldier->roster().team() != gbPlayerNum || pSoldier->aiBehavior().realtimeCombat() == RTP_COMBAT_AGGRESSIVE) )
			{

				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: Doing burst calc" );
				ubBurstAPs = CalcAPsToBurst( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[BestAttack.bWeaponIn]), pSoldier );

				if ( (pSoldier->actionPoints().current() - BestAttack.ubAPCost) >= ubBurstAPs )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "ArmedVehicleDecideActionBlack: Doing GL burst" );
					BestAttack.ubAPCost = BestAttack.ubAPCost + CalcAPsToBurst( TacticalActorTurnBudget::calculateTurnGrant(*pSoldier), &(pSoldier->inventory()[HANDPOS]), pSoldier );
					// check for spread burst possibilities
					if ( pSoldier->aiBehavior().attitude() != ATTACKSLAYONLY )
					{
						CalcSpreadBurst( pSoldier, BestAttack.sTarget, BestAttack.bTargetLevel );
					}
					//dnl ch58 140913 After HAM 4 BURSTING is not in use any more, for burst bDoAutofire must be set to 0
					pSoldier->fireControl().selectBurst();
				}
			}

			if ( ubBestAttackAction == AI_ACTION_TOSS_PROJECTILE && IsGrenadeLauncherAttached( &pSoldier->inventory()[HANDPOS] ) )//dnl ch63 240813
				pSoldier->attackSelection().weaponMode() = WM_ATTACHED_GL;

			return(ubBestAttackAction);
		}
	}

	// end of tank AI
	if ( !gGameExternalOptions.fEnemyTanksCanMoveInTactical )
		return(AI_ACTION_NONE);
		
	////////////////////////////////////////////////////////////////////////////
	// IF SPOTTERS HAVE BEEN CALLED FOR, AND WE HAVE SOME NEW SIGHTINGS, RADIO!
	////////////////////////////////////////////////////////////////////////////

	// if we're a computer merc, and we have the action points remaining to RADIO
	// (we never want NPCs to choose to radio if they would have to wait a turn)
	// and we're not swimming in deep water, and somebody has called for spotters
	// and we see the location of at least 2 opponents
	if ( !(pSoldier->featureFlags().primaryFlags() & SOLDIER_RAISED_REDALERT) && (gTacticalStatus.ubSpottersCalledForBy != NOBODY) && (pSoldier->actionPoints().current() >= APBPConstants[AP_RADIO]) &&
		 (pSoldier->awareness().opponentCount() > 1) &&
		 (gTacticalStatus.Team[pSoldier->roster().team()].bMenInSector > 1) && !bInDeepWater )
	{
		// base chance depends on how much new info we have to radio to the others
		iChance = 25 * WhatIKnowThatPublicDont( pSoldier, TRUE );	// just count them

		// if I actually know something they don't
		if ( iChance )
		{
#ifdef DEBUGDECISIONS
			AINumMessage( "Chance to radio for SPOTTING = ", iChance );
#endif

			if ( (INT16)PreRandom( 100 ) < iChance )
			{
#ifdef DEBUGDECISIONS
				AINameMessage( pSoldier, "decides to radio a RED for SPOTTING!", 1000 );
#endif

				return(AI_ACTION_RED_ALERT);
			}
		}
	}
	
	////////////////////////////////////////////////////////////////////////////
	// TURN TO FACE CLOSEST KNOWN OPPONENT (IF NOT FACING THERE ALREADY)
	////////////////////////////////////////////////////////////////////////////

	if ( !gfTurnBasedAI || GetAPsToLook( pSoldier ) <= pSoldier->actionPoints().current() )
	{
		// hopeless guys shouldn't waste their time this way, UNLESS they CAN move
		// but chose not to to get this far (which probably means they're cornered)
		// ALSO, don't bother turning if we're already aiming a gun
		if ( !gfHiddenInterrupt && ((pSoldier->morale().aiMorale() > MORALE_HOPELESS) || ubCanMove) && !AimingGun( pSoldier ) )
		{
			// determine the location of the known closest opponent
			// (don't care if he's conscious, don't care if he's reachable at all)
			
			sClosestOpponent = ClosestSeenOpponent( pSoldier, NULL, NULL );

			// if we have a closest reachable opponent			
			if ( !TileIsOutOfBounds( sClosestOpponent ) )
			{
				if ( !TileIsOutOfBounds( pSoldier->targeting().lastGridNo() ) )//dnl ch58 150913
					sClosestOpponent = pSoldier->targeting().lastGridNo();
				bDirection = GetDirectionFromCenterCellXYGridNo(pSoldier->position().gridNo(), sClosestOpponent);

				// if we're not facing towards him
				if ( pSoldier->position().direction() != bDirection && TacticalActorMobility::isCurrentStanceValid(*pSoldier, bDirection) )
				{
					pSoldier->aiPlanning().actionData() = bDirection;

#ifdef DEBUGDECISIONS
					sprintf( tempstr, "%s - TURNS to face CLOSEST OPPONENT in direction %d", pSoldier->identity().name(), pSoldier->aiPlanning().actionData() );
					AIPopMessage( tempstr );
#endif

					return(AI_ACTION_CHANGE_FACING);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////
	// DO NOTHING: Not enough points left to move, so save them for next turn
	////////////////////////////////////////////////////////////////////////////

#ifdef DEBUGDECISIONS
	AINameMessage( pSoldier, "- DOES NOTHING (BLACK)", 1000 );
#endif

	// by default, if everything else fails, just stand in place and wait
	pSoldier->aiPlanning().actionData() = NOWHERE;
	return(AI_ACTION_NONE);
}


extern UINT32 guiTurnCnt;
extern UINT32 guiReinforceTurn;
extern UINT32 guiArrived;

void LogDecideInfo(TacticalActor *pSoldier)
{
	if (!gfLogsEnabled)
		return;

	DebugAI(AI_MSG_INFO, pSoldier, String("Turn num %d aware %d", guiTurnCnt, gTacticalStatus.Team[pSoldier->roster().team()].bAwareOfOpposition));
	DebugAI(AI_MSG_INFO, pSoldier, String("current team %d interrupt occurred %d", GetJa2TacticalCurrentTeam(), gTacticalStatus.fInterruptOccurred));
	DebugAI(AI_MSG_INFO, pSoldier, String("AP=%d/%d %s %s %s %s %s", pSoldier->actionPoints().current(), pSoldier->actionPoints().initial(), gStr8AlertStatus[pSoldier->aiBehavior().alertStatus()], gStr8Orders[pSoldier->aiBehavior().orders()], gStr8Attitude[pSoldier->aiBehavior().attitude()], gStr8Team[pSoldier->roster().team()], gStr8Class[pSoldier->roster().soldierClass()]));
	DebugAI(AI_MSG_INFO, pSoldier, String("Health %d/%d Breath %d/%d Shock %d Tolerance %d AI Morale %d Morale %d", pSoldier->vitals().health(), pSoldier->vitals().maximumHealth(), pSoldier->vitals().breath(), pSoldier->vitals().maximumBreath(), pSoldier->suppression().shock(), CalcSuppressionTolerance(pSoldier), pSoldier->morale().aiMorale(), pSoldier->morale().morale()));
	DebugAI(AI_MSG_INFO, pSoldier, String("Spot %d level %d opponents %d", pSoldier->position().gridNo(), pSoldier->position().level(), pSoldier->awareness().opponentCount()));
	DebugAI(AI_MSG_INFO, pSoldier, String("ubServiceCount %d ubServicePartner %d fDoingSurgery %d", pSoldier->service().providerCount(), pSoldier->service().partner().i, pSoldier->vitals().undergoingSurgery()));
	if (TacticalActorConditions::isCowering(*pSoldier))
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("Cowering"));
	}
	if (TacticalActorConditions::isGivingAid(*pSoldier))
	{
		DebugAI(AI_MSG_INFO, pSoldier, String("Giving aid"));
	}
	//CHAR8 str8[1024];

	// show watched locations
	INT8	bLoop;
	for (bLoop = 0; bLoop < NUM_WATCHED_LOCS; bLoop++)
	{
		if (!TileIsOutOfBounds(gsWatchedLoc[pSoldier->identity().id()][bLoop]))
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("Watched location %d level %d points %d", gsWatchedLoc[pSoldier->identity().id()][bLoop], gbWatchedLocLevel[pSoldier->identity().id()][bLoop], gubWatchedLocPoints[pSoldier->identity().id()][bLoop]));
		}
	}

	LogKnowledgeInfo(pSoldier);

	DebugAI(AI_MSG_INFO, pSoldier, String("What I know %d", WhatIKnowThatPublicDont(pSoldier, FALSE)));
	DebugAI(AI_MSG_INFO, pSoldier, String("Has Gun %d, Short range weapon %d, Gun Range %d, Gun Ammo %d, Gun Scoped %d ", AICheckHasGun(pSoldier), AICheckShortWeaponRange(pSoldier), AIGunRange(pSoldier), AIGunAmmo(pSoldier), AIGunScoped(pSoldier)));
	DebugAI(AI_MSG_INFO, pSoldier, String("RetreatCounter %d", TacticalActorAiBehavior::retreatCounter(*pSoldier)));
}

void LogKnowledgeInfo(TacticalActor *pSoldier)
{
	//CHAR8 str8[1024];
	//memset(str8, 0, 1024 * sizeof(char));

	// show public opponents
	for (UINT16 oppID = 0; oppID < MAX_NUM_SOLDIERS; oppID++)
	{
		TacticalActor* opponent =
			GetJa2SoldierRepository().resolve(oppID);
		if (gbPublicOpplist[pSoldier->roster().team()][oppID] != NOT_HEARD_OR_SEEN &&
			opponent &&
			!opponent->aiBehavior().neutral())
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("public opponent [%d] knowledge %s gridno %d level %d", oppID, gStr8Knowledge[gbPublicOpplist[pSoldier->roster().team()][oppID] - OLDEST_HEARD_VALUE], gsPublicLastKnownOppLoc[pSoldier->roster().team()][oppID], gbPublicLastKnownOppLevel[pSoldier->roster().team()][oppID]));
		}
	}
	// show personal opponents
	for (UINT16 oppID = 0; oppID < MAX_NUM_SOLDIERS; oppID++)
	{
		TacticalActor* opponent =
			GetJa2SoldierRepository().resolve(oppID);
		if (pSoldier->awareness().opponentKnowledge()[oppID] != NOT_HEARD_OR_SEEN &&
			opponent &&
			!opponent->aiBehavior().neutral())
		{
			DebugAI(AI_MSG_INFO, pSoldier, String("personal opponent [%d] knowledge %s gridno %d level %d", oppID, gStr8Knowledge[pSoldier->awareness().opponentKnowledge()[oppID] - OLDEST_HEARD_VALUE], gsLastKnownOppLoc[pSoldier->identity().id()][oppID], gbLastKnownOppLevel[pSoldier->identity().id()][oppID]));
		}
	}
}
