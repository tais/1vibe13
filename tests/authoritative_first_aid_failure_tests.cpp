// Isolated process: allocation failure injection must never affect the general
// headless suite. Native startup, AP charging, services and command drains are
// real; only the animation pixels/geometry and optional kit drug are in memory.
#include "Animation Cache.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "Drugs And Alcohol.h"
#include "GameContext.h"
#include "Grid Direction.h"
#include "Isometric Utils.h"
#include "Items.h"
#include "MemMan.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Points.h"
#include "renderworld.h"
#include "Simulation Commands.h"
#include "Soldier Profile Constants.h"
#include "SoldierRepository.h"
#include "Squads.h"
#include "TacticalActor.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "connect.h"
#include "structure.h"
#include "World Tile Map.h"
#include "worlddef.h"
#include "worldman.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// Native animation mappings have application-owned definitions but no public
// declaration, matching the existing data-free headless animation fixtures.
extern UINT16 gubAnimSurfaceIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
extern UINT16 gubAnimSurfaceItemSubIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE;
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE;
BOOLEAN gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};

void ShutdownWithErrorBox(const CHAR8* message)
{
	std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : "");
	std::exit(1);
}

namespace
{
int failures = 0;
TacticalActor* faultMedic = nullptr;
TacticalActor* faultPatient = nullptr;
bool allocationFaultArmed = false;
unsigned allocationFaults = 0;

bool FailDrugVectorCopy(std::size_t size) noexcept
{
	if (!allocationFaultArmed || size != sizeof(DRUG_EFFECT) ||
		faultMedic == nullptr || faultPatient == nullptr ||
		faultMedic->actionPoints().current() != 17 ||
		faultMedic->service().partner() != faultPatient->identity().id() ||
		faultPatient->service().providerCount() != 1)
		return false;
	allocationFaultArmed = false;
	++allocationFaults;
	return true;
}

#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
		++failures; \
	} \
} while (false)
}

void* operator new(std::size_t size)
{
	if (FailDrugVectorCopy(size)) throw std::bad_alloc();
	if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
	throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	const bool missingAnimation = argc == 2 && std::strcmp(argv[1], "--missing-animation") == 0;
	const bool standingStart = argc == 2 && std::strcmp(argv[1], "--standing-start") == 0;
	GameContext& game = GetGameContext();
	const bool running = game.beginInitialization() &&
		game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning();
	CHECK(running, "fault fixture starts the canonical native runtime");
	if (!running) return 1;
	Ja2SoldierRepository& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	ResetJa2TacticalActorRosters();
	ResetJa2TacticalInterruptForNewWorld();
	CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)),
		"fault fixture allocates the real logical map");
	gubWorldMovementCosts = static_cast<UINT8 (*)[MAXDIR][2]>(
		MemAlloc(static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2));
	if (!gubWorldMovementCosts || !GetWorldTileMapSize()) return 1;
	std::memset(gubWorldMovementCosts, TRAVELCOST_BLOCKED,
		static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
	InitRenderParams(0);
	NotifyJa2TacticalWorldLoaded(1);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM;
	is_networked = is_client = is_server = false;
	gGameOptions.fNewTraitSystem = FALSE;
	gTacticalStatus.fAutoBandageMode = FALSE;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	APBPConstants[AP_START_FIRST_AID] = 3;
	APBPConstants[BP_START_FIRST_AID] = 0;
	APBPConstants[AP_CROUCH] = 2;
	const INT32 medicGrid = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS / 2;
	const INT32 patientGrid = medicGrid + WORLD_COLS;
	gubWorldMovementCosts[medicGrid][NORTH][0] = 0;
	TacticalActor& medic = *repository.resolve(0);
	TacticalActor& patient = *repository.resolve(1);
	for (TacticalActor* actor : {&medic, &patient})
	{
		const UINT16 slot = actor == &medic ? 0 : 1;
		actor->identity().id() = SoldierID{slot};
		actor->identity().incarnation() = 100 + slot;
		actor->identity().bodyType() = REGMALE;
		actor->identity().profile() = NO_PROFILE;
		actor->roster().active() = actor->roster().inSector() = TRUE;
		actor->roster().team() = OUR_TEAM;
		actor->assignment().current() = FIRST_SQUAD;
		actor->position().gridNo() = actor == &medic ? medicGrid : patientGrid;
		actor->position().level() = FIRST_LEVEL;
		actor->position().direction() = SOUTH;
		actor->pathing().desiredDirection() = SOUTH;
		actor->animationPlayback().state() = CROUCHING;
		actor->animationPlayback().surface() = 0;
		actor->animationIntent().clearPendingAnimations();
		actor->pendingAction().clearAction();
		actor->movement().mode() = WALKING;
		actor->vitals().health() = actor->vitals().maximumHealth() = 100;
		actor->vitals().breath() = actor->vitals().maximumBreath() = 100;
		actor->actionPoints().current() = 20;
		actor->statistics().medical() = 90;
		actor->statistics().dexterity() = 90;
		actor->statistics().experienceLevel() = 5;
		CHECK(AdoptJa2TacticalEntity(*actor), "fault fixture adopts exact actor identities");
	}
	patient.vitals().health() = 40;
	patient.vitals().bleeding() = 50;
	patient.collapseState().tactical() = TRUE;
	if (standingStart) medic.animationPlayback().state() = STANDING;
	gMAXITEMS_READ = FIRSTAIDKIT + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
	CHECK(CreateItem(FIRSTAIDKIT, 100, &medic.inventory()[HANDPOS]),
		"fault fixture carries a native first-aid kit");
	STRUCTURE patientStructure{};
	patientStructure.fFlags = STRUCTURE_PERSON;
	patientStructure.usStructureID = patient.identity().id().i;
	patientStructure.sGridNo = patientGrid;
	gpWorldLevelData[patientGrid].pStructureHead = &patientStructure;
	CHECK(WhoIsThere2(patientGrid, 0) == patient.identity().id(),
		"native occupancy identifies the adjacent patient");

	ETRLEObject frames[8]{};
	SGPVObject video{};
	video.usNumberOfObjects = 8;
	video.pETRLEObject = frames;
	const AnimationSurfaceType retainedSurface = gAnimSurfaceDatabase[0];
	gAnimSurfaceDatabase[0].hVideoObject = &video;
	gAnimSurfaceDatabase[0].uiNumDirections = 8;
	gAnimSurfaceDatabase[0].uiNumFramesPerDir = 1;
	gAnimSurfaceDatabase[0].bStructDataType = NO_STRUCT;
	gAnimSurfaceDatabase[0].bProfile = -1;
	for (UINT16 animation : {UINT16(CROUCHING), UINT16(START_AID), UINT16(STANDING), UINT16(KNEEL_DOWN)})
	{
		gubAnimSurfaceIndex[REGMALE][animation] = 0;
		gubAnimSurfaceItemSubIndex[REGMALE][animation] = INVALID_ANIMATION;
	}
	if (missingAnimation)
	{
		// Deterministic native load failure, no missing-file/VFS dependency:
		// a one-entry cache cannot evict the actor's current crouching surface.
		// START_AID has a valid body mapping but cannot acquire its other surface.
		guiCacheSize = 1;
		CHECK(medic.animationCache().acquire(medic.identity().id(), 0, CROUCHING),
			"missing-animation fixture occupies the only non-evictable cache entry");
		gubAnimSurfaceIndex[REGMALE][START_AID] = 1;
		CHECK(DetermineSoldierAnimationSurface(&medic, START_AID) == 1 &&
			IsAnimationValidForBodyType(&medic, START_AID),
			"START_AID passes body validation before its native surface acquisition fails");
	}
	else if (!standingStart)
	{
		// Both profiles are NO_PROFILE, so dialogue allocates nothing. The first
		// one-element DRUG_EFFECT copy occurs only after AP and services changed.
		Item[FIRSTAIDKIT].drugtype = 1;
		NewDrug[1].drug_effects.resize(1);
		NewDrug[1].drug_effects[0] = DRUG_EFFECT{};
	}
	const TacticalEntityId medicId = GetJa2TacticalEntityId(medic);
	const TacticalEntityId patientId = GetJa2TacticalEntityId(patient);
	const auto cleanup = [&] {
		allocationFaultArmed = false;
		faultMedic = faultPatient = nullptr;
		medic.animationCache().reset();
		patient.animationCache().reset();
		gAnimSurfaceDatabase[0] = retainedSurface;
		gpWorldLevelData[patientGrid].pStructureHead = nullptr;
		MemFree(gubWorldMovementCosts);
		gubWorldMovementCosts = nullptr;
		ReleaseWorldTileMap();
	};
	BeginFirstAidCommand command;
	CHECK(PrepareBeginFirstAidCommand(medicId, patientId, command) &&
		command.expectedActionPointCost == (standingStart ? 5 : 3) && IsJa2TacticalWorldIntegrityValid(),
		"native aid preflight captures the current pose's startup cost before execution");
	if (!command.expectedKitStateFingerprint) { cleanup(); return 1; }
	faultMedic = &medic;
	faultPatient = &patient;
	allocationFaultArmed = !missingAnimation && !standingStart;
	BeginSimulationCommandFrameBudget(1, 32);
	const auto attempted = TryDispatchSimulationCommandNow(SimulationCommand{command});
	allocationFaultArmed = false;
	if (standingStart)
	{
		CHECK(attempted.status == SimulationCommandDispatchStatus::Applied && attempted.submitted &&
			game.commands().empty() && IsJa2TacticalWorldIntegrityValid() && allocationFaults == 0 &&
			medic.animationPlayback().state() == KNEEL_DOWN &&
			medic.animationIntent().pendingAnimation() == START_AID &&
			medic.animationPlayback().surface() == 0 &&
			patient.animationPlayback().surface() == 0 &&
			medic.service().partner() == patient.identity().id() && patient.service().providerCount() == 1 &&
			medic.targeting().gridNo() == patientGrid && medic.actionPoints().current() == 18 &&
			patient.vitals().bleeding() == 50 && medic.inventory()[HANDPOS][0]->data.objectStatus == 100,
			"standing medic legitimately applies initiation with a loaded kneel and pending START_AID, not completed bandaging");
		cleanup();
		std::printf("authoritative first-aid standing start: %s\n", failures == 0 ? "PASS" : "FAIL");
		return failures == 0 ? 0 : 1;
	}
	CHECK(attempted.status == SimulationCommandDispatchStatus::RetryDeferred && attempted.submitted &&
		!attempted && game.commands().size() == 1 && !IsJa2TacticalWorldIntegrityValid(),
		"post-mutation failure poisons the world before the retained attempt can report Applied");
	CHECK(medic.actionPoints().current() == 17 &&
		medic.service().partner() == patient.identity().id() && patient.service().providerCount() == 1 &&
		medic.targeting().gridNo() == patientGrid && patient.vitals().bleeding() == 50 &&
		medic.inventory()[HANDPOS][0]->data.objectStatus == 100,
		"failure really occurs after one native AP charge and service association, not at preflight");
	if (missingAnimation)
		CHECK(allocationFaults == 0 && medic.animationPlayback().surface() == INVALID_ANIMATION_SURFACE,
			"ignored native animation failure is caught by the post-success invariant");
	else
		CHECK(allocationFaults == 1 && medic.animationPlayback().state() == START_AID,
			"exactly one real post-association drug-vector allocation throws");
	const auto recordsBeforeRetry = game.commandJournal().snapshot();
	bool remainedQueued = false;
	for (const auto& record : recordsBeforeRetry)
		if (record.sequence == attempted.sequence)
			remainedQueued = record.status == CommandJournalStatus::Queued;
	CHECK(remainedQueued, "throwing native attempt has no Applied terminal disposition");
	TacticalWorldSnapshot rejectedSnapshot;
	CHECK(GetJa2TacticalWorldAdapter().capture(rejectedSnapshot) == TacticalWorldCaptureResult::AdapterFailure &&
		GetJa2TacticalWorldAdapter().lastCaptureFailure() == Ja2TacticalWorldCaptureFailure::IntegrityInvalid,
		"the observer source rejects poisoned state at the dedicated runtime's fatal publication boundary");
	BeginFirstAidCommand unmodified = command;
	CHECK(!PrepareBeginFirstAidCommand(medicId, patientId, unmodified),
		"poisoned world refuses new first-aid preparation");
	const UINT16 retainedAnimation = medic.animationPlayback().state();
	const UINT16 retainedAnimationSurface = medic.animationPlayback().surface();
	BeginSimulationCommandFrameBudget(2, 32);
	const auto retried = ExecuteSimulationCommandsThrough(attempted.tick, 32);
	CHECK(retried.applied == 0 && retried.discarded == 1 && game.commands().empty() &&
		!IsJa2TacticalWorldIntegrityValid() && medic.actionPoints().current() == 17 &&
		medic.service().partner() == patient.identity().id() && patient.service().providerCount() == 1 &&
		medic.animationPlayback().state() == retainedAnimation &&
		medic.animationPlayback().surface() == retainedAnimationSurface &&
		patient.vitals().bleeding() == 50 && medic.inventory()[HANDPOS][0]->data.objectStatus == 100,
		"forced later drain refuses the retained operation without a second AP charge or service mutation");
	bool foundDiscard = false;
	for (const auto& record : game.commandJournal().snapshot())
		if (record.sequence == attempted.sequence)
			foundDiscard = record.status == CommandJournalStatus::Discarded;
	CHECK(foundDiscard, "controlled retry has an actual discarded record, never a false successful treatment");

	cleanup();
	std::printf("authoritative first-aid %s: %s\n", missingAnimation ? "missing animation" : "allocation failure",
		failures == 0 ? "PASS" : "FAIL");
	return failures == 0 ? 0 : 1;
}
