#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorCombatReactions.h"

#include "TacticalActorOrientation.h"
#include "Animation Control.h"
#include "Debug Control.h"
#include "Isometric Utils.h"
#include "PATHAI.H"
#include "TacticalActor.h"
#include "TacticalWorldAdapter.h"
#include "random.h"
#include "worlddef.h"

#include <cstddef>
#include <cstdint>

namespace
{
bool hasLiveReactionContext(
	const TacticalActor& actor) noexcept
{
	return IsJa2TacticalWorldLoaded() &&
		actor.roster().active() &&
		actor.roster().inSector() &&
		actor.identity().id().i < TOTAL_SOLDIERS &&
		actor.identity().bodyType() < TOTALBODYTYPES &&
		!TileIsOutOfBounds(actor.position().gridNo()) &&
		actor.position().level() >= FIRST_LEVEL &&
		actor.position().level() <= SECOND_LEVEL &&
		actor.position().direction() <
			NUM_WORLD_DIRECTIONS &&
		actor.animationPlayback().state() <
			NUMANIMATIONSTATES;
}

bool resolveReactionGrid(
	std::int32_t origin,
	std::uint8_t direction,
	std::int32_t& destination)
{
	if (TileIsOutOfBounds(origin) ||
		direction >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	destination = NewGridNo(
		origin,
		static_cast<std::int16_t>(
			DirectionInc(direction)));
	return !TileIsOutOfBounds(destination) &&
		destination != origin;
}

bool movementIsBlocked(
	const TacticalActor& actor,
	std::int32_t destination,
	std::uint8_t direction) noexcept
{
	return TileIsOutOfBounds(destination) ||
		direction >= NUM_WORLD_DIRECTIONS ||
		actor.position().level() < FIRST_LEVEL ||
		actor.position().level() > SECOND_LEVEL ||
		gubWorldMovementCosts[destination][direction]
			[actor.position().level()] >=
			TRAVELCOST_BLOCKED;
}

void markBeginningToFall(TacticalActor& actor)
{
	actor.animationActivity().beginFall(
		actor.position().direction());
	actor.animationActivity().fallClockwise() =
		Random(50) < 25 ? TRUE : FALSE;
}

bool beginForwardFall(TacticalActor& actor)
{
	markBeginningToFall(actor);
	return TacticalActorAnimationTransitions::changeState(actor,
		FALLFORWARD_FROMHIT_STAND,
		0,
		FALSE) != FALSE;
}

bool prepareReactionPath(
	TacticalActor& actor,
	std::int32_t destination,
	std::size_t stepCount)
{
	if (stepCount == 0 ||
		stepCount > MAX_PATH_LIST_SIZE ||
		TileIsOutOfBounds(destination))
	{
		return false;
	}

	const std::uint8_t pathDirection =
		gOppositeDirection[
			actor.position().direction()];
	if (pathDirection >= NUM_WORLD_DIRECTIONS)
		return false;

	actor.pendingAction().clearAction();
	actor.runtime().pendingAction.pathSearchSourceGrid =
		actor.position().gridNo();
	actor.movement().clearPastDestination();
	actor.pathing().pathSize() = 0;
	actor.pathing().pathIndex() = 0;
	for (std::size_t step = 0;
		 step < stepCount;
		 ++step)
	{
		actor.pathing().path()[
			actor.pathing().pathSize()] =
				pathDirection;
		++actor.pathing().pathSize();
	}
	actor.pathing().finalDestinationGrid() =
		destination;
	return true;
}
}

bool TacticalActorCombatReactions::setCowering(
	TacticalActor& actor,
	bool cowering)
{
	if (!hasLiveReactionContext(actor))
		return false;

	if (actor.identity().bodyType() == ROBOTNOWEAPON)
	{
		DebugMsg(
			TOPIC_JA2,
			DBG_LEVEL_3,
			String("ERROR: Robot was told to cower!"));
		return false;
	}

	if (cowering)
	{
		if (actor.status().flags() & SOLDIER_COWERING)
			return true;

		TacticalActorAnimationTransitions::initializeAnimation(actor,
			START_COWER,
			0,
			FALSE);
		actor.status().flags() |= SOLDIER_COWERING;
		actor.animationIntent().desiredHeight() =
			ANIM_CROUCH;
		return true;
	}

	if (!(actor.status().flags() & SOLDIER_COWERING) &&
		gAnimControl[actor.animationPlayback().state()]
			.ubEndHeight == ANIM_STAND)
	{
		return true;
	}

	TacticalActorAnimationTransitions::initializeAnimation(actor,
		END_COWER,
		0,
		FALSE);
	actor.status().flags() &= ~SOLDIER_COWERING;
	actor.animationIntent().desiredHeight() = ANIM_STAND;
	return true;
}

bool TacticalActorCombatReactions::beginFall(
	TacticalActor& actor)
{
	if (!hasLiveReactionContext(actor))
		return false;

	markBeginningToFall(actor);
	return true;
}

bool TacticalActorCombatReactions::beginFlyback(
	TacticalActor& actor,
	std::uint8_t impactDirection)
{
	if (!hasLiveReactionContext(actor) ||
		gubWorldMovementCosts == nullptr ||
		impactDirection >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	const std::uint8_t oppositeDirection =
		gOppositeDirection[impactDirection];
	if (oppositeDirection >= NUM_WORLD_DIRECTIONS)
		return false;

	std::int32_t firstGrid = NOWHERE;
	if (!resolveReactionGrid(
			actor.position().gridNo(),
			oppositeDirection,
			firstGrid) ||
		movementIsBlocked(
			actor,
			firstGrid,
			oppositeDirection))
	{
		return beginForwardFall(actor);
	}

	std::int32_t secondGrid = NOWHERE;
	if (!resolveReactionGrid(
			firstGrid,
			oppositeDirection,
			secondGrid) ||
		movementIsBlocked(
			actor,
			secondGrid,
			oppositeDirection))
	{
		markBeginningToFall(actor);
		return TacticalActorAnimationTransitions::changeState(actor,
			FALLBACK_HIT_STAND,
			0,
			FALSE) != FALSE;
	}

	if (!prepareReactionPath(actor, secondGrid, 2))
		return false;

	(void)TacticalActorOrientation::setMovementDestination(actor,
		actor.pathing().path()[
			actor.pathing().pathIndex()],
		FALSE,
		FLYBACK_HIT);
	return TacticalActorAnimationTransitions::initializeAnimation(actor,
		FLYBACK_HIT,
		0,
		FALSE) != FALSE;
}

bool TacticalActorCombatReactions::beginFallback(
	TacticalActor& actor,
	std::uint8_t impactDirection)
{
	if (!hasLiveReactionContext(actor) ||
		gubWorldMovementCosts == nullptr ||
		impactDirection >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	const std::uint8_t oppositeDirection =
		gOppositeDirection[impactDirection];
	if (oppositeDirection >= NUM_WORLD_DIRECTIONS)
		return false;

	std::int32_t destination = NOWHERE;
	if (!resolveReactionGrid(
			actor.position().gridNo(),
			oppositeDirection,
			destination) ||
		movementIsBlocked(
			actor,
			destination,
			oppositeDirection))
	{
		return beginForwardFall(actor);
	}

	if (!prepareReactionPath(actor, destination, 1))
		return false;

	(void)TacticalActorOrientation::setMovementDestination(actor,
		actor.pathing().path()[
			actor.pathing().pathIndex()],
		FALSE,
		FALLBACK_HIT_STAND);
	return TacticalActorAnimationTransitions::initializeAnimation(actor,
		FALLBACK_HIT_STAND,
		0,
		FALSE) != FALSE;
}
