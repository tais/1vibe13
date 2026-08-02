#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorVisibility
{
	void initializeRanges();

	[[nodiscard]] std::int8_t straightRange() noexcept;
	[[nodiscard]] std::int16_t normalMaximumDistance() noexcept;
	[[nodiscard]] bool hasLimitedVision(TacticalActor& actor);

	[[nodiscard]] std::int16_t adjustForEnvironment(
		TacticalActor& actor,
		std::int8_t lightLevel,
		std::int16_t distance);

	[[nodiscard]] std::int16_t distance(
		TacticalActor& actor,
		std::int8_t facingDirection,
		std::int8_t subjectDirection,
		std::int32_t subjectGrid,
		std::int8_t subjectLevel,
		bool isCowering,
		std::uint8_t tunnelVision,
		TacticalActor* knownSubject = nullptr);

	[[nodiscard]] std::int16_t maximumDistance(
		TacticalActor& actor,
		std::int32_t subjectGrid,
		std::int8_t subjectLevel = -1,
		int calculationMode = -1,
		TacticalActor* knownSubject = nullptr);
}
