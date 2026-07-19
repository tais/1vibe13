#!/usr/bin/env python3
"""Replace the bounded game-version placeholders used by release builds."""

from __future__ import annotations

import argparse
from pathlib import Path


def _portable_cpp_text(value: str, maximum_bytes: int) -> bytes:
    sanitized = "".join(
        character
        for character in value
        if 0x20 <= ord(character) <= 0x7E and character not in {'"', "\\"}
    )
    return sanitized.encode("ascii")[:maximum_bytes]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=Path("Ja2/GameVersion.cpp"))
    parser.add_argument("--version", required=True)
    parser.add_argument("--build", required=True)
    arguments = parser.parse_args()

    version = _portable_cpp_text(arguments.version, 15)
    build = _portable_cpp_text(arguments.build, 255)
    if not version:
        parser.error("the sanitized version label is empty")
    if not build:
        parser.error("the sanitized build label is empty")

    version_placeholder = b"@Version@"
    build_placeholder = b"@Build@"
    if any(
        placeholder in value
        for placeholder in (version_placeholder, build_placeholder)
        for value in (version, build)
    ):
        parser.error("version and build labels cannot contain template placeholders")

    source = arguments.source.read_bytes()
    if source.count(version_placeholder) != 1 or source.count(build_placeholder) != 1:
        parser.error("GameVersion.cpp must contain exactly one version and build placeholder")

    arguments.source.write_bytes(
        source.replace(version_placeholder, version).replace(build_placeholder, build)
    )
    print(
        f"Stamped {arguments.source} with version {version.decode('ascii')} "
        f"and build {build.decode('ascii')}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
