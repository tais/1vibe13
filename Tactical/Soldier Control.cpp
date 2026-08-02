#include "TacticalActorLocomotion.h"
#include "TacticalActor.h"
#include "TacticalActorBloodState.h"
#include "TacticalActorEvents.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalActorPredicates.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorBattleSounds.h"
#include "TacticalActorDamageResolution.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorLifecycle.h"
#include "TacticalActorAppearance.h"
#include "TacticalActorOrientation.h"
#include "TacticalActorVisibility.h"
#include "TacticalActorWorldPlacement.h"
#include "Soldier Functions.h"
#include "TacticalActorAnimationFootprint.h"
#include "TacticalActorAnimationFrames.h"
#include "TacticalActorConsumables.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorRobotics.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalActorTurnMaintenance.h"
#include "TacticalActorAssignments.h"
#include "TacticalActorConditions.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorRecovery.h"
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
#include "Soldier Palette.h"
#include "Grid Direction.h"
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
#include "Disease.h"
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


#ifdef JA2UB
#include "Ja25_Tactical.h"
#include "Ja25 Strategic Ai.h"
#else
#include "Meanwhile.h"
#endif


//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;

UINT16 usForceAnimState = INVALID_ANIMATION;//dnl ch70 170913

//turnspeed
//UINT8 gubPlayerTurnSpeedUpFactor = 1;
//UINT8 gubEnemyTurnSpeedUpFactor = 1;
//UINT8 gubCreatureTurnSpeedUpFactor = 1;
//UINT8 gubMilitiaTurnSpeedUpFactor = 1;
//UINT8 gubCivilianTurnSpeedUpFactor = 1;
//turnspeed

//extern BOOLEAN fAllowTacticalMilitiaCommand; //lal

extern INT16 DirIncrementer[8];

// sevenfm: used in auto taking concertina/sandbag items from inventory
extern BOOLEAN gfShiftBombPlant;

#include "connect.h"

extern void TeleportSelectedSoldier( void );
extern BOOLEAN AddSoldierToSectorNoCalculateDirectionUseAnimation( UINT16 ubID, UINT16 usAnimState, UINT16 usAnimCode );

// sevenfm: check availability of actions
extern BOOLEAN CheckAutoBandage(void);

extern void ReduceAttachmentsOnGunForNonPlayerChars( TacticalActor *pSoldier, OBJECTTYPE * pObj );

UINT8	bHealthStrRanges[] =
{
	15,
	30,
	45,
	60,
	75,
	90,
	101
};


INT16 gsTerrainTypeSpeedModifiers[] =
{
	5,						// NO_TERRAIN // anv: that one was missing
	5,						// Flat ground
	5,						// Floor
	5,						// Paved road
	5,						// Dirt road
	10,						// LOW GRASS
	15,						// HIGH GRASS
	20,						// TRAIN TRACKS
	20,						// LOW WATER
	25,						// MID WATER
	30							// DEEP WATER
};

//Kris:
//Temporary for testing the speed of the translucency.  Pressing Ctrl+L in turn based
//input will toggle this flag.  When clear, the translucency checking is turned off to
//increase the speed of the game.
BOOLEAN gfCalcTranslucency = FALSE;


INT16		gsFullTileDirections[MAX_FULLTILE_DIRECTIONS] =
{
	(INT16)-1, (INT16)(-WORLD_COLS - 1), (INT16)-WORLD_COLS

};

extern BOOLEAN fReDrawFace;
extern UINT8 gubWaitingForAllMercsToExitCode;
BOOLEAN	gfGetNewPathThroughPeople = FALSE;

// LOCAL FUNCTIONS
UINT16 PickSoldierReadyAnimation( TacticalActor *pSoldier, BOOLEAN fEndReady, BOOLEAN fAltWeaponHolding );
BOOLEAN CheckForFullStruct( INT32 sGridNo, UINT16 *pusIndex );
void SetSoldierLocatorOffsets( TacticalActor *pSoldier );
void CheckForFullStructures( TacticalActor *pSoldier );
void SetSoldierAniSpeed( TacticalActor *pSoldier );
void AdjustForFastTurnAnimation( TacticalActor *pSoldier );
UINT16 SelectFireAnimation( TacticalActor *pSoldier, UINT8 ubHeight );
void SelectFallAnimation( TacticalActor *pSoldier );
BOOLEAN FullStructAlone( INT32 sGridNo, UINT8 ubRadius );
void SoldierGotHitGunFire( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation );
void SoldierGotHitBlade( TacticalActor* pSoldier, UINT8 ubHitLocation );
void SoldierGotHitPunch( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation );
void SoldierGotHitExplosion( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation );
void SoldierGotHitVehicle( TacticalActor* pSoldier, UINT16 bDirection );
UINT8 CalcScreamVolume( TacticalActor * pSoldier, UINT8 ubCombinedLoss );
void PlaySoldierFootstepSound( TacticalActor *pSoldier );
void HandleSystemNewAISituation( TacticalActor *pSoldier, BOOLEAN fResetABC );

PIXEL *CreateEnemyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen );
PIXEL *CreateEnemyGreyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen );

void SoldierBleed( TacticalActor *pSoldier, BOOLEAN fBandagedBleed );
INT32 CheckBleeding( TacticalActor *pSoldier );

void HandleVehicleMovementSound( TacticalActor *pSoldier, BOOLEAN fOn )
{
	VEHICLETYPE *pVehicle = &(pVehicleList[pSoldier->vehicleState().tacticalVehicleId()]);

	if ( fOn )
	{
		if ( pVehicle->iMovementSoundID == NO_SAMPLE )
		{
			// anv: will be played in InternalPlaySoldierFootstepSound
			//pVehicle->iMovementSoundID = PlayJA2Sample( pVehicle->iMoveSound, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->sGridNo ), 1, SoundDir( pSoldier->sGridNo ) );
		}
	}
	else
	{
		if ( pVehicle->iMovementSoundID != NO_SAMPLE )
		{
			SoundStop( pVehicle->iMovementSoundID );
			pVehicle->iMovementSoundID = NO_SAMPLE;
		}
	}
}


//gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight
//					TacticalActorAnimationTransitions::changeState(*pSoldier,  SHOOT_RIFLE_STAND, 0 , FALSE );

UINT16 SelectFireAnimation( TacticalActor *pSoldier, UINT8 ubHeight )
{
	INT16 sDist;
	UINT16 usItem;
	FLOAT		dTargetX;
	FLOAT		dTargetY;
	FLOAT		dTargetZ;
	BOOLEAN	fDoLowShot = FALSE;
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "SelectFireAnimation" ) );


	//Do different things if we are a monster
	if ( pSoldier->status().flags() & SOLDIER_MONSTER )
	{
		switch ( pSoldier->identity().bodyType() )
		{
		case ADULTFEMALEMONSTER:
		case AM_MONSTER:
		case YAF_MONSTER:
		case YAM_MONSTER:

			return(MONSTER_SPIT_ATTACK);
			break;

		case LARVAE_MONSTER:

			break;

		case INFANT_MONSTER:

			return(INFANT_ATTACK);
			break;

		case QUEENMONSTER:

			return(QUEEN_SPIT);
			break;

		}
		return(TRUE);
	}

	if ( pSoldier->identity().bodyType() == ROBOTNOWEAPON )
	{
		if ( pSoldier->fireControl().burstCounter() > 0 )
		{
			return(ROBOT_BURST_SHOOT);
		}
		else
		{
			return(ROBOT_SHOOT);
		}
	}

	// Check for rocket laucncher....
	if (ItemIsRocketLauncher(pSoldier->inventory()[HANDPOS].usItem))
	{
		//***ddd if shoot crouched
		if ( ubHeight == ANIM_STAND )
			return(SHOOT_ROCKET);
		if ( ubHeight == ANIM_CROUCH )
			return(SHOOT_ROCKET_CROUCHED);
	}

	// Check for mortar....
	if (ItemIsMortar(pSoldier->inventory()[HANDPOS].usItem))
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "SelectFireAnimation: shoot_mortar" );
		return(SHOOT_MORTAR);
	}

	// Check for tank cannon
	if (ItemIsCannon(pSoldier->inventory()[HANDPOS].usItem))
	{
		return(TANK_SHOOT);
	}

	if ( ARMED_VEHICLE( pSoldier ) )
	{
		return(TANK_BURST);
	}

	// Determine which animation to do...depending on stance and gun in hand...
	switch ( ubHeight )
	{
	case ANIM_STAND:

		usItem = pSoldier->inventory()[HANDPOS].usItem;

		// CHECK 2ND HAND!
		if ( TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) )
		{
			return(BURST_DUAL_STAND);
		}
		else if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !pSoldier->fireControl().burstCounter() )
		{
			return(SHOOT_DUAL_STAND);
		}
		else
		{
			// OK, while standing check distance away from target, and shoot low if we should!
			sDist = PythSpacesAway( pSoldier->position().gridNo(), pSoldier->targeting().gridNo() );

			//ATE: OK, SEE WERE WE ARE TARGETING....
			GetTargetWorldPositions( pSoldier, pSoldier->targeting().gridNo(), &dTargetX, &dTargetY, &dTargetZ );

			//CalculateSoldierZPos( pSoldier, FIRING_POS, &dFirerZ );

			if ( sDist <= 2 && dTargetZ <= 100 )
			{
				fDoLowShot = TRUE;
			}

			// Don't do any low shots if in water
			if ( TacticalActorMobility::inWater(*pSoldier) )
			{
				fDoLowShot = FALSE;
			}


			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				if ( fDoLowShot )
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(LOW_BURST_ALTERNATIVE_STAND);
					}
					else
					{
						return(FIRE_BURST_LOW_STAND);
					}
				}
				else
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(BURST_ALTERNATIVE_STAND);
					}
					else
					{
						return(STANDING_BURST);
					}
				}
			}
			else
			{
				if ( fDoLowShot )
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(LOW_SHOT_ALTERNATIVE_STAND);
					}
					else
					{
						return(FIRE_LOW_STAND);
					}
				}
				else
				{
					if ( TacticalActorWeaponHandling::isValidAlternativeFireMode(*pSoldier,  pSoldier->aiPlanning().aimTime(), pSoldier->targeting().gridNo() ) )
					{
						return(SHOOT_ALTERNATIVE_STAND);
					}
					else
					{
						return(SHOOT_RIFLE_STAND);
					}
				}
			}
		}
		break;

	case ANIM_CROUCH:

		if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && pSoldier->fireControl().burstCounter() > 0 && TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) )
		{
			return(BURST_DUAL_CROUCH);
		}
		else if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !pSoldier->fireControl().burstCounter() )
		{
			return(SHOOT_DUAL_CROUCH);
		}
		else
		{
			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				return(CROUCHED_BURST);
			}
			else
			{
				return(SHOOT_RIFLE_CROUCH);
			}
		}
		break;

	case ANIM_PRONE:

		if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && pSoldier->fireControl().burstCounter() > 0 && TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) )
		{
			return(BURST_DUAL_PRONE);
		}
		else if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !pSoldier->fireControl().burstCounter() )
		{
			return(SHOOT_DUAL_PRONE);
		}
		else
		{
			if ( pSoldier->fireControl().burstCounter() > 0 )
			{
				return(PRONE_BURST);
			}
			else
			{
				return(SHOOT_RIFLE_PRONE);
			}
		}
		break;

	default:
		AssertMsg( FALSE, String( "SelectFireAnimation: ERROR - Invalid height %d", ubHeight ) );
		break;
	}


	// If here, an internal error has occured!
	Assert( FALSE );
	return (0);
}


void SelectFallAnimation( TacticalActor *pSoldier )
{
	// Determine which animation to do...depending on stance and gun in hand...
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FLYBACK_HIT, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FLYBACK_HIT, 0, FALSE );
		break;
	}

}

UINT16 PickSoldierReadyAnimation( TacticalActor *pSoldier, BOOLEAN fEndReady, BOOLEAN fAltWeaponHolding )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "PickSoldierReadyAnimation" ) );

	// Invalid animation if nothing in our hands
	if ( pSoldier->inventory()[HANDPOS].exists( ) == false )
	{
		return(INVALID_ANIMATION);
	}

	if ( TacticalActorMobility::inDeepWater(*pSoldier) )
	{
		return(INVALID_ANIMATION);
	}

	if ( pSoldier->identity().bodyType() == ROBOTNOWEAPON )
	{
		return(INVALID_ANIMATION);
	}

	// Check if we have a gun.....
	if ( Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass != IC_GUN && !ItemIsGrenadeLauncher(pSoldier->inventory()[HANDPOS].usItem) )
	{
		return(INVALID_ANIMATION);
	}

	if (ItemIsRocketLauncher(pSoldier->inventory()[HANDPOS].usItem))
	{
		return(INVALID_ANIMATION);
	}

	if ( ARMED_VEHICLE( pSoldier ) )
	{
		return(INVALID_ANIMATION);
	}

	if ( fEndReady )
	{
		// IF our gun is already drawn, do not change animation, just direction
		if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE) )
		{

			switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
			{
			case ANIM_STAND:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(END_DUAL_STAND);
				}
				else
				{
					if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING) )//&& Item[ pSoldier->inventory()[HANDPOS].usItem ].twohanded)
					{
						return(UNREADY_ALTERNATIVE_STAND);
					}
					//else if (gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_ALT_WEAPON_HOLDING ) && !Item[ pSoldier->inventory()[HANDPOS].usItem ].twohanded)
					//{
					//	return( PISTOL_FASTSHOT_UNREADY );
					//}
					else
					{
						return(END_RIFLE_STAND);
					}
				}
				break;

			case ANIM_PRONE:

				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(END_DUAL_PRONE);
				}
				else
				{
					return(END_RIFLE_PRONE);
				}
				break;

			case ANIM_CROUCH:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(END_DUAL_CROUCH);
				}
				else
				{
					return(END_RIFLE_CROUCH);
				}
				break;

			}

		}
	}
	else
	{
		// if our gun is in alternative holding (hip rifle/one-hand pistol) and we are going to shoulder
		if ( (gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING)) && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND && !fAltWeaponHolding && !Weapon[pSoldier->inventory()[pSoldier->attackSelection().hand()].usItem].HeavyGun )
		{
			return(READY_RIFLE_STAND);
		}
		// this is a specific situation when we have a gun in standard holding (shouldered rifle/two-hand pistol) and was told to go to alternative holding
		else if ( (gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE)) && !(gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING))
				  && fAltWeaponHolding && gGameExternalOptions.ubAllowAlternativeWeaponHolding == 3 && pSoldier->attackSelection().scopeMode() == -1 && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND
				  && ((!ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem) && !TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) && !TacticalActorMobility::inWater(*pSoldier)) || ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem)) )
		{
			return(READY_ALTERNATIVE_STAND);
		}
		// IF our gun is already drawn, do not change animation, just direction
		else if ( !(gAnimControl[pSoldier->animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE)) )
		{
			switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
			{
			case ANIM_STAND:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(READY_DUAL_STAND);
				}
				else
				{
					if ( gGameExternalOptions.ubAllowAlternativeWeaponHolding )
					{
						if ( fAltWeaponHolding || (Weapon[pSoldier->inventory()[pSoldier->attackSelection().hand()].usItem].HeavyGun && ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem)) )
						{
							return(READY_ALTERNATIVE_STAND);
						}
						else
						{
							return(READY_RIFLE_STAND);
						}
					}
					else
					{
						return(READY_RIFLE_STAND);
					}
				}
				break;

			case ANIM_PRONE:
				// Go into crouch, turn, then go into prone again
				//(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_CROUCH );
				//pSoldier->animationIntent().desiredHeight() = ANIM_PRONE;
				//TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_UP );
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(READY_DUAL_PRONE);
				}
				else
				{
					return(READY_RIFLE_PRONE);
				}
				break;

			case ANIM_CROUCH:

				// CHECK 2ND HAND!
				if ( TacticalActorWeaponHandling::isValidSecondHandShot(*pSoldier) )
				{
					return(READY_DUAL_CROUCH);
				}
				else
				{
					return(READY_RIFLE_CROUCH);
				}
				break;

			}

		}
	}

	return(INVALID_ANIMATION);
}

// 0verhaul:  These routines are obsolete.  Just call ReduceAttackBusyCount to reduce the ABC or
// FreeUpAttacker to abort the current action.
// extern TacticalActor * FreeUpAttackerGivenTarget( UINT8 ubID, UINT8 ubTargetID );
// extern TacticalActor * ReduceAttackBusyGivenTarget( UINT8 ubID, UINT8 ubTargetID );

// ATE: THIS FUNCTION IS USED FOR ALL SOLDIER TAKE DAMAGE FUNCTIONS!

UINT8 CalcScreamVolume( TacticalActor * pSoldier, UINT8 ubCombinedLoss )
{
	// NB explosions are so loud they should drown out screams
	UINT8 ubVolume;

	if ( ubCombinedLoss < 1 )
	{
		ubVolume = 1;
	}
	else
	{
		ubVolume = ubCombinedLoss;
	}

	// Victim yells out in pain, making noise.  Yelps are louder from greater
	// wounds, but softer for more experienced soldiers.

	if ( ubVolume > (10 - EffectiveExpLevel( pSoldier )) )
	{
		ubVolume = 10 - EffectiveExpLevel( pSoldier );
	}

	/*
	// the "Speck factor"...  He's a whiner, and extra-sensitive to pain!
	if (ptr->trait == NERVOUS)
	ubVolume += 2;
	*/

	if ( ubVolume < 0 )
	{
		ubVolume = 0;
	}

	return(ubVolume);
}


void DoGenericHit( TacticalActor *pSoldier, UINT8 ubSpecial, INT16 bDirection )
{
	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		// For now, check if we are affected by a burst
		// For now, if the weapon was a gun, special 1 == burst
		// ATE: Only do this for mercs!
		if ( ubSpecial == FIRE_WEAPON_BURST_SPECIAL && pSoldier->identity().bodyType() <= REGFEMALE )
		{
			//SetSoldierDesiredDirection( pSoldier, bDirection );
			(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)bDirection );
			(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  STANDING_BURST_HIT, 0, FALSE );
		}
		else
		{
			// Check in hand for rifle
			if ( TacticalActorEquipment::carriesTwoHandedWeapon(*pSoldier) )
			{
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  RIFLE_STAND_HIT, 0, FALSE );
			}
			else
			{
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_STAND, 0, FALSE );
			}
		}
		break;

	case ANIM_PRONE:

		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_PRONE, 0, FALSE );
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	}
}


void SoldierGotHitGunFire( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation )
{
	INT32	usNewGridNo;
	BOOLEAN	fBlownAway = FALSE;
	BOOLEAN	fHeadHit = FALSE;
	BOOLEAN	fFallenOver = FALSE;
	TacticalActor* attacker =
		GetJa2SoldierRepository().resolve( ubAttackerID );

	// MAYBE CHANGE TO SPECIAL ANIMATION BASED ON VALUE SET BY DAMAGE CALCULATION CODE
	// ALL THESE ONLY WORK ON STANDING PEOPLE
	if ( !(pSoldier->status().flags() & SOLDIER_MONSTER) && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND && (!(gTacticalStatus.uiFlags & GODMODE) || pSoldier->roster().team() != OUR_TEAM))
	{
		if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND )
		{
			if ( ubSpecial == FIRE_WEAPON_HEAD_EXPLODE_SPECIAL )
			{
				if ( gGameSettings.fOptions[TOPTION_BLOOD_N_GORE] )
				{
					// HEADROCK HAM 3.6: Reattached the XML maximum-distance setting.

					UINT8 ubDistMessy = Weapon[usWeaponIndex].maxdistformessydeath;
					// modify by ini values
					if ( Item[usWeaponIndex].usItemClass == IC_GUN )
						ubDistMessy *= gItemSettings.fDistMessyModifierGun[Weapon[usWeaponIndex].ubWeaponType];

					if ( attacker != nullptr &&
						SpacesAway( pSoldier->position().gridNo(),
							attacker->position().gridNo() ) <= ubDistMessy )
					{
						usNewGridNo = NewGridNo( pSoldier->position().gridNo(), (INT8)(DirectionInc( pSoldier->position().direction() )) );

						// CHECK OK DESTINATION!
						if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), pSoldier->position().direction(), JFK_HITDEATH ) )
						{
							usNewGridNo = NewGridNo( usNewGridNo, (INT8)(DirectionInc( pSoldier->position().direction() )) );

							if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), pSoldier->position().direction(), pSoldier->animationPlayback().state() ) )
							{
								fHeadHit = TRUE;
							}
						}
					}
				}
			}
			else if ( ubSpecial == FIRE_WEAPON_CHEST_EXPLODE_SPECIAL )
			{
				if ( gGameSettings.fOptions[TOPTION_BLOOD_N_GORE] )
				{
					// HEADROCK HAM 3.6: Reattached the XML maximum-distance setting.

					UINT8 ubDistMessy = Weapon[usWeaponIndex].maxdistformessydeath;
					// modify by ini values
					if ( Item[usWeaponIndex].usItemClass == IC_GUN )
						ubDistMessy *= gItemSettings.fDistMessyModifierGun[Weapon[usWeaponIndex].ubWeaponType];

					if ( attacker != nullptr &&
						SpacesAway( pSoldier->position().gridNo(),
							attacker->position().gridNo() ) <= ubDistMessy )
					{

						// possibly play torso explosion anim!
						if ( pSoldier->position().direction() == bDirection )
						{
							usNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[pSoldier->position().direction()] ) );

							if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], FLYBACK_HIT ) )
							{
								usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( gOppositeDirection[bDirection] ) );

								if ( OKFallDirection( pSoldier, usNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], pSoldier->animationPlayback().state() ) )
								{
									fBlownAway = TRUE;
								}
							}
						}
					}
				}
			}
			else if ( ubSpecial == FIRE_WEAPON_LEG_FALLDOWN_SPECIAL )
			{
				// possibly play fall over anim!
				// this one is NOT restricted by distance
				if ( IsValidStance( pSoldier, ANIM_PRONE ) )
				{
					// Can't be in water, or not standing
					if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND && !TacticalActorMobility::inWater(*pSoldier) )
					{
						fFallenOver = TRUE;
						ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, gzLateLocalizedString[20], pSoldier->GetName( ) );
					}
				}
			}
		}
	}

	// Flugente: if hit in legs or torso, blood will be on our uniform - parts of the clothes cannot be worn anymore
	if ( ubHitLocation == AIM_SHOT_TORSO )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
	else if ( ubHitLocation == AIM_SHOT_LEGS )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_PANTS;

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		// 0verhaul:  Handled in the soldier state change code
		// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker,Dead soldier hit" ) );
		// ReleaseSoldiersAttacker( pSoldier );
		return;
	}

	if ( fFallenOver )
	{
		// HEADROCK HAM 3.2: Critical legshots cost an extra number of APs, based on shot damage.
		if ( gGameExternalOptions.fCriticalLegshotCausesAPLoss )
		{
			DeductPoints( pSoldier, APBPConstants[AP_LOSS_PER_LEGSHOT_DAMAGE] * sDamage, 0, DISABLED_INTERRUPT );
		}
		(void)TacticalActorRecovery::collapse(*pSoldier);
		return;
	}

	if ( fBlownAway )
	{
		// Only for mercs...
		if ( pSoldier->identity().bodyType() < 4 )
		{
			(void)TacticalActorCombatReactions::
				beginFlyback(
					*pSoldier,
					static_cast<std::uint8_t>(
						bDirection));

			// Flugente: dynamic opinions
			if (gGameExternalOptions.fDynamicOpinions && attacker != nullptr )
				HandleDynamicOpinionChange( attacker, OPINIONEVENT_BRUTAL_GOOD, TRUE, TRUE );

			return;
		}
	}

	if ( fHeadHit )
	{
		// Only for mercs ( or KIDS! )
		if ( pSoldier->identity().bodyType() < 4 || pSoldier->identity().bodyType() == HATKIDCIV || pSoldier->identity().bodyType() == KIDCIV )
		{
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  JFK_HITDEATH, 0, FALSE );

			// Flugente: dynamic opinions
			if (gGameExternalOptions.fDynamicOpinions && attacker != nullptr )
				HandleDynamicOpinionChange( attacker, OPINIONEVENT_BRUTAL_GOOD, TRUE, TRUE );

			return;
		}
	}

	DoGenericHit( pSoldier, ubSpecial, bDirection );
}

void SoldierGotHitExplosion( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation )
{
	// Flugente: if hit in legs or torso, blood will be on our uniform - parts of the clothes cannot be worn anymore
	if ( ubHitLocation == AIM_SHOT_TORSO )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
	else if ( ubHitLocation == AIM_SHOT_LEGS )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_PANTS;

	INT32 sNewGridNo;

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}

	//check for services
	TacticalActorMedicalServices::cancelReceiving(
		*pSoldier);
	TacticalActorMedicalServices::cancelProviding(
		*pSoldier);


	if ( gGameSettings.fOptions[TOPTION_BLOOD_N_GORE] )
	{
		if ( Explosive[Item[usWeaponIndex].ubClassIndex].ubRadius >= 3 && pSoldier->vitals().health() == 0 && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight != ANIM_PRONE )
		{
			if ( sRange >= 2 && sRange <= 4 )
			{
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_HIT1 );

				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  CHARIOTS_OF_FIRE, 0, FALSE );
				return;
			}
			else if ( sRange <= 1 )
			{
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_HIT1 );

				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  BODYEXPLODING, 0, FALSE );
				return;
			}
		}
	}

	// If we can't fal back or such, so generic hit...
	if ( pSoldier->identity().bodyType() >= 4 )
	{
		DoGenericHit( pSoldier, 0, bDirection );
		return;
	}

	// Lesh: possible soldier behavior when affected by flashbang
	// Soldier can:
	//   1. stand as if there was no explosion at all
	//   2. crouch. represent that soldier didn't expect such blow and instinctively
	//      made defensive movement to protect his body
	//   3. fall forward. again, he didn't expect that something will explode behind
	//      him and deafens him
	//   4. fall backward. unexpected blast, fear, clumsy moves and soldier flies backward.

	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		if ( ubSpecial == FIRE_WEAPON_DEAFENED )
		{
			switch ( Random( 10 ) )
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				// 6 of 10 - crouch
				(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_CROUCH );
				break;
			case 6:
			case 7:
			case 8:
				// 3 of 10 - fall forward
				(void)TacticalActorCombatReactions::
					beginFall(*pSoldier);
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
				break;
			case 9:
				// 1 of 10 - still standing
				DoGenericHit( pSoldier, 0, bDirection );
				break;
			};
			break;
		}
		else if ( ubSpecial == FIRE_WEAPON_BLINDED_AND_DEAFENED )
		{
			switch ( Random( 10 ) )
			{
			case 0:
			case 1:
			case 2:
			case 3:
			case 4:
				// 5 of 10 - crouch
				(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_CROUCH );
				break;
			case 5:
			case 6:
			case 7:
			case 8:
				// 4 of 10 - fall backward (if possible) either forward
				// Check behind us!
				sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[bDirection] ) );
				if ( OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], FLYBACK_HIT ) )
				{
					(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)bDirection );
					(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
					(void)TacticalActorCombatReactions::
						beginFallback(
							*pSoldier,
							static_cast<std::uint8_t>(
								bDirection));
				}
				else
				{
					(void)TacticalActorCombatReactions::
						beginFall(*pSoldier);
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
				}
				break;
			case 9:
				// 1 of 10 - still standing
				DoGenericHit( pSoldier, 0, bDirection );
				break;
			};
			break;
		}
		else if ( ubSpecial == FIRE_WEAPON_BLINDED )
		{
		}

	case ANIM_CROUCH:

		if ( ubSpecial == FIRE_WEAPON_BLINDED ||
			 ubSpecial == FIRE_WEAPON_BLINDED_AND_DEAFENED ||
			 ubSpecial == FIRE_WEAPON_DEAFENED )
		{
			DoGenericHit( pSoldier, 0, bDirection );
			break;
		}

		(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)bDirection );
		(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

		// Check behind us!
		sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[bDirection] ) );

		if ( OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), gOppositeDirection[bDirection], FLYBACK_HIT ) )
		{
			(void)TacticalActorCombatReactions::
				beginFallback(
					*pSoldier,
					static_cast<std::uint8_t>(
						bDirection));
		}
		else
		{
			if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND )
			{
				(void)TacticalActorCombatReactions::
					beginFall(*pSoldier);
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
			}
			else
			{
				(void)TacticalActorRecovery::collapse(*pSoldier);
			}
		}
		break;

	case ANIM_PRONE:

		(void)TacticalActorRecovery::collapse(*pSoldier);
		break;
	}
}


void SoldierGotHitBlade( TacticalActor *pSoldier, UINT8 ubHitLocation )
{
	// Flugente: if hit in legs or torso, blood will be on our uniform - parts of the clothes cannot be worn anymore
	if ( ubHitLocation == AIM_SHOT_TORSO )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
	else if ( ubHitLocation == AIM_SHOT_LEGS )
		pSoldier->featureFlags().primaryFlags() |= SOLDIER_DAMAGED_PANTS;

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}


	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:

		// Check in hand for rifle
		if ( TacticalActorEquipment::carriesTwoHandedWeapon(*pSoldier) )
		{
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  RIFLE_STAND_HIT, 0, FALSE );
		}
		else
		{
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_STAND, 0, FALSE );
		}
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_PRONE, 0, FALSE );
		break;
	}
}


void SoldierGotHitPunch( TacticalActor *pSoldier, UINT16 usWeaponIndex, INT16 sDamage, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation )
{

	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}

	// Based on stance, select generic hit animation
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		// Check in hand for rifle
		if ( TacticalActorEquipment::carriesTwoHandedWeapon(*pSoldier) )
		{
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  RIFLE_STAND_HIT, 0, FALSE );
		}
		else
		{
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_STAND, 0, FALSE );
		}
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_CROUCH, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  GENERIC_HIT_PRONE, 0, FALSE );
		break;

	}

}

void SoldierGotHitVehicle(TacticalActor *pSoldier, UINT16 bDirection)
{
	INT32 sNewGridNo = 0;
	// IF HERE AND GUY IS DEAD, RETURN!
	if ( pSoldier->status().flags() & SOLDIER_DEAD )
	{
		return;
	}

	if ( pSoldier->animationActivity().tryingToFall() )
	{
		return;
	}

	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:

		sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( bDirection ) );//DirectionInc( gOppositeDirection[ bDirection ] ) );
		if ( IS_MERC_BODY_TYPE( pSoldier ) && OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), bDirection, FLYBACK_HIT ) )
		{
			(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)gOppositeDirection[bDirection] );
			(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
			(void)TacticalActorCombatReactions::
				beginFallback(
					*pSoldier,
					gOppositeDirection[bDirection]);
		}
		else if ( IS_MERC_BODY_TYPE( pSoldier ) )
		{
			(void)TacticalActorOrientation::setDirection(*pSoldier, bDirection );
			(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
			(void)TacticalActorCombatReactions::
				beginFall(*pSoldier);
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
		}
		else
		{
			(void)TacticalActorRecovery::collapse(*pSoldier);
		}
		break;


	case ANIM_CROUCH:

		(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)gOppositeDirection[bDirection] );
		(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

		// Check behind us!
		sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( bDirection ) );

		if ( IS_MERC_BODY_TYPE( pSoldier ) && OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), gOppositeDirection[pSoldier->position().direction()], FLYBACK_HIT ) )
		{
			(void)TacticalActorCombatReactions::
				beginFallback(
					*pSoldier,
					gOppositeDirection[bDirection]);
		}
		else
		{
			(void)TacticalActorRecovery::collapse(*pSoldier);
		}
		break;

	case ANIM_PRONE:

		(void)TacticalActorRecovery::collapse(*pSoldier);
		break;
	}

}


UINT8	gRedGlowR[] =
{
	0,			// Normal shades
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

	0,		// For gray palettes
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

};



UINT8	gOrangeGlowR[] =
{
	0,			// Normal shades
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

	0,		// For gray palettes
	25,
	50,
	75,
	100,
	125,
	150,
	175,
	200,
	225,

};



UINT8	gOrangeGlowG[] =
{
	0,			// Normal shades
	20,
	40,
	60,
	80,
	100,
	120,
	140,
	160,
	180,

	0,		// For gray palettes
	20,
	40,
	60,
	80,
	100,
	120,
	140,
	160,
	180,

};



void AdjustAniSpeed( TacticalActor *pSoldier )
{
	if ( (gTacticalStatus.uiFlags & SLOW_ANIMATION) )
	{
		if ( gTacticalStatus.bRealtimeSpeed == -1 )
		{
			pSoldier->animationPlayback().delay() = 10000;
		}
		else
		{
			pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() * (1 * gTacticalStatus.bRealtimeSpeed / 2);
		}
	}

	//pSoldier->animationPlayback().delay() =1;//for max speed uncomment //ddd
	pSoldier->timing().start(SoldierTimingComponent::Timer::AnimationUpdate, pSoldier->animationPlayback().delay());
}


void CalculateSoldierAniSpeed( TacticalActor *pSoldier, TacticalActor *pStatsSoldier )
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "CalculateSoldierAniSpeed" );
	INT16 sTerrainDelay;

	INT8 bBreathDef = 0, bLifeDef = 0;
	INT16 bAgilDef = 0;
	INT16 bAdditional = 0;
	INT16 legbrokenpenalty = 60;

	// for those animations which have a speed of zero, we have to calculate it
	// here. Some animation, such as water-movement, have an ADDITIONAL speed
	switch ( pSoldier->animationPlayback().state() )
	{
		// Lesh: bursting animation delay control begins
		// Add your animation ID to control it
	case STANDING_BURST:
	case FIRE_STAND_BURST_SPREAD:
	case FIRE_BURST_LOW_STAND:
	case TANK_BURST:
	case CROUCHED_BURST:
	case PRONE_BURST:
	case BURST_ALTERNATIVE_STAND:
	case LOW_BURST_ALTERNATIVE_STAND:
		pSoldier->animationPlayback().delay() = Weapon[Item[pSoldier->inventory()[HANDPOS].usItem].ubClassIndex].sAniDelay;
		AdjustAniSpeed( pSoldier );
		return;
	case BURST_DUAL_STAND:
	case BURST_DUAL_CROUCH:
	case BURST_DUAL_PRONE:
		pSoldier->animationPlayback().delay() = (Weapon[Item[pSoldier->inventory()[HANDPOS].usItem].ubClassIndex].sAniDelay) / 2;
		AdjustAniSpeed( pSoldier );
		return;

	case PRONE:
	case STANDING:

		pSoldier->animationPlayback().delay() = (pStatsSoldier->vitals().breath() * 2) + (100 - pStatsSoldier->vitals().health());

		// Limit it!
		if ( pSoldier->animationPlayback().delay() < 40 )
		{
			pSoldier->animationPlayback().delay() = 40;
		}
		AdjustAniSpeed( pSoldier );
		return;

	case CROUCHING:

		pSoldier->animationPlayback().delay() = (pStatsSoldier->vitals().breath() * 2) + ((100 - pStatsSoldier->vitals().health()));

		// Limit it!
		if ( pSoldier->animationPlayback().delay() < 40 )
		{
			pSoldier->animationPlayback().delay() = 40;
		}
		AdjustAniSpeed( pSoldier );
		return;

	case WALKING:
	case WALKING_WEAPON_RDY:
	case WALKING_DUAL_RDY:
	case WALKING_ALTERNATIVE_RDY:

		// Adjust based on body type
		bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

		// Flugente: disease can stop us from using our arms normally
		if ( gGameExternalOptions.fDisease
			&& gGameExternalOptions.fDiseaseSevereLimitations
			&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
			bAdditional += legbrokenpenalty;

		if ( bAdditional < 0 )
			bAdditional = 0;
		break;

	case RUNNING:

		// Adjust based on body type
		bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

		// Flugente: disease can stop us from using our arms normally
		if ( gGameExternalOptions.fDisease
			&& gGameExternalOptions.fDiseaseSevereLimitations
			&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
			bAdditional += legbrokenpenalty;

		if ( bAdditional < 0 )
			bAdditional = 0;
		break;

	case SWATTING:
		//***ddd
	case SWATTING_WK:
	case SWAT_BACKWARDS_WK:
	case CROUCHEDMOVE_RIFLE_READY:
	case CROUCHEDMOVE_PISTOL_READY:
	case CROUCHEDMOVE_DUAL_READY:
	case SIDE_STEP_CROUCH_RIFLE:
	case SIDE_STEP_CROUCH_PISTOL:
	case SIDE_STEP_CROUCH_DUAL:

		// Adjust based on body type
		if ( pStatsSoldier->identity().bodyType() <= REGFEMALE )
		{
			bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

			// Flugente: disease can stop us from using our arms normally
			if ( gGameExternalOptions.fDisease
				&& gGameExternalOptions.fDiseaseSevereLimitations
				&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
				bAdditional += legbrokenpenalty;

			if ( bAdditional < 0 )
				bAdditional = 0;
		}
		break;

	case CRAWLING:

		// Adjust based on body type
		if ( pStatsSoldier->identity().bodyType() <= REGFEMALE )
		{
			bAdditional = gubAnimWalkSpeeds[pStatsSoldier->identity().bodyType()].sSpeed;

			// Flugente: disease can stop us from using our arms normally
			if ( gGameExternalOptions.fDisease
				&& gGameExternalOptions.fDiseaseSevereLimitations
				&& TacticalActorDisease::hasOutbreakProperty(*pSoldier, DISEASE_PROPERTY_LIMITED_USE_LEGS ) )
				bAdditional += legbrokenpenalty;

			if ( bAdditional < 0 )
				bAdditional = 0;
		}
		break;

	case READY_RIFLE_STAND:

		// Raise rifle based on aim vs non-aim.
		if ( pSoldier->aiPlanning().aimTime() == 0 )
		{
			// Quick shot
			pSoldier->animationPlayback().delay() = 100;
		}
		else
		{
			pSoldier->animationPlayback().delay() = 200;
		}
		AdjustAniSpeed( pSoldier );
		return;
	}

	// figure out movement speed (terrspeed)
	if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_MOVING )
	{
		sTerrainDelay = gsTerrainTypeSpeedModifiers[pStatsSoldier->position().terrainType()];
	}
	else
	{
		sTerrainDelay = 40;			// standing still
	}

	if ( !(pSoldier->status().flags() & SOLDIER_VEHICLE) )
	{
		bBreathDef = 50 - (pStatsSoldier->vitals().breath() / 2);

		if ( bBreathDef > 30 )
			bBreathDef = 30;

		bAgilDef = 50 - (EffectiveAgility( pStatsSoldier, FALSE ) / 4);
		bLifeDef = 50 - (pStatsSoldier->vitals().health() / 2);
	}
	else
	{
		// anv: vehicles have no agility and making them slower with less fuel would make no sense
		// instead take gear into consideration here
		if ( pSoldier->status().flags() & SOLDIER_VEHICLE && pSoldier->animationPlayback().state() == RUNNING )
		{
			bAgilDef = 10;
		}
		else
		{
			bAgilDef = 30;
		}
	}

	sTerrainDelay += (bLifeDef + bBreathDef + bAgilDef + bAdditional);

	// Flugente: backgrounds
	switch ( pSoldier->animationPlayback().state() )
	{
	case WALKING:
	case WALKING_WEAPON_RDY:
	case WALKING_DUAL_RDY:
	case CROUCHEDMOVE_RIFLE_READY:
	case CROUCHEDMOVE_PISTOL_READY:
	case CROUCHEDMOVE_DUAL_READY:
	case WALKING_ALTERNATIVE_RDY:
	case RUNNING:
	case SWATTING:
	case SWATTING_WK:
	case SIDE_STEP_CROUCH_RIFLE:
	case SIDE_STEP_CROUCH_PISTOL:
	case SIDE_STEP_CROUCH_DUAL:
	case SWAT_BACKWARDS_WK:
		// Flugente: background running speed reduces time needed: + is good, - is bad
		sTerrainDelay = ( sTerrainDelay * (100 - TacticalActorModifiers::backgroundValue(*pSoldier, BG_PERC_SPEED_RUNNING ))) / 100;
		break;

	default:
		break;
	}

	pSoldier->animationPlayback().delay() = sTerrainDelay;

	// If a moving animation and we're on drugs, increase speed....
	if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_MOVING )
	{
		if ( pSoldier->drugState().magnitude(DRUG_EFFECT_AP) )
		{
			pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
		}
	}

	// MODIFTY NOW BASED ON REAL-TIME, ETC
	// Adjust speed, make twice as fast if in turn-based!
	if ( IsJa2TacticalTurnBasedCombat() )
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
	}

	// MODIFY IF REALTIME COMBAT
	if ( !(IsJa2TacticalCombatActive()) )
	{
		// ATE: If realtime, and stealth mode...
		if ( pStatsSoldier->movement().stealthMode() )
		{
			if ( gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT( pSoldier, STEALTHY_NT ) )
			{
				// Stealthy skill decreases movement speed penalty while on stealthy mode - SANDRO
				pSoldier->animationPlayback().delay() = (INT16)((pSoldier->animationPlayback().delay() * (200 - gSkillTraitValues.ubSTStealthModeSpeedBonus)) / 100);
			}
			else // original
			{
				pSoldier->animationPlayback().delay() = (INT16)(pSoldier->animationPlayback().delay() * 2);
			}
		}

		// SANDRO - STOMP traits - bonus to movement speed for Athletics
		if ( gGameOptions.fNewTraitSystem && (gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_MOVING) )
		{
			if ( HAS_SKILL_TRAIT( pSoldier, ATHLETICS_NT ) )
			{
				pSoldier->animationPlayback().delay() = (INT16)(pSoldier->animationPlayback().delay() * (100 - min( 75, gSkillTraitValues.ubATAPsMovementReduction )) / 100);
			}
		}

		//pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() * ( 1 * gTacticalStatus.bRealtimeSpeed / 2 );
	}

	// Flugente: riot shields lower movement speed
	if (TacticalActorEquipment::hasEquippedRiotShield(*pSoldier))
	{
		pSoldier->animationPlayback().delay() = gItemSettings.fShieldMovementAPCostModifier * pSoldier->animationPlayback().delay();
	}

	// Flugente: drag people
	if (TacticalActorDragging::isDragging(*pSoldier))
	{
		pSoldier->animationPlayback().delay() = gItemSettings.fDragAPCostModifier * pSoldier->animationPlayback().delay();
	}
}

FLOAT GetSpeedUpFactor( )
{
	switch ( GetJa2TacticalCurrentTeam() )
	{
	case OUR_TEAM:
		return gGameExternalOptions.giPlayerTurnSpeedUpFactor;
	case ENEMY_TEAM:
		return gGameExternalOptions.giEnemyTurnSpeedUpFactor;
	case CREATURE_TEAM:
		return gGameExternalOptions.giCreatureTurnSpeedUpFactor;
	case MILITIA_TEAM:
		return gGameExternalOptions.giMilitiaTurnSpeedUpFactor;
	case CIV_TEAM:
		return gGameExternalOptions.giCivilianTurnSpeedUpFactor;
	}

	return 1.0;
}

void SetSoldierAniSpeed( TacticalActor *pSoldier )
{
	TacticalActor *pStatsSoldier;

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "SetSoldierAniSpeed" );

	// ATE: If we are an enemy and are not visible......
	// Set speed to 0
	if ( !is_client )
	{
		if ( (IsJa2TacticalTurnBasedCombat()) || gTacticalStatus.fAutoBandageMode )
		{
			if ( ((pSoldier->awareness().visibility() == -1 && pSoldier->awareness().visibility() == pSoldier->awareness().lastRenderedVisibility()) || gTacticalStatus.fAutoBandageMode) && pSoldier->animationPlayback().state() != MONSTER_UP )
			{
				if ( pSoldier->fireControl().burstCounter() && !PTR_OURTEAM )
				{
					pSoldier->animationPlayback().delay() = 50;
				}
				else
				{
					pSoldier->animationPlayback().delay() = 0;
				}
				pSoldier->timing().start(SoldierTimingComponent::Timer::AnimationUpdate, pSoldier->animationPlayback().delay());
				return;
			}
		}
	}

	// Default stats soldier to same as normal soldier.....
	pStatsSoldier = pSoldier;

	if ( pSoldier->movement().usesMoveSpeedOverride() )
	{
		if (pSoldier->movement().moveSpeedOverride() < NOBODY)
		{
			TacticalActor* overrideSoldier =
				GetJa2SoldierRepository().resolve(
					pSoldier->movement().moveSpeedOverride() );
			if ( overrideSoldier != nullptr )
			{
				pStatsSoldier = overrideSoldier;
			}
		}
	}

	// Only calculate if set to zero
	if ( (pSoldier->animationPlayback().delay() = gAnimControl[pSoldier->animationPlayback().state()].sSpeed) == 0 )
	{
		CalculateSoldierAniSpeed( pSoldier, pStatsSoldier );
	}

	AdjustAniSpeed( pSoldier );

	// SANDRO - make the spin kick animation a bit faster
	if (pSoldier->animationPlayback().state() == NINJA_SPINKICK ||
		pSoldier->animationPlayback().state() == FOCUSED_PUNCH || pSoldier->animationPlayback().state() == FOCUSED_STAB || pSoldier->animationPlayback().state() == FOCUSED_HTH_KICK)
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
	}

	// sevenfm: faster radio animation
	if (pSoldier->animationPlayback().state() == AI_RADIO || pSoldier->animationPlayback().state() == AI_CR_RADIO)
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 2;
	}

	// sevenfm: faster sidestepping
	if (pSoldier->animationPlayback().state() == SIDE_STEP || pSoldier->animationPlayback().state() == SIDE_STEP_ALTERNATIVE_RDY || pSoldier->animationPlayback().state() == SIDE_STEP_WEAPON_RDY || pSoldier->animationPlayback().state() == SIDE_STEP_DUAL_RDY)
	{
		pSoldier->animationPlayback().delay() = pSoldier->animationPlayback().delay() / 4;
	}

	if ( _KeyDown( SPACE ) )
	{
		//pSoldier->animationPlayback().delay() = 1000;
	}

	if ( IsJa2TacticalTurnBasedCombat() )
	{
		// braces make the binding explicit: the else belongs to the inner
		// 'if ( GetSpeedUpFactor() )', not the outer combat check.
		if ( GetSpeedUpFactor( ) )
			pSoldier->animationPlayback().delay() = (INT16)((FLOAT)pSoldier->animationPlayback().delay() * GetSpeedUpFactor( ));
		else
			pSoldier->animationPlayback().delay() = 0;
	}
}


void CheckForFullStructures( TacticalActor *pSoldier )
{
	// This function checks to see if we are near a specific structure type which requires us to blit a
	// small obscuring peice
	INT32 sGridNo;
	UINT16 usFullTileIndex;
	INT32		cnt;
	SoldierFrontArcComponent& frontArc = pSoldier->frontArc();


	// Check in all 'Above' directions
	for ( cnt = 0; cnt < MAX_FULLTILE_DIRECTIONS; cnt++ )
	{
		sGridNo = pSoldier->position().gridNo() + gsFullTileDirections[cnt];

		if ( CheckForFullStruct( sGridNo, &usFullTileIndex ) )
		{
			// Add one for the item's obsuring part
			frontArc.bindOccluder(
				static_cast<UINT8>(cnt), usFullTileIndex + 1, sGridNo);
			AddTopmostToHead(
				sGridNo, frontArc.tileIndex(static_cast<UINT8>(cnt)));
		}
		else
		{
			if ( frontArc.hasOccluder(static_cast<UINT8>(cnt)) )
			{
				RemoveTopmost(
					frontArc.gridNo(static_cast<UINT8>(cnt)),
					frontArc.tileIndex(static_cast<UINT8>(cnt)));
			}
			frontArc.clearOccluder(static_cast<UINT8>(cnt));
		}
	}

}


BOOLEAN CheckForFullStruct( INT32 sGridNo, UINT16 *pusIndex )
{
	LEVELNODE	*pStruct = NULL;
	LEVELNODE	*pOldStruct = NULL;
	UINT32				fTileFlags;

	pStruct = GetMapElement(sGridNo).pStructHead;

	// Look through all structs and Search for type

	while ( pStruct != NULL )
	{

		if ( pStruct->usIndex != NO_TILE && pStruct->usIndex < giNumberOfTiles )
		{

			GetTileFlags( pStruct->usIndex, &fTileFlags );

			// Advance to next
			pOldStruct = pStruct;
			pStruct = pStruct->pNext;

			//if( (pOldStruct->pStructureData!=NULL) && ( pOldStruct->pStructureData->fFlags&STRUCTURE_TREE ) )
			if ( fTileFlags & FULL3D_TILE )
			{
				// CHECK IF THIS TREE IS FAIRLY ALONE!
				if ( FullStructAlone( sGridNo, 2 ) )
				{
					// Return true and return index
					*pusIndex = pOldStruct->usIndex;
					return(TRUE);
				}
				else
				{
					return(FALSE);
				}

			}

		}
		else
		{
			// Advance to next
			pOldStruct = pStruct;
			pStruct = pStruct->pNext;
		}

	}

	// Could not find it, return FALSE
	return(FALSE);

}


BOOLEAN FullStructAlone( INT32 sGridNo, UINT8 ubRadius )
{
	INT32  sTop, sBottom;
	INT32  sLeft, sRight;
	INT32  cnt1, cnt2;
	INT32	 iNewIndex;
	INT32	 leftmost;


	// Determine start end end indicies and num rows
	sTop = ubRadius;
	sBottom = -ubRadius;
	sLeft = -ubRadius;
	sRight = ubRadius;

	for ( cnt1 = sBottom; cnt1 <= sTop; cnt1++ )
	{

		leftmost = ((sGridNo + (WORLD_COLS * cnt1)) / WORLD_COLS) * WORLD_COLS;

		for ( cnt2 = sLeft; cnt2 <= sRight; cnt2++ )
		{
			iNewIndex = sGridNo + (WORLD_COLS * cnt1) + cnt2;


			if ( iNewIndex >= 0 && iNewIndex < WORLD_MAX &&
				 iNewIndex >= leftmost && iNewIndex < (leftmost + WORLD_COLS) )
			{
				if ( iNewIndex != sGridNo )
				{
					if ( FindStructure( iNewIndex, STRUCTURE_TREE ) != NULL )
					{
						return(FALSE);
					}
				}
			}

		}
	}

	return(TRUE);
}


void AdjustForFastTurnAnimation( TacticalActor *pSoldier )
{

	// CHECK FOR FASTTURN ANIMATIONS
	// ATE: Mod: Only fastturn for OUR guys!
	if ( gAnimControl[pSoldier->animationPlayback().state()].uiFlags & ANIM_FASTTURN && pSoldier->roster().team() == gbPlayerNum && !(pSoldier->status().flags() & SOLDIER_TURNINGFROMHIT) )
	{
		if ( pSoldier->position().direction() != pSoldier->pathing().desiredDirection() )
		{
			pSoldier->animationPlayback().delay() = FAST_TURN_ANIM_SPEED;
		}
		else
		{
			SetSoldierAniSpeed( pSoldier );
			//	FreeUpNPCFromTurning( pSoldier, LOOK);
		}
	}

}

// What?  A zombie function?


PIXEL *CreateEnemyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen )
{
	PIXEL *p16BPPPalette, r16, g16, b16, usColor;
	UINT32 cnt;
	UINT32 rmod, gmod, bmod;
	UINT8	 r, g, b;

	Assert( pPalette != NULL );

	p16BPPPalette = (PIXEL *)MemAlloc( sizeof(PIXEL)* 256 );

	for ( cnt = 0; cnt < 256; ++cnt )
	{
		gmod = (pPalette[cnt].peGreen);
		bmod = (pPalette[cnt].peBlue);

		rmod = __max( rscale, (pPalette[cnt].peRed) );

		if ( fAdjustGreen )
		{
			gmod = __max( gscale, (pPalette[cnt].peGreen) );
		}

		r = (UINT8)__min( rmod, 255 );
		g = (UINT8)__min( gmod, 255 );
		b = (UINT8)__min( bmod, 255 );

#if SGP_PIXEL_DEPTH == 32
		usColor = 0xFF000000u | ((UINT32)r << 16) | ((UINT32)g << 8) | (UINT32)b;
		// Prevent creation of pure black color
		if ( ((usColor & 0x00FFFFFFu) == 0) && ((r + g + b) != 0) )
			usColor = 0xFF000001u;
#else
		if ( gusRedShift < 0 )
			r16 = ((UINT16)r >> (-gusRedShift));
		else
			r16 = ((UINT16)r << gusRedShift);

		if ( gusGreenShift < 0 )
			g16 = ((UINT16)g >> (-gusGreenShift));
		else
			g16 = ((UINT16)g << gusGreenShift);


		if ( gusBlueShift < 0 )
			b16 = ((UINT16)b >> (-gusBlueShift));
		else
			b16 = ((UINT16)b << gusBlueShift);

		// Prevent creation of pure black color
		usColor = (r16&gusRedMask) | (g16&gusGreenMask) | (b16&gusBlueMask);

		if ( (usColor == 0) && ((r + g + b) != 0) )
			usColor = 0x0001;
#endif

		p16BPPPalette[cnt] = usColor;
	}
	(void)RegisterLegacyRenderPalette(p16BPPPalette);
	return(p16BPPPalette);
}


PIXEL *CreateEnemyGreyGlow16BPPPalette( SGPPaletteEntry *pPalette, UINT32 rscale, UINT32 gscale, BOOLEAN fAdjustGreen )
{
	PIXEL *p16BPPPalette, r16, g16, b16, usColor;
	UINT32 cnt, lumin;
	UINT32 rmod, gmod, bmod;
	UINT8	 r, g, b;

	Assert( pPalette != NULL );

	p16BPPPalette = (PIXEL *)MemAlloc( sizeof(PIXEL)* 256 );

	for ( cnt = 0; cnt < 256; cnt++ )
	{
		lumin = (pPalette[cnt].peRed * 299 / 1000) + (pPalette[cnt].peGreen * 587 / 1000) + (pPalette[cnt].peBlue * 114 / 1000);
		rmod = (100 * lumin) / 256;
		gmod = (100 * lumin) / 256;
		bmod = (100 * lumin) / 256;



		rmod = __max( rscale, rmod );

		if ( fAdjustGreen )
		{
			gmod = __max( gscale, gmod );
		}


		r = (UINT8)__min( rmod, 255 );
		g = (UINT8)__min( gmod, 255 );
		b = (UINT8)__min( bmod, 255 );

#if SGP_PIXEL_DEPTH == 32
		usColor = 0xFF000000u | ((UINT32)r << 16) | ((UINT32)g << 8) | (UINT32)b;
		// Prevent creation of pure black color
		if ( ((usColor & 0x00FFFFFFu) == 0) && ((r + g + b) != 0) )
			usColor = 0xFF000001u;
#else
		if ( gusRedShift < 0 )
			r16 = ((UINT16)r >> (-gusRedShift));
		else
			r16 = ((UINT16)r << gusRedShift);

		if ( gusGreenShift < 0 )
			g16 = ((UINT16)g >> (-gusGreenShift));
		else
			g16 = ((UINT16)g << gusGreenShift);


		if ( gusBlueShift < 0 )
			b16 = ((UINT16)b >> (-gusBlueShift));
		else
			b16 = ((UINT16)b << gusBlueShift);

		// Prevent creation of pure black color
		usColor = (r16&gusRedMask) | (g16&gusGreenMask) | (b16&gusBlueMask);

		if ( (usColor == 0) && ((r + g + b) != 0) )
			usColor = 0x0001;
#endif

		p16BPPPalette[cnt] = usColor;
	}
	(void)RegisterLegacyRenderPalette(p16BPPPalette);
	return(p16BPPPalette);
}


void ContinueMercMovement( TacticalActor *pSoldier )
{
	INT16		sAPCost;
	INT32 sGridNo;

	sGridNo = pSoldier->pathing().finalDestinationGrid();

	// Can we afford this?
	if ( pSoldier->movement().continuedPathValid() )
	{
		sGridNo = pSoldier->movement().continuedPathGrid();
	}
	else
	{
		// ATE: OK, don't cancel count, so pending actions are still valid...
		pSoldier->pendingAction().resetAnimationCount();
	}

	// get a path to dest...
	if ( FindBestPath( pSoldier, sGridNo, pSoldier->position().level(), pSoldier->movement().mode(), NO_COPYROUTE, 0 ) )
	{
		sAPCost = PtsToMoveDirection( pSoldier, (UINT8)guiPathingData[0] );

		if ( EnoughPoints( pSoldier, sAPCost, 0, (BOOLEAN)(pSoldier->roster().team() == gbPlayerNum) ) )
		{
			// Acknowledge
			if ( pSoldier->roster().team() == gbPlayerNum )
			{
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_OK1 );

				// If we have a face, tell text in it to go away!
				if ( pSoldier->renderBindings().faceIndex() != -1 )
				{
					gFacesData[pSoldier->renderBindings().faceIndex()].fDisplayTextOver = FACE_ERASE_TEXT_OVER;
				}
			}

			(void)TacticalActorRouteExecution::setOutOfActionPoints(*pSoldier, false );

			SetUIBusy( pSoldier->identity().id() );

			// OK, try and get a path to out dest!
			(void)TacticalActorRouteExecution::requestPath(*pSoldier, sGridNo, pSoldier->movement().mode(), TacticalActorRouteExecution::PathOrigin::System, true);
		}
	}
}


BOOLEAN IsValidStance( TacticalActor *pSoldier, INT8 bNewStance )
{
	return pSoldier &&
		TacticalActorMobility::isValidStance(
			*pSoldier,
			pSoldier->position().direction(),
			bNewStance);
}


BOOLEAN IsValidMovementMode( TacticalActor *pSoldier, INT16 usMovementMode )
{
	// Check, if dest is prone, we can actually do this!

	// Check if we are in water?
	if ( pSoldier && TacticalActorMobility::inWater(*pSoldier) )
	{
		if ( usMovementMode == RUNNING || usMovementMode == SWATTING || usMovementMode == CRAWLING )
		{
			return(FALSE);
		}
	}

	return(TRUE);
}


void SelectMoveAnimationFromStance( TacticalActor *pSoldier )
{
	if (!pSoldier ||
		pSoldier->animationPlayback().state() >=
			NUMANIMATIONSTATES)
	{
		return;
	}

	// Determine which animation to do...depending on stance and gun in hand...
	switch ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight )
	{
	case ANIM_STAND:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  WALKING, 0, FALSE );
		break;

	case ANIM_PRONE:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  CRAWLING, 0, FALSE );
		break;

	case ANIM_CROUCH:
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  SWATTING, 0, FALSE );
		break;
	}
}

void GetActualSoldierAnimDims( TacticalActor *pSoldier, INT16 *psHeight, INT16 *psWidth )
{
	UINT16		usAnimSurface;
	ETRLEObject *pTrav;

	usAnimSurface = GetSoldierAnimationSurface( pSoldier, pSoldier->animationPlayback().state() );

	if ( usAnimSurface == INVALID_ANIMATION_SURFACE )
	{
		*psHeight = (INT16)5;
		*psWidth = (INT16)5;

		return;
	}

	if ( gAnimSurfaceDatabase[usAnimSurface].hVideoObject == NULL )
	{
		*psHeight = (INT16)5;
		*psWidth = (INT16)5;
		return;
	}

	// OK, noodle here on what we should do... If we take each frame, it will be different slightly
	// depending on the frame and the value returned here will vary thusly. However, for the
	// uses of this function, we should be able to use just the first frame...

	if ( pSoldier->animationPlayback().frame() >= gAnimSurfaceDatabase[usAnimSurface].hVideoObject->usNumberOfObjects )
	{
		//int i = 0;
		return;
	}

	pTrav = &(gAnimSurfaceDatabase[usAnimSurface].hVideoObject->pETRLEObject[pSoldier->animationPlayback().frame()]);

	*psHeight = (INT16)pTrav->usHeight;
	*psWidth = (INT16)pTrav->usWidth;
}

void GetActualSoldierAnimOffsets( TacticalActor *pSoldier, INT16 *sOffsetX, INT16 *sOffsetY )
{
	UINT16											 usAnimSurface;
	ETRLEObject *pTrav;

	usAnimSurface = GetSoldierAnimationSurface( pSoldier, pSoldier->animationPlayback().state() );

	if ( usAnimSurface == INVALID_ANIMATION_SURFACE )
	{
		*sOffsetX = (INT16)0;
		*sOffsetY = (INT16)0;

		return;
	}

	if ( gAnimSurfaceDatabase[usAnimSurface].hVideoObject == NULL )
	{
		*sOffsetX = (INT16)0;
		*sOffsetY = (INT16)0;
		return;
	}

	pTrav = &(gAnimSurfaceDatabase[usAnimSurface].hVideoObject->pETRLEObject[pSoldier->animationPlayback().frame()]);

	*sOffsetX = (INT16)pTrav->sOffsetX;
	*sOffsetY = (INT16)pTrav->sOffsetY;
}


void SetSoldierLocatorOffsets( TacticalActor *pSoldier )
{
	INT16 sHeight, sWidth;
	INT16 sOffsetX, sOffsetY;


	// OK, from our animation, get height, width
	GetActualSoldierAnimDims( pSoldier, &sHeight, &sWidth );
	GetActualSoldierAnimOffsets( pSoldier, &sOffsetX, &sOffsetY );

	// OK, here, use the difference between center of animation ( sWidth/2 ) and our offset!
	//pSoldier->uiPresentation().locatorOffsetX() = ( abs( sOffsetX ) ) - ( sWidth / 2 );

	pSoldier->renderState().setBoundingBox(sWidth, sHeight, sOffsetX, sOffsetY);

}


INT32 CheckBleeding( TacticalActor *pSoldier )
{
	INT8		bBandaged; //,savedOurTurn;
	INT32	iBlood = NOBLOOD;
	BOOLEAN bleeder = FALSE;

	if ( pSoldier->vitals().health() != 0 )
	{
		bleeder = DoesMercHaveDisability( pSoldier, HEMOPHILIAC );

		// if merc is hurt beyond the minimum required to bleed, or he's dying
		// Flugente: or if they are a hemophiliac
		if ( (pSoldier->vitals().bleeding() > MIN_BLEEDING_THRESHOLD) || pSoldier->vitals().health() < OKLIFE || bleeder )
		{
			// if he's NOT in the process of being bandaged or DOCTORed
			if ( !pSoldier->service().hasProviders() && (AnyDoctorWhoCanHealThisPatient( pSoldier, HEALABLE_EVER ) == NULL) )
			{
				// may drop blood whether or not any bleeding takes place this turn
				if ( !pSoldier->movementMetrics().movedThisTurn() )
				{
					iBlood = max(0, ((pSoldier->vitals().bleeding() - MIN_BLEEDING_THRESHOLD) / BLOODDIVISOR) ); // + pSoldier->dying;
					if ( iBlood > MAXBLOODQUANTITY )
					{
						iBlood = MAXBLOODQUANTITY;
					}
				}
				else
				{
					iBlood = NOBLOOD;
				}

				// Flugente: bleeders, well, bleed
				if ( bleeder )
					iBlood = min( 1, iBlood );

				// Are we in a different mode?
				if ( !(IsJa2TacticalTurnBased()) || !(IsJa2TacticalCombatActive()) )
				{
					pSoldier->vitals().nextBleedAt() -= (FLOAT)RT_NEXT_BLEED_MODIFIER;
				}
				else
				{
					// Do a single step descrease
					pSoldier->vitals().nextBleedAt()--;
				}

				// if it's time to lose some blood
				if ( pSoldier->vitals().nextBleedAt() <= 0 )
				{
					// first, calculate if soldier is bandaged
					bBandaged = pSoldier->vitals().maximumHealth() - pSoldier->vitals().bleeding() - pSoldier->vitals().health();

					// as long as he's bandaged and not "dying"
					if ( bBandaged && pSoldier->vitals().health() >= OKLIFE )
					{
						// just bleeding through existing bandages
						pSoldier->vitals().bleeding()++;

						SoldierBleed( pSoldier, TRUE );
					}
					else	// soldier is either not bandaged at all or is dying
					{
						if ( pSoldier->vitals().health() < OKLIFE )		// if he's dying
						{
							// if he's conscious, and he hasn't already, say his "dying quote"
							if ( (pSoldier->vitals().health() >= CONSCIOUSNESS) && !pSoldier->dialogue().hasMadeDyingComment() )
							{
								TacticalCharacterDialogue( pSoldier, QUOTE_SERIOUSLY_WOUNDED );

								pSoldier->dialogue().markDyingCommentSpoken();
							}

							// can't permit lifemax to ever bleed beneath OKLIFE, or that
							// soldier might as well be dead!
							if ( pSoldier->vitals().maximumHealth() >= OKLIFE )
							{
								// Flugente: reduce PERMANENT points of life only if through 'normal' bleeding, not by poisoning
								// problem is that this function applies every bleeding cycle, while loosing points through natural restoration (too much poison in body) only happens every hour.
								// so one might lose 1pt of life through poisoning at 8:00, and then lose 30 points of life PERMANTENLY in the following hour without dying
								// We bypass this by only allowing PERMANTENT lifeloss if really bleeding
								if ( pSoldier->vitals().bleeding() )
								{
									// bleeding while "dying" costs a PERMANENT point of life each time!
									pSoldier->vitals().maximumHealth()--;
									pSoldier->vitals().bleeding() = max( 0, pSoldier->vitals().bleeding() - 1 );

									if ( pSoldier->vitals().healableInjury() >= 100 ) // added check for insta-healable injury - SANDRO
										pSoldier->vitals().healableInjury() -= 100;
								}
							}
						}
					}

					// either way, a point of life (health) is lost because of bleeding
					if ( pSoldier->vitals().bleeding() )
					{
						// This will also update the life bar
						SoldierBleed( pSoldier, FALSE );
					}
					else
					{
						// just to update everything, like going unconscious or dying
						TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 0, 0, TAKE_DAMAGE_BLOODLOSS, NOBODY, NOWHERE, 0, TRUE );
					}


					// if he's not dying (which includes him saying the dying quote just
					// now), and he hasn't warned us that he's bleeding yet, he does so
					// Also, not if they are being bandaged....
					if ( (pSoldier->vitals().health() >= OKLIFE) && !pSoldier->dialogue().hasMadeDyingComment() && !pSoldier->dialogue().hasWarnedAboutBleeding() && !gTacticalStatus.fAutoBandageMode && !pSoldier->service().hasProviders() )
					{
						TacticalCharacterDialogue( pSoldier, QUOTE_STARTING_TO_BLEED );

						// "starting to bleed" quote
						pSoldier->dialogue().markBleedingWarningSpoken();
					}

					pSoldier->vitals().nextBleedAt() = CalcSoldierNextBleed( pSoldier );
				}
			}
		}
	}
	return(iBlood);
}


void SoldierBleed( TacticalActor *pSoldier, BOOLEAN fBandagedBleed )
{
	// OK, here make some stuff happen for bleeding
	// A banaged bleed does not show damage taken , just through existing bandages

	// ATE: Do this ONLY if buddy is in sector.....
	if ( (pSoldier->roster().inSector() && GetCurrentScreen() == GAME_SCREEN) || GetCurrentScreen() != GAME_SCREEN )
	{
		pSoldier->uiPresentation().startPortraitFlash();
		pSoldier->uiPresentation().portraitFlashFrame() = FLASH_PORTRAIT_STARTSHADE;
		pSoldier->timing().start(SoldierTimingComponent::Timer::PortraitFlash, FLASH_PORTRAIT_DELAY);

		// If we are in mapscreen, set this person as selected
		if ( GetCurrentScreen() == MAP_SCREEN )
		{
			SetInfoChar( pSoldier->identity().id() );
		}
	}

	// If we are already dead, don't show damage!
	if ( !fBandagedBleed )
	{
		// SANDRO - if the soldier is bleeding out, consider this damage as done by the last attacker
		if ( pSoldier->combatResult().currentAttacker() != NOBODY )
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().currentAttacker(), NOWHERE, 0, TRUE );
		else if ( pSoldier->combatResult().previousAttacker() != NOBODY )
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().previousAttacker(), NOWHERE, 0, TRUE );
		else if ( pSoldier->combatResult().earlierAttacker() != NOBODY )
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, pSoldier->combatResult().earlierAttacker(), NOWHERE, 0, TRUE );
		else
			TacticalActorDamageResolution::takeDamage(*pSoldier,  ANIM_CROUCH, 1, 100, TAKE_DAMAGE_BLOODLOSS, NOBODY, NOWHERE, 0, TRUE );
	}
}


FLOAT CalcSoldierNextBleed( TacticalActor *pSoldier )
{
	// calculate how many turns before he bleeds again
	// bleeding faster the lower life gets, and if merc is running around
	//pSoldier->nextbleed = 2 + (pSoldier->life / (10 + pSoldier->tilesMoved));  // min = 2

	// if bandaged, give 1/2 of the bandaged life points back into equation
	INT8 bBandaged = pSoldier->vitals().maximumHealth() - pSoldier->vitals().health() - pSoldier->vitals().bleeding();

	FLOAT val = 1.0f;

	// Flugente: hemophiliacs bleed a lot faster
	if ( DoesMercHaveDisability( pSoldier, HEMOPHILIAC ) )
		val += ((FLOAT)(pSoldier->vitals().health()) /
			(FLOAT)(30 + 2 * pSoldier->movementMetrics().tilesMoved()));
	else
		val += ((FLOAT)(pSoldier->vitals().health() + bBandaged / 2) /
			(FLOAT)(10 + pSoldier->movementMetrics().tilesMoved()));

	return val;
}

FLOAT CalcSoldierNextUnmovingBleed( TacticalActor *pSoldier )
{
	INT8		bBandaged;

	// calculate bleeding rate without the penalty for tiles moved

	// if bandaged, give 1/2 of the bandaged life points back into equation
	bBandaged = pSoldier->vitals().maximumHealth() - pSoldier->vitals().health() - pSoldier->vitals().bleeding();

	return((FLOAT)1 + (FLOAT)((pSoldier->vitals().health() + bBandaged / 2) / 10));  // min = 1
}

void HandlePlacingRoofMarker( TacticalActor *pSoldier, INT32 sGridNo, BOOLEAN fSet, BOOLEAN fForce )
{
	LEVELNODE *pRoofNode;
	LEVELNODE *pNode;

	if ( pSoldier->awareness().visibility() == -1 && fSet )
	{
		return;
	}

	//CHRISL: If sGridNo is -1, which can be the case if there is a dead merc still listed as part of a unit, crashes will occur
	if ( sGridNo == -1 )
		return;

	if ( pSoldier->roster().team() != gbPlayerNum )
	{
		//return;
	}

	// If we are on the roof, add roof UI peice!
	if ( pSoldier->position().level() == SECOND_LEVEL )
	{
		MAP_ELEMENT& mapElement = GetMapElement(sGridNo);
		// Get roof node
		pRoofNode = mapElement.pRoofHead;

		// Return if we are still climbing roof....
		if ( pSoldier->animationPlayback().state() == CLIMBUPROOF && !fForce )
		{
			return;
		}

		if ( pSoldier->animationPlayback().state() == JUMPUPWALL && !fForce )
		{
			return;
		}

		if ( pRoofNode != NULL )
		{
			if ( fSet )
			{
				if ( mapElement.uiFlags & MAPELEMENT_REVEALED )
				{
					// Set some flags on this poor thing
					//pRoofNode->uiFlags |= ( LEVELNODE_USEBESTTRANSTYPE | LEVELNODE_REVEAL | LEVELNODE_DYNAMIC  );
					//pRoofNode->uiFlags |= ( LEVELNODE_DYNAMIC );
					//pRoofNode->uiFlags &= ( ~LEVELNODE_HIDDEN );
					//ResetSpecificLayerOptimizing( TILES_DYNAMIC_ROOF );
				}
			}
			else
			{
				if ( mapElement.uiFlags & MAPELEMENT_REVEALED )
				{
					// Remove some flags on this poor thing
					//pRoofNode->uiFlags &= ~( LEVELNODE_USEBESTTRANSTYPE | LEVELNODE_REVEAL | LEVELNODE_DYNAMIC );

					//pRoofNode->uiFlags |= LEVELNODE_HIDDEN;
				}
			}

			if ( fSet )
			{
				// If it does not exist already....
				if ( !IndexExistsInRoofLayer( sGridNo, FIRSTPOINTERS11 ) )
				{
					pNode = AddRoofToTail( sGridNo, FIRSTPOINTERS11 );
					pNode->ubShadeLevel = DEFAULT_SHADE_LEVEL;
					pNode->ubNaturalShadeLevel = DEFAULT_SHADE_LEVEL;
				}
			}
			else
			{
				RemoveRoof( sGridNo, FIRSTPOINTERS11 );
			}
		}
	}
}

void PickPickupAnimation( TacticalActor *pSoldier, INT32 iItemIndex, INT32 sGridNo, INT8 bZLevel )
{
	INT8				bDirection;
	STRUCTURE		*pStructure;
	BOOLEAN			fDoNormalPickup = TRUE;
	// OK, Given the gridno, determine if it's the same one or different....
	if ( sGridNo != pSoldier->position().gridNo() )
	{
		// Get direction to face....
		bDirection = (INT8)GetDirectionFromGridNo( sGridNo, pSoldier );
		pSoldier->animationIntent().pendingDirection() = bDirection;

		// Change to pickup animation
		// SANDRO - determine which animation to choose, if we pickup item from struct, we can either stand or be crouched
		// when picking items from lying soldier (collapsed maybe), we need to be crouched always
		{
			if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_CROUCH || gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE )
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  ADJACENT_GET_ITEM_CROUCHED, 0, FALSE );
			else
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  ADJACENT_GET_ITEM, 0, FALSE );
		}

		if ( !(pSoldier->status().flags() & SOLDIER_PC) )
		{
			// set "pending action" value for AI so it will wait
			pSoldier->aiPlanning().action() = AI_ACTION_PENDING_ACTION;
		}

	}
	else
	{
		// If in water....
		if ( TacticalActorMobility::inWater(*pSoldier) )
		{
			UnSetUIBusy( pSoldier->identity().id() );
			HandleSoldierPickupItem( pSoldier, iItemIndex, sGridNo, bZLevel );
			pSoldier->pendingAction().clearAction();
			(void)TacticalActorRouteExecution::settleIntoStationaryStance(*pSoldier);
			if ( !(pSoldier->status().flags() & SOLDIER_PC) )
			{
				// reset action value for AI because we're done!
				ActionDone( pSoldier );
			}

		}
		else
		{
			// Don't show animation of getting item, if we are not standing
			switch ( gAnimControl[pSoldier->animationPlayback().state()].ubHeight )
			{
			case ANIM_STAND:

				// OK, if we are looking at z-level >0, AND
				// we have a strucxture with items in it
				// look for orientation and use angle accordingly....
				if ( bZLevel > 0 )
				{
					//#if 0
					// Get direction to face....
					if ( (pStructure = FindStructure( sGridNo, (STRUCTURE_HASITEMONTOP | STRUCTURE_OPENABLE) )) != NULL )
					{
						fDoNormalPickup = FALSE;

						// OK, look at orientation
						switch ( pStructure->ubWallOrientation )
						{
						case OUTSIDE_TOP_LEFT:
						case INSIDE_TOP_LEFT:

							bDirection = (INT8)NORTH;
							break;

						case OUTSIDE_TOP_RIGHT:
						case INSIDE_TOP_RIGHT:

							bDirection = (INT8)WEST;
							break;

						default:

							bDirection = pSoldier->position().direction();
							break;
						}

						//pSoldier->animationIntent().pendingDirection() = bDirection;
						(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, bDirection );
						(void)TacticalActorOrientation::setDirection(*pSoldier, bDirection );

						// Change to pickup animation
						TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  ADJACENT_GET_ITEM, 0, FALSE );
					}
					//#endif
				}

				if ( fDoNormalPickup )
				{
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  PICKUP_ITEM, 0, FALSE );
				}

				if ( !(pSoldier->status().flags() & SOLDIER_PC) )
				{
					// set "pending action" value for AI so it will wait
					pSoldier->aiPlanning().action() = AI_ACTION_PENDING_ACTION;
				}
				break;

			case ANIM_CROUCH:
			case ANIM_PRONE:

				UnSetUIBusy( pSoldier->identity().id() );
				HandleSoldierPickupItem( pSoldier, iItemIndex, sGridNo, bZLevel );
				pSoldier->pendingAction().clearAction();
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(*pSoldier);
				if ( !(pSoldier->status().flags() & SOLDIER_PC) )
				{
					// reset action value for AI because we're done!
					ActionDone( pSoldier );
				}
				break;
			}
		}
	}
}

BOOLEAN MercStealFromMerc( TacticalActor *pSoldier, TacticalActor *pTarget )
{
	INT32 sActionGridNo, sGridNo, sAdjustedGridNo;
	UINT8	ubDirection;

	if ( pSoldier == NULL || pTarget == NULL || pSoldier == pTarget )
	{
		return FALSE;
	}

	// OK, find an adjacent gridno....
	sGridNo = pTarget->position().gridNo();

	// See if we can get there to punch
	sActionGridNo = FindAdjacentGridEx( pSoldier, sGridNo, &ubDirection, &sAdjustedGridNo, TRUE, FALSE );
	if ( sActionGridNo != -1 )
	{
		const INT16 sAPCost =
			GetAPsToStealItem( pSoldier, pTarget, (INT16)sActionGridNo );
		if ( !EnoughPoints( pSoldier, sAPCost, 0, FALSE ) )
		{
			return FALSE;
		}

		// SEND PENDING ACTION
		pSoldier->pendingAction().begin(MERC_STEAL);
		pSoldier->targeting().level() = pTarget->position().level(); // Overhaul:  Update the level too!
		pSoldier->pendingAction().primaryData() = pTarget->identity().id();
		pSoldier->pendingAction().secondaryData() = pTarget->position().gridNo();
		pSoldier->pendingAction().tertiaryData() = ubDirection;
		pSoldier->pendingAction().quaternaryData() = 0;
		pSoldier->runtime().pendingAction.targetIncarnation =
			pTarget->identity().incarnation();
		pSoldier->pendingAction().resetAnimationCount();

		// CHECK IF WE ARE AT THIS GRIDNO NOW
		if ( pSoldier->position().gridNo() != sActionGridNo )
		{
			// WALK UP TO DEST FIRST
			SendGetNewSoldierPathEvent( pSoldier, sActionGridNo, pSoldier->movement().mode() );
		}
		else
		{
			if ( !TryCompletePendingStealCommand( *pSoldier ) )
			{
				return FALSE;
			}
		}

		// OK, set UI
		//		GetJa2PendingTacticalCombatActions()++;
		// reset attacking item (hand)
		pSoldier->attackSelection().weapon() = 0;
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "!!!!!!! Starting STEAL attack, attack count now %d", GetJa2PendingTacticalCombatActions() ) );
		DebugAttackBusy( String( "!!!!!!! Starting STEAL attack, attack count now %d\n", GetJa2PendingTacticalCombatActions() ) );

		SetUIBusy( pSoldier->identity().id() );
		return TRUE;
	}

	return FALSE;
}

void HandleSystemNewAISituation( TacticalActor *pSoldier, BOOLEAN fResetABC )
{
	// Are we an AI guy?
	// 0verhaul:
	// This code will only stop a soldier if it is not the player's turn.  The problem here is that the soldier's
	// actions may have triggered an interrupt.  This code is called in order to cancel the soldier's movement 
	// after the interrupt is triggered, so if the AI causes an interrupt and it's the player's turn, he will
	// continue doing what he was going to do.  We need this function to work even when it's the player's turn,
	// at least in this case.
	//if ( GetJa2TacticalCurrentTeam() != gbPlayerNum && pSoldier->roster().team() != gbPlayerNum )
	{
		if ( pSoldier->aiBehavior().newSituation() == IS_NEW_SITUATION )
		{
			// Cancel what they were doing....
			// silversurfer: bugfix for endless dying mercs on roof edges
			// If we delete their pending animation here, turn advancement will still face them for the fall.
			// and stand there forever afterwards in "dying" state, so let this guy fall off the roof first!
			if ( pSoldier->animationIntent().pendingAnimation() != FALLOFF && pSoldier->animationIntent().pendingAnimation() != FALLFORWARD_ROOF )
				pSoldier->animationIntent().clearPendingAnimation();
			pSoldier->animationIntent().clearSecondaryPendingAnimation();
			pSoldier->animationActivity().turningFromProneMode() = FALSE;
			pSoldier->animationIntent().clearPendingDirection();
			pSoldier->pendingAction().clearAction();
			pSoldier->schedule().cancelDoorContinuation();

			// if this guy isn't under direct AI control, WHO GIVES A FLYING FLICK?
			if ( pSoldier->status().flags() & SOLDIER_UNDERAICONTROL )
			{
				if ( pSoldier->animationActivity().turningToShoot() )
				{
					pSoldier->animationActivity().turningToShoot() = FALSE;
					// Release attacker
					// OK - this is hightly annoying , but due to the huge combinations of
					// things that can happen - 1 of them is that sLastTarget will get unset
					// after turn is done - so set flag here to tell it not to...
					pSoldier->targeting().retainLastTargetFromTurn() = TRUE;
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something: DONE IN SYSTEM NEW SITUATION" ) );
					DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending fire because saw something: DONE IN SYSTEM NEW SITUATION\n" );
					FreeUpAttacker( );
				}

				if ( pSoldier->pendingItem().hasObject() )
				{
					// Place it back into inv....
					AutoPlaceObject( pSoldier, pSoldier->pendingItem().object(), FALSE );
					pSoldier->pendingItem().clearThrowTransaction();
					pSoldier->animationIntent().clearPendingAnimations();

					// Decrement attack counter...
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Reducing attacker busy count..., ending throw because saw something: DONE IN SYSTEM NEW SITUATION" ) );
					DebugAttackBusy( "@@@@@@@ Reducing attacker busy count..., ending throw because saw something: DONE IN SYSTEM NEW SITUATION\n" );
					FreeUpAttacker( );
				}

			}
		}
	}
}

void InternalPlaySoldierFootstepSound( TacticalActor * pSoldier )
{
	UINT8					ubRandomSnd;
	INT8					bVolume = MIDVOLUME;
	// Assume outside
	UINT32					ubSoundBase = WALK_LEFT_OUT;
	UINT8					ubRandomMax = 4;

	// Determine if we are on the floor
	if ( !(pSoldier->status().flags() & SOLDIER_VEHICLE) )
	{
		if ( pSoldier->animationPlayback().state() == HOPFENCE || pSoldier->animationPlayback().state() == JUMPWINDOWS )
		{
			bVolume = HIGHVOLUME;
		}

		if ( pSoldier->status().flags() & SOLDIER_ROBOT )
		{
			PlaySoldierJA2Sample( pSoldier->identity().id(), ROBOT_BEEP, RATE_11025, SoundVolume( bVolume, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
			return;
		}

		//if ( SoldierOnScreen( pSoldier->identity().id() ) )
		{
			if ( pSoldier->animationPlayback().state() == CRAWLING )
			{
				ubSoundBase = CRAWL_1;
			}
			else
			{
				// Pick base based on terrain over....
				if ( pSoldier->position().terrainType() == FLAT_FLOOR )
				{
					ubSoundBase = WALK_LEFT_IN;
				}
				else if ( pSoldier->position().terrainType() == DIRT_ROAD || pSoldier->position().terrainType() == PAVED_ROAD )
				{
					ubSoundBase = WALK_LEFT_ROAD;
				}
				else if ( TacticalActorMobility::inShallowWater(*pSoldier) )
				{
					ubSoundBase = WATER_WALK1_IN;
					ubRandomMax = 2;
				}
				else if ( TacticalActorMobility::inDeepWater(*pSoldier) )
				{
					ubSoundBase = SWIM_1;
					ubRandomMax = 2;
				}
			}

			// Pick a random sound...
			do
			{
				ubRandomSnd = (UINT8)Random( ubRandomMax );

			} while ( ubRandomSnd == pSoldier->audio().lastFootstepVariant() );

			pSoldier->audio().recordFootstepVariant(ubRandomSnd);

			// OK, if in realtime, don't play at full volume, because too many people walking around
			// sounds don't sound good - ( unless we are the selected guy, then always play at reg volume )
			if ( !(IsJa2TacticalCombatActive()) && (pSoldier->identity().id() != gusSelectedSoldier) )
			{
				bVolume = LOWVOLUME;
			}

			PlaySoldierJA2Sample( pSoldier->identity().id(),
				ubSoundBase + pSoldier->audio().lastFootstepVariant(),
				RATE_11025, SoundVolume( bVolume, pSoldier->position().gridNo() ),
				1, SoundDir( pSoldier->position().gridNo() ), TRUE );
		}
	}
	else
	{
		// anv: vehicle sounds
		//PlaySoldierJA2Sample( pSoldier->identity().id(), S_VECH1_MOVE, RATE_11025, SoundVolume( bVolume, pSoldier->sGridNo ), 1, SoundDir( pSoldier->sGridNo ), TRUE );
		if ( pSoldier->animationPlayback().state() == RUNNING )
		{
			bVolume = HIGHVOLUME;
		}

		if ( pVehicleList )
			PlaySoldierJA2Sample( pSoldier->identity().id(), pVehicleList[pSoldier->vehicleState().tacticalVehicleId()].iMoveSound, RATE_11025, SoundVolume( bVolume, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
	}
}

void PlaySoldierFootstepSound( TacticalActor *pSoldier )
{
	// normally, not in stealth mode
	if ( !pSoldier->movement().stealthMode() )
	{
		InternalPlaySoldierFootstepSound( pSoldier );
	}
}

void PlayStealthySoldierFootstepSound( TacticalActor *pSoldier )
{
	// even if in stealth mode
	InternalPlaySoldierFootstepSound( pSoldier );
}



BOOLEAN DoesSoldierWearGasMask( TacticalActor *pSoldier )//dnl ch40 200909
{
	INT8 bPosOfMask = FindGasMask( pSoldier );

	if ( (bPosOfMask == HEAD1POS || bPosOfMask == HEAD2POS) && pSoldier->inventory()[bPosOfMask][0]->data.objectStatus >= USABLE )
		return(TRUE);
	return(FALSE);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SANDRO - added following functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN HAS_SKILL_TRAIT( TacticalActor * pSoldier, UINT8 uiSkillTraitNumber )
{
	if ( pSoldier == NULL )
		return FALSE;

	// Flugente: compatibility with skills
	if ( uiSkillTraitNumber == INTEL || uiSkillTraitNumber == DISGUISE || uiSkillTraitNumber == VARIOUSSKILLS )
		return TRUE;

	// sevenfm: add Autobandage option to skills menu
	if (uiSkillTraitNumber == AUTOBANDAGESKILLS)
	{
		return CheckAutoBandage();
	}

	INT8 bNumMajorTraitsCounted = 0;
	INT8 bMaxTraits = gSkillTraitValues.ubMaxNumberOfTraits;
	INT8 bMaxMajorTraits = gSkillTraitValues.ubNumberOfMajorTraitsAllowed;

	// check old/new traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// exception for special merc
		//if ( gSkillTraitValues.fAllowSpecialMercTraitsException && pSoldier->identity().profile() == gSkillTraitValues.ubSpecialMercID)
		//{
		//	bMaxTraits++;
		//	bMaxMajorTraits++;
		//}

		for ( INT8 bCnt = 0; bCnt < min( 30, bMaxTraits ); ++bCnt )
		{
			if ( pSoldier->statistics().skillTrait(bCnt) == uiSkillTraitNumber )
				return(TRUE);

			if ( MajorTrait( pSoldier->statistics().skillTrait(bCnt) ) )
				++bNumMajorTraitsCounted;

			// if we exceeded the allowed number of major traits, ignore the rest of them
			if ( bNumMajorTraitsCounted > min( 20, bMaxMajorTraits ) )
				break;
		}
	}
	else
	{
		if ( pSoldier->statistics().skillTrait(0) == uiSkillTraitNumber )
			return(TRUE);

		if ( pSoldier->statistics().skillTrait(1) == uiSkillTraitNumber )
			return(TRUE);
	}

	return(FALSE);
}

INT8 NUM_SKILL_TRAITS( TacticalActor * pSoldier, UINT8 uiSkillTraitNumber )
{
	if ( pSoldier == NULL )
		return(0);

	INT8 bNumberOfTraits = 0;
	INT8 bNumMajorTraitsCounted = 0;
	INT8 bMaxTraits = gSkillTraitValues.ubMaxNumberOfTraits;
	INT8 bMaxMajorTraits = gSkillTraitValues.ubNumberOfMajorTraitsAllowed;

	// check old/new traits
	if ( gGameOptions.fNewTraitSystem )
	{
		// exception for special merc
		//if ( gSkillTraitValues.fAllowSpecialMercTraitsException && pSoldier->identity().profile() == gSkillTraitValues.ubSpecialMercID)
		//{
		//	bMaxTraits++;
		//	bMaxMajorTraits++;
		//}

		for ( INT8 bCnt = 0; bCnt < min( 30, bMaxTraits ); ++bCnt )
		{
			if ( pSoldier->statistics().skillTrait(bCnt) == uiSkillTraitNumber )
				++bNumberOfTraits;
				
			if ( MajorTrait( pSoldier->statistics().skillTrait(bCnt) ) )
				++bNumMajorTraitsCounted;

			// if we exceeded the allowed number of major traits, ignore the rest of them
			if ( bNumMajorTraitsCounted > min( 20, bMaxMajorTraits ) )
				break;
		}

		// cannot have more than one same minor trait
		if ( !TwoStagedTrait( uiSkillTraitNumber ) )
			return (min( 1, bNumberOfTraits ));
		
		return (min( 2, bNumberOfTraits ));
	}
	else
	{
		if ( pSoldier->statistics().skillTrait(0) == uiSkillTraitNumber )
			++bNumberOfTraits;

		if ( pSoldier->statistics().skillTrait(1) == uiSkillTraitNumber )
			++bNumberOfTraits;

		// Electronics, Ambidextrous and Camouflaged can only be of one grade
		if ( uiSkillTraitNumber == ELECTRONICS_OT ||
			 uiSkillTraitNumber == AMBIDEXT_OT ||
			 uiSkillTraitNumber == CAMOUFLAGED_OT )
			 return (min( 1, bNumberOfTraits ));

		return (bNumberOfTraits);
	}
}

UINT8 GetSquadleadersCountInVicinity( TacticalActor * pSoldier, BOOLEAN fWithHigherLevel, BOOLEAN fDontCheckDistance )
{
	UINT8 ubNumberSL = 0;

	// loop through all soldiers around
	for ( SoldierID cnt = gTacticalStatus.Team[pSoldier->roster().team()].bFirstID; cnt <= gTacticalStatus.Team[pSoldier->roster().team()].bLastID; ++cnt )
	{
		TacticalActor *pSquadLeader =
			GetJa2SoldierRepository().resolve(
				cnt );
		// Get active conscious soldier
		if ( pSquadLeader != nullptr && pSquadLeader != pSoldier && pSquadLeader->roster().active() &&
			 pSquadLeader->vitals().health() >= OKLIFE && HAS_SKILL_TRAIT( pSquadLeader, SQUADLEADER_NT ) )
		{
			// check if within distance
			// if both have extended ear, the distance is bigger and they don't need to sea each other 
			// note that enemy always get the bonus if within distance, regardless of extended ears
			// Flugente: moved around arguments for speed reason
			if ( fDontCheckDistance ||
				 (PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusNormal) ||
				 ((pSoldier->roster().team() == ENEMY_TEAM || (HasExtendedEarOn( pSoldier ) && HasExtendedEarOn( pSquadLeader ))) && PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusExtendedEar)
				 )
			{
				// If checking for higher level SL
				// also count in already aquired level increses from other SLs
				if ( fWithHigherLevel )
				{
					if ( pSquadLeader->statistics().experienceLevel() > (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius)) )
						ubNumberSL += min( (max( 0, (pSquadLeader->statistics().experienceLevel() - (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius))) )), (NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT )) );
				}
				else
				{
					ubNumberSL += NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT );
				}

				if ( ubNumberSL >= gSkillTraitValues.ubSLMaxBonuses )
					break;
			}
		}
	}

	// special loop for militia - they can get a bonus from our mercs
	if ( pSoldier->roster().team() == MILITIA_TEAM && ubNumberSL < gSkillTraitValues.ubSLMaxBonuses )
	{
		for ( SoldierID cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID; cnt <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++cnt )
		{
			TacticalActor *pSquadLeader =
				GetJa2SoldierRepository().resolve(
					cnt );
			// Get active conscious soldier
			if ( pSquadLeader != nullptr && pSquadLeader != pSoldier && pSquadLeader->roster().active() &&
				 pSquadLeader->vitals().health() >= OKLIFE && HAS_SKILL_TRAIT( pSquadLeader, SQUADLEADER_NT ) )
			{
				// check if within distance
				// Flugente: moved around arguments for speed reason
				if ( fDontCheckDistance ||
					 (PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusNormal) ||
					 ((HasExtendedEarOn( pSquadLeader ) && PythSpacesAway( pSoldier->position().gridNo(), pSquadLeader->position().gridNo() ) <= gSkillTraitValues.usSLRadiusExtendedEar))
					 )
				{
					// If checking for higher level SL
					// also count in already aquired level increses from other SLs
					if ( fWithHigherLevel )
					{
						if ( pSquadLeader->statistics().experienceLevel() > (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius)) )
							ubNumberSL += min( (max( 0, (pSquadLeader->statistics().experienceLevel() - (pSoldier->statistics().experienceLevel() + (ubNumberSL*gSkillTraitValues.ubSLEffectiveLevelInRadius))) )), (NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT )) );
					}
					else
					{
						ubNumberSL += NUM_SKILL_TRAITS( pSquadLeader, SQUADLEADER_NT );
					}

					if ( ubNumberSL >= gSkillTraitValues.ubSLMaxBonuses )
						break;
				}
			}
		}
	}

	// 3 bonuses are a max by default
	return(min( gSkillTraitValues.ubSLMaxBonuses, ubNumberSL ));
}


////////////////////////////////////////////////////////////////////////////////////////////
// SANDRO - Improved Interrupt System
/////////////////////////////////////////
BOOLEAN ResolvePendingInterrupt( TacticalActor * pSoldier, UINT8 ubInterruptType )
{
	// real time or not in combat? disable and clear
	if ( !(IsJa2TacticalTurnBased()) ||
		 !(IsJa2TacticalCombatActive()) )
	{
		gTacticalStatus.ubInterruptPending = DISABLED_INTERRUPT;
		ClearIntList( );
		return(FALSE);
	}

	// invalid guy
	if ( pSoldier == NULL )
	{
		//ClearIntList();
		return(FALSE);
	}

	// can't be interrupted if it's not our turn at all
	if ( GetJa2TacticalCurrentTeam() != pSoldier->roster().team() )
	{
		return(FALSE);
	}

	// no interrupt called or not gonna trigger it now
	if ( gTacticalStatus.ubInterruptPending == DISABLED_INTERRUPT ||
		 gTacticalStatus.ubInterruptPending == UNTRIGGERED_INTERRUPT )
	{
		return(FALSE);
	}

	// if the interrupt called match the type we are trying to resolve..
	if ( gTacticalStatus.ubInterruptPending == ubInterruptType || ubInterruptType == INSTANT_INTERRUPT )
	{
		/////////////////////////////
		// Gather all interrupters //
		/////////////////////////////
		TacticalActor *pInterrupter;
		UINT8 ubInterruptersFound = 0;
		UINT16 ubaInterruptersList[64];
		UINT16 uCnt = 0, uiReactionTime;
		INT16 iInjuryPenalty;

		for ( uCnt = 0; uCnt < MAX_NUM_SOLDIERS; uCnt++ )
		{
			// first find all guys who can see us
			pInterrupter =
				GetJa2SoldierRepository().resolve( uCnt );
			if ( pInterrupter == NULL )
				continue;			// not valid
			if (pInterrupter->vitals().health() < OKLIFE || pInterrupter->collapseState().tactical() || !pInterrupter->roster().active() || !pInterrupter->roster().inSector() || pInterrupter->actionPoints().current() < 4)
				continue;			// not active
			if (pInterrupter->vitals().breath() < OKBREATH && pInterrupter->roster().team() != OUR_TEAM)
				continue;			// BOB: prevent NPCs from getting interrupts when out of breath
			if ( pSoldier->roster().team() == pInterrupter->roster().team() )
				continue;			// same team
			if ( pSoldier->roster().side() == pInterrupter->roster().side() )
				continue;			// not enemy
			if ( CONSIDERED_NEUTRAL( pSoldier, pInterrupter ) )
				continue;			// neutral
			if ( CONSIDERED_NEUTRAL( pInterrupter, pSoldier ) )
				continue;			// neutral

			/////////////////////////////////////////////////////////////
			// Calculate Reaction Time (i.e. interrupt counter length) //
			/////////////////////////////////////////////////////////////

			// set base value ( interrupt per every X APs an enemy uses )
			// if not seen but just heard... we interrupt only if they attack us (or if they are very close) in that case
			if ( (pInterrupter->awareness().opponentKnowledge()[pSoldier->identity().id()] == SEEN_CURRENTLY) || (pInterrupter->awareness().opponentKnowledge()[pSoldier->identity().id()] == HEARD_THIS_TURN && (ubInterruptType == AFTERSHOT_INTERRUPT || ubInterruptType == AFTERACTION_INTERRUPT || PythSpacesAway( pInterrupter->position().gridNo(), pSoldier->position().gridNo() ) < 3)) )
			{
				uiReactionTime = gGameExternalOptions.ubBasicReactionTimeLengthIIS;
			}
			else
			{
				// not seen or not heard anything worth interrupting
				continue;
			}
			uiReactionTime = uiReactionTime * 10; // x10 ... we will divide by 10 after all adjustments done
			// adjust based on Agility
			if ( pInterrupter->statistics().agility() >= 80 )
			{
				uiReactionTime = (uiReactionTime * (100 - (2 * (pInterrupter->statistics().agility() - 80))) / 100);
			}
			else if ( pInterrupter->statistics().agility() < 80 && pInterrupter->statistics().agility() > 50 )
			{
				uiReactionTime = (uiReactionTime * (100 + (2 * (80 - pInterrupter->statistics().agility()))) / 100);
			}
			else
			{
				uiReactionTime = (uiReactionTime * 8 / 5);
			}
			// adjust based on APs left
			// at full possible APs no adjustement (100% applies), +1% length per every 2% of APs down from full
			uiReactionTime = (uiReactionTime * (100 + (50 - (50 * pInterrupter->actionPoints().current() / TacticalActorTurnBudget::calculateTurnGrant(*pInterrupter)))) / 100);
			// adjust based on injuries
			if ( pInterrupter->vitals().health() < pInterrupter->vitals().maximumHealth() )
			{
				// OK, this looks a bit complicated..
				// our HP lost minus half of the bandaged part gives us 2% longer reaction time per 1% of our health down from full health
				// this penalty is however slightly reduced by our experience level
				iInjuryPenalty = (200 * (pInterrupter->vitals().maximumHealth() - pInterrupter->vitals().health() + ((pInterrupter->vitals().maximumHealth() - pInterrupter->vitals().health() - pInterrupter->vitals().bleeding()) / 2))) / (pInterrupter->vitals().maximumHealth());
				uiReactionTime = (uiReactionTime * (100 + iInjuryPenalty * (100 - (3 * EffectiveExpLevel( pInterrupter ))) / 100) / 100);
			}

			// adjust by breath down
			if ( pSoldier->vitals().breath() < 100 )
			{
				// +1% per 2 points of breath down
				uiReactionTime = (uiReactionTime * (100 + ((100 - pSoldier->vitals().breath()) / 2)) / 100);
			}

			// adjust for getting aid, being in gas or being in shock
			if ( pInterrupter->status().flags() & SOLDIER_GASSED )
				uiReactionTime = (uiReactionTime * (100 + AIM_PENALTY_GASSED) / 100);

			if ( pInterrupter->service().hasProviders() )
				uiReactionTime = (uiReactionTime * (100 + AIM_PENALTY_GETTINGAID) / 100);

			if ( pInterrupter->suppression().shock() )
				uiReactionTime = (uiReactionTime * (100 + (pInterrupter->suppression().shock() * 20)) / 100); // this is severe, 20% per point

			// Phlegmatic characters has slightly longer reaction time			
			if ( DoesMercHavePersonality( pSoldier, CHAR_TRAIT_PHLEGMATIC ) )
			{
				uiReactionTime = ((uiReactionTime * 110) / 100);
			}

			// finally divide back by 10 to get the needed result (round properly)
			uiReactionTime = ((uiReactionTime + 5) / 10);

			/////////////////////////////////////////////
			// Check if we reached reaction time value //
			/////////////////////////////////////////////

			// if we have hit the needed amount, the actual interrupt occurs for the observer
			if ( pInterrupter->turnState().interruptCounters()[pSoldier->identity().id()] >= uiReactionTime )
			{
				///////////////////////////
				// Success! Add to list! //
				///////////////////////////

				// the soldier to be interrupted is added to the list (once only)
				if ( ubInterruptersFound == 0 )
				{
					AddToIntList( pSoldier->identity().id(), FALSE, TRUE );
				}
				if ( ubInterruptersFound < 64 )   // guard the fixed 64-entry stack buffer (OOB write)
					ubaInterruptersList[ubInterruptersFound++] = pInterrupter->identity().id();

				// add the observer who got the interrupt
				AddToIntList( pInterrupter->identity().id(), TRUE, TRUE );
				// reset the counter
				pInterrupter->turnState().interruptCounters()[pSoldier->identity().id()] = 0;
			}
		}
		if ( ubInterruptersFound > 0 )
		{
			////////////////////////////////////////////////
			// Check for possible "Collective Interrupts" //
			////////////////////////////////////////////////
			if ( gGameExternalOptions.fAllowCollectiveInterrupts )
			{
				TacticalActor *pTeammate;
				UINT16 usColIntChance = 0;
				UINT8 ubOriginalInterruptersCount = ubInterruptersFound, uCnt3 = 0;
				BOOLEAN fAlreadyIn;

				for ( uCnt = 0; uCnt < ubOriginalInterruptersCount; uCnt++ )
				{
					pInterrupter = GetJa2SoldierRepository().resolve(
						ubaInterruptersList[uCnt] );
					if ( pInterrupter == nullptr )
						continue;

					SoldierID uCnt2 = gTacticalStatus.Team[pInterrupter->roster().team()].bFirstID;
					for ( ; uCnt2 <= gTacticalStatus.Team[pInterrupter->roster().team()].bLastID; ++uCnt2 )
					{
						pTeammate =
							GetJa2SoldierRepository().resolve( uCnt2 );
						if ( pTeammate == NULL )
							continue;			// not valid
						if ( pTeammate->roster().team() != pInterrupter->roster().team() )
							continue;			// little paranoya check here
						if ( pTeammate->vitals().health() < OKLIFE || pTeammate->collapseState().tactical() || !pTeammate->roster().active() || !pTeammate->roster().inSector() || pTeammate->actionPoints().current() < 4 )
							continue;			// not active

						// check if we haven't been added to the list already
						fAlreadyIn = FALSE;
						for ( uCnt3 = 0; uCnt3 < ubInterruptersFound; uCnt3++ )
						{
							if ( pTeammate->identity().id() == ubaInterruptersList[uCnt3] )
							{
								fAlreadyIn = TRUE;
								break;
							}
						}
						// if we are close enough
						if ( !fAlreadyIn && PythSpacesAway( pInterrupter->position().gridNo(), pTeammate->position().gridNo() ) <= 5 )
						{
							// calculate the chance
							// we would have base chance 100% (if both have maxed stats)
							// 0-30% is determined by Leadership of the original interrupted - i.e. how well and if he can "inform" us
							// 0-20% is determined by his Experience Level
							// 0-20% is determined by our Experience Level - i.e how well can we realize that we must act
							// 0-20% is determined by our Agility - can our body react so swiftly at all
							// 0-10% is determined by our Wisdom - do we have enough mental agility as well?
							usColIntChance = 10 * (((pInterrupter->statistics().leadership() * 3) +
								(EffectiveExpLevel( pInterrupter ) * 20) +
								(EffectiveExpLevel( pTeammate ) * 20) +
								(pTeammate->statistics().agility() * 2) +
								(pTeammate->statistics().wisdom())) / 100);
							// add bonus per Squadleader trait of the original interrupter
							if ( HAS_SKILL_TRAIT( pInterrupter, SQUADLEADER_NT ) && gGameOptions.fNewTraitSystem )
							{
								usColIntChance += gSkillTraitValues.ubSLCollectiveInterruptsBonus * NUM_SKILL_TRAITS( pInterrupter, SQUADLEADER_NT );
							}
							if ( PreChance( usColIntChance ) )
							{
								if ( ubInterruptersFound < 64 )   // guard the fixed 64-entry stack buffer (OOB write)
									ubaInterruptersList[ubInterruptersFound++] = pTeammate->identity().id();
								// if he can react on collective interrupt, give it to him
								AddToIntList( pTeammate->identity().id(), TRUE, TRUE );
								// reset the counter for him
								pTeammate->turnState().interruptCounters()[pSoldier->identity().id()] = 0;
							}
						}
					}
				}
			}

			/////////////////////////////////////////////
			// OK, done, all interrupters added, SEND! //
			/////////////////////////////////////////////

			// remove AI control from the interrupted guy just in case may not be neccessary, but it's harmless anyway
			if ( (GetJa2TacticalCurrentTeam() != pSoldier->roster().team()) && !(gTacticalStatus.Team[GetJa2TacticalCurrentTeam()].bHuman) )
			{
				if ( pSoldier->status().flags() & SOLDIER_UNDERAICONTROL )
				{
					pSoldier->status().flags() &= (~SOLDIER_UNDERAICONTROL);
				}
			}
			// reset 
			gTacticalStatus.ubInterruptPending = DISABLED_INTERRUPT;
			// start interrupt
			DoneAddingToIntList( pSoldier, TRUE, 1 );

			return(TRUE);
		}
		else // no interrupters found, reset until next occasion
		{
			// reset 
			gTacticalStatus.ubInterruptPending = DISABLED_INTERRUPT;
		}
	}
	return(FALSE);
}

BOOLEAN AIDecideHipOrShoulderStance( TacticalActor * pSoldier, INT32 iGridNo )
{
	// TO DO: this should be much more sophisticated

	UINT16 usInHand = pSoldier->attackSelection().weapon();

	// not 2-handed or not standing 
	if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight != ANIM_STAND || !ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem) )
	{
		return FALSE;
	}
	// heavy gun only from hip if standing
	if ( Weapon[usInHand].HeavyGun )
	{
		return TRUE;
	}
	// we want to make an aimed shot
	if ( pSoldier->aiPlanning().aimTime() > GetNumberAltFireAimLevels( pSoldier, iGridNo ) )
	{
		return FALSE;
	}

	INT8 bChanceHip = 0;

	if ( pSoldier->fireControl().burstCounter() > 0 )
		bChanceHip += 25;
	if ( Weapon[usInHand].ubWeaponType == GUN_LMG )
		bChanceHip += 30;
	if ( Weapon[usInHand].ubWeaponType == GUN_SHOTGUN )
		bChanceHip += 15;

	// chance to hit with no aiming, add it to the chance to fire from hip
	if ( !TileIsOutOfBounds( iGridNo ) )
	{
		bChanceHip += CalcChanceToHitGun( pSoldier, iGridNo, 0, AIM_SHOT_TORSO );
	}

	if ( PreChance( bChanceHip ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}

}

BOOLEAN DecideAltAnimForBigMerc( TacticalActor * pSoldier )
{
	if ( pSoldier->identity().bodyType() != BIGMALE )
	{
		// WTF!
		return FALSE;
	}

	//always use the other anim for badass mercs
	if ( pSoldier->animationPlayback().subFlags() & SUB_ANIM_BIGGUYSHOOT2 )
	{
		return TRUE;
	}

	// if it is player controlled merc
	if ( pSoldier->status().flags() & SOLDIER_PC )
	{
		// are we in combat?
		if ( IsJa2TacticalCombatActive() )
		{
			// then only use it if morale is very high (we are definately winning)
			if ( pSoldier->morale().morale() > 95 )
			{
				return TRUE;
			}
		}
		else
		{
			// if not we use this with slightly above avarage morale
			if ( pSoldier->morale().morale() > 65 )
			{
				return TRUE;
			}
		}
	}
	// enemy guy
	else
	{
		//never use this for regular enemies, only elites with high morale and level can sometimes show this animation
		if ( (pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE || pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA) &&
			 (pSoldier->morale().aiMorale() >= MORALE_FEARLESS) && (pSoldier->statistics().experienceLevel() > 8) )
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN TwoStagedTrait( UINT8 uiSkillTraitNumber )
{
	if ( gGameOptions.fNewTraitSystem )
	{
		if ( uiSkillTraitNumber > 0 )
		{
			// covert ops is a major trait that is in a different location
			if ( uiSkillTraitNumber == COVERT_NT )
				return TRUE;

			// other traits below NUM_ORIGINAL_MAJOR_TRAITS are all major
			if ( uiSkillTraitNumber <= NUM_ORIGINAL_MAJOR_TRAITS )
				return TRUE;
		}
	}
	else
	{
		if ( uiSkillTraitNumber == IMP_SKILL_TRAITS__ELECTRONICS ||
			 uiSkillTraitNumber == IMP_SKILL_TRAITS__AMBIDEXTROUS ||
			 uiSkillTraitNumber == IMP_SKILL_TRAITS__CAMO )
			return(FALSE);

		return TRUE;
	}

	return FALSE;
}

// determine if this is a major trait (no longer all two-staged)
BOOLEAN MajorTrait( UINT8 uiSkillTraitNumber )
{
	if ( uiSkillTraitNumber > 0 )
	{
		// covert ops is a major trait that is in a different location
		if ( uiSkillTraitNumber == COVERT_NT )
			return TRUE;

		// other traits below NUM_ORIGINAL_MAJOR_TRAITS are all major
		if ( uiSkillTraitNumber <= NUM_ORIGINAL_MAJOR_TRAITS )
			return TRUE;
	}

	return FALSE;
}

// get overt penalty duration in AP for using an animation
UINT16	GetSuspiciousAnimationAPDuration( UINT16 usAnimation )
{
	switch ( usAnimation )
	{
	case NINJA_PUNCH:
	case NINJA_LOWKICK:
	case PUNCH_LOW:
	case CROWBAR_ATTACK:
	case DODGE_ONE:
	case SLICE:
	case STAB:
	case CROUCH_STAB:
	case BAYONET_STAB_STANDING_VS_STANDING:
	case BAYONET_STAB_STANDING_VS_PRONE:
	case PUNCH:
	case PUNCH_BREATH:
	case KICK_DOOR:
	case FOCUSED_PUNCH:
	case FOCUSED_STAB:
	case HTH_KICK:
	case FOCUSED_HTH_KICK:
		return 60; break;

	case THROW_GRENADE_STANCE:
	case LOB_GRENADE_STANCE:
	case THROW_KNIFE:
	case THROW_KNIFE_SP_BM:
	case THROW_ITEM:
	case LOB_ITEM:
	case THROW_ITEM_CROUCHED:
		return 50; break;

	case PICKUP_ITEM:
	case DROP_ITEM:
		return 30; break;

	case DECAPITATE:
	case TAKE_BLOOD_FROM_CORPSE:
		return 50; break;

	case PLANT_BOMB:
	case USE_REMOTE:
	case STEAL_ITEM:
	case PICK_LOCK:
	case LOCKPICK_CROUCHED:
	case STEAL_ITEM_CROUCHED:
		return 50; break;

	case SHOOT_ROCKET_CROUCHED:
	case SHOOT_ROCKET:
	case HELIDROP:
	case NINJA_SPINKICK:
		return 100; break;

	case CUTTING_FENCE:
	case JUMPWINDOWS:
	case LONG_JUMP:
		return 60; break;
	}

	return 0;
}

void SetDamageDisplayCounter( TacticalActor* pSoldier )
{
	INT16 sOffsetX, sOffsetY;

	if ( pSoldier->damageDisplay().displaying() )
	{
		pSoldier->damageDisplay().restart();
		return;
	}

	if ( pSoldier->identity().bodyType() == QUEENMONSTER )
	{
		pSoldier->damageDisplay().activateAt(0, 0);
	}
	else
	{
		GetSoldierAnimOffsets( pSoldier, &sOffsetX, &sOffsetY );
		pSoldier->damageDisplay().activateAt(sOffsetX, sOffsetY);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// SANDRO - This whole procedure was merged with the surgery ability of the doctor trait
//////////////////////////////////////////////////////////////////////////////////////////////////////

// Flugente: apply a consumable item on a soldier. Returns true if item was successfully interacted with
// Shadooow: Now returns 2 in case that the action failed due to the not enough action points!
BOOLEAN ApplyConsumable(TacticalActor* pSoldier, OBJECTTYPE *pObj, BOOLEAN fForce, BOOLEAN fUseAPs)
{
	if (!pSoldier || !pObj)
		return FALSE;

	// if it's not a kit or a misc item, we cannot consume it
	if (!(Item[pObj->usItem].usItemClass & (IC_KIT | IC_MISC)))
		return FALSE;

	BOOLEAN fSuccess = FALSE;
	BOOLEAN fDoSound = FALSE;

	// use portionsize, if none was entered, use full item
	UINT8 portionsize = Item[pObj->usItem].usPortionSize;
	if (!portionsize)
		portionsize = 100;

	// how much of this item do we use up
	UINT16 statusused = min(portionsize, (*pObj)[0]->data.objectStatus);
	if (!statusused || (statusused == 1 && ItemIsCanteen(pObj->usItem)))
		return FALSE;

	INT16 apcost = 0;
	
	// if we check for APs, do so - if we don't have enough, stop
	if ( fUseAPs )
	{
		// an object can be consumed in several ways (like food that is also a drug), but each consumption might have a different AP cost.
		// as it would be very odd if an effect does not happen because the corresponding AP cost could not be met, we analyze the item first and determine the AP cost.
		// We then either apply everything or nothing
		
		if ( HasItemFlag( pObj->usItem, CAMO_REMOVAL ) && gGameExternalOptions.fCamoRemoving )
		{
			apcost = max( apcost, (APBPConstants[AP_CAMOFLAGE] / 2) );
		}

		if (ItemIsCamoKit(pObj->usItem))
		{
			apcost = max( apcost, APBPConstants[AP_CAMOFLAGE] );
		}

		if (ItemIsCanteen(pObj->usItem))
		{
			apcost = max( apcost, APBPConstants[AP_DRINK] );
		}

		if ( pObj->usItem == JAR_ELIXIR )
		{
			apcost = max( apcost, APBPConstants[AP_CAMOFLAGE] );
		}

		if ( Item[pObj->usItem].clothestype )
		{
			INT16 disguise_apcost = (APBPConstants[AP_DISGUISE] * (100 - gSkillTraitValues.sCODisguiseAPReduction * NUM_SKILL_TRAITS( pSoldier, COVERT_NT ))) / 100;

			apcost = max( apcost, disguise_apcost );
		}

		if ( Item[pObj->usItem].drugtype )
		{
			apcost = max( apcost, APBPConstants[AP_DRINK] );
		}

		if ( Item[pObj->usItem].foodtype )
		{
			// do we eat or drink this stuff?
			UINT8 apcost_type = AP_EAT;
			if ( Food[Item[pObj->usItem].foodtype].bDrinkPoints > Food[Item[pObj->usItem].foodtype].bFoodPoints )
				apcost_type = AP_DRINK;

			apcost = max( apcost, APBPConstants[apcost_type] );
		}
	
		if ( !fForce && !EnoughPoints( pSoldier, apcost, 0, TRUE ) )
		{
			return 2;
		}
	}

	// under certain conditions, a merc can but simply does not want to consume an item, and can refuse if not forced to.
	if ( !fForce )
	{
		if ( DoesSoldierRefuseToEat( pSoldier, pObj ) )
		{
			return FALSE;
		}

		// some mercs will refuse to smoke
		if (ItemIsCigarette(pObj->usItem) && TacticalActorModifiers::backgroundValue(*pSoldier, BG_SMOKERTYPE ) == 2 )
		{
			// merc gets slightly pissed by the player even suggesting this
			TacticalCharacterDialogue( pSoldier, QUOTE_REFUSE_TO_SMOKE );
			pSoldier->morale().morale() = max( 0, pSoldier->morale().morale() - 1 );

			return FALSE;
		}
	}
	
	// Try to apply camo....
	// this returns true if camo can be applied, but APs were only used, and the action happened, if *pfGoodAPs is TRUE
	if ( ApplyCamo( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = TRUE;

		// WANNE: We should only delete the face, if there was a camo we applied.
		// This should fix the bug and crashes with missing faces
		if ( gGameExternalOptions.fShowCamouflageFaces )
		{
			// Flugente: refresh face regardless of result of SetCamoFace(), otherwise applying a rag will not clean the picture
			SetCamoFace( pSoldier );
			DeleteSoldierFace( pSoldier );// remove face
			pSoldier->renderBindings().faceIndex() = InitSoldierFace( pSoldier );// create new face
		}
	}
	
	if ( ApplyCanteen( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = FALSE;
	}
	
	if ( ApplyElixir( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = TRUE;
	}
	
	if ( ApplyClothes( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;
	}
	
	if ( ApplyFood( pSoldier, pObj, statusused ) )
	{
		fSuccess = TRUE;
		fDoSound = FALSE;
	}
	
	if ( ApplyDrugs_New( pSoldier, pObj->usItem, statusused ) )
	{
		fSuccess = TRUE;

		// no sound on consuming cigarettes, as that is very annoying
		if ( !ItemIsCigarette(pObj->usItem) )
		{
			fDoSound = TRUE;
		}
	}

	if ( !gGameExternalOptions.fFoodEatingSounds )
		fDoSound = FALSE;
	
	if ( fSuccess )
	{
		// Flugente: additional dialogue
		AdditionalTacticalCharacterDialogue_CallsLua( pSoldier, ADE_CONSUMEITEM, pObj->usItem );

		// use up object
		UseKitPoints( pObj, statusused, pSoldier );

		if ( fUseAPs )
		{
			DeductPoints( pSoldier, (INT16)apcost, 0, false );

			// Dirty
			fInterfacePanelDirty = DIRTYLEVEL2;
		}

		if ( fDoSound )
		{
			// Say OK acknowledge....
			TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_COOL1 );
		}

		return TRUE;
	}

	return FALSE;
}
