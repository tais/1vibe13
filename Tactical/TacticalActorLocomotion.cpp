#include "TacticalActorLocomotion.h"

#include "TacticalActorDamageResolution.h"
#include "TacticalActorOrientation.h"
#include "TacticalActorWorldPlacement.h"
#include "Soldier Functions.h"
#include "TacticalActorAnimationFootprint.h"
#include "TacticalActorAnimationFrames.h"
#include "TacticalActorConsumables.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorRadio.h"
#include "TacticalActorRobotics.h"
#include "TacticalActorSkills.h"
#include "TacticalActorSpotting.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalActorTurnMaintenance.h"
#include "TacticalActorTurncoats.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorRecovery.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorDisease.h"
#include "TacticalActorDragging.h"
#include "TacticalActorAiBehavior.h"
#include "TacticalActorDamageQueue.h"
#include "TacticalActorDamageFeedback.h"
#include "TacticalActorLongActions.h"
#include "TacticalActorLighting.h"
#include "TacticalActorMedicalSession.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalActorProfileClassification.h"
#include "TacticalActorRangedActions.h"
#include "TacticalActorRouteExecution.h"
#include "TacticalActorWeaponHandling.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "builddefines.h"
#include <wchar.h>
#include <stdio.h>
#include <string.h>
#include "WCheck.h"
#include "stdlib.h"
#include "DEBUG.H"
#include "MemMan.h"
#include "Overhead Types.h"
#include "Animation Cache.h"
#include "Animation Data.h"
#include "Animation Control.h"
#define _USE_MATH_DEFINES // for C
#include <math.h>
#include "PATHAI.H"
#include "random.h"
#include "worldman.h"
#include "Isometric Utils.h"
#include "renderworld.h"
#include "render_palette_registry.h"
#include "video.h"
#include "Points.h"
#include "Sound Control.h"
#include "Weapons.h"
#include "shading.h"
#include "Handle UI.h"
#include "Soldier Ani.h"
#include "Event Pump.h"
#include "opplist.h"
#include "ai.h"
#include "Interface.h"
#include "lighting.h"
#include "faces.h"
#include "Soldier Profile.h"
#include "Campaign.h"
#include "Soldier macros.h"
#include "english.h"
#include "Squads.h"
#ifdef NETWORKED
#include "Networking.h"
#include "NetworkEvent.h"
#endif
#include "Structure Wrap.h"
#include "Items.h"
#include "soundman.h"
#include "Utilities.h"
#include "strategic.h"
#include "soldier tile.h"
#include "Smell.h"
#include "Keys.h"
#include "Dialogue Control.h"
#include "rt time defines.h"
#include "Quests.h"
#include "message.h"
#include "NPC.h"
#include "SkillCheck.h"
#include "Handle Doors.h"
#include "interface Dialogue.h"
#include "SmokeEffects.h"
#include	"GameSettings.h"
#include "Tile Animation.h"
#include "ShopKeeper Interface.h"
#include "Vehicles.h"
#include "Rotting Corpses.h"
#include "Interface Control.h"
#include "strategicmap.h"
#include "Morale.h"
#include "Drugs And Alcohol.h"
#include "Boxing.h"
#include "overhead map.h"
#include "Map Information.h"
#include "environment.h"
#include "Game Clock.h"
#include "Explosion Control.h"
#include "Buildings.h"
#include "Text.h"
#include "Strategic Merc Handler.h"
#include "Campaign Types.h"
#include "Strategic Status.h"
#include "Civ Quotes.h"
#include "Debug Control.h"
#include "LOS.h" // added by SANDRO
#include "CampaignStats.h"		// added by Flugente
#include "Interface Panels.h"
#include "Queen Command.h"		// added by Flugente
#include "Town Militia.h"		// added by Flugente
#include "Auto Bandage.h"		// added by Flugente
#include "Facilities.h"			// added by Flugente
#include "Cheats.h"				// added by Flugente
#include "MilitiaIndividual.h"	// added by Flugente
#include "Arms Dealer Init.h"	// added by Flugente for armsDealerInfo[]
#include "LuaInitNPCs.h"		// added by Flugente
#include "qarray.h"				// added by Flugente
#include "GameInitOptionsScreen.h"
#include "fresh_header.h"
#include "IMP Skill Trait.h"	// added by Flugente
#include "Food.h"				// added by Flugente
#include "Tactical Save.h"		// added by Flugente for AddItemsToUnLoadedSector()
#include "LightEffects.h"		// added by Flugente for CreatePersonalLight()
#include "DynamicDialogue.h"	// added by Flugente for HandleDynamicOpinions()
#include "Strategic Town Loyalty.h"		// added by Flugente for gTownLoyalty
#include "Rebel Command.h"
#include "Simulation Command Legacy.h"
#include "Simulation Commands.h"
#include "Strategic Movement.h"
#include "StrategicSquadHost.h"
#include "TacticalEntityHost.h"
#include "VehiclePassengerHost.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>


bool TacticalActorLocomotion::checkRoofHit(TacticalActor& subject)
{
	if (!IsJa2TacticalWorldLoaded() ||
		!subject.roster().active() ||
		!subject.roster().inSector() ||
		TileIsOutOfBounds(subject.position().gridNo()) ||
		subject.position().level() > SECOND_LEVEL ||
		subject.position().direction() >= NUM_WORLD_DIRECTIONS ||
		subject.animationPlayback().state() >= NUMANIMATIONSTATES)
	{
		return false;
	}

	// Check if we are near a lower level
	INT8							bNewDirection;
	BOOLEAN						fReturnVal = FALSE;
	INT32							sNewGridNo;
	// Default to true
	BOOLEAN						fDoForwards = TRUE;

	if ( subject.vitals().health() >= OKLIFE )
	{
		return(FALSE);
	}

	if ( FindLowerLevel( &subject, subject.position().gridNo(), subject.position().direction(), &bNewDirection ) && (subject.position().level() > 0) )
	{
		// ONly if standing!
		if ( gAnimControl[subject.animationPlayback().state()].ubHeight == ANIM_STAND )
		{
			// We are near a lower level.
			// Use opposite direction
			bNewDirection = gOppositeDirection[bNewDirection];

			// Alrighty, let's not blindly change here, look at whether the dest gridno is good!
			sNewGridNo = NewGridNo( subject.position().gridNo(), DirectionInc( gOppositeDirection[bNewDirection] ) );
			if ( !NewOKDestination( &subject, sNewGridNo, TRUE, 0 ) )
			{
				return(FALSE);
			}
			sNewGridNo = NewGridNo( sNewGridNo, DirectionInc( gOppositeDirection[bNewDirection] ) );
			if ( !NewOKDestination( &subject, sNewGridNo, TRUE, 0 ) )
			{
				return(FALSE);
			}

			// Are wee near enough to fall forwards....
			if ( subject.position().direction() == gOneCDirection[bNewDirection] ||
				 subject.position().direction() == gTwoCDirection[bNewDirection] ||
				 subject.position().direction() == bNewDirection ||
				 subject.position().direction() == gOneCCDirection[bNewDirection] ||
				 subject.position().direction() == gTwoCCDirection[bNewDirection] )
			{
				// Do backwards...
				fDoForwards = FALSE;
			}

			// If we are facing the opposite direction, fall backwards
			// ATE: Make &subject more usefull...
			if ( fDoForwards )
			{
				subject.position().temporaryGrid() = NewGridNo( subject.position().gridNo(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				subject.position().temporaryGrid() = NewGridNo( subject.position().temporaryGrid(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				(void)TacticalActorOrientation::setDesiredDirection(subject, gOppositeDirection[bNewDirection] );
				subject.animationActivity().turningUntilDone() = TRUE;
				subject.animationIntent().pendingAnimation() = FALLFORWARD_ROOF;
				//TacticalActorAnimationTransitions::initializeAnimation(subject,  FALLFORWARD_ROOF, 0 , FALSE );

				// Deduct hitpoints/breath for falling!
				TacticalActorDamageResolution::takeDamage(subject,  ANIM_CROUCH, 100, 5000, TAKE_DAMAGE_FALLROOF, NOBODY, NOWHERE, 0, TRUE );

				fReturnVal = TRUE;
			}
			else
			{
				subject.position().temporaryGrid() = NewGridNo( subject.position().gridNo(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				subject.position().temporaryGrid() = NewGridNo( subject.position().temporaryGrid(), (INT16)(-1 * DirectionInc( bNewDirection )) );
				(void)TacticalActorOrientation::setDesiredDirection(subject, bNewDirection );
				subject.animationActivity().turningUntilDone() = TRUE;
				subject.animationIntent().pendingAnimation() = FALLOFF;

				// Deduct hitpoints/breath for falling!
				TacticalActorDamageResolution::takeDamage(subject,  ANIM_CROUCH, 100, 5000, TAKE_DAMAGE_FALLROOF, NOBODY, NOWHERE, 0, TRUE );

				fReturnVal = TRUE;
			}

			// Flugente: some body types cannot perform these animations, so skip it.
			if ( IsAnimationValidForBodyType( &subject, subject.animationIntent().pendingAnimation() ) == FALSE )
				fReturnVal = FALSE;
		}
	}

	return(fReturnVal);
}

INT16 gsDragSoundNum = -1;

void TacticalActorLocomotion::move(TacticalActor& subject, FLOAT dMovementChange, FLOAT dAngle, bool fCheckRange)
{
	if (!IsJa2TacticalWorldLoaded() ||
		!subject.roster().active() ||
		!subject.roster().inSector() ||
		TileIsOutOfBounds(subject.position().gridNo()) ||
		subject.position().level() > SECOND_LEVEL ||
		subject.position().direction() >= NUM_WORLD_DIRECTIONS ||
		subject.movement().animationDirection() >= NUM_WORLD_DIRECTIONS ||
		subject.animationPlayback().state() >= NUMANIMATIONSTATES ||
		!std::isfinite(dMovementChange) || !std::isfinite(dAngle))
	{
		return;
	}

	FLOAT					dDeltaPos;
	FLOAT					dXPos, dYPos;
	BOOLEAN					fStop = FALSE;

	//dDegAngle = (INT16)( dAngle * 180 / PI );
	//sprintf( gDebugStr, "Move Angle: %d", (int)dDegAngle );

	// Find delta Movement for X pos
	dDeltaPos = (FLOAT)(dMovementChange * sin( dAngle ));

	// Find new position
	dXPos = subject.position().worldX() + dDeltaPos;

	if ( fCheckRange )
	{
		fStop = FALSE;

		switch ( subject.movement().animationDirection() )
		{
		case NORTHEAST:
		case EAST:
		case SOUTHEAST:

			if ( dXPos >= subject.pathing().destinationX() )
			{
				fStop = TRUE;
			}
			break;

		case NORTHWEST:
		case WEST:
		case SOUTHWEST:

			if ( dXPos <= subject.pathing().destinationX() )
			{
				fStop = TRUE;
			}
			break;

		case NORTH:
		case SOUTH:

			fStop = TRUE;
			break;

		}

		if ( fStop )
		{
			//dXPos = subject.pathing().destinationX();
			subject.movement().markPastXDestination();

			if ( subject.position().gridNo() == subject.pathing().finalDestinationGrid() )
			{
				dXPos = subject.pathing().destinationX();
			}
		}
	}

	// Find delta Movement for Y pos
	dDeltaPos = (FLOAT)(dMovementChange * cos( dAngle ));

	// Find new pos
	dYPos = subject.position().worldY() + dDeltaPos;

	if ( fCheckRange )
	{
		fStop = FALSE;

		switch ( subject.movement().animationDirection() )
		{
		case NORTH:
		case NORTHEAST:
		case NORTHWEST:

			if ( dYPos <= subject.pathing().destinationY() )
			{
				fStop = TRUE;
			}
			break;

		case SOUTH:
		case SOUTHWEST:
		case SOUTHEAST:

			if ( dYPos >= subject.pathing().destinationY() )
			{
				fStop = TRUE;
			}
			break;

		case EAST:
		case WEST:

			fStop = TRUE;
			break;

		}

		if ( fStop )
		{
			//dYPos = subject.pathing().destinationY();
			subject.movement().markPastYDestination();

			if ( subject.position().gridNo() == subject.pathing().finalDestinationGrid() )
			{
				dYPos = subject.pathing().destinationY();
			}
		}
	}

	// Flugente: as we move a tile, we would now be too far away to drag someone.
	// So remember whether we were dragging (we have to set our position now, otherwise the person we drag woul soon occupy our gridno).
	BOOLEAN currentlydragging = TacticalActorDragging::isDragging(subject, true);
	INT32 sOldGridNo = subject.position().gridNo();

	// OK, set new position
	(void)TacticalActorWorldPlacement::setPosition(subject, dXPos, dYPos, FALSE, FALSE, FALSE );

	TacticalActorDamageQueue::resolve(subject);

	// Flugente: drag people
	if ( currentlydragging )
	{
		bool dragaborted = false;

		if ( subject.interaction().draggingPerson() )
		{
			TacticalActor* pSoldier =
				GetJa2SoldierRepository().resolve(
					subject.interaction().draggedPerson() );

			if ( pSoldier )
			{
				// while it would be neat to take the opposite direction (which would make it look like we drag the other person by the legs),
				// &subject causes problems, as a prone person needs additional space for the legs. So just take the same direction
				pSoldier->position().direction() = subject.position().direction();

				FLOAT dx = 0;
				FLOAT dy = 0;

				INT32 gridnotouse = pSoldier->position().gridNo();
				if ( sOldGridNo != subject.position().gridNo() )
				{
					gridnotouse = sOldGridNo;
				}
				else
				{
					INT16 this_base_x = 0;
					INT16 this_base_y = 0;
					ConvertGridNoToCenterCellXY( subject.position().gridNo(), &this_base_x, &this_base_y );

					dx = subject.position().worldX() - this_base_x;
					dy = subject.position().worldY() - this_base_y;
				}

				INT16 base_x = 0;
				INT16 base_y = 0;
				ConvertGridNoToCenterCellXY( gridnotouse, &base_x, &base_y );

				(void)TacticalActorWorldPlacement::setPosition(*pSoldier, base_x + dx, base_y + dy, FALSE, FALSE, FALSE );
			}
			else
			{
				dragaborted = true;
			}
		}
		else if ( subject.interaction().draggingCorpse() )
		{
			ROTTING_CORPSE* pCorpse = GetRottingCorpse( subject.interaction().draggedCorpse() );

			if ( pCorpse )
			{
				// move all enemy-dropped items along with the corpse, to make it look as if the items are still 'on' the body
				if ( sOldGridNo != subject.position().gridNo() )
					MoveItemPools_ForDragging( pCorpse->def.sGridNo, sOldGridNo, subject.position().level(), subject.position().level() );

				// move corpse to new location. We have to actually delete and recreate the corpse, otherwise direction changes will only be visible after saving the game
				ROTTING_CORPSE_DEFINITION CorpseDef;

				// Copy corpse definition...
				memcpy(&CorpseDef, &(pCorpse->def), sizeof(ROTTING_CORPSE_DEFINITION));

				// Remove old one...
				RemoveCorpse(pCorpse->iID);

				// drop blood at old location
				InternalDropBlood(pCorpse->def.sGridNo, subject.position().level(), 0, 5, 1);

				// adjust both gridno and x,y coordinates
				if (sOldGridNo != subject.position().gridNo())
				{
					INT16 sX, sY;
					ConvertGridNoToCenterCellXY(sOldGridNo, &sX, &sY);

					CorpseDef.sGridNo = sOldGridNo;
					CorpseDef.dXPos = sX;
					CorpseDef.dYPos	= sY;
				}
				else
				{
					// move corpse a bit
					INT16 this_base_x = 0;
					INT16 this_base_y = 0;
					ConvertGridNoToCenterCellXY(subject.position().gridNo(), &this_base_x, &this_base_y);

					FLOAT dx = subject.position().worldX() - this_base_x;
					FLOAT dy = subject.position().worldY() - this_base_y;

					INT16 base_x = 0;
					INT16 base_y = 0;
					ConvertGridNoToCenterCellXY(pCorpse->def.sGridNo, &base_x, &base_y);

					INT16 sX, sY;
					ConvertGridNoToCenterCellXY(pCorpse->def.sGridNo, &sX, &sY);

					CorpseDef.sGridNo	= pCorpse->def.sGridNo;
					CorpseDef.dXPos		= sX + dx;
					CorpseDef.dYPos		= sY + dy;
				}

				CorpseDef.usFlags		|= ROTTING_CORPSE_USE_XY_PROVIDED;

				CorpseDef.ubDirection	= subject.position().direction();

				subject.interaction().dragCorpse(
					static_cast<INT16>( AddRottingCorpse(&CorpseDef) ) );
			}
			else
			{
				dragaborted = true;
			}
		}
		else if ( sOldGridNo != subject.position().gridNo() && subject.interaction().draggingStructure() )
		{
			bool success = false;
			UINT32 arusTileType;
			UINT16 arusStructureNumber;
			UINT8 hitpoints;
			UINT8 decalflag;

			if ( IsDragStructurePresent( subject.interaction().draggedStructureGrid(), subject.position().level(), arusTileType, arusStructureNumber, hitpoints, decalflag ) )
			{
				// add
				if ( BuildStructDrag( sOldGridNo, gsInterfaceLevel, arusTileType, arusStructureNumber, subject.identity().id() ) )
				{
					// as structures might be damaged/have decals, make sure to keep the old values
					CorrectDragStructData( sOldGridNo, (INT8)gsInterfaceLevel, hitpoints, decalflag );

					// remove
					RemoveStructDrag( subject.interaction().draggedStructureGrid(), (INT8)gsInterfaceLevel, arusTileType );

					// also move doors, &subject includes moving locks and traps
					DOOR* pDoor = FindDoorInfoAtGridNo( subject.interaction().draggedStructureGrid() );
					if ( pDoor )
						pDoor->sGridNo = sOldGridNo;

					success = true;
				}
			}

			if ( success )
			{
				// move all items in/on the structure along
				MoveItemPools_ForDragging( subject.interaction().draggedStructureGrid(), sOldGridNo, subject.position().level(), subject.position().level() );

				subject.interaction().dragStructure( sOldGridNo );
			}
			else
			{
				TacticalActorDragging::cancel(subject);

				dragaborted = true;
			}
		}

		if ( !dragaborted )
		{
			// sevenfm: play sound while dragging
			if ( !( subject.featureFlags().secondaryFlags() & SOLDIER_DRAG_SOUND ) )
			{
				SGPFILENAME		zFilename_Used;
				CHAR8	zFilename[512];
				// prepare drag sound
				if ( gsDragSoundNum < 0 )
				{
					gsDragSoundNum = 0;
					do
					{
						gsDragSoundNum++;
						sprintf( zFilename, "Sounds\\Misc\\DragBody%d", gsDragSoundNum );
					} while ( SoundFileExists( zFilename, zFilename_Used ) );
					gsDragSoundNum--;
				}

				if ( gsDragSoundNum > 0 )
				{
					sprintf( zFilename, "Sounds\\Misc\\DragBody%d", Random( gsDragSoundNum ) + 1 );
					if ( SoundFileExists( zFilename, zFilename_Used ) )
					{
						PlayJA2SampleFromFile( zFilename_Used, RATE_11025, SoundVolume( MIDVOLUME, subject.position().gridNo() ), 1, SoundDir( subject.position().gridNo() ) );
					}

					subject.featureFlags().secondaryFlags() |= SOLDIER_DRAG_SOUND;
				}
			}
		}
		else
		{
			TacticalActorDragging::cancel(subject);
		}
	}

	//	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("X: %f Y: %f", dXPos, dYPos ) );
}
