#include "FullEngineCoopClientMedicalInput.h"
#include "input.h"
#include "english.h"
#include "sgp.h"

FullEngineCoopClientMedicalInputResult RouteFullEngineCoopClientMedicalInput(
	FullEngineCoopClientController& controller,
	const FullEngineCoopClientControllerView& view,
	const InputAtom& event, bool modalAtFrameStart) noexcept
{
	// Remember the modal boundary even if this fresh view revokes its patient.
	// The atom that discovers revocation must not fall through into movement.
	const bool wasSelecting = modalAtFrameStart || controller.selectingFirstAid();
	controller.synchronize(view);
	if (wasSelecting)
	{
		FullEngineCoopClientMedicalInputResult result{true, {}};
		if (!controller.selectingFirstAid() || event.usEvent != KEY_DOWN) return result;
		switch (event.usParam)
		{
			case TAB: case DNARROW: (void)controller.selectNextPatient(view); break;
			case UPARROW: (void)controller.selectPreviousPatient(view); break;
			case ESC: case SPACE: controller.cancelFirstAidSelection(); break;
			case ENTER: result.request = controller.submitFirstAid(view); break;
		}
		return result;
	}
	if (event.usEvent == KEY_DOWN && (event.usParam == 'b' || event.usParam == 'B') &&
		!controller.targetingAttack() && !controller.enteringDestination() &&
		!controller.selectingDoor() && !controller.inventoryOpen())
	{
		(void)controller.beginFirstAidSelection(view);
		return {true, {}};
	}
	return {};
}

bool ConsumeFullEngineCoopClientMedicalOwnerChange(
	FullEngineCoopClientController& controller, bool medicalAtFrameStart) noexcept
{
	if (!medicalAtFrameStart) return false;
	controller.synchronize(FullEngineCoopClientControllerView{});
	InputAtom discarded{};
	while (DequeueEvent(&discarded)) {}
	return true;
}
