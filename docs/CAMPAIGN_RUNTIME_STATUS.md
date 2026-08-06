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
campaign-colliding aliases. The M.E.R.C. landing, account, hire, files, and
Speck-dialogue paths use `CampaignMercSitePolicy`; both campaigns' account,
billing, equipment, availability, and dialogue behavior are emitted in every
host. Eight colliding Speck dialogue roles are resolved through
`CampaignSpeckQuoteCode::Role` rather than campaign-ambiguous raw aliases. The
A.I.M. links and member pages use `CampaignAimSitePolicy`: configurable UB
links, mission-fee presentation, fixed contract pricing, one-time-offer text,
and hidden hiring controls coexist with Arulco's complete link and contract UI
in every host. Email catalog selection, insurance notices, and Bobby Ray/John
Kulba shipment notifications use `CampaignLaptopCommunicationsPolicy`. The
shared implementation disambiguates semantic mail kinds before interpreting
legacy offsets, including UB's collision between IMP profile results and the
Bobby Ray shipment record at offset 198.
File-viewer briefing/text/artwork and history quest-text selection use
`CampaignLaptopContentPolicy`; both campaigns and the optional UB asset
fallbacks coexist in the shared `files.cpp` and `history.cpp` implementations.

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
for Miguel through Slay. The M.E.R.C. policy preserves both campaigns' profile,
account, finance, email, and speech records, including the exact 19- and
20-entry idle-quote rosters and the shifted Speck records 76-83 versus 94-101.
The A.I.M. policy preserves its existing bookmarks, artwork and text records,
daily/weekly/biweekly salaries, medical deposits, equipment prices, and finance
transactions. The communications policy likewise preserves Email.edt,
Email25.edt, XML identifiers, sender bytes, substitution tags, finance/history
records, and save layouts. Bobby Ray's subsequent commerce-state extraction
also preserves its 100-purchase legacy records and shipment serialization:
`BobbyRayCommerceModel` supplies campaign-neutral capacity/count/value rules,
while validated adapters retain the existing data format and publish loaded
state only after complete reads. `BobbyRMailOrder.cpp` is consequently shared
by every application host. The subsequent file/history content extraction
makes those pages shared as well. The subsequent Laptop input-boundary pass keeps that
same common ownership: every Laptop XML reader uses the campaign-neutral
`LocalizationInputModel` for bounded chunks, strict numeric/boolean parsing,
explicit preservation of documented `-1` byte sentinels, and index validation,
then publishes staged records only after a complete document. The legacy UTF-8
adapter remains above the engine boundary, and the existing XML schemas,
numeric IDs, and fixed game structures are unchanged. In particular, the
shipping-destination schema still permits the established name-only base
records; map coordinates are accepted when all four location fields are
present and rejected only when that optional group is partial. Campaign-stat
news likewise retains the legacy fixed-field behavior for installed text that
exceeds 299 characters: converted text is boundedly truncated and terminated
instead of rejecting the complete startup document.
The subsequent IMP lifecycle pass follows the same campaign-neutral ownership:
`ImpCreationStateModel` validates page, portrait, voice, and free-profile
selection, while staged IMP import publishes a complete profile only after
validation and successful hire. It changes neither campaign selection nor the
legacy IMP `.dat`/`.dat2`, portrait XML, profile, finance, or history formats.
The file/history content pass preserves the existing RIS, quest, map, and
biography asset names and record layouts while correcting missing-UB-asset
fallbacks: `RIS25.edt` falls back to `RIS.edt`, and a quest completion retains
its paired odd record when `quests25.edt` is absent. Missing-profile killed-
merc records also retain their diagnostic in every host rather than leaving
release/shared output unset.
The subsequent A.I.M. resource-ownership pass makes Facial Index, Links, and
Members shared as well, so Laptop now builds 77 common translation units and
only 21 per-application variants. A staged owner covers the complete A.I.M.
page cluster's video objects, surfaces, button images, buttons, and mouse-region
registrations without changing content, save data, or campaign behavior.
The subsequent M.E.R.C. ownership pass makes its landing, Files, Account, and
No Account implementations shared too, bringing Laptop to 81 common
translation units and 17 variants. The same staged owner now covers the whole
site, its gear controls and help regions, and Speck's transient subtitle and
video controls; `CampaignMercSitePolicy` replaces the last host-identity branch
without changing profile, billing, dialogue, XML, or save records.
The Florist/Funeral service-site batch then makes flower delivery campaign
behavior runtime-owned and promotes `florist Order Form.cpp`, bringing Laptop
to 82 common translation units and 16 variants. Five connected pages now own
their complete page resources transactionally, including gallery swaps and
the XML-sized delivery drop-down; dependency-free bounds cover mutable
destination and gallery state without changing order, finance, postal, or
save formats.
The Insurance batch retains that 82-common/16-variant split while making all
four already-common pages one transactional resource boundary. Contract forms
and mutable roster pagination are runtime-safe, and purchase, extension, and
payout state is validated before publication without changing campaign mail,
premium calculations, saves, or data records.
The Bobby Ray catalogue batch retains the same split and makes the five
already-common Guns, Ammunition, Armour, Miscellaneous, and Used pages one
transactional ownership boundary. Shared catalogue controls and replaceable
item regions now have ordered lifetime ownership, while dependency-free page,
visible-slot, and data-index rules harden empty/stale navigation and malformed
LBE input without changing inventory, purchasing, campaign, or save behavior.
The adjoining Bobby Ray fulfilment batch retains the same split and makes the
home, mail-order, and shipment pages one transactional ownership boundary.
Replaceable destination, order-scroll, and shipment-row regions now use stable
owned storage; dependency-free empty, stale, visible-slot, and sparse-record
rules remove destination-list crashes and stale callbacks, while shipment
redraws no longer leak order-grid graphics. Pricing, inventory, PostalService,
order/shipment saves, and campaign behavior remain unchanged.
The finance/history ledger batch also retains the same shared/variant split.
Both pages now own complete resource transactions and use one campaign-neutral
record-page model plus scoped exact FileMan adapters. Headered finance and
headerless history layouts, persisted page fields, transaction/history codes,
quest behavior, Lua/XML interfaces, and campaign content remain unchanged;
malformed tails, stale pages, overflow, partial publication, borrowed handles,
and the retired corrupt history page writer are rejected above the runtime
campaign boundary.
The email ownership batch retains that shared/variant split and the existing
runtime communications policy. Four scoped resource transactions now cover the
inbox and its transient modes, while dependency-free page, index, serialized-
width, fixed-text, and unread-order rules protect the mutable list. Complete
page topology and loaded-message replacements are staged before publication;
exact FileMan adapters reject malformed or duplicate saved records without
discarding the live inbox. The established email catalogs, XML identifiers,
sender and substitution semantics, save structures, artwork, and campaign
behavior remain unchanged.

## Literal remaining tail

`tools/lint_campaign_compile_guards.py` is the canonical inventory. At this
milestone its per-file baseline contains 130 active conditionals in 45
first-party files:

| Area | Conditionals |
| --- | ---: |
| Laptop content/pages | 11 |
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
`CampaignMercenaryPolicy`. The full M.E.R.C. site cluster no longer contributes
either: account creation and settlement, hire pricing and gear, site
availability, and Speck dialogue use `CampaignMercSitePolicy` and typed quote
roles. The A.I.M. links/member cluster no longer contributes either: link
availability, salary/mission-fee presentation, contract charging, and hiring
controls use `CampaignAimSitePolicy`. Email, insurance, and shipment notices
use `CampaignLaptopCommunicationsPolicy`; file-viewer and history content uses
`CampaignLaptopContentPolicy`. All seven policies are guarded by
data-free headless tests plus named architecture checks. The seven remaining
`Ja2` conditionals are the
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
| `Strategic/mapscreen.cpp` | 12 |
| `Strategic/LuaInitNPCs.cpp` | 11 |
| `Tactical/Handle Doors.cpp` | 8 |
| `Tactical/Campaign.cpp` | 6 |
| `Strategic/Luaglobal.cpp` | 5 |
| `Tactical/Civ Quotes.cpp` | 5 |
| `Tactical/Interface.cpp` | 5 |
| `Laptop/email.h` | 4 |

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
