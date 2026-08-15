# Runtime i18n architecture

## Goal and current boundary

The target is one application build whose text language is selected at startup
and can be changed in the options screen for the next restart. Text and voice
are separate choices; hot-reloading either one is outside this migration.

The completed foundation, conditional-data, and first seven domain slices do
**not** make the legacy text catalogs runtime selectable. The foundation
establishes one typed runtime catalog for the eight supported languages and
inventories the legacy ABI. The conditional-data slice separates
campaign/build-conditioned translations from the choice that publishes them.
The first six domain slices move five immutable Laptop titles, the AIM Links
page title, the Help-screen exit label, and the game-clock day label behind a
validated pack, then add five indexed game-time tables and the AIM Sort table
without changing startup selection. The seventh moves the complete 14-table,
85-entry exporter-only cohort into that same pack. `g_lang` remains immutable:
changing it while the other global text variables still point at one compiled
language would create a mixed and invalid runtime.

## Why i18n is currently built per application and language

CMake creates an `${application}_${language}_i18n` archive for every requested
pair and passes exactly one of `ENGLISH`, `GERMAN`, `DUTCH`, `POLISH`,
`RUSSIAN`, `FRENCH`, `ITALIAN`, or `CHINESE` to it. There are four independent
reasons this is not merely a build-system naming problem:

1. `i18n/include/Text.h` now contains 457 `extern` lines. The normalized surface
   is 454 unique data declarations, two utility functions, and one duplicate
   `pDownloadString` declaration; two of those data declarations have no
   compiled-catalog definition, while five catalog globals are declared only
   at their consumers. The initial schema contained 485 base data definitions;
   the first seven domain slices retire twenty-eight, leaving 457 base definitions,
   while the JA25 compatibility surface still adds 35. There are 240 source/header
   files that include `Text.h` directly. The eight base-language translation units
   and eight JA25 translation units deliberately define the remaining same
   external names. Compiling more than one language body into a program
   therefore still creates duplicate global symbols; simply linking every
   existing archive cannot work.
2. The variables do not form one uniform immutable table. They include arrays
   of string pointers, fixed two-dimensional writable character buffers, and
   differently sized domain tables. Callers consume them directly and rely on
   their existing array types, indices, addresses, and in some cases mutable
   storage. A safe selector needs a schema plus accessors or a controlled
   compatibility publication step, not a cast between translation units.
3. The base catalogs historically contained 58 application/configuration
   preprocessor guards across the eight languages. `JA2UB` selected campaign
   text such as Arulco versus Tracona and changed several option/help entries;
   `JA2BETAVERSION` selected save-compatibility warnings. Both alternatives now
   live in every affected language source behind explicit campaign/build keys,
   but the compatibility publisher still selects one when constructing the old
   global tables. Runtime language selection still cannot silently choose the
   campaign.
4. `SelectedCatalogExport.cpp` now links the normally compiled selected
   base-language globals and emits them through an injected immediate-copy
   sink. `ExportStrings.cpp` owns only the VFS writer and six raw EDT exporters;
   it no longer creates a second namespaced catalog copy. The 28 migrated
   sections consume the same `TextPack` as the game. The remaining 210 globals
   still need domain-by-domain pack migration before all language bodies can
   coexist in one process.

The old language-dependent entries in `Ja2 Libs.cpp` were not a fifth runtime
requirement. The legacy file database is a stub and bfVFS owns SLF mounting.
Those dead preprocessor-shaped entries are removed; their archive names now
live only as language/package metadata in the runtime catalog.

The existing XML localization support is not yet a replacement for the global
ABI. `LocalizedStrings` serves AIM biography/history/policy and dialogue-style
resources. `ExportStrings` can write a large `GameStrings.xml`, but there is no
inverse publisher that validates and installs all 457 remaining base catalog
variables.
`XML_Language.cpp` transactionally overlays its dedicated tactical-message
table, not the general text catalog.

## Foundation contract

`i18n::SupportedLanguages` is now the canonical catalog. Existing `i18n::Lang`
numeric values remain stable because Lua exposes them. Every entry has a unique
configuration name and persisted two-letter code, plus its data prefix,
legacy archive name, localization suffix, and map-message row count. Lookups
reject unknown values rather than indexing an enum-sized array unchecked.

The obsolete and differently ordered `Loc::Language` enum (which also claimed
an unsupported Taiwanese entry) is gone. Export conversion uses `i18n::Lang`,
so code no longer has two incompatible language identities. Language-neutral
SLF metadata and multi-language graphic selection are compiled once into
`ja2_i18n_runtime` and embedded in each compatibility archive. The legacy text
sources remain variant objects by design.

The data-free `i18n_language_catalog_tests` pins all eight identities, their
existing Lua numbers and paths, unique lookup keys, Chinese layout behavior,
and invalid-input rejection. Architecture checks prevent language compile
guards from spreading back into neutral i18n files and pin the remaining
457 + 35 global definition surfaces exactly.

## Canonical compiled-text ABI schema

Migration step 1 is now a build-free, mandatory source gate. The committed
`i18n/text_abi_schema.json` inventories the 457 remaining base definitions and
35 JA25 definitions as 492 unique data symbols. It also normalizes a historical
duplicate `pDownloadString` declaration, keeps function declarations separate
from data, records each array rank and effective dimension, and distinguishes
mutable pointer slots from the 26 writable `CHAR16` buffers. Campaign/build
selection no longer changes initializer structure, so the canonical ABI now
contains zero condition-shaped symbols in all four quadrants.

`tools/check_i18n_text_schema.py` checks English, German, Russian, Dutch,
Polish, French, Italian, and Chinese in all four campaign/build quadrants:
JA2 release, JA2 debug, JA2UB release, and JA2UB debug. It reports missing and
extra symbols and type, source-dimension, effective-dimension, initializer
entry-count, mutability, and conditional-layout mismatches. The lint workflow
runs this validator and its focused tests unconditionally; unlike the legacy
full foreign-executable jobs, none of the eight catalog checks is soft.
Catalog conditionals may name only their language macro. Reintroducing
`JA2UB`, `JA2BETAVERSION`, or another configuration macro fails instead of
creating an untracked catalog variant.

The schema records 41 pre-existing foreign-catalog compatibility gaps by
language and symbol. They are exact, reviewable debt rather than a wildcard:
any new gap or any unreviewed change to one fails validation. Removing the two
foreign-only guard shapes first shrank this ceiling from 59 to 57. Normalizing
the 14 unsafe GameStrings range pairs then removed another 14 exact overlays;
realigning Italian `gzGIOScreenText` to the canonical 69 entries removed its
last index-shift overlay. The range repairs insert nonempty entries at their
canonical semantic ordinals rather than silently padding or shifting later
keys. Migrating German `pIMPFinishButtonText` also removed its former dimension
overlay. Compatibility debt may not waive a missing or extra symbol, a type
mismatch, or a mutability mismatch; only the exact remaining legacy
dimension/entry shapes are grandfathered.

## Canonical GameStrings export schema

The build-free `i18n/export_text_schema.json` manifest now records the entire
developer `GameStrings` boundary without changing it. Its ordered list contains
exactly 238 unique logical sections at their source positions: 210 direct
legacy `ExportSection` calls and 28 `TextPack` mappings. Every legacy entry
pins its section name, zero-based half-open range expression, base-ABI symbol,
declaration owner, rank, source and effective dimensions in all four compiled
quadrants, and storage mutability. Every pack entry instead pins its `TextKey`
or `TextTableKey` descriptor, historical section name, and export range. The
current legacy lane consists of 194 mutable
pointer-slot tables and 16 writable `CHAR16` buffers; 205 symbols are declared
in `Text.h` and five retain consumer-local extern declarations. Misspellings
and deliberately partial tables,
including
`TimeStings[0,1)`, remain data rather than being normalized by the validator.

The export view intersects 17 of the ABI schema's 41 remaining foreign
compatibility-debt pairs, and none exceeds its selected source array. The 14
previously unsafe German, Russian, Dutch, Polish, French, and Italian instances
of `Message`, `TacticalStr`, `TeamTurnString`, `pBookMarkStrings`, and
`pPersonnelScreenStrings` now define all 19 missing physical entries at their
canonical semantic ordinals. The Polish TeamTurn repair separates its two
adjacent literals; Italian and Dutch middle insertions preserve every later
GameStrings key. Source-level goldens pin all repaired entries plus their
alignment/tail anchors, require nonempty values, and reject a missing comma,
missing middle entry, empty replacement, or exporter-range shrink. That
prerequisite now feeds a dedicated linked-global export adapter.
`SelectedCatalogExport.cpp` is compiled in the same language, campaign, and
build variant target as `language.cpp` and the catalog sources. It enumerates
the unchanged 238-section order through an injected borrowed-view sink and
never textually includes a catalog body.

That adapter safety claim is exhaustive rather than foreign-debt-only. For every
one of the 210 legacy sections, the validator evaluates the exact exporter
range expression and compares it with each raw catalog-source definition in all
eight languages and four campaign/build quadrants: 6,720 comparisons. A
wildcard first dimension means the top-level initializer count; the enclosing
`Text.h` declaration cannot mask a short linked definition. Unknown names,
unsupported expressions, missing symbols, and unresolved dimensions fail
closed. All 77 symbolic limits have one strict, unconditional compiler contract
of direct decimal `static_assert` rows. `SelectedCatalogExport.cpp` includes it
as the first adapter statement, so every normal language/configuration build compares
the reviewed values with the live C++ macro or enum providers in the actual
exporter translation unit; the exporter source may not locally shadow any of
those provider names. An immediately preceding `#ifdef static_assert`/`#error`
guard also prevents a transitive or command-line macro from erasing the compiler
checks. The build-free validator parses that same controlled list for range
arithmetic; it deliberately does not reimplement the C++
preprocessor, enum grammar, or signed integer semantics. In particular,
`NUM_ICONS`, `TEXT_NUM_AIM_ALUMNI`, and `TEXT_NUM_LARGESTR` are 18, 5, and 3.
The captured pre-fix
fixture proves why this matters: eight sections,
59 language/table pairs, 236 quadrant failures, and 64,127 potential
out-of-bounds reads per selected build. The committed manifest records zero in
all four failure measures.

Capacity alone is insufficient for mutable pointer-slot tables: an in-range
`nullptr`, identifier, or other expression would still make export behavior
undefined or adapter-dependent. The same pass therefore validates all 83,040
selected `STR16` entries across the 32 catalog/quadrant combinations: 82,712
are direct (possibly concatenated) wide literals and 328 are exact
`I18N_COMPILED_BUILD_TEXT` or `I18N_COMPILED_CAMPAIGN_TEXT` selectors. Any
implicit zero-initialized tail or other expression fails closed. Each catalog
must also retain its exact six-include preamble, with
the re-includable macro-only `CompiledConditionalTextSelectors.inc` as the
final include before every selector and no unreviewed catalog directive that
could turn a reviewed selector into `nullptr`. The global
`CompiledConditionalText.h` owns only the policy API and imports that same
17-macro seam once; each selected catalog resets it after its other headers.

The closure adds exactly 104 semantic entries across the eight catalogs:
`pPersonnelAssignmentStrings[83..84]`, `pLongAssignmentStrings[54,83..84]`,
`pDoorTrapStrings[5..6]`, and `pLandTypeStrings[41..46]`. Assignment and trap
aliases reuse the same-catalog canonical literals; untranslated land-type
tails use explicit `TODO.Translate` English fallbacks. `WeaponType` exports the
nine gun-type labels rather than `MAXITEMS`, and the two two-entry merc-leave
tables export two entries. Italian GIO removes its duplicate arsenal label,
moves `INSANE` to canonical index 12, and drops the unused ultimate-iron-man
enum slot. Exact middle/tail goldens, alias checks, 69-entry GIO alignment, and
four named `ASSIGNMENT_EMPTY` consumers prevent ordinal regression.

The same source inventory previously identified exactly 14 tables whose
identifiers occurred nowhere else in production C/C++ outside catalog bodies,
`Text.h`, and the exporter. They totaled 85 exported entries per language.
Those tables now have typed `TextTableKey` ownership, so zero exporter-only
tables remain in the legacy inventory. A commented historical consumer still
counts as ownership, preventing this category from silently expanding.

`tools/check_i18n_export_schema.py` derives the manifest from
`SelectedCatalogExport.cpp`, `ExportStrings.cpp`, `TextCatalog.h`, the
validated ABI schema, and all eight catalog shapes. It also pins startup
ordering: the guarded export precedes
`Loc::ImportStrings` inside the legacy-content subsystem, legacy content starts
before the game subsystem, and the only two production `LoadAllExternalText`
calls remain in the later rules `LoadContent` path and multiplayer reload path.
Its focused unit tests reject commented/raw-literal parsing bypasses, range or
name drift, unknown range expressions, unresolved wildcard dimensions,
wildcard or growing debt, storage/schema mismatch, startup reversal, missing
middle or tail entries, empty repaired slots, missing translation markers, and
adjacent literal concatenation. The variant adapter has an exact flat
depth-zero inventory of 238 direct export statements. Only the compiler guard
and first-statement limit-contract include are allowed in that body, and all
symbolic-limit identifier uses must belong to parsed ranges.
`Loc::ExportStrings` separately owns three declarations, the one adapter call,
two property writes, six raw EDT exporters, and the final return. Its sink
materializes both borrowed views into VFS strings before the callback returns;
the adapter suppresses empty values, retains absolute source indices, and has
explicit paths for `STR16` slots, fixed `wchar_t[N]` rows, and scalar `wchar_t`
storage. The unique adapter and exporter definitions begin at preprocessor
depth zero, and no conditional may cross either boundary. TextPack descriptors
and startup-order tokens cannot hide in inactive
preprocessor branches or macro bodies. The selector `.inc` has no active
non-directive residue, while its policy API and final selector import remain
unconditional. Reviewed startup functions are likewise active top-level
definitions; their required load/phase calls retain exact evaluated expression
shapes and cannot follow direct control transfer. The shared lexer hides multiline raw strings before
looking for storage. CMake exposes the build-free checks through the existing
i18n target and lint CI runs them unconditionally; mandatory normal builds own
the separate live-provider `static_assert` proof. The eight textual catalog
includes are gone. The exact five globals absent from `Text.h`
(`pBullseyeStrings`, `pContractButtonString`, `pUpdatePanelButtons`,
`gzIntroScreen`, and `sRepairsDoneString`) have direct adapter-local extern
declarations; all other legacy values link through `Text.h`. `g_lang` remains
immutable, and one language-target definition selects linked globals,
`g_lang`, and `GetCompiledTextPack`. Three deliberately overbroad export limits
remain narrowed to their defined semantic tables; the 19 earlier foreign
repairs and 104 catalog entries retain exact source-level goldens. The manifest
also hashes ordered section/index/value UTF-8 bytes in all 32
language/campaign/build snapshots, so output order and values cannot drift
under a schema rewrite. A focused linked-English sanitizer test additionally
executes pointer-slot, fixed-row, scalar-buffer, empty-suppression, absolute-
index, immediate-copy, and later/manual mutable-global snapshot behavior.

The sole production export still runs inside the legacy-content boundary before
`Loc::ImportStrings`, game startup, or either external-text load. At that point
the linked mutable globals retain their compiled initializers, making its output
byte-for-byte equivalent to the retired private textual copies. A deliberate
later/manual call has a clearer contract: it snapshots the then-current linked
globals. The sink still copies each view immediately and never retains an
address across calls.

## Conditional value/schema policy

Migration step 2 retires exactly 58 catalog guard groups: seven common groups
in each of eight languages, plus the Dutch and French `pFilesSenderList`
groups. They cover 98 conditioned table entries and 196 exact literal
alternatives in `pCountryNames`,
`zSaveLoadText`, `gzGIOScreenText`, `pMapScreenJustStartedHelpText`,
`gzLateLocalizedString`, and `pFilesSenderList`.

`ConditionalTextPolicy.h` classifies 13 value keys by campaign or build axis
and records their canonical legacy table positions. Twelve keys occur in all
eight catalogs. `FilesSenderReport` occurs only in Dutch and French because
only those two sources historically guarded `pFilesSenderList[0]`; the other
six catalogs retain their one unconditional value and do not claim a second
translation. Each schema-owned catalog instance supplies both literal
alternatives. `CompiledConditionalText.h` and its re-includable macro-only
selector seam form the deliberately narrow compatibility boundary: they map
the legacy `JA2UB` and `JA2BETAVERSION` definitions to a
`ConditionalTextPolicy` and publish the matching value into the unchanged
global table ABI. Thus a catalog translates campaign/build keys but does not
contain the policy that chooses a campaign. The fixed writable
`pCountryNames` rows are why this seam still publishes at compile time; step 3
will replace that limitation with validated startup pack publication.

`i18n/conditional_text_schema.json` pins every alternative exactly, including
all seven foreign translations, per-language key availability, and the Italian
positional compatibility debt.
`tools/check_i18n_conditional_text.py` rejects a catalog configuration guard,
unknown or misplaced key, wrong policy axis, expression/fallback in place of a
literal, missing selector, or wording drift. Its model exercises JA2 release,
JA2 beta/debug, JA2UB release, and JA2UB beta/debug independently. This is value
validation; the ABI schema remains responsible for storage shape and
mutability.

English fallback is explicit and prospective. Every symbol is required by
default, there are currently no optional symbols, and the linker is never a
fallback mechanism. `TextFallbackPolicy::EnglishForOptionalKeys` may resolve a
key from English only when its descriptor explicitly opts in and only after
the whole catalog validates. All eight current `TextKey` descriptors are
required, so an absent title rejects construction rather than falling back.
All twenty current `TextTableKey` descriptors are required as well; construction
reports the exact missing table and index before publishing any selected pack.
Each selected legacy compiled catalog must likewise remain complete according
to its ratcheted compatibility schema. `g_lang` therefore remains immutable
and no runtime language selection occurs in this slice.

## Immutable Laptop-title pack boundary

Migration step 3 begins with exactly five one-entry tables:
`pPersonnelTitle`, `pEmailTitleText`, `pFinanceTitle`, `pFilesTitle`, and
`pHistoryTitle`. Their 40 literals move unchanged from the eight duplicate
language bodies into neutral `BuiltinDefinitions`. The five `Text.h`
declarations and all 40 global definitions are gone.

`TextCatalog::Create` validates one uniquely identified pack for each of the
eight `SupportedLanguages`, rejects unknown or duplicate identities, and
rejects every missing required key before returning a catalog. Construction
copies the input into immutable shared storage. A selected `TextPack` shares
ownership of that storage, so its `wstring_view` results stay valid for the
pack lifetime even if the `TextCatalog` value is destroyed. The compiled seam
publishes one function-static pack selected by the existing immutable
`g_lang`; it performs no startup selection and never swaps consumer addresses.

Nine Laptop render sites (four email sites and one on each other page) now use
typed keys. The five existing `GameStrings.xml` sections are emitted at their
original positions from that same pack, preserving exporter names and values.
Dependency-free tests pin all 40 literals, all five exporter mappings, compiled
English behavior, invalid identities/keys, duplicate-identity rejection,
required-key rollback, lookup provenance, and pointer/lifetime stability.
Mutable buffers, the other legacy globals, archive collapse, persisted language
selection, voice, and hot reload remain outside this bounded slice.

## Immutable AIM Links-title pack boundary

The next-domain audit found 42 remaining one-entry base `STR16` pointer tables.
`AimLinkText` is a smallest complete consumer boundary: it has one literal in
each of the eight languages, no conditional layout in any of the four checked
campaign/build quadrants, one read-only render use in `Laptop/AimLinks.cpp`,
and one `AimLink` exporter section. Repository-wide use inventory found no
pointer-slot writes. The slots were mutable only as a property of the legacy
ABI, not because the AIM Links page changed them. Unlike the tied singleton
labels embedded in larger consumers, this choice retires the complete page's
only direct legacy text dependency.

All eight exact literals now occupy the required `AimLinksTitle` key. The old
declaration, eight duplicate definitions, and one-entry indexing constants are
gone, and the complete AIM Links consumer no longer includes `Text.h`. Its
render obtains lifetime-stable text from `GetCompiledTextPack`; its exporter
call remains between `AimPopUp` and `AimHistory`, preserving the original XML
section order and `AimLink` name. There is no linker or English fallback.

At that boundary the dependency-free catalog model pinned all 48 migrated
literals and six export mappings, including the exact compiled English AIM
Links title, missing AIM Links translation rollback, lookup provenance, and
storage lifetime. The same sanitizer CI target covers every completed domain.
At that earlier boundary startup selection, `g_lang`, legacy archives, mutable
buffers, and textual inclusion for the other tables remained unchanged.

## Immutable Help-screen exit-label pack boundary

The next-domain audit found 41 remaining one-entry base `STR16` pointer tables.
`gzHelpScreenText` was the smallest complete consumer boundary: one unguarded
literal in each language and build quadrant, one read-only use for the Help
screen's exit-button fast-help label, and one existing `HelpScreen` exporter
section. It was the only such candidate whose migration also removed its
consumer's complete direct `Text.h` dependency. This does not migrate the
larger EDT-backed body-text system declared by `HelpScreenText.h`.

All eight exact literals now occupy the required, append-only
`HelpScreenExit` key, preserving the six existing key ordinals. The legacy
declaration, eight definitions, and sole one-entry enum are gone. The Help
screen reads the process-lifetime compiled pack, and the button system copies
the label into region-owned storage. The exporter still emits `HelpScreen`
between `LaptopHelp` and `NonPersistantPBI`, with its original one-entry order.
There is no linker or English fallback.

The catalog model now pins all 56 migrated literals and seven exporter
mappings, compiled English behavior, required-key rollback, lookup provenance,
and storage lifetime. The canonical ABI contains 478 base plus 35 JA25 data
symbols (513 total), and 40 base singleton pointer tables remain. Startup
selection, `g_lang`, archives, mutable buffers, Help-screen body text, voice,
and hot reload remain outside this slice.

## Immutable game-clock day-label pack boundary

The next audit found 40 remaining base singleton pointer tables.
`gpGameClockString` was the smallest complete exported domain: one unguarded
literal in every language and build quadrant, three read-only formatting uses,
and one existing `GameClock` exporter section. `Strategic/Game Events.cpp`
used no other base `Text.h` catalog symbol, so this migration also closes that
consumer's complete direct legacy-text dependency. `Strategic/Game Clock.cpp`
and `Laptop/BobbyRShipments.cpp` retain `Text.h` only for unrelated tables.
Repository-wide inventory found no pointer-slot writes or retained legacy
addresses.

All eight exact literals now occupy the required, append-only `GameClockDay`
key, preserving the seven existing key ordinals. The old declaration, eight
definitions, one-entry enum, and obsolete commented legacy read are gone. Each
consumer formats immediately from lifetime-stable catalog storage; no runtime
publication or address swap was introduced. The exporter still emits
`GameClock` between `Strategic` and `KeyDescription`, preserving its original
section name, order, and sole entry. There is no linker or English fallback.

The catalog model now pins all 64 migrated literals and eight exporter
mappings, compiled English behavior, required-key rollback, lookup provenance,
and storage lifetime. The canonical ABI contains 477 base plus 35 JA25 data
symbols (512 total), and 39 base singleton pointer tables remain. The direct
`Text.h` include surface is 242 files. Startup selection, `g_lang`, archives,
mutable buffers, voice, and hot reload remain outside this slice.

## Immutable indexed game-time pack boundary

The fifth complete domain introduces first-class indexed tables instead of
flattening multi-entry data into unrelated scalar keys. `TextTableKey` and its
descriptor schema pin each table's stable identity, contiguous offset, entry
count, fallback rule, and historical exporter range. Catalog construction owns
and validates every indexed entry transactionally; lookup rejects an invalid
table or index, reports exact missing-entry provenance, and retains the same
lifetime-stable storage contract as scalar text.

Exactly `sTimeStrings[6]`, `gsTimeStrings[4]`, `pDayStrings[1]`,
`pEtaString[1]`, and `pPausedGameText[3]` move into that boundary: 15 entries
in each of eight languages, or 120 exact indexed translations. All live time
compression, unit-suffix, day, ETA, and pause-screen consumers now use typed
indexed lookup. `Strategic/Game Clock.cpp` consequently closes its remaining
direct `Text.h` dependency. Italian deliberately retains `Giorno` for the
former `pDayStrings` entry while the separate scalar `GameClockDay` remains
`Gg`; the two historical domains are not normalized together.

The exporter keeps all five calls at their original positions and retains the
exact section contracts: `Time` exports indices `[0, 6)`, the misspelled
`TimeStings` exports only `[0, 1)` from its four-entry runtime table, `Day` and
`Eta` each export `[0, 1)`, and `PausedGame` exports `[0, 3)`. The old five
declarations and all 40 duplicate table definitions are gone. The catalog now
covers 184 literals and 13 exporter mappings. At this fifth-domain boundary the
canonical legacy ABI contained 472 base plus 35 JA25 data symbols (507 total),
37 base singleton pointer tables remained, and the direct `Text.h` include
surface was 241 files. Startup selection, mutable buffers, archive collapse,
language macros, voice, and hot reload remain outside this slice.

## Immutable AIM Sort pack boundary

The sixth complete domain moves the entire `AimSortText[20]` table rather than
another isolated singleton. Its 160 exact literals across all eight languages
append to the indexed pack as required `TextTableKey::AimSort` ordinal 5. The
descriptor fixes offset 15, runtime count 20, fallback count zero, and exporter
count 20; the earlier five table ordinals and their ranges do not move. Together
the two indexed domains now pin 280 exact indexed translations. The Chinese
`排序: ` value deliberately retains its trailing ASCII space.

`Laptop/AimSort.cpp` owns a scoped 20-value index model covering every entry in
legacy order and routes all seven bounded consumer reads through the compiled
pack. The old unscoped index constants, `Text.h` declaration, and eight duplicate
definitions are gone, so the complete AIM Sort consumer no longer includes
`Text.h`. Source inventory found no conditional initializer, compatibility-debt
entry, pointer-slot write, or retained-address use for this table; the migration
therefore does not publish mutable state or add a startup selector.

The exporter emits all 20 entries from the same descriptor and preserves the
exact `BobbyRaysFront` < `AimSort` < `AimPolicy` section order. Missing-entry,
invalid-index-20, stable-address, language-provenance, compiled-English, and
exact-literal tests cover the new boundary. The catalog now covers 344 literals
and 14 exporter mappings. The canonical ABI contains 471 base plus 35 JA25 data
symbols (506 total), including 480 mutable pointer slots and 26 writable fixed
buffers; 37 base singleton pointer tables remain. The direct `Text.h` include
surface is 241 files (including the linked adapter), and 224 executable legacy
exporter sections remain.
Startup selection, `g_lang`, per-language archive collapse, mutable globals,
voice, and hot reload remain outside this slice.

## Immutable exporter-only TextPack cohort

The seventh complete domain closes the exporter-only legacy tail identified by
the exhaustive GameStrings inventory. Fourteen tables append in their original
exporter order: `LongAttribute[10]`, `Training[4]`, `GuardMenu[10]`,
`OtherGuardMenu[10]`, `ContractExtend[3]`, `NoiseType[12]`, `Traverse[2]`,
`MercContractOver[5]`, `SkiAtm[15]`, `IMPFinishButton[1]`, `IMPVoices[1]`,
`DepartedMercPortrait[3]`, `MiscString[5]`, and `GioDifConfirm[4]`. Their 85
entries in each of eight languages add 680 exact literals without an English
fallback. Together all twenty indexed tables now pin 960 exact indexed
translations, and the complete catalog owns 1,024 literals.

The new keys occupy append-only ordinals 6 through 19 and contiguous offsets
35 through 119. All fourteen old `Text.h` declarations and 112 catalog
definitions are gone. The unused Ski ATM and difficulty-confirmation index
enums are retired; the gameplay-owned contract-duration enum remains in its
strategic owner. German `pIMPFinishButtonText` no longer needs its historical
dimension overlay, shrinking canonical compatibility debt from 42 to 41.

Each adapter call stays at its exact former ordinal—16, 19, 20, 21, 43, 46,
94, 117, 176, 195, 197, 198, 210, and 220—and emits the same absolute indices.
All 32 language/campaign/build output hashes are identical to the linked-adapter
parent. The manifest therefore remains 238 logical sections while changing its
ownership split from 224 legacy plus 14 pack mappings to 210 legacy plus 28
pack mappings. The live named-limit contract drops only the three bounds no
longer consumed by legacy export calls and now contains 77 rows. Exact-literal,
missing-required-entry, upper-bound, provenance, stable-address, linked-English
sanitizer, and ordered-output tests cover the boundary. Zero exporter-only
tables remain.

The canonical ABI now contains 457 base plus 35 JA25 data symbols (492 total),
including 466 mutable pointer-slot tables and 26 writable fixed buffers. Runtime
language selection, archive collapse, mutable destinations, voice, and hot
reload remain outside this slice.

## Migration sequence

1. **Complete:** generate and validate a text-pack schema from the current
   declarations and all eight language definitions. The build-free validator
   covers both campaign families and both build modes and reports missing or
   extra symbols, type/dimension/conditional mismatches, and mutable buffers.
   English is the explicit future fallback for an absent optional translation,
   never an implicit linker accident.
2. **Complete:** separate campaign/build-conditioned text from the policy that
   selects it. All 58 catalog guards are retired, both alternatives are exact
   schema data, campaign keys belong to campaign policy, and beta/debug wording
   belongs to build policy. A language catalog translates those keys but does
   not choose the campaign.
3. **In progress:** the immutable, validated `TextCatalog`/`TextPack` boundary
   owns the first five one-entry Laptop title tables, the complete AIM Links
   title domain, the Help-screen exit label, the game-clock day label, and five
   indexed game-time tables, the complete AIM Sort table, and the complete
   14-table/85-entry exporter-only cohort across all eight languages.
   Continue migrating direct globals domain by domain, then fixed character
   buffers and genuinely mutable destinations. No slice may copy partially
   validated data or swap addresses after consumers initialize.
4. **Complete prerequisite:** commit the exact ordered
   GameStrings manifest, including 210 legacy and 28 pack sections, 17 exported
   compatibility-debt pairs, 6,720 exhaustive raw-dimension comparisons with
   zero unsafe results, zero remaining exporter-only tables, and startup
   export-before-import/external-load order. The repaired semantic ordinals and
   three narrowed limits remove undefined adjacent-array reads without changing
   startup order.
5. **Adapter complete; pack migration in progress:** the XML exporter consumes
   the same pack for those 28 sections, and one variant adapter now links the
   remaining 210 globals without any textual `.cpp` inclusion. Its borrowed
   views are copied immediately and all 32 ordered-output snapshots are pinned.
   Move the remaining sections into validated packs domain by domain.
   Decide separately whether shipped packs remain generated C++ data or become
   versioned package resources; runtime API and validation rules stay identical.
6. Select and validate the language code during startup before rules/campaign
   text is consumed. Persist changes from the options screen for the next
   restart, then route text paths, graphics, word wrapping/fonts, and data
   overlays through the selected descriptor and pack. Voice selection remains
   independent.
7. Once no process-global catalog definitions remain, remove the eight legacy
   language macros, collapse `${application}_${language}_i18n` into one i18n
   target, and update release artifacts from language-named executables to one
   executable plus language packages.

## Explicit blockers and review gates

- All eight catalogs now have mandatory schema checks even though CI still
  treats non-English/non-German full legacy executable builds as soft. The 41
  inventoried compatibility gaps must shrink or remain exact; they may not grow
  while translated tables move toward canonical parity.
- The 58 retired campaign/build guard groups, 98 conditioned entries, and 196
  exact literal alternatives must stay behind the value/schema policy.
  Flattening either axis under one arbitrary application would be behavior
  drift.
- Every one of the 210 selected developer-export ranges now fits all eight
  catalogs in all four quadrants. Zero unsafe sections, language pairs,
  quadrant failures, and potential OOB reads is a hard gate; exact nonempty
  semantic goldens prevent empty padding or shifted-key repairs.
- Asset availability and overlay precedence must be defined for prefixes,
  graphics, fonts, SLFs, XML, Lua, dialogue, and voice. Selecting a text pack
  without the matching required assets must fail before game initialization or
  fall back according to an explicit package policy.
- Mutable/fixed-size globals need per-symbol migration rules. Rebinding pointer
  arrays is not sufficient for callers compiled against fixed array objects.
- Saved configuration needs a stable string code. Enum ordinals remain a Lua
  compatibility surface, not a persistence format for new settings.

Each later slice must build all affected application variants, run catalog
schema validation for all eight languages, exercise invalid/missing pack
rollback, and keep AddressSanitizer coverage before the next group of global
consumers moves.
