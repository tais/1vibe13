#include "DedicatedCoopBattleNotice.h"
#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Game Clock.h"
#include "Overhead.h"
#include "SoldierRepository.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
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
extern void DeathTimerCallback();
extern void CaptureTimerCallback();
extern BOOLEAN gfSurrendered;
namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)
}

int main(int argc, char** argv)
{
	const bool stale = argc == 2 && std::strcmp(argv[1], "--stale") == 0;
	const bool staleAfter = argc == 2 && std::strcmp(argv[1], "--stale-after-ack") == 0;
	const bool capture = argc == 2 && std::strcmp(argv[1], "--capture") == 0;
	const bool creatures = argc == 2 && std::strcmp(argv[1], "--creatures") == 0;
	if (argc > 2 || (argc == 2 && !stale && !staleAfter && !capture && !creatures)) return 2;
	using Kind = DedicatedCoopBattleNoticeKind;
	using Result = DedicatedCoopBattleNoticeResult;
	CHECK(InstallGameSimulationRandom(20260927) == GameSimulationRandomInstallError::None, "native RNG installed");
	DedicatedServerOptions options; options.enabled = true; options.mode = DedicatedServerMode::Coop;
	InstallDedicatedServerOptions(options);
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"native battle notice fixture starts");
	if (failures) return 1;
	GetJa2SoldierRepository().initializeSlots(); ResetJa2TacticalActorRosters();
	SetJa2TacticalWorldSector(9, 1, 0); NotifyJa2TacticalWorldLoaded(41);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM; is_networked = is_client = is_server = false;
	gTacticalStatus.fLastBattleWon = FALSE;
	ResetTacticalTeamPopulations();
	DedicatedCoopBattleNoticeState state, other;
	CHECK(!DeferDedicatedCoopBattleNotice(Kind::Defeated) && !DedicatedCoopBattleNoticePending(),
		"unbound runtime preserves ordinary native presentation");
	CHECK(BindDedicatedCoopBattleNoticeState(state) && BindDedicatedCoopBattleNoticeState(state) &&
		!BindDedicatedCoopBattleNoticeState(other), "one runtime owns the native battle continuation");
	options.enabled = false; InstallDedicatedServerOptions(options);
	CHECK(!DeferDedicatedCoopBattleNotice(Kind::Defeated) && !state.pending(), "single player retains its local dialog");
	options.enabled = true; options.mode = DedicatedServerMode::Pvp; InstallDedicatedServerOptions(options);
	CHECK(!DeferDedicatedCoopBattleNotice(Kind::Defeated) && !state.pending(), "dedicated PvP retains its local dialog");
	options.mode = DedicatedServerMode::Coop; InstallDedicatedServerOptions(options);
	is_networked = true;
	CHECK(!DeferDedicatedCoopBattleNotice(Kind::Defeated) && !state.pending(), "legacy networking cannot enter the native hold");
	is_networked = false;
	if (creatures) CHECK(SetTacticalTeamPopulation(CREATURE_TEAM, 1, TRUE), "native creature population set");
	const auto randomBefore = GetGameSimulationRandomSource()->checkpoint();
	if (capture) { gfSurrendered = TRUE; CaptureTimerCallback(); }
	else DeathTimerCallback();
	CHECK(state.pending() && !state.failure() && DedicatedCoopBattleNoticePending() && GamePaused(),
		"real native timer publishes a held notice without opening the dedicated message box");
	if (!state.pending()) return 1;
	auto notice = *state.pending();
	CHECK(notice.worldGeneration == 41 && notice.turnSerial && notice.sector == (TacticalWorldSession::Sector{9, 1, 0}) &&
		notice.kind == (capture ? Kind::Surrendered : creatures ? Kind::DefeatedByCreatures : Kind::Defeated),
		"notice retains exact native outcome, sector, world and turn");
	CHECK(GetGameSimulationRandomSource()->checkpoint() == randomBefore && IsJa2TacticalWorldLoaded(),
		"publishing the notice neither rerolls gameplay nor unloads the world");
	CHECK(DeferDedicatedCoopBattleNotice(notice.kind) && state.pending()->id == notice.id && !state.failure(),
		"duplicate native publication preserves the held identity");
	CHECK(AcknowledgeDedicatedCoopBattleNotice(notice.id + 1) == Result::StaleNotice && state.pending(),
		"foreign acknowledgement cannot consume a notice");
	if (capture)
	{
		CHECK(!gfSurrendered, "native capture timer clears its original surrender flag");
		state.reset();
		CaptureTimerCallback();
		CHECK(state.pending() && state.pending()->kind == Kind::Captured && state.pending()->id > notice.id,
			"unconscious capture stays distinct and teardown cannot reuse an identity");
		if (!state.pending()) return 1;
		notice = *state.pending();
	}
	CHECK(CompleteDedicatedCoopBattleNotice(notice.id) == Result::NotPending && state.pending() && !state.acknowledged(),
		"native continuation cannot run without an explicit acknowledgement");
	if (stale)
	{
		NotifyJa2TacticalWorldLoaded(42);
		CHECK(AcknowledgeDedicatedCoopBattleNotice(notice.id) == Result::NativeContextChanged && state.pending(),
			"old acknowledgement cannot unload a replacement world");
		CHECK(DeferDedicatedCoopBattleNotice(notice.kind) && state.failure() && state.pending()->id == notice.id,
			"overlapping world fails closed without replacing the original notice");
	}
	else
	{
		CHECK(AcknowledgeDedicatedCoopBattleNotice(notice.id) == Result::Applied && state.pending() && state.acknowledged() &&
			IsJa2TacticalWorldLoaded() && DedicatedCoopBattleNoticePending(),
			"acknowledgement keeps the native world held until receipt delivery and a later committed boundary");
		CHECK(AcknowledgeDedicatedCoopBattleNotice(notice.id) == Result::NotPending && state.acknowledged(),
			"duplicate acknowledgement cannot restart or complete native unloading");
		CHECK(CompleteDedicatedCoopBattleNotice(notice.id + 1) == Result::StaleNotice && state.pending(),
			"foreign completion cannot consume the acknowledged native context");
		if (staleAfter)
		{
			NotifyJa2TacticalWorldLoaded(42);
			CHECK(CompleteDedicatedCoopBattleNotice(notice.id) == Result::Failed && state.failure() && IsJa2TacticalWorldLoaded(),
				"context is revalidated after acknowledgement so a replacement world cannot be unloaded");
		}
		else
		{
			// Invoke the actual unload, with its ordinary starting-sector guard. No
			// callback, sector writer or unload result is replaced by a fixture hook.
			gTacticalStatus.fDidGameJustStart = TRUE;
			gGameExternalOptions.ubDefaultArrivalSectorX = 9;
			gGameExternalOptions.ubDefaultArrivalSectorY = 1;
			CHECK(CompleteDedicatedCoopBattleNotice(notice.id) == Result::Failed && !state.pending() &&
				state.failure() && IsJa2TacticalWorldLoaded(),
				"native unload refusal consumes the notice and latches failure without pretending to return to campaign");
		}
	}
	CHECK(AcknowledgeDedicatedCoopBattleNotice(notice.id) == Result::Failed && DedicatedCoopBattleNoticePending(),
		"failed or partially attempted continuation cannot be replayed");
	UnbindDedicatedCoopBattleNoticeState(other);
	CHECK(DedicatedCoopBattleNoticePending(), "foreign teardown cannot release the hold");
	UnbindDedicatedCoopBattleNoticeState(state);
	CHECK(!DedicatedCoopBattleNoticePending(), "owner teardown releases the binding without acknowledging");
	std::puts(failures ? "native battle notice: FAIL" : "native battle notice: PASS");
	return failures ? 1 : 0;
}
