"""Controlled persistence evidence cannot substitute for a completed battle."""
import contextlib
from copy import deepcopy
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

import full_engine_coop_installed_cohort_restore as restore
from test_full_engine_coop_installed_hiring_restore import checkpoint, snapshot


def cohort(outcome="death"):
    current = snapshot(3)
    for groups in current["groups"]:
        groups["groups"] = []
    for economy in current["economy"]:
        for index, actor in enumerate(economy["roster"][:4]):
            prisoner = outcome == "capture" and index >= 2
            actor.update(assignment=59 if prisoner else 57, group=[0, 0], sector=[13, 9, 0] if prisoner else [9, 1, 0])
    return current


class ControlledCohortTests(unittest.TestCase):
    def test_complete_death_and_native_first_capture_cohorts(self):
        for outcome in ("death", "capture"):
            current = cohort(outcome)
            untouched = deepcopy(current)
            restore.require_cohort(current, snapshot(), outcome)
            self.assertEqual(current, untouched)
            with self.assertRaises(restore.SmokeFailure):
                restore.require_cohort(current, snapshot(), "capture" if outcome == "death" else "death")

    def test_fresh_session_and_exact_profile_bindings_required(self):
        for change in ("session", "profile", "identity", "missing"):
            current = cohort()
            for index in range(2):
                if change == "session":
                    for rows in current.values():
                        rows[index]["session"] = 2
                elif change == "missing":
                    current["economy"][index]["roster"].pop()
                else:
                    actors = current["economy"][index]["roster"]
                    key = "profile" if change == "profile" else "actor"
                    actors[1][key] = deepcopy(actors[0][key])
            with self.subTest(change=change), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                restore.require_cohort(current, snapshot(), "death")

    def test_pending_hire_contract_and_prison_location_are_exact(self):
        changes = ((4, "assignment", 57), (4, "arrival_minutes", 2251), (4, "pending_hire", False),
                   (4, "contract_end_minutes", 11940), (4, "group", [1, 1]), (4, "sector", [13, 9, 0]),
                   (0, "contract_end_minutes", 13740), (0, "assignment", 0), (0, "in_sector", True),
                   (2, "assignment", 57), (2, "sector", [7, 14, 0]), (2, "between", True), (2, "vehicle", True))
        for index, key, value in changes:
            current = cohort("capture")
            for economy in current["economy"]:
                economy["roster"][index][key] = value
            with self.subTest(index=index, key=key), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                restore.require_cohort(current, snapshot(), "capture")

    def test_clock_funds_groups_and_offers_cannot_drift(self):
        changes = (("campaign", "world_seconds", 112201), ("campaign", "phase", 3),
                   ("campaign", "paused", False), ("campaign", "pause_locked", True),
                   ("economy", "balance", 13639), ("economy", "mercenary_limit", 41),
                   ("groups", "groups", snapshot()["groups"][0]["groups"]),
                   ("quotes", "landing", [10, 1]), ("quotes", "arrival_minutes", 2251), ("quotes", "quotes", []))
        for kind, key, value in changes:
            current = cohort()
            for row in current[kind]:
                row[key] = deepcopy(value)
            with self.subTest(kind=kind, key=key), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                restore.require_cohort(current, snapshot(), "death")

    def test_two_clients_and_cross_projection_revisions_must_agree(self):
        for kind in ("campaign", "groups", "economy", "quotes"):
            current = cohort()
            current[kind][1]["revision"] += 1
            with self.subTest(kind=kind), self.assertRaises(restore.SmokeFailure):
                restore.require_cohort(current, snapshot(), "death")
        current = cohort()
        for quotes in current["quotes"]:
            quotes["economy_revision"] += 1
        with self.assertRaises(restore.SmokeFailure):
            restore.require_cohort(current, snapshot(), "death")

    def test_reconnect_may_advance_status_but_not_handles_or_leadership(self):
        before, current = cohort("capture"), cohort("capture")
        for status in current["campaign"]:
            status["revision"] += 2
        restore.require_reconnected(current, before, snapshot(), "capture")
        for change in ("session", "leader", "time_control_revision", "actor", "quote_revision"):
            changed = deepcopy(current)
            for index in range(2):
                if change == "actor":
                    changed["economy"][index]["roster"][0]["actor"][1] += 100
                elif change == "quote_revision":
                    changed["quotes"][index]["revision"] += 1
                else:
                    changed["campaign"][index][change] = [2] + [0] * 15 if change == "leader" else 100
                    if change == "session":
                        for rows in changed.values():
                            rows[index]["session"] = 100
            with self.subTest(change=change), self.assertRaises((restore.SmokeFailure, restore.MovementFailure)):
                restore.require_reconnected(changed, before, snapshot(), "capture")

    def test_seeded_evidence_disclaims_combat_and_native_arrival(self):
        for outcome in ("death", "capture"):
            evidence = restore.evidence_scope(outcome)
            self.assertEqual(evidence["qualification_scope"], f"seeded_native_{outcome}_cohort_restore")
            self.assertIs(evidence["seeded_native_fixture"], True)
            for key in ("combat_verified", "battle_return_verified", "victory_verified", "battle_acknowledgement_verified",
                        "confiscated_gear_restored", "native_arrival_verified", "physical_input_verified", "paint_verified", "passed"):
                self.assertIs(evidence[key], False)
        with self.assertRaises(restore.SmokeFailure):
            restore.evidence_scope("victory")

    def test_only_argument_free_observations_are_permitted(self):
        underlying = Mock()
        probe = restore.ReadOnlyProbe(underlying)
        for verb in ("TIME", "KEY", "CLICK", "HIRE", "TRAVEL", "ACK_BATTLE"):
            with self.subTest(verb=verb), self.assertRaises(restore.SmokeFailure):
                probe.send(verb)
        with self.assertRaises(restore.SmokeFailure):
            probe.send("CAMPAIGN", 1)
        underlying.send.assert_not_called()
        probe.send("CAMPAIGN")
        underlying.send.assert_called_once_with("CAMPAIGN")

    def test_seed_checkpoint_survives_runtime_slot_rotation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            checkpoint(root / "state")
            record = restore.pending_checkpoint_record(root / "state")
            saved = restore.preserve_checkpoint(record, root / "evidence")
            restore.require_checkpoint_match(saved, record)
            original = Path(saved["save_path"]).read_bytes()
            Path(record["save_path"]).write_bytes(b"rotated")
            self.assertEqual(Path(saved["save_path"]).read_bytes(), original)

    def test_outcome_source_and_finite_bounds_are_required(self):
        valid = ["--source-evidence=/test/source.json", "--outcome=death"]
        self.assertEqual(restore.parse_arguments(valid).hold_seconds, 3)
        for args in ([], valid + ["--outcome=victory"], valid + ["--timeout-seconds=nan"],
                     valid + ["--timeout-seconds=601"], valid + ["--hold-seconds=inf"], valid + ["--hold-seconds=0"]):
            with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                restore.parse_arguments(args)


if __name__ == "__main__":
    unittest.main()
