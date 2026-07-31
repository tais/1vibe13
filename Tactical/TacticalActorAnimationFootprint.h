#pragma once

#include <cstdint>

struct LEVELNODE;
class TacticalActor;

namespace TacticalActorAnimationFootprint
{
	[[nodiscard]] bool add(
		TacticalActor& actor,
		std::uint16_t animationState);
	[[nodiscard]] bool addForSurface(
		TacticalActor& actor,
		std::uint16_t animationState,
		std::uint16_t animationSurface);
	[[nodiscard]] bool remove(
		TacticalActor& actor,
		std::uint16_t animationState);
	[[nodiscard]] bool flagsAtGrid(
		TacticalActor& actor,
		std::uint16_t animationState,
		std::int32_t grid,
		std::uint16_t& flags);
	[[nodiscard]] LEVELNODE* nextWorldNode(
		std::int32_t grid,
		std::uint16_t& flags,
		TacticalActor*& actor,
		LEVELNODE* previous = nullptr) noexcept;
}
