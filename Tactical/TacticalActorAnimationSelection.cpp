#include "TacticalActorAnimationSelection.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "Debug Control.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "LOS.h"
#include "Overhead Types.h"
#include "Soldier Class.h"
#include "Soldier macros.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorMobility.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorWeaponHandling.h"
#include "TacticalWorldAdapter.h"
#include "Weapons.h"

std::uint16_t TacticalActorAnimationSelection::selectFire(
	TacticalActor& actor,
	std::uint8_t ubHeight)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES)
	{
		return INVALID_ANIMATION;
	}
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

	if (ubHeight != ANIM_STAND &&
		ubHeight != ANIM_CROUCH &&
		ubHeight != ANIM_PRONE)
	{
		return INVALID_ANIMATION;
	}

	if (pSoldier->inventory()[HANDPOS].usItem >=
		MAXITEMS)
	{
		return INVALID_ANIMATION;
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


void TacticalActorAnimationSelection::selectFall(
	TacticalActor& actor)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES)
	{
		return;
	}
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

std::uint16_t TacticalActorAnimationSelection::pickReady(
	TacticalActor& actor,
	bool fEndReady,
	bool fAltWeaponHolding)
{
	TacticalActor* pSoldier = &actor;
	if (pSoldier->identity().bodyType() >=
		TOTALBODYTYPES ||
		pSoldier->animationPlayback().state() >=
		NUMANIMATIONSTATES)
	{
		return INVALID_ANIMATION;
	}
	DebugMsg( TOPIC_JA2, DBG_LEVEL_3, String( "PickSoldierReadyAnimation" ) );

	// Invalid animation if nothing in our hands
	if ( pSoldier->inventory()[HANDPOS].exists( ) == false )
	{
		return(INVALID_ANIMATION);
	}

	if (pSoldier->inventory()[HANDPOS].usItem >=
		MAXITEMS ||
		pSoldier->attackSelection().hand() >=
		pSoldier->inventory().size() ||
		pSoldier->inventory()[
			pSoldier->attackSelection().hand()].usItem >=
			MAXITEMS)
	{
		return INVALID_ANIMATION;
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

bool TacticalActorAnimationSelection::useAlternativeBigMercAnimation(
	const TacticalActor& actor) noexcept
{
	if (actor.identity().bodyType() != BIGMALE)
		return false;
	if (actor.animationPlayback().subFlags() & SUB_ANIM_BIGGUYSHOOT2)
		return true;

	if (actor.status().flags() & SOLDIER_PC)
	{
		return actor.morale().morale() >
			(IsJa2TacticalCombatActive() ? 95 : 65);
	}

	return (actor.roster().soldierClass() == SOLDIER_CLASS_ELITE ||
			actor.roster().soldierClass() == SOLDIER_CLASS_ELITE_MILITIA) &&
		actor.morale().aiMorale() >= MORALE_FEARLESS &&
		actor.statistics().experienceLevel() > 8;
}

std::uint16_t TacticalActorAnimationSelection::
	suspiciousActionPointDuration(std::uint16_t animation) noexcept
{
	switch (animation)
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
	case CUTTING_FENCE:
	case JUMPWINDOWS:
	case LONG_JUMP:
		return 60;

	case THROW_GRENADE_STANCE:
	case LOB_GRENADE_STANCE:
	case THROW_KNIFE:
	case THROW_KNIFE_SP_BM:
	case THROW_ITEM:
	case LOB_ITEM:
	case THROW_ITEM_CROUCHED:
	case DECAPITATE:
	case TAKE_BLOOD_FROM_CORPSE:
	case PLANT_BOMB:
	case USE_REMOTE:
	case STEAL_ITEM:
	case PICK_LOCK:
	case LOCKPICK_CROUCHED:
	case STEAL_ITEM_CROUCHED:
		return 50;

	case PICKUP_ITEM:
	case DROP_ITEM:
		return 30;

	case SHOOT_ROCKET_CROUCHED:
	case SHOOT_ROCKET:
	case HELIDROP:
	case NINJA_SPINKICK:
		return 100;
	default:
		return 0;
	}
}

BOOLEAN DecideAltAnimForBigMerc(TacticalActor* actor)
{
	return actor != nullptr &&
		TacticalActorAnimationSelection::
			useAlternativeBigMercAnimation(*actor)
		? TRUE
		: FALSE;
}

UINT16 GetSuspiciousAnimationAPDuration(UINT16 animation)
{
	return TacticalActorAnimationSelection::
		suspiciousActionPointDuration(animation);
}
