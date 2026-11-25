// Real pool insertion/removal, retained native pickup and private ledger. All
// fixture objects/map/animation metadata live only in this isolated process.
#include "Animation Cache.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "CoopInventoryAuthority.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Handle Items.h"
#include "Interface Items.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "random.h"
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
#include "TacticalWorldItemHost.h"
#include "World Items.h"
#include "World Tile Map.h"
#include "connect.h"
#include "worlddef.h"

#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Core/SimulationRandom.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

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
unsigned removalFailures = 0;
#define CHECK(condition, message) do { if (!(condition)) { \
	std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, message); ++failures; } } while (false)

void MakeObject(OBJECTTYPE& object, UINT16 item, UINT8 quantity, INT16 condition)
{
	object.initialize();
	object.usItem = item;
	object.ubNumberOfObjects = quantity;
	object.objectStack.resize(quantity);
	for (auto& stack : object.objectStack) stack.data.objectStatus = condition;
}

void FailNativeLocatorRemoval()
{
	++removalFailures;
	throw std::bad_alloc();
}
}

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	const bool removalFailure = argc == 2 && std::strcmp(argv[1], "--removal-failure") == 0;
	CHECK(InstallGameSimulationRandom(0x67726f756e646c6fULL) == GameSimulationRandomInstallError::None,
		"ground fixture installs the authoritative random stream before observing it");
	SimulationRandom* const simulationRandom = GetGameSimulationRandomSource();
	if (!simulationRandom) return 1;
	GameContext& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) &&
		game.markRunning(), "native runtime starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	ResetJa2TacticalActorRosters();
	ResetJa2TacticalInterruptForNewWorld();
	ResetJa2TacticalWorldItemDirectory();
	CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "logical world exists");
	InitRenderParams(0);
	NotifyJa2TacticalWorldLoaded(1);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM;
	is_networked = is_client = is_server = false;
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	gGameOptions.fNewTraitSystem = FALSE;
	APBPConstants[AP_PICKUP_ITEM] = 4;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	TacticalActor& actor = *repository.resolve(0);
	TacticalActor& competitor = *repository.resolve(1);
	const INT32 origin = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS / 2;
	for (TacticalActor* merc : {&actor, &competitor})
	{
		const UINT16 slot = merc == &actor ? 0 : 1;
		merc->identity().id() = SoldierID{slot};
		merc->identity().incarnation() = 101 + slot;
		merc->identity().profile() = NO_PROFILE;
		merc->identity().bodyType() = REGMALE;
		merc->roster().active() = merc->roster().inSector() = TRUE;
		merc->roster().team() = OUR_TEAM;
		merc->assignment().current() = FIRST_SQUAD;
		merc->position().gridNo() = origin;
		merc->position().level() = FIRST_LEVEL;
		merc->position().direction() = SOUTH;
		merc->pathing().desiredDirection() = SOUTH;
		merc->animationPlayback().state() = CROUCHING;
		merc->animationPlayback().surface() = 0;
		merc->animationIntent().clearPendingAnimations();
		merc->pendingAction().clearAction();
		merc->vitals().health() = merc->vitals().maximumHealth() = 100;
		merc->vitals().breath() = merc->vitals().maximumBreath() = 100;
		merc->actionPoints().current() = 20;
		merc->fireControl().selectBarrelMode(1);
		CHECK(AdoptJa2TacticalEntity(*merc), "exact actor identity adopted");
	}
	const auto actorId = GetJa2TacticalEntityId(actor);
	const auto competitorId = GetJa2TacticalEntityId(competitor);
	gMAXITEMS_READ = DEFAULT_BPACK + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
	Item[FIRSTAIDKIT].ubPerPocket = 4;
	Item[COMBAT_KNIFE].usItemClass = IC_BLADE;
	Item[COMBAT_KNIFE].ubPerPocket = 4;
	Item[KEY_1].usItemClass = IC_KEY;
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
	const auto insertAt = [&](INT32 grid, UINT16 kind, UINT8 quantity, INT16 condition) {
		OBJECTTYPE object;
		MakeObject(object, kind, quantity, condition);
		INT32 slot = -1;
		CHECK(AddItemToPoolAndGetIndex(grid, &object, VISIBLE, FIRST_LEVEL,
			WORLD_ITEM_REACHABLE, 0, NOBODY, &slot) != nullptr && slot >= 0,
			"native pool insertion creates item and renderer links");
		return slot >= 0 ? GetJa2TacticalWorldItemId(static_cast<UINT32>(slot)) : TacticalWorldItemId{};
	};
	const auto insert = [&](UINT16 kind, UINT8 quantity, INT16 condition) {
		return insertAt(origin, kind, quantity, condition);
	};
	const auto cleanup = [&] {
		for (std::size_t slot = 0; slot < gWorldItems.size(); ++slot)
			if (gWorldItems[slot].fExists)
				(void)RemoveItemFromPool(gWorldItems[slot].sGridNo, static_cast<INT32>(slot), gWorldItems[slot].ubLevel);
		for (auto* merc : {&actor, &competitor}) { merc->palette().reset(); merc->animationCache().reset(); }
		gAnimSurfaceDatabase[0] = retainedSurface;
		ReleaseWorldTileMap();
	};
	const TacticalWorldItemId itemId = insert(FIRSTAIDKIT, 2, 73);
	if (!itemId.valid()) { cleanup(); return 1; }
	const auto worldObject = [&]() -> OBJECTTYPE& { return ResolveJa2TacticalWorldItem(itemId)->object; };
	worldObject().ubMission = 7;
	worldObject().fFlags = OBJECT_MODIFIED;
	worldObject()[0]->data.sRepairThreshold = 79;
	worldObject()[0]->data.sObjectFlag = 0x1020304050607080ULL;
	worldObject()[0]->data.bTemperature = 12.5f;
	worldObject()[0]->attachments.resize(4);
	const auto* originalNode = &worldObject().objectStack.front();
	OBJECTTYPE& destination = actor.inventory()[BIGPOCK2POS];
	std::uint64_t frame = 0;
	const auto dispatch = [&](const AuthoritativePickupWorldItemCommand& command) {
		BeginSimulationCommandFrameBudget(++frame, 1);
		return TryDispatchSimulationCommandNow(SimulationCommand{command});
	};
	AuthoritativePickupWorldItemCommand command;
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, command),
		"server prepares exact stable item, whole object, pool, empty slot, actor and native AP proofs");
	if (!command.expectedWorldGeneration) { cleanup(); return 1; }
	CHECK(command.expectedActionPointCost == 4, "same-tile pickup uses native AP_PICKUP_ITEM");
	const auto objectFingerprint = command.sourceStateFingerprint;
	TacticalCoopGroundItemCandidate candidate;
	CHECK(CaptureCoopPickupGroundItem(actorId, itemId, candidate) && candidate.item == itemId &&
		candidate.grid == origin && candidate.level == FIRST_LEVEL && candidate.objectStateFingerprint == objectFingerprint,
		"projection captures exact authority identity and ordinary object state");
	if (removalFailure)
	{
		ITEM_POOL* pool = nullptr;
		CHECK(GetItemPool(origin, &pool, FIRST_LEVEL) && pool, "real item pool exists for callback fault");
		SetItemPoolLocatorWithCallback(pool, FailNativeLocatorRemoval);
		const auto result = dispatch(command);
		CHECK(removalFailures == 1 && result.status == SimulationCommandDispatchStatus::RetryDeferred &&
			!IsJa2TacticalWorldIntegrityValid() && game.commands().size() == 1 &&
			CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == objectFingerprint &&
			&destination.objectStack.front() == originalNode && actor.actionPoints().current() == 20,
			"native removal callback failure after object move poisons world without Applied or AP charge");
		BeginSimulationCommandFrameBudget(++frame, 8);
		const auto retry = ExecuteSimulationCommandsThrough(game.runtime().simulationTicks().completedTickSequence(), 8);
		CHECK(retry.discarded == 1 && retry.applied == 0 && game.commands().empty() &&
			CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == objectFingerprint && actor.actionPoints().current() == 20,
			"failed parent cannot replay the mutation or charge AP");
		for (const auto& record : game.commandJournal().snapshot())
			if (record.sequence == result.sequence)
				CHECK(record.status != CommandJournalStatus::Applied, "failed pickup never journals Applied");
		cleanup();
		return failures ? 1 : 0;
	}

	ITEM_POOL* discoveryPool = nullptr;
	CHECK(GetItemPool(origin, &discoveryPool, FIRST_LEVEL) && discoveryPool, "discovery fixture has real pool links");
	if (!discoveryPool) { cleanup(); return 1; }
	const auto inspectNearby = [&](TacticalEntityId requestedActor, TacticalWorldItemId requestedItem,
		bool expected, const char* reason) {
		const auto beforeAP = actor.actionPoints().current();
		const auto beforeCompetitorAP = competitor.actionPoints().current();
		const auto beforeActorGrid = actor.position().gridNo();
		const auto beforeActorLevel = actor.position().level();
		const auto beforeWorldFlags = GetMapElement(origin).uiFlags;
		const auto beforeRenderFlags = GetRenderFlags();
		const auto beforePoolFlags = discoveryPool->usFlags;
		const auto beforePoolVisibility = discoveryPool->bVisible;
		const auto beforeItemFlags = gWorldItems[itemId.slot].usFlags;
		const auto beforeItemVisibility = gWorldItems[itemId.slot].bVisible;
		const auto beforeItemMirror = gWorldItems[itemId.slot].uiUniqueWorldItemIdValue;
		const auto beforeDirectorySequence = GetJa2TacticalWorldItemDirectory().nextIncarnation();
		const auto beforeDirectoryCount = GetJa2TacticalWorldItemDirectory().activeCount();
		const auto beforeRandom = simulationRandom->checkpoint();
		const auto beforeRandomEpoch = simulationRandom->consumptionEpoch();
		const auto beforeTick = game.runtime().simulationTicks().completedTickSequence();
		const auto beforeCommands = game.commands().size();
		const auto* beforeNode = &worldObject().objectStack.front();
		TacticalCoopGroundItemCandidate nearby{TacticalWorldItemId{99, 999}, 123, 1, 42};
		const bool captured = CaptureCoopNearbyLootItem(requestedActor, requestedItem, nearby);
		CHECK(captured == expected, reason);
		if (captured)
		{
			const WORLDITEM* selected = ResolveJa2TacticalWorldItem(requestedItem);
			CHECK(selected && nearby.item == requestedItem && nearby.grid == selected->sGridNo &&
				nearby.level == selected->ubLevel && nearby.objectStateFingerprint == objectFingerprint,
				"nearby discovery preserves exact identity, location and whole-object proof");
		}
		else
			CHECK(nearby.item == (TacticalWorldItemId{99, 999}) && nearby.grid == 123 && nearby.level == 1 &&
				nearby.objectStateFingerprint == 42, "failed nearby discovery preserves every output field");
		CHECK(actor.actionPoints().current() == beforeAP && competitor.actionPoints().current() == beforeCompetitorAP &&
			actor.position().gridNo() == beforeActorGrid && actor.position().level() == beforeActorLevel &&
			GetMapElement(origin).uiFlags == beforeWorldFlags && GetRenderFlags() == beforeRenderFlags &&
			discoveryPool->usFlags == beforePoolFlags && discoveryPool->bVisible == beforePoolVisibility &&
			gWorldItems[itemId.slot].usFlags == beforeItemFlags && gWorldItems[itemId.slot].bVisible == beforeItemVisibility &&
			gWorldItems[itemId.slot].uiUniqueWorldItemIdValue == beforeItemMirror &&
			GetJa2TacticalWorldItemDirectory().nextIncarnation() == beforeDirectorySequence &&
			GetJa2TacticalWorldItemDirectory().activeCount() == beforeDirectoryCount &&
			simulationRandom->checkpoint() == beforeRandom && simulationRandom->consumptionEpoch() == beforeRandomEpoch &&
			game.runtime().simulationTicks().completedTickSequence() == beforeTick && game.commands().size() == beforeCommands &&
			&worldObject().objectStack.front() == beforeNode && !destination.exists() && gpItemPointer == nullptr,
			"nearby discovery never changes AP, pose, world/pool/reveal flags, identity, RNG, clocks, commands or object storage");
	};
	inspectNearby(actorId, itemId, true, "same-tile ordinary loot is also a nearby candidate");
	actor.position().gridNo() = origin + 1;
	inspectNearby(actorId, itemId, true, "adjacent discovered loot is available without moving the actor");
	CHECK(!CaptureCoopPickupGroundItem(actorId, itemId, candidate), "nearby discovery does not widen exact-tile pickup projection");
	AuthoritativePickupWorldItemCommand nearbyPickup;
	CHECK(!PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, nearbyPickup),
		"nearby discovery does not prepare an adjacent automatic pickup or walking continuation");
	actor.actionPoints().current() = 0;
	actor.animationActivity().nonInterruptible() = TRUE;
	inspectNearby(actorId, itemId, true, "busy zero-AP actor can see already-discovered nearby loot");
	actor.actionPoints().current() = 20;
	actor.animationActivity().nonInterruptible() = FALSE;
	for (const INT32 sign : {-1, 1})
	{
		actor.position().gridNo() = origin + sign * TacticalCoopNearbyLootRadius * (WORLD_COLS + 1);
		inspectNearby(actorId, itemId, true, "diagonal radius-12 edge is inside the bounded Chebyshev search");
		actor.position().gridNo() += sign;
		inspectNearby(actorId, itemId, false, "radius-13 column is outside nearby discovery");
		actor.position().gridNo() = origin + sign * (TacticalCoopNearbyLootRadius + 1) * WORLD_COLS;
		inspectNearby(actorId, itemId, false, "radius-13 row is outside nearby discovery");
	}
	const INT32 edgeGrid = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS - 1;
	const auto edgeItem = insertAt(edgeGrid, FIRSTAIDKIT, 1, 50);
	if (!edgeItem.valid()) { cleanup(); return 1; }
	actor.position().gridNo() = edgeGrid + 1;
	inspectNearby(actorId, edgeItem, false, "consecutive native grid numbers cannot wrap the map row into a nearby tile");
	CHECK(RemoveItemFromPool(edgeGrid, static_cast<INT32>(edgeItem.slot), FIRST_LEVEL), "row-edge fixture removes its real pool");
	actor.position().gridNo() = origin;
	actor.position().level() = SECOND_LEVEL;
	inspectNearby(actorId, itemId, false, "nearby discovery cannot cross floors");
	actor.position().level() = FIRST_LEVEL;
	actor.roster().active() = FALSE;
	inspectNearby(actorId, itemId, false, "inactive actor cannot discover nearby loot");
	actor.roster().active() = TRUE;
	actor.roster().inSector() = FALSE;
	inspectNearby(actorId, itemId, false, "out-of-sector actor cannot discover nearby loot");
	actor.roster().inSector() = TRUE;
	actor.roster().team() = ENEMY_TEAM;
	inspectNearby(actorId, itemId, false, "enemy actor cannot use owner loot projection");
	actor.roster().team() = OUR_TEAM;
	inspectNearby(TacticalEntityId{actorId.slot, actorId.incarnation + 1}, itemId, false, "stale actor identity rejects discovery");
	inspectNearby(actorId, TacticalWorldItemId{itemId.slot, itemId.incarnation + 1}, false, "stale item identity rejects discovery");
	for (const INT32 invalidGrid : {-1, WORLD_MAX})
	{
		actor.position().gridNo() = invalidGrid;
		inspectNearby(actorId, itemId, false, "invalid actor tile is rejected before native tile access");
	}
	actor.position().gridNo() = origin;
	for (const INT32 invalidGrid : {-1, WORLD_MAX})
	{
		gWorldItems[itemId.slot].sGridNo = invalidGrid;
		inspectNearby(actorId, itemId, false, "invalid item's own tile is rejected before native tile access");
	}
	gWorldItems[itemId.slot].sGridNo = origin + 1;
	inspectNearby(actorId, itemId, false, "item location without matching real pool membership cannot become a marker");
	gWorldItems[itemId.slot].sGridNo = origin;
	const auto retainedTacticalFlags = gTacticalStatus.uiFlags;
	gTacticalStatus.uiFlags |= SHOW_ALL_ITEMS;
	CHECK(discoveryPool->bVisible == VISIBLE, "native pool aggregate is visible for individual hidden-item checks");
	for (const INT8 hidden : {INT8(HIDDEN_ITEM), INT8(BURIED), INT8(HIDDEN_IN_OBJECT), INT8(INVISIBLE)})
	{
		gWorldItems[itemId.slot].bVisible = hidden;
		inspectNearby(actorId, itemId, false, "aggregate pool visibility and debug reveal never expose an undiscovered item");
	}
	gWorldItems[itemId.slot].bVisible = VISIBLE;
	gTacticalStatus.uiFlags = retainedTacticalFlags;
	gWorldItems[itemId.slot].usFlags |= WORLD_ITEM_DONTRENDER;
	inspectNearby(actorId, itemId, false, "world do-not-render flag excludes nearby markers");
	gWorldItems[itemId.slot].usFlags &= ~WORLD_ITEM_DONTRENDER;
	discoveryPool->usFlags |= WORLD_ITEM_DONTRENDER;
	inspectNearby(actorId, itemId, false, "native renderer pool-node do-not-render flag also excludes nearby markers");
	CHECK(CaptureCoopPickupGroundItem(actorId, itemId, candidate),
		"nearby-only pool render restriction leaves the existing exact-tile pickup predicate unchanged");
	discoveryPool->usFlags &= ~WORLD_ITEM_DONTRENDER;
	inspectNearby(actorId, itemId, true, "restoring fixture flags restores unchanged nearby proof");

	Ja2CoopInventoryAuthority authority;
	CoopSession::CoopOwnerInventorySnapshot ledger, next;
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, ledger) && ledger.groundGrid == origin &&
		ledger.groundLevel == FIRST_LEVEL && !ledger.groundItemsTruncated && ledger.groundItems.size() == 1 &&
		ledger.groundItems[0].id == itemId && ledger.groundItems[0].summary.item == FIRSTAIDKIT &&
		ledger.groundItems[0].summary.statusKind == CoopSession::CoopInventoryStatusKind::MedicalKitPoints &&
		ledger.groundItems[0].summary.resourceTotal == 146,
		"private ledger lists exact same-tile item and whole-stack medical supply points");
	CHECK(ledger.nearbyLoot == (std::vector<CoopSession::CoopNearbyLootMarker>{{origin, FIRST_LEVEL, true}}) &&
		!ledger.nearbyLootTruncated, "private nearby ledger marks the original usable medical pile once");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) && next.inventoryRevision == ledger.inventoryRevision,
		"unchanged ground/item capture retains private revision");
	worldObject()[0]->data.sObjectFlag ^= 1;
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) && next.inventoryRevision > ledger.inventoryRevision &&
		next.groundItems == ledger.groundItems, "private ground fingerprint catches same-display metadata changes");
	worldObject()[0]->data.sObjectFlag ^= 1;
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, ledger), "ledger observes restored metadata");
	const auto singleGround = ledger.groundItems;
	std::vector<TacticalWorldItemId> extraItems;
	extraItems.reserve(65);
	for (unsigned index = 0; index < 65; ++index)
		extraItems.push_back(insert(FIRSTAIDKIT, 1, 40 + index % 30));
	CoopSession::CoopOwnerInventorySnapshot crowded, crowdedAgain;
	const auto readOnlySequence = GetJa2TacticalWorldItemDirectory().nextIncarnation();
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, crowded) && crowded.groundItemsTruncated &&
		crowded.groundItems.size() == CoopSession::MaximumCoopGroundItems && crowded.groundGrid == origin &&
		crowded.groundLevel == FIRST_LEVEL, "66 real native ground items yield a bounded 64-entry truncated projection");
	std::vector<TacticalWorldItemId> expectedIds = extraItems;
	expectedIds.push_back(itemId);
	std::sort(expectedIds.begin(), expectedIds.end());
	for (std::size_t index = 0; index < crowded.groundItems.size(); ++index)
		CHECK(crowded.groundItems[index].id == expectedIds[index] && crowded.groundItems[index].summary.slot == index &&
			ResolveJa2TacticalWorldItem(crowded.groundItems[index].id) != nullptr,
			"bounded ground list uses sorted exact native IDs and dense private indices");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, crowdedAgain) && crowdedAgain == crowded &&
		GetJa2TacticalWorldItemDirectory().nextIncarnation() == readOnlySequence,
		"separate unchanged crowded capture retains revision and never issues an item identity");
	const auto tailOwnership = insert(OWNERSHIP, 1, 100);
	if (!tailOwnership.valid()) { cleanup(); return 1; }
	ResolveJa2TacticalWorldItem(tailOwnership)->bVisible = HIDDEN_ITEM;
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, crowdedAgain) &&
		crowdedAgain.inventoryRevision > crowded.inventoryRevision && crowdedAgain.groundItems.empty() &&
		!crowdedAgain.groundItemsTruncated && crowdedAgain.groundGrid == origin && crowdedAgain.nearbyLoot.empty(),
		"hidden ownership marker after 66 real items excludes the entire pool without a partial truncated view");
	CHECK(RemoveItemFromPool(origin, static_cast<INT32>(tailOwnership.slot), FIRST_LEVEL),
		"native cleanup removes large-pool ownership marker");
	for (const auto extra : extraItems)
		CHECK(RemoveItemFromPool(origin, static_cast<INT32>(extra.slot), FIRST_LEVEL),
			"native cleanup retires each extra ground item");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, ledger) && ledger.groundItems == singleGround &&
		!ledger.groundItemsTruncated, "large-pool fixture cleanup restores the exact original visible stack");
	{
		using CoopSession::CoopNearbyLootMarker;
		const auto hasMarker = [](const CoopSession::CoopOwnerInventorySnapshot& snapshot, INT32 grid, bool medical) {
			return std::find(snapshot.nearbyLoot.begin(), snapshot.nearbyLoot.end(),
				CoopNearbyLootMarker{grid, FIRST_LEVEL, medical}) != snapshot.nearbyLoot.end();
		};
		CoopSession::CoopOwnerInventorySnapshot markers = ledger, changed, repeated;
		const INT32 nearbyGrid = origin + 2;
		const auto emptyKit = insertAt(nearbyGrid, FIRSTAIDKIT, 1, 0);
		const auto knife = insertAt(nearbyGrid, COMBAT_KNIFE, 1, 80);
		if (!emptyKit.valid() || !knife.valid()) { cleanup(); return 1; }
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.inventoryRevision > markers.inventoryRevision && changed.groundItems == singleGround &&
			changed.nearbyLoot.size() == 2 && hasMarker(changed, nearbyGrid, false),
			"actual nearby multi-item tile has one marker, no pickup token, and no medical flag for a zero-point kit");
		markers = changed;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, repeated) && repeated == markers,
			"independent unchanged nearby marker capture retains the entire private snapshot and revision");
		ResolveJa2TacticalWorldItem(emptyKit)->object[0]->data.objectStatus = 1;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.inventoryRevision > markers.inventoryRevision && changed.groundItems == singleGround &&
			hasMarker(changed, nearbyGrid, true), "positive medical points change the nearby flag and advance private revision");
		markers = changed;
		ResolveJa2TacticalWorldItem(emptyKit)->object[0]->data.objectStatus = 2;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, repeated) && repeated == markers,
			"marker-only projection does not leak or revise unexposed nearby item quantity details");
		ResolveJa2TacticalWorldItem(emptyKit)->bVisible = HIDDEN_ITEM;
		const auto retainedFlags = gTacticalStatus.uiFlags;
		gTacticalStatus.uiFlags |= SHOW_ALL_ITEMS;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.inventoryRevision > markers.inventoryRevision && hasMarker(changed, nearbyGrid, false),
			"visible knife cannot reveal a hidden kit through aggregate pool visibility or debug reveal");
		gTacticalStatus.uiFlags = retainedFlags;
		ResolveJa2TacticalWorldItem(emptyKit)->bVisible = VISIBLE;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, markers) && hasMarker(markers, nearbyGrid, true),
			"restored native discovery restores the nearby medical marker");
		const auto nearbyOwner = insertAt(nearbyGrid, OWNERSHIP, 1, 100);
		if (!nearbyOwner.valid()) { cleanup(); return 1; }
		ResolveJa2TacticalWorldItem(nearbyOwner)->bVisible = HIDDEN_ITEM;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.inventoryRevision > markers.inventoryRevision && changed.nearbyLoot.size() == 1 &&
			hasMarker(changed, origin, true) && changed.groundItems == singleGround,
			"hidden ownership excludes the complete nearby multi-item tile without affecting exact-tile loot");
		markers = changed;
		CHECK(RemoveItemFromPool(nearbyGrid, static_cast<INT32>(nearbyOwner.slot), FIRST_LEVEL), "nearby ownership cleanup");
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.inventoryRevision > markers.inventoryRevision && hasMarker(changed, nearbyGrid, true),
			"native ownership removal restores a nearby marker and advances revision");
		markers = changed;
		CHECK(RemoveItemFromPool(nearbyGrid, static_cast<INT32>(emptyKit.slot), FIRST_LEVEL) &&
			RemoveItemFromPool(nearbyGrid, static_cast<INT32>(knife.slot), FIRST_LEVEL), "nearby fixture items removed natively");
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.inventoryRevision > markers.inventoryRevision && changed.nearbyLoot.size() == 1,
			"removing the last nearby loot object removes its marker and advances revision");
		for (auto& stack : worldObject().objectStack) stack.data.objectStatus = 0;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.nearbyLoot.size() == 1 && hasMarker(changed, origin, false) &&
			changed.groundItems.size() == 1 && changed.groundItems[0].summary.resourceTotal == 0,
			"an ordinary zero-point medkit is still loot but cannot advertise usable medical supplies");
		for (auto& stack : worldObject().objectStack) stack.data.objectStatus = 73;
		const INT32 radiusGrid = origin + TacticalCoopNearbyLootRadius * (WORLD_COLS + 1);
		const auto edge = insertAt(radiusGrid, COMBAT_KNIFE, 1, 80);
		const auto beyond = insertAt(radiusGrid + 1, COMBAT_KNIFE, 1, 80);
		if (!edge.valid() || !beyond.valid()) { cleanup(); return 1; }
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) && changed.nearbyLoot.size() == 2 &&
			hasMarker(changed, radiusGrid, false) && !hasMarker(changed, radiusGrid + 1, false) &&
			changed.groundItems == singleGround, "private marker search includes radius 12 but excludes radius 13");
		actor.position().level() = SECOND_LEVEL;
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, changed) &&
			changed.nearbyLoot.empty() && changed.groundItems.empty() && changed.groundLevel == SECOND_LEVEL,
			"owner inventory never exposes nearby ground-floor loot on the actor's other floor");
		actor.position().level() = FIRST_LEVEL;
		CHECK(RemoveItemFromPool(radiusGrid, static_cast<INT32>(edge.slot), FIRST_LEVEL) &&
			RemoveItemFromPool(radiusGrid + 1, static_cast<INT32>(beyond.slot), FIRST_LEVEL), "radius fixture cleanup");

		std::vector<TacticalWorldItemId> pileItems;
		std::vector<INT32> nearestGrids{origin};
		for (INT32 row = 6; row >= -6; --row)
			for (INT32 column = 6; column >= -6; --column)
			{
				if (row == 0 && column == 0) continue;
				const INT32 grid = origin + row * WORLD_COLS + column;
				pileItems.push_back(insertAt(grid, COMBAT_KNIFE, 1, 80));
				nearestGrids.push_back(grid);
			}
		const auto distanceSquared = [&](INT32 grid) {
			const auto row = grid / WORLD_COLS - origin / WORLD_COLS;
			const auto column = grid % WORLD_COLS - origin % WORLD_COLS;
			return row * row + column * column;
		};
		std::sort(nearestGrids.begin(), nearestGrids.end(), [&](INT32 left, INT32 right) {
			return distanceSquared(left) != distanceSquared(right) ? distanceSquared(left) < distanceSquared(right) : left < right;
		});
		nearestGrids.resize(CoopSession::MaximumCoopNearbyLootMarkers);
		std::sort(nearestGrids.begin(), nearestGrids.end());
		const auto beforeMarkerSequence = GetJa2TacticalWorldItemDirectory().nextIncarnation();
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, markers) && markers.nearbyLootTruncated &&
			markers.nearbyLoot.size() == CoopSession::MaximumCoopNearbyLootMarkers && markers.groundItems == singleGround,
			"169 distinct native loot tiles produce the nearest 128 markers without widening exact pickup inventory");
		for (std::size_t index = 0; index < markers.nearbyLoot.size(); ++index)
			CHECK(markers.nearbyLoot[index].grid == nearestGrids[index] && markers.nearbyLoot[index].level == FIRST_LEVEL &&
				markers.nearbyLoot[index].hasMedicalKit == (nearestGrids[index] == origin),
				"capped markers select nearest tiles with stable tie-breaking then serialize canonical grid order");
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, repeated) && repeated == markers &&
			GetJa2TacticalWorldItemDirectory().nextIncarnation() == beforeMarkerSequence,
			"repeated capped marker capture is identical and never repairs or issues item identities");
		const auto duplicate = insertAt(origin + 1, COMBAT_KNIFE, 1, 70);
		if (!duplicate.valid()) { cleanup(); return 1; }
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, repeated) && repeated == markers,
			"additional nonmedical objects on a marked nearby tile neither duplicate markers nor change private revision");
		CHECK(RemoveItemFromPool(origin + 1, static_cast<INT32>(duplicate.slot), FIRST_LEVEL), "duplicate marker item cleanup");
		for (const auto id : pileItems)
		{
			const WORLDITEM* item = ResolveJa2TacticalWorldItem(id);
			CHECK(item && RemoveItemFromPool(item->sGridNo, static_cast<INT32>(id.slot), FIRST_LEVEL), "bounded marker pile cleanup");
		}
		CHECK(authority.capture(actorId, command.expectedWorldGeneration, ledger) && ledger.groundItems == singleGround &&
			ledger.nearbyLoot == (std::vector<CoopNearbyLootMarker>{{origin, FIRST_LEVEL, true}}) &&
			!ledger.nearbyLootTruncated, "all discovery fixtures restore the original exact native ground stack and marker");
	}
	// Growing native gWorldItems can copy its storage during fixture setup.
	// Retain the post-setup node; the actual authority transfer must not copy it.
	originalNode = &worldObject().objectStack.front();
	const auto preservedLedger = ledger;
	next = ledger;
	const auto noRepairSequence = GetJa2TacticalWorldItemDirectory().nextIncarnation();
	const auto noRepairActive = GetJa2TacticalWorldItemDirectory().activeCount();
	gWorldItems[itemId.slot].uiUniqueWorldItemIdValue = 0;
	TacticalCoopGroundItemCandidate preservedCandidate{TacticalWorldItemId{99, 999}, 123, 1, 42};
	CHECK(!CaptureCoopPickupGroundItem(actorId, itemId, preservedCandidate) &&
		preservedCandidate.item == (TacticalWorldItemId{99, 999}) && preservedCandidate.grid == 123 &&
		preservedCandidate.level == 1 && preservedCandidate.objectStateFingerprint == 42,
		"read-only candidate rejects native mirror divergence without replacing prior output");
	CHECK(!CaptureCoopNearbyLootItem(actorId, itemId, preservedCandidate) &&
		preservedCandidate.item == (TacticalWorldItemId{99, 999}) && preservedCandidate.grid == 123 &&
		preservedCandidate.level == 1 && preservedCandidate.objectStateFingerprint == 42,
		"nearby candidate also rejects mirror divergence without repairing identity or replacing output");
	CHECK(!authority.capture(actorId, command.expectedWorldGeneration, next) && next == preservedLedger &&
		gWorldItems[itemId.slot].uiUniqueWorldItemIdValue == 0 &&
		GetJa2TacticalWorldItemDirectory().identity(itemId.slot) == itemId &&
		GetJa2TacticalWorldItemDirectory().nextIncarnation() == noRepairSequence &&
		GetJa2TacticalWorldItemDirectory().activeCount() == noRepairActive,
		"failed ledger capture preserves all output and neither repairs, adopts, nor issues an identity");
	gWorldItems[itemId.slot].uiUniqueWorldItemIdValue = itemId.incarnation;
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) && next == preservedLedger,
		"repairing only fixture mirror restores unchanged ledger revision after failed read-only capture");
	actor.position().gridNo() = origin + 1;
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) && next.groundGrid == origin + 1 &&
		next.groundItems.empty() && next.inventoryRevision > ledger.inventoryRevision,
		"actor tile change replaces private ground context and advances revision");
	actor.position().gridNo() = origin;

	AuthoritativePickupWorldItemCommand sentinel;
	sentinel.expectedWorldGeneration = 999;
	const auto rejected = [&](const char* reason) {
		const auto beforeAP = actor.actionPoints().current();
		const auto beforeCompetitorAP = competitor.actionPoints().current();
		CHECK(!PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, sentinel) &&
			sentinel.expectedWorldGeneration == 999 && dispatch(command).status == SimulationCommandDispatchStatus::Discarded &&
			!destination.exists() && ResolveJa2TacticalWorldItem(itemId) != nullptr && IsJa2TacticalWorldIntegrityValid() &&
			gpItemPointer == nullptr && actor.actionPoints().current() == beforeAP &&
			competitor.actionPoints().current() == beforeCompetitorAP, reason);
	};
	actor.actionPoints().current() = 3;
	CHECK(CaptureCoopPickupGroundItem(actorId, itemId, candidate), "display remains available without sufficient AP");
	rejected("insufficient native pickup AP rejects before mutation");
	actor.actionPoints().current() = 20;
	actor.animationActivity().nonInterruptible() = TRUE;
	CHECK(CaptureCoopPickupGroundItem(actorId, itemId, candidate), "display remains available while actor is busy");
	rejected("busy actor rejects retained execution");
	actor.animationActivity().nonInterruptible() = FALSE;
	// Armed mercs settle into stationary aim poses after combat movement; the
	// animation-free pickup accepts them at the same-height idle cost.
	{
		const struct { UINT16 aim; UINT16 idle; } aimPoses[] = {
			{AIM_RIFLE_STAND, STANDING}, {AIM_RIFLE_CROUCH, CROUCHING}, {AIM_RIFLE_PRONE, PRONE},
			{AIM_DUAL_STAND, STANDING}, {AIM_DUAL_CROUCH, CROUCHING}, {AIM_DUAL_PRONE, PRONE}};
		for (const auto& pose : aimPoses)
		{
			AuthoritativePickupWorldItemCommand idleCommand, aimCommand;
			actor.animationPlayback().state() = pose.idle;
			const bool idleReady = PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, idleCommand);
			actor.animationPlayback().state() = pose.aim;
			const bool aimReady = PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, aimCommand);
			CHECK(idleReady && aimReady && aimCommand.expectedAnimationState == pose.aim &&
				aimCommand.expectedActionPointCost == idleCommand.expectedActionPointCost &&
				!destination.exists() && ResolveJa2TacticalWorldItem(itemId) != nullptr &&
				actor.actionPoints().current() == 20 && gpItemPointer == nullptr,
				"settled aim pose prepares a ground pickup at the same-height idle cost without effects");
		}
		for (const UINT16 busy : {UINT16(PUNCH_BREATH), UINT16(KNIFE_BREATH), UINT16(SHOOT_RIFLE_STAND)})
		{
			actor.animationPlayback().state() = busy;
			rejected("unsettled fire-ready and firing poses cannot pick up");
		}
		actor.animationPlayback().state() = CROUCHING;
	}
	actor.position().gridNo() = origin + 1;
	rejected("adjacent tile is outside same-tile pickup scope");
	actor.position().gridNo() = origin;
	actor.position().level() = SECOND_LEVEL;
	rejected("different floor rejects");
	actor.position().level() = FIRST_LEVEL;
	actor.roster().team() = ENEMY_TEAM;
	rejected("non-player actor rejects");
	actor.roster().team() = OUR_TEAM;
	is_client = true;
	rejected("legacy networking cannot execute authoritative pickup");
	is_client = false;
	ResolveJa2TacticalWorldItem(itemId)->bVisible = HIDDEN_ITEM;
	gTacticalStatus.uiFlags |= SHOW_ALL_ITEMS;
	rejected("debug reveal cannot authorize a hidden item");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) && next.groundItems.empty(),
		"owner ledger omits hidden items even with debug reveal enabled");
	gTacticalStatus.uiFlags &= ~SHOW_ALL_ITEMS;
	ResolveJa2TacticalWorldItem(itemId)->bVisible = VISIBLE;
	ResolveJa2TacticalWorldItem(itemId)->bRenderZHeightAboveLevel = 5;
	inspectNearby(actorId, itemId, false, "raised/table item is not an ordinary ground marker");
	rejected("raised/table item is unsupported");
	ResolveJa2TacticalWorldItem(itemId)->bRenderZHeightAboveLevel = 0;
	ResolveJa2TacticalWorldItem(itemId)->usFlags |= WORLD_ITEM_ARMED_BOMB;
	inspectNearby(actorId, itemId, false, "armed world item is not exposed as nearby loot");
	rejected("world bomb flag rejects");
	ResolveJa2TacticalWorldItem(itemId)->usFlags &= ~WORLD_ITEM_ARMED_BOMB;
	gpWorldLevelData[origin].uiFlags |= MAPELEMENT_PLAYER_MINE_PRESENT;
	inspectNearby(actorId, itemId, false, "marked mine tile cannot advertise ordinary loot");
	rejected("marked mine tile cannot bypass the native disarm branch");
	gpWorldLevelData[origin].uiFlags &= ~MAPELEMENT_PLAYER_MINE_PRESENT;
	worldObject()[0]->data.bTrap = 1;
	inspectNearby(actorId, itemId, false, "trapped ordinary-looking object is not exposed as nearby loot");
	rejected("trapped ordinary-looking object rejects");
	worldObject()[0]->data.bTrap = 0;
	worldObject().fFlags |= OBJECT_ARMED_BOMB;
	inspectNearby(actorId, itemId, false, "armed object payload is not exposed as nearby loot");
	rejected("object armed-bomb flag rejects");
	worldObject().fFlags &= ~OBJECT_ARMED_BOMB;
	MakeObject(worldObject()[0]->attachments.front(), COMBAT_KNIFE, 1, 80);
	inspectNearby(actorId, itemId, false, "unsupported attachment graph is not exposed as nearby loot");
	rejected("attachment-bearing graph is not an ordinary world pickup");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) && next.groundItems.empty(),
		"owner ledger omits unsupported ground graphs");
	worldObject()[0]->attachments.front().initialize();
	worldObject()[0]->data.sObjectFlag ^= 1;
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded && !destination.exists(),
		"same stable ID with altered hidden object metadata rejects stale retained proof");
	worldObject()[0]->data.sObjectFlag ^= 1;
	const TacticalWorldItemId ownership = insert(OWNERSHIP, 1, 100);
	CHECK(ownership.valid(), "separate ownership pool marker inserted");
	ResolveJa2TacticalWorldItem(ownership)->bVisible = HIDDEN_ITEM;
	inspectNearby(actorId, itemId, false, "hidden ownership anywhere in the pool suppresses nearby loot too");
	rejected("hidden ownership marker anywhere in same pool prevents theft bypass");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) && next.groundItems.empty(),
		"owner ledger omits all ordinary items on owned pool");
	CHECK(RemoveItemFromPool(origin, static_cast<INT32>(ownership.slot), FIRST_LEVEL), "native ownership marker cleanup");
	destination.ubMission = 1;
	rejected("noncanonical empty pocket rejects");
	destination.initialize();
	MakeObject(destination, COMBAT_KNIFE, 1, 90);
	CHECK(!PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, sentinel) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Discarded && destination.usItem == COMBAT_KNIFE,
		"occupied destination cannot merge/swap/drop any object");
	destination.initialize();
	CHECK(!PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, HANDPOS, sentinel) &&
		!PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, VESTPOCKPOS, sentinel),
		"hands and worn LBE slots are outside this pocket-only pickup slice");
	for (int kind = 0; kind < 4; ++kind)
	{
		auto stale = command;
		if (kind == 0) ++stale.soldier.incarnation;
		if (kind == 1) ++stale.item.incarnation;
		if (kind == 2) ++stale.expectedWorldGeneration;
		if (kind == 3) ++stale.expectedTurnSerial;
		CHECK(dispatch(stale).status == SimulationCommandDispatchStatus::Discarded && !destination.exists(),
			"stale actor/item/world/turn identity rejects retained pickup");
	}
	// Native new-inventory pocket admission is shared with the existing safe
	// inventory path; test bad data and combat backpack accessibility explicitly.
	gGameOptions.ubInventorySystem = INVENTORY_NEW;
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
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, command),
		"native default LBE pocket accepts complete fitting stack");
	LBEPocketType[1].ItemCapacityPerSize.assign(2, 1);
	rejected("whole stack cannot be partially stuffed into undersized pocket");
	LBEPocketType[1].ItemCapacityPerSize.assign(2, 4);
	LoadBearingEquipment[0].lbePocketIndex.assign(12, 0);
	rejected("disabled native LBE pocket rejects before placement helper indexing");
	LoadBearingEquipment[0].lbePocketIndex.assign(12, 1);
	CHECK(!PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK4POS, sentinel),
		"closed backpack pocket inaccessible in combat");
	actor.inventory().zipperFlag() = TRUE;
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK4POS, command),
		"open crouched backpack ordinary pocket can be selected");
	actor.animationPlayback().state() = STANDING;
	CHECK(!PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK4POS, sentinel) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Discarded, "standing blocks combat backpack pickup");
	actor.animationPlayback().state() = CROUCHING;
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	MakeObject(actor.inventory()[SECONDHANDPOS], KEY_1, 1, 97);
	const auto* opaqueNode = &actor.inventory()[SECONDHANDPOS].objectStack.front();
	CHECK(CaptureInventorySwapObjectState(actorId, SECONDHANDPOS) == 0 &&
		PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, command),
		"opaque untouched offhand does not block an ordinary pocket pickup");
	AuthoritativePickupWorldItemCommand competing;
	CHECK(PrepareAuthoritativePickupWorldItemCommand(competitorId, itemId, BIGPOCK2POS, competing),
		"competing actor can capture the same live item before the first execution");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, ledger), "private ledger captures final pre-pickup state");
	std::vector<RecordedSimulationCommand> records{{7, 9, CommandJournalStatus::Queued, SimulationCommand{command}}};
	std::vector<std::uint8_t> encoded;
	CHECK(EncodeSimulationCommandJournal(records, 0, encoded), "retained ground pickup has explicit journal tag encoding");
	// Persistence header is 6 bytes (U32 magic, U16 version), followed by
	// U64 dropped + U32 count (12), and U64 tick + U64 sequence + U8 status (17).
	CHECK(encoded.size() == 111 && encoded[35] == 38,
		"ground pickup uses fixed explicit tag 38 and 76-byte command payload");
	if (encoded.size() != 111) { cleanup(); return 1; }
	MemoryTacticalSimulation memory;
	TacticalSimulationSnapshot memoryState;
	memoryState.actors.push_back(TacticalSimulationActorState{actorId, origin});
	CHECK(memory.reset(memoryState) == TacticalSimulationResetError::None &&
		memory.execute(SimulationCommand{command}, 7, 9) == CommandDisposition::Discard &&
		memory.snapshot() == memoryState, "portable memory executor cannot invent native ground pickup semantics");
	for (int kind = 0; kind < 8; ++kind)
	{
		auto invalid = command;
		if (kind == 0) invalid.destinationSlot = HANDPOS;
		if (kind == 1) invalid.item.incarnation = 0;
		if (kind == 2) invalid.sourceStateFingerprint = 0;
		if (kind == 3) invalid.expectedActionPointCost = 0;
		if (kind == 4) invalid.expectedActorLevel = 2;
		if (kind == 5) invalid.source = SimulationCommandSource::System;
		if (kind == 6) invalid.authority = TacticalCommandAuthorityPolicy::Legacy;
		if (kind == 7) invalid.expectedPoolStateFingerprint = 0;
		std::vector<std::uint8_t> preserved{0x91};
		CHECK(!IsStructurallyValidSimulationCommand(SimulationCommand{invalid}) &&
			ValidateSimulationCommandDomain(SimulationCommand{invalid}) != SimulationCommandDomainError::None &&
			!EncodeSimulationCommandJournal({RecordedSimulationCommand{7, 9, CommandJournalStatus::Queued,
				SimulationCommand{invalid}}}, 0, preserved) && preserved == std::vector<std::uint8_t>{0x91},
			"malformed pickup command domain cannot enter journal or execute");
	}
	std::vector<RecordedSimulationCommand> decoded;
	std::uint64_t dropped = 999;
	CHECK(DecodeSimulationCommandJournal(encoded, decoded, dropped) == SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 1 && dropped == 0 && std::holds_alternative<AuthoritativePickupWorldItemCommand>(decoded[0].command),
		"ground pickup journal round trips the exact command variant");
	std::vector<std::uint8_t> reencoded;
	CHECK(EncodeSimulationCommandJournal(decoded, dropped, reencoded) && reencoded == encoded,
		"ground pickup journal preserves every captured proof byte");
	for (const auto offset : {std::size_t{35}, std::size_t{50}, std::size_t{71}, std::size_t{109}, std::size_t{110}})
	{
		auto malformed = encoded;
		malformed[offset] = 255;
		dropped = 42;
		CHECK(DecodeSimulationCommandJournal(malformed, decoded, dropped) == SimulationCommandJournalDecodeResult::Invalid &&
			dropped == 42 && decoded.size() == 1, "malformed pickup wire tag/pocket/floor/source/authority rejects transactionally");
		CHECK(EncodeSimulationCommandJournal(decoded, 0, reencoded) && reencoded == encoded,
			"malformed decode preserves every field of the previous output command");
	}
	for (std::size_t length = 0; length < encoded.size(); ++length)
	{
		const std::vector<std::uint8_t> truncated(encoded.begin(), encoded.begin() + length);
		dropped = 42;
		CHECK(DecodeSimulationCommandJournal(truncated, decoded, dropped) != SimulationCommandJournalDecodeResult::Success &&
			dropped == 42 && decoded.size() == 1, "truncated pickup journal rejects transactionally");
	}
	const auto applied = dispatch(command);
	CHECK(applied.status == SimulationCommandDispatchStatus::Applied && game.commands().empty() &&
		ResolveJa2TacticalWorldItem(itemId) == nullptr && !ItemExistsAtLocation(origin, itemId.slot, FIRST_LEVEL) &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == objectFingerprint &&
		&destination.objectStack.front() == originalNode && destination.ubNumberOfObjects == 2 &&
		destination[0]->data.objectStatus + destination[1]->data.objectStatus == 146 &&
		actor.actionPoints().current() == 16 && competitor.actionPoints().current() == 20 && gpItemPointer == nullptr,
		"one retained native pickup conserves kit points/list storage, retires exact ID and charges 4 AP once");
	CHECK(&actor.inventory()[SECONDHANDPOS].objectStack.front() == opaqueNode &&
		actor.inventory()[SECONDHANDPOS][0]->data.objectStatus == 97,
		"unrelated unsupported offhand retains exact object storage and data");
	const auto* immediateActor = GetJa2TacticalEntityDirectory().state(actorId);
	CHECK(immediateActor && immediateActor->actionPoints == 16, "entity-directory AP updates before observer reconciliation");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, next) &&
		next.inventoryRevision > ledger.inventoryRevision && next.groundItems.empty() &&
		next.slots[BIGPOCK2POS].item == FIRSTAIDKIT && next.slots[BIGPOCK2POS].resourceTotal == 146,
		"one private replacement removes ground object and exposes carried medical supply stack");
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded &&
		dispatch(competing).status == SimulationCommandDispatchStatus::Discarded &&
		actor.actionPoints().current() == 16 && competitor.actionPoints().current() == 20 &&
		!competitor.inventory()[BIGPOCK2POS].exists(), "duplicate and competing pickup cannot duplicate resources or AP cost");
	const auto replacement = insert(FIRSTAIDKIT, 1, 49);
	CHECK(replacement.slot == itemId.slot && replacement.incarnation != itemId.incarnation &&
		!PrepareAuthoritativePickupWorldItemCommand(competitorId, itemId, BIGPOCK2POS, sentinel) &&
		dispatch(competing).status == SimulationCommandDispatchStatus::Discarded &&
		ResolveJa2TacticalWorldItem(replacement)->object[0]->data.objectStatus == 49,
		"reused native world slot never retargets a stale item incarnation");
	RestoreJa2TacticalTurnState(ACTIVE | REALTIME, OUR_TEAM, 0);
	competitor.actionPoints().current() = 0;
	CHECK(PrepareAuthoritativePickupWorldItemCommand(competitorId, replacement, BIGPOCK2POS, competing) &&
		dispatch(competing).status == SimulationCommandDispatchStatus::Applied &&
		competitor.actionPoints().current() == 0 && competitor.inventory()[BIGPOCK2POS][0]->data.objectStatus == 49,
		"native real-time pickup needs no combat AP and still moves the exact object once");
	cleanup();
	std::printf("authoritative ground pickup: %d failure(s)\n", failures);
	return failures ? 1 : 0;
}
