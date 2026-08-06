#!/usr/bin/env python3
"""Validate the macOS bundle-signing contract in the release workflow."""

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "release.yml"


def fail(message: str) -> None:
    raise RuntimeError(message)


def validate_release_workflow(contents: str) -> None:
    stage_marker = "- name: Stage payload (macos)"
    sign_marker = "- name: Sign and verify macOS app bundles"
    archive_marker = "- name: Archive (unix)"
    stage_position = contents.find(stage_marker)
    sign_position = contents.find(sign_marker)
    archive_position = contents.find(archive_marker)
    if min(stage_position, sign_position, archive_position) < 0:
        fail("release workflow lost macOS staging, signing, or archiving")
    if not stage_position < sign_position < archive_position:
        fail("macOS bundles must be signed after staging and before archiving")

    next_step = contents.find("\n      - name:", sign_position + len(sign_marker))
    if next_step < 0:
        fail("cannot determine the macOS signing step boundary")
    signing_step = contents[sign_position:next_step]
    required_fragments = (
        "if: matrix.platform.name == 'macos'",
        "for app in JA2 JA2UB JA2MAPEDITOR; do",
        'bundle="dist/${app}_ENGLISH.app"',
        'codesign --force --deep --sign - "$bundle"',
        'codesign --verify --deep --strict --verbose=2 "$bundle"',
    )
    missing = [fragment for fragment in required_fragments
               if fragment not in signing_step]
    if missing:
        fail("macOS signing step lost: " + ", ".join(missing))


def main() -> int:
    try:
        validate_release_workflow(WORKFLOW.read_text(encoding="utf-8"))
    except (OSError, RuntimeError) as error:
        print(f"release workflow check failed: {error}", file=sys.stderr)
        return 1

    print("Release workflow macOS bundle signing verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
