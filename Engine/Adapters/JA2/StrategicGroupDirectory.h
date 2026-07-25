#ifndef ENGINE_ADAPTERS_JA2_STRATEGIC_GROUP_DIRECTORY_H
#define ENGINE_ADAPTERS_JA2_STRATEGIC_GROUP_DIRECTORY_H

#include <array>
#include <cstddef>
#include <cstdint>

#include <Engine/Adapters/JA2/StrategicGroup.h>

// Runtime ownership of strategic-group liveness. The legacy linked list
// remains JA2's compatibility storage; this directory distinguishes successive
// occupants of each reusable one-byte group ID.
class StrategicGroupDirectory
{
public:
	static constexpr std::size_t MaximumSlots = 256;

	std::size_t activeCount() const noexcept { return activeCount_; }
	std::uint32_t nextIncarnation() const noexcept { return nextIncarnation_; }
	void mergeNextIncarnation(std::uint32_t nextIncarnation) noexcept;

	StrategicGroupId adopt(std::uint8_t slot) noexcept;
	bool release(StrategicGroupId group) noexcept;
	bool contains(StrategicGroupId group) const noexcept;
	StrategicGroupId identity(std::uint8_t slot) const noexcept;
	void reset() noexcept;

private:
	std::uint32_t issueIncarnation() noexcept;

	std::array<std::uint32_t, MaximumSlots> incarnations_{};
	std::size_t activeCount_ = 0;
	std::uint32_t nextIncarnation_ = 1;
};

#endif
