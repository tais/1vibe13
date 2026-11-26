"""Reinforcement probe helpers."""
import unittest

import full_engine_coop_installed_reinforcements as reinforcements


LINE = ("[dedicated] militia reinforcements called: id=3; sector=9,2; militia_before=0; militia_after=6; prepared=1")


class ReinforcementProbeTests(unittest.TestCase):
    def test_parses_the_server_answer(self):
        self.assertEqual(reinforcements.parse_answer("x\n" + LINE + "\n"),
                         {"called": True, "decision": 3, "x": 9, "y": 2, "militia_before": 0, "militia_after": 6,
                          "prepared": True})
        self.assertIsNone(reinforcements.parse_answer("[dedicated] nothing"))

    def test_each_answer_has_its_native_effect(self):
        called = reinforcements.parse_answer(LINE)
        reinforcements.check_answer(called, True)
        with self.assertRaises(reinforcements.SmokeFailure):
            reinforcements.check_answer(called, False)
        declined = dict(called, called=False, militia_after=0)
        reinforcements.check_answer(declined, False)
        with self.assertRaises(reinforcements.SmokeFailure):
            reinforcements.check_answer(dict(called, militia_after=0), True)
        with self.assertRaises(reinforcements.SmokeFailure):
            reinforcements.check_answer(dict(called, prepared=False), True)


if __name__ == "__main__":
    unittest.main()
