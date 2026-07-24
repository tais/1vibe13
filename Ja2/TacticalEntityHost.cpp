#include "TacticalEntityHost.h"

#include "Overhead.h"
#include "Soldier Control.h"

namespace
{
TacticalEntityDirectory& StandaloneDirectory() noexcept
{
	static TacticalEntityDirectory directory(TOTAL_SOLDIERS);
	return directory;
}

TacticalEntityDirectory*& BoundDirectory() noexcept
{
	static TacticalEntityDirectory* directory = &StandaloneDirectory();
	return directory;
}

TacticalEntityId LegacyIdentity(const SOLDIERTYPE& soldier) noexcept
{
	return TacticalEntityId{
		static_cast<std::uint16_t>(soldier.ubID),
		soldier.uiUniqueSoldierIdValue};
}
}

void BindJa2TacticalEntityDirectory(TacticalEntityDirectory& directory) noexcept
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
	if (!entity.valid() || entity.slot >= TOTAL_SOLDIERS ||
		MercPtrs[entity.slot] != &soldier || !soldier.bActive)
		return false;
	return BoundDirectory()->activate(entity);
}

bool ReleaseJa2TacticalEntity(const SOLDIERTYPE& soldier) noexcept
{
	const TacticalEntityId entity = LegacyIdentity(soldier);
	if (entity.slot >= TOTAL_SOLDIERS || MercPtrs[entity.slot] != &soldier)
		return false;
	return BoundDirectory()->release(entity);
}

void ResetJa2TacticalEntityDirectory() noexcept
{
	BoundDirectory()->reset();
}

void RebuildJa2TacticalEntityDirectory() noexcept
{
	ResetJa2TacticalEntityDirectory();
	for (std::uint16_t slot = 0; slot < TOTAL_SOLDIERS; ++slot)
	{
		SOLDIERTYPE* soldier = MercPtrs[slot];
		if (soldier) (void)AdoptJa2TacticalEntity(*soldier);
	}
}

bool SwapJa2TacticalEntitySlots(
	std::uint16_t firstSlot, std::uint16_t secondSlot)
{
	if (firstSlot >= TOTAL_SOLDIERS || secondSlot >= TOTAL_SOLDIERS ||
		firstSlot == secondSlot)
		return false;

	SOLDIERTYPE first = Menptr[firstSlot];
	Menptr[firstSlot] = Menptr[secondSlot];
	Menptr[secondSlot] = first;
	Menptr[firstSlot].ubID = SoldierID{firstSlot};
	Menptr[secondSlot].ubID = SoldierID{secondSlot};
	RebuildJa2TacticalEntityDirectory();
	return true;
}

SOLDIERTYPE* ResolveJa2TacticalEntity(TacticalEntityId entity) noexcept
{
	if (!BoundDirectory()->contains(entity) || entity.slot >= TOTAL_SOLDIERS)
		return nullptr;
	SOLDIERTYPE* soldier = MercPtrs[entity.slot];
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
