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
bool Loc::ExportStrings()
{
    // ExportSection(props, L"Commented", Loc::Old, 0, 9);
    const char* ordinary =
        "ExportSection(props, L\"StringLiteral\", Loc::Literal, 0, 8);";
    const char* raw = R"schema(
        ExportSection(props, L"RawLiteral", Loc::RawLiteral, 0, 7);
    )schema";
    ExportSection(props, L"First", Loc::FirstTable, 0, LIMIT - 1);
    ExportTextPackEntry(props, i18n::TextKey::Title);
    /* ExportTextPackTable(props, i18n::TextTableKey::Old); */
    ExportTextPackTable(props, i18n::TextTableKey::Times);
    return true;
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

    def test_textual_catalog_include_parser_rejects_raw_string_bypass(self):
        fake_chinese = r'''R"schema(
#include "_ChineseText.cpp"
)schema";
'''
        real_includes = "\n".join(
            f'#include "{name}"'
            for name in EXPORT.EXPECTED_TEXTUAL_CATALOG_INCLUDES[1:]
        )
        with self.assertRaisesRegex(
            EXPORT.ExportSchemaError, "selected textual catalog includes changed"
        ):
            EXPORT.textual_catalog_includes(fake_chinese + real_includes)

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
        with self.assertRaisesRegex(EXPORT.ExportSchemaError, "lost required order"):
            EXPORT.legacy_startup_contract(reversed_source)
        raw_literal_bypass = reversed_source.replace(
            "    if(g_bUseXML_Strings)",
            r'''    const char* ignored = R"schema(
        if(s_bExportStrings) Loc::ExportStrings();
        Loc::ImportStrings();
    )schema";
    if(g_bUseXML_Strings)''',
        )
        with self.assertRaisesRegex(EXPORT.ExportSchemaError, "lost required order"):
            EXPORT.legacy_startup_contract(raw_literal_bypass)

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

    def test_committed_manifest_pins_all_238_sections_and_storage_membership(self):
        self.assertEqual(EXPORT.validate_manifest_contract(self.schema), [])
        self.assertEqual(len(self.schema["sections"]), 238)
        self.assertEqual(
            self.schema["textual_catalog_includes"],
            EXPORT.EXPECTED_TEXTUAL_CATALOG_INCLUDES,
        )
        self.assertEqual(
            [entry["source_kind"] for entry in self.schema["sections"]].count("legacy"),
            224,
        )
        self.assertEqual(len(self.schema["legacy_symbols"]), 224)
        self.assertEqual(self.schema["counts"]["legacy_text_h_symbols"], 219)
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

    def test_exact_33_debt_pairs_and_14_unsafe_ranges_are_reviewable(self):
        debt = self.schema["exported_compatibility_debt"]
        self.assertEqual(len(debt), 33)
        unsafe = {
            (entry["language"], entry["symbol"])
            for entry in debt
            if entry["unsafe_range"]
        }
        self.assertEqual(
            unsafe,
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
            },
        )
        self.assertTrue(all(entry["shortfall"] > 0 for entry in debt if entry["unsafe_range"]))

    def test_exact_14_exporter_only_tables_total_85_entries(self):
        inventory = self.schema["exporter_only_tables"]
        self.assertEqual(
            {entry["symbol"] for entry in inventory},
            {
                "pLongAttributeStrings",
                "pTrainingStrings",
                "pGuardMenuStrings",
                "pOtherGuardMenuStrings",
                "pContractExtendStrings",
                "pNoiseTypeStr",
                "pTraverseStrings",
                "pMercContractOverStrings",
                "SkiAtmText",
                "pIMPFinishButtonText",
                "pIMPVoicesStrings",
                "pDepartedMercPortraitStrings",
                "gzMiscString",
                "zGioDifConfirmText",
            },
        )
        self.assertEqual(sum(entry["entries_per_language"] for entry in inventory), 85)

    def test_manifest_rejects_wildcard_or_growing_debt(self):
        changed = copy.deepcopy(self.schema)
        changed["exported_compatibility_debt"][0]["*"] = "wildcard"
        diagnostics = "\n".join(EXPORT.validate_manifest_contract(changed))
        self.assertIn("missing/unexpected fields", diagnostics)

        changed = copy.deepcopy(self.schema)
        extra = copy.deepcopy(changed["exported_compatibility_debt"][0])
        extra["language"] = "NewLanguage"
        changed["exported_compatibility_debt"].append(extra)
        changed["counts"]["exported_compatibility_debt_pairs"] = 34
        diagnostics = "\n".join(EXPORT.validate_manifest_contract(changed))
        self.assertIn("33-pair ceiling", diagnostics)

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
