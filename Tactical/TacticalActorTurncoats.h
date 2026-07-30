#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorTurncoats
{
	[[nodiscard]] bool inPositionForAttempt(
		TacticalActor& actor,
		SoldierID target);
	[[nodiscard]] std::uint8_t convictionChance(
		TacticalActor& actor,
		SoldierID target,
		std::int16_t approach);
	void attempt(SoldierID target);
	[[nodiscard]] bool orderOne(SoldierID target);
	void orderAll();
}
