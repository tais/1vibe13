#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorBattleSounds
{
	bool play(
		TacticalActor& actor,
		std::uint8_t soundId);
	bool playWithCode(
		TacticalActor& actor,
		std::uint8_t soundId,
		std::int8_t specialCode);
}
