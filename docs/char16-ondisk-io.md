# CHAR16 (=wchar_t) on-disk binary-I/O — hazard, pattern, and checklist

## The hazard
`CHAR16` is `typedef wchar_t CHAR16` (sgp/types.h). `wchar_t` is **4 bytes on
macOS/Linux** but **2 bytes on Windows**, while JA2 1.13's on-disk binary formats
(Prof.dat, `.map`, save games, sector temp files, UB v6.x maps, …) store wide
characters as **2-byte UTF-16**.

So any POD struct that backs a binary file and contains a `CHAR16 name[N]` member
is the **wrong size off-Windows**. A raw `FileRead(&s, sizeof(s))` / `FileWrite`
of such a struct mis-reads every field *after* the first wide string (and ABI
padding differs too) → silent data corruption / crashes. This is historically the
single most-recurring crash class on the non-Windows port (savegame, map-soldier,
schedules, merc-profile, 7z-name fixes, …).

## The remedy (already applied to the high-frequency paths)
Do **not** read/write the in-memory CHAR16 struct as raw bytes. Use a **fixed-width
mirror**: either a parallel struct whose wide fields are `UINT16[N]`, or an explicit
field-by-field transfer that writes/reads each `CHAR16` as 2-byte UTF-16. The
existing families that do this correctly:

- `MAPDISK_*` mirror structs (map I/O)
- `OLD_*_101` legacy-format structs
- the `Xfer`-visitor serializers (≈12 of them)
- per-loader field-wise Save/Load helpers (e.g. `Save/LoadRottingCorpseDefinition`,
  which writes `CHAR16 name[10]` as 16-bit on disk explicitly)

## Status (completeness pass)
`tools/audit_char16_ondisk_io.py` is a heuristic lint that flags any CHAR16-
containing struct used as a raw `sizeof()` FileRead/FileWrite/LOADDATA target. As of
this writing it reports **51 CHAR16-containing structs, 0 raw-I/O targets** — i.e.
every wide-char POD that touches disk goes through a mirror/field-wise path. Run it
in CI (non-zero exit on a flag) to catch a regression at merge time.

```
python3 tools/audit_char16_ondisk_io.py
```

The lint is advisory, not a proof — it cannot see a raw `FileRead(ptr, n)` whose
size isn't a literal `sizeof(Type)`, and a flagged hit may already be mirror-safe.
**Triage every hit by reading the loader; never "fix" one blind** — a wrong mirror
off-by-one ships *silent save corruption* that this environment cannot regression-
test (esp. UB v6.x map paths).

## Checklist for any NEW binary loader/saver
1. Does the struct you `FileRead`/`FileWrite` contain a `CHAR16` (or `wchar_t`)
   array, directly or nested? If no → fine.
2. If yes → **do not** raw-I/O it. Add a fixed-width path:
   - write/read each wide field as `UINT16` (2 bytes/char), or
   - define a `*_ONDISK`/mirror struct with `UINT16[N]` fields + an explicit
     convert, matching the `MAPDISK_*` / `Xfer` pattern.
3. Keep the on-disk byte layout identical across platforms (no `sizeof(wchar_t)`
   in any file offset or length).
4. Add/keep the loader green under `tools/audit_char16_ondisk_io.py`.
