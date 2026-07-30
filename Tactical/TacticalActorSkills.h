#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorSkills
{
	[[nodiscard]] bool canUse(
		TacticalActor& actor,
		std::int32_t skill,
		bool checkForActionPoints = true,
		std::int32_t targetGridNo = -1);
	[[nodiscard]] bool use(
		TacticalActor& actor,
		std::uint32_t skill,
		std::int32_t targetGridNo,
		std::uint32_t targetId);
	[[nodiscard]] const wchar_t* description(
		TacticalActor& actor,
		std::int32_t skill,
		std::int32_t targetGridNo = -1);
}
