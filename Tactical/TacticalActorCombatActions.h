#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorCombatActions
{
	[[nodiscard]] bool beginBladeAttack(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool beginPunchAttack(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool beginKnifeThrow(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool continueNinjaAttack(
		TacticalActor& actor);
}
