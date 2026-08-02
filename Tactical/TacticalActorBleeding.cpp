#include "TacticalActorBleeding.h"

#include "TacticalActor.h"
#include "Animation Control.h"
#include "Auto Bandage.h"
#include "Dialogue Control.h"
#include "Drugs And Alcohol.h"
#include "GameSettings.h"
#include "Interface.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "Smell.h"
#include "Soldier Profile.h"
#include "TacticalActorBloodState.h"
#include "TacticalActorDamageResolution.h"
#include "TacticalWorldAdapter.h"
#include "rt time defines.h"

#include <algorithm>

namespace
{
void applyBleedDamage(TacticalActor& actor, bool bandagedBleed)
{
	if ((actor.roster().inSector() && GetCurrentScreen() == GAME_SCREEN) ||
		GetCurrentScreen() != GAME_SCREEN)
	{
		actor.uiPresentation().startPortraitFlash();
		actor.uiPresentation().portraitFlashFrame() =
			FLASH_PORTRAIT_STARTSHADE;
		actor.timing().start(
			SoldierTimingComponent::Timer::PortraitFlash,
			FLASH_PORTRAIT_DELAY);

		if (GetCurrentScreen() == MAP_SCREEN)
		{
			SetInfoChar(actor.identity().id());
		}
	}

	if (bandagedBleed)
		return;

	SoldierID attacker = actor.combatResult().currentAttacker();
	if (attacker == NOBODY)
		attacker = actor.combatResult().previousAttacker();
	if (attacker == NOBODY)
		attacker = actor.combatResult().earlierAttacker();

	TacticalActorDamageResolution::takeDamage(
		actor,
		ANIM_CROUCH,
		1,
		100,
		TAKE_DAMAGE_BLOODLOSS,
		attacker,
		NOWHERE,
		0,
		TRUE);
}
}

FLOAT TacticalActorBleeding::nextInterval(
	const TacticalActor& actor)
{
	const INT8 bandaged =
		actor.vitals().maximumHealth() -
		actor.vitals().health() -
		actor.vitals().bleeding();

	FLOAT interval = 1.0f;
	if (DoesMercHaveDisability(
			&actor,
			HEMOPHILIAC))
	{
		interval += static_cast<FLOAT>(actor.vitals().health()) /
			static_cast<FLOAT>(
				30 + 2 * actor.movementMetrics().tilesMoved());
	}
	else
	{
		interval += static_cast<FLOAT>(
			actor.vitals().health() + bandaged / 2) /
			static_cast<FLOAT>(
				10 + actor.movementMetrics().tilesMoved());
	}

	return interval;
}

FLOAT TacticalActorBleeding::nextUnmovingInterval(
	const TacticalActor& actor)
{
	const INT8 bandaged =
		actor.vitals().maximumHealth() -
		actor.vitals().health() -
		actor.vitals().bleeding();
	return static_cast<FLOAT>(1) +
		static_cast<FLOAT>(
			(actor.vitals().health() + bandaged / 2) / 10);
}

INT32 TacticalActorBleeding::check(TacticalActor& actor)
{
	INT32 blood = NOBLOOD;
	if (actor.vitals().health() == 0)
		return blood;

	const bool hemophiliac =
		DoesMercHaveDisability(&actor, HEMOPHILIAC) != FALSE;
	if (actor.vitals().bleeding() <= MIN_BLEEDING_THRESHOLD &&
		actor.vitals().health() >= OKLIFE &&
		!hemophiliac)
	{
		return blood;
	}

	if (actor.service().hasProviders() ||
		AnyDoctorWhoCanHealThisPatient(&actor, HEALABLE_EVER) != nullptr)
	{
		return blood;
	}

	if (!actor.movementMetrics().movedThisTurn())
	{
		blood = std::max(
			0,
			(actor.vitals().bleeding() - MIN_BLEEDING_THRESHOLD) /
				BLOODDIVISOR);
		blood = std::min<INT32>(blood, MAXBLOODQUANTITY);
	}

	if (hemophiliac)
		blood = std::min<INT32>(1, blood);

	if (!IsJa2TacticalTurnBased() || !IsJa2TacticalCombatActive())
		actor.vitals().nextBleedAt() -=
			static_cast<FLOAT>(RT_NEXT_BLEED_MODIFIER);
	else
		actor.vitals().nextBleedAt()--;

	if (actor.vitals().nextBleedAt() > 0)
		return blood;

	const INT8 bandaged =
		actor.vitals().maximumHealth() -
		actor.vitals().bleeding() -
		actor.vitals().health();
	if (bandaged && actor.vitals().health() >= OKLIFE)
	{
		actor.vitals().bleeding()++;
		applyBleedDamage(actor, true);
	}
	else if (actor.vitals().health() < OKLIFE)
	{
		if (actor.vitals().health() >= CONSCIOUSNESS &&
			!actor.dialogue().hasMadeDyingComment())
		{
			TacticalCharacterDialogue(
				&actor,
				QUOTE_SERIOUSLY_WOUNDED);
			actor.dialogue().markDyingCommentSpoken();
		}

		if (actor.vitals().maximumHealth() >= OKLIFE &&
			actor.vitals().bleeding())
		{
			actor.vitals().maximumHealth()--;
			actor.vitals().bleeding() = std::max(
				0,
				actor.vitals().bleeding() - 1);
			if (actor.vitals().healableInjury() >= 100)
				actor.vitals().healableInjury() -= 100;
		}
	}

	if (actor.vitals().bleeding())
	{
		applyBleedDamage(actor, false);
	}
	else
	{
		TacticalActorDamageResolution::takeDamage(
			actor,
			ANIM_CROUCH,
			0,
			0,
			TAKE_DAMAGE_BLOODLOSS,
			NOBODY,
			NOWHERE,
			0,
			TRUE);
	}

	if (actor.vitals().health() >= OKLIFE &&
		!actor.dialogue().hasMadeDyingComment() &&
		!actor.dialogue().hasWarnedAboutBleeding() &&
		!gTacticalStatus.fAutoBandageMode &&
		!actor.service().hasProviders())
	{
		TacticalCharacterDialogue(&actor, QUOTE_STARTING_TO_BLEED);
		actor.dialogue().markBleedingWarningSpoken();
	}

	actor.vitals().nextBleedAt() = nextInterval(actor);
	return blood;
}

FLOAT CalcSoldierNextBleed(TacticalActor* actor)
{
	return actor != nullptr
		? TacticalActorBleeding::nextInterval(*actor)
		: 0.0f;
}

FLOAT CalcSoldierNextUnmovingBleed(TacticalActor* actor)
{
	return actor != nullptr
		? TacticalActorBleeding::nextUnmovingInterval(*actor)
		: 0.0f;
}
