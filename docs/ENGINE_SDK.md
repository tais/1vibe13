# JA2 Engine SDK

`JA2::EngineCore` is the campaign- and platform-independent C++17 engine
surface. It can be installed without SDL, SGP, VFS, game data, or any JA2
application library and consumed from an unrelated CMake project.
`JA2::RuntimeAdapter` adds the pointer-free JA2 command, replay, and tactical
world contracts and links `JA2::EngineCore` transitively. Neither target links
the legacy game or platform libraries.

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
`Engine/Adapters/JA2` target layers `EngineRuntime`, tactical commands, their
codec, tactical-world observation/publication, and durable replay on Core. It
is installed as `JA2::RuntimeAdapter`; platform adapters and legacy game types
remain outside the SDK boundary.

The `engine_sdk_consumer` CTest installs the component, copies its fixture away
from the repository tree, rejects source/build paths in the exported metadata,
and builds the fresh project against `find_package(JA2Engine)`. It exercises
Core plus the command codec, durable replay, runtime composition, tactical
world diff/codec/observer, message publisher, and tactical command service
surfaces.

`TacticalCommandService` is the package-facing, pointer-free write boundary for
JA2 tactical commands. A host owns a finite `TacticalCommandInbox`, registers it
as `ja2.tactical-commands` before package bootstrap, validates application
domains at its safe simulation boundary, and drains only a configured prefix.
The service deliberately does not expose draining or cancellation authority.
Every callback receives a registry-issued `PackageIdentity`. It can be copied
and passed to package-aware services, but cannot be constructed from an
arbitrary package ID. Retaining it does not keep a package active: services
must still reject work after that package leaves the active set. Native code in
the same process remains a cooperative trust boundary rather than a sandbox.
The JA2 application additionally tracks the bounded accepted batch by command
sequence so lifecycle teardown can cancel both pending inbox requests and any
accepted command retained after an execution failure. Admission requires a
loaded tactical world (and turn-based combat for end-turn commands); actor
incarnation and live-sector checks remain executor policy so stale references
are deterministically journaled as discarded. Any existing authoritative
queue, including a future-tick staged replay, pauses package admission until
that stream clears. This keeps live ingress from forcing a large replay sort on
the frame thread or interleaving two authoritative producers.

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
for the runtime session. This is not yet a stable native plugin ABI.

Hosts may also populate `RuntimeConfiguration` with portable keys and boolean,
signed integer, double, or string values before initialization. Packages read
the sealed configuration from their bootstrap/runtime context. Replacing a key
with a different type is rejected so configuration contracts cannot silently
change beneath consumers.

Each package callback also receives `PackageStorage`, a view bound to that
package's ID. Record names are portable identifiers and data uses the engine's
bounded checksummed envelope format under `PackageData/<package>/<record>.bin`.
This is the preferred durable-state API for new packages.

State that belongs to a particular game save uses a separate contract. Set
`PackageDescriptor::saveStateSchemaVersion` to a non-zero version and override
`saveState`, `validateState`, and `loadState`. Capture publishes opaque bytes;
validation must parse without mutation; load commits transactionally for that
package. The host orders records by package activation, enforces 4 MiB per
package and 16 MiB in aggregate, binds the archive to the runtime fingerprint,
and contains callback failures. Installation/profile preferences should remain
in `PackageStorage`; campaign progress belongs in the per-save callbacks.

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
snapshots for replay diagnostics. The host seed and per-package stream limit
are composition settings; the legacy `EngineServices::random` remains intact.

Packages may override `EnginePackage::simulate` for fixed-step work that should
not depend on rendering cadence. The host publishes the configured step and
maximum catch-up count, executes only that bounded number after a hitch, and
records dropped ticks in frame telemetry. `updateRuntime` remains the per-frame
hook for interpolation, UI, and other presentation-paced work.

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
publishing records. JA2 attaches these archives beside legacy saves; other
hosts can choose their own domain-save transaction and naming policy.

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
all remaining owned playback. Existing game audio remains on direct
`AudioOutput` adapters while it is migrated incrementally.

Packages may declare `requiredCapabilities` alongside contributed
`capabilities`. The host validates the list at registration and preflights each
requirement against host and active-package capabilities before the first
bootstrap callback. A missing feature produces a structured package/capability
failure and a fault-journal record instead of forcing mod code to inspect build
targets or global campaign state.

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
