#include "TacticalActorTurncoats.h"
#include "TacticalActorCovertOps.h"
#include "TacticalActorModifiers.h"

#include "GameSettings.h"
#include "Drugs And Alcohol.h"
#include "IMP Skill Trait.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "LOS.h"
#include "Map Information.h"
#include "Overhead.h"
#include "Points.h"
#include "Queen Command.h"
#include "SkillCheck.h"
#include "TacticalActor.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"
#include "SoldierRepository.h"
#include "Town Militia.h"
#include "ai.h"

#include <algorithm>
#include <cstdint>

extern UINT32 gCoolnessBySector[256];

bool TacticalActorTurncoats::inPositionForAttempt(
	TacticalActor& actor,
	SoldierID targetId)
{
	if ( !gSkillTraitValues.fCOTurncoats
		|| gbWorldSectorZ
		|| gTacticalStatus.Team[ENEMY_TEAM].bAwareOfOpposition )
		return false;

	if (actor.vitals().health() < OKLIFE ||
		actor.assignment().isAsleep() ||
		actor.collapseState().tactical() ||
		(actor.featureFlags().primaryFlags() & SOLDIER_POW) ||
		actor.skillState().cooldown(
			SOLDIER_COOLDOWN_INTEL_PENALTY) > 20 ||
		targetId == NOBODY ||
		actor.animationPlayback().state() >=
			NUMANIMATIONSTATES ||
		TileIsOutOfBounds(actor.position().gridNo()))
	{
		return false;
	}

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if ( !pSoldier
		|| pSoldier->roster().team() != ENEMY_TEAM
		|| pSoldier->identity().profile() != NO_PROFILE
		|| pSoldier->vitals().health() != pSoldier->vitals().maximumHealth()
		|| pSoldier->collapseState().tactical()
		|| ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
		|| !SOLDIER_CLASS_ENEMY( pSoldier->roster().soldierClass() )
		|| TileIsOutOfBounds(pSoldier->position().gridNo())
		|| !TacticalActorCovertOps::seemsLegitimate(
			actor,
			pSoldier->identity().id()))
	{
		return false;
	}

	// additional checks if we want to know wether we can target a specific location
	if (PythSpacesAway(
			actor.position().gridNo(),
			pSoldier->position().gridNo()) < 10)
	{
		INT32 val = SoldierToVirtualSoldierLineOfSightTest(
			&actor,
			pSoldier->position().gridNo(),
			actor.position().level(),
			gAnimControl[actor.animationPlayback().state()]
				.ubEndHeight,
			FALSE,
			10);

		// error if we cannot see the target
		return val != 0;
	}

	return false;
}

std::uint8_t TacticalActorTurncoats::convictionChance(
	TacticalActor& actor,
	SoldierID targetId,
	std::int16_t approach)
{
	auto* const self = &actor;
	const auto recruiterProfile = actor.identity().profile();
	if (targetId >= NOBODY ||
		approach < 1 ||
		approach > 4 ||
		recruiterProfile == NO_PROFILE ||
		recruiterProfile >= NUM_PROFILES ||
		actor.deployment().sectorX() < 1 ||
		actor.deployment().sectorX() >= MAP_WORLD_X - 1 ||
		actor.deployment().sectorY() < 1 ||
		actor.deployment().sectorY() >= MAP_WORLD_Y - 1)
	{
		return 0;
	}

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if ( !pSoldier
		|| pSoldier->roster().team() != ENEMY_TEAM )
		return 0;

	// enemy robots can't be turncoats
	if (pSoldier->roster().soldierClass() == SOLDIER_CLASS_ROBOT)
		return 0;

	if (actor.vitals().health() < OKLIFE)
		return 0;

	// determine effectiveness of merc
	// nominally in [0; 1000]
	INT32 basestatrating =
		6 * EffectiveLeadership(self) +
		40 * EffectiveExpLevel(self, FALSE);

	FLOAT recruitmodifier =
		(100 +
		 TacticalActorModifiers::backgroundValue(
			 actor,
			 BG_PERC_APPROACH_RECRUIT)) /
		100.0f;

	// personality/disability modifiers
	FLOAT persmodifier = 1.0f;
	if (DoesMercHaveDisability(self, NERVOUS))
		persmodifier -= 0.10f;

	if ( gGameOptions.fNewTraitSystem )
	{
		if (DoesMercHavePersonality(self, CHAR_TRAIT_SOCIABLE))
			persmodifier += 0.08f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_LONER))
			persmodifier -= 0.04f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_ASSERTIVE))
			persmodifier += 0.05f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_AGGRESSIVE))
			persmodifier -= 0.05f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_PHLEGMATIC))
			persmodifier -= 0.02f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_DAUNTLESS))
			persmodifier += 0.03f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_SHOWOFF))
			persmodifier += 0.04f;
		if (DoesMercHavePersonality(self, CHAR_TRAIT_COWARD))
			persmodifier -= 0.07f;
	}

	// nominally in [0; 100]
	INT32 recruitrating =
		static_cast<INT32>(
			basestatrating *
			recruitmodifier *
			persmodifier *
			gMercProfiles[recruiterProfile].usApproachFactor[3] /
			1000);

	// optional ini bonus
	recruitrating += gSkillTraitValues.sCOTurncoats_PlayerConvinctionBonus;

	ReducePointsForFatigue(self, &recruitrating);

	// determine resistance of soldier to our subversion
	INT32 ubLocationModifier =
		2 * max(
			1,
			min(
				20,
				gCoolnessBySector[SECTOR(
					actor.deployment().sectorX(),
					actor.deployment().sectorY())]));

	// enemy resistance is dependent on their level, class and the sector rating
	INT32 enemyresistancerating = ubLocationModifier + 8 * EffectiveExpLevel( pSoldier, FALSE );

	switch ( pSoldier->roster().soldierClass() )
	{
	case SOLDIER_CLASS_ADMINISTRATOR:	enemyresistancerating -= 30;	break;
	case SOLDIER_CLASS_ELITE:			enemyresistancerating += 30;	break;
	default:	break;
	}

	if ( pSoldier->featureFlags().primaryFlags() & SOLDIER_ENEMY_OFFICER )
		enemyresistancerating += 30;

	ReducePointsForFatigue( pSoldier, &enemyresistancerating );

	switch (approach)
	{
		// base approach
	case 1:
		break;

		// we use our looks for seduction
		// thus looking attractive lowers enemy resistance, while being ugly can increase it
		// however this fails if the soldier is not attracted to us
	case 2:
	{
		// determine whether the soldier is attracted to us in the first place (don't display this, otherwise people will want to set sexual orientation and whatnot)
		INT32 stat_dependant_roll = ( 37 * EffectiveStrength( pSoldier, FALSE ) + 92 * EffectiveMedical( pSoldier ) + 51 * EffectiveDexterity( pSoldier, FALSE ) + 61 * pSoldier->vitals().health() ) % 100;
		bool samesexattraction = ( stat_dependant_roll < 8 );

		bool female_player =
			(actor.identity().bodyType() == REGFEMALE);
		bool female_soldier = ( pSoldier->identity().bodyType() == REGFEMALE );

		bool fittingattraction = false;
		if ( female_player != female_soldier && !samesexattraction )
			fittingattraction = true;
		else if ( female_player == female_soldier && samesexattraction )
			fittingattraction = true;

		if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_UGLY)
			enemyresistancerating +=
				50 - (fittingattraction ? 5 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_HOMELY)
			enemyresistancerating +=
				40 - (fittingattraction ? 15 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_AVERAGE)
			enemyresistancerating +=
				30 - (fittingattraction ? 30 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_ATTRACTIVE)
			enemyresistancerating +=
				20 - (fittingattraction ? 45 : 0);
		else if (gMercProfiles[recruiterProfile].bAppearance ==
			APPEARANCE_BABE)
			enemyresistancerating +=
				10 - (fittingattraction ? 60 : 0);

		// seduction works better in civilian clothing
		if (actor.featureFlags().primaryFlags() &
			SOLDIER_COVERT_CIV)
			enemyresistancerating -= 5;
	}
	break;

	// we try to bribe the soldier with money
	case 3:
	{
		// the amount of money depends on progress and unimportant in this case
		// the worse the location, the poorer the soldier, thus the more effective money is
		enemyresistancerating -= 30 + (40 - ubLocationModifier);
	}
	break;

	// we try to bribe the soldier with intel
	case 4:
	{
		// the amount of intel depends on progress and unimportant in this case
		enemyresistancerating -= 80;
	}
	break;

	default:
		break;
	}

	if ( enemyresistancerating > recruitrating )
		return 0;

	return static_cast<std::uint8_t>(
		std::clamp(
			recruitrating - enemyresistancerating,
			0,
			100));
}

void TacticalActorTurncoats::attempt(SoldierID targetId)
{
	if (targetId >= NOBODY)
		return;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if ( !pSoldier
		|| pSoldier->roster().team() != ENEMY_TEAM
		|| ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT ) )
		return;

	HandleTurncoatAttempt( pSoldier );
}

bool TacticalActorTurncoats::orderOne(SoldierID targetId)
{
	if (targetId >= NOBODY)
		return false;

	TacticalActor* pSoldier =
		GetJa2SoldierRepository().resolve(
			targetId);

	if (!pSoldier
		|| pSoldier->roster().team() != ENEMY_TEAM
		|| !( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
		|| pSoldier->deployment().sectorX() < 1
		|| pSoldier->deployment().sectorX() >= MAP_WORLD_X - 1
		|| pSoldier->deployment().sectorY() < 1
		|| pSoldier->deployment().sectorY() >= MAP_WORLD_Y - 1)
		return false;

	if ( IsFreeSlotAvailable( MILITIA_TEAM ) )
	{
		// remove turncoat property
		pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_TURNCOAT;
		RemoveOneTurncoat( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->roster().soldierClass(), TRUE );

		MakeCivHostile( pSoldier );

		return true;
	}

	return false;
}

void TacticalActorTurncoats::orderAll()
{
	TacticalActor *pSoldier;
	SoldierID cnt = gTacticalStatus.Team[ENEMY_TEAM].bFirstID;

	// rftr: force the player to enter turn-based combat. this function already includes a check to see if we're already in combat, so no harm calling this.
	// this also prevents a hang when activating a sector with 100% turncoats
	EnterCombatMode(OUR_TEAM);

	// run through list
	for ( ; cnt <= gTacticalStatus.Team[ENEMY_TEAM].bLastID; ++cnt )
	{
		pSoldier =
			GetJa2SoldierRepository().resolve(
				cnt );
		if (pSoldier != nullptr &&
			pSoldier->roster().active() &&
			pSoldier->roster().inSector() &&
			pSoldier->roster().team() == ENEMY_TEAM &&
			pSoldier->deployment().sectorX() >= 1 &&
			pSoldier->deployment().sectorX() <
				MAP_WORLD_X - 1 &&
			pSoldier->deployment().sectorY() >= 1 &&
			pSoldier->deployment().sectorY() <
				MAP_WORLD_Y - 1)
		{
			if ( pSoldier->featureFlags().secondaryFlags() & SOLDIER_TURNCOAT )
			{
				if ( IsFreeSlotAvailable( MILITIA_TEAM ) )
				{
					// remove turncoat property
					pSoldier->featureFlags().secondaryFlags() &= ~SOLDIER_TURNCOAT;
					RemoveOneTurncoat( pSoldier->deployment().sectorX(), pSoldier->deployment().sectorY(), pSoldier->roster().soldierClass(), TRUE );

					MakeCivHostile( pSoldier );
				}
				else
				{
					return;
				}
			}
		}
	}
}
