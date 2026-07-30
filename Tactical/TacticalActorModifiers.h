#pragma once

#include <cstdint>
#include <vector>

class TacticalActor;
enum class BackgroundVectorTypes;

namespace TacticalActorModifiers
{
	[[nodiscard]] bool hasBackgroundFlag(
		const TacticalActor& actor,
		std::uint64_t flag);
	[[nodiscard]] std::int16_t backgroundValue(
		const TacticalActor& actor,
		std::uint16_t property);
	[[nodiscard]] const std::vector<std::int16_t>& backgroundValues(
		const TacticalActor& actor,
		BackgroundVectorTypes property);

	[[nodiscard]] std::int8_t suppressionResistanceBonus(
		const TacticalActor& actor);
	[[nodiscard]] std::int16_t meleeDamageBonus(
		const TacticalActor& actor);
	[[nodiscard]] std::int16_t actionPointBonus(
		const TacticalActor& actor);
	[[nodiscard]] std::int8_t fearResistanceBonus(
		const TacticalActor& actor);
	[[nodiscard]] float moraleModifier(const TacticalActor& actor);
	[[nodiscard]] std::int16_t interruptModifier(
		TacticalActor& actor);
}
