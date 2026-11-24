#!/usr/bin/env python3
"""Reject release images that still name dynamically loaded audio codecs."""

from pathlib import Path
import re
import sys


DYNAMIC_AUDIO_LIBRARY_PATTERN = re.compile(
    rb"(?<![A-Za-z0-9_.-])(?:lib)?"
    rb"(?:FLAC|gme|ogg|opus(?:file)?|vorbis(?:file|idec)?|"
    rb"wavpack(?:dll)?|xmp(?:-lite)?|mpg123|fluidsynth|modplug|sndfile)"
    rb"(?:[-_.][A-Za-z0-9]+)*"
    rb"(?:[.]dylib|[.]so(?:[.][0-9]+)*|[.]dll)"
    rb"(?![A-Za-z0-9_.-])",
    re.IGNORECASE,
)


def dynamic_audio_libraries(image: bytes) -> tuple[str, ...]:
    """Return unique dynamic codec names embedded in one executable image."""

    return tuple(sorted({
        match.group(0).decode("ascii", errors="replace")
        for match in DYNAMIC_AUDIO_LIBRARY_PATTERN.finditer(image)
    }, key=str.casefold))


def validate_binary(path: Path) -> None:
    matches = dynamic_audio_libraries(path.read_bytes())
    if matches:
        raise RuntimeError(
            f"{path} embeds dynamic audio-library names: {', '.join(matches)}")


def main(arguments: list[str]) -> int:
    if not arguments:
        print("usage: check_static_audio_dependencies.py BINARY...", file=sys.stderr)
        return 64
    try:
        for argument in arguments:
            validate_binary(Path(argument))
    except (OSError, RuntimeError) as error:
        print(f"static audio dependency check failed: {error}", file=sys.stderr)
        return 1
    print(f"qualified static audio dependencies in {len(arguments)} binaries")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
