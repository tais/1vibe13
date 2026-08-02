#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorDamageFeedback
{
	[[nodiscard]] bool presentHit(TacticalActor& actor);
	[[nodiscard]] std::uint8_t calculateScreamVolume(
		TacticalActor& actor,
		std::uint8_t combinedLoss);
	void applyGenericHit(
		TacticalActor& actor,
		std::uint8_t special,
		std::int16_t direction);
	void applyGunfireHit(
		TacticalActor& actor,
		std::uint16_t weaponIndex,
		std::int16_t damage,
		std::uint16_t direction,
		std::uint16_t range,
		SoldierID attackerId,
		std::uint8_t special,
		std::uint8_t hitLocation);
	void applyExplosionHit(
		TacticalActor& actor,
		std::uint16_t weaponIndex,
		std::int16_t damage,
		std::uint16_t direction,
		std::uint16_t range,
		SoldierID attackerId,
		std::uint8_t special,
		std::uint8_t hitLocation);
	void applyBladeHit(
		TacticalActor& actor,
		std::uint8_t hitLocation);
	void applyPunchHit(
		TacticalActor& actor,
		std::uint16_t weaponIndex,
		std::int16_t damage,
		std::uint16_t direction,
		std::uint16_t range,
		SoldierID attackerId,
		std::uint8_t special,
		std::uint8_t hitLocation);
	void applyVehicleHit(
		TacticalActor& actor,
		std::uint16_t direction);
	void setDamageDisplayCounter(TacticalActor& actor);
}
