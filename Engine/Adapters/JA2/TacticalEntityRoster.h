#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_ENTITY_ROSTER_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_ENTITY_ROSTER_H

#include <cstddef>
#include <optional>
#include <vector>

#include <Engine/Adapters/JA2/TacticalEntity.h>

// Fixed-capacity, pointer-free membership for ordered tactical actor lists.
// Storage is allocated at construction; inserts reuse the lowest vacant slot
// and never allocate in simulation hot paths. Membership is exact across
// reusable JA2 soldier slots because each entry carries its incarnation.
class TacticalEntityRoster
{
public:
	using Slot = std::size_t;

	explicit TacticalEntityRoster(std::size_t capacity);

	std::optional<Slot> insert(TacticalEntityId actor) noexcept;
	bool erase(TacticalEntityId actor) noexcept;
	bool replace(Slot slot, TacticalEntityId actor) noexcept;

	TacticalEntityId actor(Slot slot) const noexcept;
	bool contains(TacticalEntityId actor) const noexcept;
	void clear() noexcept;

	std::size_t capacity() const noexcept { return actors_.size(); }
	std::size_t size() const noexcept { return size_; }
	std::size_t highWaterMark() const noexcept { return highWaterMark_; }
	bool empty() const noexcept { return size_ == 0; }
	bool full() const noexcept { return size_ == actors_.size(); }

private:
	std::vector<TacticalEntityId> actors_;
	std::size_t size_ = 0;
	std::size_t highWaterMark_ = 0;
};

#endif
