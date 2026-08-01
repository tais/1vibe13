#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorWorldPlacement
{
	[[nodiscard]] bool removeFromGrid(
		TacticalActor& actor,
		bool force = false);
	[[nodiscard]] bool setPosition(
		TacticalActor& actor,
		float worldX,
		float worldY,
		bool updateDestination = true,
		bool updateFinalDestination = true,
		bool forceRemove = false);
	[[nodiscard]] bool setHeight(
		TacticalActor& actor,
		float height,
		bool updateLevel = true);
	[[nodiscard]] bool setGrid(
		TacticalActor& actor,
		std::int32_t gridNo,
		bool forceRemove = false);
}
