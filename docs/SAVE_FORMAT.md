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

- ☐ Serializer core (`sgp/SaveSerializer.{h,cpp}`) — portable LE primitives +
  `wstr` (16-bit on disk ↔ `wchar_t` in memory).
- ☐ Migrate leaf structs (OBJECTTYPE, REAL_OBJECT, INVENTORY_IN_SLOT, WORLDITEM,
  MILITIA), then MERCPROFILESTRUCT / SOLDIERCREATE_STRUCT / SOLDIERTYPE, then
  containers / sector data.
- ☐ Audit top-level `SaveLoadGame` `FileWrite`/`FileRead` for bare
  `int`/`long`/`enum`/`BOOLEAN` and raw struct blobs.
- ☐ Bump `SAVE_GAME_VERSION`; verify save→quit→reload + round-trip diff on macOS;
  cross-check a save loads on Windows/Linux.

### Verification

There is no test framework in the repo yet (that's a separate task). Until then,
verification is by **playtesting**: save → quit → reload, and confirm a save made
on one OS loads on another. When a test framework lands, the ideal coverage for
this work is (a) serializer round-trip tests (write → read → assert-equal for
every primitive incl. non-ASCII `wstr`) and (b) **golden-byte tests** that assert
the exact little-endian bytes for known values — those run in CI on Win/Lin/Mac
and would *prove* cross-platform format parity rather than assuming it. The
serializer is easy to make memory-buffer-backed for that.
