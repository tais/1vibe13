#include "TacticalActorAnimationTiming.h"

#include "Animation Control.h"
#include "Disease.h"
#include "GameSettings.h"
#include "IMP Skill Trait.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "SoldierRepository.h"
#include "SkillCheck.h"
#include "TacticalActor.h"
#include "TacticalActorDisease.h"
#include "TacticalActorDragging.h"
#include "TacticalActorEquipment.h"
#include "TacticalActorModifiers.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"
#include "TacticalWorldAdapter.h"
#include "Weapons.h"
#include "connect.h"
#include "tiledef.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace
{
constexpr std::array<std::int16_t, NUM_TERRAIN_TYPES>
	TerrainSpeedModifiers{
		5, 5, 5, 5, 5, 10, 15, 20, 20, 25, 30};

bool hasAnimationContext(const TacticalActor& actor) noexcept
{
	return actor.animationPlayback().state() < NUMANIMATIONSTATES &&
		actor.identity().bodyType() < TOTALBODYTYPES &&
		actor.position().terrainType() < NUM_TERRAIN_TYPES;
}

bool readBurstDelay(
	const TacticalActor& actor,
	std::int16_t& delay) noexcept
{
	if (HANDPOS >= actor.inventory().size())
		return false;
	const OBJECTTYPE& hand = actor.inventory()[HANDPOS];
	if (hand.usItem >= MAXITEMS ||
		Item[hand.usItem].ubClassIndex >= MAXITEMS)
	{
		return false;
	}
	delay = Weapon[Item[hand.usItem].ubClassIndex].sAniDelay;
	return true;
}

void adjustForTacticalSpeed(TacticalActor& actor)
{
	if (gTacticalStatus.uiFlags & SLOW_ANIMATION)
	{
		if (gTacticalStatus.bRealtimeSpeed == -1)
			actor.animationPlayback().delay() = 10000;
		else
		{
			actor.animationPlayback().delay() =
				actor.animationPlayback().delay() *
				(1 * gTacticalStatus.bRealtimeSpeed / 2);
		}
	}
	actor.timing().start(
		SoldierTimingComponent::Timer::AnimationUpdate,
		actor.animationPlayback().delay());
}

bool calculateDelay(
	TacticalActor& actor,
	TacticalActor& statsActor)
{
	DebugMsg(
		TOPIC_JA2,
		DBG_LEVEL_3,
		"TacticalActorAnimationTiming::calculateDelay");
	std::int16_t terrainDelay = 0;
	std::int8_t breathDeficit = 0;
	std::int8_t lifeDeficit = 0;
	std::int16_t agilityDeficit = 0;
	std::int16_t additional = 0;
	constexpr std::int16_t BrokenLegPenalty = 60;
	const std::uint16_t animationState =
		actor.animationPlayback().state();

	switch (animationState)
	{
	case STANDING_BURST:
	case FIRE_STAND_BURST_SPREAD:
	case FIRE_BURST_LOW_STAND:
	case TANK_BURST:
	case CROUCHED_BURST:
	case PRONE_BURST:
	case BURST_ALTERNATIVE_STAND:
	case LOW_BURST_ALTERNATIVE_STAND:
		if (!readBurstDelay(actor, actor.animationPlayback().delay()))
			return false;
		adjustForTacticalSpeed(actor);
		return true;

	case BURST_DUAL_STAND:
	case BURST_DUAL_CROUCH:
	case BURST_DUAL_PRONE:
		if (!readBurstDelay(actor, actor.animationPlayback().delay()))
			return false;
		actor.animationPlayback().delay() /= 2;
		adjustForTacticalSpeed(actor);
		return true;

	case PRONE:
	case STANDING:
	case CROUCHING:
		actor.animationPlayback().delay() =
			(statsActor.vitals().breath() * 2) +
			(100 - statsActor.vitals().health());
		actor.animationPlayback().delay() = std::max<std::int16_t>(
			actor.animationPlayback().delay(), 40);
		adjustForTacticalSpeed(actor);
		return true;

	case WALKING:
	case WALKING_WEAPON_RDY:
	case WALKING_DUAL_RDY:
	case WALKING_ALTERNATIVE_RDY:
	case RUNNING:
		additional = gubAnimWalkSpeeds[
			statsActor.identity().bodyType()].sSpeed;
		if (gGameExternalOptions.fDisease &&
			gGameExternalOptions.fDiseaseSevereLimitations &&
			TacticalActorDisease::hasOutbreakProperty(
				actor,
				DISEASE_PROPERTY_LIMITED_USE_LEGS))
		{
			additional += BrokenLegPenalty;
		}
		additional = std::max<std::int16_t>(additional, 0);
		break;

	case SWATTING:
	case SWATTING_WK:
	case SWAT_BACKWARDS_WK:
	case CROUCHEDMOVE_RIFLE_READY:
	case CROUCHEDMOVE_PISTOL_READY:
	case CROUCHEDMOVE_DUAL_READY:
	case SIDE_STEP_CROUCH_RIFLE:
	case SIDE_STEP_CROUCH_PISTOL:
	case SIDE_STEP_CROUCH_DUAL:
	case CRAWLING:
		if (statsActor.identity().bodyType() <= REGFEMALE)
		{
			additional = gubAnimWalkSpeeds[
				statsActor.identity().bodyType()].sSpeed;
			if (gGameExternalOptions.fDisease &&
				gGameExternalOptions.fDiseaseSevereLimitations &&
				TacticalActorDisease::hasOutbreakProperty(
					actor,
					DISEASE_PROPERTY_LIMITED_USE_LEGS))
			{
				additional += BrokenLegPenalty;
			}
			additional = std::max<std::int16_t>(additional, 0);
		}
		break;

	case READY_RIFLE_STAND:
		actor.animationPlayback().delay() =
			actor.aiPlanning().aimTime() == 0 ? 100 : 200;
		adjustForTacticalSpeed(actor);
		return true;
	}

	if (gAnimControl[animationState].uiFlags & ANIM_MOVING)
	{
		terrainDelay = TerrainSpeedModifiers[
			statsActor.position().terrainType()];
	}
	else
		terrainDelay = 40;

	if (!(actor.status().flags() & SOLDIER_VEHICLE))
	{
		breathDeficit = 50 - (statsActor.vitals().breath() / 2);
		breathDeficit = std::min<std::int8_t>(breathDeficit, 30);
		agilityDeficit =
			50 - (EffectiveAgility(&statsActor, FALSE) / 4);
		lifeDeficit = 50 - (statsActor.vitals().health() / 2);
	}
	else
	{
		agilityDeficit = animationState == RUNNING ? 10 : 30;
	}

	terrainDelay +=
		lifeDeficit + breathDeficit + agilityDeficit + additional;
	switch (animationState)
	{
	case WALKING:
	case WALKING_WEAPON_RDY:
	case WALKING_DUAL_RDY:
	case CROUCHEDMOVE_RIFLE_READY:
	case CROUCHEDMOVE_PISTOL_READY:
	case CROUCHEDMOVE_DUAL_READY:
	case WALKING_ALTERNATIVE_RDY:
	case RUNNING:
	case SWATTING:
	case SWATTING_WK:
	case SIDE_STEP_CROUCH_RIFLE:
	case SIDE_STEP_CROUCH_PISTOL:
	case SIDE_STEP_CROUCH_DUAL:
	case SWAT_BACKWARDS_WK:
		terrainDelay = static_cast<std::int16_t>(
			terrainDelay *
			(100 - TacticalActorModifiers::backgroundValue(
				actor,
				BG_PERC_SPEED_RUNNING)) /
			100);
		break;
	default:
		break;
	}

	actor.animationPlayback().delay() = terrainDelay;
	if ((gAnimControl[animationState].uiFlags & ANIM_MOVING) &&
		actor.drugState().magnitude(DRUG_EFFECT_AP))
	{
		actor.animationPlayback().delay() /= 2;
	}
	if (IsJa2TacticalTurnBasedCombat())
		actor.animationPlayback().delay() /= 2;

	if (!IsJa2TacticalCombatActive())
	{
		if (statsActor.movement().stealthMode())
		{
			if (gGameOptions.fNewTraitSystem &&
				HAS_SKILL_TRAIT(&actor, STEALTHY_NT))
			{
				actor.animationPlayback().delay() =
					static_cast<std::int16_t>(
						actor.animationPlayback().delay() *
						(200 - gSkillTraitValues.ubSTStealthModeSpeedBonus) /
						100);
			}
			else
				actor.animationPlayback().delay() *= 2;
		}

		if (gGameOptions.fNewTraitSystem &&
			(gAnimControl[animationState].uiFlags & ANIM_MOVING) &&
			HAS_SKILL_TRAIT(&actor, ATHLETICS_NT))
		{
			actor.animationPlayback().delay() =
				static_cast<std::int16_t>(
					actor.animationPlayback().delay() *
					(100 - std::min<std::uint8_t>(
						75,
						gSkillTraitValues.ubATAPsMovementReduction)) /
					100);
		}
	}

	if (TacticalActorEquipment::hasEquippedRiotShield(actor))
	{
		actor.animationPlayback().delay() =
			static_cast<std::int16_t>(
				gItemSettings.fShieldMovementAPCostModifier *
				actor.animationPlayback().delay());
	}
	if (TacticalActorDragging::isDragging(actor))
	{
		actor.animationPlayback().delay() =
			static_cast<std::int16_t>(
				gItemSettings.fDragAPCostModifier *
				actor.animationPlayback().delay());
	}
	return true;
}
}

float TacticalActorAnimationTiming::currentTeamSpeedFactor() noexcept
{
	switch (GetJa2TacticalCurrentTeam())
	{
	case OUR_TEAM:
		return gGameExternalOptions.giPlayerTurnSpeedUpFactor;
	case ENEMY_TEAM:
		return gGameExternalOptions.giEnemyTurnSpeedUpFactor;
	case CREATURE_TEAM:
		return gGameExternalOptions.giCreatureTurnSpeedUpFactor;
	case MILITIA_TEAM:
		return gGameExternalOptions.giMilitiaTurnSpeedUpFactor;
	case CIV_TEAM:
		return gGameExternalOptions.giCivilianTurnSpeedUpFactor;
	}
	return 1.0F;
}

bool TacticalActorAnimationTiming::refresh(TacticalActor& actor)
{
	if (!hasAnimationContext(actor))
		return false;

	DebugMsg(
		TOPIC_JA2,
		DBG_LEVEL_3,
		"TacticalActorAnimationTiming::refresh");
	if (!is_client &&
		(IsJa2TacticalTurnBasedCombat() ||
		 gTacticalStatus.fAutoBandageMode) &&
		((actor.awareness().visibility() == -1 &&
		  actor.awareness().visibility() ==
			actor.awareness().lastRenderedVisibility()) ||
		 gTacticalStatus.fAutoBandageMode) &&
		actor.animationPlayback().state() != MONSTER_UP)
	{
		actor.animationPlayback().delay() =
			actor.fireControl().burstCounter() &&
			actor.roster().team() != gbPlayerNum
				? 50 : 0;
		actor.timing().start(
			SoldierTimingComponent::Timer::AnimationUpdate,
			actor.animationPlayback().delay());
		return true;
	}

	TacticalActor* statsActor = &actor;
	if (actor.movement().usesMoveSpeedOverride() &&
		actor.movement().moveSpeedOverride() < NOBODY)
	{
		TacticalActor* const resolved =
			GetJa2SoldierRepository().resolve(
				actor.movement().moveSpeedOverride());
		if (resolved != nullptr && hasAnimationContext(*resolved))
			statsActor = resolved;
	}

	actor.animationPlayback().delay() =
		gAnimControl[actor.animationPlayback().state()].sSpeed;
	if (actor.animationPlayback().delay() == 0 &&
		!calculateDelay(actor, *statsActor))
	{
		return false;
	}
	adjustForTacticalSpeed(actor);

	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if (animationState == NINJA_SPINKICK ||
		animationState == FOCUSED_PUNCH ||
		animationState == FOCUSED_STAB ||
		animationState == FOCUSED_HTH_KICK ||
		animationState == AI_RADIO ||
		animationState == AI_CR_RADIO)
	{
		actor.animationPlayback().delay() /= 2;
	}
	if (animationState == SIDE_STEP ||
		animationState == SIDE_STEP_ALTERNATIVE_RDY ||
		animationState == SIDE_STEP_WEAPON_RDY ||
		animationState == SIDE_STEP_DUAL_RDY)
	{
		actor.animationPlayback().delay() /= 4;
	}

	if (IsJa2TacticalTurnBasedCombat())
	{
		const float factor = currentTeamSpeedFactor();
		actor.animationPlayback().delay() = factor != 0.0F
			? static_cast<std::int16_t>(
				actor.animationPlayback().delay() * factor)
			: 0;
	}
	return true;
}

bool TacticalActorAnimationTiming::adjustForFastTurn(
	TacticalActor& actor)
{
	if (!hasAnimationContext(actor) ||
		actor.position().direction() < 0 ||
		actor.position().direction() >= NUM_WORLD_DIRECTIONS ||
		actor.pathing().desiredDirection() < 0 ||
		actor.pathing().desiredDirection() >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	if ((gAnimControl[actor.animationPlayback().state()].uiFlags &
		 ANIM_FASTTURN) &&
		actor.roster().team() == gbPlayerNum &&
		!(actor.status().flags() & SOLDIER_TURNINGFROMHIT))
	{
		if (actor.position().direction() !=
			actor.pathing().desiredDirection())
		{
			actor.animationPlayback().delay() =
				FAST_TURN_ANIM_SPEED;
		}
		else
			return refresh(actor);
	}
	return true;
}
