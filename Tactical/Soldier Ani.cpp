#include "TacticalActorLocomotion.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorEvents.h"
#include "TacticalActorInterrupts.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalActorEmploymentTypes.h"
#include "TacticalActorQuoteFlags.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"
#include "Grid Direction.h"
#include "Soldier Profile Constants.h"
#include "TacticalActorBattleSounds.h"
#include "TacticalActorDamageResolution.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorOrientation.h"
#include "TacticalActorRouteExecution.h"
#include "TacticalActorWorldPlacement.h"
#include "TacticalActorAnimationFrames.h"
#include "TacticalActorCombatActions.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorLighting.h"
#include "TacticalActorRecovery.h"
#include "TacticalActorRangedActions.h"
#include "TacticalActorWeaponHandling.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalActorEquipment.h"
#include "stdlib.h"
#include "TacticalWorldAdapter.h"
#include "DEBUG.H"
#include "MemMan.h"
#include "Overhead Types.h"
#include "TacticalActor.h" // I need this here - SANDRO
#include "SoldierRepository.h"
#include "Animation Data.h"
#include "Animation Control.h"
#include "Weapons.h"
#include "Soldier Ani.h"
#include "random.h"
#include "video.h"
#include "shading.h"
#include "Sound Control.h"
#include "Isometric Utils.h"
#include "Handle UI.h"
#include "Event Pump.h"
#include "opplist.h"
#include "lighting.h"
#include "ai.h"
#include "renderworld.h"
#include "Interactive Tiles.h"
#include "Points.h"
#include "message.h"
#include "World Items.h"
#include "physics.h"
#include "Soldier Create.h"
#include "Dialogue Control.h"
#include "Soldier Functions.h"
#include "Rotting Corpses.h"
#include "merc entering.h"
#include "Soldier Add.h"
#include "Soldier Profile.h"
#include "Interface.h"
#include "qarray.h"
#include "Soldier macros.h"
#include "Squads.h"
#include "worldman.h"
#include "Structure Wrap.h"
#include "PATHAI.H"
#include "pits.h"
#include "Text.h"
#include "NPC.h"
#include "Explosion Control.h"
#include "LOS.h"
#include "GameSettings.h"
#include "Boxing.h"
#include "Drugs And Alcohol.h"
#include "Smell.h"
#include "interface Dialogue.h"
#include "Strategic Status.h"
#include "Food.h"
#include "CampaignStats.h"				// added by Flugente
#include "DynamicDialogue.h"			// added by Flugente
#include "MilitiaIndividual.h"			// added by Flugente
#include "Town Militia.h"				// added by Flugente
#include "PreBattle Interface.h"		// added by Flugente
#include "Rebel Command.h"
#include "Civ Quotes.h"
#include "connect.h"
#include "fresh_header.h"
#include "CampaignMercenaryPolicy.h"
#include "GameContext.h"

#ifdef JA2UB
#else
#include "Meanwhile.h"
#endif // JA2UB


//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class TacticalActor;


#define		NO_JUMP											0
#define		MAX_ANIFRAMES_PER_FLASH			3
//#define		TIME_FOR_RANDOM_ANIM_CHECK	10
#define		TIME_FOR_RANDOM_ANIM_CHECK	1

SoldierID gfLastMercTalkedAboutKillingID = NOBODY;

extern void AddFuelToVehicle( TacticalActor *pSoldier, TacticalActor *pVehicle );


DOUBLE		gHopFenceForwardSEDist[ NUMSOLDIERBODYTYPES ] = { 2.2, 0.7, 3.2, 0.7 };
DOUBLE		gHopFenceForwardNWDist[ NUMSOLDIERBODYTYPES ] = { 2.7, 1.0, 2.7, 1.0 };
DOUBLE		gHopFenceForwardFullSEDist[ NUMSOLDIERBODYTYPES ] = { 1.1, 1.0, 2.1, 1.1 };
DOUBLE		gHopFenceForwardFullNWDist[ NUMSOLDIERBODYTYPES ] = { 0.8, 0.2, 2.7, 0.8 };
DOUBLE		gFalloffBackwardsDist[ NUMSOLDIERBODYTYPES ] = { 1, 0.8, 1, 1 };
DOUBLE		gClimbUpRoofDist[ NUMSOLDIERBODYTYPES ] = { 2, 0.1, 2, 2 };
DOUBLE		gClimbUpRoofLATDist[ NUMSOLDIERBODYTYPES ] = { 0.7, 0.5, 0.7, 0.5 };
DOUBLE		gClimbDownRoofStartDist[ NUMSOLDIERBODYTYPES ] = { 5.0, 1.0, 1, 1 };
DOUBLE		gClimbUpRoofDistGoingLower[ NUMSOLDIERBODYTYPES ] = { 0.9, 0.0, 1, 0.9 };



BOOLEAN HandleSoldierDeath( TacticalActor *pSoldier , BOOLEAN *pfMadeCorpse );

void CheckForAndHandleSoldierIncompacitated( TacticalActor *pSoldier );
BOOLEAN CheckForImproperFireGunEnd( TacticalActor *pSoldier );
BOOLEAN OKHeightDest( TacticalActor *pSoldier, INT32 sNewGridNo );
BOOLEAN HandleUnjamAnimation( TacticalActor *pSoldier );

extern void HandleSystemNewAISituation( TacticalActor *pSoldier, BOOLEAN fResetABC );
extern void PlaySoldierFootstepSound( TacticalActor *pSoldier );
extern UINT16 NumCapableEnemyInSector( );
extern BOOLEAN gfKillingGuysForLosingBattle;

extern SoldierID gubInterruptProvoker;

extern bool RemoveOneTurncoat( INT16 sSectorX, INT16 sSectorY, UINT8 aSoldierClass, BOOLEAN alsoRemoveFromGroup );
extern void PlaySplashSound(INT32 sGridNo);

// Animation code explanations!
//
//	0-399:	Actual animation frame indices
// 400-499:	Object manipulation codes
// 500-598:	Jump to frame x-501, same state.	Obvious limit 0-97
// 600-699:	Jump to new state x-600, frame 0
// 700-798:	Play sound
// 800-999:	Jump to new state x-700, frame 0 (starts with 100)
//

BOOLEAN AdjustToNextAnimationFrame( TacticalActor *pSoldier )
{
	EV_S_FIREWEAPON			SFireWeapon;

	UINT16				sNewAniFrame, anAniFrame;
	INT8					ubCurrentHeight;
	UINT16				usOldAnimState;
	static UINT32 uiJumpAddress = NO_JUMP;
	INT32 sNewGridNo;
	INT16					sX, sY;
	BOOLEAN				fStop;
	UINT32				cnt;
	UINT8					ubDiceRoll;						// Percentile dice roll
	UINT8					ubRandomHandIndex;		// Index value into random animation table to use base don what is in the guys hand...
	UINT16				usItem;
	RANDOM_ANI_DEF	*pAnimDef;
	UINT8					ubNewDirection;
	UINT8					ubDesiredHeight;
	BOOLEAN				bOKFireWeapon;
	BOOLEAN				bWeaponJammed;
	BOOLEAN				fFreeUpAttacker=FALSE;
	UINT16		usMovementMode;

	do
	{
		// Get new frame code
		sNewAniFrame = gusAnimInst[ pSoldier->animationPlayback().state() ][ pSoldier->animationPlayback().code() ];

		// Handle muzzle flashes
		if ( pSoldier->renderState().muzzleFlashFrame() > 0 )
		{
			// FLash for about 3 frames
			if ( pSoldier->renderState().muzzleFlashExpired(MAX_ANIFRAMES_PER_FLASH) )
			{
				if ( pSoldier->renderState().hasMuzzleFlashSprite() )
				{
					LightSpriteDestroy( pSoldier->renderState().muzzleFlashSprite() );
				}
				pSoldier->renderState().clearMuzzleFlashSprite();
			}
			else
			{
				pSoldier->renderState().advanceMuzzleFlashFrame();
			}

		}

		if ( pSoldier->collapseState().breathTriggered() )
		{
			// ATE: If we have fallen, and we can't get up... no
			// really, if we were told to collapse but have been hit after, don't
			// do anything...
			if ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_HITSTOP | ANIM_HITFINISH ) )
			{
				pSoldier->collapseState().clearBreathCollapse();
			}
			else if ( pSoldier->vitals().health() == 0 )
			{
				// Death takes precedence...
				pSoldier->collapseState().clearBreathCollapse();
			}
			else if ( pSoldier->animationIntent().pendingAnimation() == FALLFORWARD_ROOF || pSoldier->animationIntent().pendingAnimation() == FALLOFF || pSoldier->animationPlayback().state() == FALLFORWARD_ROOF || pSoldier->animationPlayback().state() == FALLOFF )
			{
				pSoldier->collapseState().clearBreathCollapse();
			}
			else
			{
				// Wait here until we are free....
				if ( !pSoldier->animationActivity().nonInterruptible() )
				{
					// UNset UI
					UnSetUIBusy( pSoldier->identity().id() );

					(void)TacticalActorRecovery::collapse(*pSoldier);

					pSoldier->collapseState().clearBreathCollapse();

					return( TRUE );
				}
			}
		}
		
		// Check for special code
		if ( sNewAniFrame < 399 )
		{

			// Adjust / set true ani frame
			// Use -1 because ani files are 1-based, these are 0-based
			(void)TacticalActorAnimationFrames::selectFrame(
				*pSoldier,
				static_cast<UINT16>(sNewAniFrame - 1));

			// Adjust frame control pos, and try again
			pSoldier->animationPlayback().code()++;
			break;
		}
		else if ( sNewAniFrame < 500 )
		{
			// Switch on special code
			switch( sNewAniFrame )
			{
			case 402:

				// DO NOT MOVE FOR THIS FRAME
				pSoldier->movement().pauseMovement();
				break;

			case 403:

				// MOVE GUY FORWARD SOME VALUE
				MoveMercFacingDirection( pSoldier, FALSE, (FLOAT)0.7 );

				break;

			case 404:

				// MOVE GUY BACKWARD SOME VALUE
				sNewGridNo = pSoldier->position().gridNo();
				// Use same function as forward, but is -ve values!
				MoveMercFacingDirection( pSoldier , TRUE, (FLOAT)1 );
				//shadooow: since we just moved from original grid to the jump destination, now it is proper time to change bLevel
				if (sNewGridNo != pSoldier->position().gridNo())
				{
					pSoldier->position().level() = 0;
				}
				break;

			case 405:

				return( TRUE );

			case 406:

				// Move merc up
				if ( pSoldier->position().direction() == NORTH )
				{
					(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().heightAdjustment() + 2 )	);
				}
				else
				{
					(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)( pSoldier->position().heightAdjustment() + 3 ) );
				}
				break;

			case 408:

				// CODE: SPECIAL MOVE CLIMB UP ROOF EVENT

				// Moved here because this represents "already on the roof", so breath collapses and interrupts should
				// keep the soldier on the roof where he belongs
				// Move merc up specific height
				(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)50 );

				{
					INT16		sXPos, sYPos;

					//sNewGridNo = NewGridNo( pSoldier->sGridNo, (UINT16)DirectionInc( pSoldier->ubDirection ) );
					ConvertGridNoToCenterCellXY( pSoldier->position().temporaryGrid(), &sXPos, &sYPos );
					(void)TacticalActorWorldPlacement::setPosition(*pSoldier, (FLOAT)sXPos, (FLOAT)sYPos );
				}

				// re-enable sight
				gTacticalStatus.uiFlags &= (~DISALLOW_SIGHT);

				// Move two CC directions
				(void)TacticalActorOrientation::setDirection(*pSoldier, gTwoCCDirection[ pSoldier->position().direction() ] );

				(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

				// Set desired anim height!
				pSoldier->animationIntent().desiredHeight() = ANIM_CROUCH;

				// Madd
				usMovementMode = TacticalActorMobility::movementStateForCurrentStance(*pSoldier);
				pSoldier->movement().mode() = usMovementMode;

				// ATE: Change interface level.....
				// CJC: only if we are a player merc
				if (pSoldier->roster().team() == gbPlayerNum)
				{

					if (gTacticalStatus.fAutoBandageMode)
					{
						// in autobandage, handle as AI, but add roof marker too
						FreeUpNPCFromRoofClimb( pSoldier );
						HandlePlacingRoofMarker( pSoldier, pSoldier->position().gridNo(), TRUE, TRUE );
					}
					else
					{
						// OK, UNSET INTERFACE FIRST
						UnSetUIBusy( pSoldier->identity().id() );

						if ( pSoldier->identity().id() == gusSelectedSoldier )
						{
							ChangeInterfaceLevel( 1 );
						}
						HandlePlacingRoofMarker( pSoldier, pSoldier->position().gridNo(), TRUE, TRUE );
					}
				}
				else
				{
					FreeUpNPCFromRoofClimb( pSoldier );
				}

				// ATE: Handle sight...
				HandleSight( pSoldier,SIGHT_LOOK | SIGHT_RADIO | SIGHT_INTERRUPT );
				break;

			case 409:

				//CODE: MOVE DOWN
				(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)( pSoldier->position().heightAdjustment() - 2 ) );
				break;

			case 410:

				// Move merc down specific height
				(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)0 );
				break;

			case 411:

				// CODE: SPECIALMOVE CLIMB DOWN EVENT
				// Move two C directions
				(void)TacticalActorOrientation::setDirection(*pSoldier, gTwoCDirection[ pSoldier->position().direction() ] );

				// Remove the roof marker
				HandlePlacingRoofMarker( pSoldier, pSoldier->position().gridNo(), FALSE, TRUE );

				(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
				// Adjust height //shadooow: do not change bLevel yet, we are still at roof!
				(void)TacticalActorWorldPlacement::setHeight(*pSoldier,(FLOAT)gClimbDownRoofStartDist[pSoldier->identity().bodyType()], FALSE);
				// Adjust position
				MoveMercFacingDirection( pSoldier , TRUE, (FLOAT)3.5 );
				break;

			case 412:

				// CODE: HANDLING PRONE DOWN - NEED TO MOVE GUY BACKWARDS!
				MoveMercFacingDirection( pSoldier , FALSE, (FLOAT).2 );
				break;

			case 413:

				// CODE: HANDLING PRONE UP - NEED TO MOVE GUY FORWARDS!
				MoveMercFacingDirection( pSoldier , TRUE, (FLOAT).2 );
				break;

			case 430:
				{
					// sevenfm: breaking window with crowbar code
					if (pSoldier->inventory()[HANDPOS].exists() &&
						(ItemIsCrowbar(pSoldier->inventory()[HANDPOS].usItem) &&	Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass & (IC_PUNCH) ||
						Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass & IC_GUN && ItemIsTwoHanded(pSoldier->inventory()[HANDPOS].usItem) && ItemIsMetal(pSoldier->inventory()[HANDPOS].usItem)))
					{
						INT32 sWindowGridNo = pSoldier->targeting().gridNo();
						if (pSoldier->position().direction() == NORTH || pSoldier->position().direction() == WEST)
							sWindowGridNo = NewGridNo(pSoldier->position().gridNo(), (UINT16)DirectionInc((UINT8)pSoldier->position().direction()));

						// is there really an intact window that we jump through?
						if (IsJumpableWindowPresentAtGridNo(sWindowGridNo, pSoldier->position().direction(), TRUE) && !IsJumpableWindowPresentAtGridNo(sWindowGridNo, pSoldier->position().direction(), FALSE))
						{
							STRUCTURE * pStructure = FindStructure(sWindowGridNo, STRUCTURE_WALLNWINDOW);
							if (pStructure && !(pStructure->fFlags & STRUCTURE_OPEN))
							{
								// intact window found. Smash it!
								WindowHit(sWindowGridNo, pStructure->usStructureID, (pSoldier->position().direction() == SOUTH || pSoldier->position().direction() == EAST), TRUE);
								// damage weapon
								if (Chance(50 - 10 * min(5, Item[pSoldier->inventory()[HANDPOS].usItem].bReliability)))
								{
									pSoldier->inventory()[HANDPOS][0]->data.objectStatus--;
									if (Random(100) < Item[pSoldier->inventory()[HANDPOS].usItem].usDamageChance)
									{
										pSoldier->inventory()[HANDPOS][0]->data.sRepairThreshold--;
									}
								}
							}
						}
					}

					DebugMsg(TOPIC_JA2,DBG_LEVEL_3,"AdjustToNextAnimationFrame: case 430");
					// SHOOT GUN
					// MAKE AN EVENT, BUT ONLY DO STUFF IF WE OWN THE GUY!
					SFireWeapon.usSoldierID			= pSoldier->identity().id();
					SFireWeapon.uiUniqueId			= pSoldier->identity().incarnation();
					SFireWeapon.sTargetGridNo		= pSoldier->targeting().gridNo();
					SFireWeapon.bTargetLevel		= pSoldier->targeting().level();
					SFireWeapon.bTargetCubeLevel= pSoldier->targeting().cubeLevel();
					if((is_server && pSoldier->identity().id()<120) || (!is_server && is_client && pSoldier->identity().id()<20) || (!is_server && !is_client) )
					{
						//only carry on if own merc
						AddGameEvent( S_FIREWEAPON, 0, &SFireWeapon );
				
						//hayden
						if(is_server || (is_client && pSoldier->identity().id() <20) )
							send_fireweapon( &SFireWeapon );
					}

					OBJECTTYPE* pObjHand = TacticalActorEquipment::usedWeapon(*pSoldier, &pSoldier->inventory()[HANDPOS] );

					//DIGICRAB: Burst UnCap
					//Loop around in the animation if we still have burst rounds to fire
					if (pSoldier->fireControl().burstCounter() && !(TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier))
						&& ( pSoldier->fireControl().burstCounter() <= ( (pSoldier->fireControl().autofireShots())?(pSoldier->fireControl().autofireShots()):(GetShotsPerBurst( pObjHand ))	)
						|| (( pSoldier->attackSelection().weaponMode() == WM_ATTACHED_GL_BURST && pSoldier->fireControl().burstCounter() <= Weapon[GetAttachedGrenadeLauncher(&pSoldier->inventory()[HANDPOS])].ubShotsPerBurst)) ))
					{
						if(pSoldier->animationPlayback().state() == STANDING_BURST || pSoldier->animationPlayback().state() == CROUCHED_BURST || pSoldier->animationPlayback().state() == PRONE_BURST || pSoldier->animationPlayback().state() == BURST_ALTERNATIVE_STAND && pSoldier->animationPlayback().code() == 33) //we are standing, crounching or prone, firing the fast shot
							pSoldier->animationPlayback().code() = 3;
						else if(pSoldier->animationPlayback().state() == FIRE_BURST_LOW_STAND || pSoldier->animationPlayback().state() == LOW_BURST_ALTERNATIVE_STAND && pSoldier->animationPlayback().code() == 37) //we are firing down to something very close, last shot
							pSoldier->animationPlayback().code() = 14;
						else if(pSoldier->animationPlayback().state() == TANK_BURST)//dnl ch64 280813 fix 6 round burst limitation
						{
							if(pSoldier->fireControl().burstCounter() < pSoldier->fireControl().autofireShots())//!!! this will be fine as long you not decide to equip tank with other weapons add limitted ammo, weapon jam, etc.
								pSoldier->animationPlayback().code() = 4;
							else
								pSoldier->animationPlayback().code() = 34;
						}
						else if(pSoldier->animationPlayback().state() == ROBOT_BURST_SHOOT)	//silversurfer: Bugfix JaggZilla #532 6 round burst limitation
						{
							if(pSoldier->fireControl().burstCounter() < pSoldier->fireControl().autofireShots())
								pSoldier->animationPlayback().code() = 4;
							else
								pSoldier->animationPlayback().code() = 34;
						}
					}

					OBJECTTYPE* pObjUsed = TacticalActorEquipment::usedWeapon(*pSoldier, &pSoldier->inventory()[ pSoldier->attackSelection().hand() ] );
					UINT16 usedGun = TacticalActorEquipment::usedWeaponNumber(*pSoldier, &pSoldier->inventory()[ pSoldier->attackSelection().hand() ] );
					//DIGICRAB: Burst Sound
					//This code is stolen from Tactical\Weapons.c - UseGun(...)
					if (pSoldier->fireControl().burstCounter() &&
						!pSoldier->audio().hasBurstSound() &&
						Weapon[ usedGun ].sSound != 0 &&
						Item[ usedGun ].usItemClass != IC_THROWING_KNIFE )
					{
						// Switch on silencer...
						INT16 noisefactor = GetPercentNoiseVolume( pObjUsed );
						if( noisefactor < gGameExternalOptions.gubMaxPercentNoiseSilencedSound || Weapon[ usedGun ].ubAttackVolume <= 10 )
						{
							INT32 uiSound;

							uiSound = Weapon [	usedGun ].silencedSound;
							//if ( Weapon[ pSoldier->attackSelection().weapon() ].ubCalibre == AMMO9 || Weapon[ pSoldier->attackSelection().weapon() ].ubCalibre == AMMO38 || Weapon[ pSoldier->attackSelection().weapon() ].ubCalibre == AMMO57 )
							//{
							//	uiSound = S_SILENCER_1;
							//}
							//else
							//{
							//	uiSound = S_SILENCER_2;
							//}

							//randomize the rate a bit so that the sound is more believable
							PlayJA2Sample( uiSound, 44100-Random(5000)-Random(5000)-Random(5000), SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );

						}
						else
						{
							INT8 volume = HIGHVOLUME;
							if ( noisefactor < 100 ) volume = (volume * noisefactor) / 100;
							//randomize the rate a bit so that the sound is more believable
							PlayJA2Sample( Weapon[ usedGun ].sSound, 44100-Random(5000)-Random(5000)-Random(5000), SoundVolume( volume, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
						}
					}
				}
				break;

			case 431:

				// FLASH FRAME WHITE
				pSoldier->palette().setForcedShade(White16BPPPalette);
				break;

			case 432:

				// PLAY RANDOM IMPACT SOUND!
				//	PlayJA2Sample( (UINT8)( BULLET_IMPACT_1 + Random(3) ), RATE_11025, MIDVOLUME, 1, MIDDLEPAN );

				// PLAY RANDOM GETTING HIT SOUND
				//	TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_HIT1 );

				break;

			case 433:

				// CODE: GENERIC HIT!

				CheckForAndHandleSoldierIncompacitated( pSoldier );


				break;

			case 434:

				// JUMP TO ANOTHER ANIMATION ( BLOOD ) IF WE WANT BLOOD
				uiJumpAddress = pSoldier->animationPlayback().state();
				TacticalActorAnimationTransitions::changeState(*pSoldier,  FLYBACK_HIT_BLOOD_STAND, 0, FALSE );
				return( TRUE );
				break;

			case 435:

				// HOOK FOR A RETURN JUMP
				break;

			case 436:

				// Loop through script to find entry address
				if ( uiJumpAddress == NO_JUMP )
				{
					break;
				}
				usOldAnimState = pSoldier->animationPlayback().state();
				pSoldier->animationPlayback().code()	= 0;

				do
				{
					// Get new frame code
					anAniFrame = gusAnimInst[ uiJumpAddress ][ pSoldier->animationPlayback().code() ];

					if ( anAniFrame == 435 )
					{
						// START PROCESSING HERE
						TacticalActorAnimationTransitions::changeState(*pSoldier,  (UINT16)uiJumpAddress, pSoldier->animationPlayback().code(), FALSE );
						return( TRUE );
					}
					// Adjust frame control pos, and try again
					pSoldier->animationPlayback().code()++;
				}
				while( anAniFrame != 999 );

				uiJumpAddress = NO_JUMP;

				if ( anAniFrame == 999 )
				{
					// Fail jump, re-load old anim
					TacticalActorAnimationTransitions::changeState(*pSoldier,  usOldAnimState, 0, FALSE );
					return( TRUE );
				}
				break;

			case 437:

				// CHANGE DIRECTION AND GET-UP
				//sGridNo = NewGridNo( pSoldier->sGridNo, (UINT16)(-1 * DirectionInc( pSoldier->ubDirection ) ) );
				//ConvertMapPosToWorldTileCenter( pSoldier->sGridNo, &sXPos, &sYPos );
				//SetSoldierPosition( pSoldier, (FLOAT)sXPos, (FLOAT)sYPos );


				// Reverse direction
				(void)TacticalActorOrientation::setDirection(*pSoldier, 	gOppositeDirection[ pSoldier->position().direction() ] );
				(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

				TacticalActorAnimationTransitions::changeState(*pSoldier,  GETUP_FROM_ROLLOVER, 0 , FALSE );
				return( TRUE );

			case 438:

				//CODE: START HOLD FLASH
				pSoldier->renderState().enableForceShade();
				break;

			case 439:

				//CODE: END HOLD FLASH
				pSoldier->renderState().disableForceShade();
				break;

			case 440:
				//CODE: Set buddy as dead!
				{
					BOOLEAN fMadeCorpse;

					// ATE: Piggyback here on stopping the burn sound...
					if ( pSoldier->animationPlayback().state() == CHARIOTS_OF_FIRE ||
						pSoldier->animationPlayback().state() == BODYEXPLODING )
					{
						SoundStop( pSoldier->pendingAction().primaryData() );
					}


					CheckForAndHandleSoldierDeath( pSoldier, &fMadeCorpse );

					if ( fMadeCorpse )
					{
						return( FALSE );
					}
					else
					{
						return( TRUE );
					}
				}
				break;

			case 441:
				// CODE: Show muzzle flash
				{
					OBJECTTYPE* pObjAttHand = TacticalActorEquipment::usedWeapon(*pSoldier, &pSoldier->inventory()[pSoldier->attackSelection().hand()]);
					if (IsFlashSuppressor(pObjAttHand, pSoldier) || (*pObjAttHand)[0]->data.gun.bGunAmmoStatus < 0)
					{
						pSoldier->renderState().hideMuzzleFlash();
					}
					else
					{
						pSoldier->renderState().showMuzzleFlash();
					}
					DebugMsg(TOPIC_JA2, DBG_LEVEL_3, String("UseGun: Muzzle flash = %d", pSoldier->renderState().muzzleFlashVisible()));
				}
				if ( !pSoldier->renderState().muzzleFlashVisible() )
				{
					break;
				}

				// DO ONLY IF WE'RE AT A GOOD LEVEL
				if ( ubAmbientLightLevel < MIN_AMB_LEVEL_FOR_MERC_LIGHTS )
				{
					break;
				}

				if( ( pSoldier->renderState().muzzleFlashSprite()=LightSpriteCreate("L-R03.LHT", 0 ) )==(-1))
				{
					return( TRUE );
				}

				LightSpritePower(pSoldier->renderState().muzzleFlashSprite(), TRUE);
				// Get one move forward
				{
					INT32	usNewGridNo;
					INT16 sXPos, sYPos;

					usNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( pSoldier->position().direction() ) );
					ConvertGridNoToCenterCellXY( usNewGridNo, &sXPos, &sYPos );
					LightSpritePosition( pSoldier->renderState().muzzleFlashSprite(), (INT16)(sXPos/CELL_X_SIZE), (INT16)(sYPos/CELL_Y_SIZE));

					// Start count
					pSoldier->renderState().startMuzzleFlashSprite(
						pSoldier->renderState().muzzleFlashSprite());
				}
				break;

			case 442:

				//CODE: FOR A NON-INTERRUPTABLE SCRIPT - SIGNAL DONE
				pSoldier->animationActivity().nonInterruptible() = FALSE;

				// ATE: if it's the begin cower animation, unset ui, cause it could
				// be from player changin stance
				if ( pSoldier->animationPlayback().state() == START_COWER || pSoldier->animationPlayback().state() == START_COWER_CROUCHED || pSoldier->animationPlayback().state() == START_COWER_PRONE )
				{
					UnSetUIBusy( pSoldier->identity().id() );
				}
				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				// SANDRO - if pending interrupt flag was set for after-attack type of interupt, try to resolve it now
				else if ( UsingImprovedInterruptSystem() )
				{
					if ( ResolvePendingInterrupt( pSoldier, AFTERACTION_INTERRUPT ) )
					{
						pSoldier->animationIntent().clearFacingAnimation();
						// "artificially" set lock ui flag in this case
						if (pSoldier->roster().team() == gbPlayerNum)
						{
							//AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, Message[STR_INTERRUPT] );
							guiPendingOverrideEvent = LU_BEGINUILOCK;								
							HandleTacticalUI( );
						}
						return( TRUE );		
					}
				}
				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				break;

			case 443:

				// MOVE GUY FORWARD FOR FENCE HOP ANIMATION
				switch( pSoldier->position().direction() )
				{
				case SOUTH:
				case EAST:

					MoveMercFacingDirection( pSoldier, FALSE, (FLOAT)gHopFenceForwardSEDist[ pSoldier->identity().bodyType() ] );
					break;

				case NORTH:
				case WEST:
					MoveMercFacingDirection( pSoldier, FALSE, (FLOAT)gHopFenceForwardNWDist[ pSoldier->identity().bodyType() ] );
					break;
				}
				break;

			case 444:

				// CODE: End Hop Fence
				// MOVE TO FORCASTED GRIDNO
				ConvertGridNoToCenterCellXY(pSoldier->animationActivity().traversalForecastGrid(), &sX, &sY);

				(void)TacticalActorWorldPlacement::setPosition(*pSoldier, (FLOAT) sX, (FLOAT) sY, FALSE, FALSE, FALSE );
				(void)TacticalActorOrientation::setDirection(*pSoldier, 	gTwoCDirection[ pSoldier->position().direction() ] );
				pSoldier->animationActivity().clearRenderZOverride();
				(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

				if ( gTacticalStatus.bBoxingState == BOXING_WAITING_FOR_PLAYER || gTacticalStatus.bBoxingState == PRE_BOXING || gTacticalStatus.bBoxingState == BOXING )
				{
					BoxingMovementCheck( pSoldier );
				}

				if ( SetOffBombsInGridNo( pSoldier->identity().id(), pSoldier->position().gridNo(), FALSE, pSoldier->position().level() ))
				{
					(void)TacticalActorRouteExecution::stopAt(*pSoldier, pSoldier->position().gridNo(), pSoldier->position().direction() );
					return( TRUE );
				}

				// take the correct stance
				if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight != gAnimControl[ pSoldier->movement().mode() ].ubEndHeight )
				{
					// Goto Stance...
					(void)TacticalActorOrientation::changeStance(*pSoldier, gAnimControl[ pSoldier->movement().mode() ].ubEndHeight );

					if ( pSoldier->position().gridNo() == pSoldier->pathing().finalDestinationGrid() )
					{
						// Set UI Busy
						UnSetUIBusy( pSoldier->identity().id() );
					}

					return( TRUE );
				}
				break;

			case 445:

				// CODE: MOVE GUY FORWARD ONE TILE, BASED ON WHERE WE ARE FACING
				switch( pSoldier->position().direction() )
				{
				case SOUTH:
				case EAST:

					MoveMercFacingDirection( pSoldier, FALSE, (FLOAT)gHopFenceForwardFullSEDist[ pSoldier->identity().bodyType() ] );
					break;

				case NORTH:
				case WEST:

					MoveMercFacingDirection( pSoldier, FALSE, (FLOAT)gHopFenceForwardFullNWDist[ pSoldier->identity().bodyType() ] );
					break;

				}
				break;

			case 446:

				// CODE: Turn pause move flag on
				pSoldier->status().flags() |= SOLDIER_PAUSEANIMOVE;
				break;

			case 447:

				// TRY TO FALL!!!
				if ( pSoldier->animationActivity().tryingToFall() )
				{
					INT16 sLastAniFrame;

					// TRY FORWARDS...
					// FIRST GRIDNO
						sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( pSoldier->position().direction() ) );

					if ( OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), pSoldier->position().direction(), FALLFORWARD_HITDEATH_STOP ) )
					{
						// SECOND GRIDNO
						// sNewGridNo = NewGridNo( sNewGridNo, (UINT16)( DirectionInc( pSoldier->ubDirection ) ) );

						// if ( OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), pSoldier->ubDirection, FALLFORWARD_HITDEATH_STOP ) )
						{
							// ALL'S OK HERE...
							pSoldier->animationActivity().clearFall();
							break;
						}
					}


					// IF HERE, INCREMENT DIRECTION
					// ATE: Added Feb1 - can be either direction....
					if ( pSoldier->animationActivity().fallClockwise() )
					{
						(void)TacticalActorOrientation::setDirection(*pSoldier, 	gOneCDirection[ pSoldier->position().direction() ] );
					}
					else
					{
						(void)TacticalActorOrientation::setDirection(*pSoldier, 	gOneCCDirection[ pSoldier->position().direction() ] );
					}
					(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
					sLastAniFrame = gusAnimInst[ pSoldier->animationPlayback().state() ][ ( pSoldier->animationPlayback().code() - 2 ) ];
					(void)TacticalActorAnimationFrames::selectFrame(
						*pSoldier,
						static_cast<UINT16>(sLastAniFrame));

					if ( pSoldier->position().direction() == pSoldier->animationActivity().fallDirection() )
					{
						// GO FORWARD HERE...
						pSoldier->animationActivity().clearFall();
						break;;
					}
					// IF HERE, RETURN SO WE DONOT INCREMENT DIR
					return( TRUE );
				}
				break;


			case 448:
				{
					// CODE: HANDLE BURST
					// FIRST CHECK IF WE'VE REACHED MAX FOR GUN
					fStop = FALSE;

					OBJECTTYPE* pObjHand = TacticalActorEquipment::usedWeapon(*pSoldier, &pSoldier->inventory()[HANDPOS] );

					if ( ( pSoldier->attackSelection().weaponMode() == WM_ATTACHED_GL_BURST && pSoldier->fireControl().burstCounter() > Weapon[GetAttachedGrenadeLauncher(&pSoldier->inventory()[HANDPOS])].ubShotsPerBurst) )
					{
						DebugMsg(TOPIC_JA2,DBG_LEVEL_3,"AdjustToNextAnimationFrame: Burst case 448, stopping because gl max burst size reached");
						fStop = TRUE;
						fFreeUpAttacker = TRUE;
					}
					else if (pSoldier->attackSelection().weaponMode() != WM_ATTACHED_GL_BURST && !(TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier)) && (pSoldier->fireControl().burstCounter() > ((pSoldier->fireControl().autofireShots())?(pSoldier->fireControl().autofireShots()):(GetShotsPerBurst( pObjHand )))))
					{
						DebugMsg(TOPIC_JA2,DBG_LEVEL_3,"AdjustToNextAnimationFrame: Burst case 448, stopping because gun max burst size reached");
						fStop = TRUE;
						fFreeUpAttacker = TRUE;
					}
					else if (pSoldier->attackSelection().weaponMode() != WM_ATTACHED_GL_BURST && TacticalActorWeaponHandling::isValidSecondHandBurst(*pSoldier) && (pSoldier->fireControl().burstCounter() > ((pSoldier->fireControl().autofireShots())?(2*pSoldier->fireControl().autofireShots()):(2*GetShotsPerBurst( pObjHand )))))
					{
						DebugMsg(TOPIC_JA2,DBG_LEVEL_3,"AdjustToNextAnimationFrame: Burst case 448, stopping because dual max burst size reached");
						fStop = TRUE;
						fFreeUpAttacker = TRUE;
					}

					// CHECK IF WE HAVE AMMO LEFT, IF NOT, END ANIMATION!
					if ( !EnoughAmmo( pSoldier, FALSE, pSoldier->attackSelection().hand() ) )
					{
						DebugMsg(TOPIC_JA2,DBG_LEVEL_3,"AdjustToNextAnimationFrame: Burst case 448, stopping because not enough ammo");
						fStop = TRUE;
						fFreeUpAttacker = TRUE;
						if ( pSoldier->roster().team() == gbPlayerNum	)
						{
							ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, TacticalStr[ BURST_FIRE_DEPLETED_CLIP_STR ] );
						}
					}
					// Why only check for a jam on the first bullet?
					else if (pSoldier->fireControl().burstCounter() == 1)
					{
						// CHECK FOR GUN JAM
						bWeaponJammed = CheckForGunJam( pSoldier );
						if ( bWeaponJammed == TRUE )
						{
							DebugMsg(TOPIC_JA2,DBG_LEVEL_3,"AdjustToNextAnimationFrame: Burst case 448, stopping because weapon jammed");
							fStop = TRUE;
							fFreeUpAttacker = TRUE;
							// stop shooting!
							// Yeah, but what does this have to do with that?
							//							pSoldier->fireControl().bulletsLeft() = 0;

							// OK, Stop burst sound...
							if ( pSoldier->audio().hasBurstSound() )
							{
								SoundStop( pSoldier->audio().burstSoundId() );
								pSoldier->audio().clearBurstSound();
							}

							if ( pSoldier->roster().team() == gbPlayerNum	)
							{
								PlayJA2Sample( S_DRYFIRE1, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
								//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Gun jammed!" );
							}

							DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Freeing up attacker - aborting start of attack due to burst gun jam") );
							FreeUpAttacker( );
						}
						else if ( bWeaponJammed == 255 )
						{
							// Play intermediate animation...
							if ( HandleUnjamAnimation( pSoldier ) )
							{
								return( TRUE );
							}
						}
					}

					if ( fStop )
					{
						if(pSoldier->fireControl().autofireShots()) //reset the autofire cursor after firing
						{
							pSoldier->fireControl().autofireLastStep() = FALSE;
							pSoldier->fireControl().autofireShots() = 1;
						}

						pSoldier->fireControl().spreadIndex() = FALSE;
						pSoldier->fireControl().burstCounter() = 1;
						// pSoldier->flags.fBurstCompleted = TRUE;
						if ( fFreeUpAttacker )
						{
							// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Freeing up attacker - aborting start of attack") );
							// FreeUpAttacker( pSoldier->identity().id() );
						}

						// ATE; Reduce it due to animation being stopped...
						// 0verhaul: No longer necessary or desired
						// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Freeing up attacker - Burst animation ended") );
						// ReduceAttackBusyCount( pSoldier->identity().id(), FALSE );


						if ( CheckForImproperFireGunEnd( pSoldier ) )
						{
							return( TRUE );
						}

						// END: GOTO AIM STANCE BASED ON HEIGHT
						// If we are a robot - we need to do stuff different here
						// 0verhaul:	Ya know, if the robot simply used the same animation for standing and rifle standing,
						// we probably wouldn't need this special case code.
						if ( AM_A_ROBOT( pSoldier ) )
						{
							TacticalActorAnimationTransitions::changeState(*pSoldier,  STANDING, 0 , FALSE );
						}
						else
						{
							switch ( gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight )
							{
							case ANIM_STAND:
								if ( pSoldier->animationPlayback().state() == BURST_DUAL_STAND )
									TacticalActorAnimationTransitions::changeState(*pSoldier,  AIM_DUAL_STAND, 0 , FALSE );
								else if ( pSoldier->animationPlayback().state() == BURST_ALTERNATIVE_STAND || pSoldier->animationPlayback().state() == LOW_BURST_ALTERNATIVE_STAND )
									TacticalActorAnimationTransitions::changeState(*pSoldier,  AIM_ALTERNATIVE_STAND, 0 , FALSE );
								else 
									TacticalActorAnimationTransitions::changeState(*pSoldier,  AIM_RIFLE_STAND, 0 , FALSE );
								break;

							case ANIM_PRONE:
								if ( pSoldier->animationPlayback().state() == BURST_DUAL_PRONE )
									TacticalActorAnimationTransitions::changeState(*pSoldier,  AIM_DUAL_PRONE, 0 , FALSE );
								else
									TacticalActorAnimationTransitions::changeState(*pSoldier,  AIM_RIFLE_PRONE, 0 , FALSE );
								break;

							case ANIM_CROUCH:
								if ( pSoldier->animationPlayback().state() == BURST_DUAL_CROUCH )
									TacticalActorAnimationTransitions::changeState(*pSoldier,  AIM_DUAL_CROUCH, 0 , FALSE );
								else
									TacticalActorAnimationTransitions::changeState(*pSoldier,  AIM_RIFLE_CROUCH, 0 , FALSE );
								break;

							}
						}
						return( TRUE );
					}

					// MOVETO CURRENT SPREAD LOCATION
					if ( pSoldier->fireControl().spreadIndex() )
					{
						if ( pSoldier->fireControl().spreadLocations()[ pSoldier->fireControl().spreadIndex() - 1 ] != 0 )
						{
							(void)TacticalActorOrientation::setDirection(*pSoldier, (INT8)GetDirectionToGridNoFromGridNo( pSoldier->position().gridNo(), pSoldier->fireControl().spreadLocations()[ pSoldier->fireControl().spreadIndex() - 1 ] ) );
							(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );
						}
					}
				}
				break;

			case 449:

				//CODE: FINISH BURST
				pSoldier->fireControl().spreadIndex() = FALSE;
				pSoldier->fireControl().burstCounter() = 1;
				//				pSoldier->flags.fBurstCompleted = TRUE;
				break;

			case 450:

				//CODE: BEGINHOPFENCE
				// MOVE TWO FACGIN GRIDNOS
				// Flugente: the old complicated method relied on the route path being filled correctly, which it often wasn't.
				// This is unneccessary, as we've already filled sTempNewGridNo with the correct data
				// We could fill the traversal forecast when initiating the jump,
				// but keep this animation-script hook.
				if ( pSoldier->position().temporaryGrid() != NOWHERE )
					pSoldier->animationActivity().forecastTraversalAt(pSoldier->position().temporaryGrid());
				else
					// hey, it's better than nowhere
					pSoldier->animationActivity().forecastTraversalAt(pSoldier->position().gridNo());

				break;


			case 451:

				// CODE: MANAGE START z-buffer override
				switch( pSoldier->position().direction() )
				{
				case NORTH:
				case WEST:

					pSoldier->animationActivity().setRenderZOverride(TOPMOST_Z_LEVEL);
					break;
				}
				break;

			case 452:

				// CODE: MANAGE END z-buffer override
				switch( pSoldier->position().direction() )
				{
				case SOUTH:
				case EAST:

					pSoldier->animationActivity().setRenderZOverride(TOPMOST_Z_LEVEL);
					break;

				case NORTH:
				case WEST:

					pSoldier->animationActivity().clearRenderZOverride();
					break;

				}
				break;

			case 453:

				//CODE: FALLOFF ROOF ( BACKWARDS ) - MOVE BACK SOME!
				// Use same function as forward, but is -ve values!
				MoveMercFacingDirection( pSoldier , TRUE, (FLOAT)gFalloffBackwardsDist[ pSoldier->identity().bodyType() ] );
				break;

			case 454:

				// CODE: HANDLE CLIMBING ROOF,
				// Move merc up
				if ( pSoldier->position().direction() == NORTH )
				{
					(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().animationHeightAdjustment() + gClimbUpRoofDist[ pSoldier->identity().bodyType() ] ) );
				}
				else
				{
					(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().animationHeightAdjustment() + gClimbUpRoofDist[ pSoldier->identity().bodyType() ] ) );
				}
				break;

			case 455:

				// MOVE GUY FORWARD SOME VALUE
				MoveMercFacingDirection( pSoldier, FALSE, (FLOAT)gClimbUpRoofLATDist[ pSoldier->identity().bodyType() ] );

				// MOVE DOWN SOME VALUE TOO!
				(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().animationHeightAdjustment() - gClimbUpRoofDistGoingLower[ pSoldier->identity().bodyType() ] ) );

				break;

			case 456:

				// CODE: HANDLE CLIMBING ROOF,
				// Move merc DOWN
				if ( pSoldier->position().direction() == NORTH )
				{
					(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().animationHeightAdjustment() - gClimbUpRoofDist[ pSoldier->identity().bodyType() ] ) );
				}
				else
				{
					(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().animationHeightAdjustment() - gClimbUpRoofDist[ pSoldier->identity().bodyType() ] ) );
				}
				break;

			case 457:

				// CODE: CHANGCE STANCE TO STANDING
				SendChangeSoldierStanceEvent( pSoldier, ANIM_STAND );
				break;

			case 459:

				// CODE: CHANGE ATTACKING TO FIRST HAND
				pSoldier->attackSelection().selectWeapon(
					HANDPOS, pSoldier->inventory()[HANDPOS].usItem);
				// Clear the fire-control reload state.
				pSoldier->fireControl().reloading() = FALSE;
				break;

			case 458:

				// CODE: CHANGE ATTACKING TO SECOND HAND
				pSoldier->attackSelection().selectWeapon(
					SECONDHANDPOS, pSoldier->inventory()[SECONDHANDPOS].usItem);
				// Clear the fire-control reload state.
				pSoldier->fireControl().reloading() = FALSE;
				break;

			case 460:
			case 461:

				//CODE: THROW ITEM
				// Launch ITem!
				if ( pSoldier->pendingItem().readyToThrow() )
				{
					// ATE: If we are armmed...
					if ( pSoldier->pendingItem().throwParameters()->ubActionCode == THROW_ARM_ITEM )
					{
						//AXP 25.03.2007: MinAPsToThrow now actually returns the real cost, not 0
						// ATE: Deduct points!
						DeductPoints( pSoldier, MinAPsToThrow( pSoldier, pSoldier->targeting().gridNo(), FALSE ), 0, AFTERACTION_INTERRUPT );
					}
					else
					{
						// ATE: Deduct points!
						DeductPoints( pSoldier, APBPConstants[AP_TOSS_ITEM], 0, AFTERACTION_INTERRUPT );
					}

					// sevenfm: show flash light
					UINT16 usItem = pSoldier->pendingItem().object()->usItem;
					UINT16 usBuddyItem = Item[usItem].usBuddyItem;
					if (pSoldier->pendingItem().throwParameters()->ubActionCode == THROW_ARM_ITEM &&
						(ItemIsFlare(usItem) ||
						Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_FLARE ||
						Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_BURNABLEGAS ||
						usBuddyItem && (Item[usBuddyItem].usItemClass & IC_EXPLOSV) && (ItemIsFlare(usBuddyItem) || Explosive[Item[usBuddyItem].ubClassIndex].ubType == EXPLOSV_FLARE)))
					{
						if ((pSoldier->renderState().muzzleFlashSprite() = LightSpriteCreate("L-R03.LHT", 0)) != -1)
						{
							LightSpritePower(pSoldier->renderState().muzzleFlashSprite(), TRUE);

							INT32	usNewGridNo;
							INT16 sXPos, sYPos;

							usNewGridNo = NewGridNo(pSoldier->position().gridNo(), DirectionInc(pSoldier->position().direction()));
							ConvertGridNoToCenterCellXY(usNewGridNo, &sXPos, &sYPos);
							LightSpritePosition(pSoldier->renderState().muzzleFlashSprite(), (INT16)(sXPos / CELL_X_SIZE), (INT16)(sYPos / CELL_Y_SIZE));

							// Start count
							pSoldier->renderState().startMuzzleFlashSprite(
								pSoldier->renderState().muzzleFlashSprite());
						}
					}

					INT32 iRealObjectID = CreatePhysicalObject( pSoldier->pendingItem().object(), pSoldier->pendingItem().throwParameters()->dLifeSpan,	pSoldier->pendingItem().throwParameters()->dX, pSoldier->pendingItem().throwParameters()->dY, pSoldier->pendingItem().throwParameters()->dZ, pSoldier->pendingItem().throwParameters()->dForceX, pSoldier->pendingItem().throwParameters()->dForceY, pSoldier->pendingItem().throwParameters()->dForceZ, pSoldier->identity().id(), pSoldier->pendingItem().throwParameters()->ubActionCode, pSoldier->pendingItem().throwParameters()->uiActionData, FALSE );

					// OJW - 20091002 - Explosives
					if (is_networked && is_client)
					{
						if (pSoldier->roster().team() == 0 || (pSoldier->roster().team() == 1 && is_server))
						{
							send_grenade( pSoldier->pendingItem().object(), pSoldier->pendingItem().throwParameters()->dLifeSpan,	pSoldier->pendingItem().throwParameters()->dX, pSoldier->pendingItem().throwParameters()->dY, pSoldier->pendingItem().throwParameters()->dZ, pSoldier->pendingItem().throwParameters()->dForceX, pSoldier->pendingItem().throwParameters()->dForceY, pSoldier->pendingItem().throwParameters()->dForceZ, pSoldier->targeting().gridNo(), pSoldier->identity().id(), pSoldier->pendingItem().throwParameters()->ubActionCode, pSoldier->pendingItem().throwParameters()->uiActionData, iRealObjectID, true);
						}
					}

					// Remove object
					//RemoveObjFrom( &(pSoldier->inventory()[ HANDPOS ] ), 0 );

					// Update UI
					DirtyMercPanelInterface( pSoldier, DIRTYLEVEL2 );

					pSoldier->pendingItem().clearThrowTransaction();
				}
				break;

			case 462:

				// CODE: MOVE UP FROM CLIFF CLIMB
				pSoldier->position().animationHeightAdjustment() += (float)2.1;
				pSoldier->position().heightAdjustment() = (INT16)pSoldier->position().animationHeightAdjustment();
				// Move over some...
				//MoveMercFacingDirection( pSoldier , FALSE, (FLOAT)0.5 );
				break;

			case 463:

				// MOVE GUY FORWARD SOME VALUE
				// Creature move
				MoveMercFacingDirection( pSoldier, FALSE, (FLOAT)1.5 );
				break;

			case 464:

				// CODE: END CLIFF CLIMB
				pSoldier->position().animationHeightAdjustment() = (float)0;
				pSoldier->position().heightAdjustment() = (INT16)pSoldier->position().animationHeightAdjustment();

				// Set new gridno
				{
					INT32 sTempGridNo;
					INT16 sNewX, sNewY;

					//Get Next GridNo;
					sTempGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc(pSoldier->position().direction() ) );

					// Get center XY
					ConvertGridNoToCenterCellXY( sTempGridNo, &sNewX, &sNewY );

					// Set position
					(void)TacticalActorWorldPlacement::setPosition(*pSoldier, sNewX, sNewY );

					// Move two CC directions
					(void)TacticalActorOrientation::setDirection(*pSoldier, gTwoCCDirection[ pSoldier->position().direction() ] );
					(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

					// Set desired anim height!
					pSoldier->animationIntent().desiredHeight() = ANIM_CROUCH;
					pSoldier->pathing().finalDestinationGrid() = pSoldier->position().gridNo();

				}
				break;

			case 465:

				// CODE: SET GUY TO LIFE OF 0
				pSoldier->vitals().health() = 0;
				break;

			case 466:

				// CODE: ADJUST TO OUR DEST HEIGHT
				if ( pSoldier->position().heightAdjustment() != pSoldier->position().desiredHeight() )
				{
					INT16 sDiff = pSoldier->position().heightAdjustment() - pSoldier->position().desiredHeight();

					if ( abs( sDiff ) > 4 )
					{
						if ( sDiff > 0 )
						{
							// Adjust!
							(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().animationHeightAdjustment() - 2 ) );
						}
						else
						{
							// Adjust!
							(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().animationHeightAdjustment() + 2 ) );
						}
					}
					else
					{
						// Adjust!
						(void)TacticalActorWorldPlacement::setHeight(*pSoldier, (FLOAT)(pSoldier->position().desiredHeight()) );
					}
				}
				else
				{
					// Goto eating animation
					if ( pSoldier->position().desiredHeight() == 0 )
					{
						TacticalActorAnimationTransitions::changeState(*pSoldier,  CROW_EAT, 0 , FALSE );
					}
					else
					{
						// We should leave now!
						TacticalRemoveSoldier( pSoldier->identity().id() );
						return( FALSE );
					}
					return( TRUE );
				}
				break;

			case 467:

				///CODE: FOR HELIDROP, SET DIRECTION
				(void)TacticalActorOrientation::setDirection(*pSoldier, EAST );
				(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

				gfIngagedInDrop = FALSE;

				// OK, now get a sweetspot ( not the place we are now! )
				//	sNewGridNo =	FindGridNoFromSweetSpotExcludingSweetSpot( pSoldier, pSoldier->sGridNo, 5, &ubNewDirection );
				//sNewGridNo =	FindRandomGridNoFromSweetSpotExcludingSweetSpot( pSoldier, pSoldier->sGridNo, 3, &ubNewDirection );

				sNewGridNo =	FindGridNoFromSweetSpotExcludingSweetSpotInQuardent( pSoldier, pSoldier->position().gridNo(), 3, &ubNewDirection, SOUTHEAST );

				// Check for merc arrives quotes...
				HandleMercArrivesQuotes( pSoldier );

				// Find a path to it!
				(void)TacticalActorRouteExecution::requestPath(*pSoldier, sNewGridNo, WALKING );

				return( TRUE );
				break;

			case 468:

				// CODE: End PUNCH
				{
					BOOLEAN fNPCPunch = FALSE;

					// ATE: Put some code in for NPC punches...
					if ( pSoldier->status().flags() & SOLDIER_NPC_DOING_PUNCH )
					{
						fNPCPunch = TRUE;

						// Turn off
						pSoldier->status().flags() &= (~SOLDIER_NPC_DOING_PUNCH );

						// Trigger approach...
						TriggerNPCWithGivenApproach( pSoldier->identity().profile(), (UINT8)pSoldier->pendingAction().quaternaryData(), FALSE );
					}


					// Are we a martial artist?
					{
						BOOLEAN fMartialArtist = FALSE;

						if ( pSoldier->identity().profile() != NO_PROFILE && pSoldier->identity().bodyType() == REGMALE ) // SANDRO - added check for body type
						{
							// SANDRO - old/new traits
							if (gGameOptions.fNewTraitSystem)
							{
								if ( NUM_SKILL_TRAITS( pSoldier, MARTIAL_ARTS_NT ) >= ((gSkillTraitValues.fPermitExtraAnimationsOnlyToMA) ? 2 : 1 ) )
								{
									fMartialArtist = TRUE;
								}
							}
							else
							{
								if ( ProfileHasSkillTrait( pSoldier->identity().profile(), MARTIALARTS_OT ) > 0 )
								{
									fMartialArtist = TRUE;
								}
							}
						}

						if ( gAnimControl[ pSoldier->animationPlayback().state() ].ubHeight == ANIM_CROUCH )
						{
							if ( fNPCPunch )
							{
								(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_STAND );
								return( TRUE );
							}
							else
							{
								TacticalActorAnimationTransitions::changeState(*pSoldier,  CROUCHING, 0, FALSE );
								return( TRUE );
							}
						}
						else
						{
#ifdef JA2UB
					//Ja25 No meanwhiles		
					          	if ( fMartialArtist )
#else
							if ( fMartialArtist && !AreInMeanwhile( ) )

#endif
							{
								TacticalActorAnimationTransitions::changeState(*pSoldier,  NINJA_BREATH, 0, FALSE );
								return( TRUE );
							}
							else
							{
								TacticalActorAnimationTransitions::changeState(*pSoldier,  PUNCH_BREATH, 0, FALSE );
								return( TRUE );
							}
						}
					}
				}
				break;

			case 469:

				// CODE: Begin martial artist attack
				(void)TacticalActorCombatActions::
					continueNinjaAttack(*pSoldier);
				return( TRUE );
				break;

			case 470:

				// CODE: CHECK FOR OK WEAPON SHOT!
				bOKFireWeapon =	OKFireWeapon( pSoldier );

				if ( bOKFireWeapon == FALSE )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Fire Weapon: Gun Cannot fire, code 470") );

					// OK, SKIP x # OF FRAMES
					// Skip 3 frames, ( a third ia added at the end of switch.. ) For a total of 4
					pSoldier->animationPlayback().code() += 4;

					// Reduce by a bullet...
					//						pSoldier->fireControl().bulletsLeft()--;

					PlayJA2Sample( S_DRYFIRE1, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );

					// Free-up!
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Freeing up attacker - gun failed to fire") );
					FreeUpAttacker( );

				}
				else if ( bOKFireWeapon == 255 )
				{
					// Play intermediate animation...
					if ( HandleUnjamAnimation( pSoldier ) )
					{
						return( TRUE );
					}
				}
				break;

			case 471:

				// CODE: Turn pause move flag off
				pSoldier->status().flags() &= (~SOLDIER_PAUSEANIMOVE);
				break;

			case 472:

				{
					INT8 bGoBackToAimAfterHit;

					// Save old flag, then reset. If we do nothing special here, at least go back
					// to aim if we were.
					bGoBackToAimAfterHit = pSoldier->animationActivity().postHitStance();
					pSoldier->animationActivity().postHitStance() = NO_SPEC_STANCE_AFTER_HIT;

					// CODE: HANDLE ANY RANDOM HIT VARIATIONS WE WISH TO DO.....
					if ( pSoldier->vitals().health() >= OKLIFE && bGoBackToAimAfterHit)
					{
						if ( bGoBackToAimAfterHit == GO_TO_AIM_AFTER_HIT )
						{				
							(void)TacticalActorRangedActions::readyFacing(
								*pSoldier,
								pSoldier->position().direction(),
								false,
								false);
						}
						else if ( bGoBackToAimAfterHit == GO_TO_ALTERNATIVE_AIM_AFTER_HIT && (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_STAND) )
						{						
							(void)TacticalActorRangedActions::readyFacing(
								*pSoldier,
								pSoldier->position().direction(),
								false,
								true);
						}
						else if ( bGoBackToAimAfterHit == GO_TO_HTH_BREATH_AFTER_HIT && (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_STAND))
						{						
							if ( Item[ pSoldier->inventory()[HANDPOS].usItem ].usItemClass & (IC_NONE | IC_PUNCH) )
							{
								if ((((NUM_SKILL_TRAITS( pSoldier, MARTIAL_ARTS_NT ) >= ((gSkillTraitValues.fPermitExtraAnimationsOnlyToMA) ? 2 : 1 )) && gGameOptions.fNewTraitSystem ) ||
									(HAS_SKILL_TRAIT( pSoldier, MARTIALARTS_OT ) && !gGameOptions.fNewTraitSystem ) ) && pSoldier->identity().bodyType() == REGMALE )
								{
									if(is_networked)
										TacticalActorAnimationTransitions::changeState(*pSoldier,  NINJA_BREATH, 0 , FALSE );
									else
										TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  NINJA_BREATH, 0 , FALSE );
								}
								else
								{
									if(is_networked)
										TacticalActorAnimationTransitions::changeState(*pSoldier,  PUNCH_BREATH, 0 , FALSE );
									else
										TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  PUNCH_BREATH, 0 , FALSE );
								}
							}
							else if ( Item[ pSoldier->inventory()[HANDPOS].usItem ].usItemClass & (IC_BLADE) )
							{
								if(is_networked)
									TacticalActorAnimationTransitions::changeState(*pSoldier,  KNIFE_BREATH, 0 , FALSE );
								else
									TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  KNIFE_BREATH, 0 , FALSE );
							}
						}
						else if ( bGoBackToAimAfterHit == GO_TO_COWERING_AFTER_HIT )
						{			
							if (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_CROUCH)
							{
								if(is_networked)
									TacticalActorAnimationTransitions::changeState(*pSoldier,  START_COWER_CROUCHED, 0 , FALSE );
								else
									TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  START_COWER_CROUCHED, 0 , FALSE );
							}
							else if (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_PRONE)
							{
								if(is_networked)
									TacticalActorAnimationTransitions::changeState(*pSoldier,  START_COWER_PRONE, 0 , FALSE );
								else
									TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  START_COWER_PRONE, 0 , FALSE );
							}
						}
						return( TRUE );
					}
				}
				break;

			case 473:

				// CODE: CHECK IF WE HAVE JUST JAMMED / OUT OF AMMO, DONOT CONTINUE, BUT
				// GOTO STATIONARY ANIM
				if ( CheckForImproperFireGunEnd( pSoldier ) )
				{
					return( TRUE );
				}
				break;

			case 474:

				// CODE: GETUP FROM SLEEP
				(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_STAND );
				return( TRUE );

			case 475:

				// CODE: END CLIMB DOWN ROOF
				pSoldier->animationIntent().desiredHeight() = ANIM_STAND;
				pSoldier->pathing().finalDestinationGrid() = pSoldier->position().gridNo();

				// re-enable sight
				gTacticalStatus.uiFlags &= (~DISALLOW_SIGHT);

				// ATE: Change interface level.....
				// CJC: only if we are a player merc
				if ( (pSoldier->roster().team() == gbPlayerNum) && !gTacticalStatus.fAutoBandageMode)
				{
					if ( pSoldier->identity().id() == gusSelectedSoldier )
					{
						ChangeInterfaceLevel( 0 );
					}
					// OK, UNSET INTERFACE FIRST
					UnSetUIBusy( pSoldier->identity().id() );
				}
				else
				{
					FreeUpNPCFromRoofClimb( pSoldier );
				}
				if(pSoldier->movement().mode() != RUNNING)
					pSoldier->movement().mode() = WALKING;

				// ATE: Handle sight...
				HandleSight( pSoldier,SIGHT_LOOK | SIGHT_RADIO | SIGHT_INTERRUPT );
				break;

			case 476:

				// CODE: GOTO PREVIOUS ANIMATION
				TacticalActorAnimationTransitions::changeState(*pSoldier,  ( pSoldier->pendingAction().secondaryData() ), (UINT8)( pSoldier->pendingAction().primaryData() + 1 ), FALSE );
				return( TRUE );
				break;


			case 477:

				// CODE: Locate to target ( if an AI guy.. )
				if ( IsJa2TacticalTurnBasedCombat() )
				{
					if ( pSoldier->roster().team() != gbPlayerNum )
					{
						const TacticalActor* target =
							GetJa2SoldierRepository().resolve( pSoldier->targeting().targetId() );

						// only locate if the enemy is visible or he's aiming at a player
						if ( pSoldier->awareness().visibility() != -1 ||
							(target != nullptr && target->roster().team() == gbPlayerNum) )
						{
							LocateGridNo( pSoldier->targeting().gridNo() );
						}
					}
				}
				break;

			case 478:

				// CODE: Decide to turn from hit.......
				{
					INT8		bNewDirection;
					UINT32	uiChance;

					// ONLY DO THIS IF CERTAIN CONDITIONS ARISE!
					// For one, only do for mercs!
					// Flugente: don't do this while equipping a shield, as this renders them almost useless
					if ( pSoldier->identity().bodyType() <= REGFEMALE && !TacticalActorEquipment::hasEquippedRiotShield(*pSoldier) )
					{
						// Secondly, don't if we are going to collapse
						if ( pSoldier->vitals().health() >= OKLIFE && pSoldier->vitals().breath() > 0 && pSoldier->position().level() == 0 )
						{
							// Save old direction
							pSoldier->pendingAction().primaryData() = pSoldier->position().direction();

							// If we got a head shot...more chance of turning...
							if ( pSoldier->combatResult().hitLocation() != AIM_SHOT_HEAD )
							{
								uiChance = Random( 100 );

								// 30 % chance to change direction one way
								if ( uiChance	< 30 )
								{
									bNewDirection = gOneCDirection[ pSoldier->position().direction() ];
								}
								// 30 % chance to change direction the other way
								else if ( uiChance >= 30 && uiChance < 60 )
								{
									bNewDirection = gOneCCDirection[ pSoldier->position().direction() ];
								}
								// 30 % normal....
								else
								{
									bNewDirection = pSoldier->position().direction();
								}

								(void)TacticalActorOrientation::setDirection(*pSoldier, bNewDirection );
								(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, pSoldier->position().direction() );

							}
							else
							{
								// OK, 50% chance here to turn...
								uiChance = Random( 100 );

								if ( uiChance < 50 )
								{
									// OK, pick a larger direction to goto....
									pSoldier->status().flags() |= SOLDIER_TURNINGFROMHIT;
									// This becomes an attack busy situation
									// 0verhaul:  There is an attack busy problem with this.  The soldier could be in mid-turn
									// when another bullet is fired (auto-fire or dual-wield, for instance), and the soldier is
									// knocked down in the middle of the turn.  In such a case, the attack busy does not get
									// cancelled.  So if we indeed need to keep the attack busy (which may not be the case),
									// we will need to find a more reliable method.  For now, I'm going to cancel out the
									// ABC adjustment here and we'll see if there needs to be something in its place.
									//GetJa2PendingTacticalCombatActions()++;
									DebugAttackBusy( String( "Soldier turning from a hit.  Not Increasing attack busy.  Now %d\n", GetJa2PendingTacticalCombatActions() ) );

									// Pick evenly between both
									if ( Random( 50 ) < 25 )
									{
										bNewDirection = gOneCDirection[ pSoldier->position().direction() ];
										bNewDirection = gOneCDirection[ bNewDirection ];
										bNewDirection = gOneCDirection[ bNewDirection ];
									}
									else
									{
										bNewDirection = gOneCCDirection[ pSoldier->position().direction() ];
										bNewDirection = gOneCCDirection[ bNewDirection ];
										bNewDirection = gOneCCDirection[ bNewDirection ];
									}

									(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, bNewDirection );
								}
							}
						}
					}
					break;
				}

			case 479:

				// CODE: Return to old direction......
				if ( pSoldier->identity().bodyType() <= REGFEMALE )
				{
					// Secondly, don't if we are going to collapse
					//if ( pSoldier->vitals().health() >= OKLIFE && pSoldier->vitals().breath() > 0 )
					//{
					///	if ( !( pSoldier->status().flags() & SOLDIER_TURNINGFROMHIT ) )
					//	{
					///		pSoldier->ubDirection				= (INT8)pSoldier->pendingAction().primaryData();
					//		pSoldier->pathing().desiredDirection() = (INT8)pSoldier->pendingAction().primaryData();
					//	}
					//}
				}
				break;

			case 480:

				// 0verhaul:	This is handled in the ReduceAttackBusyCount call
				// CODE: FORCE FREE ATTACKER
				// Release attacker
				//DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, code 480") );

				//ReleaseSoldiersAttacker( pSoldier );

				//FREEUP GETTING HIT FLAG
				//pSoldier->animationActivity().hitPhase() = FALSE;
				break;

			case 481:

				// CODE: CUT FENCE...
				CutWireFence( pSoldier->targeting().gridNo() );
				break;

			case 482:

				// CODE: END CRIPPLE KICKOUT...
				KickOutWheelchair( pSoldier );
				break;

			case 483:

				// CODE: HANDLE DROP BOMB...
				HandleSoldierDropBomb( pSoldier, pSoldier->pendingAction().secondaryData() );
				break;

			case 484:

				// CODE: HANDLE REMOTE...
				HandleSoldierUseRemote( pSoldier, pSoldier->pendingAction().secondaryData() );
				break;

			case 485:

				// CODE: Try steal.....
//				UnSetUIBusy( pSoldier->identity().id());
				UseHandToHand( pSoldier, pSoldier->pendingAction().secondaryData(), TRUE );
				//jackaians:
				//if we are not waiting for the pickup menu to be displayed
//				if (guiPendingOverrideEvent != G_GETTINGITEM)
//				{
//					PreventFromTheFreezingBug(pSoldier);
//				}
				break;

			case 486:

				// CODE: GIVE ITEM
				SoldierGiveItemFromAnimation( pSoldier );
			//	if (pSoldier->identity().profile() != NO_PROFILE && pSoldier->identity().profile() >= FIRST_NPC )
				//new profiles by Jazz	
				if (pSoldier->identity().profile() != NO_PROFILE && ( gMercProfiles[pSoldier->identity().profile()].Type == PROFILETYPE_RPC ||
					gMercProfiles[pSoldier->identity().profile()].Type == PROFILETYPE_NPC ||
					gMercProfiles[pSoldier->identity().profile()].Type == PROFILETYPE_VEHICLE ) )
				{
					TriggerNPCWithGivenApproach( pSoldier->identity().profile(), APPROACH_DONE_GIVING_ITEM, FALSE );
				}
				break;

			case 487:

				// CODE: DROP ITEM
				SoldierHandleDropItem( pSoldier );
				break;

			case 489:


				//CODE: REMOVE GUY FRMO WORLD DUE TO EXPLOSION
				//TacticalActorAnimationTransitions::changeState(*pSoldier,  RAISE_RIFLE, 0 , FALSE );
				//return( TRUE );
				//Delete guy
				//TacticalRemoveSoldier( pSoldier->identity().id() );
				//return( FALSE );
				break;

			case 490:

				// CODE: HANDLE END ITEM PICKUP
				//LOOK INTO HAND, RAISE RIFLE IF WE HAVE ONE....
				/*
				if ( pSoldier->inventory()[ HANDPOS ].exists() == true )
				{
				// CHECK IF GUN
				if ( Item[ pSoldier->inventory()[ HANDPOS ].usItem ].usItemClass == IC_GUN )
				{
				if ( Weapon[ pSoldier->inventory()[ HANDPOS ].usItem ].ubWeaponClass != HANDGUNCLASS )
				{
				// RAISE
				TacticalActorAnimationTransitions::changeState(*pSoldier,  RAISE_RIFLE, 0 , FALSE );
				return( TRUE );
				}

				}

				}
				*/
				break;

			case 491:
				// SANDRO - I've been here, messing with stuff...

				// CODE: HANDLE RANDOM BREATH ANIMATION
				//if ( pSoldier->vitals().health() > INJURED_CHANGE_THREASHOLD )
				if ( pSoldier->vitals().health() >= OKLIFE )
				{
					// Increment time from last update
					pSoldier->animationActivity().advanceRandomActionCheck();

					if ( pSoldier->animationActivity().randomActionCheckDue(TIME_FOR_RANDOM_ANIM_CHECK) || pSoldier->vitals().health() < INJURED_CHANGE_THREASHOLD || GetDrunkLevel( pSoldier ) >= BORDERLINE )
					{
						pSoldier->animationActivity().resetRandomActionCheck();

						// Don't play these generally if this is the guy selected by player, as this one is "awaiting orders"
						if (pSoldier->identity().id() != gusSelectedSoldier || Random( 10 ) == 0 )
						{
							// Don't do any in water!
							// Also don't play if we are in the middle of something
							if ( !TacticalActorMobility::inWater(*pSoldier) && !pSoldier->animationActivity().turningUntilDone() )
							{
								// OK, make a dice roll
								ubDiceRoll = (UINT8)Random( 100 );

								// Determine what is in our hand;
								usItem = pSoldier->inventory()[ HANDPOS ].usItem;

								// Default to nothing in hand ( nothing in quotes, we do have something but not just visible )
								ubRandomHandIndex = RANDOM_ANIM_NOTHINGINHAND;

								if ( pSoldier->inventory()[ HANDPOS ].exists() == true )
								{
									if ( Item[ usItem ].usItemClass == IC_GUN )
									{
										//										if ( (Item[ usItem ].fFlags & ITEM_TWO_HANDED) )
										if (ItemIsTwoHanded(usItem))
										{
											// Set to rifle
											ubRandomHandIndex = RANDOM_ANIM_RIFLEINHAND;
										}
										//else
										//{
										//	// Don't EVER do a trivial anim...
										//	break;
										//	ubRandomHandIndex = RANDOM_ANIM_NOTHINGORPISTOLINHAND;
										//}
									}
								}

								// Check which animation to play....
								for ( cnt = 0; cnt < MAX_RANDOM_ANIMS_PER_BODYTYPE; cnt++ )
								{
									pAnimDef = &( gRandomAnimDefs[ pSoldier->identity().bodyType() ][ cnt ] );

									if ( pAnimDef->sAnimID	!= 0 )
									{
										BOOLEAN fStarving = FALSE;
										UINT8 foodsituation;
										UINT8 watersituation;
										GetFoodSituation( pSoldier, &foodsituation, &watersituation );
										if ( foodsituation >= FOOD_VERY_LOW || watersituation >= FOOD_VERY_LOW )
											fStarving = TRUE;

										// If it's an injured animation and we are not in the threashold....
										if ( ( pAnimDef->ubFlags & RANDOM_ANIM_INJURED ) && pSoldier->vitals().health() >= INJURED_CHANGE_THREASHOLD && !fStarving )
										{
											continue;
										}

										// If we need to do an injured one, don't do any others...
										if ( !( pAnimDef->ubFlags & RANDOM_ANIM_INJURED ) && (pSoldier->vitals().health() < INJURED_CHANGE_THREASHOLD || fStarving) )
										{
											continue;
										}

										// If it's a drunk animation and we are not in the threashold....
										if ( ( pAnimDef->ubFlags & RANDOM_ANIM_DRUNK ) && GetDrunkLevel( pSoldier ) < BORDERLINE && !fStarving )
										{
											continue;
										}

										// If we need to do an injured one, don't do any others...
										if ( !( pAnimDef->ubFlags & RANDOM_ANIM_DRUNK ) && (GetDrunkLevel( pSoldier ) >= BORDERLINE || fStarving) )
										{
											continue;
										}

										// Check if it's our hand
										/*if ( pAnimDef->ubHandRestriction != RANDOM_ANIM_IRRELEVENTINHAND && pAnimDef->ubHandRestriction != ubRandomHandIndex )
										{
											continue;
										}*/

										// Don't do this if a pistol in hand
										if ( ubRandomHandIndex == RANDOM_ANIM_RIFLEINHAND && pAnimDef->sAnimID == BIGGUY_STONE )
										{
											continue;
										}

										// Check if it's casual and we're in combat and it's not our guy
										if ( ( pAnimDef->ubFlags & RANDOM_ANIM_CASUAL ) )
										{
											// If he's a bad guy, do not do it!
											if ( pSoldier->roster().team() != gbPlayerNum	|| ( IsJa2TacticalCombatActive() ) )
											{
												continue;
											}
										}
										
										// If it is lookaround animation, don't play it if we see at least one enemy
										if ( pAnimDef->ubFlags & RANDOM_ANIM_LOOKAROUND )
										{
											// enemy on sight, don't pretend we don't see him!
											if ( pSoldier->awareness().opponentCount() > 0 )
											{
												continue;
											}
										}
										
										// If it is lookaround animation, don't play it if we see at least one enemy
										if ( pAnimDef->ubFlags & RANDOM_ANIM_SHOWOFF )
										{
											// enemy on sight, don't pretend we don't see him!
											if ( pSoldier->identity().profile() != NO_PROFILE )
											{
												if ( Random( 10 ) < 9 && !DoesMercHavePersonality( pSoldier, CHAR_TRAIT_SHOWOFF ) )
												{
													continue;
												}
											}
											else 
											{
												if ( Random( 10 ) < 7 )
												{
													continue;
												}
											}
										}

										// If we are an alternate big guy and have been told to use a normal big merc ani...
										//if ( ( pAnimDef->ubFlags & RANDOM_ANIM_FIRSTBIGMERC ) && ( pSoldier->animationPlayback().subFlags() & SUB_ANIM_BIGGUYTHREATENSTANCE ) )
										if ( ( pAnimDef->ubFlags & RANDOM_ANIM_FIRSTBIGMERC ) && !( DecideAltAnimForBigMerc( pSoldier )) )
										{
											continue;
										}

										// If we are a normal big guy and have been told to use an alternate big merc ani...
										//if ( ( pAnimDef->ubFlags & RANDOM_ANIM_SECONDBIGMERC ) && !( pSoldier->animationPlayback().subFlags() & SUB_ANIM_BIGGUYTHREATENSTANCE ) )
										if ( ( pAnimDef->ubFlags & RANDOM_ANIM_SECONDBIGMERC ) && ( DecideAltAnimForBigMerc( pSoldier )) )
										{
											continue;
										}

										// Check if it's the proper height
										if ( pAnimDef->ubAnimHeight == gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight )
										{
											// OK, If we rolled a value that lies within the range for this random animation, use this one!
											if ( ubDiceRoll >= pAnimDef->ubStartRoll && ubDiceRoll <= pAnimDef->ubEndRoll )
											{
												// Are we playing a sound
												if ( pAnimDef->sAnimID == RANDOM_ANIM_SOUND )
												{
													if ( pSoldier->identity().bodyType() == COW )
													{
														if ( Random( 10 ) == 1 )
														{
															if ( ( IsJa2TacticalCombatActive() ) && pSoldier->awareness().visibility() == -1 )
															{
																// DO this every 10th time or so...
																if ( Random( 100 ) < 10 )
																{
																	// Play sound
																	if (pSoldier->awareness().visibility() == TRUE || TeamMemberNear(gbPlayerNum, pSoldier->position().gridNo(), TACTICAL_RANGE))
																		PlayJA2SampleFromFile(	pAnimDef->zSoundFile, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
																}
															}
															else
															{

																// Play sound
																if (pSoldier->awareness().visibility() == TRUE || TeamMemberNear(gbPlayerNum, pSoldier->position().gridNo(), TACTICAL_RANGE))
																	PlayJA2SampleFromFile( pAnimDef->zSoundFile, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
															}
														}
													}
													else if ( pSoldier->identity().bodyType() == CROW )
													{
														if ( Random( 4 ) == 1 )
														{
															if ( ( IsJa2TacticalCombatActive() ) && pSoldier->awareness().visibility() == -1 )
															{
																// DO this every 10th time or so...
																if ( Random( 100 ) < 10 )
																{
																	// Play sound
																	if (pSoldier->awareness().visibility() == TRUE || TeamMemberNear(gbPlayerNum, pSoldier->position().gridNo(), TACTICAL_RANGE))
																		PlayJA2SampleFromFile(	pAnimDef->zSoundFile, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
																}
															}
															else
															{

																// Play sound
																if (pSoldier->awareness().visibility() == TRUE || TeamMemberNear(gbPlayerNum, pSoldier->position().gridNo(), TACTICAL_RANGE))
																	PlayJA2SampleFromFile( pAnimDef->zSoundFile, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
															}
														}
													}
													else
													{
														// Play sound
														if (pSoldier->awareness().visibility() == TRUE || TeamMemberNear(gbPlayerNum, pSoldier->position().gridNo(), TACTICAL_RANGE))
															PlayJA2SampleFromFile( pAnimDef->zSoundFile, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
													}
												}
												else
												{
													if ( pAnimDef->ubHandRestriction != RANDOM_ANIM_IRRELEVENTINHAND && pAnimDef->ubHandRestriction != ubRandomHandIndex )
													{
														// only if we are told to play an anim without rifle and we have a rifle (we can lower it for an instance), not vice versa
														if ( pAnimDef->ubHandRestriction == RANDOM_ANIM_RIFLEINHAND && ubRandomHandIndex != RANDOM_ANIM_RIFLEINHAND )
														{
															continue;
														}
														// this is not for enemy fools, only for cool mercs
														if ( pSoldier->roster().team() != gbPlayerNum )
														{
															continue;
														}
														// these funny moves are not likely to be used in combat
														if ( ( IsJa2TacticalCombatActive() ) )
														{
															if ( pSoldier->morale().morale() < 95 ) // .. unless we are really confident about ourselves
															{ 
																continue;
															}
															else 
															{ 
																if ( Random( 2 ) == 1 ) // even if we are, still make them show seldomly
																	continue;
															}
														}
														if ( Random( 4 ) == 1 ) // make this rare as we need to lower the weapon -> make the move -> raise the weapon again... rather show off
														{ 
															continue;
														}
													}
													// generally make funny moves less common in combat, we need to focus!
													if ( ( IsJa2TacticalCombatActive() ) && !( pAnimDef->ubFlags & ( RANDOM_ANIM_INJURED )) && !( pAnimDef->ubFlags & ( RANDOM_ANIM_DRUNK )) && !( pAnimDef->ubFlags & ( RANDOM_ANIM_LOOKAROUND )) )
													{
														if ( Random( 3 ) == 1 )
															continue;
													}
													// finally if we got here, send state change
													TacticalActorAnimationTransitions::changeState(*pSoldier,  pAnimDef->sAnimID, 0 , FALSE );
												}
												return( TRUE );
											}
										}
									}
								}
							}
						}
					}
				}
				break;

			case 492:


				// SIGNAL DODGE!
				// ATE: Only do if we're not inspecial case...
				if ( !( pSoldier->status().flags() & SOLDIER_NPC_DOING_PUNCH ) )
				{
					TacticalActor *pTSoldier;
					UINT32 uiMercFlags;
					SoldierID usSoldierIndex;

					if ( FindSoldier( pSoldier->targeting().gridNo(), &usSoldierIndex, &uiMercFlags, FIND_SOLDIER_GRIDNO ) )
					{
						GetSoldier( &pTSoldier, usSoldierIndex );

						// IF WE ARE AN ANIMAL, CAR, MONSTER, DONT'T DODGE
						if ( IS_MERC_BODY_TYPE( pTSoldier ) )
						{
							// ONLY DODGE IF WE ARE SEEN
							if ( pTSoldier->awareness().opponentKnowledge()[ pSoldier->identity().id() ] != 0 || pTSoldier->roster().team() == pSoldier->roster().team() )
							{
								if ( gAnimControl[ pTSoldier->animationPlayback().state() ].ubHeight == ANIM_STAND )
								{
									// OK, stop merc....
									(void)TacticalActorRouteExecution::stopAt(*pTSoldier, pTSoldier->position().gridNo(), pTSoldier->position().direction() );

									if ( pTSoldier->roster().team() != gbPlayerNum )
									{
										DebugAI(AI_MSG_INFO, pTSoldier, String("CancelAIAction: dodge"));
										CancelAIAction( pTSoldier, TRUE );
									}

									// SANDRO - Set goback to aim after hit flag
									if (( Item[ pTSoldier->inventory()[HANDPOS].usItem ].usItemClass & (IC_BLADE | IC_PUNCH | IC_NONE) ) && pTSoldier->vitals().health() > 30 && pTSoldier->vitals().breath() > 25 && (gAnimControl[ pTSoldier->animationPlayback().state() ].ubEndHeight == ANIM_STAND) )
									{
										if ( pTSoldier->vitals().health() > 30 && pTSoldier->vitals().breath() > 25 )
										{
											pTSoldier->animationActivity().postHitStance() = GO_TO_HTH_BREATH_AFTER_HIT;
										}
									}
									// If we were aiming
									// actually no... favor htH a bit more, by making the opponent drop his readied stance when attacked in close combat, regardless he will dodge it or not
									//else if ( gAnimControl[ pTSoldier->animationPlayback().state() ].uiFlags & ANIM_FIREREADY )
									//{
									//	if ( gAnimControl[ pTSoldier->animationPlayback().state() ].uiFlags & ANIM_ALT_WEAPON_HOLDING ) // alternative weapon holding stance
									//		pTSoldier->animationActivity().postHitStance() = GO_TO_ALTERNATIVE_AIM_AFTER_HIT;
									//	else // standard
									//		pTSoldier->animationActivity().postHitStance() = GO_TO_AIM_AFTER_HIT;
									//}
									// if we were cowering (this is different from the bellow, we don't use that status flag for this animation)
									else if ( pTSoldier->animationPlayback().state() == COWERING )
									{
										pTSoldier->animationActivity().postHitStance() = GO_TO_COWERING_AFTER_HIT;
									}
									else 
									{
										pTSoldier->animationActivity().postHitStance() = NO_SPEC_STANCE_AFTER_HIT;
									}

									// Turn towards the person!
									(void)TacticalActorOrientation::setDesiredDirection(*pTSoldier, GetDirectionFromGridNo( pSoldier->position().gridNo(), pTSoldier ) );

									// PLAY SOLDIER'S DODGE ANIMATION
									TacticalActorAnimationTransitions::changeState(*pTSoldier,  DODGE_ONE, 0 , FALSE );

									// SANDRO - after dodging melee attack go to apropriate stance
									//if ( (gAnimControl[ pTSoldier->animationPlayback().state() ].ubHeight == ANIM_STAND) && pTSoldier->vitals().health() > 30 && pTSoldier->vitals().breath() > 25 && (Item[pTSoldier->inventory()[HANDPOS].usItem].usItemClass == IC_PUNCH || Item[pTSoldier->inventory()[HANDPOS].usItem].usItemClass == IC_NONE))
									//{
									//	if ((((NUM_SKILL_TRAITS( pTSoldier, MARTIAL_ARTS_NT ) >= ((gSkillTraitValues.fPermitExtraAnimationsOnlyToMA) ? 2 : 1 )) && gGameOptions.fNewTraitSystem ) ||
									//		(HAS_SKILL_TRAIT( pTSoldier, MARTIALARTS_OT ) && !gGameOptions.fNewTraitSystem ) ) &&
									//		 pTSoldier->identity().bodyType() == REGMALE )
									//	{
									//		//pTSoldier->animationIntent().pendingAnimation() = NINJA_GOTOBREATH;
									//		pTSoldier->animationIntent().pendingAnimation() = NINJA_BREATH ;
									//	}
									//	else
									//	{
									//		pTSoldier->animationIntent().pendingAnimation() = PUNCH_BREATH ;
									//	}
									//}
									//else if ( (gAnimControl[ pTSoldier->animationPlayback().state() ].ubHeight == ANIM_STAND) && pTSoldier->vitals().health() > 30 && pTSoldier->vitals().breath() > 25 && (Item[pTSoldier->inventory()[HANDPOS].usItem].usItemClass == IC_BLADE))
									//{
									//	//pTSoldier->animationIntent().pendingAnimation() = KNIFE_GOTOBREATH;
									//	pTSoldier->animationIntent().pendingAnimation() = KNIFE_BREATH ;
									//}
								}
							}
						}
					}
				}
				break;

			case 493:

				//CODE: PICKUP ITEM!
				// CHECK IF THIS EVENT HAS BEEN SETUP
				//if ( pSoldier->pendingAction().action() == MERC_PICKUPITEM )
				//{
				// DROP ITEM
				HandleSoldierPickupItem( pSoldier, pSoldier->pendingAction().primaryData(), pSoldier->pendingAction().quaternaryData(), pSoldier->pendingAction().tertiaryData() );
				// EVENT HAS BEEN HANDLED
				pSoldier->pendingAction().clearAction();

				//}
				//else
				//{
				//	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Soldier Ani: CODE 493 Error, Pickup item action called but not setup" );
				//}
				break;

			case 494:

				//CODE: OPEN STRUCT!
				// CHECK IF THIS EVENT HAS BEEN SETUP
				//if ( pSoldier->pendingAction().action() == MERC_OPENSTRUCT )
				//{
				SoldierHandleInteractiveObject( pSoldier );

				// EVENT HAS BEEN HANDLED
				pSoldier->pendingAction().clearAction();

				//}
				//else
				//{
				//	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Soldier Ani: CODE 494 Error, OPen door action called but not setup" );
				//}
				break;

			case 495:

				if (pSoldier->aiPlanning().action() == AI_ACTION_UNLOCK_DOOR || (pSoldier->aiPlanning().action() == AI_ACTION_LOCK_DOOR && !(pSoldier->aiBehavior().flags() & AI_LOCK_DOOR_INCLUDES_CLOSE) ) )
				{
					// EVENT HAS BEEN HANDLED
					pSoldier->pendingAction().clearAction();

					// do nothing here
				}
				else
				{
					pSoldier->aiBehavior().flags() &= ~(AI_LOCK_DOOR_INCLUDES_CLOSE);

					pSoldier->audio().recordDoorOpeningNoise(
						DoorOpeningNoise( pSoldier ) );

					if ( SoldierHandleInteractiveObject( pSoldier ) )
					{
						// HANDLE SIGHT!
						HandleSight(pSoldier,SIGHT_LOOK | SIGHT_RADIO | SIGHT_INTERRUPT );

						InitOpplistForDoorOpening();
						//shadooow: this has been moved inside HandleDoorsOpenClose
						//MakeNoise( pSoldier->identity().id(), pSoldier->pendingAction().secondaryData(), pSoldier->position().level(), gpWorldLevelData[pSoldier->sGridNo].ubTerrainID, pSoldier->audio().doorOpeningNoise(), NOISE_CREAKING );
						//	gfDelayResolvingBestSighting = FALSE;

						gubInterruptProvoker = pSoldier->identity().id();
						AllTeamsLookForAll( TRUE );

						// ATE: Now, check AI guy to cancel what he was going....
						HandleSystemNewAISituation( pSoldier, TRUE );
					}

					// EVENT HAS BEEN HANDLED
					pSoldier->pendingAction().clearAction();
				}


				break;

			case 496:
				// CODE: GOTO PREVIOUS ANIMATION
				TacticalActorAnimationTransitions::changeState(*pSoldier,  pSoldier->animationPlayback().previousState(), pSoldier->animationPlayback().previousCode(), FALSE );
				return( TRUE );

			case 497:

				// CODE: CHECK FOR UNCONSCIOUS OR DEATH
				// IF 496 - GOTO PREVIOUS ANIMATION, OTHERWISE PAUSE ANIMATION
				if ( pSoldier->vitals().health() == 0 )
				{

					//HandleSoldierDeath( pSoldier );

					// If guy is now dead, and we have not played death sound before, play
					if ( pSoldier->vitals().health() == 0 && !pSoldier->dialogue().deathSoundPlayed()	)
					{
						if ( pSoldier->animationPlayback().state() != JFK_HITDEATH )
						{
							TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_DIE1 );
							pSoldier->dialogue().markDeathSoundPlayed();
						}
					}

					if ( pSoldier->skillState().cooldown(SOLDIER_COOLDOWN_CRYO) && pSoldier->animationPlayback().state() != CRYO_DEATH && pSoldier->animationPlayback().state() != CRYO_DEATH_CROUCHED )
					{
						if ( gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND )
							TacticalActorAnimationTransitions::changeState(*pSoldier,  CRYO_DEATH, 0, TRUE );
						else
							TacticalActorAnimationTransitions::changeState(*pSoldier,  CRYO_DEATH_CROUCHED, 0, TRUE );
					}
					else if ( gGameSettings.fOptions[ TOPTION_BLOOD_N_GORE ] )
					{
						// If we are dead, play some death animations!!
						switch( pSoldier->animationPlayback().state() )
						{
						case FLYBACK_HIT:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  FLYBACK_HIT_DEATH, 0, FALSE );
							break;

						case GENERIC_HIT_DEATHTWITCHNB:
						case FALLFORWARD_FROMHIT_STAND:
						case ENDFALLFORWARD_FROMHIT_CROUCH:

							TacticalActorAnimationTransitions::changeState(*pSoldier,  GENERIC_HIT_DEATH, 0, FALSE );
							break;

						case FALLBACK_HIT_DEATHTWITCHNB:
						case FALLBACK_HIT_STAND:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLBACK_HIT_DEATH, 0, FALSE );
							break;

						case PRONE_HIT_DEATHTWITCHNB:
						case PRONE_LAY_FROMHIT:

							TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_HIT_DEATH, 0, FALSE );
							break;

						case FALLOFF:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_DEATH, 0, FALSE );
							break;

						case FALLFORWARD_ROOF:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_FORWARD_DEATH, 0, FALSE);
							break;

						case ADULTMONSTER_DYING:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  ADULTMONSTER_DYING_STOP, 0, FALSE);
							break;

						case LARVAE_DIE:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  LARVAE_DIE_STOP, 0, FALSE);
							break;

						case QUEEN_DIE:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  QUEEN_DIE_STOP, 0, FALSE);
							break;

						case INFANT_DIE:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  INFANT_DIE_STOP, 0, FALSE);
							break;

						case CRIPPLE_DIE:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  CRIPPLE_DIE_STOP, 0, FALSE);
							break;

						case ROBOTNW_DIE:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  ROBOTNW_DIE_STOP, 0, FALSE);
							break;

						case CRIPPLE_DIE_FLYBACK:
							TacticalActorAnimationTransitions::changeState(*pSoldier,  CRIPPLE_DIE_FLYBACK_STOP, 0, FALSE);
							break;

						default:
							// IF we are here - something is wrong - we should have a death animation here
							DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Ani: Death sequence needed for animation %d", pSoldier->animationPlayback().state() ) );

						}
					}
					else
					{
						BOOLEAN fMadeCorpse;

						CheckForAndHandleSoldierDeath( pSoldier, &fMadeCorpse );

						// ATE: Needs to be FALSE!
						return( FALSE );
					}

					return( TRUE );
				}
				else
				{
					// We can safely be here as well.. ( ie - next turn we may be able to get up )
					// DO SOME CHECKS HERE TO FREE UP ATTACKERS IF WE ARE WAITING AT SPECIFIC ANIMATIONS
					if ( ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ANIM_HITFINISH ) )
					{
						gfPotentialTeamChangeDuringDeath = TRUE;

						// 0verhaul: This is now already handled
						// Release attacker
						// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, code 497 = check for death") );
						// ReleaseSoldiersAttacker( pSoldier );

						// ATE: OK - the above call can potentially
						// render the soldier bactive to false - check heare
						if ( !pSoldier->roster().active() )
						{
							return( FALSE );
						}

						gfPotentialTeamChangeDuringDeath = FALSE;

						// FREEUP GETTING HIT FLAG
						pSoldier->animationActivity().clearHit();
					}

					HandleCheckForDeathCommonCode( pSoldier );

					return( TRUE );
				}
				break;

			case 498:

				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				// SANDRO - if pending interrupt flag was set for before-attack type of interupt, try to resolve it now
				if ( UsingImprovedInterruptSystem() )
				{
					if ( ResolvePendingInterrupt( pSoldier, BEFORESHOT_INTERRUPT ) )
					{	
						if ( pSoldier->animationActivity().turningToShoot() )
							pSoldier->animationActivity().turningToShoot() = FALSE;

						pSoldier->animationIntent().clearFacingAnimation();
						// "artificially" set lock ui flag in this case
						if (pSoldier->roster().team() == gbPlayerNum)
						{
							//AddTopMessage( COMPUTER_INTERRUPT_MESSAGE, Message[STR_INTERRUPT] );
							guiPendingOverrideEvent = LU_BEGINUILOCK;								
							HandleTacticalUI( );
						}
						return( TRUE );				
						break;
					}
				}
				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				// CONDITONAL JUMP
				// If we have a pending animation, play it, else continue
				if ( pSoldier->animationIntent().pendingAnimation() != NO_PENDING_ANIMATION )
				{
					TacticalActorAnimationTransitions::changeState(*pSoldier,  pSoldier->animationIntent().pendingAnimation(), 0, FALSE );
					pSoldier->animationIntent().clearPendingAnimation();
					return( TRUE );
				}
				break;

				// JUMP TO NEXT STATIONARY ANIMATION ACCORDING TO HEIGHT
			case 499:

				if (!(pSoldier->status().flags() & SOLDIER_PC))
				{
					if ( pSoldier->aiPlanning().action() == AI_ACTION_PULL_TRIGGER )
					{
						if ( pSoldier->animationPlayback().state() == AI_PULL_SWITCH && GetJa2PendingTacticalCombatActions() == 0 && gubElementsOnExplosionQueue == 0 )
						{
							FreeUpNPCFromPendingAction( pSoldier );
						}
					}
					else if ( pSoldier->aiPlanning().action() == AI_ACTION_PENDING_ACTION
						|| pSoldier->aiPlanning().action() == AI_ACTION_OPEN_OR_CLOSE_DOOR
						|| pSoldier->aiPlanning().action() == AI_ACTION_YELLOW_ALERT
						|| pSoldier->aiPlanning().action() == AI_ACTION_RED_ALERT
						|| pSoldier->aiPlanning().action() == AI_ACTION_PULL_TRIGGER
						|| pSoldier->aiPlanning().action() == AI_ACTION_CREATURE_CALL
						|| pSoldier->aiPlanning().action() == AI_ACTION_UNLOCK_DOOR
						|| pSoldier->aiPlanning().action() == AI_ACTION_LOCK_DOOR	)
					{
						if ( pSoldier->animationPlayback().state() == PICKUP_ITEM || pSoldier->animationPlayback().state() == ADJACENT_GET_ITEM || pSoldier->animationPlayback().state() == ADJACENT_GET_ITEM_CROUCHED || pSoldier->animationPlayback().state() == DROP_ITEM || pSoldier->animationPlayback().state() == END_OPEN_DOOR || pSoldier->animationPlayback().state() == END_OPEN_DOOR_CROUCHED || pSoldier->animationPlayback().state() == CLOSE_DOOR || pSoldier->animationPlayback().state() == MONSTER_UP || pSoldier->animationPlayback().state() == AI_RADIO || pSoldier->animationPlayback().state() == AI_CR_RADIO || pSoldier->animationPlayback().state() == END_OPENSTRUCT || pSoldier->animationPlayback().state() == END_OPENSTRUCT_CROUCHED || pSoldier->animationPlayback().state() == QUEEN_CALL )
						{
							FreeUpNPCFromPendingAction( pSoldier );
						}
					}
				}

				ubDesiredHeight = pSoldier->animationIntent().desiredHeight();

				// Check if we are at the desired height
				if ( pSoldier->animationIntent().desiredHeight() == gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight || pSoldier->animationIntent().desiredHeight() == NO_DESIRED_HEIGHT )
				{
					// Adjust movement mode......
					if ( pSoldier->roster().team() == gbPlayerNum && !pSoldier->animationIntent().continuationMode() )
					{
						usMovementMode =	TacticalActorMobility::movementStateForCurrentStance(*pSoldier);

						// ATE: if we are currently running but have been told to walk, don't!
						if ( pSoldier->movement().mode() == RUNNING && usMovementMode == WALKING )
						{
							// No!
						}
						else
						{
							pSoldier->movement().mode() = usMovementMode;
						}
					}

					if (pSoldier->animationPlayback().state() == DODGE_ONE && pSoldier->animationIntent().pendingAnimation() == NO_PENDING_ANIMATION )
					{						
						INT8 bGoBackToAimAfterHit = pSoldier->animationActivity().postHitStance();
						pSoldier->animationActivity().postHitStance() = NO_SPEC_STANCE_AFTER_HIT;

						// CODE: HANDLE ANY RANDOM HIT VARIATIONS WE WISH TO DO.....
						if ( pSoldier->vitals().health() >= OKLIFE && bGoBackToAimAfterHit )
						{
							if ( bGoBackToAimAfterHit == GO_TO_AIM_AFTER_HIT )
							{		
								(void)TacticalActorRangedActions::readyFacing(
									*pSoldier,
									pSoldier->position().direction(),
									false,
									false);
							}
							else if ( bGoBackToAimAfterHit == GO_TO_ALTERNATIVE_AIM_AFTER_HIT && (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_STAND) )
							{						
								(void)TacticalActorRangedActions::readyFacing(
									*pSoldier,
									pSoldier->position().direction(),
									false,
									true);
							}
							else if ( bGoBackToAimAfterHit == GO_TO_HTH_BREATH_AFTER_HIT && (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_STAND))
							{						
								if ( Item[ pSoldier->inventory()[HANDPOS].usItem ].usItemClass & (IC_NONE | IC_PUNCH) )
								{
									if ((((NUM_SKILL_TRAITS( pSoldier, MARTIAL_ARTS_NT ) >= ((gSkillTraitValues.fPermitExtraAnimationsOnlyToMA) ? 2 : 1 )) && gGameOptions.fNewTraitSystem ) ||
										(HAS_SKILL_TRAIT( pSoldier, MARTIALARTS_OT ) && !gGameOptions.fNewTraitSystem ) ) && pSoldier->identity().bodyType() == REGMALE )
									{
										if(is_networked)
											TacticalActorAnimationTransitions::changeState(*pSoldier,  NINJA_BREATH, 0 , FALSE );
										else
											TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  NINJA_BREATH, 0 , FALSE );
									}
									else
									{
										if(is_networked)
											TacticalActorAnimationTransitions::changeState(*pSoldier,  PUNCH_BREATH, 0 , FALSE );
										else
											TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  PUNCH_BREATH, 0 , FALSE );
									}
								}
								else if ( Item[ pSoldier->inventory()[HANDPOS].usItem ].usItemClass & (IC_BLADE) )
								{
									if(is_networked)
										TacticalActorAnimationTransitions::changeState(*pSoldier,  KNIFE_BREATH, 0 , FALSE );
									else
										TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  KNIFE_BREATH, 0 , FALSE );
								}
							}
							else if ( bGoBackToAimAfterHit == GO_TO_COWERING_AFTER_HIT )
							{			
								if (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_CROUCH)
								{
									if(is_networked)
										TacticalActorAnimationTransitions::changeState(*pSoldier,  START_COWER_CROUCHED, 0 , FALSE );
									else
										TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  START_COWER_CROUCHED, 0 , FALSE );
								}
								else if (gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight == ANIM_PRONE)
								{
									if(is_networked)
										TacticalActorAnimationTransitions::changeState(*pSoldier,  START_COWER_PRONE, 0 , FALSE );
									else
										TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  START_COWER_PRONE, 0 , FALSE );
								}
							}
							return( TRUE );
						}
					}

					pSoldier->animationIntent().clearDesiredHeight();

					// 0verhaul:	This is moved to the animation state transition code to make sure it isn't sidestepped.
					// if (pSoldier->animationActivity().suppressionStanceChange())
					// {
					//	pSoldier->animationActivity().suppressionStanceChange() = FALSE;
					//	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Freeing up attacker - end of suppression stance change") );
					//	ReduceAttackBusyCount( pSoldier->suppression().suppressor(), FALSE );
					// }

					if ( pSoldier->animationIntent().pendingAnimation() == NO_PENDING_ANIMATION &&
						( pSoldier->animationActivity().turningFromProneMode() != TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE ) &&
						( pSoldier->animationActivity().turningFromProneMode() != TURNING_FROM_PRONE_ON ) )
					{
						if ( GetJa2PendingTacticalCombatActions() == 0 )
						{
							// OK, UNSET INTERFACE FIRST
							UnSetUIBusy( pSoldier->identity().id() );
							// ( before we could get interrupted potentially by an interrupt )
						}
					}

					// Check to see if we have changed stance and need to update visibility
					if ( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ANIM_STANCECHANGEANIM)
					{
						if ( pSoldier->animationIntent().pendingAnimation() == NO_PENDING_ANIMATION &&
							GetJa2PendingTacticalCombatActions() == 0 &&
							pSoldier->animationActivity().turningFromProneMode() != TURNING_FROM_PRONE_ENDING_UP_FROM_MOVE &&
							pSoldier->animationActivity().turningFromProneMode() != TURNING_FROM_PRONE_ON )
						{
							HandleSight(pSoldier,SIGHT_LOOK | SIGHT_RADIO | SIGHT_INTERRUPT );
						}
						else
						{
							HandleSight(pSoldier,SIGHT_LOOK | SIGHT_RADIO );
						}

						// Keep ui busy if we are now in a hidden interrupt
						// say we're prone and we crouch, we may get a hidden
						// interrupt and in such a case we'd really like the UI
						// still locked
						if ( gfHiddenInterrupt )
						{
							guiPendingOverrideEvent	= LA_BEGINUIOURTURNLOCK;
							HandleTacticalUI( );
						}

						// ATE: Now, check AI guy to cancel what he was going....
						HandleSystemNewAISituation( pSoldier, TRUE );

						// sevenfm: update tree visibility after changing stance
						if (//pSoldier->awareness().visibility() != -1 &&
							pSoldier->roster().team() != OUR_TEAM &&
							(pSoldier->roster().team() != MILITIA_TEAM || !gGameExternalOptions.bWeSeeWhatMilitiaSeesAndViceVersa))
							UpdateTreeVisibility();
					}

					// Have we finished opening doors?
					// 0verhaul:  Added additional check:  Are we told to stop at this point, maybe due to being interrupted?
					if ( !pSoldier->movement().outOfActionPoints() &&
						(pSoldier->animationPlayback().state() == END_OPEN_DOOR ||
						pSoldier->animationPlayback().state() == END_OPEN_DOOR_CROUCHED ||
						pSoldier->animationPlayback().state() == CRIPPLE_CLOSE_DOOR ||
						pSoldier->animationPlayback().state() == CRIPPLE_END_OPEN_DOOR ) )
					{
						// Are we told to continue movement...?
						if ( pSoldier->schedule().doorAnimationStarted() )
						{
							// OK, set this value to 2 such that once we are into a new gridno,
							// we close the door!
							pSoldier->schedule().completeDoorAnimation();

							// yes..
							(void)TacticalActorRouteExecution::requestPath(*pSoldier, pSoldier->pathing().finalDestinationGrid(), pSoldier->movement().mode() );

							if ( !( gAnimControl[ pSoldier->animationPlayback().state() ].uiFlags & ( ANIM_MOVING ) ) )
							{								
								if (!TileIsOutOfBounds(pSoldier->movement().absoluteDestination()))
								{
									DebugAI(AI_MSG_INFO, pSoldier, String("CancelAIAction: end door open code"));
									CancelAIAction( pSoldier, FORCE );
								}
							}

							// The schedule door continuation will be cancelled if anything
							// cuases guy to stop - StopMerc() will set it...

							return( TRUE );
						}
					}

					// Check if we should contine into a moving animation
					if ( pSoldier->animationIntent().pendingAnimation() != NO_PENDING_ANIMATION )
					{
						UINT16 usPendingAnimation = pSoldier->animationIntent().pendingAnimation();

						pSoldier->animationIntent().clearPendingAnimation();
						TacticalActorAnimationTransitions::changeState(*pSoldier,  usPendingAnimation, 0, FALSE );
						return( TRUE );
					}

					// Alrighty, do we wish to continue
					if ( pSoldier->animationIntent().continuationMode() )
					{
						// OK, if the code is == 2, get the path and try to move....
						if ( pSoldier->animationIntent().continuationMode() == 2 )
						{
							pSoldier->pathing().pathIndex()++;

							if ( pSoldier->pathing().pathIndex() > pSoldier->pathing().pathSize() )
							{
								pSoldier->pathing().pathIndex() = pSoldier->pathing().pathSize();
							}

							if ( pSoldier->pathing().pathIndex() == pSoldier->pathing().pathSize() )
							{
								// Stop, don't do anything.....
								// 0verhaul:	Only if not at the final destination
								// Another reason for rebuilding the animation system.	This should be part of a common
								// path continuation code so that any other bug fixes won't need to be duplicated in other areas.
								if ( pSoldier->position().gridNo() != pSoldier->pathing().finalDestinationGrid())
								{
									if ( !TacticalActorRouteExecution::requestPath(*pSoldier, pSoldier->pathing().finalDestinationGrid(), pSoldier->movement().mode(), TacticalActorRouteExecution::PathOrigin::ContinueMovement, false) )
									{
									}
								}
							}
							else
							{
								TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  pSoldier->movement().mode(), 0 , FALSE );

								// UNSET LOCK PENDING ACTION COUNTER FLAG
								pSoldier->status().flags() &= ( ~SOLDIER_LOCKPENDINGACTIONCOUNTER );

							}
						}
						else
						{
							(void)TacticalActorMobility::selectMovementForCurrentStance(*pSoldier);
						}

						pSoldier->animationIntent().clearContinuation();
						return( TRUE );
					}
					(void)TacticalActorRouteExecution::settleIntoStationaryStance(*pSoldier);
					return( TRUE );
				}
				else
				{
					ubCurrentHeight = gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight;

					// We need to go more, continue
					if ( ubDesiredHeight == ANIM_STAND && ubCurrentHeight == ANIM_CROUCH )
					{
						// Return here because if now, we will skipp a few frames
						TacticalActorAnimationTransitions::changeState(*pSoldier,  KNEEL_UP, 0 , FALSE );
						return( TRUE );
					}
					if ( ubDesiredHeight == ANIM_CROUCH && ubCurrentHeight == ANIM_STAND )
					{
						// Return here because if now, we will skipp a few frames
						TacticalActorAnimationTransitions::changeState(*pSoldier,  KNEEL_DOWN, 0 , FALSE );
						return( TRUE );
					}
					else if ( ubDesiredHeight == ANIM_PRONE && ubCurrentHeight == ANIM_CROUCH )
					{
						// Return here because if now, we will skipp a few frames
						TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_DOWN, 0 , FALSE );
						return( TRUE );
					}
					else if ( ubDesiredHeight == ANIM_CROUCH && ubCurrentHeight == ANIM_PRONE )
					{
						// Return here because if now, we will skipp a few frames
						TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_UP, 0 , FALSE );
						return( TRUE );
					}
				}
				// IF we are here - something is wrong - we should have a death animation here
#ifdef JA2BETAVERSION
				ScreenMsg( FONT_ORANGE, MSG_BETAVERSION, L"Soldier Ani: GOTO Stance not chained properly: %d %d %d", ubDesiredHeight, ubCurrentHeight, pSoldier->animationPlayback().state() );
#endif

				(void)TacticalActorRouteExecution::settleIntoStationaryStance(*pSoldier);
				return( TRUE );
			}

			// Adjust frame control pos, and try again
			pSoldier->animationPlayback().code()++;

		}
		else if ( sNewAniFrame > 499 && sNewAniFrame < 599 )
		{
			// Jump,
			// Do not adjust, just try again
			pSoldier->animationPlayback().code() = sNewAniFrame - 501;
		}
		else if ( sNewAniFrame > 599 && sNewAniFrame <= 699 )
		{
			// SANDRO - added some hacking in here, so I don't need to overwrite all those animation frame scripts
			switch(pSoldier->animationPlayback().state())
			{
			// go to apropriate stance for alternative weapon holding
			case READY_ALTERNATIVE_STAND:
			case SHOOT_ALTERNATIVE_STAND:
			case BURST_ALTERNATIVE_STAND:
			case LOW_SHOT_ALTERNATIVE_STAND:
			case LOW_BURST_ALTERNATIVE_STAND:
				TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  AIM_ALTERNATIVE_STAND, 0 , FALSE );
				return( TRUE );
				break;
			// hack to raise rifle after using idle animation without one in hand
			case REG_SQUISH:
			case REG_PULL:
			case BIGBUY_FLEX:
			case BIGBUY_STRECH:
			case FEM_KICKSN:
			case FEM_WIPE: 
				if ( pSoldier->inventory()[ HANDPOS ].exists() == true && Item[ pSoldier->inventory()[ HANDPOS ].usItem ].usItemClass == IC_GUN && ItemIsTwoHanded(pSoldier->inventory()[ HANDPOS ].usItem) )
				{
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  RAISE_RIFLE, 0 , FALSE );
					return( TRUE );
				}
				break;
			default:
				break;
			}
			// Jump, to animation script
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  (UINT16)(sNewAniFrame - 600 ), 0 , FALSE );
			return( TRUE );
		}
		else if ( sNewAniFrame > 799 && sNewAniFrame <= 899 )
		{
			// Jump, to animation script ( But in the 100's range )
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  (UINT16)(sNewAniFrame - 700 ), 0 , FALSE );
			return( TRUE );
		}
		else if ( sNewAniFrame > 899 && sNewAniFrame <= 999 )
		{
			// Jump, to animation script ( But in the 200's range )
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  (UINT16)(sNewAniFrame - 700 ), 0 , FALSE );
			return( TRUE );
		}
		else if ( sNewAniFrame > 699 && sNewAniFrame < 799 )
		{
			switch( sNewAniFrame )
			{
			case 702:
				// Play fall to knees sound
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( FALL_1 + Random(2) ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				break;

			case 703:
			case 704:

				// Play footprints
				PlaySoldierFootstepSound( pSoldier );
				break;

			case 705:
				// PLay body splat sound
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)BODY_SPLAT_1, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 706:
				// PLay head splat
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)HEADSPLAT_1, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) , TRUE );
				break;

			case 707:
				// PLay creature battle cry
				PlayJA2StreamingSample( (UINT8)CREATURE_BATTLECRY_1, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
				break;

			case 708:

				// PLay lock n' load sound for gun....
				// Get LNL sound for current gun
				{
					UINT16	usItem;
					UINT16	usSoundID;

					usItem = pSoldier->inventory()[ HANDPOS ].usItem;

					OBJECTTYPE* pObjUsed =  TacticalActorEquipment::usedWeapon(*pSoldier, &pSoldier->inventory()[ HANDPOS ] );
					UINT16 usItemUsemUsed = TacticalActorEquipment::usedWeaponNumber(*pSoldier, &pSoldier->inventory()[ HANDPOS ] );

					if ( pObjUsed->exists() == true )
					{
						usSoundID = Weapon[ usItemUsemUsed ].sLocknLoadSound;

						if ( usSoundID != 0 )
						{
							if (pSoldier->awareness().visibility() == TRUE || TeamMemberNear(gbPlayerNum, pSoldier->position().gridNo(), TACTICAL_RANGE))
								PlayJA2Sample( usSoundID, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
						}
					}
				}
				break;

			case 709:

				// Knife throw sound...
				// anv: possiblity to use custom sounds for throwing knives
				{
					UINT16 usedWeapon = TacticalActorEquipment::usedWeaponNumber(*pSoldier, &pSoldier->inventory()[ pSoldier->attackSelection().hand() ] );
					if ( Weapon[ usedWeapon ].sSound != 0 )
					{
						PlayJA2Sample( Weapon[ usedWeapon ].sSound, 44100-Random(5000)-Random(5000)-Random(5000), SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
					}
					else
					{
						PlayJA2Sample( Weapon[ THROWING_KNIFE ].sSound, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
					}
				}
				break;

			case 710:

				// Monster footstep in
				if ( SoldierOnScreen( pSoldier->identity().id() ) )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), ACR_STEP_1, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 711:

				// Monster footstep in
				if ( SoldierOnScreen( pSoldier->identity().id() ) )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), ACR_STEP_2, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 712:

				// Monster footstep in
				if ( SoldierOnScreen( pSoldier->identity().id() ) )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), LCR_MOVEMENT, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 713:

				// Monster footstep in
				if ( pSoldier->identity().bodyType() == INFANT_MONSTER )
				{
					if ( SoldierOnScreen( pSoldier->identity().id() ) )
					{
						PlaySoldierJA2Sample( pSoldier->identity().id(), BCR_DRAGGING, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
					}
				}
				break;

			case 714:

				// Lunges....
				PlaySoldierJA2Sample( pSoldier->identity().id(), ACR_LUNGE, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 715:

				// Swipe
				PlaySoldierJA2Sample( pSoldier->identity().id(), ACR_SWIPE, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 716:

				// Eat flesh
				PlaySoldierJA2Sample( pSoldier->identity().id(), ACR_EATFLESH, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 717:

				// Battle cry
				{
					INT32			iSoundID=0;
					BOOLEAN		fDoCry = FALSE;

					//if ( SoldierOnScreen( pSoldier->identity().id() ) )
					{
						switch( pSoldier->aiPlanning().actionData() )
						{
						case CALL_1_PREY:

							if ( pSoldier->identity().bodyType() == QUEENMONSTER )
							{
								iSoundID = LQ_SMELLS_THREAT;
							}
							else
							{
								iSoundID = ACR_SMEEL_PREY;
							}
							fDoCry = TRUE;
							break;

						case CALL_MULTIPLE_PREY:

							if ( pSoldier->identity().bodyType() == QUEENMONSTER )
							{
								iSoundID = LQ_SMELLS_THREAT;
							}
							else
							{
								iSoundID = ACR_SMELL_THREAT;
							}
							fDoCry = TRUE;
							break;

						case CALL_ATTACKED:

							if ( pSoldier->identity().bodyType() == QUEENMONSTER )
							{
								iSoundID = LQ_ENRAGED_ATTACK;
							}
							else
							{
								iSoundID = ACR_SMELL_THREAT;
							}
							fDoCry = TRUE;
							break;

						case CALL_CRIPPLED:

							if ( pSoldier->identity().bodyType() == QUEENMONSTER )
							{
								iSoundID = LQ_CRIPPLED;
							}
							else
							{
								iSoundID = ACR_CRIPPLED;
							}
							fDoCry = TRUE;
							break;
						}

						if ( fDoCry )
						{
							PlayJA2Sample( iSoundID, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
						}
					}
				}
				break;

			case 718:


				PlaySoldierJA2Sample( pSoldier->identity().id(), LQ_RUPTURING, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 719:

				// Spit attack start sound...
				PlaySoldierJA2Sample( pSoldier->identity().id(), LQ_ENRAGED_ATTACK, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 720:

				// Spit attack start sound...
				PlaySoldierJA2Sample( pSoldier->identity().id(), LQ_WHIP_ATTACK, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 721:
				// Play fall from knees to ground...
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( FALL_TO_GROUND_1 + Random(3) ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				if ( pSoldier->animationPlayback().state() == FALLFORWARD_FROMHIT_STAND )
				{
					CheckEquipmentForFragileItemDamage( pSoldier, 20 );
				}
				break;

			case 722:
				// Play fall heavy
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( HEAVY_FALL_1 ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				if ( pSoldier->animationPlayback().state() == FALLFORWARD_FROMHIT_CROUCH )
				{
					CheckEquipmentForFragileItemDamage( pSoldier, 15 );
				}
				break;

			case 723:

				// Play armpit noise...
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( IDLE_ARMPIT ), RATE_11025, SoundVolume( LOWVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 724:

				// Play ass scratch
				// PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( IDLE_SCRATCH ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->sGridNo ), 1, SoundDir( pSoldier->sGridNo ), TRUE );
				break;

			case 725:

				// Play back crack
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( IDLE_BACKCRACK ), RATE_11025, SoundVolume( LOWVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 726:

				// Kickin door
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( KICKIN_DOOR ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 727:

				// Swoosh
				// anv: possiblity to use custom sounds for melee weapons
				{
					UINT16 usedWeapon = TacticalActorEquipment::usedWeaponNumber(*pSoldier, &pSoldier->inventory()[ pSoldier->attackSelection().hand() ] );
					if ( Weapon[ usedWeapon ].sSound != 0 )
					{
						PlayJA2Sample( Weapon[ usedWeapon ].sSound, 44100-Random(5000)-Random(5000)-Random(5000), SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ) );
					}
					else
					{
						PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( SWOOSH_1 + Random( 6 ) ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
					}

					// Flugente: play a little sound for melee attacks
					if ( !usedWeapon || Item[usedWeapon].usItemClass & IC_PUNCH )
					{
						TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_PUNCH );
					}
					else if ( Item[usedWeapon].usItemClass & (IC_BLADE | IC_THROWING_KNIFE) )
					{
						TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_KNIFE );
					}
				}
				break;

			case 728:

				// Creature fall
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( ACR_FALL_1 ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 729:

				// grab roof....
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( GRAB_ROOF ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 730:

				// end climb roof....
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( LAND_ON_ROOF ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 731:

				// Stop climb roof..
				PlaySoldierJA2Sample( pSoldier->identity().id(), (UINT8)( FALL_TO_GROUND_1 + Random(3) ), RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 732:

				// Play die sound
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_DIE1 );
				pSoldier->dialogue().markDeathSoundPlayed();
				break;

			case 750:

				// CODE: Move Vehicle UP
				if ( pSoldier->status().flags() & SOLDIER_VEHICLE )
				{
				}
				break;

			case 751:

				// CODE: Move vehicle down
				if ( pSoldier->status().flags() & SOLDIER_VEHICLE )
				{
				}
				break;

			case 752:

				// Code: decapitate
				//DecapitateCorpse( pSoldier, pSoldier->targeting().gridNo(), pSoldier->targeting().level() );

				// Flugente: instead of jsut decapitating, we call a selection window where we can choose what to do with the corpse
				HandleSoldierUseCorpse( pSoldier, pSoldier->targeting().gridNo(), pSoldier->targeting().level() );	// Flugente: handle corpses
				break;

			case 753:

				// code: freeup attcker
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Reducing attacker busy count..., CODE FROM ANIMATION %s ( %d )", gAnimControl[ pSoldier->animationPlayback().state() ].zAnimStr, pSoldier->animationPlayback().state() ) );
				DebugAttackBusy( String("@@@@@@@ Reducing attacker busy count..., CODE FROM ANIMATION %s ( %d )\n", gAnimControl[ pSoldier->animationPlayback().state() ].zAnimStr, pSoldier->animationPlayback().state() ) );
				// ReduceAttackBusyCount( pSoldier->identity().id(), FALSE );

				// ATE: Here, reduce again if creaturequeen tentical attack...
				// Uh, why not add a second 753 code to the queen swipe instead of adding code to make things more complex?
				if ( pSoldier->animationPlayback().state() == QUEEN_SWIPE )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Reducing attacker busy count for end of queen swipe\n" ) );
					DebugAttackBusy( "@@@@@@@ Reducing attacker busy count for end of queen swipe" );
					// ReduceAttackBusyCount( pSoldier->identity().id(), FALSE );
				}
				
				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				// SANDRO - if pending interrupt flag was set for after-attack type of interupt, try to resolve it now
				if ( UsingImprovedInterruptSystem() )
				{
					ResolvePendingInterrupt( pSoldier, AFTERACTION_INTERRUPT );
				}
				////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				break;

			case 754:

				HandleFallIntoPitFromAnimation( pSoldier->identity().id() );
				break;

			case 755 :

				DishoutQueenSwipeDamage( pSoldier );
				break;

			case 756:

				// Reload robot....
				{
					SoldierID ubPerson = WhoIsThere2( pSoldier->pendingAction().secondaryData(), pSoldier->position().level() );
					TacticalActor* pRobot =
						GetJa2SoldierRepository().resolve( ubPerson );

					if ( pRobot != nullptr &&
						(pRobot->status().flags() & SOLDIER_ROBOT) )
					{
						ReloadGun( pRobot, &(pRobot->inventory()[ HANDPOS ] ), pSoldier->pendingItem().object() );

						// OK, check what was returned and place in inventory if it's non-zero
						if ( pSoldier->pendingItem().object()->exists() == true )
						{
							// Add to inv..
							AutoPlaceObject( pSoldier, pSoldier->pendingItem().object(), TRUE );
						}

						pSoldier->pendingItem().clearObject();
					}
				}
				break;

			case 757:

				// INcrement attacker busy count....
//				GetJa2PendingTacticalCombatActions()++;
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("!!!!! Incrementing attacker busy count..., CODE FROM ANIMATION %s ( %d ) : Count now %d", gAnimControl[ pSoldier->animationPlayback().state() ].zAnimStr, pSoldier->animationPlayback().state(), GetJa2PendingTacticalCombatActions() ) );
				DebugAttackBusy( String("!!!!! CODE FROM ANIMATION %s ( %d )\n", gAnimControl[ pSoldier->animationPlayback().state() ].zAnimStr, pSoldier->animationPlayback().state() ) );
				break;

			case 758:

				// Trigger after slap...
				TriggerNPCWithGivenApproach( QUEEN, APPROACH_DONE_SLAPPED , TRUE );
				break;

			case 759:

				// Getting hit by slap
				{
					TacticalActor *pTarget;

					pTarget = FindSoldierByProfileID( ELLIOT, FALSE );

					if ( pTarget )
					{
						TacticalActorAnimationTransitions::initializeAnimation(*pTarget,  SLAP_HIT, 0 , FALSE );

						// Play noise....
						//PlaySoldierJA2Sample( pTarget->identity().id(), ( S_SLAP_IMPACT ), RATE_11025, SoundVolume( HIGHVOLUME, pTarget->sGridNo ), 1, SoundDir( pTarget->sGridNo ), TRUE );

						//TacticalActorBattleSounds::play( pTarget, (INT8)( BATTLE_SOUND_HIT1 + Random( 2 ) ) );

					}
				}
				break;

			case 760:

				// Get some blood.....
				// Corpse Id is from pending action data
				GetBloodFromCorpse( pSoldier );
				// Dirty interface....
				DirtyMercPanelInterface( pSoldier, DIRTYLEVEL2 );
				break;

			case 761:

				{
					// Dish out damage!
					TacticalActor* target =
						GetJa2SoldierRepository().resolve(
							pSoldier->pendingAction().quaternaryData() );
					if ( target )
						TacticalActorDamageResolution::applyHit(*target,  TAKE_DAMAGE_BLADE, (INT16) 25, (INT16) 25, gOppositeDirection[ pSoldier->position().direction() ], 50, pSoldier->identity().id(), 0, ANIM_PRONE, 0, 0 );
				}
				break;

			case 762:
				{
					// CODE: Set off Trigger
					INT8 bPanicTrigger = ClosestPanicTrigger( pSoldier );
					if (bPanicTrigger != -1)
					{
						SetOffPanicBombs( pSoldier->identity().id(), bPanicTrigger );
					}
					// any AI guy has been specially given keys for this, now take them
					// away
					pSoldier->inventory().keyAccess() = pSoldier->inventory().keyAccess() >> 1;
				}
				break;

			case 763:

				// CODE: Drop item at gridno
				if ( pSoldier->pendingItem().object() != NULL )
				{
					if ( pSoldier->awareness().visibility() != -1 )
					{
						if (Water(pSoldier->pendingAction().secondaryData(), pSoldier->position().level()))
						{
							UINT16 usItem = pSoldier->pendingItem().object()->usItem;
							INT32 sGridNo = pSoldier->pendingAction().secondaryData();

							if (HasItemFlag(usItem, CORPSE))
								PlayJA2Sample(ENTER_DEEP_WATER_1, RATE_11025, SoundVolume(MIDVOLUME, sGridNo), 1, SoundDir(sGridNo));
							else if (Item[usItem].ubWeight > 10)
								PlayJA2Sample(ENTER_WATER_1, RATE_11025, SoundVolume(MIDVOLUME, sGridNo), 1, SoundDir(sGridNo));
							else
								PlaySplashSound(sGridNo);
						}
						else
						{
							PlayJA2Sample(THROW_IMPACT_2, RATE_11025, SoundVolume(MIDVOLUME, pSoldier->position().gridNo()), 1, SoundDir(pSoldier->position().gridNo()));
						}
					}

					AddItemToPool( pSoldier->pendingAction().secondaryData(), pSoldier->pendingItem().object(), 1, pSoldier->position().level(), 0 , -1 );
					NotifySoldiersToLookforItems( );

					pSoldier->pendingItem().clearObject();
				}
				break;

			case 764:

				PlaySoldierJA2Sample( pSoldier->identity().id(), PICKING_LOCK, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 765:

				// Flyback hit - do blood!
				// PLace in existing tile and one back...
				{
						INT32 sNewGridNo;

					InternalDropBlood( pSoldier->position().gridNo(), pSoldier->position().level(), 0, (UINT8)(MAXBLOODQUANTITY), 1 );

					// Move forward one gridno....
						sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[ pSoldier->position().direction() ] ) );

					InternalDropBlood( sNewGridNo, pSoldier->position().level(), 0, (UINT8)(MAXBLOODQUANTITY), 1 );

				}
				break;

			case 766:

				// Flugente: if doing this action a lot, this gets annoying - only play sound sometimes
				if ( Chance(gGameExternalOptions.iChanceSayAnnoyingPhrase) )
				{
					// Say COOL quote
					TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_COOL1 );
				}
				break;

			case 767:

				// Slap sound effect
				PlaySoldierJA2Sample( pSoldier->identity().id(), SLAP_2, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				break;

			case 768:

				// OK, after ending first aid, stand up if not in combat....
				if ( NumCapableEnemyInSector( ) == 0 )
				{
					// Stand up...
					(void)TacticalActorOrientation::changeStance(*pSoldier, ANIM_STAND );
					return( FALSE );
				}
				break;

			case 769:

				// ATE: LOOK HERE FOR CODE IN INTERNALS FOR
				// REFUELING A VEHICLE
				// THE GAS_CAN IS IN THE MERCS MAIN HAND AT THIS TIME
				{
					// Get pointer to vehicle...
					SoldierID ubPerson = WhoIsThere2( pSoldier->pendingAction().secondaryData(), pSoldier->position().level() );
					TacticalActor* pVehicle =
						GetJa2SoldierRepository().resolve( ubPerson );
					if ( pVehicle != nullptr )
					{
						// this is a ubID for soldiertype....
						AddFuelToVehicle( pSoldier, pVehicle );

						fInterfacePanelDirty = DIRTYLEVEL2;
					}
				}
				break;

			case 770:

				PlayJA2Sample( USE_WIRE_CUTTERS, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
				break;

			case 771:

				PlayJA2Sample( BLOODCAT_ATTACK, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
				break;

			case 772:

				//CODE: FOR A REALTIME NON-INTERRUPTABLE SCRIPT - SIGNAL DONE
				pSoldier->animationActivity().realtimeNonInterruptible() = FALSE;
				break;

			case 773:

				// Kneel up...
				if ( !pSoldier->movement().stealthMode() )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), KNEEL_UP_SOUND, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 774:

				// Kneel down..
				if ( !pSoldier->movement().stealthMode() )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), KNEEL_DOWN_SOUND, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 775:

				// prone up..
				if ( !pSoldier->movement().stealthMode() )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), PRONE_UP_SOUND, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 776:

				// prone down..
				if ( !pSoldier->movement().stealthMode() )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), PRONE_DOWN_SOUND, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 777:

				// picking something up
				if ( !pSoldier->movement().stealthMode() )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), PICKING_SOMETHING_UP, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 778:
				if (!pSoldier->movement().stealthMode() && (pSoldier->awareness().visibility() == TRUE || TeamMemberNear(gbPlayerNum, pSoldier->position().gridNo(), TACTICAL_RANGE)))
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), ENTER_DEEP_WATER_1, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 779:

				PlaySoldierJA2Sample( pSoldier->identity().id(), COW_FALL, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 780:

				PlaySoldierJA2Sample( pSoldier->identity().id(), COW_HIT_SND, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 781:

				PlaySoldierJA2Sample( pSoldier->identity().id(), ACR_DIE_PART2, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				break;

			case 782:

				PlaySoldierJA2Sample( pSoldier->identity().id(), CREATURE_DISSOLVE_1, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				break;

			case 784:

				PlaySoldierJA2Sample( pSoldier->identity().id(), CREATURE_FALL, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				break;

			case 785:

				if ( Random( 5 ) == 0 )
				{
					PlaySoldierJA2Sample( pSoldier->identity().id(), CROW_PECKING_AT_FLESH, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				}
				break;

			case 786:

				PlaySoldierJA2Sample( pSoldier->identity().id(), CROW_FLYING_AWAY, RATE_11025, SoundVolume( MIDVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 787:

				PlaySoldierJA2Sample( pSoldier->identity().id(), SLAP_1, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), FALSE );
				break;

			case 788:

				PlaySoldierJA2Sample( pSoldier->identity().id(), MORTAR_START, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 789:

				PlaySoldierJA2Sample( pSoldier->identity().id(), MORTAR_LOAD, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 790:

				PlaySoldierJA2Sample( pSoldier->identity().id(), COW_FALL_2, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 791:

				PlaySoldierJA2Sample( pSoldier->identity().id(), FENCE_OPEN, RATE_11025, SoundVolume( HIGHVOLUME, pSoldier->position().gridNo() ), 1, SoundDir( pSoldier->position().gridNo() ), TRUE );
				break;

			case 792:

				
				break;
			}
			// Adjust frame control pos, and try again
			pSoldier->animationPlayback().code()++;
		}
		else if ( sNewAniFrame == 999 )
		{

			// Go to start, by default
			pSoldier->animationPlayback().code() = 0;

		}
		else if ( sNewAniFrame > 999 && sNewAniFrame <= 1099 )
		{
			// Jump, to animation script ( in the 300+ range )
			TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  (UINT16)(sNewAniFrame - 700 ), 0 , FALSE );
			return( TRUE );
		}
		else if ( sNewAniFrame > 1099 )
		{				
			switch( sNewAniFrame )
			{

			case 1101:
				// SANDRO - dual burst check for repeating animation
			{
				OBJECTTYPE* pObjHand = TacticalActorEquipment::usedWeapon(*pSoldier, &pSoldier->inventory()[pSoldier->attackSelection().hand()] );

				if (pSoldier->fireControl().burstCounter() && ( pSoldier->fireControl().burstCounter() <= ( (pSoldier->fireControl().autofireShots())?(2*pSoldier->fireControl().autofireShots()):(2*GetShotsPerBurst( pObjHand ))	) ))
				{
					if ( pSoldier->animationPlayback().state() == BURST_DUAL_PRONE )
						pSoldier->animationPlayback().code() = 2;
					else
						pSoldier->animationPlayback().code() = 1;
				}
				else
				{
					pSoldier->animationPlayback().code()++;
				}
				break;
			}
			case 1102:
				// SANDRO - end dual burst check for going to proper aim state

				switch ( gAnimControl[ pSoldier->animationPlayback().state() ].ubEndHeight )
				{
				case ANIM_STAND:
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  AIM_DUAL_STAND, 0 , FALSE );
					break;
				case ANIM_CROUCH:
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  AIM_DUAL_CROUCH, 0 , FALSE );
					break;
				case ANIM_PRONE:
					TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  AIM_DUAL_PRONE, 0 , FALSE );
					break;
				}
				return( TRUE );
				break;
			}
		}

		// Loop here until we break on a real item!
	} while ( TRUE );

	// We're done
	return( TRUE );
}

#define MIN_DEADLINESS_FOR_LIKE_GUN_QUOTE			20

BOOLEAN ShouldMercSayHappyWithGunQuote( TacticalActor *pSoldier )
{
	// How do we do this....

	if ( QuoteExp[ pSoldier->identity().profile() ].QuoteExpGotGunOrUsedGun == QUOTE_SATISFACTION_WITH_GUN_AFTER_KILL )
	{
		// For one, only once a day...
		if ( pSoldier->dialogue().hasSaid(SOLDIER_QUOTE_SAID_LIKESGUN) )
		{
			return( FALSE );
		}

		// is it a gun?
		if ( Item[ pSoldier->attackSelection().weapon() ].usItemClass & IC_GUN )
		{
			// Is our weapon powerfull enough?
			if ( Weapon[ pSoldier->attackSelection().weapon() ].ubDeadliness > MIN_DEADLINESS_FOR_LIKE_GUN_QUOTE )
			{
				// 20 % chance?
				if ( Chance( 20 ) )
				{
					return( TRUE );
				}
			}
		}
	}

	return( FALSE );
}


void SayBuddyWitnessedQuoteFromKill( TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel )
{
// WDS - make number of mercenaries, etc. be configurable
	std::vector<UINT16>	ubMercsInSector (CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS, 0);
//	UINT8	ubMercsInSector[ CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS ] = { 0 };
	std::vector<INT8>	bBuddyIndex (CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS, -1);
//	INT8	bBuddyIndex[ CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS ] = { -1 };
	INT8	bTempBuddyIndex;
	UINT16	ubNumMercs = 0;
	UINT16	ubChosenMerc;
	TacticalActor *pTeamSoldier;
	UINT16	usQuoteNum;
	BOOLEAN buddyquoteused = FALSE;

	// Loop through all our guys and randomly say one from someone in our sector

	// Flugente: as we only play a sound in 20% of all cases, pass that check first before going through all this stuff
	if ( !Chance( 20 ) )
		return;

	// set up soldier ptr as first element in mercptrs list
	SoldierID cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

	// run through list
	for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
	{
		pTeamSoldier = GetJa2SoldierRepository().resolve( cnt );
		if ( pTeamSoldier == nullptr )
		{
			continue;
		}

		// Add guy if he's a candidate...		
		if ( OK_INSECTOR_MERC( pTeamSoldier ) && !AM_AN_EPC( pTeamSoldier ) && !( pTeamSoldier->status().flags() & SOLDIER_GASSED ) && !(AM_A_ROBOT( pTeamSoldier ))
			&& !pTeamSoldier->assignment().isAsleep() && !TileIsOutOfBounds(pTeamSoldier->position().gridNo()) && pTeamSoldier->identity().profile() != pKillerSoldier->identity().profile() )
		{
			// Are we a buddy of killer?
			bTempBuddyIndex = WhichBuddy( pTeamSoldier->identity().profile(), pKillerSoldier->identity().profile() );
			
			if ( bTempBuddyIndex != -1 )
			{
				switch( bTempBuddyIndex )
				{
				case 0:
					if ( pTeamSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_1_WITNESSED) )
					{
						continue;
					}
					break;

				case 1:
					if ( pTeamSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_2_WITNESSED) )
					{
						continue;
					}
					break;

				case 2:
					if ( pTeamSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_3_WITNESSED) )
					{
						continue;
					}
					break;

				case 3:
					if ( pTeamSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_4_WITNESSED) )
					{
						continue;
					}
					break;

				case 4:
					if ( pTeamSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_5_WITNESSED) )
					{
						continue;
					}
					break;

				case 5:
					if ( pTeamSoldier->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_6_WITNESSED) )
					{
						continue;
					}
					break;
				}

				// TO LOS check to killed
				// Can we see location of killer?
				if ( SoldierTo3DLocationLineOfSightTest( pTeamSoldier, pKillerSoldier->position().gridNo(),  pKillerSoldier->position().level(), 3, TRUE, CALC_FROM_ALL_DIRS ) == 0 )
				{
					continue;
				}
				
				// Can we see location of killed?
				if ( SoldierTo3DLocationLineOfSightTest( pTeamSoldier, sGridNo,  bLevel, 3, TRUE, CALC_FROM_ALL_DIRS ) == 0 )
				{
					continue;
				}

				// OK, a good candidate...
				ubMercsInSector[ ubNumMercs ] = (UINT16)cnt;
				bBuddyIndex[ ubNumMercs ]	 = bTempBuddyIndex;
				++ubNumMercs;
			}
		}
	}

	// If we are > 0
	if ( ubNumMercs > 0 )
	{
		ubChosenMerc = (UINT16)Random( ubNumMercs );
		TacticalActor *pChosen = GetJa2SoldierRepository().resolve(
			ubMercsInSector[ubChosenMerc] );
		if ( pChosen == nullptr )
			return;

		switch( bBuddyIndex[ ubChosenMerc ] )
		{
		case 0:
			usQuoteNum = QUOTE_BUDDY_1_GOOD;
			pChosen->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_1_WITNESSED);
			break;

		case 1:
			usQuoteNum = QUOTE_BUDDY_2_GOOD;
			pChosen->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_2_WITNESSED);
			break;

		case 2:
			if( pChosen->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
				usQuoteNum = QUOTE_AIM_BUDDY_3_GOOD;
			else
				usQuoteNum = QUOTE_NON_AIM_BUDDY_3_GOOD;
			pChosen->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_3_WITNESSED);
			break;

		case 3:
			if( pChosen->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
				usQuoteNum = QUOTE_AIM_BUDDY_4_GOOD;
			else
				usQuoteNum = QUOTE_NON_AIM_BUDDY_4_GOOD;
			pChosen->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_4_WITNESSED);
			break;

		case 4:
			if( pChosen->employment().mercenaryType() == MERC_TYPE__AIM_MERC )
				usQuoteNum = QUOTE_AIM_BUDDY_5_GOOD;
			else
				usQuoteNum = QUOTE_NON_AIM_BUDDY_5_GOOD;
			pChosen->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_5_WITNESSED);
			break;

		case 5:
			usQuoteNum = QUOTE_LEARNED_TO_LIKE_WITNESSED;
			pChosen->dialogue().markSaidExtended(SOLDIER_QUOTE_SAID_BUDDY_6_WITNESSED);
			break;
		}

		TacticalCharacterDialogue( pChosen, usQuoteNum );

		buddyquoteused = TRUE;
	}

	// Flugente: if we want to play a sound, but have not found a fitting buddy, try additional dialogue
	// this could be expensive, as we need quite a few sight tests here...
	if ( !buddyquoteused && pKillerSoldier->roster().team() == gbPlayerNum )
	{
		cnt = gTacticalStatus.Team[gbPlayerNum].bFirstID;

		for ( ; cnt <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++cnt )
		{
			pTeamSoldier = GetJa2SoldierRepository().resolve( cnt );
			if ( pTeamSoldier == nullptr )
			{
				continue;
			}

			// we do not exclude the buddies from above. If we get to this point, it might have been a buddy that already said his line. In that case additional dialogue might play other ones

			// Add guy if he's a candidate...		
			if ( OK_INSECTOR_MERC( pTeamSoldier ) && !AM_AN_EPC( pTeamSoldier ) && !( pTeamSoldier->status().flags() & SOLDIER_GASSED ) && !( AM_A_ROBOT( pTeamSoldier ) )
				&& !pTeamSoldier->assignment().isAsleep() && !TileIsOutOfBounds( pTeamSoldier->position().gridNo() ) )//&& pTeamSoldier->identity().profile() != pKillerSoldier->identity().profile() )
			{
				// TO LOS check to killed
				// Can we see location of killer?
				// not if we're the killer
				if ( pTeamSoldier->identity().id() != pKillerSoldier->identity().id() &&
					SoldierTo3DLocationLineOfSightTest( pTeamSoldier, pKillerSoldier->position().gridNo(), pKillerSoldier->position().level(), 3, TRUE, CALC_FROM_ALL_DIRS ) == 0 )
				{
					continue;
				}

				// Can we see location of killed?
				if ( SoldierTo3DLocationLineOfSightTest( pTeamSoldier, sGridNo, bLevel, 3, TRUE, CALC_FROM_ALL_DIRS ) == 0 )
				{
					continue;
				}

				AdditionalTacticalCharacterDialogue_CallsLua( pTeamSoldier, ADE_WITNESS_GOOD, pKillerSoldier->identity().profile(), 0 );
			}
		}
	}
}


void HandleKilledQuote( TacticalActor *pKilledSoldier, TacticalActor *pKillerSoldier, INT32 sGridNo, INT8 bLevel )
{
	TacticalActor *pTeamSoldier;
// WDS - make number of mercenaries, etc. be configurable
	std::vector<UINT16>	ubMercsInSector (CODE_MAXIMUM_NUMBER_OF_PLAYER_SLOTS, 0);
	UINT16	ubNumMercs = 0;
	UINT16	ubChosenMerc;
	BOOLEAN fDoSomeoneElse = FALSE;

	gfLastMercTalkedAboutKillingID = pKilledSoldier->identity().id();

	// Can we see location?
	BOOLEAN	fCanWeSeeLocation = ( SoldierTo3DLocationLineOfSightTest( pKillerSoldier, sGridNo,  bLevel, 3, TRUE, CALC_FROM_ALL_DIRS ) != 0 );

	// Are we killing mike?
	if ( pKilledSoldier->identity().profile() == MIKE && ( pKillerSoldier->employment().mercenaryType() == MERC_TYPE__AIM_MERC || (  pKillerSoldier->employment().mercenaryType() == MERC_TYPE__MERC && gMercProfiles[pKillerSoldier->identity().profile()].bLearnToHate == 255 ) ) )
	{
		// Can we see?
		if ( fCanWeSeeLocation )
		{
			TacticalCharacterDialogue( pKillerSoldier, QUOTE_AIM_KILLED_MIKE );
		}
	}
	// Are we killing factory mamager?
	else if ( pKilledSoldier->identity().profile() == DOREEN )
	{
		// Can we see?
		//f ( fCanWeSeeLocation )
		{
			TacticalCharacterDialogue( pKillerSoldier, QUOTE_KILLED_FACTORY_MANAGER );
		}
	}
	else
	{
		// Steps here...

		// If not head shot, just say killed quote

		// If head shot...

		// If we have a head shot saying,, randomly try that one

		// If not doing that one, search for anybody who can see person

		// If somebody did, play his quote plus attackers killed quote.

		// Checkf for headhot!
		if ( pKilledSoldier->animationPlayback().state() == JFK_HITDEATH )
		{
			//Randomliy say it!
			if ( Random( 100 ) < 40 )
			{
				TacticalCharacterDialogue( pKillerSoldier, QUOTE_HEADSHOT );
			}
			else
			{
				fDoSomeoneElse = TRUE;
			}

			if ( fDoSomeoneElse )
			{
				// Check if a person is here that has this quote....
				SoldierID  cnt = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

				// run through list
				for ( ; cnt <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++cnt )
				{
					pTeamSoldier = GetJa2SoldierRepository().resolve( cnt );
					if ( pTeamSoldier == nullptr )
					{
						continue;
					}

					if ( cnt != pKillerSoldier->identity().id() )
					{
						if ( OK_INSECTOR_MERC( pTeamSoldier ) && !( pTeamSoldier->status().flags() & SOLDIER_GASSED ) && !AM_AN_EPC( pTeamSoldier ) )
						{
							// Can we see location?
							if ( SoldierTo3DLocationLineOfSightTest( pTeamSoldier, sGridNo,  bLevel, 3, TRUE, CALC_FROM_ALL_DIRS ) )
							{
								ubMercsInSector[ ubNumMercs ] = (UINT16)cnt;
								ubNumMercs++;
							}
						}
					}
				}

				// Did we find anybody?
				if ( ubNumMercs > 0 )
				{
					ubChosenMerc = (UINT16)Random( ubNumMercs );

					// We have a random chance of not saying our we killed a guy quote
					if ( Random( 100 ) < 50 )
					{
						// Say this guys quote but the killer's quote as well....
						// if killed was not a plain old civ, say quote
						if (pKilledSoldier->roster().team() != CIV_TEAM || pKilledSoldier->roster().civilianGroup() != 0)
						{
							TacticalCharacterDialogue( pKillerSoldier, QUOTE_KILLED_AN_ENEMY );
						}
					}

					TacticalActor* witness =
						GetJa2SoldierRepository().resolve(
							ubMercsInSector[ ubChosenMerc ] );
					if ( witness )
						TacticalCharacterDialogue(
							witness, QUOTE_HEADSHOT );
				}
				else
				{
					// Can we see?
					if ( fCanWeSeeLocation )
					{
						// Say this guys quote but the killer's quote as well....
						// if killed was not a plain old civ, say quote
						if (pKilledSoldier->roster().team() != CIV_TEAM || pKilledSoldier->roster().civilianGroup() != 0)
						{
							TacticalCharacterDialogue( pKillerSoldier, QUOTE_KILLED_AN_ENEMY );
						}
					}
				}
			}
		}
		else
		{
			// Can we see?
			if ( fCanWeSeeLocation )
			{
				// if killed was not a plain old civ, say quote
				if (pKilledSoldier->roster().team() != CIV_TEAM || pKilledSoldier->roster().civilianGroup() != 0)
				{
					// Are we happy with our gun?
					if ( ShouldMercSayHappyWithGunQuote( pKillerSoldier )	)
					{
						TacticalCharacterDialogue( pKillerSoldier, QUOTE_SATISFACTION_WITH_GUN_AFTER_KILL );
						pKillerSoldier->dialogue().markSaid(SOLDIER_QUOTE_SAID_LIKESGUN);
					}
					else if ( pKillerSoldier->roster().side() == pKilledSoldier->roster().side() )
					{
						// if the attacker was from the same side, play a curse
						TacticalActorBattleSounds::play(*pKillerSoldier,  (INT8)(BATTLE_SOUND_CURSE1) );
					}
					else
						// Randomize between laugh, quote...
					{
						if ( Random( 100 ) < 33 && pKilledSoldier->identity().bodyType() != BLOODCAT )
						{
							// If it's a creature......
							if ( pKilledSoldier->status().flags() & SOLDIER_MONSTER )
							{
								TacticalCharacterDialogue( pKillerSoldier, QUOTE_KILLED_A_CREATURE );
							}
							else
							{
								TacticalCharacterDialogue( pKillerSoldier, QUOTE_KILLED_AN_ENEMY );
							}
						}
						else
						{
							if ( Random( 50 ) == 25 )
							{
								TacticalActorBattleSounds::play(*pKillerSoldier,  (INT8)( BATTLE_SOUND_LAUGH1 ) );
							}
							else
							{
								TacticalActorBattleSounds::play(*pKillerSoldier,  (INT8)( BATTLE_SOUND_COOL1 ) );
							}
						}
					}

					// Buddy witnessed?
					SayBuddyWitnessedQuoteFromKill( pKillerSoldier, sGridNo, bLevel );
				}
			}
		}
	}
}

BOOLEAN HandleSoldierDeath( TacticalActor *pSoldier , BOOLEAN *pfMadeCorpse )
{
	BOOLEAN fBuddyJustDead = FALSE;
	*pfMadeCorpse = FALSE;

	if ( pSoldier->vitals().health() == 0 && !( pSoldier->status().flags() & SOLDIER_DEAD )	)
	{
		// Haydent/send death info
		if (is_networked)
		{			
			if(pSoldier->roster().team()==0)
				send_death(pSoldier);
			else if(pSoldier->roster().team() <6 && ((gTacticalStatus.combatUI.ubTopMessageType == PLAYER_TURN_MESSAGE) || (gTacticalStatus.combatUI.ubTopMessageType == PLAYER_INTERRUPT_MESSAGE)))
				send_death(pSoldier);						
		}

		// anv: enemy taunts after kill
		TacticalActor *pKillerSoldier = NULL;
		if(pSoldier->combatResult().currentAttacker() != NOBODY)
		{
			pKillerSoldier =
				GetJa2SoldierRepository().resolve( pSoldier->combatResult().currentAttacker() );
		}
		if(pKillerSoldier == nullptr &&
			pSoldier->combatResult().previousAttacker() != NOBODY)
		{
			pKillerSoldier =
				GetJa2SoldierRepository().resolve( pSoldier->combatResult().previousAttacker() );
		}
		if(pKillerSoldier != NULL)
		{
			if( pSoldier->animationPlayback().state() == JFK_HITDEATH )
				PossiblyStartEnemyTaunt( pKillerSoldier, TAUNT_HEAD_POP, pSoldier->identity().id() );
			else if( Item[pKillerSoldier->attackSelection().weapon()].usItemClass & IC_GUN )
				PossiblyStartEnemyTaunt( pKillerSoldier, TAUNT_KILL_GUNFIRE, pSoldier->identity().id() );
			else if( Item[pKillerSoldier->attackSelection().weapon()].usItemClass & IC_BLADE )
				PossiblyStartEnemyTaunt( pKillerSoldier, TAUNT_KILL_BLADE, pSoldier->identity().id() );
			else if( Item[pKillerSoldier->attackSelection().weapon()].usItemClass & IC_PUNCH )
				PossiblyStartEnemyTaunt( pKillerSoldier, TAUNT_KILL_HTH, pSoldier->identity().id() );
			else if( Item[pKillerSoldier->attackSelection().weapon()].usItemClass & IC_THROWING_KNIFE )
				PossiblyStartEnemyTaunt( pKillerSoldier, TAUNT_KILL_THROWING_KNIFE, pSoldier ->identity().id());
			else
				PossiblyStartEnemyTaunt( pKillerSoldier, TAUNT_KILL, pSoldier->identity().id() );
		}

		// Cancel services here...
		TacticalActorMedicalServices::cancelReceiving(
			*pSoldier);
		TacticalActorMedicalServices::cancelProviding(
			*pSoldier);

		if ( pSoldier->renderState().hasMuzzleFlashSprite() )
		{
			LightSpriteDestroy( pSoldier->renderState().muzzleFlashSprite() );
			pSoldier->renderState().clearMuzzleFlashSprite();
		}
		(void)TacticalActorLighting::destroyPersonalLight(*pSoldier);

		//FREEUP GETTING HIT FLAG
		pSoldier->animationActivity().clearHit();

		// Find next closest team member!
		if ( pSoldier->roster().team() == gbPlayerNum )
		{
			// Set guy to close panel!
			// ONLY IF VISIBLE ON SCREEN
			if ( IsMercPortraitVisible( pSoldier->identity().id() ) )
			{
				fInterfacePanelDirty = DIRTYLEVEL2;
			}
			pSoldier->uiPresentation().queueDeadMercUi();

			if ( !gfKillingGuysForLosingBattle )
			{
				// ATE: THIS IS S DUPLICATE SETTING OF SOLDIER_DEAD. Is set in StrategicHandlePlayerTeamMercDeath()
				// also, but here it's needed to tell tectical to ignore this dude...
				// until StrategicHandlePlayerTeamMercDeath() can get called after death skull interface is done
				pSoldier->status().flags() |= SOLDIER_DEAD;

			}
		}
		else
		{
			//////////////////////////////////////////////////////////////
			// SANDRO - some changes here
			SoldierID attackerId = pSoldier->combatResult().currentAttacker();
			SoldierID assisterId = pSoldier->combatResult().previousAttacker();
			// If attacker is nobody, and we died, then set the last attacker(if exists) as our killer
			if ( attackerId == NOBODY )
			{
				if ( assisterId != NOBODY )
				{
					attackerId = pSoldier->combatResult().previousAttacker();
					assisterId = pSoldier->combatResult().earlierAttacker();
				}
				else if ( pSoldier->combatResult().earlierAttacker() != NOBODY )
				{
					attackerId = pSoldier->combatResult().earlierAttacker();
					assisterId = NOBODY;
				}
			}
			else if ( assisterId == NOBODY )
			{
				assisterId = pSoldier->combatResult().earlierAttacker();
			}

			//////////////////////////////////////////////////////////////
			TacticalActor* attacker =
				GetJa2SoldierRepository().resolve( attackerId );
			TacticalActor* assister =
				GetJa2SoldierRepository().resolve( assisterId );
			const TacticalActor* directAttacker =
				GetJa2SoldierRepository().resolve( pSoldier->combatResult().currentAttacker() );

			{
				// anv: note that attackerId can be already different from pSoldier->combatResult().currentAttacker()
				// IF this guy has an attacker and he's a good guy, play sound
				if ( directAttacker != nullptr )
				{
					if ( directAttacker->roster().team() == gbPlayerNum &&
						GetJa2PendingTacticalCombatActions() > 0 )
					{
						gTacticalStatus.fKilledEnemyOnAttack	= TRUE;
						gTacticalStatus.ubEnemyKilledOnAttack = pSoldier->identity().id();
						gTacticalStatus.ubEnemyKilledOnAttackLocation = pSoldier->position().gridNo();
						gTacticalStatus.bEnemyKilledOnAttackLevel = pSoldier->position().level();
						gTacticalStatus.ubEnemyKilledOnAttackKiller = attackerId;

						// also check if we are in mapscreen, if so update soldier's list
						if( GetCurrentScreen() == MAP_SCREEN )
						{
							ReBuildCharactersList( );
						}
					}
					else if ( pSoldier->awareness().visibility() == TRUE )
					{
						// We were a visible enemy, say laugh!
						if ( attacker != nullptr && Random(3) == 0 &&
							!CREATURE_OR_BLOODCAT( attacker ) )
						{
							// if the attacker was from the same team, play a curse, otherwise play a laugh
							if ( attacker->roster().side() == pSoldier->roster().side() )
								TacticalActorBattleSounds::play(*attacker,  BATTLE_SOUND_CURSE1 );
							else
								TacticalActorBattleSounds::play(*attacker,  BATTLE_SOUND_LAUGH1 );
						}
					}
				}

				// Handle NPC Dead
				HandleNPCTeamMemberDeath( pSoldier );

				// if a friendly with a profile, increment kills
				// militia also now track kills...
				if ( attacker != nullptr )
				{
					if ( attacker->roster().team() == gbPlayerNum )
					{
						// increment kills
						/////////////////////////////////////////////////////////////////////////////////////
						// SANDRO - experimental - more specific statistics of mercs
						switch(pSoldier->roster().soldierClass())
						{
							case SOLDIER_CLASS_ROBOT:
								gMercProfiles[ attacker->identity().profile() ].records.usKillsOthers++;
								break;
							case SOLDIER_CLASS_ELITE :
								gMercProfiles[ attacker->identity().profile() ].records.usKillsElites++;
								break;
							case SOLDIER_CLASS_ARMY :
								gMercProfiles[ attacker->identity().profile() ].records.usKillsRegulars++;
								break;
							case SOLDIER_CLASS_ADMINISTRATOR :
								gMercProfiles[ attacker->identity().profile() ].records.usKillsAdmins++;
								break;
							case SOLDIER_CLASS_CREATURE :
								gMercProfiles[ attacker->identity().profile() ].records.usKillsCreatures++;
								break;
							case SOLDIER_CLASS_ZOMBIE :
								gMercProfiles[ attacker->identity().profile() ].records.usKillsZombies++;
								break;
							case SOLDIER_CLASS_BANDIT:
								gMercProfiles[ attacker->identity().profile() ].records.usKillsOthers++;
								break;
							default :
								if ( CREATURE_OR_BLOODCAT( pSoldier ) )
									gMercProfiles[ attacker->identity().profile() ].records.usKillsCreatures++;
								else if ( ARMED_VEHICLE( pSoldier ) )
									gMercProfiles[ attacker->identity().profile() ].records.usKillsTanks++;
								else if ( pSoldier->roster().team() == CIV_TEAM && !pSoldier->aiBehavior().neutral() && pSoldier->roster().side() != gbPlayerNum )
									gMercProfiles[ attacker->identity().profile() ].records.usKillsHostiles++;
								else
								{
									gMercProfiles[ attacker->identity().profile() ].records.usKillsOthers++;

									// Flugente: dynamic opinions: if this guy is not hostile towards us, then some mercs will complain about killing civilians
									if (gGameExternalOptions.fDynamicOpinions && !(is_networked && pSoldier->roster().team() >= LAN_TEAM_ONE) && pSoldier->roster().team() != OUR_TEAM && (pSoldier->aiBehavior().neutral() || pSoldier->roster().side() == attacker->roster().side()) )
									{
										// not for killing animals though...
										if ( pSoldier->identity().bodyType() != CROW && pSoldier->identity().bodyType() != COW )
											HandleDynamicOpinionChange( attacker, OPINIONEVENT_CIVKILLER, TRUE, TRUE );
									}
								}
								break;
						}
						/////////////////////////////////////////////////////////////////////////////////////
						gStrategicStatus.usPlayerKills++;

						// Flugente: dynamic opinions: if this guy is not hostile towards us, then some mercs will complain about killing civilians
						if (gGameExternalOptions.fDynamicOpinions && !(is_networked && pSoldier->roster().team() >= LAN_TEAM_ONE))
						{
							if (pSoldier->roster().team() != OUR_TEAM && (pSoldier->aiBehavior().neutral() || pSoldier->roster().side() == attacker->roster().side()))
							{
								// not for killing animals though...
								if (pSoldier->identity().bodyType() != CROW && pSoldier->identity().bodyType() != COW)
									HandleDynamicOpinionChange(attacker, OPINIONEVENT_CIVKILLER, TRUE, TRUE);
							}
							else
							{
								// if this enemy was attacking a freshly wounded merc, it is likely they posed a real threat - the merc will be thankful for saving their life
								const TacticalActor* target =
									GetJa2SoldierRepository().resolve( pSoldier->targeting().targetId() );
								if (target != nullptr && target->vitals().bleeding() > 10)
								{
									AddOpinionEvent(target->identity().profile(), attacker->identity().profile(), OPINIONEVENT_BATTLE_SAVIOUR);
								}
								else
								{
									// complain about a fragthief, or thank for assistance - correct event is chosen internally
									HandleDynamicOpinionChange(attacker, OPINIONEVENT_FRAGTHIEF, TRUE, TRUE);
								}
							}
						}
					}
					else if ( attacker->roster().team() == MILITIA_TEAM )
					{
						// get a kill! 2 points!
						attacker->combatContribution().recordMilitiaKill();
					}
				}

				if ( assister != nullptr && assisterId != attackerId )
				{
					if ( assister->roster().team() == gbPlayerNum )
					{
						/////////////////////////////////////////////////////////////////////////////////////
						// SANDRO - new mercs' records
						if( attacker != nullptr )
						{
							if( attacker->roster().team() == gbPlayerNum )
								gMercProfiles[ assister->identity().profile() ].records.usAssistsMercs++;
							else if ( attacker->roster().team() == MILITIA_TEAM )
								gMercProfiles[ assister->identity().profile() ].records.usAssistsMilitia++;
							else
								gMercProfiles[ assister->identity().profile() ].records.usAssistsOthers++;
						}
						else
						{
							gMercProfiles[ assister->identity().profile() ].records.usAssistsOthers++;
						}
						/////////////////////////////////////////////////////////////////////////////////////
					}
					else if ( assister->roster().team() == MILITIA_TEAM )
					{
						// get an assist - 1 points
						assister->combatContribution().recordMilitiaAssist();
					}
				}
			}
		}

		// rftr: soldier bounty payout
		RebelCommand::ApplySoldierBounty(pSoldier);

		// Flugente: campaign stats
		gCurrentIncident.AddStat( pSoldier, CAMPAIGNHISTORY_TYPE_KILL );

		// Flugente: for raids, we need to keep track of killed forces
		if ( GetEnemyEncounterCode() == BLOODCAT_ATTACK_CODE ||
			GetEnemyEncounterCode() == ZOMBIE_ATTACK_CODE ||
			GetEnemyEncounterCode() == BANDIT_ATTACK_CODE )
		{
			AddRaidPersonnel( -( pSoldier->identity().bodyType() == BLOODCAT ), -( pSoldier->roster().soldierClass() == SOLDIER_CLASS_ZOMBIE ), -( pSoldier->roster().soldierClass() == SOLDIER_CLASS_BANDIT ) );
		}

		// Flugente: individual militia
		MILITIA militia;
		if ( GetMilitia( pSoldier->identity().individualMilitiaId(), &militia ) && !(militia.flagmask & MILITIAFLAG_DEAD) )
		{
			militia.healthratio = 0.0f;
			militia.flagmask |= MILITIAFLAG_DEAD;

			// note the current incident (when closing the incident, we only do this for those still alive)
			MILITIA_BATTLEREPORT report;
			report.id = GetIdOfCurrentlyOngoingIncident( );
			report.flagmask = MILITIA_BATTLEREPORT_FLAG_DIED;

			if ( pSoldier->combatContribution().hasMilitiaKills() )
				report.flagmask |= MILITIA_BATTLEREPORT_FLAG_KILLEDENEMY;

			militia.history.push_back( report );

			UpdateMilitia(militia);
		}
		
		if ( TurnSoldierIntoCorpse( pSoldier, TRUE, TRUE ) )
		{
			*pfMadeCorpse = TRUE;
		}

		// Flugente: VIPs
		if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_VIP )
		{
			DeleteVIP( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY() );
		}
		
		// Flugente: turncoats
		if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
			RemoveOneTurncoat( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->roster().soldierClass(), FALSE );

		// Flugente: additional dialogue
		if ( pSoldier->identity().profile() != NO_PROFILE )
		{
			AdditionalTacticalCharacterDialogue_AllInSector( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->deployment().sectorZ(), pSoldier->identity().profile(), ADE_NPC_DEATH,
				pSoldier->identity().profile(), pKillerSoldier ? pKillerSoldier->identity().profile() : NO_PROFILE, pSoldier->identity().bodyType() );
		}

		// Remove mad as target, one he has died!
		RemoveManAsTarget( pSoldier );

		// Re-evaluate visiblitiy for the team!
		BetweenTurnsVisibilityAdjustments();

		// 0verhaul: This is now handled in the death state transitions
		// if ( pSoldier->roster().team() != gbPlayerNum )
		// {
		//	if ( !pSoldier->animationActivity().externalDeath() )
				//	{
		//		// Release attacker
		//		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, code 497 = handle soldier death") );
		//		ReleaseSoldiersAttacker( pSoldier );
		//	}
		// }

		if ( !( *pfMadeCorpse ) )
		{
			fBuddyJustDead = TRUE;
		}

	}

	if ( pSoldier->vitals().health() > 0 )
	{
		// If we are here - something funny has heppende
		// We either have played a death animation when we are not dead, or we are calling
		// this ani code in an animation which is not a death animation
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Soldier Ani: Death animation called when not dead..." );
	}

	return( fBuddyJustDead );
}


void HandlePlayerTeamMemberDeathAfterSkullAnimation( TacticalActor *pSoldier )
{
	// 0verhaul:	This is now handled in the death state transition.
	// Release attacker
	// if ( !pSoldier->animationActivity().externalDeath() )
	// {
	//	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, code 497 = handle soldier death") );
	//	ReleaseSoldiersAttacker( pSoldier );
	// }

	HandlePlayerTeamMemberDeath( pSoldier );

	// now remove character from a squad
	RemoveCharacterFromSquads( pSoldier );
}

BOOLEAN CheckForAndHandleSoldierDeath( TacticalActor *pSoldier, BOOLEAN *pfMadeCorpse )
{

	if ( HandleSoldierDeath( pSoldier, pfMadeCorpse ) )
	{
		// Select approriate death
		switch( pSoldier->animationPlayback().state() )
		{
		case FLYBACK_HIT_DEATH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  FLYBACK_HITDEATH_STOP, 0, FALSE );
			break;

		case GENERIC_HIT_DEATH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLFORWARD_HITDEATH_STOP, 0, FALSE );
			break;

		case FALLBACK_HIT_DEATH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLBACK_HITDEATH_STOP, 0, FALSE );
			break;

		case PRONE_HIT_DEATH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_HITDEATH_STOP, 0, FALSE );
			break;

		case JFK_HITDEATH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  JFK_HITDEATH_STOP, 0, FALSE );
			break;

		case FALLOFF_DEATH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_DEATH_STOP, 0, FALSE );
			break;

		case FALLOFF_FORWARD_DEATH:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_FORWARD_DEATH_STOP, 0, FALSE );
			break;

		case WATER_DIE:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  WATER_DIE_STOP, 0, FALSE );
			break;

		case DEEP_WATER_DIE:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  DEEP_WATER_DIE_STOPPING, 0, FALSE );
			break;

		case COW_DYING:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  COW_DYING_STOP, 0, FALSE);
			break;

		case BLOODCAT_DYING:
			TacticalActorAnimationTransitions::changeState(*pSoldier,  BLOODCAT_DYING_STOP, 0, FALSE);
			break;

		default:

			// IF we are here - something is wrong - we should have an animation stop here
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Soldier Ani: CODE 440 Error, Death STOP not handled" );
		}

		return( TRUE );
	}

	return( FALSE );
}


//#define TESTFALLBACK
//#define TESTFALLFORWARD

void CheckForAndHandleSoldierIncompacitated( TacticalActor *pSoldier )
{
	INT32					sNewGridNo;

	if ( pSoldier->vitals().health() < OKLIFE )
	{
		// Cancel services here...
		TacticalActorMedicalServices::cancelReceiving(
			*pSoldier);
		TacticalActorMedicalServices::cancelProviding(
			*pSoldier);


		// If we are a monster, set life to zero ( no unconscious )
		switch( pSoldier->identity().bodyType() )
		{
		case ADULTFEMALEMONSTER:
		case AM_MONSTER:
		case YAF_MONSTER:
		case YAM_MONSTER:
		case LARVAE_MONSTER:
		case INFANT_MONSTER:
		case CRIPPLECIV:
		case ROBOTNOWEAPON:
		case QUEENMONSTER:
		case TANK_NW:
		case TANK_NE:
		case COMBAT_JEEP:

			pSoldier->vitals().health() = 0;
			break;
		}

		// OK, if we are in a meanwhile and this is elliot...
#ifdef JA2UB
//ja25: No queen
#else
		if ( AreInMeanwhile( ) )
		{
			TacticalActor *pQueen;

			pQueen = FindSoldierByProfileID( QUEEN, FALSE );

			if ( pQueen )
			{
				TriggerNPCWithGivenApproach( QUEEN, APPROACH_DONE_SLAPPED, FALSE );
			}
		}
#endif
		// We are unconscious now, play randomly, this animation continued, or a new death
		if ( TacticalActorLocomotion::checkRoofHit(*pSoldier) )
		{
			return;
		}

		// If guy is now dead, play sound!
		if ( pSoldier->vitals().health() == 0	)
		{
#ifdef JA2UB
//Ja25 No meanwhiles		
#else
			if ( !AreInMeanwhile() )
#endif
			{
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_DIE1 );
				pSoldier->dialogue().markDeathSoundPlayed();
			}
		}

		// Randomly fall back or forward, if we are in the standing hit animation
		if ( pSoldier->animationPlayback().state() == GENERIC_HIT_STAND || pSoldier->animationPlayback().state() == STANDING_BURST_HIT || pSoldier->animationPlayback().state() == RIFLE_STAND_HIT )
		{
			INT8		bTestDirection	= pSoldier->position().direction();
			BOOLEAN		fForceDirection = FALSE;
			BOOLEAN		fDoFallback		= FALSE;
			BOOLEAN		fAlwaysFallBack = FALSE; // added by SANDRO
			TacticalActor* attacker =
				GetJa2SoldierRepository().resolve( pSoldier->combatResult().currentAttacker() );


			// Lesh: lets fix dead humans fallback through obstacles

			// TRY FALLING BACKWARDS, ( ONLY IF WE ARE A MERC! )
#ifdef TESTFALLBACK
			if ( IS_MERC_BODY_TYPE( pSoldier ) )
#elif defined ( TESTFALLFORWARD )
			if ( 0 )
#else
			// SANDRO - if Martial Artist took someone down, always fall back if possible (for the fun)
			if ( attacker != nullptr && gGameOptions.fNewTraitSystem )
			{
				if ( HAS_SKILL_TRAIT( attacker, MARTIAL_ARTS_NT ) &&
					(!attacker->attackSelection().weapon() ||
					 ItemIsBrassKnuckles(attacker->inventory()[HANDPOS].usItem)) )
				{
					fAlwaysFallBack = TRUE;
				}
			}
			if ( (Random(100 ) > 40 || fAlwaysFallBack) && IS_MERC_BODY_TYPE( pSoldier ) && !IsProfileATerrorist( pSoldier->identity().profile() ) )
#endif
			{
				// CHECK IF WE HAVE AN ATTACKER, TAKE OPPOSITE DIRECTION!
				if ( attacker != nullptr )
				{
					// Find direction!
					bTestDirection =
						(INT8)GetDirectionFromGridNo( attacker->position().gridNo(), pSoldier );
					fForceDirection = TRUE;
				}

				sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( gOppositeDirection[ bTestDirection ] ) );

				if ( OKFallDirection( pSoldier, sNewGridNo, pSoldier->position().level(), bTestDirection, FLYBACK_HIT ) )
				{
					// CHECKED BEHIND GRIDS - OK
					fDoFallback = TRUE;
				}
				else
				{
					fDoFallback = FALSE;
				}

			}
			else
			{
				fDoFallback = FALSE;
			}

			if ( !fDoFallback )
			{
				// 1 ) REC DIRECTION
				// 2 ) SET FLAG FOR STARTING TO FALL
				(void)TacticalActorCombatReactions::
					beginFall(*pSoldier);
				TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLFORWARD_FROMHIT_STAND, 0, FALSE );
				return;
			}
			else
			{
				// ALL'S OK HERE..... IF WE FORCED DIRECTION, SET!
				if ( fForceDirection )
				{
					(void)TacticalActorOrientation::setDesiredDirection(*pSoldier, bTestDirection );
					(void)TacticalActorOrientation::setDirection(*pSoldier, bTestDirection );
				}
				(void)TacticalActorCombatReactions::
					beginFallback(
						*pSoldier,
						pSoldier->position().direction());
				return;
			}
		}
		else if ( pSoldier->animationPlayback().state() == GENERIC_HIT_CROUCH || pSoldier->animationPlayback().state() == CIV_COWER_HIT)
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLFORWARD_FROMHIT_CROUCH, 0 , FALSE);
			(void)TacticalActorCombatReactions::
				beginFall(*pSoldier);
			return;
		}
		else if ( pSoldier->animationPlayback().state() == GENERIC_HIT_PRONE )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_LAY_FROMHIT, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == ADULTMONSTER_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  ADULTMONSTER_DYING, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == LARVAE_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  LARVAE_DIE, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == QUEEN_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  QUEEN_DIE, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == CRIPPLE_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  CRIPPLE_DIE, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == ROBOTNW_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  ROBOTNW_DIE, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == INFANT_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  INFANT_DIE, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == COW_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  COW_DYING, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == BLOODCAT_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  BLOODCAT_DYING, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == WATER_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  WATER_DIE, 0 , FALSE );
			return;
		}
		else if ( pSoldier->animationPlayback().state() == DEEP_WATER_HIT )
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier,  DEEP_WATER_DIE, 0 , FALSE );
			return;
		}
		else
		{
			// We have missed something here - send debug msg
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "Soldier Ani: Genmeric hit not chained" );
		}
	}

}


BOOLEAN CheckForAndHandleSoldierDyingNotFromHit( TacticalActor *pSoldier )
{
	if ( pSoldier->vitals().health() == 0 )
	{
		TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_DIE1 );
		pSoldier->dialogue().markDeathSoundPlayed();

		// 0verhaul:	The bBeingAttackedCount is now obsolete.
		// Increment	being attacked count
		// pSoldier->bBeingAttackedCount++;

		// OJW - Send bleeding death
		if (is_networked)
		{
			if(pSoldier->roster().team()==0) send_death(pSoldier);
			else if(pSoldier->roster().team() <6 && ((gTacticalStatus.combatUI.ubTopMessageType == PLAYER_TURN_MESSAGE) || (gTacticalStatus.combatUI.ubTopMessageType == PLAYER_INTERRUPT_MESSAGE)))send_death(pSoldier);
			else if (pSoldier->roster().team() < 6 && (is_server)) send_death(pSoldier);
		}

		// Flugente: cows only have one death animation. If we're not in the proper aniamtion, enforce it, otherwise the corpse isn't created
		if ( pSoldier->identity().bodyType() == COW
			&& pSoldier->animationPlayback().state() != COW_HIT )
			TacticalActorAnimationTransitions::changeState(*pSoldier,  COW_DYING, 0, FALSE );

		if ( gGameSettings.fOptions[ TOPTION_BLOOD_N_GORE ] )
		{
			switch( pSoldier->animationPlayback().state() )
			{
			case FLYBACKHIT_STOP:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  FLYBACK_HIT_DEATH, 0, FALSE );
				break;

			case FALLFORWARD_FROMHIT_STAND:
			case FALLFORWARD_FROMHIT_CROUCH:
			case STAND_FALLFORWARD_STOP:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  GENERIC_HIT_DEATH, 0, FALSE );
				break;

			case FALLBACKHIT_STOP:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLBACK_HIT_DEATH, 0, FALSE );
				break;

			case PRONE_LAYFROMHIT_STOP:
			case PRONE_LAY_FROMHIT:

				TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_HIT_DEATH, 0, FALSE );
				break;

			case FALLOFF_STOP:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_DEATH, 0, FALSE );
				break;

			case FALLOFF_FORWARD_STOP:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_FORWARD_DEATH, 0, FALSE);
				break;

			case ADULTMONSTER_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  ADULTMONSTER_DYING, 0 , FALSE );
				break;

			case LARVAE_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  LARVAE_DIE, 0 , FALSE );
				break;

			case QUEEN_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  QUEEN_DIE, 0 , FALSE );
				break;

			case CRIPPLE_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  CRIPPLE_DIE, 0 , FALSE );
				break;

			case ROBOTNW_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  ROBOTNW_DIE, 0 , FALSE );
				break;

			case INFANT_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  INFANT_DIE, 0 , FALSE );
				break;

			case COW_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  COW_DYING, 0 , FALSE );
				break;

			case BLOODCAT_HIT:
				TacticalActorAnimationTransitions::changeState(*pSoldier,  BLOODCAT_DYING, 0 , FALSE );
				break;

			default:
				DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Control: Death state %d has no death hit", pSoldier->animationPlayback().state() ) );
				{
					BOOLEAN fMadeCorpse;
					CheckForAndHandleSoldierDeath( pSoldier, &fMadeCorpse );
				}
				break;

			}
		}
		else
		{
			BOOLEAN fMadeCorpse;

			CheckForAndHandleSoldierDeath( pSoldier, &fMadeCorpse );
		}
		return( TRUE );
	}

	return( FALSE );
}


BOOLEAN CheckForImproperFireGunEnd( TacticalActor *pSoldier )
{

	if ( AM_A_ROBOT( pSoldier ) )
	{
		return( FALSE );
	}

	//dnl ch72 260913
	OBJECTTYPE *pObjHand;
	BOOLEAN outOfAmmo;

	if(pSoldier->attackSelection().weaponMode() == WM_ATTACHED_GL || pSoldier->attackSelection().weaponMode() == WM_ATTACHED_GL_BURST || pSoldier->attackSelection().weaponMode() == WM_ATTACHED_GL_AUTO)
		pObjHand = FindAttachment_GrenadeLauncher(&pSoldier->inventory()[HANDPOS]);
	else
		pObjHand = TacticalActorEquipment::usedWeapon(*pSoldier, &pSoldier->inventory()[HANDPOS]);

	// Extracted from EnoughAmmo() to avoid double calculation of pObjHand
	if (Item[ pObjHand->usItem ].usItemClass & IC_LAUNCHER)
		outOfAmmo = (FindAttachmentByClass( pObjHand, IC_GRENADE ) == NULL && FindAttachmentByClass( pObjHand, IC_BOMB ) == NULL);
	else
		outOfAmmo = (*pObjHand)[0]->data.gun.ubGunShotsLeft == 0;

	// Check single hand for jammed status, ( or ammo is out.. )
	if ( (*pObjHand)[0]->data.gun.bGunAmmoStatus < 0 || outOfAmmo )
	{
		// If we have 2 pistols, donot go back!
		if ( Item[ pSoldier->inventory()[ SECONDHANDPOS ].usItem ].usItemClass != IC_GUN )
		{
			// OK, put gun down....
			(void)TacticalActorRangedActions::readyFacing(
				*pSoldier,
				pSoldier->position().direction(),
				true,
				false);
			return( TRUE );
		}
	}

	// SANDRO: if we are holding up a very heavy gun, and can't do it anymore, lower it
	if ( gGameExternalOptions.ubEnergyCostForWeaponWeight && pSoldier->vitals().breath() < OKBREATH )
	{		
		// Check for breath collapse, though this should rarely happen
		if (TacticalActorRecovery::checkBreathCollapse(*pSoldier))
		{
			UnSetUIBusy( pSoldier->identity().id() );
			(void)TacticalActorRecovery::collapse(*pSoldier);
			pSoldier->collapseState().clearBreathCollapse();
			return( TRUE );
		}
		// ok, if this gun is rather heavy, and cost us at least 3 energy points per turn, and we got very low on breath
		else if ( (GetBPCostPer10APsForGunHolding( pSoldier ) * 10) >= (300 * gGameExternalOptions.ubEnergyCostForWeaponWeight / 100) ) 
		{
			// throw quote
			if ( !pSoldier->dialogue().hasSaid(SOLDIER_QUOTE_SAID_LOW_BREATH) )
			{
				TacticalCharacterDialogue( pSoldier, QUOTE_OUT_OF_BREATH );
				pSoldier->dialogue().markSaid(SOLDIER_QUOTE_SAID_LOW_BREATH);
			}
			// Put gun down....
			(void)TacticalActorRangedActions::readyFacing(
				*pSoldier,
				pSoldier->position().direction(),
				true,
				false);
			return( TRUE );
		}
	}

	return( FALSE );

}


BOOLEAN OKHeightDest( TacticalActor *pSoldier, INT32 sNewGridNo )
{
	if ( pSoldier->position().level() == 0 )
	{
		return( TRUE );
	}

	// Check if there is a lower place here....
	if ( IsLowerLevel( sNewGridNo ) )
	{
		return( FALSE );
	}

	return( TRUE );
}


BOOLEAN HandleUnjamAnimation( TacticalActor *pSoldier )
{
	// OK, play intermediate animation here..... save in pending animation data, the current
	// code we are at!
	pSoldier->pendingAction().primaryData() = pSoldier->animationPlayback().code();
	pSoldier->pendingAction().secondaryData()	= pSoldier->animationPlayback().state();
	// Check what animatnion we should do.....
	switch( pSoldier->animationPlayback().state() )
	{
	case SHOOT_RIFLE_STAND:
	case STANDING_BURST:
	case FIRE_STAND_BURST_SPREAD:
		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  STANDING_SHOOT_UNJAM, 0 , FALSE );
		return( TRUE );

	case PRONE_BURST:
	case SHOOT_RIFLE_PRONE:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_SHOOT_UNJAM, 0 , FALSE );
		return( TRUE );

	case CROUCHED_BURST:
	case SHOOT_RIFLE_CROUCH:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  CROUCH_SHOOT_UNJAM, 0 , FALSE );
		return( TRUE );

	case SHOOT_DUAL_STAND:
	case BURST_DUAL_STAND:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  STANDING_SHOOT_DWEL_UNJAM, 0 , FALSE );
		return( TRUE );

	case SHOOT_DUAL_PRONE:
	case BURST_DUAL_PRONE:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_SHOOT_DWEL_UNJAM, 0 , FALSE );
		return( TRUE );

	case SHOOT_DUAL_CROUCH:
	case BURST_DUAL_CROUCH:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  CROUCH_SHOOT_DWEL_UNJAM, 0 , FALSE );
		return( TRUE );

	case FIRE_LOW_STAND:
	case FIRE_BURST_LOW_STAND:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  STANDING_SHOOT_LOW_UNJAM, 0 , FALSE );
		return( TRUE );

	case SHOOT_ALTERNATIVE_STAND:
	case BURST_ALTERNATIVE_STAND:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  UNJAM_ALTERNATIVE_STAND, 0 , FALSE );
		return( TRUE );

	case LOW_SHOT_ALTERNATIVE_STAND:
	case LOW_BURST_ALTERNATIVE_STAND:

		// Normal shoot rifle.... play
		TacticalActorAnimationTransitions::changeState(*pSoldier,  LOW_UNJAM_ALTERNATIVE_STAND, 0 , FALSE );
		return( TRUE );
		
	}

	return( FALSE );
}





BOOLEAN OKFallDirection( TacticalActor *pSoldier, INT32 sGridNo, INT8 bLevel, UINT8 ubTestDirection, UINT16 usAnimState )
{
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"OKFallDirection");
	STRUCTURE_FILE_REF *	pStructureFileRef;
	UINT16								usAnimSurface;
	UINT8					bOverTerrainType;

	// WANNE - MP: MP crashed here, so I added the prevention	
	if (TileIsOutOfBounds(sGridNo))
		return ( FALSE );

	// How are the movement costs?
	if ( gubWorldMovementCosts[ sGridNo ][ ubTestDirection ][ bLevel ] > TRAVELCOST_SHORE )
	{
		return( FALSE );
	}

	bOverTerrainType = GetTerrainType( sGridNo);

	// WANNE.WATER: If our soldier is not on the ground level and the tile is a "water" tile, then simply set the tile to "FLAT_GROUND"
	// This should fix "problems" for special modified maps
	if ( TERRAIN_IS_WATER( bOverTerrainType) && bLevel > 0 )
		bOverTerrainType = FLAT_GROUND;

	//NOT ok if in water....
	if ( TERRAIN_IS_WATER( bOverTerrainType) )
	{
		return( FALSE );
	}

	// How are we for OK dest?
	if (!NewOKDestination( pSoldier, sGridNo, TRUE, bLevel ) )
	{
		return( FALSE );
	}

	usAnimSurface = DetermineSoldierAnimationSurface( pSoldier, usAnimState );
	pStructureFileRef = GetAnimationStructureRef( pSoldier->identity().id(), usAnimSurface, usAnimState );

	if ( pStructureFileRef )
	{
		UINT16		usStructureID;
		INT32			sTestGridNo;

		// must make sure that structure data can be added in the direction of the target

		usStructureID = pSoldier->identity().id();

		// Okay this is really SCREWY but it's due to the way this function worked before and must
		// work now.	The function is passing in an adjacent gridno but we need to place the structure
		// data in the tile BEFORE.	So we take one step back in the direction opposite to bTestDirection
		// and use that gridno
		sTestGridNo = NewGridNo( sGridNo, DirectionInc( gOppositeDirection[ ubTestDirection ] ) );

		if ( ! OkayToAddStructureToWorld( sTestGridNo, bLevel, &(pStructureFileRef->pDBStructureRef[ gOneCDirection[ ubTestDirection ] ]), usStructureID ) )
		{
			// can't go in that dir!
			return( FALSE );
		}
	}

	return( TRUE );
}

BOOLEAN HandleCheckForDeathCommonCode( TacticalActor *pSoldier )
{
	//shadooow: fix for going back to cower animation after collapsing
	if (TacticalActorRecovery::checkBreathCollapse(*pSoldier) ||
		pSoldier->collapseState().tactical())
	{
		pSoldier->animationIntent().clearPendingAnimations();
	}
	else
	{
		// Do we have a primary pending animation?
		if (pSoldier->animationIntent().secondaryPendingAnimation() != NO_PENDING_ANIMATION)
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier, pSoldier->animationIntent().secondaryPendingAnimation(), 0, FALSE);
			pSoldier->animationIntent().clearSecondaryPendingAnimation();
			return(TRUE);
		}

		// CHECK IF WE HAVE A PENDING ANIMATION HERE
		if (pSoldier->animationIntent().pendingAnimation() != NO_PENDING_ANIMATION)
		{
			TacticalActorAnimationTransitions::changeState(*pSoldier, pSoldier->animationIntent().pendingAnimation(), 0, FALSE);
			pSoldier->animationIntent().clearPendingAnimation();
			return(TRUE);
		}
	}
	// OTHERWISE, GOTO APPROPRIATE STOPANIMATION!
	pSoldier->collapseState().collapse();

	// CC has requested - handle sight here...
	HandleSight( pSoldier, SIGHT_LOOK );

	// ATE: If it is our turn, make them try to getup...
	if ( GetJa2TacticalCurrentTeam() == pSoldier->roster().team() )
	{
		// Try to getup...
		(void)TacticalActorRecovery::beginGetUp(*pSoldier);

		// Check this to see if above worked
		if ( !pSoldier->collapseState().tactical() )
		{
			return( TRUE );
		}
	}

	switch( pSoldier->animationPlayback().state() )
	{
	case FLYBACK_HIT:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FLYBACKHIT_STOP, 0, FALSE );
		break;

	case GENERIC_HIT_DEATHTWITCHNB:
	case FALLFORWARD_FROMHIT_STAND:
	case ENDFALLFORWARD_FROMHIT_CROUCH:

		TacticalActorAnimationTransitions::changeState(*pSoldier,  STAND_FALLFORWARD_STOP, 0, FALSE );
		break;

	case FALLBACK_HIT_DEATHTWITCHNB:
	case FALLBACK_HIT_STAND:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLBACKHIT_STOP, 0, FALSE );
		break;

	case PRONE_HIT_DEATHTWITCHNB:
	case PRONE_LAY_FROMHIT:

		TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_LAYFROMHIT_STOP, 0, FALSE );
		break;

	case FALLOFF:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_STOP, 0, FALSE );
		break;

	case FALLFORWARD_ROOF:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_FORWARD_STOP, 0, FALSE);
		break;

	default:
		// IF we are here - something is wrong - we should have a death animation here
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Ani: unconscious hit sequence needed for animation %d", pSoldier->animationPlayback().state() ) );

	}
	// OTHERWISE, GOTO APPROPRIATE STOPANIMATION!
	pSoldier->collapseState().collapse();

	// ATE: If it is our turn, make them try to getup...
	if ( GetJa2TacticalCurrentTeam() == pSoldier->roster().team() )
	{
		// Try to getup...
		(void)TacticalActorRecovery::beginGetUp(*pSoldier);

		// Check this to see if above worked
		if ( !pSoldier->collapseState().tactical() )
		{
			return( TRUE );
		}
	}

	switch( pSoldier->animationPlayback().state() )
	{
	case FLYBACK_HIT:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FLYBACKHIT_STOP, 0, FALSE );
		break;

	case GENERIC_HIT_DEATHTWITCHNB:
	case FALLFORWARD_FROMHIT_STAND:
	case ENDFALLFORWARD_FROMHIT_CROUCH:

		TacticalActorAnimationTransitions::changeState(*pSoldier,  STAND_FALLFORWARD_STOP, 0, FALSE );
		break;

	case FALLBACK_HIT_DEATHTWITCHNB:
	case FALLBACK_HIT_STAND:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLBACKHIT_STOP, 0, FALSE );
		break;

	case PRONE_HIT_DEATHTWITCHNB:
	case PRONE_LAY_FROMHIT:

		TacticalActorAnimationTransitions::changeState(*pSoldier,  PRONE_LAYFROMHIT_STOP, 0, FALSE );
		break;

	case FALLOFF:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_STOP, 0, FALSE );
		break;

	case FALLFORWARD_ROOF:
		TacticalActorAnimationTransitions::changeState(*pSoldier,  FALLOFF_FORWARD_STOP, 0, FALSE);
		break;

	default:
		// IF we are here - something is wrong - we should have a death animation here
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Ani: unconscious hit sequence needed for animation %d", pSoldier->animationPlayback().state() ) );

	}

	return( TRUE );
}


void KickOutWheelchair( TacticalActor *pSoldier )
{
	INT32 sNewGridNo;

	// Move forward one gridno....
	sNewGridNo = NewGridNo( pSoldier->position().gridNo(), DirectionInc( pSoldier->position().direction() ) );

	// ATE: Make sure that the gridno is unoccupied!
	if ( !NewOKDestination( pSoldier, sNewGridNo, TRUE, pSoldier->position().level() ) )
	{
		// We should just stay put - will look kind of funny but nothing I can do!
		sNewGridNo = pSoldier->position().gridNo();
	}

	(void)TacticalActorRouteExecution::stopAt(*pSoldier, sNewGridNo, pSoldier->position().direction() );
	pSoldier->identity().bodyType() = REGMALE;
	if ( CampaignMercenaryPolicy(GetGameContext().capabilities()).isProfile(
			pSoldier->identity().profile(), CampaignProfileCode::Role::Slay) &&
		pSoldier->roster().team() == CIV_TEAM &&
		!pSoldier->aiBehavior().neutral() )
	{
		HandleNPCDoAction( pSoldier->identity().profile(), NPC_ACTION_THREATENINGLY_RAISE_GUN, 0 );
	}
	else
	{
		TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  STANDING, 0 , TRUE );
	}

	// If this person has a profile ID, set body type to regmale
	if ( pSoldier->identity().profile() != NO_PROFILE )
	{
		gMercProfiles[ pSoldier->identity().profile() ].ubBodyType = REGMALE;
	}

}
