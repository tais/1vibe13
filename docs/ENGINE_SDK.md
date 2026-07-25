# JA2 Engine SDK

`JA2::EngineCore` is the campaign- and platform-independent C++17 engine
surface. It can be installed without SDL, SGP, VFS, game data, or any JA2
application library and consumed from an unrelated CMake project.
`JA2::RuntimeAdapter` adds the pointer-free JA2 campaign-clock, command, replay,
and tactical-world contracts and links `JA2::EngineCore` transitively. Neither
target links the legacy game or platform libraries.

## Install

Build the repository normally, then install only the SDK component:

```sh
cmake --install build --prefix /path/to/ja2-engine-sdk --component EngineSDK
```

The SDK currently uses its own `0.1.x` compatibility line while the engine API
is being extracted. The `EngineSDK` install component contains both static
archives, their complete public headers under `Engine/Core` and
`Engine/Adapters/JA2`, and CMake package metadata.

## Consume

```cmake
find_package(JA2Engine 0.1 CONFIG REQUIRED)
target_link_libraries(your_host PRIVATE JA2::EngineCore)
```

Tools that need the JA2-specific value model can request that component
explicitly. `RuntimeAdapter` requires and exposes `EngineCore` transitively:

```cmake
find_package(JA2Engine 0.1 CONFIG REQUIRED COMPONENTS RuntimeAdapter)
target_link_libraries(your_tool PRIVATE JA2::RuntimeAdapter)
```

Windows consumers must use the same static MSVC runtime ABI as the installed
archive. The package exports the exact CMake value for that purpose:

```cmake
if(MSVC OR CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
  set_property(TARGET your_host PROPERTY
    MSVC_RUNTIME_LIBRARY "${JA2Engine_MSVC_RUNTIME_LIBRARY}")
endif()
```

Configure the consumer with either `CMAKE_PREFIX_PATH` pointing at the install
prefix or `JA2Engine_DIR` pointing at its `lib/cmake/JA2Engine` directory.

`EngineHost` is the smallest reusable composition root: service contracts,
packages and capabilities, versioned persistence, assets, state control, and
lifecycle without any game command vocabulary. The generic `CommandStream`,
queue, processor, and journal building blocks are also public. The repository's
`Engine/Adapters/JA2` target layers `EngineRuntime`, campaign-clock state,
tactical commands, their codec, tactical-world observation/publication, and
durable replay on Core. It is installed as `JA2::RuntimeAdapter`; platform
adapters and legacy game types remain outside the SDK boundary.

New hosts should configure the composition root through `EngineHostOptions`
rather than relying on the legacy positional constructor:

```cpp
EngineHostOptions options;
options.hostCapabilities.add("host.map-tool");
options.packageRandomSeed = 42;
options.limits.maximumQueuedRuntimeMessages = 2048;
options.limits.maximumPersistencePayloadBytes = 32u * 1024u * 1024u;
options.limits.maximumRuntimeSaveDomainBytes = 64u * 1024u * 1024u;
options.limits.maximumRuntimeSaveContainerBytes = 16u * 1024u * 1024u;

EngineHost<> host(std::move(options), services, packageEvents);
```

`EngineHostLimits` names every host-owned resource ceiling, including message,
input, telemetry, package-state, persistence, and outer-save bounds that were
previously implicit. `ValidateEngineHostOptions` provides a non-throwing
preflight; the named constructor rejects an invalid range before constructing
services or registering sinks. Zero remains valid where the underlying service
supports a disabled mode. The original positional constructor and all of its
defaults are retained, and default named options produce the same configuration
and runtime fingerprint as that compatibility path. `EngineRuntime` exposes the
same named constructor above the JA2 adapter.

`RuntimeSession` treats application startup and package bootstrap as one
transaction. `markRunning()` succeeds only after `StartRuntime`; cancelling an
initialization unwinds every completed phase in reverse while keeping packages
active for a retry. Final `markStopped()` requires `shutdownPackages()` to have
completed, and repeated shutdown attempts never invoke an already-unwound
bootstrap callback again. Established boolean transition methods remain
source-compatible. Hosts that need diagnostics can call `tryBeginInitialization`,
`tryCancelInitialization`, `tryMarkRunning`, `tryBeginShutdown`, and
`tryMarkStopped` to receive `RuntimeSessionTransitionResult`, including rollback
phase/callback counts and structured incomplete/failure errors.
`RuntimeSessionAdvanceResult::packages.callbackException` retains a thrown
bootstrap callback after deterministic rollback for local application
diagnostics. It is not portable package state and must never be serialized or
used as a package-to-package contract.

The `engine_sdk_consumer` CTest installs the component, copies its fixture away
from the repository tree, rejects source/build paths in the exported metadata,
and builds the fresh project against `find_package(JA2Engine)`. It exercises
Core plus campaign-clock ownership, campaign-event ownership/snapshots, the
command codec, durable replay, runtime composition, tactical world
diff/codec/observer, message publisher, and tactical command service surfaces.

`CampaignClockSession` is the value-only strategic-time state owned by each
`EngineRuntime`. It distinguishes uncommitted event slices from a completed
monotonic tick, derives day/hour/minute without platform APIs, and reports when
32-bit legacy time would flow backward. The JA2 application gateway and its
established `GetWorld*` accessors are intentionally outside the SDK; there are
no duplicate scalar clock mirrors. External tools can inspect, restore, and
advance the session without linking any game or save code.

`CampaignClockScheduler`, also owned by `EngineRuntime`, converts elapsed
fixed-step microseconds into the existing campaign speed/resolution model
without consulting a platform clock. It retains only fractional pacing state,
reports accepted and dropped elapsed time, rejects resolutions above 60, and
bounds one call to one real second. Strategic-event execution remains a host
responsibility: headless and replay hosts can consume the scheduled seconds
without linking the JA2 application adapter.

`CampaignClockService` is the versioned, read-only package and tooling view of
that state (`ja2.campaign-clock`, version 1.0). The application registers its
runtime-owned provider before package bootstrap. Each capture copies one
pointer-free snapshot; consumers cannot mutate the live clock or retain a
reference into the runtime. Capture it on the main-thread package boundary.
When `totalSeconds` differs from `previousTotalSeconds`, the snapshot represents
an in-progress strategic-event slice rather than a committed outer clock tick.
`MemoryCampaignClockService` and `NullCampaignClockService` let package tests,
replay tools, and headless hosts use the same contract without the JA2 process.

`CampaignEventService` exposes a versioned, read-only view of scheduled
strategic work (`ja2.campaign-events`, version 1.0). Each
`CampaignEventQueueSnapshot` contains only values: scheduled seconds, parameter,
time offset, raw event type, raw callback ID, and flags. Numeric values stay opaque
so mods can extend the legacy vocabulary without importing game headers or
changing the SDK. Captures are bounded, validate nondecreasing timestamps,
preserve FIFO order for equal timestamps, and replace caller state only after a
complete capture. The JA2 adapter also rejects damaged cyclic queues. This
package contract does not grant event creation, deletion, dispatch, or save
authority.
`MemoryCampaignEventService` and `NullCampaignEventService` provide equivalent
package-test, replay-tool, and headless-host fixtures.

At the host layer, every `EngineRuntime` owns one `CampaignEventQueue`.
Scheduling is bounded, equal-time insertion is FIFO, nodes have non-repeating
runtime identities, and whole-queue replacement is transactional. The node
surface exists to bridge JA2's established `STRATEGICEVENT` callback API; new
packages should consume `CampaignEventService` snapshots instead of retaining
host nodes.

`TacticalCommandService` is the package-facing, pointer-free write boundary for
JA2 tactical commands. A host owns a finite `TacticalCommandInbox`, registers it
as `ja2.tactical-commands` before package bootstrap, validates application
domains at its safe simulation boundary, and drains only a configured prefix.
The current command vocabulary includes pointer-free world-object activation
and approach, stable conversation and vehicle-entry targets, typed roof, fence,
wall, and window traversal, and player weapon-mode, scope-mode, reload, turn,
stance, fire, movement, facing, stealth, and stop-movement intent. An approach
combines movement with its pending interaction so command pressure cannot apply
one without the other. Conversation partners and vehicles carry both their pool
slot and incarnation; delayed arrival therefore rejects a despawned or reused
target instead of addressing whichever actor later occupies the same slot.
Scope targets use `TacticalNoTargetGrid` when no aim tile is available; reload
intent explicitly records whether a non-empty weapon may be reloaded.
Traversal uses `TacticalTraversalKind`, keeping legacy soldier and structure
pointers, AP calculations, and animation constants outside the package-facing
contract.

The command journal has one current wire layout. It retains a version field so
a genuinely published format can evolve later, but unsupported versions are
rejected rather than supported speculatively.
The service deliberately does not expose draining or cancellation authority.
Every callback receives a registry-issued `PackageIdentity`. It can be copied
and passed to package-aware services, but cannot be constructed from an
arbitrary package ID. Retaining it does not keep a package active: services
must still reject work after that package leaves the active set. Native code in
the same process remains a cooperative trust boundary rather than a sandbox.

JA2 packages should call `BindTacticalCommandClient` with the callback's
`extensionServices` and `identity`, then retain the returned client for runtime
submission. The client supplies ownership automatically. Direct
`TacticalCommandService::submit(packageId, command)` remains available only as
a source-compatibility path for existing hosts.
The JA2 application additionally tracks the bounded accepted batch by command
sequence so lifecycle teardown can cancel both pending inbox requests and any
accepted command retained after an execution failure. Admission requires a
loaded tactical world (and turn-based combat for end-turn commands); actor
incarnation and live-sector checks remain executor policy so stale references
are deterministically journaled as discarded. Any existing authoritative
queue, including a future-tick staged replay, pauses package admission until
that stream clears. This keeps live ingress from forcing a large replay sort on
the frame thread or interleaving two authoritative producers.

`TacticalWorldService` exposes immutable, pointer-free snapshots of the loaded
world. In the JA2 host, `snapshot.epoch()` is the nonzero world-load generation
and `snapshot.turn().serial` is a nonzero identity scoped to that epoch: serial
one denotes the newly loaded pre-turn state, each accepted `BeginTeamTurn`
boundary advances it, and exhaustion saturates instead of wrapping. Compare a
turn serial only within the same epoch. `EngineRuntime` also owns the
turn-based/combat mode and current team in that same session; the JA2 adapter
is the only writer of their established `gTacticalStatus` mirrors. The existing
tactical-delta wire already encodes the complete turn snapshot, so this
ownership move requires no wire or service-version change.

The application retains `gWorldSectorX`, `gWorldSectorY`, and `gbWorldSectorZ`
as const-reference projections for source-compatible, allocation-free hot-path
reads. Their hidden storage is published only by `TacticalWorldAdapter`; writes
and mutable address escapes are rejected by both the C++ type system and the
architecture boundary test. They are compatibility views, not engine storage:
packages and external tools consume `TacticalWorldService`, and application
transitions use the adapter gateways. The `TURNBASED`, `INCOMBAT`, and
`ubCurrentTeam` portions of `gTacticalStatus` follow the same single-writer
policy while the remaining broad legacy status structure is migrated by its
own gameplay domains.

`TacticalWorldObserver` invalidates `latest()` when its source becomes
unavailable or the host calls `reset()`; any previously returned publication
pointers expire at that boundary. The next available world establishes a fresh
baseline with publication serial one. Capacity, allocation, adapter, validation,
and diff failures still preserve the last complete publication. A direct
nonzero epoch replacement without an unavailable boundary continues to emit the
existing `TacticalWorldResetEvent`. The production bridge drops a retained
queue-failed delta on world transition before retrying, and reports the current
world/turn identity, transition and observer-reset counts, and discarded pending
deltas through `Ja2TacticalWorldObserverDiagnostics`.

Hosts that execute shared command/replay queues should use the budgeted
`ProcessCommandsThrough(queue, tick, maximum, handler)` overload. It reports
`CommandProcessStatus::BudgetExhausted` when more of the original ready prefix
remains, without copying or invoking that remainder. The original overload is
unchanged for compatibility and continues to process its complete initial
ready set.

Optional source-built host services are discovered through `ServiceCatalog`
using portable IDs and major/minor contracts. Registrations are non-owning and
must outlive the host. The catalog seals when initialization or package
bootstrap begins, so packages may safely retain a successfully resolved service
for the runtime session. `EngineServiceContract<Interface>` accepts a concrete
implementation derived from that interface, while invalid/default contracts
return structured registration or lookup errors. This is not yet a stable
native plugin ABI.

Hosts may also populate `RuntimeConfiguration` with portable keys and boolean,
signed integer, double, or string values before initialization. Packages read
the sealed configuration from their bootstrap/runtime context. Replacing a key
with a different type is rejected so configuration contracts cannot silently
change beneath consumers.

Each package callback also receives `PackageStorage`, a view bound to that
package's ID. Record names are portable identifiers and data uses the engine's
bounded checksummed envelope format under `PackageData/<package>/<record>.bin`.
This is the preferred durable-state API for new packages.

Portable package, capability, service, message, locale, definition, and record
identifiers are limited to 256 bytes; opaque package version labels use the
same ceiling. Logical asset paths are limited to 4096 bytes before
normalization. These metadata bounds are separate from payload limits and are
enforced consistently by live queues, catalogs, and persisted records.

State that belongs to a particular game save uses a separate contract. Set
`PackageDescriptor::saveStateSchemaVersion` to a non-zero version and override
`saveState`, `validateState`, and `loadState`. Capture publishes opaque bytes;
validation must parse without mutation; load commits transactionally for that
package. The host orders records by package activation, defaults to 4 MiB per
package and 16 MiB in aggregate, applies the same named bounds to live capture
and archive I/O, binds the archive to the runtime fingerprint, and contains
callback failures. Installation/profile preferences should remain in
`PackageStorage`; campaign progress belongs in the per-save callbacks.

Use `PackageBootstrapContext::messagePublisher` for outbound package messages.
It binds the source to the registered package ID and accepts only a portable
topic plus the bounded byte payload. The raw message bus remains available
during the compatibility window for established integrations.

Declare mandatory host integrations in `PackageDescriptor::requiredServices`.
IDs must be unique portable identifiers and minimum major versions must be
non-zero. The engine checks all active packages against the sealed service
catalog before configuration starts, so a missing or incompatible integration
fails deterministically before package code acquires partial resources.

For new deterministic package logic, use `PackageBootstrapContext::random` and
a stable portable stream name such as `combat` or `loot`. Streams are isolated
by package and name, use unbiased bounded values, and expose sorted usage
snapshots for replay diagnostics. Versioned checkpoints include the generator
state and draw counter for every stream; package save archive v3 captures them
without changing a package's opaque callback schema. The host seed and
per-package stream limit are composition settings; the legacy
`EngineServices::random` remains intact.

Packages may override `EnginePackage::simulate` for fixed-step work that should
not depend on rendering cadence. The host publishes the configured step and
maximum catch-up count, executes only that bounded number after a hitch, and
records dropped ticks in frame telemetry. `updateRuntime` remains the per-frame
hook for interpolation, UI, and other presentation-paced work. Hosts can use
`SimulationTickDispatcher::addSinkBefore` when authoritative system state must
commit before a package-facing sink. The JA2 application uses that ordering for
campaign time, so package callbacks observe the clock after the corresponding
strategic slice.

Message, input, runtime-update, simulation-tick, deferred-task, and registered
state callbacks are non-reentrant boundaries. A nested dispatch or lifecycle
mutation returns an explicit operation-in-progress result; work published or
scheduled by a callback remains eligible for the next outer boundary.

Call `AssetSource::metadata` when a package only needs existence, size, or
winning overlay provenance. The built-in sources answer without allocating the
asset payload, normalize paths exactly like `read`, and clear output on every
failure. Custom sources may return `Unsupported` until they provide a fast
metadata implementation.

The default host exposes package assets through a bounded read-through cache.
Its entry and byte budgets are sealed configuration values, statistics are a
versioned host service, and package activation/deactivation clears cached
overlay results. Oversized assets still load normally but are not retained.

`EngineHost::diagnostics()` returns a self-contained observation suitable for
launchers, automated bug reports, and headless assertions. It combines package
health, frame timing, cache behavior, host contracts, capabilities, and live
queue counters without exposing application-owned objects or mutable services.
Its compatibility fingerprint is also available directly from
`EngineHost::compatibilityFingerprint()`. Compare the schema and both hash words
before loading portable saves/replays or joining a deterministic session. A
different result identifies a package, contract, capability, configuration, or
versioned-definition mismatch; it is diagnostic rather than a security proof.

`EngineHost::saveRuntimeCheckpoint` writes that identity together with active
package IDs/versions and completed frame/tick counters through the bounded
checksummed persistence envelope. `loadRuntimeCheckpoint` publishes metadata
only after integrity, schema, bounds, package identity, and current-runtime
compatibility all pass. It is a preflight manifest for domain save/replay data,
not yet a replacement serializer for JA2's tactical or strategic state.

`capturePackageSaveState`, `validatePackageSaveState`, and
`restorePackageSaveState` coordinate package-owned campaign state. The
`PackageSaveArchiveService` serializes that snapshot through the same bounded,
checksummed persistence boundary and rejects a different runtime before
publishing records. Opaque bytes and encoded engine records share the aggregate
save budget. RNG replacements for all packages are prepared before any live
state changes; v2 commits them with no-throw swaps after every callback succeeds,
while v1 and failed loads preserve the current streams. JA2 attaches these
archives beside legacy saves; other hosts can choose their own domain-save
transaction and naming policy.

The host also publishes `engine.runtime-faults`. Each contained package
failure receives a monotonic record with package ID, callback, kind, and
occurrence count. The bounded history never throws into gameplay and remains
complete independently of duplicate-log suppression; it is included in the
unified diagnostics snapshot.

Register new framework text through `PackageBootstrapContext::localization`.
Locale and key are portable identifiers, text size and total entries are
bounded, later package layers win, and lookups can explicitly fall back to
`en`. Returned views are valid until the catalog changes. The host removes all
owned entries during configure rollback or shutdown; legacy JA2 localization
remains untouched during the migration window.

Use `PackageBootstrapContext::definitions` for new data-driven rules and other
domain records. Each definition has a portable type and ID, non-zero schema
version, and bounded opaque bytes. The top package override is authoritative:
an incompatible schema is reported instead of silently falling through to a
lower definition. Package rollback and shutdown restore the previous layer.

Use `PackageBootstrapContext::entities` when data must cross framework
boundaries without exposing pointers or legacy array indexes. The registry
returns a slot plus generation, rejects stale handles after reuse, bounds total
live identities, and automatically destroys everything owned by a package at
rollback or shutdown. Domain objects and components stay application-owned.

Play new framework audio through `PackageBootstrapContext::audio`. The package
identity is host-bound; callers provide a portable logical group and normalized
asset path, may stop or retune only their own group, and cannot exceed the
host's sealed playback capacity. Configure rollback and package shutdown stop
all remaining owned playback. Completed one-shot playback is pruned through
the adapter's `isPlaying` contract before it can strand bounded capacity.
Volume and pan are retained in diagnostics and may be changed for a complete
owned group. Existing JA2 `Sound*` calls keep their public signatures and
session-local handles, but now pass through the same `AudioOutput` contract;
the raw SDL mixer identifiers remain private to the legacy platform adapter.

Supply frame presentation through `EngineServices::frames`. `FrameDriver`
delivers normal completed frames directly to that engine-owned contract. The
compiled JA2 host also binds its established `RefreshScreen` and `PresentNow`
entry points to the same presenter, retaining paced and immediate semantics
without exposing the SDL renderer. A headless host can bind a recording or null
presenter while exercising legacy loading and UI flows; recursive gateway calls
are suppressed and presenter exceptions are contained. Raw SDL submission
remains private to the platform adapter.

Framebuffer damage is separate from presentation. Bind
`EngineServices::frameInvalidation` to capture half-open dirty regions, complete
redraws, and semantic change markers without creating a window. The compiled
host routes the legacy `Invalidate*` entry points through this service while
retaining their clipping and buffer-state behavior.

`EngineServices::renderSurfaces` provides the low-level pixel-storage boundary.
It resolves standard surface roles, describes dimensions, storage format, and
logical content depth, and maps adapter-owned mutable bytes until the matching
`unmap`. Hosts must serialize mapping, renderer lifetime, and surface
registration on their render thread. `MemoryRenderSurfaceAccess` supplies
bounded deterministic surfaces for headless tools and tests. Existing SGP
numeric handles remain accepted by the compatibility gateway, but new package
code should treat `RenderSurfaceId` values as opaque and obtain standard targets
through `surfaceFor`. This is a storage/access contract; higher-level portable
draw commands layer above it rather than exposing SDL objects. `DepthBuffer` is
a standard role with `Depth16` storage. A depth mapping may have a pitch larger
than `width * 2`; consumers must advance rows by `pitchBytes` and never treat
padding as logical depth pixels.

`EngineServices::renderCommands` is that higher-level boundary.
`RenderSurfaceFillCommand` uses an opaque surface ID, a half-open region, and
an RGBA colour. `RenderSurfaceCopyCommand` adds a source region, destination
origin, and either opaque or RGB source-colour-key copying.
`RenderSurfaceStretchCommand` uses explicit source and destination regions for
portable nearest-neighbour scaling; clipping retains the original sampling
phase, out-of-range source texels are skipped, and scaled same-surface work
snapshots its bounded source before writing. `RenderSurfaceShadeCommand`
multiplies RGB by an explicit rational fraction while preserving ARGB alpha.
`RenderDepthFillCommand` fills a clipped `Depth16` region with one unsigned
ordering value and never touches row padding. It is intentionally separate from
RGB565 colour work: colour fill, copy, stretch, and shade reject depth surfaces.
`RenderImageDrawCommand` identifies a host-owned image and frame with opaque
stable values, plus a destination anchor and explicit opaque,
source-transparent, destination-shadow, or destination-intensity composite
mode. Shadow and intensity treat visible source runs as a mask over the existing
destination; the platform adapter retains their exact shade-table behavior.
`ClearDestination` uses the same visible runs to write transparent black while
respecting the destination format, pitch, row padding, and explicit clip.
`PaletteWithShadowMarker` names a host-owned immutable 256-entry lookup through
`RenderPaletteId`, treats source index 254 as destination shading unless
`ignoreShadows` is set, and can name a parallel alpha image through another
stable `RenderImageId`.
The half-open clipping region is part of the command, so recording and
forwarding hosts do not depend on mutable renderer-global clip state.
Image-local offsets, compression, palette storage, and physical pixels remain
adapter concerns; engine and package code never receives an `HVOBJECT`, ETRLE
pointer, native palette pointer, or backend pixel layout.
`RenderImageOutlineCommand` uses the same stable image identity and explicit
clip while distinguishing colour-outline rendering from body-shadow rendering.
Its RGBA colour and `drawOutline` switch replace packed framebuffer colours and
format-specific marker values at the SDK boundary.
`RenderImageDepthOutlineCommand` adds separate colour and `Depth16` surface
identities, explicit strict or inclusive comparison, preserve/replace depth
policy, and visible-only or checkerboard-when-obscured behavior. Image-defined
outline markers do not change depth in visible-only mode. The pixelated form
retains the legacy rule that every front-facing pixel, including a marker,
replaces depth.
`RenderImageDepthDrawCommand` identifies its colour and `Depth16` surfaces
separately. `SourcePalette`, `BlendSourcePalette50Percent`, and
`CheckerboardSourcePalette` perform the established inclusive greater-or-equal
test and respectively copy, blend, or sample source palette colour through a
stable absolute-coordinate checkerboard. `ShadeDestination` and
`IntensifyDestination` use the source image as a mask, perform the established
strict greater-than test, and transform the destination colour.
`PixelateObscuredSourcePalette` also uses a strict test: passing pixels render
normally while failed pixels sample through the same stable checkerboard.
`ReplaceOnPass` updates front-facing depth only; `ReplaceOnDraw` additionally
updates sampled obscured pixels. `PaletteWithShadowMarker` pairs with the
inclusive test and preserve/replace-on-pass policy while retaining custom
palette remapping, marker shading, ignore behavior, and optional parallel
alpha. `PaletteWithShadowMarkerPixelateObscured` preserves depth, applies those
same rules to passing pixels, and samples failed non-marker pixels through the
absolute-coordinate checkerboard; marker shading remains strictly
front-facing. `StripDepthSourcePalette` and its obscured form name
`depthProfileFrame`, a source-owned profile that varies depth across successive
vertical strips, while retaining strict structure or inclusive wall comparison
and pass-only or sampled depth writes. The strip palette/marker forms preserve
their own profile increment, optional alpha, marker behavior, and the
historical alpha-obscured strict comparison versus the non-alpha inclusive
comparison. Unsupported resources, profiles, effects, comparisons, or write
pairings are rejected rather than acquiring backend-specific meaning.
`RenderImageDepthVisibilityQuery` performs the corresponding read-only
occlusion test against a `Depth16` surface. Its tri-state
`RenderImageDepthVisibility` result separates `FullyOccluded`, `Visible`, and
`Unsupported`; the last value is essential for safe compatibility fallback and
must never be interpreted as hidden.
The mapped implementation supports indexed opaque copy/stretch and true-colour
fill, copy, stretch, and shade operations, defines corruption-safe
same-surface overlap, never writes row padding, and balances every successful
map. Image commands require a host resource adapter, so the generic mapped sink
rejects them while `RecordingRenderCommandSink` captures all nine command types
and depth-visibility queries without a renderer. The compiled host routes
existing rectangle fills, numeric
`BltVideoSurface`/`BltStretchVideoSurface`, surface-shadow calls, and stable
managed video-object draws and outlines through this service. Tactical
full-world redraws clear the live Z-buffer through the depth-fill command, and
ordinary transparent-Z tactical sprites plus basic tactical shadow and
intensity masks use the depth-image command. Depth-tested 50% blends and
checkerboard-sampled tactical sprites use it as well, preserving inclusive
testing, absolute checkerboard phase, clipping, and optional depth writes.
Checkerboard-when-obscured tactical sprites now use the same command while
retaining their strict front test and their historical clipped versus
unclipped depth-update distinction.
Tactical item outlines use the regular or depth-outline command, preserving
their exact marker-depth, strict-versus-inclusive equality, clipping, and
obscured checkerboard rules.
The clipped mask path now honors the preserve-depth policy instead of selecting
its writing compatibility blitter. Every successfully created host video
object receives a stable opaque render identity without changing its legacy
manager handle; deletion retires that identity before releasing image storage.
Generated render palettes receive their own opaque identities above the legacy
32-bit manager range. Re-registering the same live pointer is idempotent,
retired identities are never reused, and palette owners retire their borrowed
registry entry before replacement or destruction.
Rejecting hosts and manually assembled fixtures fall back to the exact old
blitter. Basic non-depth transparent, shadow, and intensity tactical sprites
use the regular image command with the same fallback. Ordinary merc and corpse
palette-shadow draws, including clipped/unclipped, alpha, and depth-write
variants, now use the same command boundary. Their obscured variants also use
it while preserving inclusive front pixels, checkerboard phase, strict marker
shading, alpha, clipping, and unchanged depth. Multi-Z walls, structures,
multi-tile actors, and corpses use the same boundary with an explicit profile
frame. The platform adapter executes their consolidated SGP backend; rejecting
hosts and manually assembled fixtures retain the exact raw fallback.
The platform surface adapter reference-counts nested maps and rejects deletion
or replacement through a live mapping. Legacy packed colours, mutable shade
percentages, and RGB565 transparency tokens are translated only in compatibility
code; package code uses explicit engine values.

Packages may declare `requiredCapabilities` alongside contributed
`capabilities`. The host validates the list at registration and preflights each
requirement against host and active-package capabilities before the first
bootstrap callback. A missing feature produces a structured package/capability
failure and a fault-journal record instead of forcing mod code to inspect build
targets or global campaign state. Capability provider names beginning with
`application.`, `engine.`, or `host.` are reserved for
`EngineHostOptions::hostCapabilities`. Packages may require those features, but
package registration rejects any attempt to contribute them; package-owned
capabilities should use domains such as `campaign.`, `rules.`, `ui.`, or a
project-specific prefix.

Use `PackageBootstrapContext::tasks.defer` for small pieces of package-owned
main-thread work that should run on a later frame. The queue and per-frame drain
are bounded host configuration, recursively deferred work cannot loop in the
same frame, and thrown callbacks are contained in runtime diagnostics. Pending
callbacks are cancelled automatically during rollback or package teardown;
packages must still avoid capturing objects with shorter lifetimes than their
own active lifecycle.

`EngineHost::packageResourceUsage()` and the unified diagnostics snapshot
attribute live framework resources to every registered package in deterministic
catalog order. Use the per-package counts/byte totals to diagnose runaway mods,
tune host capacities, and verify teardown; a non-zero `unattributedRecords`
value signals an ownership invariant violation that should be treated as an
engine bug.
