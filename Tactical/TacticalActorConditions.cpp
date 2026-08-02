#include "TacticalActorConditions.h"

#include "Animation Control.h"
#include "Drugs And Alcohol.h"
#include "Food.h"
#include "GameSettings.h"
#include "Interface.h"
#include "Soldier Class.h"
#include "TacticalActor.h"
#include "Soldier macros.h"
#include "TacticalActorDisease.h"
#include "TacticalWorldAdapter.h"

#include <algorithm>

namespace TacticalActorConditions
{
bool isZombie(const TacticalActor& actor) noexcept
{
	return actor.roster().soldierClass() == SOLDIER_CLASS_ZOMBIE;
}

bool isAssassin(const TacticalActor& actor) noexcept
{
	const auto profile = actor.identity().profile();
	return (profile >= JIM && profile <= TYRONE) ||
		(actor.featureFlags().primaryFlags() & SOLDIER_ASSASSIN) != 0;
}

bool canBeCaptured(const TacticalActor& actor) noexcept
{
	if ((actor.featureFlags().primaryFlags() & SOLDIER_POW) != 0 ||
		actor.identity().profile() != NO_PROFILE)
	{
		return false;
	}

	if (ARMED_VEHICLE(&actor) || ENEMYROBOT(&actor))
	{
		return false;
	}

	if (actor.roster().team() == ENEMY_TEAM)
	{
		return true;
	}

	if (actor.roster().team() == CREATURE_TEAM &&
		actor.roster().soldierClass() == SOLDIER_CLASS_BANDIT)
	{
		return true;
	}

	return actor.roster().team() == CIV_TEAM &&
		zCivGroupName[actor.roster().civilianGroup()].fCanBeCaptured &&
		!actor.aiBehavior().neutral() &&
		actor.roster().side() == 1;
}

bool canDonateBlood(TacticalActor& actor)
{
	if (IsJa2TacticalCombatActive() ||
		actor.roster().team() != gbPlayerNum)
	{
		return false;
	}

	if (actor.status().flags() & SOLDIER_VEHICLE)
		return false;

	const auto profile = actor.identity().profile();
	if (profile != NO_PROFILE)
	{
		if (profile >= NUM_PROFILES ||
			gMercProfiles[profile].ubBodyType == ROBOTNOWEAPON)
		{
			return false;
		}
	}

	if (actor.vitals().maximumHealth() <= 0 ||
		actor.vitals().health() != actor.vitals().maximumHealth() ||
		actor.vitals().health() - bloodDonationAmount < OKLIFE)
	{
		return false;
	}

	if (TacticalActorDisease::hasAny(actor, true, false) ||
		MercDruggedOrDrunk(&actor))
	{
		return false;
	}

	if (UsingFoodSystem())
	{
		UINT8 foodSituation = FOOD_NORMAL;
		UINT8 waterSituation = FOOD_NORMAL;
		GetFoodSituation(&actor, &foodSituation, &waterSituation);
		if (foodSituation > FOOD_NORMAL ||
			waterSituation > FOOD_NORMAL)
		{
			return false;
		}
	}

	return true;
}

bool isCowering(const TacticalActor& actor) noexcept
{
	const auto animation = actor.animationPlayback().state();
	return animation == COWERING || animation == COWERING_PRONE;
}

bool isUnconscious(const TacticalActor& actor) noexcept
{
	return actor.collapseState().tactical() &&
		actor.vitals().breath() < OKBREATH;
}

bool isGivingAid(const TacticalActor& actor) noexcept
{
	const auto animation = actor.animationPlayback().state();
	return animation == GIVING_AID ||
		animation == GIVING_AID_PRN ||
		animation == START_AID ||
		animation == START_AID_PRN;
}

bool hasTakenLargeHit(const TacticalActor& actor) noexcept
{
	return (actor.featureFlags().secondaryFlags() & SOLDIER_TAKEN_LARGE_HIT) != 0;
}

std::uint8_t suppressionShockPercent(const TacticalActor& actor) noexcept
{
	const auto maximumShock = gGameExternalOptions.ubMaxSuppressionShock;
	if (maximumShock == 0)
	{
		return 0;
	}

	return static_cast<std::uint8_t>(
		std::min(100, 100 * actor.suppression().shock() / maximumShock));
}
}
