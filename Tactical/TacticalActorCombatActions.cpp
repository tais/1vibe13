#include "TacticalActorDamageResolution.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorCombatActions.h"

#include "TacticalActorOrientation.h"
#include "TacticalActorRouteExecution.h"

#include "Animation Control.h"
#include "Drugs And Alcohol.h"
#include "Explosion Control.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Meanwhile.h"
#include "Overhead.h"
#include "Rotting Corpses.h"
#include "TacticalActor.h"
#include "Soldier Functions.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "Soldier macros.h"
#include "Sound Control.h"
#include "TacticalActorConditions.h"
#include "TacticalActorDisease.h"
#include "TacticalActorRecovery.h"
#include "TacticalWorldAdapter.h"
#include "World Tile Map.h"
#include "ai.h"
#include "faces.h"
#include "opplist.h"
#include "random.h"
#include "worlddef.h"
#include "worldman.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern UINT16 usForceAnimState;

namespace
{
bool hasLiveCombatContext(
	const TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.vitals().health() >= OKLIFE &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		!TileIsOutOfBounds(targetGrid) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.position().direction() <
			NUM_WORLD_DIRECTIONS &&
		direction < NUM_WORLD_DIRECTIONS &&
		actor.animationPlayback().state() <
			NUMANIMATIONSTATES;
}

TacticalActor* resolveTarget(
	const TacticalActor& actor,
	std::int32_t targetGrid)
{
	const SoldierID targetId =
		WhoIsThere2(targetGrid, actor.position().level());
	TacticalActor* const target =
		GetJa2SoldierRepository().resolve(targetId);
	if (!target ||
		target == &actor ||
		!target->roster().active() ||
		!target->roster().inSector() ||
		target->position().gridNo() != targetGrid ||
		target->position().level() !=
			actor.position().level() ||
		target->position().direction() >=
			NUM_WORLD_DIRECTIONS ||
		target->animationPlayback().state() >=
			NUMANIMATIONSTATES)
	{
		return nullptr;
	}
	return target;
}

std::uint16_t handItem(
	const TacticalActor& actor) noexcept
{
	if (HANDPOS >= actor.inventory().size())
		return NOTHING;

	const OBJECTTYPE& object = actor.inventory()[HANDPOS];
	return object.usItem < MAXITEMS
		? object.usItem
		: NOTHING;
}

void markMeleeApproach(
	TacticalActor& actor,
	TacticalActor& target)
{
	target.featureFlags().secondaryFlags() &=
		~(SOLDIER_BACK_ATTACK | SOLDIER_SNEAK_ATTACK);

	const std::uint8_t attackDirection =
		AIDirection(
			actor.position().gridNo(),
			target.position().gridNo());
	const std::uint8_t targetDirection =
		target.position().direction();
	if (attackDirection < NUM_WORLD_DIRECTIONS &&
		(attackDirection == targetDirection ||
		 attackDirection == gOneCDirection[targetDirection] ||
		 attackDirection == gOneCCDirection[targetDirection]))
	{
		target.featureFlags().secondaryFlags() |=
			SOLDIER_BACK_ATTACK;
	}

	const auto& knowledge =
		target.awareness().opponentKnowledge();
	const SoldierID actorId = actor.identity().id();
	if (knowledge[actorId] != SEEN_CURRENTLY &&
		knowledge[actorId] != SEEN_THIS_TURN &&
		knowledge[actorId] != HEARD_THIS_TURN)
	{
		target.featureFlags().secondaryFlags() |=
			SOLDIER_SNEAK_ATTACK;
	}
}

void faceForMelee(
	TacticalActor& actor,
	std::uint8_t direction)
{
	if (actor.position().direction() == direction)
		return;

	const std::uint16_t animation =
		actor.animationPlayback().state();
	if (animation != CRAWLING &&
		gAnimControl[animation].ubEndHeight == ANIM_PRONE)
	{
		usForceAnimState = CROUCHING;
	}

	actor.status().flags() |= SOLDIER_LOOK_NEXT_TURNSOLDIER;
	(void)TacticalActorOrientation::setDesiredDirection(actor, direction);
	(void)TacticalActorOrientation::setDirection(actor, direction);
	if (actor.animationActivity().turningFromProneMode())
	{
		actor.animationActivity().turningFromProneMode() =
			TURNING_FROM_PRONE_FOR_PUNCH_OR_STAB;
	}
	usForceAnimState = INVALID_ANIMATION;
}

void turnAwareTarget(
	TacticalActor& attacker,
	TacticalActor& target)
{
	if (target.status().flags() &
		(SOLDIER_MONSTER |
		 SOLDIER_ANIMAL |
		 SOLDIER_VEHICLE))
	{
		return;
	}

	(void)TacticalActorRouteExecution::stopAt(target,
		target.position().gridNo(),
		target.position().direction());
	if (target.roster().team() != gbPlayerNum)
	{
		DebugAI(
			AI_MSG_INFO,
			&target,
			String("CancelAIAction: begin melee attack"));
		CancelAIAction(&target, TRUE);
	}

	const std::uint8_t direction =
		static_cast<std::uint8_t>(
			GetDirectionFromGridNo(
				attacker.position().gridNo(),
				&target));
	if (direction < NUM_WORLD_DIRECTIONS)
		SendSoldierSetDesiredDirectionEvent(&target, direction);
}

void beginUprightBladeAnimation(
	TacticalActor& actor)
{
	const std::uint16_t actorAnimation =
		actor.animationPlayback().state();
	if (actor.attackSelection().weaponMode() ==
			WM_ATTACHED_BAYONET &&
		gAnimControl[actorAnimation].ubEndHeight ==
			ANIM_STAND)
	{
		if (actorAnimation == RUNNING ||
			actorAnimation == RUNNING_W_PISTOL)
		{
			actor.featureFlags().secondaryFlags() |=
				SOLDIER_BAYONET_RUNBONUS;
		}
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			BAYONET_STAB_STANDING_VS_STANDING,
			0,
			FALSE);
	}
	else if (
		gGameExternalOptions.fEnhancedCloseCombatSystem &&
		actor.aiPlanning().aimTime() > 0)
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			FOCUSED_STAB,
			0,
			FALSE);
	}
	else
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			Chance(50) ? STAB : SLICE,
			0,
			FALSE);
	}
}

void beginProneBladeAnimation(
	TacticalActor& actor)
{
	const std::uint16_t actorAnimation =
		actor.animationPlayback().state();
	if (actor.attackSelection().weaponMode() ==
			WM_ATTACHED_BAYONET &&
		gAnimControl[actorAnimation].ubEndHeight ==
			ANIM_STAND)
	{
		if (actorAnimation == RUNNING ||
			actorAnimation == RUNNING_W_PISTOL)
		{
			actor.featureFlags().secondaryFlags() |=
				SOLDIER_BAYONET_RUNBONUS;
		}
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			BAYONET_STAB_STANDING_VS_PRONE,
			0,
			FALSE);
	}
	else if (
		gAnimControl[actorAnimation].ubEndHeight !=
			ANIM_CROUCH)
	{
		SendChangeSoldierStanceEvent(&actor, ANIM_CROUCH);
		actor.animationIntent().pendingAnimation() =
			CROUCH_STAB;
	}
	else
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			CROUCH_STAB,
			0,
			FALSE);
	}
}

bool isMartialArtist(TacticalActor& actor)
{
	if (gGameOptions.fNewTraitSystem)
	{
		const int requiredTraits =
			gSkillTraitValues.fPermitExtraAnimationsOnlyToMA
				? 2
				: 1;
		return NUM_SKILL_TRAITS(
				&actor,
				MARTIAL_ARTS_NT) >= requiredTraits;
	}
	return HAS_SKILL_TRAIT(&actor, MARTIALARTS_OT);
}

void simulateZombiePunch(
	TacticalActor& actor,
	TacticalActor& target)
{
	if (gGameExternalOptions.fZombieExplodingCivs)
	{
		if (actor.vitals().health() > 2)
			actor.vitals().health() = 2;

		constexpr std::uint16_t zombieExplosionItem = 136;
		if (zombieExplosionItem < MAXITEMS)
		{
			IgniteExplosion(
				actor.identity().id(),
				actor.position().worldXInt(),
				actor.position().worldYInt(),
				GetMapElement(
					static_cast<std::uint32_t>(
						actor.position().gridNo()))
					.sHeight,
				actor.position().gridNo(),
				zombieExplosionItem,
				actor.position().level());
		}
	}
	else
	{
		if (Random(100) > 30)
		{
			PlayJA2SampleFromFile(
				"Sounds\\zombie_swish1.wav",
				RATE_11025,
				HIGHVOLUME,
				1,
				MIDDLEPAN);

			const INT8 oldLife = target.vitals().health();
			INT16 damage =
				static_cast<INT16>(5 + Random(20));
			if (target.vitals().health() - damage < 0)
				damage = oldLife;
			if (oldLife >= OKLIFE && oldLife <= damage)
			{
				damage = std::max<INT16>(
					1,
					damage -
						static_cast<INT16>(
							5 + Random(5)));
			}

			INT16 breathDamage =
				static_cast<INT16>(500 + Random(1500));
			if (target.vitals().breath() - breathDamage < 0)
				breathDamage = target.vitals().breath();

			TacticalActorDamageResolution::takeDamage(target,
				0,
				damage,
				breathDamage,
				TAKE_DAMAGE_HANDTOHAND,
				actor.identity().id(),
				target.position().gridNo(),
				0,
				TRUE);
			if (target.vitals().health() <= 0)
			{
				HandleTakeDamageDeath(
					&target,
					oldLife,
					TAKE_DAMAGE_BLOODLOSS);
			}
			else if (
				target.vitals().health() < OKLIFE &&
				!target.collapseState().tactical())
			{
				(void)TacticalActorRecovery::collapse(target);
			}
		}
		else
		{
			PlayJA2SampleFromFile(
				"Sounds\\zombie_swish2.wav",
				RATE_11025,
				HIGHVOLUME,
				1,
				MIDDLEPAN);
		}
		EndAIGuysTurn(&actor);
	}

	TacticalActorAnimationTransitions::initializeAnimation(actor, RUNNING, 0, FALSE);
}

void beginUprightPunchAnimation(
	TacticalActor& actor,
	TacticalActor& target,
	std::uint8_t direction,
	std::uint16_t item,
	bool cannotKick)
{
	if (ItemIsCrowbar(item))
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			CROWBAR_ATTACK,
			0,
			FALSE);
		return;
	}

	const bool enhanced =
		gGameExternalOptions.fEnhancedCloseCombatSystem &&
		actor.aiPlanning().aimTime() > 0;
	const bool targetStanding =
		gAnimControl[target.animationPlayback().state()]
			.ubEndHeight == ANIM_STAND;
	const bool diagonal = (direction & 1) != 0;
	const auto aimLocation =
		enhanced && !targetStanding
			? actor.attackSelection().shotLocation()
			: actor.attackSelection().meleeLocation();

	std::uint16_t animation = PUNCH;
	if (targetStanding)
	{
		if (aimLocation == AIM_SHOT_LEGS &&
			!diagonal &&
			!cannotKick)
		{
			animation =
				enhanced ? FOCUSED_HTH_KICK : HTH_KICK;
		}
		else if (aimLocation == AIM_SHOT_HEAD || diagonal)
		{
			animation =
				enhanced ? FOCUSED_PUNCH : PUNCH;
		}
		else if (cannotKick || Random(20) > 8)
		{
			animation =
				enhanced ? FOCUSED_PUNCH : PUNCH;
		}
		else
		{
			animation =
				enhanced ? FOCUSED_HTH_KICK : HTH_KICK;
		}
	}
	else if (
		aimLocation == AIM_SHOT_HEAD ||
		diagonal ||
		cannotKick)
	{
		if (cannotKick || Random(20) > 12 || diagonal)
		{
			animation =
				enhanced ? FOCUSED_PUNCH : PUNCH;
		}
		else
		{
			animation =
				enhanced ? FOCUSED_HTH_KICK : HTH_KICK;
		}
	}
	else
	{
		animation =
			enhanced ? FOCUSED_HTH_KICK : HTH_KICK;
	}

	TacticalActorAnimationTransitions::initializeAnimation(actor, animation, 0, FALSE);
}

void playNinjaAttackSound(TacticalActor& actor)
{
	if (actor.identity().profile() != 33)
		return;

	SOUNDPARMS parameters;
	std::memset(
		&parameters,
		0xff,
		sizeof(parameters));
	parameters.uiSpeed = RATE_11025;
	parameters.uiVolume =
		static_cast<std::int8_t>(
			CalculateSpeechVolume(HIGHVOLUME));
	if (actor.roster().team() != gbPlayerNum)
	{
		parameters.uiVolume = SoundVolume(
			static_cast<std::uint8_t>(
				parameters.uiVolume),
			actor.position().gridNo());
	}
	parameters.uiLoop = 1;
	parameters.uiPan =
		SoundDir(actor.position().gridNo());
	parameters.uiPriority = GROUP_PLAYER;

	std::uint32_t soundId = SOUND_ERROR;
	if (actor.animationPlayback().state() ==
		NINJA_SPINKICK)
	{
		soundId = SoundPlay(
			"BATTLESNDS\\033_CHOP2.WAV",
			&parameters);
	}
	else
	{
		soundId = SoundPlay(
			Random(2) == 0
				? "BATTLESNDS\\033_CHOP3.WAV"
				: "BATTLESNDS\\033_CHOP1.WAV",
			&parameters);
	}

	if (soundId == SOUND_ERROR)
		return;

	actor.dialogue().activeBattleSound() = soundId;
	const std::int32_t faceIndex =
		actor.renderBindings().faceIndex();
	if (faceIndex >= 0 &&
		faceIndex < NUM_FACE_SLOTS)
	{
		ExternSetFaceTalking(faceIndex, soundId);
	}
}
}

bool TacticalActorCombatActions::beginBladeAttack(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasLiveCombatContext(
			actor,
			targetGrid,
			direction))
	{
		return false;
	}

	faceForMelee(actor, direction);
	actor.targeting().gridNo() = targetGrid;
	actor.targeting().level() = actor.position().level();
	actor.targeting().targetId() =
		WhoIsThere2(targetGrid, actor.targeting().level());

	if (actor.status().flags() & SOLDIER_MONSTER)
	{
		TacticalActor* const target =
			resolveTarget(actor, targetGrid);
		if (target &&
			((target->vitals().health() < OKLIFE &&
			  target->vitals().health() > 0) ||
			 (target->vitals().breath() < OKBREATH &&
			  target->collapseState().tactical())))
		{
			actor.pendingAction().quaternaryData() =
				target->identity().id();
			actor.drugState().magnitude(DRUG_EFFECT_HP) += 10;
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				MONSTER_BEGIN_EATTING_FLESH,
				0,
				FALSE);
		}
		else
		{
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				PythSpacesAway(
					actor.position().gridNo(),
					targetGrid) <= 1
					? MONSTER_CLOSE_ATTACK
					: ADULTMONSTER_ATTACKING,
				0,
				FALSE);
		}
		return true;
	}

	if (actor.identity().bodyType() == BLOODCAT)
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor,
			handItem(actor) == BLOODCAT_CLAW_ATTACK
				? BLOODCAT_SWIPE
				: BLOODCAT_BITE_ANIM,
			0,
			FALSE);
		return true;
	}

	TacticalActor* const target =
		resolveTarget(actor, targetGrid);
	if (target)
	{
		markMeleeApproach(actor, *target);
		const std::uint8_t targetHeight =
			gAnimControl[target->animationPlayback().state()]
				.ubEndHeight;
		if (targetHeight == ANIM_STAND ||
			targetHeight == ANIM_CROUCH)
		{
			beginUprightBladeAnimation(actor);
			if (target->awareness().opponentKnowledge()[
					actor.identity().id()] != 0 ||
				target->roster().team() ==
					actor.roster().team())
			{
				turnAwareTarget(actor, *target);
			}
			return true;
		}
		if (targetHeight == ANIM_PRONE)
		{
			beginProneBladeAnimation(actor);
			return true;
		}
		return false;
	}

	if (!NewOKDestination(
			&actor,
			targetGrid,
			FALSE,
			actor.position().level()))
	{
		TacticalActorAnimationTransitions::initializeAnimation(actor, STAB, 0, FALSE);
		return true;
	}

	ROTTING_CORPSE* const corpse =
		GetCorpseAtGridNo(
			targetGrid,
			actor.position().level());
	const bool usableCorpse =
		corpse &&
		(IsValidDecapitationCorpse(corpse) ||
		 IsValidGutCorpse(corpse) ||
		 IsValidStripCorpse(corpse) ||
		 IsValidTakeCorpse(corpse));
	TacticalActorAnimationTransitions::initializeAnimation(actor,
		usableCorpse ? DECAPITATE : CROUCH_STAB,
		0,
		FALSE);
	return true;
}

bool TacticalActorCombatActions::beginPunchAttack(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasLiveCombatContext(
			actor,
			targetGrid,
			direction))
	{
		return false;
	}

	TacticalActor* const target =
		resolveTarget(actor, targetGrid);
	if (!target)
		return false;

	actor.targeting().gridNo() = targetGrid;
	actor.targeting().level() = actor.position().level();
	actor.targeting().targetId() = target->identity().id();
	markMeleeApproach(actor, *target);
	faceForMelee(actor, direction);

	const std::uint16_t item = handItem(actor);
	const bool unfinishedBusiness =
		GetGameContext().capabilities().isUnfinishedBusiness();
	bool canUseMartialAnimation =
		isMartialArtist(actor) &&
		!ItemIsCrowbar(item) &&
		actor.identity().bodyType() == REGMALE;
	if (!unfinishedBusiness)
	{
		canUseMartialAnimation =
			canUseMartialAnimation &&
			!AreInMeanwhile() &&
			!TacticalActorConditions::isZombie(actor) &&
			!(gGameExternalOptions
				  .fDiseaseSevereLimitations &&
			  TacticalActorDisease::hasOutbreakProperty(
				  actor,
				  DISEASE_PROPERTY_LIMITED_USE_LEGS));
	}

	if (canUseMartialAnimation)
	{
		if (actor.animationPlayback().state() !=
				NINJA_BREATH &&
			gAnimControl[actor.animationPlayback().state()]
					.ubHeight == ANIM_STAND &&
			gAnimControl[target->animationPlayback().state()]
					.ubHeight != ANIM_PRONE)
		{
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				NINJA_GOTOBREATH,
				0,
				FALSE);
		}
		else
		{
			(void)TacticalActorCombatActions::
				continueNinjaAttack(actor);
		}
	}
	else if (
		TacticalActorConditions::isZombie(actor) &&
		!IsAnimationValidForBodyType(&actor, PUNCH))
	{
		simulateZombiePunch(actor, *target);
	}
	else
	{
		bool cannotKick =
			TacticalActorConditions::isZombie(actor) ||
			actor.identity().bodyType() > REGFEMALE ||
			(HANDPOS < actor.inventory().size() &&
			 actor.inventory()[HANDPOS].exists());
		if (!cannotKick &&
			gGameExternalOptions.fDisease &&
			gGameExternalOptions.fDiseaseSevereLimitations &&
			TacticalActorDisease::hasOutbreakProperty(
				actor,
				DISEASE_PROPERTY_LIMITED_USE_LEGS))
		{
			cannotKick = true;
		}

		const std::uint8_t targetHeight =
			gAnimControl[target->animationPlayback().state()]
				.ubEndHeight;
		if (targetHeight == ANIM_STAND ||
			targetHeight == ANIM_CROUCH)
		{
			beginUprightPunchAnimation(
				actor,
				*target,
				direction,
				item,
				cannotKick);
			if (target->awareness().opponentKnowledge()[
					actor.identity().id()] == 0 &&
				target->roster().team() !=
					actor.roster().team())
			{
				turnAwareTarget(actor, *target);
			}
		}
		else if (targetHeight == ANIM_PRONE)
		{
			if (actor.identity().bodyType() > REGFEMALE)
			{
				TacticalActorAnimationTransitions::initializeAnimation(actor,
					PUNCH,
					0,
					FALSE);
			}
			else if (
				gAnimControl[
					actor.animationPlayback().state()]
					.ubEndHeight != ANIM_CROUCH)
			{
				SendChangeSoldierStanceEvent(
					&actor,
					ANIM_CROUCH);
				actor.animationIntent().pendingAnimation() =
					PUNCH_LOW;
			}
			else
			{
				TacticalActorAnimationTransitions::initializeAnimation(actor,
					PUNCH_LOW,
					0,
					FALSE);
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool TacticalActorCombatActions::continueNinjaAttack(
	TacticalActor& actor)
{
	const std::int32_t targetGrid =
		actor.targeting().gridNo();
	if (!hasLiveCombatContext(
			actor,
			targetGrid,
			actor.position().direction()) ||
		actor.identity().id().i >= MAX_NUM_SOLDIERS)
	{
		return false;
	}

	TacticalActor* const target =
		resolveTarget(actor, targetGrid);
	if (!target)
		return false;

	const std::uint8_t targetStance =
		gAnimControl[
			target->animationPlayback().state()]
			.ubEndHeight;
	bool animationStarted = false;
	if (gGameExternalOptions
			.fEnhancedCloseCombatSystem &&
		actor.aiPlanning().aimTime() > 0 &&
		targetStance != ANIM_PRONE)
	{
		animationStarted =
			TacticalActorAnimationTransitions::changeState(actor,
				NINJA_SPINKICK,
				0,
				FALSE) != FALSE;
	}
	else if (
		!gGameExternalOptions
			.fEnhancedCloseCombatSystem &&
		(target->vitals().health() <= 30 ||
		 target->vitals().breath() <= 30) &&
		targetStance != ANIM_PRONE)
	{
		animationStarted =
			TacticalActorAnimationTransitions::changeState(actor,
				NINJA_SPINKICK,
				0,
				FALSE) != FALSE;
	}
	else if (targetStance != ANIM_PRONE)
	{
		animationStarted =
			TacticalActorAnimationTransitions::changeState(actor,
				Random(2) == 0
					? NINJA_LOWKICK
					: NINJA_PUNCH,
				0,
				FALSE) != FALSE;

		if (target->awareness().opponentKnowledge()[
				actor.identity().id()] == 0 &&
			target->roster().team() !=
				actor.roster().team() &&
			!(target->status().flags() &
				(SOLDIER_MONSTER |
				 SOLDIER_ANIMAL |
				 SOLDIER_VEHICLE)))
		{
			const std::uint8_t targetDirection =
				GetDirectionFromGridNo(
					actor.position().gridNo(),
					target);
			if (targetDirection <
				NUM_WORLD_DIRECTIONS)
			{
				SendSoldierSetDesiredDirectionEvent(
					target,
					targetDirection);
			}
		}
	}
	else if (
		gAnimControl[
			actor.animationPlayback().state()]
			.ubEndHeight != ANIM_CROUCH)
	{
		SendChangeSoldierStanceEvent(
			&actor,
			ANIM_CROUCH);
		actor.animationIntent().pendingAnimation() =
			PUNCH_LOW;
		animationStarted = true;
	}
	else
	{
		animationStarted =
			TacticalActorAnimationTransitions::initializeAnimation(actor,
				PUNCH_LOW,
				0,
				FALSE) != FALSE;
	}

	if (animationStarted)
		playNinjaAttackSound(actor);
	return animationStarted;
}

bool TacticalActorCombatActions::beginKnifeThrow(
	TacticalActor& actor,
	std::int32_t targetGrid,
	std::uint8_t direction)
{
	if (!hasLiveCombatContext(
			actor,
			targetGrid,
			direction) ||
		actor.targeting().level() < FIRST_LEVEL ||
		actor.targeting().level() > SECOND_LEVEL ||
		HANDPOS >= actor.inventory().size())
	{
		return false;
	}

	const OBJECTTYPE& object = actor.inventory()[HANDPOS];
	if (!object.exists() ||
		object.usItem >= MAXITEMS ||
		object.objectStack.empty() ||
		!object[0] ||
		Item[object.usItem].usItemClass !=
			IC_THROWING_KNIFE)
	{
		return false;
	}

	const bool specialBigMercThrow =
		actor.identity().bodyType() == BIGMALE &&
		(DoesMercHavePersonality(
			 &actor,
			 CHAR_TRAIT_SHOWOFF) ||
		 (gGameOptions.fNewTraitSystem &&
		  HAS_SKILL_TRAIT(&actor, THROWING_NT)) ||
		 (!gGameOptions.fNewTraitSystem &&
		  HAS_SKILL_TRAIT(&actor, THROWING_OT)));
	TacticalActorAnimationTransitions::initializeAnimation(actor,
		specialBigMercThrow
			? THROW_KNIFE_SP_BM
			: THROW_KNIFE,
		0,
		FALSE);

	if (actor.animationPlayback().state() == THROW_KNIFE ||
		actor.animationPlayback().state() ==
			THROW_KNIFE_SP_BM)
	{
		usForceAnimState =
			actor.animationPlayback().state();
	}
	else if (
		actor.animationIntent().pendingAnimation() ==
			THROW_KNIFE ||
		actor.animationIntent().pendingAnimation() ==
			THROW_KNIFE_SP_BM)
	{
		usForceAnimState =
			actor.animationIntent().pendingAnimation();
	}
	else
	{
		usForceAnimState = INVALID_ANIMATION;
	}

	actor.targeting().retainLastTargetFromTurn() = TRUE;
	if (actor.position().direction() != direction)
	{
		actor.status().flags() |=
			SOLDIER_LOOK_NEXT_TURNSOLDIER;
		(void)TacticalActorOrientation::setDesiredDirection(actor, direction);
		(void)TacticalActorOrientation::setDirection(actor, direction);
	}
	usForceAnimState = INVALID_ANIMATION;

	actor.targeting().gridNo() = targetGrid;
	actor.animationActivity().turningFromProneMode() = 0;
	actor.targeting().targetId() =
		WhoIsThere2(targetGrid, actor.targeting().level());
	return true;
}
