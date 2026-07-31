#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorCombatReactions
{
	[[nodiscard]] bool beginFall(TacticalActor& actor);
	[[nodiscard]] bool beginFlyback(
		TacticalActor& actor,
		std::uint8_t impactDirection);
	[[nodiscard]] bool beginFallback(
		TacticalActor& actor,
		std::uint8_t impactDirection);
}
