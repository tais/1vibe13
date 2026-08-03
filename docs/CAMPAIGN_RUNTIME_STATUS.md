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
`GameCapabilities`. Dealer inventory, lifecycle, repair rules, and shopkeeper
transactions now do the same through `CampaignDealerPolicy`. Mercenary profile
loading, hiring and arrival, recruitment, creation, contract refunds, daily
availability, and campaign-specific Slay handling use
`CampaignMercenaryPolicy`. Tactical AI, strategic events, laptop/personnel UI,
multiplayer, save migration, and tactical gameplay now resolve Miguel through
Slay by semantic role as well. The shared profile enum no longer exposes raw
campaign-colliding aliases.

The architecture check names those migrated files and rejects any reintroduced
`JA2UB` conditional. The separate JA2, Unfinished Business, and Map Editor
products remain compatibility hosts with their established default campaign.

This work does not alter maps, XML, Lua, dialogue, email text, artwork,
archives, package overlays, or other game-data formats. Campaign-qualified
constants and typed profile/action/map-change/dealer resolvers preserve the
existing numeric records. In particular, dealer save and merchant-XML storage
remain 80 raw slots: the typed dealer policy resolves the campaign-dependent
meanings of the colliding slots 5-18 without changing their serialized values.
The mercenary policy likewise resolves the established one-slot profile shift
for Miguel through Slay and preserves both campaigns' profile, email, and quote
record numbers.

## Literal remaining tail

`tools/lint_campaign_compile_guards.py` is the canonical inventory. At this
milestone its per-file baseline contains 265 active conditionals in 58
first-party files:

| Area | Conditionals |
| --- | ---: |
| Laptop content/pages | 146 |
| Tactical gameplay/content | 52 |
| Strategic gameplay/content | 48 |
| JA2 compatibility shell/layout | 7 |
| Tactical AI | 8 |
| Editor | 1 |
| Multiplayer | 1 |
| Tile engine | 1 |
| Utilities | 1 |

The common content loader, tactical game-screen loop, helicopter arrival,
underground loading-screen selection, XML campaign paths, dealer identity and
inventory routing, shopkeeper behavior, and the migrated mercenary lifecycle
no longer contribute to this tail. Those paths use
`CampaignApplicationPolicy`, `CampaignDealerPolicy`, and
`CampaignMercenaryPolicy` and are guarded by data-free headless tests plus
named architecture checks. The seven remaining `Ja2` conditionals are the
compiled host-capability seed, the two alternate new-game-screen
implementations, product/build labels, and the legacy `GAME_SETTINGS` layout;
they are deliberately separate compatibility seams.

The repository-wide semantic-profile migration is complete for the colliding
Miguel-through-Slay range. `Tactical/Soldier Profile.h` retains the stable raw
slot boundary at 65 but contains neither a campaign guard nor campaign-specific
names for slots 57-65. Gameplay callers resolve those meanings through
`CampaignProfileCode::Role` and `CampaignMercenaryPolicy`; architecture CI
rejects restoration of the retired raw aliases.

The largest individual legacy leaves are:

| File | Conditionals |
| --- | ---: |
| `Laptop/mercs.cpp` | 48 |
| `Laptop/AimLinks.cpp` | 16 |
| `Laptop/email.cpp` | 14 |
| `Laptop/mercs Account.cpp` | 12 |
| `Strategic/mapscreen.cpp` | 12 |
| `Strategic/LuaInitNPCs.cpp` | 11 |
| `Laptop/mercs Files.cpp` | 10 |
| `Laptop/AimMembers.cpp` | 9 |
| `Laptop/files.cpp` | 9 |
| `Tactical/Handle Doors.cpp` | 8 |
| `Laptop/insurance Contract.cpp` | 7 |

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
