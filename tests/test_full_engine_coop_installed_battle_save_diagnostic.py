"""Battle save diagnostic report parsing."""
import unittest

import full_engine_coop_installed_battle_save_diagnostic as diagnostic


class BattleSaveDiagnosticTests(unittest.TestCase):
    def test_report(self):
        text = "\n".join([
            "[co-op entry test] battle save diagnostic: saved=1 loaded=1",
            "[co-op entry test] battle save diagnostic save: 0 differing lines",
            "[co-op entry test] battle save diagnostic load: - actor 3 life=40 | + actor 3 life=39",
            "[co-op entry test] battle save diagnostic load: 1 differing lines"])
        report = diagnostic.parse_report(text)
        self.assertTrue(report["saved"] and report["loaded"])
        self.assertEqual(report["counts"], {"save": 0, "load": 1})
        self.assertEqual(report["differences"], [{"phase": "load", "before": "actor 3 life=40", "after": "actor 3 life=39"}])

    def test_refused_save(self):
        report = diagnostic.parse_report("[co-op entry test] battle save diagnostic: saved=0 loaded=0")
        self.assertFalse(report["saved"] or report["loaded"])


if __name__ == "__main__":
    unittest.main()
