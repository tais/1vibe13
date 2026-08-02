#pragma once

#include "types.h"

#include <cstdint>

class OBJECTTYPE;
class TacticalActor;
struct SoldierID;

namespace TacticalActorEquipment
{
	[[nodiscard]] bool carriesTwoHandedWeapon(
		const TacticalActor& actor);
	[[nodiscard]] OBJECTTYPE* usedWeapon(
		const TacticalActor& actor,
		OBJECTTYPE* object);
	[[nodiscard]] std::uint16_t usedWeaponNumber(
		const TacticalActor& actor,
		OBJECTTYPE* object);

	[[nodiscard]] bool externalFeeding(
		TacticalActor& actor,
		SoldierID* firstActorId,
		std::uint16_t* firstGunSlot,
		std::uint16_t* firstAmmoSlot,
		SoldierID* secondActorId,
		std::uint16_t* secondGunSlot,
		std::uint16_t* secondAmmoSlot);

	[[nodiscard]] OBJECTTYPE* objectWithFlag(
		TacticalActor& actor,
		std::uint64_t flag);
	[[nodiscard]] bool usesScubaGear(const TacticalActor& actor);
	[[nodiscard]] std::uint8_t bestEquippedFlashlightRange(
		TacticalActor& actor);

	[[nodiscard]] bool hasItem(
		const TacticalActor& actor,
		std::uint16_t item);
	[[nodiscard]] bool hasMortar(const TacticalActor& actor);
	[[nodiscard]] bool hasSniperRifle(const TacticalActor& actor);
	[[nodiscard]] OBJECTTYPE* equippedRiotShield(TacticalActor& actor);
	[[nodiscard]] bool hasEquippedRiotShield(TacticalActor& actor);
	[[nodiscard]] bool wearsUsableGasMask(TacticalActor& actor);

	void coolDownInventory(TacticalActor& actor);
	bool dropSectorEquipment(TacticalActor& actor);
	bool takeItemIntoHand(
		TacticalActor& actor,
		std::uint16_t item);
	bool takeBombIntoHand(
		TacticalActor& actor,
		std::uint16_t item);
	bool switchWeapon(
		TacticalActor& actor,
		bool knife = false,
		bool sidearm = false);

	void refreshFlashlights(TacticalActor& actor);
	bool damageRiotShield(
		TacticalActor& actor,
		std::int32_t damage);
	bool removeOneItem(
		TacticalActor& actor,
		std::uint16_t item);
}

BOOLEAN DoesSoldierWearGasMask(TacticalActor* actor);
