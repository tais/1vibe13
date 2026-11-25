// Isolated real native transfer execution; fixtures supply data, not outcomes.
#include "Animation Cache.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Items.h"
#include "Overhead.h"
#include "Points.h"
#include "renderworld.h"
#include "Simulation Commands.h"
#include "Soldier Profile Constants.h"
#include "SoldierRepository.h"
#include "Squads.h"
#include "TacticalActor.h"
#include "TacticalActorPendingActionTypes.h"
#include "TacticalActorVisibility.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "Weapons.h"
#include "World Tile Map.h"
#include "connect.h"
#include "worlddef.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
}

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	const bool failureCase = argc == 2 && std::strcmp(argv[1], "--missing-animation") == 0;
	GameContext& game = GetGameContext();
	CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) &&
		game.markRunning(), "native runtime starts");
	if (failures) return 1;
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	ResetJa2TacticalActorRosters();
	ResetJa2TacticalInterruptForNewWorld();
	CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "logical world exists");
	InitRenderParams(0);
	NotifyJa2TacticalWorldLoaded(1);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM;
	is_networked = is_client = is_server = false;
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	gGameExternalOptions.fInventoryCostsAP = TRUE;
	gGameExternalOptions.ubStraightSightRange = 20;
	for (auto& modifier : gGameExternalOptions.ubBrightnessVisionMod) modifier = 100;
	TacticalActorVisibility::initializeRanges();
	gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
	gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{1};
	TacticalActor& sender = *repository.resolve(0);
	TacticalActor& receiver = *repository.resolve(1);
	const INT32 origin = (WORLD_ROWS / 2) * WORLD_COLS + WORLD_COLS / 2;
	for (TacticalActor* actor : {&sender, &receiver})
	{
		const UINT16 slot = actor == &sender ? 0 : 1;
		actor->identity().id() = SoldierID{slot};
		actor->identity().incarnation() = 101 + slot;
		actor->identity().profile() = NO_PROFILE;
		actor->identity().bodyType() = REGMALE;
		actor->roster().active() = actor->roster().inSector() = TRUE;
		actor->roster().team() = OUR_TEAM;
		actor->assignment().current() = FIRST_SQUAD;
		actor->position().gridNo() = origin + slot;
		actor->position().level() = FIRST_LEVEL;
		actor->position().direction() = SOUTH;
		actor->pathing().desiredDirection() = SOUTH;
		actor->animationPlayback().state() = CROUCHING;
		actor->animationPlayback().surface() = 0;
		actor->animationIntent().clearPendingAnimations();
		actor->pendingAction().clearAction();
		actor->vitals().health() = actor->vitals().maximumHealth() = 100;
		actor->vitals().breath() = actor->vitals().maximumBreath() = 100;
		actor->actionPoints().current() = 20;
		actor->fireControl().selectBarrelMode(1);
		CHECK(AdoptJa2TacticalEntity(*actor), "exact merc identity adopted");
	}
	const auto from = GetJa2TacticalEntityId(sender);
	const auto to = GetJa2TacticalEntityId(receiver);
	gMAXITEMS_READ = DEFAULT_BPACK + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
	Item[FIRSTAIDKIT].ubPerPocket = 4;
	Item[COMBAT_KNIFE].usItemClass = IC_BLADE;
	Item[COMBAT_KNIFE].ubPerPocket = 4;
	Item[KEY_1].usItemClass = IC_KEY;
	OBJECTTYPE& source = sender.inventory()[BIGPOCK1POS];
	OBJECTTYPE& destination = receiver.inventory()[BIGPOCK2POS];
	MakeObject(source, FIRSTAIDKIT, failureCase ? 1 : 2, 73);
	source.ubMission = 7;
	source.fFlags = OBJECT_MODIFIED;
	source[0]->data.sRepairThreshold = 79;
	source[0]->data.sObjectFlag = 0x1020304050607080ULL;
	source[0]->data.bTemperature = 12.5f;
	source[0]->attachments.resize(4);
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
		for (auto* actor : {&sender, &receiver}) { actor->palette().reset(); actor->animationCache().reset(); }
		gAnimSurfaceDatabase[0] = retainedSurface;
		ReleaseWorldTileMap();
	};
	std::uint64_t frame = 0;
	const auto dispatch = [&](const TransferInventoryItemCommand& command) {
		BeginSimulationCommandFrameBudget(++frame, 1);
		return TryDispatchSimulationCommandNow(SimulationCommand{command});
	};
	TransferInventoryItemCommand command;
	const UINT8 targetSlot = failureCase ? HANDPOS : BIGPOCK2POS;
	CHECK(PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, targetSlot, command),
		"server prepares native range/LOS, placement, identities and both actor proofs");
	if (!command.expectedWorldGeneration) { cleanup(); return 1; }
	const auto objectFingerprint = CaptureInventorySwapObjectState(from, BIGPOCK1POS);
	const auto* originalNode = &source.objectStack.front();
	if (failureCase)
	{
		gubAnimSurfaceIndex[REGMALE][CROUCHING] = FOUND_INVALID_ANIMATION;
		const auto result = dispatch(command);
		CHECK(result.status == SimulationCommandDispatchStatus::RetryDeferred &&
			!IsJa2TacticalWorldIntegrityValid() && game.commands().size() == 1 && !source.exists() &&
			CaptureInventorySwapObjectState(to, HANDPOS) == objectFingerprint &&
			sender.actionPoints().current() == 20 && receiver.actionPoints().current() == 20,
			"post-transfer native animation failure poisons integrity and never reports Applied or charges AP");
		BeginSimulationCommandFrameBudget(++frame, 8);
		const auto retry = ExecuteSimulationCommandsThrough(game.runtime().simulationTicks().completedTickSequence(), 8);
		CHECK(retry.discarded == 1 && retry.applied == 0 && game.commands().empty() && !source.exists() &&
			CaptureInventorySwapObjectState(to, HANDPOS) == objectFingerprint,
			"invalid-world retry discards without duplicating or reversing transfer");
		for (const auto& record : game.commandJournal().snapshot())
			if (record.sequence == result.sequence)
				CHECK(record.status != CommandJournalStatus::Applied, "failed parent has no Applied journal record");
		cleanup();
		return failures ? 1 : 0;
	}
	CHECK(command.handStateFingerprint == 0 && command.targetOffhandStateFingerprint == 0 &&
		command.expectedActionPointCost == 2, "pocket-only proof does not interpret unrelated hands");
	const auto unchanged = [&] {
		return CaptureInventorySwapObjectState(from, BIGPOCK1POS) == objectFingerprint && !destination.exists();
	};
	TransferInventoryItemCommand sentinel; sentinel.expectedWorldGeneration = 999;
	const auto rejects = [&](const char* reason) {
		CHECK(!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, sentinel) &&
			sentinel.expectedWorldGeneration == 999 && dispatch(command).status == SimulationCommandDispatchStatus::Discarded &&
			unchanged() && IsJa2TacticalWorldIntegrityValid(), reason);
	};
	sender.actionPoints().current() = 2;
	rejects("sender with 2 AP cannot enter native 3 AP admission gate");
	sender.actionPoints().current() = 20;
	receiver.actionPoints().current() = 2;
	rejects("receiver with 2 AP cannot spend someone else's unavailable AP");
	receiver.actionPoints().current() = 20;
	receiver.position().gridNo() = origin + 4;
	rejects("out-of-range recipient rejects without moving actors or items");
	receiver.position().gridNo() = origin + 1;
	receiver.position().level() = SECOND_LEVEL;
	rejects("different elevation rejects");
	receiver.position().level() = FIRST_LEVEL;
	sender.perception().setBlindness(1);
	rejects("native sight denial rejects even an adjacent same-level recipient");
	sender.perception().setBlindness(0);
	receiver.vitals().health() = 1;
	rejects("unconscious recipient is outside this transfer slice");
	receiver.vitals().health() = 100;
	receiver.animationActivity().nonInterruptible() = TRUE;
	rejects("busy recipient cannot be interrupted by transfer");
	receiver.animationActivity().nonInterruptible() = FALSE;
	// Armed mercs settle into stationary aim poses after combat movement. Either
	// role may be aiming; unsettled fire-ready and firing states still own the actor.
	for (TacticalActor* aiming : {&sender, &receiver})
	{
		for (const UINT16 aim : {UINT16(AIM_RIFLE_STAND), UINT16(AIM_RIFLE_CROUCH), UINT16(AIM_RIFLE_PRONE),
				UINT16(AIM_DUAL_STAND), UINT16(AIM_DUAL_CROUCH), UINT16(AIM_DUAL_PRONE)})
		{
			aiming->animationPlayback().state() = aim;
			TransferInventoryItemCommand aimed;
			CHECK(PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, aimed) &&
				(aiming == &sender ? aimed.expectedAnimationState : aimed.expectedTargetAnimationState) == aim &&
				aimed.expectedActionPointCost == command.expectedActionPointCost && unchanged() &&
				sender.actionPoints().current() == 20 && receiver.actionPoints().current() == 20,
				"settled aim pose prepares a pocket transfer for either role at the stock cost without effects");
		}
		for (const UINT16 busy : {UINT16(PUNCH_BREATH), UINT16(KNIFE_BREATH), UINT16(SHOOT_RIFLE_STAND)})
		{
			aiming->animationPlayback().state() = busy;
			rejects("unsettled fire-ready and firing poses cannot transfer");
		}
		aiming->animationPlayback().state() = CROUCHING;
	}
	receiver.roster().team() = ENEMY_TEAM;
	rejects("non-player recipient rejects");
	receiver.roster().team() = OUR_TEAM;
	is_client = true;
	rejects("legacy networking cannot execute authoritative transfer");
	is_client = false;
	destination.ubMission = 1;
	rejects("noncanonical empty destination rejects");
	destination.initialize();
	MakeObject(destination, COMBAT_KNIFE, 1, 90);
	CHECK(!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, sentinel) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Discarded && destination.usItem == COMBAT_KNIFE,
		"occupied destination cannot exchange or merge");
	destination.initialize();
	source[0]->data.sObjectFlag ^= 1;
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded && !destination.exists(),
		"stale ordinary metadata discards retained command");
	source[0]->data.sObjectFlag ^= 1;
	TransferInventoryItemCommand stale = command;
	++stale.target.incarnation;
	CHECK(dispatch(stale).status == SimulationCommandDispatchStatus::Discarded && unchanged(), "stale target incarnation rejects");
	stale = command; ++stale.expectedWorldGeneration;
	CHECK(dispatch(stale).status == SimulationCommandDispatchStatus::Discarded && unchanged(), "stale world rejects");
	stale = command; ++stale.expectedTurnSerial;
	CHECK(dispatch(stale).status == SimulationCommandDispatchStatus::Discarded && unchanged(), "stale turn rejects");
	CHECK(!PrepareTransferInventoryItemCommand(from, from, BIGPOCK1POS, BIGPOCK2POS, sentinel) &&
		!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, VESTPOCKPOS, sentinel),
		"same actor and worn LBE destinations are unsupported");
	MakeObject(source[0]->attachments.front(), COMBAT_KNIFE, 1, 80);
	CHECK(!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, sentinel) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Discarded,
		"attached source graph is not an ordinary transferable stack");
	source[0]->attachments.front().initialize();
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
	CHECK(PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, command),
		"native default-LBE ordinary pocket transfer prepares");
	LBEPocketType[1].ItemCapacityPerSize[0] = 1;
	CHECK(!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, sentinel) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Discarded && unchanged(),
		"insufficient whole-stack pocket capacity cannot split or drop overflow");
	LBEPocketType[1].ItemCapacityPerSize[0] = 4;
	MakeObject(receiver.inventory()[CPACKPOCKPOS], DEFAULT_CPACK, 1, 100);
	receiver.inventory()[CPACKPOCKPOS][0]->data.lbe.bLBE = 1;
	CHECK(!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, sentinel) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Discarded && unchanged(),
		"active destination LBE graph is outside ordinary pocket transfer");
	receiver.inventory()[CPACKPOCKPOS].initialize();
	receiver.inventory().zipperFlag() = FALSE;
	CHECK(!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK4POS, sentinel),
		"closed receiver backpack is inaccessible in combat");
	receiver.inventory().zipperFlag() = TRUE;
	CHECK(PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK4POS, command),
		"crouched receiver with open backpack exposes a fitting ordinary pocket");
	receiver.animationPlayback().state() = STANDING;
	CHECK(!PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK4POS, sentinel) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Discarded && unchanged(),
		"standing receiver cannot access even an open backpack in combat");
	receiver.animationPlayback().state() = CROUCHING;
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	// These exact native objects remain opaque, untouched and unreplicated as
	// transferable payloads. A pocket transfer never depends on their graph.
	MakeObject(sender.inventory()[SECONDHANDPOS], KEY_1, 1, 97);
	MakeObject(receiver.inventory()[SECONDHANDPOS], KEY_1, 1, 81);
	const auto* senderOpaqueNode = &sender.inventory()[SECONDHANDPOS].objectStack.front();
	const auto* receiverOpaqueNode = &receiver.inventory()[SECONDHANDPOS].objectStack.front();
	CHECK(CaptureInventorySwapObjectState(from, SECONDHANDPOS) == 0 &&
		CaptureInventorySwapObjectState(to, SECONDHANDPOS) == 0 &&
		PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, BIGPOCK2POS, command),
		"normal opaque untouched offhands do not block a pocket-only transfer");
	const auto applied = dispatch(command);
	CHECK(applied.status == SimulationCommandDispatchStatus::Applied && game.commands().empty() &&
		!source.exists() && CaptureInventorySwapObjectState(from, BIGPOCK1POS) == 1 &&
		CaptureInventorySwapObjectState(to, BIGPOCK2POS) == objectFingerprint &&
		&destination.objectStack.front() == originalNode && destination.ubNumberOfObjects == 2 &&
		sender.actionPoints().current() == 18 && receiver.actionPoints().current() == 18,
		"one retained transfer preserves exact whole stack/list nodes and charges native 2 AP to each actor");
	const auto* immediateSender = GetJa2TacticalEntityDirectory().state(from);
	const auto* immediateReceiver = GetJa2TacticalEntityDirectory().state(to);
	CHECK(immediateSender && immediateReceiver && immediateSender->actionPoints == 18 &&
		immediateReceiver->actionPoints == 18,
		"both entity-directory AP states commit immediately, before any observer reconciliation");
	CHECK(&sender.inventory()[SECONDHANDPOS].objectStack.front() == senderOpaqueNode &&
		&receiver.inventory()[SECONDHANDPOS].objectStack.front() == receiverOpaqueNode &&
		sender.inventory()[SECONDHANDPOS][0]->data.objectStatus == 97 &&
		receiver.inventory()[SECONDHANDPOS][0]->data.objectStatus == 81,
		"untouched unsupported offhand objects retain exact storage and values");
	CHECK(dispatch(command).status == SimulationCommandDispatchStatus::Discarded &&
		sender.actionPoints().current() == 18 && receiver.actionPoints().current() == 18,
		"repeated retained command cannot duplicate the item or charge AP again");
	CHECK(PrepareTransferInventoryItemCommand(to, from, BIGPOCK2POS, BIGPOCK1POS, command) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		CaptureInventorySwapObjectState(from, BIGPOCK1POS) == objectFingerprint && !destination.exists(),
		"reverse transfer restores exact original stack, charging both actors again");
	sender.inventory()[SECONDHANDPOS].initialize();
	receiver.inventory()[SECONDHANDPOS].initialize();
	MakeObject(source, FIRSTAIDKIT, 1, 73);
	CHECK(PrepareTransferInventoryItemCommand(from, to, BIGPOCK1POS, HANDPOS, command) &&
		command.handStateFingerprint != 0 && command.targetOffhandStateFingerprint != 0 &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		!source.exists() && receiver.inventory()[HANDPOS].usItem == FIRSTAIDKIT &&
		IsJa2TacticalWorldIntegrityValid(), "hand-involved transfer retains complete ordinary hand proofs and native equipment continuation");
	immediateReceiver = GetJa2TacticalEntityDirectory().state(to);
	CHECK(immediateReceiver && immediateReceiver->loadout.primaryHand.item == FIRSTAIDKIT &&
		immediateReceiver->handMedicalKitPoints == 73 &&
		immediateReceiver->actionPoints == receiver.actionPoints().current(),
		"receiver equipped medical loadout, kit points and AP synchronize at dispatch completion");

	// Verify real held-equipment consequences on BOTH actors, including palette
	// updates. These bounded in-memory definitions do not touch installed data.
	receiver.inventory()[HANDPOS].initialize();
	gGameOptions.ubInventorySystem = INVENTORY_NEW;
	Item[SW38].usItemClass = IC_GUN;
	Item[SW38].ubPerPocket = 1;
	Item[SW38].usItemFlag &= ~ITEM_twohanded;
	Weapon[SW38].ubShotsPerBurst = 0;
	Weapon[SW38].NoSemiAuto = FALSE;
	Item[SW38].camobonus = 5;
	Item[SW38].urbanCamobonus = 7;
	Item[SW38].desertCamobonus = 9;
	Item[SW38].snowCamobonus = 11;
	gGameExternalOptions.bCamoKitArea = 90;
	MakeObject(sender.inventory()[HANDPOS], SW38, 1, 94);
	sender.inventory()[HANDPOS][0]->data.gun.ubGunShotsLeft = 6;
	sender.inventory()[HANDPOS][0]->data.gun.usGunAmmoItem = CLIP38_6;
	sender.inventory()[HANDPOS][0]->data.gun.bGunAmmoStatus = 100;
	sender.inventory()[HANDPOS][0]->data.gun.ubGunState = GS_CARTRIDGE_IN_CHAMBER;
	SGPPaletteEntry palette[256]{};
	palette[17].peRed = 87;
	palette[17].peGreen = 43;
	palette[17].peBlue = 19;
	video.pPaletteEntry = palette;
	gubAnimSurfaceIndex[REGMALE][STANDING] = 0;
	gubAnimSurfaceItemSubIndex[REGMALE][STANDING] = INVALID_ANIMATION;
	for (auto* actor : {&sender, &receiver})
	{
		actor->camouflage().jungleApplied() = 1;
		actor->camouflage().urbanApplied() = 2;
		actor->camouflage().desertApplied() = 3;
		actor->camouflage().snowApplied() = 4;
		actor->actionPoints().current() = 100;
		CHECK(ApplyEquipmentBonusesChecked(*actor), "initial native held-equipment bonuses refresh");
	}
	const auto wornIs = [](const TacticalActor& actor, bool held) {
		return actor.camouflage().jungleWorn() == (held ? 5 : 0) &&
			actor.camouflage().urbanWorn() == (held ? 7 : 0) &&
			actor.camouflage().desertWorn() == (held ? 9 : 0) &&
			actor.camouflage().snowWorn() == (held ? 10 : 0) &&
			actor.camouflage().jungleApplied() == 1 && actor.camouflage().urbanApplied() == 2 &&
			actor.camouflage().desertApplied() == 3 && actor.camouflage().snowApplied() == 4;
	};
	const auto gunFingerprint = CaptureInventorySwapObjectState(from, HANDPOS);
	CHECK(wornIs(sender, true) && wornIs(receiver, false) && gunFingerprint != 0 &&
		sender.palette().base8() && sender.palette().base8()[17].peRed == 87,
		"donor's held gun contributes actual native camo and a registered palette");
	CHECK(PrepareTransferInventoryItemCommand(from, to, HANDPOS, HANDPOS, command) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		!sender.inventory()[HANDPOS].exists() && CaptureInventorySwapObjectState(to, HANDPOS) == gunFingerprint &&
		wornIs(sender, false) && wornIs(receiver, true) &&
		sender.actionPoints().current() == 98 && receiver.actionPoints().current() == 98 &&
		receiver.palette().base8() && receiver.palette().base8()[17].peRed == 87,
		"hand-to-hand transfer removes donor and applies recipient worn camo, preserves object and charges both AP");
	CHECK(PrepareTransferInventoryItemCommand(to, from, HANDPOS, HANDPOS, command) &&
		dispatch(command).status == SimulationCommandDispatchStatus::Applied &&
		!receiver.inventory()[HANDPOS].exists() && CaptureInventorySwapObjectState(from, HANDPOS) == gunFingerprint &&
		wornIs(sender, true) && wornIs(receiver, false) &&
		sender.actionPoints().current() == 96 && receiver.actionPoints().current() == 96 &&
		IsJa2TacticalWorldIntegrityValid(),
		"hand transfer return restores exact donor gun/camo and clears recipient bonuses without changing applied camo");
	cleanup();
	std::printf("authoritative inventory transfer: %s\n", failures ? "FAIL" : "PASS");
	return failures ? 1 : 0;
}
