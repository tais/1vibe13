#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorAiBehavior.h"

#include "Animation Control.h"
#include "Handle Items.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "Weapons.h"
#include "ai.h"
#include "random.h"
#include "worldman.h"

#include <algorithm>
#include <cstddef>

bool TacticalActorAiBehavior::hasInitialActionPoints(
	const TacticalActor& actor) noexcept
{
	return actor.actionPoints().current() >=
			actor.actionPoints().initial() &&
		!(actor.featureFlags().secondaryFlags() &
			SOLDIER_SPENT_AP);
}

bool TacticalActorAiBehavior::isFlanking(
	const TacticalActor& actor) noexcept
{
	return actor.aiBehavior().alertStatus() >= STATUS_YELLOW &&
		actor.aiPlanning().flanking(MAX_FLANKS_RED);
}

void TacticalActorAiBehavior::setUnderControl(
	TacticalActor& actor)
{
	auto& repository = GetJa2SoldierRepository();
	const std::size_t first =
		gTacticalStatus.Team[OUR_TEAM].bFirstID.i;
	const std::size_t last =
		gTacticalStatus.Team[LAST_TEAM].bLastID.i;
	if (first <= last && first < repository.capacity())
	{
		const std::size_t boundedLast =
			std::min(last, repository.capacity() - 1);
		for (std::size_t index = first;
			 index <= boundedLast;
			 ++index)
		{
			TacticalActor* const candidate =
				repository.resolve(index);
			if (candidate && candidate->roster().active())
			{
				candidate->status().flags() &=
					~SOLDIER_UNDERAICONTROL;
			}
		}
	}

	actor.status().flags() |= SOLDIER_UNDERAICONTROL;
}

void TacticalActorAiBehavior::stopCowering(
	TacticalActor& actor)
{
	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if (animationState < NUMANIMATIONSTATES)
	{
		const std::uint8_t stance =
			gAnimControl[animationState].ubEndHeight;
		if (animationState == COWERING)
		{
			if (stance == ANIM_STAND)
			{
				actor.animationIntent().desiredHeight() =
					ANIM_STAND;
				TacticalActorAnimationTransitions::initializeAnimation(actor,
					END_COWER,
					0,
					FALSE);
			}
			else if (stance == ANIM_CROUCH)
			{
				actor.animationIntent().desiredHeight() =
					ANIM_CROUCH;
				TacticalActorAnimationTransitions::initializeAnimation(actor,
					END_COWER_CROUCHED,
					0,
					FALSE);
			}
		}
		else if (
			animationState == COWERING_PRONE &&
			stance == ANIM_PRONE)
		{
			actor.animationIntent().desiredHeight() =
				ANIM_PRONE;
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				END_COWER_PRONE,
				0,
				FALSE);
		}
	}

	actor.status().flags() &= ~SOLDIER_COWERING;
}

void TacticalActorAiBehavior::startRetreat(
	TacticalActor& actor,
	std::uint16_t turns) noexcept
{
	auto& counter =
		actor.skillState().counter(SOLDIER_COUNTER_RETREAT);
	counter = std::max(turns, counter);
}

std::uint16_t TacticalActorAiBehavior::retreatCounter(
	const TacticalActor& actor) noexcept
{
	return actor.skillState().counter(SOLDIER_COUNTER_RETREAT);
}

bool TacticalActorAiBehavior::startRadioAnimation(
	TacticalActor& actor)
{
	const std::uint8_t bodyType = actor.identity().bodyType();
	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if ((bodyType != REGMALE && bodyType != BIGMALE) ||
		actor.awareness().visibility() != TRUE ||
		!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(actor.position().gridNo()) ||
		actor.position().level() < FIRST_LEVEL ||
		actor.position().level() > SECOND_LEVEL ||
		animationState >= NUMANIMATIONSTATES ||
		Water(
			actor.position().gridNo(),
			actor.position().level()))
	{
		return false;
	}

	switch (gAnimControl[animationState].ubEndHeight)
	{
	case ANIM_STAND:
		return TacticalActorAnimationTransitions::initializeAnimation(actor,
			AI_RADIO,
			0,
			FALSE);
	case ANIM_CROUCH:
		return TacticalActorAnimationTransitions::initializeAnimation(actor,
			AI_CR_RADIO,
			0,
			FALSE);
	default:
		return false;
	}
}

void TacticalActorAiBehavior::clearBoxerFlag(
	TacticalActor& actor) noexcept
{
	actor.status().flags() &= ~SOLDIER_BOXER;
}

void TacticalActorAiBehavior::handleNewSituation(
	TacticalActor& actor,
	bool /*resetActionBudget*/)
{
	if (actor.aiBehavior().newSituation() != IS_NEW_SITUATION)
		return;

	if (actor.animationIntent().pendingAnimation() != FALLOFF &&
		actor.animationIntent().pendingAnimation() != FALLFORWARD_ROOF)
	{
		actor.animationIntent().clearPendingAnimation();
	}
	actor.animationIntent().clearSecondaryPendingAnimation();
	actor.animationActivity().turningFromProneMode() = FALSE;
	actor.animationIntent().clearPendingDirection();
	if (actor.runtime().worldObject.owner() ==
		SoldierWorldObjectContinuationOwner::PathRoute)
		actor.runtime().worldObject.reset();
	actor.pendingAction().clearAction();
	actor.schedule().cancelDoorContinuation();

	if (!(actor.status().flags() & SOLDIER_UNDERAICONTROL))
		return;

	if (actor.animationActivity().turningToShoot())
	{
		actor.animationActivity().turningToShoot() = FALSE;
		actor.targeting().retainLastTargetFromTurn() = TRUE;
		DebugMsg(
			TOPIC_JA2,
			DBG_LEVEL_3,
			String(
				"@@@@@@@ Reducing attacker busy count..., ending fire because saw something: DONE IN SYSTEM NEW SITUATION"));
		DebugAttackBusy(
			"@@@@@@@ Reducing attacker busy count..., ending fire because saw something: DONE IN SYSTEM NEW SITUATION\n");
		FreeUpAttacker();
	}

	if (actor.pendingItem().hasObject())
	{
		AutoPlaceObject(&actor, actor.pendingItem().object(), FALSE);
		actor.pendingItem().clearThrowTransaction();
		actor.animationIntent().clearPendingAnimations();
		DebugMsg(
			TOPIC_JA2,
			DBG_LEVEL_3,
			String(
				"@@@@@@@ Reducing attacker busy count..., ending throw because saw something: DONE IN SYSTEM NEW SITUATION"));
		DebugAttackBusy(
			"@@@@@@@ Reducing attacker busy count..., ending throw because saw something: DONE IN SYSTEM NEW SITUATION\n");
		FreeUpAttacker();
	}
}

bool TacticalActorAiBehavior::decideHipOrShoulderStance(
	TacticalActor& actor,
	std::int32_t targetGrid)
{
	const std::uint16_t animation = actor.animationPlayback().state();
	const std::uint16_t handItem = actor.inventory()[HANDPOS].usItem;
	const std::uint16_t selectedWeapon = actor.attackSelection().weapon();
	if (animation >= NUMANIMATIONSTATES ||
		handItem >= MAXITEMS ||
		selectedWeapon >= MAXITEMS ||
		gAnimControl[animation].ubEndHeight != ANIM_STAND ||
		!ItemIsTwoHanded(handItem))
	{
		return false;
	}

	if (Weapon[selectedWeapon].HeavyGun)
		return true;
	if (actor.aiPlanning().aimTime() >
		GetNumberAltFireAimLevels(&actor, targetGrid))
	{
		return false;
	}

	INT8 hipChance = 0;
	if (actor.fireControl().burstCounter() > 0)
		hipChance += 25;
	if (Weapon[selectedWeapon].ubWeaponType == GUN_LMG)
		hipChance += 30;
	if (Weapon[selectedWeapon].ubWeaponType == GUN_SHOTGUN)
		hipChance += 15;
	if (!TileIsOutOfBounds(targetGrid))
	{
		hipChance += CalcChanceToHitGun(
			&actor,
			targetGrid,
			0,
			AIM_SHOT_TORSO);
	}

	return PreChance(hipChance) != FALSE;
}

void HandleSystemNewAISituation(
	TacticalActor* actor,
	BOOLEAN resetActionBudget)
{
	if (actor != nullptr)
	{
		TacticalActorAiBehavior::handleNewSituation(
			*actor,
			resetActionBudget != FALSE);
	}
}

BOOLEAN AIDecideHipOrShoulderStance(
	TacticalActor* actor,
	INT32 targetGrid)
{
	return actor != nullptr &&
		TacticalActorAiBehavior::decideHipOrShoulderStance(
			*actor,
			targetGrid)
		? TRUE
		: FALSE;
}
