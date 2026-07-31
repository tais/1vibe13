#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorTurnBudget
{
	[[nodiscard]] std::int16_t calculateTurnGrant(
		TacticalActor& actor);
	[[nodiscard]] bool refreshForTurn(
		TacticalActor& actor);
}
