#ifndef JA2_SOLDIER_REPOSITORY_H
#define JA2_SOLDIER_REPOSITORY_H

#include <cstddef>
#include <cstdint>

class SOLDIERTYPE;
class Ja2SoldierRepository;

void BindJa2SoldierRepository(Ja2SoldierRepository& repository) noexcept;
Ja2SoldierRepository& GetJa2SoldierRepository() noexcept;

// Application-owned boundary around JA2's fixed soldier records and slot
// table. Record replacement, slot validation, and whole-record relocation have
// one owner instead of being open-coded against process-global storage.
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
	SOLDIERTYPE* resolve(std::size_t slot) noexcept
	{
		return slot < capacity_ ? slots_[slot] : nullptr;
	}
	const SOLDIERTYPE* resolve(std::size_t slot) const noexcept
	{
		return slot < capacity_ ? slots_[slot] : nullptr;
	}
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
	friend void BindJa2SoldierRepository(
		Ja2SoldierRepository& repository) noexcept;
	friend Ja2SoldierRepository& GetJa2SoldierRepository() noexcept;

	bool hasCanonicalBinding(std::size_t slot) const noexcept;
	static Ja2SoldierRepository& standalone() noexcept;

	static Ja2SoldierRepository* boundRepository_;
	SOLDIERTYPE* records_ = nullptr;
	SOLDIERTYPE** slots_ = nullptr;
	std::size_t capacity_ = 0;
};

// Composition gateway used while legacy application subsystems migrate toward
// explicit storage access. Before GameContext composition it resolves to the
// same repository-owned backing store.
inline Ja2SoldierRepository& GetJa2SoldierRepository() noexcept
{
	return Ja2SoldierRepository::boundRepository_
		? *Ja2SoldierRepository::boundRepository_
		: Ja2SoldierRepository::standalone();
}

#endif
