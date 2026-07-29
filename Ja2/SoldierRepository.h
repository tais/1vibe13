#ifndef JA2_SOLDIER_REPOSITORY_H
#define JA2_SOLDIER_REPOSITORY_H

#include <cstddef>
#include <cstdint>

class TacticalActor;
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
		TacticalActor* records, TacticalActor** slots,
		std::size_t capacity) noexcept;

	std::size_t capacity() const noexcept { return capacity_; }

	TacticalActor& record(std::size_t slot) noexcept;
	const TacticalActor& record(std::size_t slot) const noexcept;
	TacticalActor* resolve(std::size_t slot) noexcept
	{
		return slot < capacity_ ? slots_[slot] : nullptr;
	}
	const TacticalActor* resolve(std::size_t slot) const noexcept
	{
		return slot < capacity_ ? slots_[slot] : nullptr;
	}
	bool contains(std::size_t slot, const TacticalActor& soldier) const noexcept;

	// Restores the established one-record-per-slot compatibility layout.
	void initializeSlots() noexcept;

	// Whole-record mutation is deliberately centralized. Callers retain the
	// established persistent TacticalActor copy semantics while the repository
	// validates that the reusable slot still points at its canonical backing
	// record. Runtime animation surface ownership stays with the slot because
	// the legacy usage-history table is indexed by that identity.
	TacticalActor* replace(
		std::size_t slot, const TacticalActor& soldier) noexcept;
	bool swapRecords(std::uint16_t firstSlot, std::uint16_t secondSlot);

private:
	friend void BindJa2SoldierRepository(
		Ja2SoldierRepository& repository) noexcept;
	friend Ja2SoldierRepository& GetJa2SoldierRepository() noexcept;

	bool hasCanonicalBinding(std::size_t slot) const noexcept;
	static Ja2SoldierRepository& standalone() noexcept;

	static Ja2SoldierRepository* boundRepository_;
	TacticalActor* records_ = nullptr;
	TacticalActor** slots_ = nullptr;
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
