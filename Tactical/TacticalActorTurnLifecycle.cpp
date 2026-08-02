#include "TacticalActorTurnLifecycle.h"

#include "TacticalActorBloodState.h"
#include "TacticalActorBleeding.h"
#include "TacticalActorEmploymentTypes.h"
#include "TacticalActorQuoteFlags.h"
#include "TacticalActorStateFlags.h"

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


void TacticalActorTurnLifecycle::beginTurn(TacticalActor& subject, bool fFromRealTime, INT32 iRealTimeCounter)
{
	if (!subject.roster().active())
	{
		return;
	}

	// NB realtimecounter is not used, always passed in as 0 now!

	INT32 iBlood;

	if ( subject.suppression().underFire() )
	{
		// UnderFire now starts at 2 for "under fire &subject turn",
		// down to 1 for "under fire last turn", to 0.
		subject.suppression().underFire()--;

		if ( !subject.suppression().underFire() )
			subject.featureFlags().secondaryFlags() &= ~SOLDIER_TAKEN_LARGE_HIT;
	}

	// sevenfm: reset AI flags
	subject.featureFlags().secondaryFlags() &= ~SOLDIER_BACK_ATTACK;
	subject.featureFlags().secondaryFlags() &= ~SOLDIER_SNEAK_ATTACK;

	// Flugente: reset extra stats. Currently they only depend on drug effects, and those are reset every turn
	subject.condition().clearExtraStats();

	// ATE: Add decay effect sfor drugs...
	//if ( fFromRealTime  ) //&& iRealTimeCounter % 300 )
	{
		HandleEndTurnDrugAdjustments_New( &subject );
	}

	// sevenfm: update morale
	RefreshSoldierMorale(&subject);

	// ATE: Don't bleed if in AUTO BANDAGE!
	if ( !gTacticalStatus.fAutoBandageMode )
	{
		// Blood is not for the weak of heart, or mechanical
		if ( !(subject.status().flags() & (SOLDIER_VEHICLE | SOLDIER_ROBOT)) )
		{
			if ( subject.vitals().bleeding() || subject.vitals().health() < OKLIFE ) // is he bleeding or dying?
			{
				iBlood = TacticalActorBleeding::check(subject);	// check if he might lose another life point

				// ATE: Only if in sector!
				if ( subject.roster().inSector() )
				{
					if ( iBlood != NOBLOOD )
					{
						DropBlood( &subject, (INT8)iBlood, subject.awareness().visibility() );
					}
				}
			}
		}
	}

	// survived bleeding, but is he out of breath?
	if ( subject.vitals().health() && !subject.vitals().breath() && TacticalActorMobility::inWater(subject) )
	{
		// Drowning...
	}

	// if he is still alive (didn't bleed to death)
	if ( subject.vitals().health() )
	{
		// reduce the effects of any residual shock from past injuries by half
		subject.suppression().shock() /= 2;

		// sevenfm: increase morale for AI soldiers
		if (subject.identity().profile() == NO_PROFILE &&
			!(subject.status().flags() & SOLDIER_VEHICLE) &&
			!AM_A_ROBOT((&subject)) &&
			!ARMED_VEHICLE((&subject)) &&
			subject.suppression().shock() == 0 &&
			!subject.suppression().underFire() &&
			subject.morale().morale() < 80 + 2 * subject.statistics().experienceLevel())
		{
			subject.morale().morale() = __min(80 + 2 * subject.statistics().experienceLevel(), subject.morale().morale() + 2 + subject.statistics().experienceLevel() / 5);
		}

		// If the subject has heard a noise that has not been investigated.
		if ( subject.perception().noiseGrid() != NOWHERE )
		{
			if ( subject.perception().noiseVolume() )	// and the noise volume is still positive
			{
				subject.perception().noiseVolume()--;	// the volume of the noise "decays" by 1 point

				if ( !subject.perception().noiseVolume() )	// if the volume has reached zero
				{
					subject.perception().noiseGrid() = NOWHERE;		// forget about the noise!
				}
			}
		}

		if ( subject.status().flags() & SOLDIER_GASSED )
		{
			// then must get a gas mask or leave the gassed area to get over it
			if ( DoesSoldierWearGasMask( &subject ) && subject.inventory()[FindGasMask( &subject )][0]->data.objectStatus >= GASMASK_MIN_STATUS || !(GetSmokeEffectOnTile( subject.position().gridNo(), subject.position().level() )) )//dnl ch40 200909
				subject.status().flags() &= (~SOLDIER_GASSED);
		}

		if ( subject.perception().ageBlindness() )
		{
			// we can SEE!!!!!
			HandleSight( &subject, SIGHT_LOOK );
			// Dirty panel
			fInterfacePanelDirty = DIRTYLEVEL2;
		}


		subject.perception().ageDeafness();

		// ATE: To get around a problem...
		// If an AI guy, and we have 0 life, and are still at higher hieght,
		// Kill them.....

		// Flugente: update for various personal properties
		// &subject has to happen before CalculateCarriedWeight(), otherwise strength modfiers will not be detected correctly
		TacticalActorTurnMaintenance::maintainAtTurnStart(subject);

		// Flugente: drug users might consume useful drugs on their own in combat
		(void)TacticalActorConsumables::autoUseDrug(subject);

		subject.movementMetrics().recordCarriedWeightAtTurnStart(
			(INT16)CalculateCarriedWeight( &subject ) );

		UnusedAPsToBreath( &subject );

		// Set flag back to normal, after reaching a certain statge
		if ( subject.vitals().breath() > 80 )
		{
			subject.dialogue().clearSaid(SOLDIER_QUOTE_SAID_LOW_BREATH);
		}
		if ( subject.vitals().breath() > 50 )
		{
			subject.dialogue().clearSaid(SOLDIER_QUOTE_SAID_DROWNING);
		}


		if ( subject.dialogue().heardNoiseCooldownTurns() > 0 )
		{
			subject.dialogue().ageHeardNoiseCooldown();
		}

		if ( subject.roster().inSector() )
		{
			(void)TacticalActorRecovery::checkBreathCollapse(subject);
		}

		(void)TacticalActorTurnBudget::refreshForTurn(subject);

		// SANDRO - Improved Interrupt System - reset interrupt counter
		memset( subject.turnState().interruptCounters(), 0, sizeof(subject.turnState().interruptCounters()) );

		// HEADROCK HAM 3.6: If this soldier is in a "moving" animation, but has not moved any tiles
		// in the previous turn, then the player has apparently forgotten that he was moving.
		// In &subject case, abort the character's action.

		// If hasn't moved since the start of last round
		// AND this function is being executed in turn-based mode
		// AND character is a player-controlled merc
		if ( !fFromRealTime && !subject.movementMetrics().movedThisTurn() && subject.roster().team() == OUR_TEAM )
		{
			// but are doing a movement animation
			if ( !(gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_STATIONARY) )
			{
				// Stop the merc
				(void)TacticalActorRouteExecution::stopAt(subject, subject.position().gridNo(), subject.position().direction() );
				subject.pathing().finalDestinationGrid() = NOWHERE;
			}

			// Reset destination
			//subject.pathing().finalDestinationGrid() = subject.sGridNo;
		}

		subject.movementMetrics().clearTurnDistance();

		if ( subject.roster().inSector() )
		{
			(void)TacticalActorRecovery::beginGetUp(subject);

			// CJC Nov 30: handle RT opplist decaying in another function which operates less often
			if ( IsJa2TacticalCombatActive() )
			{
				VerifyAndDecayOpplist( &subject );

				// turn off xray
				if ( subject.perception().xrayActive() )
				{
					TurnOffXRayEffects( &subject );
				}
			}

			if ( (subject.roster().team() == gbPlayerNum) && (subject.identity().profile() != NO_PROFILE) )
			{
				if ( DoesMercHaveDisability( &subject, HEAT_INTOLERANT ) )
				{
					if ( MercIsHot( &subject ) )
					{
						HandleMoraleEvent( &subject, MORALE_HEAT_INTOLERANT_IN_DESERT, subject.deployment().sectorX(), subject.deployment().sectorY(), subject.deployment().sectorZ() );
						if ( !subject.dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) && subject.employment().mercenaryType() != MERC_TYPE__PLAYER_CHARACTER )
						{
							TacticalCharacterDialogue( &subject, QUOTE_PERSONALITY_TRAIT );
							subject.dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

							// Flugente: dynamic opinions
							if (gGameExternalOptions.fDynamicOpinions)
							{
								HandleDynamicOpinionChange(&subject, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
							}
						}
					}
				}

				if ( DoesMercHaveDisability( &subject, FEAR_OF_INSECTS ) )
				{
					if ( MercSeesCreature( &subject ) )
					{
						HandleMoraleEvent( &subject, MORALE_INSECT_PHOBIC_SEES_CREATURE, subject.deployment().sectorX(), subject.deployment().sectorY(), subject.deployment().sectorZ() );
						if ( !subject.dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) )
						{
							TacticalCharacterDialogue( &subject, QUOTE_PERSONALITY_TRAIT );
							subject.dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

							// Flugente: dynamic opinions
							if (gGameExternalOptions.fDynamicOpinions)
							{
								HandleDynamicOpinionChange(&subject, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
							}
						}
					}
				}

				if ( DoesMercHaveDisability( &subject, CLAUSTROPHOBIC ) )
				{
					if ( gbWorldSectorZ > 0 && Random( 6 - gbWorldSectorZ ) == 0 )
					{
						// underground!
						HandleMoraleEvent( &subject, MORALE_CLAUSTROPHOBE_UNDERGROUND, subject.deployment().sectorX(), subject.deployment().sectorY(), subject.deployment().sectorZ() );
						if ( !subject.dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) )
						{
							TacticalCharacterDialogue( &subject, QUOTE_PERSONALITY_TRAIT );
							subject.dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

							// Flugente: dynamic opinions
							if (gGameExternalOptions.fDynamicOpinions)
							{
								HandleDynamicOpinionChange(&subject, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
							}
						}
					}
				}

				if ( DoesMercHaveDisability( &subject, NERVOUS ) )
				{
					if ( DistanceToClosestFriend( &subject ) > NERVOUS_RADIUS )
					{
						// augh!!
						if ( subject.morale().morale() < 50 )
						{
							HandleMoraleEvent( &subject, MORALE_NERVOUS_ALONE, subject.deployment().sectorX(), subject.deployment().sectorY(), subject.deployment().sectorZ() );
							if ( !subject.dialogue().hasSaid(SOLDIER_QUOTE_SAID_PERSONALITY) )
							{
								TacticalCharacterDialogue( &subject, QUOTE_PERSONALITY_TRAIT );
								subject.dialogue().markSaid(SOLDIER_QUOTE_SAID_PERSONALITY);

								// Flugente: dynamic opinions
								if (gGameExternalOptions.fDynamicOpinions)
								{
									HandleDynamicOpinionChange(&subject, OPINIONEVENT_ANNOYINGDISABILITY, TRUE, TRUE);
								}
							}
						}
					}
					else
					{
						if ( subject.morale().morale() > 45 )
						{
							// turn flag off, so that we say it every two turns
							subject.dialogue().clearSaid(SOLDIER_QUOTE_SAID_PERSONALITY);
						}
					}
				}
			}
		}

		// Reset quote flags for under heavy fire and close call!
		subject.dialogue().clearSaid(SOLDIER_QUOTE_SAID_BEING_PUMMELED);
		subject.dialogue().clearSaidExtended(SOLDIER_QUOTE_SAID_EXT_CLOSE_CALL);
		subject.combatResult().hitsThisTurn() = 0;

		// HEADROCK HAM 3.5: After considerable testing, suppression is now cleared after every attack. Total APs lost
		// is cleared every turn (here) and only acts as reference now (no effect on AP loss).
		subject.suppression().beginTurn();
		subject.runtime().combatFeedback.lastShock = 0;
		subject.runtime().combatFeedback.lastSuppression = 0;
		subject.runtime().combatFeedback.lastActionPoints = 0;
		subject.runtime().combatFeedback.lastMorale = 0;
		subject.runtime().combatFeedback.lastActionPointsFromHit = 0;
		subject.runtime().combatFeedback.lastShockFromHit = 0;
		subject.runtime().combatFeedback.lastMoraleFromHit = 0;
		subject.runtime().combatFeedback.lastBulletImpact = 0;
		subject.runtime().combatFeedback.lastArmourProtection = 0;

		subject.perception().clearMovementDirections();

		// If soldier has new APs, reset flags!
		if ( subject.actionPoints().current() > 0 )
		{
			subject.uiPresentation().clearNoActionPoints();
			subject.turnState().moved() = FALSE;
			subject.turnState().passedLastInterrupt() = FALSE;
		}
	}

	// HEADROCK HAM 4: Store this soldier's X/Y cell coordinates in its actor data.
	INT16 sStartPosX = 0;
	INT16 sStartPosY = 0;
	ConvertGridNoToCenterCellXY( subject.position().gridNo(), &sStartPosX, &sStartPosY );
	subject.position().recordTurnStart(sStartPosX, sStartPosY);

	// Flugente: Cool down all weapons and decay food in inventory
	TacticalActorEquipment::coolDownInventory(subject);
}
