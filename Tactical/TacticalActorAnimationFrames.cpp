#include "TacticalActorAnimationFrames.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "Isometric Utils.h"
#include "Soldier Control.h"

#include <array>
#include <cstdint>

namespace
{
constexpr std::uint8_t ExtendedDirectionCount = 32;
constexpr std::array<std::uint8_t, NUM_WORLD_DIRECTIONS>
	DirectionFromEightToTwo{0, 0, 1, 1, 0, 1, 1, 0};

bool currentSurface(
	TacticalActor& actor,
	std::uint16_t& surface)
{
	if (actor.animationPlayback().state() >= NUMANIMATIONSTATES)
		return false;

	surface = actor.animationPlayback().surface();
	if (surface == INVALID_ANIMATION_SURFACE ||
		surface >= NUMANIMATIONSURFACETYPES)
	{
		return false;
	}

	surface = GetSoldierAnimationSurface(
		&actor,
		actor.animationPlayback().state());
	return surface != INVALID_ANIMATION_SURFACE &&
		surface < NUMANIMATIONSURFACETYPES;
}
}

bool TacticalActorAnimationFrames::spriteDirectionForSurface(
	const TacticalActor& actor,
	std::uint16_t animationSurface,
	std::uint8_t& direction) noexcept
{
	if (animationSurface >= NUMANIMATIONSURFACETYPES ||
		actor.position().direction() >= NUM_WORLD_DIRECTIONS)
	{
		return false;
	}

	std::uint8_t resolvedDirection =
		gOneCDirection[actor.position().direction()];
	const std::uint32_t directionCount =
		gAnimSurfaceDatabase[animationSurface].uiNumDirections;
	if (directionCount == ExtendedDirectionCount)
	{
		if (actor.movement().highResolutionDirection() >=
			ExtendedDirectionCount)
		{
			return false;
		}
		resolvedDirection = static_cast<std::uint8_t>(
			(actor.movement().highResolutionDirection() + 4) %
			ExtendedDirectionCount);
	}
	else if (directionCount == 4)
	{
		resolvedDirection /= 2;
	}
	else if (directionCount == 1)
	{
		resolvedDirection = 0;
	}
	else if (directionCount == 3)
	{
		if (actor.position().direction() == NORTHWEST)
			resolvedDirection = 1;
		else if (actor.position().direction() == WEST)
			resolvedDirection = 0;
		else if (actor.position().direction() == EAST)
			resolvedDirection = 2;
	}
	else if (directionCount == 2)
	{
		resolvedDirection = DirectionFromEightToTwo[
			actor.position().direction()];
	}
	direction = resolvedDirection;
	return true;
}

std::uint16_t TacticalActorAnimationFrames::frozenFrame(
	TacticalActor& actor)
{
	std::uint16_t surface = INVALID_ANIMATION_SURFACE;
	if (!currentSurface(actor, surface))
		return 0;

	const AnimationSurfaceType& animationSurface =
		gAnimSurfaceDatabase[surface];
	if (animationSurface.hVideoObject == nullptr ||
		(animationSurface.ubFlags & ANIM_DATA_FLAG_NOFRAMES))
	{
		return 0;
	}

	std::uint8_t direction = 0;
	if (!spriteDirectionForSurface(
			actor,
			surface,
			direction))
	{
		return 0;
	}

	const std::uint16_t frame = static_cast<std::uint16_t>(
		animationSurface.uiNumFramesPerDir * direction);
	return frame < animationSurface.hVideoObject->usNumberOfObjects
		? frame : 0;
}

bool TacticalActorAnimationFrames::selectFrame(
	TacticalActor& actor,
	std::uint16_t animationFrame)
{
	std::uint16_t surface = INVALID_ANIMATION_SURFACE;
	if (!currentSurface(actor, surface))
		return false;

	AnimationSurfaceType& animationSurface =
		gAnimSurfaceDatabase[surface];
	if (animationSurface.ubFlags & ANIM_DATA_FLAG_NOFRAMES)
		animationFrame = 0;

	std::uint8_t direction = 0;
	if (!spriteDirectionForSurface(
			actor,
			surface,
			direction))
	{
		return false;
	}

	actor.animationPlayback().frame() =
		animationFrame + static_cast<std::uint16_t>(
			animationSurface.uiNumFramesPerDir * direction);
	if (animationSurface.hVideoObject == nullptr)
	{
		actor.animationPlayback().frame() = 0;
		return true;
	}
	if (actor.animationPlayback().frame() >=
		animationSurface.hVideoObject->usNumberOfObjects)
	{
		actor.animationPlayback().frame() = 0;
	}
	return true;
}
