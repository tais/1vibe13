#!/usr/bin/env python3
"""Ratchet lint for unbounded string-format sinks (sprintf / strcpy / strcat).

These three libc calls write into a destination with no length bound, so every
call site is a potential buffer overflow -- the classic overflow class, and a
recurring one in this codebase whenever a filename / message is built from
external (XML / .dat / network) data into a fixed CHAR8 buffer. The safe
replacements are snprintf / strlcpy / strlcat (or std::string), which take an
explicit destination size.

The tree still has ~1.6k legacy occurrences; mass-converting them in one pass
would be an unreviewable, regression-prone diff. So instead of banning them
outright this is a RATCHET: it counts the occurrences per function, compares
against a committed baseline (string_sink_baseline.json), and FAILS only when a
count goes UP -- i.e. when a change introduces a NEW sprintf/strcpy/strcat. Each
conversion of a legacy site lowers the baseline (run with --update), so the
number only ever moves in the safe direction and can never silently grow back.

Counting rules (kept deliberately simple and deterministic):
  * first-party source dirs only (SRC_DIRS); vendored ext/ and lua/ excluded.
  * C/C++ sources only (.c .cpp .h .hpp, case-insensitive).
  * // and /* */ comments are stripped first, so a call mentioned only in a
    comment (e.g. "was sprintf(dst, ...)") is NOT counted.
  * word-boundary match, so the safe n-variants are excluded automatically:
    snprintf, vsprintf, strncpy, strlcpy, strncat, strlcat do NOT match.

Usage:
    tools/lint_string_sinks.py            # check against baseline (CI)
    tools/lint_string_sinks.py --update   # rewrite the baseline from the tree
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASELINE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                        'string_sink_baseline.json')

# First-party engine source dirs (mirrors tools/audit_char16_ondisk_io.py).
# Vendored deps (ext/, lua/) and the build tree are intentionally excluded.
SRC_DIRS = ['sgp', 'Tactical', 'Strategic', 'Ja2', 'Laptop', 'TileEngine',
            'TacticalAI', 'Utils', 'Editor', 'Multiplayer',
            'ModularizedTacticalAI']

SRC_EXTS = ('.c', '.cpp', '.h', '.hpp')

# The unbounded sinks we ratchet. \b in front means snprintf / vsprintf /
# strncpy / strlcpy / strncat / strlcat (the bounded forms) are not matched.
SINK = re.compile(r'\b(sprintf|strcpy|strcat)\s*\(')


def strip_comments(text):
    """Drop // line and /* */ block comments so a sink named only in a comment
    isn't counted (e.g. a "// was sprintf(...)" note next to a fixed call)."""
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', ' ', text)
    return text


def iter_files():
    for d in SRC_DIRS:
        base = os.path.join(ROOT, d)
        for dirpath, _dirs, filenames in os.walk(base):
            if os.sep + 'build' in dirpath:
                continue
            for fn in filenames:
                if fn.lower().endswith(SRC_EXTS):
                    yield os.path.join(dirpath, fn)


def count_sinks():
    """Return {'sprintf': n, 'strcpy': n, 'strcat': n} over the whole tree."""
    counts = {'sprintf': 0, 'strcpy': 0, 'strcat': 0}
    for path in iter_files():
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        for m in SINK.finditer(strip_comments(text)):
            counts[m.group(1)] += 1
    return counts


def load_baseline():
    with open(BASELINE, encoding='utf-8') as f:
        data = json.load(f)
    return {k: int(data[k]) for k in ('sprintf', 'strcpy', 'strcat')}


def write_baseline(counts):
    with open(BASELINE, 'w', encoding='utf-8') as f:
        json.dump(counts, f, indent=2, sort_keys=True)
        f.write('\n')


def main(argv):
    counts = count_sinks()

    if '--update' in argv:
        write_baseline(counts)
        print('Wrote baseline:', counts)
        return 0

    if not os.path.exists(BASELINE):
        print('ERROR: baseline missing:', BASELINE, file=sys.stderr)
        print('Run: tools/lint_string_sinks.py --update', file=sys.stderr)
        return 2

    baseline = load_baseline()
    regressed = False
    for name in ('sprintf', 'strcpy', 'strcat'):
        cur, base = counts[name], baseline[name]
        flag = ''
        if cur > base:
            regressed = True
            flag = '  <-- NEW occurrence(s), not allowed'
        elif cur < base:
            flag = '  (improved -- run --update to lower the baseline)'
        print(f'  {name:8} {cur:5d}  (baseline {base}){flag}')

    if regressed:
        print('\nFAIL: a new sprintf/strcpy/strcat was introduced.', file=sys.stderr)
        print('Use the bounded form (snprintf / strlcpy / strlcat / std::string)',
              file=sys.stderr)
        print('with an explicit destination size instead.', file=sys.stderr)
        return 1

    print('\nOK: no new unbounded string-format sinks.')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
