# Portable save / data file format — analysis & proposal

*Investigation for the cross-platform save-game problem on the JA2 1.13 SDL3
port (macOS/Linux). Companion to [SDL3_PORT.md](SDL3_PORT.md).*

## TL;DR

JA2's save format is an **implicit memory dump** of C++ structs. That is
non-portable for three independent reasons (wide-char width, struct padding,
implicit type sizes). The robust fix is to replace the implicit dump with an
**explicit, versioned, little-endian, field-based serialization** — formalizing
the field-by-field approach the codebase already started. Recommendation:
**Option B** below. It's localized to the save/load code, doesn't touch the
thousands of text-rendering call sites, and yields a format that's identical on
every platform.

The current `.sav` also has an engine-owned outer container. The portable
domain stream remains at offset zero, followed by mandatory runtime-checkpoint
and package-state sections plus an exact-length, independently checksummed
trailer. See [Runtime save container](RUNTIME_SAVES.md). This outer contract
does not replace the field-by-field work described below; it gives that
migration one bounded, inspectable transaction.

---

## The problem

Saving is done by writing raw struct bytes to disk, e.g.
([SaveLoadGame.cpp:1835](../Ja2/SaveLoadGame.cpp)):

```cpp
FileWrite( hFile, this, SIZEOF_SOLDIERTYPE_POD, &uiNumBytesWritten );
// SIZEOF_SOLDIERTYPE_POD == offsetof( SOLDIERTYPE, endOfPOD )
```

`offsetof(…, endOfPOD)` is the in-memory size of the struct's "plain old data"
prefix **on the building compiler/platform**. That makes the on-disk layout
depend on platform ABI details. There are **22 such `SIZEOF_*_POD` structs**
(SOLDIERTYPE, MERCPROFILESTRUCT, OBJECTTYPE, WORLDITEM, MILITIA, REAL_OBJECT,
INVENTORY_IN_SLOT, …) plus their nested structs.

### Three independent portability breakers

1. **Wide-char width.** `CHAR16` is `typedef wchar_t`. `wchar_t` is **2 bytes on
   Win32** but **4 bytes on macOS/Linux**. Every `CHAR16 field[N]` in a saved
   struct is double-size off-Windows, so the on-disk layout (and `offsetof`)
   differs. (This already bit `LoadMercProfiles` reading the shipped Win32
   `Prof.dat`; fixed there, deferred everywhere else.)
2. **Struct padding / alignment.** Raw dumps capture compiler-inserted padding.
   Different compilers/ABIs can pad differently → layout drift even with equal
   field sizes.
3. **Implicit type sizes.** `enum`, `BOOLEAN`, bare `int`/`long` in saved structs
   have platform-dependent sizes/signedness. (`long` is 32-bit on Win32, 64-bit
   on macOS/Linux — the same trap as the `-Wshorten-64-to-32` audit.)

### Current real-world consequence

- A save written on macOS is *self-consistent* (mac→mac read-back can work) but
  **byte-incompatible with Windows saves**.
- Reading any **shipped Win32-format file** (`Prof.dat`, maps) on macOS is broken
  unless that reader is fixed per-file.
- Some structs were **partially migrated** to field-by-field
  (`MERCPROFILESTRUCT::Load` uses `ReadFieldByField`) — and that helper only
  handles **alignment padding, not width**, so it currently reads
  `sizeof(CHAR16)`=4-byte chars from a 2-byte-per-char file. Where a struct's
  Save and Load don't use matching mechanisms, **even mac→mac is corrupt.**

### What already exists to build on

- A save **version** field (`guiCurrentSaveGameVersion` / `SAVE_GAME_VERSION`),
  with a precedent for format breaks at a version boundary
  (`NIV_SAVEGAME_DATATYPE_CHANGE = 102`, "before this we used the old structure
  system"). So a clean new-format baseline at a new version number is idiomatic.
- `ReadFieldByField()` (padding-aware) and a half-finished field-by-field
  migration in `MERCPROFILESTRUCT`/`SOLDIERTYPE` load paths.

---

## Options considered

| # | Approach | Portable? | Effort | Risk | Notes |
|---|---|---|---|---|---|
| **A** | `CHAR16 → char16_t` globally (stracciatella's choice) + keep raw blobs | Fixes wide-char only; **padding/type-size still fragile** | **High** | High | Cascades into ~all text APIs (`swprintf`/`wcs*`/font `GetIndex`/UI). Doesn't fix padding (#2) or `long`/enum (#3). |
| **B** ✅ | **Explicit field-based serialization** (typed Writer/Reader, 16-bit wide-string helpers, defined endianness) | **Fully** (no ABI/padding/wchar dependence) | Medium-high | **Low** | Localized to save/load. Fixes all 3 breakers. Matches the direction the code already started. |
| **C** | "Completely different": tagged/library format (JSON / MessagePack / Protobuf / Cap'n Proto / SQLite) | Fully + self-describing/versioned | Very high | Med | Great greenfield; impractical retrofit — no C++ reflection, so you still enumerate every field, plus rewrite all version-migration logic and eat size/perf for big maps. |
| **D** | `#pragma pack(1)` POD **mirror** structs (UINT16 chars, fixed-width ints) serialized as blobs | Fully (if endianness defined) | Medium | Med | Fewer call sites than B, but mirror structs silently drift out of sync with the live structs. |

---

## Recommendation — Option B

Replace the implicit memory-dump with an **explicit, versioned, little-endian,
field-based format**. Concretely:

### 1. A tiny serializer core (new `sgp/SaveSerializer.{h,cpp}`)

A `SaveWriter`/`SaveReader` wrapping `HWFILE` with typed primitives — the format
is *defined* by these, independent of any struct layout:

```cpp
w.u8(x); w.u16(x); w.u32(x); w.i8/i16/i32; w.f32; w.boolAsU8;
w.wstr(CHAR16* p, size_t n);   // writes n × UINT16 (narrow wchar_t → 16-bit)
w.bytes(p, n);                  // fixed byte arrays / CHAR8[]
// reader mirrors these; wstr widens 16-bit → wchar_t into the live CHAR16[]
```

- All multi-byte values written **little-endian** explicitly (byte-swap on
  big-endian — none of our targets are, but the format is defined regardless).
- `wstr` is the single chokepoint for the `CHAR16` problem: **16-bit on disk,
  `wchar_t` in memory** — no struct-layout dependence.

### 2. Per-struct `Serialize(writer)` / `Deserialize(reader)`

One explicit field list per saved struct (the 22 POD structs + nested types).
Tedious but mechanical, safe, and reviewable. Many already have half-built
`Save()`/`Load()` to convert.

### 3. Version the new format

Cut a new `SAVE_GAME_VERSION` baseline = "portable format v1". The loader picks
old vs new by the version header (the machinery exists). Old (pre-port,
Win32-only) saves are already non-portable to mac, so a clean break is
acceptable; we keep the *current* in-development saves working by writing/reading
the new format on both sides.

### 4. Apply the same to shipped data files

`Prof.dat`, map `.dat`, etc. are separate fixed-format files authored on Win32.
They need the same 16-bit-wide-string reading (the `LoadMercProfiles` fix
pattern). Track as related work — the `wstr` reader helper is reused.

### Migration order (incremental, each independently testable)

1. Land the serializer core + helpers (no behaviour change yet).
2. Convert the leaf structs first (OBJECTTYPE, REAL_OBJECT, INVENTORY_IN_SLOT,
   WORLDITEM, MILITIA) — small, high-reuse.
3. Convert MERCPROFILESTRUCT, SOLDIERCREATE_STRUCT, SOLDIERTYPE (finish the
   half-done work) — the big ones.
4. Convert the remaining containers / sector data.
5. Bump `SAVE_GAME_VERSION`; verify **save → quit → reload** on macOS, and a
   round-trip diff (save, reload, re-save → byte-identical).

### Effort & risk

Medium-high but **bounded and low-risk**: it's confined to save/load (no
text-API cascade like Option A), each struct is independent, and correctness is
verifiable by save→reload + round-trip-diff. The format becomes byte-identical
on every platform, which also makes saves shareable across Win/Lin/Mac.

---

## Dependencies & compression (no upgrade required)

- **Save games are plain, uncompressed binary** written directly via `FileMan`
  to a save directory (`MakeFileManDirectory(saveDir)`). They do **not** pass
  through 7z or zlib, so this work needs **no dependency upgrade**.
- **7z** (`ext/VFS/ext/7z`) is a **read-only** ANSI-C decoder used by bfVFS for
  the game's SLF/7z **data archives** (assets) — unrelated to saving, and it
  can't write archives anyway.
- **zlib** is `1.2.8` (current upstream is `1.3.1`); used by libpng/VFS, not by
  saves.
- **Only if** we choose to make the new format **compressed** (optional;
  attractive for large sector/map blobs) would zlib enter the save path — it's
  already linked, so that's a `deflate`/`inflate` wrapper around the serializer,
  plus an optional `1.2.8 → 1.3.1` bump as independent maintenance. The
  serializer in Option B is agnostic: it can write to a raw `HWFILE` today and a
  zlib stream later without changing any per-struct `Serialize()`.

## Decisions (locked)

- **Clean break — no backward compatibility.** We do *not* read pre-migration
  saves. The new format is save-version-gated; old saves are rejected. (These
  are dev saves; "kick the game into the next generation" > preserve old saves.)
- **Cross-platform parity is a goal.** The format is fully defined (little-
  endian, fixed widths, 16-bit wide chars) so a save is byte-identical on
  Win/Lin/Mac and shareable between them.
- **Scope = save games only.** Shipped `Prof.dat` and map readers stay as-is for
  now (tracked separately; reuse the `wstr` helper when we get to them).
- **Compression deferred.** Saves stay uncompressed for now; the serializer is
  written stream-agnostic so a zlib layer (and a `1.2.8 → 1.3.1` bump) can be
  added later without touching per-struct code.

## Implementation status

- ☑ Serializer core (`sgp/SaveSerializer.{h,cpp}`) — portable LE primitives +
  `wstr` (16-bit on disk ↔ `wchar_t` in memory).
- ☑ Migrate the item/object leaf chain: `ObjectData` (incl. its anonymous
  scalar union as a canonical fixed-size LE byte block), `StackedObjectData`,
  `OBJECTTYPE`, `LBENODE`, `REAL_OBJECT`, `INVENTORY_IN_SLOT`, `WORLDITEM`,
  plus standalone `MILITIA` and `DEALER_SPECIAL_ITEM`.
    - Shared save/map structs (`OBJECTTYPE`/`WORLDITEM`/`StackedObjectData`/
      `LBENODE`) branch on `fSavingMap`: the new format is used for savegames
      only; map files keep the legacy raw-blob write and the separate
      `Load(INT8**, mapVersion)` map-load path, so `.map` files are untouched.
    - `REAL_OBJECT`'s runtime `LEVELNODE*` pointers (`pNode`/`pShadow`) are no
      longer persisted — set NULL on load; the physics update rebuilds them.
- ☑ Field-visitor adapters (`SaveFieldWriter`/`SaveFieldReader` in
  `SaveSerializer.h`): one templated `Xfer` field list, visited by either,
  drives both save and load so the on-disk order can't drift on big structs.
- ☑ `SOLDIERCREATE_STRUCT` (savegame Save/Load; map path on `fSavingMap`
  stays legacy), plus `MERC_LEAVE_ITEM` and `ITEM_CURSOR_SAVE_INFO`.
- ☑ `MERCPROFILESTRUCT` via the field visitor (CHAR16 names → `wstr`,
  PaletteRepID → `str8`, inventory vectors, `STRUCT_Records`, dynamic-opinion
  2D arrays, growth modifiers as full INT16). The legacy/encrypted Prof.dat
  load path (`forceLoadOldVersion=true`) is preserved untouched.
- ☑ **`SOLDIERTYPE`** (the central struct) + its 7 sub-structs
  (`STRUCT_AIData`/`Flags`/`TimeChanges`/`TimeCounters`/`DRUGS`/`Statistics`/
  `Pathing`), ~550 fields, via the field visitor. ~20 interleaved runtime
  pointers use `ar.ptr()` (written as nothing, NULL on load — re-derived by
  `InitializeExtraData()`/palette+world rebuild, matching old behaviour which
  only persisted garbage pointer values). `signed long` pinned to 32-bit via
  `ar.slong`; `SoldierID` via its UINT16 `.i`. Builds on JA2/JA2UB/MAPEDITOR.
- ☐ Audit the **rest of the save stream** in `SaveLoadGame.cpp` and the other
  save modules (`Tactical Save`, strategic/laptop/finance/history/email/quest
  blobs, sector data) for bare `int`/`long`/`enum`/`BOOLEAN` and raw struct
  blobs written via direct `FileWrite`/`FileRead`. The wide-char-bearing
  structs (the portability priority) are now all migrated; this is the
  remaining breadth.
- ☑ Gate the loader at `PORTABLE_SAVE_FORMAT` / save version 1002 so older
  layouts fail before format-dependent reads.
- ☐ Add direct primitive/struct golden-byte coverage and cross-check a complete
  save round trip between macOS, Windows, and Linux.

### Verification

The test framework now covers the engine-owned outer container and its
application transaction: section round trips, bounds, corruption, exact runtime
matching, package contracts, staged restore, and incomplete-save cleanup run in
engine-core and headless tests. Full domain serialization is still verified by
**playtesting** (save → quit → reload). Direct serializer round-trip and
golden-byte tests for every primitive, including non-ASCII `wstr`, remain useful
follow-up work to prove cross-platform domain parity rather than infer it from
the shared implementation.

**Status (macOS playtest):** a save written by the migrated build loads
end-to-end into the strategic view — soldiers (incl. inventory), merc profiles,
and world items all deserialize correctly (per-soldier and per-profile checksums
validate). Getting the *first* full load to complete on 64-bit also flushed out a
batch of **pre-existing** latent bugs in the surrounding (non-serializer) load
path; see [Latent bugs uncovered](#latent-6432-bugs-uncovered-during-first-full-load).

---

## Format specification (v2)

The on-disk format is **positional and schema-less**: there are no per-field tags
or lengths. A struct's bytes are the concatenation of its fields in the order the
serializer visits them, so the reader must call the exact same sequence as the
writer. Cross-platform identity comes from defining every primitive's byte
encoding explicitly (below) rather than from struct memory layout.

### Primitive encodings (`sgp/SaveSerializer.{h,cpp}`)

`SaveWriter` / `SaveReader` wrap an `HWFILE` and define the byte layout:

| Primitive | On disk | Notes |
|---|---|---|
| `u8` / `i8` | 1 byte | |
| `u16` / `i16` | 2 bytes, **little-endian** | written byte-by-byte, host-endian-independent |
| `u32` / `i32` | 4 bytes, LE | |
| `u64` / `i64` | 8 bytes, LE | |
| `f32` | 4 bytes | IEEE-754 bit pattern reinterpreted as `u32` (LE) |
| `f64` | 8 bytes | bit pattern as `u64` (LE) |
| `boolean` | 1 byte | `0`/`1` |
| `wstr(p, n)` | `n × u16` (2n bytes) | **the CHAR16 chokepoint**: each wide char is narrowed to 16-bit on write, widened back into `wchar_t` (2 or 4 bytes in memory) on read |
| `str8(p, n)` / `bytes(p, n)` | `n` bytes verbatim | fixed `CHAR8[]` / `UINT8[]` fields |
| `skip(n)` | `n` zero bytes | reserved / dropped-field placeholder |

Everything is little-endian regardless of host (byte-swaps on a big-endian host;
none of the targets are, but the format is defined regardless).

### Field-visitor adapters

For large structs, `SaveFieldWriter` / `SaveFieldReader` expose the same
by-reference method names over a writer or reader. A single
`template<class Ar> Xfer(Ar&, T&)` field list per struct is visited by **either**
adapter, so save and load can never drift out of order. Extra methods:

- `ptr(T*&)` — **runtime pointers are never persisted**: writes nothing, sets the
  pointer `NULL` on load. The game rebuilds them after load (palette/shade tables,
  `LEVELNODE*`, `pMercPath`, `pGroup`, …). This matches the legacy
  behaviour, which only ever persisted meaningless pointer *values*.
- The soldier animation cache no longer contains pointers. Its retired `ptr`
  visits emitted no bytes, so load resets its fixed-capacity inline working set
  directly without changing the field stream.
- Current/max health and breath, bleeding, previous and fractional health,
  breath reduction, treatable injury, surgery, unrecoverable breath,
  critical-stat damage, bleed scheduling and sound throttling, and the retired
  regeneration slots are now stored by `SoldierVitalsComponent`. The visitor
  emits every value at its original scattered position and width, including
  32-bit-on-disk `slong` for unrecoverable breath. v101 conversion maps all
  fields present in that record and clears the later injury, surgery,
  unrecoverable-breath, and critical-damage fields. No save, packet, map, XML,
  Lua, or installed-data bytes change.
- Tactical service activity, patient provider count, provider-to-patient
  identity, and the automatic-bandage medic reservation are now stored by
  `SoldierServiceComponent`. The visitor emits the signed activity byte,
  unsigned provider-count byte, and both 16-bit `SoldierID` values at their
  original scattered positions; v101 conversion maps its four narrower legacy
  fields into that owner. No save, packet, map, XML, Lua, or installed-data
  bytes change.
- NPC quote plans, standard and extended quote-history masks, battle-voice
  selection and playback throttling, heard-noise speech cooldown, civilian
  quote progression, last-spoke time, and corpse-comment tolerance are now
  stored by `SoldierDialogueComponent`. The visitor emits all fourteen values
  at their original scattered positions and widths, and v101 conversion maps
  every established field. No save, packet, map, XML, Lua, dialogue, or
  installed-data bytes change.
- Footstep variation, remembered door-opening noise, and burst,
  positional-ambience, and turret-turning sound handles are now stored by
  `SoldierAudioComponent`. The visitor emits the two unsigned bytes and three
  signed 32-bit values at their four original scattered positions; v101
  conversion maps every established raw value. Fresh handles use the explicit
  no-sample sentinel, without altering values loaded from either schema. No
  save, packet, map, XML, Lua, audio, or installed-data bytes change.
- Movement and optimum timing, last network update time, update sequence and
  type, scheduled stop grid, and the soldier integrity checksum are now stored
  by `SoldierReplicationComponent`. The visitor emits five unsigned 32-bit
  values, one unsigned byte, and one signed 32-bit value at their three original
  groups and widths; the checksum continues to be calculated immediately
  before save and verified immediately after load. v101 conversion maps its six
  established fields and resets the scheduled stop that was absent from that
  record. No save, packet, map, XML, Lua, or installed-data bytes change.
- Turn-start carried weight, per-turn tiles moved, realtime breath-update tiles,
  and the last realtime movement animation are now stored by
  `SoldierMovementMetricsComponent`. The visitor emits the signed 16-bit
  weight, signed 8-bit turn distance, unsigned 8-bit realtime distance, and
  unsigned 16-bit animation at their three original positions; v101 conversion
  maps all four raw values. Runtime recording saturates both narrow distance
  counters without changing their representation. No save, packet, map, XML,
  Lua, or installed-data bytes change.
- Tactical-AI flank count, flank anchor, sniper posture, flank origin
  direction, and modular plan index are now stored by
  `SoldierAiPlanningComponent`. The visitor emits the signed 8-bit count,
  signed 32-bit anchor, signed 8-bit posture, signed 16-bit direction, and
  signed 16-bit plan index at their two original groups and widths. v101
  conversion maps its first four raw values and resets the later plan index.
  Runtime flank progress saturates without changing its representation. No
  save, packet, map, XML, Lua, AI-plan, or installed-data bytes change.
- Repeated skill-check identity and attempts, the AI's selected skill,
  20 persistent trait counters, 20 heterogeneous cooldown values, and the
  focus target are now stored by `SoldierSkillStateComponent`. The visitor
  emits each value at its original scattered position and width, including all
  unused fixed-capacity entries. v101 conversion maps its three skill-check
  fields and clears AI selection, counters, cooldowns, and focus, which were
  absent from that record. No save, packet, map, XML, Lua, or installed-data
  bytes change.
- Temporary stat modifiers, food and drink levels, starvation damage, 20
  disease-progress values, 20 disease-flag values, and the acquired-disability
  mask are now stored by `SoldierConditionComponent`. The visitor emits the
  four signed 16-bit stat modifiers, signed 8-bit experience modifier, signed
  32-bit nutrition values, unsigned 8-bit starvation values, all signed 16-bit
  disease points, all unsigned 8-bit disease flags, and the unsigned 32-bit
  disability mask at their original scattered positions. v101 conversion
  clears this whole later domain. The independent `NUM_DISEASES == 20`
  declaration is a C++ dependency cleanup, not a data-format change; no save,
  packet, map, XML, Lua, or installed-data bytes change.
- Multi-turn action kind, retained context grid, and remaining AP cost are now
  stored by `SoldierLongActionComponent`. The context grid continues to double
  as the return location for off-world intel assignments. The visitor still
  emits the signed 16-bit AP cost, signed 32-bit grid, and unsigned 8-bit action
  consecutively at their original position; v101 conversion clears this later
  domain. No save, packet, map, XML, Lua, or installed-data bytes change.
- Non-profile merchant identity, dragged-person identity, dragged-corpse
  identity, chat-partner identity, and dragged-structure grid are now stored by
  `SoldierInteractionComponent`. The visitor still emits the signed 8-bit
  merchant ID, unsigned 16-bit person ID, signed 16-bit corpse ID, unsigned
  16-bit chat ID, and later signed 32-bit structure grid at their original
  scattered positions. v101 conversion clears this later domain and restores
  explicit no-corpse/no-structure sentinels. No save, packet, map, XML, Lua, or
  installed-data bytes change.
- Pending action kind, animation-transition count, four primary payload values,
  door operation, queued-AI special data, interruption marker, and the fifth
  payload byte are now stored by `SoldierPendingActionComponent`. The visitor
  still emits the two unsigned 8-bit action values, unsigned 32-bit primary and
  quaternary payloads, signed 32-bit secondary payload, signed 8-bit tertiary
  payload and door operation, signed 32-bit queued data, unsigned 8-bit
  interruption marker, and signed 8-bit fifth payload at their original widths
  and scattered schema positions. v101 conversion maps every historical value.
  The existing runtime-only pending-action scratch remains unpersisted. No
  save, packet, map, XML, Lua, or installed-data bytes change.
- Current and turn-start action-point budgets are now stored by
  `SoldierActionPointComponent`. The visitor still emits both signed 16-bit
  values immediately after body type, and v101 conversion maps the original
  pair into that owner. Multiplayer reconciliation keeps its existing packet
  member and width.
- Tactical and breath-triggered collapse, recovery turns, the sleep-drug
  counter, and strategic fatigue collapse are now stored by
  `SoldierCollapseComponent`. The visitor still emits the fatigue flag and four
  signed 8-bit values at their original positions, and v101 conversion maps
  the original fields into that owner. No save or multiplayer packet bytes
  change.
- View range, directional movement-noise memory, heard-noise elevation,
  blindness/deafness durations, and X-ray activation time are now stored by
  `SoldierPerceptionComponent`. The visitor emits the five 8-bit values and one
  32-bit timestamp at their original POD positions, and v101 conversion maps
  the original fields into that owner. No save, packet, map, XML, Lua, or
  installed-data bytes change.
- Current tactical visibility, last-rendered visibility, new-opponent count, and
  movement distance used to expire stale knowledge are now stored by
  `SoldierAwarenessComponent`. The visitor still emits the three signed 8-bit
  values and one unsigned 8-bit value at their original four POD positions, and
  v101 conversion maps the original fields into that owner. No save, packet,
  map, XML, Lua, or installed-data bytes change.
- Applied and equipment-derived jungle, urban, desert, and snow camouflage are
  now stored by `SoldierCamouflageComponent`. The visitor still emits all eight
  signed 8-bit values at their original POD positions, and v101 conversion maps
  the original values into that owner while retaining the established kit-area
  caps. No save, packet, map, XML, Lua, or installed-data bytes change.
- Live contract timing, mercenary classification, deposits, insurance terms,
  renewal/dismissal bookkeeping, signing eligibility, and the per-soldier
  hospital modifier are now stored by `SoldierEmploymentComponent`. The field
  visitor still emits all fifteen values at their original POD positions and
  widths, and v101 conversion maps the original fields into that owner. Hire
  requests, profiles, and all save, packet, map, XML, Lua, and installed-data
  bytes remain unchanged.
- Current and previous duty, training choice, assignment timing, squad-merge
  state, repair/facility context, item-moving sector, and mini-event time are
  now stored by `SoldierAssignmentComponent`. The visitor still emits the
  eleven values at their original scattered POD positions and widths. v101
  conversion maps its eight historical fields and clears the three fields
  absent from v101. No save, packet, map, XML, Lua, or installed-data bytes
  change.
- Strategic sector, movement-group and vehicle membership, tactical insertion,
  traversal origin, off-world staging, and arrival bookkeeping are now stored
  by `SoldierDeploymentComponent`. The visitor still emits all fifteen values
  at their original scattered POD positions and widths, and v101 conversion
  maps every historical field into that owner. Strategic path and group
  pointers remain serialization adapters. No save, packet, map, XML, Lua, or
  installed-data bytes change.
- NPC schedule identity and progress plus the open-door continuation phase and
  grid are now stored by `SoldierScheduleComponent`. The visitor still emits
  the four values at their three original POD sites and widths. v101
  conversion maps identity, progress, and the widened door grid while
  retaining its historical behavior of clearing the transient door phase.
  Schedule nodes, editor placement records, multiplayer creation packets,
  maps, XML, Lua, and installed-data bytes are unchanged.
- Precise and integer-projected world coordinates, turn-start coordinates,
  initial/current grid, elevation and facing, current/desired height, the
  advanced-animation staging grid, room, and terrain history are now stored by
  `SoldierPositionComponent`. The visitor still emits all sixteen values at
  their original scattered POD positions and widths, and v101 conversion maps
  every historical value into the owner. No save, packet, map, XML, Lua, or
  installed-data bytes change.
- The last departed grid and two-location AI loop history are now stored by
  `SoldierMovementHistoryComponent`. The visitor still emits all three signed
  32-bit grids at their two original scattered POD sites, and v101 conversion
  maps every historical value into the owner. Current world placement and
  route execution remain separate domains. No save, packet, map, XML, Lua, or
  installed-data bytes change.
- Soldier target geometry and target identity are now stored by
  `SoldierTargetingComponent`, but the field visitor emits them at the same two
  established positions: geometry after movement reservation and identity
  beside attacking weapon/mode. No save or multiplayer packet bytes change.
- Attacking hand and weapon, weapon and scope mode, and ranged and melee aim
  locations are now stored by `SoldierAttackSelectionComponent`. The visitor
  retains their four established schema sites around visibility, hit location,
  target identity, and facility state. Existing fire command and multiplayer
  packet fields remain unchanged.
- Mutable volley execution is now stored by `SoldierFireControlComponent`.
  Spread and autofire flags, recoil/counterforce history, initial muzzle
  offsets, bullets in flight, burst progress, all six spread targets, autofire
  count, and the multi-barrel cursor remain at their established field-visitor
  positions. The v101 converter now copies the complete six-element `INT32`
  spread array instead of only its first half. This is an in-memory ownership
  and old-conversion correctness change; it does not change the current save
  stream, multiplayer packets, or installed data.
- Incoming attacker history, hit metadata, hit/pellet counters, and accumulated
  damage are now stored by `SoldierCombatResultComponent`. Floating-number
  flag, cursor, offsets, and direction are stored separately by
  `SoldierDamageDisplayComponent`. The visitor emits every value at its former
  flags/POD position, and the v101 converter maps the original fields into
  their new owners. Current save bytes, multiplayer packets, maps, Lua, XML,
  and installed data are unchanged.
- Militia kills, militia assists, and the fixed 156-entry player-team damage
  attribution table are now stored by `SoldierCombatContributionComponent`.
  The visitor emits the two unsigned 8-bit counters and every unsigned 8-bit
  attribution entry at their original three scattered positions. The v101
  converter retains its kill counter and now copies the complete historical
  attribution table instead of dropping it; assists remain zero because that
  record predates them. Current save bytes, multiplayer packets, maps, Lua,
  XML, and installed data are unchanged.
- Under-fire aging, shock, suppression points, AP loss, suppressor identity,
  and close-call feedback are now stored by `SoldierSuppressionComponent`.
  The visitor still emits them at their original AI-data, flags, and POD
  positions, and v101 conversion maps each original field into that owner.
  Current save bytes, multiplayer packets, maps, Lua, XML, and installed data
  remain unchanged.
- `slong(signed long&)` — pins `long` to **32 bits** on disk (`long` is 32-bit on
  Win32 but 64-bit on macOS/Linux).
- `isLoading` — compile-time bool for the rare asymmetric spot (e.g. `vector`
  resize-then-read).

### Type & layout conventions

- **Wide chars** (`CHAR16`/`wchar_t`): 16-bit on disk via `wstr`. Single source of
  the wide-char portability fix.
- **`SoldierID`**: serialized through its public `UINT16 i` member (`field.i`).
- **`std::vector`s**: written as an `i32`/`u32` element count followed by the
  elements; the reader resizes (with a sanity bound) before reading.
- **Anonymous POD unions** (e.g. `ObjectData`'s gun/money/bomb/key/owner/lbe
  union): written as a fixed canonical byte block of `offsetof(struct, next_field)`
  bytes. Portable because every union member is a fixed-width, naturally-aligned
  scalar whose layout is byte-identical on all targets.
- **Float-typed game fields** (`FLOAT`/`real`/`vector_3`): `f32` per component.
- **Maps are untouched.** Structs shared between savegames and `.map` files
  (`OBJECTTYPE`, `WORLDITEM`, `StackedObjectData`, `LBENODE`, `SOLDIERCREATE_STRUCT`)
  branch on the `fSavingMap` flag in `Save()`: the v2 format is used only for
  savegames; map writes keep the legacy raw-blob path, and map reads are the
  separate `Load(INT8**, dMajorMapVersion, …)` overload, both left as-is.
- **Legacy/encrypted readers preserved.** `MERCPROFILESTRUCT::Load(…,
  forceLoadOldVersion=true)` (the shipped `Prof.dat` path) and the pre-`NIV`
  encrypted branches are kept verbatim; only the modern savegame branch is v2.

### Where each struct's layout lives

There is no separate schema file — the layout *is* the per-struct field list:

- Item/object chain, soldiers, profiles, dealer/militia leaves:
  `Ja2/SaveLoadGame.cpp` (`Xfer*` templates + the `Save`/`Load` methods).
- `MILITIA`: `Strategic/MilitiaIndividual.cpp`.
- Serializer + visitor adapters: `sgp/SaveSerializer.{h,cpp}`.

### Versioning

`SAVE_GAME_VERSION` (in `Ja2/GameVersion.h`) gates old vs new format at load. It is
now `PORTABLE_SAVE_FORMAT = 1002` — a generational jump clear of upstream's
sequential numbering (~186), marking the clean break and avoiding collisions with
future 1.13 increments. (1000 was the initial portable format; 1001 added
field-by-field `ROTTING_CORPSE_DEFINITION` serialization; 1002 moved strategic
events into the runtime-owned EVQ2 value stream.) `LoadSavedGame`
**rejects any save below the current `PORTABLE_SAVE_FORMAT`** up front
(before any format-dependent read), so pre-migration saves fail cleanly instead of
mis-reading old bytes as v2. `uiSavedGameVersion` is the first field in the file,
so the gate reads correctly regardless of the rest of the (not-yet-portable)
header layout.

---

## Latent (64/32-bit) bugs uncovered during first full load

The migration let a savegame load run to completion for the first time on the
64-bit SDL3 port, which surfaced a string of **pre-existing** bugs in the
surrounding load/render code (none in the new serializer). Found with
**AddressSanitizer**, using a load-scoped render bypass to avoid ASan/Guard-Malloc
false positives in the Apple Metal/GCD allocator path. All are fixed:

| # | Location | Bug | Fix |
|---|---|---|---|
| 1 | `WriteTempFileNameToFile` (SaveLoadGame.cpp) | param typed `HFILE` (32-bit `int`) truncated the 64-bit `HWFILE` pointer → `FileGetPos` map lookup on a garbage key → crash | use `HWFILE` |
| 2 | `gzPlayerHandleField` (MPJoinScreen.cpp) | `CHAR16[10+1]` but every caller passes capacity `12` → 1-element global overflow at startup | size buffer `[11+1]` |
| 3 | `gubElementsOnExplosionQueue` (Explosion Control.cpp) | `UINT8` global saved/loaded as `sizeof(UINT32)` → 3-byte global overflow (the heap corruptor behind the late `trace trap`) | go through a `UINT32` temp |
| 4 | map-screen messages (message.cpp) | byte size computed as `len*2` (hard-coded old wchar size) instead of `len*sizeof(CHAR16)` → string written half-size / unterminated on 4-byte-`wchar_t` platforms | use `sizeof(CHAR16)`; load hardened to bound + force-terminate + size alloc to actual string |
| 5 | `OBJECTTYPE::operator=(OLD_OBJECTTYPE_101&)` (Item Types.cpp) | reads `ugYucky.bStatus[x]` for stacks `> OLD_MAX_OBJECTS_PER_SLOT_101` (8) → over-read | clamp index to the old 8-element bound |
| 6 | `RestrictMouseCursor` (input.cpp) | `memcpy(sizeof(gCursorClipRect))` where `gCursorClipRect` is a Win32 `RECT` (32 bytes on 64-bit, `LONG==long`) but the source is a 16-byte `SGPRect` → over-read | copy `sizeof(*pRectangle)` |

The recurring theme is the same one the format work targets: **hard-coded
2-byte wide chars and 32-bit-wide assumptions** that only break once a 64-bit
build actually exercises the code.

---

## Save-stream audit (remaining serialization beyond the migrated structs)

The structs migrated to v2 (soldiers/profiles/items/etc.) are the bulk of save
data and the wide-char-heavy part. The **rest** of the save stream is still a mix
of ad-hoc length-prefixed strings and raw struct blobs. Audited in two buckets:

### Same-platform crash / corruption risks (priority — these actually bite)

The dangerous patterns are (a) a *file-controlled* length read into a fixed/typed
buffer, and (b) a scalar global serialized with a wrong-width `sizeof`. Found and
fixed:

- **map-screen messages** (`Utils/message.cpp`) — `*2` wchar size + unbounded
  read into a fixed buffer. *(fixed)*
- **explosion queue count** (`TileEngine/Explosion Control.cpp`) — `UINT8` global
  read/written as `sizeof(UINT32)` → 3-byte global overflow. *(fixed)*
- **email subject** (`Ja2/SaveLoadGame.cpp`) — `*2` wchar size; load read now
  bounded against the fixed `EMAIL_SUBJECT_LENGTH` buffer. *(fixed)*

Greps for the same shapes elsewhere came up clean (e.g. `gubModderLuaData` is a
misleadingly-named `INT32[]`, correctly serialized). Exhaustive crash-hunting is
best continued by **ASan playtesting** across more screens (laptop/email/Bobby
Ray/sectors), since file-controlled reads are hard to enumerate statically.

### Cross-platform parity (DONE)

The remaining save stream was a mix of raw `FileWrite(&s, sizeof(s))` blobs.
Auditing the candidate structs precisely (reading each definition, not the noisy
grep) showed **~70% were already portable**: pure fixed-width-scalar structs are
byte-identical on all our targets because the only types whose size/alignment
differ between the **32-bit Windows** build and 64-bit mac/Linux are **pointers
(4 vs 8), `long` (4 vs 8) and `CHAR16`/`wchar_t` (2 vs 4)** — `UINT*`/`INT*`/
`FLOAT`/`double`/`BOOLEAN`/`enum` are identical. So the Shipment structs,
`SavedEmailStruct`, `ENEMYGROUP`, `GENERAL_SAVE_INFO`, `KEY_ON_RING`,
contract/air-raid/team-turn structs, etc. needed **no change**.

> **Correction (save format 1001):** `ROTTING_CORPSE_DEFINITION` was originally
> mis-classified here as already-portable. It is **not** — it ends in
> `CHAR16 name[10]`, so the raw `FileWrite(&def, sizeof(def))` dumped 2 bytes/char
> on Win32 but 4 on macOS/Linux (plus padding). It is now serialized field-by-field
> via `Save/LoadRottingCorpseDefinition` (`Tactical/Rotting Corpses.cpp`, one
> `XferRottingCorpseDef` field list), `name` written 16-bit on disk. This is the
> change that bumped the format to 1001.

The structs that actually carried breakers were all migrated to field-by-field
serialization (no padding ever written — byte-block shortcuts are unsafe because
pointer-alignment padding differs between 32- and 64-bit):

| Struct | Breaker | Handling |
|---|---|---|
| `SAVED_GAME_HEADER` | CHAR16 desc + GAME_OPTIONS | `wstr` desc; scalar GAME_OPTIONS as bytes; read before version gate |
| `TacticalStatusType` | CHAR16 top-message | `wstr`; SoldierID via `.i`; scalar `Team[]` as bytes |
| `MERCPROFILESTRUCT`, `SOLDIERTYPE`, `SOLDIERCREATE_STRUCT` | CHAR16 names | (original migration) `wstr` |
| email subject, map-screen messages | CHAR16 `*2` | `sizeof(CHAR16)` + bounded reads |
| `VEHICLETYPE` | ptrs (pMercPath, pPassengers) | skip; passenger profile IDs as fixed `u32` |
| `PathSt` (vehicle/militia/merc paths) | ptrs (pNext/pPrev) | shared node helper; links rebuilt |
| `STRATEGICEVENT` | linked-list `next`, runtime ID | EVQ2 queue section: magic/version/count plus six semantic fields per node; links and IDs rebuilt transactionally |
| `UNDERGROUND_SECTORINFO` | linked-list `next` | skip; rebuilt on load |
| `LaptopSaveInfoStruct` | 2 array ptrs | skip (arrays saved separately); scalars/sub-structs as bytes |
| `BULLET` | ptrs (firer/tracer/anitiles) | skip; firer re-derived from ID |
| `GROUP` + `WAYPOINT` | ptrs (waypoints/union/next) | skip; sub-lists saved separately, links rebuilt |

`signed long` fields (e.g. SOLDIERTYPE's `lUnregainableBreath`) are pinned to
32-bit (`ar.slong`). Same-platform saves were always fine; this pass makes saves
**shareable across Win/Lin/Mac**. Verification remains by playtest until a test
framework lands (golden-byte cross-platform tests are the ideal coverage).

---

## Map-file (`.dat` / `Maps.slf`) version-branched loaders

Separate from the savegame stream: each map record is loaded through a
`::Load(hBuffer, dMajorMapVersion, ubMinorMapVersion)` that branches on the map
version and blob-reads an era-specific "OLD" struct. **The on-disk map format is
frozen** — legacy/1.13/UB/mod maps must keep loading byte-for-byte — so every fix
here is read-side only: where an OLD struct embeds a `CHAR16` (2 bytes on disk,
4 in memory on mac/Linux) or a pointer (4 on disk, 8 in memory), the in-memory
field is narrowed to a fixed-width on-disk slot (`UINT16` / `UINT32`) and
widened/zeroed at the conversion operator, so the struct's POD region matches the
original 32-bit layout exactly. This is the same breaker set as the save work,
in the map path.

These bugs manifested as the **Omerta intro being unplayable** on the port: the
kid + Fatima never spawned in A9, and giving Fatima the letter crashed.

### Fixes (merged)

| Version path | Struct | Breaker | Fix |
|---|---|---|---|
| v5.0 detailed placement | `OLD_SOLDIERCREATE_STRUCT_101` | `CHAR16 name[10]` + `SOLDIERTYPE* pExistingSoldier` scrambled every field after `name` → `LoadSoldiersFromMap` derailed at first CIV → no NPCs | `UINT16 name` + `UINT32` slot; widen/NULL at operators |
| v<8.0 NPC schedules | `_OLD_SCHEDULENODE`, `_OLD_SCHEDULENODE_PRE_ITS` | leading `next` pointer (8 vs 4) shifted every schedule field by 4 | `next` → `UINT32` slot (transient link, rebuilt on load) |
| give-item dialogue | `uiApproachData` carrier | smuggles an `OBJECTTYPE*` but typed `UINT32` → pointer truncated at `InitiateConversation`, crash on cast-back in `Converse`/`ReturnItemToPlayerIfNecessary` | widen to `uintptr_t` end-to-end |
| v6.0.27–6.0.30 detailed placement | `_OLD_SOLDIERCREATE_STRUCT` | same `CHAR16 name` + `SOLDIERTYPE*` as v5.0 | `UINT16 name` + `UINT32` slot (branch `fix-v6x-map-soldier-load`) |

### Audit — version-branched map structs verified safe (no change needed)

All other structs blob-loaded in a version branch are scalar-only (pure
fixed-width members, no `CHAR16`/pointer/`long`/vtable), so they're byte-identical
on every target: `MAPCREATE_STRUCT`, `BASIC_SOLDIERCREATE_STRUCT`, `DOOR`,
`EXITGRID`, the `WORLDITEM` family, `OBJECTTYPE` (5-byte POD), `LBENODE`. Note
`SoldierID` is a 2-byte `UINT16` wrapper with no vtable, so it serializes safely
where it appears.

### Install map-version inventory (test game, 2026-05-23)

Parsed from each `.dat` header (`FLOAT major @0`, `UINT8 minor @4`):

| Source | Count | Versions |
|---|---|---|
| `Data/Maps.slf` | 261 | 127× v5.0.24, 134× v5.0.25 |
| `Data/Maps` (loose) | 9 | v5.0.25 |
| `Data-1.13/Maps` | 7 | 1× v5.0.25, 5× v7.0.31, 1× v8.0.31 |
| `Data-UB/Maps` | 24 | 19× v5.0.25, **5× v6.0.26** (`I10A`, `I13`, `I13_B1`, `J13_B1`, `K16`) |

### ⚠️ Pending runtime verification

- **UB v6.0.26 maps** are untested at runtime. They do **not** exercise the v6.x
  fix: the soldier loader gate is `major>=6.0 && minor>26` (strict), so minor=26
  routes through the already-verified `OLD_SOLDIERCREATE_STRUCT_101` (v5.0) path.
  They should "just work" — confirm by entering UB sectors I10/I13/J13/K16
  (+ basements).
- **The v6.x `_OLD_SOLDIERCREATE_STRUCT` path** (v6.0.27–6.0.30) is not present in
  this install, so the fix is mechanically identical to the verified v5.0 one but
  never runtime-exercised. Needs a mod shipping v6.0.27+ maps to confirm.

Verified working so far: the full Omerta intro (kid + Fatima in A9, letter handoff,
escort through A10 + A10 basement which is a v7.0 map, Ira recruited).
