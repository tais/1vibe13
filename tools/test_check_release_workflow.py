#!/usr/bin/env python3
"""Tests for the tagged-release workflow signing ratchet."""

import unittest

from tools.check_release_workflow import validate_release_workflow


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


if __name__ == "__main__":
    unittest.main()
