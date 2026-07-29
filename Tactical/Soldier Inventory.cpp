#include "Soldier Inventory.h"

#include "DEBUG.H"

#include <cassert>

InventorySlots::InventorySlots(int slotCount)
{
	if (slotCount < 0)
	{
		DebugBreakpoint();
		slotCount = 0;
	}
	resize(static_cast<size_type>(slotCount));
}

OBJECTTYPE& InventorySlots::operator[](size_type slot)
{
	ensureSlot(slot);
	return items_[slot];
}

const OBJECTTYPE& InventorySlots::operator[](size_type slot) const
{
	assert(slot < items_.size());
	return items_[slot];
}

int& InventorySlots::newItemCount(size_type slot)
{
	ensureSlot(slot);
	return newItemCounts_[slot];
}

const int& InventorySlots::newItemCount(size_type slot) const
{
	assert(slot < newItemCounts_.size());
	return newItemCounts_[slot];
}

int& InventorySlots::newItemCycleCount(size_type slot)
{
	ensureSlot(slot);
	return newItemCycleCounts_[slot];
}

const int& InventorySlots::newItemCycleCount(size_type slot) const
{
	assert(slot < newItemCycleCounts_.size());
	return newItemCycleCounts_[slot];
}

void InventorySlots::clear()
{
	const size_type slotCount = size();
	items_.clear();
	newItemCounts_.clear();
	newItemCycleCounts_.clear();
	resize(slotCount);
}

bool InventorySlots::coherent() const noexcept
{
	return items_.size() == newItemCounts_.size() &&
		items_.size() == newItemCycleCounts_.size();
}

void InventorySlots::ensureSlot(size_type slot)
{
	if (slot >= size())
	{
		resize(slot + 1);
		DebugBreakpoint();
	}
}

void InventorySlots::resize(size_type slotCount)
{
	items_.resize(slotCount);
	newItemCounts_.resize(slotCount);
	newItemCycleCounts_.resize(slotCount);
	assert(coherent());
}

SoldierInventory& SoldierInventory::operator=(const InventorySlots& slots)
{
	InventorySlots::operator=(slots);
	return *this;
}

void SoldierInventory::reset()
{
	clear();
	keyAccess_ = 0;
	checkForNewItems_ = FALSE;
	zipperFlag_ = FALSE;
	dropPackFlag_ = FALSE;
}
