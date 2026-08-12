# Runtime i18n architecture

## Goal and current boundary

The target is one application build whose text language is selected at startup
and can be changed in the options screen for the next restart. Text and voice
are separate choices; hot-reloading either one is outside this migration.

The first three migration slices do **not** make the legacy text catalogs
runtime selectable. The foundation establishes one typed runtime catalog for
the eight supported languages and inventories the legacy ABI. The second slice
separates campaign/build-conditioned translations from the choice that
publishes them. The first domain slice moves five immutable Laptop titles
behind a validated pack without changing startup selection. `g_lang` remains
immutable because changing it while the other global text variables still
point at one compiled language would create a mixed and invalid runtime.

## Why i18n is currently built per application and language

CMake creates an `${application}_${language}_i18n` archive for every requested
pair and passes exactly one of `ENGLISH`, `GERMAN`, `DUTCH`, `POLISH`,
`RUSSIAN`, `FRENCH`, `ITALIAN`, or `CHINESE` to it. There are four independent
reasons this is not merely a build-system naming problem:

1. `i18n/include/Text.h` now contains 480 `extern` lines. The normalized surface
   is 477 unique data declarations, two utility functions, and one duplicate
   `pDownloadString` declaration; two of those data declarations have no
   compiled-catalog definition, while five catalog globals are declared only
   at their consumers. The initial schema contained 485 base data definitions;
   the first domain slice retires five, leaving 480 base definitions, while the
   JA25 compatibility surface still adds 35. There are 245 source/header files
   that include `Text.h` directly. The eight base-language translation units
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
   the developer XML exporter. The five migrated title sections now consume the
   same `TextPack` as the game, but the remaining selected source is also
   compiled normally to define process globals. This tool path must finish
   consuming pack schemas before all language bodies can coexist.

The old language-dependent entries in `Ja2 Libs.cpp` were not a fifth runtime
requirement. The legacy file database is a stub and bfVFS owns SLF mounting.
Those dead preprocessor-shaped entries are removed; their archive names now
live only as language/package metadata in the runtime catalog.

The existing XML localization support is not yet a replacement for the global
ABI. `LocalizedStrings` serves AIM biography/history/policy and dialogue-style
resources. `ExportStrings` can write a large `GameStrings.xml`, but there is no
inverse publisher that validates and installs all 480 remaining base catalog
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
480 + 35 global definition surfaces exactly.

## Canonical compiled-text ABI schema

Migration step 1 is now a build-free, mandatory source gate. The committed
`i18n/text_abi_schema.json` inventories the 480 remaining base definitions and
35 JA25 definitions as 515 unique data symbols. It also normalizes a historical
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
the whole catalog validates. All five current `TextKey` descriptors are
required, so an absent title rejects construction rather than falling back.
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
   owns the first five one-entry Laptop title tables across all eight languages.
   Continue migrating direct globals domain by domain, then fixed character
   buffers and genuinely mutable destinations. No slice may copy partially
   validated data or swap addresses after consumers initialize.
4. **In progress:** the XML exporter consumes the same pack for those five
   sections. Move the remaining sections, then remove textual `.cpp` inclusion.
   Decide separately whether shipped packs remain generated C++ data or become
   versioned package resources; runtime API and validation rules stay identical.
5. Select and validate the language code during startup before rules/campaign
   text is consumed. Persist changes from the options screen for the next
   restart, then route text paths, graphics, word wrapping/fonts, and data
   overlays through the selected descriptor and pack. Voice selection remains
   independent.
6. Once no process-global catalog definitions remain, remove the eight legacy
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
