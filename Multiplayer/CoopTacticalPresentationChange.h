#ifndef JA2_COOP_TACTICAL_PRESENTATION_CHANGE_H
#define JA2_COOP_TACTICAL_PRESENTATION_CHANGE_H

#include <Engine/Adapters/JA2/TacticalWorldDelta.h>

#include <cstddef>
#include <variant>

namespace CoopSession
{
// This is a narrow revision-classification predicate, not permission to run a
// command. Admission still checks the authenticated peer's acknowledged state,
// world/turn identity, actor ownership, and the current native gameplay gates.
// Surface, palette, height, and direction values intentionally remain strict:
// JA2 uses some nominal presentation fields for footprints and disguise rules.
inline bool IsFrameOnlyTacticalWorldChange(
	const TacticalWorldSnapshot& previous,
	const TacticalWorldSnapshot& current,
	const TacticalWorldDelta& delta) noexcept
{
	if (previous.epoch() == 0 || current.epoch() != previous.epoch() ||
		delta.previousEpoch != previous.epoch() ||
		delta.currentEpoch != current.epoch() || delta.events.empty() ||
		!previous.dimensions().valid() || !current.dimensions().valid() ||
		previous.dimensions().columns != current.dimensions().columns ||
		previous.dimensions().rows != current.dimensions().rows ||
		!IsValidTacticalSectorSnapshot(previous.sector()) ||
		!IsValidTacticalSectorSnapshot(current.sector()) ||
		!IsValidTacticalInterruptState(previous.turn()) ||
		!IsValidTacticalInterruptState(current.turn()) ||
		!previous.lighting().valid() || !current.lighting().valid() ||
		previous.lighting() != current.lighting() ||
		previous.actors().size() != current.actors().size() ||
		previous.doors().size() != current.doors().size())
		return false;

	const TacticalSectorSnapshot& oldSector = previous.sector();
	const TacticalSectorSnapshot& newSector = current.sector();
	if (oldSector.x != newSector.x || oldSector.y != newSector.y ||
		oldSector.z != newSector.z || oldSector.loaded != newSector.loaded ||
		oldSector.mapAssetKey != newSector.mapAssetKey)
		return false;
	const TacticalTurnSnapshot& oldTurn = previous.turn();
	const TacticalTurnSnapshot& newTurn = current.turn();
	if (oldTurn.turnBased != newTurn.turnBased ||
		oldTurn.inCombat != newTurn.inCombat ||
		oldTurn.activeTeam != newTurn.activeTeam || oldTurn.serial != newTurn.serial ||
		oldTurn.commandsBlocked != newTurn.commandsBlocked ||
		oldTurn.interruptPhase != newTurn.interruptPhase ||
		oldTurn.interruptSerial != newTurn.interruptSerial)
		return false;

	for (std::size_t index = 0; index < previous.doors().size(); ++index)
	{
		const TacticalDoorSnapshot& oldDoor = previous.doors()[index];
		const TacticalDoorSnapshot& newDoor = current.doors()[index];
		if (!previous.dimensions().contains(oldDoor.baseGrid) ||
			oldDoor.structureId == 0 || oldDoor.baseGrid != newDoor.baseGrid ||
			oldDoor.structureId != newDoor.structureId || oldDoor.open != newDoor.open ||
			(index != 0 && previous.doors()[index - 1].baseGrid >= oldDoor.baseGrid))
			return false;
	}

	std::size_t eventIndex = 0;
	for (std::size_t index = 0; index < previous.actors().size(); ++index)
	{
		const TacticalActorSnapshot& oldActor = previous.actors()[index];
		const TacticalActorSnapshot& newActor = current.actors()[index];
		if (!oldActor.id.valid() || !newActor.id.valid() ||
			!oldActor.loadout.valid() || !newActor.loadout.valid() ||
			!IsValidTacticalActorPresentation(oldActor, previous.dimensions()) ||
			!IsValidTacticalActorPresentation(newActor, current.dimensions()) ||
			!IsValidTacticalInterruptEligibility(oldActor, oldTurn) ||
			!IsValidTacticalInterruptEligibility(newActor, newTurn) ||
			(index != 0 &&
			 (!(previous.actors()[index - 1].id < oldActor.id) ||
			  previous.actors()[index - 1].id.slot == oldActor.id.slot)))
			return false;

		// Normalizing exactly one scalar before full equality keeps every other
		// current actor field (including presentation and equipment) significant.
		TacticalActorSnapshot normalized = oldActor;
		normalized.presentation.animationFrame = newActor.presentation.animationFrame;
		if (normalized != newActor) return false;
		if (oldActor.presentation.animationFrame == newActor.presentation.animationFrame)
			continue;
		if (!oldActor.active || !oldActor.inSector || eventIndex == delta.events.size())
			return false;
		const TacticalActorUpdatedEvent* event =
			std::get_if<TacticalActorUpdatedEvent>(&delta.events[eventIndex++]);
		if (event == nullptr || event->actor != newActor) return false;
	}
	// Exact correspondence also rejects omitted, duplicate, reordered, no-op,
	// mixed-category, and unknown/valueless events without allocating storage.
	return eventIndex != 0 && eventIndex == delta.events.size();
}
}

#endif
