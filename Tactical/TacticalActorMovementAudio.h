#pragma once

#include "types.h"

class TacticalActor;

namespace TacticalActorMovementAudio
{
	void setVehicleMovement(TacticalActor& actor, bool enabled);
	void playFootstep(TacticalActor& actor, bool ignoreStealth = false);
}

// Compatibility adapters for legacy animation and overhead callers.
void HandleVehicleMovementSound(TacticalActor* actor, BOOLEAN enabled);
void PlaySoldierFootstepSound(TacticalActor* actor);
void PlayStealthySoldierFootstepSound(TacticalActor* actor);
