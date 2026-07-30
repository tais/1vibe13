#include "TacticalActorRobotics.h"

#include "Assignments.h"
#include "Campaign Types.h"
#include "Items.h"
#include "Overhead.h"
#include "Soldier Profile.h"
#include "SoldierRepository.h"

#include <algorithm>
#include <cstddef>

namespace
{
bool hasCanonicalSlot(const TacticalActor& actor) noexcept
{
	const SoldierID id = actor.identity().id();
	auto& repository = GetJa2SoldierRepository();
	return id != NOBODY &&
		id.i < repository.capacity() &&
		repository.resolve(id.i) == &actor;
}

bool hasValidSector(const TacticalActor& actor) noexcept
{
	return actor.deployment().sectorX() >=
			MINIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorX() <=
			MAXIMUM_VALID_X_COORDINATE &&
		actor.deployment().sectorY() >=
			MINIMUM_VALID_Y_COORDINATE &&
		actor.deployment().sectorY() <=
			MAXIMUM_VALID_Y_COORDINATE &&
		actor.deployment().sectorZ() >=
			MINIMUM_VALID_Z_COORDINATE &&
		actor.deployment().sectorZ() <=
			MAXIMUM_VALID_Z_COORDINATE;
}

bool isPlayerRobot(const TacticalActor& actor) noexcept
{
	return gbPlayerNum < MAXTEAMS &&
		actor.roster().active() &&
		actor.roster().team() == gbPlayerNum &&
		(actor.status().flags() & SOLDIER_ROBOT) != 0;
}

bool hasRobotRemote(const TacticalActor& actor) noexcept
{
	const auto& inventory = actor.inventory();
	if (BODYPOSFINAL <= BODYPOSSTART)
		return false;

	const std::size_t first =
		static_cast<std::size_t>(BODYPOSSTART);
	const std::size_t last = std::min(
		inventory.size(),
		static_cast<std::size_t>(BODYPOSFINAL));
	if (first >= last)
		return false;

	for (std::size_t slot = first; slot < last; ++slot)
	{
		const OBJECTTYPE& object = inventory[slot];
		if (object.exists() &&
			object.usItem < MAXITEMS &&
			ItemIsRobotRemote(object.usItem))
		{
			return true;
		}
	}

	return false;
}

bool isEligibleController(const TacticalActor& actor) noexcept
{
	if (!hasCanonicalSlot(actor) ||
		!actor.roster().active() ||
		actor.roster().team() != gbPlayerNum ||
		(actor.status().flags() & SOLDIER_ROBOT) != 0 ||
		actor.vitals().health() < OKLIFE)
	{
		return false;
	}

	const auto profile = actor.identity().profile();
	if (profile != NO_PROFILE)
	{
		if (profile >= NUM_PROFILES ||
			(gMercProfiles[profile].ubMiscFlags &
			 PROFILE_MISC_FLAG_EPCACTIVE) != 0)
		{
			return false;
		}
	}

	if (actor.assignment().current() >= ON_DUTY &&
		actor.assignment().current() != VEHICLE)
	{
		return false;
	}

	return hasRobotRemote(actor);
}

bool canControl(
	const TacticalActor& controller,
	const TacticalActor& robot) noexcept
{
	if (&controller == &robot ||
		!isEligibleController(controller) ||
		!hasCanonicalSlot(robot) ||
		!isPlayerRobot(robot) ||
		!hasValidSector(controller) ||
		!hasValidSector(robot))
	{
		return false;
	}

	if (controller.deployment().sectorX() !=
			robot.deployment().sectorX() ||
		controller.deployment().sectorY() !=
			robot.deployment().sectorY() ||
		controller.deployment().sectorZ() !=
			robot.deployment().sectorZ() ||
		controller.deployment().isBetweenSectors() !=
			robot.deployment().isBetweenSectors())
	{
		return false;
	}

	if (!robot.deployment().isBetweenSectors())
		return true;

	if (controller.assignment().current() !=
		robot.assignment().current())
	{
		return false;
	}

	return robot.assignment().current() != VEHICLE ||
		controller.deployment().vehicleId() ==
			robot.deployment().vehicleId();
}

bool playerTeamRange(
	std::size_t& first,
	std::size_t& last) noexcept
{
	if (gbPlayerNum >= MAXTEAMS)
		return false;

	auto& repository = GetJa2SoldierRepository();
	if (repository.capacity() == 0)
		return false;

	const auto& team = gTacticalStatus.Team[gbPlayerNum];
	first = team.bFirstID.i;
	if (team.bFirstID > team.bLastID ||
		first >= repository.capacity())
	{
		return false;
	}

	last = std::min<std::size_t>(
		team.bLastID.i,
		repository.capacity() - 1);
	return first <= last;
}
}

TacticalActor* TacticalActorRobotics::controller(
	TacticalActor& robot) noexcept
{
	if (!hasCanonicalSlot(robot) || !isPlayerRobot(robot))
		return nullptr;

	const SoldierID controllerId =
		robot.vehicleState().robotRemoteHolder();
	if (controllerId == NOBODY)
		return nullptr;

	auto& repository = GetJa2SoldierRepository();
	TacticalActor* resolved =
		repository.resolve(controllerId.i);
	if (resolved == nullptr ||
		resolved->identity().id() != controllerId ||
		!canControl(*resolved, robot))
	{
		return nullptr;
	}

	return resolved;
}

bool TacticalActorRobotics::canBeControlled(
	TacticalActor& robot) noexcept
{
	return controller(robot) != nullptr;
}

bool TacticalActorRobotics::isControlling(
	TacticalActor& candidate) noexcept
{
	if (!isEligibleController(candidate))
		return false;

	std::size_t first = 0;
	std::size_t last = 0;
	if (!playerTeamRange(first, last))
		return false;

	auto& repository = GetJa2SoldierRepository();
	for (std::size_t slot = first; slot <= last; ++slot)
	{
		TacticalActor* robot = repository.resolve(slot);
		if (robot != nullptr && canControl(candidate, *robot))
			return true;
	}

	return false;
}

void TacticalActorRobotics::refreshControllerForRobot(
	TacticalActor& robot) noexcept
{
	robot.vehicleState().clearRobotRemoteHolder();
	if (!hasCanonicalSlot(robot) || !isPlayerRobot(robot))
		return;

	std::size_t first = 0;
	std::size_t last = 0;
	if (!playerTeamRange(first, last))
		return;

	auto& repository = GetJa2SoldierRepository();
	for (std::size_t slot = first; slot <= last; ++slot)
	{
		TacticalActor* candidate = repository.resolve(slot);
		if (candidate != nullptr &&
			candidate->identity().id().i == slot &&
			canControl(*candidate, robot))
		{
			robot.vehicleState().robotRemoteHolder() =
				candidate->identity().id();
			return;
		}
	}
}

void TacticalActorRobotics::refreshRobotsForController(
	TacticalActor& candidate) noexcept
{
	if (!hasCanonicalSlot(candidate))
		return;

	std::size_t first = 0;
	std::size_t last = 0;
	if (!playerTeamRange(first, last))
		return;

	const SoldierID candidateId =
		candidate.identity().id();
	auto& repository = GetJa2SoldierRepository();
	for (std::size_t slot = first; slot <= last; ++slot)
	{
		TacticalActor* robot = repository.resolve(slot);
		if (robot == nullptr ||
			robot->identity().id().i != slot ||
			!isPlayerRobot(*robot))
		{
			continue;
		}

		if (canControl(candidate, *robot))
		{
			robot->vehicleState().robotRemoteHolder() =
				candidateId;
		}
		else if (robot->vehicleState().robotRemoteHolder() ==
			candidateId)
		{
			refreshControllerForRobot(*robot);
		}
	}
}
