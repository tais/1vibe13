#include "FullEngineCoopClientPresentationActorDirectory.h"

namespace
{
bool ValidSnapshotForPresentation(
	const TacticalWorldSnapshot& snapshot) noexcept
{
	return snapshot.epoch() != 0 && snapshot.dimensions().valid() &&
		snapshot.sector().loaded &&
		IsValidTacticalSectorSnapshot(snapshot.sector()) &&
		IsValidTacticalInterruptState(snapshot.turn());
}

bool DeadActorRemovedFromGrid(const TacticalActorSnapshot& actor) noexcept
{
	// Native corpse conversion keeps a player's roster identity but removes its
	// graphic from the grid. The observer then publishes a canonical absent
	// pose. A dying actor that still has a pose must remain visible; no corpse
	// or replacement position can be inferred from this actor record.
	return actor.life == 0 && actor.grid == -1 &&
		(actor.presentation.flags & TacticalActorRenderPosePresent) == 0 &&
		IsCanonicalTacticalActorPresentation(actor.presentation);
}
}

FullEngineCoopClientPresentationActorDirectoryResult
FullEngineCoopClientPresentationActorDirectory::stage(
	const TacticalWorldSnapshot& snapshot,
	std::size_t localSlotCapacity,
	FullEngineCoopClientPresentationActorDirectoryPlan& output) const noexcept
{
	using Result = FullEngineCoopClientPresentationActorDirectoryResult;
	if (!ValidSnapshotForPresentation(snapshot))
		return Result::InvalidSnapshot;
	if (localSlotCapacity == 0 ||
		localSlotCapacity >
			MaximumFullEngineCoopClientPresentationActorSlots)
		return Result::InvalidCapacity;

	FullEngineCoopClientPresentationActorDirectoryPlan candidate;
	candidate.origin_ = this;
	candidate.worldGeneration_ = snapshot.epoch();
	candidate.baseStateSerial_ = stateSerial_;
	candidate.localSlotCapacity_ = localSlotCapacity;

	std::array<bool,
		MaximumFullEngineCoopClientPresentationActorSlots> seenSlots{};
	for (const TacticalActorSnapshot& actor : snapshot.actors())
	{
		if (!actor.id.valid()) return Result::InvalidSnapshot;
		if (actor.id.slot >= localSlotCapacity)
			return Result::ActorSlotOutOfRange;
		if (seenSlots[actor.id.slot]) return Result::DuplicateActorSlot;
		seenSlots[actor.id.slot] = true;
		if (!actor.active || !actor.inSector) continue;
		if (DeadActorRemovedFromGrid(actor)) continue;
		if (!snapshot.dimensions().contains(actor.grid))
			return Result::InvalidActorGrid;
		candidate.desired_[actor.id.slot] = actor.id;
	}

	const bool sameWorld = worldGeneration_ == snapshot.epoch();
	for (std::size_t slot = 0; slot < localSlotCapacity_; ++slot)
	{
		const TacticalEntityId current = actors_[slot];
		const TacticalEntityId desired =
			slot < localSlotCapacity ? candidate.desired_[slot] :
			TacticalEntityId{};
		if (!current.valid() || (sameWorld && current == desired)) continue;
		candidate.operations_[candidate.operationCount_++] = {
			FullEngineCoopClientPresentationActorOperationKind::Detach,
			current, static_cast<std::uint16_t>(slot)};
	}
	for (std::size_t slot = 0; slot < localSlotCapacity; ++slot)
	{
		const TacticalEntityId desired = candidate.desired_[slot];
		if (!desired.valid()) continue;
		const bool refresh = sameWorld && slot < localSlotCapacity_ &&
			actors_[slot] == desired;
		candidate.operations_[candidate.operationCount_++] = {
			refresh
				? FullEngineCoopClientPresentationActorOperationKind::Refresh
				: FullEngineCoopClientPresentationActorOperationKind::Attach,
			desired, static_cast<std::uint16_t>(slot)};
	}
	candidate.valid_ = true;
	output = candidate;
	return Result::Success;
}

FullEngineCoopClientPresentationActorDirectoryResult
FullEngineCoopClientPresentationActorDirectory::commit(
	const FullEngineCoopClientPresentationActorDirectoryPlan& plan) noexcept
{
	using Result = FullEngineCoopClientPresentationActorDirectoryResult;
	if (!plan.valid_ || plan.origin_ != this ||
		plan.baseStateSerial_ != stateSerial_)
		return Result::StalePlan;
	actors_ = plan.desired_;
	worldGeneration_ = plan.worldGeneration_;
	localSlotCapacity_ = plan.localSlotCapacity_;
	++stateSerial_;
	if (stateSerial_ == 0) stateSerial_ = 1;
	return Result::Success;
}

void FullEngineCoopClientPresentationActorDirectory::reset() noexcept
{
	actors_ = {};
	worldGeneration_ = 0;
	localSlotCapacity_ = 0;
	++stateSerial_;
	if (stateSerial_ == 0) stateSerial_ = 1;
}
