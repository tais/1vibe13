#include "FullEngineCoopClientPresentationInput.h"

#include <Multiplayer/CoopTacticalProtocol.h>

#include <cmath>
#include <limits>

namespace
{
bool PresentedAtGrid(const TacticalActorSnapshot& actor,
	const TacticalWorldDimensions& dimensions, std::int32_t grid) noexcept
{
	return actor.active && actor.inSector && actor.grid == grid &&
		dimensions.contains(actor.grid) && actor.level >= 0 && actor.level <= 1 &&
		actor.direction < 8 &&
		(actor.presentation.flags & TacticalActorRenderPosePresent) != 0;
}

bool ValidAssignedActors(const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount) noexcept
{
	if (assignedActorCount >
			CoopSession::MaximumCoopTacticalAssignedActors ||
		(assignedActorCount != 0 && assignedActors == nullptr))
		return false;
	bool haveTeam = false;
	std::uint8_t team = 0;
	for (std::size_t index = 0; index < assignedActorCount; ++index)
	{
		if (!assignedActors[index].valid() ||
			(index != 0 &&
			 !(assignedActors[index - 1] < assignedActors[index])))
			return false;
		const TacticalActorSnapshot* const actor =
			snapshot.find(assignedActors[index]);
		if (actor == nullptr || !actor->active || !actor->inSector ||
			(actor->presentation.flags & TacticalActorRenderPosePresent) == 0 ||
			(haveTeam && actor->team != team))
			return false;
		team = actor->team;
		haveTeam = true;
	}
	return true;
}

bool ContainsActor(const TacticalEntityId* actors, std::size_t count,
	TacticalEntityId sought) noexcept
{
	for (std::size_t index = 0; index < count; ++index)
		if (actors[index] == sought) return true;
	return false;
}
}

bool FullEngineCoopClientPresentationViewportTransform::valid() const noexcept
{
	return viewportLeft >= 0 && viewportTop >= 0 &&
		viewportRight > viewportLeft && viewportBottom > viewportTop &&
		screenOriginX >= viewportLeft && screenOriginX < viewportRight &&
		screenOriginY >= viewportTop && screenOriginY < viewportBottom &&
		renderCenterWorldX >= 0 && renderCenterWorldY >= 0;
}

bool FullEngineCoopClientPresentationRadarTransform::valid() const noexcept
{
	return radarLeft >= 0 && radarTop >= 0 && radarRight > radarLeft &&
		radarBottom > radarTop && std::isfinite(scaleX) &&
		std::isfinite(scaleY) && scaleX > 0.0 && scaleY > 0.0;
}

FullEngineCoopClientPresentationGridHitResult
ResolveFullEngineCoopClientPresentationGrid(
	const FullEngineCoopClientPresentationViewportTransform& transform,
	const TacticalWorldDimensions& dimensions,
	std::int32_t screenX,
	std::int32_t screenY,
	std::int32_t& destinationGrid) noexcept
{
	using Result = FullEngineCoopClientPresentationGridHitResult;
	if (!transform.valid() || !dimensions.valid())
		return Result::InvalidInput;
	if (screenX < transform.viewportLeft ||
		screenX >= transform.viewportRight ||
		screenY < transform.viewportTop ||
		screenY >= transform.viewportBottom)
		return Result::OutsideViewport;

	// The renderer projects a world delta as (2x - 2y, x + y), then
	// subtracts its render-world offset.  Keep the historical +2 inverse
	// rounding used by FromScreenToCellCoordinates so click ownership agrees
	// with the native renderer at diamond boundaries.
	const std::int64_t deltaScreenX =
		static_cast<std::int64_t>(screenX) - transform.screenOriginX +
		transform.renderWorldOffsetX;
	const std::int64_t deltaScreenY =
		static_cast<std::int64_t>(screenY) - transform.screenOriginY +
		transform.renderWorldOffsetY - transform.renderHeight;
	const std::int64_t deltaWorldX =
		(deltaScreenX + 2 * deltaScreenY + 2) / 4;
	const std::int64_t deltaWorldY =
		(2 * deltaScreenY - deltaScreenX + 2) / 4;
	const std::int64_t worldX =
		static_cast<std::int64_t>(transform.renderCenterWorldX) + deltaWorldX;
	const std::int64_t worldY =
		static_cast<std::int64_t>(transform.renderCenterWorldY) + deltaWorldY;
	if (worldX < 0 || worldY < 0)
		return Result::OutsideWorld;

	const std::int64_t column = worldX / TacticalWorldCellSize;
	const std::int64_t row = worldY / TacticalWorldCellSize;
	if (column < 0 || column >= dimensions.columns ||
		row < 0 || row >= dimensions.rows)
		return Result::OutsideWorld;
	const std::int64_t grid = row * dimensions.columns + column;
	if (grid < 0 ||
		grid > (std::numeric_limits<std::int32_t>::max)() ||
		!dimensions.contains(static_cast<std::int32_t>(grid)))
		return Result::OutsideWorld;

	destinationGrid = static_cast<std::int32_t>(grid);
	return Result::Success;
}

FullEngineCoopClientPresentationGridHitResult
ResolveFullEngineCoopClientPresentationRadarGrid(
	const FullEngineCoopClientPresentationRadarTransform& transform,
	const TacticalWorldDimensions& dimensions,
	std::int32_t screenX,
	std::int32_t screenY,
	std::int32_t& destinationGrid) noexcept
{
	using Result = FullEngineCoopClientPresentationGridHitResult;
	if (!transform.valid() || !dimensions.valid())
		return Result::InvalidInput;
	if (screenX < transform.radarLeft || screenX >= transform.radarRight ||
		screenY < transform.radarTop || screenY >= transform.radarBottom)
		return Result::OutsideViewport;

	// Marker rasterization truncates each projected coordinate to its owning
	// pixel. Invert from that pixel's center, then subtract the exact absolute
	// origin used by the forward projection. This makes every finite pixel
	// choose a stable representative of the interval that rasterized into it.
	const double deltaScreenX =
		(static_cast<double>(screenX) + 0.5 - transform.radarLeft) /
			transform.scaleX - transform.markerOriginX;
	const double deltaScreenY =
		(static_cast<double>(screenY) + 0.5 - transform.radarTop) /
			transform.scaleY - transform.markerOriginY;
	const double worldX =
		(deltaScreenX + 2.0 * deltaScreenY) / 4.0;
	const double worldY =
		(2.0 * deltaScreenY - deltaScreenX) / 4.0;
	const double maximumWorldX =
		static_cast<double>(dimensions.columns) * TacticalWorldCellSize;
	const double maximumWorldY =
		static_cast<double>(dimensions.rows) * TacticalWorldCellSize;
	if (!std::isfinite(worldX) || !std::isfinite(worldY) ||
		worldX < 0.0 || worldY < 0.0 || worldX >= maximumWorldX ||
		worldY >= maximumWorldY)
		return Result::OutsideWorld;

	const std::int64_t column = static_cast<std::int64_t>(
		std::floor(worldX / TacticalWorldCellSize));
	const std::int64_t row = static_cast<std::int64_t>(
		std::floor(worldY / TacticalWorldCellSize));
	const std::int64_t grid = row * dimensions.columns + column;
	if (column < 0 || column >= dimensions.columns || row < 0 ||
		row >= dimensions.rows || grid < 0 ||
		grid > (std::numeric_limits<std::int32_t>::max)() ||
		!dimensions.contains(static_cast<std::int32_t>(grid)))
		return Result::OutsideWorld;

	destinationGrid = static_cast<std::int32_t>(grid);
	return Result::Success;
}

bool ResolveFullEngineCoopClientAssignedActorAtGrid(
	const TacticalWorldSnapshot& snapshot,
	const TacticalEntityId* assignedActors,
	std::size_t assignedActorCount,
	TacticalEntityId preferredActor,
	std::int32_t grid,
	TacticalEntityId& output) noexcept
{
	const TacticalWorldDimensions& dimensions = snapshot.dimensions();
	if (!dimensions.contains(grid) ||
		!ValidAssignedActors(snapshot, assignedActors, assignedActorCount))
		return false;
	if (preferredActor.valid() &&
		ContainsActor(assignedActors, assignedActorCount, preferredActor))
	{
		const TacticalActorSnapshot* const actor = snapshot.find(preferredActor);
		if (actor != nullptr && PresentedAtGrid(*actor, dimensions, grid))
		{
			output = preferredActor;
			return true;
		}
	}
	for (std::size_t index = 0; index < assignedActorCount; ++index)
	{
		const TacticalActorSnapshot* const actor =
			snapshot.find(assignedActors[index]);
		if (actor == nullptr || !PresentedAtGrid(*actor, dimensions, grid))
			continue;
		output = assignedActors[index];
		return true;
	}
	return false;
}

bool ResolveFullEngineCoopClientHostileActorAtGrid(
	const TacticalWorldSnapshot& snapshot,
	TacticalEntityId selectedActor,
	TacticalEntityId preferredTarget,
	std::int32_t grid,
	TacticalEntityId& output) noexcept
{
	const TacticalWorldDimensions& dimensions = snapshot.dimensions();
	if (!selectedActor.valid() || snapshot.find(selectedActor) == nullptr ||
		!dimensions.contains(grid))
		return false;
	const auto eligible = [&](const TacticalActorSnapshot* actor) noexcept {
		return actor != nullptr && actor->id != selectedActor &&
			PresentedAtGrid(*actor, dimensions, grid) && actor->life > 0 &&
			actor->hostileToPlayerTeam;
	};
	const TacticalActorSnapshot* const retained = snapshot.find(preferredTarget);
	if (eligible(retained))
	{
		output = retained->id;
		return true;
	}
	for (const TacticalActorSnapshot& actor : snapshot.actors())
	{
		if (!eligible(&actor)) continue;
		output = actor.id;
		return true;
	}
	return false;
}
