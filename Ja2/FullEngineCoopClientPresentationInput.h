#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_INPUT_H

#include "FullEngineCoopClientPresentationLoot.h"

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>

#include <cstddef>
#include <cstdint>

// Canonical semantic result of resolving one physical presentation click.
// Coordinates never cross this boundary: roster identity or an authority grid
// plus its presentation surface is copied by value before dispatch.
enum class FullEngineCoopClientPresentationClickKind : std::uint8_t
{
	None,
	RosterActor,
	RadarGrid,
	WorldGrid
};

struct FullEngineCoopClientPresentationClick
{
	FullEngineCoopClientPresentationClickKind kind =
		FullEngineCoopClientPresentationClickKind::None;
	TacticalEntityId actor;
	std::int32_t grid = -1;
	// Keep the terrain result as well: tactical targeting modes must never
	// reinterpret a loot overlay as their own target or bypass modal priority.
	bool hasLoot = false;
	FullEngineCoopClientPresentationLootHit loot;
};

// Immutable renderer state required to invert one logical-screen point into
// the authority's row-major tactical grid.  The passive client captures these
// values from its presentation-only renderer; the pure resolver never reads a
// cold JA2 campaign, mouse region, pathing structure, or simulation global.
struct FullEngineCoopClientPresentationViewportTransform
{
	std::int32_t viewportLeft = 0;
	std::int32_t viewportTop = 0;
	std::int32_t viewportRight = 0;  // exclusive
	std::int32_t viewportBottom = 0; // exclusive
	std::int32_t screenOriginX = 0;
	std::int32_t screenOriginY = 0;
	std::int32_t renderCenterWorldX = 0;
	std::int32_t renderCenterWorldY = 0;
	std::int32_t renderWorldOffsetX = 0;
	std::int32_t renderWorldOffsetY = 0;
	std::int32_t renderHeight = 0;

	bool valid() const noexcept;
	bool operator==(const FullEngineCoopClientPresentationViewportTransform& other) const noexcept
	{
		return viewportLeft == other.viewportLeft && viewportTop == other.viewportTop &&
			viewportRight == other.viewportRight && viewportBottom == other.viewportBottom &&
			screenOriginX == other.screenOriginX && screenOriginY == other.screenOriginY &&
			renderCenterWorldX == other.renderCenterWorldX && renderCenterWorldY == other.renderCenterWorldY &&
			renderWorldOffsetX == other.renderWorldOffsetX && renderWorldOffsetY == other.renderWorldOffsetY &&
			renderHeight == other.renderHeight;
	}
};

// Immutable passive-radar state required to invert one logical-screen point
// into the authority's row-major tactical grid.  The origin is the exact
// absolute isometric coordinate used to draw map cell (0, 0), so input cannot
// silently assume that a rectangular map is centered in the radar rectangle.
struct FullEngineCoopClientPresentationRadarTransform
{
	std::int32_t radarLeft = 0;
	std::int32_t radarTop = 0;
	std::int32_t radarRight = 0;  // exclusive
	std::int32_t radarBottom = 0; // exclusive
	double scaleX = 0.0;
	double scaleY = 0.0;
	std::int32_t markerOriginX = 0;
	std::int32_t markerOriginY = 0;

	bool valid() const noexcept;
};

enum class FullEngineCoopClientPresentationGridHitResult : std::uint8_t
{
	Success,
	InvalidInput,
	OutsideViewport,
	OutsideWorld
};

// Mirrors JA2's renderer projection and FromScreenToCellCoordinates rounding
// in checked 64-bit arithmetic, then applies the authority-published world
// dimensions.  Failure is strongly transactional: destinationGrid is left
// unchanged.
FullEngineCoopClientPresentationGridHitResult
ResolveFullEngineCoopClientPresentationGrid(
	const FullEngineCoopClientPresentationViewportTransform& transform,
	const TacticalWorldDimensions& dimensions,
	std::int32_t screenX,
	std::int32_t screenY,
	std::int32_t& destinationGrid) noexcept;

// Inverts the same isometric transform used by the passive radar camera
// outline.  It reads no legacy radar region or tactical input state. Failure
// is strongly transactional: destinationGrid is left unchanged.
FullEngineCoopClientPresentationGridHitResult
ResolveFullEngineCoopClientPresentationRadarGrid(
	const FullEngineCoopClientPresentationRadarTransform& transform,
	const TacticalWorldDimensions& dimensions,
	std::int32_t screenX,
	std::int32_t screenY,
	std::int32_t& destinationGrid) noexcept;

// Resolves actor occupancy only from the same committed snapshot that drew the
// passive presentation. Exact identities are preserved, and canonical
// assignment order provides a deterministic tie-break when actors share a
// grid. Both helpers leave output untouched when there is no eligible hit.
bool ResolveFullEngineCoopClientAssignedActorAtGrid(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	TacticalEntityId preferredActor,
	std::int32_t grid,
	TacticalEntityId& output) noexcept;

bool ResolveFullEngineCoopClientHostileActorAtGrid(
	const TacticalWorldSnapshot& snapshot,
	TacticalEntityId selectedActor,
	TacticalEntityId preferredTarget,
	std::int32_t grid,
	TacticalEntityId& output) noexcept;

#endif
