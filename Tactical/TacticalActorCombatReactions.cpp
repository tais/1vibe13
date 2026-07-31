#include "TacticalActorCombatReactions.h"

#include "Animation Control.h"
#include "Isometric Utils.h"
#include "PATHAI.H"
#include "Soldier Control.h"
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
	return actor.ChangeSoldierState(
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
		return actor.ChangeSoldierState(
			FALLBACK_HIT_STAND,
			0,
			FALSE) != FALSE;
	}

	if (!prepareReactionPath(actor, secondGrid, 2))
		return false;

	actor.EVENT_InternalSetSoldierDestination(
		actor.pathing().path()[
			actor.pathing().pathIndex()],
		FALSE,
		FLYBACK_HIT);
	return actor.EVENT_InitNewSoldierAnim(
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

	actor.EVENT_InternalSetSoldierDestination(
		actor.pathing().path()[
			actor.pathing().pathIndex()],
		FALSE,
		FALLBACK_HIT_STAND);
	return actor.EVENT_InitNewSoldierAnim(
		FALLBACK_HIT_STAND,
		0,
		FALSE) != FALSE;
}
