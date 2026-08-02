#include "TacticalActorVisibility.h"

#include "Grid Direction.h"
#include "Soldier Profile Constants.h"
#include "TacticalActorSkills.h"
#include "TacticalActorStateFlags.h"

#include "GameInitOptionsScreen.h"
#include "GameSettings.h"
#include "IMP Skill Trait.h"
#include "Animation Control.h"
#include "Explosion Control.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "LOS.h"
#include "Overhead.h"
#include "Overhead Types.h"
#include "Smell.h"
#include "TacticalActor.h"
#include "Soldier Functions.h"
#include "Soldier macros.h"
#include "TacticalActorConditions.h"
#include "TacticalWorldAdapter.h"
#include "Vehicles.h"
#include "environment.h"
#include "lighting.h"
#include "strategicmap.h"
#include "worlddef.h"
#include "worldman.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

extern BOOLEAN gfLightningInProgress;
extern UINT8 ubRealAmbientLightLevel;

namespace
{
std::int8_t lookDistance[NUM_WORLD_DIRECTIONS][NUM_WORLD_DIRECTIONS]{};
std::int8_t behindDistance = 0;
std::int8_t slightBehindDistance = 0;
std::int8_t sideDistance = 0;
std::int8_t angleDistance = 0;
std::int8_t straightDistance = 0;

bool validDirection(std::int8_t direction) noexcept
{
	return direction == DIRECTION_IRRELEVANT ||
		(direction >= NORTH && direction < NUM_WORLD_DIRECTIONS);
}

bool validLevel(std::int8_t level) noexcept
{
	return level >= FIRST_LEVEL && level <= SECOND_LEVEL;
}

bool validActorTableReferences(const TacticalActor& actor) noexcept
{
	const std::uint8_t profile = actor.identity().profile();
	return actor.identity().bodyType() < TOTALBODYTYPES &&
		(profile == NO_PROFILE || profile < NUM_PROFILES) &&
		actor.roster().team() >= 0 &&
		actor.roster().team() < MAXTEAMS;
}

bool validWorldContext(
	const TacticalActor& actor,
	std::int32_t subjectGrid,
	std::int8_t subjectLevel) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		gpWorldLevelData != nullptr &&
		validActorTableReferences(actor) &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		!TileIsOutOfBounds(subjectGrid) &&
		validLevel(actor.position().level()) &&
		validLevel(subjectLevel) &&
		actor.position().direction() < NUM_WORLD_DIRECTIONS &&
		actor.pathing().desiredDirection() < NUM_WORLD_DIRECTIONS;
}

std::int16_t smellableDistance(
	const TacticalActor* subject) noexcept
{
	std::int16_t visibleDistance = straightDistance;
	visibleDistance *= 2;

	if (subject != nullptr &&
		!(subject->status().flags() & SOLDIER_MONSTER))
	{
		visibleDistance = visibleDistance *
			(subject->perception().normalSmell() -
			 subject->perception().monsterSmell()) /
			NORMAL_HUMAN_SMELL_STRENGTH;
		visibleDistance = std::max<std::int16_t>(0, visibleDistance);
	}

	return visibleDistance;
}

bool validVehicleSeat(const TacticalActor& actor, std::int8_t seat) noexcept
{
	if (seat < 0)
		return true;

	const std::int32_t vehicleId = actor.deployment().vehicleId();
	if (pVehicleList == nullptr || vehicleId < 0 ||
		vehicleId >= ubNumberOfVehicles || seat >= MAXPASSENGERS)
		return false;

	return pVehicleList[vehicleId].ubVehicleType < NUM_PROFILES;
}
}

void TacticalActorVisibility::initializeRanges()
{
	behindDistance = static_cast<std::int8_t>(
		BEHIND_RATIO * gGameExternalOptions.ubStraightSightRange);
	slightBehindDistance = static_cast<std::int8_t>(
		SBEHIND_RATIO * gGameExternalOptions.ubStraightSightRange);
	sideDistance = static_cast<std::int8_t>(
		SIDE_RATIO * gGameExternalOptions.ubStraightSightRange);
	angleDistance = static_cast<std::int8_t>(
		ANGLE_RATIO * gGameExternalOptions.ubStraightSightRange);
	straightDistance = static_cast<std::int8_t>(
		STRAIGHT_RATIO * gGameExternalOptions.ubStraightSightRange);

	std::int8_t expanded[15][15]{};
	for (int direction = 0; direction < NUM_WORLD_DIRECTIONS; ++direction)
	{
		expanded[direction][direction] = straightDistance;
		expanded[direction][direction + 1] = angleDistance;
		expanded[direction + 1][direction] = angleDistance;
		expanded[direction][direction + 2] = sideDistance;
		expanded[direction + 2][direction] = sideDistance;
		expanded[direction][direction + 3] = slightBehindDistance;
		expanded[direction + 3][direction] = slightBehindDistance;
		expanded[direction][direction + 4] = behindDistance;
		expanded[direction + 4][direction] = behindDistance;
		expanded[direction][direction + 5] = slightBehindDistance;
		expanded[direction + 5][direction] = slightBehindDistance;
		expanded[direction][direction + 6] = sideDistance;
		expanded[direction + 6][direction] = sideDistance;
		expanded[direction][direction + 7] = angleDistance;
		expanded[direction + 7][direction] = angleDistance;
	}

	for (int facing = 0; facing < NUM_WORLD_DIRECTIONS; ++facing)
	{
		for (int subject = 0; subject < NUM_WORLD_DIRECTIONS; ++subject)
			lookDistance[facing][subject] = expanded[facing][subject];
	}
}

std::int8_t TacticalActorVisibility::straightRange() noexcept
{
	return straightDistance;
}

std::int16_t TacticalActorVisibility::normalMaximumDistance() noexcept
{
	return static_cast<std::int16_t>(straightDistance * 2);
}

bool TacticalActorVisibility::hasLimitedVision(TacticalActor& actor)
{
	return validActorTableReferences(actor) &&
		(gGameExternalOptions.gfAllowLimitedVision ||
		 GetPercentTunnelVision(&actor) > 0);
}

std::int16_t TacticalActorVisibility::adjustForEnvironment(
	TacticalActor& actor,
	std::int8_t lightLevel,
	std::int16_t distance)
{
	if (!validActorTableReferences(actor))
		return 0;

	constexpr std::size_t brightnessLevelCount =
		sizeof(gGameExternalOptions.ubBrightnessVisionMod) /
		sizeof(gGameExternalOptions.ubBrightnessVisionMod[0]);
	if (lightLevel < 0 ||
		static_cast<std::size_t>(lightLevel) >= brightnessLevelCount)
		return 0;

	std::int16_t adjusted = static_cast<std::int16_t>(
		distance * gGameExternalOptions.ubBrightnessVisionMod[lightLevel] /
		100);

	const bool validSurfaceSector =
		actor.deployment().sectorX() > 0 &&
		actor.deployment().sectorX() < MAP_WORLD_X - 1 &&
		actor.deployment().sectorY() > 0 &&
		actor.deployment().sectorY() < MAP_WORLD_Y - 1;
	if (actor.deployment().sectorZ() == 0 && validSurfaceSector)
	{
		const auto weather = SectorInfo[
			SECTOR(actor.deployment().sectorX(),
				actor.deployment().sectorY())].usWeather;
		if (weather >= WEATHER_FORECAST_MAX)
			return 0;
		const float weatherPenalty =
			gGameExternalOptions.dVisDistDecrease[weather];

		float appliedPenalty = 1.0f;
		if (gGameOptions.fNewTraitSystem &&
			HAS_SKILL_TRAIT(&actor, SURVIVAL_NT))
		{
			appliedPenalty = std::clamp(
				appliedPenalty -
					gSkillTraitValues.dSVWeatherPenaltiesReduction *
					NUM_SKILL_TRAITS(&actor, SURVIVAL_NT),
				0.0f,
				1.0f);
		}

		adjusted -= static_cast<std::int16_t>(
			adjusted * weatherPenalty * appliedPenalty);
		if (gfLightningInProgress)
			adjusted += adjusted * ubRealAmbientLightLevel / 10;
	}

	return adjusted;
}

std::int16_t TacticalActorVisibility::maximumDistance(
	TacticalActor& actor,
	std::int32_t subjectGrid,
	std::int8_t subjectLevel,
	int calculationMode,
	TacticalActor* knownSubject)
{
	if (subjectGrid == NOWHERE)
		return normalMaximumDistance();

	if (subjectLevel == -1)
		subjectLevel = actor.position().level();
	if (!validWorldContext(actor, subjectGrid, subjectLevel))
		return 0;

	const std::uint8_t tunnelVision = GetPercentTunnelVision(&actor);
	if (calculationMode == CALC_FROM_ALL_DIRS)
	{
		return distance(
			actor,
			DIRECTION_IRRELEVANT,
			DIRECTION_IRRELEVANT,
			subjectGrid,
			subjectLevel,
			TacticalActorConditions::isCowering(actor),
			tunnelVision,
			knownSubject);
	}

	return distance(
		actor,
		hasLimitedVision(actor)
			? actor.pathing().desiredDirection()
			: DIRECTION_IRRELEVANT,
		DIRECTION_IRRELEVANT,
		subjectGrid,
		subjectLevel,
		TacticalActorConditions::isCowering(actor),
		tunnelVision,
		knownSubject);
}

std::int16_t TacticalActorVisibility::distance(
	TacticalActor& actor,
	std::int8_t facingDirection,
	std::int8_t subjectDirection,
	std::int32_t subjectGrid,
	std::int8_t subjectLevel,
	bool isCowering,
	std::uint8_t tunnelVision,
	TacticalActor* knownSubject)
{
	TacticalActor* const actorPointer = &actor;
	if (!validDirection(facingDirection) ||
		!validDirection(subjectDirection) ||
		!validWorldContext(actor, subjectGrid, subjectLevel))
		return 0;

	TacticalActor* subject = knownSubject != nullptr
		? knownSubject
		: SimpleFindSoldier(subjectGrid, subjectLevel);
	if (subject != nullptr && !validActorTableReferences(*subject))
		return 0;
	std::int16_t visibleDistance = 0;
	bool sideViewLimit = false;
	std::int16_t tunnelVisionPercent = 0;

	if (actor.status().flags() & SOLDIER_MONSTER)
	{
		return subject != nullptr
			? smellableDistance(subject)
			: 0;
	}

	if (actor.perception().isBlinded() ||
		(actor.collapseState().tactical() && actor.vitals().breath() == 0))
		return 0;

	const std::int8_t seatIndex = GetSeatIndexFromSoldier(actorPointer);
	if (!validVehicleSeat(actor, seatIndex))
		return 0;
	if (seatIndex >= 0)
	{
		const std::int32_t vehicleId = actor.deployment().vehicleId();
		const std::uint8_t vehicleType =
			pVehicleList[vehicleId].ubVehicleType;
		if (gNewVehicle[vehicleType].VehicleSeats[seatIndex].fBlockedView)
			return 0;
	}

	if (facingDirection == DIRECTION_IRRELEVANT &&
		ARMED_VEHICLE(actorPointer))
	{
		facingDirection = actor.pathing().desiredDirection();
		subjectDirection = static_cast<std::int8_t>(
			GetDirectionToGridNoFromGridNo(
				actor.position().gridNo(), subjectGrid));
	}

	if (!validDirection(facingDirection) ||
		!validDirection(subjectDirection))
		return 0;

	if (!ARMED_VEHICLE(actorPointer) &&
		(facingDirection == DIRECTION_IRRELEVANT ||
		 (actor.status().flags() & SOLDIER_ROBOT) ||
		 (subject != nullptr &&
		  subject->renderState().muzzleFlashVisible())))
	{
		visibleDistance = normalMaximumDistance();
	}
	else if (actor.position().gridNo() == subjectGrid)
	{
		visibleDistance = normalMaximumDistance();
	}
	else
	{
		const bool limitedVision =
			gGameExternalOptions.gfAllowLimitedVision || tunnelVision > 0;
		if (limitedVision)
		{
			subjectDirection = static_cast<std::int8_t>(
				GetDirectionToGridNoFromGridNo(
					actor.position().gridNo(), subjectGrid));
		}

		if (facingDirection == DIRECTION_IRRELEVANT)
			facingDirection = actor.position().direction();
		if (!validDirection(facingDirection) ||
			!validDirection(subjectDirection) ||
			facingDirection == DIRECTION_IRRELEVANT ||
			subjectDirection == DIRECTION_IRRELEVANT)
			return 0;

		visibleDistance = lookDistance[facingDirection][subjectDirection];
		if (visibleDistance == 0 && limitedVision)
			return 0;

		if (visibleDistance == angleDistance &&
			(actor.roster().team() == OUR_TEAM ||
			 actor.aiBehavior().alertStatus() >= STATUS_RED))
		{
			visibleDistance = straightDistance;
		}

		if (visibleDistance != straightDistance ||
			(limitedVision && facingDirection != subjectDirection))
		{
			tunnelVisionPercent = tunnelVision;
		}

		visibleDistance *= 2;
		if (actor.animationPlayback().state() == RUNNING &&
			lookDistance[facingDirection][subjectDirection] !=
				straightDistance)
		{
			visibleDistance = static_cast<std::int16_t>(
				visibleDistance * ANGLE_RATIO);
		}

		if (tunnelVisionPercent > 0)
		{
			sideViewLimit = true;
			visibleDistance = visibleDistance *
				(100 - tunnelVisionPercent) / 100;
		}
	}

	if (actor.position().level() != subjectLevel)
		visibleDistance += visibleDistance / 6;

	const std::int8_t lightLevel = static_cast<std::int8_t>(
		LightTrueLevel(subjectGrid, subjectLevel));
	if (!(subject != nullptr &&
		subject->renderState().muzzleFlashVisible() &&
		lightLevel > NORMAL_LIGHTLEVEL_DAY))
	{
		visibleDistance = adjustForEnvironment(
			actor, lightLevel, visibleDistance);
	}

	if (!sideViewLimit)
	{
		visibleDistance += visibleDistance *
			GetTotalVisionRangeBonus(&actor, lightLevel) / 100;
		if ((gGameExternalOptions.ubCoweringReducesSightRange == 1 ||
			 gGameExternalOptions.ubCoweringReducesSightRange == 2) &&
			IS_MERC_BODY_TYPE(actorPointer) &&
			(actor.roster().team() == ENEMY_TEAM ||
			 actor.roster().team() == MILITIA_TEAM ||
			 actor.roster().team() == gbPlayerNum) &&
			gGameExternalOptions.ubMaxSuppressionShock > 0 &&
			visibleDistance > 0 && isCowering)
		{
			visibleDistance = std::max<std::int16_t>(
				1,
				visibleDistance *
					(gGameExternalOptions.ubMaxSuppressionShock -
					 actor.suppression().shock()) /
					gGameExternalOptions.ubMaxSuppressionShock);
		}
	}

	if (gGameOptions.fNewTraitSystem)
	{
		if (HAS_SKILL_TRAIT(&actor, NIGHT_OPS_NT))
		{
			visibleDistance += NightBonusScale(
				gSkillTraitValues.ubNOeSightRangeBonusInDark,
				lightLevel);
		}
	}
	else if (HAS_SKILL_TRAIT(&actor, NIGHTOPS_OT))
	{
		visibleDistance += NightBonusScale(
			NUM_SKILL_TRAITS(&actor, NIGHTOPS_OT), lightLevel);
	}

	if (actor.identity().bodyType() == BLOODCAT && gbWorldSectorZ == 0)
	{
		visibleDistance += NightBonusScale(
			UVGOGGLES_BONUS, lightLevel);
	}
	else if (AM_A_ROBOT(actorPointer))
	{
		visibleDistance += NightBonusScale(
			NIGHTSIGHTGOGGLES_BONUS, lightLevel);
	}

	if ((ARMED_VEHICLE(actorPointer) && visibleDistance > 0) ||
		(subject != nullptr && ARMED_VEHICLE(subject)))
	{
		visibleDistance += 5;
	}

	const MAP_ELEMENT& actorTile = GetMapElement(
		static_cast<UINT32>(actor.position().gridNo()));
	if (actorTile.ubExtFlags[subjectLevel] &
		(MAPELEMENT_EXT_TEARGAS | MAPELEMENT_EXT_MUSTARDGAS))
	{
		const std::int8_t maskPosition = FindGasMask(&actor);
		if (maskPosition == HEAD1POS || maskPosition == HEAD2POS)
		{
			const auto status =
				actor.inventory()[maskPosition][0]->data.objectStatus;
			if (status < GASMASK_MIN_STATUS)
				visibleDistance = std::min<std::int16_t>(
					visibleDistance, 2 + status / 15);
		}
		else
		{
			visibleDistance = std::min<std::int16_t>(visibleDistance, 2);
		}
	}

	if (actorTile.ubExtFlags[subjectLevel] & MAPELEMENT_EXT_BURNABLEGAS)
		visibleDistance = std::min<std::int16_t>(visibleDistance, 2);

	return visibleDistance;
}
