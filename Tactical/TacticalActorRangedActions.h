#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorRangedActions
{
	[[nodiscard]] bool beginFire(
		TacticalActor& actor,
		std::int32_t targetGridNo);
	[[nodiscard]] bool ready(
		TacticalActor& actor);
	[[nodiscard]] bool readyToward(
		TacticalActor& actor,
		std::int16_t targetX,
		std::int16_t targetY,
		bool endReady,
		bool raiseToHipOnly);
	[[nodiscard]] bool readyFacing(
		TacticalActor& actor,
		std::uint8_t facingDirection,
		bool endReady,
		bool raiseToHipOnly);
	[[nodiscard]] bool refreshAfterHandItemChange(
		TacticalActor& actor,
		std::uint16_t oldItem,
		std::uint16_t newItem);
	[[nodiscard]] bool canRefreshAfterHandItemChange(
		const TacticalActor& actor,
		std::uint16_t oldItem,
		std::uint16_t newItem) noexcept;
}
