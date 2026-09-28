#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorMedicalSession
{
	// Read-only policy shared with server-prepared first-aid commands. Ordinary
	// local beginFirstAid retains its existing surgery behavior.
	[[nodiscard]] bool wouldPerformSurgery(
		TacticalActor& medic,
		const TacticalActor& patient);

	[[nodiscard]] std::int16_t beginActionPointCost(
		TacticalActor& medic);

	[[nodiscard]] bool beginFirstAid(
		TacticalActor& medic,
		std::int32_t patientGrid,
		std::uint8_t direction);

	[[nodiscard]] bool resumeProvidingAnimation(
		TacticalActor& medic);
}
