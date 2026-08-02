#include "TacticalActorProfileClassification.h"

#include "GameSettings.h"
#include "Overhead Types.h"
#include "TacticalActor.h"

std::int8_t TacticalActorProfileClassification::profileTableIndex(
	const TacticalActor& actor,
	std::uint8_t team) noexcept
{
	if (team == ENEMY_TEAM &&
		gGameExternalOptions.fSoldierProfiles_Enemy)
	{
		switch (actor.roster().soldierClass())
		{
		case SOLDIER_CLASS_ADMINISTRATOR:
			return 0;
		case SOLDIER_CLASS_ARMY:
			return 1;
		case SOLDIER_CLASS_ELITE:
			return 2;
		default:
			return -1;
		}
	}

	if (team == MILITIA_TEAM &&
		gGameExternalOptions.fSoldierProfiles_Militia)
	{
		switch (actor.roster().soldierClass())
		{
		case SOLDIER_CLASS_GREEN_MILITIA:
			return 3;
		case SOLDIER_CLASS_REG_MILITIA:
			return 4;
		case SOLDIER_CLASS_ELITE_MILITIA:
			return 5;
		default:
			return -1;
		}
	}

	return -1;
}
