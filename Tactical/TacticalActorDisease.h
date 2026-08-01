#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorDisease
{
	inline constexpr std::uint32_t diagnosedFlag = 0x00000001;
	inline constexpr std::uint32_t outbreakFlag = 0x00000002;
	inline constexpr std::uint32_t reversingFlag = 0x00000004;
	inline constexpr std::uint32_t legSplintFlag = 0x00000008;
	inline constexpr std::uint32_t armSplintFlag = 0x00000010;

	void infect(TacticalActor& actor, std::uint8_t disease);
	void addPoints(
		TacticalActor& actor,
		std::uint8_t disease,
		std::int32_t points);
	void announce(TacticalActor& actor, std::uint8_t disease);
	void addDisability(TacticalActor& actor, std::uint8_t disability);

	[[nodiscard]] bool canReceiveSplint(TacticalActor& actor);
	[[nodiscard]] bool hasAny(
		TacticalActor& actor,
		bool diagnosedOnly,
		bool healableOnly,
		bool symbolOnly = false);
	[[nodiscard]] bool hasOutbreakProperty(
		TacticalActor& actor,
		std::uint32_t property);
	[[nodiscard]] float magnitude(
		const TacticalActor& actor,
		std::uint8_t disease);

	[[nodiscard]] float contactProtection(TacticalActor& actor);
	[[nodiscard]] std::int16_t resistance(TacticalActor& actor);
	[[nodiscard]] std::uint16_t diagnosisPoints(TacticalActor& actor);
}
