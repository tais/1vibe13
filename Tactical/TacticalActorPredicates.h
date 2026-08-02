#pragma once

class TacticalActor;

namespace TacticalActorPredicates
{
	[[nodiscard]] bool isCivilian(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isCivilianOrMilitia(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isCrouched(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isStanding(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool isProne(const TacticalActor& actor) noexcept;
	[[nodiscard]] bool consideredNeutralForAttack(
		const TacticalActor& observer,
		const TacticalActor& target) noexcept;
}

// Source-compatible spellings for implementation code that still uses the
// original implicit pSoldier convention. New code should call the named
// predicates above.
#define PTR_CIVILIAN TacticalActorPredicates::isCivilian(*pSoldier)
#define PTR_CROUCHED TacticalActorPredicates::isCrouched(*pSoldier)
#define PTR_STANDING TacticalActorPredicates::isStanding(*pSoldier)
#define PTR_PRONE TacticalActorPredicates::isProne(*pSoldier)
#define CONSIDERED_NEUTRAL(me, them) \
	TacticalActorPredicates::consideredNeutralForAttack(*(me), *(them))
