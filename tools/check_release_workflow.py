#!/usr/bin/env python3
"""Validate tagged-release packaging and ownership contracts without a build."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "release.yml"
WINDOWS_INSTALLER = ROOT / "packaging" / "windows" / "ja2-sdl3.nsi"
APPIMAGE_PREPARE = ROOT / "packaging" / "linux" / "prepare_appdir.sh"
APPIMAGE_BUILD = ROOT / "packaging" / "linux" / "build_appimage.sh"
APPIMAGE_RUN = ROOT / "packaging" / "linux" / "AppRun"


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


def validate_release_packaging(
        workflow: str,
        windows_installer: str,
        appimage_prepare: str,
        appimage_build: str,
        appimage_run: str) -> None:
    """Ratchet legacy zips, native packages, pins, and uninstall safety."""

    legacy_fragments = (
        "{ os: ubuntu-latest,    name: linux,       family: linux }",
        "{ os: ubuntu-24.04-arm, name: linux-arm64, family: linux }",
        "{ os: macos-latest,     name: macos,       family: macos }",
        "{ os: windows-latest,   name: windows,     family: windows }",
        "- name: Archive (unix)",
        '( cd dist && zip -r "../${PKG}.zip" . )',
        "- name: Archive (windows)",
        'Compress-Archive -Path dist/* -DestinationPath "$env:PKG.zip"',
        "name: ja2-sdl3-${{ matrix.platform.name }}",
        "path: ${{ env.ASSET }}",
        "files: ${{ env.ASSET }}",
    )
    missing = [fragment for fragment in legacy_fragments
               if fragment not in workflow]
    if missing:
        fail("release workflow changed the four-zip contract: "
             + ", ".join(missing))

    appimage_fragments = (
        '[[ ! "$RELEASE_LABEL" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$ ]]',
        'echo "RELEASE_LABEL=$RELEASE_LABEL" >> "$GITHUB_ENV"',
        'echo "SOURCE_DATE_EPOCH=$(git show -s --format=%ct "$GITHUB_SHA")"',
        "- name: Download checksum-pinned AppImage tools",
        "AppImage/appimagetool/releases/download/continuous/",
        "AppImage/type2-runtime/releases/download/continuous/",
        "a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0",
        "1cc49bcf1e2ccd593c379adb17c9f85a36d619088296504de95b1d06215aebbf",
        "1b00524ba8c6b678dc15ef88a5c25ec24def36cdfc7e3abb32ddcd068e8007fe",
        "7d5d772b7c32f0c84caf0a452a3072a5709027d7eac5856feb89a7a7a8881372",
        "sha256sum --check --strict",
        "- name: Build and reproduce AppImage",
        "packaging/linux/prepare_appdir.sh",
        "packaging/linux/build_appimage.sh",
        'cmp --silent "${PKG}.AppImage" "$second_image"',
        "- name: Upload AppImage workflow artifact",
        "- name: Attach AppImage to release",
    )
    missing = [fragment for fragment in appimage_fragments
               if fragment not in workflow]
    if missing:
        fail("release workflow lost reproducible AppImage packaging: "
             + ", ".join(missing))

    windows_fragments = (
        "- name: Download checksum-pinned NSIS compiler",
        "NSIS%203/3.12/nsis-3.12.zip",
        "56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f",
        "Get-FileHash -Algorithm SHA256",
        "nsis-3.12/makensis.exe",
        "- name: Build native Windows installer",
        '"/DRELEASE_LABEL=$env:RELEASE_LABEL"',
        "packaging/windows/ja2-sdl3.nsi",
        "- name: Smoke-test Windows installer ownership",
        "must-survive-uninstall.txt",
        "preserve this operator configuration",
        "preserve this pre-existing uninstaller",
        "- name: Upload Windows installer workflow artifact",
        "- name: Attach Windows installer to release",
    )
    missing = [fragment for fragment in windows_fragments
               if fragment not in workflow]
    if missing:
        fail("release workflow lost native Windows packaging: "
             + ", ".join(missing))

    unix_archive_position = workflow.find("- name: Archive (unix)")
    appimage_position = workflow.find("- name: Build and reproduce AppImage")
    archive_position = workflow.find("- name: Archive (windows)")
    native_position = workflow.find("- name: Build native Windows installer")
    upload_position = workflow.find("- name: Upload workflow artifact")
    if min(unix_archive_position, appimage_position, archive_position,
           native_position, upload_position) < 0 or not (
            unix_archive_position < appimage_position < upload_position
            and archive_position < native_position < upload_position):
        fail("native packages must be built from staged payloads before upload")

    for line in workflow.splitlines():
        stripped = line.strip()
        if not stripped.startswith("uses:"):
            continue
        action = stripped[len("uses:"):].split("#", 1)[0].strip()
        if not re.fullmatch(r"[^@\s]+@[0-9a-f]{40}", action):
            fail("release actions must use an immutable full commit SHA: "
                 + action)

    installer_fragments = (
        "RequestExecutionLevel user",
        '!error "RELEASE_LABEL must name the validated tagged release"',
        'InstallDir "$LOCALAPPDATA\\Programs\\JA2-1.13-SDL3"',
        'WriteUninstaller "$INSTDIR\\Uninstall-JA2-SDL3.exe"',
        "WriteRegStr HKCU",
        'Delete "$INSTDIR\\JA2_ENGLISH.exe"',
        'Delete "$INSTDIR\\JA2UB_ENGLISH.exe"',
        'Delete "$INSTDIR\\JA2MAPEDITOR_ENGLISH.exe"',
        'Delete "$INSTDIR\\ja2server\\ja2server.exe"',
        'File /oname=ja2_mp.ini.sample',
        'Delete "$INSTDIR\\ja2server\\ja2_mp.ini.sample"',
        'RMDir "$INSTDIR"',
    )
    for line in windows_installer.splitlines():
        command = line.strip().lower()
        if command.startswith("rmdir ") and "/r" in command.split():
            fail("Windows uninstaller must never recursively remove a directory")
        if command.startswith("delete ") and "*" in command:
            fail("Windows uninstaller must name every owned file explicitly")
        if command.startswith("file ") and "/oname=ja2_mp.ini " in command:
            fail("Windows installer must not overwrite user-owned server config")
        if command == 'delete "$instdir\\ja2_mp.ini"' or command == (
                'delete "$instdir\\ja2server\\ja2_mp.ini"'):
            fail("Windows uninstaller must preserve user-owned server config")
        if command == 'writeuninstaller "$instdir\\uninstall.exe"' or (
                command == 'delete "$instdir\\uninstall.exe"'):
            fail("Windows package must not claim a generic uninstaller name")
    missing = [fragment for fragment in installer_fragments
               if fragment not in windows_installer]
    if missing:
        fail("Windows installer lost its per-user ownership contract: "
             + ", ".join(missing))

    prepare_fragments = (
        "missing or unsafe AppImage payload file",
        "refusing to replace existing AppDir",
        "ja2_mp.ini.sample",
        "SOURCE_DATE_EPOCH",
        "touch --no-dereference",
    )
    build_fragments = (
        "refusing to overwrite AppImage output",
        "--runtime-file",
        "SOURCE_DATE_EPOCH",
        "--appimage-offset",
    )
    run_fragments = (
        "--ja2ub",
        "--editor",
        "--server",
        'appimage_directory/Ja2.ini',
        'appimage_directory/ja2_mp.ini',
        "server_ini_explicit",
        "The AppImage contains the engine",
    )
    for label, contents, fragments in (
            ("AppDir preparation", appimage_prepare, prepare_fragments),
            ("AppImage build", appimage_build, build_fragments),
            ("AppImage launcher", appimage_run, run_fragments)):
        missing = [fragment for fragment in fragments
                   if fragment not in contents]
        if missing:
            fail(f"{label} lost: " + ", ".join(missing))
        if "rm -rf" in contents or "rm -fr" in contents:
            fail(f"{label} must not recursively replace packaging paths")


def main() -> int:
    try:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        validate_release_workflow(workflow)
        validate_release_packaging(
            workflow,
            WINDOWS_INSTALLER.read_text(encoding="utf-8"),
            APPIMAGE_PREPARE.read_text(encoding="utf-8"),
            APPIMAGE_BUILD.read_text(encoding="utf-8"),
            APPIMAGE_RUN.read_text(encoding="utf-8"))
    except (OSError, RuntimeError) as error:
        print(f"release workflow check failed: {error}", file=sys.stderr)
        return 1

    print("Release workflow signing, packaging, and ownership verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
