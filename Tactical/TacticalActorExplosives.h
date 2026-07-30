#pragma once

class TacticalActor;

namespace TacticalActorExplosives
{
	void degradeInventoryAfterExplosion(TacticalActor& actor);
	void applyInventoryExplosion(TacticalActor& actor);

	[[nodiscard]] bool selfDetonate(TacticalActor& actor);
}
