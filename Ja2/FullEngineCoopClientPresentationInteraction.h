#ifndef JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_INTERACTION_H
#define JA2_FULL_ENGINE_COOP_CLIENT_PRESENTATION_INTERACTION_H

#include "FullEngineCoopClientController.h"
#include "FullEngineCoopClientPresentationInput.h"

#include <cstdint>

enum class FullEngineCoopClientPresentationInteractionKind : std::uint8_t
{
	Ignored,
	ActorSelected,
	TargetSelected,
	DoorSelected,
	InventoryOpened,
	IntentReady
};

struct FullEngineCoopClientPresentationInteractionResult
{
	FullEngineCoopClientPresentationInteractionKind kind =
		FullEngineCoopClientPresentationInteractionKind::Ignored;
	FullEngineCoopClientIntentRequest intent;
};

// Applies one already-resolved native presentation click to local controller
// state. The borrowed controller view is the sole snapshot/assignment source.
// IntentReady returns a typed request for the caller to submit; this seam never
// performs network I/O or predicts any result into the committed replica.
FullEngineCoopClientPresentationInteractionResult
DispatchFullEngineCoopClientPresentationClick(
	FullEngineCoopClientController& controller,
	const FullEngineCoopClientControllerView& view,
	const FullEngineCoopClientPresentationClick& click) noexcept;

#endif
