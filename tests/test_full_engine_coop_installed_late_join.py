"""Late-join probe helpers."""
import unittest

import full_engine_coop_installed_late_join as late_join


class LateJoinProbeTests(unittest.TestCase):
    ALONE = {(1, 1): 0, (2, 1): 0, (3, 1): 0, (4, 1): 0}

    def test_the_last_two_move_to_the_joiner(self):
        shared = {(1, 1): 0, (2, 1): 0, (3, 1): 1, (4, 1): 1}
        self.assertIsNone(late_join.late_share(self.ALONE, shared))

    def test_a_joiner_with_nothing_or_the_wrong_mercs_fails(self):
        self.assertIn("expected", late_join.late_share(self.ALONE, self.ALONE))
        swapped = {(1, 1): 1, (2, 1): 1, (3, 1): 0, (4, 1): 0}
        self.assertIn("expected", late_join.late_share(self.ALONE, swapped))

    def test_the_first_player_must_have_held_all_four(self):
        split = {(1, 1): 0, (2, 1): 1, (3, 1): 0, (4, 1): 1}
        self.assertIn("did not hold all four", late_join.late_share(split, split))


if __name__ == "__main__":
    unittest.main()
