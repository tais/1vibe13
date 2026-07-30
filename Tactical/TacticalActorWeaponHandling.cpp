#include "TacticalActorWeaponHandling.h"

#include "Animation Control.h"
#include "DynamicDialogue.h"
#include "GameSettings.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Soldier Control.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"
#include "TacticalActorMobility.h"
#include "TacticalWorldAdapter.h"
#include "Vehicles.h"
#include "Weapons.h"
#include "Overhead.h"
#include "structure.h"
#include "worlddef.h"
#include "worldman.h"

#include <cstddef>

namespace
{
OBJECTTYPE* inventoryObject(
	TacticalActor& actor,
	std::size_t slot) noexcept
{
	if (slot >= actor.inventory().size())
		return nullptr;

	OBJECTTYPE& object = actor.inventory()[slot];
	if (!object.exists() ||
		object.usItem >= MAXITEMS ||
		object.objectStack.empty())
	{
		return nullptr;
	}

	return &object;
}

bool isOneHandedGun(const OBJECTTYPE* object) noexcept
{
	return object &&
		Item[object->usItem].usItemClass == IC_GUN &&
		!ItemIsTwoHanded(object->usItem);
}

bool isRobotOrArmedVehicle(
	const TacticalActor& actor) noexcept
{
	const auto profile = actor.identity().profile();
	const bool profileRobot =
		profile != NO_PROFILE &&
		profile < NUM_PROFILES &&
		gMercProfiles[profile].ubBodyType == ROBOTNOWEAPON;
	const auto bodyType = actor.identity().bodyType();
	return profileRobot ||
		bodyType == ROBOTNOWEAPON ||
		bodyType == TANK_NE ||
		bodyType == TANK_NW ||
		bodyType == COMBAT_JEEP;
}

bool isArmedVehicle(
	const TacticalActor& actor) noexcept
{
	const auto bodyType = actor.identity().bodyType();
	return bodyType == TANK_NE ||
		bodyType == TANK_NW ||
		bodyType == COMBAT_JEEP;
}

bool hasValidProfile(const TacticalActor& actor) noexcept
{
	const auto profile = actor.identity().profile();
	return profile == NO_PROFILE || profile < NUM_PROFILES;
}

void addMountingOpinion(
	const TacticalActor& support,
	const TacticalActor& shooter)
{
	if (!gGameExternalOptions.fDynamicOpinions ||
		!hasValidProfile(support) ||
		!hasValidProfile(shooter) ||
		support.identity().profile() == NO_PROFILE ||
		shooter.identity().profile() == NO_PROFILE)
	{
		return;
	}

	AddOpinionEvent(
		support.identity().profile(),
		shooter.identity().profile(),
		OPINIONEVENT_YOUMOUNTEDAGUNONMYBREASTS);
}

bool passengerWeaponIsMounted(TacticalActor& actor)
{
	const std::int32_t vehicleId =
		actor.deployment().vehicleId();
	if (!pVehicleList ||
		vehicleId < 0 ||
		vehicleId >= ubNumberOfVehicles ||
		!pVehicleList[vehicleId].fValid)
	{
		return false;
	}

	const std::uint8_t vehicleType =
		pVehicleList[vehicleId].ubVehicleType;
	if (vehicleType >= NUM_PROFILES)
		return false;

	const std::int8_t seat =
		GetSeatIndexFromSoldier(&actor);
	const std::int32_t capacity =
		GetVehicleSeatingCapacity(vehicleId);
	if (seat < 0 ||
		seat >= capacity ||
		seat >= MAXPASSENGERS)
	{
		return false;
	}

	return !gNewVehicle[vehicleType]
		.VehicleSeats[static_cast<std::size_t>(seat)]
		.fBlockedShots;
}
}

bool TacticalActorWeaponHandling::isValidSecondHandBurst(
	TacticalActor& actor)
{
	OBJECTTYPE* const hand =
		inventoryObject(actor, HANDPOS);
	OBJECTTYPE* const secondHand =
		inventoryObject(actor, SECONDHANDPOS);
	if (!isOneHandedGun(hand) ||
		!isOneHandedGun(secondHand) ||
		ItemIsGrenadeLauncher(hand->usItem) ||
		!actor.fireControl().burstCounter() ||
		(*secondHand)[0]->data.gun.bGunStatus < USABLE ||
		(*secondHand)[0]->data.gun.ubGunShotsLeft == 0)
	{
		return false;
	}

	if (actor.fireControl().autofireShots())
		return IsGunAutofireCapable(secondHand);

	return IsGunBurstCapable(
			secondHand,
			FALSE,
			nullptr) &&
		GetShotsPerBurst(hand) ==
			GetShotsPerBurst(secondHand);
}

bool TacticalActorWeaponHandling::isValidSecondHandShot(
	TacticalActor& actor)
{
	OBJECTTYPE* const hand =
		inventoryObject(actor, HANDPOS);
	OBJECTTYPE* const secondHand =
		inventoryObject(actor, SECONDHANDPOS);
	return isOneHandedGun(hand) &&
		isOneHandedGun(secondHand) &&
		!ItemIsGrenadeLauncher(hand->usItem) &&
		(!actor.fireControl().burstCounter() ||
			isValidSecondHandBurst(actor)) &&
		(*secondHand)[0]->data.gun.bGunStatus >= USABLE &&
		(*secondHand)[0]->data.gun.ubGunShotsLeft > 0;
}

bool TacticalActorWeaponHandling::isValidSecondHandShotForReloading(
	TacticalActor& actor)
{
	OBJECTTYPE* const hand =
		inventoryObject(actor, HANDPOS);
	OBJECTTYPE* const secondHand =
		inventoryObject(actor, SECONDHANDPOS);
	return hand &&
		secondHand &&
		Item[hand->usItem].usItemClass == IC_GUN &&
		Item[secondHand->usItem].usItemClass == IC_GUN &&
		!ItemIsGrenadeLauncher(hand->usItem) &&
		(*secondHand)[0]->data.gun.bGunStatus >= USABLE;
}

bool TacticalActorWeaponHandling::isValidShotFromHip(
	TacticalActor& actor,
	std::int16_t aimTime,
	std::int32_t targetGridNo)
{
	OBJECTTYPE* const hand =
		inventoryObject(actor, HANDPOS);
	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if (!gGameExternalOptions.ubAllowAlternativeWeaponHolding ||
		!hand ||
		Item[hand->usItem].usItemClass != IC_GUN ||
		animationState >= NUMANIMATIONSTATES ||
		gAnimControl[animationState].ubEndHeight != ANIM_STAND ||
		!hasValidProfile(actor) ||
		isRobotOrArmedVehicle(actor) ||
		!ItemIsTwoHanded(hand->usItem))
	{
		return false;
	}

	if (gGameExternalOptions.ubAllowAlternativeWeaponHolding == 2)
	{
		const std::uint32_t animationFlags =
			gAnimControl[animationState].uiFlags;
		if ((animationFlags & (ANIM_FIREREADY | ANIM_FIRE)) &&
			!(animationFlags & ANIM_ALT_WEAPON_HOLDING))
		{
			return false;
		}

		if (aimTime >
				GetNumberAltFireAimLevels(
					&actor,
					targetGridNo) &&
			!Weapon[hand->usItem].HeavyGun)
		{
			return false;
		}
	}
	else if (
		gGameExternalOptions.ubAllowAlternativeWeaponHolding == 3)
	{
		if (actor.attackSelection().scopeMode() !=
			USE_ALT_WEAPON_HOLD)
		{
			return false;
		}
	}
	else if (aimTime > 0)
	{
		return false;
	}

	return true;
}

bool TacticalActorWeaponHandling::isValidPistolFastShot(
	TacticalActor& actor,
	std::int16_t aimTime,
	std::int32_t targetGridNo)
{
	OBJECTTYPE* const hand =
		inventoryObject(actor, HANDPOS);
	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if (!gGameExternalOptions.ubAllowAlternativeWeaponHolding ||
		!hand ||
		Item[hand->usItem].usItemClass != IC_GUN ||
		animationState >= NUMANIMATIONSTATES ||
		gAnimControl[animationState].ubEndHeight != ANIM_STAND ||
		!hasValidProfile(actor) ||
		isRobotOrArmedVehicle(actor) ||
		TacticalActorMobility::inWater(actor) ||
		isValidSecondHandShot(actor) ||
		ItemIsTwoHanded(hand->usItem))
	{
		return false;
	}

	if (gGameExternalOptions.ubAllowAlternativeWeaponHolding == 2)
	{
		const std::uint32_t animationFlags =
			gAnimControl[animationState].uiFlags;
		if ((animationFlags & (ANIM_FIREREADY | ANIM_FIRE)) &&
			!(animationFlags & ANIM_ALT_WEAPON_HOLDING))
		{
			return false;
		}

		if (aimTime >
			GetNumberAltFireAimLevels(
				&actor,
				targetGridNo))
		{
			return false;
		}
	}
	else if (
		gGameExternalOptions.ubAllowAlternativeWeaponHolding == 3)
	{
		if (actor.attackSelection().scopeMode() !=
			USE_ALT_WEAPON_HOLD)
		{
			return false;
		}
	}
	else if (aimTime > 0)
	{
		return false;
	}

	return true;
}

bool TacticalActorWeaponHandling::isValidAlternativeFireMode(
	TacticalActor& actor,
	std::int16_t aimTime,
	std::int32_t targetGridNo)
{
	return isValidShotFromHip(
			actor,
			aimTime,
			targetGridNo) ||
		isValidPistolFastShot(
			actor,
			aimTime,
			targetGridNo);
}

bool TacticalActorWeaponHandling::isWeaponMounted(
	TacticalActor& actor)
{
	const std::uint16_t animationState =
		actor.animationPlayback().state();
	if (!actor.roster().active() ||
		!actor.roster().inSector() ||
		!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(actor.position().gridNo()) ||
		animationState >= NUMANIMATIONSTATES)
	{
		return false;
	}

	if (isArmedVehicle(actor))
		return true;

	if (actor.status().flags() &
		(SOLDIER_DRIVER | SOLDIER_PASSENGER))
	{
		return passengerWeaponIsMounted(actor);
	}

	if (!WeaponReady(&actor))
		return false;

	const ANIMCONTROLTYPE& animation =
		gAnimControl[animationState];
	if (animation.ubEndHeight == ANIM_PRONE)
		return true;

	if (actor.position().level() == 1 ||
		actor.position().direction() >=
			NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	const std::uint8_t direction =
		actor.position().direction();
	const std::int32_t gridNo =
		actor.position().gridNo();
	std::int32_t heightGridNo = gridNo;
	if (direction == NORTH ||
		direction == SOUTHWEST ||
		direction == WEST ||
		direction == NORTHWEST)
	{
		heightGridNo = NewGridNo(
			heightGridNo,
			DirectionInc(direction));
	}
	if (TileIsOutOfBounds(heightGridNo))
		return false;

	const std::int8_t adjacentHeight =
		GetTallestStructureHeight(heightGridNo, FALSE);
	const std::int32_t adjacentGridNo =
		NewGridNo(gridNo, DirectionInc(direction));
	if (TileIsOutOfBounds(adjacentGridNo))
		return false;

	const bool alternativeHold =
		(animation.uiFlags & ANIM_ALT_WEAPON_HOLDING) != 0;
	bool applyBipod = false;

	if ((animation.ubEndHeight == ANIM_CROUCH &&
			(adjacentHeight == 1 || adjacentHeight == 2)) ||
		(adjacentHeight == 2 &&
		 animation.ubEndHeight == ANIM_STAND &&
		 alternativeHold))
	{
		if (!FindStructure(adjacentGridNo, STRUCTURE_TREE) &&
			!IsLocationSittable(adjacentGridNo, 0))
		{
			const SoldierID occupantId =
				WhoIsThere2(
					adjacentGridNo,
					actor.position().level());
			if (occupantId == NOBODY)
			{
				applyBipod = true;
			}
			else
			{
				TacticalActor* const support =
					GetJa2SoldierRepository().resolve(
						occupantId.i);
				if (!support)
					return false;

				if (support->status().flags() &
					SOLDIER_VEHICLE)
				{
					applyBipod = true;
				}
				else if (
					actor.roster().side() ==
						support->roster().side() &&
					support->animationPlayback().state() <
						NUMANIMATIONSTATES &&
					support->position().direction() <
						NUM_WORLD_DIRECTIONS &&
					gAnimControl[
						support->animationPlayback().state()]
						.ubEndHeight == ANIM_PRONE &&
					(direction ==
						gTwoCCDirection[
							support->position().direction()] ||
					 direction ==
						gTwoCDirection[
							support->position().direction()]))
				{
					applyBipod = true;
					addMountingOpinion(*support, actor);
				}
			}
		}
	}
	else if (
		adjacentHeight == 4 &&
		(animation.ubEndHeight == ANIM_CROUCH ||
		 (animation.ubEndHeight == ANIM_STAND &&
		  alternativeHold)))
	{
		STRUCTURE* const structure =
			FindStructure(
				heightGridNo,
				STRUCTURE_WALLNWINDOW);
		if (structure)
		{
			const bool openWindow =
				(structure->fFlags & STRUCTURE_WALLNWINDOW) &&
				(structure->fFlags & STRUCTURE_OPEN);
			const bool topLeft =
				structure->ubWallOrientation ==
					OUTSIDE_TOP_LEFT ||
				structure->ubWallOrientation ==
					INSIDE_TOP_LEFT;
			const bool topRight =
				structure->ubWallOrientation ==
					OUTSIDE_TOP_RIGHT ||
				structure->ubWallOrientation ==
					INSIDE_TOP_RIGHT;
			const bool diagonal =
				direction == SOUTHWEST ||
				direction == NORTHWEST ||
				direction == SOUTHEAST ||
				direction == NORTHEAST;
			applyBipod = openWindow &&
				(((direction == SOUTH ||
				   direction == NORTH) &&
				  topLeft) ||
				 ((direction == EAST ||
				   direction == WEST) &&
				  topRight) ||
				 (diagonal && (topLeft || topRight)));
		}
	}
	else if (
		animation.ubEndHeight == ANIM_STAND &&
		adjacentHeight == 3 &&
		!alternativeHold &&
		!FindStructure(adjacentGridNo, STRUCTURE_TREE) &&
		!IsLocationSittable(adjacentGridNo, 0))
	{
		const SoldierID occupantId =
			WhoIsThere2(
				adjacentGridNo,
				actor.position().level());
		if (occupantId == NOBODY)
		{
			applyBipod = true;
		}
		else
		{
			TacticalActor* const support =
				GetJa2SoldierRepository().resolve(
					occupantId.i);
			if (!support)
				return false;

			if (support->status().flags() & SOLDIER_VEHICLE)
			{
				applyBipod = true;
			}
			else if (
				actor.roster().side() ==
					support->roster().side() &&
				support->animationPlayback().state() <
					NUMANIMATIONSTATES &&
				gAnimControl[
					support->animationPlayback().state()]
					.ubEndHeight == ANIM_CROUCH)
			{
				applyBipod = true;
				addMountingOpinion(*support, actor);
			}
		}
	}

	return applyBipod;
}
