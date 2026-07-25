#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_ENTITY_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_ENTITY_H

#include <cstdint>
#include <limits>

// Stable identity for a live JA2 tactical actor. A slot alone is reusable, so
// commands, snapshots, and package messages must carry the incarnation that
// JA2 assigns when constructing the SOLDIERTYPE occupying that slot.
struct TacticalEntityId
{
	std::uint16_t slot = std::numeric_limits<std::uint16_t>::max();
	std::uint32_t incarnation = 0;

	constexpr bool valid() const
	{
		return slot != std::numeric_limits<std::uint16_t>::max() && incarnation != 0;
	}
};

constexpr bool operator==(TacticalEntityId left, TacticalEntityId right)
{
	return left.slot == right.slot && left.incarnation == right.incarnation;
}

constexpr bool operator!=(TacticalEntityId left, TacticalEntityId right)
{
	return !(left == right);
}

constexpr bool operator<(TacticalEntityId left, TacticalEntityId right)
{
	return left.slot < right.slot ||
		(left.slot == right.slot && left.incarnation < right.incarnation);
}

#endif
