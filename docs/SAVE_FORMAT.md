# Portable save / data file format

*Investigation for the cross-platform save-game problem on the JA2 1.13 SDL3
port (macOS/Linux). Companion to [SDL3_PORT.md](SDL3_PORT.md).*

## TL;DR

Upstream JA2 used implicit C++ memory dumps for much of its save format. This
port is replacing those dumps with explicit, versioned, little-endian field
serialization. Tactical actors completed that cutover at save baseline 1003:
their live object layout is no longer a save schema and earlier saves are
deliberately rejected.

The current `.sav` also has an engine-owned outer container. The portable
domain stream remains at offset zero, followed by mandatory runtime-checkpoint
and package-state sections plus an exact-length, independently checksummed
trailer. See [Runtime save container](RUNTIME_SAVES.md). This outer contract
does not replace the field-by-field work described below; it gives that
migration one bounded, inspectable transaction.

---

## Historical problem

The upstream actor record was written as raw struct bytes:

```cpp
FileWrite(hFile, actorBytes, objectLayoutPrefixSize, &bytesWritten);
```

That prefix size depended on the building compiler and platform ABI. Other
legacy records still use related `SIZEOF_*_POD` mechanisms, including
`MERCPROFILESTRUCT`, `OBJECTTYPE`, `WORLDITEM`, and `MILITIA`; those are tracked
separately from the completed tactical-actor cutover.

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

### Legacy-format consequence

- A save written on macOS is *self-consistent* (mac→mac read-back can work) but
  **byte-incompatible with Windows saves**.
- Reading any **shipped Win32-format file** (`Prof.dat`, maps) on macOS is broken
  unless that reader is fixed per-file.
- Some structs were **partially migrated** to field-by-field
  (`MERCPROFILESTRUCT::Load` uses `ReadFieldByField`) — and that helper only
  handles **alignment padding, not width**, so it currently reads
  `sizeof(CHAR16)`=4-byte chars from a 2-byte-per-char file. Where a struct's
  Save and Load don't use matching mechanisms, **even mac→mac is corrupt.**

### What the migration built on

- A save **version** field (`guiCurrentSaveGameVersion` / `SAVE_GAME_VERSION`),
  with a precedent for format breaks at a version boundary
  (`NIV_SAVEGAME_DATATYPE_CHANGE = 102`, "before this we used the old structure
  system"). So a clean new-format baseline at a new version number is idiomatic.
- `ReadFieldByField()` (padding-aware) and a half-finished field-by-field
  migration in `MERCPROFILESTRUCT`/`TacticalActor` load paths.

---

## Options considered

| # | Approach | Portable? | Effort | Risk | Notes |
|---|---|---|---|---|---|
| **A** | `CHAR16 → char16_t` globally (stracciatella's choice) + keep raw blobs | Fixes wide-char only; **padding/type-size still fragile** | **High** | High | Cascades into ~all text APIs (`swprintf`/`wcs*`/font `GetIndex`/UI). Doesn't fix padding (#2) or `long`/enum (#3). |
| **B** ✅ | **Explicit field-based serialization** (typed Writer/Reader, 16-bit wide-string helpers, defined endianness) | **Fully** (no ABI/padding/wchar dependence) | Medium-high | **Low** | Localized to save/load. Fixes all 3 breakers. Matches the direction the code already started. |
| **C** | "Completely different": tagged/library format (JSON / MessagePack / Protobuf / Cap'n Proto / SQLite) | Fully + self-describing/versioned | Very high | Med | Great greenfield; impractical retrofit — no C++ reflection, so you still enumerate every field, plus rewrite all version-migration logic and eat size/perf for big maps. |
| **D** | `#pragma pack(1)` POD **mirror** structs (UINT16 chars, fixed-width ints) serialized as blobs | Fully (if endianness defined) | Medium | Med | Fewer call sites than B, but mirror structs silently drift out of sync with the live structs. |

---

## Implemented direction — Option B

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

### Historical migration order

1. Land the serializer core + helpers (no behaviour change yet).
2. Convert the leaf structs first (OBJECTTYPE, REAL_OBJECT, INVENTORY_IN_SLOT,
   WORLDITEM, MILITIA) — small, high-reuse.
3. Convert MERCPROFILESTRUCT, SOLDIERCREATE_STRUCT, TacticalActor (finish the
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
- ☑ **`TacticalActor`** (the central record), ~550 fields, via the field
  visitor. Its former generic AI/flags/timing/drug/statistics/pathing wrappers
  are live domain components while the visitor retains their historical stream
  sections and ordering. ~20 interleaved runtime
  pointers use `ar.ptr()` (written as nothing, NULL on load — re-derived by
  the runtime-state reset and palette/world rebuild, matching old behaviour
  which only persisted garbage pointer values). `signed long` pinned to 32-bit
  via `ar.slong`; `SoldierID` via its UINT16 `.i`. Builds on
  JA2/JA2UB/MAPEDITOR.
- ☑ Audit the **rest of the save stream** in `SaveLoadGame.cpp` and the other
  save modules. This was a portability audit, not a wholesale serializer
  rewrite: records made only of fixed-width scalars were proven ABI-stable and
  deliberately retain their established raw bytes, while every record carrying
  a pointer, `long`, or `CHAR16` breaker was migrated or handled explicitly.
  The complete classification is recorded under [Cross-platform parity](#cross-platform-parity-done).
- ☑ Gate the loader at `PORTABLE_SAVE_FORMAT` / save version 1003 so older
  layouts fail before format-dependent reads.
- ☑ Add direct golden-byte coverage for every `SaveSerializer` primitive,
  signed edges, floating bit patterns, `str8`, non-ASCII `wstr`, the shared
  field visitors, and representative production header/path schemas. A
  canonical four-section, data-free fixture is emitted and consumed in the
  Windows/macOS/Linux matrix and Linux ASan job.
- ☐ Cross-check an actual complete campaign save produced and loaded by each
  platform. This is runtime/playtest verification, separate from the now
  deterministic byte-schema proof.

### Verification

The test framework covers the engine-owned outer container and its application
transaction: section round trips, bounds, corruption, exact runtime matching,
package contracts, staged restore, and incomplete-save cleanup run in
engine-core and headless tests. `save_serializer_golden_tests` adds the inner
byte-schema proof: literal little-endian bytes for every primitive and signed
edge, floating bit patterns, fixed and non-ASCII strings, visitor-only pointer
semantics, and the production `SAVED_GAME_HEADER`/`PathSt` field lists. It seals
those into one canonical four-section fixture and reads it back entirely in
memory, without installed game data.

That golden fixture proves deterministic schema bytes; it does **not** pretend
to exercise every live subsystem that contributes to a real campaign save.
Full-domain serialization and reconstruction still require **runtime
playtesting** (save → quit → reload), followed by exchanging a complete
save among Windows, macOS, and Linux.

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

### Field visitors and tactical actors

`SaveFieldWriter` and `SaveFieldReader` expose the same by-reference operations
over a writer or reader. A single `template<class Ar> Xfer(Ar&, T&)` field list
therefore defines both directions without relying on object padding, alignment,
pointer width, or `wchar_t` width.

Runtime pointers are never persisted. `ptr(T*&)` emits no bytes and clears the
pointer on load; `retiredPtr()` is a zero-byte landmark for positions that were
present in an earlier explicit field list but never represented data. Runtime
render bindings, palettes, animation surfaces, modular AI plans, and pending
item transactions are reconstructed or reset after loading.

Save baseline 1003 completes the tactical-actor cutover:

- `TacticalActor` is a canonical aggregate of private typed components. It has
  no raw/POD prefix, `endOfPOD` marker, reserved compatibility tail, placeholder
  pointers, or v101 mirror record.
- `SaveTacticalActor`, `LoadTacticalActor`, and
  `ComputeTacticalActorChecksum` are save-system operations rather than actor
  methods. `XferTacticalActor` visits the component values explicitly.
- Loading is replacement, not overlay: it calls `initialize()` first, then
  restores persistent component values. Runtime-only state cannot leak from a
  previously used record.
- The old ten-byte compatibility tail is not emitted. Saves from baseline 1002
  and earlier are intentionally rejected before format-dependent reads.
- Inventory, key-ring, and strategic-path payloads retain their current
  semantic encodings, but are owned by `SoldierInventory`,
  `SoldierKeyRingComponent`, and `SoldierStrategicPathComponent` rather than
  pointer fields on a monolithic record.
- Process-local face, level-node, animation-tile, palette, and surface-cache
  state remains detached on load and is rebuilt by its owning subsystem.

This save break does **not** alter installed game data. Existing maps, XML, Lua,
profiles, artwork, audio, and campaign packages retain their established
formats. The four-byte actor-size slot in a map header remains in the stream for
map compatibility, is ignored by readers, and is now written as zero instead of
exposing a platform-specific C++ object size.

### Where each struct's layout lives

There is no separate schema file — the layout *is* the per-struct field list:

- Item/object chain, tactical actors, profiles, and dealer/militia leaves:
  `Ja2/SaveLoadGame.cpp` (`Xfer*` templates plus explicit save/load functions).
  The representative save-header and strategic-path field lists live beside
  their public records in `Ja2/SaveLoadGame.h`, so the data-free golden target
  instantiates the production schema rather than a copied test schema.
- `MILITIA`: `Strategic/MilitiaIndividual.cpp`.
- Serializer + visitor adapters: `sgp/SaveSerializer.{h,cpp}`.

### Versioning

`SAVE_GAME_VERSION` (in `Ja2/GameVersion.h`) gates old vs new format at load. It is
now `PORTABLE_SAVE_FORMAT = 1003` — a generational jump clear of upstream's
sequential numbering (~186), marking the clean break and avoiding collisions with
future 1.13 increments. (1000 was the initial portable format; 1001 added
field-by-field `ROTTING_CORPSE_DEFINITION` serialization; 1002 moved strategic
events into the runtime-owned EVQ2 value stream; 1003 removed the actor POD
layout, compatibility tail, and v101 conversion record.) `LoadSavedGame`
**rejects any save below the current `PORTABLE_SAVE_FORMAT`** up front
(before any format-dependent read), so pre-migration saves fail cleanly instead of
mis-reading old bytes as v2. `uiSavedGameVersion` is the first field in the file,
so the gate is available before any version-dependent domain payload is read.

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

## Save-stream audit (retained serialization beyond the migrated structs)

The structs migrated to v2 (soldiers/profiles/items/etc.) are the bulk of save
data and the wide-char-heavy part. The **rest** of the save stream deliberately
retains a mix of ad-hoc length-prefixed strings and ABI-stable raw struct blobs.
It was audited in two buckets:

### Same-platform crash / corruption risks (priority — these actually bite)

The dangerous patterns are (a) a *file-controlled* length read into a fixed/typed
buffer, and (b) a scalar global serialized with a wrong-width `sizeof`. Found and
fixed:

- **map-screen messages** (`Utils/message.cpp`) — `*2` wchar size + unbounded
  read into a fixed buffer. *(fixed)*
- **explosion queue count** (`TileEngine/Explosion Control.cpp`) — `UINT8` global
  read/written as `sizeof(UINT32)` → 3-byte global overflow. *(fixed)*
- **email subject** (`Ja2/SaveLoadGame.cpp`) — legacy `*2` wchar sizing and a
  fixed destination. Save now bounds the live subject to `MAIL_STRING_SIZE`;
  load validates a nonzero, `sizeof(CHAR16)`-aligned byte count, allocates that
  exact size, requires its terminator, rejects duplicate/out-of-range message
  IDs, and publishes the staged inbox only after every record succeeds. *(fixed)*

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
| `TacticalStatusType` | CHAR16 top-message + runtime-owned turn/creature/interrupt state | `wstr`; SoldierID via `.i`; scalar `Team[]` as bytes; turn, creature-quote, and interrupt-control values captured/restored through `TacticalWorldSession` at their established positions; its tail remains surgery-target `u16`, pending-interrupt `u8`, then disabled-interrupt boolean `u8` |
| `MERCPROFILESTRUCT`, `TacticalActor`, `SOLDIERCREATE_STRUCT` | CHAR16 names | (original migration) `wstr` |
| email subject, map-screen messages | CHAR16 `*2` | `sizeof(CHAR16)` + exact, bounded reads; email list staged before publication |
| `VEHICLETYPE` | ptrs (pMercPath, pPassengers) | skip; passenger profile IDs as fixed `u32` |
| `PathSt` (vehicle/militia/merc paths) | ptrs (pNext/pPrev) | shared node helper; links rebuilt |
| `STRATEGICEVENT` | linked-list `next`, runtime ID | EVQ2 queue section: magic/version/count plus six semantic fields per node; links and IDs rebuilt transactionally |
| `UNDERGROUND_SECTORINFO` | linked-list `next` | skip; rebuilt on load |
| `LaptopSaveInfoStruct` | 2 array ptrs | skip (arrays saved separately); scalars/sub-structs as bytes |
| `BULLET` | ptrs (firer/tracer/anitiles) | skip; firer re-derived from ID |
| `GROUP` + `WAYPOINT` | ptrs (waypoints/union/next) | skip; sub-lists saved separately, links rebuilt |

The Personnel ownership extraction does not change this schema. Its fixed
dead, fired, and other profile arrays keep their established order, width, and
`-1` empty sentinel. `PersonnelRosterModel` only validates and deduplicates the
rendered view and stages category mutations in memory before copying them back
to those same arrays.

The Campaign History safety extraction likewise preserves its existing
campaign header and `Incident_Stats` POD field order and widths. Saves now
require exact byte counts and initialize the already-persisted filler and
alignment bytes;
loads validate into the object transactionally and restore the previous live
ledger after a short header or incident record. The configured report limit
still retains the newest tail of the serialized history, but limits larger
than the saved count now correctly retain every available report instead of
underflowing the cutoff. No save-version or on-disk layout change is required.

`signed long` fields (e.g. TacticalActor's `lUnregainableBreath`) are pinned to
32-bit (`ar.slong`). Same-platform saves were always fine; this pass makes saves
**shareable across Win/Lin/Mac**. The golden test now pins `slong` and every
other serializer operation to exact bytes across the platform matrix. Actual
campaign-state reconstruction remains a separate playtest obligation.

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
| v5.0 detailed placement | `OLD_SOLDIERCREATE_STRUCT_101` | `CHAR16 name[10]` + `TacticalActor* pExistingSoldier` scrambled every field after `name` → `LoadSoldiersFromMap` derailed at first CIV → no NPCs | `UINT16 name` + `UINT32` slot; widen/NULL at operators |
| v<8.0 NPC schedules | `_OLD_SCHEDULENODE`, `_OLD_SCHEDULENODE_PRE_ITS` | leading `next` pointer (8 vs 4) shifted every schedule field by 4 | `next` → `UINT32` slot (transient link, rebuilt on load) |
| give-item dialogue | `uiApproachData` carrier | smuggles an `OBJECTTYPE*` but typed `UINT32` → pointer truncated at `InitiateConversation`, crash on cast-back in `Converse`/`ReturnItemToPlayerIfNecessary` | widen to `uintptr_t` end-to-end |
| v6.0.27–6.0.30 detailed placement | `_OLD_SOLDIERCREATE_STRUCT` | same `CHAR16 name` + `TacticalActor*` as v5.0 | `UINT16 name` + `UINT32` slot (branch `fix-v6x-map-soldier-load`) |

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
