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
MACOS_SHARED_SDL_PATTERN = (
    r"(^|/)(lib)?SDL3(_mixer|_net)?([.][0-9]+)*[.]dylib$|"
    r"(^|/)SDL3(_mixer|_net)?[.]framework(/|$)")
STATIC_SDL_QUALIFICATION_STEP = "Qualify static SDL dependency configuration"
STATIC_SDL_QUALIFICATION_COMMAND = (
    "python3 tools/qualify_static_release_dependencies.py "
    "--build-dir build --platform-family '${{ matrix.platform.family }}'"
)
EXPECTED_RELEASE_PLATFORMS = {
    ("ubuntu-22.04", "linux-x86_64-glibc2.35", "linux", "x86_64"),
    ("ubuntu-22.04-arm", "linux-arm64-glibc2.35", "linux", "arm64"),
    ("macos-15-intel", "macos-x86_64-macos12", "macos", "x86_64"),
    ("macos-15", "macos-arm64-macos12", "macos", "arm64"),
    ("windows-2022", "windows-x86_64", "windows", "x86_64"),
}
NSIS_DOWNLOAD_STEP = "Download checksum-pinned NSIS compiler"
NSIS_URL = (
    "https://downloads.sourceforge.net/project/nsis/"
    "NSIS%203/3.12/nsis-3.12.zip")
NSIS_SHA256 = (
    "56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f")
NSIS_STEP_PREFIX = (
    "      - name: Download checksum-pinned NSIS compiler\n"
    "        if: matrix.platform.family == 'windows'\n"
    "        shell: pwsh\n"
    "        run: |\n")
NSIS_SETUP_BLOCK = (
    "          $ErrorActionPreference = 'Stop'\n"
    "          $tools = Join-Path $PWD '.release-tools'\n"
    "          New-Item -ItemType Directory -Force $tools | Out-Null\n"
    "          $archive = Join-Path $tools 'nsis-3.12.zip'\n")
NSIS_URL_LINE = f"          $url = '{NSIS_URL}'\n"
NSIS_EXPECTED_LINE = f"          $expected = '{NSIS_SHA256}'\n"
NSIS_CURL_BLOCK = (
    "          & curl.exe --proto '=https' --tlsv1.2 --fail --location `\n"
    "            --retry 5 --retry-all-errors --output $archive $url\n"
    "          if ($LASTEXITCODE -ne 0) {\n"
    "            throw \"NSIS archive download failed with curl exit code "
    "$LASTEXITCODE\"\n"
    "          }\n")
NSIS_HASH_BLOCK = (
    "          $actual = (Get-FileHash -Algorithm SHA256 "
    "$archive).Hash.ToLowerInvariant()\n"
    "          if ($actual -ne $expected) {\n"
    "            throw \"NSIS archive checksum mismatch: expected $expected, "
    "got $actual\"\n"
    "          }\n")
NSIS_EXPAND_LINE = (
    "          Expand-Archive -LiteralPath $archive -DestinationPath $tools\n")
NSIS_COMPILER_BLOCK = (
    "          # Use the portable distribution's public launcher. It resolves "
    "the\n"
    "          # matching compiler and its Include/Contrib/Stubs directories.\n"
    "          $compiler = Join-Path $tools 'nsis-3.12/makensis.exe'\n"
    "          if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) {\n"
    "            throw \"NSIS compiler was not present after extraction\"\n"
    "          }\n"
    "          \"NSIS_MAKENSIS=$compiler\" | Out-File -FilePath "
    "$env:GITHUB_ENV -Append\n")
NSIS_EXPECTED_STEP = (
    NSIS_STEP_PREFIX + NSIS_SETUP_BLOCK + NSIS_URL_LINE + NSIS_EXPECTED_LINE
    + NSIS_CURL_BLOCK + NSIS_HASH_BLOCK + NSIS_EXPAND_LINE
    + NSIS_COMPILER_BLOCK + "\n")
RESOLVE_PACKAGE_NAME_STEP = "Resolve package name"
RESOLVE_PACKAGE_NAME_EXPECTED_STEP = (
    "      - name: Resolve package name\n"
    "        shell: bash\n"
    "        run: |\n"
    "          set -eux\n"
    "          echo \"PKG=ja2-sdl3-${{ matrix.platform.name }}-"
    "${RELEASE_LABEL}\" >> \"$GITHUB_ENV\"\n"
    "          echo \"SDK_PKG=ja2-engine-sdk-${{ matrix.platform.name }}-"
    "${RELEASE_LABEL}\" >> \"$GITHUB_ENV\"\n\n")


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
            r"name:\s*([^,}]+),\s*family:\s*([^,}]+),\s*"
            r"arch:\s*([^,}]+)\s*}\s*$",
            platform_blocks[0].group("rows"))
    ]
    if (len(platform_rows) != len(EXPECTED_RELEASE_PLATFORMS)
            or set(platform_rows) != EXPECTED_RELEASE_PLATFORMS):
        fail("package job must retain exactly five qualified release platforms")

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

    steps = release_steps(package_job(contents))
    names = [name for name, _body in steps]
    required_steps = (
        "Stage payload (macos)",
        "Sign and verify macOS app bundles",
        "Archive (unix)",
    )
    if any(names.count(name) != 1 for name in required_steps):
        fail("release workflow lost macOS staging, signing, or archiving")
    stage_position = names.index(required_steps[0])
    sign_position = names.index(required_steps[1])
    archive_position = names.index(required_steps[2])
    if not stage_position < sign_position < archive_position:
        fail("macOS bundles must be signed after staging and before archiving")

    signing_step = steps[sign_position][1]
    if mapping_key("continue-on-error", 8).search(signing_step):
        fail("macOS signing failures must block packaging")
    if re.search(r"(?m)^ {8}<<\s*:", signing_step):
        fail("macOS signing step must not inherit hidden YAML controls")
    conditions = re.findall(
        r"(?m)^ {8}(?:if|'if'|\"if\")\s*:[^\n]*$", signing_step)
    if conditions != ["        if: matrix.platform.family == 'macos'"]:
        fail("macOS signing step must retain only its exact platform condition")
    if not re.search(r"(?m)^ {8}shell:\s*bash\s*$", signing_step):
        fail("macOS signing step must use the Bash runner")
    shell_commands = tuple(
        line.strip() for line in signing_step.splitlines()
        if line.startswith("          ") and line.strip()
        and not line.lstrip().startswith("#")
    )
    expected_commands = (
        "set -eux",
        "for app in JA2 JA2UB JA2MAPEDITOR; do",
        'bundle="dist/${app}_ENGLISH.app"',
        'codesign --force --deep --sign - "$bundle"',
        'codesign --verify --deep --strict --verbose=2 "$bundle"',
        "done",
    )
    if shell_commands != expected_commands:
        fail("macOS signing step lost its exact fail-closed command sequence")


def validate_nsis_download(workflow: str) -> None:
    """Require the bounded, fail-closed NSIS acquisition statement sequence."""

    matches = [body for name, body in release_steps(package_job(workflow))
               if name == NSIS_DOWNLOAD_STEP]
    if len(matches) != 1:
        fail("release workflow must contain exactly one active NSIS download step")
    step = matches[0]

    if mapping_key("continue-on-error", 8).search(step):
        fail("NSIS download failures must block packaging")
    conditions = re.findall(
        r"(?m)^ {8}(?:if|'if'|\"if\")\s*:[^\n]*$", step)
    if conditions != ["        if: matrix.platform.family == 'windows'"]:
        fail("NSIS download must retain only its Windows matrix condition")
    if re.search(r"(?m)^ {8}<<\s*:", step):
        fail("NSIS download step must not inherit hidden YAML controls")
    if "Invoke-WebRequest" in step:
        fail("NSIS download must not use Invoke-WebRequest")
    if NSIS_URL_LINE not in step:
        fail("NSIS download must retain its exact SourceForge URL assignment")
    if NSIS_CURL_BLOCK not in step:
        fail("NSIS download must use the pinned curl.exe transport contract")
    if NSIS_EXPECTED_LINE not in step or NSIS_HASH_BLOCK not in step:
        fail("NSIS download must retain its exact SHA-256 verification contract")
    ordered_blocks = (
        NSIS_URL_LINE,
        NSIS_EXPECTED_LINE,
        NSIS_CURL_BLOCK,
        NSIS_HASH_BLOCK,
        NSIS_EXPAND_LINE,
    )
    positions = [step.find(block) for block in ordered_blocks]
    if min(positions) < 0 or positions != sorted(positions):
        fail("NSIS download must directly order curl, exit gate, hash, "
             "mismatch gate, then extraction")
    if step != NSIS_EXPECTED_STEP:
        fail("NSIS download step contains unapproved controls or statements")


def validate_release_compatibility(workflow: str) -> None:
    """Require versioned toolchains and fail-closed binary baseline checks."""

    job = package_job(workflow)
    steps = release_steps(job)
    names = [name for name, _body in steps]

    configure_steps = [body for name, body in steps if name == "Configure"]
    if len(configure_steps) != 1:
        fail("release workflow must contain exactly one Configure step")
    configure = configure_steps[0]
    configure_fragments = (
        "platform_args=()",
        'if [[ "${{ matrix.platform.family }}" == macos ]]; then',
        '"-DCMAKE_OSX_ARCHITECTURES=${{ matrix.platform.arch }}"',
        "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
        "-DCMAKE_DISABLE_FIND_PACKAGE_SDL3=ON",
        "-DCMAKE_DISABLE_FIND_PACKAGE_SDL3_mixer=ON",
        "-DCMAKE_DISABLE_FIND_PACKAGE_SDL3_net=ON",
        "-DSDLMIXER_DEPS_SHARED=OFF",
        '${platform_args[@]+"${platform_args[@]}"}',
    )
    missing = [fragment for fragment in configure_fragments
               if fragment not in configure]
    if missing:
        fail("release configure lost its macOS deployment contract: "
             + ", ".join(missing))

    static_steps = [
        (index, body) for index, (name, body) in enumerate(steps)
        if name == STATIC_SDL_QUALIFICATION_STEP
    ]
    if len(static_steps) != 1:
        fail("release workflow must contain exactly one static SDL qualification step")
    static_position, static_step = static_steps[0]
    if mapping_key("if|continue-on-error", 8).search(static_step):
        fail("static SDL qualification must run unconditionally and block packaging")
    if re.search(r"(?m)^ {8}<<\s*:", static_step):
        fail("static SDL qualification must not inherit hidden YAML controls")
    if not re.search(r"(?m)^ {8}shell:\s*bash\s*$", static_step):
        fail("static SDL qualification must use the Bash runner")
    run_lines = re.findall(r"(?m)^ {8}run:\s*(\S.*)\s*$", static_step)
    if run_lines != [STATIC_SDL_QUALIFICATION_COMMAND]:
        fail("static SDL qualification must run exactly: "
             + STATIC_SDL_QUALIFICATION_COMMAND)

    requirements = {
        "Qualify Linux release binaries": (
            "expected_machine='Advanced Micro Devices X86-64'",
            "expected_machine='AArch64'",
            'readelf --file-header "$binary"',
            'dynamic_table="$(readelf --dynamic "$binary")"',
            '<<< "$dynamic_table"',
            "libSDL3(_mixer|_net)?[.]so",
            "unexpectedly depends on a shared SDL3 library",
            '["readelf", "--version-info", binary]',
            "version > (2, 35)",
            "GLIBC newer than 2.35",
            "build/JA2_ENGLISH",
            "build/JA2UB_ENGLISH",
            "build/JA2MAPEDITOR_ENGLISH",
            "build/tests/ja2server",
        ),
        "Qualify macOS release binaries": (
            'actual_arch="$(lipo -archs "$binary")"',
            '"${{ matrix.platform.arch }}"',
            'otool -l "$binary"',
            'dependency_table="$(otool -L "$binary")"',
            "awk '{print $1}'",
            MACOS_SHARED_SDL_PATTERN,
            "unexpectedly depends on a shared SDL3 library",
            "grep -Ev '^(/usr/lib/|/System/Library/)'",
            "has non-system dynamic dependencies",
            'rpaths="$(otool -l "$binary" | awk',
            'LC_RPATH',
            "has runtime search paths despite static packaging",
            'LC_BUILD_VERSION',
            'if [[ "$minos" != 12.0 ]]',
            "build/JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH",
            "build/JA2UB_ENGLISH.app/Contents/MacOS/JA2UB_ENGLISH",
            "build/JA2MAPEDITOR_ENGLISH.app/Contents/MacOS/JA2MAPEDITOR_ENGLISH",
            "build/tests/ja2server",
        ),
        "Qualify Windows release binaries": (
            "build/JA2_ENGLISH.exe",
            "build/JA2UB_ENGLISH.exe",
            "build/JA2MAPEDITOR_ENGLISH.exe",
            "build/tests/ja2server.exe",
            'image[:2] != b"MZ"',
            'image[pe_offset:pe_offset + 4] != b"PE\\0\\0"',
            "machine != 0x8664",
            'import_table="$(llvm-readobj --coff-imports "$binary")"',
            '<<< "$import_table"',
            "SDL3(_mixer|_net)?[.]dll",
            "unexpectedly depends on a shared SDL3 library",
        ),
    }
    expected_conditions = {
        "Qualify Linux release binaries":
            "        if: matrix.platform.family == 'linux'",
        "Qualify macOS release binaries":
            "        if: matrix.platform.family == 'macos'",
        "Qualify Windows release binaries":
            "        if: matrix.platform.family == 'windows'",
    }
    expected_active_lines = {
        "Qualify Linux release binaries": (
            '            dynamic_table="$(readelf --dynamic "$binary")"',
            '                <<< "$dynamic_table" >/dev/null; then',
        ),
        "Qualify macOS release binaries": (
            '            dependency_table="$(otool -L "$binary")"',
            "                '" + MACOS_SHARED_SDL_PATTERN + "' \\",
            '            unexpected_dependencies="$(',
            '            rpaths="$(otool -l "$binary" | awk \'',
        ),
        "Qualify Windows release binaries": (
            '            import_table="$(llvm-readobj --coff-imports "$binary")"',
            '                <<< "$import_table" >/dev/null; then',
        ),
    }

    qualification_positions = [static_position]
    for step_name, fragments in requirements.items():
        matches = [
            (index, body) for index, (name, body) in enumerate(steps)
            if name == step_name
        ]
        if len(matches) != 1:
            fail(f"release workflow must contain exactly one {step_name} step")
        position, body = matches[0]
        qualification_positions.append(position)
        if mapping_key("continue-on-error", 8).search(body):
            fail(f"{step_name} failures must block packaging")
        if re.search(r"(?m)^ {8}<<\s*:", body):
            fail(f"{step_name} must not inherit hidden YAML controls")
        conditions = re.findall(
            r"(?m)^ {8}(?:if|'if'|\"if\")\s*:[^\n]*$", body)
        if conditions != [expected_conditions[step_name]]:
            fail(f"{step_name} must retain only its exact platform condition")
        active_lines = body.splitlines()
        missing_active = [line for line in expected_active_lines[step_name]
                          if active_lines.count(line) != 1]
        if missing_active:
            fail(f"{step_name} lost its fail-closed active inspection commands")
        if not re.search(r"(?m)^ {8}shell:\s*bash\s*$", body):
            fail(f"{step_name} must use the Bash runner")
        missing = [fragment for fragment in fragments if fragment not in body]
        if missing:
            fail(f"{step_name} lost: " + ", ".join(missing))

    if names.count("Build") != 1:
        fail("release workflow must contain exactly one Build step")
    staging_positions = [
        index for index, name in enumerate(names) if name.startswith("Stage ")]
    if not staging_positions or not all(
            names.index("Build") < position < min(staging_positions)
            for position in qualification_positions):
        fail("release binary qualification must run after Build and before staging")


def validate_release_package_names(workflow: str) -> None:
    """Require unique, architecture-bearing game and SDK archive basenames."""

    matches = [body for name, body in release_steps(package_job(workflow))
               if name == RESOLVE_PACKAGE_NAME_STEP]
    if len(matches) != 1:
        fail("release workflow must contain exactly one package-name step")
    if matches[0] != RESOLVE_PACKAGE_NAME_EXPECTED_STEP:
        fail("release package names must be assigned exactly from platform and label")


def validate_release_packaging(
        workflow: str,
        windows_installer: str,
        appimage_prepare: str,
        appimage_build: str,
        appimage_run: str) -> None:
    """Ratchet portable zips, native packages, pins, and uninstall safety."""

    validate_nsis_download(workflow)
    validate_release_compatibility(workflow)
    validate_release_package_names(workflow)

    portable_zip_fragments = (
        "{ os: ubuntu-22.04,     name: linux-x86_64-glibc2.35, family: linux,   arch: x86_64 }",
        "{ os: ubuntu-22.04-arm, name: linux-arm64-glibc2.35,  family: linux,   arch: arm64 }",
        "{ os: macos-15-intel,   name: macos-x86_64-macos12,   family: macos,   arch: x86_64 }",
        "{ os: macos-15,         name: macos-arm64-macos12,    family: macos,   arch: arm64 }",
        "{ os: windows-2022,     name: windows-x86_64,         family: windows, arch: x86_64 }",
        "binutils clang cmake ninja-build pkg-config",
        "- name: Archive (unix)",
        '( cd dist && zip -r "../${PKG}.zip" . )',
        "- name: Archive (windows)",
        'Compress-Archive -Path dist/* -DestinationPath "$env:PKG.zip"',
        "name: ja2-sdl3-${{ matrix.platform.name }}",
        "path: ${{ env.ASSET }}",
        "files: ${{ env.ASSET }}",
    )
    missing = [fragment for fragment in portable_zip_fragments
               if fragment not in workflow]
    if missing:
        fail("release workflow changed the five-platform portable zip contract: "
             + ", ".join(missing))

    appimage_fragments = (
        '[[ ! "$RELEASE_LABEL" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$ ]]',
        'echo "RELEASE_LABEL=$RELEASE_LABEL" >> "$GITHUB_ENV"',
        'echo "SOURCE_DATE_EPOCH=$(git show -s --format=%ct "$GITHUB_SHA")"',
        "- name: Download checksum-pinned AppImage tools",
        "AppImage/appimagetool/releases/download/1.9.1/",
        "AppImage/type2-runtime/releases/download/20251108/",
        "ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0",
        "2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d",
        "f0837e7448a0c1e4e650a93bb3e85802546e60654ef287576f46c71c126a9158",
        "00cbdfcf917cc6c0ff6d3347d59e0ca1f7f45a6df1a428a0d6d8a78664d87444",
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
        NSIS_URL,
        NSIS_SHA256,
        "nsis-3.12/makensis.exe",
        "- name: Build native Windows installer",
        '"/DRELEASE_LABEL=$env:RELEASE_LABEL"',
        "packaging/windows/ja2-sdl3.nsi",
        "- name: Smoke-test Windows installer ownership",
        "must-survive-uninstall.txt",
        "preserve this operator configuration",
        "preserve this pre-existing uninstaller",
        "- name: Upload Windows installer workflow artifact",
        "name: ja2-sdl3-${{ matrix.platform.name }}-installer",
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
