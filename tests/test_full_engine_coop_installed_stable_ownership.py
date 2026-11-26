"""Stable-ownership probe helpers."""
import unittest

import full_engine_coop_installed_stable_ownership as ownership


class StableOwnershipProbeTests(unittest.TestCase):
    def test_ownership_map_and_double_claims(self):
        states = [{"assigned": [[1, 2], [3, 4]]}, {"assigned": [[0, 1], [2, 3]]}]
        self.assertEqual(ownership.ownership(states), {(1, 2): 0, (3, 4): 0, (0, 1): 1, (2, 3): 1})
        self.assertIsNone(ownership.ownership([{"assigned": [[1, 2]]}, {"assigned": [[1, 2]]}]))

    def test_changed_owners_ignores_the_removed_merc(self):
        before = {(0, 1): 1, (1, 2): 0, (2, 3): 1, (3, 4): 0}
        stable = {(0, 1): 1, (2, 3): 1, (3, 4): 0}
        redealt = {(0, 1): 1, (2, 3): 0, (3, 4): 1}
        self.assertEqual(ownership.changed_owners(before, stable, (1, 2)), [])
        self.assertEqual(ownership.changed_owners(before, redealt, (1, 2)), [(2, 3), (3, 4)])


if __name__ == "__main__":
    unittest.main()
