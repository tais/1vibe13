# Multiplayer architecture

## Status and terminology

The target product is one mode-configurable full-engine `JA2 --dedicated`
server for PvP and co-op. During migration the tree still contains two server
programs, and they must not be described as interchangeable:

- `ja2server` is a transitional, data-free lobby, turn-serialization, and packet-relay
  coordinator. It does not link the JA2 engine, load a world, run AI, advance a
  campaign, or create saves.
- `JA2 --dedicated` runs the complete game with dummy presentation drivers. It
  is the only current server process capable of owning tactical AI and campaign
  simulation.

`Multiplayer/SdlNetTransport` is the project-native transport. It owns the
project-specific framed SDL3_net TCP stream, opaque process-local connection
identities, liveness, fairness, backpressure, and bounded file transfer. The
obsolete compatibility API and dormant third-party source archive have been
removed. Historical multiplayer clients are not expected to connect.

Existing `MP v3.2` arena traffic remains a legacy compatibility protocol. New
authoritative co-op traffic uses a separately versioned protocol and must not
reinterpret or silently extend a legacy packet layout.

Co-op is pre-release, so its wire formats are not versioned per change. The
global session protocol, server hello, tactical envelope, tactical intent,
snapshot, delta, owner inventory and simulation-command journal are all
version 1. Host and clients must run the identical build; a peer with any other
protocol version fails admission rather than discovering the mismatch after
admission, and reconnect credentials from other builds are not migrated. Other
snapshot layouts are rejected instead of inferring dimensions, hostility, door,
loadout, interrupt state, presentation, ambient light, medical state, portrait identity, or map identity. The snapshot carries the authority's
fixed-width, path-safe exact `.dat` map basename, one public `commandsBlocked`
bit, compact interrupt phase/serial, per-actor interrupt-action eligibility,
ambient light, a bounded actor-render presentation, and five
bounded 12-byte combat-equipment records: primary hand, secondary hand, helmet,
vest, and legs. Native interrupt lists and hidden interrupters remain private.

The actor presentation includes a three-byte portrait descriptor: normal/IMP/
absent family, an unsigned face index, and a closed camouflage variant. The host
resolves the profile-to-face mapping and native 151–154 profile alias. Applied
camouflage follows the stock last-positive rule (snow, desert, urban, wood), not
worn clothing or a possibly stale legacy face cache. Capture does not allocate
faces or consume simulation randomness. Portrait-only changes participate in
snapshot equality and full-current actor deltas; malformed enums or decorated
absence reject before replacement.

The passive TEAM roster copies that descriptor and loads manifest-bound stock
`FACES`/`IMPFACES` frame-zero assets into a per-instance six-visible-slot cache.
The renderer never looks up cold profile tables or invokes `InitFace`. Exact
world/actor/descriptor/page/assignment changes revoke cached handles, including
failed loads. Missing camouflage art may fall back only to that actor's base
portrait; missing or invalid base art retains the neutral silhouette without
discarding the tactical view. Complete bounded ETRLE/palette validation precedes
blitting, writes are clipped to the 48×43 face rectangle, and inherited clipping
is restored verbatim. This slice is static portrait presentation, not blinking,
speech, facial equipment overlays or a live legacy face system.

The current co-op deliverable is a functional but deliberately narrow
technical vertical slice. The full-engine server owns a persistent campaign,
automatic starter encounter, admission, campaign transfer, and authoritative
tactical execution. A normal `JA2` client process joins over the production
SDL3_net adapter, commits the campaign checkpoint into private scratch storage,
and retains a worldless committed-snapshot control screen as its INIT waiting,
resynchronization, and failure fallback. Once the exact map and a matching
committed baseline are ready, a guarded normal `GAME_SCREEN` renders static JA2
map layers, authority ambient lighting, actor proxies, and server-assigned
selection/camera state. This path is presentation-only: it does not run the
legacy tactical interface or panels, legacy tactical input, visibility
calculation, or local tactical/campaign simulation. This status does not imply a
complete combat, world-presentation, or strategic replica.

## Deployment shape

The first campaign implementation supports one campaign per dedicated game
process. JA2 still has extensive process-global campaign and presentation
state, so multiple campaigns in one address space are explicitly out of scope.

Initially clients connect directly to the full-engine dedicated process. A
future `ja2server` control plane may advertise sessions and supervise one
full-engine worker per campaign, but the coordinator must never validate or
execute gameplay intent itself.

## Authority model

Co-op uses authoritative server simulation, not deterministic client lockstep.

The dedicated process owns:

- campaign and tactical state;
- actor identity and player-to-actor ownership;
- legality, action points, turns, interrupts, AI, and combat outcomes;
- the campaign clock, scheduled events, and random-number state;
- sector transitions and synchronization barriers;
- saves, loads, autosaves, and reconnect baselines.

Clients own only presentation and local input. The current wire exposes exactly
fourteen bounded intents: move, face, change stance, stop, end turn, an
exact-target aimed single-shot firearm attack with aim time from zero through
eight, selected-actor reload, visible adjacent-door open/close, exact-serial
interrupt pass, adjacent teammate first-aid initiation, ordinary whole-stack
ordinary equipment/pocket inventory swap, same-owner whole-stack inventory transfer, and
visible neutral same-tile ground pickup into an empty pocket, and ordinary
whole-stack pocket drop onto clear empty ground. The reload
payload is empty: the server
prepares the existing `ReloadWeaponCommand`, resolves the selected weapon and
ammunition, and revalidates native `AutoReload`, including manual chambering.
The door payload is the exact public base grid, current ephemeral structure ID,
and desired open state; it carries no lock, trap, key, database, or perceived-
state fingerprint. The first-aid kind 10 payload is only the exact six-byte
patient identity. A selected medic with an already equipped usable medical kit
may request native first aid for a distinct cardinally adjacent wounded teammate, including
one controlled by the other peer. The authority repeats native identity,
team/level/adjacency/obstruction, animation, service, kit, and AP checks; it
captures private prepared state before queueing `BeginFirstAidCommand`.
An Applied receipt proves treatment initiation, not completed bandaging.
Native startup is a fail-closed boundary: exceptions after AP/service mutation,
or a successful return with missing animation surfaces or broken service
association, invalidate the world before propagating. The retained command
cannot start treatment again in that world, and observer failure stops the
session before a terminal receipt can use stale state. This is containment of a
partial failure, not a claim that AP or medical state was rolled back.
Public medical state includes bleeding, usable equipped hand-kit points and an
eight-byte service descriptor: providing flag, optional six-byte patient identity
and unsigned receiving-provider count. Both roles may coexist, including native
self-bandaging; this does not enable a self-treatment intent. A patient identity
is projected only between active same-player-team actors in the same sector and
elevation. Non-player or unresolved service partners retain only activity/count,
not an inferred or hidden target. Native service storage remains slot-based;
capture resolves the current live incarnation but does not retrofit lifetime
identities into native associations. Canonical absence is accepted without a
published patient; malformed descriptors reject transactionally. Clients resolve
patient vitals only in the current committed snapshot, suppress redundant aid
starts, and clear target feedback on disappearance, replacement or service end.
Space uses the existing native Stop cancellation for either service role; an
incapacitated recipient is prompted to select the medic instead. Native turn
services advance treatment; zero bleeding alone does not declare completion.
There is no automatic kit equipping or transfer, full inventory mutation,
self-treatment, surgery, or auto-bandaging in this intent.
Outside targeting modals, clicking that patient's world tile submits the
request; roster clicks remain selection, and an ambiguous shared tile is
rejected. The native medic body/animation contract is also revalidated.
Interaction beyond this native treatment start, bounded inventory swap/transfer and
synchronous ordinary door action, general inventory management, burst/autofire, melee, thrown
attacks, and other actions remain future protocol work. Clients do not send an authoritative path,
action points, damage, death, inventory, world state, or AI result. For aimed
fire, the server resolves both entity incarnations, captures the live target
position and selected hand item, and revalidates turn, interrupts, visibility,
weapon/ammunition, target drift, aim, and action points before a local-only JA2
fire event. It publishes an ordered receipt plus committed state delta; the
client predicts neither action-point spend nor damage.

End turn, move, face, stance, stop, and reload reuse established command
shapes, so the dedicated translator marks each one with
`TacticalCommandAuthorityPolicy::DedicatedCoop`. Structural validation permits
that policy only for `NetworkPeer` and `Replay`, and simulation-command journal
retains the authority-policy serialization, so replay source substitution cannot erase co-op
authority. At execution, the common resolver repeats live-world and
controllable on-foot actor checks and, in combat, the player-turn,
no-pending-action checks. A resolving interrupt blocks ordinary input; during
an active player interrupt, ordinary actions require that actor's replicated
eligibility. End turn is rejected in that phase and `T` instead submits an
exact-serial, exact-incarnation pass vote. The final remaining eligible vote
resumes native turn flow without fabricating a move; active AI interrupts remain
under native AI control. Outside an active interrupt, end turn additionally
requires the exact next player team; reload repeats its weapon,
ammunition/chamber, and AP checks. The default `Legacy` policy intentionally preserves established
network-replica and system command behavior. Aimed fire retains its separate
strict synchronization-source resolver.

The one-command causal lock is also enforced at the untrusted server boundary:
each peer may have only one pending authoritative command. An exact-next
pipelined command is rejected with non-consuming `InvalidCommandSequence`
without entering replication, reservation, or gameplay, leaving its command
cursor available for retry after the prior terminal result. Global authority-
sequence exhaustion is different: `AuthoritySequenceExhausted` is reason 20 and
consumes the exact peer cursor. The server remains active long enough to flush
that terminal receipt; the client transactionally accepts its receipt history
and cursor, then fails and closes because the active authority cannot recover.

Legacy synchronization RPCs remain available only to the legacy arena mode.
Authoritative co-op must fail closed if a legacy client attempts to inject
effect or snapshot packets.

## Session admission

Transport connection is not session admission. Before any authoritative intent
is accepted, peers exchange a fixed-width, explicitly little-endian admission
message containing:

- authoritative co-op protocol version;
- server session epoch;
- runtime compatibility fingerprint;
- installed-content manifest digest;
- optional server-issued peer identity and reconnect token.

The live listener sends a fixed 72-byte server hello before processing the
current 116-byte request. Neither has a separate application or campaign-name
field: the session is derived from the server endpoint and the exact runtime
and installed-content identities opened by that worker, never from
client-provided prose.

An all-zero identity/token requests a new seat. A successful response returns a
unique identity and opaque reconnect token, but that seat remains pending and
non-authorizing until the same transport returns the exact fixed 64-byte ACK.
Pending seats are reclaimed on disconnect, timeout, rejection-budget
exhaustion, or listener shutdown. A reconnect presents the pair, atomically
replaces the previous transport binding, and must ACK that new binding before
it can authorize gameplay. The registry binds identity to the
transport-derived opaque connection ID; gameplay payloads never select their
own sender identity.

Reconnect tokens travel over the current plaintext transport. They are bearer
identity continuity, not encryption, Internet authentication, or protection
from an on-path attacker. Public Internet hosting requires a separately scoped
secure transport or authenticated tunnel.

The production passive client durably publishes an admitted identity/token
before sending the 64-byte ACK that makes the seat authoritative. Its private
campaign directory, outside the mounted profile/VFS, stores one 224-byte record:
the canonical 128-byte bootstrap descriptor, canonical 64-byte `AdmissionAck`,
and a 32-byte SHA-256 digest. Private staging is flushed and atomically published;
rewriting an already exact record is idempotent. After installed-content and
runtime compatibility are verified, startup loads the record. An exact same-
epoch bearer is restored after client configuration and before socket connect;
an otherwise identical descriptor from an old epoch is erased only at that late
verified boundary. Corruption, unsafe permissions/path identity, or any non-
epoch binding mismatch fails closed without returning or silently erasing the
bearer. The live hello is pinned to the preflight epoch and closes before an
admission request if it differs. New or revalidated credentials are synchronously
persisted before ACK and in-memory adoption; storage failure closes the socket
without claiming the seat.

The private credential loader also recognizes the unchanged protocol-14, protocol-15 and protocol-16 record
layouts during the protocol-18 upgrade. It validates the original outer SHA-256,
bootstrap checksum, schema and reserved bytes, then checks all semantic fields
through the current codecs using a temporary copy. Only an exact campaign seed,
identity, runtime fingerprint and content manifest with a different server epoch
may follow the existing stale-session retirement path. Same-epoch protocol
mismatches remain invalid; retired markers retain their original bytes and
terminal meaning. Live admission and bootstrap decoders still reject protocol
14, and an old bearer is never converted into a current-session credential.

Once a durable same-epoch bearer is retained, reconnect attempts are not capped:
this lets an already admitted seat wait through the server's intentional post-
combat admission blackout. The unsigned retry counter saturates rather than
wrapping. A never-admitted credential-less startup remains capped at eight
attempts, and a retained bearer from a different descriptor epoch fails closed.

Protocol v3 adds voluntary self-retirement with no selectable victim field. Its
exact 24-byte request contains only the protocol version, session
epoch, and a nonzero request ID. The listener first authenticates the transport,
and the admission registry resolves that binding's own identity; no peer
identity or bearer in the request can name somebody else. A 48-byte result
returns that server-resolved identity and the exact request ID. Malformed,
cross-epoch, unauthenticated, or conflicting requests fail closed.

Beginning retirement atomically reserves one of 64 bounded same-epoch retired-
credential slots and marks the active record Pending. Pending immediately
revokes gameplay authorization while retaining transport authentication needed
to drain and return a truthful result. A reconnect with the exact bearer sees
`CredentialRetirementPending`; after commit it sees `CredentialRetired`, while
a wrong token still sees `InvalidReconnectToken` and gets no tombstone oracle.
Exact begin and completion replays are idempotent. If tombstone capacity is
full, the server returns `TombstoneCapacityReached` before revoking gameplay or
freezing input, so the client may resume its prior state.

The first accepted request is a global input boundary, not merely a per-peer
mute. The listener globally freezes admission/ACK/abandonment, tactical, and campaign
input, captures the one retirement request, and discards both bounded inbound
FIFOs plus the tactical server's deferred frame. The committed-frame runtime
then waits only for already-authorized local work—host correlations, immediate
and pending receipts, the command inbox, deferred cancellations, and tracked
commands—to drain. Unrelated `RuntimeMessageBus` publications cannot starve
retirement, and campaign/delta ACKs are not prerequisites after the global
freeze. The registry copies identity, token, and request ID into the same-epoch
credential tombstone and removes the active seat before the listener may send the
best-effort success result. Thus result loss is recoverable by presenting the
old bearer; the server never reports success before its authoritative fact.

After the tombstone commits, the dedicated runtime stops admission and
reconciles every transport-facing layer to Offline before compacting the
retiree from replication assignments/history, command peers, actor ACLs,
authority sequences, campaign Ready membership, and the world-participant set.
The compaction preflight is all-or-nothing and stable: surviving cursors,
pending counts, receipt bytes, and replication state remain exact. Assignment
publication is reset so survivors and a distinct replacement enter through
fresh baselines. Tests prove four seats compact to three, preserve every
survivor, and admit a distinct fifth identity with a fresh command cursor of one.

The client UI exposes this irreversible operation as a bounded two-step `L`
gesture. The first key-down must be followed by a physical release and a later
second key-down; repeats cannot self-confirm, other commands/mode changes cancel
the prompt, and both waiting and presentation screens drain the complete input
FIFO. Before its first send the core retains the request ID in RAM. A
same-process reconnect sends its normal admission ACK first and then replays that
exact request instead of silently reauthorizing gameplay; a pending response
also preserves the request and bearer until later convergence.

Once the server proves `CredentialRetired`, the client must atomically rename
the exact private 224-byte bearer to
`client-reconnect-credential.retired` before it clears credentials, enters clean
`Retired`, or closes transport. The no-replace marker retains the canonical
bootstrap + `AdmissionAck` + SHA-256 bytes, stays mode-private, is idempotently
exact-checkable, and cannot be overwritten or treated as stale. Startup
validates it after content/runtime compatibility and enters terminal Retired
before constructing a network composition, even when only the server epoch has
changed. Active-plus-retired ambiguity, corruption, unsafe storage, or marker
publication failure fails closed and retains evidence.

One narrow combined failure is intentionally documented. If the authority has
committed its tombstone, the client fails to publish the marker before the
atomic rename (so the active `.bin` bearer remains), and the authority then
independently rolls to a new epoch before same-epoch convergence, a late
compatibility-verified startup classifies that still-active record as
`StaleSession`, erases it, and may make a fresh admission. Same-epoch startup
continues to fail closed; durable cross-epoch terminality begins only after the
`.retired` marker has published.

The current first-join policy is permissionless up to the configured seat
limit; the ACK proves receipt of a server-issued credential, not a human user
account or access grant. This is a trusted-LAN/VPN milestone only. Public
hosting additionally needs operator authorization and encrypted transport.
ACK-confirmed identities otherwise form a fixed admission-seat roster for the
process epoch: ordinary disconnect removes only their transport binding, not
their seat. The authenticated player may now relinquish only their own seat
through the committed retirement boundary above. Operator-forced revocation,
credential expiry, ownership transfer, and administrative eviction are not
implemented. A campaign-ready seat
that arrives after the active world's first cohort may still join that world,
but only when every command, inbound message, receipt, and replication
obligation has drained. At that boundary the server publishes a canonical
grow-only participant union, redistributes assignments, and sends fresh
baselines; the new actor ACL remains closed until the exact baseline ACK.
Disconnect never removes a world participant or transfers that actor to another
identity.
Session epochs, peer identities, and reconnect tokens are generated directly
by the host OS CSPRNG (`BCryptGenRandom` on Windows and `/dev/urandom` on POSIX)
and fail closed if that source is unavailable; they never use the deterministic
campaign RNG.

Unknown protocol versions, malformed lengths, incomplete authority
configuration, fingerprint/content mismatches, stale epochs, duplicate live
bindings, invalid tokens, and capacity exhaustion are explicit rejection
reasons. Disabled or incompletely configured authoritative mode admits nobody.

## Intent envelope and ordering

Each authoritative intent carries:

- the admitted session epoch and server-assigned peer identity;
- a monotonically increasing client command identifier;
- tactical world generation and base revision;
- current turn/interrupt serial where relevant;
- stable `TacticalEntityId` actor identity;
- one closed, versioned intent payload.

The network ingress replaces payload identity with the transport-bound peer,
checks the peer-to-actor ACL, rejects duplicate/stale/future identifiers, and
then maps the intent into the existing deterministic command queue. Structural
command validation is necessary but insufficient: gameplay legality currently
performed in UI code must move to the authoritative admission/executor path
before that action is exposed remotely.

The server response includes the client command identifier, authoritative
sequence, simulation tick, terminal status, rejection reason, and resulting
world revision. TCP ordering is not used as a substitute for these application
identifiers.

The data-free foundation now includes exact codecs for move, face, stance,
stop, end-turn, aimed-firearm, zero-payload reload, door open/close, interrupt
pass, first aid, inventory swap and same-owner transfer intents, plus a bounded authority gate. The
gate resolves the peer from the transport-bound admission registry, verifies the claimed identity,
world generation, exact revision and turn serial, enforces a per-peer monotonic
command sequence, and checks a server-owned peer-to-actor ACL. The reusable gate
executes no game action itself; the production dedicated composition now hands
its sanitized value to the main-thread JA2 tactical host, which rechecks live
turn/actor policy and submits the twelve supported intent kinds to the bounded
simulation command queue.

The reusable authority gate remains exact. Before calling it, the dedicated
coordinator may normalize an already acknowledged revision to the current
revision only across a proven consecutive animation-frame-only lineage.
`IsFrameOnlyTacticalWorldChange` compares the complete predecessor/result
snapshots and their exact delta, allowing only
`presentation.animationFrame` to differ. Surface, palettes, precise position,
lighting, turn/interrupt state, actor membership, equipment, and all other
fields remain significant. A monotonic command-revision floor prevents a
gameplay change followed by a reversal from reopening an older state.

Normalization requires the original revision to be at or above that floor
and no newer than that peer's acknowledged revision, with the same world and
turn. Identity, actor, command cursor and action are never rewritten. The
typed ingress revalidates structure and runs the unchanged exact authority and
native execution checks. Revision gaps, unclassified delta-only publication,
fallback/staged baselines, assignment or world changes, pending native work,
and execution close the window. The execution barrier runs before receipt
handling, including early backpressure when a native result is still pending;
optional cache-allocation failure falls back to strict admission. The cache uses the trusted observer's canonical paired
snapshot, not a client-supplied reconstruction. Replication ACKs, resync and
checkpoint/drain readiness remain exact, and no wire format changes.

`FullEngineCoopIngress` now composes those contracts behind an explicit
admission/tactical-generation lifecycle. It
decodes admission and intent bytes, derives authority only from the receiving
transport, and hands an authorized, identity-sanitized value to an injected
execution sink. `DedicatedCoopRuntime` constructs that sink only after the exact
campaign identity opens, registers it with the runtime message bus, begins the
admission epoch and tactical server, and then starts a separate SDL_net
listener. The listener's inbound handlers cover admission request and ACK,
one-shot credential abandonment after an exact `UnknownPeer` reconnect,
tactical intent, baseline ACK, delta ACK, campaign-sync ACK/result/resync, and
authenticated self-retirement, and the implemented exact tactical resync
request in the same bounded dispatch.
They never touch the legacy PvP RPC table or self-client. Tactical callbacks
merely resolve the ACK-confirmed transport identity and copy exact, bounded
bytes into a main-thread queue.
The tactical and campaign queues are separate bounded shared FIFOs. When one is
full, the listener disconnects and evicts another transport only when that
producer owns strictly more queued entries than the incoming sender, then
retains the incoming frame. If no producer is strictly dominant, the incoming
sender fails closed; equal producer counts do not choose an arbitrary victim.
Permissionless first join remains transport-bound trusted-LAN admission, not
human-user authentication.

`CoopTacticalProtocol` and `FullEngineCoopServerSession` provide the next pure,
data-free layer: exact receipt, baseline, baseline-ACK, delta, and delta-ACK
codecs plus bounded peer phase, actor assignment, resend, history, and
backpressure bookkeeping. They own no JA2 pointers and execute no gameplay.
The outer production server now composes them with the listener, tactical
observer, JA2 command host, committed delta publication, and ordered queued/
terminal receipt delivery. Baselines carry the admission-epoch command cursor
in their 76-byte header and baseline ACKs echo it in 88 bytes before actor ACLs
activate.

The passive side is now composed into the normal `JA2` executable.
`FullEngineCoopClient` owns the bounded admission/tactical state machine,
`FullEngineCoopClientTransport` owns the real SDL3_net connection, and
`FullEngineCoopSnapshotReplica` publishes only transactionally committed
baselines/deltas. A separate campaign-sync client receives bounded chunks,
verifies the declared checkpoint hash, atomically commits private scratch, and
cold-loads that checkpoint before it reports Ready. Tactical baseline or delta
publication before campaign Ready is fatal rather than speculative.

Live campaign observations travel independently over the exact server-to-client
`coop.campaign.status` namespace: a fixed 96-byte, little-endian record at the
current protocol version. The dedicated committed-frame pump reads
native campaign seconds, phase, remembered compression mode, pause/lock/
compression/interruption flags and campaign-ready peers. Its bounded ledger
increments a separate revision only when those observations change; invalid
states or same-session clock rollback fail closed. Delivery is authenticated,
coalesces full replacements under backpressure and resends on a fresh transport
even if the observation itself did not change. Failed sends never advance the
delivery cursor. No status ACK is needed. Bytes 56–63 hold a nonzero
time-control revision, independent of ordinary clock ticks. Native pause/rate,
phase, arrival-decision or leadership-readiness changes advance it, as do
consumed time and shared campaign-action requests.

The final 32 bytes describe the front native arrival decision: non-reused ID,
NPC/battle/attack-coordination kind, preparation stage, sector, remaining queue
count and (only when prepared) native participant counts and permitted battle
choices. This is cached read-only data, not a remote action or permission grant.
Reinforcement questions and refused native contexts are distinguished from a
prepared battle. No opponent roster, native pointer, RNG or second encounter
calculation crosses this channel. An absent decision is a canonical zero
replacement. Both ledger and client retain a decision high-water mark across
that replacement, so a consumed decision cannot reappear under a newer status
revision. Same-ID updates cannot silently retarget the sector or kind. A fresh
transport requires a fresh observation; disconnect/retirement/failure hide it
with the rest of campaign status.

The worldless client waiting screen displays the native notice and
explains the held campaign clock. Time shortcuts and the shared time-request
validator reject attempts during an arrival hold, including attempts by the
time leader. Protocol 14 connects the supported native travel/battle choices
to a separate authenticated action API and ordinary client controls, described
below. The notice itself remains an observation; native eligibility and current
authority are revalidated for every request. Matched client/server builds are
required.

`full_engine_coop_installed_arrival.py` is an opt-in observation fixture. Its
test-only server executable seeds a native A10 battle, leaves it prepared and
paused, and runs normal admission/status delivery for two independent test
clients. It checks shared metadata, reconnect and clearing after server loss,
and requests bounded native PNG captures of the actual waiting screens. The
test does not authorize travel or entry, drive physical OS input, or constitute
two-client campaign-play acceptance. It uses private process/state roots and
checks that installed inputs remain unchanged.

Native retreat is a separate internal committed-frame action on a sole,
exactly prepared battle. It shares the ordinary retreat button's records,
group departure, loyalty effects and enemy response without touching widgets.
Only native-permitted surface on-foot squad retreats are supported. Every
previous sector, native travel cost, member/route owner and existing movement
event is checked before paths are freed; a militia autoresolve continuation is
not silently chosen. Retreat uses fresh movement-cost modifiers, not the last
map-screen plotting cache. Native arrival/warning events and each participant's
transit state are checked after gameplay. Success clears the decision while
leaving strategic time paused; subsequent native event processing owns the
return journey. Allocation/queue/RNG failure after mutation latches permanently,
including failure to schedule the warning after the main arrival was posted.
Headless event failures unwind to that owner instead of entering the legacy
assertion renderer. This action grants no peer or time-leader authority.
`full_engine_coop_installed_entry.py --scenario retreat` qualifies a seeded
installed retreat and native enemy response, not remote player choices.

The first observed ready cohort designates its lowest peer identity as the
session's time leader. Later lower identities cannot take over. Disconnect,
retirement and world teardown retain the leader but remove readiness; the same
identity becomes ready on reconnect. A new server session selects afresh. This
is the current development default, not a completed manual designation/handover
or durable leadership system. It grants no spending, hiring or tactical actor
permissions. Strategic time pauses on detected leader loss/unreadiness or
voluntary leader retirement; reconnect does not resume it. This applies only
to worldless established strategic state, not tactical combat or startup.

Friendly movement groups use the separate server-only `coop.campaign.groups`
observation namespace at the same development protocol version. A 32-byte header
contains session/revision, availability and counts, followed by 32-byte group
records and 16-byte member records (maximum 255/256 respectively: 12 KiB total).
Records carry exact native group/actor incarnations, live profile/assignment,
sector, transit/vehicle/transport flags, next-sector native arrival/traverse
minutes and the final waypoint. No enemy union, private inventory, complete path
or whole-route ETA is exposed. Idle next-sector/timing fields are canonical zero.
Groups and each group's member slice are sorted by slot; duplicates, invalid
coordinates, nonadjacent next legs and inconsistent slices are rejected.

The committed server boundary captures only stable tactical/strategic phases.
List traversal is bounded before identity gateways; native incoherence publishes
an unavailable replacement and a diagnostic rather than a partial/stale roster
or campaign-fatal observer error. This path never performs native eligibility
checks (which can mutate fatigue), pathfinding, route rebuilding or movement.
Its independent ledger coalesces unchanged state, and delivery cursors advance
only after successful authenticated sends. New transports receive replacements
even at the same observation revision. Passive clients reject wrong epochs,
regressions and contradictory same-revision frames; campaign readiness gates
the copied runtime view, retirement hides it and disconnect clears it. Tactical
baseline/resync alone does not erase this independent campaign view. This
observation channel remains separate from the protocol-14 action requests;
clients never mutate groups from received observations.

`PlanDedicatedCoopTravel` now provides a separate, main-thread native preflight
for an idle surface/on-foot group in an established worldless map-screen host.
It first validates the bounded live graph, then checks the exact group/member
incarnations, locations and existing routes. It calls shared native movement
eligibility with UI-message and fatigue-collapse side effects suppressed; an
unrelated selected mercenary does not become part of the requested movement set.
The museum's room-dependent restriction explicitly returns context unavailable
without its exact tactical map, rather than reading an absent/stale room table
or bypassing that restriction. Vehicles, underground groups and existing travel
are outside this initial planner's supported scope.

The same native strategic search now accepts explicit query options and private
scratch/output. This preflight uses fastest-route terrain costs, with the actual
members' inventory, survival traits and native modifiers, independently of Shift,
enemy-avoidance plotting flags and the GUI's shared encumbrance cache. Legacy UI
plotting retains its existing options/cache. The new query can return all 255
adjacent legs across the 256-sector map instead of silently truncating at the
legacy tactical path limit. The planner validates every leg, destination and
native timestamp range, and publishes only a complete pointer-free candidate.
Any failure leaves the previous output unchanged. Preview creates no events,
paths or waypoints and changes no actor, clock or GUI state.

That preflight is separate from `StartDedicatedCoopTravel`, the initial native
single-leg commit. Start replans from current state and accepts only one adjacent
surface/on-foot leg; longer routes are rejected, never silently truncated. Active
event dispatch, tactical traversal, patrol randomization, pre-battle UI, actors
still in-sector and crossing groups needing native event/RNG rescheduling are
rejected. Existing arrival/warning events are a conflict, not implicitly erased.
Native path nodes and the waypoint are allocated privately, then an all-or-nothing
arrival/warning event batch is inserted before changing the group or any member.
`CampaignEventQueue::scheduleBatch` preserves existing node addresses/IDs and
equal-timestamp FIFO; failed preparation preserves the next event identity too.
Five-minute town travel posts only an arrival, not an underflowed warning.

Successful commit adopts a separate native path for each exact member, sets
native next-sector/arrival/transit state and performs native tactical departure
bookkeeping. It neither advances time nor consumes RNG. The existing fixed-step
clock and ordinary `EVENT_GROUP_ARRIVAL` callback own arrival, including campaign
Lua liberation effects. An engine-linked regression with read-only test-owned
Lua content proves a peaceful A9-to-A10 trip for both actors, completed-path
cleanup and exactly one scripted native effect. It does not mount retail data or
certify an installed two-client travel/encounter workflow. The existing live group
observation can capture committed transit state without another replication path.

The dedicated runtime now binds `DedicatedCoopArrivalState` after campaign entry.
Native wilderness-NPC, simultaneous-attack coordination and arrival-battle
detection defer their dialogs into bounded, exact-group decisions when the host
is worldless. NPC exclusions, native bloodcat rules, friendly simultaneous
arrivals and group validation still run first; this does not replace them with a
new encounter classifier. Unbound single-player/legacy multiplayer keeps its
normal dialogs. Capture pauses before pre-battle preparation. A separate native
preparation step can now run without answering or consuming the pending record;
it is not a completed battle-entry action or resolved encounter.

Each decision records a non-reused local ID, group/dialog-group incarnations,
sector, native observation time and encounter code. Multiple notices are retained
in FIFO order; identical observations coalesce. Capture errors/capacity exhaustion
retain accepted notices, keep time stopped and produce an explicit host failure,
never a silently accepted choice. Native resume eligibility, the fixed-step clock
and new departures all honor the hold. The runtime logs the pending decision and
blocks cold checkpoints: these pending continuations are not serialized yet.

An internal exact-ID NPC reply can acknowledge a completed trip or stop a retained
route using the native operations, without GUI selection or automatic unpause.
Changed clock/group/sector/route-completion context is rejected. Stop validates
each member's path links and separate ownership before freeing any route.
Continuing a multi-leg route, coordinating an attack, retreating, autoresolving
remain unsupported replies; they leave the decision pending. Battle entry has
its own explicit action below, not an NPC acknowledgment side effect.
Engine-linked tests exercise real scheduled NPC/enemy/bloodcat/coordination
arrivals, exact event-time interruption, retained later events, native Lua effects,
multiple notices, stale identities and damaged/full decision/path state.

For supported surface on-foot arrivals, the runtime calls the exact-once
`PrepareDedicatedCoopArrivalBattle` boundary. It validates current time, exact
group/dialog identities and bounded membership before calling
`PrepareHeadlessPreBattle`. Native participants, robot/wake rules, ambush rolls,
scout prevention, leadership deployment, experience records, enemy knowledge and
action restrictions are shared with the ordinary PBI, not reimplemented in a
co-op encounter engine. The native coward morale rule is shared too. Arrival
captures the just-retreated flag before native event cleanup erases it. Normal
speaker RNG consumption is retained without queuing GUI dialogue or its pause
lock; bloodcat notifications retain their separate no-quote path.
Low-morale warnings retain their native one-time quote flags while suppressing
the local portrait continuation only during active headless preparation.

Logical headless battle state supplies both native battle-sector queries without
setting `gfPreBattleInterfaceActive` or creating widgets. The GUI initialization
and automatic PBI continuations cannot replay preparation while it is occupied.
Successful retries validate the native group projection and return cached
results, preserving RNG, morale, ambush records and radius. Preflight rejects
before mutation; an exception or simulation RNG failure after starting latches a
host failure with no result publication. The pending decision still prevents
time advancement, new travel and checkpoints. Teardown clears only the owned
logical context. Adjacent militia requiring a reinforcement choice and currently
unsupported unconscious/militia-only, nonplayer, traversal and coordination
contexts remain pending. Nothing calls GUI buttons, chooses a result, loads a
sector or takes a global pause lock through this preparation API.

The separate `EnterDedicatedCoopArrivalBattle` action now enters a supported
prepared surface/on-foot battle through `EnterHeadlessPreBattle` and the ordinary
`SetCurrentWorldSector`/`EnterSector`/`PrepareLoadedSector` path. It requires the
exact sole pending decision and unchanged prepared group/member incarnations.
Native forced insertion is accepted only for encounters without a deployment
choice; ordinary and leadership-enabled ambush deployment require an explicit
`Spread` choice. The local GUI and headless path share participant selection,
insertion-side fallback, incident flags, native edgepoint/sweet-spot RNG, facing
and preview-piece placement/removal. Headless entry never opens the overhead
deployment screen or silently chooses Spread. Manual deployment remains open.

Entry preflight rejects unsupported vehicle/airdrop/POW/robot/EPC and assignment
contexts, unsafe actor/waypoint ownership, conflicting UI/loading/pause state and
missing real maps before route/quest/world mutation. The native map-name probe
preserves the one-shot alternate-map flag. The actual loader also refuses
placeholder substitution and propagates temporary-sector integrity failure
without a local dialog. Native squad/path cleanup, sector-entry quests, map/temp
state, participant/enemy population and AI initialization still run in their
normal order. Success consumes the pending decision only after native world and
actor placement postconditions and simulation RNG health pass. A partial load,
exception or RNG failure latches the host failure; retries cannot reload or
reroll it. Other queued worldless decisions cannot be invalidated by entry.

The runtime's internal committed-frame `enterArrivalBattle` requires strategic
idle and a fully drained prior tactical world. Success rejoins ordinary
controllable-actor readiness, fresh observation/baseline admission and the
victory/drain/cold-checkpoint lifecycle. It is not automatically invoked by the
pending-decision pump. The separate opt-in `full_engine_coop_installed_entry`
target boots real installed data and seeds disposable native A10 encounter
fixtures; it is not linked into shipped apps and does not prove remote travel,
decision authorization, two-client gameplay or physical-input acceptance.

Shared campaign actions use `coop.campaign.action.request` (56 bytes) and
`coop.campaign.action.result` (80 bytes), introduced at global protocol 14 and
retained in protocol 18. Requests carry
the session epoch, observed control and group revisions, a per-transport request
ID, an exact group incarnation plus adjacent destination for travel, or an exact
pending decision for an arrival action. They contain no claimed peer identity.
The listener copies only exact-width authenticated bytes into its bounded
campaign FIFO. The committed-frame runtime resolves the current transport again
and checks both the pre-poll readiness gate and current checkpoint readiness.

The user-confirmed policy permits any campaign-ready player to request shared
travel and supported battle choices; time leadership is not required. Eligible
requests reach `StartDedicatedCoopTravel`, the native NPC acknowledge/stop
boundary, or the explicit forced/Spread entry and retreat boundaries above.
Travel and arrival choices require worldless strategic phase, current
observations and exact live native context. Prepared battle entry/retreat requires one pending encounter;
unsupported contexts remain held rather than acquiring a fallback outcome.

`CoopCampaignActionAuthority` reserves one retained terminal result per exact
authenticated identity/transport pair. It consumes the shared time-control
revision before the native callback, including native refusal. Consequently,
another action or leader time request based on the old observation cannot
mutate the campaign. Exact duplicates return their historical outcome; older
or conflicting request IDs cannot overwrite a pending or delivered outcome.
Backpressure and same-transport resync retain receipts, while replacement
transports discard them. Native partial failure stops the authority instead of
allowing a replay. The runtime recaptures committed status/groups after handling
an action and delivers their current observations before sending the terminal
receipt. The passive client accepts only its matching outcome and clears
unknown requests on disconnect.

Protocol 17 distinguishes native attack completion from accepted dispatch.
The server retains the exact shooter incarnation, world and authoritative
execution sequence through the ordinary attack-busy drain. Native completion
uses that shooter rather than the UI-selected mercenary. `Applied` is withheld
until native gun ammunition accounting has recorded a discharge and its chain
has completed. A chain that ends before firing produces `Cancelled` with reason
21 (`NativeAttackInterrupted`); native stance/turn AP costs can still have been
charged. Receipt backpressure retains the exact outcome and prevents another
attack from overwriting it. Neither result claims a hit or kill. Lost world or
actor identity fails closed. Clients require the matching protocol at admission.

Protocol 16 adds an exact native surrender decision during a loaded tactical
battle. The 96-byte campaign status uses bytes 86–93 for the pending offer ID;
bytes 94–95 remain reserved zero. A nonzero offer requires paused tactical
status without time compression or an arrival decision. Action values 7 and 8
mean decline and accept surrender; they retain the existing request/receipt
widths, authentication and shared control-revision barrier. The native
continuation retains world generation, turn serial and speaker incarnation,
consumes the offer before mutation, and completes that exact speaker's action.
Transport and observations continue while native gameplay and checkpoints hold.
Both players receive the same modal choice: `N` continues fighting, `Y` accepts
capture, and a later released `Enter` confirms. Changed context clears the
armed choice. Installed qualification requires an explicit surrender choice;
it never supplies an automatic answer. The capture continuation is connected
but captivity and its persistence are not yet qualified.

The ordinary worldless co-op Screen provides group selection with `Tab`/`]`
and `[`, arrow-key adjacent destination selection and `Enter`/`Esc` confirmation
or cancellation. It shows native transit destination and arrival time rather
than predicting movement. Supported NPC notices expose final-destination `A`
acknowledgment or interrupted-route `S` stop. A sole prepared battle exposes `E`
for the native forced insertion or explicit Spread choice, and permitted `R`
retreat requires `Enter` confirmation. Held keys cannot repeat commands; changed
control/group/decision context cancels armed choices. Outstanding time or action
requests disable both families until their terminal result. Neither these
controls nor their input helper perform local campaign simulation.

This is implemented source behavior, not a new installed two-client
travel/encounter acceptance result. Multiple legs, crossing-group rescheduling,
remaining preparation contexts, manual deployment, autoresolve and a complete
strategic interface remain unfinished. Hiring/shared spending permission is
also shared by user policy, but the authoritative transactions and client
controls are described below and still require installed acceptance. Travelling/wounded disk save/resume and
active-battle persistence still require implementation or qualification as
tracked in [COOP_COMPLETION.md](COOP_COMPLETION.md).

Global protocol 15 adds `coop.campaign.economy` (40-byte header plus up to
260 32-byte roster entries), `coop.campaign.aim.quotes` (48-byte header plus up
to 255 52-byte offers), and `coop.campaign.hire.request` / `.result` (56 / 96
bytes). Economy and quotes have independent monotonic observation revisions;
every quote replacement binds to its exact economy revision. A quote includes
native willingness/availability, three salaries, the enabled medical deposit
and checked totals. Optional equipment is currently unavailable because its
legacy copy routine does not report complete transfer. No local laptop price,
gear selection or hire function authorizes a remote request.

Any campaign-ready player may hire. The exact authenticated transport's
request binds to current control/economy/quote revisions. The hire authority
consumes the same control barrier as travel and leader time before invoking
native code, retaining the result across backpressure and same-transport resync.
Native revalidation includes profile availability, willingness, duplicate
actors including IN_TRANSIT, native capacity, current landing/arrival context,
signed price limits, sufficient funds and read-only finance/history append
readiness. Checked actor creation and one delayed event precede the checked
salary/deposit/history writes. Any partial failure stops the dedicated process
without an assertion autosave; an old checkpoint remains the recovery boundary.

The server recaptures status/groups/economy/quotes after each queued action.
Quote delivery waits until the referenced economy is queued for that exact
transport, and receipts wait for the current observations. Receipt economic
stamps describe eligibility, not a committed checkpoint. A first Applied client
checks the echoed request and quoted total plus the exact actor/profile in its
current roster before releasing input. Exact historical receipts remain
unchanged and cannot release a newer request. Reconnect clears unknown pending
hires without replay. Time, travel and hiring share one client pending guard.

The worldless Screen's `H` panel displays synchronized profile labels and server
offers, shared funds and capacity. `1`/`2`/`3` select one/seven/fourteen days;
`Enter` arms the exact price summary, then a released second `Enter` confirms.
Changed session/control/economy/quote context cancels confirmation. The panel
suppresses travel/time keys while open and performs no campaign simulation.
The opt-in installed driver uses those same shipping key events; its probes
copy observations and never invoke the hire function directly. Native arrival,
in-transit persistence, optional gear and renewal remain separate qualification
and implementation work.

Clock commands use the bounded `coop.campaign.time.request` (32 bytes) and
`coop.campaign.time.result` (40 bytes) namespaces at the current development
protocol version. Requests contain session, control revision, request ID and
one absolute action: pause, 5, 30 or 60 campaign minutes/real second. No peer
identity, toggle, arbitrary compression, clock warp or tactical pause is exposed.
The listener copies authenticated requests into its separate campaign FIFO.
At the committed boundary the host re-resolves the exact current transport,
checks both pre-poll and current campaign readiness, the designated leader,
revision and worldless strategic phase. Native `TrySetWorldlessStrategicTimeCompression`
also rejects loaded worlds and startup, preserves `AllowedToTimeCompress` and
first-compression cleanup, and never calls the native GUI refusal path. It
does not advance time or clear an event interrupt/pause lock; the regular
fixed-step scheduler remains authoritative.

The authority reserves one terminal-result slot per authenticated transport
before executing. Eligible requests consume their control revision even when
the native guard refuses them, preventing delayed retries from taking effect
after an obstacle disappears. Exact duplicates return the historical outcome.
Same-connection campaign resync retains queued outcomes; replacement transports
discard them. Results are queued only after the current authoritative clock
observation. The passive client permits one outstanding time or shared-action
request, validates its exact result and observation lineage, and clears unknown outcomes on
disconnect instead of replaying a resume. The interim strategic waiting screen
provides `P` / `1` / `2` / `3` and explicit pending/refusal feedback with a physical
down/up latch; tactical input and the read-only tactical clock overlay are unchanged.

The client validates exact width, reserved bytes, session and revision lineage,
then exposes a copied observation independently of the tactical baseline. Clock
or role data disappears on disconnect, retirement or failure; a new connection
must receive fresh status. A small read-only native-view overlay and the waiting
screen use only these server values. `pause off` does not claim that time is
advancing: native combat/events may still hold the scheduler. Local cold
checkpoint time is never presented as live server time. The installed driver's
`CAMPAIGN` command observes this same runtime getter without sending gameplay.
Its opt-in `--verify-campaign-status` test checks exact two-client agreement,
disconnects the observed leader, requires the other client to report that
identity offline without taking over, reconnects the same credential, and checks
that server shutdown removes all live campaign fields. These observations do
not qualify travel, time-speed commands, pause-on-loss or subsequent encounters.

The passive game-loop branch never advances the local JA2 clock, frame driver,
package host, runtime-message bus, command queue, observer, AI, pathing,
campaign, or tactical simulation. During startup, resynchronization, or a
fail-closed presentation rejection, `INIT_SCREEN` renders a worldless
sector/turn header, an exact authority-dimensioned isometric diamond with
friendly-team actor markers, and a committed actor-state table fallback. When
the transactional presentation gates are ready, the same client loop enters
`GAME_SCREEN` but calls only the passive static-world and actor renderer. It
does not fall through to `HandleTacticalUI`, the tactical panels/radar, legacy
scrolling/input, visibility calculation, clocks, events, or simulation.
Camera centering shares the legacy bounds and forced-correction arithmetic, but
uses `ApplyScrollingForStaticPresentation`: neither focus refresh nor a full
static render consults dialog/editor/mouse policy or mutates exit markers, the
debug overlay string, positional audio, or other interactive scrolling state.
The authority always retains player-team actors in that snapshot. Every
non-player actor requires the player team's exact public `SEEN_CURRENTLY`
knowledge; loss and reacquisition become ordinary actor-left and actor-entered
deltas.
The isolated controller identifies the client's server-assigned actors and
reports outstanding commands and terminal receipts. It drains the complete
input queue in both INIT and GAME views before legacy tactical handling can
observe it. Outside every modal, direct movement is an
allocation-free exact-grid calculation: Up applies row -1/column -1, Down
+1/+1, Left +1/-1, and Right -1/+1. Both the current and destination grid must
be within the authoritative dimensions, and no replica position is predicted.
Tab or `]` selects the next assigned actor; `[` selects the previous one. `M`
retains numeric destination entry. Together with `Q`/`E`, `1`/`2`/`3`, Space,
and `T`, these controls produce five typed movement/turn intents. Outside
destination entry, `R` sends the selected actor's
zero-payload reload; while entering a destination, it retains its reverse-move
meaning. `F` enters target mode; Up/Down/Tab cycle replicated opposing actors,
`+`/`-` adjust bounded aim, and Enter submits aimed fire. `D` enters modal door
selection; Up/Down/Tab cycle projected same-tile or
cardinally adjacent doors, Enter submits the exact inverse state, and Esc
cancels. In the native map viewport, clicking one of those exact published
door grids changes the retained door token without submitting or falling
through to movement. Door mode blocks the voluntary-leave confirmation gesture. A committed
`commandsBlocked` bit disables every action and closes destination, attack, and
door modes until a later committed baseline or delta clears it. The
three modes also retain their originating world generation, cancel on a world
replacement, and reject direct use with a mismatched view. The
one-command lock remains held from submission through the authoritative terminal
receipt; the screen never predicts movement, AP spend, or damage. Mouse,
key-up/repeat, and unsupported events are discarded by the same drain, so they
cannot leak into a legacy JA2 screen or simulation path.

This gives players a JA2 static-terrain/actor viewport plus a technical
protocol-control surface, not the complete JA2 tactical interface. The first
object in each of five combat-equipment slots is replicated, including loaded-
ammunition state for the two hand records. The
owner-only inventory channel additionally supplies dense carried-slot summaries
and a separate private revision, but not full object graphs. Complete stack
tails, attachments/LBE, cross-owner transfers and other inventory actions remain future work.
General structures, door lock/key/trap state and asynchronous door work,
complete per-peer
visibility/opponent knowledge, bombs/smoke, projectiles/explosions,
interactions, broader attacks, interrupt detail, and pending asynchronous
actions are not yet complete replica domains. Friendly-only plot markers are a
presentation choice: the table exposes every record that passed the shared
public-current-visibility gate, including non-player actors. That server gate is
data minimization, not a complete per-client fog-of-war or confidentiality
system.

## Tactical client presentation migration

Authoritative multiplayer is not a replacement game interface. Co-op and the
eventual PvP migration must converge on the ordinary JA2 tactical screen while
changing which process owns gameplay mutation. The existing legacy PvP path
keeps working during that migration; the authoritative client must not opt into
the legacy `is_client`/`is_server` replication globals because those globals
also enable local simulation and hundreds of legacy rule branches.

The migration is deliberately staged:

1. Gate `MainGameScreenHandle` with an explicit authoritative-client
   presentation-only branch. Ordinary single-player and legacy PvP continue to
   run the existing complete path; the co-op client cannot fall through into
   its local-command, tactical-event, or campaign-clock work.
2. Add a presentation-only sector loader. It may verify and load the local
   static map needed by `RenderWorld`, but it must bypass `SetCurrentWorldSector`
   and `EnterSector` authority work: quests, clocks, strategic flags, temp-state
   restore, AI/enemy creation, and tactical preparation remain server-owned.
3. Materialize each committed public snapshot into renderer-facing actor
   proxies. Exact replicated entity identities and assigned-actor ACLs drive
   selection; the client never reconstructs hidden actors from the local map or
   AI data. Renderer-facing visible-door proxies remain pending.
4. Route the supported isolated co-op controls into typed intents before any
   legacy local event or mutation occurs. Migrating ordinary tactical UI actions
   remains future work; unsupported actions stay unavailable until their
   authoritative state, validation, execution, and replication domains exist.
5. Enter `GAME_SCREEN` only after the static presentation world and one matching
   committed baseline are both ready. Any load/compatibility failure returns to
   the worldless diagnostic rather than falling back to local authority.

Phase A of step 2 is live: every loaded authority snapshot carries the exact
path-safe `.dat` basename actually accepted by `LoadWorld`. After the complete
effective-content manifest has matched, the client opens that exact VFS asset
read-only and reads it once into a bounded owning plan. A pure parser validates
the known map version and flags, authoritative dimensions, every static layer
and optional variable-length section, recursive object/LBE bounds, renderer-
representable heights, and exact EOF. The immutable bytes and checked section
spans are retained only for the matching session epoch, world generation, map
key, and dimensions; disconnect, resync, replacement identity, or parse failure
revokes them. Presentation code may borrow the plan for the same main-thread
lifetime as its matching snapshot.

Phase B of step 2 is also live as an isolated production component. It consumes
only those retained bytes and checked spans, provisions exact-dimension map and
room storage, loads the admitted tileset without movement-cost callbacks, and
replays heights, saved map flags, land, object, structure, shadow, roof,
on-roof, and 8/16-bit room records through checked TileEngine insertions.
Authority ambient lighting is applied without touching personal lights, local
light sprites, clocks, or RNG; underground map identity is retained for normal
roof rendering. Only renderer bounds, wireframes, shadow pruning, and redraw
invalidation are initialized. Failure at any stage tears the presentation
geometry and lighting back down without entering a sector or constructing
items, placements, exits, doors, edgepoints, schedules, navigation, AI, or
campaign state. Legacy resize-map translation is rejected because plan grids
already use the authority dimensions. Time-conditioned map light sprites remain
intentionally omitted until their active state is replicated; evaluating them
from the passive client's cold campaign clock would invent presentation state.

The presentation actor materializer is likewise available as an isolated
production component. It stages the incarnation-aware public actor directory
and a complete hidden `TOTAL_SOLDIERS` repository before detaching any published
merc nodes. Each replacement uses the authority's exact slot, incarnation,
grid/world coordinates, body, ordinary direction, logical animation direction,
animation state/surface/frame, visibility, and manifest-covered palette IDs.
JA2 locomotion deliberately advances the logical grid to the next path tile
before its sub-tile world coordinates finish crossing into that tile. Snapshot
validation therefore admits a same-or-adjacent cardinal/diagonal disagreement,
while bounding both representations to the map and rejecting greater
divergence. This keeps a normal mid-walk frame observable without weakening the
fail-closed coordinate envelope.
Only `AddMercToHead(..., FALSE)` nodes are published: no collision structures,
pathing, AI, inventory, personal lights, entity-authority directory, clock, or
RNG state is constructed. Failed preparation or publication restores the prior
bank and repository; teardown removes the presentation nodes and restores the
application repository. The legacy high-resolution facing fields currently use
the replicated ordinary eight-way direction because the wire domain has no
separate high-resolution values; logical-body layering uses the independently
replicated `animationDirection` and never derives it locally.

The transactional presentation-world host/runtime binding is now live. It
composes the static geometry and actor proxy materializers, reapplies authority
ambient lighting before publishing each new replica revision, and grants
`GAME_SCREEN` readiness only after its matching static-world, replica, and
interface gates have all committed. The interface gate installs server-assigned
selection/camera focus plus an adapter-owned passive native TEAM background and
exact-map radar bitmap. Its bounded pin and controlled-roster models are copied
from each committed snapshot and reclassified synchronously if admission
assignments change without a tactical revision; no replica pointer, legacy
roster, or cold campaign squad setting is retained. The roster keeps the full
wire-bounded ownership list in canonical assignment order and shows the
six-entry window containing the selected actor. Each visible slot renders the
authority-published display name, AP, simple current/maximum life and breath,
stance/body placeholder, and the first primary/secondary hand records with
condition and jam state. It does not infer a JA2 squad assignment, portrait,
morale, bleeding, or full inventory. The radar uses one fixed neutral day palette, local
presentation camera bounds, and fixed colors derived only from published actor
membership, hostility, assignment, selection, target, and level. The current
exact firearm target receives a clipped, presentation-owned radar outline;
neither that flag nor any other control mode is written into actor proxies.
Every authority-published visible door is also retained as an exact
`{baseGrid, structureId, open}` radar marker. Closed/open state and the current
door-mode selection use distinct presentation-only styling; no local structure
state is mutated.
Hostile membership is
already the authority's shared co-op team-public view: tactical capture excludes
every non-player actor unless `gbPublicOpplist[gbPlayerNum][slot]` is
`SEEN_CURRENTLY`, and removal from that snapshot removes the pin. Exact legacy
dead/crow/off-visible-world-tile filtering requires a future bounded
`radarRenderable` presentation field rather than client-side guesses.
Resynchronization, world replacement,
or a malformed, missing, stale, or dimension-mismatched asset tears the
presentation transaction down and returns to the diagnostic INIT view rather
than enabling local authority. Renderer-facing visible-door proxies remain a
follow-up.

The first native interaction milestone remains deliberately narrow: the normal
map viewport, actor sprites, assigned selection/camera, authority ambient light,
native TEAM chrome and controlled-actor status, and passive radar render from
committed state. Clicking a visible TEAM-roster entry selects the exact assigned
actor represented by that entry. A terrain click emits exactly one
server-authoritative move intent. While `F` targeting is active, clicking an
authority-published hostile selects that exact target and Enter confirms the
attack. While `D` mode is active, clicking an exact same-tile or cardinally
adjacent published door selects its `{baseGrid, structureId, open}` record and
Enter submits the inverse state. A click on the passive radar pans only the
local static-presentation camera; `Home` recenters on the selected actor and
resumes following. That navigation preserves any active attack, numeric
destination, or door mode and cannot produce a gameplay intent. Radar clicks,
actor/door pins, and the camera outline all use the same captured absolute map
origin and pixel-center inverse, so rectangular geometry does not silently
fall back to a centered-map approximation. Door state is layered below actor
and target pins, while the exact selected-door outline remains above them. A
pure modal dispatcher gives roster,
targeting, numeric destination, and door clicks precedence before it may
produce one unsent movement intent. The native status strip renders the active
move/aim/door mode (including target, aim, and exact door state), otherwise it
renders the matching server receipt status. A request that fails before an
authoritative receipt exists is rendered as a separate red local failure,
stamped to its attempted world generation and suppressed after replacement;
it is never reclassified as a server rejection.
This is an isolated, bounded click translator; it never enters
`HandleTacticalUI`, creates legacy mouse regions, invokes client pathing, or
runs local gameplay simulation. The panel
and radar are separately owned VObjects; this is not `InitializeTEAMPanel`,
`RenderTEAMPanel`, or `RenderRadarScreen`. No broader stock buttons or mouse
regions, animated faces, full inventory, strategic squad roster, native clock/town
state, animated cursor, visibility
calculation, or `ScrollWorld` runs; client AI, pathing, bullets, turns, clocks,
and legacy tactical event handling also remain off. The deterministic stock
six-slot chrome is selected from the initialized native layout: a radar
extending beyond the
nominal panel identifies JA2's shifted 1024-family 1280x720 layout even if the
window manager changes the physical dimensions. It never consults cold local
squad state; larger legal ownership lists page automatically with actor
selection. Static portraits now use the compact authority-selected manifest-bound
portrait identity; profile alone does not select the normal/IMP face and applied
camouflage variant. Unsupported artwork remains neutral; animated face/gear
overlays are not included. Supported key commands still go
through the isolated co-op controller as server intents, not ordinary tactical
UI code. This keeps the server-authoritative boundary that removes lockstep and
owner-simulation desynchronization classes. Broader stock UI, cursor modes and
actions, full panel interaction, and per-peer visibility/fog remain follow-up
work.

## Dedicated launch lifecycle

The full-engine process now has an explicit startup contract:

- `JA2 --dedicated` and `JA2 --dedicated --dedicated-mode=pvp` select the
  transitional PvP host. Only legacy deathmatch and team-deathmatch settings
  are accepted; legacy `GAME_TYPE=2` is rejected.
- `--dedicated-mode=coop` requires a bounded campaign identifier, an explicit
  `--campaign-action=new|resume`, and an absolute `--dedicated-state-dir`.
  Campaign identifiers are lowercase-canonical. Creating a campaign also
  requires an explicit decimal `--campaign-seed` (zero is valid); resume
  forbids an operator seed and discovers the immutable value from the two
  bounded manifests instead. Before SDL/VFS startup the worker acquires the
  campaign lease, prepares its isolated writable profile and fixed empty
  logical save-scratch entries, and installs the manifest-bound simulation RNG.
  After data packages mount, a separate rollback-safe `co-op installed content
  manifest` subsystem captures and caches the effective installed-content
  digest before `legacy content` and `game`; capture failure unwinds the active
  package subsystem. It validates and counts every encountered occurrence,
  omits only exclusive runtime directories and the separately allowlisted exact
  root-level `Strategic Decisions.txt` and `DebugMessage.txt` logger outputs, and applies the
  normalized effective read-only overlay. Case-only spellings across different
  layers are ordinary overlays selected by the smallest layer; a same-layer
  spelling ambiguity, same-layer exact duplicate, or remaining writable shadow
  fails closed. Late campaign open uses the cached digest rather than
  re-enumerating the VFS, then binds the runtime fingerprint and opens the exact
  A/B campaign. Resume materializes the selected checkpoint bytes into the
  already-created scratch entry before the legacy save load begins. After
  campaign entry, the admission listener binds `--dedicated-coop-bind` and
  `--dedicated-coop-port`, which default to the private loopback endpoint
  `127.0.0.1:60005`. An invalid explicit address or port fails closed instead of
  silently widening exposure. LAN operators must explicitly select a reachable
  interface and restrict that interface to a trusted LAN/VPN.
- `--coop-client` requires `--coop-server` and an absolute
  `--coop-state-dir`; `--coop-port` defaults to 60005. It cannot be combined
  with `--dedicated`. Immediately after SDL initialization and before random,
  `GameContext`, or VFS initialization, a one-shot bootstrap connection obtains
  the server descriptor. The client derives and leases campaign-private
  scratch, installs the descriptor seed, and substitutes that private profile
  for the normal writable VFS profile. After exact outer campaign-identity
  validation, every restart atomically quarantines a nonempty disposable client
  VFS profile to a strict private `profile.orphan.<pid>.<seq>` sibling and
  requires a freshly empty replacement before creating its two scratch files.
  Existing quarantine siblings are accepted only with the identity record;
  `Temp`, `ShadeTables`, `ja2_mp.ini`, and prior load output are disposable,
  while reconnect/retired evidence remains in the held parent directory. The same post-package, pre-legacy
  manifest subsystem captures the installed-content digest and compares it to
  the descriptor before any legacy cache writes. Late open consumes that cached
  digest, requires the exact runtime fingerprint, restores the private same-
  epoch reconnect credential, starts the live socket adapter, completes
  checkpoint transfer/load, and only then admits a tactical snapshot to
  presentation. A compatibility-verified epoch rollover erases only a stale
  active bearer and performs a fresh join; an exact `.retired` marker instead
  terminates startup without opening the network, across epoch changes.
- Both co-op modes bypass the ordinary `InitMainMenu` transition during INIT.
  The dedicated host therefore reaches its stage-four campaign-entry request,
  while the passive client reaches its worldless INIT waiting/diagnostic view,
  before a pending main-menu transition can commit. Ordinary and legacy PvP
  shell routing still owns `InitMainMenu`. The passive client bypasses
  `INTRO_SCREEN`, selecting
  INIT state zero directly so its INIT-only frame policy cannot loop before
  `InitializeJA2` opens live transport. Only a dedicated new campaign calls `InitGameOptions()`
  immediately before `InitNewGame(FALSE)`; bypassing the interactive settings
  screen otherwise leaves an invalid zero difficulty and zero squad capacity.
  The canonical defaults select installed difficulty 2 and a six-actor squad;
  resume never overwrites checkpointed options.
- Dedicated hosts suppress automatic and forced interactive Help overlays at
  the Help policy boundary without changing save-owned help bits or ordinary
  GUI preferences. This is required at first tactical entry: the modal Help
  handler otherwise returns before the initial helicopter starts and waits for
  local input that a dedicated process deliberately does not own.
- Dedicated hosts use the ordinary immediate helicopter-skip insertion path.
  They do not make authoritative actor arrival depend on advancing a local
  presentation-only helicopter cinematic; interactive processes retain that
  cinematic and its manual Escape shortcut.
- GUI and headless PvP hosts both enter `MP_CONNECT_SCREEN`. That screen owns
  the pre-game `NetworkAutoStart()` call, starts the listener, self-connects
  the transitional host client, receives canonical settings, and only then
  initializes the game. The map also retains a defensive autostart call, but a
  direct host-to-map route is forbidden because it initializes world state
  before the host has received those canonical settings.
- Invalid PvP settings, an unsupported legacy game type, or listener bind
  failure stops the headless process with a nonzero status instead of leaving
  an idle supervisor-visible process or retrying an unreleased transport.
  Exceptions during any auto-driven setup or later game-loop frame are fatal
  to the dedicated process rather than being swallowed behind a latched UI
  transition.
- `SIGINT` and `SIGTERM` publish only a signal-safe termination flag. The main
  thread stops the frame loop, stops and bounded-drains the listener, reconciles
  the tactical server, ends a world-free session epoch, and only then performs
  the final strategic checkpoint.
  A final equal-priority SGP boundary detaches the tactical runtime-message sink
  before the game boundary destroys `GameContext`; the already-stopped admission
  transport remains ahead of SDL, and VFS shuts down before the campaign lease.
  If an exceptional exit finds that sink inside message dispatch, teardown
  stops transport, severs runtime pointers, and intentionally leaves the live
  composition allocated instead of freeing a callback target. The process
  returns a supervisor-visible status.
  A signal during a tactical world deliberately fails the final checkpoint and
  resumes from the preceding durable strategic generation; no save or network
  work runs in the signal handler.

This preserves the full-engine PvP bootstrap and supplies strategic campaign
create/resume/checkpoint, a wired authoritative tactical server, a composed
passive full-engine client, complete campaign transfer, a diagnostic worldless
typed-intent view, and a renderer-only JA2 static-map/actor `GAME_SCREEN` for
co-op. Legacy tactical panels and input, complete visibility and dynamic-world
replication, and general strategic mission/session control remain outside this
slice.

For example, with matching builds and installed content on a trusted LAN, a
new campaign and one client can be launched as follows. The state roots are
absolute, private to their process, and intentionally not shared:

```bash
/opt/ja2/JA2_ENGLISH --dedicated --dedicated-mode=coop \
  --campaign=lan-demo --campaign-action=new --campaign-seed=20260817 \
  --dedicated-state-dir=/home/ja2/.local/state/ja2/coop-server \
  --dedicated-coop-bind=0.0.0.0 --dedicated-coop-port=60005 \
  --checkpoint-seconds=300

/opt/ja2/JA2_ENGLISH --coop-client --coop-server=192.168.1.50 \
  --coop-port=60005 \
  --coop-state-dir=/home/alice/.local/state/ja2/coop-client
```

To resume, use the same server state root and campaign ID with
`--campaign-action=resume` and omit `--campaign-seed`. The cold-state classes
and mission behavior admitted after load are bounded below.

## Automatic encounter and return policy

This vertical slice recognizes three cold campaign shapes:

- an exact untouched initial strategic state (new, or an empty legacy
  checkpoint resumed cold) with no player mercenaries, no tactical world/
  sector, initial world time, no delayed hire, a valid configured arrival
  sector, and at least one hostile there;
- on `--campaign-action=resume`, the exact durable prepared-initial shape: four
  validated seven-day A.I.M. mercenaries still in transit, with their profile
  equipment already purchased and exactly one matching ordinary delayed-hire
  event apiece, while all other initial-state predicates still hold; or
- on resume, a cold non-initial strategic checkpoint with no loaded world/
  sector and at least one valid live on-foot squad mercenary. It is classified
  as established, byte-preserved at entry, and starts admission/campaign sync
  without starter mutation. Vehicle bodies, drivers, passengers, actors assigned
  to vehicle duty, and other non-squad duties are not direct co-op actors.

Any ambiguous/partial initial roster/event set, loaded tactical state, invalid
starter environment, or prepared/established shape presented as a new campaign
is rejected; this path does not repair or silently reinterpret it. For an
untouched state the server scans the hireable healthy A.I.M. profiles, computes
their campaign-specific seven-day hire/equipment/deposit charge, and selects
the cheapest four by charge then profile ID. It requires the whole roster to
fit team capacity and available funds before the first mutation, then uses the
normal hire path with profile items and records the normal finance and history
entries; it does not install QuickStart/debug equipment. The canonical arrival
minute is installed in both actor and delayed event before the first checkpoint.
A legacy failure after the first hire is process-fatal and is not described as
rollback-safe.

The newly prepared roster and its arrival events are checkpointed before the
listener starts or campaign bytes can reach a client. An exact prepared resume
is observed and left unchanged rather than hiring twice. Once the strategic map
is ready, the server launches immediately if four campaign-ready peers have
arrived, or after a ten-second gather grace if at least one remains ready. It
then uses JA2's ordinary first-arrival/time-compression path and requires all
four actors to become controllable within two minutes. For an established cold
campaign, the first campaign-ready peer triggers selection of the canonical
surface/depth, row, then column hostile sector occupied by an eligible on-foot
squad actor. A peaceful-only established campaign remains connected and
worldless in `StrategicIdle`; the runtime does not trap it in a tactical sector
that the client cannot leave.

After either automatically entered hostile encounter becomes playable, the
runtime arms a process-local post-combat return. The serialized victory flag is
not sufficient by itself: the first trigger additionally requires the armed
world, exact loaded game-screen sector, no enemies or combat, and drained
tactical actions, interrupts, bullets, explosions, dialogue/trigger work,
autoresolve, meanwhile, traversal, auto-bandage, boxing, save/load, and UI/custom
timers. At that first committed evidence the server immediately stops admission,
reconciles campaign/tactical Ready sets to empty, and discards inbound work;
continued ACKs or otherwise valid intents therefore cannot starve the return.
A pure three-way decision rechecks gameplay evidence on subsequent committed
frames. Regressed evidence restores `Playable` and same-epoch admission; stable
evidence waits for the command/receipt/replication fresh-baseline boundary; only
stable evidence plus that boundary invokes the dedicated cold-unload seam. That
seam bypasses only JA2's live-player occupancy guard and
reuses the normal sector-temp, `HandleDefiniteUnloadingOfWorld`, `TrashWorld`,
flag-clear, temporary-schedule retirement, and tactical-world notification path.

Admission remains closed while the tactical command host and server end the
world. Once the ordinary deferred transition reaches a worldless `MAP_SCREEN`,
the server commits a required cold strategic checkpoint, supersedes the campaign
transfer with that immutable generation, and only then reopens admission in the
same session epoch. Dedicated co-op also suppresses the interactive auto-bandage
prompt at victory. These fixed enter/return rules are the current automatic
mission/session control; there is still no replicated strategic map, arbitrary
mission selection, or client sector-transition intent.

## Durable campaign checkpoints

The data-free dedicated campaign store defines the crash boundary before the
legacy save system is connected to it. A campaign has two checkpoint slots,
`A` and `B`, and one fixed, checksummed manifest beside each slot. A checkpoint
always writes the inactive slot, flushes it through an injected backend,
verifies its exact size and SHA-256 digest, and publishes that slot's manifest
last. Only a successful manifest publication advances the in-memory active
generation. Resume validates both manifest/checkpoint pairs and selects the
highest valid generation; it can fall back from a corrupt or incomplete newer
pair, but rejects equal-generation split brain and any valid incompatible
manifest. A checksum-valid unknown envelope/version or oversized future record
blocks resume rather than being mistaken for corruption and overwritten by a
downgrade.

The version-2, 184-byte little-endian manifest binds the full lowercase-canonical
campaign identifier, mode, immutable 64-bit campaign seed, slot, generation,
world time, runtime compatibility fingerprint, separately named
installed-content SHA-256, and checkpoint size/SHA-256. A checksummed version-1
record is rejected rather than silently inventing a seed. New campaigns refuse
any pre-existing manifest bytes, and missing storage is distinct from an I/O
failure. Co-op creation and resume reject an absent installed-content digest.
The checksum protects the manifest from accidental corruption; it is not an
authentication mechanism.

The native filesystem backend now turns an operator-created absolute state
root into one retained process lease and fixed campaign-local paths. The root
must live on a local filesystem beneath trusted, stable ancestors; its leaf may
not be a symlink or reparse point. On POSIX, the backend verifies current-user
ownership and private permissions; managed directories are opened relative to
retained directory descriptors, forced to mode `0700`, and synced when created.
On Windows, the service operator must provision an equivalently private ACL;
the backend rejects reparse points, pins root/campaign directories against
replacement, and retains the profile handle with delete sharing only for its
controlled identity-checked quarantine rename. A stable `process.lock` is never removed, so two workers cannot share
the same writable root. Campaign directory names use a
`campaign-` prefix and lowercase key, avoiding case-only duplicates and Win32
device-name components.

Each campaign also owns a separately managed `profile/` child. Checkpoints,
manifests, and their publication temporaries stay in the parent campaign
directory and are never mounted into the legacy VFS catalogue. The bfVFS
initializer has a writable-profile replacement seam which skips every
configured `WRITE=true` profile before scanning it, retains the configured
read-only Data layers, and installs exactly one supplied writable root. Focused
tests prove that global user-profile files do not leak into the campaign,
campaign root/`Temp`/`SavedGames` writes remain isolated, and later read-only
package overlays stay below the campaign profile. Resume may reuse its existing
profile. An explicit new-campaign retry first proves that both manifests are
strictly missing; if an interrupted pre-manifest startup left profile files, it
atomically moves the complete directory to a unique `profile.orphan.*` sibling
and creates a fresh private profile without traversing, deleting, or
overwriting the evidence. Any present, malformed, or unreadable manifest
refuses recovery. Orphan directories are never mounted or removed
automatically; operators may archive/inspect and delete them only while the
campaign worker is stopped. Dedicated co-op startup installs this seam before
VFS discovery. The generic injected manifest collector remains strict:
normalized writable collision with selected read-only content is
`WritableShadow`. Production bfVFS enumeration first validates and counts every
occurrence, then omits `CVirtualLocation::getIsExclusive()` directory ancestors
such as `ShadeTables`, `Temp`, and save sidecars. A separate exact-path rule
omits only the root-level `Strategic Decisions.txt` and `DebugMessage.txt`
runtime logs; any remaining writable shadow
still fails closed. Native opt-in diagnostic sinks outside
bfVFS still need an explicit campaign-local log policy.

Checkpoint probing is bounded to 256 MiB and hashes the closed file with
streaming SHA-256. Manifest reads buffer at most 184 bytes plus an oversized
sentinel. Manifest publication uses an exclusive same-directory temporary,
file flush, atomic replacement, and POSIX parent-directory sync. The publish
contract distinguishes not-published, durably published, known-visible with
unknown durability, and an indeterminate old-versus-new publication state. The
store commits matching state only when visibility is known; either uncertain
outcome poisons further work, and an indeterminate publication also invalidates
the in-memory state until process restart. The lifecycle must fail-stop.

The legacy save adapter now exposes only fixed A/B scratch names inside the
campaign profile and maps them to the reserved legacy slot identities 6 and 7.
It semantically preflights the closed runtime container, then truncates and
copies it into the exact backend-reserved inactive staging file without
replacing that file's native identity. Dedicated invocations suppress the
InventoryPoolQ sidecar, legacy last-slot/settings writes, and all legacy
autosave, quick-save, and quick-load entry points. Dedicated load
bypasses only the interactive save-array presence gate; a Temp application or
package restore failure is terminal. Cosmetic header/load-screen selection
does not consume simulation RNG during a dedicated save. The interactive
save/load paths retain their legacy filename derivation and save-screen
presence checks.

The adapter is wired through `DedicatedCampaignBoot`: it acquires the lease and
precreates fixed empty scratch entries before VFS discovery, binds the
checkpoint writer once, and installs the campaign-isolated writable profile
(including `Temp/`). New campaigns require an empty or safely quarantinable
pre-manifest profile; resume reuses the same private profile. Immediately after
the data-package boundary, a separate transactional manifest subsystem hashes
and caches the effective read-only namespace before legacy content or game
initialization. That timing keeps new visual caches out of identity and lets a
capture failure roll back the already-active package subsystem. The collector
selects the smallest-layer normalized occurrence, permits case-only spelling
differences across layers, rejects same-layer case ambiguity or duplicate paths,
omits explicit exclusive runtime namespaces, and rejects every other writable
shadow. One residual cross-platform hardening gap remains: bfVFS's
case-insensitive per-layer map may collapse raw same-container `Foo`/`foo`
aliases before manifest enumeration. The manifest still hashes the effective
winner and cross-process differences fail identity mismatch, but it cannot
diagnose an alias already collapsed by bfVFS; installed data does not expose
this shape. Only after that cached identity pass opens the exact store does resume
materialize and verify the selected checkpoint in the precreated scratch entry,
immediately before the dedicated legacy load. Periodic
checkpoints preflight every non-network eligibility hazard while admission is
live, then stop and bounded-drain it before rechecking and saving. Final
shutdown stops and drains admission first. Every checkpoint runs at a committed
main-thread frame and publishes the inactive manifest last.

The deterministic foundation is now installed for dedicated co-op. The engine
core provides a fixed PCG32 simulation stream with a canonical 40-byte
little-endian checkpoint, campaign-seed binding, exact replay, unbiased bounded
draws, and fail-closed sequence exhaustion. A separate data-free eligibility
policy permits only a paused, committed, queue-drained, cold strategic
checkpoint: no tactical world, combat, projectiles, active or queued dialogue
effects, pending dialogue trigger timers, realtime AI, temporary schedules,
reinforcement transients, MiniEvents, or unrestricted Lua randomness. The
first persistent-campaign milestone therefore rolls an interrupted tactical
battle back to the preceding strategic checkpoint; it
does not claim mid-battle resume.

The same boundary now captures runtime boundary state. It transactionally
validates the next frame identity, completed frame count, fixed-tick sequence,
simulated time, remainder, and scheduler configuration. Cold restore establishes
a new process-local monotonic clock anchor. Package RNG records now use PGST v4 to
carry each package's derived root seed and stream limit, so streams created for
the first time after resume remain deterministic; interactive PGST v3 saves
still restore their existing streams under the externally configured seed.
CHKP v2 carries the full restorable frame and simulation-tick boundary, while
CHKP v1 remains an explicitly metadata-only compatibility record. GRNG v1 is a
separate checksummed 68-byte record binding the simulation stream to the
runtime fingerprint, campaign seed, and immutable package-host root. A strict
package RNG callback policy rejects successful `PackageRandomSource::next()`
draws made on the synchronous package save, validation, or load callback
thread, including draws through pre-existing retained or directly
copy/move-derived values and draws followed by a stream rewind. Read-only
checkpoint inspection remains allowed; unrelated random services require
their own guards. The
lifetime-safe provenance and callback-scope probes become inert when the
callback ends. Restore stages the persisted package RNG before both
validation and load, then republishes a pristine persisted copy, so callback
inspection of the callback-bound package RNG is independent of destination
history and interactive callback draws remain rollback-only. Existing
synchronous UI saves still emit interactive CHKP v1 metadata because their
screen handler runs inside an uncommitted application frame; they do not claim
a restorable deterministic boundary.

Every registry-bound package RNG and its directly derived copies now also
share a process-local, non-rewindable consumption epoch. A move-only package
RNG transaction captures that epoch plus exact activation-ordered PGST engine
records, freezes package lifecycle and runtime dispatch, and preallocates
rollback state for every active registry-owned source. Save commits require an
unchanged epoch and state; load commits require the exact preflighted target,
including the original package root seed and stream limit. Failed commits stay
armed, and rollback restores deterministic active-source state without
rewinding consumption evidence. The epoch is intentionally absent from PGST
and GRNG. Out-of-band consumption through an inactive retained derivative is
terminal evidence; the transaction does not claim it can roll back arbitrary
package-owned objects.

The runtime-save coordinator now has separate interactive and dedicated
policies. Every fixed-slot dedicated bridge explicitly selects the strict
policy. Strict saves require a healthy canonical `SimulationRandom`, a
committed CHKP v2 boundary, PGST v4, and GRNG v1, and publish exactly those
three sections. Strict load requires that same exact section set and rejects
older metadata-only or seed-incomplete records before the legacy domain loader
dismantles live state. Package-defined opaque save records also remain closed
until their semantic validation can be staged before that destructive load;
engine-owned package RNG records are structurally preflighted separately.

Move-only runtime execution guards now own the canonical simulation checkpoint,
its non-rewindable consumption epoch, and the package RNG transaction across
the complete legacy serializer or loader. The save guard is armed before the
legacy save mutates pause/UI state, seals CHKP + PGST + GRNG, and commits the
unchanged package transaction only as the final publication check; a failed
final check closes and removes the sealed file while the transaction is still
armed, then rolls both RNG domains back. Strict compatibility overloads without
a caller-owned guard reject before storage, callbacks, or transaction
acquisition. The load guard is armed and preflights all three sections before
`TrashAllSoldiers`, then publishes package RNG state,
the frame/tick boundary, and the simulation stream in that order before its
final exact-target transaction commit. Every later false exit closes the file,
clears the loading flag, and explicitly rolls both RNG domains back. Those RNG
rollbacks do not claim to reconstruct tactical or strategic objects already
dismantled by the legacy loader, so any dedicated load failure after that
boundary is fatal to the attempted session rather than retryable in process.
The same full-load scope owns `ScopedSavedGameFaceReconstruction` for dedicated
loads. Soldier and static-NPC faces are reconstructed from profile presentation
timing without consuming canonical simulation RNG; outside that scope
ordinary face creation retains all three legacy draws. Strategic AI is loaded under
`StrategicAILoadPolicy::DedicatedExactRestore`: current SAI save version 29 is
accepted without compatibility migrations, movement/airspace rebuilding,
priority evolution, group repoll/repair, or repair validation, while
a stale SAI version fails the load. Interactive loads retain their historical repair
policy.
Validation-only loads explicitly roll their guard back and never leave package
dispatch frozen. Interactive save/load guards remain inert and preserve the
legacy CHKP v1 behavior.

The process-global game RNG has a one-shot installation seam: before either
typed or generic accessor observes the default source, a caller may install the
actual `SimulationRandom` object with the immutable campaign seed. Both
accessors then expose that exact object for the rest of the process; late or
repeat installation fails and there is no reset. Dedicated co-op installs it
from the immutable campaign seed before `InitializeRandom()` and constructs
`GameContext` with that same object and package-host seed. Authoritative legacy
RNG bypasses used by the supported path now route through the canonical source,
and gameplay Lua routes `math.random` through it. Dedicated co-op's
`math.randomseed` wrapper validates Lua's stock one/two-argument shape, requires
the installed campaign RNG, and acts as a deterministic compatibility no-op: it
returns the immutable campaign seed's low/high 32-bit pair without advancing or
replacing the stream. This safely admits shipped bootstrap calls such as
`math.randomseed(os.time())`; ordinary and legacy PvP continue to call Lua's
original `math.randomseed`. Tactical reinforcement delays/backlogs, which are not serialized, reset at
the authoritative world load/unload boundary. Interactive and legacy PvP keep
their established random source and save policy.

Writing the inactive A/B slot means the runtime container itself need not gain
a new `ByteStorage` atomic-write API: close, sync, semantic probe, and hash must
all succeed before its manifest is published. Those lifecycle and deterministic
conditions now gate live strategic campaign persistence. Admission is open only
for the declared trusted-LAN threat model. Exact tactical receipt, baseline,
delta, ACK, server-session bookkeeping, main-thread execution, and observer
publication are composed on the dedicated server. The passive client runtime,
real socket adapter, campaign synchronizer, scratch commit/load boundary,
snapshot replica, worldless fallback, and transactional static presentation
runtime are composed in `JA2`; none of those client layers run authoritative
simulation.

## Replication

Join and reconnect begin with a complete, checksummed baseline for one world
generation. A campaign-ready peer arriving after the first cohort waits for a
clean fresh-baseline boundary before assignment; it never inherits an existing
participant's actor merely because that transport disconnected. Ordered deltas
then advance a monotonically increasing revision. Clients acknowledge applied
revisions; the isolated client core treats a gap, checksum mismatch, queue
limit, or generation change as resync-required and freezes input. Tactical
protocol wire v2 adds an authenticated exact 88-byte self-only request with six
canonical reasons: delta-sequence gap, payload-checksum mismatch, state
mismatch, replica rejection, invalid envelope, and baseline rejection. The
client retains the last committed view while input stays frozen. The server
responds on the same socket with a fresh baseline, and only its normal ACK
returns the peer to live replication. The server validates the last committed
checkpoint across rotated replacement-baseline retries. Admission, socket,
identity, actor assignment, the authoritative command cursor, pending commands,
and receipt history remain intact. A bounded per-peer proof ledger records only
baselines and deltas successfully sent to transport. Exact late ACKs may advance
committed recovery evidence without changing the current phase or staged send;
monotonic send ordinals prevent equal-revision regression. A newly accepted
resync request purges prior sent proofs after validating its committed
checkpoint. Exact cursor reconciliation clears only a proven-unconsumed
outstanding command; a consumed command remains input-blocking through baseline
ACK until retained Queued and terminal receipts replay in the same tactical
world. A newer-world baseline adopts its authoritative cursor and retires the
obsolete old-world lock; a late old-world receipt is then idempotent. A
reconnect baseline waits behind a retained pending command, while same-
connection resync remains baseline-eligible with that lock intact. Three failed
replacement-baseline attempts close the connection. Disconnect/reconnect
remains transport recovery. Focused, integration, and real-socket tests validate
the implemented path.

An accepted command reservation is bound to the coordinator's main-thread pump
frame. Its configurable nonzero completion deadline defaults to 3600 frames.
The expiry pump result identifies the exact peer and command and fail-stops the
tactical session, including immediate actor-ACL removal. It deliberately
preserves the pending obligation and command cursor and emits no invented
terminal receipt, because the engine may already have applied the command. The
world-drain loop advances
the same watchdog while normal inbound pumping is suspended, bounding both
teardown and subsequent admission/reconnect recovery without wall-clock policy.

The embedded tactical snapshot wire is version 1. Its 314-byte header, 148-byte
actor record, and 7-byte door record carry exact dimensions, ambient light, and the authority's
exact loaded map asset key, canonical actor
`hostileToPlayerTeam`, interrupt-action eligibility, a bounded 44-byte render
presentation, five bounded 12-byte combat-equipment records, and signed 16-bit
bleeding plus unsigned 16-bit usable equipped hand-kit points and the eight-byte
service descriptor appended after presentation. Bleeding is bounded to 0..127; nonzero kit points require an
occupied non-ammunition primary hand. These are public medical outcomes, not
reserve inventory. The snapshot also carries the
visible-door `{baseGrid, structureId, open}` projection. The records cover
primary hand, secondary hand, helmet, vest, and legs. Each captures only the
first object's item ID, stack count, and condition. For an ammunition-bearing
hand object it also captures loaded-ammunition item/count, signed ammunition
condition (including a negative jam state), and chambered state; those fields
stay canonical zero for ordinary equipment. Decode and replica application are
transactional; older layouts are rejected rather than having missing state
inferred. The header carries canonical `commandsBlocked`, interrupt phase, and
interrupt serial. Snapshot v1's generic ceiling is 617786 bytes. Delta wire v1
publishes ambient-light changes, coalesces all changes to one actor into one
full actor-update event, and carries a same-serial phase transition as one exact
43-byte turn event. It has a generic 10243-event ceiling. `TacticalWorldService` and
`TacticalWorldObserverService` are both
version 3.0, and observer `DoorCapacityReached` is the stable value 12.

The narrower co-op envelope uses inner tactical wire v1 and caps a publication
at 256 actors, 1024 doors, and 2563 events. Its baseline payload/envelope bounds
are 45626/47238 bytes and its category-aware delta payload/envelope bounds are
54109/54181 bytes, all below the public 64 KiB ceiling. Intent wire v1 retains a
72-byte header, 26-byte maximum payload, and 98-byte maximum record. The command
journal is wire v1; pointer-free authoritative door/pass commands use tags 33/34,
`BeginFirstAidCommand` uses tag 35, inventory swap tag 36, and same-owner transfer
tag 37. Medical changes remain gameplay significant for revision admission,
including changes with a sprite-frame update.

Authority capture includes only visible ordinary base-door structures and never
replicates lock/trap internals. Hostility is the authority's exact JA2 predicate,
not a client inference from numeric team. For door execution the server
revalidates visibility, cardinal adjacency, exact actor/object identity and
fingerprints, world/turn, AP/BP, and idle controllable on-foot state; stealth,
lock/trap, busy/tin-can, animation, pending-action, and legacy-network cases are
rejected. The explicit-grid noise value is computed before mutation without RNG.
After graphic/status preflight, the native synchronous helper swaps the partner,
verifies logical state, commits door status and the `LEVELNODE` graphic,
recompiles movement, performs POW checks and flashlight refresh, then returns.
Only success deducts AP/BP, emits exactly one `OurNoise`, and runs sight,
opponent-list, interrupt, and AI updates. A post-swap integrity failure latches
the world; it emits no AP spend or noise, and `DoorCapacityReached` or retained
integrity failure stops the dedicated replication path.

The existing tactical observer is only a starting point. The current guarded
`GAME_SCREEN` renders the exact local static map with authority ambient light
and committed public-visibility-filtered actor proxies; the worldless INIT
diamond and table remain the waiting/diagnostic fallback. This renderer does
not calculate local visibility or run live tactical panels; its `I` inventory
panel is presentation-only. The five-slot public combat-equipment projection
and separate owner-only slot summaries are not complete inventory objects.
A full combat baseline additionally needs bounded remaining equipment-stack objects,
attachments/LBE, other items, general structures, door lock/key/trap and
asynchronous interaction state, bombs, smoke,
projectiles/explosions, statuses, per-peer visibility/opponent knowledge,
interrupts, broader attacks/interactions, and pending asynchronous actions.

Sector entry/exit, tactical-to-strategic transitions, autoresolve, and campaign
load are generation barriers. Player input remains frozen until required peers
acknowledge the new baseline.

### Owner inventory, passive equipping and same-owner transfers

The current co-op protocol uses the isolated `coop.inventory.owner` full-replacement
channel: an 80-byte header, up to 256 dense 16-byte carried-slot summaries,
up to 64 24-byte ground-item records and 128 six-byte nearby location markers,
at most 6,480 bytes. It is not added to
shared public deltas. The native 55-slot
view supplies item ID, stack count, first native status, support classification,
an explicit status kind and a 32-bit whole-stack resource total. Ordinary ammo
reports rounds, medical kits report medical points and tool kits report tool
points; other supported classes report condition without a resource total.
Ammo reads the unsigned native `ubShotsLeft` union member, including quantities
above 32,767. Totals include every ordinary stacked object, not a first-object
estimate or a truncated per-object array. Empty and `UnsupportedComplex` slots
carry unknown semantics and zero resource total; the latter is explicit, not an
empty slot. Attachments, active LBE contents and individual object states are
still not serialized. A total does not imply every resource is immediately usable.

Matching client/server builds are required during coordinated development;
there is no mixed-build compatibility or migration promise. This change does
not change the native server checkpoint format or increment protocol versions.

Each frame carries session/world/baseline, exact owner and actor incarnation,
inventory mode and an actor-private revision. The main-thread ledger checks
canonical empty storage and bounded stack shape, and hashes ordinary copied
object fields so hidden ordinary changes can advance the private revision even
when display summaries and the public snapshot do not change. Unsupported graph
internals are not included in that proof. Revisions/fingerprints are not persistent
item IDs and do not certify unobserved remove/reinsert cycles.

The server stages only current assigned owners, sends after baseline ACK and the
corresponding public state, and limits each peer to two private frames per flush
with a rotating actor cursor. Pending replacements hold command receipts so a
terminal receipt cannot unlock a stale inventory. This logical barrier is
reported even on message-budget exits and must not suppress inbound ACK
processing as though the socket were blocked. The reliable ordered TCP path
uses the existing baseline/recovery barrier; there is no separate private-frame
ACK. Reassignment, resync, disconnect, new baseline and world replacement revoke
cached private state. Shared campaign checkpoint transfer still exposes complete
checkpoint-era native data to admitted clients: this live owner filter is not
full confidentiality or adversarial PvP support.

Intent wire v1 kind 11 contains two `u16` slots and one `u64` expected private
revision (12-byte payload, 84-byte swap record). The server requires the latest
staged **and sent** private revision, re-captures native inventory, then retains
one `SwapInventorySlotsCommand` (journal tag 36). Execution revalidates exact
actor/world/turn/pose, AP, source/destination and both hand fingerprints, slot
capacity, two-handed restrictions and backpack access. Ordinary whole stacks in
helmet/vest/leggings/face slots 0..4, hands 5..6 and pockets 14..54 are supported.
Native armor type, loaded armor definitions, face-pair compatibility and Nails'
leather-jacket restriction are checked before mutation. Face changes fingerprint
the opposite face slot and use one checked native personal-light refresh after
the final pair is committed. Daylight/disabled light is a valid no-op; failed
required lighting is an integrity failure. Opaque hand objects still block swaps
that require ordinary hand proofs. LBE equipment locations, attached items,
active containers, splitting, merging and automatic stowing are
not silently approximated. Equipment side effects use a shared private native
core in left-to-right order, never recursively dispatch system commands. Once
copy-swap begins, a thrown or failed continuation poisons world integrity and
cannot be retried or published as Applied; this is containment, not rollback.

In the native game view, `I` opens the selected owned merc's inventory using
stock old/new inventory geometry and verified content item names/art. The detail
panel distinguishes first-object condition, ammo rounds, medical points and tool
points, and shows exact stack totals for resource kinds. Opaque objects retain
a generic reported status instead of invented semantic labels. Select an
ordinary supported equipment/pocket source, then a supported destination; select the source
again to cancel, and `Esc` or `I` to close. The panel only retains local choices
and copies summaries; it never opens the live native inventory cursor, invokes
stock item callbacks or mutates client objects. Input remains locked through
the terminal receipt. Inspection is available while gameplay is gated, but no
swap can bypass turns, interrupts or another pending command. Actor/world/session/
baseline replacement closes the panel and private revision changes clear its
source selection.

Same-owner transfer kind 12 carries an exact six-byte recipient identity, two
`u16` slots and independent `u64` source/recipient private revisions: 26 payload
bytes and a 98-byte intent. The existing wire versions stay unchanged during
coordinated development. Both actors must belong to the submitting peer, and
both current private revisions must have been sent to that same peer. One retained
`TransferInventoryItemCommand` (journal tag 37) rechecks both exact actors,
world/turn, pose, ordinary objects, native distance/LOS and capacity. This follows
the stock inventory-panel rule: conscious human mercs within three tiles, at
least 3 AP each in combat, then 2 AP charged to each after successful placement.
The entire ordinary stack goes to an explicitly empty supported hand/pocket;
cross-owner giving, occupied-destination exchange, merges and splits are excluded.
Hand-involved transfers retain checked equipment/palette continuations; pure
pocket transfers do not inspect or alter unrelated hands. Post-mutation failure
invalidates world integrity instead of permitting replay of a partial transfer.

In the passive inventory panel, select an ordinary item and press `G`, cycle a
nearby owned recipient with `Tab`/`[`/`]`, then click an empty supported slot.
`Esc`/`G` cancels; changed private revisions, ownership, baseline, world or
eligibility clear the selection. This client path is unavailable during interrupts.
Assignment changes defer until all retained native commands terminate, preserving
the ownership acquired at admission; unchanged assignments remain idempotent.
Both inventory replacements cross the reliable owner-only channel before its
terminal receipt can unlock input. This is a bounded development slice, not
cross-player consent or general trading; see [COOP_COMPLETION.md](COOP_COMPLETION.md)
for the current implementation scope, remaining work and verification evidence.

Ground pickup kind 13 carries the exact eight-byte `TacticalWorldItemId`, a
`u16` destination pocket and a `u64` expected private revision (18 payload bytes).
The owner header's seven formerly reserved bytes now carry ground grid, floor,
count and truncation flag. Each ground record combines an exact item incarnation
with the same typed summary; records are unique, sorted and densely indexed.
Absent ground context is canonical -1/-1 with no entries; a valid empty tile
is distinct. The ledger includes tile/floor, selected object fingerprints and
the bounded list in its revision, so moving or observing a changed item revokes
an earlier selection. The projection does not discover hidden items, adopt or
repair IDs, or require AP/idle readiness merely to inspect an item.

The native pool must be visible and neutral: any separate OWNERSHIP marker
excludes the whole pool. Only ordinary whole stacks at render height zero on
the merc's exact tile/floor are offered. The server requires the exact ID in
the current successfully sent owner revision, re-captures that revision, and
retains `AuthoritativePickupWorldItemCommand` (journal tag 38). Execution repeats
world/turn/actor/pose/pool/object/slot/capacity/AP proofs, exchanges native storage
with a canonical-empty pocket 14–54, then uses `RemoveItemFromPool` to remove the
graphic, linked pool entry and stable identity. Native ground-pickup AP is
deducted only after successful removal (no AP charge in real time). Failure
after mutation invalidates world integrity, cannot publish Applied and cannot
retry. This bypasses no theft, trap or cursor decision: unsupported cases are
rejected instead of invoking the asynchronous legacy pickup UI.

`I` then `P` opens the ground browse mode; `Tab`/`[`/`]` cycles exact listed
items, and clicking an empty pocket submits once. `Esc`/`P` cancels. The UI
shows item names, typed totals, stack count, list position and a 64-entry cap
warning. Busy mercs may inspect but not submit; position, revision, scope or
ownership changes invalidate the selection. This does not implement automatic
walking-and-pickup chains, hidden-item discovery, ownership/theft,
disarming, complex objects, merges, splits or full inventory handling.
The installed driver exposes read-only `STATE.inventories[].ground` and a controller-only
`PICKUP actorSlot incarnation itemSlot itemIncarnation destination revision`
operation. Its presence alone is not installed pickup or Screen-input evidence.

Ordinary pocket drop kind 14 carries only a `u16` source pocket and a `u64`
expected owner-private revision (10 payload bytes). The existing wire version
and maximum 26-byte payload remain unchanged; coordinated matching builds are
required. The source must be an ordinary whole stack in pockets 14–54 and its
current private revision must already have been sent to that owner. Admission
retains `AuthoritativeDropInventoryItemCommand` (journal tag 39, unchanged
journal version) with exact actor/world/turn/pose/object/AP proofs, not a client
destination or world-item identity.

Execution requires an idle conscious human on the exact dry `FIRST_LEVEL` tile,
without existing item pools/live world items, mine markers, roof/overhead nodes
or unsupported structures. Hands/equipment, relocation, water/roof drops,
merging, splitting, complex objects and legacy ownership/trap conversions are
not implemented. The native primitive validates ordinary stack capacity and
safe item graphics, then allocates detached world-item growth, graphic/pool
nodes and directory capacity. It rechecks source, placement, storage and
identity before publication. The ensuing allocation-free section activates
one fresh identity, preserves exact scalar/list object storage while growing
the container, empties the source and links the visible ground pool/graphic.
Unrelated complex world objects are moved without invoking normalizing native
object assignment. Retired-slot object storage is detached, never placed into
the newly empty source pocket.

Only after exact new pool/object and canonical-empty source checks does the
retained command deduct the native basic pickup cost once (zero AP charge in
real time). The empty-tile `GetAPsToPickupItem` result is not a valid drop cost.
Pre-publication rejection or allocation failure does not move items, consume
an identity or charge AP; post-publication integrity failure poisons the world
and cannot publish Applied or permit replay. No legacy asynchronous drop,
generic pool insertion, item cursor, recursive dispatch or speculative client
object mutation participates. Updated private carried/ground state crosses the
existing owner-only replacement barrier before the terminal receipt.

The existing passive inventory panel selects a pocket source, then requires
`X` → key release → `X` to confirm the whole stack; `Esc` cancels and `I` closes.
Confirmation blocks other slot/world actions and retains the exact private
revision, owner, assignments, session/world/baseline and actor pose. Changed
context or readiness cancels it. The UI labels the server's placement decision,
not a client guarantee that a tile is safe. Matching builds and bounded native/
client tests pass, and the matching installed controller drop/pickup run passed;
see [COOP_COMPLETION.md](COOP_COMPLETION.md) for evidence limits.

Nearby loot uses the same owner-private replacement, but carries only grid,
floor and a medical-supplies flag, never item identities or pickup authority.
The native projection scans a clipped same-floor Chebyshev radius of 12 tiles,
keeps the nearest 128 eligible tile locations (squared-distance then grid tie
break), and emits them in strict grid order. A further eligible tile sets the
truncation flag. Each item must already be individually discovered (`VISIBLE`),
ordinary, neutral and supported by the native read-only proof. Aggregate pool
visibility and debug show-all state do not reveal hidden entries; any ownership
marker suppresses its entire pool. Discovery is shared persistent player state,
not a new current-LOS test. Positive whole-stack medical points set the cross;
this does not certify that the merc can immediately begin treatment. Marker
changes advance the private revision, but only exact ground records authorize
pickup. Both endpoints check radius, floor, bounds and canonical wire fields.

The passive game view projects these copied markers using verified static tile
heights and roof height. Clicks retain the exact owner, actor, assignment,
session, world, baseline, private revision, origin and camera context. Existing
inventory/targeting controls and other actors retain priority. An eligible
marker click sends the normal move intent, or opens the existing `I`/`P` browse
mode on the selected merc's current tile; there is no automatic pickup command.
The pre-world render hook forces a full static redraw while an overlay is
visible and for one clearing frame after it disappears. Hover and rendering do
not mutate native inventory, reveal items or run client tactical simulation.
The installed driver's read-only `LOOT_UI` observation correlates copied render
models with `STATE.inventories[].nearby_loot`; an empty coherent list is not
positive marker-paint, mouse-input or pickup evidence.

The installed smoke's combined `--verify-inventory --verify-fire --verify-reload`
path checks a narrower extra invariant: exposed ordinary reserve-round decrease
must equal the primary weapon's loaded-round increase. It requires current exact
owner/baseline state, a newer private revision before terminal unlock, unchanged
opaque/non-ammo reserve summaries and stable two-client public agreement. It
retains `reload-reserve-evidence.json`; standalone reload testing does not claim
this private balance proof. Aggregate rounds do not certify per-ammunition-type
identity, hidden LBE or full object conservation.


The integrated protocol-18 executable extends campaign status to 112 bytes for
native battle notices: ID at bytes 96–103, distinct outcome at 104 and sector
at 105–107 and a canonical surface town-control-loss flag at 108, with
94–95 and 109–111 reserved zero. Adding a committed sector loss retires the
earlier notice ID. The native unload entry refuses both unacknowledged and
acknowledged holds until explicit completion. A notice exclusively holds a
paused tactical campaign. The ledger and passive client reject changed outcome
or location under one identity and resurrection of a retired identity. Action 9
acknowledges that exact notice for either ready player through the shared
control revision. The client requires Continue followed by a released Enter;
reconnect, stale observations and an outstanding request cancel the choice.

Native acknowledgement retains the world. At a subsequent committed boundary,
the runtime waits for retained campaign receipts to be queued and tactical work
to drain, closes ingress, revalidates the native context and invokes ordinary
world unloading. Unload refusal or partial failure stops the runtime without
replaying the continuation. Successful unloading must still pass the strategic
checkpoint and cold synchronization barriers. Installed defeat/capture return
qualification remains outstanding; this does not certify victory or hire arrival.

## Campaign policy

Legacy network mode currently suppresses strategic updates and time
compression. Authoritative co-op is moving those blanket skips to an explicit
authority/replica policy:

- only the server executes strategic events and campaign mutation;
- the current client synchronizes and cold-loads the checkpoint, presents
  committed tactical records and live campaign/group/arrival observations, and
  submits supported actions from the worldless co-op Screen; it does not run a
  local strategic simulation;
- the user selected one designated leader for campaign time advancement and
  speed; leader-only pause/speed requests and pause-on-leader-loss are
  implemented, while explicit designation/handover remain unfinished;
- the user confirmed (2026-09-22) that any campaign-ready player may travel,
  choose supported battle actions, hire mercenaries and spend shared funds.
  Shared travel and supported arrival requests are implemented. Hiring,
  contracts, spending transactions and remaining actor-ownership workflows
  still need authoritative implementation and client controls;
- dialogue, general laptop transactions, multi-leg travel, manual deployment
  and autoresolve remain outside the supported campaign-action interface.

Only the server writes campaign saves. Initial load occurs at server startup.
The startup campaign sync sends the selected cold checkpoint as a separate
bounded, hash-verified transfer before tactical readiness; it is not used as a
frame-by-frame replica protocol. General live-load/session-epoch control is not
implemented.

## Security and resource boundaries

The transport treats every peer and frame as untrusted even on a trusted LAN:

- validate frame type and exact control-body length before buffering a body;
- perform checked subtraction rather than overflow-prone offset addition;
- cap frame, file, set, active-transfer, and aggregate buffered sizes;
- bind file-transfer receive sets to their allowed sender;
- reject overlaps, holes, duplicates, metadata changes, and replayed set IDs;
- bound file-transfer pending writes before every frame (32 MiB by default),
  stop at the first query/limit/send failure, and make non-retry callers close
  that recipient so a partial reliable set cannot be mistaken for completion;
- derive roster/team/actor identity from the admitted transport binding;
- apply per-peer queue and work budgets without silently dropping reliable
  state; disconnect or backpressure instead;
- cap each co-op connection's SDL_net pending-write bytes (256 KiB by default)
  before enqueueing another baseline, delta, or receipt; query failure or a
  crossed high-water immediately unbinds and closes that transport;
- reject an invalid explicit bind address rather than falling back to every
  interface.

The full-engine listener defaults to `127.0.0.1:60005`. Selecting any bind that
is not a canonical numeric loopback address emits an exposure warning; values
such as `--dedicated-coop-bind=0.0.0.0`, `::`, or `*` admit on every matching
interface. `--dedicated-coop-port` changes the listener port; the client's
`--coop-port` must match. Plaintext bearer tokens
are transport-bound continuity credentials, not user authentication. The standalone coordinator's
plaintext protocol and admin password are likewise suitable only for
experimental trusted-LAN use. No release should claim secure public hosting or
historical-client interoperability.

The generic SdlNet peer retains a 256 KiB/s sustained inbound rate and a 1 MiB
burst. `SdlNetInboundMessageBudget` may be selected only before `Start()` and is
hard-capped at 32 MiB/s and 4 MiB. Only `FullEngineCoopClientTransport` with a
non-null campaign sink selects that maximum campaign profile; bootstrap,
core-only, legacy, and other generic peers keep the strict defaults. Its static
bound covers one canonical campaign window at 144 FPS. The production socket
E2E sends an exact 11,796,517-byte checkpoint twice, 193 chunks per transfer,
with 7 ms pump pacing.

One legal authority flush can contain 64 in-flight deltas, 64 retained
receipts, and 64 transient receipts, while multiple flushes may occur before an
independently scheduled client polls. No finite callback-FIFO enlargement can
therefore establish the receive bound. `FullEngineCoopClientTransport` instead
configures an immutable, single-connection named-frame parser batch of 16
before `Start()`. Excess complete frames remain in the SDL transport's bounded
connection input and resume in wire order on later polls. The adapter drains
each batch into the core only after that individual parser/callback stack has
unwound and before a following BYE, malformed control frame, EOF, or loss event
is surfaced. The configurable application FIFO limit remains independent, so a
value below the physical 16-entry bound still fails closed on saturation.

## Validation boundary

Focused data-free tests cover the codecs, admission/campaign/tactical state
machines, deterministic persistence boundaries, starter-roster selection,
client controller/renderer, and passive game-loop exclusions. The production
SDL3_net adapters also have a real loopback-socket end-to-end test: it completes
campaign sync, destroys the first client composition, restores its durable
credential into a new composition without issuing a second identity, reconnects
to a distinct exact checkpoint transfer (while equivocation is rejected),
publishes and ACKs world/assignment/baseline state, and sends
typed Move, exact-target aimed-fire, and selected-actor reload requests, observes
the authoritative movement/AP/damage deltas, applies them to the snapshot
replica before their terminal Applied receipts, and releases the client's one-
command lock exactly once per command. The same socket composition then retires
its authenticated identity, proves that no terminal result is publishable
before the tombstone, durably invalidates the client bearer before socket stop,
and removes the peer from tactical state before world/epoch close.

The repository also contains a separate installed-data, independently launched
process certification. It is absent from the default CTest graph and is
registered only when the absolute CMake cache paths
`JA2_COOP_INSTALLED_SMOKE_EXECUTABLE` and
`JA2_COOP_INSTALLED_SMOKE_DATA_ROOT` are both set. The current harness is
POSIX-only and serial. It uses a free loopback port plus private temporary
server, client, and launch roots; creates a campaign; waits for the stable Ready
marker; captures the private 224-byte reconnect credential; restarts an
independent client on the same state root; and requires another Ready with the
credential byte-identical. It then cleanly SIGTERMs the still-worldless server,
requires its final checkpoint and zero exit, then resumes that checkpoint on a
fresh port and starts two independent clients concurrently. A separate bounded
tactical deadline covers both installed-data client cold loads and the
starter-actor arrival window. The harness requires the server's playable-
tactical-world marker and, from both clients, the exact-map-verified marker, the
one-shot committed baseline with a non-empty assignment marker, and the
tactical-presentation-ready marker emitted after the transactional renderer
reaches `GAME_SCREEN` readiness. Because an active world
may outlive a client process, client B is then stopped and restarted against
its same private state. Its credential must remain byte-identical, and its
fresh assigned baseline and presentation must reach the same world generation
and exact map while client A and the authority remain alive. Because an active
world has no cold checkpoint, the authority's final SIGTERM must reject checkpointing and return
the documented failure status; the default harness claims no tactical persistence or
gameplay action. The two clients remain running during that shutdown and must
each log one automatic-reconnection transition, survive multiple retry
intervals, and never publish a false restoration or new campaign Ready while
the authority is absent. A mutation-relevant installed-data
metadata fingerprint must match before and after. Process groups and temporary state are
cleaned deterministically, and installed data is mounted only through the
private run roots. This certifies that bounded lifecycle, not a complete
tactical playthrough or long-running soak.

An optional `--gameplay-client-executable` argument selects the separately built
`full_engine_coop_installed_client` for the tactical client processes. Its
entry-point object alone redirects `GameLoop` to a test wrapper that calls the
real loop before servicing bounded local stdin commands. All production JA2
libraries remain unchanged; shipped executables expose neither this driver nor
an automation option. The wrapper borrows only the public committed client
presentation view and routes movement through the existing roster/terrain
interaction dispatcher and `sendIntent`. Its JSON replies contain replica
coordinates, AP, assignment, and receipt evidence; no credential is exported.

The gameplay harness alternates A then B, selecting unoccupied adjacent
destinations from replicated state and allowing bounded retries for explicit
gameplay rejection or a revision/acknowledgement race. A consumed
`NotBaselineReady` rejection can mean a newer delta is still in flight even
when the client has a valid presentation baseline; it is retried with a fresh
command ID, never by replaying an Applied command. Eight fresh-view attempts
per candidate bound this liveness check. Snapshot animation frames still share
the replication revision, but the server now recognizes a consecutive
frame-only command-equivalence window. Genuine gameplay-state races still
report "state catching up; retry"; the harness does not hide persistent
rejection by retrying without a bound.
It waits past the terminal Applied
receipt for actual coordinate displacement and the exact target tile center,
requires agreement between both clients, and checks the moved state again
after B's process restart. AP must agree and remain nonnegative; a decrease is
required only in turn-based combat, matching native `DeductPoints` semantics.
Real-time movement has no AP charge and may refresh AP between the move and
reconnect, so the restarted client is also compared with the live observer.
If both clients have advanced beyond the recorded movement turn, native combat
may also have changed health, stance or facing while the process reloaded.
Reconnect then requires the same actor identities and exact completed positions,
matching current tactical phases, and agreement on the current gameplay fields
between both replicas, rather than freezing their pre-combat values. Missing
turn evidence or a missing live observer does not authorize this exception.
The result explicitly distinguishes real-time movement from combat AP coverage.
It does not equate request acceptance with completed
locomotion or treat an untested firefight as certified. Build and run from the
source directory, for example on macOS:

```sh
cmake --build build --target JA2_ENGLISH full_engine_coop_installed_client -j6
python3 tests/full_engine_coop_installed_process_smoke.py \
  --executable "$PWD/build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH" \
  --installed-data-root /absolute/path/to/installed/JA2 \
  --gameplay-client-executable "$PWD/build/full_engine_coop_installed_client" \
  --keep-artifacts
```

On Linux, the regular executable is normally `build/JA2_ENGLISH` instead of
the macOS bundle path. Both modes keep all writable state in private temporary
directories and verify installed-content metadata afterward.

Build both targets explicitly before each certification run. The test driver is
`EXCLUDE_FROM_ALL`: a successful default build can leave its executable linked
against older protocol code even when the shipping app is current. The resulting
bootstrap rejection is not evidence of a tactical gameplay failure.

Add `--verify-inventory` (requires `--gameplay-client-executable`) to exercise
ordinary item equipping, exact summary/primary-hand restoration, a pocket-only
swap and restoration through the shipping controller and server intent path.
`--inventory-timeout-seconds` defaults to 90 and is capped at 300. The probe
requires actual carried items, both public replicas, current owner-only caches,
new private revisions and Applied receipts. A pocket-only update need not
advance the public revision. Unsupported/gameplay-rejected actions fail the
certificate; only verified side-effect-free stale-context rejections can use
bounded fresh-command retries. No fixture items or local native inventory
mutations are injected. The probe also opens the actual Screen-owned inventory
with queued `I` input and repeats the hand equip/restore through ordinary source
and destination mouse clicks. At most four normal `Tab` inputs select the exact
owned actor; there is no Screen-controller setter. The two destination-click
attempts have separate exact scope/source guards and are never replayed, even
after a stale rejection. Queue acceptance alone is not submission or Applied:
the probe observes the next actual command identity, its terminal receipt and
both replicas' primary-hand/private-summary convergence. It then inspects real
ordinary and inspect-only items, captures a native PNG in the private client
directory, and closes with `Esc`. That inspection/capture/close postfix must not
submit further gameplay or change owner summaries. Read-only inputs remain
bounded to eight, separately from the two destination clicks and four actor keys.
Before selecting any inventory candidate, an interrupt-only readiness phase
passes real grants through their exact eligible owners and serials, waits through
native AI/deferred phases and resamples current state. It does not submit an
ordinary EndTurn, inject a kit or bypass native action eligibility. Its pass
evidence is separate from inventory actions and shares the same overall deadline.
Successful inventory certification writes `inventory-evidence.json` beside the
private run directories, including the readiness passes, controller actions,
click-driven actions and screenshot path. Existing evidence files are never
overwritten; use `--keep-artifacts` to retain them after the run.
This does not prove full item graphs, checkpoint secrecy or a person operating
the visual panel. It runs before the optional firearm tests.

Add `--verify-fire` (requires `--gameplay-client-executable`) to extend the
installed run with one authoritative firearm action after the movement and
live-reconnect assertions. `--fire-timeout-seconds` defaults to 90 and is bounded
to 300 seconds. The driver exposes only public snapshot hostility, interrupt
and primary-hand state, and its bounded `FIRE` request selects the exact actor
and target through the existing controller with zero additional aim. The
normal runtime sends the resulting intent; the server still owns visibility,
weapon mode, ammunition, action-point and interrupt validation.

The firing probe requires two disjoint-owner clients in one world, coherent
pre-command state, a terminal Applied receipt and released command lock, then
newer snapshots agreeing on a positive loaded-ammunition decrease and current
shooter hand/AP state, with a positive ammunition condition and no jam. Native
jams can consume a round without discharging it. The test does not infer
success from a chamber change or from
target damage, and never resends an Applied action. Explicit revision/baseline
rejections have bounded fresh-state retries; gameplay rejections may advance
candidate pairs only after checking for forbidden ammunition/AP side effects.
Native weapon-usability checks can themselves cause wear, so a changed hand on
rejection is reported as an inconclusive firing attempt rather than hidden by
another request. External-feed and attached-weapon shots without a primary
magazine decrease also remain unproven by this narrow observation.
Combat AP cost is certified only when the observed turn-based combat phase
remains the same throughout observation; a temporary turn/interrupt change
cannot restore that coverage merely by returning to the original phase.
Missing eligible targets or missing ammunition evidence fails
the bounded check rather than being reported as a pass. A complete turn cycle
requires the separate opt-in check below; final projectile/damage resolution
remains separate coverage.
After ammunition certification, a separate bounded observation requires both
replicas to clear the native command block and agree on current shooter/target
state and turn/interrupt phase for a stable interval. It submits no action and
never repeats the shot. Later authoritative damage, death, turn changes, or a
target disappearing from both public views do not freeze the test to historical
state. This is command-gate recovery, not a complete combat-drain certificate:
pending actions contribute to the public blocking flag only during turn-based
combat, and raw bullet/explosion counts are not replicated.

A prior installed firing run agreed on one consumed round and an AP change
from 91 to 70, but its deliberate server stop logged an attack-busy warning.
World teardown resets the pending-action counter before deleting animations
that can release a bullet; the warning alone does not establish that the bullet
was still in flight. The existing whole-process `ShuttingDown` lifecycle now
prevents `InternalReduceAttackBusyCount` and `FreeUpAttacker` from running
further combat completion or reserving synthetic combat work during resource
deletion. Normal combat, sector unloading, and runtime counter-underflow
diagnostics are unchanged. An isolated native lifecycle regression checks both
zero and nonzero counters, real bullet-resource cleanup, and preservation of
actor/battle-completion state.
The follow-up installed run passed movement from both clients, live reconnect,
one consumed round with same-phase combat AP cost, and replicated command-block
recovery. The native log reached zero pending actions before the deliberate
server stop, which emitted no attack-busy warning.
The active-world checkpoint remains explicitly ineligible; this test does not
claim tactical persistence or final projectile/damage resolution.

Add `--verify-reload` alongside `--verify-fire` for native primary-magazine
replenishment after that shot and the command-block recovery wait.
`--reload-timeout-seconds` defaults to 30 and is bounded to 300. The separate
test driver selects the exact shooter's identity through the existing reload
controller; the server's native `AutoReload` chooses reserve ammunition. This
does not change the shipping client or expose an inventory mutation hook.

The reload certificate requires a fresh Applied command, newer snapshots from
both clients, a released command lock and cleared blocking gate, matching
current actor/primary state, and a stable positive loaded-round increase.
Weapon identity must be preserved, but ammunition type, condition and chamber
state may change. Combat AP cost is certified only while the same observed
combat phase persists; otherwise only current AP agreement is checked. No
Applied action is retried. Only explicit revision/baseline rejections with no
observed primary equipment/ammunition change or AP spend get bounded fresh-ID
retries; controller and gameplay refusals fail the check.

Starter equipment does not guarantee compatible reserves. Native reload can
also chamber an existing round or reload only the secondary hand. Those may
be valid gameplay outcomes, but cannot certify primary-magazine replenishment;
the test reports failure to certify instead of modifying inventory or silently
skipping. Reserve slots have owner-only summaries, not complete per-object
ammunition state, so reserve-ammunition
conservation remains outside this check.

Native dual-hand reload now preserves success if the primary reload completes
but secondary old-magazine placement subsequently fails. The secondary is
still attempted; its failure must not turn already-applied primary ammo/AP
changes into a rejected command. A data-free native regression exercises
primary-only, secondary-only, both-hand and neither-hand success with exact
ammunition/AP assertions and a full-inventory secondary placement failure.

The first installed reload-enabled run passed movement and live reconnect but
stopped before submitting any shot: an interrupt stayed in `Resolving` with
all actors ineligible. An immediate sight interrupt can supersede a pending
improved-system movement check; after the native team handoff, the old moving
actor can no longer resolve that check. The validated native interrupt-start
boundary now consumes the obsolete marker before callbacks or the team switch.
An isolated native regression checks the real grant, invalid queue starts, and
a fresh pending action after the grant. Projection precedence remains unchanged,
so genuinely new pending interrupt work still blocks actions.

The following installed run certified fire and reload on both clients: the
same primary magazine went from 8 to 7 to 8 rounds, with AP 91 to 70 to 54 in
the same combat phase. Movement and live reconnect passed too. The overall
lifecycle run still failed after the deliberate server stop: client A reported
a tactical-client wire-send failure instead of remaining in automatic reconnect,
while client B entered reconnect normally.

The tactical transport now recognizes socket loss during a validated outbound
write, including an ACK sent after a replica commit but before the queued
disconnect event is polled. At deferred teardown, only the exact combination
of socket loss and the core's `Failed`/`WireFailure` state is converted to a
normal disconnect. Credentials survive; accepted authority, actor assignments,
and outstanding gameplay commands are cleared. Re-admission and a fresh server
baseline supply the next command sequence; ambiguous gameplay is never replayed.
Malformed input, write-capacity limits, credential/storage errors, and local
send exceptions do not use this recovery exception. Campaign-checkpoint wire
failure handling is separate from this tactical recovery path.

The deterministic transport regression links the real client/core to a fake
socket boundary. It covers initial/replacement baseline ACKs, delta ACKs, and
direct intent sends outside the poll loop through both socket-failure paths;
it checks deferred destruction, discarded dependent frames, exact credential
re-admission, no automatic intent replay, and fresh server command authority.
Three hard-failure controls retain their terminal state. The installed gameplay
shutdown check additionally samples both clients' public presentation throughout
the reconnect window and rejects stale worlds, actors, assignments or receipts;
that public-view check is not evidence about private internal command queues.

The corrected reload-enabled installed lifecycle passed end to end on
2026-09-05: campaign creation, exact saved-credential reconnect, cold checkpoint
resume, both-client movement, live tactical client restart, firing, reloading,
and automatic reconnection after the authority stopped. Both clients retired
their public tactical state without a fatal wire-send error. The same primary
magazine went from 6 to 5 to 6 rounds on both replicas; this run certified AP
agreement for the shot and combat AP cost for the reload. All 171 registered
automated tests passed, including the 11 deterministic send-failure scenarios.
Artifacts: `ja2-coop-installed-smoke-ixjykbdt` in the local temporary directory.
The active-world server stop still correctly reports checkpoint ineligibility
with exit 2; this is connection-loss recovery evidence, not live-battle
persistence or a complete AI-turn-cycle certificate.

### Installed turn-cycle evidence

`--verify-turn-cycle` requires `--verify-fire` and the gameplay driver; reload
remains a separate option. `--turn-cycle-timeout-seconds` defaults to 120 and
must be finite, positive, and at most 300 seconds.

The full installed lifecycle passed with `--verify-turn-cycle` on 2026-09-05,
after both-client movement, live reconnect, fire, and reload. One ordinary
shared-team EndTurn was certified Applied; one explicit serial-bound interrupt
pass completed, and both replicas converged on ordinary player combat turn 7
from turn 4 with fresh revisions and clear command gates. The certificate does
not require sampling an intermediate AI snapshot, increased AP, unchanged
health, or a particular enemy attack. It waits through resolving interrupts,
passes only currently eligible owned actors, and captures each exact Applied
receipt before another command. Ambiguous outcomes and Applied commands are
never replayed; only explicit side-effect-free stale-state rejections permit
bounded fresh-command-ID retries.

Artifacts:
`/var/folders/9y/v1xx2nbd5jg8pt7ntqtndr0w0000gn/T/ja2-coop-installed-smoke-s912um1h`.

The matching global-v9/snapshot-v10 medical-state build subsequently passed the
full lifecycle in `ja2-coop-installed-smoke-uh2nikua` on 2026-09-05. Both clients
completed movement and client restart; native fire and reload both certified
combat AP cost. One shared EndTurn advanced ordinary player turn 4 to 7, with
four explicit interrupt passes. The turn probe now also requires well-formed,
equal bleeding and held-medical-kit points on both replicas. Server shutdown
again left no stale public tactical state. This is medical **wire integration**
evidence, not an installed treatment certificate: the probe did not equip a kit
or submit first aid. The earlier `1adhovwg` attempt failed bootstrap because the
excluded-from-default-build driver had not been relinked; it is not a passing run.
This run used the pre-medical binaries and is distinct from the earlier
reload-only `ixjykbdt` evidence. It certifies a native player/AI/player turn cycle
and post-server-loss cleanup, not first aid, a complete firefight, active-battle
persistence, or a soak. The subsequent medical wire/intent implementation has
data-free codec, policy, and native regression coverage; installed first-aid
certification remains outstanding.

## Delivery milestones

1. Harden framing and file transfer; add malformed raw-wire tests.
2. Add the versioned admission registry and exact codec tests while leaving
   legacy arena bytes unchanged.
3. Add one explicit `JA2 --dedicated` PvP/co-op lifecycle plus crash-safe
   campaign create/resume, locking, periodic checkpoint, graceful shutdown,
   trusted-LAN admission, and a bounded authoritative tactical server. This
   server-side foundation and the passive campaign-sync client are wired.
4. Admit players to one tactical sector, bind mercenaries to them, and
   authoritatively execute movement, facing, stance, stop, end turn, one
   exact-target aimed single-shot firearm attack, and selected-actor reload. The
   current bounded five-slot combat-equipment baseline/delta makes the first
   object in the primary hand, secondary hand, helmet, vest, and legs observable,
   including loaded rounds, signed jam condition, and chamber state in the hand
   records. The current automatic
   four-mercenary starter encounter, grow-only late-peer fresh-baseline joins,
   hostile-victory cold return/checkpoint, twelve-intent path including one
   synchronous visible-door open/close action, exact-serial interrupt pass, and
   native equipped-kit adjacent teammate first-aid initiation and bounded
   ordinary equipment/pocket inventory swaps and same-owner whole-stack transfers,
   passive logical fallback, renderer-only JA2 static map/actor view with
   authority lighting and assigned camera, and real-socket
   Move/aimed-fire/reload/interrupt-pass test cover the previously verified technical slice;
   new transfer verification is tracked in [COOP_COMPLETION.md](COOP_COMPLETION.md). A
   complete tactical-panel/visibility presentation and multiplayer firefight do
   not.
5. Add a complete tactical baseline and one authoritative firefight, including
   AI, RNG, interrupts, damage, death, the authorized/chunked complete 55-slot
   inventory, reserve ammunition, attachments/LBE, and reconnect.
6. Add player-driven tactical-to-map transition, one shared squad, adjacent-
   sector travel, and a paused strategic map. The current transition is only the
   automatic drained hostile-victory return. Protocol 14 now connects adjacent
   travel and supported arrival choices to shared-player requests and the
   ordinary worldless Screen; installed multi-encounter qualification remains
   required.
7. Add one server-owned campaign day: compression policy, hourly events,
   assignments, contracts, finance, and one serialized story interaction.
8. Add server-worker restart/new-epoch identity continuity, operator-directed
   eviction/ownership transfer, and state-hash diagnostics. Bounded in-band
   tactical revision resynchronization is implemented, including rotated
   replacement-baseline retry and pending-command receipt replay.
   Client-process/composition restart within the same server
   epoch, grow-only same-epoch late-peer join at a fresh-baseline boundary, and
   authenticated voluntary self-retirement with bounded same-epoch replacement
   are already present.

Every milestone requires data-free model/codec tests plus a separate
installed-data end-to-end certification for the behavior it claims. The current
process smoke additionally requires a real tactical-world start and two clients
verifying their exact maps, committing non-empty assigned baselines, and
reaching renderer-ready presentation. Its optional gameplay client additionally
certifies actual A/B movement, replica agreement, and moved-state reconnect;
building and packaging the server is not proof that a milestone is fully
playable.
