#include "FullEngineCoopClientController.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <limits>

namespace
{
bool ValidView(const FullEngineCoopClientControllerView& view) noexcept
{
	return view.snapshot != nullptr &&
		view.snapshot->sector().loaded &&
		view.assignedActorCount <=
			CoopSession::MaximumCoopTacticalAssignedActors &&
		(view.assignedActorCount == 0 || view.assignedActors != nullptr);
}
}

void FullEngineCoopClientController::synchronize(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!ValidView(view))
	{
		selectedActor_ = TacticalEntityId{};
		cancelDestinationEntry();
		cancelAttackTargeting();
		cancelDoorSelection();
		return;
	}

	if (assignedAndPresent(view, selectedActor_))
	{
		if (!actionsEnabled(view))
		{
			cancelDestinationEntry();
			cancelAttackTargeting();
			cancelDoorSelection();
		}
		else if (targetingAttack_ &&
			!attackTargetCandidate(view, attackTarget_))
		{
			attackTarget_ = TacticalEntityId{};
			if (!selectTargetRelative(view, true)) cancelAttackTargeting();
		}
		if (selectingDoor_ && !exactDoorSelectionCurrent(view))
			cancelDoorSelection();
		return;
	}

	selectedActor_ = TacticalEntityId{};
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
	{
		const TacticalEntityId candidate = view.assignedActors[index];
		if (view.snapshot->find(candidate) == nullptr) continue;
		selectedActor_ = candidate;
		break;
	}
	cancelDestinationEntry();
	cancelAttackTargeting();
	cancelDoorSelection();
}

bool FullEngineCoopClientController::ready(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	return ValidView(view) && selectedActor_.valid() &&
		assignedAndPresent(view, selectedActor_);
}

bool FullEngineCoopClientController::actionsEnabled(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (!ready(view) || view.resynchronizing ||
		view.outstandingCommandId != 0)
		return false;
	const TacticalActorSnapshot* const actor =
		view.snapshot->find(selectedActor_);
	if (actor == nullptr || !actor->active || !actor->inSector ||
		actor->life <= 0)
		return false;
	const TacticalTurnSnapshot& turn = view.snapshot->turn();
	if (turn.commandsBlocked) return false;
	switch (turn.interruptPhase)
	{
		case TacticalInterruptPhase::None:
			return !turn.turnBased || !turn.inCombat ||
				turn.activeTeam == actor->team;
		case TacticalInterruptPhase::Resolving:
			return false;
		case TacticalInterruptPhase::Active:
			return turn.turnBased && turn.inCombat &&
				turn.interruptSerial != 0 &&
				actor->interruptActionEligible;
	}
	return false;
}

bool FullEngineCoopClientController::selectNext(
	const FullEngineCoopClientControllerView& view) noexcept
{
	return selectRelative(view, true);
}

bool FullEngineCoopClientController::selectPrevious(
	const FullEngineCoopClientControllerView& view) noexcept
{
	return selectRelative(view, false);
}

bool FullEngineCoopClientController::selectRelative(
	const FullEngineCoopClientControllerView& view,
	bool forward) noexcept
{
	if (!ValidView(view) || view.assignedActorCount == 0) return false;
	std::size_t selectedIndex = view.assignedActorCount;
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		if (view.assignedActors[index] == selectedActor_)
		{
			selectedIndex = index;
			break;
		}

	for (std::size_t offset = 1;
		offset <= view.assignedActorCount; ++offset)
	{
		const std::size_t index = forward
			? (selectedIndex == view.assignedActorCount
				? offset - 1
				: (selectedIndex + offset) % view.assignedActorCount)
			: (selectedIndex == view.assignedActorCount
				? view.assignedActorCount - offset
				: (selectedIndex + view.assignedActorCount -
					(offset % view.assignedActorCount)) %
					view.assignedActorCount);
		const TacticalEntityId candidate = view.assignedActors[index];
		if (view.snapshot->find(candidate) == nullptr) continue;
		const bool changed = candidate != selectedActor_;
		selectedActor_ = candidate;
		if (changed)
		{
			cancelDestinationEntry();
			cancelAttackTargeting();
			cancelDoorSelection();
		}
		return true;
	}
	return false;
}

bool FullEngineCoopClientController::beginDestinationEntry(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!actionsEnabled(view)) return false;
	cancelAttackTargeting();
	cancelDoorSelection();
	destination_[0] = '\0';
	destinationLength_ = 0;
	enteringDestination_ = true;
	reverse_ = false;
	return true;
}

bool FullEngineCoopClientController::appendDestinationDigit(
	unsigned digit) noexcept
{
	if (!enteringDestination_ || digit > 9 ||
		destinationLength_ + 1 >= sizeof(destination_))
		return false;
	destination_[destinationLength_++] =
		static_cast<char>('0' + digit);
	destination_[destinationLength_] = '\0';
	return true;
}

bool FullEngineCoopClientController::eraseDestinationDigit() noexcept
{
	if (!enteringDestination_ || destinationLength_ == 0) return false;
	destination_[--destinationLength_] = '\0';
	return true;
}

void FullEngineCoopClientController::cancelDestinationEntry() noexcept
{
	destination_[0] = '\0';
	destinationLength_ = 0;
	enteringDestination_ = false;
	reverse_ = false;
}

void FullEngineCoopClientController::toggleReverse() noexcept
{
	if (enteringDestination_) reverse_ = !reverse_;
}

bool FullEngineCoopClientController::beginAttackTargeting(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!actionsEnabled(view)) return false;
	cancelDestinationEntry();
	cancelDoorSelection();
	targetingAttack_ = true;
	attackTarget_ = TacticalEntityId{};
	attackAimTime_ = 1;
	if (selectTargetRelative(view, true)) return true;
	cancelAttackTargeting();
	return false;
}

bool FullEngineCoopClientController::selectNextTarget(
	const FullEngineCoopClientControllerView& view) noexcept
{
	return targetingAttack_ && selectTargetRelative(view, true);
}

bool FullEngineCoopClientController::selectPreviousTarget(
	const FullEngineCoopClientControllerView& view) noexcept
{
	return targetingAttack_ && selectTargetRelative(view, false);
}

bool FullEngineCoopClientController::adjustAttackAim(int delta) noexcept
{
	if (!targetingAttack_ || delta == 0) return false;
	const int adjusted = static_cast<int>(attackAimTime_) + delta;
	if (adjusted < 0 || adjusted >
		static_cast<int>(CoopSession::MaximumTacticalFirearmAimTime))
		return false;
	attackAimTime_ = static_cast<std::uint8_t>(adjusted);
	return true;
}

void FullEngineCoopClientController::cancelAttackTargeting() noexcept
{
	attackTarget_ = TacticalEntityId{};
	attackAimTime_ = 1;
	targetingAttack_ = false;
}

bool FullEngineCoopClientController::beginDoorSelection(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!actionsEnabled(view)) return false;
	cancelDestinationEntry();
	cancelAttackTargeting();
	cancelDoorSelection();
	selectingDoor_ = true;
	doorActor_ = selectedActor_;
	doorWorldEpoch_ = view.snapshot->epoch();
	if (selectDoorRelative(view, true)) return true;
	cancelDoorSelection();
	return false;
}

bool FullEngineCoopClientController::selectNextDoor(
	const FullEngineCoopClientControllerView& view) noexcept
{
	return selectingDoor_ && selectDoorRelative(view, true);
}

bool FullEngineCoopClientController::selectPreviousDoor(
	const FullEngineCoopClientControllerView& view) noexcept
{
	return selectingDoor_ && selectDoorRelative(view, false);
}

void FullEngineCoopClientController::cancelDoorSelection() noexcept
{
	doorActor_ = TacticalEntityId{};
	doorWorldEpoch_ = 0;
	selectedDoorBaseGrid_ = -1;
	selectedDoorStructureId_ = 0;
	selectedDoorOpen_ = false;
	selectingDoor_ = false;
}

FullEngineCoopClientIntentRequest
FullEngineCoopClientController::submitMove(
	const FullEngineCoopClientControllerView& view,
	std::uint16_t movementMode) noexcept
{
	if (!enteringDestination_ || !actionsEnabled(view) ||
		destinationLength_ == 0)
		return {};

	std::uint64_t destination = 0;
	for (std::uint8_t index = 0; index < destinationLength_; ++index)
	{
		destination = destination * 10u +
			static_cast<unsigned>(destination_[index] - '0');
		if (destination > static_cast<std::uint64_t>(
			(std::numeric_limits<std::int32_t>::max)()))
			return {};
	}
	const bool reverse = reverse_;
	cancelDestinationEntry();
	return request(view, CoopSession::MoveTacticalIntent{
		static_cast<std::int32_t>(destination), movementMode, reverse});
}

FullEngineCoopClientIntentRequest
FullEngineCoopClientController::submitRelativeMove(
	const FullEngineCoopClientControllerView& view,
	int deltaRow, int deltaColumn,
	std::uint16_t movementMode) const noexcept
{
	if (!actionsEnabled(view) || deltaRow < -1 || deltaRow > 1 ||
		deltaColumn < -1 || deltaColumn > 1 ||
		(deltaRow == 0 && deltaColumn == 0))
		return {};

	const TacticalActorSnapshot* const actor =
		view.snapshot->find(selectedActor_);
	const TacticalWorldDimensions& dimensions =
		view.snapshot->dimensions();
	if (actor == nullptr || !actor->active || !actor->inSector ||
		actor->life <= 0 || !dimensions.valid() ||
		!dimensions.contains(actor->grid))
		return {};

	const std::int32_t columns = dimensions.columns;
	const std::int32_t rows = dimensions.rows;
	const std::int32_t currentRow = actor->grid / columns;
	const std::int32_t currentColumn = actor->grid % columns;
	const std::int32_t targetRow = currentRow + deltaRow;
	const std::int32_t targetColumn = currentColumn + deltaColumn;
	if (targetRow < 0 || targetRow >= rows || targetColumn < 0 ||
		targetColumn >= columns)
		return {};

	const std::int32_t targetGrid = targetRow * columns + targetColumn;
	if (!dimensions.contains(targetGrid)) return {};
	return request(view, CoopSession::MoveTacticalIntent{
		targetGrid, movementMode, false});
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::face(
	const FullEngineCoopClientControllerView& view,
	std::uint8_t direction) const noexcept
{
	if (direction >= 8) return {};
	return request(view, CoopSession::FaceTacticalIntent{direction});
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::stance(
	const FullEngineCoopClientControllerView& view,
	CoopSession::TacticalIntentStance stanceValue) const noexcept
{
	if (!CoopSession::IsKnownTacticalIntentStance(stanceValue)) return {};
	return request(view, CoopSession::StanceTacticalIntent{stanceValue});
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::stop(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	return request(view, CoopSession::StopTacticalIntent{});
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::endTurn(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (view.snapshot == nullptr || !view.snapshot->turn().turnBased ||
		!view.snapshot->turn().inCombat)
		return {};
	const TacticalTurnSnapshot& turn = view.snapshot->turn();
	if (turn.interruptPhase == TacticalInterruptPhase::Active)
	{
		if (turn.interruptSerial == 0) return {};
		return request(view, CoopSession::PassInterruptTacticalIntent{
			turn.interruptSerial});
	}
	return request(view, CoopSession::EndTurnTacticalIntent{});
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::reload(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	return request(view, CoopSession::ReloadTacticalIntent{});
}

FullEngineCoopClientIntentRequest
FullEngineCoopClientController::submitAimedFirearmAttack(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!targetingAttack_ || !actionsEnabled(view) ||
		!attackTargetCandidate(view, attackTarget_))
		return {};
	const TacticalEntityId target = attackTarget_;
	const std::uint8_t aimTime = attackAimTime_;
	cancelAttackTargeting();
	return request(view, CoopSession::AimedFirearmAttackTacticalIntent{
		target, aimTime});
}

FullEngineCoopClientIntentRequest
FullEngineCoopClientController::submitDoorOpenClose(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!selectingDoor_ || !actionsEnabled(view) ||
		!exactDoorSelectionCurrent(view))
		return {};
	const CoopSession::DoorOpenCloseTacticalIntent payload{
		selectedDoorBaseGrid_, selectedDoorStructureId_, !selectedDoorOpen_};
	cancelDoorSelection();
	return request(view, payload);
}

bool FullEngineCoopClientController::assignedAndPresent(
	const FullEngineCoopClientControllerView& view,
	TacticalEntityId actor) const noexcept
{
	if (!actor.valid() || !ValidView(view) ||
		view.snapshot->find(actor) == nullptr)
		return false;
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		if (view.assignedActors[index] == actor) return true;
	return false;
}

bool FullEngineCoopClientController::attackTargetCandidate(
	const FullEngineCoopClientControllerView& view,
	TacticalEntityId target) const noexcept
{
	if (!target.valid() || target == selectedActor_ || !ValidView(view))
		return false;
	const TacticalActorSnapshot* const actor =
		view.snapshot->find(selectedActor_);
	const TacticalActorSnapshot* const candidate = view.snapshot->find(target);
	return actor != nullptr && candidate != nullptr && candidate->active &&
		candidate->inSector && candidate->life > 0 &&
		candidate->hostileToPlayerTeam;
}

bool FullEngineCoopClientController::selectTargetRelative(
	const FullEngineCoopClientControllerView& view,
	bool forward) noexcept
{
	if (!targetingAttack_ || !ValidView(view)) return false;
	const auto& actors = view.snapshot->actors();
	if (actors.empty()) return false;
	std::size_t selectedIndex = actors.size();
	for (std::size_t index = 0; index < actors.size(); ++index)
		if (actors[index].id == attackTarget_)
		{
			selectedIndex = index;
			break;
		}
	for (std::size_t offset = 1; offset <= actors.size(); ++offset)
	{
		const std::size_t index = forward
			? (selectedIndex == actors.size()
				? offset - 1
				: (selectedIndex + offset) % actors.size())
			: (selectedIndex == actors.size()
				? actors.size() - offset
				: (selectedIndex + actors.size() -
					(offset % actors.size())) % actors.size());
		if (!attackTargetCandidate(view, actors[index].id)) continue;
		attackTarget_ = actors[index].id;
		return true;
	}
	return false;
}

bool FullEngineCoopClientController::doorCandidate(
	const FullEngineCoopClientControllerView& view,
	const TacticalDoorSnapshot& door) const noexcept
{
	if (!ValidView(view) || !view.snapshot->dimensions().valid() ||
		door.baseGrid < 0 || door.structureId == 0)
		return false;
	const TacticalActorSnapshot* const actor =
		view.snapshot->find(selectedActor_);
	if (actor == nullptr || !actor->active || !actor->inSector ||
		actor->life <= 0 || actor->level != 0 ||
		!view.snapshot->dimensions().contains(actor->grid) ||
		!view.snapshot->dimensions().contains(door.baseGrid))
		return false;
	const std::int32_t columns = view.snapshot->dimensions().columns;
	const std::int32_t actorRow = actor->grid / columns;
	const std::int32_t actorColumn = actor->grid % columns;
	const std::int32_t doorRow = door.baseGrid / columns;
	const std::int32_t doorColumn = door.baseGrid % columns;
	const std::int32_t rowDistance = actorRow > doorRow
		? actorRow - doorRow : doorRow - actorRow;
	const std::int32_t columnDistance = actorColumn > doorColumn
		? actorColumn - doorColumn : doorColumn - actorColumn;
	return rowDistance + columnDistance <= 1;
}

bool FullEngineCoopClientController::exactDoorSelectionCurrent(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (!selectingDoor_ || !ValidView(view) ||
		doorActor_ != selectedActor_ ||
		view.snapshot->epoch() != doorWorldEpoch_)
		return false;
	const TacticalDoorSnapshot* const door =
		view.snapshot->findDoor(selectedDoorBaseGrid_);
	return door != nullptr &&
		door->structureId == selectedDoorStructureId_ &&
		door->open == selectedDoorOpen_ && doorCandidate(view, *door);
}

bool FullEngineCoopClientController::selectDoorRelative(
	const FullEngineCoopClientControllerView& view,
	bool forward) noexcept
{
	if (!selectingDoor_ || !ValidView(view) ||
		doorActor_ != selectedActor_ ||
		view.snapshot->epoch() != doorWorldEpoch_)
		return false;
	const auto& doors = view.snapshot->doors();
	if (doors.empty()) return false;
	std::size_t selectedIndex = doors.size();
	for (std::size_t index = 0; index < doors.size(); ++index)
		if (doors[index].baseGrid == selectedDoorBaseGrid_)
		{
			selectedIndex = index;
			break;
		}
	for (std::size_t offset = 1; offset <= doors.size(); ++offset)
	{
		const std::size_t index = forward
			? (selectedIndex == doors.size()
				? offset - 1
				: (selectedIndex + offset) % doors.size())
			: (selectedIndex == doors.size()
				? doors.size() - offset
				: (selectedIndex + doors.size() -
					(offset % doors.size())) % doors.size());
		if (!doorCandidate(view, doors[index])) continue;
		selectedDoorBaseGrid_ = doors[index].baseGrid;
		selectedDoorStructureId_ = doors[index].structureId;
		selectedDoorOpen_ = doors[index].open;
		return true;
	}
	return false;
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::request(
	const FullEngineCoopClientControllerView& view,
	const CoopSession::TacticalIntentPayload& payload) const noexcept
{
	if (!actionsEnabled(view)) return {};
	FullEngineCoopClientIntentRequest output;
	output.actor = selectedActor_;
	output.payload = payload;
	output.valid = true;
	return output;
}
