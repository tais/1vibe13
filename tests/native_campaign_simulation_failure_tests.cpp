// Actual event/clock failure through the native campaign fixed-step adapter.
// A private Lua asset fails in the ordinary photograph-verification callback;
// no event executor, callback or production failure hook is substituted.
#include "CampaignSimulationHost.h"
#include "CampaignEventAdapter.h"
#include "CampaignEventScheduling.h"
#include "CampaignClockAdapter.h"
#include "DedicatedCoopRuntime.h"
#include "DedicatedServerOptions.h"
#include "GameContext.h"
#include "Game Clock.h"
#include "Game Events.h"
#include "Game Event Hook.h"
#include "TacticalWorldAdapter.h"
#include "Overhead.h"
#include "gameloop.h"
#include "screenids.h"
#include "FileMan.h"
#include "MemMan.h"
#include <Engine/Core/FrameDriver.h>
#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>
#include <vfs/Core/vfs_profile.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }
extern BOOLEAN gfProcessingGameEvents, gfPreventDeletionOfAnyEvent;
extern UINT8 gubEnvLightValue;

namespace
{
int failures = 0;
#define CHECK(c, m) do { if (!(c)) { ++failures; std::printf("FAIL %d: %s\n", __LINE__, m); } } while (0)

// Package fan-out retains its normal isolation semantics. Its outgoing message
// stays queued; only the application's guarded next frame could deliver it.
struct PackageProbe final : SimulationTickSink, RuntimeUpdateSink, RuntimeMessageSink
{
	RuntimeMessageBus& messages;
	CampaignSimulationHost& native;
	unsigned ticks = 0, failedTicks = 0, updates = 0, deliveries = 0;
	explicit PackageProbe(RuntimeMessageBus& bus, CampaignSimulationHost& host) : messages(bus), native(host) {}
	void simulate(const SimulationTickContext&) override
	{
		++ticks;
		if (native.failed())
		{
			++failedTicks;
			CHECK(messages.publish({"fixture.deferred", "fixture.package", {1}}), "isolated package message may remain queued");
		}
	}
	void updateRuntime(const RuntimeUpdateContext&) override { ++updates; }
	void receiveMessage(const RuntimeMessage&) override { ++deliveries; }
};
}

int main(int argc, char** argv)
{
	std::setbuf(stdout, nullptr);
	const bool singlePlayer = argc == 2 && std::strcmp(argv[1], "--single-player") == 0;
	const bool pvp = argc == 2 && std::strcmp(argv[1], "--pvp") == 0;
	if (argc > 2 || (argc == 2 && !singlePlayer && !pvp)) return 2;
	const bool coop = !singlePlayer && !pvp;
	DedicatedServerOptions options;
	options.enabled = !singlePlayer;
	options.mode = coop ? DedicatedServerMode::Coop : DedicatedServerMode::Pvp;
	InstallDedicatedServerOptions(options);
	gfDedicatedServer = singlePlayer ? FALSE : TRUE;
	CHECK(IsDedicatedCoopProcess() == coop, "explicit process scope distinguishes co-op, PvP and single player");
	auto& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
		"actual native campaign fixture starts");
	const auto root = std::filesystem::temp_directory_path() /
		("ja2-native-campaign-failure-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root / "scripts");
	{
		std::ofstream script(root / "scripts/Overhead.lua");
		script << "error('private native campaign failure fixture')\n";
		CHECK(script.good(), "private ordinary Lua asset supplies a real evaluation failure");
	}
	CHECK(InitializeMemoryManager(), "native memory manager initialized");
	vfs_init::VfsConfig config;
	auto* profile = new vfs_init::Profile();
	profile->m_name = L"native-campaign-failure";
	profile->m_root = vfs::Path(root.generic_u8string()); profile->m_writable = false;
	auto* location = new vfs_init::Location(); location->m_type = L"DIRECTORY";
	profile->addLocation(location, true); config.addProfile(profile, true);
	CHECK(vfs_init::initVirtualFileSystem(config, false) && InitializeFileManager(nullptr), "private native VFS mounted");
	if (failures) return 1;
	[[maybe_unused]] auto screen = OverrideCurrentScreen(MAP_SCREEN);
	gTacticalStatus.fDidGameJustStart = FALSE;
	NotifyJa2TacticalWorldUnloaded(); ClearJa2TacticalWorldSector();
	InitializeJa2CampaignClock(112200);
	UnLockPauseState(); UnPauseGame(); SetGameHoursPerSecond(1);
	gubEnvLightValue = 2;
	auto& queue = GetJa2CampaignEventQueue();
	CHECK(queue.empty(), "native queue starts empty");
	const auto first = AddStrategicEventUsingSecondsChecked(EVENT_CHANGELIGHTVAL, 112201, 5);
	const auto broken = AddStrategicEventUsingSecondsChecked(EVENT_INTEL_PHOTOFACT_VERIFY, 112202, 0);
	const auto later = AddStrategicEventUsingSecondsChecked(EVENT_CHANGELIGHTVAL, 112203, 9);
	CHECK(first && broken && later && queue.validate(), "three actual native events scheduled in order");
	if (!first || !broken || !later) return 1;
	const auto brokenId = broken.event->id, laterId = later.event->id;
	const auto brokenSnapshot = broken.event->snapshot(), laterSnapshot = later.event->snapshot();
	auto& native = game.campaignSimulation();
	CHECK(!native.failed(), "new native campaign host has no failure latch");
	if (!coop)
	{
		bool threw = false;
		try { native.simulate({1, 1000000, 1000000}); }
		catch (...) { threw = true; }
		CHECK(threw && !native.failed() && !native.failureTickSequence() &&
			!gfDedicatedServerProcessFailed && gfProgramIsRunning,
			"single-player and PvP retain native exception propagation without co-op process failure");
		CHECK(gubEnvLightValue == 5 && GetWorldTotalSeconds() == 112202 && queue.head() == broken.event &&
			broken.event->next == later.event && queue.size() == 2,
			"legacy control still executes the real preceding event before the same native exception");
		try { native.throwIfFailed(); }
		catch (...) { CHECK(false, "co-op frame gate is inert for unchanged single-player and PvP state"); }
	}
	else
	{
		ManualTimeSource time;
		RecordingFramePresenter presenter;
		EngineServices services; // Replace existing service seams, never native event execution.
		EngineServices timed{time, services.random, services.storage, services.log, services.input, services.audio, presenter};
		RuntimeMessageBus messages;
		InputDispatcher input(timed.input);
		RuntimeUpdateDispatcher updates;
		FrameTelemetry telemetry;
		SimulationTickDispatcher ticks(20000, 10);
		PackageProbe package(messages, native);
		CHECK(ticks.addSink(native) == SimulationTickSinkRegistrationError::None &&
			ticks.addSink(package) == SimulationTickSinkRegistrationError::None &&
			updates.addSink(package) == RuntimeUpdateSinkRegistrationError::None &&
			messages.addSink(package) == RuntimeMessageSinkRegistrationError::None, "real native host precedes isolated package callbacks");
		FrameDriver frame(timed, messages, input, updates, telemetry, ticks);
		unsigned screenCalls = 0, completedCalls = 0;
		const auto runFrame = [&] {
			// These are the same production gates used before GameLoop's message
			// drain and at the start of PrepareGameFrame after fixed simulation.
			native.throwIfFailed();
			return frame.runFrame([&] {
				native.throwIfFailed();
				++screenCalls;
				return FramePlan{};
			}, [&] { ++completedCalls; });
		};
		(void)runFrame(); // Establish only the normal monotonic anchor.
		CHECK(frame.completedFrames() == 1 && native.diagnostics().ticks == 0, "warm frame advances no native time");
		time.advanceMicroseconds(200000);
		bool threw = false;
		try { (void)runFrame(); }
		catch (...) { threw = true; }
		CHECK(threw && native.failed() && native.failureTickSequence() == 1 && native.diagnostics().ticks == 1 &&
			gfDedicatedServerProcessFailed && !gfProgramIsRunning && GamePaused() && !IsTimeBeingCompressed(),
			"swallowed native event exception latches the process and prevents later native catch-up ticks");
		CHECK(std::strcmp(native.failureReason(), "native campaign standard exception") == 0 ||
			std::strcmp(native.failureReason(), "unknown native campaign exception") == 0,
			"failure diagnostic is a bounded classification without private Lua source text");
		CHECK(package.ticks == 10 && package.failedTicks == 10 && package.updates == 2 &&
			messages.queued() == 10 && package.deliveries == 0,
			"generic package isolation continues but no queued message is delivered after the failed native tick");
		CHECK(gubEnvLightValue == 5 && GetWorldTotalSeconds() == 112202 &&
			CaptureJa2CampaignClock().previousTotalSeconds == 112200 && gfProcessingGameEvents &&
			queue.validate() && queue.size() == 2 && queue.head() == broken.event && broken.event->next == later.event &&
			broken.event->id == brokenId && later.event->id == laterId &&
			broken.event->snapshot() == brokenSnapshot && later.event->snapshot() == laterSnapshot,
			"partial native clock/light effects and both exact unretired event nodes remain, without running the later callback");
		CHECK(frame.completedFrames() == 1 && frame.nextFrameSequence() == 3 && frame.captureBoundaryState() &&
			screenCalls == 1 && completedCalls == 1 && presenter.presentations().size() == 1,
			"failed full frame unwinds before native screen, completed observer/publication, presentation or committed frame count");
		std::vector<CampaignEventSnapshot> retained;
		CHECK(queue.capture(retained), "capture failed event cohort for no-retry evidence");
		const auto failedClock = CaptureJa2CampaignClock();
		const auto failureTick = native.failureTickSequence();
		game.runtime().campaignClockScheduler().reset();
		ticks.reset(); frame.resetFrameSequence();
		// Even deliberately clearing ordinary pause controls cannot reset a
		// process-lifetime native failure or permit another callback attempt.
		UnPauseGame(); SetGameHoursPerSecond(1);
		for (unsigned attempt = 0; attempt < 32; ++attempt)
		{
			const auto result = ticks.advance(200000);
			std::vector<CampaignEventSnapshot> current;
			CHECK(result.executed == 10 && result.callbackFailures == 0 && native.diagnostics().ticks == 1 &&
				native.failed() && native.failureTickSequence() == failureTick && queue.capture(current) && current == retained &&
				CaptureJa2CampaignClock() == failedClock && gubEnvLightValue == 5,
				"resetting tick/frame/clock pacing cannot clear the latch or retry native events");
		}
		const auto queued = messages.queued();
		threw = false;
		try { (void)runFrame(); }
		catch (...) { threw = true; }
		CHECK(threw && messages.queued() == queued && !package.deliveries && frame.completedFrames() == 0 &&
			screenCalls == 1 && completedCalls == 1 && presenter.presentations().size() == 1,
			"later application-frame attempt stops before queued package command delivery or publication");
		DedicatedCoopRuntime failedPump;
		failedPump.pumpAfterCommittedFrame(game);
		CHECK(failedPump.failed() && failedPump.error() == DedicatedCoopRuntimeError::InvalidState &&
			!failedPump.admissionRunning() && !failedPump.campaignEntered(),
			"runtime refuses a nominal committed-frame publication after native simulation failed");
		DedicatedCoopRuntime failedCheckpoint;
		CHECK(!failedCheckpoint.shutdownAtCommittedBoundary(game) && failedCheckpoint.failed() &&
			failedCheckpoint.error() == DedicatedCoopRuntimeError::InvalidState &&
			!failedCheckpoint.admissionRunning() && !std::filesystem::exists(root / "SavedGames"),
			"required checkpoint boundary refuses the failed host before any campaign or writable save is opened");
	}
	// This process ends now. Test-owned queue cleanup does not clear the host
	// latch or constitute a supported recovery of the partially mutated session.
	queue.clear(); gfProcessingGameEvents = FALSE; gfPreventDeletionOfAnyEvent = FALSE;
	ShutdownFileManager();
	getVFS()->getProfileStack()->removeProfile(vfs::String("native-campaign-failure"));
	ShutdownMemoryManager(); std::filesystem::remove_all(root);
	std::printf("native campaign simulation failure: %d failures\n", failures);
	return failures ? 1 : 0;
}
