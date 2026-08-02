#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorAnimationGeometry
{
	struct FrameGeometry
	{
		std::int16_t width = 0;
		std::int16_t height = 0;
		std::int16_t offsetX = 0;
		std::int16_t offsetY = 0;
	};

	[[nodiscard]] bool currentFrame(
		TacticalActor& actor,
		FrameGeometry& geometry);
	[[nodiscard]] bool refreshBoundingBox(TacticalActor& actor);
}
