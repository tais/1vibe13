#include <Engine/Core/DedicatedCheckpointEligibility.h>

namespace
{
bool Known(DedicatedCheckpointResumeMode value) noexcept
{
	return value == DedicatedCheckpointResumeMode::Cold ||
		value == DedicatedCheckpointResumeMode::Warm;
}

bool Known(DedicatedCheckpointDrainState value) noexcept
{
	return value == DedicatedCheckpointDrainState::Drained ||
		value == DedicatedCheckpointDrainState::Pending;
}

bool Known(DedicatedCheckpointRunState value) noexcept
{
	return value == DedicatedCheckpointRunState::Paused ||
		value == DedicatedCheckpointRunState::Running;
}

bool Known(DedicatedCheckpointFrameBoundary value) noexcept
{
	return value == DedicatedCheckpointFrameBoundary::Committed ||
		value == DedicatedCheckpointFrameBoundary::InProgress;
}
}

DedicatedCheckpointEligibilityReason EvaluateDedicatedCheckpointEligibility(
	const DedicatedCheckpointEligibilitySnapshot& snapshot) noexcept
{
	using Reason = DedicatedCheckpointEligibilityReason;

	// Validate the entire declaration first. A malformed field must not be
	// hidden by an otherwise valid operational disqualifier.
	if (!Known(snapshot.resumeMode)) return Reason::InvalidResumeMode;
	if (!Known(snapshot.commandQueue)) return Reason::InvalidCommandQueueState;
	if (!Known(snapshot.networkQueue)) return Reason::InvalidNetworkQueueState;
	if (!Known(snapshot.packageQueue)) return Reason::InvalidPackageQueueState;
	if (!Known(snapshot.dialogueQueue)) return Reason::InvalidDialogueQueueState;
	if (!Known(snapshot.simulation)) return Reason::InvalidSimulationState;
	if (!Known(snapshot.frameBoundary)) return Reason::InvalidFrameBoundaryState;

	if (snapshot.resumeMode == DedicatedCheckpointResumeMode::Warm)
		return Reason::WarmResumeRequested;
	if (snapshot.tacticalWorldLoaded) return Reason::TacticalWorldLoaded;
	if (snapshot.worldItemsLoaded) return Reason::WorldItemsLoaded;
	if (snapshot.combatActive) return Reason::CombatActive;
	if (snapshot.autoResolveActive) return Reason::AutoResolveActive;
	if (snapshot.meanwhileActive) return Reason::MeanwhileActive;
	if (snapshot.projectileActive) return Reason::ProjectileActive;
	if (snapshot.explosionActive) return Reason::ExplosionActive;
	if (snapshot.dialogueActive) return Reason::DialogueActive;
	if (snapshot.dialogueQueue == DedicatedCheckpointDrainState::Pending)
		return Reason::DialogueQueueNotDrained;
	if (snapshot.realtimeAiActive) return Reason::RealtimeAiActive;
	if (snapshot.commandQueue == DedicatedCheckpointDrainState::Pending)
		return Reason::CommandQueueNotDrained;
	if (snapshot.networkQueue == DedicatedCheckpointDrainState::Pending)
		return Reason::NetworkQueueNotDrained;
	if (snapshot.packageQueue == DedicatedCheckpointDrainState::Pending)
		return Reason::PackageQueueNotDrained;
	if (snapshot.simulation == DedicatedCheckpointRunState::Running)
		return Reason::SimulationNotPaused;
	if (snapshot.frameBoundary == DedicatedCheckpointFrameBoundary::InProgress)
		return Reason::NotAtCommittedFrameBoundary;
	if (snapshot.customizableCallbackPending)
		return Reason::CustomizableCallbackPending;
	if (snapshot.reinforcementTurnCounter != 0)
		return Reason::ReinforcementTurnCounterNonzero;
	if (snapshot.enemyReinforcementTurn != 0)
		return Reason::EnemyReinforcementTurnNonzero;
	if (snapshot.enemyReinforcementsArrived != 0)
		return Reason::EnemyReinforcementsArrivedNonzero;
	if (snapshot.militiaReinforcementTurn != 0)
		return Reason::MilitiaReinforcementTurnNonzero;
	if (snapshot.militiaReinforcementsArrived != 0)
		return Reason::MilitiaReinforcementsArrivedNonzero;
	if (snapshot.temporarySchedulesPresent)
		return Reason::TemporarySchedulesPresent;
	if (snapshot.miniEventsEnabled) return Reason::MiniEventsEnabled;
	if (snapshot.unrestrictedLuaRandomnessEnabled)
		return Reason::UnrestrictedLuaRandomnessEnabled;
	return Reason::None;
}

const char* DedicatedCheckpointEligibilityReasonName(
	DedicatedCheckpointEligibilityReason reason) noexcept
{
	using Reason = DedicatedCheckpointEligibilityReason;
	switch (reason)
	{
		case Reason::None: return "none";
		case Reason::InvalidResumeMode: return "invalid resume mode";
		case Reason::InvalidCommandQueueState: return "invalid command queue state";
		case Reason::InvalidNetworkQueueState: return "invalid network queue state";
		case Reason::InvalidPackageQueueState: return "invalid package queue state";
		case Reason::InvalidDialogueQueueState:
			return "invalid dialogue queue state";
		case Reason::InvalidSimulationState: return "invalid simulation state";
		case Reason::InvalidFrameBoundaryState: return "invalid frame boundary state";
		case Reason::WarmResumeRequested: return "warm resume requested";
		case Reason::TacticalWorldLoaded: return "tactical world loaded";
		case Reason::WorldItemsLoaded: return "world items loaded";
		case Reason::CombatActive: return "combat active";
		case Reason::AutoResolveActive: return "auto resolve active";
		case Reason::MeanwhileActive: return "meanwhile active";
		case Reason::ProjectileActive: return "projectile active";
		case Reason::ExplosionActive: return "explosion active";
		case Reason::DialogueActive: return "dialogue active";
		case Reason::DialogueQueueNotDrained:
			return "dialogue queue not drained";
		case Reason::RealtimeAiActive: return "realtime AI active";
		case Reason::CommandQueueNotDrained: return "command queue not drained";
		case Reason::NetworkQueueNotDrained: return "network queue not drained";
		case Reason::PackageQueueNotDrained: return "package queue not drained";
		case Reason::SimulationNotPaused: return "simulation not paused";
		case Reason::NotAtCommittedFrameBoundary:
			return "not at committed frame boundary";
		case Reason::CustomizableCallbackPending:
			return "customizable callback pending";
		case Reason::ReinforcementTurnCounterNonzero:
			return "reinforcement turn counter nonzero";
		case Reason::EnemyReinforcementTurnNonzero:
			return "enemy reinforcement turn nonzero";
		case Reason::EnemyReinforcementsArrivedNonzero:
			return "enemy reinforcements arrived nonzero";
		case Reason::MilitiaReinforcementTurnNonzero:
			return "militia reinforcement turn nonzero";
		case Reason::MilitiaReinforcementsArrivedNonzero:
			return "militia reinforcements arrived nonzero";
		case Reason::TemporarySchedulesPresent:
			return "temporary schedules present";
		case Reason::MiniEventsEnabled: return "MiniEvents enabled";
		case Reason::UnrestrictedLuaRandomnessEnabled:
			return "unrestricted Lua randomness enabled";
	}
	return "unknown dedicated checkpoint eligibility reason";
}
