# Engine and modding framework architecture

## End state

The project should be usable as a campaign-agnostic tactical game engine. Jagged
Alliance 2, Unfinished Business, the 1.13 ruleset, editor tools, and third-party
mods should be packages above that engine rather than identities compiled into
it.

The intended dependency direction is:

```text
Applications          game executable, editor, dedicated/headless tools
    ↓
Campaign packages     JA2, UB, maps, quests, dialogue, campaign bootstrap
    ↓
Rules packages        1.13 combat, inventory, traits, AI policies
    ↓
Engine services       simulation, entities, persistence, resources, states
    ↓
Engine/Core           dependency-free primitives and public contracts
    ↓
Platform adapters     SDL3 video/audio/input/network, filesystem, clocks
```

Dependencies may point downward or into an interface owned by a lower layer.
They must not point upward. Platform adapters implement engine-owned interfaces;
the engine must not contain SDL types in its public domain model.

## Current migration seams

- `Engine/Core` is a compiled, independently link-tested library containing
  platform- and campaign-independent primitives. CTest rejects project-header
  dependencies in this directory, and `engine_core_tests` links without SDL,
  SGP, JA2 globals, or campaign code so accidental upward dependencies fail at
  the engine boundary rather than hiding in the game executable.
- `GameContext` is the composition root. It initially binds legacy globals and
  will replace them service by service without changing save layouts en masse.
- `GameCapabilities` moves JA2/UB/editor decisions from preprocessing toward
  startup-selected runtime policy.
- `ContentRegistry` validates package identity, required engine API version,
  ordered requirements, optional requirements, conflicts, and weak ordering
  relationships. Relationships may target packages that have not been
  discovered yet, so registration order never determines validity.
- `PackageRegistry` owns package validation, dependency planning, and activation
  policy while package objects remain application-owned. Its iterative planner
  produces a stable topological activation delta: requirement declaration order
  and requested-root order both run from lower to higher overlay priority, and
  shared diamond dependencies activate once. The whole multi-root graph is
  checked before callbacks run. Activation is transactional; a callback, asset
  mount, or exception failure reverses only packages newly activated by that
  request and preserves the pre-existing active set. Batch activation of an
  already-active closure is therefore an idempotent success with an empty
  result delta, while the legacy single-package call reports `AlreadyActive`.
  Optional dependencies join the closure only when registered. Declared
  conflicts are symmetric, and weak `loadAfter` edges constrain packages that
  are already in the closure without selecting new ones. Strong and weak cycles
  fail before activation. Only one campaign may be active, whereas rules,
  extensions, and tools can be composed around it.
- `PackageCatalogSnapshot` is the value-only inspection boundary for launchers,
  editors, diagnostics, and headless hosts. It reports packages in deterministic
  host-discovery order, dependency consumers, activation priority, asset state,
  the active campaign, and bootstrap progress without exposing mutable registry
  storage or application-owned package pointers. Snapshots remain valid when a
  later lifecycle operation changes the registry.
- Hosts may additionally bind a `PackageEventSink` to receive deterministic,
  value-only registration, activation, bootstrap, rollback, shutdown, and
  teardown events. Event delivery follows lifecycle callback order. Sink
  exceptions are logged and isolated from package state, making observation
  safe for launchers, live diagnostics, and headless test recorders.
- Active packages protect their direct mandatory and present optional
  requirements from removal, which in turn protects the complete active closure.
  Dependencies are not automatically pruned when a consumer is removed; the
  host chooses explicit teardown order.
  Package activation returning false must leave that package inactive and
  holding no lifecycle resources.
- Mandatory and optional requirements express package identity plus an optional
  exact version. Versions are opaque, case-sensitive strings rather than SemVer
  ranges. Conflict and ordering relationships express identity only.
- `LegacyCampaignPackage` exposes the compiled JA2 or UB campaign through that
  runtime contract. It is the compatibility bridge to replace with discovered
  package manifests and campaign bootstrap hooks incrementally.
- `PackageHost` provides the first optional, data-only discovery adapter around
  that bridge. [Data Package v1](DATA_PACKAGES.md) validates manifests and
  dependency graphs at startup, then mounts legacy-format assets in resolved
  overlay order. With no package configuration it is a strict no-op: existing
  `Data-*` trees and `vfs_config.ini` behavior remain unchanged. This version
  deliberately has no native-code loading, runtime rescan/unload, or
  disk-discovered campaign bootstrap.
- Package-host startup is transactional across discovery registration, engine
  activation, and legacy VFS mounting. Resolution, preflight, activation, or
  mount failure reverses named VFS profiles, newly activated packages, and all
  registrations introduced by that attempt. Rollback continues after an
  individual teardown error and reports every incomplete step to the host.
  Every external mount attempt is unwound, including one that throws or
  reports failure after acquiring partial VFS state; unmount adapters are
  therefore idempotent for already-absent package IDs.
- Package bootstrap advances through ordered configure, content-load, and
  runtime-start phases. A failed phase rolls back in reverse package order;
  shutdown unwinds completed phases in reverse before legacy engine teardown.
- `LogSink` gives engine and package code a structured, captureable logging
  contract. The application binds an SDL sink; tests use an in-memory sink,
  and `Engine/Core` never depends on SDL or the legacy debug manager.
- `EngineServices` is the non-owning service table assembled by `GameContext`.
  Packages receive engine contracts for time, randomness, byte storage, and
  logging, input, audio, and frame presentation while the application retains ownership of
  SDL/VFS/legacy adapters. Headless and replay hosts can inject deterministic
  memory input, capture audio requests, and record frame presentation without devices.
- `Engine/Adapters/Legacy` contains the production compatibility implementations
  of engine-owned service contracts. They are compiled once into an explicit
  adapter object target and embedded in each SGP application archive. This
  keeps SDL/VFS/sound/video dependencies below the application composition
  root while making the remaining upward legacy calls visible and replaceable.
- `AssetSource` exposes normalized, read-only logical content with provenance
  and deterministic, case-insensitive overlays. `PackageRegistry` mounts an
  active package's optional source above the trusted host source, in activation
  order, and removes it before package teardown. This lets campaigns and mods
  replace assets without receiving writable save storage or importing the
  legacy VFS API. Package asset sources are application-owned and must retain a
  stable identity for their active lifetime. Package IDs and trusted asset
  provenance use portable ASCII letters, digits, `.`, `_`, and `-` only.
  Registry lifecycle operations are serialized and reject reentrant package
  callbacks; hosts must likewise serialize lifecycle changes with asset reads.
  This source-built alpha API has no stable binary plugin ABI yet; package
  binaries must be rebuilt with the engine. Content API 1.1 identifies packages
  that depend on lifecycle-mounted asset sources; 1.2 adds ordered package
  requirements; 1.3 adds optional requirements, conflicts, and weak ordering.
  Older content remains valid when it does not use newer contracts.
- `EngineHost` is the command- and game-agnostic composition root. It owns
  lifecycle, screen state, content, packages, capabilities, persistence, and
  service bindings for games, tools, package hosts, and tests.
  `Engine/Adapters/JA2` extends that host as `EngineRuntime`, adding the current
  JA2 tactical command and replay contracts without making Core depend on a
  game vocabulary. `GameContext` remains the JA2 compatibility facade around
  it plus legacy settings/options.
- `CommandStream<Command>` binds deterministic delivery to its matching
  best-effort journal without knowing the game's command vocabulary. Its
  `DeterministicCommandQueue` provides tick/sequence ordering for simulation,
  replays, multiplayer synchronization, and headless tests.
  Tactical end-turn input is the first production path: it queues an
  engine-owned value command and processes it at the existing synchronous call
  boundary before invoking the legacy executor.
- `ProcessCommandsThrough` snapshots one bounded ready set and acknowledges
  commands only after their handler returns. Applied commands run exactly once;
  retry blocks later deterministic work without removing it; explicit discard
  is counted; and a handler exception leaves the failing and remaining commands
  queued. Commands produced during a handler wait for the next pass, preventing
  accidental unbounded same-tick dispatch.
- `CommandJournal` records a bounded, best-effort history of submitted command
  values and their queued/applied/discarded/blocked state. Recording failures
  never alter simulation delivery. The JA2 adapter's versioned
  `SimulationCommandCodec` serializes explicit command tags rather than variant
  indexes, providing a stable capture boundary for diagnostics and future
  replay/network hosts.
  `HandleItem` firearm actions now enter this gateway before the compatibility
  executor queues the existing soldier event, while legacy AP, animation, and
  multiplayer behavior remain at their original synchronous boundary.
- The JA2 adapter's `CommandReplayService` stores those journals in
  integrity-checked runtime
  persistence envelopes. Replay loads are transactional, incomplete bounded
  captures are refused, and whole batches are staged atomically so duplicate
  sequence IDs cannot leave a partially mutated simulation queue. Playback
  retains the recorded tick, sequence, value, and source through the same
  runtime gateway used by live commands.
- `RuntimeCapabilities` replaces build-target identity at engine boundaries
  with portable, ordered feature IDs. Hosts contribute application traits and
  active packages contribute campaign, rules, extension, or tool traits;
  deactivation removes them automatically. The compiled JA2, UB, and editor
  defaults remain compatibility adapters, while live campaign decisions can
  now query the active package rather than a preprocessor branch.
- `BinaryArchive` provides bounded, endian-defined, versioned persistence.
  `EngineHost` owns the `PersistenceService` bound to its configured byte
  storage, so package hosts, games, and tools share one persistence boundary.
  Established raw and versioned records retain their wire format; new records
  can opt into a bounded envelope with an explicit payload length and checksum.
  Loads publish caller-visible data only after the complete record validates.
  The legacy town-distance sidecar is the first live game path routed through
  this runtime-owned service without changing its on-disk bytes.
- `StateStack` represents base screens and modal overlays without scattered
  previous-screen globals.
- `StateController` owns current, previous, and pending application state above
  that stack. The JA2 loop routes immediate and requested transitions through
  it, including message/chat overlays, while the widely read legacy screen
  scalars remain synchronized compatibility mirrors during migration.
- `StateRegistry` owns game-agnostic state lifecycle and dispatch callbacks.
  The live JA2 screen table is registered once as a compatibility adapter, and
  all initialization, handling, and shutdown now run through the runtime host.
  Callback failures and exceptions become explicit results instead of unchecked
  array dispatch, while the established numeric screen IDs and ordering remain
  unchanged for existing game, editor, and mod code.
- `FrameDriver` owns the update/present/complete sequence and deterministic
  identity of every completed application frame. The live JA2 loop is routed
  through it while preserving its established screen-update, presentation,
  clock, and network ordering. Headless hosts use the same driver with injected
  time and presentation services instead of a window or renderer.
- `InputDispatcher` drains a bounded engine input stream before each frame and
  fans events out in deterministic subscriber order. The SDL adapter mirrors
  accepted legacy queue events, so engine packages receive live input without
  consuming or reordering the tactical/UI event queue. Runtime-started packages
  receive these events in activation order; callback exceptions are isolated.
- `PackageLifecycle` advances package configuration, content loading, and
  runtime startup as one engine-owned transaction. JA2 retains its established
  loading boundaries, while a failed later phase now unwinds every earlier
  phase automatically and normal shutdown uses the same reverse-order path
  before package deactivation.
- typed resource owners bridge numeric SGP registries while platform services
  are extracted.
- soldier component views split behavior domains without moving serialized
  `SOLDIERTYPE` fields prematurely.

## Compatibility policy

Architecture migration must preserve existing campaigns and mods deliberately,
not accidentally.

1. Existing save formats remain readable. New formats carry magic and schema
   versions and have explicit migration paths.
2. Legacy globals and numeric handles remain adapters until all supported
   callers have a replacement.
3. Runtime capability defaults match the old build target until a unified
   executable can select packages at startup.
4. `SOLDIERTYPE` data is not reordered until a versioned entity serializer has
   replaced raw layout persistence.
5. Content API major versions signal breaking contracts. A package may require
   an engine minor version no newer than the running engine supports. Exact
   package requirements compare their opaque version strings byte-for-byte.
6. Deterministic simulation code cannot read wall-clock or render timing as an
   input to rules decisions.

## Extraction sequence

1. Keep expanding dependency-free core contracts and enforce their boundary.
2. Introduce interfaces for filesystem, resources, input, audio, rendering,
   clocks, randomness, and logging; bind SDL/SGP implementations in the app.
3. Move world/entity state behind `GameContext` and versioned persistence.
4. Route player, AI, network, and replay actions through simulation commands.
5. Convert JA2/UB/editor preprocessing branches into registered runtime
   capabilities and package hooks.
6. Move campaign bootstrap, quests, maps, dialogue, and item/rules data into
   versioned content packages.
7. Publish the stable engine API/SDK, package schema, examples, and compatibility
   test kit for mod authors.

## Review gates

An engine extraction PR is complete only when:

- dependency direction is no worse and automated boundaries still pass;
- ASan and headless tests pass;
- JA2, UB, and Map Editor release variants build;
- old saves/content remain supported or a tested version migration is included;
- new public contracts state ownership, lifetime, determinism, and versioning;
- campaign-specific behavior does not enter `Engine/Core`.

The installable public surface and external-consumer contract are documented in
[`ENGINE_SDK.md`](ENGINE_SDK.md).
