#pragma once

class TacticalActor;

namespace TacticalActorLocomotion
{
	[[nodiscard]] bool checkRoofHit(TacticalActor& actor);
	void move(
		TacticalActor& actor,
		float movementChange,
		float angle,
		bool checkRange);
}
