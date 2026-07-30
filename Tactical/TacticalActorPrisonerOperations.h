#pragma once

class TacticalActor;

namespace TacticalActorPrisonerOperations
{
	[[nodiscard]] bool canProcess(
		const TacticalActor& actor) noexcept;
	bool freeAdjacent(TacticalActor& actor);
}
