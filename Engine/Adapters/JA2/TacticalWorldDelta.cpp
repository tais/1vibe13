#include <Engine/Adapters/JA2/TacticalWorldDelta.h>

#include <utility>

namespace
{
bool SameSector(const TacticalSectorSnapshot& left, const TacticalSectorSnapshot& right)
{
	return left.x == right.x && left.y == right.y && left.z == right.z &&
		left.loaded == right.loaded;
}

bool SameTurn(const TacticalTurnSnapshot& left, const TacticalTurnSnapshot& right)
{
	return left.turnBased == right.turnBased && left.inCombat == right.inCombat &&
		left.activeTeam == right.activeTeam && left.serial == right.serial;
}

bool Present(const TacticalActorSnapshot& actor)
{
	return actor.active && actor.inSector;
}

bool SamePosition(const TacticalActorSnapshot& left, const TacticalActorSnapshot& right)
{
	return left.grid == right.grid && left.level == right.level &&
		left.direction == right.direction;
}

bool SameStance(const TacticalActorSnapshot& left, const TacticalActorSnapshot& right)
{
	return left.stance == right.stance && left.animation == right.animation;
}

bool SameVitals(const TacticalActorSnapshot& left, const TacticalActorSnapshot& right)
{
	return left.actionPoints == right.actionPoints && left.life == right.life &&
		left.maximumLife == right.maximumLife && left.breath == right.breath &&
		left.maximumBreath == right.maximumBreath;
}

bool AppendEvent(TacticalWorldDelta& delta, std::size_t maximumEvents,
	TacticalWorldEvent event)
{
	if (delta.events.size() >= maximumEvents) return false;
	delta.events.push_back(std::move(event));
	return true;
}
}

TacticalWorldDiffResult DiffTacticalWorldSnapshots(
	const TacticalWorldSnapshot& previous,
	const TacticalWorldSnapshot& current,
	std::size_t maximumEvents,
	TacticalWorldDelta& output) noexcept
{
	if (previous.epoch() == 0 || current.epoch() == 0)
		return TacticalWorldDiffResult::InvalidSnapshot;

	try
	{
		TacticalWorldDelta delta;
		delta.previousEpoch = previous.epoch();
		delta.currentEpoch = current.epoch();
		delta.events.reserve(maximumEvents);

		if (previous.epoch() != current.epoch())
		{
			if (!AppendEvent(delta, maximumEvents,
				TacticalWorldResetEvent{previous.epoch(), current.epoch()}))
				return TacticalWorldDiffResult::CapacityReached;
			output = std::move(delta);
			return TacticalWorldDiffResult::Success;
		}

		if (!SameSector(previous.sector(), current.sector()) &&
			!AppendEvent(delta, maximumEvents,
				TacticalSectorChangedEvent{previous.sector(), current.sector()}))
			return TacticalWorldDiffResult::CapacityReached;
		if (!SameTurn(previous.turn(), current.turn()) &&
			!AppendEvent(delta, maximumEvents,
				TacticalTurnChangedEvent{previous.turn(), current.turn()}))
			return TacticalWorldDiffResult::CapacityReached;

		const std::vector<TacticalActorSnapshot>& oldActors = previous.actors();
		const std::vector<TacticalActorSnapshot>& newActors = current.actors();
		std::size_t oldIndex = 0;
		std::size_t newIndex = 0;
		while (oldIndex < oldActors.size() || newIndex < newActors.size())
		{
			if (newIndex == newActors.size() ||
				(oldIndex < oldActors.size() && oldActors[oldIndex].id < newActors[newIndex].id))
			{
				if (Present(oldActors[oldIndex]) &&
					!AppendEvent(delta, maximumEvents,
						TacticalActorLeftEvent{oldActors[oldIndex].id}))
					return TacticalWorldDiffResult::CapacityReached;
				++oldIndex;
				continue;
			}
			if (oldIndex == oldActors.size() || newActors[newIndex].id < oldActors[oldIndex].id)
			{
				if (Present(newActors[newIndex]) &&
					!AppendEvent(delta, maximumEvents,
						TacticalActorEnteredEvent{newActors[newIndex]}))
					return TacticalWorldDiffResult::CapacityReached;
				++newIndex;
				continue;
			}

			const TacticalActorSnapshot& oldActor = oldActors[oldIndex++];
			const TacticalActorSnapshot& newActor = newActors[newIndex++];
			const bool wasPresent = Present(oldActor);
			const bool isPresent = Present(newActor);
			if (wasPresent != isPresent)
			{
				if (!(isPresent
					? AppendEvent(delta, maximumEvents, TacticalActorEnteredEvent{newActor})
					: AppendEvent(delta, maximumEvents, TacticalActorLeftEvent{oldActor.id})))
					return TacticalWorldDiffResult::CapacityReached;
				continue;
			}
			if (!isPresent) continue;

			if (!SamePosition(oldActor, newActor) &&
				!AppendEvent(delta, maximumEvents, TacticalActorMovedEvent{
					oldActor.id, oldActor.grid, newActor.grid,
					oldActor.level, newActor.level,
					oldActor.direction, newActor.direction}))
				return TacticalWorldDiffResult::CapacityReached;
			if (!SameStance(oldActor, newActor) &&
				!AppendEvent(delta, maximumEvents, TacticalActorStanceChangedEvent{
					oldActor.id, oldActor.stance, newActor.stance,
					oldActor.animation, newActor.animation}))
				return TacticalWorldDiffResult::CapacityReached;
			if (!SameVitals(oldActor, newActor) &&
				!AppendEvent(delta, maximumEvents, TacticalActorVitalsChangedEvent{
					oldActor.id,
					oldActor.actionPoints, newActor.actionPoints,
					oldActor.life, newActor.life,
					oldActor.maximumLife, newActor.maximumLife,
					oldActor.breath, newActor.breath,
					oldActor.maximumBreath, newActor.maximumBreath}))
				return TacticalWorldDiffResult::CapacityReached;
		}

		output = std::move(delta);
		return TacticalWorldDiffResult::Success;
	}
	catch (...)
	{
		return TacticalWorldDiffResult::AllocationFailure;
	}
}
