#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_ENTITY_DIRECTORY_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_ENTITY_DIRECTORY_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include <Engine/Adapters/JA2/TacticalEntity.h>

// Pointer-free ownership of tactical actor liveness and incarnation. JA2's
// SOLDIERTYPE pool remains the compatibility storage; application adapters
// validate a directory identity against that pool before returning a pointer.
class TacticalEntityDirectory
{
public:
	static constexpr std::size_t DefaultMaximumSlots = 2048;
	static constexpr std::size_t MaximumRepresentableSlots =
		static_cast<std::size_t>(TacticalEntityId{}.slot);

	explicit TacticalEntityDirectory(
		std::size_t maximumSlots = DefaultMaximumSlots);

	std::size_t maximumSlots() const noexcept { return incarnations_.size(); }
	std::size_t activeCount() const noexcept { return activeCount_; }

	// Issuing and activating are intentionally separate. Legacy soldier
	// creation consumes an incarnation before operations that can fail, and
	// save restoration consumes a temporary incarnation before copying the
	// serialized identity over it.
	std::uint32_t issueIncarnation() noexcept;
	std::uint32_t nextIncarnation() const noexcept { return nextIncarnation_; }
	void restoreNextIncarnation(std::uint32_t nextIncarnation) noexcept
	{
		nextIncarnation_ = nextIncarnation;
	}

	bool activate(TacticalEntityId entity) noexcept;
	bool release(TacticalEntityId entity) noexcept;
	bool contains(TacticalEntityId entity) const noexcept;
	TacticalEntityId identity(std::uint16_t slot) const noexcept;
	void reset() noexcept;

private:
	std::vector<std::uint32_t> incarnations_;
	std::size_t activeCount_ = 0;
	std::uint32_t nextIncarnation_ = 1;
};

#endif
