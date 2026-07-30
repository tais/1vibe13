#pragma once

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
}
