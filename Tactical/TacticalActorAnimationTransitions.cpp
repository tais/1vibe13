#include "TacticalActorAnimationTransitions.h"

#include "Grid Direction.h"
#include "TacticalActorCrowBehavior.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorEvents.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalActorStateFlags.h"

#include "TacticalActorBattleSounds.h"
#include "TacticalActorAppearance.h"
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


#include "connect.h"

void HandleVehicleMovementSound(TacticalActor* actor, BOOLEAN enabled);
void PlaySoldierFootstepSound(TacticalActor* actor);
void SetSoldierAniSpeed(TacticalActor* actor);
void SetSoldierLocatorOffsets(TacticalActor* actor);

bool TacticalActorAnimationTransitions::changeState(TacticalActor& subject, UINT16 usNewState, UINT16 usStartingAniCode, bool fForce)
{
	if (usNewState >= NUMANIMATIONSTATES)
	{
		return false;
	}

	EV_S_CHANGESTATE	SChangeState;

	// Send message that we have changed states
	SChangeState.usNewState = usNewState;
	SChangeState.usSoldierID = subject.identity().id();
	SChangeState.uiUniqueId = subject.identity().incarnation();
	SChangeState.usStartingAniCode = usStartingAniCode;
	SChangeState.sXPos = subject.position().worldXInt();
	SChangeState.sYPos = subject.position().worldYInt();
	SChangeState.fForce = fForce;
	SChangeState.usNewDirection = subject.position().direction();
	SChangeState.usTargetGridNo = subject.targeting().gridNo();


	//AddGameEvent( S_CHANGESTATE, 0, &SChangeState );
	if ( (is_server && subject.identity().id() < 120) || (is_client && subject.identity().id() < 20) )
	{
		send_changestate( &SChangeState );
	}
	//else if((is_client && !is_server) && (subject.identity().id() < 20 || (subject.identity().id() < 120 && GetJa2TacticalCurrentTeam() == OUR_TEAM)))
	//{
	//	TacticalActorAnimationTransitions::initializeAnimation(subject,  SChangeState.usNewState, SChangeState.usStartingAniCode, SChangeState.fForce );
	//	send_changestate(&SChangeState);
	//}
	//else if (!is_client)
	//{
	TacticalActorAnimationTransitions::initializeAnimation(subject,  SChangeState.usNewState, SChangeState.usStartingAniCode, SChangeState.fForce );
	//}
	return(TRUE);

}


// This function reevaluates the stance if the guy sees us!
BOOLEAN ReevaluateEnemyStance( TacticalActor *pSoldier, UINT16 usAnimState )
{
	SoldierID	iClosestEnemy = NOBODY;
	INT16		sTargetXPos, sTargetYPos;
	BOOLEAN		fReturnVal = FALSE;
	INT16		sDist, sClosestDist = 10000;

	// make the chosen one not turn to face us
	if ( OK_ENEMY_MERC( pSoldier ) && pSoldier->identity().id() != gTacticalStatus.ubTheChosenOne && gAnimControl[usAnimState].ubEndHeight == ANIM_STAND && !(pSoldier->status().flags() & SOLDIER_UNDERAICONTROL) )
	{
		if ( pSoldier->animationActivity().turningFromProneMode() == TURNING_FROM_PRONE_OFF )
		{
			// If we are a queen and see enemies, goto ready
			if ( pSoldier->identity().bodyType() == QUEENMONSTER )
			{
				if ( gAnimControl[usAnimState].uiFlags & (ANIM_BREATH) )
				{
					if ( pSoldier->awareness().opponentCount() > 0 )
					{
						TacticalActorAnimationTransitions::initializeAnimation(*pSoldier,  QUEEN_INTO_READY, 0, TRUE );
						return(TRUE);
					}
				}
			}

			// ATE: Don't do &subject if we're not a merc.....
			if ( !IS_MERC_BODY_TYPE( pSoldier ) )
			{
				return(FALSE);
			}

			if ( gAnimControl[usAnimState].uiFlags & (ANIM_MERCIDLE | ANIM_BREATH) )
			{
				if ( pSoldier->awareness().opponentCount() > 0 )
				{
					// Pick a guy &subject buddy sees and turn towards them!
					for ( SoldierID cnt = gTacticalStatus.Team[OUR_TEAM].bFirstID; cnt <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++cnt )
					{
						if ( pSoldier->awareness().opponentKnowledge()[cnt] == SEEN_CURRENTLY )
						{
							TacticalActor* candidate =
								GetJa2SoldierRepository().resolve( cnt );
							if ( candidate == nullptr )
								continue;
							sDist = PythSpacesAway( pSoldier->position().gridNo(), candidate->position().gridNo() );
							if ( sDist < sClosestDist )
							{
								sClosestDist = sDist;
								iClosestEnemy = cnt;
							}
						}
					}

					TacticalActor* closestEnemy =
						GetJa2SoldierRepository().resolve( iClosestEnemy );
					if ( closestEnemy != nullptr )
					{

						// SANDRO - do we want &subject to be happening at all? It is somehow unwelcomed in IIS and for alternative weapon holding,
						// besides it is rather illogical... well, I've made an ini setting for it
						if ( gGameExternalOptions.fNoEnemyAutoReadyWeapon == 0 )
						{
							// Change to fire ready animation
							ConvertGridNoToXY( closestEnemy->position().gridNo(), &sTargetXPos, &sTargetYPos );

							pSoldier->animationActivity().readyCostWaived() = TRUE;

							// Ready weapon
							fReturnVal = TacticalActorRangedActions::readyToward(
								*pSoldier,
								sTargetXPos,
								sTargetYPos,
								false,
								AIDecideHipOrShoulderStance(
									pSoldier,
									closestEnemy->position().gridNo()));

							return(fReturnVal);
						}
						// &subject makes the soldier to only turn towards our direction, instead of raising his weapon
						else if ( gGameExternalOptions.fNoEnemyAutoReadyWeapon == 2 )
						{
							//ConvertGridNoToXY( iClosestEnemy->sGridNo, &sTargetXPos, &sTargetYPos );
							//sFacingDir = GetDirectionFromXY( sXPos, sYPos, pSoldier );
							INT16 sFacingDir = GetDirectionToGridNoFromGridNo( pSoldier->position().gridNo(), closestEnemy->position().gridNo() );

							if ( sFacingDir != pSoldier->position().direction() )
							{
								INT16 sAPCost = GetAPsToLook( pSoldier );

								// Check AP cost...
								if ( !EnoughPoints( pSoldier, sAPCost, 0, TRUE ) )
								{
									return(FALSE);
								}

								SendSoldierSetDesiredDirectionEvent( pSoldier, sFacingDir );
								//fReturnVal = MakeSoldierTurn( pSoldier, sXPos, sYPos );

								return(TRUE);
							}
						}
					}

				}
			}
		}
	}
	return(FALSE);

}


void CheckForFreeupFromHit( TacticalActor *pSoldier, UINT32 uiOldAnimFlags, UINT32 uiNewAnimFlags, UINT16 usOldAniState, UINT16 usNewState )
{
	// THIS COULD POTENTIALLY CALL EVENT_INITNEWAnim() if the GUY was SUPPRESSED
	// CHECK IF THE OLD ANIMATION WAS A HIT START THAT WAS NOT FOLLOWED BY A HIT FINISH
	// IF SO, RELEASE ATTACKER FROM ATTACKING

	// If old and new animations are the same, do nothing!
	if ( usOldAniState == QUEEN_HIT && usNewState == QUEEN_HIT )
	{
		return;
	}

	//if ( usOldAniState != usNewState && ( uiOldAnimFlags & ANIM_HITSTART ) && !( uiNewAnimFlags & ANIM_HITFINISH ) && !( uiNewAnimFlags & ANIM_IGNOREHITFINISH ) && !(pSoldier->status().flags() & SOLDIER_TURNINGFROMHIT ) )
	if ( usOldAniState != usNewState && (uiOldAnimFlags & ANIM_HITSTART) && !(uiNewAnimFlags & ANIM_HITFINISH) && !(pSoldier->status().flags() & SOLDIER_TURNINGFROMHIT) )
	{
		// 0verhaul:  Yet again, &subject is handled by the state transition code.
		// Release attacker
		// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, normal hit animation ended NEW: %s ( %d ) OLD: %s ( %d )", gAnimControl[ usNewState ].zAnimStr, usNewState, gAnimControl[ usOldAniState ].zAnimStr, pSoldier->animationPlayback().previousState() ) );
		// ReleaseSoldiersAttacker( pSoldier );

		//FREEUP GETTING HIT FLAG
		// pSoldier->animationActivity().hitPhase() = FALSE;

		// ATE: if our guy, have 10% change of say damn, if still conscious...
		if ( pSoldier->roster().team() == gbPlayerNum && pSoldier->vitals().health() >= OKLIFE )
		{
			if ( Random( 10 ) == 0 )
			{
				TacticalActorBattleSounds::play(*pSoldier,  (INT8)(BATTLE_SOUND_CURSE1) );
			}
		}
	}

	// CHECK IF WE HAVE FINSIHED A HIT WHILE DOWN
	// OBLY DO THIS IF 1 ) We are dead already or 2 ) We are alive still
	if ( (uiOldAnimFlags & ANIM_HITWHENDOWN) && ((pSoldier->status().flags() & SOLDIER_DEAD) || pSoldier->vitals().health() != 0) )
	{
		// 0verhaul:  Ditto
		// Release attacker
		// DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("@@@@@@@ Releasesoldierattacker, animation of kill on the ground ended") );
		// ReleaseSoldiersAttacker( pSoldier );

		//FREEUP GETTING HIT FLAG
		// pSoldier->animationActivity().hitPhase() = FALSE;

		if ( pSoldier->vitals().health() == 0 ) // SANDRO added check
		{
			//ATE: Set previous attacker's value!
			// This is so that the killer can say their killed quote....
			pSoldier->combatResult().restorePreviousAttacker();
		}
	}
}


// THIS IS CALLED FROM AN EVENT ( S_CHANGESTATE )!
bool TacticalActorAnimationTransitions::initializeAnimation(TacticalActor& subject, UINT16 usNewState, UINT16 usStartingAniCode, bool fForce)
{
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "EVENT_InitNewSoldierAnim" );
	INT32	usNewGridNo = 0;
	INT16	sAPCost = 0;
	INT32	sBPCost = 0;
	UINT32	uiOldAnimFlags;
	UINT32  uiNewAnimFlags;
	UINT16	usSubState;
	UINT16	usItem;
	BOOLEAN	fTryingToRestart = FALSE;

	CHECKF( usNewState < NUMANIMATIONSTATES );

	///////////////////////////////////////////////////////////////////////
	//			DO SOME CHECKS ON OUR NEW ANIMATION!
	/////////////////////////////////////////////////////////////////////

	if (usNewState == THROW_GRENADE_STANCE || usNewState == LOB_GRENADE_STANCE || usNewState == THROW_ITEM || usNewState == THROW_ITEM_CROUCHED)
	{
		UINT16 usItem = subject.runtime().pendingAction.grenadeItem;
		UINT8 ubVolume = Weapon[usItem].ubAttackVolume;

		// play grenade pin sound
		if (usItem && Item[usItem].usItemClass == IC_GRENADE)
		{
			CHAR8	zFilename[512];
			sprintf(zFilename, "");

			BOOLEAN fDelay = FALSE;

			if (usNewState == THROW_GRENADE_STANCE && gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_STAND && subject.identity().bodyType() < REGFEMALE)
			{
				fDelay = TRUE;
			}

			// check if custom sound is set in Weapons.xml
			if (Weapon[usItem].sSound)
			{
				PlayJA2Sample(Weapon[Item[usItem].ubClassIndex].sSound, RATE_11025, SoundVolume(MIDVOLUME, subject.position().gridNo()), 1, SoundDir(subject.position().gridNo()));
			}
			else
			{
				if (ItemIsFlare(usItem) ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_SIGNAL_SMOKE ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_FLARE)
				{
					if (usItem == BREAK_LIGHT)
					{
						if (fDelay)
							sprintf(zFilename, "sounds\\grenade\\grenade_breaklight_delay.ogg");
						else
							sprintf(zFilename, "sounds\\grenade\\grenade_breaklight.ogg");
					}
					else
					{
						if (fDelay)
							sprintf(zFilename, "sounds\\grenade\\grenade_flare_delay.ogg");
						else
							sprintf(zFilename, "sounds\\grenade\\grenade_flare.ogg");
					}
				}
				else if (Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_NORMAL ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_STUN ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_FLASHBANG)
				{
					if (fDelay)
						sprintf(zFilename, "sounds\\grenade\\grenade_pin_delay.ogg");
					else
						sprintf(zFilename, "sounds\\grenade\\grenade_pin.ogg");
				}
				else if (Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_SMOKE ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_TEARGAS ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_MUSTGAS ||
					Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_SIGNAL_SMOKE)
				{
					if (fDelay)
						sprintf(zFilename, "sounds\\grenade\\grenade_gas_delay.ogg");
					else
						sprintf(zFilename, "sounds\\grenade\\grenade_gas.ogg");
				}
				else if (Explosive[Item[usItem].ubClassIndex].ubType == EXPLOSV_BURNABLEGAS)
				{
					if (fDelay)
						sprintf(zFilename, "sounds\\grenade\\grenade_fire_delay.ogg");
					else
						sprintf(zFilename, "sounds\\grenade\\grenade_fire.ogg");
				}

				if (strlen(zFilename) > 0 && FileExists(zFilename))
				{
					PlayJA2SampleFromFile(zFilename, RATE_11025, SoundVolume(MIDVOLUME, subject.position().gridNo()), 1, SoundDir(subject.position().gridNo()));
				}
			}
		}
	}

	// If we are NOT loading a game, continue normally
	if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
	{
		usItem = subject.inventory()[HANDPOS].usItem;

		// CHECK IF WE ARE TRYING TO INTURRUPT A SCRIPT WHICH WE DO NOT WANT INTERRUPTED!
		if ( subject.animationActivity().nonInterruptible() )
		{
			return(FALSE);
		}

		if ( subject.animationActivity().realtimeNonInterruptible() )
		{
			if ( !(IsJa2TacticalCombatActive()) )
			{
				return(FALSE);
			}
			else
			{
				subject.animationActivity().realtimeNonInterruptible() = FALSE;
			}
		}


		// Check if we can restart &subject animation if it's the same as our current!
		if ( usNewState == subject.animationPlayback().state() )
		{
			if ( (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_NORESTART) && !fForce )
			{
				fTryingToRestart = TRUE;
			}
		}

		// Check state, if we are not at the same height, set &subject ani as the pending one and
		// change stance accordingly
		// ATE: ONLY IF WE ARE STARTING AT START OF ANIMATION!
		if ( usStartingAniCode == 0 )
		{
			if ( gAnimControl[usNewState].ubHeight != gAnimControl[subject.animationPlayback().state()].ubEndHeight &&
				 !(gAnimControl[usNewState].uiFlags & (ANIM_STANCECHANGEANIM | ANIM_IGNORE_AUTOSTANCE)) )
			{

				// Check if we are going from crouched height to prone height, and adjust fast turning accordingly
				// Make guy turn while crouched THEN go into prone
				if ( (gAnimControl[usNewState].ubEndHeight == ANIM_PRONE && gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_CROUCH) && !(IsJa2TacticalCombatActive()) )
				{
					subject.animationActivity().turningUntilDone() = TRUE;
					subject.animationIntent().pendingStance() = gAnimControl[usNewState].ubEndHeight;
					subject.animationIntent().pendingAnimation() = usNewState;
					return(TRUE);
				}
				// Check if we are in realtime and we are going from stand to crouch
				else if ( gAnimControl[usNewState].ubEndHeight == ANIM_CROUCH && gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_STAND && (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_MOVING) && ((gTacticalStatus.uiFlags & REALTIME) || !(IsJa2TacticalCombatActive())) )
				{
					subject.animationIntent().desiredHeight() = gAnimControl[usNewState].ubEndHeight;
					// Continue with &subject course of action IE: Do animation and skip from stand to crouch
				}
				// Check if we are in realtime and we are going from crouch to stand
				else if ( gAnimControl[usNewState].ubEndHeight == ANIM_STAND && gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_CROUCH && (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_MOVING) && ((gTacticalStatus.uiFlags & REALTIME) || !(IsJa2TacticalCombatActive())) && subject.animationPlayback().state() != HELIDROP )
				{
					subject.animationIntent().desiredHeight() = gAnimControl[usNewState].ubEndHeight;
					// Continue with &subject course of action IE: Do animation and skip from stand to crouch
				}
				else
				{
					// ONLY DO FOR EVERYONE BUT PLANNING GUYS
					if ( subject.identity().id() < MAX_NUM_SOLDIERS )
					{
						// Set our next moving animation to be pending, after
						subject.animationIntent().pendingAnimation() = usNewState;
						// Set new state to be animation to move to new stance
						SendChangeSoldierStanceEvent( &subject, gAnimControl[usNewState].ubHeight );
						return(TRUE);
					}
				}
			}

			// Going from hip stance to shoulder stance, skip first 2 frames for smoother graphic look
			if ( usNewState == READY_RIFLE_STAND && (gAnimControl[subject.animationPlayback().state()].uiFlags & (ANIM_ALT_WEAPON_HOLDING)) )
			{
				if ( subject.identity().bodyType() == BIGMALE )
					usStartingAniCode = 1; // &subject looks better for big mercs
				else
					usStartingAniCode = 2;
			}
			// Going from shoulder stance to hip stance
			else if ( usNewState == READY_ALTERNATIVE_STAND && (gAnimControl[subject.animationPlayback().state()].uiFlags & (ANIM_FIREREADY | ANIM_FIRE)) )
			{
				if (ItemIsTwoHanded(subject.inventory()[HANDPOS].usItem))
					usStartingAniCode = 1;
				else
					usStartingAniCode = 2;
			}
		}

		if ( usNewState == ADJACENT_GET_ITEM )
		{
			if ( subject.animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				(void)TacticalActorOrientation::setDesiredDirection(subject, subject.animationIntent().pendingDirection(), FALSE, subject.animationPlayback().state() );
				subject.animationIntent().clearPendingDirection();
				subject.animationIntent().pendingAnimation() = ADJACENT_GET_ITEM;
				subject.animationActivity().turningUntilDone() = TRUE;
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(subject);
				return(TRUE);
			}
		}

		if ( usNewState == ADJACENT_GET_ITEM_CROUCHED )
		{
			if ( subject.animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				(void)TacticalActorOrientation::setDesiredDirection(subject, subject.animationIntent().pendingDirection(), FALSE, subject.animationPlayback().state() );
				subject.animationIntent().clearPendingDirection();
				subject.animationIntent().pendingAnimation() = ADJACENT_GET_ITEM_CROUCHED;
				subject.animationActivity().turningUntilDone() = TRUE;
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(subject);
				return(TRUE);
			}
		}

		if ( usNewState == CLIMBUPROOF )
		{
			if ( subject.animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				(void)TacticalActorOrientation::setDesiredDirection(subject, subject.animationIntent().pendingDirection() );
				subject.animationIntent().clearPendingDirection();
				subject.animationIntent().pendingAnimation() = CLIMBUPROOF;
				subject.animationActivity().turningUntilDone() = TRUE;
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(subject);
				return(TRUE);
			}
		}

		if ( usNewState == CLIMBDOWNROOF )
		{
			if ( subject.animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				(void)TacticalActorOrientation::setDesiredDirection(subject, subject.animationIntent().pendingDirection() );
				subject.animationIntent().clearPendingDirection();
				subject.animationIntent().pendingAnimation() = CLIMBDOWNROOF;
				subject.animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
				subject.animationActivity().turningUntilDone() = TRUE;
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(subject);
				return(TRUE);
			}
		}

		if ( usNewState == JUMPUPWALL )
		{
			if ( subject.animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				(void)TacticalActorOrientation::setDesiredDirection(subject, subject.animationIntent().pendingDirection() );
				subject.animationIntent().clearPendingDirection();
				subject.animationIntent().pendingAnimation() = JUMPUPWALL;
				subject.animationActivity().turningUntilDone() = TRUE;
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(subject);
				return(TRUE);
			}
		}

		if ( usNewState == JUMPDOWNWALL )
		{
			if ( subject.animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				(void)TacticalActorOrientation::setDesiredDirection(subject, subject.animationIntent().pendingDirection() );
				subject.animationIntent().clearPendingDirection();
				subject.animationIntent().pendingAnimation() = JUMPDOWNWALL;
				subject.animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
				subject.animationActivity().turningUntilDone() = TRUE;
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(subject);
				return(TRUE);
			}
		}

		if ( usNewState == START_AID_PRN )
		{
			if ( subject.animationIntent().pendingDirection() != NO_PENDING_DIRECTION )
			{
				(void)TacticalActorOrientation::setDesiredDirection(subject, subject.animationIntent().pendingDirection() );
				subject.animationIntent().clearPendingDirection();
				subject.animationIntent().pendingAnimation() = START_AID_PRN;
				subject.animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_ON;
				subject.animationActivity().turningUntilDone() = TRUE;
				subject.animationIntent().pendingStance() = ANIM_PRONE;
				(void)TacticalActorRouteExecution::settleIntoStationaryStance(subject);
				return(TRUE);
			}
		}

		// ATE: Don't raise/lower automatically if we are low on health,
		// as our gun looks lowered anyway....
		//if ( subject.vitals().health() > INJURED_CHANGE_THREASHOLD )
		if ( !TacticalActorMobility::inWater(subject) )
		{
			// Don't do some of &subject if we are a monster!
			// ATE: LOWER AIMATION IS GOOD, RAISE ONE HOWEVER MAY CAUSE PROBLEMS FOR AI....
			if ( !(subject.status().flags() & SOLDIER_MONSTER) && subject.identity().bodyType() != ROBOTNOWEAPON && subject.roster().team() == gbPlayerNum )
			{
				// If &subject animation is a raise_weapon animation
				if ( (gAnimControl[usNewState].uiFlags & ANIM_RAISE_WEAPON) && !(gAnimControl[subject.animationPlayback().state()].uiFlags & (ANIM_RAISE_WEAPON | ANIM_NOCHANGE_WEAPON)) )
				{
					// We are told that we need to rasie weapon
					// Do so only if
					// 1) We have a rifle in hand...
					//usItem = subject.inventory()[ HANDPOS ].usItem;

					if ( subject.inventory()[HANDPOS].exists( ) == true && ItemIsTwoHanded(usItem) && !ItemIsRocketLauncher(usItem) )
					{
						// Switch on height!
						switch ( gAnimControl[subject.animationPlayback().state()].ubEndHeight )
						{
						case ANIM_STAND:

							// 2) OK, all's fine... lower weapon first....
							subject.animationIntent().pendingAnimation() = usNewState;
							// Set new state to be animation to move to new stance
							usNewState = RAISE_RIFLE;
						}
					}
				}

				// If &subject animation is a lower_weapon animation
				if ( (gAnimControl[usNewState].uiFlags & ANIM_LOWER_WEAPON) && !(gAnimControl[subject.animationPlayback().state()].uiFlags & (ANIM_LOWER_WEAPON | ANIM_NOCHANGE_WEAPON)) )
				{
					// We are told that we need to rasie weapon
					// Do so only if
					// 1) We have a rifle in hand...
					//usItem = subject.inventory()[ HANDPOS ].usItem;

					if ( subject.inventory()[HANDPOS].exists( ) == true && ItemIsTwoHanded(usItem) && !ItemIsRocketLauncher(usItem) )
					{
						// Switch on height!
						switch ( gAnimControl[subject.animationPlayback().state()].ubEndHeight )
						{
						case ANIM_STAND:

							// 2) OK, all's fine... lower weapon first....
							subject.animationIntent().pendingAnimation() = usNewState;
							// Set new state to be animation to move to new stance
							usNewState = LOWER_RIFLE;
						}
					}
				}
			}
		}

		// Are we cowering and are trying to move, getup first...
		//if ( gAnimControl[ usNewState ].uiFlags & ANIM_MOVING && subject.animationPlayback().state() == COWERING && gAnimControl[ usNewState ].ubEndHeight == ANIM_STAND )
		if ( subject.animationPlayback().state() == COWERING )
		{
			if ( gAnimControl[usNewState].ubEndHeight == ANIM_STAND )
			{
				subject.animationIntent().pendingAnimation() = usNewState;
				subject.animationIntent().desiredHeight() = ANIM_STAND;
				usNewState = END_COWER;
			}
			else if ( gAnimControl[usNewState].ubEndHeight == ANIM_CROUCH )
			{
				subject.animationIntent().pendingAnimation() = usNewState;
				subject.animationIntent().desiredHeight() = ANIM_CROUCH;
				usNewState = END_COWER_CROUCHED;
			}
		}
		else if ( subject.animationPlayback().state() == COWERING_PRONE )
		{
			if ( gAnimControl[usNewState].ubEndHeight == ANIM_PRONE )
			{
				subject.animationIntent().pendingAnimation() = usNewState;
				subject.animationIntent().desiredHeight() = ANIM_PRONE;
				usNewState = END_COWER_PRONE;
			}
		}

		// If we want to start swatting, put a pending animation
		if ( subject.animationPlayback().state() != START_SWAT && usNewState == SWATTING )
		{
			// Set new state to be animation to move to new stance
			usNewState = START_SWAT;
		}

		if ( subject.animationPlayback().state() == SWATTING && usNewState == CROUCHING )
		{
			// Set new state to be animation to move to new stance
			usNewState = END_SWAT;
		}
		///***ddd{
		if ( (subject.animationPlayback().state() == SWATTING_WK || subject.animationPlayback().state() == SWAT_BACKWARDS
			|| subject.animationPlayback().state() == SWAT_BACKWARDS_NOTHING || subject.animationPlayback().state() == SWAT_BACKWARDS_WK)
			&& usNewState == CROUCHING )
		{
			// Set new state to be animation to move to new stance
			usNewState = END_SWAT;
		}
		///***ddd}

		if ( subject.animationPlayback().state() == WALKING && usNewState == STANDING && subject.vitals().health() < INJURED_CHANGE_THREASHOLD && subject.identity().bodyType() <= REGFEMALE && !TacticalActorMobility::inWater(subject) )
		{
			// Set new state to be animation to move to new stance
			usNewState = END_HURT_WALKING;
		}

		// Check if we are an enemy, and we are in an animation what should be overriden
		// by if he sees us or not.
		if ( ReevaluateEnemyStance( &subject, usNewState ) )
		{
			return(TRUE);
		}

		// OK.......
		// SANDRO - removing unused code
		/*if ( subject.ubBodyType > REGFEMALE )
		{
		if ( subject.vitals().health() < INJURED_CHANGE_THREASHOLD )
		{
		if ( usNewState == READY_RIFLE_STAND )
		{
		//	subject.animationIntent().secondaryPendingAnimation() = usNewState;
		//	usNewState = FROM_INJURED_TRANSITION;
		}
		}
		}*/

		// Alrighty, check if we should free buddy up!
		if ( usNewState == GIVING_AID || usNewState == GIVING_AID_PRN )
		{
			UnSetUIBusy( subject.identity().id() );
		}


		// SUBSTITUDE VARIOUS REG ANIMATIONS WITH ODD BODY TYPES
		if ( SubstituteBodyTypeAnimation( &subject, usNewState, &usSubState ) )
		{
			usNewState = usSubState;
		}

		// CHECK IF WE CAN DO THIS ANIMATION!
		if ( IsAnimationValidForBodyType( &subject, usNewState ) == FALSE )
		{
			return(FALSE);
		}

		// OK, make guy transition if a big merc...
		//if ( ( subject.animationPlayback().subFlags() & SUB_ANIM_BIGGUYTHREATENSTANCE ) && !( subject.animationPlayback().subFlags() & SUB_ANIM_BIGGUYSHOOT2 ) )
		if ( subject.identity().bodyType() == BIGMALE )
		{
			// SANDRO - we are changing crouching animation here to the old vanilla one, don't do that if alt animations are used
			if ( !DecideAltAnimForBigMerc( &subject ) )
			{
				if ( usNewState == KNEEL_DOWN && subject.animationPlayback().state() != BIGMERC_CROUCH_TRANS_INTO )
				{
					//UINT16 usItem;

					// Do we have a rifle?
					//usItem = subject.inventory()[ HANDPOS ].usItem;

					if ( subject.inventory()[HANDPOS].exists( ) == true )
					{
						if ( Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
						{
							//						if ( (Item[ usItem ].fFlags & ITEM_TWO_HANDED) )
							if (ItemIsTwoHanded(usItem))
							{
								usNewState = BIGMERC_CROUCH_TRANS_INTO;
							}
						}
					}
				}

				if ( usNewState == KNEEL_UP && subject.animationPlayback().state() != BIGMERC_CROUCH_TRANS_OUTOF )
				{
					//UINT16 usItem;

					// Do we have a rifle?
					//usItem = subject.inventory()[ HANDPOS ].usItem;

					if ( subject.inventory()[HANDPOS].exists( ) == true )
					{
						if ( Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
						{
							//						if ( (Item[ usItem ].fFlags & ITEM_TWO_HANDED) )
							if (ItemIsTwoHanded(usItem))
							{
								usNewState = BIGMERC_CROUCH_TRANS_OUTOF;
							}
						}
					}
				}
			}
		}

		// OK, if we have reverse set, do the side step!
		if ( subject.movement().reverse() )
		{
			if ( usNewState == WALKING || usNewState == RUNNING || usNewState == SWATTING
				 //*** ddd
				 || usNewState == SWATTING_WK || usNewState == START_SWAT )
			{
				// CHECK FOR SIDEWAYS!
				if ( !(subject.status().flags() & SOLDIER_VEHICLE) && subject.position().direction() == gPurpendicularDirection[subject.position().direction()][subject.pathing().path()[subject.pathing().pathIndex()]] )
				{
					// We are perpendicular!
					// SANDRO - wait wait wait!!! We need to determine if gonna sidestep with weapon raised
					if ( ((gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_FIREREADY) ||
						(gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_FIRE)) && gGameExternalOptions.fAllowWalkingWithWeaponRaised )
					{
						if ( subject.inventory()[HANDPOS].exists( ) == true && Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
						{
							if ( gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_STAND )
							{
								if ( TacticalActorWeaponHandling::isValidSecondHandShot(subject) )
								{
									usNewState = SIDE_STEP_DUAL_RDY;
								}
								else if ( gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_ALT_WEAPON_HOLDING )
								{
									usNewState = SIDE_STEP_ALTERNATIVE_RDY;
								}
								else
								{
									usNewState = SIDE_STEP_WEAPON_RDY;
								}
							}
						}
					}
					else
					{
						if ( gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_STAND )
						{
							usNewState = SIDE_STEP;
						}
						// comment in once animations are ready
						/*else if ( gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_CROUCH )
						{
							if ( TacticalActorWeaponHandling::isValidSecondHandShot(subject) )
							{
								usNewState = SIDE_STEP_CROUCH_DUAL;
							}
							else if ( !Item[subject.inventory()[HANDPOS].usItem].twohanded )
							{
								usNewState = SIDE_STEP_CROUCH_PISTOL;
							}
							else
							{
								usNewState = SIDE_STEP_CROUCH_RIFLE;
							}
						}*/
					}
				}
				else
				{
					if ( gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_CROUCH )
					{
						if ( subject.inventory()[HANDPOS].exists( ) == true && Item[usItem].usItemClass == IC_GUN && ItemIsTwoHanded(usItem) && !ItemIsRocketLauncher(usItem) )
							usNewState = SWAT_BACKWARDS;
						else
							usNewState = SWAT_BACKWARDS_NOTHING;
						// move backward crouching, with a knife
						if ( subject.inventory()[HANDPOS].exists( ) == true &&
							 //(subject.ubBodyType == BIGMALE || subject.ubBodyType == REGFEMALE )&&
							 (Item[usItem].usItemClass == IC_BLADE || Item[usItem].usItemClass == IC_THROWING_KNIFE) )
							 usNewState = SWAT_BACKWARDS_WK;

					}
					else
					{
						// Here, change to  opposite direction
						usNewState = WALK_BACKWARDS;
					}
				}
			}
			//***08.12.2008*** added roll animation ;) ddd
			else if ( usNewState == CRAWLING
					  && subject.position().direction() ==
					  gPurpendicularDirection[subject.position().direction()][subject.pathing().path()
					  [subject.pathing().pathIndex()]] )
			{
				if ( QuickestDirection( subject.position().direction(), subject.pathing().path()[subject.pathing().pathIndex()] ) > 0 )
					usNewState = ROLL_PRONE_R;
				else if ( QuickestDirection( subject.position().direction(), subject.pathing().path()[subject.pathing().pathIndex()] ) < 0 )
					usNewState = ROLL_PRONE_L;

				if ( usNewState != CRAWLING )
				{
					if ( subject.position().direction() % 2 == 0 )
						gAnimControl[usNewState].dMovementChange = (FLOAT)0.8;
					else
						gAnimControl[usNewState].dMovementChange = (FLOAT)1.1;
				}
			}///

		}

		// ATE: Patch hole for breath collapse for roofs, fences
		if ( usNewState == JUMPUPWALL || usNewState == JUMPDOWNWALL || usNewState == CLIMBUPROOF || usNewState == CLIMBDOWNROOF || usNewState == HOPFENCE || usNewState == JUMPWINDOWS )
		{
			// Check for breath collapse if a given animation like
			if (TacticalActorRecovery::checkBreathCollapse(subject) ||
				subject.collapseState().tactical())
			{
				// UNset UI
				UnSetUIBusy( subject.identity().id() );

				(void)TacticalActorRecovery::collapse(subject);

				subject.collapseState().clearBreathCollapse();

				return(FALSE);

			}
		}

		// If we are in water.....and trying to run, change to run
		if ( TacticalActorMobility::inWater(subject) )
		{
			// Check animation
			// Change to walking
			if ( usNewState == RUNNING )
			{
				usNewState = WALKING;
			}
		}
		// SANDRO - check if we are gonna move with weapon raised
		else if ( gGameExternalOptions.fAllowWalkingWithWeaponRaised && ( (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_FIREREADY) || (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_FIRE) ) )
		{
			if ( subject.inventory()[HANDPOS].exists( ) == true && Item[usItem].usItemClass == IC_GUN && !ItemIsRocketLauncher(usItem) )
			{
				if ( usNewState == WALKING )
				{
					if (TacticalActorWeaponHandling::isValidSecondHandShot(subject))
					{
						usNewState = WALKING_DUAL_RDY;
					}
					else if (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_ALT_WEAPON_HOLDING)
					{
						usNewState = WALKING_ALTERNATIVE_RDY;
					}
					else
					{
						usNewState = WALKING_WEAPON_RDY;
					}
				}
				else if (usNewState == SWATTING || usNewState == START_SWAT )
				{
					if ( TacticalActorWeaponHandling::isValidSecondHandShot(subject) )
					{
						usNewState = CROUCHEDMOVE_DUAL_READY;
					}
					else if (!ItemIsTwoHanded(subject.inventory()[HANDPOS].usItem))
					{
						usNewState = CROUCHEDMOVE_PISTOL_READY;
					}
					else
					{
						usNewState = CROUCHEDMOVE_RIFLE_READY;
					}
				}
			}
		}

		// Turn off anipause flag for any anim!
		subject.status().flags() &= (~SOLDIER_PAUSEANIMOVE);

		// Unset paused for no APs.....
		(void)TacticalActorRouteExecution::setOutOfActionPoints(subject, false );


		// We are about to start moving
		// Handle buddy beginning to move...
		// check new gridno, etc
		// ATE: Added: Make check that old anim is not a moving one as well
		if ( gAnimControl[usNewState].uiFlags & ANIM_MOVING && !(gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_MOVING) || (gAnimControl[usNewState].uiFlags & ANIM_MOVING && fForce) )
		{
			BOOLEAN fKeepMoving;

			if ( usNewState == CRAWLING && subject.movement().gridUpdatePolicy() == LOCKED_NO_NEWGRIDNO )
			{
				// Turn off lock once we are crawling once...
				subject.movement().requestGridUpdateSuppression();
			}

			// ATE: Additional check here if we have just been told to update animation ONLY, not goto gridno stuff...
			if ( !subject.movement().gridUpdatePolicy() )
			{
				if ( usNewState != SWATTING )
				{
					DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Handling New gridNo for %d: Old %s, New %s", subject.identity().id(), gAnimControl[subject.animationPlayback().state()].zAnimStr, gAnimControl[usNewState].zAnimStr ) );

					if ( !(gAnimControl[usNewState].uiFlags & ANIM_SPECIALMOVE) )
					{
						// Handle goto new tile...
						if ( HandleGotoNewGridNo( &subject, &fKeepMoving, TRUE, usNewState ) )
						{
							if ( !fKeepMoving )
							{
								return(FALSE);
							}

							// Make sure desy = zeroed out...
							// subject.movement().clearPastDestination();
						}
						else
						{
							if ( subject.collapseState().breathTriggered() )
							{
								// UNset UI
								UnSetUIBusy( subject.identity().id() );

								(void)TacticalActorRecovery::collapse(subject);

								subject.collapseState().clearBreathCollapse();
							}
							return(FALSE);
						}
					}
					else
					{
						// Change desired direction
						// Just change direction
						(void)TacticalActorOrientation::setMovementDestination(subject, (UINT8) subject.pathing().path()[subject.pathing().pathIndex()], FALSE, subject.animationPlayback().state() );
					}

					//check for services
					TacticalActorMedicalServices::
						cancelReceiving(subject);
					TacticalActorMedicalServices::
						cancelProviding(subject);


					// Check if we are a vehicle, and start playing noise sound....
					if ( subject.status().flags() & SOLDIER_VEHICLE )
					{
						HandleVehicleMovementSound( &subject, TRUE );
					}
				}
			}
		}
		else
		{
			// Check for stopping movement noise...
			if ( subject.status().flags() & SOLDIER_VEHICLE )
			{
				HandleVehicleMovementSound( &subject, FALSE );

				// If a vehicle, set hewight to 0
				(void)TacticalActorWorldPlacement::setHeight(subject, (FLOAT)(0) );
			}

		}

		// Reset to false always.....
		// ( Unless locked )
		if ( gAnimControl[usNewState].uiFlags & ANIM_MOVING )
		{
			// 0verhaul:  **** Special hack!!!!
			//   If a merc begins to go prone while moving, the LOCKED_NO_NEWGRIDNO is set.  If the merc never finishes
			// going prone, either due to interrupting the stance change with a different stance change, or other possible
			// factors such as maybe getting shot (&subject is realtime so an enemy could see him), it stays on locked.  Once
			// it stays on locked, the soldier will be unable to navigate around obstacles but will simply stay put
			// twitching.  Since the LOCKED is only set when going prone, &subject unsets it.
			if ( subject.movement().gridUpdatePolicy() != LOCKED_NO_NEWGRIDNO ||
				 (subject.movement().gridUpdatePolicy() == LOCKED_NO_NEWGRIDNO && subject.animationPlayback().state() != PRONE_DOWN) )
			{
				subject.movement().clearGridUpdatePolicy();
			}
		}

		if ( fTryingToRestart )
		{
			return(FALSE);
		}

	}


	// ATE: If &subject is an AI guy.. unlock him!
	if ( gTacticalStatus.fEnemySightingOnTheirTurn )
	{
		if ( gTacticalStatus.ubEnemySightingOnTheirTurnEnemyID == subject.identity().id() )
		{
			subject.animationActivity().resume();
			gTacticalStatus.fEnemySightingOnTheirTurn = FALSE;
		}
	}

	///////////////////////////////////////////////////////////////////////
	//			HERE DOWN - WE HAVE MADE A DESCISION!
	/////////////////////////////////////////////////////////////////////

	uiOldAnimFlags = gAnimControl[subject.animationPlayback().state()].uiFlags;
	uiNewAnimFlags = gAnimControl[usNewState].uiFlags;

	usNewGridNo = NewGridNo( subject.position().gridNo(), DirectionInc( (UINT8) subject.pathing().path()[subject.pathing().pathIndex()] ) );


	// CHECKING IF WE HAVE A HIT FINISH BUT NO DEATH IS DONE WITH A SPECIAL ANI CODE
	// IN THE HIT FINSIH ANI SCRIPTS

	// CHECKING IF WE HAVE FINISHED A DEATH ANIMATION IS DONE WITH A SPECIAL ANI CODE
	// IN THE DEATH SCRIPTS


	// CHECK IF THIS NEW STATE IS NON-INTERRUPTABLE
	// IF SO - SET NON-INT FLAG
	// 0verhaul:  Okay, here is a question:  Is the "non-interrupt" supposed to be transferrable to other anims?
	// That is, if one anim is not interruptable but it chains to another anim, should the "not interruptable" flag
	// remain?  I'm going to try out the theory that new animations should reset the "don't interrupt" flag.
	subject.animationActivity().setInterruptibility(
		(uiNewAnimFlags & ANIM_NONINTERRUPT) != 0,
		(uiNewAnimFlags & ANIM_RT_NONINTERRUPT) != 0);

	// CHECK IF WE ARE NOT AIMING, IF NOT, RESET LAST TAGRET!
	if ( !(gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_FIREREADY) && !(gAnimControl[usNewState].uiFlags & ANIM_FIREREADY) )
	{
		// ATE: Also check for the transition anims to not reset &subject
		// &subject should have used a flag but we're out of them....
		if ( usNewState != READY_ALTERNATIVE_STAND && usNewState != READY_RIFLE_STAND && usNewState != READY_RIFLE_PRONE && usNewState != READY_RIFLE_CROUCH && usNewState != ROBOT_SHOOT && usNewState != TANK_SHOOT && usNewState != TANK_BURST && usNewState != THROW_KNIFE && usNewState != THROW_KNIFE_SP_BM && subject.animationPlayback().state() != THROW_KNIFE && subject.animationPlayback().state() != THROW_KNIFE_SP_BM )//dnl ch64 300813 //dnl ch70 170913
		{
			subject.targeting().lastGridNo() = NOWHERE;
		}
	}

	// If a special move state, release np aps
	if ( (gAnimControl[usNewState].uiFlags & ANIM_SPECIALMOVE) )
	{
		(void)TacticalActorRouteExecution::setOutOfActionPoints(subject, false );
	}

	if ( gAnimControl[usNewState].uiFlags & ANIM_UPDATEMOVEMENTMODE )
	{
		if ( subject.roster().team() == gbPlayerNum )
		{
			// Movement mode is selected separately from the animation transition.
		}
	}

	// ATE: If not a moving animation - turn off reverse....
	if ( !(gAnimControl[usNewState].uiFlags & ANIM_MOVING) )
	{
		subject.movement().setReverse(false);
	}

	// ONLY DO FOR EVERYONE BUT PLANNING GUYS
	if ( subject.identity().id() < MAX_NUM_SOLDIERS )
	{
		// Do special things based on new state
		// CHRISL: Make changes so that we charge extra APs while wearing a backpack while using new inventory system
		switch ( usNewState )
		{
		case STANDING:

			// Update desired height
			subject.animationIntent().desiredHeight() = ANIM_STAND;
			break;

		case CROUCHING:

			// Update desired height
			subject.animationIntent().desiredHeight() = ANIM_CROUCH;
			break;

		case PRONE:

			// Update desired height
			subject.animationIntent().desiredHeight() = ANIM_PRONE;
			break;

		case READY_RIFLE_STAND:
		case READY_RIFLE_PRONE:
		case READY_RIFLE_CROUCH:
		case READY_DUAL_STAND:
		case READY_DUAL_CROUCH:
		case READY_DUAL_PRONE:
		case READY_ALTERNATIVE_STAND:

			// OK, get points to ready weapon....
			if ( !subject.animationActivity().readyCostWaived() )
			{
				sAPCost = GetAPsToReadyWeapon( &subject, usNewState );
				// SANDRO - get BP cost for weapon manipulating
				if ( gGameExternalOptions.ubEnergyCostForWeaponWeight )
					sBPCost = sAPCost * GetBPCostPer10APsForGunHolding( &subject ) / 10;
				else
					sBPCost = 0;
				DeductPoints( &subject, sAPCost, sBPCost, BEFORESHOT_INTERRUPT );
			}
			else
			{
				subject.animationActivity().readyCostWaived() = FALSE;
			}
			break;

		case WALKING:
		case WALKING_WEAPON_RDY:
		case WALKING_DUAL_RDY:
		case WALKING_ALTERNATIVE_RDY:

			subject.animationIntent().clearPendingAnimation();
			subject.pendingAction().resetAnimationCount();
			break;

		case SWATTING:
		case CROUCHEDMOVE_RIFLE_READY:
		case CROUCHEDMOVE_PISTOL_READY:
		case CROUCHEDMOVE_DUAL_READY:

			subject.animationIntent().clearPendingAnimation();
			subject.pendingAction().resetAnimationCount();
			break;

		case CRAWLING:

			// Turn off flag...
			subject.animationActivity().turningFromProneMode() = TURNING_FROM_PRONE_OFF;
			subject.pendingAction().resetAnimationCount();
			subject.animationIntent().clearPendingAnimation();
			break;

		case RUNNING:

			// Only if our previous is not running
			/* //shadooow: moved to ActionPointCost
			if ( subject.animationPlayback().state() != RUNNING )
			{
				// CHRISL
				if ( (UsingNewInventorySystem( ) == true) && FindBackpackOnSoldier( &subject ) != ITEM_NOT_FOUND )
				{
					sAPCost = GetAPsStartRun( &subject ) + 2; // changed by SANDRO
					sBPCost += 2;
				}
				else
					sAPCost = GetAPsStartRun( &subject ); // changed by SANDRO
				DeductPoints( &subject, sAPCost, sBPCost, MOVEMENT_INTERRUPT );
			}
			*/
			// Set pending action count to 0
			subject.pendingAction().resetAnimationCount();
			subject.animationIntent().clearPendingAnimation();
			break;

		case ADULTMONSTER_WALKING:
			subject.pendingAction().resetAnimationCount();
			break;

		case ROBOT_WALK:
			subject.pendingAction().resetAnimationCount();
			break;

		case KNEEL_UP:
		case KNEEL_DOWN:
		case BIGMERC_CROUCH_TRANS_INTO:
		case BIGMERC_CROUCH_TRANS_OUTOF:

			if ( !subject.animationActivity().stanceCostWaived() )
			{
				if ( UsingNewInventorySystem( ) )
				{
					if ( usNewState == KNEEL_UP || usNewState == BIGMERC_CROUCH_TRANS_OUTOF )
					{
						sAPCost = GetAPsCrouch( &subject, TRUE * 2 );
						sBPCost = APBPConstants[BP_CROUCH] + 2;
					}
					else
					{
						sAPCost = GetAPsCrouch( &subject, TRUE );
						sBPCost = APBPConstants[BP_CROUCH] + 1;
					}
				}
				else
				{
					sAPCost = GetAPsCrouch( &subject, FALSE );
					sBPCost = APBPConstants[BP_CROUCH];
				}
				DeductPoints( &subject, sAPCost, sBPCost );
			}
			subject.animationActivity().stanceCostWaived() = FALSE;
			break;

		case PRONE_UP:
		case PRONE_DOWN:

			// ATE: If we are NOT waiting for prone down...
			if ( subject.animationActivity().turningFromProneMode() < TURNING_FROM_PRONE_START_UP_FROM_MOVE && !subject.animationActivity().stanceCostWaived() )
			{
				if ( UsingNewInventorySystem( ) )
				{
					if ( usNewState == PRONE_UP )
					{
						sAPCost = GetAPsProne( &subject, TRUE * 2 );
						sBPCost = APBPConstants[BP_PRONE] + 2;
					}
					else
					{
						sAPCost = GetAPsProne( &subject, TRUE );
						sBPCost = APBPConstants[BP_PRONE] + 1;
					}
				}
				else
				{
					sAPCost = GetAPsProne( &subject, FALSE );
					sBPCost = APBPConstants[BP_PRONE];
				}
				DeductPoints( &subject, sAPCost, sBPCost );
			}
			subject.animationActivity().stanceCostWaived() = FALSE;
			break;

			//Deduct points for stance change
			//sAPCost = GetAPsToChangeStance( &subject, gAnimControl[ usNewState ].ubEndHeight );
			//DeductPoints( &subject, sAPCost, 0 );
			//break;

		case START_AID:
		case START_AID_PRN:

			DeductPoints( &subject, APBPConstants[AP_START_FIRST_AID], APBPConstants[BP_START_FIRST_AID] );
			break;

		case CUTTING_FENCE:
			DeductPoints( &subject, APBPConstants[AP_USEWIRECUTTERS], APBPConstants[BP_USEWIRECUTTERS], AFTERACTION_INTERRUPT );
			break;

		case PLANT_BOMB:

			if (ItemIsMine(subject.inventory()[HANDPOS].usItem))	// bury a mine
				DeductPoints( &subject, GetAPsToPlantMine( &subject ), APBPConstants[BP_BURY_MINE] ); // changed by SANDRO
			else
				DeductPoints( &subject, GetAPsToDropBomb( &subject ), APBPConstants[BP_DROP_BOMB] ); // changed by SANDRO
			break;

		case STEAL_ITEM:

			// We will deduct APs for this actor elsewhere (see weapons.cpp) - SANDRO
			//DeductPoints( &subject, APBPConstants[AP_STEAL_ITEM], 0 );
			break;

		case CROW_DIE:

			// Delete shadow of crow....
			if ( subject.renderBindings().animationTile() != NULL )
			{
				DeleteAniTile( subject.renderBindings().animationTile() );
				subject.renderBindings().animationTile() = NULL;
			}
			break;

		case CROW_FLY:

			// Ate: startup a shadow ( if gridno is set )
			HandleCrowShadowNewGridNo( &subject );
			break;

		case CROW_EAT:

			// ATE: Make sure height level is 0....
			(void)TacticalActorWorldPlacement::setHeight(subject, (FLOAT)(0) );
			HandleCrowShadowRemoveGridNo( &subject );
			break;

		case USE_REMOTE:

			DeductPoints( &subject, APBPConstants[AP_USE_REMOTE], 0, AFTERACTION_INTERRUPT );
			break;

		case REFUEL_VEHICLE:

			DeductPoints(&subject, APBPConstants[AP_REFUEL_VEHICLE], 0, AFTERACTION_INTERRUPT);
			break;

		case GOTO_REPAIRMAN:

			DeductPoints(&subject, APBPConstants[AP_START_REPAIR], 0, AFTERACTION_INTERRUPT);
			break;

		case TAKE_BLOOD_FROM_CORPSE:

			DeductPoints(&subject, APBPConstants[AP_TAKE_BLOOD], 0, AFTERACTION_INTERRUPT);
			break;

			//case PUNCH:

			//Deduct points for punching
			//sAPCost = MinAPsToAttack( &subject, subject.sGridNo, FALSE );
			//DeductPoints( &subject, sAPCost, 0 );
			//break;

		case HOPFENCE:

			// CHRISL
			// SANDRO - changed &subject a bit
			if ( (UsingNewInventorySystem( ) == true) && FindBackpackOnSoldier( &subject ) != ITEM_NOT_FOUND )
				DeductPoints( &subject, GetAPsToJumpFence( &subject, TRUE ), GetBPsToJumpFence( &subject, TRUE ), SP_MOVEMENT_INTERRUPT );
			else
				DeductPoints( &subject, GetAPsToJumpFence( &subject, FALSE ), GetBPsToJumpFence( &subject, FALSE ), SP_MOVEMENT_INTERRUPT );
			break;

		case JUMPWINDOWS:
			if ( (UsingNewInventorySystem( ) == true) && FindBackpackOnSoldier( &subject ) != ITEM_NOT_FOUND )
				DeductPoints( &subject, GetAPsToJumpThroughWindows( &subject, TRUE ), GetBPsToJumpThroughWindows( &subject, TRUE ), SP_MOVEMENT_INTERRUPT );
			else
				DeductPoints( &subject, GetAPsToJumpThroughWindows( &subject, FALSE ), GetBPsToJumpThroughWindows( &subject, FALSE ), SP_MOVEMENT_INTERRUPT );
			break;

			// Deduct aps for falling down....
		case FALLBACK_HIT_STAND:
		case FALLFORWARD_FROMHIT_STAND:

			DeductPoints( &subject, APBPConstants[AP_FALL_DOWN], APBPConstants[BP_FALL_DOWN], DISABLED_INTERRUPT );
			break;

		case FALLFORWARD_FROMHIT_CROUCH:

			DeductPoints( &subject, (APBPConstants[AP_FALL_DOWN] / 2), (APBPConstants[BP_FALL_DOWN] / 2), DISABLED_INTERRUPT );
			break;

		case QUEEN_SWIPE:

			// ATE: set damage counter...
			subject.pendingAction().primaryData() = 0;
			break;

		case CLIMBDOWNROOF:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( &subject, GetAPsToClimbRoof( &subject, TRUE ), GetBPsToClimbRoof( &subject, TRUE ), SP_MOVEMENT_INTERRUPT ); // changed by SANDRO
			break;

		case CLIMBUPROOF:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( &subject, GetAPsToClimbRoof( &subject, FALSE ), GetBPsToClimbRoof( &subject, FALSE ), SP_MOVEMENT_INTERRUPT ); // changed by SANDRO
			break;

		case JUMPDOWNWALL:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( &subject, GetAPsToJumpWall( &subject, TRUE ), GetBPsToJumpWall( &subject, TRUE ), SP_MOVEMENT_INTERRUPT );
			break;

		case JUMPUPWALL:

			// disable sight
			gTacticalStatus.uiFlags |= DISALLOW_SIGHT;

			DeductPoints( &subject, GetAPsToJumpWall( &subject, FALSE ), GetBPsToJumpWall( &subject, FALSE ), SP_MOVEMENT_INTERRUPT );
			break;

		case JUMP_OVER_BLOCKING_PERSON:
			// Set path....
		{
										  INT32 usNewGridNo;

										  DeductPoints( &subject, GetAPsToJumpOver( &subject ), APBPConstants[BP_JUMP_OVER], SP_MOVEMENT_INTERRUPT );

										  usNewGridNo = NewGridNo( subject.position().gridNo(), DirectionInc( subject.position().direction() ) );
										  usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( subject.position().direction() ) );

										  subject.runtime().pendingAction.pathSearchSourceGrid = subject.position().gridNo();
										  subject.movement().clearPastDestination();
										  subject.pathing().pathSize() = 0;
										  subject.pathing().pathIndex() = 0;
										  subject.pathing().path()[subject.pathing().pathSize()] = subject.position().direction();
										  subject.pathing().pathSize()++;
										  subject.pathing().path()[subject.pathing().pathSize()] = subject.position().direction();
										  subject.pathing().pathSize()++;
										  subject.pathing().finalDestinationGrid() = usNewGridNo;
										  // Set direction
										  (void)TacticalActorOrientation::setMovementDestination(subject, (UINT8) subject.pathing().path()[subject.pathing().pathIndex()], FALSE, JUMP_OVER_BLOCKING_PERSON );
		}
			break;

		case LONG_JUMP:
			// Set path....
		{
						  INT32 usNewGridNo;

						  DeductPoints( &subject, GetAPsToJumpOver( &subject ), APBPConstants[BP_JUMP_OVER], SP_MOVEMENT_INTERRUPT );

						  usNewGridNo = NewGridNo( subject.position().gridNo(), DirectionInc( subject.position().direction() ) );
						  usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( subject.position().direction() ) );
						  usNewGridNo = NewGridNo( usNewGridNo, DirectionInc( subject.position().direction() ) );

						  subject.runtime().pendingAction.pathSearchSourceGrid = subject.position().gridNo();
						  subject.movement().clearPastDestination();
						  subject.pathing().pathSize() = 0;
						  subject.pathing().pathIndex() = 0;
						  subject.pathing().path()[subject.pathing().pathSize()] = subject.position().direction();
						  subject.pathing().pathSize()++;
						  subject.pathing().path()[subject.pathing().pathSize()] = subject.position().direction();
						  subject.pathing().pathSize()++;
						  subject.pathing().path()[subject.pathing().pathSize()] = subject.position().direction();
						  subject.pathing().pathSize()++;
						  subject.pathing().finalDestinationGrid() = usNewGridNo;
						  // Set direction
						  (void)TacticalActorOrientation::setMovementDestination(subject, (UINT8) subject.pathing().path()[subject.pathing().pathIndex()], FALSE, LONG_JUMP );
		}
			break;


		case GENERIC_HIT_STAND:
		case GENERIC_HIT_CROUCH:
		case STANDING_BURST_HIT:
		case ADULTMONSTER_HIT:
		case ADULTMONSTER_DYING:
		case COW_HIT:
		case COW_DYING:
		case BLOODCAT_HIT:
		case BLOODCAT_DYING:
		case WATER_HIT:
		case WATER_DIE:
		case DEEP_WATER_HIT:
		case DEEP_WATER_DIE:
		case RIFLE_STAND_HIT:
		case LARVAE_HIT:
		case LARVAE_DIE:
		case QUEEN_HIT:
		case QUEEN_DIE:
		case INFANT_HIT:
		case INFANT_DIE:
		case CRIPPLE_HIT:
		case CRIPPLE_DIE:
		case CRIPPLE_DIE_FLYBACK:
		case ROBOTNW_HIT:
		case ROBOTNW_DIE:

			// Set getting hit flag to TRUE
			subject.animationActivity().beginHit();
			break;

		case CHARIOTS_OF_FIRE:
		case BODYEXPLODING:

			// Merc on fire!
			subject.pendingAction().primaryData() = PlaySoldierJA2Sample( subject.identity().id(), (FIRE_ON_MERC), RATE_11025, SoundVolume( HIGHVOLUME, subject.position().gridNo() ), 5, SoundDir( subject.position().gridNo() ), TRUE );
			break;

		case CRYO_DEATH:
			PlayJA2StreamingSampleFromFile( "stsounds\\CryoBlast_1.ogg", RATE_11025, SoundVolume( HIGHVOLUME, subject.position().gridNo() ), 1, MIDDLEPAN, NULL );
			break;

		case CRYO_DEATH_CROUCHED:
			PlayJA2StreamingSampleFromFile( "stsounds\\CryoBlast_2.ogg", RATE_11025, SoundVolume( HIGHVOLUME, subject.position().gridNo() ), 1, MIDDLEPAN, NULL );
			break;
		}
	}

	// Remove old animation profile
	(void)TacticalActorAnimationFootprint::remove(
		subject,
		subject.animationPlayback().state());


	// From animation control, set surface
	if ( SetSoldierAnimationSurface( &subject, usNewState ) == FALSE )
	{
		return(FALSE);
	}


	// Set state
	subject.animationPlayback().previousState() = subject.animationPlayback().state();
	subject.animationPlayback().previousCode() = subject.animationPlayback().code();

	// Change state value!
	subject.animationPlayback().state() = usNewState;
	// Set current frame
	subject.animationPlayback().code() = usStartingAniCode;

	// Handle cleanup stuff for getting hit.  Shouldn't &subject be part of the animation script?
	CheckForFreeupFromHit( &subject, uiOldAnimFlags, uiNewAnimFlags, subject.animationPlayback().previousState(), usNewState );

	// Perform attack busy stuff
	if ( subject.animationPlayback().previousState() != subject.animationPlayback().state() )
	{
		if ( uiNewAnimFlags & ANIM_ATTACK ) {
			BeginJa2TacticalCombatAction();
			DebugAttackBusy( String( "**** Attack animation transfer to %s for %d.\nABC now %d\n", gAnimControl[usNewState].zAnimStr, subject.identity().id(), GetJa2PendingTacticalCombatActions() ) );
		} else if (uiOldAnimFlags & ANIM_ATTACK || subject.animationActivity().suppressionStanceChange() ) {
			DebugAttackBusy( String( "**** Transfer to %s for %d.\n", gAnimControl[usNewState].zAnimStr, subject.identity().id() ) );
		}
		if ( uiOldAnimFlags & ANIM_ATTACK ) {
			DebugAttackBusy( String( "**** Attack animation transfer from %s for %d.  Reducing ABC.\n", gAnimControl[subject.animationPlayback().previousState()].zAnimStr, subject.identity().id() ) );
			ReduceAttackBusyCount( );
		} else if (uiNewAnimFlags & ANIM_ATTACK  || subject.animationActivity().suppressionStanceChange() ) {
			DebugAttackBusy( String( "**** Transfer from %s for %d\n", gAnimControl[subject.animationPlayback().previousState()].zAnimStr, subject.identity().id() ) );
		}
	}

	if ( subject.animationActivity().suppressionStanceChange() )
	{
		subject.animationActivity().suppressionStanceChange() = FALSE;
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "@@@@@@@ Freeing up attacker - end of suppression stance change" ) );
		DebugAttackBusy( String( "@@@@@@@ Freeing up attacker - end of suppression stance change for %d\n", subject.identity().id() ) );
		ReduceAttackBusyCount( );
	}

	subject.animationActivity().clearRenderZOverride();

	if ( !(subject.status().flags() & SOLDIER_LOCKPENDINGACTIONCOUNTER) )
	{
		//ATE Cancel ANY pending action...
		if ( subject.pendingAction().animationCount() > 0 && (gAnimControl[subject.animationPlayback().previousState()].uiFlags & ANIM_MOVING) )
		{
			// Do some special things for some actions
			switch ( subject.pendingAction().action() )
			{
			case MERC_GIVEITEM:

				// Unset target as enaged
				if ( TacticalActor* target =
						 GetJa2SoldierRepository().resolve(
							 subject.pendingAction().quaternaryData() ) )
					target->status().flags() &=
						(~SOLDIER_ENGAGEDINACTION);
				break;
			}
			subject.pendingAction().clearAction();
		}
		else
		{
			// Increment &subject for almost all animations except some movement ones...
			// That's because &subject represents ANY animation other than the one we began when the pending action was started
			// ATE: Added to ignore &subject count if we are waiting for someone to move out of our way...
			if ( usNewState != START_SWAT && usNewState != END_SWAT && !(gAnimControl[usNewState].uiFlags & ANIM_NOCHANGE_PENDINGCOUNT) && !subject.movement().delayed() && !(subject.status().flags() & SOLDIER_ENGAGEDINACTION) )
			{
				subject.pendingAction().recordAnimationTransition();
			}
		}
	}

	// Set new animation profile
	// SetSoldierAnimationSurface (above) already computed and stashed the surface for usNewState in
	// subject.animationPlayback().surface(); reuse it instead of recomputing DetermineSoldierAnimationSurface here.
	(void)TacticalActorAnimationFootprint::addForSurface(
		subject,
		usNewState,
		subject.animationPlayback().surface());

	// Reset some animation values
	subject.renderState().disableForceShade();

	// ATE; For some animations that could use some variations, do so....
	if ( usNewState == CHARIOTS_OF_FIRE || usNewState == BODYEXPLODING )
	{
		subject.animationPlayback().code() = (UINT16)(Random( 10 ));
	}

	// ATE: Default to first frame....
	// Will get changed ( probably ) by AdjustToNextAnimationFrame()
	(void)TacticalActorAnimationFrames::selectFrame(subject, 0);

	// Set delay speed
	SetSoldierAniSpeed( &subject );

	// Reset counters
	subject.timing().start(SoldierTimingComponent::Timer::AnimationUpdate, subject.animationPlayback().delay());

	// Adjust to new animation frame ( the first one )
	AdjustToNextAnimationFrame( &subject );

	// Setup offset information for UI above guy
	SetSoldierLocatorOffsets( &subject );

	// Lesh: test fix visibility after raising gun
	if ( (gAnimControl[subject.animationPlayback().previousState()].uiFlags & ANIM_RAISE_WEAPON) && (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_FIREREADY) )
		//equivalent if ( (subject.animationPlayback().state() == AIM_RIFLE_PRONE) || (subject.animationPlayback().state() == AIM_RIFLE_CROUCH) || (subject.animationPlayback().state() == AIM_RIFLE_STAND) )
	{
		if ( (subject.animationPlayback().previousState() == READY_RIFLE_STAND) || (subject.animationPlayback().previousState() == READY_RIFLE_CROUCH) || (subject.animationPlayback().previousState() == READY_RIFLE_PRONE) ||
			 (subject.animationPlayback().previousState() == READY_DUAL_STAND) || (subject.animationPlayback().previousState() == READY_DUAL_CROUCH) || (subject.animationPlayback().previousState() == READY_DUAL_PRONE) )
		{
			HandleSight( &subject, SIGHT_LOOK );
		}
	}

	// Flugente: if we are covert and perform a suspicious action, we will be easier to uncover for a short time
	if ( subject.featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER) )
	{
		// if e perform a suspicious action, we are easier to identify
		UINT16 appenalty = GetSuspiciousAnimationAPDuration( subject.animationPlayback().state() );

		if ( appenalty )
		{
			// mark us a easily identifiable
			subject.featureFlags().primaryFlags() |= SOLDIER_COVERT_TEMPORARY_OVERT;

			// in realtime mode, remember the second when &subject event happened. Once suspicion is checked, we are either uncovered or, if enough time has passed, no longer suspicious
			// in turnbase mode, remember our current APs. If a new turn has started or enough APs have been used, remove the flag
			subject.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_SECONDS) = GetWorldTotalSeconds( ) + max( 1, appenalty / 25 );
			subject.skillState().cooldown(SOLDIER_COOLDOWN_COVERTOPS_TEMPORARYOVERT_APS) = appenalty;
		}
	}

	// If our own guy...
	if ( subject.roster().team() == gbPlayerNum )
	{
		// Are we stationary?
		if ( gAnimControl[usNewState].uiFlags & ANIM_STATIONARY )
		{
			// Position light....
		}
		else
		{
			// Hide light.....
		}
	}

	// If we are certain animations, reload palette
	if ( usNewState == VEHICLE_DIE || usNewState == CHARIOTS_OF_FIRE || usNewState == BODYEXPLODING )
	{
		(void)TacticalActorAppearance::rebuildPalettes(subject);
	}

	// ATE: if the old animation was a movement, and new is not, play sound...
	// OK, play final footstep sound...
	if ( !(gTacticalStatus.uiFlags & LOADING_SAVED_GAME) )
	{
		if ( (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_STATIONARY) &&
			 (gAnimControl[subject.animationPlayback().previousState()].uiFlags & ANIM_MOVING) )
		{
			PlaySoldierFootstepSound( &subject );
		}
	}

	// Free up from stance change
	FreeUpNPCFromStanceChange( &subject );

	return(TRUE);
}
