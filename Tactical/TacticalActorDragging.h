#pragma once

#include <cstdint>

class TacticalActor;
struct SoldierID;

namespace TacticalActorDragging
{
	[[nodiscard]] bool canDrag(TacticalActor& actor, bool checkStance = false);
	[[nodiscard]] bool canDragPerson(
		TacticalActor& actor,
		SoldierID targetId,
		bool checkStance = false);
	[[nodiscard]] bool canDragCorpse(
		TacticalActor& actor,
		std::uint16_t corpseId,
		bool checkStance = false);
	[[nodiscard]] bool canDragStructure(
		TacticalActor& actor,
		std::int32_t gridNo,
		bool checkStance = false);
	[[nodiscard]] bool isDragging(
		TacticalActor& actor,
		bool cancelIfInvalid = false);

	void dragPerson(TacticalActor& actor, SoldierID targetId);
	void dragCorpse(TacticalActor& actor, std::uint16_t corpseId);
	void dragStructure(TacticalActor& actor, std::int32_t gridNo);
	void cancel(TacticalActor& actor);

	[[nodiscard]] bool canStart(TacticalActor& actor);
	void start(TacticalActor& actor);
}
