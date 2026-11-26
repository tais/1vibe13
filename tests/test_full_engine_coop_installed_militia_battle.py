"""Militia battle probe helpers."""
import unittest

import full_engine_coop_installed_militia_battle as militia


LINE = ("[dedicated] militia battle autoresolved: sector=9,2; outcome=held; militia=12; militia_lost=0; "
        "enemies=10; enemies_killed=10; battle_seconds=13; sector_lost=0")


class MilitiaBattleProbeTests(unittest.TestCase):
    def test_parses_the_server_summary(self):
        self.assertEqual(militia.parse_resolution("noise\n" + LINE + "\n"),
                         {"x": 9, "y": 2, "held": True, "militia": 12, "militia_lost": 0, "enemies": 10,
                          "enemies_killed": 10, "battle_seconds": 13, "sector_lost": False})
        self.assertIsNone(militia.parse_resolution("[dedicated] something else"))

    def test_expected_notice_matches_the_outcome(self):
        held = militia.parse_resolution(LINE)
        self.assertEqual(militia.expected_notice(held)["outcome"], militia.MILITIA_HELD)
        lost = dict(held, held=False, militia_lost=300, sector_lost=True)
        notice = militia.expected_notice(lost)
        self.assertEqual((notice["outcome"], notice["militia_lost"], notice["z"], notice["sector_control_lost"]),
                         (militia.MILITIA_DEFEATED, 255, 0, False))
        self.assertTrue(militia.expected_notice(lost, important=True)["sector_control_lost"])

    def test_control_changes(self):
        text = ("[co-op entry test] militia target enemy-controlled=0; message-box=0; clock=1\n"
                "[co-op entry test] militia target enemy-controlled=1; message-box=0; clock=9\n")
        self.assertEqual(militia.control_changes(text), [(False, False), (True, False)])

    def test_leader_index_requires_one_leader(self):
        self.assertEqual(militia.leader_index([{"local_leader": False}, {"local_leader": True}]), 1)
        with self.assertRaises(militia.SmokeFailure):
            militia.leader_index([{"local_leader": True}, {"local_leader": True}])


if __name__ == "__main__":
    unittest.main()
