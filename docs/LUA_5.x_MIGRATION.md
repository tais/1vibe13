# Lua upgrade (5.1 → 5.5) — engine notes & mod migration guide

The SDL3 port upgraded the embedded Lua interpreter from **5.1.5** (2012) to
**5.5.0** (released 22 Dec 2025), built from source on every platform (no system
Lua) like the other vendored deps. This doc records what the engine changed and
what **mod authors** must do to bring 5.1-era scripts up to 5.5.

> Note: lua.org still labels **5.4.8** as the "current" recommended release while
> 5.5.0 is the newest. We track newest; to pin a different line, change the
> tarball URL + `URL_HASH` in the top-level `CMakeLists.txt` (the Lua block) and
> rebuild — nothing else in the engine is version-specific.

---

## What the engine changed (C side)

The Lua C library is fetched + built by CMake (`lua` target); the JA2 C++
wrapper lives in `lua/`. Upgrade touched only:

- **Build**: `CMakeLists.txt` fetches `lua-5.5.0.tar.gz`; the stale Lua 5.1
  header copies in `lua/` (`lua.h`, `lauxlib.h`, `luaconf.h`, `lualib.h`,
  `lua.hpp`) were removed so all includes resolve to the fetched 5.5 headers.
- **C API renames** (removed/renamed since 5.1) in the wrapper + Strategic code:
  - `lua_open()` → `luaL_newstate()`
  - `lua_objlen()` → `lua_rawlen()`
  - `luaL_checkint()` / `luaL_optint()` → `luaL_checkinteger()` / `luaL_optinteger()`
  - `luaL_typerror()` → `luaL_typeerror()`

The shipped 1.13 scripts (`Data*/Scripts/*.lua`) were checked and are already
5.5-clean — no engine-side script edits were needed.

---

## Mod migration guide (5.1 → 5.5)

If your mod ships `.lua` scripts written for JA2's old Lua 5.1, here are the
changes that actually bite, accumulated across 5.2 → 5.3 → 5.4 → 5.5. Most JA2
scripts are simple and need few or none of these.

### From 5.1 → 5.2
- **`setfenv` / `getfenv` removed.** Environment is now the `_ENV` upvalue. Most
  mods don't use these; if you do, restructure to pass state explicitly.
- **`unpack(t)` → `table.unpack(t)`.**
- **`table.getn(t)` / `table.setn` removed** → use the `#t` length operator.
- **`string.gfind` → `string.gmatch`.**
- **`math.mod` → `math.fmod`**, `math.log10(x)` → `math.log(x, 10)`.
- **`loadstring` → `load`** (which now also loads strings).
- **`arg` table in vararg functions removed** → use `...` and `select('#', ...)`.
- **`module()` deprecated** → return a table from the file instead.

### From 5.2 → 5.3 (the big one: integers)
- **Numbers now have integer and float subtypes.** `3` is an integer, `3.0` a
  float. Affects formatting and equality-with-display:
  - **`/` is always float division**: `5/2 == 2.5`. For integer division use
    **`//`**: `5//2 == 2`. *This is the most common JA2-mod breakage* — anywhere
    you divided expecting a truncated integer (sector math, counts) now yields a
    float; switch to `//` or wrap in `math.floor`/`math.tointeger`.
  - `tostring(3)` is `"3"` but `tostring(3.0)` is `"3.0"`.
  - Indexing a table with `3.0` vs `3` is the same key (floats with integer
    value normalize), but mixing can surprise.
- **Bitwise operators added**: `&  |  ~  <<  >>` (and `~` as unary not). If your
  5.1 mod used a bit library, switch to native operators.
- **Stricter string↔number coercion** in some spots; prefer explicit
  `tonumber`/`tostring`.

### From 5.3 → 5.4
- **`<const>` and `<close>` variable attributes** added (new syntax; only an
  issue if a variable was literally named to collide — unlikely).
- **`math.random(m, n)` / generator changed**; don't rely on exact sequences.
- **`__gc` / finalizer timing** and a new generational GC — only matters for
  mods doing exotic resource management.

### From 5.4 → 5.5
- **`global` is now a reserved word** — do not use it as a variable/field name.
- **The control variable of a numeric `for` is read-only** — you can no longer
  reassign `i` inside `for i = a, b do ... end`; declare a local with the same
  name if you need to mutate it.
- **GC tuning API changed**: `collectgarbage("incremental"/"generational")` no
  longer take tuning params; use `collectgarbage("param", ...)`.
- **A `nil` error object** thrown via `error(nil)` is replaced by a string
  message.

### Quick checklist for a 5.1 mod
1. Replace `/` with `//` wherever you meant integer division. ← most important
2. `unpack` → `table.unpack`; `table.getn(t)` → `#t`; `string.gfind` →
   `string.gmatch`; `loadstring` → `load`; `math.mod` → `math.fmod`.
3. Remove any `setfenv`/`getfenv`; replace `arg` with `...`.
4. Rename any variable/field called `global`.
5. Don't reassign numeric-`for` loop variables.
6. Test in-game: Lua errors surface at the call site (e.g. sector/NPC init,
   quests, strategic events) — load a game and exercise the affected systems.

### Testing your migrated scripts
Lua runs on game events — entering a sector (NPC init), quest checks, hourly
strategic updates, mine income, etc. Run the game through those paths; a script
error is reported with the script name and line. There is no separate validator
yet; playtesting the affected systems is the verification.
