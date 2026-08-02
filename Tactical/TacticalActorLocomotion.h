#pragma once

#include "types.h"

class TacticalActor;

// Legacy adapter used by animation code while facing-based movement is
// migrated behind the locomotion service.
void MoveMercFacingDirection(
	TacticalActor* actor, BOOLEAN reverse, FLOAT movementDistance);

namespace TacticalActorLocomotion
{
	[[nodiscard]] bool checkRoofHit(TacticalActor& actor);
	void move(
		TacticalActor& actor,
		float movementChange,
		float angle,
		bool checkRange);
}
