#ifndef JA2_SOLDIER_REPOSITORY_H
#define JA2_SOLDIER_REPOSITORY_H

#include <cstddef>
#include <cstdint>

class SOLDIERTYPE;

// Application-owned boundary around JA2's fixed soldier records and legacy
// slot table. The arrays remain compatibility storage while callers migrate,
// but record replacement, slot validation, and whole-record relocation have
// one owner instead of being open-coded against Menptr/MercPtrs.
class Ja2SoldierRepository
{
public:
	Ja2SoldierRepository() noexcept;
	Ja2SoldierRepository(
		SOLDIERTYPE* records, SOLDIERTYPE** slots,
		std::size_t capacity) noexcept;

	std::size_t capacity() const noexcept { return capacity_; }

	SOLDIERTYPE& record(std::size_t slot) noexcept;
	const SOLDIERTYPE& record(std::size_t slot) const noexcept;
	SOLDIERTYPE* resolve(std::size_t slot) noexcept;
	const SOLDIERTYPE* resolve(std::size_t slot) const noexcept;
	bool contains(std::size_t slot, const SOLDIERTYPE& soldier) const noexcept;

	// Restores the established one-record-per-slot compatibility layout.
	void initializeSlots() noexcept;

	// Whole-record mutation is deliberately centralized. Callers retain the
	// established SOLDIERTYPE copy semantics while the repository validates
	// that the reusable slot still points at its canonical backing record.
	SOLDIERTYPE* replace(
		std::size_t slot, const SOLDIERTYPE& soldier) noexcept;
	bool swapRecords(std::uint16_t firstSlot, std::uint16_t secondSlot);

private:
	bool hasCanonicalBinding(std::size_t slot) const noexcept;

	SOLDIERTYPE* records_ = nullptr;
	SOLDIERTYPE** slots_ = nullptr;
	std::size_t capacity_ = 0;
};

#endif
