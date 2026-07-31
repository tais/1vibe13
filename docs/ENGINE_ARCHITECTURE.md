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
  A package is an activation, ownership, and mount unit, not a replacement
  game-data format. Existing `Data-*` layouts, VFS profiles, archives, XML,
  maps, artwork, audio, dialogue, and Lua remain authoritative and require no
  conversion; package metadata only selects and layers them.
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
  Production tactical input routes end-turn, stance, movement, facing, firing,
  stealth, stop-movement, weapon-mode, scope-mode, single-merc reload, and
  typed roof/fence/wall/window traversal intent through engine-owned value
  commands. Door, switch, and openable-structure activation uses the same
  boundary; approaching an object combines movement and pending interaction in
  one command. Player conversation, shopkeeper approach, and vehicle entry also
  carry stable actor and target incarnations. Specific player pickup carries a
  stable world-item slot/incarnation, while grid-search pickup is represented
  explicitly without inventing an item identity. Player stealing captures the
  target incarnation, grid, and floor through both its approach and animation
  event; position exchange captures both actors and positions and applies the
  swap plus AP cost as one operation. The temporary TacticalActor pending-action
  bridge preserves these identities until movement completes, so a reused pool
  slot, moved actor or vehicle, or replaced world item cannot redirect the
  original intent. Each command is processed at the existing synchronous
  boundary before invoking its legacy executor.
- `Ja2TacticalEntityReference` is the pointer-free compatibility handle for
  delayed legacy callbacks that have not yet become simulation commands. Its
  capture API accepts only a complete `TacticalEntityId`; UI and strategic
  producers must verify a canonical soldier record before any identity becomes
  retained state. Dialogue, contracts, traversal, tactical placement, militia
  training, bomb setup, corpse actions, switch confirmation, tactical utility
  selection, delayed ownership checks, and friendly-fire/surgery confirmation
  now keep separate actor-incarnation contexts tied to the loaded world
  generation where applicable. Raw records are resolved only when compatibility
  work resumes. A released/reused actor, changed hand item, or world transition
  therefore cancels the stale callback instead of dereferencing or retargeting
  a global `TacticalActor*`. Architecture checks reject pointer capture APIs from
  returning. These references are runtime-only and do not alter soldier, save,
  map, or content layouts.
- Multi-frame tactical UI sessions follow the same rule. Planning mode,
  vehicle-seat popups, militia command menus, and the follow-up radio cursor
  accept exact actor identities and resolve them only while handling the
  current frame or callback. Closing a menu clears its context, while a
  released or slot-replaced actor closes or cancels the stale interaction
  instead of operating on its replacement. The pathfinding backpack cache also
  keys its search-scoped value by slot plus incarnation rather than retaining
  the search actor pointer.
- Strategic map UI sessions use the same exact-identity boundary. Movement-box
  rows retain actor incarnations for the lifetime of the modal and close the
  snapshot if any row becomes stale, preserving the row-to-mouse-region
  mapping instead of silently compacting it onto another merc. Assignment
  update rows may compact safely because they are not callback-indexed; stale
  rows release their face resources, while the dialogue queue carries both the
  actor slot and incarnation to the consumer. These contexts remain
  runtime-only and do not alter save, map, XML, Lua, package, or installed-data
  formats.
- Debug and synthetic actors no longer bypass that ownership model. Quest
  dialogue and the animation viewer retain exact live identities. The air-raid
  attacker remains the established fixed compatibility record but stores only
  its bounded slot and resolves the record at each use; its already-existing
  save fields now restore an in-progress raid without a null actor. Helicopter
  dialogue no longer creates or passes a fake Skyrider `TacticalActor` at all,
  because the dialogue implementation consumes only the profile and quote.
  None of these changes modifies map, XML, Lua, package, or installed-data
  formats.
- `Ja2TacticalWorldItemReference` provides the equivalent runtime-only handle
  for entries in the reusable `gWorldItems` storage. Booby-trap, buried-bomb,
  map-cursor trap, and mine-spotted dialogue chains now capture independent
  actor/item/location contexts. A rebuilt item pool, reused world-item slot,
  changed cursor item, released actor, or sector transition cancels the stale
  action. The extended inspect path also resolves and snapshots its selected
  bomb before evaluating it; no save, map, XML, archive, or content format is
  changed.
- The production flashing-item locator table is private and stores the same
  stable world-item identity plus grid, level, and world generation instead of
  an `ITEM_POOL*`. Both update and render paths reacquire the live pool; stale
  entries retire safely and run their completion callback at most once. The
  pointer-bearing `ITEM_POOL_LOCATOR` definition remains available only for
  legacy source compatibility and is not instantiated by the runtime.
- Pending NPC conversations, end-game death timers, insurance, dismissal, and
  automatic-surgery confirmations retain exact tactical-entity incarnations
  instead of reusable `TacticalActor*` slots. A removed or replaced actor now
  cancels the actor-specific work safely; end-game progression still continues
  without attributing the kill to a different slot occupant. Surgery prompts
  also clear both participants on every answer rather than leaving rejected
  raw pointers available to the next prompt. Insurance prompts snapshot their
  requested duration instead of later rereading the shared AIM hiring-screen
  selection.
- Merc departure-equipment prompts consume an exact actor incarnation once,
  and stale prompts release their pause ownership instead of stranding the
  game. Contract rehire state uses the same stable reference across the
  tactical-to-map transition. Save games still store and load the established
  soldier-slot field byte-for-byte; loading validates that slot through the
  live entity directory before the UI can use it, while unsaved prompt
  contexts are cleared before restoration.
- Active and modal dialogue sessions also retain exact source and destination
  incarnations. Dialogue UI actions, message-box callbacks, quest facts, and
  the quest-debug panel resolve participants through one stable boundary; a
  released or replaced slot therefore cancels actor-specific work instead of
  redirecting it to the new occupant. Dialogue-independent fact checks do not
  pay for those resolutions.
- Facility staffing and militia-training confirmations retain exact actor
  incarnations in private, one-shot prompt contexts. Militia prompts also
  snapshot their start/continue mode, sector, quoted total, and promotion-cost
  multiplier, so a later callback cannot be redirected by shared globals and
  every confirmed sector is charged at the displayed rate. These unsaved modal
  contexts are discarded at the load boundary; game-data and save layouts are
  unchanged.
- `StrategicGroupDirectory` gives the legacy one-byte movement-group registry
  runtime-owned liveness and incarnation. Creation, deletion, whole-list
  teardown, and save restoration publish through the directory without adding
  fields to `GROUP` or changing save bytes. Prebattle/autoresolve, tactical
  traversal, adjacent-sector movement, simultaneous-arrival, and wilderness
  prompts retain `StrategicGroupId` values rather than linked-list pointers.
  A deleted and recreated group with the same compatibility ID cannot inherit
  pending UI or transition work.
- Tactical traversal retains both its group and chosen speaker as exact
  incarnations across sector loading and screen fades. Both identities are
  resolved only when needed after the destination sector is live, so deleted
  groups and released or reused actor slots fail closed. Completion, alternate
  warps, end-game transitions, and save loading discard the unsaved context.
- Tactical placement stores exact actor identities for every deployable merc,
  including its selected and highlighted render state. Each frame validates
  the complete placement roster before UI callbacks or rendering; a released
  or reused actor slot closes the modal safely instead of dereferencing or
  highlighting its replacement. Finishing, cancelling, re-entering, and save
  loading clear the runtime-only selection contexts.
- `ProcessCommandsThrough` snapshots one bounded ready set and acknowledges
  commands only after their handler returns. Applied commands run exactly once;
  retry blocks later deterministic work without removing it; explicit discard
  is counted; and a handler exception leaves the failing and remaining commands
  queued. Commands produced during a handler wait for the next pass, preventing
  accidental unbounded same-tick dispatch.
- `CommandJournal` records a bounded, best-effort history of submitted command
  values and their queued/applied/discarded/blocked state. Recording failures
  never alter simulation delivery. The JA2 adapter's
  `SimulationCommandCodec` serializes explicit command tags rather than variant
  indexes, providing a capture boundary for diagnostics and future
  replay/network hosts. It deliberately has one current layout: the header
  reserves a version field, but no speculative historical decoders are carried
  before a format has actually shipped.
  `SimulationCommandExecutor` now separates that deterministic stream from its
  world implementation. The production compatibility executor implements the
  same installed interface used by tools and headless tests; queue processing
  is owned by `EngineRuntime` rather than by the game application. The
  composition root binds one executor for the runtime lifetime; an unbound
  runtime retries without acknowledging work and a second world binding is
  rejected. Bounded prefix drains and exact synchronous drains both invoke the
  bound executor, acknowledge its disposition, update the command journal, and
  only then notify a best-effort execution sink. Production, package ingress,
  replay, external SDK consumers, and headless scenarios therefore cannot
  carry parallel queue/journal loops. The boundary is non-reentrant: an
  executor or sink that attempts a nested drain receives `QueueChanged`, while
  the outer acknowledgement and observation complete normally.
  `MemoryTacticalSimulation` is the bounded, pointer-free reference
  implementation. It transactionally validates and canonicalizes actor
  incarnations, preallocates its configured state ceilings, and applies the
  portable movement, stance, facing, fire, synchronization, stop, and turn
  subset without linking the game or SDL. It deliberately discards unsupported
  mechanics instead of duplicating JA2 combat policy.
  Firearm actions, player weapon mode, scope, reload, and ready/lower controls,
  stance changes, drag cancellation, obstacle traversal, world-object
  interaction, conversation, vehicle entry, player stealing and position
  exchange, and player world-item pickup enter this gateway before the
  compatibility executor queues events or invokes the established inventory,
  AP, pathing, structure, vehicle, dialogue, and animation mechanics. Traversal
  and interaction availability, backpack, and AP checks remain at their
  existing player-input sites while the command records only the chosen action.
  The application command API is now pointer-free at every producer:
  local UI, AI, dialogue, and network receive sites capture a complete
  `TacticalEntityId` from the exact live `TacticalActor` through
  `TacticalEntityHost` before dispatch. The public header contains neither a
  soldier record nor split slot/incarnation overloads. A detached object
  therefore captures an invalid identity and is rejected without consuming a
  command sequence or frame budget. Only the compatibility executor resolves
  command identities back to live records. Delayed actions that still store
  pending state on `TacticalActor` are isolated in
  `Simulation Command Legacy.h`; they are completion seams, not command
  producer APIs. Replay and package ingress submit the same public values
  directly.
  The stance executor also owns the established real-time moving-animation
  transition; UI code no longer rewrites movement mode, desired height, and
  animation bookkeeping after deciding on a stance.
  Single-merc, multi-selection, panel, and current-squad stealth controls all
  submit the same `SetStealthModeCommand`; current-squad input has one shared
  implementation in both real-time and turn-based modes. Stopping a
  rubber-band selection likewise submits one `StopMovementCommand` per exact
  live actor instead of duplicating its movement-state mutations in UI code.
  Multiplayer receive handlers now capture the resolved actor incarnation and
  enter this same authoritative stream. Stance and desired-facing packets use
  the existing semantic commands without echoing outbound replication.
  Authoritative path, fire, stop, and turn packets use explicit synchronization
  commands because they carry remote state snapshots rather than local
  pathfinding intent. Their established RakNet packet bytes are unchanged.
  When synchronous execution is backpressured or the frame budget is spent, a
  validated reliable packet is retained in sequence for the safe-frame drain
  instead of being silently lost.
  AI locomotion, stance, facing, and the final selected-weapon fire event now
  use retained `System` ingress as well. A busy authoritative frame therefore
  delays an AI action the state machine already considers started instead of
  dropping it. Movement explicitly preserves the stored AI path, System path
  origin, pending action, reverse state, and restart decision. Fire captures
  the chosen hand and weapon before deferral and delays multiplayer emission
  with the local action. Dialogue-driven movement reaches this boundary through
  the same AI executor; scripted stance and facing actions also record whether
  their established behavior was local-only or multiplayer-aware. This event
  policy is independent from provenance, so a received command cannot
  accidentally echo itself and replay retains the original behavior.
  Equipment-driven mode correction, automatic/pathfinding door handling,
  low-level path traversal, non-positional dialogue effects, and multi-merc
  bulk reload remain local mechanics until they receive explicit command
  semantics rather than masquerading as player intent.
- The JA2 adapter's `CommandReplayService` stores those journals in
  integrity-checked runtime
  persistence envelopes. Replay loads are transactional, incomplete bounded
  captures are refused, and whole batches are staged atomically so duplicate
  sequence IDs cannot leave a partially mutated simulation queue. Playback
  retains the recorded tick, sequence, value, and source through the same
  runtime gateway used by live commands. The data-free headless suite now runs
  the installed `MemoryTacticalSimulation`, rather than a parallel test-local
  battle model, for a mixed player, AI/script, and network tactical turn through
  `EngineRuntime`'s bounded command drain, including an authoritative retry. It
  encodes the completed journal, decodes and stages it into a fresh runtime,
  resets the same actor/world snapshot, and requires identical disposition
  observations, applied order, final state, and journal bytes. This exercises
  replay determinism without SDL presentation, audio, installed maps, or game
  data.
  The architecture check also pins each migrated multiplayer receive handler
  and AI/dialogue producer to its command ingress function and rejects the
  corresponding legacy event/path/animation calls outside the dedicated
  executor.
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
  it, including message/chat overlays. The duplicate current-, pending-, and
  previous-screen scalars have been deleted and callers query the controller
  through `GetCurrentScreen`, `GetPendingNewScreen`, and `GetPreviousScreen`.
  Four legacy operations that historically changed the scalar temporarily now
  use scoped controller-owned overrides which leave the transition/overlay
  stack untouched and do not record false navigation history. The composition
  root publishes its stable non-owning context address once, so these hot-path
  queries do not replay adapter/package registration guards.
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
  turn serial, turn-based/combat mode, current team, and the pending
  asynchronous combat-action count as one runtime value.
  World loading, tactical combat transitions, team turns, multiplayer turn
  messages, editor mode, and save restoration pass through the application
  adapter. The duplicate world-loaded and generation scalars have been deleted;
  lifecycle consumers query the session directly through
  `IsJa2TacticalWorldLoaded` or `CaptureJa2TacticalWorld`. Turn-based mode,
  combat mode, current team, and pending combat work are also read directly
  from the session; their former `gTacticalStatus` fields and flag bits are no
  longer live mirrors. Save/load and the editor explicitly compose and restore
  the established flag/team/pending byte sequence at their compatibility
  boundaries. Bullets, explosions, physics, animations, air raids, recovery
  paths, and world teardown begin, complete, or reset work through the
  application adapter. The session rejects overflow and underflow rather than
  wrapping into a false idle state; persistence alone clamps the wider count
  when more than 255 effects overlap. The sector-heavy
  compatibility names `gWorldSectorX`, `gWorldSectorY`, and `gbWorldSectorZ`
  are const-reference projections backed only by the application adapter:
  legacy reads retain their allocation-free lvalue path, while the compiler
  and architecture ratchet reject any second writer or mismatched declaration.
  `TacticalWorldService` captures these values without consulting split mutable
  turn globals.
- `TacticalEntityDirectory` owns the bounded slot/incarnation identity used by
  commands, observations, and stale-reference rejection, plus the latest
  committed pointer-free `TacticalActorSnapshot`, while JA2 retains its fixed
  `TacticalActor` compatibility storage. The host adopts, releases, and swaps pool
  entries atomically with both identity and state; reuse or release retires the
  old projection in the same operation. Production command execution commits
  the resulting primary and peer actor projections before returning, and the
  completed-frame capture reconciles animation, vitals, and other remaining
  legacy mutations once before observation. `TacticalWorldAdapter` consequently
  reads only the runtime directory—it no longer imports soldier storage or
  animation tables as a parallel package-facing state path. The former exported
  incarnation counter has been deleted; pre-composition allocations transfer
  the fallback directory's sequence directly when `EngineRuntime` is bound.
- `TacticalEntityRoster` provides fixed-capacity, pointer-free ordered
  membership above that directory. Inserts and exact-slot reconstruction reuse
  preallocated storage; erasure, stable compaction, and identity sorting also
  perform no hot-path allocation. The JA2 tactical host owns separate active
  and away scheduler rosters, while `StrategicSquadHost` owns the 40 bounded
  strategic squad rosters and `VehiclePassengerHost` owns ten fixed passenger
  seats plus an exact driver per vehicle. Strategic movement-group member
  nodes likewise retain an exact tactical identity beside their
  persistence-only profile value. Readers resolve one exact slot/incarnation
  only at the point of use, and stale entries fail closed.
  Whole-record portrait swaps rebuild directory identities and rebind every
  scheduler, squad, passenger, driver, and movement-group reference by
  canonical repository slot in the same operation, preserving the established
  fixed-address behavior without retaining `TacticalActor*`. Deletion captures
  identity before release and removes that exact actor even after directory
  resolution rejects it. The former `MercSlots`, `AwaySlots`, mutable
  high-water globals, global `Squad[][]` pointer matrix,
  `VEHICLETYPE::pPassengers` matrix, and `PLAYERGROUP` soldier pointer/slot
  pair are deleted, and architecture checks prevent their return. Squad
  save/load still emits and consumes the established 40-by-10 legacy
  soldier-ID block. Vehicle save/load likewise retains ten 32-bit passenger
  profile IDs and its 16-bit driver slot. Strategic movement groups retain
  their count plus 32-bit profile-only member payload. Both loaders reconstruct
  exact runtime identities without casting numeric data through pointers.
  Soldier/map content, saves, Lua, and network formats are unchanged.
- `Ja2SoldierRepository` is the application-owned live-storage seam paired with
  that directory. `GameContext` owns the repository, and its fixed record
  allocation and slot table are private implementation storage in
  `SoldierRepository.cpp`; the former process-global declarations and
  `Overhead.cpp` definitions no longer exist. Repository binding is independent
  from the tactical-entity directory binding, so legacy domains can depend on
  the narrower storage adapter without importing runtime entity-host concerns.
  Its bound-repository gateway is an inline pointer load, and bounded
  resolution is inline as well, so making lookups explicit does not introduce
  a function call in hot application UI or simulation paths.
  Soldier
  creation, save/load, entity adoption/release, completed-state publication,
  and whole-record swaps now resolve or mutate records through this boundary.
  The repository validates slot bounds and canonical record bindings before
  replacement or relocation. Strategic simulation, TacticalAI, TileEngine, and
  the outer application domains—Ja2 composition/save handling, Laptop, Utils,
  Editor, the Lua bridge, and Multiplayer—cannot name the backing storage
  directly. Former raw-array and character-list pointer walks resolve each
  numeric slot independently and no longer assume contiguous `TacticalActor`
  memory. Every implicit `SoldierID`-to-pointer conversion has been removed
  from the value type itself, so lookups must name the repository explicitly
  in every target without a transitional compile definition. This includes
  input and UI, combat, animation, items, soldier lifecycle and creation,
  roster handling, morale and food, projectiles and weapons, shopkeepers, save
  traversal, vehicles, sector entry, and the Unfinished Business tactical
  rules. Architecture checks cover production sources and the headless
  harness, rejecting retired global names and contiguous soldier-pointer
  walks. The fixed capacity, numeric slots, save byte sequence, map records,
  Lua values, network packets, and mod data remain unchanged.
- `TacticalInventoryUiSession` owns the actor identities retained by the
  selected-merc panel, item cursor, item description and attachment view,
  stack/keyring popup, and pickup/stealing menu. Its stable application host is
  a pointer-free producer boundary: callers capture a complete
  `TacticalEntityId`, copy identities directly between UI roles, and explicitly
  clear absent actors. Raw `TacticalActor` resolution is isolated in
  `TacticalInventoryUiLegacy.h` at compatibility consumption sites. Each
  resolution passes through `TacticalEntityDirectory`; if an actor is released
  or its pool slot is reused, the modal closes instead of following the
  replacement soldier or dereferencing stale inventory. Panel, cursor, world,
  and load teardown clear these runtime-only roles. Architecture checks prevent
  pointer setters or raw getters from returning to the stable host. No soldier,
  inventory, map, save, or content representation changes.
- `TacticalWorldItemDirectory` gives reusable `gWorldItems` slots the same
  bounded incarnation protection without moving or reformatting game data.
  Storage grows only through an activated slot and is capped before allocation.
  Add/remove, Lua existence changes, and whole-vector strategic inventory
  replacement retire or rebuild liveness through the runtime-owned directory.
  `WORLDITEM` retains only a runtime mirror outside its serialized POD/object
  payload, so map, save, temp-item, asset, and command-journal versions do not
  change.
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
- JA25 tactical, strategic-AI, and update modules are compiled into every game
  host as the first campaign-identity extraction slice. Shared quest and fact
  vocabularies retain their established numeric slots through campaign-named
  aliases, so this does not rewrite maps, XML, Lua, or other game data. Campaign
  startup, UB option loading, multiplayer intro policy, and UB cinematic-name
  registration now select behavior from `GameCapabilities`; the separate JA2,
  UB, and editor executables still provide the same defaults while remaining
  preprocessor branches are migrated incrementally. The architecture check
  prevents the dedicated modules and migrated composition callers from
  regaining a `JA2UB` build guard.
- Strategic-event dispatch is compiled identically for both campaign hosts and
  selects Arulco-only and Unfinished Business-only callbacks from
  `GameCapabilities`. Delayed JA25 quotes, sector notifications, the shared
  endgame-quote entry point, and the Arulco MERC-site recovery callback are
  emitted in every host so runtime dispatch never depends on a missing symbol.
  Campaign-qualified email and Speck quote constants are compile-time checked
  against the existing Arulco offsets; the campaign data files and their
  numeric records remain unchanged.
- NPC dialogue and strategic-AI action records are decoded against the active
  runtime campaign. This deliberately preserves the existing collision where
  raw action 301 means Waldo's repair request in Arulco and Jerry's first
  conversation in Unfinished Business, as well as the 299/300 versus 311/312
  strategic-assault ranges. The application exposes one campaign-neutral C++
  action surface while accepting both campaigns' unchanged script data.
- Both campaign endgame state machines are emitted in every game host. Tactical
  death callbacks, queued dialogue completion, helicopter-crash recovery, and
  laptop-to-credits transitions select the active flow from `GameCapabilities`.
  Campaign-qualified event bits and email offsets preserve the existing data
  records. The JA25 soldier state used by these flows now has one in-memory and
  serialized layout in every host; this intentionally retires the pre-release
  host-specific save layout rather than carrying a second compiled identity.
- The tactical overhead loop has no compiled campaign identity. NPC and player
  death consequences, power-generator and fan hooks, first-battle and POW
  outcomes, meanwhile combat handling, and capture policy branch on the active
  runtime campaign. A typed profile resolver preserves the established 57-65
  Miguel-through-Slay slot shift, and qualified playable-Speck quote IDs keep
  Arulco speech records unchanged when that callback is emitted by a UB host.
- Tactical dialogue queues, sighting reactions, speech-file selection, Jerry
  and Morris map-screen quotes, and the UB intro exit now select their flow from
  the active runtime campaign. Jerry/Morris and Mike/militia implementations are
  emitted together, while compile-time-checked aliases preserve their shared
  legacy event and soldier-quote bits. Quote IDs, speech paths, profile slots,
  and campaign data formats remain unchanged.
- Map-temp changes are decoded and encoded against the active runtime campaign.
  This preserves the legacy collision where raw type 22 means an Arulco mine
  flag but a UB exit-grid removal, including each campaign's shifted mine and
  decal records. UB mine-collapse, fan, tunnel, fortified-door, and scripted
  explosion hooks are emitted in every host and selected through
  `GameCapabilities`; existing map-temp bytes and content files are unchanged.
- Strategic sector state now has one campaign-capable representation in every
  host, including UB terrain IDs, surface/custom/campaign metadata, and the
  underground-sector serializer. Sector entry, exit, arrival, map
  modification, player-quote, email, guide-description, POW, and daily-event
  paths select Arulco or UB behavior from `GameCapabilities`; no map, XML, Lua,
  dialogue, or other game-data format changed. This deliberately unifies the
  pre-release raw sector save layout instead of adding another compatibility
  version. UB tunnel placement is bounded by its actual destination arrays,
  and complex-map fallback placement makes one finite pass over distinct
  enemies, eliminating two legacy out-of-bounds/hang paths.
- Save/load and strategic bootstrap now follow that same runtime campaign
  identity. Every host writes and reads the JA25 strategic and tactical
  sections plus one common `GENERAL_SAVE_INFO` layout; only the active
  campaign applies UB-specific values after decoding. This intentionally
  replaces the pre-release host-specific stream rather than adding another
  compatibility version. The loader now also restores five UB options that
  were previously written but silently discarded: the laptop quest, Tex/John,
  random Manuel text, initial-sector attack, and tunnel-enemy switches.
  Strategic startup, new-campaign initialization, meanwhile temp-sector
  handling, custom-map validation, built-in UB movement costs, and Lua/Jerry/
  helicopter hooks select from `GameCapabilities`. Their implementations are
  linked into JA2, UB, and editor hosts. Existing maps, XML, Lua, dialogue,
  artwork, archives, and package overlays remain unchanged.
- Strategic ownership and group-movement rules now use the selected campaign
  rather than the executable that compiled them. Hourly Slay/helicopter work,
  Bobby Ray's opening mail, loyalty and meanwhile reactions, queen/POW hooks,
  strategic-AI group bookkeeping, SAM discovery, bloodcat ambushes, and UB
  custom-sector prompts are emitted in every host and gated by
  `GameCapabilities`. The campaign-qualified Bobby Ray offset preserves the
  existing Arulco `Email.edt`; no strategic or content data format changed.
- Queen-command and quest state machines are emitted identically in every
  host. Morris and bloodcat deaths, grid-number reinforcements, capture/POW
  handling, campaign-local facts, quest rewards and startup, and UB laptop
  recovery now branch on `GameCapabilities`. Typed profile roles avoid the
  Morris/Arulco-queen and shifted rebel-slot collisions, while compile-time
  checked email and sender IDs preserve both campaigns' existing `Email.edt`
  records. Quest numbers, maps, XML, Lua, dialogue, archives, and other content
  formats remain unchanged.
- The laptop composition/router is likewise campaign-neutral. Startup and
  shutdown, Bobby Ray and insurance availability, MERC account-page dispatch,
  UB custom-map handoff, bookmarks, broken-web routing, and page loading now
  use runtime campaign capabilities and established UB options. The page
  implementations and all laptop text, email, image, and XML data retain their
  existing formats and paths.
- typed resource owners bridge numeric SGP registries while platform services
  are extracted.

  The component notes below record the sequence of the storage migration.
  References to v101 conversion describe the retired converter's final mapping
  for historical review; baseline 1003 deleted that record and converter.
  Current saves use only explicit component visitors.

- soldier components now own the first real storage cut: pending-action
  scratch, combat feedback, and quick-item retention live in one resettable
  runtime aggregate instead of unrelated flat `TacticalActor` tail fields.
  Soldier clones start with an empty runtime aggregate, so deferred callbacks
  cannot retain and later mutate the source soldier.
  `SoldierVitalsComponent` now privately owns the complete persistent health,
  breath, wound, and recovery lifecycle: current/max values, previous-turn and
  fractional health, breath reduction, treatable trauma and surgery state,
  unrecoverable breath, critical-stat damage, bleed scheduling and sound
  throttling, plus the retired regeneration save slots. Named snapshot,
  surgery, critical-damage cleanup, life-deduction, and reset operations
  establish one lifecycle boundary. Zero-cost reference accessors preserve
  hot-path mutation semantics, while the portable field serializer emits every
  value at its established byte position and width.
  `SoldierStatisticsComponent` now privately owns the eleven persistent base
  attributes and all 30 skill-trait slots formerly exposed through
  `STRUCT_Statistics`. Combat, AI, assignments, character creation, and UI
  consume one zero-cost accessor surface, and reset clears the entire fixed
  trait capacity. The visitor retains the historical signed-byte attribute
  sequence, including current/max health at their existing interleaved
  positions, followed by all 30 unsigned trait bytes. v101 conversion maps
  every historical attribute and its two trait slots, then clears the 28 later
  slots. Profiles, trait rules, XML, Lua, multiplayer, installed game data, and
  save bytes are unchanged.
  `SoldierInventory` now privately owns the live soldier's carried-object
  slots, new-item counters, key access, refresh request, zipper state, and
  drop-pack state behind one typed accessor. Its object and counter vectors
  resize and clear as one coherent unit, and whole-soldier copies receive
  independent storage. Neutral `InventorySlots` records remain available for
  creation, map, and v101 transfers; assigning one to a live inventory replaces
  only slots and counters, preserving the historically separate live flags.
  The existing inventory stream remains an `int` slot count followed by each
  `OBJECTTYPE` and its two `int` counters. Item XML, pocket layouts, profiles,
  maps, multiplayer records, Lua, packages, installed data, and save bytes are
  unchanged.
  The final `STRUCT_Flags` bucket is retired rather than becoming another
  catch-all component. `SoldierStatusComponent` privately owns only the
  established 32-bit general soldier mask, with explicit query/set/clear
  operations. `SoldierInventory` owns key access, new-item refresh, zipper, and
  drop-pack state. The remaining former fields now live
  with replication, AI planning, conditions, targeting, fire control, and
  animation activity. Zero-cost reference accessors preserve the hot mutation
  paths. The current visitor emits all fourteen values at their exact previous
  positions and widths; v101 conversion maps its twelve historical values and
  clears zipper/drop-pack state absent from that record. Save data, profiles,
  packets, maps, XML, Lua, multiplayer protocols, packages, and installed game
  data are unchanged.
  `SoldierKeyRingComponent` owns the complementary optional key-ring payload.
  Its 255 established two-byte slots are inline and copied by value, removing
  per-soldier allocation/free traffic and preventing whole-record copies or
  repository swaps from aliasing one heap buffer. An explicit presence marker
  retains the historical player/non-player distinction. The portable soldier
  visitor keeps the retired pointer landmark byte-neutral, while the existing
  outer soldier save path still writes the same presence byte followed by the
  same fixed key payload. No key table, item, save, map, XML, Lua, package, or
  installed-data format changes.
  `SoldierPendingItemComponent` now owns the process-local object used while a
  give, drop, robot reload, placement, throw, or launcher action is in flight.
  The large `OBJECTTYPE` remains uniquely heap-backed and is deep-copied with a
  whole soldier; the small `THROW_PARAMS` payload is inline and presence-tagged.
  Named preparation and cleanup transitions pair ballistic state with its
  object and release both on completion, interruption, soldier deletion, or
  failed multiplayer physics allocation. This removes the former independent
  raw-pointer allocation paths and their cancellation leaks. Both historical
  pointer positions remain byte-neutral retired landmarks, so save, map, XML,
  Lua, multiplayer packet, package, and installed-data formats are unchanged.
  `SoldierAiPlanComponent` now owns the runtime-only modular tactical-AI plan
  tree. A plan retains a back-reference to the exact soldier record that
  created it, so whole-record copies, replacements, and swaps intentionally
  discard this cache and rebuild it lazily for the destination rather than
  shallow-copying an invalid owner. Initialization, deletion, and current
  loading release any existing plan through the same boundary.
  The legacy factory's raw result is confined to the `adopt` transfer point;
  callers query and execute through the component and tolerate a factory that
  returns no plan. Plans are runtime-only and never enter
  `XferTacticalActor`, so no map, XML, Lua, multiplayer, package, or
  installed-data format changes.
  `SoldierStrategicPathComponent` now owns each soldier's doubly-linked
  strategic route. Whole-soldier copies deep-copy the route, moves transfer it,
  repository swaps exchange the original node storage without allocation, and
  initialization/deletion release it with the record. The established
  strategic path algorithms remain compatibility adapters over `PathSt`;
  vehicle and militia routes keep their existing owners. Soldier loading builds
  nodes under a temporary owner, rejects implausible counts, and commits only a
  complete route, so truncated input cannot leak nodes or replace live state.
  The former `pMercPath` visitor emitted no bytes and is retained as a
  `retiredPtr()` landmark, while the same count and `PathSt` node payload still
  follows the soldier record. Save, map, XML, Lua, multiplayer, package, and
  installed-data formats are unchanged.
  The three later, independent compatibility banks are now privately owned by
  `SoldierFeatureFlagsComponent`: the unsigned 8-bit gunshot/explosion/X-ray
  event markers and the two unsigned 32-bit feature masks introduced by 1.13
  systems. Named event, primary, and secondary query/set/clear operations make
  their bank explicit while preserving every existing flag definition and
  hot-path reference access. These banks are not a replacement for
  `STRUCT_Flags`; they retain the later extension points already consumed by
  gameplay and mods.
  `SoldierServiceComponent` owns the complementary tactical care relationship:
  the persisted service marker, the number of active providers on a patient,
  the provider's patient identity, and the medic reserved by automatic
  bandaging. It also retains the inventory slot from which an autonomous medic
  temporarily borrowed the kit now held in hand. Named begin/end, add/remove,
  reservation, borrow/return, and reset operations keep both sides coherent
  and prevent a mismatched cancellation from underflowing the provider count.
  Face rendering observes the component but retains only its presentation
  cache; every save field keeps its established position and width.
  `SoldierDialogueComponent` owns the spoken-state lifecycle: queued NPC quote
  records and actions, normal and extended said-history masks, battle-voice
  selection and active playback, repeat throttling, bleeding/dying feedback,
  queued out-of-ammo speech, death-sound gates, heard-noise cooldown, civilian
  quote progression, last-spoke time, vocal volume, and corpse-comment
  tolerance. Named history, tactical-feedback, cooldown, quote-plan, playback,
  and reset transitions replace scattered bit manipulation while all nineteen
  save fields retain their established positions and widths.
  `SoldierAudioComponent` owns the complementary non-dialogue audio lifecycle:
  footstep variation, remembered door-opening noise, and the burst,
  world-position, and turret-turning sound handles. Named start, clear, record,
  query, and reset transitions close stale-handle paths, including invalidating
  a burst handle immediately after a jam stops it. Fresh transient handles use
  the sound system's explicit no-sample sentinel, while all five serialized
  values and v101 mappings retain their established positions, widths, and raw
  values.
  `SoldierReplicationComponent` owns the transport-facing bookkeeping that was
  previously mixed into the soldier POD: movement and optimum timing, the last
  network update timestamp, update sequence and kind, a scheduled stop grid,
  and the persisted integrity checksum. Named update, timeout, stop, checksum,
  and reset transitions give multiplayer, overhead, and persistence one
  authority. Unsigned timeout arithmetic remains clock-wrap safe; the current
  serializer retains all seven original positions and widths, while v101
  conversion maps its six established values and clears the later scheduled
  stop.
  `SoldierMovementMetricsComponent` owns the carried-weight snapshot, per-turn
  distance, and realtime breath-update distance/animation telemetry consumed by
  AP, agility, visibility, accuracy, suppression, medical, and breath rules.
  One tile-recording transition counts running as the established double
  distance and updates realtime cadence when applicable. Both narrow distance
  counters now saturate instead of wrapping on exceptionally long movement,
  while independent turn and realtime clear operations preserve their existing
  lifecycles. All four save fields and v101 mappings retain their original
  positions, widths, and raw values.
  `SoldierIdentityComponent` now owns the stable slot ID, fixed display name,
  physical body type, legacy profile link, incarnation token, data-profile
  link, and individual-militia identity. `SoldierRosterComponent` separately
  owns allocation activity, team, tactical-sector presence, side, soldier
  class, and civilian group. This makes permanent identity independent from
  mutable roster membership while preserving zero-cost reference access for
  the existing game. Current and v101 serializers keep every field at its
  established position and width; map placement, multiplayer creation, profile,
  XML, Lua, package, and installed-data formats remain unchanged.
  `SoldierAiPlanningComponent` owns the tactical-AI action plan: current,
  previous, and queued actions and payloads; action progress; target elevation;
  facing intent; the fixed patrol route and cursor; aim time; flank count,
  anchor, and origin direction; sniper posture; and the modular plan index.
  Named action queuing, flank-step, completion, posture, plan-selection, and
  reset transitions give turn-based and realtime AI one authority. Flank
  progress saturates its signed byte rather than wrapping. The current
  serializer retains every original position and width, v101 conversion maps
  all historical fields, and the patrol capacity remains the established ten.
  `SoldierAiBehaviorComponent` separately owns alertness, neutrality, situation
  state, orders, attitude, escort and green-state bypass, hunting, creature
  mobility, realtime-combat mode, and AI flags.
  `SoldierAiCommunicationComponent` owns radio/call origin, location, priority,
  and acknowledgement, while `SoldierMoraleComponent` owns personal morale,
  team/tactical/strategic modifiers, the delayed strategic modifier awaiting
  the next morale refresh, AI morale, and creature frenzy. Together these
  boundaries remove `STRUCT_AIData` as a live catch-all without changing AI
  policy, plan APIs, or content. The morale owner also removes the separate
  delayed-morale field from the public soldier tail.
  `SoldierSkillStateComponent` owns the transient skill-execution lifecycle:
  repeated mechanical-check identity and attempts, the AI's selected skill,
  persistent trait counters, heterogeneous cooldowns, and the focus target.
  Named check, aging, cooldown, focus, and reset operations replace scattered
  field manipulation. The established 20-entry counter and cooldown capacities,
  every serialized position, and every visitor width remain unchanged; v101
  conversion maps its three skill-check fields and clears state that did not
  exist in that record.
  `SoldierConditionComponent` owns the complementary ongoing-effect domain:
  temporary strength, dexterity, agility, wisdom, and experience modifiers;
  food and drink levels; accumulated starvation harm; disease progress and
  flags; and the acquired-disability mask. Disease rule definitions remain
  content, while the fixed 20-slot storage capacity now lives in a small
  dependency-neutral type header so `Disease.h` no longer includes the entire
  soldier definition. Named effect, disease-flag, disability, and reset
  operations replace scattered field and bit manipulation. Disability
  insertion validates the established 1..32 domain and uses an unsigned shift,
  avoiding undefined behavior on invalid input. Every scalar, disease slot,
  field width, and serialized position remains unchanged; v101 conversion
  clears the whole domain because that record predates it.
  `SoldierDrugStateComponent` owns the persistent drug and alcohol domain:
  the fixed 20-slot effect-duration and magnitude arrays, temporary personality
  and disability identities with their lifetimes, and accumulated alcohol.
  Named merge, temporary-trait, turn-aging, metabolism, and reset operations
  replace direct array arithmetic and keep expiry cleanup coordinated. Effect
  admission validates the established capacity before indexing soldier state.
  The portable visitor still writes all 20 unsigned 16-bit durations, all 20
  signed 16-bit magnitudes, the two unsigned 8-bit identities and unsigned
  16-bit lifetimes, and the 32-bit float alcohol level in their exact existing
  order. v101 conversion clears this later domain. Drug XML, item definitions,
  installed content, Lua, multiplayer data, and the save stream are unchanged.
  `SoldierStatProgressComponent` owns all eleven persistent stat-change
  timestamps and the complementary value-gone-up direction mask. Gameplay
  records changes through stat and mask operations, while tactical and
  strategic UI share wrap-safe recent-change and increase queries instead of
  duplicating clock arithmetic and bit manipulation. The former
  `STRUCT_TimeChanges` wrapper and loose `usValueGoneUp` field are retired.
  All eleven unsigned 32-bit timestamps and the scattered unsigned 16-bit mask
  retain their historical positions and widths, and v101 conversion maps
  every raw value exactly. Typed stat identities also correct three legacy
  presentation-bookkeeping mistakes where food, water, and explosion health
  damage recorded a strength or dexterity timestamp.
  `SoldierTimingComponent` owns the ten soldier-local countdowns plus AI and
  reload delay configuration. Timer purposes are explicit, gameplay uses
  named start, elapsed, and clear operations, and the platform clock updater
  receives mutable counters through the same owner; `STRUCT_TimeCounters` and
  its gameplay macro calls are retired. The ten signed 32-bit counters retain
  their historical consecutive order, while the unsigned 32-bit AI delay and
  signed 16-bit reload delay retain their established scattered positions.
  v101 conversion maps all twelve raw values exactly.
  `SoldierLongActionComponent` owns the action kind, retained context grid,
  and remaining AP cost for work spanning tactical turns. The same established
  grid slot deliberately retains a soldier's return location while intel work
  temporarily removes them from the tactical world. Named begin, AP
  consumption, context-retention, completion, and clear operations update the
  lifecycle atomically and clamp malformed negative or excessive costs.
  Startup now validates and inspects the requested action rather than stale
  prior state, preventing unknown or roof-invalid work from being persisted;
  hack validation and completion consistently dispatch against the retained
  target grid and treat skill equal to difficulty as success.
  All three save fields retain their original positions and widths.
  `SoldierInteractionComponent` owns the complementary direct-interaction
  domain: non-profile merchant identity, mutually exclusive person, corpse, or
  structure dragging, and the reciprocal chat partner. Named drag, clone,
  chat, clear, and reset operations replace scattered sentinel manipulation.
  Fresh soldiers now explicitly start with no corpse target instead of
  accidentally naming corpse zero, and structure placement tests an explicit
  drag state so grid zero remains valid while negative grids remain invalid.
  The referenced soldiers, corpses, and structures retain their existing
  repositories and lifetimes. All five scattered save fields retain their
  original positions and widths; v101 conversion clears this later domain.
  `SoldierPendingActionComponent` owns the persistent action plan previously
  split between `STRUCT_AIData` and flat `TacticalActor`: selected action,
  animation-transition count, five action-dependent payloads, door operation,
  queued-AI special data, and interruption marker. Fresh soldiers now start
  with the explicit no-action sentinel, and the transition count saturates
  rather than wrapping during pathological animation churn. Current-action
  payload cleanup deliberately leaves queued-AI and interruption state
  independent. Target incarnation, path-search source, launcher choice, and
  deferred callbacks remain runtime-only in
  `SoldierPendingActionRuntimeState`. Every established save position and
  visitor width remains unchanged, and v101 conversion retains all ten values.
  Current and turn-start action points now have one private
  `SoldierActionPointComponent` owner. Turn creation, turn snapshots, and
  forced zero-AP transitions update the pair through named operations, while
  authoritative multiplayer reconciliation can still update only the current
  budget without changing packet layout.
  Tactical collapse, breath-triggered collapse, recovery duration, the
  sleep-drug timer, and strategic fatigue collapse now have a separate private
  `SoldierCollapseComponent` owner. Named collapse and recovery operations
  preserve the intentionally independent breath, sleep-drug, and strategic
  fatigue lifecycles while the serializer retains every established field
  position and width.
  `SoldierPerceptionComponent` separately owns sensory range, directional
  movement-noise memory, personal noise grid/volume, normal and monster smell,
  heard-noise elevation, blindness and deafness lifetimes, and X-ray source and
  activation time. Named operations expose the recovery edge that must refresh
  sight, bound blindness extensions, and keep per-turn noise cleanup independent
  from longer-lived effects. Opponent-list knowledge and render visibility
  remain outside this boundary.
  `SoldierAwarenessComponent` owns the complementary player-knowledge state:
  current tactical visibility, the last visibility consumed by rendering, new
  opponent discovery count, the fixed per-observer opponent table and live
  opponent count, and movement distance used to expire stale knowledge. Named
  visible, hidden, indeterminate, fade, render-sync, discovery, and
  forget-distance operations replace scattered magic-state mutations; both
  narrow counters saturate rather than wrapping.
  `SoldierCamouflageComponent` owns the applied and equipment-derived
  camouflage values for jungle, urban, desert, and snow terrain. Shared
  operations now provide the signed, bounded terrain total used by line of
  sight, the strongest non-negative total displayed by tactical and strategic
  UI, and the applied-only total used by camouflage kits. Item definitions and
  their XML representation remain unchanged.
  `SoldierEmploymentComponent` owns the strategic engagement lifecycle:
  contract start, end, and accumulated length; mercenary classification;
  medical deposit and life-insurance terms; renewal bookkeeping; dismissal;
  re-signing eligibility; contract price-change acknowledgement; competing
  contract decisions; and the per-soldier hospital modifier. Hiring, personnel,
  insurance, contract, dialogue, tactical, and AI code now share that owner.
  Profile economics and the transient `MERC_HIRE_STRUCT` request remain
  separate adapters.
  `SoldierAssignmentComponent` owns the complementary strategic duty
  lifecycle: current and previous assignments, training choice and elapsed
  time, squad-merge intent, assignment-specific facility and repair targets,
  completion/idle status, sleep and forced-wake state, fatigue feedback,
  item-moving, and mini-event context. Strategic position, travel path, and
  vehicle occupancy remain separate domains. Named repair, sleep/wake, fatigue,
  and subsidiary-context transitions prevent a duty change from retaining
  stale state.
  `SoldierDeploymentComponent` now owns that separate strategic-placement
  domain: sector coordinates, movement-group and vehicle membership, tactical
  insertion, traversal origin, off-world staging, between-sector transit,
  mission-exit participation, landing-zone arrival policy, arrival
  bookkeeping, and the Unfinished Business helicopter arrival get-up timer
  and phase flags. Named sector, transit, mission-exit, insertion, traversal,
  vehicle, arrival, and arrival get-up transitions keep related values
  coherent. The existing strategic route pointer remains a boundary adapter
  rather than becoming component-owned content. The redundant per-soldier live
  group pointer has been retired: `groupId()` is the sole soldier-side
  authority, and application code resolves a live `GROUP` through the strategic
  group repository only when needed.
  `SoldierVehicleStateComponent` owns the two complementary tactical record
  links: the signed index used when a vehicle soldier resolves its live
  `VEHICLETYPE`, and the typed soldier identity used when a remote robot
  resolves its controller. This tactical vehicle index is deliberately
  separate from `SoldierDeploymentComponent::vehicleId()`, which represents
  strategic passenger membership. Vehicle records and the soldier repository
  remain application adapters; no content-facing vehicle definition changes.
  `TacticalActorRobotics` is the sole live-behavior boundary around that
  controller identity. It validates repository slots, equipped remotes,
  profiles, player-team bounds, sectors, and shared travel context before
  exposing a controller, and repairs stale associations when equipment or
  strategic placement changes. Tactical, strategic, UI, dialogue, and
  autoresolve callers no longer read the persisted slot directly. Robot
  status flags, item flags, assignments, profiles, and save fields retain
  their established data formats.
  `TacticalActorMobility` now owns the complementary live movement and posture
  decisions: water depth, movement animation selection, stance transitions,
  backpack climbing, world-aware stance validation, cover adjacency, and the
  disease-limited fast-movement edge. It rejects malformed animation, item,
  profile, direction, and grid state before reaching legacy lookup tables or
  world structures; current-animation posture queries also stay inside this
  guarded boundary. `TacticalActorWeaponHandling` owns dual-wield eligibility,
  burst pairing, alternative holding modes, and weapon mounting. Inventory,
  animation, vehicle, seat, profile, direction, and neighbouring-actor indexes
  are bounded at that boundary. These are separate compiled domains rather
  than methods on the aggregate; item, weapon, vehicle, map, XML, and Lua
  formats remain unchanged.
  `TacticalActorAiBehavior` now owns the remaining small AI state transitions
  that previously leaked through aggregate methods: exclusive AI-control
  selection, initial-AP and flanking queries, cowering teardown, bounded
  retreat counters, radio animation selection, and boxer-flag cleanup. Team
  repository scans, animation state, body type, visibility, grid, and tactical
  world availability are validated at this boundary.
  `TacticalActorLongActions` owns the complete begin/update/cancel lifecycle
  for fortification, fortification removal, and hacking. It validates actor,
  world, action, animation, inventory, item, direction, target-grid, structure,
  and AP-cost state before consulting legacy tables, while preserving the
  established animation, skill-growth, Lua callback, and AP/BP behavior.
  `TacticalActorPrisonerOperations` separately owns strategic interrogation
  availability and tactical adjacent-prisoner release. Sector, facility,
  direction, world-grid, repository, and prisoner-flag bounds are checked
  before state changes. The fourteen former `TacticalActor` façade methods are
  retired and guarded against returning. Installed maps, facility definitions,
  items, XML, Lua, audio, art, and other mod-data formats are unchanged.
  `TacticalActorMedicalServices` now owns the live AI-medic decisions and both
  sides of the medic/patient service relationship. It bounds world, grid,
  level, direction, animation, inventory, item, repository, and target state;
  stale partner IDs and corrupt provider counts are reconciled during
  teardown. `TacticalActorDamageQueue` separately owns deferred tactical
  damage replacement, exactly-once delivery, and explicit cancellation.
  Nine additional aggregate methods were retired for these two compiled
  domains. Treatment rules, items, traits, maps, XML, Lua, art, audio, and all
  other installed mod-data formats remain unchanged.
  `TacticalActorMedicalTreatment` now owns both live-sector and abstract
  strategic wound treatment plus critical/starvation-damage accounting and
  restoration. The four former aggregate/global entry points are retired, and
  the treatment boundary validates kit IDs/stacks, animation state, action
  points, profile ownership, and surgery consumption before touching legacy
  tables. Tactical, auto-bandage, auto-resolve, assignment, quest, dialogue,
  and AI callers all use the compiled domain. Existing medical items, trait
  values, XML, maps, Lua, and other installed mod-data formats are unchanged.
  `TacticalActorMedicalSession` now owns the bounded first-aid AP cost,
  initiation, and the providing-aid animation transition. It validates
  tactical-world, grid, level, direction, animation, inventory, item, profile,
  repository, and patient state before establishing the two-sided service
  relationship. The final `EVENT_SoldierBeginFirstAid` aggregate façade and
  `GetAPsToBeginFirstAid` global are retired, and the stationary-stance
  monolith delegates its medical transition to this domain.
  Existing medical items, traits, dialogue/Lua events, maps, animation data,
  and other installed mod-data formats are unchanged.
  `TacticalActorFieldOperations` now owns ten bounded actor-to-world tool
  operations: fence cutting, repair, refuelling, corpse blood collection,
  door-alarm attachment, fortification, interactive structures, robot
  reloading, and window inspection/breaking. The corresponding aggregate
  methods and the unbounded `DoInteractiveAction` global are retired. Each
  entry validates tactical-world lifetime, actor/grid/level/direction and
  animation state, plus the relevant inventory item, repository target,
  corpse, door, structure, or interactive-structure index before touching
  legacy tables. Existing items, maps, XML, Lua actions, sounds, animations,
  art, and all other installed mod-data formats are unchanged.
  `TacticalActorCombatActions` owns bounded blade, punch, and throwing-knife
  initiation, while `TacticalActorExplosives` owns bomb placement, tripwire
  disarming, detonator use, self-detonation, and inventory-explosion effects.
  Six additional aggregate `EVENT_Soldier*` methods are retired. Both domains
  reject unavailable tactical worlds and malformed actor, target, animation,
  inventory, item, or world-item state before accessing legacy tables.
  Existing weapons, explosives, maps, XML, Lua, animations, sounds, art, and
  other installed mod-data formats are unchanged.
  `TacticalActorCombatActions` now also owns martial-arts animation
  continuation and its bounded target/awareness/audio selection.
  `TacticalActorCombatReactions` owns fall intent plus one-tile fallback and
  two-tile flyback path setup shared by hit resolution, collapse handling,
  and animation playback. Four more aggregate methods are retired. The
  boundaries reject unavailable worlds and malformed actors, targets,
  animations, body types, directions, levels, grids, movement-cost storage,
  path capacity, or face indexes before accessing legacy tables. Existing
  combat rules, maps, movement costs, animations, audio, art, XML, Lua, and
  network formats are unchanged.
  `TacticalActorRecovery` owns sleep-dart application, breath-collapse
  detection, collapse execution, and get-up progression. The two former
  aggregate methods plus the global collapse entry point and sleep-dart helper
  are retired; hit resolution, turn handling, animation, boxing, explosions,
  and combat reactions enter the same bounded domain. It validates tactical
  world lifetime and actor, profile, body, animation, direction, level, grid,
  and get-up structure state before consulting legacy tables. Existing combat
  and fatigue rules, items, maps, animations, dialogue, audio, XML, Lua,
  network behavior, and other installed mod-data formats are unchanged.
  `TacticalActorInteractions` now also owns the bounded person-to-person
  action and conversation boundaries: resolved conversation initiation and
  civilian, militia, trader, surrender, volunteer, and NPC dialogue routing;
  chat teardown; item giving; handcuffing; equipment or consumable
  application; blood collection; and splint application. Six aggregate
  methods and the global volunteer-recruitment helper are retired. Stable
  simulation commands retain delayed target identities and pass resolved
  actor references into the domain. Target identity and location, actor
  world/animation state, profile, dealer, strategic sector and town bounds,
  inventory stacks and item IDs, handcuff/splint/blood-bag flags, captive
  inventory, and replacement blood-bag creation are validated before
  mutation. Failed give-item continuations no longer expose an uninitialised
  target, zero-weight items cannot divide resistance checks, and a missing
  replacement blood bag no longer consumes the empty bag. Existing items,
  traits, maps, XML, Lua, dialogue, animations, and other installed mod-data
  formats are unchanged.
  `TacticalActorLighting` owns the player-merc personal-light lifecycle:
  bounded creation, recreation, destruction, positioning, and the merc render
  node's personal-light shade levels. Five aggregate light methods plus the
  selected-soldier recreation and personal-light-level global helpers are
  retired; equipment changes, squad activation, tactical input, AI inventory
  changes, base-light updates, and the existing light-option loop enter the
  same domain. The boundary validates tactical-world lifetime, actor identity,
  team, profile, body, animation, grid, level, inventory and attachment shape,
  item indexes, light handles, and projected tile coordinates before accessing
  legacy lighting and item tables. Destruction remains idempotent and clears a
  malformed handle without indexing the light-sprite table. Existing lighting
  templates, vision bonuses, maps, render behavior, settings, XML, Lua, art,
  and other installed mod-data formats are unchanged.
  `TacticalActorTurnBudget` owns the bounded per-turn action-point grant and
  refresh lifecycle: carry-over limits, calculated grants, absolute minimum
  and maximum caps, turn-start snapshots, and the one-shot no-AP transition.
  The aggregate `CalcActionPoints` and `CalcNewActionPoints` methods are
  retired; editor creation, combat entry, turn handling, path and attack-cost
  calculations, and tactical AI all enter the compiled domain. The boundary
  validates body, team, profile, difficulty, health, inventory, item, stack,
  attachment, and tactical-vehicle state before consulting legacy tables.
  The existing `SoldierActionPointComponent` remains the storage owner, and AP
  constants, traits, difficulty bonuses, drugs, items, XML, Lua, maps, saves,
  and network formats are unchanged.
  `TacticalActorAnimationFrames` owns bounded render-frame resolution from an
  actor's current animation surface, animation code, world or extended facing,
  and per-direction frame layout, including the fixed frame used while frozen.
  The aggregate `CryoAniFrame`, `ConvertAniCodeToAniFrame`, and
  `SpriteDirForSurface` methods are retired; editor previews, animation-script
  progression, animation-state setup, and world rendering enter the compiled
  domain. The boundary validates animation-state, surface, eight-direction and
  extended-direction indexes plus video-object frame counts before indexing
  legacy animation tables. Existing animation scripts and assets, rendering,
  maps, XML, Lua, saves, and network formats are unchanged.
  `TacticalActorTraversal` owns roof ascent/descent, fence and window jumps,
  and wall-climb initiation. Player intent still enters through the stable
  `TraverseObstacleCommand`; its executor, tactical AI, and path completion
  now call the same bounded domain. Six aggregate traversal methods and a
  duplicate unused wall-descent helper are retired. The boundary validates
  tactical-world lifetime, actor identity and health, body/animation state,
  route bounds, directions, levels, destination grids, occupancy, and action
  points before starting an animation or teleport fallback. Existing maps,
  structures, animation data, AP settings, XML, Lua, art, sounds, and network
  command formats are unchanged.
  `SoldierScheduleComponent` owns the NPC schedule execution boundary shared
  by the editor, strategic events, tactical AI, animation, and movement:
  schedule identity, current action progress, and the door grid/phase used to
  resume a route after an open-door animation. Named begin, complete, consume,
  cancel, and progress transitions keep paired door state coherent and bound
  malformed progress instead of overflowing it. Placement records, schedule
  nodes, editor map data, and multiplayer creation packets remain unchanged
  adapters.
  Tactical world placement likewise has one private
  `SoldierPositionComponent` owner rather than fields split between
  `TacticalActor` and its pathing record. It owns precise and integer-projected
  coordinates, turn-start coordinates, initial/current grid, elevation and
  facing, integer and interpolated animation-height adjustment, desired height,
  the advanced-animation staging grid, room, and current/previous terrain.
  Named coordinate and terrain transitions keep their paired representations
  coherent.
  `SoldierFrontArcComponent` owns the adjacent three-direction occlusion
  overlay as paired tile-index/grid entries. The tactical world adapter binds
  or clears each pair atomically when it adds or removes topmost nodes, so
  parallel public arrays cannot drift. The fixed three-entry capacity and
  historical save positions remain unchanged.
  `SoldierMovementHistoryComponent` separately owns the most recently departed
  grid and the bounded two-location memory used to detect AI movement
  oscillation. Named departure, AI reset, observation, and full-reset
  transitions keep history from becoming a second current-position owner and
  preserve the original world-bound loop checks. Tactical route destinations,
  movement cursor,
  fixed-capacity direction list, lookup flags, and blacklist now have one
  private `SoldierPathingComponent` owner as well. Tactical route
  execution has a separate private `SoldierMovementComponent`: the selected
  movement-animation mode, stealth and reverse intent, high-resolution current
  and desired facing, movement-animation direction, animation grid-update
  policy, delayed-tile state, reservations, merc contention, scripted and
  continued destinations, stop reason, coordinated speed override, tactical
  turn ownership, water and UI-speed state, AP exhaustion and pauses, movement
  timing/network delay, presentation motion, and destination-center crossing
  plus the staged strategic-exit wait action no longer live in the generic flag
  bucket or distant public fields. UI, AI, animation, rendering, pathing, and
  simulation-command code consume that one route-execution owner. Named
  operations select stealth/reverse behavior, synchronize extended facing,
  request grid-update suppression, begin and end turns, coordinate pause/AP,
  UI-speed and strategic-exit waits, detect presentation-motion edges, and
  update paired state such as a blocker/direction or crossed X/Y destination
  together.
  `SoldierTurnStateComponent` owns the AI scheduler's moved flag, interrupt duel
  points, previous-interrupt result, starting AP snapshot, saved pre-interrupt
  moved value, and fixed per-opponent interrupt counters. Interrupt begin and
  end capture and restore scheduler state through one explicit seam.
  Current attack target grid, elevation, cube level, previous target grid,
  selected target soldier, engaged opponent, and cached line-of-fire target
  identities now have one private `SoldierTargetingComponent` owner. Tactical
  UI, AI, weapons, simulation commands, animation events, and multiplayer
  adapters all read and mutate that same component instead of independent
  public `TacticalActor` fields. `SoldierAttackSelectionComponent` separately
  owns the selected attacking hand and weapon, weapon and scope modes, and
  ranged and melee body locations. This keeps target geometry independent from
  the means of attack while giving UI, AI, weapons, simulation, and network
  ingress one canonical selection. The melee path-cost optimization has its own
  `SoldierMeleeApproachComponent`, keeping the cached target grid and movement
  mode coupled to their cost and terminal direction without making cache state
  part of target identity. `SoldierFireControlComponent` then owns the mutable
  firing sequence and configuration: editor gun archetype, grenade-launcher
  delay mode, selected multi-barrel mode, burst and autofire progress, bullets
  in flight, the one-based spread cursor and its six fixed targets, recoil and
  counterforce history, initial muzzle offsets, the autofire UI edge state, and
  the active multi-barrel cursor. It also owns the start/end grids for the
  burst-spread drag gesture, with named capture, update, and moved queries
  shared by real-time and turn-based input. Named operations coordinate
  single-shot, burst, autofire, launcher-delay, and barrel selection without
  mixing target choice or presentation-only sound and muzzle-flash handles
  into this simulation boundary. AI dual-wield spread generation is clamped
  after its shot count is doubled, preventing twelve writes into the
  established six-target buffer.
  `SoldierCombatResultComponent` separately owns incoming hit attribution:
  current, previous, and earlier attackers, hit location and reason, per-turn
  hit and pellet counts, accumulated damage, and whether the soldier's most
  recent outgoing attack hit. Its history operations keep killer and assister
  transitions atomic. Floating-number cursor, offset, and direction state live
  in `SoldierDamageDisplayComponent`, so presentation coordinates cannot become
  combat state. Accumulated damage deliberately remains simulation-owned
  because torso-hit and death rules inspect it before the number is rendered.
  `SoldierRenderStateComponent` owns the pointer-free values shared by soldier
  rendering: the five palette-replacement identities, fade mode/level/origin,
  forced-colour and shade policy, muzzle-flash visibility and light handles,
  the unblit rectangle, and the projected bounding box. Named fade,
  muzzle-flash, shade, redraw, and light-lifetime operations keep paired state
  coherent. `RenderPaletteBank` separately owns the base 8-bit and 16-bit
  palettes, 48 lighting shades, 20 glow shades, two effect shades, and the
  borrowed active/forced aliases as one RAII render resource. Copies clone
  owned tables and remap aliases, repository relocation transfers the bank
  without cloning, and palette rebuilds publish only a complete replacement.
  Logical-body palette tables compose the same owner instead of inheriting a
  fake `TacticalActor`, so lighting and rendering now consume one palette
  boundary for actors and layers. Surface, level-node, and background pointers
  remain legacy render-adapter resources.
  `SoldierUiPresentationComponent` owns the wider soldier-local tactical view
  state: portrait flash frame and phase, locator animation/cycles/visibility
  and offsets, interface elevation, panel animation/lifecycle and face
  placement, merc-panel requests, first-time AP/unconscious notifications,
  planned-action overlay, and the enemy-cycle cursor. Named locator, panel,
  notification, and planned-target transitions keep related UI state coherent.
  Render-resource pointers remain outside this component and its render-state
  neighbour rather than becoming view-model state.
  `SoldierCombatContributionComponent` separately owns outgoing militia kills
  and assists plus the fixed 156-slot player-team damage attribution record.
  Named kill, assist, promotion-credit, transfer, and reset operations give
  tactical combat, autoresolve, and militia promotion one authority. Kill and
  assist accrual saturates instead of wrapping during exceptionally long
  battles. All three scattered save sites retain their established positions
  and widths, and v101 conversion now preserves every attribution slot rather
  than silently dropping that historical array.
  `SoldierSuppressionComponent` owns the complementary hostile-fire reaction:
  under-fire aging, shock, per-attack suppression points, accumulated AP loss,
  suppressor identity, and close-call feedback. Combat, AI, turn handling, and
  explosion paths now share this single owner, with named operations coupling
  bullet attribution and bounded AP loss while preserving the established
  point-accumulation behavior.
  Animation transition requests
  now follow the same rule through `SoldierAnimationIntentComponent`: requested
  height, primary and secondary queued animations, queued stance and facing,
  UI turn origin, next-tile stopping, and post-stance continuation have one
  private reset boundary. Accepted transitions advance through a separate
  `SoldierAnimationPlaybackComponent`, which privately owns current and
  previous animation state/code, frame timing, selected render surface/depth,
  and animation subflags. A third private
  `SoldierAnimationActivityComponent` owns the surrounding lifecycle:
  prone-turn mode, pausing, turn-to-completion state, hit and fall phases,
  interruptibility, suppression stance changes, and animation AP-cost
  waivers. Traversal landing forecast and its temporary render-depth override,
  plus random-animation cadence and the last selected random animation, live
  here as well rather than as distant public scratch state. Named operations
  update coordinated lifecycle state together.
  Runtime surface residency has a fourth private boundary:
  `SoldierAnimationCacheComponent` replaces the public two-pointer
  `AnimCache` with fixed-capacity inline storage. Soldier creation cannot fail
  on cache allocation, copied soldiers cannot alias cache buffers, and
  repository record replacement retains the working set with the canonical
  slot used by global surface-usage history.
  `SoldierRenderBindingsComponent` closes the remaining process-local storage
  seam. The face-registry handle, tactical `LEVELNODE*`, and auxiliary
  `TAG_anitile*` now live behind one private accessor; ordinary whole-soldier
  copies start detached, while repository commit and slot-swap paths transfer
  bindings explicitly. This prevents temporary creation, save-load, and test
  records from aliasing a live actor's presentation/world registrations.
  Cleanup remains explicit at the existing face, world, and animation-tile
  lifecycle points because the referenced registries own those objects.
  `SoldierRuntimeComponents` is likewise private to `TacticalActor` and exposed
  only through `runtime()`, so the record has no meaningful mutable public
  storage left.
  The former actor POD prefix is gone. Face, level-node, shadow, roof,
  background, and animation-tile placeholders no longer occupy fake storage in
  `TacticalActor`; the ten-byte compatibility tail and v101 mirror record are
  gone as well. `SaveTacticalActor` and `LoadTacticalActor` visit component
  values explicitly through `XferTacticalActor`, while process-local pointer
  visits remain zero-byte serializer operations and are detached on load.
  Baseline 1003 intentionally rejects earlier saves rather than preserving the
  discarded object layout. Map placements, XML, Lua, multiplayer packets,
  packages, and installed content retain their existing schemas; the ignored
  actor-size map-header slot remains present and is written as zero.

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
   `TacticalActor` layout part of the persistence contract.
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
[`ENGINE_SDK.md`](ENGINE_SDK.md). The exact completed runtime-campaign boundary
and its ratcheted legacy tail are recorded in
[`CAMPAIGN_RUNTIME_STATUS.md`](CAMPAIGN_RUNTIME_STATUS.md).
