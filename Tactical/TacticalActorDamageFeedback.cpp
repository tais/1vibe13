#include "TacticalActorBattleSounds.h"
#include "TacticalActorDamageFeedback.h"

#include "Animation Control.h"
#include "DynamicDialogue.h"
#include "GameSettings.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Points.h"
#include "SkillCheck.h"
#include "Soldier Ani.h"
#include "Soldier Find.h"
#include "Soldier Functions.h"
#include "Soldier macros.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalActorOrientation.h"
#include "TacticalActorRecovery.h"
#include "TacticalActorStateFlags.h"
#include "Text.h"
#include "Soldier Profile Constants.h"
#include "Soldier Profile.h"
#include "Timer Control.h"
#include "Weapons.h"
#include "faces.h"
#include "gameloop.h"
#include "message.h"
#include "opplist.h"
#include "random.h"
#include "screenids.h"
#include "worldman.h"

#include <cstdint>

namespace
{
constexpr std::uint8_t battleSoundSetCount = 8;

bool hasValidFeedbackState(
	const TacticalActor& actor) noexcept
{
	const std::uint8_t profile =
		actor.identity().profile();
	const std::int32_t faceIndex =
		actor.renderBindings().faceIndex();
	return actor.identity().bodyType() < TOTALBODYTYPES &&
		(profile == NO_PROFILE || profile < NUM_PROFILES) &&
		actor.dialogue().battleSoundSet() <
			battleSoundSetCount &&
		faceIndex >= -1 &&
		faceIndex < NUM_FACE_SLOTS;
}
}

bool TacticalActorDamageFeedback::presentHit(
	TacticalActor& actor)
{
	if (!hasValidFeedbackState(actor))
		return false;

	if ((GetJA2Clock() -
		 actor.vitals().lastBleedGruntAt()) > 1000)
	{
		actor.vitals().lastBleedGruntAt() =
			GetJA2Clock();
		(void)TacticalActorBattleSounds::play(actor, BATTLE_SOUND_HIT1);
	}

	const std::uint32_t currentScreen =
		GetCurrentScreen();
	if ((actor.roster().inSector() &&
		 currentScreen == GAME_SCREEN) ||
		currentScreen != GAME_SCREEN)
	{
		actor.uiPresentation().startPortraitFlash();
		actor.uiPresentation().portraitFlashFrame() =
			FLASH_PORTRAIT_STARTSHADE;
		actor.timing().start(
			SoldierTimingComponent::Timer::PortraitFlash,
			FLASH_PORTRAIT_DELAY);
	}

	return true;
}

std::uint8_t TacticalActorDamageFeedback::calculateScreamVolume(
	TacticalActor& actor,
	std::uint8_t ubCombinedLoss)
{
	TacticalActor* pSoldier = &actor;
	const std::uint8_t profile =
		pSoldier->identity().profile();
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		(profile != NO_PROFILE &&
		 profile >= NUM_PROFILES))
	{
		return 0;
	}
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


void TacticalActorDamageFeedback::applyGenericHit(
	TacticalActor& actor,
	std::uint8_t ubSpecial,
	std::int16_t bDirection)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES ||
		pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		bDirection < 0 ||
		bDirection >= NUM_WORLD_DIRECTIONS)
	{
		return;
	}
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


void TacticalActorDamageFeedback::applyGunfireHit(
	TacticalActor& actor,
	std::uint16_t usWeaponIndex,
	std::int16_t sDamage,
	std::uint16_t bDirection,
	std::uint16_t sRange,
	SoldierID ubAttackerID,
	std::uint8_t ubSpecial,
	std::uint8_t ubHitLocation)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES ||
		usWeaponIndex >= MAXITEMS ||
		bDirection >= NUM_WORLD_DIRECTIONS)
	{
		return;
	}
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

	applyGenericHit( actor, ubSpecial, bDirection );
}

void TacticalActorDamageFeedback::applyExplosionHit(
	TacticalActor& actor,
	std::uint16_t usWeaponIndex,
	std::int16_t sDamage,
	std::uint16_t bDirection,
	std::uint16_t sRange,
	SoldierID ubAttackerID,
	std::uint8_t ubSpecial,
	std::uint8_t ubHitLocation)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES ||
		usWeaponIndex >= MAXITEMS ||
		bDirection >= NUM_WORLD_DIRECTIONS)
	{
		return;
	}
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
		applyGenericHit( actor, 0, bDirection );
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
				applyGenericHit( actor, 0, bDirection );
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
				applyGenericHit( actor, 0, bDirection );
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
			applyGenericHit( actor, 0, bDirection );
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


void TacticalActorDamageFeedback::applyBladeHit(
	TacticalActor& actor,
	std::uint8_t ubHitLocation)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES)
	{
		return;
	}
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


void TacticalActorDamageFeedback::applyPunchHit(
	TacticalActor& actor,
	std::uint16_t usWeaponIndex,
	std::int16_t sDamage,
	std::uint16_t bDirection,
	std::uint16_t sRange,
	SoldierID ubAttackerID,
	std::uint8_t ubSpecial,
	std::uint8_t ubHitLocation)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES)
	{
		return;
	}

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

void TacticalActorDamageFeedback::applyVehicleHit(
	TacticalActor& actor,
	std::uint16_t bDirection)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES ||
		bDirection >= NUM_WORLD_DIRECTIONS)
	{
		return;
	}
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


void TacticalActorDamageFeedback::setDamageDisplayCounter(
	TacticalActor& actor)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES)
	{
		return;
	}
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
