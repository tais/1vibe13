// Data-free native regression for the checked equipment-dispatch extraction.
// No map, item XML, animation assets, or installed executable are required.
// This deliberately does NOT claim a retained-compatible inventory command.
#include "Animation Data.h"
#include "GameContext.h"
#include "Items.h"
#include "Overhead.h"
#include "Simulation Commands.h"
#include "SoldierRepository.h"
#include "Soldier Profile Constants.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "Weapons.h"

#include <cstdio>
#include <cstdlib>

// Application-shell storage normally supplied by sgp.cpp, as in the existing
// headless suites. This executable owns its own main and opens no native UI.
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
#define CHECK(condition, message) do { \
	if (!(condition)) { \
		std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, message); \
		++failures; \
	} \
} while (false)

bool Carries(const TacticalActor& actor, UINT16 left, UINT16 right)
{
	return actor.inventory()[BIGPOCK1POS].usItem == left &&
		actor.inventory()[BIGPOCK2POS].usItem == right;
}

}

int main()
{
	GameContext& game = GetGameContext();
	const bool running = game.beginInitialization() &&
		game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning();
	CHECK(running, "fixture starts the bound native command runtime");
	if (!running) return 1;
	Ja2SoldierRepository& repository = GetJa2SoldierRepository();
	repository.initializeSlots();
	ResetJa2TacticalActorRosters();
	TacticalActor& actor = *repository.resolve(0);
	actor.identity().id() = SoldierID{0};
	actor.identity().incarnation() = 101;
	actor.identity().bodyType() = REGMALE;
	actor.identity().profile() = NO_PROFILE;
	actor.roster().active() = TRUE;
	actor.roster().inSector() = FALSE; // map-equipment path needs no animation assets
	actor.roster().team() = OUR_TEAM;
	actor.fireControl().selectBarrelMode(1);
	actor.actionPoints().current() = 19;
	CHECK(AdoptJa2TacticalEntity(actor), "fixture has an exact native actor identity");
	gMAXITEMS_READ = FIRSTAIDKIT + 1;
	Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
	Item[COMBAT_KNIFE].usItemClass = IC_BLADE;
	OBJECTTYPE& kit = actor.inventory()[BIGPOCK1POS];
	kit.usItem = FIRSTAIDKIT;
	kit.ubNumberOfObjects = 2;
	kit.ubMission = 7;
	kit.fFlags = OBJECT_MODIFIED;
	kit.objectStack.resize(2);
	kit[0]->data.objectStatus = 73;
	kit[0]->data.sRepairThreshold = 79;
	kit[0]->data.sObjectFlag = 0x1020304050607080ULL;
	kit[1]->data.objectStatus = 41;
	OBJECTTYPE& knife = actor.inventory()[BIGPOCK2POS];
	knife.usItem = COMBAT_KNIFE;
	knife.ubNumberOfObjects = 1;
	knife[0]->data.objectStatus = 88;
	const auto originalConfiguration = CaptureTacticalWeaponConfiguration(actor);
	std::uint64_t frame = 1;
	BeginSimulationCommandFrameBudget(frame, 16);
	TacticalWeaponConfigurationResult resolved;
	bool handRefresh = true;
	CHECK(!ResolveEquipmentTacticalWeaponConfiguration(actor, BIGPOCK1POS,
		FIRSTAIDKIT, FIRSTAIDKIT, resolved, handRefresh) && !handRefresh,
		"ordinary pocket reconciliation exercises the existing resolver-false fallback");
	const auto unchanged = TryHandleTacticalEffectsOfEquipmentChange(
		&actor, BIGPOCK1POS, FIRSTAIDKIT, FIRSTAIDKIT);
	CHECK(unchanged.status == SimulationCommandDispatchStatus::Applied && unchanged.submitted &&
		game.commands().empty() && CaptureTacticalWeaponConfiguration(actor) == originalConfiguration,
		"checked helper still dispatches and applies an unchanged native configuration");

	SimulationCommandDispatchResult outcomes[2];
	SwapObjs(&actor, BIGPOCK1POS, BIGPOCK2POS, TRUE, outcomes);
	CHECK(outcomes[0].status == SimulationCommandDispatchStatus::Applied &&
		outcomes[1].status == SimulationCommandDispatchStatus::Applied && game.commands().empty() &&
		outcomes[0].submitted && outcomes[1].submitted &&
		outcomes[0].sequence < outcomes[1].sequence &&
		Carries(actor, COMBAT_KNIFE, FIRSTAIDKIT),
		"checked actor-aware swap reports both actual native equipment completions");
	const OBJECTTYPE& moved = actor.inventory()[BIGPOCK2POS];
	CHECK(moved.ubNumberOfObjects == 2 && moved.objectStack.size() == 2 &&
		moved.ubMission == 7 && moved.fFlags == OBJECT_MODIFIED &&
		moved[0]->data.objectStatus == 73 && moved[1]->data.objectStatus == 41 &&
		moved[0]->data.sRepairThreshold == 79 && moved[0]->data.sObjectFlag == 0x1020304050607080ULL &&
		actor.actionPoints().current() == 19,
		"extraction retains native complete-stack copy behavior without inventing an AP charge");
	// The composed game owns its immutable completion sink. Only this bounded
	// fixture uses the diagnostic journal to inspect the legacy void wrapper;
	// production correctness must use the checked dispatch results, not records.
	const std::size_t beforeLegacy = game.commandJournal().size();
	const std::uint64_t beforeDropped = game.commandJournal().droppedCount();
	SwapObjs(&actor, BIGPOCK1POS, BIGPOCK2POS, TRUE);
	const auto legacyRecords = game.commandJournal().snapshot();
	const bool completeLegacyRecords = legacyRecords.size() == beforeLegacy + 2 &&
		game.commandJournal().droppedCount() == beforeDropped;
	const auto* legacyLeft = completeLegacyRecords ?
		std::get_if<ApplyWeaponConfigurationCommand>(&legacyRecords[beforeLegacy].command) : nullptr;
	const auto* legacyRight = completeLegacyRecords ?
		std::get_if<ApplyWeaponConfigurationCommand>(&legacyRecords[beforeLegacy + 1].command) : nullptr;
	CHECK(legacyLeft && legacyRight &&
		legacyRecords[beforeLegacy].status == CommandJournalStatus::Applied &&
		legacyRecords[beforeLegacy + 1].status == CommandJournalStatus::Applied &&
		legacyRecords[beforeLegacy].sequence < legacyRecords[beforeLegacy + 1].sequence &&
		legacyLeft->inventoryPosition == BIGPOCK1POS &&
		legacyLeft->previousItem == COMBAT_KNIFE && legacyLeft->changedItem == FIRSTAIDKIT &&
		legacyRight->inventoryPosition == BIGPOCK2POS &&
		legacyRight->previousItem == FIRSTAIDKIT && legacyRight->changedItem == COMBAT_KNIFE,
		"legacy void swap still dispatches exactly left then right with correct item identities");
	HandleTacticalEffectsOfEquipmentChange(&actor, BIGPOCK1POS, FIRSTAIDKIT, FIRSTAIDKIT);
	CHECK(Carries(actor, FIRSTAIDKIT, COMBAT_KNIFE) && game.commands().empty() &&
		CaptureTacticalWeaponConfiguration(actor) == originalConfiguration && actor.actionPoints().current() == 19,
		"existing void wrappers retain their native swap and no-op reconciliation behavior");

	const auto discarded = TryHandleTacticalEffectsOfEquipmentChange(
		&actor, BIGPOCK1POS, FIRSTAIDKIT, NOTHING);
	CHECK(discarded.status == SimulationCommandDispatchStatus::Discarded && !discarded &&
		!discarded.accepted() && game.commands().empty() &&
		Carries(actor, FIRSTAIDKIT, COMBAT_KNIFE) &&
		CaptureTacticalWeaponConfiguration(actor) == originalConfiguration,
		"mismatched changed-item proof is reported as Discarded, not Applied");

	BeginSimulationCommandFrameBudget(++frame, 0);
	const auto deferred = TryHandleTacticalEffectsOfEquipmentChange(
		&actor, BIGPOCK1POS, FIRSTAIDKIT, FIRSTAIDKIT);
	CHECK(deferred.status == SimulationCommandDispatchStatus::RetryDeferred && deferred.submitted &&
		!deferred && deferred.accepted() && game.commands().size() == 1,
		"exhausted budget remains accepted-but-deferred and cannot be mistaken for completion");
	BeginSimulationCommandFrameBudget(++frame, 16);
	const auto drained = ExecuteSimulationCommandsThrough(
		game.runtime().simulationTicks().completedTickSequence(), 16);
	CHECK(drained.applied == 1 && drained.discarded == 0 && game.commands().empty(),
		"existing safe-frame drain later applies exactly the one retained continuation");

	BeginSimulationCommandFrameBudget(++frame, 1);
	SwapObjs(&actor, BIGPOCK1POS, BIGPOCK2POS, TRUE, outcomes);
	CHECK(outcomes[0].status == SimulationCommandDispatchStatus::Applied &&
		outcomes[1].status == SimulationCommandDispatchStatus::RetryDeferred &&
		!outcomes[1] && outcomes[1].accepted() && game.commands().size() == 1 &&
		Carries(actor, COMBAT_KNIFE, FIRSTAIDKIT),
		"checked copy-swap exposes partial equipment completion without claiming rollback");
	BeginSimulationCommandFrameBudget(++frame, 16);
	const auto second = ExecuteSimulationCommandsThrough(
		game.runtime().simulationTicks().completedTickSequence(), 16);
	CHECK(second.applied == 1 && game.commands().empty(),
		"the second equipment continuation drains once without replaying the object swap");

	const std::size_t beforeSkipped = game.commandJournal().size();
	SwapObjs(&actor, BIGPOCK1POS, BIGPOCK2POS, FALSE, outcomes);
	CHECK(outcomes[0].status == SimulationCommandDispatchStatus::SubmissionFailure &&
		outcomes[1].status == SimulationCommandDispatchStatus::SubmissionFailure &&
		!outcomes[0].submitted && !outcomes[1].submitted && game.commands().empty() &&
		game.commandJournal().size() == beforeSkipped && Carries(actor, FIRSTAIDKIT, COMBAT_KNIFE),
		"nonpermanent swap initializes both unsubmitted outcomes and skips both continuations");
	actor.identity().bodyType() = TANK_NW;
	SwapObjs(&actor, BIGPOCK1POS, BIGPOCK2POS, TRUE, outcomes);
	CHECK(outcomes[0].status == SimulationCommandDispatchStatus::SubmissionFailure &&
		outcomes[1].status == SimulationCommandDispatchStatus::SubmissionFailure &&
		!outcomes[0].submitted && !outcomes[1].submitted && game.commands().empty() &&
		game.commandJournal().size() == beforeSkipped && Carries(actor, COMBAT_KNIFE, FIRSTAIDKIT),
		"armed-vehicle exception still swaps objects without an equipment continuation");
	actor.identity().bodyType() = REGMALE;

	ReleaseJa2TacticalEntity(actor);
	const auto invalid = TryHandleTacticalEffectsOfEquipmentChange(
		&actor, BIGPOCK1POS, COMBAT_KNIFE, COMBAT_KNIFE);
	CHECK(invalid.status == SimulationCommandDispatchStatus::InvalidActor && !invalid.submitted &&
		game.commands().empty() && Carries(actor, COMBAT_KNIFE, FIRSTAIDKIT),
		"released native identity is surfaced as an unsubmitted failure");
	actor.roster().active() = FALSE;
	std::printf("equipment dispatch extraction: %s\n", failures == 0 ? "PASS" : "FAIL");
	return failures == 0 ? 0 : 1;
}
