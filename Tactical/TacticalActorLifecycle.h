#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorLifecycle
{
	[[nodiscard]] bool create(
		TacticalActor& actor,
		std::uint8_t bodyType,
		SoldierID soldierId,
		std::uint16_t animationState);
	[[nodiscard]] bool destroy(TacticalActor& actor);
	void revive(TacticalActor& actor);
	void revivePlayerTeam();
}

// Legacy adapter retained for source and link compatibility.
void RevivePlayerTeam();
