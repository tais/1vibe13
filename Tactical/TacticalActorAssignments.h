#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorAssignments
{
	[[nodiscard]] std::int8_t sleepBreathRegeneration(
		TacticalActor& actor);
	[[nodiscard]] float burialPoints(
		TacticalActor& actor,
		std::uint16_t* corpseCount = nullptr);
	[[nodiscard]] float constructionPoints(TacticalActor& actor);

	[[nodiscard]] std::uint32_t administrationPoints(
		TacticalActor& actor);
	[[nodiscard]] float administrationModifier(const TacticalActor& actor);
	[[nodiscard]] std::uint32_t explorationPoints(TacticalActor& actor);
}
