"""Multi-leg travel probe helpers."""
import unittest

import full_engine_coop_installed_multi_leg_travel as travel


class MultiLegTravelProbeTests(unittest.TestCase):
    def test_route_visits_collapse_repeats(self):
        samples = [(9, 1, True), (9, 1, True), (9, 2, True), (9, 3, False), (9, 3, False)]
        self.assertEqual(travel.route_visits(samples), [[9, 1], [9, 2], [9, 3]])


if __name__ == "__main__":
    unittest.main()
