#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorMedicalSession
{
	[[nodiscard]] std::int16_t beginActionPointCost(
		TacticalActor& medic);

	[[nodiscard]] bool beginFirstAid(
		TacticalActor& medic,
		std::int32_t patientGrid,
		std::uint8_t direction);

	[[nodiscard]] bool resumeProvidingAnimation(
		TacticalActor& medic);
}
