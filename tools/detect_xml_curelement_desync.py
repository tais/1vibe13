#!/usr/bin/env python3
"""Detect the JA2 XML-loader 'curElement desync' bug class statically.

The hand-rolled expat loaders advance a curElement/PARSE_STAGE enum in the START
handler (one strcmp(name,"TAG") branch per recognized tag) and unwind it in a
parallel ladder in the END handler. The recurring bug (xml-loader-curelement-
desync, e.g. XML_Items #156): a tag added to the START recognized-list but with NO
matching END-handler case leaves curElement stuck at a *_PROPERTY value, so every
field parsed AFTER it is silently written to the wrong place / zeroed.

This is a *heuristic* static check, not a proof: for each XML_*.cpp loader it
collects the tag-name literals compared in the start handler vs the end handler,
and reports tags recognized on open but never matched on close, UNLESS the end
handler has a catch-all final `else` that pops the state (the #156 remedy), in
which case the class is considered closed for that loader.

Intended as a CI / pre-merge lint over data loaders. Exit code is non-zero if any
loader is flagged, so it can gate CI. Output is advisory -- triage each hit by
reading the loader; false positives are possible (a tag closed generically).
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TAG_RE = re.compile(r'strcmp\s*\(\s*name\s*,\s*"([^"]+)"\s*\)')
# A "leaf" state is one whose name marks a property/data node -- those are the
# states that MUST be popped back in the end handler. A tag that merely advances to
# a container state (ELEMENT, *_LIST) and reads its own attributes inline does not
# desync if it has no end case, so we only flag tags that push a leaf state.
LEAF_STATE_RE = re.compile(r'PROPERTY|_DATA\b|_DATA$')

def split_handlers(text):
    """Return (start_body, end_body) by slicing on the handler function headers.
    Crude brace-less split: from a *StartElement* header to the next *EndElement*
    header, and from there to the next top-level function. Good enough to bucket
    the strcmp literals by handler."""
    starts = [m.start() for m in re.finditer(r'\b\w*StartElement\w*\s*\(', text)]
    ends   = [m.start() for m in re.finditer(r'\b\w*EndElement\w*\s*\(', text)]
    if not starts or not ends:
        return None, None
    s0 = starts[0]
    e0 = next((e for e in ends if e > s0), None)
    if e0 is None:
        return None, None
    # end body runs to the next char-handler / read fn / EOF
    rest = re.search(r'\b\w*(CharacterData|ReadIn)\w*\s*\(', text[e0+1:])
    e_end = e0 + 1 + rest.start() if rest else len(text)
    return text[s0:e0], text[e0:e_end]

def has_catchall_pop(end_body):
    """The #156 remedy: a trailing `else { ...curElement = ... }` that closes any
    unmatched tag. Heuristic: an `else` not directly followed by `if`."""
    return bool(re.search(r'\belse\s*\{', end_body)) and bool(re.search(r'curElement\s*=', end_body))

def main():
    flagged = []
    for dirpath, _dirs, files in os.walk(ROOT):
        if '/build' in dirpath or '/ext/' in dirpath:
            continue
        for fn in files:
            if not fn.startswith('XML_') or not fn.endswith('.cpp'):
                continue
            path = os.path.join(dirpath, fn)
            try:
                text = open(path, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            start_body, end_body = split_handlers(text)
            if start_body is None:
                continue
            end_tags = set(TAG_RE.findall(end_body))
            # Collect only tags whose start branch advances curElement to a LEAF
            # (property/data) state -- those are the ones that must be popped on
            # close. Scan each strcmp tag and the curElement assignment that follows
            # it within its branch (up to the next strcmp).
            leaf_start_tags = set()
            matches = list(TAG_RE.finditer(start_body))
            for i, m in enumerate(matches):
                tag = m.group(1)
                seg_end = matches[i + 1].start() if i + 1 < len(matches) else len(start_body)
                segment = start_body[m.end():seg_end]
                assign = re.search(r'curElement\s*=\s*([A-Za-z0-9_]+)', segment)
                if assign and LEAF_STATE_RE.search(assign.group(1)):
                    leaf_start_tags.add(tag)
            if not leaf_start_tags:
                continue
            missing = sorted(leaf_start_tags - end_tags)
            if missing and not has_catchall_pop(end_body):
                rel = os.path.relpath(path, ROOT)
                flagged.append((rel, missing))

    if not flagged:
        print("OK: no curElement desync candidates (all loaders close their tags "
              "or have a catch-all pop).")
        return 0
    print("Potential curElement desync (tag recognized on open, no close case, "
          "no catch-all else-pop) -- triage by reading each loader:\n")
    for rel, missing in flagged:
        print(f"  {rel}")
        print(f"      start-only tags: {', '.join(missing)}")
    print(f"\n{len(flagged)} loader(s) flagged.")
    return 1

if __name__ == '__main__':
    sys.exit(main())
