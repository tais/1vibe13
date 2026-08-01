#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorOrientation
{
	[[nodiscard]] bool changeStance(
		TacticalActor& actor,
		std::uint8_t desiredStance);
	[[nodiscard]] bool setMovementDestination(
		TacticalActor& actor,
		std::uint8_t direction);
	[[nodiscard]] bool setMovementDestination(
		TacticalActor& actor,
		std::uint8_t direction,
		bool initialMove,
		std::uint16_t animationState);
	[[nodiscard]] bool setDesiredDirection(
		TacticalActor& actor,
		std::uint8_t direction);
	[[nodiscard]] bool setDesiredDirection(
		TacticalActor& actor,
		std::uint8_t direction,
		bool initialMove,
		std::uint16_t animationState);
	[[nodiscard]] bool setDirection(
		TacticalActor& actor,
		std::uint8_t direction);
	[[nodiscard]] bool advanceTurn(TacticalActor& actor);
}
