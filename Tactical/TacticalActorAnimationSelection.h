#pragma once

#include "types.h"

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
	[[nodiscard]] bool useAlternativeBigMercAnimation(
		const TacticalActor& actor) noexcept;
	[[nodiscard]] std::uint16_t suspiciousActionPointDuration(
		std::uint16_t animation) noexcept;
}

BOOLEAN DecideAltAnimForBigMerc(TacticalActor* actor);
UINT16 GetSuspiciousAnimationAPDuration(UINT16 animation);
