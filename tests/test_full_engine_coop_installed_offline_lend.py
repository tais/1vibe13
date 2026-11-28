"""Offline-lend probe helpers."""
import unittest

import full_engine_coop_installed_offline_lend as offline_lend


class OfflineLendProbeTests(unittest.TestCase):
    BEFORE = {(0, 1): 1, (1, 2): 0, (2, 3): 1, (3, 4): 0}

    def test_the_original_split_is_accepted(self):
        self.assertIsNone(offline_lend.same_split(self.BEFORE, dict(self.BEFORE)))

    def test_a_kept_lend_or_a_missing_merc_is_reported(self):
        self.assertIn("moved", offline_lend.same_split(self.BEFORE, {actor: 0 for actor in self.BEFORE}))
        partial = dict(self.BEFORE)
        del partial[(0, 1)]
        self.assertIn("moved", offline_lend.same_split(self.BEFORE, partial))


if __name__ == "__main__":
    unittest.main()
