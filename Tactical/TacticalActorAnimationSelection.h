#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorAnimationSelection
{
	[[nodiscard]] std::uint16_t selectFire(
		TacticalActor& actor,
		std::uint8_t height);
	void selectFall(TacticalActor& actor);
	[[nodiscard]] std::uint16_t pickReady(
		TacticalActor& actor,
		bool endReady,
		bool alternativeWeaponHolding);
}
