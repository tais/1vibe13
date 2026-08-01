#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorInteractions
{
	[[nodiscard]] bool startConversation(
		TacticalActor& actor,
		TacticalActor& target,
		bool validate);
	[[nodiscard]] bool stopChatting(TacticalActor& actor);
	[[nodiscard]] bool beginItemTransfer(TacticalActor& actor);
	[[nodiscard]] bool beginGivingItem(TacticalActor& actor);
	[[nodiscard]] bool handcuffPerson(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool applyItemToPerson(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool collectBloodFromPerson(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
	[[nodiscard]] bool applySplintToPerson(
		TacticalActor& actor,
		std::int32_t targetGrid,
		std::uint8_t direction);
}
