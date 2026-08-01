#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorAnimationTransitions
{
	bool changeState(
		TacticalActor& actor,
		std::uint16_t animationState,
		std::uint16_t startingCode,
		bool force);
	bool initializeAnimation(
		TacticalActor& actor,
		std::uint16_t animationState,
		std::uint16_t startingCode,
		bool force);
}
