#ifndef JA2_FULL_ENGINE_COOP_CLIENT_MOVEMENT_MODE_H
#define JA2_FULL_ENGINE_COOP_CLIENT_MOVEMENT_MODE_H

#include <Engine/Adapters/JA2/TacticalWorldSnapshot.h>
#include <cstdint>

// CoopTacticalIntent carries the authority's raw JA2 movement animation ID.
// These are the stable defaults from AnimationStates; the server still checks
// them against the live actor (TacticalActorMobility::isValidMovementMode).
inline constexpr std::uint16_t Ja2WalkingMovementMode = 0;
inline constexpr std::uint16_t Ja2CrouchedMovementMode = 5;
inline constexpr std::uint16_t Ja2RunningMovementMode = 6;
inline constexpr std::uint16_t Ja2ProneMovementMode = 8;

// The movement mode for a move of this merc: a standing merc runs while its
// player's run toggle is on (stock runs only when standing); crouched and prone
// moves keep their stance's mode. False for an unknown stance.
inline bool FullEngineCoopClientMovementModeFor(const TacticalActorSnapshot& actor, bool running,
	std::uint16_t& output) noexcept
{
	switch (actor.stance)
	{
		case TacticalStance::Standing:
			output = running ? Ja2RunningMovementMode : Ja2WalkingMovementMode;
			return true;
		case TacticalStance::Crouched:
			output = Ja2CrouchedMovementMode;
			return true;
		case TacticalStance::Prone:
			output = Ja2ProneMovementMode;
			return true;
		case TacticalStance::Unknown:
			return false;
	}
	return false;
}

#endif
