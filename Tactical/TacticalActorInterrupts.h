#pragma once

#include "types.h"

class TacticalActor;

BOOLEAN ResolvePendingInterrupt(
	TacticalActor* actor, UINT8 interruptType);
