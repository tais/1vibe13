#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorDamageQueue
{
	void schedule(
		TacticalActor& actor,
		std::int8_t height,
		std::int16_t lifeDeduct,
		std::int16_t breathDeduct,
		std::uint8_t reason,
		SoldierID attacker,
		std::int32_t sourceGrid,
		std::int16_t subsequent,
		bool showDamage);
	bool resolve(TacticalActor& actor);
	void clear(TacticalActor& actor) noexcept;
}
