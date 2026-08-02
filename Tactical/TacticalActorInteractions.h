#pragma once

#include "types.h"

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
	void pickPickupAnimation(
		TacticalActor& actor,
		std::int32_t itemIndex,
		std::int32_t gridNo,
		std::int8_t zLevel);
	[[nodiscard]] bool beginSteal(
		TacticalActor& actor,
		TacticalActor& target);
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

void PickPickupAnimation(
	TacticalActor* actor,
	INT32 itemIndex,
	INT32 gridNo,
	INT8 zLevel);
BOOLEAN MercStealFromMerc(
	TacticalActor* actor,
	TacticalActor* target);
