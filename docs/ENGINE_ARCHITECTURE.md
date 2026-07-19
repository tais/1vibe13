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

- `Engine/Core` contains platform- and campaign-independent primitives. CTest
  rejects project-header dependencies in this directory.
- `GameContext` is the composition root. It initially binds legacy globals and
  will replace them service by service without changing save layouts en masse.
- `GameCapabilities` moves JA2/UB/editor decisions from preprocessing toward
  startup-selected runtime policy.
- `ContentRegistry` validates package identity and required engine API version.
- `PackageRegistry` owns package validation and activation policy while package
  objects remain application-owned. Only one campaign may be active, whereas
  rules, extensions, and tools can be composed around it.
- `LegacyCampaignPackage` exposes the compiled JA2 or UB campaign through that
  runtime contract. It is the compatibility bridge to replace with discovered
  package manifests and campaign bootstrap hooks incrementally.
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
- `AssetSource` exposes normalized, read-only logical content with provenance
  and deterministic, case-insensitive overlays. It is the engine seam through
  which package lifecycle mounting will let campaigns and mods replace assets
  without receiving writable save storage or importing the legacy VFS API.
- `EngineRuntime` owns campaign-independent lifecycle, screen state, content,
  packages, and service bindings. `GameContext` is now a JA2 compatibility
  facade around that reusable composition root plus legacy settings/options.
- `DeterministicCommandQueue` provides tick/sequence ordering for simulation,
  replays, multiplayer synchronization, and headless tests.
- `BinaryArchive` provides bounded, endian-defined, versioned persistence.
- `StateStack` represents base screens and modal overlays without scattered
  previous-screen globals.
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
   an engine minor version no newer than the running engine supports.
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
