#include "TacticalActorAiBehavior.h"

#include "Animation Control.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "ai.h"
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
				actor.EVENT_InitNewSoldierAnim(
					END_COWER,
					0,
					FALSE);
			}
			else if (stance == ANIM_CROUCH)
			{
				actor.animationIntent().desiredHeight() =
					ANIM_CROUCH;
				actor.EVENT_InitNewSoldierAnim(
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
			actor.EVENT_InitNewSoldierAnim(
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
		return actor.EVENT_InitNewSoldierAnim(
			AI_RADIO,
			0,
			FALSE);
	case ANIM_CROUCH:
		return actor.EVENT_InitNewSoldierAnim(
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
