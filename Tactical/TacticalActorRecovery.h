#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorRecovery
{
	[[nodiscard]] bool applySleepDart(
		TacticalActor& actor,
		std::int16_t& breathLoss);
	[[nodiscard]] bool checkBreathCollapse(
		TacticalActor& actor);
	[[nodiscard]] bool collapse(TacticalActor& actor);
	[[nodiscard]] bool beginGetUp(TacticalActor& actor);
}
