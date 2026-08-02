# Runtime campaign extraction status

This document defines the completion boundary for the engine/framework
campaign-identity phase. It distinguishes the runtime-owned architecture from
the remaining legacy content implementation tail.

## Completed boundary

The dependency-free engine contains no `JA2UB` compile-time identity. The JA2
composition root, persistence/bootstrap, strategic event dispatch, dialogue
action decoding, tactical overhead/endgame flow, map changes and sector state,
strategic ownership/movement, queen command, quests/facts, and the laptop
lifecycle/router emit both campaign paths and select them through
`GameCapabilities`.

The architecture check names those migrated files and rejects any reintroduced
`JA2UB` conditional. The separate JA2, Unfinished Business, and Map Editor
products remain compatibility hosts with their established default campaign.

This work does not alter maps, XML, Lua, dialogue, email text, artwork,
archives, package overlays, or other game-data formats. Campaign-qualified
constants and typed profile/action/map-change resolvers preserve the existing
numeric records.

## Literal remaining tail

`tools/lint_campaign_compile_guards.py` is the canonical inventory. At this
milestone its per-file baseline contains 386 active conditionals in 81
first-party files:

| Area | Conditionals |
| --- | ---: |
| Laptop content/pages | 147 |
| Tactical gameplay/content | 139 |
| Strategic gameplay/content | 63 |
| JA2 application shell | 25 |
| Tactical AI | 8 |
| Editor | 1 |
| Multiplayer | 1 |
| Tile engine | 1 |
| Utilities | 1 |

The largest individual legacy leaves are:

| File | Conditionals |
| --- | ---: |
| `Laptop/mercs.cpp` | 48 |
| `Tactical/ShopKeeper Interface.cpp` | 25 |
| `Laptop/AimLinks.cpp` | 16 |
| `Laptop/email.cpp` | 14 |
| `Strategic/mapscreen.cpp` | 14 |
| `Laptop/mercs Account.cpp` | 12 |
| `Tactical/Arms Dealer Init.cpp` | 12 |
| `Strategic/LuaInitNPCs.cpp` | 11 |
| `Laptop/mercs Files.cpp` | 10 |

These are not dependencies of `Engine/Core`; they are legacy application,
page, campaign-content, and gameplay implementations above the runtime
boundary. Removing every one is a further content-unification project, not a
prerequisite for using the extracted engine/framework contracts.

## Non-regression rule

The baseline is a per-file ratchet:

- no existing file may gain a campaign compile guard;
- no new file may introduce one;
- a guard cannot be hidden by removing one elsewhere;
- runtime conversions may lower the baseline with
  `tools/lint_campaign_compile_guards.py --update`.

CI runs this check alongside the unsafe string-sink ratchet. The architecture
test additionally enforces zero compiled campaign identity in the engine and
every migrated orchestration seam.
