if(NOT DEFINED SOURCE_ROOT)
  message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/CMakeLists.txt" root_build_contents)
foreach(required_lua_fetch_fragment IN ITEMS
    "https://github.com/lua/lua/archive/refs/tags/v5.5.0.tar.gz"
    "SHA256=a33484f7ce4c14e12ea4d51cc5a7353bff2796a8074004b96ae2dc246f33f16e"
    "set(LUA_SOURCE_ROOT"
    "lua|luac|onelua|ltests")
  string(FIND "${root_build_contents}" "${required_lua_fetch_fragment}"
    required_lua_fetch_fragment_position)
  if(required_lua_fetch_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Pinned Lua mirror integration lost '${required_lua_fetch_fragment}'")
  endif()
endforeach()
if(root_build_contents MATCHES "www[.]lua[.]org/ftp")
  message(FATAL_ERROR
    "The single-host Lua FTP dependency returned; keep CI on the pinned official mirror")
endif()

# Runtime language identity is data even while the legacy text definitions are
# still compatibility archives. Keep the typed catalog and one neutral object
# partition in place, quarantine language compile guards to the unresolved text
# owners, and do not let the global ABI grow during migration.
file(READ "${SOURCE_ROOT}/i18n/include/language.hpp"
  i18n_language_catalog_contents)
foreach(required_language_catalog_fragment IN ITEMS
    "struct LanguageDescriptor"
    "SupportedLanguages"
    "FindLanguageByConfigName"
    "FindLanguageByCode"
    "legacyArchiveName"
    "localizationSuffix"
    "mapMessageRows")
  string(FIND "${i18n_language_catalog_contents}"
    "${required_language_catalog_fragment}"
    required_language_catalog_position)
  if(required_language_catalog_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime language catalog lost '${required_language_catalog_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/i18n/CMakeLists.txt" i18n_build_contents)
foreach(required_i18n_build_fragment IN ITEMS
    "set(i18nCommonSrc"
    "Ja2 Libs.cpp"
    "Multi Language Graphic Utils.cpp"
    "TextCatalog.cpp"
    "set(i18nVariantSrc"
    "language.cpp"
    "ExportStrings.cpp")
  string(FIND "${i18n_build_contents}" "${required_i18n_build_fragment}"
    required_i18n_build_position)
  if(required_i18n_build_position EQUAL -1)
    message(FATAL_ERROR
      "i18n source partition lost '${required_i18n_build_fragment}'")
  endif()
endforeach()
foreach(required_i18n_root_build_fragment IN ITEMS
    "add_library(ja2_i18n_runtime OBJECT"
    "$<TARGET_OBJECTS:ja2_i18n_runtime>")
  string(FIND "${root_build_contents}" "${required_i18n_root_build_fragment}"
    required_i18n_root_build_position)
  if(required_i18n_root_build_position EQUAL -1)
    message(FATAL_ERROR
      "Language-neutral i18n object reuse lost '${required_i18n_root_build_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/i18n/language.cpp" i18n_language_source_contents)
file(READ "${SOURCE_ROOT}/i18n/TextCatalog.cpp"
  i18n_text_catalog_source_contents)
file(READ "${SOURCE_ROOT}/i18n/include/TextCatalog.h"
  i18n_text_catalog_header_contents)
file(READ "${SOURCE_ROOT}/Utils/LocalizedStrings.h"
  localized_strings_header_contents)
file(READ "${SOURCE_ROOT}/i18n/ExportStrings.cpp"
  export_strings_source_contents)
file(READ "${SOURCE_ROOT}/i18n/Ja2 Libs.cpp" ja2_libs_source_contents)
file(READ "${SOURCE_ROOT}/i18n/include/Ja2 Libs.h"
  ja2_libs_header_contents)
foreach(neutral_i18n_contents IN ITEMS
    i18n_language_source_contents
    i18n_text_catalog_source_contents
    i18n_text_catalog_header_contents
    localized_strings_header_contents
    ja2_libs_source_contents
    ja2_libs_header_contents)
  if("${${neutral_i18n_contents}}" MATCHES
      "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*([^A-Za-z0-9_]|^)(ENGLISH|GERMAN|DUTCH|POLISH|RUSSIAN|FRENCH|ITALIAN|CHINESE)([^A-Za-z0-9_]|$)")
    message(FATAL_ERROR
      "A language compile guard escaped into neutral i18n source '${neutral_i18n_contents}'")
  endif()
endforeach()
if(localized_strings_header_contents MATCHES
    "enum[ \t\r\n]+Language|LANGUAGE_COUNT|LangSuffix")
  message(FATAL_ERROR
    "LocalizedStrings regained a second, incompatible language identity")
endif()
if(NOT export_strings_source_contents MATCHES "i18n::Lang")
  message(FATAL_ERROR
    "The legacy string exporter must consume the canonical runtime language identity")
endif()

file(GLOB i18n_guard_candidate_files
  "${SOURCE_ROOT}/i18n/*.cpp"
  "${SOURCE_ROOT}/i18n/*.h"
  "${SOURCE_ROOT}/i18n/include/*.h"
  "${SOURCE_ROOT}/i18n/include/*.hpp")
foreach(i18n_guard_candidate IN LISTS i18n_guard_candidate_files)
  file(READ "${i18n_guard_candidate}" i18n_guard_candidate_contents)
  if(NOT i18n_guard_candidate_contents MATCHES
      "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*([^A-Za-z0-9_]|^)(ENGLISH|GERMAN|DUTCH|POLISH|RUSSIAN|FRENCH|ITALIAN|CHINESE)([^A-Za-z0-9_]|$)")
    continue()
  endif()
  if(i18n_guard_candidate STREQUAL
      "${SOURCE_ROOT}/i18n/CompiledLanguage.h" OR
      i18n_guard_candidate STREQUAL
      "${SOURCE_ROOT}/i18n/ExportStrings.cpp" OR
      i18n_guard_candidate MATCHES
      "/i18n/_(Ja25)?(Chinese|Dutch|English|French|German|Italian|Polish|Russian)Text[.]cpp$")
    continue()
  endif()
  message(FATAL_ERROR
    "Language compile guard escaped its compatibility owner: ${i18n_guard_candidate}")
endforeach()

file(READ "${SOURCE_ROOT}/i18n/include/Text.h" legacy_text_header_contents)
string(REGEX MATCHALL "(^|[\r\n])[ \t]*extern[ \t]+"
  legacy_text_declarations "${legacy_text_header_contents}")
list(LENGTH legacy_text_declarations legacy_text_declaration_count)
if(NOT legacy_text_declaration_count EQUAL 478)
  message(FATAL_ERROR
    "The first three TextCatalog domains must leave exactly 478 legacy Text.h externs, found ${legacy_text_declaration_count}")
endif()
file(READ "${SOURCE_ROOT}/i18n/include/_Ja25EnglishText.h"
  legacy_ja25_text_header_contents)
string(REGEX MATCHALL "(^|[\r\n])[ \t]*extern[ \t]+"
  legacy_ja25_text_declarations "${legacy_ja25_text_header_contents}")
list(LENGTH legacy_ja25_text_declarations legacy_ja25_text_declaration_count)
if(legacy_ja25_text_declaration_count GREATER 35)
  message(FATAL_ERROR
    "The legacy JA25 text global ABI grew from 35 to ${legacy_ja25_text_declaration_count}; add new text through the runtime catalog")
endif()

file(READ "${SOURCE_ROOT}/tests/i18n_language_catalog_tests.cpp"
  i18n_language_catalog_test_contents)
foreach(required_i18n_test_fragment IN ITEMS
    "static_cast<int>(i18n::Lang::zh) == 7"
    "FindLanguageByConfigName"
    "FindLanguageByCode"
    "invalid runtime language is rejected")
  string(FIND "${i18n_language_catalog_test_contents}"
    "${required_i18n_test_fragment}" required_i18n_test_position)
  if(required_i18n_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime language catalog tests lost '${required_i18n_test_fragment}'")
  endif()
endforeach()

# The first three domain migrations own seven immutable one-entry UI labels.
# All 56 translated definitions move together, every runtime/exporter consumer
# enters through the validated pack, and no legacy declaration remains to
# provide an accidental linker fallback.
foreach(required_text_catalog_header_fragment IN ITEMS
    "enum class TextKey"
    "struct TextKeyDescriptor"
    "HasValidTextKeySchema"
    "static_cast<std::size_t>(TextKey::HelpScreenExit) == 6"
    "englishFallbackAllowed"
    "enum class TextFallbackPolicy"
    "EnglishForOptionalKeys"
    "struct TextPackDefinition"
    "class TextPack"
    "class TextCatalog"
    "std::shared_ptr<const detail::TextCatalogStorage>"
    "GetCompiledTextPack")
  string(FIND "${i18n_text_catalog_header_contents}"
    "${required_text_catalog_header_fragment}"
    required_text_catalog_header_position)
  if(required_text_catalog_header_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime TextCatalog boundary lost '${required_text_catalog_header_fragment}'")
  endif()
endforeach()
foreach(required_text_catalog_source_fragment IN ITEMS
    "BuiltinDefinitions"
    "TextCatalog::Create"
    "TextCatalogError::InvalidFallbackPolicy"
    "TextCatalogError::DuplicateLanguage"
    "TextCatalogError::MissingRequiredText"
    "TextFallbackPolicy::EnglishForOptionalKeys"
    "std::make_shared<detail::TextCatalogStorage>"
    "TextPack::lookup")
  string(FIND "${i18n_text_catalog_source_contents}"
    "${required_text_catalog_source_fragment}"
    required_text_catalog_source_position)
  if(required_text_catalog_source_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime TextCatalog implementation lost '${required_text_catalog_source_fragment}'")
  endif()
endforeach()

set(migrated_text_globals
  pPersonnelTitle
  pEmailTitleText
  pFinanceTitle
  pFilesTitle
  pHistoryTitle
  AimLinkText
  gzHelpScreenText)
set(legacy_base_catalog_files
  _EnglishText.cpp
  _GermanText.cpp
  _RussianText.cpp
  _DutchText.cpp
  _PolishText.cpp
  _FrenchText.cpp
  _ItalianText.cpp
  _ChineseText.cpp)
foreach(migrated_text_global IN LISTS migrated_text_globals)
  if(legacy_text_header_contents MATCHES
      "extern[ \t\r\n]+STR16[ \t\r\n]+${migrated_text_global}[ \t\r\n]*\\[")
    message(FATAL_ERROR
      "Migrated text regained legacy declaration '${migrated_text_global}'")
  endif()
  foreach(legacy_base_catalog_file IN LISTS legacy_base_catalog_files)
    file(READ "${SOURCE_ROOT}/i18n/${legacy_base_catalog_file}"
      legacy_base_catalog_contents)
    if(legacy_base_catalog_contents MATCHES
        "STR16[ \t\r\n]+${migrated_text_global}[ \t\r\n]*\\[")
      message(FATAL_ERROR
        "${legacy_base_catalog_file} regained migrated definition '${migrated_text_global}'")
    endif()
  endforeach()
endforeach()
if(legacy_text_header_contents MATCHES
    "HLP_SCRN_TXT__EXIT_SCREEN|TEXT_NUM_HLP")
  message(FATAL_ERROR
    "Help-screen exit regained its retired one-entry legacy index constants")
endif()

set(migrated_text_consumers
  "Laptop/personnel.cpp|pPersonnelTitle|PersonnelTitle|1"
  "Laptop/email.cpp|pEmailTitleText|EmailTitle|4"
  "Laptop/finances.cpp|pFinanceTitle|FinanceTitle|1"
  "Laptop/files.cpp|pFilesTitle|FilesTitle|1"
  "Laptop/history.cpp|pHistoryTitle|HistoryTitle|1"
  "Laptop/AimLinks.cpp|AimLinkText|AimLinksTitle|1"
  "Ja2/HelpScreen.cpp|gzHelpScreenText|HelpScreenExit|1")
foreach(migrated_text_consumer IN LISTS migrated_text_consumers)
  string(REPLACE "|" ";" migrated_text_fields
    "${migrated_text_consumer}")
  list(GET migrated_text_fields 0 migrated_text_file)
  list(GET migrated_text_fields 1 migrated_text_global)
  list(GET migrated_text_fields 2 migrated_text_key)
  list(GET migrated_text_fields 3 migrated_text_expected_count)
  file(READ "${SOURCE_ROOT}/${migrated_text_file}"
    migrated_text_consumer_contents)
  if(migrated_text_consumer_contents MATCHES
      "${migrated_text_global}")
    message(FATAL_ERROR
      "${migrated_text_file} regained direct '${migrated_text_global}' access")
  endif()
  string(REGEX MATCHALL "TextKey::${migrated_text_key}"
    migrated_text_key_uses
    "${migrated_text_consumer_contents}")
  list(LENGTH migrated_text_key_uses
    migrated_text_actual_count)
  if(NOT migrated_text_actual_count EQUAL
      migrated_text_expected_count)
    message(FATAL_ERROR
      "${migrated_text_file} must route exactly ${migrated_text_expected_count} '${migrated_text_key}' use(s), found ${migrated_text_actual_count}")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/Laptop/AimLinks.cpp" aim_links_text_consumer_contents)
if(aim_links_text_consumer_contents MATCHES
    "#[ \t]*include[ \t]*[<\"]Text[.]h[>\"]" OR
    NOT aim_links_text_consumer_contents MATCHES
    "#[ \t]*include[ \t]*[<\"]TextCatalog[.]h[>\"]")
  message(FATAL_ERROR
    "Aim Links must depend only on TextCatalog for its migrated text")
endif()
file(READ "${SOURCE_ROOT}/Ja2/HelpScreen.cpp"
  help_screen_text_consumer_contents)
if(help_screen_text_consumer_contents MATCHES
    "#[ \t]*include[ \t]*[<\"]Text[.]h[>\"]" OR
    NOT help_screen_text_consumer_contents MATCHES
    "#[ \t]*include[ \t]*[<\"]TextCatalog[.]h[>\"]")
  message(FATAL_ERROR
    "Help screen must depend only on TextCatalog for its migrated exit label")
endif()

foreach(migrated_text_export IN ITEMS
    PersonnelTitle EmailTitle FinanceTitle FilesTitle HistoryTitle AimLinksTitle
    HelpScreenExit)
  if(export_strings_source_contents MATCHES
      "Loc::(pPersonnelTitle|pEmailTitleText|pFinanceTitle|pFilesTitle|pHistoryTitle|AimLinkText|gzHelpScreenText)")
    message(FATAL_ERROR
      "ExportStrings regained a legacy migrated text global")
  endif()
  string(REGEX MATCHALL
    "ExportTextPackEntry\\(props, i18n::TextKey::${migrated_text_export}\\)"
    migrated_text_export_uses
    "${export_strings_source_contents}")
  list(LENGTH migrated_text_export_uses
    migrated_text_export_count)
  if(NOT migrated_text_export_count EQUAL 1)
    message(FATAL_ERROR
      "ExportStrings must publish '${migrated_text_export}' exactly once through TextPack")
  endif()
endforeach()
string(FIND "${export_strings_source_contents}" "Loc::AimPopUpText"
  aim_popup_export_position)
string(FIND "${export_strings_source_contents}"
  "ExportTextPackEntry(props, i18n::TextKey::AimLinksTitle)"
  aim_links_export_position)
string(FIND "${export_strings_source_contents}" "Loc::AimHistoryText"
  aim_history_export_position)
if(aim_popup_export_position EQUAL -1 OR aim_links_export_position EQUAL -1 OR
    aim_history_export_position EQUAL -1 OR
    NOT aim_popup_export_position LESS aim_links_export_position OR
    NOT aim_links_export_position LESS aim_history_export_position)
  message(FATAL_ERROR
    "Aim Links TextPack export must retain its legacy position between AimPopUp and AimHistory")
endif()
string(FIND "${export_strings_source_contents}" "Loc::gzLaptopHelpText"
  laptop_help_export_position)
string(FIND "${export_strings_source_contents}"
  "ExportTextPackEntry(props, i18n::TextKey::HelpScreenExit)"
  help_screen_exit_export_position)
string(FIND "${export_strings_source_contents}" "Loc::gzNonPersistantPBIText"
  non_persistant_pbi_export_position)
if(laptop_help_export_position EQUAL -1 OR
    help_screen_exit_export_position EQUAL -1 OR
    non_persistant_pbi_export_position EQUAL -1 OR
    NOT laptop_help_export_position LESS help_screen_exit_export_position OR
    NOT help_screen_exit_export_position LESS non_persistant_pbi_export_position)
  message(FATAL_ERROR
    "Help-screen TextPack export must retain its legacy position between LaptopHelp and NonPersistantPBI")
endif()

file(READ "${SOURCE_ROOT}/tests/i18n_text_catalog_tests.cpp"
  i18n_text_catalog_test_contents)
foreach(required_text_catalog_test_fragment IN ITEMS
    "all 56 migrated translations remain byte-for-byte exact"
    "legacy exporter section mapping stays exact"
    "all seven migrated keys remain required in every language"
    "the Help-screen key appends without renumbering existing TextKey ordinals"
    "compiled default behavior publishes the exact English Aim Links title"
    "compiled default behavior publishes the exact English help-screen exit label"
    "English fallback cannot mask a missing required Aim Links translation"
    "English fallback cannot mask a missing required help-screen exit translation"
    "typed key identities and names cannot duplicate"
    "TextPack owns stable storage after its catalog value is destroyed"
    "repeated lookup retains a stable text address"
    "duplicate pack identity rejects the whole catalog"
    "English fallback cannot mask a missing required translation"
    "an unknown fallback policy rejects the whole catalog"
    "compiled pack selection still follows immutable g_lang")
  string(FIND "${i18n_text_catalog_test_contents}"
    "${required_text_catalog_test_fragment}"
    required_text_catalog_test_position)
  if(required_text_catalog_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime TextCatalog tests lost '${required_text_catalog_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/CMakeLists.txt"
  i18n_text_catalog_test_build_contents)
foreach(required_text_catalog_build_fragment IN ITEMS
    "add_executable(i18n_text_catalog_tests"
    "i18n/TextCatalog.cpp"
    "i18n/language.cpp"
    "target_compile_definitions(i18n_text_catalog_tests PRIVATE ENGLISH)"
    "NAME i18n_text_catalog")
  string(FIND "${i18n_text_catalog_test_build_contents}"
    "${required_text_catalog_build_fragment}"
    required_text_catalog_build_position)
  if(required_text_catalog_build_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime TextCatalog test target lost '${required_text_catalog_build_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/.github/workflows/build_unix.yml"
  i18n_text_catalog_ci_contents)
if(NOT i18n_text_catalog_ci_contents MATCHES
    "cmake --build build --target[^\r\n]*i18n_text_catalog_tests")
  message(FATAL_ERROR
    "Runtime TextCatalog sanitizer build coverage was removed")
endif()
file(READ "${SOURCE_ROOT}/docs/RUNTIME_I18N_ARCHITECTURE.md"
  runtime_i18n_architecture_contents)
string(REGEX REPLACE "[ \t\r\n]+" " "
  runtime_i18n_architecture_normalized
  "${runtime_i18n_architecture_contents}")
foreach(required_runtime_i18n_doc_fragment IN ITEMS
    "475 unique data declarations"
    "retires exactly 58 catalog guard groups"
    "98 conditioned table entries and 196 exact literal alternatives"
    "CompiledConditionalText.h"
    "ExportStrings.cpp"
    "Migration sequence"
    "Explicit blockers and review gates"
    "Immutable Laptop-title pack boundary"
    "Canonical compiled-text ABI schema"
    "513 unique data symbols"
    "57 pre-existing foreign-catalog compatibility gaps"
    "All seven current `TextKey` descriptors are required"
    "Immutable AIM Links-title pack boundary"
    "Immutable Help-screen exit-label pack boundary"
    "40 base singleton pointer tables remain"
    "linker is never a fallback mechanism")
  string(FIND "${runtime_i18n_architecture_normalized}"
    "${required_runtime_i18n_doc_fragment}"
    required_runtime_i18n_doc_position)
  if(required_runtime_i18n_doc_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime i18n architecture record lost '${required_runtime_i18n_doc_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tools/check_i18n_text_schema.py"
  runtime_i18n_schema_tool_contents)
foreach(required_runtime_i18n_schema_tool_fragment IN ITEMS
    "LANGUAGES = ("
    "QUADRANTS = ("
    "FALLBACK_POLICY = {"
    "MAX_COMPATIBILITY_DEBT_SYMBOLS = 57"
    "implicit_linker_fallback"
    "untracked conditional macro"
    "missing symbol"
    "extra symbol"
    "type mismatch"
    "dimension mismatch"
    "conditional mismatch"
    "catalog_compatibility_debt")
  string(FIND "${runtime_i18n_schema_tool_contents}"
    "${required_runtime_i18n_schema_tool_fragment}"
    required_runtime_i18n_schema_tool_position)
  if(required_runtime_i18n_schema_tool_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime i18n schema validator lost '${required_runtime_i18n_schema_tool_fragment}'")
  endif()
endforeach()

# Language files own translations, not application/build selection. The 58
# retired catalog guards may not return under a different spelling.
foreach(runtime_i18n_catalog IN ITEMS
    _EnglishText.cpp
    _GermanText.cpp
    _RussianText.cpp
    _DutchText.cpp
    _PolishText.cpp
    _FrenchText.cpp
    _ItalianText.cpp
    _ChineseText.cpp)
  file(READ "${SOURCE_ROOT}/i18n/${runtime_i18n_catalog}"
    runtime_i18n_catalog_contents)
  string(REGEX MATCH
    "#[ \\t]*(if|ifdef|ifndef|elif)[^\\r\\n]*(JA2UB|JA2BETAVERSION)"
    runtime_i18n_catalog_configuration_guard
    "${runtime_i18n_catalog_contents}")
  if(runtime_i18n_catalog_configuration_guard)
    message(FATAL_ERROR
      "${runtime_i18n_catalog} regained compiled campaign/build text selection")
  endif()
  string(FIND "${runtime_i18n_catalog_contents}"
    "#include \"CompiledConditionalText.h\""
    runtime_i18n_catalog_policy_include)
  if(runtime_i18n_catalog_policy_include EQUAL -1)
    message(FATAL_ERROR
      "${runtime_i18n_catalog} lost the conditional-text publication seam")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/i18n/include/ConditionalTextPolicy.h"
  runtime_i18n_conditional_policy_contents)
foreach(required_runtime_i18n_conditional_policy_fragment IN ITEMS
    "enum class CampaignTextVariant"
    "enum class BuildTextVariant"
    "ConditionalTextKeys"
    "SelectCampaignText"
    "SelectBuildText"
    "FilesSenderReport")
  string(FIND "${runtime_i18n_conditional_policy_contents}"
    "${required_runtime_i18n_conditional_policy_fragment}"
    required_runtime_i18n_conditional_policy_position)
  if(required_runtime_i18n_conditional_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Conditional text policy lost '${required_runtime_i18n_conditional_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/i18n/CompiledConditionalText.h"
  runtime_i18n_compiled_conditional_contents)
foreach(required_runtime_i18n_compiled_conditional_fragment IN ITEMS
    "I18N_COMPILED_CAMPAIGN_TEXT"
    "I18N_COMPILED_BUILD_TEXT"
    "CompiledConditionalTextPolicy"
    "CampaignTextVariant::ja2ub"
    "BuildTextVariant::beta")
  string(FIND "${runtime_i18n_compiled_conditional_contents}"
    "${required_runtime_i18n_compiled_conditional_fragment}"
    required_runtime_i18n_compiled_conditional_position)
  if(required_runtime_i18n_compiled_conditional_position EQUAL -1)
    message(FATAL_ERROR
      "Compiled conditional text seam lost '${required_runtime_i18n_compiled_conditional_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/i18n/ExportStrings.cpp"
  runtime_i18n_export_strings_contents)
string(FIND "${runtime_i18n_export_strings_contents}"
  "#include \"CompiledConditionalText.h\""
  runtime_i18n_export_conditional_include)
if(runtime_i18n_export_conditional_include EQUAL -1)
  message(FATAL_ERROR
    "ExportStrings must import conditional policy before catalog textual inclusion")
endif()

file(READ "${SOURCE_ROOT}/tools/check_i18n_conditional_text.py"
  runtime_i18n_conditional_tool_contents)
foreach(required_runtime_i18n_conditional_tool_fragment IN ITEMS
    "RETIRED_GUARD_COUNT"
    "CONDITIONED_ENTRY_COUNT"
    "LITERAL_ALTERNATIVE_COUNT"
    "LEGACY_INDEX_OVERRIDES"
    "catalog regained JA2UB/JA2BETAVERSION directives"
    "conditioned catalog values differ from exact schema"
    "selector alternative contains an expression")
  string(FIND "${runtime_i18n_conditional_tool_contents}"
    "${required_runtime_i18n_conditional_tool_fragment}"
    required_runtime_i18n_conditional_tool_position)
  if(required_runtime_i18n_conditional_tool_position EQUAL -1)
    message(FATAL_ERROR
      "Conditional text validator lost '${required_runtime_i18n_conditional_tool_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/i18n/conditional_text_schema.json"
  runtime_i18n_conditional_schema_contents)
foreach(required_runtime_i18n_conditional_schema_fragment IN ITEMS
    "\"retired_catalog_guard_count\": 58"
    "\"conditioned_entry_count\": 98"
    "\"literal_alternative_count\": 196"
    "\"legacy_index_overrides\""
    "\"catalog_values\""
    "\"SaveAndGameVersionChanged\""
    "\"FilesSenderReport\"")
  string(FIND "${runtime_i18n_conditional_schema_contents}"
    "${required_runtime_i18n_conditional_schema_fragment}"
    required_runtime_i18n_conditional_schema_position)
  if(required_runtime_i18n_conditional_schema_position EQUAL -1)
    message(FATAL_ERROR
      "Conditional text exact-value schema lost '${required_runtime_i18n_conditional_schema_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tools/test_check_i18n_conditional_text.py"
  runtime_i18n_conditional_test_contents)
foreach(required_runtime_i18n_conditional_test_fragment IN ITEMS
    "test_all_58_legacy_guards_have_explicit_groups_and_positions"
    "test_selector_requires_one_key_and_two_literal_alternatives"
    "test_initializer_comments_cannot_invent_table_entries"
    "test_commented_policy_include_does_not_satisfy_catalog_gate"
    "test_every_catalog_has_only_schema_owned_selectors"
    "test_all_four_quadrants_choose_the_declared_axis_only"
    "test_committed_values_resolve_exactly_in_every_catalog_quadrant"
    "test_exact_value_drift_names_the_language"
    "test_duplicate_schema_keys_cannot_shadow_values"
    "test_catalog_configuration_guards_are_detected")
  string(FIND "${runtime_i18n_conditional_test_contents}"
    "${required_runtime_i18n_conditional_test_fragment}"
    required_runtime_i18n_conditional_test_position)
  if(required_runtime_i18n_conditional_test_position EQUAL -1)
    message(FATAL_ERROR
      "Conditional text validator tests lost '${required_runtime_i18n_conditional_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/i18n/text_abi_schema.json"
  runtime_i18n_schema_contents)
foreach(required_runtime_i18n_schema_fragment IN ITEMS
    "\"schema_version\": 1"
    "\"canonical_language\": \"English\""
    "\"base_data_declarations\": 475"
    "\"implicit_linker_fallback\": false"
    "\"catalog_compatibility_debt\""
    "\"ja2-release\""
    "\"ja2-debug\""
    "\"ja2ub-release\""
    "\"ja2ub-debug\"")
  string(FIND "${runtime_i18n_schema_contents}"
    "${required_runtime_i18n_schema_fragment}"
    required_runtime_i18n_schema_position)
  if(required_runtime_i18n_schema_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime i18n canonical schema lost '${required_runtime_i18n_schema_fragment}'")
  endif()
endforeach()
string(REGEX MATCHALL "\"domain\": \"(base|ja25)\""
  runtime_i18n_schema_data_symbols "${runtime_i18n_schema_contents}")
list(LENGTH runtime_i18n_schema_data_symbols
  runtime_i18n_schema_data_symbol_count)
if(NOT runtime_i18n_schema_data_symbol_count EQUAL 513 OR
    runtime_i18n_schema_contents MATCHES "\"gzHelpScreenText\"")
  message(FATAL_ERROR
    "Runtime i18n canonical schema must contain 513 data symbols without the migrated Help-screen global")
endif()

file(READ "${SOURCE_ROOT}/tools/test_check_i18n_text_schema.py"
  runtime_i18n_schema_test_contents)
foreach(required_runtime_i18n_schema_test_fragment IN ITEMS
    "test_header_inventory_normalizes_duplicates_functions_and_mutability"
    "test_campaign_and_build_quadrants_select_exact_initializer_shapes"
    "test_diagnostics_name_every_structural_mismatch_class"
    "test_untracked_catalog_configuration_cannot_escape_the_four_quadrants"
    "test_parser_rejects_unrecognized_top_level_catalog_storage"
    "test_catalog_debt_rejects_wildcard_fields_and_growth"
    "test_schema_json_rejects_shadowing_duplicate_keys"
    "test_english_fallback_is_explicit_and_never_a_linker_accident")
  string(FIND "${runtime_i18n_schema_test_contents}"
    "${required_runtime_i18n_schema_test_fragment}"
    required_runtime_i18n_schema_test_position)
  if(required_runtime_i18n_schema_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime i18n schema tests lost '${required_runtime_i18n_schema_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/.github/workflows/lint.yml"
  runtime_i18n_lint_workflow_contents)
foreach(required_runtime_i18n_lint_fragment IN ITEMS
    "Validate all compiled-text catalogs"
    "python3 tools/check_i18n_text_schema.py"
    "python3 -m unittest tools/test_check_i18n_text_schema.py"
    "python3 tools/check_i18n_conditional_text.py"
    "python3 -m unittest tools/test_check_i18n_conditional_text.py")
  string(FIND "${runtime_i18n_lint_workflow_contents}"
    "${required_runtime_i18n_lint_fragment}"
    required_runtime_i18n_lint_position)
  if(required_runtime_i18n_lint_position EQUAL -1)
    message(FATAL_ERROR
      "Mandatory runtime i18n schema lint lost '${required_runtime_i18n_lint_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/CMakeLists.txt"
  runtime_i18n_test_build_contents)
foreach(required_runtime_i18n_test_build_fragment IN ITEMS
    "add_custom_target(i18n_text_schema_check"
    "NAME i18n_text_schema"
    "NAME i18n_text_schema_tool"
    "NAME i18n_conditional_text_schema"
    "NAME i18n_conditional_text_schema_tool"
    "i18n_conditional_text_policy_tests"
    "i18n_text_catalog_tests"
    "i18n_compiled_conditional_text_ja2_release_tests"
    "i18n_compiled_conditional_text_ja2_beta_tests"
    "i18n_compiled_conditional_text_ja2ub_release_tests"
    "i18n_compiled_conditional_text_ja2ub_beta_tests")
  string(FIND "${runtime_i18n_test_build_contents}"
    "${required_runtime_i18n_test_build_fragment}"
    required_runtime_i18n_test_build_position)
  if(required_runtime_i18n_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime i18n schema CMake target lost '${required_runtime_i18n_test_build_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/.github/workflows/build_unix.yml"
  runtime_i18n_sanitizer_workflow_contents)
foreach(required_runtime_i18n_sanitizer_target IN ITEMS
    "i18n_text_catalog_tests"
    "i18n_conditional_text_policy_tests"
    "i18n_compiled_conditional_text_ja2_release_tests"
    "i18n_compiled_conditional_text_ja2_beta_tests"
    "i18n_compiled_conditional_text_ja2ub_release_tests"
    "i18n_compiled_conditional_text_ja2ub_beta_tests")
  string(REGEX MATCH
    "cmake --build build --target[^\r\n]*${required_runtime_i18n_sanitizer_target}"
    required_runtime_i18n_sanitizer_target_match
    "${runtime_i18n_sanitizer_workflow_contents}")
  if(NOT required_runtime_i18n_sanitizer_target_match)
    message(FATAL_ERROR
      "Runtime i18n sanitizer build lost '${required_runtime_i18n_sanitizer_target}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TODO" runtime_i18n_todo_contents)
foreach(required_runtime_i18n_todo_fragment IN ITEMS
    "mandatory 8-language/4-quadrant Text ABI schema"
    "first five one-entry Laptop titles use a validated immutable TextPack"
    "AIM Links title is the second complete pack domain"
    "Help-screen exit label is the third"
    "remaining 478 base and 35 JA25 definitions")
  string(FIND "${runtime_i18n_todo_contents}"
    "${required_runtime_i18n_todo_fragment}"
    required_runtime_i18n_todo_position)
  if(required_runtime_i18n_todo_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime i18n migration TODO lost '${required_runtime_i18n_todo_fragment}'")
  endif()
endforeach()

file(GLOB_RECURSE core_files
  "${SOURCE_ROOT}/Engine/Core/*.h"
  "${SOURCE_ROOT}/Engine/Core/*.hpp"
  "${SOURCE_ROOT}/Engine/Core/*.cpp")

# Keep this explicit. A permissive "angle brackets mean standard library"
# rule also admits SDL3, platform SDKs, and upper engine/game layers.
set(core_standard_headers
  algorithm
  array
  atomic
  charconv
  chrono
  cmath
  cstddef
  cstdint
  cstdio
  cstring
  deque
  exception
  functional
  initializer_list
  iterator
  limits
  mutex
  new
  optional
  string
  string_view
  stdexcept
  system_error
  type_traits
  unordered_map
  unordered_set
  utility
  variant
  vector)

foreach(core_file IN LISTS core_files)
  file(READ "${core_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/Core/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    if(standard_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Core has a forbidden dependency '${header}' in ${core_file}")
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE legacy_adapter_files
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.hpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/*.cpp")

# This is the only engine layer allowed to bridge into the existing runtime.
# Keep the compatibility surface named and deliberately small so migration of
# a service cannot quietly pull unrelated game systems into Engine.
set(legacy_compatibility_headers
  FileMan.h
  expat.h
  soundman.h
  video.h
  vsurface.h
  SDL3/SDL_log.h)

foreach(adapter_file IN LISTS legacy_adapter_files)
  file(READ "${adapter_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/(Core|Adapters/Legacy)/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    list(FIND legacy_compatibility_headers "${header}" compatibility_header_index)
    if(standard_header_index EQUAL -1 AND compatibility_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Adapters/Legacy has a forbidden dependency '${header}' in ${adapter_file}")
    endif()
  endforeach()
endforeach()

# Keep the low-level SDL video surface narrower than the Legacy adapter layer:
# presentation and invalidation implementations may consume it, gateways may
# not. This makes a new raw frame-output dependency an explicit architecture
# change instead of an incidental include.
set(platform_video_adapter_consumers
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformDepthBufferBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoObjectBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformVideoSurfaceBackend.h"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformFramePresenter.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformFrameInvalidator.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformRenderCommands.cpp"
  "${SOURCE_ROOT}/Engine/Adapters/Legacy/PlatformRenderSurfaceAccess.cpp")
foreach(adapter_file IN LISTS legacy_adapter_files)
  list(FIND platform_video_adapter_consumers
    "${adapter_file}" platform_video_consumer_index)
  if(NOT platform_video_consumer_index EQUAL -1)
    continue()
  endif()
  file(READ "${adapter_file}" contents)
  string(REGEX MATCH
    "Platform(DepthBuffer|Video(Surface|Object)?)Backend\\.h|(^|[^A-Za-z0-9_])Platform(DepthBuffer(Describe|Map|Unmap)|Video(Present|Invalidate|MarkFrameChanged|Surface|ObjectDraw|ObjectOutline)[A-Za-z0-9_]*)[ \t\r\n]*\\("
    direct_platform_video_access "${contents}")
  if(direct_platform_video_access)
    message(FATAL_ERROR
      "Legacy gateway bypasses the engine frame contracts in ${adapter_file}; keep raw SDL access in the platform frame adapters")
  endif()
endforeach()

file(GLOB_RECURSE ja2_adapter_files
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.h"
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.hpp"
  "${SOURCE_ROOT}/Engine/Adapters/JA2/*.cpp")

foreach(adapter_file IN LISTS ja2_adapter_files)
  file(READ "${adapter_file}" contents)
  string(REGEX MATCHALL "#[ \t]*include[ \t]*[<\"][^>\"\r\n]+[>\"]" includes "${contents}")
  foreach(include_line IN LISTS includes)
    string(REGEX REPLACE ".*[<\"]([^>\"]+)[>\"].*" "\\1" header "${include_line}")
    if(header MATCHES "^Engine/(Core|Adapters/JA2)/[A-Za-z0-9_./-]+$")
      continue()
    endif()
    list(FIND core_standard_headers "${header}" standard_header_index)
    if(standard_header_index EQUAL -1)
      message(FATAL_ERROR
        "Engine/Adapters/JA2 has a forbidden dependency '${header}' in ${adapter_file}")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/Engine/Adapters/JA2/CMakeLists.txt"
  ja2_runtime_adapter_build_contents)
foreach(required_roster_build_fragment IN ITEMS
    "TacticalEntityRoster.h"
    "TacticalEntityRoster.cpp")
  string(FIND "${ja2_runtime_adapter_build_contents}"
    "${required_roster_build_fragment}" required_roster_build_position)
  if(required_roster_build_position EQUAL -1)
    message(FATAL_ERROR
      "RuntimeAdapter no longer builds or installs ${required_roster_build_fragment}")
  endif()
endforeach()

file(READ
  "${SOURCE_ROOT}/Engine/Adapters/JA2/TacticalEntityRoster.h"
  tactical_entity_roster_header_contents)
foreach(required_roster_operation IN ITEMS
    "assign(Slot slot"
    "eraseAt(Slot slot"
    "compact()"
    "sortByIdentity()")
  string(FIND "${tactical_entity_roster_header_contents}"
    "${required_roster_operation}" required_roster_operation_position)
  if(required_roster_operation_position EQUAL -1)
    message(FATAL_ERROR
      "TacticalEntityRoster lost fixed-layout operation '${required_roster_operation}'")
  endif()
endforeach()

# Application package layers reach process-lifetime work through separate host
# ports, not one another's C++ interface. Campaigns may require rules through
# their manifest; a source include would recreate compile-time identity behind
# the runtime-selected package graph.
set(rules_package_files
  "${SOURCE_ROOT}/Ja2/RulesPackage.h"
  "${SOURCE_ROOT}/Ja2/RulesPackage.cpp")
foreach(package_file IN LISTS rules_package_files)
  file(READ "${package_file}" contents)
  string(REGEX MATCH
    "#[ \t]*include[ \t]*[<\"](CampaignPackage|CampaignRuntimeBootstrap)\\.h[>\"]"
    rules_campaign_dependency "${contents}")
  if(rules_campaign_dependency)
    message(FATAL_ERROR
      "The 1.13 rules package depends on campaign C++ identity in ${package_file}; use only RulesContentBootstrapHost")
  endif()
endforeach()

set(campaign_package_files
  "${SOURCE_ROOT}/Ja2/CampaignPackage.h"
  "${SOURCE_ROOT}/Ja2/CampaignPackage.cpp")
foreach(package_file IN LISTS campaign_package_files)
  file(READ "${package_file}" contents)
  string(REGEX MATCH
    "#[ \t]*include[ \t]*[<\"](RulesPackage|RulesContentBootstrap)\\.h[>\"]"
    campaign_rules_dependency "${contents}")
  if(campaign_rules_dependency)
    message(FATAL_ERROR
      "A campaign package depends on 1.13 rules C++ identity in ${package_file}; use only CampaignRuntimeBootstrapHost")
  endif()
endforeach()

foreach(retired_gameplay_runtime_file IN ITEMS
  "${SOURCE_ROOT}/Ja2/LegacyGameplayRuntime.h"
  "${SOURCE_ROOT}/Ja2/LegacyGameplayRuntime.cpp")
  if(EXISTS "${retired_gameplay_runtime_file}")
    message(FATAL_ERROR
      "Retired shared package runtime returned at ${retired_gameplay_runtime_file}; keep rules and campaign ownership separate")
  endif()
endforeach()

# Production callers must use the structured TryDispatch... result.  The
# sequence-returning wrappers remain part of the compatibility surface, but a
# zero sequence cannot distinguish rejection from a valid first command.
file(GLOB tactical_cpp_files "${SOURCE_ROOT}/Tactical/*.cpp")
set(legacy_tactical_dispatch_wrappers
  DispatchEndTurnCommandNow
  DispatchChangeStanceCommandNow
  DispatchBeginFireWeaponCommandNow
  DispatchMoveToGridCommandNow)

foreach(tactical_file IN LISTS tactical_cpp_files)
  if(tactical_file STREQUAL "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp")
    continue()
  endif()

  file(READ "${tactical_file}" contents)
  foreach(wrapper IN LISTS legacy_tactical_dispatch_wrappers)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${wrapper}[ \t\r\n]*\\("
      legacy_dispatch_call "${contents}")
    if(legacy_dispatch_call)
      message(FATAL_ERROR
        "Production code uses ambiguous compatibility wrapper '${wrapper}' in ${tactical_file}; use Try${wrapper} and inspect its structured result")
    endif()
  endforeach()
endforeach()

# Command queues must depend on the installed executor contract rather than
# calling the legacy game implementation directly. The headless replay uses
# the SDK's bounded executor so a second test-only battle model cannot drift
# away from production command semantics.
set(simulation_command_source
  "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp")
file(READ "${simulation_command_source}" simulation_command_contents)
foreach(required_executor_fragment IN ITEMS
    "class Ja2SimulationCommandExecutor final"
    "ApplicationSimulationCommandExecutor"
    "BindJa2SimulationCommandExecutor"
    "runtime.executeCommandsThrough"
    "runtime.executeExpectedCommandThrough")
  string(FIND "${simulation_command_contents}"
    "${required_executor_fragment}" required_executor_position)
  if(required_executor_position EQUAL -1)
    message(FATAL_ERROR
      "Production tactical command processing bypasses SimulationCommandExecutor; missing '${required_executor_fragment}'")
  endif()
endforeach()
foreach(retired_application_drain IN ITEMS
    "ProcessCommandsThrough"
    "ProcessExpectedNextCommandThrough")
  string(FIND "${simulation_command_contents}"
    "${retired_application_drain}" retired_application_drain_position)
  if(NOT retired_application_drain_position EQUAL -1)
    message(FATAL_ERROR
      "Application tactical code owns a parallel command drain '${retired_application_drain}'; use EngineRuntime execution")
  endif()
endforeach()

# The first compiled-campaign-identity slice deliberately makes the dedicated
# JA25 implementation part of every host. Reintroducing a JA2UB guard here
# would let an ordinary JA2 build pass while silently dropping the runtime
# campaign implementation. The selected startup callers likewise branch on
# GameCapabilities, not on which executable happened to compile them.
set(runtime_campaign_implementation_files
  "${SOURCE_ROOT}/Ja2/Ja25Update.cpp"
  "${SOURCE_ROOT}/Ja2/Ja25Update.h"
  "${SOURCE_ROOT}/Strategic/Ja25 Strategic Ai.cpp"
  "${SOURCE_ROOT}/Strategic/Ja25 Strategic Ai.h"
  "${SOURCE_ROOT}/Tactical/Ja25_Tactical.cpp"
  "${SOURCE_ROOT}/Tactical/Ja25_Tactical.h")
set(runtime_campaign_selection_files
  "${SOURCE_ROOT}/Ja2/CampaignAimSitePolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignApplicationPolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignCivilianQuotePolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignDealerPolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignGunCommentPolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignLaptopCommunicationsPolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignMapScreenPolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignMercSitePolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignMercenaryPolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignProgressPolicy.h"
  "${SOURCE_ROOT}/Ja2/CampaignSpeckQuoteCodes.h"
  "${SOURCE_ROOT}/Ja2/CompiledGameplayBootstrap.cpp"
  "${SOURCE_ROOT}/Ja2/gameloop.cpp"
  "${SOURCE_ROOT}/Ja2/gamescreen.cpp"
  "${SOURCE_ROOT}/Ja2/Intro.cpp"
  "${SOURCE_ROOT}/Ja2/Init.cpp"
  "${SOURCE_ROOT}/Ja2/Loading Screen.cpp"
  "${SOURCE_ROOT}/Ja2/Loading Screen.h"
  "${SOURCE_ROOT}/Ja2/MainMenuScreen.cpp"
  "${SOURCE_ROOT}/Ja2/MPHostScreen.cpp"
  "${SOURCE_ROOT}/Ja2/MPScoreScreen.cpp"
  "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  "${SOURCE_ROOT}/Ja2/SaveLoadScreen.cpp"
  "${SOURCE_ROOT}/Ja2/ub_config.cpp"
  "${SOURCE_ROOT}/Ja2/ub_config.h"
  "${SOURCE_ROOT}/Ja2/CampaignActionCodes.h"
  "${SOURCE_ROOT}/Ja2/CampaignMapChangeCodes.h"
  "${SOURCE_ROOT}/Ja2/CampaignProfileCodes.h"
  "${SOURCE_ROOT}/Laptop/Speck Quotes.h"
  "${SOURCE_ROOT}/Laptop/AimLinks.cpp"
  "${SOURCE_ROOT}/Laptop/AimMembers.cpp"
  "${SOURCE_ROOT}/Laptop/email.cpp"
  "${SOURCE_ROOT}/Laptop/insurance Contract.cpp"
  "${SOURCE_ROOT}/Laptop/laptop.cpp"
  "${SOURCE_ROOT}/Laptop/laptop.h"
  "${SOURCE_ROOT}/Laptop/mercs Account.cpp"
  "${SOURCE_ROOT}/Laptop/mercs Files.cpp"
  "${SOURCE_ROOT}/Laptop/mercs No Account.cpp"
  "${SOURCE_ROOT}/Laptop/mercs.cpp"
  "${SOURCE_ROOT}/Laptop/mercs.h"
  "${SOURCE_ROOT}/Laptop/personnel.cpp"
  "${SOURCE_ROOT}/Laptop/PostalService.cpp"
  "${SOURCE_ROOT}/Strategic/Campaign Init.cpp"
  "${SOURCE_ROOT}/Strategic/Campaign Types.h"
  "${SOURCE_ROOT}/Strategic/Game Init.cpp"
  "${SOURCE_ROOT}/Strategic/Game Event Hook.cpp"
  "${SOURCE_ROOT}/Strategic/Hourly Update.cpp"
  "${SOURCE_ROOT}/Strategic/LuaInitNPCs.h"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Bottom.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Bottom.h"
  "${SOURCE_ROOT}/Strategic/MapScreen Quotes.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  "${SOURCE_ROOT}/Strategic/Player Command.cpp"
  "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  "${SOURCE_ROOT}/Strategic/Quests.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Event Handler.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Merc Handler.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Town Loyalty.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.h"
  "${SOURCE_ROOT}/Tactical/Action Items.h"
  "${SOURCE_ROOT}/Tactical/Arms Dealer Init.cpp"
  "${SOURCE_ROOT}/Tactical/Arms Dealer Init.h"
  "${SOURCE_ROOT}/Tactical/ArmsDealerInvInit.cpp"
  "${SOURCE_ROOT}/Tactical/ArmsDealerInvInit.h"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.h"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.h"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Control.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/Items.cpp"
  "${SOURCE_ROOT}/Tactical/Merc Entering.cpp"
  "${SOURCE_ROOT}/Tactical/Merc Hiring.cpp"
  "${SOURCE_ROOT}/Tactical/Merc Hiring.h"
  "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Control.h"
  "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Profile.h"
  "${SOURCE_ROOT}/Tactical/Soldier Profile.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Quotes.h"
  "${SOURCE_ROOT}/Tactical/TacticalActor.h"
  "${SOURCE_ROOT}/Tactical/Tactical Save.cpp"
  "${SOURCE_ROOT}/Tactical/Tactical Turns.cpp"
  "${SOURCE_ROOT}/Tactical/XML.h"
  "${SOURCE_ROOT}/Tactical/opplist.cpp"
  "${SOURCE_ROOT}/Tactical/opplist.h"
  "${SOURCE_ROOT}/Tactical/interface Dialogue.h"
  "${SOURCE_ROOT}/TacticalAI/NPC.h"
  "${SOURCE_ROOT}/TileEngine/Explosion Control.cpp"
  "${SOURCE_ROOT}/TileEngine/Explosion Control.h"
  "${SOURCE_ROOT}/TileEngine/SaveLoadMap.cpp"
  "${SOURCE_ROOT}/TileEngine/SaveLoadMap.h")
foreach(runtime_campaign_file IN LISTS
    runtime_campaign_implementation_files runtime_campaign_selection_files)
  file(READ "${runtime_campaign_file}" runtime_campaign_contents)
  string(FIND "${runtime_campaign_contents}" "JA2UB"
    runtime_campaign_identity_hint)
  if(NOT runtime_campaign_identity_hint EQUAL -1)
    string(REGEX MATCH
      "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
      compiled_campaign_identity "${runtime_campaign_contents}")
    if(compiled_campaign_identity)
      message(FATAL_ERROR
        "Runtime campaign code regained compiled JA2UB identity in ${runtime_campaign_file}")
    endif()
  endif()
endforeach()

# The application shell now makes content-load, tactical-loop, arrival, and
# loading-screen decisions through one value-only campaign policy. Both data
# sets and their storage contracts must stay compiled into each host. Dealer
# identities are now typed while their legacy persisted slots remain stable.
file(READ "${SOURCE_ROOT}/Ja2/CampaignApplicationPolicy.h"
  runtime_campaign_application_policy_contents)
foreach(required_application_policy_fragment IN ITEMS
    "class CampaignApplicationPolicy"
    "usesLocalizedArulcoMercData"
    "usesArulcoMerchantRoster"
    "hasMeanwhileScenes"
    "runsUnfinishedBusinessTacticalHooks"
    "usesUnfinishedBusinessUndergroundLoadScreens"
    "shouldLoadExternalEnemyDeployment")
  string(FIND "${runtime_campaign_application_policy_contents}"
    "${required_application_policy_fragment}"
    required_application_policy_position)
  if(required_application_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Application campaign policy lost '${required_application_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/Init.cpp"
  runtime_campaign_content_load_contents)
foreach(required_campaign_content_fragment IN ITEMS
    "CampaignApplicationPolicy campaignPolicy"
    "MERCSTARTINGGEAR25FILENAME"
    "MERCSTARTINGGEARFILENAME"
    "BETTYINVENTORYFILENAME"
    "DEVININVENTORYFILENAME"
    "MERCPROFILESFILENAME25"
    "MERCPROFILESFILENAME"
    "shouldLoadExternalEnemyDeployment")
  string(FIND "${runtime_campaign_content_load_contents}"
    "${required_campaign_content_fragment}"
    required_campaign_content_position)
  if(required_campaign_content_position EQUAL -1)
    message(FATAL_ERROR
      "Common content loading lost runtime campaign selection '${required_campaign_content_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/gamescreen.cpp"
  runtime_campaign_tactical_shell_contents)
foreach(required_campaign_tactical_fragment IN ITEMS
    "CampaignApplicationPolicy campaignPolicy"
    "hasMeanwhileScenes"
    "runsUnfinishedBusinessTacticalHooks"
    "JA25_HandleUpdateOfStrategicAi"
    "HandlePowerGenAlarm"
    "HandleInitialEventsInHeliCrash")
  string(FIND "${runtime_campaign_tactical_shell_contents}"
    "${required_campaign_tactical_fragment}"
    required_campaign_tactical_position)
  if(required_campaign_tactical_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical application shell lost runtime campaign selection '${required_campaign_tactical_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/Loading Screen.cpp"
  runtime_campaign_loadscreen_contents)
foreach(required_campaign_loadscreen_fragment IN ITEMS
    "usesUnfinishedBusinessUndergroundLoadScreens"
    "LOADINGSCREEN_TUNNELS"
    "LOADINGSCREEN_COMPLEX_BASEMENT"
    "LOADINGSCREEN_MINE"
    "LOADINGSCREEN_CAVE")
  string(FIND "${runtime_campaign_loadscreen_contents}"
    "${required_campaign_loadscreen_fragment}"
    required_campaign_loadscreen_position)
  if(required_campaign_loadscreen_position EQUAL -1)
    message(FATAL_ERROR
      "Loading-screen selection lost campaign path '${required_campaign_loadscreen_fragment}'")
  endif()
endforeach()

foreach(common_campaign_storage_file IN ITEMS
    "${SOURCE_ROOT}/Tactical/ArmsDealerInvInit.cpp"
    "${SOURCE_ROOT}/Tactical/ArmsDealerInvInit.h")
  file(READ "${common_campaign_storage_file}"
    common_campaign_storage_contents)
  foreach(required_campaign_storage_fragment IN ITEMS
      "gBettyInventory"
      "gDevinInventory"
      "gRaulInventory"
      "gPerkoInventory")
    string(FIND "${common_campaign_storage_contents}"
      "${required_campaign_storage_fragment}"
      required_campaign_storage_position)
    if(required_campaign_storage_position EQUAL -1)
      message(FATAL_ERROR
        "Common campaign inventory storage lost '${required_campaign_storage_fragment}' in ${common_campaign_storage_file}")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/tests/CMakeLists.txt"
  runtime_campaign_policy_test_build_contents)
foreach(required_campaign_policy_test_fragment IN ITEMS
    "campaign_application_policy_tests.cpp"
    "campaign_application_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_campaign_policy_test_fragment}"
    required_campaign_policy_test_position)
  if(required_campaign_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime application campaign policy lost its headless test target")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/.github/workflows/build_unix.yml"
  runtime_campaign_policy_ci_contents)
string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_application_policy_tests"
  runtime_campaign_policy_ci_position)
if(runtime_campaign_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime application campaign policy test target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_application_policy_tests.cpp"
  runtime_campaign_policy_test_contents)
foreach(required_campaign_policy_assertion IN ITEMS
    "GameCampaign::UnfinishedBusiness"
    "usesLocalizedArulcoMercData"
    "usesArulcoMerchantRoster"
    "hasMeanwhileScenes"
    "runsUnfinishedBusinessTacticalHooks"
    "usesUnfinishedBusinessUndergroundLoadScreens"
    "shouldLoadExternalEnemyDeployment(false)"
    "shouldLoadExternalEnemyDeployment(true)")
  string(FIND "${runtime_campaign_policy_test_contents}"
    "${required_campaign_policy_assertion}"
    required_campaign_policy_assertion_position)
  if(required_campaign_policy_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime application campaign policy lost test coverage for '${required_campaign_policy_assertion}'")
  endif()
endforeach()

# Tactical dialogue, animation, melee, and radar meanwhile behavior follows the
# live application capability. The former eight JA2UB guards must stay retired,
# and every converted legacy scene probe must remain behind the campaign gate.
string(REGEX REPLACE "#[^\r\n]*" ""
  runtime_campaign_meanwhile_test_build_executable
  "${runtime_campaign_policy_test_build_contents}")
foreach(required_campaign_meanwhile_test_manifest_fragment IN ITEMS
    "add_executable(campaign_meanwhile_policy_tests"
    "campaign_meanwhile_policy_tests.cpp"
    "add_test(NAME campaign_meanwhile_policy COMMAND campaign_meanwhile_policy_tests)")
  string(FIND "${runtime_campaign_meanwhile_test_build_executable}"
    "${required_campaign_meanwhile_test_manifest_fragment}"
    required_campaign_meanwhile_test_manifest_position)
  if(required_campaign_meanwhile_test_manifest_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign meanwhile policy lost its data-free truth-table target")
  endif()
endforeach()
string(REGEX REPLACE "#[^\r\n]*" ""
  runtime_campaign_meanwhile_ci_executable
  "${runtime_campaign_policy_ci_contents}")
if(NOT runtime_campaign_meanwhile_ci_executable MATCHES
    "cmake --build build --target[^\r\n]*campaign_meanwhile_policy_tests")
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime campaign meanwhile policy target")
endif()

function(read_campaign_meanwhile_executable relative_source output_variable)
  file(READ "${SOURCE_ROOT}/${relative_source}"
    runtime_campaign_meanwhile_source_contents)
  # Ignore documentation while retaining legacy `//*` line comments; the
  # latter must not be mistaken for an unterminated block comment.
  string(REGEX REPLACE "(^|[^/])/\\*([^*]|\\*+[^*/])*\\*+/" "\\1"
    runtime_campaign_meanwhile_source_contents
    "${runtime_campaign_meanwhile_source_contents}")
  string(REGEX REPLACE "//[^\r\n]*" ""
    runtime_campaign_meanwhile_source_executable
    "${runtime_campaign_meanwhile_source_contents}")
  set(${output_variable} "${runtime_campaign_meanwhile_source_executable}"
    PARENT_SCOPE)
endfunction()

foreach(runtime_campaign_meanwhile_source_spec IN ITEMS
    "Tactical/DynamicDialogue.cpp|1|1"
    "Tactical/Soldier Ani.cpp|2|3"
    "Tactical/Weapons.cpp|1|4"
    "TileEngine/Radar Screen.cpp|1|2")
  string(REPLACE "|" ";" runtime_campaign_meanwhile_source_parts
    "${runtime_campaign_meanwhile_source_spec}")
  list(GET runtime_campaign_meanwhile_source_parts 0
    runtime_campaign_meanwhile_source)
  list(GET runtime_campaign_meanwhile_source_parts 1
    expected_campaign_meanwhile_policy_count)
  list(GET runtime_campaign_meanwhile_source_parts 2
    expected_campaign_meanwhile_probe_count)
  read_campaign_meanwhile_executable(
    "${runtime_campaign_meanwhile_source}"
    runtime_campaign_meanwhile_source_executable)
  string(REGEX REPLACE "[ \t\r\n]+" " "
    runtime_campaign_meanwhile_source_normalized
    "${runtime_campaign_meanwhile_source_executable}")
  foreach(required_campaign_meanwhile_common_fragment IN ITEMS
      "#include \"CampaignApplicationPolicy.h\""
      "CampaignApplicationPolicy campaignPolicy( GetGameContext().capabilities());"
      "campaignPolicy.hasMeanwhileScenes()")
    string(FIND "${runtime_campaign_meanwhile_source_normalized}"
      "${required_campaign_meanwhile_common_fragment}"
      required_campaign_meanwhile_common_position)
    if(required_campaign_meanwhile_common_position EQUAL -1)
      message(FATAL_ERROR
        "Runtime meanwhile integration lost '${required_campaign_meanwhile_common_fragment}' in ${runtime_campaign_meanwhile_source}")
    endif()
  endforeach()
  string(REGEX MATCH
      "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
    retired_campaign_meanwhile_guard
    "${runtime_campaign_meanwhile_source_executable}")
  if(retired_campaign_meanwhile_guard)
    message(FATAL_ERROR
      "Tactical meanwhile behavior regained compiled JA2UB identity in ${runtime_campaign_meanwhile_source}")
  endif()

  string(REGEX MATCHALL
    "CampaignApplicationPolicy campaignPolicy\\( GetGameContext\\(\\)\\.capabilities\\(\\)\\)"
    runtime_campaign_meanwhile_policy_constructions
    "${runtime_campaign_meanwhile_source_normalized}")
  list(LENGTH runtime_campaign_meanwhile_policy_constructions
    runtime_campaign_meanwhile_policy_count)
  if(NOT runtime_campaign_meanwhile_policy_count EQUAL
      expected_campaign_meanwhile_policy_count)
    message(FATAL_ERROR
      "${runtime_campaign_meanwhile_source} must source exactly ${expected_campaign_meanwhile_policy_count} meanwhile policy instance(s) from the immutable GameContext capability")
  endif()

  string(REGEX MATCHALL
    "(^|[^A-Za-z0-9_])AreInMeanwhile[ \t\r\n]*\\("
    runtime_campaign_meanwhile_probes
    "${runtime_campaign_meanwhile_source_executable}")
  list(LENGTH runtime_campaign_meanwhile_probes
    runtime_campaign_meanwhile_probe_count)
  if(NOT runtime_campaign_meanwhile_probe_count EQUAL
      expected_campaign_meanwhile_probe_count)
    message(FATAL_ERROR
      "${runtime_campaign_meanwhile_source} changed its active meanwhile-probe inventory; preserve the converted gates or review the new call explicitly")
  endif()
endforeach()

read_campaign_meanwhile_executable("Tactical/DynamicDialogue.cpp"
  runtime_campaign_meanwhile_dialogue_contents)
read_campaign_meanwhile_executable("Tactical/Soldier Ani.cpp"
  runtime_campaign_meanwhile_animation_contents)
read_campaign_meanwhile_executable("Tactical/Weapons.cpp"
  runtime_campaign_meanwhile_weapons_contents)
read_campaign_meanwhile_executable("TileEngine/Radar Screen.cpp"
  runtime_campaign_meanwhile_radar_contents)
foreach(runtime_campaign_meanwhile_code IN ITEMS
    dialogue animation weapons radar)
  string(REGEX REPLACE "[ \t\r\n]+" " "
    runtime_campaign_meanwhile_${runtime_campaign_meanwhile_code}_contents
    "${runtime_campaign_meanwhile_${runtime_campaign_meanwhile_code}_contents}")
endforeach()
foreach(required_campaign_meanwhile_dialogue_fragment IN ITEMS
    "CampaignApplicationPolicy campaignPolicy( GetGameContext().capabilities());"
    "if ( campaignPolicy.hasMeanwhileScenes() && AreInMeanwhile( ) )")
  string(FIND "${runtime_campaign_meanwhile_dialogue_contents}"
    "${required_campaign_meanwhile_dialogue_fragment}"
    required_campaign_meanwhile_dialogue_position)
  if(required_campaign_meanwhile_dialogue_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime dynamic-dialogue meanwhile gate lost '${required_campaign_meanwhile_dialogue_fragment}'")
  endif()
endforeach()
foreach(required_campaign_meanwhile_animation_fragment IN ITEMS
    "if ( fMartialArtist && ( !campaignPolicy.hasMeanwhileScenes() || !AreInMeanwhile( ) ) )"
    "if ( campaignPolicy.hasMeanwhileScenes() && AreInMeanwhile( ) )"
    "if ( !campaignPolicy.hasMeanwhileScenes() || !AreInMeanwhile() )")
  string(FIND "${runtime_campaign_meanwhile_animation_contents}"
    "${required_campaign_meanwhile_animation_fragment}"
    required_campaign_meanwhile_animation_position)
  if(required_campaign_meanwhile_animation_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime animation meanwhile gate lost '${required_campaign_meanwhile_animation_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_meanwhile_weapons_contents}"
  "if ( iDiceRoll < iHitChance || ( campaignPolicy.hasMeanwhileScenes() && AreInMeanwhile( ) ) )"
  required_campaign_meanwhile_weapons_position)
if(required_campaign_meanwhile_weapons_position EQUAL -1)
  message(FATAL_ERROR
    "Runtime melee meanwhile forced-hit gate lost its left-hand campaign policy")
endif()
string(FIND "${runtime_campaign_meanwhile_radar_contents}"
  "if( campaignPolicy.hasMeanwhileScenes() && AreInMeanwhile( ) == TRUE )"
  required_campaign_meanwhile_radar_position)
if(required_campaign_meanwhile_radar_position EQUAL -1)
  message(FATAL_ERROR
    "Runtime radar meanwhile suppression lost its left-hand campaign policy")
endif()

read_campaign_meanwhile_executable("tests/campaign_meanwhile_policy_tests.cpp"
  runtime_campaign_meanwhile_policy_test_contents)
foreach(required_campaign_meanwhile_policy_test_fragment IN ITEMS
    "Arulco editor retains the Arulco meanwhile policy"
    "UB editor retains the no-meanwhile policy"
    "Arulco follows the live meanwhile state exactly"
    "Converted UB scene gate never queries Arulco's meanwhile state"
    "Arulco suppresses the martial-artist idle only in a meanwhile"
    "Arulco martial idle preserves its left-hand short circuit"
    "UB martial idle never reads meanwhile state"
    "Arulco allows dialogue and death sounds outside a meanwhile"
    "UB dialogue and death sounds never read meanwhile state"
    "Arulco queen response and radar suppression follow the meanwhile"
    "UB queen response and radar never read meanwhile state"
    "Arulco meanwhile melee preserves its forced-hit rule"
    "Arulco melee preserves the ordinary-hit short circuit"
    "UB melee follows only the ordinary hit roll"
    "UB melee never reads meanwhile state")
  string(FIND "${runtime_campaign_meanwhile_policy_test_contents}"
    "${required_campaign_meanwhile_policy_test_fragment}"
    required_campaign_meanwhile_policy_test_position)
  if(required_campaign_meanwhile_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign meanwhile truth table lost '${required_campaign_meanwhile_policy_test_fragment}'")
  endif()
endforeach()

# Door forcing and tunnel feedback are campaign content selected from the live
# capability set. Both behavior paths must remain compiled in the shared
# tactical source, and the eight retired JA2UB guards must not return.
file(READ "${SOURCE_ROOT}/Ja2/CampaignDoorPolicy.h"
  runtime_campaign_door_policy_contents)
foreach(required_campaign_door_policy_fragment IN ITEMS
    "class CampaignDoorPolicy"
    "usesUnfinishedBusinessTunnelGate"
    "usesOpenDoorCostWhenForcing"
    "shouldAttemptForceDoor"
    "shouldAttemptDoorMenuAction"
    "shouldOfferFailedUnlockCurse")
  string(FIND "${runtime_campaign_door_policy_contents}"
    "${required_campaign_door_policy_fragment}"
    required_campaign_door_policy_position)
  if(required_campaign_door_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign door policy lost '${required_campaign_door_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Handle Doors.cpp"
  runtime_campaign_door_source_contents)
foreach(required_campaign_door_source_fragment IN ITEMS
    "#include \"CampaignDoorPolicy.h\""
    "CampaignDoorPolicy campaignPolicy("
    "campaignPolicy.usesOpenDoorCostWhenForcing()"
    "campaignPolicy.shouldAttemptForceDoor("
    "campaignPolicy.shouldOfferFailedUnlockCurse("
    "RecordForcedUnfinishedBusinessTunnelGate( sGridNo )")
  string(FIND "${runtime_campaign_door_source_contents}"
    "${required_campaign_door_source_fragment}"
    required_campaign_door_source_position)
  if(required_campaign_door_source_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign door integration lost '${required_campaign_door_source_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_door_source_contents}" "JA2UB"
  retired_campaign_door_guard_position)
if(NOT retired_campaign_door_guard_position EQUAL -1)
  message(FATAL_ERROR
    "Tactical door behavior regained compiled JA2UB identity")
endif()

foreach(required_campaign_door_test_manifest_fragment IN ITEMS
    "campaign_door_policy_tests.cpp"
    "campaign_door_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_campaign_door_test_manifest_fragment}"
    required_campaign_door_test_manifest_position)
  if(required_campaign_door_test_manifest_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign door policy lost its data-free test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_door_policy_tests"
  required_campaign_door_policy_ci_position)
if(required_campaign_door_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime campaign door policy target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_door_policy_tests.cpp"
  runtime_campaign_door_policy_test_contents)
foreach(required_campaign_door_policy_test_fragment IN ITEMS
    "Arulco has no UB tunnel-gate content"
    "UB forcing retains its open-door AP cost"
    "UB tunnel dialogue alone consumes a force attempt"
    "Arulco door-menu actions never invoke the UB tunnel gate"
    "UB tunnel dialogue consumes each intercepted door-menu action"
    "UB offers the curse only after its tunnel quote handles the failure")
  string(FIND "${runtime_campaign_door_policy_test_contents}"
    "${required_campaign_door_policy_test_fragment}"
    required_campaign_door_policy_test_position)
  if(required_campaign_door_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign door policy coverage lost '${required_campaign_door_policy_test_fragment}'")
  endif()
endforeach()

# Strategic-map guidance and campaign hooks are selected from the live
# capability set. Both paths must remain compiled in every host, and the
# twelve retired mapscreen JA2UB guards must not return.
file(READ "${SOURCE_ROOT}/Ja2/CampaignMapScreenPolicy.h"
  runtime_campaign_map_screen_policy_contents)
foreach(required_campaign_map_screen_policy_fragment IN ITEMS
    "class CampaignMapScreenPolicy"
    "usesUnfinishedBusinessMapRules"
    "usesJerryMiloGuidance"
    "shouldRebuildCustomMapList"
    "shouldPumpJerryMiloQuotes"
    "hasMeanwhileScenes"
    "treatsSanMonaAsUnimportant"
    "usesUnfinishedBusinessLossDialogue"
    "usesConfigurableMapBorderButtons"
    "shouldDisableAutoResolve"
    "runsUnfinishedBusinessStrategicAi"
    "shouldCheckHelicopterCampaignLoss")
  string(FIND "${runtime_campaign_map_screen_policy_contents}"
    "${required_campaign_map_screen_policy_fragment}"
    required_campaign_map_screen_policy_position)
  if(required_campaign_map_screen_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign map-screen policy lost '${required_campaign_map_screen_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  runtime_campaign_map_screen_source_contents)
foreach(required_campaign_map_screen_source_fragment IN ITEMS
    "#include \"CampaignMapScreenPolicy.h\""
    "CampaignMapScreenPolicy campaignPolicy("
    "campaignPolicy.usesUnfinishedBusinessMapRules() &&"
    "campaignPolicy.usesJerryMiloGuidance()"
    "campaignPolicy.shouldRebuildCustomMapList("
    "campaignPolicy.shouldPumpJerryMiloQuotes("
    "campaignPolicy.hasMeanwhileScenes()"
    "campaignPolicy.runsUnfinishedBusinessStrategicAi()"
    "campaignPolicy.shouldCheckHelicopterCampaignLoss("
    "HandleWhenPlayerHasNoMercsAndNoLaptop()")
  string(FIND "${runtime_campaign_map_screen_source_contents}"
    "${required_campaign_map_screen_source_fragment}"
    required_campaign_map_screen_source_position)
  if(required_campaign_map_screen_source_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign map-screen integration lost '${required_campaign_map_screen_source_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_map_screen_source_contents}" "JA2UB"
  retired_campaign_map_screen_guard_position)
if(NOT retired_campaign_map_screen_guard_position EQUAL -1)
  message(FATAL_ERROR
    "Strategic map-screen behavior regained compiled JA2UB identity")
endif()

foreach(required_campaign_map_screen_test_manifest_fragment IN ITEMS
    "campaign_map_screen_policy_tests.cpp"
    "campaign_map_screen_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_campaign_map_screen_test_manifest_fragment}"
    required_campaign_map_screen_test_manifest_position)
  if(required_campaign_map_screen_test_manifest_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign map-screen policy lost its data-free test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_map_screen_policy_tests"
  required_campaign_map_screen_policy_ci_position)
if(required_campaign_map_screen_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime campaign map-screen policy target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_map_screen_policy_tests.cpp"
  runtime_campaign_map_screen_policy_test_contents)
foreach(required_campaign_map_screen_policy_test_fragment IN ITEMS
    "only UB enables Jerry Milo map guidance"
    "only Arulco checks for meanwhile scenes on the map screen"
    "only Arulco suppresses the San Mona town-loss notification"
    "only UB plays its contested-town loss dialogue"
    "only UB applies its configurable map-border buttons"
    "UB disables auto-resolve exactly when configured off"
    "UB rebuilds custom maps exactly when requested"
    "UB pumps Jerry Milo quotes exactly when enabled"
    "UB checks campaign loss exactly after its helicopter crash")
  string(FIND "${runtime_campaign_map_screen_policy_test_contents}"
    "${required_campaign_map_screen_policy_test_fragment}"
    required_campaign_map_screen_policy_test_position)
  if(required_campaign_map_screen_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign map-screen policy coverage lost '${required_campaign_map_screen_policy_test_fragment}'")
  endif()
endforeach()

# Civilian dialogue catalogue selection, dedicated-group ranges, surrender
# completion, and UB's unavailable-record sentinel belong to the live campaign.
# Both source paths and both stable record sets must remain present in every
# host; the six retired JA2UB guards must not return.
file(READ "${SOURCE_ROOT}/Ja2/CampaignCivilianQuotePolicy.h"
  runtime_campaign_civilian_quote_policy_contents)
foreach(required_campaign_civilian_quote_policy_fragment IN ITEMS
    "class CampaignCivilianQuotePolicy"
    "usesUnfinishedBusinessQuoteCatalogue"
    "completesSurrenderOfferAfterQuote"
    "civilianGroupQuoteBoundary"
    "discardsUnavailableQuote")
  string(FIND "${runtime_campaign_civilian_quote_policy_contents}"
    "${required_campaign_civilian_quote_policy_fragment}"
    required_campaign_civilian_quote_policy_position)
  if(required_campaign_civilian_quote_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime civilian quote policy lost '${required_campaign_civilian_quote_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Civ Quotes.cpp"
  runtime_campaign_civilian_quote_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Civ Quotes.h"
  runtime_campaign_civilian_quote_header_contents)
foreach(required_campaign_civilian_quote_source_fragment IN ITEMS
    "#include \"CampaignCivilianQuotePolicy.h\""
    "CampaignCivilianQuotePolicy campaignPolicy("
    "campaignPolicy.completesSurrenderOfferAfterQuote()"
    "campaignPolicy.civilianGroupQuoteBoundary("
    "campaignPolicy.usesUnfinishedBusinessQuoteCatalogue()"
    "campaignPolicy.discardsUnavailableQuote(ubCivQuoteID)")
  string(FIND "${runtime_campaign_civilian_quote_source_contents}"
    "${required_campaign_civilian_quote_source_fragment}"
    required_campaign_civilian_quote_source_position)
  if(required_campaign_civilian_quote_source_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime civilian quote integration lost '${required_campaign_civilian_quote_source_fragment}'")
  endif()
endforeach()
foreach(required_campaign_civilian_quote_header_fragment IN ITEMS
    "CIV_QUOTE__CIV_ENEMY_CAN_FIGHT"
    "CIV_QUOTE_NEW_UNFINISHED_BUSINESS"
    "CIV_QUOTE_NEW_ARULCO"
    "CIV_QUOTE_NEW_UNFINISHED_BUSINESS == 1029"
    "CIV_QUOTE_NEW_ARULCO == 1039")
  string(FIND "${runtime_campaign_civilian_quote_header_contents}"
    "${required_campaign_civilian_quote_header_fragment}"
    required_campaign_civilian_quote_header_position)
  if(required_campaign_civilian_quote_header_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime civilian quote records lost '${required_campaign_civilian_quote_header_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_civilian_quote_header_contents}"
  "CIV_QUOTE_NEW =" retired_ambiguous_civilian_quote_marker_position)
if(NOT retired_ambiguous_civilian_quote_marker_position EQUAL -1)
  message(FATAL_ERROR
    "Civilian quotes regained the campaign-ambiguous CIV_QUOTE_NEW marker")
endif()
foreach(runtime_campaign_civilian_quote_file_contents IN ITEMS
    runtime_campaign_civilian_quote_source_contents
    runtime_campaign_civilian_quote_header_contents)
  string(REGEX MATCH
    "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
    retired_campaign_civilian_quote_guard
    "${${runtime_campaign_civilian_quote_file_contents}}")
  if(retired_campaign_civilian_quote_guard)
    message(FATAL_ERROR
      "Civilian quote behavior regained compiled JA2UB identity")
  endif()
endforeach()

foreach(required_campaign_civilian_quote_test_manifest_fragment IN ITEMS
    "campaign_civilian_quote_policy_tests.cpp"
    "campaign_civilian_quote_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_campaign_civilian_quote_test_manifest_fragment}"
    required_campaign_civilian_quote_test_manifest_position)
  if(required_campaign_civilian_quote_test_manifest_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime civilian quote policy lost its data-free test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_civilian_quote_policy_tests"
  required_campaign_civilian_quote_policy_ci_position)
if(required_campaign_civilian_quote_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime civilian quote policy target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_civilian_quote_policy_tests.cpp"
  runtime_campaign_civilian_quote_policy_test_contents)
foreach(required_campaign_civilian_quote_policy_test_fragment IN ITEMS
    "only UB selects the extended civilian quote catalogue"
    "only Arulco completes surrender after closing the quote"
    "each campaign retains its exact dedicated-group boundary"
    "ordinary quote identifiers remain available in both campaigns"
    "only UB discards its unavailable quote sentinel")
  string(FIND "${runtime_campaign_civilian_quote_policy_test_contents}"
    "${required_campaign_civilian_quote_policy_test_fragment}"
    required_campaign_civilian_quote_policy_test_position)
  if(required_campaign_civilian_quote_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime civilian quote policy coverage lost '${required_campaign_civilian_quote_policy_test_fragment}'")
  endif()
endforeach()

# The tactical door menu, merc dismissal, and adjoining strategic map shell
# are one behaviorally closed campaign-policy follow-through. The live
# capability must be evaluated before every UB-only callback or configuration
# global, and none of these callers may regain executable identity.
file(READ "${SOURCE_ROOT}/Tactical/Interface.cpp"
  runtime_campaign_door_menu_contents)
foreach(required_campaign_door_menu_fragment IN ITEMS
    "#include \"CampaignDoorPolicy.h\""
    "CampaignDoorPolicy campaignPolicy("
    "campaignPolicy.usesUnfinishedBusinessTunnelGate() &&"
    "campaignPolicy.shouldAttemptDoorMenuAction("
    "campaignPolicy.usesOpenDoorCostWhenForcing()"
    "HandlePlayerSayingQuoteWhenFailingToOpenGateInTunnel("
    "GetAPsToOpenDoor("
    "GetAPsToBombDoor(")
  string(FIND "${runtime_campaign_door_menu_contents}"
    "${required_campaign_door_menu_fragment}"
    required_campaign_door_menu_position)
  if(required_campaign_door_menu_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime tactical door menu lost '${required_campaign_door_menu_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  runtime_campaign_dismissal_contents)
foreach(required_campaign_dismissal_fragment IN ITEMS
    "#include \"Ja25_Tactical.h\""
    "CampaignMercenaryPolicy campaignPolicy("
    "campaignPolicy.allowsDismissalFromSector("
    "campaignPolicy.dismissalRefusalQuote("
    "IsSoldierQualifiedMerc( pSoldier )"
    "DismissalRefusalQuote::AnsweringMachine"
    "QUOTE_REFUSING_ORDER")
  string(FIND "${runtime_campaign_dismissal_contents}"
    "${required_campaign_dismissal_fragment}"
    required_campaign_dismissal_position)
  if(required_campaign_dismissal_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime merc dismissal lost '${required_campaign_dismissal_fragment}'")
  endif()
endforeach()
foreach(retired_campaign_dismissal_helper IN ITEMS
    "CanMercBeAllowedToLeaveTeam"
    "HaveMercSayWhyHeWontLeave")
  string(FIND "${runtime_campaign_dismissal_contents}"
    "${retired_campaign_dismissal_helper}"
    retired_campaign_dismissal_helper_position)
  if(NOT retired_campaign_dismissal_helper_position EQUAL -1)
    message(FATAL_ERROR
      "Merc dismissal regained legacy helper '${retired_campaign_dismissal_helper}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  runtime_campaign_map_shell_interface_contents)
foreach(required_campaign_map_shell_interface_fragment IN ITEMS
    "CampaignLaptopCommunicationsPolicy communicationsPolicy("
    "communicationsPolicy.sendsInitialArulcoCongratulations()"
    "CampaignMapScreenPolicy campaignPolicy("
    "campaignPolicy.treatsSanMonaAsUnimportant() &&"
    "campaignPolicy.usesUnfinishedBusinessLossDialogue()"
    "HandleDisplayingOfPlayerLostDialogue( )")
  string(FIND "${runtime_campaign_map_shell_interface_contents}"
    "${required_campaign_map_shell_interface_fragment}"
    required_campaign_map_shell_interface_position)
  if(required_campaign_map_shell_interface_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime map-shell interface lost '${required_campaign_map_shell_interface_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Border.cpp"
  runtime_campaign_map_shell_border_contents)
foreach(required_campaign_map_shell_border_fragment IN ITEMS
    "CampaignMapScreenPolicy campaignPolicy("
    "campaignPolicy.usesConfigurableMapBorderButtons()"
    "gGameUBOptions.BorderTown"
    "gGameUBOptions.BorderMine"
    "gGameUBOptions.BorderTeams"
    "gGameUBOptions.BorderMilitia"
    "gGameUBOptions.BorderAirspace"
    "gGameUBOptions.BorderItem")
  string(FIND "${runtime_campaign_map_shell_border_contents}"
    "${required_campaign_map_shell_border_fragment}"
    required_campaign_map_shell_border_position)
  if(required_campaign_map_shell_border_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime map-border integration lost '${required_campaign_map_shell_border_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/PreBattle Interface.cpp"
  runtime_campaign_prebattle_contents)
foreach(required_campaign_prebattle_fragment IN ITEMS
    "CampaignMapScreenPolicy campaignPolicy("
    "campaignPolicy.usesUnfinishedBusinessMapRules() &&"
    "campaignPolicy.shouldDisableAutoResolve("
    "gGameUBOptions.AutoResolve != FALSE")
  string(FIND "${runtime_campaign_prebattle_contents}"
    "${required_campaign_prebattle_fragment}"
    required_campaign_prebattle_position)
  if(required_campaign_prebattle_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime pre-battle campaign policy lost '${required_campaign_prebattle_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Helicopter.cpp"
  runtime_campaign_map_helicopter_contents)
foreach(required_campaign_map_helicopter_fragment IN ITEMS
    "CampaignMapScreenPolicy campaignPolicy("
    "campaignPolicy.hasMeanwhileScenes()"
    "HandleKillChopperMeanwhileScene()")
  string(FIND "${runtime_campaign_map_helicopter_contents}"
    "${required_campaign_map_helicopter_fragment}"
    required_campaign_map_helicopter_position)
  if(required_campaign_map_helicopter_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime helicopter map policy lost '${required_campaign_map_helicopter_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.h"
  runtime_campaign_map_header_contents)
string(FIND "${runtime_campaign_map_header_contents}"
  "extern void SetUpValidCampaignSectors( void );"
  runtime_campaign_map_header_declaration_position)
if(runtime_campaign_map_header_declaration_position EQUAL -1)
  message(FATAL_ERROR
    "Campaign-sector setup declaration is no longer available to every host")
endif()

foreach(runtime_campaign_follow_through_contents IN ITEMS
    runtime_campaign_door_menu_contents
    runtime_campaign_dismissal_contents
    runtime_campaign_map_shell_interface_contents
    runtime_campaign_map_shell_border_contents
    runtime_campaign_prebattle_contents
    runtime_campaign_map_helicopter_contents
    runtime_campaign_map_header_contents)
  string(REGEX MATCH
    "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
    retired_campaign_follow_through_guard
    "${${runtime_campaign_follow_through_contents}}")
  if(retired_campaign_follow_through_guard)
    message(FATAL_ERROR
      "Campaign policy follow-through regained compiled JA2UB identity")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  runtime_campaign_follow_through_headless_contents)
foreach(required_campaign_follow_through_headless_fragment IN ITEMS
    "#include \"CampaignCivilianQuotePolicy.h\""
    "#include \"CampaignDoorPolicy.h\""
    "#include \"CampaignGunCommentPolicy.h\""
    "#include \"CampaignImpPolicy.h\""
    "#include \"CampaignLaptopCommunicationsPolicy.h\""
    "#include \"CampaignMapScreenPolicy.h\""
    "#include \"CampaignMercenaryPolicy.h\""
    "#include \"CampaignProgressPolicy.h\""
    "live campaign capabilities drive the tactical and strategic policy follow-throughs")
  string(FIND "${runtime_campaign_follow_through_headless_contents}"
    "${required_campaign_follow_through_headless_fragment}"
    required_campaign_follow_through_headless_position)
  if(required_campaign_follow_through_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Headless campaign-policy integration lost '${required_campaign_follow_through_headless_fragment}'")
  endif()
endforeach()

foreach(required_campaign_follow_through_ci_target IN ITEMS
    "ja2_headless_tests"
    "campaign_civilian_quote_policy_tests"
    "campaign_door_policy_tests"
    "campaign_gun_comment_policy_tests"
    "campaign_imp_policy_tests"
    "campaign_map_screen_policy_tests"
    "campaign_meanwhile_policy_tests"
    "campaign_mercenary_policy_tests"
    "campaign_progress_policy_tests"
    "laptop_communications_policy_tests")
  string(FIND "${runtime_campaign_policy_ci_contents}"
    "${required_campaign_follow_through_ci_target}"
    required_campaign_follow_through_ci_position)
  if(required_campaign_follow_through_ci_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign-policy CI union lost '${required_campaign_follow_through_ci_target}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/docs/CAMPAIGN_RUNTIME_STATUS.md"
  runtime_campaign_status_contents)
string(REGEX REPLACE "[ \t\r\n]+" " "
  runtime_campaign_status_normalized
  "${runtime_campaign_status_contents}")
foreach(required_campaign_status_fragment IN ITEMS
    "62 active conditionals in 24"
    "Laptop content/pages | 4"
    "Tactical gameplay/content | 18"
    "Strategic gameplay/content | 23"
    "CampaignDoorPolicy"
    "CampaignGunCommentPolicy"
    "CampaignCivilianQuotePolicy"
    "CampaignImpPolicy"
    "CampaignMapScreenPolicy"
    "CampaignProgressPolicy"
    "Tactical meanwhile-scene follow-through"
    "All eight former `JA2UB` guards across `DynamicDialogue.cpp`"
    "Merc dismissal in `Assignments.cpp`"
    "four converted map-shell"
    "All thirteen policies"
    "three remaining guards belong to separate dead-merc")
  string(FIND "${runtime_campaign_status_normalized}"
    "${required_campaign_status_fragment}"
    required_campaign_status_position)
  if(required_campaign_status_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign runtime status lost '${required_campaign_status_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/docs/ENGINE_ARCHITECTURE.md"
  runtime_campaign_architecture_contents)
string(REGEX REPLACE "[ \t\r\n]+" " "
  runtime_campaign_architecture_normalized
  "${runtime_campaign_architecture_contents}")
foreach(required_campaign_architecture_fragment IN ITEMS
    "Civilian tactical dialogue now selects through the value-only"
    "All six"
    "former `JA2UB` branches across `Civ Quotes.cpp`"
    "IMP activation and text selection now use the value-only"
    "84-common/14-variant Laptop partition"
    "Tactical door behavior now selects through the value-only"
    "The strategic map screen now selects campaign behavior"
    "Tactical meanwhile-scene follow-through now uses the same value-only"
    "Campaign progress and its scientist-AWOL threshold event now use the"
    "signed `INT8` strategic-sector lookup exactly"
    "Three former `JA2UB` guards are gone from `Tactical/Campaign.cpp`"
    "JA25 new-gun dialogue now uses the value-only"
    "All five former `JA2UB` guards in `Tactical/Handle Items.cpp`"
    "Arulco gates both routes before either comment argument"
    "every converted"
    "`AreInMeanwhile()` call behind a left-hand `hasMeanwhileScenes()` gate"
    "All five former guards in `Interface.cpp`"
    "All four former guards in `Assignments.cpp`"
    "nine former guards across the"
    "four shell implementations and map header")
  string(FIND "${runtime_campaign_architecture_normalized}"
    "${required_campaign_architecture_fragment}"
    required_campaign_architecture_position)
  if(required_campaign_architecture_position EQUAL -1)
    message(FATAL_ERROR
      "Engine architecture lost '${required_campaign_architecture_fragment}'")
  endif()
endforeach()

# Current progress and its scientist-AWOL threshold notification select from
# immutable capabilities. The UB-only strategic-AI probe must remain after the
# runtime gate, and neither converted function may regain compiled identity.
file(READ "${SOURCE_ROOT}/Ja2/CampaignProgressPolicy.h"
  runtime_campaign_progress_policy_contents)
foreach(required_campaign_progress_policy_fragment IN ITEMS
    "class CampaignProgressPolicy"
    "usesUnfinishedBusinessProgress"
    "std::int8_t furthestSectorPlayerOwns"
    "unfinishedBusinessProgress"
    "shouldStartScientistAwolMeanwhile"
    "currentProgress > previousHighestProgress"
    "return 50")
  string(FIND "${runtime_campaign_progress_policy_contents}"
    "${required_campaign_progress_policy_fragment}"
    required_campaign_progress_policy_position)
  if(required_campaign_progress_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign progress policy lost '${required_campaign_progress_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Campaign.cpp"
  runtime_campaign_progress_source_contents)
foreach(required_campaign_progress_source_fragment IN ITEMS
    "#include \"CampaignProgressPolicy.h\""
    "#include \"GameContext.h\""
    "#include \"Ja25 Strategic Ai.h\""
    "CampaignProgressPolicy progressPolicy("
    "if (progressPolicy.usesUnfinishedBusinessProgress())"
    "GetTheFurthestSectorPlayerOwns()"
    "progressPolicy.unfinishedBusinessProgress("
    "if ( !progressPolicy.usesUnfinishedBusinessProgress() &&"
    "progressPolicy.shouldStartScientistAwolMeanwhile("
    "HandleScientistAWOLMeanwhileScene()")
  string(FIND "${runtime_campaign_progress_source_contents}"
    "${required_campaign_progress_source_fragment}"
    required_campaign_progress_source_position)
  if(required_campaign_progress_source_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign progress routing lost '${required_campaign_progress_source_fragment}'")
  endif()
endforeach()

string(REGEX MATCHALL
  "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
  runtime_campaign_progress_remaining_guards
  "${runtime_campaign_progress_source_contents}")
list(LENGTH runtime_campaign_progress_remaining_guards
  runtime_campaign_progress_remaining_guard_count)
if(runtime_campaign_progress_remaining_guard_count GREATER 3)
  message(FATAL_ERROR
    "Tactical/Campaign.cpp regained a JA2UB guard after the progress extraction")
endif()

string(FIND "${runtime_campaign_progress_source_contents}"
  "UINT8 CurrentPlayerProgressPercentage(void)"
  runtime_campaign_current_progress_start)
string(FIND "${runtime_campaign_progress_source_contents}"
  "UINT8 HighestPlayerProgressPercentage(void)"
  runtime_campaign_current_progress_end)
if(runtime_campaign_current_progress_start EQUAL -1 OR
    runtime_campaign_current_progress_end EQUAL -1 OR
    NOT runtime_campaign_current_progress_start LESS
      runtime_campaign_current_progress_end)
  message(FATAL_ERROR "Cannot locate the runtime current-progress function")
endif()
math(EXPR runtime_campaign_current_progress_length
  "${runtime_campaign_current_progress_end} - ${runtime_campaign_current_progress_start}")
string(SUBSTRING "${runtime_campaign_progress_source_contents}"
  ${runtime_campaign_current_progress_start}
  ${runtime_campaign_current_progress_length}
  runtime_campaign_current_progress_region)

string(FIND "${runtime_campaign_current_progress_region}"
  "if (progressPolicy.usesUnfinishedBusinessProgress())"
  runtime_campaign_progress_gate_position)
string(FIND "${runtime_campaign_current_progress_region}"
  "GetTheFurthestSectorPlayerOwns()"
  runtime_campaign_progress_probe_position)
string(FIND "${runtime_campaign_current_progress_region}"
  "if( gfEditMode )"
  runtime_campaign_progress_editor_position)
if(runtime_campaign_progress_gate_position EQUAL -1 OR
    runtime_campaign_progress_probe_position EQUAL -1 OR
    runtime_campaign_progress_editor_position EQUAL -1 OR
    NOT runtime_campaign_progress_gate_position LESS
      runtime_campaign_progress_probe_position OR
    NOT runtime_campaign_progress_probe_position LESS
      runtime_campaign_progress_editor_position)
  message(FATAL_ERROR
    "UB strategic progress must be capability-gated before its probe and before Arulco editor suppression")
endif()
if(runtime_campaign_current_progress_region MATCHES
    "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB")
  message(FATAL_ERROR
    "CurrentPlayerProgressPercentage regained compiled campaign identity")
endif()

string(FIND "${runtime_campaign_progress_source_contents}"
  "void HourlyProgressUpdate(void)"
  runtime_campaign_hourly_progress_start)
string(FIND "${runtime_campaign_progress_source_contents}"
  "void TestDumpStatChanges(void)"
  runtime_campaign_hourly_progress_end)
if(runtime_campaign_hourly_progress_start EQUAL -1 OR
    runtime_campaign_hourly_progress_end EQUAL -1 OR
    NOT runtime_campaign_hourly_progress_start LESS
      runtime_campaign_hourly_progress_end)
  message(FATAL_ERROR "Cannot locate the runtime hourly-progress function")
endif()
math(EXPR runtime_campaign_hourly_progress_length
  "${runtime_campaign_hourly_progress_end} - ${runtime_campaign_hourly_progress_start}")
string(SUBSTRING "${runtime_campaign_progress_source_contents}"
  ${runtime_campaign_hourly_progress_start}
  ${runtime_campaign_hourly_progress_length}
  runtime_campaign_hourly_progress_region)
if(runtime_campaign_hourly_progress_region MATCHES
    "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB")
  message(FATAL_ERROR "HourlyProgressUpdate regained compiled campaign identity")
endif()

foreach(required_campaign_progress_test_build_fragment IN ITEMS
    "add_executable(campaign_progress_policy_tests"
    "campaign_progress_policy_tests.cpp"
    "add_test(NAME campaign_progress_policy COMMAND campaign_progress_policy_tests)")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_campaign_progress_test_build_fragment}"
    required_campaign_progress_test_build_position)
  if(required_campaign_progress_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign progress policy lost test manifest '${required_campaign_progress_test_build_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/tests/campaign_progress_policy_tests.cpp"
  runtime_campaign_progress_policy_test_contents)
foreach(required_campaign_progress_test_fragment IN ITEMS
    "Arulco short-circuits the UB-only strategic progress probe"
    "UB preserves every effective signed legacy progress-key result"
    "UB short-circuits Arulco's scientist-AWOL threshold read"
    "scientist-AWOL threshold truth table"
    "UB never starts Arulco's scientist-AWOL meanwhile scene")
  string(FIND "${runtime_campaign_progress_policy_test_contents}"
    "${required_campaign_progress_test_fragment}"
    required_campaign_progress_test_position)
  if(required_campaign_progress_test_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign progress policy tests lost '${required_campaign_progress_test_fragment}'")
  endif()
endforeach()

# JA25 new-gun dialogue is selected through one immutable campaign policy at
# both tactical item boundaries. Keep the policy to the left of the raw input
# checks/reads, preserve the exact legacy arguments, and keep both sources in
# every application host.
file(READ "${SOURCE_ROOT}/Ja2/CampaignGunCommentPolicy.h"
  runtime_campaign_gun_comment_policy_contents)
foreach(required_campaign_gun_comment_policy_fragment IN ITEMS
    "class CampaignGunCommentPolicy"
    "const GameCapabilities& capabilities"
    "usesUnfinishedBusinessGunComments"
    "campaign_ == GameCampaign::UnfinishedBusiness")
  string(FIND "${runtime_campaign_gun_comment_policy_contents}"
    "${required_campaign_gun_comment_policy_fragment}"
    required_campaign_gun_comment_policy_position)
  if(required_campaign_gun_comment_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime campaign gun-comment policy lost '${required_campaign_gun_comment_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  runtime_campaign_ground_gun_comment_contents)
file(READ "${SOURCE_ROOT}/Tactical/Items.cpp"
  runtime_campaign_inventory_gun_comment_contents)
foreach(required_campaign_ground_gun_comment_fragment IN ITEMS
    "#include \"CampaignGunCommentPolicy.h\""
    "#include \"GameContext.h\""
    "#include \"Ja25_Tactical.h\""
    "CampaignGunCommentPolicy gunCommentPolicy("
    "GetGameContext().capabilities()"
    "gunCommentPolicy.usesUnfinishedBusinessGunComments() &&"
    "iItemIndex != 0"
    "HandleNewGunComment( pSoldier, iItemIndex, TRUE )")
  string(FIND "${runtime_campaign_ground_gun_comment_contents}"
    "${required_campaign_ground_gun_comment_fragment}"
    required_campaign_ground_gun_comment_position)
  if(required_campaign_ground_gun_comment_position EQUAL -1)
    message(FATAL_ERROR
      "Ground-pickup gun-comment routing lost '${required_campaign_ground_gun_comment_fragment}'")
  endif()
endforeach()
foreach(required_campaign_inventory_gun_comment_fragment IN ITEMS
    "#include \"CampaignGunCommentPolicy.h\""
    "#include \"GameContext.h\""
    "#include \"Ja25_Tactical.h\""
    "CampaignGunCommentPolicy gunCommentPolicy("
    "GetGameContext().capabilities()"
    "if (gunCommentPolicy.usesUnfinishedBusinessGunComments())"
    "HandleNewGunComment( pSoldier, pObj->usItem, FALSE )")
  string(FIND "${runtime_campaign_inventory_gun_comment_contents}"
    "${required_campaign_inventory_gun_comment_fragment}"
    required_campaign_inventory_gun_comment_position)
  if(required_campaign_inventory_gun_comment_position EQUAL -1)
    message(FATAL_ERROR
      "Inventory-placement gun-comment routing lost '${required_campaign_inventory_gun_comment_fragment}'")
  endif()
endforeach()

foreach(runtime_campaign_gun_comment_source IN ITEMS
    runtime_campaign_ground_gun_comment_contents
    runtime_campaign_inventory_gun_comment_contents)
  if("${${runtime_campaign_gun_comment_source}}" MATCHES
      "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB")
    message(FATAL_ERROR
      "Tactical gun-comment routing regained compiled JA2UB identity")
  endif()
endforeach()

string(REGEX MATCHALL "HandleNewGunComment[ \t]*\\("
  runtime_campaign_ground_gun_comment_calls
  "${runtime_campaign_ground_gun_comment_contents}")
list(LENGTH runtime_campaign_ground_gun_comment_calls
  runtime_campaign_ground_gun_comment_call_count)
string(REGEX MATCHALL "HandleNewGunComment[ \t]*\\("
  runtime_campaign_inventory_gun_comment_calls
  "${runtime_campaign_inventory_gun_comment_contents}")
list(LENGTH runtime_campaign_inventory_gun_comment_calls
  runtime_campaign_inventory_gun_comment_call_count)
if(NOT runtime_campaign_ground_gun_comment_call_count EQUAL 1 OR
    NOT runtime_campaign_inventory_gun_comment_call_count EQUAL 1)
  message(FATAL_ERROR
    "Each tactical gun-comment boundary must retain exactly one legacy call")
endif()

string(FIND "${runtime_campaign_ground_gun_comment_contents}"
  "void SoldierGetItemFromWorld(" runtime_campaign_ground_comment_start)
string(FIND "${runtime_campaign_ground_gun_comment_contents}"
  "void HandleSoldierPickupItem(" runtime_campaign_ground_comment_end)
if(runtime_campaign_ground_comment_start EQUAL -1 OR
    runtime_campaign_ground_comment_end EQUAL -1 OR
    NOT runtime_campaign_ground_comment_start LESS
      runtime_campaign_ground_comment_end)
  message(FATAL_ERROR "Cannot locate the ground-pickup gun-comment boundary")
endif()
math(EXPR runtime_campaign_ground_comment_length
  "${runtime_campaign_ground_comment_end} - ${runtime_campaign_ground_comment_start}")
string(SUBSTRING "${runtime_campaign_ground_gun_comment_contents}"
  ${runtime_campaign_ground_comment_start}
  ${runtime_campaign_ground_comment_length}
  runtime_campaign_ground_comment_region)
string(FIND "${runtime_campaign_ground_comment_region}"
  "gunCommentPolicy.usesUnfinishedBusinessGunComments()"
  runtime_campaign_ground_comment_gate_position)
string(FIND "${runtime_campaign_ground_comment_region}"
  "iItemIndex != 0" runtime_campaign_ground_comment_nonzero_position)
string(FIND "${runtime_campaign_ground_comment_region}"
  "HandleNewGunComment( pSoldier, iItemIndex, TRUE )"
  runtime_campaign_ground_comment_call_position)
string(FIND "${runtime_campaign_ground_comment_region}"
  "// Aknowledge" runtime_campaign_ground_comment_ack_position)
if(runtime_campaign_ground_comment_gate_position EQUAL -1 OR
    runtime_campaign_ground_comment_nonzero_position EQUAL -1 OR
    runtime_campaign_ground_comment_call_position EQUAL -1 OR
    runtime_campaign_ground_comment_ack_position EQUAL -1 OR
    NOT runtime_campaign_ground_comment_gate_position LESS
      runtime_campaign_ground_comment_nonzero_position OR
    NOT runtime_campaign_ground_comment_nonzero_position LESS
      runtime_campaign_ground_comment_call_position OR
    NOT runtime_campaign_ground_comment_call_position LESS
      runtime_campaign_ground_comment_ack_position)
  message(FATAL_ERROR
    "Ground gun comments must gate campaign first, preserve raw nonzero input, and precede acknowledgement")
endif()

string(FIND "${runtime_campaign_inventory_gun_comment_contents}"
  "BOOLEAN PlaceObject( TacticalActor * pSoldier, INT8 bPos, OBJECTTYPE * pObj )"
  runtime_campaign_inventory_comment_start)
string(FIND "${runtime_campaign_inventory_gun_comment_contents}"
  "bool TryToStackInSlot(" runtime_campaign_inventory_comment_end)
if(runtime_campaign_inventory_comment_start EQUAL -1 OR
    runtime_campaign_inventory_comment_end EQUAL -1 OR
    NOT runtime_campaign_inventory_comment_start LESS
      runtime_campaign_inventory_comment_end)
  message(FATAL_ERROR
    "Cannot locate the inventory-placement gun-comment boundary")
endif()
math(EXPR runtime_campaign_inventory_comment_length
  "${runtime_campaign_inventory_comment_end} - ${runtime_campaign_inventory_comment_start}")
string(SUBSTRING "${runtime_campaign_inventory_gun_comment_contents}"
  ${runtime_campaign_inventory_comment_start}
  ${runtime_campaign_inventory_comment_length}
  runtime_campaign_inventory_comment_region)
string(FIND "${runtime_campaign_inventory_comment_region}"
  "gunCommentPolicy.usesUnfinishedBusinessGunComments()"
  runtime_campaign_inventory_comment_gate_position)
string(FIND "${runtime_campaign_inventory_comment_region}"
  "HandleNewGunComment( pSoldier, pObj->usItem, FALSE )"
  runtime_campaign_inventory_comment_call_position)
string(FIND "${runtime_campaign_inventory_comment_region}"
  "pInSlot = &(pSoldier->inventory()[bPos])"
  runtime_campaign_inventory_comment_placement_position)
if(runtime_campaign_inventory_comment_gate_position EQUAL -1 OR
    runtime_campaign_inventory_comment_call_position EQUAL -1 OR
    runtime_campaign_inventory_comment_placement_position EQUAL -1 OR
    NOT runtime_campaign_inventory_comment_gate_position LESS
      runtime_campaign_inventory_comment_call_position OR
    NOT runtime_campaign_inventory_comment_call_position LESS
      runtime_campaign_inventory_comment_placement_position)
  message(FATAL_ERROR
    "Inventory gun comments must gate campaign before the exact pre-placement item read")
endif()

file(READ "${SOURCE_ROOT}/Tactical/CMakeLists.txt"
  runtime_campaign_gun_comment_tactical_build_contents)
foreach(required_campaign_gun_comment_common_source IN ITEMS
    "Handle Items.cpp"
    "Items.cpp"
    "Ja25_Tactical.cpp")
  string(FIND "${runtime_campaign_gun_comment_tactical_build_contents}"
    "${required_campaign_gun_comment_common_source}"
    required_campaign_gun_comment_common_source_position)
  if(required_campaign_gun_comment_common_source_position EQUAL -1)
    message(FATAL_ERROR
      "All-host tactical gun-comment linkage lost '${required_campaign_gun_comment_common_source}'")
  endif()
endforeach()

foreach(required_campaign_gun_comment_test_build_fragment IN ITEMS
    "add_executable(campaign_gun_comment_policy_tests"
    "campaign_gun_comment_policy_tests.cpp"
    "add_test(NAME campaign_gun_comment_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_campaign_gun_comment_test_build_fragment}"
    required_campaign_gun_comment_test_build_position)
  if(required_campaign_gun_comment_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign gun-comment policy lost test manifest '${required_campaign_gun_comment_test_build_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_gun_comment_policy_tests"
  runtime_campaign_gun_comment_ci_position)
if(runtime_campaign_gun_comment_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the campaign gun-comment policy test")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_gun_comment_policy_tests.cpp"
  runtime_campaign_gun_comment_test_contents)
foreach(required_campaign_gun_comment_test_fragment IN ITEMS
    "not editor host identity, selects gun comments"
    "Arulco short-circuits the ground item-index read and gun-comment effect"
    "UB preserves the ground path's raw zero-index rejection"
    "31000, 32000"
    "UB forwards every nonzero raw ground index exactly once with TRUE"
    "Arulco short-circuits the inventory item read and gun-comment effect"
    "UB inventory placement forwards every raw item ID once with FALSE")
  string(FIND "${runtime_campaign_gun_comment_test_contents}"
    "${required_campaign_gun_comment_test_fragment}"
    required_campaign_gun_comment_test_position)
  if(required_campaign_gun_comment_test_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign gun-comment policy tests lost '${required_campaign_gun_comment_test_fragment}'")
  endif()
endforeach()

# IMP activation and text lookup are selected through one dependency-free
# campaign policy. Production must gate UB option/file reads from the left so
# Arulco never evaluates UB-only configuration or content probes.
file(READ "${SOURCE_ROOT}/Ja2/CampaignImpPolicy.h"
  runtime_campaign_imp_policy_contents)
foreach(required_campaign_imp_policy_fragment IN ITEMS
    "enum class ActivationDecision"
    "KnownButUnavailable"
    "classifyActivation"
    "ja2PassEnabled && matchesJa2Pass"
    "unfinishedBusinessPassEnabled &&"
    "IMPText25.edt"
    "IMPText.edt"
    "IMPText.EDT")
  string(FIND "${runtime_campaign_imp_policy_contents}"
    "${required_campaign_imp_policy_fragment}"
    required_campaign_imp_policy_position)
  if(required_campaign_imp_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime IMP policy lost '${required_campaign_imp_policy_fragment}'")
  endif()
endforeach()

foreach(runtime_campaign_imp_source IN ITEMS
    "Laptop/IMP HomePage.cpp"
    "Laptop/IMP Text System.cpp")
  file(READ "${SOURCE_ROOT}/${runtime_campaign_imp_source}"
    runtime_campaign_imp_source_contents)
  string(FIND "${runtime_campaign_imp_source_contents}" "JA2UB"
    runtime_campaign_imp_guard_position)
  if(NOT runtime_campaign_imp_guard_position EQUAL -1)
    message(FATAL_ERROR
      "${runtime_campaign_imp_source} restored compile-time campaign identity")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/Laptop/IMP HomePage.cpp"
  runtime_campaign_imp_home_contents)
foreach(required_campaign_imp_home_fragment IN ITEMS
    "CampaignImpPolicy impPolicy(GetGameContext().capabilities())"
    "if (impPolicy.usesUnfinishedBusinessImpRules())"
    "CampaignImpPolicy::ActivationDecision::Authorized"
    "CampaignImpPolicy::ActivationDecision::Invalid")
  string(FIND "${runtime_campaign_imp_home_contents}"
    "${required_campaign_imp_home_fragment}"
    required_campaign_imp_home_position)
  if(required_campaign_imp_home_position EQUAL -1)
    message(FATAL_ERROR
      "IMP home-page runtime routing lost '${required_campaign_imp_home_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/Laptop/IMP Text System.cpp"
  runtime_campaign_imp_text_policy_contents)
foreach(required_campaign_imp_text_policy_fragment IN ITEMS
    "impPolicy.usesUnfinishedBusinessImpRules() &&"
    "FileExists(\"BINARYDATA\\\\IMPText25.edt\")"
    "impPolicy.impTextResource")
  string(FIND "${runtime_campaign_imp_text_policy_contents}"
    "${required_campaign_imp_text_policy_fragment}"
    required_campaign_imp_text_policy_position)
  if(required_campaign_imp_text_policy_position EQUAL -1)
    message(FATAL_ERROR
      "IMP text runtime routing lost '${required_campaign_imp_text_policy_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/tests/campaign_imp_policy_tests.cpp"
  runtime_campaign_imp_policy_test_contents)
foreach(required_campaign_imp_test_fragment IN ITEMS
    "legacy-precedence truth table"
    "KnownButUnavailable"
    "IMPText25 with its established IMPText fallback")
  string(FIND "${runtime_campaign_imp_policy_test_contents}"
    "${required_campaign_imp_test_fragment}"
    required_campaign_imp_test_position)
  if(required_campaign_imp_test_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign IMP policy tests lost '${required_campaign_imp_test_fragment}'")
  endif()
endforeach()

# Dealer and shopkeeper behavior uses a typed runtime identity because raw
# save/XML slots 5-18 have different meanings in the two campaign rosters.
# The persisted 80-slot layout must remain unchanged while both inventories
# and both behavior paths stay linkable from every host.
file(READ "${SOURCE_ROOT}/Ja2/CampaignDealerPolicy.h"
  runtime_campaign_dealer_policy_contents)
foreach(required_dealer_policy_fragment IN ITEMS
    "enum class CampaignDealer"
    "class CampaignDealerPolicy"
    "usesUnfinishedBusinessRoster"
    "dealerId(CampaignDealer dealer)"
    "dealerAt(int rawDealerId)"
    "CampaignDealer::Devin) == 5"
    "CampaignDealer::Howard) == 5"
    "CampaignDealer::Perko) == 16"
    "CampaignDealer::Raul) == 15")
  string(FIND "${runtime_campaign_dealer_policy_contents}"
    "${required_dealer_policy_fragment}" required_dealer_policy_position)
  if(required_dealer_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime dealer policy lost '${required_dealer_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Arms Dealer Init.h"
  runtime_campaign_dealer_header_contents)
foreach(required_dealer_layout_fragment IN ITEMS
    "ARMS_DEALER_LEGACY_SLOT_5"
    "ARMS_DEALER_LEGACY_SLOT_18"
    "static_assert(ARMS_DEALER_TINA == 19)"
    "static_assert(ARMS_DEALER_ADDITIONAL_1 == 20)"
    "static_assert(NUM_ARMS_DEALERS == 80)"
    "GetCampaignArmsDealerID"
    "GetCampaignArmsDealerFromID"
    "IsCampaignArmsDealer")
  string(FIND "${runtime_campaign_dealer_header_contents}"
    "${required_dealer_layout_fragment}" required_dealer_layout_position)
  if(required_dealer_layout_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime dealer boundary lost persisted-layout contract '${required_dealer_layout_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Arms Dealer Init.cpp"
  runtime_campaign_dealer_init_contents)
foreach(required_dealer_runtime_fragment IN ITEMS
    "CurrentCampaignDealerPolicy"
    "usesUnfinishedBusinessRoster"
    "GetCampaignArmsDealerID"
    "IsCampaignArmsDealer(ubArmsDealer, CampaignDealer::Raul)"
    "IsCampaignArmsDealer(ubArmsDealer, CampaignDealer::Betty)")
  string(FIND "${runtime_campaign_dealer_init_contents}"
    "${required_dealer_runtime_fragment}" required_dealer_runtime_position)
  if(required_dealer_runtime_position EQUAL -1)
    message(FATAL_ERROR
      "Dealer lifecycle lost runtime campaign routing '${required_dealer_runtime_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/ArmsDealerInvInit.cpp"
  runtime_campaign_dealer_inventory_contents)
foreach(required_dealer_inventory_fragment IN ITEMS
    "GetCampaignArmsDealerFromID"
    "CampaignDealer::Betty"
    "CampaignDealer::Devin"
    "CampaignDealer::Raul"
    "CampaignDealer::Perko"
    "ARMS_DEALER_ADDITIONAL_1")
  string(FIND "${runtime_campaign_dealer_inventory_contents}"
    "${required_dealer_inventory_fragment}" required_dealer_inventory_position)
  if(required_dealer_inventory_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime dealer inventory routing lost '${required_dealer_inventory_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp"
  runtime_campaign_shopkeeper_contents)
foreach(required_shopkeeper_runtime_fragment IN ITEMS
    "UsesUnfinishedBusinessDealerRoster"
    "SelectedDealerIs(CampaignDealer::Betty)"
    "SelectedDealerIs(CampaignDealer::Raul)"
    "SelectedDealerIs(CampaignDealer::Devin)"
    "SelectedDealerIs(CampaignDealer::Perko)")
  string(FIND "${runtime_campaign_shopkeeper_contents}"
    "${required_shopkeeper_runtime_fragment}" required_shopkeeper_runtime_position)
  if(required_shopkeeper_runtime_position EQUAL -1)
    message(FATAL_ERROR
      "Shopkeeper behavior lost runtime campaign routing '${required_shopkeeper_runtime_fragment}'")
  endif()
endforeach()

foreach(retired_dealer_id IN ITEMS
    "ARMS_DEALER_DEVIN"
    "ARMS_DEALER_HOWARD"
    "ARMS_DEALER_SAM"
    "ARMS_DEALER_FRANK"
    "ARMS_DEALER_BAR_BRO_1"
    "ARMS_DEALER_BAR_BRO_2"
    "ARMS_DEALER_BAR_BRO_3"
    "ARMS_DEALER_BAR_BRO_4"
    "ARMS_DEALER_MICKY"
    "ARMS_DEALER_ARNIE"
    "ARMS_DEALER_FREDO"
    "ARMS_DEALER_PERKO"
    "ARMS_DEALER_RAUL"
    "ARMS_DEALER_ELGIN"
    "ARMS_DEALER_MANNY"
    "ARMS_DEALER_BETTY")
  string(FIND
    "${runtime_campaign_dealer_header_contents}${runtime_campaign_dealer_init_contents}${runtime_campaign_dealer_inventory_contents}${runtime_campaign_shopkeeper_contents}"
    "${retired_dealer_id}" retired_dealer_id_position)
  if(NOT retired_dealer_id_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign-colliding raw dealer alias returned: ${retired_dealer_id}")
  endif()
endforeach()

foreach(dealer_runtime_caller IN ITEMS
    "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
    "${SOURCE_ROOT}/TacticalAI/NPC.cpp")
  file(READ "${dealer_runtime_caller}" dealer_runtime_caller_contents)
  string(FIND "${dealer_runtime_caller_contents}"
    "GetCampaignArmsDealerID" dealer_runtime_caller_position)
  if(dealer_runtime_caller_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign-colliding dealer caller lost runtime identity in ${dealer_runtime_caller}")
  endif()
endforeach()

foreach(required_dealer_policy_test_fragment IN ITEMS
    "campaign_dealer_policy_tests.cpp"
    "campaign_dealer_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_dealer_policy_test_fragment}"
    required_dealer_policy_test_position)
  if(required_dealer_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime dealer policy lost its headless test target")
  endif()
endforeach()

string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_dealer_policy_tests" runtime_dealer_policy_ci_position)
if(runtime_dealer_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime dealer policy test target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_dealer_policy_tests.cpp"
  runtime_campaign_dealer_test_contents)
foreach(required_dealer_policy_assertion IN ITEMS
    "arulcoRoster"
    "unfinishedBusinessRoster"
    "CheckRoster"
    "CampaignDealer::Raul"
    "CampaignDealer::Betty"
    "CampaignDealer::Devin"
    "CampaignDealer::Perko"
    "dealerAt(79)")
  string(FIND "${runtime_campaign_dealer_test_contents}"
    "${required_dealer_policy_assertion}" required_dealer_policy_assertion_position)
  if(required_dealer_policy_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime dealer policy lost test coverage for '${required_dealer_policy_assertion}'")
  endif()
endforeach()

# Mercenary profiles, hiring, arrival, recruitment, contracts, tactical AI,
# strategic events, UI, and daily lifecycle rules now select campaign behavior
# at runtime. The policy remains data-free, campaign-colliding profile aliases
# cannot return to the shared profile layout or migrated callers, and the
# existing raw profile/email/quote records stay unchanged.
file(READ "${SOURCE_ROOT}/Ja2/CampaignMercenaryPolicy.h"
  runtime_campaign_mercenary_policy_contents)
foreach(required_mercenary_policy_fragment IN ITEMS
    "class CampaignMercenaryPolicy"
    "primaryProfileDataFile"
    "shouldSendMedicalDepositEmail"
    "allowsDismissalFromSector"
    "dismissalRefusalQuote")
  string(FIND "${runtime_campaign_mercenary_policy_contents}"
    "${required_mercenary_policy_fragment}"
    required_mercenary_policy_position)
  if(required_mercenary_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime mercenary policy lost '${required_mercenary_policy_fragment}'")
  endif()
endforeach()

set(semantic_profile_runtime_callers
  "${SOURCE_ROOT}/Ja2/GameInitOptionsScreen.cpp"
  "${SOURCE_ROOT}/Ja2/Intro.cpp"
  "${SOURCE_ROOT}/Ja2/MPScoreScreen.cpp"
  "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  "${SOURCE_ROOT}/Laptop/personnel.cpp"
  "${SOURCE_ROOT}/Multiplayer/client.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/Auto Resolve.cpp"
  "${SOURCE_ROOT}/Strategic/Game Init.cpp"
  "${SOURCE_ROOT}/Strategic/Hourly Update.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  "${SOURCE_ROOT}/Strategic/Meanwhile.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Event Handler.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Town Loyalty.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  "${SOURCE_ROOT}/Tactical/Food.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Ani.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Init List.cpp"
  "${SOURCE_ROOT}/Tactical/TacticalActorModifiers.cpp"
  "${SOURCE_ROOT}/Tactical/TeamTurns.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIUtils.cpp"
  "${SOURCE_ROOT}/TacticalAI/Attacks.cpp"
  "${SOURCE_ROOT}/TacticalAI/FindLocations.cpp"
  "${SOURCE_ROOT}/TacticalAI/NPC.cpp")
foreach(semantic_profile_runtime_caller IN LISTS
    semantic_profile_runtime_callers)
  file(READ "${semantic_profile_runtime_caller}"
    semantic_profile_runtime_caller_contents)
  string(FIND "${semantic_profile_runtime_caller_contents}"
    "CampaignProfileCode::Role" semantic_profile_runtime_caller_position)
  if(semantic_profile_runtime_caller_position EQUAL -1)
    message(FATAL_ERROR
      "Semantic campaign-profile caller lost runtime role routing in ${semantic_profile_runtime_caller}")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(MIGUEL|CARLOS|IRA|DIMITRI|DEVIN|HAMOUS|SLAY)([^A-Za-z0-9_]|$)"
    retired_semantic_profile_alias
    "${semantic_profile_runtime_caller_contents}")
  if(retired_semantic_profile_alias)
    message(FATAL_ERROR
      "Campaign-colliding raw profile alias returned in ${semantic_profile_runtime_caller}: ${retired_semantic_profile_alias}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Merc Hiring.cpp"
  runtime_campaign_merc_hiring_contents)
string(FIND "${runtime_campaign_merc_hiring_contents}"
  "shouldStartArrivalHelicopter" required_merc_hiring_position)
if(required_merc_hiring_position EQUAL -1)
  message(FATAL_ERROR
    "Mercenary hiring/arrival lost runtime campaign routing")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Merc Entering.cpp"
  runtime_campaign_merc_entering_contents)
string(FIND "${runtime_campaign_merc_entering_contents}"
  "mercenaryPolicy.helicopterDropGridNo"
  runtime_campaign_merc_entering_position)
if(runtime_campaign_merc_entering_position EQUAL -1)
  message(FATAL_ERROR
    "Helicopter insertion lost runtime campaign grid selection")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  runtime_campaign_soldier_create_contents)
string(FIND "${runtime_campaign_soldier_create_contents}"
  "usesUnfinishedBusinessSectorCoolness" required_soldier_create_position)
if(required_soldier_create_position EQUAL -1)
  message(FATAL_ERROR
    "Soldier creation lost runtime campaign routing")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Profile.cpp"
  runtime_campaign_soldier_profile_contents)
string(FIND "${runtime_campaign_soldier_profile_contents}"
  "initialAssignmentChanceMultiplier" required_soldier_profile_position)
if(required_soldier_profile_position EQUAL -1)
  message(FATAL_ERROR
    "Mercenary profile lifecycle lost runtime campaign routing")
endif()

file(READ "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  runtime_campaign_merc_contract_contents)
string(FIND "${runtime_campaign_merc_contract_contents}"
  "shouldSendMedicalDepositEmail" required_merc_contract_position)
if(required_merc_contract_position EQUAL -1)
  message(FATAL_ERROR
    "Mercenary contracts lost runtime campaign routing")
endif()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Merc Handler.cpp"
  runtime_campaign_merc_handler_contents)
string(FIND "${runtime_campaign_merc_handler_contents}"
  "includesDevinInNpcContractGroup" required_merc_handler_position)
if(required_merc_handler_position EQUAL -1)
  message(FATAL_ERROR
    "Strategic mercenary handling lost runtime campaign routing")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Profile.h"
  runtime_campaign_profile_layout_contents)
string(FIND "${runtime_campaign_profile_layout_contents}"
  "RPC65 = 65" required_profile_layout_position)
if(required_profile_layout_position EQUAL -1)
  message(FATAL_ERROR
    "Legacy profile layout lost its stable RPC65 alias")
endif()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(MIGUEL|CARLOS|IRA|DIMITRI|DEVIN|ROBOT|HAMOUS|SLAY)([^A-Za-z0-9_]|$)"
  retired_profile_enumerator "${runtime_campaign_profile_layout_contents}")
if(retired_profile_enumerator)
  message(FATAL_ERROR
    "Shared profile layout regained campaign-colliding alias '${retired_profile_enumerator}'")
endif()

file(READ "${SOURCE_ROOT}/Laptop/email.h"
  runtime_campaign_mercenary_email_contents)
foreach(required_mercenary_email_fragment IN ITEMS
    "JA2_EMAIL_AIM_MEDICAL_DEPOSIT_REFUND = 26"
    "JA25_EMAIL_AIM_MEDICAL_DEPOSIT_REFUND = 27"
    "JA2_EMAIL_ENRICO_MIGUEL = 149"
    "JA2_EMAIL_JOHN_KULBA_MISSED_FLIGHT_1 = 224")
  string(FIND "${runtime_campaign_mercenary_email_contents}"
    "${required_mercenary_email_fragment}" required_mercenary_email_position)
  if(required_mercenary_email_position EQUAL -1)
    message(FATAL_ERROR
      "Mercenary lifecycle lost stable email record '${required_mercenary_email_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/Speck Quotes.h"
  runtime_campaign_mercenary_quote_contents)
file(READ "${SOURCE_ROOT}/Ja2/CampaignSpeckQuoteCodes.h"
  runtime_campaign_speck_quote_code_contents)
foreach(required_mercenary_quote_fragment IN ITEMS
    "JA2_SPECK_PLAYABLE_QUOTE_LARRY_RELAPSED"
    "JA2_SPECK_PLAYABLE_QUOTE_FLO_MARRIED")
  string(FIND "${runtime_campaign_mercenary_quote_contents}"
    "${required_mercenary_quote_fragment}"
    runtime_campaign_mercenary_quote_position)
  if(runtime_campaign_mercenary_quote_position EQUAL -1)
    message(FATAL_ERROR
      "Mercenary lifecycle lost quote alias '${required_mercenary_quote_fragment}'")
  endif()
endforeach()
foreach(required_mercenary_quote_code_fragment IN ITEMS
    "PlayableLarryRelapsed = 100"
    "PlayableFloMarried = 101")
  string(FIND "${runtime_campaign_speck_quote_code_contents}"
    "${required_mercenary_quote_code_fragment}"
    runtime_campaign_mercenary_quote_code_position)
  if(runtime_campaign_mercenary_quote_code_position EQUAL -1)
    message(FATAL_ERROR
      "Mercenary lifecycle lost stable quote record '${required_mercenary_quote_code_fragment}'")
  endif()
endforeach()

foreach(required_mercenary_policy_test_fragment IN ITEMS
    "campaign_mercenary_policy_tests.cpp"
    "campaign_mercenary_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_mercenary_policy_test_fragment}"
    required_mercenary_policy_test_position)
  if(required_mercenary_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime mercenary policy lost its headless test target")
  endif()
endforeach()

string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_mercenary_policy_tests" runtime_mercenary_policy_ci_position)
if(runtime_mercenary_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime mercenary policy test target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_mercenary_policy_tests.cpp"
  runtime_campaign_mercenary_test_contents)
foreach(required_mercenary_policy_assertion IN ITEMS
    "GameCampaign::Arulco"
    "GameCampaign::UnfinishedBusiness"
    "arulcoProfiles"
    "{57, 58, 59, 60, 61, 62, 63, 64}"
    "unfinishedBusinessProfiles"
    "{58, 59, 60, 61, 62, 63, 64, 65}"
    "CampaignProfileCode::Role::Miguel"
    "CampaignProfileCode::Role::Carlos"
    "CampaignProfileCode::Role::Ira"
    "CampaignProfileCode::Role::Dimitri"
    "CampaignProfileCode::Role::Devin"
    "CampaignProfileCode::Role::Robot"
    "CampaignProfileCode::Role::Hamous"
    "CampaignProfileCode::Role::Slay"
    "allowsDismissalFromSector(13)"
    "allowsDismissalFromSector(14)"
    "DismissalRefusalQuote::AnsweringMachine"
    "DismissalRefusalQuote::RefusingOrder")
  string(FIND "${runtime_campaign_mercenary_test_contents}"
    "${required_mercenary_policy_assertion}"
    required_mercenary_policy_assertion_position)
  if(required_mercenary_policy_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime mercenary policy lost test coverage for '${required_mercenary_policy_assertion}'")
  endif()
endforeach()

# The complete M.E.R.C. laptop domain now selects account lifecycle, billing,
# equipment offers, availability, and Speck speech records at runtime. Quote
# roles with campaign-local record numbers must stay typed; restoring a raw
# alias would silently give one campaign the other campaign's dialogue.
file(READ "${SOURCE_ROOT}/Ja2/CampaignMercSitePolicy.h"
  runtime_campaign_merc_site_policy_contents)
foreach(required_merc_site_policy_fragment IN ITEMS
    "class CampaignMercSitePolicy"
    "createsAccountAtGameStart"
    "usesDeferredBilling"
    "firstEquipmentKitIsFree"
    "initialHireCharge"
    "randomQuoteCount")
  string(FIND "${runtime_campaign_merc_site_policy_contents}"
    "${required_merc_site_policy_fragment}"
    required_merc_site_policy_position)
  if(required_merc_site_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime M.E.R.C. site policy lost '${required_merc_site_policy_fragment}'")
  endif()
endforeach()

foreach(required_speck_quote_role IN ITEMS
    "AdvertiseGaston"
    "AdvertiseStogie"
    "GastonDead"
    "StogieDead"
    "PlayerHiresGaston"
    "PlayerHiresStogie"
    "RandomChitChat1"
    "RandomChitChat2")
  string(FIND "${runtime_campaign_speck_quote_code_contents}"
    "${required_speck_quote_role}" required_speck_quote_role_position)
  if(required_speck_quote_role_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime Speck quote routing lost role '${required_speck_quote_role}'")
  endif()
endforeach()

foreach(retired_speck_quote_alias IN ITEMS
    "SPECK_QUOTE_ADVERTISE_GASTON"
    "SPECK_QUOTE_ADVERTISE_STOGIE"
    "SPECK_QUOTE_GASTON_DEAD"
    "SPECK_QUOTE_STOGIE_DEAD"
    "SPECK_QUOTE_PLAYER_HIRES_GASTON"
    "SPECK_QUOTE_PLAYER_HIRES_STOGIE"
    "SPECK_QUOTE_RANDOM_CHIT_CHAT_1"
    "SPECK_QUOTE_RANDOM_CHIT_CHAT_2")
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])${retired_speck_quote_alias}([^A-Za-z0-9_]|$)"
    retired_speck_quote_alias_match
    "${runtime_campaign_mercenary_quote_contents}")
  if(retired_speck_quote_alias_match)
    message(FATAL_ERROR
      "Campaign-colliding raw Speck quote alias returned: ${retired_speck_quote_alias}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/mercs.cpp"
  runtime_campaign_merc_site_contents)
foreach(required_merc_site_runtime_fragment IN ITEMS
    "CampaignMercSitePolicy"
    "CampaignSpeckQuoteCode::Role::AdvertiseGaston"
    "CampaignSpeckQuoteCode::Role::GastonDead"
    "InitializeMercRandomQuotes"
    "usesDeferredBilling"
    "supportsServerOutage")
  string(FIND "${runtime_campaign_merc_site_contents}"
    "${required_merc_site_runtime_fragment}"
    required_merc_site_runtime_position)
  if(required_merc_site_runtime_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. landing page lost runtime routing '${required_merc_site_runtime_fragment}'")
  endif()
endforeach()

foreach(merc_site_runtime_caller IN ITEMS
    "${SOURCE_ROOT}/Laptop/mercs Account.cpp"
    "${SOURCE_ROOT}/Laptop/mercs Files.cpp"
    "${SOURCE_ROOT}/Laptop/mercs No Account.cpp")
  file(READ "${merc_site_runtime_caller}"
    merc_site_runtime_caller_contents)
  string(FIND "${merc_site_runtime_caller_contents}"
    "CampaignMercSitePolicy" merc_site_runtime_caller_position)
  if(merc_site_runtime_caller_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. subpage lost runtime campaign routing in ${merc_site_runtime_caller}")
  endif()
endforeach()

foreach(required_merc_site_email_fragment IN ITEMS
    "JA2_EMAIL_MERC_WARNING = 30"
    "JA2_EMAIL_MERC_INVALID = 32"
    "JA2_EMAIL_MERC_FIRST_WARNING = 36")
  string(FIND "${runtime_campaign_mercenary_email_contents}"
    "${required_merc_site_email_fragment}" required_merc_site_email_position)
  if(required_merc_site_email_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. site lost stable email record '${required_merc_site_email_fragment}'")
  endif()
endforeach()

foreach(required_merc_site_policy_test_fragment IN ITEMS
    "campaign_merc_site_policy_tests.cpp"
    "campaign_merc_site_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_merc_site_policy_test_fragment}"
    required_merc_site_policy_test_position)
  if(required_merc_site_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime M.E.R.C. site policy lost its headless test target")
  endif()
endforeach()

string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_merc_site_policy_tests" runtime_merc_site_policy_ci_position)
if(runtime_merc_site_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime M.E.R.C. site policy test target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_merc_site_policy_tests.cpp"
  runtime_campaign_merc_site_test_contents)
foreach(required_merc_site_policy_assertion IN ITEMS
    "GameCampaign::Arulco"
    "GameCampaign::UnfinishedBusiness"
    "expectedArulcoRandomQuotes"
    "expectedUnfinishedBusinessRandomQuotes"
    "{76, 77, 78, 79, 80, 81, 82, 83}"
    "{94, 95, 96, 97, 98, 99, 100, 101}"
    "firstEquipmentKitIsFree(0)"
    "initialHireCharge(900, 300, 1)")
  string(FIND "${runtime_campaign_merc_site_test_contents}"
    "${required_merc_site_policy_assertion}"
    required_merc_site_policy_assertion_position)
  if(required_merc_site_policy_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime M.E.R.C. site policy lost test coverage for '${required_merc_site_policy_assertion}'")
  endif()
endforeach()

# A.I.M.'s complete links/member cluster selects configurable UB links, salary
# presentation, mission-fee pricing, and video-conference controls at runtime.
# Both callers and the value-only policy must remain guard-free in every host.
file(READ "${SOURCE_ROOT}/Ja2/CampaignAimSitePolicy.h"
  runtime_campaign_aim_site_policy_contents)
foreach(required_aim_site_policy_fragment IN ITEMS
    "class CampaignAimSitePolicy"
    "linkEnabled"
    "usesMissionFee"
    "showsSalaryBreakdown"
    "showsOneTimeFeeOffer"
    "contractCharge"
    "hidesContractAndEquipmentButtons")
  string(FIND "${runtime_campaign_aim_site_policy_contents}"
    "${required_aim_site_policy_fragment}"
    required_aim_site_policy_position)
  if(required_aim_site_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime A.I.M. site policy lost '${required_aim_site_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/AimLinks.cpp"
  runtime_campaign_aim_links_contents)
foreach(required_aim_links_fragment IN ITEMS
    "CampaignAimSitePolicy"
    "enum class AimLink"
    "RefreshAimLinkAvailability"
    "AIM_LINK_GRAPHICS"
    "linkEnabled")
  string(FIND "${runtime_campaign_aim_links_contents}"
    "${required_aim_links_fragment}" required_aim_links_position)
  if(required_aim_links_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. links page lost runtime lifecycle routing '${required_aim_links_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/AimMembers.cpp"
  runtime_campaign_aim_members_contents)
foreach(required_aim_members_fragment IN ITEMS
    "CampaignAimSitePolicy"
    "usesMissionFee"
    "showsSalaryBreakdown"
    "showsOneTimeFeeOffer"
    "showsSelectionLights"
    "contractCharge"
    "hidesContractAndEquipmentButtons")
  string(FIND "${runtime_campaign_aim_members_contents}"
    "${required_aim_members_fragment}" required_aim_members_position)
  if(required_aim_members_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. member page lost runtime routing '${required_aim_members_fragment}'")
  endif()
endforeach()

foreach(required_aim_site_policy_test_fragment IN ITEMS
    "campaign_aim_site_policy_tests.cpp"
    "campaign_aim_site_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_aim_site_policy_test_fragment}"
    required_aim_site_policy_test_position)
  if(required_aim_site_policy_test_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime A.I.M. site policy lost its headless test target")
  endif()
endforeach()

string(FIND "${runtime_campaign_policy_ci_contents}"
  "campaign_aim_site_policy_tests" runtime_aim_site_policy_ci_position)
if(runtime_aim_site_policy_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the runtime A.I.M. site policy test target")
endif()

file(READ "${SOURCE_ROOT}/tests/campaign_aim_site_policy_tests.cpp"
  runtime_campaign_aim_site_test_contents)
foreach(required_aim_site_policy_assertion IN ITEMS
    "GameCampaign::Arulco"
    "GameCampaign::UnfinishedBusiness"
    "linkEnabled(false)"
    "showsSalaryBreakdown"
    "showsOneTimeFeeOffer"
    "oneDay"
    "oneWeek"
    "twoWeeks"
    "!= 1700"
    "!= 700")
  string(FIND "${runtime_campaign_aim_site_test_contents}"
    "${required_aim_site_policy_assertion}"
    required_aim_site_policy_assertion_position)
  if(required_aim_site_policy_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime A.I.M. site policy lost test coverage for '${required_aim_site_policy_assertion}'")
  endif()
endforeach()

# Laptop communications now select campaign catalogs, insurance templates,
# shipment notices, and overlapping special-message IDs at runtime. The same
# batch hardens the legacy collection and resource lifetimes found by the
# Laptop-wide static-analysis walkthrough.
file(READ "${SOURCE_ROOT}/Ja2/CampaignLaptopCommunicationsPolicy.h"
  runtime_laptop_communications_policy_contents)
foreach(required_laptop_communications_policy_fragment IN ITEMS
    "class CampaignLaptopCommunicationsPolicy"
    "insuranceAvailable"
    "insuranceRecord"
    "bobbyShipmentRecord"
    "johnKulbaShipmentNoticeAvailable"
    "sendsInitialArulcoCongratulations"
    "impProfileResultsOffset"
    "isImpProfileResultsMessage")
  string(FIND "${runtime_laptop_communications_policy_contents}"
    "${required_laptop_communications_policy_fragment}"
    required_laptop_communications_policy_position)
  if(required_laptop_communications_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Runtime Laptop communications policy lost '${required_laptop_communications_policy_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/email.cpp"
  runtime_laptop_email_contents)
foreach(required_runtime_laptop_email_fragment IN ITEMS
    "CampaignLaptopCommunicationsPolicy"
    "LoadEmailRecord"
    "HandleUnfinishedBusinessMailSpecialMessages"
    "isImpProfileResultsMessage"
    "static_cast<UINT16>(XML_NOEMAIL)"
    "RecordPtr pCurrentRecord = nullptr")
  string(FIND "${runtime_laptop_email_contents}"
    "${required_runtime_laptop_email_fragment}"
    required_runtime_laptop_email_position)
  if(required_runtime_laptop_email_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop email lost runtime/safety routing '${required_runtime_laptop_email_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/insurance Contract.cpp"
  runtime_laptop_insurance_contents)
foreach(required_runtime_laptop_insurance_fragment IN ITEMS
    "CampaignLaptopCommunicationsPolicy"
    "IsValidInsurancePayout"
    "SendInsuranceNotice"
    "resizedPayouts")
  string(FIND "${runtime_laptop_insurance_contents}"
    "${required_runtime_laptop_insurance_fragment}"
    required_runtime_laptop_insurance_position)
  if(required_runtime_laptop_insurance_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop insurance lost runtime/safety routing '${required_runtime_laptop_insurance_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/PostalService.cpp"
  runtime_laptop_postal_contents)
file(READ "${SOURCE_ROOT}/Laptop/PostalService.h"
  runtime_laptop_postal_header_contents)
foreach(required_runtime_laptop_postal_fragment IN ITEMS
    "CampaignLaptopCommunicationsPolicy"
    "FindLaptopRecordById"
    "std::unique_ptr<OBJECTTYPE[]>"
    "CShipmentManipulator shipmentManipulator"
    "IsValidLaptopIndex")
  string(FIND "${runtime_laptop_postal_contents}"
    "${required_runtime_laptop_postal_fragment}"
    required_runtime_laptop_postal_position)
  if(required_runtime_laptop_postal_position EQUAL -1)
    message(FATAL_ERROR
      "PostalService lost runtime/safety routing '${required_runtime_laptop_postal_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_laptop_postal_header_contents}"
  "DestinationDeliveryInfoTable destinationDeliveryInfos"
  runtime_laptop_postal_owned_table_position)
if(runtime_laptop_postal_owned_table_position EQUAL -1)
  message(FATAL_ERROR
    "PostalService delivery methods no longer own their destination tables by value")
endif()

file(READ "${SOURCE_ROOT}/Laptop/LaptopSafety.h"
  runtime_laptop_safety_contents)
foreach(required_laptop_safety_fragment IN ITEMS
    "IsValidLaptopIndex"
    "IsValidIntelMapRegion"
    "HasScrollableBobbyOrder"
    "RemainingLaptopDays"
    "FindLaptopRecordById"
    "ResolveLaptopRosterActor")
  string(FIND "${runtime_laptop_safety_contents}"
    "${required_laptop_safety_fragment}"
    required_laptop_safety_position)
  if(required_laptop_safety_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop safety boundary lost '${required_laptop_safety_fragment}'")
  endif()
endforeach()

foreach(laptop_safety_caller_and_fragment IN ITEMS
    "Laptop/BobbyRMailOrder.cpp|HasScrollableBobbyOrder"
    "Laptop/BobbyRShipments.cpp|IsValidLaptopIndex"
    "Laptop/Intelmarket.cpp|IsValidIntelMapRegion"
    "Laptop/personnel.cpp|ResolveLaptopRosterActor")
  string(REPLACE "|" ";" laptop_safety_caller_parts
    "${laptop_safety_caller_and_fragment}")
  list(GET laptop_safety_caller_parts 0 laptop_safety_caller)
  list(GET laptop_safety_caller_parts 1 laptop_safety_fragment)
  file(READ "${SOURCE_ROOT}/${laptop_safety_caller}"
    laptop_safety_caller_contents)
  string(FIND "${laptop_safety_caller_contents}"
    "${laptop_safety_fragment}" laptop_safety_caller_position)
  if(laptop_safety_caller_position EQUAL -1)
    message(FATAL_ERROR
      "${laptop_safety_caller} bypassed ${laptop_safety_fragment}")
  endif()
endforeach()

# Personnel owns a mutable live roster, three persisted departed lists, and
# several independently replaced UI overlays. Keep its navigation and repair
# rules dependency-free and every resource set transactional.
file(READ "${SOURCE_ROOT}/Laptop/PersonnelRosterModel.h"
  runtime_laptop_personnel_model_contents)
foreach(required_laptop_personnel_model_fragment IN ITEMS
    "class RosterCursor"
    "BuildDepartedRoster"
    "FindDepartedState"
    "MoveDepartedProfile"
    "RemoveDepartedProfile"
    "NormalizeWindowStart"
    "SliderPosition"
    "WindowStartFromSlider"
    "ClampCurrency"
    "CopyText"
    "AppendText")
  string(FIND "${runtime_laptop_personnel_model_contents}"
    "${required_laptop_personnel_model_fragment}"
    required_laptop_personnel_model_position)
  if(required_laptop_personnel_model_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Personnel roster model lost '${required_laptop_personnel_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/personnel.cpp"
  runtime_laptop_personnel_contents)
foreach(required_laptop_personnel_safety_fragment IN ITEMS
    "gPersonnelPageResources"
    "gPersonnelDepartedResources"
    "gPersonnelInventoryResources"
    "gPersonnelAtmResources"
    "gPersonnelTraitResources"
    "std::vector<SoldierID> currentTeamList"
    "RosterCursor gCurrentRosterCursor"
    "RosterCursor gDepartedRosterCursor"
    "RefreshDepartedRoster"
    "GetTheStateOfDepartedMerc"
    "ProfileFor"
    "MoveDepartedProfile"
    "RemoveDepartedProfile"
    "SliderPosition"
    "WindowStartFromSlider"
    "AppendPersonnelText"
    "PersonnelRosterModel::ClampCurrency")
  string(FIND "${runtime_laptop_personnel_contents}"
    "${required_laptop_personnel_safety_fragment}"
    required_laptop_personnel_safety_position)
  if(required_laptop_personnel_safety_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Personnel safety lost '${required_laptop_personnel_safety_fragment}'")
  endif()
endforeach()

string(REGEX MATCH
  "AddVideoObject[ \t\r\n]*\\(|DeleteVideoObjectFromIndex[ \t\r\n]*\\(|LoadButtonImage[ \t\r\n]*\\(|UnloadButtonImage[ \t\r\n]*\\(|RemoveButton[ \t\r\n]*\\(|MSYS_(Add|Remove)Region[ \t\r\n]*\\("
  raw_laptop_personnel_resource_lifecycle "${runtime_laptop_personnel_contents}")
if(raw_laptop_personnel_resource_lifecycle)
  message(FATAL_ERROR
    "Laptop Personnel restored open-coded resource lifecycle '${raw_laptop_personnel_resource_lifecycle}'")
endif()
string(REGEX MATCH
  "mprintf[ \t\r\n]*\\([^,\r\n]+,[^,\r\n]+,[ \t\r\n]*[A-KM-Za-km-z_][A-Za-z0-9_]*(\\[[^]]*\\])?[ \t\r\n]*\\)"
  unsafe_laptop_personnel_render_format "${runtime_laptop_personnel_contents}")
if(unsafe_laptop_personnel_render_format)
  message(FATAL_ERROR
    "Laptop Personnel restored a variable render format '${unsafe_laptop_personnel_render_format}'")
endif()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(wcs(cpy|ncpy|cat|ncat)|swprintf)[ \t\r\n]*\\("
  unsafe_laptop_personnel_text_copy "${runtime_laptop_personnel_contents}")
if(unsafe_laptop_personnel_text_copy)
  message(FATAL_ERROR
    "Laptop Personnel restored an unbounded text operation '${unsafe_laptop_personnel_text_copy}'")
endif()

foreach(retired_laptop_personnel_fragment IN ITEMS
    "iCurrentPersonSelectedId"
    "iCurPortraitId"
    "giCurrentUpperLeftPortraitNumber"
    "maxCurrentTeamIndex"
    "currentTeamIndex"
    "currentTeamFirstIndex"
    "CreateDestroyButtonsForPersonnelDepartures"
    "GetIdOfPastMercInSlot"
    "IsPastMercDead"
    "IsPastMercFired"
    "IsPastMercOther"
    "fAddedTraitRegion")
  string(FIND "${runtime_laptop_personnel_contents}"
    "${retired_laptop_personnel_fragment}" retired_laptop_personnel_position)
  if(NOT retired_laptop_personnel_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Personnel restored retired path '${retired_laptop_personnel_fragment}'")
  endif()
endforeach()

foreach(required_laptop_personnel_test_build_fragment IN ITEMS
    "laptop_personnel_roster_model_tests.cpp"
    "laptop_personnel_roster_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_personnel_test_build_fragment}"
    required_laptop_personnel_test_build_position)
  if(required_laptop_personnel_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Personnel roster model lost its focused test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_personnel_roster_model_tests"
  runtime_laptop_personnel_ci_position)
if(runtime_laptop_personnel_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop Personnel roster model target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_personnel_roster_model_tests.cpp"
  runtime_laptop_personnel_test_contents)
foreach(required_laptop_personnel_test_fragment IN ITEMS
    "Empty rosters have no selectable or navigable entry"
    "Page navigation reaches a one-entry final page and wraps"
    "Departed views filter corrupt IDs and cross-list duplicates"
    "A full destination rejects additions without partial mutation"
    "Inventory windows normalize empty, exact, and stale offsets"
    "Inventory slider mapping handles exact-page, midpoint, and bounds cases"
    "Currency totals clamp instead of overflowing signed UI values"
    "Personnel text helpers always terminate and report truncation")
  string(FIND "${runtime_laptop_personnel_test_contents}"
    "${required_laptop_personnel_test_fragment}"
    required_laptop_personnel_test_position)
  if(required_laptop_personnel_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Personnel tests lost '${required_laptop_personnel_test_fragment}'")
  endif()
endforeach()

# Campaign History combines a mutable incident ledger with four page modes.
# Keep navigation, retention, arithmetic, transfer, picture, and text bounds in
# a dependency-free model; keep page acquisition transactional; and do not
# restore the raw resource/text or partial-load paths retired by this audit.
file(READ "${SOURCE_ROOT}/Laptop/CampaignHistoryModel.h"
  runtime_laptop_campaign_history_model_contents)
foreach(required_laptop_campaign_history_model_fragment IN ITEMS
    "IsValidIndex"
    "NormalizePage"
    "NextPage"
    "PreviousPage"
    "RetainedIncidentCount"
    "ShouldRetainIncident"
    "SaturatingAddUnsigned"
    "SaturatingAddSigned"
    "SaturatingAddFinite"
    "IsExactTransfer"
    "FrameIndex"
    "JoinSelectedDirections"
    "CopyTextFromPointer")
  string(FIND "${runtime_laptop_campaign_history_model_contents}"
    "${required_laptop_campaign_history_model_fragment}"
    required_laptop_campaign_history_model_position)
  if(required_laptop_campaign_history_model_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History model lost '${required_laptop_campaign_history_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/CampaignHistoryText.h"
  runtime_laptop_campaign_history_text_contents)
foreach(required_laptop_campaign_history_text_fragment IN ITEMS
    "FormatCampaignHistoryText"
    "CampaignHistoryModel::CopyText"
    "CampaignHistoryModel::CopyTextFromPointer"
    "sgp_swprintf(destination, Capacity")
  string(FIND "${runtime_laptop_campaign_history_text_contents}"
    "${required_laptop_campaign_history_text_fragment}"
    required_laptop_campaign_history_text_position)
  if(required_laptop_campaign_history_text_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History text boundary lost '${required_laptop_campaign_history_text_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/CampaignHistoryMain.cpp"
  runtime_laptop_campaign_history_main_contents)
file(READ "${SOURCE_ROOT}/Laptop/CampaignHistory_Summary.cpp"
  runtime_laptop_campaign_history_summary_contents)
file(READ "${SOURCE_ROOT}/Laptop/CampaignStats.cpp"
  runtime_laptop_campaign_stats_contents)
file(READ "${SOURCE_ROOT}/Laptop/CampaignStats.h"
  runtime_laptop_campaign_stats_header_contents)

foreach(required_laptop_campaign_history_main_fragment IN ITEMS
    "gCampaignHistoryResources"
    "LoadCampaignHistoryDefaults(stagedResources)"
    "gCampaignHistoryResources = std::move(stagedResources)"
    "CampaignHistoryModel::CopyTextFromPointer")
  string(FIND "${runtime_laptop_campaign_history_main_contents}"
    "${required_laptop_campaign_history_main_fragment}"
    required_laptop_campaign_history_main_position)
  if(required_laptop_campaign_history_main_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History main page lost '${required_laptop_campaign_history_main_fragment}'")
  endif()
endforeach()

foreach(required_laptop_campaign_history_summary_fragment IN ITEMS
    "gCampaignHistorySummaryResources"
    "gCampaignHistoryMostImportantResources"
    "gCampaignHistoryNewsResources"
    "IncidentCount()"
    "CampaignHistoryModel::NormalizePage"
    "CampaignHistoryModel::NextPage"
    "CampaignHistoryModel::PreviousPage"
    "CampaignHistoryModel::FrameIndex"
    "RenderCampaignHistory_News()")
  string(FIND "${runtime_laptop_campaign_history_summary_contents}"
    "${required_laptop_campaign_history_summary_fragment}"
    required_laptop_campaign_history_summary_position)
  if(required_laptop_campaign_history_summary_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History report pages lost '${required_laptop_campaign_history_summary_fragment}'")
  endif()
endforeach()

foreach(required_laptop_campaign_stats_fragment IN ITEMS
    "const Incident_Stats original = *this"
    "const Campaign_Stats original = *this"
    "*this = original"
    "CampaignHistoryModel::IsExactTransfer"
    "CampaignHistoryModel::ShouldRetainIncident"
    "CampaignHistoryModel::SaturatingAddUnsigned"
    "CampaignHistoryModel::SaturatingAddSigned"
    "CampaignHistoryModel::SaturatingAddFinite"
    "std::begin(usRatingAlignmentPadding)"
    "std::begin(usFlagsAlignmentPadding)"
    "std::fill(std::begin(usFiller)"
    "Incident_Stats::GetAttackerDirString")
  string(FIND "${runtime_laptop_campaign_stats_contents}"
    "${required_laptop_campaign_stats_fragment}"
    required_laptop_campaign_stats_position)
  if(required_laptop_campaign_stats_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History persistence/statistics lost '${required_laptop_campaign_stats_fragment}'")
  endif()
endforeach()

foreach(laptop_campaign_history_source_contents IN ITEMS
    runtime_laptop_campaign_history_main_contents
    runtime_laptop_campaign_history_summary_contents
    runtime_laptop_campaign_stats_contents)
  string(REGEX MATCH
    "AddVideoObject[ \\t\\r\\n]*\\(|DeleteVideoObjectFromIndex[ \\t\\r\\n]*\\(|MSYS_(Add|Remove)Region[ \\t\\r\\n]*\\("
    raw_laptop_campaign_history_resource_lifecycle
    "${${laptop_campaign_history_source_contents}}")
  if(raw_laptop_campaign_history_resource_lifecycle)
    message(FATAL_ERROR
      "Laptop Campaign History restored open-coded resource lifecycle '${raw_laptop_campaign_history_resource_lifecycle}'")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(wcs(cpy|ncpy|cat|ncat)|swprintf)[ \\t\\r\\n]*\\("
    unsafe_laptop_campaign_history_text_operation
    "${${laptop_campaign_history_source_contents}}")
  if(unsafe_laptop_campaign_history_text_operation)
    message(FATAL_ERROR
      "Laptop Campaign History restored an unbounded text operation '${unsafe_laptop_campaign_history_text_operation}'")
  endif()
endforeach()

foreach(retired_laptop_campaign_history_fragment IN ITEMS
    "RemoveCampaignHistoryDefaults"
    "InitCampaignHistoryDefaults"
    "StartIncident("
    "tmpdirstring"
    "tmpnr")
  string(FIND
    "${runtime_laptop_campaign_history_main_contents}${runtime_laptop_campaign_history_summary_contents}${runtime_laptop_campaign_stats_contents}${runtime_laptop_campaign_stats_header_contents}"
    "${retired_laptop_campaign_history_fragment}"
    retired_laptop_campaign_history_position)
  if(NOT retired_laptop_campaign_history_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History restored retired path '${retired_laptop_campaign_history_fragment}'")
  endif()
endforeach()

foreach(required_laptop_campaign_history_test_build_fragment IN ITEMS
    "laptop_campaign_history_model_tests.cpp"
    "laptop_campaign_history_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_campaign_history_test_build_fragment}"
    required_laptop_campaign_history_test_build_position)
  if(required_laptop_campaign_history_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History model lost its focused test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_campaign_history_model_tests"
  runtime_laptop_campaign_history_ci_position)
if(runtime_laptop_campaign_history_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop Campaign History model target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_campaign_history_model_tests.cpp"
  runtime_laptop_campaign_history_test_contents)
foreach(required_laptop_campaign_history_test_fragment IN ITEMS
    "Index validation rejects negative and exact-end values"
    "Empty, stale, and exact-end incident pages normalize safely"
    "Report limits retain all short histories and only the requested tail"
    "Incident and aggregate counters saturate instead of wrapping"
    "Persistence accepts only successful exact-size transfers"
    "Picture selection rejects empty libraries before modulo"
    "Direction text is deterministic for zero through four directions"
    "Campaign History text copies terminate and bound fixed sources")
  string(FIND "${runtime_laptop_campaign_history_test_contents}"
    "${required_laptop_campaign_history_test_fragment}"
    required_laptop_campaign_history_test_position)
  if(required_laptop_campaign_history_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Campaign History tests lost '${required_laptop_campaign_history_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/CMakeLists.txt"
  runtime_laptop_manifest_contents)
string(FIND "${runtime_laptop_manifest_contents}" "set(LaptopVariantSrc"
  runtime_laptop_variant_manifest_position)
foreach(required_common_laptop_runtime_source IN ITEMS
    "aim.cpp"
    "AimArchives.cpp"
    "AimFacialIndex.cpp"
    "AimHistory.cpp"
    "AimLinks.cpp"
    "AimMembers.cpp"
    "AimPolicies.cpp"
    "AimSort.cpp"
    "florist Order Form.cpp"
    "mercs Account.cpp"
    "mercs Files.cpp"
    "mercs No Account.cpp"
    "mercs.cpp"
    "BobbyRMailOrder.cpp"
    "email.cpp"
    "files.cpp"
    "history.cpp"
    "insurance Contract.cpp"
    "IMP HomePage.cpp"
    "IMP Text System.cpp"
    "PostalService.cpp")
  string(FIND "${runtime_laptop_manifest_contents}"
    "${required_common_laptop_runtime_source}"
    common_laptop_runtime_source_position)
  if(common_laptop_runtime_source_position EQUAL -1 OR
      common_laptop_runtime_source_position GREATER
        runtime_laptop_variant_manifest_position)
    message(FATAL_ERROR
      "Laptop common manifest lost ${required_common_laptop_runtime_source}")
  endif()
endforeach()

# A.I.M. pages publish legacy numeric handles only after a complete resource
# transaction succeeds. The dependency-free set supports APIs whose valid
# range includes zero, while the Laptop adapter releases regions and buttons
# before their backing images and render assets. Migrated pages must not
# regain open-coded acquisition or teardown paths.
file(READ "${SOURCE_ROOT}/Engine/Core/UniqueResourceHandle.h"
  runtime_unique_resource_handle_contents)
foreach(required_resource_handle_fragment IN ITEMS
    "typename Value = std::uint32_t"
    "Value InvalidValue = Value{}"
    "value_ != InvalidValue")
  string(FIND "${runtime_unique_resource_handle_contents}"
    "${required_resource_handle_fragment}"
    required_resource_handle_position)
  if(required_resource_handle_position EQUAL -1)
    message(FATAL_ERROR
      "Numeric resource ownership lost '${required_resource_handle_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Engine/Core/ResourceHandleSet.h"
  runtime_resource_handle_set_contents)
foreach(required_resource_handle_set_fragment IN ITEMS
    "ResourceHandleSet(const ResourceHandleSet&) = delete"
    "ResourceHandleSet(ResourceHandleSet&&) noexcept = default"
    "handles_.push_back(std::move(handle))"
    "publishedValue = handles_.back().get()"
    "void clear()")
  string(FIND "${runtime_resource_handle_set_contents}"
    "${required_resource_handle_set_fragment}"
    required_resource_handle_set_position)
  if(required_resource_handle_set_position EQUAL -1)
    message(FATAL_ERROR
      "Transactional resource set lost '${required_resource_handle_set_fragment}'")
  endif()
endforeach()

foreach(aim_page_bound_and_fragment IN ITEMS
    "Laptop/AimArchives.cpp|constexpr std::size_t NUM_AIM_ARCHIVE_PAGES"
    "Laptop/AimArchives.cpp|IsValidLaptopIndex(NUM_AIM_ARCHIVE_PAGES"
    "Laptop/AimHistory.cpp|NUM_AIM_HISTORY_PAGES\t\t\t\t\t(NUM_AIM_HISTORY_CONTENT_PAGES + 1)"
    "Laptop/AimHistory.cpp|IsValidLaptopIndex(NUM_AIM_HISTORY_PAGES"
    "Laptop/AimPolicies.cpp|IsValidLaptopIndex(NUM_AIM_POLICY_PAGES")
  string(REPLACE "|" ";" aim_page_bound_parts
    "${aim_page_bound_and_fragment}")
  list(GET aim_page_bound_parts 0 aim_page_bound_source)
  list(GET aim_page_bound_parts 1 aim_page_bound_fragment)
  file(READ "${SOURCE_ROOT}/${aim_page_bound_source}"
    aim_page_bound_contents)
  string(FIND "${aim_page_bound_contents}" "${aim_page_bound_fragment}"
    aim_page_bound_position)
  if(aim_page_bound_position EQUAL -1)
    message(FATAL_ERROR
      "${aim_page_bound_source} lost bounded page state '${aim_page_bound_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/LaptopPageResourceOwner.h"
  runtime_laptop_page_resource_owner_contents)
foreach(required_laptop_page_resource_fragment IN ITEMS
    "ResourceHandleSet<UniqueVideoObjectHandle>"
    "ResourceHandleSet<UniqueVideoSurfaceHandle>"
    "ResourceHandleSet<UniqueButtonImageHandle>"
    "ResourceHandleSet<UniqueButtonHandle>"
    "ResourceHandleSet<UniqueMouseRegionRegistration>"
    "regions_.clear()"
    "buttons_.clear()"
    "buttonImages_.clear()")
  string(FIND "${runtime_laptop_page_resource_owner_contents}"
    "${required_laptop_page_resource_fragment}"
    required_laptop_page_resource_position)
  if(required_laptop_page_resource_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop page resource owner lost '${required_laptop_page_resource_fragment}'")
  endif()
endforeach()

foreach(aim_owned_resource_source IN ITEMS
    "Laptop/aim.cpp"
    "Laptop/AimArchives.cpp"
    "Laptop/AimFacialIndex.cpp"
    "Laptop/AimHistory.cpp"
    "Laptop/AimLinks.cpp"
    "Laptop/AimMembers.cpp"
    "Laptop/AimPolicies.cpp"
    "Laptop/AimSort.cpp")
  file(READ "${SOURCE_ROOT}/${aim_owned_resource_source}"
    aim_owned_resource_contents)
  string(FIND "${aim_owned_resource_contents}"
    "LaptopPageResourceOwner" aim_page_owner_position)
  if(aim_page_owner_position EQUAL -1)
    message(FATAL_ERROR
      "${aim_owned_resource_source} lost transactional page ownership")
  endif()
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(CHECKF[ \t]*\\([ \t]*)?(AddVideoObject|AddVideoSurface|DeleteVideoObjectFromIndex|DeleteVideoSurfaceFromIndex|LoadButtonImage|UnloadButtonImage|RemoveButton|MSYS_AddRegion|MSYS_RemoveRegion)[ \t]*\\("
    raw_aim_page_resource_lifecycle
    "${aim_owned_resource_contents}")
  if(raw_aim_page_resource_lifecycle)
    message(FATAL_ERROR
      "${aim_owned_resource_source} restored open-coded resource ownership '${raw_aim_page_resource_lifecycle}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  runtime_resource_handle_test_contents)
foreach(required_resource_handle_test_fragment IN ITEMS
    "TestSignedResourceHandle"
    "zero remains valid when the resource API uses minus one"
    "failed acquisition leaves the published value and set unchanged"
    "moving a staged set commits its resource beyond staging scope")
  string(FIND "${runtime_resource_handle_test_contents}"
    "${required_resource_handle_test_fragment}"
    required_resource_handle_test_position)
  if(required_resource_handle_test_position EQUAL -1)
    message(FATAL_ERROR
      "Resource ownership tests lost '${required_resource_handle_test_fragment}'")
  endif()
endforeach()

# The complete M.E.R.C. site shares the same transactional resource boundary
# as A.I.M. and selects its campaign behavior at runtime. Persisted roster
# indices and alternate-profile navigation remain bounded, while video startup
# must not publish a failed face or framebuffer lock.
foreach(merc_owned_resource_source IN ITEMS
    "Laptop/mercs.cpp"
    "Laptop/mercs Account.cpp"
    "Laptop/mercs Files.cpp"
    "Laptop/mercs No Account.cpp")
  file(READ "${SOURCE_ROOT}/${merc_owned_resource_source}"
    merc_owned_resource_contents)
  string(FIND "${merc_owned_resource_contents}"
    "LaptopPageResourceOwner" merc_page_owner_position)
  if(merc_page_owner_position EQUAL -1)
    message(FATAL_ERROR
      "${merc_owned_resource_source} lost transactional page ownership")
  endif()
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(CHECKF[ \t]*\\([ \t]*)?(AddVideoObject|AddVideoSurface|DeleteVideoObjectFromIndex|DeleteVideoSurfaceFromIndex|LoadButtonImage|UnloadButtonImage|RemoveButton|MSYS_AddRegion|MSYS_RemoveRegion)[ \t]*\\("
    raw_merc_page_resource_lifecycle
    "${merc_owned_resource_contents}")
  if(raw_merc_page_resource_lifecycle)
    message(FATAL_ERROR
      "${merc_owned_resource_source} restored open-coded resource ownership '${raw_merc_page_resource_lifecycle}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/MercSiteNavigationModel.h"
  runtime_merc_navigation_model_contents)
foreach(required_merc_navigation_fragment IN ITEMS
    "ClampMercSiteIndex"
    "availableCount == 0"
    "SkipMercSiteAlternatePredecessor"
    "hasAlternateProfile && index > 0")
  string(FIND "${runtime_merc_navigation_model_contents}"
    "${required_merc_navigation_fragment}"
    required_merc_navigation_position)
  if(required_merc_navigation_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. navigation lost '${required_merc_navigation_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/mercs.cpp" runtime_merc_site_contents)
foreach(required_merc_site_fragment IN ITEMS
    "CountAvailableMercsAtMercSite"
    "if (!pBuffer) return FALSE"
    "if (giVideoSpeckFaceIndex == -1) return FALSE"
    "gfInMercSite = FALSE"
    ".usesUnfinishedBusinessSite()")
  string(FIND "${runtime_merc_site_contents}" "${required_merc_site_fragment}"
    required_merc_site_position)
  if(required_merc_site_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. site safety lost '${required_merc_site_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_merc_site_contents}" "#ifdef JA25"
  merc_compile_identity_position)
if(NOT merc_compile_identity_position EQUAL -1)
  message(FATAL_ERROR
    "M.E.R.C. restored campaign compile-identity selection")
endif()

file(READ "${SOURCE_ROOT}/Laptop/mercs Files.cpp"
  runtime_merc_files_contents)
foreach(required_merc_files_fragment IN ITEMS
    "ClampMercSiteIndex("
    "SkipMercSiteAlternatePredecessor("
    "CreateMercKitSelectionButtons(staged)"
    "CreateMercWeaponBoxMouseRegions(staged)")
  string(FIND "${runtime_merc_files_contents}" "${required_merc_files_fragment}"
    required_merc_files_position)
  if(required_merc_files_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. Files safety lost '${required_merc_files_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/mercs Account.cpp"
  runtime_merc_account_contents)
foreach(required_merc_account_fragment IN ITEMS
    "if (iCurrentAccountPage > 0)"
    "if (iCurrentAccountPage < iTotalAccountPages - 1)")
  string(FIND "${runtime_merc_account_contents}"
    "${required_merc_account_fragment}" required_merc_account_position)
  if(required_merc_account_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. Account bounds lost '${required_merc_account_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/laptop_communications_policy_tests.cpp"
  runtime_merc_navigation_test_contents)
foreach(required_merc_navigation_test_fragment IN ITEMS
    "M.E.R.C. persisted selections clamp to the available roster"
    "M.E.R.C. alternate-profile navigation cannot underflow index zero")
  string(FIND "${runtime_merc_navigation_test_contents}"
    "${required_merc_navigation_test_fragment}"
    required_merc_navigation_test_position)
  if(required_merc_navigation_test_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. navigation tests lost '${required_merc_navigation_test_fragment}'")
  endif()
endforeach()

# M.E.R.C. Files derives its equipment cells, item/count origins, tooltip
# hitboxes, background tiles, and five kit buttons from one value-only layout.
file(READ "${SOURCE_ROOT}/Laptop/MercFilesLayout.h"
  runtime_merc_files_layout_contents)
foreach(required_merc_files_layout_fragment IN ITEMS
    "namespace MercFilesLayoutModel"
    "struct InventoryLayout"
    "struct KitButtonLayout"
    "struct Layout"
    "kInventoryCapacity = 21"
    "kKitButtonCount = 5"
    "constexpr Layout MakeLayout"
    "constexpr Rect cell"
    "constexpr Point contentOrigin"
    "constexpr Point countTextOrigin"
    "constexpr Rect button")
  string(FIND "${runtime_merc_files_layout_contents}"
    "${required_merc_files_layout_fragment}"
    required_merc_files_layout_position)
  if(required_merc_files_layout_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. Files layout lost '${required_merc_files_layout_fragment}'")
  endif()
endforeach()
foreach(required_merc_files_layout_integration_fragment IN ITEMS
    "#include \"MercFilesLayout.h\""
    "CurrentMercFilesLayout()"
    "layout.inventory.cell"
    "layout.inventory.contentOrigin"
    "layout.inventory.countTextOrigin"
    "layout.kitButtons.button"
    "CurrentMercFilesLayout().inventory")
  string(FIND "${runtime_merc_files_contents}"
    "${required_merc_files_layout_integration_fragment}"
    required_merc_files_layout_integration_position)
  if(required_merc_files_layout_integration_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. Files bypassed shared geometry '${required_merc_files_layout_integration_fragment}'")
  endif()
endforeach()
foreach(retired_merc_files_coordinate IN ITEMS
    "MERC_WEAPONBOX_X_NSGI"
    "MERC_WEAPONBOX_COLUMNS"
    "MERC_WEAPONBOX_LOADOUT_ONE_X"
    "MERC_WEAPONBOX_LOADOUT_FIVE_X")
  string(FIND "${runtime_merc_files_contents}"
    "${retired_merc_files_coordinate}"
    retired_merc_files_coordinate_position)
  if(NOT retired_merc_files_coordinate_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. Files restored retired coordinate family '${retired_merc_files_coordinate}'")
  endif()
endforeach()
foreach(required_merc_files_layout_test_build_fragment IN ITEMS
    "merc_files_layout_tests.cpp"
    "merc_files_layout")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_merc_files_layout_test_build_fragment}"
    required_merc_files_layout_test_build_position)
  if(required_merc_files_layout_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. Files layout lost focused test target '${required_merc_files_layout_test_build_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "merc_files_layout_tests" required_merc_files_layout_ci_position)
if(required_merc_files_layout_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the M.E.R.C. Files layout target")
endif()
file(READ "${SOURCE_ROOT}/tests/merc_files_layout_tests.cpp"
  runtime_merc_files_layout_test_contents)
foreach(required_merc_files_layout_test_fragment IN ITEMS
    "inventory drawing, backgrounds, and hitboxes share one row-major grid"
    "item artwork and count text preserve their exact cell-relative offsets"
    "every M.E.R.C. inventory hitbox stays inside the page bounds"
    "all five M.E.R.C. loadout buttons derive from one tested stride"
    "M.E.R.C. inventory and loadout geometry follows centered-screen offsets")
  string(FIND "${runtime_merc_files_layout_test_contents}"
    "${required_merc_files_layout_test_fragment}"
    required_merc_files_layout_test_position)
  if(required_merc_files_layout_test_position EQUAL -1)
    message(FATAL_ERROR
      "M.E.R.C. Files layout tests lost '${required_merc_files_layout_test_fragment}'")
  endif()
endforeach()

# Florist and Funeral form one transactionally owned service-site cluster.
# Gallery/destination indices remain safe across re-entry and mutable XML
# content, localized text cannot underflow its centering calculation, and the
# Arulco-only delivery scene is selected from runtime campaign capabilities.
foreach(florist_owned_resource_source IN ITEMS
    "Laptop/florist.cpp"
    "Laptop/florist Cards.cpp"
    "Laptop/florist Gallery.cpp"
    "Laptop/florist Order Form.cpp"
    "Laptop/funeral.cpp")
  file(READ "${SOURCE_ROOT}/${florist_owned_resource_source}"
    florist_owned_resource_contents)
  string(FIND "${florist_owned_resource_contents}"
    "LaptopPageResourceOwner" florist_page_owner_position)
  if(florist_page_owner_position EQUAL -1)
    message(FATAL_ERROR
      "${florist_owned_resource_source} lost transactional page ownership")
  endif()
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(CHECKF[ \t]*\\([ \t]*)?(AddVideoObject|AddVideoSurface|DeleteVideoObjectFromIndex|DeleteVideoSurfaceFromIndex|LoadButtonImage|UnloadButtonImage|RemoveButton|MSYS_AddRegion|MSYS_RemoveRegion)[ \t]*\\("
    raw_florist_page_resource_lifecycle
    "${florist_owned_resource_contents}")
  if(raw_florist_page_resource_lifecycle)
    message(FATAL_ERROR
      "${florist_owned_resource_source} restored open-coded resource ownership '${raw_florist_page_resource_lifecycle}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/FloristSiteModel.h"
  runtime_florist_site_model_contents)
foreach(required_florist_model_fragment IN ITEMS
    "ClampFloristIndex"
    "itemCount == 0"
    "FloristGalleryPageCount"
    "NextFloristGalleryPageStart"
    "PreviousFloristGalleryPageStart"
    "FloristGalleryPageNumber"
    "CenteredFloristTextOffset"
    "struct FloristCardGrid"
    "struct FloristGalleryRow"
    "MakeFloristCardsLayout"
    "MakeFloristGalleryLayout")
  string(FIND "${runtime_florist_site_model_contents}"
    "${required_florist_model_fragment}" required_florist_model_position)
  if(required_florist_model_position EQUAL -1)
    message(FATAL_ERROR
      "Florist site model lost '${required_florist_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/florist Cards.cpp"
  runtime_florist_cards_contents)
foreach(required_florist_cards_layout_fragment IN ITEMS
    "CurrentFloristCardsLayout()"
    "layout.cards.card"
    "layout.title"
    "layout.backButton")
  string(FIND "${runtime_florist_cards_contents}"
    "${required_florist_cards_layout_fragment}"
    required_florist_cards_layout_position)
  if(required_florist_cards_layout_position EQUAL -1)
    message(FATAL_ERROR
      "Florist cards bypassed shared geometry '${required_florist_cards_layout_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/florist Gallery.cpp"
  runtime_florist_gallery_contents)
foreach(required_florist_gallery_fragment IN ITEMS
    "std::fill(std::begin(FloristGallerySubPagesVisitedFlag)"
    "FloristGallerySubPagesVisitedFlag[kFloristGalleryPageCount]"
    "gFloristGalleryFlowerResources.clear()"
    "gFloristGalleryFlowerResources = std::move(staged)"
    "NextFloristGalleryPageStart("
    "PreviousFloristGalleryPageStart("
    "FloristGalleryPageNumber("
    "CurrentFloristGalleryLayout()"
    "layout.row"
    "if (swscanf(sTemp, L\"%hu\", &usPrice) != 1) usPrice = 0")
  string(FIND "${runtime_florist_gallery_contents}"
    "${required_florist_gallery_fragment}"
    required_florist_gallery_position)
  if(required_florist_gallery_position EQUAL -1)
    message(FATAL_ERROR
      "Florist gallery safety lost '${required_florist_gallery_fragment}'")
  endif()
endforeach()
foreach(retired_florist_coordinate IN ITEMS
    "FLORIST_CARD_FIRST_POS_X"
    "FLORIST_CARD_FIRST_OFFSET_X"
    "FLOR_GALLERY_FLOWER_BUTTON_X"
    "FLOR_GALLERY_FLOWER_BUTTON_OFFSET_Y"
    "FLOR_GALLERY_FLOWER_TITLE_X")
  string(FIND
    "${runtime_florist_cards_contents}${runtime_florist_gallery_contents}"
    "${retired_florist_coordinate}"
    retired_florist_coordinate_position)
  if(NOT retired_florist_coordinate_position EQUAL -1)
    message(FATAL_ERROR
      "Florist restored retired coordinate family '${retired_florist_coordinate}'")
  endif()
endforeach()
string(FIND "${runtime_florist_gallery_contents}"
  "gFloristGalleryFlowerResources.clear()"
  florist_gallery_last_dynamic_clear REVERSE)
string(FIND "${runtime_florist_gallery_contents}"
  "gFloristGalleryFlowerResources = std::move(staged)"
  florist_gallery_last_dynamic_commit REVERSE)
if(florist_gallery_last_dynamic_clear EQUAL -1 OR
    florist_gallery_last_dynamic_commit EQUAL -1 OR
    florist_gallery_last_dynamic_clear GREATER
      florist_gallery_last_dynamic_commit)
  message(FATAL_ERROR
    "Florist gallery dynamic replacement lost dependency-ordered teardown")
endif()

file(READ "${SOURCE_ROOT}/Laptop/florist Order Form.cpp"
  runtime_florist_order_contents)
foreach(required_florist_order_fragment IN ITEMS
    "std::vector<MOUSE_REGION> gSelectedFlowerDropDownRegion"
    "RefreshFloristDestinations()"
    "gSelectedFlowerDropDownRegion.resize(gDestinationTable.size())"
    "gFloristDropDownResources.clear()"
    "CaptureFlowerOrderTextInputBoxes()"
    "gDestinationTable.empty()"
    ".flowerDeliveryMeanwhileAvailable()")
  string(FIND "${runtime_florist_order_contents}"
    "${required_florist_order_fragment}" required_florist_order_position)
  if(required_florist_order_position EQUAL -1)
    message(FATAL_ERROR
      "Florist order safety lost '${required_florist_order_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_florist_order_contents}" "#ifdef JA2UB"
  florist_compile_identity_position)
if(NOT florist_compile_identity_position EQUAL -1)
  message(FATAL_ERROR
    "Florist order restored campaign compile-identity selection")
endif()

foreach(florist_centering_caller_and_fragment IN ITEMS
    "Laptop/florist Cards.cpp|CenteredFloristTextOffset("
    "Laptop/funeral.cpp|CenteredFloristTextOffset(")
  string(REPLACE "|" ";" florist_centering_parts
    "${florist_centering_caller_and_fragment}")
  list(GET florist_centering_parts 0 florist_centering_source)
  list(GET florist_centering_parts 1 florist_centering_fragment)
  file(READ "${SOURCE_ROOT}/${florist_centering_source}"
    florist_centering_contents)
  string(FIND "${florist_centering_contents}"
    "${florist_centering_fragment}" florist_centering_position)
  if(florist_centering_position EQUAL -1)
    message(FATAL_ERROR
      "${florist_centering_source} bypassed bounded localized-text centering")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CampaignLaptopCommunicationsPolicy.h"
  runtime_florist_policy_contents)
string(FIND "${runtime_florist_policy_contents}"
  "flowerDeliveryMeanwhileAvailable" runtime_florist_policy_position)
if(runtime_florist_policy_position EQUAL -1)
  message(FATAL_ERROR
    "Laptop communications policy lost Florist delivery-scene selection")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_communications_policy_tests.cpp"
  runtime_florist_test_contents)
foreach(required_florist_test_fragment IN ITEMS
    "flower delivery meanwhile scenes remain Arulco-only"
    "Florist selections clamp empty and stale content indices"
    "Florist gallery navigation stays within its final partial page"
    "Florist and Funeral localized text centering cannot underflow"
    "Florist card drawing and hitboxes share one row-major grid"
    "Florist gallery buttons and descriptions share one row sequence"
    "Florist layout follows centered-screen offsets")
  string(FIND "${runtime_florist_test_contents}"
    "${required_florist_test_fragment}" required_florist_test_position)
  if(required_florist_test_position EQUAL -1)
    message(FATAL_ERROR
      "Florist tests lost '${required_florist_test_fragment}'")
  endif()
endforeach()

# The complete Insurance site stages shared/default and page-specific
# resources as one transaction. Its mutable roster/form controls have a
# separate replaceable owner, while dependency-free policy arithmetic keeps
# stale pages, rejected purchases, expired contracts, and payout counts safe.
foreach(insurance_owned_resource_source IN ITEMS
    "Laptop/insurance.cpp"
    "Laptop/insurance Info.cpp"
    "Laptop/insurance Contract.cpp"
    "Laptop/insurance Comments.cpp")
  file(READ "${SOURCE_ROOT}/${insurance_owned_resource_source}"
    insurance_owned_resource_contents)
  string(FIND "${insurance_owned_resource_contents}"
    "LaptopPageResourceOwner" insurance_page_owner_position)
  if(insurance_page_owner_position EQUAL -1)
    message(FATAL_ERROR
      "${insurance_owned_resource_source} lost transactional page ownership")
  endif()
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(CHECKF[ \t]*\\([ \t]*)?(AddVideoObject|AddVideoSurface|DeleteVideoObjectFromIndex|DeleteVideoSurfaceFromIndex|LoadButtonImage|UnloadButtonImage|RemoveButton|MSYS_AddRegion|MSYS_RemoveRegion)[ \t]*\\("
    raw_insurance_page_resource_lifecycle
    "${insurance_owned_resource_contents}")
  if(raw_insurance_page_resource_lifecycle)
    message(FATAL_ERROR
      "${insurance_owned_resource_source} restored open-coded resource ownership '${raw_insurance_page_resource_lifecycle}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/InsuranceSiteModel.h"
  runtime_insurance_site_model_contents)
foreach(required_insurance_model_fragment IN ITEMS
    "InsuranceContractPageStart"
    "InsuranceContractPageSize"
    "NextInsuranceContractPageStart"
    "PreviousInsuranceContractPageStart"
    "IsInsuranceInfoPage"
    "IsInsuranceMercProfile"
    "ValidateInsurancePurchase"
    "InsuranceCoverageAfterPurchase"
    "RemainingInsuranceEmploymentDays"
    "InsurancePayoutCountsAreConsistent"
    "InsurancePayoutStorageIsConsistent")
  string(FIND "${runtime_insurance_site_model_contents}"
    "${required_insurance_model_fragment}" required_insurance_model_position)
  if(required_insurance_model_position EQUAL -1)
    message(FATAL_ERROR
      "Insurance site model lost '${required_insurance_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/insurance Contract.cpp"
  runtime_insurance_contract_site_contents)
foreach(required_insurance_contract_fragment IN ITEMS
    "gInsuranceContractFormResources.clear()"
    "gInsuranceContractFormResources = std::move(stagedForms)"
    "gInsuranceMercProfiles"
    "NormalizeInsuranceContractPage()"
    "UniqueVideoObjectHandle faceImage"
    "ValidateInsurancePurchase(uiInsuranceLength"
    "InsuranceCoverageAfterPurchase("
    "InsurancePayoutStorageIsConsistent("
    "LaptopSaveInfo.ubNumberLifeInsurancePayouts = 0")
  string(FIND "${runtime_insurance_contract_site_contents}"
    "${required_insurance_contract_fragment}"
    required_insurance_contract_position)
  if(required_insurance_contract_position EQUAL -1)
    message(FATAL_ERROR
      "Insurance contract safety lost '${required_insurance_contract_fragment}'")
  endif()
endforeach()
foreach(retired_insurance_contract_fragment IN ITEMS
    "gubInsuranceMercArray"
    "gsForm1InsuranceLengthNumber"
    "CreateDestroyInsuranceContractFormButtons")
  string(FIND "${runtime_insurance_contract_site_contents}"
    "${retired_insurance_contract_fragment}"
    retired_insurance_contract_position)
  if(NOT retired_insurance_contract_position EQUAL -1)
    message(FATAL_ERROR
      "Insurance contract restored stale state '${retired_insurance_contract_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/insurance Info.cpp"
  runtime_insurance_info_site_contents)
foreach(required_insurance_info_fragment IN ITEMS
    "InsuranceInfoSubPagesVisitedFlag[ INS_INFO_LAST_PAGE ]"
    "std::fill(std::begin(InsuranceInfoSubPagesVisitedFlag)"
    "IsInsuranceInfoPage(ubSubPageNumber")
  string(FIND "${runtime_insurance_info_site_contents}"
    "${required_insurance_info_fragment}"
    required_insurance_info_position)
  if(required_insurance_info_position EQUAL -1)
    message(FATAL_ERROR
      "Insurance information-page safety lost '${required_insurance_info_fragment}'")
  endif()
endforeach()

foreach(required_insurance_test_build_fragment IN ITEMS
    "insurance_site_model_tests.cpp"
    "insurance_site_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_insurance_test_build_fragment}"
    required_insurance_test_build_position)
  if(required_insurance_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Insurance site model lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "insurance_site_model_tests" runtime_insurance_site_ci_position)
if(runtime_insurance_site_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Insurance site model test target")
endif()

file(READ "${SOURCE_ROOT}/tests/insurance_site_model_tests.cpp"
  runtime_insurance_site_test_contents)
foreach(required_insurance_test_fragment IN ITEMS
    "Insurance roster pages handle empty, stale, and final partial pages"
    "Insurance page and profile indices reject exact-end sentinels"
    "Insurance purchases validate every prerequisite before committing"
    "Expired employment cannot underflow into a huge insurance term"
    "Insurance payout counts and backing storage stay consistent")
  string(FIND "${runtime_insurance_site_test_contents}"
    "${required_insurance_test_fragment}"
    required_insurance_test_position)
  if(required_insurance_test_position EQUAL -1)
    message(FATAL_ERROR
      "Insurance site tests lost '${required_insurance_test_fragment}'")
  endif()
endforeach()

# Bobby Ray commerce uses one dependency-free capacity/value contract while
# the legacy page, save, and PostalService adapters retain their established
# serialized structures. Runtime configuration and corrupt persisted counts
# must never define the size of fixed arrays or publish partially read state.
file(READ "${SOURCE_ROOT}/Laptop/BobbyRayCommerceModel.h"
  runtime_bobby_ray_commerce_model_contents)
foreach(required_bobby_ray_commerce_model_fragment IN ITEMS
    "PurchaseCapacity = 100"
    "PurchaseLimit"
    "LegacyOrderCountsAreConsistent"
    "LegacyShipmentCountFits"
    "VisibleShipmentCount"
    "RemoveStock"
    "AddStock"
    "CountActiveOrders")
  string(FIND "${runtime_bobby_ray_commerce_model_contents}"
    "${required_bobby_ray_commerce_model_fragment}"
    required_bobby_ray_commerce_model_position)
  if(required_bobby_ray_commerce_model_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray commerce model lost '${required_bobby_ray_commerce_model_fragment}'")
  endif()
endforeach()

foreach(bobby_ray_bounded_purchase_source IN ITEMS
    "Laptop/BobbyRMailOrder.cpp"
    "Laptop/BobbyRGuns.cpp")
  file(READ "${SOURCE_ROOT}/${bobby_ray_bounded_purchase_source}"
    bobby_ray_bounded_purchase_contents)
  string(REGEX MATCHALL "ubBobbyRayMaxPurchaseAmount"
    bobby_ray_configured_purchase_uses
    "${bobby_ray_bounded_purchase_contents}")
  list(LENGTH bobby_ray_configured_purchase_uses
    bobby_ray_configured_purchase_use_count)
  if(NOT bobby_ray_configured_purchase_use_count EQUAL 1)
    message(FATAL_ERROR
      "${bobby_ray_bounded_purchase_source} bypassed the single bounded purchase-limit adapter")
  endif()
endforeach()

foreach(bobby_ray_commerce_caller_and_fragment IN ITEMS
    "Laptop/BobbyRGuns.cpp|GetConfiguredBobbyRayPurchaseLimit"
    "Laptop/BobbyR.cpp|BobbyRayCommerceModel::AddStock"
    "Laptop/BobbyRMailOrder.cpp|ImportOldBobbyRayOrders"
    "Laptop/BobbyRMailOrder.cpp|LegacyShipmentCountFits"
    "Laptop/BobbyRShipments.cpp|VisibleShipmentCount"
    "Laptop/laptop.cpp|LegacyOrderCountsAreConsistent"
    "Ja2/SaveLoadGame.cpp|ImportOldBobbyRayOrders"
    "Ja2/SaveLoadGame.cpp|make_unique<OLD_DEALER_ITEM_HEADER_101[]>"
    "Ja2/SaveLoadGame.cpp|SavedEmailSubjectAllocationDeleter"
    "Laptop/PostalService.cpp|loadedShipments"
    "Laptop/PostalService.cpp|PurchaseCapacity")
  string(REPLACE "|" ";" bobby_ray_commerce_caller_parts
    "${bobby_ray_commerce_caller_and_fragment}")
  list(GET bobby_ray_commerce_caller_parts 0 bobby_ray_commerce_caller)
  list(GET bobby_ray_commerce_caller_parts 1 bobby_ray_commerce_fragment)
  file(READ "${SOURCE_ROOT}/${bobby_ray_commerce_caller}"
    bobby_ray_commerce_caller_contents)
  string(FIND "${bobby_ray_commerce_caller_contents}"
    "${bobby_ray_commerce_fragment}" bobby_ray_commerce_caller_position)
  if(bobby_ray_commerce_caller_position EQUAL -1)
    message(FATAL_ERROR
      "${bobby_ray_commerce_caller} bypassed ${bobby_ray_commerce_fragment}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/BobbyR.cpp"
  runtime_bobby_ray_storefront_contents)
foreach(retired_bobby_ray_inventory_sentinel IN ITEMS
    "BobbyRayInventory[ usBobbyrIndex ].usItemIndex = BOBBYR_NO_ITEMS"
    "BobbyRayUsedInventory[ usBobbyrIndex ].usItemIndex = BOBBYR_NO_ITEMS")
  string(FIND "${runtime_bobby_ray_storefront_contents}"
    "${retired_bobby_ray_inventory_sentinel}"
    retired_bobby_ray_inventory_sentinel_position)
  if(NOT retired_bobby_ray_inventory_sentinel_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray restored an exact-end inventory sentinel write")
  endif()
endforeach()

foreach(required_bobby_ray_commerce_test_fragment IN ITEMS
    "bobby_ray_commerce_model_tests.cpp"
    "bobby_ray_commerce_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_bobby_ray_commerce_test_fragment}"
    required_bobby_ray_commerce_test_position)
  if(required_bobby_ray_commerce_test_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray commerce model lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "bobby_ray_commerce_model_tests" runtime_bobby_ray_commerce_ci_position)
if(runtime_bobby_ray_commerce_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Bobby Ray commerce model test target")
endif()

file(READ "${SOURCE_ROOT}/tests/bobby_ray_commerce_model_tests.cpp"
  runtime_bobby_ray_commerce_test_contents)
foreach(required_bobby_ray_commerce_assertion IN ITEMS
    "PurchaseLimit(255)"
    "!IsIndexInBoundedList(10, 10, 10)"
    "!LegacyOrderCountsAreConsistent(3, 4)"
    "LegacyShipmentCountFits(-1"
    "MaximumLegacyShipmentSlots + 1"
    "VisibleShipmentCount(20, 18, 13)"
    "RemoveStock(5, 8)"
    "AddStock(250, 8)"
    "CountActiveOrders<Order>(nullptr, 3)")
  string(FIND "${runtime_bobby_ray_commerce_test_contents}"
    "${required_bobby_ray_commerce_assertion}"
    required_bobby_ray_commerce_assertion_position)
  if(required_bobby_ray_commerce_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray commerce tests lost '${required_bobby_ray_commerce_assertion}'")
  endif()
endforeach()

# Bobby Ray's five catalogue pages share one staged resource lifecycle and a
# dependency-free pagination/input model. Page exits must release registered
# regions and buttons before their backing images, while empty/stale pages,
# queued callbacks, malformed LBE data, and temporary item graphics stay safe.
foreach(bobby_ray_catalogue_source IN ITEMS
    "Laptop/BobbyRGuns.cpp"
    "Laptop/BobbyRAmmo.cpp"
    "Laptop/BobbyRArmour.cpp"
    "Laptop/BobbyRMisc.cpp"
    "Laptop/BobbyRUsed.cpp")
  file(READ "${SOURCE_ROOT}/${bobby_ray_catalogue_source}"
    bobby_ray_catalogue_source_contents)
  string(FIND "${bobby_ray_catalogue_source_contents}"
    "LaptopPageResourceOwner" bobby_ray_catalogue_owner_position)
  if(bobby_ray_catalogue_owner_position EQUAL -1)
    message(FATAL_ERROR
      "${bobby_ray_catalogue_source} lost transactional page ownership")
  endif()
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(CHECKF[ \t]*\\([ \t]*)?(AddVideoObject|AddVideoSurface|DeleteVideoObjectFromIndex|DeleteVideoSurfaceFromIndex|LoadButtonImage|UnloadButtonImage|RemoveButton|MSYS_AddRegion|MSYS_RemoveRegion)[ \t]*\\("
    raw_bobby_ray_catalogue_resource_lifecycle
    "${bobby_ray_catalogue_source_contents}")
  if(raw_bobby_ray_catalogue_resource_lifecycle)
    message(FATAL_ERROR
      "${bobby_ray_catalogue_source} restored open-coded resource ownership '${raw_bobby_ray_catalogue_resource_lifecycle}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/BobbyRayCatalogueModel.h"
  runtime_bobby_ray_catalogue_model_contents)
foreach(required_bobby_ray_catalogue_model_fragment IN ITEMS
    "ItemsPerPage = 4"
    "PageCount"
    "NormalizePage"
    "NextPage"
    "PreviousPage"
    "VisibleItemCount"
    "DisplayPageNumber"
    "IsVisibleItemSlot"
    "IsCatalogueIndexInBounds")
  string(FIND "${runtime_bobby_ray_catalogue_model_contents}"
    "${required_bobby_ray_catalogue_model_fragment}"
    required_bobby_ray_catalogue_model_position)
  if(required_bobby_ray_catalogue_model_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray catalogue model lost '${required_bobby_ray_catalogue_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/BobbyRGuns.cpp"
  runtime_bobby_ray_catalogue_contents)
foreach(required_bobby_ray_catalogue_fragment IN ITEMS
    "InitBobbyMenuBar(LaptopPageResourceOwner& owner)"
    "gBobbyRBigImageResources"
    "gBobbyRBigImageResources = std::move(stagedResources)"
    "UniqueVideoObjectHandle itemImage"
    "IsLoadedBobbyRayItem"
    "BobbyRayCommerceModel::BoundedLength"
    "BobbyRayCatalogueModel::NormalizePage"
    "BobbyRayCatalogueModel::IsVisibleItemSlot"
    "gubNumItemsOnScreen != ubCount"
    "LoadBearingEquipment.size()"
    "lbePocketIndex.size()"
    "firstPocketIndex, LBEPocketType.size()"
    "pocketNum.size()")
  string(FIND "${runtime_bobby_ray_catalogue_contents}"
    "${required_bobby_ray_catalogue_fragment}"
    required_bobby_ray_catalogue_position)
  if(required_bobby_ray_catalogue_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray catalogue safety lost '${required_bobby_ray_catalogue_fragment}'")
  endif()
endforeach()
foreach(bobby_ray_catalogue_page_counter IN ITEMS
    "gubCurPage"
    "gubNumPages")
  string(REGEX MATCH
    "UINT(16|32|64)[ \t]+${bobby_ray_catalogue_page_counter}"
    widened_bobby_ray_catalogue_page_counter
    "${runtime_bobby_ray_catalogue_contents}")
  if(NOT widened_bobby_ray_catalogue_page_counter)
    message(FATAL_ERROR
      "Bobby Ray catalogue page counter '${bobby_ray_catalogue_page_counter}' narrowed below 16 bits")
  endif()
endforeach()
foreach(retired_bobby_ray_catalogue_fragment IN ITEMS
    "DeleteBobbyRGunsFilter"
    "DeleteBobbyRAmmoFilter"
    "DeleteBobbyRArmourFilter"
    "DeleteBobbyRMiscFilter"
    "DeleteBobbyRUsedFilter"
    "DeleteBobbyMenuBar")
  string(FIND "${runtime_bobby_ray_catalogue_contents}"
    "${retired_bobby_ray_catalogue_fragment}"
    retired_bobby_ray_catalogue_position)
  if(NOT retired_bobby_ray_catalogue_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray catalogue restored raw teardown '${retired_bobby_ray_catalogue_fragment}'")
  endif()
endforeach()

foreach(required_bobby_ray_catalogue_test_build_fragment IN ITEMS
    "bobby_ray_catalogue_model_tests.cpp"
    "bobby_ray_catalogue_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_bobby_ray_catalogue_test_build_fragment}"
    required_bobby_ray_catalogue_test_build_position)
  if(required_bobby_ray_catalogue_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray catalogue model lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "bobby_ray_catalogue_model_tests" runtime_bobby_ray_catalogue_ci_position)
if(runtime_bobby_ray_catalogue_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Bobby Ray catalogue model test target")
endif()

file(READ "${SOURCE_ROOT}/tests/bobby_ray_catalogue_model_tests.cpp"
  runtime_bobby_ray_catalogue_test_contents)
foreach(required_bobby_ray_catalogue_test_fragment IN ITEMS
    "PageCount(1024) == 256"
    "Catalogue page counts cover empty, full, and partial pages"
    "Catalogue navigation clamps empty and stale page selections"
    "Catalogue presentation normalizes empty and final partial pages"
    "Catalogue hotkeys and callbacks reject hidden item slots"
    "Catalogue data indices reject the exact-end value")
  string(FIND "${runtime_bobby_ray_catalogue_test_contents}"
    "${required_bobby_ray_catalogue_test_fragment}"
    required_bobby_ray_catalogue_test_position)
  if(required_bobby_ray_catalogue_test_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray catalogue tests lost '${required_bobby_ray_catalogue_test_fragment}'")
  endif()
endforeach()

# Bobby Ray's home, order, and shipment pages form one fulfilment lifecycle.
# Page resources and callback-replaced region sets must be owned, while empty
# or changing external destination/shipment data stays behind a data-free
# selection/window model.
foreach(bobby_ray_fulfilment_source IN ITEMS
    "Laptop/BobbyR.cpp"
    "Laptop/BobbyRMailOrder.cpp"
    "Laptop/BobbyRShipments.cpp")
  file(READ "${SOURCE_ROOT}/${bobby_ray_fulfilment_source}"
    bobby_ray_fulfilment_source_contents)
  string(FIND "${bobby_ray_fulfilment_source_contents}"
    "LaptopPageResourceOwner" bobby_ray_fulfilment_owner_position)
  if(bobby_ray_fulfilment_owner_position EQUAL -1)
    message(FATAL_ERROR
      "${bobby_ray_fulfilment_source} lost transactional page ownership")
  endif()
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(CHECKF[ \t]*\\([ \t]*)?(AddVideoObject|AddVideoSurface|DeleteVideoObjectFromIndex|DeleteVideoSurfaceFromIndex|LoadButtonImage|UnloadButtonImage|RemoveButton|MSYS_AddRegion|MSYS_RemoveRegion)[ \t]*\\("
    raw_bobby_ray_fulfilment_resource_lifecycle
    "${bobby_ray_fulfilment_source_contents}")
  if(raw_bobby_ray_fulfilment_resource_lifecycle)
    message(FATAL_ERROR
      "${bobby_ray_fulfilment_source} restored open-coded resource ownership '${raw_bobby_ray_fulfilment_resource_lifecycle}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/BobbyRayFulfilmentModel.h"
  runtime_bobby_ray_fulfilment_model_contents)
foreach(required_bobby_ray_fulfilment_model_fragment IN ITEMS
    "NoSelection"
    "VisibleCount"
    "NormalizeWindowStart"
    "NormalizeSelection"
    "IndexForVisibleSlot"
    "NextSelection"
    "PreviousSelection"
    "IndexForMatchingSlot")
  string(FIND "${runtime_bobby_ray_fulfilment_model_contents}"
    "${required_bobby_ray_fulfilment_model_fragment}"
    required_bobby_ray_fulfilment_model_position)
  if(required_bobby_ray_fulfilment_model_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray fulfilment model lost '${required_bobby_ray_fulfilment_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/BobbyRMailOrder.cpp"
  runtime_bobby_ray_fulfilment_order_contents)
foreach(required_bobby_ray_fulfilment_order_fragment IN ITEMS
    "gBobbyRMailOrderResources"
    "gBobbyRDropDownResources"
    "gBobbyROrderGridResources"
    "std::vector<MOUSE_REGION> gSelectedDropDownRegions"
    "std::vector<MOUSE_REGION> gSelectedGridScrollColumnRegions"
    "RefreshBobbyRayDestinationSnapshot"
    "ClearBobbyRayOrderGridMouseRegions"
    "BobbyRayFulfilmentModel::IndexForVisibleSlot")
  string(FIND "${runtime_bobby_ray_fulfilment_order_contents}"
    "${required_bobby_ray_fulfilment_order_fragment}"
    required_bobby_ray_fulfilment_order_position)
  if(required_bobby_ray_fulfilment_order_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray order safety lost '${required_bobby_ray_fulfilment_order_fragment}'")
  endif()
endforeach()
foreach(retired_bobby_ray_fulfilment_fragment IN ITEMS
    "new MOUSE_REGION"
    "delete [] gSelected"
    "DestroyBobbyROrderTitle"
    "DeleteBobbyRWoodBackground")
  foreach(bobby_ray_fulfilment_source IN ITEMS
      "Laptop/BobbyR.cpp"
      "Laptop/BobbyRMailOrder.cpp"
      "Laptop/BobbyRShipments.cpp")
    file(READ "${SOURCE_ROOT}/${bobby_ray_fulfilment_source}"
      retired_bobby_ray_fulfilment_source_contents)
    string(FIND "${retired_bobby_ray_fulfilment_source_contents}"
      "${retired_bobby_ray_fulfilment_fragment}"
      retired_bobby_ray_fulfilment_position)
    if(NOT retired_bobby_ray_fulfilment_position EQUAL -1)
      message(FATAL_ERROR
        "Bobby Ray fulfilment restored '${retired_bobby_ray_fulfilment_fragment}'")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/BobbyRShipments.cpp"
  runtime_bobby_ray_fulfilment_shipment_contents)
foreach(required_bobby_ray_fulfilment_shipment_fragment IN ITEMS
    "gBobbyRShipmentResources"
    "gBobbyRPreviousShipmentRegionResources"
    "staged.addVideoObject(&VObjectDesc, guiBobbyROrderGrid)"
    "BobbyRayFulfilmentModel::IndexForMatchingSlot")
  string(FIND "${runtime_bobby_ray_fulfilment_shipment_contents}"
    "${required_bobby_ray_fulfilment_shipment_fragment}"
    required_bobby_ray_fulfilment_shipment_position)
  if(required_bobby_ray_fulfilment_shipment_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray shipment safety lost '${required_bobby_ray_fulfilment_shipment_fragment}'")
  endif()
endforeach()

foreach(required_bobby_ray_fulfilment_test_build_fragment IN ITEMS
    "bobby_ray_fulfilment_model_tests.cpp"
    "bobby_ray_fulfilment_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_bobby_ray_fulfilment_test_build_fragment}"
    required_bobby_ray_fulfilment_test_build_position)
  if(required_bobby_ray_fulfilment_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray fulfilment model lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "bobby_ray_fulfilment_model_tests"
  runtime_bobby_ray_fulfilment_ci_position)
if(runtime_bobby_ray_fulfilment_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Bobby Ray fulfilment model test target")
endif()

file(READ "${SOURCE_ROOT}/tests/bobby_ray_fulfilment_model_tests.cpp"
  runtime_bobby_ray_fulfilment_test_contents)
foreach(required_bobby_ray_fulfilment_test_fragment IN ITEMS
    "Fulfilment lists cap visible rows without inventing empty rows"
    "Fulfilment list windows normalize empty, short, and stale starts"
    "Fulfilment selections reject empty and exact-end indices"
    "Fulfilment callbacks reject hidden and stale visible slots"
    "Fulfilment keyboard navigation stays within empty and list bounds"
    "Shipment rows map visible slots to sparse live records safely")
  string(FIND "${runtime_bobby_ray_fulfilment_test_contents}"
    "${required_bobby_ray_fulfilment_test_fragment}"
    required_bobby_ray_fulfilment_test_position)
  if(required_bobby_ray_fulfilment_test_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray fulfilment tests lost '${required_bobby_ray_fulfilment_test_fragment}'")
  endif()
endforeach()

# Bobby Ray presentation geometry is shared by homepage artwork/hitboxes, all
# catalogue hosts, mail-order drawing/input/invalidation, and shipment rows.
# Keep those relationships in one dependency-free layout rather than allowing
# independent coordinate arrays or page-local row arithmetic to return.
file(READ "${SOURCE_ROOT}/Laptop/BobbyRayLayout.h"
  runtime_bobby_ray_layout_contents)
foreach(required_bobby_ray_layout_fragment IN ITEMS
    "struct HomeLayout"
    "SignCount = 5"
    "struct CatalogueLayout"
    "ItemCount = 4"
    "itemQuantity"
    "itemPurchasedCount"
    "itemQuality"
    "struct OrderGridLayout"
    "VisibleRowCount = 10"
    "struct MailOrderLayout"
    "ShippingSpeedCount = 3"
    "destinationRow"
    "struct ShipmentLayout"
    "VisibleRowCount = 13"
    "MakeHomeLayout"
    "MakeCatalogueLayout"
    "MakeOrderGridLayout"
    "MakeMailOrderLayout"
    "MakeShipmentLayout")
  string(FIND "${runtime_bobby_ray_layout_contents}"
    "${required_bobby_ray_layout_fragment}"
    required_bobby_ray_layout_position)
  if(required_bobby_ray_layout_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray layout lost '${required_bobby_ray_layout_fragment}'")
  endif()
endforeach()

foreach(bobby_ray_layout_caller_and_fragment IN ITEMS
    "Laptop/BobbyR.cpp|layout.signs"
    "Laptop/BobbyR.cpp|layout.underConstruction.at"
    "Laptop/BobbyRGuns.cpp|layout.itemImage(i)"
    "Laptop/BobbyRGuns.cpp|layout.filterButton"
    "Laptop/BobbyRGuns.cpp|GetBobbyRayCatalogueColumns"
    "Laptop/BobbyRAmmo.cpp|DrawBobbyRayCatalogueBackground"
    "Laptop/BobbyRArmour.cpp|DrawBobbyRayCatalogueBackground"
    "Laptop/BobbyRMisc.cpp|DrawBobbyRayCatalogueBackground"
    "Laptop/BobbyRUsed.cpp|DrawBobbyRayCatalogueBackground"
    "Laptop/BobbyRMailOrder.cpp|layout.shippingSpeedLights.at"
    "Laptop/BobbyRMailOrder.cpp|layout.destinationRow"
    "Laptop/BobbyRMailOrder.cpp|MakeOrderGridLayout"
    "Laptop/BobbyRShipments.cpp|layout.rows.at")
  string(REPLACE "|" ";" bobby_ray_layout_caller_parts
    "${bobby_ray_layout_caller_and_fragment}")
  list(GET bobby_ray_layout_caller_parts 0 bobby_ray_layout_caller)
  list(GET bobby_ray_layout_caller_parts 1 bobby_ray_layout_fragment)
  file(READ "${SOURCE_ROOT}/${bobby_ray_layout_caller}"
    bobby_ray_layout_caller_contents)
  string(FIND "${bobby_ray_layout_caller_contents}"
    "${bobby_ray_layout_fragment}" bobby_ray_layout_caller_position)
  if(bobby_ray_layout_caller_position EQUAL -1)
    message(FATAL_ERROR
      "${bobby_ray_layout_caller} bypassed ${bobby_ray_layout_fragment}")
  endif()
endforeach()

foreach(retired_bobby_ray_layout_fragment IN ITEMS
    "gShippingSpeedAreas"
    "usMouseRegionPosArray"
    "BOBBYR_ITEM_DESC_START_X"
    "BOBBYR_ORDERGRID_X"
    "#define\t\tBOBBYR_SHIPMENT_ORDER_NUM_X"
    "#define\t\tBOBBYR_SHIPMENT_GAP_BTN_LINES")
  foreach(bobby_ray_layout_source IN ITEMS
      "Laptop/BobbyR.cpp"
      "Laptop/BobbyRGuns.cpp"
      "Laptop/BobbyRMailOrder.cpp"
      "Laptop/BobbyRShipments.cpp")
    file(READ "${SOURCE_ROOT}/${bobby_ray_layout_source}"
      retired_bobby_ray_layout_source_contents)
    string(FIND "${retired_bobby_ray_layout_source_contents}"
      "${retired_bobby_ray_layout_fragment}"
      retired_bobby_ray_layout_position)
    if(NOT retired_bobby_ray_layout_position EQUAL -1)
      message(FATAL_ERROR
        "Bobby Ray restored split layout state '${retired_bobby_ray_layout_fragment}'")
    endif()
  endforeach()
endforeach()

foreach(required_bobby_ray_layout_test_build_fragment IN ITEMS
    "bobby_ray_layout_tests.cpp"
    "bobby_ray_layout")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_bobby_ray_layout_test_build_fragment}"
    required_bobby_ray_layout_test_build_position)
  if(required_bobby_ray_layout_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray layout lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "bobby_ray_layout_tests" runtime_bobby_ray_layout_ci_position)
if(runtime_bobby_ray_layout_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Bobby Ray layout test target")
endif()

file(READ "${SOURCE_ROOT}/tests/bobby_ray_layout_tests.cpp"
  runtime_bobby_ray_layout_test_contents)
foreach(required_bobby_ray_layout_test_fragment IN ITEMS
    "Bobby Ray sign drawing and hitboxes use one authored sign table"
    "catalogue artwork and item hitboxes share the four-row layout"
    "shipping labels, hitboxes, drawing, and invalidation share one stride"
    "destination drawing and mouse regions use one drop-down layout"
    "order-grid scroll drawing and mouse regions share one geometry"
    "shipment drawing and all thirteen row hitboxes share one sequence"
    "Bobby Ray geometry follows centered-screen and web-page offsets")
  string(FIND "${runtime_bobby_ray_layout_test_contents}"
    "${required_bobby_ray_layout_test_fragment}"
    required_bobby_ray_layout_test_position)
  if(required_bobby_ray_layout_test_position EQUAL -1)
    message(FATAL_ERROR
      "Bobby Ray layout tests lost '${required_bobby_ray_layout_test_fragment}'")
  endif()
endforeach()

# Laptop XML/localization input is staged behind one dependency-free boundary
# model and one legacy UTF-8 adapter. Expat may split character data at any
# byte, and malformed or oversized data must not narrow, overflow, address an
# exact-end slot, or partially publish live Laptop state. The one established
# campaign-news truncation is an explicit adapter policy with integration
# coverage rather than an unchecked conversion side effect.
file(READ "${SOURCE_ROOT}/Laptop/LocalizationInputModel.h"
  runtime_laptop_localization_model_contents)
foreach(required_laptop_localization_model_fragment IN ITEMS
    "AppendText"
    "ParseInteger"
    "ParseIntegerOrMinusOneSentinel"
    "ParseBoolean"
    "IsIndexInRange"
    "CopyText"
    "std::from_chars")
  string(FIND "${runtime_laptop_localization_model_contents}"
    "${required_laptop_localization_model_fragment}"
    required_laptop_localization_model_position)
  if(required_laptop_localization_model_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop localization input model lost '${required_laptop_localization_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/LocalizationInputAdapter.h"
  runtime_laptop_localization_adapter_contents)
foreach(required_laptop_localization_adapter_fragment IN ITEMS
    "ConvertUtf8"
    "ConvertUtf8WithPolicy"
    "TextOverflowPolicy::Reject"
    "ConvertUtf8Truncated"
    "required <= 0"
    "std::array<CHAR16, Capacity> converted"
    "std::vector<CHAR16> converted"
    "std::array<CHAR16, Capacity> staged"
    "std::copy(staged.begin(), staged.end(), destination)")
  string(FIND "${runtime_laptop_localization_adapter_contents}"
    "${required_laptop_localization_adapter_fragment}"
    required_laptop_localization_adapter_position)
  if(required_laptop_localization_adapter_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop localization adapter lost '${required_laptop_localization_adapter_fragment}'")
  endif()
endforeach()

file(GLOB runtime_laptop_xml_input_sources
  "${SOURCE_ROOT}/Laptop/XML_*.cpp")
foreach(runtime_laptop_xml_input_source IN LISTS
    runtime_laptop_xml_input_sources)
  file(READ "${runtime_laptop_xml_input_source}"
    runtime_laptop_xml_input_contents)
  foreach(required_laptop_xml_input_fragment IN ITEMS
      "LocalizationInputAdapter.h"
      "LaptopLocalizationModel::AppendText"
      "pData.valid"
      "pending")
    string(FIND "${runtime_laptop_xml_input_contents}"
      "${required_laptop_xml_input_fragment}"
      required_laptop_xml_input_position)
    if(required_laptop_xml_input_position EQUAL -1)
      message(FATAL_ERROR
        "${runtime_laptop_xml_input_source} bypassed transactional Laptop XML input '${required_laptop_xml_input_fragment}'")
    endif()
  endforeach()

  string(REGEX MATCH
    "(strncat|wcscpy|MultiByteToWideChar|atol|atoi|strtoul)[ \t\r\n]*\\("
    retired_laptop_xml_input_operation
    "${runtime_laptop_xml_input_contents}")
  if(retired_laptop_xml_input_operation)
    message(FATAL_ERROR
      "${runtime_laptop_xml_input_source} restored unchecked text conversion or numeric narrowing '${retired_laptop_xml_input_operation}'")
  endif()
  string(REGEX MATCH "[A-Za-z0-9_]+_TextOnly"
    retired_laptop_xml_global_mode
    "${runtime_laptop_xml_input_contents}")
  if(retired_laptop_xml_global_mode)
    message(FATAL_ERROR
      "${runtime_laptop_xml_input_source} restored mutable parser mode '${retired_laptop_xml_global_mode}'")
  endif()
endforeach()

foreach(laptop_xml_boundary_caller_and_fragment IN ITEMS
    "Laptop/XML_BriefingRoom.cpp|destinationSize"
    "Laptop/XML_BriefingRoom.cpp|uiBytesRead != SIZE_MERC_BIO_INFO"
    "Laptop/XML_BriefingRoom.cpp|SIZE_MERC_ADDITIONAL_INFO / 2"
    "Laptop/XML_AIMAvailability.cpp|ParseIntegerOrMinusOneSentinel"
    "Laptop/XML_ConditionsForMercAvailability.cpp|ParseIntegerOrMinusOneSentinel"
    "Laptop/XML_OldAIMArchive.cpp|std::array<OLD_MERC_ARCHIVES_VALUES, NUM_PROFILES>"
    "Laptop/XML_CampaignStatsEvents.cpp|ConvertUtf8Truncated"
    "Laptop/XML_ShippingDestinations.cpp|IsShippingDestinationRecordValid"
    "Laptop/XML_ShippingDestinations.cpp|found == liveDestinations.end()"
    "Laptop/XML_DeliveryMethods.cpp|GetDeliveryMethodCount() != 0"
    "Ja2/Init.cpp|gBriefingRoomData, NUM_MISSION, 4")
  string(REPLACE "|" ";" laptop_xml_boundary_caller_parts
    "${laptop_xml_boundary_caller_and_fragment}")
  list(GET laptop_xml_boundary_caller_parts 0 laptop_xml_boundary_caller)
  list(GET laptop_xml_boundary_caller_parts 1 laptop_xml_boundary_fragment)
  file(READ "${SOURCE_ROOT}/${laptop_xml_boundary_caller}"
    laptop_xml_boundary_caller_contents)
  string(FIND "${laptop_xml_boundary_caller_contents}"
    "${laptop_xml_boundary_fragment}" laptop_xml_boundary_caller_position)
  if(laptop_xml_boundary_caller_position EQUAL -1)
    message(FATAL_ERROR
      "${laptop_xml_boundary_caller} lost Laptop XML boundary '${laptop_xml_boundary_fragment}'")
  endif()
endforeach()

foreach(required_laptop_localization_test_fragment IN ITEMS
    "laptop_localization_input_model_tests.cpp"
    "laptop_localization_input_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_localization_test_fragment}"
    required_laptop_localization_test_position)
  if(required_laptop_localization_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop localization input model lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_localization_input_model_tests"
  runtime_laptop_localization_ci_position)
if(runtime_laptop_localization_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop localization input model test target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_localization_input_model_tests.cpp"
  runtime_laptop_localization_test_contents)
foreach(required_laptop_localization_assertion IN ITEMS
    "AppendText(text, \"cde\", 3)"
    "AppendText(unterminated, \"x\", 1)"
    "ParseInteger(\"256\", byte)"
    "ParseInteger(\"-129\", value)"
    "ParseIntegerOrMinusOneSentinel(\"-1\", value)"
    "ParseIntegerOrMinusOneSentinel(\"-2\", value)"
    "ParseBoolean(\"2\", flag)"
    "IsIndexInRange(255, 255)"
    "IsShippingDestinationRecordValid("
    "false, true, true, false, false, false, false"
    "locationFields & 8"
    "CopyText(destination, unterminated)")
  string(FIND "${runtime_laptop_localization_test_contents}"
    "${required_laptop_localization_assertion}"
    required_laptop_localization_assertion_position)
  if(required_laptop_localization_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop localization tests lost '${required_laptop_localization_assertion}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  runtime_laptop_xml_integration_test_contents)
foreach(required_laptop_xml_integration_fragment IN ITEMS
    "std::string oversizedText( 304, 'x' )"
    "campaign-stats XML preserves the established bounded text truncation schema")
  string(FIND "${runtime_laptop_xml_integration_test_contents}"
    "${required_laptop_xml_integration_fragment}"
    required_laptop_xml_integration_position)
  if(required_laptop_xml_integration_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop XML headless integration lost '${required_laptop_xml_integration_fragment}'")
  endif()
endforeach()

# IMP creation owns navigation and bounded selection in one dependency-free
# model. Legacy pages may observe the current page through a read-only
# compatibility view, but only CharProfile may mutate it. Profile creation and
# import must validate slots, portraits, and complete file records before
# publishing live mercenary state.
file(READ "${SOURCE_ROOT}/Laptop/ImpCreationStateModel.h"
  runtime_laptop_imp_creation_model_contents)
foreach(required_laptop_imp_creation_model_fragment IN ITEMS
    "ScopedRollback"
    "NavigationState"
    "static_assert(PageCount > 0"
    "IsPageValid"
    "RequestPage"
    "ResetVisited"
    "IsIndexInRange"
    "FindFirstMatchingIndex"
    "FindPreferredOrFirstMatchingIndex"
    "FindNextMatchingIndex"
    "FindPreviousMatchingIndex")
  string(FIND "${runtime_laptop_imp_creation_model_contents}"
    "${required_laptop_imp_creation_model_fragment}"
    required_laptop_imp_creation_model_position)
  if(required_laptop_imp_creation_model_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop IMP creation model lost '${required_laptop_imp_creation_model_fragment}'")
  endif()
endforeach()

foreach(laptop_imp_boundary_caller_and_fragment IN ITEMS
    "Laptop/CharProfile.h|extern const INT32& iCurrentImpPage"
    "Laptop/CharProfile.cpp|gImpNavigation.RequestPage(page)"
    "Laptop/CharProfile.cpp|ResetSelectedIMPGear()"
    "Laptop/IMP MainPage.cpp|FindPreferredOrFirstMatchingIndex"
    "Laptop/IMP Portraits.cpp|FindNextMatchingIndex"
    "Laptop/IMP Portraits.cpp|IsSelectableIMPPortraitForGender"
    "Laptop/IMP Voices.cpp|FindPreviousMatchingIndex"
    "Laptop/IMP Compile Character.cpp|IsIMPSlotFree(profileId)"
    "Laptop/IMP Compile Character.cpp|LaptopLocalizationModel::CopyText"
    "Laptop/IMP Confirm.cpp|class ScopedImpFile"
    "Laptop/IMP Confirm.cpp|ReadImpFileExact"
    "Laptop/IMP Confirm.cpp|MERCPROFILESTRUCT pendingProfile"
    "Laptop/IMP Confirm.cpp|pendingProfile.Type != PROFILETYPE_IMP"
    "Laptop/IMP Confirm.cpp|ScopedRollback<MERCPROFILESTRUCT>"
    "Laptop/IMP Confirm.cpp|profileId == -1"
    "Laptop/IMP Confirm.cpp|WriteImpFileExact"
    "Laptop/XML_IMPPortraits.cpp|std::fill_n(gIMPValues")
  string(REPLACE "|" ";" laptop_imp_boundary_caller_parts
    "${laptop_imp_boundary_caller_and_fragment}")
  list(GET laptop_imp_boundary_caller_parts 0 laptop_imp_boundary_caller)
  list(GET laptop_imp_boundary_caller_parts 1 laptop_imp_boundary_fragment)
  file(READ "${SOURCE_ROOT}/${laptop_imp_boundary_caller}"
    laptop_imp_boundary_caller_contents)
  string(FIND "${laptop_imp_boundary_caller_contents}"
    "${laptop_imp_boundary_fragment}" laptop_imp_boundary_caller_position)
  if(laptop_imp_boundary_caller_position EQUAL -1)
    message(FATAL_ERROR
      "${laptop_imp_boundary_caller} lost IMP creation boundary '${laptop_imp_boundary_fragment}'")
  endif()
endforeach()

file(GLOB runtime_laptop_imp_page_sources
  "${SOURCE_ROOT}/Laptop/*.h"
  "${SOURCE_ROOT}/Laptop/*.cpp")
foreach(runtime_laptop_imp_page_source IN LISTS runtime_laptop_imp_page_sources)
  if(runtime_laptop_imp_page_source STREQUAL
      "${SOURCE_ROOT}/Laptop/CharProfile.cpp")
    continue()
  endif()
  file(READ "${runtime_laptop_imp_page_source}"
    runtime_laptop_imp_page_contents)
  string(REGEX MATCH "iCurrentImpPage[ \t\r\n]*=[^=]"
    retired_laptop_imp_page_assignment "${runtime_laptop_imp_page_contents}")
  if(retired_laptop_imp_page_assignment)
    message(FATAL_ERROR
      "${runtime_laptop_imp_page_source} restored direct IMP page mutation; use RequestIMPPage")
  endif()

  if(runtime_laptop_imp_page_source MATCHES "/Laptop/IMP[ _]")
    string(REGEX MATCH
      "mprintf[ \t\r\n]*\\([^,;]*,[^,;]*,[ \t\r\n]*[A-KM-Za-km-z_][A-Za-z0-9_]*(\\[[^]]*\\])?[ \t\r\n]*\\)"
      retired_laptop_imp_variable_format "${runtime_laptop_imp_page_contents}")
    if(retired_laptop_imp_variable_format)
      message(FATAL_ERROR
        "${runtime_laptop_imp_page_source} passes IMP text as an mprintf format string; use an explicit %s format")
    endif()
  endif()
endforeach()

foreach(retired_laptop_imp_creation_fragment IN ITEMS
    "sFacePositions"
    "char zImpFileName["
    "char zFileName[")
  file(READ "${SOURCE_ROOT}/Laptop/IMP Confirm.cpp"
    retired_laptop_imp_confirm_contents)
  file(READ "${SOURCE_ROOT}/Laptop/IMP Compile Character.cpp"
    retired_laptop_imp_compile_contents)
  string(FIND
    "${retired_laptop_imp_confirm_contents}${retired_laptop_imp_compile_contents}"
    "${retired_laptop_imp_creation_fragment}"
    retired_laptop_imp_creation_position)
  if(NOT retired_laptop_imp_creation_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop IMP creation restored retired unsafe storage '${retired_laptop_imp_creation_fragment}'")
  endif()
endforeach()

foreach(required_laptop_imp_test_fragment IN ITEMS
    "laptop_imp_creation_state_model_tests.cpp"
    "laptop_imp_creation_state_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_imp_test_fragment}"
    required_laptop_imp_test_position)
  if(required_laptop_imp_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop IMP creation model lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_imp_creation_state_model_tests"
  runtime_laptop_imp_creation_ci_position)
if(runtime_laptop_imp_creation_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop IMP creation model test target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_imp_creation_state_model_tests.cpp"
  runtime_laptop_imp_creation_test_contents)
foreach(required_laptop_imp_test_assertion IN ITEMS
    "ScopedRollback<int> rollback"
    "publishedValue == 7"
    "publishedValue == 23"
    "navigation.RequestPage(-1)"
    "navigation.RequestPage(4)"
    "FindPreferredOrFirstMatchingIndex(selectable.size(), 4"
    "FindPreferredOrFirstMatchingIndex(selectable.size(), -1"
    "FindPreferredOrFirstMatchingIndex(0, 0"
    "FindNextMatchingIndex(0, 0"
    "FindPreviousMatchingIndex(5, 0, never)")
  string(FIND "${runtime_laptop_imp_creation_test_contents}"
    "${required_laptop_imp_test_assertion}"
    required_laptop_imp_test_assertion_position)
  if(required_laptop_imp_test_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop IMP creation tests lost '${required_laptop_imp_test_assertion}'")
  endif()
endforeach()

foreach(required_laptop_communications_test_fragment IN ITEMS
    "laptop_communications_policy_tests.cpp"
    "laptop_communications_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_communications_test_fragment}"
    required_laptop_communications_test_position)
  if(required_laptop_communications_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop communications policy lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_communications_policy_tests"
  runtime_laptop_communications_ci_position)
if(runtime_laptop_communications_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop communications policy test target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_communications_policy_tests.cpp"
  runtime_laptop_communications_test_contents)
foreach(required_laptop_communications_assertion IN ITEMS
    "GameCampaign::Arulco"
    "GameCampaign::UnfinishedBusiness"
    "SuspiciousDeathFraud"
    "the initial Enrico congratulations mail remains Arulco-only"
    "impProfileResultsOffset() == 198"
    "IsValidLaptopIndex(1, 1)"
    "IsValidIntelMapRegion(-1)"
    "HasScrollableBobbyOrder(0)"
    "RemainingLaptopDays(7, 8)"
    "missing == records.end()"
    "ResolveLaptopRosterActor(false"
    "resolverCalls == 0")
  string(FIND "${runtime_laptop_communications_test_contents}"
    "${required_laptop_communications_assertion}"
    required_laptop_communications_assertion_position)
  if(required_laptop_communications_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop communications/safety tests lost '${required_laptop_communications_assertion}'")
  endif()
endforeach()

# File-viewer briefing catalogs/artwork and history quest text are runtime
# content choices. Both campaigns and their fallback assets must remain in one
# value-only policy, while the legacy pages only probe, load, and render the
# selected records.
file(READ "${SOURCE_ROOT}/Ja2/CampaignLaptopContentPolicy.h"
  runtime_laptop_campaign_content_policy_contents)
foreach(required_laptop_campaign_content_policy_fragment IN ITEMS
    "class CampaignLaptopContentPolicy"
    "briefingCatalog("
    "filesMapPath("
    "biographyPicturePath("
    "questTextRecord("
    "unfinishedBusinessBriefingAvailable"
    "unfinishedBusinessQuestTextAvailable"
    "completed ? 1 : 0")
  string(FIND "${runtime_laptop_campaign_content_policy_contents}"
    "${required_laptop_campaign_content_policy_fragment}"
    required_laptop_campaign_content_policy_position)
  if(required_laptop_campaign_content_policy_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop campaign content policy lost '${required_laptop_campaign_content_policy_fragment}'")
  endif()
endforeach()

foreach(laptop_campaign_content_caller_and_fragment IN ITEMS
    "Laptop/files.cpp|briefingCatalog("
    "Laptop/files.cpp|filesMapPath("
    "Laptop/files.cpp|biographyPicturePath("
    "Laptop/history.cpp|questTextRecord(")
  string(REPLACE "|" ";" laptop_campaign_content_caller_parts
    "${laptop_campaign_content_caller_and_fragment}")
  list(GET laptop_campaign_content_caller_parts 0
    laptop_campaign_content_caller)
  list(GET laptop_campaign_content_caller_parts 1
    laptop_campaign_content_fragment)
  file(READ "${SOURCE_ROOT}/${laptop_campaign_content_caller}"
    laptop_campaign_content_caller_contents)
  string(FIND "${laptop_campaign_content_caller_contents}"
    "${laptop_campaign_content_fragment}"
    laptop_campaign_content_caller_position)
  if(laptop_campaign_content_caller_position EQUAL -1)
    message(FATAL_ERROR
      "${laptop_campaign_content_caller} bypassed runtime Laptop content selection '${laptop_campaign_content_fragment}'")
  endif()
  string(REGEX MATCH
    "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
    compiled_laptop_campaign_content
    "${laptop_campaign_content_caller_contents}")
  if(compiled_laptop_campaign_content)
    message(FATAL_ERROR
      "${laptop_campaign_content_caller} regained compile-time campaign content selection")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/history.cpp"
  runtime_laptop_history_contents)
string(FIND "${runtime_laptop_history_contents}"
  "L\"ERROR!!!\tNO_PROFILE\"" runtime_laptop_history_no_profile_position)
if(runtime_laptop_history_no_profile_position EQUAL -1)
  message(FATAL_ERROR
    "Laptop history lost its unconditional missing-profile render fallback")
endif()
string(REGEX MATCH
  "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2BETAVERSION"
  compiled_laptop_history_beta_identity "${runtime_laptop_history_contents}")
if(compiled_laptop_history_beta_identity)
  message(FATAL_ERROR
    "Laptop history regained application-only beta rendering behavior")
endif()

# Finance and history share one dependency-free record-page model and exact-I/O
# owner. Both pages stage render/input resources, normalize persisted pages,
# reject malformed record tails, and keep legacy handles out of open-coded
# cleanup paths.
file(READ "${SOURCE_ROOT}/Laptop/LaptopRecordPageModel.h"
  runtime_laptop_record_page_model_contents)
foreach(required_laptop_record_page_model_fragment IN ITEMS
    "IsWellFormedFile"
    "IsAppendableFile"
    "NormalizeZeroBasedPage"
    "NormalizeOneBasedPage"
    "PageByteOffset"
    "RecordsOnPage"
    "BoundedIndex"
    "CanApplyBalanceChange"
    "SaturatingAdd"
    "SaturatingAddUnsigned"
    "SaturatingSubtract"
    "IsRecordOnDayOffset")
  string(FIND "${runtime_laptop_record_page_model_contents}"
    "${required_laptop_record_page_model_fragment}"
    required_laptop_record_page_model_position)
  if(required_laptop_record_page_model_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop record-page model lost '${required_laptop_record_page_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/LaptopRecordFile.h"
  runtime_laptop_record_file_contents)
foreach(required_laptop_record_file_fragment IN ITEMS
    "class ScopedLaptopFile"
    "ReadLaptopFileExact"
    "bytesRead == size"
    "WriteLaptopFileExact"
    "bytesWritten == size")
  string(FIND "${runtime_laptop_record_file_contents}"
    "${required_laptop_record_file_fragment}"
    required_laptop_record_file_position)
  if(required_laptop_record_file_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop record-file boundary lost '${required_laptop_record_file_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/finances.cpp"
  runtime_laptop_finance_contents)
foreach(laptop_record_page_source IN ITEMS
    runtime_laptop_finance_contents
    runtime_laptop_history_contents)
  string(FIND "${${laptop_record_page_source}}"
    "LaptopPageResourceOwner" laptop_record_page_owner_position)
  if(laptop_record_page_owner_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop ledger page lost transactional resource ownership")
  endif()
  string(REGEX MATCH
    "DeleteVideoObjectFromIndex[ \t\r\n]*\\(|RemoveButton[ \t\r\n]*\\(|UnloadButtonImage[ \t\r\n]*\\("
    raw_laptop_record_resource_lifecycle "${${laptop_record_page_source}}")
  if(raw_laptop_record_resource_lifecycle)
    message(FATAL_ERROR
      "Laptop ledger page restored open-coded resource teardown '${raw_laptop_record_resource_lifecycle}'")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])File(Read|Write|Close)[ \t\r\n]*\\("
    raw_laptop_record_file_io "${${laptop_record_page_source}}")
  if(raw_laptop_record_file_io)
    message(FATAL_ERROR
      "Laptop ledger page bypassed exact/scoped file ownership '${raw_laptop_record_file_io}'")
  endif()
  string(REGEX MATCH
    "mprintf[ \t\r\n]*\\([^,\r\n]+,[^,\r\n]+,[ \t\r\n]*(pFinance|pHistory|sString|tmp\\.data)"
    unsafe_laptop_record_page_format "${${laptop_record_page_source}}")
  if(unsafe_laptop_record_page_format)
    message(FATAL_ERROR
      "Laptop ledger page restored a variable render format '${unsafe_laptop_record_page_format}'")
  endif()
endforeach()

foreach(required_laptop_finance_safety_fragment IN ITEMS
    "gFinancePageResources"
    "ReadFinanceRecordExact"
    "PersistFinanceTransaction"
    "IsAppendableFile"
    "CanApplyBalanceChange"
    "NormalizeZeroBasedPage"
    "gFinanceRecordPageCount"
    "IsRecordOnDayOffset"
    "BoundedIndex"
    "FormatFinanceMagnitude"
    "L\"%s\", pFinanceSummary"
    "SaturatingSubtract")
  string(FIND "${runtime_laptop_finance_contents}"
    "${required_laptop_finance_safety_fragment}"
    required_laptop_finance_safety_position)
  if(required_laptop_finance_safety_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop finance safety lost '${required_laptop_finance_safety_fragment}'")
  endif()
endforeach()

foreach(required_laptop_history_safety_fragment IN ITEMS
    "gHistoryPageResources"
    "guiHistoryTitle"
    "ReadHistoryRecordExact"
    "WriteHistoryRecordExact"
    "NormalizeOneBasedPage"
    "gHistoryRecordPageCount"
    "BoundedIndex"
    "TextKey::HistoryTitle"
    "sgp_swprintf(pString, 512, L\"%s\", sString)"
    "AppendHistoryToEndOfFile(const HistoryUnit&")
  string(FIND "${runtime_laptop_history_contents}"
    "${required_laptop_history_safety_fragment}"
    required_laptop_history_safety_position)
  if(required_laptop_history_safety_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop history safety lost '${required_laptop_history_safety_fragment}'")
  endif()
endforeach()

foreach(retired_laptop_record_page_fragment IN ITEMS
    "IncrementCurrentPageFinancialDisplay"
    "IncrementCurrentPageHistoryDisplay"
    "WriteOutHistoryRecords"
    "ReadInLastElementOfHistoryListAndReturnIdNumber"
    "GetNumberOfHistoryPages")
  string(FIND "${runtime_laptop_finance_contents}"
    "${retired_laptop_record_page_fragment}"
    retired_laptop_finance_record_page_position)
  string(FIND "${runtime_laptop_history_contents}"
    "${retired_laptop_record_page_fragment}"
    retired_laptop_history_record_page_position)
  if(NOT retired_laptop_finance_record_page_position EQUAL -1 OR
      NOT retired_laptop_history_record_page_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop ledgers restored retired page/file path '${retired_laptop_record_page_fragment}'")
  endif()
endforeach()

foreach(required_laptop_record_page_test_build_fragment IN ITEMS
    "laptop_record_page_model_tests.cpp"
    "laptop_record_page_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_record_page_test_build_fragment}"
    required_laptop_record_page_test_build_position)
  if(required_laptop_record_page_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop record-page model lost its focused test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_record_page_model_tests"
  runtime_laptop_record_page_ci_position)
if(runtime_laptop_record_page_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop record-page model target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_record_page_model_tests.cpp"
  runtime_laptop_record_page_test_contents)
foreach(required_laptop_record_page_test_fragment IN ITEMS
    "Record layouts reject short, partial, and zero-width files"
    "Fresh ledgers accept their first header and record append"
    "Saved ledger pages normalize empty, zero, and stale values"
    "Ledger offsets include headers and reject overflowing pages"
    "Record-driven indices reject exact-end and oversized values"
    "Finance balance publication rejects signed overflow"
    "Ledger summaries saturate instead of overflowing signed totals"
    "Ledger day buckets handle legacy adjustment, early days, and overflow")
  string(FIND "${runtime_laptop_record_page_test_contents}"
    "${required_laptop_record_page_test_fragment}"
    required_laptop_record_page_test_position)
  if(required_laptop_record_page_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop record-page tests lost '${required_laptop_record_page_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  runtime_laptop_finance_headless_test_contents)
string(FIND "${runtime_laptop_finance_headless_test_contents}"
  "new-game finance ledger persists starting cash before later transactions"
  runtime_laptop_finance_new_game_test_position)
if(runtime_laptop_finance_new_game_test_position EQUAL -1)
  message(FATAL_ERROR
    "Laptop finance lost its FileMan/VFS new-game persistence regression test")
endif()

# Email owns one mutable inbox/page graph plus several independently replaced
# UI overlays. Keep its bounds and text rules dependency-free, its resource
# acquisition transactional, and its save publication staged.
file(READ "${SOURCE_ROOT}/Laptop/LaptopEmailListModel.h"
  runtime_laptop_email_list_model_contents)
foreach(required_laptop_email_list_model_fragment IN ITEMS
    "NormalizeInboxPage"
    "IsIndexInRange"
    "CanStoreUnsigned16"
    "CanAppendMessage"
    "IsMoreRecent"
    "WildfireSubjectLine"
    "NormalizeBodyPage"
    "HasNextBodyPage"
    "CopyText"
    "AppendText"
    "BoundedLength"
    "IsSerializedSubjectSizeValid")
  string(FIND "${runtime_laptop_email_list_model_contents}"
    "${required_laptop_email_list_model_fragment}"
    required_laptop_email_list_model_position)
  if(required_laptop_email_list_model_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop email-list model lost '${required_laptop_email_list_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/email.cpp" runtime_laptop_email_contents)
foreach(required_laptop_email_safety_fragment IN ITEMS
    "gEmailPageResources"
    "gEmailMessageResources"
    "gEmailNewMailResources"
    "gEmailDeleteResources"
    "BuildEmailPages"
    "CommitEmailPages"
    "ReplaceEmailListFromSavedGame"
    "NormalizeInboxPage"
    "WildfireSubjectLine"
    "NormalizeBodyPage"
    "CopyEmailText"
    "gEmailRecordBuildFailed"
    "IsMoreRecent"
    "WriteLaptopFileExact"
    "ReadLaptopFileExact"
    "pA->iFirstData"
    "pA->iCurrentShipmentDestinationID"
	"An indivisible record taller than a page"
    "pEmail->EmailType = gEmailT[uiNumOfEmails].EmailType")
  string(FIND "${runtime_laptop_email_contents}"
    "${required_laptop_email_safety_fragment}"
    required_laptop_email_safety_position)
  if(required_laptop_email_safety_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop email safety lost '${required_laptop_email_safety_fragment}'")
  endif()
endforeach()

string(REGEX MATCH
  "AddVideoObject[ \t\r\n]*\\(|DeleteVideoObjectFromIndex[ \t\r\n]*\\(|LoadButtonImage[ \t\r\n]*\\(|UnloadButtonImage[ \t\r\n]*\\(|RemoveButton[ \t\r\n]*\\(|MSYS_(Add|Remove)Region[ \t\r\n]*\\("
  raw_laptop_email_resource_lifecycle "${runtime_laptop_email_contents}")
if(raw_laptop_email_resource_lifecycle)
  message(FATAL_ERROR
    "Laptop email restored open-coded resource lifecycle '${raw_laptop_email_resource_lifecycle}'")
endif()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])File(Read|Write)[ \t\r\n]*\\("
  raw_laptop_email_file_io "${runtime_laptop_email_contents}")
if(raw_laptop_email_file_io)
  message(FATAL_ERROR
    "Laptop email bypassed exact file I/O '${raw_laptop_email_file_io}'")
endif()
string(REGEX MATCH
  "mprintf[ \t\r\n]*\\([^,\r\n]+,[^,\r\n]+,[ \t\r\n]*[A-KM-Za-km-z_][A-Za-z0-9_]*(\\[[^]]*\\])?[ \t\r\n]*\\)"
  unsafe_laptop_email_render_format "${runtime_laptop_email_contents}")
if(unsafe_laptop_email_render_format)
  message(FATAL_ERROR
    "Laptop email restored a variable render format '${unsafe_laptop_email_render_format}'")
endif()
string(REGEX MATCH "(^|[^A-Za-z0-9_])(wcs(cpy|ncpy|cat|ncat)|swprintf)[ \t\r\n]*\\("
  unsafe_laptop_email_text_copy "${runtime_laptop_email_contents}")
if(unsafe_laptop_email_text_copy)
  message(FATAL_ERROR
    "Laptop email restored an unbounded text operation '${unsafe_laptop_email_text_copy}'")
endif()

foreach(retired_laptop_email_fragment IN ITEMS
    "AddEmailPage"
    "RemoveEmailPage"
    "AddMessageToPages"
    "ReplaceBiffNameWithProperMercName"
    "CreateDestroyNextPreviousRegions"
    "DestroyMailScreenButtons"
    "DisplayEmailHeaders"
    "fJustStartedEmail"
    "gfPageButtonsWereCreated"
    "static BOOLEAN fOldNewMailFlag")
  string(FIND "${runtime_laptop_email_contents}"
    "${retired_laptop_email_fragment}" retired_laptop_email_position)
  if(NOT retired_laptop_email_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop email restored retired path '${retired_laptop_email_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  runtime_save_load_game_contents)
foreach(required_laptop_email_save_fragment IN ITEMS
    "LoadedEmailListDeleter"
    "IsSerializedSubjectSizeValid"
    "ReadLaptopFileExact"
    "WriteLaptopFileExact"
    "ReplaceEmailListFromSavedGame(loadedEmails.get())")
  string(FIND "${runtime_save_load_game_contents}"
    "${required_laptop_email_save_fragment}"
    required_laptop_email_save_position)
  if(required_laptop_email_save_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop email save boundary lost '${required_laptop_email_save_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/WordWrap.cpp"
  runtime_word_wrap_contents)
string(FIND "${runtime_word_wrap_contents}"
  "if (CurrentRecord == firstRecordOnPage)"
  email_oversized_record_progress_position)
if(email_oversized_record_progress_position EQUAL -1)
  message(FATAL_ERROR
    "Email pagination lost oversized-record progress protection")
endif()

foreach(required_laptop_email_test_build_fragment IN ITEMS
    "laptop_email_list_model_tests.cpp"
    "laptop_email_list_model")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_email_test_build_fragment}"
    required_laptop_email_test_build_position)
  if(required_laptop_email_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop email-list model lost its focused test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_email_list_model_tests" runtime_laptop_email_ci_position)
if(runtime_laptop_email_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop email-list model target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_email_list_model_tests.cpp"
  runtime_laptop_email_test_contents)
foreach(required_laptop_email_test_fragment IN ITEMS
    "Inbox page counts handle empty and exact-page message sets"
    "Saved inbox pages normalize empty and stale values"
    "Profile and message indices reject negative and exact-end values"
    "Persisted email offsets reject narrowing and negative values"
    "Message ID generation rejects capacity and signed overflow"
    "Most-recent unread selection uses date then message ID"
    "Wildfire subjects reject underflow and preserve paired indices"
    "Message body pages reserve a bounded sentinel entry"
    "Fixed email text copies terminate exact, long, and null inputs"
    "Subject bounds reject unterminated and malformed serialized text")
  string(FIND "${runtime_laptop_email_test_contents}"
    "${required_laptop_email_test_fragment}"
    required_laptop_email_test_position)
  if(required_laptop_email_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop email-list tests lost '${required_laptop_email_test_fragment}'")
  endif()
endforeach()

foreach(required_laptop_campaign_content_test_fragment IN ITEMS
    "laptop_campaign_content_policy_tests.cpp"
    "laptop_campaign_content_policy")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_campaign_content_test_fragment}"
    required_laptop_campaign_content_test_position)
  if(required_laptop_campaign_content_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop campaign content policy lost its headless test target")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "laptop_campaign_content_policy_tests"
  runtime_laptop_campaign_content_ci_position)
if(runtime_laptop_campaign_content_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the Laptop campaign content policy test target")
endif()

file(READ "${SOURCE_ROOT}/tests/laptop_campaign_content_policy_tests.cpp"
  runtime_laptop_campaign_content_test_contents)
foreach(required_laptop_campaign_content_assertion IN ITEMS
    "briefingCatalog(false)"
    "BINARYDATA\\\\RIS.edt"
    "filesMapPath(false)"
    "biographyPicturePath(4) == nullptr"
    "questTextRecord(7, true, false)"
    "ubQuestEndFallback.recordIndex == 15")
  string(FIND "${runtime_laptop_campaign_content_test_contents}"
    "${required_laptop_campaign_content_assertion}"
    required_laptop_campaign_content_assertion_position)
  if(required_laptop_campaign_content_assertion_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop campaign content tests lost '${required_laptop_campaign_content_assertion}'")
  endif()
endforeach()

# The scheduled Laptop walkthrough is closed by one shared dependency-free UI
# boundary, transactional ownership in the residual shell/page/widget cluster,
# per-page ownership for every IMP screen, and a real compile check for the
# otherwise dormant Encyclopedia implementation.
file(READ "${SOURCE_ROOT}/Laptop/LaptopUiStateModel.h"
  runtime_laptop_ui_state_model_contents)
foreach(required_laptop_ui_state_model_fragment IN ITEMS
    "IsValidIndex"
    "NormalizeIndex"
    "AdjacentIndex"
    "PageCount"
    "NormalizePageStart"
    "NormalizeWindowStart"
    "CopyText"
    "AppendText"
    "IsExactTransfer"
    "NormalizeSentinelList"
    "AppendUniqueSentinel"
    "RemoveSentinelValue"
    "ResourceTransactionState")
  string(FIND "${runtime_laptop_ui_state_model_contents}"
    "${required_laptop_ui_state_model_fragment}"
    required_laptop_ui_state_model_position)
  if(required_laptop_ui_state_model_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop UI state boundary lost '${required_laptop_ui_state_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/ImpPageResourceOwner.h"
  runtime_laptop_imp_page_owner_contents)
file(READ "${SOURCE_ROOT}/Laptop/ImpPageResourceState.h"
  runtime_laptop_imp_page_state_contents)
file(READ "${SOURCE_ROOT}/sgp/ButtonResourceHandle.h"
  runtime_laptop_button_resource_handle_contents)
foreach(required_laptop_imp_failure_fragment IN ITEMS
    "FailImpPageResources"
    "UseLoadedImpPageButtonImage"
    "UseLoadedButtonImageOwned"
    "#define UseLoadedButtonImage UseLoadedImpPageButtonImage"
    "SetImpPageButtonClicked"
    "ImpPageResourcesReady")
  string(FIND
    "${runtime_laptop_imp_page_owner_contents}${runtime_laptop_imp_page_state_contents}${runtime_laptop_button_resource_handle_contents}"
    "${required_laptop_imp_failure_fragment}"
    required_laptop_imp_failure_position)
  if(required_laptop_imp_failure_position EQUAL -1)
    message(FATAL_ERROR
      "IMP failure-atomic resource ownership lost '${required_laptop_imp_failure_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/CharProfile.cpp"
  runtime_laptop_imp_dispatch_contents)
foreach(required_laptop_imp_dispatch_fragment IN ITEMS
    "if (!ImpPageResourcesReady()) return"
    "#include \"ImpPageResourceState.h\"")
  string(FIND "${runtime_laptop_imp_dispatch_contents}"
    "${required_laptop_imp_dispatch_fragment}"
    required_laptop_imp_dispatch_position)
  if(required_laptop_imp_dispatch_position EQUAL -1)
    message(FATAL_ERROR
      "IMP dispatcher lost failed-page guard '${required_laptop_imp_dispatch_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Encrypted File.cpp"
  runtime_encrypted_text_reader_contents)
foreach(required_encrypted_text_reader_fragment IN ITEMS
    "uiBytesRead != uiByteCount"
    "pDestString[0] = L'\\0'"
    "pDestString[uiSeekAmount / 2 - 1] = L'\\0'"
    "const UINT32 recordCount"
    "uiSeekFrom = Random(recordCount)"
    "if (!pDestString) return")
  string(FIND "${runtime_encrypted_text_reader_contents}"
    "${required_encrypted_text_reader_fragment}"
    required_encrypted_text_reader_position)
  if(required_encrypted_text_reader_position EQUAL -1)
    message(FATAL_ERROR
      "Encrypted text reader lost '${required_encrypted_text_reader_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/IMP Text System.cpp"
  runtime_laptop_imp_text_contents)
foreach(required_laptop_imp_text_fragment IN ITEMS
    "CHAR16 sString[ 1024 ] = {}"
    "const BOOLEAN loaded = LoadEncryptedDataFromFile"
    "if (loaded)")
  string(FIND "${runtime_laptop_imp_text_contents}"
    "${required_laptop_imp_text_fragment}"
    required_laptop_imp_text_position)
  if(required_laptop_imp_text_position EQUAL -1)
    message(FATAL_ERROR
      "IMP text failure handling lost '${required_laptop_imp_text_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/sgp/Button System.cpp"
  runtime_laptop_button_system_contents)
foreach(required_laptop_button_guard_fragment IN ITEMS
    "LoadedImg < 0 || LoadedImg >= MAX_BUTTON_PICS"
    "GUI_BUTTON *b = GetButtonPtr(iBtnId)"
    "GUI_BUTTON *b = GetButtonPtr(iButtonID)")
  string(FIND "${runtime_laptop_button_system_contents}"
    "${required_laptop_button_guard_fragment}"
    required_laptop_button_guard_position)
  if(required_laptop_button_guard_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop button compatibility boundary lost '${required_laptop_button_guard_fragment}'")
  endif()
endforeach()

set(runtime_laptop_closure_owned_sources
  "Laptop/BaseTable.cpp"
  "Laptop/DropDown.cpp"
  "Laptop/DynamicDialogueWidget.cpp"
  "Laptop/BriefingRoom.cpp"
  "Laptop/BriefingRoomM.cpp"
  "Laptop/BriefingRoom_Data.cpp"
  "Laptop/FacilityProduction.cpp"
  "Laptop/Intelmarket.cpp"
  "Laptop/MilitiaWebsite.cpp"
  "Laptop/PMC.cpp"
  "Laptop/WHO.cpp"
  "Laptop/files.cpp"
  "Laptop/laptop.cpp"
  "Laptop/merccompare.cpp"
  "Laptop/Encyclopedia_new.cpp"
  "Laptop/Encyclopedia_Data_new.cpp")
foreach(runtime_laptop_closure_owned_source IN LISTS
    runtime_laptop_closure_owned_sources)
  file(READ "${SOURCE_ROOT}/${runtime_laptop_closure_owned_source}"
    runtime_laptop_closure_owned_contents)
  string(FIND "${runtime_laptop_closure_owned_contents}"
    "LaptopPageResourceOwner" runtime_laptop_closure_owner_position)
  if(runtime_laptop_closure_owner_position EQUAL -1)
    message(FATAL_ERROR
      "${runtime_laptop_closure_owned_source} lost transactional Laptop ownership")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(AddVideoObject|DeleteVideoObjectFromIndex|AddVideoSurface|DeleteVideoSurfaceFromIndex|LoadButtonImage|UnloadButtonImage|RemoveButton)[ \t\r\n]*\\("
    raw_laptop_closure_resource_lifecycle
    "${runtime_laptop_closure_owned_contents}")
  if(raw_laptop_closure_resource_lifecycle)
    message(FATAL_ERROR
      "${runtime_laptop_closure_owned_source} restored raw resource lifecycle '${raw_laptop_closure_resource_lifecycle}'")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(wcs(cpy|ncpy|cat|ncat)|swprintf|sprintf|str(cpy|ncpy|cat|ncat))[ \t\r\n]*\\("
    unsafe_laptop_closure_text_operation
    "${runtime_laptop_closure_owned_contents}")
  if(unsafe_laptop_closure_text_operation)
    message(FATAL_ERROR
      "${runtime_laptop_closure_owned_source} restored unbounded text '${unsafe_laptop_closure_text_operation}'")
  endif()
endforeach()

set(runtime_laptop_imp_page_sources
  "Laptop/IMP AboutUs.cpp"
  "Laptop/IMP Attribute Entrance.cpp"
  "Laptop/IMP Attribute Finish.cpp"
  "Laptop/IMP Attribute Selection.cpp"
  "Laptop/IMP Background.cpp"
  "Laptop/IMP Begin Screen.cpp"
  "Laptop/IMP Character Trait.cpp"
  "Laptop/IMP Character and Disability Entrance.cpp"
  "Laptop/IMP Color Choosing.cpp"
  "Laptop/IMP Confirm.cpp"
  "Laptop/IMP Disability Trait.cpp"
  "Laptop/IMP Finish.cpp"
  "Laptop/IMP Gear Entrance.cpp"
  "Laptop/IMP Gear.cpp"
  "Laptop/IMP HomePage.cpp"
  "Laptop/IMP MainPage.cpp"
  "Laptop/IMP Minor Trait.cpp"
  "Laptop/IMP Personality Entrance.cpp"
  "Laptop/IMP Personality Finish.cpp"
  "Laptop/IMP Personality Quiz.cpp"
  "Laptop/IMP Portraits.cpp"
  "Laptop/IMP Prejudice.cpp"
  "Laptop/IMP Skill Trait.cpp"
  "Laptop/IMP Voices.cpp")
foreach(runtime_laptop_imp_page_source IN LISTS runtime_laptop_imp_page_sources)
  file(READ "${SOURCE_ROOT}/${runtime_laptop_imp_page_source}"
    runtime_laptop_imp_page_contents)
  foreach(required_laptop_imp_page_fragment IN ITEMS
      "ImpPageResourceOwner.h" "BeginImpPageResources")
    string(FIND "${runtime_laptop_imp_page_contents}"
      "${required_laptop_imp_page_fragment}"
      required_laptop_imp_page_position)
    if(required_laptop_imp_page_position EQUAL -1)
      message(FATAL_ERROR
        "${runtime_laptop_imp_page_source} bypassed per-page IMP ownership '${required_laptop_imp_page_fragment}'")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/IMPVideoObjects.cpp"
  runtime_laptop_imp_video_object_contents)
foreach(required_laptop_imp_video_object_fragment IN ITEMS
    "static LaptopPageResourceOwner gImpGraphicResources"
    "gImpGraphicResources.addVideoObject"
    "gImpGraphicResources.clear")
  string(FIND "${runtime_laptop_imp_video_object_contents}"
    "${required_laptop_imp_video_object_fragment}"
    required_laptop_imp_video_object_position)
  if(required_laptop_imp_video_object_position EQUAL -1)
    message(FATAL_ERROR
      "IMP shared graphics lost '${required_laptop_imp_video_object_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/files.cpp" runtime_laptop_closure_files_contents)
foreach(required_laptop_files_closure_fragment IN ITEMS
    "MemAlloc((length + 1) * sizeof(CHAR16))"
    "recordCount >= MAX_FILES_LIST_LENGTH"
    "FileStringPtr next = pFileString->Next"
    "if (!LoadEncryptedDataFromFile"
    "LaptopUiStateModel::IsValidIndex(NUM_PROFILES, profileId)"
    "ReadLaptopFileExact"
    "WriteLaptopFileExact"
    "LaptopPageResourceOwner stagedResources")
  string(FIND "${runtime_laptop_closure_files_contents}"
    "${required_laptop_files_closure_fragment}"
    required_laptop_files_closure_position)
  if(required_laptop_files_closure_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Files closure lost '${required_laptop_files_closure_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/laptop.cpp"
  runtime_laptop_closure_shell_contents)
foreach(required_laptop_shell_closure_fragment IN ITEMS
    "CreateMouseRegionsForLaptopNotifications"
    "DestroyMouseRegionsForLaptopNotifications"
    "NormalizeBookmarkList"
    "LaptopSaveInfoStruct pendingInfo{}"
    "gLaptopCoreResources"
    "gLaptopNotificationRegionResources")
  string(FIND "${runtime_laptop_closure_shell_contents}"
    "${required_laptop_shell_closure_fragment}"
    required_laptop_shell_closure_position)
  if(required_laptop_shell_closure_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop shell closure lost '${required_laptop_shell_closure_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/BriefingRoom_Data.cpp"
  runtime_laptop_closure_briefing_contents)
file(READ "${SOURCE_ROOT}/Laptop/merccompare.cpp"
  runtime_laptop_closure_merccompare_contents)
file(READ "${SOURCE_ROOT}/Laptop/Encyclopedia_Data_new.cpp"
  runtime_laptop_closure_encyclopedia_contents)
foreach(required_laptop_transient_owner_fragment IN ITEMS
    "LaptopUiStateModel::AdjacentIndex"
    "UniqueVideoObjectHandle")
  string(FIND
    "${runtime_laptop_closure_briefing_contents}${runtime_laptop_closure_encyclopedia_contents}"
    "${required_laptop_transient_owner_fragment}"
    required_laptop_transient_owner_position)
  if(required_laptop_transient_owner_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop briefing/Encyclopedia closure lost '${required_laptop_transient_owner_fragment}'")
  endif()
endforeach()
foreach(required_laptop_encyclopedia_safety_fragment IN ITEMS
    "CHAR16 questStartedString[160] = {}"
    "GUI_BUTTON* otherButton ="
    "GUI_BUTTON* createdButton = GetButtonPtr")
  string(FIND
    "${runtime_laptop_closure_encyclopedia_contents}${runtime_laptop_closure_briefing_contents}"
    "${required_laptop_encyclopedia_safety_fragment}"
    required_laptop_encyclopedia_safety_position)
  if(required_laptop_encyclopedia_safety_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop dormant-path safety lost '${required_laptop_encyclopedia_safety_fragment}'")
  endif()
endforeach()
foreach(required_laptop_merccompare_closure_fragment IN ITEMS
    "DisplayMercFace"
    "UniqueVideoObjectHandle"
    "gMercCompareMatrixResources"
    "NUMBER_OF_SQUADS")
  string(FIND "${runtime_laptop_closure_merccompare_contents}"
    "${required_laptop_merccompare_closure_fragment}"
    required_laptop_merccompare_closure_position)
  if(required_laptop_merccompare_closure_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop Merc Compare closure lost '${required_laptop_merccompare_closure_fragment}'")
  endif()
endforeach()

foreach(required_laptop_closure_test_build_fragment IN ITEMS
    "laptop_ui_state_model_tests.cpp"
    "laptop_ui_state_model"
    "laptop_encyclopedia_compile_check"
    "ENCYCLOPEDIA_WORKS")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_laptop_closure_test_build_fragment}"
    required_laptop_closure_test_build_position)
  if(required_laptop_closure_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop closure lost test/compile target '${required_laptop_closure_test_build_fragment}'")
  endif()
endforeach()
foreach(required_laptop_closure_ci_fragment IN ITEMS
    "laptop_ui_state_model_tests"
    "laptop_encyclopedia_compile_check")
  string(FIND "${runtime_campaign_policy_ci_contents}"
    "${required_laptop_closure_ci_fragment}"
    required_laptop_closure_ci_position)
  if(required_laptop_closure_ci_position EQUAL -1)
    message(FATAL_ERROR
      "AddressSanitizer CI lost Laptop closure target '${required_laptop_closure_ci_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/laptop_ui_state_model_tests.cpp"
  runtime_laptop_ui_state_test_contents)
foreach(required_laptop_ui_state_test_fragment IN ITEMS
    "signed negative indices are rejected"
    "adjacent selection rejects both ends and stale indices"
    "zero-sized pages cannot divide by zero"
    "hidden and exact-end page slots are rejected"
    "scrolling windows clamp both stale and undersized end positions"
    "bounded copies truncate and terminate"
    "binary transfers must be exact"
    "full sentinel lists reject insertion without an exact-end write"
    "resource acquisition failure stays latched for the page"
    "a new page resets the resource failure latch")
  string(FIND "${runtime_laptop_ui_state_test_contents}"
    "${required_laptop_ui_state_test_fragment}"
    required_laptop_ui_state_test_position)
  if(required_laptop_ui_state_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop UI state tests lost '${required_laptop_ui_state_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  runtime_laptop_failure_headless_test_contents)
foreach(required_laptop_failure_headless_test_fragment IN ITEMS
    "encrypted text reads decode exact records and force an in-bounds terminator"
    "short encrypted text records fail without publishing partial or stale text"
    "missing encrypted text records clear the caller destination"
    "legacy button operations reject invalid compatibility handles"
    "legacy button configuration ignores invalid compatibility handles")
  string(FIND "${runtime_laptop_failure_headless_test_contents}"
    "${required_laptop_failure_headless_test_fragment}"
    required_laptop_failure_headless_test_position)
  if(required_laptop_failure_headless_test_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop failure-path headless tests lost '${required_laptop_failure_headless_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/docs/LAPTOP_CODE_WALKTHROUGH.md"
  runtime_laptop_walkthrough_contents)
foreach(required_laptop_closure_documentation_fragment IN ITEMS
    "Shared shell, widgets, residual pages, and dormant-path closure"
    "All 98 Laptop translation units"
    "no remaining Laptop audit batch"
    "Post-closure IMP and encrypted-text hardening")
  string(FIND "${runtime_laptop_walkthrough_contents}"
    "${required_laptop_closure_documentation_fragment}"
    required_laptop_closure_documentation_position)
  if(required_laptop_closure_documentation_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop closure documentation lost '${required_laptop_closure_documentation_fragment}'")
  endif()
endforeach()

# Laptop visual-layout models share dependency-free points, rectangles, text
# areas, and containment helpers instead of redefining geometry primitives.
file(READ "${SOURCE_ROOT}/Laptop/LaptopLayout.h"
  runtime_laptop_layout_contents)
foreach(required_laptop_layout_fragment IN ITEMS
    "namespace LaptopLayoutModel"
    "struct Point"
    "struct Rect"
    "struct TextArea"
    "constexpr bool Contains"
    "constexpr bool Overlaps")
  string(FIND "${runtime_laptop_layout_contents}"
    "${required_laptop_layout_fragment}" required_laptop_layout_position)
  if(required_laptop_layout_position EQUAL -1)
    message(FATAL_ERROR
      "Shared Laptop layout primitives lost '${required_laptop_layout_fragment}'")
  endif()
endforeach()

# A.I.M.'s static member-profile composition has one dependency-free geometry
# model. Drawing, hitboxes, invalidation, and both equipment variants must keep
# selecting the same layout instead of restoring parallel coordinate macros.
file(READ "${SOURCE_ROOT}/Laptop/AimMemberProfileLayout.h"
  runtime_aim_member_profile_layout_contents)
foreach(required_aim_member_profile_layout_fragment IN ITEMS
    "#include \"LaptopLayout.h\""
    "namespace AimMemberProfileLayoutModel"
    "using LaptopLayoutModel::Rect"
    "struct StatsLayout"
    "struct InventoryLayout"
    "struct NavigationLayout"
    "struct Layout"
    "kExpandedInventoryCapacity = 21"
    "constexpr Layout MakeLayout"
    "constexpr Rect cell")
  string(FIND "${runtime_aim_member_profile_layout_contents}"
    "${required_aim_member_profile_layout_fragment}"
    required_aim_member_profile_layout_position)
  if(required_aim_member_profile_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. member-profile layout lost '${required_aim_member_profile_layout_fragment}'")
  endif()
endforeach()

# A.I.M.'s remaining site-wide geometry is organized independently from the
# member profile but uses the same primitives. Default artwork, video-call
# state, Facial Index pagination/grid geometry, and Policies controls must keep
# drawing and input on the same selected layouts.
file(READ "${SOURCE_ROOT}/Laptop/AimWebsiteLayout.h"
  runtime_aim_website_layout_contents)
foreach(required_aim_website_layout_fragment IN ITEMS
    "namespace AimWebsiteLayoutModel"
    "struct AimDefaultsLayout"
    "MakeAimDefaultsLayout"
    "struct VideoConferenceLayout"
    "MakeVideoConferenceLayout"
    "titleFrame(std::size_t frame)"
    "struct FacialIndexGrid"
    "MakeFacialIndexLayout"
    "FacialIndexPageCount"
    "NormalizeFacialIndexPageStart"
    "NextFacialIndexPageStart"
    "PreviousFacialIndexPageStart"
    "FacialIndexVisibleSlotCount"
    "FacialIndexPageTextIndex"
    "struct PolicyLayout"
    "MakePolicyLayout"
    "struct SortLayout"
    "MakeSortLayout"
    "criterionHitbox"
    "orderHitbox"
    "hasControl"
    "struct ArchiveGrid"
    "struct ArchivePopupLayout"
    "struct ArchiveLayout"
    "MakeArchiveLayout"
    "ArchiveProfileIndex"
    "ArchivePageHasVisible"
    "NextArchivePage")
  string(FIND "${runtime_aim_website_layout_contents}"
    "${required_aim_website_layout_fragment}"
    required_aim_website_layout_position)
  if(required_aim_website_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. website layout lost '${required_aim_website_layout_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/AimMembers.cpp"
  runtime_aim_member_profile_contents)
file(READ "${SOURCE_ROOT}/Laptop/AimFacialIndex.cpp"
  runtime_aim_facial_index_contents)
file(READ "${SOURCE_ROOT}/Laptop/AimPolicies.cpp"
  runtime_aim_policies_contents)
file(READ "${SOURCE_ROOT}/Laptop/aim.cpp"
  runtime_aim_defaults_contents)
file(READ "${SOURCE_ROOT}/Laptop/AimSort.cpp"
  runtime_aim_sort_contents)
file(READ "${SOURCE_ROOT}/Laptop/AimArchives.cpp"
  runtime_aim_archives_contents)
foreach(required_aim_members_video_layout_fragment IN ITEMS
    "CurrentAimVideoConferenceLayout()"
    "layout.titleFrame"
    "layout.contractButtons.at")
  string(FIND "${runtime_aim_member_profile_contents}"
    "${required_aim_members_video_layout_fragment}"
    required_aim_members_video_layout_position)
  if(required_aim_members_video_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Members bypassed video geometry '${required_aim_members_video_layout_fragment}'")
  endif()
endforeach()
foreach(required_aim_facial_index_layout_fragment IN ITEMS
    "CurrentAimFacialIndexLayout()"
    "layout.grid.cell"
    "ChangeAimFacialIndexPage"
    "FacialIndexVisibleSlotCount"
    "for (std::size_t i = 0; i < MAX_NUMBER_MERCS; ++i)")
  string(FIND "${runtime_aim_facial_index_contents}"
    "${required_aim_facial_index_layout_fragment}"
    required_aim_facial_index_layout_position)
  if(required_aim_facial_index_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Facial Index bypassed shared geometry '${required_aim_facial_index_layout_fragment}'")
  endif()
endforeach()
string(REPLACE "\r" "" runtime_aim_facial_index_normalized_contents
  "${runtime_aim_facial_index_contents}")
string(FIND "${runtime_aim_facial_index_normalized_contents}"
  "ChangeAimFacialIndexPage(true);\n\t\t\treturn;"
  aim_facial_index_callback_return_position)
if(aim_facial_index_callback_return_position EQUAL -1)
  message(FATAL_ERROR
    "A.I.M. Facial Index page callback can reuse its destroyed GUI button")
endif()
foreach(required_aim_policy_layout_fragment IN ITEMS
    "CurrentAimPolicyLayout()"
    "layout.tocButtons.at")
  string(FIND "${runtime_aim_policies_contents}"
    "${required_aim_policy_layout_fragment}"
    required_aim_policy_layout_position)
  if(required_aim_policy_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Policies bypassed shared geometry '${required_aim_policy_layout_fragment}'")
  endif()
endforeach()
foreach(required_aim_defaults_layout_fragment IN ITEMS
    "CurrentAimDefaultsLayout()"
    "layout.logo"
    "layout.backgroundTile")
  string(FIND "${runtime_aim_defaults_contents}"
    "${required_aim_defaults_layout_fragment}"
    required_aim_defaults_layout_position)
  if(required_aim_defaults_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. defaults bypassed shared geometry '${required_aim_defaults_layout_fragment}'")
  endif()
endforeach()
foreach(required_aim_sort_layout_fragment IN ITEMS
    "CurrentAimSortLayout()"
    "layout.navigationArtwork.at"
    "layout.criterionHitbox"
    "layout.orderHitbox"
    "layout.criterionText"
    "layout.orderText"
    "layout.control"
    "layout.hasControl"
    "SelectAimSortCriterionRegionCallback"
    "SelectAimSortOrderRegionCallback")
  string(FIND "${runtime_aim_sort_contents}"
    "${required_aim_sort_layout_fragment}"
    required_aim_sort_layout_position)
  if(required_aim_sort_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Sort bypassed shared geometry '${required_aim_sort_layout_fragment}'")
  endif()
endforeach()
foreach(required_aim_archive_layout_fragment IN ITEMS
    "CurrentAimArchiveLayout()"
    "layout.grid.frame"
    "layout.grid.hitbox"
    "const auto& popup = layout.popup"
    "popup.doneButton"
    "popup.doneHitbox"
    "ArchiveProfileIndex"
    "ArchivePageHasVisible"
    "NextArchivePage")
  string(FIND "${runtime_aim_archives_contents}"
    "${required_aim_archive_layout_fragment}"
    required_aim_archive_layout_position)
  if(required_aim_archive_layout_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Archives bypassed shared geometry '${required_aim_archive_layout_fragment}'")
  endif()
endforeach()
foreach(retired_aim_sort_coordinate_or_callback IN ITEMS
    "AimSortCheckBoxLoc"
    "AIM_SORT_SORT_BY_X"
    "AIM_SORT_TO_MUGSHOTS_X"
    "SelectPriceBoxRegionCallBack"
    "SelectAscendBoxRegionCallBack")
  string(FIND "${runtime_aim_sort_contents}"
    "${retired_aim_sort_coordinate_or_callback}"
    retired_aim_sort_coordinate_or_callback_position)
  if(NOT retired_aim_sort_coordinate_or_callback_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Sort restored retired split geometry '${retired_aim_sort_coordinate_or_callback}'")
  endif()
endforeach()
foreach(retired_aim_archive_coordinate_or_state IN ITEMS
    "AIM_ALUMNI_START_GRID_X"
    "AIM_ALUMNI_GRID_OFFSET_X"
    "AIM_ALUMNI_PAGE1_X"
    "AIM_POPUP_X"
    "AIM_ALUMNI_DONE_X"
    "idPage"
    "pageEnabled[0] = vOldMerc[0]")
  string(FIND "${runtime_aim_archives_contents}"
    "${retired_aim_archive_coordinate_or_state}"
    retired_aim_archive_coordinate_or_state_position)
  if(NOT retired_aim_archive_coordinate_or_state_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Archives restored retired split geometry/state '${retired_aim_archive_coordinate_or_state}'")
  endif()
endforeach()
foreach(retired_aim_members_video_coordinate IN ITEMS
    "AIM_MEMBER_VIDEO_CONF_TERMINAL_X"
    "AIM_MEMBER_BUY_CONTRACT_LENGTH_X"
    "AIM_POPUP_BOX_X")
  string(FIND "${runtime_aim_member_profile_contents}"
    "${retired_aim_members_video_coordinate}"
    retired_aim_members_video_coordinate_position)
  if(NOT retired_aim_members_video_coordinate_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Members restored retired coordinate family '${retired_aim_members_video_coordinate}'")
  endif()
endforeach()
foreach(retired_aim_facial_index_coordinate IN ITEMS
    "AIM_FI_FIRST_MUGSHOT_X"
    "AIM_FI_NUM_MUGSHOTS_X")
  string(FIND "${runtime_aim_facial_index_contents}"
    "${retired_aim_facial_index_coordinate}"
    retired_aim_facial_index_coordinate_position)
  if(NOT retired_aim_facial_index_coordinate_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Facial Index restored retired coordinate family '${retired_aim_facial_index_coordinate}'")
  endif()
endforeach()
foreach(retired_aim_policy_coordinate IN ITEMS
    "AIM_POLICY_TOC_X"
    "AIM_POLICY_MENU_X"
    "AIM_POLICY_AGREEMENT_X")
  string(FIND "${runtime_aim_policies_contents}"
    "${retired_aim_policy_coordinate}"
    retired_aim_policy_coordinate_position)
  if(NOT retired_aim_policy_coordinate_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. Policies restored retired coordinate family '${retired_aim_policy_coordinate}'")
  endif()
endforeach()

foreach(required_aim_website_layout_test_build_fragment IN ITEMS
    "aim_website_layout_tests.cpp"
    "aim_website_layout")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_aim_website_layout_test_build_fragment}"
    required_aim_website_layout_test_build_position)
  if(required_aim_website_layout_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. website layout lost focused test target '${required_aim_website_layout_test_build_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "aim_website_layout_tests" required_aim_website_layout_ci_position)
if(required_aim_website_layout_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the A.I.M. website layout target")
endif()
file(READ "${SOURCE_ROOT}/tests/aim_website_layout_tests.cpp"
  runtime_aim_website_layout_test_contents)
foreach(required_aim_website_layout_test_fragment IN ITEMS
    "A.I.M. logo drawing and hitboxes select one variant rectangle"
    "video terminal and face retain their exact authored geometry"
    "title-bar animation frames preserve legacy integer interpolation"
    "facial-index drawing and hitboxes use one row-major grid"
    "mouse and keyboard navigation share the same wrapping page transitions"
    "stale pages and partial final grids cannot expose nonexistent profiles"
    "policy table-of-contents drawing and hitboxes share one vertical sequence"
    "A.I.M. Sort navigation drawing and hitboxes share one sequence"
    "invalid A.I.M. Sort modes cannot index beyond the control model"
    "long localized sort labels cannot overflow or underflow the panel hitboxes"
    "all A.I.M. Sort geometry follows centered-screen translation"
    "archive drawing, names, and face hitboxes share one row-major grid"
    "archive popup growth keeps the done artwork and hitbox paired"
    "archive profile mapping rejects partial-page and exact-end slots"
    "archive pages remain discoverable when their first slot is empty"
    "archive navigation skips sparse pages and wraps without invalid indices")
  string(FIND "${runtime_aim_website_layout_test_contents}"
    "${required_aim_website_layout_test_fragment}"
    required_aim_website_layout_test_position)
  if(required_aim_website_layout_test_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. website layout tests lost '${required_aim_website_layout_test_fragment}'")
  endif()
endforeach()

foreach(required_aim_member_profile_integration_fragment IN ITEMS
    "#include \"AimMemberProfileLayout.h\""
    "Layout CurrentAimMemberProfileLayout()"
    "const Layout layout = CurrentAimMemberProfileLayout()"
    "CreateWeaponBoxMouseRegions(stagedResources, layout)"
    "DisplayMercsInventory(gbCurrentSoldier, layout)"
    "DisplayMercsFace(layout)"
    "DisplayMercStats(layout)"
    "DisplayAimMemberClickOnFaceHelpText(layout)"
    "layout.navigation.contactButton"
    "layout.inventory.cell")
  string(FIND "${runtime_aim_member_profile_contents}"
    "${required_aim_member_profile_integration_fragment}"
    required_aim_member_profile_integration_position)
  if(required_aim_member_profile_integration_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. member profile bypassed shared geometry '${required_aim_member_profile_integration_fragment}'")
  endif()
endforeach()
foreach(retired_aim_member_profile_coordinate IN ITEMS
    "STATS_X"
    "WEAPONBOX_X"
    "PREVIOUS_X"
    "AIM_MERC_INFO_X"
    "FIRST_COLUMN_DOT")
  string(FIND "${runtime_aim_member_profile_contents}"
    "${retired_aim_member_profile_coordinate}"
    retired_aim_member_profile_coordinate_position)
  if(NOT retired_aim_member_profile_coordinate_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. member profile restored retired coordinate family '${retired_aim_member_profile_coordinate}'")
  endif()
endforeach()

foreach(required_aim_member_profile_test_build_fragment IN ITEMS
    "aim_member_profile_layout_tests.cpp"
    "aim_member_profile_layout")
  string(FIND "${runtime_campaign_policy_test_build_contents}"
    "${required_aim_member_profile_test_build_fragment}"
    required_aim_member_profile_test_build_position)
  if(required_aim_member_profile_test_build_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. member-profile layout lost focused test target '${required_aim_member_profile_test_build_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_campaign_policy_ci_contents}"
  "aim_member_profile_layout_tests"
  required_aim_member_profile_ci_position)
if(required_aim_member_profile_ci_position EQUAL -1)
  message(FATAL_ERROR
    "AddressSanitizer CI lost the A.I.M. member-profile layout target")
endif()

file(READ "${SOURCE_ROOT}/tests/aim_member_profile_layout_tests.cpp"
  runtime_aim_member_profile_test_contents)
foreach(required_aim_member_profile_test_fragment IN ITEMS
    "profile variants preserve their exact stats-panel origins"
    "stat rows are derived from one consistent fifteen-pixel rhythm"
    "equipment cells and item origins use the same row-major mapping"
    "every expanded equipment hitbox stays inside the web page"
    "navigation controls remain non-overlapping"
    "all geometry, including dot leaders, follows centered-screen offsets"
    "profile visuals and controls remain inside the fixed Laptop web canvas")
  string(FIND "${runtime_aim_member_profile_test_contents}"
    "${required_aim_member_profile_test_fragment}"
    required_aim_member_profile_test_position)
  if(required_aim_member_profile_test_position EQUAL -1)
    message(FATAL_ERROR
      "A.I.M. member-profile tests lost '${required_aim_member_profile_test_fragment}'")
  endif()
endforeach()

foreach(required_aim_member_profile_documentation_fragment IN ITEMS
    "Post-closure Laptop visual-layout organization"
    "LaptopLayoutModel"
    "AimMemberProfileLayoutModel"
    "AimWebsiteLayoutModel"
    "MercFilesLayoutModel"
    "FloristSiteModel")
  string(FIND "${runtime_laptop_walkthrough_contents}"
    "${required_aim_member_profile_documentation_fragment}"
    required_aim_member_profile_documentation_position)
  if(required_aim_member_profile_documentation_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop walkthrough lost A.I.M. layout contract '${required_aim_member_profile_documentation_fragment}'")
  endif()
endforeach()

# Shared legacy UI infrastructure is the first post-Laptop Utils audit batch.
# Keep the state-only rules dependency-free, legacy registries implementation-
# private, ownership explicit, parsing transactional, and both model and real-
# engine failure-path coverage in the normal/ASan test matrix.
file(READ "${SOURCE_ROOT}/Utils/UtilsUiStateModel.h"
  runtime_utils_ui_state_model_contents)
foreach(required_utils_ui_state_model_fragment IN ITEMS
    "IsValidIndex"
    "ClampIncrement"
    "SliderIncrementFromPosition"
    "SliderPositionFromIncrement"
    "class BoundedIdDirectory")
  string(FIND "${runtime_utils_ui_state_model_contents}"
    "${required_utils_ui_state_model_fragment}"
    required_utils_ui_state_model_position)
  if(required_utils_ui_state_model_position EQUAL -1)
    message(FATAL_ERROR
      "Utils UI state boundary lost '${required_utils_ui_state_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/popup_class.h"
  runtime_utils_popup_header_contents)
file(READ "${SOURCE_ROOT}/Utils/popup_class.cpp"
  runtime_utils_popup_source_contents)
file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Map Inventory.cpp"
  runtime_utils_popup_map_inventory_contents)
file(READ "${SOURCE_ROOT}/Tactical/SkillMenu.cpp"
  runtime_utils_popup_skill_menu_contents)
file(READ "${SOURCE_ROOT}/Tactical/Interface Items.cpp"
  runtime_utils_popup_interface_items_contents)
foreach(required_utils_popup_owner_fragment IN ITEMS
    "std::unique_ptr<popupCallback> action"
    "std::unique_ptr<popupCallback> initCallback"
    "POPUP_OPTION(const std::wstring& name"
    "addSubMenuOption(const std::wstring& name)")
  string(FIND "${runtime_utils_popup_header_contents}"
    "${required_utils_popup_owner_fragment}"
    required_utils_popup_owner_position)
  if(required_utils_popup_owner_position EQUAL -1)
    message(FATAL_ERROR
      "Utils popup callback ownership lost '${required_utils_popup_owner_fragment}'")
  endif()
endforeach()
foreach(required_utils_popup_directory_fragment IN ITEMS
    "BoundedIdDirectory<UINT16, UINT32>"
    "gPopupRegionIndex.insert"
    "gPopupRegionIndex.erase")
  string(FIND "${runtime_utils_popup_source_contents}"
    "${required_utils_popup_directory_fragment}"
    required_utils_popup_directory_position)
  if(required_utils_popup_directory_position EQUAL -1)
    message(FATAL_ERROR
      "Utils popup region ownership lost '${required_utils_popup_directory_fragment}'")
  endif()
endforeach()
if(runtime_utils_popup_source_contents MATCHES
    "new[ \t\r\n]+PopupIndex|findMouseRegionInIndex[^;\r\n]*->|static[ \t\r\n]+BOOLEAN[ \t\r\n]+fCreated")
  message(FATAL_ERROR
    "Utils popup code restored heap callback-index nodes, unchecked callback lookup, or shared wrapper state")
endif()
foreach(utils_popup_value_string_contents IN ITEMS
    runtime_utils_popup_header_contents
    runtime_utils_popup_source_contents
    runtime_utils_popup_map_inventory_contents
    runtime_utils_popup_skill_menu_contents
    runtime_utils_popup_interface_items_contents)
  if("${${utils_popup_value_string_contents}}" MATCHES
      "&[ \t\r\n]*std::wstring|new[ \t\r\n]+std::wstring")
    message(FATAL_ERROR
      "Utils popup code restored temporary-address or heap-string ownership in ${utils_popup_value_string_contents}; pass labels by const reference")
  endif()
endforeach()
if(runtime_utils_popup_header_contents MATCHES
    "std::wstring[ \t\r\n]*\\*")
  message(FATAL_ERROR
    "Utils popup API restored ambiguous std::wstring pointer ownership; pass labels by const reference")
endif()
if(root_build_contents MATCHES
    "Wno-error=address-of-temporary")
  message(FATAL_ERROR
    "The retired popup temporary-address warning exemption returned")
endif()
file(READ "${SOURCE_ROOT}/sgp/debug_win_util.cpp"
  runtime_windows_stack_trace_contents)
if(runtime_windows_stack_trace_contents MATCHES
    "&[ \t\r\n]*\\([ \t\r\n]*SGP_LOG")
  message(FATAL_ERROR
    "Windows stack tracing restored an address of a temporary logger")
endif()

file(READ "${SOURCE_ROOT}/Utils/popup_definition.h"
  runtime_utils_popup_definition_header_contents)
foreach(required_utils_popup_definition_fragment IN ITEMS
    "std::vector<std::unique_ptr<popupDefContent>> content"
    "clone() const"
    "popupDef(const popupDef& other)")
  string(FIND "${runtime_utils_popup_definition_header_contents}"
    "${required_utils_popup_definition_fragment}"
    required_utils_popup_definition_position)
  if(required_utils_popup_definition_position EQUAL -1)
    message(FATAL_ERROR
      "Utils popup-definition ownership lost '${required_utils_popup_definition_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/PopUpBox.cpp"
  runtime_utils_popup_box_source_contents)
file(READ "${SOURCE_ROOT}/Utils/PopUpBox.h"
  runtime_utils_popup_box_header_contents)
foreach(required_utils_popup_box_fragment IN ITEMS
    "std::array<PopUpBoxPt, MAX_POPUP_BOX_COUNT> PopUpBoxList"
    "GetPopupBox(Index handle)"
    "IsValidTextLocation")
  string(FIND "${runtime_utils_popup_box_source_contents}"
    "${required_utils_popup_box_fragment}"
    required_utils_popup_box_position)
  if(required_utils_popup_box_position EQUAL -1)
    message(FATAL_ERROR
      "Utils popup-box bounds/ownership lost '${required_utils_popup_box_fragment}'")
  endif()
endforeach()
if(runtime_utils_popup_box_header_contents MATCHES
    "static[ \t\r\n]+PopUpBoxPt[ \t\r\n]+PopUpBoxList" OR
   runtime_utils_popup_box_source_contents MATCHES
    "CHAR16[ \t\r\n]+sString[ \t\r\n]*\\[[ \t\r\n]*100[ \t\r\n]*\\]")
  message(FATAL_ERROR
    "Utils popup boxes restored a header registry or fixed-size label copy")
endif()

file(READ "${SOURCE_ROOT}/Utils/Animated ProgressBar.cpp"
  runtime_utils_progress_source_contents)
file(READ "${SOURCE_ROOT}/Utils/Animated ProgressBar.h"
  runtime_utils_progress_header_contents)
foreach(required_utils_progress_fragment IN ITEMS
    "std::array<PROGRESSBAR*, MAX_PROGRESSBARS> gProgressBars"
    "PROGRESSBAR* GetProgressBar"
    "UtilsUiStateModel::IsValidIndex")
  string(FIND "${runtime_utils_progress_source_contents}"
    "${required_utils_progress_fragment}"
    required_utils_progress_position)
  if(required_utils_progress_position EQUAL -1)
    message(FATAL_ERROR
      "Utils progress-bar registry lost '${required_utils_progress_fragment}'")
  endif()
endforeach()
if(runtime_utils_progress_header_contents MATCHES
    "extern[ \t\r\n]+PROGRESSBAR[ \t\r\n]*\\*[ \t\r\n]*pBar")
  message(FATAL_ERROR
    "Utils progress-bar registry became public again")
endif()

file(READ "${SOURCE_ROOT}/Utils/Slider.cpp"
  runtime_utils_slider_contents)
foreach(required_utils_slider_fragment IN ITEMS
    "UtilsUiStateModel::SliderIncrementFromPosition"
    "UtilsUiStateModel::SliderPositionFromIncrement"
    "UniqueVideoObjectHandle gSliderBoxImage")
  string(FIND "${runtime_utils_slider_contents}"
    "${required_utils_slider_fragment}"
    required_utils_slider_position)
  if(required_utils_slider_position EQUAL -1)
    message(FATAL_ERROR
      "Utils slider safety lost '${required_utils_slider_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/MercTextBox.cpp"
  runtime_utils_merc_text_box_contents)
foreach(required_utils_merc_text_box_fragment IN ITEMS
    "gMercPopupSystemInitialized"
    "UtilsUiStateModel::IsValidIndex"
    "for ( UINT32 fillIndex")
  string(FIND "${runtime_utils_merc_text_box_contents}"
    "${required_utils_merc_text_box_fragment}"
    required_utils_merc_text_box_position)
  if(required_utils_merc_text_box_position EQUAL -1)
    message(FATAL_ERROR
      "Utils merc-text-box ownership lost '${required_utils_merc_text_box_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Text Input.cpp"
  runtime_utils_text_input_contents)
foreach(required_utils_text_input_fragment IN ITEMS
    "TEXTINPUTNODE *tail"
    "TEXTINPUTNODE *active"
    "std::size_t destinationSize"
    "curr = curr->next")
  string(FIND "${runtime_utils_text_input_contents}"
    "${required_utils_text_input_fragment}"
    required_utils_text_input_position)
  if(required_utils_text_input_position EQUAL -1)
    message(FATAL_ERROR
      "Utils text-input stack/bounds safety lost '${required_utils_text_input_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/WordWrap.cpp"
  runtime_utils_word_wrap_contents)
foreach(required_utils_word_wrap_fragment IN ITEMS
    "AppendWrappedString"
    "DeleteWrappedString(FirstWrappedString.pNextWrappedString)"
    "std::size(TempString)")
  string(FIND "${runtime_utils_word_wrap_contents}"
    "${required_utils_word_wrap_fragment}"
    required_utils_word_wrap_position)
  if(required_utils_word_wrap_position EQUAL -1)
    message(FATAL_ERROR
      "Utils word-wrap failure handling lost '${required_utils_word_wrap_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/XML_LBEPocketPopup.cpp"
  runtime_utils_popup_xml_contents)
foreach(required_utils_popup_xml_fragment IN ITEMS
    "std::map<UINT8, popupDef> parsedPopups"
    "pocketPopupParseData pData{}"
    "LBEPocketPopup[pocketId] = std::move(definition)")
  string(FIND "${runtime_utils_popup_xml_contents}"
    "${required_utils_popup_xml_fragment}"
    required_utils_popup_xml_position)
  if(required_utils_popup_xml_position EQUAL -1)
    message(FATAL_ERROR
      "LBE popup XML transactional parsing lost '${required_utils_popup_xml_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/CMakeLists.txt"
  runtime_utils_ui_test_build_contents)
file(READ "${SOURCE_ROOT}/.github/workflows/build_unix.yml"
  runtime_utils_ui_ci_contents)
foreach(utils_ui_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${utils_ui_test_manifest}}"
    "utils_ui_state_model_tests" required_utils_ui_test_position)
  if(required_utils_ui_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils UI model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/tests/utils_ui_state_model_tests.cpp"
  runtime_utils_ui_model_test_contents)
file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  runtime_utils_ui_headless_contents)
foreach(required_utils_ui_model_test_fragment IN ITEMS
    "UI indices reject negative and exact-end values"
    "slider input handles zero extent, rounding, and positions past the end"
    "callback directories reject duplicate IDs and full-capacity insertion"
    "callback teardown clears every stale mapping")
  string(FIND "${runtime_utils_ui_model_test_contents}"
    "${required_utils_ui_model_test_fragment}"
    required_utils_ui_model_test_position)
  if(required_utils_ui_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils UI model tests lost '${required_utils_ui_model_test_fragment}'")
  endif()
endforeach()
foreach(required_utils_ui_headless_fragment IN ITEMS
    "popup owners cannot be copied, moved, or constructed from ambiguous string pointers"
    "popup option teardown does not double-delete cleared callbacks"
    "popup registry reset releases all boxes and current state"
    "progress bar replacement releases the previous owned title"
    "popping a nested text-input mode restores valid head, tail, and active ownership"
    "text-input teardown drains empty and populated ownership levels")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_utils_ui_headless_fragment}"
    required_utils_ui_headless_position)
  if(required_utils_ui_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Utils UI headless coverage lost '${required_utils_ui_headless_fragment}'")
  endif()
endforeach()

# Shared text/localization infrastructure keeps buffer, persistence, and lazy-
# load rules dependency-free, while legacy adapters publish only complete
# state and use one explicitly counted cross-platform variadic formatter.
file(READ "${SOURCE_ROOT}/Utils/TextInfrastructureModel.h"
  runtime_utils_text_state_model_contents)
foreach(required_utils_text_model_fragment IN ITEMS
    "IsValidIndex"
    "CopyBounded"
    "AppendBounded"
    "CanReadSerializedText"
    "struct LazyLoadState"
    "RecordLoadResult")
  string(FIND "${runtime_utils_text_state_model_contents}"
    "${required_utils_text_model_fragment}"
    required_utils_text_model_position)
  if(required_utils_text_model_position EQUAL -1)
    message(FATAL_ERROR
      "Utils text state boundary lost '${required_utils_text_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/message.cpp"
  runtime_utils_message_contents)
foreach(required_utils_message_fragment IN ITEMS
    "sgp_vswprintf"
    "struct StagedMessageList"
    "CanReadSerializedText"
    "terminatorIndex = std::min"
    "FreeGlobalMessageList();"
    "AppendBounded(sString")
  string(FIND "${runtime_utils_message_contents}"
    "${required_utils_message_fragment}"
    required_utils_message_position)
  if(required_utils_message_position EQUAL -1)
    message(FATAL_ERROR
      "Utils message safety boundary lost '${required_utils_message_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_utils_message_contents}"
  "vswprintf(DestString, pStringA" retired_utils_message_format_position)
if(NOT retired_utils_message_format_position EQUAL -1)
  message(FATAL_ERROR
    "Utils message restored an uncounted variadic formatting sink")
endif()

file(READ "${SOURCE_ROOT}/Utils/Text Utils.cpp"
  runtime_utils_text_utils_contents)
foreach(required_utils_text_utils_fragment IN ITEMS
    "std::min<std::size_t>(gMAXITEMS_READ, MAXITEMS)"
    "LoadItemInfo(itemId, ItemNames[itemIndex], NULL)"
    "vfs::String::as_utf16"
    "vfs::String::as_utf8")
  string(FIND "${runtime_utils_text_utils_contents}"
    "${required_utils_text_utils_fragment}"
    required_utils_text_utils_position)
  if(required_utils_text_utils_position EQUAL -1)
    message(FATAL_ERROR
      "Utils text boundary lost '${required_utils_text_utils_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/LocalizedStrings.cpp"
  runtime_utils_localized_strings_contents)
foreach(required_utils_localization_fragment IN ITEMS
    "std::array<vfs::PropertyContainer, TOPIC_COUNT>"
    "const auto common = files.find"
    "const auto specific = files.find"
    "RecordLoadResult(state.lifecycle, loaded)"
    "return true;")
  string(FIND "${runtime_utils_localized_strings_contents}"
    "${required_utils_localization_fragment}"
    required_utils_localization_position)
  if(required_utils_localization_position EQUAL -1)
    message(FATAL_ERROR
      "Utils localization safety boundary lost '${required_utils_localization_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/utils_text_state_model_tests.cpp"
  runtime_utils_text_model_test_contents)
foreach(required_utils_text_model_test_fragment IN ITEMS
    "text indices reject negative, exact-end, and oversized values"
    "bounded text copy truncates and terminates oversized payloads"
    "serialized message sizes accept full buffers and reject oversized payloads"
    "failed localization loads remain retryable")
  string(FIND "${runtime_utils_text_model_test_contents}"
    "${required_utils_text_model_test_fragment}"
    required_utils_text_model_test_position)
  if(required_utils_text_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils text model tests lost '${required_utils_text_model_test_fragment}'")
  endif()
endforeach()
foreach(required_utils_text_headless_fragment IN ITEMS
    "message text replacement validates input and publishes owned storage atomically"
    "truncated map-message loads leave the live message state untouched")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_utils_text_headless_fragment}"
    required_utils_text_headless_position)
  if(required_utils_text_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Utils text headless coverage lost '${required_utils_text_headless_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/tests/platform_legacy_tests.cpp"
  runtime_utils_text_platform_test_contents)
foreach(required_utils_text_platform_fragment IN ITEMS
    "bounded variadic wide formatting preserves legacy cross-platform specifiers"
    "bounded variadic wide formatting terminates rejected oversized output")
  string(FIND "${runtime_utils_text_platform_test_contents}"
    "${required_utils_text_platform_fragment}"
    required_utils_text_platform_position)
  if(required_utils_text_platform_position EQUAL -1)
    message(FATAL_ERROR
      "Utils text platform coverage lost '${required_utils_text_platform_fragment}'")
  endif()
endforeach()

# Character-panel formatting runs immediately after the initial Laptop exit.
# Its destinations are small local arrays, so an invented pointer-style count
# corrupts the caller's stack when sgp_vswprintf enforces termination at the
# requested boundary. Keep those calls tied to their real array extents.
file(READ "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  runtime_utils_text_mapscreen_contents)
string(FIND "${runtime_utils_text_mapscreen_contents}"
  "void DrawPay(INT16 sCharNumber)" mapscreen_character_panel_start)
string(FIND "${runtime_utils_text_mapscreen_contents}"
  "// this character is in transit has an item picked up"
  mapscreen_character_panel_end)
if(mapscreen_character_panel_start EQUAL -1 OR
   mapscreen_character_panel_end EQUAL -1 OR
   mapscreen_character_panel_end LESS mapscreen_character_panel_start)
  message(FATAL_ERROR
    "Map-screen character-panel formatting boundary could not be inspected")
endif()
math(EXPR mapscreen_character_panel_length
  "${mapscreen_character_panel_end} - ${mapscreen_character_panel_start}")
string(SUBSTRING "${runtime_utils_text_mapscreen_contents}"
  ${mapscreen_character_panel_start} ${mapscreen_character_panel_length}
  mapscreen_character_panel_contents)
string(REGEX MATCH
  "sgp_swprintf\\([ \t]*sString[ \t]*,[ \t]*[0-9]+[ \t]*,"
  mapscreen_character_panel_literal_capacity
  "${mapscreen_character_panel_contents}")
if(mapscreen_character_panel_literal_capacity)
  message(FATAL_ERROR
    "Map-screen character-panel formatting must derive local array capacities")
endif()
string(FIND "${mapscreen_character_panel_contents}"
  "std::size(sString)" mapscreen_character_panel_derived_capacity)
if(mapscreen_character_panel_derived_capacity EQUAL -1)
  message(FATAL_ERROR
    "Map-screen character-panel formatting lost extent-derived bounds")
endif()

# The ETA helpers format through pointer parameters, so their capacity cannot
# be recovered inside the callee. In particular, DrawCharacterInfo owns an
# 80-element destination; claiming 128 elements makes the forced terminator
# overwrite DisplayCharacterInfo's saved x22 register on macOS/arm64.
foreach(required_mapscreen_time_capacity_fragment IN ITEMS
    "ConvertMinTimeToETADayHourMinString( pSoldier->deployment().arrivalTime(), sString, std::size(sString) )"
    "ConvertMinTimeToETADayHourMinString( uiArrivalTime, sString, std::size(sString) )"
    "void ConvertMinTimeToDayHourMinString( UINT32 uiTimeInMin, CHAR16 *sString, std::size_t capacity )"
    "sgp_swprintf( sString, capacity, L\"%02d:%02d\""
    "ConvertMinTimeToDayHourMinString( uiTimeInMin, timestring, std::size(timestring) )"
    "void ConvertMinTimeToETADayHourMinString( UINT32 uiTimeInMin, CHAR16 *sString, std::size_t capacity )"
    "sgp_swprintf( sString, capacity, L\"%s %s\"")
  string(FIND "${runtime_utils_text_mapscreen_contents}"
    "${required_mapscreen_time_capacity_fragment}"
    required_mapscreen_time_capacity_position)
  if(required_mapscreen_time_capacity_position EQUAL -1)
    message(FATAL_ERROR
      "Map-screen time formatting lost explicit capacity '${required_mapscreen_time_capacity_fragment}'")
  endif()
endforeach()
foreach(retired_mapscreen_time_capacity_fragment IN ITEMS
    "sgp_swprintf( sString, 64,"
    "sgp_swprintf( sString, 128,")
  string(FIND "${runtime_utils_text_mapscreen_contents}"
    "${retired_mapscreen_time_capacity_fragment}"
    retired_mapscreen_time_capacity_position)
  if(NOT retired_mapscreen_time_capacity_position EQUAL -1)
    message(FATAL_ERROR
      "Map-screen time formatting restored invented capacity '${retired_mapscreen_time_capacity_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  runtime_utils_text_map_interface_contents)
string(FIND "${runtime_utils_text_map_interface_contents}"
  "ConvertMinTimeToDayHourMinString( uiArrivalTime, timestring, std::size(timestring) )"
  mapscreen_map_interface_derived_time_capacity)
if(mapscreen_map_interface_derived_time_capacity EQUAL -1)
  message(FATAL_ERROR
    "Map-screen militia ETA formatting lost its array-derived capacity")
endif()

# Rebel Command had the same pointer-style 500-element assumption on local
# arrays ranging from 100 to 400 elements. The one pointer-buffer formatter is
# above this marker; local buffers below it must always derive their capacity.
file(READ "${SOURCE_ROOT}/Strategic/Rebel Command.cpp"
  runtime_utils_text_rebel_command_contents)
string(FIND "${runtime_utils_text_rebel_command_contents}"
  "void ImproveDirective(const RebelCommandDirectives directive)"
  rebel_command_local_format_start)
if(rebel_command_local_format_start EQUAL -1)
  message(FATAL_ERROR
    "Rebel Command local-formatting boundary could not be inspected")
endif()
string(SUBSTRING "${runtime_utils_text_rebel_command_contents}"
  ${rebel_command_local_format_start} -1 rebel_command_local_format_contents)
string(REGEX MATCH
  "sgp_swprintf\\([ \t]*text[ \t]*,[ \t]*[0-9]+[ \t]*,"
  rebel_command_local_literal_capacity
  "${rebel_command_local_format_contents}")
if(rebel_command_local_literal_capacity)
  message(FATAL_ERROR
    "Rebel Command local formatting must not use invented literal capacities")
endif()
foreach(utils_text_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${utils_text_test_manifest}}"
    "utils_text_state_model_tests" required_utils_text_test_position)
  if(required_utils_text_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils text model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()

# INI and property XML are shared untrusted-data boundaries. Keep conversion,
# bounded-copy, escaping, nesting, and exact-transfer rules dependency-free;
# concrete VFS adapters stage complete overlays before publication.
file(READ "${SOURCE_ROOT}/Utils/DataBoundaryModel.h"
  runtime_utils_data_boundary_model_contents)
foreach(required_utils_data_model_fragment IN ITEMS
    "ParseInt64"
    "ParseUInt64"
    "ParseDouble"
    "std::locale::classic()"
    "ParseInt32List"
    "ParseFloatList"
    "CopyString"
    "PublishTransactionally"
    "class UnknownXmlSubtree"
    "EscapeXml"
    "&#13;"
    "SanitizeXmlComment"
    "IsExactTransfer")
  string(FIND "${runtime_utils_data_boundary_model_contents}"
    "${required_utils_data_model_fragment}"
    required_utils_data_model_position)
  if(required_utils_data_model_position EQUAL -1)
    message(FATAL_ERROR
      "Utils data boundary model lost '${required_utils_data_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/INIReader.cpp"
  runtime_utils_ini_reader_contents)
file(READ "${SOURCE_ROOT}/Utils/INIReader.h"
  runtime_utils_ini_reader_header_contents)
foreach(required_utils_ini_fragment IN ITEMS
    "PublishTransactionally(properties"
    "LoadMergedIni(m_oProps, szFileName)"
    "ParseInt64(text, parsed)"
    "ParseUInt64(text, parsed)"
    "ParseDouble(text, parsed)"
    "ParseFloatList(text, staged)"
    "ParseInt32List(text, staged)"
    "vec.assign(1, 0)"
    "CopyString(input_buffer, buffer_size, value)"
    "m_legacyStringBuffer.data()")
  string(FIND "${runtime_utils_ini_reader_contents}"
    "${required_utils_ini_fragment}" required_utils_ini_position)
  if(required_utils_ini_position EQUAL -1)
    message(FATAL_ERROR
      "INI reader data boundary lost '${required_utils_ini_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_utils_ini_reader_header_contents}"
  "std::array<CHAR8, 255> m_legacyStringBuffer"
  required_utils_ini_owned_compatibility_buffer)
if(required_utils_ini_owned_compatibility_buffer EQUAL -1)
  message(FATAL_ERROR
    "INI legacy string compatibility storage is no longer reader-owned")
endif()
foreach(retired_utils_ini_fragment IN ITEMS
    "std::stof("
    "std::stoi("
    "buffer_size-1"
    "new char[255]"
    "m_oProps.initFromIniFile")
  string(FIND "${runtime_utils_ini_reader_contents}"
    "${retired_utils_ini_fragment}" retired_utils_ini_position)
  if(NOT retired_utils_ini_position EQUAL -1)
    message(FATAL_ERROR
      "INI reader restored unsafe '${retired_utils_ini_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/XMLProperties.cpp"
  runtime_utils_xml_properties_contents)
foreach(required_utils_xml_property_fragment IN ITEMS
    "UnknownXmlSubtree unknown_subtree"
    "unknown_subtree.active()"
    "readRequiredAttribute"
    "IsExactTransfer(size, bytesRead)"
    "PropertyContainer staged(destination)"
    "PropertyParserContext context(staged, tagmap)"
    "context.document.complete()"
    "destination = std::move(staged)")
  string(FIND "${runtime_utils_xml_properties_contents}"
    "${required_utils_xml_property_fragment}"
    required_utils_xml_property_position)
  if(required_utils_xml_property_position EQUAL -1)
    message(FATAL_ERROR
      "Property XML transactional boundary lost '${required_utils_xml_property_fragment}'")
  endif()
endforeach()
foreach(retired_utils_xml_property_fragment IN ITEMS
    "PropertyParserContext context(*this"
    "rfile.release()"
    "&buffer[0]")
  string(FIND "${runtime_utils_xml_properties_contents}"
    "${retired_utils_xml_property_fragment}"
    retired_utils_xml_property_position)
  if(NOT retired_utils_xml_property_position EQUAL -1)
    message(FATAL_ERROR
      "Property XML restored partial/manual state '${retired_utils_xml_property_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/XMLWriter.cpp"
  runtime_utils_xml_writer_contents)
file(READ "${SOURCE_ROOT}/Utils/XMLWriter.h"
  runtime_utils_xml_writer_header_contents)
foreach(required_utils_xml_writer_fragment IN ITEMS
    "addEscapedAttribute(attribute, temp_buffer.str())"
    "addEscapedValue(key, temp_buffer.str())"
    "temp_buffer.imbue(std::locale::classic())"
    "SanitizeXmlComment"
    "EscapeXml(value, escaped)"
    "if (!isComplete()) return false"
    "if (!pFile || !isComplete()) return false"
    "UtilsDataBoundaryModel::IsExactTransfer("
    "file->isOpenRead() || file->isOpenWrite()"
    "file->openWrite(true, true)"
    "file->close()"
    "m_ssBuffer.good()"
    "replaceFileAtomically(sFileName")
  set(utils_xml_writer_combined
    "${runtime_utils_xml_writer_header_contents}\n${runtime_utils_xml_writer_contents}")
  string(FIND "${utils_xml_writer_combined}"
    "${required_utils_xml_writer_fragment}"
    required_utils_xml_writer_position)
  if(required_utils_xml_writer_position EQUAL -1)
    message(FATAL_ERROR
      "XML writer integrity boundary lost '${required_utils_xml_writer_fragment}'")
  endif()
endforeach()

# XML path publication is one physical-and-catalogue transaction. Keep the
# same-directory native replacement, injectable rollback seam, prepared VFS
# entry, mount-aware physical resolution, and CVFS serialization together.
file(READ "${SOURCE_ROOT}/ext/VFS/include/vfs/Core/vfs.h"
  runtime_vfs_atomic_header_contents)
file(READ "${SOURCE_ROOT}/ext/VFS/include/vfs/Core/vfs_os_functions.h"
  runtime_vfs_atomic_os_header_contents)
file(READ "${SOURCE_ROOT}/ext/VFS/include/vfs/Core/vfs_vloc.h"
  runtime_vfs_atomic_location_header_contents)
file(READ "${SOURCE_ROOT}/ext/VFS/src/Core/vfs.cpp"
  runtime_vfs_atomic_contents)
file(READ "${SOURCE_ROOT}/ext/VFS/src/Core/vfs_os_functions.cpp"
  runtime_vfs_atomic_os_contents)
file(READ "${SOURCE_ROOT}/ext/VFS/src/Core/Location/vfs_directory_tree.cpp"
  runtime_vfs_atomic_tree_contents)
file(READ "${SOURCE_ROOT}/ext/VFS/src/Core/vfs_vloc.cpp"
  runtime_vfs_atomic_location_contents)
set(runtime_vfs_atomic_combined
  "${runtime_vfs_atomic_header_contents}\n${runtime_vfs_atomic_os_header_contents}\n${runtime_vfs_atomic_location_header_contents}\n${runtime_vfs_atomic_contents}\n${runtime_vfs_atomic_os_contents}\n${runtime_vfs_atomic_tree_contents}\n${runtime_vfs_atomic_location_contents}")
foreach(required_vfs_atomic_fragment IN ITEMS
    "class VFS_API IAtomicFileReplacement"
    "replaceFileAtomically("
    "std::recursive_mutex m_mutex"
    "class VFS_API CAtomicFileReplacement"
    "bool ownsTemporary;"
    "m_impl->prepared || m_impl->ownsTemporary"
    "O_WRONLY | O_CREAT | O_EXCL"
    "::fsync(file)"
    "::rename(temporary.c_str(), target.c_str())"
    "(std::numeric_limits<DWORD>::max)()"
    "MOVEFILE_WRITE_THROUGH"
    "if(vfs::OS::deleteRealFile(candidate))"
    "if(::unlink(temporary.c_str()) == 0)"
    "class VFS_API PreparedFileEntry"
    "prepareFileEntry("
    "removePreparedFileFromCatalogue("
    "resolveRealFilePath("
    "currentDirectory->getRealPath() + component")
  string(FIND "${runtime_vfs_atomic_combined}"
    "${required_vfs_atomic_fragment}" required_vfs_atomic_position)
  if(required_vfs_atomic_position EQUAL -1)
    message(FATAL_ERROR
      "Atomic VFS publication boundary lost '${required_vfs_atomic_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_vfs_atomic_os_contents}"
  "m_impl->temporary = candidate;" vfs_atomic_staged_path_position)
string(FIND "${runtime_vfs_atomic_os_contents}"
  "CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL)"
  vfs_atomic_windows_exclusive_create_position)
string(FIND "${runtime_vfs_atomic_os_contents}"
  "::open(temporary.c_str(), flags" vfs_atomic_posix_exclusive_create_position)
string(FIND "${runtime_vfs_atomic_os_contents}"
  "m_impl->ownsTemporary = true;" vfs_atomic_windows_ownership_position)
if(vfs_atomic_staged_path_position EQUAL -1 OR
   vfs_atomic_windows_exclusive_create_position EQUAL -1 OR
   vfs_atomic_posix_exclusive_create_position EQUAL -1 OR
   vfs_atomic_windows_ownership_position EQUAL -1 OR
   NOT vfs_atomic_staged_path_position LESS vfs_atomic_windows_exclusive_create_position OR
   NOT vfs_atomic_staged_path_position LESS vfs_atomic_posix_exclusive_create_position OR
   NOT vfs_atomic_windows_exclusive_create_position LESS vfs_atomic_windows_ownership_position OR
   NOT vfs_atomic_windows_ownership_position LESS vfs_atomic_posix_exclusive_create_position)
  message(FATAL_ERROR
    "Atomic VFS replacement must stage its Path before and claim ownership after native exclusive creation")
endif()
string(SUBSTRING "${runtime_vfs_atomic_os_contents}"
  ${vfs_atomic_posix_exclusive_create_position} -1
  runtime_vfs_atomic_posix_create_contents)
string(FIND "${runtime_vfs_atomic_posix_create_contents}"
  "m_impl->ownsTemporary = true;" vfs_atomic_posix_ownership_position)
if(vfs_atomic_posix_ownership_position EQUAL -1)
  message(FATAL_ERROR
    "Atomic VFS replacement must claim POSIX temporary ownership after exclusive creation")
endif()
string(FIND "${runtime_vfs_atomic_os_contents}"
  "bool vfs::OS::CAtomicFileReplacement::publish() noexcept"
  vfs_atomic_publish_start)
if(vfs_atomic_publish_start EQUAL -1)
  message(FATAL_ERROR "Atomic VFS replacement lost its no-throw publish boundary")
endif()
string(SUBSTRING "${runtime_vfs_atomic_os_contents}"
  ${vfs_atomic_publish_start} -1 runtime_vfs_atomic_publish_contents)
string(FIND "${runtime_vfs_atomic_publish_contents}"
  "committed = true;" vfs_atomic_committed_position)
string(FIND "${runtime_vfs_atomic_publish_contents}"
  "m_impl->ownsTemporary = false;" vfs_atomic_release_position)
if(vfs_atomic_committed_position EQUAL -1 OR
   vfs_atomic_release_position EQUAL -1 OR
   NOT vfs_atomic_committed_position LESS vfs_atomic_release_position)
  message(FATAL_ERROR
    "Atomic VFS replacement must record native commitment before releasing temp ownership")
endif()
foreach(retired_vfs_atomic_fragment IN ITEMS
    "MOVEFILE_COPY_ALLOWED"
    "std::numeric_limits<DWORD>::max()")
  string(FIND "${runtime_vfs_atomic_combined}"
    "${retired_vfs_atomic_fragment}" retired_vfs_atomic_position)
  if(NOT retired_vfs_atomic_position EQUAL -1)
    message(FATAL_ERROR
      "Atomic VFS publication restored cross-volume replacement '${retired_vfs_atomic_fragment}'")
  endif()
endforeach()
string(REGEX MATCHALL
  "temp_buffer\\.imbue\\(std::locale::classic\\(\\)\\)"
  utils_xml_writer_classic_numeric_streams
  "${runtime_utils_xml_writer_header_contents}")
list(LENGTH utils_xml_writer_classic_numeric_streams
  utils_xml_writer_classic_numeric_stream_count)
if(NOT utils_xml_writer_classic_numeric_stream_count EQUAL 2)
  message(FATAL_ERROR
    "Every XML writer numeric stream must use the classic locale")
endif()

file(READ "${SOURCE_ROOT}/tests/utils_data_boundary_model_tests.cpp"
  runtime_utils_data_model_test_contents)
foreach(required_utils_data_model_test_fragment IN ITEMS
    "numeric conversion rejects trailing text, overflow, signs outside the domain, and non-finite values"
    "floating conversion remains locale-independent after LC_NUMERIC changes"
    "malformed and overflowing integer arrays leave prior state untouched"
    "bounded INI strings reject zero-capacity and null destinations without writing"
    "failed data loads leave the published state untouched"
    "unknown property XML subtrees are skipped with balanced nesting depth"
    "XML text and attribute payloads escape all reserved characters"
    "file transfers succeed only when every requested byte is written")
  string(FIND "${runtime_utils_data_model_test_contents}"
    "${required_utils_data_model_test_fragment}"
    required_utils_data_model_test_position)
  if(required_utils_data_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils data boundary model tests lost '${required_utils_data_model_test_fragment}'")
  endif()
endforeach()
foreach(required_utils_data_headless_fragment IN ITEMS
    "INI reader enforces null and zero-capacity strings, strict numbers, whole-list fallback, and owned compatibility text"
    "INI merged loading reports found only after a profile contributes a valid file"
    "property XML overlays atomically while ignoring balanced unknown subtrees"
    "truncated property XML leaves the published overlay untouched"
    "property XML missing required identifiers rolls back staged state"
    "property XML writer escapes attribute identifiers and text for round-trip"
    "XML writer numeric output stays locale-independent"
    "failed XML writer close poisons the document before file truncation")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_utils_data_headless_fragment}"
    required_utils_data_headless_position)
  if(required_utils_data_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Utils property XML headless coverage lost '${required_utils_data_headless_fragment}'")
  endif()
endforeach()
foreach(utils_data_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${utils_data_test_manifest}}"
    "utils_data_boundary_model_tests" required_utils_data_test_position)
  if(required_utils_data_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils data boundary model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()

# The two shared indexed localization loaders preserve sparse overlays and
# duplicate-last-wins order, but they may not publish any record until the
# complete document has passed bounded index, UTF conversion, and text-capacity
# validation. Keep their reusable rules independent from Expat and JA2 globals.
file(READ "${SOURCE_ROOT}/Utils/IndexedXmlModel.h"
  runtime_utils_indexed_xml_model_contents)
foreach(required_utils_indexed_xml_model_fragment IN ITEMS
    "ParseBoundedIndex"
    "IndexSyntax::CStyleUnsigned"
    "enum class StageResult"
    "class StagedIndexedText"
    "StageResult stage"
    "void publish")
  string(FIND "${runtime_utils_indexed_xml_model_contents}"
    "${required_utils_indexed_xml_model_fragment}"
    required_utils_indexed_xml_model_position)
  if(required_utils_indexed_xml_model_position EQUAL -1)
    message(FATAL_ERROR
      "Utils indexed XML model lost '${required_utils_indexed_xml_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/XML_Language.cpp"
  runtime_utils_language_xml_contents)
foreach(required_utils_language_xml_fragment IN ITEMS
    "LanguageLocationParseData data("
    "IndexedXmlModel::StagedIndexedText<std::wstring> staged"
    "IndexedXmlModel::ParseBoundedIndex("
    "ConvertUtf8Text<MAX_MESSAGE_NAMES_CHARS>"
    "data.staged.publish"
    "if (!data.localized)"
    "if (data.fileType == TacticalMessageFileType)")
  string(FIND "${runtime_utils_language_xml_contents}"
    "${required_utils_language_xml_fragment}"
    required_utils_language_xml_position)
  if(required_utils_language_xml_position EQUAL -1)
    message(FATAL_ERROR
      "Utils language XML transaction lost '${required_utils_language_xml_fragment}'")
  endif()
endforeach()
foreach(retired_utils_language_xml_fragment IN ITEMS
    "LanguageLocation_TextOnly"
    "FileTypeXml"
    "LANGUAGE_LOCATION *pLang"
    "strtoul("
    "wcscpy(")
  string(FIND "${runtime_utils_language_xml_contents}"
    "${retired_utils_language_xml_fragment}"
    retired_utils_language_xml_position)
  if(NOT retired_utils_language_xml_position EQUAL -1)
    message(FATAL_ERROR
      "Utils language XML restored unsafe global/direct parsing '${retired_utils_language_xml_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/XML_SenderNameList.cpp"
  runtime_utils_sender_xml_contents)
foreach(required_utils_sender_xml_fragment IN ITEMS
    "SenderNameParseData data"
    "IndexedXmlModel::StagedIndexedText<std::wstring> staged"
    "IndexedXmlModel::ParseBoundedIndex("
    "ConvertUtf8Name<MAX_SENDER_NAMES_CHARS>"
    "data.staged.publish")
  string(FIND "${runtime_utils_sender_xml_contents}"
    "${required_utils_sender_xml_fragment}"
    required_utils_sender_xml_position)
  if(required_utils_sender_xml_position EQUAL -1)
    message(FATAL_ERROR
      "Utils sender-name XML transaction lost '${required_utils_sender_xml_fragment}'")
  endif()
endforeach()
foreach(retired_utils_sender_xml_fragment IN ITEMS
    "SenderNameList_TextOnly"
    "atol("
    "wcscpy(")
  string(FIND "${runtime_utils_sender_xml_contents}"
    "${retired_utils_sender_xml_fragment}"
    retired_utils_sender_xml_position)
  if(NOT retired_utils_sender_xml_position EQUAL -1)
    message(FATAL_ERROR
      "Utils sender-name XML restored unsafe global/direct parsing '${retired_utils_sender_xml_fragment}'")
  endif()
endforeach()

foreach(utils_indexed_xml_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${utils_indexed_xml_test_manifest}}"
    "utils_indexed_xml_model_tests" required_utils_indexed_xml_test_position)
  if(required_utils_indexed_xml_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils indexed XML model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/tests/utils_indexed_xml_model_tests.cpp"
  runtime_utils_indexed_xml_test_contents)
foreach(required_utils_indexed_xml_test_fragment IN ITEMS
    "language indices accept 999 and reject exact-end 1000"
    "sender indices accept 499 and reject exact-end 500"
    "indexed XML rejects negative, overflowing, and partial numeric text"
    "publication preserves sparse entries and duplicate-last-wins order"
    "rejects exact-end, oversized, and zero-capacity records")
  string(FIND "${runtime_utils_indexed_xml_test_contents}"
    "${required_utils_indexed_xml_test_fragment}"
    required_utils_indexed_xml_test_position)
  if(required_utils_indexed_xml_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils indexed XML model tests lost '${required_utils_indexed_xml_test_fragment}'")
  endif()
endforeach()
foreach(required_utils_indexed_xml_platform_fragment IN ITEMS
    "sender-name XML preserves sparse, duplicate-last-wins, unknown-tag, and index-499 behavior"
    "malformed sender-name XML preserves the complete live table"
    "sender-name XML rejects oversized converted text without publication"
    "language XML preserves sparse, duplicate-last-wins, unknown-tag, and index-999 behavior"
    "localized language XML overlays text without replacing base metadata"
    "malformed language XML preserves text and metadata atomically"
    "language XML validates text capacity, numeric overflow, and localized missing-file policy")
  string(FIND "${runtime_utils_text_platform_test_contents}"
    "${required_utils_indexed_xml_platform_fragment}"
    required_utils_indexed_xml_platform_position)
  if(required_utils_indexed_xml_platform_position EQUAL -1)
    message(FATAL_ERROR
      "Utils indexed XML platform coverage lost '${required_utils_indexed_xml_platform_fragment}'")
  endif()
endforeach()

# Legacy compatibility utilities retain only counted, transactional behavior.
# Text/path decisions stay dependency-free, FileMan handles have one owner, and
# the uncompiled DirectSound-era Win Util pair must not return as a platform
# abstraction.
file(READ "${SOURCE_ROOT}/Utils/LegacyUtilitiesModel.h"
  runtime_legacy_utilities_model_contents)
foreach(required_legacy_utilities_model_fragment IN ITEMS
    "struct WideTextSplit"
    "SplitWideText("
    "overflowIndex >= text.size()"
    "JoinPath("
    "relative.size() > capacity - 1 - root.size()"
    "destination = std::move(staged)")
  string(FIND "${runtime_legacy_utilities_model_contents}"
    "${required_legacy_utilities_model_fragment}"
    required_legacy_utilities_model_position)
  if(required_legacy_utilities_model_position EQUAL -1)
    message(FATAL_ERROR
      "Legacy utilities model lost '${required_legacy_utilities_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Utilities.h"
  runtime_legacy_utilities_header_contents)
foreach(required_legacy_utilities_header_fragment IN ITEMS
    "std::size_t destinationCapacity"
    "std::size_t strCapacity"
    "CHAR8 (&pDestination)[Capacity]"
    "CHAR16 (&pStr)[FirstCapacity]")
  string(FIND "${runtime_legacy_utilities_header_contents}"
    "${required_legacy_utilities_header_fragment}"
    required_legacy_utilities_header_position)
  if(required_legacy_utilities_header_position EQUAL -1)
    message(FATAL_ERROR
      "Legacy utilities counted API lost '${required_legacy_utilities_header_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Utilities.cpp"
  runtime_legacy_utilities_source_contents)
foreach(required_legacy_utilities_source_fragment IN ITEMS
    "#include \"LegacyUtilitiesModel.h\""
    "UniqueResourceHandle<"
    "bytesRead != bytes.size()"
    "std::wmemchr("
    "LegacyUtilitiesModel::SplitWideText("
    "LegacyUtilitiesModel::JoinPath("
    "if (!file) return FALSE")
  string(FIND "${runtime_legacy_utilities_source_contents}"
    "${required_legacy_utilities_source_fragment}"
    required_legacy_utilities_source_position)
  if(required_legacy_utilities_source_position EQUAL -1)
    message(FATAL_ERROR
      "Legacy utilities production boundary lost '${required_legacy_utilities_source_fragment}'")
  endif()
endforeach()
foreach(retired_legacy_utilities_fragment IN ITEMS
    "strcpy("
    "wcscpy("
    "sprintf("
    "#ifdef _WIN32"
    "IfWinNT("
    "IfWin95("
    "HandleLimitedNumExecutions("
    "HandleJA2CDCheckTwo("
    "PerformTimeLimitedCheck(")
  string(FIND "${runtime_legacy_utilities_source_contents}"
    "${retired_legacy_utilities_fragment}"
    retired_legacy_utilities_position)
  if(NOT retired_legacy_utilities_position EQUAL -1)
    message(FATAL_ERROR
      "Legacy utilities restored retired unsafe/dead path '${retired_legacy_utilities_fragment}'")
  endif()
endforeach()
if(EXISTS "${SOURCE_ROOT}/Utils/Win Util.cpp" OR
   EXISTS "${SOURCE_ROOT}/Utils/Win Util.h")
  message(FATAL_ERROR
    "The retired uncompiled DirectSound-era Win Util pair must stay removed")
endif()

file(READ "${SOURCE_ROOT}/tests/utils_legacy_utilities_model_tests.cpp"
  runtime_legacy_utilities_model_test_contents)
foreach(required_legacy_utilities_model_test_fragment IN ITEMS
    "wide text wraps at the last in-range space"
    "legacy hyphen fallback without underflow"
    "absent overflow leaves the prior split untouched"
    "path joins reject overflow transactionally and accept the exact limit")
  string(FIND "${runtime_legacy_utilities_model_test_contents}"
    "${required_legacy_utilities_model_test_fragment}"
    required_legacy_utilities_model_test_position)
  if(required_legacy_utilities_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Legacy utilities model coverage lost '${required_legacy_utilities_model_test_fragment}'")
  endif()
endforeach()
foreach(legacy_utilities_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${legacy_utilities_test_manifest}}"
    "utils_legacy_utilities_model_tests"
    required_legacy_utilities_test_manifest_position)
  if(required_legacy_utilities_test_manifest_position EQUAL -1)
    message(FATAL_ERROR
      "Legacy utilities model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()
foreach(required_legacy_utilities_headless_fragment IN ITEMS
    "COL palette loading publishes every RGB entry and preserves caller flags"
    "truncated COL palette input leaves the complete caller palette untouched")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_legacy_utilities_headless_fragment}"
    required_legacy_utilities_headless_position)
  if(required_legacy_utilities_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Legacy utilities FileMan coverage lost '${required_legacy_utilities_headless_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/docs/UTILS_CODE_WALKTHROUGH.md"
  runtime_utils_indexed_xml_walkthrough_contents)
foreach(required_utils_indexed_xml_documentation_fragment IN ITEMS
    "Indexed localization XML boundary"
    "IndexedXmlModel.h"
    "duplicate-last-wins"
    "utils_indexed_xml_model_tests")
  string(FIND "${runtime_utils_indexed_xml_walkthrough_contents}"
    "${required_utils_indexed_xml_documentation_fragment}"
    required_utils_indexed_xml_documentation_position)
  if(required_utils_indexed_xml_documentation_position EQUAL -1)
    message(FATAL_ERROR
      "Utils indexed XML walkthrough lost '${required_utils_indexed_xml_documentation_fragment}'")
  endif()
endforeach()

# Cursor selection and rendering geometry stay data-free while the concrete
# legacy gateway owns lazy image resources and validates every raw catalogue
# boundary before touching SDL3's fixed-capacity cursor storage.
file(READ "${SOURCE_ROOT}/Utils/CursorStateModel.h"
  runtime_utils_cursor_model_contents)
foreach(required_utils_cursor_model_fragment IN ITEMS
    "ResolveSurface"
    "NextAnimationFrame"
    "AdvanceFlash"
    "TrySelectMouseLevelOffset"
    "BoundedCount"
    "OneBasedIndex"
    "ComputeChanceBarGeometry"
    "ChanceBarFillPixels"
    "ClampToInt"
    "PixelOffset")
  string(FIND "${runtime_utils_cursor_model_contents}"
    "${required_utils_cursor_model_fragment}"
    required_utils_cursor_model_position)
  if(required_utils_cursor_model_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cursor state model lost '${required_utils_cursor_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Cursors.cpp"
  runtime_utils_cursor_contents)
foreach(required_utils_cursor_fragment IN ITEMS
    "static_assert(std::size(CursorFileDatabase) == NUM_CURSOR_FILES"
    "static_assert(std::size(CursorDatabase) == NUM_CURSORS"
    "CursorStateModel::TrySelectMouseLevelOffset"
    "CursorStateModel::ResolveSurface"
    "CursorStateModel::NextAnimationFrame"
    "CursorStateModel::AdvanceFlash"
    "CursorStateModel::BoundedCount"
    "CursorStateModel::PixelOffset"
    "class MouseBufferLock"
    "MAX_CURSOR_WIDTH, MAX_CURSOR_HEIGHT"
    "mprintf( sX, sY + 12, L\"%ls\", gzLocation )")
  string(FIND "${runtime_utils_cursor_contents}"
    "${required_utils_cursor_fragment}"
    required_utils_cursor_position)
  if(required_utils_cursor_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cursor boundary lost '${required_utils_cursor_fragment}'")
  endif()
endforeach()
foreach(retired_utils_cursor_fragment IN ITEMS
    "gsMouseGlobalYOffsets[ bLevel ]"
    "ptrBuf["
    "gbCtH[ i ]"
    "pCurImage->uiCurrentFrame++"
    "mprintf( sX, sY + 12, gzLocation )"
    "SDL_"
    "#ifdef _WIN32")
  string(FIND "${runtime_utils_cursor_contents}"
    "${retired_utils_cursor_fragment}"
    retired_utils_cursor_position)
  if(NOT retired_utils_cursor_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cursor path restored unsafe/platform-coupled '${retired_utils_cursor_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/UI Cursors.cpp"
  runtime_tactical_cursor_producer_contents)
foreach(required_tactical_cursor_producer_fragment IN ITEMS
    "gbCtHBurstCount > CTH_BURST_SAMPLE_CAPACITY"
    "CTH_BURST_SAMPLE_CAPACITY);")
  string(FIND "${runtime_tactical_cursor_producer_contents}"
    "${required_tactical_cursor_producer_fragment}"
    required_tactical_cursor_producer_position)
  if(required_tactical_cursor_producer_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical CTH producer lost '${required_tactical_cursor_producer_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/sgp/Cursor Control.cpp"
  runtime_sgp_cursor_gateway_contents)
foreach(required_sgp_cursor_gateway_fragment IN ITEMS
    "gusNumCursorData"
    "using ImageOwner"
    "class CursorFileLoadTransaction"
    "HasValidCompositeCount"
    "TryGetRegion"
    "if (gfCursorDatabaseInit) CursorDatabaseClear()"
    "CursorData stagedCursor"
    "guiOldSetCursor = VIDEO_NO_CURSOR"
    "if (!LoadCursorData(uiCursorIndex)) return FALSE"
    "const CursorData cursorSnapshot = cursor"
    "gpCursorFileDatabase[snapshot.index] = snapshot.value")
  string(FIND "${runtime_sgp_cursor_gateway_contents}"
    "${required_sgp_cursor_gateway_fragment}"
    required_sgp_cursor_gateway_position)
  if(required_sgp_cursor_gateway_position EQUAL -1)
    message(FATAL_ERROR
      "SGP cursor resource gateway lost '${required_sgp_cursor_gateway_fragment}'")
  endif()
endforeach()
foreach(retired_sgp_cursor_gateway_fragment IN ITEMS
    "pETRLEObject[ usVideoObjectSubIndex ]"
    "UnLoadCursorData"
    "DestroyImage( hImage )"
    "SDL_")
  string(FIND "${runtime_sgp_cursor_gateway_contents}"
    "${retired_sgp_cursor_gateway_fragment}"
    retired_sgp_cursor_gateway_position)
  if(NOT retired_sgp_cursor_gateway_position EQUAL -1)
    message(FATAL_ERROR
      "SGP cursor gateway restored unchecked ownership/platform path '${retired_sgp_cursor_gateway_fragment}'")
  endif()
endforeach()

foreach(utils_cursor_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${utils_cursor_test_manifest}}"
    "utils_cursor_state_model_tests" required_utils_cursor_test_position)
  if(required_utils_cursor_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cursor model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/tests/utils_cursor_state_model_tests.cpp"
  runtime_utils_cursor_model_test_contents)
foreach(required_utils_cursor_model_test_fragment IN ITEMS
    "cursor indices reject negative, exact-end, and oversized values"
    "cursor surface selection round-trips legacy and NCTH families"
    "animation frames wrap stale and zero-frame metadata safely"
    "mouse levels reject negative and exact-end values without publication"
    "chance-bar geometry stays signed and clips every fixed-buffer write"
    "chance-bar arithmetic saturates coordinates and avoids size overflow"
    "chance-bar shot counts clamp to the CTH sample capacity"
    "flash transitions preserve sound-on-visible-frame behavior"
    "one-based AP shades clamp oversized requests")
  string(FIND "${runtime_utils_cursor_model_test_contents}"
    "${required_utils_cursor_model_test_fragment}"
    required_utils_cursor_model_test_position)
  if(required_utils_cursor_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cursor model tests lost '${required_utils_cursor_model_test_fragment}'")
  endif()
endforeach()
foreach(required_utils_cursor_headless_fragment IN ITEMS
    "cursor gateway rejects negative and exact-end mouse levels without publication"
    "cursor gateways reject exact-end catalogue IDs before array or resource access"
    "cursor loads normalize stale animated frames before 16-bit subimage selection"
    "cursor database clear drops borrowed references without destroying them"
    "borrowed cursor replacement rolls back when a companion asset fails"
    "missing cursor assets fail without publishing or dereferencing a partial load"
    "cursor database replacement retires the previous cache lifetime")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_utils_cursor_headless_fragment}"
    required_utils_cursor_headless_position)
  if(required_utils_cursor_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cursor headless coverage lost '${required_utils_cursor_headless_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/docs/UTILS_CODE_WALKTHROUGH.md"
  runtime_utils_cursor_walkthrough_contents)
foreach(required_utils_cursor_documentation_fragment IN ITEMS
    "Cursor state and resource boundary"
    "CursorStateModel.h"
    "fixed 64-by-64"
    "explicitly borrowed"
    "sdl_video.cpp"
    "No unaudited Utils inventory remains")
  string(FIND "${runtime_utils_cursor_walkthrough_contents}"
    "${required_utils_cursor_documentation_fragment}"
    required_utils_cursor_documentation_position)
  if(required_utils_cursor_documentation_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cursor walkthrough lost '${required_utils_cursor_documentation_fragment}'")
  endif()
endforeach()

# Shared media adapters keep callback incarnation, clipped frame geometry, PCM
# narrowing, volume, and fixed-slot rules dependency-free. The concrete legacy
# gateways must validate before touching arrays/opaque handles and release
# borrowing resources in a deterministic order.
file(READ "${SOURCE_ROOT}/Utils/MediaLifecycleModel.h"
  runtime_utils_media_model_contents)
foreach(required_utils_media_model_fragment IN ITEMS
    "class PlaybackEpoch"
    "ComputeClippedBlit"
    "IsSupportedAudioFormat"
    "CanQueueAudioChunk"
    "HasElapsedMicroseconds"
    "ActivePrefixSize"
    "ScaleVolumeByDistance")
  string(FIND "${runtime_utils_media_model_contents}"
    "${required_utils_media_model_fragment}"
    required_utils_media_model_position)
  if(required_utils_media_model_position EQUAL -1)
    message(FATAL_ERROR
      "Utils media lifecycle model lost '${required_utils_media_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Cinematics.cpp"
  runtime_utils_cinematics_contents)
foreach(required_utils_cinematics_fragment IN ITEMS
    "FileHandleOwner"
    "SmkOwnsFlic"
    "ReleaseFlic"
    "MediaLifecycleModel::ComputeClippedBlit"
    "MediaLifecycleModel::IsSupportedAudioFormat"
    "MediaLifecycleModel::CanQueueAudioChunk"
    "SDL_AudioSpec spec{}"
    "MediaLifecycleModel::HasElapsedMicroseconds")
  string(FIND "${runtime_utils_cinematics_contents}"
    "${required_utils_cinematics_fragment}"
    required_utils_cinematics_position)
  if(required_utils_cinematics_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cinematic ownership/bounds boundary lost '${required_utils_cinematics_fragment}'")
  endif()
endforeach()
foreach(retired_utils_cinematics_fragment IN ITEMS
    "gSmkList[i] = SMKFLIC{}"
    "SDL_PutAudioStreamData(f.audioStream.get(), pcm, (int)sz)")
  string(FIND "${runtime_utils_cinematics_contents}"
    "${retired_utils_cinematics_fragment}"
    retired_utils_cinematics_position)
  if(NOT retired_utils_cinematics_position EQUAL -1)
    message(FATAL_ERROR
      "Utils cinematic path restored unsafe '${retired_utils_cinematics_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Cinematics Bink.cpp"
  runtime_utils_bink_contents)
foreach(required_utils_bink_fragment IN ITEMS
    "BOOLEAN  BinkPollFlics(void)"
    "void     BinkCloseFlic(BINKFLIC*)"
    "void     BinkShutdownVideo(void)"
    "return nullptr")
  string(FIND "${runtime_utils_bink_contents}"
    "${required_utils_bink_fragment}"
    required_utils_bink_position)
  if(required_utils_bink_position EQUAL -1)
    message(FATAL_ERROR
      "Unsupported Bink compatibility boundary lost '${required_utils_bink_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Music Control.cpp"
  runtime_utils_music_contents)
foreach(required_utils_music_fragment IN ITEMS
    "using MusicListEntries = std::array<std::vector<std::string>"
    "MusicListEntries stagedLists"
    "MusicListEntry"
    "gMusicPlayback.begin()"
    "gMusicPlayback.cancel()"
    "gMusicPlayback.accept(playbackToken)"
    "ClampFadeSpeed")
  string(FIND "${runtime_utils_music_contents}"
    "${required_utils_music_fragment}"
    required_utils_music_position)
  if(required_utils_music_position EQUAL -1)
    message(FATAL_ERROR
      "Utils music ownership/callback boundary lost '${required_utils_music_fragment}'")
  endif()
endforeach()
foreach(retired_utils_music_fragment IN ITEMS
    "std::vector<STR> MusicLists"
    "MemAlloc(buf"
    "MemFree(const_cast<CHAR8*>")
  string(FIND "${runtime_utils_music_contents}"
    "${retired_utils_music_fragment}"
    retired_utils_music_position)
  if(NOT retired_utils_music_position EQUAL -1)
    message(FATAL_ERROR
      "Utils music restored raw filename ownership '${retired_utils_music_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Sound Control.cpp"
  runtime_utils_sound_control_contents)
foreach(required_utils_sound_fragment IN ITEMS
    "IsValidSoundEffect"
    "IsValidAmbient"
    "HasSoundFilename"
    "CalculateWeaponVolume"
    "MediaLifecycleModel::ScaleVolume"
    "MediaLifecycleModel::ActivePrefixSize"
    "MAX_POSITION_SOUND_EFFECT_SLOTS"
    "ResetPositionSounds"
    "SoundIsPlaying")
  string(FIND "${runtime_utils_sound_control_contents}"
    "${required_utils_sound_fragment}"
    required_utils_sound_position)
  if(required_utils_sound_position EQUAL -1)
    message(FATAL_ERROR
      "Utils sound gateway/lifecycle boundary lost '${required_utils_sound_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/utils_media_lifecycle_model_tests.cpp"
  runtime_utils_media_model_test_contents)
foreach(required_utils_media_model_test_fragment IN ITEMS
    "media indices reject negative, exact-end, and oversized values"
    "stale playback callbacks cannot retire a replacement track"
    "cinematic blits reject oversized positions and zero extents"
    "cinematic audio validates formats and the SDL queue length boundary"
    "positional sound recount reaches zero after the final deletion"
    "positional sound attenuation clamps distance and zero-size viewports")
  string(FIND "${runtime_utils_media_model_test_contents}"
    "${required_utils_media_model_test_fragment}"
    required_utils_media_model_test_position)
  if(required_utils_media_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils media model tests lost '${required_utils_media_model_test_fragment}'")
  endif()
endforeach()
foreach(required_utils_media_platform_fragment IN ITEMS
    "cinematic gateways reject null, empty, and foreign handles and shut down repeatedly"
    "sound gateways reject exact-end IDs and null filenames before array access"
    "positional sound capacity and final-slot recount remain bounded and reusable"
    "JA2 sound facade shutdown clears delayed and positional state repeatedly"
    "stale music completion callbacks cannot retire replacement playback"
    "music shutdown invalidates deferred callbacks before releasing owned lists")
  string(FIND "${runtime_utils_text_platform_test_contents}"
    "${required_utils_media_platform_fragment}"
    required_utils_media_platform_position)
  if(required_utils_media_platform_position EQUAL -1)
    message(FATAL_ERROR
      "Utils media platform coverage lost '${required_utils_media_platform_fragment}'")
  endif()
endforeach()
foreach(utils_media_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${utils_media_test_manifest}}"
    "utils_media_lifecycle_model_tests" required_utils_media_test_position)
  if(required_utils_media_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils media lifecycle model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()

# Items.xml needs one executable publication contract before the legacy Expat
# callback can be migrated safely. Keep the model independent from INVTYPE and
# preserve its compatibility decisions in focused tests and the ASan matrix.
file(READ "${SOURCE_ROOT}/Utils/ItemDataStagingModel.h"
  runtime_item_data_staging_model_contents)
foreach(required_item_data_staging_model_fragment IN ITEMS
    "TryNarrow"
    "TryParseInteger"
    "TryParseBoolean"
    "TryParseFiniteFloat"
    "TryParseClampedInteger"
    "TryCopyUtf8"
    "class CharacterAccumulator"
    "StanceModifierFieldCount = 11"
    "ResolveStanceInheritance"
    "struct AuxiliaryTables"
    "struct IndexedItemRecord"
    "struct BasePublicationView"
    "class RequiredBaseLoadTransaction"
    "class OptionalLocalizedLoadTransaction"
    "StagingFailed"
    "RejectedStagingFailure"
    "stagedAuxiliary_ = liveAuxiliary"
    "itemSlots_"
    "stagedMaxItemsRead_ = std::max"
    "std::is_nothrow_invocable_v<Publisher&"
    "std::is_same_v<std::invoke_result_t<Publisher&"
    "fail(Failure::MissingRequiredResource)"
    "state_ = Detail::TransactionState::Validating"
    "state_ = Detail::TransactionState::MissingOptional")
  string(FIND "${runtime_item_data_staging_model_contents}"
    "${required_item_data_staging_model_fragment}"
    required_item_data_staging_model_position)
  if(required_item_data_staging_model_position EQUAL -1)
    message(FATAL_ERROR
      "Item data staging model lost '${required_item_data_staging_model_fragment}'")
  endif()
endforeach()
foreach(forbidden_item_data_staging_model_fragment IN ITEMS
    "struct ItemTables"
    "ItemTables<ItemRecord> candidate"
    "std::vector<ItemRecord> stagedItems_")
  string(FIND "${runtime_item_data_staging_model_contents}"
    "${forbidden_item_data_staging_model_fragment}"
    forbidden_item_data_staging_model_position)
  if(NOT forbidden_item_data_staging_model_position EQUAL -1)
    message(FATAL_ERROR
      "Item data staging model regained the retired multi-table staging aggregate '${forbidden_item_data_staging_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/item_data_staging_model_tests.cpp"
  runtime_item_data_staging_model_test_contents)
foreach(required_item_data_staging_model_test_fragment IN ITEMS
    "integer parsing rejects malformed and narrowing overflow"
    "unsigned masks retain exact values above double precision and reject overflow"
    "boolean parsing preserves signed nonzero semantics with full-token bounds"
    "clamped integers apply schema limits in signed wide storage before narrowing"
    "item floats are locale-independent finite full-token values in float range"
    "character accumulation reports truncation and preserves termination"
    "UTF-8 conversion accepts an exact-fit UTF-16 destination"
    "UTF-8 conversion rejects a one-unit overflow without partial output"
    "invalid UTF-8 is rejected without changing destination text"
    "stance inheritance matches stand crouch prone legacy rules"
    "base staging retains only authored item records"
    "base publication borrows sparse authored records without copying them"
    "localized staging remains patch-only during validation and publish"
    "staging contains record and localized allocation/copy failures before C callback unwind"
    "base loads publish atomically and retain unspecified auxiliary values"
    "sparse unsorted and duplicate base indices use deterministic high-water rules"
    "base failures and truncated documents leave every live table unchanged"
    "zero-capacity base tables reject records before access"
    "missing optional localized resources succeed without mutation"
    "localized overlays publish only fields actually present"
    "late localized validation rejection performs no live writes"
    "validation state changes abort before localized publication")
  string(FIND "${runtime_item_data_staging_model_test_contents}"
    "${required_item_data_staging_model_test_fragment}"
    required_item_data_staging_model_test_position)
  if(required_item_data_staging_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Item data staging model tests lost '${required_item_data_staging_model_test_fragment}'")
  endif()
endforeach()
foreach(item_data_staging_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${item_data_staging_test_manifest}}"
    "item_data_staging_model_tests" required_item_data_staging_test_position)
  if(required_item_data_staging_test_position EQUAL -1)
    message(FATAL_ERROR
      "Item data staging tests left the build or AddressSanitizer matrix")
  endif()
endforeach()

# Keep the production Items.xml adapter on the bounded transaction. Expat
# callbacks may build sparse records or localized patches, but live
# tables can change only at the checked no-fail publication boundary.
file(READ "${SOURCE_ROOT}/Utils/XML_Items.cpp"
  runtime_item_xml_adapter_contents)
foreach(required_item_xml_adapter_fragment IN ITEMS
    "BOOLEAN ReadInItemStats(STR fileName, BOOLEAN localizedVersion) noexcept"
    "RequiredBaseLoadTransaction<INVTYPE>"
    "OptionalLocalizedLoadTransaction<"
    "SnapshotItemAuxiliaryTables"
    "ValidateLocalizedItemText"
    "PublishLocalizedItemText"
    "static void PublishBaseItemTables("
    "std::memset(Item, 0, sizeof(Item))"
    "std::memcpy(&Item[stagedItem.index], &stagedItem.item"
    "baseLoad->commit(MAXITEMS, PublishBaseItemTables)"
    "localizedLoad->resourceMissing()"
    "baseLoad->resourceMissing()"
    "pData->curItem = INVTYPE{}"
    "pData->currentAuxiliary = {}"
    "ItemCharacterDataCapacity"
    "ParseItemBooleanValue"
    "ParseItemFloatValue"
    "ParseItemClampedIntegerValue"
    "TryAdjustItemApBonus"
    "ParseLegacyItemPrice"
    "ItemDataStagingModel::TryCopyUtf8"
    "no C++ exception can unwind through the C parser frames"
    "if (pData->failed) return"
    "strcmp(name, \"Cigarette\" ) == 0 ||"
    "const UINT16 attachment = ParseItemIntegerValue<UINT16>(pData)"
    "strcmp(name, \"RemoteDetonator\")"
    "ItemDataStagingModel::ResolveStanceInheritance(modifiers)"
    "std::is_trivially_copyable_v<INVTYPE>"
    "std::numeric_limits<UINT32>::max()")
  string(FIND "${runtime_item_xml_adapter_contents}"
    "${required_item_xml_adapter_fragment}"
    required_item_xml_adapter_position)
  if(required_item_xml_adapter_position EQUAL -1)
    message(FATAL_ERROR
      "Items XML adapter lost '${required_item_xml_adapter_fragment}'")
  endif()
endforeach()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(atol|atoi|atof|strtoul|MultiByteToWideChar)[ \t\r\n]*\\("
  retired_item_xml_conversion "${runtime_item_xml_adapter_contents}")
if(retired_item_xml_conversion)
  message(FATAL_ERROR
    "Items XML adapter restored a retired unchecked conversion '${retired_item_xml_conversion}'")
endif()
foreach(retired_item_xml_adapter_fragment IN ITEMS
    "BOOLEAN localizedTextOnly"
    "MAX_CHAR_DATA_LENGTH"
    "szCharData"
    "pData->curArray"
    "StoreInventory[pData->curItem.uiIndex]"
    "WeaponROF[pData->curItem.uiIndex]"
    "gMAXITEMS_READ = pData->curItem.uiIndex"
    "std::vector<INVTYPE>")
  string(FIND "${runtime_item_xml_adapter_contents}"
    "${retired_item_xml_adapter_fragment}"
    retired_item_xml_adapter_position)
  if(NOT retired_item_xml_adapter_position EQUAL -1)
    message(FATAL_ERROR
      "Items XML adapter restored retired live-write path '${retired_item_xml_adapter_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_item_xml_adapter_contents}"
  "const UINT32 maxItemsRead =" item_xml_high_water_validation_position)
string(FIND "${runtime_item_xml_adapter_contents}"
  "std::memset(Item, 0, sizeof(Item))"
  item_xml_first_publication_position)
if(item_xml_high_water_validation_position EQUAL -1 OR
   item_xml_first_publication_position EQUAL -1 OR
   NOT item_xml_high_water_validation_position LESS item_xml_first_publication_position)
  message(FATAL_ERROR
    "Items XML adapter must derive the validated high-water value before publication")
endif()

foreach(required_item_xml_headless_fragment IN ITEMS
    "struct ItemXmlItemSnapshot"
    "std::vector<ItemXmlItemSnapshot> items"
    "ItemXmlItemRecordIsZero"
    "class ScopedItemXmlTablesRestore"
    "transactional item base loads use the last nonzero-class item, merged auxiliary fields, and an unsorted high-water bound"
    "valid item XML exact-end records are ignored before live table access"
    "malformed item base XML rolls back Item StoreInventory WeaponROF and loaded bound"
    "item XML records without uiIndex poison the full base transaction"
    "item XML auxiliary integer overflow rolls back every live table"
    "malformed checked fields poison item XML even in out-of-range records"
    "localized item overlays publish only authored text and preserve auxiliary tables"
    "localized item XML ignores valid BR inventory and ROF tags for publication"
    "empty localized item XML is a successful no-op"
    "malformed localized item XML rolls back every staged text patch"
    "item XML required base and optional localization missing-file policies preserve live tables"
    "empty item base XML publishes a cleared Item table with preserved auxiliary values"
    "item XML parser preserves exact masks base-zero and signed booleans while clamping before narrowing"
    "item XML canonical Cigarette tag and full default-attachment capacity load"
    "item XML strict text scalar and late-field failures roll back every live table"
    "item XML validates a default attachment after storage capacity is full"
    "item XML AP bonus scaling rejects invalid bounds transactionally and preserves valid signed legacy rounding"
    "item XML parser state is invocation-local and floats ignore the process numeric locale"
    "localized item text overflow rolls back while an empty overlay is a no-op"
    "an explicitly requested installed Items.xml remains parser-compatible")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_item_xml_headless_fragment}"
    required_item_xml_headless_position)
  if(required_item_xml_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Items XML production coverage lost '${required_item_xml_headless_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_utils_ui_headless_contents}"
  "\ttry\n\t{\n\t\tRunTransactionalItemXmlTests(vfsPriorityToken);\n\t\tRunItemXmlWriterTests(vfsPriorityToken);\n\t\tRunAtomicVfsPublicationTests(vfsPriorityToken);"
  guarded_item_xml_test_invocation_position)
if(guarded_item_xml_test_invocation_position EQUAL -1)
  message(FATAL_ERROR
    "Items XML reader/writer/atomic integration tests lost their shared exception guard")
endif()

# Keep the item exporter on the shared XMLWriter/data-boundary path. The public
# seams must preserve the compatibility wrapper while making the requested
# count and exact file-transfer result independently testable.
file(READ "${SOURCE_ROOT}/Utils/ItemXmlWriter.h"
  runtime_item_xml_writer_header_contents)
foreach(required_item_xml_writer_header_fragment IN ITEMS
    "BoundedItemCount("
    "return std::min(requested, capacity)"
    "bool Write(const vfs::Path& path, std::size_t requestedCount);"
    "bool Write(vfs::tWritableFile* file, std::size_t requestedCount);")
  string(FIND "${runtime_item_xml_writer_header_contents}"
    "${required_item_xml_writer_header_fragment}"
    required_item_xml_writer_header_position)
  if(required_item_xml_writer_header_position EQUAL -1)
    message(FATAL_ERROR
      "Item XML writer seam lost '${required_item_xml_writer_header_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/XML.h" runtime_tactical_xml_header_contents)
string(FIND "${runtime_tactical_xml_header_contents}"
  "WriteItemStatsToFile(STR fileName, UINT32 itemCount)"
  required_item_xml_writer_public_seam_position)
if(required_item_xml_writer_public_seam_position EQUAL -1)
  message(FATAL_ERROR "Item XML writer lost its public path/count seam")
endif()

foreach(required_item_xml_writer_fragment IN ITEMS
    "#include \"ItemXmlWriter.h\""
    "#include \"XMLWriter.h\""
    "formatted.imbue(std::locale::classic())"
    "std::numeric_limits<double>::max_digits10"
    "static_cast<double>(value)"
    "codePoint == 0xfffe || codePoint == 0xffff"
    "ItemTextToUtf8("
    "AddSpreadPattern("
    "giSpreadPatternCount <= patternIndex"
    "const std::string serialized = std::to_string(patternIndex)"
    "ValidateItemRecord("
    "IsSemanticallyEmptyItem("
    "allowedItemFlags = UINT64_C(0xCFFFFFFFFFFFFFFF)"
    "allowedItemFlags2 = UINT64_C(0x000007FFFFFFFFFF)"
    "sawZeroAttachment"
    "TrySerializeApBonus("
    "TryAdjustItemApBonus("
    "roundTripped == liveValue"
    "AddItemInteger(writer, \"AvailableAttachmentPoint\""
    "AddItemInteger(writer, \"AttachmentPoint\""
    "AddItemInteger(writer, \"AttachToPointAPCost\""
    "AddItemInteger(writer, \"PercentAim\""
    "AddItemInteger(writer, \"PercentRangeBonus\", item.percentrangebonus)"
    "AddItemInteger(writer, \"AutoFireToHitBonus\""
    "AddItemFloat(writer, \"RobotDamageReduction\""
    "AddItemInteger(writer, \"TransportGroupMinProgress\""
    "AddItemInteger(writer, \"TransportGroupMaxProgress\""
    "AddItemBoolean(writer, \"TripWireActivation\""
    "AddItemBoolean(writer, \"Cigarette\""
    "ItemXmlWriter::BoundedItemCount("
    "BuildItemStatsXml(XMLWriter& writer, std::size_t requestedCount)"
    "writer.writeToFile(file)"
    "BOOLEAN WriteItemStatsToFile(STR fileName, UINT32 itemCount)"
    "std::min<std::size_t>("
    "vfs::Path(\"TABLEDATA\\\\Items out.xml\")")
  string(FIND "${runtime_item_xml_adapter_contents}"
    "${required_item_xml_writer_fragment}"
    required_item_xml_writer_position)
  if(required_item_xml_writer_position EQUAL -1)
    message(FATAL_ERROR
      "Item XML writer lost '${required_item_xml_writer_fragment}'")
  endif()
endforeach()
string(FIND "${runtime_item_xml_adapter_contents}"
  "if (!ValidateItemRecord(index)) return false;"
  item_xml_writer_preflight_position)
string(FIND "${runtime_item_xml_adapter_contents}"
  "writer.openNode(\"ITEMLIST\")" item_xml_writer_document_position)
if(item_xml_writer_preflight_position EQUAL -1 OR
   item_xml_writer_document_position EQUAL -1 OR
   NOT item_xml_writer_preflight_position LESS item_xml_writer_document_position)
  message(FATAL_ERROR
    "Item XML writer must validate every live record before building the document")
endif()
foreach(retired_item_xml_writer_fragment IN ITEMS
    "FilePrintf(hFile"
    "cnt < 351"
    "AddItemBoolean(writer, \"TripwireActivation\""
    "AddItemBoolean(writer, \"cigarette\""
    "AddItemFloat(writer, \"RobotDamageReductionModifier\""
    "AddItemInteger(writer, \"PercentRangeBonus\", item.rangebonus)"
    "AddItemInteger(writer, \"Detonator\""
    "AddItemInteger(writer, \"RemoteDetonator\""
    "AddItemInteger(writer, \"ItemFlag\""
    "AddItemInteger(writer, \"fFlags\"")
  string(FIND "${runtime_item_xml_adapter_contents}"
    "${retired_item_xml_writer_fragment}"
    retired_item_xml_writer_position)
  if(NOT retired_item_xml_writer_position EQUAL -1)
    message(FATAL_ERROR
      "Item XML writer restored retired output '${retired_item_xml_writer_fragment}'")
  endif()
endforeach()

foreach(required_item_xml_writer_headless_fragment IN ITEMS
    "const ScopedItemXmlTablesRestore restoreTables(originalTables)"
    "const ScopedItemXmlSpreadPatternsRestore restoreSpreadPatterns"
    "const ScopedItemXmlApConstantsRestore restoreApConstants"
    "const ScopedGlobalLocaleRestore restoreLocale"
    "item XML writer emits escaped UTF-8 and exact 79/399-character text"
    "item XML writer preserves every 64-bit mask above 2^53 as decimal"
    "item XML writer uses round-trip float precision under a comma locale"
    "item XML writer keeps both signed FLT_MAX endpoints inside the reader range"
    "item XML writer reverse-adjusts APBonus under a non-100 AP maximum"
    "item XML writer exports corrected attachment range robot transport and spread fields"
    "item XML writer exports PercentAim in every stance for every item"
    "item XML writer uses canonical reader tags beyond the legacy 351-item limit and omits no-op aliases"
    "item XML writer count seam clamps requested records to live capacity"
    "item XML writer propagates open short-write and null-file failures exactly"
    "item XML writer output reloads through the canonical reader schema"
    "item XML writer text round-trips reserved UTF-8 and exact-capacity fields"
    "item XML writer round-trips four UINT64 masks all known flags and Cigarette"
    "item XML writer modifiers and progress bounds round-trip canonically"
    "item XML writer flags auxiliaries and high indices round-trip canonically"
    "item XML stream seam opens fresh truncates trailing bytes and closes explicitly"
    "item XML stream seam contains throwing open and close while attempting cleanup"
    "item XML stream seam rejects pre-opened read and write files without mutation"
    "item XML writer rejects XML-invalid Unicode and non-finite floats before opening the target"
    "item XML writer rejects clamped scalar domains before opening the target"
    "item XML writer rejects attachment holes and every inherited stance sentinel"
    "item XML writer rejects unsafe AP reversal and unavailable spread indices"
    "item XML writer rejects reserved flags and lossy structural high-water states"
    "item XML writer accepts spread zero without a loaded spread table"
    "item XML writer accepts both clamped representable AP inverse endpoints"
    "item XML writer permits auxiliary publication through a semantic class-zero gap"
    "atomic VFS failed publish restores lower visibility without a writable ghost"
    "atomic VFS wholly absent rollback removes the empty virtual location and file entry"
    "atomic VFS publishes a mounted writable override and shorter replacement exactly"
    "atomic VFS failed existing-target publish preserves bytes and catalogue identity"
    "atomic VFS creates and registers a wholly absent file under mixed-case mounted lookup"
    "atomic VFS resolves caller case through the matched mounted physical directory"
    "atomic native replacement cleans its temp after a pre-commit publish failure")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_item_xml_writer_headless_fragment}"
    required_item_xml_writer_headless_position)
  if(required_item_xml_writer_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Item XML writer coverage lost '${required_item_xml_writer_headless_fragment}'")
  endif()
endforeach()

# Event Pump's queue contract is dependency-free and owns packet bytes. Keep
# the three queue semantics, strict legacy expiry rule, bounded failure surface,
# production schema adapter, and integration cutover executable under ASan.
file(READ "${SOURCE_ROOT}/Utils/TacticalEventQueueModel.h"
  tactical_event_queue_model_contents)
foreach(required_tactical_event_queue_model_fragment IN ITEMS
    "enum class EventKind"
    "struct EventSchema"
    "class OwnedEvent"
    "class EventQueues"
    "enum class EnqueueResult"
    "maximumQueuedPayloadBytes"
    "struct AllocationControl"
    "SequenceExhausted"
    "AllocationFailure"
    "drainPrimaryAndExpired"
    "drainDemand"
    "static_cast<Tick>(now - event.scheduledAt) > event.event.delay()"
    "void clear() noexcept")
  string(FIND "${tactical_event_queue_model_contents}"
    "${required_tactical_event_queue_model_fragment}"
    required_tactical_event_queue_model_position)
  if(required_tactical_event_queue_model_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical event queue model lost '${required_tactical_event_queue_model_fragment}'")
  endif()
endforeach()
foreach(retired_tactical_event_queue_model_fragment IN ITEMS
    "#include \""
    "MemAlloc"
    "MemFree"
    "GetJA2Clock"
    "PTR")
  string(FIND "${tactical_event_queue_model_contents}"
    "${retired_tactical_event_queue_model_fragment}"
    retired_tactical_event_queue_model_position)
  if(NOT retired_tactical_event_queue_model_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical event queue model gained legacy dependency '${retired_tactical_event_queue_model_fragment}'")
  endif()
endforeach()

foreach(tactical_event_queue_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${tactical_event_queue_test_manifest}}"
    "tactical_event_queue_model_tests"
    required_tactical_event_queue_test_position)
  if(required_tactical_event_queue_test_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical event queue model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/tests/tactical_event_queue_model_tests.cpp"
  tactical_event_queue_model_test_contents)
foreach(required_tactical_event_queue_test_fragment IN ITEMS
    "event kinds retain legacy dispatch values and reject category gaps"
    "queued events own a copy independent from the caller"
    "primary queue is FIFO and drains same-cycle appended work"
    "delayed readiness uses the legacy strict-greater-than boundary"
    "expired delayed events retain deterministic insertion order"
    "delayed expiry preserves unsigned clock-wrap behavior"
    "demand queue is FIFO and drains independently from primary work"
    "schema validation rejects null, mismatched, and sentinel events"
    "aggregate owned-byte capacity covers every queue"
    "injected enqueue allocation failure publishes no ownership"
    "injected delayed allocation failure preserves primary ownership"
    "allocation failure does not consume the next successful sequence"
    "full delayed storage preserves the unpromoted primary event"
    "discard mode drops primary and expired delayed work without execution"
    "one idempotent clear releases primary, delayed, and demand ownership")
  string(FIND "${tactical_event_queue_model_test_contents}"
    "${required_tactical_event_queue_test_fragment}"
    required_tactical_event_queue_test_position)
  if(required_tactical_event_queue_test_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical event queue model coverage lost '${required_tactical_event_queue_test_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Event Pump.cpp"
  tactical_event_pump_source_contents)
file(READ "${SOURCE_ROOT}/Utils/Event Pump.h"
  tactical_event_pump_header_contents)
foreach(required_tactical_event_pump_source_fragment IN ITEMS
    "#include \"TacticalEventQueueModel.h\""
    "constexpr QueueLimits EventQueueLimits"
    "static_assert(S_WINDOWHIT == static_cast<UINT32>(EventKind::WindowHit))"
    "EventSchema::For<EV_S_WINDOWHIT>(EventKind::WindowHit)"
    "pEventData == nullptr"
    "gEventQueues.enqueuePrimary("
    "gEventQueues.enqueueDemand("
    "gEventQueues.drainPrimaryAndExpired("
    "gEventQueues.drainDemand("
    "*expected != event.schema()"
    "event.decode<Payload>("
    "gEventQueues.clear()"
    "GetEventQueueStatistics()")
  string(FIND "${tactical_event_pump_source_contents}"
    "${required_tactical_event_pump_source_fragment}"
    required_tactical_event_pump_source_position)
  if(required_tactical_event_pump_source_position EQUAL -1)
    message(FATAL_ERROR
      "Owned production Event Pump lost '${required_tactical_event_pump_source_fragment}'")
  endif()
endforeach()
foreach(retired_tactical_event_pump_source_fragment IN ITEMS
    "struct EVENT"
    "MemAlloc("
    "MemFree("
    "memcpy("
    "std::queue<"
    "hEventQueue"
    "hDelayEventQueue"
    "hDemandEventQueue"
    "EVENT_EXPIRED"
    "SChangeState;"
    "SWindowHit;"
    "SNoise;")
  string(FIND "${tactical_event_pump_source_contents}"
    "${retired_tactical_event_pump_source_fragment}"
    retired_tactical_event_pump_source_position)
  if(NOT retired_tactical_event_pump_source_position EQUAL -1)
    message(FATAL_ERROR
      "Production Event Pump restored raw/manual/global state '${retired_tactical_event_pump_source_fragment}'")
  endif()
endforeach()
foreach(required_tactical_event_pump_header_fragment IN ITEMS
    "struct EventQueueStatistics"
    "UINT32 primary = 0;"
    "UINT32 delayed = 0;"
    "UINT32 demand = 0;"
    "EventQueueStatistics GetEventQueueStatistics();")
  string(FIND "${tactical_event_pump_header_contents}"
    "${required_tactical_event_pump_header_fragment}"
    required_tactical_event_pump_header_position)
  if(required_tactical_event_pump_header_position EQUAL -1)
    message(FATAL_ERROR
      "Production Event Pump diagnostics lost '${required_tactical_event_pump_header_fragment}'")
  endif()
endforeach()
foreach(required_tactical_event_pump_headless_fragment IN ITEMS
    "owned Event Pump binds WindowHit to its exact payload size"
    "owned Event Pump rejects malformed packets and clears primary delayed and demand queues")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_tactical_event_pump_headless_fragment}"
    required_tactical_event_pump_headless_position)
  if(required_tactical_event_pump_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Production Event Pump headless coverage lost '${required_tactical_event_pump_headless_fragment}'")
  endif()
endforeach()

# Offline image/developer utilities must validate every legacy dimension before
# access, own conversion state, stage the complete on-disk representation, and
# publish through the atomic VFS boundary. Keep both pure rules and real
# adapters in normal/ASan coverage, and reject the former delete/write sinks.
file(READ "${SOURCE_ROOT}/Utils/ImageUtilityModel.h"
  runtime_utils_image_model_contents)
foreach(required_utils_image_model_fragment IN ITEMS
    "CheckedImageByteCount"
    "CheckedEtrleCapacity"
    "ContainsRectangle"
    "CanAppendSerializedBytes"
    "NearestPaletteIndex"
    "(std::numeric_limits<std::uint32_t>::max)()")
  string(FIND "${runtime_utils_image_model_contents}"
    "${required_utils_image_model_fragment}"
    required_utils_image_model_position)
  if(required_utils_image_model_position EQUAL -1)
    message(FATAL_ERROR
      "Utils image boundary lost '${required_utils_image_model_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/STIConvert.cpp"
  runtime_utils_sti_convert_contents)
foreach(required_utils_sti_fragment IN ITEMS
    "BOOLEAN TryWriteSTIFile("
    "MemOwner<UINT8> outputOwner"
    "MemOwner<UINT8> subImageOwner"
    "CheckedEtrleCapacity("
    "CanAppendSerializedBytes("
    "ContainsRectangle("
    "AppendIndexedHeader("
    "AppendSubImages("
    "AppendLittleEndian32("
    "offsetof(STCIHeader, uiAppDataSize) == 48"
    "replaceFileAtomically("
    "*ppDest = NULL"
    "*ppSubImageBuffer = NULL")
  string(FIND "${runtime_utils_sti_convert_contents}"
    "${required_utils_sti_fragment}" required_utils_sti_position)
  if(required_utils_sti_position EQUAL -1)
    message(FATAL_ERROR
      "Utils STI conversion boundary lost '${required_utils_sti_fragment}'")
  endif()
endforeach()
foreach(retired_utils_sti_fragment IN ITEMS
    "FileDelete(cOutputName)"
    "FileWrite(hFile"
    "FileOpen(cOutputName"
    "SplitUINT32"
    "sWidth * sHeight")
  string(FIND "${runtime_utils_sti_convert_contents}"
    "${retired_utils_sti_fragment}" retired_utils_sti_position)
  if(NOT retired_utils_sti_position EQUAL -1)
    message(FATAL_ERROR
      "Utils STI conversion restored unsafe '${retired_utils_sti_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Quantize.cpp"
  runtime_utils_quantizer_contents)
file(READ "${SOURCE_ROOT}/Utils/Quantize Wrap.cpp"
  runtime_utils_quantizer_wrap_contents)
set(runtime_utils_quantizer_combined
  "${runtime_utils_quantizer_contents}\n${runtime_utils_quantizer_wrap_contents}")
foreach(required_utils_quantizer_fragment IN ITEMS
    "new (std::nothrow) NODE{}"
    "std::uint64_t"
    "CheckedImageByteCount("
    "TryMapPalette("
    "stagedPixels"
    "stagedPalette"
    "NearestPaletteIndex(")
  string(FIND "${runtime_utils_quantizer_combined}"
    "${required_utils_quantizer_fragment}" required_utils_quantizer_position)
  if(required_utils_quantizer_position EQUAL -1)
    message(FATAL_ERROR
      "Utils quantizer boundary lost '${required_utils_quantizer_fragment}'")
  endif()
endforeach()
foreach(retired_utils_quantizer_fragment IN ITEMS
    "HeapAlloc ("
    "HeapFree ("
    "(RGBQUAD*)pPalette"
    "VGetLength")
  string(FIND "${runtime_utils_quantizer_combined}"
    "${retired_utils_quantizer_fragment}" retired_utils_quantizer_position)
  if(NOT retired_utils_quantizer_position EQUAL -1)
    message(FATAL_ERROR
      "Utils quantizer restored unsafe '${retired_utils_quantizer_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/MapUtility.cpp"
  runtime_utils_map_utility_contents)
foreach(required_utils_map_utility_fragment IN ITEMS
    "std::vector<UINT8> g24BitValues"
    "std::array<UINT8, MINIMAP_X_SIZE * MINIMAP_Y_SIZE> quantizedPixels"
    "static void ReleaseMapFileList()"
    "FileList = NULL"
    "FListNode = NULL"
    "static_cast<std::size_t>(iY) * MINIMAP_X_SIZE"
    "if (!pDestBuf)"
    "if (!pSrcBuf)"
    "uiDestPitchBYTES < MINIMAP_X_SIZE * sizeof(PIXEL)"
    "if (!QuantizeImage("
    "TryWriteSTIFile("
    "std::snprintf(")
  string(FIND "${runtime_utils_map_utility_contents}"
    "${required_utils_map_utility_fragment}"
    required_utils_map_utility_position)
  if(required_utils_map_utility_position EQUAL -1)
    message(FATAL_ERROR
      "Utils radar-map boundary lost '${required_utils_map_utility_fragment}'")
  endif()
endforeach()
foreach(retired_utils_map_utility_fragment IN ITEMS
    "p24BitValues"
    "RGBValues"
    "LockVideoSurface(gui8BitMiniMap"
    "sprintf(zFilename"
    "WriteSTIFile((INT8*)")
  string(FIND "${runtime_utils_map_utility_contents}"
    "${retired_utils_map_utility_fragment}" retired_utils_map_utility_position)
  if(NOT retired_utils_map_utility_position EQUAL -1)
    message(FATAL_ERROR
      "Utils radar-map conversion restored unsafe '${retired_utils_map_utility_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Utils/Debug Control.cpp"
  runtime_utils_debug_control_contents)
foreach(required_utils_debug_control_fragment IN ITEMS
    "static const sgp::Logger_ID id = []"
    "static vfs::Log* const mpMsg"
    "catch (...)")
  string(FIND "${runtime_utils_debug_control_contents}"
    "${required_utils_debug_control_fragment}"
    required_utils_debug_control_position)
  if(required_utils_debug_control_position EQUAL -1)
    message(FATAL_ERROR
      "Utils diagnostic boundary lost '${required_utils_debug_control_fragment}'")
  endif()
endforeach()
foreach(retired_utils_debug_control_fragment IN ITEMS
    "static struct LiveLog"
    "static vfs::Log& mpMsg")
  string(FIND "${runtime_utils_debug_control_contents}"
    "${retired_utils_debug_control_fragment}"
    retired_utils_debug_control_position)
  if(NOT retired_utils_debug_control_position EQUAL -1)
    message(FATAL_ERROR
      "Utils diagnostic boundary restored eager or unchecked logger state")
  endif()
endforeach()
string(REGEX MATCHALL "if \\(!strMessage\\) return"
  utils_debug_null_guards "${runtime_utils_debug_control_contents}")
list(LENGTH utils_debug_null_guards utils_debug_null_guard_count)
if(NOT utils_debug_null_guard_count EQUAL 5)
  message(FATAL_ERROR
    "Every Utils debug/log message boundary must reject null text")
endif()

file(READ "${SOURCE_ROOT}/tests/utils_image_utility_model_tests.cpp"
  runtime_utils_image_model_test_contents)
foreach(required_utils_image_model_test_fragment IN ITEMS
    "checked products reject host-size overflow without publishing a partial result"
    "image dimensions reject empty, negative, and overflowing byte counts without publishing a partial result"
    "ETRLE scratch capacity remains representable in the serialized 32-bit format"
    "subimages reject negative, empty, exact-end, and overflowing geometry"
    "serialized STI staging rejects 32-bit size overflow"
    "palette mapping uses bounded integer distance with deterministic first-match ties")
  string(FIND "${runtime_utils_image_model_test_contents}"
    "${required_utils_image_model_test_fragment}"
    required_utils_image_model_test_position)
  if(required_utils_image_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils image model tests lost '${required_utils_image_model_test_fragment}'")
  endif()
endforeach()
foreach(required_utils_image_headless_fragment IN ITEMS
    "optional diagnostic sinks reject null caller text without initialization or dispatch"
    "invalid ETRLE subimage geometry is rejected before touching destination bytes"
    "malformed STI conversion preserves the complete existing destination"
	"STI writer atomically publishes the exact legacy header palette subimage payload and zeroed app data"
	"uncompressed STI output writes caller pixels instead of an uninitialized legacy image pointer"
	"quantizer owns complete palette and pixel staging while preserving legacy RGB results"
    "invalid quantizer dimensions leave caller pixels and palette untouched")
  string(FIND "${runtime_utils_ui_headless_contents}"
    "${required_utils_image_headless_fragment}"
    required_utils_image_headless_position)
  if(required_utils_image_headless_position EQUAL -1)
    message(FATAL_ERROR
      "Utils image adapter coverage lost '${required_utils_image_headless_fragment}'")
  endif()
endforeach()
foreach(utils_image_test_manifest IN ITEMS
    runtime_utils_ui_test_build_contents runtime_utils_ui_ci_contents)
  string(FIND "${${utils_image_test_manifest}}"
    "utils_image_utility_model_tests" required_utils_image_test_position)
  if(required_utils_image_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils image model tests left the build or AddressSanitizer matrix")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/docs/UTILS_CODE_WALKTHROUGH.md"
  runtime_utils_walkthrough_contents)
foreach(required_utils_walkthrough_fragment IN ITEMS
    "Interactive UI ownership batch"
    "Text and localization safety batch"
    "Media lifecycle batch"
    "Encrypted text-record boundary"
    "Data persistence foundation"
    "Indexed localization XML boundary"
    "Item XML transaction and writer closure"
    "Owned tactical event queue and production cutover"
    "TacticalEventQueueModel.h"
    "now - scheduledAt > delay"
    "tactical_event_queue_model_tests"
    "Offline image and developer-utility closure"
    "Legacy compatibility utilities closure"
    "LegacyUtilitiesModel.h"
    "production reader through"
    "authored nonzero-class item"
    "invalid UTF-8"
    "Utils audit completion"
    "No unaudited Utils inventory remains"
    "XMLWriter"
    "installed-data canonical `Cigarette`"
    "same-directory sibling"
    "caller-owned writable-file overload"
    "power-loss durability"
    "TextInfrastructureModel.h"
    "IndexedXmlModel.h"
    "MediaLifecycleModel.h"
    "DataBoundaryModel.h"
    "ItemDataStagingModel.h")
  string(FIND "${runtime_utils_walkthrough_contents}"
    "${required_utils_walkthrough_fragment}"
    required_utils_walkthrough_position)
  if(required_utils_walkthrough_position EQUAL -1)
    message(FATAL_ERROR
      "Utils walkthrough lost '${required_utils_walkthrough_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/docs/ENGINE_ARCHITECTURE.md"
  runtime_engine_architecture_contents)
foreach(required_utils_architecture_fragment IN ITEMS
    "post-Laptop Utils refactor"
    "UtilsUiStateModel"
    "TextInfrastructureModel"
    "IndexedXmlModel"
    "ItemDataStagingModel"
    "TacticalEventQueueModel"
    "strict unsigned `now - scheduledAt > delay`"
    "`Event Pump.cpp` now binds every legacy value"
    "`S_WINDOWHIT` uses its actual payload size"
    "sparse authored nonzero-class records"
    "invocation-local character buffer"
    "MediaLifecycleModel"
    "DataBoundaryModel"
    "encrypted text-record"
    "ImageUtilityModel"
    "LegacyUtilitiesModel"
    "CursorStateModel"
    "no unaudited Utils inventory remains"
    "all four 64-bit masks exact"
    "installed-data"
    "exact representable inverse"
    "same-directory sibling"
    "caller-owned writable-file overload remains"
    "power-loss durability")
  string(FIND "${runtime_engine_architecture_contents}"
    "${required_utils_architecture_fragment}"
    required_utils_architecture_position)
  if(required_utils_architecture_position EQUAL -1)
    message(FATAL_ERROR
      "Engine architecture roadmap lost '${required_utils_architecture_fragment}'")
  endif()
endforeach()
foreach(required_aim_member_architecture_fragment IN ITEMS
    "LaptopLayoutModel"
    "AimMemberProfileLayoutModel"
    "AimWebsiteLayoutModel"
    "MercFilesLayoutModel"
    "FloristSiteModel")
  string(FIND "${runtime_engine_architecture_contents}"
    "${required_aim_member_architecture_fragment}"
    required_aim_member_architecture_position)
  if(required_aim_member_architecture_position EQUAL -1)
    message(FATAL_ERROR
      "Engine architecture roadmap lost A.I.M. layout contract '${required_aim_member_architecture_fragment}'")
  endif()
endforeach()

# Save/load and process-lifetime campaign bootstrap now use one compiled
# representation. Every host emits the JA25 persistence sections and the
# runtime campaign decides which restored state and startup hooks take effect.
# The support implementations below must likewise remain linkable from an
# ordinary JA2 host; the three-host build matrix proves that linkage while
# these named fragments prevent accidental replacement with stubs.
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  runtime_campaign_save_contents)
foreach(required_runtime_save_fragment IN ITEMS
    "SaveJa25SaveInfoToSaveGame"
    "SaveJa25TacticalInfoToSaveGame"
    "LoadJa25SaveInfoFromSavedGame"
    "LoadJa25TacticalInfoFromSavedGame"
    "gGameUBOptions.LaptopQuestEnabled = sGeneralInfo.sLaptopQuestEnabled"
    "gGameUBOptions.fTexAndJohn = sGeneralInfo.sTEX_AND_JOHN"
    "gGameUBOptions.fRandomManuelText = sGeneralInfo.sRandom_Manuel_Text"
    "gGameUBOptions.EventAttackInitialSectorIfPlayerStillThere ="
    "gGameUBOptions.HandleAddingEnemiesToTunnelMaps ="
    "JA2_EMAIL_MERC_INTRO")
  string(FIND "${runtime_campaign_save_contents}"
    "${required_runtime_save_fragment}" runtime_save_fragment_position)
  if(runtime_save_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign persistence lost its common runtime-selected schema; missing '${required_runtime_save_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Game Init.cpp"
  runtime_campaign_bootstrap_contents)
foreach(required_runtime_bootstrap_fragment IN ITEMS
    "bool IsUnfinishedBusinessCampaign()"
    "InitCustomStrategicLayer"
    "InitJa25StrategicAi"
    "InitTownLoyalty"
    "InitializeHeliGridnoAndTime"
    "InitJerryMiloInfo")
  string(FIND "${runtime_campaign_bootstrap_contents}"
    "${required_runtime_bootstrap_fragment}" runtime_bootstrap_fragment_position)
  if(runtime_bootstrap_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic startup bypassed runtime campaign selection; missing '${required_runtime_bootstrap_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Hourly Update.cpp"
  runtime_campaign_hourly_rules_contents)
foreach(required_runtime_hourly_rule_fragment IN ITEMS
    "HourlyCheckIfSlayAloneSoHeCanLeave"
    "HourlyHelicopterRepair"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_hourly_rules_contents}"
    "${required_runtime_hourly_rule_fragment}" runtime_hourly_rule_fragment_position)
  if(runtime_hourly_rule_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Hourly campaign rules lost runtime selection; missing '${required_runtime_hourly_rule_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Player Command.cpp"
  runtime_campaign_sector_control_contents)
foreach(required_runtime_sector_control_fragment IN ITEMS
    "JA2_EMAIL_BOBBYR_NOW_OPEN"
    "StrategicHandleQueenLosingControlOfSector"
    "HandlePOWQuestState"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_sector_control_contents}"
    "${required_runtime_sector_control_fragment}" runtime_sector_control_fragment_position)
  if(runtime_sector_control_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic sector-control rules lost runtime selection; missing '${required_runtime_sector_control_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  runtime_campaign_group_movement_contents)
foreach(required_runtime_group_movement_fragment IN ITEMS
    "const bool isArulcoMeanwhile"
    "StrategicAILookForAdjacentGroups"
    "TestForBloodcatAmbush"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_group_movement_contents}"
    "${required_runtime_group_movement_fragment}" runtime_group_movement_fragment_position)
  if(runtime_group_movement_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic group movement lost runtime campaign selection; missing '${required_runtime_group_movement_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  runtime_campaign_queen_rules_contents)
foreach(required_runtime_queen_rule_fragment IN ITEMS
    "pSoldier->identity().profile() == MORRIS_UB"
    "HandleBloodCatDeaths"
    "CalculateMaximumPrisonerAmount"
    "useUnfinishedBusinessGridNo"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_queen_rules_contents}"
    "${required_runtime_queen_rule_fragment}" runtime_queen_rule_fragment_position)
  if(runtime_queen_rule_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Queen-command rules lost runtime campaign selection; missing '${required_runtime_queen_rule_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Quests.cpp"
  runtime_campaign_quest_rules_contents)
foreach(required_runtime_quest_rule_fragment IN ITEMS
    "CampaignProfileCode::Role::Slay"
    "QUEST_DESTROY_MISSLES"
    "JA25_EMAIL_PILOT_MISSING"
    "HandlePOWQuestState"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_quest_rules_contents}"
    "${required_runtime_quest_rule_fragment}" runtime_quest_rule_fragment_position)
  if(runtime_quest_rule_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Quest/fact rules lost runtime campaign selection; missing '${required_runtime_quest_rule_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/laptop.cpp"
  runtime_campaign_laptop_recovery_contents)
foreach(required_runtime_laptop_recovery_fragment IN ITEMS
    "IsLaptopInsuranceEnabled"
    "AreMercAccountPagesEnabled"
    "IsCampaignWebAvailable"
    "InitJa25SaveStruct"
    "JA2_EMAIL_IMP_AGAIN"
    "JA25_EMAIL_IMP_AGAIN"
    "ShouldImpReminderEmailBeSentWhenLaptopBackOnline"
    "GetGameContext().capabilities().isUnfinishedBusiness()")
  string(FIND "${runtime_campaign_laptop_recovery_contents}"
    "${required_runtime_laptop_recovery_fragment}" runtime_laptop_recovery_fragment_position)
  if(runtime_laptop_recovery_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop quest recovery lost runtime campaign selection; missing '${required_runtime_laptop_recovery_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement Costs.cpp"
  runtime_campaign_movement_cost_contents)
foreach(required_runtime_movement_cost_fragment IN ITEMS
    "const bool useBuiltInUbMovementCosts"
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "void AddCustomMap("
    "void MakeBadSectorListFromMapsOnHardDrive("
    "void UpdateCustomMapMovementCosts()")
  string(FIND "${runtime_campaign_movement_cost_contents}"
    "${required_runtime_movement_cost_fragment}" runtime_movement_cost_fragment_position)
  if(runtime_movement_cost_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign movement-cost support is no longer available through runtime selection; missing '${required_runtime_movement_cost_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/LuaInitNPCs.cpp"
  runtime_campaign_lua_bootstrap_contents)
foreach(required_runtime_lua_bootstrap_fragment IN ITEMS
    "BOOLEAN LetLuaMakeBadSectorListFromMapsOnHardDrive("
    "BOOLEAN LuaInitStrategicLayer(")
  string(FIND "${runtime_campaign_lua_bootstrap_contents}"
    "${required_runtime_lua_bootstrap_fragment}" runtime_lua_bootstrap_fragment_position)
  if(runtime_lua_bootstrap_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign Lua bootstrap implementation is unavailable to common hosts; missing '${required_runtime_lua_bootstrap_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  runtime_campaign_map_validation_contents)
string(FIND "${runtime_campaign_map_validation_contents}"
  "void SetUpValidCampaignSectors( void )"
  runtime_campaign_map_validation_position)
if(runtime_campaign_map_validation_position EQUAL -1)
  message(FATAL_ERROR
    "Runtime campaign map validation lost its common implementation")
endif()

# Sector state and entry/exit behavior are now one runtime-selected campaign
# seam. The shared fields intentionally use one pre-release save layout; a
# build-target guard must not quietly remove their representation or behavior.
file(READ "${SOURCE_ROOT}/Strategic/Campaign Types.h"
  runtime_campaign_sector_type_contents)
foreach(required_runtime_sector_type_fragment IN ITEMS
    "SF_HAVE_SAID_PLAYER_QUOTE_NEW_SECTOR"
    "FINAL_COMPLEX,"
    "fValidSector;"
    "fCustomSector;"
    "fCampaignSector;")
  string(FIND "${runtime_campaign_sector_type_contents}"
    "${required_runtime_sector_type_fragment}" runtime_sector_type_fragment_position)
  if(runtime_sector_type_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic sector state lost common campaign metadata; missing '${required_runtime_sector_type_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  runtime_campaign_sector_serializer_contents)
foreach(required_runtime_sector_serializer_fragment IN ITEMS
    "ar.boolean(s.fCustomSector)"
    "ar.boolean(s.fCampaignSector)")
  string(FIND "${runtime_campaign_sector_serializer_contents}"
    "${required_runtime_sector_serializer_fragment}" runtime_sector_serializer_fragment_position)
  if(runtime_sector_serializer_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Underground sector persistence lost common campaign metadata; missing '${required_runtime_sector_serializer_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/email.h"
  runtime_campaign_sector_email_contents)
foreach(required_runtime_sector_email_fragment IN ITEMS
    "JA2_EMAIL_BOBBYR_NOW_OPEN = 182"
    "JA2_EMAIL_BOBBYR_NOW_OPEN_LENGTH = 3"
    "JA25_EMAIL_MIGUEL_SORRY = 25"
    "JA25_EMAIL_MIGUEL_MANUEL = 28"
    "JA25_EMAIL_MIGUEL_SICK = 32"
    "JA25_EMAIL_PILOT_FOUND = 42"
    "JA25_EMAIL_PILOT_MISSING = 8"
    "JA25_EMAIL_AIM_PROMOTION_1 = 184"
    "JA25_EMAIL_AIM_REPLY_BARRY = 98"
    "JA2_EMAIL_AIM_REPLY_BARRY = 58"
    "JA25_MAIL_MIGUEL = 51")
  string(FIND "${runtime_campaign_sector_email_contents}"
    "${required_runtime_sector_email_fragment}" runtime_sector_email_fragment_position)
  if(runtime_sector_email_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "JA25 sector email identity lost its stable campaign-qualified value; missing '${required_runtime_sector_email_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  runtime_campaign_strategic_sector_contents)
foreach(required_runtime_strategic_sector_fragment IN ITEMS
    "bool IsUnfinishedBusinessCampaign()"
    "HandlePlayerTeamQuotesWhenEnteringSector("
    "HandleEmailBeingSentWhenEnteringSector("
    "HandleSectorSpecificModificatioToMap("
    "HandleMovingTheEnemiesToBeNearPlayerWhenEnteringComplexMap("
    "JA25_EMAIL_MIGUEL_SORRY"
    "while ( ubIndex < sizeof(sGridNos) / sizeof(sGridNos[0])")
  string(FIND "${runtime_campaign_strategic_sector_contents}"
    "${required_runtime_strategic_sector_fragment}" runtime_strategic_sector_fragment_position)
  if(runtime_strategic_sector_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic sector behavior bypassed runtime campaign selection; missing '${required_runtime_strategic_sector_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Merc Hiring.cpp"
  runtime_campaign_arrival_contents)
foreach(required_runtime_arrival_fragment IN ITEMS
    "void UpdateJerryMiloInInitialSector()"
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "if ( pSoldier == NULL )")
  string(FIND "${runtime_campaign_arrival_contents}"
    "${required_runtime_arrival_fragment}" runtime_arrival_fragment_position)
  if(runtime_arrival_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "UB arrival behavior lost its runtime-selected safe entry point; missing '${required_runtime_arrival_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Tactical Turns.cpp"
  runtime_campaign_rpc_description_contents)
foreach(required_runtime_rpc_description_fragment IN ITEMS
    "void HandleRPCDescription("
    "CampaignProfileCode::Role::Ira"
    "CampaignProfileCode::Role::Miguel"
    "CampaignProfileCode::Role::Carlos"
    "CampaignProfileCode::Role::Dimitri")
  string(FIND "${runtime_campaign_rpc_description_contents}"
    "${required_runtime_rpc_description_fragment}" runtime_rpc_description_fragment_position)
  if(runtime_rpc_description_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "RPC sector descriptions lost runtime campaign profile resolution; missing '${required_runtime_rpc_description_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CampaignActionCodes.h"
  runtime_campaign_action_code_contents)
foreach(required_runtime_action_fragment IN ITEMS
    "decodeDialogueAction("
    "GameCampaign::Arulco, 301"
    "GameCampaign::UnfinishedBusiness, 301"
    "normalizeStrategicAction("
    "GameCampaign::UnfinishedBusiness, 311")
  string(FIND "${runtime_campaign_action_code_contents}"
    "${required_runtime_action_fragment}" runtime_action_fragment_position)
  if(runtime_action_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign action-code decoding lost runtime compatibility; missing '${required_runtime_action_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CampaignProfileCodes.h"
  runtime_campaign_profile_code_contents)
foreach(required_runtime_profile_fragment IN ITEMS
    "constexpr std::uint8_t profile("
    "GameCampaign campaign, Role role"
    "matches("
    "GameCampaign::Arulco, Role::Miguel"
    "GameCampaign::UnfinishedBusiness, Role::Miguel"
    "GameCampaign::Arulco, Role::Robot"
    "GameCampaign::UnfinishedBusiness, Role::Robot"
    "GameCampaign::Arulco, Role::Slay"
    "GameCampaign::UnfinishedBusiness, Role::Slay")
  string(FIND "${runtime_campaign_profile_code_contents}"
    "${required_runtime_profile_fragment}" runtime_profile_fragment_position)
  if(runtime_profile_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign profile-code decoding lost runtime compatibility; missing '${required_runtime_profile_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CampaignMapChangeCodes.h"
  runtime_campaign_map_change_code_contents)
foreach(required_runtime_map_change_code_fragment IN ITEMS
    "constexpr Type decode("
    "constexpr std::uint8_t encode("
    "ArulcoMinePresent = 22"
    "ArulcoRemoveMinePresent = 23"
    "ArulcoDecal = 24"
    "UnfinishedBusinessRemoveExitGrid = 22"
    "UnfinishedBusinessMinePresent = 23"
    "UnfinishedBusinessRemoveMinePresent = 24"
    "UnfinishedBusinessDecal = 25"
    "decode(GameCampaign::Arulco, 22)"
    "decode(GameCampaign::UnfinishedBusiness, 22)")
  string(FIND "${runtime_campaign_map_change_code_contents}"
    "${required_runtime_map_change_code_fragment}" runtime_map_change_code_fragment_position)
  if(runtime_map_change_code_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign map-change decoding lost legacy compatibility; missing '${required_runtime_map_change_code_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TileEngine/SaveLoadMap.cpp"
  runtime_campaign_map_change_io_contents)
foreach(required_runtime_map_change_io_fragment IN ITEMS
    "CampaignMapChangeCode::decode("
    "CampaignMapChangeCode::encode("
    "GetGameContext().capabilities().campaign"
    "MapChangeType::RemoveExitGrid"
    "MapChangeType::MinePresent"
    "AddRemoveExitGridToUnloadedMapTempFile"
    "uiFileSize % sizeof( MODIFY_MAP )"
    "uiNumberOfElementsSavedBackToFile")
  string(FIND "${runtime_campaign_map_change_io_contents}"
    "${required_runtime_map_change_io_fragment}" runtime_map_change_io_fragment_position)
  if(runtime_map_change_io_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Map-temp I/O lost runtime campaign decoding; missing '${required_runtime_map_change_io_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TileEngine/Explosion Control.cpp"
  runtime_campaign_explosion_contents)
foreach(required_runtime_explosion_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "ReplaceMineEntranceGraphicWithCollapsedEntrance"
    "HandleDestructionOfPowerGenFan"
    "HandleExplosionsInTunnelSector"
    "ACTION_ITEM_BIGGENS_BOMBS"
    "ACTION_ITEM_SEE_FORTIFIED_DOOR"
    "ACTION_ITEM_SEE_POWER_GEN_FAN")
  string(FIND "${runtime_campaign_explosion_contents}"
    "${required_runtime_explosion_fragment}" runtime_explosion_fragment_position)
  if(runtime_explosion_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical explosion hooks lost runtime campaign selection; missing '${required_runtime_explosion_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Action Items.h"
  runtime_campaign_action_item_contents)
foreach(required_runtime_action_item_fragment IN ITEMS
    "ACTION_ITEM_BIGGENS_BOMBS"
    "ACTION_ITEM_SEE_POWER_GEN_FAN"
    "static_assert(ACTION_ITEM_BIGGENS_BOMBS == 25)"
    "static_assert(ACTION_ITEM_NEW == 244)")
  string(FIND "${runtime_campaign_action_item_contents}"
    "${required_runtime_action_item_fragment}" runtime_action_item_fragment_position)
  if(runtime_action_item_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared action-item vocabulary lost numeric compatibility; missing '${required_runtime_action_item_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TacticalAI/NPC.cpp"
  runtime_campaign_npc_quote_contents)
foreach(required_runtime_npc_quote_fragment IN ITEMS
    "BOOLEAN HasNpcSaidQuoteBefore("
    "ubNPC >= NUM_PROFILES"
    "ubRecord >= NUM_NPC_QUOTE_RECORDS"
    "gpNPCQuoteInfoArray[ ubNPC ] == NULL")
  string(FIND "${runtime_campaign_npc_quote_contents}"
    "${required_runtime_npc_quote_fragment}" runtime_npc_quote_fragment_position)
  if(runtime_npc_quote_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared NPC quote lookup lost bounds validation; missing '${required_runtime_npc_quote_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  runtime_campaign_dialogue_contents)
foreach(required_runtime_dialogue_fragment IN ITEMS
    "TryHandleCampaignDialogueAction("
    "GetGameContext().capabilities().campaign"
    "DialogueAction::JerryConversation1"
    "DialogueAction::WaldoRepairRequestor")
  string(FIND "${runtime_campaign_dialogue_contents}"
    "${required_runtime_dialogue_fragment}" runtime_dialogue_fragment_position)
  if(runtime_dialogue_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "NPC dialogue actions lost runtime campaign dispatch; missing '${required_runtime_dialogue_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Game Event Hook.cpp"
  runtime_campaign_event_hook_contents)
foreach(required_runtime_event_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "case EVENT_ATTACK_INITIAL_SECTOR_IF_PLAYER_STILL_THERE:"
    "case EVENT_SAY_DELAYED_MERC_QUOTE:"
    "case EVENT_SEND_ENRICO_UNDERSTANDING_EMAIL:"
    "case EVENT_MEANWHILE:"
    "case EVENT_KINGPIN_BOUNTY_INITIAL:")
  string(FIND "${runtime_campaign_event_hook_contents}"
    "${required_runtime_event_fragment}" runtime_event_fragment_position)
  if(runtime_event_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic event dispatch lost runtime campaign selection; missing '${required_runtime_event_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/End Game.cpp"
  runtime_campaign_endgame_contents)
foreach(required_runtime_endgame_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "JA2_MULTIPURPOSE_EVENT_DONE_KILLING_DEIDRANNA"
    "JA2_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING"
    "JA25_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING"
    "DoneFadeOutJa25EndCinematic"
    "EndQueenDeathEndgameBeginEndCimenatic")
  string(FIND "${runtime_campaign_endgame_contents}"
    "${required_runtime_endgame_fragment}" runtime_endgame_fragment_position)
  if(runtime_endgame_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign endgame flow lost runtime composition; missing '${required_runtime_endgame_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  runtime_campaign_dialogue_event_contents)
foreach(required_runtime_dialogue_event_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "DIALOGUE_SPECIAL_EVENT_JERRY_MILO"
    "HandleInterfaceMessageForContinuingTrainingMilitia"
    "gfMorrisShouldSayHi"
    "gfMikeShouldSayHi"
    "CampaignProfileCode::Role::Ira"
    "RemoveJerryMiloBrokenLaptopOverlay"
    "JA25_MULTIPURPOSE_EVENT_GETUP_AFTER_HELI_CRASH"
    "JA25_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING"
    "JA2_MULTIPURPOSE_EVENT_DONE_KILLING_DEIDRANNA"
    "JA2_MULTIPURPOSE_EVENT_TEAM_MEMBERS_DONE_TALKING")
  string(FIND "${runtime_campaign_dialogue_event_contents}"
    "${required_runtime_dialogue_event_fragment}" runtime_dialogue_event_fragment_position)
  if(runtime_dialogue_event_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign dialogue-event flow lost runtime selection; missing '${required_runtime_dialogue_event_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Bottom.cpp"
  runtime_campaign_map_exit_contents)
foreach(required_runtime_map_exit_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "MAP_EXIT_TO_INTRO_SCREEN"
    "MAP_EXIT_TO_MAINMENU"
    "WillJerryMiloAllowThePlayerToCompressTimeAtBeginingOfGame"
    "BeginLoadScreen")
  string(FIND "${runtime_campaign_map_exit_contents}"
    "${required_runtime_map_exit_fragment}" runtime_map_exit_fragment_position)
  if(runtime_map_exit_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Map-screen campaign flow lost runtime selection; missing '${required_runtime_map_exit_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/MapScreen Quotes.cpp"
  runtime_campaign_map_quote_contents)
foreach(required_runtime_map_quote_fragment IN ITEMS
    "JerryMiloTalk("
    "WillJerryMiloAllowThePlayerToCompressTimeAtBeginingOfGame"
    "HandleJerryMiloQuotes("
    "HasJerryAlreadySaidTheMapScreenIntroSequence")
  string(FIND "${runtime_campaign_map_quote_contents}"
    "${required_runtime_map_quote_fragment}" runtime_map_quote_fragment_position)
  if(runtime_map_quote_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Jerry map-screen quote flow lost campaign-neutral emission; missing '${required_runtime_map_quote_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/opplist.cpp"
  runtime_campaign_sighting_contents)
foreach(required_runtime_sighting_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "CampaignProfileCode::Role::Slay"
    "gfMorrisShouldSayHi"
    "gfMikeShouldSayHi"
    "SOLDIER_QUOTE_SAID_EXT_MORRIS"
    "SOLDIER_QUOTE_SAID_EXT_MIKE")
  string(FIND "${runtime_campaign_sighting_contents}"
    "${required_runtime_sighting_fragment}" runtime_sighting_fragment_position)
  if(runtime_sighting_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical sighting flow lost runtime campaign selection; missing '${required_runtime_sighting_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.h"
  runtime_campaign_dialogue_alias_contents)
foreach(required_runtime_dialogue_alias_fragment IN ITEMS
    "DIALOGUE_SPECIAL_EVENT_JERRY_MILO"
    "DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA"
    "DIALOGUE_SPECIAL_EVENT_JERRY_MILO =="
    "DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA);")
  string(FIND "${runtime_campaign_dialogue_alias_contents}"
    "${required_runtime_dialogue_alias_fragment}" runtime_dialogue_alias_fragment_position)
  if(runtime_dialogue_alias_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared campaign dialogue-event aliases lost compatibility; missing '${required_runtime_dialogue_alias_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/TacticalActorQuoteFlags.h"
  runtime_campaign_quote_alias_contents)
foreach(required_runtime_quote_alias_fragment IN ITEMS
    "SOLDIER_QUOTE_SAID_EXT_MORRIS"
    "SOLDIER_QUOTE_SAID_EXT_MIKE"
    "SOLDIER_QUOTE_SAID_EXT_MORRIS =="
    "SOLDIER_QUOTE_SAID_EXT_MIKE);")
  string(FIND "${runtime_campaign_quote_alias_contents}"
    "${required_runtime_quote_alias_fragment}" runtime_quote_alias_fragment_position)
  if(runtime_quote_alias_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Shared campaign soldier-quote aliases lost compatibility; missing '${required_runtime_quote_alias_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  runtime_campaign_tactical_death_contents)
foreach(required_runtime_tactical_death_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "CampaignProfileCode::Role::Robot"
    "CampaignProfileCode::Role::Slay"
    "BeginHandleDeidrannaDeath("
    "BeginHandleQueenBitchDeath("
    "JA25_MULTIPURPOSE_EVENT_GETUP_AFTER_HELI_CRASH"
    "HandleDeathInPowerGenSector("
    "HandleFanStartingAtEndOfCombat("
    "HandlePOWQuestState("
    "HandleFirstBattleEndingWhileInTown("
    "AttemptToCapturePlayerSoldiers()")
  string(FIND "${runtime_campaign_tactical_death_contents}"
    "${required_runtime_tactical_death_fragment}" runtime_tactical_death_fragment_position)
  if(runtime_tactical_death_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical campaign endgame dispatch lost runtime selection; missing '${required_runtime_tactical_death_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/mercs.cpp"
  runtime_campaign_speck_death_contents)
foreach(required_runtime_speck_death_fragment IN ITEMS
    "HandleSpeckWitnessingEmployeeDeath("
    "CampaignProfileCode::ArulcoGaston"
    "CampaignProfileCode::ArulcoStogie"
    "JA2_SPECK_PLAYABLE_QUOTE_FIRST_MERC_DIES"
    "JA2_SPECK_PLAYABLE_QUOTE_STOGIE_DEAD")
  string(FIND "${runtime_campaign_speck_death_contents}"
    "${required_runtime_speck_death_fragment}" runtime_speck_death_fragment_position)
  if(runtime_speck_death_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Arulco Speck death callback lost campaign-neutral emission; missing '${required_runtime_speck_death_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Laptop/laptop.cpp"
  runtime_campaign_laptop_endgame_contents)
foreach(required_runtime_laptop_endgame_fragment IN ITEMS
    "GetGameContext().capabilities().isUnfinishedBusiness()"
    "HandleJa25EndGameAndGoToCreditsScreen(")
  string(FIND "${runtime_campaign_laptop_endgame_contents}"
    "${required_runtime_laptop_endgame_fragment}" runtime_laptop_endgame_fragment_position)
  if(runtime_laptop_endgame_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Laptop exit lost runtime campaign endgame selection; missing '${required_runtime_laptop_endgame_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/TacticalActor.h"
  runtime_campaign_soldier_state_contents)
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  runtime_campaign_soldier_save_contents)
foreach(required_runtime_soldier_state_fragment IN ITEMS
    "SoldierDeploymentComponent"
    "SoldierCombatContributionComponent")
  string(FIND "${runtime_campaign_soldier_state_contents}"
    "${required_runtime_soldier_state_fragment}" runtime_soldier_state_fragment_position)
  if(runtime_soldier_state_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Unified campaign soldier state lost field '${required_runtime_soldier_state_fragment}'")
  endif()
endforeach()
foreach(required_runtime_soldier_save_fragment IN ITEMS
    "ar.boolean(deployment.ignoreCollapseGetupCheck())"
    "ar.i32(deployment.arrivalGetupCounter())"
    "ar.boolean(deployment.waitingForArrivalGetup())"
    "ar.u8(combatContribution.damageByTeam()[i])")
  string(FIND "${runtime_campaign_soldier_save_contents}"
    "${required_runtime_soldier_save_fragment}" runtime_soldier_save_fragment_position)
  if(runtime_soldier_save_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Unified campaign soldier state lost persistence '${required_runtime_soldier_save_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CompiledGameplayBootstrap.cpp"
  compiled_gameplay_bootstrap_contents)
foreach(required_runtime_campaign_fragment IN ITEMS
    "capabilities.isUnfinishedBusiness()"
    "InitGridNoUB()")
  string(FIND "${compiled_gameplay_bootstrap_contents}"
    "${required_runtime_campaign_fragment}" runtime_campaign_fragment_position)
  if(runtime_campaign_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Campaign bootstrap no longer selects JA25 startup at runtime; missing '${required_runtime_campaign_fragment}'")
  endif()
endforeach()

set(headless_test_source "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp")
file(READ "${headless_test_source}" headless_test_contents)
string(FIND "${headless_test_contents}" "HeadlessTacticalTurnModel"
  retired_headless_model_position)
if(NOT retired_headless_model_position EQUAL -1)
  message(FATAL_ERROR
    "The retired test-local tactical battle model returned; use MemoryTacticalSimulation")
endif()
foreach(required_headless_executor_fragment IN ITEMS
    "MemoryTacticalSimulation captureModel"
    "runtime.bindSimulationCommandExecutor(executor)"
    "runtime.executeCommandsThrough(3, 2, &executionSink)")
  string(FIND "${headless_test_contents}"
    "${required_headless_executor_fragment}" required_headless_executor_position)
  if(required_headless_executor_position EQUAL -1)
    message(FATAL_ERROR
      "Headless replay bypasses the installed tactical executor; missing '${required_headless_executor_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/GameContext.cpp" game_context_contents)
string(FIND "${game_context_contents}" "BindJa2SimulationCommandExecutor(context)"
  production_executor_binding_position)
if(production_executor_binding_position EQUAL -1)
  message(FATAL_ERROR
    "The production composition root does not bind EngineRuntime's tactical executor")
endif()

# Network receive handlers, AI decisions, and scripted dialogue are
# authoritative command producers. Keep the legacy mutation APIs confined to
# Tactical/Simulation Commands.cpp so future fixes cannot quietly recreate a
# second execution path beside the deterministic queue.
function(strip_cxx_comments input_variable output_variable)
  # Avoid one whole-file nested regular expression here. CMake's regex engine
  # can exhaust its stack on large first-party translation units. Consume each
  # comment interval with bounded FIND/SUBSTRING operations. Choosing the first
  # delimiter matters: decorative //******** lines contain a later /* pair,
  # while block comments may themselves contain // text.
  set(comment_remaining "${${input_variable}}")
  set(comment_stripped "")
  while(1)
    string(FIND "${comment_remaining}" "/*" block_comment_start)
    string(FIND "${comment_remaining}" "//" line_comment_start)
    string(FIND "${comment_remaining}" "\"" double_quote_start)
    string(FIND "${comment_remaining}" "'" single_quote_start)
    set(next_comment_start -1)
    set(next_comment_kind "")
    if(line_comment_start GREATER -1)
      set(next_comment_start ${line_comment_start})
      set(next_comment_kind "line")
    endif()
    if(block_comment_start GREATER -1 AND
       (next_comment_start EQUAL -1 OR
        block_comment_start LESS next_comment_start))
      set(next_comment_start ${block_comment_start})
      set(next_comment_kind "block")
    endif()
    if(double_quote_start GREATER -1 AND
       (next_comment_start EQUAL -1 OR
        double_quote_start LESS next_comment_start))
      set(next_comment_start ${double_quote_start})
      set(next_comment_kind "double_quote")
    endif()
    if(single_quote_start GREATER -1 AND
       (next_comment_start EQUAL -1 OR
        single_quote_start LESS next_comment_start))
      set(next_comment_start ${single_quote_start})
      set(next_comment_kind "single_quote")
    endif()
    if(next_comment_start EQUAL -1)
      string(APPEND comment_stripped "${comment_remaining}")
      break()
    endif()
    if(next_comment_start GREATER 0)
      string(SUBSTRING "${comment_remaining}" 0
        ${next_comment_start} comment_prefix)
      string(APPEND comment_stripped "${comment_prefix}")
    endif()
    if(next_comment_kind STREQUAL "double_quote" OR
       next_comment_kind STREQUAL "single_quote")
      string(SUBSTRING "${comment_remaining}"
        ${next_comment_start} -1 quoted_tail)
      if(next_comment_kind STREQUAL "double_quote")
        string(REGEX MATCH "^\"([^\"\\\\]|\\\\.)*\""
          quoted_literal "${quoted_tail}")
      else()
        string(REGEX MATCH "^'([^'\\\\]|\\\\.)*'"
          quoted_literal "${quoted_tail}")
      endif()
      if(quoted_literal STREQUAL "")
        # A lone apostrophe can be a C++ digit separator. It is not a comment
        # delimiter, so preserve it and continue instead of rejecting the file.
        if(next_comment_kind STREQUAL "single_quote")
          string(APPEND comment_stripped "'")
          math(EXPR quoted_remainder_start "${next_comment_start} + 1")
          string(SUBSTRING "${comment_remaining}"
            ${quoted_remainder_start} -1 comment_remaining)
          continue()
        endif()
        message(FATAL_ERROR
          "Unterminated string literal while checking executable source")
      endif()
      string(APPEND comment_stripped "${quoted_literal}")
      string(LENGTH "${quoted_literal}" quoted_literal_length)
      math(EXPR quoted_remainder_start
        "${next_comment_start} + ${quoted_literal_length}")
      string(SUBSTRING "${comment_remaining}"
        ${quoted_remainder_start} -1 comment_remaining)
      continue()
    endif()
    math(EXPR comment_body_start "${next_comment_start} + 2")
    string(SUBSTRING "${comment_remaining}"
      ${comment_body_start} -1 comment_tail)
    if(next_comment_kind STREQUAL "line")
      string(FIND "${comment_tail}" "\n" line_comment_end)
      if(line_comment_end EQUAL -1)
        set(comment_remaining "")
        break()
      endif()
      string(APPEND comment_stripped "\n")
      math(EXPR line_comment_remainder_start "${line_comment_end} + 1")
      string(SUBSTRING "${comment_tail}"
        ${line_comment_remainder_start} -1 comment_remaining)
      continue()
    endif()
    string(FIND "${comment_tail}" "*/" block_comment_end)
    if(block_comment_end EQUAL -1)
      message(FATAL_ERROR
        "Unterminated block comment while checking executable source")
    endif()
    math(EXPR block_comment_remainder_start "${block_comment_end} + 2")
    string(SUBSTRING "${comment_tail}"
      ${block_comment_remainder_start} -1 comment_remaining)
  endwhile()
  set(${output_variable} "${comment_stripped}" PARENT_SCOPE)
endfunction()

function(check_network_command_ingress
    start_marker end_marker required_call forbidden_calls description)
  set(network_source_file "${SOURCE_ROOT}/Multiplayer/client.cpp")
  file(READ "${network_source_file}" network_source_contents)
  string(FIND "${network_source_contents}" "${start_marker}"
    ingress_start)
  if(ingress_start EQUAL -1)
    message(FATAL_ERROR
      "Cannot find multiplayer ingress region '${start_marker}'")
  endif()
  string(SUBSTRING "${network_source_contents}" ${ingress_start} -1
    ingress_tail)
  string(FIND "${ingress_tail}" "${end_marker}" ingress_end)
  if(ingress_end EQUAL -1)
    message(FATAL_ERROR
      "Cannot find end of multiplayer ingress region '${start_marker}'")
  endif()
  string(SUBSTRING "${ingress_tail}" 0 ${ingress_end}
    ingress_region)
  # Old commented-out examples are documentation, not executable bypasses.
  strip_cxx_comments(ingress_region ingress_executable)

  string(FIND "${ingress_executable}" "${required_call}"
    required_call_index)
  if(required_call_index EQUAL -1)
    message(FATAL_ERROR
      "${description} no longer enters through ${required_call}")
  endif()
  string(REGEX MATCH "${forbidden_calls}"
    direct_network_action "${ingress_executable}")
  if(direct_network_action)
    message(FATAL_ERROR
      "${description} bypasses SimulationCommand in Multiplayer/client.cpp")
  endif()
endfunction()

check_network_command_ingress(
  "void recievePATH("
  "void send_stance"
  "TryDispatchNetworkActorPathCommand"
  "(^|[^A-Za-z0-9_])(EVENT_InternalGetNewSoldierPath|EVENT_GetNewSoldierPath|SendGetNewSoldierPathEvent|EVENT_InternalSetSoldierPosition|EVENT_InitNewSoldierAnim)[ \t\r\n]*\\(|pSoldier[ \t]*->[ \t]*pathing[ \t]*\\."
  "Received multiplayer path")
check_network_command_ingress(
  "void recieveSTANCE("
  "void send_dir"
  "TryDispatchNetworkChangeStanceCommand"
  "(^|[^A-Za-z0-9_])(SendChangeSoldierStanceEvent|ChangeSoldierStance)[ \t\r\n]*\\("
  "Received multiplayer stance")
check_network_command_ingress(
  "void recieveDIR("
  "void send_fire("
  "TryDispatchNetworkSetFacingCommand"
  "(^|[^A-Za-z0-9_])(SendSoldierSetDesiredDirectionEvent|EVENT_SetSoldierDesiredDirection|EVENT_SetSoldierDirection)[ \t\r\n]*\\("
  "Received multiplayer facing")
check_network_command_ingress(
  "void recieveFIRE("
  "void send_hit("
  "TryDispatchNetworkActorFireCommand"
  "(^|[^A-Za-z0-9_])(SendBeginFireWeaponEvent|EVENT_FireSoldierWeapon|EVENT_SoldierBeginFireWeapon)[ \t\r\n]*\\("
  "Received multiplayer fire")
check_network_command_ingress(
  "void recieveEndTurn("
  "UINT8 numenemyLAN("
  "TryDispatchNetworkTurnCommand"
  "(^|[^A-Za-z0-9_])(EndTurn|BeginTeamTurn|EnterCombatMode|EndTurnEvents)[ \t\r\n]*\\("
  "Received multiplayer turn")
check_network_command_ingress(
  "void recieveSTOP ("
  "void mp_log_event("
  "TryDispatchNetworkActorStopCommand"
  "(^|[^A-Za-z0-9_])(StopSoldier|EVENT_StopMerc|EVENT_InternalSetSoldierPosition|EVENT_SetSoldierDirection|AdjustNoAPToFinishMove)[ \t\r\n]*\\("
  "Received multiplayer stop")

set(system_command_ingress_files
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIUtils.cpp"
  "${SOURCE_ROOT}/TacticalAI/Movement.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp")
foreach(system_command_ingress_file IN LISTS system_command_ingress_files)
  file(READ "${system_command_ingress_file}"
    system_command_ingress_contents)
  strip_cxx_comments(system_command_ingress_contents
    system_command_ingress_executable)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(EVENT_InternalGetNewSoldierPath|SendSoldierSetDesiredDirectionEvent|SendChangeSoldierStanceEvent|EVENT_SetSoldierDesiredDirection|ChangeSoldierStance|SendBeginFireWeaponEvent)[ \t\r\n]*\\("
    direct_system_action
    "${system_command_ingress_executable}")
  if(direct_system_action)
    message(FATAL_ERROR
      "AI or script action bypasses reliable System SimulationCommand ingress in ${system_command_ingress_file}")
  endif()
endforeach()

function(read_cxx_executable source_file output_variable)
  file(READ "${source_file}" source_contents)
  strip_cxx_comments(source_contents source_executable)
  set(${output_variable} "${source_executable}" PARENT_SCOPE)
endfunction()

function(require_system_command_ingress source_file required_call description)
  read_cxx_executable("${source_file}" source_executable)
  string(FIND "${source_executable}" "${required_call}" required_call_index)
  if(required_call_index EQUAL -1)
    message(FATAL_ERROR
      "${description} no longer enters through ${required_call}")
  endif()
endfunction()

require_system_command_ingress(
  "${SOURCE_ROOT}/TacticalAI/AIUtils.cpp"
  "TryDispatchSystemMoveToGridCommand"
  "AI movement")
require_system_command_ingress(
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "TryDispatchSystemSetFacingCommand"
  "AI facing")
require_system_command_ingress(
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "TryDispatchSystemChangeStanceCommand"
  "AI stance")
require_system_command_ingress(
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "TryDispatchSystemSetFacingCommand"
  "Dialogue facing")
require_system_command_ingress(
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "TryDispatchSystemChangeStanceCommand"
  "Scripted stance")
require_system_command_ingress(
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "TryDispatchSystemBeginSelectedFireWeaponCommand"
  "AI selected-weapon fire")

read_cxx_executable(
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  tactical_item_fire_executable)
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])SendBeginFireWeaponEvent[ \t\r\n]*\\("
  direct_item_fire_events "${tactical_item_fire_executable}")
list(LENGTH direct_item_fire_events direct_item_fire_event_count)
if(NOT direct_item_fire_event_count EQUAL 1)
  message(FATAL_ERROR
    "Handle Items must contain exactly one executor-continuation fire event; found ${direct_item_fire_event_count}")
endif()
string(FIND "${tactical_item_fire_executable}"
  "bool DispatchBeginFireWeaponFromHandleItem"
  item_fire_continuation_start)
string(FIND "${tactical_item_fire_executable}"
  "struct TacticalActorCallbackContext"
  item_fire_continuation_end)
if(item_fire_continuation_start EQUAL -1 OR
   item_fire_continuation_end LESS_EQUAL item_fire_continuation_start)
  message(FATAL_ERROR
    "Cannot locate the Handle Items executor-continuation fire slice")
endif()
math(EXPR item_fire_continuation_length
  "${item_fire_continuation_end} - ${item_fire_continuation_start}")
string(SUBSTRING "${tactical_item_fire_executable}"
  ${item_fire_continuation_start} ${item_fire_continuation_length}
  item_fire_continuation_slice)
foreach(required_item_fire_continuation IN ITEMS
  "if (continuation)"
  "SendBeginFireWeaponEvent"
  "ShouldReplicateWeaponConfigurationFire"
  "continuation->targetLevel"
  "continuation->targetCubeLevel")
  string(FIND "${item_fire_continuation_slice}"
    "${required_item_fire_continuation}"
    required_item_fire_continuation_index)
  if(required_item_fire_continuation_index EQUAL -1)
    message(FATAL_ERROR
      "Handle Items fire continuation lost ${required_item_fire_continuation}")
  endif()
endforeach()
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])SendBeginFireWeaponEvent[ \t\r\n]*\\("
  scoped_item_fire_events "${item_fire_continuation_slice}")
list(LENGTH scoped_item_fire_events scoped_item_fire_event_count)
if(NOT scoped_item_fire_event_count EQUAL 1)
  message(FATAL_ERROR
    "The sole direct Handle Items fire event escaped its admitted continuation")
endif()

# The public application command boundary is pointer-free. Every JA2 producer
# captures one complete, generation-aware TacticalEntityId from the exact live
# compatibility record before dispatch. Slot and incarnation must never be
# reassembled independently at UI, AI, dialogue, or network ingress.
file(READ "${SOURCE_ROOT}/Tactical/Simulation Commands.h"
  simulation_command_api)
string(FIND "${simulation_command_api}" "TacticalActor"
  simulation_command_record_index)
if(NOT simulation_command_record_index EQUAL -1)
  message(FATAL_ERROR
    "Tactical/Simulation Commands.h exposes TacticalActor; keep the public command boundary pointer-free")
endif()
string(REGEX MATCH
  "uniqueSoldierId|uiUniqueSoldierIdValue"
  split_simulation_command_api "${simulation_command_api}")
if(split_simulation_command_api)
  message(FATAL_ERROR
    "Tactical/Simulation Commands.h exposes a split actor identity; accept TacticalEntityId as one value")
endif()

set(player_command_ingress_files
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp")
foreach(player_command_ingress_file IN LISTS player_command_ingress_files)
  file(READ "${player_command_ingress_file}" contents)
  string(REGEX MATCH
    "TryDispatch[A-Za-z0-9_]+CommandNow[ \t\r\n]*\\([ \t\r\n]*\\*|TryDispatch[A-Za-z0-9_]+CommandNow[ \t\r\n]*\\([^;]*(identity\\(\\)\\.id\\(\\)|identity\\(\\)\\.incarnation\\(\\)|uiUniqueSoldierIdValue)"
    split_player_command_actor "${contents}")
  if(split_player_command_actor)
    message(FATAL_ERROR
      "Player command ingress bypasses complete TacticalEntityId capture in ${player_command_ingress_file}")
  endif()

  string(FIND "${contents}" "GetJa2TacticalEntityId"
    actor_capture_index)
  if(actor_capture_index EQUAL -1)
    message(FATAL_ERROR
      "Player command ingress no longer captures TacticalEntityId in ${player_command_ingress_file}")
  endif()

  string(REGEX MATCH
    "->movement\\(\\)\\.stealthMode\\(\\)[ \t]*=[ \t]*[^=]|->movement\\(\\)\\.setStealth[ \t\r\n]*\\(|(^|[^A-Za-z0-9_])StopSoldier[ \t\r\n]*\\("
    direct_player_squad_state_mutation "${contents}")
  if(direct_player_squad_state_mutation)
    message(FATAL_ERROR
      "Player squad input mutates stealth or movement state directly in ${player_command_ingress_file}; use the existing SimulationCommand")
  endif()
endforeach()

set(non_player_actor_command_ingress_files
  "${SOURCE_ROOT}/Multiplayer/client.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIUtils.cpp"
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp")
foreach(actor_command_ingress_file IN LISTS non_player_actor_command_ingress_files)
  file(READ "${actor_command_ingress_file}" contents)
  string(REGEX MATCH
    "TryDispatch(Network|System)[A-Za-z0-9_]+Command[ \t\r\n]*\\([ \t\r\n]*\\*|TryDispatch(Network|System)[A-Za-z0-9_]+Command[ \t\r\n]*\\([^;]*(identity\\(\\)\\.id\\(\\)|identity\\(\\)\\.incarnation\\(\\)|uiUniqueSoldierIdValue)"
    split_command_actor "${contents}")
  if(split_command_actor)
    message(FATAL_ERROR
      "Command ingress bypasses complete TacticalEntityId capture in ${actor_command_ingress_file}")
  endif()

  string(FIND "${contents}" "GetJa2TacticalEntityId"
    actor_capture_index)
  if(actor_capture_index EQUAL -1)
    message(FATAL_ERROR
      "Command ingress no longer captures TacticalEntityId in ${actor_command_ingress_file}")
  endif()
endforeach()

# Stance intent owns both stationary events and real-time moving-animation
# transitions inside the compatibility executor. Escape-driven drag
# cancellation is an actor command as well; UI code may react to an Applied
# result but may not mutate either gameplay state directly.
file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  player_stance_input_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])ChangeSoldierState[ \t\r\n]*\\(|movement\\(\\)\\.gridUpdatePolicy"
  direct_player_stance_transition "${player_stance_input_contents}")
if(direct_player_stance_transition)
  message(FATAL_ERROR
    "Player stance input bypasses the SimulationCommand executor in Tactical/Handle UI.cpp")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  player_drag_input_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])CancelDrag[ \t\r\n]*\\("
  direct_player_drag_cancellation "${player_drag_input_contents}")
if(direct_player_drag_cancellation)
  message(FATAL_ERROR
    "Player drag cancellation bypasses SimulationCommand in Tactical/Turn Based Input.cpp")
endif()

# Player peer interactions must cross the stable two-actor command boundary.
# AI/path obstruction swaps remain compatibility mechanics in their own
# subsystems, but UI input may not retain raw target pointers across execution.
set(player_peer_interaction_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp")
foreach(player_peer_interaction_file IN LISTS player_peer_interaction_files)
  file(READ "${player_peer_interaction_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(MercStealFromMerc|SwapMercPositions)[ \t\r\n]*\\("
    direct_player_peer_interaction "${contents}")
  if(direct_player_peer_interaction)
    message(FATAL_ERROR
      "Player peer interaction bypasses SimulationCommand in ${player_peer_interaction_file}")
  endif()
endforeach()

# Tactical modal requests must retain actor incarnations rather than raw
# TacticalActor globals.
file(READ "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  tactical_item_callback_contents)
string(REGEX MATCH
  "static[ \t]+TacticalActor[ \t]*\\*[ \t]*gpTempSoldier"
  raw_tactical_item_callback_actor
  "${tactical_item_callback_contents}")
if(raw_tactical_item_callback_actor)
  message(FATAL_ERROR
    "Tactical item callbacks retain a raw TacticalActor global")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  tactical_requester_callback_contents)
string(REGEX MATCH
  "gpRequesterMerc|gpRequesterTargetMerc"
  raw_tactical_requester_callback_actor
  "${tactical_requester_callback_contents}")
if(raw_tactical_requester_callback_actor)
  message(FATAL_ERROR
    "Tactical requester callbacks retain raw TacticalActor globals")
endif()

# Inventory panels and their modal children retain actor incarnations in the
# runtime-owned TacticalInventoryUiSession. Keep the retired pointer globals
# and pickup-menu member from returning under another call path.
file(READ "${SOURCE_ROOT}/Ja2/TacticalInventoryUiHost.h"
  tactical_inventory_host_contents)
if(tactical_inventory_host_contents MATCHES "TacticalActor")
  message(FATAL_ERROR
    "TacticalInventoryUiHost exposes TacticalActor; stable producers must use TacticalEntityId")
endif()
string(REGEX MATCH
  "(GetSMCurrentMerc|GetItemPointerSoldier|GetItemDescSoldier|GetAttachSoldier|GetItemPopupSoldier|GetItemPickupActor|GetItemPickupOpponent)[ \t\r\n]*\\("
  tactical_inventory_raw_getter
  "${tactical_inventory_host_contents}")
if(tactical_inventory_raw_getter)
  message(FATAL_ERROR
    "TacticalInventoryUiHost exposes raw actor resolution; keep it in TacticalInventoryUiLegacy")
endif()

file(READ "${SOURCE_ROOT}/Ja2/TacticalInventoryUiLegacy.h"
  tactical_inventory_legacy_contents)
if(NOT tactical_inventory_legacy_contents MATCHES
    "ResolveJa2TacticalInventoryActor")
  message(FATAL_ERROR
    "TacticalInventoryUiLegacy must retain the isolated compatibility resolver")
endif()

set(tactical_inventory_ui_files
  "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Map Inventory.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Enhanced.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Items.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Items.h"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.h"
  "${SOURCE_ROOT}/Tactical/Items.cpp"
  "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Rotting Corpses.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Profile.cpp"
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp")
foreach(tactical_inventory_ui_file IN LISTS tactical_inventory_ui_files)
  file(READ "${tactical_inventory_ui_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpSMCurrentMerc|gpItemPointerSoldier|gpItemDescSoldier|gpAttachSoldier|gpItemPopupSoldier|gpOpponent)([^A-Za-z0-9_]|$)|gItemPickupMenu[ \t\r\n]*\\.[ \t\r\n]*pSoldier"
    raw_tactical_inventory_ui_actor
    "${contents}")
  if(raw_tactical_inventory_ui_actor)
    message(FATAL_ERROR
      "Inventory UI retains a retired raw actor in ${tactical_inventory_ui_file}; use TacticalInventoryUiHost")
  endif()
  string(REGEX MATCH
    "Set(SMCurrentMerc|ItemPointerSoldier|ItemDescSoldier|AttachSoldier|ItemPopupSoldier|ItemPickupActor|ItemPickupOpponent)[ \t\r\n]*\\([ \t\r\n]*(NULL|nullptr|Get(SMCurrentMerc|ItemPointerSoldier|ItemDescSoldier|AttachSoldier|ItemPopupSoldier|ItemPickupActor|ItemPickupOpponent)[ \t\r\n]*\\()"
    raw_tactical_inventory_ui_producer
    "${contents}")
  if(raw_tactical_inventory_ui_producer)
    message(FATAL_ERROR
      "Inventory UI producer passes a raw actor in ${tactical_inventory_ui_file}; capture or copy TacticalEntityId and clear roles explicitly")
  endif()
endforeach()

# Booby-trap and mine-spotted callbacks must not bring back their former raw
# actor/item-pool/location globals. Callback-local compatibility aliases have
# initializers and therefore do not match these retired declarations.
string(REGEX MATCH
  "TacticalActor[ \t]*\\*[ \t]*gpBoobyTrapSoldier[ \t]*;|ITEM_POOL[ \t]*\\*[ \t]*gpBoobyTrapItemPool[ \t]*;|INT32[ \t]+gsBoobyTrapGridNo[ \t]*;|INT8[ \t]+gbBoobyTrapLevel[ \t]*;|BOOLEAN[ \t]+gfDisarmingBuriedBomb[ \t]*;|INT8[ \t]+gbTrapDifficulty[ \t]*;|BOOLEAN[ \t]+gfJustFoundBoobyTrap"
  raw_booby_trap_callback_state
  "${tactical_item_callback_contents}")
if(raw_booby_trap_callback_state)
  message(FATAL_ERROR
    "Booby-trap callbacks retain raw shared callback state")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Handle Items.h"
  tactical_item_callback_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  tactical_overhead_callback_contents)
string(REGEX MATCH
  "gpBoobyTrapSoldier|gsBoobyTrapGridNo"
  external_booby_trap_callback_state
  "${tactical_item_callback_header_contents};${tactical_overhead_callback_contents}")
if(external_booby_trap_callback_state)
  message(FATAL_ERROR
    "Mine-spotted callbacks mutate booby-trap callback globals")
endif()

# Multi-frame item locators resolve a stable world-item identity every update
# and render. The pointer-bearing ITEM_POOL_LOCATOR definition remains only as
# a legacy source-compatibility layout and must not back the production table.
string(REGEX MATCH
  "struct[ \t]+ItemPoolLocator[^}]*ITEM_POOL[ \t]*\\*|(^|[\r\n \t])FlashItemSlots[ \t]*\\[|guiNumFlashItemSlots"
  raw_production_item_locator
  "${tactical_item_callback_contents}")
if(raw_production_item_locator)
  message(FATAL_ERROR
    "Production item locators retain ITEM_POOL pointers")
endif()

# Delayed conversations, end-game timers, insurance prompts, and dismissal
# prompts resolve exact tactical-entity incarnations when work resumes. Their
# former raw callback globals must not return.
file(READ "${SOURCE_ROOT}/Ja2/TacticalEntityHost.h"
  tactical_entity_reference_header_contents)
string(REGEX MATCH
  "capture[ \t\r\n]*\\([^\\)]*TacticalActor"
  raw_tactical_entity_reference_capture
  "${tactical_entity_reference_header_contents}")
if(raw_tactical_entity_reference_capture)
  message(FATAL_ERROR
    "Ja2TacticalEntityReference captures TacticalActor; producers must cross to TacticalEntityId before retention")
endif()
string(REGEX MATCH
  "bool[ \t\r\n]+capture[ \t\r\n]*\\([ \t\r\n]*TacticalEntityId"
  stable_tactical_entity_reference_capture
  "${tactical_entity_reference_header_contents}")
if(NOT stable_tactical_entity_reference_capture)
  message(FATAL_ERROR
    "Ja2TacticalEntityReference is missing its TacticalEntityId capture boundary")
endif()

set(delayed_actor_capture_boundary_files
  "${SOURCE_ROOT}/Tactical/Handle Items.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.cpp"
  "${SOURCE_ROOT}/Strategic/Town Militia.cpp"
  "${SOURCE_ROOT}/TileEngine/Tactical Placement GUI.cpp")
foreach(delayed_actor_capture_boundary_file
    IN LISTS delayed_actor_capture_boundary_files)
  file(READ "${delayed_actor_capture_boundary_file}"
    delayed_actor_capture_boundary_contents)
  string(REGEX MATCH
    "(bool|void)[ \t\r\n]+(capture|captureTactical|captureMapCursor|begin)[ \t\r\n]*\\([^\\)]*TacticalActor"
    raw_delayed_actor_capture_boundary
    "${delayed_actor_capture_boundary_contents}")
  if(raw_delayed_actor_capture_boundary)
    message(FATAL_ERROR
      "Delayed actor context captures TacticalActor in ${delayed_actor_capture_boundary_file}; retain TacticalEntityId")
  endif()
endforeach()

set(delayed_actor_public_headers
  "${SOURCE_ROOT}/Tactical/interface Dialogue.h"
  "${SOURCE_ROOT}/Strategic/Merc Contract.h"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.h")
foreach(delayed_actor_public_header IN LISTS delayed_actor_public_headers)
  file(READ "${delayed_actor_public_header}"
    delayed_actor_public_header_contents)
  string(REGEX MATCH
    "(SetDialogueDestinationSoldier|SetContractRehireSoldier|CaptureTacticalTraversalChosenSoldier)[ \t\r\n]*\\([^\\)]*TacticalActor"
    raw_delayed_actor_public_producer
    "${delayed_actor_public_header_contents}")
  if(raw_delayed_actor_public_producer)
    message(FATAL_ERROR
      "Delayed actor producer exposes TacticalActor in ${delayed_actor_public_header}; use TacticalEntityId")
  endif()
endforeach()

set(delayed_actor_callback_files
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp")
foreach(delayed_actor_callback_file IN LISTS delayed_actor_callback_files)
  file(READ "${delayed_actor_callback_file}"
    delayed_actor_callback_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpPendingDestSoldier|gpPendingSrcSoldier|gpKillerSoldier|gpInsuranceSoldier|gpDismissSoldier|pAutomaticSurgeryDoctor|pAutomaticSurgeryPatient)([^A-Za-z0-9_]|$)"
    raw_delayed_callback_actor
    "${delayed_actor_callback_contents}")
  if(raw_delayed_callback_actor)
    message(FATAL_ERROR
      "Delayed callback retains a raw TacticalActor global in ${delayed_actor_callback_file}")
  endif()
endforeach()

# Interactive tactical sessions and application tools may span frames or modal
# callbacks just like delayed dialogue. Keep their former process-global actor
# pointers retired, require ID-only producer boundaries, and keep synthetic
# records out of dialogue APIs.
set(global_actor_session_files
  "${SOURCE_ROOT}/Tactical/Handle UI Plan.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI Plan.h"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.h"
  "${SOURCE_ROOT}/Tactical/Militia Control.cpp"
  "${SOURCE_ROOT}/Tactical/Militia Control.h"
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.h"
  "${SOURCE_ROOT}/Tactical/PATHAI.cpp"
  "${SOURCE_ROOT}/Tactical/Points.cpp"
  "${SOURCE_ROOT}/Tactical/Points.h"
  "${SOURCE_ROOT}/Tactical/Air Raid.cpp"
  "${SOURCE_ROOT}/Strategic/Quest Debug System.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Helicopter.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.h"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  "${SOURCE_ROOT}/Strategic/Strategic Merc Handler.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "${SOURCE_ROOT}/Ja2/aniviewscreen.cpp"
  "${SOURCE_ROOT}/Editor/EditorMercs.cpp")
set(retired_global_actor_session_names
  gpUIPlannedSoldier
  gpUIStartPlannedSoldier
  pTMilitiaSoldier
  gTalkingMercSoldier
  gpPathingBackpackCacheSoldier
  gpRaidSoldier
  pSkyRider
  SoldierSkyRider
  pProcessingSoldier
  fProcessingAMerc
  pTempSoldier
  pSoldierMovingList
  fSoldierIsMoving
  pUpdateSoldierBox
  giUpdateSoldierFaces)
set(global_actor_session_surface "")
foreach(global_actor_session_file IN LISTS global_actor_session_files)
  file(READ "${global_actor_session_file}"
    global_actor_session_contents)
  string(APPEND global_actor_session_surface
    "\n${global_actor_session_contents}")
  foreach(retired_global_actor_session_name
      IN LISTS retired_global_actor_session_names)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_global_actor_session_name}([^A-Za-z0-9_]|$)"
      retired_global_actor_session
      "${global_actor_session_contents}")
    if(retired_global_actor_session)
      message(FATAL_ERROR
        "Retired global actor session '${retired_global_actor_session_name}' returned in ${global_actor_session_file}")
    endif()
  endforeach()
endforeach()

set(stable_actor_session_headers
  "${SOURCE_ROOT}/Tactical/Handle UI Plan.h"
  "${SOURCE_ROOT}/Tactical/Handle UI.h"
  "${SOURCE_ROOT}/Tactical/Militia Control.h"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.h"
  "${SOURCE_ROOT}/Tactical/Points.h"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.h")
set(stable_actor_session_producers
  BeginUIPlan
  PopupMilitiaControlMenu
  CaptureMilitiaControlTarget
  VehicleMenu
  BeginPathingBackpackCache
  AddSoldierToMovingLists
  SelectSoldierForMovement
  DeselectSoldierForMovement
  AddSoldierToUpdateBox
  AddSoldierToWaitingListQueue)
set(stable_actor_session_header_surface "")
foreach(stable_actor_session_header IN LISTS stable_actor_session_headers)
  file(READ "${stable_actor_session_header}"
    stable_actor_session_header_contents)
  string(APPEND stable_actor_session_header_surface
    "\n${stable_actor_session_header_contents}")
  foreach(stable_actor_session_producer
      IN LISTS stable_actor_session_producers)
    string(REGEX MATCH
      "${stable_actor_session_producer}[ \t\r\n]*\\([^\\)]*TacticalActor"
      raw_actor_session_producer
      "${stable_actor_session_header_contents}")
    if(raw_actor_session_producer)
      message(FATAL_ERROR
        "Actor-session producer '${stable_actor_session_producer}' exposes TacticalActor in ${stable_actor_session_header}")
    endif()
  endforeach()
endforeach()
foreach(stable_actor_session_producer
    IN LISTS stable_actor_session_producers)
  string(REGEX MATCH
    "${stable_actor_session_producer}[ \t\r\n]*\\([^\\)]*TacticalEntityId"
    stable_actor_session_producer_found
    "${stable_actor_session_header_surface}")
  if(NOT stable_actor_session_producer_found)
    message(FATAL_ERROR
      "Actor-session producer '${stable_actor_session_producer}' lost its TacticalEntityId boundary")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Helicopter.cpp"
  helicopter_dialogue_contents)
file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  helicopter_map_dialogue_contents)
string(REGEX MATCH
  "HeliCharacterDialogue[ \t\r\n]*\\([^\\)]*TacticalActor"
  synthetic_helicopter_dialogue_actor
  "${helicopter_dialogue_contents};${helicopter_map_dialogue_contents}")
if(synthetic_helicopter_dialogue_actor)
  message(FATAL_ERROR
    "Helicopter dialogue regained a synthetic TacticalActor argument")
endif()

foreach(required_actor_session_fragment IN ITEMS
    "Ja2TacticalEntityReference[ \t]+gUiPlannedSoldier"
    "VehicleMenuContext[ \t]+gVehicleMenuContext"
    "Ja2TacticalEntityReference[ \t]+gMilitiaControlTarget"
    "BOOLEAN[ \t]+gfMilitiaControlTargetPendingMove"
    "TacticalEntityId[ \t]+gPathingBackpackCacheActor"
    "Ja2TacticalEntityReference[ \t]+gQdsTalkingMerc"
    "Ja2TacticalEntityReference[ \t]+gAniEditSoldier"
    "SoldierID[ \t]+gRaidSoldierSlot"
    "struct[ \t]+MapScreenMovementActorEntry"
    "struct[ \t]+MapScreenUpdateActorEntry")
  set(actor_session_fragment_found FALSE)
  string(REGEX MATCH "${required_actor_session_fragment}"
    actor_session_fragment
    "${global_actor_session_surface}")
  if(actor_session_fragment)
    set(actor_session_fragment_found TRUE)
  endif()
  if(NOT actor_session_fragment_found)
    message(FATAL_ERROR
      "Stable actor-session ownership lost required fragment '${required_actor_session_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  map_screen_actor_session_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])giMercPanelImage([^A-Za-z0-9_]|$)"
  shared_map_screen_update_panel
  "${map_screen_actor_session_contents}")
if(shared_map_screen_update_panel)
  message(FATAL_ERROR
    "Map-screen update UI regained the tactical-placement panel handle")
endif()
string(REGEX MATCH
  "SpecialCharacterDialogueEvent[ \t\r\n]*\\([^;]*actor\\.slot[ \t\r\n]*,[ \t\r\n]*actor\\.incarnation"
  stable_update_dialogue_producer
  "${map_screen_actor_session_contents}")
if(NOT stable_update_dialogue_producer)
  message(FATAL_ERROR
    "Map-screen update dialogue no longer queues the complete actor incarnation")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  dialogue_actor_session_contents)
string(REGEX MATCH
  "AddSoldierToUpdateBox[ \t\r\n]*\\([ \t\r\n]*TacticalEntityId[ \t\r\n]*\\{[^}]*uiSpecialEventData2[^}]*uiSpecialEventData3"
  stable_update_dialogue_consumer
  "${dialogue_actor_session_contents}")
if(NOT stable_update_dialogue_consumer)
  message(FATAL_ERROR
    "Map-screen update dialogue no longer consumes the complete actor incarnation")
endif()

# Merc departure prompts and contract-screen transitions retain stable actor
# incarnations. The save game continues to carry the legacy soldier slot, but
# save/load code must pass that slot through the resolving contract boundary.
set(contract_actor_lifetime_files
  "${SOURCE_ROOT}/Strategic/Merc Contract.cpp"
  "${SOURCE_ROOT}/Strategic/Merc Contract.h"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp")
foreach(contract_actor_lifetime_file IN LISTS contract_actor_lifetime_files)
  file(READ "${contract_actor_lifetime_file}"
    contract_actor_lifetime_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(pLeaveSoldier|pContractReHireSoldier)([^A-Za-z0-9_]|$)"
    raw_contract_lifetime_actor
    "${contract_actor_lifetime_contents}")
  if(raw_contract_lifetime_actor)
    message(FATAL_ERROR
      "Contract lifecycle retains a raw TacticalActor global in ${contract_actor_lifetime_file}")
  endif()
endforeach()

# The active and modal dialogue session likewise resolves exact actor
# incarnations. Quest facts and the quest-debug panel may use only the public
# resolving accessors, never resurrect the former raw participant globals.
set(active_dialogue_actor_files
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp"
  "${SOURCE_ROOT}/Tactical/interface Dialogue.h"
  "${SOURCE_ROOT}/Strategic/Quests.cpp"
  "${SOURCE_ROOT}/Strategic/Quest Debug System.cpp")
foreach(active_dialogue_actor_file IN LISTS active_dialogue_actor_files)
  file(READ "${active_dialogue_actor_file}"
    active_dialogue_actor_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpSrcSoldier|gpDestSoldier)([^A-Za-z0-9_]|$)"
    raw_dialogue_session_actor
    "${active_dialogue_actor_contents}")
  if(raw_dialogue_session_actor)
    message(FATAL_ERROR
      "Dialogue session retains a raw TacticalActor global in ${active_dialogue_actor_file}")
  endif()
endforeach()

# Facility and militia message-box callbacks retain immutable private prompt
# contexts. Actor identities must resolve through the tactical entity host, and
# the militia quote/mode may not return as independently mutable globals.
set(strategic_assignment_prompt_files
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.h"
  "${SOURCE_ROOT}/Strategic/Town Militia.cpp"
  "${SOURCE_ROOT}/Strategic/Town Militia.h")
foreach(strategic_assignment_prompt_file IN LISTS strategic_assignment_prompt_files)
  file(READ "${strategic_assignment_prompt_file}"
    strategic_assignment_prompt_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpFacilityStaffer|pMilitiaTrainerSoldier|gfYesNoPromptIsForContinue|giTotalCostOfTraining|gfAreWePromotingGreen|gfAreWePromotingRegular)([^A-Za-z0-9_]|$)"
    raw_strategic_assignment_prompt_state
    "${strategic_assignment_prompt_contents}")
  if(raw_strategic_assignment_prompt_state)
    message(FATAL_ERROR
      "Strategic assignment prompt retains raw shared callback state in ${strategic_assignment_prompt_file}")
  endif()
endforeach()

# Tactical traversal may cross a sector load and fade before its selected merc
# speaks. The chosen actor therefore remains an exact incarnation rather than
# an exported pointer into a reusable soldier slot.
set(tactical_traversal_actor_files
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.cpp"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.h"
  "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  "${SOURCE_ROOT}/Tactical/Strategic Exit GUI.cpp"
  "${SOURCE_ROOT}/Tactical/End Game.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp")
foreach(tactical_traversal_actor_file IN LISTS tactical_traversal_actor_files)
  file(READ "${tactical_traversal_actor_file}"
    tactical_traversal_actor_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpTacticalTraversalGroup|gpTacticalTraversalChosenSoldier)([^A-Za-z0-9_]|$)"
    raw_tactical_traversal_identity
    "${tactical_traversal_actor_contents}")
  if(raw_tactical_traversal_identity)
    message(FATAL_ERROR
      "Tactical traversal retains a raw group or chosen-merc pointer in ${tactical_traversal_actor_file}")
  endif()
endforeach()

# Every delayed strategic-group context now retains the runtime incarnation,
# not a reusable one-byte ID or a pointer into the legacy linked list.
set(strategic_group_context_files
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.cpp"
  "${SOURCE_ROOT}/Strategic/PreBattle Interface.h"
  "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.cpp"
  "${SOURCE_ROOT}/Strategic/strategicmap.h"
  "${SOURCE_ROOT}/Strategic/Auto Resolve.cpp"
  "${SOURCE_ROOT}/Strategic/mapscreen.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  "${SOURCE_ROOT}/Strategic/Map Screen Interface Map.cpp"
  "${SOURCE_ROOT}/Strategic/Queen Command.cpp"
  "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  "${SOURCE_ROOT}/Multiplayer/client.cpp")
foreach(strategic_group_context_file IN LISTS strategic_group_context_files)
  file(READ "${strategic_group_context_file}"
    strategic_group_context_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpBattleGroup|gpAdjacentGroup|gpPendingSimultaneousGroup|gpGroupPrompting|gpInitPrebattleGroup)([^A-Za-z0-9_]|$)"
    raw_strategic_group_context
    "${strategic_group_context_contents}")
  if(raw_strategic_group_context)
    message(FATAL_ERROR
      "Delayed strategic work retains a raw GROUP context in ${strategic_group_context_file}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  strategic_group_dialogue_contents)
string(REGEX MATCH
  "GetGroup[ \t\r\n]*\\([^\\)]*(uiSpecialEventData|uiUserData)"
  raw_dialogue_group_slot_resolution
  "${strategic_group_dialogue_contents}")
if(raw_dialogue_group_slot_resolution)
  message(FATAL_ERROR
    "Delayed dialogue resolves a reusable strategic-group slot without its incarnation")
endif()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  strategic_group_lifecycle_contents)
foreach(strategic_group_lifecycle_gateway IN ITEMS
  AdoptJa2StrategicGroup
  ReleaseJa2StrategicGroup
  RebuildJa2StrategicGroupDirectory)
  string(FIND "${strategic_group_lifecycle_contents}"
    "${strategic_group_lifecycle_gateway}"
    strategic_group_lifecycle_gateway_index)
  if(strategic_group_lifecycle_gateway_index EQUAL -1)
    message(FATAL_ERROR
      "Strategic group storage bypasses ${strategic_group_lifecycle_gateway}")
  endif()
endforeach()

# Tactical placement retains every participant and its selected/highlighted
# render state by exact actor incarnation for the full deployment modal.
set(tactical_placement_actor_files
  "${SOURCE_ROOT}/TileEngine/Tactical Placement GUI.cpp"
  "${SOURCE_ROOT}/TileEngine/Tactical Placement GUI.h"
  "${SOURCE_ROOT}/TileEngine/overhead map.cpp")
foreach(tactical_placement_actor_file IN LISTS tactical_placement_actor_files)
  file(READ "${tactical_placement_actor_file}"
    tactical_placement_actor_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(gpTacticalPlacementSelectedSoldier|gpTacticalPlacementHilightedSoldier)([^A-Za-z0-9_]|$)|\\.pSoldier"
    raw_tactical_placement_actor
    "${tactical_placement_actor_contents}")
  if(raw_tactical_placement_actor)
    message(FATAL_ERROR
      "Tactical placement retains raw actor state in ${tactical_placement_actor_file}")
  endif()
endforeach()

# Player weapon-mode, scope-mode, single-merc reload, and multi-merc reload-all
# intent now cross the deterministic command boundary. Internal weapon
# compatibility corrections, AI retaliation, and attachment changes remain
# local mechanics rather than pretending to be separate player commands.
set(player_weapon_control_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp")
foreach(player_weapon_control_file IN LISTS player_weapon_control_files)
  read_cxx_executable("${player_weapon_control_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(ChangeWeaponMode|ChangeScopeMode|InternalSoldierReadyWeapon|SoldierReadyWeapon)[ \t\r\n]*\\("
    direct_player_weapon_mode_call "${contents}")
  if(direct_player_weapon_mode_call)
    message(FATAL_ERROR
      "Player weapon configuration or ready intent bypasses SimulationCommand in ${player_weapon_control_file}")
  endif()
endforeach()

foreach(single_reload_file IN ITEMS
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp")
  read_cxx_executable("${single_reload_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])AutoReload[ \t\r\n]*\\("
    direct_player_reload_call "${contents}")
  if(direct_player_reload_call)
    message(FATAL_ERROR
      "Player reload intent bypasses SimulationCommand in ${single_reload_file}")
  endif()
endforeach()

read_cxx_executable("${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  turn_based_input_contents)
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])AutoReload[ \t\r\n]*\\("
  turn_based_bulk_reload_calls "${turn_based_input_contents}")
list(LENGTH turn_based_bulk_reload_calls turn_based_bulk_reload_count)
if(NOT turn_based_bulk_reload_count EQUAL 3)
  message(FATAL_ERROR
    "The three established AutoReload calls must remain confined to the bulk-reload compatibility executor")
endif()

file(READ "${SOURCE_ROOT}/Engine/Adapters/JA2/SimulationCommand.h"
  simulation_command_value_contents)
foreach(required_bulk_reload_value IN ITEMS
  "TacticalBulkReloadActorCapacity = 260"
  "enum class TacticalBulkReloadMode"
  "struct BulkReloadWeaponsCommand"
  "std::array<TacticalEntityId, TacticalBulkReloadActorCapacity> soldiers"
  "value.soldiers[index - 1].slot >=")
  string(FIND "${simulation_command_value_contents}"
    "${required_bulk_reload_value}" required_bulk_reload_value_index)
  if(required_bulk_reload_value_index EQUAL -1)
    message(FATAL_ERROR
      "Bulk-reload value boundary lost ${required_bulk_reload_value}")
  endif()
endforeach()

foreach(bulk_reload_gateway IN ITEMS
  "TryDispatchBulkReloadWeaponsCommandNow"
  "ExecuteBulkReloadWeaponsCommand"
  "CaptureCanonicalBulkReloadRoster"
  "CurrentSquad() != command.squad"
  "CaptureTacticalBulkReloadMode() != command.mode"
  "canonical.soldierCount != command.soldierCount")
  string(FIND "${turn_based_input_contents}"
    "${bulk_reload_gateway}" bulk_reload_gateway_index)
  if(bulk_reload_gateway_index EQUAL -1)
    message(FATAL_ERROR
      "Turn-based reload-all lost ${bulk_reload_gateway}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp"
  simulation_command_executor_contents)
foreach(required_bulk_reload_executor IN ITEMS
  "std::is_same<Command, BulkReloadWeaponsCommand>::value"
  "ExecuteBulkReloadWeaponsCommand(value)"
  "InvalidBulkReloadRoster"
  "TryDispatchBulkReloadWeaponsCommandNow"
  "disposition != CommandDisposition::Applied"
  "value.soldierCount > TacticalBulkReloadActorCapacity")
  string(FIND "${simulation_command_executor_contents}"
    "${required_bulk_reload_executor}" required_bulk_reload_executor_index)
  if(required_bulk_reload_executor_index EQUAL -1)
    message(FATAL_ERROR
      "Bulk-reload executor lost ${required_bulk_reload_executor}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Engine/Adapters/JA2/SimulationCommandCodec.cpp"
  simulation_command_codec_contents)
foreach(required_bulk_reload_codec IN ITEMS
  "BulkReloadWeapons = 28"
  "case CommandTag::BulkReloadWeapons"
  "value.soldierCount > TacticalBulkReloadActorCapacity")
  string(FIND "${simulation_command_codec_contents}"
    "${required_bulk_reload_codec}" required_bulk_reload_codec_index)
  if(required_bulk_reload_codec_index EQUAL -1)
    message(FATAL_ERROR
      "Bulk-reload replay codec lost ${required_bulk_reload_codec}")
  endif()
endforeach()

foreach(bulk_reload_test_manifest IN ITEMS
  "${SOURCE_ROOT}/tests/simulation_command_bulk_reload_model_tests.cpp"
  "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  "${SOURCE_ROOT}/tests/CMakeLists.txt"
  "${SOURCE_ROOT}/.github/workflows/build_unix.yml")
  if(NOT EXISTS "${bulk_reload_test_manifest}")
    message(FATAL_ERROR
      "Bulk-reload test manifest is missing: ${bulk_reload_test_manifest}")
  endif()
  file(READ "${bulk_reload_test_manifest}"
    bulk_reload_test_manifest_contents)
  if(bulk_reload_test_manifest MATCHES "\\.cpp$")
    set(bulk_reload_test_marker "BulkReloadWeaponsCommand")
  else()
    set(bulk_reload_test_marker
      "simulation_command_bulk_reload_model_tests")
  endif()
  string(FIND "${bulk_reload_test_manifest_contents}"
    "${bulk_reload_test_marker}"
    required_bulk_reload_test_index)
  if(required_bulk_reload_test_index EQUAL -1)
    message(FATAL_ERROR
      "Bulk-reload test manifest lost its target marker: ${bulk_reload_test_manifest}")
  endif()
endforeach()

foreach(bulk_reload_document IN ITEMS
  "${SOURCE_ROOT}/docs/ENGINE_ARCHITECTURE.md"
  "${SOURCE_ROOT}/docs/ENGINE_SDK.md")
  file(READ "${bulk_reload_document}" bulk_reload_document_contents)
  string(FIND "${bulk_reload_document_contents}"
    "BulkReloadWeaponsCommand" bulk_reload_document_index)
  if(bulk_reload_document_index EQUAL -1)
    message(FATAL_ERROR
      "Bulk-reload command contract is undocumented in ${bulk_reload_document}")
  endif()
endforeach()

# Equipment and attachment correction is an exact System/Replay result, not a
# player Cycle command. Presentation, gameplay continuation, and event
# replication remain separate values, and the portable model must explicitly
# decline the JA2-only inventory policy.
read_cxx_executable(
  "${SOURCE_ROOT}/Engine/Adapters/JA2/SimulationCommand.h"
  weapon_configuration_value_executable)
read_cxx_executable(
  "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp"
  weapon_configuration_executor_executable)
read_cxx_executable(
  "${SOURCE_ROOT}/Engine/Adapters/JA2/SimulationCommandCodec.cpp"
  weapon_configuration_codec_executable)
foreach(required_weapon_configuration_value IN ITEMS
  "struct TacticalWeaponConfigurationResult"
  "struct ApplyWeaponConfigurationCommand"
  "enum class TacticalWeaponConfigurationCause"
  "enum class TacticalWeaponConfigurationPostApplyPolicy"
  "enum class TacticalWeaponConfigurationContinuation"
  "TacticalEventPolicy eventPolicy"
  "IsSimulationSystemSource(value.source)"
  "value.handItem"
  "inventoryPosition"
  "SimulationCommandPlaybackPolicy"
  "ShouldReplicateWeaponConfigurationFire")
  string(FIND "${weapon_configuration_value_executable}"
    "${required_weapon_configuration_value}"
    required_weapon_configuration_value_index)
  if(required_weapon_configuration_value_index EQUAL -1)
    message(FATAL_ERROR
      "Weapon-configuration value boundary lost ${required_weapon_configuration_value}")
  endif()
endforeach()

string(FIND "${weapon_configuration_executor_executable}"
  "std::is_same<Command, ApplyWeaponConfigurationCommand>::value)"
  weapon_configuration_executor_start)
string(FIND "${weapon_configuration_executor_executable}"
  "else if constexpr (std::is_same<Command, ChangeStanceCommand>::value)"
  weapon_configuration_executor_end)
if(weapon_configuration_executor_start EQUAL -1 OR
   weapon_configuration_executor_end LESS_EQUAL
     weapon_configuration_executor_start)
  message(FATAL_ERROR
    "Cannot locate the weapon-configuration execution branch")
endif()
math(EXPR weapon_configuration_executor_length
  "${weapon_configuration_executor_end} - ${weapon_configuration_executor_start}")
string(SUBSTRING "${weapon_configuration_executor_executable}"
  ${weapon_configuration_executor_start}
  ${weapon_configuration_executor_length}
  weapon_configuration_executor_slice)
foreach(required_weapon_configuration_executor IN ITEMS
  "std::is_same<Command, ApplyWeaponConfigurationCommand>::value"
  "ResolveExpectedWeaponConfiguration"
  "ApplyWeaponConfigurationResult"
  "ApplyWeaponConfigurationPresentation"
  "CompleteHandItemChange"
  "CompleteEquipmentChange"
  "CompleteEquipmentTacticalEffects"
  "BeginFriendlyRetaliation"
  "HandleItemFromWeaponConfigurationCommand")
  string(FIND "${weapon_configuration_executor_slice}"
    "${required_weapon_configuration_executor}"
    required_weapon_configuration_executor_index)
  if(required_weapon_configuration_executor_index EQUAL -1)
    message(FATAL_ERROR
      "Weapon-configuration executor lost ${required_weapon_configuration_executor}")
  endif()
endforeach()
string(FIND "${weapon_configuration_executor_slice}"
  "ResolveExpectedWeaponConfiguration"
  weapon_configuration_resolve_before_mutation_index)
string(FIND "${weapon_configuration_executor_slice}"
  "ApplyWeaponConfigurationResult"
  weapon_configuration_first_mutation_index)
if(weapon_configuration_resolve_before_mutation_index EQUAL -1 OR
   weapon_configuration_first_mutation_index EQUAL -1 OR
   weapon_configuration_resolve_before_mutation_index GREATER
     weapon_configuration_first_mutation_index)
  message(FATAL_ERROR
    "Weapon configuration is no longer re-resolved before mutation")
endif()

foreach(required_weapon_configuration_codec IN ITEMS
  "ApplyWeaponConfiguration = 29"
  "case CommandTag::ApplyWeaponConfiguration"
  "WeaponConfigurationKnownFlags")
  string(FIND "${weapon_configuration_codec_executable}"
    "${required_weapon_configuration_codec}"
    required_weapon_configuration_codec_index)
  if(required_weapon_configuration_codec_index EQUAL -1)
    message(FATAL_ERROR
      "Weapon-configuration replay codec lost ${required_weapon_configuration_codec}")
  endif()
endforeach()

read_cxx_executable(
  "${SOURCE_ROOT}/Engine/Adapters/JA2/MemoryTacticalSimulation.cpp"
  memory_tactical_simulation_executable)
string(FIND "${memory_tactical_simulation_executable}"
  "std::is_same<Command, ApplyWeaponConfigurationCommand>::value"
  weapon_configuration_reference_discard_index)
if(weapon_configuration_reference_discard_index EQUAL -1)
  message(FATAL_ERROR
    "MemoryTacticalSimulation no longer explicitly declines weapon-configuration policy")
endif()

read_cxx_executable("${SOURCE_ROOT}/Tactical/UI Cursors.cpp"
  weapon_configuration_cursor_contents)
read_cxx_executable("${SOURCE_ROOT}/Tactical/Interface Items.cpp"
  weapon_configuration_interface_items_contents)
read_cxx_executable("${SOURCE_ROOT}/Tactical/Weapons.cpp"
  weapon_configuration_weapons_contents)
read_cxx_executable("${SOURCE_ROOT}/Tactical/Overhead.cpp"
  weapon_configuration_overhead_contents)
read_cxx_executable("${SOURCE_ROOT}/Tactical/TacticalActorRangedActions.cpp"
  weapon_configuration_ranged_actions_contents)
read_cxx_executable("${SOURCE_ROOT}/Tactical/Items.cpp"
  deferred_weapon_configuration_items_contents)
foreach(weapon_configuration_producer_contents IN ITEMS
  weapon_configuration_cursor_contents
  weapon_configuration_interface_items_contents)
  string(FIND "${${weapon_configuration_producer_contents}}"
    "TryDispatchSystemApplyWeaponConfigurationCommand"
    weapon_configuration_ingress_index)
  if(weapon_configuration_ingress_index EQUAL -1)
    message(FATAL_ERROR
      "Equipment/attachment producer lost weapon-configuration command ingress")
  endif()
endforeach()
foreach(required_attachment_configuration_marker IN ITEMS
  "ResolveNextTacticalScopeConfiguration"
  "TryDispatchSystemApplyWeaponConfigurationCommand"
  "if (!dispatch.accepted())"
  "ManLooksForOtherTeams(configurationActor)")
  string(FIND "${weapon_configuration_interface_items_contents}"
    "${required_attachment_configuration_marker}"
    required_attachment_configuration_marker_index)
  if(required_attachment_configuration_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Attachment removal/sight refresh lost ${required_attachment_configuration_marker}")
  endif()
endforeach()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])ChangeWeaponMode[ 	\r\n]*\\("
  direct_cursor_weapon_mode_change
  "${weapon_configuration_cursor_contents}")
if(direct_cursor_weapon_mode_change)
  message(FATAL_ERROR
    "Unavailable-launcher cursor correction reused player ChangeWeaponMode")
endif()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])ChangeScopeMode[ 	\r\n]*\\("
  direct_attachment_scope_change
  "${weapon_configuration_interface_items_contents}")
if(direct_attachment_scope_change)
  message(FATAL_ERROR
    "Attachment scope correction reused player ChangeScopeMode")
endif()

string(FIND "${weapon_configuration_weapons_contents}"
  "void HandleTacticalEffectsOfEquipmentChange"
  weapon_configuration_equipment_start)
string(FIND "${weapon_configuration_weapons_contents}"
  "void ChangeWeaponMode" weapon_configuration_equipment_end)
if(weapon_configuration_equipment_start EQUAL -1 OR
   weapon_configuration_equipment_end EQUAL -1 OR
   weapon_configuration_equipment_end LESS_EQUAL
     weapon_configuration_equipment_start)
  message(FATAL_ERROR
    "Cannot locate the equipment weapon-configuration compatibility slice")
endif()
math(EXPR weapon_configuration_equipment_length
  "${weapon_configuration_equipment_end} - ${weapon_configuration_equipment_start}")
string(SUBSTRING "${weapon_configuration_weapons_contents}"
  ${weapon_configuration_equipment_start}
  ${weapon_configuration_equipment_length}
  weapon_configuration_equipment_slice)
foreach(forbidden_equipment_configuration_write IN ITEMS
  "pSoldier->attackSelection().weaponMode() = WM_"
  "pSoldier->attackSelection().scopeMode() = USE_"
  "pSoldier->fireControl().selectSingleShot()"
  "pSoldier->fireControl().selectAutofire()")
  string(FIND "${weapon_configuration_equipment_slice}"
    "${forbidden_equipment_configuration_write}"
    forbidden_equipment_configuration_write_index)
  if(NOT forbidden_equipment_configuration_write_index EQUAL -1)
    message(FATAL_ERROR
      "Equipment reconciliation bypasses its exact command result: ${forbidden_equipment_configuration_write}")
  endif()
endforeach()
foreach(required_equipment_configuration_marker IN ITEMS
  "ResolveEquipmentTacticalWeaponConfiguration"
  "TryDispatchSystemApplyWeaponConfigurationCommand"
  "CompleteHandItemChange"
  "CompleteEquipmentChange"
  "CompleteEquipmentTacticalEffects")
  string(FIND "${weapon_configuration_equipment_slice}"
    "${required_equipment_configuration_marker}"
    required_equipment_configuration_marker_index)
  if(required_equipment_configuration_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Equipment reconciliation lost ${required_equipment_configuration_marker}")
  endif()
endforeach()

string(FIND "${weapon_configuration_ranged_actions_contents}"
  "bool TacticalActorRangedActions::refreshAfterHandItemChange"
  weapon_configuration_refresh_start)
if(weapon_configuration_refresh_start EQUAL -1)
  message(FATAL_ERROR
    "Cannot locate the post-configuration hand-item refresh slice")
endif()
string(SUBSTRING "${weapon_configuration_ranged_actions_contents}"
  ${weapon_configuration_refresh_start} -1
  weapon_configuration_refresh_slice)
string(REGEX MATCH
  "(weaponMode|scopeMode|burstCounter|autofireShots)[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*=[ \t\r\n]*[^= \t\r\n]"
  direct_refresh_configuration_write
  "${weapon_configuration_refresh_slice}")
if(direct_refresh_configuration_write)
  message(FATAL_ERROR
    "Hand-item refresh reclaimed weapon-configuration mutation")
endif()

string(FIND "${weapon_configuration_overhead_contents}"
  "const bool hasGun =" retaliation_configuration_start)
string(FIND "${weapon_configuration_overhead_contents}"
  "if ( IsProfileAHeadMiner" retaliation_configuration_end)
if(retaliation_configuration_start EQUAL -1 OR
   retaliation_configuration_end LESS_EQUAL retaliation_configuration_start)
  message(FATAL_ERROR
    "Cannot locate the friendly-retaliation configuration slice")
endif()
math(EXPR retaliation_configuration_length
  "${retaliation_configuration_end} - ${retaliation_configuration_start}")
string(SUBSTRING "${weapon_configuration_overhead_contents}"
  ${retaliation_configuration_start} ${retaliation_configuration_length}
  retaliation_configuration_slice)
foreach(required_retaliation_configuration_marker IN ITEMS
  "TacticalWeaponConfigurationCause::"
  "FriendlyRetaliation"
  "BeginFriendlyRetaliation"
  "TacticalEventPolicy::Replicated"
  "retaliationConfigurationRequired"
  "!retaliationConfigurationRequired")
  string(FIND "${retaliation_configuration_slice}"
    "${required_retaliation_configuration_marker}"
    required_retaliation_configuration_marker_index)
  if(required_retaliation_configuration_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Overhead weapon-configuration slice lost ${required_retaliation_configuration_marker}")
  endif()
endforeach()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])ChangeWeaponMode[ 	\r\n]*\\("
  direct_retaliation_weapon_mode_change
  "${retaliation_configuration_slice}")
if(direct_retaliation_weapon_mode_change)
  message(FATAL_ERROR
    "Friendly retaliation reused player ChangeWeaponMode")
endif()
string(REGEX MATCH
  "(weaponMode|scopeMode|burstCounter|autofireShots)[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*=[ \t\r\n]*[^= \t\r\n]"
  direct_retaliation_configuration_write
  "${retaliation_configuration_slice}")
if(direct_retaliation_configuration_write)
  message(FATAL_ERROR
    "Friendly retaliation mutates configuration outside its command")
endif()

string(FIND "${weapon_configuration_overhead_contents}"
  "if ( pSoldier && (pSoldier->attackSelection().weaponMode() == WM_ATTACHED_GL"
  post_attack_configuration_start)
string(FIND "${weapon_configuration_overhead_contents}"
  "pSoldier->targeting().lastGridNo() ="
  post_attack_configuration_end)
if(post_attack_configuration_start EQUAL -1 OR
   post_attack_configuration_end LESS_EQUAL post_attack_configuration_start)
  message(FATAL_ERROR
    "Cannot locate the attached-launcher post-attack reset slice")
endif()
math(EXPR post_attack_configuration_length
  "${post_attack_configuration_end} - ${post_attack_configuration_start}")
string(SUBSTRING "${weapon_configuration_overhead_contents}"
  ${post_attack_configuration_start} ${post_attack_configuration_length}
  post_attack_configuration_slice)
string(FIND "${post_attack_configuration_slice}"
  "TacticalWeaponConfigurationCause::AttachedLauncherShotCompleted"
  post_attack_configuration_ingress_index)
string(REGEX MATCH
  "(weaponMode|scopeMode|burstCounter|autofireShots)[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*=[ \t\r\n]*[^= \t\r\n]"
  direct_post_attack_configuration_write
  "${post_attack_configuration_slice}")
if(post_attack_configuration_ingress_index EQUAL -1 OR
   direct_post_attack_configuration_write)
  message(FATAL_ERROR
    "Attached-launcher post-attack reset bypasses its exact command result")
endif()

# This wave intentionally does not claim the lower-level item/animation
# mechanics that still write configuration directly. Pin the complete active
# debt so new sites cannot hide behind that deferral and migration cannot be
# claimed by comments alone.
string(REGEX MATCHALL
  "pSoldier->attackSelection[ \t\r\n]*\\([ \t\r\n]*\\)\\.(weaponMode|scopeMode)[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*=[ \t\r\n]*(WM_|USE_)"
  deferred_items_configuration_writes
  "${deferred_weapon_configuration_items_contents}")
list(LENGTH deferred_items_configuration_writes
  deferred_items_configuration_write_count)
string(REGEX MATCHALL
  "pSoldier->fireControl[ \t\r\n]*\\([ \t\r\n]*\\)\\.select(SingleShot|Autofire)[ \t\r\n]*\\("
  deferred_items_fire_control_writes
  "${deferred_weapon_configuration_items_contents}")
list(LENGTH deferred_items_fire_control_writes
  deferred_items_fire_control_write_count)
string(FIND "${deferred_weapon_configuration_items_contents}"
  "pSoldier->attackSelection().weaponMode() = WM_ATTACHED_GL;"
  deferred_rifle_grenade_configuration_index)
if(NOT deferred_items_configuration_write_count EQUAL 9 OR
   NOT deferred_items_fire_control_write_count EQUAL 4 OR
   deferred_rifle_grenade_configuration_index EQUAL -1)
  message(FATAL_ERROR
    "Items.cpp weapon-configuration debt changed; migrate it or update the explicit deferred contract")
endif()
string(REGEX MATCHALL
  "pSoldier->attackSelection[ \t\r\n]*\\([ \t\r\n]*\\)\\.scopeMode[ \t\r\n]*\\([ \t\r\n]*\\)[ \t\r\n]*=[ \t\r\n]*USE_BEST_SCOPE"
  deferred_overhead_scope_writes
  "${weapon_configuration_overhead_contents}")
list(LENGTH deferred_overhead_scope_writes
  deferred_overhead_scope_write_count)
if(NOT deferred_overhead_scope_write_count EQUAL 1)
  message(FATAL_ERROR
    "Overhead suppression-scope debt changed; migrate it or update the explicit deferred contract")
endif()

foreach(weapon_configuration_debt_document IN ITEMS
  "${SOURCE_ROOT}/docs/ENGINE_ARCHITECTURE.md"
  "${SOURCE_ROOT}/docs/ENGINE_SDK.md")
  file(READ "${weapon_configuration_debt_document}"
    weapon_configuration_debt_document_contents)
  string(REGEX REPLACE "[ \t\r\n]+" " "
    weapon_configuration_debt_document_contents
    "${weapon_configuration_debt_document_contents}")
  foreach(weapon_configuration_debt_marker IN ITEMS
    "rifle-grenade attachment insertion"
    "direct hand placement"
    "attachment removal"
    "HandleSuppressionFire")
    string(FIND "${weapon_configuration_debt_document_contents}"
      "${weapon_configuration_debt_marker}"
      weapon_configuration_debt_marker_index)
    if(weapon_configuration_debt_marker_index EQUAL -1)
      message(FATAL_ERROR
        "Weapon-configuration deferred debt is overclaimed in ${weapon_configuration_debt_document}: missing ${weapon_configuration_debt_marker}")
    endif()
  endforeach()
endforeach()

foreach(weapon_configuration_test_manifest IN ITEMS
  "${SOURCE_ROOT}/tests/simulation_command_weapon_configuration_model_tests.cpp"
  "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  "${SOURCE_ROOT}/tests/CMakeLists.txt"
  "${SOURCE_ROOT}/.github/workflows/build_unix.yml")
  if(weapon_configuration_test_manifest MATCHES "ja2_headless_tests\\.cpp$")
    read_cxx_executable("${weapon_configuration_test_manifest}"
      weapon_configuration_test_manifest_contents)
    set(weapon_configuration_test_marker
      "validWeaponConfiguration")
  elseif(weapon_configuration_test_manifest MATCHES "\\.cpp$")
    read_cxx_executable("${weapon_configuration_test_manifest}"
      weapon_configuration_test_manifest_contents)
    set(weapon_configuration_test_marker
      "ApplyWeaponConfigurationCommand")
  else()
    file(READ "${weapon_configuration_test_manifest}"
      weapon_configuration_test_manifest_contents)
    string(REGEX REPLACE "#[^\r\n]*" ""
      weapon_configuration_test_manifest_contents
      "${weapon_configuration_test_manifest_contents}")
    set(weapon_configuration_test_marker
      "simulation_command_weapon_configuration_model_tests")
  endif()
  string(FIND "${weapon_configuration_test_manifest_contents}"
    "${weapon_configuration_test_marker}"
    weapon_configuration_test_marker_index)
  if(weapon_configuration_test_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Weapon-configuration test manifest lost its marker: ${weapon_configuration_test_manifest}")
  endif()
endforeach()

read_cxx_executable(
  "${SOURCE_ROOT}/tests/simulation_command_weapon_configuration_model_tests.cpp"
  weapon_configuration_model_test_executable)
foreach(required_weapon_configuration_model_marker IN ITEMS
  "const std::vector<std::uint8_t> expectedWire"
  "encoded == expectedWire"
  "SourceOffset = 80"
  "SameCommand(roundTrip, fixture)"
  "stageRecordedPlaybackBatch"
  "SimulationCommandSource::Replay"
  "ShouldReplicateWeaponConfigurationFire"
  "reference.snapshot() == referenceState")
  string(FIND "${weapon_configuration_model_test_executable}"
    "${required_weapon_configuration_model_marker}"
    required_weapon_configuration_model_marker_index)
  if(required_weapon_configuration_model_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Weapon-configuration model lost ${required_weapon_configuration_model_marker}")
  endif()
endforeach()

read_cxx_executable("${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  weapon_configuration_headless_test_executable)
foreach(required_weapon_configuration_headless_marker IN ITEMS
  "ResolveDefaultTacticalWeaponConfiguration"
  "staleConfigurationDiscarded"
  "same-hand stale configuration")
  string(FIND "${weapon_configuration_headless_test_executable}"
    "${required_weapon_configuration_headless_marker}"
    required_weapon_configuration_headless_marker_index)
  if(required_weapon_configuration_headless_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Weapon-configuration headless coverage lost ${required_weapon_configuration_headless_marker}")
  endif()
endforeach()

read_cxx_executable(
  "${SOURCE_ROOT}/Engine/Adapters/JA2/EngineRuntime.h"
  weapon_configuration_runtime_executable)
read_cxx_executable("${SOURCE_ROOT}/Engine/Core/CommandStream.h"
  weapon_configuration_stream_executable)
foreach(required_playback_origin_marker IN ITEMS
  "CommandStream<SimulationCommand, SimulationCommandPlaybackPolicy>"
  "stageRecordedPlaybackBatch(recordedBatch)"
  "IsStructurallyValidSimulationCommand(record.command)")
  string(FIND "${weapon_configuration_runtime_executable}"
    "${required_playback_origin_marker}"
    required_playback_origin_marker_index)
  if(required_playback_origin_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Runtime playback origin lost ${required_playback_origin_marker}")
  endif()
endforeach()
string(FIND "${weapon_configuration_stream_executable}"
  "stageRecordedPlaybackBatch(const std::vector<Entry>& recordedBatch)"
  constrained_playback_batch_index)
if(constrained_playback_batch_index EQUAL -1)
  message(FATAL_ERROR
    "CommandStream playback publication is no longer derived from one recorded batch")
endif()

foreach(playback_origin_consumer IN ITEMS
  "${SOURCE_ROOT}/tests/ja2_runtime_adapter_tests.cpp"
  "${SOURCE_ROOT}/tests/sdk_consumer/main.cpp")
  read_cxx_executable("${playback_origin_consumer}"
    playback_origin_consumer_executable)
  foreach(playback_origin_consumer_marker IN ITEMS
    "SimulationCommandSource::Replay"
    "commandJournal().snapshot()")
    string(FIND "${playback_origin_consumer_executable}"
      "${playback_origin_consumer_marker}"
      playback_origin_consumer_marker_index)
    if(playback_origin_consumer_marker_index EQUAL -1)
      message(FATAL_ERROR
        "Replay consumer lost ${playback_origin_consumer_marker}: ${playback_origin_consumer}")
    endif()
  endforeach()
endforeach()

foreach(weapon_configuration_document IN ITEMS
  "${SOURCE_ROOT}/docs/ENGINE_ARCHITECTURE.md"
  "${SOURCE_ROOT}/docs/ENGINE_SDK.md")
  file(READ "${weapon_configuration_document}"
    weapon_configuration_document_contents)
  string(FIND "${weapon_configuration_document_contents}"
    "ApplyWeaponConfigurationCommand"
    weapon_configuration_document_index)
  if(weapon_configuration_document_index EQUAL -1)
    message(FATAL_ERROR
      "Weapon-configuration command is undocumented in ${weapon_configuration_document}")
  endif()
endforeach()

# Player, AI, and moving-path obstacle traversal share one deterministic value
# command. Only the compatibility executor may enter the low-level traversal
# domain; comments cannot satisfy or bypass these call-site checks.
set(player_traversal_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/Real Time Input.cpp"
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Panels.cpp")
foreach(player_traversal_file IN LISTS player_traversal_files)
  read_cxx_executable("${player_traversal_file}" contents)
  string(REGEX MATCH
    "BeginSoldierClimb(UpRoof|DownRoof|Fence|Wall|Window)[ \t\r\n]*\\("
    direct_player_traversal_call "${contents}")
  if(direct_player_traversal_call)
    message(FATAL_ERROR
      "Player tactical traversal bypasses SimulationCommand in ${player_traversal_file}")
  endif()
endforeach()

file(GLOB traversal_command_producer_candidates
  "${SOURCE_ROOT}/Tactical/*.cpp"
  "${SOURCE_ROOT}/TacticalAI/*.cpp")
foreach(traversal_command_producer IN LISTS
    traversal_command_producer_candidates)
  if(traversal_command_producer STREQUAL
      "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp" OR
     traversal_command_producer STREQUAL
      "${SOURCE_ROOT}/Tactical/TacticalActorTraversal.cpp")
    continue()
  endif()
  read_cxx_executable("${traversal_command_producer}"
    traversal_command_producer_executable)
  string(REGEX MATCH
    "TacticalActorTraversal::[ \t\r\n]*begin(RoofClimb|RoofDescent|FenceJump|WallClimb|WindowJump)[ \t\r\n]*\\("
    direct_traversal_domain_call
    "${traversal_command_producer_executable}")
  if(direct_traversal_domain_call)
    message(FATAL_ERROR
      "Tactical traversal bypasses TraverseObstacleCommand in ${traversal_command_producer}")
  endif()
endforeach()

read_cxx_executable("${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  traversal_ai_executable)
string(REGEX MATCHALL
  "TryDispatchSystemAiTraverseObstacleCommandNow[ \t\r\n]*\\("
  traversal_ai_gateways "${traversal_ai_executable}")
list(LENGTH traversal_ai_gateways traversal_ai_gateway_count)
if(NOT traversal_ai_gateway_count EQUAL 2)
  message(FATAL_ERROR
    "AI roof/window traversal must retain exactly two System command gateways")
endif()

read_cxx_executable("${SOURCE_ROOT}/Tactical/Overhead.cpp"
  traversal_path_executable)
string(REGEX MATCHALL
  "TryDispatchSystemPathTraverseObstacleCommandNow[ \t\r\n]*\\("
  traversal_path_gateways "${traversal_path_executable}")
list(LENGTH traversal_path_gateways traversal_path_gateway_count)
if(NOT traversal_path_gateway_count EQUAL 1)
  message(FATAL_ERROR
    "Fence path completion must retain exactly one System command gateway")
endif()
foreach(required_traversal_path_marker IN ITEMS
    "IsReplaySimulationCommandExecutionActive"
    "HasPendingReplayPathTraversalCommand")
  string(FIND "${traversal_path_executable}"
    "${required_traversal_path_marker}"
    required_traversal_path_marker_index)
  if(required_traversal_path_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Fence path replay suppression lost '${required_traversal_path_marker}'")
  endif()
endforeach()
string(FIND "${traversal_path_executable}"
  "HasPendingReplayPathTraversalCommand"
  traversal_path_replay_owner_index)
string(FIND "${traversal_path_executable}"
  "EnoughPoints( pSoldier, sAPCost, sBPCost, FALSE )"
  traversal_path_action_point_branch_index)
if(traversal_path_action_point_branch_index EQUAL -1 OR
   traversal_path_replay_owner_index GREATER
     traversal_path_action_point_branch_index)
  message(FATAL_ERROR
    "Queued Replay must own the fence path before either action-point branch")
endif()
string(FIND "${traversal_ai_executable}"
  "consumeRoofCompletionReplication"
  traversal_roof_completion_policy_index)
if(traversal_roof_completion_policy_index EQUAL -1)
  message(FATAL_ERROR
    "Deferred roof completion lost its captured replay replication policy")
endif()

read_cxx_executable("${SOURCE_ROOT}/Engine/Adapters/JA2/SimulationCommand.h"
  traversal_command_value_contents)
foreach(required_traversal_value IN ITEMS
    "TacticalTraversalOrigin"
    "TacticalTraversalContinuation"
    "expectedGrid"
    "expectedFinalDestination"
    "expectedAnimationState"
    "movementAnimationState"
    "expectedPathIndex"
    "expectedPathSize"
    "expectedPathDirection"
    "expectedNextPathDirection"
    "expectedStateFingerprint"
    "expectedActionPointCost"
    "expectedBreathPointCost"
    "ShouldReplicateTraversalContinuation")
  string(FIND "${traversal_command_value_contents}"
    "${required_traversal_value}" required_traversal_value_index)
  if(required_traversal_value_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal command lost '${required_traversal_value}'")
  endif()
endforeach()

read_cxx_executable(
  "${SOURCE_ROOT}/Engine/Adapters/JA2/SimulationCommandCodec.cpp"
  traversal_codec_executable)
foreach(required_traversal_codec IN ITEMS
    "value.origin"
    "value.continuation"
    "value.expectedGrid"
    "value.expectedPathIndex"
    "value.expectedNextPathDirection"
    "value.expectedStateFingerprint"
    "value.expectedActionPointCost"
    "value.expectedBreathPointCost")
  string(FIND "${traversal_codec_executable}"
    "${required_traversal_codec}" required_traversal_codec_index)
  if(required_traversal_codec_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal replay codec lost '${required_traversal_codec}'")
  endif()
endforeach()

read_cxx_executable("${SOURCE_ROOT}/Tactical/Simulation Commands.cpp"
  traversal_executor_executable)
foreach(required_traversal_executor IN ITEMS
    "TraversalExpectationMatches"
    "ExecutePathCompletionTraversal"
    "HaltGuyFromNewGridNoBecauseOfNoAPs"
    "HandleNextTile"
    "settleIntoStationaryStance"
    "continueAfterStance(2)"
    "CaptureTraversalStateFingerprint"
    "setRoofCompletionReplication"
    "IsReplaySimulationCommandExecutionActive"
    "HasPendingReplayPathTraversalCommand")
  string(FIND "${traversal_executor_executable}"
    "${required_traversal_executor}" required_traversal_executor_index)
  if(required_traversal_executor_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal executor lost '${required_traversal_executor}'")
  endif()
endforeach()

read_cxx_executable("${SOURCE_ROOT}/TacticalAI/Movement.cpp"
  traversal_halt_executable)
read_cxx_executable(
  "${SOURCE_ROOT}/Tactical/TacticalActorRouteExecution.cpp"
  traversal_route_execution_executable)
read_cxx_executable("${SOURCE_ROOT}/Tactical/TeamTurns.cpp"
  traversal_turn_completion_executable)
foreach(required_local_replay_marker IN ITEMS
    "fReplicateStop && is_networked"
    "replicate && is_networked")
  if(required_local_replay_marker STREQUAL
      "fReplicateStop && is_networked")
    set(local_replay_contents "${traversal_halt_executable}")
  else()
    set(local_replay_contents "${traversal_route_execution_executable}")
  endif()
  string(FIND "${local_replay_contents}"
    "${required_local_replay_marker}" local_replay_marker_index)
  if(local_replay_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal Replay can reflect stop traffic: missing '${required_local_replay_marker}'")
  endif()
endforeach()
foreach(required_replay_interrupt_marker IN ITEMS
    "fReplicateInterrupt"
    "replicateInterrupt"
    "IsReplaySimulationCommandExecutionActive"
    "EndInterrupt( FALSE, fReplicateInterrupt )")
  string(FIND "${traversal_turn_completion_executable}"
    "${required_replay_interrupt_marker}"
    required_replay_interrupt_marker_index)
  if(required_replay_interrupt_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal Replay can reflect interrupt traffic: missing '${required_replay_interrupt_marker}'")
  endif()
endforeach()
foreach(required_ai_completion_policy IN ITEMS
    "EndAIGuysTurn( pSoldier, fReplicateStop )"
    "EndAIGuysTurn(pSoldier, fReplicateStop)")
  if(required_ai_completion_policy STREQUAL
      "EndAIGuysTurn( pSoldier, fReplicateStop )")
    set(ai_completion_policy_contents "${traversal_ai_executable}")
  else()
    set(ai_completion_policy_contents "${traversal_halt_executable}")
  endif()
  string(FIND "${ai_completion_policy_contents}"
    "${required_ai_completion_policy}"
    required_ai_completion_policy_index)
  if(required_ai_completion_policy_index EQUAL -1)
    message(FATAL_ERROR
      "AI completion lost the captured Replay interrupt policy")
  endif()
endforeach()

foreach(traversal_test_manifest IN ITEMS
    "${SOURCE_ROOT}/tests/CMakeLists.txt"
    "${SOURCE_ROOT}/.github/workflows/build_unix.yml")
  file(READ "${traversal_test_manifest}"
    traversal_test_manifest_contents)
  string(REGEX REPLACE "#[^\r\n]*" ""
    traversal_test_manifest_contents
    "${traversal_test_manifest_contents}")
  string(FIND "${traversal_test_manifest_contents}"
    "simulation_command_traversal_model_tests"
    traversal_test_manifest_index)
  if(traversal_test_manifest_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal model target is missing from ${traversal_test_manifest}")
  endif()
endforeach()

read_cxx_executable(
  "${SOURCE_ROOT}/tests/simulation_command_traversal_model_tests.cpp"
  traversal_model_test_executable)
foreach(required_traversal_model_marker IN ITEMS
    "const std::vector<std::uint8_t> expectedWire"
    "encoded == expectedWire"
    "SameCommand"
    "stageRecordedPlaybackBatch"
    "ShouldReplicateTraversalContinuation"
    "roofAiMissingState"
    "roofAiMissingCost"
    "roofAiAtWrongLevel"
    "reference.snapshot() == referenceState")
  string(FIND "${traversal_model_test_executable}"
    "${required_traversal_model_marker}"
    required_traversal_model_marker_index)
  if(required_traversal_model_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal model lost '${required_traversal_model_marker}'")
  endif()
endforeach()

read_cxx_executable("${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  traversal_headless_test_executable)
foreach(required_traversal_headless_marker IN ITEMS
    "liveAiTraversalContinuationOwned"
    "livePathTraversalOrdered"
    "pendingReplaySuppressesAsyncPathProducer"
    "pendingReplayOwnsNoActionPointBranch"
    "retainedWindowRejectedChangedRoute"
    "staleAiActionPreserved"
    "retainedTraversalRejectedStaleState"
    "retainedRoofRejectedChangedPointBudget")
  string(FIND "${traversal_headless_test_executable}"
    "${required_traversal_headless_marker}"
    required_traversal_headless_marker_index)
  if(required_traversal_headless_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Traversal headless coverage lost '${required_traversal_headless_marker}'")
  endif()
endforeach()

foreach(traversal_document IN ITEMS
    "${SOURCE_ROOT}/docs/ENGINE_ARCHITECTURE.md"
    "${SOURCE_ROOT}/docs/ENGINE_SDK.md")
  file(READ "${traversal_document}" traversal_document_contents)
  foreach(traversal_document_marker IN ITEMS
      "TraverseObstacleCommand"
      "blocked-tile waiting"
      "Replay")
    string(FIND "${traversal_document_contents}"
      "${traversal_document_marker}" traversal_document_marker_index)
    if(traversal_document_marker_index EQUAL -1)
      message(FATAL_ERROR
        "Traversal command contract lost '${traversal_document_marker}' in ${traversal_document}")
    endif()
  endforeach()
endforeach()

# Mouse-driven and automatic door/switch/openable-structure intent use public
# pointer-free commands. Only the compatibility executor may call the legacy
# begin/interaction pair; animation keyframes stay internal mechanics.
file(READ "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  player_world_object_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(StartInteractiveObject|InteractWithInteractiveObject)[ \t\r\n]*\\("
  direct_player_world_object_call "${player_world_object_contents}")
if(direct_player_world_object_call)
  message(FATAL_ERROR
    "Player world-object interaction bypasses SimulationCommand in Tactical/Handle UI.cpp")
endif()

set(system_world_object_producers
  "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  "${SOURCE_ROOT}/Tactical/Interface Dialogue.cpp")
foreach(system_world_object_producer IN LISTS system_world_object_producers)
  read_cxx_executable("${system_world_object_producer}"
    system_world_object_producer_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(StartInteractiveObject|InteractWithInteractiveObject)[ \t\r\n]*\\("
    direct_system_world_object_call
    "${system_world_object_producer_contents}")
  if(direct_system_world_object_call)
    message(FATAL_ERROR
      "Automatic world-object interaction bypasses SimulationCommand in ${system_world_object_producer}")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Simulation Commands.cpp"
  system_world_object_executor_contents)
foreach(required_system_world_object_executor_marker IN ITEMS
    "TryDispatchSystemAiWorldObjectInteractionCommandNow"
    "TryDispatchSystemPathWorldObjectInteractionCommandNow"
    "TryDispatchSystemPendingWorldObjectInteractionCommandNow"
    "TryDispatchSystemDialogueWorldObjectInteractionCommandNow"
    "CaptureWorldObjectActorStateFingerprint"
    "CaptureWorldObjectFingerprint"
    "HasPendingReplayWorldObjectInteractionCommand"
    "ShouldReplicateWorldObjectCompletion")
  string(FIND "${system_world_object_executor_contents}"
    "${required_system_world_object_executor_marker}"
    required_system_world_object_executor_marker_index)
  if(required_system_world_object_executor_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Automatic world-object command lost '${required_system_world_object_executor_marker}'")
  endif()
endforeach()

foreach(system_world_object_test_manifest IN ITEMS
    "${SOURCE_ROOT}/tests/CMakeLists.txt"
    "${SOURCE_ROOT}/.github/workflows/build_unix.yml")
  file(READ "${system_world_object_test_manifest}"
    system_world_object_test_manifest_contents)
  string(FIND "${system_world_object_test_manifest_contents}"
    "simulation_command_world_object_model_tests"
    system_world_object_test_manifest_index)
  if(system_world_object_test_manifest_index EQUAL -1)
    message(FATAL_ERROR
      "Automatic world-object model target is missing from ${system_world_object_test_manifest}")
  endif()
endforeach()

read_cxx_executable(
  "${SOURCE_ROOT}/tests/simulation_command_world_object_model_tests.cpp"
  system_world_object_model_executable)
foreach(required_system_world_object_model_marker IN ITEMS
    "const std::vector<std::uint8_t> expectedWire"
    "encoded == expectedWire"
    "SameCommand"
    "legalCommands.size() == 28"
    "sentinel[0].tick == decoded[0].tick"
    "stageRecordedPlaybackBatch"
    "ShouldReplicateWorldObjectCompletion"
    "reference.snapshot() == referenceState")
  string(FIND "${system_world_object_model_executable}"
    "${required_system_world_object_model_marker}"
    required_system_world_object_model_marker_index)
  if(required_system_world_object_model_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Automatic world-object model lost '${required_system_world_object_model_marker}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  system_world_object_headless_executable)
foreach(required_system_world_object_headless_marker IN ITEMS
    "retainedDoorRejectedChangedStructure"
    "expectedObjectFingerprint"
    "pendingReplaySuppressesAutomaticDoorProducer"
    "replayDoorCompletionLocalOnly"
    "localActorCompletionStillReleasesOwner"
    "replayPathContinuationLocalOnly"
    "staleReplayDoorCompletionLocalOnly"
    "unownedDoorCompletionKeepsLegacyReplication"
    "reused actor slots do not inherit")
  string(FIND "${system_world_object_headless_executable}"
    "${required_system_world_object_headless_marker}"
    required_system_world_object_headless_marker_index)
  if(required_system_world_object_headless_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Automatic world-object headless coverage lost '${required_system_world_object_headless_marker}'")
  endif()
endforeach()

set(system_world_object_lifecycle_files
    "${SOURCE_ROOT}/Tactical/Handle Doors.cpp"
    "${SOURCE_ROOT}/Tactical/Interface.cpp"
    "${SOURCE_ROOT}/Tactical/TacticalActorRouteExecution.cpp"
    "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
    "${SOURCE_ROOT}/Tactical/Soldier Ani.cpp"
    "${SOURCE_ROOT}/Tactical/TacticalActorAiBehavior.cpp"
    "${SOURCE_ROOT}/TileEngine/Interactive Tiles.cpp")
set(system_world_object_lifecycle_markers
    "completeDoorChange"
    "worldObject.reset()"
    "worldObject.reset()"
    "consumeActorActionCompletionReplication(fReplicateStop)"
    "consumePathContinuationReplication"
    "worldObject.reset()"
    "worldObject.reset()")
list(LENGTH system_world_object_lifecycle_files
  system_world_object_lifecycle_count)
math(EXPR system_world_object_lifecycle_last
  "${system_world_object_lifecycle_count} - 1")
foreach(system_world_object_lifecycle_index RANGE
    ${system_world_object_lifecycle_last})
  list(GET system_world_object_lifecycle_files
    ${system_world_object_lifecycle_index}
    system_world_object_lifecycle_file)
  list(GET system_world_object_lifecycle_markers
    ${system_world_object_lifecycle_index}
    system_world_object_lifecycle_marker)
  file(READ "${system_world_object_lifecycle_file}"
    system_world_object_lifecycle_contents)
  string(FIND "${system_world_object_lifecycle_contents}"
    "${system_world_object_lifecycle_marker}"
    system_world_object_lifecycle_reset_index)
  if(system_world_object_lifecycle_reset_index EQUAL -1)
    message(FATAL_ERROR
      "Automatic world-object lifecycle marker '${system_world_object_lifecycle_marker}' is missing from ${system_world_object_lifecycle_file}")
  endif()
endforeach()

foreach(system_world_object_document IN ITEMS
    "${SOURCE_ROOT}/docs/ENGINE_ARCHITECTURE.md"
    "${SOURCE_ROOT}/docs/ENGINE_SDK.md")
  file(READ "${system_world_object_document}"
    system_world_object_document_contents)
  foreach(system_world_object_document_marker IN ITEMS
      "SystemWorldObjectInteractionCommand"
      "Replay")
    string(FIND "${system_world_object_document_contents}"
      "${system_world_object_document_marker}"
      system_world_object_document_marker_index)
    if(system_world_object_document_marker_index EQUAL -1)
      message(FATAL_ERROR
        "Automatic world-object contract lost '${system_world_object_document_marker}' in ${system_world_object_document}")
    endif()
  endforeach()
endforeach()

# The legacy dialogue queue exposes every bit of its 32-bit special-event
# field. One value has two campaign-specific names, hence 33 symbols. Keep the
# complete inventory explicit before migrating behaviorally closed slices.
file(READ "${SOURCE_ROOT}/Tactical/Dialogue Control.h"
  dialogue_effect_header_contents)
string(REGEX MATCHALL
  "#[ \t]*define[ \t]+(DIALOGUE_SPECIAL_EVENT_[A-Za-z0-9_]+|DIALOGUE_ADD_EVENT_FOR_SOLDIER_UPDATE_BOX)([ \t\r\n]|$)"
  dialogue_effect_exposed_definitions "${dialogue_effect_header_contents}")
list(LENGTH dialogue_effect_exposed_definitions
  dialogue_effect_exposed_definition_count)
if(NOT dialogue_effect_exposed_definition_count EQUAL 33)
  message(FATAL_ERROR
    "Dialogue-effect header must expose exactly 33 special-event symbols; found ${dialogue_effect_exposed_definition_count}")
endif()
set(dialogue_effect_symbol_values
  "DIALOGUE_SPECIAL_EVENT_GIVE_ITEM|0x00000001"
  "DIALOGUE_SPECIAL_EVENT_TRIGGER_NPC|0x00000002"
  "DIALOGUE_SPECIAL_EVENT_GOTO_GRIDNO|0x00000004"
  "DIALOGUE_SPECIAL_EVENT_DO_ACTION|0x00000008"
  "DIALOGUE_SPECIAL_EVENT_CLOSE_PANEL|0x00000010"
  "DIALOGUE_SPECIAL_EVENT_PCTRIGGERNPC|0x00000020"
  "DIALOGUE_SPECIAL_EVENT_BEGINPREBATTLEINTERFACE|0x00000040"
  "DIALOGUE_SPECIAL_EVENT_SKYRIDERMAPSCREENEVENT|0x00000080"
  "DIALOGUE_SPECIAL_EVENT_SHOW_CONTRACT_MENU|0x00000100"
  "DIALOGUE_SPECIAL_EVENT_MINESECTOREVENT|0x00000200"
  "DIALOGUE_SPECIAL_EVENT_SHOW_UPDATE_MENU|0x00000400"
  "DIALOGUE_SPECIAL_EVENT_ENABLE_AI|0x00000800"
  "DIALOGUE_SPECIAL_EVENT_USE_ALTERNATE_FILES|0x00001000"
  "DIALOGUE_SPECIAL_EVENT_JERRY_MILO|0x00002000"
  "DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA|0x00002000"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING|0x00004000"
  "DIALOGUE_SPECIAL_EVENT_MULTIPURPOSE|0x00008000"
  "DIALOGUE_SPECIAL_EVENT_SLEEP|0x00010000"
  "DIALOGUE_SPECIAL_EVENT_DO_BATTLE_SND|0x00020000"
  "DIALOGUE_SPECIAL_EVENT_SIGNAL_ITEM_LOCATOR_START|0x00040000"
  "DIALOGUE_SPECIAL_EVENT_SHOPKEEPER|0x00080000"
  "DIALOGUE_SPECIAL_EVENT_SKIP_A_FRAME|0x00100000"
  "DIALOGUE_SPECIAL_EVENT_EXIT_MAP_SCREEN|0x00200000"
  "DIALOGUE_SPECIAL_EVENT_DISPLAY_STAT_CHANGE|0x00400000"
  "DIALOGUE_SPECIAL_EVENT_UNSET_ARRIVES_FLAG|0x00800000"
  "DIALOGUE_SPECIAL_EVENT_TRIGGERPREBATTLEINTERFACE|0x01000000"
  "DIALOGUE_ADD_EVENT_FOR_SOLDIER_UPDATE_BOX|0x02000000"
  "DIALOGUE_SPECIAL_EVENT_ENTER_MAPSCREEN|0x04000000"
  "DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE|0x08000000"
  "DIALOGUE_SPECIAL_EVENT_REMOVE_EPC|0x10000000"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_WANTS_TO_RENEW|0x20000000"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_NOGO_TO_RENEW|0x40000000"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING_NO_ASK_EQUIP|0x80000000")
list(LENGTH dialogue_effect_symbol_values dialogue_effect_symbol_count)
if(NOT dialogue_effect_symbol_count EQUAL 33)
  message(FATAL_ERROR
    "Dialogue-effect audit must enumerate exactly 33 exposed symbols")
endif()
set(dialogue_effect_bit_values)
set(dialogue_effect_consumer_symbols)
foreach(dialogue_effect_symbol_value IN LISTS dialogue_effect_symbol_values)
  string(REPLACE "|" ";" dialogue_effect_pair
    "${dialogue_effect_symbol_value}")
  list(GET dialogue_effect_pair 0 dialogue_effect_symbol)
  list(GET dialogue_effect_pair 1 dialogue_effect_value)
  list(APPEND dialogue_effect_bit_values "${dialogue_effect_value}")
  list(APPEND dialogue_effect_consumer_symbols "${dialogue_effect_symbol}")
  string(REGEX MATCH
    "#[ \t]*define[ \t]+${dialogue_effect_symbol}[ \t]+${dialogue_effect_value}([ \t\r\n]|$)"
    dialogue_effect_exact_definition "${dialogue_effect_header_contents}")
  if(NOT dialogue_effect_exact_definition)
    message(FATAL_ERROR
      "Dialogue-effect inventory lost ${dialogue_effect_symbol}=${dialogue_effect_value}")
  endif()
endforeach()
list(REMOVE_DUPLICATES dialogue_effect_bit_values)
list(REMOVE_ITEM dialogue_effect_consumer_symbols
  "DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA")
list(LENGTH dialogue_effect_bit_values dialogue_effect_bit_count)
if(NOT dialogue_effect_bit_count EQUAL 32)
  message(FATAL_ERROR
    "Dialogue-effect vocabulary no longer covers exactly 32 bit values")
endif()

set(dialogue_effect_producer_inventory
  "DIALOGUE_SPECIAL_EVENT_GIVE_ITEM|1|Tactical/Interface Dialogue.cpp"
  "DIALOGUE_SPECIAL_EVENT_TRIGGER_NPC|1|Tactical/Interface Dialogue.cpp"
  "DIALOGUE_SPECIAL_EVENT_GOTO_GRIDNO|1|Tactical/Interface Dialogue.cpp"
  "DIALOGUE_SPECIAL_EVENT_DO_ACTION|1|Tactical/Interface Dialogue.cpp"
  "DIALOGUE_SPECIAL_EVENT_CLOSE_PANEL|1|Tactical/Interface Dialogue.cpp"
  "DIALOGUE_SPECIAL_EVENT_PCTRIGGERNPC|1|TacticalAI/NPC.cpp"
  "DIALOGUE_SPECIAL_EVENT_BEGINPREBATTLEINTERFACE|1|Strategic/Strategic Movement.cpp"
  "DIALOGUE_SPECIAL_EVENT_SKYRIDERMAPSCREENEVENT|1|Strategic/Map Screen Helicopter.cpp"
  "DIALOGUE_SPECIAL_EVENT_SHOW_CONTRACT_MENU|2|Strategic/Map Screen Interface.cpp|Strategic/Merc Contract.cpp"
  "DIALOGUE_SPECIAL_EVENT_MINESECTOREVENT|2|Strategic/Map Screen Interface.cpp|Tactical/Merc Hiring.cpp"
  "DIALOGUE_SPECIAL_EVENT_SHOW_UPDATE_MENU|2|Strategic/Assignments.cpp|Strategic/Merc Contract.cpp"
  "DIALOGUE_SPECIAL_EVENT_ENABLE_AI|1|Tactical/Merc Entering.cpp"
  "DIALOGUE_SPECIAL_EVENT_USE_ALTERNATE_FILES|2|Strategic/Quest Debug System.cpp|Tactical/Tactical Turns.cpp"
  "DIALOGUE_SPECIAL_EVENT_JERRY_MILO|2|Strategic/MapScreen Quotes.cpp|Tactical/Ja25_Tactical.cpp"
  "DIALOGUE_SPECIAL_EVENT_CONTINUE_TRAINING_MILITIA|1|Strategic/Town Militia.cpp"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING|3|Strategic/Assignments.cpp|Strategic/Merc Contract.cpp|Strategic/Strategic Merc Handler.cpp"
  "DIALOGUE_SPECIAL_EVENT_MULTIPURPOSE|3|Tactical/Dialogue Control.cpp|Tactical/End Game.cpp|Tactical/Overhead.cpp"
  "DIALOGUE_SPECIAL_EVENT_SLEEP|4|Strategic/Assignments.cpp|Strategic/Strategic Movement.cpp|Tactical/Dialogue Control.cpp|Tactical/Morale.cpp"
  "DIALOGUE_SPECIAL_EVENT_DO_BATTLE_SND|3|Tactical/TacticalActorBattleSounds.cpp|Tactical/TacticalActorRadio.cpp|TileEngine/Interactive Tiles.cpp"
  "DIALOGUE_SPECIAL_EVENT_SIGNAL_ITEM_LOCATOR_START|2|Tactical/Overhead.cpp|Tactical/fov.cpp"
  "DIALOGUE_SPECIAL_EVENT_SHOPKEEPER|1|Tactical/ShopKeeper Interface.cpp"
  "DIALOGUE_SPECIAL_EVENT_SKIP_A_FRAME|1|Tactical/ShopKeeper Interface.cpp"
  "DIALOGUE_SPECIAL_EVENT_EXIT_MAP_SCREEN|1|Tactical/Air Raid.cpp"
  "DIALOGUE_SPECIAL_EVENT_DISPLAY_STAT_CHANGE|1|Tactical/Campaign.cpp"
  "DIALOGUE_SPECIAL_EVENT_UNSET_ARRIVES_FLAG|1|Tactical/Merc Hiring.cpp"
  "DIALOGUE_SPECIAL_EVENT_TRIGGERPREBATTLEINTERFACE|1|Strategic/Strategic Movement.cpp"
  "DIALOGUE_ADD_EVENT_FOR_SOLDIER_UPDATE_BOX|1|Strategic/Map Screen Interface.cpp"
  "DIALOGUE_SPECIAL_EVENT_ENTER_MAPSCREEN|3|Strategic/Map Screen Interface.cpp|Strategic/Merc Contract.cpp|Strategic/Strategic Merc Handler.cpp"
  "DIALOGUE_SPECIAL_EVENT_LOCK_INTERFACE|2|Strategic/Assignments.cpp|Strategic/Merc Contract.cpp"
  "DIALOGUE_SPECIAL_EVENT_REMOVE_EPC|1|Strategic/Assignments.cpp"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_WANTS_TO_RENEW|1|Strategic/Merc Contract.cpp"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_NOGO_TO_RENEW|1|Strategic/Merc Contract.cpp"
  "DIALOGUE_SPECIAL_EVENT_CONTRACT_ENDING_NO_ASK_EQUIP|3|Strategic/Hourly Update.cpp|Strategic/Strategic Merc Handler.cpp|Strategic/strategicmap.cpp")
list(LENGTH dialogue_effect_producer_inventory
  dialogue_effect_producer_inventory_count)
if(NOT dialogue_effect_producer_inventory_count EQUAL 33)
  message(FATAL_ERROR
    "Dialogue-effect source inventory must enumerate exactly 33 symbols")
endif()
foreach(dialogue_effect_producer_entry IN LISTS
    dialogue_effect_producer_inventory)
  string(REPLACE "|" ";" dialogue_effect_producer_parts
    "${dialogue_effect_producer_entry}")
  list(GET dialogue_effect_producer_parts 0 dialogue_effect_symbol)
  set("dialogue_effect_actual_${dialogue_effect_symbol}")
endforeach()
# A bare symbol reference is not producer evidence. Require the symbol to be an
# argument of one of the legacy queue-enqueue APIs, up to that call's statement
# terminator. This also lets Dialogue Control.cpp participate without mistaking
# its centralized flag tests for producers.
set(dialogue_effect_producer_call_pattern
  "(TacticalCharacterDialogueWithSpecialEventEx|TacticalCharacterDialogueWithSpecialEvent|CharacterDialogueWithSpecialEventEx|CharacterDialogueWithSpecialEvent|SpecialCharacterDialogueEventWithExtraParam|SpecialCharacterDialogueEvent|SnitchCharacterDialogue)")
file(GLOB dialogue_effect_source_candidates
  "${SOURCE_ROOT}/Strategic/*.cpp"
  "${SOURCE_ROOT}/Tactical/*.cpp"
  "${SOURCE_ROOT}/TacticalAI/*.cpp"
  "${SOURCE_ROOT}/TileEngine/*.cpp")
foreach(dialogue_effect_source_candidate IN LISTS
    dialogue_effect_source_candidates)
  read_cxx_executable("${dialogue_effect_source_candidate}"
    dialogue_effect_source_executable)
  file(RELATIVE_PATH dialogue_effect_source_relative
    "${SOURCE_ROOT}" "${dialogue_effect_source_candidate}")
  foreach(dialogue_effect_producer_entry IN LISTS
      dialogue_effect_producer_inventory)
    string(REPLACE "|" ";" dialogue_effect_producer_parts
      "${dialogue_effect_producer_entry}")
    list(GET dialogue_effect_producer_parts 0 dialogue_effect_symbol)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${dialogue_effect_producer_call_pattern}[ \t\r\n]*\\([^;{}]*[^A-Za-z0-9_]${dialogue_effect_symbol}([^A-Za-z0-9_]|$)"
      dialogue_effect_source_producer
      "${dialogue_effect_source_executable}")
    if(dialogue_effect_source_producer)
      list(APPEND "dialogue_effect_actual_${dialogue_effect_symbol}"
        "${dialogue_effect_source_relative}")
    endif()
  endforeach()
endforeach()
read_cxx_executable("${SOURCE_ROOT}/Tactical/Dialogue Control.cpp"
  dialogue_effect_queue_executable)
string(FIND "${dialogue_effect_queue_executable}"
  "void HandleDialogue( )" dialogue_effect_consumer_start)
string(FIND "${dialogue_effect_queue_executable}"
  "BOOLEAN DelayedTacticalCharacterDialogue("
  dialogue_effect_consumer_end)
if(dialogue_effect_consumer_start EQUAL -1 OR
   dialogue_effect_consumer_end EQUAL -1 OR
   NOT dialogue_effect_consumer_start LESS dialogue_effect_consumer_end)
  message(FATAL_ERROR
    "Dialogue-effect audit cannot isolate the centralized HandleDialogue consumer")
endif()
math(EXPR dialogue_effect_consumer_length
  "${dialogue_effect_consumer_end} - ${dialogue_effect_consumer_start}")
string(SUBSTRING "${dialogue_effect_queue_executable}"
  ${dialogue_effect_consumer_start} ${dialogue_effect_consumer_length}
  dialogue_effect_consumer_executable)
list(LENGTH dialogue_effect_consumer_symbols dialogue_effect_consumer_count)
if(NOT dialogue_effect_consumer_count EQUAL 32)
  message(FATAL_ERROR
    "Dialogue-effect audit must enumerate exactly 32 centralized bit consumers")
endif()
foreach(dialogue_effect_consumer_symbol IN LISTS
    dialogue_effect_consumer_symbols)
  string(REGEX MATCH
    "QItem[.]uiSpecialEventFlag[^\r\n]*${dialogue_effect_consumer_symbol}([^A-Za-z0-9_]|$)"
    dialogue_effect_consumer_use "${dialogue_effect_consumer_executable}")
  if(NOT dialogue_effect_consumer_use)
    message(FATAL_ERROR
      "Dialogue queue lost centralized consumer for ${dialogue_effect_consumer_symbol}")
  endif()
endforeach()
foreach(dialogue_effect_producer_entry IN LISTS
    dialogue_effect_producer_inventory)
  string(REPLACE "|" ";" dialogue_effect_producer_parts
    "${dialogue_effect_producer_entry}")
  list(GET dialogue_effect_producer_parts 0 dialogue_effect_symbol)
  list(GET dialogue_effect_producer_parts 1 dialogue_effect_expected_count)
  list(SUBLIST dialogue_effect_producer_parts 2 -1
    dialogue_effect_expected_producers)
  set(dialogue_effect_actual_variable
    "dialogue_effect_actual_${dialogue_effect_symbol}")
  set(dialogue_effect_actual_producers
    "${${dialogue_effect_actual_variable}}")
  list(SORT dialogue_effect_expected_producers)
  list(SORT dialogue_effect_actual_producers)
  list(LENGTH dialogue_effect_actual_producers
    dialogue_effect_actual_count)
  if(NOT dialogue_effect_actual_count EQUAL dialogue_effect_expected_count OR
     NOT "${dialogue_effect_actual_producers}" STREQUAL
       "${dialogue_effect_expected_producers}")
    message(FATAL_ERROR
      "Dialogue-effect producer inventory changed for ${dialogue_effect_symbol}: expected '${dialogue_effect_expected_producers}', found '${dialogue_effect_actual_producers}'")
  endif()
endforeach()

read_cxx_executable("${SOURCE_ROOT}/Strategic/LuaInitNPCs.cpp"
  dialogue_effect_lua_gateway_executable)
foreach(required_dialogue_effect_lua_marker IN ITEMS
    "lua_register(L, \"TacticalCharacterDialogueWithSpecialEvent\", l_TacticalCharacterDialogueWithSpecialEvent)"
    "UINT32 uiFlag = lua_tointeger(L, 3)"
    "TacticalCharacterDialogueWithSpecialEvent(pSoldier, usQuoteNum, uiFlag")
  string(FIND "${dialogue_effect_lua_gateway_executable}"
    "${required_dialogue_effect_lua_marker}"
    required_dialogue_effect_lua_marker_index)
  if(required_dialogue_effect_lua_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Dialogue-effect audit lost dynamic Lua ingress '${required_dialogue_effect_lua_marker}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/docs/DIALOGUE_EFFECT_INVENTORY.md"
  dialogue_effect_inventory_contents)
foreach(dialogue_effect_symbol_value IN LISTS dialogue_effect_symbol_values)
  string(REPLACE "|" ";" dialogue_effect_pair
    "${dialogue_effect_symbol_value}")
  list(GET dialogue_effect_pair 0 dialogue_effect_symbol)
  list(GET dialogue_effect_pair 1 dialogue_effect_value)
  string(REGEX MATCH
    "\\|[ \t]*`${dialogue_effect_value}`[ \t]*\\|[ \t]*`${dialogue_effect_symbol}`[ \t]*\\|[^\r\n]*"
    dialogue_effect_document_value_row
    "${dialogue_effect_inventory_contents}")
  if(NOT dialogue_effect_document_value_row)
    message(FATAL_ERROR
      "Dialogue-effect document lost exact row ${dialogue_effect_symbol}=${dialogue_effect_value}")
  endif()
endforeach()
foreach(dialogue_effect_producer_entry IN LISTS
    dialogue_effect_producer_inventory)
  string(REPLACE "|" ";" dialogue_effect_producer_parts
    "${dialogue_effect_producer_entry}")
  list(GET dialogue_effect_producer_parts 0 dialogue_effect_symbol)
  list(GET dialogue_effect_producer_parts 1 dialogue_effect_expected_count)
  list(SUBLIST dialogue_effect_producer_parts 2 -1
    dialogue_effect_expected_producers)
  string(REGEX MATCH
    "\\|[^\r\n]*`${dialogue_effect_symbol}`[^\r\n]*\\|"
    dialogue_effect_document_row "${dialogue_effect_inventory_contents}")
  if(NOT dialogue_effect_document_row)
    message(FATAL_ERROR
      "Dialogue-effect document lost row for ${dialogue_effect_symbol}")
  endif()
  string(REGEX MATCHALL "`[^`]+\\.cpp`"
    dialogue_effect_document_producers "${dialogue_effect_document_row}")
  list(LENGTH dialogue_effect_document_producers
    dialogue_effect_document_producer_count)
  if(NOT dialogue_effect_document_producer_count EQUAL
      dialogue_effect_expected_count)
    message(FATAL_ERROR
      "Dialogue-effect document has the wrong producer count for ${dialogue_effect_symbol}")
  endif()
  foreach(dialogue_effect_expected_producer IN LISTS
      dialogue_effect_expected_producers)
    string(FIND "${dialogue_effect_document_row}"
      "`${dialogue_effect_expected_producer}`"
      dialogue_effect_document_producer_index)
    if(dialogue_effect_document_producer_index EQUAL -1)
      message(FATAL_ERROR
        "Dialogue-effect document lost ${dialogue_effect_expected_producer} for ${dialogue_effect_symbol}")
    endif()
  endforeach()
endforeach()
set(dialogue_effect_audit_document_markers
  "docs/ENGINE_ARCHITECTURE.md|The non-positional dialogue-effect audit covers all 32 legacy bits and all 33"
  "docs/ENGINE_SDK.md|It records all 32 bit values, all 33 exposed"
  "docs/DIALOGUE_EFFECT_INVENTORY.md|All 32 bit values and 33 exposed names remain legacy work")
foreach(dialogue_effect_audit_document_marker IN LISTS
    dialogue_effect_audit_document_markers)
  string(REPLACE "|" ";" dialogue_effect_audit_document_parts
    "${dialogue_effect_audit_document_marker}")
  list(GET dialogue_effect_audit_document_parts 0
    dialogue_effect_audit_document_relative)
  list(GET dialogue_effect_audit_document_parts 1
    dialogue_effect_audit_document_required_marker)
  file(READ "${SOURCE_ROOT}/${dialogue_effect_audit_document_relative}"
    dialogue_effect_audit_document_contents)
  string(FIND "${dialogue_effect_audit_document_contents}"
    "${dialogue_effect_audit_document_required_marker}"
    dialogue_effect_audit_document_marker_index)
  if(dialogue_effect_audit_document_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Dialogue-effect audit lost '${dialogue_effect_audit_document_required_marker}' in ${dialogue_effect_audit_document_relative}")
  endif()
endforeach()
foreach(dialogue_effect_debt_marker IN ITEMS
    "All 32 bit values and 33 exposed names remain legacy work"
    "live `HandleFirstHeliDropOfGame`"
    "dead `BeginMercEntering`"
    "C++ declaration or call site"
    "complete pause-state mutation token"
    "continuation for the remaining same-item effects"
    "command-ownership, provenance"
    "codec, or replay claim")
  string(FIND "${dialogue_effect_inventory_contents}"
    "${dialogue_effect_debt_marker}"
    dialogue_effect_debt_marker_index)
  if(dialogue_effect_debt_marker_index EQUAL -1)
    message(FATAL_ERROR
      "Dialogue-effect scope honesty lost '${dialogue_effect_debt_marker}'")
  endif()
endforeach()

# ENABLE_AI's sole named C++ producer is in the live helicopter/airdrop
# completion helper. The nearby manual helicopter pause is instead trapped in
# the undeclared, uncalled BeginMercEntering body, so the live producer cannot
# claim ownership of that pause. Keep both sides of that provenance explicit.
read_cxx_executable("${SOURCE_ROOT}/Tactical/Merc Entering.cpp"
  dialogue_enable_ai_producer_executable)
string(FIND "${dialogue_enable_ai_producer_executable}"
  "void BeginMercEntering(" dialogue_begin_merc_entering_start)
string(FIND "${dialogue_enable_ai_producer_executable}"
  "void HandleFirstHeliDropOfGame( )"
  dialogue_handle_first_heli_drop_start REVERSE)
string(FIND "${dialogue_enable_ai_producer_executable}"
  "SoldierID SpawnAirDropElite(" dialogue_handle_first_heli_drop_end)
if(dialogue_begin_merc_entering_start EQUAL -1 OR
   dialogue_handle_first_heli_drop_start EQUAL -1 OR
   dialogue_handle_first_heli_drop_end EQUAL -1 OR
   NOT dialogue_begin_merc_entering_start LESS
     dialogue_handle_first_heli_drop_start OR
   NOT dialogue_handle_first_heli_drop_start LESS
     dialogue_handle_first_heli_drop_end)
  message(FATAL_ERROR
    "Dialogue-effect audit cannot isolate helicopter ENABLE_AI ownership")
endif()
math(EXPR dialogue_begin_merc_entering_length
  "${dialogue_handle_first_heli_drop_start} - ${dialogue_begin_merc_entering_start}")
string(SUBSTRING "${dialogue_enable_ai_producer_executable}"
  ${dialogue_begin_merc_entering_start}
  ${dialogue_begin_merc_entering_length}
  dialogue_begin_merc_entering_executable)
math(EXPR dialogue_handle_first_heli_drop_length
  "${dialogue_handle_first_heli_drop_end} - ${dialogue_handle_first_heli_drop_start}")
string(SUBSTRING "${dialogue_enable_ai_producer_executable}"
  ${dialogue_handle_first_heli_drop_start}
  ${dialogue_handle_first_heli_drop_length}
  dialogue_handle_first_heli_drop_executable)
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])DIALOGUE_SPECIAL_EVENT_ENABLE_AI([^A-Za-z0-9_]|$)"
  dialogue_handle_first_heli_drop_enable_ai_uses
  "${dialogue_handle_first_heli_drop_executable}")
list(LENGTH dialogue_handle_first_heli_drop_enable_ai_uses
  dialogue_handle_first_heli_drop_enable_ai_use_count)
if(NOT dialogue_handle_first_heli_drop_enable_ai_use_count EQUAL 1)
  message(FATAL_ERROR
    "Dialogue-effect audit assumption changed: HandleFirstHeliDropOfGame must own the sole named ENABLE_AI enqueue")
endif()
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])PauseAIUntilManuallyUnpaused[ \t\r\n]*\\("
  dialogue_begin_merc_entering_pause_uses
  "${dialogue_begin_merc_entering_executable}")
list(LENGTH dialogue_begin_merc_entering_pause_uses
  dialogue_begin_merc_entering_pause_use_count)
if(NOT dialogue_begin_merc_entering_pause_use_count EQUAL 1)
  message(FATAL_ERROR
    "Dialogue-effect audit assumption changed: dead BeginMercEntering must contain the sole nearby manual helicopter pause")
endif()
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])HandleFirstHeliDropOfGame([^A-Za-z0-9_]|$)"
  dialogue_handle_first_heli_drop_local_uses
  "${dialogue_enable_ai_producer_executable}")
list(LENGTH dialogue_handle_first_heli_drop_local_uses
  dialogue_handle_first_heli_drop_local_use_count)
if(NOT dialogue_handle_first_heli_drop_local_use_count EQUAL 8)
  message(FATAL_ERROR
    "Dialogue-effect audit assumption changed: HandleFirstHeliDropOfGame must have one declaration, one definition, and six live call sites")
endif()
string(REGEX MATCHALL
  "(^|[^A-Za-z0-9_])BeginMercEntering([^A-Za-z0-9_]|$)"
  dialogue_begin_merc_entering_local_uses
  "${dialogue_enable_ai_producer_executable}")
list(LENGTH dialogue_begin_merc_entering_local_uses
  dialogue_begin_merc_entering_local_use_count)
if(NOT dialogue_begin_merc_entering_local_use_count EQUAL 1)
  message(FATAL_ERROR
    "Dialogue-effect audit assumption changed: BeginMercEntering is no longer definition-only")
endif()
set(dialogue_begin_merc_entering_source_directories
  Editor
  Engine
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  i18n
  lua
  sgp)
set(dialogue_begin_merc_entering_candidates)
foreach(dialogue_begin_merc_entering_source_directory IN LISTS
    dialogue_begin_merc_entering_source_directories)
  file(GLOB_RECURSE dialogue_begin_merc_entering_directory_candidates
    "${SOURCE_ROOT}/${dialogue_begin_merc_entering_source_directory}/*.cc"
    "${SOURCE_ROOT}/${dialogue_begin_merc_entering_source_directory}/*.cpp"
    "${SOURCE_ROOT}/${dialogue_begin_merc_entering_source_directory}/*.cxx"
    "${SOURCE_ROOT}/${dialogue_begin_merc_entering_source_directory}/*.h"
    "${SOURCE_ROOT}/${dialogue_begin_merc_entering_source_directory}/*.hh"
    "${SOURCE_ROOT}/${dialogue_begin_merc_entering_source_directory}/*.hpp")
  list(APPEND dialogue_begin_merc_entering_candidates
    ${dialogue_begin_merc_entering_directory_candidates})
endforeach()
foreach(dialogue_begin_merc_entering_candidate IN LISTS
    dialogue_begin_merc_entering_candidates)
  if(dialogue_begin_merc_entering_candidate STREQUAL
      "${SOURCE_ROOT}/Tactical/Merc Entering.cpp")
    continue()
  endif()
  # This is an absence check, so raw matching is deliberately conservative:
  # even a comment or hand-written forward declaration outside the owner is
  # enough to make the definition-only audit stale. Avoid the executable-text
  # parser here; walking an in-tree build and its fetched dependencies made the
  # former unbounded scan both slow and capable of exhausting CMake.
  file(STRINGS "${dialogue_begin_merc_entering_candidate}"
    dialogue_begin_merc_entering_external_use
    LIMIT_COUNT 1
    REGEX "(^|[^A-Za-z0-9_])BeginMercEntering([^A-Za-z0-9_]|$)")
  if(dialogue_begin_merc_entering_external_use)
    message(FATAL_ERROR
      "Dialogue-effect audit assumption changed: BeginMercEntering appears outside its definition owner in ${dialogue_begin_merc_entering_candidate}")
  endif()
endforeach()

# Player conversation and vehicle-entry targets are stable tactical entity
# identities. In particular, delayed movement must not retain only a reusable
# SoldierID or a grid containing a different actor by completion time.
set(player_conversation_files
  "${SOURCE_ROOT}/Tactical/Handle UI.cpp"
  "${SOURCE_ROOT}/Tactical/ShopKeeper Interface.cpp")
foreach(player_conversation_file IN LISTS player_conversation_files)
  file(READ "${player_conversation_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(PlayerSoldierStartTalking|MERC_TALK)([^A-Za-z0-9_]|$)"
    direct_player_conversation_call "${contents}")
  if(direct_player_conversation_call)
    message(FATAL_ERROR
      "Player conversation bypasses stable SimulationCommand targeting in ${player_conversation_file}")
  endif()
endforeach()

set(player_vehicle_entry_files
  "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  "${SOURCE_ROOT}/Tactical/VehicleMenu.cpp")
foreach(player_vehicle_entry_file IN LISTS player_vehicle_entry_files)
  file(READ "${player_vehicle_entry_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(EVENT_SoldierEnterVehicle|EnterVehicle|MERC_ENTER_VEHICLE)([^A-Za-z0-9_]|$)"
    direct_player_vehicle_entry_call "${contents}")
  if(direct_player_vehicle_entry_call)
    message(FATAL_ERROR
      "Player vehicle entry bypasses stable SimulationCommand targeting in ${player_vehicle_entry_file}")
  endif()
endforeach()

# Player pickup must capture the live world-item incarnation before movement.
# Calling the legacy helper directly retains only a reusable gWorldItems slot.
file(READ "${SOURCE_ROOT}/Tactical/Turn Based Input.cpp"
  player_item_input_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])SoldierPickupItem([^A-Za-z0-9_]|$)"
  direct_player_item_pickup "${player_item_input_contents}")
if(direct_player_item_pickup)
  message(FATAL_ERROR
    "Player world-item pickup bypasses stable SimulationCommand targeting in Tactical/Turn Based Input.cpp")
endif()

# Strategic inventory code still replaces the complete legacy world-item
# vector. Keep a matching directory rebuild in every file that owns one of
# those replacement paths; review keeps the call immediately after assignment.
set(live_world_item_replacement_files
  "${SOURCE_ROOT}/Tactical/Inventory Choosing.cpp"
  "${SOURCE_ROOT}/Strategic/Assignments.cpp"
  "${SOURCE_ROOT}/Strategic/Hourly Update.cpp")
foreach(live_world_item_replacement_file IN LISTS live_world_item_replacement_files)
  file(READ "${live_world_item_replacement_file}"
    live_world_item_replacement_contents)
  string(REGEX MATCHALL
    "gWorldItems[ \t]*=[^=;\r\n]+"
    live_world_item_replacements
    "${live_world_item_replacement_contents}")
  string(REGEX MATCHALL
    "RebuildJa2TacticalWorldItemDirectory[ \t\r\n]*\\("
    rebuilt_live_world_item_replacements
    "${live_world_item_replacement_contents}")
  list(LENGTH live_world_item_replacements
    live_world_item_replacement_count)
  list(LENGTH rebuilt_live_world_item_replacements
    rebuilt_live_world_item_replacement_count)
  if(NOT live_world_item_replacement_count EQUAL
      rebuilt_live_world_item_replacement_count)
    message(FATAL_ERROR
      "A whole gWorldItems replacement lacks a matching stable-identity rebuild in ${live_world_item_replacement_file}")
  endif()
endforeach()

# New live-vector replacement or direct existence mutation paths must opt into
# the stable-identity lifecycle deliberately. The low-level world-item owner
# and the checked Lua adapter are the only remaining existence writers.
file(GLOB live_world_item_production_files
  "${SOURCE_ROOT}/Tactical/*.cpp"
  "${SOURCE_ROOT}/Strategic/*.cpp")
set(live_world_item_existence_writers
  "${SOURCE_ROOT}/Tactical/World Items.cpp"
  "${SOURCE_ROOT}/Strategic/LuaInitNPCs.cpp")
foreach(live_world_item_production_file IN LISTS live_world_item_production_files)
  file(READ "${live_world_item_production_file}"
    live_world_item_production_contents)
  string(REGEX MATCH
    "gWorldItems[ \t]*="
    whole_live_world_item_replacement
    "${live_world_item_production_contents}")
  if(whole_live_world_item_replacement)
    list(FIND live_world_item_replacement_files
      "${live_world_item_production_file}"
      live_world_item_replacement_owner_index)
    if(live_world_item_replacement_owner_index EQUAL -1)
      message(FATAL_ERROR
        "Untracked whole gWorldItems replacement in ${live_world_item_production_file}; rebuild TacticalWorldItemDirectory after replacement")
    endif()
  endif()

  string(REGEX MATCH
    "gWorldItems[ \t]*\\[[^\r\n]+\\][ \t]*\\.fExists[ \t]*=[^=]"
    direct_live_world_item_existence_write
    "${live_world_item_production_contents}")
  if(direct_live_world_item_existence_write)
    list(FIND live_world_item_existence_writers
      "${live_world_item_production_file}"
      live_world_item_existence_writer_index)
    if(live_world_item_existence_writer_index EQUAL -1)
      message(FATAL_ERROR
        "Direct live world-item existence mutation bypasses stable identity in ${live_world_item_production_file}")
    endif()
  endif()
endforeach()

# Tactical world identity is owned by EngineRuntime's TacticalWorldSession.
# Exact legacy globals remain readable compatibility mirrors, but a second
# production writer would silently split world and turn identity again.
set(world_state_source_directories
  Editor
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  sgp)
set(world_state_files)
foreach(source_directory IN LISTS world_state_source_directories)
  file(GLOB_RECURSE source_files
    "${SOURCE_ROOT}/${source_directory}/*.cpp")
  list(APPEND world_state_files ${source_files})
endforeach()

set(world_state_owner "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp")
set(world_state_mirrors
  gWorldSectorX
  gWorldSectorY
  gbWorldSectorZ)
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${world_state_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS world_state_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${mirror}[ \t\r\n]*(\\+\\+|--|[+*/%-]?=[^=])|(^|[^A-Za-z0-9_])(\\+\\+|--)[ \t\r\n]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_write "${contents}")
    if(world_state_write)
      message(FATAL_ERROR
        "Production code writes tactical-world compatibility mirror '${mirror}' in ${source_file}; route the transition through TacticalWorldAdapter")
    endif()
    string(REGEX MATCH
      "(^|[^&])&[ \t\r\n]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_address_escape "${contents}")
    if(world_state_address_escape)
      message(FATAL_ERROR
        "Production code passes tactical-world compatibility mirror '${mirror}' by address in ${source_file}; stage a local value and route writes through TacticalWorldAdapter")
    endif()
    string(REGEX MATCH
      "GetSectorFromFileName[ \t\r\n]*\\([^\\)]*${mirror}([^A-Za-z0-9_]|$)"
      world_state_reference_escape "${contents}")
    if(world_state_reference_escape)
      message(FATAL_ERROR
        "Editor filename parsing passes tactical-world compatibility mirror '${mirror}' by mutable reference in ${source_file}; parse locals and route writes through TacticalWorldAdapter")
    endif()
  endforeach()
endforeach()

# World-load state and generation have no legacy storage requirement. They are
# read directly from TacticalWorldSession and must not return as duplicate
# scalar owners.
set(retired_tactical_world_mirrors
  gfWorldLoaded
  guiWorldLoadGeneration)
set(world_state_declaration_files ${world_state_files})
foreach(source_directory IN LISTS world_state_source_directories)
  file(GLOB_RECURSE declaration_files
    "${SOURCE_ROOT}/${source_directory}/*.h"
    "${SOURCE_ROOT}/${source_directory}/*.hpp")
  list(APPEND world_state_declaration_files ${declaration_files})
endforeach()

# Coordinate compatibility names are const-reference projections. This keeps
# their established cheap read syntax while making an accidental writer fail at
# compilation even before this source-level ratchet runs. A stale mutable
# redeclaration in one translation unit would violate the ODR and reinterpret a
# reference object as an integer, so reject those declarations explicitly.
foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS world_state_mirrors)
    string(REGEX REPLACE
      "extern[ \t]+const[ \t]+(INT8|INT16)[ \t]*&[ \t]*${mirror}[ \t]*;"
      "" contents_without_canonical_world_projection "${contents}")
    string(REGEX MATCH
      "extern[^\r\n;]*${mirror}[ \t]*;"
      invalid_world_projection_declaration
      "${contents_without_canonical_world_projection}")
    if(invalid_world_projection_declaration)
      message(FATAL_ERROR
        "Tactical-world projection '${mirror}' has a noncanonical declaration in ${source_file}; use the canonical const-reference declaration")
    endif()
  endforeach()
endforeach()

foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS retired_tactical_world_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${mirror}([^A-Za-z0-9_]|$)"
      retired_tactical_world_mirror "${contents}")
    if(retired_tactical_world_mirror)
      message(FATAL_ERROR
        "Retired tactical-world mirror '${mirror}' returned in ${source_file}; read TacticalWorldSession through IsJa2TacticalWorldLoaded or CaptureJa2TacticalWorld")
    endif()
  endforeach()
endforeach()

# Current, pending, and previous application screen state are owned by
# StateController.
# The former scalar mirrors have no serialization or external ABI requirement
# and must not return as second sources of truth.
set(retired_screen_state_mirrors
  guiCurrentScreen
  guiPendingScreen
  guiPreviousScreen)
foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS retired_screen_state_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${mirror}([^A-Za-z0-9_]|$)"
      retired_screen_state_mirror "${contents}")
    if(retired_screen_state_mirror)
      message(FATAL_ERROR
        "Retired screen-state mirror '${mirror}' returned in ${source_file}; query StateController through the gameloop screen accessors")
    endif()
  endforeach()
endforeach()

# A state callback failure is an application diagnostic, not just an error
# enum. Core retains the exception without interpreting it, and the JA2 adapter
# rethrows it into SGP's established error-screen boundary.
file(READ "${SOURCE_ROOT}/Engine/Core/StateRegistry.h"
  state_registry_header_contents)
foreach(required_state_exception_fragment IN ITEMS
    "std::exception_ptr callbackException"
    "std::current_exception()")
  string(FIND "${state_registry_header_contents}"
    "${required_state_exception_fragment}" required_state_exception_position)
  if(required_state_exception_position EQUAL -1)
    message(FATAL_ERROR
      "StateRegistry lost callback diagnostic '${required_state_exception_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/Screens.cpp" state_screen_adapter_contents)
string(FIND "${state_screen_adapter_contents}"
  "std::rethrow_exception(result.callbackException)"
  state_screen_exception_position)
if(state_screen_exception_position EQUAL -1)
  message(FATAL_ERROR
    "JA2 screen adapter no longer forwards StateRegistry callback diagnostics")
endif()

file(READ "${SOURCE_ROOT}/tests/engine_core_tests.cpp"
  state_registry_test_contents)
string(FIND "${state_registry_test_contents}" "preserved handler failure"
  state_registry_exception_test_position)
if(state_registry_exception_test_position EQUAL -1)
  message(FATAL_ERROR
    "Engine Core tests no longer prove StateRegistry exception retention")
endif()

# TacticalActor is the canonical component aggregate. The staged migration
# previously kept a second v101 record, raw POD footprint, placeholder pointers,
# and hundreds of compatibility-specific assertions here. The cutover is now a
# final-state contract: one private owner per component, explicit reset and
# persistence operations, and no dependency on the C++ object layout.
file(READ "${SOURCE_ROOT}/Tactical/Soldier Control.h"
  tactical_soldier_control_header_contents)

# Soldier Control.h is now a deprecated include-only umbrella. Keeping every
# non-comment line to pragma/include directives prevents it from regaining
# constants, declarations, aliases, or inline behavior.
string(REPLACE "\r\n" "\n" tactical_soldier_control_header_lines
  "${tactical_soldier_control_header_contents}")
string(REPLACE "\n" ";" tactical_soldier_control_header_lines
  "${tactical_soldier_control_header_lines}")
foreach(tactical_soldier_control_header_line IN LISTS
    tactical_soldier_control_header_lines)
  string(STRIP "${tactical_soldier_control_header_line}"
    tactical_soldier_control_header_line)
  if(tactical_soldier_control_header_line STREQUAL "" OR
     tactical_soldier_control_header_line MATCHES "^//" OR
     tactical_soldier_control_header_line STREQUAL "#pragma once" OR
     tactical_soldier_control_header_line MATCHES
       "^#[ \t]*include[ \t]+[<\"].*[>\"]$")
    continue()
  endif()
  message(FATAL_ERROR
    "Soldier Control.h is an include-only compatibility facade but contains '${tactical_soldier_control_header_line}'")
endforeach()
file(READ "${SOURCE_ROOT}/Tactical/TacticalActor.h"
  tactical_actor_header_contents)
set(tactical_soldier_control_source
  "${SOURCE_ROOT}/Tactical/Soldier Control.cpp")
if(EXISTS "${tactical_soldier_control_source}")
  message(FATAL_ERROR
    "Retired Soldier Control.cpp returned; place actor behavior in its focused owner")
endif()
# Retain the empty compatibility variable while older extraction ratchets are
# progressively rewritten around the now-retired monolith.
set(tactical_actor_source_contents "")
file(READ "${SOURCE_ROOT}/Tactical/Civ Quotes.h"
  tactical_civ_quotes_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActor.cpp"
  tactical_actor_aggregate_source_contents)
file(READ "${SOURCE_ROOT}/tests/tactical_actor_header_tests.cpp"
  tactical_actor_header_test_contents)
file(READ "${SOURCE_ROOT}/tests/CMakeLists.txt"
  tactical_test_build_contents)
file(READ "${SOURCE_ROOT}/CMakeLists.txt"
  tactical_root_build_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  tactical_actor_creation_source_contents)
file(READ "${SOURCE_ROOT}/Utils/Timer Control.cpp"
  tactical_actor_timer_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Functions.h"
  tactical_soldier_functions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Components.h"
  tactical_actor_components_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConditions.h"
  tactical_actor_conditions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConditions.cpp"
  tactical_actor_conditions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorBleeding.h"
  tactical_actor_bleeding_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorBleeding.cpp"
  tactical_actor_bleeding_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAssignments.h"
  tactical_actor_assignments_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAssignments.cpp"
  tactical_actor_assignments_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConsumables.h"
  tactical_actor_consumables_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConsumables.cpp"
  tactical_actor_consumables_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationFrames.h"
  tactical_actor_animation_frames_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationFrames.cpp"
  tactical_actor_animation_frames_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationGeometry.h"
  tactical_actor_animation_geometry_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationGeometry.cpp"
  tactical_actor_animation_geometry_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationTiming.h"
  tactical_actor_animation_timing_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationTiming.cpp"
  tactical_actor_animation_timing_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationFootprint.h"
  tactical_actor_animation_footprint_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationFootprint.cpp"
  tactical_actor_animation_footprint_source_contents)
file(READ "${SOURCE_ROOT}/TileEngine/worlddef.h"
  tactical_world_definition_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorModifiers.h"
  tactical_actor_modifiers_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorModifiers.cpp"
  tactical_actor_modifiers_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorEquipment.h"
  tactical_actor_equipment_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorEquipment.cpp"
  tactical_actor_equipment_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRadio.h"
  tactical_actor_radio_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRadio.cpp"
  tactical_actor_radio_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRobotics.h"
  tactical_actor_robotics_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRobotics.cpp"
  tactical_actor_robotics_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMobility.h"
  tactical_actor_mobility_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMobility.cpp"
  tactical_actor_mobility_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorWeaponHandling.h"
  tactical_actor_weapon_handling_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorWeaponHandling.cpp"
  tactical_actor_weapon_handling_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRangedActions.h"
  tactical_actor_ranged_actions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRangedActions.cpp"
  tactical_actor_ranged_actions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorWorldPlacement.h"
  tactical_actor_world_placement_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorWorldPlacement.cpp"
  tactical_actor_world_placement_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRouteExecution.h"
  tactical_actor_route_execution_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRouteExecution.cpp"
  tactical_actor_route_execution_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorOrientation.h"
  tactical_actor_orientation_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorOrientation.cpp"
  tactical_actor_orientation_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationSelection.h"
  tactical_actor_animation_selection_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationSelection.cpp"
  tactical_actor_animation_selection_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAiBehavior.h"
  tactical_actor_ai_behavior_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAiBehavior.cpp"
  tactical_actor_ai_behavior_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMovementAudio.h"
  tactical_actor_movement_audio_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMovementAudio.cpp"
  tactical_actor_movement_audio_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorInterrupts.h"
  tactical_actor_interrupts_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorInterrupts.cpp"
  tactical_actor_interrupts_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLongActions.h"
  tactical_actor_long_actions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLongActions.cpp"
  tactical_actor_long_actions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageQueue.h"
  tactical_actor_damage_queue_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageQueue.cpp"
  tactical_actor_damage_queue_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageFeedback.h"
  tactical_actor_damage_feedback_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageFeedback.cpp"
  tactical_actor_damage_feedback_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Render Palette Effects.h"
  render_palette_effects_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Render Palette Effects.cpp"
  render_palette_effects_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorProfileClassification.h"
  tactical_actor_profile_classification_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorProfileClassification.cpp"
  tactical_actor_profile_classification_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalServices.h"
  tactical_actor_medical_services_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalServices.cpp"
  tactical_actor_medical_services_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalSession.h"
  tactical_actor_medical_session_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalSession.cpp"
  tactical_actor_medical_session_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorFieldOperations.h"
  tactical_actor_field_operations_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorFieldOperations.cpp"
  tactical_actor_field_operations_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Points.h"
  tactical_points_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Points.cpp"
  tactical_points_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalTreatment.h"
  tactical_actor_medical_treatment_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMedicalTreatment.cpp"
  tactical_actor_medical_treatment_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorPrisonerOperations.h"
  tactical_actor_prisoner_operations_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorPrisonerOperations.cpp"
  tactical_actor_prisoner_operations_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorSkills.h"
  tactical_actor_skills_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorSkills.cpp"
  tactical_actor_skills_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorSpotting.h"
  tactical_actor_spotting_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorSpotting.cpp"
  tactical_actor_spotting_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurncoats.h"
  tactical_actor_turncoats_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurncoats.cpp"
  tactical_actor_turncoats_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatActions.h"
  tactical_actor_combat_actions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatActions.cpp"
  tactical_actor_combat_actions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatReactions.h"
  tactical_actor_combat_reactions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCombatReactions.cpp"
  tactical_actor_combat_reactions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorVisibility.h"
  tactical_actor_visibility_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorVisibility.cpp"
  tactical_actor_visibility_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationTransitions.h"
  tactical_actor_animation_transitions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationTransitions.cpp"
  tactical_actor_animation_transitions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAppearance.h"
  tactical_actor_appearance_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAppearance.cpp"
  tactical_actor_appearance_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/LogicalBodyTypes/PaletteTable.cpp"
  tactical_logical_palette_table_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorBattleSounds.h"
  tactical_actor_battle_sounds_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorBattleSounds.cpp"
  tactical_actor_battle_sounds_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageResolution.h"
  tactical_actor_damage_resolution_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDamageResolution.cpp"
  tactical_actor_damage_resolution_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLifecycle.h"
  tactical_actor_lifecycle_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLifecycle.cpp"
  tactical_actor_lifecycle_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLocomotion.h"
  tactical_actor_locomotion_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLocomotion.cpp"
  tactical_actor_locomotion_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnLifecycle.h"
  tactical_actor_turn_lifecycle_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnLifecycle.cpp"
  tactical_actor_turn_lifecycle_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRecovery.h"
  tactical_actor_recovery_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorRecovery.cpp"
  tactical_actor_recovery_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTraversal.h"
  tactical_actor_traversal_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTraversal.cpp"
  tactical_actor_traversal_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorExplosives.h"
  tactical_actor_explosives_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorExplosives.cpp"
  tactical_actor_explosives_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorInteractions.h"
  tactical_actor_interactions_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorInteractions.cpp"
  tactical_actor_interactions_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLighting.h"
  tactical_actor_lighting_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorLighting.cpp"
  tactical_actor_lighting_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnBudget.h"
  tactical_actor_turn_budget_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnBudget.cpp"
  tactical_actor_turn_budget_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnMaintenance.h"
  tactical_actor_turn_maintenance_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorTurnMaintenance.cpp"
  tactical_actor_turn_maintenance_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConditionPresentation.h"
  tactical_actor_condition_presentation_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorConditionPresentation.cpp"
  tactical_actor_condition_presentation_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCovertOps.h"
  tactical_actor_covert_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCovertOps.cpp"
  tactical_actor_covert_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDisease.h"
  tactical_actor_disease_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDisease.cpp"
  tactical_actor_disease_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Interface Items.cpp"
  tactical_interface_items_contents)
file(READ "${SOURCE_ROOT}/Tactical/Interface Panels.cpp"
  tactical_interface_panels_contents)
file(READ "${SOURCE_ROOT}/Strategic/Map Screen Interface.cpp"
  strategic_map_screen_interface_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDragging.h"
  tactical_actor_dragging_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDragging.cpp"
  tactical_actor_dragging_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/CMakeLists.txt"
  tactical_build_contents)
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.h"
  tactical_actor_persistence_header_contents)
file(READ "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp"
  tactical_actor_persistence_source_contents)
file(READ "${SOURCE_ROOT}/Ja2/GameVersion.h"
  tactical_actor_save_version_contents)
file(READ "${SOURCE_ROOT}/TileEngine/worlddef.cpp"
  tactical_actor_map_writer_contents)

string(FIND "${tactical_actor_header_contents}"
  "class TacticalActor"
  tactical_actor_begin)
string(FIND "${tactical_actor_header_contents}"
  "};\n\n#endif"
  tactical_actor_end)
if(tactical_actor_begin EQUAL -1 OR tactical_actor_end EQUAL -1 OR
   tactical_actor_end LESS tactical_actor_begin)
  message(FATAL_ERROR
    "Could not locate the canonical TacticalActor component aggregate")
endif()
math(EXPR tactical_actor_length
  "${tactical_actor_end} - ${tactical_actor_begin}")
string(SUBSTRING "${tactical_actor_header_contents}"
  ${tactical_actor_begin} ${tactical_actor_length}
  tactical_actor_contents)

string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"TacticalActor.h\""
  tactical_actor_compatibility_include)
string(REGEX MATCH
  "class[ \t\r\n]+TacticalActor[ \t\r\n]*\\{"
  tactical_actor_definition_in_legacy_header
  "${tactical_soldier_control_header_contents}")
string(FIND "${tactical_actor_aggregate_source_contents}"
  "#include \"TacticalActor.h\""
  tactical_actor_implementation_include)
string(FIND "${tactical_actor_header_test_contents}"
  "#include \"TacticalActor.h\""
  tactical_actor_header_test_include)
string(FIND "${tactical_actor_header_test_contents}"
  "Soldier Control.h"
  tactical_actor_header_test_legacy_include)
string(FIND "${tactical_test_build_contents}"
  "add_executable(tactical_actor_header_tests"
  tactical_actor_header_test_target)
string(FIND "${tactical_test_build_contents}"
  "add_test(NAME tactical_actor_header COMMAND tactical_actor_header_tests)"
  tactical_actor_header_test_registration)
string(FIND "${tactical_root_build_contents}"
  "add_dependencies(ja2_headless_tests tactical_actor_header_tests)"
  tactical_actor_header_headless_dependency)
if(tactical_actor_compatibility_include EQUAL -1 OR
   tactical_actor_definition_in_legacy_header OR
   tactical_actor_implementation_include EQUAL -1 OR
   tactical_actor_header_test_include EQUAL -1 OR
   NOT tactical_actor_header_test_legacy_include EQUAL -1 OR
   tactical_actor_header_test_target EQUAL -1 OR
   tactical_actor_header_test_registration EQUAL -1 OR
   tactical_actor_header_headless_dependency EQUAL -1)
  message(FATAL_ERROR
    "TacticalActor lost its focused aggregate header, legacy compatibility include, or standalone compile guard")
endif()

string(FIND "${tactical_build_contents}"
  "Soldier Control.cpp"
  tactical_actor_retired_source_build_entry)
if(NOT tactical_actor_retired_source_build_entry EQUAL -1)
  message(FATAL_ERROR
    "Retired Soldier Control.cpp returned to the tactical build")
endif()

file(GLOB tactical_actor_implementation_files
  "${SOURCE_ROOT}/Tactical/TacticalActor*.cpp")
if(NOT tactical_actor_implementation_files)
  message(FATAL_ERROR
    "No TacticalActor implementation sources were found for dependency validation")
endif()
foreach(tactical_actor_implementation_file IN LISTS
    tactical_actor_implementation_files)
  file(READ "${tactical_actor_implementation_file}"
    tactical_actor_implementation_contents)
  string(FIND "${tactical_actor_implementation_contents}"
    "#include \"Soldier Control.h\""
    tactical_actor_legacy_header_include)
  if(NOT tactical_actor_legacy_header_include EQUAL -1)
    message(FATAL_ERROR
      "${tactical_actor_implementation_file} regained a direct Soldier Control.h dependency; include TacticalActor.h and focused collaborators")
  endif()
endforeach()

set(tactical_actor_pointer_api_headers
  "Laptop/CampaignStats.h"
  "Laptop/insurance Contract.h"
  "Laptop/personnel.h"
  "Strategic/Assignments.h"
  "Strategic/Facilities.h"
  "Strategic/Map Screen Interface.h"
  "Strategic/Map Screen Interface Bottom.h"
  "Strategic/Merc Contract.h"
  "Strategic/MilitiaSquads.h"
  "Strategic/Quest Debug System.h"
  "Strategic/Queen Command.h"
  "Strategic/Rebel Command.h"
  "Strategic/Strategic Movement.h"
  "Strategic/Strategic Status.h"
  "Strategic/Strategic Town Loyalty.h"
  "Strategic/Town Militia.h"
  "Strategic/mapscreen.h"
  "Strategic/strategic.h"
  "Strategic/strategicmap.h"
  "Strategic/strategic town reputation.h"
  "Tactical/Air Raid.h"
  "Tactical/Animation Control.h"
  "Tactical/Animation Data.h"
  "Tactical/Bullets.h"
  "Tactical/Civ Quotes.h"
  "Tactical/Drugs And Alcohol.h"
  "Tactical/DynamicDialogue.h"
  "Tactical/Food.h"
  "Tactical/Handle Items.h"
  "Tactical/Handle UI.h"
  "Tactical/Interface Utils.h"
  "Tactical/Items.h"
  "Tactical/LOS.h"
  "Tactical/LogicalBodyTypes/Filter.h"
  "Tactical/Merc Hiring.h"
  "Tactical/Militia Control.h"
  "Tactical/Morale.h"
  "Tactical/Overhead.h"
  "Tactical/SkillCheck.h"
  "Tactical/Soldier Add.h"
  "Tactical/Soldier Create.h"
  "Tactical/Soldier Functions.h"
  "Tactical/Soldier Profile.h"
  "Tactical/Squads.h"
  "Tactical/TeamTurns.h"
  "Tactical/Vehicles.h"
  "Tactical/Weapons.h"
  "Tactical/faces.h"
  "Tactical/soldier profile type.h"
  "Tactical/soldier tile.h"
  "TacticalAI/ai.h"
  "TileEngine/Smell.h")
foreach(tactical_actor_pointer_api_header IN LISTS
    tactical_actor_pointer_api_headers)
  file(READ "${SOURCE_ROOT}/${tactical_actor_pointer_api_header}"
    tactical_actor_pointer_api_header_contents)
  string(FIND "${tactical_actor_pointer_api_header_contents}"
    "#include \"Soldier Control.h\""
    tactical_actor_pointer_api_legacy_include)
  if(NOT tactical_actor_pointer_api_legacy_include EQUAL -1)
    message(FATAL_ERROR
      "${tactical_actor_pointer_api_header} regained the Soldier Control.h compatibility facade; forward-declare TacticalActor and include owned value types")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Class.h"
  tactical_soldier_class_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/opplist.h"
  tactical_opplist_header_contents)
file(READ "${SOURCE_ROOT}/tests/tactical_actor_api_headers_tests.cpp"
  tactical_actor_api_header_test_contents)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"Soldier Class.h\""
  tactical_soldier_class_compatibility_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "SOLDIER_CLASS_NONE,"
  tactical_soldier_class_legacy_definition)
string(FIND "${tactical_soldier_class_header_contents}"
  "SOLDIER_CLASS_NONE,"
  tactical_soldier_class_begin)
string(FIND "${tactical_soldier_class_header_contents}"
  "SOLDIER_CLASS_MAX,"
  tactical_soldier_class_end)
string(FIND "${tactical_opplist_header_contents}"
  "#include \"Overhead Types.h\""
  tactical_opplist_owned_types_include)
string(FIND "${tactical_actor_api_header_test_contents}"
  "#include \"Tactical/Soldier Class.h\""
  tactical_actor_api_header_test_class_include)
string(FIND "${tactical_actor_api_header_test_contents}"
  "SOLDIER_CLASS_CREATURE == 7"
  tactical_actor_api_header_test_class_values)
string(FIND "${tactical_test_build_contents}"
  "add_executable(tactical_actor_api_headers_tests"
  tactical_actor_api_header_test_target)
string(FIND "${tactical_test_build_contents}"
  "add_test(NAME tactical_actor_api_headers"
  tactical_actor_api_header_test_registration)
string(FIND "${tactical_root_build_contents}"
  "add_dependencies(ja2_headless_tests tactical_actor_api_headers_tests)"
  tactical_actor_api_header_headless_dependency)
if(tactical_soldier_class_compatibility_include EQUAL -1 OR
   NOT tactical_soldier_class_legacy_definition EQUAL -1 OR
   tactical_soldier_class_begin EQUAL -1 OR
   tactical_soldier_class_end EQUAL -1 OR
   tactical_opplist_owned_types_include EQUAL -1 OR
   tactical_actor_api_header_test_class_include EQUAL -1 OR
   tactical_actor_api_header_test_class_values EQUAL -1 OR
   tactical_actor_api_header_test_target EQUAL -1 OR
   tactical_actor_api_header_test_registration EQUAL -1 OR
   tactical_actor_api_header_headless_dependency EQUAL -1)
  message(FATAL_ERROR
    "Actor-reference API headers lost their focused compile guard or soldier-class boundary")
endif()

# The broad soldier-control facade is now an implementation/compatibility
# surface, not a dependency of application headers. Utils All.h remains the
# one intentional umbrella for legacy source compatibility.
set(tactical_actor_public_header_roots
  Editor
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  sgp)
set(tactical_actor_public_headers)
foreach(tactical_actor_public_header_root IN LISTS
    tactical_actor_public_header_roots)
  file(GLOB_RECURSE tactical_actor_public_headers_in_root
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/${tactical_actor_public_header_root}/*.h")
  list(APPEND tactical_actor_public_headers
    ${tactical_actor_public_headers_in_root})
endforeach()
foreach(tactical_actor_public_header IN LISTS tactical_actor_public_headers)
  file(RELATIVE_PATH tactical_actor_public_header_relative
    "${SOURCE_ROOT}" "${tactical_actor_public_header}")
  if(tactical_actor_public_header_relative STREQUAL "Utils/Utils All.h")
    continue()
  endif()
  file(READ "${tactical_actor_public_header}"
    tactical_actor_public_header_contents)
  string(REGEX MATCH
    "(^|\n)[ \t]*#[ \t]*include[ \t]*\"Soldier Control\\.h\""
    tactical_actor_public_header_facade_include
    "${tactical_actor_public_header_contents}")
  if(tactical_actor_public_header_facade_include)
    message(FATAL_ERROR
      "${tactical_actor_public_header_relative} imports the Soldier Control.h compatibility facade; include focused contracts and forward-declare TacticalActor")
  endif()
endforeach()

# The compatibility facade is retired from every production implementation
# file. The headless harness deliberately includes it to verify source
# compatibility, but application translation units must name their owners.
set(tactical_actor_production_sources)
foreach(tactical_actor_public_header_root IN LISTS
    tactical_actor_public_header_roots)
  file(GLOB_RECURSE tactical_actor_production_sources_in_root
    LIST_DIRECTORIES false
    "${SOURCE_ROOT}/${tactical_actor_public_header_root}/*.cpp")
  list(APPEND tactical_actor_production_sources
    ${tactical_actor_production_sources_in_root})
endforeach()
foreach(tactical_actor_production_source IN LISTS
    tactical_actor_production_sources)
  file(RELATIVE_PATH tactical_actor_production_source_relative
    "${SOURCE_ROOT}" "${tactical_actor_production_source}")
  file(READ "${tactical_actor_production_source}"
    tactical_actor_production_source_contents)
  string(REGEX MATCH
    "(^|\n)[ \t]*#[ \t]*include[ \t]*\"[^\"]*Soldier Control\\.h\""
    tactical_actor_production_source_facade_include
    "${tactical_actor_production_source_contents}")
  if(tactical_actor_production_source_facade_include)
    message(FATAL_ERROR
      "${tactical_actor_production_source_relative} imports the retired Soldier Control.h implementation facade; include TacticalActor.h and focused contracts")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Path Types.h"
  strategic_path_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Patrol Types.h"
  tactical_soldier_patrol_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Profile Constants.h"
  tactical_soldier_profile_constants_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Palette.h"
  tactical_soldier_palette_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Background Types.h"
  tactical_soldier_background_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorEmploymentTypes.h"
  tactical_actor_employment_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorStateFlags.h"
  tactical_actor_state_flags_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Components.h"
  tactical_soldier_components_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Create.h"
  tactical_soldier_create_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Profile.h"
  tactical_soldier_profile_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/DynamicDialogue.h"
  tactical_dynamic_dialogue_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/qarray.h"
  tactical_qarray_header_contents)
file(READ "${SOURCE_ROOT}/Strategic/mapscreen.h"
  strategic_mapscreen_header_contents)
file(READ "${SOURCE_ROOT}/tests/tactical_actor_service_api_headers_tests.cpp"
  tactical_actor_service_api_header_test_contents)
file(READ "${SOURCE_ROOT}/Utils/Utilities.cpp"
  tactical_utilities_source_contents)
file(READ "${SOURCE_ROOT}/Multiplayer/connect.h"
  tactical_multiplayer_connect_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/LogicalBodyTypes/BodyType.h"
  tactical_logical_body_type_header_contents)

string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"Strategic Path Types.h\""
  tactical_soldier_control_path_types_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"Soldier Patrol Types.h\""
  tactical_soldier_control_patrol_types_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"Soldier Palette.h\""
  tactical_soldier_control_palette_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"Soldier Background Types.h\""
  tactical_soldier_control_background_types_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"Soldier Profile Constants.h\""
  tactical_soldier_control_profile_constants_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"TacticalActorEmploymentTypes.h\""
  tactical_soldier_control_employment_types_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"TacticalActorLighting.h\""
  tactical_soldier_control_lighting_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#include \"TacticalActorStateFlags.h\""
  tactical_soldier_control_state_flags_include)
string(FIND "${tactical_soldier_control_header_contents}"
  "#define SOLDIER_PCUNDERAICONTROL"
  tactical_soldier_control_legacy_status_flag)
string(FIND "${tactical_soldier_control_header_contents}"
  "MERC_TYPE__PLAYER_CHARACTER,"
  tactical_soldier_control_legacy_employment_type)
string(FIND "${tactical_soldier_control_header_contents}"
  "struct CLOTHES_STRUCT"
  tactical_soldier_control_legacy_clothes_type)
string(FIND "${strategic_path_types_header_contents}"
  "struct path"
  tactical_strategic_path_definition)
string(FIND "${strategic_path_types_header_contents}"
  "using PathStPtr = PathSt*;"
  tactical_strategic_path_alias)
string(FIND "${tactical_soldier_patrol_types_header_contents}"
  "SOLDIER_PATROL_GRID_COUNT = 10"
  tactical_soldier_patrol_capacity)
string(FIND "${tactical_soldier_profile_constants_header_contents}"
  "#define NO_PROFILE 200"
  tactical_soldier_no_profile_sentinel)
string(FIND "${tactical_soldier_palette_header_contents}"
  "BOOLEAN GetPaletteRepIndexFromID"
  tactical_soldier_palette_lookup_declaration)
string(FIND "${tactical_soldier_palette_header_contents}"
  "#define CLOTHES_MAX 50"
  tactical_soldier_palette_clothes_capacity)
string(FIND "${tactical_soldier_palette_header_contents}"
  "extern CLOTHES_STRUCT Clothes[CLOTHES_MAX];"
  tactical_soldier_palette_clothes_table)
string(FIND "${tactical_soldier_background_types_header_contents}"
  "#define BACKGROUND_XENOPHOBIC 0x0000000000000002"
  tactical_soldier_background_xenophobic_flag)
string(FIND "${tactical_actor_employment_types_header_contents}"
  "MERC_TYPE__VEHICLE,"
  tactical_actor_vehicle_employment_type)
string(FIND "${tactical_actor_state_flags_header_contents}"
  "#define SOLDIER_PCUNDERAICONTROL               0x00000020"
  tactical_actor_player_ai_control_flag)
string(FIND "${tactical_actor_state_flags_header_contents}"
  "#define SOLDIER_COVERT_NPC_SPECIAL             0x00000020"
  tactical_actor_covert_npc_flag)
string(FIND "${tactical_utilities_source_contents}"
  "#include \"Soldier Palette.h\""
  tactical_utilities_palette_include)
string(FIND "${tactical_multiplayer_connect_header_contents}"
  "#include \"TacticalActor.h\""
  tactical_multiplayer_connect_actor_include)
string(FIND "${tactical_logical_body_type_header_contents}"
  "#include \"TacticalActor.h\""
  tactical_logical_body_type_actor_include)
string(FIND "${tactical_soldier_components_header_contents}"
  "#include \"Soldier Patrol Types.h\""
  tactical_soldier_components_patrol_include)
string(FIND "${tactical_soldier_create_header_contents}"
  "#include \"Soldier Patrol Types.h\""
  tactical_soldier_create_patrol_include)
string(FIND "${tactical_actor_skills_header_contents}"
  "BOOLEAN TwoStagedTrait"
  tactical_soldier_profile_trait_stage_declaration)
string(FIND "${tactical_dynamic_dialogue_header_contents}"
  "#include \"Soldier Profile Constants.h\""
  tactical_dynamic_dialogue_profile_include)
string(FIND "${tactical_actor_battle_sounds_header_contents}"
  "NUM_MERC_BATTLE_SOUNDS"
  tactical_actor_battle_sound_count)
string(FIND "${tactical_qarray_header_contents}"
  "#include \"TacticalActorBattleSounds.h\""
  tactical_qarray_battle_sound_include)
string(FIND "${strategic_mapscreen_header_contents}"
  "#include \"Strategic Path Types.h\""
  tactical_mapscreen_path_include)
string(FIND "${tactical_test_build_contents}"
  "add_executable(tactical_actor_service_api_headers_tests"
  tactical_actor_service_api_header_test_target)
string(FIND "${tactical_test_build_contents}"
  "add_test(NAME tactical_actor_service_api_headers"
  tactical_actor_service_api_header_test_registration)
string(FIND "${tactical_test_build_contents}"
  "static_assert(!IsComplete<TacticalActor>::value);"
  tactical_actor_service_api_header_incomplete_assertion)
string(FIND "${tactical_root_build_contents}"
  "add_dependencies(ja2_headless_tests tactical_actor_service_api_headers_tests)"
  tactical_actor_service_api_header_headless_dependency)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(std::is_standard_layout_v<PathSt>);"
  tactical_actor_service_api_path_layout_assertion)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(NO_PROFILE == 200);"
  tactical_actor_service_api_profile_assertion)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(NUM_MERC_BATTLE_SOUNDS == 16);"
  tactical_actor_service_api_battle_sound_assertion)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(BACKGROUND_FLAG_MAX == 12);"
  tactical_actor_service_api_background_assertion)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(MERC_TYPE__VEHICLE == 6);"
  tactical_actor_service_api_employment_assertion)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(CLOTHES_MAX == 50);"
  tactical_actor_service_api_clothes_capacity_assertion)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(SOLDIER_PCUNDERAICONTROL == 0x20);"
  tactical_actor_service_api_status_flag_assertion)
string(FIND "${tactical_actor_lighting_header_contents}"
  "void HandlePlayerTogglingLightEffects(BOOLEAN toggleValue);"
  tactical_actor_light_toggle_declaration)
string(FIND "${tactical_actor_lighting_source_contents}"
  "void HandlePlayerTogglingLightEffects(BOOLEAN toggleValue)"
  tactical_actor_light_toggle_definition)
string(FIND "${tactical_actor_source_contents}"
  "void HandlePlayerTogglingLightEffects("
  tactical_soldier_control_legacy_light_toggle)
string(FIND "${headless_test_contents}"
  "lightingRefreshPreservesOptionAndInvalidatesRender"
  tactical_actor_light_toggle_coverage)
if(tactical_soldier_control_path_types_include EQUAL -1 OR
   tactical_soldier_control_patrol_types_include EQUAL -1 OR
   tactical_soldier_control_palette_include EQUAL -1 OR
   tactical_soldier_control_background_types_include EQUAL -1 OR
   tactical_soldier_control_profile_constants_include EQUAL -1 OR
   tactical_soldier_control_employment_types_include EQUAL -1 OR
   tactical_soldier_control_lighting_include EQUAL -1 OR
   tactical_soldier_control_state_flags_include EQUAL -1 OR
   NOT tactical_soldier_control_legacy_status_flag EQUAL -1 OR
   NOT tactical_soldier_control_legacy_employment_type EQUAL -1 OR
   NOT tactical_soldier_control_legacy_clothes_type EQUAL -1 OR
   tactical_strategic_path_definition EQUAL -1 OR
   tactical_strategic_path_alias EQUAL -1 OR
   tactical_soldier_patrol_capacity EQUAL -1 OR
   tactical_soldier_no_profile_sentinel EQUAL -1 OR
   tactical_soldier_palette_lookup_declaration EQUAL -1 OR
   tactical_soldier_palette_clothes_capacity EQUAL -1 OR
   tactical_soldier_palette_clothes_table EQUAL -1 OR
   tactical_soldier_background_xenophobic_flag EQUAL -1 OR
   tactical_actor_vehicle_employment_type EQUAL -1 OR
   tactical_actor_player_ai_control_flag EQUAL -1 OR
   tactical_actor_covert_npc_flag EQUAL -1 OR
   tactical_utilities_palette_include EQUAL -1 OR
   tactical_multiplayer_connect_actor_include EQUAL -1 OR
   tactical_logical_body_type_actor_include EQUAL -1 OR
   tactical_soldier_components_patrol_include EQUAL -1 OR
   tactical_soldier_create_patrol_include EQUAL -1 OR
   tactical_soldier_profile_trait_stage_declaration EQUAL -1 OR
   tactical_dynamic_dialogue_profile_include EQUAL -1 OR
   tactical_actor_battle_sound_count EQUAL -1 OR
   tactical_qarray_battle_sound_include EQUAL -1 OR
   tactical_mapscreen_path_include EQUAL -1 OR
   tactical_actor_service_api_header_test_target EQUAL -1 OR
   tactical_actor_service_api_header_test_registration EQUAL -1 OR
   tactical_actor_service_api_header_incomplete_assertion EQUAL -1 OR
   tactical_actor_service_api_header_headless_dependency EQUAL -1 OR
   tactical_actor_service_api_path_layout_assertion EQUAL -1 OR
   tactical_actor_service_api_profile_assertion EQUAL -1 OR
   tactical_actor_service_api_battle_sound_assertion EQUAL -1 OR
   tactical_actor_service_api_background_assertion EQUAL -1 OR
   tactical_actor_service_api_employment_assertion EQUAL -1 OR
   tactical_actor_service_api_clothes_capacity_assertion EQUAL -1 OR
   tactical_actor_service_api_status_flag_assertion EQUAL -1 OR
   tactical_actor_light_toggle_declaration EQUAL -1 OR
   tactical_actor_light_toggle_definition EQUAL -1 OR
   NOT tactical_soldier_control_legacy_light_toggle EQUAL -1 OR
   tactical_actor_light_toggle_coverage EQUAL -1)
  message(FATAL_ERROR
    "Actor service headers lost their focused path, patrol, profile, or isolated-compile boundary")
endif()

# The compatibility facade may re-export focused actor contracts, but it may
# not become their second owner. Keep the persisted values and event entry
# points pinned in their standalone headers and in compile-time tests.
file(READ "${SOURCE_ROOT}/Tactical/Animation Data.h"
  tactical_animation_data_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Grid Direction.h"
  tactical_grid_direction_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Drug Types.h"
  tactical_soldier_drug_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Stat Types.h"
  tactical_soldier_stat_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Taunt Types.h"
  tactical_taunt_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCrowBehavior.h"
  tactical_actor_crow_behavior_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDebug.h"
  tactical_actor_debug_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorPredicates.h"
  tactical_actor_predicates_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorPredicates.cpp"
  tactical_actor_predicates_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalDestinationTypes.h"
  tactical_destination_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorAnimationState.h"
  tactical_actor_animation_state_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorBloodState.h"
  tactical_actor_blood_state_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorEvents.h"
  tactical_actor_events_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorInterrupts.h"
  tactical_actor_interrupts_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorMovementState.h"
  tactical_actor_movement_state_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorPendingActionTypes.h"
  tactical_actor_pending_action_types_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorQuoteFlags.h"
  tactical_actor_quote_flags_header_contents)

foreach(tactical_actor_focused_contract_include IN ITEMS
    "Grid Direction.h"
    "Soldier Stat Types.h"
    "Taunt Types.h"
    "TacticalActorAnimationState.h"
    "TacticalActorBloodState.h"
    "TacticalActorConditions.h"
    "TacticalActorCrowBehavior.h"
    "TacticalActorDamageResolution.h"
    "TacticalActorDebug.h"
    "TacticalActorEvents.h"
    "TacticalActorInterrupts.h"
    "TacticalActorLifecycle.h"
    "TacticalActorLocomotion.h"
    "TacticalActorLongActions.h"
    "TacticalActorMovementState.h"
    "TacticalActorModifiers.h"
    "TacticalActorPendingActionTypes.h"
    "TacticalActorPredicates.h"
    "TacticalActorQuoteFlags.h"
    "TacticalActorSkills.h"
    "TacticalDestinationTypes.h")
  string(FIND "${tactical_soldier_control_header_contents}"
    "#include \"${tactical_actor_focused_contract_include}\""
    tactical_actor_focused_contract_include_position)
  if(tactical_actor_focused_contract_include_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control.h stopped re-exporting focused actor contract ${tactical_actor_focused_contract_include}")
  endif()
endforeach()

string(FIND "${tactical_animation_data_header_contents}"
  "struct ANIM_PROF_TILE" tactical_animation_profile_tile_contract)
string(FIND "${tactical_grid_direction_header_contents}"
  "UINT8 GetDirectionFromGridNo" tactical_grid_direction_contract)
string(FIND "${tactical_soldier_drug_types_header_contents}"
  "DRUG_EFFECT_MAX = 20" tactical_soldier_drug_capacity_contract)
string(FIND "${tactical_soldier_drug_types_header_contents}"
  "DRUG_TYPE_MAX = 32" tactical_soldier_drug_type_capacity_contract)
string(FIND "${tactical_soldier_stat_types_header_contents}"
  "CHANGE_STAT_RECENTLY_DURATION = 60000"
  tactical_soldier_stat_duration_contract)
string(FIND "${tactical_soldier_stat_types_header_contents}"
  "LVL_INCREASE = 0x0400" tactical_soldier_stat_mask_contract)
string(FIND "${tactical_taunt_types_header_contents}"
  "TAUNT_S_MISS_THROWING_KNIFE = 0x1000000000000000"
  tactical_taunt_flag_contract)
string(FIND "${tactical_taunt_types_header_contents}"
  "TAUNT_FLAG_MAX = TAUNT_FLAG_1_MAX + TAUNT_FLAG_2_MAX"
  tactical_taunt_capacity_contract)
string(FIND "${tactical_actor_crow_behavior_header_contents}"
  "void CrowsFlyAway(std::uint8_t team);"
  tactical_actor_crow_behavior_contract)
string(FIND "${tactical_actor_debug_header_contents}"
  "void DebugValidateSoldierData();" tactical_actor_debug_contract)
string(FIND "${tactical_actor_locomotion_header_contents}"
  "void MoveMercFacingDirection(" tactical_actor_facing_adapter_contract)
string(FIND "${tactical_actor_predicates_header_contents}"
  "bool consideredNeutralForAttack(" tactical_actor_neutral_predicate_contract)
string(FIND "${tactical_actor_predicates_source_contents}"
  "animationState < NUMANIMATIONSTATES"
  tactical_actor_predicate_animation_validation)
string(FIND "${tactical_build_contents}"
  "TacticalActorPredicates.cpp" tactical_actor_predicates_build_source)
string(FIND "${tactical_destination_types_header_contents}"
  "FALLINGTEST = 3" tactical_destination_mode_contract)
string(FIND "${tactical_soldier_components_header_contents}"
  "#include \"Soldier Drug Types.h\"" tactical_soldier_drug_component_include)
string(FIND "${tactical_soldier_components_header_contents}"
  "DRUG_EFFECT_HP = 0" tactical_soldier_drug_component_duplicate)
string(FIND "${tactical_actor_animation_state_header_contents}"
  "NO_PENDING_ANIMATION = 32001" tactical_actor_animation_sentinel_contract)
string(FIND "${tactical_actor_blood_state_header_contents}"
  "MAXBLOOD = 40" tactical_actor_blood_capacity_contract)
string(FIND "${tactical_actor_events_header_contents}"
  "void SendSoldierPositionEvent(" tactical_actor_position_event_contract)
string(FIND "${tactical_actor_interrupts_header_contents}"
  "BOOLEAN ResolvePendingInterrupt(" tactical_actor_interrupt_contract)
string(FIND "${tactical_actor_movement_state_header_contents}"
  "REASON_STOPPED_NO_APS = 0" tactical_actor_movement_reason_contract)
string(FIND "${tactical_actor_pending_action_types_header_contents}"
  "MERC_OPENDOOR = 0" tactical_actor_pending_action_contract)
string(FIND "${tactical_actor_pending_action_types_header_contents}"
  "THROW_TARGET_MERC_CATCH" tactical_actor_throw_action_contract)
string(FIND "${tactical_actor_quote_flags_header_contents}"
  "#define SOLDIER_QUOTE_SAID_IN_SHIT" tactical_actor_quote_flag_contract)
string(FIND "${tactical_actor_skills_header_contents}"
  "SKILLS_MAX," tactical_actor_skill_capacity_contract)
string(FIND "${tactical_actor_damage_resolution_header_contents}"
  "TAKE_DAMAGE_GAS_NOTFIRE" tactical_actor_damage_reason_contract)
string(FIND "${tactical_actor_long_actions_header_contents}"
  "MTA_HACK" tactical_actor_long_action_contract)
string(FIND "${tactical_test_build_contents}"
  "set(tactical_actor_contract_headers" tactical_actor_contract_compile_list)
string(FIND "${tactical_test_build_contents}"
  "tactical_actor_contract_header_sources" tactical_actor_contract_compile_sources)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(DRUG_EFFECT_MAX == 20);" tactical_actor_drug_capacity_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(DRUG_TYPE_MAX == 32);" tactical_actor_drug_type_capacity_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(NO_PENDING_ANIMATION == 32001);" tactical_actor_animation_sentinel_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(MERC_MEDICALSPLINT == 23);" tactical_actor_pending_action_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(SKILLS_MAX == 20);" tactical_actor_skill_capacity_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(FALLINGTEST == 3);" tactical_actor_destination_mode_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(LVL_INCREASE == 0x0400);" tactical_actor_stat_mask_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(TAUNT_FLAG_MAX == 77);" tactical_actor_taunt_capacity_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(HIT_BY_SMOKEGAS == 0x10);" tactical_actor_gas_flag_test)
string(FIND "${headless_test_contents}"
  "tactical actor predicates preserve legacy classification and neutral-target rules"
  tactical_actor_predicate_behavior_test)
string(FIND "${tactical_soldier_components_header_contents}"
  "MAX_FULLTILE_DIRECTIONS =" tactical_actor_front_arc_compatibility_value)
string(FIND "${tactical_soldier_components_header_contents}"
  "MAX_BURST_SPREAD_TARGETS =" tactical_actor_spread_compatibility_value)
string(FIND "${tactical_soldier_components_header_contents}"
  "SOLDIER_UNBLIT_SIZE =" tactical_actor_unblit_compatibility_value)
string(FIND "${tactical_civ_quotes_header_contents}"
  "MAXCIVLASTNAMES = 30" tactical_civilian_name_capacity)
string(FIND "${tactical_actor_header_test_contents}"
  "static_assert(MAX_FULLTILE_DIRECTIONS == 3);"
  tactical_actor_front_arc_capacity_test)
string(FIND "${tactical_actor_header_test_contents}"
  "static_assert(MAX_BURST_SPREAD_TARGETS == 6);"
  tactical_actor_spread_capacity_test)
string(FIND "${tactical_actor_header_test_contents}"
  "static_assert(SOLDIER_UNBLIT_SIZE == 75 * 75 * 2);"
  tactical_actor_unblit_capacity_test)
string(FIND "${tactical_actor_service_api_header_test_contents}"
  "static_assert(MAXCIVLASTNAMES == 30);"
  tactical_civilian_name_capacity_test)

foreach(tactical_actor_new_contract_compile_header IN ITEMS
	"Tactical/Soldier Stat Types.h"
	"Tactical/Taunt Types.h"
	"Tactical/Render Palette Effects.h"
	"Tactical/TacticalActorAnimationGeometry.h"
	"Tactical/TacticalActorAnimationSelection.h"
	"Tactical/TacticalActorAnimationTiming.h"
	"Tactical/TacticalActorCrowBehavior.h"
    "Tactical/TacticalActorDebug.h"
    "Tactical/TacticalActorLocomotion.h"
    "Tactical/TacticalActorPredicates.h"
    "Tactical/TacticalDestinationTypes.h")
  string(FIND "${tactical_test_build_contents}"
    "\"${tactical_actor_new_contract_compile_header}\""
    tactical_actor_new_contract_compile_header_position)
  if(tactical_actor_new_contract_compile_header_position EQUAL -1)
    message(FATAL_ERROR
      "Focused actor contract ${tactical_actor_new_contract_compile_header} lost its standalone compile guard")
  endif()
endforeach()

foreach(tactical_actor_retired_facade_definition IN ITEMS
    "NO_PENDING_ANIMATION = 32001"
    "MAXBLOOD = 40"
    "MERC_OPENDOOR = 0"
    "#define SOLDIER_QUOTE_SAID_IN_SHIT"
    "SKILLS_FIRST = 0"
    "TAKE_DAMAGE_GUNFIRE = 1"
    "MTA_NONE = 0"
    "#define PTR_CIVILIAN"
    "#define CONSIDERED_NEUTRAL"
    "#define DRUG_TYPE_MAX"
    "#define IGNOREPEOPLE"
    "#define CHANGE_STAT_RECENTLY_DURATION"
    "#define TAUNT_A_CUNNING_SOLO"
    "HIT_BY_TEARGAS = 0x01"
    "#define HEALTH_INCREASE"
    "void MoveMercFacingDirection("
    "void CrowsFlyAway("
    "void DebugValidateSoldierData("
    "BOOLEAN MajorTrait("
    "struct ANIM_PROF_TILE")
  string(FIND "${tactical_soldier_control_header_contents}"
    "${tactical_actor_retired_facade_definition}"
    tactical_actor_retired_facade_definition_position)
  if(NOT tactical_actor_retired_facade_definition_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control.h regained focused contract definition '${tactical_actor_retired_facade_definition}'")
  endif()
endforeach()

if(tactical_animation_profile_tile_contract EQUAL -1 OR
   tactical_grid_direction_contract EQUAL -1 OR
   tactical_soldier_drug_capacity_contract EQUAL -1 OR
   tactical_soldier_drug_type_capacity_contract EQUAL -1 OR
   tactical_soldier_stat_duration_contract EQUAL -1 OR
   tactical_soldier_stat_mask_contract EQUAL -1 OR
   tactical_taunt_flag_contract EQUAL -1 OR
   tactical_taunt_capacity_contract EQUAL -1 OR
   tactical_actor_crow_behavior_contract EQUAL -1 OR
   tactical_actor_debug_contract EQUAL -1 OR
   tactical_actor_facing_adapter_contract EQUAL -1 OR
   tactical_actor_neutral_predicate_contract EQUAL -1 OR
   tactical_actor_predicate_animation_validation EQUAL -1 OR
   tactical_actor_predicates_build_source EQUAL -1 OR
   tactical_destination_mode_contract EQUAL -1 OR
   tactical_soldier_drug_component_include EQUAL -1 OR
   NOT tactical_soldier_drug_component_duplicate EQUAL -1 OR
   tactical_actor_animation_sentinel_contract EQUAL -1 OR
   tactical_actor_blood_capacity_contract EQUAL -1 OR
   tactical_actor_position_event_contract EQUAL -1 OR
   tactical_actor_interrupt_contract EQUAL -1 OR
   tactical_actor_movement_reason_contract EQUAL -1 OR
   tactical_actor_pending_action_contract EQUAL -1 OR
   tactical_actor_throw_action_contract EQUAL -1 OR
   tactical_actor_quote_flag_contract EQUAL -1 OR
   tactical_actor_skill_capacity_contract EQUAL -1 OR
   tactical_actor_damage_reason_contract EQUAL -1 OR
   tactical_actor_long_action_contract EQUAL -1 OR
   tactical_actor_contract_compile_list EQUAL -1 OR
   tactical_actor_contract_compile_sources EQUAL -1 OR
   tactical_actor_drug_capacity_test EQUAL -1 OR
   tactical_actor_drug_type_capacity_test EQUAL -1 OR
   tactical_actor_animation_sentinel_test EQUAL -1 OR
   tactical_actor_pending_action_test EQUAL -1 OR
   tactical_actor_skill_capacity_test EQUAL -1 OR
   tactical_actor_destination_mode_test EQUAL -1 OR
   tactical_actor_stat_mask_test EQUAL -1 OR
   tactical_actor_taunt_capacity_test EQUAL -1 OR
   tactical_actor_gas_flag_test EQUAL -1 OR
   tactical_actor_predicate_behavior_test EQUAL -1 OR
   tactical_actor_front_arc_compatibility_value EQUAL -1 OR
   tactical_actor_spread_compatibility_value EQUAL -1 OR
   tactical_actor_unblit_compatibility_value EQUAL -1 OR
   tactical_civilian_name_capacity EQUAL -1 OR
   tactical_actor_front_arc_capacity_test EQUAL -1 OR
   tactical_actor_spread_capacity_test EQUAL -1 OR
   tactical_actor_unblit_capacity_test EQUAL -1 OR
   tactical_civilian_name_capacity_test EQUAL -1)
  message(FATAL_ERROR
    "Focused actor compatibility contracts lost ownership, standalone compilation, or stable-value coverage")
endif()

set(tactical_actor_component_accessors
  identity
  roster
  vitals
  statistics
  status
  featureFlags
  inventory
  keyRing
  pendingItem
  service
  dialogue
  audio
  replication
  movementMetrics
  aiPlanning
  aiPlan
  aiBehavior
  aiCommunication
  morale
  skillState
  condition
  drugState
  statProgress
  timing
  longAction
  interaction
  pendingAction
  actionPoints
  collapseState
  perception
  awareness
  camouflage
  employment
  assignment
  deployment
  strategicPath
  vehicleState
  schedule
  position
  frontArc
  movementHistory
  pathing
  movement
  turnState
  targeting
  attackSelection
  meleeApproach
  fireControl
  combatResult
  combatContribution
  suppression
  damageDisplay
  palette
  renderState
  uiPresentation
  animationIntent
  animationPlayback
  animationActivity
  animationCache
  renderBindings
  runtime)

foreach(component IN LISTS tactical_actor_component_accessors)
  string(REGEX MATCH
    "[A-Za-z_][A-Za-z0-9_:<>]*&[ \t]+${component}\\(\\) noexcept"
    mutable_component_accessor
    "${tactical_actor_contents}")
  string(REGEX MATCH
    "const[ \t]+[A-Za-z_][A-Za-z0-9_:<>]*&[ \t]+${component}\\(\\) const noexcept"
    const_component_accessor
    "${tactical_actor_contents}")
  string(REGEX MATCH
    "[\r\n][ \t]+[A-Za-z_][A-Za-z0-9_:<>]*[ \t]+${component}_[ \t]*;"
    private_component_owner
    "${tactical_actor_contents}")
  string(FIND "${tactical_actor_aggregate_source_contents}"
    "${component}().reset();"
    component_reset)
  if(NOT mutable_component_accessor OR
     NOT const_component_accessor OR
     NOT private_component_owner OR
     component_reset EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor component '${component}' lost its mutable/const accessor, private owner, or explicit initialization reset")
  endif()
endforeach()

string(FIND "${tactical_actor_aggregate_source_contents}"
  "void TacticalActor::initialize"
  tactical_actor_initialize_begin)
string(FIND "${tactical_actor_aggregate_source_contents}"
  "UINT8 tmpuser"
  tactical_actor_initialize_end)
if(tactical_actor_initialize_begin EQUAL -1 OR
   tactical_actor_initialize_end EQUAL -1 OR
   tactical_actor_initialize_end LESS tactical_actor_initialize_begin)
  message(FATAL_ERROR
    "Could not locate TacticalActor's explicit component initialization boundary")
endif()
math(EXPR tactical_actor_initialize_length
  "${tactical_actor_initialize_end} - ${tactical_actor_initialize_begin}")
string(SUBSTRING "${tactical_actor_aggregate_source_contents}"
  ${tactical_actor_initialize_begin} ${tactical_actor_initialize_length}
  tactical_actor_initialize_contents)
string(REGEX MATCH
  "(memset|memcpy)[ \t\r\n]*\\("
  tactical_actor_raw_initialization
  "${tactical_actor_initialize_contents}")
if(tactical_actor_raw_initialization)
  message(FATAL_ERROR
    "TacticalActor initialization must reset typed components, never raw object bytes")
endif()

foreach(retired_layout_fragment IN ITEMS
  "OLDSOLDIERTYPE_101"
  "SIZEOF_SOLDIERTYPE_POD"
  "SIZEOF_OLDSOLDIERTYPE_101_POD"
  "compatibilityBytes"
  "retiredFaceIndexSlot_"
  "retiredLevelNodeSlot_"
  "retiredExternShadowLevelNodeSlot_"
  "retiredRoofUiLevelNodeSlot_"
  "retiredBackgroundSlot_"
  "retiredZBackgroundSlot_"
  "retiredAnimationTileSlot_")
  string(FIND
    "${tactical_soldier_control_header_contents}${tactical_actor_header_contents}${tactical_actor_aggregate_source_contents}${tactical_actor_source_contents}${tactical_actor_persistence_source_contents}${headless_test_contents}"
    "${retired_layout_fragment}"
    retired_layout_fragment_site)
  if(NOT retired_layout_fragment_site EQUAL -1)
    message(FATAL_ERROR
      "Retired TacticalActor layout/conversion fragment '${retired_layout_fragment}' returned")
  endif()
endforeach()

string(FIND "${tactical_actor_contents}"
  "endOfPOD"
  tactical_actor_pod_marker)
if(NOT tactical_actor_pod_marker EQUAL -1)
  message(FATAL_ERROR
    "TacticalActor regained an in-memory serialization boundary; the save schema must remain explicit")
endif()

foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" tactical_actor_name_scan_contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])SOLDIERTYPE([^A-Za-z0-9_]|$)"
    retired_soldier_type_name
    "${tactical_actor_name_scan_contents}")
  if(retired_soldier_type_name)
    message(FATAL_ERROR
      "Retired SOLDIERTYPE source name returned in ${source_file}; use TacticalActor")
  endif()
endforeach()

foreach(retired_persistence_method IN ITEMS
  "TacticalActor::Save"
  "TacticalActor::Load"
  "TacticalActor::GetChecksum"
  "XferSoldierTypePOD")
  string(FIND
    "${tactical_soldier_control_header_contents}${tactical_actor_header_contents}${tactical_actor_source_contents}${tactical_actor_persistence_source_contents}"
    "${retired_persistence_method}"
    retired_persistence_method_site)
  if(NOT retired_persistence_method_site EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained persistence facade '${retired_persistence_method}'")
  endif()
endforeach()

foreach(retired_condition_method IN ITEMS
  "IsZombie"
  "IsAssassin"
  "CanBeCaptured"
  "IsCowering"
  "IsUnconscious"
  "IsGivingAid"
  "TakenLargeHit"
  "ShockLevelPercent")
  string(FIND "${tactical_actor_contents}"
    "${retired_condition_method}("
    retired_condition_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_condition_method}"
    retired_condition_definition)
  if(NOT retired_condition_declaration EQUAL -1 OR
     NOT retired_condition_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained condition facade '${retired_condition_method}'")
  endif()
endforeach()

string(FIND "${tactical_actor_conditions_header_contents}"
  "bandagedAmount(" tactical_actor_bandaged_amount_declaration)
string(FIND "${tactical_actor_conditions_source_contents}"
  "bandagedAmount(" tactical_actor_bandaged_amount_definition)
string(FIND "${headless_test_contents}"
  "TacticalActorConditions::bandagedAmount"
  tactical_actor_bandaged_amount_coverage)
string(FIND "${headless_test_contents}"
  "BANDAGED( &target ) == 15" tactical_actor_bandaged_adapter_coverage)
if(tactical_actor_bandaged_amount_declaration EQUAL -1 OR
   tactical_actor_bandaged_amount_definition EQUAL -1 OR
   tactical_actor_bandaged_amount_coverage EQUAL -1 OR
   tactical_actor_bandaged_adapter_coverage EQUAL -1)
  message(FATAL_ERROR
    "Bandaged-health calculation lost its focused conditions owner or compatibility coverage")
endif()

foreach(required_condition_query IN ITEMS
  "isZombie"
  "isAssassin"
  "canBeCaptured"
  "isCowering"
  "isUnconscious"
  "isGivingAid"
  "hasTakenLargeHit"
  "suppressionShockPercent")
  string(FIND "${tactical_actor_conditions_header_contents}"
    "${required_condition_query}(const TacticalActor& actor)"
    condition_query_declaration)
  string(FIND "${tactical_actor_conditions_source_contents}"
    "${required_condition_query}(const TacticalActor& actor)"
    condition_query_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorConditions::${required_condition_query}"
    condition_query_coverage)
  if(condition_query_declaration EQUAL -1 OR
     condition_query_definition EQUAL -1 OR
     condition_query_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor condition query '${required_condition_query}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

string(FIND "${tactical_build_contents}"
  "TacticalActorConditions.cpp"
  tactical_actor_conditions_build_entry)
if(tactical_actor_conditions_build_entry EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor conditions must remain a compiled tactical domain boundary")
endif()

foreach(retired_covert_method IN ITEMS
  "LooksLikeACivilian"
  "LooksLikeASoldier"
  "GetUniformType"
  "EquipmentTooGood"
  "SeemsLegit"
  "RecognizeAsCombatant"
  "LooseDisguise"
  "Disguise"
  "ApplyCovert"
  "Strip"
  "SpySelfTest"
  "GetUncoverRisk"
  "GetIntelGain")
  string(FIND "${tactical_actor_contents}"
    "${retired_covert_method}("
    retired_covert_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_covert_method}"
    retired_covert_definition)
  if(NOT retired_covert_declaration EQUAL -1 OR
     NOT retired_covert_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained covert-operations facade '${retired_covert_method}'")
  endif()
endforeach()

foreach(required_covert_operation IN ITEMS
  "looksLikeCivilian"
  "looksLikeSoldier"
  "uniformType"
  "equipmentTooGood"
  "seemsLegitimate"
  "recognizesCombatant"
  "loseDisguise"
  "disguise"
  "applyCovert"
  "strip"
  "runSelfTest"
  "uncoverRisk"
  "intelGain")
  string(FIND "${tactical_actor_covert_header_contents}"
    "${required_covert_operation}("
    covert_operation_declaration)
  string(FIND "${tactical_actor_covert_source_contents}"
    "TacticalActorCovertOps::${required_covert_operation}("
    covert_operation_definition)
  if(covert_operation_declaration EQUAL -1 OR
     covert_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor covert operation '${required_covert_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_covert_coverage IN ITEMS
  "looksLikeCivilian"
  "looksLikeSoldier"
  "uniformType"
  "equipmentTooGood"
  "seemsLegitimate"
  "recognizesCombatant"
  "uncoverRisk"
  "intelGain")
  string(FIND "${headless_test_contents}"
    "TacticalActorCovertOps::${required_covert_coverage}"
    covert_operation_coverage)
  if(covert_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor covert operations lost bounded headless coverage for '${required_covert_coverage}'")
  endif()
endforeach()

foreach(retired_dragging_method IN ITEMS
  "CanDragInPrinciple"
  "CanDragPerson"
  "CanDragCorpse"
  "CanDragStructure"
  "IsDragging"
  "SetDragOrderPerson"
  "SetDragOrderCorpse"
  "SetDragOrderStructure"
  "CancelDrag"
  "CanStartDrag"
  "StartDrag")
  string(FIND "${tactical_actor_contents}"
    "${retired_dragging_method}("
    retired_dragging_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_dragging_method}"
    retired_dragging_definition)
  if(NOT retired_dragging_declaration EQUAL -1 OR
     NOT retired_dragging_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained dragging facade '${retired_dragging_method}'")
  endif()
endforeach()

foreach(required_dragging_operation IN ITEMS
  "canDrag"
  "canDragPerson"
  "canDragCorpse"
  "canDragStructure"
  "isDragging"
  "dragPerson"
  "dragCorpse"
  "dragStructure"
  "cancel"
  "canStart"
  "start")
  string(FIND "${tactical_actor_dragging_header_contents}"
    "${required_dragging_operation}("
    dragging_operation_declaration)
  string(FIND "${tactical_actor_dragging_source_contents}"
    "TacticalActorDragging::${required_dragging_operation}("
    dragging_operation_definition)
  if(dragging_operation_declaration EQUAL -1 OR
     dragging_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor dragging operation '${required_dragging_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_dragging_coverage IN ITEMS
  "canDrag"
  "canDragPerson"
  "canDragCorpse"
  "canDragStructure"
  "isDragging"
  "dragPerson"
  "dragCorpse"
  "dragStructure"
  "cancel"
  "canStart"
  "start")
  string(FIND "${headless_test_contents}"
    "TacticalActorDragging::${required_dragging_coverage}"
    dragging_operation_coverage)
  if(dragging_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor dragging lost malformed-state coverage for '${required_dragging_coverage}'")
  endif()
endforeach()

foreach(retired_disease_method IN ITEMS
  "Infect"
  "AddDiseasePoints"
  "AnnounceDisease"
  "AddDisability"
  "CanReceiveSplint"
  "HasDisease"
  "HasDiseaseWithFlag"
  "GetDiseaseMagnitude"
  "PrintDiseaseDesc"
  "GetDiseaseContactProtection"
  "GetDiseaseResistance"
  "GetDiseaseDiagnosePoints")
  string(FIND "${tactical_actor_contents}"
    "${retired_disease_method}("
    retired_disease_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_disease_method}"
    retired_disease_definition)
  if(NOT retired_disease_declaration EQUAL -1 OR
     NOT retired_disease_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained disease facade '${retired_disease_method}'")
  endif()
endforeach()

foreach(required_disease_operation IN ITEMS
  "infect"
  "addPoints"
  "announce"
  "addDisability"
  "canReceiveSplint"
  "hasAny"
  "hasOutbreakProperty"
  "magnitude"
  "contactProtection"
  "resistance"
  "diagnosisPoints")
  string(FIND "${tactical_actor_disease_header_contents}"
    "${required_disease_operation}("
    disease_operation_declaration)
  string(FIND "${tactical_actor_disease_source_contents}"
    "TacticalActorDisease::${required_disease_operation}("
    disease_operation_definition)
  if(disease_operation_declaration EQUAL -1 OR
     disease_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor disease operation '${required_disease_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_disease_flag IN ITEMS
  "diagnosedFlag"
  "outbreakFlag"
  "reversingFlag"
  "legSplintFlag"
  "armSplintFlag")
  string(FIND "${tactical_actor_disease_header_contents}"
    "${required_disease_flag}"
    disease_flag_declaration)
  if(disease_flag_declaration EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor disease state lost '${required_disease_flag}'")
  endif()
endforeach()

foreach(retired_disease_flag IN ITEMS
  "SOLDIERDISEASE_DIAGNOSED"
  "SOLDIERDISEASE_OUTBREAK"
  "SOLDIERDISEASE_REVERSEAL"
  "SOLDIERDISEASE_SPLINTAPPLIED_LEG"
  "SOLDIERDISEASE_SPLINTAPPLIED_ARM")
  string(FIND
    "${tactical_soldier_control_header_contents}${tactical_actor_header_contents}${tactical_actor_source_contents}"
    "${retired_disease_flag}"
    retired_disease_flag_site)
  if(NOT retired_disease_flag_site EQUAL -1)
    message(FATAL_ERROR
      "Retired tactical actor disease flag '${retired_disease_flag}' returned")
  endif()
endforeach()

foreach(required_disease_coverage IN ITEMS
  "infect"
  "addPoints"
  "announce"
  "addDisability"
  "canReceiveSplint"
  "hasAny"
  "hasOutbreakProperty"
  "magnitude"
  "contactProtection")
  string(FIND "${headless_test_contents}"
    "TacticalActorDisease::${required_disease_coverage}"
    disease_operation_coverage)
  if(disease_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor disease lost headless coverage for '${required_disease_coverage}'")
  endif()
endforeach()

foreach(retired_assignment_method IN ITEMS
  "GetSleepBreathRegeneration"
  "GetBurialPoints"
  "GetConstructionPoints"
  "GetAdministrationPoints"
  "GetAdministrationModifier"
  "GetExplorationPoints")
  string(FIND "${tactical_actor_contents}"
    "${retired_assignment_method}("
    retired_assignment_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_assignment_method}"
    retired_assignment_definition)
  if(NOT retired_assignment_declaration EQUAL -1 OR
     NOT retired_assignment_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained assignment-productivity facade '${retired_assignment_method}'")
  endif()
endforeach()

foreach(required_assignment_operation IN ITEMS
  "sleepBreathRegeneration"
  "burialPoints"
  "constructionPoints"
  "administrationPoints"
  "administrationModifier"
  "explorationPoints")
  string(FIND "${tactical_actor_assignments_header_contents}"
    "${required_assignment_operation}("
    assignment_operation_declaration)
  string(FIND "${tactical_actor_assignments_source_contents}"
    "TacticalActorAssignments::${required_assignment_operation}("
    assignment_operation_definition)
  if(assignment_operation_declaration EQUAL -1 OR
     assignment_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor assignment operation '${required_assignment_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_assignment_coverage IN ITEMS
  "sleepBreathRegeneration"
  "burialPoints"
  "constructionPoints"
  "administrationPoints"
  "administrationModifier"
  "explorationPoints")
  string(FIND "${headless_test_contents}"
    "TacticalActorAssignments::${required_assignment_coverage}"
    assignment_operation_coverage)
  if(assignment_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor assignment rules lost headless coverage for '${required_assignment_coverage}'")
  endif()
endforeach()

foreach(retired_modifier_method IN ITEMS
  "HasBackgroundFlag"
  "GetBackgroundValue"
  "GetBackgroundValueVector"
  "GetSuppressionResistanceBonus"
  "GetMeleeDamageBonus"
  "GetAPBonus"
  "GetFearResistanceBonus"
  "GetMoraleThreshold"
  "GetMoraleModifier"
  "GetInterruptModifier"
  "GetDamageResistance"
  "GetHearingBonus"
  "GetSightRangeBonus"
  "GetSurrenderStrength"
  "GetTraitCTHModifier"
  "GetBodyWeight"
  "GetWaterSnakeDefenseChance"
  "GetInteractiveActionSkill"
  "GetThiefStealMoneyChance"
  "GetThiefEvadeDetectionChance")
  string(FIND "${tactical_actor_contents}"
    "${retired_modifier_method}("
    retired_modifier_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_modifier_method}"
    retired_modifier_definition)
  if(NOT retired_modifier_declaration EQUAL -1 OR
     NOT retired_modifier_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained modifier facade '${retired_modifier_method}'")
  endif()
endforeach()

foreach(required_modifier_operation IN ITEMS
  "hasBackgroundFlag"
  "backgroundValue"
  "backgroundValues"
  "suppressionResistanceBonus"
  "meleeDamageBonus"
  "actionPointBonus"
  "fearResistanceBonus"
  "moraleModifier"
  "interruptModifier"
  "damageResistance"
  "hearingBonus"
  "sightRangeBonus"
  "surrenderStrength"
  "traitChanceToHitModifier"
  "bodyWeight"
  "waterSnakeDefenseChance"
  "interactiveActionSkill"
  "thiefStealMoneyChance"
  "thiefEvadeDetectionChance")
  string(FIND "${tactical_actor_modifiers_header_contents}"
    "${required_modifier_operation}("
    modifier_operation_declaration)
  string(FIND "${tactical_actor_modifiers_source_contents}"
    "TacticalActorModifiers::${required_modifier_operation}("
    modifier_operation_definition)
  if(modifier_operation_declaration EQUAL -1 OR
     modifier_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor modifier operation '${required_modifier_operation}' lost its domain declaration or definition")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::backgroundValue"
  modifier_background_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::actionPointBonus"
  modifier_derived_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::surrenderStrength"
  modifier_malformed_vitals_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::traitChanceToHitModifier"
  modifier_malformed_identity_coverage)
string(FIND "${headless_test_contents}"
  "TacticalActorModifiers::interactiveActionSkill"
  modifier_interaction_coverage)
if(modifier_background_coverage EQUAL -1 OR
   modifier_derived_coverage EQUAL -1 OR
   modifier_malformed_vitals_coverage EQUAL -1 OR
   modifier_malformed_identity_coverage EQUAL -1 OR
   modifier_interaction_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor modifiers lost their malformed-data or data-free headless coverage")
endif()

foreach(retired_equipment_method IN ITEMS
  "SoldierCarriesTwoHandedWeapon"
  "GetUsedWeapon"
  "GetUsedWeaponNumber"
  "IsFeedingExternal"
  "GetObjectWithFlag"
  "UsesScubaGear"
  "GetBestEquippedFlashLightRange"
  "HasItem"
  "GetEquippedRiotShield"
  "IsRiotShieldEquipped"
  "GetObjectWithItemFlag"
  "HasItemInInventory"
  "SoldierInventoryCoolDown"
  "DropSectorEquipment"
  "TakeNewItemFromInventory"
  "TakeNewBombFromInventory"
  "SwitchWeapons"
  "HandleFlashLights"
  "AddBestFlashLight"
  "DestroyEquippedRiotShield"
  "RiotShieldTakeDamage"
  "DestroyOneItemInInventory")
  string(FIND "${tactical_actor_contents}"
    "${retired_equipment_method}("
    retired_equipment_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_equipment_method}"
    retired_equipment_definition)
  if(NOT retired_equipment_declaration EQUAL -1 OR
     NOT retired_equipment_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained equipment facade '${retired_equipment_method}'")
  endif()
endforeach()

foreach(required_equipment_operation IN ITEMS
  "carriesTwoHandedWeapon"
  "usedWeapon"
  "usedWeaponNumber"
  "externalFeeding"
  "objectWithFlag"
  "usesScubaGear"
  "bestEquippedFlashlightRange"
  "hasItem"
  "hasMortar"
  "hasSniperRifle"
  "equippedRiotShield"
  "hasEquippedRiotShield"
  "coolDownInventory"
  "dropSectorEquipment"
  "takeItemIntoHand"
  "takeBombIntoHand"
  "switchWeapon"
  "refreshFlashlights"
  "damageRiotShield"
  "removeOneItem")
  string(FIND "${tactical_actor_equipment_header_contents}"
    "${required_equipment_operation}("
    equipment_operation_declaration)
  string(FIND "${tactical_actor_equipment_source_contents}"
    "TacticalActorEquipment::${required_equipment_operation}("
    equipment_operation_definition)
  if(equipment_operation_declaration EQUAL -1 OR
     equipment_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor equipment operation '${required_equipment_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_equipment_coverage IN ITEMS
  "TacticalActorEquipment::usedWeapon"
  "TacticalActorEquipment::externalFeeding"
  "TacticalActorEquipment::objectWithFlag"
  "TacticalActorEquipment::hasMortar"
  "TacticalActorEquipment::hasSniperRifle"
  "TacticalActorEquipment::equippedRiotShield"
  "TacticalActorEquipment::dropSectorEquipment"
  "TacticalActorEquipment::switchWeapon"
  "TacticalActorEquipment::refreshFlashlights"
  "TacticalActorEquipment::damageRiotShield"
  "TacticalActorEquipment::removeOneItem")
  string(FIND "${headless_test_contents}"
    "${required_equipment_coverage}"
    equipment_operation_coverage)
  if(equipment_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor equipment lost data-free or malformed-input coverage for '${required_equipment_coverage}'")
  endif()
endforeach()

foreach(retired_actor_facade IN ITEMS
  "InventoryExplosion"
  "SelfDetonate"
  "StopChatting"
  "PlayerSoldierStartTalking"
  "DrugAutoUse"
  "IsValidBloodDonor"
  "CanUseRadio"
  "UseRadio"
  "HasMortar"
  "CanAnyArtilleryStrikeBeOrdered"
  "OrderArtilleryStrike"
  "IsJamming"
  "JamCommunications"
  "IsScanning"
  "ScanForJam"
  "IsRadioListening"
  "RadioListen"
  "RadioCallReinforcements"
  "SwitchOffRadio"
  "RadioOrderAllTurnCoatToSwitchSides"
  "RadioFail"
  "IsSpotting"
  "CanSpot"
  "BecomeSpotter"
  "HasSniper"
  "CanUseSkill"
  "UseSkill"
  "PrintSkillDesc"
  "InPositionForTurncoatAttempt"
  "GetTurncoatConvinctionChance"
  "AttemptToCreateTurncoat"
  "OrderTurnCoatToSwitchSides"
  "OrderAllTurnCoatToSwitchSides"
  "UpdateRobotControllerGivenController"
  "UpdateRobotControllerGivenRobot"
  "GetRobotController"
  "CanRobotBeControlled"
  "ControllingRobot"
  "MercInWater"
  "MercInShallowWater"
  "MercInDeepWater"
  "MercInHighWater"
  "GetNewSoldierStateFromNewStance"
  "GetMoveStateBasedOnStance"
  "CanClimbWithCurrentBackpack"
  "InternalIsValidStance"
  "IsCrouchedAgainstCoverFromDir"
  "IsFastMovement"
  "IsValidSecondHandShot"
  "IsValidSecondHandBurst"
  "IsValidSecondHandShotForReloadingPurposes"
  "IsValidAlternativeFireMode"
  "IsValidShotFromHip"
  "IsValidPistolFastShot"
  "IsWeaponMounted"
  "SetSoldierAsUnderAiControl"
  "CheckInitialAP"
  "IsFlanking"
  "StopCoweringAnimation"
  "RetreatCounterStart"
  "RetreatCounterValue"
  "StartRadioAnimation"
  "DeleteBoxingFlag"
  "CanProcessPrisoners"
  "FreePrisoner"
  "GetMultiTurnAction"
  "StartMultiTurnAction"
  "CancelMultiTurnAction"
  "UpdateMultiTurnAction"
  "SoldierTakeDelayedDamage"
  "ResolveDelayedDamage"
  "CanMedicAI"
  "AIDoctorFriend"
  "AIDoctorSelf"
  "ReceivingSoldierCancelServices"
  "GivingSoldierCancelServices"
  "InternalReceivingSoldierCancelServices"
  "InternalGivingSoldierCancelServices"
  "SoldierDressWound"
  "VirtualSoldierDressWound"
  "NumberOfDamagedStats"
  "RegainDamagedStats"
  "EVENT_SoldierBeginFirstAid"
  "EVENT_SoldierBeginCutFence"
  "EVENT_SoldierBeginRepair"
  "EVENT_SoldierBeginRefuel"
  "EVENT_SoldierBeginReloadRobot"
  "EVENT_SoldierBeginTakeBlood"
  "EVENT_SoldierBeginAttachCan"
  "EVENT_SoldierBuildStructure"
  "EVENT_SoldierInteractiveAction"
  "BreakWindow"
  "CanBreakWindow"
  "EVENT_SoldierBeginGiveItem"
  "EVENT_SoldierHandcuffPerson"
  "EVENT_SoldierApplyItemToPerson"
  "EVENT_SoldierTakeBloodFromPerson"
  "EVENT_SoldierApplySplintToPerson"
  "EVENT_SoldierBeginDropBomb"
  "EVENT_SoldierDefuseTripwire"
  "EVENT_SoldierBeginUseDetonator"
  "EVENT_SoldierBeginBladeAttack"
  "EVENT_SoldierBeginPunchAttack"
  "EVENT_SoldierBeginKnifeThrowAttack"
  "DoNinjaAttack"
  "BeginTyingToFall"
  "ChangeToFlybackAnimation"
  "ChangeToFallbackAnimation"
  "SetSoldierCowerState"
  "HandleSoldierTakeDamageFeedback"
  "GetSoldierProfileType"
  "ResetExtraStats"
  "ResetSoldierChangeStatTimer"
  "InitializeExtraData"
  "PickDropItemAnimation"
  "SoldierPropertyUpkeep"
  "PrintFoodDesc"
  "PrintSleepDesc"
  "EVENT_FireSoldierWeapon"
  "SoldierReadyWeapon"
  "InternalSoldierReadyWeapon"
  "ReLoadSoldierAnimationDueToHandItemChange"
  "InternalRemoveSoldierFromGridNo"
  "RemoveSoldierFromGridNo"
  "EVENT_InternalSetSoldierPosition"
  "EVENT_SetSoldierPosition"
  "EVENT_SetSoldierPositionForceDelete"
  "InternalSetSoldierHeight"
  "SetSoldierHeight"
  "SetSoldierGridNo"
  "AdjustNoAPToFinishMove"
  "EVENT_InternalGetNewSoldierPath"
  "EVENT_GetNewSoldierPath"
  "StopSoldier"
  "SoldierGotoStationaryStance"
  "HaultSoldierFromSighting"
  "EVENT_StopMerc"
  "EVENT_SetSoldierDestination"
  "EVENT_InternalSetSoldierDestination"
  "EVENT_SetSoldierDesiredDirection"
  "EVENT_SetSoldierDirection"
  "TurnSoldier"
  "ChangeSoldierStance"
  "BeginSoldierGetup"
  "CheckForBreathCollapse"
  "BeginSoldierClimbUpRoof"
  "BeginSoldierClimbDownRoof"
  "BeginSoldierClimbFence"
  "BeginSoldierClimbWall"
  "BeginSoldierClimbWindow"
  "BeginSoldierClimbWallUp"
  "CreateSoldierLight"
  "ReCreateSoldierLight"
  "DeleteSoldierLight"
  "PositionSoldierLight"
  "SetCheckSoldierLightFlag"
  "CalcActionPoints"
  "CalcNewActionPoints"
  "CryoAniFrame"
  "ConvertAniCodeToAniFrame"
  "SpriteDirForSurface"
  "HandleAnimationProfile"
  "GetProfileFlagsFromGridno"
  "CreateSoldierCommon"
  "DeleteSoldier"
  "ReviveSoldier"
  "CreateSoldierPalettes"
  "ChangeSoldierState"
  "EVENT_InitNewSoldierAnim"
  "EVENT_SoldierGotHit"
  "SoldierTakeDamage"
  "EVENT_BeginMercTurn"
  "DoMercBattleSound"
  "InternalDoMercBattleSound"
  "CheckSoldierHitRoof"
  "MoveMerc"
  "GetMaxDistanceVisible")
  string(FIND "${tactical_actor_contents}"
    "${retired_actor_facade}("
    retired_actor_facade_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "TacticalActor::${retired_actor_facade}"
    retired_actor_facade_definition)
  if(NOT retired_actor_facade_declaration EQUAL -1 OR
     NOT retired_actor_facade_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor regained retired facade '${retired_actor_facade}'")
  endif()
endforeach()

foreach(actor_source IN LISTS world_state_files)
  file(READ "${actor_source}" actor_source_contents)
  foreach(retired_actor_entry IN ITEMS
    "CreateSoldierCommon"
    "DeleteSoldier"
    "ReviveSoldier"
    "CreateSoldierPalettes"
    "ChangeSoldierState"
    "EVENT_InitNewSoldierAnim"
    "EVENT_SoldierGotHit"
    "SoldierTakeDamage"
    "EVENT_BeginMercTurn"
    "DoMercBattleSound"
    "InternalDoMercBattleSound"
    "CheckSoldierHitRoof"
    "MoveMerc"
    "GetMaxDistanceVisible"
    "InitSightRange"
    "AdjustMaxSightRangeForEnvEffects"
    "MaxNormalDistanceVisible"
    "DistanceVisible"
    "SoldierHasLimitedVision"
    "MaxDistanceVisible")
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_actor_entry}[ \t\r\n]*\\("
      retired_actor_entry_usage
      "${actor_source_contents}")
    if(retired_actor_entry_usage)
      message(FATAL_ERROR
        "Production caller in ${actor_source} restored retired TacticalActor entry '${retired_actor_entry}'")
    endif()
  endforeach()
endforeach()

foreach(visibility_declaration_source IN LISTS world_state_declaration_files)
  file(READ "${visibility_declaration_source}"
    visibility_declaration_source_contents)
  string(REGEX MATCH
    "(^|[\r\n])[ \t]*(extern[ \t]+)?INT8[ \t]+(gbLookDistance|BEHIND|SBEHIND|SIDE|ANGLE|STRAIGHT)([^A-Za-z0-9_]|$)"
    retired_visibility_state_declaration
    "${visibility_declaration_source_contents}")
  if(retired_visibility_state_declaration)
    message(FATAL_ERROR
      "Production source ${visibility_declaration_source} restored retired global tactical visibility state")
  endif()
endforeach()

foreach(final_actor_domain_operation IN ITEMS
  "TacticalActorAppearance|tactical_actor_appearance_header_contents|tactical_actor_appearance_source_contents|rebuildPalettes"
  "TacticalActorLifecycle|tactical_actor_lifecycle_header_contents|tactical_actor_lifecycle_source_contents|create"
  "TacticalActorLifecycle|tactical_actor_lifecycle_header_contents|tactical_actor_lifecycle_source_contents|destroy"
  "TacticalActorLifecycle|tactical_actor_lifecycle_header_contents|tactical_actor_lifecycle_source_contents|revive"
  "TacticalActorLifecycle|tactical_actor_lifecycle_header_contents|tactical_actor_lifecycle_source_contents|revivePlayerTeam"
  "TacticalActorAnimationTransitions|tactical_actor_animation_transitions_header_contents|tactical_actor_animation_transitions_source_contents|changeState"
  "TacticalActorAnimationTransitions|tactical_actor_animation_transitions_header_contents|tactical_actor_animation_transitions_source_contents|initializeAnimation"
  "TacticalActorDamageResolution|tactical_actor_damage_resolution_header_contents|tactical_actor_damage_resolution_source_contents|applyHit"
  "TacticalActorDamageResolution|tactical_actor_damage_resolution_header_contents|tactical_actor_damage_resolution_source_contents|takeDamage"
  "TacticalActorTurnLifecycle|tactical_actor_turn_lifecycle_header_contents|tactical_actor_turn_lifecycle_source_contents|beginTurn"
  "TacticalActorBattleSounds|tactical_actor_battle_sounds_header_contents|tactical_actor_battle_sounds_source_contents|play"
  "TacticalActorBattleSounds|tactical_actor_battle_sounds_header_contents|tactical_actor_battle_sounds_source_contents|playWithCode"
  "TacticalActorBattleSounds|tactical_actor_battle_sounds_header_contents|tactical_actor_battle_sounds_source_contents|preload"
  "TacticalActorLocomotion|tactical_actor_locomotion_header_contents|tactical_actor_locomotion_source_contents|checkRoofHit"
  "TacticalActorLocomotion|tactical_actor_locomotion_header_contents|tactical_actor_locomotion_source_contents|move"
  "TacticalActorVisibility|tactical_actor_visibility_header_contents|tactical_actor_visibility_source_contents|initializeRanges"
  "TacticalActorVisibility|tactical_actor_visibility_header_contents|tactical_actor_visibility_source_contents|straightRange"
  "TacticalActorVisibility|tactical_actor_visibility_header_contents|tactical_actor_visibility_source_contents|normalMaximumDistance"
  "TacticalActorVisibility|tactical_actor_visibility_header_contents|tactical_actor_visibility_source_contents|hasLimitedVision"
  "TacticalActorVisibility|tactical_actor_visibility_header_contents|tactical_actor_visibility_source_contents|adjustForEnvironment"
  "TacticalActorVisibility|tactical_actor_visibility_header_contents|tactical_actor_visibility_source_contents|distance"
  "TacticalActorVisibility|tactical_actor_visibility_header_contents|tactical_actor_visibility_source_contents|maximumDistance")
  string(REPLACE "|" ";" final_actor_domain_fields
    "${final_actor_domain_operation}")
  list(GET final_actor_domain_fields 0 final_actor_domain)
  list(GET final_actor_domain_fields 1 final_actor_header_variable)
  list(GET final_actor_domain_fields 2 final_actor_source_variable)
  list(GET final_actor_domain_fields 3 final_actor_operation)
  string(FIND "${${final_actor_header_variable}}"
    "${final_actor_operation}("
    final_actor_operation_declaration)
  string(FIND "${${final_actor_source_variable}}"
    "${final_actor_domain}::${final_actor_operation}("
    final_actor_operation_definition)
  string(FIND "${headless_test_contents}"
    "${final_actor_domain}::${final_actor_operation}"
    final_actor_operation_coverage)
  if(final_actor_operation_declaration EQUAL -1 OR
     final_actor_operation_definition EQUAL -1 OR
     final_actor_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Final tactical actor operation '${final_actor_domain}::${final_actor_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_route_execution_operation IN ITEMS
  "setOutOfActionPoints"
  "requestPath"
  "continueMovement"
  "stop"
  "settleIntoStationaryStance"
  "haltForSighting"
  "stopAt")
  string(FIND "${tactical_actor_route_execution_header_contents}"
    "${required_route_execution_operation}("
    route_execution_operation_declaration)
  string(FIND "${tactical_actor_route_execution_source_contents}"
    "TacticalActorRouteExecution::${required_route_execution_operation}("
    route_execution_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorRouteExecution::${required_route_execution_operation}("
    route_execution_operation_coverage)
  if(route_execution_operation_declaration EQUAL -1 OR
     route_execution_operation_definition EQUAL -1 OR
     route_execution_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor route-execution operation '${required_route_execution_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_orientation_operation IN ITEMS
  "changeStance"
  "setMovementDestination"
  "setDesiredDirection"
  "setDirection"
  "advanceTurn")
  string(FIND "${tactical_actor_orientation_header_contents}"
    "${required_orientation_operation}("
    orientation_operation_declaration)
  string(FIND "${tactical_actor_orientation_source_contents}"
    "TacticalActorOrientation::${required_orientation_operation}("
    orientation_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorOrientation::${required_orientation_operation}("
    orientation_operation_coverage)
  if(orientation_operation_declaration EQUAL -1 OR
     orientation_operation_definition EQUAL -1 OR
     orientation_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor orientation operation '${required_orientation_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(orientation_source IN LISTS world_state_files)
  file(READ "${orientation_source}"
    orientation_source_contents)
  foreach(retired_orientation_call IN ITEMS
    "EVENT_SetSoldierDestination"
    "EVENT_InternalSetSoldierDestination"
    "EVENT_SetSoldierDesiredDirection"
    "EVENT_InternalSetSoldierDesiredDirection"
    "EVENT_SetSoldierDirection"
    "TurnSoldier"
    "ChangeSoldierStance"
    "MultiTiledTurnDirection")
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_orientation_call}[ \t\r\n]*\\("
      retired_orientation_usage
      "${orientation_source_contents}")
    if(retired_orientation_usage)
      message(FATAL_ERROR
        "Production caller in ${orientation_source} restored retired orientation entry '${retired_orientation_call}'")
    endif()
  endforeach()
endforeach()

foreach(route_execution_source IN LISTS world_state_files)
  file(READ "${route_execution_source}"
    route_execution_source_contents)
  foreach(retired_route_execution_call IN ITEMS
    "AdjustNoAPToFinishMove"
    "EVENT_InternalGetNewSoldierPath"
    "EVENT_GetNewSoldierPath"
    "StopSoldier"
    "SoldierGotoStationaryStance"
    "HaultSoldierFromSighting"
    "EVENT_StopMerc")
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_route_execution_call}[ \t\r\n]*\\("
      retired_route_execution_usage
      "${route_execution_source_contents}")
    if(retired_route_execution_usage)
      message(FATAL_ERROR
        "Production caller in ${route_execution_source} restored retired route-execution entry '${retired_route_execution_call}'")
    endif()
  endforeach()
endforeach()

string(FIND "${tactical_actor_source_contents}"
  "EVENT_SetSoldierPositionAndMaybeFinalDestAndMaybeNotDestination("
  retired_actor_position_wrapper)
if(NOT retired_actor_position_wrapper EQUAL -1)
  message(FATAL_ERROR
    "Retired global tactical actor position wrapper returned; use TacticalActorWorldPlacement::setPosition")
endif()

foreach(required_world_placement_operation IN ITEMS
  "removeFromGrid"
  "setPosition"
  "setHeight"
  "setGrid")
  string(FIND "${tactical_actor_world_placement_header_contents}"
    "${required_world_placement_operation}("
    world_placement_operation_declaration)
  string(FIND "${tactical_actor_world_placement_source_contents}"
    "TacticalActorWorldPlacement::${required_world_placement_operation}("
    world_placement_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorWorldPlacement::${required_world_placement_operation}("
    world_placement_operation_coverage)
  if(world_placement_operation_declaration EQUAL -1 OR
     world_placement_operation_definition EQUAL -1 OR
     world_placement_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor world-placement operation '${required_world_placement_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

set(world_placement_production_sources ${world_state_files})
list(APPEND world_placement_production_sources
  "${SOURCE_ROOT}/lua/lua_tactical.cpp")
foreach(world_placement_source IN LISTS world_placement_production_sources)
  file(READ "${world_placement_source}"
    world_placement_source_contents)
  foreach(retired_world_placement_call IN ITEMS
    "InternalRemoveSoldierFromGridNo"
    "RemoveSoldierFromGridNo"
    "EVENT_InternalSetSoldierPosition"
    "EVENT_SetSoldierPosition"
    "EVENT_SetSoldierPositionForceDelete"
    "EVENT_SetSoldierPositionAndMaybeFinalDestAndMaybeNotDestination"
    "InternalSetSoldierHeight"
    "SetSoldierHeight"
    "SetSoldierGridNo")
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_world_placement_call}[ \t\r\n]*\\("
      retired_world_placement_usage
      "${world_placement_source_contents}")
    if(retired_world_placement_usage)
      message(FATAL_ERROR
        "Production caller in ${world_placement_source} restored retired world-placement entry '${retired_world_placement_call}'")
    endif()
  endforeach()
endforeach()

foreach(required_ranged_action IN ITEMS
  "beginFire"
  "ready"
  "readyToward"
  "readyFacing"
  "refreshAfterHandItemChange")
  string(FIND "${tactical_actor_ranged_actions_header_contents}"
    "${required_ranged_action}("
    ranged_action_declaration)
  string(FIND "${tactical_actor_ranged_actions_source_contents}"
    "TacticalActorRangedActions::${required_ranged_action}("
    ranged_action_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorRangedActions::${required_ranged_action}"
    ranged_action_coverage)
  if(ranged_action_declaration EQUAL -1 OR
     ranged_action_definition EQUAL -1 OR
     ranged_action_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor ranged action '${required_ranged_action}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_condition_presentation_operation IN ITEMS
  "appendFoodDescription"
  "appendDiseaseDescription"
  "appendSleepDescription"
  "appendSummary")
  string(FIND "${tactical_actor_condition_presentation_header_contents}"
    "${required_condition_presentation_operation}("
    condition_presentation_operation_declaration)
  string(FIND "${tactical_actor_condition_presentation_source_contents}"
    "TacticalActorConditionPresentation::${required_condition_presentation_operation}("
    condition_presentation_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorConditionPresentation::${required_condition_presentation_operation}("
    condition_presentation_operation_coverage)
  if(condition_presentation_operation_declaration EQUAL -1 OR
     condition_presentation_operation_definition EQUAL -1 OR
     condition_presentation_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor condition-presentation operation '${required_condition_presentation_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

string(FIND "${tactical_actor_disease_header_contents}"
  "appendDescription("
  legacy_disease_presentation_declaration)
string(FIND "${tactical_actor_source_contents}"
  "TacticalActorDisease::appendDescription("
  legacy_disease_presentation_definition)
string(FIND "${tactical_interface_items_contents}"
  "TacticalActorConditionPresentation::appendSummary("
  tactical_condition_summary_caller)
string(FIND "${strategic_map_screen_interface_contents}"
  "TacticalActorConditionPresentation::appendSummary("
  strategic_condition_summary_caller)
string(FIND "${tactical_interface_panels_contents}"
  "TacticalActorConditionPresentation::appendDiseaseDescription("
  tactical_disease_presentation_caller)
if(NOT legacy_disease_presentation_declaration EQUAL -1 OR
   NOT legacy_disease_presentation_definition EQUAL -1 OR
   tactical_condition_summary_caller EQUAL -1 OR
   strategic_condition_summary_caller EQUAL -1 OR
   tactical_disease_presentation_caller EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor condition presentation regained a disease facade or lost a direct tactical/strategic caller")
endif()

string(FIND "${tactical_actor_turn_maintenance_header_contents}"
  "maintainAtTurnStart(TacticalActor& actor)"
  turn_maintenance_operation_declaration)
string(FIND "${tactical_actor_turn_maintenance_source_contents}"
  "TacticalActorTurnMaintenance::maintainAtTurnStart("
  turn_maintenance_operation_definition)
string(FIND "${tactical_actor_turn_lifecycle_source_contents}"
  "TacticalActorTurnMaintenance::maintainAtTurnStart(subject);"
  turn_maintenance_owner_call)
string(FIND "${headless_test_contents}"
  "TacticalActorTurnMaintenance::maintainAtTurnStart("
  turn_maintenance_operation_coverage)
if(turn_maintenance_operation_declaration EQUAL -1 OR
   turn_maintenance_operation_definition EQUAL -1 OR
   turn_maintenance_owner_call EQUAL -1 OR
   turn_maintenance_operation_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor turn maintenance lost its declaration, definition, turn-start caller, or focused headless coverage")
endif()

string(FIND "${tactical_actor_turn_lifecycle_source_contents}"
  "subject.condition().clearExtraStats();"
  actor_condition_reset_owner_call)
string(FIND "${tactical_actor_timer_source_contents}"
  "soldier->statProgress().reset();"
  actor_stat_progress_reset_owner_call)
string(FIND "${tactical_actor_creation_source_contents}"
  "pSoldier->runtime().reset();"
  actor_creation_runtime_reset_owner_call)
string(FIND "${tactical_actor_persistence_source_contents}"
  "actor.runtime().reset();"
  actor_load_runtime_reset_owner_call)
string(FIND "${headless_test_contents}"
  "soldier initialization directly resets condition, stat-progress, runtime"
  actor_reset_owner_coverage)
if(actor_condition_reset_owner_call EQUAL -1 OR
   actor_stat_progress_reset_owner_call EQUAL -1 OR
   actor_creation_runtime_reset_owner_call EQUAL -1 OR
   actor_load_runtime_reset_owner_call EQUAL -1 OR
   actor_reset_owner_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor lifecycle reset ownership lost a direct component caller or focused headless coverage")
endif()

foreach(retired_animation_profile_helper IN ITEMS
  "GetAnimProfileFlags(")
  string(FIND "${tactical_actor_source_contents}"
    "${retired_animation_profile_helper}"
    retired_animation_profile_source_helper)
  string(FIND "${tactical_world_definition_header_contents}"
    "${retired_animation_profile_helper}"
    retired_animation_profile_header_helper)
  if(NOT retired_animation_profile_source_helper EQUAL -1 OR
     NOT retired_animation_profile_header_helper EQUAL -1)
    message(FATAL_ERROR
      "Retired animation-profile helper '${retired_animation_profile_helper}' returned; use TacticalActorAnimationFootprint")
  endif()
endforeach()

foreach(retired_conversation_helper IN ITEMS
  "HandleVolunteerRecruitment"
  "AbandonBoxingDueToSurrenderCallback")
  string(FIND "${tactical_soldier_control_header_contents}${tactical_actor_header_contents}"
    "${retired_conversation_helper}("
    retired_conversation_helper_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "${retired_conversation_helper}("
    retired_conversation_helper_definition)
  if(NOT retired_conversation_helper_declaration EQUAL -1 OR
     NOT retired_conversation_helper_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control regained retired conversation helper '${retired_conversation_helper}'")
  endif()
endforeach()

string(FIND "${tactical_soldier_functions_header_contents}"
  "SoldierCollapse("
  retired_global_soldier_collapse_declaration)
string(FIND "${tactical_actor_source_contents}"
  "SoldierCollapse("
  retired_global_soldier_collapse_definition)
if(NOT retired_global_soldier_collapse_declaration EQUAL -1 OR
   NOT retired_global_soldier_collapse_definition EQUAL -1)
  message(FATAL_ERROR
    "The retired global SoldierCollapse recovery entry point returned")
endif()

string(FIND "${tactical_actor_source_contents}"
  "SleepDartSuccumbChance("
  retired_sleep_dart_succumb_helper)
if(NOT retired_sleep_dart_succumb_helper EQUAL -1)
  message(FATAL_ERROR
    "Soldier Control regained the retired sleep-dart recovery helper")
endif()

string(FIND "${tactical_actor_source_contents}"
  "void BeginSoldierClimbWallUp("
  retired_global_wall_descent_helper)
if(NOT retired_global_wall_descent_helper EQUAL -1)
  message(FATAL_ERROR
    "Soldier Control regained the retired duplicate wall-descent helper")
endif()

foreach(retired_lighting_helper IN ITEMS
  "ReCreateSelectedSoldierLight"
  "SetSoldierPersonalLightLevel")
  string(FIND "${tactical_soldier_control_header_contents}${tactical_actor_header_contents}"
    "${retired_lighting_helper}("
    retired_lighting_helper_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "${retired_lighting_helper}("
    retired_lighting_helper_definition)
  if(NOT retired_lighting_helper_declaration EQUAL -1 OR
     NOT retired_lighting_helper_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control regained retired lighting helper '${retired_lighting_helper}'")
  endif()
endforeach()

foreach(required_medical_session_operation IN ITEMS
  "beginActionPointCost"
  "beginFirstAid"
  "resumeProvidingAnimation")
  string(FIND "${tactical_actor_medical_session_header_contents}"
    "${required_medical_session_operation}("
    medical_session_operation_declaration)
  string(FIND "${tactical_actor_medical_session_source_contents}"
    "TacticalActorMedicalSession::${required_medical_session_operation}("
    medical_session_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMedicalSession::"
    medical_session_domain_coverage)
  string(FIND "${headless_test_contents}"
    "${required_medical_session_operation}("
    medical_session_operation_coverage)
  if(medical_session_operation_declaration EQUAL -1 OR
     medical_session_operation_definition EQUAL -1 OR
     medical_session_domain_coverage EQUAL -1 OR
     medical_session_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor medical-session operation '${required_medical_session_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_field_operation IN ITEMS
  "beginFenceCutting"
  "beginRepair"
  "beginRefuel"
  "beginCorpseBloodCollection"
  "attachDoorAlarm"
  "beginFortification"
  "performInteractiveAction"
  "beginRobotReload"
  "canBreakWindow"
  "breakWindow")
  string(FIND "${tactical_actor_field_operations_header_contents}"
    "${required_field_operation}("
    field_operation_declaration)
  string(FIND "${tactical_actor_field_operations_source_contents}"
    "TacticalActorFieldOperations::${required_field_operation}("
    field_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorFieldOperations::"
    field_operations_domain_coverage)
  string(FIND "${headless_test_contents}"
    "${required_field_operation}("
    field_operation_coverage)
  if(field_operation_declaration EQUAL -1 OR
     field_operation_definition EQUAL -1 OR
     field_operations_domain_coverage EQUAL -1 OR
     field_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor field operation '${required_field_operation}' lost its domain declaration, definition, or unavailable-world coverage")
  endif()
endforeach()

string(FIND "${tactical_item_callback_header_contents}"
  "DoInteractiveAction("
  retired_interactive_action_declaration)
string(FIND "${tactical_item_callback_contents}"
  "void DoInteractiveAction("
  retired_interactive_action_definition)
if(NOT retired_interactive_action_declaration EQUAL -1 OR
   NOT retired_interactive_action_definition EQUAL -1)
  message(FATAL_ERROR
    "Handle Items regained the retired unbounded interactive-action global")
endif()

string(FIND "${tactical_points_header_contents}"
  "GetAPsToBeginFirstAid("
  retired_first_aid_cost_declaration)
string(FIND "${tactical_points_source_contents}"
  "GetAPsToBeginFirstAid("
  retired_first_aid_cost_definition)
if(NOT retired_first_aid_cost_declaration EQUAL -1 OR
   NOT retired_first_aid_cost_definition EQUAL -1)
  message(FATAL_ERROR
    "Points regained the retired first-aid action-point global")
endif()

foreach(retired_medical_treatment_global IN ITEMS
  "VirtualSoldierDressWound"
  "NumberOfDamagedStats"
  "RegainDamagedStats")
  string(FIND "${tactical_actor_source_contents}"
    "${retired_medical_treatment_global}("
    retired_medical_treatment_definition)
  if(NOT retired_medical_treatment_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control regained retired medical-treatment global '${retired_medical_treatment_global}'")
  endif()
endforeach()

foreach(required_damage_queue_operation IN ITEMS
  "schedule"
  "resolve"
  "clear")
  string(FIND "${tactical_actor_damage_queue_header_contents}"
    "${required_damage_queue_operation}("
    damage_queue_operation_declaration)
  string(FIND "${tactical_actor_damage_queue_source_contents}"
    "TacticalActorDamageQueue::${required_damage_queue_operation}("
    damage_queue_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorDamageQueue::${required_damage_queue_operation}"
    damage_queue_operation_coverage)
  if(damage_queue_operation_declaration EQUAL -1 OR
     damage_queue_operation_definition EQUAL -1 OR
     damage_queue_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor damage-queue operation '${required_damage_queue_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_medical_treatment_operation IN ITEMS
  "treatInSector"
  "treatAbstract"
  "damagedStatCount"
  "restoreDamagedStats")
  string(FIND "${tactical_actor_medical_treatment_header_contents}"
    "${required_medical_treatment_operation}("
    medical_treatment_operation_declaration)
  string(FIND "${tactical_actor_medical_treatment_source_contents}"
    "TacticalActorMedicalTreatment::${required_medical_treatment_operation}("
    medical_treatment_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMedicalTreatment::"
    medical_treatment_domain_coverage)
  string(FIND "${headless_test_contents}"
    "${required_medical_treatment_operation}("
    medical_treatment_operation_coverage)
  if(medical_treatment_operation_declaration EQUAL -1 OR
     medical_treatment_operation_definition EQUAL -1 OR
     medical_treatment_domain_coverage EQUAL -1 OR
     medical_treatment_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor medical-treatment operation '${required_medical_treatment_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_medical_service_operation IN ITEMS
  "canTreatForAi"
  "treatAdjacentForAi"
  "treatSelfForAi"
  "cancelReceiving"
  "cancelProviding")
  string(FIND "${tactical_actor_medical_services_header_contents}"
    "${required_medical_service_operation}("
    medical_service_operation_declaration)
  string(FIND "${tactical_actor_medical_services_source_contents}"
    "TacticalActorMedicalServices::${required_medical_service_operation}("
    medical_service_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMedicalServices::${required_medical_service_operation}"
    medical_service_operation_coverage)
  if(medical_service_operation_declaration EQUAL -1 OR
     medical_service_operation_definition EQUAL -1 OR
     medical_service_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor medical-service operation '${required_medical_service_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_robotics_operation IN ITEMS
  "controller"
  "canBeControlled"
  "isControlling"
  "refreshControllerForRobot"
  "refreshRobotsForController")
  string(FIND "${tactical_actor_robotics_header_contents}"
    "${required_robotics_operation}("
    robotics_operation_declaration)
  string(FIND "${tactical_actor_robotics_source_contents}"
    "TacticalActorRobotics::${required_robotics_operation}("
    robotics_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorRobotics::${required_robotics_operation}"
    robotics_operation_coverage)
  if(robotics_operation_declaration EQUAL -1 OR
     robotics_operation_definition EQUAL -1 OR
     robotics_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor robotics operation '${required_robotics_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

string(FIND "${tactical_build_contents}"
  "TacticalActorRobotics.cpp"
  robotics_build_registration)
if(robotics_build_registration EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor robotics is no longer built as its own implementation unit")
endif()

foreach(required_mobility_operation IN ITEMS
  "inWater"
  "inShallowWater"
  "inDeepWater"
  "inHighWater"
  "movementStateForStance"
  "movementStateForCurrentStance"
  "transitionStateForStance"
  "canClimbWithCurrentBackpack"
  "isValidStance"
  "isValidMovementMode"
  "selectMovementForCurrentStance"
  "isCurrentStanceValid"
  "isCrouchedAgainstCover"
  "isFastMovement")
  string(FIND "${tactical_actor_mobility_header_contents}"
    "${required_mobility_operation}("
    mobility_operation_declaration)
  string(FIND "${tactical_actor_mobility_source_contents}"
    "TacticalActorMobility::${required_mobility_operation}("
    mobility_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorMobility::${required_mobility_operation}"
    mobility_operation_coverage)
  if(mobility_operation_declaration EQUAL -1 OR
     mobility_operation_definition EQUAL -1 OR
     mobility_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor mobility operation '${required_mobility_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_weapon_handling_operation IN ITEMS
  "isValidSecondHandShot"
  "isValidSecondHandBurst"
  "isValidSecondHandShotForReloading"
  "isValidAlternativeFireMode"
  "isValidShotFromHip"
  "isValidPistolFastShot"
  "isWeaponMounted")
  string(FIND "${tactical_actor_weapon_handling_header_contents}"
    "${required_weapon_handling_operation}("
    weapon_handling_operation_declaration)
  string(FIND "${tactical_actor_weapon_handling_source_contents}"
    "TacticalActorWeaponHandling::${required_weapon_handling_operation}("
    weapon_handling_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorWeaponHandling::${required_weapon_handling_operation}"
    weapon_handling_operation_coverage)
  if(weapon_handling_operation_declaration EQUAL -1 OR
     weapon_handling_operation_definition EQUAL -1 OR
     weapon_handling_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor weapon-handling operation '${required_weapon_handling_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_ai_behavior_operation IN ITEMS
  "hasInitialActionPoints"
  "isFlanking"
  "setUnderControl"
  "stopCowering"
  "startRetreat"
  "retreatCounter"
  "startRadioAnimation"
  "clearBoxerFlag")
  string(FIND "${tactical_actor_ai_behavior_header_contents}"
    "${required_ai_behavior_operation}("
    ai_behavior_operation_declaration)
  string(FIND "${tactical_actor_ai_behavior_source_contents}"
    "TacticalActorAiBehavior::${required_ai_behavior_operation}("
    ai_behavior_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorAiBehavior::${required_ai_behavior_operation}"
    ai_behavior_operation_coverage)
  if(ai_behavior_operation_declaration EQUAL -1 OR
     ai_behavior_operation_definition EQUAL -1 OR
     ai_behavior_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor AI behavior operation '${required_ai_behavior_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_long_action_operation IN ITEMS
  "current"
  "start"
  "cancel"
  "update")
  string(FIND "${tactical_actor_long_actions_header_contents}"
    "${required_long_action_operation}("
    long_action_operation_declaration)
  string(FIND "${tactical_actor_long_actions_source_contents}"
    "TacticalActorLongActions::${required_long_action_operation}("
    long_action_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorLongActions::${required_long_action_operation}"
    long_action_operation_coverage)
  if(long_action_operation_declaration EQUAL -1 OR
     long_action_operation_definition EQUAL -1 OR
     long_action_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor long-action operation '${required_long_action_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_prisoner_operation IN ITEMS
  "canProcess"
  "freeAdjacent")
  string(FIND "${tactical_actor_prisoner_operations_header_contents}"
    "${required_prisoner_operation}("
    prisoner_operation_declaration)
  string(FIND "${tactical_actor_prisoner_operations_source_contents}"
    "TacticalActorPrisonerOperations::${required_prisoner_operation}("
    prisoner_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorPrisonerOperations::${required_prisoner_operation}"
    prisoner_operation_coverage)
  if(prisoner_operation_declaration EQUAL -1 OR
     prisoner_operation_definition EQUAL -1 OR
     prisoner_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor prisoner operation '${required_prisoner_operation}' lost its domain declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_actor_domain_source IN ITEMS
  "TacticalActorMobility.cpp"
  "TacticalActorOrientation.cpp"
  "TacticalActorRangedActions.cpp"
  "TacticalActorRouteExecution.cpp"
  "TacticalActorWorldPlacement.cpp"
  "TacticalActorWeaponHandling.cpp"
  "TacticalActorAiBehavior.cpp"
  "TacticalActorDamageQueue.cpp"
  "TacticalActorFieldOperations.cpp"
  "TacticalActorLongActions.cpp"
  "TacticalActorMedicalSession.cpp"
  "TacticalActorMedicalServices.cpp"
  "TacticalActorMedicalTreatment.cpp"
  "TacticalActorPrisonerOperations.cpp")
  string(FIND "${tactical_build_contents}"
    "${required_actor_domain_source}"
    actor_domain_build_registration)
  if(actor_domain_build_registration EQUAL -1)
    message(FATAL_ERROR
      "${required_actor_domain_source} is no longer built as its own tactical implementation unit")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Items.h"
  tactical_items_header_contents)
file(READ "${SOURCE_ROOT}/Tactical/Items.cpp"
  tactical_items_source_contents)
string(FIND
  "${tactical_items_header_contents}${tactical_items_source_contents}"
  "FindRemoteControl("
  retired_robot_remote_lookup)
if(NOT retired_robot_remote_lookup EQUAL -1)
  message(FATAL_ERROR
    "Legacy global robot-remote lookup returned; keep bounded equipment discovery inside TacticalActorRobotics")
endif()

foreach(required_skill_operation IN ITEMS
  "canUse"
  "use"
  "description")
  string(FIND "${tactical_actor_skills_header_contents}"
    "${required_skill_operation}("
    skill_operation_declaration)
  string(FIND "${tactical_actor_skills_source_contents}"
    "TacticalActorSkills::${required_skill_operation}("
    skill_operation_definition)
  if(skill_operation_declaration EQUAL -1 OR
     skill_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor skill operation '${required_skill_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_skill_coverage IN ITEMS
  "TacticalActorSkills::canUse"
  "TacticalActorSkills::use"
  "TacticalActorSkills::description")
  string(FIND "${headless_test_contents}"
    "${required_skill_coverage}"
    skill_operation_coverage)
  if(skill_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor skills lost malformed-input coverage for '${required_skill_coverage}'")
  endif()
endforeach()

foreach(required_turncoat_operation IN ITEMS
  "inPositionForAttempt"
  "convictionChance"
  "attempt"
  "orderOne"
  "orderAll")
  string(FIND "${tactical_actor_turncoats_header_contents}"
    "${required_turncoat_operation}("
    turncoat_operation_declaration)
  string(FIND "${tactical_actor_turncoats_source_contents}"
    "TacticalActorTurncoats::${required_turncoat_operation}("
    turncoat_operation_definition)
  if(turncoat_operation_declaration EQUAL -1 OR
     turncoat_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor turncoat operation '${required_turncoat_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_turncoat_coverage IN ITEMS
  "TacticalActorTurncoats::inPositionForAttempt"
  "TacticalActorTurncoats::convictionChance"
  "TacticalActorTurncoats::attempt"
  "TacticalActorTurncoats::orderOne")
  string(FIND "${headless_test_contents}"
    "${required_turncoat_coverage}"
    turncoat_operation_coverage)
  if(turncoat_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor turncoats lost malformed-input coverage for '${required_turncoat_coverage}'")
  endif()
endforeach()

foreach(retired_radio_global IN ITEMS
  "GetRadioOperatorSignal"
  "IsValidArtilleryOrderSector"
  "SectorJammed"
  "PlayerTeamIsScanning"
  "GridNoSpotterCTHBonus")
  string(FIND "${tactical_soldier_control_header_contents}${tactical_actor_header_contents}"
    "${retired_radio_global}("
    retired_radio_global_declaration)
  string(FIND "${tactical_actor_source_contents}"
    "${retired_radio_global}("
    retired_radio_global_definition_or_call)
  if(NOT retired_radio_global_declaration EQUAL -1 OR
     NOT retired_radio_global_definition_or_call EQUAL -1)
    message(FATAL_ERROR
      "Legacy radio/spotting global '${retired_radio_global}' returned; use the TacticalActorRadio or TacticalActorSpotting domain")
  endif()
endforeach()

foreach(required_radio_operation IN ITEMS
  "canUse"
  "use"
  "canOrderAnyArtilleryStrike"
  "orderArtilleryStrike"
  "isJamming"
  "startJamming"
  "isScanning"
  "startScanning"
  "isListening"
  "startListening"
  "callReinforcements"
  "switchOff"
  "orderAllTurncoats"
  "reportFailure"
  "operatorSignal"
  "isValidArtillerySector"
  "sectorJammed"
  "playerTeamScanning")
  string(FIND "${tactical_actor_radio_header_contents}"
    "${required_radio_operation}("
    radio_operation_declaration)
  string(FIND "${tactical_actor_radio_source_contents}"
    "TacticalActorRadio::${required_radio_operation}("
    radio_operation_definition)
  if(radio_operation_declaration EQUAL -1 OR
     radio_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor radio operation '${required_radio_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_radio_coverage IN ITEMS
  "TacticalActorRadio::canUse"
  "TacticalActorRadio::use"
  "TacticalActorRadio::orderArtilleryStrike"
  "TacticalActorRadio::isJamming"
  "TacticalActorRadio::startJamming"
  "TacticalActorRadio::startScanning"
  "TacticalActorRadio::startListening"
  "TacticalActorRadio::callReinforcements"
  "TacticalActorRadio::switchOff"
  "TacticalActorRadio::orderAllTurncoats"
  "TacticalActorRadio::reportFailure"
  "TacticalActorRadio::isValidArtillerySector"
  "TacticalActorRadio::operatorSignal"
  "TacticalActorRadio::canOrderAnyArtilleryStrike")
  string(FIND "${headless_test_contents}"
    "${required_radio_coverage}"
    radio_operation_coverage)
  if(radio_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor radio lost malformed-input or state-transition coverage for '${required_radio_coverage}'")
  endif()
endforeach()

foreach(required_spotting_operation IN ITEMS
  "isSpotting"
  "canSpot"
  "startSpotting"
  "chanceToHitBonus")
  string(FIND "${tactical_actor_spotting_header_contents}"
    "${required_spotting_operation}("
    spotting_operation_declaration)
  string(FIND "${tactical_actor_spotting_source_contents}"
    "TacticalActorSpotting::${required_spotting_operation}("
    spotting_operation_definition)
  if(spotting_operation_declaration EQUAL -1 OR
     spotting_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor spotting operation '${required_spotting_operation}' lost its domain declaration or definition")
  endif()
endforeach()

foreach(required_spotting_coverage IN ITEMS
  "TacticalActorSpotting::isSpotting"
  "TacticalActorSpotting::canSpot"
  "TacticalActorSpotting::startSpotting"
  "TacticalActorSpotting::chanceToHitBonus")
  string(FIND "${headless_test_contents}"
    "${required_spotting_coverage}"
    spotting_operation_coverage)
  if(spotting_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor spotting lost malformed-input coverage for '${required_spotting_coverage}'")
  endif()
endforeach()

foreach(required_consumable_operation IN ITEMS
  "autoUseDrug")
  string(FIND "${tactical_actor_consumables_header_contents}"
    "${required_consumable_operation}("
    consumable_operation_declaration)
  string(FIND "${tactical_actor_consumables_source_contents}"
    "${required_consumable_operation}(TacticalActor& actor)"
    consumable_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorConsumables::${required_consumable_operation}"
    consumable_operation_coverage)
  if(consumable_operation_declaration EQUAL -1 OR
     consumable_operation_definition EQUAL -1 OR
     consumable_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor consumable operation '${required_consumable_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_combat_action IN ITEMS
  "beginBladeAttack"
  "beginPunchAttack"
  "beginKnifeThrow"
  "continueNinjaAttack")
  string(FIND "${tactical_actor_combat_actions_header_contents}"
    "${required_combat_action}("
    combat_action_declaration)
  string(FIND "${tactical_actor_combat_actions_source_contents}"
    "TacticalActorCombatActions::${required_combat_action}("
    combat_action_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorCombatActions::${required_combat_action}"
    combat_action_coverage)
  if(combat_action_declaration EQUAL -1 OR
     combat_action_definition EQUAL -1 OR
     combat_action_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor combat action '${required_combat_action}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_combat_reaction IN ITEMS
  "setCowering"
  "beginFall"
  "beginFlyback"
  "beginFallback")
  string(FIND "${tactical_actor_combat_reactions_header_contents}"
    "${required_combat_reaction}("
    combat_reaction_declaration)
  string(FIND "${tactical_actor_combat_reactions_source_contents}"
    "TacticalActorCombatReactions::${required_combat_reaction}("
    combat_reaction_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorCombatReactions::${required_combat_reaction}"
    combat_reaction_coverage)
  if(combat_reaction_declaration EQUAL -1 OR
     combat_reaction_definition EQUAL -1 OR
     combat_reaction_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor combat reaction '${required_combat_reaction}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/TacticalAI/AIMain.cpp"
  tactical_ai_main_contents)
string(FIND "${tactical_ai_main_contents}"
  "TacticalActorCombatReactions::setCowering"
  tactical_ai_cower_domain_call)
if(tactical_ai_cower_domain_call EQUAL -1)
  message(FATAL_ERROR
    "Tactical AI cower transitions must enter TacticalActorCombatReactions::setCowering")
endif()

string(FIND "${tactical_actor_damage_feedback_header_contents}"
  "presentHit(TacticalActor& actor)"
  damage_feedback_declaration)
string(FIND "${tactical_actor_damage_feedback_source_contents}"
  "TacticalActorDamageFeedback::presentHit("
  damage_feedback_definition)
string(FIND "${headless_test_contents}"
  "TacticalActorDamageFeedback::presentHit"
  damage_feedback_coverage)
string(FIND "${tactical_actor_damage_resolution_source_contents}"
  "TacticalActorDamageFeedback::presentHit"
  soldier_damage_feedback_call)
file(READ "${SOURCE_ROOT}/Tactical/Vehicles.cpp"
  tactical_vehicles_contents)
string(FIND "${tactical_vehicles_contents}"
  "TacticalActorDamageFeedback::presentHit"
  vehicle_damage_feedback_call)
if(damage_feedback_declaration EQUAL -1 OR
   damage_feedback_definition EQUAL -1 OR
   damage_feedback_coverage EQUAL -1 OR
   soldier_damage_feedback_call EQUAL -1 OR
   vehicle_damage_feedback_call EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor damage feedback lost its declaration, definition, caller migration, or malformed-state coverage")
endif()

foreach(animation_selection_operation IN ITEMS
    selectFire
    selectFall
    pickReady)
  string(FIND
    "${tactical_actor_animation_selection_header_contents}"
    "${animation_selection_operation}("
    animation_selection_declaration)
  string(FIND
    "${tactical_actor_animation_selection_source_contents}"
    "TacticalActorAnimationSelection::${animation_selection_operation}("
    animation_selection_definition)
  if(animation_selection_declaration EQUAL -1 OR
     animation_selection_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor animation selection lost '${animation_selection_operation}' ownership")
  endif()
endforeach()

foreach(damage_feedback_operation IN ITEMS
    calculateScreamVolume
    applyGenericHit
    applyGunfireHit
    applyExplosionHit
    applyBladeHit
    applyPunchHit
    applyVehicleHit
    setDamageDisplayCounter)
  string(FIND "${tactical_actor_damage_feedback_header_contents}"
    "${damage_feedback_operation}("
    damage_feedback_operation_declaration)
  string(FIND "${tactical_actor_damage_feedback_source_contents}"
    "TacticalActorDamageFeedback::${damage_feedback_operation}("
    damage_feedback_operation_definition)
  if(damage_feedback_operation_declaration EQUAL -1 OR
     damage_feedback_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor damage feedback lost '${damage_feedback_operation}' ownership")
  endif()
endforeach()

foreach(retired_soldier_control_presentation_definition IN ITEMS
    "SelectFireAnimation("
    "SelectFallAnimation("
    "PickSoldierReadyAnimation("
    "CalcScreamVolume("
    "DoGenericHit("
    "SoldierGotHitGunFire("
    "SoldierGotHitExplosion("
    "SoldierGotHitBlade("
    "SoldierGotHitPunch("
    "SoldierGotHitVehicle("
    "SetDamageDisplayCounter(")
  string(FIND "${tactical_actor_source_contents}"
    "${retired_soldier_control_presentation_definition}"
    retired_soldier_control_presentation_returned)
  if(NOT retired_soldier_control_presentation_returned EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control.cpp regained retired presentation definition '${retired_soldier_control_presentation_definition}'")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "tactical actor animation selection and hit feedback own ready, fire, fall, scream, reaction, uniform, and damage-display behavior"
  actor_presentation_extraction_coverage)
if(actor_presentation_extraction_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor animation-selection and hit-feedback extraction lost combined headless coverage")
endif()

string(FIND "${tactical_actor_profile_classification_header_contents}"
  "profileTableIndex("
  profile_classification_declaration)
string(FIND "${tactical_actor_profile_classification_source_contents}"
  "TacticalActorProfileClassification::profileTableIndex("
  profile_classification_definition)
string(FIND "${headless_test_contents}"
  "TacticalActorProfileClassification::profileTableIndex"
  profile_classification_coverage)
string(FIND "${tactical_actor_aggregate_source_contents}"
  "TacticalActorProfileClassification::profileTableIndex"
  actor_name_profile_classification_call)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  tactical_actor_profile_creation_contents)
string(FIND "${tactical_actor_profile_creation_contents}"
  "TacticalActorProfileClassification::profileTableIndex"
  actor_creation_profile_classification_call)
if(profile_classification_declaration EQUAL -1 OR
   profile_classification_definition EQUAL -1 OR
   profile_classification_coverage EQUAL -1 OR
   actor_name_profile_classification_call EQUAL -1 OR
   actor_creation_profile_classification_call EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor profile classification lost its declaration, definition, caller migration, or bounded headless coverage")
endif()

foreach(required_recovery_operation IN ITEMS
  "applySleepDart"
  "checkBreathCollapse"
  "collapse"
  "beginGetUp")
  string(FIND "${tactical_actor_recovery_header_contents}"
    "${required_recovery_operation}("
    recovery_operation_declaration)
  string(FIND "${tactical_actor_recovery_source_contents}"
    "TacticalActorRecovery::${required_recovery_operation}("
    recovery_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorRecovery::${required_recovery_operation}"
    recovery_operation_coverage)
  if(recovery_operation_declaration EQUAL -1 OR
     recovery_operation_definition EQUAL -1 OR
     recovery_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor recovery operation '${required_recovery_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

foreach(required_traversal_action IN ITEMS
  "beginRoofClimb"
  "beginRoofDescent"
  "beginFenceJump"
  "beginWallClimb"
  "beginWindowJump")
  string(FIND "${tactical_actor_traversal_header_contents}"
    "${required_traversal_action}("
    traversal_action_declaration)
  string(FIND "${tactical_actor_traversal_source_contents}"
    "TacticalActorTraversal::${required_traversal_action}("
    traversal_action_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorTraversal::${required_traversal_action}"
    traversal_action_coverage)
  if(traversal_action_declaration EQUAL -1 OR
     traversal_action_definition EQUAL -1 OR
     traversal_action_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor traversal action '${required_traversal_action}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_explosive_operation IN ITEMS
  "degradeInventoryAfterExplosion"
  "applyInventoryExplosion"
  "selfDetonate")
  string(FIND "${tactical_actor_explosives_header_contents}"
    "${required_explosive_operation}("
    explosive_operation_declaration)
  string(FIND "${tactical_actor_explosives_source_contents}"
    "${required_explosive_operation}(TacticalActor& actor)"
    explosive_operation_definition)
  if(explosive_operation_declaration EQUAL -1 OR
     explosive_operation_definition EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor explosive operation '${required_explosive_operation}' lost its declaration or definition")
  endif()
endforeach()

foreach(required_explosive_coverage IN ITEMS
  "degradeInventoryAfterExplosion"
  "selfDetonate")
  string(FIND "${headless_test_contents}"
    "TacticalActorExplosives::${required_explosive_coverage}"
    explosive_operation_coverage)
  if(explosive_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor explosives lost data-free or malformed-input coverage for '${required_explosive_coverage}'")
  endif()
endforeach()

foreach(required_explosive_action IN ITEMS
  "beginBombPlacement"
  "beginTripwireDisarm"
  "beginDetonatorUse")
  string(FIND "${tactical_actor_explosives_header_contents}"
    "${required_explosive_action}("
    explosive_action_declaration)
  string(FIND "${tactical_actor_explosives_source_contents}"
    "${required_explosive_action}("
    explosive_action_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorExplosives::${required_explosive_action}"
    explosive_action_coverage)
  if(explosive_action_declaration EQUAL -1 OR
     explosive_action_definition EQUAL -1 OR
     explosive_action_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor explosive action '${required_explosive_action}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_interaction_operation IN ITEMS
  "startConversation"
  "stopChatting"
  "beginItemTransfer"
  "beginGivingItem"
  "handcuffPerson"
  "applyItemToPerson"
  "collectBloodFromPerson"
  "applySplintToPerson")
  string(FIND "${tactical_actor_interactions_header_contents}"
    "${required_interaction_operation}("
    interaction_operation_declaration)
  string(FIND "${tactical_actor_interactions_source_contents}"
    "TacticalActorInteractions::${required_interaction_operation}("
    interaction_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorInteractions::${required_interaction_operation}"
    interaction_operation_coverage)
  if(interaction_operation_declaration EQUAL -1 OR
     interaction_operation_definition EQUAL -1 OR
     interaction_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor interaction '${required_interaction_operation}' lost its declaration, definition, or headless coverage")
  endif()
endforeach()

string(REGEX MATCHALL
  "TacticalActorInteractions::beginItemTransfer\\([ \t\r\n]*\\*pSoldier\\)"
  item_transfer_owner_calls
  "${tactical_item_fire_executable}")
list(LENGTH item_transfer_owner_calls item_transfer_owner_call_count)
if(NOT item_transfer_owner_call_count EQUAL 2)
  message(FATAL_ERROR
    "Tactical item pickup/drop callers must enter TacticalActorInteractions::beginItemTransfer")
endif()

foreach(required_lighting_operation IN ITEMS
  "createPersonalLight"
  "recreatePersonalLight"
  "destroyPersonalLight"
  "positionPersonalLight"
  "setPersonalLightLevel")
  string(FIND "${tactical_actor_lighting_header_contents}"
    "${required_lighting_operation}("
    lighting_operation_declaration)
  string(FIND "${tactical_actor_lighting_source_contents}"
    "TacticalActorLighting::${required_lighting_operation}("
    lighting_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorLighting::${required_lighting_operation}"
    lighting_operation_coverage)
  if(lighting_operation_declaration EQUAL -1 OR
     lighting_operation_definition EQUAL -1 OR
     lighting_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor lighting operation '${required_lighting_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_animation_frame_operation IN ITEMS
  "spriteDirectionForSurface"
  "frozenFrame"
  "selectFrame")
  string(FIND "${tactical_actor_animation_frames_header_contents}"
    "${required_animation_frame_operation}("
    animation_frame_operation_declaration)
  string(FIND "${tactical_actor_animation_frames_source_contents}"
    "TacticalActorAnimationFrames::${required_animation_frame_operation}("
    animation_frame_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorAnimationFrames::${required_animation_frame_operation}"
    animation_frame_operation_coverage)
  if(animation_frame_operation_declaration EQUAL -1 OR
     animation_frame_operation_definition EQUAL -1 OR
     animation_frame_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor animation-frame operation '${required_animation_frame_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

foreach(required_animation_geometry_operation IN ITEMS
  "currentFrame"
  "refreshBoundingBox")
  string(FIND "${tactical_actor_animation_geometry_header_contents}"
    "${required_animation_geometry_operation}("
    animation_geometry_operation_declaration)
  string(FIND "${tactical_actor_animation_geometry_source_contents}"
    "TacticalActorAnimationGeometry::${required_animation_geometry_operation}("
    animation_geometry_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorAnimationGeometry::${required_animation_geometry_operation}"
    animation_geometry_operation_coverage)
  if(animation_geometry_operation_declaration EQUAL -1 OR
     animation_geometry_operation_definition EQUAL -1 OR
     animation_geometry_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor animation-geometry operation '${required_animation_geometry_operation}' lost its declaration, definition, or live/malformed-state coverage")
  endif()
endforeach()

foreach(required_animation_timing_operation IN ITEMS
  "currentTeamSpeedFactor"
  "refresh"
  "adjustForFastTurn")
  string(FIND "${tactical_actor_animation_timing_header_contents}"
    "${required_animation_timing_operation}("
    animation_timing_operation_declaration)
  string(FIND "${tactical_actor_animation_timing_source_contents}"
    "TacticalActorAnimationTiming::${required_animation_timing_operation}("
    animation_timing_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorAnimationTiming::${required_animation_timing_operation}"
    animation_timing_operation_coverage)
  if(animation_timing_operation_declaration EQUAL -1 OR
     animation_timing_operation_definition EQUAL -1 OR
     animation_timing_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor animation-timing operation '${required_animation_timing_operation}' lost its declaration, definition, or live/malformed-state coverage")
  endif()
endforeach()

string(FIND "${render_palette_effects_header_contents}"
  "populateActorShades(" render_palette_effects_declaration)
string(FIND "${render_palette_effects_source_contents}"
  "RenderPaletteEffects::populateActorShades("
  render_palette_effects_definition)
string(FIND "${headless_test_contents}"
  "RenderPaletteEffects::populateActorShades("
  render_palette_effects_coverage)
if(render_palette_effects_declaration EQUAL -1 OR
   render_palette_effects_definition EQUAL -1 OR
   render_palette_effects_coverage EQUAL -1)
  message(FATAL_ERROR
    "Render palette effects lost their declaration, definition, or headless ownership coverage")
endif()

foreach(retired_soldier_control_animation_support IN ITEMS
  "AdjustAniSpeed("
  "CalculateSoldierAniSpeed("
  "GetSpeedUpFactor("
  "SetSoldierAniSpeed("
  "AdjustForFastTurnAnimation("
  "GetActualSoldierAnimDims("
  "GetActualSoldierAnimOffsets("
  "SetSoldierLocatorOffsets("
  "ContinueMercMovement("
  "IsValidStance("
  "IsValidMovementMode("
  "SelectMoveAnimationFromStance("
  "CreateEnemyGlow16BPPPalette("
  "CreateEnemyGreyGlow16BPPPalette("
  "CheckForFullStructures("
  "CheckForFullStruct("
  "FullStructAlone(")
  string(FIND "${tactical_actor_source_contents}"
    "${retired_soldier_control_animation_support}"
    retired_animation_support_monolith_definition)
  if(NOT retired_animation_support_monolith_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control.cpp regained retired animation-support implementation '${retired_soldier_control_animation_support}'")
  endif()
endforeach()

string(FIND "${tactical_actor_appearance_source_contents}"
  "RenderPaletteEffects::populateActorShades("
  actor_appearance_palette_effects_call)
string(FIND "${tactical_logical_palette_table_source_contents}"
  "RenderPaletteEffects::populateActorShades("
  logical_palette_table_effects_call)
if(actor_appearance_palette_effects_call EQUAL -1 OR
   logical_palette_table_effects_call EQUAL -1)
  message(FATAL_ERROR
    "Actor and logical-body palette rebuilds must share RenderPaletteEffects")
endif()

foreach(required_animation_footprint_operation IN ITEMS
  "add"
  "addForSurface"
  "remove"
  "flagsAtGrid"
  "nextWorldNode")
  string(FIND "${tactical_actor_animation_footprint_header_contents}"
    "${required_animation_footprint_operation}("
    animation_footprint_operation_declaration)
  string(FIND "${tactical_actor_animation_footprint_source_contents}"
    "TacticalActorAnimationFootprint::${required_animation_footprint_operation}("
    animation_footprint_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorAnimationFootprint::${required_animation_footprint_operation}"
    animation_footprint_operation_coverage)
  if(animation_footprint_operation_declaration EQUAL -1 OR
     animation_footprint_operation_definition EQUAL -1 OR
     animation_footprint_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor animation-footprint operation '${required_animation_footprint_operation}' lost its declaration, definition, or live/malformed-state coverage")
  endif()
endforeach()

foreach(required_turn_budget_operation IN ITEMS
  "calculateTurnGrant"
  "refreshForTurn")
  string(FIND "${tactical_actor_turn_budget_header_contents}"
    "${required_turn_budget_operation}("
    turn_budget_operation_declaration)
  string(FIND "${tactical_actor_turn_budget_source_contents}"
    "TacticalActorTurnBudget::${required_turn_budget_operation}("
    turn_budget_operation_definition)
  string(FIND "${headless_test_contents}"
    "TacticalActorTurnBudget::${required_turn_budget_operation}"
    turn_budget_operation_coverage)
  if(turn_budget_operation_declaration EQUAL -1 OR
     turn_budget_operation_definition EQUAL -1 OR
     turn_budget_operation_coverage EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor turn-budget operation '${required_turn_budget_operation}' lost its declaration, definition, or malformed-state coverage")
  endif()
endforeach()

string(FIND "${tactical_actor_conditions_header_contents}"
  "canDonateBlood(TacticalActor& actor)"
  donor_condition_declaration)
string(FIND "${tactical_actor_conditions_source_contents}"
  "canDonateBlood(TacticalActor& actor)"
  donor_condition_definition)
string(FIND "${headless_test_contents}"
  "TacticalActorConditions::canDonateBlood"
  donor_condition_coverage)
if(donor_condition_declaration EQUAL -1 OR
   donor_condition_definition EQUAL -1 OR
   donor_condition_coverage EQUAL -1)
  message(FATAL_ERROR
    "Tactical actor donor eligibility lost its declaration, definition, or headless coverage")
endif()

# The last Soldier Control implementation was retired as one ownership
# milestone. Require every residual behavior in its focused source, require the
# combined malformed-state tests, and make both the file and build entry a
# forbidden regression.
foreach(retired_soldier_control_owner IN ITEMS
    "tactical_actor_bleeding_source_contents|TacticalActorBleeding::check("
    "tactical_actor_bleeding_source_contents|TacticalActorBleeding::nextInterval("
    "tactical_actor_bleeding_source_contents|TacticalActorBleeding::nextUnmovingInterval("
    "tactical_actor_movement_audio_source_contents|TacticalActorMovementAudio::setVehicleMovement("
    "tactical_actor_movement_audio_source_contents|TacticalActorMovementAudio::playFootstep("
    "tactical_actor_ai_behavior_source_contents|TacticalActorAiBehavior::handleNewSituation("
    "tactical_actor_ai_behavior_source_contents|TacticalActorAiBehavior::decideHipOrShoulderStance("
    "tactical_actor_animation_selection_source_contents|useAlternativeBigMercAnimation("
    "tactical_actor_animation_selection_source_contents|suspiciousActionPointDuration("
    "tactical_actor_consumables_source_contents|BOOLEAN ApplyConsumable("
    "tactical_actor_equipment_source_contents|TacticalActorEquipment::wearsUsableGasMask("
    "tactical_actor_interactions_source_contents|TacticalActorInteractions::pickPickupAnimation("
    "tactical_actor_interactions_source_contents|TacticalActorInteractions::beginSteal("
    "tactical_actor_interrupts_source_contents|BOOLEAN ResolvePendingInterrupt("
    "tactical_actor_skills_source_contents|BOOLEAN HAS_SKILL_TRAIT("
    "tactical_actor_skills_source_contents|INT8 NUM_SKILL_TRAITS("
    "tactical_actor_skills_source_contents|UINT8 GetSquadleadersCountInVicinity("
    "tactical_actor_skills_source_contents|BOOLEAN TwoStagedTrait("
    "tactical_actor_skills_source_contents|BOOLEAN MajorTrait("
    "tactical_actor_world_placement_source_contents|TacticalActorWorldPlacement::setRoofMarker(")
  string(REPLACE "|" ";" retired_soldier_control_fields
    "${retired_soldier_control_owner}")
  list(GET retired_soldier_control_fields 0
    retired_soldier_control_owner_variable)
  list(GET retired_soldier_control_fields 1
    retired_soldier_control_definition)
  string(FIND "${${retired_soldier_control_owner_variable}}"
    "${retired_soldier_control_definition}"
    retired_soldier_control_owner_definition)
  if(retired_soldier_control_owner_definition EQUAL -1)
    message(FATAL_ERROR
      "Focused owner lost retired Soldier Control behavior '${retired_soldier_control_definition}'")
  endif()
endforeach()

foreach(retired_soldier_control_contract IN ITEMS
    "tactical_actor_bleeding_header_contents|nextInterval("
    "tactical_actor_bleeding_header_contents|nextUnmovingInterval("
    "tactical_actor_movement_audio_header_contents|setVehicleMovement("
    "tactical_actor_movement_audio_header_contents|playFootstep("
    "tactical_actor_ai_behavior_header_contents|handleNewSituation("
    "tactical_actor_ai_behavior_header_contents|decideHipOrShoulderStance("
    "tactical_actor_animation_selection_header_contents|useAlternativeBigMercAnimation("
    "tactical_actor_animation_selection_header_contents|suspiciousActionPointDuration("
    "tactical_actor_equipment_header_contents|wearsUsableGasMask("
    "tactical_actor_interactions_header_contents|pickPickupAnimation("
    "tactical_actor_interactions_header_contents|beginSteal("
    "tactical_actor_world_placement_header_contents|setRoofMarker(")
  string(REPLACE "|" ";" retired_soldier_control_contract_fields
    "${retired_soldier_control_contract}")
  list(GET retired_soldier_control_contract_fields 0
    retired_soldier_control_contract_variable)
  list(GET retired_soldier_control_contract_fields 1
    retired_soldier_control_declaration)
  string(FIND "${${retired_soldier_control_contract_variable}}"
    "${retired_soldier_control_declaration}"
    retired_soldier_control_contract_declaration)
  if(retired_soldier_control_contract_declaration EQUAL -1)
    message(FATAL_ERROR
      "Focused contract lost retired Soldier Control operation '${retired_soldier_control_declaration}'")
  endif()
endforeach()

foreach(retired_soldier_control_facade_contract IN ITEMS
    "TacticalActorAiBehavior.h"
    "TacticalActorAnimationSelection.h"
    "TacticalActorBleeding.h"
    "TacticalActorConsumables.h"
    "TacticalActorEquipment.h"
    "TacticalActorInteractions.h"
    "TacticalActorInterrupts.h"
    "TacticalActorMovementAudio.h"
    "TacticalActorSkills.h"
    "TacticalActorWorldPlacement.h")
  string(FIND "${tactical_soldier_control_header_contents}"
    "#include \"${retired_soldier_control_facade_contract}\""
    retired_soldier_control_facade_contract_include)
  if(retired_soldier_control_facade_contract_include EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control.h stopped re-exporting focused compatibility contract '${retired_soldier_control_facade_contract}'")
  endif()
endforeach()

string(FIND "${tactical_actor_animation_transitions_source_contents}"
  "UINT16 usForceAnimState = INVALID_ANIMATION;"
  forced_animation_state_owner)
string(FIND "${tactical_actor_animation_transitions_header_contents}"
  "extern std::uint16_t usForceAnimState;"
  forced_animation_state_contract)
string(FIND "${tactical_actor_route_execution_source_contents}"
  "BOOLEAN gfGetNewPathThroughPeople = FALSE;"
  route_people_path_owner)
string(FIND "${tactical_actor_route_execution_header_contents}"
  "extern std::uint8_t gfGetNewPathThroughPeople;"
  route_people_path_contract)
string(FIND "${tactical_soldier_stat_types_header_contents}"
  "inline constexpr std::uint8_t bHealthStrRanges[]"
  health_string_range_owner)
if(forced_animation_state_owner EQUAL -1 OR
   forced_animation_state_contract EQUAL -1 OR
   route_people_path_owner EQUAL -1 OR
   route_people_path_contract EQUAL -1 OR
   health_string_range_owner EQUAL -1)
  message(FATAL_ERROR
    "A residual Soldier Control global lost its focused animation, route, or stat owner")
endif()

string(FIND "${headless_test_contents}"
  "retired soldier-control behavior is owned by focused actor services with malformed-state coverage"
  retired_soldier_control_headless_coverage)
if(retired_soldier_control_headless_coverage EQUAL -1)
  message(FATAL_ERROR
    "Retired Soldier Control behavior lost its combined headless coverage")
endif()

foreach(required_actor_domain_source IN ITEMS
  "TacticalActor.cpp"
  "TacticalActorAnimationFootprint.cpp"
  "TacticalActorAnimationFrames.cpp"
  "TacticalActorAnimationGeometry.cpp"
  "TacticalActorAnimationSelection.cpp"
  "TacticalActorAnimationTiming.cpp"
  "TacticalActorAnimationTransitions.cpp"
  "TacticalActorAppearance.cpp"
  "TacticalActorAssignments.cpp"
  "TacticalActorBattleSounds.cpp"
  "TacticalActorBleeding.cpp"
  "TacticalActorConsumables.cpp"
  "TacticalActorCombatActions.cpp"
  "TacticalActorCombatReactions.cpp"
  "TacticalActorCovertOps.cpp"
  "TacticalActorVisibility.cpp"
  "TacticalActorConditionPresentation.cpp"
  "TacticalActorDamageFeedback.cpp"
  "TacticalActorDamageResolution.cpp"
  "TacticalActorDisease.cpp"
  "TacticalActorDragging.cpp"
  "TacticalActorLifecycle.cpp"
  "TacticalActorLocomotion.cpp"
  "TacticalActorModifiers.cpp"
  "TacticalActorMovementAudio.cpp"
  "TacticalActorRecovery.cpp"
  "TacticalActorTraversal.cpp"
  "TacticalActorExplosives.cpp"
  "TacticalActorEquipment.cpp"
  "TacticalActorInteractions.cpp"
  "TacticalActorInterrupts.cpp"
  "TacticalActorLighting.cpp"
  "TacticalActorProfileClassification.cpp"
  "TacticalActorRadio.cpp"
  "TacticalActorSkills.cpp"
  "TacticalActorSpotting.cpp"
  "TacticalActorTurnBudget.cpp"
  "TacticalActorTurnLifecycle.cpp"
  "TacticalActorTurnMaintenance.cpp"
  "TacticalActorTurncoats.cpp"
  "Render Palette Effects.cpp")
  string(FIND "${tactical_build_contents}"
    "${required_actor_domain_source}"
    actor_domain_build_entry)
  if(actor_domain_build_entry EQUAL -1)
    message(FATAL_ERROR
      "Tactical actor domain source '${required_actor_domain_source}' must remain in the tactical build")
  endif()
endforeach()

foreach(required_actor_aggregate_definition IN ITEMS
  "TacticalActor::~TacticalActor("
  "TacticalActor::TacticalActor("
  "TacticalActor::initialize("
  "TacticalActor::GetName(")
  string(FIND "${tactical_actor_aggregate_source_contents}"
    "${required_actor_aggregate_definition}"
    actor_aggregate_definition)
  if(actor_aggregate_definition EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor.cpp lost aggregate definition '${required_actor_aggregate_definition}'")
  endif()
endforeach()

string(REGEX MATCH
  "TacticalActor::(~TacticalActor|TacticalActor|initialize|GetName)[ \t\r\n]*\\("
  actor_aggregate_definition_in_monolith
  "${tactical_actor_source_contents}")
if(actor_aggregate_definition_in_monolith)
  message(FATAL_ERROR
    "A TacticalActor aggregate definition returned to Soldier Control.cpp")
endif()

foreach(required_actor_aggregate_coverage IN ITEMS
  "aggregateActor.GetName("
  "aggregateActor.initialize(")
  string(FIND "${headless_test_contents}"
    "${required_actor_aggregate_coverage}"
    actor_aggregate_coverage)
  if(actor_aggregate_coverage EQUAL -1)
    message(FATAL_ERROR
      "TacticalActor aggregate implementation lost headless coverage for '${required_actor_aggregate_coverage}'")
  endif()
endforeach()

string(REGEX MATCH
  "(^|\n)[A-Za-z_][A-Za-z0-9_:<>,*& \t]*TacticalActor(Assignments|CovertOps|Disease|Dragging|Equipment|Modifiers|Radio|Skills|Spotting|Turncoats)::[A-Za-z0-9_]+[ \t\r\n]*\\("
  actor_utility_definition_in_monolith
  "${tactical_actor_source_contents}")
if(actor_utility_definition_in_monolith)
  message(FATAL_ERROR
    "A physically extracted tactical actor domain definition returned to Soldier Control.cpp")
endif()

foreach(required_persistence_fragment IN ITEMS
  "ComputeTacticalActorChecksum"
  "SaveTacticalActor"
  "LoadTacticalActor")
  string(FIND
    "${tactical_actor_persistence_header_contents}"
    "${required_persistence_fragment}"
    persistence_declaration)
  string(FIND
    "${tactical_actor_persistence_source_contents}"
    "${required_persistence_fragment}"
    persistence_definition)
  if(persistence_declaration EQUAL -1 OR persistence_definition EQUAL -1)
    message(FATAL_ERROR
      "Explicit TacticalActor persistence lost '${required_persistence_fragment}'")
  endif()
endforeach()

foreach(required_persistence_operation IN ITEMS
  "XferTacticalActor(ar, actor);"
  "actor.initialize();"
  "actor.replication().recordChecksum( ComputeTacticalActorChecksum( actor ) );"
  "ComputeTacticalActorChecksum( actor ) != actor.replication().checksum()"
  "SaveTacticalActor(hFile, soldier)"
  "LoadTacticalActor(hFile, SavedSoldierInfo)")
  string(FIND "${tactical_actor_persistence_source_contents}"
    "${required_persistence_operation}"
    persistence_operation)
  if(persistence_operation EQUAL -1)
    message(FATAL_ERROR
      "Explicit TacticalActor persistence lost operation '${required_persistence_operation}'")
  endif()
endforeach()

string(REGEX MATCH
  "PORTABLE_SAVE_FORMAT[ \t]+1003"
  tactical_actor_save_baseline
  "${tactical_actor_save_version_contents}")
if(NOT tactical_actor_save_baseline)
  message(FATAL_ERROR
    "The layout-free TacticalActor schema must remain a deliberate save baseline")
endif()

# Portable save bytes need executable proof on every shipped host, independent
# of VFS initialization or installed game data. The representative schemas are
# production field lists rather than copies maintained only by the test.
file(READ "${SOURCE_ROOT}/tests/save_serializer_golden_tests.cpp"
  save_serializer_golden_test_contents)
file(READ "${SOURCE_ROOT}/docs/SAVE_FORMAT.md"
  save_serializer_format_document_contents)
file(READ "${SOURCE_ROOT}/.github/workflows/build_unix.yml"
  save_serializer_ci_contents)
foreach(required_save_schema_fragment IN ITEMS
    "XferSaveGameHeaderFields"
    "XferPathNodeFields")
  string(FIND "${tactical_actor_persistence_header_contents}"
    "${required_save_schema_fragment}" required_save_schema_header_position)
  string(FIND "${tactical_actor_persistence_source_contents}"
    "${required_save_schema_fragment}" required_save_schema_source_position)
  if(required_save_schema_header_position EQUAL -1 OR
     required_save_schema_source_position EQUAL -1)
    message(FATAL_ERROR
      "Portable save production schema lost '${required_save_schema_fragment}'")
  endif()
endforeach()
foreach(required_save_golden_fragment IN ITEMS
    "writer.u8(0xA5u)"
    "writer.u16(0x1234u)"
    "writer.u32(0x89ABCDEFu)"
    "writer.u64(UINT64_C(0x0123456789ABCDEF))"
    "writer.i8(std::numeric_limits<INT8>::min())"
    "writer.i16(std::numeric_limits<INT16>::min())"
    "writer.i32(std::numeric_limits<INT32>::min())"
    "writer.i64(std::numeric_limits<INT64>::min())"
    "writer.f32(-13.25f)"
    "writer.f64(-2.5)"
    "writer.boolean(FALSE)"
    "writer.wstr(wide, 4)"
    "writer.str8(narrow, 5)"
    "writer.bytes(opaque, 4)"
    "writer.skip(3)"
    "ar.slong(fixture.legacyLong)"
    "ar.ptr(fixture.runtimePointer)"
    "every SaveSerializer primitive has exact little-endian golden bytes"
    "non-ASCII wstr uses fixed 16-bit code units"
    "production save-header and path-node visitors match canonical bytes"
    "canonical save fixture is emitted and consumed without installed game data")
  string(FIND "${save_serializer_golden_test_contents}"
    "${required_save_golden_fragment}" required_save_golden_position)
  if(required_save_golden_position EQUAL -1)
    message(FATAL_ERROR
      "Portable save golden coverage lost '${required_save_golden_fragment}'")
  endif()
endforeach()
foreach(required_save_golden_build_fragment IN ITEMS
    "add_executable(save_serializer_golden_tests"
    "sgp/SaveSerializer.cpp"
    "add_test(NAME save_serializer_golden COMMAND save_serializer_golden_tests)")
  string(FIND "${tactical_test_build_contents}"
    "${required_save_golden_build_fragment}"
    required_save_golden_build_position)
  if(required_save_golden_build_position EQUAL -1)
    message(FATAL_ERROR
      "Portable save golden target lost '${required_save_golden_build_fragment}'")
  endif()
endforeach()
string(FIND "${save_serializer_ci_contents}"
  "save_serializer_golden_tests" save_serializer_asan_ci_position)
if(save_serializer_asan_ci_position EQUAL -1)
  message(FATAL_ERROR
    "Portable save golden target is no longer built by sanitizer CI")
endif()
foreach(required_save_document_fragment IN ITEMS
    "This was a portability audit"
    "not a wholesale serializer"
    "That golden fixture proves deterministic schema bytes"
    "campaign-state reconstruction remains a separate playtest obligation")
  string(FIND "${save_serializer_format_document_contents}"
    "${required_save_document_fragment}" required_save_document_position)
  if(required_save_document_position EQUAL -1)
    message(FATAL_ERROR
      "Portable save verification scope lost '${required_save_document_fragment}'")
  endif()
endforeach()

string(FIND "${tactical_actor_map_writer_contents}"
  "uiSoldierSize = 0;"
  layout_free_map_actor_marker)
if(layout_free_map_actor_marker EQUAL -1)
  message(FATAL_ERROR
    "Map headers must keep their reserved actor-size slot without exposing the C++ TacticalActor layout")
endif()

foreach(required_actor_test IN ITEMS
  "SaveTacticalActor( output, savedSoldier )"
  "LoadTacticalActor( input, loadedSoldier )"
  "actor save/load preserves the face index while detaching process-local world bindings"
  "soldier initialization resets every status and feature compatibility flag"
  "whole-soldier copies own independent strategic routes and record reuse releases the destination route")
  string(FIND "${headless_test_contents}"
    "${required_actor_test}"
    actor_test)
  if(actor_test EQUAL -1)
    message(FATAL_ERROR
      "Headless coverage lost final-state TacticalActor fixture '${required_actor_test}'")
  endif()
endforeach()

# Loaded-world turn state is owned exclusively by TacticalWorldSession. The
# former current-team and pending-combat fields no longer exist in
# TacticalStatusType, and the TURNBASED/INCOMBAT bits are composed only at
# persistence/editor compatibility boundaries. Gameplay must read the session
# accessors rather than recreating a second state path through uiFlags.
set(tactical_turn_owner "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp")
set(retired_tactical_turn_fields
  ubCurrentTeam
  ubAttackBusyCount)
foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  string(REGEX REPLACE "//[^\r\n]*" ""
    tactical_turn_executable "${contents}")
  foreach(retired_field IN LISTS retired_tactical_turn_fields)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_field}([^A-Za-z0-9_]|$)"
      retired_tactical_turn_field "${tactical_turn_executable}")
    if(retired_tactical_turn_field)
      message(FATAL_ERROR
        "Retired tactical-turn field '${retired_field}' returned in ${source_file}; use TacticalWorldSession through TacticalWorldAdapter")
    endif()
  endforeach()

  if(NOT "${source_file}" STREQUAL "${tactical_turn_owner}")
    string(REGEX MATCH
      "gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*uiFlags[ \t\r\n]*[&|^=]+[ \t\r\n]*(~[ \t\r\n]*)?(\\([^;\r\n]*(TURNBASED|INCOMBAT)|(TURNBASED|INCOMBAT))|(TURNBASED|INCOMBAT)[^;\r\n]*[&|^=]+[ \t\r\n]*gTacticalStatus[ \t\r\n]*\\.[ \t\r\n]*uiFlags"
      tactical_turn_flag_access "${tactical_turn_executable}")
    if(tactical_turn_flag_access)
      message(FATAL_ERROR
        "Production code reads or writes retired tactical turn/combat uiFlags in ${source_file}; use TacticalWorldAdapter accessors")
    endif()
  endif()
endforeach()

# Creature encounter quote state is another TacticalWorldSession domain. Its
# five former TacticalStatusType fields must not return as writable mirrors;
# application code supplies random delays and quote effects through the narrow
# adapter while persistence retains the established field order.
set(retired_creature_quote_fields
  fSaidCreatureFlavourQuote
  fHaveSeenCreature
  fSaidCreatureSmellQuote
  sCreatureTenseQuoteDelay
  uiCreatureTenseQuoteLastUpdate)
foreach(source_file IN LISTS world_state_declaration_files)
  file(READ "${source_file}" contents)
  string(REGEX REPLACE "//[^\r\n]*" ""
    creature_quote_executable "${contents}")
  foreach(retired_field IN LISTS retired_creature_quote_fields)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_])${retired_field}([^A-Za-z0-9_]|$)"
      retired_creature_quote_field "${creature_quote_executable}")
    if(retired_creature_quote_field)
      message(FATAL_ERROR
        "Retired creature-quote field '${retired_field}' returned in ${source_file}; use TacticalWorldSession through TacticalWorldAdapter")
    endif()
  endforeach()
endforeach()

foreach(creature_quote_contract IN ITEMS
    "${SOURCE_ROOT}/Engine/Adapters/JA2/TacticalWorldSession.h|creatureTenseQuoteDue"
    "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp|ResetJa2TacticalCreatureEncounterFlags"
    "${SOURCE_ROOT}/Strategic/strategicmap.cpp|SetJa2TacticalCreatureTenseQuoteDelay"
    "${SOURCE_ROOT}/Tactical/Overhead.cpp|RecordJa2TacticalCreatureTenseQuoteTime"
    "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp|RestoreJa2TacticalCreatureQuoteState"
    "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp|creature quote timing uses the runtime-owned strict deadline")
  string(REPLACE "|" ";" creature_quote_parts "${creature_quote_contract}")
  list(GET creature_quote_parts 0 creature_quote_file)
  list(GET creature_quote_parts 1 creature_quote_fragment)
  file(READ "${creature_quote_file}" creature_quote_contents)
  string(FIND "${creature_quote_contents}" "${creature_quote_fragment}"
    creature_quote_fragment_position)
  if(creature_quote_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Creature-quote session contract lost '${creature_quote_fragment}' in ${creature_quote_file}")
  endif()
endforeach()

# Campaign time identity is owned solely by EngineRuntime's
# CampaignClockSession. These former writable scalars have been retired; the
# established game reads value accessors backed by the session instead.
set(retired_campaign_clock_mirrors
  guiGameClock
  guiPreviousGameClock
  guiDay
  guiHour
  guiMin)
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  foreach(mirror IN LISTS retired_campaign_clock_mirrors)
    string(REGEX MATCH
      "(^|[^A-Za-z0-9_\"])${mirror}([^A-Za-z0-9_]|$)"
      retired_campaign_clock_use "${contents}")
    if(retired_campaign_clock_use)
      message(FATAL_ERROR
        "Retired campaign-clock scalar '${mirror}' returned in ${source_file}; read CampaignClockSession through CaptureJa2CampaignClock or GetWorld*")
    endif()
  endforeach()
endforeach()

# Campaign pacing is part of the fixed-step runtime. The strategic clock may
# retain presentation compatibility, but it must not quietly return to
# sampling a platform timer from the render loop.
set(campaign_clock_source "${SOURCE_ROOT}/Strategic/Game Clock.cpp")
file(READ "${campaign_clock_source}" campaign_clock_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])GetJA2Clock[ \t\r\n]*\\("
  campaign_clock_wall_time_sampling "${campaign_clock_contents}")
if(campaign_clock_wall_time_sampling)
  message(FATAL_ERROR
    "Strategic campaign time samples GetJA2Clock in ${campaign_clock_source}; pace it through CampaignClockScheduler")
endif()

set(game_loop_source "${SOURCE_ROOT}/Ja2/gameloop.cpp")
file(READ "${game_loop_source}" game_loop_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])UpdateClock[ \t\r\n]*\\("
  render_paced_campaign_clock "${game_loop_contents}")
if(render_paced_campaign_clock)
  message(FATAL_ERROR
    "The render loop calls legacy UpdateClock in ${game_loop_source}; campaign time belongs on the fixed-step sink")
endif()

# Strategic-event nodes and ordering are owned by EngineRuntime's
# CampaignEventQueue. The former gpEventList mirror has been retired; callers
# must query the live runtime-owned head when they begin a traversal.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])gpEventList([^A-Za-z0-9_]|$)"
    retired_campaign_event_head "${contents}")
  if(retired_campaign_event_head)
    message(FATAL_ERROR
      "Retired strategic-event head mirror returned in ${source_file}; begin traversal with GetStrategicEventListHead")
  endif()
endforeach()

# TacticalEntityDirectory owns the incarnation sequence directly. The former
# exported counter was unused outside the host and must not return as a second
# authority.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])guiCurrentUniqueSoldierId([^A-Za-z0-9_]|$)"
    retired_entity_sequence_mirror "${contents}")
  if(retired_entity_sequence_mirror)
    message(FATAL_ERROR
      "Retired tactical-entity incarnation mirror returned in ${source_file}; use TacticalEntityHost sequence gateways")
  endif()
endforeach()

# Package-facing tactical capture consumes state committed in the runtime-owned
# entity directory. TacticalActor projection belongs to the application host;
# putting pool or animation reads back in TacticalWorldAdapter would recreate a
# second actor-state path beside command execution.
file(READ "${SOURCE_ROOT}/Ja2/TacticalWorldAdapter.cpp"
  tactical_world_adapter_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(MercPtrs|TacticalActor|gAnimControl)([^A-Za-z0-9_]|$)"
  direct_tactical_world_actor_projection
  "${tactical_world_adapter_contents}")
if(direct_tactical_world_actor_projection)
  message(FATAL_ERROR
    "TacticalWorldAdapter reads legacy actor storage directly; publish state through TacticalEntityHost")
endif()
foreach(required_actor_state_fragment IN ITEMS
    "SynchronizeJa2TacticalEntityStates"
    "directory.state(entity)")
  string(FIND "${tactical_world_adapter_contents}"
    "${required_actor_state_fragment}" required_actor_state_position)
  if(required_actor_state_position EQUAL -1)
    message(FATAL_ERROR
      "TacticalWorldAdapter bypasses runtime-owned actor state; missing '${required_actor_state_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/TacticalEntityHost.cpp"
  tactical_entity_host_contents)
foreach(required_actor_projection_fragment IN ITEMS
    "TacticalActorSnapshot LegacyState"
    "publishState(LegacyState(soldier))"
    "TacticalEntityRoster& ActiveActorRoster()"
    "TacticalEntityRoster& AwayActorRoster()"
    "RebindRosterAfterRecordSwap(ActiveActorRoster())"
    "RebindRosterAfterRecordSwap(AwayActorRoster())"
    "RebindJa2StrategicSquadRostersAfterRecordSwap()"
    "RebindJa2VehicleOccupantsAfterRecordSwap()")
  string(FIND "${tactical_entity_host_contents}"
    "${required_actor_projection_fragment}" required_actor_projection_position)
  if(required_actor_projection_position EQUAL -1)
    message(FATAL_ERROR
      "TacticalEntityHost no longer commits live actor state; missing '${required_actor_projection_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/CMakeLists.txt"
  ja2_application_build_contents)
string(FIND "${ja2_application_build_contents}"
  "StrategicSquadHost.cpp" strategic_squad_host_build_position)
if(strategic_squad_host_build_position EQUAL -1)
  message(FATAL_ERROR
    "JA2 application no longer builds StrategicSquadHost.cpp")
endif()
string(FIND "${ja2_application_build_contents}"
  "VehiclePassengerHost.cpp" vehicle_passenger_host_build_position)
if(vehicle_passenger_host_build_position EQUAL -1)
  message(FATAL_ERROR
    "JA2 application no longer builds VehiclePassengerHost.cpp")
endif()

file(READ "${SOURCE_ROOT}/Ja2/StrategicSquadHost.cpp"
  strategic_squad_host_contents)
foreach(required_strategic_squad_host_fragment IN ITEMS
    "TacticalEntityRoster(kJa2StrategicSquadCapacity)"
    "ResetJa2StrategicSquadRosters"
    "ResolveJa2StrategicSquadActor"
    "AssignJa2StrategicSquadActor"
    "ActorBelongsToAnotherSquad"
    "RebindJa2StrategicSquadRostersAfterRecordSwap")
  string(FIND "${strategic_squad_host_contents}"
    "${required_strategic_squad_host_fragment}"
    required_strategic_squad_host_position)
  if(required_strategic_squad_host_position EQUAL -1)
    message(FATAL_ERROR
      "StrategicSquadHost lost exact-ID ownership fragment '${required_strategic_squad_host_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/VehiclePassengerHost.cpp"
  vehicle_passenger_host_contents)
foreach(required_vehicle_passenger_host_fragment IN ITEMS
    "TacticalEntityRoster(kJa2VehiclePassengerCapacity)"
    "ResetJa2VehicleOccupants"
    "ResolveJa2VehiclePassengerActor"
    "AssignJa2VehiclePassengerActor"
    "MoveJa2VehiclePassengerActor"
    "SwapJa2VehiclePassengerActors"
    "SetJa2VehicleDriverActor"
    "ActorBelongsToAnotherVehicle"
    "RebindJa2VehicleOccupantsAfterRecordSwap")
  string(FIND "${vehicle_passenger_host_contents}"
    "${required_vehicle_passenger_host_fragment}"
    required_vehicle_passenger_host_position)
  if(required_vehicle_passenger_host_position EQUAL -1)
    message(FATAL_ERROR
      "VehiclePassengerHost lost exact-ID ownership fragment '${required_vehicle_passenger_host_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Squads.cpp"
  strategic_squad_compatibility_contents)
foreach(required_strategic_squad_compatibility_fragment IN ITEMS
    "legacy squad save record layout must remain unchanged"
    "ResetJa2StrategicSquadRosters();"
    "ResolveJa2StrategicSquadActor("
    "GetJa2StrategicSquadActor("
    "AssignJa2StrategicSquadActor("
    "GetJa2TacticalEntityId("
    "CompactJa2StrategicSquad("
    "SortJa2StrategicSquadByIdentity(")
  string(FIND "${strategic_squad_compatibility_contents}"
    "${required_strategic_squad_compatibility_fragment}"
    required_strategic_squad_compatibility_position)
  if(required_strategic_squad_compatibility_position EQUAL -1)
    message(FATAL_ERROR
      "Squads.cpp bypasses pointer-free strategic membership; missing '${required_strategic_squad_compatibility_fragment}'")
  endif()
endforeach()

string(FIND "${tactical_actor_lifecycle_source_contents}"
  "RemoveJa2StrategicSquadActor(actor)"
  strategic_squad_delete_cleanup)
if(strategic_squad_delete_cleanup EQUAL -1)
  message(FATAL_ERROR
    "Soldier deletion no longer removes exact strategic squad membership")
endif()
string(FIND "${tactical_actor_lifecycle_source_contents}"
  "RemoveJa2VehiclePassengerActor(actor)"
  vehicle_passenger_delete_cleanup)
if(vehicle_passenger_delete_cleanup EQUAL -1)
  message(FATAL_ERROR
    "Soldier deletion no longer removes exact vehicle occupancy")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Vehicles.cpp"
  vehicle_compatibility_contents)
foreach(required_vehicle_compatibility_fragment IN ITEMS
    "MAX_VEHICLES == kJa2VehicleSlotCount"
    "MAXPASSENGERS == kJa2VehiclePassengerCapacity"
    "ResolveJa2VehiclePassengerActor("
    "AssignJa2VehiclePassengerActor("
    "RemoveJa2VehiclePassengerSeat("
    "MoveJa2VehiclePassengerActor("
    "SwapJa2VehiclePassengerActors("
    "ResolveJa2VehicleDriverActor("
    "SetJa2VehicleDriverActor("
    "savedPassengerProfiles[ ubPassengerCnt ] = r.u32()"
    "savedDriverSlot = r.u16()"
    "w.u32(uiPassengerID)"
    "driver ? driver->identity().id().i : NOBODY.i"
    "ubNumberOfVehicles > MAX_VEHICLES")
  string(FIND "${vehicle_compatibility_contents}"
    "${required_vehicle_compatibility_fragment}"
    required_vehicle_compatibility_position)
  if(required_vehicle_compatibility_position EQUAL -1)
    message(FATAL_ERROR
      "Vehicles.cpp bypasses pointer-free exact occupancy or changed its established persistence projection; missing '${required_vehicle_compatibility_fragment}'")
  endif()
endforeach()

# The application composition root exposes one repository object to JA2
# systems. Its fixed-capacity backing records and slot table are private to the
# repository implementation rather than process-global lookup APIs.
set(soldier_repository_source
  "${SOURCE_ROOT}/Ja2/SoldierRepository.cpp")
file(READ "${soldier_repository_source}"
  soldier_repository_contents)
foreach(required_repository_fragment IN ITEMS
    "TacticalActor soldierRecords[TOTAL_SOLDIERS];"
    "TacticalActor* soldierSlots[TOTAL_SOLDIERS];"
    "Ja2SoldierRepository(soldierRecords, soldierSlots, TOTAL_SOLDIERS)"
    "TacticalActor* Ja2SoldierRepository::replace"
    "bool Ja2SoldierRepository::swapRecords"
    "void BindJa2SoldierRepository")
  string(FIND "${soldier_repository_contents}"
    "${required_repository_fragment}" required_repository_position)
  if(required_repository_position EQUAL -1)
    message(FATAL_ERROR
      "JA2 soldier repository no longer owns '${required_repository_fragment}'")
  endif()
endforeach()
file(READ "${SOURCE_ROOT}/Ja2/SoldierRepository.h"
  soldier_repository_header_contents)
foreach(required_repository_gateway_fragment IN ITEMS
    "static Ja2SoldierRepository* boundRepository_"
    "inline Ja2SoldierRepository& GetJa2SoldierRepository")
  string(FIND "${soldier_repository_header_contents}"
    "${required_repository_gateway_fragment}"
    required_repository_gateway_position)
  if(required_repository_gateway_position EQUAL -1)
    message(FATAL_ERROR
      "JA2 soldier repository gateway lost '${required_repository_gateway_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/GameContext.cpp"
  game_context_source_contents)
string(FIND "${game_context_source_contents}"
  "BindJa2SoldierRepository(context.soldiers())"
  game_context_soldier_repository_binding)
string(FIND "${game_context_source_contents}"
  "BindJa2TacticalEntityDirectory("
  game_context_tactical_directory_binding)
if(game_context_soldier_repository_binding EQUAL -1 OR
    game_context_tactical_directory_binding EQUAL -1 OR
    game_context_soldier_repository_binding GREATER
      game_context_tactical_directory_binding)
  message(FATAL_ERROR
    "GameContext must bind the soldier repository before rebuilding the tactical entity directory")
endif()

file(READ "${SOURCE_ROOT}/Ja2/GameContext.h"
  game_context_header_contents)
string(FIND "${game_context_header_contents}"
  "Ja2SoldierRepository soldiers_;"
  game_context_soldier_repository_owner)
if(game_context_soldier_repository_owner EQUAL -1)
  message(FATAL_ERROR
    "GameContext must own the JA2 soldier repository")
endif()

file(GLOB ja2_application_sources
  "${SOURCE_ROOT}/Ja2/*.cpp")
foreach(source_file IN LISTS ja2_application_sources)
  if("${source_file}" STREQUAL "${soldier_repository_source}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
    direct_ja2_soldier_pool_access "${contents}")
  if(direct_ja2_soldier_pool_access)
    message(FATAL_ERROR
      "JA2 application code accesses legacy soldier arrays in ${source_file}; use GetJa2SoldierRepository")
  endif()
endforeach()

# SoldierID is now a numeric identity only. Storage resolution is unconditionally
# explicit, so the temporary per-target compile-definition migration machinery
# and every pointer conversion must stay retired.
file(READ "${SOURCE_ROOT}/Tactical/Overhead Types.h"
  soldier_id_header_contents)
foreach(soldier_build_file IN ITEMS
    "${SOURCE_ROOT}/CMakeLists.txt"
    "${SOURCE_ROOT}/lua/CMakeLists.txt"
    "${SOURCE_ROOT}/Multiplayer/CMakeLists.txt")
  file(READ "${soldier_build_file}" soldier_build_contents)
  string(FIND "${soldier_build_contents}"
    "JA2_EXPLICIT_SOLDIER_RESOLUTION"
    retired_soldier_resolution_gate)
  if(NOT retired_soldier_resolution_gate EQUAL -1)
    message(FATAL_ERROR
      "Retired SoldierID transition gate returned in ${soldier_build_file}")
  endif()
endforeach()

foreach(retired_pointer_conversion IN ITEMS
    "operator->()"
    "operator TacticalActor*"
    "operator const TacticalActor*")
  string(FIND "${soldier_id_header_contents}"
    "${retired_pointer_conversion}"
    retired_pointer_conversion_position)
  if(NOT retired_pointer_conversion_position EQUAL -1)
    message(FATAL_ERROR
      "SoldierID regained storage resolution through '${retired_pointer_conversion}'")
  endif()
endforeach()

# Tactical code must resolve numeric slots independently through the repository.
# This rejects both the retired process-global names and pointer arithmetic that
# would silently recreate a contiguous-storage dependency.
file(GLOB tactical_soldier_pool_consumers
  "${SOURCE_ROOT}/Tactical/*.cpp")
foreach(source_file IN LISTS tactical_soldier_pool_consumers)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
    direct_tactical_soldier_pool_access "${contents}")
  if(direct_tactical_soldier_pool_access)
    message(FATAL_ERROR
      "Tactical code accesses legacy soldier arrays in ${source_file}; use GetJa2SoldierRepository")
  endif()
  string(REGEX MATCH
    "(p[A-Za-z0-9_]*Soldier[ \t]*\\+\\+|\\+\\+[ \t]*p[A-Za-z0-9_]*Soldier([^A-Za-z0-9_.-]|$))"
    contiguous_tactical_soldier_walk "${contents}")
  if(contiguous_tactical_soldier_walk)
    message(FATAL_ERROR
      "Tactical code increments a soldier pointer in ${source_file}; traverse numeric slots through GetJa2SoldierRepository")
  endif()
endforeach()

# Strategic player movement groups retain exact tactical identities, not
# TacticalActor addresses or reusable numeric slots. The legacy save payload
# remains profile-only and load reconstructs the current exact incarnation.
file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.h"
  strategic_movement_header_contents)
string(REGEX MATCH
  "typedef struct PLAYERGROUP[^}]*}PLAYERGROUP;"
  player_group_declaration
  "${strategic_movement_header_contents}")
if(NOT player_group_declaration)
  message(FATAL_ERROR
    "PLAYERGROUP declaration is missing from Strategic Movement.h")
endif()
string(REGEX MATCH
  "TacticalEntityId[ \t\r\n]+actor"
  exact_player_group_member_identity
  "${player_group_declaration}")
if(NOT exact_player_group_member_identity)
  message(FATAL_ERROR
    "PLAYERGROUP must retain TacticalEntityId actor")
endif()
string(REGEX MATCH
  "TacticalActor[ \t\r\n]*\\*[ \t\r\n]*pSoldier"
  raw_player_group_member_pointer
  "${player_group_declaration}")
string(REGEX MATCH
  "SoldierID[ \t\r\n]+ubID"
  reusable_player_group_member_slot
  "${player_group_declaration}")
if(NOT "${raw_player_group_member_pointer}" STREQUAL "" OR
   NOT "${reusable_player_group_member_slot}" STREQUAL "")
  message(FATAL_ERROR
    "PLAYERGROUP regained raw or reusable soldier member storage")
endif()

file(READ "${SOURCE_ROOT}/Strategic/Strategic Movement.cpp"
  strategic_movement_source_contents)
foreach(required_group_identity_fragment IN ITEMS
    "ResolvePlayerGroupMember"
    "ResolveJa2TacticalEntity"
    "RemovePlayerFromStrategicGroups"
    "RebindStrategicGroupMembersAfterRecordSwap"
    "FindSoldierByProfileID"
    "uiProfileID = pTemp->ubProfileID"
    "uiNumberOfNodes != pTempGroup->ubGroupSize")
  string(FIND "${strategic_movement_source_contents}"
    "${required_group_identity_fragment}"
    required_group_identity_fragment_position)
  if(required_group_identity_fragment_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic movement exact-member lifecycle lost '${required_group_identity_fragment}'")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Ja2/TacticalEntityHost.cpp"
  tactical_entity_host_source_contents)
string(FIND "${tactical_entity_host_source_contents}"
  "RebindStrategicGroupMembersAfterRecordSwap();"
  strategic_group_swap_rebind)
if(strategic_group_swap_rebind EQUAL -1)
  message(FATAL_ERROR
    "Whole-record tactical actor swaps no longer rebind strategic movement members")
endif()

string(FIND "${tactical_actor_lifecycle_source_contents}"
  "RemovePlayerFromStrategicGroups(actor);"
  strategic_group_deletion_cleanup)
if(strategic_group_deletion_cleanup EQUAL -1)
  message(FATAL_ERROR
    "Soldier deletion no longer removes the released exact actor from strategic movement groups")
endif()

file(READ "${SOURCE_ROOT}/tests/ja2_headless_tests.cpp"
  ja2_headless_test_contents)
foreach(required_group_identity_test IN ITEMS
    "swappedMovementGroupMembersRebound"
    "releasedMovementGroupMemberRemoved"
    "strategic movement members retain the established count plus 32-bit profile-only save payload")
  string(FIND "${ja2_headless_test_contents}"
    "${required_group_identity_test}"
    required_group_identity_test_position)
  if(required_group_identity_test_position EQUAL -1)
    message(FATAL_ERROR
      "Strategic movement identity coverage lost '${required_group_identity_test}'")
  endif()
endforeach()

set(soldier_storage_source_directories
  Editor
  Engine
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  i18n
  lua
  sgp
  tests)
set(soldier_storage_sources)
foreach(source_directory IN LISTS soldier_storage_source_directories)
  file(GLOB_RECURSE source_files
    "${SOURCE_ROOT}/${source_directory}/*.cpp"
    "${SOURCE_ROOT}/${source_directory}/*.h"
    "${SOURCE_ROOT}/${source_directory}/*.hpp")
  list(APPEND soldier_storage_sources ${source_files})
endforeach()
list(REMOVE_DUPLICATES soldier_storage_sources)

foreach(source_file IN LISTS soldier_storage_sources)
  file(READ "${source_file}" contents)
  if(NOT source_file STREQUAL
       "${SOURCE_ROOT}/Tactical/TacticalActorRobotics.cpp" AND
     NOT source_file STREQUAL
       "${SOURCE_ROOT}/Tactical/Soldier Components.h" AND
     NOT source_file STREQUAL
       "${SOURCE_ROOT}/Ja2/SaveLoadGame.cpp" AND
     NOT source_file MATCHES "/tests/")
    string(FIND "${contents}"
      "robotRemoteHolder("
      direct_robot_controller_storage_access)
    if(NOT direct_robot_controller_storage_access EQUAL -1)
      message(FATAL_ERROR
        "Production code accesses robot controller storage in ${source_file}; use TacticalActorRobotics")
    endif()
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(MercSlots|AwaySlots|guiNumMercSlots|guiNumAwaySlots|GetFreeMercSlot|AddMercSlot|RemoveMercSlot|AddAwaySlot|RemoveAwaySlot)([^A-Za-z0-9_]|$)"
    retired_tactical_actor_roster_api
    "${contents}")
  if(retired_tactical_actor_roster_api)
    message(FATAL_ERROR
      "Retired raw tactical actor roster API returned in ${source_file}; use TacticalEntityHost exact-ID roster gateways")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])Squad[ \t\r\n]*\\[[^]]+\\][ \t\r\n]*\\["
    retired_strategic_squad_pointer_matrix
    "${contents}")
  if(retired_strategic_squad_pointer_matrix)
    message(FATAL_ERROR
      "Retired raw strategic Squad pointer matrix returned in ${source_file}; use ResolveSquadMember or StrategicSquadHost exact-ID gateways")
  endif()
  string(FIND "${contents}" "pPassengers"
    retired_vehicle_passenger_pointer_storage)
  if(NOT retired_vehicle_passenger_pointer_storage EQUAL -1)
    message(FATAL_ERROR
      "Retired raw vehicle passenger pointer storage returned in ${source_file}; use ResolveVehiclePassenger or VehiclePassengerHost exact-ID gateways")
  endif()
  string(REGEX MATCH
    "(p(Soldier|TeamSoldier|TSoldier|TargetSoldier|Merc)|get_npc[ \t\r\n]*\\([ \t\r\n]*\\)|GetSMCurrentMerc[ \t\r\n]*\\([ \t\r\n]*\\)|GetItemPointerSoldier[ \t\r\n]*\\([ \t\r\n]*\\)|MercSlots[ \t\r\n]*\\[[^]]+\\]|AwaySlots[ \t\r\n]*\\[[^]]+\\])[ \t\r\n]*->[ \t\r\n]*(ubID|name|ubBodyType|ubProfile|uiUniqueSoldierIdValue|usSoldierProfile|usIndividualMilitiaID|bActive|bTeam|bInSector|bSide|ubSoldierClass|ubCivilianGroup)([^A-Za-z0-9_]|$)"
    direct_retired_identity_roster_access
    "${contents}")
  if(direct_retired_identity_roster_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor identity/roster storage in ${source_file}; use identity() or roster()")
  endif()
  string(REGEX MATCH
    "(->|\\.)[ \t\r\n]*aiData([^A-Za-z0-9_]|$)"
    retired_soldier_ai_data_access
    "${contents}")
  if(retired_soldier_ai_data_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor aiData in ${source_file}; use the owning AI, awareness, perception, morale, turn, position, or combat component")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])ai_masterplan_([^A-Za-z0-9_]|$)"
    retired_modular_ai_plan_access
    "${contents}")
  if(retired_modular_ai_plan_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor ai_masterplan_ in ${source_file}; use aiPlan()")
  endif()
  string(REGEX MATCH
    "(->|\\.)[ \t\r\n]*pKeyRing([^A-Za-z0-9_]|$)"
    direct_retired_key_ring_access
    "${contents}")
  if(direct_retired_key_ring_access)
    message(FATAL_ERROR
      "Application code accesses retired TacticalActor pKeyRing in ${source_file}; use keyRing()")
  endif()
  if(NOT source_file MATCHES "/tests/")
    string(REGEX MATCH
      "(->|\\.)[ \t\r\n]*(pTempObject|pThrowParams)([^A-Za-z0-9_]|$)"
      direct_retired_pending_item_access
      "${contents}")
    if(direct_retired_pending_item_access)
      message(FATAL_ERROR
        "Application code accesses retired TacticalActor pending-item pointers in ${source_file}; use pendingItem()")
    endif()
    string(REGEX MATCH
      "(malloc|MemAlloc)[ \t\r\n]*\\([^)]*THROW_PARAMS"
      raw_throw_parameters_allocation
      "${contents}")
    if(raw_throw_parameters_allocation)
      message(FATAL_ERROR
        "Application code heap-allocates THROW_PARAMS in ${source_file}; use SoldierPendingItemComponent inline storage")
    endif()
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
    direct_migrated_soldier_pool_access "${contents}")
  if(direct_migrated_soldier_pool_access)
    message(FATAL_ERROR
      "Application code accesses retired soldier arrays in ${source_file}; use GetJa2SoldierRepository")
  endif()
  string(REGEX MATCH
    "(p(Soldier|TeamSoldier|Trainer|Student|Snitch|CheckedTrainer)[ \t]*\\+\\+|\\+\\+[ \t]*p(Soldier|TeamSoldier|Trainer|Student|Snitch|CheckedTrainer)([^A-Za-z0-9_.-]|$))"
    contiguous_migrated_soldier_walk "${contents}")
  if(contiguous_migrated_soldier_walk)
    message(FATAL_ERROR
      "Migrated application code increments a soldier pointer in ${source_file}; traverse numeric slots through GetJa2SoldierRepository")
  endif()
endforeach()

file(READ "${SOURCE_ROOT}/Tactical/Soldier Create.cpp"
  soldier_creation_contents)
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])(Menptr|MercPtrs)([^A-Za-z0-9_]|$)"
  direct_soldier_creation_pool_access
  "${soldier_creation_contents}")
if(direct_soldier_creation_pool_access)
  message(FATAL_ERROR
    "Soldier creation bypasses Ja2SoldierRepository")
endif()
string(FIND "${soldier_creation_contents}"
  "soldiers.replace("
  repository_soldier_creation)
if(repository_soldier_creation EQUAL -1)
  message(FATAL_ERROR
    "Soldier creation no longer commits complete records through Ja2SoldierRepository")
endif()

file(READ "${SOURCE_ROOT}/Tactical/Overhead.cpp"
  tactical_overhead_contents)
foreach(required_overhead_repository_fragment IN ITEMS
    "soldiers.initializeSlots()"
    "soldiers.resolve(cnt)"
    "ResetJa2TacticalActorRosters()")
  string(FIND "${tactical_overhead_contents}"
    "${required_overhead_repository_fragment}"
    required_overhead_repository_position)
  if(required_overhead_repository_position EQUAL -1)
    message(FATAL_ERROR
      "Tactical overhead lifecycle bypasses Ja2SoldierRepository; missing '${required_overhead_repository_fragment}'")
  endif()
endforeach()

foreach(required_exact_roster_deletion_fragment IN ITEMS
    "actor = GetJa2TacticalEntityId(subject)"
    "(void)ReleaseJa2TacticalEntity(subject)"
    "RemoveJa2ActiveTacticalActor(actor)"
    "RemoveJa2AwayTacticalActor(actor)")
  string(FIND "${tactical_actor_lifecycle_source_contents}"
    "${required_exact_roster_deletion_fragment}"
    required_exact_roster_deletion_position)
  if(required_exact_roster_deletion_position EQUAL -1)
    message(FATAL_ERROR
      "Soldier deletion no longer captures and removes exact tactical roster membership; missing '${required_exact_roster_deletion_fragment}'")
  endif()
endforeach()

string(FIND "${simulation_command_contents}"
  "SynchronizeExecutedCommandActors(command, disposition)"
  executed_actor_state_position)
if(executed_actor_state_position EQUAL -1)
  message(FATAL_ERROR
    "Production command execution no longer commits resulting actor state")
endif()

# Whole TacticalActor record relocation changes which incarnation occupies a
# legacy pool slot. Keep those rare mutations in the repository and let the
# entity host rebuild the runtime directory atomically after a repository swap.
set(entity_pool_owner "${SOURCE_ROOT}/Ja2/SoldierRepository.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${entity_pool_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "Menptr[ \t\r\n]*\\[[^\\]]+\\][ \t\r\n]*=[^=]|\\*[ \t\r\n]*MercPtrs[ \t\r\n]*\\[[^\\]]+\\][ \t\r\n]*=[^=]"
    entity_pool_record_write "${contents}")
  if(entity_pool_record_write)
    message(FATAL_ERROR
      "Production code relocates a complete tactical-entity pool record in ${source_file}; route the mutation through Ja2SoldierRepository and rebuild through TacticalEntityHost")
  endif()
endforeach()

# Expat parser allocation and release are an engine-adapter responsibility.
# Production loaders may borrow a parser during setup and keep their existing
# callbacks, but direct ownership would bypass bounded AssetSource reads,
# structured diagnostics, and the common lifetime guard.
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "XML_(ExternalEntity)?ParserCreate[ \t\r\n]*\\("
    direct_xml_parser_create "${contents}")
  if(direct_xml_parser_create)
    message(FATAL_ERROR
      "Production code directly creates an Expat parser in ${source_file}; use LegacyXmlDocument")
  endif()
endforeach()

# Raw mixer handles belong to the platform adapter. The sound manager provides
# that backend, but every production caller must use the stable Sound* gateway
# so engine-owned and legacy playback cannot alias each other's identifiers.
set(platform_audio_backend_owner "${SOURCE_ROOT}/sgp/soundman.cpp")
foreach(source_file IN LISTS world_state_files)
  if("${source_file}" STREQUAL "${platform_audio_backend_owner}")
    continue()
  endif()
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])PlatformSound[A-Za-z0-9_]*[ \t\r\n]*\\("
    direct_platform_audio_call "${contents}")
  if(direct_platform_audio_call)
    message(FATAL_ERROR
      "Production code bypasses the engine audio gateway in ${source_file}; use the public Sound* compatibility API")
  endif()
endforeach()

# Raw SDL frame output belongs to the platform adapters. The video manager
# implements that backend, while normal FrameDriver work and established
# RefreshScreen/PresentNow/Invalidate*/ColorFillVideoSurfaceArea callers cross
# engine-owned contracts.
file(READ "${SOURCE_ROOT}/sgp/DEBUG.cpp" assertion_handler_contents)
foreach(required_assertion_transition_fragment IN ITEMS
    "ScheduleErrorScreen"
    "SetPendingNewScreen(ERROR_SCREEN)"
    "throw std::runtime_error")
  string(FIND "${assertion_handler_contents}"
    "${required_assertion_transition_fragment}"
    required_assertion_transition_position)
  if(required_assertion_transition_position EQUAL -1)
    message(FATAL_ERROR
      "Assertion handling lost frame-safe error transition '${required_assertion_transition_fragment}'")
  endif()
endforeach()
string(REGEX MATCH
  "(^|[^A-Za-z0-9_])GameLoop[ \t\r\n]*\\("
  nested_assertion_game_loop "${assertion_handler_contents}")
if(nested_assertion_game_loop)
  message(FATAL_ERROR
    "Assertion handling recursively drives GameLoop; unwind to the outer FrameDriver before rendering errors")
endif()

set(platform_video_backend_owners
  "${SOURCE_ROOT}/sgp/sdl_video.cpp"
  "${SOURCE_ROOT}/sgp/sdl_vsurface.cpp"
  "${SOURCE_ROOT}/sgp/vobject.cpp"
  "${SOURCE_ROOT}/sgp/vobject_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_depth_queries.cpp"
  "${SOURCE_ROOT}/sgp/vobject_mask_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_multiz_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_image.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_blitters.cpp"
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_cache.cpp")
set(multiz_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_multiz_blitters.cpp")
set(depth_query_owner
  "${SOURCE_ROOT}/sgp/vobject_depth_queries.cpp")
set(mask_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_mask_blitters.cpp")
set(native_pixel_blitter_owner
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_blitters.cpp")
set(native_pixel_cache_owner
  "${SOURCE_ROOT}/sgp/vobject_native_pixel_cache.cpp")
set(native_image_import_owner
  "${SOURCE_ROOT}/sgp/vobject_native_image.cpp")
file(READ "${SOURCE_ROOT}/sgp/vobject.cpp" video_object_owner_contents)
string(REGEX MATCH
  "gVideoObjectHandles[ \\t\\r\\n]*\\.[ \\t\\r\\n]*find[ \\t\\r\\n]*\\("
  manager_only_render_identity "${video_object_owner_contents}")
if(manager_only_render_identity)
  message(FATAL_ERROR
    "Video-object drawing derives render identity from legacy manager membership; every CreateVideoObject lifetime must use its stable render-image identity")
endif()
foreach(source_file IN LISTS world_state_files)
  file(READ "${source_file}" contents)
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ColorFillVideoSurfaceArea[ \t\r\n]*\\("
    direct_surface_fill_implementation "${contents}")
  if(direct_surface_fill_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy surface-fill entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+BltVideoSurface[ \t\r\n]*\\("
    direct_surface_copy_implementation "${contents}")
  if(direct_surface_copy_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy numeric surface-copy entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+BltStretchVideoSurface[ \t\r\n]*\\("
    direct_surface_stretch_implementation "${contents}")
  if(direct_surface_stretch_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy numeric surface-stretch entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ShadowVideoSurfaceRect[ \t\r\n]*\\("
    direct_surface_shade_implementation "${contents}")
  if(direct_surface_shade_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy surface-shade entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "BOOLEAN[ \t\r\n]+ShadowVideoSurfaceRectUsingLowPercentTable[ \t\r\n]*\\("
    direct_surface_low_shade_implementation "${contents}")
  if(direct_surface_low_shade_implementation)
    message(FATAL_ERROR
      "Production code implements the legacy low-intensity surface-shade entry point in ${source_file}; keep it in LegacyRenderCommandGateway")
  endif()
  string(REGEX MATCH
    "(^|[^A-Za-z0-9_])memset[ \t\r\n]*\\([ \t\r\n]*gpZBuffer"
    direct_depth_buffer_clear "${contents}")
  if(direct_depth_buffer_clear)
    message(FATAL_ERROR
      "Production code clears gpZBuffer directly in ${source_file}; use RenderDepthFillCommand through the renderer gateway")
  endif()
  if(NOT "${source_file}" STREQUAL "${SOURCE_ROOT}/sgp/vobject.cpp")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+BltVideoObject[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_image_draw_implementation "${contents}")
    if(direct_image_draw_implementation)
      message(FATAL_ERROR
        "Production code implements a legacy video-object draw entry point in ${source_file}; keep image command translation in the video-object compatibility owner")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${multiz_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+Blt8BPPDataTo16BPPBufferTransZ(Inc|TransShadowInc)[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_multiz_blitter_implementation "${contents}")
    if(direct_multiz_blitter_implementation)
      message(FATAL_ERROR
        "Production code implements a raw multi-Z blitter in ${source_file}; keep strip-depth rasterization in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${mask_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+Zero8BPPDataTo16BPPBufferTransparent[A-Za-z0-9_]*[ \t\r\n]*\\("
      direct_mask_clear_implementation "${contents}")
    if(direct_mask_clear_implementation)
      message(FATAL_ERROR
        "Production code implements a raw image-mask clear in ${source_file}; keep footprint clearing in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${depth_query_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(Blt8BPPDataTo16BPPBufferTransInvZ|Query8BPPDataToDepthBufferOcclusion|IsTileRedundent)[ \t\r\n]*\\("
      direct_depth_query_implementation "${contents}")
    if(direct_depth_query_implementation)
      message(FATAL_ERROR
        "Production code implements a raw inverse-depth draw or visibility query in ${source_file}; keep depth reads in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_pixel_blitter_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(BltNativePixelDataToBufferTransparentClip|Blt16BPPDataTo16BPPBufferTransparentClip|BltNativePixelImageToBufferClip)[ \t\r\n]*\\("
      direct_native_pixel_blitter_implementation "${contents}")
    if(direct_native_pixel_blitter_implementation)
      message(FATAL_ERROR
        "Production code implements native-pixel rasterization in ${source_file}; keep it in the dedicated SGP backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_pixel_cache_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+(CacheVObjectRegionNativePixels|FindCachedVObjectNativePixelRegion|ConvertVObjectRegionTo16BPP|CheckFor16BPPRegion)[ \t\r\n]*\\("
      direct_native_pixel_cache_implementation "${contents}")
    if(direct_native_pixel_cache_implementation)
      message(FATAL_ERROR
        "Production code implements native-pixel sprite-cache ownership in ${source_file}; keep it in the dedicated SGP cache backend")
    endif()
  endif()
  if(NOT "${source_file}" STREQUAL "${native_image_import_owner}")
    string(REGEX MATCH
      "BOOLEAN[ \t\r\n]+ImportNativeVideoObjectImage[ \t\r\n]*\\("
      direct_native_image_import_implementation "${contents}")
    if(direct_native_image_import_implementation)
      message(FATAL_ERROR
        "Production code implements native true-colour image import in ${source_file}; keep byte-order and RGB565 conversion at the dedicated HIMAGE boundary")
    endif()
  endif()

  list(FIND platform_video_backend_owners
    "${source_file}" platform_video_backend_owner_index)
  if(NOT platform_video_backend_owner_index EQUAL -1)
    continue()
  endif()
  string(REGEX MATCH
    "Platform(DepthBuffer|Video(Surface|Object)?)Backend\\.h|(^|[^A-Za-z0-9_])Platform(DepthBuffer(Describe|Map|Unmap)|Video(Present|Invalidate|MarkFrameChanged|Surface|Object(DepthDraw|DepthVisibility|Draw|Outline))[A-Za-z0-9_]*)[ \t\r\n]*\\("
    direct_platform_video_access "${contents}")
  if(direct_platform_video_access)
    message(FATAL_ERROR
      "Production code bypasses an engine renderer gateway in ${source_file}; use FramePresenter, FrameInvalidator, RenderSurfaceAccess, RenderCommandSink, or the public compatibility API")
  endif()
endforeach()

# Soldier Control.cpp is retired. Support services with focused contracts must
# remain compiled from their owning translation units and must not regain
# second implementations in a replacement monolith.
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorCrowBehavior.cpp"
  tactical_actor_crow_behavior_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorDebug.cpp"
  tactical_actor_debug_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/TacticalActorEvents.cpp"
  tactical_actor_events_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Grid Direction.cpp"
  tactical_grid_direction_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Palette.cpp"
  tactical_soldier_palette_source_contents)
file(READ "${SOURCE_ROOT}/Tactical/Soldier Profile Records.cpp"
  tactical_soldier_profile_records_source_contents)

foreach(soldier_control_support_source IN ITEMS
    "Grid Direction.cpp"
    "Soldier Palette.cpp"
    "Soldier Profile Records.cpp"
    "TacticalActorCrowBehavior.cpp"
    "TacticalActorDebug.cpp"
    "TacticalActorEvents.cpp")
  string(FIND "${tactical_build_contents}"
    "${soldier_control_support_source}"
    soldier_control_support_build_entry)
  if(soldier_control_support_build_entry EQUAL -1)
    message(FATAL_ERROR
      "Extracted Soldier Control support source '${soldier_control_support_source}' must remain in the tactical build")
  endif()
endforeach()

foreach(soldier_control_support_operation IN ITEMS
    "tactical_actor_crow_behavior_source_contents|void HandleCrowShadowVisibility("
    "tactical_actor_crow_behavior_source_contents|void HandleCrowShadowNewGridNo("
    "tactical_actor_crow_behavior_source_contents|void HandleCrowShadowRemoveGridNo("
    "tactical_actor_crow_behavior_source_contents|void HandleCrowShadowNewDirection("
    "tactical_actor_crow_behavior_source_contents|void HandleCrowShadowNewPosition("
    "tactical_actor_crow_behavior_source_contents|void CrowsFlyAway("
    "tactical_actor_debug_source_contents|void DebugValidateSoldierData("
    "tactical_actor_events_source_contents|void SendSoldierPositionEvent("
    "tactical_actor_events_source_contents|void SendSoldierDestinationEvent("
    "tactical_actor_events_source_contents|void SendSoldierSetDirectionEvent("
    "tactical_actor_events_source_contents|void SendSoldierSetDesiredDirectionEvent("
    "tactical_actor_events_source_contents|void SendGetNewSoldierPathEvent("
    "tactical_actor_events_source_contents|void SendChangeSoldierStanceEvent("
    "tactical_actor_events_source_contents|void SendBeginFireWeaponEvent("
    "tactical_actor_locomotion_source_contents|void MoveMercFacingDirection("
    "tactical_grid_direction_source_contents|BOOLEAN GetDirectionChangeAmount("
    "tactical_grid_direction_source_contents|UINT8 GetDirectionFromGridNo("
    "tactical_grid_direction_source_contents|INT16 GetDirectionToGridNoFromGridNo("
    "tactical_grid_direction_source_contents|UINT8 GetDirectionFromXY("
    "tactical_grid_direction_source_contents|INT16 GetDirectionFromCenterCellXYGridNo("
    "tactical_grid_direction_source_contents|UINT8 atan8("
    "tactical_grid_direction_source_contents|UINT8 atan8FromAngle("
    "tactical_soldier_palette_source_contents|BOOLEAN LoadPaletteData("
    "tactical_soldier_palette_source_contents|BOOLEAN SetPaletteReplacement("
    "tactical_soldier_palette_source_contents|BOOLEAN DeletePaletteData("
    "tactical_soldier_palette_source_contents|BOOLEAN GetPaletteRepIndexFromID("
    "tactical_soldier_profile_records_source_contents|MERCPROFILEGEAR::MERCPROFILEGEAR("
    "tactical_soldier_profile_records_source_contents|MERCPROFILEGEAR& MERCPROFILEGEAR::operator=("
    "tactical_soldier_profile_records_source_contents|UINT32 MERCPROFILESTRUCT::GetChecksum("
    "tactical_soldier_profile_records_source_contents|OLD_MERCPROFILESTRUCT_101::OLD_MERCPROFILESTRUCT_101("
    "tactical_soldier_profile_records_source_contents|MERCPROFILESTRUCT::MERCPROFILESTRUCT("
    "tactical_soldier_profile_records_source_contents|MERCPROFILESTRUCT& MERCPROFILESTRUCT::operator=("
    "tactical_soldier_profile_records_source_contents|void MERCPROFILESTRUCT::CopyOldInventoryToNew(")
  string(REPLACE "|" ";" soldier_control_support_fields
    "${soldier_control_support_operation}")
  list(GET soldier_control_support_fields 0 soldier_control_support_owner)
  list(GET soldier_control_support_fields 1 soldier_control_support_definition)
  string(FIND "${${soldier_control_support_owner}}"
    "${soldier_control_support_definition}"
    soldier_control_support_owner_definition)
  string(FIND "${tactical_actor_source_contents}"
    "${soldier_control_support_definition}"
    soldier_control_support_monolith_definition)
  if(soldier_control_support_owner_definition EQUAL -1)
    message(FATAL_ERROR
      "Extracted support owner lost '${soldier_control_support_definition}'")
  endif()
  if(NOT soldier_control_support_monolith_definition EQUAL -1)
    message(FATAL_ERROR
      "Soldier Control.cpp regained extracted support definition '${soldier_control_support_definition}'")
  endif()
endforeach()

string(FIND "${headless_test_contents}"
  "soldier-control support services own crow, event, facing, direction, palette, and profile-record behavior"
  soldier_control_support_headless_coverage)
if(soldier_control_support_headless_coverage EQUAL -1)
  message(FATAL_ERROR
    "Extracted Soldier Control support services lost their combined headless behavior coverage")
endif()

# Utils runtime control has one portable persisted-key codec and one
# dependency-free clock scheduler. SDL/platform state belongs only to the
# KeyMap/Timer adapters, and campaign identity cannot return to the timer.
file(READ "${SOURCE_ROOT}/Utils/KeyMap.cpp" utils_key_map_contents)
file(READ "${SOURCE_ROOT}/Utils/KeyMap.h" utils_key_map_header_contents)
file(READ "${SOURCE_ROOT}/Utils/KeyBindingModel.cpp"
  utils_key_binding_model_contents)
file(READ "${SOURCE_ROOT}/Utils/KeyBindingModel.h"
  utils_key_binding_model_header_contents)
file(READ "${SOURCE_ROOT}/Utils/LegacyClockScheduler.cpp"
  utils_clock_scheduler_contents)
file(READ "${SOURCE_ROOT}/Utils/LegacyClockScheduler.h"
  utils_clock_scheduler_header_contents)
file(READ "${SOURCE_ROOT}/Utils/Timer Control.cpp"
  utils_timer_control_contents)
file(READ "${SOURCE_ROOT}/Utils/CMakeLists.txt" utils_build_contents)
file(READ "${SOURCE_ROOT}/tests/CMakeLists.txt" utils_test_build_contents)
file(READ "${SOURCE_ROOT}/tests/utils_runtime_control_model_tests.cpp"
  utils_runtime_control_test_contents)
file(READ "${SOURCE_ROOT}/.github/workflows/build_unix.yml"
  utils_runtime_control_ci_contents)
file(READ "${SOURCE_ROOT}/docs/UTILS_CODE_WALKTHROUGH.md"
  utils_walkthrough_contents)

foreach(retired_keymap_fragment IN ITEMS
    "GetAsyncKeyState"
    "#include <windows.h>"
    "#ifdef _WIN32"
    "#if defined(_WIN32)")
  string(FIND "${utils_key_map_contents}" "${retired_keymap_fragment}"
    retired_keymap_position)
  if(NOT retired_keymap_position EQUAL -1)
    message(FATAL_ERROR
      "KeyMap regained platform-split parsing/polling '${retired_keymap_fragment}'")
  endif()
endforeach()

foreach(required_keymap_fragment IN ITEMS
    "boundedKeyBindingTextLength(value)"
    "parsePackedKeyBinding(boundedValue)"
    "isPackedKeyBindingPressed"
    "class SdlLegacyKeyStateSource"
    "BindJA2KeyStateSource"
    "RestoreJA2PlatformKeyStateSource")
  string(FIND "${utils_key_map_contents}" "${required_keymap_fragment}"
    required_keymap_position)
  if(required_keymap_position EQUAL -1)
    message(FATAL_ERROR
      "Portable KeyMap boundary lost '${required_keymap_fragment}'")
  endif()
endforeach()

foreach(required_keymap_contract_fragment IN ITEMS
    "must not race"
    "must keep it alive until restoring"
    "Callbacks are type-enforced nonthrowing"
    "default adapter safely reports those as")
  string(FIND "${utils_key_map_header_contents}"
    "${required_keymap_contract_fragment}" required_keymap_contract_position)
  if(required_keymap_contract_position EQUAL -1)
    message(FATAL_ERROR
      "KeyMap injection contract lost '${required_keymap_contract_fragment}'")
  endif()
endforeach()

foreach(model_contents IN ITEMS
    utils_key_binding_model_contents
    utils_key_binding_model_header_contents
    utils_clock_scheduler_contents
    utils_clock_scheduler_header_contents)
  string(REGEX MATCH
    "#[ \t]*include[ \t]*[<\"](SDL|windows\\.h|types\\.h|TacticalActor|GameContext|PlatformTime)"
    forbidden_runtime_model_include "${${model_contents}}")
  if(forbidden_runtime_model_include)
    message(FATAL_ERROR
      "Dependency-free Utils runtime model '${model_contents}' regained a platform/game include")
  endif()
endforeach()

foreach(model_contents IN ITEMS
    utils_key_binding_model_contents utils_clock_scheduler_contents)
  string(REGEX MATCH
    "SDL|windows\\.h|GetAsyncKeyState|GetPlatformTimeSource|TacticalActor|GameContext"
    forbidden_runtime_model_dependency "${${model_contents}}")
  if(forbidden_runtime_model_dependency)
    message(FATAL_ERROR
      "Dependency-free Utils runtime model '${model_contents}' regained a platform/game dependency")
  endif()
endforeach()

string(REGEX MATCH
  "#[ \t]*(if|ifdef|ifndef|elif)[^\r\n]*JA2UB"
  utils_timer_compiled_campaign_identity "${utils_timer_control_contents}")
if(utils_timer_compiled_campaign_identity)
  message(FATAL_ERROR
    "Timer Control regained compiled JA2UB identity; route through actor runtime state")
endif()

foreach(required_timer_fragment IN ITEMS
    "LegacyClockScheduler gClockSchedule"
    "gClockSchedule.pump("
    "gClockSchedule.settleBeforeTransition("
    "BindJA2ClockSources("
    "arrivalGetupPending()"
    "deployment().arrivalGetupCounter()")
  string(FIND "${utils_timer_control_contents}" "${required_timer_fragment}"
    required_timer_position)
  if(required_timer_position EQUAL -1)
    message(FATAL_ERROR
      "Frame-owned Timer Control boundary lost '${required_timer_fragment}'")
  endif()
endforeach()

foreach(required_runtime_control_build_fragment IN ITEMS
    "KeyBindingModel.cpp"
    "LegacyClockScheduler.cpp")
  string(FIND "${utils_build_contents}"
    "${required_runtime_control_build_fragment}"
    required_runtime_control_build_position)
  if(required_runtime_control_build_position EQUAL -1)
    message(FATAL_ERROR
      "Utils build lost runtime-control model '${required_runtime_control_build_fragment}'")
  endif()
endforeach()

foreach(required_runtime_control_test_fragment IN ITEMS
    "parsePackedKeyBinding"
    "F4294967297"
    "MemoryKeyState"
    "settleBeforeTransition"
    "maximumRetainedDebtMicroseconds")
  string(FIND "${utils_runtime_control_test_contents}"
    "${required_runtime_control_test_fragment}"
    required_runtime_control_test_position)
  if(required_runtime_control_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils runtime-control model lost test coverage '${required_runtime_control_test_fragment}'")
  endif()
endforeach()
foreach(required_runtime_control_adapter_test_fragment IN ITEMS
    "unterminatedKeyBinding"
    "overlongKeyBinding"
    "bounds raw configuration reads"
    "injected hosts may implement persisted keys without SDL scancodes"
    "SDL adapter safely rejects persisted keys without scancodes")
  string(FIND "${headless_test_contents}"
    "${required_runtime_control_adapter_test_fragment}"
    required_runtime_control_adapter_test_position)
  if(required_runtime_control_adapter_test_position EQUAL -1)
    message(FATAL_ERROR
      "Utils runtime-control adapter lost coverage '${required_runtime_control_adapter_test_fragment}'")
  endif()
endforeach()
string(FIND "${utils_test_build_contents}"
  "utils_runtime_control_model_tests"
  utils_runtime_control_test_target_position)
string(FIND "${utils_runtime_control_ci_contents}"
  "utils_runtime_control_model_tests"
  utils_runtime_control_ci_position)
if(utils_runtime_control_test_target_position EQUAL -1 OR
   utils_runtime_control_ci_position EQUAL -1)
  message(FATAL_ERROR
    "Utils runtime-control standalone tests are no longer built by CMake/ASan CI")
endif()
string(FIND "${utils_walkthrough_contents}"
  "Runtime-control boundary: packed input and frame clock"
  utils_runtime_control_audit_position)
if(utils_runtime_control_audit_position EQUAL -1)
  message(FATAL_ERROR
    "Utils runtime-control audit record is missing")
endif()

# Phase 9 has one portable, bounded scalable-font adapter. Unicode decoding
# and size arithmetic stay dependency-free; SDL/VFS/stb/surface concerns stay
# in the production adapter.
file(READ "${SOURCE_ROOT}/sgp/WinFont.h" portable_font_header_contents)
file(READ "${SOURCE_ROOT}/sgp/WinFont.cpp" portable_font_adapter_contents)
file(READ "${SOURCE_ROOT}/sgp/Font.cpp" portable_font_caller_contents)
file(READ "${SOURCE_ROOT}/sgp/PortableFontModel.h"
  portable_font_model_contents)
file(READ "${SOURCE_ROOT}/sgp/CMakeLists.txt" portable_font_build_contents)
file(READ "${SOURCE_ROOT}/Utils/Font Control.cpp"
  portable_font_control_contents)
file(READ "${SOURCE_ROOT}/tests/portable_font_model_tests.cpp"
  portable_font_test_contents)
file(READ "${SOURCE_ROOT}/tests/CMakeLists.txt"
  portable_font_test_build_contents)
file(READ "${SOURCE_ROOT}/.github/workflows/build_unix.yml"
  portable_font_ci_contents)
file(READ "${SOURCE_ROOT}/docs/SDL3_PORT.md" sdl3_port_contents)

foreach(retired_font_stub_fragment IN ITEMS
    "inline void InitWinFonts()"
    "CreateWinFont(void*)"
    "inline void PrintWinFont"
    "#include <windows.h>")
  string(FIND "${portable_font_header_contents}"
    "${retired_font_stub_fragment}" retired_font_stub_position)
  if(NOT retired_font_stub_position EQUAL -1)
    message(FATAL_ERROR
      "Portable font header regained retired stub/platform path '${retired_font_stub_fragment}'")
  endif()
endforeach()

foreach(required_portable_font_fragment IN ITEMS
    "STB_TRUETYPE_IMPLEMENTATION"
    "MaximumFontBytes"
    "MaximumGlyphPixels"
    "CheckedBitmapArea"
    "DecodeNext"
    "GetVideoSurface"
    "LockVideoSurface"
    "surface->ubBitDepth <= 8"
    "BlendGlyph"
    "WIN_FONT_FILE"
    "WIN_FONT_BOLD_FILE"
    "g_lang == i18n::Lang::zh"
    "stbtt_FindGlyphIndex(&probe, 0x4E00)"
    "FONTS\\\\ja2font3.ttf"
    "catch (...)")
  string(FIND "${portable_font_adapter_contents}"
    "${required_portable_font_fragment}" required_portable_font_position)
  if(required_portable_font_position EQUAL -1)
    message(FATAL_ERROR
      "Portable font adapter lost '${required_portable_font_fragment}'")
  endif()
endforeach()

string(FIND "${portable_font_adapter_contents}"
  "surface->ubBitDepth != SGP_PIXEL_DEPTH"
  rejected_native_font_surface_gate_position)
if(NOT rejected_native_font_surface_gate_position EQUAL -1)
  message(FATAL_ERROR
    "Portable font adapter again rejects native 32-bit storage by its legacy 16-bit content tag")
endif()

string(FIND "${portable_font_caller_contents}"
  "SurfaceData::GetSurfaceID" mapped_font_surface_position)
string(FIND "${portable_font_caller_contents}"
  "PrintWinFont(CurrentSurface" stale_font_surface_position)
if(mapped_font_surface_position EQUAL -1 OR
   NOT stale_font_surface_position EQUAL -1)
  message(FATAL_ERROR
    "Portable buffer-font calls no longer resolve the exact mapped surface")
endif()

string(REGEX MATCH
  "#[ 	]*include[ 	]*[<\"](SDL|windows[.]h|types[.]h|FileMan|vsurface|stb_truetype)"
  forbidden_portable_font_model_include "${portable_font_model_contents}")
if(forbidden_portable_font_model_include)
  message(FATAL_ERROR
    "Portable font model regained a platform/game/parser dependency")
endif()

foreach(required_portable_font_model_test IN ITEMS
    "UTF-16 surrogate-pair decoding"
    "out-of-range UTF-32 is replaced"
    "signed-min height cannot overflow"
    "infinite scale is bounded"
    "exact-capacity glyph bitmap"
    "half coverage rounds deterministically"
    "width accumulation saturates")
  string(FIND "${portable_font_test_contents}"
    "${required_portable_font_model_test}"
    required_portable_font_model_test_position)
  if(required_portable_font_model_test_position EQUAL -1)
    message(FATAL_ERROR
      "Portable font model lost test '${required_portable_font_model_test}'")
  endif()
endforeach()

string(FIND "${portable_font_build_contents}" "WinFont.cpp"
  portable_font_build_position)
string(FIND "${portable_font_test_build_contents}"
  "portable_font_model_tests" portable_font_test_build_position)
string(FIND "${portable_font_ci_contents}"
  "portable_font_model_tests" portable_font_ci_position)
if(portable_font_build_position EQUAL -1 OR
   portable_font_test_build_position EQUAL -1 OR
   portable_font_ci_position EQUAL -1)
  message(FATAL_ERROR
    "Portable font implementation/model is no longer mandatory in build/ASan CI")
endif()
foreach(required_portable_font_control_fragment IN ITEMS
    "if (iUseWinFonts && !InitWinFonts()) iUseWinFonts = 0"
    "fTooltipScaleFactor > 1.0F"
    "ShutdownTooltipFonts()"
    "ShutdownWinFonts()")
  string(FIND "${portable_font_control_contents}"
    "${required_portable_font_control_fragment}"
    required_portable_font_control_position)
  if(required_portable_font_control_position EQUAL -1)
    message(FATAL_ERROR
      "Portable font lifecycle lost '${required_portable_font_control_fragment}'")
  endif()
endforeach()

string(REGEX REPLACE "[ \t\r\n]+" " " sdl3_port_normalized_contents
  "${sdl3_port_contents}")
foreach(required_portable_font_doc_fragment IN ITEMS
    "Exit criterion: met."
    "WIN_FONT_FILE"
    "per-section `Name`"
    "CJK-capable faces"
    "trusted local configuration/content boundary"
    "bitmap rendering remains active")
  string(FIND "${sdl3_port_normalized_contents}"
    "${required_portable_font_doc_fragment}"
    required_portable_font_doc_position)
  if(required_portable_font_doc_position EQUAL -1)
    message(FATAL_ERROR
      "SDL3 font completion record lost '${required_portable_font_doc_fragment}'")
  endif()
endforeach()

message(STATUS
  "Engine boundaries verified (Core: ${core_files}; Legacy adapter: ${legacy_adapter_files}; JA2 adapter: ${ja2_adapter_files})")
