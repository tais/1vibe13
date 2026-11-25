// Opt-in, disposable installed-data fixture. This target (never a shipped app)
// seeds two hired actors and a hostile A10 encounter after normal engine boot.
// Native-only modes test preparation/entry/placement. The travel mode leaves
// an A9 squad in the shipping runtime for the two-client campaign input runner.
// The fixture never qualifies physical input or full campaign completion.
#include "types.h"
#include "DedicatedCoopArrival.h"
#include "DedicatedCoopRuntime.h"
#include "DedicatedCoopMissionBootstrap.h"
#include "Exit Grids.h"
#include "TacticalActorInteractions.h"
#include "NPC.h"
#include "interface Dialogue.h"
#include "Isometric Utils.h"
#include "DedicatedServerOptions.h"
#include "DedicatedCoopCampaignGroups.h"
#include "DedicatedCoopCampaignEconomy.h"
#include "DedicatedCoopBattleNotice.h"
#include "DedicatedCoopSurrender.h"
#include "CampaignEventAdapter.h"
#include "CampaignClockAdapter.h"
#include "Game Clock.h"
#include "Game Event Hook.h"
#include "Game Events.h"
#include "GameSettings.h"
#include "PreBattle Interface.h"
#include "Strategic Movement.h"
#include "StrategicGroupHost.h"
#include "Strategic AI.h"
#include "strategic.h"
#include "strategicmap.h"
#include "Squads.h"
#include "Assignments.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "SoldierRepository.h"
#include "TacticalActorStateFlags.h"
#include "Interface Panels.h"
#include "Dialogue Control.h"
#include "Strategic Status.h"
#include "Quests.h"
#include "screenids.h"
#include "Soldier Profile.h"
#include "Soldier Ani.h"
#include "Queen Command.h"
#include "Tactical Placement GUI.h"
#include "Overhead.h"
#include "Soldier Profile Constants.h"
#include "Isometric Utils.h"
#include "Map Information.h"
#include "Grid Direction.h"
#include "gameloop.h"
#include "random.h"
#include "FileMan.h"
#include <Engine/Core/SimulationRandom.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

extern BOOLEAN gfProgramIsRunning, gfDedicatedServerProcessFailed;
extern BOOLEAN fInMapMode, gfProcessingGameEvents;
// NPC.cpp's quote table and loader (not exported by NPC.h); fixture-only reads.
extern NPCQuoteInfo* gpNPCQuoteInfoArray[NUM_PROFILES];
BOOLEAN EnsureQuoteFileLoaded(UINT8 ubNPC);

namespace
{
CoopSession::CoopCampaignGroups preparedObservation;
bool observing = false;
bool cohortSeeded = false;
enum class ControlledBattle { None, Defeat, Capture, Victory, StarterVictory };
ControlledBattle controlledBattle = ControlledBattle::None;
bool controlledBattleSeeded = false;
std::chrono::steady_clock::time_point cohortSeedTime;
void Finish(bool passed, const char* detail)
{
	std::printf("[co-op entry test] %s: %s\n", passed ? "PASS" : "FAIL", detail);
	std::fflush(stdout);
	if (!passed) gfDedicatedServerProcessFailed = TRUE;
	gfProgramIsRunning = FALSE;
}

void SeedColdCohort(bool capture)
{
	// Deliberately separate persistence qualification from battle qualification.
	// This test-only executable changes a disposable, already-qualified hire
	// checkpoint through native death cleanup/capture. It never fabricates a
	// combat result, action receipt, victory, sector loss or battle history row.
	CoopSession::CoopCampaignEconomy before;
	CoopSession::CoopCampaignGroups groups;
	if (IsJa2TacticalWorldLoaded() || DedicatedCoopArrivalDecisionPending() || !GamePaused() ||
		CaptureDedicatedCoopCampaignEconomy(before) || CaptureDedicatedCoopCampaignGroups(groups) ||
		before.rosterCount != 5 || before.mercenaryCount != 5 || groups.groupCount != 1 || groups.memberCount != 4 ||
		groups.groups[0].x != 9 || groups.groups[0].y != 1 || groups.groups[0].betweenSectors ||
		gStrategicStatus.ubNumCapturedForRescue || gubQuest[QUEST_HELD_IN_ALMA] != QUESTNOTSTARTED ||
		InspectDedicatedCoopStarterCampaign() != DedicatedCoopStarterCampaignState::EstablishedStrategicCold)
	{ Finish(false, "cohort fixture requires the paused four-mercenary squad and one paid pending hire"); return; }
	std::vector<std::size_t> ordinary;
	unsigned pendingCount = 0;
	for (std::size_t i = 0; i < before.rosterCount; ++i)
	{
		const auto& row = before.roster[i];
		if (row.pendingHire)
		{
			if (row.profile != 6 || row.assignment != IN_TRANSIT || row.group.valid())
			{ Finish(false, "cohort fixture pending hire is not the qualified Grunty flight"); return; }
			++pendingCount;
		}
		else
		{
			if (row.assignment != FIRST_SQUAD || row.x != 9 || row.y != 1 || row.z || !row.group.valid())
			{ Finish(false, "cohort fixture ordinary roster is not the qualified A9 squad"); return; }
			ordinary.push_back(i);
		}
	}
	if (pendingCount != 1 || ordinary.size() != 4)
	{ Finish(false, "cohort fixture roster partition is invalid"); return; }
	// Record the exact native flight, which neither death nor capture may retire.
	std::vector<std::pair<CampaignEventId, CampaignEventSnapshot>> flights;
	const auto captureFlights = [&]() {
		std::vector<std::pair<CampaignEventId, CampaignEventSnapshot>> result;
		for (auto* event = GetJa2CampaignEventQueue().head(); event; event = event->next)
			if (event->callbackId == EVENT_DELAYED_HIRING_OF_MERC) result.emplace_back(event->id, event->snapshot());
		return result;
	};
	flights = captureFlights();
	if (flights.size() != 1) { Finish(false, "cohort fixture requires one exact delayed-hire event"); return; }
	const auto clock = GetWorldTotalSeconds();
	const auto pendingScreen = GetPendingNewScreen();
	SetPendingNewScreen(MAINMENU_SCREEN); // Suppress only local panel selection refresh.
	CoopSession::CoopCampaignEconomy expected = before;
	const auto deadCount = capture ? 2u : 4u;
	for (std::size_t i = 0; i < deadCount; ++i)
	{
		auto& row = expected.roster[ordinary[i]];
		auto* actor = ResolveJa2TacticalEntity(row.actor);
		if (!actor) { Finish(false, "cohort fixture lost an exact actor identity"); return; }
		actor->vitals().health() = 0;
		actor->status().flags() |= SOLDIER_DEAD;
		actor->uiPresentation().queueDeadMercUi();
		FinishAnySkullPanelAnimations();
		row.assignment = ASSIGNMENT_DEAD; row.group = {}; row.inSector = false;
	}
	if (capture)
	{
		SetJa2TacticalWorldSector(9, 1, 0); // Native quest history records the capture location.
		BeginCaptureSquence();
		for (std::size_t i = deadCount; i < ordinary.size(); ++i)
		{
			auto& row = expected.roster[ordinary[i]];
			auto* actor = ResolveJa2TacticalEntity(row.actor);
			if (!actor) { Finish(false, "cohort fixture lost an exact prisoner identity"); return; }
			EnemyCapturesPlayerSoldier(actor);
			row.assignment = ASSIGNMENT_POW; row.group = {}; row.inSector = false;
			row.x = gModSettings.ubInitialPOWSectorX; row.y = gModSettings.ubInitialPOWSectorY; row.z = 0;
		}
		EndCaptureSequence();
		ClearJa2TacticalWorldSector();
	}
	SetPendingNewScreen(pendingScreen);
	StopTimeCompression(); UnLockPauseState(); PauseGame();
	CoopSession::CoopCampaignEconomy after;
	if (CaptureDedicatedCoopCampaignEconomy(after) || !CoopSession::SameCoopCampaignEconomyContent(expected, after) ||
		GetWorldTotalSeconds() != clock || captureFlights() != flights ||
		InspectDedicatedCoopStarterCampaign() != DedicatedCoopStarterCampaignState::EstablishedStrategicCold)
	{ Finish(false, "native cohort transition changed a contract, pending flight, ledger, clock or required roster state"); return; }
	std::printf("[co-op cohort seed] mode=%s world_seconds=%u deaths=%u prisoners=%u\n",
		capture ? "capture" : "death", clock, deadCount, capture ? 2u : 0u);
	std::fflush(stdout);
	cohortSeeded = true;
	cohortSeedTime = std::chrono::steady_clock::now();
}

// Read-only map facts for exit-grid probes: once per newly loaded world, the
// first exit-grid tile per destination in that sector.
void ReportLoadedWorldExitGrids()
{
	if (controlledBattle != ControlledBattle::StarterVictory || !gfProgramIsRunning) return;
	static std::uint64_t reportedWorld = 0;
	const auto world = CaptureJa2TacticalWorld();
	if (!world.loaded || world.worldGeneration == reportedWorld) return;
	reportedWorld = world.worldGeneration;
	std::vector<std::array<int, 5>> exits;
	for (INT32 grid = 0; grid < MAX_MAP_POS && exits.size() < 8; ++grid)
	{
		EXITGRID exit;
		if (!GetExitGrid(static_cast<UINT32>(grid), &exit)) continue;
		const bool known = std::any_of(exits.begin(), exits.end(), [&](const std::array<int, 5>& value) {
			return value[1] == exit.ubGotoSectorX && value[2] == exit.ubGotoSectorY && value[3] == exit.ubGotoSectorZ; });
		if (!known) exits.push_back({grid, exit.ubGotoSectorX, exit.ubGotoSectorY, exit.ubGotoSectorZ, exit.usGridNo});
	}
	std::printf("[co-op entry test] world=%llu sector=%d,%d,%d exits=%zu\n", static_cast<unsigned long long>(world.worldGeneration),
		world.sector.x, world.sector.y, world.sector.z, exits.size());
	for (const auto& exit : exits)
		std::printf("[co-op entry test] exit-grid source=%d destination=%d,%d,%d grid=%d\n",
			exit[0], exit[1], exit[2], exit[3], exit[4]);
	std::fflush(stdout);
}

// Fixture-only observation (starter-victory mode): log where every profiled
// non-player NPC stands whenever that changes, so a probe can walk toward one
// the player team has not seen yet, and log the native talk-panel state on
// each change. Never opens, advances or closes a conversation.
void PollConversationExperiment()
{
	if (controlledBattle != ControlledBattle::StarterVictory || !gfProgramIsRunning) return;
	static auto nextPoll = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	if (now >= nextPoll && CaptureJa2TacticalWorld().loaded)
	{
		nextPoll = now + std::chrono::milliseconds(200);
		static std::array<std::int32_t, 256> reported{};
		static bool initialized = false;
		if (!initialized) { reported.fill(-1); initialized = true; }
		for (SoldierID id = 0; id < TOTAL_SOLDIERS; ++id)
		{
			const auto* npc = GetJa2SoldierRepository().resolve(id);
			if (!npc || !npc->roster().active() || !npc->roster().inSector() || npc->roster().team() == OUR_TEAM) continue;
			const unsigned profile = npc->identity().profile();
			if (profile == NO_PROFILE || profile >= reported.size()) continue;
			const std::int32_t where = npc->position().gridNo() * 2 + (npc->position().level() ? 1 : 0);
			if (reported[profile] == where) continue;
			reported[profile] = where;
			std::printf("[co-op entry test] npc profile=%u grid=%d level=%d\n", unsigned(profile),
				int(npc->position().gridNo()), int(npc->position().level()));
			std::fflush(stdout);
		}
	}
	// One-shot dump of Fatima's quote records (waypoints, required grids, facts,
	// actions) so probes can follow her scripted lead. Fixture-only.
	static bool dumped = false;
	if (!dumped && CaptureJa2TacticalWorld().loaded && EnsureQuoteFileLoaded(101) && gpNPCQuoteInfoArray[101])
	{
		dumped = true;
		for (unsigned r = 0; r < NUM_NPC_QUOTE_RECORDS; ++r)
		{
			const NPCQuoteInfo& q = gpNPCQuoteInfoArray[101][r];
			if (!q.ubQuoteNum && !q.ubNumQuotes && !q.sActionData && !q.usGoToGridNo) continue;
			std::printf("[co-op entry test] npc-record 101#%u flags=%u reqItem=%d reqGrid=%d factTrue=%u factFalse=%u approach=%u "
				"quote=%u+%u trigger=%u#%u setFact=%u gift=%u goto=%d action=%d\n", r, unsigned(q.fFlags), int(q.sRequiredItem),
				int(q.sRequiredGridNo), unsigned(q.usFactMustBeTrue), unsigned(q.usFactMustBeFalse), unsigned(q.ubApproachRequired),
				unsigned(q.ubQuoteNum), unsigned(q.ubNumQuotes), unsigned(q.ubTriggerNPC), unsigned(q.ubTriggerNPCRec),
				unsigned(q.usSetFactTrue), unsigned(q.usGiftItem), int(q.usGoToGridNo), int(q.sActionData));
		}
		std::fflush(stdout);
	}
	static std::string last;
	char line[700];
	char quote[200] = {};
	std::snprintf(quote, sizeof(quote), "%ls", gTalkPanel.zQuoteStr);
	std::snprintf(line, sizeof(line), "panel=%u dialogue=%u queue-empty=%u engaged=%u paused=%u char=%u quote=%.120s",
		gfInTalkPanel, DialogueActive(), DialogueQueueIsEmpty(), (gTacticalStatus.uiFlags & ENGAGED_IN_CONV) != 0,
		GamePaused(), gTalkPanel.ubCharNum, quote);
	if (last != line) { last = line; std::printf("[co-op talk test] %s\n", line); std::fflush(stdout); }
}

void PollControlledBattleTrigger()
{
	if (controlledBattle == ControlledBattle::None || controlledBattleSeeded || !gfProgramIsRunning) return;
	static auto nextPoll = std::chrono::steady_clock::now();
	const auto now = std::chrono::steady_clock::now();
	if (now < nextPoll) return;
	nextPoll = now + std::chrono::milliseconds(25);
	const std::filesystem::path trigger("controlled-battle-trigger.txt");
	if (!std::filesystem::exists(trigger)) return;
	if (controlledBattle == ControlledBattle::StarterVictory)
	{
		// A newly created campaign's starter battle, with no pinned cohort. Seed
		// only enemy incapacitation; native end-of-battle processing decides the
		// victory, history and sector control, and the runtime owns the rest.
		std::string magic, requestedOutcome, extra;
		std::uint64_t requestedWorld = 0, requestedTurn = 0;
		std::ifstream input(trigger);
		if (std::filesystem::is_symlink(trigger) || !std::filesystem::is_regular_file(trigger) ||
			!(input >> magic >> requestedOutcome >> requestedWorld >> requestedTurn) || (input >> extra) ||
			magic != "JA2_COOP_CONTROLLED_BATTLE_V1" || requestedOutcome != "starter-victory" ||
			!requestedWorld || !requestedTurn)
		{ Finish(false, "invalid starter victory trigger"); return; }
		input.close();
		// One seeded victory per loaded battle world; each trigger is consumed
		// under its own name and can never be replayed for the same world.
		static std::uint64_t lastSeededWorld = 0;
		static unsigned consumed = 0;
		std::filesystem::rename(trigger, "controlled-battle-trigger.consumed." + std::to_string(++consumed));
		if (requestedWorld <= lastSeededWorld) { Finish(false, "repeated starter victory trigger for one world"); return; }
		lastSeededWorld = requestedWorld;
		const auto world = CaptureJa2TacticalWorld();
		if (!world.loaded || world.worldGeneration != requestedWorld || world.turnSerial != requestedTurn ||
			world.turn.pendingCombatActions || (world.turn.inCombat && world.turn.currentTeam != OUR_TEAM) ||
			DedicatedCoopArrivalDecisionPending() || DedicatedCoopSurrenderPending() || DedicatedCoopBattleNoticePending() ||
			GetTacticalTeamMenInSector(ENEMY_TEAM) <= 0)
		{ Finish(false, "starter victory lacks an idle native starter battle with enemies"); return; }
		std::vector<TacticalActor*> enemies;
		auto& repository = GetJa2SoldierRepository();
		for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
		{
			auto* enemy = repository.resolve(slot);
			if (!enemy || !enemy->roster().active() || !enemy->roster().inSector() ||
				enemy->roster().team() != ENEMY_TEAM || (enemy->status().flags() & SOLDIER_DEAD)) continue;
			if (enemy->vitals().health() <= 0 || enemy->aiBehavior().neutral() || enemy->roster().side() == OUR_TEAM)
			{ Finish(false, "starter victory contains an unsupported enemy death or allegiance"); return; }
			enemies.push_back(enemy);
		}
		if (enemies.empty()) { Finish(false, "starter victory has no native enemies"); return; }
		for (auto* enemy : enemies) { enemy->vitals().health() = 1; enemy->vitals().bleeding() = 0; }
		if (!CheckForEndOfBattle(FALSE) || IsJa2TacticalCombatActive() || !gTacticalStatus.fLastBattleWon ||
			DedicatedCoopBattleNoticePending())
		{ Finish(false, "native starter victory failed to resolve"); return; }
		std::printf("[co-op controlled battle] seeded outcome=starter-victory world=%llu turn=%llu enemies=%zu; awaiting native return\n",
			static_cast<unsigned long long>(world.worldGeneration), static_cast<unsigned long long>(world.turnSerial), enemies.size());
		std::fflush(stdout);
		return;
	}
	const bool capture = controlledBattle == ControlledBattle::Capture;
	const bool victory = controlledBattle == ControlledBattle::Victory;
	const char* outcome = victory ? "victory" : capture ? "capture" : "defeat";
	std::string magic, requestedOutcome, extra;
	std::uint64_t requestedWorld = 0, requestedTurn = 0;
	std::ifstream input(trigger);
	if (std::filesystem::is_symlink(trigger) || !std::filesystem::is_regular_file(trigger) ||
		!(input >> magic >> requestedOutcome >> requestedWorld >> requestedTurn) || (input >> extra) ||
		magic != "JA2_COOP_CONTROLLED_BATTLE_V1" || requestedOutcome != outcome || !requestedWorld || !requestedTurn ||
		std::filesystem::exists("controlled-battle-trigger.consumed"))
	{ Finish(false, "invalid or repeated controlled battle trigger"); return; }
	input.close();
	std::filesystem::rename(trigger, "controlled-battle-trigger.consumed");
	controlledBattleSeeded = true; // A native partial transition is never replayed.
	const auto world = CaptureJa2TacticalWorld();
	CoopSession::CoopCampaignEconomy before;
	if (!world.loaded || world.worldGeneration != requestedWorld || world.turnSerial != requestedTurn ||
		!(world.sector == (TacticalWorldSession::Sector{9, 1, 0})) || world.turn.pendingCombatActions ||
		(world.turn.inCombat && world.turn.currentTeam != OUR_TEAM) ||
		DedicatedCoopArrivalDecisionPending() || DedicatedCoopSurrenderPending() || DedicatedCoopBattleNoticePending() ||
		CaptureDedicatedCoopCampaignEconomy(before) || before.rosterCount != 5 || before.mercenaryCount != 5 ||
		before.balance != 13640 || gStrategicStatus.ubNumCapturedForRescue ||
		gubQuest[QUEST_HELD_IN_ALMA] != QUESTNOTSTARTED || GetTacticalTeamMenInSector(ENEMY_TEAM) <= 0)
	{ Finish(false, "controlled loss lacks its exact idle native A9 battle and pending-hire cohort"); return; }
	std::vector<std::size_t> ordinary;
	unsigned pendingCount = 0;
	for (std::size_t i = 0; i < before.rosterCount; ++i)
	{
		const auto& row = before.roster[i];
		auto* actor = ResolveJa2TacticalEntity(row.actor);
		if (!actor || row.vehicle || row.betweenSectors)
		{ Finish(false, "controlled loss contains an unresolved or unsupported actor"); return; }
		if (row.pendingHire)
		{
			if (row.profile != 6 || row.assignment != IN_TRANSIT || row.inSector || row.group.valid() ||
				row.arrivalMinutes != 2250 || row.contractEndMinutes != 13740)
			{ Finish(false, "controlled loss changed the independent pending hire"); return; }
			++pendingCount;
		}
		else
		{
			if (row.assignment != FIRST_SQUAD || row.x != 9 || row.y != 1 || row.z || !row.inSector ||
				!row.group.valid() || actor->vitals().health() < OKLIFE || (actor->status().flags() & SOLDIER_DEAD))
			{ Finish(false, "controlled loss needs four healthy, placed ordinary defenders"); return; }
			ordinary.push_back(i);
		}
	}
	if (pendingCount != 1 || ordinary.size() != 4)
	{ Finish(false, "controlled loss has an invalid roster partition"); return; }
	const auto flights = []() {
		std::vector<std::pair<CampaignEventId, CampaignEventSnapshot>> result;
		for (auto* event = GetJa2CampaignEventQueue().head(); event; event = event->next)
			if (event->callbackId == EVENT_DELAYED_HIRING_OF_MERC) result.emplace_back(event->id, event->snapshot());
		return result;
	};
	const auto beforeFlights = flights();
	if (beforeFlights.size() != 1) { Finish(false, "controlled loss lost its exact pending flight"); return; }
	const auto clock = GetWorldTotalSeconds();
	if (victory)
	{
		// Seed incapacitation only in this disposable host. Native end-of-battle
		// processing applies the actual blood-loss damage, enemy group deaths,
		// history, morale and sector control; the runtime owns drain/checkpoint.
		// Never declare a win, delete a group or write a campaign flag here.
		std::vector<TacticalActor*> enemies;
		auto& repository = GetJa2SoldierRepository();
		for (std::size_t slot = 0; slot < repository.capacity(); ++slot)
		{
			auto* enemy = repository.resolve(slot);
			if (!enemy || !enemy->roster().active() || !enemy->roster().inSector() ||
				enemy->roster().team() != ENEMY_TEAM || (enemy->status().flags() & SOLDIER_DEAD)) continue;
			if (enemy->vitals().health() <= 0 || enemy->aiBehavior().neutral() || enemy->roster().side() == OUR_TEAM)
			{ Finish(false, "controlled victory contains an unsupported enemy death or allegiance"); return; }
			enemies.push_back(enemy);
		}
		if (enemies.empty()) { Finish(false, "controlled victory has no native enemies"); return; }
		for (auto* enemy : enemies)
		{
			enemy->vitals().health() = 1;
			enemy->vitals().bleeding() = 0;
		}
		CoopSession::CoopCampaignEconomy after;
		if (!CheckForEndOfBattle(FALSE) || IsJa2TacticalCombatActive() || !gTacticalStatus.fLastBattleWon ||
			DedicatedCoopBattleNoticePending() || CaptureDedicatedCoopCampaignEconomy(after) ||
			!CoopSession::SameCoopCampaignEconomyContent(before, after) ||
			GetWorldTotalSeconds() != clock || flights() != beforeFlights)
		{ Finish(false, "native controlled victory changed the cohort, pending flight or failed to resolve"); return; }
		std::printf("[co-op controlled battle] seeded outcome=victory world=%llu turn=%llu clock=%u enemies=%zu; awaiting native return\n",
			static_cast<unsigned long long>(world.worldGeneration), static_cast<unsigned long long>(world.turnSerial), clock, enemies.size());
		std::fflush(stdout);
		return;
	}
	// Only the opt-in fixture writes these health preconditions. Native death
	// handling removes graphics/inventory; CheckForEndOfBattle decides whether
	// the remaining incapacitated merc(s) die or are captured, publishes history
	// and sector loss, and invokes the real timer/notice path. Never call a notice,
	// acknowledgement, unload or checkpoint function on behalf of the clients.
	const auto initialDeaths = capture ? 2u : 3u;
	for (std::size_t i = 0; i < ordinary.size(); ++i)
	{
		auto* actor = ResolveJa2TacticalEntity(before.roster[ordinary[i]].actor);
		actor->vitals().health() = i < initialDeaths ? 0 : 1;
		actor->vitals().bleeding() = 0;
	}
	for (std::size_t i = 0; i < initialDeaths; ++i)
	{
		BOOLEAN madeCorpse = FALSE;
		HandleSoldierDeath(ResolveJa2TacticalEntity(before.roster[ordinary[i]].actor), &madeCorpse);
	}
	if (!CheckForEndOfBattle(FALSE) || IsJa2TacticalCombatActive() || gTacticalStatus.fLastBattleWon ||
		!DedicatedCoopBattleNoticePending())
	{ Finish(false, "native loss did not produce its held battle notice"); return; }
	FinishAnySkullPanelAnimations();
	CoopSession::CoopCampaignEconomy expected = before, after;
	for (std::size_t i = 0; i < ordinary.size(); ++i)
	{
		auto& row = expected.roster[ordinary[i]];
		const bool prisoner = capture && i >= initialDeaths;
		row.assignment = prisoner ? ASSIGNMENT_POW : ASSIGNMENT_DEAD;
		row.group = {};
		if (prisoner)
		{
			row.inSector = false;
			row.x = gModSettings.ubInitialPOWSectorX; row.y = gModSettings.ubInitialPOWSectorY; row.z = 0;
		}
	}
	if (CaptureDedicatedCoopCampaignEconomy(after) || !CoopSession::SameCoopCampaignEconomyContent(expected, after) ||
		GetWorldTotalSeconds() != clock || flights() != beforeFlights || !GamePaused())
	{ Finish(false, "native loss changed a contract, pending flight, clock or expected death/prisoner partition"); return; }
	std::printf("[co-op controlled battle] seeded outcome=%s world=%llu turn=%llu clock=%u; awaiting real player acknowledgement\n",
		outcome, static_cast<unsigned long long>(world.worldGeneration), static_cast<unsigned long long>(world.turnSerial), clock);
	std::fflush(stdout);
}

void RunEntryFixture()
{
	const char* mode = std::getenv("JA2_COOP_ENTRY_TEST_MODE");
	if (!mode) mode = "spread";
	const bool forced = std::strcmp(mode, "forced") == 0;
	const bool ambushSpread = std::strcmp(mode, "ambush-spread") == 0;
	const bool failedLoad = std::strcmp(mode, "failed-load") == 0;
	const bool observe = std::strcmp(mode, "observe") == 0;
	const bool retreat = std::strcmp(mode, "retreat") == 0;
	const bool travel = std::strcmp(mode, "travel-seed") == 0;
	if (!forced && !ambushSpread && !failedLoad && !observe && !retreat && !travel && std::strcmp(mode, "spread") != 0)
	{ Finish(false, "unknown fixture mode"); return; }
	if (IsJa2TacticalWorldLoaded() || DedicatedCoopArrivalDecisionPending())
	{ Finish(false, "fixture requires a fresh worldless campaign"); return; }
	// Native reinforcement creation forbids a player-occupied source sector.
	// Create the hostile group before placing fixture mercs; otherwise it queues
	// an invariant-warning dialog which makes the later battle context invalid.
	auto& sector = SectorInfo[SECTOR(10, 1)];
	sector.ubGarrisonID = NO_GARRISON;
	sector.ubNumAdmins = sector.ubNumTroops = sector.ubNumElites = 0;
	if (!CreateNewEnemyGroupDepartingFromSector(SECTOR(10, 1), 0, 7, 0, 0, 0, 0))
	{ Finish(false, "fixture enemy group creation failed"); return; }

	// Explicit fixture setup only. These are not shipping travel/arrival calls.
	std::vector<TacticalEntityId> actors(travel ? DedicatedCoopStarterRosterSize : 2);
	std::size_t count = 0;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID;
		slot <= gTacticalStatus.Team[OUR_TEAM].bLastID && count < actors.size(); ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (!actor || !actor->roster().active() || actor->assignment().current() != IN_TRANSIT) continue;
		actor->deployment().setSector(travel ? 9 : 10, 1, 0);
		if (retreat) actor->deployment().previousSectorId() = SECTOR(9, 1);
		actor->deployment().strategicInsertionCode() = INSERTION_CODE_WEST;
		actor->statistics().leadership() = ambushSpread ? 95 : 0;
		actor->statistics().experienceLevel() = ambushSpread ? 10 : 1;
		if (!AddCharacterToSquad(actor, FIRST_SQUAD)) { Finish(false, "native fixture squad assignment failed"); return; }
		actors[count++] = GetJa2TacticalEntityId(*actor);
	}
	if (count != actors.size()) { Finish(false, "fixture needs two initialized hired actors"); return; }
	auto* first = ResolveJa2TacticalEntity(actors[0]);
	auto* group = GetGroup(first->deployment().groupId());
	if (!group) { Finish(false, "fixture group missing"); return; }
	group->ubPrevX = 9; group->ubPrevY = 1;
	group->uiFlags &= ~GROUPFLAG_JUST_RETREATED_FROM_BATTLE;
	// Preserve the trait system used when native profiles and actors were loaded.
	// Changing it here writes incompatible trait IDs into the cold seed.
	if (!travel) gGameOptions.fNewTraitSystem = FALSE;
	gGameExternalOptions.gfAllowReinforcements = FALSE;
	// Exercise native encirclement rather than depending on installed options
	// or whether an enemy's original map placement happens to need relocation.
	if (forced || ambushSpread) gGameExternalOptions.uAmbushEnemyEncircle = 2;
	gfHighPotentialForAmbush = gfCantRetreatInPBI = FALSE;
	gfAutoAmbush = forced || ambushSpread;
	SetEnemyEncounterCode(ENEMY_ENCOUNTER_CODE);
	CoopSession::CoopCampaignGroups captured;
	if (const char* error = CaptureDedicatedCoopCampaignGroups(captured)) { Finish(false, error); return; }
	if (travel)
	{
		// Create a disposable established cold checkpoint, then let a new normal
		// runtime classify it on resume. Never alter the shipping bootstrap state
		// machine or retain a partial starter roster with pending hiring arrivals.
		DeleteAllStrategicEventsOfType(EVENT_DELAYED_HIRING_OF_MERC);
		for (const auto id : actors)
		{
			auto* actor = ResolveJa2TacticalEntity(id);
			const int traitLimit = gGameOptions.fNewTraitSystem ? NUM_SKILLTRAITS_NT : NUM_SKILLTRAITS_OT;
			for (unsigned index = 0; index < 30; ++index)
			{
				const int profileTrait = gMercProfiles[actor->identity().profile()].bSkillTraits[index];
				const int actorTrait = actor->statistics().skillTrait(index);
				if (profileTrait < 0 || profileTrait >= traitLimit || actorTrait < 0 || actorTrait >= traitLimit)
				{ Finish(false, "cold seed trait IDs do not match the native campaign options"); return; }
			}
			gMercProfiles[actor->identity().profile()].bMercStatus = static_cast<INT8>(actor->employment().totalLength());
		}
		gTacticalStatus.fDidGameJustStart = FALSE;
		RestoreJa2CampaignClock(111600, 111600);
		auto& origin = SectorInfo[SECTOR(9, 1)];
		origin.ubNumAdmins = origin.ubNumTroops = origin.ubNumElites = 0;
		origin.ubNumRobots = origin.ubNumTanks = origin.ubNumJeeps = 0;
		origin.ubGarrisonID = NO_GARRISON;
		for (auto* enemy = gpGroupList; enemy;)
		{
			auto* next = enemy->next;
			if (enemy->usGroupTeam == ENEMY_TEAM && !enemy->fBetweenSectors && enemy->ubSectorX == 9 && enemy->ubSectorY == 1)
				RemoveGroup(enemy->ubGroupID);
			enemy = next;
		}
		StopTimeCompression(); PauseGame();
		if (group->ubSectorX != 9 || group->ubSectorY != 1 || group->fBetweenSectors ||
			group->ubGroupSize != actors.size() || !GamePaused() || NumEnemiesInAnySector(9, 1, 0) ||
			InspectDedicatedCoopStarterCampaign() != DedicatedCoopStarterCampaignState::EstablishedCold)
		{ Finish(false, "travel fixture requires a paused idle A9 squad"); return; }
		Finish(true, "installed campaign travel seed checkpoint; no remote gameplay");
		return;
	}
	const auto seconds = GetWorldTotalSeconds();
	if (!DeferDedicatedCoopArrival(DedicatedCoopArrivalKind::Battle, group, group))
	{ Finish(false, "fixture encounter was not captured"); return; }
	NativePreBattlePreparation preparation;
	std::printf("[co-op entry test] preparing mode=%s\n", mode); std::fflush(stdout);
	if (PrepareDedicatedCoopArrivalBattle(1, preparation) != DedicatedCoopArrivalPrepareResult::Prepared)
	{ Finish(false, "native preparation failed"); return; }
	const auto expected = forced ? ENEMY_AMBUSH_CODE : ambushSpread ? ENEMY_AMBUSH_DEPLOYMENT_CODE : ENEMY_ENCOUNTER_CODE;
	if (preparation.encounterCode != expected || preparation.actions.tacticalPlacement == forced)
	{ std::printf("encounter=%u expected=%u\n", preparation.encounterCode, expected); Finish(false, "unexpected native encounter/deployment"); return; }
	if (observe)
	{
		// Stay in the real runtime pump for independent clients to observe this
		// exact native preparation. This fixture does not authorize/submit entry.
		observing = true;
		(void)CaptureDedicatedCoopCampaignGroups(preparedObservation);
		std::printf("[co-op entry test] observing pending battle id=1\n"); std::fflush(stdout);
		return;
	}
	const auto before = GetGameSimulationRandomSource()->checkpoint();
	if (retreat)
	{
		const auto result = RetreatFromDedicatedCoopArrivalBattle(1);
		std::printf("[co-op entry test] retreat-context: result=%u; pending=%u; active=%u; world=%u; paused=%u; clock=%u; healthy=%u; moving=%u; next=%u,%u; flags=%u\n",
			static_cast<unsigned>(result), DedicatedCoopArrivalDecisionPending(), IsHeadlessPreBattleActive(), IsJa2TacticalWorldLoaded(),
			GamePaused(), GetWorldTotalSeconds(), GetGameSimulationRandomSource()->healthy(), group->fBetweenSectors,
			group->ubNextX, group->ubNextY, group->uiFlags);
		if (result != DedicatedCoopArrivalRetreatResult::Retreated || DedicatedCoopArrivalDecisionPending() ||
			IsHeadlessPreBattleActive() || IsJa2TacticalWorldLoaded() || !GamePaused() ||
			GetWorldTotalSeconds() != seconds || !GetGameSimulationRandomSource()->healthy() ||
			!group->fBetweenSectors || group->ubNextX != 9 || group->ubNextY != 1 ||
			!(group->uiFlags & GROUPFLAG_JUST_RETREATED_FROM_BATTLE))
		{ std::printf("retreat-result=%u\n", static_cast<unsigned>(result)); Finish(false, "native retreat postcondition failed"); return; }
		for (const auto id : actors)
		{
			const auto* actor = ResolveJa2TacticalEntity(id);
			if (!actor || !actor->deployment().isBetweenSectors() || actor->roster().inSector() || !actor->strategicPath().empty())
			{ Finish(false, "native retreat participant was not moved into transit"); return; }
		}
		CoopSession::CoopCampaignGroups moving;
		if (const char* error = CaptureDedicatedCoopCampaignGroups(moving)) { Finish(false, error); return; }
		const auto committed = GetGameSimulationRandomSource()->checkpoint();
		const auto events = GetJa2CampaignEventQueue().size();
		for (unsigned retry = 0; retry < 100; ++retry)
			if (RetreatFromDedicatedCoopArrivalBattle(1) != DedicatedCoopArrivalRetreatResult::NotPending ||
				GetGameSimulationRandomSource()->checkpoint() != committed || GetJa2CampaignEventQueue().size() != events)
			{ Finish(false, "duplicate native retreat repeated gameplay/events"); return; }
		std::printf("[co-op entry test] retreat: from=A10; to=A9; arrival=%u; events=%zu; clock=%u\n", group->uiArrivalTime, events, seconds);
		Finish(true, "installed native retreat and enemy response; seeded fixture, no remote decision/travel claim");
		return;
	}
	const auto choice = forced ? NativePreBattleDeployment::Forced : NativePreBattleDeployment::Spread;
	std::printf("[co-op entry test] entering mode=%s\n", mode); std::fflush(stdout);
	if (failedLoad)
	{
		// Empty override in the campaign's private writable VFS profile. Exists
		// for preflight, but the actual native loader rejects its zero length.
		const HWFILE file = FileOpen("MAPS\\A10.DAT", FILE_ACCESS_WRITE | FILE_CREATE_ALWAYS);
		if (!file) { Finish(false, "cannot create private failed-load fixture"); return; }
		FileClose(file);
	}
	const auto result = EnterDedicatedCoopArrivalBattle(1, choice);
	if (failedLoad)
	{
		if (result != DedicatedCoopArrivalEnterResult::Failed || !DedicatedCoopArrivalDecisionPending() ||
			!GamePaused() || IsHeadlessPreBattleEntryInProgress() || IsJa2TacticalWorldLoaded() ||
			gWorldSectorX != 10 || gWorldSectorY != 1 || GetWorldTotalSeconds() != seconds)
		{ Finish(false, "native load failure was not latched/paused after mutation began"); return; }
		const auto failedRng = GetGameSimulationRandomSource()->checkpoint();
		for (unsigned retry = 0; retry < 100; ++retry)
			if (EnterDedicatedCoopArrivalBattle(1, choice) != DedicatedCoopArrivalEnterResult::Failed ||
				GetGameSimulationRandomSource()->checkpoint() != failedRng)
			{ Finish(false, "partial native load failure was replayed"); return; }
		Finish(true, "native partial-load failure latched; no retry/decision consumption");
		return;
	}
	if (result != DedicatedCoopArrivalEnterResult::Entered)
	{ std::printf("entry-result=%u\n", static_cast<unsigned>(result)); Finish(false, "native world entry failed"); return; }
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 10 || gWorldSectorY != 1 || gbWorldSectorZ != 0 ||
		IsHeadlessPreBattleActive() || IsHeadlessPreBattleEntryInProgress() || gfPreBattleInterfaceActive ||
		gfEnterTacticalPlacementGUI || gfTacticalPlacementGUIActive || DedicatedCoopArrivalDecisionPending() ||
		GetWorldTotalSeconds() != seconds || !GetGameSimulationRandomSource()->healthy() ||
		GetGameSimulationRandomSource()->checkpoint() == before)
	{ Finish(false, "native world/clock/GUI/RNG postcondition failed"); return; }
	for (const auto id : actors)
	{
		const auto* actor = ResolveJa2TacticalEntity(id);
		if (!actor || !actor->roster().inSector() || TileIsOutOfBounds(actor->position().gridNo()) ||
			static_cast<UINT8>(actor->position().direction()) >= NUM_WORLD_DIRECTIONS ||
			static_cast<UINT8>(actor->pathing().desiredDirection()) >= NUM_WORLD_DIRECTIONS)
		{ Finish(false, "native participant placement missing"); return; }
		std::printf("[co-op entry test] actor=%u:%u grid=%d direction=%u\n", id.slot, id.incarnation,
			actor->position().gridNo(), actor->position().direction());
	}
	unsigned enemies = 0;
	for (SoldierID slot = gTacticalStatus.Team[ENEMY_TEAM].bFirstID; slot <= gTacticalStatus.Team[ENEMY_TEAM].bLastID; ++slot)
	{
		const auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (!actor || !actor->roster().active() || !actor->roster().inSector() || TileIsOutOfBounds(actor->position().gridNo())) continue;
		if (static_cast<UINT8>(actor->position().direction()) >= NUM_WORLD_DIRECTIONS ||
			static_cast<UINT8>(actor->pathing().desiredDirection()) >= NUM_WORLD_DIRECTIONS ||
			static_cast<UINT8>(actor->aiPlanning().dominantDirection()) >= NUM_WORLD_DIRECTIONS ||
			actor->deployment().insertionDirection() >= NUM_WORLD_DIRECTIONS || !actor->renderBindings().levelNode())
		{ Finish(false, "native enemy facing/footprint invalid after insertion"); return; }
		if ((forced || ambushSpread) && actor->aiPlanning().dominantDirection() !=
			GetDirectionToGridNoFromGridNo(actor->deployment().insertionGrid(), gMapInformation.sCenterGridNo))
		{ Finish(false, "native encirclement lost inward-facing direction"); return; }
		++enemies;
	}
	if (enemies != 7) { Finish(false, "native enemy population incomplete"); return; }
	const auto committed = GetGameSimulationRandomSource()->checkpoint();
	for (unsigned retry = 0; retry < 100; ++retry)
		if (EnterDedicatedCoopArrivalBattle(1, choice) != DedicatedCoopArrivalEnterResult::NotPending ||
			GetGameSimulationRandomSource()->checkpoint() != committed)
		{ Finish(false, "duplicate entry repeated native side effects"); return; }
	std::printf("[co-op entry test] mode=%s enemies=%u map=A10 clock=%u\n", mode, enemies, seconds);
	Finish(true, "installed native entry; seeded fixture, no travel/peers/physical input claim");
}
}

void FullEngineCoopInstalledTestGameLoop()
{
	GameLoop();
	ReportLoadedWorldExitGrids();
	PollConversationExperiment();
	PollControlledBattleTrigger();
	if (cohortSeeded && gfProgramIsRunning)
	{
		if (!DialogueActive() && DialogueQueueIsEmpty())
		{ Finish(true, "native cold cohort seed; no combat or battle-return qualification"); return; }
		if (std::chrono::steady_clock::now() - cohortSeedTime > std::chrono::seconds(30))
		{ Finish(false, "native cohort dialogue did not drain"); return; }
		return;
	}
	static unsigned observationChecks = 0;
	if (observing && gfProgramIsRunning && observationChecks++ < 2)
	{
		CoopSession::CoopCampaignGroups current;
		const auto* diagnostic = CaptureDedicatedCoopCampaignGroups(current);
		NativePreBattlePreparation prepared;
		const auto result = PrepareDedicatedCoopArrivalBattle(1, prepared);
		std::printf("[co-op entry test] observation-context: result=%u; screen=%u; map=%u; event=%u; active=%u; group=%u; encounter=%u; clock=%u; groups-same=%u; diagnostic=%s\n",
			static_cast<unsigned>(result), GetCurrentScreen(), fInMapMode, gfProcessingGameEvents,
			IsHeadlessPreBattleActive(), ResolvePreBattleGroup() != nullptr, GetEnemyEncounterCode(), GetWorldTotalSeconds(),
			CoopSession::SameCoopCampaignGroups(current, preparedObservation), diagnostic ? diagnostic : "none");
		std::fflush(stdout);
		if (result != DedicatedCoopArrivalPrepareResult::Prepared || diagnostic ||
			!CoopSession::SameCoopCampaignGroups(current, preparedObservation))
		{ Finish(false, "prepared native context did not survive a normal runtime frame"); return; }
		if (observationChecks == 2)
		{ std::printf("[co-op entry test] pending native context stable\n"); std::fflush(stdout); }
	}
	static bool ran = false;
	if (ran || !gfProgramIsRunning || !GetDedicatedCoopRuntime().campaignEntered() || !IsDedicatedCoopStarterMissionMapReady()) return;
	ran = true;
	const char* mode = std::getenv("JA2_COOP_ENTRY_TEST_MODE");
	if (mode && (std::strcmp(mode, "battle-return-defeat") == 0 || std::strcmp(mode, "battle-return-capture") == 0 ||
		std::strcmp(mode, "battle-return-victory") == 0) &&
		GetDedicatedServerOptions().campaignAction == DedicatedCampaignAction::Resume)
	{
		controlledBattle = std::strcmp(mode, "battle-return-victory") == 0 ? ControlledBattle::Victory :
			std::strcmp(mode, "battle-return-capture") == 0 ? ControlledBattle::Capture : ControlledBattle::Defeat;
		std::printf("[co-op controlled battle] armed; ordinary Resume, time control and battle entry required\n");
		std::fflush(stdout);
		return;
	}
	if (mode && std::strcmp(mode, "starter-victory") == 0 &&
		GetDedicatedServerOptions().campaignAction == DedicatedCampaignAction::Create)
	{
		// Leave the shipping starter mission untouched until the trigger arrives.
		controlledBattle = ControlledBattle::StarterVictory;
		std::printf("[co-op controlled battle] armed; starter battle victory trigger required\n");
		std::fflush(stdout);
		return;
	}
	if (mode && (std::strcmp(mode, "cohort-death") == 0 || std::strcmp(mode, "cohort-capture") == 0) &&
		GetDedicatedServerOptions().campaignAction == DedicatedCampaignAction::Resume)
	{
		SeedColdCohort(std::strcmp(mode, "cohort-capture") == 0);
		return;
	}
	if (mode && std::strcmp(mode, "travel") == 0 && GetDedicatedServerOptions().campaignAction == DedicatedCampaignAction::Resume)
	{
		CoopSession::CoopCampaignGroups groups;
		if (CaptureDedicatedCoopCampaignGroups(groups) || groups.groupCount != 1 || groups.memberCount != DedicatedCoopStarterRosterSize ||
			groups.groups[0].x != 9 || groups.groups[0].y != 1 || groups.groups[0].betweenSectors ||
			NumEnemiesInAnySector(9, 1, 0) || !NumEnemiesInAnySector(10, 1, 0))
		{ Finish(false, "resumed campaign travel seed is invalid"); return; }
		// No seed mutation on resume: the usual readiness/established-sector logic
		// must expose the peaceful cold campaign before any remote travel request.
		std::printf("[co-op entry test] travel fixture ready A9 -> A10\n"); std::fflush(stdout);
		return;
	}
	if (GetDedicatedServerOptions().campaignAction != DedicatedCampaignAction::Create)
	{ Finish(false, "only disposable newly created campaigns are supported"); return; }
	RunEntryFixture();
}
