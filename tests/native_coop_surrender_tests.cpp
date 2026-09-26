#include "DedicatedCoopSurrender.h"
#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "TacticalActor.h"
#include "TacticalActorRouteExecution.h"
#include "SoldierRepository.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Civ Quotes.h"
#include "Overhead.h"
#include "Game Clock.h"
#include "Soldier Profile.h"
#include "Soldier Profile Constants.h"
#include "ai.h"
#include "connect.h"
#include "sgp.h"
#include "random.h"
#include <Engine/Core/SimulationRandom.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
}

int main(int argc, char** argv)
{
	const bool stale = argc == 2 && std::strcmp(argv[1], "--stale") == 0;
	if (argc > 2 || (argc == 2 && !stale)) return 2;
	CHECK(InstallGameSimulationRandom(20260926) == GameSimulationRandomInstallError::None, "native quote RNG installed");
	using Result = DedicatedCoopSurrenderResult;
	using Reply = DedicatedCoopSurrenderReply;
	DedicatedServerOptions options; options.enabled = true; options.mode = DedicatedServerMode::Coop;
	InstallDedicatedServerOptions(options);
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native surrender fixture starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots(); ResetJa2TacticalActorRosters();
	NotifyJa2TacticalWorldLoaded(41);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, ENEMY_TEAM, 0);
	gbPlayerNum = OUR_TEAM; is_networked = is_client = is_server = false;
	auto& speaker = *repository.resolve(263);
	speaker.identity().id() = SoldierID{263}; speaker.identity().incarnation() = 72;
	speaker.identity().profile() = NO_PROFILE; speaker.identity().bodyType() = REGMALE;
	speaker.dialogue().clearCivilianQuote();
	speaker.roster().active() = speaker.roster().inSector() = TRUE; speaker.roster().team() = ENEMY_TEAM;
	speaker.vitals().health() = speaker.vitals().maximumHealth() = 100;
	speaker.position().gridNo() = 120; speaker.position().direction() = SOUTH;
	// Native ActionDone still executes, but this stationary fixture has no route
	// or renderer/animation assets to stop. No callback is substituted.
	(void)TacticalActorRouteExecution::setOutOfActionPoints(speaker, true, false);
	speaker.aiPlanning().action() = AI_ACTION_OFFER_SURRENDER;
	speaker.aiPlanning().actionInProgress() = TRUE;
	CHECK(AdoptJa2TacticalEntity(speaker), "exact native enemy speaker adopted");
	DedicatedCoopSurrenderState state, other;
	CHECK(!DeferDedicatedCoopSurrender(speaker) && !DedicatedCoopSurrenderPending(), "unbound host retains normal presentation");
	CHECK(BindDedicatedCoopSurrenderState(state) && BindDedicatedCoopSurrenderState(state) &&
		!BindDedicatedCoopSurrenderState(other), "only one runtime may own the native continuation");
	options.enabled = false; InstallDedicatedServerOptions(options);
	CHECK(!DeferDedicatedCoopSurrender(speaker) && !state.pending(), "single-player retains the native dialog");
	options.enabled = true; options.mode = DedicatedServerMode::Pvp; InstallDedicatedServerOptions(options);
	CHECK(!DeferDedicatedCoopSurrender(speaker) && !state.pending(), "dedicated PvP retains its legacy path");
	options.mode = DedicatedServerMode::Coop; InstallDedicatedServerOptions(options);
	is_networked = true;
	CHECK(!DeferDedicatedCoopSurrender(speaker) && !state.pending(), "legacy networking cannot enter the continuation");
	is_networked = false;
	SimulationRandom expectedQuoteRandom(20260926);
	CHECK(expectedQuoteRandom.restoreCheckpoint(GetGameSimulationRandomSource()->checkpoint()) == SimulationRandomCheckpointError::None,
		"native quote starts from the observed campaign RNG checkpoint");
	(void)expectedQuoteRandom.next(3);
	StartCivQuote(&speaker);
	CHECK(GetGameSimulationRandomSource()->checkpoint() == expectedQuoteRandom.checkpoint(),
		"deferring presentation preserves the real native surrender quote RNG draw");
	CHECK(state.pending() && !state.failure() && DedicatedCoopSurrenderPending() && GamePaused(),
		"native offer reaches an explicit held decision without a quote renderer or local message box");
	if (!state.pending()) return 1;
	const auto offer = *state.pending();
	CHECK(offer.worldGeneration == 41 && offer.speaker == (TacticalEntityId{263, 72}) && offer.turnSerial,
		"offer retains its exact world, turn and native actor incarnation");
	const auto heldRandom = GetGameSimulationRandomSource()->checkpoint();
	StartCivQuote(&speaker);
	CHECK(GetGameSimulationRandomSource()->checkpoint() == heldRandom, "duplicate offer cannot reroll a quote");
	CHECK(state.pending()->id == offer.id && speaker.aiPlanning().action() == AI_ACTION_OFFER_SURRENDER &&
		!(gTacticalStatus.fEnemyFlags & ENEMY_OFFERED_SURRENDER), "duplicate native presentation neither answers nor allocates a new offer");
	CHECK(ReplyToDedicatedCoopSurrender(offer.id + 1, Reply::ContinueFighting) == Result::StaleOffer && state.pending(),
		"wrong offer cannot consume the native action");
	CHECK(ReplyToDedicatedCoopSurrender(offer.id, static_cast<Reply>(99)) == Result::NativeContextChanged && state.pending(),
		"an invalid answer cannot become an implicit refusal");
	if (stale)
	{
		NotifyJa2TacticalWorldLoaded(42);
		RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, ENEMY_TEAM, 0);
		CHECK(ReplyToDedicatedCoopSurrender(offer.id, Reply::ContinueFighting) == Result::NativeContextChanged && state.pending(),
			"an old-world answer cannot complete a same-slot speaker");
		CHECK(DeferDedicatedCoopSurrender(speaker) && state.failure() && DedicatedCoopSurrenderPending(),
			"overlapping world context latches failure instead of replacing the offer");
		CHECK(ReplyToDedicatedCoopSurrender(offer.id, Reply::ContinueFighting) == Result::Failed,
			"failed continuation cannot retry native effects");
	}
	else
	{
		CHECK(ReplyToDedicatedCoopSurrender(offer.id, Reply::ContinueFighting) == Result::Applied && !state.pending() &&
			!DedicatedCoopSurrenderPending() && (gTacticalStatus.fEnemyFlags & ENEMY_OFFERED_SURRENDER) &&
			speaker.aiPlanning().action() == AI_ACTION_NONE && speaker.aiPlanning().lastAction() == AI_ACTION_OFFER_SURRENDER &&
			!speaker.aiPlanning().actionInProgress(), "explicit refusal executes native ActionDone once for the exact speaker");
		CHECK(ReplyToDedicatedCoopSurrender(offer.id, Reply::Surrender) == Result::NotPending,
			"late conflicting answer cannot reverse the consumed decision");
		state.reset(); gTacticalStatus.fEnemyFlags &= ~ENEMY_OFFERED_SURRENDER;
		speaker.aiPlanning().action() = AI_ACTION_OFFER_SURRENDER;
		StartCivQuote(&speaker);
		CHECK(state.pending() && state.pending()->id > offer.id, "runtime teardown never reuses an offer ID");
	}
	UnbindDedicatedCoopSurrenderState(other);
	CHECK(DedicatedCoopSurrenderPending(), "foreign teardown cannot release the runtime's decision");
	UnbindDedicatedCoopSurrenderState(state);
	CHECK(!DedicatedCoopSurrenderPending(), "owner teardown removes the continuation without choosing a reply");
	if (failures) return 1;
	std::puts("native co-op surrender continuation tests passed");
	return 0;
}
