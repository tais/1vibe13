// Actual native carried inventory projection; no installed assets or client simulation.
#include "CoopInventoryAuthority.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Items.h"
#include "Overhead.h"
#include "Simulation Commands.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "connect.h"

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

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
int allocationsUntilFailure = -1;
unsigned injectedFailures = 0;
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
	if (allocationsUntilFailure == 0)
	{
		allocationsUntilFailure = -1;
		++injectedFailures;
		throw std::bad_alloc();
	}
	if (allocationsUntilFailure > 0) --allocationsUntilFailure;
	if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
	throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

int main()
{
	using Status = CoopSession::CoopInventoryStatusKind;
	using Support = CoopSession::CoopInventorySlotSupport;
	GameContext& game = GetGameContext();
	const bool running = game.beginInitialization() &&
		game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning();
	CHECK(running, "native inventory fixture starts the bound runtime");
	if (!running) return 1;
	auto& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	ResetJa2TacticalActorRosters();
	ResetJa2TacticalInterruptForNewWorld();
	NotifyJa2TacticalWorldLoaded(1);
	RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
	gbPlayerNum = OUR_TEAM;
	is_networked = is_client = is_server = false;
	gGameOptions.ubInventorySystem = INVENTORY_OLD;
	TacticalActor& actor = *repository.resolve(0);
	actor.identity().id() = SoldierID{0};
	actor.identity().incarnation() = 101;
	actor.roster().active() = actor.roster().inSector() = TRUE;
	actor.roster().team() = OUR_TEAM;
	actor.actionPoints().current() = 0; // Projection does not require action admission.
	CHECK(AdoptJa2TacticalEntity(actor), "exact native actor adopted");
	const TacticalEntityId id = GetJa2TacticalEntityId(actor);
	const auto world = GetJa2TacticalWorldAdapter().liveTurnIdentity();
	gMAXITEMS_READ = DEFAULT_BPACK + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
	Item[COMBAT_KNIFE].usItemClass = IC_BLADE;
	Item[LOCKSMITHKIT].usItemClass = IC_KIT;
	Item[SW38].usItemClass = IC_GUN;
	Item[CLIP38_6].usItemClass = IC_AMMO;
	Item[DEFAULT_VEST].usItemClass = IC_LBEGEAR;
	Item[CANTEEN].usItemClass = IC_MISC;
	OBJECTTYPE& pocket = actor.inventory()[BIGPOCK1POS];
	MakeObject(pocket, FIRSTAIDKIT, 1, 73);
	pocket.ubMission = 7;
	pocket.fFlags = OBJECT_MODIFIED;
	pocket[0]->data.sRepairThreshold = 79;
	pocket[0]->data.sObjectFlag = 0x1020304050607080ULL;
	pocket[0]->data.bTemperature = 12.5f;
	pocket[0]->attachments.resize(4); // Native NAS empty placeholders.

	Ja2CoopInventoryAuthority authority;
	CoopSession::CoopOwnerInventorySnapshot view;
	CHECK(authority.capture(id, world.worldGeneration, view) &&
		view.inventoryRevision == 1 && view.slots.size() == NUM_INV_SLOTS &&
		view.actor == id && view.worldGeneration == world.worldGeneration &&
		view.slots[BIGPOCK1POS].item == FIRSTAIDKIT &&
		view.slots[BIGPOCK1POS].support == Support::OrdinarySwappable &&
		view.slots[BIGPOCK1POS].statusKind == Status::MedicalKitPoints &&
		view.slots[BIGPOCK1POS].resourceTotal == 73 &&
		view.groundGrid == -1 && view.groundLevel == -1 && view.groundItems.empty() && view.nearbyLoot.empty(),
		"capture projects dense carried slots while leaving unavailable ground context explicit");
	CHECK(authority.capture(id, world.worldGeneration, view) && view.inventoryRevision == 1,
		"unchanged capture retains its private inventory revision");
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(authority.capture(id, world.worldGeneration, view) && view.inventoryRevision == 2 &&
		view.slots[BIGPOCK1POS].firstCondition == 73,
		"private metadata changes advance revision despite identical display summary");
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(authority.capture(id, world.worldGeneration, view) && view.inventoryRevision == 3,
		"value restoration advances revision instead of reusing a previous token");
	const auto retained = view;
	CHECK(!authority.capture(id, world.worldGeneration + 1, view) && view == retained,
		"wrong-world capture preserves previous caller output");
	actor.inventory()[BIGPOCK3POS].ubMission = 1;
	CHECK(!authority.capture(id, world.worldGeneration, view) && view == retained,
		"noncanonical empty storage is never advertised as a usable empty slot");
	actor.inventory()[BIGPOCK3POS].initialize();
	pocket.ubNumberOfObjects += 1;
	CHECK(!authority.capture(id, world.worldGeneration, view) && view == retained,
		"malformed stack count cannot manufacture a new inventory revision");
	pocket.ubNumberOfObjects -= 1;
	CHECK(authority.capture(id, world.worldGeneration, view) && view.inventoryRevision == 3,
		"rejected captures did not commit hidden revision changes");


	// Fail both initial candidate storage and the return-copy allocation. No
	// committed token may change until a complete replacement can be returned.
	pocket[0]->data.sObjectFlag ^= 1;
	for (int failurePoint : {0, 1})
	{
		allocationsUntilFailure = failurePoint;
		const bool captured = authority.capture(id, world.worldGeneration, view);
		allocationsUntilFailure = -1;
		CHECK(!captured && view == retained, "allocation failure leaves caller output intact");
	}
	CHECK(injectedFailures == 2, "both capture allocations fail inside native projection");
	pocket[0]->data.sObjectFlag ^= 1;
	CHECK(authority.capture(id, world.worldGeneration, view) && view.inventoryRevision == 3,
		"failed changed-state captures never commit an unseen revision");

	const auto ordinary = CaptureInventorySwapObjectState(id, BIGPOCK1POS);
	CHECK(ordinary != 0 && CaptureInventorySwapObjectState({id.slot, id.incarnation + 1}, BIGPOCK1POS) == 0 &&
		CaptureInventorySwapObjectState(id, UINT8_MAX) == 0,
		"object proof requires exact actor identity and an existing slot");
	pocket[0]->data.bFiller = 19.5f;
	CHECK(CaptureInventorySwapObjectState(id, BIGPOCK1POS) == ordinary,
		"unused native filler does not cause stale-state changes");
	pocket[0]->data.bTrap = 1;
	CHECK(CaptureInventorySwapObjectState(id, BIGPOCK1POS) == 0 &&
		authority.capture(id, world.worldGeneration, view) &&
		view.slots[BIGPOCK1POS].support == Support::UnsupportedComplex &&
		view.slots[BIGPOCK1POS].statusKind == Status::Unknown && view.slots[BIGPOCK1POS].resourceTotal == 0,
		"trapped stacks stay visible as unsupported without invented metrics");
	pocket[0]->data.bTrap = 0;
	pocket[0]->data.bTemperature = std::numeric_limits<float>::quiet_NaN();
	CHECK(CaptureInventorySwapObjectState(id, BIGPOCK1POS) == 0, "nonfinite object state is unsupported");
	pocket[0]->data.bTemperature = 12.5f;
	MakeObject(pocket[0]->attachments.front(), COMBAT_KNIFE, 1, 80);
	CHECK(CaptureInventorySwapObjectState(id, BIGPOCK1POS) == 0, "actual attachment graphs are unsupported");
	pocket[0]->attachments.front().initialize();
	pocket[0]->data.gun.ubGunAmmoType = UINT8_MAX;
	CHECK(CaptureInventorySwapObjectState(id, BIGPOCK1POS) == 0, "active LBE union alias is unsupported");
	pocket[0]->data.gun.ubGunAmmoType = 0;
	CHECK(CaptureInventorySwapObjectState(id, BIGPOCK1POS) == ordinary,
		"ordinary proof restores after all unsupported fields are restored");

	OBJECTTYPE& misc = actor.inventory()[BIGPOCK2POS];
	MakeObject(misc, CANTEEN, 1, 80);
	CHECK(authority.capture(id, world.worldGeneration, view) &&
		view.slots[BIGPOCK2POS].support == Support::OrdinarySwappable &&
		view.slots[BIGPOCK2POS].statusKind == Status::Unknown && view.slots[BIGPOCK2POS].resourceTotal == 0,
		"ordinary canteen is movable without claiming its status represents condition or known resource points");
	for (UINT16 special : {UINT16(SWITCH), UINT16(ACTION_ITEM), UINT16(OWNERSHIP)})
	{
		Item[special].usItemClass = IC_MISC;
		MakeObject(misc, special, 1, 80);
		CHECK(CaptureInventorySwapObjectState(id, BIGPOCK2POS) == 0,
			"native control/ownership objects never become ordinary movable misc items");
	}
	misc.initialize();

	OBJECTTYPE& hand = actor.inventory()[HANDPOS];
	OBJECTTYPE& offhand = actor.inventory()[SECONDHANDPOS];
	OBJECTTYPE& ammunition = actor.inventory()[SMALLPOCK1POS];
	OBJECTTYPE& medical = actor.inventory()[SMALLPOCK2POS];
	MakeObject(hand, SW38, 1, 94);
	hand[0]->data.gun.ubGunAmmoType = 1;
	hand[0]->data.gun.ubGunShotsLeft = 6;
	hand[0]->data.gun.usGunAmmoItem = CLIP38_6;
	hand[0]->data.gun.bGunAmmoStatus = 100;
	MakeObject(offhand, LOCKSMITHKIT, 1, 99);
	MakeObject(ammunition, CLIP38_6, 2, 6);
	ammunition[1]->data.ubShotsLeft = 3;
	ammunition[0]->attachments.resize(1);
	MakeObject(medical, FIRSTAIDKIT, 3, 0);
	medical[0]->data.objectStatus = 73;
	medical[1]->data.objectStatus = 40;
	MakeObject(actor.inventory()[VESTPOCKPOS], DEFAULT_VEST, 1, 97);
	Ja2CoopInventoryAuthority metricsAuthority;
	CoopSession::CoopOwnerInventorySnapshot metrics;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics.inventoryRevision == 1 &&
		metrics.slots[HANDPOS].statusKind == Status::Condition && metrics.slots[HANDPOS].firstCondition == 94 &&
		metrics.slots[HANDPOS].resourceTotal == 0 &&
		metrics.slots[SECONDHANDPOS].statusKind == Status::ToolKitPoints && metrics.slots[SECONDHANDPOS].resourceTotal == 99 &&
		metrics.slots[SMALLPOCK1POS].statusKind == Status::AmmoRounds && metrics.slots[SMALLPOCK1POS].resourceTotal == 9 &&
		metrics.slots[SMALLPOCK2POS].statusKind == Status::MedicalKitPoints && metrics.slots[SMALLPOCK2POS].resourceTotal == 113 &&
		metrics.slots[VESTPOCKPOS].support == Support::UnsupportedComplex &&
		metrics.slots[VESTPOCKPOS].statusKind == Status::Unknown && metrics.slots[VESTPOCKPOS].resourceTotal == 0,
		"metrics distinguish gun condition, tool points and full stack rounds/medical points without inspecting LBE");
	ammunition[1]->data.ubShotsLeft = 2;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics.inventoryRevision == 2 &&
		metrics.slots[SMALLPOCK1POS].firstCondition == 6 && metrics.slots[SMALLPOCK1POS].resourceTotal == 8,
		"non-first ammunition use updates aggregate and private revision");
	medical[2]->data.objectStatus = 1;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics.inventoryRevision == 3 &&
		metrics.slots[SMALLPOCK2POS].firstCondition == 73 && metrics.slots[SMALLPOCK2POS].resourceTotal == 114,
		"non-first medical points change is visible without changing the first item or object count");
	medical[0]->data.objectStatus = 0;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) &&
		metrics.slots[SMALLPOCK2POS].firstCondition == 0 && metrics.slots[SMALLPOCK2POS].resourceTotal == 41,
		"a depleted first kit remains distinct from remaining points in the stack");
	ammunition[0]->data.ubShotsLeft = 40000;
	ammunition[1]->data.ubShotsLeft = 65535;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) &&
		static_cast<UINT16>(metrics.slots[SMALLPOCK1POS].firstCondition) == 40000 &&
		metrics.slots[SMALLPOCK1POS].resourceTotal == 105535,
		"unsigned native ammunition quantities do not turn into negative totals");
	MakeObject(ammunition[0]->attachments.front(), COMBAT_KNIFE, 1, 80);
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) &&
		metrics.slots[SMALLPOCK1POS].support == Support::UnsupportedComplex &&
		metrics.slots[SMALLPOCK1POS].statusKind == Status::Unknown && metrics.slots[SMALLPOCK1POS].resourceTotal == 0,
		"attachment graphs suppress formerly known metrics instead of publishing partial totals");
	MakeObject(ammunition, CLIP38_6, 255, 0);
	for (auto& object : ammunition.objectStack) object.data.ubShotsLeft = 65535;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) &&
		metrics.slots[SMALLPOCK1POS].resourceTotal == 16711425 && metrics.slots[SMALLPOCK1POS].count == 255,
		"largest native stack sums exactly without narrowing or truncation");
	const auto priorRevision = metrics.inventoryRevision;
	Item[LOCKSMITHKIT].usItemClass = IC_MEDKIT;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics.inventoryRevision == priorRevision + 1 &&
		metrics.slots[SECONDHANDPOS].statusKind == Status::MedicalKitPoints,
		"authority-selected semantic changes advance revision even with identical object bytes");
	gGameOptions.ubInventorySystem = INVENTORY_NEW;
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics.inventoryRevision == priorRevision + 2 &&
		metrics.usesNewInventory, "inventory mode is included in the replacement revision");
	const auto gunFingerprint = CaptureInventorySwapObjectState(id, HANDPOS);
	hand[0]->data.gun.ubGunShotsLeft -= 1;
	CHECK(CaptureInventorySwapObjectState(id, HANDPOS) != gunFingerprint &&
		metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics.inventoryRevision == priorRevision + 3,
		"loaded gun ammunition changes invalidate the inventory token even when display condition is unchanged");


	// The authority produces native values only. Session/baseline/owner fields
	// remain for the authenticated transport to stamp before the merged codec.
	CHECK(metrics.sessionEpoch == 0 && metrics.baselineId == 0 && metrics.owner == CoopSession::PeerIdentity{},
		"native projection does not claim transport or ownership identity");
	const auto nativeMetrics = metrics;
	metrics.sessionEpoch = 17;
	metrics.baselineId = 23;
	metrics.owner[0] = 9;
	std::vector<std::uint8_t> bytes;
	CoopSession::CoopOwnerInventorySnapshot decoded;
	CHECK(CoopSession::EncodeCoopOwnerInventorySnapshot(metrics, bytes) == CoopSession::CoopInventoryCodecResult::Success &&
		CoopSession::DecodeCoopOwnerInventorySnapshot(bytes.data(), bytes.size(), decoded) == CoopSession::CoopInventoryCodecResult::Success &&
		decoded == metrics, "actual native carried projection survives the bounded owner codec unchanged");
	CHECK(metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics == nativeMetrics,
		"recapture replaces prior caller transport stamps with native-only contents");

	const auto beforeRejectedActor = metrics;
	actor.roster().inSector() = FALSE;
	CHECK(!metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics == beforeRejectedActor,
		"departed actors cannot publish new carried inventory");
	actor.roster().inSector() = TRUE;
	actor.roster().team() = ENEMY_TEAM;
	CHECK(!metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics == beforeRejectedActor,
		"non-player actors cannot publish owner inventory");
	actor.roster().team() = OUR_TEAM;
	CHECK(ReleaseJa2TacticalEntity(actor), "retire original actor identity");
	actor.identity().incarnation() = 102;
	CHECK(AdoptJa2TacticalEntity(actor), "adopt replacement actor in the same native slot");
	const auto replacement = GetJa2TacticalEntityId(actor);
	CHECK(!metricsAuthority.capture(id, world.worldGeneration, metrics) && metrics == beforeRejectedActor &&
		metricsAuthority.capture(replacement, world.worldGeneration, metrics) && metrics.inventoryRevision == 1 &&
		metrics.actor == replacement, "replacement actor resets its ledger without reviving stale identity");
	NotifyJa2TacticalWorldLoaded(2);
	CHECK(metricsAuthority.capture(replacement, 2, metrics) && metrics.inventoryRevision == 1 && metrics.worldGeneration == 2,
		"new world resets all carried inventory revision ledgers");
	MarkJa2TacticalWorldIntegrityFailure();
	const auto beforeIntegrityFailure = metrics;
	CHECK(!metricsAuthority.capture(replacement, 2, metrics) && metrics == beforeIntegrityFailure,
		"failed native world integrity blocks owner publication");
	const auto beforeUnload = metrics;
	NotifyJa2TacticalWorldUnloaded();
	CHECK(!metricsAuthority.capture(replacement, 2, metrics) && metrics == beforeUnload,
		"unloaded worlds reject capture without changing caller output");
	CHECK(game.commands().empty() && game.commandJournal().size() == 0 && actor.actionPoints().current() == 0,
		"observation creates no command, journal entry or AP mutation");
	std::printf("native owner inventory capture: %s\n", failures == 0 ? "PASS" : "FAIL");
	return failures ? 1 : 0;
}
