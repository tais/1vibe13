#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorSpotting
{
	[[nodiscard]] bool isSpotting(TacticalActor& actor);
	[[nodiscard]] bool canSpot(
		TacticalActor& actor,
		std::int32_t targetGridNo = -1);
	[[nodiscard]] bool startSpotting(
		TacticalActor& actor,
		std::int32_t targetGridNo);

	[[nodiscard]] std::uint16_t chanceToHitBonus(
		TacticalActor* sniper,
		std::int32_t targetGridNo,
		std::int8_t team);
}
