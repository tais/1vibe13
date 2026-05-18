# SDL3 Port Plan

Tracking doc for migrating JA2 1.13 off Win32 / DirectDraw / FMOD / Smacker
onto SDL3 + cross-platform equivalents, with a 32-bit RGBA8888 internal
rendering pipeline.

This lives on the `sdl3-port` branch. `master` is left untouched.

**End state — important**: SDL3 fully replaces the Win32 + DirectDraw +
FMOD + GDI stack **on every platform**, including Windows. There is no
permanent two-path build. The `#ifdef _WIN32` gates that appeared all
over `sgp/` during Phase 1 are **transitional scaffolding** — they
exist only because the Win32 implementations are still the only ones
that work today. Each phase deletes a chunk of Win32 code AND its
`_WIN32` gates on its way out, replacing them with the portable SDL3
implementation. By Phase 10 there should be zero `_WIN32` gates left
in the codebase (other than packaging differences in CMake), and the
pre-built `.lib` blobs at the repo root are deleted.

## Status at a glance

| Phase | Description | State |
|---|---|---|
| 0 | Branch + plan doc | ✅ Done |
| 1 | Build system portability — configures, compiles, and links on macOS | ✅ Done |
| 2 | Portable file I/O / time / memory / debug | Partial via compat shims; real porting pending |
| 3 | SDL3 window + event loop, drop WinMain | 🟡 Started — SDL3 wired into CMake, minimal window+event-loop main() on non-Windows. JA2 game loop not yet dispatched; SDL event translation lives in main() but routes to a local debug stub instead of the real `QueueEvent`. |
| 4 | SDL3 input, drop DirectInput / Win32 hooks | Not started |
| 5 | SDL3 video (RGB565 transitional), retire DirectDraw | 🟡 Started — RGB565 SDL_Texture upload path working with a test pattern in sgp.cpp's main(). Needs the JA2 framebuffer wired in. |
| 6 | RGBA8888 pipeline, rewrite blitters, kill inline asm | 🟡 Started — all inline x86 asm in `sgp/vobject_blitters.cpp` ported to portable C (~30 blitter variants); RGB565 still the internal format. RGBA8888 conversion + palette LUT regen pending. |
| 7 | Audio — SDL3_mixer / SoLoud, drop FMOD | Not started |
| 8 | Cinematics — libsmacker, decide on Bink | Not started |
| 9 | Fonts — stb_truetype, drop GDI | Not started |
| 10 | Platform packaging + CI | Not started |
| ∞ | Multiplayer — port to RakNet 4 or netlib swap | Deferred indefinitely |

As of Phase 1 closing, the build hits `[100%] Built target JA2_ENGLISH`
on macOS. The resulting binary prints a "not yet implemented" notice
and exits — nothing renders, nothing accepts input, nothing plays
audio. **Every Win32-gated block in the codebase corresponds to a
named phase below**; that's the work plan.

## Goals

- Native builds on Windows, macOS, and Linux (all first-class), all
  using the same SDL3-backed implementation. No Windows-only legacy
  path past Phase 10.
- No DirectX, no Win32 GUI/audio APIs, no Wine workarounds, no cnc-ddraw
  shim. The `wine/` subdirectory goes away. **On Windows** the
  message pump, window class, DirectDraw rendering, DirectSound, GDI
  fonts, Win32 file I/O, and Win32 input hooks all get deleted as
  their phase swaps them for SDL3.
- Internal rendering pipeline is RGBA8888 (32-bit). RGB565 surfaces,
  16-bit palette LUTs, and the inline-asm RGB565 alpha blender are
  retired.
- Audio (SFX, music, Smacker cinematics) replaced with portable
  libraries in this same effort.
- Build system is CMake-only and fetches its dependencies (no
  pre-built `.lib` blobs checked into the repo).

## Non-goals (for this branch)

- Refactoring game logic beyond what the API changes force.
- Touching the modding/INI/Lua interfaces.
- Rewriting the editor as a separate concern — it must keep building
  but is not the focus.
- Networking (Multiplayer) beyond keeping it linking; SDL_net swap is
  out of scope.

## Approach

Land the work in independent, reviewable phases. Each phase should
leave the branch in a buildable state on at least Windows, and the
later phases should leave it buildable on all three targets. Each
phase below is roughly a PR-sized chunk and will be broken into
several commits.

The strategy is **strangler-fig**: introduce the SDL3 surface
underneath the existing SGP API, then swap one subsystem at a time,
deleting the Win32 implementation as each subsystem is migrated. The
SGP public API (what the rest of the game calls) stays mostly stable;
the implementation behind it is what changes.

---

## How Phase 1 was actually closed

Phase 1's exit criterion turned out to be more invasive than the
original plan anticipated. Rather than write SDL3 code in this phase,
we landed an extensive **gating + compat-stub layer** so the existing
Win32/DirectDraw/FMOD code is preserved verbatim under `#ifdef _WIN32`
and non-Windows builds get either:

- Inline stubs (for typedefs and small helpers) defined in
  [sgp/msvc_compat.h](../sgp/msvc_compat.h), or
- No-op function definitions (for whole subsystems like Multiplayer).

This means **on Windows nothing changed at runtime**. On non-Windows
the build compiles and links but most subsystems are non-functional
until their respective phase replaces the gated blocks with real
SDL3-backed implementations.

### The compat header — `sgp/msvc_compat.h`

Single biggest piece of new code. Active only on non-Windows. Provides:

- **Win32 unsized typedefs**: `BOOL`, `UINT`, `INT`, `ULONG`, `LONG`,
  `WORD`, `USHORT`, `DWORD`, `BYTE`, `CHAR`, `WCHAR`, `UCHAR`,
  `LONGLONG`, `ULONGLONG`, `HANDLE`, `LPVOID`, `LPSTR`, `LPCSTR`,
  `LPWSTR`, `LPCWSTR`, `LPDWORD`, `VOID`.
- **Win32 structs**: `FILETIME`, `RECT`, `RGBQUAD`, `SYSTEMTIME`,
  `WIN32_FIND_DATA`, `LARGE_INTEGER` (union with `QuadPart`),
  `CRITICAL_SECTION`, `MEMORYSTATUS`, `TIMECAPS`.
- **`MAX_PATH`** as `PATH_MAX`.
- **Min/max**: `__min`, `__max`, `min`, `max` macros (POSIX has them
  but lowercase `min`/`max` conflict with `std::min`/`std::max`; we
  pick the ternary-macro form for compat with Windows behavior).
- **String/array**: `_countof`, `_stricmp`/`_strnicmp`/`_wcsicmp`
  mapped to `strcasecmp`/`strncasecmp`/`wcscasecmp`, `_strupr` /
  `_strlwr` / `_wcsupr` / `_wcslwr` written as ASCII-range loops,
  `_itow` / `_ltow` hand-rolled, `_wtoi` as `wcstol`, `wcstok_2arg`
  shim mapping MSVC's 2-arg form onto POSIX's 3-arg form, `sprintf_s`
  as `snprintf`, `MAXUINT8/16/32` macros.
- **swprintf macro** routing through a `sgp_compat::arrsize` template
  that pulls the array extent at compile time. Pointer-buffer call
  sites that legitimately need explicit size use **`sgp_swprintf`** —
  defined at the bottom of the file, available cross-platform.
- **`vswprintf` template overload** for the same array-extent trick.
- **Win32 string conversion stubs**: `MultiByteToWideChar` /
  `WideCharToMultiByte` return 0 (failure). `CP_ACP` / `CP_UTF8`
  macros. *Phase 5–7 should replace these with `std::codecvt` or
  `iconv`.*
- **Time stubs**: `GetTickCount` via `std::chrono::steady_clock`,
  `Sleep` via `std::this_thread::sleep_for`, `timeBeginPeriod` /
  `timeEndPeriod` / `timeSetEvent` / `timeKillEvent` as no-ops,
  `QueryPerformanceFrequency` / `QueryPerformanceCounter` via
  `std::chrono::steady_clock`, `GetCurrentThreadId` returns 1.
- **Thread/event stubs (BIG WARNING)**: `CreateThread` returns NULL —
  the JA2 clock thread and notify thread don't actually run on
  non-Windows. `CreateEvent` / `SetEvent` / `ResetEvent` /
  `WaitForSingleObject` / `WaitForMultipleObjects[Ex]` /
  `CloseHandle` are no-ops returning `WAIT_TIMEOUT`. Critical-section
  ops are no-ops on a tiny struct.
- **File I/O stubs (BIG WARNING)**: `CreateFile` returns
  `INVALID_HANDLE_VALUE`. `ReadFile` / `WriteFile` / `SetFilePointer`
  / `GetFileSize` / `FormatMessage` are no-ops. **SLF library reading
  will not work** until Phase 2 ports `LibraryDataBase.cpp` to use
  `std::ifstream`.
- **Heap stubs**: `GetProcessHeap` returns sentinel, `HeapAlloc` /
  `HeapFree` route through `malloc`/`free`, `VirtualAlloc` /
  `VirtualFree` / `VirtualLock` / `VirtualUnlock` route through
  `malloc`/`free` (lock semantics dropped).
- **Error codes**: `ERROR_SUCCESS`, `ERROR_NOT_READY`,
  `ERROR_INSUFFICIENT_BUFFER`. `GetLastError` returns 0.
- **`OutputDebugString[A|W]`** emit to `stderr`.
- **`__debugbreak`** as `__builtin_trap`.
- **`ZeroMemory`** as `memset(_, 0, _)`.
- **`_abs64`** as `llabs`.
- **`GetPrivateProfileString[A]`** stub that writes the supplied
  default. *Phase 2 should remove this when the legacy INI helpers
  are dropped.*
- **`OPEN_EXISTING` / `GENERIC_READ` etc.**: full set of Win32 file
  flag constants for compile-time use.

When any of these gets a real cross-platform implementation in its
phase, the compat shim should be deleted to avoid drift.

### Gated subsystems

The big picture: every Win32-specific subsystem now compiles to an
empty translation unit (or stub) on non-Windows. Listed by which phase
will revive them:

#### Phase 2 — file I/O / time / memory

- [sgp/FileMan.cpp](../sgp/FileMan.cpp): direct `<io.h>` /
  `<direct.h>` / `<windows.h>` gated. Uses `WIN32_FIND_DATA`,
  `CreateFile`, `ReadFile`, `SetFilePointer`, etc. — currently routes
  through the compat stubs which fail. **Phase 2 rewrites on
  `std::filesystem` / `<cstdio>`** with case-insensitive resolution
  for asset paths (mirror Stracciatella's known-good implementation).
- [sgp/LibraryDataBase.cpp](../sgp/LibraryDataBase.cpp): SLF archive
  reader. Same story — uses Win32 file APIs via the compat stubs.
  Phase 2 ports to portable streams. The `_splitpath` site has a
  basename-by-last-slash fallback at line ~984.
- [sgp/MemMan.cpp](../sgp/MemMan.cpp): `<windows.h>` gated. The
  `_DEBUG && _MSC_VER` heap-tracking branch falls through to plain
  malloc on non-MSVC.
- [sgp/timer.cpp](../sgp/timer.cpp): `Clock` callback and
  `SetTimer` / `KillTimer` gated. Non-Windows reads `GetTickCount`
  (compat shim) directly. Phase 2 should provide a proper monotonic
  timer.
- [Utils/Timer Control.cpp](../Utils/Timer%20Control.cpp): the
  JA2 clock thread + notify thread machinery uses Win32 events,
  mutexes, and `timeSetEvent` callbacks. Currently those are all
  no-op compat stubs — **the clock thread doesn't tick on non-Windows**,
  so anything counting on `guiBaseJA2Clock` advancing is broken.
  Phase 2 rebuilds on `std::thread` + `std::chrono` + `std::mutex` +
  `std::condition_variable`. SEH `__try`/`__except`/`__finally`
  blocks gated on `_MSC_VER`.
- [sgp/Random.cpp](../sgp/Random.cpp): `<windows.h>` and the
  `GetCursorPos` cursor-position seed contribution are gated.
- [sgp/DEBUG.cpp](../sgp/DEBUG.cpp): the assertion message pump that
  pumps `PeekMessage` is gated; non-Windows path just calls
  `GameLoop` until `gfProgramIsRunning` flips. SEH blocks gated on
  `_MSC_VER`. The `<crtdbg.h>` include is gated on `_MSC_VER`.

#### Phase 3 — window, event loop, message plumbing

- [sgp/sgp.cpp](../sgp/sgp.cpp): the **entire 1410-line file** is
  gated behind `_WIN32`. `WinMain`, `WindowProcedure`, the window
  class registration, the cnc-ddraw detection (`bCncDdraw`), the
  `iScreenMode` / windowed/fullscreen plumbing, the dll-override-by-
  Wine helper — all of it. The non-Windows replacement is a stub
  `main()` at the bottom of the file. **Phase 3 writes a portable
  `SDL_main` that calls `SDL_CreateWindow`, runs `SDL_PollEvent`
  fanning into JA2's event queue, and dispatches into the game loop.**
- [wine/](../wine/): the registry-poking Wine DLL override helper.
  Already excluded from non-Windows builds by the top-level
  CMakeLists `if(WIN32)`. Phase 3 deletes the directory once SDL3
  replaces DirectDraw entirely.
- [sgp/video.h](../sgp/video.h): `HWND ghWindow` and the
  `LPDIRECTDRAW2 GetDirectDraw2Object()`-style getters are behind
  `_WIN32`. Phase 3 introduces a portable accessor for the window
  handle (likely `SDL_Window*` directly, or an opaque pointer).
- `MessageBox` / `ShowWindow` / `SetCursor` call sites across the
  codebase. Phase 3 swaps to `SDL_ShowSimpleMessageBox` /
  `SDL_HideCursor` / `SDL_SetCursor`.

#### Phase 4 — input

- [sgp/input.cpp](../sgp/input.cpp): `KeyboardHandler` /
  `MouseHandler` Win32 hook procedures (`LRESULT CALLBACK`),
  `SetWindowsHookEx` / `UnhookWindowsHookEx`, `CallNextHookEx`,
  `MOUSEHOOKSTRUCT` consumer, `WM_*` switch — all gated `_WIN32`.
  `HHOOK` globals gated too. `RestrictMouseCursor` / `FreeMouseCursor`
  / `RestoreCursorClipRect` / `GetRestrictedClipCursor` /
  `SimulateMouseMovement` cursor-clipping gated. `DequeueAllKeyBoardEvents`
  `PeekMessage` pump gated. SEH `__try`/`__except`/`__finally` blocks
  gated on `_MSC_VER`. `GetCursorPos` mouse-poll sites all replaced
  with `gusMouseXPos` / `gusMouseYPos` fallback on non-Windows.
- [Utils/KeyMap.cpp](../Utils/KeyMap.cpp): the entire VK_* virtual-
  key-code translation table (160 constants) is gated `_WIN32`.
  **Phase 4 must rebuild this on `SDL_Scancode` while preserving the
  numeric VK_* values for save-game compatibility** (those values are
  persisted in savegames and config files).
- [Utils/Text Input.cpp](../Utils/Text%20Input.cpp): `PasteClipboardText`
  / `CopyToClipboard` (Win32 clipboard) gated; non-Windows stubs
  return 0 / no-op. Phase 4 wires them to `SDL_GetClipboardText` /
  `SDL_SetClipboardText`.
- 12+ `GetCursorPos` / `ScreenToClient` sites in screen-handler
  files (Ja2/FeaturesScreen, Ja2/gameloop, Laptop/BriefingRoom*,
  Laptop/laptop, Laptop/personnel, Laptop/MPChatScreen,
  Strategic/Map Screen Interface Bottom, Tactical/Turn Based Input,
  TileEngine/Tactical Placement GUI, Ja2/HelpScreen) all gated
  `_WIN32` with `gusMouseXPos` / `gusMouseYPos` fallback.

#### Phase 5 — video (DirectDraw → SDL3, keep RGB565 internally first)

- [sgp/video.cpp](../sgp/video.cpp): entire 3317-line file gated
  `_WIN32`. The whole DirectDraw video subsystem.
- [sgp/vsurface.cpp](../sgp/vsurface.cpp): entire 2803-line file
  gated `_WIN32`. DirectDraw surface manager.
- [sgp/DirectDraw Calls.cpp](../sgp/DirectDraw%20Calls.cpp) +
  [sgp/DirectX Common.cpp](../sgp/DirectX%20Common.cpp): wholly
  gated `_WIN32`.
- [sgp/DirectDraw Calls.h](../sgp/DirectDraw%20Calls.h): wholly gated
  `_WIN32` so the `<ddraw.h>` pull doesn't reach non-Windows.
- [sgp/ddraw.h](../sgp/ddraw.h), `ddraw.lib`, `fmodvc.lib`,
  `binkw32.lib`, `SMACKW32.LIB`, `mss32.lib` — only linked on
  Windows via the top-level CMakeLists `if(WIN32)`. Deleted in
  Phase 5 once SDL3 owns the surface stack.
- Phase 5 milestone: **game boots into main menu on macOS/Linux**,
  presenting the existing RGB565 framebuffer via `SDL_Texture` with
  `SDL_PIXELFORMAT_RGB565`. End-to-end at least one battle playable.

#### Phase 6 — RGBA8888 pipeline

- [sgp/vobject_blitters.cpp](../sgp/vobject_blitters.cpp): all 77
  inline-asm blocks gated `_WIN32`. **Game renders blank on non-Windows
  until Phase 6 rewrites these as portable C/SIMD** against a 32-bit
  RGBA framebuffer.
- [TileEngine/renderworld.cpp](../TileEngine/renderworld.cpp): 10
  more inline-asm blocks (sprite blitters with Z-buffer handling).
- [sgp/shading.cpp](../sgp/shading.cpp): single inline-asm block.
- [Ja2/local.h](../Ja2/local.h): `PIXEL_DEPTH 16` constant. Phase 6
  changes to 32 and propagates.
- The 8bpp→16bpp palette LUTs, shading tables, translucency tables,
  fade tables — all need RGBA8888 regenerations.
- `GetRGBDistribution()` in video.cpp goes away (only relevant for
  RGB565 mask detection).

#### Phase 7 — audio

- [Utils/dsutil.cpp](../Utils/dsutil.cpp): entire DirectSound utility
  gated `_WIN32`. Phase 7 replaces with SDL3_mixer or SoLoud /
  miniaudio glue.
- [sgp/soundman.cpp](../sgp/soundman.cpp): the JA2 sound API. Uses
  FMOD throughout. Phase 7 keeps the public API stable but swaps the
  backend.
- `fmodvc.lib` / `mss32.lib` linkage already conditional on `WIN32`.
- [sgp/Mss.h](../sgp/Mss.h) and `Mss-old.h`: Miles Sound System
  headers. Phase 7 deletes once FMOD/MSS callers are migrated.

#### Phase 8 — cinematics

- [Utils/Cinematics.h](../Utils/Cinematics.h) / `.cpp` (Smacker):
  entirely gated `_WIN32`. Phase 8 replaces with libsmacker (MIT,
  open-source decoder for original Smacker format).
- [Utils/Cinematics Bink.h](../Utils/Cinematics%20Bink.h) / `.cpp`
  (Bink): entirely gated `_WIN32`. Phase 8 must decide between
  asset-conversion (Smacker/WebM), FFmpeg-with-license, or just
  skipping Bink cutscenes on non-Windows.
- [Ja2/Intro.cpp](../Ja2/Intro.cpp): the intro-screen video player —
  entire body gated `_WIN32`. Not currently dispatched to from gameplay
  (the `MAP_EXIT_TO_INTRO_SCREEN` switch is commented out everywhere),
  so the gate is safe.
- `binkw32.lib` / `SMACKW32.LIB` linkage already conditional on `WIN32`.

#### Phase 9 — fonts

- [sgp/WinFont.cpp](../sgp/WinFont.cpp): entire GDI-backed TrueType
  font rasterizer gated `_WIN32`. Phase 9 replaces with `stb_truetype`
  (single-header, public domain).
- [sgp/WinFont.h](../sgp/WinFont.h): non-Windows branch declares
  inline no-op stubs for `InitWinFonts`, `CreateWinFont`,
  `SetWinFontForeColor`, `PrintWinFont`, `GetWinFontHeight`, etc.
  Phase 9 replaces these with the stb_truetype implementations.
- [Utils/Font Control.cpp](../Utils/Font%20Control.cpp): two
  `InitWinFonts` / `ShutdownWinFonts` call sites gated `_WIN32`.

#### Phase 10 — packaging + CI

- Top-level [CMakeLists.txt](../CMakeLists.txt): `add_executable(... WIN32 ...)`
  could grow `MACOSX_BUNDLE` and Linux variants. Pre-built `.lib`
  files (`binkw32.lib`, `lua51.lib`, `SMACKW32.LIB`,
  `libexpatMT.lib`) already gated `if(WIN32)`. Pulling Lua, Expat,
  Bink (when replaced), Smacker (when replaced) from source on every
  platform is Phase 10 cleanup.
- CI matrix for Windows / macOS / Linux. App bundle / AppImage / zip
  packaging. None of this exists yet.

### Deferred indefinitely — Multiplayer

- JA2 uses **RakNet 3.401** as a Win32 prebuilt `.lib`
  (`Multiplayer/raknet/RakNetLibStatic.lib`, 5.3 MB). The matching
  3.x source isn't on any public mirror — Jenkins Software's SVN
  died with the company. Facebook open-sourced only 4.x
  ([facebookarchive/RakNet](https://github.com/facebookarchive/RakNet))
  and 4.x has substantial API breaks from 3.x (`RakNetworkFactory`
  removed, `AutoRPC` removed, `RPC()` gone, `BitStream` semantics
  changed, types namespaced).
- [Multiplayer/mp_stubs.cpp](../Multiplayer/mp_stubs.cpp) provides
  no-op definitions for every external symbol JA2 references from
  the network wrapper (~50 globals like `is_networked` /
  `is_connected`, ~70 functions like `send_path` / `send_fire` /
  `NetworkAutoStart`, plus `CTransferRules`). The `Multiplayer`
  CMake target builds `mp_stubs.cpp` on non-Windows; on Windows it
  builds the three wrapper files and links RakNet as before.
- Reviving multiplayer on non-Windows = vendoring RakNet 4 + porting
  JA2's wrapper code (~327 RakNet API references) per the upstream
  `3.x_to_4.x_upgrade.txt`, **or** retargeting to a different netlib
  (SDL3_net, enet, asio). Multi-day project either way.

### Other clang-strictness fixes landed in Phase 1

MSVC-tolerated patterns clang rejects, fixed as we hit them:

- **MSVC `__int64`** → `int64_t`/`uint64_t` from `<cstdint>`.
- **Stray `typename`** in template argument lists (popup_callback.h,
  Singleton.h, several other places).
- **`POPUP_OPTION::POPUP_OPTION(void)`** self-qualification on inline
  declarations.
- **`static`/non-static mismatch**: forward decls without `static`
  but defs with `static` (DisplayCover.cpp, Music Control.cpp,
  soundman.cpp, lua_env.cpp, lua_tactical.cpp).
- **Out-of-line template-member redeclarations** without body
  (BaseTable.h, DropDown.h, MilitiaWebsite.cpp).
- **Explicit template specialization after implicit instantiation**:
  forward-declared specializations at top of file (MilitiaWebsite.cpp).
- **Narrowing in brace initializers**: per-entry casts to the target
  type (Isometric Utils.cpp, Soldier Control.cpp), or rewrite as
  assignments (AimSort.cpp, BobbyR.cpp).
- **Pointer-to-smaller-int casts** on 64-bit: intermediate
  `(uintptr_t)` cast acknowledges the truncation (Vehicles.cpp,
  Strategic Movement.cpp, Soldier Control.cpp, soundman.cpp,
  FileMan.cpp).
- **Pointer-to-bool / pointer-to-zero comparisons**: rewritten with
  `!= NULL` (XML_ComboMergeInfo.cpp, XML_ItemAdjustments.cpp). The
  `SGP_THROW_IFFALSE` macro switched from `== false` to `!(cond)`.
- **String-literal-macro concatenation** needs a space between
  literal and identifier in C++11 (Button System.cpp).
- **MSVC `swprintf(buf, fmt, ...)` 2-arg signature** vs POSIX 3-arg:
  resolved by the `swprintf` macro + template helper. ~3000 call
  sites covered automatically; ~120 pointer-buffer sites converted
  to explicit `sgp_swprintf(buf, count, ...)`.
- **`SoldierID` ambiguous conversion**: added `long` /
  `unsigned long` constructors so `lua_Integer` (which is `long` on
  clang/libc++) finds an unambiguous match.
- **`_array` reserved name + dependent-base access** in
  sgp_auto_memory.h: dropped extra `typename`, added `this->_array`.
- **`Exception::what()` exception spec mismatch**: added `throw()`
  to definition to match header.
- **VFS macOS support**: ext/VFS/include/vfs/Core/vfs_types.h
  switched `__linux__` branch to default-non-Windows so Apple
  picks up the `<stdint.h>` typedefs.
- **libpng macOS support**: ext/libpng/pngconf.h classic-MacOS
  `<fp.h>` branch gated on `!__APPLE__`.
- **zlib macOS support**: ext/zlib/zutil.h classic-MacOS `fdopen`
  stub gated on `!__APPLE__`; gzguts.h includes `<unistd.h>` on
  non-Windows.

---

## Phase 0 — Branch & baseline ✅

Done.

- [docs/SDL3_PORT.md](SDL3_PORT.md) — this doc, committed.
- `sdl3-port` branch created from `master`; no `master` changes.

---

## Phase 1 — Build system portability ✅

**Stated goal**: configures on macOS/Linux.
**Actual outcome**: configures AND compiles AND links 100% on macOS,
producing a runnable (no-op) `JA2_ENGLISH` executable. Took ~50 commits
because of the extensive Win32 surface across `sgp/`, the multiple
inline-asm blocks, the Multiplayer/RakNet situation, and AppleClang's
strictness vs MSVC.

What landed:

- Build system: pre-built `.lib` files gated `if(WIN32)`. `wine/`
  subdir excluded from non-Windows. Resource compiler (`.rc`)
  excluded.
- Type portability: `sgp/types.h` uses `<cstdint>` instead of MSVC
  `__int64`. Profiler ditto.
- Vendored libs: bfVFS, libpng, zlib teach themselves macOS.
- The whole gating/stub layer described above (msvc_compat.h + ~50
  source files).

Build verification on macOS:

```
$ cmake -S . -B build -DLanguages=ENGLISH -DApplications=JA2
-- Configuring done
-- Generating done
$ cmake --build build -j 4
[100%] Built target JA2_ENGLISH
$ ./build/JA2_ENGLISH
JA2 SDL3 port: non-Windows entry point not yet implemented.
This stub exists so the build links; Phase 3 wires SDL3 in.
```

Exit criteria met. Phase 2 begins.

---

## Phase 2 — Portable I/O, time, memory, debug

Strip Win32 dependencies out of the lowest layers of SGP so higher
layers can be ported without dragging Windows in transitively. Replace
compat-stub no-ops with real implementations.

**Status update (2026-05-16):** the planned `FileMan.cpp` rewrite is
substantially smaller than expected. FileMan.cpp **does not call any
Win32 file API directly** — it routes everything through `vfs::*` /
`getVFS()`, and bfVFS (`ext/VFS/src/Core/vfs_os_functions.cpp`) already
has a complete POSIX branch (`scandir`, `getcwd`, `mkdir`, `remove`,
`chdir`). The only macOS-specific gap was `getExecutablePath` using
Linux's `readlink("/proc/self/exe")` — fixed by switching to
`_NSGetExecutablePath` from `<mach-o/dyld.h>` under `__APPLE__`. The
compat-layer `CreateFileA` / `ReadFile` / `WriteFile` / `HFILE` stubs
in msvc_compat.h are **not actually exercised** by FileMan; they exist
only so legacy unported translation units still compile.

**Concrete files to port:**

1. ~~[sgp/FileMan.cpp](../sgp/FileMan.cpp) — replace `CreateFile` /
   `ReadFile` / `WriteFile` / `SetFilePointer` / `GetFileSize` /
   `WIN32_FIND_DATA` enumeration with `std::ifstream` /
   `std::ofstream` / `std::filesystem`.~~ **Not needed** — already
   abstracted through bfVFS, which has working POSIX paths. macOS
   exec-path gap patched.
2. ~~[sgp/LibraryDataBase.cpp](../sgp/LibraryDataBase.cpp) — port the
   SLF archive reader. Endianness audit on the SLF header.~~
   **Not needed.** bfVFS has its own SLF reader
   (`ext/VFS/src/Ext/slf/vfs_slf_library.cpp`, enabled by `VFS_WITH_SLF`)
   and that's what `InitVirtualFileSystem` actually mounts. The legacy
   `LibraryDataBase.cpp` is only kept alive by a single
   `ShutDownFileDatabase()` cleanup call which is a no-op against an
   uninitialised database. bfVFS's reader does no byte-swapping —
   correct for all current LE targets (x86_64, arm64).
3. ~~[Utils/Timer Control.cpp](../Utils/Timer%20Control.cpp) — rebuild
   the clock + notify threads on `std::thread` + `std::chrono` +
   `std::mutex` + `std::condition_variable`. Replace `timeSetEvent`
   periodic callback with a scheduler thread.~~ **Done.** Rewrite
   replaces every Win32 primitive (`timeSetEvent` / `CreateThread` /
   `CreateEvent` / `CRITICAL_SECTION` / `QueryPerformanceCounter` /
   `SEH __try`) with portable C++11. Single implementation for every
   platform — no `_WIN32` gate. Public API + globals unchanged.
4. ~~[sgp/MemMan.cpp](../sgp/MemMan.cpp) — drop the Win32 heap
   debug-tracking branch entirely.~~ **Done.** Removed
   `VirtualAlloc`/`VirtualLock` from `MemAllocLocked`/`MemFreeLocked`
   (collapses to `malloc`/`free`) and dropped the
   `MEMORYSTATUS`/`GlobalMemoryStatus` call in
   `MemGetFree`/`MemGetTotalSystem` (now returns 0; only used for
   diagnostic ScreenMsg). The MSVC `_DEBUG` leak-tracking branch is
   left in place — it only activates on MSVC debug builds and gives
   real value there. The corresponding stubs in `msvc_compat.h` were
   dropped along with the call sites.
5. ~~[sgp/DEBUG.cpp](../sgp/DEBUG.cpp), [sgp/debug_win_util.cpp](../sgp/debug_win_util.cpp) — portable logging (`sgp_logger` already
   handles most of it). The stack-trace helpers in
   `debug_win_util.cpp` should be gated `_WIN32`; non-Windows can use
   `<stacktrace>` (C++23, not universally available yet) or libunwind.~~
   **Done.** `debug_win_util.cpp` was already `_MSC_VER`-gated (compiles
   to an empty TU on clang/gcc). Added a portable `StackTrace::StackTrace`
   / `PrintBacktrace` / `OutputToStream` body in `debug_util.cpp` that
   uses `<execinfo.h>` (`backtrace` + `backtrace_symbols`, available on
   macOS and glibc Linux). `DEBUG.cpp`'s `<windows.h>` include + the
   inline Win32 message pump in `_FailMessage` were already
   `_WIN32`-gated; `OutputDebugString` routes to stderr via the compat
   layer.
6. ~~[sgp/timer.cpp](../sgp/timer.cpp) — replace the `SetTimer`-driven
   game clock with a `std::chrono`-driven equivalent. Tiny file.~~
   **Done.** Dropped the SetTimer/KillTimer dance; `GetClock()` now
   samples `std::chrono::steady_clock` directly.
7. ~~Delete `wine/`.~~ **Done.** The Wine cnc-ddraw override is
   obsolete now that SDL3 replaces DirectDraw everywhere. Removed the
   subdirectory, its CMakeLists entry, and the call site in
   `sgp.cpp`'s WinMain.
8. Remove the now-redundant Win32 file I/O stubs from msvc_compat.h.

**Exit criterion**: the non-video, non-input, non-audio parts of SGP
do useful work on non-Windows. File I/O succeeds, the clock advances,
SLF archives load.

---

## Phase 3 — SDL3 window, event loop, and message plumbing

Bring SDL3 in. This is the architectural shift the whole branch is
named after.

**Build system:** ✅ already landed.

- SDL3 is pulled in via `find_package(SDL3 CONFIG)` with a
  `FetchContent` fallback against `release-3.2.16`. Linked to the
  executable on every platform.
- Expat is built from source on every platform now too (the prebuilt
  `libexpatMT.lib` is dropped from the Windows Ja2_Libraries list).
  This means non-Windows builds get a working XML parser without
  any platform-specific binary blobs.

**Source changes:**

1. ✅ Done (minimal): Non-Windows `main()` in [sgp/sgp.cpp](../sgp/sgp.cpp)
   now initializes SDL3, opens a window, runs an `SDL_PollEvent`
   loop that exits on quit / Escape, presents an RGB565
   `SDL_Texture` with a test pattern, and translates
   `SDL_EVENT_KEY_*` / `SDL_EVENT_MOUSE_*` events into JA2 event
   codes via a local stub. **Still TODO**: route the translated
   events into JA2's real `QueueEvent`, call `GameLoop` from the
   loop, point the SDL_Texture at the real framebuffer.

**The link-cascade gotcha** (learned the hard way): pulling
JA2_sgp.a's `input.cpp` into the executable's link surface (via
`extern "C" void QueueEvent(...)`) drags ~57 unresolved function
symbols with it -- every video/vsurface/intro/winfont/sound/debug
function that other (un-gated) translation units call. Stubbing all
57 in one shot is fragile (signature mismatches, includes that pull
in DirectDraw types, etc.). The right path is to land the SDL3-
backed rewrites of those subsystems first (Phases 4 proper / 5 /
8 / 9), letting each one delete its `_WIN32` gate as it goes.
Then `QueueEvent` is reachable naturally.

Foundation pieces already in place to support that work:
- [sgp/sgp_non_win32_globals.cpp](../sgp/sgp_non_win32_globals.cpp)
  defines the bare *global variables* that gated subsystems
  exposed via extern. Each phase moves its globals out of this
  stub and into the new SDL3 implementation.
- SDL3, Expat, and Lua 5.1.5 all build from source on every
  platform now; no Windows-only `.lib` blobs in the link path.
2. Replace `MessageBoxW` / `MessageBox` call sites with
   `SDL_ShowSimpleMessageBox`. ~20-30 sites; mostly in
   FatalError paths.
3. Replace `SetCursor` / `ShowCursor` with
   `SDL_HideCursor` / `SDL_ShowCursor` / `SDL_SetCursor`.
4. Introduce a `Ja2GetWindow()` accessor that returns `SDL_Window*`
   on non-Windows and `HWND` on Windows (or unify on `SDL_Window*`
   and let DirectDraw take it via `SDL_GetWindowProperty`).
5. Replace cnc-ddraw detection logic (the `bCncDdraw` path in
   sgp.cpp) with `SDL_GetCurrentVideoDriver()` checks if needed.
6. Honour `iScreenMode` via `SDL_WINDOW_FULLSCREEN` /
   `SDL_WINDOW_RESIZABLE` etc.

**Exit criterion**: window opens on all three OSes; mouse and
keyboard events route into the event queue; game still renders via
DirectDraw on Windows; game runs to main menu (rendering blank but
not crashing) on macOS.

---

## Phase 4 — SDL3 input

Replace DirectInput / Win32 keyboard & mouse messages with SDL events.

**Status (2026-05-16):** ✅ done end-to-end. SDL_Event → JA2 event
translation lives in [sgp/sdl_input.cpp](../sgp/sdl_input.cpp) +
sdl_input.h. `sgp.cpp`'s main calls `SgpHandleSDLEvent()` which calls
input.cpp's real `QueueEvent` and updates `gfKeyState` /
`gfLeftButtonState` / `gusMouseXPos` / `gsMouseWheelDeltaValue`
directly. The local stderr debug stub is gone.

To clear the 55-symbol link cascade that originally blocked this
wire-up:
1. Stub bodies for video.cpp / vsurface.cpp / Intro.cpp / soundman /
   mousesystem / KeyMap symbols live in
   [sgp/sgp_non_win32_stubs.cpp](../sgp/sgp_non_win32_stubs.cpp) — all
   declared via public headers (vsurface.h, vobject_blitters.h,
   video.h, Intro.h, soundman.h, KeyMap.h, connect.h) so they don't
   need to include `vsurface_private.h` (the one that pulls in
   DirectDraw). As each Phase replaces a subsystem, its stub block
   here gets deleted.
2. 13 cross-library duplicate symbols dedup'd: 9 from
   `Multiplayer/mp_stubs.cpp` (real bodies already exist in JA2 libs;
   stubs removed), and 4 genuine legacy collisions where two
   independent TUs each defined a same-named UI/data global —
   `guiNextButton` / `guiPrevButton` (SaveLoadScreen.cpp vs
   mercs Files.cpp), `GlowColorsA` (mapscreen.cpp vs IMP Gear.cpp,
   different types!), `guiGalleryButton` (florist.cpp vs
   florist Gallery.cpp, scalar vs array) — fixed by making each
   definition `static` since no other TU extern's them.

**Source changes:**

1. [sgp/input.cpp](../sgp/input.cpp) — `KeyboardHandler` /
   `MouseHandler` Win32 hook procs replaced with an
   `SDL_EVENT_KEY_DOWN` / `SDL_EVENT_KEY_UP` /
   `SDL_EVENT_MOUSE_*` dispatch driven by Phase 3's `SDL_PollEvent`
   loop. **First cut done** in `sdl_input.cpp` (translation layer);
   full wire-up to `input.cpp::QueueEvent` blocked on Phase 5.
2. [Utils/KeyMap.cpp](../Utils/KeyMap.cpp) — rebuild the keycode
   translation. **The numeric VK_* values must be preserved** for
   savegame compatibility (they're persisted in hotkey config). Build
   a translation table from `SDL_Scancode` to the existing VK_*
   values.
3. Mouse handling: ScreenToClient-style coordinate conversions
   become `SDL_GetMouseState` calls (which already return window-
   coordinate). Drop the `gusMouseXPos` / `gusMouseYPos` shim once
   SDL is authoritative.
4. `SDL_StartTextInput` / `SDL_StopTextInput` around the dialogs
   that take typed names (use `gpCurrentStringDescriptor` as the
   gate signal).
5. Clipboard: `SDL_GetClipboardText` / `SDL_SetClipboardText` in
   [Utils/Text Input.cpp](../Utils/Text%20Input.cpp).
6. Cursor clipping: SDL3 has `SDL_SetWindowMouseRect`. Replace
   `RestrictMouseCursor` / `ClipCursor`-using sites.
7. Delete the `HHOOK ghKeyboardHook` / `ghMouseHook` globals and
   any other Win32-only input plumbing.

**Exit criterion**: keyboard, mouse, and clipboard fully functional
on all three platforms. Game playable through menus.

---

## Phase 5 — SDL3 video, transitional RGB565

Replace DirectDraw with SDL3 rendering while keeping the existing
RGB565 internal pipeline. **This is the milestone where macOS and
Linux first see pixels.**

**Status (2026-05-16):** first slice landed
([sgp/sdl_video.cpp](../sgp/sdl_video.cpp)). The SDL3 video manager
runs on macOS/Linux only for now — still gated `#ifndef _WIN32` while
the legacy DirectDraw `video.cpp` keeps Windows working. Flipping
Windows over to the same SDL3 path is the next slice (Phase 5b
below).

**Source changes:**

1. ~~Rewrite [sgp/video.cpp](../sgp/video.cpp) — at this scale,
   easier to rewrite than to port.~~ **Done as
   [sgp/sdl_video.cpp](../sgp/sdl_video.cpp).** Public API
   preserved: InitializeVideoManager / Lock(Frame|Back|Primary|Mouse)Buffer /
   InvalidateRegion(s|Ex|Screen) / RefreshScreen / Set8BPPPalette /
   GetCurrentVideoSettings / GetPrimaryRGBDistributionMasks /
   StartFrameBufferRender / EndFrameBufferRender / VideoCaptureToggle /
   PrintScreen / FatalError / EraseMouseCursor /
   SetMouseCursorProperties / etc. Heap UINT16* buffers behind the
   Lock entry points; `SDL_UpdateTexture` + `RenderTexture` +
   `RenderPresent` in RefreshScreen.
2. Same treatment for [sgp/vsurface.cpp](../sgp/vsurface.cpp).
   **Done as [sgp/sdl_vsurface.cpp](../sgp/sdl_vsurface.cpp)**.
   - Pure-C++ pieces (ClipRectangle, SurfaceData registries).
   - SGPVSurface manager: NewSurface/DeleteVideoSurface lifecycle;
     SetPrimaryVideoSurfaces wraps the four heap buffers
     sdl_video.cpp owns; InitializeVideoSurfaceManager /
     ShutdownVideoSurfaceManager; AddStandardVideoSurface /
     CreateVideoSurface (empty + from-file via HIMAGE/CopyImageToBuffer);
     GetVideoSurface / GetVideoSurfaceDescription /
     DeleteVideoSurfaceFromIndex; Lock/UnLockVideoSurface (dispatches
     PRIMARY/BACK/FRAME/MOUSE to sdl_video.cpp's Lock*Buffer);
     SetVideoSurfaceTransparency / Palette / GetVSurfacePaletteEntries;
     Region-list helpers.
   - CPU blitters: BltVideoSurfaceToVideoSurface (memcpy + colour-key
     loop), BltVideoSurface (Get + dispatch), BltStretchVideoSurface
     (nearest-neighbour), BltVSurfaceUsingDD (alias to the CPU blit
     path), ColorFillVideoSurfaceArea (direct rect fill).
   - Still returning FALSE: ImageFillVideoSurfaceArea (tile-fill),
     the ShadowVideoSurface* family (alpha shadow blends). These
     wait for Phase 6's RGB565 blender retirement.
3. ~~Delete [sgp/DirectDraw Calls.cpp](../sgp/DirectDraw%20Calls.cpp),
   [sgp/DirectX Common.cpp](../sgp/DirectX%20Common.cpp),
   [sgp/ddraw.h](../sgp/ddraw.h), `ddraw.lib`.~~ **Done** (Phase 5b
   commit 0271f6d9). The .cpp files are removed from sgpSrc;
   ddraw.lib dropped from the Windows Ja2_Libraries link list.
   sgp/ddraw.h still on disk but no longer referenced.
4. Delete cnc-ddraw detection (the `bCncDdraw` path). **Pending**
   — lives inside `WinMain` which gets rewritten in the WinMain-on-
   SDL_main slice.
5. ~~Delete the `ADDTEXT_16BPP_REQUIRED` error path~~ **Done
   implicitly** — sdl_video.cpp doesn't have it.
6. The inline-asm RGB565 alpha blender at
   [sgp/vobject_blitters.cpp:19-60](../sgp/vobject_blitters.cpp#L19-L60)
   needs replacement with portable C — keep RGB565 math, just stop
   using `__asm`. **Pending** — current `_WIN32` gating means the
   asm only kicks in on MSVC, so non-Windows builds already use the
   portable fallback. Phase 6 retires it entirely.

### Phase 5b — structural flip: SDL3 is the only video path (✅ done 2026-05-16)

Commits `0271f6d9` (the structural flip) + `ed98c9b6` (rename
sgp_non_win32_* → sgp_portable_*). User signaled they're not
verifying the Windows build for now — the flip is purely structural,
intended to make the codebase architecturally correct ("SDL3
everywhere") while macOS continues to be the verification platform.

What landed:

1. ~~Drop the `#ifndef _WIN32` body gate from `sdl_video.cpp`~~ done.
   No HINSTANCE-overload added — the WinMain rewrite below makes
   that overload unnecessary, so we just drop it.
2. ~~Remove `sgp/video.cpp`, `sgp/vsurface.cpp`,
   `sgp/DirectDraw Calls.cpp`, `sgp/DirectX Common.cpp` from
   `sgpSrc`.~~ done.
3. ~~Add `sdl_video.cpp` + `sgp_portable_globals.cpp` +
   `sgp_portable_stubs.cpp` to `sgpSrc` unconditionally.~~ done.
4. ~~Drop `ddraw.lib` from the Windows `Ja2_Libraries` list.~~ done.
5. ~~`WinMain` in [sgp/sgp.cpp](../sgp/sgp.cpp) at ~line 690 still owns
   ~900 lines of Win32 plumbing.~~ **Done** (commit `e2e93b60`).
   `WinMain` / `HandledWinMain` / `WindowProcedure` /
   `SyncWindowProcedure` / `CreateStandardGamingPlatform` /
   `SGPExceptionFilter` retired (gated `#if 0` for archeology).
   `InitializeStandardGamingPlatform` is no-arg now; the void
   `InitializeVideoManager` is the only video init.
   `RegisterWindowMessage(MSH_MOUSEWHEEL)` gone (SDL_EVENT_MOUSE_WHEEL).
   `CRITICAL_SECTION gcsGameLoop` → `std::mutex gGameLoopMutex`.
   `__try`/`__except` SEH replaced by plain `try`/`catch` in
   `SafeSGPExit` + `CallGameLoop`. `SGPExit` MessageBox → stderr.
   Win32 `GetCommandLineW` parse rewritten as portable argc/argv
   scan. Unified `int main(int, char**)` drives `SDL_PollEvent` +
   `CallGameLoop` directly. macOS arm64 build clean; Windows builds
   need to absorb the remaining `_WIN32`-only references in
   WinFont/vobject_blitters as part of items 6+7 below.
6. `WinFont.cpp`'s DD-using GDI text-rendering block
   ([sgp/WinFont.cpp:425-466](../sgp/WinFont.cpp#L425-L466)) calls
   `GetVideoSurfaceDDSurface` / `IDirectDrawSurface2_GetDC` /
   `TextOutW` — disable this block (the SDL_ttf / stb_truetype Phase
   9 replacement gives portable text rendering). Currently still
   `#ifdef _WIN32`-gated; will fail to compile on Windows now that
   the DD getters are gone, but Windows builds aren't being verified.
7. `vobject_blitters.cpp` inline-asm blocks. 14 ports landed so
   far (commits `800f2bc2` through `6d7a6450`):
   - `blendWithAlpha` (real RGB565 alpha math)
   - `Blt16BPPTo16BPP`, `Blt16BPPTo16BPPMirror` (16bpp opaque /
     mirror memcpy)
   - `Blt8BPPTo8BPP` (8bpp row memcpy)
   - `Blt8BPPDataTo16BPPBufferTransparent{,Clip}` (UI sprite,
     unclipped + clipped)
   - `Blt8BPPDataTo16BPPBufferTransZ{,NB,Clip,NBClip}` (Z-tested
     sprite, with/without Z update, unclipped + clipped -- workhorse
     for game-world tile rendering)
   - `Blt8BPPDataTo16BPPBufferTransMirror` (left/right sprite flip)
   - `Blt8BPPDataTo16BPPBufferMonoShadowClip` (font rasterizer
     glyph blit)
   - `Blt8BPPDataTo16BPPBufferShadowZ{,NB,Clip,NBClip}` (Z-tested
     darken via ShadeTable[], with/without Z update, unclipped +
     clipped -- merc/object shadows)

   Convention adopted commit `9928b26f`: drop the `#ifdef _WIN32`
   gating entirely when porting. The asm bodies are dead code on
   every platform we're building (clang doesn't support MSVC
   inline asm; Windows isn't being verified), so future ports just
   delete the asm block and paste a portable ETRLE decoder where
   it was. Typical diff: ~30 lines added, ~170 lines removed.

   Additional ports (cumulative through commit `ff151100`):
   - `TransShadow{,Z,ZNB,Clip,ZClip,ZNBClip,ClipAlpha,ZClipAlpha,
     ZNBClipAlpha}` (merc-with-shadow, all clip/Z/alpha combinations)
   - `TransZ{,NB,Clip,NBClip}Translucent` (50% lo-bit-strip blend)
   - `TransZ{,NB,Clip,NBClip}Pixelate` + `TransZPixelateObscured`
     (checkerboard)
   - `TransZNBColor`, `TransZNBClipColor` (silhouette tint)
   - `Blt16BPPDataTo16BPPBufferTransZClip` (8bpp ETRLE despite name)
   - `Blt16BPPDataTo16BPPBufferTransparentClip` (true 16bpp ETRLE)
   - `TransShadowAlpha`, `TransShadowZAlpha`, `TransShadowZNBAlpha`
     (parallel alpha-mask stream + blendWithAlpha per pixel)
   - `TransShadowZNBObscured{,Test,Alpha,Clip,ClipAlpha}`
     (in-front normal sprite + obscured checkerboard silhouette,
     all alpha/clip combinations)
   - Residual `#ifdef _WIN32` dropped from `blendWithAlpha` and
     `Blt16BPPTo16BPP` (toehold cleanup).

   **~31 inline-asm blocks remain** out of the original ~76 -- the
   file shrunk from 15057 to 10654 lines, ~4400 lines of dead asm
   purged. Remaining categories: BlitZRect (Z buffer fill);
   Blt8BPPDataTo16BPPBuffer{,Half,HalfRect,Mask,Shadow,ShadowClip,
   FullTransparent,MonoShadow}; Blt8BPPDataSubTo16BPPBuffer;
   Blt16BPPBufferPixelateRectWithColor /
   Blt16BPPBufferShadowRect{,AlternateTable}; FillRect16BPP;
   eight `Outline*` variants; six `Intensity*` variants;
   TransShadowBelowOrEqualZNBClip; TransZClipPixelateObscured.
   Pattern is fully mechanical -- each is the ETRLE row decoder
   with different per-pixel logic; future sessions can finish them.

**Exit criterion (Phase 5 full)**: game boots into main menu on all
three platforms and renders correctly via the existing RGB565
pipeline. End-to-end at least one battle playable.

---

## Phase 6 — RGBA8888 pipeline conversion

Now that everything routes through SDL, swap the internal pixel
format.

### 6a. Inline-asm retirement (✅ done)

All inline x86 `__asm { }` blocks in `sgp/vobject_blitters.cpp` were
replaced with portable C ETRLE decoders. The file shrank from ~15057
lines to ~8100, and `grep __asm sgp/vobject_blitters.cpp` returns
zero hits. Each port is its own commit so it can be diff'd against
the legacy asm for review. Covered variants include the basic
`Transparent` / `TransZ` / `TransZNB`, the `Outline` family (8
variants, with the `254 == outline marker` semantics preserved), the
`Intensity` family (6 variants), `MonoShadow`, `PixelateRectWithColor`,
`Shadow*`, `Half` / `HalfRect`, `Mask` (which the legacy asm never
actually used as a mask — dead-code preserved), and the
`PixelateObscured` variants (front-facing pixels render normally and
update Z; obscured pixels render only on a checkerboard mask, no Z
update).

The 8bpp→16bpp palette LUT (`p16BPPPalette`) is still RGB565 and the
Z-buffer is still `UINT16`. Pixel-format conversion to RGBA8888 is
the remaining Phase 6 work.

The remaining inline-asm sites outside `vobject_blitters.cpp` are also
gone:

- `sgp/shading.cpp` `FindIndecies` (a brute-force nearest-color
  palette lookup) is now portable C.
- `DebugBreakpoint()` in `sgp/DEBUG.H` switched from `__asm { int 3 }`
  to `__builtin_debugtrap` / `__debugbreak`.
- The vendor headers (`sgp/RAD.H`, `sgp/Mss.h`, `sgp/Mss-old.h`) still
  hold `__emit` / `int 3` macros but nothing in the live build
  includes them; they get deleted alongside their subsystem in Phase
  7/8.

Same pass deleted the dead DirectDraw subsystem files (their SDL3
replacements have been in place since Phase 5):

- `sgp/video.cpp` (~3300 lines, entirely `#ifdef _WIN32`)
- `sgp/vsurface.cpp` (~2800 lines, ditto)
- `sgp/DirectDraw Calls.cpp` and `sgp/DirectX Common.cpp` (not in the
  build; replaced by the SDL3 manager files)

The matching `*.h` files survive for now -- a few of the typedefs
(`LPDIRECTDRAWSURFACE2`, etc.) are still referenced from `WinFont.cpp`
and `vsurface_private.h`, so they get retired alongside Phase 9 font
work. A pile of leftover `#ifdef _WIN32 #include <windows.h> #endif`
gates in small SGP files (`Font.cpp`, `mousesystem.cpp`, `DEBUG.cpp`,
`Button System.cpp`, `FileMan.cpp`, `LibraryDataBase.cpp`) was also
stripped where the file no longer uses anything from `<windows.h>`,
and `Random.cpp`'s legacy `GetCursorPos` entropy was replaced with
`SDL_GetGlobalMouseState`.

### 6b. RGBA8888 conversion (pending)

1. Change `PIXEL_DEPTH` in [Ja2/local.h](../Ja2/local.h) from 16 to 32.
2. Wide-rename `UINT16* pBuffer` → `UINT32* pBuffer` across every
   blitter and caller. Adjust pitch math (bytes-per-pixel doubles).
3. Regenerate the 8bpp→32bpp palette LUTs in
   [sgp/himage.cpp](../sgp/himage.cpp). The shading, fade-to-black,
   fade-to-white, translucency, and night-vision tables in
   [sgp/shading.cpp](../sgp/shading.cpp) all need RGBA8888
   equivalents.
4. Rewrite the portable-C blitter inner loops to operate on `UINT32`
   pixels; replace `blendWithAlpha` and `IntensityTable` with portable
   per-channel arithmetic (consider SDL_SIMD intrinsics).
5. Update SDL texture format to `SDL_PIXELFORMAT_ARGB8888` or
   matching endianness.
6. Re-add the saveable surfaces / screenshot writer (the original TGA
   path lived in the now-deleted `sgp/video.cpp`; needs to live in
   `sgp/sdl_video.cpp` or a sibling).
7. Z-buffer stays `UINT16` (it's a depth value, not a color).

**Risk**: this is the phase where game-rendering regressions hide.
Plan golden-image regression testing — render a known scene under
Phase 5 (tag the commit) vs Phase 6, diff the buffers.

**Exit criterion**: game is visually identical (or better) to Phase 5
on all three platforms.

### 6c. Game-data load + first runtime pass (✅ for boot, ⚠️ for render)

The macOS binary now boots all the way through asset loading and into
the SDL3 main loop. Key fixes during the first run-through:

- **HWFILE was `UINT32`** but `FileMan.cpp` packs a `vfs::IBaseFile*`
  into it; the cast truncated the top 32 bits of every file handle
  and `FileClose` segfaulted on the first opened file. Widened to
  `uintptr_t`. (commit bee8c0e0)
- **IntroScreenInit stub** returned 0, which the screen-registration
  loop treats as fatal. Now returns 1; `IntroScreenHandle` sets
  `gfDoneWithSplashScreen = TRUE` and returns `INIT_SCREEN` (matches
  the legacy `INTRO_SPLASH` path). (commit 04ca186a)
- **Modern expat's billion-laughs DoS protection** halted
  `AnimationSurfaces.xml` parsing halfway through (the data file
  legitimately expands ~15 large external entities). Forward-declare
  and call `XML_SetBillionLaughsAttackProtectionMaximumAmplification`
  in `Tactical/LogicalBodyTypes/AbstractXMLLoader.cpp`. (commit
  04ca186a)
- **`sizeof(pointer) * 256` palette copies smashed the heap** on
  64-bit (worked on 32-bit Windows by coincidence). Swept across 6
  files in `Tactical/`. (commit 18b0a59f)
- **Quit-path backstops**: `SIGINT`/`SIGTERM` handlers, plus `ESC`
  and `Cmd+Q` in the SDL event handler. `FatalError` switched from
  `abort()` to `exit(1)` so controlled exits don't pop a crash
  dialog.

What still isn't right (next session):

- **Black window in practice** (resolved 2026-05-17). The two
  culprits turned out to be (a) `gusRedMask`/`gusGreenMask`/
  `gusBlueMask` plus `gusRedShift`/`gusGreenShift`/`gusBlueShift`
  in himage.cpp were declared but never assigned, so every
  `Get16BPPColor()` returned ~0 (black) or, with masks-only,
  blue-tinted; (b) `RefreshScreen` only uploaded the legacy
  `InvalidateRegion` dirty rect, but the streaming SDL3 texture's
  initial state is undefined so anything drawn before the first
  dirty rect stayed invisible. Both fixed in
  [sgp/sdl_video.cpp](../sgp/sdl_video.cpp) (commit cceda682).
  Main menu now renders.
- **Screen-transition leftovers**. A single leftover character from
  the InitScreen state-0 version text (`mprintf(10, 10, ...)`) is
  still visible in the top-left after the main menu loads, and the
  JA2 1.13 logo on the main menu shows an opaque black rectangle
  around the letters where it should be transparent (the waving
  flag should show through between the letters). Both look like
  the same underlying problem: the legacy DirectDraw double-buffer
  flip pattern gave each frame a fresh back buffer, and the SDL3
  single-buffer port accumulates whatever the previous frame left
  in `gFrameBuffer`. The fix probably involves either clearing
  `gFrameBuffer` on a specific transition signal (e.g. when
  `InvalidateScreen` fires AND the current screen id has changed),
  or honouring `BACKBUFFER` separately and copying it into the
  frame buffer on present.
- **macOS "broken application" crash report when closing via the
  red X**. The `SDL_EVENT_WINDOW_CLOSE_REQUESTED` path returns true
  and flips `gfProgramIsRunning` — but something on the way to
  `exit(0)` (most likely in `SGPExit()` registered via `atexit`, or
  in the timer-thread `NotifyThreadMain` which races against
  shutdown) is hitting UB. Add a clean shutdown sequence that joins
  the timer thread, calls `ShutdownVideoManager`, and only then
  returns from `main()`.
- **Input wired through** (resolved 2026-05-18, commit 25bf772d).
  `sdl_input.cpp` now updates `gusMouseXPos`/`gusMouseYPos` on motion
  but does NOT call `QueueEvent(MOUSE_POS, ...)`. The legacy
  `DequeueSpecificEvent` peeks the queue head and bails on a mask
  mismatch *without* advancing past the offending atom, so a
  `MOUSE_POS` at the head pinned every `LEFT_BUTTON_DOWN/UP` behind
  it forever. Hover state worked (the game polls the globals
  directly); only clicks were stuck. With the change clicks reach
  the button hooks -- "Start New Game" successfully navigates to the
  INITIAL GAME SETTINGS screen on macOS.

### 6d. Text rendering / font system (next session)

Now that the game is fully interactive, the remaining first-class
issue is text quality:

- **Top-left 'A' glyph leftover.** `GetIndex` returns 0 (the 'A'
  glyph slot) for any wchar_t not found in the legacy English
  translation table -- a debugging marker the original devs left in
  on purpose. On macOS this fires for some characters that
  presumably matched on Windows; the cause is most likely a
  `wchar_t` width mismatch (32-bit on macOS vs 16-bit on Win32) in
  a code path that reads the source string element-by-element.
- **Truncated button labels** ("Save Anyti" instead of "Save
  Anytime", "1.13 Feat" instead of "1.13 Features"). Same flavour
  -- some text widths reach a limit early. Probably a
  fixed-size-buffer copy that confuses byte-count for char-count, or
  a font-width measurement that walks the string as `char[]` rather
  than `wchar_t[]`.
- **Empty dialog message panels.** Yes/No confirm dialogs render
  their buttons but the body text is blank. Likely the `vswprintf`
  call path produces an empty string for those format/arg combos on
  POSIX (`%s` vs `%S` semantics for wchar_t arguments differ from
  Microsoft's vswprintf -- the InitScreen version line had the same
  problem in earlier traces).

Concrete next steps for a focused session:

1. Audit `vswprintf` and `swprintf` call sites for `%s`/`%S`
   correctness against the actual wchar_t / char* arg types.
   Microsoft's printf treats `%s` as wchar_t* in wide-printf
   functions; POSIX treats it as char*. The opposite is true for
   `%S`. Most legacy JA2 format strings use `%s` for wide args
   because they were authored on Windows.
2. Sweep for sites that compute string lengths in bytes vs chars
   (`strlen` on a wchar_t buffer; `sizeof(buf)` where char-count was
   meant).
3. Check `GetIndex` traffic to see which specific characters fail
   to find a translation. If it's punctuation or non-ASCII, extend
   the English table. If it's plain ASCII letters, the issue is
   higher up the chain (the input wchar value is wrong before it
   ever reaches the lookup).

**Prior art — ja2-stracciatella.** Their port went the wholesale
route: dropped `wchar_t` and `vswprintf` entirely, switched to a
custom `ST::String` UTF-32 type (`char32_t`-based), and rewrote
`GetGlyphIndex(char32_t c)` to look up a translation table keyed on
Unicode codepoints with a `'?'` fallback rather than the legacy 'A'
fallback. That's the "correct" long-term shape but it's a
multi-thousand-call-site sweep, far beyond a single phase. A
pragmatic interim for 1.13 would be:

- A `vswprintf` shim that pre-translates Microsoft's wide-printf
  `%s` (= wchar_t*) to POSIX `%S` and vice versa, so the legacy
  format strings keep working without touching every call site.
- Switch the `'A'` GetIndex fallback to a less visually distracting
  glyph (or a question mark) until the chars-not-in-table set is
  enumerated.

A worktree-scale experimental shim is the right way to scope this.

---

## Phase 7 — Audio: SFX & music

Replace FMOD / Miles Sound System.

- Pick: SDL3_mixer vs SoLoud vs miniaudio. Bias toward SDL3_mixer
  (already on SDL3). Audit feature coverage first — positional audio?
  arbitrary channel counts? low-latency triggering?
- Replace [sgp/soundman.cpp](../sgp/soundman.cpp) backend; keep public
  API stable.
- Audit MSS-isms in [sgp/Mss.h](../sgp/Mss.h) / `Mss-old.h` — many
  call signatures are MSS-shaped (`HSAMPLE`/`HSTREAM` handles);
  these become opaque pointers backed by the new engine.
- Music streaming (Ogg/MP3) — ensure file-format support matches
  what the game ships.
- Delete `fmodvc.lib` linkage.

**Exit criterion**: SFX and music on all three platforms.

---

## Phase 8 — Cinematics

- Replace Smacker decoding with [libsmacker](https://libsmacker.sourceforge.net/)
  (open-source, MIT-licensed, decodes original Smacker format).
- Bink: drop, or convert assets to Smacker/WebM, or use FFmpeg if
  licensing allows. Decision deferred until we audit which assets
  are which.
- Render decoded video frames into an `SDL_Texture` and present.
- Delete `binkw32.lib` / `SMACKW32.LIB` linkage.

**Exit criterion**: cinematics play (or are gracefully skipped) on
all three platforms.

---

## Phase 9 — Fonts & GDI cleanup

- Replace [sgp/WinFont.cpp](../sgp/WinFont.cpp) with stb_truetype
  (single-header, public domain).
- Most of the game uses pre-rendered bitmap fonts shipped in the
  asset bundle, so this code path is only used in a few places
  (mod-added text, debug overlays).

**Exit criterion**: all text renders on all three platforms.

---

## Phase 10 — Platform packaging & CI

- macOS: `.app` bundle, code-signing config (unsigned for now),
  resource path resolution via `SDL_GetBasePath`.
- Linux: AppImage (initially).
- Windows: keep existing installer flow; produce a zip artifact too.
- CI matrix: build all three on every PR. At minimum "configures &
  compiles". Ideally a headless boot smoke test.

---

## Open questions / decisions still pending

1. **Lua 5.1 vs Lua 5.4** — the master-branch TODO mentions building
   Lua from source. JA2 mods may depend on 5.1 semantics. Default:
   stay on 5.1.x source build, defer any version bump.
2. **Multiplayer long-term** — RakNet 4 port, SDL3_net swap, or just
   leave it Windows-only forever? Question for the maintainers.
3. **Editor builds** — JA2MAPEDITOR / JA2UBMAPEDITOR targets must
   keep building. Should they share the SDL3 surface or stay legacy?
4. **Threading** — once Phase 2 ports Timer Control to std::thread,
   audit other `CreateThread` / `_beginthread` usage. Saw a few in
   sgp.cpp's WindowProcedure.
5. **Save format compatibility** — must not change. Worth a fuzz
   round-trip test before merging Phase 6.

## Reference: upstream prior art

- [ja2-stracciatella](https://github.com/ja2-stracciatella/ja2-stracciatella) —
  ported the *original* JA2 to SDL2 over ~10 years. Their video,
  input, audio, and filesystem layers are useful reference but can't
  be copied wholesale — they diverged from the 1.13 lineage long ago
  and 1.13's feature surface is dramatically larger. License is
  compatible (SFI-1.0 / GPL-style); attribution required if we lift
  specific code.
- [facebookarchive/RakNet](https://github.com/facebookarchive/RakNet)
  — RakNet 4.081, the only public RakNet source. JA2 uses 3.401
  which has incompatible API; see Multiplayer section.
- [libsdl-org/SDL](https://github.com/libsdl-org/SDL) — SDL3, latest
  release.
- [nothings/stb](https://github.com/nothings/stb) — stb_truetype for
  Phase 9.
- [libsmacker](https://libsmacker.sourceforge.net/) — Phase 8 Smacker
  replacement.

---

## Commit log shape

Roughly 50 commits on `sdl3-port` past `master` at the close of
Phase 1. Each commit is scoped to a small, reviewable change with a
concrete message. The first commits (Phase 0/1 baseline) and the
final Phase 1 close are easy to identify in the log. The middle is a
long sequence of clang-strictness fixes and Win32-gating commits;
they could be squashed before merging upstream if desired, but the
granularity is handy for bisecting regressions.
