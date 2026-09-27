#ifndef JA2_DEDICATED_COOP_POST_COMBAT_CHECKPOINT_POLICY_H
#define JA2_DEDICATED_COOP_POST_COMBAT_CHECKPOINT_POLICY_H

#include <Engine/Core/DedicatedCheckpointEligibility.h>

enum class DedicatedCoopPostCombatCheckpointStep : std::uint8_t
{
	WaitForNativeExit,
	Commit,
	Reject,
	TimedOut
};

// Native screen exit may leave a talking face or queued death dialogue after
// the world unloads. Let ordinary map frames finish that work, with admission
// still closed. This never authorizes an ineligible checkpoint or drains the
// native dialogue itself. Every other hazard remains a required-save failure.
inline constexpr DedicatedCoopPostCombatCheckpointStep
EvaluateDedicatedCoopPostCombatCheckpointStep(
	bool mapReady,
	DedicatedCheckpointEligibilityReason eligibility,
	bool deadlineExpired) noexcept
{
	using Step = DedicatedCoopPostCombatCheckpointStep;
	if (deadlineExpired) return Step::TimedOut;
	if (!mapReady) return Step::WaitForNativeExit;
	switch (eligibility)
	{
		case DedicatedCheckpointEligibilityReason::None:
			return Step::Commit;
		case DedicatedCheckpointEligibilityReason::DialogueActive:
		case DedicatedCheckpointEligibilityReason::DialogueQueueNotDrained:
			return Step::WaitForNativeExit;
		default:
			return Step::Reject;
	}
}

#endif
