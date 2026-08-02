#include "TacticalActorPredicates.h"

#include "Animation Control.h"
#include "Overhead Types.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"

namespace
{
	bool hasAnimationHeight(const TacticalActor& actor, UINT8 height) noexcept
	{
		const UINT16 animationState = actor.animationPlayback().state();
		return animationState < NUMANIMATIONSTATES &&
			gAnimControl[animationState].ubHeight == height;
	}
}

namespace TacticalActorPredicates
{
	bool isCivilian(const TacticalActor& actor) noexcept
	{
		return actor.roster().team() == CIV_TEAM;
	}

	bool isCivilianOrMilitia(const TacticalActor& actor) noexcept
	{
		return isCivilian(actor) || actor.roster().team() == MILITIA_TEAM;
	}

	bool isCrouched(const TacticalActor& actor) noexcept
	{
		return hasAnimationHeight(actor, ANIM_CROUCH);
	}

	bool isStanding(const TacticalActor& actor) noexcept
	{
		return hasAnimationHeight(actor, ANIM_STAND);
	}

	bool isProne(const TacticalActor& actor) noexcept
	{
		return hasAnimationHeight(actor, ANIM_PRONE);
	}

	bool consideredNeutralForAttack(
		const TacticalActor& observer,
		const TacticalActor& target) noexcept
	{
		const bool targetIsNeutral = target.aiBehavior().neutral() ||
			(target.featureFlags().primaryFlags() &
				(SOLDIER_COVERT_CIV | SOLDIER_COVERT_SOLDIER | SOLDIER_POW));
		const bool creatureCanAttackTarget =
			observer.roster().team() != CREATURE_TEAM ||
			(target.status().flags() & SOLDIER_VEHICLE) ||
			target.identity().bodyType() == CROW;
		const bool boxersAreOpponents =
			(observer.status().flags() & SOLDIER_BOXER) &&
			(target.status().flags() & SOLDIER_BOXER);

		return targetIsNeutral && creatureCanAttackTarget && !boxersAreOpponents;
	}
}
