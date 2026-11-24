#ifndef JA2_DEDICATED_COOP_MOVE_DIAGNOSTIC_H
#define JA2_DEDICATED_COOP_MOVE_DIAGNOSTIC_H
#include <Engine/Adapters/JA2/TacticalMoveDiagnostic.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Explicit opt-in. C stdio failure is ignored and never changes command results.
inline bool DedicatedCoopMoveDiagnosticEnabled() noexcept
{
	const char* enabled = std::getenv("JA2_COOP_MOVE_DIAGNOSTIC");
	return enabled != nullptr && std::strcmp(enabled, "1") == 0;
}

inline void TraceDedicatedCoopMoveRejection(
	const char* stage, const TacticalMoveDiagnostic& value) noexcept
{
	if (!DedicatedCoopMoveDiagnosticEnabled()) return;
	(void)std::fprintf(stderr,
		"[coop-move] stage=%s reason=%s actor=%u:%u command=%llu observed=%u "
		"grid=%d destination=%d mode=%u reverse=%u animation=%u path=%u/%u final=%d "
		"ap=%d breath=%d direction=%u desired=%u delayedGrid=%d delayedFlags=%u "
		"firstStep=%u firstDirection=%u nextGrid=%d firstCost=%d combatActions=%u interrupt=%u\n",
		stage, TacticalMoveFailureName(value.reason),
		unsigned(value.actorSlot), unsigned(value.incarnation),
		static_cast<unsigned long long>(value.commandId), unsigned(value.actorObserved),
		int(value.origin), int(value.destination), unsigned(value.movementMode), unsigned(value.reverse),
		unsigned(value.animation), unsigned(value.pathIndex), unsigned(value.pathSize), int(value.finalDestination),
		int(value.actionPoints), int(value.breath), unsigned(value.direction), unsigned(value.desiredDirection),
		int(value.delayedGrid), unsigned(value.delayedFlags), unsigned(value.firstStepObserved),
		unsigned(value.firstDirection), int(value.nextGrid), int(value.firstStepCost),
		unsigned(value.pendingCombatActions), unsigned(value.interruptPhase));
}
#endif
