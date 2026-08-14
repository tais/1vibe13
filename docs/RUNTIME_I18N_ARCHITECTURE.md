# Runtime i18n architecture

## Goal and current boundary

The target is one application build whose text language is selected at startup
and can be changed in the options screen for the next restart. Text and voice
are separate choices; hot-reloading either one is outside this migration.

The completed foundation, conditional-data, and first six domain slices do
**not** make the legacy text catalogs runtime selectable. The foundation
establishes one typed runtime catalog for the eight supported languages and
inventories the legacy ABI. The conditional-data slice separates
campaign/build-conditioned translations from the choice that publishes them.
The first six domain slices move five immutable Laptop titles, the AIM Links
page title, the Help-screen exit label, and the game-clock day label behind a
validated pack, then add five indexed game-time tables and the AIM Sort table
without changing startup selection. `g_lang` remains immutable:
changing it while the other global text variables still point at one compiled
language would create a mixed and invalid runtime.

## Why i18n is currently built per application and language

CMake creates an `${application}_${language}_i18n` archive for every requested
pair and passes exactly one of `ENGLISH`, `GERMAN`, `DUTCH`, `POLISH`,
`RUSSIAN`, `FRENCH`, `ITALIAN`, or `CHINESE` to it. There are four independent
reasons this is not merely a build-system naming problem:

1. `i18n/include/Text.h` now contains 471 `extern` lines. The normalized surface
   is 468 unique data declarations, two utility functions, and one duplicate
   `pDownloadString` declaration; two of those data declarations have no
   compiled-catalog definition, while five catalog globals are declared only
   at their consumers. The initial schema contained 485 base data definitions;
   the first six domain slices retire fourteen, leaving 471 base definitions,
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
4. `ExportStrings.cpp` still textually includes the selected base-language
   `.cpp` inside namespace `Loc` to create a second namespaced copy for most of
   the developer XML exporter. The 14 migrated sections now consume the same
   `TextPack` as the game, but the remaining selected source is also
   compiled normally to define process globals. This tool path must finish
   consuming pack schemas before all language bodies can coexist.

The old language-dependent entries in `Ja2 Libs.cpp` were not a fifth runtime
requirement. The legacy file database is a stub and bfVFS owns SLF mounting.
Those dead preprocessor-shaped entries are removed; their archive names now
live only as language/package metadata in the runtime catalog.

The existing XML localization support is not yet a replacement for the global
ABI. `LocalizedStrings` serves AIM biography/history/policy and dialogue-style
resources. `ExportStrings` can write a large `GameStrings.xml`, but there is no
inverse publisher that validates and installs all 471 remaining base catalog
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
471 + 35 global definition surfaces exactly.

## Canonical compiled-text ABI schema

Migration step 1 is now a build-free, mandatory source gate. The committed
`i18n/text_abi_schema.json` inventories the 471 remaining base definitions and
35 JA25 definitions as 506 unique data symbols. It also normalizes a historical
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

The schema records 57 pre-existing foreign-catalog compatibility gaps by
language and symbol. They are exact, reviewable debt rather than a wildcard:
any new gap or any unreviewed change to one fails validation. Removing the two
foreign-only guard shapes shrank this ceiling from 59; the existing Italian
`gzGIOScreenText` index shift remains explicit. Neither migration slice edits
translated wording or silently pads tables. Compatibility debt may not waive a
missing or extra symbol, a type mismatch, or a mutability mismatch; only the
exact legacy dimension/entry shapes are grandfathered.

## Canonical GameStrings export schema

The build-free `i18n/export_text_schema.json` manifest now records the entire
developer `GameStrings` boundary without changing it. Its ordered list contains
exactly 238 unique logical sections at their source positions: 224 direct
legacy `ExportSection` calls and 14 `TextPack` mappings. Every legacy entry
pins its section name, zero-based half-open range expression, base-ABI symbol,
declaration owner, rank, source and effective dimensions in all four compiled
quadrants, and storage mutability. The current lane consists of 208 mutable
pointer-slot tables and 16 writable `CHAR16` buffers; 219 symbols are declared
in `Text.h` and five retain consumer-local extern declarations. Every pack
entry instead pins its `TextKey` or `TextTableKey` descriptor, historical
section name, and export range. Misspellings and deliberately partial tables,
including
`TimeStings[0,1)`, remain data rather than being normalized by the validator.

The export view intersects 33 of the ABI schema's 57 foreign compatibility
debt pairs. Fourteen of those pairs are currently unsafe: their selected
foreign source array is shorter than the active export limit. They cover
German, Russian, Dutch, Polish, French, and Italian instances of `Message`,
`TacticalStr`, `TeamTurnString`, `pBookMarkStrings`, and
`pPersonnelScreenStrings`; each manifest entry records the actual entry count,
export limit, and shortfall. These are explicit non-growing debt, not approved
behavior. Reproducing the current out-of-range reads is not an adapter
compatibility contract. A linked-global selected-catalog/export adapter remains
blocked until all fourteen ranges receive a defined policy and golden coverage.

The same source inventory conservatively identifies exactly 14 tables whose
identifiers occur nowhere else in production C/C++ outside catalog bodies,
`Text.h`, and the exporter. They total 85 exported entries per language. A
commented historical consumer still counts as ownership, so this label does
not silently expand by treating commented dependencies as nonexistent. These
tables are later pack-migration candidates; this prerequisite does not move
their translations or change their storage.

`tools/check_i18n_export_schema.py` derives the manifest from
`ExportStrings.cpp`, `TextCatalog.h`, the validated ABI schema, and all eight
catalog shapes. It also pins startup ordering: the guarded export precedes
`Loc::ImportStrings` inside the legacy-content subsystem, legacy content starts
before the game subsystem, and the only two production `LoadAllExternalText`
calls remain in the later rules `LoadContent` path and multiplayer reload path.
Its focused unit tests reject commented-call parsing, range or order drift,
unknown debt ranges, wildcard or growing debt, storage/schema mismatch, and
startup reversal. CMake exposes the checks through the existing no-build i18n
target and lint CI runs them unconditionally. `ExportStrings.cpp`, the language
arrays, its eight textual includes, and runtime output are unchanged by this
manifest slice.

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
alternatives. `CompiledConditionalText.h` is the deliberately narrow
compatibility seam: it maps the legacy `JA2UB` and `JA2BETAVERSION` definitions
to a `ConditionalTextPolicy` and publishes the matching value into the
unchanged global table ABI. Thus a catalog translates campaign/build keys but
does not contain the policy that chooses a campaign. The fixed writable
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
All six current `TextTableKey` descriptors are required as well; construction
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
Startup selection, `g_lang`, legacy archives, mutable buffers, and textual
inclusion for the other tables remain unchanged.

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
surface is 240 files, and 224 executable legacy exporter sections remain.
Startup selection, `g_lang`, per-language archive collapse, mutable globals,
voice, and hot reload remain outside this slice.

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
   indexed game-time tables plus the complete AIM Sort table across all eight
   languages.
   Continue migrating direct globals domain by domain, then fixed character
   buffers and genuinely mutable destinations. No slice may copy partially
   validated data or swap addresses after consumers initialize.
4. **Complete prerequisite; adapter blocked:** commit the exact ordered
   GameStrings manifest, including 224 legacy and 14 pack sections, 33 exported
   compatibility-debt pairs, the 14 currently unsafe range pairs, the 14
   exporter-only tables, and startup export-before-import/external-load order.
   This source-only gate changes no language storage or exporter behavior.
5. **In progress:** the XML exporter consumes the same pack for those 14
   sections. Move the remaining sections, then remove textual `.cpp` inclusion.
   The linked-global adapter cannot begin by copying undefined reads: first
   resolve the fourteen unsafe foreign ranges and add intended-output goldens.
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
  treats non-English/non-German full legacy executable builds as soft. The 57
  inventoried compatibility gaps must shrink or remain exact; they may not grow
  while translated tables move toward canonical parity.
- The 58 retired campaign/build guard groups, 98 conditioned entries, and 196
  exact literal alternatives must stay behind the value/schema policy.
  Flattening either axis under one arbitrary application would be behavior
  drift.
- The selected developer exporter still has 14 foreign-language ranges that
  exceed their source arrays. They must remain explicit, may only shrink, and
  block replacing textual catalog inclusion with a linked-global adapter until
  their intended empty/missing-entry behavior is defined and covered.
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
