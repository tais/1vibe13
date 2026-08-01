#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorRouteExecution
{
	enum class PathOrigin : std::uint8_t
	{
		System = 0,
		PlayerUi = 1,
		ContinueMovement = 2,
		TeamAwareUi = 3
	};

	[[nodiscard]] bool setOutOfActionPoints(
		TacticalActor& actor,
		bool stopped);
	[[nodiscard]] bool requestPath(
		TacticalActor& actor,
		std::int32_t destinationGrid,
		std::uint16_t movementAnimation,
		PathOrigin origin = PathOrigin::System,
		bool forceRestart = true);
	[[nodiscard]] bool stop(TacticalActor& actor);
	[[nodiscard]] bool settleIntoStationaryStance(
		TacticalActor& actor);
	[[nodiscard]] bool haltForSighting(
		TacticalActor& actor,
		bool sightingEnemy);
	[[nodiscard]] bool stopAt(
		TacticalActor& actor,
		std::int32_t gridNo,
		std::int8_t direction);
}
