#pragma once

#include "types.h"

class TacticalActor;

namespace TacticalActorLighting
{
	[[nodiscard]] bool createPersonalLight(TacticalActor& actor);
	[[nodiscard]] bool recreatePersonalLight(TacticalActor& actor);
	[[nodiscard]] bool destroyPersonalLight(TacticalActor& actor) noexcept;
	[[nodiscard]] bool positionPersonalLight(TacticalActor& actor);
	[[nodiscard]] bool setPersonalLightLevel(TacticalActor& actor) noexcept;
}

void HandlePlayerTogglingLightEffects(BOOLEAN toggleValue);
