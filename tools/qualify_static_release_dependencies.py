#!/usr/bin/env python3
"""Verify that one release build contains no dynamically loaded SDL codecs."""

import argparse
from pathlib import Path
import sys

if __package__:
    from .check_static_audio_dependencies import validate_binary
else:
    from check_static_audio_dependencies import validate_binary


REQUIRED_CACHE_ENTRIES = (
    "BUILD_SHARED_LIBS:BOOL=OFF",
    "SDL_SHARED:BOOL=OFF",
    "SDL_STATIC:BOOL=ON",
    "SDLMIXER_VENDORED:BOOL=ON",
    "SDLMIXER_DEPS_SHARED:BOOL=OFF",
    "SDLMIXER_FLAC_LIBFLAC_SHARED:BOOL=OFF",
    "SDLMIXER_GME_SHARED:BOOL=OFF",
    "SDLMIXER_MOD_XMP_SHARED:BOOL=OFF",
    "SDLMIXER_OPUS_SHARED:BOOL=OFF",
    "SDLMIXER_VORBIS_VORBISFILE_SHARED:BOOL=OFF",
    "SDLMIXER_WAVPACK_SHARED:BOOL=OFF",
)


def release_binaries(build_directory: Path, family: str) -> tuple[Path, ...]:
    if family == "linux":
        names = (
            "JA2_ENGLISH",
            "JA2UB_ENGLISH",
            "JA2MAPEDITOR_ENGLISH",
            "tests/ja2server",
        )
    elif family == "macos":
        names = (
            "JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH",
            "JA2UB_ENGLISH.app/Contents/MacOS/JA2UB_ENGLISH",
            "JA2MAPEDITOR_ENGLISH.app/Contents/MacOS/JA2MAPEDITOR_ENGLISH",
            "tests/ja2server",
        )
    elif family == "windows":
        names = (
            "JA2_ENGLISH.exe",
            "JA2UB_ENGLISH.exe",
            "JA2MAPEDITOR_ENGLISH.exe",
            "tests/ja2server.exe",
        )
    else:
        raise ValueError(f"unsupported release platform family: {family}")
    return tuple(build_directory / name for name in names)


def validate_cache(cache_path: Path) -> None:
    active_entries = [
        line.strip()
        for line in cache_path.read_text(encoding="utf-8").splitlines()
        if line and not line.startswith(("#", "//"))
    ]
    for expected in REQUIRED_CACHE_ENTRIES:
        cache_key = expected.split("=", 1)[0] + "="
        matches = [entry for entry in active_entries
                   if entry.startswith(cache_key)]
        if matches != [expected]:
            rendered = ", ".join(matches) if matches else "missing"
            raise RuntimeError(
                f"release build requires {expected}; found {rendered}")


def qualify(build_directory: Path, family: str) -> tuple[Path, ...]:
    validate_cache(build_directory / "CMakeCache.txt")
    binaries = release_binaries(build_directory, family)
    for binary in binaries:
        validate_binary(binary)
    return binaries


def main(arguments: list[str]) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument(
        "--platform-family",
        required=True,
        choices=("linux", "macos", "windows"),
    )
    options = parser.parse_args(arguments)
    try:
        binaries = qualify(options.build_dir, options.platform_family)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"static release dependency qualification failed: {error}",
              file=sys.stderr)
        return 1
    print(f"qualified static dependencies in {len(binaries)} release binaries")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
