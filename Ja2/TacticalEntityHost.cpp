#include "TacticalEntityHost.h"

#include <cstddef>

#include "Animation Control.h"
#include "Soldier Control.h"

namespace
{
Ja2SoldierRepository& StandaloneRepository() noexcept
{
	static Ja2SoldierRepository repository;
	return repository;
}

TacticalEntityDirectory& StandaloneDirectory() noexcept
{
	static TacticalEntityDirectory directory(
		StandaloneRepository().capacity());
	return directory;
}

TacticalEntityDirectory*& BoundDirectory() noexcept
{
	static TacticalEntityDirectory* directory = &StandaloneDirectory();
	return directory;
}

Ja2SoldierRepository*& BoundRepository() noexcept
{
	static Ja2SoldierRepository* repository = &StandaloneRepository();
	return repository;
}

TacticalEntityId LegacyIdentity(const SOLDIERTYPE& soldier) noexcept
{
	return TacticalEntityId{
		static_cast<std::uint16_t>(soldier.ubID),
		soldier.uiUniqueSoldierIdValue};
}

TacticalStance LegacyStance(const SOLDIERTYPE& soldier) noexcept
{
	if (soldier.usAnimState >= NUMANIMATIONSTATES)
		return TacticalStance::Unknown;
	switch (gAnimControl[soldier.usAnimState].ubHeight)
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
		static_cast<std::uint8_t>(soldier.bTeam),
		static_cast<std::uint16_t>(soldier.ubProfile),
		soldier.position().gridNo(),
		static_cast<std::int8_t>(soldier.position().level()),
		soldier.position().direction(),
		soldier.usAnimState,
		LegacyStance(soldier),
		soldier.bActionPoints,
		soldier.vitals().health(),
		soldier.vitals().maximumHealth(),
		soldier.vitals().breath(),
		soldier.vitals().maximumBreath(),
		soldier.bActive != FALSE,
		soldier.bInSector != FALSE};
}
}

void BindJa2TacticalEntityDirectory(
	TacticalEntityDirectory& directory,
	Ja2SoldierRepository& soldiers) noexcept
{
	const std::uint32_t nextIncarnation =
		BoundDirectory()->nextIncarnation();
	BoundDirectory() = &directory;
	BoundRepository() = &soldiers;
	directory.restoreNextIncarnation(nextIncarnation);
	RebuildJa2TacticalEntityDirectory();
}

TacticalEntityDirectory& GetJa2TacticalEntityDirectory() noexcept
{
	return *BoundDirectory();
}

Ja2SoldierRepository& GetJa2SoldierRepository() noexcept
{
	return *BoundRepository();
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
		!BoundRepository()->contains(entity.slot, soldier) ||
		!soldier.bActive)
		return false;
	if (!BoundDirectory()->activate(entity)) return false;
	if (BoundDirectory()->publishState(LegacyState(soldier))) return true;
	(void)BoundDirectory()->release(entity);
	return false;
}

bool ReleaseJa2TacticalEntity(const SOLDIERTYPE& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!BoundRepository()->contains(entity.slot, soldier))
		return false;
	return BoundDirectory()->release(entity);
}

bool SynchronizeJa2TacticalEntityState(
	const SOLDIERTYPE& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (!entity.valid() ||
		!BoundRepository()->contains(entity.slot, soldier) ||
		!soldier.bActive ||
		!BoundDirectory()->contains(entity))
		return false;
	return BoundDirectory()->publishState(LegacyState(soldier));
}

bool SynchronizeJa2TacticalEntityStates() noexcept
{
	std::size_t synchronized = 0;
	for (std::size_t slot = 0;
		slot < BoundRepository()->capacity(); ++slot)
	{
		const SOLDIERTYPE* soldier = BoundRepository()->resolve(slot);
		if (!soldier || !soldier->bActive)
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
	ResetJa2TacticalEntityDirectory();
	for (std::size_t slot = 0;
		slot < BoundRepository()->capacity(); ++slot)
	{
		SOLDIERTYPE* soldier = BoundRepository()->resolve(slot);
		if (soldier) (void)AdoptJa2TacticalEntity(*soldier);
	}
}

bool SwapJa2TacticalEntitySlots(
	std::uint16_t firstSlot, std::uint16_t secondSlot)
{
	if (!BoundRepository()->swapRecords(firstSlot, secondSlot)) return false;
	RebuildJa2TacticalEntityDirectory();
	return true;
}

SOLDIERTYPE* ResolveJa2TacticalEntity(TacticalEntityId entity) noexcept
{
	if (!BoundDirectory()->contains(entity))
		return nullptr;
	SOLDIERTYPE* soldier = BoundRepository()->resolve(entity.slot);
	if (!soldier || !soldier->bActive ||
		static_cast<std::uint16_t>(soldier->ubID) != entity.slot ||
		soldier->uiUniqueSoldierIdValue != entity.incarnation)
		return nullptr;
	return soldier;
}

TacticalEntityId GetJa2TacticalEntityId(std::uint16_t slot) noexcept
{
	const TacticalEntityId entity = BoundDirectory()->identity(slot);
	return ResolveJa2TacticalEntity(entity) ? entity : TacticalEntityId{};
}

bool Ja2TacticalEntityReference::capture(
	const SOLDIERTYPE* soldier) noexcept
{
	reset();
	if (!soldier) return false;
	const std::uint16_t slot =
		static_cast<std::uint16_t>(soldier->ubID);
	const TacticalEntityId entity = GetJa2TacticalEntityId(slot);
	if (!entity.valid() || ResolveJa2TacticalEntity(entity) != soldier)
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
