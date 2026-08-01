#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorProfileClassification
{
	[[nodiscard]] std::int8_t profileTableIndex(
		const TacticalActor& actor,
		std::uint8_t team) noexcept;
}
