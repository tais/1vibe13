#!/usr/bin/env python3
"""Focused tests for the build-free compiled-text ABI validator."""

import importlib.util
import json
import sys
import unittest
from pathlib import Path


TOOL_PATH = Path(__file__).with_name("check_i18n_text_schema.py")
SPEC = importlib.util.spec_from_file_location("check_i18n_text_schema", TOOL_PATH)
SCHEMA = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = SCHEMA
SPEC.loader.exec_module(SCHEMA)


def symbol(type_name="STR16", dimensions=None, conditional=None, entries=2):
    dimensions = dimensions or ["2"]
    conditional = conditional or []
    return {
        "domain": "base",
        "type": type_name,
        "source_dimensions": dimensions,
        "mutability": (
            "writable-character-buffer"
            if type_name == "CHAR16"
            else "mutable-pointer-slots-to-const-text"
        ),
        "conditional_layout": conditional,
        "quadrants": {
            quadrant.name: {
                "effective_dimensions": dimensions,
                "initializer_entries": entries,
            }
            for quadrant in SCHEMA.QUADRANTS
        },
    }


class TextSchemaToolTests(unittest.TestCase):
    def test_header_inventory_normalizes_duplicates_functions_and_mutability(self):
        header = """
extern STR16 Names[];
extern STR16 Names[ ];
extern CHAR16 Buffer[2][ 10 ];
extern void LoadAllExternalText(void);
auto FormatMoney(INT32) -> std::wstring;
const STR16 EnumToString(int, const void*);
int StringToEnum(STR16, const void*);
"""
        inventory = SCHEMA.parse_header(header, "fixture.h")
        self.assertEqual(inventory["duplicate_declarations"], {"Names": 2})
        self.assertEqual(
            {entry["name"]: entry["dimensions"] for entry in inventory["symbols"]},
            {"Buffer": ["2", "10"], "Names": ["*"]},
        )
        self.assertEqual(
            inventory["symbols"][0]["mutability"],
            "writable-character-buffer",
        )
        self.assertEqual(
            [entry["name"] for entry in inventory["functions"]],
            ["EnumToString", "FormatMoney", "LoadAllExternalText", "StringToEnum"],
        )

    def test_untracked_catalog_configuration_cannot_escape_the_four_quadrants(self):
        source = """
#ifdef JA2EDITOR
STR16 EditorOnly[] = { L"editor" };
#endif
"""
        with self.assertRaisesRegex(SCHEMA.SchemaError, "untracked conditional macro"):
            SCHEMA.validate_catalog_conditionals(
                source,
                SCHEMA.LANGUAGES[0],
                "fixture.cpp",
            )

    def test_parser_rejects_unrecognized_top_level_catalog_storage(self):
        with self.assertRaisesRegex(SCHEMA.SchemaError, "unparsed top-level extern"):
            SCHEMA.parse_header("extern\nSTR16 SplitDeclaration[];\n", "fixture.h")
        with self.assertRaisesRegex(SCHEMA.SchemaError, "unparsed top-level STR16"):
            SCHEMA.parse_definitions(
                "constinit STR16 Unsupported[] = { L\"text\" };\n",
                "fixture.cpp",
            )

    def test_definition_lexer_hides_raw_strings_and_keeps_digit_separators(self):
        source = r'''auto raw = R"tag(
STR16 Fake[] =
{ nullptr, nullptr };
)tag";
constexpr auto count = 1'000;
STR16 Real[] = { L"real" };
'''
        masked = SCHEMA.lexical_mask(source)
        self.assertNotIn("Fake", masked)
        self.assertIn("1'000", masked)
        self.assertIn("Real", masked)
        self.assertEqual(
            set(SCHEMA.parse_definitions(source, "fixture.cpp")),
            {"Real"},
        )

    def test_campaign_and_build_quadrants_select_exact_initializer_shapes(self):
        source = """
#ifdef ENGLISH
STR16 Labels[] = {
#ifdef JA2UB
  L"ub-one",
  L"ub-two",
#else
  L"base",
#endif
#ifdef JA2BETAVERSION
  L"debug",
#else
  L"release",
#endif
};
#endif
"""
        counts = {}
        for quadrant in SCHEMA.QUADRANTS:
            selected = SCHEMA.preprocess(source, {"ENGLISH", *quadrant.macros})
            counts[quadrant.name] = SCHEMA.parse_definitions(
                selected, "fixture.cpp"
            )["Labels"]["initializer_entries"]
        self.assertEqual(
            counts,
            {
                "ja2-release": 2,
                "ja2-debug": 2,
                "ja2ub-release": 3,
                "ja2ub-debug": 3,
            },
        )
        raw_definition = SCHEMA.parse_definitions(source, "fixture.cpp")["Labels"]
        layout = SCHEMA.conditional_layout(
            source[raw_definition["raw_start"]:raw_definition["raw_end"]]
        )
        self.assertIn("0:if:JA2UB", layout)
        self.assertTrue(any(entry.endswith("if:JA2BETAVERSION") for entry in layout))

    def test_diagnostics_name_every_structural_mismatch_class(self):
        canonical = {
            "alpha": symbol(),
            "missing": symbol(),
        }
        changed = symbol(
            type_name="CHAR16",
            dimensions=["3"],
            conditional=["0:if:JA2UB", "1:endif"],
            entries=3,
        )
        actual = {
            "alpha": changed,
            "extra": symbol(),
        }
        diagnostics = "\n".join(
            SCHEMA._compare_mapping(canonical, actual, "Fixture")
        )
        for expected in (
            "missing symbol missing",
            "extra symbol extra",
            "type mismatch for alpha",
            "source_dimensions mismatch for alpha",
            "mutability mismatch for alpha",
            "conditional mismatch for alpha",
            "dimension mismatch for alpha",
            "entry-count mismatch for alpha",
        ):
            self.assertIn(expected, diagnostics)

    def test_catalog_debt_overlay_is_exact_and_cannot_hide_unknown_symbols(self):
        canonical = {"alpha": symbol(), "beta": symbol()}
        actual = {
            "alpha": symbol(dimensions=["3"], entries=3),
            "extra": symbol(),
        }
        overlay = SCHEMA.inventory_overrides(canonical, actual)
        self.assertEqual(
            SCHEMA.apply_inventory_overrides(canonical, overlay),
            actual,
        )
        overlay["symbols"]["unknown"] = {"type": "STR16"}
        with self.assertRaisesRegex(SCHEMA.SchemaError, "unknown symbol"):
            SCHEMA.apply_inventory_overrides(canonical, overlay)

    def test_catalog_debt_cannot_waive_symbols_types_or_mutability(self):
        debt = {
            "Fixture": {
                "missing_symbols": ["missing"],
                "extra_symbols": {"extra": symbol()},
                "symbols": {
                    "alpha": {
                        "type": "CHAR16",
                        "mutability": "writable-character-buffer",
                    }
                },
            }
        }
        diagnostics = "\n".join(SCHEMA.validate_compatibility_debt(debt))
        self.assertIn("may not permit missing symbols", diagnostics)
        self.assertIn("may not permit extra symbols", diagnostics)
        self.assertIn("may not override mutability, type", diagnostics)

    def test_catalog_debt_rejects_wildcard_fields_and_growth(self):
        symbols = {
            f"gap_{index}": {"source_dimensions": [str(index + 1)]}
            for index in range(SCHEMA.MAX_COMPATIBILITY_DEBT_SYMBOLS + 1)
        }
        debt = {
            "Fixture": {
                "missing_symbols": [],
                "extra_symbols": {},
                "symbols": symbols,
                "*": "no wildcard escape hatch",
            }
        }
        diagnostics = "\n".join(SCHEMA.validate_compatibility_debt(debt))
        self.assertIn("unexpected field(s): *", diagnostics)
        self.assertIn("grew from the 42-symbol ceiling to 43", diagnostics)

    def test_committed_debt_is_41_and_drops_migrated_german_imp_gap(self):
        schema = json.loads(
            SCHEMA.SCHEMA_PATH.read_text(encoding="utf-8"),
            object_pairs_hook=SCHEMA.unique_json_object,
        )
        pairs = {
            (language, symbol)
            for language, overlay in schema["catalog_compatibility_debt"].items()
            for symbol in overlay["symbols"]
        }
        self.assertEqual(len(pairs), 41)
        self.assertNotIn(("German", "pIMPFinishButtonText"), pairs)
        self.assertTrue(
            pairs.isdisjoint(
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
                    ("Italian", "gzGIOScreenText"),
                }
            )
        )

    def test_schema_json_rejects_shadowing_duplicate_keys(self):
        with self.assertRaisesRegex(SCHEMA.SchemaError, "duplicate JSON object key"):
            SCHEMA.json.loads(
                '{"symbols": {}, "symbols": {}}',
                object_pairs_hook=SCHEMA.unique_json_object,
            )

    def test_english_fallback_is_explicit_and_never_a_linker_accident(self):
        self.assertEqual(SCHEMA.FALLBACK_POLICY["language"], "English")
        self.assertEqual(
            SCHEMA.FALLBACK_POLICY["default_symbol_policy"], "required"
        )
        self.assertFalse(SCHEMA.FALLBACK_POLICY["implicit_linker_fallback"])
        self.assertEqual(SCHEMA.FALLBACK_POLICY["optional_symbols"], [])


if __name__ == "__main__":
    unittest.main()
