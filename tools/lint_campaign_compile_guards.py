#!/usr/bin/env python3
"""Ratchet first-party compile-time JA2/UB campaign selection.

The engine/framework migration emits both campaign implementations and chooses
between them through GameCapabilities. A legacy tail still uses JA2UB
preprocessor conditionals inside application and content implementations.
This lint makes that tail monotonically shrink:

* a file may not gain another JA2UB conditional;
* a new file may not introduce one;
* moving a conditional between files still fails;
* deleting or converting conditionals is always accepted.

The baseline records conditional directives per file, rather than only a
repository-wide total, so an unrelated cleanup cannot hide a regression.

Usage:
    tools/lint_campaign_compile_guards.py
    tools/lint_campaign_compile_guards.py --update
"""

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
BASELINE = Path(__file__).resolve().parent / "campaign_compile_guard_baseline.json"

SOURCE_DIRS = (
    "Engine",
    "sgp",
    "Tactical",
    "Strategic",
    "Ja2",
    "Laptop",
    "TileEngine",
    "TacticalAI",
    "Utils",
    "Editor",
    "Multiplayer",
    "ModularizedTacticalAI",
)
SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}

CAMPAIGN_GUARD = re.compile(
    r"^[ \t]*#[ \t]*(?:if|ifdef|ifndef|elif)\b[^\r\n]*\bJA2UB\b",
    re.MULTILINE,
)


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    return re.sub(r"//[^\r\n]*", " ", text)


def iter_source_files():
    for source_dir in SOURCE_DIRS:
        root = ROOT / source_dir
        if not root.is_dir():
            continue
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix.lower() in SOURCE_EXTENSIONS:
                yield path


def count_guards():
    counts = {}
    for path in iter_source_files():
        try:
            contents = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            raise RuntimeError(f"cannot read {path}: {error}") from error
        count = len(CAMPAIGN_GUARD.findall(strip_comments(contents)))
        if count:
            counts[path.relative_to(ROOT).as_posix()] = count
    return counts


def write_baseline(counts):
    payload = {
        "schema": 1,
        "total": sum(counts.values()),
        "files": dict(sorted(counts.items())),
    }
    BASELINE.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def load_baseline():
    try:
        payload = json.loads(BASELINE.read_text(encoding="utf-8"))
    except FileNotFoundError as error:
        raise RuntimeError(
            f"baseline missing: {BASELINE}; run with --update"
        ) from error
    except (OSError, json.JSONDecodeError) as error:
        raise RuntimeError(f"cannot load {BASELINE}: {error}") from error

    if payload.get("schema") != 1 or not isinstance(payload.get("files"), dict):
        raise RuntimeError(f"unsupported campaign-guard baseline: {BASELINE}")

    files = {}
    for path, count in payload["files"].items():
        if not isinstance(path, str) or not isinstance(count, int) or count < 1:
            raise RuntimeError(f"invalid campaign-guard baseline entry: {path!r}")
        files[path] = count

    if payload.get("total") != sum(files.values()):
        raise RuntimeError("campaign-guard baseline total does not match its files")
    return files


def print_summary(counts, baseline_total):
    by_area = Counter()
    for path, count in counts.items():
        by_area[path.split("/", 1)[0]] += count

    total = sum(counts.values())
    print(
        f"  JA2UB conditionals {total:4d} in {len(counts):3d} files "
        f"(baseline {baseline_total})"
    )
    print(
        "  areas: "
        + ", ".join(
            f"{area}={count}" for area, count in sorted(by_area.items())
        )
    )


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--update",
        action="store_true",
        help="replace the baseline with the current, lower inventory",
    )
    args = parser.parse_args(argv)

    try:
        counts = count_guards()
        if args.update:
            write_baseline(counts)
            print(
                f"Wrote {BASELINE.relative_to(ROOT)}: "
                f"{sum(counts.values())} conditionals in {len(counts)} files"
            )
            return 0

        baseline = load_baseline()
    except RuntimeError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2

    regressions = []
    improvements = []
    for path in sorted(set(counts) | set(baseline)):
        current = counts.get(path, 0)
        previous = baseline.get(path, 0)
        if current > previous:
            regressions.append((path, current, previous))
        elif current < previous:
            improvements.append((path, current, previous))

    print_summary(counts, sum(baseline.values()))

    if improvements:
        print("  improved:")
        for path, current, previous in improvements:
            print(f"    {path}: {previous} -> {current}")
        print("  run with --update to lower the committed baseline")

    if regressions:
        print("\nFAIL: compile-time campaign identity increased:", file=sys.stderr)
        for path, current, previous in regressions:
            print(f"  {path}: {previous} -> {current}", file=sys.stderr)
        print(
            "Select campaign behavior through GameCapabilities instead.",
            file=sys.stderr,
        )
        return 1

    print("\nOK: no first-party file gained a JA2UB compile guard.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
