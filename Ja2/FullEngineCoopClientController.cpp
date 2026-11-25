#include "FullEngineCoopClientController.h"
#include "FullEngineCoopClientPresentationLoot.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <limits>

namespace
{
bool SupportedInventorySwapSlot(std::uint16_t slot) noexcept
{
	// Ordinary armor, face equipment, hands and pockets. Native placement and
	// projected face compatibility remain authoritative; worn LBE stays opaque.
	return slot <= 6 || (slot >= 14 && slot < 55);
}

bool SupportedInventoryTransferSlot(std::uint16_t slot) noexcept
{
	// Giving worn equipment is a separate native flow, not an implicit unequip.
	return slot == 5 || slot == 6 || (slot >= 14 && slot < 55);
}

bool FirstAidIdlePose(const TacticalActorSnapshot& actor) noexcept
{
	if ((actor.presentation.flags & TacticalActorRenderPosePresent) == 0) return false;
	// Settled idle or stationary aim pose, matching the native first-aid gate.
	switch (actor.stance)
	{
		case TacticalStance::Standing:
			return actor.animation == TacticalMedicalIdleStandingAnimation ||
				actor.animation == TacticalMedicalAimRifleStandingAnimation ||
				actor.animation == TacticalMedicalAimDualStandingAnimation;
		case TacticalStance::Crouched:
			return actor.animation == TacticalMedicalIdleCrouchedAnimation ||
				actor.animation == TacticalMedicalAimRifleCrouchedAnimation ||
				actor.animation == TacticalMedicalAimDualCrouchedAnimation;
		case TacticalStance::Prone:
			return actor.animation == TacticalMedicalIdleProneAnimation ||
				actor.animation == TacticalMedicalAimRifleProneAnimation ||
				actor.animation == TacticalMedicalAimDualProneAnimation;
		default:
			return false;
	}
}

bool ValidView(const FullEngineCoopClientControllerView& view) noexcept
{
	return view.snapshot != nullptr &&
		view.snapshot->sector().loaded &&
		view.assignedActorCount <=
			CoopSession::MaximumCoopTacticalAssignedActors &&
		(view.assignedActorCount == 0 || view.assignedActors != nullptr);
}

bool HasMovementActionPoints(
	const FullEngineCoopClientControllerView& view,
	TacticalEntityId actorId) noexcept
{
	if (view.snapshot == nullptr) return false;
	const TacticalTurnSnapshot& turn = view.snapshot->turn();
	if (!turn.turnBased || !turn.inCombat) return true;
	const TacticalActorSnapshot* const actor = view.snapshot->find(actorId);
	return actor != nullptr && actor->actionPoints > 0;
}
}

const CoopSession::CoopOwnerInventorySnapshot*
FullEngineCoopClientControllerView::inventoryFor(TacticalEntityId actor) const noexcept
{
	if (!ValidView(*this) || resynchronizing || !ownerInventories) return nullptr;
	for (std::size_t index = 0; index < assignedActorCount; ++index)
		if (assignedActors[index] == actor)
		{
			const auto* inventory = ownerInventories[index];
			return inventory && inventory->actor == actor &&
				inventory->worldGeneration == snapshot->epoch() &&
				CoopSession::IsValidCoopOwnerInventorySnapshot(*inventory) ? inventory : nullptr;
		}
	return nullptr;
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
		cancelFirstAidSelection();
		closeInventory();
		return;
	}

	if (selectingFirstAid_ && !exactFirstAidSelectionCurrent(view))
		cancelFirstAidSelection();

	if (inventoryOpen_)
	{
		if (droppingInventory() && !inventoryDropContextCurrent(view))
		{
			cancelInventoryDrop();
			inventoryNotice_ = "Drop cancelled: inventory, ownership, actor pose or readiness changed";
		}
		if (pickingUpInventory() && !inventoryPickupContextCurrent(view))
		{
			cancelInventoryPickup();
			inventoryNotice_ = "Pickup cancelled: inventory, ground view, ownership or actor position changed";
		}
		if (transferringInventory() && !inventoryTransferCurrent(view))
		{
			cancelInventoryTransfer();
			inventoryNotice_ = "Give cancelled: inventory, ownership or actor state changed";
		}
		const auto* inventory = view.inventoryFor(selectedActor_);
		if (!inventory || inventoryActor_ != selectedActor_ ||
			inventoryOwner_ != inventory->owner ||
			inventoryWorldEpoch_ != view.snapshot->epoch() ||
			inventorySessionEpoch_ != inventory->sessionEpoch ||
			inventoryBaselineId_ != inventory->baselineId) closeInventory();
		else
		{
			if (!actionsEnabled(view) || inventorySourceRevision_ != inventory->inventoryRevision)
			{
				inventorySourceSlot_ = 0xffff;
				inventorySourceRevision_ = 0;
			}
			if (inventoryInspectedRevision_ != inventory->inventoryRevision ||
				inventoryInspectedSlot_ >= inventory->slots.size() ||
				inventory->slots[inventoryInspectedSlot_].item == 0)
			{
				inventoryInspectedSlot_ = 0xffff;
				inventoryInspectedRevision_ = 0;
			}
		}
	}

	if (assignedAndPresent(view, selectedActor_))
	{
		if (!actionsEnabled(view))
		{
			cancelDestinationEntry();
			cancelAttackTargeting();
			cancelDoorSelection();
			cancelFirstAidSelection();
		}
		else
		{
			if (enteringDestination_ &&
				view.snapshot->epoch() != destinationWorldEpoch_)
				cancelDestinationEntry();
			if (targetingAttack_ &&
				view.snapshot->epoch() != attackWorldEpoch_)
				cancelAttackTargeting();
			else if (targetingAttack_ &&
				!attackTargetCandidate(view, attackTarget_))
			{
				attackTarget_ = TacticalEntityId{};
				if (!selectTargetRelative(view, true)) cancelAttackTargeting();
			}
			if (selectingDoor_ && !exactDoorSelectionCurrent(view))
				cancelDoorSelection();
		}
		return;
	}

	selectedActor_ = TacticalEntityId{};
	closeInventory();
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
	cancelFirstAidSelection();
}

bool FullEngineCoopClientController::ready(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	return ValidView(view) && selectedActor_.valid() &&
		assignedAndPresent(view, selectedActor_);
}

bool FullEngineCoopClientController::nearbyLootHitCurrent(
	const FullEngineCoopClientControllerView& view,
	const FullEngineCoopClientPresentationLootHit& hit) const noexcept
{
	const auto* owner = view.inventoryFor(selectedActor_);
	if (!ready(view) || view.resynchronizing || !owner ||
		!IsFullEngineCoopClientPresentationLootContextCurrent(hit.context,
			*view.snapshot, *owner, view.assignedActors, view.assignedActorCount,
			selectedActor_)) return false;
	for (const auto& marker : owner->nearbyLoot)
		if (marker.grid == hit.grid && marker.level == hit.level &&
			marker.hasMedicalKit == hit.hasMedicalKit) return true;
	return false;
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

bool FullEngineCoopClientController::activeModalWorldMatches(
	std::uint64_t worldEpoch) const noexcept
{
	return worldEpoch != 0 &&
		(!enteringDestination_ || destinationWorldEpoch_ == worldEpoch) &&
		(!targetingAttack_ || attackWorldEpoch_ == worldEpoch) &&
		(!selectingDoor_ || doorWorldEpoch_ == worldEpoch) &&
		(!selectingFirstAid_ || firstAidWorldEpoch_ == worldEpoch) &&
		(!inventoryOpen_ || inventoryWorldEpoch_ == worldEpoch);
}

void FullEngineCoopClientController::cancelModalWorldMismatches(
	std::uint64_t worldEpoch) noexcept
{
	if (enteringDestination_ && destinationWorldEpoch_ != worldEpoch)
		cancelDestinationEntry();
	if (targetingAttack_ && attackWorldEpoch_ != worldEpoch)
		cancelAttackTargeting();
	if (selectingDoor_ && doorWorldEpoch_ != worldEpoch)
		cancelDoorSelection();
	if (selectingFirstAid_ && firstAidWorldEpoch_ != worldEpoch) cancelFirstAidSelection();
	if (inventoryOpen_ && inventoryWorldEpoch_ != worldEpoch) closeInventory();
}

bool FullEngineCoopClientController::selectActor(
	const FullEngineCoopClientControllerView& view,
	TacticalEntityId actor) noexcept
{
	if (!assignedAndPresent(view, actor)) return false;
	cancelModalWorldMismatches(view.snapshot->epoch());
	const bool changed = actor != selectedActor_;
	selectedActor_ = actor;
	if (changed)
	{
		closeInventory();
		cancelDestinationEntry();
		cancelAttackTargeting();
		cancelDoorSelection();
		cancelFirstAidSelection();
	}
	return true;
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
		cancelModalWorldMismatches(view.snapshot->epoch());
		const bool changed = candidate != selectedActor_;
		selectedActor_ = candidate;
		if (changed)
		{
			closeInventory();
			cancelDestinationEntry();
			cancelAttackTargeting();
			cancelDoorSelection();
			cancelFirstAidSelection();
		}
		return true;
	}
	return false;
}

bool FullEngineCoopClientController::beginDestinationEntry(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!actionsEnabled(view)) return false;
	closeInventory();
	cancelAttackTargeting();
	cancelDoorSelection();
	cancelFirstAidSelection();
	destination_[0] = '\0';
	destinationWorldEpoch_ = view.snapshot->epoch();
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
	destinationWorldEpoch_ = 0;
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
	closeInventory();
	cancelDestinationEntry();
	cancelDoorSelection();
	cancelFirstAidSelection();
	targetingAttack_ = true;
	attackTarget_ = TacticalEntityId{};
	attackWorldEpoch_ = view.snapshot->epoch();
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

bool FullEngineCoopClientController::selectAttackTarget(
	const FullEngineCoopClientControllerView& view,
	TacticalEntityId target) noexcept
{
	if (!targetingAttack_ || !attackTargetCandidate(view, target))
		return false;
	attackTarget_ = target;
	return true;
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
	attackWorldEpoch_ = 0;
	attackAimTime_ = 1;
	targetingAttack_ = false;
}

bool FullEngineCoopClientController::beginDoorSelection(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!actionsEnabled(view)) return false;
	closeInventory();
	cancelDestinationEntry();
	cancelAttackTargeting();
	cancelDoorSelection();
	cancelFirstAidSelection();
	selectingDoor_ = true;
	doorActor_ = selectedActor_;
	doorWorldEpoch_ = view.snapshot->epoch();
	if (selectDoorRelative(view, true)) return true;
	cancelDoorSelection();
	cancelFirstAidSelection();
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

bool FullEngineCoopClientController::selectDoor(
	const FullEngineCoopClientControllerView& view,
	std::int32_t baseGrid) noexcept
{
	if (!selectingDoor_ || !ValidView(view) ||
		doorActor_ != selectedActor_ ||
		view.snapshot->epoch() != doorWorldEpoch_)
		return false;
	const TacticalDoorSnapshot* const door = view.snapshot->findDoor(baseGrid);
	if (door == nullptr || !doorCandidate(view, *door)) return false;

	selectedDoorBaseGrid_ = door->baseGrid;
	selectedDoorStructureId_ = door->structureId;
	selectedDoorOpen_ = door->open;
	return true;
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
	if (!enteringDestination_ || destinationLength_ == 0)
		return {};
	if (!ValidView(view) ||
		view.snapshot->epoch() != destinationWorldEpoch_)
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
	const FullEngineCoopClientIntentRequest move = moveToGrid(view,
		static_cast<std::int32_t>(destination), movementMode, reverse_);
	if (!move) return {};
	cancelDestinationEntry();
	return move;
}

FullEngineCoopClientIntentRequest
FullEngineCoopClientController::moveToGrid(
	const FullEngineCoopClientControllerView& view,
	std::int32_t grid,
	std::uint16_t movementMode,
	bool reverse) const noexcept
{
	if (!actionsEnabled(view) ||
		!HasMovementActionPoints(view, selectedActor_) ||
		view.snapshot == nullptr ||
		!view.snapshot->dimensions().contains(grid))
		return {};
	return request(view, CoopSession::MoveTacticalIntent{
		grid, movementMode, reverse});
}

FullEngineCoopClientIntentRequest
FullEngineCoopClientController::submitRelativeMove(
	const FullEngineCoopClientControllerView& view,
	int deltaRow, int deltaColumn,
	std::uint16_t movementMode) const noexcept
{
	if (!actionsEnabled(view) ||
		!HasMovementActionPoints(view, selectedActor_) ||
		deltaRow < -1 || deltaRow > 1 ||
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
FullEngineCoopClientController::beginFirstAid(
	const FullEngineCoopClientControllerView& view, TacticalEntityId target) const noexcept
{
	if (!actionsEnabled(view) || targetingAttack_ || enteringDestination_ || selectingDoor_ ||
		target == selectedActor_ || !view.snapshot->dimensions().valid()) return {};
	const TacticalActorSnapshot* const medic = view.snapshot->find(selectedActor_);
	const TacticalActorSnapshot* const patient = view.snapshot->find(target);
	// The stock first-aid animation bank supports the four human merc bodies.
	if (medic == nullptr || patient == nullptr || medic->presentation.bodyType > 3 ||
		!medic->medicalStateValid() || !patient->medicalStateValid() ||
		medic->medicalService.providing || medic->medicalService.receivingCount != 0 ||
		patient->medicalService.providing || patient->medicalService.receivingCount != 0 ||
		medic->handMedicalKitPoints == 0 ||
		medic->life < 15 || !patient->active || !patient->inSector || patient->life <= 0 ||
		patient->hostileToPlayerTeam || patient->team != medic->team ||
		patient->level != medic->level || (patient->bleeding == 0 && patient->life >= 15) ||
		!view.snapshot->dimensions().contains(medic->grid) ||
		!view.snapshot->dimensions().contains(patient->grid)) return {};
	const std::int32_t columns = view.snapshot->dimensions().columns;
	const std::int32_t row = medic->grid / columns - patient->grid / columns;
	const std::int32_t column = medic->grid % columns - patient->grid % columns;
	// Native FindAdjacentGridEx uses cardinal action tiles for first aid.
	if (!((row == 0 && (column == -1 || column == 1)) ||
		(column == 0 && (row == -1 || row == 1))))
		return {};
	// This is only a public presentation filter. Authority repeats native
	// adjacency/obstruction, skill, service, kit and exact-identity validation.
	return request(view, CoopSession::BeginFirstAidTacticalIntent{target});
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
	cancelFirstAidSelection();
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
	if (!target.valid() || target == selectedActor_ || !ValidView(view) ||
		!targetingAttack_ || view.snapshot->epoch() != attackWorldEpoch_)
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
	if (!actionsEnabled(view) || inventoryOpen_) return {};
	FullEngineCoopClientIntentRequest output;
	output.actor = selectedActor_;
	output.payload = payload;
	output.valid = true;
	return output;
}

bool FullEngineCoopClientController::firstAidCandidate(
	const FullEngineCoopClientControllerView& view, TacticalEntityId patientId) const noexcept
{
	if (!beginFirstAid(view, patientId)) return false;
	const auto* medic = view.snapshot->find(selectedActor_);
	const auto* patient = view.snapshot->find(patientId);
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (!medic || !patient || !FirstAidIdlePose(*medic) || !FirstAidIdlePose(*patient) ||
		!inventory || inventory->slots.size() <= 5) return false;
	const auto& kit = inventory->slots[5];
	return kit.support == CoopSession::CoopInventorySlotSupport::OrdinarySwappable &&
		kit.statusKind == CoopSession::CoopInventoryStatusKind::MedicalKitPoints &&
		kit.item == medic->loadout.primaryHand.item && kit.count == medic->loadout.primaryHand.quantity &&
		kit.resourceTotal == medic->handMedicalKitPoints && kit.resourceTotal != 0;
}

bool FullEngineCoopClientController::exactFirstAidSelectionCurrent(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	const auto* inventory = view.inventoryFor(selectedActor_);
	return selectingFirstAid_ && firstAidMedic_ == selectedActor_ && inventory &&
		firstAidOwner_ == inventory->owner && firstAidSessionEpoch_ == inventory->sessionEpoch &&
		firstAidBaselineId_ == inventory->baselineId && firstAidWorldEpoch_ == inventory->worldGeneration &&
		firstAidInventoryRevision_ == inventory->inventoryRevision &&
		firstAidCandidate(view, firstAidPatient_);
}

bool FullEngineCoopClientController::beginFirstAidSelection(
	const FullEngineCoopClientControllerView& view) noexcept
{
	synchronize(view);
	if (!actionsEnabled(view) || targetingAttack_ || enteringDestination_ || selectingDoor_ || inventoryOpen_)
		return false;
	cancelFirstAidSelection();
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (!inventory) return false;
	firstAidMedic_ = selectedActor_;
	firstAidOwner_ = inventory->owner;
	firstAidSessionEpoch_ = inventory->sessionEpoch;
	firstAidBaselineId_ = inventory->baselineId;
	firstAidWorldEpoch_ = inventory->worldGeneration;
	firstAidInventoryRevision_ = inventory->inventoryRevision;
	selectingFirstAid_ = true;
	if (selectPatientRelative(view, true)) return true;
	cancelFirstAidSelection();
	return false;
}

bool FullEngineCoopClientController::selectPatientRelative(
	const FullEngineCoopClientControllerView& view, bool forward) noexcept
{
	if (!selectingFirstAid_ || !actionsEnabled(view)) return false;
	const auto& actors = view.snapshot->actors();
	std::size_t selectedIndex = actors.size();
	for (std::size_t index = 0; index < actors.size(); ++index)
		if (actors[index].id == firstAidPatient_) { selectedIndex = index; break; }
	for (std::size_t offset = 1; offset <= actors.size(); ++offset)
	{
		const std::size_t index = forward
			? (selectedIndex == actors.size() ? offset - 1 : (selectedIndex + offset) % actors.size())
			: (selectedIndex == actors.size() ? actors.size() - offset :
				(selectedIndex + actors.size() - offset % actors.size()) % actors.size());
		if (!firstAidCandidate(view, actors[index].id)) continue;
		firstAidPatient_ = actors[index].id;
		return true;
	}
	return false;
}

bool FullEngineCoopClientController::selectNextPatient(
	const FullEngineCoopClientControllerView& view) noexcept
{
	synchronize(view);
	return selectingFirstAid_ && selectPatientRelative(view, true);
}

bool FullEngineCoopClientController::selectPreviousPatient(
	const FullEngineCoopClientControllerView& view) noexcept
{
	synchronize(view);
	return selectingFirstAid_ && selectPatientRelative(view, false);
}

void FullEngineCoopClientController::cancelFirstAidSelection() noexcept
{
	selectingFirstAid_ = false;
	firstAidMedic_ = {};
	firstAidPatient_ = {};
	firstAidOwner_ = {};
	firstAidSessionEpoch_ = firstAidBaselineId_ = firstAidWorldEpoch_ = firstAidInventoryRevision_ = 0;
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::submitFirstAid(
	const FullEngineCoopClientControllerView& view) noexcept
{
	synchronize(view);
	if (!exactFirstAidSelectionCurrent(view)) return {};
	const auto patient = firstAidPatient_;
	cancelFirstAidSelection();
	// The transport stamps the current public revision when this is sent.
	return beginFirstAid(view, patient);
}

bool FullEngineCoopClientController::openInventory(
	const FullEngineCoopClientControllerView& view) noexcept
{
	synchronize(view);
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (!ready(view) || view.resynchronizing || !inventory) return false;
	cancelDestinationEntry();
	cancelAttackTargeting();
	cancelDoorSelection();
	cancelFirstAidSelection();
	closeInventory();
	inventoryOpen_ = true;
	inventoryActor_ = selectedActor_;
	inventoryOwner_ = inventory->owner;
	inventoryWorldEpoch_ = view.snapshot->epoch();
	inventorySessionEpoch_ = inventory->sessionEpoch;
	inventoryBaselineId_ = inventory->baselineId;
	return true;
}

void FullEngineCoopClientController::closeInventory() noexcept
{
	cancelInventoryDrop();
	cancelInventoryPickup();
	cancelInventoryTransfer();
	inventoryNotice_ = "";
	inventoryOpen_ = false;
	inventoryActor_ = {};
	inventoryOwner_ = {};
	inventorySessionEpoch_ = 0;
	inventoryBaselineId_ = 0;
	inventoryWorldEpoch_ = 0;
	inventorySourceSlot_ = 0xffff;
	inventorySourceRevision_ = 0;
	inventoryInspectedSlot_ = 0xffff;
	inventoryInspectedRevision_ = 0;
}

bool FullEngineCoopClientController::inspectInventorySlot(
	const FullEngineCoopClientControllerView& view, std::uint16_t slot) noexcept
{
	synchronize(view);
	if (transferringInventory() || pickingUpInventory() || droppingInventory()) return false;
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (!inventoryOpen_ || inventoryActor_ != selectedActor_ || !ready(view) ||
		!inventory || inventoryWorldEpoch_ != view.snapshot->epoch() ||
		inventorySessionEpoch_ != inventory->sessionEpoch || inventoryBaselineId_ != inventory->baselineId ||
		slot >= 55 || slot >= inventory->slots.size() ||
		inventory->slots[slot].item == 0 || inventory->slots[slot].count == 0) return false;
	inventoryInspectedSlot_ = slot;
	inventoryInspectedRevision_ = inventory->inventoryRevision;
	return true;
}

bool FullEngineCoopClientController::selectInventorySource(
	const FullEngineCoopClientControllerView& view, std::uint16_t slot) noexcept
{
	synchronize(view);
	if (transferringInventory() || pickingUpInventory() || droppingInventory()) return false;
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (!inventoryOpen_ || inventoryActor_ != selectedActor_ ||
		inventoryWorldEpoch_ != (view.snapshot ? view.snapshot->epoch() : 0) ||
		!actionsEnabled(view) || !inventory ||
		inventorySessionEpoch_ != inventory->sessionEpoch || inventoryBaselineId_ != inventory->baselineId ||
		!SupportedInventorySwapSlot(slot) || slot >= inventory->slots.size()) return false;
	const auto& item = inventory->slots[slot];
	if (item.slot != slot || item.support != CoopSession::CoopInventorySlotSupport::OrdinarySwappable ||
		item.item == 0 || item.count == 0) return false;
	inventoryNotice_ = "";
	inventoryInspectedSlot_ = slot;
	inventoryInspectedRevision_ = inventory->inventoryRevision;
	if (inventorySourceSlot_ == slot && inventorySourceRevision_ == inventory->inventoryRevision)
	{
		inventorySourceSlot_ = 0xffff;
		inventorySourceRevision_ = 0;
	}
	else
	{
		inventorySourceSlot_ = slot;
		inventorySourceRevision_ = inventory->inventoryRevision;
	}
	return true;
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::inventorySwap(
	const FullEngineCoopClientControllerView& view, std::uint16_t destinationSlot) noexcept
{
	synchronize(view);
	if (transferringInventory() || pickingUpInventory() || droppingInventory()) return {};
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (!inventoryOpen_ || inventoryActor_ != selectedActor_ ||
		inventoryWorldEpoch_ != (view.snapshot ? view.snapshot->epoch() : 0) ||
		!actionsEnabled(view) || !inventory || inventorySourceRevision_ != inventory->inventoryRevision ||
		inventorySessionEpoch_ != inventory->sessionEpoch || inventoryBaselineId_ != inventory->baselineId ||
		!SupportedInventorySwapSlot(inventorySourceSlot_) || !SupportedInventorySwapSlot(destinationSlot) ||
		inventorySourceSlot_ >= inventory->slots.size() || destinationSlot >= inventory->slots.size() ||
		inventorySourceSlot_ == destinationSlot) return {};
	const auto& source = inventory->slots[inventorySourceSlot_];
	const auto& destination = inventory->slots[destinationSlot];
	if (source.slot != inventorySourceSlot_ || destination.slot != destinationSlot ||
		source.support != CoopSession::CoopInventorySlotSupport::OrdinarySwappable ||
		source.item == 0 || source.count == 0 ||
		destination.support == CoopSession::CoopInventorySlotSupport::UnsupportedComplex) return {};
	FullEngineCoopClientIntentRequest result{selectedActor_,
		CoopSession::SwapInventorySlotsTacticalIntent{
			inventorySourceSlot_, destinationSlot, inventory->inventoryRevision}, true};
	// No cursor item is removed locally. Only the server's replacement can
	// change either slot; a lost or rejected command is never retried implicitly.
	inventorySourceSlot_ = 0xffff;
	inventorySourceRevision_ = 0;
	return result;
}

bool FullEngineCoopClientController::inventorySourceCurrent(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	const auto* source = view.inventoryFor(selectedActor_);
	return inventoryOpen_ && !pickingUpInventory() && inventoryActor_ == selectedActor_ && actionsEnabled(view) && source &&
		inventoryWorldEpoch_ == view.snapshot->epoch() &&
		inventorySessionEpoch_ == source->sessionEpoch && inventoryBaselineId_ == source->baselineId &&
		inventorySourceRevision_ == source->inventoryRevision &&
		SupportedInventorySwapSlot(inventorySourceSlot_) && inventorySourceSlot_ < source->slots.size() &&
		source->slots[inventorySourceSlot_].support == CoopSession::CoopInventorySlotSupport::OrdinarySwappable &&
		source->slots[inventorySourceSlot_].item != 0 && source->slots[inventorySourceSlot_].count != 0;
}

bool FullEngineCoopClientController::inventoryRecipientCandidate(
	const FullEngineCoopClientControllerView& view, TacticalEntityId target) const noexcept
{
	if (droppingInventory() || !inventorySourceCurrent(view) ||
		!SupportedInventoryTransferSlot(inventorySourceSlot_) ||
		target == selectedActor_ || !assignedAndPresent(view, target)) return false;
	const auto* sourceInventory = view.inventoryFor(selectedActor_);
	const auto* targetInventory = view.inventoryFor(target);
	const auto* source = view.snapshot->find(selectedActor_);
	const auto* recipient = view.snapshot->find(target);
	if (!targetInventory || targetInventory->owner != sourceInventory->owner ||
		targetInventory->sessionEpoch != inventorySessionEpoch_ ||
		targetInventory->baselineId != inventoryBaselineId_ || !source || !recipient ||
		source->team != 0 || recipient->team != source->team || !recipient->active || !recipient->inSector ||
		source->life < 15 || recipient->life < 15 || source->level != recipient->level ||
		source->presentation.bodyType > 3 || recipient->presentation.bodyType > 3 ||
		source->medicalService.providing || source->medicalService.receivingCount ||
		recipient->medicalService.providing || recipient->medicalService.receivingCount ||
		!view.snapshot->dimensions().contains(source->grid) ||
		!view.snapshot->dimensions().contains(recipient->grid)) return false;
	const auto& turn = view.snapshot->turn();
	if (turn.interruptPhase == TacticalInterruptPhase::Resolving) return false;
	// During an active interrupt both actors spend AP, so the authority requires
	// each to be interrupt-eligible, exactly as for any other interrupt action.
	if (turn.interruptPhase == TacticalInterruptPhase::Active &&
		(!source->interruptActionEligible || !recipient->interruptActionEligible)) return false;
	// This is only a public-state preflight. The authority checks native LOS,
	// exact AP costs, animation readiness, pocket capacity and equipment rules.
	if (turn.turnBased && turn.inCombat && (source->actionPoints < 3 || recipient->actionPoints < 3)) return false;
	const std::int64_t columns = view.snapshot->dimensions().columns;
	const auto row = source->grid / columns - recipient->grid / columns;
	const auto column = source->grid % columns - recipient->grid % columns;
	return row * row + column * column <= 9;
}

bool FullEngineCoopClientController::inventoryTransferCurrent(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (!transferringInventory() || !inventoryRecipientCandidate(view, inventoryRecipient_) ||
		view.assignedActorCount != inventoryTransferAssignmentCount_) return false;
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		if (view.assignedActors[index] != inventoryTransferAssignments_[index]) return false;
	const auto* recipient = view.inventoryFor(inventoryRecipient_);
	return recipient && recipient->owner == inventoryTransferOwner_ &&
		recipient->inventoryRevision == inventoryRecipientRevision_;
}

bool FullEngineCoopClientController::selectInventoryRecipient(
	const FullEngineCoopClientControllerView& view, TacticalEntityId target) noexcept
{
	if ((transferringInventory() && !inventoryTransferCurrent(view)) ||
		!inventoryRecipientCandidate(view, target)) return false;
	inventoryRecipient_ = target;
	inventoryRecipientRevision_ = view.inventoryFor(target)->inventoryRevision;
	inventoryTransferOwner_ = view.inventoryFor(target)->owner;
	inventoryTransferAssignmentCount_ = view.assignedActorCount;
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		inventoryTransferAssignments_[index] = view.assignedActors[index];
	inventoryInspectedSlot_ = 0xffff;
	inventoryInspectedRevision_ = 0;
	inventoryNotice_ = "";
	return true;
}

bool FullEngineCoopClientController::beginInventoryTransfer(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!inventorySourceCurrent(view) || !SupportedInventoryTransferSlot(inventorySourceSlot_))
	{
		inventoryNotice_ = "Give: first select an ordinary item in a hand or pocket";
		return false;
	}
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		if (selectInventoryRecipient(view, view.assignedActors[index])) return true;
	inventoryNotice_ = "Give: no ready owned merc within 3 tiles (both need 3 AP in combat)";
	return false;
}

bool FullEngineCoopClientController::selectNextInventoryRecipient(
	const FullEngineCoopClientControllerView& view, bool forward) noexcept
{
	if (!inventoryTransferCurrent(view)) return false;
	std::size_t current = 0;
	while (current < view.assignedActorCount && view.assignedActors[current] != inventoryRecipient_) ++current;
	for (std::size_t offset = 1; offset <= view.assignedActorCount; ++offset)
	{
		const auto index = (current + (forward ? offset : view.assignedActorCount - offset)) % view.assignedActorCount;
		if (selectInventoryRecipient(view, view.assignedActors[index])) return true;
	}
	return false;
}

void FullEngineCoopClientController::cancelInventoryTransfer() noexcept
{
	inventoryRecipient_ = {};
	inventoryRecipientRevision_ = 0;
	inventoryTransferOwner_ = {};
	inventoryTransferAssignmentCount_ = 0;
	inventoryTransferAssignments_ = {};
	inventorySourceSlot_ = 0xffff;
	inventorySourceRevision_ = 0;
	inventoryInspectedSlot_ = 0xffff;
	inventoryInspectedRevision_ = 0;
	inventoryNotice_ = "Give cancelled; no items moved";
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::inventoryTransfer(
	const FullEngineCoopClientControllerView& view, TacticalEntityId target,
	std::uint16_t destinationSlot) noexcept
{
	if (!inventoryTransferCurrent(view) || target != inventoryRecipient_) return {};
	const auto* destination = view.inventoryFor(target);
	if (!SupportedInventoryTransferSlot(destinationSlot) || destinationSlot >= destination->slots.size() ||
		destination->slots[destinationSlot].support != CoopSession::CoopInventorySlotSupport::Empty)
	{
		inventoryNotice_ = "Give: choose an empty hand or pocket; no exchange, merge or split";
		return {};
	}
	FullEngineCoopClientIntentRequest result{selectedActor_,
		CoopSession::TransferInventoryItemTacticalIntent{target, inventorySourceSlot_, destinationSlot,
			inventorySourceRevision_, inventoryRecipientRevision_}, true};
	// Only the typed request leaves here. Both inventories and both AP values
	// remain entirely unchanged until their authority replacements arrive.
	cancelInventoryTransfer();
	inventoryNotice_ = "";
	return result;
}

bool FullEngineCoopClientController::inventoryDropCandidate(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (transferringInventory() || pickingUpInventory() || !inventorySourceCurrent(view) ||
		inventorySourceSlot_ < 14 || inventorySourceSlot_ >= 55) return false;
	const auto* actor = view.snapshot->find(selectedActor_);
	const auto& turn = view.snapshot->turn();
	// This is a public-state preflight only. Neither an empty owner ground list
	// nor local map geometry proves the native drop tile is empty or safe.
	return actor && actor->life >= 15 && actor->level == 0 && actor->presentation.bodyType <= 3 &&
		actor->stance != TacticalStance::Unknown && view.snapshot->dimensions().contains(actor->grid) &&
		!actor->medicalService.providing && !actor->medicalService.receivingCount &&
		turn.interruptPhase != TacticalInterruptPhase::Resolving &&
		(turn.interruptPhase != TacticalInterruptPhase::Active || actor->interruptActionEligible) &&
		(!turn.turnBased || !turn.inCombat || actor->actionPoints > 0);
}

bool FullEngineCoopClientController::inventoryDropContextCurrent(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (!droppingInventory() || !inventoryDropCandidate(view) ||
		view.assignedActorCount != inventoryDropAssignmentCount_) return false;
	for (std::size_t i = 0; i < view.assignedActorCount; ++i)
		if (view.assignedActors[i] != inventoryDropAssignments_[i]) return false;
	const auto* actor = view.snapshot->find(selectedActor_);
	return view.inventoryFor(selectedActor_)->owner == inventoryDropOwner_ &&
		actor->grid == inventoryDropGrid_ && actor->level == inventoryDropLevel_ &&
		actor->direction == inventoryDropDirection_ && actor->animation == inventoryDropAnimation_ &&
		static_cast<std::uint8_t>(actor->stance) == inventoryDropStance_ &&
		actor->presentation.bodyType == inventoryDropBodyType_ &&
		actor->presentation.worldXQ8 == inventoryDropWorldXQ8_ &&
		actor->presentation.worldYQ8 == inventoryDropWorldYQ8_ &&
		actor->presentation.heightAdjustment == inventoryDropHeightAdjustment_ &&
		actor->presentation.flags == inventoryDropPresentationFlags_;
}

bool FullEngineCoopClientController::beginInventoryDrop(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (droppingInventory()) return false;
	if (!inventoryDropCandidate(view))
	{
		inventoryNotice_ = "Select an ordinary pocket stack (14-54) to drop; actor must be ready on ground level";
		return false;
	}
	const auto* actor = view.snapshot->find(selectedActor_);
	inventoryDropOpen_ = true;
	inventoryDropOwner_ = view.inventoryFor(selectedActor_)->owner;
	inventoryDropGrid_ = actor->grid;
	inventoryDropLevel_ = actor->level;
	inventoryDropDirection_ = actor->direction;
	inventoryDropAnimation_ = actor->animation;
	inventoryDropStance_ = static_cast<std::uint8_t>(actor->stance);
	inventoryDropBodyType_ = actor->presentation.bodyType;
	inventoryDropWorldXQ8_ = actor->presentation.worldXQ8;
	inventoryDropWorldYQ8_ = actor->presentation.worldYQ8;
	inventoryDropHeightAdjustment_ = actor->presentation.heightAdjustment;
	inventoryDropPresentationFlags_ = actor->presentation.flags;
	inventoryDropAssignmentCount_ = view.assignedActorCount;
	for (std::size_t i = 0; i < view.assignedActorCount; ++i) inventoryDropAssignments_[i] = view.assignedActors[i];
	inventoryNotice_ = "";
	return true;
}

void FullEngineCoopClientController::cancelInventoryDrop() noexcept
{
	if (inventoryDropOpen_)
	{
		inventorySourceSlot_ = inventoryInspectedSlot_ = 0xffff;
		inventorySourceRevision_ = inventoryInspectedRevision_ = 0;
	}
	inventoryDropOpen_ = false;
	inventoryDropOwner_ = {};
	inventoryDropGrid_ = -1;
	inventoryDropLevel_ = -1;
	inventoryDropDirection_ = inventoryDropStance_ = inventoryDropBodyType_ = 0;
	inventoryDropAnimation_ = 0;
	inventoryDropHeightAdjustment_ = 0;
	inventoryDropWorldXQ8_ = inventoryDropWorldYQ8_ = 0;
	inventoryDropPresentationFlags_ = 0;
	inventoryDropAssignments_.fill({});
	inventoryDropAssignmentCount_ = 0;
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::inventoryDrop(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (!inventoryDropContextCurrent(view))
	{
		if (droppingInventory())
		{
			cancelInventoryDrop();
			inventoryNotice_ = "Drop cancelled: inventory, ownership, actor pose or readiness changed";
		}
		return {};
	}
	const FullEngineCoopClientIntentRequest result{selectedActor_,
		CoopSession::DropInventoryItemTacticalIntent{inventorySourceSlot_, inventorySourceRevision_}, true};
	// Confirmation spends only this local choice. The authority alone moves
	// the complete object, charges AP, and publishes the new exact ground ID.
	cancelInventoryDrop();
	inventoryNotice_ = "";
	return result;
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::clickInventorySlot(
	const FullEngineCoopClientControllerView& view, std::uint16_t slot) noexcept
{
	if (droppingInventory()) return {};
	if (transferringInventory()) return inventoryTransfer(view, inventoryRecipient_, slot);
	if (pickingUpInventory()) return inventoryPickup(view, slot);
	(void)inspectInventorySlot(view, slot);
	if (inventorySourceSlot_ == 0xffff || inventorySourceSlot_ == slot)
	{
		(void)selectInventorySource(view, slot);
		return {};
	}
	return inventorySwap(view, slot);
}

bool FullEngineCoopClientController::inventoryGroundViewCurrent(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (!inventoryOpen_ || inventoryActor_ != selectedActor_ || !ready(view) || !inventory ||
		inventoryWorldEpoch_ != view.snapshot->epoch() ||
		inventorySessionEpoch_ != inventory->sessionEpoch || inventoryBaselineId_ != inventory->baselineId)
		return false;
	const auto* actor = view.snapshot->find(selectedActor_);
	return actor && actor->active && actor->inSector &&
		view.snapshot->dimensions().contains(inventory->groundGrid) &&
		actor->grid == inventory->groundGrid && actor->level == inventory->groundLevel;
}

bool FullEngineCoopClientController::inventoryPickupContextCurrent(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (!pickingUpInventory() || !inventoryGroundViewCurrent(view) ||
		view.assignedActorCount != inventoryPickupAssignmentCount_) return false;
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		if (view.assignedActors[index] != inventoryPickupAssignments_[index]) return false;
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (inventory->inventoryRevision != inventoryPickupRevision_ || inventory->owner != inventoryPickupOwner_ ||
		inventory->groundGrid != inventoryPickupGrid_ || inventory->groundLevel != inventoryPickupLevel_) return false;
	if (!inventoryGroundItem_.valid()) return inventory->groundItems.empty();
	for (const auto& entry : inventory->groundItems)
		if (entry.id == inventoryGroundItem_) return true;
	return false;
}

bool FullEngineCoopClientController::beginInventoryPickup(
	const FullEngineCoopClientControllerView& view) noexcept
{
	if (transferringInventory() || droppingInventory() || !inventoryGroundViewCurrent(view))
	{
		inventoryNotice_ = "Pickup view unavailable here; wait for the current tile's server inventory";
		return false;
	}
	const auto* inventory = view.inventoryFor(selectedActor_);
	cancelInventoryPickup();
	cancelInventoryTransfer();
	inventoryPickupOpen_ = true;
	inventoryPickupRevision_ = inventory->inventoryRevision;
	inventoryPickupOwner_ = inventory->owner;
	inventoryPickupGrid_ = inventory->groundGrid;
	inventoryPickupLevel_ = inventory->groundLevel;
	inventoryPickupAssignmentCount_ = view.assignedActorCount;
	for (std::size_t index = 0; index < view.assignedActorCount; ++index)
		inventoryPickupAssignments_[index] = view.assignedActors[index];
	if (!inventory->groundItems.empty()) inventoryGroundItem_ = inventory->groundItems.front().id;
	inventoryNotice_ = "";
	return true;
}

bool FullEngineCoopClientController::selectInventoryGroundItem(
	const FullEngineCoopClientControllerView& view, TacticalWorldItemId item) noexcept
{
	if (!item.valid() || transferringInventory() || droppingInventory() || !inventoryGroundViewCurrent(view) ||
		(pickingUpInventory() && !inventoryPickupContextCurrent(view))) return false;
	const auto* inventory = view.inventoryFor(selectedActor_);
	for (const auto& entry : inventory->groundItems)
	{
		if (entry.id != item) continue;
		if (!pickingUpInventory() && !beginInventoryPickup(view)) return false;
		inventoryGroundItem_ = item;
		inventoryNotice_ = "";
		return true;
	}
	return false;
}

bool FullEngineCoopClientController::selectNextInventoryGroundItem(
	const FullEngineCoopClientControllerView& view, bool forward) noexcept
{
	if (!inventoryPickupContextCurrent(view)) return false;
	const auto& items = view.inventoryFor(selectedActor_)->groundItems;
	for (std::size_t index = 0; index < items.size(); ++index)
	{
		if (items[index].id != inventoryGroundItem_) continue;
		const auto next = (index + (forward ? 1 : items.size() - 1)) % items.size();
		inventoryGroundItem_ = items[next].id;
		inventoryNotice_ = "";
		return true;
	}
	return false;
}

void FullEngineCoopClientController::cancelInventoryPickup() noexcept
{
	inventoryPickupOpen_ = false;
	inventoryGroundItem_ = {};
	inventoryPickupRevision_ = 0;
	inventoryPickupOwner_ = {};
	inventoryPickupGrid_ = -1;
	inventoryPickupLevel_ = -1;
	inventoryPickupAssignments_ = {};
	inventoryPickupAssignmentCount_ = 0;
	inventorySourceSlot_ = inventoryInspectedSlot_ = 0xffff;
	inventorySourceRevision_ = inventoryInspectedRevision_ = 0;
	inventoryNotice_ = "Pickup cancelled; no ground item moved";
}

bool FullEngineCoopClientController::inventoryPickupActionsEnabled(
	const FullEngineCoopClientControllerView& view) const noexcept
{
	if (!inventoryPickupContextCurrent(view) || !inventoryGroundItem_.valid()) return false;
	const auto* actor = view.snapshot->find(selectedActor_);
	return actionsEnabled(view) && actor && actor->life >= 15 && actor->presentation.bodyType <= 3 &&
		!actor->medicalService.providing && !actor->medicalService.receivingCount &&
		HasMovementActionPoints(view, selectedActor_);
}

FullEngineCoopClientIntentRequest FullEngineCoopClientController::inventoryPickup(
	const FullEngineCoopClientControllerView& view, std::uint16_t destinationSlot) noexcept
{
	if (!inventoryPickupContextCurrent(view) || !inventoryGroundItem_.valid()) return {};
	if (!inventoryPickupActionsEnabled(view))
	{
		inventoryNotice_ = "Pickup is read-only while this merc cannot act; server checks the AP cost";
		return {};
	}
	const auto* inventory = view.inventoryFor(selectedActor_);
	if (destinationSlot < 14 || destinationSlot >= 55 || destinationSlot >= inventory->slots.size() ||
		inventory->slots[destinationSlot].support != CoopSession::CoopInventorySlotSupport::Empty)
	{
		inventoryNotice_ = "Pickup: choose an empty pocket; no hands, exchange, merge or split";
		return {};
	}
	FullEngineCoopClientIntentRequest result{selectedActor_,
		CoopSession::PickupWorldItemTacticalIntent{inventoryGroundItem_, destinationSlot, inventoryPickupRevision_}, true};
	cancelInventoryPickup();
	inventoryNotice_ = "";
	return result;
}
