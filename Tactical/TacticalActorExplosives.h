#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorExplosives
{
	void degradeInventoryAfterExplosion(TacticalActor& actor);
	void applyInventoryExplosion(TacticalActor& actor);

	[[nodiscard]] bool selfDetonate(TacticalActor& actor);
	[[nodiscard]] bool beginBombPlacement(TacticalActor& actor);
	[[nodiscard]] bool beginTripwireDisarm(
		TacticalActor& actor,
		std::int32_t gridNo,
		std::int32_t worldItemIndex);
	[[nodiscard]] bool beginDetonatorUse(TacticalActor& actor);
}
