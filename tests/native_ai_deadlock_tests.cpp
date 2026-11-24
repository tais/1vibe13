// Exercise the real AI watchdog and native actor handoff without game assets.
#include "types.h"
#include "GameContext.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalActorStateFlags.h"
#include "Soldier Profile Constants.h"
#include "Animation Data.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Overhead.h"
#include "ai.h"
#include "AIList.h"
#include "GameSettings.h"
#include "Font.h"
#include "Font Control.h"
#include "MemMan.h"
#include "message.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>
#include <vector>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }
extern time_t gtTimeSinceMercAIStart;
extern BOOLEAN gfUIInDeadlock;
extern HVOBJECT FontObjs[MAX_FONTS];

int main(int argc, char** argv)
{
    const bool interactive = argc == 2 && std::strcmp(argv[1], "--interactive") == 0;
    if (argc > 2 || (argc == 2 && !interactive)) return 2;
    gfDedicatedServer = !interactive;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int failures = 0;
    const auto check = [&](bool condition, const char* message)
    { if (!condition) { ++failures; std::printf("FAIL: %s\n", message); } };
    auto& game = GetGameContext();
    check(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
        "native runtime starts");
    check(InitializeMemoryManager(), "native memory starts");
    auto* table = CreateEnglishTransTable();
    if (!table || !InitializeFontManager(8, table)) return 1;
    std::vector<ETRLEObject> glyphs(table->usNumberOfSymbols);
    MemFree(table);
    for (auto& glyph : glyphs) { glyph.usWidth = 6; glyph.usHeight = 10; }
    SGPVObject font{};
    font.usNumberOfObjects = static_cast<UINT16>(glyphs.size());
    font.pETRLEObject = glyphs.data();
    // Timeout diagnostics still go through real native message formatting.
    FontObjs[0] = &font;
    TINYFONT1 = 0;
    FONT10ARIAL = 0;

    auto& repository = GetJa2SoldierRepository();
    repository.initializeSlots();
    ResetJa2TacticalActorRosters();
    auto& stuck = *repository.resolve(10);
    auto& next = *repository.resolve(11);
    for (auto* actor : {&stuck, &next})
    {
        actor->identity().id() = SoldierID{actor == &stuck ? 10 : 11};
        actor->identity().incarnation() = 1;
        actor->identity().profile() = NO_PROFILE;
        actor->identity().bodyType() = REGMALE;
        actor->roster().active() = actor->roster().inSector() = TRUE;
        actor->roster().team() = ENEMY_TEAM;
        actor->roster().side() = 1;
        actor->vitals().health() = actor->vitals().maximumHealth() = 80;
        actor->vitals().breath() = 100;
        actor->awareness().visibility() = -1;
        actor->aiBehavior().alertStatus() = STATUS_RED;
        actor->aiBehavior().newSituation() = NOT_NEW_SITUATION;
        actor->turnState().moved() = FALSE;
        check(AdoptJa2TacticalEntity(*actor), "actor has a real native identity");
        check(AddJa2ActiveTacticalActor(GetJa2TacticalEntityId(*actor)) >= 0, "actor joins native roster");
    }
    for (auto& team : gTacticalStatus.Team)
    { team.bFirstID = SoldierID{1}; team.bLastID = SoldierID{0}; }
    gTacticalStatus.Team[ENEMY_TEAM].bFirstID = SoldierID{10};
    gTacticalStatus.Team[ENEMY_TEAM].bLastID = SoldierID{11};
    gTacticalStatus.Team[LAST_TEAM].bLastID = SoldierID{11};
    RestoreJa2TacticalTurnState(TURNBASED | INCOMBAT, ENEMY_TEAM, 1);
    stuck.status().flags() = SOLDIER_UNDERAICONTROL;
    stuck.aiPlanning().action() = AI_ACTION_MOVE_TO_CLIMB;
    stuck.aiPlanning().actionInProgress() = TRUE;
    stuck.movement().setOutOfActionPoints(true);
    ClearAIList();
    check(InsertIntoAIList(next.identity().id(), STATUS_RED), "next actor queues for native handoff");
    if (failures) return 1;

    gGameExternalOptions.gubDeadLockDelay = 30;
    gtTimeSinceMercAIStart = std::time(nullptr);
    HandleSoldierAI(&stuck);
    check(!gfUIInDeadlock && !stuck.turnState().moved() &&
        (stuck.status().flags() & SOLDIER_UNDERAICONTROL) && GetJa2PendingTacticalCombatActions() == 1,
        "unexpired watchdog preserves the active turn and pending work");
    const auto recoveryStartedAt = std::time(nullptr);
    gtTimeSinceMercAIStart = recoveryStartedAt - 120;
    HandleSoldierAI(&stuck);
    bool expectUiHold = false;
#ifdef JA2TESTVERSION
    expectUiHold = interactive;
#endif
    if (expectUiHold)
    {
        check(gfUIInDeadlock && !stuck.turnState().moved() &&
            (stuck.status().flags() & SOLDIER_UNDERAICONTROL) &&
            !(next.status().flags() & SOLDIER_UNDERAICONTROL) &&
            GetJa2PendingTacticalCombatActions() == 1 &&
            stuck.aiPlanning().action() == AI_ACTION_MOVE_TO_CLIMB,
            "interactive Debug preserves its local deadlock inspection state");
    }
    else
    {
        check(!gfUIInDeadlock && stuck.turnState().moved() &&
            !(stuck.status().flags() & SOLDIER_UNDERAICONTROL),
            "dedicated timeout releases the stuck actor without waiting for UI");
        check(stuck.aiPlanning().action() == AI_ACTION_NONE &&
            stuck.aiPlanning().lastAction() == AI_ACTION_MOVE_TO_CLIMB &&
            !stuck.aiPlanning().actionInProgress() && GetJa2PendingTacticalCombatActions() == 0,
            "normal native recovery cancels the stuck action and clears pending work");
        check((next.status().flags() & SOLDIER_UNDERAICONTROL) && !next.turnState().moved() &&
            GetJa2TacticalCurrentTeam() == ENEMY_TEAM && gtTimeSinceMercAIStart >= recoveryStartedAt,
            "normal native handoff starts the next enemy and resets its watchdog");
    }
    check(stuck.vitals().health() == 80 && next.vitals().health() == 80,
        "watchdog recovery does not damage either actor");
    ClearAIList();
    ResetJa2TacticalActorRosters();
    ReleaseJa2TacticalEntity(stuck);
    ReleaseJa2TacticalEntity(next);
    ClearTacticalMessageQueue();
    FreeGlobalMessageList();
    FontObjs[0] = nullptr;
    ShutdownFontManager();
    std::printf("Native %s AI watchdog: %s\n", interactive ? "interactive" : "dedicated", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
