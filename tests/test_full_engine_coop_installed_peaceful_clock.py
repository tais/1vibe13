"""Peaceful-clock probe helpers."""
import unittest

import full_engine_coop_installed_peaceful_clock as peaceful_clock


class PeacefulClockProbeTests(unittest.TestCase):
    def test_both_clients_must_show_the_same_time(self):
        replies = [{"world_seconds": 111610}, {"world_seconds": 111611}]
        self.assertEqual(peaceful_clock.shared_seconds(replies), 111610)
        self.assertIsNone(peaceful_clock.shared_seconds([{"world_seconds": 100}, {"world_seconds": 110}]))
        self.assertIsNone(peaceful_clock.shared_seconds(None))


if __name__ == "__main__":
    unittest.main()
