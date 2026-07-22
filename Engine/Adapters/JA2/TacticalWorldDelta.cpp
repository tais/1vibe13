#include <Engine/Adapters/JA2/TacticalWorldDelta.h>

#include <type_traits>
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

template <typename Visitor>
bool VisitEvents(const TacticalWorldSnapshot& previous,
	const TacticalWorldSnapshot& current, Visitor&& visit)
{
	if (previous.epoch() != current.epoch())
		return visit(TacticalWorldResetEvent{previous.epoch(), current.epoch()});

	if (!SameSector(previous.sector(), current.sector()) &&
		!visit(TacticalSectorChangedEvent{previous.sector(), current.sector()}))
		return false;
	if (!SameTurn(previous.turn(), current.turn()) &&
		!visit(TacticalTurnChangedEvent{previous.turn(), current.turn()}))
		return false;

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
				!visit(TacticalActorLeftEvent{oldActors[oldIndex].id}))
				return false;
			++oldIndex;
			continue;
		}
		if (oldIndex == oldActors.size() || newActors[newIndex].id < oldActors[oldIndex].id)
		{
			if (Present(newActors[newIndex]) &&
				!visit(TacticalActorEnteredEvent{newActors[newIndex]}))
				return false;
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
				? visit(TacticalActorEnteredEvent{newActor})
				: visit(TacticalActorLeftEvent{oldActor.id})))
				return false;
			continue;
		}
		if (!isPresent) continue;

		if (!SamePosition(oldActor, newActor) &&
			!visit(TacticalActorMovedEvent{
				oldActor.id, oldActor.grid, newActor.grid,
				oldActor.level, newActor.level,
				oldActor.direction, newActor.direction}))
			return false;
		if (!SameStance(oldActor, newActor) &&
			!visit(TacticalActorStanceChangedEvent{
				oldActor.id, oldActor.stance, newActor.stance,
				oldActor.animation, newActor.animation}))
			return false;
		if (!SameVitals(oldActor, newActor) &&
			!visit(TacticalActorVitalsChangedEvent{
				oldActor.id,
				oldActor.actionPoints, newActor.actionPoints,
				oldActor.life, newActor.life,
				oldActor.maximumLife, newActor.maximumLife,
				oldActor.breath, newActor.breath,
				oldActor.maximumBreath, newActor.maximumBreath}))
			return false;
	}
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

	std::size_t eventCount = 0;
	const bool withinCapacity = VisitEvents(previous, current,
		[&](TacticalWorldEvent) {
			if (eventCount >= maximumEvents) return false;
			++eventCount;
			return true;
		});
	if (!withinCapacity) return TacticalWorldDiffResult::CapacityReached;

	try
	{
		output.events.reserve(maximumEvents);
	}
	catch (...)
	{
		return TacticalWorldDiffResult::AllocationFailure;
	}

	static_assert(std::is_nothrow_move_constructible<TacticalWorldEvent>::value,
		"reusable tactical diffs require non-throwing events");
	output.previousEpoch = previous.epoch();
	output.currentEpoch = current.epoch();
	output.events.clear();
	VisitEvents(previous, current,
		[&](TacticalWorldEvent event) {
			output.events.push_back(std::move(event));
			return true;
		});
	return TacticalWorldDiffResult::Success;
}
