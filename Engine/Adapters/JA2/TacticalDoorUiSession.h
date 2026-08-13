#ifndef ENGINE_ADAPTERS_JA2_TACTICAL_DOOR_UI_SESSION_H
#define ENGINE_ADAPTERS_JA2_TACTICAL_DOOR_UI_SESSION_H

#include <cstdint>

#include <Engine/Adapters/JA2/TacticalEntity.h>

// Exact, pointer-free identity of the structure selected by a tactical door
// menu. The map-local ID is scoped by the tactical-world generation retained
// in TacticalDoorUiContext. The fingerprint lets the JA2 adapter reject a
// removed/replaced or otherwise changed structure before a delayed callback.
struct TacticalDoorStructureIdentity
{
	std::int32_t grid = -1;
	std::int32_t baseGrid = -1;
	std::uint16_t structureId = 0;
	std::uint64_t fingerprint = 0;

	constexpr bool valid() const noexcept
	{
		return grid >= 0 && baseGrid >= 0 && structureId != 0 &&
			fingerprint != 0;
	}
};

constexpr bool operator==(
	TacticalDoorStructureIdentity left,
	TacticalDoorStructureIdentity right) noexcept
{
	return left.grid == right.grid && left.baseGrid == right.baseGrid &&
		left.structureId == right.structureId &&
		left.fingerprint == right.fingerprint;
}

constexpr bool operator!=(
	TacticalDoorStructureIdentity left,
	TacticalDoorStructureIdentity right) noexcept
{
	return !(left == right);
}

struct TacticalDoorUiContext
{
	TacticalEntityId actor;
	std::uint64_t worldGeneration = 0;
	TacticalDoorStructureIdentity structure;
	std::uint8_t direction = 0;
	bool closingDoor = false;

	constexpr bool valid() const noexcept
	{
		return actor.valid() && worldGeneration != 0 && structure.valid() &&
			direction < 8;
	}
};

// EngineRuntime owns this single-modal value session. It deliberately performs
// no JA2 lookup: the application adapter resolves the exact actor and structure
// at each use and supplies their current value identities to matches().
class TacticalDoorUiSession
{
public:
	bool begin(TacticalDoorUiContext context) noexcept;
	bool active() const noexcept { return context_.valid(); }
	const TacticalDoorUiContext& context() const noexcept { return context_; }

	bool matches(
		std::uint64_t worldGeneration,
		TacticalEntityId actor,
		TacticalDoorStructureIdentity structure) const noexcept;

	void reset() noexcept { context_ = {}; }

private:
	TacticalDoorUiContext context_;
};

#endif
