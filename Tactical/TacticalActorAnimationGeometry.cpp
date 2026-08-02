#include "TacticalActorAnimationGeometry.h"

#include "Animation Control.h"
#include "Animation Data.h"
#include "Items.h"
#include "TacticalActor.h"

#include <cstddef>

namespace
{
bool hasBoundedAnimationResolverState(
	const TacticalActor& actor) noexcept
{
	if (actor.animationPlayback().state() >= NUMANIMATIONSTATES ||
		actor.identity().bodyType() >= TOTALBODYTYPES)
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
}

bool TacticalActorAnimationGeometry::currentFrame(
	TacticalActor& actor,
	FrameGeometry& geometry)
{
	geometry = {};
	if (!hasBoundedAnimationResolverState(actor))
		return false;

	const UINT16 animationSurface = GetSoldierAnimationSurface(
		&actor,
		actor.animationPlayback().state());
	if (animationSurface == INVALID_ANIMATION_SURFACE ||
		animationSurface >= NUMANIMATIONSURFACETYPES)
	{
		return false;
	}

	const AnimationSurfaceType& surface =
		gAnimSurfaceDatabase[animationSurface];
	if (surface.hVideoObject == nullptr ||
		surface.hVideoObject->pETRLEObject == nullptr ||
		actor.animationPlayback().frame() >=
			surface.hVideoObject->usNumberOfObjects)
	{
		return false;
	}

	const ETRLEObject& frame =
		surface.hVideoObject->pETRLEObject[
			actor.animationPlayback().frame()];
	geometry.width = static_cast<std::int16_t>(frame.usWidth);
	geometry.height = static_cast<std::int16_t>(frame.usHeight);
	geometry.offsetX = static_cast<std::int16_t>(frame.sOffsetX);
	geometry.offsetY = static_cast<std::int16_t>(frame.sOffsetY);
	return true;
}

bool TacticalActorAnimationGeometry::refreshBoundingBox(
	TacticalActor& actor)
{
	FrameGeometry geometry;
	if (!currentFrame(actor, geometry))
		return false;
	actor.renderState().setBoundingBox(
		geometry.width,
		geometry.height,
		geometry.offsetX,
		geometry.offsetY);
	return true;
}
