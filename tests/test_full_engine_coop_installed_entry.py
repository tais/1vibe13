"""Build-free option checks for bounded installed native-entry qualification."""
import contextlib
import io
import unittest

from full_engine_coop_installed_entry import SCENARIOS, parse_arguments


class InstalledEntryOptionsTests(unittest.TestCase):
    def arguments(self, *extra):
        return parse_arguments(["--executable", "/fixture/server",
                                "--installed-data-root", "/fixture/data", *extra])

    def test_all_scenarios_by_default_and_explicit_single_sanitizer_case(self):
        self.assertIsNone(self.arguments().scenario)
        for scenario in SCENARIOS:
            self.assertEqual(self.arguments("--scenario", scenario).scenario, scenario)
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.arguments("--scenario", "observe")

    def test_explicit_paths_and_finite_bounded_deadlines(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            parse_arguments([])
        self.assertEqual(self.arguments().timeout_seconds, 180)
        for value in ("1", "1.25", "600"):
            self.assertEqual(self.arguments(f"--timeout-seconds={value}").timeout_seconds, float(value))
        for value in ("0", "-1", "601", "nan", "inf", "-inf"):
            with self.subTest(value=value), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.arguments(f"--timeout-seconds={value}")


if __name__ == "__main__":
    unittest.main()
