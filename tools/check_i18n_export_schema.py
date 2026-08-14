#!/usr/bin/env python3
"""Validate the complete GameStrings export contract without building JA2.

The developer exporter still mixes selected legacy catalog globals with the
immutable TextPack.  This tool records that exact ordered boundary and checks
it against ExportStrings.cpp, TextCatalog.h, the compiled-text ABI schema, all
eight catalog shapes, and the startup call chain.

The manifest is deliberately descriptive: it does not bless the fourteen
foreign-catalog ranges that currently exceed their selected source arrays.
Those ranges are explicit non-growing debt and block replacing the textual
catalog inclusion with a linked-global adapter.

Usage:
    python3 tools/check_i18n_export_schema.py
    python3 tools/check_i18n_export_schema.py --write-schema
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Iterable, Mapping, Sequence

import check_i18n_text_schema as ABI


ROOT = Path(__file__).resolve().parent.parent
SCHEMA_PATH = ROOT / "i18n" / "export_text_schema.json"
EXPORT_SOURCE = "i18n/ExportStrings.cpp"
TEXT_CATALOG_HEADER = "i18n/include/TextCatalog.h"

EXPECTED_LOGICAL_SECTIONS = 238
EXPECTED_LEGACY_SECTIONS = 224
EXPECTED_TEXT_PACK_SECTIONS = 14
EXPECTED_TEXT_PACK_ENTRIES = 8
EXPECTED_TEXT_PACK_TABLES = 6
EXPECTED_LEGACY_POINTER_TABLES = 208
EXPECTED_LEGACY_WRITABLE_BUFFERS = 16
EXPECTED_LEGACY_TEXT_H_SYMBOLS = 219
EXPECTED_LEGACY_LOCAL_EXTERN_SYMBOLS = 5
MAX_EXPORTED_COMPATIBILITY_DEBT_PAIRS = 33
MAX_UNSAFE_RANGE_DEBT_PAIRS = 14
EXPECTED_EXPORTER_ONLY_TABLES = 14
EXPECTED_EXPORTER_ONLY_ENTRIES = 85
EXPECTED_TEXTUAL_CATALOG_INCLUDES = [
    "_ChineseText.cpp",
    "_DutchText.cpp",
    "_EnglishText.cpp",
    "_FrenchText.cpp",
    "_GermanText.cpp",
    "_ItalianText.cpp",
    "_PolishText.cpp",
    "_RussianText.cpp",
]

# Only exported compatibility-debt symbols and full exporter-only tables need
# numeric resolution. Unknown expressions fail closed instead of silently
# classifying a new short foreign table as safe.
NAMED_EXPORT_LIMITS: Mapping[str, int | str] = {
    "MAXITEMS": "MAXITEMS",
    "NUM_CONTRACT_EXTEND": 3,
    "NUM_SKI_ATM_BUTTONS": 15,
    "TEXT_NUM_GIO_CFS": 4,
    "TEXT_NUM_GIO_TEXT": 69,
    "TEXT_NUM_LAPTOP_BN_BOOKMARK_TEXT": 16,
    "TEXT_NUM_LAPTOP_BOOKMARKS": 18,
    "TEXT_NUM_PRSNL": 26,
    "TEXT_NUM_STR_MESSAGE": 88,
    "TEXT_NUM_TACTICAL_STR": 213,
}

SOURCE_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"})
CATALOG_SOURCES = frozenset(
    path
    for language in ABI.LANGUAGES
    for path in (language.base_source, language.ja25_source)
)
EXPORTER_ONLY_SCAN_EXCLUSIONS = CATALOG_SOURCES | frozenset(
    {EXPORT_SOURCE, "i18n/include/Text.h"}
)


class ExportSchemaError(RuntimeError):
    """The source export contract could not be represented safely."""


def _read(relative_path: str) -> str:
    path = ROOT / relative_path
    try:
        return path.read_text(encoding="utf-8-sig")
    except OSError as error:
        raise ExportSchemaError(f"cannot read {relative_path}: {error}") from error


def _blank(character: str) -> str:
    return "\n" if character == "\n" else " "


RAW_STRING_START = re.compile(
    r'(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\v\f\r\n]{0,16})\('
)


def _raw_string_end(text: str, index: int) -> int | None:
    if index and (text[index - 1].isalnum() or text[index - 1] == "_"):
        return None
    match = RAW_STRING_START.match(text, index)
    if not match:
        return None
    closing = ")" + match.group("delimiter") + '"'
    closing_index = text.find(closing, match.end())
    if closing_index < 0:
        raise ExportSchemaError("unterminated raw string literal")
    return closing_index + len(closing)


def _blank_span(output: list[str], text: str, start: int, end: int) -> None:
    for index in range(start, end):
        output[index] = _blank(text[index])


def _is_numeric_separator(text: str, index: int) -> bool:
    if not (
        index > 0
        and index + 1 < len(text)
        and text[index - 1] in "0123456789abcdefABCDEF"
        and text[index + 1] in "0123456789abcdefABCDEF"
    ):
        return False
    if text[max(0, index - 2):index] == "u8" and (
        index == 2
        or not (text[index - 3].isalnum() or text[index - 3] == "_")
    ):
        return False
    return True


def comments_blanked(text: str) -> str:
    """Blank C/C++ comments while preserving strings and source positions."""

    output = list(text)
    state = "code"
    index = 0
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""
        if state == "code":
            raw_end = _raw_string_end(text, index)
            if raw_end is not None:
                index = raw_end
                continue
            if char == "/" and next_char == "/":
                output[index] = output[index + 1] = " "
                state = "line-comment"
                index += 2
                continue
            if char == "/" and next_char == "*":
                output[index] = output[index + 1] = " "
                state = "block-comment"
                index += 2
                continue
            if char == '"':
                state = "string"
            elif char == "'" and not _is_numeric_separator(text, index):
                state = "character"
            index += 1
            continue
        if state == "line-comment":
            output[index] = _blank(char)
            if char == "\n":
                state = "code"
            index += 1
            continue
        if state == "block-comment":
            if char == "*" and next_char == "/":
                output[index] = output[index + 1] = " "
                state = "code"
                index += 2
                continue
            output[index] = _blank(char)
            index += 1
            continue
        quote = '"' if state == "string" else "'"
        if char == "\\":
            index += 2
            continue
        if char == quote:
            state = "code"
        index += 1
    if state == "block-comment":
        raise ExportSchemaError("unterminated block comment")
    if state in {"string", "character"}:
        raise ExportSchemaError(f"unterminated {state} literal")
    return "".join(output)


def lexical_mask(text: str) -> str:
    """Blank comments and all literal tokens while preserving source layout."""

    output = list(text)
    index = 0
    while index < len(text):
        raw_end = _raw_string_end(text, index)
        if raw_end is not None:
            _blank_span(output, text, index, raw_end)
            index = raw_end
            continue
        if text.startswith("//", index):
            end = text.find("\n", index + 2)
            end = len(text) if end < 0 else end
            _blank_span(output, text, index, end)
            index = end
            continue
        if text.startswith("/*", index):
            closing = text.find("*/", index + 2)
            if closing < 0:
                raise ExportSchemaError("unterminated block comment")
            end = closing + 2
            _blank_span(output, text, index, end)
            index = end
            continue
        quote = text[index]
        numeric_separator = quote == "'" and _is_numeric_separator(text, index)
        if quote not in {'"', "'"} or numeric_separator:
            index += 1
            continue
        end = index + 1
        while end < len(text):
            if text[end] == "\\":
                end += 2
                continue
            if text[end] == quote:
                end += 1
                break
            end += 1
        else:
            kind = "string" if quote == '"' else "character"
            raise ExportSchemaError(f"unterminated {kind} literal")
        _blank_span(output, text, index, min(end, len(text)))
        index = end
    return "".join(output)


def _matching_delimiter(masked: str, opening: int, opener: str, closer: str) -> int:
    depth = 0
    for index in range(opening, len(masked)):
        if masked[index] == opener:
            depth += 1
        elif masked[index] == closer:
            depth -= 1
            if depth == 0:
                return index
    raise ExportSchemaError(f"unmatched {opener!r}")


def extract_function_body(text: str, qualified_name: str) -> str:
    """Return one function body, ignoring calls and declarations."""

    masked = lexical_mask(text)
    pattern = re.compile(
        rf"\b{re.escape(qualified_name)}\s*\([^;{{}}]*\)\s*"
        rf"(?:const\s*)?(?:noexcept\s*)?(?:override\s*)?(?:final\s*)?\{{"
    )
    matches = list(pattern.finditer(masked))
    if len(matches) != 1:
        raise ExportSchemaError(
            f"expected one definition of {qualified_name}, found {len(matches)}"
        )
    opening = masked.find("{", matches[0].start())
    closing = _matching_delimiter(masked, opening, "{", "}")
    return text[opening + 1:closing]


def _split_arguments(arguments: str) -> list[str]:
    parts = []
    start = 0
    depths = {"(": 0, "[": 0, "{": 0}
    closing = {")": "(", "]": "[", "}": "{"}
    masked = lexical_mask(arguments)
    for index, char in enumerate(masked):
        if char in depths:
            depths[char] += 1
        elif char in closing:
            opener = closing[char]
            depths[opener] -= 1
            if depths[opener] < 0:
                raise ExportSchemaError("unbalanced export-call argument")
        elif char == "," and not any(depths.values()):
            parts.append(arguments[start:index].strip())
            start = index + 1
    if any(depths.values()):
        raise ExportSchemaError("unbalanced nested export-call argument")
    parts.append(arguments[start:].strip())
    return parts


def normalize_expression(expression: str) -> str:
    return re.sub(r"\s+", "", expression)


def parse_text_pack_descriptors(header: str) -> tuple[dict[str, dict], dict[str, dict]]:
    clean = comments_blanked(header)
    active = lexical_mask(header)
    scalar_pattern = re.compile(
        r"\{\s*TextKey::(?P<key>[A-Za-z_]\w*)\s*,\s*"
        r'"(?P<name>[^"]+)"\s*,\s*L"(?P<section>[^"]+)"\s*,\s*'
        r"(?P<fallback>true|false)\s*\}"
    )
    table_pattern = re.compile(
        r"\{\s*TextTableKey::(?P<key>[A-Za-z_]\w*)\s*,\s*"
        r'"(?P<name>[^"]+)"\s*,\s*L"(?P<section>[^"]+)"\s*,\s*'
        r"(?P<offset>\d+)\s*,\s*(?P<entries>\d+)\s*,\s*"
        r"(?P<first>\d+)\s*,\s*(?P<count>\d+)\s*,\s*"
        r"(?P<fallback>true|false)\s*\}"
    )
    scalars = {}
    for match in scalar_pattern.finditer(clean):
        if active[match.start()] != "{":
            continue
        key = match.group("key")
        if key in scalars:
            raise ExportSchemaError(f"duplicate TextKey descriptor {key}")
        scalars[key] = {
            "schema": "TextKey",
            "name": match.group("name"),
            "section": match.group("section"),
            "entry_count": 1,
            "export_first": 0,
            "export_count": 1,
            "english_fallback_allowed": match.group("fallback") == "true",
        }
    tables = {}
    for match in table_pattern.finditer(clean):
        if active[match.start()] != "{":
            continue
        key = match.group("key")
        if key in tables:
            raise ExportSchemaError(f"duplicate TextTableKey descriptor {key}")
        tables[key] = {
            "schema": "TextTableKey",
            "name": match.group("name"),
            "section": match.group("section"),
            "offset": int(match.group("offset")),
            "entry_count": int(match.group("entries")),
            "export_first": int(match.group("first")),
            "export_count": int(match.group("count")),
            "english_fallback_allowed": match.group("fallback") == "true",
        }
    if not scalars or not tables:
        raise ExportSchemaError("TextCatalog.h descriptors were not parsed")
    return scalars, tables


def parse_export_calls(source: str) -> list[dict]:
    body = comments_blanked(extract_function_body(source, "Loc::ExportStrings"))
    active = lexical_mask(body)
    call_pattern = re.compile(
        r"\b(?P<name>ExportSection|ExportTextPackEntry|ExportTextPackTable)\s*\("
    )
    calls = []
    for match in call_pattern.finditer(active):
        opening = active.find("(", match.start())
        closing = _matching_delimiter(active, opening, "(", ")")
        if active[closing + 1:].lstrip()[:1] != ";":
            raise ExportSchemaError(f"{match.group('name')} call is not a statement")
        arguments = _split_arguments(body[opening + 1:closing])
        name = match.group("name")
        if name == "ExportSection":
            if len(arguments) != 5 or arguments[0] != "props":
                raise ExportSchemaError("unsupported legacy ExportSection call shape")
            section_match = re.fullmatch(r'L"([^"\\]*)"', arguments[1])
            symbol_match = re.fullmatch(r"Loc::([A-Za-z_]\w*)", arguments[2])
            if not section_match or not symbol_match:
                raise ExportSchemaError(
                    "legacy export section/symbol is not a direct literal/global"
                )
            calls.append(
                {
                    "source_kind": "legacy",
                    "section": section_match.group(1),
                    "symbol": symbol_match.group(1),
                    "range": {
                        "first": normalize_expression(arguments[3]),
                        "limit": normalize_expression(arguments[4]),
                    },
                }
            )
            continue
        if len(arguments) != 2 or arguments[0] != "props":
            raise ExportSchemaError(f"unsupported {name} call shape")
        key_type = "TextKey" if name == "ExportTextPackEntry" else "TextTableKey"
        key_match = re.fullmatch(rf"i18n::{key_type}::([A-Za-z_]\w*)", arguments[1])
        if not key_match:
            raise ExportSchemaError(f"{name} does not use a direct {key_type}")
        calls.append(
            {
                "source_kind": (
                    "text-pack-entry" if name == "ExportTextPackEntry"
                    else "text-pack-table"
                ),
                "key": key_match.group(1),
            }
        )
    return calls


def textual_catalog_includes(source: str) -> list[str]:
    clean = comments_blanked(source)
    active = lexical_mask(source)
    include_pattern = re.compile(
        r'^[ \t]*#[ \t]*include[ \t]+"(_(?:Chinese|Dutch|English|French|German|'
        r'Italian|Polish|Russian)Text[.]cpp)"',
        re.MULTILINE,
    )
    includes = [
        match.group(1)
        for match in include_pattern.finditer(clean)
        if active[match.start():match.end()].lstrip().startswith("#")
    ]
    if includes != EXPECTED_TEXTUAL_CATALOG_INCLUDES:
        raise ExportSchemaError(
            "selected textual catalog includes changed: "
            f"expected {EXPECTED_TEXTUAL_CATALOG_INCLUDES!r}, got {includes!r}"
        )
    return includes


def _load_abi_schema() -> dict:
    try:
        schema = json.loads(
            (ROOT / "i18n" / "text_abi_schema.json").read_text(encoding="utf-8"),
            object_pairs_hook=ABI.unique_json_object,
        )
    except (OSError, json.JSONDecodeError, ABI.SchemaError) as error:
        raise ExportSchemaError(f"cannot load compiled-text ABI schema: {error}") from error
    issues = ABI.validate_schema(schema)
    if issues:
        raise ExportSchemaError(
            "compiled-text ABI schema must validate before export inventory: "
            + "; ".join(issues)
        )
    return schema


def _legacy_storage(symbol: Mapping) -> dict:
    return {
        "schema_domain": symbol["domain"],
        "declaration": symbol["declaration"],
        "type": symbol["type"],
        "rank": len(symbol["source_dimensions"]),
        "source_dimensions": symbol["source_dimensions"],
        "mutability": symbol["mutability"],
        "conditional_layout": symbol["conditional_layout"],
        "effective_dimensions_by_quadrant": {
            quadrant.name: symbol["quadrants"][quadrant.name]["effective_dimensions"]
            for quadrant in ABI.QUADRANTS
        },
    }


def _resolve_export_limit(expression: str) -> int | str | None:
    if re.fullmatch(r"\d+", expression):
        return int(expression)
    return NAMED_EXPORT_LIMITS.get(expression)


def _debt_inventory(
    sections: Sequence[dict], abi_schema: Mapping
) -> list[dict]:
    legacy_by_symbol = {
        section["symbol"]: section
        for section in sections
        if section["source_kind"] == "legacy"
    }
    records = []
    for language in ABI.LANGUAGES[1:]:
        overlay = abi_schema["catalog_compatibility_debt"][language.name]
        actual = ABI.apply_inventory_overrides(abi_schema["symbols"], overlay)
        for symbol, difference in overlay["symbols"].items():
            section = legacy_by_symbol.get(symbol)
            if section is None:
                continue
            if section["range"]["first"] != "0":
                raise ExportSchemaError(
                    f"debt range analysis requires zero first index for {symbol}"
                )
            export_limit = _resolve_export_limit(section["range"]["limit"])
            if export_limit is None:
                raise ExportSchemaError(
                    f"cannot resolve exported debt range {section['range']['limit']} "
                    f"for {language.name}/{symbol}"
                )
            dimensions = {
                quadrant.name: actual[symbol]["quadrants"][quadrant.name][
                    "effective_dimensions"
                ][0]
                for quadrant in ABI.QUADRANTS
            }
            unique_dimensions = set(dimensions.values())
            if len(unique_dimensions) != 1:
                raise ExportSchemaError(
                    f"{language.name}/{symbol} has quadrant-dependent export bounds"
                )
            actual_entries: int | str = next(iter(unique_dimensions))
            if isinstance(export_limit, int):
                if not re.fullmatch(r"\d+", actual_entries):
                    raise ExportSchemaError(
                        f"cannot compare {language.name}/{symbol} dimension "
                        f"{actual_entries!r} with {export_limit}"
                    )
                actual_entries = int(actual_entries)
                unsafe = actual_entries < export_limit
            else:
                if actual_entries != export_limit:
                    raise ExportSchemaError(
                        f"cannot compare symbolic {language.name}/{symbol} dimension "
                        f"{actual_entries!r} with {export_limit!r}"
                    )
                unsafe = False
            record = {
                "language": language.name,
                "section": section["section"],
                "symbol": symbol,
                "compatibility_fields": sorted(difference),
                "unsafe_range": unsafe,
            }
            if unsafe:
                record.update(
                    {
                        "actual_entries": actual_entries,
                        "export_limit": export_limit,
                        "shortfall": export_limit - actual_entries,
                    }
                )
            records.append(record)
    return records


def _read_source_for_inventory(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig")
    except UnicodeDecodeError:
        return path.read_text(encoding="latin-1")


def production_consumers(symbols: Iterable[str]) -> dict[str, list[str]]:
    """Return conservative production references outside catalog/export owners.

    Historical commented call sites still document ownership and are therefore
    counted.  A table is called exporter-only only when its identifier is
    textually absent from every other production C/C++ source and header.
    """

    wanted = set(symbols)
    consumers = {symbol: [] for symbol in wanted}
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        relative = path.relative_to(ROOT).as_posix()
        if relative.startswith("tests/") or relative in EXPORTER_ONLY_SCAN_EXCLUSIONS:
            continue
        source = _read_source_for_inventory(path)
        identifiers = set(re.findall(r"\b[A-Za-z_]\w*\b", source)) & wanted
        for symbol in identifiers:
            consumers[symbol].append(relative)
    return consumers


def _exporter_only_inventory(
    sections: Sequence[dict], legacy_symbols: Mapping[str, dict]
) -> list[dict]:
    legacy_sections = [
        section for section in sections if section["source_kind"] == "legacy"
    ]
    consumers = production_consumers(section["symbol"] for section in legacy_sections)
    result = []
    for section in legacy_sections:
        symbol = section["symbol"]
        if consumers[symbol]:
            continue
        export_limit = _resolve_export_limit(section["range"]["limit"])
        if not isinstance(export_limit, int) or section["range"]["first"] != "0":
            raise ExportSchemaError(
                f"cannot count exporter-only range for {section['section']}/{symbol}"
            )
        storage = legacy_symbols[symbol]
        if storage["mutability"] != "mutable-pointer-slots-to-const-text":
            raise ExportSchemaError(f"exporter-only writable buffer found: {symbol}")
        result.append(
            {
                "section": section["section"],
                "symbol": symbol,
                "entries_per_language": export_limit,
            }
        )
    return result


def _ordered_tokens(body: str, tokens: Sequence[str], context: str) -> None:
    positions = [body.find(token) for token in tokens]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        raise ExportSchemaError(
            f"{context} lost required order: " + " -> ".join(tokens)
        )


def _load_all_external_text_occurrences() -> dict[str, int]:
    occurrences = {}
    for path in ROOT.rglob("*.cpp"):
        relative = path.relative_to(ROOT).as_posix()
        if relative.startswith("tests/"):
            continue
        try:
            masked = lexical_mask(_read_source_for_inventory(path))
        except ExportSchemaError as error:
            raise ExportSchemaError(f"cannot scan {relative}: {error}") from error
        count = len(re.findall(r"\bLoadAllExternalText\s*\(", masked))
        if count:
            occurrences[relative] = count
    return occurrences


def legacy_startup_contract(sgp_source: str) -> dict:
    """Validate and describe the export/import portion of SGP startup."""

    boundary = comments_blanked(
        extract_function_body(sgp_source, "InitializeLegacyContentBoundary")
    )
    boundary_mask = ABI.lexical_mask(boundary)
    xml_guard = re.search(r"\bif\s*\(\s*g_bUseXML_Strings\s*\)\s*\{", boundary_mask)
    if not xml_guard:
        raise ExportSchemaError("legacy content lost USE_XML_STRINGS export/import guard")
    xml_open = boundary_mask.find("{", xml_guard.start())
    xml_close = _matching_delimiter(boundary_mask, xml_open, "{", "}")
    xml_body = boundary_mask[xml_open + 1:xml_close]
    _ordered_tokens(
        xml_body,
        ["if(s_bExportStrings) Loc::ExportStrings();", "Loc::ImportStrings();"],
        "legacy localization boundary",
    )
    splash_position = boundary_mask.find("InitJA2SplashScreen();")
    if splash_position < xml_close:
        raise ExportSchemaError("splash initialization moved before localization export/import")

    runtime_body = comments_blanked(
        extract_function_body(sgp_source, "GetStandardGamingPlatformRuntime")
    )
    runtime_mask = lexical_mask(runtime_body)
    subsystem_pattern = re.compile(
        r'\bSubsystemDefinition\s*\{\s*"([^"]+)"'
    )
    subsystem_matches = [
        match
        for match in subsystem_pattern.finditer(runtime_body)
        if runtime_mask[match.start():match.start() + len("SubsystemDefinition")]
        == "SubsystemDefinition"
    ]
    subsystem_names = [match.group(1) for match in subsystem_matches]
    try:
        legacy_index = subsystem_names.index("legacy content")
        game_index = subsystem_names.index("game")
    except ValueError as error:
        raise ExportSchemaError("legacy content/game subsystem is missing") from error
    if legacy_index >= game_index:
        raise ExportSchemaError("legacy content no longer starts before game")
    legacy_subsystem_pattern = re.compile(
        r'SubsystemDefinition\s*\{\s*"legacy content"\s*,\s*'
        r"InitializeLegacyContentBoundary"
    )
    if not any(
        runtime_mask[match.start():match.start() + len("SubsystemDefinition")]
        == "SubsystemDefinition"
        for match in legacy_subsystem_pattern.finditer(runtime_body)
    ):
        raise ExportSchemaError("legacy content subsystem lost its export boundary")
    game_subsystem_pattern = re.compile(
        r'SubsystemDefinition\s*\{\s*"game"\s*,\s*InitializeGameBoundary'
    )
    if not any(
        runtime_mask[match.start():match.start() + len("SubsystemDefinition")]
        == "SubsystemDefinition"
        for match in game_subsystem_pattern.finditer(runtime_body)
    ):
        raise ExportSchemaError("game subsystem lost InitializeGameBoundary")
    game_boundary = lexical_mask(
        extract_function_body(sgp_source, "InitializeGameBoundary")
    )
    if "InitializeGame()" not in normalize_expression(game_boundary):
        raise ExportSchemaError("InitializeGameBoundary no longer calls InitializeGame")

    return {
        "legacy_boundary": {
            "source": "sgp/sgp.cpp",
            "function": "InitializeLegacyContentBoundary",
            "outer_guard": "g_bUseXML_Strings",
            "export_guard": "s_bExportStrings",
            "ordered_calls": [
                "Loc::ExportStrings()",
                "Loc::ImportStrings()",
                "InitJA2SplashScreen()",
            ],
        },
        "subsystem_order_constraint": ["legacy content", "game"],
    }


def startup_contract() -> dict:
    contract = legacy_startup_contract(_read("sgp/sgp.cpp"))

    compiled = lexical_mask(
        extract_function_body(
            _read("Ja2/CompiledGameplayBootstrap.cpp"), "loadRulesContent"
        )
    )
    _ordered_tokens(
        normalize_expression(compiled),
        [
            "LoadExternalGameplayData(TABLEDATA_DIRECTORY,false)",
            "LoadAllExternalText()",
        ],
        "compiled rules content load",
    )
    rules = lexical_mask(
        extract_function_body(_read("Ja2/RulesPackage.cpp"), "LegacyRulesPackage::bootstrap")
    )
    _ordered_tokens(
        normalize_expression(rules),
        ["PackageBootstrapPhase::LoadContent", "bootstrapHost_.loadRulesContent(capabilities_)"],
        "rules-package load phase",
    )
    initialize_ja2 = normalize_expression(
        lexical_mask(extract_function_body(_read("Ja2/Init.cpp"), "InitializeJA2"))
    )
    if "advancePackagesTo(PackageBootstrapPhase::LoadContent)" not in initialize_ja2:
        raise ExportSchemaError("InitializeJA2 lost the post-startup LoadContent phase")
    multiplayer = normalize_expression(
        lexical_mask(
            extract_function_body(
                _read("Ja2/MPConnectScreen.cpp"), "DoneFadeOutForExitMPCScreen"
            )
        )
    )
    _ordered_tokens(
        multiplayer,
        [
            "LoadExternalGameplayData(TABLEDATA_DIRECTORY,true)",
            "LoadAllExternalText()",
        ],
        "multiplayer reload",
    )
    occurrences = _load_all_external_text_occurrences()
    expected_occurrences = {
        "Ja2/CompiledGameplayBootstrap.cpp": 1,
        "Ja2/MPConnectScreen.cpp": 1,
        "Utils/Text Utils.cpp": 1,
    }
    if occurrences != expected_occurrences:
        raise ExportSchemaError(
            "LoadAllExternalText call/definition sites changed: "
            f"expected {expected_occurrences!r}, got {occurrences!r}"
        )

    contract["external_load_contract"] = {
        "definition": "Utils/Text Utils.cpp",
        "post_startup_rules_call": {
            "source": "Ja2/CompiledGameplayBootstrap.cpp",
            "function": "loadRulesContent",
            "phase_owner": "LegacyRulesPackage/LoadContent",
        },
        "later_multiplayer_reload": {
            "source": "Ja2/MPConnectScreen.cpp",
            "function": "DoneFadeOutForExitMPCScreen",
        },
        "production_call_sites": 2,
    }
    return contract


def _resolve_sections(calls: Sequence[dict], header: str) -> list[dict]:
    scalars, tables = parse_text_pack_descriptors(header)
    sections = []
    for ordinal, call in enumerate(calls, 1):
        resolved = {"ordinal": ordinal, **call}
        if call["source_kind"] == "legacy":
            sections.append(resolved)
            continue
        descriptors = scalars if call["source_kind"] == "text-pack-entry" else tables
        descriptor = descriptors.get(call["key"])
        if descriptor is None:
            raise ExportSchemaError(
                f"export references unknown {call['source_kind']} key {call['key']}"
            )
        resolved.update(
            {
                "section": descriptor["section"],
                "range": {
                    "first": descriptor["export_first"],
                    "limit": descriptor["export_first"] + descriptor["export_count"],
                },
                "text_pack_descriptor": descriptor,
            }
        )
        sections.append(resolved)
    return sections


def make_schema() -> dict:
    abi_schema = _load_abi_schema()
    export_source = _read(EXPORT_SOURCE)
    sections = _resolve_sections(
        parse_export_calls(export_source), _read(TEXT_CATALOG_HEADER)
    )
    section_names = [section["section"] for section in sections]
    if len(set(section_names)) != len(section_names):
        duplicates = sorted(
            name for name in set(section_names) if section_names.count(name) > 1
        )
        raise ExportSchemaError("duplicate logical export section(s): " + ", ".join(duplicates))
    legacy_sections = [
        section for section in sections if section["source_kind"] == "legacy"
    ]
    pack_entries = [
        section for section in sections if section["source_kind"] == "text-pack-entry"
    ]
    pack_tables = [
        section for section in sections if section["source_kind"] == "text-pack-table"
    ]
    if any(section["range"]["first"] != "0" for section in legacy_sections):
        raise ExportSchemaError("every active legacy export must retain first index zero")
    legacy_names = [section["symbol"] for section in legacy_sections]
    if len(set(legacy_names)) != len(legacy_names):
        raise ExportSchemaError("a legacy catalog symbol is exported more than once")
    legacy_symbols = {}
    for name in legacy_names:
        symbol = abi_schema["symbols"].get(name)
        if symbol is None:
            raise ExportSchemaError(f"legacy export symbol is absent from ABI schema: {name}")
        if symbol["domain"] != "base":
            raise ExportSchemaError(f"legacy export symbol is not in base schema: {name}")
        legacy_symbols[name] = _legacy_storage(symbol)

    debt = _debt_inventory(sections, abi_schema)
    unsafe_debt = [record for record in debt if record["unsafe_range"]]
    if len(debt) > MAX_EXPORTED_COMPATIBILITY_DEBT_PAIRS:
        raise ExportSchemaError(
            "exported compatibility debt grew from the 33-pair ceiling to "
            f"{len(debt)}"
        )
    if len(unsafe_debt) > MAX_UNSAFE_RANGE_DEBT_PAIRS:
        raise ExportSchemaError(
            "unsafe export-range debt grew from the 14-pair ceiling to "
            f"{len(unsafe_debt)}"
        )
    exporter_only = _exporter_only_inventory(sections, legacy_symbols)
    exporter_only_entries = sum(
        record["entries_per_language"] for record in exporter_only
    )

    pointer_tables = sum(
        storage["mutability"] == "mutable-pointer-slots-to-const-text"
        for storage in legacy_symbols.values()
    )
    writable_buffers = sum(
        storage["mutability"] == "writable-character-buffer"
        for storage in legacy_symbols.values()
    )
    text_h_symbols = sum(
        storage["declaration"] == "Text.h" for storage in legacy_symbols.values()
    )
    local_extern_symbols = sum(
        storage["declaration"] == "local-extern"
        for storage in legacy_symbols.values()
    )
    counts = {
        "logical_sections": len(sections),
        "legacy_sections": len(legacy_sections),
        "text_pack_sections": len(pack_entries) + len(pack_tables),
        "text_pack_entries": len(pack_entries),
        "text_pack_tables": len(pack_tables),
        "legacy_pointer_tables": pointer_tables,
        "legacy_writable_buffers": writable_buffers,
        "legacy_text_h_symbols": text_h_symbols,
        "legacy_local_extern_symbols": local_extern_symbols,
        "exported_compatibility_debt_pairs": len(debt),
        "unsafe_range_debt_pairs": len(unsafe_debt),
        "exporter_only_tables": len(exporter_only),
        "exporter_only_entries_per_language": exporter_only_entries,
    }
    expected_structural_counts = {
        "logical_sections": EXPECTED_LOGICAL_SECTIONS,
        "legacy_sections": EXPECTED_LEGACY_SECTIONS,
        "text_pack_sections": EXPECTED_TEXT_PACK_SECTIONS,
        "text_pack_entries": EXPECTED_TEXT_PACK_ENTRIES,
        "text_pack_tables": EXPECTED_TEXT_PACK_TABLES,
        "legacy_pointer_tables": EXPECTED_LEGACY_POINTER_TABLES,
        "legacy_writable_buffers": EXPECTED_LEGACY_WRITABLE_BUFFERS,
        "legacy_text_h_symbols": EXPECTED_LEGACY_TEXT_H_SYMBOLS,
        "legacy_local_extern_symbols": EXPECTED_LEGACY_LOCAL_EXTERN_SYMBOLS,
        "exporter_only_tables": EXPECTED_EXPORTER_ONLY_TABLES,
        "exporter_only_entries_per_language": EXPECTED_EXPORTER_ONLY_ENTRIES,
    }
    changed_counts = {
        key: (value, counts.get(key))
        for key, value in expected_structural_counts.items()
        if counts.get(key) != value
    }
    if changed_counts:
        raise ExportSchemaError(
            "export contract structural counts changed: "
            + ", ".join(
                f"{key} expected {wanted}, got {found}"
                for key, (wanted, found) in changed_counts.items()
            )
        )

    return {
        "schema_version": 1,
        "purpose": "ordered source model of the developer GameStrings exporter",
        "runtime_behavior": "unchanged: selected catalog bodies remain textually included",
        "adapter_status": {
            "state": "blocked",
            "reason": (
                f"{len(unsafe_debt)} foreign-language legacy ranges exceed their selected catalog "
                "arrays; resolve them before a linked-global export adapter"
            ),
        },
        "counts": counts,
        "textual_catalog_includes": textual_catalog_includes(export_source),
        "startup_contract": startup_contract(),
        "sections": sections,
        "legacy_symbols": legacy_symbols,
        "exported_compatibility_debt": debt,
        "exporter_only_tables": exporter_only,
    }


def validate_manifest_contract(schema: Mapping) -> list[str]:
    issues = []
    if schema.get("schema_version") != 1:
        issues.append("schema: unsupported schema_version")
    expected_fields = {
        "schema_version",
        "purpose",
        "runtime_behavior",
        "adapter_status",
        "counts",
        "textual_catalog_includes",
        "startup_contract",
        "sections",
        "legacy_symbols",
        "exported_compatibility_debt",
        "exporter_only_tables",
    }
    unexpected = sorted(set(schema) - expected_fields)
    missing = sorted(expected_fields - set(schema))
    if unexpected:
        issues.append("schema: unexpected field(s): " + ", ".join(unexpected))
    if missing:
        issues.append("schema: missing field(s): " + ", ".join(missing))
    counts = schema.get("counts")
    if not isinstance(counts, dict):
        issues.append("schema: counts are missing")
        counts = {}
    fixed_counts = {
        "logical_sections": EXPECTED_LOGICAL_SECTIONS,
        "legacy_sections": EXPECTED_LEGACY_SECTIONS,
        "text_pack_sections": EXPECTED_TEXT_PACK_SECTIONS,
        "text_pack_entries": EXPECTED_TEXT_PACK_ENTRIES,
        "text_pack_tables": EXPECTED_TEXT_PACK_TABLES,
        "legacy_pointer_tables": EXPECTED_LEGACY_POINTER_TABLES,
        "legacy_writable_buffers": EXPECTED_LEGACY_WRITABLE_BUFFERS,
        "legacy_text_h_symbols": EXPECTED_LEGACY_TEXT_H_SYMBOLS,
        "legacy_local_extern_symbols": EXPECTED_LEGACY_LOCAL_EXTERN_SYMBOLS,
        "exporter_only_tables": EXPECTED_EXPORTER_ONLY_TABLES,
        "exporter_only_entries_per_language": EXPECTED_EXPORTER_ONLY_ENTRIES,
    }
    expected_count_fields = set(fixed_counts) | {
        "exported_compatibility_debt_pairs", "unsafe_range_debt_pairs"
    }
    if set(counts) != expected_count_fields or any(
        counts.get(key) != value for key, value in fixed_counts.items()
    ):
        issues.append("schema: exact export counts changed")
    if not isinstance(counts.get("exported_compatibility_debt_pairs"), int) or not (
        0 <= counts["exported_compatibility_debt_pairs"]
        <= MAX_EXPORTED_COMPATIBILITY_DEBT_PAIRS
    ):
        issues.append("schema: exported compatibility debt count exceeds its ceiling")
    if not isinstance(counts.get("unsafe_range_debt_pairs"), int) or not (
        0 <= counts["unsafe_range_debt_pairs"] <= MAX_UNSAFE_RANGE_DEBT_PAIRS
    ):
        issues.append("schema: unsafe range debt count exceeds its ceiling")
    if schema.get("textual_catalog_includes") != EXPECTED_TEXTUAL_CATALOG_INCLUDES:
        issues.append("schema: the eight selected textual catalog includes changed")
    sections = schema.get("sections")
    if not isinstance(sections, list):
        issues.append("schema: ordered sections are missing")
        sections = []
    ordinals = [section.get("ordinal") for section in sections if isinstance(section, dict)]
    if ordinals != list(range(1, len(sections) + 1)):
        issues.append("schema: section ordinals are not contiguous and ordered")
    section_names = [section.get("section") for section in sections if isinstance(section, dict)]
    if len(section_names) != len(set(section_names)):
        issues.append("schema: section names are not unique")
    kinds = [section.get("source_kind") for section in sections if isinstance(section, dict)]
    known_kinds = {"legacy", "text-pack-entry", "text-pack-table"}
    if any(kind not in known_kinds for kind in kinds):
        issues.append("schema: section has an unknown source kind")
    if len(sections) != counts.get("logical_sections"):
        issues.append("schema: logical section count does not match the ordered manifest")
    legacy_section_count = kinds.count("legacy")
    pack_entry_count = kinds.count("text-pack-entry")
    pack_table_count = kinds.count("text-pack-table")
    if legacy_section_count != counts.get("legacy_sections"):
        issues.append("schema: legacy section count does not match the ordered manifest")
    if pack_entry_count != counts.get("text_pack_entries"):
        issues.append("schema: TextPack entry count does not match the ordered manifest")
    if pack_table_count != counts.get("text_pack_tables"):
        issues.append("schema: TextPack table count does not match the ordered manifest")
    legacy_symbols = schema.get("legacy_symbols")
    if not isinstance(legacy_symbols, dict):
        issues.append("schema: legacy symbol storage inventory is missing")
        legacy_symbols = {}
    referenced_symbols = {
        section.get("symbol")
        for section in sections
        if isinstance(section, dict) and section.get("source_kind") == "legacy"
    }
    if set(legacy_symbols) != referenced_symbols:
        issues.append("schema: legacy storage/schema membership does not match exported symbols")
    if any(
        isinstance(section, dict)
        and section.get("source_kind") == "legacy"
        and section.get("range", {}).get("first") != "0"
        for section in sections
    ):
        issues.append("schema: active legacy export regained a nonzero first index")
    debt = schema.get("exported_compatibility_debt")
    if not isinstance(debt, list):
        issues.append("schema: exported compatibility debt is missing")
        debt = []
    debt_pairs = [
        (entry.get("language"), entry.get("symbol"))
        for entry in debt
        if isinstance(entry, dict)
    ]
    if len(debt_pairs) != len(set(debt_pairs)):
        issues.append("schema: exported compatibility debt contains duplicate pairs")
    if len(debt) != counts.get("exported_compatibility_debt_pairs"):
        issues.append("schema: exported compatibility debt count does not match its inventory")
    if len(debt) > MAX_EXPORTED_COMPATIBILITY_DEBT_PAIRS:
        issues.append("schema: exported compatibility debt exceeds the 33-pair ceiling")
    unsafe = [
        entry for entry in debt
        if isinstance(entry, dict) and entry.get("unsafe_range") is True
    ]
    if len(unsafe) > MAX_UNSAFE_RANGE_DEBT_PAIRS:
        issues.append("schema: unsafe range debt exceeds the 14-pair ceiling")
    if len(unsafe) != counts.get("unsafe_range_debt_pairs"):
        issues.append("schema: unsafe range debt count does not match its inventory")
    safe_debt_fields = {
        "language", "section", "symbol", "compatibility_fields", "unsafe_range"
    }
    unsafe_debt_fields = safe_debt_fields | {
        "actual_entries", "export_limit", "shortfall"
    }
    for entry in debt:
        if not isinstance(entry, dict):
            issues.append("schema: compatibility debt entry is not an object")
            continue
        expected_debt_fields = (
            unsafe_debt_fields
            if entry.get("unsafe_range") is True
            else safe_debt_fields
        )
        if set(entry) != expected_debt_fields:
            issues.append(
                "schema: compatibility debt entry has missing/unexpected fields for "
                f"{entry.get('language')}/{entry.get('symbol')}"
            )
    for entry in unsafe:
        if not all(field in entry for field in ("actual_entries", "export_limit", "shortfall")):
            issues.append(
                "schema: unsafe debt entry lacks exact range evidence for "
                f"{entry.get('language')}/{entry.get('symbol')}"
            )
    exporter_only = schema.get("exporter_only_tables")
    if not isinstance(exporter_only, list):
        issues.append("schema: exporter-only inventory is missing")
        exporter_only = []
    if len(exporter_only) != EXPECTED_EXPORTER_ONLY_TABLES:
        issues.append("schema: exporter-only table count is not exactly 14")
    if len(exporter_only) != counts.get("exporter_only_tables"):
        issues.append("schema: exporter-only count does not match its inventory")
    exporter_only_pairs = [
        (entry.get("section"), entry.get("symbol"))
        for entry in exporter_only
        if isinstance(entry, dict)
    ]
    if len(exporter_only_pairs) != len(set(exporter_only_pairs)):
        issues.append("schema: exporter-only inventory contains duplicate tables")
    for entry in exporter_only:
        if not isinstance(entry, dict) or set(entry) != {
            "section", "symbol", "entries_per_language"
        }:
            issues.append("schema: exporter-only entry has missing/unexpected fields")
    exporter_only_entries = sum(
        entry.get("entries_per_language", 0)
        for entry in exporter_only
        if isinstance(entry, dict)
    )
    if exporter_only_entries != EXPECTED_EXPORTER_ONLY_ENTRIES:
        issues.append("schema: exporter-only entry count is not exactly 85")
    if exporter_only_entries != counts.get("exporter_only_entries_per_language"):
        issues.append("schema: exporter-only entry total does not match its inventory")
    adapter = schema.get("adapter_status")
    if (
        not isinstance(adapter, dict)
        or adapter.get("state") != "blocked"
        or str(counts.get("unsafe_range_debt_pairs"))
        not in str(adapter.get("reason", ""))
    ):
        issues.append("schema: adapter must remain explicitly blocked by unsafe ranges")
    return issues


def _compare(expected: object, actual: object, path: str = "schema") -> list[str]:
    if type(expected) is not type(actual):
        return [f"{path}: expected {type(expected).__name__}, got {type(actual).__name__}"]
    if isinstance(expected, dict):
        issues = []
        for key in sorted(set(expected) - set(actual)):
            issues.append(f"{path}: missing field {key}")
        for key in sorted(set(actual) - set(expected)):
            issues.append(f"{path}: unexpected field {key}")
        for key in sorted(set(expected) & set(actual)):
            issues.extend(_compare(expected[key], actual[key], f"{path}.{key}"))
        return issues
    if isinstance(expected, list):
        if len(expected) != len(actual):
            return [f"{path}: expected {len(expected)} entries, got {len(actual)}"]
        issues = []
        for index, (wanted, found) in enumerate(zip(expected, actual)):
            issues.extend(_compare(wanted, found, f"{path}[{index}]"))
        return issues
    if expected != actual:
        return [f"{path}: expected {expected!r}, got {actual!r}"]
    return []


def validate_schema(schema: Mapping) -> list[str]:
    issues = validate_manifest_contract(schema)
    try:
        actual = make_schema()
    except (ExportSchemaError, ABI.SchemaError) as error:
        issues.append(f"source: {error}")
        return issues
    issues.extend(_compare(schema, actual))
    return issues


def summary(schema: Mapping) -> str:
    counts = schema["counts"]
    return (
        f"{counts['logical_sections']} ordered GameStrings sections: "
        f"{counts['legacy_sections']} legacy + {counts['text_pack_sections']} TextPack; "
        f"{counts['exported_compatibility_debt_pairs']} exported debt pairs, "
        f"{counts['unsafe_range_debt_pairs']} unsafe; "
        f"{counts['exporter_only_tables']} exporter-only tables/"
        f"{counts['exporter_only_entries_per_language']} entries"
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--write-schema",
        action="store_true",
        help="replace the committed manifest with the current validated source model",
    )
    args = parser.parse_args(argv)
    try:
        if args.write_schema:
            schema = make_schema()
            SCHEMA_PATH.write_text(
                json.dumps(schema, indent=2, ensure_ascii=False, sort_keys=False) + "\n",
                encoding="utf-8",
            )
            print(f"Wrote {SCHEMA_PATH.relative_to(ROOT)}: {summary(schema)}")
            return 0
        try:
            schema = json.loads(
                SCHEMA_PATH.read_text(encoding="utf-8"),
                object_pairs_hook=ABI.unique_json_object,
            )
        except FileNotFoundError as error:
            raise ExportSchemaError(
                f"schema missing: {SCHEMA_PATH.relative_to(ROOT)}"
            ) from error
        except (OSError, json.JSONDecodeError, ABI.SchemaError) as error:
            raise ExportSchemaError(f"cannot load export schema: {error}") from error
        issues = validate_schema(schema)
    except (ExportSchemaError, ABI.SchemaError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    print(summary(schema))
    if issues:
        print(f"FAIL: {len(issues)} export schema mismatch(es):", file=sys.stderr)
        for issue in issues:
            print(f"  {issue}", file=sys.stderr)
        return 1
    print("OK: GameStrings order, storage, debt, ownership, and startup contracts match.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
