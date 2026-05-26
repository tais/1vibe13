# Adding a third in-map level (trench / higher roof) — feasibility

Investigation of what it would take to give a single tactical map **more than two
gameplay levels**. Today the engine has exactly two: **ground** (`bLevel == 0`) and
**roof** (`bLevel == 1`). The motivating ideas are a **trench level** (below ground)
and/or a **higher roof level** (a second roof tier above the current one).

This is a readout, not a plan of record. It exists so the cost can be weighed
before committing.

---

## TL;DR

- The two-level model is baked in as a **binary**, not a level *count*. There is no
  single switch to flip — the "2" is implicit across ~130+ sites.
- A **higher roof** is the more tractable of the two: it extends the direction the
  engine already understands (up), reusing the existing roof machinery as a template.
- A **trench** (below ground) is harder and partly novel: nothing represents
  sub-ground occupancy today (no layer, no negative height, no LOS/cover semantics).
- Either way the cross-cutting surface is large (layers, `bLevel` logic, LOS,
  lighting, pathfinding, AI, both renderers, the `.map` format, the editor, and a
  savegame bump). Realistically weeks of work, with the regression risk concentrated
  in LOS / pathfinding / rendering.
- Recommended: **don't** start a full rewrite cold. First parameterize the level
  count, then build a **render-only second-roof spike** to measure the real cost
  multiplier, then layer subsystems in behind a feature flag.

---

## How "level" works today

### Render layers are NOT gameplay levels

Each tile (`MAP_ELEMENT`, `TileEngine/worlddef.h`) holds **9 render layers**, not
levels. The layer index macros (`worlddef.h:229`–`237`):

| Index | Macro | Purpose |
|---|---|---|
| 0 | `pLandHead`   | ground tiles |
| 1 | `pLandStart`  | ground tile list start |
| 2 | `pObjectHead` | objects |
| 3 | `pStructHead` | structures |
| 4 | `pShadowHead` | shadows |
| 5 | `pMercHead`   | **occupants on the ground level** |
| 6 | `pRoofHead`   | roof tiles |
| 7 | `pOnRoofHead` | **occupants on the roof level** |
| 8 | `pTopmostHead`| topmost overlays |

```c
LEVELNODE *pLevelNodes[ 9 ];   // worlddef.h:263
```

So the gameplay level (ground vs roof) is expressed by **which layer** things live
in: ground occupants in `pMercHead`, roof occupants in `pOnRoofHead`, roof tiles in
`pRoofHead`. There is exactly **one** occupant layer per level and **one** roof tier.
A new level needs new layer(s) here and in every loop over `pLevelNodes`.

### The gameplay level: `bLevel`, treated as a boolean

`bLevel` is stored as `INT8` (so the *type* could hold more), e.g.:

- `Tactical/Soldier Control.h:1066`, `:2506`
- `Tactical/Rotting Corpses.h:120`
- `TileEngine/Explosion Control.h:25`
- items use `UINT8 ubLevel` (`Tactical/World Items.h:39,64,86,108`)

…but the **logic** treats it as binary in ~**132** sites (`bLevel == 0`,
`bLevel == 1`, `bLevel ? … : …`). There is **no** `bLevel > 1` cap or assert — the
binary assumption is diffuse, not gated in one place.

The structure system goes further and uses a literal boolean for the roof:

```c
INT8 GetTallestStructureHeight( INT32 sGridNo, BOOLEAN fOnRoof );   // structure.h:71
INT8 GetStructureTargetHeight ( INT32 sGridNo, BOOLEAN fOnRoof );   // structure.h:72
```

### Per-level data is dimensioned `[2]`

Movement costs are per-level with a hardcoded trailing `[2]`:

```c
extern UINT8 (*gubWorldMovementCosts)[MAXDIR][2];   // worlddef.h:284
```

### Terrain height is unsigned — no "below ground"

```c
UINT8 sHeight;   // worlddef.h:272  (terrain elevation 0..255, for hills)
```

`sHeight` raises tiles for hills; it cannot go negative, so a trench cannot be
modeled as "negative height." It needs a real sub-ground level.

### Inter-level movement is special-cased ground↔roof

Climbing is its own subsystem, hardwired for the single ground→roof transition:

- `FindRoofClimbingPoints( SOLDIERTYPE*, INT16 )` — `TacticalAI/ai.h:219`
- `EstimatePathCostToLocation( …, &fClimbingNecessary, &sClimbGridNo )` —
  `TacticalAI/AIUtils.cpp:1250`, `Tactical/Turn Based Input.cpp:5737`

### Underground is already a *separate sector*, not an in-map level

Basements/caves (`gfBasement` / `gfCaves`) are **distinct sector maps** the player
transitions into — that is how JA2 provides sub-ground space today, without a third
`bLevel`. This does **not** help the trench / second-roof case, because those require
seeing and shooting **between** levels within the same firefight.

---

## The two proposed features

### Higher roof (`bLevel = 2`) — more tractable

Extends the direction the engine already handles. Reuses roof machinery as a
template:

- add a 3rd occupant layer + a 2nd roof-tile layer to `MAP_ELEMENT`;
- generalize the roof-climb transition to roof→roof2;
- widen the per-level logic from `{0,1}` to `{0,1,2}`.

Hard but largely **mechanical** — it follows existing patterns.

### Trench (below ground) — harder, partly novel

Nothing in the engine represents sub-ground occupancy:

- no occupant/tile layer below ground;
- no negative terrain height (`sHeight` is `UINT8`);
- the LOS / cover semantics are new (a soldier in a trench is low and partially
  hidden; an enemy can walk over the top). A *true* trench is more invention than
  extension. (A shallow approximation via stance + cover modifiers — not a real
  level — would be far cheaper but wouldn't support over-the-top occupancy.)

---

## Cross-cutting impact surface (applies to either feature)

| Subsystem | What changes | Risk |
|---|---|---|
| `MAP_ELEMENT` layers | new occupant/tile layer(s); every `pLevelNodes` loop | medium |
| `bLevel` logic | ~132 `== 0/== 1/?` sites; `BOOLEAN fOnRoof` → level index | medium (volume) |
| Per-level arrays | `gubWorldMovementCosts[..][..][2]` → `[N]` | low |
| Line of sight | per-level visibility / blocking between tiers | **high** |
| Lighting | per-level light maps | medium |
| Pathfinding + transitions | climb generalized to N levels; costs | **high** |
| AI | level-aware target/cover/movement selection | high |
| Tactical renderer | draw + z-order extra tier(s) | medium |
| Overhead renderer | the overhead map assumes 2 tiers | medium |
| `.map` format | store extra-level tiles | medium (format bump) |
| Map editor | place the new-level tiles | medium |
| Savegame | persist extra-level state | low (we own the format) |

Effort: realistically **weeks**, dominated by LOS / pathfinding / rendering, where
the regression risk also lives.

---

## Recommended approach

1. **Parameterize the level count first.** Replace the implicit `{0,1}` /
   `BOOLEAN fOnRoof` model with a small signed range and a `NUM_LEVELS` / layer-set
   parameter (e.g. `-1` trench … `0` ground … `1,2` roofs), so "2" stops being
   implicit. Mechanical but sweeping.
2. **Prototype the smallest end-to-end slice** before committing: a **render-only
   second roof tier** — extend `MAP_ELEMENT` + the tactical/overhead renderers to
   *draw* a second roof, with no occupancy or LOS yet. This proves out the layer and
   `.map`-format changes cheaply and yields the real cost multiplier for the rest.
3. **Then layer subsystems in, one at a time, behind a feature flag:** occupancy →
   pathfinding/transitions → LOS/lighting → AI → editor → saves.

Start with the **higher roof** before the trench: it reuses existing roof code, so it
both delivers value sooner and shakes out the level-count generalization that the
trench would also need.

---

## Key references

- Tile layers / level macros: `TileEngine/worlddef.h:225`–`264`
- `MAP_ELEMENT` (incl. `sHeight` `UINT8`): `TileEngine/worlddef.h:239`–`279`
- Per-level movement costs: `TileEngine/worlddef.h:284`
- Structure roof boolean: `TileEngine/structure.h:71`–`72`
- Roof-climb transition: `TacticalAI/ai.h:219`, `TacticalAI/AIUtils.cpp:1250`
- `bLevel` fields: `Tactical/Soldier Control.h:1066,2506`, `Tactical/Rotting Corpses.h:120`
