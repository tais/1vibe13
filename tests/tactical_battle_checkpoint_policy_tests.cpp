#include <Ja2/TacticalBattleCheckpointPolicy.h>
#include <cstdio>
#include <string_view>

namespace { int failures = 0; }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
using Reason = Ja2TacticalBattleCheckpointReason;

namespace
{
Ja2TacticalCheckpointReadiness Quiet()
{
	Ja2TacticalCheckpointReadiness r;
	r.observed = true;
	r.turn.worldLoaded = r.turn.worldIntegrityValid = r.turn.sectorValid = true;
	r.turn.turnBased = r.turn.inCombat = true;
	r.turn.currentTeam = r.turn.playerTeam = 0;
	r.actors.complete = true; r.actors.activeActors = r.actors.inSectorActors = 8;
	r.effects.complete = true;
	return r;
}
template <typename Mutate>
void Expect(Reason expected, Mutate mutate, const char* message)
{
	auto r = Quiet();
	mutate(r);
	CHECK(EvaluateJa2TacticalBattleCheckpoint(r) == expected, message);
}
}

int main()
{
	CHECK(EvaluateJa2TacticalBattleCheckpoint(Quiet()) == Reason::None, "a quiet player turn of turn-based combat is supported");
	CHECK(EvaluateJa2TacticalBattleCheckpoint(Ja2TacticalCheckpointReadiness{}) == Reason::NotObserved,
		"nothing observed authorizes nothing");
	Expect(Reason::WorldNotLoaded, [](auto& r) { r.turn.worldLoaded = false; }, "a battle needs a loaded world");
	Expect(Reason::WorldIntegrityInvalid, [](auto& r) { r.turn.worldIntegrityValid = false; }, "a failed world never saves");
	Expect(Reason::NotTurnBasedCombat, [](auto& r) { r.turn.inCombat = false; }, "real-time play is not this class");
	Expect(Reason::NotTurnBasedCombat, [](auto& r) { r.turn.turnBased = false; }, "real-time combat is not this class");
	Expect(Reason::NotPlayerTurn, [](auto& r) { r.turn.currentTeam = 1; }, "an enemy turn waits");
	Expect(Reason::CombatActionsPending, [](auto& r) { r.turn.pendingCombatActions = 1; }, "a combat action in flight waits");
	Expect(Reason::EventsPending, [](auto& r) { r.events.delayed = 1; }, "a delayed tactical event waits");
	Expect(Reason::InterruptPending, [](auto& r) { r.turn.interruptActive = true; }, "an active interrupt waits");
	Expect(Reason::InterruptPending, [](auto& r) { r.turn.queuedInterrupters = 1; }, "queued interrupters wait");
	Expect(Reason::ActorScanIncomplete, [](auto& r) { r.actors.complete = false; }, "an incomplete actor scan fails closed");
	Expect(Reason::ActorBusy, [](auto& r) { r.actors.busyActors = 1; r.actors.hazards = 1u << 5; }, "a moving actor waits");
	Expect(Reason::EffectScanIncomplete, [](auto& r) { r.effects.complete = false; }, "an incomplete effect scan fails closed");
	Expect(Reason::ProjectileActive, [](auto& r) { r.effects.physicsObjects = 1; }, "a flying grenade waits");
	Expect(Reason::ProjectileActive, [](auto& r) { r.effects.bullets = 1; }, "a bullet in flight waits");
	Expect(Reason::ExplosionActive, [](auto& r) { r.effects.timedWorldBombs = 1; }, "a timed bomb is not yet supported");
	Expect(Reason::TimedEffectActive, [](auto& r) { r.effects.smokeEffects = 1; }, "smoke is not yet qualified");
	Expect(Reason::AnimationTileActive, [](auto& r) { r.effects.animationTilesPresent = true; }, "a transient tile waits");
	Expect(Reason::DialogueOrTimerPending, [](auto& r) { r.continuations.dialogueQueued = true; }, "queued dialogue waits");
	Expect(Reason::DecisionPending, [](auto& r) { r.continuations.surrenderDecision = true; }, "a pending surrender waits");
	Expect(Reason::TransitionPending, [](auto& r) { r.continuations.traversal = true; }, "a traversal waits");
	Expect(Reason::SightPending, [](auto& r) { r.continuations.enemySighting = true; }, "an enemy sighting waits");
	for (unsigned value = 0; value <= static_cast<unsigned>(Reason::SightPending); ++value)
		CHECK(std::string_view(Ja2TacticalBattleCheckpointReasonName(static_cast<Reason>(value))) != "unknown",
			"every reason has a name");
	if (failures) return 1;
	std::printf("tactical battle checkpoint policy tests passed\n");
	return 0;
}
