#include "SoldierRepository.h"

#include <cassert>

#include "Soldier Control.h"

namespace
{
SOLDIERTYPE soldierRecords[TOTAL_SOLDIERS];
SOLDIERTYPE* soldierSlots[TOTAL_SOLDIERS];
}

Ja2SoldierRepository& Ja2SoldierRepository::standalone() noexcept
{
	static Ja2SoldierRepository repository;
	return repository;
}

Ja2SoldierRepository* Ja2SoldierRepository::boundRepository_ = nullptr;

Ja2SoldierRepository::Ja2SoldierRepository() noexcept
	: Ja2SoldierRepository(soldierRecords, soldierSlots, TOTAL_SOLDIERS)
{
}

Ja2SoldierRepository::Ja2SoldierRepository(
	SOLDIERTYPE* records, SOLDIERTYPE** slots,
	std::size_t capacity) noexcept
	: records_(records), slots_(slots),
	  capacity_(records && slots ? capacity : 0)
{
}

SOLDIERTYPE& Ja2SoldierRepository::record(std::size_t slot) noexcept
{
	assert(slot < capacity_);
	return records_[slot];
}

const SOLDIERTYPE& Ja2SoldierRepository::record(
	std::size_t slot) const noexcept
{
	assert(slot < capacity_);
	return records_[slot];
}

bool Ja2SoldierRepository::contains(
	std::size_t slot, const SOLDIERTYPE& soldier) const noexcept
{
	return slot < capacity_ && slots_[slot] == &soldier;
}

void Ja2SoldierRepository::initializeSlots() noexcept
{
	for (std::size_t slot = 0; slot < capacity_; ++slot)
	{
		slots_[slot] = &records_[slot];
		slots_[slot]->roster().active() = FALSE;
	}
}

SOLDIERTYPE* Ja2SoldierRepository::replace(
	std::size_t slot, const SOLDIERTYPE& soldier) noexcept
{
	if (!hasCanonicalBinding(slot)) return nullptr;
	SoldierAnimationCacheComponent retainedCache;
	retainedCache.swapStorage(records_[slot].animationCache());
	records_[slot] = soldier;
	retainedCache.swapStorage(records_[slot].animationCache());
	return &records_[slot];
}

bool Ja2SoldierRepository::swapRecords(
	std::uint16_t firstSlot, std::uint16_t secondSlot)
{
	if (firstSlot >= capacity_ || secondSlot >= capacity_ ||
		firstSlot == secondSlot || !hasCanonicalBinding(firstSlot) ||
		!hasCanonicalBinding(secondSlot))
		return false;

	SoldierAnimationCacheComponent firstSlotCache;
	SoldierAnimationCacheComponent secondSlotCache;
	firstSlotCache.swapStorage(
		records_[firstSlot].animationCache());
	secondSlotCache.swapStorage(
		records_[secondSlot].animationCache());

	SOLDIERTYPE first = records_[firstSlot];
	records_[firstSlot] = records_[secondSlot];
	records_[secondSlot] = first;
	firstSlotCache.swapStorage(
		records_[firstSlot].animationCache());
	secondSlotCache.swapStorage(
		records_[secondSlot].animationCache());
	records_[firstSlot].identity().id() = SoldierID{firstSlot};
	records_[secondSlot].identity().id() = SoldierID{secondSlot};
	return true;
}

bool Ja2SoldierRepository::hasCanonicalBinding(
	std::size_t slot) const noexcept
{
	return slot < capacity_ && slots_[slot] == &records_[slot];
}

void BindJa2SoldierRepository(
	Ja2SoldierRepository& repository) noexcept
{
	Ja2SoldierRepository::boundRepository_ = &repository;
}
