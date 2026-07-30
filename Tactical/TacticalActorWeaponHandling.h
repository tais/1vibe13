#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorWeaponHandling
{
	[[nodiscard]] bool isValidSecondHandShot(
		TacticalActor& actor);
	[[nodiscard]] bool isValidSecondHandBurst(
		TacticalActor& actor);
	[[nodiscard]] bool isValidSecondHandShotForReloading(
		TacticalActor& actor);

	[[nodiscard]] bool isValidAlternativeFireMode(
		TacticalActor& actor,
		std::int16_t aimTime,
		std::int32_t targetGridNo);
	[[nodiscard]] bool isValidShotFromHip(
		TacticalActor& actor,
		std::int16_t aimTime,
		std::int32_t targetGridNo);
	[[nodiscard]] bool isValidPistolFastShot(
		TacticalActor& actor,
		std::int16_t aimTime,
		std::int32_t targetGridNo);

	[[nodiscard]] bool isWeaponMounted(
		TacticalActor& actor);
}
