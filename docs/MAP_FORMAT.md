# On-disk map (`.dat`) file format

Reference for the JA2 1.13 sector-map format as this engine reads it today. The
format is **frozen** — vanilla, 1.13, Unfinished Business and mod maps must keep
loading byte-for-byte — so this documents the existing layout, not a proposal.

Authoritative source: `TileEngine/worlddef.cpp` — `LoadWorld()` (read, ~line
2807) and `SaveWorld()` (write, editor-only, `#ifdef JA2EDITOR`, ~line 1787). The
two are mirror images; where this doc says "offset" it means stream order, not a
fixed byte offset (most sections are variable-length).

All integers are **little-endian**. The file is read into one buffer and walked
with a cursor (`LOADDATA(dst, buf, n)` copies `n` bytes and advances).

## Versions

Defined in `TileEngine/worlddef.h`:

| Constant | Value | Meaning |
|---|---|---|
| `VANILLA_MAJOR_MAP_VERSION` | `5.00` | original Sir-Tech maps |
| `VANILLA_MINOR_MAP_VERSION` | `25` | " |
| `MAJOR_MAP_VERSION` | `8.0` | current writer (teamsize bump 7.0→8.0) |
| `MINOR_MAP_VERSION` | `31` | `MINOR_MAP_ITEMFLAG64` (current) |

Minor-version milestones that gate on-disk layout: **25** vanilla; **<27** old
Russian 6.0 quirk block; **<29** 1-byte room numbers (else 2-byte); **27/28/30/31**
item/object-flag widenings (`MINOR_MAP_OVERHEATING`, `MINOR_MAP_REPAIR_SYSTEM`,
`MINOR_MAP_ITEMFLAG64`).

The engine only ever *writes* two self-consistent targets: vanilla `5.0.25` or
current `8.0.31`. Versions `5.0.24`, `6.0.x`, `7.0.x` exist only as **legacy
read-only** input.

## Container — `Maps.slf`

Most maps ship inside `Data/Maps.slf` (an SLF archive); mods/UB also use loose
`Data*/Maps/*.dat`. SLF layout:

- **`LIBHEADER`** (532 bytes): `sLibName[256]`, `sPathToLibrary[256]`,
  `iEntries` (`INT32` @512), …
- **Data**: each entry's bytes (a whole `.dat` map) concatenated.
- **Directory** at end of file (`filesize − iEntries*280`): `iEntries` ×
  **`DIRENTRY`** (280 bytes): `sFileName[256]`, `uiOffset` (`UINT32` @256),
  `uiLength` (`UINT32` @260), …

A map's `.dat` content begins at `uiOffset`; its version is the first 5 bytes
there (see header below).

## `.dat` structure (stream order)

### 1. Header

| Field | Type | Condition |
|---|---|---|
| major version | `FLOAT` (4B) | always |
| minor version | `UINT8` | `major >= 4.0` |
| world rows | `INT32` | **`major >= 7.0`** (older maps are fixed 160×160 = `OLD_WORLD_ROWS/COLS`) |
| world cols | `INT32` | `major >= 7.0` |
| `uiFlags` | `UINT32` | always — section presence bitmask (below) |
| tileset ID | `INT32` | always |
| soldier POD size | `UINT32` | always (`SIZEOF_SOLDIERTYPE_POD`; informational) |

> Note the write side (`SaveWorld`) emits rows/cols when the map is *not* exactly
> vanilla `5.0.25`, but the read side keys strictly on `major >= 7.0`. Only matters
> for the two writer targets, which stay consistent (5.0.25 → no dims; 8.0 → dims).

**`uiFlags`** (`worlddef.cpp:60`) — which trailing sections exist:

| Bit | Flag |
|---|---|
| `0x001` | `MAP_FULLSOLDIER_SAVED` |
| `0x002` | `MAP_WORLDONLY_SAVED` |
| `0x004` | `MAP_WORLDLIGHTS_SAVED` |
| `0x008` | `MAP_WORLDITEMS_SAVED` |
| `0x010` | `MAP_EXITGRIDS_SAVED` |
| `0x020` | `MAP_DOORTABLE_SAVED` |
| `0x040` | `MAP_EDGEPOINTS_SAVED` |
| `0x080` | `MAP_AMBIENTLIGHTLEVEL_SAVED` (underground only) |
| `0x100` | `MAP_NPCSCHEDULES_SAVED` |

### 2. Tile data (per grid cell, `iWorldSize = rows*cols` cells)

1. **Heights** — `INT16` × `iWorldSize` (one `sHeight` per cell).
2. **Layer counts** — 4 × `UINT8` per cell, nibble-packed:
   - byte0: low nibble = #land; high nibble = world flags (OR'd into `uiFlags`)
   - byte1: low = #objects; high = #structs
   - byte2: low = #shadows; high = #roof
   - byte3: low = #onroof; high = unused
   (max 15 per layer per cell — `LayerCount > 15` aborts the save.)
3. **Layer streams**, each iterating all cells then that cell's count:
   - **Land**: `UINT8 type`, `UINT8 subIndex`
   - **Object**: `UINT8 type`, **`UINT16 subIndex`** (16-bit — ROADPIECES has >300 subindices); `type >= FIRSTPOINTERS` skipped
   - **Struct**: `UINT8 type`, `UINT8 subIndex` (minor ≤ 25: `UNDERFLOW_FILLER` phantom-struct patch)
   - **Shadow**: `UINT8`, `UINT8`
   - **Roof**: `UINT8`, `UINT8`
   - **OnRoof**: `UINT8`, `UINT8`

   Each `(type, subIndex)` → tile index via `GetTileIndexFromTypeSubIndex`.

### 3. Old-6.0 discard block

If `major == 6.0 && minor < 27`: read **37 × `INT32`** and discard (legacy
"Russian 6.0" maps; major is then treated as 5.00). The bytes are skipped, never
interpreted.

### 4. Room info — per cell

`INT8` if `minor < 29`, else **`UINT16`** (DBrot widened room numbers to full
`UINT16`), × `iWorldSize`.

### 5. Trailing sections (each present iff its `uiFlags` bit is set, in this order)

| Order | Flag | Loader | Notes |
|---|---|---|---|
| a | `MAP_WORLDITEMS_SAVED` | `LoadWorldItemsFromMap` | count (`UINT32`) + `WORLDITEM`/`OBJECTTYPE` records |
| b | `MAP_AMBIENTLIGHTLEVEL_SAVED` | inline | `BOOLEAN gfBasement`, `BOOLEAN gfCaves`, `UINT8 ambientLevel` |
| c | `MAP_WORLDLIGHTS_SAVED` | `LoadMapLights` | light sprites + placements |
| — | (always) | `LoadMapInformation` | `MAPCREATE_STRUCT` (entry points, etc.) |
| d | `MAP_FULLSOLDIER_SAVED` | `LoadSoldiersFromMap` | basic + detailed placements (NPCs/enemies/civs) |
| e | `MAP_EXITGRIDS_SAVED` | `LoadExitGrids` | count + `EXITGRID` records |
| f | `MAP_DOORTABLE_SAVED` | `LoadDoorTableFromMap` | count + `DOOR` records |
| g | `MAP_EDGEPOINTS_SAVED` | `LoadMapEdgepoints` | pathing edge points (regenerated if absent/old) |
| h | `MAP_NPCSCHEDULES_SAVED` | `LoadSchedules` | count + `SCHEDULENODE` records |

`MAPCREATE_STRUCT` always loads (not flag-gated). After all sections:
`InitLoadedWorld()`, edgepoint regeneration if needed, building generation, radar
bitmap load.

## Version-divergent records

Several trailing-section records are blob-read through an era-specific struct
chosen by version. The version gates:

| Record | gate | struct used |
|---|---|---|
| **detailed soldier placement** | `major>=6.0 && minor>26` ? (`major<7.0` ? `_OLD_SOLDIERCREATE_STRUCT` : `MAPDISK_SOLDIERCREATE_STRUCT`) : `OLD_SOLDIERCREATE_STRUCT_101` | — |
| **basic soldier placement** | `major<7.0` ? `_OLD_BASIC_SOLDIERCREATE_STRUCT` : `BASIC_SOLDIERCREATE_STRUCT` | — |
| **schedule node** | `major<7.0` ? `_OLD_SCHEDULENODE` : (`major<8.0` ? `_OLD_SCHEDULENODE_PRE_ITS` : `MAPDISK_SCHEDULENODE`) | — |
| **door** | `major<7.0` ? `_OLD_DOOR` : `DOOR` | — |
| **exit grid** | `major<7.0` ? `_OLD_EXITGRID` : `EXITGRID` | — |
| **world item / object** | `_OLD_WORLDITEM` / `OLD_WORLDITEM_101` / `WORLDITEM` + `OBJECTTYPE` / `OLD_OBJECTTYPE_101` by version+minor | — |
| **map info** | `major<7.0` ? `_OLD_MAPCREATE_STRUCT` : `MAPCREATE_STRUCT` | — |

So the practical version → soldier-placement path mapping is:

- **5.0.x, 6.0.≤26** → `OLD_SOLDIERCREATE_STRUCT_101` (embedded old inventory)
- **6.0.27–6.0.30** → `_OLD_SOLDIERCREATE_STRUCT`
- **7.0.x, 8.0.x** → `MAPDISK_SOLDIERCREATE_STRUCT` (+ separate OO inventory)

### 64-bit portability

These era structs were the source of the macOS/Linux map-load bugs: where an OLD
struct embedded a `CHAR16` (2 bytes on disk, 4 in memory) or a pointer (4 vs 8),
the raw blob read mis-aligned every following field. The fixes narrow the
in-memory field to a fixed-width on-disk slot and convert at the boundary — the
on-disk format is unchanged. Full breakdown, the audit of which records are
scalar-safe, and the per-install map-version inventory live in
[`SAVE_FORMAT.md` → "Map-file version-branched loaders"](SAVE_FORMAT.md).
