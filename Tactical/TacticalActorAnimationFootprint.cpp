#include "TacticalActorAnimationFootprint.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "Soldier Control.h"
#include "World Tile Map.h"
#include "worlddef.h"
#include "worldman.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace
{
struct ResolvedProfile
{
	const ANIM_PROF_DIR* direction = nullptr;
	std::int32_t baseGrid = 0;
};

bool hasWorldStorage() noexcept
{
	if (gpWorldLevelData == nullptr ||
		WORLD_COLS <= 0 || WORLD_ROWS <= 0)
	{
		return false;
	}

	const std::uint64_t worldSize =
		static_cast<std::uint64_t>(WORLD_COLS) *
		static_cast<std::uint64_t>(WORLD_ROWS);
	return worldSize <= std::numeric_limits<std::uint32_t>::max() &&
		GetWorldTileMapSize() >= worldSize;
}

bool hasWorldGrid(std::int32_t grid) noexcept
{
	return hasWorldStorage() &&
		grid >= 0 &&
		static_cast<std::uint32_t>(grid) < GetWorldTileMapSize() &&
		static_cast<std::uint64_t>(grid) <
			static_cast<std::uint64_t>(WORLD_COLS) *
			static_cast<std::uint64_t>(WORLD_ROWS);
}

bool hasBoundedAnimationResolverState(
	const TacticalActor& actor,
	std::uint16_t animationState) noexcept
{
	if (animationState >= NUMANIMATIONSTATES ||
		actor.identity().bodyType() >= TOTALBODYTYPES ||
		actor.position().direction() >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	if (actor.identity().bodyType() > REGFEMALE)
		return true;

	if (actor.inventory().size() < NUM_INV_SLOTS)
		return false;

	for (const std::size_t slot : {HANDPOS, SECONDHANDPOS})
	{
		const OBJECTTYPE& object = actor.inventory()[slot];
		if (object.usItem >= MAXITEMS ||
			(object.exists() && object.objectStack.empty()))
		{
			return false;
		}
	}
	return true;
}

bool resolveAnimationSurface(
	TacticalActor& actor,
	std::uint16_t animationState,
	std::uint16_t& animationSurface)
{
	if (!hasBoundedAnimationResolverState(actor, animationState))
		return false;

	animationSurface = DetermineSoldierAnimationSurface(
		&actor,
		animationState);
	return animationSurface != INVALID_ANIMATION_SURFACE &&
		animationSurface < NUMANIMATIONSURFACETYPES;
}

bool resolveProfile(
	const TacticalActor& actor,
	std::uint16_t animationState,
	std::uint16_t animationSurface,
	ResolvedProfile& resolved) noexcept
{
	if (!hasWorldGrid(actor.position().gridNo()) ||
		animationState >= NUMANIMATIONSTATES ||
		animationSurface == INVALID_ANIMATION_SURFACE ||
		animationSurface >= NUMANIMATIONSURFACETYPES ||
		actor.position().direction() >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	const std::int8_t profileIndex =
		gAnimSurfaceDatabase[animationSurface].bProfile;
	resolved.baseGrid = actor.position().gridNo();
	if (profileIndex == -1)
	{
		resolved.direction = nullptr;
		return true;
	}
	if (profileIndex < 0 ||
		gpAnimProfiles == nullptr ||
		static_cast<std::uint8_t>(profileIndex) >= gubNumAnimProfiles)
	{
		return false;
	}

	const ANIM_PROF_DIR& direction =
		gpAnimProfiles[profileIndex].Dirs[
			actor.position().direction()];
	if (direction.ubNumTiles != 0 && direction.pTiles == nullptr)
		return false;

	resolved.direction = &direction;
	return true;
}

bool profileGrid(
	std::int32_t baseGrid,
	const ANIM_PROF_TILE& tile,
	std::int32_t& grid) noexcept
{
	if (!hasWorldGrid(baseGrid))
		return false;

	const std::int64_t baseColumn = baseGrid % WORLD_COLS;
	const std::int64_t baseRow = baseGrid / WORLD_COLS;
	const std::int64_t column = baseColumn + tile.bTileX;
	const std::int64_t row = baseRow + tile.bTileY;
	if (column < 0 || column >= WORLD_COLS ||
		row < 0 || row >= WORLD_ROWS)
	{
		return false;
	}

	const std::int64_t candidate = row * WORLD_COLS + column;
	if (candidate < 0 ||
		static_cast<std::uint64_t>(candidate) >= GetWorldTileMapSize())
	{
		return false;
	}

	grid = static_cast<std::int32_t>(candidate);
	return true;
}

bool addResolved(
	TacticalActor& actor,
	const ResolvedProfile& profile)
{
	if (profile.direction == nullptr)
		return true;

	std::array<std::int32_t,
		static_cast<std::size_t>(
			std::numeric_limits<std::uint8_t>::max()) + 1>
		addedGrids{};
	std::size_t addedCount = 0;
	for (std::uint32_t index = 0;
		 index < profile.direction->ubNumTiles;
		 ++index)
	{
		const ANIM_PROF_TILE& tile =
			profile.direction->pTiles[index];
		std::int32_t grid = 0;
		if (!profileGrid(profile.baseGrid, tile, grid))
			continue;

		if (!AddMercToHead(grid, &actor, FALSE) ||
			GetMapElement(
				static_cast<std::uint32_t>(grid)).pMercHead == nullptr)
		{
			while (addedCount != 0)
			{
				--addedCount;
				RemoveMerc(
					addedGrids[addedCount],
					&actor,
					TRUE);
			}
			return false;
		}

		LEVELNODE& node = *GetMapElement(
			static_cast<std::uint32_t>(grid)).pMercHead;
		node.uiFlags |= LEVELNODE_MERCPLACEHOLDER;
		node.uiAnimHitLocationFlags = tile.usTileFlags;
		addedGrids[addedCount++] = grid;
	}
	return true;
}

bool removeResolved(
	TacticalActor& actor,
	const ResolvedProfile& profile) noexcept
{
	if (profile.direction == nullptr)
		return true;

	for (std::uint32_t index = 0;
		 index < profile.direction->ubNumTiles;
		 ++index)
	{
		std::int32_t grid = 0;
		if (profileGrid(
				profile.baseGrid,
				profile.direction->pTiles[index],
				grid))
		{
			RemoveMerc(grid, &actor, TRUE);
		}
	}
	return true;
}
}

bool TacticalActorAnimationFootprint::add(
	TacticalActor& actor,
	std::uint16_t animationState)
{
	std::uint16_t animationSurface = INVALID_ANIMATION_SURFACE;
	return resolveAnimationSurface(
			actor,
			animationState,
			animationSurface) &&
		addForSurface(actor, animationState, animationSurface);
}

bool TacticalActorAnimationFootprint::addForSurface(
	TacticalActor& actor,
	std::uint16_t animationState,
	std::uint16_t animationSurface)
{
	ResolvedProfile profile;
	return resolveProfile(
			actor,
			animationState,
			animationSurface,
			profile) &&
		addResolved(actor, profile);
}

bool TacticalActorAnimationFootprint::remove(
	TacticalActor& actor,
	std::uint16_t animationState)
{
	std::uint16_t animationSurface = INVALID_ANIMATION_SURFACE;
	if (!resolveAnimationSurface(
			actor,
			animationState,
			animationSurface))
	{
		return false;
	}

	ResolvedProfile profile;
	return resolveProfile(
			actor,
			animationState,
			animationSurface,
			profile) &&
		removeResolved(actor, profile);
}

bool TacticalActorAnimationFootprint::flagsAtGrid(
	TacticalActor& actor,
	std::uint16_t animationState,
	std::int32_t grid,
	std::uint16_t& flags)
{
	std::uint16_t animationSurface = INVALID_ANIMATION_SURFACE;
	if (!resolveAnimationSurface(
			actor,
			animationState,
			animationSurface))
	{
		return false;
	}

	ResolvedProfile profile;
	if (!resolveProfile(
			actor,
			animationState,
			animationSurface,
			profile))
	{
		return false;
	}

	flags = 0;
	if (profile.direction == nullptr || !hasWorldGrid(grid))
		return false;

	for (std::uint32_t index = 0;
		 index < profile.direction->ubNumTiles;
		 ++index)
	{
		std::int32_t profileTileGrid = 0;
		if (profileGrid(
				profile.baseGrid,
				profile.direction->pTiles[index],
				profileTileGrid) &&
			profileTileGrid == grid)
		{
			flags = profile.direction->pTiles[index].usTileFlags;
			return true;
		}
	}
	return false;
}

LEVELNODE* TacticalActorAnimationFootprint::nextWorldNode(
	std::int32_t grid,
	std::uint16_t& flags,
	TacticalActor*& actor,
	LEVELNODE* previous) noexcept
{
	flags = 0;
	actor = nullptr;
	if (!hasWorldGrid(grid))
		return nullptr;

	LEVELNODE* node = GetMapElement(
		static_cast<std::uint32_t>(grid)).pMercHead;
	if (previous != nullptr)
	{
		while (node != nullptr && node != previous)
			node = node->pNext;
		if (node == nullptr)
			return nullptr;
		node = node->pNext;
	}

	if (node != nullptr &&
		(node->uiFlags & LEVELNODE_MERCPLACEHOLDER))
	{
		flags = static_cast<std::uint16_t>(
			node->uiAnimHitLocationFlags);
		actor = node->pSoldier;
	}
	return node;
}
