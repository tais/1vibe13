#include "DedicatedCoopAttack.h"
#include "DedicatedServerOptions.h"
#include "TacticalActorRouteExecution.h"
// Regression for off-turn hit recovery stranding a remote player's shot.
// Exercise the real hit-recovery READY transition. Animation pixels
// are inert memory fixtures, while AP accounting, interrupt resolution and the
// native animation/combat-action completion callbacks remain production code.
#include "Animation Cache.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Items.h"
#include "MemMan.h"
#include "Overhead.h"
#include "PATHAI.H"
#include "Points.h"
#include "renderworld.h"
#include "Soldier Profile Constants.h"
#include "Soldier Ani.h"
#include "SoldierRepository.h"
#include "Squads.h"
#include "TacticalActor.h"
#include "TacticalActorAnimationTransitions.h"
#include "TacticalActorAnimationState.h"
#include "TacticalActorInterrupts.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "Weapons.h"
#include "World Tile Map.h"
#include "connect.h"
#include "opplist.h"
#include "random.h"
#include "sgp.h"
#include "worlddef.h"
#include <Engine/Core/SimulationRandom.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern UINT16 gubAnimSurfaceIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
extern UINT16 gubAnimSurfaceItemSubIndex[TOTALBODYTYPES][NUMANIMATIONSTATES];
int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE, gfDedicatedServer = TRUE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE, gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message)
{ std::fprintf(stderr, "ShutdownWithErrorBox: %s\n", message ? message : ""); std::exit(1); }

namespace
{
int failures = 0;
#define CHECK(condition, message) do { if (!(condition)) { \
    ++failures; std::printf("FAIL %d: %s\n", __LINE__, message); } } while (false)
}

int main(int argc, char** argv)
{
    const bool pointsOnly = argc == 2 && std::strcmp(argv[1], "--points-only") == 0;
    const bool attackLifecycle = argc == 2 && std::strcmp(argv[1], "--attack-lifecycle") == 0;
    if (argc > 2 || (argc == 2 && !pointsOnly && !attackLifecycle)) return 2;
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    CHECK(InstallGameSimulationRandom(20260922) == GameSimulationRandomInstallError::None,
        "native deterministic RNG installed");
    GameContext& game = GetGameContext();
    CHECK(game.beginInitialization() && game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning(),
        "native interrupt regression runtime starts");
    if (failures) return 1;
    auto& repository = GetJa2SoldierRepository();
    repository.initializeSlots(); ResetJa2TacticalActorRosters(); ResetJa2TacticalInterruptForNewWorld();
    CHECK(AllocateWorldTileMap(static_cast<std::uint32_t>(WORLD_MAX)), "real logical world allocated");
    gubWorldMovementCosts = static_cast<UINT8 (*)[MAXDIR][2]>(
        MemAlloc(static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2));
    if (!gubWorldMovementCosts || !GetWorldTileMapSize()) return 1;
    std::memset(gubWorldMovementCosts, TRAVELCOST_BLOCKED, static_cast<std::size_t>(WORLD_MAX) * MAXDIR * 2);
    InitRenderParams(0); NotifyJa2TacticalWorldLoaded(1);
    RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
    gbPlayerNum = OUR_TEAM; gusSelectedSoldier = SoldierID{0};
    is_networked = is_client = is_server = false;
    gGameOptions.fNewTraitSystem = FALSE;
    gGameExternalOptions.fImprovedInterruptSystem = TRUE;
    gGameExternalOptions.ubBasicPercentRegisterValueIIS = 100;
    gGameExternalOptions.ubPercentRegisterValuePerLevelIIS = 0;
    gGameExternalOptions.ubBasicReactionTimeLengthIIS = 100;
    gGameExternalOptions.ubEnergyCostForWeaponWeight = 0;
    gGameExternalOptions.fBackGround = FALSE;
    gGameExternalOptions.fDisease = FALSE;
    gGameExternalOptions.ubAllowAlternativeWeaponHolding = 0;
    for (auto& team : gTacticalStatus.Team)
    {
        team.bFirstID = SoldierID{1}; team.bLastID = SoldierID{0};
    }
    gTacticalStatus.Team[OUR_TEAM].bFirstID = SoldierID{0};
    gTacticalStatus.Team[OUR_TEAM].bLastID = SoldierID{3};
    gTacticalStatus.Team[ENEMY_TEAM].bFirstID = SoldierID{263};
    gTacticalStatus.Team[ENEMY_TEAM].bLastID = SoldierID{263};
    gTacticalStatus.fEnemyInSector = TRUE;
    APBPConstants[AP_MAXIMUM] = 100;
    APBPConstants[AP_MIN_LIMIT] = -100;
    APBPConstants[BP_RATIO_RED_PTS_TO_NORMAL] = 100;

    TacticalActor& selected = *repository.resolve(0);
    TacticalActor& shooter = *repository.resolve(3);
    TacticalActor& target = *repository.resolve(263);
    for (TacticalActor* actor : {&selected, &shooter, &target})
    {
        const UINT16 slot = actor == &selected ? 0 : actor == &shooter ? 3 : 263;
        actor->identity().id() = SoldierID{slot}; actor->identity().incarnation() = slot + 1;
        actor->identity().profile() = NO_PROFILE; actor->identity().bodyType() = REGMALE;
        actor->roster().active() = actor->roster().inSector() = TRUE;
        actor->roster().team() = actor == &target ? ENEMY_TEAM : OUR_TEAM;
        actor->roster().side() = actor == &target ? 1 : 0;
        actor->aiBehavior().neutral() = FALSE; actor->assignment().current() = FIRST_SQUAD;
        actor->position().gridNo() = 7963 + (actor == &target ? 1 : actor == &shooter ? 160 : 0);
        actor->position().level() = 0; actor->position().direction() = SOUTH;
        actor->pathing().desiredDirection() = SOUTH;
        actor->animationPlayback().state() = STANDING; actor->animationPlayback().surface() = 0;
        actor->animationIntent().clearPendingAnimations(); actor->pendingAction().clearAction();
        actor->movement().mode() = WALKING;
        actor->vitals().health() = actor->vitals().maximumHealth() = 100;
        actor->vitals().breath() = actor->vitals().maximumBreath() = 100;
        actor->actionPoints().current() = actor == &target ? -30 : 100;
        actor->statistics().experienceLevel() = 5;
        actor->statistics().agility() = actor->statistics().dexterity() = 80;
        CHECK(AdoptJa2TacticalEntity(*actor), "exact native actor identity adopted");
    }
    target.awareness().opponentKnowledge()[shooter.identity().id()] = SEEN_CURRENTLY;
    selected.awareness().opponentKnowledge()[target.identity().id()] = SEEN_CURRENTLY;
    shooter.awareness().opponentKnowledge()[target.identity().id()] = SEEN_CURRENTLY;
    CHECK(UsingImprovedInterruptSystem(), "installed-style native IIS is active");
    gMAXITEMS_READ = 7;
    Item[5].usItemClass = IC_GUN; Weapon[5].ubWeaponType = GUN_RIFLE; Weapon[5].ubReadyTime = 6;
    Weapon[5].ubWeaponClass = RIFLECLASS; Weapon[5].ubCalibre = 1; Weapon[5].ubMagSize = 30;
    Item[6].usItemClass = IC_AMMO; Item[6].ubClassIndex = 0;
    Magazine[0].ubCalibre = 1; Magazine[0].ubMagSize = 30; Magazine[0].ubAmmoType = 0;
    Magazine[1].ubCalibre = NOAMMO; AmmoTypes[0].standardIssue = TRUE;
    CHECK(CreateItem(5, 100, &target.inventory()[HANDPOS]), "target carries a real native ordinary gun object");
    const INT16 readyCost = GetAPsToReadyWeapon(&target, READY_RIFLE_STAND);
    CHECK(readyCost > 0, "native ready-weapon rule computes a positive AP cost");

    if (attackLifecycle)
    {
        using Outcome = DedicatedCoopAttackOutcome;
        DedicatedServerOptions options; options.enabled = true; options.mode = DedicatedServerMode::Coop;
        InstallDedicatedServerOptions(options);
        CHECK(CreateItem(5, 100, &shooter.inventory()[HANDPOS]), "actual native shooter gun created");
        shooter.inventory()[HANDPOS][0]->data.gun.ubGunShotsLeft = 3;
        shooter.targeting().targetId() = target.identity().id();
        shooter.targeting().gridNo() = target.position().gridNo();
        shooter.targeting().lastGridNo() = NOWHERE;
        selected.targeting().lastGridNo() = 42;
        const auto id = GetJa2TacticalEntityId(shooter);
        DedicatedCoopAttackState state, other;
        CHECK(BeginDedicatedCoopAttack(0, shooter) && !DedicatedCoopAttackPending(), "unbound seam leaves normal gameplay untracked");
        CHECK(BindDedicatedCoopAttackState(state) && !BindDedicatedCoopAttackState(other), "only one owner can bind the native chain");
        CHECK(BeginJa2TacticalCombatAction(), "unrelated native action owns its existing counter");
        CHECK(!BeginDedicatedCoopAttack(0, shooter) && !DedicatedCoopAttackPending(), "remote attack cannot take ownership of an existing native chain");
        CHECK(CompleteJa2TacticalCombatAction(), "fixture drains the unrelated action");
        CHECK(BeginDedicatedCoopAttack(0, shooter) && DedicatedCoopAttackPending(), "sequence zero starts the exact native attack");
        CHECK(!BeginDedicatedCoopAttack(1, selected), "another actor cannot overlap the native attack");
        shooter.animationActivity().turningToShoot() = TRUE;
        CHECK(TacticalActorRouteExecution::haltForSighting(shooter, true), "real native sighting cancels the pending fire turn");
        CHECK(CaptureDedicatedCoopAttackOutcome(0, 1, id) == Outcome::Interrupted && !DedicatedCoopAttackPending(),
            "native cancellation drains without manufacturing a shot");
        CHECK(!shooter.animationActivity().turningToShoot() && shooter.inventory()[HANDPOS][0]->data.gun.ubGunShotsLeft == 3,
            "sighting cancellation leaves ammunition unchanged");
        CHECK(shooter.targeting().lastGridNo() == target.position().gridNo() && selected.targeting().lastGridNo() == 42 && gusSelectedSoldier == SoldierID{0},
            "ordinary completion belongs to actor 3 while selected actor 0 stays untouched");
        CHECK(!ReleaseDedicatedCoopAttack(1, 1, id) && !ReleaseDedicatedCoopAttack(0, 2, id) &&
            !ReleaseDedicatedCoopAttack(0, 1, TacticalEntityId{id.slot, id.incarnation + 1}), "foreign acknowledgements cannot retire a native outcome");
        CHECK(!BeginDedicatedCoopAttack(1, shooter), "terminal outcome remains retained until its receipt is acknowledged");
        UnbindDedicatedCoopAttackState(other);
        CHECK(ReleaseDedicatedCoopAttack(0, 1, id), "exact native cancellation can be acknowledged once");
        CHECK(BeginDedicatedCoopAttack(1, shooter) && BeginJa2TacticalCombatAction(), "next fresh native attack owns a real combat action");
        DeductAmmo(&shooter, HANDPOS);
        CHECK(shooter.inventory()[HANDPOS][0]->data.gun.ubGunShotsLeft == 2, "native ammunition accounting consumes the actual round");
        (void)ReduceAttackBusyCount();
        CHECK(CaptureDedicatedCoopAttackOutcome(1, 1, id) == Outcome::Completed && !state.failure(),
            "native action drain follows the actual shooter's ammunition use");
        CHECK(ReleaseDedicatedCoopAttack(1, 1, id) && !ReleaseDedicatedCoopAttack(1, 1, id), "a terminal acknowledgement is consumed once");
        CHECK(BeginDedicatedCoopAttack(2, shooter), "stale-actor control starts a third native chain");
        ++shooter.identity().incarnation();
        CHECK(BeginJa2TacticalCombatAction(), "stale-actor control reaches ordinary completion");
        (void)ReduceAttackBusyCount();
        CHECK(state.failure() && CaptureDedicatedCoopAttackOutcome(2, 1, id) == Outcome::Invalid &&
            selected.targeting().lastGridNo() == 42 && !BeginDedicatedCoopAttack(3, selected),
            "reused native actor slot fails closed without fallback or a second attack");
        --shooter.identity().incarnation();
        state.reset();
        CHECK(BeginDedicatedCoopAttack(3, shooter), "fresh test context starts the stale-world control");
        NotifyJa2TacticalWorldLoaded(2);
        CHECK(BeginJa2TacticalCombatAction(), "stale-world control owns the completion callback");
        (void)ReduceAttackBusyCount();
        CHECK(state.failure() && CaptureDedicatedCoopAttackOutcome(3, 1, id) == Outcome::Invalid &&
            selected.targeting().lastGridNo() == 42, "lost native origin fails closed without UI-selection fallback");
        MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr; ReleaseWorldTileMap();
        std::printf("Native attack lifecycle: %s\n", failures ? "FAIL" : "PASS");
        return failures ? 1 : 0;
    }

    // Control: selected/shooter mismatch alone does not strand AFTERSHOT when
    // no opposing actor currently has enough AP to receive an interrupt.
    DeductPoints(&shooter, 32, 0, AFTERSHOT_INTERRUPT);
    CHECK(GetJa2PendingInterrupt() == AFTERSHOT_INTERRUPT, "actual shooter AP expenditure registers AFTERSHOT");
    CHECK(!ResolvePendingInterrupt(&selected, AFTERSHOT_INTERRUPT) && GetJa2PendingInterrupt() == DISABLED_INTERRUPT,
        "legacy selected actor completion alone consumes the pending type in the no-grant control");

    DeductPoints(&shooter, 32, 0, AFTERSHOT_INTERRUPT);
    const UINT8 beforeReady = GetJa2PendingInterrupt();
    const INT16 targetAp = target.actionPoints().current();
    const auto selectedCounter = selected.turnState().interruptCounters()[target.identity().id()];
    const auto shooterCounter = shooter.turnState().interruptCounters()[target.identity().id()];
    CHECK(beforeReady == AFTERSHOT_INTERRUPT, "fresh actual remote-shot cost is pending before target recovery");
    const AnimationSurfaceType retainedSurface = gAnimSurfaceDatabase[0];
    ETRLEObject frames[8]{}; SGPVObject video{};
    video.usNumberOfObjects = 8; video.pETRLEObject = frames;
    gAnimSurfaceDatabase[0].hVideoObject = &video;
    gAnimSurfaceDatabase[0].uiNumDirections = 8; gAnimSurfaceDatabase[0].uiNumFramesPerDir = 1;
    gAnimSurfaceDatabase[0].bStructDataType = NO_STRUCT; gAnimSurfaceDatabase[0].bProfile = -1;
    for (UINT16 animation : {UINT16(STANDING), UINT16(RIFLE_STAND_HIT), UINT16(READY_RIFLE_STAND), UINT16(AIM_RIFLE_STAND)})
    {
        gubAnimSurfaceIndex[REGMALE][animation] = 0;
        gubAnimSurfaceItemSubIndex[REGMALE][animation] = INVALID_ANIMATION;
    }
    target.animationActivity().readyCostWaived() = FALSE;
    bool transitioned = false;
    if (pointsOnly)
    {
        // Exact call made by READY_RIFLE_STAND's native initializer. This mode
        // isolates the interrupt accounting independently of animation assets.
        DeductPoints(&target, readyCost, 0, BEFORESHOT_INTERRUPT);
    }
    else
    {
        target.animationPlayback().state() = RIFLE_STAND_HIT;
        target.animationActivity().beginHit();
        target.animationActivity().postHitStance() = GO_TO_AIM_AFTER_HIT;
        target.animationPlayback().code() = 14;
        // Installed Ja2bin.dat hit script index14=472. This is the production
        // hit-recovery handler, which calls ordinary readyFacing; no driver
        // shortcut directly invokes the READY initializer in this mode.
        gusAnimInst[RIFLE_STAND_HIT][14] = 472;
        CHECK(BeginJa2TacticalCombatAction(), "hit animation owns one actual pending combat effect");
        CHECK(AdjustToNextAnimationFrame(&target), "actual hit script recovery handler runs");
        transitioned = target.animationPlayback().state() == READY_RIFLE_STAND;
    }
    const UINT8 afterReady = GetJa2PendingInterrupt();
    CHECK(target.actionPoints().current() == targetAp - readyCost, "off-turn native hit recovery charges its actual ready cost");
    CHECK(selected.turnState().interruptCounters()[target.identity().id()] == selectedCounter &&
        shooter.turnState().interruptCounters()[target.identity().id()] == shooterCounter,
        "off-turn recovery cannot accumulate interrupt credit for either player actor");
    CHECK(afterReady == (pointsOnly ? AFTERSHOT_INTERRUPT : DISABLED_INTERRUPT),
        "hit recovery preserves AFTERSHOT until ordinary completion consumes it");
    CHECK(pointsOnly || (transitioned && GetJa2PendingTacticalCombatActions() == 0),
        "native hit script transitions and drains the real combat action");
    CHECK(!ResolvePendingInterrupt(&shooter, AFTERSHOT_INTERRUPT), "ordinary completion finds no eligible interrupt recipient");
    CHECK(GetJa2PendingInterrupt() == DISABLED_INTERRUPT &&
        CaptureJa2TacticalInterruptProjection().phase == Ja2TacticalInterruptPhase::None &&
        gusSelectedSoldier == SoldierID{0} && GetJa2TacticalCurrentTeam() == OUR_TEAM,
        "native completion releases the gate without manufactured clears or selection changes");
    std::printf("Native remote completion regression: mode=%s selected=%u shooter=%u target=%u before=%u after=%u final=%u readyCost=%d targetAP=%d->%d transition=%u pendingCombat=%u\n",
        pointsOnly ? "points" : "animation", unsigned(gusSelectedSoldier.i), unsigned(shooter.identity().id().i),
        unsigned(target.identity().id().i), unsigned(beforeReady), unsigned(afterReady), unsigned(GetJa2PendingInterrupt()),
        int(readyCost), int(targetAp), int(target.actionPoints().current()), transitioned ? 1u : 0u,
        unsigned(GetJa2PendingTacticalCombatActions()));

    // Positive controls cover both ordinary teams. The same current-team rule
    // applies after an interrupt grant hands control to the granted team.
    for (TacticalActor* active : {&shooter, &target})
    {
        TacticalActor& watcher = active == &shooter ? target : shooter;
        RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, active->roster().team(), 0);
        active->actionPoints().current() = 100;
        const auto before = watcher.turnState().interruptCounters()[active->identity().id()];
        SetJa2PendingInterrupt(DISABLED_INTERRUPT);
        DeductPoints(active, 10, 0, BEFORESHOT_INTERRUPT);
        CHECK(active->actionPoints().current() == 90 &&
            watcher.turnState().interruptCounters()[active->identity().id()] > before &&
            GetJa2PendingInterrupt() == BEFORESHOT_INTERRUPT,
            "either current team's genuine action still spends AP and registers interrupt credit");
        const auto retained = watcher.turnState().interruptCounters()[active->identity().id()];
        SetJa2PendingInterrupt(AFTERSHOT_INTERRUPT);
        DeductPoints(active, 5, 0, DISABLED_INTERRUPT);
        CHECK(active->actionPoints().current() == 85 &&
            watcher.turnState().interruptCounters()[active->identity().id()] == retained &&
            GetJa2PendingInterrupt() == AFTERSHOT_INTERRUPT,
            "disabled interrupt accounting preserves AP spending and the unrelated pending action");
        is_networked = true;
        DeductPoints(active, 5, 0, BEFORESHOT_INTERRUPT);
        CHECK(active->actionPoints().current() == 80 &&
            watcher.turnState().interruptCounters()[active->identity().id()] == retained &&
            GetJa2PendingInterrupt() == AFTERSHOT_INTERRUPT,
            "legacy network mode keeps IIS disabled without changing its AP cost");
        is_networked = false;
    }

    for (TacticalActor* actor : {&selected, &shooter, &target}) actor->animationCache().reset();
    gAnimSurfaceDatabase[0] = retainedSurface;
    MemFree(gubWorldMovementCosts); gubWorldMovementCosts = nullptr; ReleaseWorldTileMap();
    std::printf("Native remote completion regression: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
