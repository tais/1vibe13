#!/usr/bin/env python3
"""Tests for the ASan CTest executable-coverage validator."""

import unittest

from tools.check_asan_ctest_coverage import (
    compiled_ctest_targets,
    validate_asan_ctest_coverage,
)


CMAKE = r'''
add_executable(alpha_tests alpha.cpp)
add_test(NAME alpha COMMAND alpha_tests)
add_test(NAME alpha_mode COMMAND alpha_tests --mode)

add_executable(beta_tests beta.cpp)
add_test(NAME beta COMMAND beta_tests)

add_executable(dependency_tests dependency.cpp)
add_test(NAME dependency COMMAND dependency_tests)
add_dependencies(alpha_tests dependency_tests)

add_executable(compile_only_target compile_only.cpp)

add_test(NAME configure_contract
  COMMAND ${CMAKE_COMMAND} -P check_contract.cmake)
add_test(NAME python_lint
  COMMAND ${Python3_EXECUTABLE} lint_source.py)

function(add_variant_test target mode)
  add_executable(${target} variant.cpp)
  add_test(NAME ${target} COMMAND ${target} --mode ${mode})
endfunction()
add_variant_test(generated_tests strict)
'''


WORKFLOW = r'''
jobs:
  build:
    steps:
      - name: A normal build cannot substitute for ASan
        run: cmake --build build --target beta_tests

  sanitizer:
    name: Linux ASan
    steps:
      - name: Configure
        run: cmake -S . -B build -DADDRESS_SANITIZER=ON
      - name: Build primary test cohort
        run: cmake --build build --target alpha_tests compile_only_target
      - name: Build another intentional cohort
        run: cmake --build build --target beta_tests generated_tests
      - name: Run tests
        run: |
          export EXAMPLE=1
          ctest --test-dir build --output-on-failure
'''


class AsanCtestCoverageTests(unittest.TestCase):
    def test_compiled_commands_exclude_script_and_configure_tests(self):
        self.assertEqual(
            compiled_ctest_targets([CMAKE]),
            {"alpha_tests", "beta_tests", "dependency_tests",
             "generated_tests"})

    def test_split_cohorts_transitive_dependency_and_extra_target_are_valid(self):
        required, explicit = validate_asan_ctest_coverage(WORKFLOW, [CMAKE])
        self.assertEqual(len(required), 4)
        self.assertIn("compile_only_target", explicit)
        self.assertNotIn("dependency_tests", explicit)

    def test_omitted_executable_is_rejected(self):
        workflow = WORKFLOW.replace("beta_tests generated_tests", "generated_tests")
        with self.assertRaisesRegex(RuntimeError, "beta_tests"):
            validate_asan_ctest_coverage(workflow, [CMAKE])

    def test_generated_executable_is_rejected_when_omitted(self):
        workflow = WORKFLOW.replace("beta_tests generated_tests", "beta_tests")
        with self.assertRaisesRegex(RuntimeError, "generated_tests"):
            validate_asan_ctest_coverage(workflow, [CMAKE])

    def test_transitive_test_dependency_is_rejected_when_edge_is_removed(self):
        cmake = CMAKE.replace("add_dependencies(alpha_tests dependency_tests)\n", "")
        with self.assertRaisesRegex(RuntimeError, "dependency_tests"):
            validate_asan_ctest_coverage(WORKFLOW, [cmake])

    def test_commented_build_command_cannot_supply_a_target(self):
        workflow = WORKFLOW.replace(
            "run: cmake --build build --target beta_tests generated_tests",
            "run: |\n"
            "          # cmake --build build --target beta_tests\n"
            "          cmake --build build --target generated_tests")
        with self.assertRaisesRegex(RuntimeError, "beta_tests"):
            validate_asan_ctest_coverage(workflow, [CMAKE])

    def test_conditional_build_step_cannot_supply_a_target(self):
        workflow = WORKFLOW.replace(
            "      - name: Build another intentional cohort\n",
            "      - name: Build another intentional cohort\n"
            "        if: false\n")
        with self.assertRaisesRegex(RuntimeError, "beta_tests"):
            validate_asan_ctest_coverage(workflow, [CMAKE])

    def test_normal_build_job_cannot_supply_a_missing_target(self):
        workflow = WORKFLOW.replace("beta_tests generated_tests", "generated_tests")
        with self.assertRaisesRegex(RuntimeError, "beta_tests"):
            validate_asan_ctest_coverage(workflow, [CMAKE])

    def test_missing_asan_configuration_is_rejected(self):
        workflow = WORKFLOW.replace("-DADDRESS_SANITIZER=ON", "")
        with self.assertRaisesRegex(RuntimeError, "configure"):
            validate_asan_ctest_coverage(workflow, [CMAKE])

    def test_commented_asan_configuration_cannot_substitute_for_flag(self):
        workflow = WORKFLOW.replace(
            "run: cmake -S . -B build -DADDRESS_SANITIZER=ON",
            "run: |\n"
            "          # -DADDRESS_SANITIZER=ON\n"
            "          cmake -S . -B build")
        with self.assertRaisesRegex(RuntimeError, "configure"):
            validate_asan_ctest_coverage(workflow, [CMAKE])

    def test_missing_ctest_execution_is_rejected(self):
        workflow = WORKFLOW.replace(
            "          ctest --test-dir build --output-on-failure\n", "")
        with self.assertRaisesRegex(RuntimeError, "run the configured CTest"):
            validate_asan_ctest_coverage(workflow, [CMAKE])


if __name__ == "__main__":
    unittest.main()
