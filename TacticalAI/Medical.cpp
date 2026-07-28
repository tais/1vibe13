	#include "types.h"

	#include "Soldier Functions.h"
	#include "ai.h"
	#include "AIInternals.h"
	#include "Animation Control.h"
	#include "PATHAI.H"
	#include "Overhead.h"
	#include "Items.h"
	#include "message.h"
	#include "Buildings.h"
	#include "worldman.h"
	#include "Assignments.h"
	// added by SANDRO
	#include "Soldier Profile.h"
	#include "GameSettings.h"
	#include "SoldierRepository.h"

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class SOLDIERTYPE;


extern BOOLEAN gfAutoBandageFailed;

//
// This file contains code devoted to the player AI-controlled medical system.	Maybe it
// can be used or adapted for the enemies too...
//

#define NOT_GOING_TO_DIE -1
#define NOT_GOING_TO_COLLAPSE -1

// can this grunt be bandaged by a teammate?
BOOLEAN CanCharacterBeAutoBandagedByTeammate( SOLDIERTYPE *pSoldier );

//c an this grunt help anyone else out?
BOOLEAN CanCharacterAutoBandageTeammate( SOLDIERTYPE *pSoldier );

BOOLEAN FindAutobandageClimbPoint( INT32 sDesiredGridNo, BOOLEAN fClimbUp )
{
	// checks for existance of location to climb up to building, not occupied by a medic
	BUILDING *	pBuilding;
	UINT8 ubNumClimbSpots;
	UINT8 ubLoop;
	SoldierID ubWhoIsThere;

	pBuilding = FindBuilding( sDesiredGridNo );
	if (!pBuilding)
	{
		return( FALSE );
	}

	ubNumClimbSpots = pBuilding->ubNumClimbSpots;

	for ( ubLoop = 0; ubLoop < ubNumClimbSpots; ubLoop++ )
	{
		ubWhoIsThere = WhoIsThere2( pBuilding->sUpClimbSpots[ ubLoop ], 1 );
		if ( ubWhoIsThere != NOBODY &&
			!CanCharacterAutoBandageTeammate(
				GetJa2SoldierRepository().resolve(
					ubWhoIsThere.i) ) )
		{
			continue;
		}
		ubWhoIsThere = WhoIsThere2( pBuilding->sDownClimbSpots[ ubLoop ], 0 );
		if ( ubWhoIsThere != NOBODY &&
			!CanCharacterAutoBandageTeammate(
				GetJa2SoldierRepository().resolve(
					ubWhoIsThere.i) ) )
		{
			continue;
		}
		return( TRUE );
	}

	return( FALSE );
}

BOOLEAN FullPatientCheck( SOLDIERTYPE * pPatient )
{
	SOLDIERTYPE * pSoldier;

	if ( CanCharacterAutoBandageTeammate( pPatient ) )
	{
		// can bandage self!
		return( TRUE );
	}

	if ( pPatient->position().level() != 0 )
	{	// look for a clear spot for jumping up

		// special "closest" search that ignores climb spots IF they are occupied by non-medics
		return( FindAutobandageClimbPoint( pPatient->position().gridNo(), TRUE ) );
	}
	else
	{
		// run though the list of chars on team
		SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
		for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
		{
			pSoldier =
				GetJa2SoldierRepository().resolve(cnt.i);
			// can this character help out?
			if ( CanCharacterAutoBandageTeammate( pSoldier ) == TRUE )
			{
				// can this guy path to the patient?
				if ( pSoldier->position().level() == 0 )
				{
					// do a regular path check
					if ( FindBestPath( pSoldier, pPatient->position().gridNo(), 0, WALKING, NO_COPYROUTE, PATH_THROUGH_PEOPLE ) )
					{
						return( TRUE );
					}
				}
				else
				{
					// if on different levels, assume okay
					return( TRUE );
				}
			}
		}
	}
	return( FALSE );
}

BOOLEAN CanAutoBandage( BOOLEAN fDoFullCheck )
{
	// returns false if we should stop being in auto-bandage mode
	UINT16 ubMedics = 0, ubPatients = 0;
	SOLDIERTYPE * pSoldier;
	static SoldierID 	ubIDForFullCheck = NOBODY;

	// run though the list of chars on team
	SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt)
	{
		// can this character help out?
		if( CanCharacterAutoBandageTeammate(
			GetJa2SoldierRepository().resolve(cnt.i) ) == TRUE )
		{
			// yep, up the number of medics in sector
			ubMedics++;
		}
	}

	if ( ubMedics == 0 )
	{
		// no one that can help
		return( FALSE );
	}

	cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(cnt.i);
		// can this character be helped out by a teammate?
		if ( CanCharacterBeAutoBandagedByTeammate( pSoldier ) == TRUE )
		{
			// yep, up the number of patients awaiting treatment in sector
			ubPatients++;
			if (fDoFullCheck)
			{
				if ( ubIDForFullCheck == NOBODY )
				{
					// do this guy NEXT time around
					ubIDForFullCheck = cnt;
				}
				else if ( cnt == ubIDForFullCheck )
				{
					// test this guy
					if ( FullPatientCheck( pSoldier ) == FALSE )
					{
						// shit!
						gfAutoBandageFailed = TRUE;
						return( FALSE );
					}
					// set ID for full check to NOBODY; will be set to someone later in loop, or to
					// the first guy on the next pass
					ubIDForFullCheck = NOBODY;
				}
			}
		}
		// is this guy REACHABLE??
	}

	if ( ubPatients == 0 )
	{
		// there is no one to help
		return( FALSE );
	}
	else
	{
		// got someone that can help and help wanted
		return( TRUE );
	}
}


BOOLEAN CanCharacterAutoBandageTeammate( SOLDIERTYPE *pSoldier )
// can this soldier autobandage others in sector
{
	// if the soldier isn't active or in sector, we have problems..leave
	if ( !(pSoldier->bActive) || !(pSoldier->bInSector) || ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || (pSoldier->assignment().current() == VEHICLE ) )
	{
		return( FALSE );
	}

	// they must have oklife or more, not be collapsed, have some level of medical competence, and have a med kit of some sort
	if ( (pSoldier->vitals().health() >= OKLIFE) && !(pSoldier->collapseState().tactical()) && (pSoldier->statistics().medical() > 0) && (FindObjClass( pSoldier, IC_MEDKIT ) != NO_SLOT) )
	{
		return( TRUE );
	}

	return( FALSE );
}


// can this soldier autobandage others in sector
BOOLEAN CanCharacterBeAutoBandagedByTeammate( SOLDIERTYPE *pSoldier )
{
	// if the soldier isn't active or in sector, we have problems..leave
	if ( !(pSoldier->bActive) || !(pSoldier->bInSector) || ( pSoldier->status().flags() & SOLDIER_VEHICLE ) || (pSoldier->assignment().current() == VEHICLE ) )
	{
		return( FALSE );
	}

	if ( (pSoldier->vitals().health() > 0) && (pSoldier->vitals().bleeding() > 0) )
	{
		// someone's bleeding and not being given first aid!
		return( TRUE );
	}

	return( FALSE );
}

INT8 FindBestPatient( SOLDIERTYPE * pSoldier, BOOLEAN * pfDoClimb )
{
	UINT8 cnt2;
	INT32 bBestPriority = 0, sBestAdjGridNo = NOWHERE;
	INT32 sPatientGridNo = NOWHERE, sBestPatientGridNo = NOWHERE;
	INT16 sShortestPath = 1000, sPathCost, sOtherMedicPathCost;
	SOLDIERTYPE * pPatient;
	SOLDIERTYPE * pBestPatient = NULL;
	SOLDIERTYPE * pOtherMedic;
	INT8 bPatientPriority;
	UINT8 ubDirection;
	INT32 sAdjustedGridNo, sAdjacentGridNo, sOtherAdjacentGridNo;
	INT32 sClimbGridNo, sBestClimbGridNo = NOWHERE, sShortestClimbPath = 1000;
	BOOLEAN fClimbingNecessary;

	gubGlobalPathFlags = PATH_THROUGH_PEOPLE;

	// search for someone who needs aid
	SoldierID cnt = gTacticalStatus.Team[ OUR_TEAM ].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; ++cnt )
	{
		pPatient =
			GetJa2SoldierRepository().resolve(cnt.i);
		if ( !pPatient || !(pPatient->bActive) ||
			!(pPatient->bInSector) )
		{
			continue; // NEXT!!!
		}

		if (pPatient->vitals().health() > 0 && pPatient->vitals().bleeding() && !pPatient->service().hasProviders())
		{
			if (pPatient->vitals().health() < OKLIFE)
			{
				bPatientPriority = 3;
			}
			else if (pPatient->vitals().health() < OKLIFE * 2)
			{
				bPatientPriority = 2;
			}
			else
			{
				bPatientPriority = 1;
			}

			if (bPatientPriority >= bBestPriority)
			{
				if ( !ClimbingNecessary( pSoldier, pPatient->position().gridNo(), pPatient->position().level() ) )
				{

					sPatientGridNo = pPatient->position().gridNo();
					sAdjacentGridNo = FindAdjacentGridEx( pSoldier, sPatientGridNo, &ubDirection, &sAdjustedGridNo, FALSE, FALSE );
					if ( sAdjacentGridNo == -1 && gAnimControl[ pPatient->animationPlayback().state() ].ubEndHeight == ANIM_PRONE )
					{
						// prone; could be the base tile is inaccessible but the rest isn't...
						for ( cnt2 = 0; cnt2 < NUM_WORLD_DIRECTIONS; cnt2++ )
						{
							sPatientGridNo = pPatient->position().gridNo() + DirectionInc( cnt2 );
							if ( WhoIsThere2( sPatientGridNo, pPatient->position().level() ) == pPatient->ubID )
							{
								// patient is also here, try this location
								sAdjacentGridNo = FindAdjacentGridEx( pSoldier, sPatientGridNo, &ubDirection, &sAdjustedGridNo, FALSE, FALSE );
								if ( sAdjacentGridNo != -1 )
								{
									break;
								}
							}
						}
					}

					if (sAdjacentGridNo != -1)
					{
						if (sAdjacentGridNo == pSoldier->position().gridNo())
						{
							sPathCost = 1;
						}
						else
						{
							sPathCost = PlotPath( pSoldier, sAdjacentGridNo, FALSE, FALSE, FALSE, RUNNING, FALSE, FALSE, 0);
						}

						if ( sPathCost != 0 )
						{
							// we can get there... can anyone else?

							if ( pPatient->service().hasAutoBandagingMedic() && pPatient->service().autoBandagingMedic() != pSoldier->ubID )
							{
								// only switch to this patient if our distance is closer than
								// the other medic's
				pOtherMedic = GetJa2SoldierRepository().resolve(
					pPatient->service().autoBandagingMedic().i);
				if ( !pOtherMedic )
					continue;
								sOtherAdjacentGridNo = FindAdjacentGridEx( pOtherMedic, sPatientGridNo, &ubDirection, &sAdjustedGridNo, FALSE, FALSE );
								if (sOtherAdjacentGridNo != -1)
								{

									if (sOtherAdjacentGridNo == pOtherMedic->position().gridNo())
									{
										sOtherMedicPathCost = 1;
									}
									else
									{
										sOtherMedicPathCost = PlotPath( pOtherMedic, sOtherAdjacentGridNo, FALSE, FALSE, FALSE, RUNNING, FALSE, FALSE, 0);
									}

									if (sPathCost >= sOtherMedicPathCost)
									{
										// this patient is best served by the merc moving to them now
										continue;
									}
								}
							}

							if (bPatientPriority == bBestPriority)
							{
								// compare path distances
								if ( sPathCost > sShortestPath )
								{
									continue;
								}
							}


							sShortestPath = sPathCost;
							pBestPatient = pPatient;
							sBestPatientGridNo = sPatientGridNo;
							bBestPriority = bPatientPriority;
							sBestAdjGridNo = sAdjacentGridNo;

						}
					}

				}
				else
				{
					sClimbGridNo = NOWHERE;
					// see if guy on another building etc and we need to climb somewhere
					sPathCost = EstimatePathCostToLocation( pSoldier, pPatient->position().gridNo(), pPatient->position().level(), FALSE, &fClimbingNecessary, &sClimbGridNo );
					// if we can get there
					if ( sPathCost != 0 && fClimbingNecessary && sPathCost < sShortestClimbPath )
					{
						sBestClimbGridNo = sClimbGridNo;
						sShortestClimbPath = sPathCost;
					}

				}

			}

		}
	}

	gubGlobalPathFlags = 0;

	if (pBestPatient)
	{
		if (pBestPatient->service().hasAutoBandagingMedic())
		{
			// cancel that medic
			SOLDIERTYPE* previousMedic =
				GetJa2SoldierRepository().resolve(
					pBestPatient->service().autoBandagingMedic().i);
			DebugAI(AI_MSG_INFO, previousMedic,
				String("CancelAIAction: medic: find patient"));
			if ( previousMedic )
				CancelAIAction( previousMedic, TRUE );
		}
		pBestPatient->service().assignAutoBandagingMedic( pSoldier->ubID );
		*pfDoClimb = FALSE;
		if ( CardinalSpacesAway( pSoldier->position().gridNo(), sBestPatientGridNo ) == 1 )
		{
			pSoldier->aiData.usActionData = sBestPatientGridNo;
			return( AI_ACTION_GIVE_AID );
		}
		else
		{
			pSoldier->aiData.usActionData = sBestAdjGridNo;
			return( AI_ACTION_GET_CLOSER );
		}
	}	
	else if (!TileIsOutOfBounds(sBestClimbGridNo))
	{
		*pfDoClimb = TRUE;
		pSoldier->aiData.usActionData = sBestClimbGridNo;
		return( AI_ACTION_MOVE_TO_CLIMB );
	}
	else
	{
		return( AI_ACTION_NONE );
	}
}

INT8 DecideAutoBandage( SOLDIERTYPE * pSoldier )
{
	INT8					bSlot;
	BOOLEAN				fDoClimb;


	if (pSoldier->statistics().medical() == 0 || pSoldier->service().hasPartner())
	{
		// don't/can't make decision
		return( AI_ACTION_NONE );
	}

	// Flugente: when bandaging, use first aid kits first (it's their only usage, medkits can be used for healing and surgery as well)
	bSlot = FindFirstAidKit( pSoldier );
	if ( bSlot == NO_SLOT )
	{
		bSlot = FindMedKit( pSoldier );
		if ( bSlot == NO_SLOT )
		{
			// no medical kit!
			return( AI_ACTION_NONE );
		}
	}

	if (pSoldier->vitals().bleeding())
	{
		// heal self first!
		pSoldier->aiData.usActionData = pSoldier->position().gridNo();
		if (bSlot != HANDPOS)
		{
			pSoldier->service().borrowInventorySlot( bSlot );

			SwapObjs( pSoldier, HANDPOS, bSlot, TRUE );
			/*
			memset( &TempObj, 0, sizeof( OBJECTTYPE ) );
			// move the med kit out to temp obj
			SwapObjs( &TempObj, &(pSoldier->inv[bSlot]) );
			// swap the med kit with whatever was in the hand
			SwapObjs( &TempObj, &(pSoldier->inv[HANDPOS]) );
			// replace whatever was in the hand somewhere in inventory
			AutoPlaceObject( pSoldier, &TempObj, FALSE );
			*/
		}
		return( AI_ACTION_GIVE_AID );
	}

//	pSoldier->aiData.usActionData = FindClosestPatient( pSoldier );
	pSoldier->aiData.bAction = FindBestPatient( pSoldier, &fDoClimb );
	if (pSoldier->aiData.bAction != AI_ACTION_NONE)
	{
		pSoldier->movement().mode() = RUNNING;
		if (bSlot != HANDPOS)
		{
			pSoldier->service().borrowInventorySlot( bSlot );

			SwapObjs( pSoldier, HANDPOS, bSlot, TRUE );
		}
		return( pSoldier->aiData.bAction );
	}

	// do nothing
	return( AI_ACTION_NONE );
}

// SANDRO - added a function
BOOLEAN DoctorIsPresent( SOLDIERTYPE * pPatient, BOOLEAN fOnDoctorAssignmentCheck )
{
	SOLDIERTYPE *	pMedic = NULL;
	INT8			bSlot;
	BOOLEAN			fDoctorHasBeenFound = FALSE;

	SoldierID cnt = gTacticalStatus.Team[ OUR_TEAM ].bFirstID;
	for ( ; cnt <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; ++cnt )
	{
		pMedic = GetJa2SoldierRepository().resolve(cnt.i);
		if ( !pMedic || !(pMedic->bActive) ||
			!(pMedic->bInSector) ||
			( pMedic->status().flags() & SOLDIER_VEHICLE ) ||
			(pMedic->assignment().current() == VEHICLE ) )
		{
			// is nowhere around!
			continue; // NEXT!!!
		}

		if ( pPatient->ubID == pMedic->ubID )
		{
			// cannot make surgery on self!
			continue; // NEXT!!!		
		}
		if ( fOnDoctorAssignmentCheck && !IS_DOCTOR(pMedic->assignment().current()) )
		{
			// not on the right assignment!
			continue; // NEXT!!!
		}

		bSlot = FindMedKit( pMedic );
		if (bSlot == NO_SLOT)
		{
			// no medical kit!
			continue; // NEXT!!!
		}

		if (pMedic->vitals().health() >= OKLIFE && !(pMedic->collapseState().tactical()) && pMedic->statistics().medical() > 0 && (NUM_SKILL_TRAITS( pMedic, DOCTOR_NT ) >= gSkillTraitValues.ubDONumberTraitsNeededForSurgery))
		{
			fDoctorHasBeenFound = TRUE;
		}
	}

	return( fDoctorHasBeenFound );
}
