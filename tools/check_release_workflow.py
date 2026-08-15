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

RELEASE_SAVE_CONTRACT_STEP = "Run release save-contract tests"
RELEASE_SAVE_CONTRACT_TESTS = (
    "engine_core",
    "save_serializer_golden",
    "ja2_headless",
)
RELEASE_SAVE_CONTRACT_INVOCATIONS = tuple(
    "ctest --test-dir build --output-on-failure --no-tests=error "
    f"--tests-regex '^{test_name}$'"
    for test_name in RELEASE_SAVE_CONTRACT_TESTS)
RELEASE_SAVE_CONTRACT_COMMAND = " && ".join(
    RELEASE_SAVE_CONTRACT_INVOCATIONS)
EXPECTED_RELEASE_PLATFORMS = {
    ("ubuntu-latest", "linux", "linux"),
    ("ubuntu-24.04-arm", "linux-arm64", "linux"),
    ("macos-latest", "macos", "macos"),
    ("windows-latest", "windows", "windows"),
}


def release_steps(contents: str) -> list[tuple[str, str]]:
    """Return active package step names and bodies from the workflow."""

    matches = list(re.finditer(
        r"(?m)^ {6}- name: ([A-Za-z0-9][A-Za-z0-9 ()-]*)\s*$",
        contents))
    return [
        (match.group(1), contents[match.start():
                                  matches[index + 1].start()
                                  if index + 1 < len(matches)
                                  else len(contents)])
        for index, match in enumerate(matches)
    ]


def package_job(contents: str) -> str:
    """Return the one active package job, excluding sibling jobs."""

    matches = list(re.finditer(r"(?m)^ {2}package:\s*$", contents))
    if len(matches) != 1:
        fail("release workflow must contain exactly one active package job")
    start = matches[0].start()
    next_job = re.search(
        r"(?m)^ {2}[A-Za-z_][A-Za-z0-9_-]*:\s*$", contents[matches[0].end():])
    end = (matches[0].end() + next_job.start()
           if next_job is not None else len(contents))
    return contents[start:end]


def mapping_key(pattern: str, indent: int) -> re.Pattern[str]:
    """Match a plain or quoted YAML mapping key at one exact indentation."""

    return re.compile(
        rf"(?m)^ {{{indent}}}(?:{pattern}|'(?:{pattern})'|\"(?:{pattern})\")\s*:")


def fail(message: str) -> None:
    raise RuntimeError(message)


def validate_release_save_contract(contents: str) -> None:
    """Pin the unconditional, data-free save contract before staging."""

    job = package_job(contents)
    steps_markers = list(re.finditer(r"(?m)^ {4}steps:\s*$", job))
    if len(steps_markers) != 1:
        fail("package job must contain exactly one active steps list")
    job_header = job[:steps_markers[0].start()]
    if mapping_key("if|continue-on-error", 4).search(job_header):
        fail("package job must run unconditionally and block on failure")
    if re.search(r"(?m)^ {4}<<\s*:", job_header):
        fail("package job must not inherit hidden YAML controls")
    if not re.search(
            r"(?m)^ {4}runs-on:\s*\$\{\{\s*matrix[.]platform[.]os\s*}}\s*$",
            job_header):
        fail("package job must run on the declared platform matrix")
    if mapping_key("include|exclude", 8).search(job_header):
        fail("release platform matrix must not include or exclude hidden rows")
    if re.search(r"(?m)^ {6,8}<<\s*:", job_header):
        fail("release platform matrix must not inherit hidden YAML rows")
    platform_blocks = list(re.finditer(
        r"(?m)^ {8}platform:\s*$\n(?P<rows>(?:^ {10}[^\n]*\n?)+)",
        job_header))
    if len(platform_blocks) != 1:
        fail("package job must contain one active platform matrix")
    platform_rows = [
        tuple(value.strip() for value in match.groups())
        for match in re.finditer(
            r"(?m)^ {10}-\s*\{\s*os:\s*([^,}]+),\s*"
            r"name:\s*([^,}]+),\s*family:\s*([^,}]+)\s*}\s*$",
            platform_blocks[0].group("rows"))
    ]
    if (len(platform_rows) != len(EXPECTED_RELEASE_PLATFORMS)
            or set(platform_rows) != EXPECTED_RELEASE_PLATFORMS):
        fail("package job must retain exactly four active release platforms")

    steps = release_steps(job)
    names = [name for name, _body in steps]
    if names.count("Build") != 1:
        fail("release workflow must contain exactly one Build step")
    if names.count(RELEASE_SAVE_CONTRACT_STEP) != 1:
        fail("release workflow must contain exactly one save-contract step")

    build_position = names.index("Build")
    contract_position = names.index(RELEASE_SAVE_CONTRACT_STEP)
    staging_positions = [
        index for index, name in enumerate(names) if name.startswith("Stage ")]
    if not staging_positions:
        fail("release workflow lost its staging steps")
    if not build_position < contract_position < min(staging_positions):
        fail("release save-contract tests must run after Build and before staging")

    contract_step = steps[contract_position][1]
    if mapping_key("if", 8).search(contract_step):
        fail("release save-contract tests must run unconditionally")
    if mapping_key("continue-on-error", 8).search(contract_step):
        fail("release save-contract failures must block packaging")
    if re.search(r"(?m)^ {8}<<\s*:", contract_step):
        fail("release save-contract step must not inherit hidden YAML controls")
    if not re.search(r"(?m)^ {8}shell:\s*bash\s*$", contract_step):
        fail("release save-contract tests must use the Bash runner")
    if not re.search(
            r"(?m)^ {8}env:\s*$\n"
            r"^ {10}SDL_VIDEODRIVER:\s*dummy\s*$\n"
            r"^ {10}SDL_AUDIODRIVER:\s*dummy\s*$"
            r"(?!\n {10})",
            contract_step):
        fail("release save-contract tests require only the two dummy SDL drivers")

    run_lines = re.findall(r"(?m)^ {8}run:\s*(\S.*)\s*$", contract_step)
    if run_lines != [RELEASE_SAVE_CONTRACT_COMMAND]:
        fail("release save-contract tests must run exactly: "
             + RELEASE_SAVE_CONTRACT_COMMAND)


def validate_release_workflow(contents: str) -> None:
    validate_release_save_contract(contents)

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
