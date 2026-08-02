#include "SoldierRepository.h"

#include <cassert>

#include "TacticalActor.h"

namespace
{
TacticalActor soldierRecords[TOTAL_SOLDIERS];
TacticalActor* soldierSlots[TOTAL_SOLDIERS];
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
	TacticalActor* records, TacticalActor** slots,
	std::size_t capacity) noexcept
	: records_(records), slots_(slots),
	  capacity_(records && slots ? capacity : 0)
{
}

TacticalActor& Ja2SoldierRepository::record(std::size_t slot) noexcept
{
	assert(slot < capacity_);
	return records_[slot];
}

const TacticalActor& Ja2SoldierRepository::record(
	std::size_t slot) const noexcept
{
	assert(slot < capacity_);
	return records_[slot];
}

bool Ja2SoldierRepository::contains(
	std::size_t slot, const TacticalActor& soldier) const noexcept
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

TacticalActor* Ja2SoldierRepository::replace(
	std::size_t slot, const TacticalActor& soldier) noexcept
{
	if (!hasCanonicalBinding(slot)) return nullptr;
	SoldierAnimationCacheComponent retainedCache;
	SoldierRenderBindingsComponent incomingBindings;
	incomingBindings.copyBindingsFrom(soldier.renderBindings());
	retainedCache.swapStorage(records_[slot].animationCache());
	records_[slot] = soldier;
	retainedCache.swapStorage(records_[slot].animationCache());
	records_[slot].renderBindings().copyBindingsFrom(incomingBindings);
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
	SoldierStrategicPathComponent firstStrategicPath;
	SoldierStrategicPathComponent secondStrategicPath;
	RenderPaletteBank firstPalette;
	RenderPaletteBank secondPalette;
	SoldierRenderBindingsComponent firstRenderBindings;
	SoldierRenderBindingsComponent secondRenderBindings;
	firstSlotCache.swapStorage(
		records_[firstSlot].animationCache());
	secondSlotCache.swapStorage(
		records_[secondSlot].animationCache());
	firstStrategicPath.swapStorage(
		records_[firstSlot].strategicPath());
	secondStrategicPath.swapStorage(
		records_[secondSlot].strategicPath());
	firstPalette.swapStorage(records_[firstSlot].palette());
	secondPalette.swapStorage(records_[secondSlot].palette());
	firstRenderBindings.swapStorage(
		records_[firstSlot].renderBindings());
	secondRenderBindings.swapStorage(
		records_[secondSlot].renderBindings());

	TacticalActor first = records_[firstSlot];
	records_[firstSlot] = records_[secondSlot];
	records_[secondSlot] = first;
	firstSlotCache.swapStorage(
		records_[firstSlot].animationCache());
	secondSlotCache.swapStorage(
		records_[secondSlot].animationCache());
	secondStrategicPath.swapStorage(
		records_[firstSlot].strategicPath());
	firstStrategicPath.swapStorage(
		records_[secondSlot].strategicPath());
	secondPalette.swapStorage(records_[firstSlot].palette());
	firstPalette.swapStorage(records_[secondSlot].palette());
	secondRenderBindings.swapStorage(
		records_[firstSlot].renderBindings());
	firstRenderBindings.swapStorage(
		records_[secondSlot].renderBindings());
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
