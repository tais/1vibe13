#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_ITEM_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_WORLD_ITEM_H

#include <cstdint>
#include <limits>

// Stable identity for one live entry in JA2's reusable gWorldItems storage.
// The slot remains useful for the compatibility adapter, while the incarnation
// prevents a delayed command from addressing a different item placed into that
// slot after the original item was removed.
struct TacticalWorldItemId
{
	std::uint32_t slot = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t incarnation = 0;

	constexpr bool valid() const noexcept
	{
		return slot != std::numeric_limits<std::uint32_t>::max() &&
			incarnation != 0;
	}
};

constexpr bool operator==(
	TacticalWorldItemId left, TacticalWorldItemId right) noexcept
{
	return left.slot == right.slot &&
		left.incarnation == right.incarnation;
}

constexpr bool operator!=(
	TacticalWorldItemId left, TacticalWorldItemId right) noexcept
{
	return !(left == right);
}

constexpr bool operator<(
	TacticalWorldItemId left, TacticalWorldItemId right) noexcept
{
	return left.slot < right.slot ||
		(left.slot == right.slot &&
			left.incarnation < right.incarnation);
}

#endif
