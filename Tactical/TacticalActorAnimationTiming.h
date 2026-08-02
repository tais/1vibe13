#pragma once

class TacticalActor;

namespace TacticalActorAnimationTiming
{
	[[nodiscard]] float currentTeamSpeedFactor() noexcept;
	[[nodiscard]] bool refresh(TacticalActor& actor);
	[[nodiscard]] bool adjustForFastTurn(TacticalActor& actor);
}
