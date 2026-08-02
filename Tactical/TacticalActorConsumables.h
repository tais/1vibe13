#pragma once

#include "types.h"

class TacticalActor;
class OBJECTTYPE;

BOOLEAN ApplyConsumable(
	TacticalActor* actor,
	OBJECTTYPE* object,
	BOOLEAN force,
	BOOLEAN useActionPoints);

namespace TacticalActorConsumables
{
	[[nodiscard]] bool autoUseDrug(TacticalActor& actor);
}
