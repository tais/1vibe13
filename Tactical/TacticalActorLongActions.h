#pragma once

#include <cstdint>

class TacticalActor;

enum
{
	MTA_NONE = 0,
	MTA_FORTIFY,
	MTA_REMOVE_FORTIFY,
	MTA_HACK,
	NUM_MTA,
};

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
