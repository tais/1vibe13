#include "TacticalActorMobility.h"

#include "Disease.h"
#include "Soldier Profile Constants.h"
#include "TacticalActorStateFlags.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationTransitions.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "TacticalActorDisease.h"
#include "TacticalWorldAdapter.h"
#include "Overhead.h"
#include "structure.h"
#include "tiledef.h"
#include "worlddef.h"
#include "worldman.h"

#include <cstddef>

namespace
{
bool isKnownStance(std::int8_t stance) noexcept
{
	return stance == ANIM_STAND ||
		stance == ANIM_CROUCH ||
		stance == ANIM_PRONE;
}

bool readCurrentStance(
	const TacticalActor& actor,
	std::uint8_t& stance) noexcept
{
	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if (animationState >= NUMANIMATIONSTATES)
		return false;

	stance = gAnimControl[animationState].ubEndHeight;
	return isKnownStance(static_cast<std::int8_t>(stance));
}

bool isActiveEpc(const TacticalActor& actor) noexcept
{
	const auto profile = actor.identity().profile();
	return profile != NO_PROFILE &&
		profile < NUM_PROFILES &&
		(gMercProfiles[profile].ubMiscFlags &
			PROFILE_MISC_FLAG_EPCACTIVE) != 0;
}
}

bool TacticalActorMobility::inWater(
	const TacticalActor& actor) noexcept
{
	return TERRAIN_IS_WATER(actor.position().terrainType()) &&
		actor.position().level() <= 0;
}

bool TacticalActorMobility::inShallowWater(
	const TacticalActor& actor) noexcept
{
	return TERRAIN_IS_SHALLOW_WATER(
			actor.position().terrainType()) &&
		actor.position().level() <= 0;
}

bool TacticalActorMobility::inDeepWater(
	const TacticalActor& actor) noexcept
{
	return TERRAIN_IS_DEEP_WATER(
			actor.position().terrainType()) &&
		actor.position().level() <= 0;
}

bool TacticalActorMobility::inHighWater(
	const TacticalActor& actor) noexcept
{
	return TERRAIN_IS_HIGH_WATER(
			actor.position().terrainType()) &&
		actor.position().level() <= 0;
}

bool TacticalActorMobility::isFastMovement(
	TacticalActor& actor)
{
	if (actor.movement().fastUiMovement() &&
		gGameExternalOptions.fDisease &&
		gGameExternalOptions.fDiseaseSevereLimitations &&
		TacticalActorDisease::hasOutbreakProperty(
			actor,
			DISEASE_PROPERTY_LIMITED_USE_LEGS))
	{
		actor.movement().clearUiMovementFast();
	}

	return actor.movement().fastUiMovement();
}

std::uint16_t TacticalActorMobility::movementStateForStance(
	TacticalActor& actor,
	std::uint8_t stance)
{
	switch (stance)
	{
	case ANIM_STAND:
		return isFastMovement(actor) ? RUNNING : WALKING;

	case ANIM_PRONE:
		return CRAWLING;

	case ANIM_CROUCH:
		if (isFastMovement(actor))
			return SWATTING;

		if (HANDPOS < actor.inventory().size())
		{
			const OBJECTTYPE& hand = actor.inventory()[HANDPOS];
			if (hand.exists() &&
				hand.usItem < MAXITEMS &&
				(Item[hand.usItem].usItemClass == IC_BLADE ||
				 Item[hand.usItem].usItemClass ==
					IC_THROWING_KNIFE))
			{
				return SWATTING_WK;
			}
		}
		return SWATTING;

	default:
		return WALKING;
	}
}

std::uint16_t
TacticalActorMobility::movementStateForCurrentStance(
	TacticalActor& actor)
{
	std::uint8_t stance = ANIM_STAND;
	if (!readCurrentStance(actor, stance))
		return WALKING;

	return movementStateForStance(actor, stance);
}

std::uint16_t TacticalActorMobility::transitionStateForStance(
	const TacticalActor& actor,
	std::uint8_t desiredStance) noexcept
{
	const std::uint16_t currentState =
		actor.animationPlayback().state();
	if (currentState >= NUMANIMATIONSTATES)
		return STANDING;

	if (!isKnownStance(static_cast<std::int8_t>(desiredStance)))
		return currentState;

	const std::int8_t heightDelta =
		static_cast<std::int8_t>(
			desiredStance -
			gAnimControl[currentState].ubEndHeight);
	switch (heightDelta)
	{
	case ANIM_STAND - ANIM_CROUCH:
		return KNEEL_UP;
	case ANIM_CROUCH - ANIM_STAND:
		return KNEEL_DOWN;
	case ANIM_STAND - ANIM_PRONE:
		return PRONE_UP;
	case ANIM_PRONE - ANIM_STAND:
		return KNEEL_DOWN;
	case ANIM_CROUCH - ANIM_PRONE:
		return PRONE_UP;
	case ANIM_PRONE - ANIM_CROUCH:
		return PRONE_DOWN;
	default:
		return currentState;
	}
}

bool TacticalActorMobility::canClimbWithCurrentBackpack(
	TacticalActor& actor)
{
	if (!UsingNewInventorySystem() ||
		actor.roster().team() != OUR_TEAM ||
		BPACKPOCKPOS >= actor.inventory().size())
	{
		return true;
	}

	OBJECTTYPE& backpack = actor.inventory()[BPACKPOCKPOS];
	if (!backpack.exists())
		return true;

	if (backpack.usItem >= MAXITEMS ||
		backpack.objectStack.empty())
	{
		return false;
	}

	const std::int32_t adjustedWeight =
		static_cast<std::int32_t>(
			backpack.GetWeightOfObjectInStack()) +
		Item[backpack.usItem].sBackpackWeightModifier;
	const bool exceedsLimit =
		gGameExternalOptions.sBackpackWeightToClimb == -1 ||
		adjustedWeight >
			gGameExternalOptions.sBackpackWeightToClimb;
	const bool climbingIsRestricted =
		gGameExternalOptions.fUseGlobalBackpackSettings ||
		!ItemAllowsClimbing(backpack.usItem);

	return !(exceedsLimit && climbingIsRestricted);
}

bool TacticalActorMobility::isValidStance(
	TacticalActor& actor,
	std::int8_t direction,
	std::int8_t stance)
{
	if (direction < 0 ||
		direction >= NUM_WORLD_DIRECTIONS ||
		!isKnownStance(stance))
	{
		return false;
	}

	if ((actor.status().flags() & SOLDIER_VEHICLE) &&
		stance != ANIM_STAND)
	{
		return false;
	}

	if (inWater(actor) &&
		(stance == ANIM_PRONE || stance == ANIM_CROUCH))
	{
		return false;
	}

	if (actor.identity().bodyType() == ROBOTNOWEAPON &&
		stance != ANIM_STAND)
	{
		return false;
	}

	const auto profile = actor.identity().profile();
	if (profile != NO_PROFILE && profile >= NUM_PROFILES)
		return false;

	if (isActiveEpc(actor))
		return stance != ANIM_PRONE;

	if (actor.collapseState().tactical())
	{
		if (stance == ANIM_CROUCH)
			return false;

		if (stance == ANIM_STAND &&
			actor.identity().bodyType() <= REGFEMALE)
		{
			return false;
		}
	}

	if (TileIsOutOfBounds(actor.position().gridNo()))
		return false;

	const SoldierID actorId = actor.identity().id();
	auto& repository = GetJa2SoldierRepository();
	if (!IsJa2TacticalWorldLoaded() ||
		actorId == NOBODY ||
		actorId.i >= repository.capacity() ||
		repository.resolve(actorId.i) != &actor)
	{
		return false;
	}

	std::uint16_t excludedStructure = INVALID_STRUCTURE_ID;
	if (actor.renderBindings().levelNode() &&
		actor.renderBindings().levelNode()->pStructureData)
	{
		excludedStructure =
			actor.renderBindings()
				.levelNode()
				->pStructureData
				->usStructureID;
	}

	std::uint16_t animationState = STANDING;
	switch (stance)
	{
	case ANIM_STAND:
		animationState = STANDING;
		break;
	case ANIM_CROUCH:
		animationState = CROUCHING;
		break;
	case ANIM_PRONE:
		animationState = PRONE;
		break;
	}

	const std::uint16_t animationSurface =
		DetermineSoldierAnimationSurface(
			&actor,
			animationState);
	STRUCTURE_FILE_REF* const structure =
		GetAnimationStructureRef(
			actor.identity().id(),
			animationSurface,
			animationState);
	if (!structure)
		return true;

	return OkayToAddStructureToWorld(
		actor.position().gridNo(),
		actor.position().level(),
		&structure->pDBStructureRef[
			gOneCDirection[
				static_cast<std::uint8_t>(direction)]],
		excludedStructure);
}

bool TacticalActorMobility::isValidStance(
	TacticalActor& actor,
	std::int8_t stance)
{
	return actor.position().direction() >= 0 &&
		actor.position().direction() < NUM_WORLD_DIRECTIONS &&
		isValidStance(
			actor,
			actor.position().direction(),
			stance);
}

bool TacticalActorMobility::isValidMovementMode(
	const TacticalActor& actor,
	std::uint16_t movementMode) noexcept
{
	if (movementMode >= NUMANIMATIONSTATES)
		return false;
	return !inWater(actor) ||
		(movementMode != RUNNING &&
		 movementMode != SWATTING &&
		 movementMode != CRAWLING);
}

bool TacticalActorMobility::selectMovementForCurrentStance(
	TacticalActor& actor)
{
	std::uint8_t stance = 0;
	if (!readCurrentStance(actor, stance))
		return false;

	std::uint16_t movementState = WALKING;
	switch (stance)
	{
	case ANIM_STAND:
		movementState = WALKING;
		break;
	case ANIM_PRONE:
		movementState = CRAWLING;
		break;
	case ANIM_CROUCH:
		movementState = SWATTING;
		break;
	default:
		return false;
	}
	return TacticalActorAnimationTransitions::initializeAnimation(
		actor,
		movementState,
		0,
		FALSE);
}

bool TacticalActorMobility::isCurrentStanceValid(
	TacticalActor& actor,
	std::int8_t direction)
{
	std::uint8_t stance = ANIM_STAND;
	return readCurrentStance(actor, stance) &&
		isValidStance(
			actor,
			direction,
			static_cast<std::int8_t>(stance));
}

bool TacticalActorMobility::isCrouchedAgainstCover(
	const TacticalActor& actor,
	std::uint8_t direction)
{
	if (!actor.roster().active() ||
		direction >= NUM_WORLD_DIRECTIONS ||
		actor.animationPlayback().state() >=
			NUMANIMATIONSTATES ||
		gAnimControl[actor.animationPlayback().state()]
				.ubEndHeight != ANIM_CROUCH ||
		!actor.roster().inSector() ||
		!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(actor.position().gridNo()) ||
		(actor.status().flags() &
			(SOLDIER_DRIVER | SOLDIER_PASSENGER)))
	{
		return false;
	}

	const std::int32_t coverGridNo = NewGridNo(
		actor.position().gridNo(),
		DirectionInc(direction));
	if (TileIsOutOfBounds(coverGridNo) ||
		WhoIsThere2(
			coverGridNo,
			actor.position().level()) != NOBODY ||
		IsLocationSittable(
			coverGridNo,
			actor.position().level()))
	{
		return false;
	}

	return GetTallestStructureHeight(
			coverGridNo,
			actor.position().level()) >= 2;
}
