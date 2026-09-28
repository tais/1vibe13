// Real retained native inventory execution with data-free item/map/animation
// fixtures. Fault injection is confined to this standalone test process.
#include "Animation Cache.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "CoopInventoryAuthority.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Items.h"
#include "Interface Panels.h"
#include "Isometric Utils.h"
#include "Overhead.h"
#include "Points.h"
#include "renderworld.h"
#include "Simulation Commands.h"
#include "Soldier Profile Constants.h"
#include "SoldierRepository.h"
#include "Squads.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "TacticalActorLighting.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "Weapons.h"
#include "World Tile Map.h"
#include "connect.h"
#include "worlddef.h"
#include "lighting.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <tuple>
#include <array>
#include <algorithm>

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
TacticalActor* faultActor = nullptr;
bool faultArmed = false;
unsigned injectedFailures = 0;
bool FailPartiallyCopiedSwap() noexcept
{
	if (!faultArmed || !faultActor) return false;
	const OBJECTTYPE& left = faultActor->inventory()[BIGPOCK1POS];
	const OBJECTTYPE& right = faultActor->inventory()[BIGPOCK2POS];
	if (left.usItem != COMBAT_KNIFE || right.usItem != FIRSTAIDKIT ||
		right.ubNumberOfObjects != 2 || right.objectStack.size() != 1) return false;
	faultArmed = false;
	++injectedFailures;
	return true;
}
#define CHECK(condition, message) do { if (!(condition)) { \
	std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, message); ++failures; } } while (false)

void MakeObject(OBJECTTYPE& object, UINT16 item, UINT8 quantity, INT16 condition)
{
	object.initialize();
	object.usItem = item;
	object.ubNumberOfObjects = quantity;
	object.objectStack.resize(quantity);
	for (StackedObjectData& stack : object.objectStack) stack.data.objectStatus = condition;
}
}

void* operator new(std::size_t size)
{
	if (FailPartiallyCopiedSwap()) throw std::bad_alloc();
	if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
	throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	const bool copyFailure = argc == 2 && std::strcmp(argv[1], "--copy-failure") == 0;
	const bool missingAnimation = argc == 2 && std::strcmp(argv[1], "--missing-animation") == 0;
	const bool missingLight = argc == 2 && std::strcmp(argv[1], "--missing-light") == 0;
	GameContext& game = GetGameContext();
	const bool running = game.beginInitialization() &&
		game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning();
	CHECK(running, "native inventory fixture starts the bound runtime");
	if (!running) return 1;
	Ja2SoldierRepository& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	ResetJa2TacticalActorRosters();
	ResetJa2TacticalInterruptForNewWorld();
	CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "logical map exists");
	InitRenderParams(0);
	NotifyJa2TacticalWorldLoaded(1);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM;
	is_networked = is_client = is_server = false;
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	gGameExternalOptions.fInventoryCostsAP = TRUE;
	gGameExternalOptions.uWeightDivisor = 0;
	APBPConstants[AP_INV_FROM_BIG_POCKET] = 2;
	APBPConstants[AP_INV_TO_HANDS] = 3;
	APBPConstants[AP_INV_FROM_HANDS] = 3;
	APBPConstants[AP_INV_TO_BIG_POCKET] = 2;
	APBPConstants[AP_INV_MAX_COST] = 20;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{0};
	TacticalActor& actor = *repository.resolve(0);
	actor.identity().id() = SoldierID{0};
	actor.identity().incarnation() = 101;
	actor.identity().profile() = NO_PROFILE;
	actor.identity().bodyType() = REGMALE;
	actor.roster().active() = actor.roster().inSector() = TRUE;
	actor.roster().team() = OUR_TEAM;
	actor.assignment().current() = FIRST_SQUAD;
	actor.position().gridNo() = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS / 2;
	actor.position().level() = FIRST_LEVEL;
	actor.position().direction() = SOUTH;
	actor.pathing().desiredDirection() = SOUTH;
	actor.animationPlayback().state() = CROUCHING;
	actor.animationPlayback().surface() = 0;
	actor.animationIntent().clearPendingAnimations();
	actor.pendingAction().clearAction();
	actor.vitals().health() = actor.vitals().maximumHealth() = 100;
	actor.vitals().breath() = actor.vitals().maximumBreath() = 100;
	actor.actionPoints().current() = 20;
	actor.fireControl().selectBarrelMode(1);
	CHECK(AdoptJa2TacticalEntity(actor), "exact native actor adopted");
	const TacticalEntityId id = GetJa2TacticalEntityId(actor);
	gMAXITEMS_READ = DEFAULT_BPACK + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
	Item[FIRSTAIDKIT].ubPerPocket = 4;
	Item[COMBAT_KNIFE].usItemClass = IC_BLADE;
	Item[COMBAT_KNIFE].ubPerPocket = 4;
	OBJECTTYPE& pocket = actor.inventory()[BIGPOCK1POS];
	OBJECTTYPE& hand = actor.inventory()[HANDPOS];
	MakeObject(pocket, FIRSTAIDKIT, copyFailure ? 2 : 1, 73);
	pocket.ubMission = 7;
	pocket.fFlags = OBJECT_MODIFIED;
	pocket[0]->data.sRepairThreshold = 79;
	pocket[0]->data.sObjectFlag = 0x1020304050607080ULL;
	pocket[0]->data.bTemperature = 12.5f;
	pocket[0]->attachments.resize(4); // Native NAS empty placeholders, not attachments.
	if (copyFailure) MakeObject(actor.inventory()[BIGPOCK2POS], COMBAT_KNIFE, 1, 88);

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
	gubAnimSurfaceIndex[REGMALE][CROUCHING] = 0;
	gubAnimSurfaceItemSubIndex[REGMALE][CROUCHING] = INVALID_ANIMATION;
	const auto cleanup = [&] {
		faultArmed = false; faultActor = nullptr;
		// Release fixture-owned registered shades while their registry is alive,
		// before the repository's process-global actors begin static teardown.
		actor.palette().reset();
		actor.animationCache().reset();
		gAnimSurfaceDatabase[0] = retainedSurface;
		ReleaseWorldTileMap();
	};
	std::uint64_t frame = 0;
	const auto dispatch = [&](const SwapInventorySlotsCommand& command) {
		BeginSimulationCommandFrameBudget(++frame, 1);
		return TryDispatchSimulationCommandNow(SimulationCommand{command});
	};
	SwapInventorySlotsCommand command;
	Ja2CoopInventoryAuthority inventoryAuthority;
	CoopSession::CoopOwnerInventorySnapshot inventoryView;
	const auto worldIdentity = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	CHECK(inventoryAuthority.capture(id, worldIdentity.worldGeneration, inventoryView) &&
		inventoryView.inventoryRevision == 1 && inventoryView.slots.size() == NUM_INV_SLOTS &&
		inventoryView.slots[BIGPOCK1POS].item == FIRSTAIDKIT &&
		inventoryView.slots[BIGPOCK1POS].support == CoopSession::CoopInventorySlotSupport::OrdinarySwappable &&
		inventoryView.slots[BIGPOCK1POS].statusKind == CoopSession::CoopInventoryStatusKind::MedicalKitPoints &&
		inventoryView.slots[BIGPOCK1POS].resourceTotal == (copyFailure ? 146u : 73u),
		"native owner inventory ledger captures the complete dense ordinary-slot summary");
	CHECK(inventoryAuthority.capture(id, worldIdentity.worldGeneration, inventoryView) &&
		inventoryView.inventoryRevision == 1, "unchanged native capture retains its private inventory revision");
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(inventoryAuthority.capture(id, worldIdentity.worldGeneration, inventoryView) &&
		inventoryView.inventoryRevision == 2 && inventoryView.slots[BIGPOCK1POS].item == FIRSTAIDKIT &&
		inventoryView.slots[BIGPOCK1POS].firstCondition == 73,
		"private ordinary metadata changes advance revision despite identical public item summary");
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(inventoryAuthority.capture(id, worldIdentity.worldGeneration, inventoryView) &&
		inventoryView.inventoryRevision == 3, "observed value restoration advances revision rather than reusing an old view token");
	const auto retainedInventoryView = inventoryView;
	CHECK(!inventoryAuthority.capture(id, worldIdentity.worldGeneration + 1, inventoryView) &&
		inventoryView == retainedInventoryView, "wrong-world inventory capture preserves previous caller output");
	actor.inventory()[BIGPOCK3POS].ubMission = 1;
	CHECK(!inventoryAuthority.capture(id, worldIdentity.worldGeneration, inventoryView) &&
		inventoryView == retainedInventoryView, "noncanonical empty storage is never advertised as a usable empty slot");
	actor.inventory()[BIGPOCK3POS].initialize();
	pocket.ubNumberOfObjects += 1;
	CHECK(!inventoryAuthority.capture(id, worldIdentity.worldGeneration, inventoryView) &&
		inventoryView == retainedInventoryView, "malformed stack count cannot manufacture a new inventory revision");
	pocket.ubNumberOfObjects -= 1;
	CHECK(inventoryAuthority.capture(id, worldIdentity.worldGeneration, inventoryView) &&
		inventoryView.inventoryRevision == 3, "rejected native captures did not commit hidden revision changes");
	const UINT8 destination = copyFailure ? BIGPOCK2POS : HANDPOS;
	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, destination, command),
		"preparation supports complete ordinary stacks and canonical NAS placeholders");
	if (!command.expectedWorldGeneration) { cleanup(); return 1; }
	CHECK(command.expectedActionPointCost == (copyFailure ? 0 : 5), "native inventory AP cost captured");
	if (missingAnimation)
	{
		// The current pose remains loaded, but the native equipment refresh
		// cannot resolve its new pose. Pre-mark the failed mapping to avoid a
		// VFS lookup or evicting the externally owned fixture pixel object.
		gubAnimSurfaceIndex[REGMALE][CROUCHING] = FOUND_INVALID_ANIMATION;
	}
	if (copyFailure || missingAnimation)
	{
		faultActor = &actor;
		faultArmed = copyFailure;
		const auto attempted = dispatch(command);
		faultArmed = false;
		CHECK(attempted.status == SimulationCommandDispatchStatus::RetryDeferred &&
			game.commands().size() == 1 && !IsJa2TacticalWorldIntegrityValid(),
			"post-mutation failure poisons the world and never reports Applied");
		CHECK(copyFailure ? injectedFailures == 1 && pocket.usItem == COMBAT_KNIFE :
			hand.usItem == FIRSTAIDKIT && actor.animationPlayback().surface() == INVALID_ANIMATION_SURFACE,
			"fault occurred inside a genuinely mutating native swap or equipment continuation");
		const INT16 remainingAP = actor.actionPoints().current();
		BeginSimulationCommandFrameBudget(++frame, 8);
		const auto retried = ExecuteSimulationCommandsThrough(game.runtime().simulationTicks().completedTickSequence(), 8);
		bool foundDiscarded = false;
		for (const auto& record : game.commandJournal().snapshot())
			if (record.sequence == attempted.sequence)
			{
				CHECK(record.status != CommandJournalStatus::Applied, "failed parent has no Applied journal entry");
				foundDiscarded |= record.status == CommandJournalStatus::Discarded;
			}
		CHECK(retried.discarded == 1 && retried.applied == 0 && game.commands().empty() &&
			foundDiscarded && actor.actionPoints().current() == remainingAP,
			"invalid-integrity retry discards without repeating the copy or AP charge");
		cleanup();
		return failures == 0 ? 0 : 1;
	}

	if (missingLight)
	{
		Item[NIGHTGOGGLES].usItemClass = IC_FACE;
		MakeObject(pocket, NIGHTGOGGLES, 1, 83);
		actor.position().worldXInt() = static_cast<INT16>((actor.position().gridNo() % WORLD_COLS) * CELL_X_SIZE);
		actor.position().worldYInt() = static_cast<INT16>((actor.position().gridNo() / WORLD_COLS) * CELL_Y_SIZE);
		ubAmbientLightLevel = MIN_AMB_LEVEL_FOR_MERC_LIGHTS;
		gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] = TRUE;
		// Exhaust the real native sprite allocator before it can load a light
		// template. No fabricated production hook or installed assets needed.
		for (auto& light : LightSprites) light.uiFlags = LIGHT_SPR_ACTIVE;
		const auto faceObject = CaptureInventorySwapObjectState(id, BIGPOCK1POS);
		CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HEAD1POS, command),
			"face swap can prepare before a genuinely exhausted native light allocation");
		const auto beforeAP = actor.actionPoints().current();
		const auto attempted = dispatch(command);
		CHECK(attempted.status == SimulationCommandDispatchStatus::RetryDeferred &&
			!IsJa2TacticalWorldIntegrityValid() && game.commands().size() == 1 &&
			CaptureInventorySwapObjectState(id, HEAD1POS) == faceObject && !pocket.exists() &&
			actor.actionPoints().current() == beforeAP && !actor.renderState().hasLightSprite(),
			"required-light allocation failure after an exact face swap poisons instead of reporting Applied or charging AP");
		BeginSimulationCommandFrameBudget(++frame, 8);
		const auto retry = ExecuteSimulationCommandsThrough(game.runtime().simulationTicks().completedTickSequence(), 8);
		CHECK(retry.discarded == 1 && retry.applied == 0 && game.commands().empty() &&
			CaptureInventorySwapObjectState(id, HEAD1POS) == faceObject && !pocket.exists() &&
			actor.actionPoints().current() == beforeAP,
			"failed light continuation cannot replay the swap or spend AP on retry");
		for (const auto& record : game.commandJournal().snapshot())
			if (record.sequence == attempted.sequence)
				CHECK(record.status == CommandJournalStatus::Discarded, "failed lighting parent has no Applied journal record");
		for (auto& light : LightSprites) light.uiFlags = 0;
		cleanup(); return failures ? 1 : 0;
	}

	// Equipment effects must never replace an active traversal or queued native
	// continuation, including work that appears after a command was prepared.
	const auto idleIntent = actor.animationIntent();
	const auto idleActivity = actor.animationActivity();
	const auto idleMovement = actor.movement();
	const auto idlePath = actor.pathing();
	const auto idleSchedule = actor.schedule();
	const auto idlePending = actor.pendingAction();
	const auto idleFire = actor.fireControl();
	const auto idleFlags = actor.status().flags();
	const auto idleDelayedDamage = actor.runtime().pendingAction.delayedDamage;
	const auto pendingState = [&] {
		std::array<UINT16, MAX_PATH_LIST_SIZE> path{};
		std::copy_n(actor.pathing().path(), path.size(), path.begin());
		return std::make_tuple(actor.animationPlayback().state(), actor.animationIntent().pendingAnimation(),
			actor.animationIntent().secondaryPendingAnimation(), actor.animationIntent().pendingStance(),
			actor.animationIntent().pendingDirection(), actor.pathing().desiredDirection(), actor.animationIntent().continuationMode(),
			actor.animationIntent().stopPendingNextTile(), actor.animationActivity().turningToShoot(),
			actor.animationActivity().turningUntilDone(), actor.animationActivity().turningFromProneMode(),
			actor.animationActivity().hitPhase(), actor.animationActivity().paused(),
			actor.schedule().id(), actor.schedule().doorOpenPhase(), actor.schedule().doorGrid(),
			actor.movement().delayCounter(), actor.movement().continuedPathValid(), actor.movement().paused(),
			actor.pathing().pathIndex(), actor.pathing().pathSize(), actor.pathing().finalDestinationGrid(), path,
			actor.pendingAction().action(), actor.status().flags(), actor.fireControl().bulletsLeft(),
			static_cast<bool>(actor.runtime().pendingAction.delayedDamage));
	};
	const auto rejectsBusyState = [&](auto makeBusy, const char* message) {
		SwapInventorySlotsCommand retained;
		if (!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, retained))
		{
			CHECK(false, "ordinary idle state prepares before pending-work regression");
			return false;
		}
		makeBusy();
		const auto pending = pendingState();
		const auto sourceObject = CaptureInventorySwapObjectState(id, BIGPOCK1POS);
		const auto destinationObject = CaptureInventorySwapObjectState(id, HANDPOS);
		const INT16 beforeAP = actor.actionPoints().current();
		SwapInventorySlotsCommand unchanged; unchanged.expectedWorldGeneration = 999;
		const bool rejected = !PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, unchanged);
		const auto executed = dispatch(retained);
		const bool intact = rejected && unchanged.expectedWorldGeneration == 999 &&
			executed.status == SimulationCommandDispatchStatus::Discarded && game.commands().empty() &&
			pendingState() == pending && actor.actionPoints().current() == beforeAP &&
			CaptureInventorySwapObjectState(id, BIGPOCK1POS) == sourceObject &&
			CaptureInventorySwapObjectState(id, HANDPOS) == destinationObject && IsJa2TacticalWorldIntegrityValid();
		CHECK(intact, message);
		actor.animationPlayback().state() = CROUCHING;
		actor.animationIntent() = idleIntent; actor.animationActivity() = idleActivity;
		actor.movement() = idleMovement; actor.pathing() = idlePath; actor.schedule() = idleSchedule;
		actor.pendingAction() = idlePending; actor.fireControl() = idleFire;
		actor.status().flags() = idleFlags; actor.runtime().pendingAction.delayedDamage = idleDelayedDamage;
		return intact;
	};
	CHECK((gAnimControl[HOPFENCE].uiFlags & ANIM_STATIONARY) != 0,
		"actual native HOPFENCE is stationary and requires an explicit idle-pose whitelist");
	if (!rejectsBusyState([&] { actor.animationPlayback().state() = HOPFENCE; },
			"active native HOPFENCE rejects preparation and retained execution without changing objects/AP") ||
		!rejectsBusyState([&] { actor.animationIntent().queueAnimation(HOPFENCE); }, "pending HOPFENCE is preserved") ||
		!rejectsBusyState([&] { actor.animationIntent().queueSecondaryAnimation(HOPFENCE); }, "secondary traversal is preserved") ||
		!rejectsBusyState([&] { actor.animationIntent().queueStance(ANIM_PRONE); }, "pending stance is preserved") ||
		!rejectsBusyState([&] { actor.animationIntent().queueDirection(EAST); }, "pending direction is preserved") ||
		!rejectsBusyState([&] { actor.pathing().desiredDirection() = EAST; }, "native desired facing is preserved without queued direction") ||
		!rejectsBusyState([&] { actor.animationIntent().continueAfterStance(); }, "stance continuation is preserved") ||
		!rejectsBusyState([&] { actor.animationIntent().requestStopAtNextTile(); }, "pending tile stop is preserved") ||
		!rejectsBusyState([&] { actor.animationActivity().turningToShoot() = TRUE; }, "pending shooting turn is preserved") ||
		!rejectsBusyState([&] { actor.animationActivity().turningUntilDone() = TRUE; }, "native turn is preserved") ||
		!rejectsBusyState([&] { actor.animationActivity().turningFromProneMode() = 1; }, "prone turn is preserved") ||
		!rejectsBusyState([&] { actor.animationActivity().hitPhase() = 1; }, "hit recovery is preserved") ||
		!rejectsBusyState([&] { actor.animationActivity().paused() = TRUE; }, "paused native animation is preserved") ||
		!rejectsBusyState([&] { actor.schedule().id() = 1; }, "assigned native schedule is preserved") ||
		!rejectsBusyState([&] { actor.schedule().beginDoorContinuation(actor.position().gridNo()+1); }, "scheduled door continuation is preserved") ||
		!rejectsBusyState([&] { actor.pathing().pathSize() = 1; actor.pathing().pathIndex() = 0; }, "unconsumed native route is preserved") ||
		!rejectsBusyState([&] { actor.pathing().pathSize() = actor.pathing().pathIndex() = 1;
			actor.pathing().finalDestinationGrid() = actor.position().gridNo()+1; }, "incomplete stopped route is preserved") ||
		!rejectsBusyState([&] { actor.movement().waitForGrid(actor.position().gridNo()+1, 1); }, "delayed tile wait is preserved") ||
		!rejectsBusyState([&] { actor.movement().setContinuedPath(actor.position().gridNo()+1); }, "continued route is preserved") ||
		!rejectsBusyState([&] { actor.movement().paused() = TRUE; }, "paused movement is preserved") ||
		!rejectsBusyState([&] { actor.pendingAction().begin(1); }, "pending native action is preserved") ||
		!rejectsBusyState([&] { actor.status().flags() |= SOLDIER_LOCKPENDINGACTIONCOUNTER; }, "pending-action lock is preserved") ||
		!rejectsBusyState([&] { actor.fireControl().bulletsLeft() = 1; }, "pending bullets are preserved") ||
		!rejectsBusyState([&] { actor.runtime().pendingAction.delayedDamage = [] {}; }, "delayed native damage is preserved"))
	{
		cleanup(); return 1;
	}

	const auto original = CaptureInventorySwapObjectState(id, BIGPOCK1POS);
	const std::size_t recordsBefore = game.commandJournal().size();
	const auto equipped = dispatch(command);
	CHECK(equipped.status == SimulationCommandDispatchStatus::Applied && game.commands().empty() &&
		game.commandJournal().size() == recordsBefore + 1 && actor.actionPoints().current() == 15 &&
		CaptureInventorySwapObjectState(id, HANDPOS) == original && !pocket.exists() &&
		IsJa2TacticalWorldIntegrityValid(), "one retained parent equips kit, preserves object and charges AP once, with no child commands");
	CHECK(hand[0]->data.sRepairThreshold == 79 && hand[0]->data.sObjectFlag == 0x1020304050607080ULL &&
		hand[0]->data.bTemperature == 12.5f && hand[0]->attachments.size() == 4 && hand.ubMission == 7,
		"complete moved object retains common fields and empty NAS layout");
	SwapInventorySlotsCommand unequip;
	CHECK(PrepareSwapInventorySlotsCommand(id, HANDPOS, BIGPOCK1POS, unequip), "kit can be unequipped");
	CHECK(dispatch(unequip).status == SimulationCommandDispatchStatus::Applied &&
		actor.actionPoints().current() == 10 && CaptureInventorySwapObjectState(id, BIGPOCK1POS) == original,
		"reverse operation moves exactly the original kit back");

	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, command), "prepare stale-object case");
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded && !hand.exists() &&
		actor.actionPoints().current() == 10 && IsJa2TacticalWorldIntegrityValid(), "same item/count but changed metadata rejects without effects");
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, command), "prepare AP-drift case");
	actor.actionPoints().current() = 9;
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded && !hand.exists() &&
		actor.actionPoints().current() == 9, "exact actor AP drift rejects without deduction");
	actor.actionPoints().current() = 20;
	SwapInventorySlotsCommand sentinel; sentinel.expectedWorldGeneration = 999;
	pocket[0]->data.bTrap = 1;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel) &&
		sentinel.expectedWorldGeneration == 999 && CaptureInventorySwapObjectState(id, BIGPOCK1POS) == 0,
		"trapped state is unsupported and rejection leaves output untouched");
	pocket[0]->data.bTrap = 0;
	MakeObject(pocket[0]->attachments.front(), COMBAT_KNIFE, 1, 80);
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel), "actual attachments reject without implicit graph operations");
	pocket[0]->attachments.front().initialize();
	pocket.ubNumberOfObjects = 2; pocket.objectStack.resize(2);
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel), "hand capacity rejects partial-stack movement");
	pocket.ubNumberOfObjects = 1; pocket.objectStack.resize(1);
	actor.service().beginProvidingTo(SoldierID{1});
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel), "active medical service cannot be silently cancelled by inventory");
	actor.service().finishProviding();
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, BIGPOCK1POS, sentinel) &&
		!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, VESTPOCKPOS, sentinel), "same slot and worn LBE editing are unsupported");
	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, command), "prepare exact identity rejection");
	command.soldier.incarnation += 1;
	CHECK(dispatch(command).status != SimulationCommandDispatchStatus::Applied && !hand.exists() &&
		actor.actionPoints().current() == 20, "reused slot incarnation cannot mutate the live actor");
	MakeObject(actor.inventory()[SECONDHANDPOS], COMBAT_KNIFE, 1, 80);
	Item[FIRSTAIDKIT].usItemFlag |= ITEM_twohanded;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel) &&
		actor.inventory()[SECONDHANDPOS].usItem == COMBAT_KNIFE && !hand.exists(),
		"two-handed result cannot silently relocate an occupied offhand");
	Item[FIRSTAIDKIT].usItemFlag &= ~ITEM_twohanded;
	actor.inventory()[SECONDHANDPOS].initialize();
	actor.actionPoints().current() = 4;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel) && !hand.exists(),
		"insufficient native inventory AP fails before copying");
	actor.actionPoints().current() = 20;
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, ENEMY_TEAM, 0);
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel) &&
		CaptureInventorySwapObjectState(id, BIGPOCK1POS) == original,
		"object capture remains readable during enemy turn but authority swap is gated");
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);

	// Spider's purchased kit arrives beside an ordinary offhand canteen. The
	// unchanged canteen must have an exact native proof without inventing water
	// points or treating its first-status union as equipment condition.
	{
		OBJECTTYPE& offhand = actor.inventory()[SECONDHANDPOS];
		OBJECTTYPE& spare = actor.inventory()[BIGPOCK2POS];
		Item[CANTEEN].usItemClass = IC_MISC;
		Item[CANTEEN].ubPerPocket = 2;
		MakeObject(offhand, CANTEEN, 1, 63);
		offhand.ubMission = 9;
		offhand.fFlags = OBJECT_MODIFIED;
		offhand[0]->data.bTemperature = 31.25f;
		offhand[0]->data.sObjectFlag = 0x123456789ABCDEF0ULL;
		offhand[0]->attachments.resize(2);
		const auto canteen = CaptureInventorySwapObjectState(id, SECONDHANDPOS);
		CoopSession::CoopOwnerInventorySnapshot miscView;
		CHECK(canteen != 0 && inventoryAuthority.capture(id, worldIdentity.worldGeneration, miscView) &&
			miscView.slots[SECONDHANDPOS].item == CANTEEN &&
			miscView.slots[SECONDHANDPOS].support == CoopSession::CoopInventorySlotSupport::OrdinarySwappable &&
			miscView.slots[SECONDHANDPOS].statusKind == CoopSession::CoopInventoryStatusKind::Unknown &&
			miscView.slots[SECONDHANDPOS].firstCondition == 63 && miscView.slots[SECONDHANDPOS].resourceTotal == 0,
			"plain canteen has an exact swappable owner summary with no invented resource metric");
		const bool ready = PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, command);
		CHECK(ready, "medical kit equips with an unchanged ordinary canteen in offhand");
		if (!ready) { cleanup(); return 1; }
		CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
			CaptureInventorySwapObjectState(id, HANDPOS) == original && !pocket.exists() &&
			CaptureInventorySwapObjectState(id, SECONDHANDPOS) == canteen && actor.actionPoints().current() == 15,
			"medical equip preserves the entire unrelated canteen and charges AP once");
		CHECK(PrepareSwapInventorySlotsCommand(id, HANDPOS, BIGPOCK1POS, command) &&
			dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
			CaptureInventorySwapObjectState(id, BIGPOCK1POS) == original && !hand.exists() &&
			CaptureInventorySwapObjectState(id, SECONDHANDPOS) == canteen && actor.actionPoints().current() == 10,
			"medical unequip also preserves the exact canteen");
		CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, command), "prepare unchanged-misc proof drift");
		offhand[0]->data.sObjectFlag ^= 1;
		CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded && !hand.exists() &&
			CaptureInventorySwapObjectState(id, BIGPOCK1POS) == original && actor.actionPoints().current() == 10,
			"changed offhand misc metadata rejects the retained medical equip without effects");
		offhand[0]->data.sObjectFlag ^= 1;
		CHECK(PrepareSwapInventorySlotsCommand(id, SECONDHANDPOS, BIGPOCK2POS, command) &&
			dispatch(command).status == SimulationCommandDispatchStatus::Applied && !offhand.exists() &&
			CaptureInventorySwapObjectState(id, BIGPOCK2POS) == canteen && actor.actionPoints().current() == 5,
			"ordinary canteen can be stowed through the same checked native equipment path");
		CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK2POS, SECONDHANDPOS, command) &&
			dispatch(command).status == SimulationCommandDispatchStatus::Applied && !spare.exists() &&
			CaptureInventorySwapObjectState(id, SECONDHANDPOS) == canteen && actor.actionPoints().current() == 0,
			"canteen stow and restore conserve exact metadata and native AP");
		actor.actionPoints().current() = 20;
		offhand[0]->data.bTrap = 1;
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel) &&
			CaptureInventorySwapObjectState(id, SECONDHANDPOS) == 0, "trapped misc remains unsupported");
		offhand[0]->data.bTrap = 0;
		offhand.fFlags |= OBJECT_ARMED_BOMB;
		CHECK(CaptureInventorySwapObjectState(id, SECONDHANDPOS) == 0, "armed misc remains unsupported");
		offhand.fFlags &= ~OBJECT_ARMED_BOMB;
		MakeObject(offhand[0]->attachments.front(), COMBAT_KNIFE, 1, 80);
		CHECK(CaptureInventorySwapObjectState(id, SECONDHANDPOS) == 0, "attached misc remains unsupported");
		offhand[0]->attachments.front().initialize();
		offhand[0]->data.gun.ubGunAmmoType = UINT8_MAX;
		CHECK(CaptureInventorySwapObjectState(id, SECONDHANDPOS) == 0, "active-LBE alias cannot masquerade as misc");
		offhand[0]->data.gun.ubGunAmmoType = 0;
		offhand[0]->data.gun.usGunAmmoItem = 1;
		CHECK(CaptureInventorySwapObjectState(id, SECONDHANDPOS) == 0, "alternate union payload cannot masquerade as misc");
		offhand[0]->data.gun.usGunAmmoItem = 0;
		CHECK(CaptureInventorySwapObjectState(id, SECONDHANDPOS) == canteen, "negative misc cases restore exact unchanged storage");
		for (UINT16 marker : {UINT16(SWITCH), UINT16(ACTION_ITEM), UINT16(OWNERSHIP)})
		{
			const auto oldClass = Item[marker].usItemClass;
			Item[marker].usItemClass = IC_MISC;
			MakeObject(offhand, marker, 1, 100);
			CHECK(CaptureInventorySwapObjectState(id, SECONDHANDPOS) == 0,
				"world control markers are never ordinary inventory even with a misc class and empty payload");
			Item[marker].usItemClass = oldClass;
		}
		offhand.initialize();
		CHECK(!hand.exists() && !spare.exists() && CaptureInventorySwapObjectState(id, BIGPOCK1POS) == original,
			"misc regression leaves the original kit and other slots intact");
	}

	// Ordinary armour and face gear reuse the same retained swap and native
	// placement/AP rules. Only the two named slots may exchange objects.
	{
		OBJECTTYPE retainedPocket;
		std::swap(pocket, retainedPocket);
		APBPConstants[AP_INV_TO_EQUIPMENT] = 7; APBPConstants[AP_INV_FROM_EQUIPMENT] = 6;
		APBPConstants[AP_INV_TO_FACE] = 5; APBPConstants[AP_INV_FROM_FACE] = 4;
		const auto oldAmbient = ubAmbientLightLevel;
		const auto oldCastsLight = gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT];
		ubAmbientLightLevel = 0; gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] = TRUE;
		const UINT16 armourItems[] = {STEEL_HELMET, FLAK_JACKET, KEVLAR_LEGGINGS};
		const UINT8 armourSlots[] = {HELMETPOS, VESTPOS, LEGPOS};
		const UINT8 armourClasses[] = {ARMOURCLASS_HELMET, ARMOURCLASS_VEST, ARMOURCLASS_LEGGINGS};
		for (UINT8 index = 0; index < 3; ++index)
		{
			Item[armourItems[index]].usItemClass = IC_ARMOUR;
			Item[armourItems[index]].ubClassIndex = index;
			Item[armourItems[index]].ubPerPocket = 2;
			Armour[index].ubArmourClass = armourClasses[index];
			Armour[index].uiIndex = index;
		}
		for (UINT16 item : {UINT16(NIGHTGOGGLES), UINT16(SUNGOGGLES), UINT16(GASMASK)})
		{ Item[item].usItemClass = IC_FACE; Item[item].ubPerPocket = 2; }
		const auto swapGear = [&](UINT8 from, UINT8 to, const char* message) {
			SwapInventorySlotsCommand prepared;
			const auto fromState = CaptureInventorySwapObjectState(id, from);
			const auto toState = CaptureInventorySwapObjectState(id, to);
			const INT16 expectedCost = static_cast<INT16>(
				(actor.inventory()[from].exists() ? GetInvMovementCost(&actor.inventory()[from], from, to) : 0) +
				(actor.inventory()[to].exists() ? GetInvMovementCost(&actor.inventory()[to], to, from) : 0));
			actor.actionPoints().current() = 100;
			const auto previousRecords = game.commandJournal().size();
			const bool ready = PrepareSwapInventorySlotsCommand(id, from, to, prepared);
			CHECK(ready && prepared.expectedActionPointCost == expectedCost, "equipment cost uses the stock source/destination categories");
			const bool applied = ready && dispatch(prepared).status == SimulationCommandDispatchStatus::Applied;
			CHECK(applied && CaptureInventorySwapObjectState(id, from) == toState &&
				CaptureInventorySwapObjectState(id, to) == fromState && actor.actionPoints().current() == 100 - expectedCost &&
				game.commands().empty() && game.commandJournal().size() == previousRecords + 1 &&
				IsJa2TacticalWorldIntegrityValid(), message);
			return applied;
		};
		for (UINT8 index = 0; index < 3; ++index)
		{
			MakeObject(pocket, armourItems[index], 1, 83);
			pocket[0]->data.sObjectFlag = 0x2000000000000000ULL;
			const auto armourState = CaptureInventorySwapObjectState(id, BIGPOCK1POS);
			CHECK(swapGear(BIGPOCK1POS, armourSlots[index], "native ordinary armour equips into its matching body slot") &&
				swapGear(armourSlots[index], BIGPOCK1POS, "native ordinary armour unequips without autostowing other objects") &&
				CaptureInventorySwapObjectState(id, BIGPOCK1POS) == armourState, "every ordinary armour slot roundtrips exact metadata");
			for (UINT8 wrong : armourSlots)
				if (wrong != armourSlots[index])
					CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, wrong, sentinel), "native armour-class mismatch rejects");
			pocket.ubNumberOfObjects = 2; pocket.objectStack.resize(2);
			CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, armourSlots[index], sentinel),
				"a worn armour slot never splits a multi-object source stack");
			pocket.ubNumberOfObjects = 1; pocket.objectStack.resize(1);
		}
		Item[KEVLAR_LEGGINGS].ubClassIndex = UINT16_MAX;
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, LEGPOS, sentinel), "invalid armour table index fails before native lookup");
		Item[KEVLAR_LEGGINGS].ubClassIndex = MAXITEMS;
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, LEGPOS, sentinel), "unloaded armour sentinel index cannot masquerade as a body definition");
		Item[KEVLAR_LEGGINGS].ubClassIndex = 2;
		Armour[2].uiIndex = 0;
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, LEGPOS, sentinel), "missing native armour definition cannot be inferred from a default class byte");
		Armour[2].uiIndex = 2;
		MakeObject(pocket, FIRSTAIDKIT, 1, 80);
		for (UINT8 slot : {HELMETPOS, VESTPOS, LEGPOS, HEAD1POS, HEAD2POS})
			CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, slot, sentinel), "non-equipment item cannot occupy a worn slot");

		MakeObject(pocket, NIGHTGOGGLES, 1, 84);
		CHECK(swapGear(BIGPOCK1POS, HEAD1POS, "daylight face equip is successful with no unnecessary light allocation") &&
			!actor.renderState().hasLightSprite(), "daylight face continuation is a valid no-op");
		MakeObject(pocket, SUNGOGGLES, 1, 85);
		std::memset(CompatibleFaceItems, 0, sizeof(CompatibleFaceItems));
		const auto firstFace = CaptureInventorySwapObjectState(id, HEAD1POS);
		const auto facePocket = CaptureInventorySwapObjectState(id, BIGPOCK1POS);
		const auto rejectedAP = actor.actionPoints().current();
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HEAD2POS, sentinel) &&
			CaptureInventorySwapObjectState(id, HEAD1POS) == firstFace &&
			CaptureInventorySwapObjectState(id, BIGPOCK1POS) == facePocket && actor.actionPoints().current() == rejectedAP,
			"native face compatibility rejects incompatible pair without changing either object or AP");
		CompatibleFaceItems[0][0] = SUNGOGGLES; CompatibleFaceItems[0][1] = NIGHTGOGGLES;
		CompatibleFaceItems[1][0] = NIGHTGOGGLES; CompatibleFaceItems[1][1] = SUNGOGGLES;
		CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HEAD2POS, command), "compatible pair prepares");
		actor.inventory()[HEAD1POS][0]->data.sObjectFlag ^= 1;
		CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded && !actor.inventory()[HEAD2POS].exists() &&
			actor.actionPoints().current() == rejectedAP, "changed untouched face metadata invalidates retained pair proof");
		actor.inventory()[HEAD1POS][0]->data.sObjectFlag ^= 1;
		CHECK(swapGear(BIGPOCK1POS, HEAD2POS, "second face slot obeys native compatible-pair admission") &&
			swapGear(HEAD1POS, HEAD2POS, "two occupied face slots are validated as their projected final pair"),
			"compatible face objects swap exactly without an intermediate self-pair or autostow");
		// No light is required when the option is disabled, even at night and
		// even if no native sprite is available to allocate.
		ubAmbientLightLevel = MIN_AMB_LEVEL_FOR_MERC_LIGHTS;
		gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] = FALSE;
		for (auto& light : LightSprites) light.uiFlags = LIGHT_SPR_ACTIVE;
		CHECK(swapGear(HEAD1POS, BIGPOCK1POS, "disabled nighttime personal lighting is a successful equipment no-op") &&
			!actor.renderState().hasLightSprite(), "disabled lighting does not report a false resource failure");
		for (auto& light : LightSprites) light.uiFlags = 0;
		pocket.initialize();
		CHECK(swapGear(HEAD2POS, BIGPOCK1POS, "last face item can be removed with exact AP and metadata"), "face slots return empty");
		pocket.ubNumberOfObjects = 2; pocket.objectStack.resize(2);
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HEAD1POS, sentinel), "face slots also require exactly one object");
		pocket.ubNumberOfObjects = 1; pocket.objectStack.resize(1);
		MakeObject(actor.inventory()[HEAD1POS], SUNGOGGLES, 1, 85);
		for (auto& row : CompatibleFaceItems) { row[0] = SUNGOGGLES; row[1] = GASMASK; }
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HEAD2POS, sentinel),
			"unterminated native face compatibility data rejects without an unbounded table walk");
		std::memset(CompatibleFaceItems, 0, sizeof(CompatibleFaceItems));
		actor.inventory()[HEAD1POS].initialize(); pocket.initialize();

		// The stock Nails vest preference is preserved without running dialogue.
		actor.identity().profile() = 34;
		MakeObject(actor.inventory()[VESTPOS], LEATHER_JACKET, 1, 86);
		Item[LEATHER_JACKET].usItemClass = IC_ARMOUR; Item[LEATHER_JACKET].ubClassIndex = 1;
		Item[LEATHER_JACKET].usItemFlag2 |= ITEM_leatherjacket;
		CHECK(!PrepareSwapInventorySlotsCommand(id, VESTPOS, BIGPOCK1POS, sentinel), "Nails will not remove his vest");
		MakeObject(pocket, FLAK_JACKET, 1, 87);
		CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, VESTPOS, sentinel), "Nails refuses a non-leather replacement vest");
		MakeObject(pocket, LEATHER_JACKET, 1, 88);
		CHECK(swapGear(BIGPOCK1POS, VESTPOS, "Nails accepts a native leather-jacket replacement without dialogue"),
			"Nails ordinary replacement preserves the stock leather rule");
		actor.identity().profile() = NO_PROFILE;
		actor.inventory()[VESTPOS].initialize(); pocket.initialize();
		for (UINT8 slot : {HELMETPOS, VESTPOS, LEGPOS, HEAD1POS, HEAD2POS})
		{
			CHECK(IsSupportedInventorySwapSlot(slot), "ordinary same-actor equipment slots are supported");
		}
		std::swap(pocket, retainedPocket);
		actor.actionPoints().current() = 20;
		ubAmbientLightLevel = oldAmbient; gGameSettings.fOptions[TOPTION_MERC_CASTS_LIGHT] = oldCastsLight;
		CHECK(CaptureInventorySwapObjectState(id, BIGPOCK1POS) == original,
			"equipment fixture restores the original ordinary pocket before remaining regressions");
	}

	// Exercise native NIV slot policy with bounded in-memory pocket metadata.
	gGameOptions.ubInventorySystem = INVENTORY_NEW;
	gGameExternalOptions.fInventoryCostsAP = FALSE;
	gGameExternalOptions.guiMaxItemSize = 1;
	LoadBearingEquipment.resize(1);
	LoadBearingEquipment[0].lbePocketIndex.assign(12, 1);
	LBEPocketType.resize(2);
	LBEPocketType[1].ItemCapacityPerSize.assign(2, 4);
	for (UINT16 gear : {UINT16(DEFAULT_CPACK), UINT16(DEFAULT_BPACK), UINT16(DEFAULT_VEST), UINT16(DEFAULT_THIGH)})
	{
		Item[gear].usItemClass = IC_LBEGEAR;
		Item[gear].ubClassIndex = 0;
	}
	MakeObject(actor.inventory()[BIGPOCK3POS], STEEL_HELMET, 1, 91);
	const auto nivHelmet = CaptureInventorySwapObjectState(id, BIGPOCK3POS);
	const auto nivEquipmentAP = actor.actionPoints().current();
	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK3POS, HELMETPOS, command) &&
		command.expectedActionPointCost == 0 && dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		CaptureInventorySwapObjectState(id, HELMETPOS) == nivHelmet &&
		PrepareSwapInventorySlotsCommand(id, HELMETPOS, BIGPOCK3POS, command) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		CaptureInventorySwapObjectState(id, BIGPOCK3POS) == nivHelmet && actor.actionPoints().current() == nivEquipmentAP,
		"NIV worn body slots bypass pocket metadata while preserving native source access and disabled AP costs");
	actor.inventory()[BIGPOCK3POS].initialize();
	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, command) &&
		command.expectedActionPointCost == 0, "ordinary NIV pocket resolves its native default gear and capacity");
	LBEPocketType[1].ItemCapacityPerSize[0] = 0;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, BIGPOCK2POS, sentinel),
		"zero destination capacity rejects even when the destination is empty");
	LBEPocketType[1].ItemCapacityPerSize[0] = 4;
	MakeObject(actor.inventory()[BIGPOCK4POS], FIRSTAIDKIT, 1, 90);
	actor.inventory().zipperFlag() = FALSE;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK4POS, HANDPOS, sentinel),
		"closed backpack cannot be accessed in combat through read-only CanItemFit shortcut");
	actor.inventory().zipperFlag() = TRUE;
	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK4POS, HANDPOS, command),
		"open backpack is accessible while crouched");
	actor.animationPlayback().state() = STANDING;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK4POS, HANDPOS, sentinel),
		"standing actor cannot use even an open backpack in combat");
	actor.animationPlayback().state() = CROUCHING;
	actor.inventory()[BIGPOCK4POS].initialize();
	MakeObject(actor.inventory()[CPACKPOCKPOS], DEFAULT_CPACK, 1, 100);
	actor.inventory()[CPACKPOCKPOS][0]->data.lbe.bLBE = -1;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel),
		"active LBE graphs cannot become an implicit inventory continuation");
	actor.inventory()[CPACKPOCKPOS].initialize();
	LoadBearingEquipment[0].lbePocketIndex[icPocket[BIGPOCK1POS]] = 0;
	CHECK(!PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, sentinel),
		"disabled or attachment-supplied NIV pockets are outside the bounded swap slice");
	LoadBearingEquipment[0].lbePocketIndex[icPocket[BIGPOCK1POS]] = 1;
	CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK1POS, HANDPOS, command) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		CaptureInventorySwapObjectState(id, HANDPOS) == original && actor.actionPoints().current() == 20,
		"actual retained NIV equip works with the native no-inventory-AP option");

	// Match the installed candidate's occupied-hand and two-loader shapes, but
	// use explicitly synthetic definitions and the in-memory crouching surface.
	// These are native command regressions, not installed item/animation proof.
	gGameExternalOptions.fInventoryCostsAP = TRUE;
	gGameExternalOptions.guiMaxItemSize = 14;
	LBEPocketType[1].ItemCapacityPerSize.assign(15, 3);
	APBPConstants[AP_INV_FROM_VEST] = 4;
	APBPConstants[AP_INV_TO_VEST] = 6;
	Item[SW38].usItemClass = IC_GUN;
	Item[SW38].ubPerPocket = 1;
	Item[SW38].usItemFlag &= ~ITEM_twohanded;
	Weapon[SW38].ubShotsPerBurst = 0;
	Weapon[SW38].NoSemiAuto = FALSE;
	Item[LOCKSMITHKIT].usItemClass = IC_KIT;
	Item[LOCKSMITHKIT].ubPerPocket = 1;
	Item[LOCKSMITHKIT].usItemFlag &= ~ITEM_twohanded;
	Item[CLIP38_6].usItemClass = IC_AMMO;
	Item[CLIP38_6].ItemSize = 14;
	Item[CLIP38_6].ubPerPocket = 8;
	OBJECTTYPE& offhand = actor.inventory()[SECONDHANDPOS];
	MakeObject(hand, SW38, 1, 94);
	hand.ubMission = 3;
	hand.fFlags = OBJECT_MODIFIED;
	hand[0]->data.gun.ubGunAmmoType = 1;
	hand[0]->data.gun.ubGunShotsLeft = 6;
	hand[0]->data.gun.usGunAmmoItem = CLIP38_6;
	hand[0]->data.gun.bGunAmmoStatus = 100;
	hand[0]->data.gun.ubGunState = GS_CARTRIDGE_IN_CHAMBER;
	hand[0]->data.sRepairThreshold = 98;
	hand[0]->data.sObjectFlag = 0x1122334455667788ULL;
	hand[0]->data.bTemperature = 3.5f;
	hand[0]->attachments.resize(3);
	MakeObject(offhand, LOCKSMITHKIT, 1, 99);
	offhand.ubMission = 4;
	offhand[0]->data.sObjectFlag = 0x8877665544332211ULL;
	offhand[0]->data.sRepairThreshold = 100;
	offhand[0]->attachments.resize(2);
	const auto originalGun = CaptureInventorySwapObjectState(id, HANDPOS);
	const auto originalKit = CaptureInventorySwapObjectState(id, SECONDHANDPOS);
	CHECK(originalGun != 0 && originalKit != 0 && originalGun != originalKit,
		"loaded gun and kit have distinct complete ordinary-state fingerprints");
	const auto swapAtZeroCost = [&](UINT8 source, UINT8 target) {
		SwapInventorySlotsCommand prepared;
		const bool ready = PrepareSwapInventorySlotsCommand(id, source, target, prepared);
		CHECK(ready, "same-type occupied-hand or vest-pocket swap prepares natively");
		if (!ready) return false;
		CHECK(prepared.expectedActionPointCost == 0,
			"same native slot type costs zero AP even with inventory AP enabled in combat");
		if (prepared.expectedActionPointCost != 0) return false;
		const INT16 previousAP = actor.actionPoints().current();
		const auto previousRecords = game.commandJournal().size();
		const auto result = dispatch(prepared);
		CHECK(result.status == SimulationCommandDispatchStatus::Applied && game.commands().empty() &&
			game.commandJournal().size() == previousRecords + 1 &&
			actor.actionPoints().current() == previousAP && IsJa2TacticalWorldIntegrityValid(),
			"one retained parent applies without nested commands or charging zero-cost inventory AP");
		return result.status == SimulationCommandDispatchStatus::Applied;
	};
	CHECK(swapAtZeroCost(SECONDHANDPOS, HANDPOS) &&
		CaptureInventorySwapObjectState(id, HANDPOS) == originalKit &&
		CaptureInventorySwapObjectState(id, SECONDHANDPOS) == originalGun,
		"occupied hand swap moves the entire loaded gun and kit in both directions");
	CHECK(swapAtZeroCost(HANDPOS, SECONDHANDPOS) &&
		CaptureInventorySwapObjectState(id, HANDPOS) == originalGun &&
		CaptureInventorySwapObjectState(id, SECONDHANDPOS) == originalKit &&
		hand[0]->data.gun.ubGunShotsLeft == 6 && hand[0]->data.gun.usGunAmmoItem == CLIP38_6 &&
		hand[0]->data.gun.ubGunState == GS_CARTRIDGE_IN_CHAMBER &&
		hand[0]->data.sObjectFlag == 0x1122334455667788ULL && hand[0]->attachments.size() == 3 &&
		offhand[0]->data.objectStatus == 99 && offhand[0]->attachments.size() == 2,
		"occupied hand roundtrip restores both complete fingerprints and loaded gun/kit metadata");

	// Slots 25/26 are vest pockets 0/1, not backpack pockets. Wearing a plain
	// inactive fixture LBE also exercises the worn-gear access path, rather than
	// only the default-gear fallback used above.
	static_assert(SMALLPOCK1POS == 25 && SMALLPOCK2POS == 26, "native vest slot mapping");
	MakeObject(actor.inventory()[VESTPOCKPOS], DEFAULT_VEST, 1, 97);
	OBJECTTYPE& ammunition = actor.inventory()[SMALLPOCK1POS];
	OBJECTTYPE& emptyVestPocket = actor.inventory()[SMALLPOCK2POS];
	MakeObject(ammunition, CLIP38_6, 2, 6);
	ammunition.ubMission = 5;
	ammunition.fFlags = OBJECT_MODIFIED;
	ammunition[0]->data.sObjectFlag = 0x1234567890abcdefULL;
	ammunition[0]->attachments.resize(1);
	ammunition[1]->data.ubShotsLeft = 3;
	ammunition[1]->data.sObjectFlag = 0xfedcba0987654321ULL;
	ammunition[1]->data.bTemperature = 1.25f;
	ammunition[1]->attachments.resize(2);
	emptyVestPocket.initialize();
	const auto originalAmmunition = CaptureInventorySwapObjectState(id, SMALLPOCK1POS);
	const auto originalEmpty = CaptureInventorySwapObjectState(id, SMALLPOCK2POS);
	CHECK(originalAmmunition != 0 && originalEmpty != 0 && originalAmmunition != originalEmpty,
		"two distinct loader objects and canonical empty pocket are supported");
	CHECK(swapAtZeroCost(SMALLPOCK1POS, SMALLPOCK2POS) &&
		CaptureInventorySwapObjectState(id, SMALLPOCK1POS) == originalEmpty &&
		CaptureInventorySwapObjectState(id, SMALLPOCK2POS) == originalAmmunition &&
		emptyVestPocket.ubNumberOfObjects == 2 && emptyVestPocket[0]->data.ubShotsLeft == 6 &&
		emptyVestPocket[1]->data.ubShotsLeft == 3,
		"vest move preserves the entire two-loader stack and each object's distinct round count");
	CHECK(swapAtZeroCost(SMALLPOCK2POS, SMALLPOCK1POS) &&
		CaptureInventorySwapObjectState(id, SMALLPOCK1POS) == originalAmmunition &&
		CaptureInventorySwapObjectState(id, SMALLPOCK2POS) == originalEmpty &&
		CaptureInventorySwapObjectState(id, HANDPOS) == originalGun &&
		CaptureInventorySwapObjectState(id, SECONDHANDPOS) == originalKit &&
		ammunition.ubNumberOfObjects == 2 && ammunition.objectStack.size() == 2 &&
		ammunition[0]->data.ubShotsLeft == 6 && ammunition[1]->data.ubShotsLeft == 3 &&
		ammunition[0]->attachments.size() == 1 && ammunition[1]->attachments.size() == 2 &&
		actor.inventory()[VESTPOCKPOS][0]->data.lbe.bLBE == 0,
		"vest roundtrip restores both object fingerprints, preserves hands and creates no LBE graph");

	// Owner metrics describe all ordinary stack members, independently of which
	// object appears first. No native items or injuries are injected by the
	// installed probe; these edge cases belong only to this data-free fixture.
	using Status = CoopSession::CoopInventoryStatusKind;
	using Support = CoopSession::CoopInventorySlotSupport;
	Ja2CoopInventoryAuthority metricsAuthority;
	MakeObject(emptyVestPocket, FIRSTAIDKIT, 3, 0);
	emptyVestPocket[0]->data.objectStatus = 73;
	emptyVestPocket[1]->data.objectStatus = 40;
	CoopSession::CoopOwnerInventorySnapshot metrics;
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.inventoryRevision == 1 &&
		metrics.slots[HANDPOS].statusKind == Status::Condition &&
		metrics.slots[HANDPOS].firstCondition == 94 && metrics.slots[HANDPOS].resourceTotal == 0 &&
		metrics.slots[SECONDHANDPOS].statusKind == Status::ToolKitPoints &&
		metrics.slots[SECONDHANDPOS].resourceTotal == 99 &&
		metrics.slots[SMALLPOCK1POS].statusKind == Status::AmmoRounds &&
		metrics.slots[SMALLPOCK1POS].resourceTotal == 9 &&
		metrics.slots[SMALLPOCK2POS].statusKind == Status::MedicalKitPoints &&
		metrics.slots[SMALLPOCK2POS].resourceTotal == 113 &&
		metrics.slots[VESTPOCKPOS].support == Support::UnsupportedComplex &&
		metrics.slots[VESTPOCKPOS].statusKind == Status::Unknown &&
		metrics.slots[VESTPOCKPOS].resourceTotal == 0,
		"owner metrics distinguish gun condition, tool points, total reserve rounds and total medical points without inspecting LBE");
	ammunition[1]->data.ubShotsLeft = 2;
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.inventoryRevision == 2 && metrics.slots[SMALLPOCK1POS].firstCondition == 6 &&
		metrics.slots[SMALLPOCK1POS].resourceTotal == 8,
		"non-first ammunition use updates both exact aggregate and private revision");
	emptyVestPocket[2]->data.objectStatus = 1;
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.inventoryRevision == 3 && metrics.slots[SMALLPOCK2POS].firstCondition == 73 &&
		metrics.slots[SMALLPOCK2POS].resourceTotal == 114,
		"non-first kit change is visible without changing first kit points or object count");
	emptyVestPocket[0]->data.objectStatus = 0;
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.slots[SMALLPOCK2POS].firstCondition == 0 && metrics.slots[SMALLPOCK2POS].resourceTotal == 41,
		"a depleted first kit remains visibly distinct from remaining points in the rest of its stack");
	ammunition[0]->data.ubShotsLeft = 40000;
	ammunition[1]->data.ubShotsLeft = 65535;
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.slots[SMALLPOCK1POS].firstCondition < 0 &&
		static_cast<UINT16>(metrics.slots[SMALLPOCK1POS].firstCondition) == 40000 &&
		metrics.slots[SMALLPOCK1POS].resourceTotal == 105535,
		"unsigned native ammunition quantities do not turn into negative resource totals");
	MakeObject(ammunition[0]->attachments.front(), COMBAT_KNIFE, 1, 80);
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.slots[SMALLPOCK1POS].support == Support::UnsupportedComplex &&
		metrics.slots[SMALLPOCK1POS].statusKind == Status::Unknown &&
		metrics.slots[SMALLPOCK1POS].resourceTotal == 0,
		"actual attachment graphs suppress formerly known metrics rather than advertising partial totals");
	MakeObject(ammunition, CLIP38_6, 255, 0);
	for (auto& object : ammunition.objectStack) object.data.ubShotsLeft = 65535;
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.slots[SMALLPOCK1POS].statusKind == Status::AmmoRounds &&
		metrics.slots[SMALLPOCK1POS].resourceTotal == 16711425 &&
		metrics.slots[SMALLPOCK1POS].count == 255,
		"largest representable native stack is summed exactly without narrowing or truncation");
	const auto revisionBeforeSemanticChange = metrics.inventoryRevision;
	Item[LOCKSMITHKIT].usItemClass = IC_MEDKIT;
	CHECK(metricsAuthority.capture(id, worldIdentity.worldGeneration, metrics) &&
		metrics.inventoryRevision == revisionBeforeSemanticChange + 1 &&
		metrics.slots[SECONDHANDPOS].statusKind == Status::MedicalKitPoints &&
		metrics.slots[SECONDHANDPOS].resourceTotal == 99,
		"authority-selected semantic changes advance revision even with identical native object bytes");
	Item[LOCKSMITHKIT].usItemClass = IC_KIT;

	// Ordinary held weapons also contribute native worn camouflage. The item
	// object itself is unchanged by moving it between hands and pockets; these
	// derived values must nevertheless follow the committed equipment layout.
	// Keep the totals below the palette-substitution threshold so the fixture
	// uses only this real in-memory standing palette, never installed files.
	SGPPaletteEntry equipmentPalette[256]{};
	equipmentPalette[17].peRed = 87;
	equipmentPalette[17].peGreen = 43;
	equipmentPalette[17].peBlue = 19;
	video.pPaletteEntry = equipmentPalette;
	gubAnimSurfaceIndex[REGMALE][STANDING] = 0;
	gubAnimSurfaceItemSubIndex[REGMALE][STANDING] = INVALID_ANIMATION;
	gGameExternalOptions.bCamoKitArea = 90;
	Item[SW38].camobonus = 5;
	Item[SW38].urbanCamobonus = 7;
	Item[SW38].desertCamobonus = 9;
	Item[SW38].snowCamobonus = 11; // Native worn bonus cap is 100 - kit area.
	actor.camouflage().jungleApplied() = 1;
	actor.camouflage().urbanApplied() = 2;
	actor.camouflage().desertApplied() = 3;
	actor.camouflage().snowApplied() = 4;
	actor.actionPoints().current() = 100;
	const auto wornCamoIs = [&](INT8 jungle, INT8 urban, INT8 desert, INT8 snow) {
		return actor.camouflage().jungleWorn() == jungle &&
			actor.camouflage().urbanWorn() == urban &&
			actor.camouflage().desertWorn() == desert &&
			actor.camouflage().snowWorn() == snow;
	};
	const auto appliedCamoUnchanged = [&] {
		return actor.camouflage().jungleApplied() == 1 &&
			actor.camouflage().urbanApplied() == 2 &&
			actor.camouflage().desertApplied() == 3 &&
			actor.camouflage().snowApplied() == 4;
	};
	CHECK(ApplyEquipmentBonusesChecked(actor) && wornCamoIs(5, 7, 9, 10) &&
		actor.palette().base8() && actor.palette().base16() &&
		actor.palette().base8()[17].peRed == 87,
		"checked native bonus refresh applies all four held-weapon bonuses and cap with a real actor palette");
	const auto swapWithBonuses = [&](UINT8 source, UINT8 target, bool held) {
		SwapInventorySlotsCommand prepared;
		const auto sourceObject = CaptureInventorySwapObjectState(id, source);
		const auto targetObject = CaptureInventorySwapObjectState(id, target);
		const INT16 previousAP = actor.actionPoints().current();
		const auto previousRecords = game.commandJournal().size();
		const bool ready = PrepareSwapInventorySlotsCommand(id, source, target, prepared);
		CHECK(ready && sourceObject != 0 && targetObject != 0,
			"ordinary camouflaged gun hand/pocket swap prepares without native object mutation");
		if (!ready) return false;
		const auto result = dispatch(prepared);
		CHECK(result.status == SimulationCommandDispatchStatus::Applied && game.commands().empty() &&
			game.commandJournal().size() == previousRecords + 1 &&
			actor.actionPoints().current() == previousAP - prepared.expectedActionPointCost &&
			CaptureInventorySwapObjectState(id, source) == targetObject &&
			CaptureInventorySwapObjectState(id, target) == sourceObject &&
			(held ? wornCamoIs(5, 7, 9, 10) : wornCamoIs(0, 0, 0, 0)) &&
			appliedCamoUnchanged() && IsJa2TacticalWorldIntegrityValid(),
			"one retained swap updates native worn bonuses, preserves applied camouflage and exact objects, and charges AP once");
		return result.status == SimulationCommandDispatchStatus::Applied;
	};
	CHECK(!pocket.exists() && swapWithBonuses(HANDPOS, BIGPOCK1POS, false) &&
		swapWithBonuses(BIGPOCK1POS, HANDPOS, true) &&
		CaptureInventorySwapObjectState(id, HANDPOS) == originalGun,
		"main-hand gun roundtrip removes and restores all worn camouflage without losing gun metadata");
	CHECK(swapWithBonuses(HANDPOS, SECONDHANDPOS, true) &&
		swapWithBonuses(SECONDHANDPOS, BIGPOCK1POS, false) &&
		swapWithBonuses(BIGPOCK1POS, SECONDHANDPOS, true) &&
		swapWithBonuses(SECONDHANDPOS, HANDPOS, true) &&
		CaptureInventorySwapObjectState(id, HANDPOS) == originalGun &&
		CaptureInventorySwapObjectState(id, SECONDHANDPOS) == originalKit,
		"offhand gun roundtrip also follows native held-weapon rules and restores both hands");
	{
		OBJECTTYPE retainedPocket2;
		std::swap(actor.inventory()[BIGPOCK2POS], retainedPocket2);
		MakeObject(actor.inventory()[BIGPOCK2POS], STEEL_HELMET, 1, 95);
		const auto helmetObject = CaptureInventorySwapObjectState(id, BIGPOCK2POS);
		Item[STEEL_HELMET].camobonus = 2;
		SwapInventorySlotsCommand helmetCommand;
		CHECK(PrepareSwapInventorySlotsCommand(id, BIGPOCK2POS, HELMETPOS, helmetCommand) &&
			dispatch(helmetCommand).status == SimulationCommandDispatchStatus::Applied &&
			CaptureInventorySwapObjectState(id, HELMETPOS) == helmetObject &&
			wornCamoIs(7, 7, 9, 10) && appliedCamoUnchanged() && actor.palette().base8() && actor.palette().base16(),
			"equipping ordinary armour refreshes native worn bonuses through the checked real palette continuation");
		CHECK(PrepareSwapInventorySlotsCommand(id, HELMETPOS, BIGPOCK2POS, helmetCommand) &&
			dispatch(helmetCommand).status == SimulationCommandDispatchStatus::Applied &&
			CaptureInventorySwapObjectState(id, BIGPOCK2POS) == helmetObject &&
			wornCamoIs(5, 7, 9, 10) && appliedCamoUnchanged(),
			"unequipping armour restores original worn bonuses without consuming applied camouflage");
		Item[STEEL_HELMET].camobonus = 0;
		std::swap(actor.inventory()[BIGPOCK2POS], retainedPocket2);
	}
	// Preserve the old-inventory rule: held guns do not add these bonuses there.
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	ApplyEquipmentBonuses(&actor);
	CHECK(wornCamoIs(0, 0, 0, 0) && appliedCamoUnchanged(),
		"legacy equipment wrapper preserves old-inventory camouflage rules");
	const auto* legacyPalette = actor.palette().base8();
	ApplyEquipmentBonuses(nullptr);
	ApplyEquipmentBonuses(&actor);
	CHECK(actor.palette().base8() == legacyPalette && wornCamoIs(0, 0, 0, 0),
		"legacy null and unchanged-bonus refreshes remain harmless and do not rebuild the palette");
	gGameOptions.ubInventorySystem = INVENTORY_NEW;
	CHECK(ApplyEquipmentBonusesChecked(actor) && wornCamoIs(5, 7, 9, 10),
		"new-inventory held bonuses are restored before the genuine palette-failure regression");

	// The animation remains valid and loaded but its palette is unavailable.
	// A native swap really commits before the checked palette rebuild fails;
	// that parent must poison the world, not publish success or replay mutation.
	// Applied jungle camo selects the substitution path, whose native preflight
	// requires this loaded surface's palette before any COL-file lookup. This
	// avoids the legitimate non-substitution fallback to a blank base palette.
	actor.camouflage().jungleApplied() = 50;
	SwapInventorySlotsCommand missingPaletteCommand;
	CHECK(PrepareSwapInventorySlotsCommand(id, HANDPOS, BIGPOCK1POS, missingPaletteCommand),
		"prepare the loaded-animation missing-equipment-palette continuation");
	const auto* retainedPalette = actor.palette().base8();
	const auto* retainedPalette16 = actor.palette().base16();
	const INT16 paletteFailureAP = actor.actionPoints().current();
	video.pPaletteEntry = nullptr;
	const auto failedPaletteSwap = dispatch(missingPaletteCommand);
	CHECK(failedPaletteSwap.status == SimulationCommandDispatchStatus::RetryDeferred &&
		game.commands().size() == 1 && !IsJa2TacticalWorldIntegrityValid() &&
		!hand.exists() && CaptureInventorySwapObjectState(id, BIGPOCK1POS) == originalGun &&
		actor.animationPlayback().surface() == 0 && wornCamoIs(0, 0, 0, 0) &&
		actor.palette().base8() == retainedPalette && actor.palette().base16() == retainedPalette16 &&
		actor.actionPoints().current() == paletteFailureAP,
		"failed palette continuation after a real equipment swap preserves the old palette, poisons integrity and never reports Applied");
	video.pPaletteEntry = equipmentPalette;
	BeginSimulationCommandFrameBudget(++frame, 8);
	const auto paletteRetry = ExecuteSimulationCommandsThrough(game.runtime().simulationTicks().completedTickSequence(), 8);
	bool paletteFailureDiscarded = false;
	for (const auto& record : game.commandJournal().snapshot())
		if (record.sequence == failedPaletteSwap.sequence)
		{
			CHECK(record.status != CommandJournalStatus::Applied,
				"failed equipment-bonus parent has no Applied journal entry");
			paletteFailureDiscarded |= record.status == CommandJournalStatus::Discarded;
		}
	CHECK(paletteRetry.discarded == 1 && paletteRetry.applied == 0 && game.commands().empty() &&
		paletteFailureDiscarded && !hand.exists() &&
		CaptureInventorySwapObjectState(id, BIGPOCK1POS) == originalGun &&
		actor.actionPoints().current() == paletteFailureAP &&
		actor.camouflage().jungleApplied() == 50 && actor.camouflage().urbanApplied() == 2 &&
		actor.camouflage().desertApplied() == 3 && actor.camouflage().snowApplied() == 4,
		"invalid-integrity palette retry discards without swapping items back or charging AP");
	cleanup();
	std::printf("authoritative inventory swap: %s\n", failures == 0 ? "PASS" : "FAIL");
	return failures == 0 ? 0 : 1;
}
