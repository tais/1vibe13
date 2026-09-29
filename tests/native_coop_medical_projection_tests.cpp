// Actual native carried inventory projection; no installed assets or client simulation.
#include "Animation Control.h"
#include "GameContext.h"
#include "GameSettings.h"
#include "Items.h"
#include "Overhead.h"
#include "SoldierRepository.h"
#include "TacticalActor.h"
#include "TacticalEntityHost.h"
#include "TacticalInterruptHost.h"
#include "TacticalWorldAdapter.h"
#include "connect.h"

#include <cstdio>
#include <cstdlib>
#include <limits>

static_assert(TacticalMedicalIdleStandingAnimation == STANDING);
static_assert(TacticalMedicalIdleCrouchedAnimation == CROUCHING);
static_assert(TacticalMedicalIdleProneAnimation == PRONE);

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
void MakeKit(OBJECTTYPE& object, UINT8 quantity, INT16 points)
{
 object.initialize(); object.usItem = FIRSTAIDKIT; object.ubNumberOfObjects = quantity;
 object.objectStack.resize(quantity);
 for (auto& stack : object.objectStack) stack.data.objectStatus = points;
}
}
int main()
{
 GameContext& game = GetGameContext();
 const bool running = game.beginInitialization() &&
  game.advancePackagesTo(PackageBootstrapPhase::StartRuntime) && game.markRunning();
 CHECK(running, "native public medical fixture starts bound runtime");
 if (!running) return 1;
 auto& repository = GetJa2SoldierRepository();
 repository.initializeSlots(); ResetJa2TacticalActorRosters();
 ResetJa2TacticalInterruptForNewWorld(); NotifyJa2TacticalWorldLoaded(1);
 RestoreJa2TacticalTurnState(ACTIVE | TURNBASED | INCOMBAT, OUR_TEAM, 0);
 gbPlayerNum = OUR_TEAM; is_networked = is_client = is_server = false;
 gGameOptions.ubInventorySystem = INVENTORY_OLD;
 Item[FIRSTAIDKIT].usItemClass = IC_MEDKIT;
 TacticalActor& medic = *repository.resolve(0);
 TacticalActor& patient = *repository.resolve(1);
 for (auto* actor : {&medic, &patient})
 {
  const UINT16 slot = actor == &medic ? 0 : 1;
  actor->identity().id() = SoldierID{slot}; actor->identity().incarnation() = 101 + slot;
  actor->roster().active() = actor->roster().inSector() = TRUE;
  actor->roster().team() = OUR_TEAM; actor->deployment().setSector(9, 1, 0);
  actor->position().gridNo() = -1; // No render pose is needed for directory capture.
  actor->position().level() = 0;
  CHECK(AdoptJa2TacticalEntity(*actor), "native actor adopted into exact directory");
 }
 const auto medicId = GetJa2TacticalEntityId(medic);
 const auto patientId = GetJa2TacticalEntityId(patient);
 const auto state = [&]() { return GetJa2TacticalEntityDirectory().state(medicId); };
 OBJECTTYPE& hand = medic.inventory()[HANDPOS];
 MakeKit(hand, 3, 100); hand[1]->data.objectStatus = 0; hand[2]->data.objectStatus = -1;
 medic.vitals().bleeding() = 127;
 CHECK(SynchronizeJa2TacticalEntityState(medic) && state() &&
  state()->bleeding == 127 && state()->handMedicalKitPoints == 100,
  "public capture sums usable equipped points while retaining bounded bleeding");
 const TacticalActorSnapshot retained = *state();
 hand[2]->data.objectStatus = 101;
 CHECK(!SynchronizeJa2TacticalEntityState(medic) && *state() == retained,
  "out-of-range later kit entry rejects transactionally despite valid first entry");
 hand[2]->data.objectStatus = 0;
 hand.ubNumberOfObjects = 4;
 CHECK(!SynchronizeJa2TacticalEntityState(medic) && *state() == retained,
  "short stack rejects without publishing partial medical state");
 MakeKit(hand, 255, 100);
 CHECK(SynchronizeJa2TacticalEntityState(medic) && state()->handMedicalKitPoints == 25500,
  "maximum native stack sums without overflow or truncation");
 MakeKit(medic.inventory()[BIGPOCK1POS], 1, 89);
 hand.initialize(); medic.vitals().bleeding() = -1;
 CHECK(SynchronizeJa2TacticalEntityState(medic) && state()->handMedicalKitPoints == 0 &&
  state()->bleeding == 0, "reserve kits stay private and negative native bleeding projects zero");
 medic.service().beginProvidingTo(patient.identity().id()); medic.service().providerCount() = 3;
 patient.service().providerCount() = 1;
 const auto nativePartner = medic.service().partner();
 CHECK(SynchronizeJa2TacticalEntityState(medic) &&
  state()->medicalService == (TacticalMedicalServiceSnapshot{true, patientId, 3}) &&
  medic.service().partner() == nativePartner && patient.service().providerCount() == 1,
  "same-team exact patient is public without changing either native service role");
 for (unsigned hidden = 0; hidden != 6; ++hidden)
 {
  if (hidden == 0) patient.roster().team() = ENEMY_TEAM;
  if (hidden == 1) patient.position().level() = 1;
  if (hidden == 2) patient.deployment().setSector(8, 1, 0);
  if (hidden == 3) patient.roster().inSector() = FALSE;
  if (hidden == 4) patient.roster().active() = FALSE;
  if (hidden == 5) CHECK(ReleaseJa2TacticalEntity(patient), "patient binding released");
  CHECK(SynchronizeJa2TacticalEntityState(medic) &&
   state()->medicalService == (TacticalMedicalServiceSnapshot{true, {}, 3}),
   "hidden, wrong-level, off-sector or unresolved partner withholds identity but retains activity");
  patient.roster().team() = OUR_TEAM; patient.roster().active() = TRUE; patient.position().level() = 0;
  patient.deployment().setSector(9, 1, 0); patient.roster().inSector() = TRUE;
 }
 ++patient.identity().incarnation(); CHECK(AdoptJa2TacticalEntity(patient), "replacement incarnation adopted");
 CHECK(SynchronizeJa2TacticalEntityState(medic) &&
  state()->medicalService.patient == GetJa2TacticalEntityId(patient) &&
  state()->medicalService.patient != patientId,
  "numeric native partner resolves only its currently bound incarnation");
 medic.service().beginProvidingTo(medic.identity().id());
 CHECK(SynchronizeJa2TacticalEntityState(medic) && state()->medicalService.patient == medicId,
  "native self-service is faithfully represented independently of start-intent restrictions");
 medic.roster().team() = ENEMY_TEAM;
 CHECK(SynchronizeJa2TacticalEntityState(medic) && state()->medicalService.patient == TacticalEntityId{},
  "non-player provider cannot expose partner identity");
 medic.roster().team() = OUR_TEAM; medic.service().finishProviding();
 CHECK(SynchronizeJa2TacticalEntityState(medic) &&
  state()->medicalService == (TacticalMedicalServiceSnapshot{false, {}, 3}),
  "stopped provider retains independent receiving count");
 std::printf("native medical projection: %s\n", failures ? "FAILED" : "passed");
 return failures ? 1 : 0;
}
