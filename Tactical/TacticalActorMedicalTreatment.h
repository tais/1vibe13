#pragma once

#include <cstdint>

class OBJECTTYPE;
class TacticalActor;

namespace TacticalActorMedicalTreatment
{
	[[nodiscard]] std::uint32_t treatInSector(
		TacticalActor& doctor,
		TacticalActor& patient,
		std::int16_t kitPoints,
		std::int16_t kitStatus);
	[[nodiscard]] std::uint32_t treatAbstract(
		TacticalActor& doctor,
		TacticalActor& patient,
		OBJECTTYPE& kit,
		std::int16_t kitPoints,
		std::int16_t kitStatus,
		bool surgery);

	[[nodiscard]] std::uint16_t damagedStatCount(
		const TacticalActor& actor) noexcept;
	std::uint8_t restoreDamagedStats(
		TacticalActor& actor,
		std::uint16_t amountHundredths);
}
