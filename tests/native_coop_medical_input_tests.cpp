// Queued native atoms through the shipping medical router and real passive clients.
#include "FullEngineCoopClientMedicalInput.h"
#include "FullEngineCoopClientCampaignTimeInput.h"
#include "FullEngineCoopSnapshotReplica.h"
#include "CoopHandshakeProtocol.h"
#include "input.h"
#include "Animation Control.h"
#include "english.h"
#include "sgp.h"
#include "sdl_input.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

static_assert(TacticalMedicalIdleStandingAnimation == STANDING);
static_assert(TacticalMedicalIdleCrouchedAnimation == CROUCHING);
static_assert(TacticalMedicalIdleProneAnimation == PRONE);
static_assert(TacticalMedicalAimRifleStandingAnimation == AIM_RIFLE_STAND);
static_assert(TacticalMedicalAimRifleCrouchedAnimation == AIM_RIFLE_CROUCH);
static_assert(TacticalMedicalAimRifleProneAnimation == AIM_RIFLE_PRONE);
static_assert(TacticalMedicalAimDualStandingAnimation == AIM_DUAL_STAND);
static_assert(TacticalMedicalAimDualCrouchedAnimation == AIM_DUAL_CROUCH);
static_assert(TacticalMedicalAimDualProneAnimation == AIM_DUAL_PRONE);

int iWindowedMode = 1;
BOOLEAN gfProgramIsRunning = TRUE;
BOOLEAN gfDedicatedServer = FALSE;
BOOLEAN gfDedicatedServerProcessFailed = FALSE;
BOOLEAN gfDontUseDDBlits = FALSE;
bool g_bUseXML_Structures = false;
CHAR8 gzCommandLine[100] = {};
void ShutdownWithErrorBox(const CHAR8* message) { std::fprintf(stderr, "%s\n", message ? message : ""); std::exit(1); }

using namespace CoopSession;
namespace
{
int failures = 0;
#define CHECK(condition, message) do { if (!(condition)) { ++failures; std::printf("FAIL %d %s\n", __LINE__, message); } } while (false)
constexpr TacticalEntityId MedicA{1, 5}, MedicB{2, 8}, ThirdPatient{3, 9};
struct Frame { std::string name; std::vector<std::uint8_t> bytes; };
class Wire final : public FullEngineCoopClientWire
{
public:
 bool send(const char* name, const std::uint8_t* bytes, std::size_t size) noexcept override
 { try { frames.push_back({name, {bytes, bytes + size}}); return true; } catch (...) { return false; } }
 void close() noexcept override { ++closed; }
 std::vector<Frame> frames; unsigned closed = 0;
};
struct Client
{
 Wire wire;
 FullEngineCoopSnapshotReplica replica;
 FullEngineCoopClient client{wire, replica};
 FullEngineCoopClientController controller;
 TacticalEntityId owned{};
 std::array<TacticalEntityId, 1> assigned{};
 std::array<const CoopOwnerInventorySnapshot*, 1> inventories{};
 FullEngineCoopClientControllerView view()
 {
  assigned[0] = client.assignedActor(0);
  inventories[0] = client.ownerInventory(assigned[0]);
  return {replica.hasSnapshot() ? &replica.snapshot() : nullptr, assigned.data(), client.assignedActorCount(),
   client.outstandingCommandId(), client.state() != FullEngineCoopClientState::Active, inventories.data()};
 }
};
std::vector<TacticalActorSnapshot> Actors()
{
 std::vector<TacticalActorSnapshot> actors;
 for (const auto id : {MedicA, MedicB, ThirdPatient})
 {
  TacticalActorSnapshot a;
  a.id = id; a.profile = id.slot; a.grid = id == MedicA ? 1001 : id == MedicB ? 1002 : 1161;
  a.team = 0; a.direction = 2; a.animation = TacticalMedicalIdleStandingAnimation;
  a.stance = TacticalStance::Standing; a.active = a.inSector = true;
  a.life = 75; a.maximumLife = 90; a.breath = a.maximumBreath = 100; a.actionPoints = 40;
  a.bleeding = 8; a.handMedicalKitPoints = 100;
  a.loadout.primaryHand = {202, 1, 100};
  a.presentation.flags = TacticalActorRenderPosePresent;
  a.presentation.animationSurface = 0; a.presentation.animationDirection = 2;
  a.presentation.worldXQ8 = (a.grid % 160 * 10 + 5) * 256;
  a.presentation.worldYQ8 = (a.grid / 160 * 10 + 5) * 256;
  a.presentation.displayNameUtf16[0] = static_cast<std::uint16_t>('A' + id.slot - 1);
  actors.push_back(a);
 }
 return actors;
}
TacticalWorldSnapshot Snapshot(const std::vector<TacticalActorSnapshot>& actors, std::uint64_t world = 11)
{
 TacticalWorldSnapshot snapshot;
 CHECK(TacticalWorldSnapshot::create(world, {160, 160}, {9, 1, 0, true, TacticalMapAssetKey{{"A9.dat"}}},
  {true, true, 0, 3}, actors, snapshot) == TacticalSnapshotCreateError::None, "medical public snapshot is canonical");
 return snapshot;
}
CoopOwnerInventorySnapshot Inventory(const Client& h, std::uint64_t revision = 1)
{
 CoopOwnerInventorySnapshot owner;
 owner.sessionEpoch = 101; owner.worldGeneration = 11; owner.baselineId = 1;
 owner.inventoryRevision = revision; owner.owner = h.client.peerIdentity(); owner.actor = h.owned;
 owner.slots.resize(55);
 for (std::size_t i = 0; i < owner.slots.size(); ++i) owner.slots[i].slot = static_cast<std::uint16_t>(i);
 owner.slots[5] = {5, 202, 1, 100, CoopInventorySlotSupport::OrdinarySwappable,
  CoopInventoryStatusKind::MedicalKitPoints, 100};
 owner.slots[14] = {14, static_cast<std::uint16_t>(300 + h.owned.slot), 1, 80,
  CoopInventorySlotSupport::OrdinarySwappable, CoopInventoryStatusKind::Condition, 0};
 return owner;
}
void PublishInventory(Client& h, const CoopOwnerInventorySnapshot& owner)
{
 std::vector<std::uint8_t> bytes;
 CHECK(EncodeCoopOwnerInventorySnapshot(owner, bytes) == CoopInventoryCodecResult::Success &&
  h.client.receiveOwnerInventory(bytes.data(), bytes.size()) == FullEngineCoopClientResult::Success,
  "authenticated private inventory commits without local item mutation");
}
void Activate(Client& h, TacticalEntityId owned, const std::vector<TacticalActorSnapshot>& actors)
{
 h.owned = owned;
 FullEngineCoopClientConfiguration config;
 config.runtimeFingerprint = {1, 17, 23}; config.contentManifestSha256.fill(5);
 CHECK(h.client.configure(config) == FullEngineCoopClientResult::Success &&
  h.client.beginConnection() == FullEngineCoopClientResult::Success &&
  h.client.transportConnected() == FullEngineCoopClientResult::Success, "passive client begins authenticated connection");
 CoopServerHello hello; hello.protocolVersion = config.protocolVersion; hello.sessionEpoch = 101; hello.runtimeFingerprint = config.runtimeFingerprint;
 hello.contentManifestSha256 = config.contentManifestSha256;
 CoopServerHelloBytes helloBytes{};
 CHECK(EncodeCoopServerHello(hello, helloBytes) && h.client.receiveServerHello(helloBytes.data(), helloBytes.size()) ==
  FullEngineCoopClientResult::Success, "medical protocol hello accepted");
 AdmissionResponse admitted; admitted.sessionEpoch = 101; admitted.rejectReason = AdmissionRejectReason::None;
 admitted.peerIdentity.fill(static_cast<std::uint8_t>(owned.slot)); admitted.reconnectToken.fill(static_cast<std::uint8_t>(owned.slot + 20));
 AdmissionResponseBytes response{};
 CHECK(EncodeAdmissionResponse(admitted, response) && h.client.receiveAdmissionResponse(response.data(), response.size()) ==
  FullEngineCoopClientResult::Success, "each owner ACKs its separate credential");
 CoopTacticalBaseline baseline; baseline.state.sessionEpoch = 101; baseline.state.worldGeneration = 11; baseline.state.revision = 2; baseline.state.turnSerial = 3; baseline.baselineId = 1;
 baseline.nextExpectedCommandId = 1; baseline.assignedActors = {owned}; baseline.snapshot = Snapshot(actors);
 std::vector<std::uint8_t> bytes;
 CHECK(EncodeCoopTacticalBaseline(baseline, bytes) == CoopTacticalCodecResult::Success &&
  h.client.receiveBaseline(bytes.data(), bytes.size()) == FullEngineCoopClientResult::Success,
  "real passive replica adopts one owner's exact assignment");
 PublishInventory(h, Inventory(h));
 h.controller.synchronize(h.view());
}
void ReplaceBaseline(Client& h, const std::vector<TacticalActorSnapshot>& actors,
 std::uint64_t world = 11, bool assigned = true)
{
 CoopTacticalBaseline baseline; baseline.state.sessionEpoch = 101; baseline.state.worldGeneration = world; baseline.state.revision = h.client.acceptedState().revision + 1; baseline.state.turnSerial = 3;
 baseline.baselineId = 2; baseline.nextExpectedCommandId = 1; baseline.snapshot = Snapshot(actors, world);
 if (assigned) baseline.assignedActors = {h.owned};
 std::vector<std::uint8_t> bytes;
 CHECK(EncodeCoopTacticalBaseline(baseline, bytes) == CoopTacticalCodecResult::Success &&
  h.client.receiveBaseline(bytes.data(), bytes.size()) == FullEngineCoopClientResult::Success,
  "new exact baseline revokes prior owner and selection lineage");
 if (assigned)
 {
  auto owner = Inventory(h); owner.baselineId = 2; owner.worldGeneration = world;
  PublishInventory(h, owner);
 }
}
void PublishPublic(Client& h, const std::vector<TacticalActorSnapshot>& actors)
{
 CoopTacticalDelta delta; delta.state = h.client.acceptedState(); delta.baseRevision = delta.state.revision++;
 delta.deltaId = delta.state.revision; delta.delta.previousEpoch = delta.delta.currentEpoch = delta.state.worldGeneration;
 for (const auto& actor : actors)
  if (const auto* current = h.replica.snapshot().find(actor.id); current && *current != actor)
   delta.delta.events.emplace_back(TacticalActorUpdatedEvent{actor});
 std::vector<std::uint8_t> bytes;
 CHECK(EncodeCoopTacticalDelta(delta, bytes) == CoopTacticalCodecResult::Success &&
  h.client.receiveDelta(bytes.data(), bytes.size()) == FullEngineCoopClientResult::Success,
  "current public update commits through production replica and ACK path");
}
unsigned IntentCount(const Client& h)
{
 unsigned count = 0; for (const auto& frame : h.wire.frames) if (frame.name == CoopTacticalIntentMessageName) ++count;
 return count;
}
unsigned Drain(Client& h)
{
 auto view = h.view(); bool modal = h.controller.selectingFirstAid();
 h.controller.synchronize(view);
 InputAtom event{}; unsigned unconsumed = 0;
 while (DequeueEvent(&event))
 {
  const auto routed = RouteFullEngineCoopClientMedicalInput(h.controller, view, event, modal);
  modal = modal || h.controller.selectingFirstAid();
  if (!routed.consumed) ++unconsumed;
  if (routed.request)
  {
   CHECK(h.client.sendIntent(routed.request.actor, routed.request.payload) == FullEngineCoopClientResult::Success,
    "queued medical confirmation uses production client sendIntent");
   view.outstandingCommandId = h.client.outstandingCommandId();
  }
 }
 return unconsumed;
}
void Key(Client& h, UINT32 key) { QueueEvent(KEY_DOWN, key, 0); CHECK(Drain(h) == 0, "medical router consumes its queued key"); }
void TestTwoOwnersQueuedMedicalWireAndCache()
{
 auto a = std::make_unique<Client>(); auto b = std::make_unique<Client>(); auto actors = Actors();
 Activate(*a, MedicA, actors); Activate(*b, MedicB, actors);
 CHECK(!a->client.ownerInventory(MedicB) && !b->client.ownerInventory(MedicA), "other owner's inventory is never borrowed");
 for (Client* h : {a.get(), b.get()})
 {
  const auto expectedPatient = h == a.get() ? MedicB : MedicA;
  const auto* cache = h->client.ownerInventory(h->owned);
  CHECK(cache != nullptr, "medical fixture owner cache is present");
  if (!cache) return;
  const auto privateBefore = *cache;
  Key(*h, 'B');
  CHECK(h->controller.selectingFirstAid() && h->controller.firstAidPatient() == expectedPatient,
   "B selects an exact adjacent friendly patient while medic ownership stays separate");
  if (h == a.get())
  {
   Key(*h, TAB); CHECK(h->controller.firstAidPatient() == ThirdPatient, "explicit cycle chooses next eligible patient");
   Key(*h, UPARROW); CHECK(h->controller.firstAidPatient() == MedicB, "reverse cycle restores exact selected patient");
  }
  auto unrelated = actors; unrelated[2].actionPoints -= 1;
  PublishPublic(*h, unrelated); h->controller.synchronize(h->view());
  CHECK(h->controller.selectingFirstAid() && h->controller.firstAidPatient() == expectedPatient,
   "unrelated public revision preserves multi-key medical selection");
  const auto revision = h->client.acceptedState().revision;
  QueueEvent(KEY_DOWN, ENTER, 0); QueueEvent(KEY_REPEAT, ENTER, 0); QueueEvent(KEY_DOWN, RIGHTARROW, 0);
  CHECK(Drain(*h) == 0 && IntentCount(*h) == 1 && h->client.outstandingCommandId() == 1,
   "one confirmation sends once and modal queue cannot fall through to movement");
  const Frame* frame = nullptr; for (const auto& f : h->wire.frames) if (f.name == CoopTacticalIntentMessageName) frame = &f;
  TacticalIntent intent;
  CHECK(frame && DecodeTacticalIntent(frame->bytes.data(), frame->bytes.size(), intent) == TacticalIntentCodecResult::Success &&
   intent.actor == h->owned && intent.claimedPeerIdentity == h->client.peerIdentity() && intent.baseRevision == revision &&
   std::get<BeginFirstAidTacticalIntent>(intent.payload).target == expectedPatient && frame->bytes[6] == 10,
   "production wire binds kind10 to exact current owner, patient incarnation and fresh public revision");
  CHECK(h->client.ownerInventory(h->owned) && *h->client.ownerInventory(h->owned) == privateBefore && h->replica.snapshot().actors() == unrelated,
   "medical intent makes no private item, AP, service or health prediction");
  auto changed = Inventory(*h, 2); changed.slots[5].firstCondition = 99; changed.slots[5].resourceTotal = 99;
  PublishInventory(*h, changed);
  CHECK(h->client.outstandingCommandId() == 1 && h->client.ownerInventory(h->owned)->slots[5].resourceTotal == 99,
   "authoritative private replacement arrives while command lock remains");
  CoopTacticalIntentReceipt receipt; receipt.state = h->client.acceptedState(); receipt.peerIdentity = h->client.peerIdentity();
  receipt.commandId = 1; receipt.nextExpectedCommandId = 2; receipt.authoritativeSequence = 1; receipt.simulationTick = 10;
  receipt.status = CoopTacticalIntentReceiptStatus::Applied; receipt.reason = CoopTacticalIntentReceiptReason::None;
  CoopTacticalIntentReceiptBytes bytes{};
  CHECK(EncodeCoopTacticalIntentReceipt(receipt, bytes) == CoopTacticalCodecResult::Success &&
   h->client.receiveIntentReceipt(bytes.data(), bytes.size()) == FullEngineCoopClientResult::Success &&
   h->client.outstandingCommandId() == 0 && h->client.ownerInventory(h->owned)->inventoryRevision == 2,
   "terminal receipt unlocks only with the already received replacement cache");
 }
}
void TestMedicalOwnerHandoffConsumesQueuedFrame()
{
 auto h = std::make_unique<Client>(); Activate(*h, MedicA, Actors()); Key(*h, 'B');
 CHECK(h->controller.activeModalWorldMatches(h->view().snapshot->epoch()) &&
  !h->controller.activeModalWorldMatches(h->view().snapshot->epoch() + 1),
  "medical mode participates in the integrated renderer exact-world modal guard");
 const bool medicalAtFrameStart = h->controller.selectingFirstAid();
 // World loss can revoke the controller before the downstream owner is known.
 h->controller.synchronize(FullEngineCoopClientControllerView{});
 for (const auto key : {UINT32('1'), UINT32(ENTER), UINT32('L')}) QueueEvent(KEY_DOWN, key, 0);
 CHECK(ConsumeFullEngineCoopClientMedicalOwnerChange(h->controller, medicalAtFrameStart),
  "medical owner handoff consumes the queued frame even after selection revocation");
 FullEngineCoopClientCampaignTimeInput time;
 InputAtom event{}; unsigned remaining = 0, campaignActions = 0;
 while (DequeueEvent(&event))
 {
  ++remaining;
  if (time.handle(event.usParam, event.usEvent == KEY_DOWN, event.usEvent == KEY_UP, true)) ++campaignActions;
 }
 CHECK(remaining == 0 && campaignActions == 0 && IntentCount(*h) == 0,
  "queued medical 1/Enter/L cannot become campaign ResumeNormal, notice confirmation or leave");
 // The next fresh input frame belongs to the new owner normally.
 QueueEvent(KEY_DOWN, '1', 0);
 CHECK(!ConsumeFullEngineCoopClientMedicalOwnerChange(h->controller, false) && DequeueEvent(&event) &&
  time.handle(event.usParam, true, false, true).has_value(), "fresh campaign input remains available after handoff");
 SDL_Event quit{}; quit.type = SDL_EVENT_QUIT;
 CHECK(SgpHandleSDLEvent(&quit), "medical handoff never consumes the application-close event");
}
void TestMedicalRevocationAndModalCancellation()
{
 for (unsigned test = 0; test < 23; ++test)
 {
  auto h = std::make_unique<Client>(); auto actors = Actors(); Activate(*h, MedicA, actors); Key(*h, 'b');
  CHECK(h->controller.selectingFirstAid(), "medical revocation starts with a valid choice");
  if (test == 0) actors[1].bleeding = 0;
  if (test == 1) actors[1].grid += 160;
  if (test == 2) actors[1].medicalService.receivingCount = 1;
  if (test == 3) actors[1].animation = 6;
  if (test == 4) actors[0].animation = 0;
  if (test == 5) actors[1].stance = TacticalStance::Crouched;
  if (test == 6) actors[1].presentation = {};
  if (test <= 6) PublishPublic(*h, actors);
  if (test == 7) { auto inv = Inventory(*h, 2); PublishInventory(*h, inv); }
  if (test == 8) { auto view = h->view(); view.assignedActorCount = 0; h->controller.synchronize(view); }
  if (test == 9) { auto view = h->view(); view.resynchronizing = true; h->controller.synchronize(view); }
  if (test == 10) { auto view = h->view(); view.outstandingCommandId = 99; h->controller.synchronize(view); }
  if (test == 11) Key(*h, ESC);
  if (test == 12) Key(*h, SPACE);
  if (test == 13) ReplaceBaseline(*h, actors);
  if (test == 14) ReplaceBaseline(*h, actors, 12);
  if (test == 15) { ++actors[1].id.incarnation; ReplaceBaseline(*h, actors); }
  if (test == 16) ReplaceBaseline(*h, actors, 11, false);
  if (test == 17 || test == 18)
  {
   auto forged = Inventory(*h); if (test == 17) forged.owner.fill(77); else ++forged.sessionEpoch;
   std::vector<std::uint8_t> bytes;
   CHECK(EncodeCoopOwnerInventorySnapshot(forged, bytes) == CoopInventoryCodecResult::Success &&
    h->client.receiveOwnerInventory(bytes.data(), bytes.size()) != FullEngineCoopClientResult::Success &&
    !h->client.ownerInventory(MedicA), "foreign owner or session frame fails closed and revokes cached kit");
  }
  if (test == 19) { actors[0].handMedicalKitPoints = 50; actors[0].loadout.primaryHand.condition = 50; PublishPublic(*h, actors); }
  if (test == 20) { actors[1].team = 1; PublishPublic(*h, actors); }
  if (test == 21) { actors[0].presentation = {}; PublishPublic(*h, actors); }
  if (test == 22) { ++actors[0].id.incarnation; h->owned = actors[0].id; ReplaceBaseline(*h, actors); }
  // Capture and consume the pending medical frame before fresh eligibility is checked.
  QueueEvent(KEY_DOWN, ENTER, 0); (void)Drain(*h);
  CHECK(!h->controller.selectingFirstAid() && IntentCount(*h) == 0 && h->controller.firstAidPatient() == TacticalEntityId{},
   "eligibility, private revision, readiness or explicit cancellation revokes without retargeting or sending");
 }
 auto h = std::make_unique<Client>(); Activate(*h, MedicA, Actors()); Key(*h, 'b');
 for (const auto key : {'i', 'm', 'f', 'd', 'l', 't', 'r'}) Key(*h, static_cast<UINT32>(key));
 CHECK(h->controller.selectingFirstAid() && !h->controller.inventoryOpen() && !h->controller.enteringDestination() &&
  !h->controller.targetingAttack() && IntentCount(*h) == 0, "medical mode consumes other tactical, inventory and leave keys");
 SDL_Event quit{}; quit.type = SDL_EVENT_QUIT;
 CHECK(SgpHandleSDLEvent(&quit), "application quit remains outside the medical input modal");
 Key(*h, ESC); CHECK(h->controller.openInventory(h->view()), "inventory can open after medical cancellation");
 QueueEvent(KEY_DOWN, 'b', 0); CHECK(Drain(*h) == 1 && !h->controller.selectingFirstAid(), "medical toggle does not steal inventory modal input");
}
}
void TestStationaryAimPosesSelectMedicalService()
{
 // Armed mercs settle into stationary aim poses after combat movement. The
 // native gate accepts these for both medic and patient; transitions and
 // stance/animation mismatches remain ineligible.
 struct Pose { std::uint16_t animation; TacticalStance stance; bool eligible; };
 const Pose poses[] = {
  {TacticalMedicalAimRifleStandingAnimation, TacticalStance::Standing, true},
  {TacticalMedicalAimRifleCrouchedAnimation, TacticalStance::Crouched, true},
  {TacticalMedicalAimRifleProneAnimation, TacticalStance::Prone, true},
  {TacticalMedicalAimDualStandingAnimation, TacticalStance::Standing, true},
  {TacticalMedicalAimDualCrouchedAnimation, TacticalStance::Crouched, true},
  {TacticalMedicalAimDualProneAnimation, TacticalStance::Prone, true},
  {TacticalMedicalAimRifleCrouchedAnimation, TacticalStance::Standing, false},
  {READY_RIFLE_STAND, TacticalStance::Standing, false},
  {SHOOT_RIFLE_STAND, TacticalStance::Standing, false},
 };
 for (const auto& pose : poses)
  for (const bool medicPose : {true, false})
  {
   auto actors = Actors();
   auto& changed = actors[medicPose ? 0 : 1];
   changed.animation = pose.animation; changed.stance = pose.stance;
   auto h = std::make_unique<Client>(); Activate(*h, MedicA, actors);
   Key(*h, 'B');
   const bool selected = h->controller.selectingFirstAid() && h->controller.firstAidPatient() == MedicB;
   CHECK(selected == pose.eligible,
    "stationary aim poses select medical service for medic and patient; transitions and mismatched stances do not");
   if (h->controller.selectingFirstAid()) Key(*h, ESC);
   CHECK(IntentCount(*h) == 0, "pose eligibility check sends no intent");
  }
}
int main()
{
 std::setvbuf(stdout, nullptr, _IONBF, 0);
 CHECK(InitializeInputManager(), "native input queue initializes");
 TestTwoOwnersQueuedMedicalWireAndCache(); TestMedicalRevocationAndModalCancellation();
 TestMedicalOwnerHandoffConsumesQueuedFrame(); TestStationaryAimPosesSelectMedicalService();
 ShutdownInputManager();
 if (failures) return 1;
 std::puts("queued medical input and two-owner wire/cache tests passed"); return 0;
}
