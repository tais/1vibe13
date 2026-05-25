
# Jagged Alliance 2 v1.13 — SDL3 port

<br />
<br />

<p align="center">
  <img src="ja2v1.13.png" alt="JA2 v1.13">
</p>

<br />

A **cross-platform** source port of Jagged Alliance 2 v1.13. The original
Win32 / DirectDraw / DirectInput / FMOD / GDI / Smacker stack has been
replaced with portable **SDL3**-based equivalents and a 32-bit colour
rendering pipeline (up from the original 16-bit), so the same code now builds and runs natively on
**Windows, macOS and Linux**. The toolchain is **CMake + clang + Ninja** —
**no Visual Studio / MSVC** project, no `.sln`/`.vcxproj`.

> JA2 v1.13 is a long-running community engine/mod for Jagged Alliance 2.
> Upstream development moved from SVN to GitHub in 2022. This repository is
> the SDL3 cross-platform port of that codebase.

For background and community:
- [The Bear's Pit Forum](https://thepit.ja-galaxy-forum.com)
- [JA2 v1.13 Starter Documentation](https://github.com/1dot13/documentation)
- [The Bear's Pit Discord](https://discord.gg/GqrVZUM)


## ⚠️ Disclaimer

This SDL3 port is **entirely "vibecoded"** — developed rapidly with heavy
AI assistance and a lot of iterative play-testing rather than a formal,
audited process. It is provided **as-is, with absolutely no warranty of any
kind**: no guarantee of correctness, stability, save-game integrity, or
fitness for any purpose, and **no responsibility is accepted for any software
or hardware breakage**, data loss, corrupted saves, or any other mishap that
may result from building or running it. Use at your own risk, keep backups,
and have fun. :)


## What's different in this port

| Area | Was (Win32) | Now |
|---|---|---|
| Platforms | Windows only | Windows, macOS, Linux (one SDL3 codebase) |
| Build | Visual Studio / MSVC | CMake (≥ 3.20) + clang + Ninja, C++17 |
| Window / video | DirectDraw, 16-bit colour | SDL3, 32-bit colour internally |
| Input | DirectInput + Win32 hooks | SDL3 events |
| Audio | FMOD (`fmodvc.lib`) / Miles (`mss32.lib`) | SDL3_mixer |
| Cinematics | Smacker via `binkw32.lib` / `SMACKW32.LIB` | libsmacker (Bink path stubbed — no shipped `.bik`) |
| Dependencies | prebuilt MSVC `.lib` blobs in-tree | built from source (FetchContent) / vendored |
| Display shim | cnc-ddraw | none — SDL3 owns the window & scaling |
| Multiplayer | RakNet (32-bit Win32 `.lib`) | stubbed / disabled (not ported) |


## Dependencies

Everything is fetched-from-source or vendored — there are **no system
package requirements** for the libraries themselves and **no prebuilt
`.lib` blobs** checked into the repo.

| Dependency | Version | How |
|---|---|---|
| SDL3 | `main` | FetchContent (`libsdl-org/SDL`) |
| SDL3_mixer | `main` | FetchContent (`libsdl-org/SDL_mixer`) |
| Lua | 5.5.0 | FetchContent (lua.org) |
| Expat | 2.8.1 | FetchContent (`libexpat`) |
| zlib | 1.3.2 | vendored (`ext/zlib`) |
| libpng | 1.6.58 | vendored (`ext/libpng`) |
| utfcpp | 4.1.1 | vendored |
| 7-Zip / LZMA SDK | 26.01 | vendored (used by bfVFS) |
| libsmacker | vendored | `ext/libsmacker` (SMK cinematics) |
| bfVFS | vendored | `ext/VFS` (virtual file system: SLF / 7z / folders) |

SDL3 and SDL3_mixer are tracked from `main` (ahead of any tagged release),
so the bundled runtime libraries must come from this build — see the
release packaging notes below.


## Building from source

Requirements: **CMake ≥ 3.20**, **clang**, **Ninja**, **git**.

```bash
git clone https://github.com/tais/source.git ja2-source
cd ja2-source
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLanguages=ENGLISH \
  "-DApplications=JA2;JA2UB;JA2MAPEDITOR"
cmake --build build
```

Build options:

- **`Languages`** (one or more): `CHINESE DUTCH ENGLISH FRENCH GERMAN ITALIAN POLISH RUSSIAN`
- **`Applications`** (one or more): `JA2`, `JA2UB` (Unfinished Business), `JA2MAPEDITOR`, `JA2UBMAPEDITOR`

Each app is built per language, e.g. `JA2_ENGLISH` / `JA2_ENGLISH.exe`.

### Linux

```bash
sudo apt install clang cmake ninja-build pkg-config \
  libwayland-dev wayland-protocols libdecor-0-dev \
  libx11-dev libxext-dev libxcursor-dev libxi-dev libxinerama-dev \
  libxrandr-dev libxss-dev libxkbcommon-dev libxtst-dev libxfixes-dev \
  libgl1-mesa-dev libegl1-mesa-dev \
  libasound2-dev libpulse-dev libdbus-1-dev libudev-dev libibus-1.0-dev
export CC=clang CXX=clang++
# then the cmake commands above
```

### macOS

```bash
brew install cmake ninja        # clang ships with the Command Line Tools
# then the cmake commands above
```

### Windows

Build with **clang targeting the MSVC ABI** (no Visual Studio IDE needed, but
the MSVC SDK/CRT headers must be available — e.g. from the VS Build Tools or
the `ilammy/msvc-dev-cmd` environment in CI):

```bash
export CC=clang CXX=clang++
export CFLAGS=--target=x86_64-pc-windows-msvc
export CXXFLAGS=--target=x86_64-pc-windows-msvc
# then the cmake commands above
```

CI (`.github/workflows/build_unix.yml`) compiles all three platforms on every
push; `.github/workflows/release.yml` packages per-platform zips on a `v*` tag.


## Downloads

Pre-built, per-platform archives are on the
[releases page](https://github.com/tais/source/releases) — `windows`,
`macos` and `linux` zips. Each archive contains the **engine executables plus
the matching SDL3 / SDL3_mixer runtime libraries** (the bundled SDL libs are
the ones this build produced — do not mix with a different SDL release).

The archives contain the **engine only, no game data**.


## Installation

1. Install the original **Jagged Alliance 2** and a working **JA2 1.13** data set.
2. Download the archive for your platform and drop its contents into the JA2
   game directory (overwrite when asked).
3. Adjust the `.ini` settings if you like, then run the executable.

No `cnc-ddraw` / DirectDraw shim is required — SDL3 handles the window,
fullscreen and scaling on every platform.


## Project status

The port landed in phases (see [`docs/SDL3_PORT.md`](docs/SDL3_PORT.md) for the
full plan). Window, input, video, audio and cinematics are all SDL3-backed,
colour is 32-bit internally, and the build is CMake-only with dependencies
pulled from source. Multiplayer is deferred (currently stubbed).


## Reports & participation

- Issues / PRs are welcome here on GitHub.
- Bug reports & discussion: [Bear's Pit Forum](http://thepit.ja-galaxy-forum.com/index.php?t=thread&frm_id=216&) · [Bear's Pit Discord](https://discord.gg/GqrVZUM)
