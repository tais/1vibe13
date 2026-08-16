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

The SDK currently uses its own experimental `0.1.x` compatibility line while
the engine API is being extracted. The `EngineSDK` install component contains
both static archives, their complete public headers under `Engine/Core` and
`Engine/Adapters/JA2`, CMake package metadata, this guide, a public package-host
example, and the `0.1` compatibility kit. Tagged release CI publishes this
install tree as a separate `ja2-engine-sdk-<platform>-<tag>.zip` beside each
game archive.

### Pre-1.0 compatibility policy

The contract is intentionally narrower than a stable `1.0` ABI:

- Patch releases in one minor line, such as `0.1.0` to `0.1.1`, retain the
  documented installed public source contract. They may add APIs. A breaking
  source change advances the minor line, such as `0.1` to `0.2`.
- `find_package(JA2Engine 0.1 ...)` uses CMake's `SameMinorVersion` rule. A
  future `0.2` package will not silently satisfy a consumer requesting `0.1`.
- There is no cross-toolchain C++ ABI promise before `1.0`. Static archives and
  consumers must match platform, architecture, compiler/standard library,
  build configuration, and on Windows the exported MSVC runtime-library mode.
- Versioned service, replay, persistence, content, and message schemas keep
  their own compatibility rules. The SDK package version does not override
  those on-disk or runtime protocol versions.

The installed CMake package exposes `JA2Engine_VERSION`,
`JA2Engine_COMPATIBILITY_LINE`, `JA2Engine_STABILITY`,
`JA2Engine_SOURCE_COMPATIBILITY`, `JA2Engine_BINARY_COMPATIBILITY`, and the
path `JA2Engine_COMPATIBILITY_MANIFEST`. The JSON manifest is a
machine-readable release identity and policy record, not a claim that every
C++ symbol has a stable binary layout.

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

## Public example and compatibility kit

The installed `share/JA2Engine/examples/package-host` project is a small real
host rather than a test-only fixture. It defines an application-owned
`EnginePackage`, publishes a host capability through `EngineHostOptions`, and
drives registration, activation, all bootstrap phases, and transactional
shutdown while linking only `JA2::EngineCore`:

```sh
cmake -S share/JA2Engine/examples/package-host -B example-build \
  -DCMAKE_PREFIX_PATH=/path/to/ja2-engine-sdk
cmake --build example-build \
  --target run_ja2_engine_sdk_package_host_example
```

`share/JA2Engine/compatibility` contains the installed `0.1` JSON manifest,
its standalone verifier, and an executable source probe for both public
targets. Downstream packagers can run it against an extracted SDK without a
JA2 checkout:

```sh
cmake -S share/JA2Engine/compatibility -B compatibility-build \
  -DJA2Engine_DIR=/path/to/ja2-engine-sdk/lib/cmake/JA2Engine \
  -DJA2_ENGINE_REQUIRED_COMPATIBILITY_LINE=0.1
cmake --build compatibility-build \
  --target run_ja2_engine_sdk_compatibility_probe
```

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

JA2 application code that must select between the built-in campaigns uses the
value-only `CampaignApplicationPolicy` above `Engine/Core`. It owns the common
content-loader, tactical-loop, helicopter-arrival, and underground-load-screen
decisions; both choices are covered by a standalone data-free CTest. Engine or
package contracts must continue to consume capabilities and declared content,
not include this application policy or test the legacy `JA2UB` build macro.
The policy does not expose mutable globals, platform services, dealer IDs, or
save records, and it changes no existing XML path or content format.

The `engine_sdk_consumer` CTest installs the component, copies its private deep
fixture away from the repository tree, rejects source/build paths in every
textual installed artifact, and builds the fresh project against
`find_package(JA2Engine)`. It also configures, builds, and runs the public
package-host example and the compatibility kit directly from the installed
tree. The deep fixture exercises Core plus campaign-clock ownership,
campaign-event ownership/snapshots, the command codec, durable replay, runtime
composition, tactical world diff/codec/observer, message publisher, and
tactical command service surfaces.

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
The current command vocabulary includes pointer-free player world-object
activation/approach and retained System/Replay automatic world-object
interaction, stable conversation and vehicle-entry targets, typed roof, fence,
wall, and window traversal, and player weapon-mode, scope-mode, reload,
ready/lower, turn, stance, fire, movement, facing, stealth, stop-movement,
drag cancellation, stealing, position exchange, and world-item pickup intent.
An approach combines movement with its pending interaction so command pressure
cannot apply one without the other.

`SystemWorldObjectInteractionCommand` is the automatic counterpart, not a
player request. AI, path traversal, pending-action completion, and dialogue NPC
opening carry separate origins and closed continuation shapes. Package and replay
hosts see only values: exact actor/object identities, selected operation, route
and point-cost preconditions where applicable, deterministic state fingerprints,
and event policy. The portable reference simulation deliberately discards this
JA2 structure/animation policy. Only a newly produced command may retain peer
replication for its later animation keyframe: local player and System work keep
that legacy behavior through any selected AI/path continuation. Replay, and a
shared player-object command carrying peer provenance, always complete locally
without door, path, or stop reflection.

The full non-positional dialogue-effect audit lives in
`DIALOGUE_EFFECT_INVENTORY.md`. It records all 32 bit values, all 33 exposed
symbols, their source-coded producers, the centralized queue effects, and the
dynamic Lua gateway. The first-party stat-change producer now uses a typed
private queue payload while retaining the raw bit/data lane for Lua and legacy
composites. This is not an SDK simulation command and changes no installed
content contract. The other 31 effects remain legacy until a slice closes over
composite ordering, stale-state identity, UI/campaign continuation, provenance,
and replay behavior.

`SimulationCommandExecutor` is the host-owned execution boundary beneath that
value stream. It receives the exact command plus deterministic tick and
sequence metadata and returns `Applied`, `Retry`, or `Discard`. The compiled
game implements this interface in its compatibility adapter, where established
JA2 pathing, animation, inventory, dialogue, vehicle, and combat mechanics
remain. Queue and replay code therefore no longer knows whether it is driving
the live game, a tool, or a data-free simulation.

Bind that implementation once with
`EngineRuntime::bindSimulationCommandExecutor`; the host-owned executor must
outlive the runtime, rebinding to a different world is rejected, and repeating
the same binding is harmless. An unbound runtime uses
`NullSimulationCommandExecutor`, which returns `Retry` so staged authoritative
work remains queued. `executeCommandsThrough` and
`executeExpectedCommandThrough` are the common bounded and synchronous drains.
They invoke the bound executor, acknowledge only its applied/discarded result,
update the runtime journal, and then notify an optional
`SimulationCommandExecutionSink`. Sink failures are contained by the command
processor and cannot change delivery. A nested drain from an executor or sink
returns `QueueChanged` without invoking another command; the outer transaction
continues normally.

`MemoryTacticalSimulation` is the installed deterministic implementation for
headless hosts, replay inspection, and package tests. Its reset accepts a
pointer-free `TacticalSimulationSnapshot`, validates and sorts exact actor
incarnations transactionally, and reserves the configured actor and shot
ceilings before publication. Movement, stance, facing, stealth, stop,
path/stop/vitals synchronization, fire, and turn commands update this stable
state without SDL,
game data, or legacy headers. Unsupported commands and stale identities are
discarded without partial mutation; shot and turn overflow fail closed. Raw
stance and movement values remain opaque adapter data, so this reference model
does not invent a second set of JA2 combat rules. Recorded shots retain their
tick and sequence and can be cleared after a host consumes that bounded event
history.

Conversation partners, vehicles, steal targets, exchange partners, and exact
pickup targets carry both their reusable slot and incarnation; delayed arrival
therefore rejects a despawned, moved, or reused target instead of addressing
whichever actor or item later occupies the same slot. Exchange also captures
both issued grids and applies the swap plus both AP deductions atomically. A
`SearchGrid` pickup deliberately carries the default-invalid world-item ID
because the selected intent is discovery rather than one exact object.
Scope targets use `TacticalNoTargetGrid` when no aim tile is available; reload
intent explicitly records whether a non-empty weapon may be reloaded.
Ready/lower intent records the selected eight-way direction and alternative
weapon-hold choice rather than a cursor position or an animation constant.
Stance intent remains one value command for both stationary and moving actors;
the JA2 executor selects the established real-time movement animation where
needed. Drag cancellation carries only the exact actor identity.
Squad controls remain a UI-side fan-out: each accepted stealth or stop intent is
still an ordinary per-actor command with an independently verified incarnation,
so no package-only batch type or global squad identity leaks into the engine
contract.
Legacy multiplayer path, fire, stop, healing, stance, facing, and turn packets
retain their existing wire structures. The application adapter resolves their
reusable soldier slot once and submits the same stable command vocabulary.
Path and stop packets are represented as explicit bounded reconciliation
snapshots rather than being misread as local movement intent; synchronized fire captures the
packet's selected weapon; `SynchronizeActorVitalsCommand` captures the heal
packet's signed health and bleeding bytes with the exact actor incarnation; and
synchronized turn capture records whether the
receiving host must enter combat or close its client turn. These synchronization
commands accept only network/replay provenance. Vitals application is
intrinsically local-only, so neither received nor replayed snapshots can emit
another heal packet. Reliable network ingress queues
behind existing authoritative work when immediate execution is unavailable.
AI and script producers use an equivalent retained `System` ingress path.
AI movement preserves System path origin and pending-action state; final fire
captures the attacking hand and weapon rather than consulting mutable actor
selection after a delay. `ChangeStanceCommand` and `SetFacingCommand` carry an
explicit `TacticalEventPolicy`, allowing the adapter to preserve either the
legacy replicated wrapper or local-only application independently from command
source. Network stance/facing ingress is required to be local-only, preventing
received commands from being echoed. Dialogue-directed movement, stance, and
facing now share these semantics.
Traversal uses `TacticalTraversalKind`, keeping legacy soldier and structure
pointers, AP calculations, and animation constants outside the package-facing
contract.

The JA2 application captures actors through
`GetJa2TacticalEntityId(const TacticalActor&)` at each local UI, AI, dialogue, or
network producer. That host adapter atomically reads the referenced merc's slot
and incarnation and verifies that the runtime directory resolves the identity
back to the same object. Detached or forged objects return an invalid
`TacticalEntityId`, which produces
`SimulationCommandDispatchStatus::InvalidActor` before queue submission.
`Simulation Commands.h` accepts only complete `TacticalEntityId` values; it
does not expose `TacticalActor` or separate slot/incarnation parameters. Packages,
replay tools, and network hosts therefore use the same pointer-free command
surface through `TacticalCommandService`. Legacy delayed-action completion
helpers that must inspect pending fields remain isolated behind
`Simulation Command Legacy.h` and are not SDK command ingress.

Application callbacks that cannot yet become simulation commands follow the
same producer rule. `Ja2TacticalEntityReference::capture` accepts only
`TacticalEntityId`; dialogue, contract, traversal, placement, confirmation, and
timer producers cross the canonical-record adapter before retaining an actor.
Legacy callback bodies may resolve that identity at the point of use, but cannot
store or recapture the originating `TacticalActor*`. This is application-only,
runtime state and does not extend the package API or any persistent format.
Planning mode, vehicle and militia menus, debug dialogue, and the animation
viewer use that same ID-only ingress. Search-scoped pathing cache state stores
the identity value rather than its record address. Synthetic systems that do
not represent live entities, such as the fixed air-raid attacker, resolve their
owned compatibility record on demand and are never exposed as package actors.
Map-screen movement and assignment-update sessions also accept only complete
actor identities. Movement rows fail closed as one immutable callback-indexed
snapshot, while non-interactive update rows discard stale incarnations and
their owned face resources. Queued update dialogue transports both identity
fields; none of this runtime-only state changes game-data or persistence
formats.

Every `EngineRuntime` owns a bounded `TacticalEntityDirectory`. In addition to
slot/incarnation liveness, a host can commit the latest public
`TacticalActorSnapshot` with `publishState` and retrieve it only through the
same exact identity with `state`; replacing or releasing an incarnation removes
its state atomically. This is host authority, not a package mutation service.
The JA2 host publishes creation and command results immediately and reconciles
remaining legacy animation/vitals changes at its completed-frame boundary.
`TacticalWorldService` is built from this committed pointer-free state rather
than exposing or rereading `TacticalActor`. The projection is runtime-only and
does not change soldier, map, save, content, or tactical-delta formats.

`TacticalEntityRoster` is the SDK's fixed-capacity ordered-membership
primitive for exact tactical identities. It preallocates its storage, inserts
into the lowest vacant slot without hot-path allocation, retains a sparse
high-water traversal bound, and never stores application pointers. `insert`,
`assign`, `erase`, `eraseAt`, and `replace` are identity-exact, while
`compact` and `sortByIdentity` provide allocation-free deterministic layout,
so reusable soldier slots cannot silently redirect retained membership to a
later incarnation. The JA2 application uses host-owned instances for active
and away tactical scheduling, bounded strategic squad membership, and fixed
vehicle passenger seats. Vehicle driver ownership and strategic movement-group
membership are exact as well. These remain application runtime details, not
package services. Strategic squad saves still write the established 40-by-10
legacy soldier-ID block. Vehicle saves retain their ten 32-bit passenger
profile IDs and 16-bit driver slot, while movement groups retain their count
plus 32-bit profile-only member payload. Their loaders reconstruct exact
runtime identities. No save, game-data, Lua, or network format changes.

`GameContext` also owns the application-only `Ja2SoldierRepository` that
connects this pointer-free runtime identity to JA2's current fixed soldier
records. The repository is not part of the SDK and does not expose
`TacticalActor` to packages. It centralizes bounded slot resolution,
whole-record creation/replacement, save/load access, and record swaps while
legacy consumers are migrated. Its fixed records and slot table are private to
`SoldierRepository.cpp`; no process-global storage declaration remains.
Strategic simulation and the other application domains resolve stable numeric
slots without relying on adjacent records. `SoldierID` itself no longer has
pointer conversion operators, so explicit repository resolution applies
unconditionally to TacticalAI, TileEngine, Ja2 composition/save handling,
Laptop, Utils, Editor, Lua, Multiplayer, and every Tactical translation unit.
Architecture checks reject retired global names and contiguous soldier-pointer
walks across production code and the headless harness. Numeric soldier slots
and every external data format remain stable.

The component notes below include historical v101 mapping details from the
staged migration. Baseline 1003 removed that mirror record and converter; those
details are provenance, not a supported load path. Current tactical-actor saves
use only explicit component visitors.

Inside the JA2 application, transient soldier state is also being separated by
behavior. Pending-action scratch and deferred work, combat-feedback counters,
and quick-item retention are owned by a resettable runtime component rather
than independent flat `TacticalActor` fields. This component is not exposed
through the SDK and is deliberately absent from soldier persistence.
`SoldierIdentityComponent` privately owns the application actor's slot,
fixed-width display name, body type, legacy and data profile links, exact
incarnation, and individual-militia identity. `SoldierRosterComponent`
separately owns activity, team and side, tactical-sector presence, soldier
class, and civilian group. Application code uses `identity()` and `roster()`;
packages continue to use pointer-free `TacticalEntityId` and snapshots rather
than importing either application component. Multiplayer creation records,
maps, profiles, XML, Lua, packages, and installed data do not change.
`SoldierVitalsComponent` privately owns the application soldier's complete
persistent health, breath, wound, and recovery lifecycle: current/max values,
previous-turn and fractional health, breath reduction, treatable injury and
surgery state, unrecoverable breath, critical-stat damage, bleed scheduling and
sound throttling, and the retired regeneration save slots. Named snapshot,
surgery, damage-recovery, life-deduction, and reset transitions keep the domain
coherent; the explicit serializer retains every established position and width.
`SoldierStatisticsComponent` separately owns the eleven persistent base
attributes and fixed 30-slot learned-trait array. Combat, AI, assignments,
character creation, and UI use the same reference-accessor surface rather than
a public legacy aggregate. The serializer retains the exact signed-byte
attribute sequence, the two historically interleaved health bytes, and all 30
unsigned trait bytes. v101 conversion maps every historical attribute and its
first two trait slots while clearing the 28 slots absent from that record.
Profile structures, trait rules, XML, Lua, multiplayer, installed data, and save
bytes do not change.
Live carried objects now have one private `SoldierInventory` owner. It keeps
the object slots, both new-item counter banks, key access, refresh request,
zipper flag, and drop-pack flag coherent and gives copied soldiers independent
storage. `InventorySlots` is the neutral transfer type used by creation, map,
and v101 records; a slot-only transfer cannot overwrite live inventory flags.
The established inventory stream—slot count, then each `OBJECTTYPE` and its two
`int` counters—is unchanged. Item XML, pocket layouts, profiles, maps,
multiplayer records, Lua, packages, and installed data keep their existing
formats.
The retired `STRUCT_Flags` does not gain a replacement generic bucket.
`SoldierStatusComponent` owns the established 32-bit status mask and supplies
explicit mask operations, while `SoldierInventory` owns key
access, new-item refresh, zipper, and drop-pack state. Target intent and
retention, reload and aim pauses, hit/death reactions, network refresh, gas
hits, and flank orientation live in their existing targeting, fire-control,
animation, replication, condition, and AI-planning domains. The serializer
retains all fourteen original positions and widths. v101 conversion maps its
twelve historical fields and clears the later zipper/drop-pack flags. No save,
profile, packet, map, XML, Lua, multiplayer, package, or installed-data format
changes.
The separately persisted optional key ring now has one inline
`SoldierKeyRingComponent` owner. Its fixed 255-slot `KEY_ON_RING` representation
and presence semantics are unchanged, but soldier copies and repository swaps
receive independent storage instead of sharing a raw heap pointer. Creation,
deletion, initialization, and current loading use explicit
activate/reset transitions. The outer soldier save adapter still emits the
same presence byte and fixed payload; content-facing key tables, items, maps,
XML, Lua, multiplayer, packages, and installed data do not change.
Temporary item actions now use `SoldierPendingItemComponent`. It uniquely owns
the copied `OBJECTTYPE` shared by give, drop, robot reload, placement, throw,
and launcher flows, while optional `THROW_PARAMS` live inline beside it.
Whole-soldier copies deep-copy the object, and named completion/cancellation
operations prevent interrupted actions from leaking or retaining stale
ballistic state. The component is runtime-only: both former pointer positions
remain zero-byte serializer landmarks, and multiplayer packets and all
content-facing formats remain unchanged.
Modular tactical-AI plan trees are likewise process-local and now belong to
`SoldierAiPlanComponent`. Because each plan points back to the exact
`TacticalActor` that created it, application record copies, repository
replacement, and slot swaps discard the plan instead of copying its address;
the selected factory recreates it lazily on the next AI decision. Initialization,
deletion, and loading also release it deterministically. This is an application
ownership boundary, not an SDK or package API. Plans are runtime-only and never
enter `XferTacticalActor`, so maps, XML, Lua, multiplayer packets, packages, and
installed data do not change.
Soldier routes now have a separate `SoldierStrategicPathComponent` ownership
boundary. Copying an application soldier clones its `PathSt` chain; moving or
swapping records transfers the node storage, and record reset/destruction
releases it. Strategic pathing remains an application adapter and vehicle or
militia path storage is unchanged, so this is not a new package data model or
SDK serialization surface. The former `pMercPath` field emitted no bytes in
the soldier visitor, and the outer save adapter continues to write the same
node count and semantic node fields. Loading constructs a temporary route and
publishes it only after the full payload succeeds.
The post-v101 extension banks remain deliberately separate from those former
general flags. `SoldierFeatureFlagsComponent` privately owns the unsigned
8-bit gunshot/explosion/X-ray event markers and both unsigned 32-bit 1.13
feature masks. Event, primary, and secondary query/set/clear operations expose
which bank a mod feature consumes while retaining the established flag
constants and zero-cost mutable references.
`SoldierServiceComponent` separately owns the persisted service marker, patient
provider count, provider-to-patient identity, and automatic-bandage medic
reservation, plus the inventory slot temporarily borrowed by an autonomous
medic. Named relationship and borrow/return transitions give tactical AI,
medical actions, and presentation one authority while guarding provider-count
removal from underflow. The face UI keeps only its old-value render cache, and
persistence retains all five established positions and widths.
`SoldierDialogueComponent` separately owns NPC quote planning, normal and
extended spoken-history masks, battle-voice selection and playback throttling,
bleeding/dying feedback, queued out-of-ammo speech, death-sound gates,
heard-noise speech cooldown, civilian quote progression, last-spoke time,
vocal volume, and corpse-comment tolerance. Named history, tactical-feedback,
quote-plan, cooldown, and playback transitions give tactical AI and dialogue
one authority while the portable serializer and v101 conversion retain every
original byte position.
`SoldierAudioComponent` separately owns footstep variation, remembered
door-opening noise, and burst, positional-ambience, and turret-turning sound
handles. Named record, start, clear, query, and reset transitions give weapon,
door, movement, and spatial-audio code one lifecycle boundary; stopped burst
handles are invalidated immediately and fresh handles use the explicit
no-sample sentinel. The serializer and v101 conversion retain all five raw
values at their original positions and widths.
`SoldierReplicationComponent` separately owns movement/update timestamps,
update sequence and kind, the scheduled synchronization stop, and the persisted
soldier checksum. Multiplayer and tactical overhead now record and age updates
through named wrap-safe transitions, while persistence records and verifies the
checksum through the same owner. The current serializer retains all seven
original positions and widths; v101 conversion maps its six established values
and clears the later scheduled-stop field.
`SoldierMovementMetricsComponent` separately owns the turn-start carried-weight
snapshot, turn movement distance, and realtime breath cadence plus its latest
movement animation. Tactical movement records these through one transition;
AP, agility, visibility, accuracy, suppression, medical, and breath rules read
the same owner. The signed and unsigned narrow distance counters saturate
instead of wrapping, while the serializer and v101 conversion retain all four
original positions, widths, and raw values.
`SoldierAiPlanningComponent` separately owns current/previous/queued actions
and payloads, action progress and target level, dominant facing, the fixed
patrol route and cursor, aim time, flank progress and geometry, sniper posture,
and modular plan selection. Tactical AI queues actions, records flank steps,
terminal progress, posture changes, and default plan selection through named
transitions shared by realtime and turn-based execution. The signed flank
counter saturates instead of wrapping; the serializer retains every original
position and width and v101 conversion maps the historical fields.
`SoldierAiBehaviorComponent` owns alert, disposition, orders, escort, creature,
realtime, and AI-flag modes. `SoldierAiCommunicationComponent` owns radio and
call exchange state, and `SoldierMoraleComponent` owns personal and calculated
morale channels, the delayed strategic modifier, and creature frenzy. These are
state seams for AI and mod code,
not replacements for existing policy, plan, XML, or Lua APIs.
`SoldierSkillStateComponent` separately owns repeated mechanical-check
identity and attempts, the AI's selected skill, fixed-capacity trait counters,
heterogeneous cooldowns, and the focus target. Named check, per-turn aging,
cooldown, focus, and reset transitions give tactical and AI code one authority.
The explicit serializer retains both 20-entry capacities and every established
position and width; v101 conversion maps its three skill-check values and
clears the later fields absent from that record.
`SoldierConditionComponent` separately owns temporary stat modifiers, nutrition
levels, starvation damage, the fixed 20-slot disease progress/flag arrays, and
the acquired-disability mask. Named effect, disease-flag, disability, and reset
operations give tactical, strategic, UI, and persistence code one authority.
Acquired-disability operations validate the persisted 1..32 bit domain before
using an unsigned shift. Disease rules and installed content stay outside this
owner, and the dependency-neutral disease-capacity header removes the former
`Disease.h`/`TacticalActor` include cycle. The serializer retains every original
position and width; v101 conversion clears this later domain.
`SoldierDrugStateComponent` separately owns the 20 persistent effect durations
and magnitudes, temporary personality and disability lifetimes, and accumulated
alcohol. Gameplay merges and ages effects, applies temporary traits, and
metabolizes alcohol through named operations; invalid effect identities are
rejected before indexing the fixed save capacity. The serializer retains all
20 unsigned 16-bit durations, all 20 signed 16-bit magnitudes, both unsigned
8-bit trait identities, both unsigned 16-bit lifetimes, and the 32-bit float
alcohol value in their exact existing order. v101 conversion clears this later
domain. This changes no drug XML, item definition, package, Lua, multiplayer,
installed-data, or save bytes.
`SoldierStatProgressComponent` separately owns all eleven persistent
stat-change timestamps and the value-gone-up direction mask. Gameplay records
a change by stat identity and marks or clears increase feedback through named
mask operations; tactical and strategic presentation code share its wrap-safe
queries. This retires `STRUCT_TimeChanges` and the loose `usValueGoneUp`
field without absorbing profile stat values. The serializer retains all eleven
unsigned 32-bit timestamp positions and the scattered unsigned 16-bit mask
position, while v101 conversion maps every raw value exactly. Food, water, and
explosion health damage now record the health timestamp rather than an
unrelated strength or dexterity timestamp.
`SoldierTimingComponent` separately owns all ten soldier-local countdown
timers and the AI/reload delay configuration. Gameplay starts, observes, and
clears timers by purpose; only the platform clock updater asks the owner for
mutable counters. This removes `STRUCT_TimeCounters` and soldier-local uses of
the legacy timer macros. The serializer retains the ten consecutive signed
32-bit counter positions and the established unsigned 32-bit AI-delay and
signed 16-bit reload-delay positions; v101 conversion maps all twelve values.
`SoldierLongActionComponent` owns the complementary extended-work lifecycle:
the tactical action kind, its retained context grid, and the AP cost remaining
across turns. The established grid slot also carries the return location while
an intel assignment temporarily removes a soldier from the tactical world.
Named begin, bounded AP consumption, context retention, completion, clear, and
reset transitions keep those values coherent. Tactical startup rejects unknown
action IDs and validates the requested action instead of stale prior state;
hack validation and result dispatch use the retained target grid consistently,
including the established skill-equals-difficulty success boundary.
The serializer retains all three original positions and widths.
`SoldierInteractionComponent` owns non-profile merchant identity, mutually
exclusive person, corpse, or structure dragging, and the reciprocal chat
partner. Named drag, copy, chat, clear, and reset transitions give tactical,
AI, placement, and persistence code one authority without owning the referenced
entities. Fresh soldiers use explicit no-corpse and no-structure sentinels, and
grid zero is handled as a valid structure location. The serializer retains all
five original scattered positions and widths; v101 conversion clears this
later domain.
`SoldierPendingActionComponent` owns the complementary persisted action plan:
the selected action, animation-transition count, its five polymorphic payload
values, door operation, queued-AI special data, and interruption marker. Named
begin, cancel, payload-clear, transition, and reset operations give tactical,
AI, tile, and persistence code one authority. Fresh soldiers start at the
explicit no-action sentinel, and transition counting saturates instead of
wrapping. Runtime-only target incarnation, path-search, launcher, and callback
scratch remain in `SoldierPendingActionRuntimeState`. The serializer retains
all ten original scattered positions and widths; v101 conversion maps the
complete historical domain.
`SoldierActionPointComponent` separately owns the current and turn-start
tactical AP budgets. Named turn setup, snapshot, and clear transitions keep that
pair coherent, while network reconciliation still uses the established
owner-authoritative packet field.
`SoldierCollapseComponent` independently owns tactical and breath-triggered
collapse, recovery turns, the sleep-drug counter, and strategic fatigue
collapse. Named transitions keep those related states coherent without
coupling the independent timers; old-save conversion and explicit persistence
retain all five established byte positions and widths.
`SoldierPerceptionComponent` separately owns view range, directional
movement-noise memory, personal noise grid/volume, smell values, heard-noise
elevation, blindness/deafness lifetimes, and X-ray source/activation time. Its
named operations preserve the exact sight-recovery edge and per-turn noise
cleanup. Render visibility remains separate. `SoldierAwarenessComponent` owns
current player-facing visibility, the last visibility consumed by rendering,
the fixed per-observer opponent table and counts, and movement distance used to
age stale knowledge. Its named visibility, fade, render-sync, discovery, and
forget transitions preserve the tactical state machine. Current
`SoldierCamouflageComponent` owns the applied and equipment-derived values for
all four established terrain families. Its bounded terrain and strongest-total
queries give line-of-sight and UI code one definition of effective camouflage,
while its applied-only total preserves camouflage-kit behavior. Existing item
fields and XML data remain the content-facing API. Current
`SoldierEmploymentComponent` owns live contract timing, mercenary
classification, deposits, insurance, renewal/dismissal bookkeeping, signing
eligibility, price-change acknowledgement, competing contract decisions, and
the hospital modifier. Laptop, strategic, tactical, AI, and persistence
adapters use the same owner; mercenary profiles and hire requests retain their
existing content- and command-facing structures.
`SoldierAssignmentComponent` separately owns current/previous duty, training,
time on assignment, squad-merge intent, repair targets, completion/idle status,
sleep and forced-wake state, fatigue feedback, and the facility, item-move, and
mini-event context belonging to that duty. Strategic travel and sector location
remain independent; existing assignment constants and gameplay entry points
are unchanged. `SoldierDeploymentComponent` owns the complementary
location boundary: strategic sector, movement group, vehicle, tactical
insertion, traversal origin, off-world staging, between-sector transit,
mission-exit participation, landing-zone arrival policy, arrival bookkeeping,
and the Unfinished Business helicopter arrival get-up timer and phase flags.
Strategic route objects remain adapters, while strategic groups are referenced
by `groupId()`—the sole soldier-side strategic group identity—and resolved
through the strategic group repository instead of being cached as a second
per-soldier pointer. All existing sector,
transit, insertion, vehicle, arrival, and arrival get-up gameplay entry points
continue to operate on the same values. `SoldierVehicleStateComponent`
separately owns a vehicle soldier's tactical `VEHICLETYPE` record index and a
remote robot's typed controller identity. The tactical record index is not the
strategic passenger-membership ID exposed by `SoldierDeploymentComponent`;
both retain their established meanings and adapters. Vehicle definitions,
creation records, and repository APIs remain unchanged.
`TacticalActorRobotics` exclusively resolves and refreshes the remote
controller link at runtime. It bounds repository and team scans, validates
remote equipment and profile indexes, and requires compatible sector/transit
state before returning a controller. Gameplay callers therefore cannot treat
an unchecked persisted slot as a live actor; existing robot, item, profile,
assignment, and save formats remain unchanged.
`TacticalActorMobility` is the runtime boundary for water classification,
movement-mode validation, current-stance movement animation selection, stance
transitions, backpack climbing, structure-aware stance validation, adjacent
cover, and disease-limited fast movement.
Callers can query current-animation movement and stance validity without
indexing the legacy animation table themselves.
`TacticalActorWeaponHandling` separately owns dual-wield and paired-burst
eligibility, alternative fire posture, and mounted-weapon queries. Both
domains validate the legacy indexes they consume before consulting animation,
item, weapon, profile, vehicle, seat, direction, or world tables. Mods keep
using the existing item, weapon, vehicle, map, XML, and Lua data formats; this
change only replaces C++ aggregate methods with explicit engine-facing
operations.
`TacticalActorEquipment`, `TacticalActorModifiers`, and
`TacticalActorAssignments` are also independent compiled application domains.
Use the equipment boundary for effective weapon selection, inventory cooldown,
external feeding, hand-item changes, flashlights, sector drops, and riot
shields; use the modifier boundary for background-backed and derived combat or
perception values; use the assignment boundary for sleep recovery and burial,
construction, administration, or exploration productivity. Their definitions
must not be placed back in `Soldier Control.cpp`. Equipment and background
lookups reject malformed item, inventory, profile, or background indexes, and
sleep recovery rejects an absent profile or non-positive maximum health before
calling legacy profile rules. These are C++ ownership boundaries only and do
not change installed content or persistence formats.
Use `TacticalActorCovertOps` for disguise, recognition, exposure, and spy
productivity; use `TacticalActorDragging` for drag eligibility and drag-order
lifetime; and use `TacticalActorSpotting` for spotter preparation and the
team-assisted chance-to-hit bonus. These are independent compiled domains, so
their definitions must not return to `Soldier Control.cpp`. Dragging validates
actor animation, grid, level, direction, corpse, and target state before world
table access, while covert spy calculations reject malformed strategic
sectors. The move changes C++ ownership and validation only; existing uniform,
item, corpse, profile, map, persistence, network, XML, Lua, and installed mod
formats remain compatible.
Use `TacticalActorSkills` for trait-skill eligibility, execution, and denial
text; use `TacticalActorRadio` for radio equipment, artillery, operating modes,
reinforcements, and team signal queries; use `TacticalActorDisease` for infection
and derived disease rules; and use `TacticalActorTurncoats` for conviction and
activation. These four domains are independent compiled implementation units,
and their definitions must not return to `Soldier Control.cpp`. Their boundaries
retain the established application adapters and validate malformed skill, item,
sector, disease, profile, target, animation, and radio-configuration input before
unsafe legacy lookups. This completes physical isolation of the existing domain
namespaces without changing installed content or persistence formats. The
aggregate constructor/destructor, complete reset, and compatibility name lookup
are likewise compiled from `TacticalActor.cpp`, so no `TacticalActor::`
definition remains in `Soldier Control.cpp`. Include `TacticalActor.h` when a
complete application actor aggregate is required; `Soldier Control.h` keeps a
compatibility include but no longer owns the class declaration. Actor domain
implementations likewise include the focused aggregate and their explicit
collaborators rather than directly including the legacy facade.
`TacticalActorRangedActions` owns the stateful ranged-weapon lifecycle that
uses those rules: `beginFire`, the `ready`/`readyToward`/`readyFacing`
operations, and `refreshAfterHandItemChange`. Event handling, tactical AI,
animation playback, UI inventory changes, and weapon code call this domain
instead of restoring `TacticalActor::EVENT_FireSoldierWeapon`,
`SoldierReadyWeapon`, `InternalSoldierReadyWeapon`, or the hand-item animation
refresh façade. The boundary rejects unavailable tactical worlds and malformed
actor, grid, direction, animation, inventory, item, or weapon indexes before
legacy state is touched. This is an application-side C++ boundary only;
weapon, attachment, animation, sound, network-command, map, XML, Lua, save,
and installed mod formats are unchanged.
`TacticalActorWorldPlacement` is the application-side boundary for placing a
live actor in the tactical world. Use `removeFromGrid`, `setPosition`,
`setGrid`, and `setHeight`; do not restore the aggregate position, grid,
height, or force-delete façades, or the former long-named global position
wrapper. These operations synchronize precise and projected coordinates,
route destinations, merc-layer nodes, animation footprints, lighting, vehicle
passengers, terrain, reservations, and adjacency bookkeeping. They reject an
unavailable tactical world, malformed actor/animation/level/grid state, and
non-finite coordinates or heights without partial mutation. This boundary
does not change maps, schedules, animation data, network events, saves, Lua,
or installed content formats.
`TacticalActorRouteExecution` is the application-side boundary for executing
live tactical routes. Use `requestPath`, `continueMovement`,
`setOutOfActionPoints`, `stop`, `stopAt`, `settleIntoStationaryStance`, and
`haltForSighting`; do not restore the former aggregate path, no-AP, stop,
stationary-stance, sighting-halt, or global continuation façades. These
operations coordinate path buffers and destinations, pending
actions and animations, medical-service cancellation, movement reservations,
UI ownership, vehicle movement sound, and network path/stop replication. They
reject an unavailable tactical world and malformed actor, animation, body,
level, grid, direction, path-buffer, movement-animation, origin, or vehicle
state before mutation. This C++ boundary does not change pathfinding, network
commands, maps, schedules, saves, the `ActionStopMerc` Lua API, or installed
content formats.
`TacticalActorOrientation` is the application-side boundary for live stance
and facing transitions. Use `changeStance`, `setMovementDestination`,
`setDesiredDirection`, `setDirection`, and `advanceTurn`; do not restore the
former aggregate destination, facing, stance, and turn façades or the global
desired-direction and multi-tile-turn helpers. These operations coordinate AP
and breath costs, prone transitions, animation footprints, structures,
flashlights, sight/UI ownership, vehicle sounds, and passenger rotation. They
reject an unavailable tactical world and malformed actor, animation, stance,
grid, direction, movement, queued transition, high-resolution facing,
hit-turn, passenger, or vehicle state without partial mutation. This C++
boundary does not change animation data, pathfinding, network messages, replay
commands, maps, schedules, saves, Lua APIs, or installed content formats.
`TacticalActorConditionPresentation` is the application-side boundary for
localized food, disease, and sleep text. Use `appendSummary` when composing a
complete tactical or strategic status tooltip, or the focused append
operations for a single condition domain. New callers must not restore
`TacticalActor::PrintFoodDesc`, `TacticalActor::PrintSleepDesc`, or disease
presentation on `TacticalActorDisease`; that domain continues to own rules and
magnitude only. The presentation boundary rejects null destinations, vehicles,
and invalid profiles before profile-dependent evaluation, without changing
localization, food/disease content, assignment, XML, Lua, save, or network
formats.
`TacticalActorAiBehavior` exposes the corresponding bounded operations for AI
ownership, initial-turn AP, flanking, animation-specific merc cower teardown,
retreat cadence, radio animation, and boxing cleanup.
`TacticalActorLongActions` owns the
fortify/remove/hack begin-update-cancel lifecycle, including validation before
legacy animation, inventory, structure, and cost tables are read.
`TacticalActorPrisonerOperations` owns strategic prisoner-processing
eligibility and tactical adjacent release. These are application-side
engine-facing domains, not new package or content schemas: existing maps,
facility data, items, XML, Lua, art, audio, and installed mod data continue to
use their current formats. New C++ code should call these domains instead of
adding behavior back to the `TacticalActor` aggregate.
`TacticalActorMedicalServices` is the explicit boundary for enemy-AI treatment
and medic/patient service teardown. It rejects unavailable worlds and malformed
actors, animations, directions, kits, targets, and repository links, and it
repairs stale provider bookkeeping while cancelling a service.
`TacticalActorDamageQueue` owns replaceable deferred damage, exactly-once
resolution, and cancellation. New code should schedule or resolve delayed
damage through this domain rather than storing another actor-level callback or
restoring the retired aggregate methods. Neither domain introduces a content
package or changes installed game-data formats.
`TacticalActorMedicalTreatment` owns live tactical treatment, abstract
auto-bandage/auto-resolve treatment, damaged-stat totals, and stat restoration.
Callers pass actor, patient, and kit references through this boundary; they
must not recreate the retired `SoldierDressWound`,
`VirtualSoldierDressWound`, `NumberOfDamagedStats`, or
`RegainDamagedStats` entry points. The domain bounds malformed kit, animation,
action-point, profile, and surgery-consumption state without changing medical
item, trait, XML, map, Lua, or other installed content formats.
`TacticalActorMedicalSession` owns the bounded first-aid AP cost, initiation,
and the transition back into the providing-aid animation. New callers use
`beginActionPointCost`, then pass a medic, patient grid, and direction to
`beginFirstAid`; malformed world, grid, animation, inventory, item, profile,
or repository state is rejected before a service relationship is established.
`resumeProvidingAnimation` is the sole stationary-stance handoff for an active
provider. The existing item definitions, trait values, dialogue and Lua
events, maps, animations, and installed content formats are unchanged.
`TacticalActorFieldOperations` is the actor-to-world boundary for fence
cutting, repair, refuelling, corpse blood collection, door alarms,
fortification, interactive structures, robot reloading, and window breaking.
New callers pass the actor and validated intent to this domain instead of
reintroducing aggregate `EVENT_Soldier*`, `BreakWindow`, `CanBreakWindow`, or
the global `DoInteractiveAction` entry points. The domain rejects unloaded
worlds and malformed actor, grid, direction, item, target, or structure-index
state before accessing legacy tables. It does not change installed item,
map, XML, Lua, sound, animation, art, or other content formats.
`TacticalActorCombatActions` owns blade, punch, and throwing-knife initiation.
`TacticalActorExplosives` owns bomb placement, tripwire disarming, detonator
use, self-detonation, and inventory-explosion effects. New callers use these
bounded domains instead of restoring the retired aggregate `EVENT_Soldier*`
methods. Both reject unavailable worlds and malformed actor, target,
animation, inventory, item, or world-item state before consulting legacy
tables; weapon, explosive, map, XML, Lua, animation, and installed content
formats remain unchanged.
Martial-arts animation continuation also enters
`TacticalActorCombatActions`; callers must not restore `DoNinjaAttack`.
`TacticalActorCombatReactions` owns the aggregate cower-flag transition, fall
intent, and bounded fallback/flyback path setup. Tactical AI and civilian
stance handling use `setCowering`; animation-specific merc cower teardown
remains in `TacticalActorAiBehavior`. Hit resolution, collapse handling, and
animation playback use `beginFall`, `beginFallback`, or `beginFlyback` rather
than writing posture, route, and fall state through the aggregate. These
operations validate world lifetime, actor, target, body, animation, direction,
level, grid, movement-cost, route, and face state. Robots reject cower
requests, and repeated requests for the current cower posture are accepted
without restarting the animation. Combat rules, maps, movement costs,
animations, audio, XML, Lua, and network formats remain unchanged.
`TacticalActorAnimationSelection::selectFire`, `selectFall`, and
`pickReady` are the application-side animation-choice boundary. Tactical
orientation, ranged actions, route settlement, UI, points, overhead, and
tactical AI call this contract instead of declaring the former global
selection helpers. Empty hands and malformed body, animation, inventory, item,
or stance values resolve to a neutral result before legacy animation, item, or
weapon tables are indexed.

`TacticalActorDamageFeedback` is the complete immediate hit-presentation
boundary. In addition to `presentHit`, it owns scream-volume calculation,
generic and weapon-specific reaction selection, uniform-damage flags, and the
floating damage-display cursor. Damage resolution, vehicle feedback, and
overhead presentation call this contract instead of restoring
`CalcScreamVolume`, `DoGenericHit`, `SoldierGotHit*`, or
`SetDamageDisplayCounter`. The domain preserves the one-second grunt throttle,
the tactical-screen in-sector flash rule, hit animations, lethal-hit uniform
damage, and the active damage-cursor restart behavior. It validates body,
animation, item, direction, profile, NPC sound-set, and face-registry indexes
before reaching legacy tables. Animation and damage rules, battle sounds,
portraits, UI timing, uniform flags, XML, Lua, saves, and network formats remain
unchanged.
`TacticalActorProfileClassification::profileTableIndex` is the bounded
application-side entry point for selecting the optional generated enemy or
militia profile table. Actor creation and display-name resolution call it
instead of restoring `TacticalActor::GetSoldierProfileType`. The operation
returns the established enemy indexes 0..2 or militia indexes 3..5 only for an
enabled matching team/class pair; disabled tables, unrelated teams,
cross-team classes, and malformed class values return -1. Soldier-profile XML,
table ordering, generated identities, palettes, traits, names, saves, Lua, and
network formats remain unchanged.
Reset the actor's condition, stat-progress, or transient runtime domain through
`condition().clearExtraStats()`, `statProgress().reset()`, or
`runtime().reset()` at the owning lifecycle boundary. Turn start, clock-wrap
recovery, actor creation/reuse, and post-load cleanup use those component APIs
directly; new code must not restore `ResetExtraStats`,
`ResetSoldierChangeStatTimer`, or `InitializeExtraData` on `TacticalActor`.
These operations do not change save bytes, content formats, or campaign rules.
`TacticalActorRecovery` owns sleep-dart application, breath-collapse
detection, collapse execution, and get-up progression. New callers use
`applySleepDart`, `checkBreathCollapse`, `collapse`, or `beginGetUp` instead of
restoring `SleepDartSuccumbChance`, `SoldierCollapse`,
`CheckForBreathCollapse`, or `BeginSoldierGetup`. The boundary validates the
live tactical world plus actor, profile, body, animation, direction, level,
grid, and get-up structure state while preserving combat/fatigue rules and
installed item, map, animation, dialogue, audio, XML, Lua, and network
formats.
`TacticalActorInteractions` owns resolved conversation initiation and
civilian, militia, trader, surrender, volunteer, and NPC dialogue routing in
addition to chat teardown, item-transfer animation routing, person-to-person
item giving, handcuffing, equipment/consumable application, blood collection,
and splint application. Pickup and drop callers use `beginItemTransfer`, which
defers a standing actor through `DROP_ITEM` and completes crouched or prone
transfers immediately. New callers use `startConversation` and the other
bounded operations instead of restoring `PlayerSoldierStartTalking`,
`HandleVolunteerRecruitment`, `PickDropItemAnimation`, or the retired aggregate
`EVENT_Soldier*` methods. Delayed player intent remains a stable simulation
command until execution resolves both actor identities.
The boundary revalidates the live world, actor and target locations,
directions, animation state, profiles, dealer/sector/town indexes, inventory
stacks, item IDs and flags, and repository identity after movement. Installed
item definitions, traits, maps, XML, Lua/dialogue behavior, animations, and
other content formats are unchanged.
`TacticalActorLighting` owns personal player-merc light creation, recreation,
destruction, positioning, and the personal shade applied to the actor's render
node. New callers use `createPersonalLight`, `recreatePersonalLight`,
`destroyPersonalLight`, `positionPersonalLight`, or `setPersonalLightLevel`
instead of restoring the retired aggregate light methods,
`ReCreateSelectedSoldierLight`, or `SetSoldierPersonalLightLevel`. The domain
rejects unloaded worlds and malformed actor, profile, animation, inventory,
attachment, item, grid, level, projected-position, or light-handle state before
consulting legacy tables; destruction safely clears malformed handles and is
idempotent. Existing lighting templates, vision rules, maps, settings, render
behavior, XML, Lua, art, and installed content formats remain unchanged.
`TacticalActorTurnBudget` owns each actor's calculated per-turn AP grant and
the carry, cap, snapshot, and no-AP refresh transition. New callers use
`calculateTurnGrant` for the per-turn grant and `refreshForTurn` when starting
a turn; they must not restore `CalcActionPoints` or `CalcNewActionPoints` on
the aggregate. The boundary rejects malformed body, team, profile,
difficulty, health, inventory, item, stack, attachment, or tactical-vehicle
state before table access. `SoldierActionPointComponent` remains the canonical
storage owner, and existing AP constants, traits, difficulty settings, drugs,
items, maps, XML, Lua, save, and network formats remain unchanged.
`TacticalActorTurnMaintenance::maintainAtTurnStart` is the application-side
entry point for expiring per-turn actor flags, aging counters and cooldowns,
ending transient muzzle/radio state, resolving covert exposure, handling robot
X-ray readiness, and recording battle participation.
`TacticalActorTurnLifecycle::beginTurn` is the complete turn-start entry point
and calls this focused maintenance operation; new code must not restore
`EVENT_BeginMercTurn` or `SoldierPropertyUpkeep` on the aggregate. The
boundaries validate robot utility storage and item IDs before item-table access
while preserving established turn timing, covert, radio, incident, content,
save, and network behavior.
For the remaining application behavior, include and call the focused boundary:
`TacticalActorLifecycle::{create,destroy,revive}` for actor resource lifetime,
`TacticalActorAppearance::rebuildPalettes` for transactional render palettes,
`TacticalActorAnimationTransitions::{changeState,initializeAnimation}` for
animation events, `TacticalActorDamageResolution::{applyHit,takeDamage}` for
combat damage, `TacticalActorBattleSounds::{play,playWithCode}` for merc audio,
`TacticalActorLocomotion::{checkRoofHit,move}` for live movement, and
`TacticalActorVisibility::{maximumDistance,distance}` for bounded sight-range
queries. Initialize directional sight state through `initializeRanges`; use
`straightRange`, `normalMaximumDistance`, `hasLimitedVision`, and
`adjustForEnvironment` instead of reading or reproducing visibility globals.
Do not reintroduce the former `CreateSoldier*`, `DeleteSoldier`, `ReviveSoldier`,
`ChangeSoldierState`, `EVENT_*`, `SoldierTakeDamage`, `DoMercBattleSound`,
`CheckSoldierHitRoof`, `MoveMerc`, `GetMaxDistanceVisible`, `InitSightRange`,
`DistanceVisible`, or the other former global sight-range entry points.
`TacticalActor` now serves as the component aggregate; only `initialize()` and
compatibility `GetName()` remain as member behavior, with their definitions and
the constructor/destructor isolated in `TacticalActor.cpp`. Its declaration and
inline component accessors are isolated in `TacticalActor.h`; new focused actor
code should include that header instead of the broad legacy soldier-control
surface. Architecture CI rejects direct `Soldier Control.h` includes from every
production implementation under Editor, Ja2, Laptop, modular AI, Multiplayer,
Strategic, Tactical, TacticalAI, TileEngine, Utils, and SGP. The former
`Soldier Control.cpp` implementation is retired from both the source tree and
the tactical build. The facade contains only includes and comments; the
boundary checker rejects any declaration, value, alias, or behavior added to
it, as well as restoration of the implementation file. Pointer-only legacy
application APIs also
forward-declare `TacticalActor` rather than importing that facade; their
standalone compile guard covers laptop, strategic, tactical, and tile-engine
headers. This now covers the complete application-header surface: a second
isolated compile matrix builds each of the final 30 service/API headers in its
own translation unit and verifies that `TacticalActor` remains incomplete. A
second matrix compiles all 35 focused compatibility/schema headers standalone;
compile-time assertions pin their stable values and layouts.
Use `Strategic Path Types.h` for the stable `PathSt` route node,
`Soldier Patrol Types.h` for map-placement patrol capacities, and
`Soldier Profile Constants.h` for the reserved `NO_PROFILE` sentinel. The
palette contract is available through `Soldier Palette.h`, stable background
bits through `Soldier Background Types.h`, battle-voice indices through
`TacticalActorBattleSounds.h`, serialized status/feature masks through
`TacticalActorStateFlags.h`, and employment codes through
`TacticalActorEmploymentTypes.h`. Animation intent and hit-transition sentinels
are in `TacticalActorAnimationState.h`; pending interactions and throw actions
in `TacticalActorPendingActionTypes.h`; stop reasons in
`TacticalActorMovementState.h`; and quote-history and blood schema in
`TacticalActorQuoteFlags.h` and `TacticalActorBloodState.h`. Skills, damage
reasons, long actions, interrupts, and replicated event wrappers stay with
their focused actor domains. Use `Animation Data.h` for animation-profile
layouts, `Grid Direction.h` for direction helpers, `Soldier Drug Types.h` for
persistent drug-effect indices/capacity, `Soldier Stat Types.h` for stable stat
change masks/timing, `Taunt Types.h` for external taunt bits, and
`TacticalDestinationTypes.h` for occupancy-query modes. Transient event and
gas-hit bits remain in `TacticalActorStateFlags.h`. Use
`TacticalActorPredicates` for civilian/militia, posture, and attack-neutrality
queries; it bounds animation state before consulting animation tables. Debug
validation, crow behavior, replicated-event translation, and the legacy
facing-movement adapter are implemented by the focused
`TacticalActorDebug`, `TacticalActorCrowBehavior`, `TacticalActorEvents`, and
`TacticalActorLocomotion` sources. Direction helpers compile from
`Grid Direction.cpp`; palette loading and replacement compile from
`Soldier Palette.cpp`; profile gear/record lifecycle and legacy conversion
compile from `Soldier Profile Records.cpp`. `Soldier Palette.h` owns uniform
IDs, clothing-palette records, and palette contracts. Front-arc, spread-target, and
unblit capacities alias their owning components, civilian-name capacity lives
in `Civ Quotes.h`, and bandaged-health calculation is available through
`TacticalActorConditions::bandagedAmount`. Player-team revival and sound
preloading are owned by `TacticalActorLifecycle` and
`TacticalActorBattleSounds`; their legacy adapters are declared by those same
focused headers. The player light-option
refresh is declared
with the personal-light operations in `TacticalActorLighting.h`. The legacy
`Soldier Control.h` facade re-exports those contracts, while
`Utils All.h` remains the only intentional public compatibility umbrella;
architecture CI rejects direct facade imports from all other application
headers. Include `Soldier Class.h` for stable soldier-class codes and
enemy/militia classification without importing actor storage or soldier-control
operations. Visibility requests with
unavailable world storage or malformed actor, grid, level, direction, vehicle,
or light state fail without legacy table access. These C++ boundaries do not
alter save bytes, network or animation event formats, combat/content data,
visibility rules, status/feature masks, employment codes, palettes, audio,
maps, XML, or Lua APIs.
`TacticalActorAnimationFrames` owns directional animation-surface mapping,
animation-code-to-render-frame selection, and the fixed frame used for frozen
actors. New callers use `spriteDirectionForSurface`, `selectFrame`, or
`frozenFrame` instead of restoring `SpriteDirForSurface`,
`ConvertAniCodeToAniFrame`, or `CryoAniFrame` on the aggregate. The domain
rejects malformed animation-state, surface, world-direction, extended-facing,
and video-object frame metadata before indexing legacy animation tables.
Existing animation scripts and assets, rendering behavior, maps, XML, Lua,
save, and network formats remain unchanged.
`TacticalActorAnimationTiming` owns dynamic and fixed animation-delay refresh,
team turn-speed factors, tactical speed adjustment, and fast-turn timing. Use
`refresh`, `currentTeamSpeedFactor`, and `adjustForFastTurn` rather than
restoring the former free timing helpers. `TacticalActorAnimationGeometry`
owns current-frame dimensions and offsets and applies them transactionally to
the actor render bounding box through `currentFrame` and
`refreshBoundingBox`. Both services bound the animation, body, terrain,
direction, inventory, surface, video-object, and frame indexes they consume;
failed geometry requests do not partially replace the existing render box.
Use `TacticalActorMobility::isValidMovementMode` and
`selectMovementForCurrentStance` for the remaining movement adapters, and
`TacticalActorRouteExecution::continueMovement` for AP-checked route
continuation. `RenderPaletteEffects::populateActorShades` is the shared
actor-independent palette-effect generator used by actor appearance and
logical body palettes; callers must first provide the base 8-bit and converted
base palette. The old enemy-glow creators and the unused full-tile occlusion
implementation are retired. These are C++ ownership and validation changes
only; animation data, palette sources, rendered output, maps, items, saves,
network events, XML, Lua, and installed mod formats remain compatible.
The final APIs formerly implemented in `Soldier Control.cpp` now belong to
focused services. Use `TacticalActorBleeding::{check,nextInterval,
nextUnmovingInterval}` for bleeding, `TacticalActorMovementAudio` for vehicle
and footstep audio, `ResolvePendingInterrupt` from
`TacticalActorInterrupts.h`, `TacticalActorAiBehavior` for new-situation and
hip-fire stance policy, and `TacticalActorAnimationSelection` for big-merc and
suspicious-action selection. Consumable application, gas-mask inspection,
pickup and stealing, trait classification and nearby squad-leader counting,
and roof markers belong to `TacticalActorConsumables`,
`TacticalActorEquipment`, `TacticalActorInteractions`,
`TacticalActorSkills`, and `TacticalActorWorldPlacement`. Existing global
spellings remain as adapters declared by those focused contracts; new code
should use the namespace operations. The force-animation and
path-through-people globals live with animation transitions and route
execution, and the health/strength threshold table is provided by
`Soldier Stat Types.h`. Each contract is standalone-compiled and its malformed
inputs are covered by the headless tests. These ownership moves do not change
save, network, campaign, content, map, item, animation, audio, XML, Lua, or mod
formats.
`TacticalActorAnimationFootprint` owns the tactical-world placeholder tiles
described by an actor's animation profile. New callers use `add`,
`addForSurface`, `remove`, `flagsAtGrid`, and `nextWorldNode` instead of
restoring `HandleAnimationProfile`, `GetProfileFlagsFromGridno`, or
`GetAnimProfileFlags`. The boundary rejects unloaded or inconsistently sized
world storage and malformed animation, body/inventory resolver, surface,
signed profile index, direction, profile-tile storage, and projected-grid
state. Off-map profile tiles are skipped as before, and add failure rolls back
already-created placeholders. Existing animation-profile binaries,
hit-location flags, cursor behavior, maps, rendering, XML, Lua, save, and
network formats remain unchanged.
`TacticalActorTraversal` owns roof ascent and descent, fence and window jumps,
and wall-climb initiation. Player, tactical-AI roof/window, and moving-path
fence producers all emit `TraverseObstacleCommand`; only the compatibility
executor enters the bounded domain. The original actor/kind/source codec prefix
is retained. Player intent uses sentinel-only appended state, while retained
System/Replay forms capture origin, continuation, event policy, exact issued
grid/level/direction/animation, issued AP/breath costs, bounded route state, and
a deterministic fingerprint of every actor field the retained continuation
can clear. Changed incarnations, state, routes, point budgets, or cost results
discard before mutation. Because AI actions do not yet expose a generation
token, stale traversal fails closed without calling `ActionDone`; an
indistinguishable identical-state ABA remains explicit follow-up debt, while
the expected post-ingress `actionInProgress` transition is excluded from the
fingerprint. Playback suppresses both nested and later per-frame duplicate path
production before either fence AP branch, so a queued Replay command owns even
a locally divergent no-AP outcome and cannot reflect its stance, stop, or
AI-completion/interrupt traffic to peers; deferred roof animation completion
carries that policy in actor-local runtime state until it is consumed. New code
must not restore the retired
`BeginSoldierClimb*` aggregate methods or call the traversal domain outside the
command executor. Obstacle discovery, AP costs, blocked-tile waiting, and
animation completion remain JA2 executor mechanics. The domain validates world
lifetime, actor, body, animation, route, direction, level, destination,
occupancy, and action-point state without changing maps, structures, animation
data, AP settings, saves, XML, Lua, or legacy multiplayer packet formats.
`SoldierScheduleComponent` owns live NPC schedule identity,
action progress, and the door continuation phase/grid shared by strategic
scheduling and tactical movement. Named transitions atomically begin,
complete, consume, or cancel the door continuation; editor placements,
schedule nodes, and creation/network records keep their established public
formats. Precise and integer-projected world coordinates, turn-start
coordinates, initial/current grid, elevation and facing, current/desired
height plus interpolated animation height, temporary animation grid, room, and
terrain history are privately owned by `SoldierPositionComponent` as one
persistent storage domain.
Zero-cost reference accessors remain available to application hot paths,
while named coordinate and terrain transitions keep paired values coherent.
Old-save conversion and explicit persistence retain every established field
position. `SoldierFrontArcComponent` separately owns the fixed three-direction
occlusion overlay. Each tile index is bound to or cleared with the grid that
owns its topmost node, preventing the former parallel arrays from diverging
while retaining their exact save schema. `SoldierMovementHistoryComponent`
separately owns the last departed
grid and the bounded two-location AI loop memory. Named departure, AI reset,
observation, and full-reset transitions retain the original world-bound
oscillation behavior without confusing history with current placement. The
remaining fixed-capacity tactical route now follows the same rule through
`SoldierPathingComponent`: destinations, cursor, directions, lookup flags, and
blacklist have one private owner and reset boundary instead of a public
`STRUCT_Pathing`. Its reference accessors preserve hot-path
mutation while the portable serializer retains the established byte sequence.
`SoldierMovementComponent` now owns the complementary route-execution domain:
the selected movement-animation mode, stealth/reverse intent, high-resolution
current and desired facing, animation direction and grid-update policy,
delayed-tile counters and causes, movement reservation, merc contention,
scripted and continued destinations, stop reason, and coordinated speed
override. Tactical turn ownership, prior water state, UI speed, AP exhaustion,
pause state, movement-clock/network-delay flags, presentation motion, and
destination-center crossing, plus staged strategic-exit waits live there as
well. UI, AI, animation, rendering, pathing, and simulation-command adapters
all use `movement()` as the one authority. Named operations cover intent
changes, synchronized extended facing, grid-update suppression, turn and pause
lifecycles, water/UI-speed edges, strategic-exit waits, and paired destination
crossing instead of independently mutating generic flags.
`SoldierTurnStateComponent` owns scheduler movement state, interrupt duel
points/result/start AP, the pre-interrupt movement snapshot, and the fixed
per-opponent interrupt counters. Temporary interrupt ownership therefore
captures and restores one explicit state seam without exposing flat fields.
`SoldierTargetingComponent` owns the selected target grid, elevation, cube
level, previous target grid, selected target soldier, engaged opponent, and
cached line-of-fire target identities.
The application UI, AI, weapons, simulation-command, animation-event, and
multiplayer adapters use this one private owner; packages still receive only
stable pointer-free tactical identities and snapshots.
`SoldierAttackSelectionComponent` owns the selected attacking hand and weapon,
weapon and scope modes, and ranged and melee body locations. It is distinct
from target geometry and from the later mutable firing sequence, so every
producer chooses how to attack through one boundary.
`SoldierMeleeApproachComponent` owns the cached melee path key, cost, and
terminal direction. Movement invalidates that cache through its named
operation, while the historical partial-cache behavior remains intact.
`SoldierFireControlComponent` owns that mutable firing sequence and its
persistent configuration: editor gun archetype, grenade-launcher delay mode,
selected multi-barrel mode, burst and autofire progress, bullets in flight, the
one-based spread cursor and six fixed spread targets, recoil and counterforce
history, initial muzzle offsets, the autofire UI edge state, the active
multi-barrel cursor, and burst-drag start/end grids. Real-time and turn-based
input share named drag transitions. Named single-shot, burst, autofire,
launcher-delay, and barrel-selection transitions keep these modes consistent.
Target selection remains separate, as do presentation-only sound and
muzzle-flash handles. AI dual-wield spread generation is clamped after
doubling its shot count, so it cannot write twelve locations into the
established six-target buffer.
`SoldierCombatResultComponent` owns incoming attacker history, hit
location/reason, per-turn hit and pellet counts, accumulated damage, and the
outgoing last-attack-hit result consumed by AI. Named history operations
preserve killer and assister attribution as one transition.
`SoldierDamageDisplayComponent` separately owns the floating-number cursor,
screen offset, and direction. Accumulated damage stays in the simulation
component because existing torso-hit and death rules consume it; render
coordinates cannot leak into those rules.
`SoldierRenderStateComponent` owns soldier-local rendering values: the five
palette-replacement identities, fade mode/level/origin, forced colour and shade
policy, muzzle-flash visibility and light handles, the unblit rectangle, and
projected bounds. Use `renderState()` and its named fade, flash, shade, redraw,
and light-lifetime operations from tactical graphics adapters.
`RenderPaletteBank`, available through `palette()`, owns the generated 8-bit
base, 16-bit base, lighting, glow, and effect tables and tracks the active and
forced aliases. It provides deep-copy, transfer, transactional replacement,
registry cleanup, and reset semantics for both soldiers and composed
logical-body palette tables. Lighting accepts this narrow owner directly;
logical palette data therefore no longer masquerades as a `TacticalActor`.
The installed `.col` palette format is unchanged. Surface, level-node, and
background pointers remain with the legacy graphics adapter and are not
component or package API.
`SoldierUiPresentationComponent` owns the remaining pointer-free tactical view
model for a soldier: portrait animation and flash phase, locator animation,
cycle and visibility, interface elevation, panel placement and lifecycle,
merc-panel requests, first-time AP/unconscious notifications, planned-action
overlay, and enemy cycling. Use `uiPresentation()` or its named locator, panel,
notification, and planned-target transitions from UI adapters. Render-resource
pointers deliberately remain behind the legacy graphics adapter rather than
entering this view model.
`SoldierCombatContributionComponent` separately owns outgoing militia kills,
assists, promotion points, and the fixed 156-slot player-team damage
attribution record. Named accrual and transfer operations coordinate tactical
combat, autoresolve, and militia persistence; counters saturate instead of
wrapping. The serializer retains all three original scattered positions and
widths, while v101 conversion preserves every historical attribution slot.
`SoldierSuppressionComponent` owns under-fire aging, shock, per-attack
suppression points, accumulated AP loss, suppressor identity, and close-call
feedback. Combat, tactical AI, explosions, and turn transitions use the same
hostile-fire boundary; named operations coordinate bullet attribution and
clamp AP loss without changing the established suppression-point behavior. The
`SoldierAnimationIntentComponent` owns the next
persistent domain: desired stance height, both queued animations, queued stance
and facing, UI turn origin, next-tile stopping, and the post-stance continuation
mode. `SoldierAnimationPlaybackComponent` separately owns the accepted current
and previous animation, frame/code/delay cursor, render surface/depth, and
subflags. `SoldierAnimationActivityComponent` owns the lifecycle around that
playback: prone-turn mode, pausing, hit and fall phases, interruptibility,
turn-to-completion state, one-shot AP-cost waivers, traversal forecast, and
temporary render-depth override. Random-animation cadence and the last selected
random animation also live at this boundary. Named operations now change
coordinated hit, fall, pause, interruptibility, traversal presentation, and
random-animation state together.
`SoldierAnimationCacheComponent` owns the runtime surface working set in
fixed-capacity inline arrays. Creating a soldier no longer performs two cache
allocations, copies start with an empty working set instead of aliased owning
pointers, and repository replacement keeps loaded-surface identity attached
to its canonical slot. The serializer keeps targeting, attack selection,
fire-control, incoming combat results, damage-display presentation,
render-state values, and suppression reaction state at their established byte
positions and preserves fade mode `2`, continuation mode `2`, and hit phase
`2` as 8-bit values rather than reducing them to boolean `1`. The retired
cache-pointer
visitors emitted no bytes; load now resets the inline cache directly. The
unused legacy delayed-cause-merc byte is retained only at its save position and
is no longer live soldier state. The v101 converter copies all six 32-bit
spread targets rather than half of that legacy array. None of these components
changes content, map, packet, Lua, or save schemas.

`SoldierRenderBindingsComponent` owns the application-only attachment between a
soldier record and the face, tactical-world, and animation-tile registries.
Normal `TacticalActor` copies deliberately receive no face index, `LEVELNODE*`, or
`TAG_anitile*`; only the bounded soldier repository may transfer those bindings
when committing or swapping canonical records. The component does not destroy
registry objects, so existing face/world/tile teardown remains authoritative.
`SoldierRuntimeComponents` is also private behind `runtime()`. Together with
the typed persistent domains above, this leaves no meaningful mutable public
storage in the live soldier class. Compatibility code still sees the same POD
footprint and save order through opaque legacy slots and explicit field
visitors; packages receive neither these process-local bindings nor raw
`TacticalActor` access.

Every `EngineRuntime` owns a bounded `TacticalWorldItemDirectory`. It grows
only through activated slots, fails closed when its incarnation space is
exhausted, and never exposes `WORLDITEM` or `gWorldItems` through the SDK.
JA2 keeps a runtime-only incarnation mirror outside the serialized world-item
payload. This identity boundary therefore changes neither installed game data
nor map, save, or temporary-item formats. Peer-interaction identity is likewise
runtime-only and does not add fields to serialized soldiers.

Every `EngineRuntime` also owns one `TacticalInventoryUiSession`. It is a
pointer-free table of `TacticalInventoryActorRole` to `TacticalEntityId` for
application inventory panels and modal children. Hosts can set, query, clear,
copy, count, or reset these value-only roles without importing `TacticalActor`.
JA2 producers capture canonical identities before retaining them; legacy
consumers resolve records only through the separately named compatibility
header. Resolution and stale-modal cancellation remain responsibilities of the
JA2 application adapter, so this SDK type grants neither inventory mutation nor
UI control and changes no game-data or persistence format.

`TacticalDoorUiSession` is the corresponding single-modal value contract for
JA2's door-action popup. Hosts may begin, inspect, match, and reset a context
containing `TacticalEntityId`, tactical-world generation, exact structure grid,
base-grid and map-local ID plus a value fingerprint, direction, and close/open
mode. Both public identity records are standard-layout, trivially copyable
values. The SDK object never
imports `TacticalActor`, `STRUCTURE`, mouse regions, or buttons. JA2's
application adapter captures and re-resolves those compatibility records at
each UI use, rejects a world transition, actor incarnation change, invalid
base-grid, or missing/replaced structure, and clears a cancelled continuation
only while its exact pending-door identity still matches. The session is
runtime-only and adds no map, save, command-journal, or package schema.

The command journal has one current wire layout. It retains a version field so
a genuinely published format can evolve later, but unsupported versions are
rejected rather than supported speculatively.
`BulkReloadWeaponsCommand` represents reload-all as one ordered inventory
transaction. It carries a canonical, fixed-capacity roster of exact actor
incarnations plus the selected squad and issued tactical mode; only local-player
and replay provenance are valid. The portable reference simulation deliberately
discards it because the reference model owns no JA2 item stacks or world-item
pool. JA2's compatibility executor alone applies the established inventory
policy after transactionally rebuilding and matching the complete live roster,
selected squad, and tactical mode. Any stale or subset command is discarded
before inventory or world-item state changes.
`ApplyWeaponConfigurationCommand` represents equipment/attachment correction
and friendly-retaliation mode selection as a System/Replay-only exact result;
it is deliberately distinct from the local-player Cycle commands. The command
captures the expected hand item plus the complete weapon mode, scope, fire
progress, barrel, shown aim, and launcher-delay state. Equipment continuations
also capture inventory position and old/new item IDs. Stable cause,
presentation policy, ordered gameplay continuation, and outbound event policy
are independent values. Before mutation, JA2 re-runs the cause-specific resolver
and requires the captured result to match current actor incarnation, hand item,
equipment slot/item, attachments, configuration, and target state. This gives
legacy in-progress fire-counter tuples semantic validation without pretending
the SDK owns JA2's item-capability tables. Equipment side effects remain one
ordered continuation across retained ingress. Retaliation additionally captures
the exact attacker incarnation and issued grid/level, fails closed when a
required configuration is rejected, and replays its ready/configure/fire
continuation locally at that level without re-emitting a peer event. Playback
queues a `Replay`-origin copy while its diagnostic journal preserves the
captured `System` source; this transformation is fixed by the runtime stream
policy rather than supplied as two caller-controlled batches. Programmatic
staging returns `CommandReplayStageResult::Invalid` if either the captured or
normalized value is structurally invalid. The portable reference
simulation explicitly discards this JA2 inventory/UI policy. Codec tag 29 is
appended to the current journal version; no application packet or persistence
format is reinterpreted.
This wave does not migrate lower-level configuration writes in `Items.cpp`
(rifle-grenade attachment insertion, direct hand placement, and attachment
removal), nor `HandleSuppressionFire`'s forced alternative-hold scope reset in
`Overhead.cpp`. Those named compatibility mechanics remain deferred and are not
part of the command's ownership claim.
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

`EngineRuntime::strategicGroupDirectory()` owns pointer-free liveness for JA2's
reusable one-byte movement-group IDs. `StrategicGroupId` combines that
compatibility slot with a runtime incarnation; delayed application adapters
must resolve the complete identity rather than retaining a linked-list pointer
or resolving the slot alone. Reset retires every live identity without
rewinding the sequence, and incarnation exhaustion fails closed. The directory
does not serialize state or change the established `GROUP` and save layouts.

`TacticalWorldService` exposes immutable, pointer-free snapshots of the loaded
world. In the JA2 host, `snapshot.epoch()` is the nonzero world-load generation
and `snapshot.turn().serial` is a nonzero identity scoped to that epoch: serial
one denotes the newly loaded pre-turn state, each accepted `BeginTeamTurn`
boundary advances it, and exhaustion saturates instead of wrapping. Compare a
turn serial only within the same epoch. `EngineRuntime` also owns the
turn-based/combat mode, current team, and pending asynchronous combat-action
count in that same session. Gameplay reads these values through the JA2
adapter's session-backed accessors; the former `gTacticalStatus` team,
attack-busy, and turn/combat flag mirrors have been retired. The pending-work
gateway prevents wrapping or underflowing, while save code explicitly clamps
the wider authoritative count into the established byte position. Pending
execution work remains host-internal;
the package-facing tactical turn snapshot still contains mode, team, and turn
identity, so this ownership move requires no wire or service-version change.

The same host session now owns creature-encounter narrative state: three
legacy quote flags, the next tense-quote delay, and its last-update timestamp.
The application supplies random delays and performs dialogue effects, while
the session applies the established unsigned strict-deadline rule. These
values remain host-internal and retain their exact save-stream positions; the
five writable `gTacticalStatus` mirrors have been retired.

Pending improved-interrupt kind and the player-turn interrupt-suppression
latch are also host-session state. The JA2 adapter exposes value accessors and
the only production write gateways; input, turn logic, and interrupt
resolution no longer share writable `gTacticalStatus` fields. Both values
retain their exact trailing save-stream positions and are restored together
after the complete tactical-status section validates. They remain
host-internal legacy-rule inputs and do not extend the package-facing tactical
snapshot or any network protocol.
`InitOverhead` still clears both values at the former whole-status reset
boundary. World commit and unload introduce no implicit interrupt reset.

The same session owns each tactical team's in-sector count and activity byte
as one host-internal value. JA2's gameplay readers use a narrow application
gateway, and add/remove or multiplayer recount operations publish the pair in
one transition. This does not expose team rosters to packages: ID ranges,
side, color, awareness, and human-control metadata remain in the application,
and the package-facing tactical snapshot is unchanged. Save compatibility
reconstructs the established 20-byte records at the persistence boundary.
`InitOverhead` resets all 11 values at the former whole-status reset boundary;
world commit and unload preserve them. Because `TacticalWorldSession` is an
installed C++ source contract, consumers must rebuild for its additive state
layout. `TacticalWorldSnapshot`, wire data, and the versioned tactical-world
service contract remain unchanged.

The application retains `gWorldSectorX`, `gWorldSectorY`, and `gbWorldSectorZ`
as const-reference projections for source-compatible, allocation-free hot-path
reads. Their hidden storage is published only by `TacticalWorldAdapter`; writes
and mutable address escapes are rejected by both the C++ type system and the
architecture boundary test. They are compatibility views, not engine storage:
packages and external tools consume `TacticalWorldService`, and application
transitions use the adapter gateways. Turn/combat mode and current team have
gone one step further: no live `gTacticalStatus` projection remains, and
architecture checks reject its return while the remaining broad legacy status
structure is migrated by its own gameplay domains.

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

Package assets retain the formats and logical paths consumed by JA2. Packages
add deterministic identity, dependency, ownership, and overlay order around
existing data; they do not require converting `Data-*` directories, VFS
profiles, archives, XML, maps, artwork, audio, dialogue, or Lua. See
[`DATA_PACKAGES.md`](DATA_PACKAGES.md) for the disk layout.

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
retired identities are never reused, and palette owners retire each owned
table's registry entry before replacement or destruction.
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
