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

bool SameDimensions(const TacticalWorldDimensions& left,
	const TacticalWorldDimensions& right)
{
	return left.columns == right.columns && left.rows == right.rows;
}

bool SameTurn(const TacticalTurnSnapshot& left, const TacticalTurnSnapshot& right)
{
	return left.turnBased == right.turnBased && left.inCombat == right.inCombat &&
		left.activeTeam == right.activeTeam && left.serial == right.serial &&
		left.interruptPhase == right.interruptPhase &&
		left.interruptSerial == right.interruptSerial &&
		left.commandsBlocked == right.commandsBlocked;
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
		left.maximumBreath == right.maximumBreath &&
		left.hostileToPlayerTeam == right.hostileToPlayerTeam &&
		left.interruptActionEligible == right.interruptActionEligible;
}

bool SameLoadout(const TacticalActorSnapshot& left,
	const TacticalActorSnapshot& right)
{
	return left.loadout == right.loadout;
}

bool SameDoor(const TacticalDoorSnapshot& left,
	const TacticalDoorSnapshot& right)
{
	return left.baseGrid == right.baseGrid &&
		left.structureId == right.structureId && left.open == right.open;
}

template <typename Visitor>
bool VisitActorPairs(const TacticalWorldSnapshot& previous,
	const TacticalWorldSnapshot& current, Visitor&& visit)
{
	const std::vector<TacticalActorSnapshot>& oldActors = previous.actors();
	const std::vector<TacticalActorSnapshot>& newActors = current.actors();
	std::size_t oldIndex = 0;
	std::size_t newIndex = 0;
	while (oldIndex < oldActors.size() || newIndex < newActors.size())
	{
		if (newIndex == newActors.size() ||
			(oldIndex < oldActors.size() && oldActors[oldIndex].id < newActors[newIndex].id))
		{
			if (!visit(&oldActors[oldIndex], nullptr)) return false;
			++oldIndex;
			continue;
		}
		if (oldIndex == oldActors.size() || newActors[newIndex].id < oldActors[oldIndex].id)
		{
			if (!visit(nullptr, &newActors[newIndex])) return false;
			++newIndex;
			continue;
		}

		const TacticalActorSnapshot& oldActor = oldActors[oldIndex++];
		const TacticalActorSnapshot& newActor = newActors[newIndex++];
		if (!visit(&oldActor, &newActor)) return false;
	}
	return true;
}

template <typename Visitor>
bool VisitDoorPairs(const TacticalWorldSnapshot& previous,
	const TacticalWorldSnapshot& current, Visitor&& visit)
{
	const std::vector<TacticalDoorSnapshot>& oldDoors = previous.doors();
	const std::vector<TacticalDoorSnapshot>& newDoors = current.doors();
	std::size_t oldIndex = 0;
	std::size_t newIndex = 0;
	while (oldIndex < oldDoors.size() || newIndex < newDoors.size())
	{
		if (newIndex == newDoors.size() ||
			(oldIndex < oldDoors.size() &&
			 oldDoors[oldIndex].baseGrid < newDoors[newIndex].baseGrid))
		{
			if (!visit(&oldDoors[oldIndex], nullptr)) return false;
			++oldIndex;
			continue;
		}
		if (oldIndex == oldDoors.size() ||
			newDoors[newIndex].baseGrid < oldDoors[oldIndex].baseGrid)
		{
			if (!visit(nullptr, &newDoors[newIndex])) return false;
			++newIndex;
			continue;
		}
		const TacticalDoorSnapshot& oldDoor = oldDoors[oldIndex++];
		const TacticalDoorSnapshot& newDoor = newDoors[newIndex++];
		if (!visit(&oldDoor, &newDoor)) return false;
	}
	return true;
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

	// The co-op wire canonical form is category-major and then identity-major.
	// Keep each pass allocation-free; DiffTacticalWorldSnapshots still performs
	// one complete counting traversal before reserving and one publishing
	// traversal after capacity is proven.
	if (!VisitActorPairs(previous, current,
		[&](const TacticalActorSnapshot* oldActor,
			const TacticalActorSnapshot* newActor) {
			const bool wasPresent = oldActor != nullptr && Present(*oldActor);
			const bool isPresent = newActor != nullptr && Present(*newActor);
			return !isPresent || wasPresent ||
				visit(TacticalActorEnteredEvent{*newActor});
		}))
		return false;

	if (!VisitActorPairs(previous, current,
		[&](const TacticalActorSnapshot* oldActor,
			const TacticalActorSnapshot* newActor) {
			const bool wasPresent = oldActor != nullptr && Present(*oldActor);
			const bool isPresent = newActor != nullptr && Present(*newActor);
			return !wasPresent || isPresent ||
				visit(TacticalActorLeftEvent{oldActor->id});
		}))
		return false;

	if (!VisitActorPairs(previous, current,
		[&](const TacticalActorSnapshot* oldActor,
			const TacticalActorSnapshot* newActor) {
			if (oldActor == nullptr || newActor == nullptr ||
				!Present(*oldActor) || !Present(*newActor) ||
				SamePosition(*oldActor, *newActor))
				return true;
			return visit(TacticalActorMovedEvent{
				oldActor->id, oldActor->grid, newActor->grid,
				oldActor->level, newActor->level,
				oldActor->direction, newActor->direction});
		}))
		return false;

	if (!VisitActorPairs(previous, current,
		[&](const TacticalActorSnapshot* oldActor,
			const TacticalActorSnapshot* newActor) {
			if (oldActor == nullptr || newActor == nullptr ||
				!Present(*oldActor) || !Present(*newActor) ||
				SameStance(*oldActor, *newActor))
				return true;
			return visit(TacticalActorStanceChangedEvent{
				oldActor->id, oldActor->stance, newActor->stance,
				oldActor->animation, newActor->animation});
		}))
		return false;

	if (!VisitActorPairs(previous, current,
		[&](const TacticalActorSnapshot* oldActor,
			const TacticalActorSnapshot* newActor) {
			if (oldActor == nullptr || newActor == nullptr ||
				!Present(*oldActor) || !Present(*newActor) ||
				SameVitals(*oldActor, *newActor))
				return true;
			return visit(TacticalActorVitalsChangedEvent{
				oldActor->id,
				oldActor->actionPoints, newActor->actionPoints,
				oldActor->life, newActor->life,
				oldActor->maximumLife, newActor->maximumLife,
				oldActor->breath, newActor->breath,
				oldActor->maximumBreath, newActor->maximumBreath,
				oldActor->hostileToPlayerTeam,
				newActor->hostileToPlayerTeam,
				oldActor->interruptActionEligible,
				newActor->interruptActionEligible});
		}))
		return false;

	if (!VisitActorPairs(previous, current,
		[&](const TacticalActorSnapshot* oldActor,
			const TacticalActorSnapshot* newActor) {
			if (oldActor == nullptr || newActor == nullptr ||
				!Present(*oldActor) || !Present(*newActor) ||
				SameLoadout(*oldActor, *newActor))
				return true;
			return visit(TacticalActorLoadoutChangedEvent{
				oldActor->id, oldActor->loadout, newActor->loadout});
		}))
		return false;

	if (!VisitDoorPairs(previous, current,
		[&](const TacticalDoorSnapshot* oldDoor,
			const TacticalDoorSnapshot* newDoor) {
			return oldDoor != nullptr || newDoor == nullptr ||
				visit(TacticalDoorEnteredEvent{*newDoor});
		}))
		return false;

	if (!VisitDoorPairs(previous, current,
		[&](const TacticalDoorSnapshot* oldDoor,
			const TacticalDoorSnapshot* newDoor) {
			return oldDoor == nullptr || newDoor != nullptr ||
				visit(TacticalDoorLeftEvent{oldDoor->baseGrid});
		}))
		return false;

	if (!VisitDoorPairs(previous, current,
		[&](const TacticalDoorSnapshot* oldDoor,
			const TacticalDoorSnapshot* newDoor) {
			return oldDoor == nullptr || newDoor == nullptr ||
				SameDoor(*oldDoor, *newDoor) ||
				visit(TacticalDoorChangedEvent{*oldDoor, *newDoor});
		}))
		return false;

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
	if (!previous.dimensions().valid() || !current.dimensions().valid() ||
		(previous.epoch() == current.epoch() &&
			!SameDimensions(previous.dimensions(), current.dimensions())))
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
