// Reconstructing a saved animation at opcode 491 must not choose a new idle
// action. Exercise the real interpreter, including a live-play positive control.
#include "GameContext.h"
#include "RuntimeSaveState.h"
#include "TacticalActor.h"
#include "SoldierRepository.h"
#include "Soldier Ani.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "Overhead.h"
#include "random.h"
#include <Engine/Core/SimulationRandom.h>
#include <cstdio>
#include <cstdlib>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1); }

int main()
{
	int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::fprintf(stderr, "FAIL %d: %s\n", __LINE__, m); } } while (0)
	CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None, "native RNG installed");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native runtime starts");
	auto* random = GetGameSimulationRandomSource(); if (!random || failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots();
	auto& actor = *repository.resolve(0);
	actor.identity().id() = SoldierID{0}; actor.identity().bodyType() = REGMALE;
	actor.vitals().health() = actor.vitals().maximumHealth() = 80;
	actor.animationPlayback().state() = STANDING;
	gusSelectedSoldier = NOBODY;
	// The idle opcode is followed by the native return opcode, so the fixture
	// uses no renderer or animation assets and cannot run unrelated actions.
	gusAnimInst[STANDING][0] = 491;
	gusAnimInst[STANDING][1] = 405;
	const auto before = random->checkpoint(); const auto epoch = random->consumptionEpoch();
	{
		auto guard = BeginRuntimeLoadExecution(game, RuntimeSavePolicy::DedicatedDeterministic);
		CHECK(static_cast<bool>(guard), "strict native load guard armed");
		gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;
		for (UINT32 savedCounter : {0u, 100000u})
		{
			actor.animationPlayback().code() = 0;
			actor.animationActivity().randomActionCheckCounter() = savedCounter;
			CHECK(AdjustToNextAnimationFrame(&actor), "native saved animation reconstruction advances past idle opcode");
			CHECK(actor.animationPlayback().state() == STANDING && actor.animationPlayback().code() == 1 &&
				actor.animationActivity().randomActionCheckCounter() == savedCounter,
				"loading preserves the saved idle counter and does not choose another animation");
		}
		CHECK(random->checkpoint() == before && random->consumptionEpoch() == epoch,
			"animation reconstruction preserves both RNG state and nonrewindable consumption evidence");
		CHECK(static_cast<bool>(guard.rollback()), "native load guard cleans up");
		gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;
	}
	actor.animationPlayback().code() = 0;
	actor.animationActivity().randomActionCheckCounter() = 100000;
	CHECK(AdjustToNextAnimationFrame(&actor), "ordinary animation interpreter still runs");
	CHECK(actor.animationActivity().randomActionCheckCounter() == 0 && random->consumptionEpoch() > epoch,
		"live play still performs the due native random idle check");
	std::printf("native saved idle animation: %d failures\n", failures);
	return failures ? 1 : 0;
}
