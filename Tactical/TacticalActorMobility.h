#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorMobility
{
	[[nodiscard]] bool inWater(
		const TacticalActor& actor) noexcept;
	[[nodiscard]] bool inShallowWater(
		const TacticalActor& actor) noexcept;
	[[nodiscard]] bool inDeepWater(
		const TacticalActor& actor) noexcept;
	[[nodiscard]] bool inHighWater(
		const TacticalActor& actor) noexcept;

	[[nodiscard]] std::uint16_t movementStateForStance(
		TacticalActor& actor,
		std::uint8_t stance);
	[[nodiscard]] std::uint16_t movementStateForCurrentStance(
		TacticalActor& actor);
	[[nodiscard]] std::uint16_t transitionStateForStance(
		const TacticalActor& actor,
		std::uint8_t desiredStance) noexcept;
	[[nodiscard]] bool canClimbWithCurrentBackpack(
		TacticalActor& actor);
	[[nodiscard]] bool isValidStance(
		TacticalActor& actor,
		std::int8_t direction,
		std::int8_t stance);
	[[nodiscard]] bool isValidStance(
		TacticalActor& actor,
		std::int8_t stance);
	[[nodiscard]] bool isCurrentStanceValid(
		TacticalActor& actor,
		std::int8_t direction);
	[[nodiscard]] bool isValidMovementMode(
		const TacticalActor& actor,
		std::uint16_t movementMode) noexcept;
	[[nodiscard]] bool selectMovementForCurrentStance(
		TacticalActor& actor);
	[[nodiscard]] bool isCrouchedAgainstCover(
		const TacticalActor& actor,
		std::uint8_t direction);
	[[nodiscard]] bool isFastMovement(
		TacticalActor& actor);
}
