#ifndef JA2_DEDICATED_COOP_EXIT_GRID_H
#define JA2_DEDICATED_COOP_EXIT_GRID_H

#include "Engine/Adapters/JA2/StrategicGroup.h"
#include "Engine/Adapters/JA2/TacticalEntity.h"
#include <array>
#include <cstddef>
#include <cstdint>

// A whole on-foot player group leaving a secured, loaded sector through a
// native exit grid (stairs, trapdoor, tunnel): the stock exit-grid insertion and
// strategic group relocation without the walk-off animation or exit dialog.
inline constexpr std::size_t MaximumDedicatedCoopExitGridMembers = 32;

enum class DedicatedCoopExitGridCode : std::uint8_t
{
	Planned = 1,
	WorldUnavailable,
	GroupUnavailable,
	UnsupportedGroup,
	MemberNotReady,
	NotOnExitGrid,
	AmbiguousExitGrid,
	InvalidDestination,
	HostileDestination,
	Changed
};

struct DedicatedCoopExitGridPlan
{
	StrategicGroupId group{};
	std::int32_t sourceGrid = -1;       // exit-grid tile a member stands on
	std::int32_t destinationGrid = -1;  // native insertion grid in the new sector
	std::uint8_t x = 0, y = 0, z = 0;   // destination sector
	std::uint8_t originX = 0, originY = 0, originZ = 0; // the loaded sector it leaves
	std::array<TacticalEntityId, MaximumDedicatedCoopExitGridMembers> members{};
	std::size_t memberCount = 0;
	// A scripted traversal (an NPC cutscene such as the Omerta hideout) has no
	// exit-grid tile: each member gets its own native insertion grid instead.
	bool scripted = false;
	std::array<std::int32_t, MaximumDedicatedCoopExitGridMembers> insertionGrids{};
};

bool SameDedicatedCoopExitGridPlan(const DedicatedCoopExitGridPlan& a, const DedicatedCoopExitGridPlan& b) noexcept;

// Read-only: the group must be in the loaded sector, every member conscious,
// controllable and on foot, at least one member on an exit grid, and every
// occupied exit grid must lead to the same hostile-free destination.
DedicatedCoopExitGridCode PlanDedicatedCoopExitGridTraversal(StrategicGroupId group,
	DedicatedCoopExitGridPlan& output) noexcept;

// Read-only, for a scripted traversal: the same whole-group readiness as an
// exit grid, to an explicit hostile-free destination with stock per-member
// insertion grids (members past `gridCount` reuse the last grid).
DedicatedCoopExitGridCode PlanDedicatedCoopScriptedTraversal(StrategicGroupId group,
	std::uint8_t x, std::uint8_t y, std::uint8_t z, const std::int32_t* grids, std::size_t gridCount,
	DedicatedCoopExitGridPlan& output) noexcept;

// Re-plans an accepted traversal of either kind against the current world.
DedicatedCoopExitGridCode ReplanDedicatedCoopTraversal(const DedicatedCoopExitGridPlan& accepted,
	DedicatedCoopExitGridPlan& fresh) noexcept;

// The player group of the conscious merc nearest the in-sector NPC `profile`
// (the squad that followed a scripted NPC), or an invalid id.
StrategicGroupId DedicatedCoopGroupNearestToProfile(std::uint8_t profile) noexcept;

// Applies the stock exit-grid insertion values (INSERTION_CODE_GRIDNO at the
// destination grid, exit-grid re-entry direction) and relocates the group. Runs
// only after the origin world was unloaded through the co-op return path, so
// SetGroupSectorValue's native unload check cannot tear down a live world.
// Returns Changed (no mutation) when the group no longer matches the plan.
// `resultGroup` names the group holding the first member afterwards (a native
// merge may replace the original group).
DedicatedCoopExitGridCode ApplyDedicatedCoopExitGridRelocation(const DedicatedCoopExitGridPlan& plan,
	StrategicGroupId& resultGroup) noexcept;

#endif
