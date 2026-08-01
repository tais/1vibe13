#include "TacticalActorRecovery.h"

#include "TacticalActorOrientation.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "Boxing.h"
#include "Dialogue Control.h"
#include "DEBUG.H"
#include "DynamicDialogue.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Meanwhile.h"
#include "Overhead.h"
#include "Points.h"
#include "SkillCheck.h"
#include "Soldier Control.h"
#include "Soldier Profile.h"
#include "Soldier macros.h"
#include "TacticalActorCombatReactions.h"
#include "TacticalActorDragging.h"
#include "TacticalActorMedicalServices.h"
#include "TacticalActorMobility.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "ai.h"
#include "connect.h"
#include "opplist.h"
#include "random.h"
#include "structure.h"
#include "worlddef.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace
{
bool hasLiveRecoveryContext(
	const TacticalActor& actor) noexcept
{
	const std::uint8_t profile =
		actor.identity().profile();
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		actor.identity().bodyType() < TOTALBODYTYPES &&
		(profile == NO_PROFILE || profile < NUM_PROFILES) &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.position().direction() <
			NUM_WORLD_DIRECTIONS &&
		actor.animationPlayback().state() <
			NUMANIMATIONSTATES;
}

bool hasValidSleepDrugVitals(
	const TacticalActor& actor) noexcept
{
	return actor.vitals().maximumHealth() > 0 &&
		actor.vitals().maximumBreath() >= 0;
}

std::uint32_t sleepDartSuccumbChance(
	TacticalActor& actor)
{
	const std::int16_t effectiveStrength =
		EffectiveStrength(&actor, TRUE);
	std::int32_t chance = 0;
	if (effectiveStrength > 90)
		chance = 110 - effectiveStrength;
	else if (effectiveStrength > 80)
		chance = 120 - effectiveStrength;
	else if (effectiveStrength > 70)
		chance = 130 - effectiveStrength;
	else
		chance = 140 - effectiveStrength;

	const std::int32_t sleepCounter =
		std::clamp<std::int32_t>(
			actor.collapseState().sleepDrugCounter(),
			0,
			10);
	chance += 10 - sleepCounter;
	return static_cast<std::uint32_t>(
		std::max<std::int32_t>(chance, 0));
}

STRUCTURE_FILE_REF* getUpStructure(
	TacticalActor& actor,
	std::uint16_t animation)
{
	const std::uint16_t surface =
		DetermineSoldierAnimationSurface(
			&actor,
			animation);
	return GetAnimationStructureRef(
		actor.identity().id(),
		surface,
		animation);
}

bool hasRoomToGetUp(TacticalActor& actor)
{
	STRUCTURE_FILE_REF* structure = nullptr;
	if (actor.identity().bodyType() <= REGFEMALE)
	{
		switch (actor.animationPlayback().state())
		{
		case FALLBACKHIT_STOP:
		case FALLOFF_STOP:
		case FLYBACKHIT_STOP:
		case FALLBACK_HIT_STAND:
		case FALLOFF:
		case FLYBACK_HIT:
			structure = getUpStructure(actor, ROLLOVER);
			break;

		default:
			structure = getUpStructure(actor, ANIM_CROUCH);
			break;
		}
	}
	else if (!(actor.status().flags() & SOLDIER_VEHICLE))
	{
		structure = getUpStructure(actor, END_COWER);
	}

	if (!structure)
		return true;
	if (!structure->pDBStructureRef)
		return false;

	return OkayToAddStructureToWorld(
		actor.position().gridNo(),
		actor.position().level(),
		&structure->pDBStructureRef[
			gOneCDirection[
				actor.position().direction()]],
		actor.identity().id(),
		FALSE,
		NOBODY) != FALSE;
}

void cancelDraggingActor(TacticalActor& actor)
{
	for (std::size_t slot = 0;
		 slot < Ja2ActiveTacticalActorSlotCount();
		 ++slot)
	{
		TacticalActor* const candidate =
			ResolveJa2ActiveTacticalActorSlot(slot);
		if (candidate &&
			candidate->interaction().draggedPerson() ==
				actor.identity().id())
		{
			TacticalActorDragging::cancel(*candidate);
		}
	}
}

void startGetUpAnimation(TacticalActor& actor)
{
	if (actor.identity().bodyType() <= REGFEMALE)
	{
		switch (actor.animationPlayback().state())
		{
		case FALLBACKHIT_STOP:
		case FALLOFF_STOP:
		case FLYBACKHIT_STOP:
		case FALLBACK_HIT_STAND:
		case FALLOFF:
		case FLYBACK_HIT:
			actor.EVENT_InitNewSoldierAnim(
				ROLLOVER,
				0,
				FALSE);
			break;

		default:
			(void)TacticalActorOrientation::changeStance(actor, ANIM_CROUCH);
			break;
		}
	}
	else if (!(actor.status().flags() & SOLDIER_VEHICLE))
	{
		actor.EVENT_InitNewSoldierAnim(
			END_COWER,
			0,
			FALSE);
	}
}
}

bool TacticalActorRecovery::applySleepDart(
	TacticalActor& actor,
	std::int16_t& breathLoss)
{
	if (!hasLiveRecoveryContext(actor) ||
		!hasValidSleepDrugVitals(actor))
		return false;

	actor.collapseState().sleepDrugCounter() = 10;
	if (PreRandom(100) < sleepDartSuccumbChance(actor))
	{
		breathLoss = static_cast<std::int16_t>(
			actor.vitals().maximumBreath() * 100);
	}
	return true;
}

bool TacticalActorRecovery::checkBreathCollapse(
	TacticalActor& actor)
{
	if (!hasLiveRecoveryContext(actor))
		return false;

	if (is_networked &&
		actor.roster().team() >= LAN_TEAM_ONE)
	{
		return false;
	}

	if (actor.vitals().maximumBreath() > 70 &&
		actor.vitals().breath() < 20 &&
		!actor.dialogue().hasSaid(
			SOLDIER_QUOTE_SAID_LOW_BREATH) &&
		gAnimControl[
			actor.animationPlayback().state()]
			.ubEndHeight == ANIM_STAND &&
		!actor.service().hasProviders())
	{
		if (MercIsHot(&actor) &&
			actor.employment().mercenaryType() !=
				MERC_TYPE__PLAYER_CHARACTER)
		{
			TacticalCharacterDialogue(
				&actor,
				QUOTE_PERSONALITY_TRAIT);
			if (gGameExternalOptions.fDynamicOpinions)
			{
				HandleDynamicOpinionChange(
					&actor,
					OPINIONEVENT_ANNOYINGDISABILITY,
					TRUE,
					TRUE);
			}
		}
		else
		{
			TacticalCharacterDialogue(
				&actor,
				QUOTE_OUT_OF_BREATH);
		}
		actor.dialogue().markSaid(
			SOLDIER_QUOTE_SAID_LOW_BREATH);
	}

	if (actor.vitals().breath() == 0 &&
		!actor.collapseState().tactical() &&
		!(actor.status().flags() &
			(SOLDIER_VEHICLE |
			 SOLDIER_ANIMAL |
			 SOLDIER_MONSTER)) &&
		!actor.service().hasProviders())
	{
		actor.collapseState().markBreathCollapse();
		return true;
	}

	return false;
}

bool TacticalActorRecovery::collapse(
	TacticalActor& actor)
{
	if (!hasLiveRecoveryContext(actor))
		return false;

	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if (actor.collapseState().tactical() &&
		gAnimControl[animationState].ubEndHeight ==
			ANIM_PRONE)
	{
		return false;
	}

	switch (actor.identity().bodyType())
	{
	case ADULTFEMALEMONSTER:
	case AM_MONSTER:
	case YAF_MONSTER:
	case YAM_MONSTER:
	case LARVAE_MONSTER:
	case INFANT_MONSTER:
	case QUEENMONSTER:
		DeductPoints(&actor, 0, -5000);
		return true;
	}

	actor.collapseState().collapse();
	actor.movement().mode() = CRAWLING;
	TacticalActorMedicalServices::cancelReceiving(actor);
	HandleSight(&actor, SIGHT_LOOK);

	switch (gAnimControl[animationState].ubEndHeight)
	{
	case ANIM_STAND:
		if (TacticalActorMobility::inDeepWater(actor))
		{
			actor.EVENT_InitNewSoldierAnim(
				DEEP_WATER_DIE,
				0,
				FALSE);
		}
		else if (TacticalActorMobility::inShallowWater(actor))
		{
			actor.EVENT_InitNewSoldierAnim(
				WATER_DIE,
				0,
				FALSE);
		}
		else
		{
			(void)TacticalActorCombatReactions::beginFall(actor);
			if (is_networked)
			{
				actor.ChangeSoldierState(
					FALLFORWARD_FROMHIT_STAND,
					0,
					FALSE);
			}
			else
			{
				actor.EVENT_InitNewSoldierAnim(
					FALLFORWARD_FROMHIT_STAND,
					0,
					FALSE);
			}
		}
		break;

	case ANIM_CROUCH:
		(void)TacticalActorCombatReactions::beginFall(actor);
		if (is_networked)
		{
			actor.ChangeSoldierState(
				FALLFORWARD_FROMHIT_CROUCH,
				0,
				FALSE);
		}
		else
		{
			actor.EVENT_InitNewSoldierAnim(
				FALLFORWARD_FROMHIT_CROUCH,
				0,
				FALSE);
		}
		break;

	case ANIM_PRONE:
		switch (animationState)
		{
		case FALLFORWARD_FROMHIT_STAND:
		case ENDFALLFORWARD_FROMHIT_CROUCH:
			actor.ChangeSoldierState(
				STAND_FALLFORWARD_STOP,
				0,
				FALSE);
			break;

		case FALLBACK_HIT_STAND:
			actor.ChangeSoldierState(
				FALLBACKHIT_STOP,
				0,
				FALSE);
			break;

		default:
			if (is_networked)
			{
				actor.ChangeSoldierState(
					PRONE_LAY_FROMHIT,
					0,
					FALSE);
			}
			else
			{
				actor.EVENT_InitNewSoldierAnim(
					PRONE_LAY_FROMHIT,
					0,
					FALSE);
			}
			break;
		}
		break;
	}

	if (actor.status().flags() & SOLDIER_ENEMY)
	{
		if (gTacticalStatus.ubTheChosenOne ==
			actor.identity().id())
		{
			const std::int8_t panicTrigger =
				ClosestPanicTrigger(&actor);
			if (panicTrigger >= 0 &&
				panicTrigger < NUM_PANIC_TRIGGERS &&
				!gTacticalStatus
					.bPanicTriggerIsAlarm[panicTrigger])
			{
				gTacticalStatus.ubTheChosenOne = NOBODY;
				MakeClosestEnemyChosenOne();
			}
		}

		if (IsJa2TacticalTurnBasedCombat() &&
			(actor.status().flags() &
				SOLDIER_UNDERAICONTROL))
		{
#ifdef TESTAICONTROL
			DebugAI(String(
				"Ending turn for %d because of error from HandleItem",
				actor.identity().id()));
#endif
			EndAIGuysTurn(&actor);
		}
	}

	return true;
}

bool TacticalActorRecovery::beginGetUp(
	TacticalActor& actor)
{
	if (!hasLiveRecoveryContext(actor) ||
		actor.service().hasProviders())
	{
		return false;
	}

	if (!GetGameContext().capabilities()
			.isUnfinishedBusiness() &&
		AreInMeanwhile())
	{
		return false;
	}

	if (actor.collapseState().tactical())
	{
		if (actor.vitals().health() >= OKLIFE &&
			actor.vitals().breath() >= OKBREATH &&
			actor.collapseState().sleepDrugCounter() == 0 &&
			hasRoomToGetUp(actor))
		{
			cancelDraggingActor(actor);
			actor.collapseState().recover();
			startGetUpAnimation(actor);
		}
		else
		{
			actor.collapseState().turns()++;
			if (gTacticalStatus.bBoxingState == BOXING &&
				(actor.status().flags() & SOLDIER_BOXER) &&
				actor.collapseState().turns() > 1)
			{
				EndBoxingMatch(&actor);
			}
		}
	}
	else if (actor.collapseState().sleepDrugCounter() > 0)
	{
		if (!hasValidSleepDrugVitals(actor))
			return false;

		if (PreRandom(100) < sleepDartSuccumbChance(actor))
		{
			DeductPoints(
				&actor,
				0,
				static_cast<std::int16_t>(
					actor.vitals().maximumBreath() * 100));
			(void)TacticalActorRecovery::collapse(actor);
		}
	}

	if (actor.collapseState().sleepDrugCounter() > 0)
		--actor.collapseState().sleepDrugCounter();

	return true;
}
