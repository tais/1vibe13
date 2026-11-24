#include "TacticalCheckpointReadiness.h"

#include "DedicatedCoopArrival.h"
#include "DedicatedCoopBattleNotice.h"
#include "DedicatedCoopMeanwhile.h"
#include "DedicatedCoopSurrender.h"
#include "SoldierRepository.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"

#include "Animation Control.h"
#include "Auto Resolve.h"
#include "Boxing.h"
#include "Bullets.h"
#include "Dialogue Control.h"
#include "Event Pump.h"
#include "Explosion Control.h"
#include "Handle UI.h"
#include "LightEffects.h"
#include "Meanwhile.h"
#include "Overhead.h"
#include "physics.h"
#include "PreBattle Interface.h"
#include "SmokeEffects.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TeamTurns.h"
#include "Tile Animation.h"
#include "Timer Control.h"
#include "World Items.h"
#include "ai.h"
#include "gamescreen.h"
#include "strategicmap.h"

#include <algorithm>

extern BOOLEAN gfWaitingForTriggerTimer;

namespace
{
using Hazard = Ja2TacticalCheckpointActorHazard;

bool SteadyAnimation(UINT16 state) noexcept
{
	switch (state)
	{
	case STANDING:
	case CROUCHING:
	case PRONE:
	case AIM_RIFLE_STAND:
	case AIM_RIFLE_CROUCH:
	case AIM_RIFLE_PRONE:
	case AIM_DUAL_STAND:
	case AIM_DUAL_CROUCH:
	case AIM_DUAL_PRONE:
	case AIM_ALTERNATIVE_STAND:
	case PUNCH_BREATH:
	case NINJA_BREATH:
	case KNIFE_BREATH:
		return true;
	default:
		return false;
	}
}

std::uint32_t ActorHazards(const TacticalActor& actor) noexcept
{
	std::uint32_t hazards = 0;
	const auto add = [&](Hazard value) { hazards |= static_cast<std::uint32_t>(value); };
	const auto state = actor.animationPlayback().state();
	if (state >= NUMANIMATIONSTATES) add(Hazard::InvalidAnimation);
	else if (state == COWERING || state == COWERING_PRONE ||
		(actor.status().flags() & SOLDIER_COWERING) != 0)
		add(Hazard::CoweringOrCollapsed);
	else if (!SteadyAnimation(state)) add(Hazard::NonIdleAnimation);
	if (actor.collapseState().collapsed() || actor.collapseState().breathCollapsed())
		add(Hazard::CoweringOrCollapsed);

	const auto& route = actor.pathing();
	const auto& movement = actor.movement();
	if (route.pathSize() > MAX_PATH_LIST_SIZE || route.pathIndex() > route.pathSize())
		add(Hazard::InvalidRoute);
	else if (route.pathIndex() < route.pathSize()) add(Hazard::RouteContinuation);
	// A consumed 30/30 cursor is valid and must never index the route array.
	if (movement.delayed() || movement.continuedPathValid() ||
		movement.blockedByAnotherMerc() || movement.waitingForAction() ||
		movement.reservedGrid() != NOWHERE || movement.delayedByNetwork())
		add(Hazard::RouteContinuation);

	const auto& activity = actor.animationActivity();
	const auto& intent = actor.animationIntent();
	if (route.desiredDirection() != actor.position().direction() ||
		movement.highResolutionDirection() != movement.highResolutionDesiredDirection() ||
		activity.turningToShoot() || activity.turningToFall() || activity.turningUntilDone() ||
		activity.turningFromProneMode() != 0 || intent.turningFromUi())
		add(Hazard::Turning);
	if (intent.hasPendingAnimation() || intent.hasSecondaryPendingAnimation() ||
		intent.hasPendingStance() || intent.hasPendingDirection() ||
		intent.continuesAfterStance() || intent.stopPendingNextTile() ||
		activity.nonInterruptible() || activity.realtimeNonInterruptible())
		add(Hazard::AnimationContinuation);
	if (actor.pendingAction().active() || actor.longAction().active() ||
		(actor.status().flags() & (SOLDIER_ENGAGEDINACTION | SOLDIER_LOCKPENDINGACTIONCOUNTER)) != 0)
		add(Hazard::PendingAction);
	if (actor.pendingItem().hasObject() || actor.pendingItem().hasThrowParameters())
		add(Hazard::PendingItem);
	const auto& service = actor.service();
	if (service.active() || service.hasProviders() || service.hasPartner() ||
		service.hasAutoBandagingMedic()) add(Hazard::MedicalService);
	if (activity.gettingHit() || activity.holdAttackerUntilDone() ||
		activity.postHitStance() != 0 || activity.tryingToFall() ||
		activity.suppressionStanceChange() || activity.reactingFromShot() ||
		activity.externalDeath() || (actor.status().flags() & SOLDIER_TURNINGFROMHIT) != 0)
		add(Hazard::DamageRecovery);
	if (actor.aiPlanning().hasActionInProgress() ||
		actor.aiPlanning().action() != AI_ACTION_NONE ||
		actor.aiPlanning().nextAction() != AI_ACTION_NONE || actor.aiPlan().hasPlan())
		add(Hazard::AiContinuation);
	// A schedule assignment alone is persistent idle data, not a continuation.
	if (actor.schedule().doorContinuationPending() || actor.schedule().progress() != 0 ||
		actor.aiBehavior().hasFlag(AI_CHECK_SCHEDULE)) add(Hazard::ScheduleContinuation);
	// The door keyframe can finish before its actor action or route resumes.
	if (actor.runtime().worldObject.active()) add(Hazard::WorldObjectContinuation);
	if (actor.runtime().pendingAction.delayedDamage) add(Hazard::DeferredCallback);
	if (actor.interaction().dragging() || actor.interaction().chatting()) add(Hazard::Interaction);
	// Burst/autofire selection is a retained weapon mode, not proof of a shot.
	if (actor.fireControl().bulletsLeft() != 0 || actor.fireControl().reloading())
		add(Hazard::FireContinuation);
	return hazards;
}

Ja2TacticalCheckpointActorEvidence CaptureActors() noexcept
{
	Ja2TacticalCheckpointActorEvidence result;
	const auto& repository = GetJa2SoldierRepository();
	result.complete = repository.capacity() <= TOTAL_SOLDIERS;
	const auto count = std::min(repository.capacity(), std::size_t(TOTAL_SOLDIERS));
	for (std::size_t slot = 0; slot < count; ++slot)
	{
		++result.scannedSlots;
		const auto& actor = repository.record(slot);
		if (!actor.roster().active()) continue;
		++result.activeActors;
		if (actor.roster().inSector()) ++result.inSectorActors;
		else ++result.awayActors;
		auto hazards = ActorHazards(actor);
		if (!repository.contains(slot, actor) ||
			static_cast<std::uint16_t>(actor.identity().id()) != slot ||
			!GetJa2TacticalEntityId(actor).valid())
			hazards |= static_cast<std::uint32_t>(Hazard::InvalidBinding);
		result.hazards |= hazards;
		if (hazards != 0)
		{
			if (result.busyActors++ == 0)
			{
				result.firstBusySlot = static_cast<std::uint16_t>(slot);
				result.firstBusyIncarnation = actor.identity().incarnation();
			}
		}
	}
	return result;
}

Ja2TacticalCheckpointEffectEvidence CaptureEffects() noexcept
{
	Ja2TacticalCheckpointEffectEvidence result;
	result.complete = guiNumBullets <= NUM_BULLET_SLOTS &&
		guiNumObjectSlots <= NUM_OBJECT_SLOTS && guiNumSmokeEffects <= NUM_SMOKE_EFFECT_SLOTS;
	for (INT32 slot = 0; slot < NUM_BULLET_SLOTS; ++slot)
		if (GetBulletPtr(slot)->fAllocated) ++result.bullets;
	for (const auto& object : ObjectSlots)
		if (object.fAllocated) ++result.physicsObjects;
	for (const auto& explosion : gExplosionData)
		if (explosion.fAllocated) ++result.explosions;
	for (const auto& smoke : gSmokeEffectData)
		if (smoke.fAllocated) ++result.smokeEffects;
	result.lightEffects = GetAllocatedLightEffectCount();
	result.queuedExplosions = gubElementsOnExplosionQueue;
	result.explosionQueueActive = gfExplosionQueueActive != FALSE;
	result.explosionSightUpdatePending = gfExplosionQueueMayHaveChangedSight != FALSE;
	result.animationTilesPresent = pAniTileHead != nullptr;

	// Bound the legacy bomb table and validate references before dereferencing
	// its item objects. This is observation, not the native detonation helper.
	constexpr UINT32 MaximumObservedWorldBombs = 65536;
	if (guiNumWorldBombs > MaximumObservedWorldBombs ||
		(guiNumWorldBombs != 0 && gWorldBombs == nullptr))
		result.complete = false;
	else for (UINT32 slot = 0; slot < guiNumWorldBombs; ++slot)
	{
		const auto& bomb = gWorldBombs[slot];
		if (!bomb.fExists) continue;
		if (bomb.iItemIndex < 0 || std::size_t(bomb.iItemIndex) >= gWorldItems.size())
		{
			result.complete = false;
			continue;
		}
		const auto& item = gWorldItems[bomb.iItemIndex];
		if (!item.fExists || !item.object.exists() || item.object.objectStack.empty() ||
			item.object.objectStack.size() != item.object.ubNumberOfObjects)
		{
			result.complete = false;
			continue;
		}
		if (item.object[0]->data.misc.bDetonatorType == BOMB_TIMED &&
			(item.object.fFlags & OBJECT_DISABLED_BOMB) == 0) ++result.timedWorldBombs;
	}
	return result;
}
}

Ja2TacticalCheckpointReadiness CaptureJa2TacticalCheckpointReadiness() noexcept
{
	Ja2TacticalCheckpointReadiness result;
	const auto& world = CaptureJa2TacticalWorld();
	const auto& turn = world.turn;
	const auto interrupt = CaptureJa2TacticalInterruptProjection();
	result.turn.worldLoaded = world.loaded;
	result.turn.worldIntegrityValid = IsJa2TacticalWorldIntegrityValid();
	result.turn.sectorValid = world.sector.x >= 1 && world.sector.x <= 16 &&
		world.sector.y >= 1 && world.sector.y <= 16 && world.sector.z >= 0 && world.sector.z <= 3;
	result.turn.turnBased = turn.turnBased;
	result.turn.inCombat = turn.inCombat;
	result.turn.currentTeam = turn.currentTeam;
	result.turn.playerTeam = gbPlayerNum;
	result.turn.pendingCombatActions = turn.pendingCombatActions;
	result.turn.pendingInterrupt = world.interrupt.pending;
	result.turn.queuedInterrupters = gubOutOfTurnPersons;
	result.turn.interruptResolving = interrupt.phase == Ja2TacticalInterruptPhase::Resolving;
	result.turn.interruptActive = interrupt.phase == Ja2TacticalInterruptPhase::Active;
	result.turn.hiddenInterrupt = gfHiddenInterrupt != FALSE;
	result.turn.playerInterruptsDisabled = world.interrupt.playerInterruptsDisabled;
	result.actors = CaptureActors();
	const auto events = GetEventQueueStatistics();
	result.events = {events.primary, events.delayed, events.demand, events.payloadBytes};
	result.effects = CaptureEffects();

	auto& pending = result.continuations;
	pending.dialogueActive = DialogueActive() != FALSE;
	pending.dialogueQueued = !DialogueQueueIsEmpty();
	pending.triggerTimer = gfWaitingForTriggerTimer != FALSE;
	pending.customTimer = gpCustomizableTimerCallback != nullptr;
	pending.arrivalDecision = DedicatedCoopArrivalDecisionPending();
	pending.surrenderDecision = DedicatedCoopSurrenderPending();
	pending.battleNotice = DedicatedCoopBattleNoticePending();
	pending.meanwhileDecision = DedicatedCoopMeanwhilePending();
	pending.meanwhile = gfInMeanwhile || gfMeanwhileTryingToStart;
	pending.autoResolve = IsAutoResolveActive() || gfAutomaticallyStartAutoResolve;
	pending.traversal = gfTacticalTraversal != FALSE;
	pending.autoBandage = gTacticalStatus.fAutoBandageMode || gTacticalStatus.fAutoBandagePending;
	pending.boxing = gTacticalStatus.bBoxingState != NOT_BOXING;
	pending.loading = (gTacticalStatus.uiFlags & LOADING_SAVED_GAME) != 0;
	pending.uiTransition = guiPendingOverrideEvent != I_DO_NOTHING || gfEnteringMapScreen;
	pending.conversation = (gTacticalStatus.uiFlags & ENGAGED_IN_CONV) != 0;
	pending.sightSuppressed = (gTacticalStatus.uiFlags & DISALLOW_SIGHT) != 0;
	pending.enemySighting = gTacticalStatus.fEnemySightingOnTheirTurn != FALSE;
	result.observed = true;
	return result;
}
