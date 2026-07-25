#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_ITEM_DIRECTORY_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_ITEM_DIRECTORY_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalWorldItem.h>

// Pointer-free ownership of world-item liveness and incarnation. Storage grows
// only through an activated slot and remains capped, so malformed package input
// cannot turn a sparse slot value into an unbounded allocation.
class TacticalWorldItemDirectory
{
public:
	static constexpr std::size_t DefaultMaximumSlots = 1024u * 1024u;
	static constexpr std::size_t MaximumRepresentableSlots =
		static_cast<std::size_t>(TacticalWorldItemId{}.slot);

	explicit TacticalWorldItemDirectory(
		std::size_t maximumSlots = DefaultMaximumSlots) noexcept;

	std::size_t maximumSlots() const noexcept { return maximumSlots_; }
	std::size_t trackedSlots() const noexcept { return incarnations_.size(); }
	std::size_t activeCount() const noexcept { return activeCount_; }

	std::uint32_t issueIncarnation() noexcept;
	std::uint32_t nextIncarnation() const noexcept { return nextIncarnation_; }
	void mergeNextIncarnation(std::uint32_t nextIncarnation) noexcept;

	bool activate(TacticalWorldItemId item) noexcept;
	bool release(TacticalWorldItemId item) noexcept;
	bool contains(TacticalWorldItemId item) const noexcept;
	TacticalWorldItemId identity(std::uint32_t slot) const noexcept;
	void reset() noexcept;

private:
	std::size_t maximumSlots_;
	std::vector<std::uint32_t> incarnations_;
	std::size_t activeCount_ = 0;
	std::uint32_t nextIncarnation_ = 1;
};

#endif
