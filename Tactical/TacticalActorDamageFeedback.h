#pragma once

class TacticalActor;

void SetDamageDisplayCounter(TacticalActor* actor);

namespace TacticalActorDamageFeedback
{
	[[nodiscard]] bool presentHit(TacticalActor& actor);
}
