#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorLongActions
{
	[[nodiscard]] std::uint8_t current(
		const TacticalActor& actor) noexcept;
	bool start(
		TacticalActor& actor,
		std::uint8_t action,
		std::int32_t contextGrid);
	void cancel(TacticalActor& actor, bool finished);
	bool update(TacticalActor& actor);
}
