// Native turn decay passes a just-expired knowledge value to UpdatePublic.
// Forgetting must clear knowledge/location without indexing the comparison
// matrix with the transient value outside its supported range.
#include "types.h"
#include "GameContext.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalWorldAdapter.h"
#include "Overhead.h"
#include "opplist.h"
#include "Isometric Utils.h"

// The same native turn callback invoked by BeginTeamTurn.
void DecayPublicOpplist(INT8 team);
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }

int main(int argc, char** argv)
{
    const bool seen = argc == 2 && std::strcmp(argv[1], "--seen") == 0;
    if (argc > 2 || (argc == 2 && !seen)) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    int failures = 0;
    const auto check = [&](bool condition, const char* message)
    { if (!condition) { ++failures; std::printf("FAIL: %s\n", message); } };
    auto& game = GetGameContext();
    check(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
        "native runtime starts");
    auto& repository = GetJa2SoldierRepository(); repository.initializeSlots();
    ResetJa2TacticalActorRosters();
    auto& opponent = *repository.resolve(0);
    opponent.identity().id() = SoldierID{0}; opponent.identity().incarnation() = 1;
    opponent.roster().active() = opponent.roster().inSector() = TRUE;
    opponent.roster().team() = OUR_TEAM;
    opponent.vitals().health() = opponent.vitals().maximumHealth() = 80;
    check(AdoptJa2TacticalEntity(opponent), "opponent has a real native identity");
    const auto actor = GetJa2TacticalEntityId(opponent);
    check(AddJa2ActiveTacticalActor(actor) >= 0, "opponent enters the real turn roster");
    if (failures) return 1;
    // No observers are needed to test memory expiry. Empty team bounds avoid
    // unrelated line-of-sight and watched-location work requiring map assets.
    for (auto& team : gTacticalStatus.Team)
    { team.bFirstID = SoldierID{1}; team.bLastID = SoldierID{0}; }
    RestoreJa2TacticalTurnState(TURNBASED | INCOMBAT, ENEMY_TEAM, 0);
    gbPublicOpplist[CIV_TEAM][0] = HEARD_LAST_TURN;
    gsPublicLastKnownOppLoc[CIV_TEAM][0] = 4321;
    gbPublicLastKnownOppLevel[CIV_TEAM][0] = 1;
    const auto seed = [&](INT8 knowledge)
    {
        gbPublicOpplist[ENEMY_TEAM][0] = knowledge;
        gsPublicLastKnownOppLoc[ENEMY_TEAM][0] = 1234;
        gbPublicLastKnownOppLevel[ENEMY_TEAM][0] = 1;
    };
    seed(seen ? OLDEST_SEEN_VALUE : OLDEST_HEARD_VALUE);
    gTacticalStatus.Team[ENEMY_TEAM].ubLastMercToRadio = SoldierID{0};
    DecayPublicOpplist(ENEMY_TEAM);
    check(gbPublicOpplist[ENEMY_TEAM][0] == NOT_HEARD_OR_SEEN &&
        gsPublicLastKnownOppLoc[ENEMY_TEAM][0] == NOWHERE && gbPublicLastKnownOppLevel[ENEMY_TEAM][0] == 0,
        "actual turn decay forgets expired knowledge and its exact location");
    DecayPublicOpplist(ENEMY_TEAM);
    check(gTacticalStatus.Team[ENEMY_TEAM].ubLastMercToRadio == NOBODY,
        "the following empty turn clears the old radio caller");
    for (INT8 knowledge : {HEARD_THIS_TURN, SEEN_THIS_TURN, SEEN_CURRENTLY})
    {
        seed(knowledge); DecayPublicOpplist(ENEMY_TEAM);
        const INT8 expected = knowledge == HEARD_THIS_TURN ? HEARD_LAST_TURN :
            knowledge == SEEN_THIS_TURN ? SEEN_LAST_TURN : SEEN_CURRENTLY;
        check(gbPublicOpplist[ENEMY_TEAM][0] == expected && gsPublicLastKnownOppLoc[ENEMY_TEAM][0] == 1234 &&
            gbPublicLastKnownOppLevel[ENEMY_TEAM][0] == 1,
            "unexpired and currently visible knowledge retains normal ageing and location");
    }
    seed(SEEN_CURRENTLY);
    UpdatePublic(ENEMY_TEAM, SoldierID{0}, HEARD_THIS_TURN, 2345, 0);
    check(gbPublicOpplist[ENEMY_TEAM][0] == SEEN_CURRENTLY && gsPublicLastKnownOppLoc[ENEMY_TEAM][0] == 2345,
        "older knowledge cannot replace current sight, while native location updates remain intact");
    seed(HEARD_LAST_TURN);
    UpdatePublic(ENEMY_TEAM, SoldierID{0}, SEEN_THIS_TURN, 3456, 0);
    check(gbPublicOpplist[ENEMY_TEAM][0] == SEEN_THIS_TURN && gsPublicLastKnownOppLoc[ENEMY_TEAM][0] == 3456,
        "newer sight still replaces older hearing");
    check(gbPublicOpplist[CIV_TEAM][0] == HEARD_LAST_TURN && gsPublicLastKnownOppLoc[CIV_TEAM][0] == 4321 &&
        gbPublicLastKnownOppLevel[CIV_TEAM][0] == 1 && GetJa2TacticalEntityId(opponent) == actor &&
        opponent.vitals().health() == 80, "decay preserves other teams and the opponent identity/health");
    ResetJa2TacticalActorRosters();
    ReleaseJa2TacticalEntity(opponent);
    std::printf("Native public %s knowledge expiry: %s\n", seen ? "seen" : "heard", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
