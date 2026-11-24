"""Controlled native return cannot imply combat or bypass shared decisions."""
import contextlib
from copy import deepcopy
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

import full_engine_coop_installed_controlled_battle_return as controlled
from test_full_engine_coop_installed_cohort_restore import cohort
from test_full_engine_coop_installed_defensive_entry import states
from test_full_engine_coop_installed_hiring_restore import snapshot
from test_full_engine_coop_installed_battle_notice import view


class ControlledBattleReturnTests(unittest.TestCase):
    def test_seed_scope_cannot_certify_real_combat(self):
        for outcome in ("defeat", "capture", "victory"):
            scope = controlled.evidence_scope(outcome)
            self.assertIs(scope["seeded_native_fixture"], True)
            self.assertEqual(scope["qualification_scope"], f"seeded_native_{outcome}_battle_return")
            for field in ("unseeded_combat_verified", "two_player_combat_contribution_verified", "victory_verified",
                          "native_hire_arrival_verified", "confiscated_gear_restored", "physical_input_verified", "paint_verified", "passed"):
                self.assertIs(scope[field], False)
        with self.assertRaises(controlled.SmokeFailure):
            controlled.evidence_scope("retreat")

    def test_boundary_probe_cannot_smuggle_combat_commands(self):
        underlying = Mock()
        probe = controlled.BoundaryProbe(underlying)
        for verb in ("FIRE", "MOVE", "ENDTURN", "KEY", "SURRENDER_INPUT", "HIRE", "PASS_INTERRUPT"):
            with self.subTest(verb=verb), self.assertRaises(controlled.SmokeFailure):
                probe.send(verb, 1)
        for verb, args in (("STATE", (1,)), ("CAMPAIGN_INPUT", (1,) * 9), ("CAMPAIGN_INPUT", (1,) * 10),
                           ("BATTLE_NOTICE_INPUT", (1,) * 5), ("BATTLE_NOTICE_INPUT", (1,) * 5 + (3,))):
            with self.subTest(verb=verb, args=args), self.assertRaises(controlled.SmokeFailure):
                probe.send(verb, *args)
        underlying.send.assert_not_called()
        for verb, args in (("STATE", ()), ("CAMPAIGN_INPUT", (1,) * 9 + (16,)),
                           ("CAMPAIGN_INPUT", (1,) * 9 + (11,)), ("BATTLE_NOTICE_INPUT", (1,) * 5 + (2,))):
            probe.send(verb, *args)
        self.assertEqual(underlying.send.call_count, 4)

    def test_trigger_published_atomically_and_never_replayed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            record = controlled.write_trigger(root, "capture", states())
            expected = "JA2_COOP_CONTROLLED_BATTLE_V1 capture 1 1\n"
            self.assertEqual((root / "controlled-battle-trigger.txt").read_text(), expected)
            self.assertEqual(Path(record["path"]).read_text(), expected)
            self.assertIs(record["replay_forbidden"], True)
            with self.assertRaises(FileExistsError):
                controlled.write_trigger(root, "capture", states())
            self.assertEqual((root / "controlled-battle-trigger.txt").read_text(), expected)

    def test_trigger_rejects_unready_foreign_turn_or_unsettled_world(self):
        for field, value in (("ready", False), ("commands_blocked", True), ("outstanding", 1),
                             ("resynchronizing", True), ("interrupt_phase", 1), ("interrupt_phase", 2), ("team", 1)):
            values = states()
            for state in values:
                state[field] = value
            with tempfile.TemporaryDirectory() as directory:
                with self.subTest(field=field, value=value), self.assertRaises((controlled.SmokeFailure, controlled.MovementFailure)):
                    controlled.write_trigger(Path(directory), "defeat", values)
                self.assertFalse(list(Path(directory).iterdir()))
        values = states()
        values[1]["turn_serial"] += 1
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(controlled.SmokeFailure):
            controlled.write_trigger(Path(directory), "defeat", values)

    def test_partition_retains_pending_hire_and_exact_native_actor_order(self):
        source = snapshot()
        dead, captured = controlled.partition(source, "capture")
        self.assertEqual(dead, {(0, 1), (1, 2)})
        self.assertEqual(captured, {(2, 3), (3, 4)})
        self.assertEqual(controlled.partition(source, "victory"), (set(), set()))
        self.assertNotIn((4, 5), dead | captured)
        source["economy"][0]["roster"].reverse()
        self.assertEqual(controlled.partition(source, "capture"), (dead, captured))
        self.assertEqual(controlled.partition(source, "defeat"), (dead | captured, set()))

    def test_cold_resume_retains_complete_loss_cohort_under_a_fresh_epoch(self):
        before = cohort("capture")
        for economy in before["economy"]:
            for actor in economy["roster"]:
                actor["in_sector"] = actor["assignment"] == 57
        current = deepcopy(before)
        for rows in current.values():
            for row in rows:
                row["session"] = 4
        for economy in current["economy"]:
            for actor in economy["roster"]:
                actor["in_sector"] = False
        controlled.require_cold_restored(current, before)
        stale_presence = deepcopy(current)
        for economy in stale_presence["economy"]:
            economy["roster"][0]["in_sector"] = True
        with self.assertRaises(controlled.SmokeFailure):
            controlled.require_cold_restored(stale_presence, before)
        with self.assertRaises(controlled.SmokeFailure):
            controlled.require_cold_restored(before, before)
        for kind, field, value in (("campaign", "world_seconds", 112201), ("campaign", "paused", False),
                                   ("economy", "balance", 13639), ("quotes", "arrival_minutes", 2251)):
            changed = deepcopy(current)
            for row in changed[kind]:
                row[field] = value
            with self.subTest(field=field), self.assertRaises((controlled.SmokeFailure, controlled.MovementFailure)):
                controlled.require_cold_restored(changed, before)
        changed = deepcopy(current)
        for economy in changed["economy"]:
            economy["roster"][2]["assignment"] = 57
        with self.assertRaises(controlled.SmokeFailure):
            controlled.require_cold_restored(changed, before)

    def test_world_retirement_clears_both_clients_controls_and_private_inventory(self):
        cleared = states()
        for state in cleared:
            state.update(ready=False, world=0, actors=[], assigned=[], inventories=[])
        controlled.require_tactical_cleared(cleared)
        for field, value in (("world", 1), ("actors", states()[0]["actors"]),
                             ("assigned", [[0, 1]]), ("inventories", [{}])):
            changed = deepcopy(cleared)
            changed[1][field] = value
            with self.subTest(field=field), self.assertRaises(controlled.SmokeFailure):
                controlled.require_tactical_cleared(changed)

    def test_held_notice_allows_fresh_group_revisions_but_not_new_notices_or_worlds(self):
        previous = [view(), view()]
        for row in previous:
            row['sector_control_lost'] = True
        current = deepcopy(previous)
        for row in current:
            row['groups_revision'] += 1
        campaigns = snapshot()['campaign']
        for row in campaigns:
            row.update(phase=3, world_seconds=controlled.BATTLE_SECONDS)
        tactical = states()
        controlled.require_held_loss(current, previous, campaigns, tactical, 1)
        for field, value in (('notice', 3), ('sector_control_lost', False), ('groups_revision', 0)):
            changed = deepcopy(current)
            changed[1][field] = value
            with self.subTest(field=field), self.assertRaises(controlled.SmokeFailure):
                controlled.require_held_loss(changed, previous, campaigns, tactical, 1)
        for field, value in (('phase', 2), ('paused', False), ('world_seconds', controlled.BATTLE_SECONDS + 1)):
            changed = deepcopy(campaigns)
            changed[1][field] = value
            with self.subTest(field=field), self.assertRaises(controlled.SmokeFailure):
                controlled.require_held_loss(current, previous, changed, tactical, 1)
        changed = deepcopy(tactical)
        changed[1]['world'] += 1
        with self.assertRaises(controlled.SmokeFailure):
            controlled.require_held_loss(current, previous, campaigns, changed, 1)

    def test_arguments_require_explicit_source_outcome_and_finite_bounds(self):
        valid = ["--source-evidence=/test/hiring.json", "--outcome=defeat"]
        self.assertEqual(controlled.parse_arguments(valid).notice_hold_seconds, 12)
        for args in ([], valid + ["--outcome=retreat"], valid + ["--timeout-seconds=nan"],
                     valid + ["--timeout-seconds=601"], valid + ["--notice-hold-seconds=inf"], valid + ["--notice-hold-seconds=0"]):
            with self.subTest(args=args), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                controlled.parse_arguments(args)


if __name__ == "__main__":
    unittest.main()
