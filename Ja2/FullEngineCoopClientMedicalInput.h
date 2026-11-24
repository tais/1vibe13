#ifndef JA2_FULL_ENGINE_COOP_CLIENT_MEDICAL_INPUT_H
#define JA2_FULL_ENGINE_COOP_CLIENT_MEDICAL_INPUT_H

#include "FullEngineCoopClientController.h"
struct InputAtom;

struct FullEngineCoopClientMedicalInputResult
{
	bool consumed = false;
	FullEngineCoopClientIntentRequest request;
};

// Shipping Screen router for already queued native input atoms. Window-close
// SDL events stay in the application event pump, outside this modal consumer.
FullEngineCoopClientMedicalInputResult RouteFullEngineCoopClientMedicalInput(
	FullEngineCoopClientController& controller,
	const FullEngineCoopClientControllerView& view,
	const InputAtom& event, bool modalAtFrameStart = false) noexcept;

// On a frame that changes from medical selection to another input owner,
// discard all already queued atoms before campaign/notice controls can see them.
// Returns true exactly when it consumed that medical handoff frame.
bool ConsumeFullEngineCoopClientMedicalOwnerChange(
	FullEngineCoopClientController& controller, bool medicalAtFrameStart) noexcept;

#endif
