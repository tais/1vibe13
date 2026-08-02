#include "TacticalActorAiBehavior.h"
	#include "TacticalActor.h"
	#include "TacticalActorEmploymentTypes.h"
	#include "TacticalActorPredicates.h"
	#include "TacticalActorSkills.h"
	#include "TacticalActorStateFlags.h"
	#include "Grid Direction.h"
	#include "Soldier Profile Constants.h"
	#include "Soldier Palette.h"
#include "TacticalActorLighting.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorTurnBudget.h"
#include "TacticalActorVisibility.h"
#include "TacticalActorWeaponHandling.h"
#include "TacticalActorMobility.h"
	#include "ai.h"
	#include "TacticalActorConditions.h"
	#include "Weapons.h"
	#include "opplist.h"
	#include "Points.h"
	#include "PATHAI.H"
	#include "worldman.h"
	#include "AIInternals.h"
	#include "Items.h"
	#include "message.h"
	#include "LOS.h"
	#include "Assignments.h"
	#include "Soldier Functions.h"
	#include "Points.h"
	#include "GameSettings.h"
	#include "Buildings.h"
	#include "Soldier macros.h"
	#include "Render Fun.h"
	#include "strategicmap.h"
	#include "environment.h"
	#include "lighting.h"
	#include "Soldier Create.h"
	#include "SkillCheck.h"			// added by SANDRO
	#include "Vehicles.h"			// added by silversurfer
	#include "Game Clock.h"			// sevenfm
	#include "Rotting Corpses.h"	// sevenfm
	#include "WCheck.h"				// sevenfm
	#include "SmokeEffects.h"		// sevenfm

#include "GameInitOptionsScreen.h"
#include "Simulation Commands.h"
#include "SoldierRepository.h"
#include "TacticalEntityHost.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorRadio.h"

//////////////////////////////////////////////////////////////////////////////
// SANDRO - In this file, all APBPConstants[AP_CROUCH] and APBPConstants[AP_PRONE] were changed to GetAPsCrouch() and GetAPsProne()
//			On the bottom here, there are these functions made
//////////////////////////////////////////////////////////////////////

//
// CJC's DG->JA2 conversion notes
//
// Commented out:
//
// InWaterOrGas - gas stuff
// RoamingRange - point patrol stuff

extern SECTOR_EXT_DATA	SectorExternalData[256][4];

UINT8 Urgency[NUM_STATUS_STATES][NUM_MORALE_STATES] =
{
	{URGENCY_LOW,  URGENCY_LOW,  URGENCY_LOW,  URGENCY_LOW,  URGENCY_LOW}, // green
	{URGENCY_HIGH, URGENCY_MED,  URGENCY_MED,  URGENCY_LOW,  URGENCY_LOW}, // yellow
	{URGENCY_HIGH, URGENCY_MED,  URGENCY_MED,  URGENCY_MED,  URGENCY_MED}, // red
	{URGENCY_HIGH, URGENCY_HIGH, URGENCY_HIGH, URGENCY_MED,  URGENCY_MED}  // black
};

UINT16 MovementMode[LAST_MOVEMENT_ACTION + 1][NUM_URGENCY_STATES] =
{
	{WALKING,	 WALKING,  WALKING }, // AI_ACTION_NONE

	{WALKING,  WALKING,  WALKING }, // AI_ACTION_RANDOM_PATROL
	{WALKING,  RUNNING,  RUNNING }, // AI_ACTION_SEEK_FRIEND
	{WALKING,  RUNNING,  RUNNING }, // AI_ACTION_SEEK_OPPONENT
	{RUNNING,  RUNNING,  RUNNING }, // AI_ACTION_TAKE_COVER
	{WALKING,  RUNNING,  RUNNING }, // AI_ACTION_GET_CLOSER

	{WALKING,  WALKING,  WALKING }, // AI_ACTION_POINT_PATROL,
	{WALKING,  RUNNING,  RUNNING }, // AI_ACTION_LEAVE_WATER_GAS,
	{WALKING,  SWATTING,  RUNNING }, // AI_ACTION_SEEK_NOISE,
	{RUNNING,  RUNNING,  RUNNING }, // AI_ACTION_ESCORTED_MOVE,
	{WALKING,  RUNNING,  RUNNING }, // AI_ACTION_RUN_AWAY,

	{RUNNING,  RUNNING,  RUNNING }, // AI_ACTION_KNIFE_MOVE
	{WALKING,  WALKING,  WALKING }, // AI_ACTION_APPROACH_MERC
	{RUNNING,  RUNNING,  RUNNING }, // AI_ACTION_TRACK
	{RUNNING,	 RUNNING,  RUNNING },	// AI_ACTION_EAT 
	{WALKING,	 RUNNING,  RUNNING},	// AI_ACTION_PICKUP_ITEM

	{WALKING,	 WALKING,  WALKING},	// AI_ACTION_SCHEDULE_MOVE
	{WALKING,	 WALKING,  WALKING},	// AI_ACTION_WALK
	{WALKING,	 RUNNING,  RUNNING},	// withdraw
	{RUNNING,	 RUNNING,  SWATTING},	// flank left
	{RUNNING,	 RUNNING,  SWATTING},	// flank right
	{RUNNING,	 RUNNING,  RUNNING},	// AI_ACTION_MOVE_TO_CLIMB
};

INT8 OKToAttack(TacticalActor * pSoldier, int target)
{
	// can't shoot yourself
	if (target == pSoldier->position().gridNo())
		return(NOSHOOT_MYSELF);

	if (WaterTooDeepForAttacks(pSoldier->position().gridNo(), pSoldier->position().level()))
		return(NOSHOOT_WATER);

	// make sure a weapon is in hand (FEB.8 ADDITION: tossable items are also OK)
	if (!WeaponInHand(pSoldier))
	{
		return(NOSHOOT_NOWEAPON);
	}

	// JUST PUT THIS IN ON JULY 13 TO TRY AND FIX OUT-OF-AMMO SITUATIONS

	if ( Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass == IC_GUN)
	{
		if (ItemIsCannon(pSoldier->inventory()[HANDPOS].usItem))
		{
			// look for another tank shell ELSEWHERE IN INVENTORY
			if ( FindLaunchable( pSoldier, pSoldier->inventory()[HANDPOS].usItem ) == NO_SLOT )
			//if ( !ItemHasAttachments( &(pSoldier->inventory()[HANDPOS]) ) )
			{
				return(NOSHOOT_NOLOAD);
			}
		}
		else if (pSoldier->inventory()[HANDPOS][0]->data.gun.ubGunShotsLeft == 0 /*SB*/ ||
			!(pSoldier->inventory()[HANDPOS][0]->data.gun.ubGunState & GS_CARTRIDGE_IN_CHAMBER) ||
			(TacticalActorWeaponHandling::isValidSecondHandShotForReloading(*pSoldier) &&
			(pSoldier->inventory()[SECONDHANDPOS][0]->data.gun.ubGunShotsLeft == 0 ||
			!(pSoldier->inventory()[SECONDHANDPOS][0]->data.gun.ubGunState & GS_CARTRIDGE_IN_CHAMBER))))
		{
			return(NOSHOOT_NOAMMO);
		}
	}
	else if (Item[pSoldier->inventory()[HANDPOS].usItem].usItemClass == IC_LAUNCHER)
	{
		if ( FindLaunchable( pSoldier, pSoldier->inventory()[HANDPOS].usItem ) == NO_SLOT )
		//if ( !ItemHasAttachments( &(pSoldier->inventory()[HANDPOS]) ) )
		{
			return(NOSHOOT_NOLOAD);
		}
	}

	return(TRUE);
}

BOOLEAN ConsiderProne( TacticalActor * pSoldier )
{
	INT32		sOpponentGridNo;
	INT8		bOpponentLevel;
	INT32		iRange;

	if (pSoldier->morale().aiMorale() >= MORALE_NORMAL)
	{
		return( FALSE );
	}
	// We don't want to go prone if there is a nearby enemy
	ClosestKnownOpponent( pSoldier, &sOpponentGridNo, &bOpponentLevel );
	iRange = PythSpacesAway( pSoldier->position().gridNo(), sOpponentGridNo );
	if (iRange > 10)
	{
		return( TRUE );
	}
	else
	{
		return( FALSE );
	}
}

UINT8 StanceChange( TacticalActor * pSoldier, INT16 ubAttackAPCost )
{
	// consider crouching or going prone

	if (PTR_STANDING)
	{
		if (pSoldier->actionPoints().current() - ubAttackAPCost >= GetAPsCrouch(pSoldier, TRUE))
		{
			if ( (pSoldier->actionPoints().current() - ubAttackAPCost >= GetAPsCrouch(pSoldier, TRUE) + GetAPsProne(pSoldier, TRUE)) && IsValidStance( pSoldier, ANIM_PRONE ) && ConsiderProne( pSoldier ) )
			{
				return( ANIM_PRONE );
			}
			else if ( IsValidStance( pSoldier, ANIM_CROUCH ) )
			{
				return( ANIM_CROUCH );
			}
		}
	}
	else if (PTR_CROUCHED)
	{
		if ( (pSoldier->actionPoints().current() - ubAttackAPCost >= GetAPsProne(pSoldier, TRUE)) && IsValidStance( pSoldier, ANIM_PRONE ) && ConsiderProne( pSoldier ) )
		{
			return( ANIM_PRONE );
		}
	}
	return( 0 );
}

UINT8 ShootingStanceChange( TacticalActor * pSoldier, ATTACKTYPE * pAttack, INT8 bDesiredDirection )
{
	// Figure out the best stance for this attack

	// We don't want to go through a lot of complex calculations here,
	// just compare the chance of the bullet hitting if we are
	// standing, crouched, or prone

	UINT16	usRealAnimState, usBestAnimState;
	INT8		bBestStanceDiff=-1;
	INT8		bLoop, bStanceNum, bStanceDiff, bAPsAfterAttack, bCurAimTime, bSetScopeMode;
	UINT32	uiChanceOfDamage, uiBestChanceOfDamage, uiCurrChanceOfDamage;
	UINT32	uiStanceBonus, uiMinimumStanceBonusPerChange = 20 - 3 * pAttack->ubAimTime;
	INT32		iRange;

	bStanceNum = 0;
	uiCurrChanceOfDamage = 0;

	bSetScopeMode = pSoldier->attackSelection().scopeMode();
	pSoldier->attackSelection().scopeMode() = pAttack->bScopeMode;
	bAPsAfterAttack = pSoldier->actionPoints().current() - MinAPsToAttack( pSoldier, pAttack->sTarget, ADDTURNCOST, pAttack->ubAimTime, 1);
	pSoldier->attackSelection().scopeMode() = bSetScopeMode;
	if (bAPsAfterAttack < GetAPsCrouch(pSoldier, TRUE))
	{
		return( 0 );
	}
	// Unfortunately, to get this to work, we have to fake the AI guy's
	// animation state so we get the right height values
	usRealAnimState = pSoldier->animationPlayback().state();
	usBestAnimState = pSoldier->animationPlayback().state();
	uiBestChanceOfDamage = 0;
	iRange = GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo(), pAttack->sTarget );

	switch( gAnimControl[usRealAnimState].ubEndHeight )
	{
		// set a stance number comparable with our loop variable so we can easily compute
		// stance differences and thus AP cost
		case ANIM_STAND:
			bStanceNum = 0;
			break;
		case ANIM_CROUCH:
			bStanceNum = 1;
			break;
		case ANIM_PRONE:
			bStanceNum = 2;
			break;
	}
	for (bLoop = 0; bLoop < 3; bLoop++)
	{
		bStanceDiff = abs( bLoop - bStanceNum );
		if (bStanceDiff == 2 && bAPsAfterAttack < GetAPsCrouch(pSoldier, TRUE) + GetAPsProne(pSoldier, TRUE))
		{
			// can't consider this!
			continue;
		}

		switch( bLoop )
		{
			case 0:
				if ( !TacticalActorMobility::isValidStance(*pSoldier,  bDesiredDirection, ANIM_STAND ) )
				{
					continue;
				}
				pSoldier->animationPlayback().state() = STANDING;
				break;
			case 1:
				if ( !TacticalActorMobility::isValidStance(*pSoldier,  bDesiredDirection, ANIM_CROUCH ) )
				{
					continue;
				}
				pSoldier->animationPlayback().state() = CROUCHING;
				break;
			default:
				if ( !TacticalActorMobility::isValidStance(*pSoldier,  bDesiredDirection, ANIM_PRONE ) )
				{
					continue;
				}
				pSoldier->animationPlayback().state() = PRONE;
				break;
		}

		// Hack:	Assumes the cost to reach the target stance from the current stance is the same as going back.	Probably true.
		bCurAimTime = __min( bAPsAfterAttack - GetAPsToChangeStance( pSoldier, bStanceNum), pAttack->ubAimTime);
		// If can't fire at all from this stance, don't bother with the chances of hitting
		if (bCurAimTime < 0)
		{
			continue;
		}

		uiChanceOfDamage = SoldierToLocationChanceToGetThrough( pSoldier, pAttack->sTarget, pSoldier->targeting().level(), pSoldier->targeting().cubeLevel(), pAttack->ubOpponent ) * CalcChanceToHitGun( pSoldier, pAttack->sTarget, bCurAimTime, AIM_SHOT_TORSO ) / 100;
		if (uiChanceOfDamage > 0)
		{
			uiStanceBonus = 0;
			// artificially augment "chance of damage" to reflect penalty to be shot at various stances
			switch( pSoldier->animationPlayback().state() )
			{
				case CROUCHING:
					if (iRange > POINT_BLANK_RANGE + 10 * (AIM_PENALTY_TARGET_CROUCHED / 3))
					{
						uiStanceBonus = AIM_BONUS_CROUCHING;
					}
					else if (iRange > POINT_BLANK_RANGE)
					{
						// reduce chance to hit with distance to the prone/immersed target
						uiStanceBonus = 3 * ((iRange - POINT_BLANK_RANGE) / CELL_X_SIZE); // penalty -3%/tile
					}
					break;
				case PRONE:
					if (iRange <= MIN_PRONE_RANGE)
					{
						// HATE being prone this close!
						uiChanceOfDamage = 0;
					}
					else //if (iRange > POINT_BLANK_RANGE)
					{
						// reduce chance to hit with distance to the prone/immersed target
						uiStanceBonus = 3 * ((iRange - POINT_BLANK_RANGE) / CELL_X_SIZE); // penalty -3%/tile
					}
					break;
				default:
					break;
			}
			// reduce stance bonus according to how much we have to change stance to get there
			//uiStanceBonus = uiStanceBonus * (4 - bStanceDiff) / 4;
			uiChanceOfDamage += uiStanceBonus;
		}

		if (bStanceDiff == 0)
		{
			uiCurrChanceOfDamage = uiChanceOfDamage;
		}
		if (uiChanceOfDamage > uiBestChanceOfDamage )
		{
			uiBestChanceOfDamage = uiChanceOfDamage;
			usBestAnimState = pSoldier->animationPlayback().state();
			bBestStanceDiff = bStanceDiff;
		}
	}

	pSoldier->animationPlayback().state() = usRealAnimState;

	// return 0 or the best height value to be at
	if (bBestStanceDiff == 0 || ((uiBestChanceOfDamage - uiCurrChanceOfDamage) / bBestStanceDiff) < uiMinimumStanceBonusPerChange)
	{
		// better off not changing our stance!
		return( 0 );
	}
	else
	{
		return( gAnimControl[ usBestAnimState ].ubEndHeight );
	}
}


UINT16 DetermineMovementMode( TacticalActor * pSoldier, INT8 bAction )
{
	if ( TacticalActorMobility::isFastMovement(*pSoldier) )
	{
		return( RUNNING );
	}
	else if ( CREATURE_OR_BLOODCAT( pSoldier ) )
	{
		if (pSoldier->aiBehavior().alertStatus() == STATUS_GREEN)
			return( WALKING );
		else
			return( RUNNING );
	}
	// zombies always run if they know enemy location
	else if (gGameExternalOptions.fAIMovementMode &&
			TacticalActorConditions::isZombie(*pSoldier) &&
			IS_MERC_BODY_TYPE(pSoldier))
	{
		if (!TileIsOutOfBounds(ClosestKnownOpponent(pSoldier, NULL, NULL)))
			return RUNNING;
		else
			return WALKING;
	}
	else if (pSoldier->identity().bodyType() == COW || pSoldier->identity().bodyType() == CROW)
	{
		return( WALKING );
	}
	else
	{
		if ( (pSoldier->aiBehavior().flags() & AI_CAUTIOUS) && (MovementMode[bAction][Urgency[pSoldier->aiBehavior().alertStatus()][pSoldier->morale().aiMorale()]] == RUNNING) )
		{
			return( WALKING );
		}

		// if soldier is already crouched/prone, use SWATTING
		if ((pSoldier->aiBehavior().flags() & AI_CAUTIOUS) &&
			gGameExternalOptions.fAIMovementMode &&
			IS_MERC_BODY_TYPE(pSoldier) &&
			(pSoldier->roster().team() == ENEMY_TEAM || pSoldier->roster().team() == MILITIA_TEAM) &&
			gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight <= ANIM_CROUCH)
		{
			return SWATTING;
		}
		if ((pSoldier->aiBehavior().flags() & AI_CAUTIOUS) && (MovementMode[bAction][Urgency[pSoldier->aiBehavior().alertStatus()][pSoldier->morale().aiMorale()]] == RUNNING))
		{
			return(WALKING);
		}
		else if ( bAction == AI_ACTION_SEEK_NOISE && pSoldier->roster().team() == CIV_TEAM && !IS_MERC_BODY_TYPE( pSoldier ) )
		{
			return( WALKING );
		}
		else if ( (pSoldier->identity().bodyType() == HATKIDCIV || pSoldier->identity().bodyType() == KIDCIV) && (pSoldier->aiBehavior().alertStatus() == STATUS_GREEN) && Random( 10 ) == 0 )
		{
			return( KID_SKIPPING );
		}
		else
		{
			// sevenfm: movement mode tweaks
			if (gGameExternalOptions.fAIMovementMode)
			{
				INT32 sClosestThreat = ClosestKnownOpponent(pSoldier, NULL, NULL);				

				// use walking mode if no enemy known
				if (pSoldier->aiBehavior().alertStatus() < STATUS_RED &&
					TileIsOutOfBounds(sClosestThreat) &&
					!pSoldier->suppression().underFire() &&
					(bAction == AI_ACTION_SEEK_FRIEND || bAction == AI_ACTION_SEEK_NOISE || bAction == AI_ACTION_TAKE_COVER))
				{
					return WALKING;
				}

				// use swatting when blinded
				if (IS_MERC_BODY_TYPE(pSoldier) &&
					pSoldier->perception().isBlinded())
				{
					return SWATTING;
				}

				if (IS_MERC_BODY_TYPE(pSoldier) &&
					pSoldier->aiBehavior().alertStatus() >= STATUS_YELLOW &&
					!InWaterGasOrSmoke(pSoldier, pSoldier->position().gridNo()) &&
					!(pSoldier->status().flags() & SOLDIER_BOXER) &&
					!TileIsOutOfBounds(sClosestThreat) &&
					(pSoldier->roster().team() == ENEMY_TEAM || pSoldier->roster().team() == MILITIA_TEAM))
				{
					INT16 sDistanceVisible = VISION_RANGE;
					INT32 iRCD = RangeChangeDesire(pSoldier);

					// use running when in light at night
					if (NightTime() &&
						InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level()) &&
						(bAction == AI_ACTION_SEEK_OPPONENT ||
						bAction == AI_ACTION_GET_CLOSER ||
						bAction == AI_ACTION_SEEK_FRIEND ||
						bAction == AI_ACTION_TAKE_COVER))
					{
						return RUNNING;
					}

					// use swatting for seeking at night or when soldier is already crouched
					if (!InLightAtNight(pSoldier->position().gridNo(), pSoldier->position().level()) &&
						pSoldier->aiBehavior().alertStatus() == STATUS_RED &&
						iRCD < 4 &&
						!pSoldier->suppression().underFire() &&
						pSoldier->suppression().shock() == 0 &&
						!GuySawEnemy(pSoldier) &&
						(NightTime() || gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight <= ANIM_CROUCH) &&
						CountNearbyFriends(pSoldier, pSoldier->position().gridNo(), TACTICAL_RANGE / 4) < 3 &&
						PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) < 3 * sDistanceVisible / 2 &&
						CountFriendsBlack(pSoldier) == 0 &&
						bAction == AI_ACTION_SEEK_OPPONENT)
					{
						return SWATTING;
					}

					// use swatting for taking cover
					if (pSoldier->aiBehavior().alertStatus() >= STATUS_RED &&
						PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > (INT16)TACTICAL_RANGE / 8 &&
						(pSoldier->suppression().underFire() && iRCD < 4 ||
						pSoldier->suppression().shock() > 2 * iRCD ||
						pSoldier->suppression().shock() > 0 && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE) &&
						!pSoldier->combatResult().lastAttackHit() &&
						bAction == AI_ACTION_TAKE_COVER)
					{
						return SWATTING;
					}

					// use SWATTING when under fire 
					if (pSoldier->aiBehavior().alertStatus() >= STATUS_RED &&
						(pSoldier->suppression().shock() > iRCD && PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > (INT16)TACTICAL_RANGE / 2 ||
						pSoldier->suppression().shock() > 0 && gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE && PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > (INT16)TACTICAL_RANGE / 4) &&
						PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) < 3 * sDistanceVisible / 2 &&
						gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight <= ANIM_CROUCH &&
						!pSoldier->combatResult().lastAttackHit() &&
						(bAction == AI_ACTION_SEEK_OPPONENT ||
						bAction == AI_ACTION_GET_CLOSER ||
						bAction == AI_ACTION_SEEK_FRIEND ||
						bAction == AI_ACTION_TAKE_COVER))
					{
						return SWATTING;
					}

					// use SWATTING when in a room and seen enemy recently or under fire
					if (InARoom(pSoldier->position().gridNo(), NULL) &&
						pSoldier->aiBehavior().alertStatus() >= STATUS_YELLOW &&
						(pSoldier->aiBehavior().orders() == SNIPER ||
						pSoldier->aiBehavior().orders() == STATIONARY ||
						(GuySawEnemy(pSoldier) || pSoldier->suppression().shock() > 0) && iRCD < 4) &&
						PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > (INT16)TACTICAL_RANGE / 4 &&
						(bAction == AI_ACTION_SEEK_OPPONENT ||
						bAction == AI_ACTION_GET_CLOSER ||
						bAction == AI_ACTION_SEEK_FRIEND ||
						bAction == AI_ACTION_TAKE_COVER ||
						bAction == AI_ACTION_SEEK_NOISE))
					{
						return SWATTING;
					}

					// use swatting for snipers on roof or when under fire
					if (pSoldier->position().level() > 0 &&
						pSoldier->aiBehavior().alertStatus() >= STATUS_YELLOW &&
						(pSoldier->aiBehavior().orders() == SNIPER ||
						pSoldier->aiBehavior().orders() == STATIONARY ||
						pSoldier->suppression().shock() > 0 && iRCD < 4) &&
						PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > (INT16)TACTICAL_RANGE / 4 &&
						(bAction == AI_ACTION_SEEK_OPPONENT ||
						bAction == AI_ACTION_GET_CLOSER ||
						bAction == AI_ACTION_SEEK_FRIEND ||
						bAction == AI_ACTION_TAKE_COVER ||
						bAction == AI_ACTION_SEEK_NOISE))
					{
						return SWATTING;
					}

					// use running for taking cover when not under attack
					if (!pSoldier->suppression().underFire() &&
						bAction == AI_ACTION_TAKE_COVER &&
						pSoldier->actionPoints().initial() > APBPConstants[AP_MINIMUM] &&
						(!InARoom(pSoldier->position().gridNo(), NULL) || PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > sDistanceVisible * 2) &&
						pSoldier->morale().aiMorale() >= MORALE_NORMAL &&
						pSoldier->vitals().breath() > 25 &&
						pSoldier->position().level() == 0 &&
						pSoldier->aiBehavior().orders() != STATIONARY &&
						pSoldier->aiBehavior().orders() != SNIPER &&
						(gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight > ANIM_PRONE || pSoldier->actionPoints().current() > APBPConstants[AP_MINIMUM]))
					{
						return RUNNING;
					}

					// decide movement mode when getting closer
					if (bAction == AI_ACTION_GET_CLOSER)
					{
						if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_STAND)
						{
							if (WeaponReady(pSoldier) && !pSoldier->suppression().underFire() && pSoldier->aiBehavior().alertStatus() == STATUS_BLACK)
								return WALKING;
							else
								return RUNNING;
						}
						else if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_CROUCH)
						{
							if (WeaponReady(pSoldier) && !pSoldier->suppression().underFire() && pSoldier->aiBehavior().alertStatus() == STATUS_BLACK ||
								pSoldier->suppression().underFire() && PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > (INT16)TACTICAL_RANGE / 8)
								return SWATTING;
							else
								return RUNNING;
						}
						else if (gAnimControl[pSoldier->animationPlayback().state()].ubEndHeight == ANIM_PRONE)
						{
							if (pSoldier->suppression().underFire() && !pSoldier->combatResult().lastAttackHit() && PythSpacesAway(pSoldier->position().gridNo(), sClosestThreat) > (INT16)TACTICAL_RANGE / 8)
								return SWATTING;
							else
								return RUNNING;
						}
					}

					// use walking/swatting when flanking in realtime
					if (AICheckIsFlanking(pSoldier) && !gfTurnBasedAI)
					{
						if (NightTime())
							return SWATTING;

						if (pSoldier->vitals().breath() < pSoldier->vitals().maximumBreath() / 2)
							return WALKING;
					}
				}
			}

			return( MovementMode[bAction][Urgency[pSoldier->aiBehavior().alertStatus()][pSoldier->morale().aiMorale()]] );
		}
	}
}

SimulationCommandDispatchResult NewDest(
	TacticalActor *pSoldier, INT32 usGridNo)
{
	// sevenfm: always use DetermineMovementMode with new code
	if (gGameExternalOptions.fAIMovementMode)
	{
		pSoldier->movement().mode() = DetermineMovementMode(pSoldier, pSoldier->aiPlanning().action());
		// check for non merc bodytypes
		if ((pSoldier->movement().mode() == SWATTING || pSoldier->movement().mode() == SWATTING_WK) && !IS_MERC_BODY_TYPE(pSoldier))
		{
			pSoldier->movement().mode() = WALKING;
		}
	}
	else
	{
		// ATE: Setting sDestination? Tis does not make sense...
		//pSoldier->pathing().destinationGrid() = usGridNo;
		BOOLEAN fSet = FALSE;

		if (IS_MERC_BODY_TYPE(pSoldier) && pSoldier->aiPlanning().action() == AI_ACTION_TAKE_COVER && (pSoldier->aiBehavior().attitude() == DEFENSIVE || pSoldier->aiBehavior().attitude() == CUNNINGSOLO || pSoldier->aiBehavior().attitude() == CUNNINGAID) && (SoldierDifficultyLevel(pSoldier) >= 2))
		{
			UINT16 usMovementMode;

			// getting real movement anim for someone who is going to take cover, not just considering
			usMovementMode = MovementMode[AI_ACTION_TAKE_COVER][Urgency[pSoldier->aiBehavior().alertStatus()][pSoldier->morale().aiMorale()]];
			if (usMovementMode != SWATTING)
			{
				// really want to look at path, see how far we could get on path while swatting
				if (EnoughPoints(pSoldier, RecalculatePathCost(pSoldier, SWATTING), 0, FALSE) || (pSoldier->aiPlanning().lastAction() == AI_ACTION_TAKE_COVER && pSoldier->movement().mode() == SWATTING))
				{
					pSoldier->movement().mode() = SWATTING;
				}
				else
				{
					pSoldier->movement().mode() = usMovementMode;
				}
			}
			else
			{
				pSoldier->movement().mode() = usMovementMode;
			}
			fSet = TRUE;
		}
		else
		{
			if (pSoldier->roster().team() == ENEMY_TEAM && pSoldier->aiBehavior().alertStatus() == STATUS_RED)
			{
				switch (pSoldier->aiPlanning().action())
				{
				case AI_ACTION_MOVE_TO_CLIMB:
				case AI_ACTION_RUN_AWAY:
					pSoldier->movement().mode() = DetermineMovementMode(pSoldier, pSoldier->aiPlanning().action());
					fSet = TRUE;
					break;
				default:
					if (!fSet)
					{
						pSoldier->movement().mode() = DetermineMovementMode(pSoldier, pSoldier->aiPlanning().action());
						fSet = TRUE;
					}
					break;
				}

			}
			else
			{
				pSoldier->movement().mode() = DetermineMovementMode(pSoldier, pSoldier->aiPlanning().action());
				fSet = TRUE;
			}

			if (pSoldier->movement().mode() == SWATTING && !IS_MERC_BODY_TYPE(pSoldier))
			{
				pSoldier->movement().mode() = WALKING;
			}
		}
	}

	// Request a replacement path here if movement needs to resume.
	// ATE: Using this more versatile version
	// Last parameter says whether to re-start the soldier's animation
	// This should be done if buddy was paused for fNoApstofinishMove...
	return TryDispatchSystemMoveToGridCommand(
		GetJa2TacticalEntityId(*pSoldier),
		usGridNo, pSoldier->movement().mode(),
		pSoldier->movement().reverse() != FALSE,
		pSoldier->movement().outOfActionPoints() != FALSE);
}


BOOLEAN IsActionAffordable(TacticalActor *pSoldier, INT8 bAction)
{
	INT16	bMinPointsNeeded = 0;
	
	//NumMessage("AffordableAction - Guy#",pSoldier->identity().id());

	if( bAction == AI_ACTION_NONE )
	{
		bAction = pSoldier->aiPlanning().action();
	}

	switch (bAction)
	{
		case AI_ACTION_NONE:                  // maintain current position & facing
			// no cost for doing nothing!
			break;

		case AI_ACTION_CHANGE_FACING:         // turn to face another direction
			bMinPointsNeeded = (INT8) GetAPsToLook( pSoldier );
			break;

		case AI_ACTION_RANDOM_PATROL:         // move towards a particular location
		case AI_ACTION_SEEK_FRIEND:           // move towards friend in trouble
		case AI_ACTION_SEEK_OPPONENT:         // move towards a reported opponent
		case AI_ACTION_TAKE_COVER:            // run for nearest cover from threat
		case AI_ACTION_GET_CLOSER:            // move closer to a strategic location
		case AI_ACTION_POINT_PATROL:          // move towards next patrol point
		case AI_ACTION_LEAVE_WATER_GAS:       // seek nearest spot of ungassed land
		case AI_ACTION_SEEK_NOISE:            // seek most important noise heard
		case AI_ACTION_ESCORTED_MOVE:         // go where told to by escortPlayer
		case AI_ACTION_RUN_AWAY:              // run away from nearby opponent(s)
		case AI_ACTION_APPROACH_MERC:
		case AI_ACTION_TRACK:
		case AI_ACTION_EAT:
		case AI_ACTION_SCHEDULE_MOVE:
		case AI_ACTION_WALK:
		case AI_ACTION_MOVE_TO_CLIMB:
			// for movement, must have enough APs to move at least 1 tile's worth
			bMinPointsNeeded = MinPtsToMove(pSoldier);
			break;

		case AI_ACTION_PICKUP_ITEM:           // grab things lying on the ground
			bMinPointsNeeded = __max( MinPtsToMove( pSoldier ), GetBasicAPsToPickupItem( pSoldier ) ); // SANDRO
			break;

		case AI_ACTION_OPEN_OR_CLOSE_DOOR:
		case AI_ACTION_UNLOCK_DOOR:
		case AI_ACTION_LOCK_DOOR:
			bMinPointsNeeded = MinPtsToMove(pSoldier);
			break;

		case AI_ACTION_DROP_ITEM:
			bMinPointsNeeded = GetBasicAPsToPickupItem( pSoldier ); // SANDRO
			break;

		case AI_ACTION_FIRE_GUN:              // shoot at nearby opponent
		case AI_ACTION_TOSS_PROJECTILE:       // throw grenade at/near opponent(s)
		case AI_ACTION_KNIFE_MOVE:            // preparing to stab adjacent opponent
		case AI_ACTION_THROW_KNIFE:
			// only FIRE_GUN currently actually pays extra turning costs!
			bMinPointsNeeded = MinAPsToAttack(pSoldier,pSoldier->aiPlanning().actionData(),ADDTURNCOST,pSoldier->aiPlanning().aimTime());

#ifdef BETAVERSION
			if (ptsNeeded > pSoldier->actionPoints().current())
			{
			/*
				sprintf(tempstr,"AI ERROR: %s has insufficient points for attack action %d at grid %d",
							pSoldier->identity().name(),pSoldier->aiPlanning().action(),pSoldier->aiPlanning().actionData());
				PopMessage(tempstr);
				*/
			}
#endif
			break;

		case AI_ACTION_PULL_TRIGGER:          // activate an adjacent panic trigger
			bMinPointsNeeded = APBPConstants[AP_PULL_TRIGGER];
			break;

		case AI_ACTION_USE_DETONATOR:         // grab detonator and set off bomb(s)
			bMinPointsNeeded = APBPConstants[AP_USE_REMOTE];
			break;

		case AI_ACTION_YELLOW_ALERT:          // tell friends opponent(s) heard
		case AI_ACTION_RED_ALERT:             // tell friends opponent(s) seen
		case AI_ACTION_CREATURE_CALL:				 // for now
			bMinPointsNeeded = APBPConstants[AP_RADIO];
			break;

		case AI_ACTION_CHANGE_STANCE:                // crouch
			bMinPointsNeeded = GetAPsCrouch(pSoldier, TRUE);
			break;

		case AI_ACTION_GIVE_AID:              // help injured/dying friend
			bMinPointsNeeded = 0;
			break;

		case AI_ACTION_CLIMB_ROOF:
			{
				INT8 bAPForStandUp = 0;
				INT8 bAPToLookAtWall = (FindDirectionForClimbing( pSoldier, pSoldier->position().gridNo(), pSoldier->position().level() ) == pSoldier->position().direction()) ? 0 : GetAPsToLook( pSoldier );

				// SANDRO - improved this a bit
				if (pSoldier->position().level() == 0)
				{
					if( PTR_CROUCHED ) bAPForStandUp = (INT8)(GetAPsCrouch(pSoldier, TRUE));
					else if( PTR_PRONE ) bAPForStandUp = GetAPsCrouch(pSoldier, TRUE) + GetAPsProne(pSoldier, TRUE);
					bMinPointsNeeded = GetAPsToClimbRoof( pSoldier, FALSE ) + bAPForStandUp + bAPToLookAtWall;
				}
				else
				{
					if( !PTR_CROUCHED ) bAPForStandUp = (INT8)(GetAPsCrouch(pSoldier, TRUE));
					bMinPointsNeeded = GetAPsToClimbRoof( pSoldier, TRUE ) + bAPForStandUp + bAPToLookAtWall;
				}
			}
			break;

		case AI_ACTION_COWER:
		case AI_ACTION_STOP_COWERING:
		case AI_ACTION_LOWER_GUN:
		case AI_ACTION_END_COWER_AND_MOVE:
		case AI_ACTION_TRAVERSE_DOWN:
		case AI_ACTION_OFFER_SURRENDER:
			bMinPointsNeeded = 0;
			break;

		case AI_ACTION_STEAL_MOVE: // added by SANDRO
			//bMinPointsNeeded = GetAPsToStealItem( pSoldier, NULL, pSoldier->aiPlanning().actionData() );;
			break;

		case AI_ACTION_JUMP_WINDOW:
			if (!TacticalActorMobility::canClimbWithCurrentBackpack(*pSoldier))
				bMinPointsNeeded = GetAPsToJumpThroughWindows( pSoldier, TRUE );
			else
				bMinPointsNeeded = GetAPsToJumpFence( pSoldier, FALSE );

			break;

		case AI_ACTION_FREE_PRISONER:
			bMinPointsNeeded = APBPConstants[AP_HANDCUFF];
			break;

		case AI_ACTION_USE_SKILL:
			bMinPointsNeeded = 10;	// TODO
			break;

		case AI_ACTION_DOCTOR:
		case AI_ACTION_DOCTOR_SELF:
			bMinPointsNeeded = 20;	// TODO
			break;

		case AI_ACTION_SELFDETONATE:
			bMinPointsNeeded = 20;	// TODO
			break;

		default:
#ifdef BETAVERSION
			//NumMessage("AffordableAction - Illegal action type = ",pSoldier->aiPlanning().action());
#endif
			break;
	}

	// check whether or not we can afford to do this action
	if (bMinPointsNeeded > pSoldier->actionPoints().current())
	{
		return(FALSE);
	}
	else
	{
		return(TRUE);
	}
}

INT16 RandomFriendWithin(TacticalActor *pSoldier)
{
	UINT32				uiLoop;
	UINT16				usMaxDist;
	UINT16				ubFriendCount;
	SoldierID			ubFriendIDs[MAXMERCS], ubFriendID;
	UINT8				ubDirection;
	UINT8					ubDirsLeft;
	BOOLEAN				fDirChecked[8];
	BOOLEAN				fFound = FALSE;
	INT32				usDest, usOrigin;
	TacticalActor *	pFriend;


	// obtain maximum roaming distance from soldier's origin
	usMaxDist = RoamingRange(pSoldier,&usOrigin);

	// if range is restricted, make sure origin is a legal gridno!
	if (usOrigin < 0 || usOrigin >= GRIDSIZE)
	{
#ifdef BETAVERSION
		NameMessage(pSoldier,"has illegal origin, but his roaming range is restricted!",1000);
#endif
		return(FALSE);
	}

	ubFriendCount = 0;

	// build a list of the guynums of all active, eligible friendly mercs

	// go through each soldier, looking for "friends" (soldiers on same side)
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pFriend = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, not in sector, or dead
		if (!pFriend)
		{
			continue;
		}

		// skip ourselves
		if (pFriend->identity().id() == pSoldier->identity().id())
		{
			continue;
		}

		// if this man not neutral, but is on my side, OR if he is neutral, but
		// so am I, then he's a "friend" for the purposes of random visitations
		if ((!pFriend->aiBehavior().neutral() && (pSoldier->roster().side() == pFriend->roster().side())) ||
			(pFriend->aiBehavior().neutral() && pSoldier->aiBehavior().neutral()))
		{
			// if we're not already neighbors
			if (SpacesAway(pSoldier->position().gridNo(),pFriend->position().gridNo()) > 1)
			{
		// remember his guynum, increment friend counter
			ubFriendIDs[ubFriendCount++] = pFriend->identity().id();
			}
		}
	}

	while (ubFriendCount && !fFound)
	{
		// randomly select one of the remaining friends in the list
		ubFriendID = ubFriendIDs[PreRandom(ubFriendCount)];
		TacticalActor* selectedFriend =
			GetJa2SoldierRepository().resolve(ubFriendID.i);

		// if our movement range is NOT restricted, or this friend's within range
		// use distance - 1, because there must be at least 1 tile 1 space closer
		if (selectedFriend &&
			SpacesAway(usOrigin, selectedFriend->position().gridNo()) - 1 <= usMaxDist)
		{
			// should be close enough, try to find a legal destination within 1 tile

			// clear dirChecked flag for all 8 directions
			for ( ubDirection = 0; ubDirection < NUM_WORLD_DIRECTIONS; ++ubDirection )
			{
				fDirChecked[ubDirection] = FALSE;
			}

			ubDirsLeft = NUM_WORLD_DIRECTIONS;

			// examine all 8 spots around 'ubFriendID'
			// keep looking while directions remain and a satisfactory one not found
			while ((ubDirsLeft--) && !fFound)
			{
				// randomly select a direction which hasn't been 'checked' yet
				do
				{
					ubDirection = (UINT8)Random( NUM_WORLD_DIRECTIONS );
				}
				while (fDirChecked[ubDirection]);

				fDirChecked[ubDirection] = TRUE;

				// determine the gridno 1 tile away from current friend in this direction
				usDest = NewGridNo(selectedFriend->position().gridNo(), DirectionInc(ubDirection));

				// if that's out of bounds, ignore it & check next direction
				if (usDest == selectedFriend->position().gridNo())
				{
					continue;
				}

				if (SpacesAway(usOrigin, usDest) > usMaxDist)
				{
					continue;
				}

				if (!CheckNPCDestination(pSoldier, usDest))
				{
					continue;
				}

				if (LegalNPCDestination(pSoldier, usDest, ENSURE_PATH, NOWATER, 0))
				{
					fFound = TRUE;			// found a spot
					pSoldier->aiPlanning().actionData() = usDest;	// store this destination
					pSoldier->pathing().stored() = TRUE;	// optimization - Ian
					break;					// stop checking in other directions
				}
			}
		}

		if (!fFound)
		{
			--ubFriendCount;

			// if we hadn't already picked the last friend currently in the list
			if (ubFriendCount != ubFriendID)
			{
				ubFriendIDs[ubFriendID] = ubFriendIDs[ubFriendCount];
			}
		}
	}

	return(fFound);
}


INT32 RandDestWithinRange(TacticalActor *pSoldier)
{
	INT32 sRandDest = NOWHERE;
	INT32 usOrigin, usMaxDist;
	UINT8	ubTriesLeft;
	BOOLEAN fLimited = FALSE, fFound = FALSE;
	INT16 sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXRange, sYRange, sXOffset, sYOffset;
	INT16 sOrigX, sOrigY;
	INT16 sX, sY;
	//DBrot: More Rooms
	//UINT8	ubRoom = 0, ubTempRoom;
	UINT16 usRoom = 0, usTempRoom;
	sOrigX = sOrigY = -1;
	sMaxLeft = sMaxRight = sMaxUp = sMaxDown = sXRange = sYRange = -1;

	// Try to find a random destination that's no more than maxDist away from
	// the given gridno of origin

	if (gfTurnBasedAI)
	{
		ubTriesLeft = 10;
	}
	else
	{
		ubTriesLeft = 1;
	}

	usMaxDist = RoamingRange(pSoldier,&usOrigin);

	if ( pSoldier->aiBehavior().orders() <= CLOSEPATROL && (pSoldier->roster().team() == CIV_TEAM || pSoldier->identity().profile() != NO_PROFILE ) )
	{
		// any other combo uses the default of ubRoom == 0, set above
		if ( !InARoom( pSoldier->aiPlanning().patrolGrid()[0], &usRoom ) )
		{
			usRoom = 0;
		}
	}

	// if the maxDist is truly a restriction
	if (usMaxDist < (MAXCOL - 1))
	{
		fLimited = TRUE;

		// determine maximum horizontal limits
		sOrigX = usOrigin % MAXCOL;
		sOrigY = usOrigin / MAXCOL;

		sMaxLeft	= min(usMaxDist, sOrigX);
		sMaxRight = min(usMaxDist,MAXCOL - (sOrigX + 1));

		// determine maximum vertical limits
		sMaxUp	= min(usMaxDist, sOrigY);
		sMaxDown = min(usMaxDist,MAXROW - (sOrigY + 1));

		sXRange = sMaxLeft + sMaxRight + 1;
		sYRange = sMaxUp + sMaxDown + 1;
	}

	if (pSoldier->identity().bodyType() == LARVAE_MONSTER)
	{
		// only crawl 1 tile, within our roaming range
		while ((ubTriesLeft--) && !fFound)
		{
			sXOffset = (INT16) Random( 3 ) - 1; // generates -1 to +1
			sYOffset = (INT16) Random( 3 ) - 1;

			if (fLimited)
			{
				sX = pSoldier->position().gridNo() % MAXCOL + sXOffset;
				sY = pSoldier->position().gridNo() / MAXCOL + sYOffset;
				if (sX < sOrigX - sMaxLeft || sX > sOrigX + sMaxRight)
				{
					continue;
				}
				if (sY < sOrigY - sMaxUp || sY > sOrigY + sMaxDown)
				{
					continue;
				}
				sRandDest = usOrigin + sXOffset + (MAXCOL * sYOffset);
			}
			else
			{
				sRandDest = usOrigin + sXOffset + (MAXCOL * sYOffset);
			}

			if (!LegalNPCDestination(pSoldier,sRandDest,ENSURE_PATH,NOWATER,0))
			{
				sRandDest = NOWHERE;
				continue;					// try again!
			}

			// passed all the tests; the destination is acceptable
			fFound = TRUE;
			pSoldier->pathing().stored() = TRUE;	// optimization - Ian
		}
	}
	else
	{
		// keep rolling random destinations until one is satisfactory or retries are used
		while ((ubTriesLeft--) && !fFound)
		{
			if (fLimited)
			{
				UINT8 ubTriesLeft2 = 128;
				do//dnl ch53 111009 This loop should increase search performance, but probably need some counter to prevent eventual endless loop
				{
					sXOffset = ((INT16)Random(sXRange)) - sMaxLeft;
					sYOffset = ((INT16)Random(sYRange)) - sMaxUp;
					sRandDest = usOrigin + sXOffset + (MAXCOL * sYOffset);
				}while(!GridNoOnVisibleWorldTile(sRandDest) && --ubTriesLeft2);
	#ifdef BETAVERSION
				if ((sRandDest < 0) || (sRandDest >= GRIDSIZE))
				{
					NumMessage("RandDestWithinRange: ERROR - Gridno out of range! = ",sRandDest);
					sRandDest = random(GRIDSIZE);
				}
	#endif
			}
			else
			{
				UINT8 ubTriesLeft2 = 128;
				do//dnl ch53 111009 This loop should increase search performance, but probably need some counter to prevent eventual endless loop
				{
					sRandDest = PreRandom(GRIDSIZE);
				}while(!GridNoOnVisibleWorldTile(sRandDest) && --ubTriesLeft2);
			}

			if ( usRoom && InARoom( sRandDest, &usTempRoom ) && usTempRoom != usRoom )
			{
				// outside of room available for patrol!
				sRandDest = NOWHERE;
				continue;
			}

			if (!CheckNPCDestination(pSoldier, sRandDest))
			{
				sRandDest = NOWHERE;
				continue;
			}

			if (!LegalNPCDestination(pSoldier,sRandDest,ENSURE_PATH,NOWATER,0))
			{
				sRandDest = NOWHERE;
				continue;					// try again!
			}

			// passed all the tests; the destination is acceptable
			fFound = TRUE;
			pSoldier->pathing().stored() = TRUE;	// optimization - Ian
		}
	}

	return(sRandDest); // defaults to NOWHERE
}

INT32 ClosestReachableDisturbance(TacticalActor *pSoldier, BOOLEAN * pfChangeLevel )
{
	INT32		*psLastLoc, *pusNoiseGridNo;
	INT8		*pbLastLevel;
	INT32		sGridNo=-1;
	INT8		bLevel, bClosestLevel = -1;
	BOOLEAN		fClimbingNecessary, fClosestClimbingNecessary = FALSE;
	INT32		iPathCost;
	INT32		sClosestDisturbance = NOWHERE;
	UINT32		uiLoop;
	UINT32		closestConscious = NOWHERE,closestUnconscious = NOWHERE;
	INT32		iShortestPath = 1000;
	INT32		iShortestPathConscious = 1000,iShortestPathUnconscious = 1000;
	UINT8		*pubNoiseVolume;
	INT8		*pbNoiseLevel;
	INT8		*pbPersOL,*pbPublOL;
	INT32		sClimbGridNo;
	TacticalActor *pOpponent;
	TacticalActor	*pClosestOpponent = NULL;
	INT32		sDistToEnemy, sDistToClosestEnemy = 10000;

	// sevenfm: safety check
	if ( pfChangeLevel )
	{
		*pfChangeLevel = FALSE;
	}

	pubNoiseVolume = &gubPublicNoiseVolume[pSoldier->roster().team()];
	pusNoiseGridNo = &gsPublicNoiseGridNo[pSoldier->roster().team()];
	pbNoiseLevel = &gbPublicNoiseLevel[pSoldier->roster().team()];

	// hang pointers at start of this guy's personal and public opponent opplists
//	pbPersOL = &pSoldier->awareness().opponentKnowledge()[0];
//	pbPublOL = &(gbPublicOpplist[pSoldier->roster().team()][0]);
//	psLastLoc = &(gsLastKnownOppLoc[pSoldier->identity().id()][0]);

	// look through this man's personal & public opplists for opponents known
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, or dead
		if (!pOpponent)
		{
			continue;			// next merc
		}

		// if this merc is neutral/on same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()) )
		{
			continue;			// next merc
		}

		// silversurfer: ignore empty vehicles
		if ( pOpponent->employment().mercenaryType() == MERC_TYPE__VEHICLE && GetNumberInVehicle( pOpponent->vehicleState().tacticalVehicleId() ) == 0 )
		{
			continue;
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();
		pbPublOL = gbPublicOpplist[pSoldier->roster().team()] + pOpponent->identity().id();
		psLastLoc = gsLastKnownOppLoc[pSoldier->identity().id()] + pOpponent->identity().id();
		pbLastLevel = gbLastKnownOppLevel[pSoldier->identity().id()] + pOpponent->identity().id();

		// if this opponent is unknown personally and publicly
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			continue;			// next merc
		}

		// this is possible if get here from BLACK AI in one of those rare
		// instances when we couldn't get a meaningful shot off at a guy in sight
		/*if ((*pbPersOL == SEEN_CURRENTLY) && (pOpponent->vitals().health() >= OKLIFE))
		{
			// don't allow this to return any valid values, this guy remains a
			// serious threat and the last thing we want to do is approach him!
			return(NOWHERE);
		}*/

		// if personal knowledge is more up to date or at least equal
		if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) ||
			(*pbPersOL == *pbPublOL))
		{
			// using personal knowledge, obtain opponent's "best guess" gridno
			sGridNo = *psLastLoc;
			bLevel = *pbLastLevel;
		}
		else
		{
			// using public knowledge, obtain opponent's "best guess" gridno
			sGridNo = gsPublicLastKnownOppLoc[pSoldier->roster().team()][pOpponent->identity().id()];
			bLevel = gbPublicLastKnownOppLevel[pSoldier->roster().team()][pOpponent->identity().id()];
		}

		// if we are standing at that gridno (!, obviously our info is old...)
		if (sGridNo == pSoldier->position().gridNo())
		{
			continue;			// next merc
		}
		
		if (TileIsOutOfBounds(sGridNo))
		{
			// huh?
			continue;
		}

		// sevenfm: if soldier is zombie and he cannot climb, skip location
		if (TacticalActorConditions::isZombie(*pSoldier) && pSoldier->position().level() != bLevel && !gGameExternalOptions.fZombieCanClimb)
		{
			continue;
		}

		// sevenfm: zombies do not attack vehicles (rftr: or robots)
		if (TacticalActorConditions::isZombie(*pSoldier) && (AM_A_ROBOT(pOpponent) || ENEMYROBOT(pOpponent) || ARMED_VEHICLE(pOpponent) || (pOpponent->status().flags() & SOLDIER_VEHICLE)))
		{
			continue;
		}

		// sevenfm: when in deep water, skip opponents in deep water
		if (DeepWater(pSoldier->position().gridNo(), pSoldier->position().level()) && DeepWater(sGridNo, bLevel))
		{
			continue;
		}

		// sevenfm: if we found reachable enemy, check other enemies only if they are closer
		sDistToEnemy = PythSpacesAway( pSoldier->position().gridNo(), sGridNo );
		if (sDistToEnemy < sDistToClosestEnemy || TileIsOutOfBounds(sClosestDisturbance) )
		{
			iPathCost = EstimatePathCostToLocation( pSoldier, sGridNo, bLevel, FALSE, &fClimbingNecessary, &sClimbGridNo );

			// if we can get there and it's first reachable enemy or closer than other known enemies
			if (iPathCost != 0 && 
				(!pClosestOpponent || pClosestOpponent->vitals().health() < OKLIFE || pOpponent->vitals().health() >= OKLIFE) &&
				(TileIsOutOfBounds(sClosestDisturbance) || 
				iPathCost < iShortestPath ||
				pClosestOpponent && !TacticalActorConditions::isZombie(*pClosestOpponent) && pClosestOpponent->vitals().health() < OKLIFE && pOpponent->vitals().health() >= OKLIFE))
			{
				if (fClimbingNecessary)
				{
					sClosestDisturbance = sClimbGridNo;
				}
				else
				{
					sClosestDisturbance = sGridNo;
				}

				pClosestOpponent = pOpponent;
				sDistToClosestEnemy = sDistToEnemy;
				iShortestPath = iPathCost;
				fClosestClimbingNecessary = fClimbingNecessary;
			}
		}
	}
	
	// if any "misc. noise" was also heard recently	
	if (!TileIsOutOfBounds(pSoldier->perception().noiseGrid()) && pSoldier->perception().noiseGrid() != sClosestDisturbance)
	{
		// test this gridno, too
		sGridNo = pSoldier->perception().noiseGrid();
		bLevel = pSoldier->perception().heardNoiseLevel();

		// if we are there (at the noise gridno)
		if (sGridNo == pSoldier->position().gridNo())
		{
			for(uiLoop=0; uiLoop<Ja2ActiveTacticalActorSlotCount(); uiLoop++)//dnl ch58 160813
			{
				pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);
				if(pOpponent && pSoldier->roster().side() == pOpponent->roster().side() && pSoldier->identity().id() != pOpponent->identity().id()&& pSoldier->perception().noiseGrid() == pOpponent->perception().noiseGrid())
				{
					pOpponent->perception().noiseGrid() = NOWHERE;// Erase for all from the same team as it not useful anymore, this will avoid others to check already tested location
					pOpponent->perception().noiseVolume() = 0;
				}
			}
			pSoldier->perception().noiseGrid() = NOWHERE;		// wipe it out, not useful anymore
			pSoldier->perception().noiseVolume() = 0;
		}
		else
		{
			// get the AP cost to get to the location of the noise
			iPathCost = EstimatePathCostToLocation( pSoldier, sGridNo, bLevel, FALSE, &fClimbingNecessary, &sClimbGridNo );
			// if we can get there
			// sevenfm: only if we don't know enemy location or noise source is close and we have not seen enemy recently
			if (iPathCost != 0 &&
				!AICheckIsFlanking(pSoldier) &&
				(TileIsOutOfBounds( sClosestDisturbance ) || iPathCost < iShortestPath && !GuySawEnemy( pSoldier )) )
			{
				if (fClimbingNecessary)
				{
					sClosestDisturbance = sClimbGridNo;
				}
				else
				{
					sClosestDisturbance = sGridNo;
				}
				iShortestPath = iPathCost;
				fClosestClimbingNecessary = fClimbingNecessary;
			}
		}
	}


	// if any PUBLIC "misc. noise" was also heard recently	
	if (!TileIsOutOfBounds(*pusNoiseGridNo) && *pusNoiseGridNo != sClosestDisturbance )
	{
		// test this gridno, too
		sGridNo = *pusNoiseGridNo;
		bLevel = *pbNoiseLevel;

		// if we are not NEAR the noise gridno...
		if ( pSoldier->position().level() != bLevel || PythSpacesAway( pSoldier->position().gridNo(), sGridNo ) >= 6 || SoldierTo3DLocationLineOfSightTest( pSoldier, sGridNo, bLevel, 0, FALSE, NO_DISTANCE_LIMIT ) == 0 )
		// if we are NOT there (at the noise gridno)
		//	if (sGridNo != pSoldier->sGridNo)
		{
			// get the AP cost to get to the location of the noise
			iPathCost = EstimatePathCostToLocation( pSoldier, sGridNo, bLevel, FALSE, &fClimbingNecessary, &sClimbGridNo );
			// if we can get there
			// sevenfm: only if we don't know enemy location or noise source is close and we have not seen enemy recently
			if (iPathCost != 0 &&
				!AICheckIsFlanking(pSoldier) &&
				(TileIsOutOfBounds(sClosestDisturbance) || iPathCost < iShortestPath && !GuySawEnemy(pSoldier)))
			{
				if (fClimbingNecessary)
				{
					sClosestDisturbance = sClimbGridNo;
				}
				else
				{
					sClosestDisturbance = sGridNo;
				}
				iShortestPath = iPathCost;
				fClosestClimbingNecessary = fClimbingNecessary;
			}
		}
		else
		{
			// degrade our public noise a bit
			//dnl ch58 160813
			//*pusNoiseGridNo -= 2;
			if(*pubNoiseVolume > 1)
				(*pubNoiseVolume)--;
		}
	}

#ifdef DEBUGDECISIONS	
	if (!TileIsOutOfBounds(sClosestDisturbance))
	{
		AINumMessage("CLOSEST DISTURBANCE is at gridno ",sClosestDisturbance);
	}
#endif

	// sevenfm: safety check
	if ( pfChangeLevel )
	{
		*pfChangeLevel = fClosestClimbingNecessary;
	}

	return(sClosestDisturbance);
}


INT32 ClosestKnownOpponent(TacticalActor *pSoldier, INT32 * psGridNo, INT8 * pbLevel, SoldierID * pubOpponentID)
{
	INT32 *psLastLoc,sGridNo, sClosestOpponent = NOWHERE;
	UINT32 uiLoop;
	INT32 iRange, iClosestRange = 1500;
	INT8	*pbPersOL,*pbPublOL;
	INT8	bLevel, bClosestLevel;
	TacticalActor *pOpponent;
	TacticalActor *pClosestOpponent = NULL;

	bClosestLevel = -1;

	if (pubOpponentID)
	{
		*pubOpponentID = NOBODY;
	}

	// NOTE: THIS FUNCTION ALLOWS RETURN OF UNCONSCIOUS AND UNREACHABLE OPPONENTS
	psLastLoc = &(gsLastKnownOppLoc[pSoldier->identity().id()][0]);

	// hang pointers at start of this guy's personal and public opponent opplists
	pbPersOL = &pSoldier->awareness().opponentKnowledge()[0];
	pbPublOL = &(gbPublicOpplist[pSoldier->roster().team()][0]);

	// look through this man's personal & public opplists for opponents known
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, or dead
		if (!pOpponent)
		{
			continue;			// next merc
		}

		// if this merc is neutral/on same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()))
		{
			continue;			// next merc
		}

		// silversurfer: ignore empty vehicles
		if ( pOpponent->employment().mercenaryType() == MERC_TYPE__VEHICLE && GetNumberInVehicle( pOpponent->vehicleState().tacticalVehicleId() ) == 0 )
			continue;

		// Special stuff for Carmen the bounty hunter
		if (pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY && pOpponent->identity().profile() != SLAY)
		{
			continue;	// next opponent
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();
		pbPublOL = gbPublicOpplist[pSoldier->roster().team()] + pOpponent->identity().id();
		psLastLoc = gsLastKnownOppLoc[pSoldier->identity().id()] + pOpponent->identity().id();

		// if this opponent is unknown personally and publicly
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			continue;			// next merc
		}

		// if personal knowledge is more up to date or at least equal
		if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) ||
			(*pbPersOL == *pbPublOL))
		{
			// using personal knowledge, obtain opponent's "best guess" gridno
			sGridNo = gsLastKnownOppLoc[pSoldier->identity().id()][pOpponent->identity().id()];
			bLevel = gbLastKnownOppLevel[pSoldier->identity().id()][pOpponent->identity().id()];
		}
		else
		{
			// using public knowledge, obtain opponent's "best guess" gridno
			sGridNo = gsPublicLastKnownOppLoc[pSoldier->roster().team()][pOpponent->identity().id()];
			bLevel = gbPublicLastKnownOppLevel[pSoldier->roster().team()][pOpponent->identity().id()];
		}

		// if we are standing at that gridno(!, obviously our info is old...)
		if (sGridNo == pSoldier->position().gridNo())
		{
			continue;			// next merc
		}

		// this function is used only for turning towards closest opponent or changing stance
		// as such, if they AI is in a building,
		// we should ignore people who are on the roof of the same building as the AI
		if ( (bLevel != pSoldier->position().level()) && SameBuilding( pSoldier->position().gridNo(), sGridNo ) )
		{
			continue;
		}

		// I hope this will be good enough; otherwise we need a fractional/world-units-based 2D distance function
		//sRange = PythSpacesAway( pSoldier->sGridNo, sGridNo);
		iRange = GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo(), sGridNo );

		if (sClosestOpponent == NOWHERE ||
			iRange < iClosestRange ||
			pClosestOpponent && !TacticalActorConditions::isZombie(*pClosestOpponent) && !(pSoldier->status().flags() & SOLDIER_BOXER) && pClosestOpponent->vitals().health() < OKLIFE && pOpponent->vitals().health() >= OKLIFE)
		{
			iClosestRange = iRange;
			sClosestOpponent = sGridNo;
			bClosestLevel = bLevel;
			pClosestOpponent = pOpponent;
		}
	}

#ifdef DEBUGDECISIONS	
	if (!TileIsOutOfBounds(sClosestOpponent))
	{
		AINumMessage("CLOSEST OPPONENT is at gridno ",sClosestOpponent);
	}
#endif

	if (psGridNo)
	{
		*psGridNo = sClosestOpponent;
	}
	if (pbLevel)
	{
		*pbLevel = bClosestLevel;
	}
	if (pubOpponentID && pClosestOpponent)
	{
		*pubOpponentID = pClosestOpponent->identity().id();
	}
	return( sClosestOpponent );
}

INT32 ClosestSeenOpponent(TacticalActor *pSoldier, INT32 * psGridNo, INT8 * pbLevel)
{
	INT32 sGridNo, sClosestOpponent = NOWHERE;
	UINT32 uiLoop;
	INT32 iRange, iClosestRange = 1500;
	INT8	*pbPersOL;
	INT8	bLevel, bClosestLevel;
	TacticalActor * pOpp;

	bClosestLevel = -1;

	// look through this man's personal & public opplists for opponents known
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpp = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, or dead
		if (!pOpp)
		{
			continue;			// next merc
		}

		// if this merc is neutral/on same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpp ) || (pSoldier->roster().side() == pOpp->roster().side()))
		{
			continue;			// next merc
		}

		// silversurfer: ignore empty vehicles
		if ( pOpp->employment().mercenaryType() == MERC_TYPE__VEHICLE && GetNumberInVehicle( pOpp->vehicleState().tacticalVehicleId() ) == 0 )
			continue;

		// Special stuff for Carmen the bounty hunter
		if (pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY && pOpp->identity().profile() != SLAY)
		{
			continue;	// next opponent
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpp->identity().id();

		// if this opponent is not seen personally
		if (*pbPersOL != SEEN_CURRENTLY)
		{
			continue;			// next merc
		}

		// since we're dealing with seen people, use exact gridnos
		sGridNo = pOpp->position().gridNo();
		bLevel = pOpp->position().level();

		// if we are standing at that gridno(!, obviously our info is old...)
		if (sGridNo == pSoldier->position().gridNo())
		{
			continue;			// next merc
		}

		// this function is used only for turning towards closest opponent or changing stance
		// as such, if they AI is in a building,
		// we should ignore people who are on the roof of the same building as the AI
		if ( (bLevel != pSoldier->position().level()) && SameBuilding( pSoldier->position().gridNo(), sGridNo ) )
		{
			continue;
		}

		// I hope this will be good enough; otherwise we need a fractional/world-units-based 2D distance function
		//sRange = PythSpacesAway( pSoldier->sGridNo, sGridNo);
		iRange = GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo(), sGridNo );

		if (iRange < iClosestRange)
		{
			iClosestRange = iRange;
			sClosestOpponent = sGridNo;
			bClosestLevel = bLevel;
		}
	}

#ifdef DEBUGDECISIONS	
	if (!TileIsOutOfBounds(sClosestOpponent))
	{
		AINumMessage("CLOSEST OPPONENT is at gridno ",sClosestOpponent);
	}
#endif

	if (psGridNo)
	{
		*psGridNo = sClosestOpponent;
	}
	if (pbLevel)
	{
		*pbLevel = bClosestLevel;
	}
	return( sClosestOpponent );
}


// special variant with a minor twist
INT32 ClosestSeenOpponentWithRoof(TacticalActor *pSoldier, INT32 * psGridNo, INT8 * pbLevel)
{
	INT32 sGridNo, sClosestOpponent = NOWHERE;
	UINT32 uiLoop;
	INT32 iRange, iClosestRange = 1500;
	INT8	*pbPersOL;
	INT8	bLevel, bClosestLevel;
	TacticalActor * pOpp;

	bClosestLevel = -1;

	// look through this man's personal & public opplists for opponents known
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpp = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, or dead
		if (!pOpp)
		{
			continue;			// next merc
		}

		// if this merc is neutral/on same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpp ) || (pSoldier->roster().side() == pOpp->roster().side()))
		{
			continue;			// next merc
		}

		// silversurfer: ignore empty vehicles
		if ( pOpp->employment().mercenaryType() == MERC_TYPE__VEHICLE && GetNumberInVehicle( pOpp->vehicleState().tacticalVehicleId() ) == 0 )
			continue;

		// Special stuff for Carmen the bounty hunter
		if (pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY && pOpp->identity().profile() != SLAY)
		{
			continue;	// next opponent
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpp->identity().id();

		// if this opponent is not seen personally
		if (*pbPersOL != SEEN_CURRENTLY)
		{
			continue;			// next merc
		}

		// since we're dealing with seen people, use exact gridnos
		sGridNo = pOpp->position().gridNo();
		bLevel = pOpp->position().level();

		// if we are standing at that gridno(!, obviously our info is old...)
		if (sGridNo == pSoldier->position().gridNo())
		{
			continue;			// next merc
		}

		// special: allow zombies to also find opponents on a roof
		// otherwise they have will never decide to climb a roof while they are seeing an enemy
		// this function is used only for turning towards closest opponent or changing stance
		// as such, if they AI is in a building,
		// we should ignore people who are on the roof of the same building as the AI
		/*if ( !TacticalActorConditions::isZombie(*pSoldier) && (bLevel != pSoldier->position().level()) && SameBuilding( pSoldier->sGridNo, sGridNo ) )
		{
			continue;
		}*/

		// I hope this will be good enough; otherwise we need a fractional/world-units-based 2D distance function
		//sRange = PythSpacesAway( pSoldier->sGridNo, sGridNo);
		iRange = GetRangeInCellCoordsFromGridNoDiff( pSoldier->position().gridNo(), sGridNo );

		if (iRange < iClosestRange)
		{
			iClosestRange = iRange;
			sClosestOpponent = sGridNo;
			bClosestLevel = bLevel;
		}
	}

#ifdef DEBUGDECISIONS	
	if (!TileIsOutOfBounds(sClosestOpponent))
	{
		AINumMessage("CLOSEST OPPONENT is at gridno ",sClosestOpponent);
	}
#endif

	if (psGridNo)
	{
		*psGridNo = sClosestOpponent;
	}
	if (pbLevel)
	{
		*pbLevel = bClosestLevel;
	}
	return( sClosestOpponent );
}

INT32 ClosestPC( TacticalActor *pSoldier, INT32 * psDistance )
{
	// used by NPCs... find the closest PC

	// NOTE: skips EPCs!

	TacticalActor		*pTargetSoldier;
	INT32					sMinDist = WORLD_MAX;
	INT32					sDist;
	INT32					sGridNo = NOWHERE;

	// Loop through all mercs on player team
	SoldierID ubLoop = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;

	for ( ; ubLoop <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++ubLoop)
	{
		pTargetSoldier = GetJa2SoldierRepository().resolve(ubLoop.i);

		if (!pTargetSoldier ||
			!pTargetSoldier->roster().active() || !pTargetSoldier->roster().inSector())
		{
			continue;
		}

		// if not conscious, skip him
		if (pTargetSoldier->vitals().health() < OKLIFE)
		{
		continue;
		}

		if ( AM_AN_EPC( pTargetSoldier ) )
		{
			continue;
		}

		sDist = PythSpacesAway(pSoldier->position().gridNo(),pTargetSoldier->position().gridNo());

		// if this PC is not visible to the soldier, then add a penalty to the distance
		// so that we weight in favour of visible mercs
		if ( pTargetSoldier->roster().team() != pSoldier->roster().team() && pSoldier->awareness().opponentKnowledge()[ ubLoop ] != SEEN_CURRENTLY )
		{
			sDist += 10;
		}

		if (sDist < sMinDist)
		{
			sMinDist = sDist;
			sGridNo = pTargetSoldier->position().gridNo();
		}
	}

	if ( psDistance )
	{
		*psDistance = sMinDist;
	}

	return( sGridNo );
}

// Flugente: like ClosestPC(...), but does not account for covert or not visible mercs
INT32 ClosestUnDisguisedPC( TacticalActor *pSoldier, INT32 * psDistance )
{
	// used by NPCs... find the closest PC
	// NOTE: skips EPCs!

	TacticalActor		*pTargetSoldier;
	INT32					sMinDist = WORLD_MAX;
	INT32					sDist;
	INT32					sGridNo = NOWHERE;

	// Loop through all mercs on player team
	SoldierID ubLoop = gTacticalStatus.Team[ gbPlayerNum ].bFirstID;
	for ( ; ubLoop <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; ++ubLoop )
	{
		pTargetSoldier = GetJa2SoldierRepository().resolve(ubLoop.i);

		if (!pTargetSoldier ||
			!pTargetSoldier->roster().active() || !pTargetSoldier->roster().inSector())
			continue;
				
		// if not conscious, skip him
		if (pTargetSoldier->vitals().health() < OKLIFE)
			continue;

		if ( AM_AN_EPC( pTargetSoldier ) )
			continue;

		if ( pTargetSoldier->featureFlags().primaryFlags() & (SOLDIER_COVERT_CIV|SOLDIER_COVERT_SOLDIER) )
			continue;

		sDist = PythSpacesAway(pSoldier->position().gridNo(),pTargetSoldier->position().gridNo());

		// if this PC is not visible to the soldier, then add a penalty to the distance
		// so that we weight in favour of visible mercs
		if ( pTargetSoldier->roster().team() != pSoldier->roster().team() && pSoldier->awareness().opponentKnowledge()[ ubLoop ] != SEEN_CURRENTLY )
			continue;

		if (sDist < sMinDist)
		{
			sMinDist = sDist;
			sGridNo = pTargetSoldier->position().gridNo();
		}
	}

	if ( psDistance )
	{
		*psDistance = sMinDist;
	}

	return( sGridNo );
}

INT32 FindClosestClimbPointAvailableToAI( TacticalActor * pSoldier, INT32 sStartGridNo, INT32 sDesiredGridNo, BOOLEAN fClimbUp )
{
	// sevenfm: safety check
	if (!pSoldier)
	{
		return NOWHERE;
	}

	// sevenfm: don't check roaming range
	return FindClosestClimbPoint(pSoldier, sStartGridNo, sDesiredGridNo, fClimbUp);

	/*INT32 sGridNo;
	INT32	sRoamingOrigin;
	INT16	sRoamingRange;

	if ( pSoldier->status().flags() & SOLDIER_PC )
	{
		sRoamingOrigin = pSoldier->sGridNo;
		sRoamingRange = 99;
	}
	else
	{
		sRoamingRange = RoamingRange( pSoldier, &sRoamingOrigin );
	}

	// since climbing necessary involves going an extra tile, we compare against 1 less than the roam range...
	// or add 1 to the distance to the climb point

	sGridNo = FindClosestClimbPoint( pSoldier, sStartGridNo, sDesiredGridNo, fClimbUp );


	if ( PythSpacesAway( sRoamingOrigin, sGridNo ) + 1 > sRoamingRange )
	{
		return( NOWHERE );
	}
	else
	{
		return( sGridNo );
	}*/
}

BOOLEAN ClimbingNecessary( TacticalActor * pSoldier, INT32 sDestGridNo, INT8 bDestLevel )
{
	if (pSoldier->position().level() == bDestLevel)
	{
		if ( (pSoldier->position().level() == 0) || ( gubBuildingInfo[ pSoldier->position().gridNo() ] == gubBuildingInfo[ sDestGridNo ] ) )
		{
			return( FALSE );
		}
		else // different buildings!
		{
			return( TRUE );
		}
	}
	else
	{
		return( TRUE );
	}
}

INT32 GetInterveningClimbingLocation( TacticalActor * pSoldier, INT32 sDestGridNo, INT8 bDestLevel, BOOLEAN * pfClimbingNecessary )
{
	if (pSoldier->position().level() == bDestLevel)
	{
		if ( (pSoldier->position().level() == 0) || ( gubBuildingInfo[ pSoldier->position().gridNo() ] == gubBuildingInfo[ sDestGridNo ] ) )
		{
			// on ground or same building... normal!
			*pfClimbingNecessary = FALSE;
			return( NOWHERE );
		}
		else
		{
			// different buildings!
			// yes, pass in same gridno twice... want closest climb-down spot for building we are on!
			*pfClimbingNecessary = TRUE;
			return( FindClosestClimbPointAvailableToAI( pSoldier, pSoldier->position().gridNo(), pSoldier->position().gridNo(), FALSE ) );
		}
	}
	else
	{
		*pfClimbingNecessary = TRUE;
		// different levels
		if (pSoldier->position().level() == 0)
		{
			// got to go UP onto building
			return( FindClosestClimbPointAvailableToAI( pSoldier, pSoldier->position().gridNo(), sDestGridNo, TRUE ) );
		}
		else
		{
			// got to go DOWN off building
			return( FindClosestClimbPointAvailableToAI( pSoldier, pSoldier->position().gridNo(), pSoldier->position().gridNo(), FALSE ) );
		}
	}
}

INT16 EstimatePathCostToLocation( TacticalActor * pSoldier, INT32 sDestGridNo, INT8 bDestLevel, BOOLEAN fAddCostAfterClimbingUp, BOOLEAN * pfClimbingNecessary, INT32 * psClimbGridNo )
{
	INT16	sPathCost;
	INT32 sClimbGridNo;

	if (pSoldier->position().level() == bDestLevel)
	{
		if ( (pSoldier->position().level() == 0) || ( gubBuildingInfo[ pSoldier->position().gridNo() ] == gubBuildingInfo[ sDestGridNo ] ) )
		{
			// on ground or same building... normal!
			sPathCost = EstimatePlotPath( pSoldier, sDestGridNo, FALSE, FALSE, FALSE, WALKING, FALSE, FALSE, 0);
			if (pfClimbingNecessary)
				*pfClimbingNecessary = FALSE;
			if (psClimbGridNo)
				*psClimbGridNo = NOWHERE;
		}
		else
		{
			// different buildings!
			// yes, pass in same gridno twice... want closest climb-down spot for building we are on!
			sClimbGridNo = FindClosestClimbPointAvailableToAI( pSoldier, sDestGridNo, pSoldier->position().gridNo(), FALSE );
			if (TileIsOutOfBounds(sClimbGridNo))
			{
				sPathCost = 0;
			}
			else
			{
				sPathCost = PlotPath( pSoldier, sClimbGridNo, FALSE, FALSE, FALSE, WALKING, FALSE, FALSE, 0 );
				// sevenfm: check if we are already standing at climb gridno
				if (sPathCost != 0 || pSoldier->position().gridNo() == sClimbGridNo)
				{
					// add in cost of climbing down
					if (fAddCostAfterClimbingUp)
					{
						// add in cost of later climbing up, too
						sPathCost += APBPConstants[AP_CLIMBOFFROOF] + APBPConstants[AP_CLIMBROOF];
						// add in an estimate of getting there after climbing down
						sPathCost += (APBPConstants[AP_MOVEMENT_FLAT] + APBPConstants[AP_MODIFIER_WALK]) * PythSpacesAway( sClimbGridNo, sDestGridNo );
					}
					else
					{
						sPathCost += APBPConstants[AP_CLIMBOFFROOF];
						// add in an estimate of getting there after climbing down, *but not on top of roof*
						sPathCost += (APBPConstants[AP_MOVEMENT_FLAT] + APBPConstants[AP_MODIFIER_WALK]) * PythSpacesAway( sClimbGridNo, sDestGridNo ) / 2;
					}
					if (pfClimbingNecessary)
						*pfClimbingNecessary = TRUE;
					if (psClimbGridNo)
						*psClimbGridNo = sClimbGridNo;
				}
			}
		}
	}
	else
	{
		// sevenfm: check if zombie cannot climb
		if (TacticalActorConditions::isZombie(*pSoldier) && !gGameExternalOptions.fZombieCanClimb)
		{
			return 0;
		}

		// different levels
		if (pSoldier->position().level() == 0)
		{
			//got to go UP onto building
			sClimbGridNo = FindClosestClimbPointAvailableToAI( pSoldier,	pSoldier->position().gridNo(), sDestGridNo, TRUE );
		}
		else
		{
			// got to go DOWN off building
			sClimbGridNo = FindClosestClimbPointAvailableToAI( pSoldier, sDestGridNo, pSoldier->position().gridNo(), FALSE );
		}
		
		if (TileIsOutOfBounds(sClimbGridNo))
		{
			sPathCost = 0;
		}
		else
		{
			sPathCost = PlotPath( pSoldier, sClimbGridNo, FALSE, FALSE, FALSE, WALKING, FALSE, FALSE, 0);
			// sevenfm: check if we are already standing at climb gridno
			if (sPathCost != 0 || pSoldier->position().gridNo() == sClimbGridNo)
			{
				// add in the cost of climbing up or down
				if (pSoldier->position().level() == 0)
				{
					// must climb up
					sPathCost += APBPConstants[AP_CLIMBROOF];
					if (fAddCostAfterClimbingUp)
					{
						// add to path a rough estimate of how far to go from the climb gridno to the friend
						// estimate walk cost
						sPathCost += (APBPConstants[AP_MOVEMENT_FLAT] + APBPConstants[AP_MODIFIER_WALK]) * PythSpacesAway( sClimbGridNo, sDestGridNo );
					}
				}
				else
				{
					// must climb down
					sPathCost += APBPConstants[AP_CLIMBOFFROOF];
					// add to path a rough estimate of how far to go from the climb gridno to the friend
					// estimate walk cost
					sPathCost += (APBPConstants[AP_MOVEMENT_FLAT] + APBPConstants[AP_MODIFIER_WALK]) * PythSpacesAway( sClimbGridNo, sDestGridNo );
				}
				if (pfClimbingNecessary)
					*pfClimbingNecessary = TRUE;
				if (psClimbGridNo)
					*psClimbGridNo = sClimbGridNo;
			}
		}
	}

	return( sPathCost );
}

BOOLEAN GuySawEnemy( TacticalActor * pSoldier, UINT8 ubMax )
{
	UINT8		ubTeamLoop;
	TacticalActor *pOpponent;

	for ( ubTeamLoop = 0; ubTeamLoop < MAXTEAMS; ++ubTeamLoop )
	{
		if(!gTacticalStatus.Team[ubTeamLoop].bTeamActive)//dnl ch58 070913 skip any inactive teams
			continue;

		if ( gTacticalStatus.Team[ ubTeamLoop ].bSide != pSoldier->roster().side() )
		{
			// consider guys in this team, which isn't on our side
			for ( SoldierID ubIDLoop = gTacticalStatus.Team[ ubTeamLoop ].bFirstID; ubIDLoop <= gTacticalStatus.Team[ ubTeamLoop ].bLastID; ++ubIDLoop )
			{
				pOpponent =
					GetJa2SoldierRepository().resolve(ubIDLoop.i);

				// if this merc is inactive, at base, on assignment, or dead
				if (!pOpponent)
				{
					continue;
				}

				// if this merc is neutral/on same side, he's not an opponent
				if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()) )
				{
					continue;
				}

				// sevenfm: ignore empty vehicles
				if( pOpponent->employment().mercenaryType() == MERC_TYPE__VEHICLE && GetNumberInVehicle( pOpponent->vehicleState().tacticalVehicleId() ) == 0 )
				{
					continue;
				}

				// if this guy SAW an enemy recently...
				if( pSoldier->awareness().opponentKnowledge()[ ubIDLoop ] >= SEEN_CURRENTLY &&
					pSoldier->awareness().opponentKnowledge()[ ubIDLoop ] <= ubMax )
				{
					return( TRUE );
				}
			}
		}
	}

	return( FALSE );
}

INT32 ClosestReachableFriendInTrouble(TacticalActor *pSoldier, BOOLEAN * pfClimbingNecessary)
{
	UINT32 uiLoop;
	INT32 sPathCost, sClosestFriend = NOWHERE, sShortestPath = 1000, sClimbGridNo;
	BOOLEAN fClimbingNecessary, fClosestClimbingNecessary = FALSE;
	TacticalActor *pFriend;
	INT32 sClosestKnownOpponent;
	BOOLEAN fCallHelp;

	// civilians don't really have any "friends", so they don't bother with this
	if (PTR_CIVILIAN)
	{
		return(NOWHERE);
	}

	// consider every friend of this soldier (locations assumed to be known)
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pFriend = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, or dead
		if (!pFriend)
		{
			continue;			// next merc
		}

		// if this merc is neutral or NOT on the same side, he's not a friend
		if (pFriend->aiBehavior().neutral() || (pSoldier->roster().side() != pFriend->roster().side()))
		{
			continue;			// next merc
		}

		// if this "friend" is actually US
		if (pFriend->identity().id() == pSoldier->identity().id())
		{
			continue;			// next merc
		}

		sClosestKnownOpponent = ClosestKnownOpponent(pFriend, NULL, NULL);

		// CJC: restrict "last one to radio" to only if that guy saw us this turn or last turn
		fCallHelp = FALSE;

		// if this friend is under fire or he called for help recently
		if (pFriend->suppression().underFire() || (pFriend->identity().id() == gTacticalStatus.Team[pFriend->roster().team()].ubLastMercToRadio && GuySawEnemy(pFriend)))
		{
			fCallHelp = TRUE;
		}

		// zombies always call for help if they know enemy position
		if (TacticalActorConditions::isZombie(*pSoldier) && TacticalActorConditions::isZombie(*pFriend) && !TileIsOutOfBounds(sClosestKnownOpponent))
		{
			fCallHelp = TRUE;
		}

		if (!fCallHelp)
		{
			continue;			// next merc
		}

		// if we're already neighbors
		if (SpacesAway(pSoldier->position().gridNo(),pFriend->position().gridNo()) == 1)
		{
			continue;			// next merc
		}

		// get the AP cost to go to this friend's gridno
		sPathCost = EstimatePathCostToLocation( pSoldier, pFriend->position().gridNo(), pFriend->position().level(), TRUE, &fClimbingNecessary, &sClimbGridNo );

		// if we can get there
		if (sPathCost != 0)
		{
			//sprintf(tempstr,"Path cost to friend %s's location is %d",pFriend->identity().name(),pathCost);
			//PopMessage(tempstr);

			if (sPathCost < sShortestPath)
			{
				if (fClimbingNecessary)
				{
					sClosestFriend = sClimbGridNo;
				}
				else
				{
					sClosestFriend = pFriend->position().gridNo();
				}

				sShortestPath = sPathCost;
				fClosestClimbingNecessary = fClimbingNecessary;
			}
		}
	}


#ifdef DEBUGDECISIONS	
	if (!TileIsOutOfBounds(sClosestFriend))
	{
		AINumMessage("CLOSEST FRIEND is at gridno ",sClosestFriend);
	}
#endif

	*pfClimbingNecessary = fClosestClimbingNecessary;
	return(sClosestFriend);
}

INT16 DistanceToClosestFriend( TacticalActor * pSoldier )
{
	// find the distance to the closest person on the same team
	TacticalActor		*pTargetSoldier;
	INT16					sMinDist = 1000;
	INT16					sDist;

	// Loop through all mercs on player team
	SoldierID ubLoop = gTacticalStatus.Team[ pSoldier->roster().team() ].bFirstID;

	for ( ; ubLoop <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID; ++ubLoop )
	{
		if (ubLoop == pSoldier->identity().id())
		{
			// same guy - continue!
			continue;
		}

		pTargetSoldier = GetJa2SoldierRepository().resolve(ubLoop.i);
		if (!pTargetSoldier)
		{
			continue;
		}

		if ( pSoldier->roster().active() && pSoldier->roster().inSector() )
		{
			if (!pTargetSoldier->roster().active() || !pTargetSoldier->roster().inSector())
			{
				continue;
			}
			// if not conscious, skip him
			else if (pTargetSoldier->vitals().health() < OKLIFE)
			{
				continue;
			}
		}
		else
		{
			// compare sector #s
			if ( (pSoldier->deployment().sectorX() != pTargetSoldier->deployment().sectorX()) ||
				(pSoldier->deployment().sectorY() != pTargetSoldier->deployment().sectorY()) ||
				(pSoldier->deployment().sectorZ() != pTargetSoldier->deployment().sectorZ()) )
			{
				continue;
			}
			else if (pTargetSoldier->vitals().health() < OKLIFE)
			{
				continue;
			}
			else
			{
				// well there's someone who could be near
				return( 1 );
			}
		}

		sDist = SpacesAway(pSoldier->position().gridNo(),pTargetSoldier->position().gridNo());

		if (sDist < sMinDist)
		{
			sMinDist = sDist;
		}
	}

	return( sMinDist );
}

BOOLEAN InWaterGasOrSmoke( TacticalActor *pSoldier, INT32 sGridNo )
{
	if (WaterTooDeepForAttacks( sGridNo, pSoldier->position().level() ))
	{
		return(TRUE);
	}

	// smoke
	if (gpWorldLevelData[sGridNo].ubExtFlags[ pSoldier->position().level() ] & MAPELEMENT_EXT_SMOKE)
	{
		return TRUE;
	}

	return InGas( pSoldier, sGridNo );
}

BOOLEAN InGasOrSmoke( TacticalActor *pSoldier, INT32 sGridNo )
{
	// smoke
	if ( gpWorldLevelData[sGridNo].ubExtFlags[pSoldier->position().level()] & (MAPELEMENT_EXT_SMOKE | MAPELEMENT_EXT_SIGNAL_SMOKE | MAPELEMENT_EXT_DEBRIS_SMOKE | MAPELEMENT_EXT_FIRERETARDANT_SMOKE ) )
		return TRUE;

	return InGas(pSoldier,sGridNo);
}


INT16 InWaterOrGas(TacticalActor *pSoldier, INT32 sGridNo)
{
	if (WaterTooDeepForAttacks( sGridNo, pSoldier->position().level() ))
	{
		return(TRUE);
	}

	return (INT16)InGas( pSoldier, sGridNo );
}

BOOLEAN InGasSpot(TacticalActor *pSoldier, INT32 sGridNo, INT8 bLevel)
{
	CHECKF(pSoldier);

	if (TileIsOutOfBounds(sGridNo))
		return FALSE;

	// tear gas
	if ((gpWorldLevelData[sGridNo].ubExtFlags[bLevel] & MAPELEMENT_EXT_TEARGAS) &&
		!DoesSoldierWearGasMask(pSoldier))
	{
		return(TRUE);
	}
	// fire/creature/mustard gas
	// sevenfm: avoid mustard gas even when wearing gas mask
	if (gpWorldLevelData[sGridNo].ubExtFlags[bLevel] & (MAPELEMENT_EXT_BURNABLEGAS | MAPELEMENT_EXT_CREATUREGAS | MAPELEMENT_EXT_MUSTARDGAS))
	{
		return(TRUE);
	}
	return FALSE;
}

BOOLEAN InGas(TacticalActor *pSoldier, INT32 sGridNo)
{
	CHECKF(pSoldier);

	if (TileIsOutOfBounds(sGridNo))
		return FALSE;

	if (InGasSpot(pSoldier, sGridNo, pSoldier->position().level()))
	{
		return TRUE;
	}

	//WarmSteel - One square away from gas is still considered in gas, because it could expand any moment.
	//Note: this only works for gas that expands with one tile, but hey it's better than nothing!
	int iNeighbourGridNo;
	for (int iDir = 0; iDir < NUM_WORLD_DIRECTIONS; ++iDir)
	{
		iNeighbourGridNo = sGridNo + DirectionInc(iDir);

		if (!TileIsOutOfBounds(iNeighbourGridNo) && InGasSpot(pSoldier, iNeighbourGridNo, pSoldier->position().level()))
		{
			return TRUE;
		}
	}

	return(FALSE);
}

BOOLEAN WearGasMaskIfAvailable( TacticalActor * pSoldier )
{
	INT8		bSlot, bNewSlot;

	bSlot = FindGasMask( pSoldier );
	if ( bSlot == NO_SLOT )
	{
		return( FALSE );
	}
	if ( bSlot == HEAD1POS || bSlot == HEAD2POS || bSlot == HELMETPOS )
	{
		return( FALSE );
	}
	if ( pSoldier->inventory()[ HEAD1POS ].exists() == false )
	{
		bNewSlot = HEAD1POS;
	}
	else if ( pSoldier->inventory()[ HEAD2POS ].exists() == false )
	{
		bNewSlot = HEAD2POS;
	}
	else
	{
		// screw it, going in position 1 anyhow
		bNewSlot = HEAD1POS;
	}

	RearrangePocket( pSoldier, bSlot, bNewSlot, TRUE );

	if ( pSoldier->roster().team() == gbPlayerNum )
	{
		(void)TacticalActorLighting::destroyPersonalLight(
			*pSoldier);
		(void)TacticalActorLighting::positionPersonalLight(
			*pSoldier);
	}

	return( TRUE );
}

BOOLEAN InLightAtNight( INT32 sGridNo, INT8 bLevel )
{
	UINT8 ubBackgroundLightLevel;

	// do not consider us to be "in light" if we're in an underground sector
	if ( gbWorldSectorZ > 0 )
	{
		return( FALSE );
	}

	ubBackgroundLightLevel = GetTimeOfDayAmbientLightLevel();

	if ( ubBackgroundLightLevel < NORMAL_LIGHTLEVEL_DAY + 2 )
	{
		// don't consider it nighttime, too close to daylight levels
		return( FALSE );
	}

	// could've been placed here, ignore the light
	if ( InARoom( sGridNo, NULL ) )
	{
		return( FALSE );
	}

	// NB light levels are backwards, so a lower light level means the
	// spot in question is BRIGHTER

	if ( LightTrueLevel( sGridNo, bLevel ) < ubBackgroundLightLevel )
	{
		return( TRUE );
	}

	return( FALSE );
}

INT8 CalcMorale(TacticalActor *pSoldier)
{
	UINT32 uiLoop, uiLoop2;
	INT32 iOurTotalThreat = 0, iTheirTotalThreat = 0;
	INT16 sOppThreatValue, sFrndThreatValue, sMorale;
	INT32 iPercent;
	INT8	bMostRecentOpplistValue;
	INT8 bMoraleCategory;
	UINT8 *pSeenOpp; //,*friendOlPtr;
	INT8	*pbPersOL, *pbPublOL;
	TacticalActor *pOpponent, *pFriend;

	// sevenfm: use new calculation:
	if (gGameExternalOptions.fAINewMorale)
	{
		return CalcMoraleNew(pSoldier);
	}

	// if army guy has NO weapons left then panic!
	if (pSoldier->roster().team() == ENEMY_TEAM)
	{
		if (FindAIUsableObjClass(pSoldier, IC_WEAPON) == NO_SLOT)
		{
			return(MORALE_HOPELESS);
		}
	}

	// hang pointers to my personal opplist, my team's public opplist, and my
	// list of previously seen opponents
	pSeenOpp = (UINT8 *)&(gbSeenOpponents[pSoldier->identity().id()][0]);

	// loop through every one of my possible opponents
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || (pOpponent->vitals().health() < OKLIFE))
			continue;			// next merc

		// if this merc is neutral/on same side, he's not an opponent, skip him!
		if (CONSIDERED_NEUTRAL(pSoldier, pOpponent) || (pSoldier->roster().side() == pOpponent->roster().side()))
			continue;			// next merc

		// Special stuff for Carmen the bounty hunter
		if (pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY && pOpponent->identity().profile() != SLAY)
		{
			continue;	// next opponent
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();
		pbPublOL = gbPublicOpplist[pSoldier->roster().team()] + pOpponent->identity().id();
		pSeenOpp = (UINT8 *)gbSeenOpponents[pSoldier->identity().id()] + pOpponent->identity().id();

		// if this opponent is unknown to me personally AND unknown to my team, too
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			// if I have never seen him before anywhere in this sector, either
			if (!(*pSeenOpp))
				continue;		// next merc

			// have seen him in the past, so he remains something of a threat
			bMostRecentOpplistValue = 0;		// uses the free slot for 0 opplist
		}
		else		 // decide which opplist is more current
		{
			// if personal knowledge is more up to date or at least equal
			if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) || (*pbPersOL == *pbPublOL))
				bMostRecentOpplistValue = *pbPersOL;		// use personal
			else
				bMostRecentOpplistValue = *pbPublOL;		// use public
		}

		iPercent = ThreatPercent[bMostRecentOpplistValue - OLDEST_HEARD_VALUE];

		sOppThreatValue = (iPercent * CalcManThreatValue(pOpponent, pSoldier->position().gridNo(), FALSE, pSoldier)) / 100;

		//sprintf(tempstr,"Known opponent %s, opplist status %d, percent %d, threat = %d",
		//			ExtMen[pOpponent->identity().id()].name,ubMostRecentOpplistValue,ubPercent,sOppThreatValue);
		//PopMessage(tempstr);

		// ADD this to their running total threatValue (decreases my MORALE)
		iTheirTotalThreat += sOppThreatValue;
		//NumMessage("Their TOTAL threat now = ",sTheirTotalThreat);

		// NOW THE FUN PART: SINCE THIS OPPONENT IS KNOWN TO ME IN SOME WAY,
		// ANY FRIENDS OF MINE THAT KNOW ABOUT HIM BOOST MY MORALE.	SO, LET'S GO
		// THROUGH THEIR PERSONAL OPPLISTS AND CHECK WHICH OF MY FRIENDS KNOW
		// SOMETHING ABOUT HIM AND WHAT THEIR THREAT VALUE TO HIM IS.

		for (uiLoop2 = 0; uiLoop2 < Ja2ActiveTacticalActorSlotCount(); uiLoop2++)
		{
			pFriend = ResolveJa2ActiveTacticalActorSlot(uiLoop2);

			// if this merc is inactive, at base, on assignment, dead, unconscious
			if (!pFriend || (pFriend->vitals().health() < OKLIFE))
				continue;		// next merc

			// if this merc is not on my side, then he's NOT one of my friends

			// WE CAN'T AFFORD TO CONSIDER THE ENEMY OF MY ENEMY MY FRIEND, HERE!
			// ONLY IF WE ARE ACTUALLY OFFICIALLY CO-OPERATING TOGETHER (SAME SIDE)
			if (pFriend->aiBehavior().neutral() && !(pSoldier->roster().civilianGroup() != NON_CIV_GROUP && pSoldier->roster().civilianGroup() == pFriend->roster().civilianGroup()))
			{
				continue;		// next merc
			}

			if (pSoldier->roster().side() != pFriend->roster().side())
				continue;		// next merc

			// THIS TEST IS INVALID IF A COMPUTER-TEAM IS PLAYING CO-OPERATIVELY
			// WITH A NON-COMPUTER TEAM SINCE THE OPPLISTS INVOLVED ARE NOT
			// UP-TO-DATE.	THIS SITUATION IS CURRENTLY NOT POSSIBLE IN HTH/DG.

			// ALSO NOTE THAT WE COUNT US AS OUR (BEST) FRIEND FOR THESE CALCULATIONS

			// subtract HEARD_2_TURNS_AGO (which is negative) to make values start at 0 and
			// be positive otherwise
			iPercent = ThreatPercent[pFriend->awareness().opponentKnowledge()[pOpponent->identity().id()] - OLDEST_HEARD_VALUE];

			// reduce the percentage value based on how far away they are from the enemy, if they only hear him
			if (pFriend->awareness().opponentKnowledge()[pOpponent->identity().id()] <= HEARD_LAST_TURN)
			{
				iPercent -= PythSpacesAway(pSoldier->position().gridNo(), pFriend->position().gridNo()) * 2;
				if (iPercent <= 0)
				{
					//ignore!
					continue;
				}
			}

			sFrndThreatValue = (iPercent * CalcManThreatValue(pFriend, pOpponent->position().gridNo(), FALSE, pSoldier)) / 100;

			//sprintf(tempstr,"Known by friend %s, opplist status %d, percent %d, threat = %d",
			//		 ExtMen[pFriend->identity().id()].name,pFriend->awareness().opponentKnowledge()[pOpponent->identity().id()],ubPercent,sFrndThreatValue);
			//PopMessage(tempstr);

			// ADD this to our running total threatValue (increases my MORALE)
			// We multiply by sOppThreatValue to PRO-RATE this based on opponent's
			// threat value to ME personally.	Divide later by sum of them all.
			iOurTotalThreat += sOppThreatValue * sFrndThreatValue;
		}

		// this could get slow if I have a lot of friends...
		//KeepInterfaceGoing();
	}


	// if they are no threat whatsoever
	if (!iTheirTotalThreat)
		sMorale = 500;		// our morale is just incredible
	else
	{
		// now divide sOutTotalThreat by sTheirTotalThreat to get the REAL value
		iOurTotalThreat /= iTheirTotalThreat;

		// calculate the morale (100 is even, < 100 is us losing, > 100 is good)
		sMorale = (INT16)((100 * iOurTotalThreat) / iTheirTotalThreat);
	}


	if (sMorale <= 25)				// odds 1:4 or worse
		bMoraleCategory = MORALE_HOPELESS;
	else if (sMorale <= 50)		 // odds between 1:4 and 1:2
		bMoraleCategory = MORALE_WORRIED;
	else if (sMorale <= 150)		// odds between 1:2 and 3:2
		bMoraleCategory = MORALE_NORMAL;
	else if (sMorale <= 300)		// odds between 3:2 and 3:1
		bMoraleCategory = MORALE_CONFIDENT;
	else							// odds better than 3:1
		bMoraleCategory = MORALE_FEARLESS;


	switch (pSoldier->aiBehavior().attitude())
	{
	case DEFENSIVE:	bMoraleCategory--; break;
	case BRAVESOLO:	bMoraleCategory += 2; break;
	case BRAVEAID:	bMoraleCategory += 2; break;
	case CUNNINGSOLO:	break;
	case CUNNINGAID:	 break;
	case AGGRESSIVE:	bMoraleCategory++; break;
	}

	// make idiot administrators much more aggressive
	if (pSoldier->roster().soldierClass() == SOLDIER_CLASS_ADMINISTRATOR || pSoldier->roster().soldierClass() == SOLDIER_CLASS_BANDIT)
	{
		bMoraleCategory += 2;
	}

	// if still full of energy
	if (pSoldier->vitals().breath() > 75)
		bMoraleCategory++;
	else
	{
		// if getting a bit low on breath
		if (pSoldier->vitals().breath() < 40)
			bMoraleCategory--;

		// if getting REALLY low on breath
		if (pSoldier->vitals().breath() < 10)
			bMoraleCategory--;
	}


	// if still very healthy
	if (pSoldier->vitals().health() > 75)
		bMoraleCategory++;
	else
	{
		// if getting a bit low on life
		if (pSoldier->vitals().health() < 40)
			bMoraleCategory--;

		// if getting REALLY low on life
		if (pSoldier->vitals().health() < 20)
			bMoraleCategory--;
	}


	// if soldier is currently not under fire
	if (!pSoldier->suppression().underFire())
		bMoraleCategory++;


	// if adjustments made it outside the allowed limits
	if (bMoraleCategory < MORALE_HOPELESS)
		bMoraleCategory = MORALE_HOPELESS;
	else
	{
		if (bMoraleCategory > MORALE_FEARLESS)
			bMoraleCategory = MORALE_FEARLESS;
	}

	// brave guys never get hopeless, at worst they get worried
	// SANDRO - on Insane difficulty enemy morale cannot go below worried
	if (bMoraleCategory == MORALE_HOPELESS)
	{
		if (pSoldier->aiBehavior().attitude() == BRAVESOLO || pSoldier->aiBehavior().attitude() == BRAVEAID || zDiffSetting[gGameOptions.ubDifficultyLevel].bEnemyMoraleWorried)
			bMoraleCategory = MORALE_WORRIED;
	}

#ifdef DEBUGDECISIONS
	STR tempstr;
	sprintf(tempstr, "Morale = %d (category %d)\n", pSoldier->morale().morale(), bMoraleCategory);
	DebugAI(tempstr);
#endif

	return(bMoraleCategory);
}

INT32 CalcManThreatValue( TacticalActor *pEnemy, INT32 sMyGrid, UINT8 ubReduceForCover, TacticalActor * pMe )
{
	INT32	iThreatValue = 0;
	BOOLEAN fForCreature = CREATURE_OR_BLOODCAT( pMe );

	// If man is inactive, at base, on assignment, dead, unconscious
	if (!pEnemy->roster().active() || !pEnemy->roster().inSector() || !pEnemy->vitals().health())
	{
		// he's no threat at all, return a negative number
		iThreatValue = -999;
		return(iThreatValue);
	}

	// in boxing mode, let only a boxer be considered a threat.
	if ( (gTacticalStatus.bBoxingState == BOXING) && !(pEnemy->status().flags() & SOLDIER_BOXER) )
	{
		iThreatValue = -999;
		return( iThreatValue );
	}

	if (fForCreature)
	{
		// health (1-100)
		iThreatValue += pEnemy->vitals().health();
		// bleeding (more attactive!) (1-100)
		iThreatValue += pEnemy->vitals().bleeding();
		// decrease according to distance
		iThreatValue = (iThreatValue * 10) / (10 + PythSpacesAway( sMyGrid, pEnemy->position().gridNo() ) );

	}
	else
	{
		// ADD twice the man's level (2-20)
		iThreatValue += EffectiveExpLevel(pEnemy); // SANDRO - find precise effective exp level

		// ADD man's total action points (10-35)
		iThreatValue += 25 * TacticalActorTurnBudget::calculateTurnGrant(*pEnemy) / APBPConstants[AP_MAXIMUM];

		// ADD 1/2 of man's current action points (4-17)
		iThreatValue += 25 * pEnemy->actionPoints().current() / APBPConstants[AP_MAXIMUM] / 2;

		// ADD 1/10 of man's current health (0-10)
		iThreatValue += (pEnemy->vitals().health() / 10);

		if (pEnemy->assignment().current() < ON_DUTY )
		{
			// ADD 1/4 of man's protection percentage (0-25)
			iThreatValue += ArmourPercent( pEnemy ) / 4;

			// ADD 1/5 of man's marksmanship skill (0-20)
			iThreatValue += (pEnemy->statistics().marksmanship() / 5);

			if ( Item[ pEnemy->inventory()[HANDPOS].usItem ].usItemClass & IC_WEAPON )
			{
				// ADD the deadliness of the item(weapon) he's holding (0-50)
				iThreatValue += Weapon[pEnemy->inventory()[HANDPOS].usItem].ubDeadliness;
			}
		}

		// SUBTRACT 1/5 of man's bleeding (0-20)
		iThreatValue -= (pEnemy->vitals().bleeding() / 5);

		// SUBTRACT 1/10 of man's breath deficiency (0-10)
		iThreatValue -= ((100 - pEnemy->vitals().breath()) / 10);

		// SUBTRACT man's current shock value
		iThreatValue -= pEnemy->suppression().shock();
	}

	// if I have a specifically defined spot where I'm at (sometime I don't!)	
	if (!TileIsOutOfBounds(sMyGrid))
	{
		// ADD 10% if man's already been shooting at me
		if (pEnemy->targeting().lastGridNo() == sMyGrid)
		{
			iThreatValue += (iThreatValue / 10);
		}
		else
		{
			// ADD 5% if man's already facing me
			if (pEnemy->position().direction() == GetDirectionFromCenterCellXYGridNo(pEnemy->position().gridNo(), sMyGrid))
			{
				iThreatValue += (iThreatValue / 20);
			}
		}
	}

	// if this man is conscious
	if (pEnemy->vitals().health() >= OKLIFE)
	{
		// and we were told to reduce threat for my cover		
		if (ubReduceForCover && (!TileIsOutOfBounds(sMyGrid)))
		{
			// Reduce iThreatValue to same % as the chance HE has shoot through at ME
			//iThreatValue = (iThreatValue * ChanceToGetThrough( pEnemy, myGrid, FAKE, ACTUAL, TESTWALLS, 9999, M9PISTOL, NOT_FOR_LOS)) / 100;
			//iThreatValue = (iThreatValue * SoldierTo3DLocationChanceToGetThrough( pEnemy, myGrid, FAKE, ACTUAL, TESTWALLS, 9999, M9PISTOL, NOT_FOR_LOS)) / 100;
			iThreatValue = (iThreatValue * SoldierToLocationChanceToGetThrough( pEnemy, sMyGrid, pMe->position().level(), 0, pMe->identity().id() ) ) / 100;
		}
	}
	else
	{
		// if he's still something of a threat
		if (iThreatValue > 0)
		{
			// drastically reduce his threat value (divide by 5 to 18)
			iThreatValue /= (4 + (OKLIFE - pEnemy->vitals().health()));
		}
	}

	// threat value of any opponent can never drop below 1
	if (iThreatValue < 1)
	{
		iThreatValue = 1;
	}

	//sprintf(tempstr,"%s's iThreatValue = ",pEnemy->identity().name());
	//NumMessage(tempstr,iThreatValue);

#ifdef BETAVERSION	// unnecessary for real release
	// NOTE: maximum is about 200 for a healthy Mike type with a mortar!
	if (iThreatValue > 250)
	{
		sprintf(tempstr,"CalcManThreatValue: WARNING - %d has a very high threat value of %d",pEnemy->identity().id(),iThreatValue);

#ifdef RECORDNET
		fprintf(NetDebugFile,"\t%s\n",tempstr);
#endif

#ifdef TESTVERSION
		PopMessage(tempstr);
#endif

	}
#endif

	return(iThreatValue);
}

INT16 RoamingRange(TacticalActor *pSoldier, INT32 * pusFromGridNo)
{
	BOOL OppPosKnown = FALSE;
	if (CREATURE_OR_BLOODCAT(pSoldier))
	{
		if (pSoldier->aiBehavior().alertStatus() > STATUS_YELLOW)
		{
			*pusFromGridNo = pSoldier->position().gridNo(); // from current position!
			return(MAX_ROAMING_RANGE);
		}
	}
	// sevenfm: no limits for zombies
	if (TacticalActorConditions::isZombie(*pSoldier))
	{
		*pusFromGridNo = pSoldier->position().gridNo(); // from current position!
		return(MAX_ROAMING_RANGE);
	}
	if (pSoldier->aiBehavior().orders() == POINTPATROL || pSoldier->aiBehavior().orders() == RNDPTPATROL)
	{
		// roam near NEXT PATROL POINT, not from where merc starts out
		*pusFromGridNo = pSoldier->aiPlanning().patrolGrid()[pSoldier->aiPlanning().nextPatrolPoint()];
	}
	else
	{
		// roam around where mercs started
		//*pusFromGridNo = pSoldier->position().initialGrid();
		*pusFromGridNo = pSoldier->aiPlanning().patrolGrid()[0];
	}

	//Do we know about any opponent?
	for (UINT16 oppID = 0; oppID < MAX_NUM_SOLDIERS; oppID++)
	{
		if (pSoldier->awareness().opponentKnowledge()[oppID] != NOT_HEARD_OR_SEEN &&  gbPublicOpplist[pSoldier->roster().team()][oppID] != NOT_HEARD_OR_SEEN)
		{
			OppPosKnown = TRUE;
			break;
		}
	}

	switch (pSoldier->aiBehavior().orders())
	{
		// JA2 GOLD: give non-NPCs a 5 tile roam range for cover in combat when being shot at
		// anv: and tanks who are technically NPCs
	case STATIONARY:
		if ((pSoldier->identity().profile() != NO_PROFILE && !ARMED_VEHICLE(pSoldier)) || (pSoldier->aiBehavior().alertStatus() < STATUS_BLACK && !(pSoldier->suppression().underFire())))
		{
			return(0);
		}
		else
		{
			return(5);
		}
	case ONGUARD:
		return(5);
	case CLOSEPATROL:			
		if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
		{
			return(5);
		}
		else
		{
			if (!OppPosKnown)
			{
				return(15);
			}
			else
			{
				return(30);
				//return( MAX_ROAMING_RANGE );
			}
		}
	case POINTPATROL:			
		if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
		{
			return(10);
		}
		else
		{
			if (!OppPosKnown)
			{
				return(20);
			}
			else
			{
				return(40);
				//return( MAX_ROAMING_RANGE );
			}
		}	 // from nextPatrolGrid, not whereIWas
	case RNDPTPATROL:			
		if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
		{
			return(10);
		}
		else
		{
			if (!OppPosKnown)
			{
				return(20);
			}
			else
			{
				//return( 40 );
				return(MAX_ROAMING_RANGE);
			}
		}// from nextPatrolGrid, not whereIWas
	case FARPATROL:				
		if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
		{
			return(15);
		}
		else
		{
			if (!OppPosKnown)
			{
				return(30);
			}
			else
			{
				return(MAX_ROAMING_RANGE);
			}
		}
	case ONCALL:					
		if (pSoldier->aiBehavior().alertStatus() < STATUS_RED)
		{
			return(10);
		}
		else
		{
			if (!OppPosKnown)
			{
				return(30);
			}
			else
			{
				//return(50);
				return(MAX_ROAMING_RANGE);
			}
		}
	case SEEKENEMY:				*pusFromGridNo = pSoldier->position().gridNo(); // from current position!
		return(MAX_ROAMING_RANGE);
	case SNIPER:				return ( 5 );
	default:
#ifdef BETAVERSION
		sprintf(tempstr, "%s has invalid orders = %d", pSoldier->GetName(), pSoldier->aiBehavior().orders());
		PopMessage(tempstr);
#endif
		return(0);
	}
}


void RearrangePocket(TacticalActor *pSoldier, INT8 bPocket1, INT8 bPocket2, UINT8 bPermanent)
{
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"RearrangePocket");
	// NB there's no such thing as a temporary swap for now...
	// 0verhaul:  There is now!  If not permanent, don't lose weapon ready status because the
	// weapon will be restored after the trial situation is finished.
	//SwapObjs( &(pSoldier->inventory()[bPocket1]), &(pSoldier->inventory()[bPocket2]) );
	SwapObjs( pSoldier, bPocket1, bPocket2, bPermanent );

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"RearrangePocket done");
}

BOOLEAN FindBetterSpotForItem( TacticalActor * pSoldier, INT8 bSlot )
{
	// looks for a place in the slots to put an item in a hand or armour
	// position, and moves it there.
	if (bSlot >= BIGPOCKSTART)
	{
		return( FALSE );
	}
	if (pSoldier->inventory()[bSlot].exists() == false)
	{
		// well that's just fine then!
		return( TRUE );
	}

	if(FitsInSmallPocket(&pSoldier->inventory()[bSlot]) == false)
	{
		// then we're looking for a big pocket
		bSlot = FindEmptySlotWithin( pSoldier, BIGPOCKSTART, MEDPOCKFINAL );
	}
	else
	{
		// try a small pocket first
		bSlot = FindEmptySlotWithin( pSoldier, SMALLPOCKSTART, NUM_INV_SLOTS );
		if (bSlot == NO_SLOT)
		{
			bSlot = FindEmptySlotWithin( pSoldier, BIGPOCKSTART, MEDPOCKFINAL );
		}
	}
	if (bSlot == NO_SLOT)
	{
		return( FALSE );
	}
    DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"findbetterspotforitem: swapping items");
	RearrangePocket(pSoldier, HANDPOS, bSlot, FOREVER );		
	return( TRUE );
}

UINT8 GetTraversalQuoteActionID( INT8 bDirection )
{
	switch( bDirection )
	{
		case NORTHEAST: // east
			return( QUOTE_ACTION_ID_TRAVERSE_EAST );

		case SOUTHEAST: // south
			return( QUOTE_ACTION_ID_TRAVERSE_SOUTH );

		case SOUTHWEST: // west
			return( QUOTE_ACTION_ID_TRAVERSE_WEST );

		case NORTHWEST: // north
			return( QUOTE_ACTION_ID_TRAVERSE_NORTH );

		default:
			return( 0 );
	}
}

UINT8 SoldierDifficultyLevel( TacticalActor * pSoldier )
{
	INT8 bDifficultyBase;
	INT8 bDifficulty;

	DebugMsg(TOPIC_JA2AI,DBG_LEVEL_3,String("SoldierDifficultyLevel"));
	// difficulty modifier ranges from 0 to 100
	// and we want to end up with a number between 0 and 4 (4=hardest)
	// to a base of 1, divide by 34 to get a range from 1 to 3
	bDifficultyBase = 1 + ( CalcDifficultyModifier( pSoldier->roster().soldierClass() ) / 34 );

	switch( pSoldier->roster().soldierClass() )
	{
		case SOLDIER_CLASS_ADMINISTRATOR:
		case SOLDIER_CLASS_BANDIT:
			bDifficulty = bDifficultyBase - 1;
			break;

		case SOLDIER_CLASS_ARMY:
			bDifficulty = bDifficultyBase;
			break;

		case SOLDIER_CLASS_ROBOT:
		case SOLDIER_CLASS_ELITE:
			bDifficulty = bDifficultyBase + 1;
			break;

		// hard code militia;
		case SOLDIER_CLASS_GREEN_MILITIA:
			bDifficulty = 2;
			break;

		case SOLDIER_CLASS_REG_MILITIA:
			bDifficulty = 3;
			break;

		case SOLDIER_CLASS_ELITE_MILITIA:
			bDifficulty = 4;
			break;

		case SOLDIER_CLASS_ZOMBIE:
			bDifficulty = bDifficultyBase;
			break;

		default:
			if (pSoldier->roster().team() == CREATURE_TEAM)
			{
				bDifficulty = bDifficultyBase + pSoldier->position().level() / 4;
			}
			else // civ...
			{
				bDifficulty = (bDifficultyBase + pSoldier->position().level() / 4) - 1;
			}
			break;

	}

	bDifficulty = __max( bDifficulty, 0 );
	bDifficulty = __min( bDifficulty, 4 );

	return( (UINT8) bDifficulty );
}

BOOLEAN ValidCreatureTurn( TacticalActor * pCreature, INT8 bNewDirection )
{
	INT8	bDirChange;
	INT8	bTempDir;
	INT8	bLoop;
	BOOLEAN	fFound;

	bDirChange = (INT8) QuickestDirection( pCreature->position().direction(), bNewDirection );

	for( bLoop = 0; bLoop < 2; bLoop++ )
	{
		fFound = TRUE;

		bTempDir = pCreature->position().direction();

		do
		{

			bTempDir += bDirChange;
			if (bTempDir < NORTH)
			{
				bTempDir = NORTHWEST;
			}
			else if (bTempDir > NORTHWEST)
			{
				bTempDir = NORTH;
			}
			if (!TacticalActorMobility::isValidStance(*pCreature,  bTempDir, ANIM_STAND ))
			{
				fFound = FALSE;
				break;
			}

		} while ( bTempDir != bNewDirection );

		if ( fFound )
		{
			break;
		}
		else if ( bLoop > 0 )
		{
			// can't find a dir!
			return( FALSE );
		}
		else
		{
			// try the other direction
			bDirChange = bDirChange * -1;
		}
	}

	return( TRUE );
}

INT32 RangeChangeDesire( TacticalActor * pSoldier )
{
	INT32 iRangeFactorMultiplier;

	iRangeFactorMultiplier = pSoldier->morale().aiMorale() - 1;

	// sevenfm: retreat
	if (TacticalActorAiBehavior::retreatCounter(*pSoldier) > 0)
	{
		return 0;
	}

	// civilians only run away
	if (pSoldier->aiBehavior().neutral())
	{
		return 0;
	}

	INT8 bBonus = -1;
	if (IS_MERC_BODY_TYPE(pSoldier))
	{
		if (!AICheckHasGun(pSoldier))
			bBonus = 2;	// if we have no weapons, try to get closer to enemy
		else if (GuySawEnemy(pSoldier, SEEN_LAST_TURN) && AICheckShortWeaponRange(pSoldier))
			bBonus = 1;	// bonus if weapon range is short
	}

	switch (pSoldier->aiBehavior().attitude())
	{
	case DEFENSIVE:		iRangeFactorMultiplier += max(-1, bBonus); break;
	case BRAVESOLO:		iRangeFactorMultiplier += max(2, bBonus); break;
	case BRAVEAID:		iRangeFactorMultiplier += max(2, bBonus); break;
	case CUNNINGSOLO:	iRangeFactorMultiplier += max(0, bBonus); break;
	case CUNNINGAID:	iRangeFactorMultiplier += max(0, bBonus); break;
	case ATTACKSLAYONLY:
	case AGGRESSIVE:	iRangeFactorMultiplier += max(1, bBonus); break;
	}	

	if ( gTacticalStatus.bConsNumTurnsWeHaventSeenButEnemyDoes > 0 )
	{
		iRangeFactorMultiplier += gTacticalStatus.bConsNumTurnsWeHaventSeenButEnemyDoes;
	}

	return iRangeFactorMultiplier;
}

BOOLEAN ArmySeesOpponents( void )
{
	TacticalActor *		pSoldier;

	for ( SoldierID cnt = gTacticalStatus.Team[ ENEMY_TEAM ].bFirstID; cnt <= gTacticalStatus.Team[ ENEMY_TEAM ].bLastID; ++cnt )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt.i);

		if ( pSoldier &&
			pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() >= OKLIFE && pSoldier->awareness().opponentCount() > 0 )
		{
			return( TRUE );
		}
	}

	return( FALSE );
}

#ifdef DEBUGDECISIONS
void AIPopMessage ( STR16 str )
{
	DebugAI(str);
}

void AIPopMessage ( const STR8	str )
{
	STR tempstr;
	sprintf( tempstr,"%s", str);
	DebugAI(tempstr);
}

void AINumMessage(const STR8	str, INT32 num)
{
	STR tempstr;
	sprintf( tempstr,"%s %d", str, num);
	DebugAI(tempstr);
}

void AINameMessage(TacticalActor * pSoldier,const STR8	str,INT32 num)
{
	STR tempstr;
	sprintf( tempstr,"%d %s %d",pSoldier->GetName() , str, num);
	DebugAI( tempstr );
}
#endif
/////////////////////////////////////////////////////////////////////////////////////////////////
// HEADROCK:
//
// The following function(s) are part of my half-assed attempt to have the AI analyze the tactical
// situation, by comparing (known) squad sizes, the state of all combatants, and the orders of all
// friendlies. The idea is to return a value called "TacticalSituation" which can tell a combatant
// whether he should try to undertake a smarter course of action.
/////////////////////////////////////////////////////////////////////////////////////////////////
/*
INT16 AssessTacticalSituation( INT8 bTeam )
{
	UINT16 ubFriendlyTeamTacticalValue = 0;
	UINT16 ubEnemyTeamTacticalValue = 0;
	UINT8 ubSoldierTacticalThreat;
	INT16 ubTacticalSituation;
	UINT16 cnt;
	TacticalActor * pSoldier;
	
	// begin loop through all MERCs.
	for ( cnt = gTacticalStatus.Team[ OUR_TEAM ].bFirstID; cnt <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; cnt++ )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt);
		ubSoldierTacticalThreat = CalcStraightThreatValue( pSoldier );
		// Player-controlled Mercs are 1.5 times more threatening than AIs
		if (pSoldier->status().flags() & SOLDIER_PC)
			ubSoldierTacticalThreat = (UINT8)((float)ubSoldierTacticalThreat * 1.5);
		
		// Assess Threat
		if (bTeam == OUR_TEAM || bTeam == MILITIA_TEAM)
		{
			// Friendly!
			ubFriendlyTeamTacticalValue += ubSoldierTacticalThreat;
		}
		else
		{
			// Enemy!
			if ( TeamSeesOpponent( ENEMY_TEAM, pSoldier ) )
				ubEnemyTeamTacticalValue += ubSoldierTacticalThreat;

		}
	}

	// begin loop through all Militia.
	for ( cnt = gTacticalStatus.Team[ MILITIA_TEAM ].bFirstID; cnt <= gTacticalStatus.Team[ MILITIA_TEAM ].bLastID; cnt++ )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt);
		ubSoldierTacticalThreat = CalcStraightThreatValue( pSoldier );
		
		// Assess Threat
		if (bTeam == OUR_TEAM || bTeam == MILITIA_TEAM)
		{
			// Friendly!
			ubFriendlyTeamTacticalValue += ubSoldierTacticalThreat;
		}
		else
		{
			// Enemy!
			if ( TeamSeesOpponent( ENEMY_TEAM, pSoldier ) )
				ubEnemyTeamTacticalValue += ubSoldierTacticalThreat;
		}
	}	

	// begin loop through all Enemies.
	for ( cnt = gTacticalStatus.Team[ ENEMY_TEAM ].bFirstID; cnt <= gTacticalStatus.Team[ ENEMY_TEAM ].bLastID; cnt++ )
	{
		pSoldier = GetJa2SoldierRepository().resolve(cnt);
		ubSoldierTacticalThreat = CalcStraightThreatValue( pSoldier );
		
		// Assess Threat

		if (bTeam == ENEMY_TEAM)
		{
			// Friendly!
			ubFriendlyTeamTacticalValue += ubSoldierTacticalThreat;
		}
		else
		{
			// Enemy!
			if ( TeamSeesOpponent( OUR_TEAM, pSoldier ) || TeamSeesOpponent ( MILITIA_TEAM, pSoldier) )
				ubEnemyTeamTacticalValue += ubSoldierTacticalThreat;
		}
		
	}

	ubTacticalSituation = ubEnemyTeamTacticalValue - ubFriendlyTeamTacticalValue;

	return (ubTacticalSituation);


}
*/

// HEADROCK: Function to check whether a team can see the specified soldier.
BOOLEAN TeamSeesOpponent( INT8 bTeam, TacticalActor * pOpponent )
{
	TacticalActor * pSoldier;
	SoldierID cnt;

	// This assertion can be safely removed, assuming the program does what it should. It simply checks
	// whether the "opponent" is on the same team being checked. That should be avoided when calling this
	// function.
	Assert( pOpponent->roster().team() != bTeam );

	// We're checking Merc/Militia visibility
	if (bTeam == OUR_TEAM || bTeam == MILITIA_TEAM )
	{
		for ( cnt = gTacticalStatus.Team[ MILITIA_TEAM ].bFirstID; cnt <= gTacticalStatus.Team[ MILITIA_TEAM ].bLastID; ++cnt )
		{
			pSoldier = GetJa2SoldierRepository().resolve(cnt.i);

			if (pSoldier &&
				pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() >= OKLIFE)
			{


				if (pSoldier->awareness().opponentKnowledge()[ pOpponent->identity().id() ] == SEEN_CURRENTLY)
					return ( TRUE );
			}
		}
		for ( cnt = gTacticalStatus.Team[ OUR_TEAM ].bFirstID; cnt <= gTacticalStatus.Team[ OUR_TEAM ].bLastID; ++cnt )
		{
			pSoldier = GetJa2SoldierRepository().resolve(cnt.i);

			if (pSoldier &&
				pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() >= OKLIFE)
			{
				// This assertion can be safely removed, assuming the program does what it should. It simply checks
				// whether the "opponent" is on the same team being checked. That should be avoided when calling this
				// function.
				//Assert( pOpponent->roster().side() != bSide );

				if (pSoldier->awareness().opponentKnowledge()[ pOpponent->identity().id() ] == SEEN_CURRENTLY)
					return ( TRUE );
			}
		}		
		
		return ( FALSE );
	}
	// Check enemy visibility
	else if (bTeam == ENEMY_TEAM)
	{
		for ( cnt = gTacticalStatus.Team[ ENEMY_TEAM ].bFirstID; cnt <= gTacticalStatus.Team[ ENEMY_TEAM ].bLastID; ++cnt )
		{
			pSoldier = GetJa2SoldierRepository().resolve(cnt.i);

			if (pSoldier &&
				pSoldier->roster().active() && pSoldier->roster().inSector() && pSoldier->vitals().health() >= OKLIFE)
			{
				// This assertion can be safely removed, assuming the program does what it should. It simply checks
				// whether the "opponent" is on the same team being checked. That should be avoided when calling this
				// function.
				//Assert( pOpponent->roster().side() != bSide );

				if (pSoldier->awareness().opponentKnowledge()[ pOpponent->identity().id() ] == SEEN_CURRENTLY)
					return ( TRUE );
			}
		}	
		return ( FALSE );
	}

	else
		return (FALSE);

}

// HEADROCK: Function to assess an enemy's threat value without "me" argument.
INT32 CalcStraightThreatValue( TacticalActor *pEnemy )
{
	INT32	iThreatValue = 0;

	// If man is inactive, at base, on assignment, dead, unconscious
	if (!pEnemy->roster().active() || !pEnemy->roster().inSector() || !pEnemy->vitals().health() )
	{
		// he's no threat at all, return a negative number
		iThreatValue = 0;
		return(iThreatValue);
	}

	else
	{
		// ADD twice the man's level (2-20)
		iThreatValue += EffectiveExpLevel(pEnemy); // SANDRO - find precise effective exp level

		// ADD man's total action points (10-35)
		iThreatValue += 25 * TacticalActorTurnBudget::calculateTurnGrant(*pEnemy) / APBPConstants[AP_MAXIMUM];

		// ADD 1/2 of man's current action points (4-17)
		iThreatValue += 25 * pEnemy->actionPoints().current() / APBPConstants[AP_MAXIMUM] / 2;

		// ADD 1/10 of man's current health (0-10)
		iThreatValue += (pEnemy->vitals().health() / 10);

		if (pEnemy->assignment().current() < ON_DUTY )
		{
			// ADD 1/4 of man's protection percentage (0-25)
			iThreatValue += ArmourPercent( pEnemy ) / 4;

			// ADD 1/5 of man's marksmanship skill (0-20)
			iThreatValue += (pEnemy->statistics().marksmanship() / 5);

			if ( Item[ pEnemy->inventory()[HANDPOS].usItem ].usItemClass & IC_WEAPON )
			{
				// ADD the deadliness of the item(weapon) he's holding (0-50)
				iThreatValue += Weapon[pEnemy->inventory()[HANDPOS].usItem].ubDeadliness;
			}
		}

		// SUBTRACT 1/5 of man's bleeding (0-20)
		iThreatValue -= (pEnemy->vitals().bleeding() / 5);

		// SUBTRACT 1/10 of man's breath deficiency (0-10)
		iThreatValue -= ((100 - pEnemy->vitals().breath()) / 10);

		// SUBTRACT man's current shock value
		iThreatValue -= pEnemy->suppression().shock();
	}

	// if this man is conscious
	if (pEnemy->vitals().health() < OKLIFE)
	{
		// if he's still something of a threat
		if (iThreatValue > 0)
		{
			// drastically reduce his threat value (divide by 5 to 18)
			iThreatValue /= (4 + (OKLIFE - pEnemy->vitals().health()));
		}
	}

	// threat value of any opponent can never drop below 1
	if (iThreatValue < 0)
	{
		iThreatValue = 0;
	}

	return(iThreatValue);
}

// Flugente: get the id of the closest soldier with a specific flag that we can currently see
SoldierID GetClosestFlaggedSoldierID( TacticalActor * pSoldier, INT16 aRange, UINT8 auTeam, UINT32 aFlag, BOOLEAN fCheckSight )
{
	SoldierID			id = NOBODY;
	TacticalActor *		pFriend;
	INT16				range = aRange;

	// go through each soldier, looking for "friends" (soldiers on same team)
	for ( SoldierID uiLoop = gTacticalStatus.Team[ auTeam ].bFirstID; uiLoop <= gTacticalStatus.Team[ auTeam ].bLastID; ++uiLoop)
	{
		pFriend = GetJa2SoldierRepository().resolve(uiLoop.i);

		// if this merc is inactive, not in sector, or dead
		if (!pFriend)
			continue;
								
		// skip ourselves
		if (pFriend->identity().id() == pSoldier->identity().id())
			continue;

		// must be on the same level
		if ( pFriend->position().level() != pSoldier->position().level() )
			continue;

		// this is not for tanks
		if ( ARMED_VEHICLE( pFriend ) || ENEMYROBOT( pFriend ))
			continue;
		
		// skip if this guy is dead
		if ( pFriend->vitals().health() <= 0 )
			continue;

		// check for flag
		if ( !(pFriend->featureFlags().primaryFlags() & aFlag) )
			continue;

		// are we close enough?
		INT16 friendrange = SpacesAway(pSoldier->position().gridNo(), pFriend->position().gridNo());
		if ( friendrange < range)
		{
			// can we see this guy?
			if ( !fCheckSight || SoldierTo3DLocationLineOfSightTest( pSoldier, pFriend->position().gridNo(), pSoldier->position().level(), 3, TRUE, CALC_FROM_WANTED_DIR ) )
			{
				range = friendrange;
				id = pFriend->identity().id();
			}
		}
	}
		
	return id;
}

// get the id of the closest soldier (closer than x tiles) of a specific team that is wounded that we can currently see
SoldierID GetClosestWoundedSoldierID( TacticalActor * pSoldier, INT16 aRange, UINT8 auTeam )
{
	SoldierID			id = NOBODY;
	TacticalActor *		pFriend;
	INT16				range = aRange;

	// go through each soldier, looking for "friends" (soldiers on same team)
	for ( SoldierID uiLoop = gTacticalStatus.Team[ auTeam ].bFirstID; uiLoop <= gTacticalStatus.Team[ auTeam ].bLastID; ++uiLoop)
	{
		pFriend = GetJa2SoldierRepository().resolve(uiLoop.i);

		// if this merc is inactive, not in sector, or dead
		if (!pFriend)
			continue;
								
		// skip ourselves (if not allowed)
		if ( !gGameExternalOptions.fEnemyMedicsHealSelf && pFriend->identity().id() == pSoldier->identity().id())
			continue;

		// must be on the same level
		if ( pFriend->position().level() != pSoldier->position().level() )
			continue;

		// this is not for tanks
		if ( ARMED_VEHICLE( pFriend ) || ENEMYROBOT( pFriend ) )
			continue;
		
		// skip if this guy is dead, or not wounded (enough)
		if ( pFriend->vitals().health() <= 0 || pFriend->vitals().healableInjury() < gGameExternalOptions.sEnemyMedicsWoundMinAmount )
			continue;

		// are we close enough?
		INT16 friendrange = SpacesAway(pSoldier->position().gridNo(), pFriend->position().gridNo());
		if ( friendrange < range)
		{
			// can we see this guy?
			//if ( SoldierTo3DLocationLineOfSightTest( pSoldier, pFriend->sGridNo, pSoldier->position().level(), 3, TRUE, CALC_FROM_WANTED_DIR ) )
			{
				range = friendrange;
				id = pFriend->identity().id();
			}
		}
	}
		
	return id;
}

// get the id of the closest medic (closer than x tiles) of a specific team
SoldierID GetClosestMedicSoldierID( TacticalActor * pSoldier, INT16 aRange, UINT8 auTeam )
{
	SoldierID			id = NOBODY;
	TacticalActor *		pFriend;
	INT16				range = aRange;

	// go through each soldier, looking for "friends" (soldiers on same team)
	for ( SoldierID uiLoop = gTacticalStatus.Team[ auTeam ].bFirstID; uiLoop <= gTacticalStatus.Team[ auTeam ].bLastID; ++uiLoop)
	{
		pFriend = GetJa2SoldierRepository().resolve(uiLoop.i);

		// if this merc is inactive, not in sector, or dead
		if (!pFriend)
			continue;
								
		// skip ourselves (we seek OTHER people)
		if ( pFriend->identity().id() == pSoldier->identity().id())
			continue;

		// must be on the same level
		if ( pFriend->position().level() != pSoldier->position().level() )
			continue;

		// this is not for tanks
		if ( ARMED_VEHICLE( pFriend ) || ENEMYROBOT( pFriend ) )
			continue;

		// skip this guy if he is dead or unconscious
		if ( pFriend->vitals().health() < OKLIFE )
			continue;
		
		// skip if this guy if he is no medic
		if (!TacticalActorMedicalServices::
				canTreatForAi(*pFriend))
			continue;

		// are we close enough?
		INT16 friendrange = SpacesAway(pSoldier->position().gridNo(), pFriend->position().gridNo());
		if ( friendrange < range)
		{
			// can we see this guy?
			//if ( SoldierTo3DLocationLineOfSightTest( pSoldier, pFriend->sGridNo, pSoldier->position().level(), 3, TRUE, CALC_FROM_WANTED_DIR ) )
			{
				range = friendrange;
				id = pFriend->identity().id();
			}
		}
	}
		
	return id;
}

// sevenfm: define normal vision limits for day/night
INT16 MaxNormalVisionDistance( void )
{
	if( NightTime() )
	{
		return gGameExternalOptions.ubStraightSightRange * STRAIGHT_RATIO;
	}
	return gGameExternalOptions.ubStraightSightRange * 2 * STRAIGHT_RATIO;
}

// sevenfm: check friendly soldiers between me and noise gridno
// count only friends that are active and not stationary/onguard/sniper
UINT16 CountFriendsInDirection( TacticalActor *pSoldier, INT32 sTargetGridNo )
{
	TacticalActor * pFriend;
	UINT8 ubFriendDir, ubMyDir;
	UINT16 ubFriends = 0;

	CHECKF(pSoldier);

	ubMyDir = GetDirectionFromCenterCellXYGridNo(sTargetGridNo, pSoldier->position().gridNo());

	// Run through each friendly.
	for ( SoldierID iCounter = gTacticalStatus.Team[ pSoldier->roster().team() ].bFirstID ; iCounter <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID ; ++iCounter )
	{
		pFriend = GetJa2SoldierRepository().resolve(iCounter.i);
		if (!pFriend)
		{
			continue;
		}
		ubFriendDir = GetDirectionFromCenterCellXYGridNo(sTargetGridNo, pFriend->position().gridNo());

		if (pFriend != pSoldier &&
			pFriend->roster().active() &&
			pFriend->vitals().health() >= OKLIFE &&
			pFriend->vitals().health() >= pFriend->vitals().maximumHealth()/2 &&
			pFriend->aiBehavior().orders() > ONGUARD &&
			pFriend->aiBehavior().orders() != SNIPER &&
			(ubFriendDir == ubMyDir || ubFriendDir == gOneCDirection[ubMyDir] || ubFriendDir == gOneCCDirection[ubMyDir]) &&
			PythSpacesAway( sTargetGridNo, pFriend->position().gridNo()) < PythSpacesAway(sTargetGridNo, pSoldier->position().gridNo()) )
		{
			ubFriends++;
		}
	}

	return ubFriends;
}

// sevenfm: count nearby friend soldiers
UINT16 CountNearbyFriends( TacticalActor *pSoldier, INT32 sGridNo, UINT8 ubDistance )
{
	TacticalActor * pFriend;
	UINT16 ubFriendCount = 0;

	// safety check
	if( !pSoldier )
		return 0;

	// Run through each friendly.
	for ( SoldierID iCounter = gTacticalStatus.Team[ pSoldier->roster().team() ].bFirstID ; iCounter <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID ; ++iCounter )
	{
		pFriend = GetJa2SoldierRepository().resolve(iCounter.i);
		// Make sure that character is alive, not too shocked, and conscious, and of higher experience level
		// than the character being suppressed.
		if (pFriend && pFriend != pSoldier && pFriend->roster().active() && pFriend->vitals().health() >= OKLIFE &&
			PythSpacesAway( sGridNo, pFriend->position().gridNo() ) <= ubDistance )
		{
			ubFriendCount++;
		}
	}

	return ubFriendCount;
}

// sevenfm: new AI morale calculation
INT8 CalcMoraleNew(TacticalActor *pSoldier)
{
	UINT32	uiLoop, uiLoop2;
	INT32	iOurTotalThreat = 0, iTheirTotalThreat = 0;
	INT16	sOppThreatValue, sFrndThreatValue, sMorale;
	INT32	iPercent;
	INT8	bMostRecentOpplistValue;
	INT8	bMoraleCategory;
	UINT8	*pSeenOpp; //,*friendOlPtr;
	INT8	*pbPersOL, *pbPublOL;
	TacticalActor *pOpponent, *pFriend;

	// zombies always have high morale
	if (TacticalActorConditions::isZombie(*pSoldier))
	{
		return MORALE_FEARLESS;
	}

	// sevenfm: retreat, also when blinded
	if (TacticalActorAiBehavior::retreatCounter(*pSoldier) > 0 || pSoldier->perception().isBlinded())
	{
		return MORALE_WORRIED;
	}

	// sevenfm: neutrals always have low AI morale even if they have weapons (so they run from enemy)
	if (pSoldier->aiBehavior().neutral())
	{
		return(MORALE_WORRIED);
	}

	// if army guy has NO weapons left then panic!
	if ( pSoldier->roster().team() == ENEMY_TEAM || !pSoldier->aiBehavior().neutral() )
	{
		if ( FindAIUsableObjClass( pSoldier, IC_WEAPON ) == NO_SLOT )
		{
			// sevenfm: instead of leaving sector, try to attack with hands/knife
			return( MORALE_FEARLESS );
			//return( MORALE_HOPELESS );
		}
	}

	// hang pointers to my personal opplist, my team's public opplist, and my
	// list of previously seen opponents
	pSeenOpp	= (UINT8 *) &(gbSeenOpponents[pSoldier->identity().id()][0]);

	// loop through every one of my possible opponents
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || (pOpponent->vitals().health() < OKLIFE))
			continue;			// next merc

		// if this merc is neutral/on same side, he's not an opponent, skip him!
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()))
			continue;			// next merc

		// Special stuff for Carmen the bounty hunter
		if (pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY && pOpponent->identity().profile() != SLAY)
		{
			continue;	// next opponent
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();
		pbPublOL = gbPublicOpplist[pSoldier->roster().team()] + pOpponent->identity().id();
		pSeenOpp = (UINT8 *)gbSeenOpponents[pSoldier->identity().id()] + pOpponent->identity().id();

		// if this opponent is unknown to me personally AND unknown to my team, too
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			// if I have never seen him before anywhere in this sector, either
			if (!(*pSeenOpp))
				continue;		// next merc

			// have seen him in the past, so he remains something of a threat
			bMostRecentOpplistValue = 0;		// uses the free slot for 0 opplist
		}
		else		 // decide which opplist is more current
		{
			// if personal knowledge is more up to date or at least equal
			if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) || (*pbPersOL == *pbPublOL))
				bMostRecentOpplistValue = *pbPersOL;		// use personal
			else
				bMostRecentOpplistValue = *pbPublOL;		// use public
		}

		iPercent = ThreatPercent[bMostRecentOpplistValue - OLDEST_HEARD_VALUE];

		sOppThreatValue = (iPercent * CalcManThreatValue(pOpponent,pSoldier->position().gridNo(),FALSE,pSoldier)) / 100;

		//sprintf(tempstr,"Known opponent %s, opplist status %d, percent %d, threat = %d",
		//			ExtMen[pOpponent->identity().id()].name,ubMostRecentOpplistValue,ubPercent,sOppThreatValue);
		//PopMessage(tempstr);

		// ADD this to their running total threatValue (decreases my MORALE)
		iTheirTotalThreat += sOppThreatValue;
		//NumMessage("Their TOTAL threat now = ",sTheirTotalThreat);

		// NOW THE FUN PART: SINCE THIS OPPONENT IS KNOWN TO ME IN SOME WAY,
		// ANY FRIENDS OF MINE THAT KNOW ABOUT HIM BOOST MY MORALE.	SO, LET'S GO
		// THROUGH THEIR PERSONAL OPPLISTS AND CHECK WHICH OF MY FRIENDS KNOW
		// SOMETHING ABOUT HIM AND WHAT THEIR THREAT VALUE TO HIM IS.

		for (uiLoop2 = 0; uiLoop2 < Ja2ActiveTacticalActorSlotCount(); uiLoop2++)
		{
			pFriend = ResolveJa2ActiveTacticalActorSlot(uiLoop2);

			// if this merc is inactive, at base, on assignment, dead, unconscious
			if (!pFriend || (pFriend->vitals().health() < OKLIFE))
				continue;		// next merc

			// if this merc is not on my side, then he's NOT one of my friends

			// WE CAN'T AFFORD TO CONSIDER THE ENEMY OF MY ENEMY MY FRIEND, HERE!
			// ONLY IF WE ARE ACTUALLY OFFICIALLY CO-OPERATING TOGETHER (SAME SIDE)
			if ( pFriend->aiBehavior().neutral() && !( pSoldier->roster().civilianGroup() != NON_CIV_GROUP && pSoldier->roster().civilianGroup() == pFriend->roster().civilianGroup() ) )
			{
				continue;		// next merc
			}

			if ( pSoldier->roster().side() != pFriend->roster().side() )
				continue;		// next merc

			// THIS TEST IS INVALID IF A COMPUTER-TEAM IS PLAYING CO-OPERATIVELY
			// WITH A NON-COMPUTER TEAM SINCE THE OPPLISTS INVOLVED ARE NOT
			// UP-TO-DATE.	THIS SITUATION IS CURRENTLY NOT POSSIBLE IN HTH/DG.

			// ALSO NOTE THAT WE COUNT US AS OUR (BEST) FRIEND FOR THESE CALCULATIONS

			// subtract HEARD_2_TURNS_AGO (which is negative) to make values start at 0 and
			// be positive otherwise
			iPercent = ThreatPercent[pFriend->awareness().opponentKnowledge()[pOpponent->identity().id()] - OLDEST_HEARD_VALUE];

			// reduce the percentage value based on how far away they are from the enemy, if they only hear him
			if ( pFriend->awareness().opponentKnowledge()[ pOpponent->identity().id() ] <= HEARD_LAST_TURN )
			{
				iPercent -= PythSpacesAway( pSoldier->position().gridNo(), pFriend->position().gridNo() ) * 2;
				if ( iPercent <= 0 )
				{
					//ignore!
					continue;
				}
			}

			sFrndThreatValue = (iPercent * CalcManThreatValue(pFriend,pOpponent->position().gridNo(),FALSE,pSoldier)) / 100;

			//sprintf(tempstr,"Known by friend %s, opplist status %d, percent %d, threat = %d",
			//		 ExtMen[pFriend->identity().id()].name,pFriend->awareness().opponentKnowledge()[pOpponent->identity().id()],ubPercent,sFrndThreatValue);
			//PopMessage(tempstr);

			// ADD this to our running total threatValue (increases my MORALE)
			// We multiply by sOppThreatValue to PRO-RATE this based on opponent's
			// threat value to ME personally.	Divide later by sum of them all.
			iOurTotalThreat += sOppThreatValue * sFrndThreatValue;
		}

		// this could get slow if I have a lot of friends...
		//KeepInterfaceGoing();
	}


	// if they are no threat whatsoever
	if (!iTheirTotalThreat)
		sMorale = 500;		// our morale is just incredible
	else
	{
		// now divide sOutTotalThreat by sTheirTotalThreat to get the REAL value
		iOurTotalThreat /= iTheirTotalThreat;

		// calculate the morale (100 is even, < 100 is us losing, > 100 is good)
		sMorale = (INT16) ((100 * iOurTotalThreat) / iTheirTotalThreat);
	}

	if (sMorale <= 25)				// odds 1:4 or worse
		bMoraleCategory = MORALE_HOPELESS;
	else if (sMorale <= 50)		 // odds between 1:4 and 1:2
		bMoraleCategory = MORALE_WORRIED;
	else if (sMorale <= 150)		// odds between 1:2 and 3:2
		bMoraleCategory = MORALE_NORMAL;
	else if (sMorale <= 300)		// odds between 3:2 and 3:1
		bMoraleCategory = MORALE_CONFIDENT;
	else							// odds better than 3:1
		bMoraleCategory = MORALE_FEARLESS;

	switch (pSoldier->aiBehavior().attitude())
	{
	case DEFENSIVE:	bMoraleCategory--; break;
	case BRAVESOLO:	bMoraleCategory += 2; break;
	case BRAVEAID:	bMoraleCategory += 2; break;
	case CUNNINGSOLO:	break;
	case CUNNINGAID:	 break;
	case AGGRESSIVE:	bMoraleCategory++; break;
	}

	// make idiot administrators more aggressive
	if ( pSoldier->roster().soldierClass() == SOLDIER_CLASS_ADMINISTRATOR || pSoldier->roster().soldierClass() == SOLDIER_CLASS_BANDIT )
	{
		bMoraleCategory += 2;
	}

	// if have good health
	if( pSoldier->vitals().health() > pSoldier->vitals().maximumHealth() / 2 )
	{
		bMoraleCategory++;
	}
	// bad health
	if( pSoldier->vitals().health() < pSoldier->vitals().maximumHealth() / 4 )
	{
		bMoraleCategory--;
	}
	// good breath
	if( pSoldier->vitals().breath() > 50 )
	{
		bMoraleCategory++;
	}
	// bad breath
	if( pSoldier->vitals().breath() < 25 )
	{
		bMoraleCategory--;
	}
	// if not under fire - attack
	if( !pSoldier->suppression().underFire() )
	{
		bMoraleCategory++;
	}

	// count friends that flank around the same spot
	if( CountFriendsFlankSameSpot( pSoldier ) == 0 )
	{
		bMoraleCategory ++;
	}

	INT32 sClosestOpponent = ClosestKnownOpponent(pSoldier, NULL, NULL);

	// if last attack of this soldier hit enemy - increase morale
	if( pSoldier->combatResult().lastAttackHit() )
	{
		bMoraleCategory++;
	}

	// if some friend hit enemy - increase morale
	if (CountNearbyFriendsLastAttackHit(pSoldier, pSoldier->position().gridNo(), TACTICAL_RANGE / 4) > 0)
	{
		bMoraleCategory++;
	}

	// limit AI morale when soldier is under heavy fire
	/*if (TacticalActorConditions::suppressionShockPercent(*pSoldier) > 75)
		bMoraleCategory = min(bMoraleCategory, MORALE_NORMAL);
	else if (TacticalActorConditions::suppressionShockPercent(*pSoldier) > 50)
		bMoraleCategory = min(bMoraleCategory, MORALE_CONFIDENT);*/

	// limit AI morale depending on morale and shock level
	bMoraleCategory = min(bMoraleCategory, max(MORALE_WORRIED, ((pSoldier->aiBehavior().orders() == SEEKENEMY ? pSoldier->morale().morale() + 20 : pSoldier->morale().morale()) * 100 / (100 + TacticalActorConditions::suppressionShockPercent(*pSoldier))) / 20));

	// prevent hopeless morale when not under attack
	if (bMoraleCategory == MORALE_HOPELESS && !pSoldier->suppression().underFire())
	{
		bMoraleCategory = MORALE_WORRIED;
	}

	// check limits
	bMoraleCategory = max(bMoraleCategory, MORALE_HOPELESS);
	bMoraleCategory = min(bMoraleCategory, MORALE_FEARLESS);

	return(bMoraleCategory);
}

BOOLEAN AICheckSpecialRole(TacticalActor *pSoldier)
{
	if (AICheckIsSniper(pSoldier) || AICheckIsMachinegunner(pSoldier) || AICheckIsMortarOperator(pSoldier) || AICheckIsRadioOperator(pSoldier) || AICheckIsCommander(pSoldier))
		return TRUE;

	return FALSE;
}

BOOLEAN WeAttack(INT8 bTeam)
{
	if (bTeam >= MAXTEAMS)
	{
		return FALSE;
	}

	if (bTeam != ENEMY_TEAM)
	{
		return FALSE;
	}

	// check that every soldier has SEEKENEMY order
	TacticalActor * pFriend;

	// Run through each friendly.
	for ( SoldierID iCounter = gTacticalStatus.Team[bTeam].bFirstID; iCounter <= gTacticalStatus.Team[bTeam].bLastID; ++iCounter )
	{
		pFriend = GetJa2SoldierRepository().resolve(iCounter.i);

		if (pFriend &&
			pFriend->roster().active() &&
			pFriend->vitals().health() >= OKLIFE &&
			pFriend->aiBehavior().orders() != SEEKENEMY)
		{
			return FALSE;
		}
	}

	return TRUE;
}

UINT8 CountNearbyFriendsLastAttackHit( TacticalActor *pSoldier, INT32 sGridNo, UINT8 ubDistance )
{
	CHECKF(pSoldier);

	TacticalActor * pFriend;
	UINT8 ubFriendCount = 0;

	// Run through each friendly.
	for ( SoldierID iCounter = gTacticalStatus.Team[ pSoldier->roster().team() ].bFirstID ; iCounter <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID ; ++iCounter )
	{
		pFriend = GetJa2SoldierRepository().resolve(iCounter.i);

		if (pFriend && pFriend != pSoldier &&
			pFriend->roster().active() &&
			pFriend->vitals().health() >= OKLIFE &&
			pFriend->aiBehavior().orders() > ONGUARD &&
			pFriend->aiBehavior().orders() != SNIPER &&
			PythSpacesAway( sGridNo, pFriend->position().gridNo() ) <= ubDistance &&
			pFriend->combatResult().lastAttackHit() )
		{
			ubFriendCount++;
		}
	}

	return ubFriendCount;
}

UINT8 CountFriendsFlankSameSpot(TacticalActor *pSoldier, INT32 sSpot)
{
	CHECKF(pSoldier);

	TacticalActor * pFriend;
	UINT8 ubFriendCount = 0;

	UINT8 ubFlankLeft = 0;
	UINT8 ubFlankRight = 0;

	if (TileIsOutOfBounds(sSpot))
	{
		sSpot = ClosestKnownOpponent(pSoldier, NULL, NULL);
	}

	if (TileIsOutOfBounds(sSpot))
	{
		return 0;
	}

	// Run through each friendly.
	for ( SoldierID iCounter = gTacticalStatus.Team[pSoldier->roster().team()].bFirstID; iCounter <= gTacticalStatus.Team[pSoldier->roster().team()].bLastID; ++iCounter )
	{
		pFriend = GetJa2SoldierRepository().resolve(iCounter.i);

		if (pFriend &&
			pFriend != pSoldier &&
			pFriend->roster().active() &&
			pFriend->vitals().health() >= OKLIFE &&
			pFriend->aiBehavior().alertStatus() == STATUS_RED &&
			pFriend->aiBehavior().orders() > ONGUARD &&
			pFriend->aiBehavior().orders() != SNIPER)
		{
			// check if this friend flanks around the same spot
			if (pFriend->aiPlanning().flankCount() > 0 && pFriend->aiPlanning().flankCount() < MAX_FLANKS_RED &&
				PythSpacesAway(pFriend->aiPlanning().flankAnchorGrid(), sSpot) < VISION_RANGE / 2)
			{
				if (pFriend->aiPlanning().lastFlankLeft())
				{
					ubFlankLeft++;
				}
				else
				{
					ubFlankRight++;
				}
			}
		}
	}

	return ubFlankLeft + ubFlankRight;
}

// check that soldier is flanking
BOOLEAN AICheckIsFlanking( TacticalActor *pSoldier )
{
	CHECKF(pSoldier);

	if( pSoldier->aiBehavior().alertStatus() < STATUS_YELLOW ||
		pSoldier->aiPlanning().flankCount() == 0 ||
		pSoldier->aiPlanning().flankCount() >= MAX_FLANKS_RED )
	{
		return FALSE;
	}

	return TRUE;
}

// sevenfm: determine minimum flanking directions to stop flanking depending on soldier's attitude
UINT8 MinFlankDirections( TacticalActor *pSoldier )
{
	CHECKF(pSoldier);

	switch(pSoldier->aiBehavior().attitude())
	{
	case CUNNINGAID:
	case CUNNINGSOLO:
		return 4;		
	}
	return 2;
}

// count mobile friends that are in BLACK state and not in a dangerous place or have 3/4 APs or hit enemy recently
// this is mostly used to check if we can cross dangerous area (in light at night or fresh corpses)
UINT8 CountFriendsBlack( TacticalActor *pSoldier, INT32 sClosestOpponent )
{
	CHECKF(pSoldier);

	TacticalActor * pFriend;
	UINT8 ubFriendCount = 0;
	INT32 sFriendClosestOpponent;

	// by default, use closest known opponent
	if( sClosestOpponent == NOWHERE )
	{
		sClosestOpponent = ClosestKnownOpponent( pSoldier, NULL, NULL );
	}

	if(TileIsOutOfBounds(sClosestOpponent))
	{
		return 0;
	}

	// Run through each friendly.
	for ( SoldierID iCounter = gTacticalStatus.Team[ pSoldier->roster().team() ].bFirstID ; iCounter <= gTacticalStatus.Team[ pSoldier->roster().team() ].bLastID ; ++iCounter )
	{
		pFriend = GetJa2SoldierRepository().resolve(iCounter.i);

		// Make sure that character is alive, not too shocked, and conscious
		if (pFriend && pFriend != pSoldier &&
			pFriend->roster().active() &&
			pFriend->vitals().health() >= OKLIFE)
		{
			//sFriendClosestOpponent = ClosestKnownOpponent( pFriend, NULL, NULL );
			sFriendClosestOpponent = ClosestSeenOpponent( pFriend, NULL, NULL );
			if(!TileIsOutOfBounds(sFriendClosestOpponent) &&
				PythSpacesAway( sClosestOpponent, sFriendClosestOpponent ) < (INT16)TACTICAL_RANGE / 4 &&
				pFriend->aiBehavior().alertStatus() == STATUS_BLACK &&
				pFriend->vitals().health() > pFriend->vitals().maximumHealth() / 2 &&
				( GetNearestRottingCorpseAIWarning( pFriend->position().gridNo() ) == 0 && !InLightAtNight(pFriend->position().gridNo(), pFriend->position().level()) ||
				pFriend->actionPoints().current() > 3*pFriend->actionPoints().initial()/4 ||
				pFriend->combatResult().lastAttackHit() )
				)
			{
				ubFriendCount++;
			}
		}
	}

	return ubFriendCount;
}

// count friends under fire or with shock
UINT16 CountTeamUnderAttack(INT8 bTeam, INT32 sGridNo, INT16 sDistance)
{
	TacticalActor * pFriend;
	UINT16 ubFriendCount = 0;

	// safety check
	if (bTeam >= MAXTEAMS)
		return 0;

	// Run through each friendly.
	for ( SoldierID iCounter = gTacticalStatus.Team[bTeam].bFirstID; iCounter <= gTacticalStatus.Team[bTeam].bLastID; ++iCounter )
	{
		pFriend = GetJa2SoldierRepository().resolve(iCounter.i);

		if (pFriend &&
			pFriend->roster().active() &&
			pFriend->vitals().health() >= OKLIFE &&
			PythSpacesAway(sGridNo, pFriend->position().gridNo()) <= sDistance &&
			(pFriend->suppression().underFire() || pFriend->suppression().shock() > 0))
		{
			ubFriendCount++;
		}
	}

	return ubFriendCount;
}

// check if we have a prone sight cover from known enemies at spot
BOOLEAN ProneSightCoverAtSpot(TacticalActor *pSoldier, INT32 sSpot, BOOLEAN fUnlimited)
{
	CHECKF(pSoldier);

	UINT32		uiLoop;
	TacticalActor *pOpponent;
	INT32		*pusLastLoc;
	INT8		*pbPersOL;
	INT8		*pbPublOL;
	INT8		*pbLastLevel;

	INT32		sThreatLoc;
	//INT32		iThreatCertainty;
	INT8		iThreatLevel;

	INT32 iDistanceVisible;

	// look through all opponents for those we know of
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || pOpponent->vitals().health() < OKLIFE)
		{
			continue;			// next merc
		}

		// if this man is neutral / on the same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()))
		{
			continue;			// next merc
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();
		pbPublOL = gbPublicOpplist[pSoldier->roster().team()] + pOpponent->identity().id();

		pusLastLoc = gsLastKnownOppLoc[pSoldier->identity().id()] + pOpponent->identity().id();
		pbLastLevel = gbLastKnownOppLevel[pSoldier->identity().id()] + pOpponent->identity().id();

		// if this opponent is unknown personally and publicly
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			continue;			// next merc
		}

		// if personal knowledge is more up to date or at least equal
		if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) ||
			(*pbPersOL == *pbPublOL))
		{
			// using personal knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = *pusLastLoc;
			iThreatLevel = *pbLastLevel;
			//iThreatCertainty = ThreatPercent[*pbPersOL - OLDEST_HEARD_VALUE];
		}
		else
		{
			// using public knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = gsPublicLastKnownOppLoc[pSoldier->roster().team()][pOpponent->identity().id()];
			iThreatLevel = gbPublicLastKnownOppLevel[pSoldier->roster().team()][pOpponent->identity().id()];
			//iThreatCertainty = ThreatPercent[*pbPublOL - OLDEST_HEARD_VALUE];
		}

		if (TileIsOutOfBounds(sThreatLoc))
		{
			continue;
		}

		if (fUnlimited)
		{
			iDistanceVisible = NO_DISTANCE_LIMIT;
		}
		else
		{
			gbForceWeaponReady = true;
			iDistanceVisible = TacticalActorVisibility::distance(*pSoldier, DIRECTION_IRRELEVANT, DIRECTION_IRRELEVANT, sSpot, pSoldier->position().level(), TacticalActorConditions::isCowering(*pSoldier), GetPercentTunnelVision(pSoldier));
			gbForceWeaponReady = false;
			//iDistanceVisible = TacticalActorVisibility::maximumDistance(*pSoldier, sSpot, pSoldier->position().level(), CALC_FROM_WANTED_DIR);
		}

		if (LocationToLocationLineOfSightTest(sThreatLoc, iThreatLevel, sSpot, pSoldier->position().level(), TRUE, iDistanceVisible, STANDING_LOS_POS, PRONE_LOS_POS))
		{
			return FALSE;
		}
	}

	return TRUE;
}

// check if we have a sight cover from known enemies at spot
BOOLEAN SightCoverAtSpot(TacticalActor *pSoldier, INT32 sSpot, BOOLEAN fUnlimited)
{
	CHECKF(pSoldier);

	UINT32		uiLoop;
	TacticalActor *pOpponent;
	INT32		*pusLastLoc;
	INT8		*pbPersOL;
	INT8		*pbPublOL;
	INT8		*pbLastLevel;

	INT32		sThreatLoc;
	//INT32		iThreatCertainty;
	INT8		iThreatLevel;

	INT32 iDistanceVisible;

	// look through all opponents for those we know of
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || pOpponent->vitals().health() < OKLIFE)
		{
			continue;			// next merc
		}

		// if this man is neutral / on the same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()))
		{
			continue;			// next merc
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();
		pbPublOL = gbPublicOpplist[pSoldier->roster().team()] + pOpponent->identity().id();

		pusLastLoc = gsLastKnownOppLoc[pSoldier->identity().id()] + pOpponent->identity().id();
		pbLastLevel = gbLastKnownOppLevel[pSoldier->identity().id()] + pOpponent->identity().id();

		// if this opponent is unknown personally and publicly
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			continue;			// next merc
		}

		// if personal knowledge is more up to date or at least equal
		if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) ||
			(*pbPersOL == *pbPublOL))
		{
			// using personal knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = *pusLastLoc;
			iThreatLevel = *pbLastLevel;
			//iThreatCertainty = ThreatPercent[*pbPersOL - OLDEST_HEARD_VALUE];
		}
		else
		{
			// using public knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = gsPublicLastKnownOppLoc[pSoldier->roster().team()][pOpponent->identity().id()];
			iThreatLevel = gbPublicLastKnownOppLevel[pSoldier->roster().team()][pOpponent->identity().id()];
			//iThreatCertainty = ThreatPercent[*pbPublOL - OLDEST_HEARD_VALUE];
		}

		if (TileIsOutOfBounds(sThreatLoc))
		{
			continue;
		}

		if(fUnlimited)
		{
			iDistanceVisible = NO_DISTANCE_LIMIT;
		}
		else
		{
			gbForceWeaponReady = true;
			iDistanceVisible = TacticalActorVisibility::distance(*pSoldier, DIRECTION_IRRELEVANT, DIRECTION_IRRELEVANT, sSpot, pSoldier->position().level(), TacticalActorConditions::isCowering(*pSoldier), GetPercentTunnelVision(pSoldier));
			gbForceWeaponReady = false;
			//iDistanceVisible = TacticalActorVisibility::maximumDistance(*pSoldier, sSpot, pSoldier->position().level(), CALC_FROM_WANTED_DIR);
		}		

		if (LocationToLocationLineOfSightTest(sThreatLoc, iThreatLevel, sSpot, pSoldier->position().level(), TRUE, iDistanceVisible))
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOLEAN EnemySeenSoldierRecently( TacticalActor *pSoldier, UINT8 ubMax )
{
	UINT32		uiLoop;
	TacticalActor *pOpponent;

	//loop through all the enemies and determine the cover
	for ( uiLoop = 0; uiLoop<Ja2ActiveTacticalActorSlotCount(); ++uiLoop )
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if ( !pOpponent || pOpponent->vitals().health() < OKLIFE )
		{
			continue;			// next merc
		}

		// if this man is neutral / on the same side, he's not an opponent
		if ( CONSIDERED_NEUTRAL( pSoldier, pOpponent ) || (pSoldier->roster().side() == pOpponent->roster().side()) )
		{
			continue;			// next merc
		}

		// sevenfm: ignore empty vehicles
		if ( pOpponent->employment().mercenaryType() == MERC_TYPE__VEHICLE && GetNumberInVehicle( pOpponent->vehicleState().tacticalVehicleId() ) == 0 )
		{
			continue;
		}

		// if opponent is collapsed/breath collapsed
		if ( pOpponent->collapseState().tactical() || pOpponent->collapseState().breathTriggered() )
		{
			continue;
		}

		// check that this opponent sees us
		if ( pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] >= SEEN_CURRENTLY &&
			 pOpponent->awareness().opponentKnowledge()[pSoldier->identity().id()] <= ubMax )
		{
			return(TRUE);
		}
	}

	return FALSE;
}

UINT16 CountTeamSeeSoldier( INT8 bTeam, TacticalActor *pSoldier )
{
	TacticalActor *pFriend;
	UINT16 ubFriends = 0;

	CHECKF( pSoldier );

	if ( bTeam >= MAXTEAMS )
		return 0;

	for ( SoldierID cnt = gTacticalStatus.Team[bTeam].bFirstID; cnt <= gTacticalStatus.Team[bTeam].bLastID; ++cnt )
	{
		pFriend = GetJa2SoldierRepository().resolve(cnt.i);

		if ( pFriend &&
			 pFriend->roster().active() &&
			 pFriend->roster().inSector() &&
			 pFriend->vitals().health() >= OKLIFE &&
			 !pFriend->collapseState().tactical() &&
			 !pFriend->collapseState().breathTriggered() )
		{
			if ( pFriend->awareness().opponentKnowledge()[pSoldier->identity().id()] == SEEN_CURRENTLY ||
				 pFriend->awareness().opponentKnowledge()[pSoldier->identity().id()] == SEEN_THIS_TURN )
			{
				++ubFriends;
			}
		}
	}

	return ubFriends;
}

BOOLEAN CheckDoorAtGridno(UINT32 usGridNo)
{
	STRUCTURE *pStructure;

	pStructure = FindStructure(usGridNo, STRUCTURE_ANYDOOR);
	if (pStructure != NULL)
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN CheckDoorNearGridno(UINT32 usGridNo)
{
	UINT8	ubMovementCost;
	INT32	sTempGridNo;
	UINT8	ubDirection;

	if (CheckDoorAtGridno(usGridNo))
	{
		return TRUE;
	}

	for (ubDirection = 0; ubDirection < NUM_WORLD_DIRECTIONS; ubDirection++)
	{
		sTempGridNo = NewGridNo(usGridNo, DirectionInc(ubDirection));

		if (sTempGridNo != usGridNo)
		{
			ubMovementCost = gubWorldMovementCosts[sTempGridNo][ubDirection][0];
			if (IS_TRAVELCOST_DOOR(ubMovementCost))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOLEAN FindBombNearby( TacticalActor *pSoldier, INT32 sGridNo, UINT8 ubDistance )
{
	UINT32	uiBombIndex;

	// sevenfm/opt: nothing to find if there are no bombs at all
	if (guiNumWorldBombs == 0)
	{
		return FALSE;
	}

	INT16 sMaxLeft, sMaxRight, sMaxUp, sMaxDown;

	// determine maximum horizontal limits
	sMaxLeft  = min( ubDistance, (sGridNo % MAXCOL));
	sMaxRight = min( ubDistance, MAXCOL - ((sGridNo % MAXCOL) + 1));

	// determine maximum vertical limits
	sMaxUp   = min( ubDistance, (sGridNo / MAXROW));
	sMaxDown = min( ubDistance, MAXROW - ((sGridNo / MAXROW) + 1));

	// opt: inverted loop -- iterate the bomb list once and box-test each armed,
	// visible, same-level bomb against [sGridNo +/- ubDistance].  Same (tile,bomb)
	// match set and same first-match short-circuit as the original neighbour scan.
	const INT32 sCenterCol = sGridNo % MAXCOL;
	const INT32 sCenterRow = sGridNo / MAXROW;

	for (uiBombIndex = 0; uiBombIndex < guiNumWorldBombs; ++uiBombIndex)
	{
		if (gWorldBombs[ uiBombIndex ].fExists &&
			gWorldItems[ gWorldBombs[ uiBombIndex ].iItemIndex ].ubLevel == pSoldier->position().level() &&
			gWorldItems[ gWorldBombs[ uiBombIndex ].iItemIndex ].bVisible == VISIBLE &&
			gWorldItems[ gWorldBombs[ uiBombIndex ].iItemIndex ].usFlags & WORLD_ITEM_ARMED_BOMB )
		{
			const INT32 sBombGridNo = gWorldItems[ gWorldBombs[ uiBombIndex ].iItemIndex ].sGridNo;
			const INT32 sColOffset = (sBombGridNo % MAXCOL) - sCenterCol;
			const INT32 sRowOffset = (sBombGridNo / MAXROW) - sCenterRow;

			// inside the (edge-clamped) box that the original neighbour loop visited
			if (sColOffset >= -sMaxLeft && sColOffset <= sMaxRight &&
				sRowOffset >= -sMaxUp   && sRowOffset <= sMaxDown)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

// danger percent based on distance to closest smoke effect
UINT8 RedSmokeDanger(INT32 sGridNo, INT8 bLevel)
{
	UINT32	uiCnt;
	INT32	sDist;
	INT32	sClosestDist;
	INT32	sMaxDist = min(gSkillTraitValues.usVOMortarRadius, TACTICAL_RANGE);
	INT32	sClosestSmoke = NOWHERE;
	UINT8	ubDangerPercent = 0;

	if (TileIsOutOfBounds(sGridNo))
	{
		return 0;
	}

	if (!gSkillTraitValues.fROAllowArtillery)
	{
		return 0;
	}

	// no artillery strike danger underground
	if (gbWorldSectorZ > 0)
	{
		return 0;
	}

	// check if artillery strike was ordered by any team
	if (!CheckArtilleryStrike())
	{
		return 0;
	}

	// no danger when in a building
	if (bLevel == 0 && CheckRoof(sGridNo))
	{
		return 0;
	}

	// deep water should be safe
	if (DeepWater(sGridNo, bLevel))
	{
		return 0;
	}

	// no danger when in dense terrain
	if (bLevel == 0 && TerrainDensity(sGridNo, bLevel, 2, FALSE) >= 20)
	{
		return 0;
	}

	//loop through all red smoke effects and find closest
	for (uiCnt = 0; uiCnt < guiNumSmokeEffects; uiCnt++)
	{
		if (gSmokeEffectData[uiCnt].fAllocated &&
			gSmokeEffectData[uiCnt].bType == SIGNAL_SMOKE_EFFECT &&
			!TileIsOutOfBounds(gSmokeEffectData[uiCnt].sGridNo))
		{
			sDist = PythSpacesAway(gSmokeEffectData[uiCnt].sGridNo, sGridNo);

			if (sClosestSmoke == NOWHERE || sDist < sClosestDist)
			{
				sClosestDist = sDist;
				sClosestSmoke = gSmokeEffectData[uiCnt].sGridNo;
			}
		}
	}

	// if we found red smoke, calculate danger percent based on distance
	// 0% at DAY_VISION_RANGE/2, 100% at zero range
	if (sClosestSmoke != NOWHERE)
	{
		ubDangerPercent = 100 * (sMaxDist - min(sMaxDist, sClosestDist)) / sMaxDist;
	}

	return ubDangerPercent;
}

// check if artillery strike was ordered by any team
BOOLEAN CheckArtilleryStrike(void)
{
	UINT32	uiBombIndex;
	OBJECTTYPE *pObj;

	// search all bombs
	for (uiBombIndex = 0; uiBombIndex < guiNumWorldBombs; uiBombIndex++)
	{
		if (gWorldBombs[uiBombIndex].fExists &&
			gWorldItems[gWorldBombs[uiBombIndex].iItemIndex].usFlags & WORLD_ITEM_ARMED_BOMB)
		{
			pObj = &(gWorldItems[gWorldBombs[uiBombIndex].iItemIndex].object);

			if (pObj && pObj->exists() && (*pObj)[0]->data.ubWireNetworkFlag & ANY_ARTILLERY_FLAG)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

BOOLEAN CheckRoof(INT32 sGridNo)
{
	if (FindStructure(sGridNo, STRUCTURE_ROOF) != NULL)
	{
		return TRUE;
	}

	return FALSE;
}


BOOLEAN	FindNearbyExplosiveStructure(INT32 sSpot, INT8 bLevel)
{
	INT32	sTempGridNo;
	UINT8	ubDirection;

	if (TileIsOutOfBounds(sSpot))
	{
		return 0;
	}

	// check adjacent reachable tiles
	for (ubDirection = 0; ubDirection < NUM_WORLD_DIRECTIONS; ubDirection++)
	{
		sTempGridNo = NewGridNo(sSpot, DirectionInc(ubDirection));

		if (sTempGridNo != sSpot)
		{
			if (FindStructFlag(sTempGridNo, bLevel, STRUCTURE_EXPLOSIVE))
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}

UINT8 TerrainDensity(INT32 sSpot, INT8 bLevel, UINT8 ubDistance, BOOLEAN fGrass)
{
	if (TileIsOutOfBounds(sSpot))
		return 0;

	INT16	sMaxLeft, sMaxRight, sMaxUp, sMaxDown, sXOffset, sYOffset;
	INT32	sCheckSpot = NOWHERE;
	INT32	sCountSpots = 0;
	INT32	sCountObstacles = 0;
	UINT16	usRoom1 = 0, usRoom2 = 0;	// init: InARoom leaves the out-param untouched when it returns FALSE, and they're compared below

	STRUCTURE	*pCurrent;
	INT16	sDesiredLevel;

	// determine maximum horizontal limits
	sMaxLeft = min(ubDistance, (sSpot % MAXCOL));
	sMaxRight = min(ubDistance, MAXCOL - ((sSpot % MAXCOL) + 1));

	// determine maximum vertical limits
	sMaxUp = min(ubDistance, (sSpot / MAXROW));
	sMaxDown = min(ubDistance, MAXROW - ((sSpot / MAXROW) + 1));

	// count obstacles
	for (sYOffset = -sMaxUp; sYOffset <= sMaxDown; sYOffset++)
	{
		for (sXOffset = -sMaxLeft; sXOffset <= sMaxRight; sXOffset++)
		{
			sCheckSpot = sSpot + sXOffset + (MAXCOL * sYOffset);

			if (TileIsOutOfBounds(sCheckSpot))
			{
				continue;
			}

			if (InARoom(sSpot, &usRoom1) != InARoom(sCheckSpot, &usRoom2) || usRoom1 != usRoom2)
			{
				continue;
			}

			sCountSpots++;

			if (!IsLocationSittableExcludingPeople(sCheckSpot, bLevel))
			{
				sCountObstacles++;
			}

			if (fGrass && IsLocationSittableExcludingPeople(sCheckSpot, bLevel))
			{
				pCurrent = gpWorldLevelData[sCheckSpot].pStructureHead;

				if (bLevel > 0)
					sDesiredLevel = STRUCTURE_ON_ROOF;
				else
					sDesiredLevel = STRUCTURE_ON_GROUND;

				if (pCurrent != NULL &&
					pCurrent->sCubeOffset == sDesiredLevel &&
					pCurrent->pDBStructureRef->pDBStructure->ubArmour == 4)	// light vegetation
				{
					sCountObstacles++;
				}
			}
		}
	}

	if (sCountSpots > 0)
		return 100 * sCountObstacles / sCountSpots;
	else
		return 0;
}

INT16 DistanceToClosestActiveOpponent(TacticalActor *pSoldier, INT32 sSpot)
{
	INT32		sGridNo;
	UINT32		uiLoop;
	INT8		bLevel;
	TacticalActor *pOpponent;
	INT16		sDistance, sClosestDistance = -1;

	if (!pSoldier || TileIsOutOfBounds(sSpot))
	{
		return 0;
	}

	// look through this man's personal & public opplists for opponents known
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, or dead
		if (!pOpponent)
		{
			continue;			// next merc
		}

		if (!ValidOpponent(pSoldier, pOpponent))
		{
			continue;
		}

		if (pOpponent->vitals().health() < OKLIFE)
		{
			continue;
		}

		// if this opponent is unknown personally and publicly
		if (pSoldier->awareness().opponentKnowledge()[pOpponent->identity().id()] == NOT_HEARD_OR_SEEN)
		{
			continue;
		}

		// obtain opponent's location and level
		sGridNo = gsLastKnownOppLoc[pSoldier->identity().id()][pOpponent->identity().id()];
		bLevel = gbLastKnownOppLevel[pSoldier->identity().id()][pOpponent->identity().id()];

		if (TileIsOutOfBounds(sGridNo))
		{
			continue;
		}

		sDistance = PythSpacesAway(sSpot, sGridNo);

		if (sClosestDistance < 0 ||
			sDistance < sClosestDistance)
		{
			sClosestDistance = sDistance;
		}
	}

	return sClosestDistance;
}

BOOLEAN ValidOpponent(TacticalActor* pSoldier, TacticalActor* pOpponent)
{
	if (!pSoldier || !pOpponent)
	{
		return FALSE;
	}

	if (!pOpponent->roster().active() ||
		!pOpponent->roster().inSector() ||
		pOpponent->vitals().health() == 0 ||
		CONSIDERED_NEUTRAL(pSoldier, pOpponent) ||
		pSoldier->roster().side() == pOpponent->roster().side() ||
		pSoldier->aiBehavior().attitude() == ATTACKSLAYONLY && pOpponent->identity().profile() != SLAY ||
		(pOpponent->employment().mercenaryType() == MERC_TYPE__VEHICLE && GetNumberInVehicle(pOpponent->vehicleState().tacticalVehicleId()) == 0) ||
		gTacticalStatus.bBoxingState == BOXING && (pSoldier->status().flags() & SOLDIER_BOXER) && !(pOpponent->status().flags() & SOLDIER_BOXER) ||
		pOpponent->identity().bodyType() == CROW)
	{
		return FALSE;
	}

	return TRUE;
}

BOOLEAN AnyCoverFromSpot( INT32 sSpot, INT8 bLevel, INT32 sThreatLoc, INT8 bThreatLevel )
{
	UINT8	ubDirection;
	INT32	sCoverSpot;
	INT8	bCoverHeight;

	if( TileIsOutOfBounds( sSpot ) || TileIsOutOfBounds(sThreatLoc) )
	{
		return FALSE;
	}

	ubDirection = GetDirectionFromCenterCellXYGridNo(sSpot, sThreatLoc);
	sCoverSpot = NewGridNo( sSpot, DirectionInc( ubDirection ) );

	if ( TileIsOutOfBounds( sCoverSpot ) )
	{
		return FALSE;
	}

	if ( WhoIsThere2( sCoverSpot, bLevel ) != NOBODY )
	{
		return FALSE;
	}

	if ( IsLocationSittable( sCoverSpot, bLevel ) )
	{
		return FALSE;
	}

	bCoverHeight = GetTallestStructureHeight( sCoverSpot, bLevel );

	if ( bCoverHeight >= 2 )
	{
		return TRUE;
	}

	return FALSE;
}

UINT16 CountSeenEnemiesLastTurn( TacticalActor* pSoldier )
{
	CHECKF(pSoldier);

	UINT8	ubTeamLoop;
	UINT16	cnt = 0;

	for( ubTeamLoop = 0; ubTeamLoop < MAXTEAMS; ubTeamLoop++ )
	{
		if( !gTacticalStatus.Team[ubTeamLoop].bTeamActive )
			continue;

		if( gTacticalStatus.Team[ ubTeamLoop ].bSide != pSoldier->roster().side() )
		{
			// consider guys in this team, which isn't on our side
			for( SoldierID ubIDLoop = gTacticalStatus.Team[ ubTeamLoop ].bFirstID; ubIDLoop <= gTacticalStatus.Team[ ubTeamLoop ].bLastID; ++ubIDLoop )
			{
				// if this guy SAW an enemy recently...
				if( pSoldier->awareness().opponentKnowledge()[ ubIDLoop ] >= SEEN_CURRENTLY &&
					pSoldier->awareness().opponentKnowledge()[ ubIDLoop ] <= SEEN_LAST_TURN )
				{
					cnt++;
				}
			}
		}
	}

	return cnt;
}

BOOLEAN NorthSpot(INT32 sSpot, INT8 bLevel)
{
	if (TileIsOutOfBounds(sSpot))
		return FALSE;

	if (bLevel > 0)
		return FALSE;

	INT32 sNextSpot = NewGridNo(sSpot, DirectionInc(NORTHWEST));

	if (gubWorldMovementCosts[sSpot + DirectionInc(NORTHWEST)][NORTHWEST][0] == TRAVELCOST_OFF_MAP ||
		gubWorldMovementCosts[sNextSpot + DirectionInc(NORTHWEST)][NORTHWEST][0] == TRAVELCOST_OFF_MAP)
	{
		return TRUE;
	}

	return FALSE;
}

// use soldier AI - merc bodytype, no robots/tanks/boxers/etc
BOOLEAN SoldierAI(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	BOOLEAN fCivilian = (PTR_CIVILIAN && (pSoldier->roster().civilianGroup() == NON_CIV_GROUP ||
		(pSoldier->aiBehavior().neutral() && gTacticalStatus.fCivGroupHostile[pSoldier->roster().civilianGroup()] == CIV_GROUP_NEUTRAL) ||
		(pSoldier->identity().bodyType() >= FATCIV && pSoldier->identity().bodyType() <= CRIPPLECIV)));

	if (!IS_MERC_BODY_TYPE(pSoldier) || 
		pSoldier->aiBehavior().neutral() ||
		fCivilian ||
		pSoldier->status().flags() & SOLDIER_BOXER ||
		ARMED_VEHICLE(pSoldier) ||
		pSoldier->status().flags() & SOLDIER_VEHICLE ||
		AM_A_ROBOT(pSoldier) ||
		ENEMYROBOT(pSoldier))
		return FALSE;

	return TRUE;
}

UINT8 SpotDangerLevel(TacticalActor *pSoldier, INT32 sGridNo)
{
	if (!pSoldier)
		return 0;

	if (TileIsOutOfBounds(sGridNo))
		return 0;

	BOOLEAN fAlerted = FALSE;
	BOOLEAN fGreen = FALSE;
	BOOLEAN fProfile = FALSE;
	BOOLEAN fNeutral = FALSE;
	UINT8 ubLevel = 0;

	if (pSoldier->aiBehavior().alertStatus() < STATUS_YELLOW)
		fGreen = TRUE;

	if (pSoldier->identity().profile() != NO_PROFILE)
		fProfile = TRUE;

	if (pSoldier->aiBehavior().neutral())
		fNeutral = TRUE;

	if ((pSoldier->aiBehavior().alertStatus() >= STATUS_RED || pSoldier->roster().soldierClass() == SOLDIER_CLASS_ELITE))
		fAlerted = TRUE;

	if (!fProfile && !fNeutral && !gGameExternalOptions.fAITacticalRetreat && NorthSpot(sGridNo, pSoldier->position().level()) ||
		!fProfile && fGreen && CheckDoorNearGridno(sGridNo) ||
		Water(sGridNo, pSoldier->position().level()) && !TacticalActorAiBehavior::isFlanking(*pSoldier) ||
		CorpseWarning(pSoldier, sGridNo, pSoldier->position().level()))
		ubLevel = 1;

	if (fAlerted && !fNeutral && 
		(InLightAtNight(sGridNo, pSoldier->position().level()) || FindNearbyExplosiveStructure(sGridNo, pSoldier->position().level())))
		ubLevel = 2;

	if (DeepWater(sGridNo, pSoldier->position().level()) && !TacticalActorAiBehavior::isFlanking(*pSoldier) ||
		RedSmokeDanger(sGridNo, pSoldier->position().level()))
		ubLevel = 3;

	if (InGas(pSoldier, sGridNo) ||
		FindBombNearby(pSoldier, sGridNo, BOMB_DETECTION_RANGE))
		ubLevel = 4;

	return ubLevel;
}

BOOLEAN CheckNPCDestination(TacticalActor *pSoldier, INT32 sGridNo)
{
	if (!pSoldier)
		return FALSE;

	if (TileIsOutOfBounds(sGridNo))
		return FALSE;

	// find current danger level
	UINT8 ubLevel = SpotDangerLevel(pSoldier, pSoldier->position().gridNo());
	// find danger level at target spot
	UINT8 ubTargetLevel = SpotDangerLevel(pSoldier, sGridNo);

	// avoid moving into dangerous spot
	if (ubTargetLevel > 0 && ubTargetLevel >= ubLevel)
		return FALSE;

	return TRUE;
}

BOOLEAN AllowDeepWaterFlanking(TacticalActor *pSoldier)
{
	if (SoldierAI(pSoldier) &&
		pSoldier->roster().team() == ENEMY_TEAM &&
		pSoldier->aiBehavior().orders() == SEEKENEMY &&
		(pSoldier->aiBehavior().attitude() == CUNNINGSOLO || gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT(pSoldier, ATHLETICS_NT)) &&
		pSoldier->aiBehavior().alertStatus() >= STATUS_RED &&
		!pSoldier->suppression().underFire() &&
		!GuySawEnemy(pSoldier))
	{
		return TRUE;
	}

	return FALSE;
}

UINT8 AIDirection(INT32 sSpot1, INT32 sSpot2)
{
	if (TileIsOutOfBounds(sSpot1) || TileIsOutOfBounds(sSpot2))
	{
		return DIRECTION_IRRELEVANT;
	}

	return GetDirectionFromCenterCellXYGridNo(sSpot1, sSpot2);
}

BOOLEAN AICheckIsSniper(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	/*if( AIGunType(pSoldier) != GUN_SN_RIFLE )
	{
	return FALSE;
	}*/

	if ((AIGunRange(pSoldier) > TACTICAL_RANGE || AICheckHasWeaponOfType(pSoldier, GUN_SN_RIFLE)) &&
		AIGunScoped(pSoldier) &&
		pSoldier->statistics().marksmanship() > 90 &&
		gGameOptions.fNewTraitSystem &&
		HAS_SKILL_TRAIT(pSoldier, SNIPER_NT))
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN AICheckIsMarksman(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	if (!AICheckHasGun(pSoldier))
	{
		return FALSE;
	}

	if (AIGunType(pSoldier) != GUN_SN_RIFLE &&
		AIGunType(pSoldier) != GUN_RIFLE &&
		AIGunType(pSoldier) != GUN_AS_RIFLE)
	{
		return FALSE;
	}

	if (AIGunRange(pSoldier) >= TACTICAL_RANGE &&
		(AIGunScoped(pSoldier) || pSoldier->statistics().marksmanship() > 90 || gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT(pSoldier, SNIPER_NT)))
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN AICheckIsRadioOperator(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	if (TacticalActorRadio::canUse(*pSoldier, false))
		return TRUE;

	return FALSE;
}

BOOLEAN AICheckIsMedic(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	if (TacticalActorMedicalServices::
			canTreatForAi(*pSoldier) &&
		gGameOptions.fNewTraitSystem &&
		HAS_SKILL_TRAIT(pSoldier, DOCTOR_NT))
	{
		return TRUE;
	}

	/*if( !HAS_SKILL_TRAIT( pSoldier, DOCTOR_NT) )
	{
	return FALSE;
	}

	if( FindFirstAidKit( pSoldier ) != NO_SLOT ||
	FindMedKit( pSoldier ) != NO_SLOT )
	{
	return TRUE;
	}*/

	return FALSE;
}

BOOLEAN AICheckIsMortarOperator(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	if (TacticalActorEquipment::hasMortar(*pSoldier))
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN AICheckIsGLOperator(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	// find GL
	INT8 bWeaponIn = FindAIUsableObjClass(pSoldier, IC_LAUNCHER);
	if (bWeaponIn != NO_SLOT &&
		(EnoughAmmo(pSoldier, FALSE, bWeaponIn) || FindAmmoToReload(pSoldier, bWeaponIn, NO_SLOT) != NO_SLOT))
	{
		return TRUE;
	}

	// check for attached GL
	INT8 bGunSlot = FindAIUsableObjClass(pSoldier, IC_GUN);
	INT8 bRealWeaponMode = pSoldier->attackSelection().weaponMode();
	pSoldier->attackSelection().weaponMode() = WM_ATTACHED_GL;		// So that EnoughAmmo will check for a grenade not a bullet
	if (bGunSlot != NO_SLOT &&
		IsGrenadeLauncherAttached(&pSoldier->inventory()[bGunSlot]) &&
		(EnoughAmmo(pSoldier, FALSE, bGunSlot) || FindAmmoToReload(pSoldier, bGunSlot, NO_SLOT) != NO_SLOT))
	{
		pSoldier->attackSelection().weaponMode() = bRealWeaponMode;
		return TRUE;
	}
	pSoldier->attackSelection().weaponMode() = bRealWeaponMode;

	return FALSE;
}

BOOLEAN AICheckIsOfficer(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	if (gGameOptions.fNewTraitSystem && HAS_SKILL_TRAIT(pSoldier, SQUADLEADER_NT))
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN AICheckIsCommander(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	if (gGameOptions.fNewTraitSystem && NUM_SKILL_TRAITS(pSoldier, SQUADLEADER_NT) > 1)
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN AICheckIsMachinegunner(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!SoldierAI(pSoldier))
	{
		return FALSE;
	}

	if (AIGunType(pSoldier) == GUN_LMG)
	{
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AIGunInHandScoped(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (UsingNewCTHSystem() == false && IsScoped(&pSoldier->inventory()[HANDPOS]))
	{
		return TRUE;
	}

	if (UsingNewCTHSystem() == true && NCTHIsScoped(&pSoldier->inventory()[HANDPOS]))
	{
		return TRUE;
	}
	return FALSE;
}

// check if gun that AI can use is scoped
BOOLEAN AIGunScoped(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	INT8 bWeaponIn;

	bWeaponIn = FindAIUsableObjClass(pSoldier, IC_GUN);

	if (bWeaponIn == NO_SLOT)
	{
		return FALSE;
	}

	if (pSoldier->inventory()[bWeaponIn].exists())
	{
		if (UsingNewCTHSystem())
		{
			return NCTHIsScoped(&pSoldier->inventory()[bWeaponIn]);
		}
		else
		{
			return IsScoped(&pSoldier->inventory()[bWeaponIn]);
		}
	}

	return FALSE;
}

// return range for the AI gun in tiles
UINT16 AIGunRange(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	INT8 bWeaponIn;

	bWeaponIn = FindAIUsableObjClass(pSoldier, IC_GUN);

	if (bWeaponIn == NO_SLOT)
	{
		return 0;
	}

	if (pSoldier->inventory()[bWeaponIn].exists())
	{
		return GunRange(&pSoldier->inventory()[bWeaponIn], pSoldier) / CELL_X_SIZE;
	}
	return 0;
}

UINT16 AIGunClass(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	INT8 bWeaponIn;

	bWeaponIn = FindAIUsableObjClass(pSoldier, IC_GUN);

	if (bWeaponIn == NO_SLOT)
	{
		return 0;
	}

	if (pSoldier->inventory()[bWeaponIn].exists())
	{
		return Weapon[pSoldier->inventory()[bWeaponIn].usItem].ubWeaponClass;
	}
	return 0;
}

UINT16 AIGunType(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	INT8 bWeaponIn;

	bWeaponIn = FindAIUsableObjClass(pSoldier, IC_GUN);

	if (bWeaponIn == NO_SLOT)
	{
		return 0;
	}

	if (pSoldier->inventory()[bWeaponIn].exists())
	{
		return Weapon[pSoldier->inventory()[bWeaponIn].usItem].ubWeaponType;
	}
	return 0;
}

UINT8 AIGunDeadliness(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	INT8 bWeaponIn;

	bWeaponIn = FindAIUsableObjClass(pSoldier, IC_GUN);

	if (bWeaponIn == NO_SLOT)
	{
		return 0;
	}

	if (pSoldier->inventory()[bWeaponIn].exists())
	{
		return Weapon[pSoldier->inventory()[bWeaponIn].usItem].ubDeadliness;
	}

	return 0;
}

UINT16 AIGunAmmo(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	INT8 bWeaponIn = FindAIUsableObjClass(pSoldier, IC_GUN);

	if (bWeaponIn == NO_SLOT)
	{
		return 0;
	}

	if (pSoldier->inventory()[bWeaponIn].exists())
	{
		return pSoldier->inventory()[bWeaponIn][0]->data.gun.ubGunShotsLeft;
	}

	return 0;
}

BOOLEAN AIGunAutofireCapable(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	INT8 bWeaponIn = FindAIUsableObjClass(pSoldier, IC_GUN);

	if (bWeaponIn == NO_SLOT)
	{
		return FALSE;
	}

	if (pSoldier->inventory()[bWeaponIn].exists())
	{
		return IsGunAutofireCapable(&pSoldier->inventory()[bWeaponIn]);
	}

	return FALSE;
}

BOOLEAN FindObstacleNearSpot(INT32 sSpot, INT8 bLevel)
{
	if (TileIsOutOfBounds(sSpot))
	{
		return FALSE;
	}
	/*if( !IsLocationSittableExcludingPeople(sSpot, bLevel) )
	{
	return FALSE;
	}*/

	UINT8	ubMovementCost;
	INT32	sTempGridNo;
	UINT8	ubDirection;

	// check adjacent reachable tiles
	for (ubDirection = 0; ubDirection < NUM_WORLD_DIRECTIONS; ubDirection++)
	{
		sTempGridNo = NewGridNo(sSpot, DirectionInc(ubDirection));

		if (sTempGridNo != sSpot)
		{
			ubMovementCost = gubWorldMovementCosts[sTempGridNo][ubDirection][bLevel];

			if (ubMovementCost >= TRAVELCOST_BLOCKED || !IsLocationSittableExcludingPeople(sTempGridNo, bLevel))
			{
				return(TRUE);
			}
		}
	}

	return FALSE;
}

BOOLEAN InSmoke(INT32 sGridNo, INT8 bLevel)
{
	if (TileIsOutOfBounds(sGridNo))
	{
		return FALSE;
	}

	if (gpWorldLevelData[sGridNo].ubExtFlags[bLevel] & (MAPELEMENT_EXT_SMOKE))
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN InSmokeNearby(INT32 sGridNo, INT8 bLevel)
{
	if (TileIsOutOfBounds(sGridNo))
	{
		return FALSE;
	}

	if (gpWorldLevelData[sGridNo].ubExtFlags[bLevel] & (MAPELEMENT_EXT_SMOKE))
	{
		return TRUE;
	}

	UINT8 ubDirection;
	INT32 sTempGridNo;
	UINT8 ubMovementCost;

	// check adjacent reachable tiles
	for (ubDirection = 0; ubDirection < NUM_WORLD_DIRECTIONS; ubDirection++)
	{
		sTempGridNo = NewGridNo(sGridNo, DirectionInc(ubDirection));

		if (sTempGridNo != sGridNo)
		{
			ubMovementCost = gubWorldMovementCosts[sTempGridNo][ubDirection][bLevel];

			if (ubMovementCost < TRAVELCOST_BLOCKED &&
				gpWorldLevelData[sTempGridNo].ubExtFlags[bLevel] & (MAPELEMENT_EXT_SMOKE))
			{
				return(TRUE);
			}
		}
	}
	return FALSE;
}

BOOLEAN CorpseWarning(TacticalActor *pSoldier, INT32 sGridNo, INT8 bLevel)
{
	CHECKF(pSoldier);

	INT32			cnt;
	ROTTING_CORPSE *pCorpse;
	UINT8			ubDistance = CORPSE_WARNING_DIST;

	for (cnt = 0; cnt < giNumRottingCorpse; ++cnt)
	{
		pCorpse = &(gRottingCorpse[cnt]);

		if (pCorpse &&
			pCorpse->fActivated &&
			pCorpse->def.ubType < ROTTING_STAGE2 &&
			//pCorpse->def.ubType <= FMERC_FALLF &&
			pCorpse->def.ubBodyType <= REGFEMALE &&
			pCorpse->def.ubAIWarningValue > 0 &&
			pCorpse->def.bLevel == bLevel &&
			!TileIsOutOfBounds(pCorpse->def.sGridNo) &&
			PythSpacesAway(sGridNo, pCorpse->def.sGridNo) <= ubDistance &&
			(pSoldier->roster().team() == ENEMY_TEAM && CorpseEnemyTeam(pCorpse) || pSoldier->roster().team() == MILITIA_TEAM && CorpseMilitiaTeam(pCorpse) || pSoldier->roster().team() == CIV_TEAM && !pSoldier->aiBehavior().neutral()))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN CorpseEnemyTeam(ROTTING_CORPSE *pCorpse)
{
	CHECKF(pCorpse);

	// check whether corpse has soldier's uniform
	for (UINT8 i = UNIFORM_ENEMY_ADMIN; i <= UNIFORM_ENEMY_ELITE; ++i)
	{
		if (COMPARE_PALETTEREP_ID(pCorpse->def.VestPal, gUniformColors[i].vest) && COMPARE_PALETTEREP_ID(pCorpse->def.PantsPal, gUniformColors[i].pants))
		{
			return TRUE;
		}
	}

	return FALSE;
}

BOOLEAN CorpseMilitiaTeam(ROTTING_CORPSE *pCorpse)
{
	CHECKF(pCorpse);

	// check whether corpse has soldier's uniform
	for (UINT8 i = UNIFORM_MILITIA_ROOKIE; i <= UNIFORM_MILITIA_ELITE; ++i)
	{
		if (COMPARE_PALETTEREP_ID(pCorpse->def.VestPal, gUniformColors[i].vest) && COMPARE_PALETTEREP_ID(pCorpse->def.PantsPal, gUniformColors[i].pants))
		{
			return TRUE;
		}
	}

	return FALSE;
}

// check that current loaded sector is underground
BOOLEAN AICheckUnderground(void)
{
	// not underground
	if (gbWorldSectorZ > 0)
	{
		return TRUE;
	}

	return FALSE;
}

// check if we have any sight cover from known enemies at spot
BOOLEAN AnyCoverAtSpot(TacticalActor *pSoldier, INT32 sSpot)
{
	CHECKF(pSoldier);

	UINT32		uiLoop;
	TacticalActor *pOpponent;
	INT32		*pusLastLoc;
	INT8		*pbPersOL;
	INT8		*pbPublOL;
	INT8		*pbLastLevel;

	INT32		sThreatLoc;
	//INT32		iThreatCertainty;
	INT8		iThreatLevel;

	// look through all opponents for those we know of
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || pOpponent->vitals().health() < OKLIFE)
		{
			continue;			// next merc
		}

		// if this man is neutral / on the same side, he's not an opponent
		if (CONSIDERED_NEUTRAL(pSoldier, pOpponent) || (pSoldier->roster().side() == pOpponent->roster().side()))
		{
			continue;			// next merc
		}

		pbPersOL = pSoldier->awareness().opponentKnowledge() + pOpponent->identity().id();
		pbPublOL = gbPublicOpplist[pSoldier->roster().team()] + pOpponent->identity().id();

		pusLastLoc = gsLastKnownOppLoc[pSoldier->identity().id()] + pOpponent->identity().id();
		pbLastLevel = gbLastKnownOppLevel[pSoldier->identity().id()] + pOpponent->identity().id();

		// if this opponent is unknown personally and publicly
		if ((*pbPersOL == NOT_HEARD_OR_SEEN) && (*pbPublOL == NOT_HEARD_OR_SEEN))
		{
			continue;			// next merc
		}

		// if personal knowledge is more up to date or at least equal
		// sevenfm: fix for unknown public location
		if ((gubKnowledgeValue[*pbPublOL - OLDEST_HEARD_VALUE][*pbPersOL - OLDEST_HEARD_VALUE] > 0) ||
			(*pbPersOL == *pbPublOL) ||
			*pbPersOL != NOT_HEARD_OR_SEEN && TileIsOutOfBounds(gsPublicLastKnownOppLoc[pSoldier->roster().team()][pOpponent->identity().id()]) && !TileIsOutOfBounds(gsLastKnownOppLoc[pSoldier->identity().id()][pOpponent->identity().id()]))
		{
			// using personal knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = *pusLastLoc;
			iThreatLevel = *pbLastLevel;
			//iThreatCertainty = ThreatPercent[*pbPersOL - OLDEST_HEARD_VALUE];
		}
		else
		{
			// using public knowledge, obtain opponent's "best guess" gridno
			sThreatLoc = gsPublicLastKnownOppLoc[pSoldier->roster().team()][pOpponent->identity().id()];
			iThreatLevel = gbPublicLastKnownOppLevel[pSoldier->roster().team()][pOpponent->identity().id()];
			//iThreatCertainty = ThreatPercent[*pbPublOL - OLDEST_HEARD_VALUE];
		}

		if (!AnyCoverFromSpot(sSpot, pSoldier->position().level(), sThreatLoc, iThreatLevel) &&
			LocationToLocationLineOfSightTest(sThreatLoc, iThreatLevel, sSpot, pSoldier->position().level(), TRUE, NO_DISTANCE_LIMIT, STANDING_LOS_POS, PRONE_LOS_POS))
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOLEAN AICheckHasWeaponOfType(TacticalActor *pSoldier, UINT8 ubWeaponType)
{
	CHECKF(pSoldier);

	INT8 invsize = (INT8)pSoldier->inventory().size();
	for (INT8 bLoop = 0; bLoop < invsize; ++bLoop)
	{
		if (pSoldier->inventory()[bLoop].exists() &&
			Item[pSoldier->inventory()[bLoop].usItem].usItemClass == IC_GUN &&
			Weapon[Item[pSoldier->inventory()[bLoop].usItem].ubClassIndex].ubWeaponType == ubWeaponType)
		{
			return TRUE;
		}
	}

	return FALSE;

}

BOOLEAN AICheckHasGun(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (FindAIUsableObjClass(pSoldier, IC_GUN) != NO_SLOT)
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN AICheckShortWeaponRange(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	if (!AICheckHasGun(pSoldier))
	{
		return TRUE;
	}

	if (AIGunRange(pSoldier) < TACTICAL_RANGE / 2)
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN NightLight(void)
{
	//if( GetTimeOfDayAmbientLightLevel() >= NORMAL_LIGHTLEVEL_DAY + 2 )
	if (gubEnvLightValue >= NORMAL_LIGHTLEVEL_NIGHT - 3)
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN DuskLight(void)
{
	// average between day and night
	if (gubEnvLightValue >= (NORMAL_LIGHTLEVEL_NIGHT + NORMAL_LIGHTLEVEL_DAY) / 2)
	{
		return TRUE;
	}

	return FALSE;
}

BOOLEAN UsePersonalKnowledge(TacticalActor *pSoldier, SoldierID ubOpponentID)
{
	INT8		bPersonalKnowledge;
	INT8		bPublicKnowledge;

	if (!pSoldier || ubOpponentID == NOBODY)
	{
		return FALSE;
	}

	bPersonalKnowledge = PersonalKnowledge(pSoldier, ubOpponentID);
	bPublicKnowledge = PublicKnowledge(pSoldier->roster().team(), ubOpponentID);

	if (gubKnowledgeValue[bPublicKnowledge - OLDEST_HEARD_VALUE][bPersonalKnowledge - OLDEST_HEARD_VALUE] > 0 ||
		bPersonalKnowledge != NOT_HEARD_OR_SEEN &&
		TileIsOutOfBounds(KnownPublicLocation(pSoldier->roster().team(), ubOpponentID)) &&
		!TileIsOutOfBounds(KnownPersonalLocation(pSoldier, ubOpponentID)))
	{
		return TRUE;
	}

	return FALSE;
}

INT8 Knowledge(TacticalActor *pSoldier, SoldierID ubOpponentID)
{
	if (!pSoldier || ubOpponentID == NOBODY)
	{
		return NOT_HEARD_OR_SEEN;
	}

	if (UsePersonalKnowledge(pSoldier, ubOpponentID))
	{
		return PersonalKnowledge(pSoldier, ubOpponentID);
	}

	return PublicKnowledge(pSoldier->roster().team(), ubOpponentID);
}

INT32 KnownLocation(TacticalActor *pSoldier, SoldierID ubOpponentID)
{
	if (!pSoldier || ubOpponentID == NOBODY)
	{
		return NOWHERE;
	}

	if (UsePersonalKnowledge(pSoldier, ubOpponentID))
	{
		return KnownPersonalLocation(pSoldier, ubOpponentID);
	}

	return KnownPublicLocation(pSoldier->roster().team(), ubOpponentID);
}

INT8 KnownLevel(TacticalActor *pSoldier, SoldierID ubOpponentID)
{
	if (!pSoldier || ubOpponentID == NOBODY)
	{
		return 0;
	}

	if (UsePersonalKnowledge(pSoldier, ubOpponentID))
	{
		return KnownPersonalLevel(pSoldier, ubOpponentID);
	}

	return KnownPublicLevel(pSoldier->roster().team(), ubOpponentID);
}

INT8 PersonalKnowledge(TacticalActor *pSoldier, SoldierID ubOpponentID)
{
	if (!pSoldier || ubOpponentID == NOBODY)
	{
		return NOT_HEARD_OR_SEEN;
	}

	return pSoldier->awareness().opponentKnowledge()[ubOpponentID];
}

INT32 KnownPersonalLocation(TacticalActor *pSoldier, SoldierID ubOpponentID)
{
	if (!pSoldier || ubOpponentID == NOBODY)
	{
		return NOWHERE;
	}
	/*if(PersonalKnowledge(pSoldier, ubOpponentID) == NOT_HEARD_OR_SEEN)
	{
	return NOWHERE;
	}*/

	return gsLastKnownOppLoc[pSoldier->identity().id()][ubOpponentID];
}

INT8 KnownPersonalLevel(TacticalActor *pSoldier, SoldierID ubOpponentID)
{
	if (!pSoldier || ubOpponentID == NOBODY)
	{
		return 0;
	}

	return gbLastKnownOppLevel[pSoldier->identity().id()][ubOpponentID];
}

INT8 PublicKnowledge(UINT8 bTeam, SoldierID ubOpponentID)
{
	if (bTeam >= MAXTEAMS || ubOpponentID == NOBODY)
	{
		return NOT_HEARD_OR_SEEN;
	}

	return gbPublicOpplist[bTeam][ubOpponentID];
}

INT32 KnownPublicLocation(UINT8 bTeam, SoldierID ubOpponentID)
{
	if (bTeam >= MAXTEAMS || ubOpponentID == NOBODY)
	{
		return NOWHERE;
	}
	/*if (PublicKnowledge(bTeam, ubOpponentID) == NOT_HEARD_OR_SEEN)
	{
		return NOWHERE;
	}*/

	return gsPublicLastKnownOppLoc[bTeam][ubOpponentID];
}

INT8 KnownPublicLevel(UINT8 bTeam, SoldierID ubOpponentID)
{
	if (bTeam >= MAXTEAMS || ubOpponentID == NOBODY)
	{
		return 0;
	}

	return gbPublicLastKnownOppLevel[bTeam][ubOpponentID];
}

UINT8 ArmyPercentKilled(void)
{
	if (gTacticalStatus.Team[ENEMY_TEAM].bMenInSector + gTacticalStatus.ubArmyGuysKilled == 0)
	{
		return 0;
	}

	return 100 * gTacticalStatus.ubArmyGuysKilled / (gTacticalStatus.Team[ENEMY_TEAM].bMenInSector + gTacticalStatus.ubArmyGuysKilled);
}

UINT8 TeamPercentKilled(INT8 bTeam)
{
	if (bTeam == ENEMY_TEAM)
	{
		return ArmyPercentKilled();
	}
	return 0;
}

BOOLEAN TeamHighPercentKilled(INT8 bTeam)
{
	if (bTeam == ENEMY_TEAM && ArmyPercentKilled() > ArmyPercentKilledTolerance())
	{
		return TRUE;
	}

	return FALSE;
}

// decide how many soldiers can be killed before alarm will be raised
UINT8 ArmyPercentKilledTolerance(void)
{
	// 50% at day, 25% at night, 25-33% for restricted sectors
	return 100 / (2 + SectorCurfew(TRUE));
}

UINT8 SectorCurfew(BOOLEAN fNight)
{
	UINT8	ubSectorId = SECTOR(gWorldSectorX, gWorldSectorY);
	UINT8	ubSectorData = 0;

	ubSectorData = SectorExternalData[ubSectorId][gbWorldSectorZ].usCurfewValue;

	if (fNight && NightLight())			// suspicious at night
		ubSectorData = max(ubSectorData, 1);

	if (gbWorldSectorZ > 0)	// underground we are always suspicious				
		ubSectorData = max(ubSectorData, 2);

	return ubSectorData;
}

INT32	RandomizeLocation(INT32 sSpot, INT8 bLevel, UINT8 ubTimes, TacticalActor *pSightSoldier)
{
	if (TileIsOutOfBounds(sSpot))
	{
		return NOWHERE;
	}

	UINT8 ubDirection;
	UINT8 ubMovementCost;
	INT32 sTempSpot;
	INT32 sSpotArray[NUM_WORLD_DIRECTIONS + 1];
	UINT8 ubSpots;

	for (UINT8 ubCnt = 0; ubCnt < ubTimes; ubCnt++)
	{
		// store original location
		ubSpots = 1;
		sSpotArray[0] = sSpot;

		// find adjacent locations
		for (ubDirection = 0; ubDirection < NUM_WORLD_DIRECTIONS; ubDirection++)
		{
			sTempSpot = NewGridNo(sSpot, DirectionInc(ubDirection));

			if (sTempSpot != sSpot)
			{
				ubMovementCost = gubWorldMovementCosts[sTempSpot][ubDirection][bLevel];

				if (ubMovementCost < TRAVELCOST_BLOCKED &&
					IsLocationSittableExcludingPeople(sTempSpot, bLevel) &&
					(!pSightSoldier || SoldierToVirtualSoldierLineOfSightTest(pSightSoldier, sTempSpot, bLevel, ANIM_STAND, TRUE, NO_DISTANCE_LIMIT)))
				{
					sSpotArray[ubSpots] = sTempSpot;
					ubSpots++;
				}
			}
		}
		// find random location
		sSpot = sSpotArray[Random(ubSpots)];
		// stop if could not find any adjacent spot
		if (ubSpots < 2)
		{
			break;
		}
	}

	return sSpot;
}

INT32	RandomizeOpponentLocation(INT32 sSpot, TacticalActor *pOpponent, INT16 sMaxDistance)
{
	if (TileIsOutOfBounds(sSpot))
	{
		return NOWHERE;
	}

	INT8 bXOffset, bYOffset;
	INT32 sRandomSpot;

	if (sMaxDistance > 0)
	{
		for (INT cnt = 0; cnt < min(sMaxDistance * 2, 100); cnt++)
		{
			bXOffset = Random(sMaxDistance * 2 + 1) - sMaxDistance;
			bYOffset = Random(sMaxDistance * 2 + 1) - sMaxDistance;

			sRandomSpot = sSpot + bXOffset + (MAXCOL * bYOffset);

			if (!TileIsOutOfBounds(sRandomSpot) && NewOKDestination(pOpponent, sRandomSpot, FALSE, pOpponent->position().level()))
			{
				return sRandomSpot;
			}
		}
	}

	return sSpot;
}

// first call PrepareThreatlist to make threat list
SoldierID ClosestKnownThreatID(TacticalActor *pSoldier, UINT32 uiThreatCnt)
{
	CHECKF(pSoldier);

	UINT32	uiLoop;
	INT32	sClosestOpponent = NOWHERE;
	INT32	iRange, iClosestRange;
	SoldierID	ubClosestOpponentID = NOBODY;

	// use global defined threat list
	for (uiLoop = 0; uiLoop < uiThreatCnt; uiLoop++)
	{
		// if for some reason we have incorrect location
		if (TileIsOutOfBounds(Threat[uiLoop].sGridNo))
			continue;

		iRange = GetRangeInCellCoordsFromGridNoDiff(pSoldier->position().gridNo(), Threat[uiLoop].sGridNo);

		if (ubClosestOpponentID == NOBODY || iRange < iClosestRange)
		{
			ubClosestOpponentID = Threat[uiLoop].pOpponent->identity().id();
			iClosestRange = iRange;
			sClosestOpponent = Threat[uiLoop].sGridNo;
		}
	}

	return(ubClosestOpponentID);
}

// first call PrepareThreatlist to make threat list
SoldierID ClosestSeenThreatID(TacticalActor *pSoldier, UINT32 uiThreatCnt, UINT8 ubMax)
{
	CHECKF(pSoldier);

	UINT32	uiLoop;
	INT32	sClosestOpponent = NOWHERE;
	INT32	iRange, iClosestRange;
	SoldierID	ubClosestOpponentID = NOBODY;

	// use global defined threat list
	for (uiLoop = 0; uiLoop < uiThreatCnt; uiLoop++)
	{
		// if for some reason we have incorrect location
		if (TileIsOutOfBounds(Threat[uiLoop].sGridNo))
			continue;

		// check knowledge
		if (Threat[uiLoop].bPersonalKnowledge < SEEN_CURRENTLY || Threat[uiLoop].bPersonalKnowledge > ubMax)
			continue;

		iRange = GetRangeInCellCoordsFromGridNoDiff(pSoldier->position().gridNo(), Threat[uiLoop].sGridNo);

		if (ubClosestOpponentID == NOBODY || iRange < iClosestRange)
		{
			ubClosestOpponentID = Threat[uiLoop].pOpponent->identity().id();
			iClosestRange = iRange;
			sClosestOpponent = Threat[uiLoop].sGridNo;
		}
	}

	return(ubClosestOpponentID);
}

UINT32 PrepareThreatlist(TacticalActor *pSoldier)
{
	TacticalActor *pOpponent;
	INT32	iThreatRange, iClosestThreatRange = 1500;
	UINT32	uiLoop;
	INT8	bPersonalKnowledge;
	INT8	bPublicKnowledge;
	INT8	bKnowledge;
	INT32	sThreatLoc;
	INT8	bThreatLevel;
	INT32	iThreatCertainty;
	INT32	iMaxThreatRange = MAX_THREAT_RANGE + AI_PATHCOST_RADIUS;

	UINT32 uiThreatCnt = 0;

	if (!pSoldier)
		return 0;

	// look through all opponents for those we know of
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); uiLoop++)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || pOpponent->vitals().health() < OKLIFE)
		{
			continue;			// next merc
		}

		if (!ValidOpponent(pSoldier, pOpponent))
		{
			continue;
		}

		bKnowledge = Knowledge(pSoldier, pOpponent->identity().id());
		bPersonalKnowledge = PersonalKnowledge(pSoldier, pOpponent->identity().id());
		bPublicKnowledge = PublicKnowledge(pSoldier->roster().team(), pOpponent->identity().id());

		// if this opponent is unknown personally and publicly
		if (bKnowledge == NOT_HEARD_OR_SEEN)
		{
			continue;			// next merc
		}

		sThreatLoc = KnownLocation(pSoldier, pOpponent->identity().id());
		bThreatLevel = KnownLevel(pSoldier, pOpponent->identity().id());
		iThreatCertainty = ThreatPercent[bKnowledge - OLDEST_HEARD_VALUE];

		// safety check
		if (TileIsOutOfBounds(sThreatLoc))
		{
			continue;
		}

		// calculate how far away this threat is (in adjusted pixels)
		iThreatRange = GetRangeInCellCoordsFromGridNoDiff(pSoldier->position().gridNo(), sThreatLoc);

		// if this opponent is believed to be too far away to really be a threat
		if (iThreatRange > iMaxThreatRange)
		{
			continue;			// check next opponent
		}

		// remember this opponent as a current threat, but DON'T REDUCE FOR COVER!
		Threat[uiThreatCnt].iValue = CalcManThreatValue(pOpponent, pSoldier->position().gridNo(), FALSE, pSoldier);

		// if the opponent is no threat at all for some reason
		if (Threat[uiThreatCnt].iValue == -999)
		{
			continue;			// check next opponent
		}

		Threat[uiThreatCnt].pOpponent = pOpponent;
		Threat[uiThreatCnt].sGridNo = sThreatLoc;
		Threat[uiThreatCnt].iCertainty = iThreatCertainty;
		Threat[uiThreatCnt].iOrigRange = iThreatRange;

		// calculate how many APs he will have at the start of the next turn
		Threat[uiThreatCnt].iAPs = TacticalActorTurnBudget::calculateTurnGrant(*pOpponent);

		// sevenfm: more information
		Threat[uiThreatCnt].bLevel = bThreatLevel;
		Threat[uiThreatCnt].bKnowledge = bKnowledge;
		Threat[uiThreatCnt].bPersonalKnowledge = bPersonalKnowledge;
		Threat[uiThreatCnt].bPublicKnowledge = bPublicKnowledge;

		if (iThreatRange < iClosestThreatRange)
		{
			iClosestThreatRange = iThreatRange;
		}

		uiThreatCnt++;
	}

	return uiThreatCnt;
}

UINT16 CountPublicKnownEnemies(TacticalActor *pSoldier, INT32 sGridNo, INT16 sDistance)
{
	CHECKF(pSoldier);

	UINT32		uiLoop;
	TacticalActor *pOpponent;

	INT32		sThreatLoc;
	INT8		iThreatLevel;

	UINT16		ubNum = 0;

	// loop through all the enemies
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || pOpponent->vitals().health() < OKLIFE)
		{
			continue;
		}

		if (!ValidOpponent(pSoldier, pOpponent))
		{
			continue;
		}

		// check public knowledge
		if (PublicKnowledge(pSoldier->roster().team(), pOpponent->identity().id()) == NOT_HEARD_OR_SEEN)
		{
			continue;
		}

		sThreatLoc = KnownPublicLocation(pSoldier->roster().team(), pOpponent->identity().id());
		iThreatLevel = KnownPublicLevel(pSoldier->roster().team(), pOpponent->identity().id());

		if (TileIsOutOfBounds(sThreatLoc))
		{
			continue;
		}

		// check distance
		if (PythSpacesAway(sThreatLoc, sGridNo) > sDistance)
		{
			continue;
		}

		ubNum++;
	}

	return ubNum;
}

UINT16 CountPublicKnownEnemies(TacticalActor *pSoldier)
{
	CHECKF(pSoldier);

	UINT32		uiLoop;
	TacticalActor *pOpponent;

	INT32		sThreatLoc;
	INT8		iThreatLevel;

	UINT16		ubNum = 0;

	// loop through all the enemies
	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop)
	{
		pOpponent = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		// if this merc is inactive, at base, on assignment, dead, unconscious
		if (!pOpponent || pOpponent->vitals().health() < OKLIFE)
		{
			continue;
		}

		if (!ValidOpponent(pSoldier, pOpponent))
		{
			continue;
		}

		// check public knowledge
		if (PublicKnowledge(pSoldier->roster().team(), pOpponent->identity().id()) == NOT_HEARD_OR_SEEN)
		{
			continue;
		}

		sThreatLoc = KnownPublicLocation(pSoldier->roster().team(), pOpponent->identity().id());
		iThreatLevel = KnownPublicLevel(pSoldier->roster().team(), pOpponent->identity().id());

		if (TileIsOutOfBounds(sThreatLoc))
		{
			continue;
		}

		ubNum++;
	}

	return ubNum;
}

// sevenfm: check if suppression is possible (count friends in the fire direction)
BOOLEAN CheckSuppressionDirection(TacticalActor *pSoldier, INT32 sTargetGridNo, INT8 bTargetLevel)
{
	TacticalActor * pFriend;
	UINT8 ubShootingDir;
	UINT32 uiLoop;

	CHECKF(pSoldier);
	CHECKF(!TileIsOutOfBounds(sTargetGridNo));

	ubShootingDir = AIDirection(pSoldier->position().gridNo(), sTargetGridNo);

	for (uiLoop = 0; uiLoop < Ja2ActiveTacticalActorSlotCount(); ++uiLoop)
	{
		pFriend = ResolveJa2ActiveTacticalActorSlot(uiLoop);

		if (pFriend &&
			pFriend != pSoldier &&
			pFriend->roster().active() &&
			pFriend->awareness().visibility() == TRUE &&
			pFriend->vitals().health() >= OKLIFE &&
			(pFriend->roster().side() == pSoldier->roster().side() || CONSIDERED_NEUTRAL(pSoldier, pFriend)) &&
			(pFriend->position().level() == pSoldier->position().level() || pFriend->position().level() == bTargetLevel) &&
			ubShootingDir == AIDirection(pSoldier->position().gridNo(), pFriend->position().gridNo()) &&
			PythSpacesAway(pSoldier->position().gridNo(), pFriend->position().gridNo()) > 1 &&
			PythSpacesAway(pSoldier->position().gridNo(), pFriend->position().gridNo()) < 2 * TACTICAL_RANGE &&
			(gAnimControl[pFriend->animationPlayback().state()].ubHeight == ANIM_STAND || gGameExternalOptions.fAllowTargetHeadAndLegIfProne) &&
			//!TacticalActorConditions::isCowering(*pFriend) &&
			AISoldierToSoldierChanceToGetThrough(pSoldier, pFriend) > 25)
			//LocationToLocationLineOfSightTest(pSoldier->sGridNo, pSoldier->position().level(), pFriend->sGridNo, pFriend->position().level(), TRUE, NO_DISTANCE_LIMIT))
		{
			return FALSE;
		}
	}

	return TRUE;
}
