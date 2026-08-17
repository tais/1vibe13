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

Clients own only presentation and local input. They send bounded intent such as
move, face, change stance, fire, interact, or end turn. They do not send an
authoritative path, action points, damage, death, inventory, world state, or AI
result. The server validates intent, submits a value-only simulation command,
and publishes an ordered receipt plus state delta.

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

The current 116-byte request has no separate application or campaign-name
field. A future live adapter must derive the session being joined from the
server endpoint and bind its exact runtime/content identities before enabling
admission; it must not infer a campaign identity from client-provided prose.

An all-zero identity/token requests a new seat. A successful new admission
returns a unique identity and opaque reconnect token. A reconnect presents the
pair and atomically replaces the previous transport binding. The registry binds
identity to the transport-derived opaque connection ID; gameplay payloads never select
their own sender identity.

Reconnect tokens travel over the current plaintext transport. They are bearer
identity continuity, not encryption, Internet authentication, or protection
from an on-path attacker. Public Internet hosting requires a separately scoped
secure transport or authenticated tunnel.

Unknown protocol versions, malformed lengths, incomplete authority
configuration, fingerprint/content mismatches, stale epochs, duplicate live
bindings, invalid tokens, and capacity exhaustion are explicit rejection
reasons. Disabled or incompletely configured authoritative mode admits nobody.

## Intent envelope and ordering

Each future authoritative intent carries:

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
stop, and end-turn intents plus a bounded authority gate. The gate resolves the
peer from the transport-bound admission registry, verifies the claimed identity,
world generation, exact revision and turn serial, enforces a per-peer monotonic
command sequence, and checks a server-owned peer-to-actor ACL. It deliberately
executes no game action. Wiring successful intents to live JA2 legality checks
and the simulation command queue remains part of milestone 3.

`FullEngineCoopIngress` now composes those contracts behind an explicit
session/generation lifecycle for the future full-engine dedicated adapter. It
decodes admission and intent bytes, derives authority only from the receiving
transport, and hands an authorized, identity-sanitized value to an injected
execution sink. It is deliberately not installed in `Multiplayer/server.cpp`:
the full-engine startup path does not yet produce a canonical installed-content
manifest digest, and no authoritative receipt/delta path exists. The seam does
not register a legacy RPC, calculate a placeholder digest, execute JA2 commands,
or make co-op playable.

## Dedicated launch lifecycle

The full-engine process now has an explicit startup contract:

- `JA2 --dedicated` and `JA2 --dedicated --dedicated-mode=pvp` select the
  transitional PvP host. Only legacy deathmatch and team-deathmatch settings
  are accepted; legacy `GAME_TYPE=2` is rejected.
- `--dedicated-mode=coop` requires a bounded campaign identifier, an explicit
  `--campaign-action=new|resume`, and an absolute `--dedicated-state-dir`.
  Campaign identifiers are lowercase-canonical. The options are parsed so the
  durable lifecycle can attach to a stable interface, but startup currently
  exits before SDL initialization because campaign authority and replication
  are not installed.
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
  thread stops the frame loop and closes the local client before the server
  transport, ahead of game and VFS teardown. A future final campaign
  checkpoint belongs at this same main-thread boundary, never in the signal
  handler.

This repairs the full-engine PvP bootstrap; it does not yet satisfy campaign
persistence milestone 3 or make network co-op playable.

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

The 176-byte little-endian manifest binds the full campaign identifier, mode,
slot, generation, world time, runtime compatibility fingerprint, separately
named installed-content SHA-256, and checkpoint size/SHA-256. New campaigns
refuse any pre-existing manifest bytes, and missing storage is distinct from an
I/O failure. Co-op creation and resume reject an absent installed-content
digest. The checksum protects the manifest from accidental corruption; it is
not an authentication mechanism.

The native filesystem backend now turns an operator-created absolute state
root into one retained process lease and fixed campaign-local paths. The root
must live on a local filesystem beneath trusted, stable ancestors; its leaf may
not be a symlink or reparse point. On POSIX, the backend verifies current-user
ownership and private permissions; managed directories are opened relative to
retained directory descriptors, forced to mode `0700`, and synced when created.
On Windows, the service operator must provision an equivalently private ACL;
the backend rejects reparse points and retains no-delete-share directory
handles. A stable `process.lock` is never removed, so two workers cannot share
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
package overlays stay below the campaign profile. A new campaign must reject a
pre-existing nonempty `profile/`; resume may reuse it. Startup has not invoked
this seam yet, and native opt-in diagnostic sinks outside bfVFS still need an
explicit campaign-local log policy.

Checkpoint probing is bounded to 256 MiB and hashes the closed file with
streaming SHA-256. Manifest reads buffer at most 176 bytes plus an oversized
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

The adapter is deliberately not wired into dedicated startup yet. Startup
still needs to acquire the lease before VFS initialization, install the
prepared campaign-isolated writable profile (including `Temp/`, not merely
`SavedGames/`), enforce the new-versus-resume profile policy, materialize and
verify the active checkpoint before the VFS scan, and run periodic/final
checkpoints only at committed main-thread frame boundaries. Installed-content
identity and deterministic host RNG/reinforcement state also remain mandatory.

The deterministic foundation is now explicit but still unwired. The engine
core provides a fixed PCG32 simulation stream with a canonical 40-byte
little-endian checkpoint, campaign-seed binding, exact replay, unbiased bounded
draws, and fail-closed sequence exhaustion. A separate data-free eligibility
policy permits only a paused, committed, queue-drained, cold strategic
checkpoint: no tactical world, combat, projectiles, active or queued dialogue
effects, pending dialogue trigger timers, realtime AI, temporary schedules,
reinforcement transients, MiniEvents, or unrestricted Lua randomness. The
first persistent-campaign milestone therefore rolls an interrupted tactical
battle back to the preceding strategic checkpoint; it does not claim
mid-battle resume.

Writing the inactive A/B slot means the runtime container itself need not gain
a new `ByteStorage` atomic-write API: close, sync, semantic probe, and hash must
all succeed before its manifest is published. Until those lifecycle and
determinism conditions are met, co-op admission remains closed and no
persistent-campaign compatibility claim is made.

## Replication

Join and reconnect begin with a complete, checksummed baseline for one world
generation. Ordered deltas then advance a monotonically increasing revision.
Clients acknowledge applied revisions; a gap, checksum mismatch, queue limit,
or generation change discards the partial replica and requests a fresh
baseline.

The existing tactical observer is only a starting point. A playable combat
baseline additionally needs inventories/ammunition, items, structures and
doors, bombs, smoke, projectiles/explosions, statuses, visibility/opponent
knowledge, interrupts, and pending asynchronous actions.

Sector entry/exit, tactical-to-strategic transitions, autoresolve, and campaign
load are generation barriers. Player input remains frozen until required peers
acknowledge the new baseline.

## Campaign policy

Legacy network mode currently suppresses strategic updates and time
compression. Authoritative co-op replaces those blanket skips with an explicit
authority/replica policy:

- only the server executes strategic events and campaign mutation;
- clients render the replicated clock and map state;
- any player may pause immediately;
- compression initially requires every active player ready, or a configured
  campaign leader;
- dialogue, combat, and scheduled server events cancel compression;
- laptop/dialogue transactions are serialized until finer ownership exists.

Only the server writes campaign saves. Initial load occurs at server startup.
Live load, when added, creates a new session epoch and forces a complete
baseline; disk saves are never sent as a replica protocol.

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
- reject an invalid explicit bind address rather than falling back to every
  interface.

The current plaintext protocol and admin password are suitable only for
experimental trusted-LAN use. No release should claim secure public hosting or
historical-client interoperability.

## Delivery milestones

1. Harden framing and file transfer; add malformed raw-wire tests.
2. Add the versioned admission registry and exact codec tests while leaving
   legacy arena bytes unchanged.
3. Add one explicit `JA2 --dedicated` PvP/co-op lifecycle plus crash-safe
   campaign create/resume, locking, periodic/manual checkpoint, and graceful
   shutdown checkpoint. Co-op networking remains closed until authority and
   replication are ready.
4. Admit two players to one fixed tactical sector, bind one mercenary to each,
   and authoritatively execute movement, facing, stance, stop, and end turn.
5. Add a complete tactical baseline and one authoritative firefight, including
   AI, RNG, interrupts, damage, death, inventory/ammunition, and reconnect.
6. Add tactical-to-map transition, one shared squad, adjacent-sector travel,
   and a paused strategic map.
7. Add one server-owned campaign day: compression policy, hourly events,
   assignments, contracts, finance, and one serialized story interaction.
8. Add reconnect after restart, join-in-progress, revision resynchronization,
   and state-hash diagnostics.

Every milestone requires data-free model/codec tests plus a separate
installed-data end-to-end smoke. Building and packaging the server is not proof
that the milestone is playable.
