# JA2 SDL3 Port — Optimization & Modernization Menu

Output of a read-only optimization survey (15 analysts across the hot subsystems,
each finding adversarially triaged by a second agent). **95 opportunities found,
57 kept** after triage; 38 dropped as cold / premature / too risky. Nothing here is
implemented — it's a menu to pick from.

Hotness tags: **[per-frame]** / **[per-tick]** / **[per-turn]** are genuinely hot;
**[load]** / **[event/UI]** / **[rare]** are nice-to-have. Effort S/M/L, risk
low/med. Paths are repo-relative.

> Convention for this work: each item (or small bundle) goes on its own branch; no
> pushing until reviewed.

---

## 1. Quick wins (S, low risk, real benefit)

### Hot-path (actual runtime gains)

| # | Item | File:func | Change | Win | Hot? |
|---|------|-----------|--------|-----|------|
| Q1 | **Skip atan2 in LOS unless ray hits a wall** | `Tactical/LOS.cpp:1383` `LineOfSightTest` | Drop the unconditional `ddHorizAngle = atan2(...)`; add a `BOOLEAN fHorizComputed` flag and compute lazily right before each `ResolveHitOnWall` (1741, 1826). dDeltaX/Y still in scope. | One atan2 saved per LOS test on the common (no-wall) line, × O(M²) per-turn sight scan. | **[per-turn]** |
| Q2 | **Stop computing TerrainActionPoints twice per A\* neighbor** | `Tactical/Points.cpp:719` `EstimateActionPointCost` | Add internal `ActionPointCost` overload taking precomputed `sTileCost`+`sSwitchValue`; EstimateActionPointCost already has both, so pass them through instead of letting `ActionPointCost` recompute at :399/:408. | Halves TerrainActionPoints (door/fence/water/hidden-struct checks) per neighbor in the A\* inner loop. | **[per-frame]** |
| Q3 | **Hoist `RangeChangeDesire()` out of the cover tile loop** | `TacticalAI/FindLocations.cpp:1078` `FindBestNearbyCover` | `const INT32 iRCD = RangeChangeDesire(pSoldier);` once before the loop; reuse at 811/815/1078/1081 (called up to 2×/tile). | Removes up to ~2× `GuySawEnemy` (O(teams×soldiers)) scans per reachable cover tile. | **[per-turn]** |
| Q4 | **Skip cover LOS raycast when its result is dead** | `TacticalAI/FindLocations.cpp:1052` `FindBestNearbyCover` | Hoist `const bool fWantProneCover = fAIBetterCover && RangeChangeDesire(pSoldier) < 4;`; guard the per-(tile,threat) `LocationToLocationLineOfSightTest` with it, and short-circuit once `fProneCover==FALSE` for a tile. | Removes O(tiles×threats) raycasts when soldier isn't in defending posture; caps to ≤1 raycast/tile otherwise. LOS raycast is the most expensive primitive here. | **[per-turn]** |
| Q5 | **Reorder FindFlankingSpot guards; defer the edgepoint scan** | `TacticalAI/FindLocations.cpp:2734` `FindFlankingSpot` | Move bounds check (2718) above the direction/range math (also fixes an unvalidated-sGridNo read); compute dir/dist lazily after the cheap reachable/blacklist continues; make `FindNearestEdgePoint` test the LAST cheap continue. **Do NOT** swap it for a raw border check (changes AI behavior). | Removes a full sqrt-based edgepoint-array scan for tiles killed by cheap filters, per flank tile. | **[per-turn]** |
| Q6 | **Hoist invariant flags in light recompute** | `TileEngine/lighting.cpp:2131/2772` `LightDraw`/`LightErase` | `const UINT32 uiSpriteFlags = LightSprites[uiSprite].uiFlags;` once; precompute on-roof predicate + extra flag bits before the loop. Skip the generation-counter idea. | Removes per-node reloads of invariant sprite flags from the per-light-move recompute. | **[per-tick]** |
| Q7 | **Hoist `LightTileHasWall` out of the per-struct loop** | `TileEngine/lighting.cpp:829-863/972-1006` `LightAddTile`/`LightSubtractTile` | Compute `BOOLEAN fHasWall = LightTileHasWall(...)` once before the `while(pStruct)` loop; delete the shadowing recompute of `uiTileNo` at `LightTileBlocked:483/486`. | Eliminates N× atan2-per-tile during every light recompute (SOLDIER_RECHECKLIGHT fires per-step). | **[per-tick]** |
| Q8 | **`break` after first affordable autofire count** | `Tactical/Interface.cpp:2896` `DrawCTHIndicator` | Add `break;` after `uiMaxAutofire = uiCurBullet;`. The loop only records the first/largest affordable count. Reject the binary-search rewrite. | Common case goes from O(mag) `CalcTotalAPsToAttack` calls to 1–3, per aiming frame. | **[per-frame]** (autofire-aim UI) |
| Q9 | **Cheaper CTH circle trig** | `Tactical/Interface.cpp:3322` `DrawCTHIndicator` | Hoist `const double delta = RADIANS_IN_CIRCLE/Circ` out of the loop; compute angle once/iter feeding both cos+sin; switch to `cosf`/`sinf` (output cast to INT16 anyway). Avoid an incremental rotation accumulator (drifts). | Removes a per-point division + halves trig-arg arithmetic per aiming frame. | **[per-frame]** (aim cursor) |
| Q10 | **Cache SAM max coverage distance** | `Strategic/strategicmap.cpp:5372` `DoesSamCoverSector` | Lazily memoize `GetSAMMaxDistanceToCoveredSector(usSam)` in `static FLOAT[MAX_NUMBER_OF_SAMS]` (sentinel −1); reset at the reload point (:725). | O(256-cell sqrt scan) → O(1) read. Helps `UpdateAirspaceControl` + the airspace overlay. | **[per-frame]** when overlay shown |

### Cleanup / safety quick wins (small or no runtime gain — pitched honestly)

| # | Item | File:func | Note |
|---|------|-----------|------|
| Q11 | **Gate the per-decision AI diagnostic** | `TacticalAI/DecideAction.cpp:10358` `LogDecideInfo` | `if (!gfLogsEnabled) return;` first → short-circuits `LogKnowledgeInfo`'s two 1284-wide loops. **[per-turn]** but logging is ON by default, so pair with M-AI3 (make opt-in). |
| Q12 | **Delete dead GetMouseXY in RenderTiles** | `TileEngine/renderworld.cpp:1313` `RenderTiles` | Remove call + two unused locals (1222) + stale comment. Pure dead-code; not a perf win despite per-frame call. |
| Q13 | **Tighten MovementNoise scratch struct** | `Tactical/opplist.cpp:5302` `MovementNoise` | Move the `ADDITIONAL_TILE_PROPERTIES_VALUES` decl + drop the redundant memset into the feature-on branch. Clarity only. |
| Q14 | **Remove A\* pathfind timing log** | `Tactical/PATHAI.cpp:2167` `FindBestPath` | Delete `GetJA2Clock`/`sprintf`/`LiveMessage` (result never consumed). Stops a logfile write per path for ALT_PATHFINDING users (off by default). |
| Q15 | **`delete[]` on the binary heap** | `Tactical/BinaryHeap.hpp:78` `~CBinaryHeap` | `delete` → `delete[]` (UB fix); ideally swap raw ptr for `std::vector`. Drop the dead `__asm` block. POD so benign today, but UB. **[rare]** |
| Q16 | **Shrink the 91 MB A\* BSS array** | `Tactical/PATHAI.H:119` `AStarData` | `AStar_Data[MAX_ALLOWED_WORLD_MAX]` (91.6 MB BSS) → `std::vector<AStar_Data>` sized to `WORLD_MAX` at init. Win is commit-charge/binary hygiene (Windows), not RSS. **[load]** |
| Q17 | **Guard the 1284 startup `remove()` calls** | `TacticalAI/AIMain.cpp:398` `InitAI` | Gate the per-soldier log-delete loop on `gfLogsEnabled && DirExists("Logs")`. Removes 1284 unlink syscalls per sector init. **[load]** |
| Q18 | **Free old shades / build gear palettes once** | `Tactical/Faces.cpp:1474` `GetXYForRightIconPlacement_FaceGear` | **Min:** `MemFree` old `pShades[]` before overwrite in `SetPalettes` (stops a per-frame 6×256-PIXEL leak). **Better:** build gear shade palettes once (guard flag). Genuine **[per-frame]** leak fix in tactical. |
| Q19 | **Avoid the slot-vector by-value copies** | `Tactical/Items.cpp:2282/2412/2577/2371` (+`Items.h:168/172/173/257`) | `std::vector<UINT16> usAttachmentSlotIndexVector` → `const std::vector<UINT16>&` across the 4 attachment-validity functions. Kills O(slots²) heap allocs/copies per item-desc render. **[per-frame]** when inventory UI is up. |

> **Q18** (per-frame heap leak) and **Q19** (per-frame heap churn) are S-effort but
> carry real per-frame benefit — treat them as first-class wins, not just cleanups.

---

## 2. Medium projects (M effort, good payoff)

### Pathfinding
- **M-PF1 — A\* decrease-key: O(n) linear scan → O(log n).** `Tactical/BinaryHeap.hpp:169` `editElement/findData` (from `PATHAI.cpp:1211`). Add a node→heap-index map (an `int heapIndex` on `AStar_Data` or a parallel array keyed by gridno) updated on every heap move; `editElement` becomes a real decrease-key. **The single largest pathfinding speedup here** — on open maps most relaxations hit the "already-open, cheaper" branch. **[per-frame]**, risk medium (touches a core structure; pair with Q15's vector swap).
- **M-PF2 — Hoist path-invariant soldier facts out of the per-neighbor cost helpers.** `Tactical/Points.cpp:596` `ActionPointCost`/`TerrainActionPoints`. `FindBackpackOnSoldier` (inner double-loop over inventory), `HAS_SKILL_TRAIT(ATHLETICS/STEALTHY)`, `IsRiotShieldEquipped`, `IsDragging`, `SCUBA_FINS`, `UsingNewInventorySystem` are recomputed per tile. `PlotPath`/`FindBestPath` already hoist the backpack check — the modern A\* search path (`PATHAI.cpp:1073`) does not. Thread invariants in via pathing scratch fields. **[per-frame]**, risk medium. **Subsumes Q2** (do Q2 first as the cheap slice).

### AI (per-turn)
- **M-AI1 — Per-target sight adjustment cache in the O(n²) sight scan.** `Tactical/LOS.cpp:2325/2332/2339` `SoldierToSoldierLineOfSightTest`. `GetSightAdjustment`/`GetStealth`/`GetSightAdjustmentBasedOnLBE` depend only on the *target*, yet are recomputed per looker. Collapse the 3 duplicated lines into one local, then thread a precomputed value down from the scan, or a per-scan array keyed by target ubID invalidated in `InitSightArrays()`. **Risk: ~20 non-scan callers** (Weapons CTH, UI) must stay byte-identical — do NOT use a global static cache. **[per-turn]**, risk medium.
- **M-AI2 — CalcMoraleNew: precompute per-friend base threat.** `TacticalAI/AIUtils.cpp:3770`. Build a per-friend `CalcManThreatValue(pFriend, NOWHERE, FALSE, pSoldier)` base array before the opponent loop; inside, apply only the cheap +5/+10% position bonus. Guard with `!CREATURE_OR_BLOODCAT(pSoldier)` (creature path is position-dependent). Also fix the duplicate legacy `CalcMorale`. **[per-turn]**, risk low.
- **M-AI3 — Make AI decision logging skip String()/vsprintf when off.** `TacticalAI/AIMain.cpp` `DebugAI`. Add `extern BOOLEAN gfLogsEnabled;` to `ai.h`; rename impls `DebugAI_` (keep both overloads + default arg) and add one variadic macro `#define DebugAI(...) do{ if(gfLogsEnabled) DebugAI_(__VA_ARGS__);}while(0)`. Add an `AILOG(...)` macro and sweep `FindLocations.cpp` `SearchForItems` (~43 sites) + the rest of TacticalAI. **The overload/default-arg trap is the only thing that bites** — test 1-arg and 3-arg forms. **[per-turn]**, risk low. ⚠️ `gfLogsEnabled` is **TRUE by default** and DebugAI does fopen+fputs+fclose on **two** files per line — so in a normal install this verbose per-decision file logging is actually running. Strongly consider making it ini-opt-in.

### Opplist (per-turn / per-tick, crowded sectors)
- **M-OP1 — `AllTeamsLookForAll` O(N²): conservative distance prefilter.** `Tactical/opplist.cpp:1655`. Cache a per-looker *upper bound* on sight range (scope/elevation-adjusted max) once per pass; if `PythSpacesAway(looker,opp) > bound`, reject before the `DistanceVisible` env/light pipeline and LOS raytrace. Place inside `ManLooksForMan`. **Risk: the bound must provably be ≥ DistanceVisible's max** (roof +1/6, vision-range bonus, muzzle-flash override) or you silently change who-sees-whom — add a debug assert during rollout. **[per-turn]**, risk medium.
- **M-OP2 — Hoist looker-only work out of `DistanceVisible`.** `Tactical/opplist.cpp:1235`. **Change 1 (S, do first):** replace `SoldierHasLimitedVision(pSoldier)` at :1249 with `(gfAllowLimitedVision || tunnelVision > 0)` — looker already passes `tunnelVision`; kills a redundant full-inventory scan per pair, bit-identical. **Change 2 (M):** refactor `GetTotalVisionRangeBonus` so the light-independent equipment/trait scan is computed once per looker, only the light-threshold selection per-pair. **[per-tick]**, risk medium — profile first.
- **M-OP3 — `RadioSightings(EVERYBODY)`: iterate active slots, not all 1284.** `Tactical/opplist.cpp:3667`. Special-case the EVERYBODY branch to walk `MercSlots[]/guiNumMercSlots` (index opplist arrays by `pOpponent->ubID`); leave single-opponent as-is. 1284-iteration cold-struct scan → ~tens-wide per radio pass. **Verify no order-dependence** on ascending ubID. **[per-turn]**, risk medium.

### Save/Load (load-time, big Windows win)
- **M-SL1 — Buffer the save serializer.** `sgp/SaveSerializer.cpp:7` `SaveWriter::raw`/`SaveReader::raw`. Each scalar field is one `FileWrite`/`FileRead` (one syscall/field on Windows; `wstr`/`skip` one I/O call *per character*). Give SaveWriter an internal buffer flushed in `flush()`/dtor; give SaveReader a refill-on-drain read-ahead buffer. **No on-disk format change.** Caveats: writer must flush in dtor (per-record SaveWriters share one HWFILE — ordering); reader must keep the zero-fill-on-short-read tolerance. 1–2 orders of magnitude fewer I/O calls. **[load]**, risk low. **Subsumes both FileMan map-lookup findings.**
- **M-SL2 — Drop the `s_mapFiles` lookup on every file op.** `sgp/FileMan.cpp:416`. Replace `s_mapFiles[pFile].op == READ/WRITE` with the `tReadableFile::cast`/`tWritableFile::cast` non-null check already executed in each body; delete the std::map + operator[] default-insert footgun + insert/erase at open/close. **Precondition:** confirm no VFS file class implements both IReadable+IWritable (stock typedefs don't). **S/low**. **[load]**

### Physics / aiming (per-frame)
- **M-PH1 — Memoize throw/launch trajectory force.** `TileEngine/physics.cpp:2248` `CalculateForceFromRange`. Per aim frame does a `CreateItem` + up to 8 full physics-sim iterations to derive a near-constant max force. Cache on key `(proxy-item {MORTAR_SHELL|HAND_GRENADE}, exact INT16 sRange, sSrcGridNo)` — **do not bucket sRange**; invalidate on sector load via the stored sSrcGridNo guard. **[per-frame]** (grenade/mortar/GL aim), risk medium.
- **M-PH2 — Skip the full OBJECTTYPE deep-copy for test objects.** `TileEngine/physics.cpp:67` `CreatePhysicalObject`. For `fTestObject != NO_TEST_OBJECT`, replace `pObject->Obj = *pGameObj` (deep-copies a `std::list<LBENODE>`) with a scalar `Obj.usItem = pGameObj->usItem`; keep the full copy for real objects. **[per-frame]** during aiming, risk low.

### Render / Items (per-frame, conditional)
- **M-RD1 — Per-frame memo of logical BodyType.** `TileEngine/renderworld.cpp:2193` `RenderTiles`. When `TOPTION_USE_LOGICAL_BODYTYPES` is on (default), `bodyTypeDB->Find(pSoldier)` runs per-merc-node per-layer (author-flagged hotspot). Add a per-frame cache keyed by soldier ID + frame stamp, cleared at top of `RenderDynamicWorld`. **Per-frame memo, NOT "invalidate on appearance change."** Delete the dead `static numLayers`. **[per-frame]** (when enabled), risk low.
- **M-IT1 — Cache the item-description slot vector.** `Tactical/Items.cpp:5680` `SetAttachmentSlotsFlag` (via `GetItemSlots` → `RenderItemDescriptionBox`). The full `gMAXITEMS_READ` (up to 16001) scan reruns every frame the item-desc box is open. Memoize keyed on `{usItem, statusIndex, attachment-list signature, magSize}`. **Warm/interaction-time, NAS-only**, not a general FPS win. Fold in T-MOD3 (`pow(2,x)` → shift) while here. **[event/UI]**, risk medium.

---

## 3. Big projects (L effort, high payoff — honest about scope/risk)

- **B1 — LEVELNODE slab/arena allocator.** `TileEngine/worldman.cpp:44` `CreateLevelNode`. Every per-tile render-layer node is an individual `MemAlloc`+memset (25 alloc / 52 free sites), scattered across the heap; `RenderTiles` streams these lists every frame. Replace with a **fixed-block slab pool** (4K-node slabs, intrusive free-list through `pNext`) behind the existing `CreateLevelNode`/`DeleteLevelNode` signatures.
  - **Honest scope:** real payoff is **cache locality of the per-frame walk**, not alloc cost (most nodes alloc'd once at map load) — so a plain free-list does little; you need a slab/arena. Risk medium, effort M–L: the `pNext`/`pPrevNode` union is load-bearing and differs per layer; pool must reset cleanly on `TrashWorld`; this code has prior OOB/double-free history; the memset-zero default (`ubShadeLevel=LightGetAmbient`) must be preserved. **Stage it:** first centralize alloc/free behind a malloc-per-slab pool, profile `RenderTiles` with Instruments, then decide. Do **not** convert to std::vector/RAII — the intrusive list is load-bearing.

- **B2 — Items.xml parser: replace the ~250-way strcmp chains with hash dispatch.** `Utils/XML_Items.cpp:110` (start, ~200 tags) and `:419` (end, ~1400-line if/else-if ladder, the bigger target — hit a compiler chain-length limit, lines 1028/1171/1176/1182/1188 forced to plain `if`). Build a function-local `static const std::unordered_map<std::string_view,int>` (tag → enum); switch on the result; keep each branch body verbatim.
  - **Honest scope:** **load-time only** (~tens-to-low-hundreds of ms cold parse of the 2 MB file) — a one-time startup reduction, not FPS. Rank below anything per-frame. **Critical pre-work:** the forced-`if` branches + line-327 precedence quirk mean a naive map could change whether some trailing tags are accepted outside ELEMENT context — diff the parsed `Item[]` array old-vs-new on real Items.xml to prove byte-identical. Lower-effort alt: just reorder the chain so common tags test first. Risk medium, effort L.

- **B3 — Structure-file DB: kill the manual-cleanup leak class (TIERED, don't big-bang).** `TileEngine/structure.cpp:62`.
  - **Tier 1 (S, low risk — DO FIRST, real bug):** fix the wrong-index double-free at `:2320/:2472/:2516` — `ppZStripInfo[uiLoop]` → `ppZStripInfo[ubLoop2]` in all three cleanup loops.
  - **Tier 2 (M, medium):** replace `AddZStripInfoToVObject`'s three identical manual-cleanup ladders with one RAII guard.
  - **Tier 3 (L, medium):** migrate owned arrays to `unique_ptr<T[]>`/`vector` and the list to `vector<unique_ptr<STRUCTURE_FILE_REF>>` (**stable addresses required** — raw `STRUCTURE_FILE_REF*` are cached in `TILE_CACHE_STRUCT`/`gAnimStructureDatabase`/`TILE_IMAGERY`; keep `pubStructureData` a raw byte buffer; update `CStructureDataReader` in lockstep).
  - **Honest scope:** **load-time only — not a perf item.** Value is safety/clarity. Tier 1 is the high-value, low-risk core.

---

## 4. Modernization themes (recurring patterns worth a systematic sweep)

- **T-MOD1 — Eager `String()`/vsprintf into AI logging.** The whole TacticalAI module calls `DebugAI(..., String(...))` with the format **always evaluated** before the disabled-check (`DecideAction.cpp`, `FindLocations.cpp`, `Attacks.cpp`, `Movement.cpp`, `AIUtils.cpp`). One `AILOG`/guarded-macro sweep (M-AI3) kills it module-wide. `DebugMsg` is already a release no-op — only the real `DebugAI` calls leak. **~256 sites tree-wide.** Biggest recurring per-turn waste.
- **T-MOD2 — Dense `MercPtrs[0..MAXMERCS=1284]` scans over cold structs instead of `MercSlots[]/guiNumMercSlots`.** Offenders: `RadioSightings` (M-OP3), `BetweenTurnsVisibilityAdjustments` (`opplist.cpp:3371`). Each touches a multi-KB SOLDIERTYPE (embeds a 1284-byte bOppList) for ~1200 inactive slots. Active-slot idiom already established in this file — sweep the rest. **[per-turn]**, S each, low risk.
- **T-MOD3 — `pow((double)2, x)` used as a bit shift.** `Tactical/Items.cpp:5723/5746/1511/6511/6619` → `1ULL << x` / `1u << x`. Clarity/correctness (bit masks), not perf. Fold into M-IT1.
- **T-MOD4 — Read-only `std::vector`/`std::list` args passed BY VALUE.** `ValidAttachment`/`ValidItemAttachmentSlot` (Q19, per-frame), `SaveRottingCorpsesToTempCorpseFile` (`Tactical Save.cpp:1690` +2 redecls), `GetNumberOfMovableItems` (`Tactical Save.cpp:776`, deep-copies a `vector<WORLDITEM>` of nested lists just to count). One "pass read-only args by const-ref" sweep; most cold but Q19 hot. Move duplicated `extern` decls (e.g. `Assignments.cpp:6453`) into headers.
- **T-MOD5 — Manual `MemAlloc`/`MemFree` whole-file buffers with leak-on-error → RAII.** `Utils/XML_Items.cpp:1824` `ReadInItemStats`, `XML_Sounds.cpp` `ReadInSoundArray`, `Ja2/SaveLoadGame.cpp:6375` `SaveFilesToSavedGame`/`LoadFilesFromSavedGame`. Use `std::vector<UINT8>`/`unique_ptr` + RAII for the expat handle. **Real defect:** `hSrcFile` leaks on 4 early-return branches in SaveFilesToSavedGame. Drop redundant pre-read memsets.
- **T-MOD6 — Near-duplicate functions that drift.** `ClosestSeenOpponent`/`ClosestSeenOpponentWithRoof` (`AIUtils.cpp:1509/1605`, ~90 dup lines differing by one disabled check) → one impl + two thin wrappers (don't re-enable the commented zombie clause). Legacy `CalcMorale` vs `CalcMoraleNew` (M-AI2). Triple-duplicated LOS adjustment block (M-AI1). Divergence is the bug risk.
- **T-MOD7 — Raw C lookup tables / `#define` dimensions → `constexpr std::array`.** `Tactical/opplist.cpp:138` `gubKnowledgeValue[10][10]`/`gbSmellStrength[3]`; `gbLookDistance[8][8]` → `std::array` filled directly in `InitSightRange` (deletes the `INT8 dummy[15][15]` "VC6-compat" scratch hack). **Keep `operator[]`, not `.at()`** (hot reads). Zero behavior change.
- **T-MOD8 — Linear scans that never break / O(n) where O(1) is cheap.** `GetFaceRelativeCoordinates` (`Faces.cpp:607`, scans full 200-entry table, no break, last-wins) — add `break;`. `LightSpriteGetFree` (`lighting.cpp:3118`, scans 4096 slots) — a single static search-hint index (not a full free-list). `FindLocations.cpp:1867` likely repeats the cover/RangeChangeDesire pattern. Low value individually; cheap.

---

## Recommended STARTER SET (best value-to-risk for a first pass)

Five items: each **S effort / low risk**, on a **genuinely hot path**, behavior-preserving,
touching a different subsystem (so they don't collide).

1. **Q1 — Lazy atan2 in `LineOfSightTest`** (`LOS.cpp:1383`). Hottest primitive (O(M²)/turn), one-flag change, bit-identical.
2. **Q2 — Stop double-computing `TerrainActionPoints` per A\* neighbor** (`Points.cpp:719`). Direct inner-loop saving; thin internal overload. On-ramp to M-PF2/M-PF1.
3. **Q4 (+Q3) — Skip dead cover LOS raycasts** (`FindLocations.cpp:1052`). One hoisted guard removes O(tiles×threats) of the most expensive AI primitive when the result is provably unused.
4. **Q18 — Fix the per-frame face-gear palette leak** (`Faces.cpp:1474`). Stops a steady tactical-render heap leak; ~5 lines for the minimum fix.
5. **Q19 — `const&` the attachment slot-vector** (`Items.cpp:2282` +3 + `Items.h`). Kills O(slots²) per-frame heap allocs/copies while inventory UI is open; mechanical.

**Then graduate to:** **M-AI3** (AI-logging guard — removes a real per-turn cost that's
*on by default* today, unlocks T-MOD1) and **M-SL1** (serializer buffering — the standout
load-time win, huge on Windows, no format change). If you want one Big project after that,
**M-PF1** (A\* O(log n) decrease-key) is the largest single algorithmic speedup available.

---

## Survey method

15 analysts (one per subsystem group: render-core, blitters, LOS, pathfinding, AI-decide,
AI-util, opplist, world-struct, lighting, items-weapons, soldier-faces, strategic, saveload,
sgp-core, xml-loaders) each produced concrete findings; every finding was independently
triaged by a second agent for hotness, worth, and risk. 95 candidates → 57 kept. The 38
drops were cold-path micro-opts, premature optimizations, or rewrites whose behavior-change
risk outweighed the gain.
