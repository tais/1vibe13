#!/usr/bin/env python3
"""Validate explicit campaign/build alternatives in compiled text catalogs.

The legacy global tables still publish one selected value, but every catalog
instance that historically had a guard must contain both translations and may
not inspect JA2UB or JA2BETAVERSION. Unconditionally translated instances are
not pulled into this seam.
This source-only gate pins all 58 retired guard groups, their 98 conditioned
table entries (196 literal alternatives), and the exact values selected in the
four legacy quadrants.

Usage:
    python3 tools/check_i18n_conditional_text.py
    python3 tools/check_i18n_conditional_text.py --write-schema
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence

import check_i18n_text_schema as ABI


ROOT = Path(__file__).resolve().parent.parent
SCHEMA_PATH = ROOT / "i18n" / "conditional_text_schema.json"
POLICY_HEADER = ROOT / "i18n" / "include" / "ConditionalTextPolicy.h"
COMPILED_POLICY_HEADER = ROOT / "i18n" / "CompiledConditionalText.h"
COMPILED_SELECTOR_SOURCE = (
    ROOT / "i18n" / "include" / "CompiledConditionalTextSelectors.inc"
)


@dataclass(frozen=True)
class ConditionedValue:
    key: str
    axis: str
    table: str
    index: int
    group: str


@dataclass(frozen=True)
class GuardGroup:
    name: str
    axis: str
    table: str
    keys: tuple[str, ...]
    languages: tuple[str, ...]


ALL_LANGUAGES = tuple(language.name for language in ABI.LANGUAGES)
CONDITIONED_VALUES = (
    ConditionedValue("CountryName", "campaign", "pCountryNames", 0, "CountryNames"),
    ConditionedValue("CountryNoun", "campaign", "pCountryNames", 1, "CountryNames"),
    ConditionedValue("SaveVersionChanged", "build", "zSaveLoadText", 11, "SaveVersionChanged"),
    ConditionedValue("SaveAndGameVersionChanged", "build", "zSaveLoadText", 12, "SaveAndGameVersionChanged"),
    ConditionedValue("GameStyleLabel", "campaign", "gzGIOScreenText", 1, "GameStyle"),
    ConditionedValue("GameStyleFirstChoice", "campaign", "gzGIOScreenText", 2, "GameStyle"),
    ConditionedValue("GameStyleSecondChoice", "campaign", "gzGIOScreenText", 3, "GameStyle"),
    ConditionedValue("TerroristOptionsLabel", "campaign", "gzGIOScreenText", 42, "TerroristOptions"),
    ConditionedValue("TerroristOptionsFirstChoice", "campaign", "gzGIOScreenText", 43, "TerroristOptions"),
    ConditionedValue("TerroristOptionsSecondChoice", "campaign", "gzGIOScreenText", 44, "TerroristOptions"),
    ConditionedValue("MapStartDestinationHelp", "campaign", "pMapScreenJustStartedHelpText", 1, "MapStartDestinationHelp"),
    ConditionedValue("LateCountryName", "campaign", "gzLateLocalizedString", 14, "LateCountryName"),
    ConditionedValue("FilesSenderReport", "campaign", "pFilesSenderList", 0, "FilesSenderReport"),
)
CONDITIONED_VALUE_BY_KEY = {value.key: value for value in CONDITIONED_VALUES}

LEGACY_INDEX_OVERRIDES: Mapping[tuple[str, str], int] = {}

GUARD_GROUPS = (
    GuardGroup("CountryNames", "campaign", "pCountryNames",
               ("CountryName", "CountryNoun"), ALL_LANGUAGES),
    GuardGroup("SaveVersionChanged", "build", "zSaveLoadText",
               ("SaveVersionChanged",), ALL_LANGUAGES),
    GuardGroup("SaveAndGameVersionChanged", "build", "zSaveLoadText",
               ("SaveAndGameVersionChanged",), ALL_LANGUAGES),
    GuardGroup("GameStyle", "campaign", "gzGIOScreenText",
               ("GameStyleLabel", "GameStyleFirstChoice", "GameStyleSecondChoice"),
               ALL_LANGUAGES),
    GuardGroup("TerroristOptions", "campaign", "gzGIOScreenText",
               ("TerroristOptionsLabel", "TerroristOptionsFirstChoice",
                "TerroristOptionsSecondChoice"), ALL_LANGUAGES),
    GuardGroup("MapStartDestinationHelp", "campaign",
               "pMapScreenJustStartedHelpText", ("MapStartDestinationHelp",),
               ALL_LANGUAGES),
    GuardGroup("LateCountryName", "campaign", "gzLateLocalizedString",
               ("LateCountryName",), ALL_LANGUAGES),
    GuardGroup("FilesSenderReport", "campaign", "pFilesSenderList",
               ("FilesSenderReport",), ("Dutch", "French")),
)

RETIRED_GUARD_COUNT = sum(len(group.languages) for group in GUARD_GROUPS)
CONDITIONED_ENTRY_COUNT = sum(
    len(group.keys) * len(group.languages) for group in GUARD_GROUPS
)
LITERAL_ALTERNATIVE_COUNT = CONDITIONED_ENTRY_COUNT * 2
CATALOG_CONFIGURATION_GUARD = re.compile(
    r"^[ \t]*#[ \t]*(?:if|ifdef|ifndef|elif)\b[^\r\n]*"
    r"(?:JA2UB|JA2BETAVERSION)",
    re.MULTILINE,
)
SELECTOR = re.compile(
    r"\bI18N_COMPILED_(CAMPAIGN|BUILD)_TEXT[ \t\r\n]*\("
)
WIDE_LITERAL = re.compile(r'L"(?:\\.|[^"\\])*"')
COMPILED_SELECTOR_INCLUDE = re.compile(
    r'^[ \t]*#[ \t]*include[ \t]*"CompiledConditionalTextSelectors[.]inc"',
    re.MULTILINE,
)
CAMPAIGN_SELECTOR_KEYS = tuple(
    value.key for value in CONDITIONED_VALUES if value.axis == "campaign"
)
BUILD_SELECTOR_KEYS = tuple(
    value.key for value in CONDITIONED_VALUES if value.axis == "build"
)
OWNED_SELECTOR_MACROS = (
    "I18N_DETAIL_SELECT_CAMPAIGN_TEXT",
    "I18N_DETAIL_SELECT_BUILD_TEXT",
    *(f"I18N_DETAIL_CAMPAIGN_{key}" for key in CAMPAIGN_SELECTOR_KEYS),
    *(f"I18N_DETAIL_BUILD_{key}" for key in BUILD_SELECTOR_KEYS),
    "I18N_COMPILED_CAMPAIGN_TEXT",
    "I18N_COMPILED_BUILD_TEXT",
)

EXPECTED_COMPILED_POLICY_DIRECTIVES = (
    "#pragma once",
    "#include <ConditionalTextPolicy.h>",
    "#if defined(JA2UB)",
    "#else",
    "#endif",
    "#if defined(JA2BETAVERSION)",
    "#else",
    "#endif",
    '#include "CompiledConditionalTextSelectors.inc"',
)


class ConditionalTextError(RuntimeError):
    """Conditioned catalog data violated the deliberately narrow schema."""


def _read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig")
    except OSError as error:
        raise ConditionalTextError(f"cannot read {path.relative_to(ROOT)}: {error}") from error


def _initializer_entries(raw_definition: str) -> list[str]:
    masked = ABI.lexical_mask(raw_definition)
    opening = masked.find("{")
    if opening < 0:
        raise ConditionalTextError("conditioned table has no initializer")
    closing = ABI._matching_brace(masked, opening)
    body = raw_definition[opening + 1:closing]
    masked_body = masked[opening + 1:closing]
    depth = {"(": 0, "[": 0, "{": 0}
    matching = {")": "(", "]": "[", "}": "{"}
    entries = []
    start = 0
    for index, character in enumerate(masked_body):
        if character in depth:
            depth[character] += 1
        elif character in matching:
            depth[matching[character]] -= 1
        elif character == "," and not any(depth.values()):
            entries.append(body[start:index])
            start = index + 1
    # A comment after the final, legal trailing comma is not another
    # initializer entry. Check the lexical mask rather than the raw slice so
    # comments cannot shift schema-owned table positions.
    if masked_body[start:].strip():
        entries.append(body[start:])
    return entries


def code_match_count(text: str, pattern: re.Pattern[str]) -> int:
    """Count raw-text matches whose first token is not inside a comment."""

    masked = ABI.lexical_mask(text)
    count = 0
    for match in pattern.finditer(text):
        token = match.start()
        while token < match.end() and text[token].isspace():
            token += 1
        if token < match.end() and masked[token] == text[token]:
            count += 1
    return count


def _selector_source_snapshot(source: str) -> tuple[list[str], str]:
    logical = re.sub(r"\\\r?\n[ \t]*", " ", source)
    masked = ABI.lexical_mask(logical)
    directives = []
    residual = list(masked)
    for match in re.finditer(r"^[ \t]*#[^\r\n]*", masked, re.MULTILINE):
        raw = logical[match.start():match.end()].strip()
        directives.append(re.sub(r"[ \t]+", " ", raw))
        residual[match.start():match.end()] = " " * (match.end() - match.start())
    return directives, "".join(residual)


def _normalized_selector_directives(source: str) -> list[str]:
    return _selector_source_snapshot(source)[0]


def _expected_selector_directives() -> list[str]:
    expected = [f"#undef {name}" for name in OWNED_SELECTOR_MACROS]
    expected.extend(
        [
            "#if defined(JA2UB)",
            "#define I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2_text, ja2ub_text) ja2ub_text",
            "#else",
            "#define I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2_text, ja2ub_text) ja2_text",
            "#endif",
            "#if defined(JA2BETAVERSION)",
            "#define I18N_DETAIL_SELECT_BUILD_TEXT(release_text, beta_text) beta_text",
            "#else",
            "#define I18N_DETAIL_SELECT_BUILD_TEXT(release_text, beta_text) release_text",
            "#endif",
        ]
    )
    expected.extend(
        f"#define I18N_DETAIL_CAMPAIGN_{key}(ja2, ja2ub) "
        "I18N_DETAIL_SELECT_CAMPAIGN_TEXT(ja2, ja2ub)"
        for key in CAMPAIGN_SELECTOR_KEYS
    )
    expected.extend(
        f"#define I18N_DETAIL_BUILD_{key}(release, beta) "
        "I18N_DETAIL_SELECT_BUILD_TEXT(release, beta)"
        for key in BUILD_SELECTOR_KEYS
    )
    expected.extend(
        [
            "#define I18N_COMPILED_CAMPAIGN_TEXT(key, ja2, ja2ub) "
            "I18N_DETAIL_CAMPAIGN_##key(ja2, ja2ub)",
            "#define I18N_COMPILED_BUILD_TEXT(key, release, beta) "
            "I18N_DETAIL_BUILD_##key(release, beta)",
        ]
    )
    return expected


def selector_source_issues(source: str) -> list[str]:
    directives, residual = _selector_source_snapshot(source)
    if directives != _expected_selector_directives():
        return [
            "CompiledConditionalTextSelectors.inc lost its exact re-includable "
            f"{len(OWNED_SELECTOR_MACROS)}-macro directive inventory"
        ]
    if residual.strip():
        return [
            "CompiledConditionalTextSelectors.inc must remain a macro-only "
            "directive seam"
        ]
    return []


def compiled_policy_source_issues(source: str) -> list[str]:
    """Pin the policy header and selector import outside enclosing conditionals."""

    masked = ABI.lexical_mask(source)
    if re.search(r"^[ \t]*(?:%:|\?\?=)", masked, re.MULTILINE) or re.search(
        r"\\[ \t]*(?:\r?\n|$)|\?\?/", masked
    ):
        return [
            "CompiledConditionalText.h uses unsupported preprocessor spelling"
        ]
    directive_matches = list(
        re.finditer(r"^[ \t]*#[^\r\n]*", masked, re.MULTILINE)
    )
    directives = tuple(
        re.sub(r"[ \t]+", " ", source[match.start():match.end()].strip())
        for match in directive_matches
    )
    if directives != EXPECTED_COMPILED_POLICY_DIRECTIVES:
        return [
            "CompiledConditionalText.h lost its exact active policy/selector "
            "directive inventory"
        ]

    policy = re.compile(
        r"\bCompiledConditionalTextPolicy\s*\(\s*\)\s*noexcept\s*"
        r"->[ \t\r\n]*ConditionalTextPolicy\s*\{"
    )
    definitions = list(policy.finditer(masked))
    selector_include = re.compile(
        r'^[ \t]*#[ \t]*include[ \t]*'
        r'"CompiledConditionalTextSelectors[.]inc"[ \t]*$',
        re.MULTILINE,
    )
    includes = list(selector_include.finditer(source))
    if len(definitions) != 1 or len(includes) != 1:
        return [
            "CompiledConditionalText.h must own one policy function and one "
            "selector import"
        ]
    opening = masked.find("{", definitions[0].start(), definitions[0].end())
    try:
        closing = ABI._matching_brace(masked, opening)
    except ABI.SchemaError:
        return ["CompiledConditionalText.h policy function is not balanced"]
    depth = 0
    function_depth = None
    include_depth = None
    for directive in directive_matches:
        if function_depth is None and directive.start() >= definitions[0].start():
            function_depth = depth
        if include_depth is None and directive.start() >= includes[0].start():
            include_depth = depth
        name_match = re.match(r"^[ \t]*#[ \t]*([A-Za-z_]\w*)", masked[
            directive.start():directive.end()
        ])
        if name_match is None:
            return ["CompiledConditionalText.h has an unreadable directive"]
        name = name_match.group(1)
        if name in {"if", "ifdef", "ifndef"}:
            depth += 1
        elif name in {"else", "elif"}:
            if depth == 0:
                return ["CompiledConditionalText.h has an unmatched branch"]
        elif name == "endif":
            depth -= 1
            if depth < 0:
                return ["CompiledConditionalText.h has an unmatched #endif"]
    if function_depth is None:
        function_depth = depth
    if include_depth is None:
        include_depth = depth
    policy_conditionals = directive_matches[2:8]
    if (
        depth != 0
        or function_depth != 0
        or include_depth != 0
        or includes[0].start() <= closing
        or not all(opening < match.start() < closing for match in policy_conditionals)
    ):
        return [
            "CompiledConditionalText.h policy function and selector import "
            "must remain unconditional and top-level"
        ]
    return []


def _split_arguments(raw: str, masked: str) -> list[str]:
    depth = {"(": 0, "[": 0, "{": 0}
    matching = {")": "(", "]": "[", "}": "{"}
    arguments = []
    start = 0
    for index, character in enumerate(masked):
        if character in depth:
            depth[character] += 1
        elif character in matching:
            depth[matching[character]] -= 1
        elif character == "," and not any(depth.values()):
            arguments.append(raw[start:index].strip())
            start = index + 1
    arguments.append(raw[start:].strip())
    return arguments


def canonical_literal(expression: str) -> str:
    masked = ABI.lexical_mask(expression)
    tokens = [
        match for match in WIDE_LITERAL.finditer(expression)
        if masked[match.start():match.start() + 2] == 'L"'
    ]
    if not tokens:
        raise ConditionalTextError("selector alternative is not a wide string literal")
    residual = list(masked)
    for token in tokens:
        residual[token.start():token.end()] = " " * (token.end() - token.start())
    if "".join(residual).strip():
        raise ConditionalTextError(
            "selector alternative contains an expression instead of literal data"
        )
    return " ".join(token.group() for token in tokens)


def parse_selector(entry: str) -> tuple[str, str, str, str]:
    masked = ABI.lexical_mask(entry)
    matches = list(SELECTOR.finditer(masked))
    if len(matches) != 1:
        raise ConditionalTextError(
            f"conditioned table entry has {len(matches)} policy selectors"
        )
    match = matches[0]
    opening = match.end() - 1
    closing = ABI._matching_brace(masked.replace("(", "{").replace(")", "}"), opening)
    # _matching_brace only understands braces. Parentheses inside literals and
    # comments are already blanked, and nested calls were converted as well.
    arguments = _split_arguments(
        entry[opening + 1:closing], masked[opening + 1:closing]
    )
    if len(arguments) != 3:
        raise ConditionalTextError(
            f"conditioned selector has {len(arguments)} arguments instead of 3"
        )
    if not re.fullmatch(r"[A-Za-z_]\w*", arguments[0]):
        raise ConditionalTextError("conditioned selector key is not an identifier")
    outside = masked[:match.start()] + masked[closing + 1:]
    if outside.strip():
        raise ConditionalTextError("conditioned table entry mixes selector and other data")
    return (
        match.group(1).lower(),
        arguments[0],
        canonical_literal(arguments[1]),
        canonical_literal(arguments[2]),
    )


def expected_keys(language_name: str) -> set[str]:
    return {
        key
        for group in GUARD_GROUPS
        if language_name in group.languages
        for key in group.keys
    }


def collect_catalog(language: ABI.Language) -> dict[str, dict[str, str]]:
    source_path = ROOT / language.base_source
    text = _read(source_path)
    relative = source_path.relative_to(ROOT)
    guards = CATALOG_CONFIGURATION_GUARD.findall(ABI.lexical_mask(text))
    if guards:
        raise ConditionalTextError(
            f"{relative}: catalog regained JA2UB/JA2BETAVERSION directives"
        )
    if code_match_count(text, COMPILED_SELECTOR_INCLUDE) != 1:
        raise ConditionalTextError(
            f"{relative}: compiled selector seam include is missing or duplicated"
        )

    definitions = ABI.parse_definitions(text, str(relative))
    values = {}
    for key in sorted(expected_keys(language.name)):
        descriptor = CONDITIONED_VALUE_BY_KEY[key]
        definition = definitions.get(descriptor.table)
        if definition is None:
            raise ConditionalTextError(
                f"{relative}: conditioned table {descriptor.table} is missing"
            )
        raw = text[definition["raw_start"]:definition["raw_end"]]
        entries = _initializer_entries(raw)
        legacy_index = LEGACY_INDEX_OVERRIDES.get(
            (language.name, key), descriptor.index
        )
        if legacy_index >= len(entries):
            raise ConditionalTextError(
                f"{relative}:{descriptor.table}: conditioned index "
                f"{legacy_index} is outside {len(entries)} entries"
            )
        axis, found_key, stable, conditioned = parse_selector(entries[legacy_index])
        if found_key != key:
            raise ConditionalTextError(
                f"{relative}:{descriptor.table}[{legacy_index}]: expected key "
                f"{key}, got {found_key}"
            )
        if axis != descriptor.axis:
            raise ConditionalTextError(
                f"{relative}:{descriptor.table}[{legacy_index}]: {key} uses "
                f"{axis} policy instead of {descriptor.axis}"
            )
        variants = (
            {"ja2": stable, "ja2ub": conditioned}
            if axis == "campaign"
            else {"release": stable, "beta": conditioned}
        )
        values[key] = variants

    found_selectors = len(SELECTOR.findall(ABI.lexical_mask(text)))
    if found_selectors != len(values):
        raise ConditionalTextError(
            f"{relative}: found {found_selectors} policy selectors but schema owns "
            f"{len(values)}"
        )
    return values


def _group_schema() -> list[dict]:
    return [
        {
            "name": group.name,
            "axis": group.axis,
            "legacy_table": group.table,
            "keys": list(group.keys),
            "languages": list(group.languages),
        }
        for group in GUARD_GROUPS
    ]


def _key_schema() -> list[dict]:
    return [
        {
            "key": value.key,
            "axis": value.axis,
            "legacy_table": value.table,
            "legacy_index": value.index,
            "legacy_guard_group": value.group,
            "languages": list(next(
                group.languages for group in GUARD_GROUPS
                if value.key in group.keys
            )),
        }
        for value in CONDITIONED_VALUES
    ]


def validate_policy_headers() -> list[str]:
    issues = []
    public = _read(POLICY_HEADER)
    public_masked = ABI.lexical_mask(public)
    descriptor = re.compile(
        r'\{ConditionalTextKey::(?P<key>\w+),\s*"(?P<name>\w+)",\s*'
        r'ConditionalTextAxis::(?P<axis>\w+),\s*"(?P<table>\w+)",\s*'
        r'(?P<index>\d+),\s*"(?P<group>\w+)"\}',
        re.DOTALL,
    )
    found = [
        ConditionedValue(
            match.group("key"), match.group("axis"), match.group("table"),
            int(match.group("index")), match.group("group")
        )
        for match in descriptor.finditer(public)
        if public_masked[match.start()] == "{"
        if match.group("key") == match.group("name")
    ]
    if tuple(found) != CONDITIONED_VALUES:
        issues.append("ConditionalTextPolicy.h key descriptors differ from source schema")

    compiled = _read(COMPILED_POLICY_HEADER)
    compiled_masked = ABI.lexical_mask(compiled)
    issues.extend(compiled_policy_source_issues(compiled))
    selector_include = re.compile(
        r'^[ \t]*#[ \t]*include[ \t]*'
        r'"CompiledConditionalTextSelectors[.]inc"',
        re.MULTILINE,
    )
    if code_match_count(compiled, selector_include) != 1:
        issues.append(
            "CompiledConditionalText.h must import the selector seam exactly once"
        )
    selectors = _read(COMPILED_SELECTOR_SOURCE)
    selectors_masked = ABI.lexical_mask(selectors)
    issues.extend(selector_source_issues(selectors))
    for value in CONDITIONED_VALUES:
        prefix = "CAMPAIGN" if value.axis == "campaign" else "BUILD"
        marker = f"#define I18N_DETAIL_{prefix}_{value.key}("
        if selectors_masked.count(marker) != 1:
            issues.append(
                "CompiledConditionalTextSelectors.inc lost exact "
                f"{value.axis} alias for {value.key}"
            )
    for required in (
        "CampaignTextVariant::ja2",
        "CampaignTextVariant::ja2ub",
        "BuildTextVariant::release",
        "BuildTextVariant::beta",
    ):
        if required not in compiled_masked:
            issues.append(f"CompiledConditionalText.h lost {required}")
    return issues


def make_schema() -> dict:
    catalogs = {
        language.name: collect_catalog(language) for language in ABI.LANGUAGES
    }
    return {
        "schema_version": 1,
        "purpose": "exact values behind retired compiled catalog guards",
        "variant_axes": {
            "campaign": ["ja2", "ja2ub"],
            "build": ["release", "beta"],
        },
        "retired_catalog_guard_count": RETIRED_GUARD_COUNT,
        "conditioned_entry_count": CONDITIONED_ENTRY_COUNT,
        "literal_alternative_count": LITERAL_ALTERNATIVE_COUNT,
        "legacy_index_overrides": [
            {"language": language, "key": key, "legacy_index": index}
            for (language, key), index in LEGACY_INDEX_OVERRIDES.items()
        ],
        "guard_groups": _group_schema(),
        "conditioned_keys": _key_schema(),
        "catalog_values": catalogs,
    }


def validate_schema(schema: Mapping) -> list[str]:
    issues = validate_policy_headers()
    if schema.get("schema_version") != 1:
        issues.append("conditional schema: unsupported schema_version")
    expected_metadata = make_schema()
    for field in (
        "purpose",
        "variant_axes",
        "retired_catalog_guard_count",
        "conditioned_entry_count",
        "literal_alternative_count",
        "legacy_index_overrides",
        "guard_groups",
        "conditioned_keys",
    ):
        if schema.get(field) != expected_metadata[field]:
            issues.append(f"conditional schema: {field} changed or is incomplete")

    expected_values = schema.get("catalog_values")
    if not isinstance(expected_values, dict):
        issues.append("conditional schema: catalog_values is missing")
    elif expected_values != expected_metadata["catalog_values"]:
        expected_catalogs = expected_metadata["catalog_values"]
        for language in sorted(set(expected_values) | set(expected_catalogs)):
            if expected_values.get(language) != expected_catalogs.get(language):
                issues.append(
                    f"{language}: conditioned catalog values differ from exact schema"
                )
    return issues


def select_catalog_values(
    catalog: Mapping[str, Mapping[str, str]], campaign: str, build: str
) -> dict[str, str]:
    if campaign not in {"ja2", "ja2ub"}:
        raise ConditionalTextError(f"unknown campaign text variant: {campaign}")
    if build not in {"release", "beta"}:
        raise ConditionalTextError(f"unknown build text variant: {build}")
    selected = {}
    for key, variants in catalog.items():
        descriptor = CONDITIONED_VALUE_BY_KEY[key]
        selected[key] = variants[campaign if descriptor.axis == "campaign" else build]
    return selected


def summary() -> str:
    return (
        f"{len(ABI.LANGUAGES)} catalogs; {RETIRED_GUARD_COUNT} retired guard groups, "
        f"{CONDITIONED_ENTRY_COUNT} conditioned entries / "
        f"{LITERAL_ALTERNATIVE_COUNT} exact alternatives, 4 policy quadrants"
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--write-schema",
        action="store_true",
        help="replace the committed exact-value schema from current selectors",
    )
    args = parser.parse_args(argv)
    try:
        if args.write_schema:
            schema = make_schema()
            header_issues = validate_policy_headers()
            if header_issues:
                raise ConditionalTextError("; ".join(header_issues))
            SCHEMA_PATH.write_text(
                json.dumps(schema, indent=2, ensure_ascii=False) + "\n",
                encoding="utf-8",
            )
            print(f"Wrote {SCHEMA_PATH.relative_to(ROOT)}: {summary()}")
            return 0
        try:
            schema = json.loads(
                SCHEMA_PATH.read_text(encoding="utf-8"),
                object_pairs_hook=ABI.unique_json_object,
            )
        except FileNotFoundError as error:
            raise ConditionalTextError(
                f"schema missing: {SCHEMA_PATH.relative_to(ROOT)}"
            ) from error
        except (OSError, json.JSONDecodeError) as error:
            raise ConditionalTextError(f"cannot load conditional schema: {error}") from error
        issues = validate_schema(schema)
    except (ConditionalTextError, ABI.SchemaError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    print(summary())
    if issues:
        print(f"FAIL: {len(issues)} conditional text mismatch(es):", file=sys.stderr)
        for issue in issues:
            print(f"  {issue}", file=sys.stderr)
        return 1
    print("OK: all catalog alternatives match the explicit value and policy schema.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
