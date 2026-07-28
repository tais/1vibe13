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
wall, and window traversal, and player weapon-mode, scope-mode, reload,
ready/lower, turn, stance, fire, movement, facing, stealth, stop-movement,
drag cancellation, stealing, position exchange, and world-item pickup intent.
An approach combines movement with its pending interaction so command pressure
cannot apply one without the other.

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
ceilings before publication. Movement, stance, facing, stealth, stop, path/stop
synchronization, fire, and turn commands update this stable state without SDL,
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
Legacy multiplayer path, fire, stop, stance, facing, and turn packets retain
their existing wire structures. The application adapter resolves their reusable
soldier slot once and submits the same stable command vocabulary. Path and stop
packets are represented as explicit bounded reconciliation snapshots rather
than being misread as local movement intent; synchronized fire captures the
packet's selected weapon; and synchronized turn capture records whether the
receiving host must enter combat or close its client turn. These synchronization
commands accept only network/replay provenance. Reliable network ingress queues
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

The JA2 application has a separate actor-reference ingress overload for local
UI code. It atomically captures the referenced merc's slot and incarnation,
verifies that the runtime directory resolves that identity back to the same
object, and only then creates the pointer-free command. Detached or forged
objects produce `SimulationCommandDispatchStatus::InvalidActor` before queue
submission. Packages, replay tools, and network hosts never receive this
application-only overload; they continue to use `TacticalEntityId` command
values through `TacticalCommandService`.

Every `EngineRuntime` owns a bounded `TacticalEntityDirectory`. In addition to
slot/incarnation liveness, a host can commit the latest public
`TacticalActorSnapshot` with `publishState` and retrieve it only through the
same exact identity with `state`; replacing or releasing an incarnation removes
its state atomically. This is host authority, not a package mutation service.
The JA2 host publishes creation and command results immediately and reconciles
remaining legacy animation/vitals changes at its completed-frame boundary.
`TacticalWorldService` is built from this committed pointer-free state rather
than exposing or rereading `SOLDIERTYPE`. The projection is runtime-only and
does not change soldier, map, save, content, or tactical-delta formats.

`GameContext` also owns the application-only `Ja2SoldierRepository` that
connects this pointer-free runtime identity to JA2's current fixed soldier
records. The repository is not part of the SDK and does not expose
`SOLDIERTYPE` to packages. It centralizes bounded slot resolution,
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

Inside the JA2 application, transient soldier state is also being separated by
behavior. Pending-action scratch and deferred work, combat-feedback counters,
and quick-item retention are owned by a resettable runtime component rather
than independent flat `SOLDIERTYPE` fields. This component is not exposed
through the SDK and is deliberately absent from soldier persistence.
`SoldierVitalsComponent` privately owns the application soldier's complete
persistent health, breath, wound, and recovery lifecycle: current/max values,
previous-turn and fractional health, breath reduction, treatable injury and
surgery state, unrecoverable breath, critical-stat damage, bleed scheduling and
sound throttling, and the retired regeneration save slots. Named snapshot,
surgery, damage-recovery, life-deduction, and reset transitions keep the domain
coherent; the explicit serializer retains every established position and width.
`SoldierServiceComponent` separately owns the persisted service marker, patient
provider count, provider-to-patient identity, and automatic-bandage medic
reservation. Named relationship transitions give tactical AI, medical actions,
and presentation one authority while guarding provider-count removal from
underflow. The face UI keeps only its old-value render cache, and persistence
retains all four established positions and widths.
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
`SoldierAiPlanningComponent` separately owns flank progress and geometry,
sniper posture, and modular plan selection. Tactical AI records flank steps,
terminal progress, posture changes, and default plan selection through named
transitions shared by realtime and turn-based execution. The signed flank
counter saturates instead of wrapping; the serializer retains all five original
positions and widths, and v101 conversion maps its four established values
while clearing the later plan index.
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
`Disease.h`/`SOLDIERTYPE` include cycle. The serializer retains every original
position and width; v101 conversion clears this later domain.
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
movement-noise memory, heard-noise elevation, blindness/deafness lifetimes, and
X-ray activation time. Its named operations preserve the exact sight-recovery
edge and per-turn noise cleanup while persistence retains all six original
positions and widths. Opponent lists and render visibility are intentionally
separate. `SoldierAwarenessComponent` owns current player-facing visibility,
the last visibility consumed by rendering, newly discovered opponent count, and
movement distance used to age stale knowledge. Its named visibility, fade,
render-sync, discovery, and forget transitions preserve the tactical state
machine while keeping per-observer opponent lists in the AI adapter. Current
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
Strategic route/group objects remain adapters, and all existing sector,
transit, insertion, vehicle, arrival, and arrival get-up gameplay entry points
continue to operate on the same values. `SoldierScheduleComponent` owns live NPC schedule identity,
action progress, and the door continuation phase/grid shared by strategic
scheduling and tactical movement. Named transitions atomically begin,
complete, consume, or cancel the door continuation; editor placements,
schedule nodes, and creation/network records keep their established public
formats. Precise and integer-projected world coordinates, turn-start
coordinates, initial/current grid, elevation and facing, current/desired
height, temporary animation grid, room, and terrain history are privately
owned by `SoldierPositionComponent` as one persistent storage domain.
Zero-cost reference accessors remain available to application hot paths,
while named coordinate and terrain transitions keep paired values coherent.
Old-save conversion and explicit persistence retain every established field
position. `SoldierMovementHistoryComponent` separately owns the last departed
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
destination-center crossing live there as well. UI, AI, animation, rendering,
pathing, and simulation-command adapters all use `movement()` as the one
authority. Named operations cover intent changes, synchronized extended
facing, grid-update suppression, turn and pause lifecycles, water/UI-speed
edges, and paired destination crossing instead of independently mutating
generic flags.
`SoldierInterruptSnapshotComponent` captures the scheduler's moved state across
temporary interrupt ownership without exposing another flat soldier field.
`SoldierTargetingComponent` owns the selected target
grid, elevation, cube level, previous target grid, and target soldier identity.
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
`SoldierFireControlComponent` owns that mutable firing sequence: burst and
autofire progress, bullets in flight, the one-based spread cursor and six
fixed spread targets, recoil and counterforce history, initial muzzle offsets,
the autofire UI edge state, the active multi-barrel cursor, and burst-drag
start/end grids. Real-time and turn-based input share named drag transitions.
Named single-shot, burst, and autofire transitions keep these paired modes
consistent. Target selection remains separate, as do presentation-only sound
and muzzle-flash handles. AI dual-wield spread generation is clamped after
doubling its shot count, so it cannot write twelve locations into the
established six-target buffer.
`SoldierCombatResultComponent` owns incoming attacker history, hit
location/reason, per-turn hit and pellet counts, and accumulated damage. Named
history operations preserve killer and assister attribution as one transition.
`SoldierDamageDisplayComponent` separately owns the floating-number cursor,
screen offset, and direction. Accumulated damage stays in the simulation
component because existing torso-hit and death rules consume it; render
coordinates cannot leak into those rules.
`SoldierRenderStateComponent` owns soldier-local rendering values: the five
palette-replacement identities, fade mode/level/origin, forced colour and shade
policy, muzzle-flash visibility and light handles, the unblit rectangle, and
projected bounds. Use `renderState()` and its named fade, flash, shade, redraw,
and light-lifetime operations from tactical graphics adapters. Raw palette,
shade, surface, level-node, and background pointers remain with the legacy
graphics adapter and are not component or package API.
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
count, or reset these value-only roles without importing `SOLDIERTYPE`.
Resolution and stale-modal cancellation remain responsibilities of the JA2
application adapter, so this SDK type grants neither inventory mutation nor UI
control and changes no game-data or persistence format.

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
