#!/usr/bin/env python3
"""Audit the CHAR16(=wchar_t) on-disk binary-I/O hazard (completeness lint).

Hazard (char16-wchar-file-format): CHAR16 is typedef'd to wchar_t, which is 4
bytes on macOS/Linux but only 2 bytes in JA2's on-disk binary formats (Prof.dat,
.map, saves, ...). Any POD struct that backs a binary file and contains a CHAR16
array is therefore the WRONG SIZE off-Windows, so a raw FileRead/FileWrite of it
mis-reads every field after the first wide string -- the single most-recurring
crash/corruption class on the port.

The established remedy is a fixed-width MIRROR struct (UINT16 fields) used at the
I/O boundary with an explicit field-by-field Xfer/convert (see the MAPDISK_*,
OLD_*_101 and Xfer-visitor families). Most high-frequency paths are already fixed
this way; the residual risk is a SINGLE missed loader that still does raw I/O on a
CHAR16-containing struct.

This tool is a *heuristic completeness lint*, NOT a proof or an auto-fixer (a wrong
"fix" here ships silent save corruption -- triage every hit by hand). For each
first-party struct that contains a CHAR16 array member, it reports whether that
struct's typename appears as the target of a raw FileRead/FileWrite/LOADDATA in any
.cpp -- i.e. a candidate raw binary I/O of a wide-char POD that should be checked
for a fixed-width mirror.
"""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC_DIRS = ['sgp', 'Tactical', 'Strategic', 'Ja2', 'Laptop', 'TileEngine',
            'TacticalAI', 'Utils', 'Editor', 'Multiplayer']
CHAR16_MEMBER = re.compile(r'\bCHAR16\s+\w+\s*\[')
# struct/typedef name capture (best-effort)
STRUCT_OPEN = re.compile(r'\b(?:typedef\s+)?struct\s+(?:tag)?(\w+)?')
IO_CALL = re.compile(r'\b(FileRead|FileWrite|LOADDATA|EXTRACTDATA|JA2EncryptedFileRead|JA2EncryptedFileWrite)\s*\(')

def strip_comments(text):
    """Remove // line and /* */ block comments so a sizeof() mentioned only in a
    comment (e.g. describing already-replaced raw I/O) isn't flagged."""
    text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    text = re.sub(r'//[^\n]*', ' ', text)
    return text

def iter_files(exts):
    for d in SRC_DIRS:
        base = os.path.join(ROOT, d)
        for dp, _dirs, fs in os.walk(base):
            if '/build' in dp:
                continue
            for fn in fs:
                if fn.endswith(exts):
                    yield os.path.join(dp, fn)

def find_char16_structs():
    """Return {structName: relpath} for structs whose body has a CHAR16 array."""
    out = {}
    for path in iter_files(('.h', '.hpp')):
        try:
            text = open(path, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        # crude brace matching for each `struct [tag] NAME {...}` / typedef struct {...} NAME;
        for m in re.finditer(r'\bstruct\b[^\{;]*\{', text):
            start = m.end()
            depth = 1; i = start
            while i < len(text) and depth:
                if text[i] == '{': depth += 1
                elif text[i] == '}': depth -= 1
                i += 1
            body = text[start:i-1]
            if not CHAR16_MEMBER.search(body):
                continue
            # name: either `struct NAME {` (before) or `} NAME;` (after)
            before = text[max(0, m.start()-80):m.start()+m.group().find('{')]
            nm = re.search(r'struct\s+(?:tag)?(\w+)', text[m.start():m.end()])
            after = re.search(r'\}\s*(\w+)\s*;', text[i-1:i+60])
            name = (nm.group(1) if nm and nm.group(1) else None) or (after.group(1) if after else None)
            if name:
                out[name] = os.path.relpath(path, ROOT)
    return out

def main():
    structs = find_char16_structs()
    if not structs:
        print("No first-party structs with CHAR16 array members found (unexpected).")
        return 0

    # Collect raw I/O target typenames: look at FileRead/FileWrite(..., &var, sizeof(...))
    # and remember sizeof(TYPE) usage near an I/O call.
    io_types = {}  # struct name -> set of relpaths where it's I/O'd raw
    for path in iter_files(('.cpp',)):
        try:
            text = strip_comments(open(path, encoding='utf-8', errors='replace').read())
        except OSError:
            continue
        for m in IO_CALL.finditer(text):
            seg = text[m.end():m.end()+240]
            for name in structs:
                # sizeof(Type) inside the call args is the strongest raw-POD-I/O signal
                if re.search(r'sizeof\s*\(\s*' + re.escape(name) + r'\s*\)', seg):
                    io_types.setdefault(name, set()).add(os.path.relpath(path, ROOT))

    print(f"CHAR16-containing structs found: {len(structs)}")
    print(f"...of which appear as a raw sizeof()-I/O target: {len(io_types)}\n")
    if not io_types:
        print("OK: no CHAR16 struct is read/written via a raw sizeof() FileRead/FileWrite.")
        print("(Wide strings are going through fixed-width mirrors / field-wise Xfer.)")
        return 0
    print("TRIAGE -- each of these does raw binary I/O on a wide-char POD; confirm it")
    print("uses a fixed-width (UINT16) mirror, not the raw CHAR16 struct:\n")
    for name in sorted(io_types):
        print(f"  {name}  (declared in {structs[name]})")
        for p in sorted(io_types[name]):
            print(f"      raw I/O in {p}")
    return 1

if __name__ == '__main__':
    sys.exit(main())
