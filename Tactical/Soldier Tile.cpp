	#include "Render Fun.h"
	#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "TacticalActorMedicalServices.h"
	#include "DEBUG.H"
	#include "Overhead Types.h"

	#include "EditorMercs.h"
#include "Overhead.h"
	#include "Animation Control.h"
	#include "PATHAI.H"
	#include "worldman.h"
	#include "Isometric Utils.h"
	#include "renderworld.h"
	#include "Points.h"
	#include "lighting.h"
	#include "opplist.h"
	#include "ai.h"
		
	#ifdef NETWORKED
	#include "Networking.h"
	#include "NetworkEvent.h"
	#endif

	#include "Items.h"
	#include "soldier tile.h"
	#include "Soldier Add.h"
	#include "fov.h"
	#include "Font Control.h"
	#include "message.h"
	#include "Text.h"
	#include "NPC.h"
	#include "Soldier macros.h"

extern UINT8	gubWaitingForAllMercsToExitCode;

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;

#define NEXT_TILE_CHECK_DELAY		700

#ifdef JA2BETAVERSION

void OutputDebugInfoForTurnBasedNextTileWaiting( TacticalActor * pSoldier )
{
	if ( (IsJa2TacticalCombatActive()) && (pSoldier->pathing().pathSize() > 0) )
	{
		UINT32	uiLoop;
		INT32	usTemp = NOWHERE;
		INT32	usNewGridNo;

		usNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( (UINT8)pSoldier->pathing().path()[ pSoldier->pathing().pathIndex() ] ) );

		// provide more info!!
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("	Soldier path size %d, index %d", pSoldier->pathing().pathSize(), pSoldier->pathing().pathIndex() ) );
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("	Who is at blocked gridno: %d", WhoIsThere2( usNewGridNo, pSoldier->position().level() ) ) );

		for ( uiLoop = 0; uiLoop < pSoldier->pathing().pathSize(); uiLoop++ )
		{
			if ( uiLoop > pSoldier->pathing().pathIndex() )
			{
				usTemp = NewGridNo( usTemp, DirectionInc( (UINT8)pSoldier->pathing().path()[ uiLoop ] ) );
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("	Soldier path[%d]: %d == gridno %d", uiLoop, pSoldier->pathing().path()[uiLoop], usTemp ) );
			}
			else if ( uiLoop == pSoldier->pathing().pathIndex() )
			{
				usTemp = usNewGridNo;
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("	Soldier path[%d]: %d == gridno %d", uiLoop, pSoldier->pathing().path()[uiLoop], usTemp ) );
			}
			else
			{
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("	Soldier path[%d]: %d", uiLoop, pSoldier->pathing().path()[uiLoop] ) );
			}
		}

	}
}
#endif



void SetDelayedTileWaiting( TacticalActor *pSoldier, INT32 sCauseGridNo, UINT8 bValue )
{
	// Cancel AI Action
	// CancelAIAction( pSoldier, TRUE );

	pSoldier->movement().waitForGrid(sCauseGridNo, bValue);

	pSoldier->timing().start(SoldierTimingComponent::Timer::NextTile, NEXT_TILE_CHECK_DELAY);

	// ATE: Now update realtime movement speed....
	// check if guy exists here...
	SoldierID ubPerson = WhoIsThere2( sCauseGridNo, pSoldier->position().level() );

	// There may not be anybody there, but it's reserved by them!
	if ( ( gpWorldLevelData[ sCauseGridNo ].uiFlags & MAPELEMENT_MOVEMENT_RESERVED ) )
	{
		ubPerson = gpWorldLevelData[ sCauseGridNo ].ubReservedSoldierID;
	}

	if ( ubPerson != NOBODY )
	{
		TacticalActor* blockingPerson =
			GetJa2SoldierRepository().resolve(ubPerson.i);
		// if they are our own team members ( both )
		if ( blockingPerson != nullptr &&
			blockingPerson->roster().team() == gbPlayerNum &&
			pSoldier->roster().team() == gbPlayerNum )
		{
			// Here we have another guy.... save his stats so we can use them for
			// speed determinations....
			pSoldier->movement().overrideMoveSpeedWith(ubPerson);
		}
	}
}


void SetFinalTile( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fGivenUp )
{
	// OK, If we were waiting for stuff, do it here...

	// ATE: Disabled stuff below, made obsolete by timeout...
	//if ( pSoldier->movement().waitAction()	)
	//{
	//	pSoldier->movement().clearWaitAction();
	//	gbNumMercsUntilWaitingOver--;
	//}
	pSoldier->pathing().finalDestinationGrid() = pSoldier->position().gridNo();

	#ifdef JA2BETAVERSION
		if ( IsJa2TacticalCombatActive() )
		{
			OutputDebugInfoForTurnBasedNextTileWaiting( pSoldier );
		}
	#endif

	if ( pSoldier->roster().team() == gbPlayerNum	&& fGivenUp )
	{
		ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, TacticalStr[ NO_PATH_FOR_MERC ], pSoldier->identity().name() );
	}

	pSoldier->EVENT_StopMerc( pSoldier->position().gridNo(), pSoldier->position().direction() );

}


void MarkMovementReserved( TacticalActor *pSoldier, INT32 sGridNo )
{
	// Check if we have one reserrved already, and free it first!	
	if (!TileIsOutOfBounds(pSoldier->movement().reservedGrid()))
	{
		UnMarkMovementReserved( pSoldier );
	}

	// For single-tiled mercs, set this gridno
	gpWorldLevelData[ sGridNo ].uiFlags |= MAPELEMENT_MOVEMENT_RESERVED;

	// Save soldier's reserved ID #
	gpWorldLevelData[ sGridNo ].ubReservedSoldierID = pSoldier->identity().id();

	pSoldier->movement().reservedGrid() = sGridNo;
}

void UnMarkMovementReserved( TacticalActor *pSoldier )
{
	INT32 sNewGridNo;

	sNewGridNo = GETWORLDINDEXFROMWORLDCOORDS(pSoldier->position().worldY(), pSoldier->position().worldX() );

	// OK, if NOT in fence anim....
	if ( pSoldier->animationPlayback().state() == HOPFENCE && pSoldier->movement().reservedGrid() != sNewGridNo )
	{
		return;
	}
	
	if ( pSoldier->animationPlayback().state() == JUMPWINDOWS && pSoldier->movement().reservedGrid() != sNewGridNo )
	{
		return;
	}

	// For single-tiled mercs, unset this gridno
	// See if we have one reserved!	
	if (!TileIsOutOfBounds(pSoldier->movement().reservedGrid()))
	{
		gpWorldLevelData[ pSoldier->movement().reservedGrid() ].uiFlags &= (~MAPELEMENT_MOVEMENT_RESERVED);

		pSoldier->movement().reservedGrid() = NOWHERE;
	}
}

INT8 TileIsClear( TacticalActor *pSoldier, INT8 bDirection,  INT32 sGridNo, INT8 bLevel )
{
	INT32		sTempDestGridNo;
	INT32 sNewGridNo;
	BOOLEAN	fSwapInDoor = FALSE;
	
	if (TileIsOutOfBounds(sGridNo))
	{
		return( MOVE_TILE_CLEAR );
	}

	// anv: vehicles can ram people
	if ( !ARMED_VEHICLE( pSoldier ) && gGameExternalOptions.fAllowCarsDrivingOverPeople && pSoldier->status().flags() & SOLDIER_VEHICLE && pSoldier->featureFlags().secondaryFlags() & SOLDIER_RAM_THROUGH_OBSTACLES )
	{
		return( MOVE_TILE_CLEAR );
	}
	else if ( ARMED_VEHICLE( pSoldier ) && gGameExternalOptions.fAllowTanksDrivingOverPeople && pSoldier->status().flags() & SOLDIER_VEHICLE )
	{
		return( MOVE_TILE_CLEAR );
	}

	SoldierID ubPerson = WhoIsThere2( sGridNo, bLevel );


	if ( ubPerson != NOBODY )
	{
		TacticalActor* blockingPerson =
			GetJa2SoldierRepository().resolve(ubPerson.i);
		// If this us?
		if ( blockingPerson != nullptr && ubPerson != pSoldier->identity().id() )
		{
			// OK, set flag indicating we are blocked by a merc....
			if ( pSoldier->roster().team() != gbPlayerNum ) // CJC: shouldn't this be in all cases???
		//if ( 0 )
			{
				pSoldier->movement().blockInDirection(bDirection);

				// Are we only temporarily blocked?
				// Check if our final destination is = our gridno
				if ( ( blockingPerson->pathing().finalDestinationGrid() ==
					blockingPerson->position().gridNo() )	)
				{
					return( MOVE_TILE_STATIONARY_BLOCKED );
				}
				else
				{
					// OK, if buddy who is blocking us is trying to move too...
					// And we are in opposite directions...
					if ( blockingPerson->movement().blockedByAnotherMerc() &&
						blockingPerson->movement().blockedDirection() ==
							gOppositeDirection[ bDirection ] )
					{
						// OK, try and get a path around buddy....
						// We have to temporarily make buddy stopped...
						sTempDestGridNo =
							blockingPerson->pathing().finalDestinationGrid();
						blockingPerson->pathing().finalDestinationGrid() =
							blockingPerson->position().gridNo();

						if ( PlotPath( pSoldier, pSoldier->pathing().finalDestinationGrid(), NO_COPYROUTE, NO_PLOT, TEMPORARY, pSoldier->movement().mode(), NOT_STEALTH, FORWARD, pSoldier->actionPoints().current() ) )
						{
							pSoldier->pathing().stored() = FALSE;
							// OK, make guy go here...
							pSoldier->EVENT_GetNewSoldierPath( pSoldier->pathing().finalDestinationGrid(), pSoldier->movement().mode() );
							// Restore final dest....
							blockingPerson->pathing().finalDestinationGrid() =
								sTempDestGridNo;
							pSoldier->movement().clearBlock();

							// Is the next tile blocked too?
							sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( (UINT8)guiPathingData[ 0 ] ) );

							return( TileIsClear( pSoldier, (UINT8)guiPathingData[ 0 ], sNewGridNo, pSoldier->position().level() ) );
						}
						else
						{

								// Not for multi-tiled things...
								if ( !( pSoldier->status().flags() & SOLDIER_MULTITILE ) )
								{
									// Is the next movement cost for a door?
									if ( DoorTravelCost( pSoldier, sGridNo, gubWorldMovementCosts[ sGridNo ][ bDirection ][ pSoldier->position().level() ], (BOOLEAN)( pSoldier->roster().team() == gbPlayerNum ), NULL ) == TRAVELCOST_DOOR )
									{
										fSwapInDoor = TRUE;
									}

									// If we are to swap and we're near a door, open door first and then close it...?


									// Swap now!
									blockingPerson->movement().clearBlock();

									// Restore final dest....
									blockingPerson->pathing().finalDestinationGrid() =
										sTempDestGridNo;

									// Swap merc positions.....
									SwapMercPositions(
										pSoldier, blockingPerson);

									// With these two guys swapped, they should try and continue on their way....
									// Start them both again along their way...
									pSoldier->EVENT_GetNewSoldierPath( pSoldier->pathing().finalDestinationGrid(), pSoldier->movement().mode() );
									blockingPerson->EVENT_GetNewSoldierPath(
										blockingPerson->pathing().finalDestinationGrid(),
										blockingPerson->movement().mode());
								}
						}
					}
					return( MOVE_TILE_TEMP_BLOCKED );
				}
			}
			else
			{
				//return( MOVE_TILE_STATIONARY_BLOCKED );
				// ATE: OK, put some smartshere...
				// If we are waiting for more than a few times, change to stationary...
				if ( blockingPerson->movement().delayCounter() >= 105 )
				{
					// Set to special 'I want to walk through people' value
					pSoldier->movement().delayCounter() = 150;

					return( MOVE_TILE_STATIONARY_BLOCKED );
				}
				if ( blockingPerson->position().gridNo() ==
					blockingPerson->pathing().finalDestinationGrid() )
				{
					return( MOVE_TILE_STATIONARY_BLOCKED );
				}
				return( MOVE_TILE_TEMP_BLOCKED );
			}
		}
	}

	if ( ( gpWorldLevelData[ sGridNo ].uiFlags & MAPELEMENT_MOVEMENT_RESERVED ) )
	{
		if ( gpWorldLevelData[ sGridNo ].ubReservedSoldierID != pSoldier->identity().id() )
		{
			return( MOVE_TILE_TEMP_BLOCKED );
		}
	}

	// Are we clear of structs?
	if ( !NewOKDestination( pSoldier, sGridNo, FALSE, pSoldier->position().level() ) )
	{
			// ATE: Fence cost is an exclusiuon here....
			if ( gubWorldMovementCosts[ sGridNo ][ bDirection ][ pSoldier->position().level() ] != TRAVELCOST_FENCE )
			{
				// ATE: HIdden structs - we do something here... reveal it!
				if ( gubWorldMovementCosts[ sGridNo ][ bDirection ][ pSoldier->position().level() ] == TRAVELCOST_HIDDENOBSTACLE )
				{
					gpWorldLevelData[ sGridNo ].uiFlags|=MAPELEMENT_REVEALED;
					gpWorldLevelData[ sGridNo ].uiFlags|=MAPELEMENT_REDRAW;
					SetRenderFlags(RENDER_FLAG_MARKED);
					RecompileLocalMovementCosts( sGridNo );
				}

				// Unset flag for blocked by soldier...
				pSoldier->movement().clearBlock();
				return( MOVE_TILE_STATIONARY_BLOCKED );
			}
	}

	// Unset flag for blocked by soldier...
	pSoldier->movement().clearBlock();

	return( MOVE_TILE_CLEAR );

}



BOOLEAN HandleNextTile( TacticalActor *pSoldier, INT8 bDirection, INT32 sGridNo, INT32 sFinalDestTile )//dnl ch53 111009
{
	INT8 bBlocked;
	INT16	bOverTerrainType;

	// Check for blocking if in realtime
	///if ( ( gTacticalStatus.uiFlags & REALTIME ) || !( IsJa2TacticalCombatActive() ) )

	// ATE: If not on visible tile, return clear ( for path out of map )
	if ( !GridNoOnVisibleWorldTile( sGridNo ) )
	{
		return( TRUE );
	}

	// If animation state is crow, iall is clear
	if ( pSoldier->animationPlayback().state() == CROW_FLY )
	{
		return( TRUE );
	}

	{
		bBlocked = TileIsClear( pSoldier, bDirection, sGridNo, pSoldier->position().level() );

		// Check if we are blocked...
		if ( bBlocked != MOVE_TILE_CLEAR )
		{
			// Is the next gridno our destination?
			// OK: Let's check if we are NOT walking off screen			
			if ( sGridNo == sFinalDestTile && pSoldier->movement().waitAction() == 0 && (pSoldier->roster().team() == gbPlayerNum || TileIsOutOfBounds(pSoldier->movement().absoluteDestination())) )
			{
				// Yah, well too bad, stop here.
				SetFinalTile( pSoldier, pSoldier->position().gridNo(), FALSE );

				return( FALSE );
			}
			// CHECK IF they are stationary
			else if ( bBlocked == MOVE_TILE_STATIONARY_BLOCKED )
			{
				// Stationary,
				{
					INT32 sOldFinalDest;//dnl ch53 111009

					// Maintain sFinalDest....
					sOldFinalDest = pSoldier->pathing().finalDestinationGrid();
					#ifdef JA2BETAVERSION
						if ( IsJa2TacticalCombatActive() )
						{
							OutputDebugInfoForTurnBasedNextTileWaiting( pSoldier );
						}
					#endif
					pSoldier->EVENT_StopMerc( pSoldier->position().gridNo(), pSoldier->position().direction() );
					// Restore...
					pSoldier->pathing().finalDestinationGrid() = sOldFinalDest;

					SetDelayedTileWaiting( pSoldier, sGridNo, 1 );

					return( FALSE );
				}
			}
			else
			{
				{
					INT32 sOldFinalDest;//dnl ch53 111009

					// Maintain sFinalDest....
					sOldFinalDest = pSoldier->pathing().finalDestinationGrid();
					#ifdef JA2BETAVERSION
						if ( IsJa2TacticalCombatActive() )
						{
							OutputDebugInfoForTurnBasedNextTileWaiting( pSoldier );
						}
					#endif
					pSoldier->EVENT_StopMerc( pSoldier->position().gridNo(), pSoldier->position().direction() );
					// Restore...
					pSoldier->pathing().finalDestinationGrid() = sOldFinalDest;

					// Setting to two means: try and wait until this tile becomes free....
					SetDelayedTileWaiting( pSoldier, sGridNo, 100 );
				}

				return( FALSE );
			}
		}
		else
		{
			// Mark this tile as reserverd ( until we get there! )
			if ( !( IsJa2TacticalTurnBasedCombat() ) )
			{
				MarkMovementReserved( pSoldier, sGridNo );
			}

			bOverTerrainType = GetTerrainType( sGridNo );

			// WANNE.WATER: If our soldier is not on the ground level and the tile is a "water" tile, then simply set the tile to "FLAT_GROUND"
			// This should fix "problems" for special modified maps
			if ( TERRAIN_IS_WATER( bOverTerrainType) && pSoldier->position().level() > 0 )
				bOverTerrainType = FLAT_GROUND;

			// Check if we are going into water!
			if ( TERRAIN_IS_WATER( bOverTerrainType) )
			{
				// Check if we are of prone or crawl height and change stance accordingly....
				switch( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight )
				{
					case ANIM_PRONE:
					case ANIM_CROUCH:

						// Change height to stand
						pSoldier->animationIntent().continueAfterStance();
						SendChangeSoldierStanceEvent( pSoldier, ANIM_STAND );
						break;
				}

				// Check animation
				// Change to walking
				if ( pSoldier->animationPlayback().state() == RUNNING )
				{
					pSoldier->ChangeSoldierState( WALKING, 0 , FALSE );
				}
			}
		}
	}
	return( TRUE );
}



BOOLEAN HandleNextTileWaiting( TacticalActor *pSoldier )
{
	// Buddy is waiting to continue his path
	INT8			bBlocked, bPathBlocked;
	INT32		sCost;
	INT32		sNewGridNo, sCheckGridNo;
	UINT8		ubDirection, bCauseDirection;
	SoldierID	ubPerson;
	UINT8		fFlags = 0;


	if ( pSoldier->movement().delayed() )
	{
		if ( pSoldier->timing().elapsed(SoldierTimingComponent::Timer::NextTile) )
		{
			pSoldier->timing().start(SoldierTimingComponent::Timer::NextTile, NEXT_TILE_CHECK_DELAY);

			// ATE: Allow path to exit grid!
			if ( pSoldier->movement().waitAction() == 1 && gubWaitingForAllMercsToExitCode == WAIT_FOR_MERCS_TO_WALK_TO_GRIDNO )
			{
				gfPlotPathToExitGrid = TRUE;
			}

			// Get direction from gridno...
			bCauseDirection = (INT8)GetDirectionToGridNoFromGridNo( pSoldier->position().gridNo(), pSoldier->movement().delayedCauseGrid() );

			bBlocked = TileIsClear( pSoldier, bCauseDirection, pSoldier->movement().delayedCauseGrid(), pSoldier->position().level() );

			// If we are waiting for a temp blockage.... continue to wait
			if ( pSoldier->movement().delayCounter() >= 100 &&	bBlocked == MOVE_TILE_TEMP_BLOCKED )
			{
				// ATE: Increment 1
				pSoldier->movement().delayCounter()++;

				// Are we close enough to give up? ( and are a pc )
				if ( pSoldier->movement().delayCounter() > 120 )
				{
					// Quit...
					SetFinalTile( pSoldier, pSoldier->position().gridNo(), TRUE );
					pSoldier->movement().clearDelay();
				}
				gfPlotPathToExitGrid = FALSE;
				return( TRUE );
			}

			// Try new path if anything but temp blockage!
			if ( bBlocked != MOVE_TILE_TEMP_BLOCKED )
			{
				// Set to normal delay
				if ( pSoldier->movement().delayCounter() >= 100 && pSoldier->movement().delayCounter() != 150 )
				{
					pSoldier->movement().delayCounter() = 1;
				}

				// Default to pathing through people
				fFlags = PATH_THROUGH_PEOPLE;

				// Now, if we are in the state where we are desparently trying to get out...
				// Use other flag
				// CJC: path-through-people includes ignoring person at dest
				/*
				if ( pSoldier->movement().delayCounter() >= 150 )
				{
					fFlags = PATH_IGNORE_PERSON_AT_DEST;
				}
				*/

				// Check destination first!
				if ( pSoldier->movement().absoluteDestination() == pSoldier->pathing().finalDestinationGrid() )
				{
					// on last lap of scripted move, make sure we get to final dest
					sCheckGridNo = pSoldier->movement().absoluteDestination();
				}
				else if (!NewOKDestination( pSoldier, pSoldier->pathing().finalDestinationGrid(), TRUE, pSoldier->position().level() ))
				{
					if ( pSoldier->movement().delayCounter() >= 150 )
					{
						// OK, look around dest for the first one!
						sCheckGridNo = FindGridNoFromSweetSpot( pSoldier, pSoldier->pathing().finalDestinationGrid(), 6, &ubDirection );
						
						if (TileIsOutOfBounds(sCheckGridNo))
						{
							// If this is nowhere, try harder!
							sCheckGridNo = FindGridNoFromSweetSpot( pSoldier, pSoldier->pathing().finalDestinationGrid(), 16, &ubDirection );
						}
					}
					else
					{
						// OK, look around dest for the first one!
						sCheckGridNo = FindGridNoFromSweetSpotThroughPeople( pSoldier, pSoldier->pathing().finalDestinationGrid(), 6, &ubDirection );
						
						if (TileIsOutOfBounds(sCheckGridNo))
						{
							// If this is nowhere, try harder!
							sCheckGridNo = FindGridNoFromSweetSpotThroughPeople( pSoldier, pSoldier->pathing().finalDestinationGrid(), 16, &ubDirection );
						}
					}
				}
				else
				{
					sCheckGridNo = pSoldier->pathing().finalDestinationGrid();
				}

				sCost = FindBestPath( pSoldier, sCheckGridNo, pSoldier->position().level(), pSoldier->movement().mode(), NO_COPYROUTE, fFlags );

				// Can we get there
				if ( sCost > 0 )
				{
					// Is the next tile blocked too?
					sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( (UINT8)guiPathingData[ 0 ] ) );

					bPathBlocked = TileIsClear( pSoldier, (UINT8)guiPathingData[ 0 ], sNewGridNo, pSoldier->position().level() );

					if ( bPathBlocked == MOVE_TILE_STATIONARY_BLOCKED )
					{
						// Try to path around everyone except dest person

						sCost = FindBestPath( pSoldier, sCheckGridNo, pSoldier->position().level(), pSoldier->movement().mode(), NO_COPYROUTE, PATH_IGNORE_PERSON_AT_DEST );

						// Is the next tile in this new path blocked too?
						sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( (UINT8)guiPathingData[ 0 ] ) );

						bPathBlocked = TileIsClear( pSoldier, (UINT8)guiPathingData[ 0 ], sNewGridNo, pSoldier->position().level() );

						// now working with a path which does not go through people
						pSoldier->movement().delayedFlags() &= (~DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE);
					}
					else
					{
						// path through people worked fine
						if ( pSoldier->movement().delayCounter() < 150 )
						{
							pSoldier->movement().delayedFlags() |= DELAYED_MOVEMENT_FLAG_PATH_THROUGH_PEOPLE;
						}
					}

					// Are we clear?
					if ( bPathBlocked == MOVE_TILE_CLEAR )
					{
						//pSoldier->movement().delayCounter() = FALSE;
						// ATE: THis will get set in EENT_GetNewSoldierPath....
						pSoldier->aiPlanning().actionData() = sCheckGridNo;

						pSoldier->pathing().stored() = FALSE;

						pSoldier->EVENT_GetNewSoldierPath( sCheckGridNo, pSoldier->movement().mode() );
						gfPlotPathToExitGrid = FALSE;

						return( TRUE );
					}
				}

				pSoldier->movement().delayCounter()++;

				if ( pSoldier->movement().delayCounter() == 99 )
				{
					// Cap at 99
					pSoldier->movement().delayCounter() = 99;
				}

				// Do we want to force a swap?				
				if (pSoldier->movement().delayCounter() == 3 && (!TileIsOutOfBounds(pSoldier->movement().absoluteDestination()) || gTacticalStatus.fAutoBandageMode) )
				{
					// with person who is in the way?
					ubPerson = WhoIsThere2( pSoldier->movement().delayedCauseGrid(), pSoldier->position().level() );
					TacticalActor* blockingPerson =
						GetJa2SoldierRepository().resolve(ubPerson.i);

					// if either on a mission from god, or two AI guys not on stationary...
					if ( ubPerson != NOBODY &&
						blockingPerson != nullptr &&
						( pSoldier->dialogue().hasQuoteRecord() ||
							( pSoldier->roster().team() != gbPlayerNum &&
								pSoldier->aiBehavior().orders() != STATIONARY &&
								blockingPerson->roster().team() != gbPlayerNum &&
								blockingPerson->aiBehavior().orders() != STATIONARY ) ||
							( pSoldier->roster().team() == gbPlayerNum &&
								gTacticalStatus.fAutoBandageMode &&
								!( blockingPerson->roster().team() == CIV_TEAM &&
									blockingPerson->aiBehavior().orders() ==
										STATIONARY ) ) ) )
					{
						// Swap now!
						//ubPerson->movement().blockedByAnotherMerc() = FALSE;

						// Restore final dest....
						//ubPerson->pathing().finalDestinationGrid() = sTempDestGridNo;

						// Swap merc positions.....
						SwapMercPositions( pSoldier, blockingPerson );

						// With these two guys swapped, we should try to continue on our way....
						pSoldier->movement().clearDelay();

						// We must calculate the path here so that we can give it the "through people" parameter						
						if ( gTacticalStatus.fAutoBandageMode && TileIsOutOfBounds(pSoldier->movement().absoluteDestination()))
						{
							FindBestPath( pSoldier, pSoldier->pathing().finalDestinationGrid(), pSoldier->position().level(), pSoldier->movement().mode(), COPYROUTE, PATH_THROUGH_PEOPLE );
						}						
						else if (!TileIsOutOfBounds(pSoldier->movement().absoluteDestination()) && !FindBestPath( pSoldier, pSoldier->movement().absoluteDestination(), pSoldier->position().level(), pSoldier->movement().mode(), COPYROUTE, PATH_THROUGH_PEOPLE ) )
						{
							// check to see if we're there now!
							if ( pSoldier->position().gridNo() == pSoldier->movement().absoluteDestination() )
							{
								NPCReachedDestination( pSoldier, FALSE );
								pSoldier->aiPlanning().nextAction() = AI_ACTION_WAIT;
								pSoldier->aiPlanning().nextActionData() = 500;
								gfPlotPathToExitGrid = FALSE;
								return( TRUE );
							}
						}
						pSoldier->pathing().stored() = TRUE;

						pSoldier->EVENT_GetNewSoldierPath( pSoldier->movement().absoluteDestination(), pSoldier->movement().mode() );
						//EVENT_GetNewSoldierPath( ubPerson, ubPerson->pathing().finalDestinationGrid(), ubPerson->movement().mode() );
					}

				}

				// Are we close enough to give up? ( and are a pc )
				if ( pSoldier->movement().delayCounter() > 20 && pSoldier->movement().delayCounter() != 150)
				{
					if ( PythSpacesAway( pSoldier->position().gridNo(), pSoldier->pathing().finalDestinationGrid() ) < 5 && pSoldier->roster().team() == gbPlayerNum )
					{
						// Quit...
						SetFinalTile( pSoldier, pSoldier->position().gridNo(), FALSE );
						pSoldier->movement().clearDelay();
					}
				}

				// Are we close enough to give up? ( and are a pc )
				if ( pSoldier->movement().delayCounter() > 170 )
				{
					if ( PythSpacesAway( pSoldier->position().gridNo(), pSoldier->pathing().finalDestinationGrid() ) < 5 && pSoldier->roster().team() == gbPlayerNum )
					{
						// Quit...
						SetFinalTile( pSoldier, pSoldier->position().gridNo(), FALSE );
						pSoldier->movement().clearDelay();
					}
				}

			}
		}
	}

	gfPlotPathToExitGrid = FALSE;
	return( TRUE );
}


BOOLEAN TeleportSoldier( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fForce )
{
	INT16 sX, sY;

	// Check dest...
	if ( NewOKDestination( pSoldier, sGridNo, TRUE, 0 ) || fForce )
	{
		// TELEPORT TO THIS LOCATION!
		ConvertGridNoToCenterCellXY(sGridNo, &sX, &sY);
		pSoldier->EVENT_SetSoldierPosition( (FLOAT) sX, (FLOAT) sY );

		pSoldier->pathing().finalDestinationGrid() = sGridNo;

		// Make call to FOV to update items...
		RevealRoofsAndItems(pSoldier, TRUE, TRUE, pSoldier->position().level(), TRUE );

		// Handle sight!
		HandleSight(pSoldier,SIGHT_LOOK | SIGHT_RADIO);

		// Cancel services...
		TacticalActorMedicalServices::cancelProviding(
			*pSoldier);

		// Change light....
		if ( pSoldier->position().level() == 0 )
		{
			if(pSoldier->renderState().lightSprite()!=(-1))
				LightSpriteRoofStatus(pSoldier->renderState().lightSprite(), FALSE );
		}
		else
		{
			if(pSoldier->renderState().lightSprite()!=(-1))
				LightSpriteRoofStatus(pSoldier->renderState().lightSprite(), TRUE );
		}
		return( TRUE );
	}

	return( FALSE );
}

// Swaps 2 soldier positions...
BOOLEAN SwapMercPositions( TacticalActor *pSoldier1, TacticalActor *pSoldier2 )
{
	INT32 sGridNo1, sGridNo2;

	if ( pSoldier1 == NULL || pSoldier2 == NULL || pSoldier1 == pSoldier2 )
	{
		return FALSE;
	}

	// OK, save positions...
	sGridNo1 = pSoldier1->position().gridNo();
	sGridNo2 = pSoldier2->position().gridNo();

	// OK, remove each.....
	pSoldier1->RemoveSoldierFromGridNo( );
	pSoldier2->RemoveSoldierFromGridNo( );

	// OK, test OK destination for each.......
	if ( NewOKDestination( pSoldier1, sGridNo2, TRUE, 0 ) && NewOKDestination( pSoldier2, sGridNo1, TRUE, 0 ) )
	{
		// OK, call teleport function for each.......
		if ( TeleportSoldier( pSoldier1, sGridNo2, FALSE ) &&
			TeleportSoldier( pSoldier2, sGridNo1, FALSE ) )
		{
			return TRUE;
		}
	}

	// Place back...
	TeleportSoldier( pSoldier1, sGridNo1, TRUE );
	TeleportSoldier( pSoldier2, sGridNo2, TRUE );
	return FALSE;
}


BOOLEAN CanExchangePlaces( TacticalActor *pSoldier1, TacticalActor *pSoldier2, BOOLEAN fShow )
{
	// NB checks outside of this function
	if ( EnoughPoints( pSoldier1, APBPConstants[AP_EXCHANGE_PLACES], 0, fShow ) ){
	if ( EnoughPoints( pSoldier2, APBPConstants[AP_EXCHANGE_PLACES], 0, fShow ) ){
		if ( ( gAnimControl[ pSoldier2->animationPlayback().state() ].uiFlags & ANIM_MOVING ) )
			return( FALSE );

		if ( ( gAnimControl[ pSoldier1->animationPlayback().state() ].uiFlags & ANIM_MOVING ) && !(IsJa2TacticalCombatActive()) )
			return( FALSE );

		if ( pSoldier2->roster().side() == 0 )
		return( TRUE );

		// hehe - don't allow animals to exchange places
		if ( pSoldier2->status().flags() & ( SOLDIER_ANIMAL ) )
			return( FALSE );

		// must NOT be hostile, must NOT have stationary orders OR militia team, must be >= OKLIFE		
		if( pSoldier2->aiBehavior().neutral() && pSoldier2->vitals().health() >= OKLIFE &&
			pSoldier2->roster().civilianGroup() != HICKS_CIV_GROUP &&
			( ( pSoldier2->aiBehavior().orders() != STATIONARY || pSoldier2->roster().team() == MILITIA_TEAM ) ||
			( !TileIsOutOfBounds(pSoldier2->movement().absoluteDestination()) && pSoldier2->movement().absoluteDestination() != pSoldier2->position().gridNo() ) )
		)
			return( TRUE );

		if ( fShow ){
			if ( pSoldier2->identity().profile() == NO_PROFILE )
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_UI_FEEDBACK, TacticalStr[ REFUSE_EXCHANGE_PLACES ] );
			else
				ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_UI_FEEDBACK, gzLateLocalizedString[3], pSoldier2->identity().name() );
		}

		// ATE: OK, reduce this guy's next ai counter....
		pSoldier2->timing().aiDelay() = 100;
		return( FALSE );
	}else{
		return( FALSE );
	}
	}
	// if SirTech wouldn't setup your nested if statements so messily then perhaps wierd code like this
	//	could be avoided... cleaned up the ifs too (jonathanl)
	/*
	else
	{
		return( FALSE );
	}
	return( TRUE );
	*/
	return FALSE;
}
