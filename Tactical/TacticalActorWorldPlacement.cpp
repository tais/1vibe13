#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorVisibility.h"
#include "TacticalActorWorldPlacement.h"

#include "Animation Control.h"
#include "Campaign.h"
#include "Explosion Control.h"
#include "Game Clock.h"
#include "GameSettings.h"
#include "Handle Doors.h"
#include "Handle UI.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "Smell.h"
#include "SmokeEffects.h"
#include "Soldier Ani.h"
#include "Soldier Control.h"
#include "Soldier Functions.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "Sound Control.h"
#include "Structure Wrap.h"
#include "TacticalActorAnimationFootprint.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorLighting.h"
#include "TacticalActorMobility.h"
#include "TacticalWorldAdapter.h"
#include "Vehicles.h"
#include "connect.h"
#include "lighting.h"
#include "opplist.h"
#include "renderworld.h"
#include "soldier tile.h"
#include "soundman.h"
#include "strategic.h"
#include "worlddef.h"
#include "worldman.h"

#include <cmath>
#include <cstdint>
#include <limits>

extern INT16 DirIncrementer[8];

void HandleCrowShadowNewGridNo(TacticalActor* actor);
void HandleCrowShadowNewPosition(TacticalActor* actor);
void HandleCrowShadowRemoveGridNo(TacticalActor* actor);
void SetSoldierAniSpeed(TacticalActor* actor);

namespace
{
bool hasWorldPlacementContext(const TacticalActor& actor) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		actor.animationPlayback().state() < NUMANIMATIONSTATES &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL;
}

bool isFiniteHeight(float height) noexcept
{
	return std::isfinite(height) &&
		height >= static_cast<float>(
			std::numeric_limits<INT16>::min()) &&
		height <= static_cast<float>(
			std::numeric_limits<INT16>::max());
}

bool isValidWorldPosition(float worldX, float worldY) noexcept
{
	if (!std::isfinite(worldX) || !std::isfinite(worldY) ||
		worldX < 0.0f || worldY < 0.0f ||
		WORLD_COORD_COLS <= 0 || WORLD_COORD_ROWS <= 0 ||
		worldX >= static_cast<float>(WORLD_COORD_COLS) ||
		worldY >= static_cast<float>(WORLD_COORD_ROWS))
	{
		return false;
	}

	return !TileIsOutOfBounds(
		GETWORLDINDEXFROMWORLDCOORDS(worldY, worldX));
}

void setGridUnchecked(
	TacticalActor& actor,
	INT32 sNewGridNo,
	BOOLEAN fForceRemove);

void removeFromGridUnchecked(TacticalActor& actor, BOOLEAN fForce)
{
	INT8 bDir;
	INT32 iGridNo;

	if ( !TileIsOutOfBounds( actor.position().gridNo() ) )
	{
		if ( actor.roster().inSector() || fForce )
		{
			// Remove from world ( old pos )
			RemoveMerc( actor.position().gridNo(), &actor, FALSE );
			(void)TacticalActorAnimationFootprint::remove(
				actor,
				actor.animationPlayback().state());

			// Remove records of this guy being adjacent
			for ( bDir = 0; bDir < NUM_WORLD_DIRECTIONS; bDir++ )
			{
				iGridNo = actor.position().gridNo() + DirIncrementer[bDir];
				if ( iGridNo >= 0 && iGridNo < WORLD_MAX )
				{
					GetMapElement(
						static_cast<std::uint32_t>(iGridNo))
						.ubAdjacentSoldierCnt--;
				}
			}

			HandlePlacingRoofMarker( &actor, actor.position().gridNo(), FALSE, FALSE );

			// Remove reseved movement value
			UnMarkMovementReserved( &actor );

			HandleCrowShadowRemoveGridNo( &actor );

			// Reset gridno...
			actor.position().gridNo() = NOWHERE;
		}
	}
}

void setPositionUnchecked(
	TacticalActor& actor,
	FLOAT dNewXPos,
	FLOAT dNewYPos,
	BOOLEAN fUpdateDest,
	BOOLEAN fUpdateFinalDest,
	BOOLEAN fForceRemove)
{
	INT32 sNewGridNo;

	// Not if we're dead!
	if ( (actor.status().flags() & SOLDIER_DEAD) )
	{
		return;
	}

	// Set new map index
	sNewGridNo = GETWORLDINDEXFROMWORLDCOORDS( dNewYPos, dNewXPos );

	if ( fUpdateDest )
	{
		actor.pathing().destinationGrid() = sNewGridNo;
	}

	if ( fUpdateFinalDest )
	{
		actor.pathing().finalDestinationGrid() = sNewGridNo;
	}

	// Set the precise coordinates and their established integer projection as
	// one transition. Turn-start coordinates intentionally remain unchanged.
	actor.position().setWorldCoordinates(dNewXPos, dNewYPos);

	HandleCrowShadowNewPosition( &actor );

	setGridUnchecked(actor, sNewGridNo, fForceRemove);

	if ( !(actor.status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
	{
		(void)TacticalActorLighting::positionPersonalLight(actor);
	}

	// ATE: Mirror calls if we are a vehicle ( for all our passengers )
	UpdateAllVehiclePassengersGridNo( &actor );

}

void setHeightUnchecked(TacticalActor& actor, FLOAT dNewHeight, BOOLEAN fUpdateLevel)
{

	INT8	bOldLevel = actor.position().level();

	actor.position().animationHeightAdjustment() = dNewHeight;
	actor.position().heightAdjustment() = (INT16)actor.position().animationHeightAdjustment();

	if ( !fUpdateLevel )
	{
		return;
	}

	// 0verhaul:  Changed this to half the wall height.  During a climb up, a soldier's height increases to about 8, then falls
	// to near 0 before being set to 50 at the end.  The animation offsets should probably be changed to make this unnecessary
	// but this is good enough to keep him from bouncing between level 1 and level 0 (and also triggering weird sight bugs).
	if ( actor.position().heightAdjustment() > 25 )
	{
		actor.position().level() = SECOND_LEVEL;

		ApplyTranslucencyToWalls( (INT16)(actor.position().worldX() / CELL_X_SIZE), (INT16)(actor.position().worldY() / CELL_Y_SIZE) );
		//LightHideTrees((INT16)(actor.position().worldX()/CELL_X_SIZE), (INT16)(actor.position().worldY()/CELL_Y_SIZE));
		//ConcealAllWalls();
	}
	else
	{
		actor.position().level() = FIRST_LEVEL;
	}

	if ( bOldLevel == 0 && actor.position().level() == 0 )
	{

	}
	else
	{
		// Show room at new level
		//HideRoom( actor.sGridNo, &actor );
	}
}



void setGridUnchecked(TacticalActor& actor, INT32 sNewGridNo, BOOLEAN fForceRemove)
{
	BOOLEAN	fInWaterValue;
	INT8		bDir;
	INT32		cnt;
	TacticalActor * pEnemy;

	//INT16	sX, sY, sWorldX, sZLevel;

	// Not if we're dead!
	if ( (actor.status().flags() & SOLDIER_DEAD) )
	{
		return;
	}

	if ( sNewGridNo != actor.position().gridNo() || actor.renderBindings().levelNode() == NULL )
	{
		// Check if we are moving AND this is our next dest gridno....
		if ( gAnimControl[actor.animationPlayback().state()].uiFlags & (ANIM_MOVING | ANIM_SPECIALMOVE) )
		{
			if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
			{
				if ( sNewGridNo != actor.pathing().destinationGrid() )
				{
					// THIS MUST be our new one......MAKE IT SO
					sNewGridNo = actor.pathing().destinationGrid();
				}

				// Now check this baby....
				if ( sNewGridNo == actor.position().gridNo() )
				{
					return;
				}
			}
		}

		actor.movementHistory().recordDeparture(actor.position().gridNo());

		if ( actor.identity().bodyType() == QUEENMONSTER )
		{
			SetPositionSndGridNo( actor.audio().positionSoundId(), sNewGridNo );
		}

		if ( !(actor.status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
		{
			removeFromGridUnchecked(actor, fForceRemove);
		}

		// CHECK IF OUR NEW GIRDNO IS VALID,IF NOT DONOT SET!
		if ( !GridNoOnVisibleWorldTile( sNewGridNo ) )
		{
			actor.position().gridNo() = sNewGridNo;
			return;
		}

		// Alrighty, update UI for this guy, if he's the selected guy...
		if ( gusSelectedSoldier == actor.identity().id() )
		{
			if ( guiCurrentEvent == C_WAIT_FOR_CONFIRM )
			{
				// Update path!
				gfPlotNewMovement = TRUE;
			}
		}


		// Reset some flags for optimizations..
		actor.meleeApproach().invalidate();

		// ATE: Make sure!
		// RemoveMerc( actor.sGridNo, &actor, FALSE );

		actor.position().gridNo() = sNewGridNo;

		// OK, check for special code to close door...
		if ( actor.schedule().doorAnimationComplete() )
		{
			HandleDoorChangeFromGridNo(
				&actor, actor.schedule().consumeDoorGrid(), FALSE );
		}

		// OK, Update buddy's strategic insertion code....
		actor.deployment().strategicInsertionCode() = INSERTION_CODE_GRIDNO;
		actor.deployment().strategicInsertionData() = sNewGridNo;


		// Remove this gridno as a reserved place!
		if ( !(actor.status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
		{
			UnMarkMovementReserved( &actor );
		}

		if ( actor.position().initialGrid() == 0 )
		{
			actor.position().initialGrid() = sNewGridNo;
			actor.aiPlanning().patrolGrid()[0] = sNewGridNo;
		}

		// Add records of this guy being adjacent
		for ( bDir = 0; bDir < NUM_WORLD_DIRECTIONS; bDir++ )
		{
			const INT32 adjacentGrid =
				actor.position().gridNo() + DirIncrementer[bDir];
			if (!TileIsOutOfBounds(adjacentGrid))
			{
				GetMapElement(
					static_cast<std::uint32_t>(adjacentGrid))
					.ubAdjacentSoldierCnt++;
			}
		}

		if ( !(actor.status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
		{
			DropSmell( &actor );
		}

		// HANDLE ANY SPECIAL RENDERING SITUATIONS
		actor.animationActivity().clearRenderZOverride();
		// If we are over a fence ( hopping ), make us higher!

		if ( IsJumpableFencePresentAtGridNo( sNewGridNo ) )
		{
			//sX = MapX( sNewGridNo );
			//sY = MapY( sNewGridNo );
			//GetWorldXYAbsoluteScreenXY( sX, sY, &sWorldX, &sZLevel);
			//actor.animationActivity().setRenderZOverride((sZLevel*Z_SUBLAYERS)+ROOF_Z_LEVEL);
			actor.animationActivity().setRenderZOverride(TOPMOST_Z_LEVEL);
		}
		/*
		if ( IsJumpableWindowPresentAtGridNo( sNewGridNo ) )
		{
		//sX = MapX( sNewGridNo );
		//sY = MapY( sNewGridNo );
		//GetWorldXYAbsoluteScreenXY( sX, sY, &sWorldX, &sZLevel);
		//actor.animationActivity().setRenderZOverride((sZLevel*Z_SUBLAYERS)+ROOF_Z_LEVEL);
		actor.animationActivity().setRenderZOverride(TOPMOST_Z_LEVEL);
		}
		*/

		//ddd window{ ???????
		//if ( IsOknoFencePresentAtGridno( sNewGridNo ) )
		//{
		//	actor.animationActivity().setRenderZOverride(TOPMOST_Z_LEVEL);
		//}
		//ddd window}

		// Add/ remove tree if we are near it
		// CheckForFullStructures( &actor );

		// Add merc at new pos
		if ( !(actor.status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
		{
			AddMercToHead( actor.position().gridNo(), &actor, TRUE );

			// If we are in the middle of climbing the roof!
			if ( actor.animationPlayback().state() == CLIMBUPROOF )
			{
				if ( actor.renderState().lightSprite() != (-1) )
					LightSpriteRoofStatus( actor.renderState().lightSprite(), TRUE );
			}
			else if ( actor.animationPlayback().state() == CLIMBDOWNROOF )
			{
				if ( actor.renderState().lightSprite() != (-1) )
					LightSpriteRoofStatus( actor.renderState().lightSprite(), FALSE );
			}

			if ( actor.animationPlayback().state() == JUMPUPWALL )
			{
				if ( actor.renderState().lightSprite() != (-1) )
					LightSpriteRoofStatus( actor.renderState().lightSprite(), TRUE );
			}
			else if ( actor.animationPlayback().state() == JUMPDOWNWALL )
			{
				if ( actor.renderState().lightSprite() != (-1) )
					LightSpriteRoofStatus( actor.renderState().lightSprite(), FALSE );
			}

			//JA2Gold:
			//if the player wants the merc to cast the fake light AND it is night
			if ( actor.roster().team() != OUR_TEAM || gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] && NightTime( ) )
			{
				MAP_ELEMENT& mapElement = GetMapElement(
					static_cast<std::uint32_t>(
						actor.position().gridNo()));
				if ( actor.position().level() > 0 && mapElement.pRoofHead != NULL )
				{
					mapElement.pMercHead->ubShadeLevel = mapElement.pRoofHead->ubShadeLevel;
					mapElement.pMercHead->ubSumLights = mapElement.pRoofHead->ubSumLights;
					mapElement.pMercHead->ubMaxLights = mapElement.pRoofHead->ubMaxLights;
					mapElement.pMercHead->ubNaturalShadeLevel = mapElement.pRoofHead->ubNaturalShadeLevel;
				}
				else
				{
					mapElement.pMercHead->ubShadeLevel = mapElement.pLandHead->ubShadeLevel;
					mapElement.pMercHead->ubSumLights = mapElement.pLandHead->ubSumLights;
					mapElement.pMercHead->ubMaxLights = mapElement.pLandHead->ubMaxLights;
					mapElement.pMercHead->ubNaturalShadeLevel = mapElement.pLandHead->ubNaturalShadeLevel;
				}
			}

			TacticalActorEquipment::refreshFlashlights(actor);

			///HandlePlacingRoofMarker( &actor, actor.sGridNo, TRUE, FALSE );

			(void)TacticalActorAnimationFootprint::add(
				actor,
				actor.animationPlayback().state());

			HandleCrowShadowNewGridNo( &actor );
		}

		actor.position().enterTerrain(GetTerrainType(actor.position().gridNo()));

		// OK, check that our animation is up to date!
		// Check our water value
		INT16 usUIMovementModeToSet = actor.movement().mode();
		if ( !(actor.status().flags() & (SOLDIER_DRIVER | SOLDIER_PASSENGER)) )
		{
			fInWaterValue = TacticalActorMobility::inWater(actor);

			// ATE: If ever in water MAKE SURE WE WALK AFTERWOODS!
			if ( fInWaterValue )
			{
				usUIMovementModeToSet = WALKING;
			}

			if ( fInWaterValue != actor.movement().previousInWater() )
			{
				//Update Animation data
				SetSoldierAnimationSurface( &actor, actor.animationPlayback().state() );

				// Update flag
				actor.movement().rememberWaterState(fInWaterValue != FALSE);

				// Update sound...
				if ( fInWaterValue )
				{
					PlaySoldierJA2Sample( actor.identity().id(), ENTER_WATER_1, RATE_11025, SoundVolume( MIDVOLUME, actor.position().gridNo() ), 1, SoundDir( actor.position().gridNo() ), TRUE );
				}
				else
				{
					// ATE: Check if we are going from water to land - if so, resume
					// with regular movement mode...
					TacticalActorAnimationTransitions::initializeAnimation(actor,  usUIMovementModeToSet, 0, FALSE );
				}

			}

			// WANNE.WATER: If our soldier is not on the ground level and the tile is a "water" tile, then simply set the tile to "FLAT_GROUND"
			// This should fix "problems" for special modified maps
			if ( (TERRAIN_IS_WATER( actor.position().terrainType() ) || TERRAIN_IS_WATER( actor.position().previousTerrainType() )) && actor.position().level() > 0 )
			{
				actor.position().terrainType() = FLAT_GROUND;
				actor.position().previousTerrainType() = FLAT_GROUND;
			}

			// OK, If we were not in deep water but we are now, handle deep animations!
			if ( TERRAIN_IS_DEEP_WATER( actor.position().terrainType() ) && !TERRAIN_IS_DEEP_WATER( actor.position().previousTerrainType() ) )
			{
				// Based on our current animation, change!
				switch ( actor.animationPlayback().state() )
				{
				case WALKING:
				case WALKING_WEAPON_RDY:
				case WALKING_DUAL_RDY:
				case WALKING_ALTERNATIVE_RDY:
				case RUNNING:
					// IN deep water, swim!
					// Make transition from low to deep
					TacticalActorAnimationTransitions::initializeAnimation(actor,  LOW_TO_DEEP_WATER, 0, FALSE );
					actor.animationIntent().pendingAnimation() = DEEP_WATER_SWIM;
					actor.movement().requestGridUpdateSuppression();
					PlayJA2Sample( ENTER_DEEP_WATER_1, RATE_11025, SoundVolume( MIDVOLUME, actor.position().gridNo() ), 1, SoundDir( actor.position().gridNo() ) );
				}
			}

			// Damage water if in deep water....
			if ( TacticalActorMobility::inHighWater(actor) )
			{
				WaterDamage( &actor );
			}

			// OK, If we were in deep water but we are NOT now, handle mid animations!
			if ( !TERRAIN_IS_DEEP_WATER( actor.position().terrainType() ) && TERRAIN_IS_DEEP_WATER( actor.position().previousTerrainType() ) )
			{
				// Make transition from low to deep
				TacticalActorAnimationTransitions::initializeAnimation(actor,  DEEP_TO_LOW_WATER, 0, FALSE );
				actor.movement().requestGridUpdateSuppression();
				actor.animationIntent().pendingAnimation() = usUIMovementModeToSet;
			}
		}

		// are we now standing in tear gas without a decently working gas mask?
		if ( GetSmokeEffectOnTile( sNewGridNo, actor.position().level() ) > 1 ) //lal: removed normal smoke
		{
			BOOLEAN fSetGassed = TRUE;

			// If we have a functioning gas mask...
			if ( DoesSoldierWearGasMask( &actor ) && actor.inventory()[FindGasMask( &actor )][0]->data.objectStatus >= GASMASK_MIN_STATUS )//dnl ch40 200909
				fSetGassed = FALSE;
			if ( fSetGassed )
			{
				actor.status().flags() |= SOLDIER_GASSED;
			}
		}

		// Flugente: award agility stat increase if we sneak upon an enemy undetected
		// do NOT award this bonus if we are currently loading a game - otherwise one could increase agility by repeatedly saving and reloading the game
		if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
		{
			if ( actor.roster().team() == gbPlayerNum && actor.movement().stealthMode() )
			{
				// Merc got to a new tile by "sneaking". Did we theoretically sneak
				// past an enemy?

				if ( actor.awareness().opponentCount() > 0 )		// opponents in sight
				{
					// check each possible enemy
					for ( cnt = 0; cnt < MAX_NUM_SOLDIERS; ++cnt )
					{
						pEnemy =
							GetJa2SoldierRepository().resolve( cnt );
						// if this guy is here and alive enough to be looking for us
						if ( pEnemy && pEnemy->roster().active() && pEnemy->roster().inSector() && (pEnemy->vitals().health() >= OKLIFE) )
						{
							// no points for sneaking by the neutrals & friendlies!!!
							if ( !pEnemy->aiBehavior().neutral() && (actor.roster().side() != pEnemy->roster().side()) && (pEnemy->identity().bodyType() != COW && pEnemy->identity().bodyType() != CROW) )
							{
								// if we SEE this particular oppponent, and he DOESN'T see us... and he COULD see us...
								if ( (actor.awareness().opponentKnowledge()[cnt] == SEEN_CURRENTLY) &&
									 pEnemy->awareness().opponentKnowledge()[actor.identity().id()] != SEEN_CURRENTLY &&
									 PythSpacesAway( actor.position().gridNo(), pEnemy->position().gridNo() ) < TacticalActorVisibility::maximumDistance(*pEnemy,  actor.position().gridNo(), actor.position().level() ) )
								{
									// AGILITY (5):  Soldier snuck 1 square past unaware enemy
									// MP: skip -- a deathmatch opener (long run, many unaware
									// enemies) turns this trickle into a stat firehose.
									if ( !is_networked )
									{
										StatChange( &actor, AGILAMT, 5, FALSE );
									}
									// Keep looping, we'll give'em 1 point for EACH such enemy!
								}
							}
						}
					}
				}
			}
		}

		// Adjust speed based on terrain, etc
		SetSoldierAniSpeed( &actor );
	}
}
}

bool TacticalActorWorldPlacement::removeFromGrid(
	TacticalActor& actor,
	bool force)
{
	if (!hasWorldPlacementContext(actor))
		return false;

	removeFromGridUnchecked(actor, force ? TRUE : FALSE);
	return true;
}

bool TacticalActorWorldPlacement::setPosition(
	TacticalActor& actor,
	float worldX,
	float worldY,
	bool updateDestination,
	bool updateFinalDestination,
	bool forceRemove)
{
	if (!hasWorldPlacementContext(actor) ||
		(actor.status().flags() & SOLDIER_DEAD) ||
		!isValidWorldPosition(worldX, worldY))
	{
		return false;
	}

	setPositionUnchecked(
		actor,
		worldX,
		worldY,
		updateDestination ? TRUE : FALSE,
		updateFinalDestination ? TRUE : FALSE,
		forceRemove ? TRUE : FALSE);
	return true;
}

bool TacticalActorWorldPlacement::setHeight(
	TacticalActor& actor,
	float height,
	bool updateLevel)
{
	if (!IsJa2TacticalWorldLoaded() ||
		actor.identity().id().i >= TOTAL_SOLDIERS ||
		!isFiniteHeight(height) ||
		!std::isfinite(actor.position().worldX()) ||
		!std::isfinite(actor.position().worldY()))
	{
		return false;
	}

	setHeightUnchecked(
		actor,
		height,
		updateLevel ? TRUE : FALSE);
	return true;
}

bool TacticalActorWorldPlacement::setGrid(
	TacticalActor& actor,
	std::int32_t gridNo,
	bool forceRemove)
{
	if (!hasWorldPlacementContext(actor) ||
		(actor.status().flags() & SOLDIER_DEAD) ||
		(gridNo != NOWHERE && TileIsOutOfBounds(gridNo)))
	{
		return false;
	}

	setGridUnchecked(
		actor,
		static_cast<INT32>(gridNo),
		forceRemove ? TRUE : FALSE);
	return true;
}
