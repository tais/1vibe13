#pragma once

#include "types.h"

#include <cstdint>

class TacticalActor;

namespace TacticalActorAiBehavior
{
	[[nodiscard]] bool hasInitialActionPoints(
		const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isFlanking(
		const TacticalActor& actor) noexcept;
	void setUnderControl(TacticalActor& actor);
	void stopCowering(TacticalActor& actor);
	void startRetreat(
		TacticalActor& actor,
		std::uint16_t turns) noexcept;
	[[nodiscard]] std::uint16_t retreatCounter(
		const TacticalActor& actor) noexcept;
	bool startRadioAnimation(
		TacticalActor& actor);
	void clearBoxerFlag(TacticalActor& actor) noexcept;
	void handleNewSituation(
		TacticalActor& actor,
		bool resetActionBudget);
	[[nodiscard]] bool decideHipOrShoulderStance(
		TacticalActor& actor,
		std::int32_t targetGrid);
}

void HandleSystemNewAISituation(
	TacticalActor* actor,
	BOOLEAN resetActionBudget);
BOOLEAN AIDecideHipOrShoulderStance(
	TacticalActor* actor,
	INT32 targetGrid);
