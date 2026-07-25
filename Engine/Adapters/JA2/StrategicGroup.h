#ifndef ENGINE_ADAPTERS_JA2_STRATEGIC_GROUP_H
#define ENGINE_ADAPTERS_JA2_STRATEGIC_GROUP_H

#include <cstdint>

// Stable identity for a live JA2 strategic movement group. Legacy group IDs
// are reused after deletion, so delayed UI and transition state must retain the
// incarnation as well as the one-byte compatibility ID.
struct StrategicGroupId
{
	std::uint8_t slot = 0;
	std::uint32_t incarnation = 0;

	constexpr bool valid() const
	{
		return slot != 0 && incarnation != 0;
	}
};

constexpr bool operator==(StrategicGroupId left, StrategicGroupId right)
{
	return left.slot == right.slot && left.incarnation == right.incarnation;
}

constexpr bool operator!=(StrategicGroupId left, StrategicGroupId right)
{
	return !(left == right);
}

constexpr bool operator<(StrategicGroupId left, StrategicGroupId right)
{
	return left.slot < right.slot ||
		(left.slot == right.slot && left.incarnation < right.incarnation);
}

#endif
