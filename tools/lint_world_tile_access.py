#!/usr/bin/env python3
"""Ratchet direct access to JA2's legacy world-tile buffer.

WorldTileMapStorage is the sole production lifecycle owner of the MAP_ELEMENT
array. gpWorldLevelData remains as a hot compatibility projection while the
legacy call graph is migrated to GetMapElement. This lint makes that raw tail
monotonically shrink:

* a file may not gain another gpWorldLevelData[index] expression;
* a new file may not introduce one;
* moving raw access between files still fails;
* allocation, assignment, reallocation, and release are forbidden outside the
  dedicated owner;
* replacing raw access with GetMapElement is always accepted.

Usage:
    tools/lint_world_tile_access.py
    tools/lint_world_tile_access.py --update
"""

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
BASELINE = Path(__file__).resolve().parent / "world_tile_access_baseline.json"

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

OWNER = "TileEngine/World Tile Map.cpp"
ACCESSOR = "TileEngine/worlddef.h"
RAW_ACCESS = re.compile(r"\bgpWorldLevelData\s*\[")
LIFECYCLE_ACCESS = re.compile(
    r"\bgpWorldLevelData\s*=(?!=)"
    r"|\bMem(?:Free|Realloc)\s*\(\s*gpWorldLevelData\b"
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


def inspect_sources():
    counts = {}
    lifecycle_violations = []
    for path in iter_source_files():
        relative = path.relative_to(ROOT).as_posix()
        try:
            contents = path.read_text(encoding="utf-8", errors="replace")
        except OSError as error:
            raise RuntimeError(f"cannot read {path}: {error}") from error
        contents = strip_comments(contents)

        if relative != OWNER:
            for match in LIFECYCLE_ACCESS.finditer(contents):
                line = contents.count("\n", 0, match.start()) + 1
                lifecycle_violations.append((relative, line))

        if relative in (OWNER, ACCESSOR):
            continue
        count = len(RAW_ACCESS.findall(contents))
        if count:
            counts[relative] = count
    return counts, lifecycle_violations


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
        raise RuntimeError(f"unsupported world-tile baseline: {BASELINE}")

    files = {}
    for path, count in payload["files"].items():
        if not isinstance(path, str) or not isinstance(count, int) or count < 1:
            raise RuntimeError(f"invalid world-tile baseline entry: {path!r}")
        files[path] = count

    if payload.get("total") != sum(files.values()):
        raise RuntimeError("world-tile baseline total does not match its files")
    return files


def print_summary(counts, baseline_total):
    by_area = Counter()
    for path, count in counts.items():
        by_area[path.split("/", 1)[0]] += count

    total = sum(counts.values())
    print(
        f"  raw world-tile accesses {total:4d} in {len(counts):3d} files "
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
        help="replace the baseline with the current per-file counts",
    )
    args = parser.parse_args(argv)

    try:
        counts, lifecycle_violations = inspect_sources()
        if args.update:
            if lifecycle_violations:
                for path, line in lifecycle_violations:
                    print(
                        f"{path}:{line}: world-tile lifecycle bypass",
                        file=sys.stderr,
                    )
                return 1
            write_baseline(counts)
            print_summary(counts, sum(counts.values()))
            print(f"updated {BASELINE.relative_to(ROOT)}")
            return 0

        baseline = load_baseline()
    except RuntimeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    increases = []
    for path, count in sorted(counts.items()):
        previous = baseline.get(path, 0)
        if count > previous:
            increases.append((path, previous, count))

    print_summary(counts, sum(baseline.values()))
    if lifecycle_violations:
        print("\nFAIL: world-tile storage lifecycle bypassed:", file=sys.stderr)
        for path, line in lifecycle_violations:
            print(f"  {path}:{line}", file=sys.stderr)
    if increases:
        print("\nFAIL: direct world-tile access increased:", file=sys.stderr)
        for path, previous, count in increases:
            print(f"  {path}: {previous} -> {count}", file=sys.stderr)

    if lifecycle_violations or increases:
        print(
            "\nUse GetMapElement for tile access and World Tile Map for storage "
            "lifecycle.",
            file=sys.stderr,
        )
        return 1

    print("world-tile compatibility tail did not grow")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
