#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorLifecycle
{
	// preserveSavedIdle is reserved for the copied existing-record load path;
	// it verifies that the reconstruction arguments match that exact actor.
	[[nodiscard]] bool create(
		TacticalActor& actor,
		std::uint8_t bodyType,
		SoldierID soldierId,
		std::uint16_t animationState,
		bool preserveSavedIdle = false);
	[[nodiscard]] bool destroy(TacticalActor& actor);
	void revive(TacticalActor& actor);
	void revivePlayerTeam();
}

// Legacy adapter retained for source and link compatibility.
void RevivePlayerTeam();
