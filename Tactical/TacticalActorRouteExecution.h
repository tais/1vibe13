#pragma once

#include <cstdint>
#include <Engine/Adapters/JA2/TacticalMoveDiagnostic.h>

class TacticalActor;

// Legacy UI path-through-people request owned by route execution.
extern std::uint8_t gfGetNewPathThroughPeople;

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
		bool stopped,
		bool replicate = true);
	// Stock halts a moving merc mid-stride after a sighting or an interrupt
	// (the no-AP-to-finish flag): frozen on one moving frame with no route
	// left, it waits for its next order. A stable state: the save carries it
	// exactly, and a load must not execute its next animation instruction.
	[[nodiscard]] bool haltedMidStride(const TacticalActor& actor) noexcept;
	// Dedicated co-op admission and immediate execution revalidation. This
	// checks a fresh ordinary route without changing actor, path policy or RNG.
	[[nodiscard]] bool canBeginMoveToGrid(
		TacticalActor& actor, std::int32_t destinationGrid,
		std::uint16_t movementMode, bool reverse,
		TacticalMoveDiagnostic* diagnostic = nullptr) noexcept;
	[[nodiscard]] bool requestPath(
		TacticalActor& actor,
		std::int32_t destinationGrid,
		std::uint16_t movementAnimation,
		PathOrigin origin = PathOrigin::System,
		bool forceRestart = true,
		bool replicate = true);
	[[nodiscard]] bool continueMovement(TacticalActor& actor);
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
