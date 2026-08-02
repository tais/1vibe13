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


//Kris:
//Temporary for testing the speed of the translucency.  Pressing Ctrl+L in turn based
//input will toggle this flag.  When clear, the translucency checking is turned off to
//increase the speed of the game.
BOOLEAN gfCalcTranslucency = FALSE;


extern BOOLEAN fReDrawFace;
extern UINT8 gubWaitingForAllMercsToExitCode;
BOOLEAN	gfGetNewPathThroughPeople = FALSE;

// LOCAL FUNCTIONS
void PlaySoldierFootstepSound( TacticalActor *pSoldier );
void HandleSystemNewAISituation( TacticalActor *pSoldier, BOOLEAN fResetABC );

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
