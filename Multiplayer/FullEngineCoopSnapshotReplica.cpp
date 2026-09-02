#include "FullEngineCoopSnapshotReplica.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace CoopSession
{
namespace
{
bool SameSector(const TacticalSectorSnapshot& left,
	const TacticalSectorSnapshot& right) noexcept
{
	return left.x == right.x && left.y == right.y && left.z == right.z &&
		left.loaded == right.loaded;
}

bool SameTurn(const TacticalTurnSnapshot& left,
	const TacticalTurnSnapshot& right) noexcept
{
	return left.turnBased == right.turnBased &&
		left.inCombat == right.inCombat &&
		left.activeTeam == right.activeTeam &&
		left.serial == right.serial &&
		left.interruptPhase == right.interruptPhase &&
		left.interruptSerial == right.interruptSerial &&
		left.commandsBlocked == right.commandsBlocked;
}

bool Present(const TacticalActorSnapshot& actor) noexcept
{
	return actor.active && actor.inSector;
}

bool ValidStance(TacticalStance stance) noexcept
{
	switch (stance)
	{
		case TacticalStance::Unknown:
		case TacticalStance::Standing:
		case TacticalStance::Crouched:
		case TacticalStance::Prone:
			return true;
	}
	return false;
}

bool ValidActor(const TacticalActorSnapshot& actor) noexcept
{
	return actor.id.valid() && ValidStance(actor.stance) &&
		actor.loadout.valid() &&
		(!actor.inSector || actor.active);
}

bool ValidSnapshotActors(
	const std::vector<TacticalActorSnapshot>& actors,
	bool requirePresent) noexcept
{
	if (actors.size() > MaximumCoopTacticalSnapshotActors) return false;
	for (std::size_t index = 0; index < actors.size(); ++index)
	{
		const TacticalActorSnapshot& actor = actors[index];
		if (!ValidActor(actor) ||
			(requirePresent && !Present(actor)))
			return false;
		if (index == 0) continue;
		const TacticalActorSnapshot& previous = actors[index - 1];
		if (!(previous.id < actor.id) || previous.id.slot == actor.id.slot)
			return false;
	}
	return true;
}

bool ValidDoor(const TacticalDoorSnapshot& door,
	const TacticalWorldDimensions& dimensions) noexcept
{
	return dimensions.contains(door.baseGrid) && door.structureId != 0;
}

bool ValidSnapshotDoors(const TacticalWorldSnapshot& snapshot) noexcept
{
	const std::vector<TacticalDoorSnapshot>& doors = snapshot.doors();
	if (doors.size() > TacticalWorldSnapshot::DefaultMaximumDoors)
		return false;
	for (std::size_t index = 0; index < doors.size(); ++index)
	{
		if (!ValidDoor(doors[index], snapshot.dimensions())) return false;
		if (index != 0 &&
			doors[index - 1].baseGrid >= doors[index].baseGrid)
			return false;
	}
	return true;
}

bool ValidAssignedActors(const CoopTacticalBaseline& baseline) noexcept
{
	if (baseline.assignedActors.size() >
		MaximumCoopTacticalAssignedActors)
		return false;
	for (std::size_t index = 0;
		index < baseline.assignedActors.size(); ++index)
	{
		const TacticalEntityId id = baseline.assignedActors[index];
		if (!id.valid()) return false;
		if (index != 0)
		{
			const TacticalEntityId previous =
				baseline.assignedActors[index - 1];
			if (!(previous < id) || previous.slot == id.slot) return false;
		}
		const TacticalActorSnapshot* actor = baseline.snapshot.find(id);
		if (actor == nullptr || !Present(*actor)) return false;
	}
	return true;
}

const TacticalEntityId* EventActor(const TacticalWorldEvent& event) noexcept
{
	if (const auto* entered =
		std::get_if<TacticalActorEnteredEvent>(&event))
		return &entered->actor.id;
	if (const auto* left = std::get_if<TacticalActorLeftEvent>(&event))
		return &left->actor;
	if (const auto* moved = std::get_if<TacticalActorMovedEvent>(&event))
		return &moved->actor;
	if (const auto* stance =
		std::get_if<TacticalActorStanceChangedEvent>(&event))
		return &stance->actor;
	if (const auto* vitals =
		std::get_if<TacticalActorVitalsChangedEvent>(&event))
		return &vitals->actor;
	if (const auto* loadout =
		std::get_if<TacticalActorLoadoutChangedEvent>(&event))
		return &loadout->actor;
	return nullptr;
}

const std::int32_t* EventDoorGrid(const TacticalWorldEvent& event) noexcept
{
	if (const auto* entered = std::get_if<TacticalDoorEnteredEvent>(&event))
		return &entered->door.baseGrid;
	if (const auto* left = std::get_if<TacticalDoorLeftEvent>(&event))
		return &left->baseGrid;
	if (const auto* changed = std::get_if<TacticalDoorChangedEvent>(&event))
		return &changed->previous.baseGrid;
	return nullptr;
}

bool EventHasChange(const TacticalWorldEvent& event) noexcept
{
	if (const auto* changed =
		std::get_if<TacticalSectorChangedEvent>(&event))
		return !SameSector(changed->previous, changed->current);
	if (const auto* changed =
		std::get_if<TacticalTurnChangedEvent>(&event))
		return !SameTurn(changed->previous, changed->current);
	if (const auto* moved =
		std::get_if<TacticalActorMovedEvent>(&event))
		return moved->previousGrid != moved->currentGrid ||
			moved->previousLevel != moved->currentLevel ||
			moved->previousDirection != moved->currentDirection;
	if (const auto* stance =
		std::get_if<TacticalActorStanceChangedEvent>(&event))
		return stance->previous != stance->current ||
			stance->previousAnimation != stance->currentAnimation;
	if (const auto* vitals =
		std::get_if<TacticalActorVitalsChangedEvent>(&event))
		return vitals->previousActionPoints != vitals->currentActionPoints ||
			vitals->previousLife != vitals->currentLife ||
			vitals->previousMaximumLife != vitals->currentMaximumLife ||
			vitals->previousBreath != vitals->currentBreath ||
			vitals->previousMaximumBreath != vitals->currentMaximumBreath ||
			vitals->previousHostileToPlayerTeam !=
				vitals->currentHostileToPlayerTeam ||
			vitals->previousInterruptActionEligible !=
				vitals->currentInterruptActionEligible;
	if (const auto* loadout =
		std::get_if<TacticalActorLoadoutChangedEvent>(&event))
		return loadout->previous != loadout->current;
	if (const auto* changed =
		std::get_if<TacticalDoorChangedEvent>(&event))
		return changed->previous.baseGrid == changed->current.baseGrid &&
			(changed->previous.structureId != changed->current.structureId ||
			 changed->previous.open != changed->current.open);
	return true;
}

bool ValidDeltaShape(const TacticalWorldDelta& delta,
	std::uint64_t previousTurnSerial,
	std::uint64_t resultingTurnSerial) noexcept
{
	if (delta.events.size() > MaximumCoopTacticalDeltaEvents) return false;

	std::size_t previousKind = 0;
	const TacticalEntityId* previousActor = nullptr;
	const std::int32_t* previousDoorGrid = nullptr;
	const TacticalTurnChangedEvent* turnChange = nullptr;
	bool havePrevious = false;
	for (const TacticalWorldEvent& event : delta.events)
	{
		if (event.valueless_by_exception() || event.index() == 0 ||
			!EventHasChange(event))
			return false;
		if (havePrevious && event.index() < previousKind) return false;

		const TacticalEntityId* actor = EventActor(event);
		const std::int32_t* doorGrid = EventDoorGrid(event);
		if (actor != nullptr && !actor->valid()) return false;
		if (havePrevious && event.index() == previousKind)
		{
			// Sector and turn changes are singleton categories. Actor categories
			// use strict identity order, matching the wire canonical form.
			if (event.index() < 3 ||
				((actor == nullptr || previousActor == nullptr ||
				  !(*previousActor < *actor)) &&
				 (doorGrid == nullptr || previousDoorGrid == nullptr ||
				  *previousDoorGrid >= *doorGrid)))
				return false;
		}

		if (const auto* entered =
			std::get_if<TacticalActorEnteredEvent>(&event))
		{
			if (!ValidActor(entered->actor) ||
				!Present(entered->actor))
				return false;
		}
		else if (const auto* changed =
			std::get_if<TacticalTurnChangedEvent>(&event))
		{
			if (turnChange != nullptr) return false;
			turnChange = changed;
		}
		else if (const auto* stance =
			std::get_if<TacticalActorStanceChangedEvent>(&event))
		{
			if (!ValidStance(stance->previous) ||
				!ValidStance(stance->current))
				return false;
		}
		else if (const auto* loadout =
			std::get_if<TacticalActorLoadoutChangedEvent>(&event))
		{
			if (!loadout->previous.valid() || !loadout->current.valid())
				return false;
		}
		else if (const auto* entered =
			std::get_if<TacticalDoorEnteredEvent>(&event))
		{
			if (entered->door.baseGrid < 0 ||
				entered->door.structureId == 0)
				return false;
		}
		else if (const auto* left =
			std::get_if<TacticalDoorLeftEvent>(&event))
		{
			if (left->baseGrid < 0) return false;
		}
		else if (const auto* changed =
			std::get_if<TacticalDoorChangedEvent>(&event))
		{
			if (changed->previous.baseGrid < 0 ||
				changed->previous.baseGrid != changed->current.baseGrid ||
				changed->previous.structureId == 0 ||
				changed->current.structureId == 0)
				return false;
		}

		previousKind = event.index();
		previousActor = actor;
		previousDoorGrid = doorGrid;
		havePrevious = true;
	}

	if (turnChange == nullptr)
		return previousTurnSerial == resultingTurnSerial;
	return turnChange->previous.serial == previousTurnSerial &&
		turnChange->current.serial == resultingTurnSerial;
}

auto FindActor(std::vector<TacticalActorSnapshot>& actors,
	TacticalEntityId id)
{
	return std::lower_bound(actors.begin(), actors.end(), id,
		[](const TacticalActorSnapshot& actor, TacticalEntityId sought) {
			return actor.id < sought;
		});
}

auto FindDoor(std::vector<TacticalDoorSnapshot>& doors,
	std::int32_t baseGrid)
{
	return std::lower_bound(doors.begin(), doors.end(), baseGrid,
		[](const TacticalDoorSnapshot& door, std::int32_t sought) {
			return door.baseGrid < sought;
		});
}

bool ApplyEvent(const TacticalWorldEvent& event,
	TacticalSectorSnapshot& sector,
	TacticalTurnSnapshot& turn,
	std::vector<TacticalActorSnapshot>& actors,
	std::vector<TacticalDoorSnapshot>& doors,
	std::array<TacticalEntityId,
		MaximumCoopTacticalSnapshotActors * 2>& membershipChanged,
	std::size_t& membershipChangedCount,
	std::array<std::int32_t,
		TacticalWorldSnapshot::DefaultMaximumDoors * 2>& doorMembershipChanged,
	std::size_t& doorMembershipChangedCount)
{
	auto membershipAlreadyChanged = [&](TacticalEntityId actor) noexcept {
		return std::find(membershipChanged.begin(),
			membershipChanged.begin() + membershipChangedCount,
			actor) != membershipChanged.begin() + membershipChangedCount;
	};
	auto recordMembershipChange = [&](TacticalEntityId actor) noexcept {
		if (membershipChangedCount >= membershipChanged.size() ||
			membershipAlreadyChanged(actor))
			return false;
		membershipChanged[membershipChangedCount++] = actor;
		return true;
	};
	auto doorMembershipAlreadyChanged = [&](std::int32_t baseGrid) noexcept {
		return std::find(doorMembershipChanged.begin(),
			doorMembershipChanged.begin() + doorMembershipChangedCount,
			baseGrid) !=
			doorMembershipChanged.begin() + doorMembershipChangedCount;
	};
	auto recordDoorMembershipChange = [&](std::int32_t baseGrid) noexcept {
		if (doorMembershipChangedCount >= doorMembershipChanged.size() ||
			doorMembershipAlreadyChanged(baseGrid))
			return false;
		doorMembershipChanged[doorMembershipChangedCount++] = baseGrid;
		return true;
	};

	if (std::get_if<TacticalWorldResetEvent>(&event) != nullptr)
	{
		// A reset carries no replacement actor/sector state. The host must send a
		// new baseline for a new generation instead of asking a replica to guess.
		return false;
	}
	if (const auto* changed =
		std::get_if<TacticalSectorChangedEvent>(&event))
	{
		if (!SameSector(sector, changed->previous)) return false;
		sector = changed->current;
		return true;
	}
	if (const auto* changed =
		std::get_if<TacticalTurnChangedEvent>(&event))
	{
		if (!SameTurn(turn, changed->previous)) return false;
		turn = changed->current;
		return true;
	}
	if (const auto* entered =
		std::get_if<TacticalActorEnteredEvent>(&event))
	{
		if (!recordMembershipChange(entered->actor.id)) return false;
		auto actor = FindActor(actors, entered->actor.id);
		if (actor != actors.end() && actor->id == entered->actor.id)
		{
			if (Present(*actor)) return false;
			*actor = entered->actor;
		}
		else
		{
			actors.insert(actor, entered->actor);
		}
		return true;
	}
	if (const auto* left = std::get_if<TacticalActorLeftEvent>(&event))
	{
		if (!recordMembershipChange(left->actor)) return false;
		auto actor = FindActor(actors, left->actor);
		if (actor == actors.end() || actor->id != left->actor ||
			!Present(*actor))
			return false;
		actors.erase(actor);
		return true;
	}
	if (const auto* moved = std::get_if<TacticalActorMovedEvent>(&event))
	{
		if (membershipAlreadyChanged(moved->actor)) return false;
		auto actor = FindActor(actors, moved->actor);
		if (actor == actors.end() || actor->id != moved->actor ||
			!Present(*actor) || actor->grid != moved->previousGrid ||
			actor->level != moved->previousLevel ||
			actor->direction != moved->previousDirection)
			return false;
		actor->grid = moved->currentGrid;
		actor->level = moved->currentLevel;
		actor->direction = moved->currentDirection;
		return true;
	}
	if (const auto* stance =
		std::get_if<TacticalActorStanceChangedEvent>(&event))
	{
		if (membershipAlreadyChanged(stance->actor)) return false;
		auto actor = FindActor(actors, stance->actor);
		if (actor == actors.end() || actor->id != stance->actor ||
			!Present(*actor) || actor->stance != stance->previous ||
			actor->animation != stance->previousAnimation)
			return false;
		actor->stance = stance->current;
		actor->animation = stance->currentAnimation;
		return true;
	}
	if (const auto* vitals =
		std::get_if<TacticalActorVitalsChangedEvent>(&event))
	{
		if (membershipAlreadyChanged(vitals->actor)) return false;
		auto actor = FindActor(actors, vitals->actor);
		if (actor == actors.end() || actor->id != vitals->actor ||
			!Present(*actor) ||
			actor->actionPoints != vitals->previousActionPoints ||
			actor->life != vitals->previousLife ||
			actor->maximumLife != vitals->previousMaximumLife ||
			actor->breath != vitals->previousBreath ||
			actor->maximumBreath != vitals->previousMaximumBreath ||
			actor->hostileToPlayerTeam !=
				vitals->previousHostileToPlayerTeam ||
			actor->interruptActionEligible !=
				vitals->previousInterruptActionEligible)
			return false;
		actor->actionPoints = vitals->currentActionPoints;
		actor->life = vitals->currentLife;
		actor->maximumLife = vitals->currentMaximumLife;
		actor->breath = vitals->currentBreath;
		actor->maximumBreath = vitals->currentMaximumBreath;
		actor->hostileToPlayerTeam =
			vitals->currentHostileToPlayerTeam;
		actor->interruptActionEligible =
			vitals->currentInterruptActionEligible;
		return true;
	}
	if (const auto* loadout =
		std::get_if<TacticalActorLoadoutChangedEvent>(&event))
	{
		if (membershipAlreadyChanged(loadout->actor)) return false;
		auto actor = FindActor(actors, loadout->actor);
		if (actor == actors.end() || actor->id != loadout->actor ||
			!Present(*actor) || actor->loadout != loadout->previous)
			return false;
		actor->loadout = loadout->current;
		return true;
	}
	if (const auto* entered =
		std::get_if<TacticalDoorEnteredEvent>(&event))
	{
		if (!recordDoorMembershipChange(entered->door.baseGrid)) return false;
		auto door = FindDoor(doors, entered->door.baseGrid);
		if (door != doors.end() && door->baseGrid == entered->door.baseGrid)
			return false;
		doors.insert(door, entered->door);
		return true;
	}
	if (const auto* left = std::get_if<TacticalDoorLeftEvent>(&event))
	{
		if (!recordDoorMembershipChange(left->baseGrid)) return false;
		auto door = FindDoor(doors, left->baseGrid);
		if (door == doors.end() || door->baseGrid != left->baseGrid)
			return false;
		doors.erase(door);
		return true;
	}
	if (const auto* changed =
		std::get_if<TacticalDoorChangedEvent>(&event))
	{
		if (doorMembershipAlreadyChanged(changed->previous.baseGrid))
			return false;
		auto door = FindDoor(doors, changed->previous.baseGrid);
		if (door == doors.end() ||
			door->baseGrid != changed->previous.baseGrid ||
			door->structureId != changed->previous.structureId ||
			door->open != changed->previous.open)
			return false;
		*door = changed->current;
		return true;
	}
	return false;
}
}

FullEngineCoopReplicaApplyResult
FullEngineCoopSnapshotReplica::applyBaseline(
	const CoopTacticalBaseline& baseline) noexcept
{
	if (!IsValidCoopTacticalStateIdentity(baseline.state) ||
		baseline.baselineId == 0 || !baseline.snapshot.sector().loaded ||
		!baseline.snapshot.dimensions().valid() ||
		baseline.snapshot.epoch() != baseline.state.worldGeneration ||
		baseline.snapshot.turn().serial != baseline.state.turnSerial ||
		!ValidSnapshotActors(baseline.snapshot.actors(), false) ||
		!ValidSnapshotDoors(baseline.snapshot) ||
		!ValidAssignedActors(baseline))
		return FullEngineCoopReplicaApplyResult::Rejected;

	try
	{
		std::vector<TacticalActorSnapshot> actors;
		actors.reserve(baseline.snapshot.actors().size());
		for (const TacticalActorSnapshot& actor : baseline.snapshot.actors())
			if (Present(actor)) actors.push_back(actor);
		std::vector<TacticalDoorSnapshot> doors = baseline.snapshot.doors();

		TacticalWorldSnapshot accepted;
		if (TacticalWorldSnapshot::create(baseline.snapshot.epoch(),
			baseline.snapshot.dimensions(), baseline.snapshot.sector(),
			baseline.snapshot.turn(),
			std::move(actors), std::move(doors), accepted,
			MaximumCoopTacticalSnapshotActors,
			TacticalWorldSnapshot::DefaultMaximumDoors) !=
			TacticalSnapshotCreateError::None)
			return FullEngineCoopReplicaApplyResult::Rejected;
		snapshot_ = std::move(accepted);
		state_ = baseline.state;
		hasSnapshot_ = true;
		return FullEngineCoopReplicaApplyResult::Committed;
	}
	catch (...)
	{
		return FullEngineCoopReplicaApplyResult::Rejected;
	}
}

FullEngineCoopReplicaApplyResult FullEngineCoopSnapshotReplica::applyDelta(
	const CoopTacticalDelta& delta) noexcept
{
	if (!hasSnapshot_ || !IsValidCoopTacticalStateIdentity(delta.state) ||
		delta.state.sessionEpoch != state_.sessionEpoch ||
		delta.state.worldGeneration != state_.worldGeneration ||
		delta.baseRevision != state_.revision ||
		delta.state.revision <= state_.revision ||
		delta.state.turnSerial < state_.turnSerial ||
		delta.delta.previousEpoch != snapshot_.epoch() ||
		delta.delta.currentEpoch != snapshot_.epoch() ||
		delta.deltaId == 0 ||
		!ValidDeltaShape(delta.delta,
			state_.turnSerial, delta.state.turnSerial))
		return FullEngineCoopReplicaApplyResult::Rejected;

	try
	{
		TacticalSectorSnapshot sector = snapshot_.sector();
		TacticalTurnSnapshot turn = snapshot_.turn();
		std::vector<TacticalActorSnapshot> actors = snapshot_.actors();
		std::vector<TacticalDoorSnapshot> doors = snapshot_.doors();
		std::array<TacticalEntityId,
			MaximumCoopTacticalSnapshotActors * 2> membershipChanged{};
		std::size_t membershipChangedCount = 0;
		std::array<std::int32_t,
			TacticalWorldSnapshot::DefaultMaximumDoors * 2>
			doorMembershipChanged{};
		std::size_t doorMembershipChangedCount = 0;
		for (const TacticalWorldEvent& event : delta.delta.events)
			if (!ApplyEvent(event, sector, turn, actors, doors,
				membershipChanged, membershipChangedCount,
				doorMembershipChanged, doorMembershipChangedCount))
				return FullEngineCoopReplicaApplyResult::Rejected;
		if (!sector.loaded || turn.serial != delta.state.turnSerial ||
			!ValidSnapshotActors(actors, true) ||
			doors.size() > TacticalWorldSnapshot::DefaultMaximumDoors)
			return FullEngineCoopReplicaApplyResult::Rejected;

		TacticalWorldSnapshot accepted;
		if (TacticalWorldSnapshot::create(snapshot_.epoch(),
			snapshot_.dimensions(), sector, turn,
			std::move(actors), std::move(doors), accepted,
			MaximumCoopTacticalSnapshotActors,
			TacticalWorldSnapshot::DefaultMaximumDoors) !=
			TacticalSnapshotCreateError::None)
			return FullEngineCoopReplicaApplyResult::Rejected;
		snapshot_ = std::move(accepted);
		state_ = delta.state;
		return FullEngineCoopReplicaApplyResult::Committed;
	}
	catch (...)
	{
		return FullEngineCoopReplicaApplyResult::Rejected;
	}
}

void FullEngineCoopSnapshotReplica::clear() noexcept
{
	snapshot_ = TacticalWorldSnapshot{};
	state_ = {};
	hasSnapshot_ = false;
}
}
