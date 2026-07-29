#include "TacticalEntityHost.h"

#include <cstddef>
#include <limits>
#include <optional>

#include <Engine/Adapters/JA2/TacticalEntityRoster.h>

#include "Animation Control.h"
#include "SoldierRepository.h"
#include "Soldier Control.h"
#include "StrategicSquadHost.h"

namespace
{
TacticalEntityDirectory& StandaloneDirectory() noexcept
{
	static TacticalEntityDirectory directory(
		GetJa2SoldierRepository().capacity());
	return directory;
}

TacticalEntityDirectory*& BoundDirectory() noexcept
{
	static TacticalEntityDirectory* directory = &StandaloneDirectory();
	return directory;
}

TacticalEntityRoster& ActiveActorRoster() noexcept
{
	static TacticalEntityRoster roster(
		GetJa2SoldierRepository().capacity());
	return roster;
}

TacticalEntityRoster& AwayActorRoster() noexcept
{
	static TacticalEntityRoster roster(
		GetJa2SoldierRepository().capacity());
	return roster;
}

TacticalEntityId LegacyIdentity(const SOLDIERTYPE& soldier) noexcept
{
	return TacticalEntityId{
		static_cast<std::uint16_t>(soldier.identity().id()),
		soldier.identity().incarnation()};
}

TacticalStance LegacyStance(const SOLDIERTYPE& soldier) noexcept
{
	if (soldier.animationPlayback().state() >= NUMANIMATIONSTATES)
		return TacticalStance::Unknown;
	switch (gAnimControl[soldier.animationPlayback().state()].ubHeight)
	{
		case ANIM_STAND: return TacticalStance::Standing;
		case ANIM_CROUCH: return TacticalStance::Crouched;
		case ANIM_PRONE: return TacticalStance::Prone;
		default: return TacticalStance::Unknown;
	}
}

TacticalActorSnapshot LegacyState(
	const SOLDIERTYPE& soldier) noexcept
{
	return TacticalActorSnapshot{
		LegacyIdentity(soldier),
		static_cast<std::uint8_t>(soldier.roster().team()),
		static_cast<std::uint16_t>(soldier.identity().profile()),
		soldier.position().gridNo(),
		static_cast<std::int8_t>(soldier.position().level()),
		soldier.position().direction(),
		soldier.animationPlayback().state(),
		LegacyStance(soldier),
		soldier.actionPoints().current(),
		soldier.vitals().health(),
		soldier.vitals().maximumHealth(),
		soldier.vitals().breath(),
		soldier.vitals().maximumBreath(),
		soldier.roster().active() != FALSE,
		soldier.roster().inSector() != FALSE};
}

void RebindRosterAfterRecordSwap(
	TacticalEntityRoster& roster) noexcept
{
	for (std::size_t slot = 0;
		slot < roster.highWaterMark(); ++slot)
	{
		const TacticalEntityId previous = roster.actor(slot);
		if (!previous.valid()) continue;

		const TacticalEntityId rebound =
			GetJa2TacticalEntityId(previous.slot);
		if (!rebound.valid())
		{
			(void)roster.erase(previous);
			continue;
		}
		if (!roster.replace(slot, rebound))
		{
			// A malformed duplicate must not leave an exact reference pointing
			// at the actor that occupied this repository slot before the swap.
			(void)roster.erase(previous);
		}
	}
}

std::int32_t AddActor(
	TacticalEntityRoster& roster,
	TacticalEntityId actor) noexcept
{
	if (!ResolveJa2TacticalEntity(actor)) return -1;
	const std::optional<TacticalEntityRoster::Slot> slot =
		roster.insert(actor);
	if (!slot ||
		*slot > static_cast<std::size_t>(
			std::numeric_limits<std::int32_t>::max()))
	{
		return -1;
	}
	return static_cast<std::int32_t>(*slot);
}
}

void BindJa2TacticalEntityDirectory(
	TacticalEntityDirectory& directory) noexcept
{
	const std::uint32_t nextIncarnation =
		BoundDirectory()->nextIncarnation();
	BoundDirectory() = &directory;
	directory.restoreNextIncarnation(nextIncarnation);
	RebuildJa2TacticalEntityDirectory();
}

TacticalEntityDirectory& GetJa2TacticalEntityDirectory() noexcept
{
	return *BoundDirectory();
}

std::uint32_t IssueJa2TacticalEntityIncarnation() noexcept
{
	return BoundDirectory()->issueIncarnation();
}

std::uint32_t NextJa2TacticalEntityIncarnation() noexcept
{
	return BoundDirectory()->nextIncarnation();
}

void RestoreJa2TacticalEntityIncarnationSequence(
	std::uint32_t nextIncarnation) noexcept
{
	BoundDirectory()->restoreNextIncarnation(nextIncarnation);
}

bool AdoptJa2TacticalEntity(SOLDIERTYPE& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!entity.valid() ||
		!GetJa2SoldierRepository().contains(entity.slot, soldier) ||
		!soldier.roster().active())
		return false;
	if (!BoundDirectory()->activate(entity)) return false;
	if (BoundDirectory()->publishState(LegacyState(soldier))) return true;
	(void)BoundDirectory()->release(entity);
	return false;
}

bool ReleaseJa2TacticalEntity(const SOLDIERTYPE& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!GetJa2SoldierRepository().contains(entity.slot, soldier))
		return false;
	return BoundDirectory()->release(entity);
}

bool SynchronizeJa2TacticalEntityState(
	const SOLDIERTYPE& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!entity.valid() ||
		!GetJa2SoldierRepository().contains(entity.slot, soldier) ||
		!soldier.roster().active() ||
		!BoundDirectory()->contains(entity))
		return false;
	return BoundDirectory()->publishState(LegacyState(soldier));
}

bool SynchronizeJa2TacticalEntityStates() noexcept
{
	Ja2SoldierRepository& soldiers = GetJa2SoldierRepository();
	std::size_t synchronized = 0;
	for (std::size_t slot = 0;
		slot < soldiers.capacity(); ++slot)
	{
		const SOLDIERTYPE* soldier = soldiers.resolve(slot);
		if (!soldier || !soldier->roster().active())
		{
			if (BoundDirectory()->identity(slot).valid()) return false;
			continue;
		}
		if (!SynchronizeJa2TacticalEntityState(*soldier)) return false;
		++synchronized;
	}
	return synchronized == BoundDirectory()->activeCount() &&
		synchronized == BoundDirectory()->stateCount();
}

void ResetJa2TacticalEntityDirectory() noexcept
{
	BoundDirectory()->reset();
}

void RebuildJa2TacticalEntityDirectory() noexcept
{
	Ja2SoldierRepository& soldiers = GetJa2SoldierRepository();
	ResetJa2TacticalEntityDirectory();
	for (std::size_t slot = 0;
		slot < soldiers.capacity(); ++slot)
	{
		SOLDIERTYPE* soldier = soldiers.resolve(slot);
		if (soldier) (void)AdoptJa2TacticalEntity(*soldier);
	}
}

bool SwapJa2TacticalEntitySlots(
	std::uint16_t firstSlot, std::uint16_t secondSlot)
{
	if (!GetJa2SoldierRepository().swapRecords(
			firstSlot, secondSlot))
		return false;
	RebuildJa2TacticalEntityDirectory();
	// The compatibility pool keeps fixed record addresses. Historically a
	// roster entry therefore followed the identity newly occupying that
	// address after a whole-record swap. Rebind exact IDs by repository slot
	// to preserve that behavior without retaining stale incarnations.
	RebindRosterAfterRecordSwap(ActiveActorRoster());
	RebindRosterAfterRecordSwap(AwayActorRoster());
	RebindJa2StrategicSquadRostersAfterRecordSwap();
	return true;
}

SOLDIERTYPE* ResolveJa2TacticalEntity(TacticalEntityId entity) noexcept
{
	if (!BoundDirectory()->contains(entity))
		return nullptr;
	SOLDIERTYPE* soldier =
		GetJa2SoldierRepository().resolve(entity.slot);
	if (!soldier || !soldier->roster().active() ||
		static_cast<std::uint16_t>(soldier->identity().id()) != entity.slot ||
		soldier->identity().incarnation() != entity.incarnation)
		return nullptr;
	return soldier;
}

TacticalEntityId GetJa2TacticalEntityId(std::uint16_t slot) noexcept
{
	const TacticalEntityId entity = BoundDirectory()->identity(slot);
	return ResolveJa2TacticalEntity(entity) ? entity : TacticalEntityId{};
}

TacticalEntityId GetJa2TacticalEntityId(
	const SOLDIERTYPE& soldier) noexcept
{
	const TacticalEntityId entity = GetJa2TacticalEntityId(
		static_cast<std::uint16_t>(soldier.identity().id()));
	return ResolveJa2TacticalEntity(entity) == &soldier
		? entity
		: TacticalEntityId{};
}

void ResetJa2TacticalActorRosters() noexcept
{
	ActiveActorRoster().clear();
	AwayActorRoster().clear();
}

std::size_t Ja2ActiveTacticalActorSlotCount() noexcept
{
	return ActiveActorRoster().highWaterMark();
}

std::size_t Ja2AwayTacticalActorSlotCount() noexcept
{
	return AwayActorRoster().highWaterMark();
}

SOLDIERTYPE* ResolveJa2ActiveTacticalActorSlot(
	std::size_t rosterSlot) noexcept
{
	return ResolveJa2TacticalEntity(
		ActiveActorRoster().actor(rosterSlot));
}

SOLDIERTYPE* ResolveJa2AwayTacticalActorSlot(
	std::size_t rosterSlot) noexcept
{
	return ResolveJa2TacticalEntity(
		AwayActorRoster().actor(rosterSlot));
}

std::int32_t AddJa2ActiveTacticalActor(
	TacticalEntityId actor) noexcept
{
	const std::int32_t slot =
		AddActor(ActiveActorRoster(), actor);
	if (slot >= 0) (void)AwayActorRoster().erase(actor);
	return slot;
}

std::int32_t AddJa2AwayTacticalActor(
	TacticalEntityId actor) noexcept
{
	const std::int32_t slot =
		AddActor(AwayActorRoster(), actor);
	if (slot >= 0) (void)ActiveActorRoster().erase(actor);
	return slot;
}

bool RemoveJa2ActiveTacticalActor(
	TacticalEntityId actor) noexcept
{
	return ActiveActorRoster().erase(actor);
}

bool RemoveJa2AwayTacticalActor(
	TacticalEntityId actor) noexcept
{
	return AwayActorRoster().erase(actor);
}

bool Ja2TacticalEntityReference::capture(
	TacticalEntityId entity) noexcept
{
	reset();
	if (!entity.valid() || !ResolveJa2TacticalEntity(entity))
		return false;
	entity_ = entity;
	return true;
}

SOLDIERTYPE* Ja2TacticalEntityReference::resolve() const noexcept
{
	return ResolveJa2TacticalEntity(entity_);
}

SOLDIERTYPE* Ja2TacticalEntityReference::consume() noexcept
{
	SOLDIERTYPE* soldier = resolve();
	reset();
	return soldier;
}
