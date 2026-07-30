#pragma once

#include <cstdint>

class TacticalActor;

namespace TacticalActorFieldOperations
{
	[[nodiscard]] bool beginFenceCutting(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool beginRepair(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool beginRefuel(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool beginCorpseBloodCollection(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool attachDoorAlarm(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool beginFortification(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool performInteractiveAction(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint16_t expectedAction);
	[[nodiscard]] bool beginRobotReload(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool canBreakWindow(
		const TacticalActor& actor);
	[[nodiscard]] bool breakWindow(
		TacticalActor& actor);
}
