#include "TacticalActorPrisonerOperations.h"

#include "Campaign Types.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "Soldier Control.h"
#include "SoldierRepository.h"
#include "TacticalWorldAdapter.h"
#include "Text.h"
#include "message.h"
#include "worldman.h"

#include <algorithm>
#include <cstdint>

namespace
{
bool hasValidSector(const TacticalActor& actor) noexcept
{
	return actor.deployment().sectorX() >=
			MINIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorX() <=
			MAXIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorY() >=
			MINIMUM_VALID_Y_COORDINATE &&
		actor.deployment().sectorY() <=
			MAXIMUM_VALID_Y_COORDINATE;
}
}

bool TacticalActorPrisonerOperations::canProcess(
	const TacticalActor& actor) noexcept
{
	if (actor.vitals().health() < OKLIFE ||
		!hasValidSector(actor))
	{
		return false;
	}

	const std::uint8_t sector =
		SECTOR(
			actor.deployment().sectorX(),
			actor.deployment().sectorY());
	bool prisonPresent = false;
	for (std::uint16_t facility = 0;
		 facility < NUM_FACILITY_TYPES;
		 ++facility)
	{
		if (gFacilityLocations[sector][facility].fFacilityHere &&
			gFacilityTypes[facility]
					.AssignmentData[
						FAC_INTERROGATE_PRISONERS]
					.usPrisonBaseLimit > 0)
		{
			prisonPresent = true;
			break;
		}
	}

	if (!prisonPresent || actor.deployment().sectorZ() != 0)
		return false;

	std::int16_t prisoners[PRISONER_MAX] = {};
	return GetNumberOfPrisoners(
			&SectorInfo[sector],
			prisoners) > 0;
}

bool TacticalActorPrisonerOperations::freeAdjacent(
	TacticalActor& actor)
{
	if (!IsJa2TacticalWorldLoaded() ||
		TileIsOutOfBounds(actor.position().gridNo()) ||
		actor.position().level() < FIRST_LEVEL ||
		actor.position().level() > SECOND_LEVEL ||
		actor.position().direction() >=
			NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	const std::int32_t adjacentGrid =
		NewGridNo(
			actor.position().gridNo(),
			DirectionInc(actor.position().direction()));
	if (TileIsOutOfBounds(adjacentGrid))
		return false;

	const SoldierID targetId =
		WhoIsThere2(
			adjacentGrid,
			actor.position().level());
	if (targetId == NOBODY)
		return false;

	TacticalActor* const target =
		GetJa2SoldierRepository().resolve(targetId.i);
	if (!target ||
		!(target->featureFlags().primaryFlags() &
			(SOLDIER_POW | SOLDIER_POW_PRISON)))
	{
		return false;
	}

	target->featureFlags().primaryFlags() &=
		~(SOLDIER_POW | SOLDIER_POW_PRISON);
	ScreenMsg(
		FONT_MCOLOR_LTYELLOW,
		MSG_INTERFACE,
		szPrisonerTextStr[STR_PRISONER_X_FREES_Y],
		actor.GetName(),
		target->GetName());

	actor.aiBehavior().alertStatus() =
		std::max<std::int8_t>(
			actor.aiBehavior().alertStatus(),
			static_cast<std::int8_t>(STATUS_RED));
	target->aiBehavior().alertStatus() =
		std::max<std::int8_t>(
			target->aiBehavior().alertStatus(),
			static_cast<std::int8_t>(STATUS_RED));
	return true;
}
