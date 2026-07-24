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
- `GameContext` is the composition root. It binds application adapters to the
  engine runtime and retires duplicate global owners service by service without
  changing unrelated save layouts en masse.
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
- `RulesContentBootstrapHost` and `CampaignRuntimeBootstrapHost` are separate
  narrow application ports for the process-lifetime work not yet represented
  as data. There is no shared gameplay-runtime object or cross-package C++
  lifecycle: each package owns its one-shot admission and the engine package
  transaction owns rollback. The compiled `ja2.1.13@1.13` rules package owns
  legacy table and text loading during `LoadContent` and contributes
  `rules.ja2-1.13`. Every built-in or v4 data campaign has that exact package as
  a mandatory dependency. Campaigns own their identity, assets, declared
  content, and grid/Lua startup during `StartRuntime`.
  `LegacyCampaignPackage` is a registered fallback rather than a pre-activated
  singleton. A Data Package v4 `campaign` is its peer and drives the same
  application bootstrap port.
  `CAMPAIGN_FAMILY` and a host capability reject a JA2/UB mismatch before any
  rules or campaign bootstrap callback runs.
- `PackageHost` is the optional, data-only discovery adapter around that
  bridge. [Data Packages](DATA_PACKAGES.md) validate manifests and dependency
  graphs at startup, then mount legacy-format assets in resolved overlay order.
  An extension-only selection automatically includes the built-in campaign;
  a selected v4 campaign replaces it. Both resolve through the registered
  `ja2.1.13` rules package. With no package configuration discovery remains a
  no-op and startup selects the registered rules → built-in campaign graph.
  There is still no native-code loading or runtime rescan/hot-unload.
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
  The detailed engine result retains the original local callback exception
  after rollback, so the application can surface established loading
  diagnostics without a package-specific exception mailbox.
- `LogSink` gives engine and package code a structured, captureable logging
  contract. The application binds an SDL sink; tests use an in-memory sink,
  and `Engine/Core` never depends on SDL or the legacy debug manager.
- `EngineServices` is the non-owning service table assembled by `GameContext`.
  Packages receive engine contracts for time, randomness, byte storage, and
  logging, input, audio, and frame presentation while the application retains
  ownership of SDL/VFS/legacy adapters. Headless and replay hosts can inject
  deterministic memory input, capture audio requests, and record frame
  presentation without devices.
- `Engine/Adapters/Legacy` contains the production compatibility implementations
  of engine-owned service contracts. They are compiled once into an explicit
  adapter object target and embedded in each SGP application archive. This
  keeps SDL/VFS/sound/video dependencies below the application composition
  root while making the remaining upward legacy calls visible and replaceable.
  Existing `RefreshScreen` and `PresentNow` callers now enter a bindable
  `FramePresenter` gateway, preserving paced versus immediate behavior while
  allowing headless hosts to capture forced legacy presentations. Nested
  gateway calls are suppressed and presenter exceptions cannot unwind through
  old UI code. Legacy dirty rectangles likewise enter `FrameInvalidator`,
  preserving clipping, union, full-redraw, and change-marker behavior while
  making damage captureable without a renderer. `RenderSurfaceAccess` is the
  lower pixel-storage seam: surface roles, descriptions, and mapped bytes are
  engine values, while the established numeric SGP handles remain compatibility
  adapters. `RenderCommandSink` is the higher-level draw seam; live rectangle
  fills and numeric surface copies cross it alongside numeric surface
  stretching, rectangular shading, typed depth fills, managed video-object
  draws, basic depth-tested image draws, and outline-aware image effects. The
  tactical Z-buffer is exposed as the standard `DepthBuffer` role with
  `Depth16` storage. Full-world redraws clear it through
  `RenderDepthFillCommand`, so they no longer lock an unrelated colour surface
  merely to rediscover the row pitch. Depth remains physically compatible with
  the established padded SGP allocation and is not accepted by colour fill,
  copy, stretch, or shade commands.
  `RenderImageDrawCommand` carries an opaque stable image identity, frame,
  destination, anchor, explicit clipping region, and composite mode. Numeric
  `BltVideoObjectFromIndex` calls and direct `BltVideoObject` calls whose object
  belongs to the stable manager therefore traverse the same bindable service
  used by packages and headless recorders. Its source-transparent mode samples
  palette colour, while shadow and intensity modes transform destination pixels
  under the visible source mask. `ClearDestination` writes transparent black
  through that mask using the destination's actual pixel width, which replaces
  the tactical renderer's last hard-coded 16-bit colour stride.
  `PaletteWithShadowMarker` additionally names
  an immutable host palette through `RenderPaletteId`; source index 254 shades
  the destination unless explicitly ignored, and an optional stable image
  identity supplies the parallel legacy alpha stream. Neither palette pointers
  nor backend pixel layout cross the engine boundary. `RenderImageOutlineCommand`
  separately models colour outlines, transparent outline markers, and
  body-shadow rendering without leaking ETRLE marker values or packed legacy
  colours through the engine API. `RenderImageDepthOutlineCommand` combines
  that outline vocabulary with separate colour and `Depth16` destinations,
  an explicit front comparison and depth-write policy, plus visible-only or
  checkerboard-when-obscured behavior. Visible-only outline markers can paint
  colour without mutating depth; the pixelated form retains the legacy rule
  that every front-facing pixel, including a marker, replaces depth.
  `RenderImageDepthDrawCommand` names colour and depth destinations separately
  and makes its effect, comparison, and preserve/replace-on-pass/replace-on-draw
  policy
  explicit. Palette effects can copy source colour, blend it with the
  destination at 50%, or sample it through an absolute-coordinate
  checkerboard; all retain their inclusive greater-or-equal test and update
  depth only for sampled passing pixels when requested. Shadow and intensity
  images are destination-transforming masks and retain their strict
  greater-than test. The obscured-sprite effect uses a strict front test,
  renders failed pixels through the stable checkerboard, and explicitly
  distinguishes replacing only front-facing depth from replacing every drawn
  pixel. The depth form of `PaletteWithShadowMarker` retains its inclusive
  comparison, optional preserve/replace-on-pass depth policy, marker shading,
  ignore behavior, and optional alpha image. Its obscured variant preserves
  depth, draws passing pixels with the same palette/marker/alpha rules, and
  samples failed non-marker pixels through the absolute-coordinate
  checkerboard. Marker shading remains strictly front-facing, including the
  historical no-shadow-at-equal-depth rule. Strip-depth effects additionally
  name a source-owned `depthProfileFrame`. That profile varies depth across
  successive vertical strips without exposing `ZStripInfo`: ordinary
  structures use strict or wall-inclusive comparison and replace passing
  depth, while obscured structures replace checkerboard samples too.
  Palette/marker strip effects retain their separate profile increment,
  optional alpha image, marker handling, and the historical alpha-obscured
  strict comparison versus the non-alpha inclusive comparison. Unsupported
  resource, profile, effect, comparison, and write combinations are rejected
  at the platform boundary.
  `RenderImageDepthVisibilityQuery` is the read-only depth counterpart. It
  names only a `Depth16` surface, stable image/frame, anchor, clip, and signed
  legacy depth, and returns `FullyOccluded`, `Visible`, or `Unsupported`.
  Keeping rejection distinct from occlusion lets external/headless hosts
  decline the read while pointer-built compatibility images fall back to the
  exact local query.
  The production sink resolves image identities and executes the established
  ETRLE/palette blitters, so asset formats, clipping, shade palettes, and
  physical pixels remain unchanged. Every successful `CreateVideoObject`
  allocation receives a non-pointer render identity above the legacy 32-bit
  manager range. Ordinary, shadow, intensity, outline, depth, palette-effect,
  and multi-Z draws use that identity whether or not the object was also
  inserted into the legacy manager; deletion retires it before releasing
  storage. Only manually assembled compatibility fixtures without a
  `CreateVideoObject` lifetime retain the exact pointer-backed fallback. Every
  generated 256-entry render palette likewise receives a non-pointer identity
  above that range. Registration is idempotent for a live palette pointer,
  identities are never reused, and each owning replacement/destructor retires
  the identity before freeing its borrowed immutable storage. Sequential
  compatibility manager handles are unchanged. Common tactical transparent-Z
  sprites, 50% blended, checkerboard-sampled, or
  checkerboard-when-obscured depth sprites, basic depth-tested
  shadow/intensity masks, regular or depth-tested item outlines, and ordinary
  or checkerboard-when-obscured palette-remapped merc/corpse sprites with
  optional alpha can
  therefore use engine commands even when their image is owned by the
  animation, tile, or logical-body subsystem. The palette-effect route
  preserves inclusive depth testing, the checkerboard's absolute screen phase,
  clipping, and optional writes. The obscured route preserves its strict front
  test and the historical distinction where the clipped blitter updates only
  front-facing depth while the unclipped blitter updates every drawn pixel.
  The outline route preserves the legacy strict comparison for unclipped
  obscured sprites, inclusive comparison for their clipped counterpart,
  marker-specific depth behavior, and stable checkerboard pixelation. The mask
  bridge also corrects the clipped no-write path to preserve depth consistently
  with its unclipped counterpart. Rejecting hosts and manually constructed
  fixtures retain the exact raw fallback. Basic non-depth tactical
  transparent/shadow/intensity sprites and ordinary palette-shadow sprites
  traverse the regular image command through the same stable identities.
  Multi-Z walls, structures, multi-tile actors, and corpses now traverse the
  depth-image command with their explicit profile frame. Their seven formerly
  duplicated implementations live in the dedicated SGP multi-Z backend rather
  than the tactical world renderer; rejecting hosts retain the exact raw
  fallback. Riot shields and wall decals also use the ordinary depth-image
  command, while sprite-footprint clears and tile-redundancy reads use the
  clear-mask command and visibility query. Their raw RGBA8888 mask, inverse-Z,
  and signed occlusion implementations now live in dedicated SGP backends;
  the historical converted-sprite cache is likewise isolated behind dedicated
  native-pixel cache and raster backends. It validates indexed ETRLE before
  allocation, expands shade entries into dense `PIXEL` storage once, and keeps
  transparency in a separate mask so opaque black is not a colour-key casualty.
  The old `*16BPP*` entry points remain source-compatible aliases but no longer
  own or decode packed 2-byte colour data. Imported true-colour video objects
  follow the same storage contract: a transactional HIMAGE boundary expands
  RGB565 or normalizes PNG RGBA bytes into native ARGB and records 0–255
  opacity separately. Clipped normal and shadow draws then share the dedicated
  native-pixel raster backend, so platform execution never performs a repeated
  source-channel swizzle.
  `renderworld.cpp` owns orchestration only. The historical clipped
  physics-object outline remains non-depth
  and uses the regular outline command. Other
  raw application buffer operations deliberately retain their compatibility
  path until their individual command semantics migrate. Copy and
  nearest-neighbour stretch commands cover clipped
  opaque and RGB-colour-key operations, including defined same-surface overlap;
  shade commands preserve alpha and carry their factor explicitly. Depth fills
  clip to logical pixels without writing row padding. Legacy
  packed colours, mutable shade percentages, and RGB565 transparency tokens are
  decoded only in compatibility code. Legacy image tiling is now a bounded,
  clipped compatibility operation instead of a stub. Mapping is serialized with
  renderer lifetime and storage remains adapter-owned. Repeated maps of one
  platform surface are reference-counted, so a nested engine command cannot
  retire the outer render pass's pointer identity; replacement and deletion
  reject live mappings. Raw SGP/SDL presentation,
  invalidation, surface-mapping, and managed-image execution entry points are
  private to platform adapters, while ownership of legacy draw entry points is
  protected by the architecture check.
  Its bounded XML document adapter now owns the common Expat lifetime and
  all-or-nothing asset read path used by the conventional tactical definition
  loaders, campaign/bootstrap definitions, startup layout, editor action data,
  explosion metadata, multiplayer team definitions, Laptop content
  definitions, and the historical `ParseXMLFile` compatibility callers.
  It accepts any `AssetSource` as well as the compatibility VFS, so the same
  legacy callbacks can consume memory/package content without importing
  FileMan. Missing and I/O failures preserve the loaders' silent fallback
  behavior; malformed, oversized, allocation, and parser failures return
  structured diagnostics. A before-parse hook runs only after a successful
  bounded read, preserving loaders whose table reset must not occur when an
  optional asset is absent. Strategic and Laptop now have no direct Expat
  parser ownership; Strategic's sector/difficulty-specific extra-item loader
  also uses bounded asset reads while retaining its established selection
  order. A parser-ready hook lets the older object-oriented property,
  tileset/structure, and image-app-data readers borrow that same scoped parser
  without taking over its lifetime. Utils and SGP now have no direct Expat
  parser ownership, and an uncompiled duplicate weapon reader has been removed.
  Dealer-inventory semantic diagnostics retain live parser line numbers through
  that hook. The logical-body loader now uses the same bounded `AssetSource`
  pipeline for both its root document and external entities. Its specialized
  callbacks still receive the live root or child parser, and semantic callback
  exceptions retain their bounded message and parser position. Parser and
  buffer ownership is consequently centralized below the game layer, with an
  architecture check preventing new production-owned Expat parsers.
- The legacy SGP, Utils, and Laptop manifests explicitly separate
  campaign-neutral translation units from sources that still consume
  JA2/UB/editor definitions. The neutral object layers compile once and are
  embedded into each unchanged application archive; sensitive sources remain
  per application. A source moves into the neutral partition only after its
  objects match for JA2, UB, Map Editor, and UB Map Editor in both Release and
  Debug/ASan builds. This reduces build duplication without pretending that
  legacy UI code has already become an engine service.
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
  requirements; 1.3 adds optional requirements, conflicts, and weak ordering;
  1.4 adds inspectable localization-file and versioned definition-asset
  declarations; 1.5 adds application-hosted data campaign selection. Older
  content remains valid when it does not use newer contracts.
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
- Package capability contracts are validated at registration and preflighted
  against that combined runtime view before configuration callbacks. Missing
  features produce structured diagnostics and a fault record without exposing
  campaign flags or application types to package code. `application.*`,
  `engine.*`, and `host.*` are host-owned provider namespaces: packages may
  require those capabilities but cannot contribute them and impersonate the
  application environment.
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
  clock, and network ordering. Forced presentation from loading, progress,
  splash, fade, and legacy UI paths reaches the same bound presenter through
  source-compatible entry points. Headless hosts use the same driver and
  gateway with injected time and presentation services instead of a window or
  renderer.
- `FrameTelemetry` records bounded, value-only phase timings and input/package
  failure totals for every completed live frame. Recording is best effort and
  cannot fail a frame, while tools and headless hosts can copy stable snapshots
  without reaching into renderer or application state.
- `InputDispatcher` drains a bounded engine input stream before each frame and
  fans events out in deterministic subscriber order. The SDL adapter mirrors
  accepted legacy queue events, so engine packages receive live input without
  consuming or reordering the tactical/UI event queue. Runtime-started packages
  receive these events in activation order; callback exceptions are isolated.
- `RuntimeUpdateDispatcher` follows input delivery with a deterministic update
  hook for runtime-started packages. Each update carries engine frame identity,
  monotonic start time, and elapsed time since the previous simulation-advanced
  frame attempt; failed presentation/application completion cannot reuse that
  identity or replay its already-committed elapsed simulation interval;
  package failures are contained before the legacy application state runs.
- Package runtime health is retained in value-only catalog snapshots. Input
  and update callback counts and failures remain observable per package, while
  repeated exception logging is reduced from every frame to a logarithmic
  cadence so one broken extension cannot create an unbounded logging workload.
- `RuntimeMessageBus` provides bounded, deterministic value messages between
  hosts and runtime-started packages without campaign headers. Each frame
  drains one snapshot before input; messages published by a callback wait for
  the next frame, preventing reentrant and unbounded same-frame work.
  Packages may declare exact portable topic subscriptions; an empty declaration
  retains broadcast compatibility, while filtered traffic is counted without
  entering package code.
- `ServiceCatalog` is the versioned, type-checked extension point for optional
  host services that do not belong in the fixed platform adapter table. The
  live host publishes persistence, frame telemetry, and runtime messaging; the
  catalog seals before package bootstrap so package-held service references
  remain valid for the complete runtime session.
- `RuntimeConfiguration` is a typed, insertion-ordered startup property store.
  Applications can add or override same-typed values during composition; it
  seals with the service catalog before bootstrap and gives packages stable
  access without coupling Core to INI parsing or campaign option globals.
- `PackageStorage` maps portable record keys into per-package persistence
  namespaces and exposes only bounded checksummed envelopes. The live registry
  binds the active package identity for every lifecycle, input, update, and
  message callback, preventing new package code from constructing another
  package's record path through this API.
- `PackageMessagePublisher` binds outbound runtime messages to the registered
  package identity. Packages choose a topic and bounded value payload, while
  the host supplies the immutable source used by diagnostics and consumers.
- `PackageIdentity` is an opaque, copyable capability issued by the registry
  for every callback. Package-aware adapter services can bind a caller without
  accepting a forgeable package-ID string; retained identities do not bypass
  the active-package lifecycle check.
- `TacticalCommandClient` is the first adapter service bound with that
  identity. New packages submit commands without supplying an owner string;
  the raw service entry point remains temporarily source-compatible.
- Package descriptors declare minimum versions of required extension services.
  The registry validates the declarations at registration and checks the
  sealed host catalog before any configure callback, retaining the package,
  service, required version, and available version when preflight fails.
- `PackageRandomSource` gives each registered package bounded named streams
  derived from a host seed. Stream state is independent of activation and call
  order elsewhere, making replay, tests, and subsystem evolution less fragile
  without changing the legacy game's established random source.
- `SimulationTickDispatcher` converts monotonic frame elapsed time into a
  fixed-step package callback stream. Catch-up per frame is bounded, discarded
  ticks are explicit telemetry, and render-paced updates remain available for
  presentation work. Its ordered insertion point lets application-owned
  simulation adapters commit domain state before package observers without a
  global priority registry.
- `AssetSource::metadata` resolves normalized logical path, winning provenance,
  and byte size without copying payloads. Memory, overlay, and legacy VFS
  adapters implement the fast path; unsupported third-party sources report
  that explicitly rather than silently performing an expensive read.
- `CachingAssetSource` is the live package registry's bounded read-through
  asset view. Normalized hits avoid VFS/archive work, least-recently-used
  payloads are evicted by entry and byte budgets, allocation failures degrade
  to uncached reads, and every package mount change invalidates stale overlays.
- `RuntimeDiagnosticsSnapshot` captures lifecycle, frame telemetry, package
  catalog and health, cache statistics, services, sealed configuration,
  capabilities, and queue/tick counters as one pointer-free value. Every
  nested collection preserves an explicit deterministic order.
- `RuntimeReportService` reduces that snapshot to a privacy-conscious model,
  serializes bounded deterministic UTF-8 JSON, and writes it through the host
  persistence adapter. The JA2 application exposes opt-in startup and shutdown
  captures documented in [Runtime diagnostic reports](RUNTIME_REPORTS.md).
- `RuntimeCompatibilityFingerprint` streams a schema-tagged platform-stable
  digest over active package contracts and order, service/configuration
  contracts, combined capabilities, and versioned definition bytes. Dynamic
  frame, audio, and task state stays outside the digest so saves, replays, and
  multiplayer handshakes can compare the runtime that interprets their data.
- `RuntimeCheckpointService` persists that fingerprint with active package
  identities/versions and the completed frame/tick boundary in a bounded
  checksummed envelope. It can encode/decode in memory so the checkpoint is a
  typed section of an application-owned save transaction. Loads are
  transactional and require the exact current runtime before publishing data
  or invoking domain deserializers.
- `RuntimeSaveContainerService` owns the outer one-file save contract. It seals
  an opaque domain prefix together with bounded typed sections and a fixed
  trailer carrying exact region lengths and independent checksums. JA2 requires
  checkpoint and package-state sections and completes their strict preflight
  before dismantling live game state. This incremental boundary is documented
  in [Runtime save container](RUNTIME_SAVES.md).
- `PackageSaveArchiveService` carries bounded opaque state for packages that
  declare a non-zero per-save schema. The registry captures in activation
  order, validates every identity/version/schema before callbacks, runs a
  non-mutating validation pass, and restores only after the application-owned
  domain load succeeds. Its value model separates package-defined bytes from
  engine-owned per-package state such as deterministic random streams, without
  changing callback payload schemas. Archive v3 has one mandatory engine-record
  count and intentionally rejects older archive versions. Encoded engine
  records and streams share the aggregate package-save byte budget with opaque
  payloads; the persistence-envelope limit remains a final cap. The registry
  stages every replacement RNG map before callbacks, rolls callback draws back
  on capture or failure, and publishes state across the active set only through
  a final series of no-throw swaps.
- `RuntimeFaultJournal` records every contained package service, lifecycle,
  input, update, simulation, and message failure in a bounded sequence. It is
  separate from logarithmically rate-limited logs, so suppression reduces I/O
  without erasing failure evidence from diagnostics.
- `LocalizationCatalog` is a bounded ordered package layer for opaque UTF-8
  text. Package identity is host-bound, later packages override earlier keys,
  fallback is explicit, and configure rollback or shutdown automatically
  removes the package's entries to reveal the lower layer again. Indexed
  lookups avoid scanning every package layer, while per-entry and aggregate
  text budgets keep valid small records from accumulating without bound.
- `DefinitionCatalog` applies the same ownership and layering rules to bounded
  versioned byte definitions. Core validates identity, schema compatibility,
  per-record and aggregate payload limits, override order, and rollback
  lifetime while game/domain adapters remain responsible for decoding their
  own rules. Its override index is rebuilt safely after package removal.
- `EntityRegistry` supplies bounded generational handles without owning domain
  objects. IDs remain safe across messages, commands, saves, and diagnostics;
  destroyed slots increment generation, exhausted generations retire, and all
  identities owned by a package are removed during rollback or shutdown.
- `CampaignClockSession` is the first engine-owned strategic-state slice. The
  JA2 runtime owns total campaign seconds, the monotonic checkpoint, and the
  derived calendar as pointer-free values. The five former campaign-clock
  scalar mirrors have been deleted: `CampaignClockAdapter` binds the session,
  while the established `GetWorld*` accessors and coherent snapshots read it
  directly. Ordinary clock ticks, strategic-event warps, initialization, and
  save restoration all pass through that gateway. Existing saves retain their
  primitive field order and size, while failed loads no longer publish a
  partially read campaign-time identity. This is simulation time, not the
  injectable platform monotonic clock used for frame pacing.
- `CampaignClockScheduler` is the engine-owned fractional pacing state above
  that session. It deterministically converts fixed 16,667-microsecond ticks
  into JA2's established speed/resolution slices, bounds a malformed host tick
  to one real second, and resets rather than catching up across pauses, fades,
  unsupported screens, or turn-based combat. `CampaignSimulationHost` is
  registered before the package registry in the production composition root,
  so strategic events commit through the existing authoritative path before
  package `simulate` callbacks observe the same tick. `UpdateClock` no longer
  samples a platform clock or advances simulation; its retained compatibility
  entry point only maintains the clock UI.
- `CampaignClockService` exposes that state to packages and tools as a
  versioned, read-only, pointer-free capture. The application registers the
  provider owned by `EngineRuntime`; consumers can observe committed time or an
  in-progress event slice without gaining mutation authority or retaining a
  reference into the session. Memory and null providers keep package, replay,
  and headless tests on the same contract.
- `TacticalWorldSession` owns loaded sector identity, world generation,
  turn serial, turn-based/combat mode, and current team as one runtime value.
  World loading, tactical combat transitions, team turns, multiplayer turn
  messages, editor mode, and save restoration pass through the application
  adapter. The duplicate world-generation scalar has been deleted and lifecycle
  consumers read the session snapshot directly; `gTacticalStatus` retains exact
  readable turn mirrors for the old game. `TacticalWorldService` captures these
  values without consulting split mutable turn globals.
- `TacticalEntityDirectory` owns the bounded slot/incarnation identity used by
  commands, observations, and stale-reference rejection while JA2 retains its
  fixed `SOLDIERTYPE` storage. The host adopts, releases, and swaps pool entries
  atomically with that directory. Its former exported incarnation counter has
  been deleted; pre-composition allocations transfer the fallback directory's
  sequence directly when `EngineRuntime` is bound.
- `CampaignEventQueue` moves strategic-event allocation, ordering, stable
  identity, capacity, replacement, and destruction into `EngineRuntime`.
  Equal timestamps remain FIFO and legacy `STRATEGICEVENT` callers retain
  stable node addresses. The former `gpEventList` compatibility mirror has
  been deleted; established traversal code queries the runtime-owned head when
  it begins, so queue mutation cannot leave a duplicate pointer stale.
  Scheduling, deletion, reposting, processing traversal, and save restoration
  all mutate the runtime-owned queue. Its EVQ2 save
  section is bounded and loaded transactionally, so malformed or incomplete
  input cannot erase the active campaign.
- `CampaignEventService` projects that owned queue into bounded, pointer-free
  snapshots for packages and tools. Numeric type/callback values remain opaque
  for mods, and capture failures preserve the consumer's last complete view.
  Packages receive observation authority only; the node bridge is a host-side
  compatibility surface rather than a package ABI.
- `AudioGroupService` binds new audio playback to package identity and logical
  groups above the existing platform output. Assets are normalized, playbacks
  are bounded and inspectable, packages cannot control another owner's sounds,
  and rollback or shutdown stops everything they still own. Legacy JA2 callers
  retain their `Sound*` signatures and handle semantics through a bounded
  gateway into the same `AudioOutput`; raw mixer handles cannot escape the
  legacy platform adapter.
- `PackageTaskQueue` is the live bounded main-thread deferral path for package
  callbacks. Each frame drains only work that was already queued, exceptions
  become fault records, recursive scheduling waits for a later frame, and
  rollback or deactivation cancels callbacks before package state is released.
- `PackageResourceUsageSnapshot` joins the engine's ownership records into one
  deterministic per-package view: localization and definition counts/bytes,
  entity identities, audio playback, deferred work, and random-stream use. It
  also reports totals and any invariant-breaking unattributed record, giving
  tooling evidence for future per-package quotas and legacy-code retirement.
  A full diagnostic capture now takes each owned snapshot once and reuses it
  for resource accounting and the compatibility fingerprint; package,
  dependency, resource, and fingerprint joins use indexed identities.
- `PackageRandomSource` exposes a versioned, value-only checkpoint of its
  package identity and every named stream's generator state/counter. Restore
  validates identity, bounds, and uniqueness before an allocation-safe swap;
  exhausted counters fail explicitly instead of wrapping deterministic history.
- `PackageLifecycle` advances package configuration, content loading, and
  runtime startup as one engine-owned transaction. JA2 retains its established
  loading boundaries, while a failed later phase now unwinds every earlier
  phase automatically. Initialization cancellation uses the same structured
  reverse-order rollback without deactivating packages, so a corrected startup
  can retry the active package set. Final shutdown rolls back once before
  package deactivation and reports callback failures separately.
- `RuntimeSession` owns the application lifecycle state and is the live gateway
  for package bootstrap and shutdown. Established JA2 loading boundaries still
  advance phases at the same points. Running requires all three bootstrap
  phases, Stopped requires a successful package shutdown, and cancellation
  cannot discard completed phases. Existing boolean transition methods remain
  compatibility wrappers over structured `try*` results.
- typed resource owners bridge numeric SGP registries while platform services
  are extracted.
- soldier component views split behavior domains without moving serialized
  `SOLDIERTYPE` fields prematurely.

## Compatibility policy

Architecture migration must preserve existing campaign content and mod behavior
deliberately, not accidentally. Pre-release save bytes are not a compatibility
contract; incompatible state changes still fail explicitly rather than being
misread.

1. Save formats carry explicit versions. A supported migration must be tested;
   an intentionally unsupported older version is rejected before
   format-dependent state is read.
2. Legacy globals and numeric handles remain adapters only until all supported
   callers have a replacement; retired mirrors are guarded against returning.
3. Runtime capability defaults match the old build target until a unified
   executable can select packages at startup.
4. Serialized entities use semantic field schemas rather than making in-memory
   `SOLDIERTYPE` layout part of the persistence contract.
5. Content API major versions signal breaking contracts. A package may require
   an engine minor version no newer than the running engine supports. Exact
   package requirements compare their opaque version strings byte-for-byte.
6. Deterministic simulation code cannot read wall-clock or render timing as an
   input to rules decisions.
7. Portable identifiers, version labels, and logical asset paths have shared
   byte ceilings so accepted runtime metadata remains archive-compatible and
   cannot bypass payload memory budgets.

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
