// Isolated process: allocation failure injection must never affect the general
// headless suite. Native startup, AP charging, services and command drains are
// real; only the animation pixels/geometry and optional kit drug are in memory.
#include "Animation Cache.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "Drugs And Alcohol.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "TacticalActorMedicalSession.h"
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
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
#include <algorithm>
#include <array>
#include <tuple>
#include <Engine/Core/SimulationRandom.h>
#include "random.h"

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
	const bool normalAid = argc == 2 && (std::strcmp(argv[1], "--native-success") == 0 ||
		std::strcmp(argv[1], "--prone-recipient") == 0 || std::strcmp(argv[1], "--prone-pair") == 0);
	const bool pronePair = argc == 2 && std::strcmp(argv[1], "--prone-pair") == 0;
	const bool standingMissingMapping = argc == 2 && std::strcmp(argv[1], "--standing-missing-mapping") == 0;
	const bool missingMapping = standingMissingMapping || (argc == 2 && std::strcmp(argv[1], "--missing-mapping") == 0);
	const bool missingProneMapping = argc == 2 && std::strcmp(argv[1], "--missing-prone-mapping") == 0;
	const bool proneStart = argc == 2 && std::strcmp(argv[1], "--prone-start") == 0;
	const bool proneRecipient = argc == 2 && std::strcmp(argv[1], "--prone-recipient") == 0;
	const bool missingAnimation = argc == 2 && std::strcmp(argv[1], "--missing-animation") == 0;
	const bool standingStart = argc == 2 && std::strcmp(argv[1], "--standing-start") == 0;
	CHECK(InstallGameSimulationRandom(20260928) == GameSimulationRandomInstallError::None,
		"first-aid fixture installs the authoritative random stream");
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
	CHECK(InitPathAI(), "read-only aid fixture initializes real path scratch for preservation checks");
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
	APBPConstants[AP_PRONE] = 2;
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
	if (proneRecipient || pronePair || missingProneMapping) patient.animationPlayback().state() = PRONE;
	if (pronePair || proneStart || missingProneMapping) medic.animationPlayback().state() = PRONE;
	if (standingStart || standingMissingMapping) medic.animationPlayback().state() = STANDING;
	const UINT16 medicIdlePose = medic.animationPlayback().state();
	const UINT16 requestedAid = (pronePair || missingProneMapping) ? START_AID_PRN : START_AID;
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
	for (UINT16 animation : {UINT16(CROUCHING), UINT16(START_AID), UINT16(STANDING), UINT16(KNEEL_DOWN), UINT16(PRONE), UINT16(PRONE_UP), UINT16(START_AID_PRN), UINT16(GIVING_AID), UINT16(GIVING_AID_PRN), UINT16(END_AID), UINT16(END_AID_PRN)})
	{
		gubAnimSurfaceIndex[REGMALE][animation] = 0;
		gubAnimSurfaceItemSubIndex[REGMALE][animation] = INVALID_ANIMATION;
	}
	if (missingMapping || missingProneMapping)
	{
		gubAnimSurfaceIndex[REGMALE][requestedAid] = INVALID_ANIMATION;
		CHECK(!IsAnimationValidForBodyType(&medic,requestedAid) &&
			medic.animationPlayback().surface()==0 && gAnimSurfaceDatabase[0].hVideoObject==&video,
			"invalid requested aid mapping leaves the old native idle surface loaded");
	}
	else if (missingAnimation)
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
	else if (!standingStart && !proneStart && !normalAid)
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
		ShutDownPathAI();
		ReleaseWorldTileMap();
	};
	BeginFirstAidCommand command;
	CHECK(PrepareBeginFirstAidCommand(medicId, patientId, command) &&
		command.expectedActionPointCost == ((standingStart || standingMissingMapping || proneStart || pronePair || missingProneMapping) ? 5 : 3) && IsJa2TacticalWorldIntegrityValid(),
		"native aid preflight captures the current pose's startup cost before execution");
	if (!command.expectedKitStateFingerprint) { cleanup(); return 1; }
	if (normalAid)
	{

		auto* campaignRandom = GetGameSimulationRandomSource();
		CHECK(campaignRandom != nullptr, "native random checkpoint is observable");
		std::memset(gubWorldMovementCosts,TRAVELCOST_FLAT,static_cast<std::size_t>(WORLD_MAX)*MAXDIR*2);
		gubGlobalPathFlags=PATH_THROUGH_PEOPLE|PATH_IGNORE_PERSON_AT_DEST;
		gubNPCAPBudget=1; gubNPCDistLimit=1; gfEstimatePath=TRUE;
		gfPathAroundObstacles=FALSE; gfPlotPathToExitGrid=TRUE; gfNPCCircularDistLimit=TRUE;
		gfPlotPathEndDirection=7;
		std::fill_n(guiPathingData,MAX_PATH_DATA_LENGTH,UINT32(77));
		const auto preparationState = [&] {
			std::array<UINT16,MAX_PATH_LIST_SIZE> path{};
			std::copy_n(medic.pathing().path(),path.size(),path.begin());
			return std::make_tuple(medic.position().gridNo(),patient.position().gridNo(),
				medic.actionPoints().current(),patient.vitals().bleeding(),
				medic.pathing().pathIndex(),medic.pathing().pathSize(),path,
				medic.pathing().finalDestinationGrid(),medic.runtime().pendingAction.pathSearchSourceGrid,
				medic.animationPlayback().state(),medic.animationIntent().pendingAnimation(),
				medic.service().partner(),patient.service().providerCount(),
				CaptureInventorySwapObjectState(medicId,HANDPOS),campaignRandom->checkpoint(),campaignRandom->consumptionEpoch(),
				gubGlobalPathFlags,gubNPCAPBudget,gubNPCDistLimit,gfEstimatePath,gfPathAroundObstacles,
				gfPlotPathToExitGrid,gfNPCCircularDistLimit,gfPlotPathEndDirection,guiPathingData,
				std::vector<UINT32>(guiPathingData,guiPathingData+MAX_PATH_DATA_LENGTH));
		};
		for (const BOOLEAN alternate : {FALSE,TRUE})
			for (const UINT8 fromPatient : {NORTH,EAST,SOUTH,WEST})
			{
				gGameSettings.fOptions[TOPTION_ALT_PATHFINDING]=alternate;
				medic.position().gridNo()=NewGridNo(patientGrid,DirectionInc(fromPatient));
				medic.runtime().pendingAction.pathSearchSourceGrid=731;
				const auto before=preparationState();
				BeginFirstAidCommand cardinal=command;
				CHECK(PrepareBeginFirstAidCommand(medicId,patientId,cardinal) &&
					cardinal.expectedActorGrid==medic.position().gridNo() && cardinal.expectedTargetGrid==patientGrid &&
					cardinal.direction==GetDirectionFromGridNo(patientGrid,&medic) && preparationState()==before,
					"all four exact action tiles preserve RNG epoch, actor route and poisoned path scratch in both pathfinder modes");
				gubWorldMovementCosts[medic.position().gridNo()][fromPatient][0]=TRAVELCOST_BLOCKED;
				CHECK(!PrepareBeginFirstAidCommand(medicId,patientId,cardinal) && preparationState()==before,
					"native blocked edge rejects without exploring an alternate approach route");
				STRUCTURE door{};
				door.fFlags=STRUCTURE_DOOR; door.sGridNo=medic.position().gridNo();
				gpWorldLevelData[door.sGridNo].pStructureHead=&door;
				gubWorldMovementCosts[door.sGridNo][fromPatient][0]=TRAVELCOST_DOOR_CLOSED_HERE;
				CHECK(!PrepareBeginFirstAidCommand(medicId,patientId,cardinal) && preparationState()==before,
					"native closed door cost rejects a medical action tile without door mutation");
				door.fFlags|=STRUCTURE_OPEN;
				CHECK(PrepareBeginFirstAidCommand(medicId,patientId,cardinal) && preparationState()==before &&
					door.fFlags==(STRUCTURE_DOOR|STRUCTURE_OPEN),
					"native open door cost admits the current action tile without pathfinding or changing the door");
				gpWorldLevelData[door.sGridNo].pStructureHead=nullptr;
				gubWorldMovementCosts[door.sGridNo][fromPatient][0]=TRAVELCOST_FLAT;
			}
		for (const INT32 rejectedGrid : {patientGrid+WORLD_COLS+1,patientGrid+2})
		{
			medic.position().gridNo()=rejectedGrid;
			const auto before=preparationState();
			BeginFirstAidCommand noApproach=command;
			CHECK(!PrepareBeginFirstAidCommand(medicId,patientId,noApproach) && preparationState()==before,
				"diagonal and distant patients cannot introduce an implicit approach route");
		}
		medic.position().gridNo()=medicGrid;
		medic.runtime().pendingAction.pathSearchSourceGrid=NOWHERE;
		gpWorldLevelData[patientGrid].pStructureHead=nullptr;
		const auto unoccupiedBefore=preparationState();
		BeginFirstAidCommand absent=command;
		CHECK(!PrepareBeginFirstAidCommand(medicId,patientId,absent) && preparationState()==unoccupiedBefore,
			"exact native patient occupancy is required even at a valid cardinal tile");
		gpWorldLevelData[patientGrid].pStructureHead=&patientStructure;
		gubGlobalPathFlags=0; gubNPCAPBudget=0; gubNPCDistLimit=0; gfEstimatePath=FALSE;
		gfPathAroundObstacles=TRUE; gfPlotPathToExitGrid=FALSE; gfNPCCircularDistLimit=FALSE;
		gfPlotPathEndDirection=0; gGameSettings.fOptions[TOPTION_ALT_PATHFINDING]=FALSE;
		std::memset(gubWorldMovementCosts,TRAVELCOST_BLOCKED,static_cast<std::size_t>(WORLD_MAX)*MAXDIR*2);
		gubWorldMovementCosts[medicGrid][NORTH][0]=0;

		std::uint64_t frame = 0;
		auto execute = [&](const BeginFirstAidCommand& prepared) {
			BeginSimulationCommandFrameBudget(++frame, 32);
			return TryDispatchSimulationCommandNow(SimulationCommand{prepared});
		};
		auto untouched = [&] {
			return medic.actionPoints().current() == 20 &&
				medic.inventory()[HANDPOS][0]->data.objectStatus == 100 &&
				patient.vitals().bleeding() == 50 &&
				!medic.service().hasPartner() && !patient.service().hasProviders();
		};
		BeginFirstAidCommand stale = command;
		++stale.target.incarnation;
		CHECK(execute(stale).status == SimulationCommandDispatchStatus::Discarded && untouched(),
			"reused patient slot cannot retarget a prepared medical command");
		stale = command;
		++stale.soldier.incarnation;
		CHECK(execute(stale).status == SimulationCommandDispatchStatus::Discarded && untouched(),
			"reused medic slot cannot spend AP or a kit");
		medic.inventory()[HANDPOS][0]->data.objectStatus = 99;
		CHECK(execute(command).status == SimulationCommandDispatchStatus::Discarded &&
			medic.actionPoints().current() == 20 && !medic.service().hasPartner(),
			"changed exact kit state rejects before starting native first aid");
		medic.inventory()[HANDPOS][0]->data.objectStatus = 100;
		medic.inventory()[HANDPOS][0]->attachments.resize(4);
		BeginFirstAidCommand placeholderKit;
		CHECK(PrepareBeginFirstAidCommand(medicId, patientId, placeholderKit) && untouched(),
			"canonical-empty native NAS slots do not make an ordinary equipped medical kit unusable");
		CHECK(execute(command).status == SimulationCommandDispatchStatus::Discarded && untouched(),
			"first aid kit fingerprint retains the exact empty attachment-slot count");
		OBJECTTYPE& attached = medic.inventory()[HANDPOS][0]->attachments.front();
		attached.usItem = FIRSTAIDKIT;
		attached.ubNumberOfObjects = 1;
		attached[0]->data.objectStatus = 80;
		CHECK(!PrepareBeginFirstAidCommand(medicId, patientId, placeholderKit) &&
			execute(placeholderKit).status == SimulationCommandDispatchStatus::Discarded && untouched(),
			"an actual attached item still rejects first aid without service or AP mutation");
		attached.initialize();
		medic.inventory()[HANDPOS][0]->attachments.clear();
		patient.position().level() = SECOND_LEVEL;
		CHECK(execute(command).status == SimulationCommandDispatchStatus::Discarded && untouched(),
			"changed target elevation rejects without medical side effects");
		patient.position().level() = FIRST_LEVEL;
		medic.actionPoints().current() = 2;
		BeginFirstAidCommand unchanged = command;
		CHECK(!PrepareBeginFirstAidCommand(medicId, patientId, unchanged) &&
			execute(command).status == SimulationCommandDispatchStatus::Discarded &&
			medic.actionPoints().current() == 2 && !medic.service().hasPartner(),
			"insufficient startup AP rejects both preparation and queued execution without spending");
		medic.actionPoints().current() = 20;
		medic.animationActivity().nonInterruptible() = TRUE;
		CHECK(!PrepareBeginFirstAidCommand(medicId, patientId, unchanged) &&
			execute(command).status == SimulationCommandDispatchStatus::Discarded && untouched(),
			"uninterruptible native animation rejects before associating a medical service");
		medic.animationActivity().nonInterruptible() = FALSE;
		medic.identity().bodyType() = COW;
		CHECK(!PrepareBeginFirstAidCommand(medicId, patientId, unchanged) &&
			execute(command).status == SimulationCommandDispatchStatus::Discarded && untouched() &&
			medic.animationPlayback().state() == medicIdlePose,
			"unsupported medic body cannot enter an absent first-aid animation or mutate medical state");
		medic.identity().bodyType() = REGMALE;
		gGameOptions.fNewTraitSystem = TRUE;
		gSkillTraitValues.ubDONumberTraitsNeededForSurgery = 0;
		Item[FIRSTAIDKIT].usItemFlag2 |= ITEM_medicalkit;
		patient.vitals().healableInjury() = 1000;
		gTacticalStatus.ubLastRequesterSurgeryTargetID = patient.identity().id();
		CHECK(TacticalActorMedicalSession::wouldPerformSurgery(medic, patient) &&
			!PrepareBeginFirstAidCommand(medicId, patientId, unchanged) &&
			execute(command).status == SimulationCommandDispatchStatus::Discarded && untouched() &&
			!medic.vitals().isUndergoingSurgery(),
			"native surgery policy is checked read-only and cannot be entered through first aid");
		gGameOptions.fNewTraitSystem = FALSE;
		Item[FIRSTAIDKIT].usItemFlag2 &= ~ITEM_medicalkit;
		patient.vitals().healableInjury() = 0;
		gTacticalStatus.ubLastRequesterSurgeryTargetID = NOBODY;

		const auto sameCommand = [&](const BeginFirstAidCommand& left, const BeginFirstAidCommand& right) {
			std::vector<std::uint8_t> leftWire, rightWire;
			return EncodeSimulationCommandJournal({{1,1,CommandJournalStatus::Queued,SimulationCommand{left}}},0,leftWire) &&
				EncodeSimulationCommandJournal({{1,1,CommandJournalStatus::Queued,SimulationCommand{right}}},0,rightWire) && leftWire==rightWire;
		};
		const auto rejectsPending = [&](auto set, auto restore, const char* reason) {
			set();
			const auto beforeNative = preparationState();
			const auto beforePending = std::make_tuple(medic.animationIntent().hasPendingStance(),
				patient.animationIntent().hasPendingStance(),patient.animationIntent().pendingDirection(),
				medic.schedule().id(),patient.schedule().id());
			const auto beforeAnimation = medic.animationPlayback().state();
			const auto beforePatientAnimation = patient.animationPlayback().state();
			const auto beforePath = medic.pathing().pathSize();
			const auto beforePatientPath = patient.pathing().pathSize();
			BeginFirstAidCommand unchangedOutput = command;
			CHECK(!PrepareBeginFirstAidCommand(medicId,patientId,unchangedOutput) && sameCommand(unchangedOutput,command) &&
				execute(command).status==SimulationCommandDispatchStatus::Discarded && untouched() &&
				medic.animationPlayback().state()==beforeAnimation && patient.animationPlayback().state()==beforePatientAnimation &&
				medic.pathing().pathSize()==beforePath && patient.pathing().pathSize()==beforePatientPath &&
				preparationState()==beforeNative &&
				std::make_tuple(medic.animationIntent().hasPendingStance(),patient.animationIntent().hasPendingStance(),
					patient.animationIntent().pendingDirection(),medic.schedule().id(),patient.schedule().id())==beforePending,reason);
			restore();
		};
		CHECK((gAnimControl[HOPFENCE].uiFlags & ANIM_STATIONARY)!=0, "real active fence traversal has stationary animation flags");
		rejectsPending([&]{medic.animationPlayback().state()=HOPFENCE;},[&]{medic.animationPlayback().state()=medicIdlePose;},
			"stationary-flagged HOPFENCE cannot be overwritten by medical startup");
		const UINT16 patientPose = patient.animationPlayback().state();
		rejectsPending([&]{patient.animationPlayback().state()=HOPFENCE;},[&]{patient.animationPlayback().state()=patientPose;},
			"collapsed recipient status does not admit active fence traversal");
		rejectsPending([&]{medic.animationIntent().pendingAnimation()=WALKING;},[&]{medic.animationIntent().clearPendingAnimations();},
			"pending animation rejects medical preparation and retained execution without mutation");
		rejectsPending([&]{medic.animationActivity().turningToShoot()=TRUE;},[&]{medic.animationActivity().turningToShoot()=FALSE;},
			"turning to shoot cannot be overwritten by medical startup");
		rejectsPending([&]{medic.pathing().desiredDirection()=NORTH;},[&]{medic.pathing().desiredDirection()=SOUTH;},
			"unfinished native orientation cannot be replaced by medical rotation");
		rejectsPending([&]{patient.pathing().pathSize()=1;},[&]{patient.pathing().pathSize()=0;},
			"patient unconsumed route rejects without cancelling movement or associating services");
		const auto idlePatientIntent = patient.animationIntent();
		rejectsPending([&]{patient.animationIntent().queueStance(ANIM_PRONE);},[&]{patient.animationIntent()=idlePatientIntent;},
			"pending recipient stance is preserved without service association");
		rejectsPending([&]{patient.animationIntent().queueDirection(EAST);},[&]{patient.animationIntent()=idlePatientIntent;},
			"pending recipient orientation is preserved without service association");
		rejectsPending([&]{medic.schedule().id()=1;},[&]{medic.schedule().id()=0;},
			"scheduled native work cannot be overwritten by first aid");
		medic.inventory()[HANDPOS][0]->data.bTemperature=1.5f;
		CHECK(execute(command).status==SimulationCommandDispatchStatus::Discarded && untouched(),
			"hidden ordinary kit metadata drift invalidates the complete carried-object fingerprint");
		medic.inventory()[HANDPOS][0]->data.bTemperature=0.0f;
		for (unsigned proof=0; proof<5; ++proof)
		{
			BeginFirstAidCommand drift=command;
			switch(proof) {
			case 0: ++drift.expectedWorldGeneration; break;
			case 1: ++drift.expectedTurnSerial; break;
			case 2: ++drift.expectedActorGrid; break;
			case 3: ++drift.expectedTargetGrid; break;
			case 4: ++drift.expectedActionPointCost; break;
			}
			CHECK(execute(drift).status==SimulationCommandDispatchStatus::Discarded && untouched(),
				"stale world, turn, grid or native AP proof cannot apply medical startup");
		}
		CHECK(PrepareBeginFirstAidCommand(medicId,patientId,unchanged) && sameCommand(unchanged,command) && untouched(),
			"restored stable medic and stationary collapsed recipient still admit the same read-only command");
		BeginSimulationCommandFrameBudget(++frame,32);
		const auto started=TryDispatchSimulationCommandNow(SimulationCommand{command});
		CHECK(started.status==SimulationCommandDispatchStatus::Applied && medic.actionPoints().current()==17 &&
			medic.animationPlayback().state()==requestedAid && medic.service().partner()==patient.identity().id() &&
			patient.service().providerCount()==1 && patient.vitals().bleeding()==50 &&
			medic.inventory()[HANDPOS][0]->data.objectStatus==100 && IsJa2TacticalWorldIntegrityValid(),
			"applied medical startup associates exact actors and charges once without claiming completed treatment");
		medic.animationActivity().nonInterruptible()=FALSE;
		CHECK(TacticalActorMedicalSession::resumeProvidingAnimation(medic) && medic.animationPlayback().state()==(pronePair ? GIVING_AID_PRN : GIVING_AID),
			"native service continuation enters giving-aid animation");
		const INT16 beforeTreatment=medic.actionPoints().current();
		HandleTeamServices(OUR_TEAM);
		CHECK(patient.vitals().bleeding()<50 && medic.inventory()[HANDPOS][0]->data.objectStatus<100 &&
			medic.actionPoints().current()<beforeTreatment,
			"real native treatment consumes kit/AP and reduces bleeding after startup");
		BeginSimulationCommandFrameBudget(++frame,32);
		const auto stopped=TryDispatchSimulationCommandNow(SimulationCommand{StopMovementCommand{medicId,
			SimulationCommandSource::NetworkPeer,TacticalCommandAuthorityPolicy::DedicatedCoop}});
		CHECK(stopped.status==SimulationCommandDispatchStatus::Applied && !medic.service().hasPartner() &&
			!patient.service().hasProviders(), "existing authoritative Stop cancels both native service roles");
		medic.service().beginProvidingTo(patient.identity().id()); patient.service().addProvider();
		patient.vitals().health()=OKLIFE-1;
		BeginSimulationCommandFrameBudget(++frame,32);
		const auto criticalStop=TryDispatchSimulationCommandNow(SimulationCommand{StopMovementCommand{patientId,
			SimulationCommandSource::NetworkPeer,TacticalCommandAuthorityPolicy::DedicatedCoop}});
		CHECK(criticalStop.status==SimulationCommandDispatchStatus::Discarded && medic.service().hasPartner() &&
			patient.service().providerCount()==1, "uncontrollable wounded recipient cannot cancel native aid through Stop");
		patient.vitals().health()=40;
		BeginSimulationCommandFrameBudget(++frame,32);
		const auto recipientStop=TryDispatchSimulationCommandNow(SimulationCommand{StopMovementCommand{patientId,
			SimulationCommandSource::NetworkPeer,TacticalCommandAuthorityPolicy::DedicatedCoop}});
		CHECK(recipientStop.status==SimulationCommandDispatchStatus::Applied && !medic.service().hasPartner() &&
			!patient.service().hasProviders(), "controllable recipient Stop cancels both service roles without a new cancel verb");
		cleanup();
		std::printf("authoritative first-aid native service: %s\n",failures ? "FAIL" : "PASS");
		return failures ? 1 : 0;
	}
	faultMedic = &medic;
	faultPatient = &patient;
	allocationFaultArmed = !missingAnimation && !missingMapping && !missingProneMapping && !standingStart && !proneStart && !normalAid;
	BeginSimulationCommandFrameBudget(1, 32);
	const auto attempted = TryDispatchSimulationCommandNow(SimulationCommand{command});
	allocationFaultArmed = false;
	if (standingStart || proneStart)
	{
		CHECK(attempted.status == SimulationCommandDispatchStatus::Applied && attempted.submitted &&
			game.commands().empty() && IsJa2TacticalWorldIntegrityValid() && allocationFaults == 0 &&
			medic.animationPlayback().state() == (proneStart ? PRONE_UP : KNEEL_DOWN) &&
			medic.animationIntent().pendingAnimation() == START_AID &&
			medic.animationPlayback().surface() == 0 &&
			patient.animationPlayback().surface() == 0 &&
			medic.service().partner() == patient.identity().id() && patient.service().providerCount() == 1 &&
			medic.targeting().gridNo() == patientGrid && medic.actionPoints().current() == 18 &&
			patient.vitals().bleeding() == 50 && medic.inventory()[HANDPOS][0]->data.objectStatus == 100,
			"standing/prone medic legitimately starts a loaded stance transition with exact pending START_AID");
		cleanup();
		std::printf("authoritative first-aid standing start: %s\n", failures == 0 ? "PASS" : "FAIL");
		return failures == 0 ? 0 : 1;
	}
	const INT16 failedStartupAP = standingMissingMapping ? 18 : (missingMapping || missingProneMapping) ? 20 : 17;
	CHECK(attempted.status == SimulationCommandDispatchStatus::RetryDeferred && attempted.submitted &&
		!attempted && game.commands().size() == 1 && !IsJa2TacticalWorldIntegrityValid(),
		"post-mutation failure poisons the world before the retained attempt can report Applied");
	CHECK(medic.actionPoints().current() == failedStartupAP &&
		medic.service().partner() == patient.identity().id() && patient.service().providerCount() == 1 &&
		medic.targeting().gridNo() == patientGrid && patient.vitals().bleeding() == 50 &&
		medic.inventory()[HANDPOS][0]->data.objectStatus == 100,
		"failure occurs after native service association, with the actual charged or unchanged startup AP");
	if (missingMapping || missingProneMapping)
		CHECK(allocationFaults == 0 && medic.animationPlayback().state() == (standingMissingMapping ? KNEEL_DOWN : medicIdlePose) &&
			medic.animationPlayback().surface() == 0 &&
			(!standingMissingMapping || medic.animationIntent().pendingAnimation()==START_AID),
			"invalid aid mapping cannot be acknowledged through an old loaded pose or a pending but unavailable aid animation");
	else if (missingAnimation)
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
		!GetJa2TacticalWorldAdapter().integrityValid(),
		"the observer source rejects poisoned state at the dedicated runtime's fatal publication boundary");
	BeginFirstAidCommand unmodified = command;
	CHECK(!PrepareBeginFirstAidCommand(medicId, patientId, unmodified),
		"poisoned world refuses new first-aid preparation");
	const UINT16 retainedAnimation = medic.animationPlayback().state();
	const UINT16 retainedAnimationSurface = medic.animationPlayback().surface();
	BeginSimulationCommandFrameBudget(2, 32);
	const auto retried = ExecuteSimulationCommandsThrough(attempted.tick, 32);
	CHECK(retried.applied == 0 && retried.discarded == 1 && game.commands().empty() &&
		!IsJa2TacticalWorldIntegrityValid() && medic.actionPoints().current() == failedStartupAP &&
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
	std::printf("authoritative first-aid %s: %s\n", missingMapping ? "missing mapping" : missingProneMapping ? "missing prone mapping" :
		missingAnimation ? "missing animation" : "allocation failure",
		failures == 0 ? "PASS" : "FAIL");
	return failures == 0 ? 0 : 1;
}
