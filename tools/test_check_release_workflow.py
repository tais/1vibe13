#!/usr/bin/env python3
"""Tests for the tagged-release signing and native-package ratchets."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.check_release_workflow import (
    RELEASE_SAVE_CONTRACT_COMMAND,
    RELEASE_SAVE_CONTRACT_INVOCATIONS,
    validate_release_packaging,
    validate_release_save_contract,
    validate_release_workflow,
)


VALID_WORKFLOW = """
  package:
    name: package (${{ matrix.platform.name }})
    strategy:
      matrix:
        platform:
          - { os: ubuntu-latest,    name: linux,       family: linux }
          - { os: ubuntu-24.04-arm, name: linux-arm64, family: linux }
          - { os: macos-latest,     name: macos,       family: macos }
          - { os: windows-latest,   name: windows,     family: windows }
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
        if: matrix.platform.name == 'macos'
        run: |
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
            "          - { os: windows-latest,   name: windows,     family: windows }",
            "          # - { os: windows-latest,   name: windows,     family: windows }",
            1)
        with self.assertRaisesRegex(RuntimeError, "four active"):
            validate_release_save_contract(workflow)

    def test_matrix_exclusion_cannot_remove_a_release_runner(self):
        workflow = VALID_WORKFLOW.replace(
            "          - { os: windows-latest,   name: windows,     family: windows }",
            "          - { os: windows-latest,   name: windows,     family: windows }\n"
            "        exclude:\n"
            "          - platform: { os: windows-latest, name: windows, family: windows }",
            1)
        with self.assertRaisesRegex(RuntimeError, "must not include or exclude"):
            validate_release_save_contract(workflow)

    def test_duplicate_matrix_row_cannot_add_a_release_runner(self):
        row = "          - { os: windows-latest,   name: windows,     family: windows }"
        workflow = VALID_WORKFLOW.replace(row, row + "\n" + row, 1)
        with self.assertRaisesRegex(RuntimeError, "exactly four"):
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
        with self.assertRaisesRegex(RuntimeError, "codesign --verify"):
            validate_release_workflow(workflow)

    def test_signing_before_staging_is_rejected(self):
        signing_position = VALID_WORKFLOW.index(
            "      - name: Sign and verify macOS app bundles")
        windows_position = VALID_WORKFLOW.index(
            "      - name: Stage payload (windows)")
        signing_step = VALID_WORKFLOW[signing_position:windows_position]
        workflow = VALID_WORKFLOW.replace(signing_step, "")
        workflow = signing_step + workflow
        with self.assertRaisesRegex(RuntimeError, "after staging"):
            validate_release_workflow(workflow)


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

    def test_legacy_linux_arm_zip_matrix_is_ratcheted(self):
        workflow = self.workflow.replace(
            "{ os: ubuntu-24.04-arm, name: linux-arm64, family: linux }",
            "")
        with self.assertRaisesRegex(RuntimeError, "four-zip contract"):
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
