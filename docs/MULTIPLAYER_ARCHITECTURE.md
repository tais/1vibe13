# Multiplayer architecture

## Status and terminology

The SDL3 multiplayer port contains two different server programs. They must not
be described as interchangeable:

- `ja2server` is a data-free lobby, turn-serialization, and packet-relay
  coordinator. It does not link the JA2 engine, load a world, run AI, advance a
  campaign, or create saves.
- `JA2 --dedicated` runs the complete game with dummy presentation drivers. It
  is the only current server process capable of owning tactical AI and campaign
  simulation.

The code under `Multiplayer/netshim/` is a narrow source/API compatibility
layer for the RakNet calls used by the legacy wrapper. Its wire protocol is a
project-specific framed SDL3_net TCP stream. It is not RakNet 3.401 wire or ABI
compatibility, and historical RakNet clients are not expected to connect.

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
identity to the transport-derived peer address; gameplay payloads never select
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
RakNet interoperability.

## Delivery milestones

1. Harden framing and file transfer; add malformed raw-wire tests.
2. Add the versioned admission registry and exact codec tests while leaving
   legacy arena bytes unchanged.
3. Admit two players to one fixed tactical sector, bind one mercenary to each,
   and authoritatively execute movement, facing, stance, stop, and end turn.
4. Add a complete tactical baseline and one authoritative firefight, including
   AI, RNG, interrupts, damage, death, inventory/ammunition, and reconnect.
5. Add tactical-to-map transition, one shared squad, adjacent-sector travel,
   and a paused strategic map.
6. Add one server-owned campaign day: compression policy, hourly events,
   assignments, contracts, finance, and one serialized story interaction.
7. Add server startup load, autosave/manual save, reconnect after restart,
   join-in-progress, revision resynchronization, and state-hash diagnostics.

Every milestone requires data-free model/codec tests plus a separate
installed-data end-to-end smoke. Building and packaging the server is not proof
that the milestone is playable.
