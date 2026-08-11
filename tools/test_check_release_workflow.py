#!/usr/bin/env python3
"""Tests for the tagged-release signing and native-package ratchets."""

from pathlib import Path
import unittest

from tools.check_release_workflow import (
    validate_release_packaging,
    validate_release_workflow,
)


VALID_WORKFLOW = """
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


if __name__ == "__main__":
    unittest.main()
