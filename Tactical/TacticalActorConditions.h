#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorConditions
{
	inline constexpr std::int16_t bloodDonationAmount = 10;

	[[nodiscard]] bool isZombie(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isAssassin(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool canBeCaptured(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool canDonateBlood(TacticalActor& actor);

	[[nodiscard]] bool isCowering(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isUnconscious(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isGivingAid(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool hasTakenLargeHit(const TacticalActor& actor) noexcept;
	[[nodiscard]] std::uint8_t suppressionShockPercent(const TacticalActor& actor) noexcept;
}
