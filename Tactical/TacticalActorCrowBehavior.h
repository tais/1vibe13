#pragma once

#include <cstdint>

class TacticalActor;

// Legacy shadow adapters retained while animation and world-placement callers
// move behind the focused crow-behavior service.
void HandleCrowShadowVisibility(TacticalActor* actor);
void HandleCrowShadowNewGridNo(TacticalActor* actor);
void HandleCrowShadowRemoveGridNo(TacticalActor* actor);
void HandleCrowShadowNewDirection(TacticalActor* actor);
void HandleCrowShadowNewPosition(TacticalActor* actor);

// Orders every active crow on the selected tactical team to take flight.
void CrowsFlyAway(std::uint8_t team);
