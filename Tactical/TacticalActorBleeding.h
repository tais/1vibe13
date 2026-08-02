#pragma once

#include "types.h"

class TacticalActor;

namespace TacticalActorBleeding
{
	[[nodiscard]] INT32 check(TacticalActor& actor);
	[[nodiscard]] FLOAT nextInterval(const TacticalActor& actor);
	[[nodiscard]] FLOAT nextUnmovingInterval(const TacticalActor& actor);
}

// Compatibility adapters retained for legacy medical calculations.
FLOAT CalcSoldierNextBleed(TacticalActor* actor);
FLOAT CalcSoldierNextUnmovingBleed(TacticalActor* actor);
