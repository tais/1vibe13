#!/usr/bin/env python3
"""Validate the legacy compiled-text ABI without building the game.

The eight translation units still publish one process-global ABI selected by
language. Campaign/build values now enter through a separate explicit policy,
but this tool continues to prove that publication has identical storage shape
in all four JA2/JA2UB x release/debug quadrants. It inventories the ABI from
the English compatibility sources and validates structure, not wording.

Usage:
    python3 tools/check_i18n_text_schema.py
    python3 tools/check_i18n_text_schema.py --write-schema
"""

from __future__ import annotations

import argparse
import copy
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Mapping, Sequence


ROOT = Path(__file__).resolve().parent.parent
SCHEMA_PATH = ROOT / "i18n" / "text_abi_schema.json"


@dataclass(frozen=True)
class Language:
    name: str
    macro: str
    base_source: str
    ja25_source: str
    ja25_header: str | None


@dataclass(frozen=True)
class Quadrant:
    name: str
    macros: frozenset[str]


LANGUAGES = (
    Language("English", "ENGLISH", "i18n/_EnglishText.cpp", "i18n/_Ja25EnglishText.cpp", "i18n/include/_Ja25EnglishText.h"),
    Language("German", "GERMAN", "i18n/_GermanText.cpp", "i18n/_Ja25GermanText.cpp", "i18n/include/_Ja25GermanText.h"),
    Language("Russian", "RUSSIAN", "i18n/_RussianText.cpp", "i18n/_Ja25RussianText.cpp", "i18n/include/_Ja25RussianText.h"),
    Language("Dutch", "DUTCH", "i18n/_DutchText.cpp", "i18n/_Ja25DutchText.cpp", "i18n/include/_Ja25DutchText.h"),
    Language("Polish", "POLISH", "i18n/_PolishText.cpp", "i18n/_Ja25PolishText.cpp", "i18n/include/_Ja25PolishText.h"),
    Language("French", "FRENCH", "i18n/_FrenchText.cpp", "i18n/_Ja25FrenchText.cpp", "i18n/include/_Ja25FrenchText.h"),
    Language("Italian", "ITALIAN", "i18n/_ItalianText.cpp", "i18n/_Ja25ItalianText.cpp", "i18n/include/_Ja25ItalianText.h"),
    # Chinese has no compatibility declaration header.  Its implementation is
    # still checked against the same canonical JA25 schema as the other seven.
    Language("Chinese", "CHINESE", "i18n/_ChineseText.cpp", "i18n/_Ja25ChineseText.cpp", None),
)

QUADRANTS = (
    Quadrant("ja2-release", frozenset()),
    Quadrant("ja2-debug", frozenset({"JA2BETAVERSION"})),
    Quadrant("ja2ub-release", frozenset({"JA2UB"})),
    Quadrant("ja2ub-debug", frozenset({"JA2UB", "JA2BETAVERSION"})),
)

FALLBACK_POLICY = {
    "language": "English",
    "legacy_behavior": "selected compiled catalog must be complete",
    "runtime_behavior": "resolve only explicitly optional keys by name after pack validation",
    "default_symbol_policy": "required",
    "optional_symbols": [],
    "implicit_linker_fallback": False,
}

# This is a ceiling, not a target.  Regeneration may remove grandfathered
# language/symbol mismatches, but it must not turn a newly introduced mismatch
# into accepted debt merely because the schema was rewritten alongside it.
MAX_COMPATIBILITY_DEBT_SYMBOLS = 57
# Campaign/build choices moved behind CompiledConditionalText.h. A catalog may
# now condition only its own legacy language body; configuration guards are no
# longer part of the ABI shape and are rejected here.
CATALOG_CONFIGURATION_MACROS = frozenset()

DATA_DECLARATION = re.compile(
    r"^[ \t]*extern[ \t]+(?P<const>const[ \t]+)?"
    r"(?P<type>STR16|CHAR16)[ \t]+(?P<name>[A-Za-z_]\w*)[ \t]*"
    r"(?P<dimensions>(?:\[[^\]\r\n]*\][ \t]*)+)[ \t]*;",
    re.MULTILINE,
)
SOURCE_DEFINITION = re.compile(
    r"^[ \t]*(?P<const>const[ \t]+)?(?P<type>STR16|CHAR16)[ \t]+"
    r"(?P<name>[A-Za-z_]\w*)[ \t]*"
    r"(?P<dimensions>(?:\[[^\]\r\n]*\][ \t]*)+)[ \t]*"
    r"(?P<terminator>=|;)",
    re.MULTILINE,
)
FUNCTION_DECLARATION = re.compile(
    r"^[ \t]*(?:extern[ \t]+)?"
    r"(?P<return>(?:const[ \t]+)?(?:auto|void|STR8|STR16|int))[ \t]+"
    r"(?P<name>[A-Za-z_]\w*)[ \t]*\((?P<arguments>[^;()]*)\)[ \t]*"
    r"(?:->[ \t]*(?P<trailing>[^;]+))?;",
    re.MULTILINE,
)
DIRECTIVE = re.compile(r"^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")
DIMENSION = re.compile(r"\[([^\]]*)\]")
TEXT_STORAGE_TYPE = re.compile(r"\b(?:STR16|CHAR16)\b")
FUNCTION_NAME_AFTER_TYPE = re.compile(r"[ \t\r\n]+[A-Za-z_]\w*[ \t\r\n]*\(")


class SchemaError(RuntimeError):
    """The source could not be represented by the intentionally small parser."""


def unique_json_object(pairs: list[tuple[str, object]]) -> dict:
    """Reject duplicate keys so a visible debt entry cannot be shadowed."""

    result = {}
    for key, value in pairs:
        if key in result:
            raise SchemaError(f"duplicate JSON object key: {key}")
        result[key] = value
    return result


def _blank(character: str) -> str:
    return "\n" if character == "\n" else " "


def lexical_mask(text: str) -> str:
    """Blank comments and literal bodies while retaining layout and delimiters."""

    output = list(text)
    index = 0
    state = "code"
    while index < len(text):
        char = text[index]
        next_char = text[index + 1] if index + 1 < len(text) else ""
        if state == "code":
            if char == "/" and next_char == "/":
                output[index] = output[index + 1] = " "
                index += 2
                state = "line-comment"
                continue
            if char == "/" and next_char == "*":
                output[index] = output[index + 1] = " "
                index += 2
                state = "block-comment"
                continue
            if char == '"':
                state = "string"
            elif char == "'":
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
                index += 2
                state = "code"
                continue
            output[index] = _blank(char)
            index += 1
            continue
        quote = '"' if state == "string" else "'"
        if char == "\\":
            output[index] = " "
            if index + 1 < len(text):
                output[index + 1] = _blank(text[index + 1])
            index += 2
            continue
        if char == quote:
            state = "code"
        elif char != "\n":
            output[index] = " "
        index += 1

    if state == "block-comment":
        raise SchemaError("unterminated block comment")
    if state in {"string", "character"}:
        raise SchemaError(f"unterminated {state} literal")
    return "".join(output)


def normalize_dimensions(dimensions: str) -> list[str]:
    return [re.sub(r"\s+", "", value) or "*" for value in DIMENSION.findall(dimensions)]


def mutability(type_name: str, const_slot: bool) -> str:
    if const_slot:
        return "immutable-storage"
    if type_name == "CHAR16":
        return "writable-character-buffer"
    return "mutable-pointer-slots-to-const-text"


def _depth_at_positions(masked: str) -> list[int]:
    depths = [0] * (len(masked) + 1)
    depth = 0
    for index, char in enumerate(masked):
        depths[index] = depth
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth < 0:
                raise SchemaError("unmatched closing brace")
    depths[len(masked)] = depth
    if depth:
        raise SchemaError("unmatched opening brace")
    return depths


def _matching_brace(masked: str, opening: int) -> int:
    depth = 0
    for index in range(opening, len(masked)):
        if masked[index] == "{":
            depth += 1
        elif masked[index] == "}":
            depth -= 1
            if depth == 0:
                return index
    raise SchemaError("initializer has no closing brace")


def initializer_entries(masked_body: str) -> int:
    delimiter_depth = {"(": 0, "[": 0, "{": 0}
    closing = {")": "(", "]": "[", "}": "{"}
    entries = 0
    has_token = False
    for char in masked_body:
        if char in delimiter_depth:
            delimiter_depth[char] += 1
            has_token = True
        elif char in closing:
            opener = closing[char]
            delimiter_depth[opener] -= 1
            if delimiter_depth[opener] < 0:
                raise SchemaError("unbalanced delimiter in initializer")
            has_token = True
        elif char == "," and not any(delimiter_depth.values()):
            if has_token:
                entries += 1
            has_token = False
        elif not char.isspace() and char != "#":
            has_token = True
    if any(delimiter_depth.values()):
        raise SchemaError("unbalanced nested initializer")
    return entries + int(has_token)


def _normalize_condition(directive: str, expression: str) -> str:
    expression = re.sub(r"//.*", "", expression).strip()
    if directive == "ifdef":
        return f"if:{expression}"
    if directive == "ifndef":
        return f"if:!{expression}"
    if directive in {"if", "elif"}:
        expression = re.sub(r"defined\s*\(\s*([A-Za-z_]\w*)\s*\)", r"\1", expression)
        expression = re.sub(r"\s+", "", expression)
        return f"{directive}:{expression}"
    return directive


def conditional_layout(raw_definition: str) -> list[str]:
    """Record relevant directive order and structural position in an initializer."""

    masked = lexical_mask(raw_definition)
    brace = masked.find("{")
    if brace < 0:
        return []
    depth = 0
    commas = 0
    profile = []
    offset = 0
    for raw_line, masked_line in zip(raw_definition.splitlines(True), masked.splitlines(True)):
        directive_match = DIRECTIVE.match(masked_line)
        if directive_match and brace < offset + len(masked_line):
            directive = directive_match.group(1)
            normalized = _normalize_condition(directive, directive_match.group(2))
            profile.append(f"{commas}:{normalized}")
        for char in masked_line:
            if offset < brace:
                offset += 1
                continue
            if char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
            elif char == "," and depth == 1:
                commas += 1
            offset += 1
    return profile


def parse_definitions(text: str, source_name: str) -> dict[str, dict]:
    masked = lexical_mask(text)
    depths = _depth_at_positions(masked)
    definitions: dict[str, dict] = {}
    for match in SOURCE_DEFINITION.finditer(masked):
        if depths[match.start()] != 0:
            continue
        name = match.group("name")
        if name in definitions:
            raise SchemaError(f"{source_name}: duplicate definition of {name}")
        dimensions = normalize_dimensions(match.group("dimensions"))
        initializer_count = 0
        end = match.end()
        if match.group("terminator") == "=":
            opening = end
            while opening < len(masked) and masked[opening].isspace():
                opening += 1
            if opening >= len(masked) or masked[opening] != "{":
                line = text.count("\n", 0, match.start()) + 1
                raise SchemaError(f"{source_name}:{line}: array initializer is not braced")
            closing = _matching_brace(masked, opening)
            initializer_count = initializer_entries(masked[opening + 1:closing])
            end = closing + 1
            while end < len(masked) and masked[end].isspace():
                end += 1
            if end >= len(masked) or masked[end] != ";":
                line = text.count("\n", 0, match.start()) + 1
                raise SchemaError(f"{source_name}:{line}: initializer is not terminated")
            end += 1
        if dimensions[0] == "*":
            if not initializer_count:
                line = text.count("\n", 0, match.start()) + 1
                raise SchemaError(f"{source_name}:{line}: unsized array has no entries")
            effective_dimensions = [str(initializer_count), *dimensions[1:]]
        else:
            effective_dimensions = dimensions
        line = text.count("\n", 0, match.start()) + 1
        definitions[name] = {
            "type": match.group("type"),
            "source_dimensions": dimensions,
            "effective_dimensions": effective_dimensions,
            "initializer_entries": initializer_count,
            "mutability": mutability(match.group("type"), bool(match.group("const"))),
            "line": line,
            "raw_start": match.start(),
            "raw_end": end,
        }
    definition_spans = [
        (definition["raw_start"], definition["raw_end"])
        for definition in definitions.values()
    ]
    for type_match in TEXT_STORAGE_TYPE.finditer(masked):
        if depths[type_match.start()] != 0:
            continue
        if any(start <= type_match.start() < end for start, end in definition_spans):
            continue
        # A helper function may legitimately return a text pointer. Everything
        # else at file scope is storage that the deliberately small parser must
        # either understand or reject instead of silently omitting.
        if FUNCTION_NAME_AFTER_TYPE.match(masked, type_match.end()):
            continue
        line = text.count("\n", 0, type_match.start()) + 1
        raise SchemaError(
            f"{source_name}:{line}: unparsed top-level {type_match.group()} storage"
        )
    return definitions


def _evaluate_expression(expression: str, defined: set[str]) -> bool:
    expression = re.sub(
        r"defined\s*\(\s*([A-Za-z_]\w*)\s*\)",
        lambda match: "True" if match.group(1) in defined else "False",
        expression,
    )
    expression = re.sub(
        r"\b[A-Za-z_]\w*\b",
        lambda match: match.group(0) if match.group(0) in {"True", "False"}
        else ("True" if match.group(0) in defined else "False"),
        expression,
    )
    expression = expression.replace("&&", " and ").replace("||", " or ")
    expression = re.sub(r"!(?!=)", " not ", expression)
    if re.search(r"[^\s()TrueFalsandornot]", expression):
        raise SchemaError(f"unsupported preprocessor expression: {expression!r}")
    try:
        return bool(eval(expression, {"__builtins__": {}}, {}))
    except (SyntaxError, NameError) as error:
        raise SchemaError(f"invalid preprocessor expression: {expression!r}") from error


def preprocess(text: str, defined: Iterable[str]) -> str:
    """Evaluate the small conditional subset used by compiled text sources."""

    known = set(defined)
    masked = lexical_mask(text)
    frames: list[dict[str, bool]] = []
    output: list[str] = []
    for raw_line, masked_line in zip(text.splitlines(True), masked.splitlines(True)):
        match = DIRECTIVE.match(masked_line)
        if not match:
            active = all(frame["active"] for frame in frames)
            output.append(raw_line if active else _blank_line(raw_line))
            continue
        directive, expression = match.group(1), match.group(2).strip()
        if directive in {"if", "ifdef", "ifndef"}:
            parent_active = all(frame["active"] for frame in frames)
            if directive == "ifdef":
                result = expression.split()[0] in known
            elif directive == "ifndef":
                result = expression.split()[0] not in known
            else:
                result = _evaluate_expression(expression, known)
            frames.append({"parent": parent_active, "active": parent_active and result, "taken": result})
        elif directive == "elif":
            if not frames:
                raise SchemaError("#elif without #if")
            frame = frames[-1]
            result = not frame["taken"] and _evaluate_expression(expression, known)
            frame["active"] = frame["parent"] and result
            frame["taken"] = frame["taken"] or result
        elif directive == "else":
            if not frames:
                raise SchemaError("#else without #if")
            frame = frames[-1]
            result = not frame["taken"]
            frame["active"] = frame["parent"] and result
            frame["taken"] = True
        else:
            if not frames:
                raise SchemaError("#endif without #if")
            frames.pop()
        output.append(_blank_line(raw_line))
    if frames:
        raise SchemaError("unterminated preprocessor conditional")
    return "".join(output)


def _blank_line(line: str) -> str:
    return "".join(_blank(character) for character in line)


def validate_catalog_conditionals(
    text: str,
    language: Language,
    source_name: str,
) -> None:
    """Reject catalog variants outside the four combinations that we check."""

    allowed = {language.macro, *CATALOG_CONFIGURATION_MACROS}
    masked = lexical_mask(text)
    for line, masked_line in enumerate(masked.splitlines(), start=1):
        match = DIRECTIVE.match(masked_line)
        if not match or match.group(1) in {"else", "endif"}:
            continue
        identifiers = set(re.findall(r"\b[A-Za-z_]\w*\b", match.group(2)))
        identifiers.discard("defined")
        unknown = sorted(identifiers - allowed)
        if unknown:
            raise SchemaError(
                f"{source_name}:{line}: untracked conditional macro(s): "
                + ", ".join(unknown)
            )


def parse_header(text: str, source_name: str) -> dict:
    masked = lexical_mask(text)
    depths = _depth_at_positions(masked)
    data = []
    functions = []
    recognized_spans = []
    for match in DATA_DECLARATION.finditer(masked):
        if depths[match.start()] != 0:
            continue
        recognized_spans.append(match.span())
        data.append({
            "name": match.group("name"),
            "type": match.group("type"),
            "dimensions": normalize_dimensions(match.group("dimensions")),
            "mutability": mutability(match.group("type"), bool(match.group("const"))),
        })
    for match in FUNCTION_DECLARATION.finditer(masked):
        if depths[match.start()] != 0:
            continue
        recognized_spans.append(match.span())
        return_type = match.group("trailing") or match.group("return")
        arguments = re.sub(r"\s+", "", match.group("arguments")) or "void"
        functions.append({
            "name": match.group("name"),
            "return_type": re.sub(r"\s+", "", return_type),
            "arguments": arguments,
        })
    for extern_match in re.finditer(r"\bextern\b", masked):
        if depths[extern_match.start()] != 0:
            continue
        if any(start <= extern_match.start() < end for start, end in recognized_spans):
            continue
        line = text.count("\n", 0, extern_match.start()) + 1
        raise SchemaError(f"{source_name}:{line}: unparsed top-level extern declaration")
    counts = Counter(entry["name"] for entry in data)
    unique = {}
    for entry in data:
        previous = unique.setdefault(entry["name"], entry)
        if previous != entry:
            raise SchemaError(f"{source_name}: conflicting declarations of {entry['name']}")
    return {
        "symbols": [unique[name] for name in sorted(unique)],
        "duplicate_declarations": {
            name: count for name, count in sorted(counts.items()) if count > 1
        },
        "functions": sorted(functions, key=lambda entry: entry["name"]),
    }


def _read(relative_path: str) -> str:
    path = ROOT / relative_path
    try:
        return path.read_text(encoding="utf-8-sig")
    except OSError as error:
        raise SchemaError(f"cannot read {relative_path}: {error}") from error


def _combine_definitions(language: Language, macros: Iterable[str]) -> dict[str, dict]:
    combined = {}
    definitions_by_domain = {}
    selected = {language.macro, *macros}
    for domain, relative_path in (("base", language.base_source), ("ja25", language.ja25_source)):
        definitions = parse_definitions(preprocess(_read(relative_path), selected), relative_path)
        definitions_by_domain[domain] = definitions
        overlap = set(combined) & set(definitions)
        if overlap:
            raise SchemaError(
                f"{language.name}: symbols occur in base and JA25 catalogs: "
                + ", ".join(sorted(overlap))
            )
        for name, definition in definitions.items():
            combined[name] = {**definition, "domain": domain}
    return combined


def build_language_inventory(language: Language) -> dict[str, dict]:
    declaration_symbols = {}
    for relative_path in (
        "i18n/include/Text.h",
        "i18n/include/_Ja25EnglishText.h",
    ):
        header = parse_header(_read(relative_path), relative_path)
        declaration_symbols.update(
            {symbol["name"]: symbol for symbol in header["symbols"]}
        )
    raw_by_domain = {}
    for domain, relative_path in (("base", language.base_source), ("ja25", language.ja25_source)):
        text = _read(relative_path)
        validate_catalog_conditionals(text, language, relative_path)
        raw_definitions = parse_definitions(text, relative_path)
        for definition in raw_definitions.values():
            definition["conditional_layout"] = conditional_layout(
                text[definition["raw_start"]:definition["raw_end"]]
            )
        raw_by_domain[domain] = raw_definitions

    quadrant_definitions = {
        quadrant.name: _combine_definitions(language, quadrant.macros)
        for quadrant in QUADRANTS
    }
    canonical_names = set(quadrant_definitions[QUADRANTS[0].name])
    for quadrant, definitions in quadrant_definitions.items():
        if set(definitions) != canonical_names:
            raise SchemaError(
                f"{language.name}/{quadrant}: conditional definitions change the symbol set"
            )

    inventory = {}
    for name in sorted(canonical_names):
        baseline = quadrant_definitions[QUADRANTS[0].name][name]
        raw = raw_by_domain[baseline["domain"]][name]
        declaration = declaration_symbols.get(name)
        resolved_quadrants = {}
        for quadrant in QUADRANTS:
            definition = quadrant_definitions[quadrant.name][name]
            for field in ("domain", "type", "source_dimensions", "mutability"):
                if definition[field] != baseline[field]:
                    raise SchemaError(
                        f"{language.name}/{quadrant.name}: {field} changes "
                        f"across quadrants for {name}"
                    )
            effective_dimensions = list(definition["effective_dimensions"])
            if declaration:
                if len(declaration["dimensions"]) != len(effective_dimensions):
                    raise SchemaError(
                        f"{language.name}: declaration/definition rank mismatch for {name}"
                    )
                for index, declared_dimension in enumerate(declaration["dimensions"]):
                    if declared_dimension != "*":
                        effective_dimensions[index] = declared_dimension
            resolved_quadrants[quadrant.name] = {
                "effective_dimensions": effective_dimensions,
                "initializer_entries": definition["initializer_entries"],
            }
        inventory[name] = {
            "domain": baseline["domain"],
            "type": baseline["type"],
            "source_dimensions": raw["source_dimensions"],
            "mutability": baseline["mutability"],
            "conditional_layout": raw["conditional_layout"],
            "quadrants": resolved_quadrants,
        }
    return inventory


def make_canonical_schema() -> dict:
    base_header = parse_header(_read("i18n/include/Text.h"), "i18n/include/Text.h")
    ja25_header = parse_header(
        _read("i18n/include/_Ja25EnglishText.h"),
        "i18n/include/_Ja25EnglishText.h",
    )
    english = build_language_inventory(LANGUAGES[0])
    declared = {
        symbol["name"]: {**symbol, "header": "Text.h"}
        for symbol in base_header["symbols"]
    }
    for symbol in ja25_header["symbols"]:
        if symbol["name"] in declared:
            raise SchemaError(f"duplicate base/JA25 declaration: {symbol['name']}")
        declared[symbol["name"]] = {**symbol, "header": "_Ja25EnglishText.h"}

    for name, symbol in english.items():
        declaration = declared.get(name)
        symbol["declaration"] = declaration["header"] if declaration else "local-extern"
        if not declaration:
            continue
        if declaration["type"] != symbol["type"]:
            raise SchemaError(f"English declaration/definition type mismatch for {name}")
        if declaration["mutability"] != symbol["mutability"]:
            raise SchemaError(f"English declaration/definition mutability mismatch for {name}")
        source_dimensions = symbol["source_dimensions"]
        if len(declaration["dimensions"]) != len(source_dimensions):
            raise SchemaError(f"English declaration/definition rank mismatch for {name}")
        for index, (declared_dimension, source_dimension) in enumerate(
            zip(declaration["dimensions"], source_dimensions)
        ):
            if declared_dimension == "*":
                continue
            if source_dimension != "*" and declared_dimension != source_dimension:
                raise SchemaError(f"English declaration/definition dimension mismatch for {name}")
            if source_dimension == "*":
                effective = {
                    tuple(variant["effective_dimensions"])
                    for variant in symbol["quadrants"].values()
                }
                if any(dimensions[index] != declared_dimension for dimensions in effective):
                    raise SchemaError(
                        f"English declaration/inferred dimension mismatch for {name}"
                    )

    unresolved = [
        {"name": name, "type": value["type"], "dimensions": value["dimensions"]}
        for name, value in sorted(declared.items())
        if name not in english
    ]
    return {
        "schema_version": 1,
        "purpose": "behavior-free inventory of the compiled legacy Text ABI",
        "canonical_language": "English",
        "fallback_policy": FALLBACK_POLICY,
        "languages": [
            {
                "name": language.name,
                "macro": language.macro,
                "base_source": language.base_source,
                "ja25_source": language.ja25_source,
                "ja25_header": language.ja25_header,
            }
            for language in LANGUAGES
        ],
        "quadrants": [
            {"name": quadrant.name, "macros": sorted(quadrant.macros)}
            for quadrant in QUADRANTS
        ],
        "header_inventory": {
            "base_data_declarations": len(base_header["symbols"]),
            "ja25_data_declarations": len(ja25_header["symbols"]),
            "duplicate_data_declarations": base_header["duplicate_declarations"],
            "functions": base_header["functions"],
            "unresolved_data_declarations": unresolved,
        },
        "symbols": english,
    }


def inventory_overrides(canonical: Mapping[str, dict], actual: Mapping[str, dict]) -> dict:
    """Return the smallest structural overlay needed to describe one catalog."""

    overlay: dict = {
        "missing_symbols": sorted(set(canonical) - set(actual)),
        "extra_symbols": {
            name: actual[name] for name in sorted(set(actual) - set(canonical))
        },
        "symbols": {},
    }
    for name in sorted(set(canonical) & set(actual)):
        wanted = canonical[name]
        found = actual[name]
        difference = {}
        for field in ("domain", "type", "source_dimensions", "mutability", "conditional_layout"):
            if wanted[field] != found[field]:
                difference[field] = found[field]
        quadrant_difference = {}
        for quadrant in QUADRANTS:
            quadrant_name = quadrant.name
            if wanted["quadrants"][quadrant_name] != found["quadrants"][quadrant_name]:
                quadrant_difference[quadrant_name] = found["quadrants"][quadrant_name]
        if quadrant_difference:
            difference["quadrants"] = quadrant_difference
        if difference:
            overlay["symbols"][name] = difference
    return overlay


def apply_inventory_overrides(canonical: Mapping[str, dict], overlay: Mapping) -> dict:
    expected = copy.deepcopy(canonical)
    missing = overlay.get("missing_symbols", [])
    extra = overlay.get("extra_symbols", {})
    symbols = overlay.get("symbols", {})
    if not isinstance(missing, list) or not isinstance(extra, dict) or not isinstance(symbols, dict):
        raise SchemaError("catalog compatibility debt has an invalid shape")
    for name in missing:
        if name not in expected:
            raise SchemaError(f"catalog compatibility debt removes unknown symbol {name}")
        del expected[name]
    for name, symbol in extra.items():
        if name in expected or not isinstance(symbol, dict):
            raise SchemaError(f"catalog compatibility debt adds invalid symbol {name}")
        expected[name] = copy.deepcopy(symbol)
    for name, difference in symbols.items():
        if name not in expected or not isinstance(difference, dict):
            raise SchemaError(f"catalog compatibility debt changes unknown symbol {name}")
        for field, value in difference.items():
            if field == "quadrants":
                if not isinstance(value, dict):
                    raise SchemaError(f"invalid quadrant debt for {name}")
                for quadrant_name, quadrant_value in value.items():
                    if quadrant_name not in expected[name]["quadrants"]:
                        raise SchemaError(f"invalid debt quadrant {quadrant_name} for {name}")
                    expected[name]["quadrants"][quadrant_name] = copy.deepcopy(quadrant_value)
            elif field in {"domain", "type", "source_dimensions", "mutability", "conditional_layout"}:
                expected[name][field] = copy.deepcopy(value)
            else:
                raise SchemaError(f"invalid compatibility-debt field {field} for {name}")
    return expected


def validate_compatibility_debt(debt: Mapping) -> list[str]:
    """Keep grandfathered debt structural; symbols and storage stay mandatory."""

    issues = []
    debt_symbol_count = 0
    expected_overlay_fields = {"missing_symbols", "extra_symbols", "symbols"}
    permitted_symbol_fields = {"source_dimensions", "conditional_layout", "quadrants"}
    for language, overlay in debt.items():
        if not isinstance(overlay, dict):
            issues.append(f"{language}: catalog compatibility debt is not an object")
            continue
        unexpected_overlay_fields = sorted(set(overlay) - expected_overlay_fields)
        if unexpected_overlay_fields:
            issues.append(
                f"{language}: compatibility debt has unexpected field(s): "
                + ", ".join(unexpected_overlay_fields)
            )
        missing = overlay.get("missing_symbols", [])
        extra = overlay.get("extra_symbols", {})
        symbols = overlay.get("symbols", {})
        if not isinstance(missing, list):
            issues.append(f"{language}: missing-symbol debt is not an array")
        elif missing:
            issues.append(f"{language}: compatibility debt may not permit missing symbols")
        if not isinstance(extra, dict):
            issues.append(f"{language}: extra-symbol debt is not an object")
        elif extra:
            issues.append(f"{language}: compatibility debt may not permit extra symbols")
        if not isinstance(symbols, dict):
            issues.append(f"{language}: symbol compatibility debt is not an object")
            continue
        debt_symbol_count += len(symbols)
        for name, difference in symbols.items():
            if not isinstance(difference, dict) or not difference:
                issues.append(f"{language}: compatibility debt for {name} is empty or invalid")
                continue
            forbidden = sorted(set(difference) & {"domain", "type", "mutability"})
            if forbidden:
                issues.append(
                    f"{language}: compatibility debt may not override "
                    f"{', '.join(forbidden)} for {name}"
                )
            unexpected_symbol_fields = sorted(
                set(difference) - permitted_symbol_fields - {"domain", "type", "mutability"}
            )
            if unexpected_symbol_fields:
                issues.append(
                    f"{language}: compatibility debt for {name} has unexpected field(s): "
                    + ", ".join(unexpected_symbol_fields)
                )
            quadrant_debt = difference.get("quadrants")
            if quadrant_debt is not None:
                if not isinstance(quadrant_debt, dict) or not quadrant_debt:
                    issues.append(f"{language}: quadrant debt for {name} is empty or invalid")
                else:
                    unknown_quadrants = sorted(
                        set(quadrant_debt) - {quadrant.name for quadrant in QUADRANTS}
                    )
                    if unknown_quadrants:
                        issues.append(
                            f"{language}: quadrant debt for {name} names unknown quadrant(s): "
                            + ", ".join(unknown_quadrants)
                        )
    if debt_symbol_count > MAX_COMPATIBILITY_DEBT_SYMBOLS:
        issues.append(
            "schema: catalog compatibility debt grew from the 57-symbol ceiling "
            f"to {debt_symbol_count}"
        )
    return issues


def make_schema() -> dict:
    schema = make_canonical_schema()
    canonical = schema["symbols"]
    schema["catalog_compatibility_debt"] = {
        language.name: inventory_overrides(canonical, build_language_inventory(language))
        for language in LANGUAGES[1:]
    }
    debt_issues = validate_compatibility_debt(schema["catalog_compatibility_debt"])
    if debt_issues:
        raise SchemaError(
            "refusing to write invalid or expanded catalog compatibility debt: "
            + "; ".join(debt_issues)
        )
    return schema


def _compare_mapping(expected: Mapping, actual: Mapping, prefix: str) -> list[str]:
    issues = []
    for name in sorted(set(expected) - set(actual)):
        issues.append(f"{prefix}: missing symbol {name}")
    for name in sorted(set(actual) - set(expected)):
        issues.append(f"{prefix}: extra symbol {name}")
    for name in sorted(set(expected) & set(actual)):
        wanted = expected[name]
        found = actual[name]
        fields = ["domain", "type", "source_dimensions", "mutability"]
        if "declaration" in wanted and "declaration" in found:
            fields.append("declaration")
        for field in fields:
            if wanted[field] != found[field]:
                issues.append(
                    f"{prefix}: {field} mismatch for {name}: "
                    f"expected {wanted[field]!r}, got {found[field]!r}"
                )
        if wanted["conditional_layout"] != found["conditional_layout"]:
            issues.append(
                f"{prefix}: conditional mismatch for {name}: "
                f"expected {wanted['conditional_layout']!r}, got {found['conditional_layout']!r}"
            )
        for quadrant in QUADRANTS:
            quadrant_name = quadrant.name
            expected_variant = wanted["quadrants"][quadrant_name]
            actual_variant = found["quadrants"][quadrant_name]
            if expected_variant["effective_dimensions"] != actual_variant["effective_dimensions"]:
                issues.append(
                    f"{prefix}/{quadrant_name}: dimension mismatch for {name}: "
                    f"expected {expected_variant['effective_dimensions']!r}, "
                    f"got {actual_variant['effective_dimensions']!r}"
                )
            if expected_variant["initializer_entries"] != actual_variant["initializer_entries"]:
                issues.append(
                    f"{prefix}/{quadrant_name}: entry-count mismatch for {name}: "
                    f"expected {expected_variant['initializer_entries']}, "
                    f"got {actual_variant['initializer_entries']}"
                )
    return issues


def _validate_catalog_header(language: Language, canonical: dict) -> list[str]:
    if language.ja25_header is None:
        return []
    actual = parse_header(_read(language.ja25_header), language.ja25_header)
    expected = parse_header(
        _read("i18n/include/_Ja25EnglishText.h"),
        "i18n/include/_Ja25EnglishText.h",
    )
    expected_symbols = {symbol["name"]: symbol for symbol in expected["symbols"]}
    actual_symbols = {symbol["name"]: symbol for symbol in actual["symbols"]}
    issues = []
    for name in sorted(set(expected_symbols) - set(actual_symbols)):
        issues.append(f"{language.name}/JA25 header: missing symbol {name}")
    for name in sorted(set(actual_symbols) - set(expected_symbols)):
        issues.append(f"{language.name}/JA25 header: extra symbol {name}")
    for name in sorted(set(expected_symbols) & set(actual_symbols)):
        if expected_symbols[name] != actual_symbols[name]:
            issues.append(
                f"{language.name}/JA25 header: declaration mismatch for {name}: "
                f"expected {expected_symbols[name]!r}, got {actual_symbols[name]!r}"
            )
    if actual["functions"]:
        issues.append(f"{language.name}/JA25 header: unexpected function declarations")
    if actual["duplicate_declarations"]:
        issues.append(f"{language.name}/JA25 header: duplicate data declarations")
    return issues


def validate_schema(schema: dict) -> list[str]:
    issues = []
    if schema.get("schema_version") != 1:
        issues.append("schema: unsupported schema_version")
    if schema.get("canonical_language") != "English":
        issues.append("schema: canonical language must be English")
    if schema.get("fallback_policy") != FALLBACK_POLICY:
        issues.append("schema: English fallback policy is missing or changed")
    generated = make_canonical_schema()
    if schema.get("languages") != generated["languages"]:
        issues.append("schema: supported language catalog changed")
    if schema.get("quadrants") != generated["quadrants"]:
        issues.append("schema: campaign/build quadrants changed")
    for key in ("header_inventory",):
        if schema.get(key) != generated[key]:
            issues.append(f"schema: canonical {key} drifted; regenerate and review the ABI change")
    expected_symbols = schema.get("symbols")
    if not isinstance(expected_symbols, dict):
        issues.append("schema: symbols inventory is missing")
        return issues
    issues.extend(_compare_mapping(expected_symbols, generated["symbols"], "English"))

    debt = schema.get("catalog_compatibility_debt")
    expected_debt_languages = {language.name for language in LANGUAGES[1:]}
    if not isinstance(debt, dict) or set(debt) != expected_debt_languages:
        issues.append("schema: catalog compatibility-debt inventory is missing or incomplete")
        debt = {}
    issues.extend(validate_compatibility_debt(debt))
    # English was just regenerated and compared above; the remaining seven
    # complete the mandatory eight-catalog gate without parsing it twice.
    for language in LANGUAGES[1:]:
        actual = build_language_inventory(language)
        expected_catalog = expected_symbols
        if language.name != "English":
            try:
                expected_catalog = apply_inventory_overrides(
                    expected_symbols,
                    debt.get(language.name, {}),
                )
            except SchemaError as error:
                issues.append(f"{language.name}: {error}")
        issues.extend(_compare_mapping(expected_catalog, actual, language.name))
        issues.extend(_validate_catalog_header(language, schema))
    return issues


def summary(schema: dict) -> str:
    symbols = schema["symbols"]
    mutable_buffers = sum(
        symbol["mutability"] == "writable-character-buffer"
        for symbol in symbols.values()
    )
    conditional = sum(bool(symbol["conditional_layout"]) for symbol in symbols.values())
    debt = schema.get("catalog_compatibility_debt", {})
    debt_symbols = sum(
        len(overlay.get("missing_symbols", []))
        + len(overlay.get("extra_symbols", {}))
        + len(overlay.get("symbols", {}))
        for overlay in debt.values()
        if isinstance(overlay, dict)
    )
    return (
        f"{len(LANGUAGES)} catalogs x {len(QUADRANTS)} quadrants; "
        f"{len(symbols)} canonical data symbols, {mutable_buffers} writable buffers, "
        f"{conditional} condition-shaped symbols, {debt_symbols} ratcheted compatibility gaps"
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--write-schema",
        action="store_true",
        help="replace the committed schema with the current English ABI",
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
                object_pairs_hook=unique_json_object,
            )
        except FileNotFoundError as error:
            raise SchemaError(f"schema missing: {SCHEMA_PATH.relative_to(ROOT)}") from error
        except (OSError, json.JSONDecodeError) as error:
            raise SchemaError(f"cannot load schema: {error}") from error
        issues = validate_schema(schema)
    except SchemaError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    print(summary(schema))
    if issues:
        print(f"FAIL: {len(issues)} text ABI schema mismatch(es):", file=sys.stderr)
        for issue in issues:
            print(f"  {issue}", file=sys.stderr)
        return 1
    print("OK: all compiled text catalogs match their canonical ABI schema contracts.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
