#pragma once

#include "types.h"

class TacticalActor;

namespace TacticalActorLighting
{
	[[nodiscard]] bool createPersonalLight(TacticalActor& actor);
	[[nodiscard]] bool recreatePersonalLight(TacticalActor& actor);
	[[nodiscard]] bool destroyPersonalLight(TacticalActor& actor) noexcept;
	[[nodiscard]] bool positionPersonalLight(TacticalActor& actor);
	// Read-only preflight plus checked equipment continuation. Disabled/daylight
	// lighting is a successful no-op after retiring an old personal light;
	// failure to create/position a required light is not silently swallowed.
	[[nodiscard]] bool canRefreshEquipmentPersonalLight(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool refreshEquipmentPersonalLight(TacticalActor& actor);
	[[nodiscard]] bool setPersonalLightLevel(TacticalActor& actor) noexcept;
}

void HandlePlayerTogglingLightEffects(BOOLEAN toggleValue);
