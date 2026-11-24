// Reconstructing a saved animation at opcode 491 must not choose a new idle
// action. Exercise the real interpreter, including a live-play positive control.
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorLifecycle.h"
#include "Soldier Create.h"
#include "TacticalEntityHost.h"
#include "SaveLoadGame.h"
#include "GameVersion.h"
#include "FileMan.h"
#include "vobject.h"
#include "Isometric Utils.h"
#include "Soldier Profile Constants.h"
#include <vfs/Core/vfs_init.h>
#include <chrono>
#include <filesystem>
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

extern UINT16 gubAnimSurfaceIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
extern UINT16 gubAnimSurfaceItemSubIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "unexpected shutdown: %s\n", message ? message : ""); std::exit(1); }

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
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

	// Reproduce the later load step, after the actual native actor bytes have
	// round-tripped. The inert two-frame surface needs no installed assets.
	const auto fixtureRoot = std::filesystem::temp_directory_path() /
		("ja2-idle-load-clock-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directory(fixtureRoot);
	vfs_init::VfsConfig config;
	auto* profile = new vfs_init::Profile();
	profile->m_name = L"idle-load-clock";
	profile->m_root = vfs::Path(fixtureRoot.generic_u8string());
	profile->m_writable = true;
	config.addProfile(profile, true);
	CHECK(vfs_init::initVirtualFileSystem(config) && InitializeFileManager(nullptr),
		"native actor fixture opens a real writable disk profile");
	ETRLEObject frames[16]{};
	for (auto& frame : frames) { frame.usWidth = 17; frame.usHeight = 19; frame.sOffsetX = -2; frame.sOffsetY = -3; }
	SGPPaletteEntry palette[256]{};
	SGPVObject video{};
	video.usNumberOfObjects = 16;
	video.pETRLEObject = frames;
	video.pPaletteEntry = palette;
	gAnimSurfaceDatabase[RGMNOTHING_STD].hVideoObject = &video;
	gAnimSurfaceDatabase[RGMNOTHING_STD].uiNumDirections = 8;
	gAnimSurfaceDatabase[RGMNOTHING_STD].uiNumFramesPerDir = 2;
	gAnimSurfaceDatabase[RGMNOTHING_STD].bStructDataType = NO_STRUCT;
	gAnimSurfaceDatabase[RGMNOTHING_STD].bProfile = -1;
	gAnimSurfaceDatabase[RGMNOTHING_STD].bUsageCount = 1; // Fixture owns the inert video object.
	gubAnimSurfaceIndex[REGMALE][STANDING] = RGMNOTHING_STD;
	gubAnimSurfaceItemSubIndex[REGMALE][STANDING] = INVALID_ANIMATION;
	actor.initialize();
	actor.identity().id() = SoldierID{0};
	actor.identity().incarnation() = 31;
	actor.identity().bodyType() = REGMALE;
	actor.identity().profile() = NO_PROFILE;
	actor.position().gridNo() = NOWHERE;
	actor.movement().reservedGrid() = NOWHERE;
	actor.renderBindings().faceIndex() = -1;
	actor.renderState().lightSprite() = -1;
	actor.roster().team() = OUR_TEAM;
	actor.roster().active() = TRUE;
	actor.vitals().health() = actor.vitals().maximumHealth() = 80;
	actor.vitals().breath() = actor.vitals().maximumBreath() = 90;
	actor.actionPoints().current() = 37;
	actor.animationPlayback().state() = STANDING;
	actor.animationPlayback().surface() = RGMNOTHING_STD;
	actor.animationPlayback().code() = 0;
	gusAnimInst[STANDING][0] = 1;
	gusAnimInst[STANDING][1] = 491;
	gusAnimInst[STANDING][2] = 2;
	gusAnimInst[STANDING][3] = 405;
	CHECK(AdjustToNextAnimationFrame(&actor) && actor.animationPlayback().code() == 1,
		"ordinary interpreter establishes a valid saved frame and next-instruction cursor");
	actor.animationActivity().randomActionCheckCounter() = 100000;
	actor.animationPlayback().delay() = 200;
	actor.timing().start(SoldierTimingComponent::Timer::AnimationUpdate, 7);
	const UINT16 savedFrame = actor.animationPlayback().frame();
	TacticalActor uninterrupted = actor;
	guiCurrentSaveGameVersion = SAVE_GAME_VERSION;
	HWFILE output = FileOpen("actor.bin", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
	CHECK(output && SaveTacticalActor(output, actor), "native actor serializer writes the real payload");
	if (output) FileClose(output);
	HWFILE input = FileOpen("actor.bin", FILE_ACCESS_READ | FILE_OPEN_EXISTING);
	CHECK(input && LoadTacticalActor(input, actor), "native actor deserializer reads the real payload");
	if (input) FileClose(input);
	CHECK(actor.animationPlayback().code() == 1 && actor.animationPlayback().frame() == savedFrame &&
		actor.timing().counter(SoldierTimingComponent::Timer::AnimationUpdate) == 7,
		"disk decoding itself preserves the exact idle clock and cursor");
	const auto reconstructedRandom = random->checkpoint();
	const auto reconstructedEpoch = random->consumptionEpoch();
	{
		auto guard = BeginRuntimeLoadExecution(game, RuntimeSavePolicy::DedicatedDeterministic);
		CHECK(static_cast<bool>(guard), "strict reconstruction load guard armed");
		gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;
		actor.animationCache().initialize(actor.identity().id());
		TacticalActor savedActor = actor;
		SOLDIERCREATE_STRUCT create{};
		create.fUseExistingSoldier = TRUE;
		create.pExistingSoldier = &savedActor;
		create.bTeam = actor.roster().team();
		SoldierID restoredId = NOBODY;
		CHECK(TacticalCreateSoldier(&create, &restoredId) == &actor &&
			restoredId == actor.identity().id() && GetJa2TacticalEntityId(actor).valid(),
			"actual copied-existing actor caller reconstructs and adopts the saved native record");
		std::printf("reconstructed idle clock: code=%u frame=%u remaining=%d delay=%d\n",
			actor.animationPlayback().code(), actor.animationPlayback().frame(),
			actor.timing().counter(SoldierTimingComponent::Timer::AnimationUpdate), actor.animationPlayback().delay());
		CHECK(actor.animationPlayback().code() == 1 && actor.animationPlayback().frame() == savedFrame,
			"loading does not execute the next saved idle instruction");
		CHECK(actor.timing().counter(SoldierTimingComponent::Timer::AnimationUpdate) == 7,
			"loading preserves the saved time until the next native frame");
		CHECK(random->checkpoint() == reconstructedRandom && random->consumptionEpoch() == reconstructedEpoch,
			"reconstruction preserves simulation random state and consumption epoch");
		CHECK(static_cast<bool>(guard.rollback()), "reconstruction guard cleans up");
		gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;
	}
	// Compare the next live instruction with an uninterrupted saved-state
	// control. Rewind only the test oracle's random state; epochs stay monotonic.
	const auto controlStart = random->checkpoint();
	const auto controlEpoch = random->consumptionEpoch();
	CHECK(AdjustToNextAnimationFrame(&uninterrupted), "uninterrupted actor executes its due next idle instruction");
	const auto controlResult = random->checkpoint();
	const auto controlDraws = random->consumptionEpoch() - controlEpoch;
	CHECK(controlDraws > 0 && uninterrupted.animationActivity().randomActionCheckCounter() == 0,
		"positive control performs the saved due idle random check");
	CHECK(random->restoreCheckpoint(controlStart) == SimulationRandomCheckpointError::None,
		"reset only the test oracle random state for resumed comparison");
	const auto resumedEpoch = random->consumptionEpoch();
	CHECK(AdjustToNextAnimationFrame(&actor), "resumed actor executes its next live instruction");
	CHECK(random->checkpoint() == controlResult && random->consumptionEpoch() - resumedEpoch == controlDraws &&
		actor.animationActivity().randomActionCheckCounter() == uninterrupted.animationActivity().randomActionCheckCounter(),
		"the first live frame after load matches uninterrupted idle RNG work");

	const auto clearPresentation = [&]()
	{
		actor.palette().reset();
		actor.animationCache().release(actor.identity().id());
	};
	const auto seedPose = [&](UINT16 pose)
	{
		clearPresentation();
		actor.initialize();
		actor.identity().id() = SoldierID{0};
		actor.identity().incarnation() = 41;
		actor.position().gridNo() = NOWHERE;
		actor.movement().reservedGrid() = NOWHERE;
		actor.renderBindings().faceIndex() = -1;
		actor.renderState().lightSprite() = -1;
		actor.identity().bodyType() = REGMALE;
		actor.identity().profile() = NO_PROFILE;
		actor.roster().team() = OUR_TEAM;
		actor.vitals().health() = actor.vitals().maximumHealth() = 80;
		actor.vitals().breath() = actor.vitals().maximumBreath() = 90;
		actor.actionPoints().current() = 37;
		actor.animationPlayback().state() = pose;
		actor.animationPlayback().surface() = RGMNOTHING_STD;
		actor.animationPlayback().code() = 0;
		gubAnimSurfaceIndex[REGMALE][pose] = RGMNOTHING_STD;
		gubAnimSurfaceItemSubIndex[REGMALE][pose] = INVALID_ANIMATION;
		gusAnimInst[pose][0] = 1;
		gusAnimInst[pose][1] = 2;
		gusAnimInst[pose][2] = 405;
		CHECK(AdjustToNextAnimationFrame(&actor), "ordinary interpreter establishes another saved idle pose");
		actor.animationPlayback().delay() = 200;
		actor.timing().start(SoldierTimingComponent::Timer::AnimationUpdate, 7);
	};
	const auto diskRoundtrip = [&]()
	{
		HWFILE saved = FileOpen("actor.bin", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
		const bool wrote = saved && SaveTacticalActor(saved, actor);
		if (saved) FileClose(saved);
		HWFILE loaded = FileOpen("actor.bin", FILE_ACCESS_READ | FILE_OPEN_EXISTING);
		const bool read = loaded && LoadTacticalActor(loaded, actor);
		if (loaded) FileClose(loaded);
		return wrote && read;
	};
	for (UINT16 pose : {UINT16(STANDING), UINT16(CROUCHING), UINT16(PRONE)})
	{
		std::printf("idle pose/control %u\n", pose);
		seedPose(pose);
		const auto frame = actor.animationPlayback().frame();
		CHECK(diskRoundtrip(), "each ordinary idle pose round-trips through the native disk format");
		const auto rng = random->checkpoint();
		const auto rngEpoch = random->consumptionEpoch();
		auto guard = BeginRuntimeLoadExecution(game, RuntimeSavePolicy::DedicatedDeterministic);
		gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;
		CHECK(TacticalActorLifecycle::create(actor, REGMALE, SoldierID{0}, pose, true),
			"actual saved actor lifecycle binds each ordinary idle pose");
		CHECK(actor.animationPlayback().state() == pose && actor.animationPlayback().code() == 1 &&
			actor.animationPlayback().frame() == frame && actor.animationPlayback().delay() == 200 &&
			actor.timing().counter(SoldierTimingComponent::Timer::AnimationUpdate) == 7 &&
			actor.identity().id() == SoldierID{0} && actor.identity().incarnation() == 41 &&
			actor.actionPoints().current() == 37 && actor.vitals().breath() == 90 && !actor.palette().empty() &&
			actor.renderState().boundingBoxWidth() == 17 && actor.renderState().boundingBoxHeight() == 19 &&
			actor.renderState().boundingBoxOffsetX() == -2 && actor.renderState().boundingBoxOffsetY() == -3,
			"all three saved idle poses retain identity, gameplay clock and AP while rebuilding palettes");
		CHECK(random->checkpoint() == rng && random->consumptionEpoch() == rngEpoch && guard.rollback(),
			"each idle pose reconstruction leaves RNG state and epoch unchanged");
		gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;
	}
	for (unsigned fault = 0; fault < 3; ++fault)
	{
		std::printf("invalid idle fixture %u\n", fault);
		seedPose(STANDING);
		if (fault == 0) actor.animationPlayback().code() = MAX_FRAMES_PER_ANIM;
		if (fault == 1) actor.animationPlayback().frame() = video.usNumberOfObjects;
		CHECK(diskRoundtrip(), "malformed saved cursor/frame fixture uses real native decoding");
		if (fault == 2) gubAnimSurfaceIndex[REGMALE][STANDING] = FOUND_INVALID_ANIMATION;
		const auto rng = random->checkpoint();
		const auto rngEpoch = random->consumptionEpoch();
		auto guard = BeginRuntimeLoadExecution(game, RuntimeSavePolicy::DedicatedDeterministic);
		gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;
		CHECK(!TacticalActorLifecycle::create(actor, REGMALE, SoldierID{0}, STANDING, true),
			"invalid saved cursor, frame or unavailable native surface fails actor reconstruction");
		CHECK(random->checkpoint() == rng && random->consumptionEpoch() == rngEpoch && guard.rollback(),
			"failed saved-idle reconstruction never executes an animation instruction or draws RNG");
		gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;
		gubAnimSurfaceIndex[REGMALE][STANDING] = RGMNOTHING_STD;
	}
	// Opt-in applies only to actual saved idle records. Fresh creation during
	// native loading and other poses keep the established transition path.
	for (UINT16 pose : {UINT16(STANDING), UINT16(AIM_RIFLE_STAND)})
	{
		seedPose(pose);
		gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;
		CHECK(TacticalActorLifecycle::create(actor, REGMALE, SoldierID{0}, pose, pose != STANDING),
			"fresh-loading and other-pose controls retain their original construction path");
		CHECK(actor.animationPlayback().code() == 2 &&
			actor.timing().counter(SoldierTimingComponent::Timer::AnimationUpdate) == actor.animationPlayback().delay(),
			"non-opted-in reconstruction still advances and starts its animation normally");
		gTacticalStatus.uiFlags &= ~LOADING_SAVED_GAME;
	}
	actor.palette().reset();
	actor.animationCache().release(actor.identity().id());
	gAnimSurfaceDatabase[RGMNOTHING_STD].hVideoObject = nullptr;
	FileDelete("actor.bin");
	ShutdownFileManager();
	std::filesystem::remove_all(fixtureRoot);
	std::printf("native saved idle animation: %d failures\n", failures);
	return failures ? 1 : 0;
}
