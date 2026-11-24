import re
import unittest
from pathlib import Path

from full_engine_coop_installed_battle_history import (
    BATTLE_RESULTS, BattleHistoryFailure, HISTORY_DEFENDEDTOWNSECTOR,
    HISTORY_RECORD, MAX_HISTORY_RECORDS, decode_history, observe_defensive_victory, observe_defensive_defeat,
)


def row(code=65, minute=1875, x=9, y=1, z=0, second=0, color=0):
    return HISTORY_RECORD.pack(code, second, minute, x, y, z, color)


class BattleHistoryTests(unittest.TestCase):
    def observe(self, before, after):
        return observe_defensive_victory(before, after, sector=(9, 1, 0),
                                        earliest_minute=1875, latest_minute=1876)

    def test_defeat_return_requires_loss_and_never_qualifies_victory(self):
        before = row(minute=1874)
        lost = before + row(code=64)
        args = dict(sector=(9, 1, 0), earliest_minute=1875, latest_minute=1876)
        result = observe_defensive_defeat(before, lost, **args)
        self.assertEqual(result["record"]["code"], 64)
        with self.assertRaises(BattleHistoryFailure):
            self.observe(before, lost)
        for changed in (before + row(code=65), before + row(code=66), lost + row(code=64), row(code=64)):
            with self.subTest(changed=changed), self.assertRaises(BattleHistoryFailure):
                observe_defensive_defeat(before, changed, **args)
        self.assertIsNone(observe_defensive_defeat(before, before, **args))

    def test_complete_native_scalar_layout(self):
        self.assertEqual(HISTORY_RECORD.size, 12)
        self.assertEqual(decode_history(row()), [dict(code=65, second_code=0,
                         minute=1875, x=9, y=1, z=0, color=0)])

    def test_constants_match_actual_native_history_enum(self):
        source = (Path(__file__).resolve().parents[1] / "Laptop/history.h").read_text()
        body = source.split("HISTORY_ENTERED_HISTORY_MODE=0,", 1)[1].split("TEXT_NUM_HISTORY", 1)[0]
        names = ["HISTORY_ENTERED_HISTORY_MODE", *re.findall(r"\bHISTORY_[A-Z0-9_]+\b", body)]
        expected = {25: "HISTORY_WONBATTLE", 64: "HISTORY_LOSTTOWNSECTOR",
                    65: "HISTORY_DEFENDEDTOWNSECTOR", 66: "HISTORY_LOSTBATTLE",
                    67: "HISTORY_FATALAMBUSH", 68: "HISTORY_WIPEDOUTENEMYAMBUSH",
                    69: "HISTORY_UNSUCCESSFULATTACK", 70: "HISTORY_SUCCESSFULATTACK",
                    72: "HISTORY_KILLEDBYBLOODCATS", 73: "HISTORY_SLAUGHTEREDBLOODCATS"}
        self.assertEqual(BATTLE_RESULTS, frozenset(expected))
        self.assertTrue(all(names[index] == name for index, name in expected.items()))

    def test_new_exact_defense_with_intervening_casualty(self):
        baseline = row(minute=1400)
        result = self.observe(baseline, baseline + row(code=3) + row())
        self.assertEqual(result["record_index"], 2)
        self.assertEqual(result["records_before"], 1)
        self.assertEqual(result["records_after"], 3)
        self.assertEqual(result["record"]["code"], HISTORY_DEFENDEDTOWNSECTOR)
        self.assertEqual(len(result["history_after_sha256"]), 64)

    def test_old_win_and_unrelated_sector_are_not_new_victory(self):
        baseline = row()
        self.assertIsNone(self.observe(baseline, baseline))
        self.assertIsNone(self.observe(baseline, baseline + row(x=10)))

    def test_losses_and_different_battle_kinds_fail(self):
        for code in BATTLE_RESULTS - {HISTORY_DEFENDEDTOWNSECTOR}:
            with self.subTest(code=code), self.assertRaises(BattleHistoryFailure):
                self.observe(b"", row(code=code))

    def test_ambiguous_outcomes_fail(self):
        for suffix in (row(), row(code=66), row(minute=1876)):
            with self.subTest(suffix=suffix), self.assertRaises(BattleHistoryFailure):
                self.observe(b"", row() + suffix)

    def test_rewrite_and_truncation_fail(self):
        baseline = row(code=1, minute=1870)
        for after in (b"", row(code=2, minute=1870), row(code=2, minute=1870) + row()):
            with self.subTest(after=after), self.assertRaises(BattleHistoryFailure):
                self.observe(baseline, after)

    def test_outcome_time_and_auxiliary_fields_fail(self):
        for result in (row(minute=1874), row(minute=1877), row(second=1), row(color=1)):
            with self.subTest(result=result), self.assertRaises(BattleHistoryFailure):
                self.observe(b"", result)

    def test_malformed_streams_and_bound_fail(self):
        for data in (bytearray(), b"x", row()[:-1], b"\0" * (12 * (MAX_HISTORY_RECORDS + 1))):
            with self.subTest(length=len(data)), self.assertRaises(BattleHistoryFailure):
                decode_history(data)

    def test_invalid_context_fails(self):
        for sector in ((0, 1, 0), (9, 1, 1), (True, 1, 0), [9, 1, 0]):
            with self.subTest(sector=sector), self.assertRaises(BattleHistoryFailure):
                observe_defensive_victory(b"", row(), sector=sector,
                                         earliest_minute=1875, latest_minute=1876)
        for start, end in ((1876, 1875), (-1, 1875), (1875, True), (1875, 0x100000000)):
            with self.subTest(start=start, end=end), self.assertRaises(BattleHistoryFailure):
                observe_defensive_victory(b"", row(), sector=(9, 1, 0),
                                         earliest_minute=start, latest_minute=end)


if __name__ == "__main__":
    unittest.main()
