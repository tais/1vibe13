#ifndef JA2_TACTICAL_BATTLE_CHECKPOINT_POLICY_H
#define JA2_TACTICAL_BATTLE_CHECKPOINT_POLICY_H

#include "TacticalCheckpointReadiness.h"
#include <cstdint>

// Phase 8.3: may a loaded battle be checkpointed at this instant? Pure: it
// reads only the native readiness evidence (CaptureJa2TacticalCheckpointReadiness)
// and authorizes nothing by itself. A battle checkpoint is supported only at
// an ordinary player turn of turn-based combat with nothing in flight: no
// pending combat action, event or interrupt, every actor settled, no bullet,
// physics object, explosion, timed effect or transient tile, and no dialogue,
// timer, decision or interface transition. The runtime's own drains (frame,
// commands, receipts, packages, transport) are the stop path's responsibility.
// Restrictions are observed, never manufactured by cancelling gameplay.
enum class Ja2TacticalBattleCheckpointReason : std::uint8_t
{
	None,
	NotObserved,
	WorldNotLoaded,
	WorldIntegrityInvalid,
	SectorInvalid,
	NotTurnBasedCombat,
	NotPlayerTurn,
	CombatActionsPending,
	EventsPending,
	InterruptPending,
	ActorScanIncomplete,
	ActorBusy,
	EffectScanIncomplete,
	ProjectileActive,
	ExplosionActive,
	TimedEffectActive,
	AnimationTileActive,
	DialogueOrTimerPending,
	DecisionPending,
	TransitionPending,
	SightPending
};

inline Ja2TacticalBattleCheckpointReason EvaluateJa2TacticalBattleCheckpoint(
	const Ja2TacticalCheckpointReadiness& readiness) noexcept
{
	using Reason = Ja2TacticalBattleCheckpointReason;
	if (!readiness.observed) return Reason::NotObserved;
	const auto& turn = readiness.turn;
	if (!turn.worldLoaded) return Reason::WorldNotLoaded;
	if (!turn.worldIntegrityValid) return Reason::WorldIntegrityInvalid;
	if (!turn.sectorValid) return Reason::SectorInvalid;
	if (!turn.turnBased || !turn.inCombat) return Reason::NotTurnBasedCombat;
	if (turn.playerTeam == UINT8_MAX || turn.currentTeam != turn.playerTeam) return Reason::NotPlayerTurn;
	if (turn.pendingCombatActions != 0) return Reason::CombatActionsPending;
	const auto& events = readiness.events;
	if (events.primary != 0 || events.delayed != 0 || events.demand != 0) return Reason::EventsPending;
	if (turn.pendingInterrupt != 0 || turn.queuedInterrupters != 0 || turn.interruptResolving ||
		turn.interruptActive || turn.hiddenInterrupt)
		return Reason::InterruptPending;
	const auto& actors = readiness.actors;
	if (!actors.complete) return Reason::ActorScanIncomplete;
	if (actors.busyActors != 0 || (actors.hazards & Ja2TacticalCheckpointBlockingActorHazards) != 0)
		return Reason::ActorBusy;
	const auto& effects = readiness.effects;
	if (!effects.complete) return Reason::EffectScanIncomplete;
	if (effects.bullets != 0 || effects.physicsObjects != 0) return Reason::ProjectileActive;
	if (effects.explosions != 0 || effects.queuedExplosions != 0 || effects.explosionQueueActive ||
		effects.explosionSightUpdatePending || effects.timedWorldBombs != 0)
		return Reason::ExplosionActive;
	if (effects.smokeEffects != 0 || effects.lightEffects != 0) return Reason::TimedEffectActive;
	if (effects.animationTilesPresent) return Reason::AnimationTileActive;
	const auto& pending = readiness.continuations;
	if (pending.dialogueActive || pending.dialogueQueued || pending.triggerTimer || pending.customTimer ||
		pending.conversation)
		return Reason::DialogueOrTimerPending;
	if (pending.arrivalDecision || pending.surrenderDecision || pending.battleNotice || pending.meanwhileDecision ||
		pending.meanwhile || pending.autoResolve)
		return Reason::DecisionPending;
	if (pending.traversal || pending.autoBandage || pending.boxing || pending.loading || pending.uiTransition)
		return Reason::TransitionPending;
	if (pending.sightSuppressed || pending.enemySighting) return Reason::SightPending;
	return Reason::None;
}

inline const char* Ja2TacticalBattleCheckpointReasonName(Ja2TacticalBattleCheckpointReason reason) noexcept
{
	using Reason = Ja2TacticalBattleCheckpointReason;
	switch (reason)
	{
		case Reason::None: return "none";
		case Reason::NotObserved: return "not observed";
		case Reason::WorldNotLoaded: return "world not loaded";
		case Reason::WorldIntegrityInvalid: return "world integrity invalid";
		case Reason::SectorInvalid: return "sector invalid";
		case Reason::NotTurnBasedCombat: return "not turn-based combat";
		case Reason::NotPlayerTurn: return "not the player turn";
		case Reason::CombatActionsPending: return "combat actions pending";
		case Reason::EventsPending: return "events pending";
		case Reason::InterruptPending: return "interrupt pending";
		case Reason::ActorScanIncomplete: return "actor scan incomplete";
		case Reason::ActorBusy: return "actor busy";
		case Reason::EffectScanIncomplete: return "effect scan incomplete";
		case Reason::ProjectileActive: return "projectile active";
		case Reason::ExplosionActive: return "explosion active";
		case Reason::TimedEffectActive: return "timed effect active";
		case Reason::AnimationTileActive: return "animation tile active";
		case Reason::DialogueOrTimerPending: return "dialogue or timer pending";
		case Reason::DecisionPending: return "decision pending";
		case Reason::TransitionPending: return "transition pending";
		case Reason::SightPending: return "sight pending";
	}
	return "unknown";
}

#endif
