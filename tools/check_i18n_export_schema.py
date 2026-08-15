#!/usr/bin/env python3
"""Validate the complete GameStrings export contract without building JA2.

The developer exporter injects an immediate-copy sink into one language-variant
adapter. That adapter enumerates the selected linked legacy globals and the
immutable TextPack in the historical order. This tool checks the split against
ExportStrings.cpp, SelectedCatalogExport.cpp, TextCatalog.h, the compiled-text
ABI schema, all eight catalog shapes, CMake ownership, and the startup call
chain.

The manifest is deliberately descriptive. Every selected catalog covers the
reviewed legacy export ranges, exact semantic-slot goldens keep those repairs
from becoming empty padding or shifted entries, and source ratchets prevent the
sink from retaining borrowed global storage.

Usage:
    python3 tools/check_i18n_export_schema.py
    python3 tools/check_i18n_export_schema.py --write-schema
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import re
import sys
from pathlib import Path
from typing import Iterable, Mapping, Sequence

import check_i18n_text_schema as ABI


ROOT = Path(__file__).resolve().parent.parent
SCHEMA_PATH = ROOT / "i18n" / "export_text_schema.json"
EXPORT_SOURCE = "i18n/SelectedCatalogExport.cpp"
PROPERTY_EXPORT_SOURCE = "i18n/ExportStrings.cpp"
EXPORT_HEADER = "i18n/include/SelectedCatalogExport.h"
I18N_BUILD_SOURCE = "i18n/CMakeLists.txt"
TEXT_CATALOG_HEADER = "i18n/include/TextCatalog.h"

EXPECTED_LOGICAL_SECTIONS = 238
EXPECTED_LEGACY_SECTIONS = 210
EXPECTED_TEXT_PACK_SECTIONS = 28
EXPECTED_TEXT_PACK_ENTRIES = 8
EXPECTED_TEXT_PACK_TABLES = 20
EXPECTED_LEGACY_POINTER_TABLES = 194
EXPECTED_LEGACY_WRITABLE_BUFFERS = 16
EXPECTED_LEGACY_TEXT_H_SYMBOLS = 205
EXPECTED_LEGACY_LOCAL_EXTERN_SYMBOLS = 5
MAX_EXPORTED_COMPATIBILITY_DEBT_PAIRS = 17
EXPECTED_LEGACY_RANGE_COMPARISONS = (
    EXPECTED_LEGACY_SECTIONS * len(ABI.LANGUAGES) * len(ABI.QUADRANTS)
)
EXPECTED_EXPORTED_POINTER_ENTRY_CHECKS = 83040
EXPECTED_DIRECT_WIDE_LITERAL_ENTRY_CHECKS = 82712
EXPECTED_COMPILED_SELECTOR_ENTRY_CHECKS = 328
MAX_UNSAFE_RANGE_SECTIONS = 0
MAX_UNSAFE_RANGE_LANGUAGE_PAIRS = 0
MAX_UNSAFE_RANGE_QUADRANT_FAILURES = 0
MAX_POTENTIAL_OOB_READS_PER_SELECTED_BUILD = 0
EXPECTED_EXPORTER_ONLY_TABLES = 0
EXPECTED_EXPORTER_ONLY_ENTRIES = 0
EXPECTED_LINKED_CATALOG_SOURCES = [
    "_ChineseText.cpp",
    "_DutchText.cpp",
    "_EnglishText.cpp",
    "_FrenchText.cpp",
    "_GermanText.cpp",
    "_ItalianText.cpp",
    "_PolishText.cpp",
    "_RussianText.cpp",
]
EXPECTED_CATALOG_PREAMBLE_INCLUDES = [
    "Text.h",
    "FileMan.h",
    "Scheduling.h",
    "EditorMercs.h",
    "Item Statistics.h",
    "CompiledConditionalTextSelectors.inc",
]

EXPECTED_EXPORT_LIMIT_SEAM_DIRECTIVES = [
    "#ifdef static_assert",
    '#error "GameStrings export limits require the built-in static_assert keyword"',
    "#endif",
    '#include "ExportStringLimitContract.inc"',
]

NAMED_EXPORT_LIMIT_MANIFEST = "i18n/include/ExportStringLimitContract.inc"
EXPECTED_NAMED_EXPORT_LIMITS = 77

EXPECTED_NORMALIZED_EXPORT_LIMITS: Mapping[str, int] = {
    "WeaponType": 9,
    "TeamTurnString": 10,
    "Message": 88,
    "pPersonnelAssignmentStrings": 85,
    "pLongAssignmentStrings": 85,
    "pPersonnelScreenStrings": 26,
    "pDoorTrapStrings": 7,
    "pLandTypeStrings": 47,
    "TacticalStr": 213,
    "pBookMarkStrings": 18,
    "pMercHeLeaveString": 2,
    "pMercSheLeaveString": 2,
    "gzGIOScreenText": 69,
}

# The repaired slots are source/schema boundaries, not translation policy.
# Native catalog literals are retained where they already exist; otherwise the
# canonical English fallback remains visibly marked TODO.Translate in source.
FOREIGN_NORMALIZED_REPAIRED_SLOTS = frozenset(
    {
        ("German", "TacticalStr", 212),
        ("German", "pBookMarkStrings", 17),
        ("Russian", "pBookMarkStrings", 17),
        ("Dutch", "TacticalStr", 212),
        ("Dutch", "pBookMarkStrings", 17),
        ("Dutch", "pPersonnelScreenStrings", 17),
        ("Polish", "TacticalStr", 212),
        ("Polish", "TeamTurnString", 5),
        ("Polish", "pBookMarkStrings", 17),
        ("French", "TacticalStr", 212),
        ("French", "pBookMarkStrings", 17),
        ("Italian", "Message", 65),
        ("Italian", "Message", 66),
        ("Italian", "Message", 67),
        ("Italian", "TacticalStr", 103),
        ("Italian", "TacticalStr", 212),
        ("Italian", "pBookMarkStrings", 8),
        ("Italian", "pBookMarkStrings", 9),
        ("Italian", "pBookMarkStrings", 17),
    }
)

# Include both repaired slots and the adjacent/tail anchors that prove later
# entries retained their canonical GameStrings ordinals.
NORMALIZED_CATALOG_GOLDENS: dict[tuple[str, str], dict[int, str]] = {
    ("German", "TacticalStr"): {
        211: "%s has stopped chatting with %s",
        212: "Attempt to turn",
    },
    ("German", "pBookMarkStrings"): {17: "A.R.C."},
    ("Russian", "pBookMarkStrings"): {17: "A.R.C."},
    ("Dutch", "TacticalStr"): {
        211: "%s has stopped chatting with %s",
        212: "Attempt to turn",
    },
    ("Dutch", "pBookMarkStrings"): {17: "A.R.C."},
    ("Dutch", "pPersonnelScreenStrings"): {
        17: "Contract:",
        18: "Huidige Tot. Service:",
        25: "Achievements:",
    },
    ("Polish", "TacticalStr"): {
        211: "%s has stopped chatting with %s",
        212: "Attempt to turn",
    },
    ("Polish", "TeamTurnString"): {
        4: "Tura cywili",
        5: "Player_Plan",
    },
    ("Polish", "pBookMarkStrings"): {17: "A.R.C."},
    ("French", "TacticalStr"): {
        211: "%s has stopped chatting with %s",
        212: "Attempt to turn",
    },
    ("French", "pBookMarkStrings"): {17: "A.R.C."},
    ("Italian", "Message"): {
        65: "accurate",
        66: "inaccurate",
        67: "no semi auto",
        68: "The enemy has no more items to steal!",
        85: "Assignment not possible at the moment",
        86: "No militia that can be drilled present.",
        87: "%s has fully explored %s.",
    },
    ("Italian", "TacticalStr"): {
        103: "PRISONER",
        104: "Settore di uscita",
        211: "%s has stopped chatting with %s",
        212: "Attempt to turn",
    },
    ("Italian", "pBookMarkStrings"): {
        8: "Encyclopedia",
        9: "Briefing Room",
        17: "A.R.C.",
    },
}

ASSIGNMENT_REPAIR_LITERALS: Mapping[str, tuple[str, str, str]] = {
    "English": ("Eat", "Event", "Mission"),
    "German": ("Essen", "Event", "Mission"),
    "Russian": ("Питается", "Event", "Mission"),
    "Dutch": ("Eat", "Event", "Mission"),
    "Polish": ("Eat", "Event", "Mission"),
    "French": ("Mange", "Event", "Mission"),
    "Italian": ("Eat", "Event", "Mission"),
    "Chinese": ("用餐", "事件", "任务"),
}

ASSIGNMENT_ALIGNMENT_LITERALS: Mapping[str, tuple[str, str, str, str]] = {
    "English": ("Exploration", "Staff Facility", "Rest at Facility", "Empty"),
    "German": ("Exploration", "Betriebspersonal", "Betriebspause", "Leer"),
    "Russian": ("Exploration", "Работает с населением", "Отдыхает в заведении", "Без пассажиров"),
    "Dutch": ("Exploration", "Staff Facility", "Rest at Facility", "Leeg"),
    "Polish": ("Exploration", "Staff Facility", "Rest at Facility", "Pusty"),
    "French": ("Exploration", "Renseignement", "Repos", "Vide"),
    "Italian": ("Exploration", "Staff Facility", "Rest at Facility", "Vuoto"),
    "Chinese": ("探索事项", "兼职", "休养", "空车"),
}

DOOR_TRAP_LITERALS: Mapping[str, tuple[str, str, str]] = {
    "English": ("an electric trap", "a siren trap", "a silent alarm trap"),
    "German": ("eine elektrische Falle", "eine Falle mit Sirene", "eine Falle mit stummem Alarm"),
    "Russian": ("электроловушка", "сирена", "сигнализация"),
    "Dutch": ("een elektrische val", "alarm", "stil alarm"),
    "Polish": ("jest pod napięciem", "posiada syrenę alarmową", "posiada dyskretny alarm"),
    "French": ("un piège électrique", "une alarme sonore", "une alarme silencieuse"),
    "Italian": ("una trappola elettrica", "una trappola con sirena", "una trappola con allarme insonoro"),
    "Chinese": ("一个带电陷阱", "一个警报陷阱", "一个无声警报陷阱"),
}

LAND_TYPE_TAIL = (
    "Final Complex",
    "Guard Post",
    "Crash Site",
    "Power Plant",
    "Mountains",
    "Unknown",
)

UNIVERSAL_RANGE_REPAIRED_SLOTS = frozenset(
    (language.name, symbol, index)
    for language in ABI.LANGUAGES
    for symbol, indices in (
        ("pPersonnelAssignmentStrings", (83, 84)),
        ("pLongAssignmentStrings", (54, 83, 84)),
        ("pDoorTrapStrings", (5, 6)),
        ("pLandTypeStrings", (41, 42, 43, 44, 45, 46)),
    )
    for index in indices
)
NORMALIZED_REPAIRED_SLOTS = (
    FOREIGN_NORMALIZED_REPAIRED_SLOTS | UNIVERSAL_RANGE_REPAIRED_SLOTS
)

for _language in ABI.LANGUAGES:
    _name = _language.name
    _eat, _event, _mission = ASSIGNMENT_REPAIR_LITERALS[_name]
    _exploration, _staff, _rest, _empty = ASSIGNMENT_ALIGNMENT_LITERALS[_name]
    _electric, _siren, _silent = DOOR_TRAP_LITERALS[_name]
    NORMALIZED_CATALOG_GOLDENS.setdefault(
        (_name, "pPersonnelAssignmentStrings"), {}
    ).update({82: _exploration, 83: _event, 84: _mission})
    NORMALIZED_CATALOG_GOLDENS.setdefault(
        (_name, "pLongAssignmentStrings"), {}
    ).update(
        {
            53: _staff,
            54: _eat,
            55: _rest,
            61: _empty,
            82: _exploration,
            83: _event,
            84: _mission,
        }
    )
    NORMALIZED_CATALOG_GOLDENS.setdefault(
        (_name, "pDoorTrapStrings"), {}
    ).update({2: _electric, 3: _siren, 4: _silent, 5: _siren, 6: _electric})
    NORMALIZED_CATALOG_GOLDENS.setdefault(
        (_name, "pLandTypeStrings"), {}
    ).update({40: "", **{41 + index: value for index, value in enumerate(LAND_TYPE_TAIL)}})

NORMALIZED_CATALOG_GOLDENS.setdefault(("Italian", "gzGIOScreenText"), {}).update(
    {
        5: "Opzioni delle armi",
        6: "Varietà di armi",
        11: "Professionista",
        12: "INSANE",
        13: "Start",
        23: "Awesome",
        24: "Inventory / Attachments",
        68: "Extreme Iron Man",
    }
)

IMPLEMENTED_ADAPTER_STATUS = {
    "state": "implemented",
    "reason": (
        "one language-variant translation unit enumerates all 238 ordered "
        "sections from selected linked globals and the immutable TextPack "
        "through an injected immediate-copy sink; all 210 legacy ranges retain "
        "their exhaustive eight-language/four-quadrant proof"
    ),
}

EXPECTED_LOCAL_EXTERN_SYMBOLS = [
    "pBullseyeStrings",
    "pContractButtonString",
    "pUpdatePanelButtons",
    "gzIntroScreen",
    "sRepairsDoneString",
]

EXPECTED_VARIANT_SOURCES = [
    "language.cpp",
    "ExportStrings.cpp",
    "SelectedCatalogExport.cpp",
    "_ChineseText.cpp",
    "_DutchText.cpp",
    "_EnglishText.cpp",
    "_FrenchText.cpp",
    "_GermanText.cpp",
    "_ItalianText.cpp",
    "_Ja25ChineseText.cpp",
    "_Ja25DutchText.cpp",
    "_Ja25EnglishText.cpp",
    "_Ja25FrenchText.cpp",
    "_Ja25GermanText.cpp",
    "_Ja25ItalianText.cpp",
    "_Ja25PolishText.cpp",
    "_Ja25RussianText.cpp",
    "_PolishText.cpp",
    "_RussianText.cpp",
]

EXPECTED_ADAPTER_HELPER_NAMESPACE = r"""
namespace
{
template<typename T>
void ExportSection(i18n::SelectedCatalogExportSink& sink,
    std::wstring_view section, T* strings, int first, int limit)
{
    for (int index = first; index < limit; ++index)
    {
        const std::wstring_view text{strings[index]};
        if (!text.empty()) sink.copyEntry(section, index, text);
    }
}

template<>
void ExportSection<wchar_t>(i18n::SelectedCatalogExportSink& sink,
    std::wstring_view section, wchar_t* strings, int first, int limit)
{
    ExportSection(sink, section, &strings, first, limit);
}

void ExportTextPackEntry(i18n::SelectedCatalogExportSink& sink,
    i18n::TextKey key)
{
    const auto* descriptor = i18n::FindTextKey(key);
    const auto text = i18n::GetCompiledTextPack().lookup(key);
    if (!descriptor || !text || text.text.empty()) return;
    sink.copyEntry(descriptor->legacyExportSection, 0, text.text);
}

void ExportTextPackTable(i18n::SelectedCatalogExportSink& sink,
    i18n::TextTableKey key)
{
    const auto* descriptor = i18n::FindTextTable(key);
    if (!descriptor) return;
    const auto& pack = i18n::GetCompiledTextPack();
    const auto exportEnd =
        descriptor->legacyExportFirst + descriptor->legacyExportCount;
    for (std::size_t index = descriptor->legacyExportFirst;
        index < exportEnd; ++index)
    {
        const auto text = pack.lookup(key, index);
        if (!text || text.text.empty()) continue;
        sink.copyEntry(descriptor->legacyExportSection,
            static_cast<int>(index), text.text);
    }
}
}
"""

SOURCE_SUFFIXES = frozenset({".c", ".cc", ".cpp", ".cxx", ".h", ".hpp"})
CATALOG_SOURCES = frozenset(
    path
    for language in ABI.LANGUAGES
    for path in (language.base_source, language.ja25_source)
)
EXPORTER_ONLY_SCAN_EXCLUSIONS = CATALOG_SOURCES | frozenset(
    {EXPORT_SOURCE, PROPERTY_EXPORT_SOURCE, "i18n/include/Text.h"}
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


NAMED_LIMIT_ENTRY = re.compile(
    r'static_assert\(([A-Za-z_]\w*)[ \t]*==[ \t]*(\d+),[ \t]*'
    r'"GameStrings:\1"\);'
)

PREPROCESSOR_DIRECTIVE = re.compile(
    r"^[ \t]*#[ \t]*(?P<name>[A-Za-z_]\w*)(?P<rest>[^\r\n]*)",
    re.MULTILINE,
)

EXPORT_FUNCTION_DEFINITION = re.compile(
    r"\bvoid[ \t\r\n]+i18n::ExportSelectedCatalog[ \t\r\n]*"
    r"\([ \t\r\n]*SelectedCatalogExportSink[ \t\r\n]*&[ \t\r\n]*sink"
    r"[ \t\r\n]*\)[ \t\r\n]*\{"
)

CONDITIONAL_DIRECTIVE_OPENERS = {"if", "ifdef", "ifndef"}
CONDITIONAL_DIRECTIVE_BRANCHES = {"elif", "else"}


def _reject_preprocessor_obfuscation(source: str, context: str) -> str:
    """Return a lexical mask after rejecting alternate/spliced directives."""

    masked = lexical_mask(source)
    if re.search(r"^[ \t]*(?:%:|\?\?=)", masked, re.MULTILINE):
        raise ExportSchemaError(
            f"{context}: alternative preprocessor tokens are unsupported"
        )
    if re.search(r"\\[ \t]*(?:\r?\n|$)|\?\?/", masked):
        raise ExportSchemaError(
            f"{context}: continued preprocessor spellings are unsupported"
        )
    return masked


def _directive_matches(source: str, context: str) -> tuple[str, list[re.Match]]:
    masked = _reject_preprocessor_obfuscation(source, context)
    return masked, list(PREPROCESSOR_DIRECTIVE.finditer(masked))


def _structural_directive_matches(
    source: str, context: str
) -> list[re.Match]:
    """Parse directive nesting while honoring ordinary line splicing."""

    masked = lexical_mask(source)
    if re.search(r"^[ \t]*(?:%:|\?\?=)", masked, re.MULTILINE) or "??/" in masked:
        raise ExportSchemaError(
            f"{context}: alternative preprocessor tokens are unsupported"
        )
    logical = re.sub(r"\\[ \t]*\r?\n", "", masked)
    return list(PREPROCESSOR_DIRECTIVE.finditer(logical))


def _conditional_depth(
    directives: Sequence[re.Match], context: str, initial: int = 0
) -> int:
    depth = initial
    for directive in directives:
        name = directive.group("name")
        if name in CONDITIONAL_DIRECTIVE_OPENERS:
            depth += 1
        elif name in CONDITIONAL_DIRECTIVE_BRANCHES:
            if depth == 0:
                raise ExportSchemaError(f"{context}: unmatched conditional branch")
        elif name == "endif":
            depth -= 1
            if depth < 0:
                raise ExportSchemaError(f"{context}: unmatched #endif")
    return depth


def _require_unconditional_definition(
    directives: Sequence[re.Match], definition_start: int, context: str
) -> None:
    """Require one definition to start/end outside every conditional region."""

    depth = 0
    definition_depth = None
    for directive in directives:
        if definition_depth is None and directive.start() >= definition_start:
            definition_depth = depth
        name = directive.group("name")
        if name in CONDITIONAL_DIRECTIVE_OPENERS:
            depth += 1
        elif name in CONDITIONAL_DIRECTIVE_BRANCHES:
            if depth == 0:
                raise ExportSchemaError(f"{context}: unmatched conditional branch")
        elif name == "endif":
            depth -= 1
            if depth < 0:
                raise ExportSchemaError(f"{context}: unmatched #endif")
    if definition_depth is None:
        definition_depth = depth
    if definition_depth != 0:
        raise ExportSchemaError(
            f"{context} must be an unconditional top-level definition"
        )
    if depth != 0:
        raise ExportSchemaError(
            f"a preprocessor conditional crosses the {context} boundary"
        )


def _validate_export_source_preprocessor(export_source: str) -> None:
    """Keep the selected-catalog adapter and compiler seam unreplaced."""

    masked = lexical_mask(export_source)
    definitions = list(EXPORT_FUNCTION_DEFINITION.finditer(masked))
    if len(definitions) != 1:
        raise ExportSchemaError(
            "selected-catalog adapter must retain one exact sink entrypoint"
        )
    opening = masked.find("{", definitions[0].start(), definitions[0].end())
    closing = _matching_delimiter(masked, opening, "{", "}")
    reviewed_source = export_source[:closing + 1]
    _masked, directives = _directive_matches(
        reviewed_source, "SelectedCatalogExport.cpp"
    )
    for directive in directives:
        if directive.group("name") in {"define", "undef"}:
            raise ExportSchemaError(
                "SelectedCatalogExport.cpp may not define or undefine macros "
                "across the reviewed export boundary"
            )
    _require_unconditional_definition(
        directives, definitions[0].start(), "i18n::ExportSelectedCatalog"
    )


def _validated_export_body_directives(raw_body: str) -> list[re.Match]:
    _masked, directives = _directive_matches(
        raw_body, "i18n::ExportSelectedCatalog"
    )
    actual = [raw_body[match.start():match.end()].strip() for match in directives]
    if actual != EXPECTED_EXPORT_LIMIT_SEAM_DIRECTIVES:
        raise ExportSchemaError(
            "i18n::ExportSelectedCatalog must begin with the exact static_assert "
            "guard and named-limit contract include"
        )
    prefix = raw_body[:directives[-1].end()]
    prefix_lines = [line.strip() for line in prefix.splitlines() if line.strip()]
    if prefix_lines != EXPECTED_EXPORT_LIMIT_SEAM_DIRECTIVES:
        raise ExportSchemaError(
            "the static_assert guard and named-limit include must be the exact "
            "adapter prefix"
        )
    return directives


def parse_named_export_limit_manifest(source: str) -> dict[str, int]:
    """Parse the strict compiler-owned list of live limit assertions."""

    masked = lexical_mask(source)
    if re.search(r"^[ \t]*(?:#|%:)", masked, re.MULTILINE) or (
        comments_blanked(source) != source
    ):
        raise ExportSchemaError(
            "named export-limit contract may not contain directives or comments"
        )
    lines = source.splitlines()
    if len(lines) != EXPECTED_NAMED_EXPORT_LIMITS or any(
        not line.strip() for line in lines
    ):
        raise ExportSchemaError(
            "named export-limit contract must be exactly "
            f"{EXPECTED_NAMED_EXPORT_LIMITS} nonblank rows"
        )
    result: dict[str, int] = {}
    order = []
    for line_number, line in enumerate(lines, start=1):
        match = NAMED_LIMIT_ENTRY.fullmatch(line.strip())
        if match is None:
            raise ExportSchemaError(
                f"named export-limit contract:{line_number}: unsupported assertion"
            )
        name, value_text = match.groups()
        if name in result:
            raise ExportSchemaError(
                f"named export-limit contract duplicates {name}"
            )
        result[name] = int(value_text)
        order.append(name)
    if len(result) != EXPECTED_NAMED_EXPORT_LIMITS:
        raise ExportSchemaError(
            "named export-limit contract must contain exactly "
            f"{EXPECTED_NAMED_EXPORT_LIMITS} entries, got {len(result)}"
        )
    if order != sorted(order):
        raise ExportSchemaError(
            "named export-limit contract entries must remain alphabetically ordered"
        )
    return result


def validate_named_limit_static_assert_seam(export_source: str) -> None:
    """Pin one direct, first-statement include in the variant adapter TU."""

    _validate_export_source_preprocessor(export_source)
    if len(list(EXPORT_FUNCTION_DEFINITION.finditer(lexical_mask(export_source)))) != 1:
        raise ExportSchemaError(
            "selected-catalog adapter must retain its exact sink entrypoint"
        )

    include_pattern = re.compile(
        r'^[ \t]*#[ \t]*include[ \t]+"ExportStringLimitContract[.]inc"[ \t]*$',
        re.MULTILINE,
    )
    includes = list(include_pattern.finditer(comments_blanked(export_source)))
    if len(includes) != 1:
        raise ExportSchemaError(
            "selected-catalog adapter must include the named-limit static-assert "
            "contract exactly once"
        )
    body = extract_function_body(export_source, "i18n::ExportSelectedCatalog")
    _validated_export_body_directives(body)


NAMED_EXPORT_LIMITS: Mapping[str, int] = parse_named_export_limit_manifest(
    _read(NAMED_EXPORT_LIMIT_MANIFEST)
)


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


def _function_definition_span(
    text: str, qualified_name: str
) -> tuple[int, int, int]:
    """Return the start/open/close offsets of one function definition."""

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
    return matches[0].start(), opening, closing


def extract_function_body(text: str, qualified_name: str) -> str:
    """Return one function body, ignoring calls and declarations."""

    _start, opening, closing = _function_definition_span(text, qualified_name)
    return text[opening + 1:closing]


def _active_function_body(
    source: str, qualified_name: str, context: str
) -> str:
    """Return a uniquely defined function that is not preprocessor-wrapped."""

    start, opening, closing = _function_definition_span(source, qualified_name)
    prefix_depth = _conditional_depth(
        _structural_directive_matches(source[:start], context), context
    )
    if prefix_depth != 0:
        raise ExportSchemaError(
            f"{context} must be an unconditional top-level definition"
        )
    span_depth = _conditional_depth(
        _structural_directive_matches(source[start:closing + 1], context), context
    )
    if span_depth != 0:
        raise ExportSchemaError(
            f"a preprocessor conditional crosses the {context} boundary"
        )
    return source[opening + 1:closing]


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


def _reject_conditionals(source: str, context: str) -> None:
    if re.search(
        r"^[ \t]*#[ \t]*(?:if|ifdef|ifndef|elif|else|endif)\b",
        lexical_mask(source),
        re.MULTILINE,
    ):
        raise ExportSchemaError(
            f"{context} may not place reviewed source behind preprocessor conditionals"
        )


def _export_top_level_statements(
    raw_body: str, directives: Sequence[re.Match]
) -> list[str]:
    """Split the deliberately flat exporter into its depth-zero statements."""

    without_directives = list(raw_body)
    for directive in directives:
        _blank_span(
            without_directives, raw_body, directive.start(), directive.end()
        )
    source = "".join(without_directives)
    masked = lexical_mask(source)
    if "{" in masked or "}" in masked:
        raise ExportSchemaError(
            "i18n::ExportSelectedCatalog must remain a flat control-flow-free body"
        )
    statements = []
    start = 0
    paren_depth = bracket_depth = 0
    for index, character in enumerate(masked):
        if character == "(":
            paren_depth += 1
        elif character == ")":
            paren_depth -= 1
        elif character == "[":
            bracket_depth += 1
        elif character == "]":
            bracket_depth -= 1
        if min(paren_depth, bracket_depth) < 0:
            raise ExportSchemaError(
                "unbalanced delimiter in i18n::ExportSelectedCatalog"
            )
        if character == ";" and paren_depth == 0 and bracket_depth == 0:
            statement = normalize_expression(
                comments_blanked(source[start:index + 1])
            )
            if statement:
                statements.append(statement)
            start = index + 1
    if (paren_depth, bracket_depth) != (0, 0):
        raise ExportSchemaError(
            "unbalanced delimiter in i18n::ExportSelectedCatalog"
        )
    if lexical_mask(source[start:]).strip():
        raise ExportSchemaError(
            "i18n::ExportSelectedCatalog has executable text outside a direct statement"
        )
    return statements


def _validate_named_limit_usage_provenance(
    raw_body: str, calls: Sequence[Mapping]
) -> None:
    """Require each limit identifier use to belong to a parsed range."""

    expected = Counter()
    for call in calls:
        if call["source_kind"] != "legacy":
            continue
        for expression in call["range"].values():
            expected.update(
                name
                for name in re.findall(r"\b[A-Za-z_]\w*\b", expression)
                if name in NAMED_EXPORT_LIMITS
            )
    active = lexical_mask(raw_body)
    actual = Counter(
        {
            name: len(re.findall(rf"\b{re.escape(name)}\b", active))
            for name in NAMED_EXPORT_LIMITS
        }
    )
    differences = [
        f"{name}:body={actual[name]},ranges={expected[name]}"
        for name in NAMED_EXPORT_LIMITS
        if actual[name] != expected[name]
    ]
    if differences:
        raise ExportSchemaError(
            "named export-limit identifier escaped parsed range ownership: "
            + ", ".join(differences)
        )


def _validate_export_statement_inventory(
    raw_body: str,
    calls: Sequence[Mapping],
    directives: Sequence[re.Match],
) -> None:
    statements = _export_top_level_statements(raw_body, directives)
    if len(statements) != len(calls):
        raise ExportSchemaError(
            "i18n::ExportSelectedCatalog statement inventory changed: "
            f"expected {len(calls)}, got {len(statements)}"
        )
    for statement, call in zip(statements, calls):
        expected_name = {
            "legacy": "ExportSection",
            "text-pack-entry": "ExportTextPackEntry",
            "text-pack-table": "ExportTextPackTable",
        }[call["source_kind"]]
        if not statement.startswith(expected_name + "(") or not statement.endswith(");"):
            raise ExportSchemaError(
                "selected-catalog adapter export statements are not exact and contiguous"
            )


def _descriptor_array_region(header: str, variable: str) -> str:
    masked = _reject_preprocessor_obfuscation(
        header, f"TextCatalog {variable} descriptors"
    )
    marker = re.compile(rf"\b{re.escape(variable)}\s*(?:=\s*)?\{{\s*\{{")
    candidates = []
    for match in marker.finditer(masked):
        line_start = masked.rfind("\n", 0, match.start()) + 1
        if re.match(r"[ \t]*#", masked[line_start:match.start()]):
            continue
        candidates.append(match)
    if len(candidates) != 1:
        raise ExportSchemaError(
            f"TextCatalog must own exactly one active {variable} descriptor array"
        )
    outer_open = masked.find("{", candidates[0].start(), candidates[0].end())
    inner_open = masked.find("{", outer_open + 1, candidates[0].end())
    inner_close = _matching_delimiter(masked, inner_open, "{", "}")
    region = header[inner_open + 1:inner_close]
    _region_masked, directives = _directive_matches(
        region, f"TextCatalog {variable} descriptor array"
    )
    if directives:
        raise ExportSchemaError(
            f"TextCatalog {variable} descriptor array may not contain directives"
        )
    return region


def parse_text_pack_descriptors(header: str) -> tuple[dict[str, dict], dict[str, dict]]:
    _reject_conditionals(header, "TextCatalog descriptors")
    scalar_region = _descriptor_array_region(header, "TextKeys")
    table_region = _descriptor_array_region(header, "TextTables")
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
    scalar_clean = comments_blanked(scalar_region)
    scalar_active = lexical_mask(scalar_region)
    scalars = {}
    for match in scalar_pattern.finditer(scalar_clean):
        if scalar_active[match.start()] != "{":
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
    table_clean = comments_blanked(table_region)
    table_active = lexical_mask(table_region)
    tables = {}
    for match in table_pattern.finditer(table_clean):
        if table_active[match.start()] != "{":
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
    _validate_export_source_preprocessor(source)
    raw_body = extract_function_body(source, "i18n::ExportSelectedCatalog")
    raw_masked = lexical_mask(raw_body)
    _masked, body_directives = _directive_matches(
        raw_body, "i18n::ExportSelectedCatalog"
    )
    has_limit_contract = 'ExportStringLimitContract.inc' in raw_body
    if has_limit_contract:
        body_directives = _validated_export_body_directives(raw_body)
    elif body_directives:
        raise ExportSchemaError(
            "i18n::ExportSelectedCatalog may not hide export calls behind "
            "preprocessor directives"
        )
    body_without_directives = list(comments_blanked(raw_body))
    for directive in body_directives:
        _blank_span(
            body_without_directives,
            raw_body,
            directive.start(),
            directive.end(),
        )
    body = "".join(body_without_directives)
    active = lexical_mask(body)
    delimiter_depths: list[tuple[int, int, int]] = []
    paren_depth = bracket_depth = brace_depth = 0
    for character in active:
        delimiter_depths.append((paren_depth, bracket_depth, brace_depth))
        if character == "(":
            paren_depth += 1
        elif character == ")":
            paren_depth -= 1
        elif character == "[":
            bracket_depth += 1
        elif character == "]":
            bracket_depth -= 1
        elif character == "{":
            brace_depth += 1
        elif character == "}":
            brace_depth -= 1
        if min(paren_depth, bracket_depth, brace_depth) < 0:
            raise ExportSchemaError(
                "unbalanced delimiter in i18n::ExportSelectedCatalog"
            )
    if (paren_depth, bracket_depth, brace_depth) != (0, 0, 0):
        raise ExportSchemaError(
            "unbalanced delimiter in i18n::ExportSelectedCatalog"
        )
    call_pattern = re.compile(
        r"\b(?P<name>ExportSection|ExportTextPackEntry|ExportTextPackTable)\s*\("
    )
    calls = []
    for match in call_pattern.finditer(active):
        if delimiter_depths[match.start()] != (0, 0, 0):
            raise ExportSchemaError(
                f"{match.group('name')} must be a direct top-level statement"
            )
        prefix = active[:match.start()].rstrip()
        if prefix and prefix[-1] not in ";}":
            raise ExportSchemaError(
                f"{match.group('name')} must be a direct top-level statement"
            )
        opening = active.find("(", match.start())
        closing = _matching_delimiter(active, opening, "(", ")")
        trailing = active[closing + 1:]
        semicolon = len(trailing) - len(trailing.lstrip())
        if trailing[semicolon:semicolon + 1] != ";":
            raise ExportSchemaError(
                f"{match.group('name')} must be a direct top-level statement"
            )
        arguments = _split_arguments(body[opening + 1:closing])
        name = match.group("name")
        if name == "ExportSection":
            if len(arguments) != 5 or arguments[0] != "sink":
                raise ExportSchemaError("unsupported legacy ExportSection call shape")
            section_match = re.fullmatch(r'L"([^"\\]*)"', arguments[1])
            symbol_match = re.fullmatch(r"::([A-Za-z_]\w*)", arguments[2])
            if not section_match or not symbol_match:
                raise ExportSchemaError(
                    "legacy export section/symbol is not a direct literal/linked global"
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
        if len(arguments) != 2 or arguments[0] != "sink":
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
    if has_limit_contract:
        _validate_named_limit_usage_provenance(raw_body, calls)
        _validate_export_statement_inventory(raw_body, calls, body_directives)
    return calls


def _textual_catalog_includes(source: str) -> list[str]:
    include_pattern = re.compile(
        r'^[ \t]*#[ \t]*include[ \t]+"(_(?:Chinese|Dutch|English|French|German|'
        r'Italian|Polish|Russian)Text[.]cpp)"',
        re.MULTILINE,
    )
    clean = comments_blanked(source)
    active = lexical_mask(source)
    return [
        match.group(1)
        for match in include_pattern.finditer(clean)
        if active[match.start():match.end()].lstrip().startswith("#")
    ]


def validate_property_container_exporter(source: str) -> None:
    """Pin the immediate-copy VFS writer and raw-export tail."""

    body = _active_function_body(
        source, "Loc::ExportStrings", "Loc::ExportStrings"
    )
    _reject_conditionals(body, "Loc::ExportStrings")
    statements = _export_top_level_statements(body, [])
    expected = [
        "vfs::PropertyContainer::TagMaptmap;",
        "vfs::PropertyContainerprops;",
        "PropertyContainerExportSinksink(props);",
        "i18n::ExportSelectedCatalog(sink);",
        'props.writeToXMLFile(L"Localization/GameStrings.xml",tmap);',
        'props.writeToIniFile(L"Localization/GameStrings.ini",true);',
        "Loc::ExportMercBio();",
        "Loc::ExportAIMHistory();",
        "Loc::ExportAIMPolicy();",
        "Loc::ExportAlumniName();",
        "Loc::ExportDialogues();",
        "Loc::ExportNPCDialogues();",
        "returntrue;",
    ]
    if statements != expected:
        raise ExportSchemaError(
            "Loc::ExportStrings lost its adapter/write/six-EDT/final-return inventory"
        )

    copy_body = _active_function_body(
        source, "copyEntry", "PropertyContainerExportSink::copyEntry"
    )
    copy_statement = normalize_expression(comments_blanked(copy_body))
    expected_copy = normalize_expression(
        """props_.setStringProperty(
            vfs::String(std::wstring(section)),
            vfs::toString<wchar_t>(index),
            vfs::String(std::wstring(text)));"""
    )
    if copy_statement != expected_copy:
        raise ExportSchemaError(
            "PropertyContainerExportSink must immediately materialize both borrowed views"
        )


def _validate_adapter_helper_namespace(source: str) -> None:
    """Pin the live helper definitions, not text that merely resembles them."""

    masked = lexical_mask(source)
    namespaces = list(re.finditer(r"\bnamespace[ \t\r\n]*\{", masked))
    if len(namespaces) != 1:
        raise ExportSchemaError(
            "selected-catalog adapter must retain one exact anonymous helper namespace"
        )
    namespace_start = namespaces[0].start()
    opening = masked.find("{", namespace_start, namespaces[0].end())
    closing = _matching_delimiter(masked, opening, "{", "}")
    context = "selected-catalog helper namespace"
    if _conditional_depth(
        _structural_directive_matches(source[:namespace_start], context), context
    ) != 0:
        raise ExportSchemaError(
            "selected-catalog helper namespace must be unconditional"
        )
    region = source[namespace_start:closing + 1]
    _masked, directives = _directive_matches(region, context)
    if directives:
        raise ExportSchemaError(
            "selected-catalog helper namespace may not contain directives"
        )
    if normalize_expression(lexical_mask(region)) != normalize_expression(
        lexical_mask(EXPECTED_ADAPTER_HELPER_NAMESPACE)
    ):
        raise ExportSchemaError(
            "selected-catalog helper definitions changed storage, range, "
            "empty-suppression, or borrowed-view behavior"
        )

    export_start, _export_opening, export_closing = _function_definition_span(
        source, "i18n::ExportSelectedCatalog"
    )
    residual = list(source)
    _blank_span(residual, source, namespace_start, closing + 1)
    _blank_span(residual, source, export_start, export_closing + 1)
    unexpected = re.search(
        r"\b(?:ExportSection|ExportTextPackEntry|ExportTextPackTable)\b",
        lexical_mask("".join(residual)),
    )
    if unexpected is not None:
        raise ExportSchemaError(
            "selected-catalog helper name escaped its exact namespace/export body"
        )


def _validate_adapter_header(source: str) -> None:
    """Require one unconditional, data-free immediate-copy sink interface."""

    _masked, directives = _directive_matches(source, "SelectedCatalogExport.h")
    clean = comments_blanked(source)
    actual_directives = [
        clean[directive.start():directive.end()].strip()
        for directive in directives
    ]
    if actual_directives != ["#pragma once", "#include <string_view>"]:
        raise ExportSchemaError(
            "SelectedCatalogExport.h must retain its exact pragma/include prefix"
        )
    without_directives = list(source)
    for directive in directives:
        _blank_span(
            without_directives, source, directive.start(), directive.end()
        )
    expected = r"""
namespace i18n
{
class SelectedCatalogExportSink
{
public:
    virtual ~SelectedCatalogExportSink() = default;
    virtual void copyEntry(std::wstring_view section, int index,
        std::wstring_view text) = 0;
};
void ExportSelectedCatalog(SelectedCatalogExportSink& sink);
}
"""
    if normalize_expression(lexical_mask("".join(without_directives))) != (
        normalize_expression(lexical_mask(expected))
    ):
        raise ExportSchemaError(
            "SelectedCatalogExport.h lost its exact sink/entrypoint interface"
        )
    if source.count("must copy section and text before copyEntry returns") != 1:
        raise ExportSchemaError(
            "selected-catalog sink must document immediate ownership of both views"
        )


def _cmake_comments_removed(source: str) -> str:
    """Remove the comment forms relevant to reviewed CMake source lists."""

    source = re.sub(r"#\[(?P<equals>=*)\[.*?\](?P=equals)\]", "", source,
                    flags=re.DOTALL)
    return re.sub(r"(?m)#.*$", "", source)


def _validate_variant_source_list(build_source: str) -> None:
    """Require direct, ordered variant sources rather than comment matches."""

    clean = _cmake_comments_removed(build_source)
    blocks = list(re.finditer(
        r"set\s*\(\s*i18nVariantSrc(?P<body>.*?)PARENT_SCOPE\s*\)",
        clean,
        re.DOTALL,
    ))
    if len(blocks) != 1:
        raise ExportSchemaError("i18n variant source partition is not parseable")
    body = blocks[0].group("body")
    entry_pattern = re.compile(
        r'"\$\{CMAKE_CURRENT_SOURCE_DIR\}/(?P<source>[^"$]+)"'
    )
    entries = [match.group("source") for match in entry_pattern.finditer(body)]
    residual = entry_pattern.sub("", body)
    if residual.strip() or entries != EXPECTED_VARIANT_SOURCES:
        raise ExportSchemaError(
            "i18nVariantSrc must retain its exact direct ordered source list"
        )
    if clean.count("SelectedCatalogExport.cpp") != 1:
        raise ExportSchemaError(
            "SelectedCatalogExport.cpp must occur only in i18nVariantSrc"
        )


def _validate_compiled_language_agreement() -> None:
    """Pin the common compile-time selector used by globals and TextPack."""

    language_source = _read("i18n/language.cpp")
    _reject_conditionals(language_source, "i18n/language.cpp")
    active_language = lexical_mask(language_source)
    identity = "const i18n::Lang g_lang{i18n::CompiledDefaultLanguage()};"
    identity_matches = list(re.finditer(re.escape(identity), active_language))
    if len(identity_matches) != 1:
        raise ExportSchemaError(
            "compiled TextPack lost its exact immutable g_lang definition"
        )
    if active_language[:identity_matches[0].start()].count("{") != (
        active_language[:identity_matches[0].start()].count("}")
    ):
        raise ExportSchemaError("g_lang must remain a top-level definition")

    pack_definition = re.compile(
        r"\bauto[ \t\r\n]+i18n::GetCompiledTextPack\(\)[ \t\r\n]+"
        r"noexcept[ \t\r\n]*->[ \t\r\n]*const[ \t\r\n]+TextPack&"
        r"[ \t\r\n]*\{"
    )
    pack_matches = list(pack_definition.finditer(active_language))
    if len(pack_matches) != 1:
        raise ExportSchemaError(
            "compiled TextPack accessor must retain one exact definition"
        )
    pack_opening = active_language.find(
        "{", pack_matches[0].start(), pack_matches[0].end()
    )
    pack_closing = _matching_delimiter(
        active_language, pack_opening, "{", "}"
    )
    pack_body = language_source[pack_opening + 1:pack_closing]
    expected_pack_body = r"""
static const TextPack pack = [] {
    auto selected = BuiltinTextCatalog().select(g_lang);
    if (!selected) std::abort();
    return std::move(*selected);
}();
return pack;
"""
    if normalize_expression(lexical_mask(pack_body)) != normalize_expression(
        lexical_mask(expected_pack_body)
    ):
        raise ExportSchemaError(
            "compiled TextPack no longer selects exactly once from immutable g_lang"
        )

    compiled_language = _read("i18n/CompiledLanguage.h")
    compiled_body = _active_function_body(
        compiled_language,
        "CompiledDefaultLanguage",
        "i18n::CompiledDefaultLanguage",
    )
    expected_compiled_body = r"""
#if defined(ENGLISH)
  return Lang::en;
#elif defined(CHINESE)
  return Lang::zh;
#elif defined(DUTCH)
  return Lang::nl;
#elif defined(FRENCH)
  return Lang::fr;
#elif defined(GERMAN)
  return Lang::de;
#elif defined(ITALIAN)
  return Lang::it;
#elif defined(POLISH)
  return Lang::pl;
#elif defined(RUSSIAN)
  return Lang::ru;
#endif
"""
    if normalize_expression(comments_blanked(compiled_body)) != (
        normalize_expression(comments_blanked(expected_compiled_body))
    ):
        raise ExportSchemaError(
            "compiled language macro-to-Lang mapping changed or became ambiguous"
        )


def selected_catalog_adapter_contract(
    adapter_source: str,
    property_source: str,
    adapter_header: str,
    build_source: str,
) -> dict:
    """Validate linkage, sink lifetime, variant ownership, and no textual copy."""

    if _textual_catalog_includes(adapter_source + "\n" + property_source):
        raise ExportSchemaError(
            "selected-catalog adapter may not textually include a language body"
        )
    combined_masked = lexical_mask(adapter_source + "\n" + property_source)
    language_macros = [language.macro for language in ABI.LANGUAGES]
    escaped_macros = [
        macro
        for macro in language_macros
        if re.search(rf"\b{re.escape(macro)}\b", combined_masked)
    ]
    if escaped_macros:
        raise ExportSchemaError(
            "export adapter/writer regained language selection macro(s): "
            + ", ".join(escaped_macros)
        )

    _validate_adapter_helper_namespace(adapter_source)

    first_namespace = adapter_source.find("namespace")
    if first_namespace < 0:
        raise ExportSchemaError("selected-catalog adapter lost its helper namespace")
    externs = re.findall(
        r"^[ \t]*extern[ \t]+STR16[ \t]+([A-Za-z_]\w*)\[\][ \t]*;[ \t]*$",
        lexical_mask(adapter_source[:first_namespace]),
        re.MULTILINE,
    )
    if externs != EXPECTED_LOCAL_EXTERN_SYMBOLS:
        raise ExportSchemaError(
            "selected-catalog adapter must own the exact five local externs"
        )

    _validate_adapter_header(adapter_header)
    _validate_variant_source_list(build_source)

    root_build = _cmake_comments_removed(_read("CMakeLists.txt"))
    for fragment in (
        "target_sources(${language_library} PRIVATE\n      ${i18nVariantSrc}",
        "target_compile_definitions(${language_library} PRIVATE "
        "${compilationFlags} ${debugFlags} ${lang})",
    ):
        if fragment not in root_build:
            raise ExportSchemaError(
                "language globals, adapter, and g_lang lost their shared variant target"
            )
    _validate_compiled_language_agreement()
    if "i18n::GetCompiledTextPack()" not in adapter_source:
        raise ExportSchemaError(
            "selected linked globals and TextPack lost their shared adapter"
        )
    validate_property_container_exporter(property_source)
    return {
        "source": EXPORT_SOURCE,
        "entrypoint": "i18n::ExportSelectedCatalog(SelectedCatalogExportSink&)",
        "sink_header": EXPORT_HEADER,
        "sink_lifetime": "borrowed views copied before callback return",
        "property_writer": PROPERTY_EXPORT_SOURCE,
        "linked_catalog_sources": EXPECTED_LINKED_CATALOG_SOURCES,
        "textual_catalog_includes": [],
        "local_extern_symbols": EXPECTED_LOCAL_EXTERN_SYMBOLS,
        "variant_axes": ["language", "campaign", "build"],
        "language_agreement": (
            "one language-target definition selects linked globals, g_lang, and TextPack"
        ),
    }


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


def _validate_legacy_storage_paths(
    sections: Sequence[Mapping], abi_schema: Mapping
) -> None:
    """Keep every legacy range on one of the adapter's three safe shapes."""

    for section in sections:
        if section["source_kind"] != "legacy":
            continue
        symbol_name = section["symbol"]
        symbol = abi_schema["symbols"].get(symbol_name)
        if symbol is None:
            raise ExportSchemaError(
                f"legacy export symbol is absent from ABI schema: {symbol_name}"
            )
        storage_type = symbol["type"]
        rank = len(symbol["source_dimensions"])
        label = f"{section['section']}/{symbol_name}"
        if storage_type == "STR16" and rank == 1:
            continue
        if storage_type == "CHAR16" and rank == 2:
            continue
        if storage_type == "CHAR16" and rank == 1:
            first = _resolve_export_limit(str(section["range"]["first"]))
            limit = _resolve_export_limit(str(section["range"]["limit"]))
            if (first, limit) == (0, 1):
                continue
            raise ExportSchemaError(
                f"{label}: scalar wchar storage must export exactly [0,1)"
            )
        raise ExportSchemaError(
            f"{label}: unsupported adapter storage shape {storage_type} rank {rank}"
        )


def _resolve_export_limit(expression: str) -> int | None:
    """Evaluate the intentionally small exact integer range grammar."""

    normalized = normalize_expression(expression)
    if re.fullmatch(r"\d+", normalized):
        return int(normalized)
    match = re.fullmatch(
        r"(?P<name>[A-Za-z_]\w*)(?:(?P<operator>[+-])(?P<delta>\d+))?",
        normalized,
    )
    if match is None:
        return None
    value = NAMED_EXPORT_LIMITS.get(match.group("name"))
    if value is None:
        return None
    delta = int(match.group("delta") or 0)
    if match.group("operator") == "-":
        value -= delta
    elif match.group("operator") == "+":
        value += delta
    return value if value >= 0 else None


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
            raw_entries = next(iter(unique_dimensions))
            resolved_entries = _resolve_export_limit(str(raw_entries))
            if resolved_entries is None:
                raise ExportSchemaError(
                    f"cannot compare {language.name}/{symbol} dimension "
                    f"{raw_entries!r} with {export_limit}"
                )
            actual_entries = resolved_entries
            unsafe = actual_entries < export_limit
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
    counts = [body.count(token) for token in tokens]
    positions = [body.find(token) for token in tokens]
    if (
        counts != [1] * len(tokens)
        or any(position < 0 for position in positions)
        or positions != sorted(positions)
    ):
        raise ExportSchemaError(
            f"{context} lost exact required order/occurrence: " + " -> ".join(tokens)
        )


def _condition_free_function_body(
    source: str, qualified_name: str, context: str
) -> str:
    body = _active_function_body(source, qualified_name, context)
    _masked, directives = _directive_matches(body, context)
    if directives:
        raise ExportSchemaError(
            f"{context} may not contain preprocessor directives"
        )
    return body


def _reject_conditionally_nested_tokens(
    source: str, tokens: Sequence[str], context: str
) -> None:
    masked, directives = _directive_matches(source, context)
    allowed = {"if", "ifdef", "ifndef", "elif", "else", "endif"}
    unreviewed = next(
        (item for item in directives if item.group("name") not in allowed), None
    )
    if unreviewed is not None:
        raise ExportSchemaError(
            f"{context}: unreviewed preprocessor directive "
            f"#{unreviewed.group('name')}"
        )
    depth = 0
    conditional_source = []
    for line_number, line in enumerate(masked.splitlines(), start=1):
        directive = re.match(
            r"^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b", line
        )
        if directive:
            kind = directive.group(1)
            if kind in {"if", "ifdef", "ifndef"}:
                depth += 1
            elif kind == "endif":
                depth -= 1
                if depth < 0:
                    raise ExportSchemaError(f"{context}: unmatched #endif")
            continue
        if depth:
            conditional_source.append(line)
    if depth:
        raise ExportSchemaError(f"{context}: unterminated preprocessor conditional")
    conditional = normalize_expression("\n".join(conditional_source))
    nested = next(
        (
            token
            for token in tokens
            if normalize_expression(token) in conditional
        ),
        None,
    )
    if nested is not None:
        raise ExportSchemaError(
            f"{context}: reviewed startup token is conditional: {nested}"
        )


def _reject_direct_control_transfer_before(
    masked: str, limit: int, context: str
) -> None:
    depths = {"(": 0, "[": 0, "{": 0}
    closing = {")": "(", "]": "[", "}": "{"}
    statement_start = 0
    transfer = re.compile(r"\b(?:return|goto|throw|co_return)\b")
    transfer_starts = {match.start() for match in transfer.finditer(masked[:limit])}
    for index, character in enumerate(masked[:limit]):
        if index in transfer_starts and not any(depths.values()):
            if not masked[statement_start:index].strip():
                raise ExportSchemaError(
                    f"{context}: direct control transfer precedes reviewed call"
                )
        if character in depths:
            depths[character] += 1
        elif character in closing:
            opener = closing[character]
            depths[opener] -= 1
            if depths[opener] < 0:
                raise ExportSchemaError(f"{context}: unbalanced delimiter")
            if character == "}" and not any(depths.values()):
                statement_start = index + 1
        elif character == ";" and not any(depths.values()):
            statement_start = index + 1


def _require_direct_statement(body: str, token: str, context: str) -> None:
    masked = lexical_mask(body)
    matches = list(re.finditer(re.escape(token), masked))
    if len(matches) != 1:
        raise ExportSchemaError(
            f"{context} must contain exactly one direct {token} statement"
        )
    match = matches[0]
    _reject_direct_control_transfer_before(masked, match.start(), context)
    brace_depth = 0
    for character in masked[:match.start()]:
        if character == "{":
            brace_depth += 1
        elif character == "}":
            brace_depth -= 1
    prefix = masked[:match.start()].rstrip()
    opening = masked.find("(", match.start(), match.end() + 2)
    if opening < 0:
        raise ExportSchemaError(f"{context}: {token} is not a call")
    closing = _matching_delimiter(masked, opening, "(", ")")
    trailing = masked[closing + 1:]
    semicolon = len(trailing) - len(trailing.lstrip())
    if (
        brace_depth != 0
        or (prefix and prefix[-1] not in ";}")
        or trailing[semicolon:semicolon + 1] != ";"
    ):
        raise ExportSchemaError(f"{context}: {token} is not a direct statement")


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
        _condition_free_function_body(
            sgp_source,
            "InitializeLegacyContentBoundary",
            "legacy localization boundary",
        )
    )
    boundary_mask = ABI.lexical_mask(boundary)
    xml_guard = re.search(r"\bif\s*\(\s*g_bUseXML_Strings\s*\)\s*\{", boundary_mask)
    if not xml_guard:
        raise ExportSchemaError("legacy content lost USE_XML_STRINGS export/import guard")
    _reject_direct_control_transfer_before(
        boundary_mask, xml_guard.start(), "legacy localization boundary"
    )
    xml_open = boundary_mask.find("{", xml_guard.start())
    xml_close = _matching_delimiter(boundary_mask, xml_open, "{", "}")
    xml_body = boundary_mask[xml_open + 1:xml_close]
    if normalize_expression(xml_body) != (
        "if(s_bExportStrings)Loc::ExportStrings();Loc::ImportStrings();"
    ):
        raise ExportSchemaError(
            "legacy localization boundary must remain the exact startup-only "
            "export-then-import body"
        )
    _ordered_tokens(
        xml_body,
        ["if(s_bExportStrings) Loc::ExportStrings();", "Loc::ImportStrings();"],
        "legacy localization boundary",
    )
    splash_position = boundary_mask.find("InitJA2SplashScreen();")
    if splash_position < xml_close:
        raise ExportSchemaError("splash initialization moved before localization export/import")
    _reject_direct_control_transfer_before(
        boundary_mask, splash_position, "legacy localization boundary"
    )

    runtime_body = comments_blanked(
        _active_function_body(
            sgp_source,
            "GetStandardGamingPlatformRuntime",
            "standard gaming platform runtime",
        )
    )
    _reject_conditionally_nested_tokens(
        runtime_body,
        ["InitializeLegacyContentBoundary", "InitializeGameBoundary"],
        "standard gaming platform subsystem order",
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
    if (
        subsystem_names.count("legacy content") != 1
        or subsystem_names.count("game") != 1
    ):
        raise ExportSchemaError(
            "legacy content/game subsystem definitions must each occur exactly once"
        )
    _reject_direct_control_transfer_before(
        runtime_mask,
        subsystem_matches[0].start(),
        "standard gaming platform subsystem order",
    )
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
        _condition_free_function_body(
            sgp_source, "InitializeGameBoundary", "game startup boundary"
        )
    )
    if normalize_expression(game_boundary).count("InitializeGame()") != 1:
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

    compiled_raw = _condition_free_function_body(
        _read("Ja2/CompiledGameplayBootstrap.cpp"),
        "loadRulesContent",
        "compiled rules content load",
    )
    compiled = lexical_mask(compiled_raw)
    _ordered_tokens(
        normalize_expression(compiled),
        [
            "LoadExternalGameplayData(TABLEDATA_DIRECTORY,false)",
            "LoadAllExternalText()",
        ],
        "compiled rules content load",
    )
    macro = re.compile(r"\bSGP_TRYCATCH_RETHROW\s*\(")
    macro_matches = list(macro.finditer(lexical_mask(compiled_raw)))
    if len(macro_matches) != 1:
        raise ExportSchemaError(
            "compiled rules content load lost its direct load wrapper"
        )
    _require_direct_statement(
        compiled_raw, "SGP_TRYCATCH_RETHROW", "compiled rules content load"
    )
    macro_open = lexical_mask(compiled_raw).find("(", macro_matches[0].start())
    macro_close = _matching_delimiter(
        lexical_mask(compiled_raw), macro_open, "(", ")"
    )
    macro_arguments = _split_arguments(compiled_raw[macro_open + 1:macro_close])
    if (
        len(macro_arguments) != 2
        or normalize_expression(lexical_mask(macro_arguments[0]))
        != "loaded=LoadExternalGameplayData(TABLEDATA_DIRECTORY,false)"
    ):
        raise ExportSchemaError(
            "compiled rules content load must directly assign the reviewed data load"
        )
    _require_direct_statement(
        compiled_raw, "LoadAllExternalText()", "compiled rules content load"
    )
    rules_raw = _condition_free_function_body(
        _read("Ja2/RulesPackage.cpp"),
        "LegacyRulesPackage::bootstrap",
        "rules-package load phase",
    )
    rules = lexical_mask(rules_raw)
    _ordered_tokens(
        normalize_expression(rules),
        ["PackageBootstrapPhase::LoadContent", "bootstrapHost_.loadRulesContent(capabilities_)"],
        "rules-package load phase",
    )
    load_phase = rules.find("PackageBootstrapPhase::LoadContent")
    _reject_direct_control_transfer_before(
        rules, load_phase, "rules-package load phase"
    )
    expected_rules_body = (
        "switch(phase){casePackageBootstrapPhase::Configure:"
        "casePackageBootstrapPhase::StartRuntime:returntrue;"
        "casePackageBootstrapPhase::LoadContent:"
        "if(contentLoaded_)returntrue;"
        "if(contentLoadAttempted_)returnfalse;"
        "contentLoadAttempted_=true;"
        "if(!bootstrapHost_.loadRulesContent(capabilities_))returnfalse;"
        "contentLoaded_=true;returntrue;}returnfalse;"
    )
    if normalize_expression(rules) != expected_rules_body:
        raise ExportSchemaError(
            "rules-package LoadContent phase lost its exact host-load condition/flow"
        )
    initialize_ja2_raw = _active_function_body(
        _read("Ja2/Init.cpp"), "InitializeJA2", "InitializeJA2 content phase"
    )
    _reject_conditionally_nested_tokens(
        initialize_ja2_raw,
        ["advancePackagesTo(PackageBootstrapPhase::LoadContent)"],
        "InitializeJA2 content phase",
    )
    initialize_ja2 = normalize_expression(lexical_mask(initialize_ja2_raw))
    if initialize_ja2.count(
        "advancePackagesTo(PackageBootstrapPhase::LoadContent)"
    ) != 1:
        raise ExportSchemaError("InitializeJA2 lost the post-startup LoadContent phase")
    initialize_ja2_masked = lexical_mask(initialize_ja2_raw)
    advance_position = initialize_ja2_masked.find(
        "gameContext.advancePackagesTo(PackageBootstrapPhase::LoadContent)"
    )
    _reject_direct_control_transfer_before(
        initialize_ja2_masked, advance_position, "InitializeJA2 content phase"
    )
    content_load_statement = re.compile(
        r"\bconst\s+RuntimeSessionAdvanceResult\s+contentLoad\s*=\s*"
        r"gameContext[.]advancePackagesTo\s*\(\s*"
        r"PackageBootstrapPhase::LoadContent\s*\)\s*;"
    )
    content_load_matches = list(content_load_statement.finditer(initialize_ja2_masked))
    if len(content_load_matches) != 1:
        raise ExportSchemaError(
            "InitializeJA2 must directly initialize contentLoad from LoadContent"
        )
    brace_depth = 0
    for character in initialize_ja2_masked[:content_load_matches[0].start()]:
        if character == "{":
            brace_depth += 1
        elif character == "}":
            brace_depth -= 1
    if brace_depth != 0:
        raise ExportSchemaError(
            "InitializeJA2 LoadContent initialization must be a top-level statement"
        )
    if (
        initialize_ja2.count("gameContext.advancePackagesTo(") != 2
        or initialize_ja2.count(
            "gameContext.advancePackagesTo(PackageBootstrapPhase::StartRuntime)"
        ) != 1
    ):
        raise ExportSchemaError(
            "InitializeJA2 changed its reviewed package-advance call inventory"
        )
    multiplayer_raw = _condition_free_function_body(
        _read("Ja2/MPConnectScreen.cpp"),
        "DoneFadeOutForExitMPCScreen",
        "multiplayer reload",
    )
    multiplayer = normalize_expression(lexical_mask(multiplayer_raw))
    _ordered_tokens(
        multiplayer,
        [
            "LoadExternalGameplayData(TABLEDATA_DIRECTORY,true)",
            "LoadAllExternalText()",
        ],
        "multiplayer reload",
    )
    _require_direct_statement(
        multiplayer_raw,
        "LoadExternalGameplayData(TABLEDATA_DIRECTORY, true)",
        "multiplayer reload",
    )
    _require_direct_statement(
        multiplayer_raw, "LoadAllExternalText()", "multiplayer reload"
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


WIDE_STRING_LITERAL = re.compile(r'\bL"(?P<body>(?:\\.|[^"\\])*)"')


def _initializer_entry_sources(
    source: str,
    source_name: str,
    definitions: Mapping[str, Mapping],
    symbol: str,
    *,
    masked: str | None = None,
) -> list[str]:
    """Return top-level initializer entries without compiling the catalog."""

    definition = definitions.get(symbol)
    if definition is None:
        raise ExportSchemaError(f"{source_name}: missing normalized symbol {symbol}")
    masked = ABI.lexical_mask(source) if masked is None else masked
    if len(masked) != len(source):
        raise ExportSchemaError(
            f"{source_name}/{symbol}: lexical mask changed source length"
        )
    opening = masked.find(
        "{",
        definition["raw_start"],
        definition["raw_end"],
    )
    if opening < 0:
        raise ExportSchemaError(f"{source_name}/{symbol}: initializer is not braced")
    closing = ABI._matching_brace(masked, opening)
    body = source[opening + 1:closing]
    body_mask = masked[opening + 1:closing]

    entries: list[str] = []
    start = 0
    delimiter_depth = {"(": 0, "[": 0, "{": 0}
    matching_open = {")": "(", "]": "[", "}": "{"}
    for index, character in enumerate(body_mask):
        if character in delimiter_depth:
            delimiter_depth[character] += 1
        elif character in matching_open:
            opener = matching_open[character]
            delimiter_depth[opener] -= 1
            if delimiter_depth[opener] < 0:
                raise ExportSchemaError(
                    f"{source_name}/{symbol}: unbalanced initializer delimiter"
                )
        elif character == "," and not any(delimiter_depth.values()):
            if body_mask[start:index].strip():
                entries.append(body[start:index])
            start = index + 1
    if any(delimiter_depth.values()):
        raise ExportSchemaError(
            f"{source_name}/{symbol}: unbalanced nested initializer"
        )
    if body_mask[start:].strip():
        entries.append(body[start:])
    return entries


def _single_wide_literal(entry: str, label: str) -> str:
    literals = list(WIDE_STRING_LITERAL.finditer(comments_blanked(entry)))
    if len(literals) != 1:
        raise ExportSchemaError(
            f"{label}: expected exactly one wide-string literal, found {len(literals)}"
        )
    return literals[0].group("body")


DIRECT_WIDE_LITERAL_SEQUENCE = re.compile(
    r'(?:L"(?:\\.|[^"\\])*"\s*)+'
)
COMPILED_SELECTOR_START = re.compile(
    r"I18N_COMPILED_(?:BUILD|CAMPAIGN)_TEXT\s*\("
)


def _pointer_initializer_kind(entry: str, label: str) -> str:
    """Classify one exported STR16 slot or reject executable/null expressions."""

    clean = comments_blanked(entry).strip()
    if DIRECT_WIDE_LITERAL_SEQUENCE.fullmatch(clean):
        return "direct_wide_literal"
    masked = lexical_mask(clean)
    selector = COMPILED_SELECTOR_START.match(masked)
    if selector is not None:
        opening = masked.find("(", selector.start(), selector.end())
        closing = _matching_delimiter(masked, opening, "(", ")")
        if not masked[closing + 1:].strip():
            arguments = _split_arguments(clean[opening + 1:closing])
            if (
                len(arguments) == 3
                and re.fullmatch(r"[A-Za-z_]\w*", arguments[0])
                and DIRECT_WIDE_LITERAL_SEQUENCE.fullmatch(
                    comments_blanked(arguments[1]).strip()
                )
                and DIRECT_WIDE_LITERAL_SEQUENCE.fullmatch(
                    comments_blanked(arguments[2]).strip()
                )
            ):
                return "compiled_selector"
    raise ExportSchemaError(
        f"{label}: exported STR16 initializer must be a direct wide-literal "
        "sequence or an exact compiled-text selector"
    )


def _decode_wide_literal_body(body: str, label: str) -> str:
    """Decode the deliberately narrow escape grammar used by text catalogs."""

    escapes = {"n": "\n", '"': '"', "'": "'", "\\": "\\"}
    result = []
    index = 0
    while index < len(body):
        if body[index] != "\\":
            result.append(body[index])
            index += 1
            continue
        if index + 1 >= len(body) or body[index + 1] not in escapes:
            raise ExportSchemaError(
                f"{label}: unsupported wide-string escape in output model"
            )
        result.append(escapes[body[index + 1]])
        index += 2
    return "".join(result)


def _decode_wide_literal_sequence(expression: str, label: str) -> str:
    clean = comments_blanked(expression).strip()
    if not DIRECT_WIDE_LITERAL_SEQUENCE.fullmatch(clean):
        raise ExportSchemaError(
            f"{label}: output model requires a direct wide-literal sequence"
        )
    return "".join(
        _decode_wide_literal_body(match.group("body"), label)
        for match in WIDE_STRING_LITERAL.finditer(clean)
    )


def _selected_pointer_text(
    expression: str, quadrant: ABI.Quadrant, label: str
) -> str:
    kind = _pointer_initializer_kind(expression, label)
    if kind == "direct_wide_literal":
        return _decode_wide_literal_sequence(expression, label)
    clean = comments_blanked(expression).strip()
    masked = lexical_mask(clean)
    selector = COMPILED_SELECTOR_START.match(masked)
    assert selector is not None
    opening = masked.find("(", selector.start(), selector.end())
    closing = _matching_delimiter(masked, opening, "(", ")")
    arguments = _split_arguments(clean[opening + 1:closing])
    if "BUILD" in selector.group(0):
        selected = arguments[2] if "JA2BETAVERSION" in quadrant.macros else arguments[1]
    else:
        selected = arguments[2] if "JA2UB" in quadrant.macros else arguments[1]
    return _decode_wide_literal_sequence(selected, label)


def _builtin_text_pack_values(source: str) -> dict[str, tuple[list[str], list[str]]]:
    """Parse the eight literal-only BuiltinDefinitions rows."""

    _reject_conditionals(source, "built-in TextPack definitions")
    region = _descriptor_array_region(source, "BuiltinDefinitions")
    rows = _split_arguments(region)
    if rows and not rows[-1]:
        rows.pop()
    if len(rows) != len(ABI.LANGUAGES):
        raise ExportSchemaError(
            "built-in TextPack must contain exactly eight direct language rows"
        )

    def parse_literal_list(text: str, label: str, expected: int) -> list[str]:
        entries = _split_arguments(text)
        if entries and not entries[-1]:
            entries.pop()
        values = [
            _decode_wide_literal_sequence(entry, f"{label}[{index}]")
            for index, entry in enumerate(entries)
        ]
        if len(values) != expected:
            raise ExportSchemaError(
                f"{label}: expected {expected} direct wide-literal entries"
            )
        return values

    result = {}
    for row_index, row in enumerate(rows):
        masked = lexical_mask(row)
        prefix = re.match(
            r"\s*\{\s*Lang::(?P<language>en|de|ru|nl|pl|fr|it|zh)\s*,\s*",
            masked,
        )
        if prefix is None:
            raise ExportSchemaError(
                f"built-in TextPack row {row_index} lost its direct Lang key"
            )
        code = prefix.group("language")
        if code in result:
            raise ExportSchemaError(f"duplicate built-in TextPack row {code}")

        key_open = prefix.end()
        if key_open >= len(masked) or masked[key_open] != "{":
            raise ExportSchemaError(
                f"TextPack/{code}: scalar entries must use one direct braced list"
            )
        key_close = _matching_delimiter(masked, key_open, "{", "}")
        cursor = key_close + 1
        separator = re.match(r"\s*,\s*", masked[cursor:])
        if separator is None:
            raise ExportSchemaError(
                f"TextPack/{code}: scalar/table list separator changed"
            )
        table_open = cursor + separator.end()
        if table_open >= len(masked) or masked[table_open] != "{":
            raise ExportSchemaError(
                f"TextPack/{code}: table entries must use one direct braced list"
            )
        table_close = _matching_delimiter(masked, table_open, "{", "}")
        cursor = table_close + 1
        tail = re.fullmatch(r"\s*\}\s*", masked[cursor:])
        if tail is None:
            raise ExportSchemaError(
                f"TextPack/{code}: language row contains unreviewed syntax"
            )

        keys = parse_literal_list(
            row[key_open + 1:key_close],
            f"TextPack/{code}/keys",
            EXPECTED_TEXT_PACK_ENTRIES,
        )
        tables = parse_literal_list(
            row[table_open + 1:table_close],
            f"TextPack/{code}/tables",
            120,
        )
        result[code] = (keys, tables)
    expected_codes = ["en", "de", "ru", "nl", "pl", "fr", "it", "zh"]
    if list(result) != expected_codes:
        raise ExportSchemaError("built-in TextPack language order changed")
    return result


def ordered_output_contract(sections: Sequence[dict]) -> dict:
    """Hash every emitted section/index/value byte in all 32 variants."""

    text_header = _read(TEXT_CATALOG_HEADER)
    scalar_descriptors, table_descriptors = parse_text_pack_descriptors(text_header)
    scalar_order = list(scalar_descriptors)
    packs = _builtin_text_pack_values(_read("i18n/TextCatalog.cpp"))
    language_codes = ["en", "de", "ru", "nl", "pl", "fr", "it", "zh"]
    snapshots = {}
    for language, language_code in zip(ABI.LANGUAGES, language_codes):
        source = _read(language.base_source)
        key_values, table_values = packs[language_code]
        for quadrant in ABI.QUADRANTS:
            active = ABI.preprocess(source, {language.macro, *quadrant.macros})
            active_masked = ABI.lexical_mask(active)
            try:
                definitions = ABI.parse_definitions(
                    active,
                    f"{language.base_source}/{quadrant.name}",
                    masked=active_masked,
                )
            except ABI.SchemaError as error:
                raise ExportSchemaError(str(error)) from error
            records: list[tuple[str, int, str]] = []
            entry_cache: dict[str, list[str]] = {}
            for section in sections:
                first = _resolve_export_limit(str(section["range"]["first"]))
                limit = _resolve_export_limit(str(section["range"]["limit"]))
                if first is None or limit is None:
                    raise ExportSchemaError(
                        f"cannot resolve output range for {section['section']}"
                    )
                if section["source_kind"] == "legacy":
                    symbol = section["symbol"]
                    entries = entry_cache.get(symbol)
                    if entries is None:
                        entries = _initializer_entry_sources(
                            active,
                            f"{language.base_source}/{quadrant.name}",
                            definitions,
                            symbol,
                            masked=active_masked,
                        )
                        entry_cache[symbol] = entries
                    storage_type = definitions[symbol]["type"]
                    for index in range(first, limit):
                        label = f"{language.name}/{quadrant.name}/{symbol}[{index}]"
                        if index >= len(entries):
                            if storage_type == "CHAR16":
                                # Fixed buffers may have an implicit zero-filled
                                # tail; the legacy exporter suppresses it.
                                continue
                            raise ExportSchemaError(
                                f"{label}: pointer output reaches implicit storage"
                            )
                        text = (
                            _selected_pointer_text(entries[index], quadrant, label)
                            if storage_type == "STR16"
                            else _decode_wide_literal_sequence(entries[index], label)
                        )
                        if text:
                            records.append((section["section"], index, text))
                    continue
                if section["source_kind"] == "text-pack-entry":
                    text = key_values[scalar_order.index(section["key"])]
                    if text:
                        records.append((section["section"], 0, text))
                    continue
                descriptor = table_descriptors[section["key"]]
                offset = descriptor["offset"]
                for index in range(first, limit):
                    text = table_values[offset + index]
                    if text:
                        records.append((section["section"], index, text))

            digest = hashlib.sha256()
            value_bytes = 0
            for section_name, index, text in records:
                fields = (
                    section_name.encode("utf-8"),
                    str(index).encode("ascii"),
                    text.encode("utf-8"),
                )
                value_bytes += len(fields[2])
                for field in fields:
                    digest.update(len(field).to_bytes(8, "big"))
                    digest.update(field)
            snapshots[f"{language.name}/{quadrant.name}"] = {
                "emitted_sections": len({record[0] for record in records}),
                "emitted_entries": len(records),
                "value_utf8_bytes": value_bytes,
                "ordered_payload_sha256": digest.hexdigest(),
            }
    if len(snapshots) != len(ABI.LANGUAGES) * len(ABI.QUADRANTS):
        raise ExportSchemaError("ordered output contract lost a build quadrant")
    return {
        "encoding": "UTF-8",
        "record_framing": "u64be length + section, index, value",
        "empty_values": "suppressed",
        "indexing": "absolute source index",
        "snapshots": snapshots,
    }


def _catalog_preamble_includes(source: str) -> list[tuple[str, int]]:
    clean = comments_blanked(source)
    direct_quoted = re.compile(r'"([^"\\]+)"[ \t]*$')
    includes = []
    _masked, directives = _directive_matches(source, "catalog preamble")
    for directive in directives:
        if directive.group("name") != "include":
            continue
        raw = clean[directive.start():directive.end()]
        include = re.fullmatch(
            r'[ \t]*#[ \t]*include[ \t]+(?P<argument>[^\r\n]+)', raw
        )
        if include is None:
            raise ExportSchemaError("catalog include directive is malformed")
        argument = direct_quoted.fullmatch(include.group("argument").strip())
        if argument is None:
            raise ExportSchemaError(
                "catalog include must use a direct reviewed quoted path"
            )
        includes.append((argument.group(1), directive.start()))
    return includes


def _validate_catalog_selector_boundary(
    source: str, language: ABI.Language
) -> None:
    """Pin the include/macro seam that makes selector-shaped slots non-null."""

    source_name = language.base_source
    _masked, directives = _directive_matches(source, source_name)
    allowed_directives = {
        "if", "ifdef", "ifndef", "elif", "else", "endif", "include"
    }
    unreviewed = next(
        (item for item in directives if item.group("name") not in allowed_directives),
        None,
    )
    if unreviewed is not None:
        line = source.count("\n", 0, unreviewed.start()) + 1
        raise ExportSchemaError(
            f"{source_name}:{line}: unreviewed catalog preprocessor directive "
            f"#{unreviewed.group('name')}"
        )
    try:
        ABI.validate_catalog_conditionals(source, language, source_name)
    except ABI.SchemaError as error:
        raise ExportSchemaError(str(error)) from error

    selector_pattern = re.compile(
        r"\bI18N_COMPILED_(?:CAMPAIGN|BUILD)_TEXT\s*\("
    )

    def validate_one(selected_source: str, label: str) -> None:
        includes = _catalog_preamble_includes(selected_source)
        include_names = [name for name, _position in includes]
        if include_names != EXPECTED_CATALOG_PREAMBLE_INCLUDES:
            raise ExportSchemaError(
                f"{label}: catalog preamble includes changed: "
                f"expected {EXPECTED_CATALOG_PREAMBLE_INCLUDES!r}, "
                f"got {include_names!r}"
            )
        if include_names[-1] != "CompiledConditionalTextSelectors.inc":
            raise ExportSchemaError(
                f"{label}: the compiled selector seam must be the final preamble include"
            )
        selector_positions = [
            match.start()
            for match in selector_pattern.finditer(lexical_mask(selected_source))
        ]
        if not selector_positions:
            raise ExportSchemaError(f"{label}: compiled selectors disappeared")
        compiled_position = next(
            position
            for name, position in includes
            if name == "CompiledConditionalTextSelectors.inc"
        )
        if compiled_position >= min(selector_positions):
            raise ExportSchemaError(
                f"{label}: compiled selector seam must precede every selector"
            )

    validate_one(source, source_name)
    for quadrant in ABI.QUADRANTS:
        active_source = ABI.preprocess(
            source, {language.macro, *quadrant.macros}
        )
        validate_one(active_source, f"{source_name}/{quadrant.name}")


def _raw_catalog_snapshot(
    sections: Sequence[dict],
    catalog_sources: Mapping[str, str] | None = None,
) -> tuple[dict[tuple[str, str, str], int], dict[str, int]]:
    """Resolve raw dimensions and validate every exported pointer initializer.

    A wildcard array owns exactly its top-level initializer count. Explicit
    dimensions use the same fail-closed integer evaluator as export ranges.
    Declaration dimensions are deliberately irrelevant here: the variant
    adapter links the selected global definition, whose raw source shape is the
    storage actually enumerated.
    """

    legacy_sections = [
        section for section in sections if section["source_kind"] == "legacy"
    ]
    ranges = {}
    for section in legacy_sections:
        first = _resolve_export_limit(str(section["range"]["first"]))
        limit = _resolve_export_limit(str(section["range"]["limit"]))
        if first is None or limit is None:
            raise ExportSchemaError(
                f"cannot resolve exact export range for {section['section']}/"
                f"{section['symbol']}: [{section['range']['first']},"
                f"{section['range']['limit']})"
            )
        if first > limit:
            raise ExportSchemaError(
                f"invalid descending export range for {section['section']}/"
                f"{section['symbol']}: [{first},{limit})"
            )
        ranges[section["symbol"]] = (first, limit)

    dimensions: dict[tuple[str, str, str], int] = {}
    pointer_entry_checks = 0
    direct_literal_checks = 0
    compiled_selector_checks = 0
    for language in ABI.LANGUAGES:
        source = (
            catalog_sources[language.name]
            if catalog_sources is not None and language.name in catalog_sources
            else _read(language.base_source)
        )
        _validate_catalog_selector_boundary(source, language)
        for quadrant in ABI.QUADRANTS:
            selected = {language.macro, *quadrant.macros}
            active_source = ABI.preprocess(source, selected)
            active_masked = ABI.lexical_mask(active_source)
            try:
                definitions = ABI.parse_definitions(
                    active_source,
                    f"{language.base_source}/{quadrant.name}",
                    masked=active_masked,
                )
            except ABI.SchemaError as error:
                raise ExportSchemaError(str(error)) from error
            for section in legacy_sections:
                symbol = section["symbol"]
                definition = definitions.get(symbol)
                if definition is None:
                    raise ExportSchemaError(
                        f"{language.name}/{quadrant.name}: missing legacy export "
                        f"symbol {symbol}"
                    )
                source_dimensions = definition["source_dimensions"]
                if not source_dimensions:
                    raise ExportSchemaError(
                        f"{language.name}/{quadrant.name}/{symbol}: scalar storage "
                        "cannot back ExportSection"
                    )
                first_dimension = source_dimensions[0]
                if first_dimension == "*":
                    physical_entries = definition["initializer_entries"]
                else:
                    resolved = _resolve_export_limit(first_dimension)
                    if resolved is None:
                        raise ExportSchemaError(
                            f"{language.name}/{quadrant.name}/{symbol}: cannot "
                            f"resolve raw first dimension {first_dimension!r}"
                        )
                    physical_entries = resolved
                dimensions[(language.name, quadrant.name, symbol)] = physical_entries

                if definition["type"] != "STR16":
                    continue
                entries = _initializer_entry_sources(
                    active_source,
                    f"{language.base_source}/{quadrant.name}",
                    definitions,
                    symbol,
                    masked=active_masked,
                )
                first, limit = ranges[symbol]
                if limit > len(entries):
                    raise ExportSchemaError(
                        f"{language.name}/{quadrant.name}/{symbol}: exported "
                        f"pointer range [{first},{limit}) reaches implicit/null "
                        f"initialization after {len(entries)} explicit entries"
                    )
                for index in range(first, limit):
                    kind = _pointer_initializer_kind(
                        entries[index],
                        f"{language.name}/{quadrant.name}/{symbol}[{index}]",
                    )
                    pointer_entry_checks += 1
                    if kind == "direct_wide_literal":
                        direct_literal_checks += 1
                    else:
                        compiled_selector_checks += 1
    return dimensions, {
        "exported_pointer_entry_checks": pointer_entry_checks,
        "direct_wide_literal_entry_checks": direct_literal_checks,
        "compiled_selector_entry_checks": compiled_selector_checks,
    }


def _raw_catalog_dimensions(
    sections: Sequence[dict],
    catalog_sources: Mapping[str, str] | None = None,
) -> dict[tuple[str, str, str], int]:
    """Compatibility wrapper for callers that need only physical dimensions."""

    dimensions, _entry_evidence = _raw_catalog_snapshot(
        sections, catalog_sources
    )
    return dimensions


def summarize_range_safety(
    sections: Sequence[dict],
    dimensions: Mapping[tuple[str, str, str], int],
) -> tuple[dict, list[dict]]:
    """Summarize every legacy language/quadrant range comparison."""

    failures: list[dict] = []
    comparisons = 0
    pair_shortfalls: dict[tuple[str, str], int] = {}
    legacy_sections = [
        section for section in sections if section["source_kind"] == "legacy"
    ]
    for section in legacy_sections:
        first = _resolve_export_limit(str(section["range"]["first"]))
        limit = _resolve_export_limit(str(section["range"]["limit"]))
        if first is None or limit is None:
            raise ExportSchemaError(
                f"cannot resolve exact export range for {section['section']}/"
                f"{section['symbol']}: [{section['range']['first']},"
                f"{section['range']['limit']})"
            )
        if first > limit:
            raise ExportSchemaError(
                f"invalid descending export range for {section['section']}/"
                f"{section['symbol']}: [{first},{limit})"
            )
        for language in ABI.LANGUAGES:
            for quadrant in ABI.QUADRANTS:
                key = (language.name, quadrant.name, section["symbol"])
                if key not in dimensions:
                    raise ExportSchemaError(
                        "missing raw catalog dimension for " + "/".join(key)
                    )
                comparisons += 1
                physical_entries = dimensions[key]
                if limit <= physical_entries:
                    continue
                shortfall = limit - physical_entries
                failures.append(
                    {
                        "language": language.name,
                        "quadrant": quadrant.name,
                        "section": section["section"],
                        "symbol": section["symbol"],
                        "physical_entries": physical_entries,
                        "export_first": first,
                        "export_limit": limit,
                        "shortfall": shortfall,
                    }
                )
                pair = (language.name, section["symbol"])
                pair_shortfalls[pair] = max(pair_shortfalls.get(pair, 0), shortfall)
    summary = {
        "comparisons": comparisons,
        "unsafe_sections": len({entry["section"] for entry in failures}),
        "unsafe_language_pairs": len(pair_shortfalls),
        "unsafe_quadrant_failures": len(failures),
        "potential_oob_reads_per_selected_build": sum(pair_shortfalls.values()),
    }
    return summary, failures


def exhaustive_range_contract(
    sections: Sequence[dict],
    catalog_sources: Mapping[str, str] | None = None,
) -> dict:
    dimensions, entry_evidence = _raw_catalog_snapshot(
        sections, catalog_sources
    )
    summary, failures = summarize_range_safety(sections, dimensions)
    if summary["comparisons"] != EXPECTED_LEGACY_RANGE_COMPARISONS:
        raise ExportSchemaError(
            "legacy range comparison count changed: expected "
            f"{EXPECTED_LEGACY_RANGE_COMPARISONS}, got {summary['comparisons']}"
        )
    ceilings = {
        "unsafe_sections": MAX_UNSAFE_RANGE_SECTIONS,
        "unsafe_language_pairs": MAX_UNSAFE_RANGE_LANGUAGE_PAIRS,
        "unsafe_quadrant_failures": MAX_UNSAFE_RANGE_QUADRANT_FAILURES,
        "potential_oob_reads_per_selected_build": (
            MAX_POTENTIAL_OOB_READS_PER_SELECTED_BUILD
        ),
    }
    exceeded = {
        field: value
        for field, value in summary.items()
        if field in ceilings and value > ceilings[field]
    }
    if exceeded:
        first = failures[0]
        raise ExportSchemaError(
            "exhaustive raw catalog range gate failed: "
            + ", ".join(f"{field}={value}" for field, value in exceeded.items())
            + "; first failure "
            + "/".join(
                str(first[field])
                for field in ("language", "quadrant", "section", "symbol")
            )
            + f" has {first['physical_entries']} entries for "
            f"[0,{first['export_limit']})"
        )
    expected_entry_evidence = {
        "exported_pointer_entry_checks": EXPECTED_EXPORTED_POINTER_ENTRY_CHECKS,
        "direct_wide_literal_entry_checks": (
            EXPECTED_DIRECT_WIDE_LITERAL_ENTRY_CHECKS
        ),
        "compiled_selector_entry_checks": (
            EXPECTED_COMPILED_SELECTOR_ENTRY_CHECKS
        ),
    }
    if entry_evidence != expected_entry_evidence:
        raise ExportSchemaError(
            "exported STR16 initializer evidence changed: expected "
            f"{expected_entry_evidence!r}, got {entry_evidence!r}"
        )
    summary.update(entry_evidence)
    return summary


def normalized_range_issues(
    sections: Sequence[dict],
    abi_schema: Mapping,
    catalog_sources: Mapping[str, str] | None = None,
    check_exhaustive: bool = True,
) -> list[str]:
    """Prove exact repaired limits and the exhaustive raw range contract."""

    del abi_schema  # ABI storage debt is intentionally not a physical Loc bound.
    issues: list[str] = []
    legacy_by_symbol = {
        section["symbol"]: section
        for section in sections
        if section["source_kind"] == "legacy"
    }
    for symbol, expected_limit in EXPECTED_NORMALIZED_EXPORT_LIMITS.items():
        section = legacy_by_symbol.get(symbol)
        if section is None:
            issues.append(f"normalized export symbol is missing: {symbol}")
            continue
        actual_limit = _resolve_export_limit(section["range"]["limit"])
        if section["range"]["first"] != "0" or actual_limit != expected_limit:
            issues.append(
                f"{symbol}: normalized export range must remain [0,{expected_limit}), "
                f"got [{section['range']['first']},{section['range']['limit']})"
            )
    if check_exhaustive:
        try:
            exhaustive_range_contract(sections, catalog_sources)
        except ExportSchemaError as error:
            issues.append(str(error))
    return issues


def catalog_source_golden_issues(language_name: str, source: str) -> list[str]:
    """Check one repaired catalog's literal values at their exact ordinals."""

    issues: list[str] = []
    languages = {language.name: language for language in ABI.LANGUAGES}
    language = languages.get(language_name)
    if language is None:
        return [f"unknown normalized catalog language: {language_name}"]
    try:
        definitions = ABI.parse_definitions(source, language.base_source)
    except ABI.SchemaError as error:
        return [str(error)]
    parsed_entries: dict[str, list[str]] = {}
    for (golden_language, symbol), expected_entries in (
        NORMALIZED_CATALOG_GOLDENS.items()
    ):
        if golden_language != language_name:
            continue
        try:
            entries = _initializer_entry_sources(
                source,
                language.base_source,
                definitions,
                symbol,
            )
            parsed_entries[symbol] = entries
            for index, expected in expected_entries.items():
                label = f"{language_name}/{symbol}[{index}]"
                if index >= len(entries):
                    issues.append(
                        f"{label}: missing; initializer has {len(entries)} entries"
                    )
                    continue
                literal = _single_wide_literal(entries[index], label)
                if not literal and (language_name, symbol, index) in NORMALIZED_REPAIRED_SLOTS:
                    issues.append(f"{label}: repaired slot must remain nonempty")
                elif literal != expected:
                    issues.append(f"{label}: expected {expected!r}, got {literal!r}")
        except ExportSchemaError as error:
            issues.append(str(error))

    exact_sizes = {
        "pPersonnelAssignmentStrings": 85,
        "pLongAssignmentStrings": 85,
        "pDoorTrapStrings": 7,
        "pLandTypeStrings": 47,
        "gzGIOScreenText": 69,
    }
    for symbol, expected_size in exact_sizes.items():
        try:
            entries = parsed_entries.get(symbol) or _initializer_entry_sources(
                source, language.base_source, definitions, symbol
            )
            parsed_entries[symbol] = entries
            if len(entries) != expected_size:
                issues.append(
                    f"{language_name}/{symbol}: expected exactly {expected_size} "
                    f"initializer entries, got {len(entries)}"
                )
        except ExportSchemaError as error:
            issues.append(str(error))

    try:
        assignment = _initializer_entry_sources(
            source, language.base_source, definitions, "pAssignmentStrings"
        )
        personnel = parsed_entries["pPersonnelAssignmentStrings"]
        door = parsed_entries["pDoorTrapStrings"]
        for target_symbol, target_index, source_entries, source_index in (
            ("pPersonnelAssignmentStrings", 83, assignment, 83),
            ("pPersonnelAssignmentStrings", 84, assignment, 84),
            ("pLongAssignmentStrings", 54, personnel, 54),
            ("pLongAssignmentStrings", 83, assignment, 83),
            ("pLongAssignmentStrings", 84, assignment, 84),
            ("pDoorTrapStrings", 5, door, 3),
            ("pDoorTrapStrings", 6, door, 2),
        ):
            target_entries = parsed_entries[target_symbol]
            target = _single_wide_literal(
                target_entries[target_index],
                f"{language_name}/{target_symbol}[{target_index}]",
            )
            source_literal = _single_wide_literal(
                source_entries[source_index],
                f"{language_name}/alias-source[{source_index}]",
            )
            if target != source_literal:
                issues.append(
                    f"{language_name}/{target_symbol}[{target_index}]: "
                    f"must remain the same-catalog alias of index {source_index}"
                )
        if language_name != "English":
            land_definition = definitions["pLandTypeStrings"]
            raw_land_definition = source[
                land_definition["raw_start"]:land_definition["raw_end"]
            ]
            for index, value in enumerate(LAND_TYPE_TAIL, 41):
                fallback_line = re.compile(
                    rf'L"{re.escape(value)}"\s*,[^\r\n]*TODO[.]Translate'
                )
                if fallback_line.search(raw_land_definition) is None:
                    issues.append(
                        f"{language_name}/pLandTypeStrings[{index}]: canonical "
                        "English fallback lost TODO.Translate"
                    )
    except (ExportSchemaError, KeyError, IndexError) as error:
        issues.append(str(error))
    return issues


def normalized_catalog_issues(
    sections: Sequence[dict],
    abi_schema: Mapping,
    catalog_sources: Mapping[str, str] | None = None,
    check_exhaustive: bool = True,
) -> list[str]:
    """Validate repaired ordinals and every affected range in all quadrants."""

    issues: list[str] = []
    if len(FOREIGN_NORMALIZED_REPAIRED_SLOTS) != 19:
        issues.append(
            "foreign normalization inventory no longer names exactly 19 repaired slots"
        )
    if len(UNIVERSAL_RANGE_REPAIRED_SLOTS) != 104:
        issues.append(
            "universal normalization inventory no longer names exactly 104 inserts"
        )
    goldens = {
        (language, symbol, index)
        for (language, symbol), entries in NORMALIZED_CATALOG_GOLDENS.items()
        for index in entries
    }
    missing_goldens = sorted(NORMALIZED_REPAIRED_SLOTS - goldens)
    if missing_goldens:
        issues.append(f"repaired slots lack goldens: {missing_goldens!r}")

    issues.extend(
        normalized_range_issues(
            sections,
            abi_schema,
            catalog_sources,
            check_exhaustive=check_exhaustive,
        )
    )
    actual_by_language = {
        language.name: ABI.apply_inventory_overrides(
            abi_schema["symbols"],
            abi_schema["catalog_compatibility_debt"][language.name],
        )
        if language.name != "English"
        else abi_schema["symbols"]
        for language in ABI.LANGUAGES
    }
    languages = {language.name: language for language in ABI.LANGUAGES}
    for (language_name, symbol), _expected_entries in (
        NORMALIZED_CATALOG_GOLDENS.items()
    ):
        language = languages.get(language_name)
        if language is None:
            issues.append(f"unknown normalized catalog language: {language_name}")
            continue
        # Range shape is proved in all four quadrants above. These repaired
        # definitions must remain unconditional, so one raw-source parse is
        # sufficient for the literal/order goldens and keeps the check light.
        inventory = actual_by_language[language_name].get(symbol)
        if inventory is not None and inventory.get("conditional_layout"):
            issues.append(
                f"{language_name}/{symbol}: repaired catalog gained conditional layout"
            )

    catalog_names: set[str] = set()
    for language_name, _symbol in NORMALIZED_CATALOG_GOLDENS:
        if language_name in catalog_names:
            continue
        catalog_names.add(language_name)
        language = languages[language_name]
        source = (
            catalog_sources[language_name]
            if catalog_sources is not None and language_name in catalog_sources
            else _read(language.base_source)
        )
        issues.extend(catalog_source_golden_issues(language_name, source))

    imp_gear = lexical_mask(_read("Laptop/IMP Gear.cpp"))
    named_empty_reads = len(
        re.findall(r"\bpLongAssignmentStrings\s*\[\s*ASSIGNMENT_EMPTY\s*\]", imp_gear)
    )
    numeric_empty_reads = len(
        re.findall(r"\bpLongAssignmentStrings\s*\[\s*60\s*\]", imp_gear)
    )
    if named_empty_reads != 4 or numeric_empty_reads:
        issues.append(
            "IMP Gear must retain exactly four pLongAssignmentStrings"
            "[ASSIGNMENT_EMPTY] reads and zero numeric [60] reads"
        )
    if re.search(r"\bGIO_ULTIMATE_IRON_MAN_TEXT\b", lexical_mask(_read("i18n/include/Text.h"))):
        issues.append("unused GIO_ULTIMATE_IRON_MAN_TEXT enum slot returned")
    return issues


def make_schema() -> dict:
    abi_schema = _load_abi_schema()
    export_source = _read(EXPORT_SOURCE)
    property_source = _read(PROPERTY_EXPORT_SOURCE)
    adapter_header = _read(EXPORT_HEADER)
    build_source = _read(I18N_BUILD_SOURCE)
    validate_named_limit_static_assert_seam(export_source)
    adapter_contract = selected_catalog_adapter_contract(
        export_source, property_source, adapter_header, build_source
    )
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
    _validate_legacy_storage_paths(legacy_sections, abi_schema)

    normalization_issues = normalized_catalog_issues(
        sections, abi_schema, check_exhaustive=False
    )
    if normalization_issues:
        raise ExportSchemaError(
            "normalized catalog contract failed: " + "; ".join(normalization_issues)
        )
    legacy_range_contract = exhaustive_range_contract(sections)
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
            "exported compatibility debt grew from the "
            f"{MAX_EXPORTED_COMPATIBILITY_DEBT_PAIRS}-pair ceiling to "
            f"{len(debt)}"
        )
    if unsafe_debt:
        raise ExportSchemaError(
            "compatibility-debt inventory still contains an unsafe export range: "
            f"{len(unsafe_debt)} pair(s)"
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
        "legacy_range_comparisons": legacy_range_contract["comparisons"],
        "unsafe_range_sections": legacy_range_contract["unsafe_sections"],
        "unsafe_range_language_pairs": legacy_range_contract[
            "unsafe_language_pairs"
        ],
        "unsafe_range_quadrant_failures": legacy_range_contract[
            "unsafe_quadrant_failures"
        ],
        "potential_oob_reads_per_selected_build": legacy_range_contract[
            "potential_oob_reads_per_selected_build"
        ],
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
        "legacy_range_comparisons": EXPECTED_LEGACY_RANGE_COMPARISONS,
        "unsafe_range_sections": MAX_UNSAFE_RANGE_SECTIONS,
        "unsafe_range_language_pairs": MAX_UNSAFE_RANGE_LANGUAGE_PAIRS,
        "unsafe_range_quadrant_failures": MAX_UNSAFE_RANGE_QUADRANT_FAILURES,
        "potential_oob_reads_per_selected_build": (
            MAX_POTENTIAL_OOB_READS_PER_SELECTED_BUILD
        ),
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

    output_contract = ordered_output_contract(sections)

    return {
        "schema_version": 2,
        "purpose": "ordered source model of the developer GameStrings exporter",
        "runtime_behavior": (
            "selected linked globals and immutable TextPack entries are emitted "
            "through an injected immediate-copy sink; language remains compile-time"
        ),
        "adapter_status": IMPLEMENTED_ADAPTER_STATUS,
        "catalog_adapter": adapter_contract,
        "counts": counts,
        "named_export_limits": dict(NAMED_EXPORT_LIMITS),
        "output_contract": output_contract,
        "startup_contract": startup_contract(),
        "sections": sections,
        "legacy_symbols": legacy_symbols,
        "legacy_range_contract": legacy_range_contract,
        "exported_compatibility_debt": debt,
        "exporter_only_tables": exporter_only,
    }


def validate_manifest_contract(schema: Mapping) -> list[str]:
    issues = []
    if schema.get("schema_version") != 2:
        issues.append("schema: unsupported schema_version")
    expected_fields = {
        "schema_version",
        "purpose",
        "runtime_behavior",
        "adapter_status",
        "catalog_adapter",
        "counts",
        "named_export_limits",
        "output_contract",
        "startup_contract",
        "sections",
        "legacy_symbols",
        "legacy_range_contract",
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
        "legacy_range_comparisons": EXPECTED_LEGACY_RANGE_COMPARISONS,
        "unsafe_range_sections": MAX_UNSAFE_RANGE_SECTIONS,
        "unsafe_range_language_pairs": MAX_UNSAFE_RANGE_LANGUAGE_PAIRS,
        "unsafe_range_quadrant_failures": MAX_UNSAFE_RANGE_QUADRANT_FAILURES,
        "potential_oob_reads_per_selected_build": (
            MAX_POTENTIAL_OOB_READS_PER_SELECTED_BUILD
        ),
        "exporter_only_tables": EXPECTED_EXPORTER_ONLY_TABLES,
        "exporter_only_entries_per_language": EXPECTED_EXPORTER_ONLY_ENTRIES,
    }
    expected_count_fields = set(fixed_counts) | {
        "exported_compatibility_debt_pairs"
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
    if schema.get("named_export_limits") != dict(NAMED_EXPORT_LIMITS):
        issues.append("schema: live named export-limit values changed")
    output_contract = schema.get("output_contract")
    if not isinstance(output_contract, dict) or set(output_contract) != {
        "encoding",
        "record_framing",
        "empty_values",
        "indexing",
        "snapshots",
    }:
        issues.append("schema: ordered output-byte contract is missing")
    else:
        snapshots = output_contract.get("snapshots")
        if (
            output_contract.get("encoding") != "UTF-8"
            or output_contract.get("empty_values") != "suppressed"
            or output_contract.get("indexing") != "absolute source index"
            or not isinstance(snapshots, dict)
            or len(snapshots) != len(ABI.LANGUAGES) * len(ABI.QUADRANTS)
        ):
            issues.append("schema: ordered output-byte/quadrant contract changed")
    range_contract = schema.get("legacy_range_contract")
    expected_range_contract = {
        "comparisons": EXPECTED_LEGACY_RANGE_COMPARISONS,
        "unsafe_sections": MAX_UNSAFE_RANGE_SECTIONS,
        "unsafe_language_pairs": MAX_UNSAFE_RANGE_LANGUAGE_PAIRS,
        "unsafe_quadrant_failures": MAX_UNSAFE_RANGE_QUADRANT_FAILURES,
        "potential_oob_reads_per_selected_build": (
            MAX_POTENTIAL_OOB_READS_PER_SELECTED_BUILD
        ),
        "exported_pointer_entry_checks": EXPECTED_EXPORTED_POINTER_ENTRY_CHECKS,
        "direct_wide_literal_entry_checks": (
            EXPECTED_DIRECT_WIDE_LITERAL_ENTRY_CHECKS
        ),
        "compiled_selector_entry_checks": (
            EXPECTED_COMPILED_SELECTOR_ENTRY_CHECKS
        ),
    }
    if range_contract != expected_range_contract:
        issues.append("schema: exhaustive raw catalog range contract changed")
    adapter = schema.get("catalog_adapter")
    expected_adapter = {
        "source": EXPORT_SOURCE,
        "entrypoint": "i18n::ExportSelectedCatalog(SelectedCatalogExportSink&)",
        "sink_header": EXPORT_HEADER,
        "sink_lifetime": "borrowed views copied before callback return",
        "property_writer": PROPERTY_EXPORT_SOURCE,
        "linked_catalog_sources": EXPECTED_LINKED_CATALOG_SOURCES,
        "textual_catalog_includes": [],
        "local_extern_symbols": EXPECTED_LOCAL_EXTERN_SYMBOLS,
        "variant_axes": ["language", "campaign", "build"],
        "language_agreement": (
            "one language-target definition selects linked globals, g_lang, and TextPack"
        ),
    }
    if adapter != expected_adapter:
        issues.append("schema: selected linked-catalog adapter contract changed")
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
        issues.append(
            "schema: exported compatibility debt exceeds the "
            f"{MAX_EXPORTED_COMPATIBILITY_DEBT_PAIRS}-pair ceiling"
        )
    unsafe = [
        entry for entry in debt
        if isinstance(entry, dict) and entry.get("unsafe_range") is True
    ]
    if unsafe:
        issues.append("schema: compatibility debt contains an unsafe export range")
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
        issues.append(
            "schema: exporter-only table count is not exactly "
            f"{EXPECTED_EXPORTER_ONLY_TABLES}"
        )
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
        issues.append(
            "schema: exporter-only entry count is not exactly "
            f"{EXPECTED_EXPORTER_ONLY_ENTRIES}"
        )
    if exporter_only_entries != counts.get("exporter_only_entries_per_language"):
        issues.append("schema: exporter-only entry total does not match its inventory")
    if schema.get("adapter_status") != IMPLEMENTED_ADAPTER_STATUS:
        issues.append(
            "schema: selected-catalog adapter must remain explicitly implemented"
        )
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
        f"{counts['legacy_range_comparisons']} exhaustive range comparisons, "
        f"{counts['unsafe_range_language_pairs']} unsafe; "
        f"{schema['legacy_range_contract']['exported_pointer_entry_checks']} "
        "pointer entries; "
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
