#!/usr/bin/env python3
"""Tests for the tagged-release signing and native-package ratchets."""

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from tools.check_release_workflow import (
    MACOS_SHARED_SDL_PATTERN,
    RELEASE_SAVE_CONTRACT_COMMAND,
    RELEASE_SAVE_CONTRACT_INVOCATIONS,
    validate_release_packaging,
    validate_release_save_contract,
    validate_release_workflow,
)
from tools.check_static_audio_dependencies import dynamic_audio_libraries
from tools.qualify_static_release_dependencies import (
    REQUIRED_CACHE_ENTRIES,
    release_binaries,
    validate_cache,
)


VALID_WORKFLOW = """
  package:
    name: package (${{ matrix.platform.name }})
    strategy:
      matrix:
        platform:
          - { os: ubuntu-22.04,     name: linux-x86_64-glibc2.35, family: linux,   arch: x86_64 }
          - { os: ubuntu-22.04-arm, name: linux-arm64-glibc2.35,  family: linux,   arch: arm64 }
          - { os: macos-15-intel,   name: macos-x86_64-macos12,   family: macos,   arch: x86_64 }
          - { os: macos-15,         name: macos-arm64-macos12,    family: macos,   arch: arm64 }
          - { os: windows-2022,     name: windows-x86_64,         family: windows, arch: x86_64 }
    runs-on: ${{ matrix.platform.os }}
    steps:
      - name: Build
      - name: Run release save-contract tests
        shell: bash
        env:
          SDL_VIDEODRIVER: dummy
          SDL_AUDIODRIVER: dummy
        run: """ + RELEASE_SAVE_CONTRACT_COMMAND + """
      - name: Stage payload (linux)
      - name: Stage payload (macos)
      - name: Sign and verify macOS app bundles
        if: matrix.platform.family == 'macos'
        shell: bash
        run: |
          set -eux
          for app in JA2 JA2UB JA2MAPEDITOR; do
            bundle="dist/${app}_ENGLISH.app"
            codesign --force --deep --sign - "$bundle"
            codesign --verify --deep --strict --verbose=2 "$bundle"
          done
      - name: Stage payload (windows)
      - name: Archive (unix)
"""


class ReleaseSaveContractTests(unittest.TestCase):
    def test_complete_save_contract_is_accepted(self):
        validate_release_save_contract(VALID_WORKFLOW)

    def test_save_contract_step_removal_is_rejected(self):
        start = VALID_WORKFLOW.index(
            "      - name: Run release save-contract tests")
        end = VALID_WORKFLOW.index("      - name: Stage payload (linux)")
        workflow = VALID_WORKFLOW[:start] + VALID_WORKFLOW[end:]
        with self.assertRaisesRegex(RuntimeError, "save-contract step"):
            validate_release_save_contract(workflow)

    def test_save_contract_before_build_is_rejected(self):
        build = "      - name: Build\n"
        workflow = VALID_WORKFLOW.replace(build, "")
        staging = workflow.index("      - name: Stage payload (linux)")
        workflow = workflow[:staging] + build + workflow[staging:]
        with self.assertRaisesRegex(RuntimeError, "after Build"):
            validate_release_save_contract(workflow)

    def test_save_contract_after_staging_is_rejected(self):
        start = VALID_WORKFLOW.index(
            "      - name: Run release save-contract tests")
        end = VALID_WORKFLOW.index("      - name: Stage payload (linux)")
        step = VALID_WORKFLOW[start:end]
        workflow = VALID_WORKFLOW[:start] + VALID_WORKFLOW[end:]
        insertion = workflow.index("      - name: Stage payload (macos)")
        workflow = workflow[:insertion] + step + workflow[insertion:]
        with self.assertRaisesRegex(RuntimeError, "before staging"):
            validate_release_save_contract(workflow)

    def test_each_required_test_name_is_pinned(self):
        for target in ("engine_core", "save_serializer_golden", "ja2_headless"):
            with self.subTest(target=target):
                workflow = VALID_WORKFLOW.replace(target, "replacement_test")
                with self.assertRaisesRegex(RuntimeError, "must run exactly"):
                    validate_release_save_contract(workflow)

    def test_required_test_cannot_be_omitted(self):
        workflow = VALID_WORKFLOW.replace(
            " && " + RELEASE_SAVE_CONTRACT_INVOCATIONS[1], "")
        with self.assertRaisesRegex(RuntimeError, "must run exactly"):
            validate_release_save_contract(workflow)

    def test_test_selection_regex_must_remain_anchored(self):
        workflow = VALID_WORKFLOW.replace(
            "^engine_core$", "engine_core")
        with self.assertRaisesRegex(RuntimeError, "must run exactly"):
            validate_release_save_contract(workflow)

    def test_each_test_must_error_when_its_registration_is_missing(self):
        for invocation in RELEASE_SAVE_CONTRACT_INVOCATIONS:
            with self.subTest(invocation=invocation):
                workflow = VALID_WORKFLOW.replace(
                    invocation, invocation.replace("--no-tests=error ", ""))
                with self.assertRaisesRegex(RuntimeError, "must run exactly"):
                    validate_release_save_contract(workflow)

    def test_condition_cannot_disable_a_release_runner(self):
        workflow = VALID_WORKFLOW.replace(
            "        shell: bash\n        env:",
            "        if: matrix.platform.name == 'linux'\n"
            "        shell: bash\n        env:",
            1)
        with self.assertRaisesRegex(RuntimeError, "unconditionally"):
            validate_release_save_contract(workflow)

    def test_false_condition_cannot_disable_the_gate(self):
        workflow = VALID_WORKFLOW.replace(
            "        shell: bash\n        env:",
            "        if: false\n        shell: bash\n        env:",
            1)
        with self.assertRaisesRegex(RuntimeError, "unconditionally"):
            validate_release_save_contract(workflow)

    def test_quoted_false_condition_cannot_disable_the_gate(self):
        workflow = VALID_WORKFLOW.replace(
            "        shell: bash\n        env:",
            "        'if': false\n        shell: bash\n        env:",
            1)
        with self.assertRaisesRegex(RuntimeError, "unconditionally"):
            validate_release_save_contract(workflow)

    def test_job_condition_cannot_disable_all_release_runners(self):
        workflow = VALID_WORKFLOW.replace(
            "    name: package (${{ matrix.platform.name }})",
            "    name: package (${{ matrix.platform.name }})\n    if: false",
            1)
        with self.assertRaisesRegex(RuntimeError, "package job must run"):
            validate_release_save_contract(workflow)

    def test_commented_matrix_row_cannot_impersonate_a_runner(self):
        workflow = VALID_WORKFLOW.replace(
            "          - { os: windows-2022,     name: windows-x86_64,         family: windows, arch: x86_64 }",
            "          # - { os: windows-2022,     name: windows-x86_64,         family: windows, arch: x86_64 }",
            1)
        with self.assertRaisesRegex(RuntimeError, "five qualified"):
            validate_release_save_contract(workflow)

    def test_matrix_exclusion_cannot_remove_a_release_runner(self):
        workflow = VALID_WORKFLOW.replace(
            "          - { os: windows-2022,     name: windows-x86_64,         family: windows, arch: x86_64 }",
            "          - { os: windows-2022,     name: windows-x86_64,         family: windows, arch: x86_64 }\n"
            "        exclude:\n"
            "          - platform: { os: windows-2022, name: windows-x86_64, family: windows, arch: x86_64 }",
            1)
        with self.assertRaisesRegex(RuntimeError, "must not include or exclude"):
            validate_release_save_contract(workflow)

    def test_duplicate_matrix_row_cannot_add_a_release_runner(self):
        row = "          - { os: windows-2022,     name: windows-x86_64,         family: windows, arch: x86_64 }"
        workflow = VALID_WORKFLOW.replace(row, row + "\n" + row, 1)
        with self.assertRaisesRegex(RuntimeError, "exactly five"):
            validate_release_save_contract(workflow)

    def test_matrix_rows_in_an_unrelated_list_cannot_substitute_targets(self):
        workflow = VALID_WORKFLOW.replace(
            "        platform:", "        unrelated:", 1)
        with self.assertRaisesRegex(RuntimeError, "platform matrix"):
            validate_release_save_contract(workflow)

    def test_continue_on_error_cannot_disable_the_gate(self):
        workflow = VALID_WORKFLOW.replace(
            "        shell: bash\n        env:",
            "        continue-on-error: true\n"
            "        shell: bash\n        env:",
            1)
        with self.assertRaisesRegex(RuntimeError, "must block packaging"):
            validate_release_save_contract(workflow)

    def test_shell_environment_cannot_substitute_ctest(self):
        workflow = VALID_WORKFLOW.replace(
            "          SDL_AUDIODRIVER: dummy",
            "          SDL_AUDIODRIVER: dummy\n"
            "          BASH_ENV: tools/fake-ctest.sh",
            1)
        with self.assertRaisesRegex(RuntimeError, "only the two dummy"):
            validate_release_save_contract(workflow)

    def test_commented_step_cannot_impersonate_the_gate(self):
        start = VALID_WORKFLOW.index(
            "      - name: Run release save-contract tests")
        end = VALID_WORKFLOW.index("      - name: Stage payload (linux)")
        step = VALID_WORKFLOW[start:end]
        commented_step = "".join(
            "# " + line if line.strip() else line
            for line in step.splitlines(keepends=True))
        workflow = VALID_WORKFLOW[:start] + commented_step + VALID_WORKFLOW[end:]
        with self.assertRaisesRegex(RuntimeError, "save-contract step"):
            validate_release_save_contract(workflow)

    def test_echoed_command_cannot_impersonate_the_gate(self):
        workflow = VALID_WORKFLOW.replace(
            "        run: ctest ", "        run: echo ctest ", 1)
        with self.assertRaisesRegex(RuntimeError, "must run exactly"):
            validate_release_save_contract(workflow)

    def test_shell_dead_branch_cannot_hide_the_gate(self):
        workflow = VALID_WORKFLOW.replace(
            "        run: ctest ",
            "        run: if false; then ctest ",
            1)
        with self.assertRaisesRegex(RuntimeError, "must run exactly"):
            validate_release_save_contract(workflow)

    def test_target_name_in_comment_cannot_replace_command_target(self):
        workflow = VALID_WORKFLOW.replace(
            "^ja2_headless$", "^replacement_test$")
        workflow = workflow.replace(
            "        run: ctest ",
            "        # ja2_headless\n        run: ctest ", 1)
        with self.assertRaisesRegex(RuntimeError, "must run exactly"):
            validate_release_save_contract(workflow)


class ReleaseWorkflowSigningTests(unittest.TestCase):
    def test_complete_signing_contract_is_accepted(self):
        validate_release_workflow(VALID_WORKFLOW)

    def test_missing_strict_verification_is_rejected(self):
        workflow = VALID_WORKFLOW.replace(
            'codesign --verify --deep --strict --verbose=2 "$bundle"', "")
        with self.assertRaisesRegex(RuntimeError, "exact fail-closed"):
            validate_release_workflow(workflow)

    def test_signing_before_staging_is_rejected(self):
        signing_position = VALID_WORKFLOW.index(
            "      - name: Sign and verify macOS app bundles")
        windows_position = VALID_WORKFLOW.index(
            "      - name: Stage payload (windows)")
        signing_step = VALID_WORKFLOW[signing_position:windows_position]
        workflow = VALID_WORKFLOW.replace(signing_step, "")
        insertion = workflow.index("      - name: Stage payload (macos)")
        workflow = workflow[:insertion] + signing_step + workflow[insertion:]
        with self.assertRaisesRegex(RuntimeError, "after staging"):
            validate_release_workflow(workflow)

    def test_commented_signing_step_is_rejected(self):
        start = VALID_WORKFLOW.index(
            "      - name: Sign and verify macOS app bundles")
        end = VALID_WORKFLOW.index("      - name: Stage payload (windows)")
        step = VALID_WORKFLOW[start:end]
        commented = "".join(
            "# " + line if line.strip() else line
            for line in step.splitlines(keepends=True))
        workflow = VALID_WORKFLOW[:start] + commented + VALID_WORKFLOW[end:]
        with self.assertRaisesRegex(RuntimeError, "lost macOS staging"):
            validate_release_workflow(workflow)

    def test_signing_commands_cannot_be_hidden_in_a_dead_branch(self):
        workflow = VALID_WORKFLOW.replace(
            "          set -eux\n",
            "          set -eux\n          if false; then\n",
            1).replace(
                "          done\n      - name: Stage payload (windows)",
                "          done\n          fi\n"
                "      - name: Stage payload (windows)",
                1)
        with self.assertRaisesRegex(RuntimeError, "exact fail-closed"):
            validate_release_workflow(workflow)


class StaticAudioDependencyScannerTests(unittest.TestCase):
    def test_dynamic_codec_names_are_detected_cross_platform(self):
        image = (
            b"prefix\0libFLAC.8.dylib\0libopusfile.so.0\0"
            b"vorbisfile-3.dll\0libxmp-lite.4.dylib\0"
            b"wavpackdll.dll\0suffix")
        self.assertEqual(
            set(dynamic_audio_libraries(image)),
            {
                "libFLAC.8.dylib",
                "libopusfile.so.0",
                "vorbisfile-3.dll",
                "libxmp-lite.4.dylib",
                "wavpackdll.dll",
            })

    def test_system_and_static_library_names_are_not_rejected(self):
        self.assertEqual(
            dynamic_audio_libraries(
                b"/usr/lib/libSystem.B.dylib\0libFLAC.a\0libc++.1.dylib\0"
                b"libFLAC.dll.a\0libFLAC.so.debug\0doc-libogg.so.txt\0"
                b"doc-libogg.so\0foo.libogg.so"),
            ())

    def test_release_binary_inventory_is_platform_exact(self):
        build = Path("/release-build")
        self.assertEqual(
            release_binaries(build, "macos"),
            (
                build / "JA2_ENGLISH.app/Contents/MacOS/JA2_ENGLISH",
                build / "JA2UB_ENGLISH.app/Contents/MacOS/JA2UB_ENGLISH",
                build / "JA2MAPEDITOR_ENGLISH.app/Contents/MacOS/JA2MAPEDITOR_ENGLISH",
                build / "tests/ja2server",
            ))
        with self.assertRaisesRegex(ValueError, "unsupported"):
            release_binaries(build, "unknown")

    def test_static_cache_contract_rejects_one_shared_decoder(self):
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary) / "CMakeCache.txt"
            cache.write_text(
                "\n".join(REQUIRED_CACHE_ENTRIES) + "\n",
                encoding="utf-8")
            validate_cache(cache)
            cache.write_text(
                ("\n".join(REQUIRED_CACHE_ENTRIES) + "\n").replace(
                    "SDLMIXER_OPUS_SHARED:BOOL=OFF",
                    "SDLMIXER_OPUS_SHARED:BOOL=ON"),
                encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "OPUS_SHARED"):
                validate_cache(cache)
            cache.write_text(
                "\n".join(REQUIRED_CACHE_ENTRIES)
                + "\nSDLMIXER_OPUS_SHARED:BOOL=ON\n",
                encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "OPUS_SHARED"):
                validate_cache(cache)


class NativeReleasePackagingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        root = Path(__file__).resolve().parent.parent
        cls.workflow = (root / ".github/workflows/release.yml").read_text(
            encoding="utf-8")
        cls.installer = (root / "packaging/windows/ja2-sdl3.nsi").read_text(
            encoding="utf-8")
        cls.prepare = (root / "packaging/linux/prepare_appdir.sh").read_text(
            encoding="utf-8")
        cls.build = (root / "packaging/linux/build_appimage.sh").read_text(
            encoding="utf-8")
        cls.app_run = (root / "packaging/linux/AppRun").read_text(
            encoding="utf-8")

    def validate(self, workflow=None, installer=None):
        validate_release_packaging(
            self.workflow if workflow is None else workflow,
            self.installer if installer is None else installer,
            self.prepare,
            self.build,
            self.app_run)

    def mutate_nsis_step(self, original, replacement):
        start = self.workflow.index(
            "      - name: Download checksum-pinned NSIS compiler")
        end = self.workflow.index("\n      - name:", start + 1)
        step = self.workflow[start:end]
        self.assertIn(original, step)
        step = step.replace(original, replacement, 1)
        return self.workflow[:start] + step + self.workflow[end:]

    def mutate_qualification_step(self, step_name, original, replacement):
        start = self.workflow.index(f"      - name: {step_name}")
        end = self.workflow.index("\n      - name:", start + 1)
        step = self.workflow[start:end]
        self.assertIn(original, step)
        step = step.replace(original, replacement, 1)
        return self.workflow[:start] + step + self.workflow[end:]

    def test_complete_native_packaging_contract_is_accepted(self):
        self.validate()

    def test_moving_action_tag_is_rejected(self):
        workflow = self.workflow.replace(
            "actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683",
            "actions/checkout@v4")
        with self.assertRaisesRegex(RuntimeError, "immutable full commit SHA"):
            self.validate(workflow=workflow)

    def test_appimage_must_be_reproduced(self):
        workflow = self.workflow.replace(
            'cmp --silent "${PKG}.AppImage" "$second_image"', "")
        with self.assertRaisesRegex(RuntimeError, "reproducible AppImage"):
            self.validate(workflow=workflow)

    def test_release_label_must_be_bounded_and_path_safe(self):
        workflow = self.workflow.replace(
            '[[ ! "$RELEASE_LABEL" =~ ^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$ ]]',
            '[[ -z "$RELEASE_LABEL" ]]')
        with self.assertRaisesRegex(RuntimeError, "reproducible AppImage"):
            self.validate(workflow=workflow)

    def test_portable_linux_arm_zip_matrix_is_ratcheted(self):
        workflow = self.workflow.replace(
            "{ os: ubuntu-22.04-arm, name: linux-arm64-glibc2.35,  family: linux,   arch: arm64 }",
            "")
        with self.assertRaisesRegex(RuntimeError, "five-platform portable zip"):
            self.validate(workflow=workflow)

    def test_linux_release_runner_cannot_drift_to_latest(self):
        workflow = self.workflow.replace("ubuntu-22.04", "ubuntu-latest", 1)
        with self.assertRaisesRegex(RuntimeError, "portable zip contract"):
            self.validate(workflow=workflow)

    def test_macos_release_runner_cannot_drift_to_latest(self):
        workflow = self.workflow.replace("macos-15-intel", "macos-latest", 1)
        with self.assertRaisesRegex(RuntimeError, "portable zip contract"):
            self.validate(workflow=workflow)

    def test_macos_deployment_target_is_ratcheted(self):
        workflow = self.workflow.replace(
            "-DCMAKE_OSX_DEPLOYMENT_TARGET=12.0",
            "-DCMAKE_OSX_DEPLOYMENT_TARGET=26.0",
            1)
        with self.assertRaisesRegex(RuntimeError, "deployment contract"):
            self.validate(workflow=workflow)

    def test_release_forces_bundled_static_sdl_dependencies(self):
        workflow = self.workflow.replace(
            "-DCMAKE_DISABLE_FIND_PACKAGE_SDL3_mixer=ON",
            "-DCMAKE_DISABLE_FIND_PACKAGE_SDL3_mixer=OFF",
            1)
        with self.assertRaisesRegex(RuntimeError, "deployment contract"):
            self.validate(workflow=workflow)

    def test_release_forces_static_sdl_mixer_codecs(self):
        workflow = self.workflow.replace(
            "-DSDLMIXER_DEPS_SHARED=OFF",
            "-DSDLMIXER_DEPS_SHARED=ON",
            1)
        with self.assertRaisesRegex(RuntimeError, "deployment contract"):
            self.validate(workflow=workflow)

    def test_static_codec_image_scan_is_required(self):
        workflow = self.mutate_qualification_step(
            "Qualify static SDL dependency configuration",
            "        run: python3 tools/qualify_static_release_dependencies.py "
            "--build-dir build --platform-family "
            "'${{ matrix.platform.family }}'",
            "        run: true")
        with self.assertRaisesRegex(RuntimeError, "static SDL qualification"):
            self.validate(workflow=workflow)

    def test_linux_glibc_gate_cannot_be_relaxed(self):
        workflow = self.workflow.replace(
            "version > (2, 35)", "version > (2, 38)", 1)
        with self.assertRaisesRegex(RuntimeError, "Qualify Linux.*lost"):
            self.validate(workflow=workflow)

    def test_linux_shared_sdl_dependency_gate_is_required(self):
        workflow = self.mutate_qualification_step(
            "Qualify Linux release binaries",
            '            dynamic_table="$(readelf --dynamic "$binary")"',
            '            # dynamic_table="$(readelf --dynamic "$binary")"\n'
            '            dynamic_table=""')
        with self.assertRaisesRegex(RuntimeError, "fail-closed active inspection"):
            self.validate(workflow=workflow)

    def test_macos_binary_target_gate_cannot_be_relaxed(self):
        workflow = self.workflow.replace(
            'if [[ "$minos" != 12.0 ]]',
            'if [[ "$minos" != 26.0 ]]',
            1)
        with self.assertRaisesRegex(RuntimeError, "Qualify macOS.*lost"):
            self.validate(workflow=workflow)

    def test_macos_shared_sdl_dependency_gate_is_required(self):
        workflow = self.mutate_qualification_step(
            "Qualify macOS release binaries",
            '            dependency_table="$(otool -L "$binary")"',
            '            # dependency_table="$(otool -L "$binary")"\n'
            '            dependency_table=""')
        with self.assertRaisesRegex(RuntimeError, "fail-closed active inspection"):
            self.validate(workflow=workflow)

    def test_macos_non_system_dependency_gate_is_required(self):
        workflow = self.mutate_qualification_step(
            "Qualify macOS release binaries",
            '            unexpected_dependencies="$(',
            '            # unexpected_dependencies="$(')
        with self.assertRaisesRegex(RuntimeError, "fail-closed active inspection"):
            self.validate(workflow=workflow)

    def test_macos_runtime_search_path_gate_is_required(self):
        workflow = self.mutate_qualification_step(
            "Qualify macOS release binaries",
            '            rpaths="$(otool -l "$binary" | awk \'',
            '            # rpaths="$(otool -l "$binary" | awk \'')
        with self.assertRaisesRegex(RuntimeError, "fail-closed active inspection"):
            self.validate(workflow=workflow)

    def test_macos_shared_sdl_pattern_covers_versioned_dylibs_and_frameworks(self):
        dependencies = (
            "@rpath/libSDL3.0.dylib",
            "/opt/local/lib/libSDL3_mixer.2.1.dylib",
            "/Library/Frameworks/SDL3.framework/Versions/A/SDL3",
            "@rpath/SDL3_net.framework/Versions/A/SDL3_net",
        )
        for dependency in dependencies:
            with self.subTest(dependency=dependency):
                self.assertIsNotNone(re.search(
                    MACOS_SHARED_SDL_PATTERN, dependency, re.IGNORECASE))
        self.assertIsNone(re.search(
            MACOS_SHARED_SDL_PATTERN,
            "/usr/lib/libSystem.B.dylib",
            re.IGNORECASE))

    def test_windows_architecture_gate_cannot_be_relaxed(self):
        workflow = self.workflow.replace(
            "machine != 0x8664", "machine != 0xaa64", 1)
        with self.assertRaisesRegex(RuntimeError, "Qualify Windows.*lost"):
            self.validate(workflow=workflow)

    def test_windows_shared_sdl_dependency_gate_is_required(self):
        workflow = self.mutate_qualification_step(
            "Qualify Windows release binaries",
            '            import_table="$(llvm-readobj --coff-imports "$binary")"',
            '            # import_table="$(llvm-readobj --coff-imports "$binary")"\n'
            '            import_table=""')
        with self.assertRaisesRegex(RuntimeError, "fail-closed active inspection"):
            self.validate(workflow=workflow)

    def test_qualification_condition_cannot_hide_as_a_shell_comment(self):
        workflow = self.mutate_qualification_step(
            "Qualify Linux release binaries",
            "        if: matrix.platform.family == 'linux'\n"
            "        shell: bash\n"
            "        run: |",
            "        if: false\n"
            "        shell: bash\n"
            "        run: |\n"
            "          # if: matrix.platform.family == 'linux'")
        with self.assertRaisesRegex(RuntimeError, "exact platform condition"):
            self.validate(workflow=workflow)

    def test_game_package_filename_must_include_matrix_platform(self):
        workflow = self.workflow.replace(
            'echo "PKG=ja2-sdl3-${{ matrix.platform.name }}-${RELEASE_LABEL}"',
            'echo "PKG=ja2-sdl3-generic-${RELEASE_LABEL}"',
            1)
        with self.assertRaisesRegex(RuntimeError, "package names must be assigned"):
            self.validate(workflow=workflow)

    def test_sdk_package_filename_must_include_matrix_platform(self):
        workflow = self.workflow.replace(
            'echo "SDK_PKG=ja2-engine-sdk-${{ matrix.platform.name }}-${RELEASE_LABEL}"',
            'echo "SDK_PKG=ja2-engine-sdk-generic-${RELEASE_LABEL}"',
            1)
        with self.assertRaisesRegex(RuntimeError, "package names must be assigned"):
            self.validate(workflow=workflow)

    def test_release_qualification_cannot_run_after_staging(self):
        start = self.workflow.index(
            "      - name: Qualify Linux release binaries")
        end = self.workflow.index(
            "      - name: Qualify macOS release binaries", start)
        step = self.workflow[start:end]
        workflow = self.workflow[:start] + self.workflow[end:]
        insertion = workflow.index("      - name: Stage payload (macos)")
        workflow = workflow[:insertion] + step + workflow[insertion:]
        with self.assertRaisesRegex(RuntimeError, "after Build and before staging"):
            self.validate(workflow=workflow)

    def test_nsis_download_cannot_revert_to_invoke_web_request(self):
        workflow = self.mutate_nsis_step(
            "& curl.exe --proto '=https'", "Invoke-WebRequest -Uri $url")
        with self.assertRaisesRegex(RuntimeError, "Invoke-WebRequest"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_remain_https_only(self):
        workflow = self.mutate_nsis_step(
            "--proto '=https'", "--proto '=http,https'")
        with self.assertRaisesRegex(RuntimeError, "curl.exe transport"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_require_tls_1_2(self):
        workflow = self.mutate_nsis_step("--tlsv1.2", "--tlsv1.0")
        with self.assertRaisesRegex(RuntimeError, "curl.exe transport"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_follow_redirects(self):
        workflow = self.mutate_nsis_step("--fail --location", "--fail")
        with self.assertRaisesRegex(RuntimeError, "curl.exe transport"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_fail_on_http_errors(self):
        workflow = self.mutate_nsis_step("--fail --location", "--location")
        with self.assertRaisesRegex(RuntimeError, "curl.exe transport"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_retry_all_errors(self):
        workflow = self.mutate_nsis_step("--retry-all-errors ", "")
        with self.assertRaisesRegex(RuntimeError, "curl.exe transport"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_write_the_validated_archive(self):
        workflow = self.mutate_nsis_step(
            "--output $archive $url", "--remote-name $url")
        with self.assertRaisesRegex(RuntimeError, "curl.exe transport"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_hard_fail_on_curl_exit(self):
        workflow = self.mutate_nsis_step(
            "if ($LASTEXITCODE -ne 0)", "if ($LASTEXITCODE -eq 0)")
        with self.assertRaisesRegex(RuntimeError, "curl.exe transport"):
            self.validate(workflow=workflow)

    def test_nsis_download_sha256_pin_cannot_change(self):
        workflow = self.mutate_nsis_step(
            "56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f",
            "0" * 64)
        with self.assertRaisesRegex(RuntimeError, "SHA-256 verification"):
            self.validate(workflow=workflow)

    def test_nsis_download_must_use_sha256(self):
        workflow = self.mutate_nsis_step(
            "Get-FileHash -Algorithm SHA256",
            "Get-FileHash -Algorithm SHA1")
        with self.assertRaisesRegex(RuntimeError, "SHA-256 verification"):
            self.validate(workflow=workflow)

    def test_nsis_download_hash_mismatch_must_fail(self):
        workflow = self.mutate_nsis_step(
            "if ($actual -ne $expected)", "if ($actual -eq $expected)")
        with self.assertRaisesRegex(RuntimeError, "SHA-256 verification"):
            self.validate(workflow=workflow)

    def test_nsis_archive_cannot_be_expanded_before_hash_verification(self):
        expand = (
            "          Expand-Archive -LiteralPath $archive "
            "-DestinationPath $tools\n")
        workflow = self.mutate_nsis_step(expand, "")
        workflow = workflow.replace(
            "          $actual = (Get-FileHash -Algorithm SHA256 $archive)",
            expand
            + "          $actual = (Get-FileHash -Algorithm SHA256 $archive)",
            1)
        with self.assertRaisesRegex(RuntimeError, "then extraction"):
            self.validate(workflow=workflow)

    def test_nsis_sourceforge_url_must_be_the_active_assignment(self):
        canonical = (
            "https://downloads.sourceforge.net/project/nsis/"
            "NSIS%203/3.12/nsis-3.12.zip")
        workflow = self.mutate_nsis_step(
            f"$url = '{canonical}'",
            "$url = 'https://example.invalid/nsis-3.12.zip'\n"
            f"          # expected source: {canonical}")
        with self.assertRaisesRegex(RuntimeError, "SourceForge URL assignment"):
            self.validate(workflow=workflow)

    def test_nsis_download_cannot_continue_on_error(self):
        workflow = self.mutate_nsis_step(
            "        shell: pwsh",
            "        continue-on-error: true\n        shell: pwsh")
        with self.assertRaisesRegex(RuntimeError, "must block packaging"):
            self.validate(workflow=workflow)

    def test_nsis_curl_block_cannot_be_wrapped_in_false_control_flow(self):
        curl = "          & curl.exe --proto '=https'"
        workflow = self.mutate_nsis_step(
            curl,
            "          if ($false) {\n" + curl)
        workflow = workflow.replace(
            "          $actual = (Get-FileHash -Algorithm SHA256 $archive)",
            "          }\n"
            "          $actual = (Get-FileHash -Algorithm SHA256 $archive)",
            1)
        with self.assertRaisesRegex(RuntimeError, "unapproved controls"):
            self.validate(workflow=workflow)

    def test_nsis_download_cannot_add_an_extra_statement(self):
        workflow = self.mutate_nsis_step(
            "          & curl.exe --proto '=https'",
            "          Write-Output 'skip gate'\n"
            "          & curl.exe --proto '=https'")
        with self.assertRaisesRegex(RuntimeError, "unapproved controls"):
            self.validate(workflow=workflow)

    def test_nsis_download_cannot_add_a_step_condition(self):
        workflow = self.mutate_nsis_step(
            "        shell: pwsh",
            "        'if': false\n        shell: pwsh")
        with self.assertRaisesRegex(RuntimeError, "only its Windows"):
            self.validate(workflow=workflow)

    def test_recursive_uninstall_is_rejected(self):
        installer = self.installer.replace(
            'RMDir "$INSTDIR"', 'RMDir /r "$INSTDIR"')
        with self.assertRaisesRegex(RuntimeError, "recursively remove"):
            self.validate(installer=installer)

    def test_owned_server_sample_cannot_become_user_config(self):
        installer = self.installer.replace(
            "File /oname=ja2_mp.ini.sample",
            "File /oname=ja2_mp.ini")
        with self.assertRaisesRegex(RuntimeError, "user-owned server config"):
            self.validate(installer=installer)

    def test_user_server_config_cannot_be_deleted(self):
        installer = self.installer.replace(
            'Delete "$INSTDIR\\ja2server\\ja2_mp.ini.sample"',
            'Delete "$INSTDIR\\ja2server\\ja2_mp.ini"')
        with self.assertRaisesRegex(RuntimeError, "user-owned server config"):
            self.validate(installer=installer)

    def test_generic_uninstaller_name_cannot_be_claimed(self):
        installer = self.installer.replace(
            'WriteUninstaller "$INSTDIR\\Uninstall-JA2-SDL3.exe"',
            'WriteUninstaller "$INSTDIR\\Uninstall.exe"')
        with self.assertRaisesRegex(RuntimeError, "generic uninstaller name"):
            self.validate(installer=installer)


class AppImageBuildScriptTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(__file__).resolve().parent.parent
        cls.build_script = cls.root / "packaging/linux/build_appimage.sh"

    def setUp(self):
        temporary_directory = tempfile.TemporaryDirectory()
        self.addCleanup(temporary_directory.cleanup)
        self.workdir = Path(temporary_directory.name)

        self.appdir = self.workdir / "AppDir"
        self.appdir.mkdir()
        for required in (
                "AppRun",
                "org.ja2v113.JA2SDL3.desktop",
                "org.ja2v113.JA2SDL3.png"):
            (self.appdir / required).write_text("fixture\n", encoding="utf-8")

        self.runtime = self.workdir / "runtime"
        self.runtime.write_bytes(b"runtime fixture\n")
        self.appimagetool = self.workdir / "fake-appimagetool"
        self.appimagetool.write_text(
            """#!/usr/bin/env bash
set -euo pipefail
output=${!#}
cat > "$output" <<'APPIMAGE'
#!/usr/bin/env bash
set -euo pipefail
[[ ${1-} == --appimage-offset ]]
printf '4096\\n'
APPIMAGE
""",
            encoding="utf-8")
        self.appimagetool.chmod(0o755)

    def build(self, output: str) -> subprocess.CompletedProcess[str]:
        environment = dict(os.environ)
        environment["PATH"] = "/usr/bin:/bin"
        return subprocess.run(
            [
                self.build_script,
                self.appdir,
                output,
                "x86_64",
                self.appimagetool,
                self.runtime,
                "1",
            ],
            cwd=self.workdir,
            env=environment,
            capture_output=True,
            text=True,
            check=False)

    def test_generated_image_is_executed_by_path(self):
        outputs = (
            ("relative", "release.AppImage"),
            ("absolute", str(self.workdir / "absolute.AppImage")),
        )
        for label, output in outputs:
            with self.subTest(label=label):
                result = self.build(output)
                self.assertEqual(
                    0,
                    result.returncode,
                    result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
