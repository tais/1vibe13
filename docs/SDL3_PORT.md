# SDL3 Port Plan

Tracking doc for migrating JA2 1.13 off Win32 / DirectDraw / FMOD / Smacker
onto SDL3 + cross-platform equivalents, with a 32-bit RGBA8888 internal
rendering pipeline.

This lives on the `sdl3-port` branch. `master` is left untouched.

## Goals

- Native builds on Windows, macOS, and Linux (all first-class).
- No DirectX, no Win32 GUI/audio APIs, no Wine workarounds, no cnc-ddraw
  shim. The `wine/` subdirectory goes away.
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

## Phase 0 — Branch & baseline

Establish a clean working baseline.

- This plan doc, committed.
- Verify the master branch still builds cleanly from this branch's
  HEAD before any changes (smoke test).
- CI placeholder if any exists today (TBD).

Exit criteria: `sdl3-port` builds bit-identically to `master` on
Windows.

---

## Phase 1 — Build system portability

Get CMake configuring on macOS and Linux, even if the resulting build
is mostly stubs.

- Replace pre-built `.lib` files at repo root (`binkw32.lib`,
  `lua51.lib`, `mss32.lib`, `SMACKW32.LIB`, `fmodvc.lib`,
  `libexpatMT.lib`, `VtuneApi.lib`, `ddraw.lib`) with source builds
  via `FetchContent` / `find_package` or vendored sources under
  `ext/`.
  - `lua51` → build Lua 5.1 from source (the existing TODO already
    flags this).
  - `libexpatMT` → use the system / vendored Expat.
  - `mss32` / `fmodvc` / `SMACKW32` / `binkw32` → delete (replaced in
    Phases 6–7). Stub their headers behind a temporary
    `#if HAVE_LEGACY_AUDIO` until those phases land so the rest of the
    code keeps compiling.
- Add `SDL3` as a build dependency (FetchContent against the SDL3
  release tag; fall back to system package if found).
- Add toolchain files for clang on macOS and gcc/clang on Linux.
- Switch executable target from `add_executable(... WIN32 ...)` to
  conditional WIN32 / MACOSX_BUNDLE / regular.
- Replace `windows.h`-only-for-typedefs usages with `<cstdint>`-based
  equivalents (`UINT8`/`UINT16`/`UINT32` etc. in [sgp/types.h](../sgp/types.h)).
- Get the project to *configure* on all three platforms even if it
  doesn't yet link.

Exit criteria: `cmake --build` succeeds on Windows; configures (may
fail to link) on macOS and Linux.

Risk: build-system churn affects every contributor. Land this phase
as a single squashable PR with reviewers from upstream.

---

## Phase 2 — Portable I/O, time, memory, debug

Strip Win32 dependencies out of the lowest layers of SGP so higher
layers can be ported without dragging Windows in transitively.

- [sgp/FileMan.cpp](../sgp/FileMan.cpp) → `<filesystem>` + `<cstdio>` /
  fopen64. Case-insensitive filename resolution helper for Linux/macOS
  (JA2 asset paths are inconsistently cased on disk; the upstream
  Stracciatella project has a known-good implementation we can mirror).
- [sgp/LibraryDataBase.cpp](../sgp/LibraryDataBase.cpp) → same, plus
  endianness audit (the SLF format is little-endian on disk).
- [sgp/timer.cpp](../sgp/timer.cpp) → `std::chrono::steady_clock`
  everywhere.
- [sgp/MemMan.cpp](../sgp/MemMan.cpp) → trim to a portable allocator
  shim; drop the Win32 heap helpers.
- [sgp/DEBUG.cpp](../sgp/DEBUG.cpp),
  [sgp/debug_win_util.cpp](../sgp/debug_win_util.cpp) → portable logging
  via `sgp_logger`; SymGetLineFromAddr stack-trace bits gated behind
  a Windows-only compilation unit (or replaced with `<stacktrace>`
  when C++23 lands).
- [sgp/Random.cpp](../sgp/Random.cpp) — already mostly portable, audit.
- Delete `wine/` (the registry override is moot once `ddraw.dll` is
  gone).

Exit criteria: the non-video, non-input, non-audio parts of SGP build
on all three platforms.

---

## Phase 3 — SDL3 window, event loop, and message plumbing

Bring SDL3 in at the top of the program, but don't yet render with it.

- Replace `WinMain` + `WindowProc` + Win32 message pump in
  [sgp/sgp.cpp](../sgp/sgp.cpp) with `SDL_Init(SDL_INIT_VIDEO | …)` +
  `SDL_CreateWindow` + a central `SDL_PollEvent` loop.
- Replace `MessageBoxW` call sites with `SDL_ShowSimpleMessageBox`.
- Replace `SetCursor`/`ShowCursor` with `SDL_HideCursor` /
  `SDL_ShowCursor` / `SDL_SetCursor`.
- Provide a `Ja2GetWindow()` / `Ja2GetRenderer()` accessor that the
  video layer will use in Phase 4.
- Keep `iScreenMode` plumbing but interpret it via SDL flags
  (`SDL_WINDOW_FULLSCREEN` etc.).
- Keep DirectDraw alive temporarily for *rendering only*; the message
  pump is now SDL.

Exit criteria: game window opens on all three platforms; game still
renders via DirectDraw on Windows.

---

## Phase 4 — SDL3 input

Replace DirectInput / Win32 keyboard & mouse messages with SDL events.

- [sgp/input.cpp](../sgp/input.cpp) — drive the input queue from
  `SDL_Event` (`SDL_EVENT_KEY_DOWN`, `SDL_EVENT_MOUSE_*`,
  `SDL_EVENT_TEXT_INPUT`, `SDL_EVENT_MOUSE_WHEEL`).
- Build a keycode translation table. JA2 stores raw scancodes in many
  places (savegame hotkeys etc.) — preserve the wire format by
  translating SDL_Scancode → the existing Win32 VK_* values that the
  game already persists. Otherwise we break saves.
- [sgp/mousesystem.cpp](../sgp/mousesystem.cpp) — should mostly just
  consume the queue; spot-check for HWND-based hit testing.
- Text-input fields (dialogs that take typed names) need
  `SDL_StartTextInput` / `SDL_StopTextInput` gating around them.
- Delete `dinput.h` / `dinput.lib` references.

Exit criteria: keyboard and mouse fully functional on all three
platforms with the DirectDraw renderer.

---

## Phase 5 — SDL3 video, transitional RGB565

Replace DirectDraw with SDL3 rendering while keeping the existing
RGB565 internal pipeline. **This is the milestone where macOS and
Linux first see pixels.**

- Rewrite [sgp/video.cpp](../sgp/video.cpp) and
  [sgp/vsurface.cpp](../sgp/vsurface.cpp):
  - Primary surface → `SDL_Texture(SDL_PIXELFORMAT_RGB565,
    SDL_TEXTUREACCESS_STREAMING)` sized to the game's logical
    resolution.
  - Backbuffer → plain heap `UINT16*` (no DD surface).
  - `Flip` / `BltFast` → `SDL_UpdateTexture` + `SDL_RenderTexture` +
    `SDL_RenderPresent`.
- Delete [sgp/DirectDraw Calls.cpp](../sgp/DirectDraw%20Calls.cpp),
  [sgp/DirectX Common.cpp](../sgp/DirectX%20Common.cpp), [sgp/ddraw.h](../sgp/ddraw.h),
  `ddraw.lib`.
- Delete the cnc-ddraw detection in [sgp/sgp.cpp](../sgp/sgp.cpp) (the
  `bCncDdraw` path).
- Delete the `ADDTEXT_16BPP_REQUIRED` error path.
- The 200+ blitters in [sgp/vobject_blitters.cpp](../sgp/vobject_blitters.cpp)
  are *not* touched in this phase. They still write RGB565 to the
  CPU-side framebuffer; SDL hands that to the GPU.
- The inline-asm alpha blender stays for one more phase. Optionally,
  replace it with portable C *now* if it's blocking macOS/Linux
  compilation (MSVC inline-asm syntax doesn't survive clang/gcc).
  This may force us to write the C fallback earlier than planned.

Exit criteria: game boots into the main menu on all three platforms
and renders correctly. End-to-end at least one battle plays.

---

## Phase 6 — RGBA8888 pipeline conversion

Now that everything routes through SDL, swap the internal pixel
format. This is the biggest, most file-touching phase.

- Change `PIXEL_DEPTH` in [Ja2/local.h](../Ja2/local.h) from 16 to 32.
- Wide-rename `UINT16* pBuffer` → `UINT32* pBuffer` across every
  blitter and caller. Adjust pitch math (bytes-per-pixel doubles).
- Regenerate the 8bpp→32bpp palette LUTs in
  [sgp/himage.cpp](../sgp/himage.cpp) and friends. The shading,
  fade-to-black, fade-to-white, translucency, and night-vision tables
  in [sgp/shading.cpp](../sgp/shading.cpp) all need RGBA8888
  equivalents — the precomputed tables that exist today are RGB565-
  shaped.
- Replace the inline-asm `blendWithAlpha` and every other RGB565
  bit-math site with portable C (or SDL_SIMD intrinsics). The
  `gusRedMask` / `gusGreenMask` / `gusBlueMask` runtime detection in
  [sgp/video.cpp](../sgp/video.cpp) becomes constant and most of
  `GetRGBDistribution()` evaporates.
- Update all the saveable surfaces / screenshot writers in
  [sgp/video.cpp](../sgp/video.cpp) (the TGA writers).
- Z-buffer stays UINT16 (it's a depth value, not a color).
- Update SDL texture format to `SDL_PIXELFORMAT_ARGB8888` (or
  whatever endianness matches the in-memory layout).

Risk: this is the phase where game-rendering regressions hide. Plan
to do golden-image regression testing — render a known scene under
the old pipeline (Phase 5 tag) and the new pipeline, diff the
buffers.

Exit criteria: game is visually identical (or better) to the Phase 5
output, end-to-end, on all three platforms.

---

## Phase 7 — Audio: SFX & music

Replace FMOD / Miles Sound System with SDL3-friendly audio.

- Evaluate **SDL3_mixer** vs **SoLoud** vs **miniaudio** for the
  primary audio engine. Bias toward SDL3_mixer since we're already on
  SDL3, unless it lacks features the game needs (positional audio,
  arbitrary channel counts, low-latency triggering).
- Replace [sgp/soundman.cpp](../sgp/soundman.cpp) backend; keep the
  public API stable so the game's call sites don't change.
- Audit MSS-isms in headers ([sgp/Mss.h](../sgp/Mss.h),
  [sgp/Mss-old.h](../sgp/Mss-old.h)) — many call signatures are MSS-
  shaped (HSAMPLE/HSTREAM handles); these become opaque pointers
  backed by the new engine.
- Music streaming (Ogg/MP3) — ensure file-format support matches what
  the game actually ships.

Exit criteria: game plays SFX and music on all three platforms.

---

## Phase 8 — Cinematics

JA2 1.13 plays Smacker (`.smk`) and Bink (`.bik`) videos for the
intro/ending. These libraries are proprietary and not buildable on
non-Windows.

- Replace Smacker decoding with **libsmacker** (open-source, MIT-
  licensed, decodes original Smacker format).
- Bink is harder — RAD Game Tools' format and the decoder are
  closed. Options:
  1. Drop Bink playback. Convert the assets to Smacker/WebM offline
     as a one-time migration; ship those instead.
  2. Stub Bink to skip the affected cutscene on non-Windows.
  3. Use FFmpeg if licensing allows.
  Decision deferred until we audit which assets are Bink vs Smacker.
- Render decoded video frames into an `SDL_Texture` and present.

Exit criteria: cinematics play (or are gracefully skipped) on all
three platforms.

---

## Phase 9 — Fonts & GDI cleanup

- [sgp/WinFont.cpp](../sgp/WinFont.cpp) uses GDI to rasterize TrueType
  text. Most of the game uses pre-rendered bitmap fonts shipped in
  the asset bundle, so this code path is only used in a few places
  (mod-added text, debug overlays). Replace with **stb_truetype**
  (single-header, public domain).

Exit criteria: all text renders on all three platforms.

---

## Phase 10 — Platform packaging & CI

- macOS: produce an `.app` bundle, code-signing config (unsigned for
  now), resource path resolution via `SDL_GetBasePath`.
- Linux: AppImage or Flatpak (probably AppImage initially for
  simplicity).
- Windows: keep the existing installer flow if any; otherwise produce
  a zip.
- CI matrix: build all three on every PR. At minimum, "configures &
  compiles" on each; ideally also a headless boot smoke test.

Exit criteria: a tagged release artifact for each of the three
platforms.

---

## Open questions to resolve as we go

1. **Lua 5.1 vs Lua 5.4** — the TODO mentions building Lua from
   source. JA2 mods may depend on 5.1 semantics. Default: stay on
   5.1.x source build, defer any version bump.
2. **Multiplayer code** — currently a separate static lib. Keep
   linking, defer any porting unless we hit Win32 deps that block
   Phase 1.
3. **Editor builds** — JA2MAPEDITOR / JA2UBMAPEDITOR targets must
   stay compilable. Plan: editor uses the same SDL3 surface; no
   separate path.
4. **Threading** — audit for `CreateThread` / `_beginthread` usage
   and convert to `std::thread`.
5. **Save format compatibility** — must not change. Worth a fuzz
   round-trip test before merging Phase 6.

## Reference: upstream prior art

- [ja2-stracciatella](https://github.com/ja2-stracciatella/ja2-stracciatella) —
  ported the *original* JA2 to SDL2 over ~10 years. Their video,
  input, audio, and filesystem layers are a useful reference but
  cannot be copied wholesale — they diverged from the 1.13 lineage
  long ago and the 1.13 feature surface is dramatically larger.
  License is compatible (SFI-1.0 / GPL-style); attribution required
  if we lift specific code.
