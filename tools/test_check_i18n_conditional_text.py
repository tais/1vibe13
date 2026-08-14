#!/usr/bin/env python3
"""Focused tests for the conditioned catalog value/schema gate."""

import importlib.util
import json
import sys
import unittest
from pathlib import Path
from unittest import mock


TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
TOOL_PATH = TOOLS / "check_i18n_conditional_text.py"
SPEC = importlib.util.spec_from_file_location("check_i18n_conditional_text", TOOL_PATH)
POLICY = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = POLICY
SPEC.loader.exec_module(POLICY)


class ConditionalTextToolTests(unittest.TestCase):
    def test_initializer_comments_cannot_invent_table_entries(self):
        definition = '''
STR16 table[] = {
    L"one",
    // A trailing comment after the legal comma is not an entry.
};
'''
        self.assertEqual(
            POLICY._initializer_entries(definition),
            ['\n    L"one"'],
        )

    def test_commented_policy_include_does_not_satisfy_catalog_gate(self):
        actual = '#include "CompiledConditionalTextSelectors.inc"\n'
        commented = '/*\n#include "CompiledConditionalTextSelectors.inc"\n*/\n'
        self.assertEqual(
            POLICY.code_match_count(actual, POLICY.COMPILED_SELECTOR_INCLUDE), 1
        )
        self.assertEqual(
            POLICY.code_match_count(commented, POLICY.COMPILED_SELECTOR_INCLUDE), 0
        )
        self.assertEqual(
            POLICY.code_match_count(actual + actual, POLICY.COMPILED_SELECTOR_INCLUDE),
            2,
        )

    def test_selector_seam_pins_complete_reincludeable_macro_inventory(self):
        source = POLICY._read(POLICY.COMPILED_SELECTOR_SOURCE)
        self.assertEqual(POLICY.selector_source_issues(source), [])
        self.assertEqual(len(POLICY.OWNED_SELECTOR_MACROS), 17)
        mutations = {
            "missing undef": source.replace(
                "#undef I18N_COMPILED_BUILD_TEXT\n", "", 1
            ),
            "null selector": source.replace(
                "I18N_DETAIL_BUILD_##key(release, beta)", "nullptr", 1
            ),
            "extra directive": source + "\n#define UNREVIEWED_SELECTOR nullptr\n",
            "C++ declaration": (
                source + "\ninline constexpr int UnreviewedSelectorSeamToken = 1;\n"
            ),
            "pragma operator": source + '\n_Pragma("unreviewed selector seam")\n',
            "bare macro invocation": source + "\nUNREVIEWED_SELECTOR_SEAM()\n",
        }
        for label, changed in mutations.items():
            with self.subTest(label=label):
                self.assertTrue(POLICY.selector_source_issues(changed))

    def test_compiled_policy_and_selector_import_remain_active(self):
        source = POLICY._read(POLICY.COMPILED_POLICY_HEADER)
        self.assertEqual(POLICY.compiled_policy_source_issues(source), [])
        include = '#include "CompiledConditionalTextSelectors.inc"'
        mutations = {
            "selector include inactive": source.replace(
                include, "#if 0\n" + include + "\n#endif", 1
            ),
            "whole header inactive": "#if 0\n" + source + "#endif\n",
        }
        for label, changed in mutations.items():
            with self.subTest(label=label):
                self.assertTrue(POLICY.compiled_policy_source_issues(changed))
                original_read = POLICY._read

                def changed_read(path):
                    if path == POLICY.COMPILED_POLICY_HEADER:
                        return changed
                    return original_read(path)

                with mock.patch.object(POLICY, "_read", side_effect=changed_read):
                    self.assertTrue(POLICY.validate_policy_headers())

    def test_all_58_legacy_guards_have_explicit_groups_and_positions(self):
        self.assertEqual(POLICY.RETIRED_GUARD_COUNT, 58)
        self.assertEqual(POLICY.CONDITIONED_ENTRY_COUNT, 98)
        self.assertEqual(POLICY.LITERAL_ALTERNATIVE_COUNT, 196)
        self.assertEqual(len(POLICY.CONDITIONED_VALUES), 13)
        self.assertEqual(len(POLICY.CONDITIONED_VALUE_BY_KEY), 13)
        self.assertEqual(POLICY.LEGACY_INDEX_OVERRIDES, {})
        self.assertEqual(
            sum(len(group.languages) for group in POLICY.GUARD_GROUPS), 58
        )

    def test_selector_requires_one_key_and_two_literal_alternatives(self):
        self.assertEqual(
            POLICY.parse_selector(
                'I18N_COMPILED_CAMPAIGN_TEXT(CountryName, L"Arulco", L"Tracona")'
            ),
            ("campaign", "CountryName", 'L"Arulco"', 'L"Tracona"'),
        )
        with self.assertRaisesRegex(POLICY.ConditionalTextError, "instead of 3"):
            POLICY.parse_selector(
                'I18N_COMPILED_BUILD_TEXT(SaveVersionChanged, L"release")'
            )
        with self.assertRaisesRegex(POLICY.ConditionalTextError, "wide string literal"):
            POLICY.parse_selector(
                "I18N_COMPILED_CAMPAIGN_TEXT(CountryName, fallback, L\"Tracona\")"
            )
        with self.assertRaisesRegex(POLICY.ConditionalTextError, "expression"):
            POLICY.parse_selector(
                "I18N_COMPILED_CAMPAIGN_TEXT(CountryName, (L\"Arulco\"), L\"Tracona\")"
            )

    def test_every_catalog_has_only_schema_owned_selectors(self):
        schema = POLICY.make_schema()
        self.assertEqual(set(schema["catalog_values"]), set(POLICY.ALL_LANGUAGES))
        self.assertEqual(
            sum(len(catalog) for catalog in schema["catalog_values"].values()),
            98,
        )
        for language, catalog in schema["catalog_values"].items():
            self.assertEqual(set(catalog), POLICY.expected_keys(language))
        self.assertEqual(
            {
                language
                for language, catalog in schema["catalog_values"].items()
                if "FilesSenderReport" in catalog
            },
            {"Dutch", "French"},
        )

    def test_all_four_quadrants_choose_the_declared_axis_only(self):
        fixture = {
            "CountryName": {"ja2": "base", "ja2ub": "ub"},
            "SaveVersionChanged": {"release": "stable", "beta": "diagnostic"},
        }
        expected = {
            ("ja2", "release"): {"CountryName": "base", "SaveVersionChanged": "stable"},
            ("ja2", "beta"): {"CountryName": "base", "SaveVersionChanged": "diagnostic"},
            ("ja2ub", "release"): {"CountryName": "ub", "SaveVersionChanged": "stable"},
            ("ja2ub", "beta"): {"CountryName": "ub", "SaveVersionChanged": "diagnostic"},
        }
        for quadrant, values in expected.items():
            self.assertEqual(
                POLICY.select_catalog_values(fixture, *quadrant), values
            )

    def test_committed_values_resolve_exactly_in_every_catalog_quadrant(self):
        schema = json.loads(POLICY.SCHEMA_PATH.read_text(encoding="utf-8"))
        for catalog in schema["catalog_values"].values():
            selected = {
                (campaign, build): POLICY.select_catalog_values(
                    catalog, campaign, build
                )
                for campaign in ("ja2", "ja2ub")
                for build in ("release", "beta")
            }
            for key, alternatives in catalog.items():
                axis = POLICY.CONDITIONED_VALUE_BY_KEY[key].axis
                for (campaign, build), values in selected.items():
                    variant = campaign if axis == "campaign" else build
                    self.assertEqual(values[key], alternatives[variant])

    def test_exact_value_drift_names_the_language(self):
        schema = POLICY.make_schema()
        changed = json.loads(json.dumps(schema))
        changed["catalog_values"]["English"]["CountryName"]["ja2"] = 'L"Elsewhere"'
        diagnostics = "\n".join(POLICY.validate_schema(changed))
        self.assertIn("English: conditioned catalog values differ", diagnostics)

    def test_duplicate_schema_keys_cannot_shadow_values(self):
        with self.assertRaisesRegex(POLICY.ABI.SchemaError, "duplicate JSON object key"):
            json.loads(
                '{"catalog_values": {}, "catalog_values": {}}',
                object_pairs_hook=POLICY.ABI.unique_json_object,
            )

    def test_catalog_configuration_guards_are_detected(self):
        self.assertIsNotNone(
            POLICY.CATALOG_CONFIGURATION_GUARD.search("#ifdef JA2UB\n")
        )
        self.assertIsNotNone(
            POLICY.CATALOG_CONFIGURATION_GUARD.search(
                "#if defined(JA2BETAVERSION)\n"
            )
        )


if __name__ == "__main__":
    unittest.main()
