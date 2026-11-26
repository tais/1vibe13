#include "FullEngineCoopClientPresentationInteraction.h"

#include "FullEngineCoopClientMovementMode.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

namespace
{

FullEngineCoopClientPresentationInteractionResult Result(
	FullEngineCoopClientPresentationInteractionKind kind) noexcept
{
	FullEngineCoopClientPresentationInteractionResult result;
	result.kind = kind;
	return result;
}
}

FullEngineCoopClientPresentationInteractionResult
DispatchFullEngineCoopClientPresentationClick(
	FullEngineCoopClientController& controller,
	const FullEngineCoopClientControllerView& view,
	const FullEngineCoopClientPresentationClick& click) noexcept
{
	using ClickKind = FullEngineCoopClientPresentationClickKind;
	using ResultKind = FullEngineCoopClientPresentationInteractionKind;
	// An inventory/recipient modal owns every click, including missed slots.
	// It must never select another actor or accidentally dispatch movement.
	if (controller.inventoryOpen()) return {};
	if (click.kind == ClickKind::RosterActor)
		return Result(controller.selectActor(view, click.actor)
			? ResultKind::ActorSelected : ResultKind::Ignored);
	if (click.kind != ClickKind::WorldGrid || view.snapshot == nullptr)
		return {};

	if (controller.targetingAttack())
	{
		TacticalEntityId target;
		if (!ResolveFullEngineCoopClientHostileActorAtGrid(
				*view.snapshot, controller.selectedActor(),
				controller.attackTarget(), click.grid, target) ||
			!controller.selectAttackTarget(view, target))
			return {};
		return Result(ResultKind::TargetSelected);
	}
	if (controller.enteringDestination()) return {};
	if (controller.selectingDoor())
		return Result(controller.selectDoor(view, click.grid)
			? ResultKind::DoorSelected : ResultKind::Ignored);
	// A stale drawn hint is a consumed miss, not a different terrain move.
	if (click.hasLoot && !controller.nearbyLootHitCurrent(view, click.loot)) return {};
	const auto grid = click.hasLoot ? click.loot.grid : click.grid;

	// With a usable medical kit held, a world click on an adjacent wounded
	// teammate uses that kit. Roster clicks above always remain selection.
	// Clicking the current patient again must not fall through to movement and
	// inadvertently cancel the service after the duplicate aid request is filtered.
	const auto* const servicingActor = view.snapshot->find(controller.selectedActor());
	if (servicingActor != nullptr && servicingActor->medicalService.providing)
	{
		const auto* const patient = view.snapshot->find(servicingActor->medicalService.patient);
		if (patient != nullptr && patient->grid == grid) return {};
	}
	FullEngineCoopClientIntentRequest firstAid;
	for (const TacticalActorSnapshot& candidate : view.snapshot->actors())
	{
		if (candidate.grid != grid) continue;
		const auto request = controller.beginFirstAid(view, candidate.id);
		if (!request) continue;
		if (firstAid) return {}; // An ambiguous shared tile must not pick a patient.
		firstAid = request;
	}
	if (firstAid)
	{
		auto result = Result(ResultKind::IntentReady);
		result.intent = firstAid;
		return result;
	}

	TacticalEntityId actor;
	if (click.hasLoot)
	{
		// Other mercs (and aid above) still own an occupied tile. The selected
		// merc alone must not swallow its own exact-tile pickup affordance.
		for (std::size_t i = 0; i < view.assignedActorCount; ++i)
		{
			const auto id = view.assignedActors[i];
			const auto* other = view.snapshot->find(id);
			if (id != controller.selectedActor() && other && other->active && other->inSector &&
				other->grid == grid && other->level == click.loot.level)
				return Result(controller.selectActor(view, id) ? ResultKind::ActorSelected : ResultKind::Ignored);
		}
		if (grid == click.loot.context.originGrid)
			return Result(controller.openInventory(view) && controller.beginInventoryPickup(view)
				? ResultKind::InventoryOpened : ResultKind::Ignored);
	}
	if (!click.hasLoot && ResolveFullEngineCoopClientAssignedActorAtGrid(
			*view.snapshot, view.assignedActors, view.assignedActorCount,
			controller.selectedActor(), grid, actor))
		return Result(controller.selectActor(view, actor)
			? ResultKind::ActorSelected : ResultKind::Ignored);

	const TacticalActorSnapshot* const selected =
		view.snapshot->find(controller.selectedActor());
	std::uint16_t movementMode = 0;
	if (selected == nullptr ||
		!FullEngineCoopClientMovementModeFor(*selected, controller.running(), movementMode))
		return {};
	FullEngineCoopClientIntentRequest intent =
		controller.moveToGrid(view, grid, movementMode);
	if (!intent) return {};
	FullEngineCoopClientPresentationInteractionResult result;
	result.kind = ResultKind::IntentReady;
	result.intent = intent;
	return result;
}
