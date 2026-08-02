#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

enum
{
	TAKE_DAMAGE_GUNFIRE = 1,
	TAKE_DAMAGE_BLADE,
	TAKE_DAMAGE_HANDTOHAND,
	TAKE_DAMAGE_FALLROOF,
	TAKE_DAMAGE_BLOODLOSS,
	TAKE_DAMAGE_EXPLOSION,
	TAKE_DAMAGE_ELECTRICITY,
	TAKE_DAMAGE_GAS_FIRE,
	TAKE_DAMAGE_TENTACLES,
	TAKE_DAMAGE_STRUCTURE_EXPLOSION,
	TAKE_DAMAGE_OBJECT,
	TAKE_DAMAGE_VEHICLE_TRAUMA,
	TAKE_DAMAGE_GAS_NOTFIRE,
};

void HandleTakeDamageDeath(
	TacticalActor* actor, std::uint8_t oldLife, std::uint8_t reason);

namespace TacticalActorDamageResolution
{
	void applyHit(
		TacticalActor& actor,
		std::uint16_t weaponIndex,
		std::int16_t damage,
		std::int16_t breathLoss,
		std::uint16_t direction,
		std::uint16_t range,
		SoldierID attackerId,
		std::uint8_t special,
		std::uint8_t hitLocation,
		std::int16_t subsequent,
		std::int32_t locationGrid);
	std::uint8_t takeDamage(
		TacticalActor& actor,
		std::int8_t height,
		std::int16_t lifeDeduct,
		std::int16_t breathLoss,
		std::uint8_t reason,
		SoldierID attackerId,
		std::int32_t sourceGrid,
		std::int16_t subsequent,
		bool showDamage);
}
