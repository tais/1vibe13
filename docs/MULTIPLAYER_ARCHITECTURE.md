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

The global authoritative co-op session protocol is version 7. The fixed server-
hello container retains its independently bounded wire-v1 layout. The inner
tactical envelope is wire v3. Tactical intent is wire v3, snapshot is wire v7, delta is wire
v6, and the simulation-command journal is wire v4. Global v1/v2/v3/v4/v5/v6 peers fail
admission rather than discovering the mismatch after admission. All older
snapshot layouts are rejected instead of inferring dimensions, hostility, door,
loadout, or interrupt state. Snapshot v7 carries one public `commandsBlocked`
bit, compact interrupt phase/serial, per-actor interrupt-action eligibility, and five
bounded 12-byte combat-equipment records: primary hand, secondary hand, helmet,
vest, and legs. Native interrupt lists and hidden interrupters remain private.

The current co-op deliverable is a functional but deliberately narrow
technical vertical slice. The full-engine server owns a persistent campaign,
automatic starter encounter, admission, campaign transfer, and authoritative
tactical execution. A normal `JA2` client process joins over the production
SDL3_net adapter, commits the campaign checkpoint into private scratch storage,
and presents a worldless committed-snapshot control screen with a passive
logical-grid plot. This status does not imply a JA2 terrain/static-world renderer
or complete combat/strategic replication.

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
nine bounded intents: move, face, change stance, stop, end turn, an
exact-target aimed single-shot firearm attack with aim time from zero through
eight, selected-actor reload, visible adjacent-door open/close, and exact-serial
interrupt pass. The reload
payload is empty: the server
prepares the existing `ReloadWeaponCommand`, resolves the selected weapon and
ammunition, and revalidates native `AutoReload`, including manual chambering.
The door payload is the exact public base grid, current ephemeral structure ID,
and desired open state; it carries no lock, trap, key, database, or perceived-
state fingerprint. Interaction beyond this synchronous ordinary door action,
general inventory management, burst/autofire, melee, thrown
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
wire v3 serializes it so replay source substitution cannot erase co-op
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
stop, end-turn, aimed-firearm, zero-payload reload, and door open/close intents
plus a bounded authority gate. The
gate resolves the peer from the transport-bound admission registry, verifies the claimed identity,
world generation, exact revision and turn serial, enforces a per-peer monotonic
command sequence, and checks a server-owned peer-to-actor ACL. The reusable gate
executes no game action itself; the production dedicated composition now hands
its sanitized value to the main-thread JA2 tactical host, which rechecks live
turn/actor policy and submits the nine supported intent kinds to the bounded
simulation command queue.

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

The passive game-loop branch never advances the local JA2 clock, frame driver,
package host, runtime-message bus, command queue, observer, AI, pathing,
campaign, or tactical simulation. Instead, the existing `INIT_SCREEN` renders a
worldless sector/turn header, an exact authority-dimensioned isometric diamond
with friendly-team actor markers, and a committed actor-state table fallback.
The authority always retains player-team actors in that snapshot. Every
non-player actor requires the player team's exact public `SEEN_CURRENTLY`
knowledge; loss and reacquisition become ordinary actor-left and actor-entered
deltas.
It identifies the client's server-assigned actors and reports outstanding
commands and terminal receipts. Outside every modal, direct movement is an
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
cancels. Door mode blocks the voluntary-leave confirmation gesture. A committed
`commandsBlocked` bit disables every action and closes destination, attack, and
door modes until a later committed baseline or delta clears it. The
one-command lock remains held from submission through the authoritative terminal
receipt; the screen never predicts movement, AP spend, or damage. It drains the
complete local input queue, so mouse, key-up/repeat, and unsupported events
cannot leak into a legacy JA2 screen or simulation path.

This gives players a usable protocol-control surface, not the JA2 tactical
terrain view. The first object in each of five combat-equipment slots is
replicated, including loaded-ammunition state for the two hand records. The
future authorized, chunked full 55-slot inventory domain must still cover
reserve ammunition, equipment-stack tails, attachments/LBE, and other items.
General structures, door lock/key/trap state and asynchronous door work,
complete per-peer
visibility/opponent knowledge, bombs/smoke, projectiles/explosions,
interactions, broader attacks, interrupt detail, and pending asynchronous
actions are not yet complete replica domains. Friendly-only plot markers are a
presentation choice: the table exposes every record that passed the shared
public-current-visibility gate, including non-player actors. That server gate is
data minimization, not a complete per-client fog-of-war or confidentiality
system.

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
  omits only paths below an explicit exclusive VFS ancestor, and applies the
  normalized effective read-only overlay. Case-only spellings across different
  layers are ordinary overlays selected by the smallest layer; a same-layer
  spelling ambiguity, same-layer exact duplicate, or remaining writable shadow
  fails closed. Late campaign open uses the cached digest rather than
  re-enumerating the VFS, then binds the runtime fingerprint and opens the exact
  A/B campaign. Resume materializes the selected checkpoint bytes into the
  already-created scratch entry before the legacy save load begins. After
  campaign entry, the admission listener binds `--dedicated-coop-bind` and
  `--dedicated-coop-port`, which default to `0.0.0.0:60005`. An invalid explicit
  address or port fails closed instead of silently widening exposure. The
  default listens on every IPv4 interface, so operators must restrict the host
  to a trusted LAN/VPN or choose a narrower bind address.
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
  while the passive client reaches its worldless presentation, before a pending
  main-menu transition can commit. Ordinary and legacy PvP shell routing still
  owns `InitMainMenu`. The passive client bypasses `INTRO_SCREEN`, selecting
  INIT state zero directly so its INIT-only frame policy cannot loop before
  `InitializeJA2` opens live transport. Only a dedicated new campaign calls `InitGameOptions()`
  immediately before `InitNewGame(FALSE)`; bypassing the interactive settings
  screen otherwise leaves zero, but the installed strategic/Lua difficulty domain is 1..4. Resume never overwrites checkpointed options.
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
passive full-engine client, complete campaign transfer, and a worldless typed-
intent UI with a logical spatial plot for co-op. Broader combat/world
replication, the JA2 terrain/static-world renderer, and general strategic
mission/session control remain outside this slice.

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
occurrence, then omits only explicit `CVirtualLocation::getIsExclusive()`
ancestors such as `ShadeTables`, `Temp`, and save sidecars; any remaining
writable shadow still fails closed. Native opt-in diagnostic sinks outside
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
snapshot replica, and worldless control screen are composed in `JA2`; none of
those client layers run authoritative simulation.

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

The embedded tactical snapshot wire is version 7. Its 53-byte header, 92-byte
actor record, and 7-byte door record carry exact dimensions, canonical actor
`hostileToPlayerTeam`, interrupt-action eligibility, five bounded 12-byte combat-equipment records, and the
visible-door `{baseGrid, structureId, open}` projection. The records cover
primary hand, secondary hand, helmet, vest, and legs. Each captures only the
first object's item ID, stack count, and condition. For an ammunition-bearing
hand object it also captures loaded-ammunition item/count, signed ammunition
condition (including a negative jam state), and chambered state; those fields
stay canonical zero for ordinary equipment. Decode and replica application are
transactional; older layouts are rejected rather than having missing state
inferred. The header carries canonical `commandsBlocked`, interrupt phase, and
interrupt serial. Snapshot v7's generic ceiling is 384053 bytes. Delta wire v6 adds
interrupt eligibility to actor vitals and carries a same-serial phase transition
as one exact 43-byte turn event; actor-loadout changes remain after vitals and
before door entered/left/changed events. It has a generic
18434-event ceiling. `TacticalWorldService` and
`TacticalWorldObserverService` are both
version 2.0, and observer `DoorCapacityReached` is the stable value 12.

The narrower co-op envelope uses inner tactical wire v3 and caps a publication
at 256 actors, 1024 doors, and 3074 events. Its baseline payload/envelope bounds
are 30773/32385 bytes and its category-aware delta payload/envelope bounds are
62034/62106 bytes, all below the public 64 KiB ceiling. Intent wire v3 retains a
72-byte header, 8-byte maximum payload, and 80-byte maximum record. The command
journal is wire v4; pointer-free authoritative door/pass commands use tags 33/34.

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

The existing tactical observer is only a starting point. The current worldless
screen renders its committed public-visibility-filtered sector/turn and actor
records as a passive logical diamond plus table, but the five-slot combat-
equipment projection is not a complete inventory. A full combat baseline
additionally needs the future authorized, chunked full 55-slot inventory domain,
including reserve ammunition, remaining equipment-stack objects,
attachments/LBE, other items, general structures, door lock/key/trap and
asynchronous interaction state, bombs, smoke,
projectiles/explosions, statuses, per-peer visibility/opponent knowledge,
interrupts, broader attacks/interactions, and pending asynchronous actions.

Sector entry/exit, tactical-to-strategic transitions, autoresolve, and campaign
load are generation barriers. Player input remains frozen until required peers
acknowledge the new baseline.

## Campaign policy

Legacy network mode currently suppresses strategic updates and time
compression. Authoritative co-op is moving those blanket skips to an explicit
authority/replica policy:

- only the server executes strategic events and campaign mutation;
- the current client synchronizes and cold-loads the checkpoint but presents
  only committed tactical sector/turn/actor records, not a replicated strategic
  map;
- pause/compression leadership, dialogue, laptop transactions, general mission
  selection, and sector transition control remain unfinished session policy.

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
- derive roster/team/actor identity from the admitted transport binding;
- apply per-peer queue and work budgets without silently dropping reliable
  state; disconnect or backpressure instead;
- cap each co-op connection's SDL_net pending-write bytes (256 KiB by default)
  before enqueueing another baseline, delta, or receipt; query failure or a
  crossed high-water immediately unbinds and closes that transport;
- reject an invalid explicit bind address rather than falling back to every
  interface.

The full-engine listener defaults to `0.0.0.0:60005`, so an operator who does
not override `--dedicated-coop-bind` exposes permissionless first-join admission
on every IPv4 interface. `--dedicated-coop-port` changes the listener port; the
client's `--coop-port` must match. Plaintext bearer tokens are transport-bound
continuity credentials, not user authentication. The standalone coordinator's
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
requires its final checkpoint and zero exit, resumes that checkpoint on a fresh
port, and shuts it down cleanly. A mutation-relevant installed-data
metadata fingerprint must match before and after. Process groups and temporary state are
cleaned deterministically, and installed data is mounted only through the
private run roots. This certifies that bounded lifecycle, not a complete
tactical playthrough or long-running soak.

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
   hostile-victory cold return/checkpoint, nine-intent path including one
   synchronous visible-door open/close action and exact-serial interrupt pass,
   passive logical plot, and real-socket Move/aimed-fire/reload/interrupt-pass
   test cover this technical slice; a JA2 terrain-
   rendered multiplayer firefight does not.
5. Add a complete tactical baseline and one authoritative firefight, including
   AI, RNG, interrupts, damage, death, the authorized/chunked complete 55-slot
   inventory, reserve ammunition, attachments/LBE, and reconnect.
6. Add player-driven tactical-to-map transition, one shared squad, adjacent-
   sector travel, and a paused strategic map. The current transition is only the
   automatic drained hostile-victory return.
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
process smoke covers create/Ready/reconnect/checkpoint/resume only; building and
packaging the server is not proof that a milestone is fully playable.
