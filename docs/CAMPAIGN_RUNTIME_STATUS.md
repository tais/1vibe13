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
`GameCapabilities`. Quest/fact campaign decisions now do so through
`CampaignQuestPolicy`; the shared implementation keeps every mutation, email,
and reward effect in `Strategic/Quests.cpp`. Dealer inventory, lifecycle,
repair rules, and shopkeeper
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
Tactical door forcing, failed-lock feedback, and the UB tunnel-gate side effect
now use `CampaignDoorPolicy`. Arulco retains the boot-door AP cost and
ordinary force/curse paths, while UB retains its open-door AP cost and tunnel
quote semantics in the same compiled implementation. The tactical door menu
now uses the same policy for cancel, open, explosive, and crowbar actions:
Arulco retains its calculated open/bomb costs, while UB retains its fixed
costs and short-circuited tunnel quote interception.
The strategic map screen now uses `CampaignMapScreenPolicy`: Arulco retains
its meanwhile-scene polling, while UB retains Jerry Milo's opening, full-load,
no-mercenary, and time-compression guidance, custom-map regeneration,
strategic-AI update, and helicopter-crash loss path in the same compiled
implementation. The adjoining map shell now follows the same runtime policy
for San Mona loss handling, UB loss dialogue, border-button configuration,
pre-battle auto-resolve, and helicopter-landing meanwhile scenes. Initial
Enrico congratulations mail is selected through
`CampaignLaptopCommunicationsPolicy`, and the campaign-sector setup declaration
is available to every host. Merc dismissal in `Assignments.cpp` likewise uses
`CampaignMercenaryPolicy`: only UB blocks dismissal from tunnel column 14
onward and retains the qualified/unqualified refusal quote choice.
The map-bottom availability and exit checks now use the same map policy at
their four former raw campaign leaves. Arulco alone polls pending meanwhile
state and continues to reject the UB intro destination. UB alone checks its
initial-sector entry before asking Jerry for time-compression permission, and
it retains the intro exit. Generic early returns, the unconditional main-menu
exit, and the sector-before-Jerry probe order remain unchanged.
The strategic-event dispatcher now uses `CampaignStrategicEventPolicy` for
all fourteen campaign-owned callbacks. Arulco retains its nine M.E.R.C.,
meanwhile, PMC, Kingpin, militia-roster, and intel routes; UB retains its five
initial-sector, delayed-dialogue, H8-warning, and Enrico-understanding routes.
The initial-sector option remains behind the UB route, the battle-delay probe
remains behind the Arulco meanwhile route, and all existing effects remain in
`ExecuteStrategicEvent` in their original order.
The remaining strategic gameplay leaves now use
`CampaignStrategicContentPolicy`, a closed six-effect value boundary. Arulco
retains first-battle town loyalty, the creature-release meanwhile and its debug
reset, the complete Enrico progress-email cycle, continued-militia-training
dialogue, and Speck's employee-death reaction. UB retains the absence of those
effects while common first-battle facts, creature state, progress probes,
militia continuation publication, daily sector reset, and buddy comments keep
their established order. Campaign-qualified Enrico records expose the exact
Arulco Email.edt offsets 152, 155, 158, 161, 164, 167, 189, 192, and 195 to
every host, with compile-time aliases back to the legacy Arulco layout. All
seven former `JA2UB` guards across
`Auto Resolve.cpp`, `Creature Spreading.cpp`, `Strategic Status.cpp`,
`Town Militia.cpp`, and `strategic.cpp` are gone; both campaigns now compile
the same calls and choose ownership from the live context. The formerly
guarded `mercs.h` declaration is unconditional, and common `mercs.cpp` remains
the all-host link source for Speck's reaction.
Civilian tactical dialogue now uses `CampaignCivilianQuotePolicy`. Both quote
catalogues and both dedicated-group ranges are emitted in every host; Arulco
retains surrender completion and its complete town/hint/miner logic, while UB
retains records 40-49, its restricted enemy-action test, and record 255 as an
unavailable sentinel. The two `CIV_QUOTE_NEW` markers remain campaign-qualified
at their exact records 1029 and 1039. The common storage uses the larger
capacity only; no quote record, EDT file, or serialized layout changes.
The IMP home page and text system now use `CampaignImpPolicy`. Arulco retains
its XEP624 pass and new-laptop gate. UB retains its independently configurable
XEP624 and GP97SL passes, including the established expression precedence that
applies the new-laptop gate only to GP97SL. Invalid and known-but-unavailable
inputs remain distinct. Every host also emits the exact UB
`IMPText25.edt`-then-`IMPText.edt` fallback while Arulco keeps `IMPText.EDT`;
the UB-only file probe is behind a left-hand runtime campaign gate. The two
implementations are now common Laptop translation units, bringing the current
partition to 84 common and 14 per-application variants without changing IMP
profiles, saves, text records, or assets.

The only two production calls to `LoadGameUBOptions` now use
`CampaignApplicationPolicy::shouldLoadUnfinishedBusinessOptions()`. The main-
menu reinitialization and common game-start path both retain the exact order:
rebel-command settings first, UB options only for UB, then tactical visibility-
range initialization. Arulco therefore still skips the UB option effect, and
neither entry point moves or duplicates a configuration read.

Tactical meanwhile-scene follow-through now uses
`CampaignApplicationPolicy::hasMeanwhileScenes()`. Arulco retains its exact
dynamic-dialogue suppression, martial-artist idle suppression, Elliot/queen
response, death-sound suppression, forced melee hit, and radar-map suppression
while a meanwhile is active. UB retains the opposite no-meanwhile behavior,
and every legacy `AreInMeanwhile()` probe introduced by this conversion is
behind a left-hand capability gate, so these converted UB paths never evaluate
Arulco-only scene state. All eight former `JA2UB` guards across
`DynamicDialogue.cpp`, `Soldier Ani.cpp`, `Weapons.cpp`, and
`Radar Screen.cpp` are gone without changing RNG, animation order, dialogue
records, tactical saves, or artwork.

Campaign progress selection and its scientist-AWOL threshold event now use
`CampaignProgressPolicy`. Arulco retains editor-mode suppression, the complete
mine/kills/control/visited composite calculation, modifiers, and its Madlab
meanwhile trigger. UB retains its legacy signed `INT8` strategic-sector key
lookup and exact fallback behavior; in particular, the later surface-sector
constants remain outside that signed key domain and therefore still resolve to
the established 50 percent default. The UB-only strategic-AI probe is behind a
left-hand runtime campaign gate, so Arulco never reads that state. The first
three former `JA2UB` guards in `Tactical/Campaign.cpp` were removed by this
progress seam; UB likewise short-circuits the Arulco-only Madlab threshold
read.

Quest/fact campaign decisions now use `CampaignQuestPolicy`. Arulco retains
all twelve guarded fact evaluations, the 25-point kill-Deidranna reward, the
deliver-letter initial quest, and POW quest handling. UB retains the exact
leave-existing-`gubFact`-value-unchanged behavior for those unavailable fact
cases, the 4-point reward, destroy-missiles initial quest, no POW processing,
and the fix-laptop completion sequence. That sequence still performs the same
save-state writes, emails, Manuel/Miguel conditions, away-email recovery, IMP
reminder, and forced email delivery in the same order. Its campaign gate stays
left of the quest comparison and the typed laptop-option accessor, so Arulco
does not read UB configuration and a different UB quest short-circuits the
option read. The application resolves the policy lazily at each former
campaign check, so unrelated fact evaluation cannot initialize `GameContext`
earlier than before. All sixteen direct campaign-identity checks in
`Strategic/Quests.cpp` and its one raw `LaptopQuestEnabled` behavioral read are
gone; quest state, profile lookup, history, rewards, email effects, and POW
mutations remain in that source.

The remaining tactical email-content seam now uses
`CampaignLaptopCommunicationsPolicy`. Arulco retains its unconditional A.I.M.
death notice for eligible profiles and its delayed M.E.R.C. level-up email,
including the XML identifiers, wrapped legacy byte offsets, and the special
165-168 template selectors. UB retains its laptop-quest and dead-merc option
gates, the Arulco `Email.edt` record 206 substitution, and the absence of the
M.E.R.C. level-up email. The runtime UB gate precedes both option reads, so
Arulco never touches `gGameUBOptions` here; UB returns before the level-up
handler reads XML mode, profile data, or Speck availability. All six former
guards in `Tactical/Campaign.cpp` are now gone without changing the death
counter, delayed event, email IDs, send time, or campaign data.

JA25 new-gun dialogue now uses `CampaignGunCommentPolicy` at both tactical
entry points. UB retains the ground-pickup path's raw nonzero world-item index
and `TRUE` source flag, including `ITEM_PICKUP_SELECTION` (31000) and
`ITEM_PICKUP_ACTION_ALL` (32000), and the
inventory-placement path's pre-placement `pObj->usItem` read with a `FALSE`
source flag. Arulco short-circuits each comment route before its argument is
evaluated or its dialogue effect is called.
All five former `JA2UB` guards in `Tactical/Handle Items.cpp` and
`Tactical/Items.cpp`, including the guarded includes and unused local, are
gone without changing item IDs, quote records, call order, or save data.

NPC script and tactical-AI campaign choices now use `CampaignNpcPolicy`.
Arulco retains the PETER/ALBERTO/CARLO-to-Herve fallback, meanwhile quote-file
overrides, the pre-version-92 Auntie script repair, and both enemy-surrender
offer paths. UB retains its independent general quote-routing pass, direct
files for the exact twelve UB NPC profiles, and Morris's pending hurt-player
turn quote. Campaign checks stay on the left of UB profile/save globals,
`AreInMeanwhile()`, and Arulco quest/team probes. All eight former `JA2UB`
guards across `TacticalAI/NPC.cpp`, `TacticalAI/AIMain.cpp`, and
`TacticalAI/DecideAction.cpp` are gone without changing NPC files, records,
quests, quotes, saves, or AI condition order.

Lua global initialization now uses `CampaignLuaGlobalPolicy`. The always-
published `iniNEW_MERC_ARRIVAL_LOCATION` reads Arulco's initial-arrival option
or UB's `LOCATEGRIDNO` only after the live campaign is selected. UB still
executes the exact 92 campaign-gated push/set pairs in their established order
and with their established integer/boolean push types: two early difficulty
assignments, 41 scenario settings, `TestUB`, and 48 character/item aliases.
Those pairs expose 91 UB-only names; the remaining `difficultyLevel` write is
the legacy early duplicate of the common write later in the initializer.
Arulco retains that one common write and omits the other 91 names. This includes
the legacy spelling
`UB_SECTOR_DOOR_IN_TUNNEL_Z`. Each campaign gate precedes the corresponding
campaign-specific read, and every push remains immediately paired with the
same `lua_setglobal`, so initializer stack balance, all 34 executable call
sites, and Lua API/error behavior are unchanged. Five additional calls remain
inside disabled legacy block comments and are not runtime call sites. All five
former `JA2UB` guards in
`Strategic/Luaglobal.cpp` are gone.

Power-generator, tunnel, fortified-door, and mine scripting now uses
`CampaignTacticalScenarioContent` and `CampaignTacticalScenarioPolicy`.
`ub_config.cpp` is the narrow read-through adapter for typed sectors, the
nine-grid fan set, paired mine targets, and explicit switch decisions.
Sector coordinates retain the legacy unsigned 32-bit value domain; signed
world coordinates are projected with the same modulo conversion used by the
former direct comparisons, including negative sentinels.
`TileEngine/Explosion Control.cpp` therefore contains neither a raw campaign
selector nor a direct `gGameUBOptions` read, and the mine block in
`Tactical/Interface Dialogue.cpp` has the same boundary. The campaign gate
precedes every content read, so Arulco neither reads UB scenario data nor
touches its effects. UB retains the two tunnel Y alternatives at hard-coded
depth one, every dialogue/save flag and map mutation in its established order,
and the exact surface/underground mine targets. The save-restored
`HandleAddingEnemiesToTunnelMaps` option is deliberately not part of the
content snapshot: it is read live at the fan-destruction decision point.

The contiguous strategic sector-script island from player-entry quotes through
roof placement now consumes `CampaignStrategicSectorScriptContent` through the
value-only `CampaignStrategicSectorScriptPolicy`. Five consuming entry points
take at most one fresh snapshot after their left-hand UB gate; the gate-only
first-tunnel quote entry point does not touch content, and the email path keeps
its earlier live laptop short circuit. The adapter reuses the tactical
fan, missile, and fortified-door sectors, then projects the remaining 41
immutable option fields exactly once. The laptop-quest switch remains a
separate live `IsLaptopQuestEnabled()` read after the quest and just-fixed
short circuits and before the email snapshot. The exact six-entry quote table,
Manuel/Biggens and email ordering, literal 12,9,0 fallback, repeated clock
reads, map load/unload action order, first-town X-as-Z comparison, money drop
and difficulty order, fortified-door grid, and roof 2a-to-2a/3a-to-3a aliases
are unchanged. The bounded block now has zero direct `gGameUBOptions` reads and
the file retains only four unrelated option reads outside that island.

JA25 strategic-AI scenario origin now crosses the application boundary through
the dependency-free `CampaignStrategicAiScenarioPolicy`. This origin is not
campaign identity: both recognized values belong to the runtime Unfinished
Business campaign and choose built-in JA25 sector-AI state or custom strategic
state. Fifteen strategic-AI entry points take one fresh origin value per
invocation, after their existing runtime campaign callers have selected UB.
The H8 warning and complex-history paths read only the state source selected by
that value; all built-in-only mutations, return sentinels, untouched output
pointers, and saved seek flags retain their established behavior.

The adapter preserves the complete legacy byte rather than converting it to a
boolean. Zero selects custom state, one selects built-in state, and every other
value continues to match neither branch. Configuration keeps its established
default of one, while the unchanged one-byte general-save field serializes and
restores the exact byte before JA25 sector state is loaded. The origin remains
a live read because both main-menu option reload and save restoration can
replace it. `Strategic/Ja25 Strategic Ai.cpp` and `SaveLoadGame.cpp` now have
no direct access to the raw member; its only two accesses are the getter and
setter in `ub_config.cpp`.

Mercenary hiring and initial-sector setup now consume a fresh
`CampaignMercenaryArrivalContent` value after the live
`CampaignMercenaryPolicy` selects UB. The value contains all seven initial
helicopter grids, all seven signed random times, and the InJerry, InGameHeli,
InGameHeliCrash, JerryGridNo, LaptopQuestEnabled, and LOCATEGRIDNO projections.
Only `ub_config.cpp` reads those legacy fields. `Tactical/Merc Hiring.cpp` now
has neither a direct campaign selector nor a direct option read; Arulco exits
or chooses its ordinary route before the adapter is evaluated. That removes
the two former eager InGameHeli argument evaluations while keeping crash-flag,
arrival, insertion, array-copy, initial-sector ownership, Jerry lookup, quest,
random animation, visibility, and interface-lock order unchanged. The
on-screen and off-screen callback routes are mutually exclusive and each
samples one fresh value at its established decision point. Repeated
InJerry/JerryGridNo, helicopter, and laptop decisions then deliberately use
that frozen projection across actor, quest, and random effects; the next
invocation refreshes it. Future setters remain separate `ub_config`-owned APIs
and do not expose the legacy record. A dependency-free truth/trace suite
covers both campaigns, the full array and scalar shape,
fresh invocation reads, all Jerry/crash combinations, and every early return;
bounded source ratchets pin the adapter mapping and production effect order.

The implementation side of the Lua UB callback surface is now available in
every host. `Strategic/LuaInitNPCs.cpp` unconditionally includes the JA25
strategic, tactical, update, end-game, and typed arrival-mutation contracts;
its exact 40 formerly guarded callback definitions and declarations therefore
compile for JA2, UB, and both editor compositions. Its three registration
blocks now use one fresh `CampaignLuaGlobalPolicy` value read immediately after
`InitMercFace` and one cached Boolean reused at all three original positions.
Arulco and unknown campaign values expose none of the selected names; UB
exposes all 40 + 2 + 28 mappings in their exact established order, independent
of editor state. The 512 common registrations, 34 executable callers, and
`bQuests` behavior remain unchanged. A dependency-free fake-registration trace
pins 70 unique names, 42 callbacks, 28 compatibility alias pairs, the late
single read, and per-invocation freeze/refresh, while source checks hash the
complete 582-entry sequence and reject preprocessor or helper bypasses.
The fourteen arrival-setting callbacks retain their original Lua argument
guards, `lua_tointeger`/`lua_toboolean` conversions, and zero return counts,
but their sixteen direct legacy writes now cross eight narrow semantic APIs.
Only `ub_config.cpp` mutates the record: a strong seven-slot type owns the
helicopter grids, and Jerry's signed command still treats positive values as
the requested grid, negative values as fallback 15943, and zero as no grid
mutation while always updating InJerry. The APIs return no record or reference
and accept no generic field selector.

The generic default-arrival callback also replaces its two compiled branches
with a fresh `CampaignLuaGlobalPolicy` value at each original branch point.
Valid Arulco input still leaves `JA2_5_START_SECTOR_X/Y` untouched while valid
UB input mirrors it; the legacy OR range condition keeps the fallback
unreachable for all byte inputs, but the preserved fallback truth table remains
Arulco 9,1 versus UB 7,8. The value adapter reuses Lua-global initialization's
single live context selector, so the reviewed 104-site/29-file selector
inventory is unchanged; the callback reads that value only after its Lua
argument gate and at the selected legacy branch point.

The architecture check names those migrated files and rejects any reintroduced
`JA2UB` conditional. The separate JA2, Unfinished Business, and Map Editor
products remain compatibility hosts with their established default campaign.

This work does not alter maps, XML, Lua scripts or their public names,
dialogue, email text, artwork,
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

Full-engine multiplayer host startup no longer reads campaign identity merely
to choose an intro route. Both supported dedicated PvP variants now enter the
connection screen so listener startup, self-connect, and canonical settings
precede game initialization. The explicit co-op launch form now acquires a
campaign lease, precreates fixed scratch entries and installs the isolated
writable profile before VFS discovery. SGP mounts packages, runs a separate
rollback-safe `co-op installed content manifest` subsystem, and only then enters
legacy content and game initialization. That boundary validates and counts all
encountered occurrences, omits explicit exclusive VFS runtime namespaces,
selects the smallest-layer normalized read-only overlay (including case-only
spellings across layers), rejects same-layer ambiguity/duplicates and remaining
writable shadows, and caches the digest. A capture failure rolls back the
active package subsystem; late campaign open consumes the cache, binds the
runtime fingerprint, and opens or resumes the durable A/B campaign
without VFS recomputation. Resume checkpoint bytes are materialized into the precreated
entry after identity/open and before the legacy save loader reads them. Both
co-op modes bypass the ordinary `InitMainMenu` transition during INIT, allowing
the dedicated stage-four campaign-entry request and passive worldless screen to
win before a pending main-menu transition can commit. Only dedicated creation
calls `InitGameOptions()` immediately before `InitNewGame(FALSE)`; the installed strategic/Lua difficulty domain is 1..4, whereas a skipped settings screen otherwise leaves zero. Resume keeps checkpointed options. A configurable trusted-LAN listener selected with
`--dedicated-coop-bind` and `--dedicated-coop-port` defaults to
`0.0.0.0:60005`. It uses OS-CSPRNG session epochs, identities, and reconnect
bearer tokens; it admits and transport-binds permissionless first joins but does
not authenticate human users. Periodic and final strategic checkpoints stop the
listener and require the tactical command host and server obligations to drain
before saving. The
  production co-op server now composes baseline-gated actor assignment, the
  tactical observer, and global co-op protocol-v7 execution of nine JA2 command
intents without entering legacy `GAME_TYPE=2`. The sixth is an exact-target
aimed single-shot firearm request; authority revalidates the live target,
visibility, weapon/ammunition, aim, action points, and turn state before a
local-only fire event and committed delta/receipt. The seventh is a zero-payload
selected-actor reload mapped through prepared `ReloadWeaponCommand` state to
native `AutoReload`, including manual chambering.
The eighth is a public visible-door open/close request carrying only exact base
grid, current ephemeral structure ID, and desired state. The authority accepts
only synchronous cardinally adjacent ordinary ground doors, revalidates
visibility, actor/object fingerprints, AP/BP, and idle on-foot state, and rejects
stealth, lock/trap, busy/tin-can, animation, pending, and legacy-network cases.
The ninth is an exact-serial interrupt pass for one exact eligible actor
incarnation. Resolving interrupts block input; active player interrupts allow
ordinary actions only for eligible actors and release after the final eligible
pass vote. Native interrupt lists and hidden interrupters remain private, and
AI interrupts remain under native AI control.
The six co-op translations that reuse legacy shapes (end turn, move, face,
stance, stop, and reload) are tagged
`TacticalCommandAuthorityPolicy::DedicatedCoop`. Only `NetworkPeer` and
`Replay` may carry that policy, and simulation-command journal wire v4 retains
it through replay source substitution. The executor repeats live-world and
controllable on-foot actor checks and, in combat, player-turn/no-pending-action
checks. Ordinary end turn is rejected during an active interrupt and otherwise
rechecks the exact next team, while reload
repeats its weapon/ammunition/chamber/AP resolver. Default `Legacy` commands
keep established replica and system behavior; aimed fire remains on its strict
synchronization-source resolver.
The server independently permits only one pending command per peer. Exact-next
pipelining is a non-consuming `InvalidCommandSequence` rejection before
replication, reservation, or gameplay and may retry after the earlier terminal
result. Global `AuthoritySequenceExhausted` reason 20 instead consumes the peer
cursor: the server stays active to flush its terminal receipt, then the client
records the exact receipt history/cursor before failing and closing.

Global co-op protocol-v7 retains voluntary self-retirement with exact 24-byte
request and 48-byte result shapes. The request carries version, epoch, and
request ID only; the authenticated transport resolves its own identity, so
there is no client-provided victim. Begin atomically reserves bounded same-epoch
tombstone capacity and marks that identity Pending, closing gameplay while
leaving just enough authenticated transport state to finish. A capacity refusal
happens before the close. An accepted request globally freezes admission,
tactical, and campaign input and discards their queued/deferred frames. The
committed-frame runtime waits only for already-authorized local correlations,
command inbox, receipts, deferred cancellations, and tracked commands—not
unrelated runtime messages or campaign/delta ACKs. It commits the
credential tombstone and releases the active seat before the truthful result is eligible
to send.

The runtime then stops and reconciles every network layer before stable-
compacting the retiree from replication, command, ACL, authority-sequence,
campaign Ready, and world-participant state. Survivor cursors, receipt history,
and replication records remain exact. Four-seat coverage retires one identity
and admits a distinct fifth only behind a fresh baseline and cursor one;
ordinary disconnect still preserves its seat and actors.
For the exact untouched initial strategic state, the runtime deterministically
selects the four cheapest eligible healthy A.I.M. profiles by computed charge
then profile ID, hires seven-day contracts with profile items and normal
finance/history records, installs one canonical arrival minute in both actor
and event, and checkpoints the complete in-transit state before admission/
campaign transfer. It launches the ordinary
first-arrival path into the configured hostile sector after either four peers
are campaign-ready or at least one has remained ready through a ten-second
gather grace, and requires all four actors to become controllable within two
minutes. Resume accepts the exact prepared-initial checkpoint without
rehiring, and an exact empty initial resume follows the same one-time bootstrap.
A cold non-initial strategic checkpoint with at least one valid live player
on-foot squad mercenary is preserved at entry and starts admission/campaign sync
without initial-roster mutation. Once a peer is campaign-ready, the server
enters the canonical hostile sector occupied by such an actor. Peaceful-only
established campaigns remain connected and worldless in `StrategicIdle`, while
vehicle bodies, drivers, passengers, vehicle-duty actors, and other non-squad
duties are excluded from direct co-op control. Ambiguous/partial initial shapes
fail closed rather than being repaired.

Every automatically entered hostile world arms one process-local post-combat
return. Its first committed trigger requires exact victory with drained combat actions,
interrupts, projectiles/explosions, dialogue/trigger work, autoresolve,
meanwhile, traversal, auto-bandage, boxing, save/load, UI/custom timers,
then immediately stops admission, clears both Ready sets, and discards inbound
work so ACK and intent traffic cannot starve return. A pure recheck restores
`Playable` and same-epoch admission when evidence regresses, waits while stable
evidence lacks the fresh assignment/receipt/replication boundary, and only then
cold-unloads through the normal sector-temp/
`HandleDefiniteUnloadingOfWorld`/`TrashWorld` tail while bypassing only the live-
player occupancy guard. Native teardown retires tactical-only
temporary schedules, and the required cold strategic checkpoint proves none survive.
Admission stays closed through tactical host/server drain and the deferred
`MAP_SCREEN` transition. That checkpoint supersedes campaign transfer before
admission reopens in the same epoch. Dedicated co-op victory also suppresses
interactive auto-bandage prompts.

Normal `JA2` now also composes the passive client. A pre-random/VFS bootstrap
obtains the authoritative campaign descriptor, installs private scratch/profile
storage and the exact simulation seed. After exact outer campaign-identity
validation, restart atomically quarantines a nonempty disposable client VFS
profile as a strict private `profile.orphan.<pid>.<seq>` sibling, requires a
freshly empty replacement, and recreates its two scratch files. `Temp`,
`ShadeTables`, settings, and prior load bytes are disposable, while reconnect
and retired evidence remain in the held parent; quarantine siblings require the
identity record. The post-package, pre-legacy manifest
subsystem captures and descriptor-verifies installed-content identity; late
open reuses that cached digest and rejects runtime-fingerprint mismatch without
re-enumerating the VFS. The passive client bypasses `INTRO_SCREEN` and enters
INIT state zero directly, preventing its INIT-only frame policy from looping
before `InitializeJA2` opens live transport. Only after those checks does it load the private 224-byte
canonical bootstrap + `AdmissionAck` + SHA-256 reconnect record outside the
profile: same-epoch state restores before connect, epoch-only stale state is
erased, and corrupt/unsafe/differently bound state fails closed. Accepted
credentials publish through private atomic staging before admission ACK, and a
live epoch mismatch closes before any admission request. A durable same-epoch
bearer retries without an attempt cap while the authority deliberately holds
admission closed; the unsigned counter saturates. Credential-less startup keeps
the eight-attempt bound, and an active credential is never restored across an
epoch. The worldless screen's `L` leave operation requires key-down, physical
release, and a later second key-down while both screen paths drain the complete
input FIFO. The core retains the request ID before send and replays that exact
request after a same-process reconnect ACK instead of restoring gameplay.

Committed success atomically renames the private 224-byte bearer to the exact
`client-reconnect-credential.retired` marker before credentials clear, the
client enters clean `Retired`, or transport closes. The idempotent marker is
validated on startup and stops before network construction even across epoch
changes; corrupt, unsafe, or simultaneous active/retired evidence fails closed.
The documented combined-failure window is narrower: if the server commits,
marker storage fails before rename so the active `.bin` remains, and the server
independently rolls epoch before convergence, a compatibility-verified restart
may classify the active record `StaleSession`, erase it, and fresh-admit.
Same-epoch remains fail-closed; cross-epoch terminality starts only once `.retired`
publishes. The real
SDL3_net adapter then transfers the selected
checkpoint; the client hash-verifies, atomically commits, and cold-loads it
before campaign Ready can expose a committed tactical snapshot. The dedicated
full-load scope owns `ScopedSavedGameFaceReconstruction`, so saved faces use
profile-derived presentation timing without consuming canonical RNG while
ordinary face creation retains all three legacy draws. Strategic AI loads with
`StrategicAILoadPolicy::DedicatedExactRestore`: current SAI save v29 skips
compatibility and repair gameplay, while a stale SAI version is rejected.
Generic SdlNet
retains a 256 KiB/s sustained inbound rate and a 1 MiB burst;
`SdlNetInboundMessageBudget` is configurable only before `Start()` and capped at
32 MiB/s and 4 MiB. Only the full-engine client with a non-null campaign sink
selects that maximum profile; bootstrap, core-only, and legacy peers keep strict
defaults. A static bound covers one campaign window at 144 FPS. The production
socket E2E transfers an exact 11,796,517-byte checkpoint twice,
193 chunks per transfer, at 7 ms pacing. Snapshot wire
v7 carries exact authority dimensions, canonical hostility, visible public
door state, the public `commandsBlocked` bool, compact interrupt phase/serial,
per-actor interrupt-action eligibility, and five bounded 12-byte combat-
equipment records: primary hand, secondary hand, helmet, vest, and legs. Each
record captures only the first object's item ID, stack count, and condition. For
an ammunition-bearing hand object it also captures loaded-ammunition item/count,
signed ammunition condition (including a negative jam state), and chambered
state; those fields stay canonical zero for ordinary equipment. Older layouts
are rejected rather
than inferring absent fields. Raw pending-action state and native interrupt
details stay private.
Authority capture always retains player-team actors, while every
non-player actor requires exact player-team public `SEEN_CURRENTLY` knowledge;
loss and reacquisition emit actor-left/actor-entered deltas. Its separate game-
loop branch advances no local clock, AI, packages, messages, command queue,
observer, campaign, or tactical simulation. Instead, `INIT_SCREEN` presents a
worldless sector/turn view with an exact-dimension logical diamond, friendly
actor markers, and an actor table fallback. Outside modals, arrow movement is an
allocation-free exact-grid request: Up is row -1/column -1, Down +1/+1, Left
+1/-1, and Right -1/+1. Current and target grids must fit the authority's exact
dimensions and the client never predicts the move. Tab or `]` selects the next
assigned actor, `[` the previous, and `M` retains numeric-grid entry. The other
controls produce facing, stance, stop, normal end-turn or active-interrupt pass,
bounded aimed-fire, and
selected-actor reload plus modal `D` door selection (Up/Down/Tab, Enter inverse
state, Esc cancel). `commandsBlocked` disables every action and closes all three
command modals until committed state clears it, without predicting AP spend,
inventory/world mutation, or damage. Campaign-ready late peers enter the grow-only active
participant set only at a drained fresh-baseline boundary, and their actor ACLs
remain closed until baseline ACK; disconnect preserves ownership. The
Snapshot wire v7 uses an exact 53-byte header, 92-byte actor (including five
12-byte combat-equipment records), and 7-byte public door records; its generic
bound is 384053 bytes. Delta wire v6 permits 18434 generic events, orders
actor-loadout
changes after vitals and before door events, and encodes a same-serial interrupt
phase change as one exact 43-byte event. Co-op tactical wire v3 narrows this to 256 actors,
1024 doors, 3074 events, 30773/32385-byte baseline payload/envelope, and
62034/62106-byte delta payload/envelope beneath 64 KiB. Intent wire v3 is a
72-byte header plus at most 8
payload bytes (80 total), both tactical-world services are 2.0,
`DoorCapacityReached` is 12; journal-v4 door/pass command tags are 33/34. The native
helper preflights status/graphic, computes explicit-grid noise, swaps and verifies
the partner, commits status plus `LEVELNODE`, recompiles movement, performs POW
and flashlight maintenance, then returns; only success deducts AP/BP, emits
exactly one `OurNoise`, and runs sight/opponent-list/interrupt/AI work.
Integrity failure latches the world with no points/noise and stops replication.
Same-connection tactical recovery is implemented. An authenticated exact 88-byte
request is self-only and names one of six bounded reasons: delta-sequence gap,
payload-checksum mismatch, state mismatch, replica rejection, invalid envelope,
or baseline rejection. During recovery the client retains its last committed
view and freezes input; the server publishes a fresh baseline on the same socket
and waits for its ACK. The server validates the last committed checkpoint even
when a rejected replacement rotates to another baseline. Admission, socket,
identity, actor assignment, the server command cursor, pending commands, and
receipt history are preserved. An unchanged cursor clears only a proven-
unconsumed command; a consumed command remains locked through baseline ACK until
retained Queued and terminal receipts replay. Three failed replacement-baseline
attempts close the connection; reconnect remains transport-loss recovery.
Focused and real-socket tests cover both replacement rejection and recovery
with a consumed pending command.
The production-adapter socket E2E destroys and recreates the client composition,
restores the same peer without a second identity issuance, then reaches Move,
aimed fire, and reload, authoritative movement/AP/damage delta application, and
Applied receipts. It finishes by proving commit-before-result, durable marker
publication before clean socket stop, and peer compaction before world/epoch
teardown.

This remains a narrow technical slice: there is no client JA2 terrain/static-
world renderer. The five-slot combat-equipment projection is not the future
authorized, chunked full 55-slot inventory domain: reserve ammunition,
remaining equipment-stack
objects, attachments/LBE,
other items, general structures, door lock/key/trap/asynchronous interaction,
per-peer visibility, and broader-combat replication remain incomplete;
general strategic mission/session control is unfinished, and admission is
plaintext and permissionless rather than TLS/public authentication. An opt-in
installed-data independent-process smoke is registered only when both absolute
CMake cache paths `JA2_COOP_INSTALLED_SMOKE_EXECUTABLE` and
`JA2_COOP_INSTALLED_SMOKE_DATA_ROOT` are set; it is absent by default and is
currently POSIX-only. With a free loopback port and private temporary roots it
proves create, Ready, independent same-root client restart with a byte-identical
224-byte credential, clean worldless final checkpoint, and server resume. It
fingerprints installed inputs before/after and deterministically removes child
processes and temporary state. That lifecycle certification is not a complete
playthrough, interoperability matrix, or soak. Removing the former
presentation-only selector reduces the reviewed inventory by one context leaf
and one consumer file.

## Runtime-selection TODO

The reviewed executable raw-selector baseline is 103 sites across 28 files,
down from 149: 98 live-context calls, four cached-campaign comparisons, and
one active-package capability leaf. The strategic sector slice reduces its
private wrapper call inventory from 26 to 20 without changing the single
underlying context selector. The UB-option boundary now has 252 executable and
254 raw external occurrences across 31 consumer files; including the declaration
and adapter owner, it has 552 executable and 554 raw occurrences across 33
files. These are source-level ratchets in architecture CI rather than
completion claims. Later work should continue replacing a complete behavioral
cluster at a time while keeping campaign gates left of configuration, save,
dialogue, and effect probes. The legacy option record remains an application
adapter until all such consumers have moved to typed content.

## Literal remaining tail

`tools/lint_campaign_compile_guards.py` is the canonical inventory. At this
milestone its per-file baseline contains 28 active conditionals in 13
first-party files:

| Area | Conditionals |
| --- | ---: |
| Laptop content/pages | 4 |
| Tactical gameplay/content | 15 |
| Strategic gameplay/content | 0 |
| JA2 compatibility shell/layout | 7 |
| Tactical AI | 0 |
| Editor | 1 |
| Multiplayer | 1 |

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
`CampaignLaptopContentPolicy`. Door forcing and tunnel feedback use
`CampaignDoorPolicy`, so `Tactical/Handle Doors.cpp` no longer contributes
eight compiled branches and `Tactical/Interface.cpp` no longer contributes
five. Merc dismissal uses `CampaignMercenaryPolicy`, removing four branches
from `Strategic/Assignments.cpp`. Strategic-map lifecycle, guidance, and the
adjoining shell use `CampaignMapScreenPolicy` plus
`CampaignLaptopCommunicationsPolicy`; `Strategic/mapscreen.cpp` no longer
contributes twelve compiled branches, while the four converted map-shell
implementations and common map header no longer contribute nine more. The
strategic-event dispatcher uses `CampaignStrategicEventPolicy`; its one raw
selector no longer contributes to the tail, and all fourteen campaign-owned
callbacks remain locally routed in their existing switch cases. Civilian
quote catalogue selection, dedicated-group boundaries, UB's unavailable-record
sentinel, and Arulco's surrender completion use
`CampaignCivilianQuotePolicy`; `Tactical/Civ Quotes.cpp` and its public header
no longer contribute six more. IMP pass validation and text fallback use
`CampaignImpPolicy`; `Laptop/IMP HomePage.cpp` and
`Laptop/IMP Text System.cpp` no longer contribute six more. All nineteen
policies are guarded by data-free tests and named architecture checks, with
headless integration coverage where host composition is involved. Tactical meanwhile
behavior uses the application policy,
so the four converted tactical/tile implementations no longer contribute
eight more guards. Campaign progress and its scientist-AWOL threshold use
`CampaignProgressPolicy`, removing the first three former guards from
`Tactical/Campaign.cpp`. `CampaignLaptopCommunicationsPolicy` removes the
remaining include/death/level-up trio while preserving exact mail identities
and keeping UB configuration behind the runtime campaign gate. JA25 new-gun
dialogue uses `CampaignGunCommentPolicy`; `Tactical/Handle Items.cpp` and
`Tactical/Items.cpp` no longer contribute five guards, while their exact raw
ground-index and pre-placement inventory-item inputs remain unchanged. The
Tactical AI tail no longer contributes: `CampaignNpcPolicy` removes all eight
guards from `NPC.cpp`, `AIMain.cpp`, and `DecideAction.cpp` while retaining
their left-to-right campaign gates and legacy call inputs. The
Lua-global tail no longer contributes either: `CampaignLuaGlobalPolicy`
removes all five guards from `Strategic/Luaglobal.cpp` while retaining its
exact 92 guarded push/set pairs and campaign-selected arrival alias. The
quest/fact tail now uses `CampaignQuestPolicy`, removing all sixteen direct
campaign identity checks and the raw laptop-option read from
`Strategic/Quests.cpp` while preserving unavailable UB fact values, reward
amounts, initial selection, laptop effects, and POW support. The
strategic-content tail now uses `CampaignStrategicContentPolicy`, removing the
seven guards from its five Strategic sources while retaining Arulco's six
effects and every common predecessor, short circuit, state write, and daily
follow-through. The
seven remaining `Ja2` conditionals are the
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
| `Laptop/email.h` | 4 |
| `Tactical/Faces.cpp` | 4 |
| `Ja2/GameVersion.cpp` | 3 |
| `Tactical/Soldier Add.cpp` | 3 |
| `Tactical/TeamTurns.cpp` | 3 |

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
