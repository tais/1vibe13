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
	// Load-only STANDING/CROUCHING/PRONE asset binding. Leaves the saved
	// script cursor, displayed frame and animation clock untouched.
	bool restoreSavedIdlePresentation(TacticalActor& actor);
	bool initializeAnimation(
		TacticalActor& actor,
		std::uint16_t animationState,
		std::uint16_t startingCode,
		bool force);
}
