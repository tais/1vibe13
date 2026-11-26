// Real retained drop -> pickup roundtrip. Objects, actors, map and allocation
// failure injection exist only in this isolated data-free test executable.
#include "Animation Cache.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "Action Items.h"
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
#include "soldier profile type.h"
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
#include "tiledef.h"
#include "worlddef.h"
#include "worldman.h"
#include <Engine/Adapters/JA2/MemoryTacticalSimulation.h>
#include <Engine/Adapters/JA2/SimulationCommandCodec.h>
#include <Engine/Core/SimulationRandom.h>
#include <cstdio>
#include <array>
#include <cstdlib>
#include <cstring>
#include <new>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }

namespace { int failures = 0; long failAllocationAfter = -1; }
void* operator new(std::size_t size)
{
	if (failAllocationAfter == 0) { failAllocationAfter = -1; throw std::bad_alloc(); }
	if (failAllocationAfter > 0) --failAllocationAfter;
	if (void* value = std::malloc(size ? size : 1)) return value;
	throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace
{
#define CHECK(condition, message) do { if (!(condition)) { \
	std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, message); ++failures; } } while (false)
void MakeObject(OBJECTTYPE& object, UINT16 item, UINT8 count, INT16 status)
{
	object.initialize(); object.usItem = item; object.ubNumberOfObjects = count;
	object.objectStack.resize(count);
	for (auto& stack : object.objectStack) stack.data.objectStatus = status;
}
}

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	const bool exhausted = argc == 2 && std::strcmp(argv[1], "--identity-exhausted") == 0;
	CHECK(InstallGameSimulationRandom(0x64726f706974656dULL) == GameSimulationRandomInstallError::None,
		"fixture owns native RNG");
	GameContext& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) &&
		game.markRunning(), "native runtime starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository(); repository.initializeSlots();
	ResetJa2TacticalActorRosters(); ResetJa2TacticalInterruptForNewWorld(); ResetJa2TacticalWorldItemDirectory();
	CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "logical world exists");
	InitRenderParams(0); NotifyJa2TacticalWorldLoaded(1);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM; is_networked = is_client = is_server = false;
	gGameOptions.ubInventorySystem = INVENTORY_OLD; gGameOptions.fNewTraitSystem = FALSE;
	APBPConstants[AP_PICKUP_ITEM] = 4;
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	const INT32 origin = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS / 2;
	GetMapElement(origin).ubTerrainID = FLAT_GROUND;
	TacticalActor& actor = *repository.resolve(0);
	TacticalActor& competitor = *repository.resolve(1);
	for (auto* merc : {&actor, &competitor})
	{
		const UINT16 slot = merc == &actor ? 0 : 1;
		merc->identity().id() = SoldierID{slot}; merc->identity().incarnation() = 201 + slot;
		merc->identity().profile() = NO_PROFILE; merc->identity().bodyType() = REGMALE;
		merc->roster().active() = merc->roster().inSector() = TRUE; merc->roster().team() = OUR_TEAM;
		merc->assignment().current() = FIRST_SQUAD;
		merc->position().gridNo() = origin; merc->position().level() = FIRST_LEVEL;
		merc->position().direction() = SOUTH; merc->pathing().desiredDirection() = SOUTH;
		merc->animationPlayback().state() = CROUCHING; merc->animationPlayback().surface() = 0;
		merc->animationIntent().clearPendingAnimations(); merc->pendingAction().clearAction();
		merc->vitals().health() = merc->vitals().maximumHealth() = 100;
		merc->vitals().breath() = merc->vitals().maximumBreath() = 100;
		merc->actionPoints().current() = 20; merc->fireControl().selectBarrelMode(1);
		CHECK(AdoptJa2TacticalEntity(*merc), "actor incarnation adopted");
	}
	const auto actorId = GetJa2TacticalEntityId(actor), competitorId = GetJa2TacticalEntityId(competitor);
	gMAXITEMS_READ = DEFAULT_BPACK + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT; Item[FIRSTAIDKIT].ubPerPocket = 4;
	Item[FIRSTAIDKIT].ubGraphicType = 0; Item[FIRSTAIDKIT].ubGraphicNum = 0;
	Item[CLIP9_15].usItemClass = IC_AMMO; Item[CLIP9_15].ubPerPocket = 4;
	Item[CLIP9_15].ubGraphicType = 0; Item[CLIP9_15].ubGraphicNum = 0;
	Item[KEY_1].usItemClass = IC_KEY;
	gNumTilesPerType[GUNS] = 1; gTileTypeStartIndex[GUNS] = 0;
	gTileDatabase[0].fType = GUNS; giNumberOfTiles = 1; giNumberOfTileTypes = NUMBEROFTILETYPES;
	ETRLEObject frames[8]{}; SGPVObject video{};
	video.usNumberOfObjects = 8; video.pETRLEObject = frames;
	const auto retainedSurface = gAnimSurfaceDatabase[0];
	gAnimSurfaceDatabase[0].hVideoObject = &video; gAnimSurfaceDatabase[0].uiNumDirections = 8;
	gAnimSurfaceDatabase[0].uiNumFramesPerDir = 1;
	gAnimSurfaceDatabase[0].bStructDataType = NO_STRUCT; gAnimSurfaceDatabase[0].bProfile = -1;
	const auto cleanup = [&] {
		for (std::size_t slot = 0; slot < gWorldItems.size(); ++slot)
			if (gWorldItems[slot].fExists) (void)RemoveItemFromPool(gWorldItems[slot].sGridNo, slot, gWorldItems[slot].ubLevel);
		for (auto* merc : {&actor, &competitor}) { merc->palette().reset(); merc->animationCache().reset(); }
		gAnimSurfaceDatabase[0] = retainedSurface; ReleaseWorldTileMap();
	};
	std::uint64_t frame = 0;
	const auto dispatch = [&](const auto& command) {
		BeginSimulationCommandFrameBudget(++frame, 1); return TryDispatchSimulationCommandNow(SimulationCommand{command});
	};
	OBJECTTYPE& source = actor.inventory()[BIGPOCK2POS];
	MakeObject(source, FIRSTAIDKIT, 2, 73);
	source.ubMission = 7; source.fFlags = OBJECT_MODIFIED;
	source[0]->data.bTemperature = 12.5f; source[0]->data.sRepairThreshold = 79;
	source[0]->data.sObjectFlag = 0x1000000000000000ULL; source[0]->attachments.resize(4);
	const auto* storage = &source.objectStack.front();
	const auto fingerprint = CaptureInventorySwapObjectState(actorId, BIGPOCK2POS);
	const auto rng = GetGameSimulationRandomSource()->checkpoint();
	const auto nextIdentity = GetJa2TacticalWorldItemDirectory().nextIncarnation();
	AuthoritativeDropInventoryItemCommand command;
	CHECK(PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK2POS, command) &&
		command.expectedActionPointCost == 4 && GetAPsToPickupItem(&actor, origin) == 0,
		"drop uses the real basic pickup cost even on an empty tile");
	if (!command.expectedWorldGeneration) { cleanup(); return 1; }
	CoopOrdinaryGroundDropPlacement placement;
	CHECK(CaptureCoopOrdinaryGroundDropPlacement(actorId, BIGPOCK2POS, placement), "native safe placement captured");
	if (exhausted)
	{
		GetJa2TacticalWorldItemDirectory().mergeNextIncarnation(0);
		TacticalWorldItemId unchanged{4, 9};
		CHECK(CommitCoopOrdinaryGroundDrop(actorId, BIGPOCK2POS, fingerprint, placement, unchanged) ==
			CoopOrdinaryGroundDropResult::IdentityUnavailable && unchanged == (TacticalWorldItemId{4, 9}) &&
			CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == fingerprint &&
			GetJa2TacticalWorldItemDirectory().activeCount() == 0 && actor.actionPoints().current() == 20 &&
			GetMapElement(origin).pStructHead == nullptr && IsJa2TacticalWorldIntegrityValid(),
			"exhausted identity space cannot consume an object, charge AP or publish graphics");
		CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded &&
			CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == fingerprint,
			"retained command also rejects exhausted identity space without mutation");
		cleanup(); return failures ? 1 : 0;
	}
	// Exercise real C++ allocation failure before insertion publication.
	TacticalWorldItemId unchanged{4, 9};
	failAllocationAfter = 0;
	const auto failed = CommitCoopOrdinaryGroundDrop(actorId, BIGPOCK2POS, fingerprint, placement, unchanged);
	failAllocationAfter = -1;
	CHECK(failed == CoopOrdinaryGroundDropResult::AllocationFailure && unchanged == (TacticalWorldItemId{4, 9}) &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == fingerprint && &source.objectStack.front() == storage &&
		GetJa2TacticalWorldItemDirectory().activeCount() == 0 &&
		GetJa2TacticalWorldItemDirectory().nextIncarnation() == nextIdentity &&
		GetMapElement(origin).pStructHead == nullptr && actor.actionPoints().current() == 20 &&
		IsJa2TacticalWorldIntegrityValid(), "allocation failure leaves source/AP/identity/pool untouched");
	const auto reject = [&](const char* message) {
		const auto ap = actor.actionPoints().current();
		const auto previous = CaptureInventorySwapObjectState(actorId, BIGPOCK2POS);
		AuthoritativeDropInventoryItemCommand sentinel; sentinel.expectedWorldGeneration = 999;
		CHECK(!PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK2POS, sentinel) &&
			sentinel.expectedWorldGeneration == 999 && dispatch(command).status == SimulationCommandDispatchStatus::Discarded &&
			actor.actionPoints().current() == ap && CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == previous &&
			GetJa2TacticalWorldItemDirectory().activeCount() == 0, message);
	};
	actor.position().level() = SECOND_LEVEL; reject("roof-level drop rejects"); actor.position().level() = FIRST_LEVEL;
	GetMapElement(origin).ubTerrainID = DEEP_WATER; reject("water drop cannot destroy or sink the stack");
	GetMapElement(origin).ubTerrainID = FLAT_GROUND;
	GetMapElement(origin).uiFlags |= MAPELEMENT_PLAYER_MINE_PRESENT; reject("mine-marked tile rejects");
	GetMapElement(origin).uiFlags &= ~MAPELEMENT_PLAYER_MINE_PRESENT;
	LEVELNODE obstacle{}; GetMapElement(origin).pStructHead = &obstacle;
	reject("structural tile cannot redirect drop placement"); GetMapElement(origin).pStructHead = nullptr;
	GetMapElement(origin).pRoofHead = &obstacle; reject("roof-covered tile is outside the empty clear-ground slice");
	GetMapElement(origin).pRoofHead = nullptr;
	source[0]->data.bTrap = 1; reject("trapped source rejects"); source[0]->data.bTrap = 0;
	MakeObject(source[0]->attachments.front(), DEFAULT_VEST, 1, 50); reject("an unsupported attached graph rejects");
	source[0]->attachments.front().initialize();
	actor.actionPoints().current() = 3; reject("insufficient combat AP never moves the stack"); actor.actionPoints().current() = 20;
	actor.animationPlayback().state() = WALKING; reject("moving merc cannot drop a retained stack");
	// Armed mercs settle into stationary aim poses after combat movement; the
	// animation-free drop accepts them. Unsettled fire-ready and firing states do not.
	{
		const auto settled = CaptureInventorySwapObjectState(actorId, BIGPOCK2POS);
		for (const UINT16 aim : {UINT16(AIM_RIFLE_STAND), UINT16(AIM_RIFLE_CROUCH), UINT16(AIM_RIFLE_PRONE),
				UINT16(AIM_DUAL_STAND), UINT16(AIM_DUAL_CROUCH), UINT16(AIM_DUAL_PRONE)})
		{
			actor.animationPlayback().state() = aim;
			AuthoritativeDropInventoryItemCommand aimed;
			CHECK(PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK2POS, aimed) &&
				aimed.expectedAnimationState == aim &&
				aimed.expectedActionPointCost == command.expectedActionPointCost &&
				CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == settled &&
				actor.actionPoints().current() == 20 && GetJa2TacticalWorldItemDirectory().activeCount() == 0,
				"settled aim pose prepares a pocket drop at the stock cost without effects");
		}
		for (const UINT16 busy : {UINT16(PUNCH_BREATH), UINT16(KNIFE_BREATH), UINT16(SHOOT_RIFLE_STAND)})
		{
			actor.animationPlayback().state() = busy;
			reject("unsettled fire-ready and firing poses cannot drop");
		}
	}
	actor.animationPlayback().state() = CROUCHING;
	auto stale = command; ++stale.sourceStateFingerprint;
	CHECK(dispatch(stale).status == SimulationCommandDispatchStatus::Discarded, "stale source proof rejects");
	stale = command; ++stale.expectedWorldGeneration;
	CHECK(dispatch(stale).status == SimulationCommandDispatchStatus::Discarded, "stale world rejects");
	stale = command; ++stale.expectedTurnSerial;
	CHECK(dispatch(stale).status == SimulationCommandDispatchStatus::Discarded, "stale turn rejects");
	for (const auto slot : {HANDPOS, SECONDHANDPOS, HELMETPOS})
		CHECK(!PrepareAuthoritativeDropInventoryItemCommand(actorId, slot, stale), "hands and equipment never enter pocket drop");
	MakeObject(actor.inventory()[SECONDHANDPOS], DEFAULT_VEST, 1, 97); // keys are ordinary now; gear stays opaque
	const auto* opaque = &actor.inventory()[SECONDHANDPOS].objectStack.front();
	CHECK(PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK2POS, command), "unrelated opaque hands do not block drop");
	MakeObject(competitor.inventory()[BIGPOCK2POS], FIRSTAIDKIT, 1, 50);
	AuthoritativeDropInventoryItemCommand competing;
	CHECK(PrepareAuthoritativeDropInventoryItemCommand(competitorId, BIGPOCK2POS, competing), "second actor can prepare the empty tile");
	Ja2CoopInventoryAuthority authority; CoopSession::CoopOwnerInventorySnapshot before, after;
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, before) && before.groundItems.empty() &&
		before.nearbyLoot.empty(), "private baseline sees carried kit and empty ground");
	std::vector<std::uint8_t> encoded;
	CHECK(EncodeSimulationCommandJournal({{7, 9, CommandJournalStatus::Queued, SimulationCommand{command}}}, 0, encoded) &&
		encoded.size() > 35 && encoded[35] == 39, "drop persists under explicit journal tag39 without version bump");
	std::vector<RecordedSimulationCommand> decoded; std::uint64_t dropped = 999;
	CHECK(DecodeSimulationCommandJournal(encoded, decoded, dropped) == SimulationCommandJournalDecodeResult::Success &&
		decoded.size() == 1 && dropped == 0, "drop journal roundtrips");
	std::vector<std::uint8_t> roundtrip;
	CHECK(EncodeSimulationCommandJournal(decoded, dropped, roundtrip) && roundtrip == encoded, "every command proof byte survives");
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Applied && actor.actionPoints().current() == 16 &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == 1 &&
		GetJa2TacticalWorldItemDirectory().activeCount() == 1, "retained drop commits once and charges four AP");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, after) && after.inventoryRevision > before.inventoryRevision &&
		after.groundItems.size() == 1 && after.nearbyLoot.size() == 1 && after.nearbyLoot[0].hasMedicalKit &&
		after.groundItems[0].summary.resourceTotal == 146,
		"one owner revision replaces carried kit with exact ground stack and medical marker");
	if (after.groundItems.size() != 1) { cleanup(); return 1; }
	const auto itemId = after.groundItems[0].id;
	const WORLDITEM* worldItem = ResolveJa2TacticalWorldItem(itemId);
	CHECK(worldItem && &worldItem->object.objectStack.front() == storage &&
		CaptureInventorySwapObjectState(actorId, SECONDHANDPOS) == 0 &&
		&actor.inventory()[SECONDHANDPOS].objectStack.front() == opaque && gpItemPointer == nullptr,
		"drop preserves exact object list storage and leaves opaque hands/cursor untouched");
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded && actor.actionPoints().current() == 16 &&
		dispatch(competing).status == SimulationCommandDispatchStatus::Discarded && competitor.actionPoints().current() == 20 &&
		GetJa2TacticalWorldItemDirectory().activeCount() == 1, "duplicate and competing drops cannot duplicate items or AP charges");
	AuthoritativePickupWorldItemCommand pickup;
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, itemId, BIGPOCK2POS, pickup) &&
		dispatch(pickup).status == SimulationCommandDispatchStatus::Applied && actor.actionPoints().current() == 12 &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == fingerprint &&
		&source.objectStack.front() == storage && ResolveJa2TacticalWorldItem(itemId) == nullptr,
		"native drop then pickup conserves every object field and list node, retiring the new identity");
	CHECK(authority.capture(actorId, command.expectedWorldGeneration, after) && after.groundItems.empty() &&
		after.nearbyLoot.empty() && after.slots == before.slots, "private carried/ground/marker state roundtrips exactly");
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED, OUR_TEAM, 0); actor.actionPoints().current() = 0;
	MakeObject(source, CLIP9_15, 2, 73); source[0]->data.ubShotsLeft = 40000; source[1]->data.ubShotsLeft = 20000;
	const auto ammoFingerprint = CaptureInventorySwapObjectState(actorId, BIGPOCK2POS);
	CHECK(PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK2POS, command) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied && actor.actionPoints().current() == 0 &&
		authority.capture(actorId, command.expectedWorldGeneration, after) && after.groundItems.size() == 1 &&
		after.groundItems[0].summary.resourceTotal == 60000 && !after.nearbyLoot[0].hasMedicalKit,
		"real-time drop charges no AP and preserves unsigned whole-stack ammunition");
	if (after.groundItems.size() == 1)
	{
		const auto replacement = after.groundItems[0].id;
		CHECK(replacement != itemId && ResolveJa2TacticalWorldItem(itemId) == nullptr,
			"reused world storage receives a fresh identity, never reviving old pickup claims");
		CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, replacement, BIGPOCK2POS, pickup) &&
			dispatch(pickup).status == SimulationCommandDispatchStatus::Applied && actor.actionPoints().current() == 0 &&
			CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == ammoFingerprint,
			"real-time ammunition roundtrip is exact and AP-free");
	}

	// Force growth from a genuinely full nonempty native vector. The unrelated
	// item has a real native pool/graphic and a deliberately noncanonical action
	// payload: OBJECTTYPE move assignment would normalize it if used here.
	const INT32 remoteGrid = origin + 30;
	OBJECTTYPE remoteSeed; MakeObject(remoteSeed, FIRSTAIDKIT, 1, 81);
	INT32 remoteSlot = -1;
	CHECK(AddItemToPoolAndGetIndex(remoteGrid, &remoteSeed, VISIBLE, FIRST_LEVEL, WORLD_ITEM_DROPPED_FROM_ENEMY, 0,
		NOBODY, &remoteSlot) != nullptr && remoteSlot == 0, "real unrelated pool occupies the first retired slot");
	if (remoteSlot != 0) { cleanup(); return 1; }
	gWorldItems.resize(1); guiNumWorldItems = 1;
	WORLDITEM& remote = gWorldItems[0];
	MakeObject(remote.object, ACTION_ITEM, 2, 81);
	remote.object.ubMission = 19; remote.object.fFlags = OBJECT_MODIFIED;
	remote.object[0]->data.misc.bActionValue = ACTION_ITEM_BLOW_UP;
	remote.object[0]->data.misc.usBombItem = FIRSTAIDKIT;
	remote.object[0]->data.ubWireNetworkFlag = 0xED;
	remote.object[0]->data.ubDirection = WEST;
	remote.object[0]->data.bDefuseFrequency = 37;
	remote.object[0]->attachments.resize(2);
	MakeObject(remote.object[0]->attachments.front(), KEY_1, 1, 47);
	remote.object[1]->data.ubWireNetworkFlag = 0x32;
	remote.object[1]->data.ubDirection = NORTHEAST;
	remote.object[1]->data.bDefuseFrequency = 23;
	remote.ubNonExistChance = 17; remote.soldierID = SoldierID{1};
	remote.usFlags = WORLD_ITEM_DROPPED_FROM_ENEMY;
	const auto remoteId = GetJa2TacticalWorldItemDirectory().identity(0);
	const auto* remoteFirstNode = &remote.object.objectStack.front();
	const auto* remoteLastNode = &remote.object.objectStack.back();
	const auto* remoteAttachmentNode = &remote.object[0]->attachments.front().objectStack.front();
	std::array<unsigned char, sizeof(ObjectData)> firstData{}, lastData{};
	std::memcpy(firstData.data(), &remote.object[0]->data, firstData.size());
	std::memcpy(lastData.data(), &remote.object[1]->data, lastData.size());
	ITEM_POOL* remotePool = nullptr;
	CHECK(GetItemPool(remoteGrid, &remotePool, FIRST_LEVEL) && remotePool,
		"unrelated item keeps a genuine native pool");
	LEVELNODE* const remoteGraphic = GetMapElement(remoteGrid).pStructHead;
	const auto remoteIntact = [&]() -> bool {
		const WORLDITEM* current = ResolveJa2TacticalWorldItem(remoteId);
		ITEM_POOL* currentPool = nullptr;
		return current && current->fExists && current->sGridNo == remoteGrid &&
			current->ubLevel == FIRST_LEVEL && current->bVisible == VISIBLE &&
			current->bRenderZHeightAboveLevel == 0 && current->ubNonExistChance == 17 &&
			current->soldierID == SoldierID{1} && current->usFlags == WORLD_ITEM_DROPPED_FROM_ENEMY &&
			current->object.usItem == ACTION_ITEM && current->object.ubNumberOfObjects == 2 &&
			current->object.ubMission == 19 && current->object.fFlags == OBJECT_MODIFIED &&
			current->object.objectStack.size() == 2 &&
			&current->object.objectStack.front() == remoteFirstNode &&
			&current->object.objectStack.back() == remoteLastNode &&
			current->object[0]->attachments.size() == 2 &&
			&current->object[0]->attachments.front().objectStack.front() == remoteAttachmentNode &&
			current->object[0]->attachments.front().usItem == KEY_1 &&
			current->object[0]->attachments.front()[0]->data.objectStatus == 47 &&
			std::memcmp(&current->object[0]->data, firstData.data(), firstData.size()) == 0 &&
			std::memcmp(&current->object[1]->data, lastData.data(), lastData.size()) == 0 &&
			GetMapElement(remoteGrid).pStructHead == remoteGraphic &&
			GetItemPool(remoteGrid, &currentPool, FIRST_LEVEL) && currentPool == remotePool &&
			currentPool->iItemIndex == 0 && currentPool->pPrev == nullptr && currentPool->pNext == nullptr &&
			currentPool->pLevelNode == remoteGraphic && remoteGraphic->pItemPool == currentPool;
	};
	CHECK(remoteIntact(), "unrelated special-object baseline is exact before staging");
	CHECK(CaptureCoopOrdinaryGroundDropPlacement(actorId, BIGPOCK2POS, placement),
		"same-tile drop placement remains eligible beside unrelated special item");
	const auto* fullVectorData = gWorldItems.data();
	const auto* fullSourceStorage = &source.objectStack.front();
	const auto fullSourceFingerprint = CaptureInventorySwapObjectState(actorId, BIGPOCK2POS);
	const auto fullTrackedSlots = GetJa2TacticalWorldItemDirectory().trackedSlots();
	const auto fullNextIdentity = GetJa2TacticalWorldItemDirectory().nextIncarnation();
	const auto fullLevelNodeCount = guiLevelNodes;
	const auto fullOriginFlags = GetMapElement(origin).uiFlags;
	const auto fullAP = actor.actionPoints().current();
	bool sweptToSuccess = false;
	long failedAllocations = 0;
	TacticalWorldItemId grownItem;
	for (long allocation = 0; allocation < 512; ++allocation)
	{
		TacticalWorldItemId resultId{77, 88};
		failAllocationAfter = allocation;
		const auto result = CommitCoopOrdinaryGroundDrop(actorId, BIGPOCK2POS,
			fullSourceFingerprint, placement, resultId);
		failAllocationAfter = -1;
		if (result == CoopOrdinaryGroundDropResult::Applied)
		{
			sweptToSuccess = true; grownItem = resultId; break;
		}
		++failedAllocations;
		CHECK(result == CoopOrdinaryGroundDropResult::AllocationFailure &&
			resultId == (TacticalWorldItemId{77, 88}) && gWorldItems.size() == 1 && guiNumWorldItems == 1 &&
			gWorldItems.data() == fullVectorData && remoteIntact() &&
			CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == fullSourceFingerprint &&
			&source.objectStack.front() == fullSourceStorage && actor.actionPoints().current() == fullAP &&
			GetJa2TacticalWorldItemDirectory().activeCount() == 1 &&
			GetJa2TacticalWorldItemDirectory().trackedSlots() == fullTrackedSlots &&
			GetJa2TacticalWorldItemDirectory().nextIncarnation() == fullNextIdentity &&
			!GetJa2TacticalWorldItemDirectory().identity(1).valid() && guiLevelNodes == fullLevelNodeCount &&
			GetMapElement(origin).pStructHead == nullptr && GetMapElement(origin).uiFlags == fullOriginFlags &&
			GetGameSimulationRandomSource()->checkpoint() == rng && IsJa2TacticalWorldIntegrityValid(),
			"every C++ staging allocation failure preserves all native logical state and unrelated item bytes");
	}
	CHECK(sweptToSuccess && failedAllocations > 100 && grownItem.slot == 1 &&
		gWorldItems.size() > 1 && guiNumWorldItems == gWorldItems.size() &&
		GetJa2TacticalWorldItemDirectory().activeCount() == 2 &&
		GetJa2TacticalWorldItemDirectory().nextIncarnation() == fullNextIdentity + 1 &&
		guiLevelNodes == fullLevelNodeCount + 1 && remoteIntact() &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == 1 &&
		ResolveJa2TacticalWorldItem(grownItem) &&
		&ResolveJa2TacticalWorldItem(grownItem)->object.objectStack.front() == fullSourceStorage,
		"full-vector growth publishes once and preserves unrelated action/attachment metadata and stable list addresses");
	if (!sweptToSuccess) { cleanup(); return 1; }
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, grownItem, BIGPOCK2POS, pickup) &&
		dispatch(pickup).status == SimulationCommandDispatchStatus::Applied &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == fullSourceFingerprint && remoteIntact(),
		"grown world item returns through real native pickup without changing unrelated special storage");
	CHECK(RemoveItemFromPool(remoteGrid, remoteId.slot, FIRST_LEVEL) &&
		!gWorldItems[0].fExists && gWorldItems[0].object.usItem == ACTION_ITEM &&
		ResolveJa2TacticalWorldItem(remoteId) == nullptr, "native retirement leaves special object data in reusable slot");
	CHECK(CaptureCoopOrdinaryGroundDropPlacement(actorId, BIGPOCK2POS, placement), "retired-slot placement recaptured");
	TacticalWorldItemId reusedSpecial;
	CHECK(CommitCoopOrdinaryGroundDrop(actorId, BIGPOCK2POS, fullSourceFingerprint, placement, reusedSpecial) ==
		CoopOrdinaryGroundDropResult::Applied && reusedSpecial.slot == 0 && reusedSpecial != remoteId &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == 1 &&
		ResolveJa2TacticalWorldItem(reusedSpecial) &&
		&ResolveJa2TacticalWorldItem(reusedSpecial)->object.objectStack.front() == fullSourceStorage &&
		ResolveJa2TacticalWorldItem(remoteId) == nullptr,
		"reusing a retired special-object slot yields a canonical empty pocket, exact ordinary stack and fresh identity");
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, reusedSpecial, BIGPOCK2POS, pickup) &&
		dispatch(pickup).status == SimulationCommandDispatchStatus::Applied &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK2POS) == fullSourceFingerprint,
		"retired special-slot reuse also roundtrips through real pickup");

	// Native trait-adjusted AP is captured and revalidated, not a hardcoded UI cost.
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	actor.actionPoints().current() = 20;
	const auto originalTrait = actor.statistics().skillTrait(0);
	const auto originalMaximumTraits = gSkillTraitValues.ubMaxNumberOfTraits;
	const auto originalPickupReduction = gSkillTraitValues.ubAMPickItemsAPsReduction;
	gGameOptions.fNewTraitSystem = TRUE; actor.statistics().skillTrait(0) = AMBIDEXTROUS_NT;
	gSkillTraitValues.ubMaxNumberOfTraits = 1; gSkillTraitValues.ubAMPickItemsAPsReduction = 50;
	CHECK(PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK2POS, command) &&
		command.expectedActionPointCost == 2 && dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		actor.actionPoints().current() == 18, "ambidextrous native drop charges the trait-adjusted two AP");
	const auto traitItem = GetJa2TacticalWorldItemDirectory().identity(0);
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, traitItem, BIGPOCK2POS, pickup) &&
		dispatch(pickup).status == SimulationCommandDispatchStatus::Applied && actor.actionPoints().current() == 16,
		"trait-adjusted native pickup completes the exact roundtrip");
	gGameOptions.fNewTraitSystem = FALSE; actor.statistics().skillTrait(0) = originalTrait;
	gSkillTraitValues.ubMaxNumberOfTraits = originalMaximumTraits;
	gSkillTraitValues.ubAMPickItemsAPsReduction = originalPickupReduction;

	// Retained drops must honor ordinary NIV access even though empty-ground
	// placement itself deliberately has no inventory UI/idle/AP dependencies.
	gGameOptions.ubInventorySystem = INVENTORY_NEW; gGameExternalOptions.guiMaxItemSize = 1;
	LoadBearingEquipment.resize(1); LoadBearingEquipment[0].lbePocketIndex.assign(12, 1);
	LBEPocketType.resize(VEHICLE_POCKET_TYPE + 1);
	LBEPocketType[1].ItemCapacityPerSize.assign(2, 4);
	LBEPocketType[VEHICLE_POCKET_TYPE].ItemCapacityPerSize.assign(2, 4);
	for (UINT16 gear : {UINT16(DEFAULT_CPACK), UINT16(DEFAULT_BPACK), UINT16(DEFAULT_VEST), UINT16(DEFAULT_THIGH)})
	{ Item[gear].usItemClass = IC_LBEGEAR; Item[gear].ubClassIndex = 0; }
	OBJECTTYPE& backpackSource = actor.inventory()[BIGPOCK4POS];
	MakeObject(backpackSource, FIRSTAIDKIT, 2, 61);
	const auto backpackFingerprint = CaptureInventorySwapObjectState(actorId, BIGPOCK4POS);
	actor.inventory().zipperFlag() = TRUE; actor.actionPoints().current() = 20;
	CHECK(PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK4POS, command),
		"open crouched backpack source prepares an ordinary drop");
	const auto rejectBackpack = [&](const char* message) {
		AuthoritativeDropInventoryItemCommand sentinel; sentinel.expectedWorldGeneration = 999;
		CHECK(!PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK4POS, sentinel) &&
			sentinel.expectedWorldGeneration == 999 && dispatch(command).status == SimulationCommandDispatchStatus::Discarded &&
			actor.actionPoints().current() == 20 && GetJa2TacticalWorldItemDirectory().activeCount() == 0 &&
			CaptureInventorySwapObjectState(actorId, BIGPOCK4POS) == backpackFingerprint, message);
	};
	actor.inventory().zipperFlag() = FALSE; rejectBackpack("closed combat backpack cannot submit or execute drop");
	actor.inventory().zipperFlag() = TRUE;
	actor.animationPlayback().state() = STANDING; rejectBackpack("standing blocks retained combat backpack drop");
	actor.animationPlayback().state() = CROUCHING;
	LoadBearingEquipment[0].lbePocketIndex[icPocket[BIGPOCK4POS]] = 0;
	rejectBackpack("disabled source LBE pocket rejects before native vector lookup");
	LoadBearingEquipment[0].lbePocketIndex[icPocket[BIGPOCK4POS]] = 1;
	MakeObject(actor.inventory()[BPACKPOCKPOS], DEFAULT_BPACK, 1, 100);
	actor.inventory()[BPACKPOCKPOS][0]->data.lbe.bLBE = 1;
	rejectBackpack("active backpack LBE graph stays outside ordinary retained drop");
	actor.inventory()[BPACKPOCKPOS].initialize();
	CHECK(PrepareAuthoritativeDropInventoryItemCommand(actorId, BIGPOCK4POS, command) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied && actor.actionPoints().current() == 16,
		"accessible ordinary backpack stack drops with native AP cost");
	const auto backpackItem = GetJa2TacticalWorldItemDirectory().identity(0);
	CHECK(PrepareAuthoritativePickupWorldItemCommand(actorId, backpackItem, BIGPOCK4POS, pickup) &&
		dispatch(pickup).status == SimulationCommandDispatchStatus::Applied && actor.actionPoints().current() == 12 &&
		CaptureInventorySwapObjectState(actorId, BIGPOCK4POS) == backpackFingerprint,
		"open crouched backpack drop/pickup conserves the full stack");
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	CHECK(GetGameSimulationRandomSource()->checkpoint() == rng && IsJa2TacticalWorldIntegrityValid(),
		"drop/pickup and rejected proposals do not consume simulation RNG or corrupt the world");
	cleanup();
	if (!failures) std::puts("authoritative ground drop tests passed");
	return failures ? 1 : 0;
}
