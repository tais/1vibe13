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
#include "ai.h"
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
#include "Scheduling.h"
#include "Reinforcement.h"
#include "TacticalBattleCheckpointPolicy.h"
#include "TacticalInterruptHost.h"
#include "DedicatedCampaignSaveBridge.h"
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
#include "Town Militia.h"
#include "Player Command.h"
#include "Keys.h"
#include "End Game.h"
#include "Explosion Control.h"
#include "TacticalActorDamageResolution.h"
#include "World Items.h"
#include "Items.h"
#include "Structure.h"
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
extern UINT32 guiArrived;
extern UINT32 guiMilitiaArrived;

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
			if (profile == 101)
			{
				// Fatima's scripted lead: pending record, its action and the
				// nearest merc, on change (fixture-only diagnostics).
				static std::string lastLead;
				INT32 distance = -1;
				TacticalActor* const mutableNpc = GetJa2SoldierRepository().resolve(id);
				const INT32 closest = mutableNpc ? ClosestPC(mutableNpc, &distance) : -1;
				char lead[200];
				std::snprintf(lead, sizeof(lead), "grid=%d record=%d action=%d every-frame=%d closest=%d distance=%d",
					int(npc->position().gridNo()), int(npc->dialogue().quoteRecord()), int(npc->dialogue().quoteActionId()),
					int((npc->aiBehavior().flags() & AI_HANDLE_EVERY_FRAME) != 0), int(closest), int(distance));
				if (lastLead != lead) { lastLead = lead; std::printf("[co-op entry test] npc-lead 101 %s\n", lead); std::fflush(stdout); }
			}
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
	// Omerta opening facts on change (letter, lead, basement, Miguel), fixture-only.
	{
		static std::string lastFacts;
		std::string facts;
		for (const unsigned fact : {13u, 14u, 15u, 16u, 19u, 20u, 22u, 23u, 25u})
			facts += " " + std::to_string(fact) + "=" + std::to_string(gubFact[fact] ? 1 : 0);
		if (facts != lastFacts) { lastFacts = facts; std::printf("[co-op entry test] facts%s\n", facts.c_str()); std::fflush(stdout); }
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

// Fixture-only (JA2_COOP_ENTRY_MILITIA_ATTACK=held|defeated, starter-victory
// mode): once the players are back on the worldless map, B9 becomes a player-
// held wilderness sector that only militia defend and an army group sets out
// from C9 to attack it. The ordinary clock and arrival rules do the rest.
// Fixture-only: "knockout-trigger.txt" (written by the probe, "<slot>
// <incarnation>") knocks that merc unconscious through the stock damage path,
// as blood loss would, and logs its health. One trigger per run.
void PollKnockOutTrigger()
{
	static bool done = false;
	const std::filesystem::path trigger("knockout-trigger.txt");
	if (done || !gfProgramIsRunning || !IsJa2TacticalWorldLoaded() || !std::filesystem::exists(trigger)) return;
	done = true;
	unsigned slot = 0, incarnation = 0;
	if (FILE* file = std::fopen("knockout-trigger.txt", "r"))
	{
		if (std::fscanf(file, "%u %u", &slot, &incarnation) != 2) slot = 0;
		std::fclose(file);
	}
	std::error_code ignored;
	std::filesystem::rename(trigger, "knockout-trigger.consumed", ignored);
	TacticalActor* merc = slot ? ResolveJa2TacticalEntity(TacticalEntityId{static_cast<std::uint16_t>(slot), static_cast<std::uint32_t>(incarnation)}) : nullptr;
	if (!merc || merc->vitals().health() < OKLIFE)
	{
		std::printf("[co-op entry test] knock-out refused: actor=%u:%u\n", slot, incarnation);
		std::fflush(stdout);
		return;
	}
	const INT16 loss = INT16(merc->vitals().health() - (OKLIFE - 1));
	(void)TacticalActorDamageResolution::takeDamage(*merc, 0, loss, 0, TAKE_DAMAGE_BLOODLOSS, NOBODY,
		merc->position().gridNo(), 0, false);
	std::printf("[co-op entry test] knocked out: actor=%u:%u; health=%d\n", slot, incarnation,
		int(merc->vitals().health()));
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_SECTOR_MARKER): "place" drops a marker item at a
// merc's feet once A9 is secured; "find:<grid>:<item>" reports, once A9 is
// loaded again (after a stop, a resume and a re-entry), whether it is still
// there. Proves the sector's own changes survive a stop in a secured sector.
void PollSectorMarker()
{
	static bool done = false;
	static std::chrono::steady_clock::time_point calmSince{};
	const char* mode = std::getenv("JA2_COOP_ENTRY_SECTOR_MARKER");
	if (done || !mode || !gfProgramIsRunning) return;
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		NumEnemiesInAnySector(9, 1, 0) || IsJa2TacticalCombatActive())
	{ calmSince = {}; return; }
	if (std::strcmp(mode, "place") == 0)
	{
		// Only once the won sector has settled into peaceful play.
		const auto now = std::chrono::steady_clock::now();
		if (calmSince == std::chrono::steady_clock::time_point{}) { calmSince = now; return; }
		if (now - calmSince < std::chrono::seconds(3)) return;
		TacticalActor* merc = nullptr;
		for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
		{
			auto* actor = GetJa2SoldierRepository().resolve(slot);
			if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE)
			{ merc = actor; break; }
		}
		const UINT16 marker = FIRSTAIDKIT;
		OBJECTTYPE object;
		if (!merc || !marker || !CreateItem(marker, 100, &object)) return;
		done = true;
		const INT32 grid = merc->position().gridNo();
		AddItemToPool(grid, &object, VISIBLE, merc->position().level(), 0, -1);
		std::printf("[co-op entry test] sector marker placed: grid=%d; item=%u\n", int(grid), unsigned(marker));
		std::fflush(stdout);
		return;
	}
	unsigned grid = 0, marker = 0;
	done = true;
	if (std::sscanf(mode, "find:%u:%u", &grid, &marker) != 2) return;
	bool found = false;
	for (UINT32 i = 0; i < guiNumWorldItems && !found; ++i)
		found = gWorldItems[i].fExists && gWorldItems[i].sGridNo == INT32(grid) && gWorldItems[i].object.usItem == marker;
	std::printf("[co-op entry test] sector marker %s: grid=%u; item=%u\n", found ? "found" : "missing", grid, marker);
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_QUEEN_DEATH=1, starter-victory mode): once A9 is
// secured for peaceful play, run the stock death handling of Deidranna as if a
// player merc had just killed her there (the finale itself is not seeded).
void PollQueenDeathSeed()
{
	static bool seeded = false;
	static std::chrono::steady_clock::time_point calmSince{};
	if (seeded || !std::getenv("JA2_COOP_ENTRY_QUEEN_DEATH") || !gfProgramIsRunning) return;
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		NumEnemiesInAnySector(9, 1, 0) || IsJa2TacticalCombatActive())
	{ calmSince = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (calmSince == std::chrono::steady_clock::time_point{}) { calmSince = now; return; }
	if (now - calmSince < std::chrono::seconds(3)) return;
	TacticalActor* merc = nullptr;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE)
		{ merc = actor; break; }
	}
	if (!merc) return;
	seeded = true;
	// As the stock NPC death handling does before it calls the end-game hook.
	gMercProfiles[QUEEN].bMercStatus = MERC_IS_DEAD;
	BeginHandleDeidrannaDeath(merc, merc->position().gridNo(), merc->position().level());
	std::printf("[co-op entry test] queen death seeded\n");
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_CLEAR_ROUTE=1, starter-victory mode): once the
// players are back on the worldless map, B9 and C9 hold no enemies, so a
// two-leg route A9 -> B9 -> C9 meets no battle.
void PollClearRouteSeed()
{
	static bool seeded = false;
	if (seeded || !std::getenv("JA2_COOP_ENTRY_CLEAR_ROUTE") || !gfProgramIsRunning ||
		!GetDedicatedCoopRuntime().campaignEntered() || !IsDedicatedCoopStarterMissionMapReady() || IsJa2TacticalWorldLoaded() ||
		DedicatedCoopArrivalDecisionPending() || StrategicMap[CALCULATE_STRATEGIC_INDEX(9, 1)].fEnemyControlled ||
		NumEnemiesInAnySector(9, 1, 0)) return;
	seeded = true;
	for (const UINT8 y : {UINT8(2), UINT8(3)})
	{
		auto& info = SectorInfo[SECTOR(9, y)];
		info.ubGarrisonID = NO_GARRISON;
		info.ubNumAdmins = info.ubNumTroops = info.ubNumElites = info.ubNumTanks = info.ubNumJeeps = info.ubNumRobots = 0;
		for (GROUP* group = gpGroupList; group;)
		{
			GROUP* next = group->next;
			if (group->usGroupTeam == ENEMY_TEAM && group->ubSectorX == 9 && group->ubSectorY == y) RemoveGroup(group->ubGroupID);
			group = next;
		}
	}
	std::printf("[co-op entry test] route seeded: B9 enemies=%u; C9 enemies=%u\n", unsigned(NumEnemiesInAnySector(9, 2, 0)),
		unsigned(NumEnemiesInAnySector(9, 3, 0)));
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_THROW_ITEM=1, starter-victory mode): once A9 is
// secured for peaceful play, put a stack of three harmless thrown items from
// the game data in the first player merc's hand and pick a walkable target four
// rows away; then report when such an item lies near the target.
void PollThrowItemSeed()
{
	static bool seeded = false, landed = false;
	static UINT16 thrownItem = 0;
	static INT32 target = -1;
	static std::chrono::steady_clock::time_point calmSince{};
	if (!std::getenv("JA2_COOP_ENTRY_THROW_ITEM") || !gfProgramIsRunning) return;
	if (seeded)
	{
		if (landed || !IsJa2TacticalWorldLoaded()) return;
		for (UINT32 i = 0; i < guiNumWorldItems; ++i)
		{
			const auto& item = gWorldItems[i];
			if (item.fExists && item.object.usItem == thrownItem && PythSpacesAway(item.sGridNo, target) <= 4)
			{
				landed = true;
				std::printf("[co-op entry test] thrown item landed: grid=%d; distance=%d\n", int(item.sGridNo),
					int(PythSpacesAway(item.sGridNo, target)));
				std::fflush(stdout);
				return;
			}
		}
		return;
	}
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		NumEnemiesInAnySector(9, 1, 0) || IsJa2TacticalCombatActive())
	{ calmSince = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (calmSince == std::chrono::steady_clock::time_point{}) { calmSince = now; return; }
	if (now - calmSince < std::chrono::seconds(3)) return;
	TacticalActor* merc = nullptr;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE &&
			!actor->position().level()) { merc = actor; break; }
	}
	if (!merc) return;
	seeded = true;
	for (UINT16 item = 1; item < MAXITEMS && !thrownItem; ++item)
		if (Item[item].usItemClass == IC_THROWN && Item[item].ubCursor == TOSSCURS) thrownItem = item;
	for (const int rows : {4, -4})
		for (const int columns : {0, 4, -4})
		{
			const INT32 candidate = merc->position().gridNo() + rows * WORLD_COLS + columns;
			if (target < 0 && candidate >= 0 && candidate < WORLD_MAX && GridNoOnVisibleWorldTile(candidate)) target = candidate;
		}
	OBJECTTYPE object;
	if (!thrownItem || target < 0 || !CreateItems(thrownItem, 100, 3, &object))
	{ Finish(false, "throw fixture found no thrown item or target"); return; }
	merc->inventory()[HANDPOS] = object;
	const auto id = GetJa2TacticalEntityId(*merc);
	std::printf("[co-op entry test] throw seeded: actor=%u:%u; item=%u; count=%u; target=%d\n", unsigned(id.slot),
		unsigned(id.incarnation), unsigned(thrownItem), unsigned(merc->inventory()[HANDPOS].ubNumberOfObjects), int(target));
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_BURST_WEAPON=1, starter-victory mode): once A9
// is secured for peaceful play, put a burst-capable gun from the game data in
// the first player merc's hand, so the stock weapon-mode cycle has modes to
// cycle through.
void PollBurstWeaponSeed()
{
	static bool seeded = false;
	static std::chrono::steady_clock::time_point calmSince{};
	if (seeded || !std::getenv("JA2_COOP_ENTRY_BURST_WEAPON") || !gfProgramIsRunning) return;
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		NumEnemiesInAnySector(9, 1, 0) || IsJa2TacticalCombatActive())
	{ calmSince = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (calmSince == std::chrono::steady_clock::time_point{}) { calmSince = now; return; }
	if (now - calmSince < std::chrono::seconds(3)) return;
	TacticalActor* merc = nullptr;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE)
		{ merc = actor; break; }
	}
	if (!merc) return;
	seeded = true;
	UINT16 gun = 0;
	for (UINT16 item = 1; item < MAXITEMS && !gun; ++item)
		if (Item[item].usItemClass == IC_GUN && Weapon[Item[item].ubClassIndex].ubShotsPerBurst > 0 &&
			!Weapon[Item[item].ubClassIndex].NoSemiAuto) gun = item;
	OBJECTTYPE object;
	if (!gun || !CreateItem(gun, 100, &object)) { Finish(false, "burst weapon fixture found no burst-capable gun"); return; }
	merc->inventory()[HANDPOS] = object;
	merc->attackSelection().weaponMode() = WM_NORMAL;
	const auto id = GetJa2TacticalEntityId(*merc);
	std::printf("[co-op entry test] burst weapon seeded: actor=%u:%u; item=%u\n", unsigned(id.slot), unsigned(id.incarnation),
		unsigned(gun));
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_COMBAT_BURST=1, starter-victory mode): in the
// untouched starter battle, once the four player mercs are in A9 with enemies
// present, put a loaded burst-capable gun from the game data in every player
// merc's hand, set to burst. A burst spends a fixed number of rounds whether it
// hits or not, so the probe can require exactly that many on both clients.
void PollCombatBurstSeed()
{
	static bool seeded = false;
	static std::chrono::steady_clock::time_point since{};
	if (seeded || !std::getenv("JA2_COOP_ENTRY_COMBAT_BURST") || !gfProgramIsRunning) return;
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		!NumEnemiesInAnySector(9, 1, 0))
	{ since = {}; return; }
	std::vector<TacticalActor*> mercs;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE)
			mercs.push_back(actor);
	}
	if (mercs.size() < 4) { since = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (since == std::chrono::steady_clock::time_point{}) { since = now; return; }
	if (now - since < std::chrono::seconds(2)) return;
	seeded = true;
	UINT16 gun = 0;
	for (UINT16 item = 1; item < MAXITEMS && !gun; ++item)
		if (Item[item].usItemClass == IC_GUN && Weapon[Item[item].ubClassIndex].ubShotsPerBurst > 1 &&
			!Weapon[Item[item].ubClassIndex].NoSemiAuto && !EXPLOSIVE_GUN(item) && DefaultMagazine(item)) gun = item;
	if (!gun) { Finish(false, "combat burst fixture found no burst-capable gun"); return; }
	for (auto* merc : mercs)
	{
		OBJECTTYPE object;
		if (!CreateItem(gun, 100, &object)) { Finish(false, "combat burst fixture could not create the gun"); return; }
		merc->inventory()[HANDPOS] = object;
		// Through the stock mode change, which also arms the burst counter.
		merc->attackSelection().weaponMode() = WM_NORMAL;
		for (int step = 0; step < 6 && merc->attackSelection().weaponMode() != WM_BURST; ++step) ChangeWeaponMode(merc);
		if (merc->attackSelection().weaponMode() != WM_BURST)
		{ Finish(false, "combat burst fixture: the stock mode change never reached burst"); return; }
	}
	std::printf("[co-op entry test] combat burst seeded: mercs=%zu; item=%u; burst=%u; magazine=%u\n", mercs.size(),
		unsigned(gun), unsigned(GetShotsPerBurst(&mercs[0]->inventory()[HANDPOS])),
		unsigned(mercs[0]->inventory()[HANDPOS][0]->data.gun.ubGunShotsLeft));
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_COMBAT_GRENADE=1, starter-victory mode): in the
// untouched starter battle, once the four player mercs are in A9 with enemies
// present, put two fragmentation grenades from the game data in every player
// merc's hand. Then report the first explosion of that grenade in the world.
void PollCombatGrenadeSeed()
{
	static bool seeded = false, exploded = false;
	static UINT16 grenade = 0;
	static std::chrono::steady_clock::time_point since{};
	if (!std::getenv("JA2_COOP_ENTRY_COMBAT_GRENADE") || !gfProgramIsRunning) return;
	if (seeded)
	{
		if (exploded || !IsJa2TacticalWorldLoaded()) return;
		for (const auto& explosion : gExplosionData)
			if (explosion.fAllocated && explosion.Params.usItem == grenade)
			{
				exploded = true;
				std::printf("[co-op entry test] grenade exploded: item=%u; grid=%d\n", unsigned(grenade),
					int(explosion.Params.sGridNo));
				std::fflush(stdout);
				return;
			}
		return;
	}
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		!NumEnemiesInAnySector(9, 1, 0))
	{ since = {}; return; }
	std::vector<TacticalActor*> mercs;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE)
			mercs.push_back(actor);
	}
	if (mercs.size() < 4) { since = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (since == std::chrono::steady_clock::time_point{}) { since = now; return; }
	if (now - since < std::chrono::seconds(2)) return;
	seeded = true;
	for (UINT16 item = 1; item < MAXITEMS && !grenade; ++item)
		if (Item[item].usItemClass == IC_GRENADE && Item[item].ubCursor == TOSSCURS &&
			Explosive[Item[item].ubClassIndex].ubType == EXPLOSV_NORMAL && Explosive[Item[item].ubClassIndex].ubDamage > 0)
			grenade = item;
	if (!grenade) { Finish(false, "combat grenade fixture found no fragmentation grenade"); return; }
	for (auto* merc : mercs)
	{
		OBJECTTYPE object;
		if (!CreateItems(grenade, 100, 2, &object)) { Finish(false, "combat grenade fixture could not create grenades"); return; }
		merc->inventory()[HANDPOS] = object;
	}
	std::printf("[co-op entry test] combat grenade seeded: mercs=%zu; item=%u; count=%u\n", mercs.size(),
		unsigned(grenade), unsigned(mercs[0]->inventory()[HANDPOS].ubNumberOfObjects));
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_COMPLEX_ITEMS=1, starter-victory mode): once A9
// is secured for peaceful play, give the first player merc campaign loot the
// co-op inventory used to refuse: a gun from the game data with an attachment
// fitted through the stock AttachObject, a key and a grenade (stock
// AutoPlaceObject), and drop a second grenade at the merc's feet.
void PollComplexItemsSeed()
{
	static bool seeded = false;
	static std::chrono::steady_clock::time_point calmSince{};
	if (seeded || !std::getenv("JA2_COOP_ENTRY_COMPLEX_ITEMS") || !gfProgramIsRunning) return;
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		NumEnemiesInAnySector(9, 1, 0) || IsJa2TacticalCombatActive())
	{ calmSince = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (calmSince == std::chrono::steady_clock::time_point{}) { calmSince = now; return; }
	if (now - calmSince < std::chrono::seconds(3)) return;
	TacticalActor* merc = nullptr;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE)
		{ merc = actor; break; }
	}
	if (!merc) return;
	seeded = true;
	UINT16 gunItem = 0, attachmentItem = 0;
	for (UINT16 gun = 1; gun < MAXITEMS && !attachmentItem; ++gun)
	{
		if (Item[gun].usItemClass != IC_GUN || EXPLOSIVE_GUN(gun) || !DefaultMagazine(gun)) continue;
		for (UINT16 attachment = 1; attachment < MAXITEMS && !attachmentItem; ++attachment)
			if (Item[attachment].usItemClass == IC_MISC && ValidAttachment(attachment, gun))
			{ gunItem = gun; attachmentItem = attachment; }
	}
	UINT16 keyItem = 0, grenadeItem = 0;
	for (UINT16 item = 1; item < MAXITEMS && (!keyItem || !grenadeItem); ++item)
	{
		if (!keyItem && Item[item].usItemClass == IC_KEY) keyItem = item;
		if (!grenadeItem && Item[item].usItemClass == IC_GRENADE && Item[item].ubCursor == TOSSCURS) grenadeItem = item;
	}
	OBJECTTYPE gun, attachment, key, grenade, ground;
	if (!gunItem || !keyItem || !grenadeItem || !CreateItem(gunItem, 100, &gun) || !CreateItem(attachmentItem, 100, &attachment) ||
		!CreateItem(keyItem, 100, &key) || !CreateItem(grenadeItem, 100, &grenade) || !CreateItem(grenadeItem, 100, &ground))
	{ Finish(false, "complex items fixture found no gun/attachment/key/grenade"); return; }
	key[0]->data.key.ubKeyID = 1;
	if (!gun.AttachObject(nullptr, &attachment, FALSE))
	{ Finish(false, "complex items fixture: the stock attach refused"); return; }
	unsigned attached = 0;
	for (const auto& fitted : gun[0]->attachments) if (fitted.exists()) ++attached;
	if (!attached) { Finish(false, "complex items fixture: nothing was attached"); return; }
	// The starter mercs' pockets are well filled: the stock auto-place first,
	// then any empty pocket the stock fit rule accepts.
	const auto place = [&](OBJECTTYPE& object) {
		if (AutoPlaceObject(merc, &object, FALSE)) return true;
		for (INT8 slot = BIGPOCK1POS; slot < static_cast<INT8>(merc->inventory().size()); ++slot)
			if (!merc->inventory()[slot].exists() && CanItemFitInPosition(merc, &object, slot, FALSE) &&
				PlaceObject(merc, slot, &object)) return true;
		return false;
	};
	// The fitted gun goes to the hand, as in the burst fixture; pockets that fit
	// a long gun need worn gear the starter mercs may not have.
	merc->inventory()[HANDPOS] = gun;
	for (auto* object : {&key, &grenade})
		if (!place(*object))
		{
			std::printf("[co-op entry test] complex items fixture could not place item %u\n", unsigned(object->usItem));
			Finish(false, "complex items fixture could not place the items");
			return;
		}
	// The empty pocket the stock fit rule accepts for the grounded grenade.
	int pocket = -1;
	for (INT8 slot = BIGPOCK1POS; slot < static_cast<INT8>(merc->inventory().size()) && pocket < 0; ++slot)
		if (!merc->inventory()[slot].exists() && CanItemFitInPosition(merc, &ground, slot, FALSE)) pocket = slot;
	AddItemToPool(merc->position().gridNo(), &ground, VISIBLE, merc->position().level(), 0, -1);
	const auto id = GetJa2TacticalEntityId(*merc);
	std::printf("[co-op entry test] complex items seeded: actor=%u:%u; gun=%u; attachment=%u; attached=%u; key=%u; grenade=%u; "
		"pocket=%d\n", unsigned(id.slot), unsigned(id.incarnation), unsigned(gunItem), unsigned(attachmentItem), attached,
		unsigned(keyItem), unsigned(grenadeItem), pocket);
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_ATTACH=1, starter-victory mode): once A9 is
// secured for peaceful play, put a gun from the game data in the first player
// merc's hand and an attachment it accepts (stock ValidAttachment) in a pocket
// the stock fit rule allows; then report when the gun really carries it.
void PollAttachSeed()
{
	static bool seeded = false, fitted = false;
	static TacticalEntityId mercId{};
	static UINT16 gunItem = 0, attachmentItem = 0;
	static std::chrono::steady_clock::time_point calmSince{};
	if (!std::getenv("JA2_COOP_ENTRY_ATTACH") || !gfProgramIsRunning) return;
	if (seeded)
	{
		TacticalActor* merc = fitted || !IsJa2TacticalWorldLoaded() ? nullptr : ResolveJa2TacticalEntity(mercId);
		if (!merc || merc->inventory()[HANDPOS].usItem != gunItem) return;
		for (const auto& attached : merc->inventory()[HANDPOS][0]->attachments)
			if (attached.exists() && attached.usItem == attachmentItem)
			{
				fitted = true;
				std::printf("[co-op entry test] attachment fitted: gun=%u; attachment=%u; ap=%d\n", unsigned(gunItem),
					unsigned(attachmentItem), int(merc->actionPoints().current()));
				std::fflush(stdout);
				return;
			}
		return;
	}
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		NumEnemiesInAnySector(9, 1, 0) || IsJa2TacticalCombatActive())
	{ calmSince = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (calmSince == std::chrono::steady_clock::time_point{}) { calmSince = now; return; }
	if (now - calmSince < std::chrono::seconds(3)) return;
	TacticalActor* merc = nullptr;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE)
		{ merc = actor; break; }
	}
	if (!merc) return;
	seeded = true;
	for (UINT16 gun = 1; gun < MAXITEMS && !attachmentItem; ++gun)
	{
		if (Item[gun].usItemClass != IC_GUN || EXPLOSIVE_GUN(gun) || !DefaultMagazine(gun)) continue;
		for (UINT16 attachment = 1; attachment < MAXITEMS && !attachmentItem; ++attachment)
			if (Item[attachment].usItemClass == IC_MISC && ValidAttachment(attachment, gun))
			{ gunItem = gun; attachmentItem = attachment; }
	}
	OBJECTTYPE gun, attachment;
	if (!attachmentItem || !CreateItem(gunItem, 100, &gun) || !CreateItem(attachmentItem, 100, &attachment))
	{ Finish(false, "attach fixture found no gun and attachment"); return; }
	merc->inventory()[HANDPOS] = gun;
	int pocket = -1;
	for (INT8 slot = BIGPOCK1POS; slot < static_cast<INT8>(merc->inventory().size()) && pocket < 0; ++slot)
		if (!merc->inventory()[slot].exists() && CanItemFitInPosition(merc, &attachment, slot, FALSE) &&
			PlaceObject(merc, slot, &attachment)) pocket = slot;
	if (pocket < 0) { Finish(false, "attach fixture could not pocket the attachment"); return; }
	mercId = GetJa2TacticalEntityId(*merc);
	std::printf("[co-op entry test] attach seeded: actor=%u:%u; gun=%u; attachment=%u; pocket=%d; hand=%u\n", unsigned(mercId.slot),
		unsigned(mercId.incarnation), unsigned(gunItem), unsigned(attachmentItem), pocket, unsigned(merc->inventory()[HANDPOS].usItem));
	std::fflush(stdout);
}

// Diagnostic-only (JA2_COOP_ENTRY_BATTLE_SAVE_DIAGNOSTIC=1, starter-victory
// mode): Phase 8.2. At an ordinary player turn of the untouched starter battle
// (turn-based combat, no pending combat action, no interrupt), digest the
// native battle, save it through the native dedicated saver (slot B scratch),
// load it back through the native loader, digest again and report every line
// that differs. Changes no production policy; ends the test host afterwards.
std::vector<std::string> NativeBattleDigest()
{
	std::vector<std::string> lines;
	char line[512];
	std::snprintf(line, sizeof line, "turn team=%d combat=%d turn-based=%d pending=%u world=%u sector=%d,%d,%d",
		int(GetJa2TacticalCurrentTeam()), int(IsJa2TacticalCombatActive()), int(IsJa2TacticalTurnBasedCombat()),
		unsigned(GetJa2PendingTacticalCombatActions()), unsigned(IsJa2TacticalWorldLoaded()), int(gWorldSectorX),
		int(gWorldSectorY), int(gbWorldSectorZ));
	lines.emplace_back(line);
	const auto rng = GetGameSimulationRandomSource()->checkpoint();
	std::snprintf(line, sizeof line, "rng state=%llu raw=%llu", static_cast<unsigned long long>(rng.state),
		static_cast<unsigned long long>(rng.rawValuesGenerated));
	lines.emplace_back(line);
	std::snprintf(line, sizeof line, "clock seconds=%u", unsigned(GetWorldTotalSeconds()));
	lines.emplace_back(line);
	std::snprintf(line, sizeof line, "reinforcements turn=%u enemy=%u/%u militia=%u/%u", unsigned(guiTurnCnt),
		unsigned(guiReinforceTurn), unsigned(guiArrived), unsigned(guiMilitiaReinforceTurn), unsigned(guiMilitiaArrived));
	lines.emplace_back(line);
	unsigned schedules = 0, temporary = 0;
	for (const SCHEDULENODE* schedule = gpScheduleList; schedule; schedule = schedule->next)
	{
		++schedules;
		if (schedule->usFlags & SCHEDULE_FLAGS_TEMPORARY) ++temporary;
	}
	std::snprintf(line, sizeof line, "schedules total=%u temporary=%u", schedules, temporary);
	lines.emplace_back(line);
	for (UINT32 slot = 0; slot < GetJa2SoldierRepository().capacity(); ++slot)
	{
		const TacticalActor* actor = GetJa2SoldierRepository().resolve(slot);
		if (!actor || !actor->roster().active() || !actor->roster().inSector()) continue;
		std::snprintf(line, sizeof line, "actor %u profile=%u team=%u grid=%d level=%d life=%d breath=%d ap=%d anim=%u dir=%u",
			unsigned(slot), unsigned(actor->identity().profile()), unsigned(actor->roster().team()),
			int(actor->position().gridNo()), int(actor->position().level()), int(actor->vitals().health()),
			int(actor->vitals().breath()), int(actor->actionPoints().current()), unsigned(actor->animationPlayback().state()),
			unsigned(actor->position().direction()));
		lines.emplace_back(line);
		// The AI plan object is a stateless wrapper rebuilt on demand, not state.
		std::snprintf(line, sizeof line, "  actor %u ai inProgress=%d action=%d next=%d",
			unsigned(slot), int(actor->aiPlanning().hasActionInProgress()), int(actor->aiPlanning().action()),
			int(actor->aiPlanning().nextAction()));
		lines.emplace_back(line);
		for (std::size_t index = 0; index < actor->inventory().size(); ++index)
		{
			const OBJECTTYPE& object = actor->inventory()[index];
			if (!object.exists()) continue;
			std::snprintf(line, sizeof line, "  actor %u slot %u item=%u count=%u status=%d shots=%u",
				unsigned(slot), unsigned(index), unsigned(object.usItem), unsigned(object.ubNumberOfObjects),
				int(object[0]->data.objectStatus), unsigned(Item[object.usItem].usItemClass == IC_GUN ? object[0]->data.gun.ubGunShotsLeft : 0));
			lines.emplace_back(line);
		}
	}
	unsigned items = 0;
	for (UINT32 i = 0; i < guiNumWorldItems; ++i)
		if (gWorldItems[i].fExists)
		{
			++items;
			std::snprintf(line, sizeof line, "ground grid=%d item=%u count=%u", int(gWorldItems[i].sGridNo),
				unsigned(gWorldItems[i].object.usItem), unsigned(gWorldItems[i].object.ubNumberOfObjects));
			lines.emplace_back(line);
		}
	std::snprintf(line, sizeof line, "ground items=%u", items);
	lines.emplace_back(line);
	return lines;
}

void PollBattleSaveDiagnostic()
{
	static bool done = false;
	static std::chrono::steady_clock::time_point since{};
	if (done || !std::getenv("JA2_COOP_ENTRY_BATTLE_SAVE_DIAGNOSTIC") || !gfProgramIsRunning) return;
	// The Phase 8.3 policy over the native readiness evidence decides when.
	static Ja2TacticalBattleCheckpointReason lastReason = Ja2TacticalBattleCheckpointReason::NotObserved;
	const auto readiness = CaptureJa2TacticalCheckpointReadiness();
	const auto reason = EvaluateJa2TacticalBattleCheckpoint(readiness);
	static std::uint32_t lastHazards = 0;
	static std::uint16_t lastBusy = UINT16_MAX;
	if ((reason != lastReason || readiness.actors.hazards != lastHazards || readiness.actors.firstBusySlot != lastBusy) &&
		IsJa2TacticalWorldLoaded())
	{
		lastReason = reason; lastHazards = readiness.actors.hazards; lastBusy = readiness.actors.firstBusySlot;
		const TacticalActor* busy = readiness.actors.firstBusySlot != UINT16_MAX
			? GetJa2SoldierRepository().resolve(readiness.actors.firstBusySlot) : nullptr;
		if (busy)
			std::printf("[co-op entry test] battle save diagnostic: ai of %u: inProgress=%d action=%d next=%d plan=%d\n",
				unsigned(readiness.actors.firstBusySlot), int(busy->aiPlanning().hasActionInProgress()),
				int(busy->aiPlanning().action()), int(busy->aiPlanning().nextAction()), int(busy->aiPlan().hasPlan()));
		std::printf("[co-op entry test] battle save diagnostic: readiness %s; team=%d; busy=%u (of %u) first=%u team=%d anim=%u; hazards=0x%x\n",
			Ja2TacticalBattleCheckpointReasonName(reason), int(readiness.turn.currentTeam), unsigned(readiness.actors.busyActors),
			unsigned(readiness.actors.inSectorActors), unsigned(readiness.actors.firstBusySlot),
			busy ? int(busy->roster().team()) : -1, busy ? unsigned(busy->animationPlayback().state()) : 0u,
			unsigned(readiness.actors.hazards));
		std::fflush(stdout);
	}
	if (reason != Ja2TacticalBattleCheckpointReason::None) { since = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (since == std::chrono::steady_clock::time_point{}) { since = now; return; }
	if (now - since < std::chrono::seconds(3)) return;
	done = true;
	const auto before = NativeBattleDigest();
	std::printf("[co-op entry test] battle save diagnostic: digest before has %zu lines\n", before.size());
	const bool saved = SaveDedicatedCampaignGame(DedicatedCampaignSlot::B);
	const auto afterSave = NativeBattleDigest();
	const bool loaded = saved && LoadDedicatedCampaignGame(DedicatedCampaignSlot::B);
	const auto after = loaded ? NativeBattleDigest() : std::vector<std::string>{};
	std::printf("[co-op entry test] battle save diagnostic: saved=%d loaded=%d\n", int(saved), int(loaded));
	const auto report = [](const char* label, const std::vector<std::string>& left, const std::vector<std::string>& right) {
		unsigned differences = 0;
		for (std::size_t i = 0; i < std::max(left.size(), right.size()); ++i)
		{
			const std::string a = i < left.size() ? left[i] : "<none>";
			const std::string b = i < right.size() ? right[i] : "<none>";
			if (a == b) continue;
			if (differences++ < 40)
				std::printf("[co-op entry test] battle save diagnostic %s: - %s | + %s\n", label, a.c_str(), b.c_str());
		}
		std::printf("[co-op entry test] battle save diagnostic %s: %u differing lines\n", label, differences);
		return differences;
	};
	const unsigned saveChanges = report("save", before, afterSave);
	const unsigned loadChanges = loaded ? report("load", before, after) : 1;
	std::fflush(stdout);
	Finish(saved && loaded && saveChanges == 0 && loadChanges == 0,
		saved && loaded ? "battle save diagnostic compared the digests" : "battle save diagnostic could not save or load");
}

// Fixture-only (JA2_COOP_ENTRY_LOCKED_DOOR=1, starter-victory mode): once A9
// is secured for peaceful play, lock the closed door nearest a player merc and
// give that merc a locksmith kit; log the door and the tile to stand on. The
// probe walks there and picks the lock through the shipping controller. Then
// report every change of that door's lock.
void PollLockedDoorSeed()
{
	static bool seeded = false;
	static std::int32_t doorGrid = -1;
	static int lastLocked = -1;
	static std::chrono::steady_clock::time_point calmSince{};
	if (!std::getenv("JA2_COOP_ENTRY_LOCKED_DOOR") || !gfProgramIsRunning) return;
	if (seeded)
	{
		const DOOR* door = doorGrid >= 0 ? FindDoorInfoAtGridNo(doorGrid) : nullptr;
		const int locked = door ? (door->fLocked ? 1 : 0) : -1;
		if (locked != lastLocked)
		{
			std::printf("[co-op entry test] seeded door locked=%d\n", locked);
			std::fflush(stdout);
			lastLocked = locked;
		}
		return;
	}
	if (!IsJa2TacticalWorldLoaded() || gWorldSectorX != 9 || gWorldSectorY != 1 || gbWorldSectorZ ||
		NumEnemiesInAnySector(9, 1, 0) || IsJa2TacticalCombatActive())
	{ calmSince = {}; return; }
	const auto now = std::chrono::steady_clock::now();
	if (calmSince == std::chrono::steady_clock::time_point{}) { calmSince = now; return; }
	if (now - calmSince < std::chrono::seconds(3)) return;
	TacticalActor* merc = nullptr;
	for (SoldierID slot = gTacticalStatus.Team[OUR_TEAM].bFirstID; slot <= gTacticalStatus.Team[OUR_TEAM].bLastID; ++slot)
	{
		auto* actor = GetJa2SoldierRepository().resolve(slot);
		if (actor && actor->roster().active() && actor->roster().inSector() && actor->vitals().health() >= OKLIFE &&
			!actor->position().level()) { merc = actor; break; }
	}
	if (!merc) return;
	seeded = true;
	DOOR* best = nullptr; INT32 bestStand = -1; INT16 bestDistance = 32767;
	for (UINT8 i = 0; i < gubNumDoors; ++i)
	{
		DOOR* door = &DoorTable[i];
		STRUCTURE* structure = FindStructure(door->sGridNo, STRUCTURE_ANYDOOR);
		STRUCTURE* base = structure ? FindBaseStructure(structure) : nullptr;
		if (!base || base->sGridNo != door->sGridNo || (base->fFlags & STRUCTURE_OPEN)) continue;
		UINT8 direction = 0;
		const INT32 stand = FindAdjacentGridEx(merc, door->sGridNo, &direction, nullptr, FALSE, TRUE);
		if (stand < 0 || stand >= WORLD_MAX) continue;
		const INT16 distance = PythSpacesAway(merc->position().gridNo(), stand);
		if (distance < bestDistance) { best = door; bestStand = stand; bestDistance = distance; }
	}
	UINT16 kit = 0;
	for (UINT16 item = 1; item < MAXITEMS && !kit; ++item)
		if (ItemIsLocksmithKit(item)) kit = item;
	OBJECTTYPE object;
	if (!best || !kit || !CreateItem(kit, 100, &object) || !AutoPlaceObject(merc, &object, TRUE))
	{ Finish(false, "locked door fixture found no door, kit or inventory room"); return; }
	// The easiest ordinary lock in the game data and a skilled merc: the stock
	// lockpick roll still decides, but the probe need not retry for long.
	UINT8 lock = 0; UINT8 difficulty = 255;
	for (UINT16 id = 0; id < NUM_LOCKS; ++id)
		if (id != LOCK_UNOPENABLE && (LockTable[id].ubLockType == LOCK_REGULAR || LockTable[id].ubLockType == LOCK_PADLOCK) &&
			LockTable[id].ubPickDifficulty < difficulty) { lock = static_cast<UINT8>(id); difficulty = LockTable[id].ubPickDifficulty; }
	merc->statistics().mechanical() = 100;
	best->fLocked = TRUE;
	best->ubLockID = lock;
	best->ubTrapID = NO_TRAP;
	best->ubTrapLevel = 0;
	std::printf("[co-op entry test] seeded lock id=%u; pick difficulty=%u\n", unsigned(lock), unsigned(difficulty));
	doorGrid = best->sGridNo;
	STRUCTURE* structure = FindBaseStructure(FindStructure(best->sGridNo, STRUCTURE_ANYDOOR));
	const auto id = GetJa2TacticalEntityId(*merc);
	std::printf("[co-op entry test] locked door seeded: actor=%u:%u; door=%d; structure=%u; stand=%d; distance=%d; kit=%u\n",
		unsigned(id.slot), unsigned(id.incarnation), int(best->sGridNo), structure ? unsigned(structure->usStructureID) : 0u,
		int(bestStand), int(bestDistance), unsigned(kit));
	std::fflush(stdout);
}

// Fixture-only (JA2_COOP_ENTRY_REINFORCEMENT_BATTLE=1, starter-victory mode):
// once the players are back on the worldless map, militia hold A9 and an army
// garrison holds B9, so the squad's arrival in B9 asks the stock militia
// reinforcement question. Reinforcements are a difficulty setting; force it on.
void PollReinforcementBattleSeed()
{
	static bool seeded = false;
	if (seeded || !std::getenv("JA2_COOP_ENTRY_REINFORCEMENT_BATTLE") || !gfProgramIsRunning ||
		!GetDedicatedCoopRuntime().campaignEntered() || !IsDedicatedCoopStarterMissionMapReady() || IsJa2TacticalWorldLoaded() ||
		DedicatedCoopArrivalDecisionPending() || StrategicMap[CALCULATE_STRATEGIC_INDEX(9, 1)].fEnemyControlled ||
		NumEnemiesInAnySector(9, 1, 0)) return;
	seeded = true;
	gGameExternalOptions.gfAllowReinforcements = TRUE;
	StrategicAddMilitiaToSector(9, 1, REGULAR_MILITIA, 6);
	auto& battle = SectorInfo[SECTOR(9, 2)];
	battle.ubGarrisonID = NO_GARRISON;
	battle.ubNumAdmins = battle.ubNumElites = battle.ubNumTanks = battle.ubNumJeeps = battle.ubNumRobots = 0;
	battle.ubNumTroops = 3;
	StrategicMap[CALCULATE_STRATEGIC_INDEX(9, 2)].fEnemyControlled = TRUE;
	std::printf("[co-op entry test] reinforcement battle seeded: militia_a9=%u; enemies_b9=%u; clock=%u\n",
		unsigned(NumNonPlayerTeamMembersInSector(9, 1, MILITIA_TEAM)), unsigned(NumEnemiesInAnySector(9, 2, 0)), GetWorldTotalSeconds());
	std::fflush(stdout);
}

std::chrono::steady_clock::time_point takenAt{};
bool takenApplied = false;

void PollMilitiaAttackSeed()
{
	static bool seeded = false;
	if (seeded && takenAt != std::chrono::steady_clock::time_point{} && !takenApplied &&
		std::chrono::steady_clock::now() - takenAt > std::chrono::seconds(3) && !IsJa2TacticalWorldLoaded())
	{
		takenApplied = true;
		const BOOLEAN lost = SetThisSectorAsEnemyControlled(10, 1, 0, TRUE);
		std::printf("[co-op entry test] town taken by the stock strategic call: result=%u\n", unsigned(lost));
		std::fflush(stdout);
	}
	static int lastControl = -1, lastPending = -1;
	const char* mode = std::getenv("JA2_COOP_ENTRY_MILITIA_ATTACK");
	if (seeded && mode)
	{
		// Observation only: who holds the attacked sector, and whether a native
		// message box is waiting on the server.
		const bool town = std::strncmp(mode, "town-", 5) == 0;
		const INT16 x = town ? 10 : 9, y = town ? 1 : 2;
		const int control = StrategicMap[CALCULATE_STRATEGIC_INDEX(x, y)].fEnemyControlled ? 1 : 0;
		const int pending = GetPendingNewScreen() == MSG_BOX_SCREEN || GetCurrentScreen() == MSG_BOX_SCREEN ? 1 : 0;
		if (control != lastControl || pending != lastPending)
		{
			std::printf("[co-op entry test] militia target enemy-controlled=%d; message-box=%d; clock=%u\n",
				control, pending, GetWorldTotalSeconds());
			std::fflush(stdout);
			lastControl = control; lastPending = pending;
		}
	}
	if (seeded || !mode || !gfProgramIsRunning || !GetDedicatedCoopRuntime().campaignEntered() ||
		!IsDedicatedCoopStarterMissionMapReady() || IsJa2TacticalWorldLoaded() || DedicatedCoopArrivalDecisionPending() ||
		StrategicMap[CALCULATE_STRATEGIC_INDEX(9, 1)].fEnemyControlled || NumEnemiesInAnySector(9, 1, 0)) return;
	seeded = true;
	const bool held = std::strcmp(mode, "held") == 0;
	// town-defeated: Omerta (A10, a town) instead of wilderness B9, attacked from A11.
	// town-taken: undefended Omerta retaken without a battle, through the stock
	// call the strategic AI uses (no militia, no attacker).
	const bool taken = std::strcmp(mode, "town-taken") == 0;
	const bool town = taken || std::strcmp(mode, "town-defeated") == 0;
	if (!held && !town && std::strcmp(mode, "defeated") != 0) { Finish(false, "unknown militia attack mode"); return; }
	const UINT8 targetX = town ? 10 : 9, targetY = town ? 1 : 2, fromX = town ? 11 : 9, fromY = town ? 1 : 3;
	for (const auto sector : {SECTOR(targetX, targetY), SECTOR(fromX, fromY)})
	{
		auto& info = SectorInfo[sector];
		info.ubGarrisonID = NO_GARRISON;
		info.ubNumAdmins = info.ubNumTroops = info.ubNumElites = info.ubNumTanks = info.ubNumJeeps = info.ubNumRobots = 0;
	}
	StrategicMap[CALCULATE_STRATEGIC_INDEX(targetX, targetY)].fEnemyControlled = FALSE;
	// A town only counts as held through the stock liberation path.
	if (town) SetThisSectorAsPlayerControlled(targetX, targetY, 0, TRUE);
	if (taken)
	{
		std::printf("[co-op entry test] militia attack seeded: mode=%s; militia=0; attackers=0; group=0; clock=%u\n", mode,
			GetWorldTotalSeconds());
		std::fflush(stdout);
		takenAt = std::chrono::steady_clock::now();
		return;
	}
	StrategicAddMilitiaToSector(targetX, targetY, held ? ELITE_MILITIA : GREEN_MILITIA, held ? 12 : 1);
	GROUP* attacker = CreateNewEnemyGroupDepartingFromSector(SECTOR(fromX, fromY), 0, held ? 3 : 0, held ? 0 : 8, 0, 0, 0);
	if (!attacker) { Finish(false, "fixture militia attacker creation failed"); return; }
	MoveSAIGroupToSector(&attacker, SECTOR(targetX, targetY), DIRECT, ASSAULT);
	if (!attacker || attacker->ubNextX != targetX || attacker->ubNextY != targetY)
	{ Finish(false, "fixture militia attacker did not set out for its target"); return; }
	std::printf("[co-op entry test] militia attack seeded: mode=%s; militia=%u; attackers=%u; group=%u; clock=%u\n", mode,
		unsigned(NumNonPlayerTeamMembersInSector(targetX, targetY, MILITIA_TEAM)), unsigned(attacker->ubGroupSize), unsigned(attacker->ubGroupID),
		GetWorldTotalSeconds());
	std::fflush(stdout);
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
	PollMilitiaAttackSeed();
	PollReinforcementBattleSeed();
	PollLockedDoorSeed();
	PollBurstWeaponSeed();
	PollThrowItemSeed();
	PollClearRouteSeed();
	PollQueenDeathSeed();
	PollSectorMarker();
	PollKnockOutTrigger();
	PollCombatBurstSeed();
	PollCombatGrenadeSeed();
	PollComplexItemsSeed();
	PollAttachSeed();
	PollBattleSaveDiagnostic();
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
	if (mode && std::strcmp(mode, "plain-resume") == 0 && GetDedicatedServerOptions().campaignAction == DedicatedCampaignAction::Resume)
	{
		// No seed mutation: the resumed campaign plays as shipped; only the
		// read-only observers (e.g. JA2_COOP_ENTRY_SECTOR_MARKER=find) run.
		std::printf("[co-op entry test] plain resume; no seed\n"); std::fflush(stdout);
		return;
	}
	if (GetDedicatedServerOptions().campaignAction != DedicatedCampaignAction::Create)
	{ Finish(false, "only disposable newly created campaigns are supported"); return; }
	RunEntryFixture();
}
