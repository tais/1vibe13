#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorAnimationFrames
{
	[[nodiscard]] bool spriteDirectionForSurface(
		const TacticalActor& actor,
		std::uint16_t animationSurface,
		std::uint8_t& direction) noexcept;
	[[nodiscard]] std::uint16_t frozenFrame(
		TacticalActor& actor);
	[[nodiscard]] bool selectFrame(
		TacticalActor& actor,
		std::uint16_t animationFrame);
}
