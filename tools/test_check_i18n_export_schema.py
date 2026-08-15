#!/usr/bin/env python3
"""Focused tests for the build-free GameStrings export validator."""

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path
from unittest import mock


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
TOOL_PATH = TOOLS / "check_i18n_export_schema.py"
SPEC = importlib.util.spec_from_file_location("check_i18n_export_schema", TOOL_PATH)
EXPORT = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = EXPORT
SPEC.loader.exec_module(EXPORT)


def abi_symbol(entries: int) -> dict:
    return {
        "domain": "base",
        "type": "STR16",
        "source_dimensions": ["*"],
        "mutability": "mutable-pointer-slots-to-const-text",
        "conditional_layout": [],
        "declaration": "Text.h",
        "quadrants": {
            quadrant.name: {
                "effective_dimensions": [str(entries)],
                "initializer_entries": entries,
            }
            for quadrant in EXPORT.ABI.QUADRANTS
        },
    }


def empty_debt() -> dict:
    return {
        "missing_symbols": [],
        "extra_symbols": {},
        "symbols": {},
    }


class ExportSchemaToolTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.schema = json.loads(
            EXPORT.SCHEMA_PATH.read_text(encoding="utf-8"),
            object_pairs_hook=EXPORT.ABI.unique_json_object,
        )
        cls.abi_schema = json.loads(
            (EXPORT.ROOT / "i18n" / "text_abi_schema.json").read_text(
                encoding="utf-8"
            ),
            object_pairs_hook=EXPORT.ABI.unique_json_object,
        )

    def test_lexical_mask_blanks_raw_literals_and_keeps_digit_separators(self):
        source = r'''void Example()
{
    const auto value = 1'000;
    const auto letter = u8'A';
    const auto ignored = R"schema({ FakeCall(); })schema";
    RealCall();
}
'''
        masked = EXPORT.lexical_mask(source)
        self.assertIn("1'000", masked)
        self.assertNotIn("FakeCall", masked)
        self.assertNotIn("u8'A'", masked)
        self.assertIn("RealCall();", masked)
        self.assertEqual(masked.count("{"), 1)
        self.assertEqual(masked.count("}"), 1)

    def test_export_parser_ignores_comments_and_preserves_interleaved_ranges(self):
        source = r'''
void i18n::ExportSelectedCatalog(SelectedCatalogExportSink& sink)
{
    // ExportSection(sink, L"Commented", ::Old, 0, 9);
    const char* ordinary =
        "ExportSection(sink, L\"StringLiteral\", ::Literal, 0, 8);";
    const char* raw = R"schema(
        ExportSection(sink, L"RawLiteral", ::RawLiteral, 0, 7);
    )schema";
    ExportSection(sink, L"First", ::FirstTable, 0, LIMIT - 1);
    ExportTextPackEntry(sink, i18n::TextKey::Title);
    /* ExportTextPackTable(sink, i18n::TextTableKey::Old); */
    ExportTextPackTable(sink, i18n::TextTableKey::Times);
}
'''
        self.assertEqual(
            EXPORT.parse_export_calls(source),
            [
                {
                    "source_kind": "legacy",
                    "section": "First",
                    "symbol": "FirstTable",
                    "range": {"first": "0", "limit": "LIMIT-1"},
                },
                {"source_kind": "text-pack-entry", "key": "Title"},
                {"source_kind": "text-pack-table", "key": "Times"},
            ],
        )

    def test_export_parser_rejects_inactive_preprocessor_calls(self):
        source = r'''
void i18n::ExportSelectedCatalog(SelectedCatalogExportSink& sink)
{
#if 0
    ExportSection(sink, L"Fake", ::Fake, 0, 1);
#endif
}
'''
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "may not hide export calls"
        ):
            EXPORT.parse_export_calls(source)

    def test_export_parser_requires_direct_top_level_statements(self):
        wrappers = [
            'if (false)\n    ExportSection(sink, L"Fake", ::Fake, 0, 1);',
            '{ ExportTextPackEntry(sink, i18n::TextKey::Title); }',
            '[] { ExportTextPackTable(sink, i18n::TextTableKey::Times); }();',
        ]
        for wrapped in wrappers:
            with self.subTest(wrapped=wrapped):
                source = (
                    "void i18n::ExportSelectedCatalog("
                    "SelectedCatalogExportSink& sink)\n{\n"
                    + wrapped
                    + "\n}\n"
                )
                with self.assertRaisesRegex(
                    EXPORT.ExportSchemaError, "direct top-level statement"
                ):
                    EXPORT.parse_export_calls(source)

        production = EXPORT._read(EXPORT.EXPORT_SOURCE)
        early_control = {
            "return": "\treturn true;\n",
            "goto": "\tgoto skip_all_exports;\n",
            "throw": "\tthrow 1;\n",
            "extra declaration": "\tint unreviewed = 0;\n",
        }
        insertion = '#include "ExportStringLimitContract.inc"\n'
        for label, statement in early_control.items():
            with self.subTest(label=label):
                changed = production.replace(
                    insertion, insertion + statement, 1
                )
                with self.assertRaisesRegex(
                    EXPORT.ExportSchemaError, "statement inventory"
                ):
                    EXPORT.parse_export_calls(changed)

        digraph = production.replace(
            insertion,
            insertion + "%:if 0\n",
            1,
        ).replace(
            '\tExportSection(sink, L"MPChatbox"',
            '%:endif\n\tExportSection(sink, L"MPChatbox"',
            1,
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "alternative preprocessor tokens"
        ):
            EXPORT.parse_export_calls(digraph)

    def test_textual_catalog_include_scanner_rejects_raw_string_bypass(self):
        fake_chinese = r'''R"schema(
#include "_ChineseText.cpp"
)schema";
'''
        self.assertEqual(
            EXPORT._textual_catalog_includes(fake_chinese),
            [],
        )
        real = fake_chinese + '#include "_EnglishText.cpp"\n'
        self.assertEqual(
            EXPORT._textual_catalog_includes(real), ["_EnglishText.cpp"]
        )

    def test_selected_catalog_adapter_contract_is_linked_and_immediate_copy(self):
        adapter = EXPORT._read(EXPORT.EXPORT_SOURCE)
        writer = EXPORT._read(EXPORT.PROPERTY_EXPORT_SOURCE)
        header = EXPORT._read(EXPORT.EXPORT_HEADER)
        build = EXPORT._read(EXPORT.I18N_BUILD_SOURCE)
        self.assertEqual(
            EXPORT.selected_catalog_adapter_contract(
                adapter, writer, header, build
            )["textual_catalog_includes"],
            [],
        )
        mutations = {
            "textual catalog": (
                adapter + '\n#include "_EnglishText.cpp"\n', writer, header, build
            ),
            "retained view": (
                adapter,
                writer.replace(
                    "props_.setStringProperty(vfs::String(std::wstring(section)),",
                    "retained_ = text;\n"
                    "props_.setStringProperty(vfs::String(std::wstring(section)),",
                    1,
                ),
                header,
                build,
            ),
            "missing local extern": (
                adapter.replace("extern STR16 gzIntroScreen[];\n", "", 1),
                writer,
                header,
                build,
            ),
            "relative index": (
                adapter.replace(
                    "sink.copyEntry(section, index, text);",
                    "sink.copyEntry(section, index - first, text);",
                    1,
                ),
                writer,
                header,
                build,
            ),
            "empty values published": (
                adapter.replace(
                    "if (!text.empty()) sink.copyEntry(section, index, text);",
                    "sink.copyEntry(section, index, text);",
                    1,
                ),
                writer,
                header,
                build,
            ),
            "scalar wchar path removed": (
                adapter.replace("template<>\nvoid ExportSection<wchar_t>",
                                "template<>\nvoid RemovedExportSection<wchar_t>", 1),
                writer,
                header,
                build,
            ),
            "neutral adapter": (
                adapter,
                writer,
                header,
                build.replace(
                    "set(i18nVariantSrc",
                    '"${CMAKE_CURRENT_SOURCE_DIR}/SelectedCatalogExport.cpp"\n'
                    "set(i18nVariantSrc",
                    1,
                ),
            ),
        }
        for label, arguments in mutations.items():
            with self.subTest(label=label):
                with self.assertRaises(EXPORT.ExportSchemaError):
                    EXPORT.selected_catalog_adapter_contract(*arguments)

        original_read = EXPORT._read

        def mismatched_language(path):
            value = original_read(path)
            if path == "i18n/language.cpp":
                return value.replace(
                    "BuiltinTextCatalog().select(g_lang)",
                    "BuiltinTextCatalog().select(i18n::Lang::en)",
                    1,
                )
            return value

        with mock.patch.object(EXPORT, "_read", side_effect=mismatched_language):
            with self.assertRaisesRegex(
                EXPORT.ExportSchemaError, "no longer selects exactly once"
            ):
                EXPORT.selected_catalog_adapter_contract(
                    adapter, writer, header, build
                )

        def mismatched_compiled_language(path):
            value = original_read(path)
            if path == "i18n/CompiledLanguage.h":
                return value.replace(
                    "#if defined(ENGLISH)\n  return Lang::en;",
                    "#if defined(ENGLISH)\n  return Lang::de;",
                    1,
                )
            return value

        with mock.patch.object(
            EXPORT, "_read", side_effect=mismatched_compiled_language
        ):
            with self.assertRaisesRegex(
                EXPORT.ExportSchemaError, "macro-to-Lang mapping"
            ):
                EXPORT.selected_catalog_adapter_contract(
                    adapter, writer, header, build
                )

    def test_adapter_helpers_reject_literal_inactive_and_control_flow_spoofs(self):
        adapter = EXPORT._read(EXPORT.EXPORT_SOURCE)
        writer = EXPORT._read(EXPORT.PROPERTY_EXPORT_SOURCE)
        header = EXPORT._read(EXPORT.EXPORT_HEADER)
        build = EXPORT._read(EXPORT.I18N_BUILD_SOURCE)
        arguments = (writer, header, build)
        live_guard = "if (!text.empty()) sink.copyEntry(section, index, text);"

        raw_spoof = (
            adapter.replace(
                live_guard,
                "if (false) sink.copyEntry(section, index, text);",
                1,
            )
            + '\nconst char* spoof = R"ratchet('
            + EXPORT.EXPECTED_ADAPTER_HELPER_NAMESPACE
            + ')ratchet";\n'
        )
        inactive_namespace = adapter.replace(
            "namespace\n{", "#if 0\nnamespace\n{", 1
        ).replace(
            "\n}\n\nvoid i18n::ExportSelectedCatalog",
            "\n}\n#endif\n\nvoid i18n::ExportSelectedCatalog",
            1,
        )
        mutations = {
            "raw literal impersonation": raw_spoof,
            "inactive namespace": inactive_namespace,
            "disabled empty suppression": adapter.replace(
                "if (!text.empty())", "if (false && !text.empty())", 1
            ),
            "extra overload": adapter + "\nvoid ExportSection();\n",
        }
        for label, changed in mutations.items():
            with self.subTest(label=label):
                with self.assertRaises(EXPORT.ExportSchemaError):
                    EXPORT.selected_catalog_adapter_contract(changed, *arguments)

    def test_adapter_header_build_and_language_contracts_ignore_spoof_text(self):
        adapter = EXPORT._read(EXPORT.EXPORT_SOURCE)
        writer = EXPORT._read(EXPORT.PROPERTY_EXPORT_SOURCE)
        header = EXPORT._read(EXPORT.EXPORT_HEADER)
        build = EXPORT._read(EXPORT.I18N_BUILD_SOURCE)

        commented_build_entry = build.replace(
            '"${CMAKE_CURRENT_SOURCE_DIR}/SelectedCatalogExport.cpp"',
            '# "${CMAKE_CURRENT_SOURCE_DIR}/SelectedCatalogExport.cpp"',
            1,
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "exact direct ordered source list"
        ):
            EXPORT.selected_catalog_adapter_contract(
                adapter, writer, header, commented_build_entry
            )

        commented_header = header.replace(
            "class SelectedCatalogExportSink",
            "class RemovedSelectedCatalogExportSink",
            1,
        ) + "\n// class SelectedCatalogExportSink\n"
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "exact sink/entrypoint interface"
        ):
            EXPORT.selected_catalog_adapter_contract(
                adapter, writer, commented_header, build
            )

        original_read = EXPORT._read

        def commented_language_agreement(path):
            value = original_read(path)
            if path == "i18n/language.cpp":
                return value.replace(
                    "BuiltinTextCatalog().select(g_lang)",
                    "BuiltinTextCatalog().select(i18n::Lang::en) "
                    "/* BuiltinTextCatalog().select(g_lang) */",
                    1,
                )
            return value

        with mock.patch.object(
            EXPORT, "_read", side_effect=commented_language_agreement
        ):
            with self.assertRaisesRegex(
                EXPORT.ExportSchemaError, "no longer selects exactly once"
            ):
                EXPORT.selected_catalog_adapter_contract(
                    adapter, writer, header, build
                )

    def test_adapter_storage_paths_reject_scalar_overrun_and_unknown_rank(self):
        sections = copy.deepcopy(self.schema["sections"])
        pros = next(
            section for section in sections
            if section.get("symbol") == "gzProsLabel"
        )
        pros["range"]["limit"] = "2"
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "scalar wchar storage must export exactly"
        ):
            EXPORT._validate_legacy_storage_paths(sections, self.abi_schema)

        sections = copy.deepcopy(self.schema["sections"])
        changed_abi = copy.deepcopy(self.abi_schema)
        changed_abi["symbols"]["WeaponType"]["source_dimensions"].append("2")
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "unsupported adapter storage shape"
        ):
            EXPORT._validate_legacy_storage_paths(sections, changed_abi)

    def test_text_pack_descriptor_parser_pins_schema_sections_and_export_ranges(self):
        header = r'''
constexpr auto ignored = R"schema(
 {TextKey::Fake, "fake.key", L"FakeSection", false},
 {TextTableKey::FakeTable, "fake.table", L"FakeTableSection", 0, 99, 0, 99, false},
)schema";
inline constexpr auto TextKeys = {{
 {TextKey::Title, "screen.title", L"TitleSection", false},
}};
inline constexpr auto TextTables = {{
 {TextTableKey::Times, "screen.times", L"TimeStings", 4, 6, 1, 2, false},
}};
'''
        scalars, tables = EXPORT.parse_text_pack_descriptors(header)
        self.assertEqual(scalars["Title"]["schema"], "TextKey")
        self.assertEqual(scalars["Title"]["section"], "TitleSection")
        self.assertEqual(scalars["Title"]["export_count"], 1)
        self.assertEqual(tables["Times"]["schema"], "TextTableKey")
        self.assertEqual(tables["Times"]["entry_count"], 6)
        self.assertEqual(tables["Times"]["export_first"], 1)
        self.assertEqual(tables["Times"]["export_count"], 2)
        self.assertNotIn("Fake", scalars)
        self.assertNotIn("FakeTable", tables)
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "preprocessor conditionals"
        ):
            EXPORT.parse_text_pack_descriptors("#if 0\n" + header + "\n#endif\n")

        macro_only = r'''
#define FAKE_SCALAR {TextKey::Title, "screen.title", L"TitleSection", false}
#define FAKE_TABLE {TextTableKey::Times, "screen.times", L"TimeStings", 4, 6, 1, 2, false}
'''
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "active TextKeys descriptor array"
        ):
            EXPORT.parse_text_pack_descriptors(macro_only)

    def test_builtin_text_pack_payload_parser_requires_direct_literal_elements(self):
        source = EXPORT._read("i18n/TextCatalog.cpp")
        baseline = EXPORT._builtin_text_pack_values(source)
        conditional_row = source.replace(
            "\t{Lang::en,", "#if 1\n\t{Lang::en,", 1
        ).replace(
            "\n\t{Lang::de,", "\n#endif\n\t{Lang::de,", 1
        )
        mutations = {
            "pointer arithmetic": source.replace(
                'L"STRENGTH",', 'L"STRENGTH" + 1,', 1
            ),
            "comment-spoofed pointer arithmetic": source.replace(
                'L"STRENGTH",',
                'L"STRENGTH" + 1 /* L"STRENGTH" */,',
                1,
            ),
            "helper call": source.replace(
                'L"STRENGTH",', 'SelectText(L"STRENGTH"),', 1
            ),
            "conditional expression": source.replace(
                'L"STRENGTH",',
                'false ? L"WRONG" : L"STRENGTH",',
                1,
            ),
            "raw wide literal": source.replace(
                'L"STRENGTH",', 'LR"(STRENGTH)",', 1
            ),
            "conditional row": conditional_row,
        }
        for label, changed in mutations.items():
            with self.subTest(label=label):
                self.assertNotEqual(changed, source)
                with self.assertRaises(EXPORT.ExportSchemaError):
                    EXPORT._builtin_text_pack_values(changed)

        adjacent = source.replace(
            'L"STRENGTH",', 'L"STREN" L"GTH",', 1
        )
        self.assertEqual(EXPORT._builtin_text_pack_values(adjacent), baseline)

        raw_spoof = source + r'''
const char* ignored = R"schema(
constexpr auto BuiltinDefinitions{{
    {Lang::en, {L"fake"}, {L"fake"}},
}};
)schema";
'''
        self.assertEqual(EXPORT._builtin_text_pack_values(raw_spoof), baseline)

    def test_debt_range_model_classifies_short_catalogs_and_fails_closed(self):
        canonical = {"Names": abi_symbol(10)}
        overlays = {
            language.name: empty_debt()
            for language in EXPORT.ABI.LANGUAGES[1:]
        }
        overlays["German"]["symbols"]["Names"] = {
            "quadrants": {
                quadrant.name: {
                    "effective_dimensions": ["9"],
                    "initializer_entries": 9,
                }
                for quadrant in EXPORT.ABI.QUADRANTS
            }
        }
        abi_schema = {
            "symbols": canonical,
            "catalog_compatibility_debt": overlays,
        }
        section = {
            "source_kind": "legacy",
            "section": "Names",
            "symbol": "Names",
            "range": {"first": "0", "limit": "10"},
        }
        self.assertEqual(
            EXPORT._debt_inventory([section], abi_schema),
            [{
                "language": "German",
                "section": "Names",
                "symbol": "Names",
                "compatibility_fields": ["quadrants"],
                "unsafe_range": True,
                "actual_entries": 9,
                "export_limit": 10,
                "shortfall": 1,
            }],
        )
        section["range"]["limit"] = "UNREVIEWED_LIMIT"
        with self.assertRaisesRegex(EXPORT.ExportSchemaError, "cannot resolve"):
            EXPORT._debt_inventory([section], abi_schema)

    def test_named_limit_table_exactly_covers_export_and_raw_dimension_names(self):
        self.assertEqual(
            self.schema["named_export_limits"],
            dict(EXPORT.NAMED_EXPORT_LIMITS),
        )
        names = set()
        for section in self.schema["sections"]:
            if section["source_kind"] != "legacy":
                continue
            expression = section["range"]["limit"]
            match = EXPORT.re.fullmatch(r"([A-Za-z_]\w*)(?:[+-]\d+)?", expression)
            if match:
                names.add(match.group(1))
        exported_symbols = {
            section["symbol"]
            for section in self.schema["sections"]
            if section["source_kind"] == "legacy"
        }
        for language in EXPORT.ABI.LANGUAGES:
            source = EXPORT._read(language.base_source)
            definitions = EXPORT.ABI.parse_definitions(source, language.base_source)
            for symbol in exported_symbols:
                dimension = definitions[symbol]["source_dimensions"][0]
                if EXPORT.re.fullmatch(r"[A-Za-z_]\w*", dimension):
                    names.add(dimension)
        self.assertEqual(set(EXPORT.NAMED_EXPORT_LIMITS), names)
        self.assertEqual(
            set(EXPORT.NAMED_EXPORT_LIMITS),
            set(EXPORT.parse_named_export_limit_manifest(
                EXPORT._read(EXPORT.NAMED_EXPORT_LIMIT_MANIFEST)
            )),
        )
        self.assertEqual(len(EXPORT.NAMED_EXPORT_LIMITS), 77)
        self.assertTrue(
            {"NUM_CONTRACT_EXTEND", "NUM_SKI_ATM_BUTTONS", "TEXT_NUM_GIO_CFS"}
            .isdisjoint(EXPORT.NAMED_EXPORT_LIMITS)
        )
        self.assertEqual(EXPORT.NAMED_EXPORT_LIMITS["NUM_ICONS"], 18)
        self.assertEqual(EXPORT.NAMED_EXPORT_LIMITS["TEXT_NUM_AIM_ALUMNI"], 5)
        self.assertEqual(EXPORT.NAMED_EXPORT_LIMITS["TEXT_NUM_LARGESTR"], 3)
        self.assertEqual(
            EXPORT._resolve_export_limit("MAX_PRISONER_MENU_STRING_COUNT-1"),
            6,
        )

    def test_compiler_owned_named_limit_contract_is_strict_and_unconditional(self):
        contract = EXPORT._read(EXPORT.NAMED_EXPORT_LIMIT_MANIFEST)
        self.assertEqual(
            EXPORT.parse_named_export_limit_manifest(contract),
            dict(EXPORT.NAMED_EXPORT_LIMITS),
        )
        icon = (
            'static_assert(NUM_ICONS == 18, "GameStrings:NUM_ICONS");'
        )
        trap = (
            'static_assert(NUM_DOOR_TRAPS == 7, "GameStrings:NUM_DOOR_TRAPS");'
        )
        mutations = {
            "expression": contract.replace("NUM_ICONS == 18", "NUM_ICONS == 18 + 0", 1),
            "wrong message": contract.replace(
                '"GameStrings:NUM_ICONS"', '"GameStrings:STALE"', 1
            ),
            "duplicate": contract.replace(icon, trap, 1),
            "conditional": "#if 0\n" + contract + "#endif\n",
            "comment": contract.replace(icon, icon + " // hidden debt", 1),
            "reordered": contract.replace(trap + "\n" + icon, icon + "\n" + trap, 1),
        }
        for label, changed in mutations.items():
            with self.subTest(label=label):
                with self.assertRaises(EXPORT.ExportSchemaError):
                    EXPORT.parse_named_export_limit_manifest(changed)

        export_source = EXPORT._read(EXPORT.EXPORT_SOURCE)
        self.assertEqual(export_source.count('#include "GameSettings.h"'), 1)
        self.assertLess(
            export_source.index('#include "GameSettings.h"'),
            export_source.index('#include "Text.h"'),
        )
        EXPORT.validate_named_limit_static_assert_seam(export_source)
        export_masked = EXPORT.lexical_mask(export_source)
        export_definition = list(
            EXPORT.EXPORT_FUNCTION_DEFINITION.finditer(export_masked)
        )[0]
        export_open = export_masked.find(
            "{", export_definition.start(), export_definition.end()
        )
        export_close = EXPORT._matching_delimiter(
            export_masked, export_open, "{", "}"
        )
        for label, opener in {"if-zero": "#if 0", "ifdef": "#ifdef DISABLED_EXPORT"}.items():
            with self.subTest(label=f"function wrapper {label}"):
                wrapped = (
                    export_source[:export_definition.start()]
                    + opener
                    + "\n"
                    + export_source[export_definition.start():export_close + 1]
                    + "\n#endif"
                    + export_source[export_close + 1:]
                )
                for validator in (
                    EXPORT.validate_named_limit_static_assert_seam,
                    EXPORT.parse_export_calls,
                ):
                    with self.assertRaisesRegex(
                        EXPORT.ExportSchemaError, "unconditional top-level"
                    ):
                        validator(wrapped)
        seam = "\n".join(EXPORT.EXPECTED_EXPORT_LIMIT_SEAM_DIRECTIVES) + "\n"
        moved = export_source.replace(
            seam,
            "",
            1,
        ).replace(
            '\tExportSection(sink, L"WeaponType"',
            seam.rstrip() + '\n\tExportSection(sink, L"WeaponType"',
            1,
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "exact static_assert guard|exact adapter prefix"
        ):
            EXPORT.validate_named_limit_static_assert_seam(moved)

        source_mutations = {
            "provider macro": "#define NUM_ICONS 18",
            "assert keyword macro": "#define static_assert(...) ",
            "export helper macro": "#define ExportSection(...) ((void)0)",
            "provider digraph": "%:define NUM_ICONS 18",
            "provider splice": "#def\\\nine NUM_ICONS 18",
        }
        for label, directive in source_mutations.items():
            with self.subTest(label=label):
                shadowed = export_source.replace(
                    '#include "Assignments.h"',
                    directive + '\n#include "Assignments.h"',
                    1,
                )
                with self.assertRaises(EXPORT.ExportSchemaError):
                    EXPORT.validate_named_limit_static_assert_seam(shadowed)

        parameter = export_source.replace(
            "void i18n::ExportSelectedCatalog(SelectedCatalogExportSink& sink)",
            "void i18n::ExportSelectedCatalog("
            "SelectedCatalogExportSink& sink, int NUM_ICONS)",
            1,
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "exact sink entrypoint"
        ):
            EXPORT.validate_named_limit_static_assert_seam(parameter)

        local_shadows = {
            "constexpr": "constexpr int NUM_ICONS = 19;",
            "enum": "enum { NUM_ICONS = 19 };",
            "using": "using NUM_ICONS = int;",
            "structured binding": "auto [NUM_ICONS] = std::array<int, 1>{19};",
        }
        contract_include = '#include "ExportStringLimitContract.inc"\n'
        for label, declaration in local_shadows.items():
            with self.subTest(label=label):
                shadowed = export_source.replace(
                    contract_include,
                    contract_include + declaration + "\n",
                    1,
                )
                with self.assertRaisesRegex(
                    EXPORT.ExportSchemaError, "escaped parsed range ownership"
                ):
                    EXPORT.parse_export_calls(shadowed)

    def test_startup_model_rejects_import_before_export(self):
        source = r'''
bool InitializeLegacyContentBoundary()
{
    if(g_bUseXML_Strings)
    {
        if(s_bExportStrings) Loc::ExportStrings();
        Loc::ImportStrings();
    }
    InitJA2SplashScreen();
    return true;
}
SubsystemRuntime& GetStandardGamingPlatformRuntime()
{
    static SubsystemRuntime runtime({
        SubsystemDefinition{"legacy content", InitializeLegacyContentBoundary, [] {}, 55},
        SubsystemDefinition{"game", InitializeGameBoundary, [] {}, 0}});
    return runtime;
}
bool InitializeGameBoundary()
{
    return InitializeGame();
}
'''
        contract = EXPORT.legacy_startup_contract(source)
        self.assertEqual(
            contract["legacy_boundary"]["ordered_calls"],
            [
                "Loc::ExportStrings()",
                "Loc::ImportStrings()",
                "InitJA2SplashScreen()",
            ],
        )
        reversed_source = source.replace(
            "if(s_bExportStrings) Loc::ExportStrings();\n        Loc::ImportStrings();",
            "Loc::ImportStrings();\n        if(s_bExportStrings) Loc::ExportStrings();",
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "exact startup-only|lost exact required order"
        ):
            EXPORT.legacy_startup_contract(reversed_source)
        raw_literal_bypass = reversed_source.replace(
            "    if(g_bUseXML_Strings)",
            r'''    const char* ignored = R"schema(
        if(s_bExportStrings) Loc::ExportStrings();
        Loc::ImportStrings();
    )schema";
    if(g_bUseXML_Strings)''',
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "exact startup-only|lost exact required order"
        ):
            EXPORT.legacy_startup_contract(raw_literal_bypass)

        inactive_bypass = reversed_source.replace(
            "    {\n        Loc::ImportStrings();",
            """    {
#if 0
        if(s_bExportStrings) Loc::ExportStrings();
        Loc::ImportStrings();
#endif
        Loc::ImportStrings();""",
            1,
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "preprocessor directives"
        ):
            EXPORT.legacy_startup_contract(inactive_bypass)

    def test_external_text_load_sites_remain_post_startup(self):
        contract = EXPORT.startup_contract()
        self.assertEqual(
            contract["subsystem_order_constraint"],
            ["legacy content", "game"],
        )
        external = contract["external_load_contract"]
        self.assertEqual(external["production_call_sites"], 2)
        self.assertEqual(
            external["post_startup_rules_call"]["phase_owner"],
            "LegacyRulesPackage/LoadContent",
        )
        self.assertEqual(
            external["later_multiplayer_reload"]["function"],
            "DoneFadeOutForExitMPCScreen",
        )

    def test_startup_helpers_reject_inactive_or_nested_load_impersonation(self):
        conditional = """
void Reload()
{
#if 0
    LoadAllExternalText();
#endif
}
"""
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "preprocessor directives"
        ):
            EXPORT._condition_free_function_body(
                conditional, "Reload", "reload fixture"
            )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "not a direct statement"
        ):
            EXPORT._require_direct_statement(
                "if (false) { LoadAllExternalText(); }",
                "LoadAllExternalText()",
                "reload fixture",
            )

        for label, body in {
            "digraph": "%:if 0\nLoadAllExternalText();\n%:endif",
            "macro": "#define LOAD_TEXT LoadAllExternalText\nLOAD_TEXT();",
            "continued": "#if\\\n 0\nLoadAllExternalText();\n#endif",
        }.items():
            with self.subTest(label=label):
                fixture = "void Reload()\n{\n" + body + "\n}\n"
                with self.assertRaises(EXPORT.ExportSchemaError):
                    EXPORT._condition_free_function_body(
                        fixture, "Reload", "reload fixture"
                    )

        split_conditional = """
#if 0
advancePackagesTo(
    PackageBootstrapPhase::LoadContent)
#endif
"""
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "reviewed startup token is conditional"
        ):
            EXPORT._reject_conditionally_nested_tokens(
                split_conditional,
                ["advancePackagesTo(PackageBootstrapPhase::LoadContent)"],
                "startup fixture",
            )

        for transfer in ("return true;", "goto done;", "throw 1;"):
            with self.subTest(transfer=transfer):
                with self.assertRaisesRegex(
                    EXPORT.ExportSchemaError, "direct control transfer"
                ):
                    EXPORT._require_direct_statement(
                        transfer + "\nLoadAllExternalText();",
                        "LoadAllExternalText()",
                        "reload fixture",
                    )

    def test_startup_chain_definitions_and_calls_remain_active_and_reachable(self):
        active_functions = (
            ("sgp/sgp.cpp", "InitializeLegacyContentBoundary"),
            ("sgp/sgp.cpp", "GetStandardGamingPlatformRuntime"),
            ("sgp/sgp.cpp", "InitializeGameBoundary"),
            ("Ja2/CompiledGameplayBootstrap.cpp", "loadRulesContent"),
            ("Ja2/RulesPackage.cpp", "LegacyRulesPackage::bootstrap"),
            ("Ja2/Init.cpp", "InitializeJA2"),
            ("Ja2/MPConnectScreen.cpp", "DoneFadeOutForExitMPCScreen"),
        )
        for path, function in active_functions:
            with self.subTest(function=function):
                source = EXPORT._read(path)
                start, _opening, closing = EXPORT._function_definition_span(
                    source, function
                )
                line_start = source.rfind("\n", 0, start) + 1
                wrapped = (
                    source[:line_start]
                    + "#if 0\n"
                    + source[line_start:closing + 1]
                    + "\n#endif"
                    + source[closing + 1:]
                )
                with self.assertRaisesRegex(
                    EXPORT.ExportSchemaError, "unconditional top-level"
                ):
                    EXPORT._active_function_body(wrapped, function, function)

        sgp_source = EXPORT._read("sgp/sgp.cpp")
        boundary_marker = "bool InitializeLegacyContentBoundary()\n{\n"
        for transfer in ("return true;", "goto skipped;", "throw 1;"):
            with self.subTest(boundary_transfer=transfer):
                changed = sgp_source.replace(
                    boundary_marker, boundary_marker + "\t" + transfer + "\n", 1
                )
                with self.assertRaisesRegex(
                    EXPORT.ExportSchemaError, "direct control transfer"
                ):
                    EXPORT.legacy_startup_contract(changed)

        original_read = EXPORT._read

        def assert_startup_rejects(path, changed, message):
            def changed_read(candidate):
                return changed if candidate == path else original_read(candidate)

            with mock.patch.object(EXPORT, "_read", side_effect=changed_read):
                with self.assertRaisesRegex(EXPORT.ExportSchemaError, message):
                    EXPORT.startup_contract()

        compiled_path = "Ja2/CompiledGameplayBootstrap.cpp"
        compiled = original_read(compiled_path)
        assert_startup_rejects(
            compiled_path,
            compiled.replace(
                "loaded = LoadExternalGameplayData(TABLEDATA_DIRECTORY, false),",
                "loaded = false && LoadExternalGameplayData(TABLEDATA_DIRECTORY, false),",
                1,
            ),
            "directly assign",
        )

        rules_path = "Ja2/RulesPackage.cpp"
        rules = original_read(rules_path)
        rules_marker = (
            "bool LegacyRulesPackage::bootstrap(\n"
            "\tPackageBootstrapContext&, PackageBootstrapPhase phase)\n{\n"
        )
        assert_startup_rejects(
            rules_path,
            rules.replace(rules_marker, rules_marker + "\treturn true;\n", 1),
            "direct control transfer",
        )
        assert_startup_rejects(
            rules_path,
            rules.replace(
                "if (!bootstrapHost_.loadRulesContent(capabilities_)) return false;",
                "if (false && !bootstrapHost_.loadRulesContent(capabilities_)) return false;",
                1,
            ),
            "exact host-load condition",
        )

        init_path = "Ja2/Init.cpp"
        initialize = original_read(init_path)
        init_marker = "UINT32 InitializeJA2(void)\n{\n"
        assert_startup_rejects(
            init_path,
            initialize.replace(
                init_marker, init_marker + "\treturn ERROR_SCREEN;\n", 1
            ),
            "direct control transfer",
        )
        advance = (
            "gameContext.advancePackagesTo("
            "PackageBootstrapPhase::LoadContent)"
        )
        assert_startup_rejects(
            init_path,
            initialize.replace(
                advance,
                "false ? " + advance + " : gameContext.advancePackagesTo("
                "PackageBootstrapPhase::Configure)",
                1,
            ),
            "directly initialize contentLoad",
        )

    def test_committed_manifest_pins_all_238_sections_and_storage_membership(self):
        self.assertEqual(EXPORT.validate_manifest_contract(self.schema), [])
        self.assertEqual(len(self.schema["sections"]), 238)
        self.assertEqual(
            self.schema["catalog_adapter"]["linked_catalog_sources"],
            EXPORT.EXPECTED_LINKED_CATALOG_SOURCES,
        )
        self.assertEqual(
            self.schema["catalog_adapter"]["textual_catalog_includes"], []
        )
        self.assertEqual(
            self.schema["catalog_adapter"]["local_extern_symbols"],
            EXPORT.EXPECTED_LOCAL_EXTERN_SYMBOLS,
        )
        self.assertEqual(
            [entry["source_kind"] for entry in self.schema["sections"]].count("legacy"),
            210,
        )
        self.assertEqual(len(self.schema["legacy_symbols"]), 210)
        self.assertEqual(self.schema["counts"]["legacy_text_h_symbols"], 205)
        self.assertEqual(self.schema["counts"]["legacy_local_extern_symbols"], 5)
        self.assertTrue(
            all(
                storage["schema_domain"] == "base"
                for storage in self.schema["legacy_symbols"].values()
            )
        )
        self.assertEqual(self.schema["legacy_symbols"]["Message"]["type"], "CHAR16")
        self.assertEqual(self.schema["legacy_symbols"]["Message"]["rank"], 2)
        self.assertEqual(self.schema["legacy_symbols"]["gzProsLabel"]["rank"], 1)
        self.assertEqual(
            self.schema["legacy_symbols"]["pAssignmentStrings"]["mutability"],
            "mutable-pointer-slots-to-const-text",
        )

    def test_exact_17_debt_pairs_and_exhaustive_zero_unsafe_gate_are_reviewable(self):
        debt = self.schema["exported_compatibility_debt"]
        self.assertEqual(len(debt), 17)
        unsafe = {
            (entry["language"], entry["symbol"])
            for entry in debt
            if entry["unsafe_range"]
        }
        self.assertEqual(unsafe, set())
        self.assertEqual(
            self.schema["legacy_range_contract"],
            {
                "comparisons": 6720,
                "unsafe_sections": 0,
                "unsafe_language_pairs": 0,
                "unsafe_quadrant_failures": 0,
                "potential_oob_reads_per_selected_build": 0,
                "exported_pointer_entry_checks": 83040,
                "direct_wide_literal_entry_checks": 82712,
                "compiled_selector_entry_checks": 328,
            },
        )
        self.assertTrue(
            {
                ("German", "TacticalStr"),
                ("German", "pBookMarkStrings"),
                ("Russian", "pBookMarkStrings"),
                ("Dutch", "TacticalStr"),
                ("Dutch", "pBookMarkStrings"),
                ("Dutch", "pPersonnelScreenStrings"),
                ("Polish", "TacticalStr"),
                ("Polish", "TeamTurnString"),
                ("Polish", "pBookMarkStrings"),
                ("French", "TacticalStr"),
                ("French", "pBookMarkStrings"),
                ("Italian", "Message"),
                ("Italian", "TacticalStr"),
                ("Italian", "pBookMarkStrings"),
            }.isdisjoint((entry["language"], entry["symbol"]) for entry in debt)
        )
        self.assertEqual(
            self.schema["adapter_status"], EXPORT.IMPLEMENTED_ADAPTER_STATUS
        )

    def test_ordered_output_bytes_and_all_build_quadrants_are_pinned(self):
        output = self.schema["output_contract"]
        self.assertEqual(output["empty_values"], "suppressed")
        self.assertEqual(output["indexing"], "absolute source index")
        self.assertEqual(len(output["snapshots"]), 32)
        self.assertEqual(
            output["snapshots"]["English/ja2-release"],
            {
                "emitted_sections": 237,
                "emitted_entries": 3078,
                "value_utf8_bytes": 69893,
                "ordered_payload_sha256": (
                    "2248046a8cbcce195213baddb848ac897a153bafaff55f8f2d207b699b8a3902"
                ),
            },
        )
        self.assertEqual(
            set(output["snapshots"]),
            {
                f"{language.name}/{quadrant.name}"
                for language in EXPORT.ABI.LANGUAGES
                for quadrant in EXPORT.ABI.QUADRANTS
            },
        )
        self.assertEqual(
            self.schema["catalog_adapter"]["language_agreement"],
            "one language-target definition selects linked globals, g_lang, and TextPack",
        )

    def test_all_19_foreign_and_104_universal_repairs_have_exact_goldens(self):
        self.assertEqual(len(EXPORT.FOREIGN_NORMALIZED_REPAIRED_SLOTS), 19)
        self.assertEqual(len(EXPORT.UNIVERSAL_RANGE_REPAIRED_SLOTS), 104)
        self.assertEqual(len(EXPORT.NORMALIZED_REPAIRED_SLOTS), 123)
        self.assertTrue(
            all(
                EXPORT.NORMALIZED_CATALOG_GOLDENS[(language, symbol)][index]
                for language, symbol, index in EXPORT.NORMALIZED_REPAIRED_SLOTS
            )
        )
        self.assertEqual(
            EXPORT.normalized_catalog_issues(
                self.schema["sections"],
                self.abi_schema,
            ),
            [],
        )

    def test_missing_polish_teamturn_comma_is_rejected(self):
        source = EXPORT._read("i18n/_PolishText.cpp")
        changed = source.replace('L"Tura cywili",', 'L"Tura cywili"', 1)
        self.assertNotEqual(changed, source)
        diagnostics = "\n".join(
            EXPORT.catalog_source_golden_issues("Polish", changed)
        )
        self.assertIn("Polish/TeamTurnString[4]", diagnostics)
        self.assertIn("found 2", diagnostics)

    def test_missing_italian_middle_slot_is_rejected(self):
        source = EXPORT._read("i18n/_ItalianText.cpp")
        changed = source.replace('\tL"accurate",\t// TODO.Translate\n', "", 1)
        self.assertNotEqual(changed, source)
        diagnostics = "\n".join(
            EXPORT.catalog_source_golden_issues("Italian", changed)
        )
        self.assertIn("Italian/Message[65]", diagnostics)
        self.assertIn("expected 'accurate'", diagnostics)

    def test_empty_repaired_slot_is_rejected(self):
        source = EXPORT._read("i18n/_ItalianText.cpp")
        changed = source.replace('L"accurate",', 'L"",', 1)
        self.assertNotEqual(changed, source)
        diagnostics = "\n".join(
            EXPORT.catalog_source_golden_issues("Italian", changed)
        )
        self.assertIn(
            "Italian/Message[65]: repaired slot must remain nonempty",
            diagnostics,
        )

    def test_normalized_export_range_shrink_is_rejected(self):
        sections = copy.deepcopy(self.schema["sections"])
        tactical = next(
            section for section in sections
            if section.get("symbol") == "TacticalStr"
        )
        tactical["range"]["limit"] = "212"
        diagnostics = "\n".join(
            EXPORT.normalized_range_issues(sections, self.abi_schema)
        )
        self.assertIn(
            "TacticalStr: normalized export range must remain [0,213)",
            diagnostics,
        )

    def test_original_eight_short_sections_pin_all_failure_totals(self):
        short_dimensions = {
            "pPersonnelAssignmentStrings": 83,
            "pLongAssignmentStrings": 82,
            "pDoorTrapStrings": 5,
            "pLandTypeStrings": 41,
            "pMercHeLeaveString": 2,
            "pMercSheLeaveString": 2,
        }
        sections = [
            {
                "source_kind": "legacy",
                "section": symbol,
                "symbol": symbol,
                "range": {"first": "0", "limit": limit},
            }
            for symbol, limit in (
                ("pPersonnelAssignmentStrings", "NUM_ASSIGNMENTS"),
                ("pLongAssignmentStrings", "NUM_ASSIGNMENTS"),
                ("pDoorTrapStrings", "NUM_DOOR_TRAPS"),
                ("pLandTypeStrings", "NUM_TRAVTERRAIN_TYPES"),
                ("pMercHeLeaveString", "5"),
                ("pMercSheLeaveString", "5"),
                ("gzGIOScreenText", "70"),
                ("WeaponType", "MAXITEMS"),
            )
        ]
        wildcard_weapon_languages = {"German", "Dutch", "French", "Italian"}
        dimensions = {}
        for language in EXPORT.ABI.LANGUAGES:
            for quadrant in EXPORT.ABI.QUADRANTS:
                for symbol, entries in short_dimensions.items():
                    dimensions[(language.name, quadrant.name, symbol)] = entries
                dimensions[(language.name, quadrant.name, "gzGIOScreenText")] = (
                    70 if language.name == "Italian" else 69
                )
                dimensions[(language.name, quadrant.name, "WeaponType")] = (
                    9 if language.name in wildcard_weapon_languages else 16001
                )
        summary, failures = EXPORT.summarize_range_safety(sections, dimensions)
        self.assertEqual(
            summary,
            {
                "comparisons": 256,
                "unsafe_sections": 8,
                "unsafe_language_pairs": 59,
                "unsafe_quadrant_failures": 236,
                "potential_oob_reads_per_selected_build": 64127,
            },
        )
        self.assertEqual(len(failures), 236)

    def test_unknown_range_and_symbol_name_mutations_fail_closed(self):
        sections = copy.deepcopy(self.schema["sections"])
        sections[0]["range"]["limit"] = "UNREVIEWED_LIMIT"
        with self.assertRaisesRegex(EXPORT.ExportSchemaError, "cannot resolve exact"):
            EXPORT.summarize_range_safety(sections, {})

        sections = copy.deepcopy(self.schema["sections"])
        sections[0]["symbol"] = "UnknownCatalogTable"
        with self.assertRaisesRegex(EXPORT.ExportSchemaError, "missing legacy export symbol"):
            EXPORT._raw_catalog_dimensions(sections)

    def test_unknown_explicit_raw_dimension_and_comment_only_fallback_fail(self):
        german = EXPORT._read("i18n/_GermanText.cpp")
        changed = german.replace(
            "CHAR16 WeaponType[][30]", "CHAR16 WeaponType[UNKNOWN_BOUND][30]", 1
        )
        self.assertNotEqual(changed, german)
        with self.assertRaisesRegex(EXPORT.ExportSchemaError, "cannot resolve raw"):
            EXPORT._raw_catalog_dimensions(
                self.schema["sections"], {"German": changed}
            )

        changed = german.replace(
            'L"Final Complex", // TODO.Translate',
            'L"Final Complex", // translation marker removed',
            1,
        )
        diagnostics = "\n".join(
            EXPORT.catalog_source_golden_issues("German", changed)
        )
        self.assertIn("pLandTypeStrings[41]", diagnostics)
        self.assertIn("lost TODO.Translate", diagnostics)

    def test_exported_pointer_slots_reject_null_and_identifier_expressions(self):
        german = EXPORT._read("i18n/_GermanText.cpp")
        original = (
            'L"Soll %s seine Ausrüstung hier lassen (%s) oder in (%s), '
            'wenn er verlässt?",'
        )
        section = next(
            section for section in self.schema["sections"]
            if section.get("symbol") == "pMercHeLeaveString"
        )
        for replacement in ("nullptr,", "pMercSheLeaveString[0],"):
            with self.subTest(replacement=replacement):
                changed = german.replace(original, replacement, 1)
                self.assertNotEqual(changed, german)
                with self.assertRaisesRegex(
                    EXPORT.ExportSchemaError,
                    "exported STR16 initializer must be",
                ):
                    EXPORT._raw_catalog_dimensions(
                        [section], {"German": changed}
                    )

        self.assertEqual(
            EXPORT._pointer_initializer_kind(
                'L"adjacent" L" literals"', "fixture"
            ),
            "direct_wide_literal",
        )
        self.assertEqual(
            EXPORT._pointer_initializer_kind(
                "I18N_COMPILED_BUILD_TEXT(Key, L\"release\", L\"debug\")",
                "fixture",
            ),
            "compiled_selector",
        )

    def test_catalog_selector_boundary_rejects_macro_and_include_bypasses(self):
        german = EXPORT._read("i18n/_GermanText.cpp")
        section = next(
            section
            for section in self.schema["sections"]
            if section.get("symbol") == "pMercHeLeaveString"
        )
        include = '#include "CompiledConditionalTextSelectors.inc"'
        mutations = {
            "selector redefinition": german.replace(
                include,
                include
                + "\n#undef I18N_COMPILED_CAMPAIGN_TEXT"
                + "\n#define I18N_COMPILED_CAMPAIGN_TEXT(key, ja2, ja2ub) nullptr",
                1,
            ),
            "inactive include": german.replace(
                include, "#if 0\n" + include + "\n#endif", 1
            ),
            "extra include": german.replace(
                include, include + '\n#include "UnreviewedSelectorOverride.h"', 1
            ),
            "include next": german.replace(
                include, include + '\n#include_next "UnreviewedSelectorOverride.h"', 1
            ),
            "selector header not last": german.replace(
                '\t\t#include "Item Statistics.h"\n'
                '\t\t#include "CompiledConditionalTextSelectors.inc"',
                '\t\t#include "CompiledConditionalTextSelectors.inc"\n'
                '\t\t#include "Item Statistics.h"',
                1,
            ),
        }
        for label, changed in mutations.items():
            with self.subTest(label=label):
                with self.assertRaises(EXPORT.ExportSchemaError):
                    EXPORT._raw_catalog_dimensions(
                        [section], {"German": changed}
                    )

    def test_italian_gio_middle_and_tail_alignment_is_exact(self):
        source = EXPORT._read("i18n/_ItalianText.cpp")
        diagnostics = EXPORT.catalog_source_golden_issues("Italian", source)
        self.assertEqual(diagnostics, [])
        changed = source.replace('\tL"INSANE",\n', "", 1)
        self.assertNotEqual(changed, source)
        diagnostics = "\n".join(
            EXPORT.catalog_source_golden_issues("Italian", changed)
        )
        self.assertIn("gzGIOScreenText", diagnostics)

        german = EXPORT._read("i18n/_GermanText.cpp")
        changed = german.replace(
            '\tL"Unknown", // TODO.Translate\n', "", 1
        )
        self.assertNotEqual(changed, german)
        diagnostics = "\n".join(
            EXPORT.catalog_source_golden_issues("German", changed)
        )
        self.assertIn("pLandTypeStrings[46]", diagnostics)

    def test_imp_gear_has_four_named_assignment_empty_reads(self):
        source = EXPORT.lexical_mask(EXPORT._read("Laptop/IMP Gear.cpp"))
        self.assertEqual(
            source.count("pLongAssignmentStrings[ASSIGNMENT_EMPTY]"), 4
        )
        self.assertNotIn("pLongAssignmentStrings[60]", source)

    def test_exporter_only_cohort_is_exactly_owned_by_typed_text_pack_tables(self):
        inventory = self.schema["exporter_only_tables"]
        self.assertEqual(inventory, [])
        cohort = [
            ("LongAttribute", "LongAttribute", 16),
            ("Training", "Training", 19),
            ("GuardMenu", "GuardMenu", 20),
            ("OtherGuardMenu", "OtherGuardMenu", 21),
            ("ContractExtend", "ContractExtend", 43),
            ("NoiseType", "NoiseType", 46),
            ("Traverse", "Traverse", 94),
            ("MercContractOver", "MercContractOver", 117),
            ("SkiAtm", "SkiAtm", 176),
            ("IMPFinishButton", "ImpFinishButton", 195),
            ("IMPVoices", "ImpVoices", 197),
            ("DepartedMercPortrait", "DepartedMercPortrait", 198),
            ("MiscString", "MiscString", 210),
            ("GioDifConfirm", "GioDifConfirm", 220),
        ]
        sections = {entry["section"]: entry for entry in self.schema["sections"]}
        self.assertEqual(
            [
                (section, sections[section]["key"], sections[section]["ordinal"])
                for section, _key, _ordinal in cohort
            ],
            cohort,
        )
        self.assertTrue(
            all(sections[section]["source_kind"] == "text-pack-table"
                for section, _key, _ordinal in cohort)
        )
        legacy_symbols = {
            "pLongAttributeStrings", "pTrainingStrings", "pGuardMenuStrings",
            "pOtherGuardMenuStrings", "pContractExtendStrings", "pNoiseTypeStr",
            "pTraverseStrings", "pMercContractOverStrings", "SkiAtmText",
            "pIMPFinishButtonText", "pIMPVoicesStrings",
            "pDepartedMercPortraitStrings", "gzMiscString", "zGioDifConfirmText",
        }
        self.assertTrue(legacy_symbols.isdisjoint(self.schema["legacy_symbols"]))
        for language in EXPORT.ABI.LANGUAGES:
            definitions = EXPORT.ABI.parse_definitions(
                EXPORT._read(language.base_source), language.base_source
            )
            self.assertTrue(legacy_symbols.isdisjoint(definitions), language.name)

    def test_manifest_rejects_wildcard_or_growing_debt(self):
        changed = copy.deepcopy(self.schema)
        changed["exported_compatibility_debt"][0]["*"] = "wildcard"
        diagnostics = "\n".join(EXPORT.validate_manifest_contract(changed))
        self.assertIn("missing/unexpected fields", diagnostics)

        changed = copy.deepcopy(self.schema)
        extra = copy.deepcopy(changed["exported_compatibility_debt"][0])
        extra["language"] = "NewLanguage"
        changed["exported_compatibility_debt"].append(extra)
        changed["counts"]["exported_compatibility_debt_pairs"] = len(
            changed["exported_compatibility_debt"]
        )
        diagnostics = "\n".join(EXPORT.validate_manifest_contract(changed))
        self.assertIn("17-pair ceiling", diagnostics)

    def test_manifest_rejects_order_range_and_symbol_bypass(self):
        changed = copy.deepcopy(self.schema)
        changed["sections"][0], changed["sections"][1] = (
            changed["sections"][1],
            changed["sections"][0],
        )
        changed["sections"][0]["ordinal"] = 1
        changed["sections"][1]["ordinal"] = 2
        changed["sections"][2]["range"]["limit"] = "FORGED_LIMIT"
        old_symbol = changed["sections"][3]["symbol"]
        forged_symbol = old_symbol + "Forged"
        changed["sections"][3]["symbol"] = forged_symbol
        changed["legacy_symbols"][forged_symbol] = changed["legacy_symbols"].pop(
            old_symbol
        )
        with mock.patch.object(EXPORT, "make_schema", return_value=self.schema):
            diagnostics = "\n".join(EXPORT.validate_schema(changed))
        self.assertIn("schema.sections[0]", diagnostics)
        self.assertIn("schema.sections[2].range.limit", diagnostics)
        self.assertIn("schema.sections[3].symbol", diagnostics)

    def test_debt_ceiling_allows_an_explicit_reviewed_shrink(self):
        changed = copy.deepcopy(self.schema)
        safe_index = next(
            index
            for index, entry in enumerate(changed["exported_compatibility_debt"])
            if not entry["unsafe_range"]
        )
        del changed["exported_compatibility_debt"][safe_index]
        changed["counts"]["exported_compatibility_debt_pairs"] -= 1
        diagnostics = "\n".join(EXPORT.validate_manifest_contract(changed))
        self.assertNotIn("compatibility debt", diagnostics)

    def test_schema_json_rejects_shadowing_duplicate_keys(self):
        with self.assertRaisesRegex(EXPORT.ABI.SchemaError, "duplicate JSON object key"):
            json.loads(
                '{"sections": [], "sections": []}',
                object_pairs_hook=EXPORT.ABI.unique_json_object,
            )


if __name__ == "__main__":
    unittest.main()
