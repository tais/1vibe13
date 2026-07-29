#ifndef TACTICAL_SOLDIER_INVENTORY_H
#define TACTICAL_SOLDIER_INVENTORY_H

#include "Item Types.h"

#include <cstddef>
#include <vector>

// Neutral slot storage used by soldier creation, map, and legacy conversion
// records. The object stack and both presentation counters always resize as one
// unit, while their established on-disk representation remains unchanged.
class InventorySlots
{
public:
	using size_type = std::size_t;

	explicit InventorySlots(int slotCount = NUM_INV_SLOTS);
	InventorySlots(const InventorySlots&) = default;
	InventorySlots& operator=(const InventorySlots&) = default;
	InventorySlots(InventorySlots&&) noexcept = default;
	InventorySlots& operator=(InventorySlots&&) noexcept = default;
	~InventorySlots() = default;

	OBJECTTYPE& operator[](size_type slot);
	const OBJECTTYPE& operator[](size_type slot) const;

	size_type size() const noexcept { return items_.size(); }
	bool empty() const noexcept { return items_.empty(); }
	const std::vector<OBJECTTYPE>& items() const noexcept { return items_; }

	int& newItemCount(size_type slot);
	const int& newItemCount(size_type slot) const;
	int& newItemCycleCount(size_type slot);
	const int& newItemCycleCount(size_type slot) const;

	BOOLEAN Load(HWFILE file);
	BOOLEAN Load(
		INT8** buffer, float majorMapVersion, UINT8 minorMapVersion);
	BOOLEAN Save(HWFILE file, bool savingMap);

	void clear();
	bool coherent() const noexcept;

private:
	void ensureSlot(size_type slot);
	void resize(size_type slotCount);

	std::vector<OBJECTTYPE> items_;
	std::vector<int> newItemCounts_;
	std::vector<int> newItemCycleCounts_;
};

// Canonical owner for one live soldier's carried objects and inventory-local
// persistent state. Slot-only assignment deliberately preserves the adjacent
// state, matching its historical separation from create-record and v101 slot
// transfers.
class SoldierInventory final : public InventorySlots
{
public:
	SoldierInventory() = default;
	explicit SoldierInventory(int slotCount) : InventorySlots(slotCount) {}
	SoldierInventory(const SoldierInventory&) = default;
	SoldierInventory& operator=(const SoldierInventory&) = default;
	SoldierInventory(SoldierInventory&&) noexcept = default;
	SoldierInventory& operator=(SoldierInventory&&) noexcept = default;
	~SoldierInventory() = default;

	SoldierInventory& operator=(const InventorySlots& slots);

	INT8& keyAccess() noexcept { return keyAccess_; }
	const INT8& keyAccess() const noexcept { return keyAccess_; }
	BOOLEAN& checkForNewItems() noexcept { return checkForNewItems_; }
	const BOOLEAN& checkForNewItems() const noexcept
	{
		return checkForNewItems_;
	}
	BOOLEAN& zipperFlag() noexcept { return zipperFlag_; }
	const BOOLEAN& zipperFlag() const noexcept { return zipperFlag_; }
	BOOLEAN& dropPackFlag() noexcept { return dropPackFlag_; }
	const BOOLEAN& dropPackFlag() const noexcept { return dropPackFlag_; }

	void reset();

private:
	INT8 keyAccess_ = 0;
	BOOLEAN checkForNewItems_ = FALSE;
	BOOLEAN zipperFlag_ = FALSE;
	BOOLEAN dropPackFlag_ = FALSE;
};

#endif
