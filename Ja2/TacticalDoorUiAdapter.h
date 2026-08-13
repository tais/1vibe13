#ifndef JA2_TACTICAL_DOOR_UI_ADAPTER_H
#define JA2_TACTICAL_DOOR_UI_ADAPTER_H

#include <cstdint>

#include <Engine/Adapters/JA2/TacticalDoorUiSession.h>

class TacticalActor;
struct TAG_STRUCTURE;

// Pointer-bearing compatibility edge for the legacy door UI. The caller owns
// the EngineRuntime session explicitly; this adapter does not publish a second
// process-global mirror of the retained modal state.
bool CaptureJa2TacticalDoorUiContext(
	TacticalDoorUiSession& session,
	const TacticalActor& actor,
	const TAG_STRUCTURE& structure,
	std::uint8_t direction,
	bool closingDoor) noexcept;

bool ResolveJa2TacticalDoorUiContext(
	const TacticalDoorUiSession& session,
	TacticalActor*& actor,
	TAG_STRUCTURE*& structure) noexcept;

// Cleanup may release the captured actor-owned command continuation even when
// the door disappeared. It requires the exact world generation, incarnation,
// pending action, and world-object operation so newer actor work is preserved.
TacticalActor* ResolveJa2TacticalDoorUiActorForCleanup(
	const TacticalDoorUiSession& session) noexcept;

#endif
