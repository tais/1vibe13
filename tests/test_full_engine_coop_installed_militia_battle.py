"""Militia battle probe helpers."""
import unittest

import full_engine_coop_installed_militia_battle as militia


LINE = ("[dedicated] militia battle autoresolved: sector=9,2; outcome=held; militia=12; militia_lost=0; "
        "enemies=10; enemies_killed=10; battle_seconds=13")


class MilitiaBattleProbeTests(unittest.TestCase):
    def test_parses_the_server_summary(self):
        self.assertEqual(militia.parse_resolution("noise\n" + LINE + "\n"),
                         {"x": 9, "y": 2, "held": True, "militia": 12, "militia_lost": 0, "enemies": 10,
                          "enemies_killed": 10, "battle_seconds": 13})
        self.assertIsNone(militia.parse_resolution("[dedicated] something else"))

    def test_expected_notice_matches_the_outcome(self):
        held = militia.parse_resolution(LINE)
        self.assertEqual(militia.expected_notice(held)["outcome"], militia.MILITIA_HELD)
        lost = dict(held, held=False, militia_lost=300)
        notice = militia.expected_notice(lost)
        self.assertEqual((notice["outcome"], notice["militia_lost"], notice["z"]), (militia.MILITIA_DEFEATED, 255, 0))

    def test_leader_index_requires_one_leader(self):
        self.assertEqual(militia.leader_index([{"local_leader": False}, {"local_leader": True}]), 1)
        with self.assertRaises(militia.SmokeFailure):
            militia.leader_index([{"local_leader": True}, {"local_leader": True}])


if __name__ == "__main__":
    unittest.main()
