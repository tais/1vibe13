#pragma once

#include <cstdint>

class TacticalActor;

// Legacy override shared by the focused animation and combat domains.
extern std::uint16_t usForceAnimState;

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
