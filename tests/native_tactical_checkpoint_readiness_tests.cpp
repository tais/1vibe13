// Real native state observation: no eligibility, save/load, or gameplay ticks.
#include "TacticalCheckpointReadiness.h"
#include "Animation Control.h"
#include "Bullets.h"
#include "Event Pump.h"
#include "Explosion Control.h"
#include "GameContext.h"
#include "Handle UI.h"
#include "Items.h"
#include "LightEffects.h"
#include "Overhead.h"
#include "physics.h"
#include "PreBattle Interface.h"
#include "SmokeEffects.h"
#include "Soldier Profile Constants.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationState.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "TeamTurns.h"
#include "Tile Animation.h"
#include "Timer Control.h"
#include "World Items.h"
#include "ai.h"
#include "gamescreen.h"
#include "opplist.h"
#include "random.h"
#include "strategicmap.h"
#include <Engine/Core/SimulationRandom.h>
#include <cstdio>
#include <cstdlib>
#include <tuple>
#include <type_traits>

extern BOOLEAN gfWaitingForTriggerTimer;
// Fixture mutates the real pool; production only gets the read-only count.
extern LIGHTEFFECT gLightEffectData[];
extern UINT32 guiNumLightEffects;
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1); }

namespace
{
int failures = 0;
int callbacks = 0;
void TimerCallback() { ++callbacks; }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (false)
using Hazard = Ja2TacticalCheckpointActorHazard;
bool Has(const Ja2TacticalCheckpointReadiness& evidence, Hazard hazard)
{ return (evidence.actors.hazards & static_cast<std::uint32_t>(hazard)) != 0; }

void ExpectHazard(Hazard hazard, const char* message)
{
	const auto evidence = CaptureJa2TacticalCheckpointReadiness();
	CHECK(evidence.observed && evidence.actors.complete && Has(evidence, hazard), message);
}

auto WorldObjectState(const TacticalActor& actor)
{
	const auto& owner = actor.runtime().worldObject;
	return std::make_tuple(owner.active(), owner.replicateCompletion,
		owner.actorIncarnation(), owner.objectGrid(), owner.objectStructureId(),
		owner.owner(), owner.awaitsDoorChange());
}

void ExpectWorldObjectEvidence(const TacticalActor& actor, bool active)
{
	const auto ownerBefore = WorldObjectState(actor);
	const auto actorBefore = std::make_tuple(actor.position().gridNo(),
		actor.actionPoints().current(), actor.vitals().health(), actor.vitals().breath(),
		actor.animationPlayback().state(), actor.animationPlayback().code(),
		actor.pathing().pathIndex(), actor.pathing().pathSize());
	const auto worldBefore = CaptureJa2TacticalWorld();
	const auto eventsBefore = GetEventQueueStatistics();
	auto* random = GetGameSimulationRandomSource();
	const auto rngBefore = random->checkpoint();
	const auto epochBefore = random->consumptionEpoch();
	for (int capture = 0; capture < 2; ++capture)
	{
		const auto evidence = CaptureJa2TacticalCheckpointReadiness();
		CHECK(evidence.observed && evidence.actors.complete &&
			evidence.actors.hazards == (active
				? static_cast<std::uint32_t>(Hazard::WorldObjectContinuation) : 0u) &&
			evidence.actors.busyActors == (active ? 1u : 0u),
			"retained world-object owner is independently visible in an otherwise idle actor");
		CHECK(!active || (evidence.actors.firstBusySlot == actor.identity().id().i &&
			evidence.actors.firstBusyIncarnation == actor.identity().incarnation()),
			"world-object evidence identifies the exact active actor");
	}
	const auto& worldAfter = CaptureJa2TacticalWorld();
	const auto eventsAfter = GetEventQueueStatistics();
	CHECK(WorldObjectState(actor) == ownerBefore && actorBefore ==
		std::make_tuple(actor.position().gridNo(), actor.actionPoints().current(),
			actor.vitals().health(), actor.vitals().breath(), actor.animationPlayback().state(),
			actor.animationPlayback().code(), actor.pathing().pathIndex(), actor.pathing().pathSize()),
		"repeated capture retains exact door identity, completion policy, phase and actor state");
	CHECK(worldBefore.sector == worldAfter.sector && worldBefore.loaded == worldAfter.loaded &&
		worldBefore.worldGeneration == worldAfter.worldGeneration &&
		worldBefore.turnSerial == worldAfter.turnSerial && worldBefore.turn == worldAfter.turn &&
		worldBefore.interrupt == worldAfter.interrupt &&
		eventsBefore.primary == eventsAfter.primary && eventsBefore.delayed == eventsAfter.delayed &&
		eventsBefore.demand == eventsAfter.demand && eventsBefore.payloadBytes == eventsAfter.payloadBytes &&
		rngBefore == random->checkpoint() && epochBefore == random->consumptionEpoch() && callbacks == 0,
		"world-object capture preserves world/turn, events, callbacks and canonical RNG state/epoch");
}

void CheckWorldObjectLifecycle(TacticalActor& actor)
{
	using Owner = SoldierWorldObjectContinuationOwner;
	auto& door = actor.runtime().worldObject;
	const auto incarnation = actor.identity().incarnation();
	const INT32 grid = actor.position().gridNo() + 1;
	constexpr UINT16 structure = 37;
	for (Owner owner : {Owner::ActorAction, Owner::PathRoute})
	{
		door.begin(false, incarnation, grid, structure, owner);
		ExpectWorldObjectEvidence(actor, true);
		CHECK(!door.completeDoorChange(incarnation, grid, structure) &&
			door.active() && !door.awaitsDoorChange(),
			"genuine door keyframe retains owned completion and suppressed replication");
		ExpectWorldObjectEvidence(actor, true);
		const BOOLEAN replicate = owner == Owner::ActorAction
			? door.consumeActorActionCompletionReplication()
			: door.consumePathContinuationReplication();
		CHECK(!replicate && !door.active(), "genuine final completion consumes the retained owner policy");
		ExpectWorldObjectEvidence(actor, false);
	}
	// An unowned keyframe completes in one phase; cancellation clears either phase.
	door.begin(true, incarnation, grid, structure);
	ExpectWorldObjectEvidence(actor, true);
	CHECK(door.completeDoorChange(incarnation, grid, structure) && !door.active(),
		"unowned native door keyframe consumes its continuation");
	ExpectWorldObjectEvidence(actor, false);
	for (bool keyframeCompleted : {false, true})
	{
		door.begin(false, incarnation, grid, structure, Owner::PathRoute);
		if (keyframeCompleted) door.completeDoorChange(incarnation, grid, structure);
		ExpectWorldObjectEvidence(actor, true);
		door.reset();
		ExpectWorldObjectEvidence(actor, false);
	}
}

TacticalActor& IdleActor(std::uint16_t slot, UINT8 team, bool inSector)
{
	auto& actor = GetJa2SoldierRepository().record(slot);
	actor.initialize();
	actor.identity().id() = SoldierID{slot};
	actor.identity().incarnation() = slot + 1;
	actor.identity().profile() = NO_PROFILE;
	actor.identity().bodyType() = REGMALE;
	actor.roster().active() = TRUE;
	actor.roster().inSector() = inSector;
	actor.roster().team() = team;
	actor.vitals().health() = actor.vitals().maximumHealth() = 80;
	actor.vitals().breath() = 76;
	actor.actionPoints().current() = 82;
	actor.position().gridNo() = 12000 + slot;
	actor.position().direction() = EAST;
	actor.pathing().desiredDirection() = EAST;
	actor.pathing().destinationGrid() = actor.pathing().finalDestinationGrid() = actor.position().gridNo();
	actor.movement().reservedGrid() = NOWHERE;
	actor.animationPlayback().state() = STANDING;
	CHECK(AdoptJa2TacticalEntity(actor), "native actor identity adopted");
	return actor;
}
}

int main()
{
	static_assert(std::is_trivially_copyable_v<Ja2TacticalCheckpointReadiness>);
	CHECK(!Ja2TacticalCheckpointReadiness{}.observed, "default evidence is unobserved");
	CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native runtime starts");
	auto* random = GetGameSimulationRandomSource();
	if (!random || failures) return 1;
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots(); ResetJa2TacticalActorRosters(); ResetJa2TacticalEntityDirectory();
	SetJa2TacticalWorldSector(9, 1, 0); NotifyJa2TacticalWorldLoaded(19);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	ResetJa2TacticalInterruptState(); ResetJa2TacticalInterruptForNewWorld();
	gbPlayerNum = OUR_TEAM;
	guiPendingOverrideEvent = I_DO_NOTHING;
	auto& actor = IdleActor(4, OUR_TEAM, true);
	IdleActor(20, ENEMY_TEAM, true);
	auto& civilian = IdleActor(40, CIV_TEAM, true);
	auto& away = IdleActor(5, OUR_TEAM, false);
	civilian.schedule().id() = 7;
	actor.pathing().pathIndex() = actor.pathing().pathSize() = MAX_PATH_LIST_SIZE;
	const auto rngBefore = random->checkpoint();
	const auto epochBefore = random->consumptionEpoch();
	const auto turnBefore = CaptureJa2TacticalTurn();
	const auto actorBefore = std::make_tuple(actor.position().gridNo(), actor.actionPoints().current(),
		actor.vitals().breath(), actor.pathing().pathIndex(), actor.pathing().pathSize());
	const auto idle = CaptureJa2TacticalCheckpointReadiness();
	CHECK(idle.observed && idle.turn.worldLoaded && idle.turn.worldIntegrityValid && idle.turn.sectorValid &&
		idle.turn.turnBased && idle.turn.inCombat && idle.turn.currentTeam == idle.turn.playerTeam,
		"native ordinary loaded player turn observed");
	CHECK(idle.actors.complete && idle.actors.activeActors == 4 && idle.actors.inSectorActors == 3 &&
		idle.actors.awayActors == 1 && idle.actors.busyActors == 0 && idle.actors.hazards == 0,
		"idle actors, completed route, and assigned schedule have no actor continuation");
	CHECK(idle.effects.complete && idle.effects.bullets == 0 && idle.effects.physicsObjects == 0 &&
		idle.effects.explosions == 0 && idle.effects.smokeEffects == 0 && idle.effects.lightEffects == 0,
		"empty native effect pools observed");

	for (UINT16 state : {UINT16(STANDING), UINT16(CROUCHING), UINT16(PRONE), UINT16(AIM_RIFLE_STAND),
		UINT16(AIM_RIFLE_CROUCH), UINT16(AIM_RIFLE_PRONE), UINT16(AIM_DUAL_STAND), UINT16(AIM_DUAL_CROUCH),
		UINT16(AIM_DUAL_PRONE), UINT16(AIM_ALTERNATIVE_STAND), UINT16(PUNCH_BREATH), UINT16(NINJA_BREATH), UINT16(KNIFE_BREATH)})
	{
		actor.animationPlayback().state() = state;
		CHECK(CaptureJa2TacticalCheckpointReadiness().actors.hazards == 0, "steady native stance/aim/melee pose is idle");
	}
	actor.fireControl().selectAutofire(3);
	CHECK(CaptureJa2TacticalCheckpointReadiness().actors.hazards == 0, "retained autofire mode alone is not active fire");
	actor.fireControl().selectSingleShot();
	for (UINT16 state : {UINT16(WALKING), UINT16(HOPFENCE), UINT16(CATCH_STANDING), UINT16(CATCH_CROUCHED),
		UINT16(START_AID), UINT16(ROLLOVER), UINT16(READY_RIFLE_STAND), UINT16(KNIFE_GOTOBREATH), UINT16(USE_REMOTE)})
	{
		actor.animationPlayback().state() = state;
		ExpectHazard(Hazard::NonIdleAnimation, "active native animation is not a steady ready pose");
	}
	actor.animationPlayback().state() = COWERING;
	ExpectHazard(Hazard::CoweringOrCollapsed, "cowering remains unqualified");
	actor.animationPlayback().state() = NUMANIMATIONSTATES;
	ExpectHazard(Hazard::InvalidAnimation, "invalid animation rejected without indexing animation table");
	actor.animationPlayback().state() = AIM_RIFLE_STAND;
	actor.collapseState().collapse(); ExpectHazard(Hazard::CoweringOrCollapsed, "collapse remains unqualified"); actor.collapseState().clearTactical();
	actor.pathing().pathIndex() = MAX_PATH_LIST_SIZE - 1; ExpectHazard(Hazard::RouteContinuation, "unconsumed route observed");
	actor.pathing().pathIndex() = MAX_PATH_LIST_SIZE + 1; ExpectHazard(Hazard::InvalidRoute, "invalid cursor observed without route read");
	actor.pathing().pathIndex() = MAX_PATH_LIST_SIZE;
	actor.movement().waitForGrid(12345, 1); ExpectHazard(Hazard::RouteContinuation, "movement contention observed"); actor.movement().clearDelay();
	actor.movement().setContinuedPath(12345); ExpectHazard(Hazard::RouteContinuation, "continued route observed"); actor.movement().clearContinuedPath();
	actor.pathing().desiredDirection() = NORTH; ExpectHazard(Hazard::Turning, "turning direction observed"); actor.pathing().desiredDirection() = EAST;
	actor.animationActivity().turningToShoot() = TRUE; ExpectHazard(Hazard::Turning, "turn to shoot observed in steady aim"); actor.animationActivity().turningToShoot() = FALSE;
	actor.animationIntent().queueAnimation(HOPFENCE); ExpectHazard(Hazard::AnimationContinuation, "queued animation observed"); actor.animationIntent().clearPendingAnimation();
	actor.animationIntent().queueStance(ANIM_CROUCH); ExpectHazard(Hazard::AnimationContinuation, "queued stance observed"); actor.animationIntent().clearPendingStance();
	actor.animationActivity().beginHit(); ExpectHazard(Hazard::DamageRecovery, "active hit recovery observed"); actor.animationActivity().clearHit();
	actor.animationActivity().holdAttackerUntilDone() = TRUE; ExpectHazard(Hazard::DamageRecovery, "held attacker observed"); actor.animationActivity().holdAttackerUntilDone() = FALSE;
	actor.service().beginProvidingTo(civilian.identity().id()); ExpectHazard(Hazard::MedicalService, "medical relationship observed"); actor.service().finishProviding();
	actor.pendingItem().beginThrow(); ExpectHazard(Hazard::PendingItem, "prepared throw observed"); actor.pendingItem().clearThrowParameters();
	away.pendingAction().begin(1);
	const auto awayPending = CaptureJa2TacticalCheckpointReadiness();
	CHECK(Has(awayPending, Hazard::PendingAction) && awayPending.actors.firstBusySlot == 5 &&
		awayPending.actors.firstBusyIncarnation == away.identity().incarnation(), "serialized active away actor is inspected");
	away.pendingAction().clearAction();
	civilian.schedule().beginDoorContinuation(12040); ExpectHazard(Hazard::ScheduleContinuation, "schedule door continuation observed"); civilian.schedule().cancelDoorContinuation();
	civilian.aiBehavior().setFlag(AI_CHECK_SCHEDULE); ExpectHazard(Hazard::ScheduleContinuation, "active schedule work observed"); civilian.aiBehavior().clearFlag(AI_CHECK_SCHEDULE);
	actor.aiPlanning().actionInProgress() = TRUE; ExpectHazard(Hazard::AiContinuation, "in-progress AI action observed"); actor.aiPlanning().actionInProgress() = FALSE;
	actor.fireControl().bulletsLeft() = 1; ExpectHazard(Hazard::FireContinuation, "remaining shot continuation observed"); actor.fireControl().bulletsLeft() = 0;
	actor.runtime().pendingAction.delayedDamage = [] { ++callbacks; };
	ExpectHazard(Hazard::DeferredCallback, "deferred actor damage callback observed");
	CHECK(callbacks == 0 && bool(actor.runtime().pendingAction.delayedDamage), "observation retains callback without running it");
	actor.runtime().pendingAction.delayedDamage = nullptr;

	CheckWorldObjectLifecycle(actor);
	CheckWorldObjectLifecycle(away);

	SetJa2TacticalCurrentTeam(ENEMY_TEAM);
	CHECK(CaptureJa2TacticalCheckpointReadiness().turn.currentTeam != gbPlayerNum, "enemy turn remains distinguishable");
	SetJa2TacticalCurrentTeam(OUR_TEAM); SetJa2TacticalTurnBasedMode(false);
	CHECK(!CaptureJa2TacticalCheckpointReadiness().turn.turnBased, "realtime remains distinguishable"); SetJa2TacticalTurnBasedMode(true);
	BeginJa2TacticalCombatAction(); CHECK(CaptureJa2TacticalCheckpointReadiness().turn.pendingCombatActions == 1, "native outstanding combat action observed"); CompleteJa2TacticalCombatAction();
	SetJa2PendingInterrupt(MOVEMENT_INTERRUPT);
	CHECK(CaptureJa2TacticalCheckpointReadiness().turn.interruptResolving, "pending interrupt resolution observed");
	SetJa2PendingInterrupt(DISABLED_INTERRUPT);
	gubOutOfTurnPersons = 1; NotifyJa2TacticalInterruptStarted();
	CHECK(CaptureJa2TacticalCheckpointReadiness().turn.interruptActive &&
		CaptureJa2TacticalCheckpointReadiness().turn.queuedInterrupters == 1, "active interrupt and native queue both observed");
	gubOutOfTurnPersons = 0; NotifyJa2TacticalInterruptCleared();

	EV_S_CHANGESTATE event{}; event.usSoldierID = actor.identity().id(); event.uiUniqueId = actor.identity().incarnation(); event.usNewState = STANDING;
	CHECK(AddGameEvent(S_CHANGESTATE, 0, &event), "actual primary native event queued");
	const auto queuedBefore = GetEventQueueStatistics();
	CHECK(CaptureJa2TacticalCheckpointReadiness().events.primary == 1 && GetEventQueueStatistics().payloadBytes == queuedBefore.payloadBytes,
		"observation neither consumes primary event nor its payload"); ClearEventQueue();
	CHECK(AddGameEvent(S_CHANGESTATE, 60000, &event) && DequeAllGameEvents(TRUE), "actual delayed native event retained by pump");
	CHECK(CaptureJa2TacticalCheckpointReadiness().events.delayed == 1, "delayed native event observed"); ClearEventQueue();
	CHECK(AddGameEvent(S_CHANGESTATE, DEMAND_EVENT_DELAY, &event), "actual demand native event queued");
	CHECK(CaptureJa2TacticalCheckpointReadiness().events.demand == 1, "demand native event observed"); ClearEventQueue();

	// Live allocation flags, including the last slots, matter even when high-water counters are zero.
	GetBulletPtr(NUM_BULLET_SLOTS - 1)->fAllocated = TRUE;
	ObjectSlots[NUM_OBJECT_SLOTS - 1].fAllocated = TRUE;
	gExplosionData[NUM_EXPLOSION_SLOTS - 1].fAllocated = TRUE;
	gSmokeEffectData[NUM_SMOKE_EFFECT_SLOTS - 1].fAllocated = TRUE;
	gLightEffectData[0].fAllocated = TRUE;
	ANITILE animation{}; pAniTileHead = &animation;
	gubElementsOnExplosionQueue = 1;
	const auto effects = CaptureJa2TacticalCheckpointReadiness().effects;
	CHECK(effects.bullets == 1 && effects.physicsObjects == 1 && effects.explosions == 1 && effects.smokeEffects == 1 &&
		effects.lightEffects == 1 && effects.animationTilesPresent && effects.queuedExplosions == 1,
		"all native live effect pools and queued transient effects observed");
	GetBulletPtr(NUM_BULLET_SLOTS - 1)->fAllocated = FALSE; ObjectSlots[NUM_OBJECT_SLOTS - 1].fAllocated = FALSE;
	gExplosionData[NUM_EXPLOSION_SLOTS - 1].fAllocated = FALSE; gSmokeEffectData[NUM_SMOKE_EFFECT_SLOTS - 1].fAllocated = FALSE;
	gLightEffectData[0].fAllocated = FALSE; pAniTileHead = nullptr; gubElementsOnExplosionQueue = 0;
	guiNumBullets = NUM_BULLET_SLOTS; guiNumObjectSlots = NUM_OBJECT_SLOTS; guiNumSmokeEffects = NUM_SMOKE_EFFECT_SLOTS; guiNumLightEffects = 500;
	const auto retained = CaptureJa2TacticalCheckpointReadiness().effects;
	CHECK(retained.complete && retained.bullets == 0 && retained.physicsObjects == 0 && retained.smokeEffects == 0 && retained.lightEffects == 0,
		"retained pool capacity is not live activity");
	guiNumBullets = NUM_BULLET_SLOTS + 1;
	CHECK(!CaptureJa2TacticalCheckpointReadiness().effects.complete, "invalid pool counter cannot certify a complete observation");
	guiNumBullets = guiNumObjectSlots = guiNumSmokeEffects = guiNumLightEffects = 0;
	WORLDBOMB bomb{}; bomb.fExists = TRUE; bomb.iItemIndex = 0; gWorldBombs = &bomb; guiNumWorldBombs = 1;
	CHECK(!CaptureJa2TacticalCheckpointReadiness().effects.complete, "invalid bomb-to-item reference reported without dereference");
	gWorldItems.resize(1); gWorldItems[0].fExists = TRUE;
	gMAXITEMS_READ = 2; Item[1].usItemClass = IC_GRENADE;
	CHECK(CreateItem(1, 100, &gWorldItems[0].object), "native timed world-item object constructed");
	gWorldItems[0].object[0]->data.misc.bDetonatorType = BOMB_TIMED;
	CHECK(CaptureJa2TacticalCheckpointReadiness().effects.timedWorldBombs == 1, "timed native world bomb observed");
	gWorldItems[0].object.ubNumberOfObjects = 2;
	const auto mismatchedStack = CaptureJa2TacticalCheckpointReadiness().effects;
	CHECK(!mismatchedStack.complete && mismatchedStack.timedWorldBombs == 0 &&
		gWorldItems[0].object.ubNumberOfObjects == 2 && gWorldItems[0].object.objectStack.size() == 1,
		"mismatched object count is reported and left unchanged");
	gWorldItems[0].object.ubNumberOfObjects = 1; gWorldItems[0].object.objectStack.clear();
	const auto emptyStack = CaptureJa2TacticalCheckpointReadiness().effects;
	CHECK(!emptyStack.complete && emptyStack.timedWorldBombs == 0 &&
		gWorldItems[0].object.ubNumberOfObjects == 1 && gWorldItems[0].object.objectStack.empty(),
		"advertised object with empty stack is reported without dereference or repair");
	gWorldItems.clear(); gWorldBombs = nullptr; guiNumWorldBombs = 0;

	gpCustomizableTimerCallback = TimerCallback; gfWaitingForTriggerTimer = TRUE;
	gTacticalStatus.fAutoBandagePending = TRUE; gfTacticalTraversal = TRUE;
	const auto continuation = CaptureJa2TacticalCheckpointReadiness().continuations;
	CHECK(continuation.customTimer && continuation.triggerTimer && continuation.autoBandage && continuation.traversal,
		"live native timer and gameplay continuations observed");
	CHECK(callbacks == 0 && gpCustomizableTimerCallback == TimerCallback && gfWaitingForTriggerTimer,
		"observation does not run or cancel native timer callbacks");
	gpCustomizableTimerCallback = nullptr; gfWaitingForTriggerTimer = FALSE;
	gTacticalStatus.fAutoBandagePending = FALSE; gfTacticalTraversal = FALSE;
	CHECK(CaptureJa2TacticalCheckpointReadiness().actors.hazards == 0, "cleared test continuations return to idle actor evidence");
	CHECK(CaptureJa2TacticalTurn() == turnBefore && rngBefore == random->checkpoint() && epochBefore == random->consumptionEpoch() &&
		actorBefore == std::make_tuple(actor.position().gridNo(), actor.actionPoints().current(), actor.vitals().breath(),
			actor.pathing().pathIndex(), actor.pathing().pathSize()), "all observations preserve turn, actor movement points and canonical RNG state/epoch");
	std::printf("native tactical checkpoint evidence: %d failures\n", failures);
	return failures ? 1 : 0;
}
