"""Climb probe helpers."""
import unittest

import full_engine_coop_installed_climb as climb


class ClimbProbeTests(unittest.TestCase):
    def test_climb_site_is_read_for_the_right_merc(self):
        text = ("[co-op entry test] climb site: actor=1:2; grid=6300\n"
                "[co-op entry test] climb site: actor=0:1; grid=6301\n")
        self.assertEqual(climb.climb_site(text, (0, 1)), 6301)
        self.assertEqual(climb.climb_site(text, (1, 2)), 6300)
        self.assertIsNone(climb.climb_site(text, (2, 3)))
        self.assertIsNone(climb.climb_site("[co-op entry test] climb site refused: actor=0:1", (0, 1)))

    def test_shared_place_needs_both_clients_to_agree(self):
        state = lambda grid, level: {"actors": [{"slot": 1, "inc": 2, "grid": grid, "level": level, "pose": True}]}
        self.assertEqual(climb.shared_place([state(6460, 1), state(6460, 1)], (1, 2)), (6460, 1))
        self.assertIsNone(climb.shared_place([state(6460, 1), state(6300, 0)], (1, 2)))


if __name__ == "__main__":
    unittest.main()
