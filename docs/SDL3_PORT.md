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
| 2 | Portable file I/O / time / memory / debug | ✅ Done — all 8 items addressed (bfVFS handles FileMan + SLF; Timer Control + MemMan + DEBUG + timer.cpp rewritten portable; wine/ deleted; legacy LibraryDataBase + Win32 file stubs dropped). |
| 3 | SDL3 window + event loop, drop WinMain | ✅ Done — SDL3 message pump runs the game on every platform; `_WIN32`-only WinMain gone. |
| 4 | SDL3 input, drop DirectInput / Win32 hooks | ✅ Done — `sgp/sdl_input.cpp` translates SDL_Event → JA2 `QueueEvent`, mouse/keyboard fully on SDL3. |
| 5 | SDL3 video (RGB565 transitional), retire DirectDraw | ✅ Done — `sgp/sdl_video.cpp` + `sgp/sdl_vsurface.cpp` own the path; legacy `video.cpp` / `vsurface.cpp` deleted (Phase 5b structural flip, 2026-05-16). |
| 6 | RGBA8888 pipeline, rewrite blitters, kill inline asm | ✅ Done — ALL inline asm ported to C (incl. the renderworld multi-Z-strip blitters that were silent no-ops on non-Windows, which had left prone soldiers, corpses and multi-tile structures invisible; tactical playable, FMOD→SDL3_mixer in 6r, Smacker→libsmacker in 6u, cursor + render + tooltip fixes). Internal format is now **RGBA8888** (`SGP_PIXEL_DEPTH 32` in `sgp/pixfmt.h`; `Get16BPPColor`→`Get32BPPColor`; `PixShade`/`PixIntensity`/`PixBlend50` do per-channel arithmetic in place of the 16-bit LUTs; ARGB8888 streaming texture). The `Get16BPPColor`-into-`UINT16` colour-truncation tail was swept across `phase-6c-truecolor-ui`, `fix-truecolor-item-glow`, and `fix-truecolor-ui-remainder`. RGB565 now only survives at asset-decode boundaries (STCI/himage expand stored 16-bit pixels to ARGB on load) and in 16bpp-only-guarded LUTs. |
| 7 | Audio — SDL3_mixer / SoLoud, drop FMOD | ✅ Done — landed as Phase 6r. SDL3_mixer is the only audio path. |
| 8 | Cinematics — libsmacker, decide on Bink | ✅ Done — landed as Phase 6u. libsmacker vendored in `ext/libsmacker`; Bink path stubbed (JA2 ships no `.bik` files). |
| 9 | Fonts — stb_truetype, drop GDI | ✅ Done — `sgp/WinFont.cpp` is a cross-platform stb_truetype rasterizer over SDL-owned native-pixel surfaces. The default STI bitmap catalogue is unchanged; scalable text and tooltip scaling use a configured/VFS or bounded platform font fallback and fall back transactionally to bitmap text when unavailable. |
| 10 | Platform packaging + CI | ✅ Done — CI compile-check and tagged zip releases cover **Linux x64, Linux ARM64, macOS, and Windows**. Tagged releases additionally publish byte-reproducible x64/ARM64 AppImages and a native per-user Windows installer. macOS `.app` bundles are ad-hoc signed and strictly verified; native packages remain unsigned until protected release keys exist. |
| ∞ | Multiplayer — client/server wrapper + project-native framed SDL3_net transport | ✅ Built. The main-menu entry is enabled, tagged releases package the data-free PvP `ja2server`, and full-engine co-op now has deterministic campaign persistence/sync, configurable trusted-LAN admission, a nine-intent authoritative server including aimed fire, selected-actor reload, synchronous visible-door open/close, and exact-serial interrupt pass, and a worldless passive `JA2` client with a logical-grid plot. It is a technical slice, not terrain-rendered JA2 co-op. |

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
  retired. Indexed ETRLE artwork can remain 8-bit at rest, the Z-buffer remains
  typed `Depth16`, and RGB565 survives only as an explicitly converted
  asset/mod compatibility token. Converted sprite caches contain dense native
  `PIXEL` data and a separate opacity mask, never a packed 2-byte colour stream.
  True-colour PNG/HIMAGE RGBA bytes are likewise normalized once into native
  ARGB plus 0–255 opacity when a video object is created; live draws do not
  retain loader byte order.
- Audio (SFX, music, Smacker cinematics) replaced with portable
  libraries in this same effort.
- Build system is CMake-only and fetches its dependencies (no
  pre-built `.lib` blobs checked into the repo).

## Non-goals (for this branch)

- Refactoring game logic beyond what the API changes force.
- Touching the modding/INI/Lua interfaces.
- Rewriting the editor as a separate concern — it must keep building
  but is not the focus.
- Certifying a complete in-engine, two-machine multiplayer playthrough. The
  standalone coordinator's PvP session path is loopback-tested. The full-engine
  co-op process now supplies persistent strategic state, trusted-LAN admission,
  campaign transfer, a wired authoritative tactical server, and a composed
  passive client with a worldless committed-snapshot control screen and logical
  spatial plot. The JA2 terrain/static-world renderer, complete combat/world
  replication, general strategic mission control, and an installed-data two-
  process smoke remain unfinished.

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
- [sgp/DEBUG.cpp](../sgp/DEBUG.cpp): assertions now schedule the error
  screen and unwind to the outer SGP exception boundary. They do not run a
  nested platform message pump or recursively call `GameLoop`; the main SDL
  loop remains the sole frame driver. SEH blocks are gated on `_MSC_VER`, and
  the `<crtdbg.h>` include is gated there as well.

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

- [sgp/WinFont.cpp](../sgp/WinFont.cpp) now owns a single portable
  `stb_truetype` backend. It decodes both 16-bit and 32-bit
  `wchar_t` hosts into Unicode scalars, bounds font bytes, pixel height and
  every glyph allocation, clips native-pixel writes, applies kerning and
  deterministic alpha coverage, and contains all load/raster failures.
- [sgp/WinFont.h](../sgp/WinFont.h) exposes one platform-neutral descriptor
  and plain-text render boundary. The former inline no-op branch and localized
  printf-format path are gone.
- [Utils/Font Control.cpp](../Utils/Font%20Control.cpp) initializes and shuts
  the backend down on every host. Initialization is transactional: an absent or
  invalid scalable font turns `USE_WINFONTS` off and continues with the
  already-loaded STI catalogue. Tooltip scaling follows the same rule.
- `WIN_FONT_FILE` and `WIN_FONT_BOLD_FILE` in `[Ja2 Settings]` may name
  a VFS-relative font or an absolute operating-system path. Without them the
  backend tries `FONTS\\ja2font3.ttf`/its bold companion and then a short
  platform font list (Microsoft YaHei/SimSun/Arial/Segoe UI,
  PingFang/Arial, or Noto/WenQuanYi/DejaVu/Liberation). CJK-capable faces are
  prioritized for the Chinese language; the other catalogues retain their
  Latin/Cyrillic-first metrics.
  A missing bold face uses a bounded one-pixel synthetic bold pass.
- The legacy catalogue's per-section `Height` and `Weight` overrides,
  `WIN_FONT_ADJUST`, default colours and mappings are retained. Its
  `Weight` values select the available regular/bold face at the legacy
  semibold boundary (`600`); stb does not synthesize intermediate GDI weights.
  The catalogue defaults are therefore unchanged. Its
  per-section `Name` value was a GDI installed-family lookup and cannot be
  reproduced by a platform-neutral file parser; migrate that value to the
  explicit `WIN_FONT_FILE`/`WIN_FONT_BOLD_FILE` paths above.
- Font files are limited to 64 MiB and are deliberately a trusted local
  configuration/content boundary; upstream stb_truetype does not promise safe
  parsing of adversarial font bytes. Normal game text still uses the shipped
  bitmap fonts and needs no new asset.

#### Phase 10 — packaging + CI

- Top-level [CMakeLists.txt](../CMakeLists.txt): `add_executable(... WIN32 ...)`
  could grow `MACOSX_BUNDLE` and Linux variants. Pre-built `.lib`
  files (`binkw32.lib`, `lua51.lib`, `SMACKW32.LIB`,
  `libexpatMT.lib`) already gated `if(WIN32)`. Pulling Lua, Expat,
  Bink (when replaced), Smacker (when replaced) from source on every
  platform is Phase 10 cleanup.
- The completed CI matrix covers Windows, macOS, Linux x64, and Linux ARM64.
  Tagged packaging retains the four zip artifacts, ships macOS app bundles,
  creates reproducible AppImages, and creates a native Windows installer. The
  implementation and signing boundary are documented in
  [packaging/README.md](../packaging/README.md).

### Multiplayer — current build and packaging state

- The `Multiplayer` CMake target builds `client.cpp`, `server.cpp`, and
  `transfer_rules.cpp`, and links the production `SdlNetTransport` target.
  The transport owns the project-private framed SDL3_net TCP protocol directly;
  the compatibility shim, dormant third-party source archive, and no-op stubs
  have been removed.
- The main-menu Multiplayer entry is enabled.
- Tagged release jobs build `ja2server` and package it with its sample
  configuration and README on Linux, macOS, and Windows. The real coordinator
  pump has a data-free two-client loopback test for admission, roster, barriers,
  PvP turns/interrupts, late join, and disconnect. Full in-engine play remains
  experimental. The standalone process remains PvP-only because it has no
  AI/campaign authority. Separately, full-engine
  `JA2 --dedicated --dedicated-mode=coop` now creates or resumes a deterministic
  strategic campaign and listens on a configurable `--dedicated-coop-bind` and
  `--dedicated-coop-port` endpoint, defaulting to `0.0.0.0:60005`. Its
  permissionless first joins, OS-CSPRNG bearer credentials, and plaintext
  transport provide transport-bound trusted-LAN admission, not user
  authentication. The dedicated runtime uses global co-op protocol v7 and
  composes `CoopTacticalProtocol`/`FullEngineCoopServerSession` with
  fresh-baseline-gated actor assignment, including grow-only late peers, nine-
  intent JA2 command execution, observer deltas, and receipts. The sixth intent
  is an exact-target aimed single-shot firearm attack with server-side live
  validation. The seventh is a zero-payload selected-actor reload prepared as a
  `ReloadWeaponCommand` and revalidated through native `AutoReload`, including
  manual chambering. The eighth requests the inverse state of an exact projected
  visible door and is limited to one synchronous adjacent open/close action.
  The ninth passes one exact eligible actor's exact interrupt serial; pass votes
  never expose the native interrupt list or mutate the actor to fake an action.
  End turn, move, face, stance, stop, and reload carry
  `TacticalCommandAuthorityPolicy::DedicatedCoop`; only `NetworkPeer` or
  `Replay` may carry it, and simulation-command journal wire v4 preserves it
  through replay source substitution. Execution repeats live-world and
  controllable on-foot actor validation and, in combat, the player-turn/no-
  pending-action gate. Resolving interrupts block input; active player
  interrupts admit only eligible actors, reject ordinary end turn, and release
  after the last eligible exact-incarnation vote. AI interrupts remain native.
  Outside that phase, end turn rechecks the exact next team and
  reload repeats its weapon/ammunition/chamber/AP resolver. Default `Legacy`
  commands keep established replica and system behavior; aimed fire retains
  its strict synchronization-source resolver. The untrusted server boundary
  allows one pending command per peer. Exact-next pipelining receives a
  non-consuming `InvalidCommandSequence` before replication/reservation/gameplay
  and can retry after the earlier terminal result. Global
  `AuthoritySequenceExhausted` reason 20 instead consumes the cursor; the server
  remains active to flush its terminal receipt, and the client records the exact
  receipt history/cursor before failing and closing.

  Protocol v3 also carries a 24-byte authenticated self-retirement request and
  a 48-byte result. The request has only version, epoch, and request ID, with
  no client-selected peer or victim. The transport resolves its own identity;
  begin reserves bounded same-epoch tombstone capacity and marks it Pending,
  closing gameplay. An accepted request globally freezes listener input and
  discards admission/tactical/campaign FIFO work. At a committed frame the
  runtime waits only for already-authorized local command/receipt work, commits
  the retired-credential tombstone and releases the seat, and only then permits
  the truthful result. It stops/reconciles the wire layers before stable-
  compacting replication, command, ACL, authority, Ready, and participant
  tables. Four-to-five churn coverage preserves every survivor cursor/history
  and gives the distinct replacement a fresh baseline and cursor one.

  An exact untouched campaign deterministically hires and durably
  saves the four cheapest eligible healthy A.I.M. mercenaries plus their normal
  arrival events before admission, then enters its configured hostile arrival
  encounter after the bounded campaign-ready gather gate. Resume currently
  accepts that exact prepared-initial in-transit roster/event shape without
  rehiring, bootstraps an exact empty initial checkpoint once, or opens a cold
  non-initial strategic checkpoint with a valid live on-foot squad mercenary.
  The last case is preserved at entry and starts admission/sync without starter
  mutation, then enters the canonical hostile sector occupied by an eligible
  actor once a peer is ready. Peaceful-only campaigns stay worldless in
  `StrategicIdle`; vehicles, drivers, passengers, and non-squad duties are not
  direct-control actors. After exact victory and full tactical/dialogue/timer/
  gameplay drain, admission closes immediately, both Ready sets are reconciled
  away, and inbound work is discarded before ACK or intent traffic can starve
  return. A pure recheck reopens `Playable` on regressed evidence, waits for the
  fresh command/receipt/replication boundary on stable evidence, and only then
  cold-unloads through JA2's normal sector-temp/`TrashWorld` tail, drains the tactical world, commits a
  required strategic checkpoint, supersedes campaign transfer, and then reopens
  admission. Native unload retires tactical-only temporary schedules before that
  cold checkpoint. Dedicated co-op suppresses the interactive auto-bandage
  prompt. Ambiguous partial initial states fail closed.
  The normal `JA2 --coop-client` path now composes pre-random/VFS bootstrap,
  private scratch, and a separate rollback-safe post-package/pre-legacy
  installed-content manifest subsystem. It validates and counts every VFS
  occurrence before omitting explicit exclusive runtime namespaces, selects the
  smallest-layer normalized read-only overlay while allowing case-only
  spellings across layers, rejects same-layer ambiguity/duplicates and any
  remaining writable shadow, and caches the descriptor-verified digest. Late
  open reuses that cache without VFS recomputation and verifies the runtime
  fingerprint before hash-verified checkpoint transfer and cold load. Dedicated
  full loading holds `ScopedSavedGameFaceReconstruction` for the complete
  synchronous load, so saved faces use profile presentation timing with no
  canonical RNG draws while ordinary face creation retains all three legacy
  draws. `StrategicAILoadPolicy::DedicatedExactRestore` accepts current SAI save
  v29 without compatibility or repair gameplay and rejects stale versions;
  interactive loads keep their repair policy. A manifest capture failure
  unwinds the already-active package subsystem. The
  bootstrap first exact-validates outer campaign identity, then atomically
  quarantines a nonempty disposable client VFS profile as a strict private
  `profile.orphan.<pid>.<seq>` sibling, requires a freshly empty replacement,
  and recreates its scratch files. VFS caches, `Temp`, settings, and load output
  are disposable; reconnect/retired evidence remains in the held parent, and
  existing quarantine siblings require the identity record. The passive client
  bypasses `INTRO_SCREEN` for INIT state zero so its INIT-only frame policy
  cannot loop before `InitializeJA2` opens live transport. The
  co-op modes bypass the ordinary `InitMainMenu` transition during INIT, so the
  dedicated stage-four campaign-entry request and passive worldless screen run
  before any pending main-menu transition commits. Dedicated creation alone
  runs `InitGameOptions()` immediately before `InitNewGame(FALSE)` because the installed strategic/Lua difficulty domain is 1..4; resume leaves checkpointed options untouched. The client also owns a private atomic 224-byte canonical
  bootstrap + `AdmissionAck` + SHA-256 reconnect record outside the profile, the real socket adapter, the snapshot
  replica, and a worldless `INIT_SCREEN` view. Tactical snapshot wire v7 supplies
  exact authority dimensions, canonical hostility, visible public door records,
  the public `commandsBlocked` bool, compact interrupt phase/serial, per-actor
  interrupt-action eligibility, and five bounded 12-byte combat-equipment
  records—primary hand, secondary hand, helmet, vest, and legs—to a passive
  logical diamond with friendly markers and an actor-table fallback. Each record
  captures only the first object's item ID, stack count, and condition. For an
  ammunition-bearing hand object it also captures loaded-ammunition item/count,
  signed ammunition condition (including a negative jam state), and chambered
  state; those fields stay canonical zero for ordinary equipment. Older
  layouts are rejected rather than inferred.
  Native interrupt-list details and hidden interrupters remain private. Player-
  team actors are always retained, while non-player records require exact shared
  public `SEEN_CURRENTLY` knowledge and leave/re-enter as that knowledge changes.
  Outside modals, its allocation-free direct arrows submit exact isometric
  row/column deltas: Up -1/-1, Down +1/+1, Left +1/-1, and Right -1/+1. Current
  and target grids are authority-dimension-bounded and never predicted.
  Tab or `]` selects the next assigned actor, `[` the previous, and `M` retains numeric-
  grid entry. Face, stance, stop, end-turn, bounded opposing-target/aim,
  selected-actor reload, modal `D` visible-door selection, and `T` end-turn or
  active-interrupt pass controls send typed
  intents; `commandsBlocked` disables all actions and closes every command
  modal while local clocks, AI,
  campaign, pathing, and tactical simulation stay paused. A real loopback socket
  E2E destroys/recreates the client composition, restores the same peer without
  a second identity issuance, and reaches Move, aimed fire, reload, and an
  exact-serial interrupt pass with authoritative deltas and Applied receipts. Credentials are
  restored before connect and persisted before ACK; epoch-only stale records are
  erased only after compatibility verification, while unsafe or differently
  bound records fail closed and live epoch mismatch precedes any request. A
  retained durable same-epoch bearer retries without an attempt cap across the
  intentional post-combat admission blackout, with a saturating unsigned retry
  counter; credential-less startup retains the eight-attempt bound. Voluntary
  leave is a bounded two-step `L` control requiring a physical release between
  key-downs, and both worldless screen states drain the complete input FIFO. The
  core retains the request before send and replays its exact ID after a
  same-process reconnect ACK instead of resuming gameplay.

  A committed result atomically renames the exact private bearer to
  `client-reconnect-credential.retired` before clean `Retired` state or socket
  stop. The idempotent marker is terminal at startup before network construction
  and across epoch changes; ambiguous, corrupt, or unsafe evidence fails closed.
  If marker storage fails before rename, however, the active `.bin` remains. If
  the server also independently rolls epoch before convergence, late verified
  startup may classify that active record `StaleSession`, erase it, and fresh-
  admit. Same-epoch remains fail-closed, and durable cross-epoch terminality
  begins only after `.retired` publishes. The real socket E2E continues through
  commit-before-result, client marker publication before clean close, and
  stopped-layer peer compaction before world/epoch teardown.
  Generic SdlNet retains a 256 KiB/s sustained inbound rate and a 1 MiB burst.
  `SdlNetInboundMessageBudget` is selectable only before `Start()` and capped at
  32 MiB/s and 4 MiB. Only a full-engine client with a non-null campaign sink
  opts into that maximum profile; bootstrap/core-only/legacy peers keep strict
  defaults. Its static bound covers one campaign window at 144 FPS. The
  production socket E2E transfers one exact 11,796,517-byte checkpoint twice,
  193 chunks per transfer, at 7 ms pacing.
  Snapshot v7's exact header/actor/door sizes are 53/92/7 bytes; the actor
  includes five 12-byte combat-equipment records and the generic bound is
  384053 bytes. Delta wire v6 permits 18434 generic events, orders actor-loadout
  changes after
  vitals and before door events, and encodes a same-serial interrupt-phase turn
  change as one exact 43-byte event. The inner co-op tactical envelope is wire v3
  and caps 256 actors, 1024 doors, and 3074 events, with baseline payload/
  envelope 30773/32385 bytes and delta payload/envelope 62034/62106 bytes under
  64 KiB. Intent wire v3 is bounded at
  72+8=80 bytes; tactical-world service/observer are 2.0,
  `DoorCapacityReached` is 12, and journal-v4 authoritative door/pass tags are 33/34.
  Same-connection tactical resynchronization is implemented around an
  authenticated exact 88-byte self-only request. Its bounded reasons cover a
  delta-sequence gap, payload-checksum mismatch, state mismatch, replica
  rejection, invalid envelope, and baseline rejection. The client retains its
  last committed view and freezes input while the server sends a fresh baseline
  on the existing socket; the normal baseline ACK restores replication. The
  server validates the last committed checkpoint across rotated replacement-
  baseline retries. Admission, socket, identity, assignment, server command
  cursor, pending commands, and receipt history survive. A bounded per-peer
  ledger records only successfully sent baselines and deltas. Exact late ACKs
  may advance committed recovery evidence without mutating the current phase or
  staged send; monotonic send ordinals prevent equal-revision regression. A new
  accepted request purges prior sent proofs after validating its checkpoint.
  Input remains frozen: an unchanged cursor clears only a proven-unconsumed
  command, while a consumed command's lock survives baseline ACK until retained
  Queued and terminal receipts replay in the same tactical world. A newer-world
  baseline adopts its authoritative cursor and retires the obsolete old-world
  lock; a late old-world receipt remains idempotent. A reconnect baseline waits
  behind a retained pending command, while same-connection resync remains
  baseline-eligible with that lock intact. Three replacement-baseline failures
  close the connection. Disconnect and reconnect remain transport-loss
  recovery. Focused and real-socket tests cover rejected replacement and
  pending-command recovery.
  Door authority accepts only visible cardinally adjacent ordinary doors and
  rejects stealth, locks/traps, busy/tin-can, animation, pending, and legacy-
  network states. The native helper preflights graphic/status, computes pure
  explicit-grid noise, swaps/verifies and commits the partner/status/
  `LEVELNODE`, recompiles movement, and performs POW/flashlight maintenance;
  AP/BP, exactly one `OurNoise`, and sight/opponent-list/interrupt/AI work follow
  only success. Integrity failure latches the world with no points/noise.
  This still lacks the JA2 terrain/static-world renderer and full combat/world
  replication. The five-slot combat-equipment projection is not the future
  authorized, chunked full 55-slot inventory domain: reserve ammunition,
  remaining equipment-stack
  objects, attachments/LBE, other
  items, general structures, and door lock/key/trap/asynchronous interaction state,
  per-peer visibility, broader attacks/interactions, and asynchronous effects.
  There is no TLS or public-host authorization, and general strategic mission/
  session control remains outside the starter slice.

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
inline-asm blocks, the legacy multiplayer transport situation, and AppleClang's
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
   macOS and glibc Linux). `DEBUG.cpp` no longer owns an inline platform
   message pump; `_FailMessage` schedules the engine error screen and unwinds
   the active frame. `OutputDebugString` routes to stderr via the compat layer.
6. ~~[sgp/timer.cpp](../sgp/timer.cpp) — replace the `SetTimer`-driven
   game clock with a `std::chrono`-driven equivalent. Tiny file.~~
   **Done.** Dropped the SetTimer/KillTimer dance; `GetClock()` now
   samples `std::chrono::steady_clock` directly.
7. ~~Delete `wine/`.~~ **Done.** The Wine cnc-ddraw override is
   obsolete now that SDL3 replaces DirectDraw everywhere. Removed the
   subdirectory, its CMakeLists entry, and the call site in
   `sgp.cpp`'s WinMain.
8. ~~Remove the now-redundant Win32 file I/O stubs from
   msvc_compat.h.~~ **Done.** The legacy `sgp/LibraryDataBase.cpp` was
   the last consumer (~1100 lines of dead SLF-reader bodies; bfVFS
   does the real SLF reading via `VFS_WITH_SLF`). Replaced with a
   ~30-line stub keeping only the two symbols other TUs still touch:
   `gzCdDirectory` (CD-root buffer, written from GameSettings.cpp)
   and `ShutDownFileDatabase()` (no-op cleanup called once from
   gameloop.cpp). All other public functions return FALSE / 0.
   Dropped `CreateFileA` / `ReadFile` / `WriteFile` /
   `SetFilePointer` / `GetFileSize` / `GetFileAttributesA` from
   `msvc_compat.h`; SetErrorMode + SEM_* defines stay because two
   `_WIN32`-only CD-drive paths in Options Screen and FeaturesScreen
   parse but don't link them on non-Windows.

**Exit criterion**: the non-video, non-input, non-audio parts of SGP
do useful work on non-Windows. File I/O succeeds, the clock advances,
SLF archives load. ✅ Achieved as of Phase 6+ runtime — game boots,
loads its SLF archives via bfVFS, save/load works, audio plays.

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
   SetMouseCursorProperties / etc. Heap native-`PIXEL` buffers behind the
   Lock entry points; `SDL_UpdateTexture` + `RenderTexture` +
   `RenderPresent` in RefreshScreen. `PrintScreen` now writes one
   logical-resolution PNG per keypress under `Screenshots/`; the old
   Ctrl+PrintScreen continuous frame dumper remains deliberately retired
   because it could consume gigabytes in minutes. The legacy cursor-file,
   object-cursor, visibility, and composite-blit entry points execute real
   SDL-buffer work rather than returning success without doing anything.
   Suspend/restore and blit-readiness calls expose the actual SDL buffer
   lifecycle, and `EndFrameBufferRender` publishes direct legacy writes into
   the partial-upload damage path.
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
     Transactional region-list helpers; transactional
     video-object-to-surface conversion; clipped surface pixelation. The old
     DirectDraw multi-rectangle `SetClipList` contract has no SDL equivalent
     and now fails explicitly instead of pretending that a requested clip was
     installed.
   - CPU blitters: BltVideoSurfaceToVideoSurface (memcpy + colour-key
     loop), BltVideoSurface (Get + dispatch), BltStretchVideoSurface
     (nearest-neighbour), BltVSurfaceUsingDD (alias to the CPU blit
     path), ColorFillVideoSurfaceArea (direct rect fill). The pointer
     compatibility path clips source and destination geometry together with
     overflow-safe arithmetic, retains destination regions, horizontal
     mirroring, source/destination colour keys, indexed fills, and
     overlap-safe same-surface copies.
   - Tile fill, shadow, copy, stretch, shade, and image draw operations now
     route through the mapped engine render-command boundary. Direct HIMAGE
     compatibility covers indexed, RGB565, 24-bit RGB TGA, and 32-bit RGBA
     sources with checked subrect and destination geometry.
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
   [sgp/vobject_blitters.cpp](../sgp/vobject_blitters.cpp)
   needs replacement with portable C — keep RGB565 math, just stop
   using `__asm`. **Done** (Phase 6a) — `blendWithAlpha` is portable C
   and all inline asm has since been purged from the rendering engine
   (vobject_blitters.cpp, renderworld.cpp, Interactive Tiles.cpp).

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
update Z; obscured pixels render only on a checkerboard mask. The
clipped plain-sprite variant preserves obscured depth while its
unclipped counterpart replaces depth for every drawn pixel; the
non-writing shadow variants preserve depth throughout).

(Historical note: at the time of the asm port the 8bpp palette LUT was
still RGB565. The pixel-format conversion to RGBA8888 has since landed —
see 6b below. The Z-buffer remains `UINT16`, which is correct: it stores
depth, not colour.)

The remaining inline-asm sites outside `vobject_blitters.cpp` are also
gone:

- `sgp/shading.cpp` `FindIndecies` (a brute-force nearest-color
  palette lookup) is now portable C.

The seven portable multi-Z-strip variants originally left inside
`TileEngine/renderworld.cpp` now share
`sgp/vobject_multiz_blitters.cpp`. Walls, structures, multi-tile actors,
and corpses submit an engine depth-image command carrying the source-owned
profile frame; the backend preserves their distinct strip increments,
same-depth wall pass-through, obscured checkerboard writes, palette marker,
and optional-alpha rules.
The remaining tactical-world raster tail is now separated too:
`ClearDestination` image commands own sprite-footprint clearing with the real
RGBA8888 stride, `RenderImageDepthVisibilityQuery` owns tile-redundancy reads,
and riot shields/wall decals use the ordinary depth-image command. Raw
footprint, inverse-equality-depth, and signed occlusion loops live in
`sgp/vobject_mask_blitters.cpp` and `sgp/vobject_depth_queries.cpp`; none remain
implemented in `TileEngine/renderworld.cpp`.
- `DebugBreakpoint()` in `sgp/DEBUG.H` switched from `__asm { int 3 }`
  to `__builtin_debugtrap` / `__debugbreak`.
- The vendor headers (`sgp/RAD.H`, `sgp/Mss.h`, `sgp/Mss-old.h`) still
  hold `__emit` / `int 3` macros but nothing in the live build
  includes them; they get deleted alongside their subsystem in Phase
  7/8.

**Blind spot, fixed 2026-05 (commits `4eed303f` / `cb022ec4` /
`f7e95aa8`).** The first asm pass missed the multi-Z-strip blitter
family in `TileEngine/renderworld.cpp` (the `…TransZInc…` /
`…TransZTransShadowInc…` blitters, `TransInvZ`, `IsTileRedundent`,
`Zero8BPPDataTo16BPPBufferTransparent`) and the pixel-hit test in
`TileEngine/Interactive Tiles.cpp`. Unlike the rest, these were gated
`#if defined(_WIN32) && defined(_M_IX86)` with **no C `#else` body**, so
on macOS/Linux (and 64-bit Windows) they `return`ed without drawing
rather than failing to compile — a silent no-op. Everything rendered
through the multi-tile Z-strip path was therefore invisible: prone
soldiers, dead bodies, and multi-tile map structures (sandbags, crates,
car wrecks). Now ported to portable C via a shared `BlitMultiZStripRun`
helper (Z steps by `Z_SUBLAYERS` every 20 columns per the object's
`ZStripInfo`, matching the original asm; logic cross-checked against the
ja2-stracciatella C++ port). `grep '__asm {' TileEngine/*.cpp` returns
zero — the rendering engine now contains **no inline assembly**.

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

### 6b. RGBA8888 conversion (✅ done)

Landed across the `phase-6b-rgba8888` and `phase-6c-truecolor-ui`
branches, with the colour-truncation tail closed on
`fix-truecolor-item-glow` and `fix-truecolor-ui-remainder`. How it maps
to the original plan:

1. ✅ Depth flag is `SGP_PIXEL_DEPTH` in
   [sgp/pixfmt.h](../sgp/pixfmt.h) (not the old `Ja2/local.h
   PIXEL_DEPTH`), set to `32`. `typedef UINT32 PIXEL`.
2. ✅ Buffers/blitters/callers use `PIXEL` throughout; pitch math is
   `sizeof(PIXEL)`. The SDL streaming texture is `SDL_PIXELFORMAT_ARGB8888`.
3. ✅ `Get16BPPColor` returns true ARGB8888 (`Get32BPPColor`). The
   16-bit shade/intensity LUTs are bypassed at 32bpp —
   [sgp/pixfmt.h](../sgp/pixfmt.h)'s `PixShade` / `PixIntensity` /
   `PixBlend50` inlines do per-channel arithmetic instead, and the 64K
   `ShadeTable`/`IntensityTable` builders in
   [sgp/shading.cpp](../sgp/shading.cpp) are `#if SGP_PIXEL_DEPTH != 32`
   only.
4. ✅ Portable-C blitter loops operate on `PIXEL`. Legacy "logical
   RGB565 token" colours pass through `PixFromColor16`, which widens a
   genuine RGB565 token to ARGB but passes an already-expanded ARGB
   value through unchanged.
5. ✅ Texture format is `SDL_PIXELFORMAT_ARGB8888` (see
   [sgp/sdl_video.cpp](../sgp/sdl_video.cpp)).
   True-colour video objects also normalize the PNG loader's RGBA byte sequence
   once at import into native ARGB plus a separate opacity plane; their clipped
   alpha and shadow draws use the native-pixel backend.
6. ✅ F12 screenshot output is restored as bounded PNG capture at the
   logical framebuffer resolution.
7. ✅ Z-buffer stays `UINT16` (depth, not colour).

**Colour-truncation gotcha (closed).** The migration's main hazard was
storing/passing an ARGB `Get16BPPColor` result through a `UINT16`/`INT16`
hop — `PixFromColor16` then re-decoded the low bits as RGB565, giving
wrong/faded/rainbow colours. Phase 6c swept 27 sites; later passes fixed
the rest (outline blitters' item glow, life/breath/morale + item-status
bars, trait-radius circles, radar/overhead-map lines, mapscreen
fill/outline, debug viewers). A tight `= Get16BPPColor` → 16-bit-decl
scan is now clean except two intentional non-bugs (`shading.cpp index`,
16bpp-only; `vobject_blitters` `us16BPP*TransColor`, dead commented code).

**Exit criterion (met):** game renders correctly on macOS through
extended playtesting; CI compiles on Linux/macOS/Windows.

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
multi-thousand-call-site sweep, far beyond a single phase.

**Actually applied (2026-05-18).** Three pragmatic shims, no
mass-rewrite:

1. `swprintf`/`vswprintf` macros in `sgp/msvc_compat.h` route through
   a translator that rewrites Microsoft-convention wide-printf
   specifiers to POSIX-unambiguous ones: `%s` → `%ls` (wide), `%S` →
   `%hs` (narrow). The two conventions are *swapped* across the
   platforms; legacy JA2 format strings were authored under MSVC.
   This unblocked button labels like "Great (2)" being rendered as
   "G (2)" because the first byte of `"G\0r\0..."` looked like a
   null-terminated narrow string to POSIX vswprintf.
2. `Font.cpp::StringPixLength` was iterating its input as
   `(UINT16*)wchar_t*` — fine on 16-bit-wchar Windows, broken on
   32-bit-wchar POSIX where the second byte of each wide char is
   zero and terminates the loop. Iterate as `CHAR16` (wchar_t)
   directly. Fixes button-text width measurement, which had been
   clipping labels mid-word ("Save Anyti" instead of "Save
   Anytime").
3. `MultiByteToWideChar` / `WideCharToMultiByte` in `msvc_compat.h`
   were returning 0 and doing nothing. JA2's XML loaders all run
   UTF-8 char data through these, so every XML-sourced wide string
   was empty (Difficulty Level dropdown blank; YES/NO confirmation
   dialog with no body). Replace with self-contained UTF-8
   decoders.

(Commits `8e9c32fb`, `1088ccb4`.) After these the macOS build can
navigate the main menu, open INITIAL GAME SETTINGS, change
difficulty, click Start, accept the confirmation dialog.

### 6e. Post-"Start New Game": black laptop screen + popupDef double-free (next session)

After accepting the confirmation dialog the game transitions to
`LAPTOP_SCREEN` (the mercenary hiring laptop). On macOS that
currently renders as a black window -- clicks don't trigger
anything, ESC closes via the backstop quit handler. The transition
itself happens (`gubScreenCount` advances from 0 to 1 inside
`Strategic/Game Init.cpp::InitNewGame`), so the screen handler is
running.

Likely culprits, in decreasing probability:

- The laptop's transition animation path
  (`gfStartMapScreenToLaptopTransition`) is keyed on a flag that's
  FALSE on the first entry from the New Game path, AND
  `fReDrawScreenFlag` (FALSE by default, reset to FALSE inside
  `EnterLaptop`) is never set. With both flags FALSE the per-frame
  handler at `Laptop/laptop.cpp:2521` skips the `RenderLapTopImage()
  + RenderLaptop()` block entirely. The legacy code's first entry
  on Windows must have set one of those flags via a path we're not
  tickling on macOS (no intro video? no fade-in transition?).
  Setting `fReDrawScreenFlag = TRUE` right after `EnterLaptop()`
  returns is the most surgical experiment to try.
- `EnterLaptop()` itself (Laptop/laptop.cpp:877) loads a pile of
  VObjects for the laptop UI; if any one fails silently the
  framebuffer stays black anyway. Trace which `CreateVideoObject`
  calls return NULL.
- A resolution-specific layout: laptop screen logic has explicit
  cases for 640x480 / 800x600 / 1024x768 and a default; at 1366x768
  the default fires and may compute wrong offsets.

**Related bug: popupDef double-free.** `Tactical/Items.cpp` declares
a static `std::map<UINT8, popupDef> LBEPocketPopup`. `popupDef`
holds a `std::vector<popupDefContent*>` and its destructor
delete-loops over the pointers, but the class has no user-defined
copy constructor / assignment. The XML loader at
`Tactical/XML_LBEPocketPopup.cpp:297` does
`LBEPocketPopup[id] = *new_popup;` -- a shallow copy that shares
the content pointers between the source popupDef and the map
entry. When the static map is destroyed at process exit, each
entry's destructor frees pointers that have already been freed
(via either the leaked-source destructor or a prior LBEPocketPopup
re-init), producing the `popupDefContent::~popupDefContent`
`SIGTRAP` we saw in the crash report (decoded from
`~/Library/Logs/DiagnosticReports/JA2_ENGLISH-*.ips`).

Attempted fix: disable copy, add move semantics, switch the XML
loader to `std::move`. That compiled but produced an instant
runtime SIGTRAP in `popupDefContent::~popupDefContent` *before*
the main menu even appeared -- meaning the move-only constraint
broke something the loader was relying on that wasn't visible from
just the one assignment site. Reverted the attempt; the original
exit-time double-free is still latent.

Real fix probably needs one of:
- give `popupDef` a deep-copy constructor (clone the content
  vector via a `popupDefContent::clone()` virtual) instead of
  move-only; the legacy code paths then keep working unchanged;
- OR convert `LBEPocketPopup` to `std::map<UINT8, popupDef*>` (or
  `std::unique_ptr<popupDef>`) so nothing is ever copied;
- OR audit every `popupDef` usage and make sure the XML loader's
  `pData->curPocketPopup` is also leaked-but-never-destructed
  (which it currently is, hence the exit crash happening only when
  the static map is finally torn down).

`popupDefContent` also has a non-virtual destructor while
`popupDef::~popupDef` `delete`s through that base pointer -- UB
that's been working on Windows by accident. Fix while you're in
there.

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

- [sgp/WinFont.cpp](../sgp/WinFont.cpp) uses a pinned stb_truetype snapshot
  on Linux, macOS and Windows; no GDI or DirectDraw font code remains.
- Most of the game uses pre-rendered bitmap fonts shipped in the
  asset bundle, so this code path is only used in a few places
  (mod-added text, debug overlays).
- Enable the optional whole-game path with `USE_WINFONTS=1`. Select an
  explicit trusted font with `WIN_FONT_FILE` and optionally
  `WIN_FONT_BOLD_FILE`; otherwise the backend searches the legacy logical
  font name and bounded platform fallbacks. If none load, bitmap rendering
  remains active. `TOOLTIP_SCALE_FACTOR > 100` uses the same backend.

**Exit criterion: met.** Bitmap text remains the default everywhere, and the
optional scalable path now renders or falls back safely on all three platforms.

---

## Phase 10 — Platform packaging & CI

- macOS: `.app` bundles ship in tagged zip releases and use
  `SDL_GetBasePath`. The release workflow replaces the linker's executable-only
  signature with an ad-hoc bundle signature, then requires strict deep
  verification before archiving.
- Linux: tagged x64 and ARM64 zip artifacts remain unchanged. Both jobs also
  build AppDir-based AppImages from the same staged payload, normalize input
  mtimes to `SOURCE_DATE_EPOCH`, checksum-pin `appimagetool` and its explicit
  runtime, and require two independently packaged images to compare equal.
- Windows: the tagged zip remains unchanged. The same payload also feeds a
  native NSIS per-user installer with explicit-file uninstall ownership. A CI
  smoke test proves that uninstalling preserves a user-owned `Data/` sentinel.
- CI builds JA2, UB, and Map Editor on every branch across Linux, macOS, and
  Windows, with Linux ASan headless tests and build-free release-workflow
  safety checks. Every action in the privileged release workflow is pinned to
  a full commit SHA; downloaded packaging tools are pinned by SHA-256.
- AppImage GPG signing and Windows Authenticode signing are deliberately not
  simulated. They require protected publisher keys and a release-key rotation
  policy; current releases disclose that limitation instead of accepting an
  unsigned fallback as publisher authentication.

---

## Display & windowing enhancements (future)

Quality-of-life / presentation work surfaced while play-testing on
macOS. None are blocking; grouped here because they all live in the
SDL3 present path ([sgp/sdl_video.cpp](../sgp/sdl_video.cpp)) and pair
naturally with the Phase 6b RGBA8888 work.

1. **Lock the window size / aspect ratio, + fullscreen toggle.** The
   window is currently freely resizable on both axes, which stretches
   the fixed-resolution framebuffer and distorts the game (verified:
   resizing vertically/horizontally messes up how it looks). Either pin
   the window to the render resolution (or integer multiples of it), or
   constrain resizing to the correct aspect ratio with letterboxing
   (`SDL_SetRenderLogicalPresentation` with `SDL_LOGICAL_PRESENTATION_LETTERBOX`).
   Add a fullscreen ↔ windowed toggle.
2. **Display sharpening / enhancement when scaled or fullscreen.**
   *Research item.* Pick a scale mode (nearest vs linear) and/or a
   sharpen/upscale filter — integer scaling, a scanline/CRT look, or an
   xBR/Scale2x-style shader — so an upscaled image reads crisp rather
   than blurry or blocky.
3. **Scalable UI + zoomable tactical view (decouple interface scale from world scale).**
   *Bigger research item — investigated, not built.* The dream: scale the
   interface to a comfortable/readable size while being able to zoom the
   tactical map (ideally zoom **out** to see more, in-game). Findings from
   tracing the render path:

   - **Architectural constraint.** The tactical view is 2D isometric
     **sprite-blitting at a fixed `WORLD_TILE_X 40 × WORLD_TILE_Y 20`**
     ([worlddef.h](../TileEngine/worlddef.h)); `RenderWorld()`
     ([renderworld.cpp](../TileEngine/renderworld.cpp)) has **no scale/zoom
     parameter** (the only `gdScale` there is for the radar minimap). The
     whole pipeline — blitters (the asm→C ones), mouse↔gridno math
     (`GetMouseMapPos`), LOS, shadows, soldier/roof Z-layering, dirty-rect
     refresh — assumes 1:1 tiles. That assumption gradients the difficulty.

   - **Tier 1 — scale the UI for readability (feasible, incremental).** The
     bottom tactical bar, strategic screen, laptop and other panels are
     fixed-coordinate **overlays** = the laptop problem repeated per screen:
     render the panel to an offscreen surface, `BltStretchVideoSurface` it
     into a larger rect, and remap mouse input by the inverse scale while
     that screen is active. The laptop is the natural first template — note
     it **already** renders to `guiSAVEBUFFER` and stretch-blits to the
     screen for its open/close zoom ([laptop.cpp ~2449](../Laptop/laptop.cpp)),
     so the mechanism exists; making it persistent is repurposing that plus
     input remap. Merc labels are just text and can be drawn at a chosen
     font size independent of world scale. Wire a `LAPTOP_SCALE_FACTOR`-style
     ini knob (mirrors the existing `TOOLTIP_SCALE_FACTOR`).

   - **Tier 2 — zoom *in* on tactical (feasible, low payoff).** Render the
     viewport offscreen at 1:1, stretch bigger, inverse-map the mouse. Same
     trick; but you see *less* map, magnified and soft. Limited usefulness.

   - **Tier 3 — zoom *out* on tactical (major engine project).** To show more
     map in the same window you must render more tiles at a smaller size,
     which means either downscale-blitting every tile/sprite/shadow/roof (a
     big rewrite of the 1:1-optimized blitters, slower, lossy on fixed-res
     art) or rendering a larger-than-screen 1:1 offscreen and downscaling
     (2×+ cost/memory, fights the incremental dirty-rect renderer). Plus
     mouse↔gridno, scroll bounds, LOS/cursor/glow overlays all hardcode the
     fixed projection and must become scale-aware. **This is why the original
     devs shipped a separate `overhead map.cpp` (schematic zoom-out) instead
     of true zoom — same wall.** Weeks of render surgery, not an afternoon.

   - **Pragmatic path (~80% of the dream).** Do Tier 1 (readable, scalable UI)
     and lean on / enrich the existing **overhead map** as the "zoom out"
     view — that sidesteps the downscaled-tile wall. A *uniform* whole-screen
     zoom (UI + world together) is trivial via `SDL_SetRenderLogicalPresentation`
     but doesn't decouple the two and can't reveal more map, so it isn't what
     this asks for. True free pan-zoom tactical (Tier 3) is a deliberate
     multi-week decision; spike it to put real numbers on the cost before
     committing.

---

## Testing (future — separate task)

There is **no test framework** in the repo yet. Standing up one (and wiring it
into CI via ctest on all three platforms) is its own task. First, highest-value
coverage once it exists:

1. **Save/load round-trip tests.** Build representative game-state structs
   (a merc with a non-ASCII name, full inventory, world items, militia, …),
   run them through the save serializer, read them back, and assert field
   equality — the automated stand-in for "save → quit → reload" playtesting.
   Pairs directly with the [save-format v2 work](SAVE_FORMAT.md).
2. **Golden-byte tests for the save format.** Assert the exact little-endian
   bytes produced for known values. Running identically on Win/Lin/Mac in CI
   *proves* cross-platform save parity instead of assuming it. (The
   `SaveWriter`/`SaveReader` can be made memory-buffer-backed for this.)
3. Beyond saves: unit tests for the pure-logic utilities (math, string,
   pathfinding helpers) that have no engine/global dependencies.

Until this lands, save-format changes are verified by playtesting.

---

## Build hygiene — warning cleanup

Tracked on branch `warning-cleanup` (off `master` after the SDL3-port
merge). The goal is to drive the build log noise down so a fresh
contributor can spot a real new warning. Starting baseline was
~30,000 warnings on a clean macOS Debug build.

### Wave 1 — `STR`/`STR8`/`STR16` const-correctness ✅
Flipped the typedefs in `sgp/types.h`:
```cpp
typedef const CHAR8 *  STR;
typedef const CHAR8 *  STR8;
typedef const CHAR16 * STR16;
```
These exist to receive string literals at function boundaries. Use
plain `CHAR8 *` / `CHAR16 *` (or a typed buffer like `CHAR8 buf[N]`)
when the param is actually a writable destination.

The flip cascaded into 440 errors across 61 files — every site that
was using `STR8`/`STR16` incorrectly as a writable buffer. Fixed
each by either typing the parameter mutable (`CHAR8 *`) when the
function genuinely writes, or by adding `const` to the receiver when
it doesn't. Net diff: 109 files, +268/-261 lines.

Result: **`-Wwritable-strings` from 28,214 → 0**.

### Wave 2 — `-Wcomment`, `-Wnull-conversion`, residual `-Wwritable-strings` ✅
Three smaller categories:
- `-Wcomment` (263 → 0): two real comment bugs (a `/*` inside an
  already-open block comment, a `///***` that clang read as `//` +
  inner `/*`).
- `-Wnull-conversion` (270 → 0): 9 sites where `NULL` was passed to
  `UINT8`/`HWFILE`/`BOOLEAN`/`bool`. Almost all were real bugs
  (caller meant `0`, `FALSE`, or `false`).
- `-Wwritable-strings` (190 → 0): supply-side fixes for plain
  `char *`/`CHAR8 *`/`CHAR16 *` params (not `STR8`/`STR16`) that
  received literals. Same fix pattern as Wave 1, added `const` to
  the parameters that didn't mutate.

Two CI-green commits on `warning-cleanup`.

### Wave 3 — real-bug categories (pending)
~157 warnings, each potentially a latent bug:
- `-Waddress-of-temporary` (63) — taking `&` of an rvalue.
- `-Wtautological-constant-out-of-range-compare` (36) — e.g. `uint8 > -1`
  always true.
- `-Wmultichar` (19) — `'abc'` 3-char literal stored as int.
- `-Wformat-security` (18) — `printf(user_input)` style.
- `-Wdelete-abstract-non-virtual-dtor` (11) — UB on `delete base_ptr`
  when base has no virtual destructor.
- `-Wint-to-pointer-cast` (10).

### Wave 4 — mechanical (pending)
~120 warnings, easy mechanical fixes, low bug-find value:
- `-Wparentheses-equality` (63) — `if ((a == b))`.
- `-Wunused-value` (35).
- `-Wextern-initializer` (14).
- `-Wparentheses` (9).

### Wave 5 — the three JA2-idiomatic big rocks (pending decision)
The remaining ~3,900 warnings all come from three categories that
JA2 uses pervasively:
- `-Wnontrivial-memcall` (1,766) — `memset`/`memcpy` on a C++ class
  with non-trivial members. Could corrupt vtables if any of those
  classes ever grows virtuals. Fix = constructor-style init,
  big refactor.
- `-Winvalid-offsetof` (1,316) — `offsetof()` on non-POD. UB by C++
  standard but works on every known compiler.
- `-Wdeprecated-declarations` (812) — macOS marks `sprintf`/`vsprintf`
  deprecated for security. Fix = use `snprintf` everywhere, or
  silence with `-Wno-deprecated-declarations` on the JA2 targets.

Decision pending — either bulk-silence via CMake flags on the JA2
libs (one-line change, ~3,900 warnings gone, but the underlying
idiom risks remain), or do a real audit. The memset/memcpy class
risk is the only one of these that could realistically bite us at
runtime; the other two are paper UB that compilers honor in practice.

---

## Known UI bugs (post-Phase 6k)

_None currently open._

- ~~**Laptop hover tooltips have transparent backgrounds.**~~ ✅ Resolved —
  this was a fallout of the RGBA8888 conversion (Phase 6) and is no longer
  reproducible; tooltip backgrounds render solid again.

## Open questions / decisions still pending

1. **Lua 5.1 vs Lua 5.4** — the master-branch TODO mentions building
   Lua from source. JA2 mods may depend on 5.1 semantics. Default:
   stay on 5.1.x source build, defer any version bump.
2. **Multiplayer validation** — the standalone coordinator/session protocol is
   exercised over real loopback sockets with two lightweight clients. The
   full-engine path now wires strategic persistence, campaign sync, admission,
   tactical authority/execution, baseline/delta replication, receipts, and a
   passive worldless client with a logical spatial plot. Its production-adapter
   socket E2E covers campaign synchronization, client-composition destruction/
   durable-credential restore without second identity issuance, Move, aimed
   fire, and reload through authoritative movement/AP/damage deltas and Applied
   receipts.
   A separate installed-data independent-process smoke is absent from default
   CTest and registered only when both absolute cache paths
   `JA2_COOP_INSTALLED_SMOKE_EXECUTABLE` and
   `JA2_COOP_INSTALLED_SMOKE_DATA_ROOT` are configured. The POSIX-only serial
   harness uses a free loopback port and private temporary roots to prove
   create/Ready, independent same-root restart with a byte-identical 224-byte
   credential, clean worldless final checkpoint, and resume. It fingerprints
   installed inputs before/after and deterministically cleans process groups and
   temporary state. Complete combat/world replication, the JA2 terrain/static-
   world renderer, general mission/session control, a full installed-data
   playthrough, and a long-running soak still remain.
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
