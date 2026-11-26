#include <Ja2/FullEngineCoopClientMovementMode.h>
#include <cstdio>

namespace { int failures = 0; }
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

int main()
{
	TacticalActorSnapshot actor;
	std::uint16_t mode = 99;
	actor.stance = TacticalStance::Standing;
	CHECK(FullEngineCoopClientMovementModeFor(actor, false, mode) && mode == Ja2WalkingMovementMode, "standing walks");
	CHECK(FullEngineCoopClientMovementModeFor(actor, true, mode) && mode == Ja2RunningMovementMode, "standing runs with the toggle");
	actor.stance = TacticalStance::Crouched;
	CHECK(FullEngineCoopClientMovementModeFor(actor, true, mode) && mode == Ja2CrouchedMovementMode,
		"a crouched merc keeps its stance's mode");
	actor.stance = TacticalStance::Prone;
	CHECK(FullEngineCoopClientMovementModeFor(actor, true, mode) && mode == Ja2ProneMovementMode, "a prone merc crawls");
	actor.stance = TacticalStance::Unknown; mode = 99;
	CHECK(!FullEngineCoopClientMovementModeFor(actor, true, mode) && mode == 99, "an unknown stance sends nothing");
	static_assert(Ja2RunningMovementMode == 6 && Ja2WalkingMovementMode == 0, "the stock AnimationStates IDs");
	if (failures) return 1;
	std::printf("co-op client movement mode tests passed\n");
	return 0;
}
