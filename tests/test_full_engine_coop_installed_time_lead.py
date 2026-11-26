"""Time-lead probe helpers."""
import unittest

import full_engine_coop_installed_time_lead as time_lead


class TimeLeadProbeTests(unittest.TestCase):
    def test_leader_index_requires_exactly_one_leader(self):
        self.assertEqual(time_lead.leader_index([{"local_leader": False}, {"local_leader": True}]), 1)
        self.assertIsNone(time_lead.leader_index([{"local_leader": True}, {"local_leader": True}]))
        self.assertIsNone(time_lead.leader_index([{"local_leader": False}, {"local_leader": False}]))


if __name__ == "__main__":
    unittest.main()
