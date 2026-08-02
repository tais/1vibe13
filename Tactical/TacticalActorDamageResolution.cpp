#include "TacticalActorDamageResolution.h"

#include "Grid Direction.h"
#include "Disease.h"
#include "Soldier Profile Constants.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorQuoteFlags.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"

#include "TacticalActorBattleSounds.h"
#include "connect.h"

#include "TacticalActorAnimationTransitions.h"
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


#include "Ja25_Tactical.h"
#include "Ja25 Strategic Ai.h"
#include "GameContext.h"
#include "Meanwhile.h"

void SoldierGotHitGunFire(TacticalActor*, UINT16, INT16, UINT16, UINT16, SoldierID, UINT8, UINT8);
void SoldierGotHitBlade(TacticalActor*, UINT8);
void SoldierGotHitPunch(TacticalActor*, UINT16, INT16, UINT16, UINT16, SoldierID, UINT8, UINT8);
void SoldierGotHitExplosion(TacticalActor*, UINT16, INT16, UINT16, UINT16, SoldierID, UINT8, UINT8);
void SoldierGotHitVehicle(TacticalActor*, UINT16);
UINT8 CalcScreamVolume(TacticalActor*, UINT8);

extern BOOLEAN fReDrawFace;
extern void ReduceAttachmentsOnGunForNonPlayerChars(
	TacticalActor* actor,
	OBJECTTYPE* object);

void TacticalActorDamageResolution::applyHit(TacticalActor& subject, UINT16 usWeaponIndex, INT16 sDamage, INT16 sBreathLoss, UINT16 bDirection, UINT16 sRange, SoldierID ubAttackerID, UINT8 ubSpecial, UINT8 ubHitLocation, INT16 sSubsequent, INT32 sLocationGrid)
{
	if (usWeaponIndex >= MAXITEMS)
	{
		return;
	}

	UINT8		ubCombinedLoss, ubVolume, ubReason;
	TacticalActor* attacker =
		GetJa2SoldierRepository().resolve( ubAttackerID );
	//	TacticalActor * pNewSoldier;

	if (gTacticalStatus.uiFlags & GODMODE && subject.roster().team() == OUR_TEAM)
	{
		sDamage = 0;
		ubSpecial = FIRE_WEAPON_NO_SPECIAL;
	}

	ubReason = 0;

	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, "EVENT_SoldierGotHit" );


	// DO STUFF COMMON FOR ALL TYPES
	if ( attacker != nullptr )
	{
		attacker->combatResult().lastAttackHit() = TRUE;
	}

	// Keep the attacker and hit location as one combat-result transition.
	subject.combatResult().recordHit(ubAttackerID, ubHitLocation);

	// handle morale for heavy damage attacks
	if ( sDamage > 25 )
	{
		if ( attacker != nullptr && attacker->roster().team() == gbPlayerNum )
		{
			HandleMoraleEvent( attacker, MORALE_DID_LOTS_OF_DAMAGE,
				attacker->deployment().sectorX(), attacker->deployment().sectorY(), attacker->deployment().sectorZ() );
			subject.runtime().combatFeedback.lastMoraleFromHit++;
		}
		if ( subject.roster().team() == gbPlayerNum )
		{
			HandleMoraleEvent( &subject, MORALE_TOOK_LOTS_OF_DAMAGE, subject.deployment().sectorX(), subject.deployment().sectorY(), subject.deployment().sectorZ() );
			subject.runtime().combatFeedback.lastMoraleFromHit++;
		}
	}

	// SWITCH IN TYPE OF WEAPON
	if ( ubSpecial == FIRE_WEAPON_TOSSED_OBJECT_SPECIAL )
	{
		ubReason = TAKE_DAMAGE_OBJECT;
	}
	else if ( Item[usWeaponIndex].usItemClass & IC_TENTACLES )
	{
		ubReason = TAKE_DAMAGE_TENTACLES;
	}
	else if ( ubSpecial == FIRE_WEAPON_VEHICLE_TRAUMA )
	{
		ubReason = TAKE_DAMAGE_VEHICLE_TRAUMA;
	}
	// Flugente: check the ammo
	else if ( attacker != nullptr
		&& attacker->inventory()[HANDPOS].exists()
		&& AmmoTypes[*&( attacker->inventory()[HANDPOS] )[0]->data.gun.ubGunAmmoType].ammoflag & AMMO_TRAIL_FIRE )
	{
		ubReason = TAKE_DAMAGE_GAS_FIRE;

		INT16 fireresistance = ArmourVersusFirePercent( &subject );

		sDamage = max( 0, sDamage  * ( 100 - fireresistance ) / 100 );
	}

	// marke take out gunfire if ammotype is explosive

	// callahan update start
	// setting new func to intercept testhit
	else if ( Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE) && attacker == nullptr )
	{
		sBreathLoss += APBPConstants[BP_GET_HIT];
		ubReason = TAKE_DAMAGE_GUNFIRE;
	}
	// callahan update end

	else if ( Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE) &&
		attacker != nullptr &&
		AmmoTypes[ attacker->inventory()[ attacker->attackSelection().hand() ][0]->data.gun.ubGunAmmoType ].explosionSize <= 1 )
	{
		if ( ubSpecial == FIRE_WEAPON_SLEEP_DART_SPECIAL )
		{
			(void)TacticalActorRecovery::applySleepDart(
				subject,
				sBreathLoss);

		}
		else if ( ubSpecial == FIRE_WEAPON_BLINDED_BY_SPIT_SPECIAL )
		{
			// blinded!!
			if ( subject.perception().blindnessTurns() == 0 )
			{
				// say quote
				if ( subject.status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( &subject, QUOTE_BLINDED );
				}
				DecayIndividualOpplist( &subject );
			}
			// will always increase counter by at least 1
			subject.perception().addBlindness((sDamage / 8) + 1);

			// Dirty panel
			fInterfacePanelDirty = DIRTYLEVEL2;
		}
		// Flugente: like FIRE_WEAPON_BLINDED_BY_SPIT_SPECIAL but without the damage dependency
		else if ( ubSpecial == FIRE_WEAPON_BLINDED_SPECIAL )
		{
			// blinded!!
			if ( subject.perception().blindnessTurns() == 0 )
			{
				// say quote
				if ( subject.status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( &subject, QUOTE_BLINDED );
				}
				DecayIndividualOpplist( &subject );
			}

			subject.perception().addBlindness(2 * Random( 3 ) + 2);

			// Dirty panel
			fInterfacePanelDirty = DIRTYLEVEL2;
		}
		sBreathLoss += APBPConstants[BP_GET_HIT];
		ubReason = TAKE_DAMAGE_GUNFIRE;
	}
	else if ( Item[usWeaponIndex].usItemClass & IC_BLADE )
	{
		// SANDRO - slightly reduce breath damage of melee weapons, it is an issue for martial arts
		if ( gGameOptions.fNewTraitSystem )
			sBreathLoss = (APBPConstants[BP_GET_HIT] * (100 + gSkillTraitValues.bPercentModifierBladesBreathLoss) / 100);
		else
			sBreathLoss = APBPConstants[BP_GET_HIT];
		ubReason = TAKE_DAMAGE_BLADE;

		// Flugente: check wether we can make &subject blade bloody
		if ( attacker != nullptr && attacker->inventory()[HANDPOS].exists( ) )
		{
			if ( Item[ attacker->inventory()[HANDPOS].usItem ].bloodieditem > 0 )
			{
				// magic happens
				attacker->inventory()[HANDPOS].usItem = Item[ attacker->inventory()[HANDPOS].usItem ].bloodieditem;
			}

			// Flugente: if the blade is infected, infect the victim
			if ( *&(attacker->inventory()[HANDPOS])[0]->data.sObjectFlag & INFECTED && gGameExternalOptions.fDiseaseContaminatesItems )
			{
				// infect us with the first disease
				TacticalActorDisease::infect(subject, 0 );
			}

			// If the subject has the disease, infect the blade.
			if ( subject.condition().infected(0) )
				*&(attacker->inventory()[HANDPOS])[0]->data.sObjectFlag |= INFECTED;
		}
	}
	else if ( Item[usWeaponIndex].usItemClass & IC_PUNCH )
	{
		////////////////////////////////////////////////////////////////////////////
		// SANDRO - STOMP traits
		UINT16 sBreathRegainPenaltyMultiplier = 0;
		if ( gGameOptions.fNewTraitSystem )
		{
			if ( attacker != nullptr )
			{
				if ( !(attacker->inventory()[HANDPOS].exists( )) || ItemIsBrassKnuckles(attacker->inventory()[HANDPOS].usItem) )
				{
					// with enhanced CCS, make the lost breath harder to regenerate, which makes CQC more usable
					if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
						sBreathRegainPenaltyMultiplier = 10;

					sBreathLoss = sDamage * (100 + gSkillTraitValues.bPercentModifierHtHBreathLoss); // 80% only for untrained mercs

					// martial arts bonus for breath damage
					if ( HAS_SKILL_TRAIT( attacker, MARTIAL_ARTS_NT ) )
					{
						sBreathLoss += sDamage * gSkillTraitValues.ubMABonusBreathDamageHandToHand * NUM_SKILL_TRAITS( attacker, MARTIAL_ARTS_NT );

						sBreathRegainPenaltyMultiplier += (gSkillTraitValues.usMALostBreathRegainPenalty * NUM_SKILL_TRAITS( attacker, MARTIAL_ARTS_NT ));
					}
				}
				else
				{
					// with enhanced CCS, make the lost breath harder to regenerate, which makes CQC more usable
					if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
						sBreathRegainPenaltyMultiplier = 15;

					sBreathLoss = sDamage * (100 + gSkillTraitValues.bPercentModifierBluntBreathLoss); // 50% only for melee weapons
				}
			}
			else
			{
				sBreathLoss = sDamage * (100 + gSkillTraitValues.bPercentModifierHtHBreathLoss);
			}
			// bodybuilding reduces &subject to half
			if ( HAS_SKILL_TRAIT( (&subject), BODYBUILDING_NT ) )
			{
				sBreathLoss = max( 10, (sBreathLoss * (100 - gSkillTraitValues.ubBBBreathLossForHtHImpactReduction) / 100) );
			}
		}
		else
		{
			// with enhanced CCS, make the lost breath harder to regenerate, which makes CQC more usable
			if ( gGameExternalOptions.fEnhancedCloseCombatSystem )
				sBreathRegainPenaltyMultiplier = 10;
			// damage from hand-to-hand is 1/4 normal, 3/4 breath.. the sDamage value
			// is actually how much breath we'll take away
			sBreathLoss = sDamage * 100;
		}
		if ( sBreathRegainPenaltyMultiplier > 0 )
		{
			// unregainable breath damage
			subject.vitals().unregainableBreath() += ((sBreathLoss * sBreathRegainPenaltyMultiplier) / 100);
		}
		////////////////////////////////////////////////////////////////////////////
		sDamage = sDamage / PUNCH_REAL_DAMAGE_PORTION;
		if ( !GetGameContext().capabilities().isUnfinishedBusiness() &&
			AreInMeanwhile( ) &&
			gCurrentMeanwhileDef.ubMeanwhileID == INTERROGATION )
		{
			sBreathLoss = 0;
			sDamage /= 4;
			sDamage = max(1, sDamage);
		}
		ubReason = TAKE_DAMAGE_HANDTOHAND;

		// Flugente: if the weapon is a taser and has enough batteries, the damage will be 0, but the breathdamage will knock out anyone
		if ( HasItemFlag( usWeaponIndex, TASER ) )
		{
			// tasers need batteries, because I say so
			if (attacker != nullptr && ItemNeedsBatteries(usWeaponIndex))
			{
				// check for batteries
				OBJECTTYPE* pBatteries = FindAttachedBatteries( &(attacker->inventory()[HANDPOS]) );
				if ( pBatteries )
				{
					sDamage = 0;
					sBreathLoss = 30000;
					ubReason = TAKE_DAMAGE_ELECTRICITY;

					// use up 8-12 percent of batteries
					if ( Item[pBatteries->usItem].percentstatusdrainreduction > 0 )
						(*pBatteries)[0]->data.objectStatus -= (INT8)((8 + Random( 5 )) * (100 - Item[(*pBatteries)[0]->data.objectStatus].percentstatusdrainreduction) / 100);
					else
						(*pBatteries)[0]->data.objectStatus -= (INT8)((8 + Random( 5 )));
					if ( (*pBatteries)[0]->data.objectStatus <= 0 )
					{
						// destroy batteries
						pBatteries->RemoveObjectsFromStack( 1 );
						if ( pBatteries->exists( ) == false ) {
							attacker->inventory()[HANDPOS].RemoveAttachment( pBatteries );
						}
					}

					// insert electrical sound effect here
					PlayJA2Sample( DOOR_ELECTRICITY, RATE_11025, SoundVolume( MIDVOLUME, subject.position().gridNo() ), 1, SoundDir( subject.position().gridNo() ) );
				}
			}
		}
	}
	// marke added one 'or' for explosive ammo. variation of: AmmoTypes[subject.inventory()[subject.attackSelection().hand() ][0]->data.gun.ubGunAmmoType].explosionSize > 1
	//  extracting attacker's ammo type
	else if ( Item[usWeaponIndex].usItemClass & IC_EXPLOSV ||
		(attacker != nullptr &&
		 AmmoTypes[attacker->inventory()[attacker->attackSelection().hand()][0]->data.gun.ubGunAmmoType].explosionSize > 1) )
	{
		INT8 bDeafValue;

		bDeafValue = Explosive[Item[usWeaponIndex].ubClassIndex].ubVolume / 10;
		if ( bDeafValue == 0 )
			bDeafValue = 1;

		// Lesh: flashbang does damage
		switch ( ubSpecial )
		{
		case FIRE_WEAPON_BLINDED_AND_DEAFENED:
			subject.perception().setDeafness(bDeafValue);
			//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Soldier is blinded and deafened" );

			// if soldier in building OR underground
			if ( InBuilding( sLocationGrid ) || (gbWorldSectorZ) )
			{
				// deal max special damage
				subject.perception().setBlindness((INT8)Explosive[Item[usWeaponIndex].ubClassIndex].ubDuration);
				// say quote
				if ( subject.status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( &subject, QUOTE_BLINDED );
				}
			}
			else if ( NightTime( ) ) // if soldier outside at night
			{
				// halve effect
				subject.perception().setBlindness((INT8)Explosive[Item[usWeaponIndex].ubClassIndex].ubDuration / 2);
				if ( subject.perception().blindnessTurns() == 0 )
					subject.perception().setBlindness(1);
				subject.perception().halveDeafness();
				// say quote
				if ( subject.status().flags() & SOLDIER_PC )
				{
					TacticalCharacterDialogue( &subject, QUOTE_BLINDED );
				}
			}
			DecayIndividualOpplist( &subject );
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_BLINDED, ubAttackerID );
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_DEAFENED, ubAttackerID );
			break;

		case FIRE_WEAPON_BLINDED:
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_BLINDED, ubAttackerID );
			break;

		case FIRE_WEAPON_DEAFENED:
			//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_INTERFACE, L"Soldier is deafened" );
			subject.perception().setDeafness(bDeafValue);
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_DEAFENED, ubAttackerID );
			break;
		};

		if ( usWeaponIndex == STRUCTURE_EXPLOSION )
		{
			ubReason = TAKE_DAMAGE_STRUCTURE_EXPLOSION;
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_STRUCTURE_EXPLOSION, ubAttackerID );
		}
		else
		{
			ubReason = TAKE_DAMAGE_EXPLOSION;
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_EXPLOSION, ubAttackerID );
		}
	}
	else
	{
		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Control: Weapon class not handled in SoldierGotHit( ) %d", usWeaponIndex ) );
	}



	// Flugente: moved the damage calculation into a separate function
	sBreathLoss = max( 1, (INT16)(sBreathLoss * (100 - TacticalActorModifiers::damageResistance(subject, false, true)) / 100) );

	// OK, If we are a vehicle.... damage vehicle...( people inside... )
	if ( subject.status().flags() & SOLDIER_VEHICLE )
	{
		TacticalActorDamageResolution::takeDamage(subject,  ANIM_CROUCH, sDamage, sBreathLoss, ubReason, subject.combatResult().currentAttacker(), NOWHERE, FALSE, TRUE );
		return;
	}

	// sevenfm: player mercs should not die instantly
	if (gGameExternalOptions.fReducedInstantDeath &&
		subject.status().flags() & SOLDIER_PC &&
		subject.vitals().health() >= OKLIFE &&
		ubSpecial != FIRE_WEAPON_HEAD_EXPLODE_SPECIAL &&
		ubSpecial != FIRE_WEAPON_CHEST_EXPLODE_SPECIAL &&
		Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE | IC_BLADE))
	{
		sDamage = __min(sDamage, subject.vitals().health() - OKLIFE + 1 + sDamage / 10);
	}

	// DEDUCT LIFE
	ubCombinedLoss = TacticalActorDamageResolution::takeDamage(subject,  ANIM_CROUCH, sDamage, sBreathLoss, ubReason, subject.combatResult().currentAttacker(), NOWHERE, FALSE, TRUE );

	// ATE: OK, Let's check our ASSIGNMENT state,
	// If anything other than on a squad or guard, make them guard....
	if ( subject.roster().team() == gbPlayerNum )
	{
		if ( subject.assignment().current() >= ON_DUTY && subject.assignment().current() != ASSIGNMENT_POW && subject.assignment().current() != ASSIGNMENT_MINIEVENT && subject.assignment().current() != ASSIGNMENT_REBELCOMMAND)
		{
			if ( subject.assignment().isAsleep() )
			{
				subject.assignment().wakeUp();
				subject.assignment().releaseForcedAwake();

				// refresh map screen
				fCharacterInfoPanelDirty = TRUE;
				fTeamPanelDirty = TRUE;
			}

			AddCharacterToAnySquad( &subject );
		}
	}


	// SCREAM!!!!
	ubVolume = CalcScreamVolume( &subject, ubCombinedLoss );

	// IF WE ARE AT A HIT_STOP ANIMATION
	// DO APPROPRIATE HITWHILE DOWN ANIMATION
	if ( !(gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_HITSTOP) || subject.animationPlayback().state() != JFK_HITDEATH_STOP )
	{
		MakeNoise( subject.identity().id(), subject.position().gridNo(), subject.position().level(), subject.position().terrainType(), ubVolume, NOISE_SCREAM );
	}

	// IAN ADDED THIS SAT JUNE 14th : HAVE TO SHOW VICTIM!
	if ( IsJa2TacticalTurnBasedCombat() && subject.awareness().visibility() != -1 && subject.roster().team() == gbPlayerNum )
		LocateSoldier( subject.identity().id(), DONTSETLOCATOR );


	if ( Item[usWeaponIndex].usItemClass & IC_BLADE )
	{
		PlayJA2Sample( (UINT32)(KNIFE_IMPACT), RATE_11025, SoundVolume( MIDVOLUME, subject.position().gridNo() ), 1, SoundDir( subject.position().gridNo() ) );
	}
	else
	{
		PlayJA2Sample( (UINT32)(BULLET_IMPACT_1 + Random( 3 )), RATE_11025, SoundVolume( MIDVOLUME, subject.position().gridNo() ), 1, SoundDir( subject.position().gridNo() ) );
	}

	// PLAY RANDOM GETTING HIT SOUND
	// ONLY IF WE ARE CONSCIOUS!
	if ( subject.vitals().health() >= CONSCIOUSNESS )
	{
		if ( subject.identity().bodyType() == CROW )
		{
			// Exploding crow...
			PlayJA2Sample( CROW_EXPLODE_1, RATE_11025, SoundVolume( HIGHVOLUME, subject.position().gridNo() ), 1, SoundDir( subject.position().gridNo() ) );
		}
		else
		{
			// ATE: This is to disallow large amounts of smaples being played which is load!
			if ( subject.animationActivity().hitPhase() && subject.animationPlayback().code() != STANDING_BURST_HIT )
			{

			}
			else
			{
				TacticalActorBattleSounds::play(subject,  BATTLE_SOUND_HIT1 );
			}
		}
	}

	// anv: soldier got rammed by vehicle
	if ( ubSpecial == FIRE_WEAPON_VEHICLE_TRAUMA )
	{
		SoldierGotHitVehicle( &subject, bDirection );
		return;
	}

	// CHECK FOR DOING HIT WHILE DOWN
	if ( (gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_HITSTOP) )
	{
		switch ( subject.animationPlayback().state() )
		{
		case FLYBACKHIT_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  FALLBACK_DEATHTWICH, 0, FALSE );
			break;

		case STAND_FALLFORWARD_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  GENERIC_HIT_DEATHTWITCHNB, 0, FALSE );
			break;

		case JFK_HITDEATH_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  JFK_HITDEATH_TWITCHB, 0, FALSE );
			break;

		case FALLBACKHIT_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  FALLBACK_HIT_DEATHTWITCHNB, 0, FALSE );
			break;

		case PRONE_LAYFROMHIT_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  PRONE_HIT_DEATHTWITCHNB, 0, FALSE );
			break;

		case PRONE_HITDEATH_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  PRONE_HIT_DEATHTWITCHB, 0, FALSE );
			break;

		case FALLFORWARD_HITDEATH_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  GENERIC_HIT_DEATHTWITCHB, 0, FALSE );
			break;

		case FALLBACK_HITDEATH_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  FALLBACK_HIT_DEATHTWITCHB, 0, FALSE );
			break;

		case FALLOFF_DEATH_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  FALLOFF_TWITCHB, 0, FALSE );
			break;

		case FALLOFF_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  FALLOFF_TWITCHNB, 0, FALSE );
			break;

		case FALLOFF_FORWARD_DEATH_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  FALLOFF_FORWARD_TWITCHB, 0, FALSE );
			break;

		case FALLOFF_FORWARD_STOP:
			TacticalActorAnimationTransitions::changeState(subject,  FALLOFF_FORWARD_TWITCHNB, 0, FALSE );
			break;

		default:
			DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "Soldier Control: Death state %d has no death hit", subject.animationPlayback().state() ) );

		}
		return;
	}

	// Set goback to aim after hit flag!
	// SANDRO - added more cases, alternative weapon holding, go back to cowering, and go back to hth/blade stance
	// If we were in hth or blade stance, and we were hit by HtH or blade attack, go back to the fighting stance (if we can still keep up)
	if ( (Item[usWeaponIndex].usItemClass & (IC_BLADE | IC_PUNCH)) && Item[subject.inventory()[HANDPOS].usItem].usItemClass & (IC_NONE | IC_BLADE | IC_PUNCH) &&
		 (subject.animationPlayback().state() == PUNCH_BREATH || subject.animationPlayback().state() == KNIFE_BREATH || subject.animationPlayback().state() == NINJA_BREATH) && (gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_STAND) )
	{
		if ( subject.vitals().health() > 30 && subject.vitals().breath() > 25 )
		{
			subject.animationActivity().postHitStance() = GO_TO_HTH_BREATH_AFTER_HIT;
		}
	}
	// If we were aiming
	else if ( gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_FIREREADY )
	{
		if ( gAnimControl[subject.animationPlayback().state()].uiFlags & ANIM_ALT_WEAPON_HOLDING ) // alternative weapon holding stance
			subject.animationActivity().postHitStance() = GO_TO_ALTERNATIVE_AIM_AFTER_HIT;
		else // standard
			subject.animationActivity().postHitStance() = GO_TO_AIM_AFTER_HIT;
	}
	// This cowering animation does not use the status flag handled below.
	else if ( subject.animationPlayback().state() == COWERING )
	{
		subject.animationActivity().postHitStance() = GO_TO_COWERING_AFTER_HIT;
	}
	else
	{
		subject.animationActivity().postHitStance() = NO_SPEC_STANCE_AFTER_HIT;
	}

	// IF COWERING, PLAY SPECIFIC GENERIC HIT STAND...
	if ( subject.status().flags() & SOLDIER_COWERING )
	{
		if ( subject.vitals().health() == 0 || IS_MERC_BODY_TYPE( (&subject) ) )
		{
			TacticalActorAnimationTransitions::initializeAnimation(subject,  GENERIC_HIT_STAND, 0, FALSE );
		}
		else
		{
			TacticalActorAnimationTransitions::initializeAnimation(subject,  CIV_COWER_HIT, 0, FALSE );
		}
		return;
	}

	// Change based on body type
	switch ( subject.identity().bodyType() )
	{
	case COW:
		TacticalActorAnimationTransitions::initializeAnimation(subject,  COW_HIT, 0, FALSE );
		return;
		break;

	case BLOODCAT:
		TacticalActorAnimationTransitions::initializeAnimation(subject,  BLOODCAT_HIT, 0, FALSE );

		if ( Item[usWeaponIndex].usItemClass & ( IC_EXPLOSV | IC_BOBBY_GUN ) )
			subject.featureFlags().primaryFlags() |= SOLDIER_DAMAGED_VEST;
		return;
		break;

	case ADULTFEMALEMONSTER:
	case AM_MONSTER:
	case YAF_MONSTER:
	case YAM_MONSTER:

		TacticalActorAnimationTransitions::initializeAnimation(subject,  ADULTMONSTER_HIT, 0, FALSE );
		return;
		break;

	case LARVAE_MONSTER:
		TacticalActorAnimationTransitions::initializeAnimation(subject,  LARVAE_HIT, 0, FALSE );
		return;
		break;

	case QUEENMONSTER:
		TacticalActorAnimationTransitions::initializeAnimation(subject,  QUEEN_HIT, 0, FALSE );
		return;
		break;

	case CRIPPLECIV:

	{
					   // OK, do some code here to allow the fact that poor buddy can be thrown back if it's a big enough hit...
					   TacticalActorAnimationTransitions::initializeAnimation(subject,  CRIPPLE_HIT, 0, FALSE );

					   //subject.vitals().health() = 0;
					   //TacticalActorAnimationTransitions::initializeAnimation(subject,  CRIPPLE_DIE_FLYBACK, 0 , FALSE );


	}
		return;
		break;

	case ROBOTNOWEAPON:
		TacticalActorAnimationTransitions::initializeAnimation(subject,  ROBOTNW_HIT, 0, FALSE );
		return;
		break;


	case INFANT_MONSTER:
		TacticalActorAnimationTransitions::initializeAnimation(subject,  INFANT_HIT, 0, FALSE );
		return;

	case CROW:

		TacticalActorAnimationTransitions::initializeAnimation(subject,  CROW_DIE, 0, FALSE );
		return;

		//case FATCIV:
	case MANCIV:
	case MINICIV:
	case DRESSCIV:
	case HATKIDCIV:
	case KIDCIV:

		// OK, if life is 0 and not set as dead ( &subject is a death hit... )
		if ( !(subject.status().flags() & SOLDIER_DEAD) && subject.vitals().health() == 0 )
		{
			// Randomize death!
			if ( Random( 2 ) )
			{
				TacticalActorAnimationTransitions::initializeAnimation(subject,  CIV_DIE2, 0, FALSE );
				return;
			}
		}

		// IF here, go generic hit ALWAYS.....
		TacticalActorAnimationTransitions::initializeAnimation(subject,  GENERIC_HIT_STAND, 0, FALSE );
		return;
		break;
	}

	// If here, we are a merc, check if we are in water
	if ( TacticalActorMobility::inShallowWater(subject) )
	{
		TacticalActorAnimationTransitions::initializeAnimation(subject,  WATER_HIT, 0, FALSE );
		return;
	}
	if ( TacticalActorMobility::inDeepWater(subject) )
	{
		TacticalActorAnimationTransitions::initializeAnimation(subject,  DEEP_WATER_HIT, 0, FALSE );
		return;
	}

	// Flugente: cryo death
	if (subject.vitals().health() <= 0 && subject.skillState().cooldown(SOLDIER_COOLDOWN_CRYO) )
	{
		if ( gAnimControl[subject.animationPlayback().state()].ubEndHeight == ANIM_STAND )
			TacticalActorAnimationTransitions::initializeAnimation(subject,  CRYO_DEATH, 0, TRUE );
		else
			TacticalActorAnimationTransitions::initializeAnimation(subject,  CRYO_DEATH_CROUCHED, 0, TRUE );

		return;
	}

	// SWITCH IN TYPE OF WEAPON
	if ( Item[usWeaponIndex].usItemClass & (IC_GUN | IC_THROWING_KNIFE) )
	{
		SoldierGotHitGunFire( &subject, usWeaponIndex, sDamage, bDirection, sRange, ubAttackerID, ubSpecial, ubHitLocation );
		if ( Item[usWeaponIndex].usItemClass & IC_GUN )
		{
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_GUNFIRE, ubAttackerID );
			if (attacker != nullptr)
			{
				PossiblyStartEnemyTaunt( attacker, TAUNT_HIT_GUNFIRE, subject.identity().id() );
			}
		}
		else
		{
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_THROWING_KNIFE, ubAttackerID );
			if (attacker != nullptr)
			{
				PossiblyStartEnemyTaunt(attacker, TAUNT_HIT_THROWING_KNIFE, subject.identity().id());
			}
		}
	}
	if ( Item[usWeaponIndex].usItemClass & IC_BLADE )
	{
		SoldierGotHitBlade( &subject, ubHitLocation );
		// anv: taunts are called from UseBlade()
	}
	// marke setting ammo explosions included here with 3rd 'or' including ubReason
	if ( Item[usWeaponIndex].usItemClass & IC_EXPLOSV || Item[usWeaponIndex].usItemClass & IC_TENTACLES || ubReason == TAKE_DAMAGE_EXPLOSION )
	{
		SoldierGotHitExplosion( &subject, usWeaponIndex, sDamage, bDirection, sRange, ubAttackerID, ubSpecial, ubHitLocation );
		if ( Item[usWeaponIndex].usItemClass & IC_EXPLOSV || ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_EXPLOSION, ubAttackerID );
			//PossiblyStartEnemyTaunt( ubAttackerID, TAUNT_HIT_EXPLOSION, &subject );
		}
		else if ( Item[usWeaponIndex].usItemClass & IC_TENTACLES )
		{
			PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_TENTACLES, ubAttackerID );
			//PossiblyStartEnemyTaunt( ubAttackerID, TAUNT_HIT_TENTACLES, &subject );
		}
	}
	if ( Item[usWeaponIndex].usItemClass & IC_PUNCH )
	{
		SoldierGotHitPunch( &subject, usWeaponIndex, sDamage, bDirection, sRange, ubAttackerID, ubSpecial, ubHitLocation );
		// anv: taunts are called from UseHandToHand()
	}
}


void HandleTakeDamageDeath( TacticalActor *pSoldier, UINT8 bOldLife, UINT8 ubReason )
{
	switch ( ubReason )
	{
	case TAKE_DAMAGE_BLOODLOSS:
	case TAKE_DAMAGE_ELECTRICITY:
	case TAKE_DAMAGE_GAS_FIRE:
	case TAKE_DAMAGE_GAS_NOTFIRE:

		if ( pSoldier->roster().inSector() )
		{
			if ( pSoldier->awareness().visibility() != -1 )
			{
				if ( ubReason != TAKE_DAMAGE_BLOODLOSS )
				{
					TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_DIE1 );
					pSoldier->dialogue().markDeathSoundPlayed();
				}
			}

			if ( (ubReason == TAKE_DAMAGE_ELECTRICITY) && pSoldier->vitals().health() < OKLIFE )
			{
				pSoldier->animationActivity().nonInterruptible() = FALSE;
			}

			// silversurfer: fix for the deadlock that could happen when the victim was running through a gas cloud that lead to his death.
			// If he is near death the next check will make him collapse. If he is really dead then he won't move anywhere anyway
			// so it should be safe to stop him here.
			if ( pSoldier->vitals().health() < OKLIFE && !pSoldier->collapseState().tactical() )
			{
				(void)TacticalActorRouteExecution::stopAt(*pSoldier, pSoldier->position().gridNo(), pSoldier->position().direction() );
			}

			// Check for < OKLIFE
			if ( pSoldier->vitals().health() < OKLIFE && pSoldier->vitals().health() != 0 && !pSoldier->collapseState().tactical() )
			{
				(void)TacticalActorRecovery::collapse(*pSoldier);
			}

			// THis is for the die animation that will be happening....
			if ( pSoldier->vitals().health() == 0 )
			{
				pSoldier->animationActivity().externalDeath() = TRUE;
			}

			// Check if he is dead....
			CheckForAndHandleSoldierDyingNotFromHit( pSoldier );

		}

		//if( !( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN ) )
		{
			(void)TacticalActorDamageFeedback::presentHit(
				*pSoldier);
		}

		if ( (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN) || !pSoldier->roster().inSector() )
		{
			if ( pSoldier->vitals().health() == 0 && !(pSoldier->status().flags() & SOLDIER_DEAD) )
			{
				StrategicHandlePlayerTeamMercDeath( pSoldier );

				// ATE: Here, force always to use die sound...
				pSoldier->dialogue().clearDeathBattleSoundUsed();
				TacticalActorBattleSounds::play(*pSoldier,  BATTLE_SOUND_DIE1 );
				pSoldier->dialogue().markDeathSoundPlayed();

				// ATE: DO death sound
				PlayJA2Sample( (UINT8)DOORCR_1, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
				PlayJA2Sample( (UINT8)HEADCR_1, RATE_11025, HIGHVOLUME, 1, MIDDLEPAN );
			}
		}
		break;
	}

	// 0verhaul:  This is also already handled by the animation transitions
	// if ( ubReason == TAKE_DAMAGE_ELECTRICITY )
	// {
	//	if ( pSoldier->vitals().health() >= OKLIFE )
	//	{
	//		DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String("Freeing up attacker from electricity damage") );
	//		ReleaseSoldiersAttacker( pSoldier );
	//	}
	// }
}


std::uint8_t TacticalActorDamageResolution::takeDamage(TacticalActor& subject, INT8 bHeight, INT16 sLifeDeduct, INT16 sBreathLoss, UINT8 ubReason, SoldierID ubAttacker, INT32 sSourceGrid, INT16 sSubsequent, bool fShowDamage)
{
	if (ubReason < TAKE_DAMAGE_GUNFIRE ||
		ubReason > TAKE_DAMAGE_GAS_NOTFIRE)
	{
		return 0;
	}

#ifdef JA2BETAVERSION
	if ( is_networked ) {
		CHAR tmpMPDbgString[512];
		snprintf( tmpMPDbgString, sizeof(tmpMPDbgString), "takeDamage ( bHeight : %i , sLifeDeduct : %i , sBreathLoss : %i , ubReason : %i , ubAttacker : %i , sSourceGrid : %i , sSubsequent : %i , fShowDamage : %i )\n", bHeight, sLifeDeduct, sBreathLoss, ubReason, ubAttacker.i, sSourceGrid, sSubsequent, fShowDamage );
		MPDebugMsg( tmpMPDbgString );
	}
#endif

	INT8		bOldLife;
	UINT8		ubCombinedLoss;
	INT8		bBandage;
	INT16		sAPCost;
	UINT8		ubBlood;
	UINT16		usItemFlags = 0; // Kaiden: Needed for the reveal all items after combat code from UB.
	TacticalActor* attacker =
		GetJa2SoldierRepository().resolve( ubAttacker );

	subject.combatResult().lastDamageReason() = ubReason;

	// Flugente: dynamic opinions
	if (attacker != nullptr)
	{
		// MP: remote players' mercs (LAN teams 6..9) share global AIM profiles with our
		// own hires -- never classify PvP damage as friendly fire / civilian attack.
		if (gGameExternalOptions.fDynamicOpinions
			&& !(is_networked && (subject.roster().team() >= LAN_TEAM_ONE || attacker->roster().team() >= LAN_TEAM_ONE)))
		{
			AddOpinionEvent(subject.identity().profile(), attacker->identity().profile(), OPINIONEVENT_FRIENDLYFIRE);

			// If the subject is a non-hostile civilian, other mercs can complain
			// about the attacker shooting innocents.
			if ((subject.roster().team() != OUR_TEAM) && (subject.aiBehavior().neutral() || subject.roster().side() == attacker->roster().side()))
			{
				// not for killing animals though...
				if (subject.identity().bodyType() != CROW && subject.identity().bodyType() != COW)
					HandleDynamicOpinionChange(attacker, OPINIONEVENT_CIV_ATTACKER, TRUE, TRUE);
			}
		}

		// if we are a turncoat, lose the flag if we were attacked by player forces
		if ( (subject.featureFlags().secondaryFlags() & SOLDIER_TURNCOAT) && attacker->roster().side() == 0 )
		{
			subject.featureFlags().secondaryFlags() &= ~SOLDIER_TURNCOAT;

			RemoveOneTurncoat( subject.deployment().sectorX(), subject.deployment().sectorY(), subject.roster().soldierClass(), FALSE );
		}
	}

	// CJC Jan 21 99: add check to see if we are hurting an enemy in an enemy-controlled
	// sector; if so, &subject is a sign of player activity
	switch ( subject.roster().team() )
	{
	case ENEMY_TEAM:
		// if we're in the wilderness &subject always counts
		if ( StrategicMap[CALCULATE_STRATEGIC_INDEX( gWorldSectorX, gWorldSectorY )].fEnemyControlled || SectorInfo[SECTOR( gWorldSectorX, gWorldSectorY )].ubTraversability[THROUGH_STRATEGIC_MOVE] != TOWN )
		{
			// update current day of activity!
			UpdateLastDayOfPlayerActivity( (UINT16)GetWorldDay( ) );
		}
		break;
	case CREATURE_TEAM:
		// always a sign of activity?
		UpdateLastDayOfPlayerActivity( (UINT16)GetWorldDay( ) );
		break;
	case CIV_TEAM:
		if ( subject.roster().civilianGroup() == KINGPIN_CIV_GROUP && gubQuest[QUEST_RESCUE_MARIA] == QUESTINPROGRESS && gTacticalStatus.bBoxingState == NOT_BOXING )
		{
			TacticalActor * pMaria = FindSoldierByProfileID( MARIA, FALSE );
			if ( pMaria && pMaria->roster().active() && pMaria->roster().inSector() )
			{
				SetFactTrue( FACT_MARIA_ESCAPE_NOTICED );
			}
		}
		break;
	default:
		break;
	}

	// Flugente: do we have a riot shield equipped?
	if ( TacticalActorEquipment::hasEquippedRiotShield(subject) )
	{
		//  if we have equipped a riot shield and are being attacked in melee, ignore damage from some directions
		if ( ubReason == TAKE_DAMAGE_BLADE || ubReason == TAKE_DAMAGE_HANDTOHAND || ubReason == TAKE_DAMAGE_TENTACLES )
		{
			if ( attacker != nullptr )
			{
				UINT8 attackdir_inverse = GetDirectionToGridNoFromGridNo( subject.position().gridNo(), attacker->position().gridNo() );

				// if the shield faces the direction of the attacker, we block the attack
				if ( attackdir_inverse == subject.position().direction() || attackdir_inverse == gOneCCDirection[subject.position().direction()] || attackdir_inverse == gOneCDirection[subject.position().direction()] )
				{
					// damaging even a wooden shield is hard. For that reason we lower the initial damage.
					INT32 damage = sLifeDeduct / 3;
					INT32 breathdamage = sBreathLoss;
					DamageRiotShield( &subject, damage, breathdamage );

					sLifeDeduct = damage;
					sBreathLoss = breathdamage;

					PlayJA2Sample( (UINT32)(S_WOOD_IMPACT1 + Random(3)), RATE_11025, SoundVolume( MIDVOLUME, subject.position().gridNo() ), 1, SoundDir( subject.position().gridNo() ) );
				}
			}
		}
	}

	if (gTacticalStatus.uiFlags & GODMODE && subject.roster().team() == OUR_TEAM)
	{
		sLifeDeduct = 0;
		sBreathLoss = 0;
	}

	// Deduct life!, Show damage if we want!
	bOldLife = subject.vitals().health();

	// OK, If we are a vehicle.... damage vehicle...( people inside... )
	if ( subject.status().flags() & SOLDIER_VEHICLE )
	{
		if ( ubReason == TAKE_DAMAGE_GUNFIRE )
		{
			;
		}
		else if ( ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			if ( ARMED_VEHICLE( (&subject) ) )
				;
			else
			{
				if ( sLifeDeduct > 50 )
				{
					// boom!
					sLifeDeduct *= 2;
				}
			}
		}

		if ( sLifeDeduct > 30 )
			subject.featureFlags().secondaryFlags() |= SOLDIER_TAKEN_LARGE_HIT;

		VehicleTakeDamage( subject.vehicleState().tacticalVehicleId(), ubReason, sLifeDeduct, subject.position().gridNo(), ubAttacker );
		HandleTakeDamageDeath( &subject, bOldLife, ubReason );

		// add to our records.
		if ( attacker != nullptr && attacker->identity().profile() != NO_PROFILE )
			gMercProfiles[attacker->identity().profile()].records.usDamageDealt += sLifeDeduct;

		if ( subject.identity().profile() != NO_PROFILE )
			gMercProfiles[subject.identity().profile()].records.usDamageTaken += sLifeDeduct;

		return(0);
	}

	// ATE: If we are elloit being attacked in a meanwhile...
	if ( subject.status().flags() & SOLDIER_NPC_SHOOTING )
	{
		// Almost kill but not quite.....
		sLifeDeduct = (subject.vitals().health() - 1);
		// Turn off
		subject.status().flags() &= (~SOLDIER_NPC_SHOOTING);
	}
	// CJC: make sure Elliot doesn't bleed to death!
	if ( !GetGameContext().capabilities().isUnfinishedBusiness() &&
		ubReason == TAKE_DAMAGE_BLOODLOSS && AreInMeanwhile( ) )
	{
		return(0);
	}

	// Calculate bandage
	bBandage = subject.vitals().maximumHealth() - subject.vitals().health() - subject.vitals().bleeding();

	if ( GetCurrentScreen() == MAP_SCREEN )
	{
		fReDrawFace = TRUE;
	}

	if ( CREATURE_OR_BLOODCAT( (&subject) ) )
	{
		INT16 sReductionFactor = 0;

		if ( subject.identity().bodyType() == BLOODCAT )
		{
			sReductionFactor = 2;
		}
		else if ( subject.status().flags() & SOLDIER_MONSTER )
		{
			switch ( subject.identity().bodyType() )
			{
			case LARVAE_MONSTER:
			case INFANT_MONSTER:
				sReductionFactor = 1;
				break;
			case YAF_MONSTER:
			case YAM_MONSTER:
				sReductionFactor = 4;
				break;
			case ADULTFEMALEMONSTER:
			case AM_MONSTER:
				sReductionFactor = 6;
				break;
			case QUEENMONSTER:
				// increase with range!
				if ( attacker == nullptr )
				{
					sReductionFactor = 8;
				}
				else
				{
					sReductionFactor = 4 + PythSpacesAway( attacker->position().gridNo(), subject.position().gridNo() ) / 2;
				}
				break;
			}
		}

		if ( ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			sReductionFactor /= 4;
		}
		if ( sReductionFactor > 1 )
		{
			sLifeDeduct = (sLifeDeduct + (sReductionFactor / 2)) / sReductionFactor;
		}
		else if ( ubReason == TAKE_DAMAGE_EXPLOSION )
		{
			// take at most 2/3rds
			sLifeDeduct = (sLifeDeduct * 2) / 3;
		}

		// reduce breath loss to a smaller degree, except for the queen...
		if ( subject.identity().bodyType() == QUEENMONSTER )
		{
			// in fact, reduce breath loss by MORE!
			sReductionFactor = __min( sReductionFactor, 8 );
			sReductionFactor *= 2;
		}
		else
		{
			sReductionFactor /= 2;
		}
		if ( sReductionFactor > 1 )
		{
			sBreathLoss = (sBreathLoss + (sReductionFactor / 2)) / sReductionFactor;
		}
	}

	// Keep the legacy TacticalActor layout, but route the actual health mutation
	// through its domain view so damage policy has a testable migration seam.
	subject.vitals().applyLifeDeduction( sLifeDeduct );

	/////////////////////////////////////////////////////////////////////////////////////////////////
	// SANDRO - Doctor trait - need a variable holding the number of insta-healable hit points
	if ( (IS_MERC_BODY_TYPE( (&subject) ) || IS_CIV_BODY_TYPE( (&subject) )) && (gGameOptions.fNewTraitSystem) )
	{
		if ( subject.vitals().health() <= 0 )
		{
			// noone can help him now, he's gone
			subject.vitals().healableInjury() = 0;
		}
		else
		{
			// Otherwise add healable injury value - it's in hundredths for better precision
			subject.vitals().healableInjury() += (sLifeDeduct * 100);
			// check if we are not mysteriously beyond a limit - we cannot have more than life we actually lost
			if ( subject.vitals().healableInjury() > ((subject.vitals().maximumHealth() - subject.vitals().health()) * 100) )
				subject.vitals().healableInjury() = ((subject.vitals().maximumHealth() - subject.vitals().health()) * 100);
		}
	}
	/////////////////////////////////////////////////////////////////////////////////////////////////

	// ATE: Put some logic in here to allow enemies to die quicker.....
	// Are we an enemy?
	if ( subject.roster().side() != gbPlayerNum && !subject.aiBehavior().neutral() && subject.identity().profile() == NO_PROFILE )
	{
		// ATE: Give them a chance to fall down...
		if ( subject.vitals().health() > 0 && subject.vitals().health() < (OKLIFE - 1) )
		{
			// Are we taking damage from bleeding?
			if ( ubReason == TAKE_DAMAGE_BLOODLOSS )
			{
				// Fifty-fifty chance to die now!
				if ( Random( 3 ) == 0 || gTacticalStatus.Team[subject.roster().team()].bMenInSector == 1 )
				{
					// Kill!
					subject.vitals().health() = 0;
				}
			}
			else
			{
				// OK, see how far we are..
				if ( subject.vitals().health() < (OKLIFE - 3) )
				{
					// Kill!
					subject.vitals().health() = 0;
				}
			}
		}
	}
	else if ( TacticalActorConditions::isZombie(subject) && subject.vitals().health() > 0 && subject.vitals().health() < OKLIFE )
	{
		// a zombie doesn't automatically die, so he would normally stand up again after being hit.
		// We don't want that, because he is dying, so we manually skip that animation
		subject.animationIntent().clearPendingAnimation();
	}

	// add to our records.
	if ( attacker != nullptr && attacker->identity().profile() != NO_PROFILE )
		gMercProfiles[attacker->identity().profile()].records.usDamageDealt += sLifeDeduct;

	if ( subject.identity().profile() != NO_PROFILE )
		gMercProfiles[subject.identity().profile()].records.usDamageTaken += sLifeDeduct;

	if ( fShowDamage )
	{
		subject.combatResult().accumulatedDamage() += sLifeDeduct;
	}

	// Truncate life
	if ( subject.vitals().health() < 0 )
	{
		subject.vitals().health() = 0;
	}

	// Flugente: note we received a fresh wound
	if ( sLifeDeduct > 0 )
		subject.featureFlags().primaryFlags() |= SOLDIER_FRESHWOUND;

	// Flugente we might get a disease from &subject...
	if ( gGameExternalOptions.fDisease && sLifeDeduct > 0 )
	{
		if ( attacker != nullptr && CREATURE_OR_BLOODCAT( attacker ) )
			HandlePossibleInfection( &subject, attacker, INFECTION_TYPE_WOUND_ANIMAL );

		if ( ubReason == TAKE_DAMAGE_TENTACLES )
			HandlePossibleInfection( &subject, NULL, INFECTION_TYPE_WOUND_ANIMAL );

		if ( ubReason == TAKE_DAMAGE_GAS_FIRE )
			HandlePossibleInfection( &subject, NULL, INFECTION_TYPE_WOUND_FIRE );
		else if ( ubReason == TAKE_DAMAGE_GAS_NOTFIRE )
			HandlePossibleInfection( &subject, NULL, INFECTION_TYPE_WOUND_GAS );

		if ( ubReason == TAKE_DAMAGE_GUNFIRE && sLifeDeduct > 20 )
			HandlePossibleInfection( &subject, NULL, INFECTION_TYPE_WOUND_GUNSHOT );
		else if ( ubReason == TAKE_DAMAGE_BLADE || ubReason == TAKE_DAMAGE_HANDTOHAND
			|| ubReason == TAKE_DAMAGE_EXPLOSION || ubReason == TAKE_DAMAGE_STRUCTURE_EXPLOSION || ubReason == TAKE_DAMAGE_TENTACLES )
		{
			FLOAT modifier = 0.5f + sLifeDeduct / 100;
			HandlePossibleInfection( &subject, NULL, INFECTION_TYPE_WOUND_OPEN, modifier );
		}

		// possibly get traumatized if damage gets close to killing us (not if we're slowly bleeding)
		if ( subject.vitals().health() < OKLIFE )//&& ubReason != TAKE_DAMAGE_BLOODLOSS )
			HandlePossibleInfection( &subject, NULL, INFECTION_TYPE_TRAUMATIC );
	}

	// Flugente: bandaging during retreat
	if ( gGameExternalOptions.fAllowBandagingDuringTravel && ubReason == TAKE_DAMAGE_BLOODLOSS && subject.deployment().isBetweenSectors() && GetBestRetreatingMercDoctor( &subject ) != NOBODY )
	{
		SetRetreatBandaging( TRUE );
	}

	// Calculate damage to our items if from an explosion!
	if ( ubReason == TAKE_DAMAGE_EXPLOSION || ubReason == TAKE_DAMAGE_STRUCTURE_EXPLOSION )
	{
		CheckEquipmentForDamage( &subject, sLifeDeduct );
	}

	// Calculate bleeding
	if ( ubReason != TAKE_DAMAGE_GAS_FIRE && ubReason != TAKE_DAMAGE_GAS_NOTFIRE && !AM_A_ROBOT( (&subject) ) )
	{
		if ( ubReason == TAKE_DAMAGE_HANDTOHAND )
		{
			if ( sLifeDeduct > 0 )
			{
				// HTH does 1 pt bleeding per hit
				subject.vitals().bleeding() = subject.vitals().bleeding() + 1;
			}
		}
		else
		{
			// we reduce bleeding only if the new bBandage would be zero
			// by &subject, we can continue bleeding, and eventually bleeding
			if ( sLifeDeduct < 0 )
			{
				INT8 oldBleeding = subject.vitals().bleeding();
				subject.vitals().bleeding() = min( subject.vitals().bleeding(), subject.vitals().maximumHealth() - subject.vitals().health() );
			}
			else
			{
				subject.vitals().bleeding() = subject.vitals().maximumHealth() - (subject.vitals().health() + bBandage);
			}
		}

	}

	//CHRISL: We need &subject to dynamically adjust based on maxAP.  Otherwise sLifeDeduct=16 results in a greater effective penalty
	//	the lower our AP_MAXIMUM value is set to.
	// Deduct breath AND APs!
	//sAPCost = (sLifeDeduct / APBPConstants[AP_GET_WOUNDED_DIVISOR]); // + fallCost;
	sAPCost = (sLifeDeduct / APBPConstants[AP_GET_WOUNDED_DIVISOR]) * APBPConstants[AP_MAXIMUM] / 100;

	// ATE: if the robot, do not deduct
	if ( !AM_A_ROBOT( (&subject) ) )
	{
		DeductPoints( &subject, sAPCost, sBreathLoss, DISABLED_INTERRUPT );
		subject.runtime().combatFeedback.lastActionPointsFromHit += sAPCost;
	}

	ubCombinedLoss = (UINT8)sLifeDeduct / 10 + sBreathLoss / 2000;

	// Add shock
	if ( !AM_A_ROBOT( (&subject) ) )
	{
		subject.suppression().shock() += ubCombinedLoss;
		subject.runtime().combatFeedback.lastShockFromHit += ubCombinedLoss;
	}

	// start the stopwatch - the blood is gushing!
	subject.vitals().nextBleedAt() = CalcSoldierNextBleed( &subject );

	if ( subject.roster().inSector() && subject.awareness().visibility() != -1 )
	{
		// If we are already dead, don't show damage!
		if ( bOldLife != 0 && fShowDamage && sLifeDeduct != 0 && sLifeDeduct < 1000 )
		{
			/*
			// Display damage
			INT16 sOffsetX, sOffsetY;

			// Set Damage display counter
			subject.damageDisplay().displayFlag() = TRUE;
			subject.damageDisplay().counter() = 0;
			if ( subject.ubBodyType == QUEENMONSTER )
			{
			subject.damageDisplay().offsetX() = 0;
			subject.damageDisplay().offsetY() = 0;
			}
			else
			{
			GetSoldierAnimOffsets( &subject, &sOffsetX, &sOffsetY );
			subject.damageDisplay().offsetX() = sOffsetX;
			subject.damageDisplay().offsetY() = sOffsetY;
			}
			*/
			// sevenfm: moved code to function
			SetDamageDisplayCounter( &subject );
			// zero suppression values stored from last attack
			subject.runtime().combatFeedback.lastShock = 0;
			subject.runtime().combatFeedback.lastSuppression = 0;
			subject.runtime().combatFeedback.lastMorale = 0;
			subject.runtime().combatFeedback.lastActionPoints = 0;
			//subject.runtime().combatFeedback.lastBulletImpact = 0;
			//subject.runtime().combatFeedback.lastArmourProtection = 0;
		}
	}

	// it is possible we have to drop the items in our hands
	bool dropiteminmainhand = false;

	// Flugente: disease can stop us from using our arms normally
	if ( gGameExternalOptions.fDisease
		&& gGameExternalOptions.fDiseaseSevereLimitations
		&& TacticalActorDisease::hasOutbreakProperty(subject, DISEASE_PROPERTY_LIMITED_USE_ARMS ) )
	{
		// drop item in main hand if twohanded
		if ( subject.inventory()[HANDPOS].exists() == true && ItemIsTwoHanded( subject.inventory()[HANDPOS].usItem ) )
			dropiteminmainhand = true;

		// we can only use one hand, so drop items in second hand
		if ( subject.inventory()[SECONDHANDPOS].exists() == true )
		{
			// ATE: if our guy, make visible....
			if ( subject.roster().team() == gbPlayerNum )
			{
				subject.awareness().markVisible();
			}
			// If the subject was an enemy.
			// Kaiden Added for UB reveal All items after combat feature!
			else if ( subject.roster().team() == ENEMY_TEAM )
			{
				//add a flag to the item so when all enemies are killed, we can run through and reveal all the enemies items
				usItemFlags |= WORLD_ITEM_DROPPED_FROM_ENEMY;
			}

			if ( UsingNewAttachmentSystem() == true )
				ReduceAttachmentsOnGunForNonPlayerChars( &subject, &( subject.inventory()[SECONDHANDPOS] ) );

			AddItemToPool( subject.position().gridNo(), &( subject.inventory()[SECONDHANDPOS] ), subject.awareness().visibility(), subject.position().level(), usItemFlags, -1 ); //Madd: added usItemFlags to function arguments
			DeleteObj( &( subject.inventory()[SECONDHANDPOS] ) );
		}
	}

	// OK, if here, let's see if we should drop our weapon....
	if (!dropiteminmainhand && ubReason != TAKE_DAMAGE_BLOODLOSS && !(AM_A_ROBOT((&subject))) && !(subject.roster().team() == CIV_TEAM && subject.identity().profile() != NO_PROFILE))
	{
		INT16 sTestOne, sTestTwo, sChanceToDrop;
		INT8	bVisible = -1;

		sTestOne = EffectiveStrength( &subject, FALSE );
		sTestTwo = 2 * max(sLifeDeduct, (sBreathLoss / 100));

		const TacticalActor* lastAttacker =
			GetJa2SoldierRepository().resolve( subject.combatResult().currentAttacker() );
		if (lastAttacker != nullptr && lastAttacker->identity().bodyType() == BLOODCAT)
		{
			// bloodcat boost, let them make people drop items more
			sTestTwo += 20;
		}

		// If damage > effective strength....
		sChanceToDrop = sTestTwo - sTestOne;

		// ATE: Increase odds of NOT dropping an UNDROPPABLE OBJECT
		if ( (subject.inventory()[HANDPOS].fFlags & OBJECT_UNDROPPABLE) )
		{
			sChanceToDrop -= 30;
		}

#ifdef JA2TESTVERSION
		//ScreenMsg( FONT_MCOLOR_LTYELLOW, MSG_TESTVERSION, L"Chance To Drop Weapon: str: %d Dam: %d Chance: %d", sTestOne, sTestTwo, sChanceToDrop );
#endif

		if ((INT16)Random(100) < sChanceToDrop)
		{
			dropiteminmainhand = true;
		}
	}

	if ( dropiteminmainhand )
	{
		// OK, drop item in main hand...
		if ( subject.inventory()[HANDPOS].exists() == true )
		{
			// Flugente: If item has an attached rifle sling, place it the sling position instead
			int bSlot = GUNSLINGPOCKPOS;
			if ( HasAttachmentOfClass( &( subject.inventory()[HANDPOS] ), AC_SLING ) && TryToPlaceInSlot( &subject, &( subject.inventory()[HANDPOS] ), FALSE, bSlot, GUNSLINGPOCKPOS ) )
			{
				;
			}
			else if ( !( subject.inventory()[HANDPOS].fFlags & OBJECT_UNDROPPABLE ) )
			{
				// ATE: if our guy, make visible....
				if ( subject.roster().team() == gbPlayerNum )
				{
					subject.awareness().markVisible();
				}
				// If the subject was an enemy.
				// Kaiden Added for UB reveal All items after combat feature!
				else if ( subject.roster().team() == ENEMY_TEAM )
				{
					//add a flag to the item so when all enemies are killed, we can run through and reveal all the enemies items
					usItemFlags |= WORLD_ITEM_DROPPED_FROM_ENEMY;
				}

				if ( UsingNewAttachmentSystem() == true )
					ReduceAttachmentsOnGunForNonPlayerChars( &subject, &( subject.inventory()[HANDPOS] ) );

				AddItemToPool( subject.position().gridNo(), &( subject.inventory()[HANDPOS] ), subject.awareness().visibility(), subject.position().level(), usItemFlags, -1 ); //Madd: added usItemFlags to function arguments
				DeleteObj( &( subject.inventory()[HANDPOS] ) );
			}
		}
	}

	// Drop some blood!
	// decide blood amt, if any
	ubBlood = (sLifeDeduct / BLOODDIVISOR);
	if ( ubBlood > MAXBLOODQUANTITY )
	{
		ubBlood = MAXBLOODQUANTITY;
	}

	if ( !(subject.status().flags() & (SOLDIER_VEHICLE | SOLDIER_ROBOT)) )
	{
		if ( ubBlood != 0 )
		{
			if ( subject.roster().inSector() )
			{
				DropBlood( &subject, ubBlood, subject.awareness().visibility() );
			}
		}
	}

	//Set UI Flag for unconscious, if it's our own guy!
	if ( subject.roster().team() == gbPlayerNum )
	{
		if ( subject.vitals().health() < OKLIFE && subject.vitals().health() > 0 && bOldLife >= OKLIFE )
		{
			subject.uiPresentation().markUnconscious();
			fInterfacePanelDirty = DIRTYLEVEL2;
		}
	}

	if ( subject.roster().inSector() )
	{
		(void)TacticalActorRecovery::checkBreathCollapse(subject);
	}

	// EXPERIENCE CLASS GAIN (combLoss): Getting wounded in battle

	DirtyMercPanelInterface( &subject, DIRTYLEVEL1 );


	if ( attacker != nullptr )
	{
		// don't give exp for hitting friends!
		if ( (attacker->roster().team() == gbPlayerNum) && (subject.roster().team() != gbPlayerNum) )
		{
			if ( ubReason == TAKE_DAMAGE_EXPLOSION )
			{
				// EXPLOSIVES GAIN (combLoss):  Causing wounds in battle
				StatChange( attacker, EXPLODEAMT, (UINT16)(10 * ubCombinedLoss), FROM_FAILURE );
			}
		}
	}

	// Why &subject? No need for new declaration..
	//TacticalActor *pSoldier = &subject;
	//if (PTR_OURTEAM)
	if ( subject.roster().team() == gbPlayerNum )
	{
		// EXPERIENCE GAIN: Took some damage
		if ( ubReason != TAKE_DAMAGE_BLOODLOSS )
			StatChange( &subject, EXPERAMT, (UINT16)(5 * ubCombinedLoss), FROM_FAILURE );

		// SANDRO - gain some exp towards max health if bleeding
		if ( subject.vitals().maximumHealth() < 100 && ubReason == TAKE_DAMAGE_BLOODLOSS && !(AM_A_ROBOT( (&subject) )) )
		{
			StatChange( &subject, HEALTHAMT, (UINT16)(3 * ubCombinedLoss), FROM_FAILURE );
		}

		// Check for quote
		if ( !subject.dialogue().hasSaid(SOLDIER_QUOTE_SAID_BEING_PUMMELED) )
		{
			// Check attacker!
			if ( ubAttacker != NOBODY && ubAttacker != subject.identity().id() )
			{
				subject.combatResult().hitsThisTurn()++;

				if ( (subject.combatResult().hitsThisTurn() >= 3) && (subject.vitals().health() - subject.vitals().previousHealth() > 20) )
				{
					if ( Random( 100 ) < (UINT16)((40 * (subject.combatResult().hitsThisTurn() - 2))) )
					{
						DelayedTacticalCharacterDialogue( &subject, QUOTE_TAKEN_A_BREATING );
						subject.dialogue().markSaid(SOLDIER_QUOTE_SAID_BEING_PUMMELED);
						subject.combatResult().hitsThisTurn() = 0;
					}
				}
			}
		}
	}

	if ( (attacker != nullptr) && (attacker->roster().team() == OUR_TEAM) && (subject.identity().profile() != NO_PROFILE) && gMercProfiles[subject.identity().profile()].Type == PROFILETYPE_RPC ||
		gMercProfiles[subject.identity().profile()].Type == PROFILETYPE_NPC )
	{
		gMercProfiles[subject.identity().profile()].ubMiscFlags |= PROFILE_MISC_FLAG_WOUNDEDBYPLAYER;
		if ( subject.identity().profile() == 114 )
		{
			SetFactTrue( FACT_PACOS_KILLED );
		}
	}

	HandleTakeDamageDeath( &subject, bOldLife, ubReason );

	// Check if we are < unconscious, and shutup if so! also wipe sight
	if ( subject.vitals().health() < CONSCIOUSNESS )
	{
		ShutupaYoFace( subject.renderBindings().faceIndex() );
	}

	if ( subject.vitals().health() < OKLIFE )
	{
		DecayIndividualOpplist( &subject );
	}

	// If the attacker is Morris in an Unfinished Business campaign and he did
	// not kill the target, defer his follow-up quote.
	if ( GetGameContext().capabilities().isUnfinishedBusiness() &&
		attacker != nullptr &&
		attacker->identity().profile() == MORRIS_UB )
	{
		//if the soldier is hurt, but not dead
		if ( subject.vitals().health() < bOldLife && subject.vitals().health() > 0 )
		{
			//if he hasnt said his quote #1 before
			if ( !attacker->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_THOUGHT_KILLED_YOU) )
			{
				//said a flag so morris can say &subject quote next turn
				gJa25SaveStruct.fMorrisToSayHurtPlayerQuoteNextTurn = TRUE;

				//Remeber who Morris is saying the quote too
				gJa25SaveStruct.ubPlayerMorrisHurt = subject.identity().profile();
			}
		}

		// else if morris is to say the quote, he hasnt said it yet and he just killed the person he WAS going to say it to
		else if ( gJa25SaveStruct.fMorrisToSayHurtPlayerQuoteNextTurn &&
				  gJa25SaveStruct.ubPlayerMorrisHurt == subject.identity().profile() &&
				  subject.vitals().health() <= 0 &&
				  !attacker->dialogue().hasSaidExtended(SOLDIER_QUOTE_SAID_THOUGHT_KILLED_YOU) )
		{
			//said a flag so morris can say &subject quote next turn
			gJa25SaveStruct.fMorrisToSayHurtPlayerQuoteNextTurn = FALSE;

			//Remeber who Morris is saying the quote too
			gJa25SaveStruct.ubPlayerMorrisHurt = NO_PROFILE;
		}
	}
	switch ( ubReason )
	{
	case TAKE_DAMAGE_FALLROOF:
		PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_FALLROOF );
		break;
	case TAKE_DAMAGE_BLOODLOSS:
		PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_BLOODLOSS );
		break;
	case TAKE_DAMAGE_GAS_FIRE:
	case TAKE_DAMAGE_GAS_NOTFIRE:
		PossiblyStartEnemyTaunt( &subject, TAUNT_GOT_HIT_GAS );
		break;
	default:
		break;
	}


	return(ubCombinedLoss);
}
