"""Melee probe helpers."""
import unittest

import full_engine_coop_installed_melee as melee


class MeleeProbeTests(unittest.TestCase):
    LOG = ("[co-op entry test] melee placed: actor=1:2; target=263:10; grid=6451; target-grid=6611; hand=37; "
           "action-grid=6451; sees=1\n")

    def test_placement_is_read_for_the_right_merc(self):
        placed = melee.placement(self.LOG, (1, 2))
        self.assertEqual(placed["target"], (263, 10))
        self.assertEqual((placed["grid"], placed["action_grid"], placed["hand"], placed["sees"]), (6451, 6451, 37, True))
        self.assertIsNone(melee.placement(self.LOG, (0, 1)))
        self.assertIsNone(melee.placement("[co-op entry test] melee placement refused: actor=1:2; x", (1, 2)))

    def test_target_life_reports_a_killed_enemy_as_gone(self):
        alive = {"actors": [{"slot": 263, "inc": 10, "life": 29, "pose": True}]}
        self.assertEqual(melee.target_life([alive, alive], (263, 10)), 29)
        self.assertEqual(melee.target_life([{"actors": []}, {"actors": []}], (263, 10)), "gone")
        self.assertIsNone(melee.target_life([alive, {"actors": []}], (263, 10)))


if __name__ == "__main__":
    unittest.main()
