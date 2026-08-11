# Runtime i18n architecture

## Goal and current boundary

The target is one application build whose text language is selected at startup
and can be changed in the options screen for the next restart. Text and voice
are separate choices; hot-reloading either one is outside this migration.

The first foundation slice does **not** make the legacy text catalogs runtime
selectable. It establishes one typed runtime catalog for the eight supported
languages, moves data prefixes, legacy archive names, localization suffixes,
and the Chinese map-message row policy into that catalog, and confines the old
preprocessor choice to `i18n/CompiledLanguage.h` and the files that still own
compiled text. `g_lang` remains immutable because changing it while the global
text variables still point at one compiled language would create a mixed and
invalid runtime.

## Why i18n is currently built per application and language

CMake creates an `${application}_${language}_i18n` archive for every requested
pair and passes exactly one of `ENGLISH`, `GERMAN`, `DUTCH`, `POLISH`,
`RUSSIAN`, `FRENCH`, `ITALIAN`, or `CHINESE` to it. There are four independent
reasons this is not merely a build-system naming problem:

1. `i18n/include/Text.h` declares 485 process-global text variables, and the
   JA25 compatibility header adds 35 more. There are 246 source/header files
   that include `Text.h` directly. The eight base-language translation units
   and eight JA25 translation units deliberately define the same external
   names. Compiling more than one language body into a program therefore
   creates duplicate global symbols; simply linking every existing archive
   cannot work.
2. The variables do not form one uniform immutable table. They include arrays
   of string pointers, fixed two-dimensional writable character buffers, and
   differently sized domain tables. Callers consume them directly and rely on
   their existing array types, indices, addresses, and in some cases mutable
   storage. A safe selector needs a schema plus accessors or a controlled
   compatibility publication step, not a cast between translation units.
3. The base catalogs contain 58 application/configuration preprocessor
   directives across the eight languages. `JA2UB` selects campaign text such as
   Arulco versus Tracona and changes several option/help entries;
   `JA2BETAVERSION` selects save-compatibility warnings. Consequently today's
   text object is an application/configuration variant as well as a language
   variant. Runtime language selection cannot silently choose the campaign.
4. `ExportStrings.cpp` textually includes the selected base-language `.cpp`
   inside namespace `Loc` to create a second namespaced copy for the developer
   XML exporter. The selected source is also compiled normally to define the
   process globals; every non-selected source compiles only its dummy archive
   symbol. This tool path must consume the future pack schema rather than
   textual source inclusion before all language bodies can coexist.

The old language-dependent entries in `Ja2 Libs.cpp` were not a fifth runtime
requirement. The legacy file database is a stub and bfVFS owns SLF mounting.
Those dead preprocessor-shaped entries are removed; their archive names now
live only as language/package metadata in the runtime catalog.

The existing XML localization support is not yet a replacement for the global
ABI. `LocalizedStrings` serves AIM biography/history/policy and dialogue-style
resources. `ExportStrings` can write a large `GameStrings.xml`, but there is no
inverse publisher that validates and installs all 485 `Text.h` variables.
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
guards from spreading back into neutral i18n files and prevent the 485 + 35
global declaration surfaces from growing.

## Migration sequence

1. Generate and validate a text-pack schema from the current declarations and
   all eight language definitions. The validator must build every supported
   language for both campaign families and report missing symbols, mismatched
   dimensions, campaign-conditioned entries, and mutable buffers. English is
   the explicit fallback for an absent optional translation, never an implicit
   linker accident.
2. Separate campaign-conditioned text from language data. Campaign keys belong
   to campaign packages or runtime campaign policy; build/debug wording belongs
   to a runtime build descriptor. A language pack may translate those keys but
   may not choose the campaign.
3. Introduce an immutable, validated `TextCatalog`/`TextPack` and migrate direct
   globals domain by domain. Start with small scalar/pointer tables, then fixed
   character buffers and genuinely mutable destinations. During migration one
   compatibility owner may publish legacy views once at startup; it must not
   copy partially validated data or swap addresses after consumers initialize.
4. Make the XML exporter consume the same pack schema and remove textual `.cpp`
   inclusion. Decide separately whether shipped packs remain generated C++
   data or become versioned package resources; the runtime API and validation
   rules must be identical either way.
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

- All eight catalogs need schema parity checks. Current CI treats only English
  and German as mandatory in the legacy language workflow, so a successful
  English build is not evidence that the remaining six packs are complete.
- The 58 campaign/build guards must be classified before catalog generation;
  flattening them under one arbitrary application would be behavior drift.
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
