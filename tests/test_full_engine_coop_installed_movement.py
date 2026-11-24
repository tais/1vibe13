"""Data-free checks for the installed movement certification's evidence gates."""

from __future__ import annotations

import copy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_movement as movement


class FakeMoveProbe:
    def __init__(self):
        self.moves = []

    def send(self, verb, *arguments):
        self.moves.append((verb, *arguments))
        return len(self.moves)

    def receive(self, request, _kind, _deadline, _guard):
        return {"submitted": True, "result": 0, "command": request}


def state(*, moved=False, ap=100, status=3, reason=0, revision=11, in_combat=True):
    actor = {
        "slot": 0, "inc": 1, "grid": 18 if moved else 17, "level": 0,
        "direction": 2, "stance": 1, "ap": ap, "life": 90, "breath": 99,
        "animation": 0, "pose": True,
        "world_x": (4 if moved else 3) * movement.CELL_Q8 + movement.CELL_Q8 // 2,
        "world_y": 2 * movement.CELL_Q8 + movement.CELL_Q8 // 2,
    }
    return {
        "ready": True, "session": 1, "world": 2, "revision": revision,
        "columns": 7, "rows": 5, "turn_serial": 1, "team": 0,
        "turn_based": True, "in_combat": in_combat, "commands_blocked": False,
        "resynchronizing": False, "outstanding": 0,
        "receipt": {"command": 1, "status": status, "reason": reason,
                    "revision": revision},
        "assigned": [[0, 1]], "actors": [actor],
    }


class MovementEvidenceTests(unittest.TestCase):
    def wait_move(self, observed, timeout=0.8, combat=True):
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        initial = [state(revision=10, in_combat=combat),
                   state(revision=10, in_combat=combat)]
        with patch.object(movement, "_states", return_value=observed), \
                patch.object(movement.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(movement.time, "sleep", side_effect=sleep):
            return movement._wait_move(
                [object(), object()], lambda: None, 0, 1, (0, 1), 18,
                initial, timeout)

    def test_applied_receipt_and_spent_ap_do_not_prove_movement(self):
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "coordinate changes=\\[False, False\\]"):
            self.wait_move([state(ap=98), state(ap=98)])

    def test_logical_grid_advancing_does_not_prove_render_movement(self):
        observed = [state(ap=98), state(ap=98)]
        for snapshot in observed:
            snapshot["actors"][0]["grid"] = 18
        with self.assertRaisesRegex(movement.MovementFailure, "did not settle"):
            self.wait_move(observed)

    def test_timeout_retains_per_client_pose_and_receipt_diagnostics(self):
        observed = [state(ap=98, revision=11), state(ap=97, revision=12)]
        observed[0]["outstanding"] = 1
        with self.assertRaises(movement.MovementFailure) as failure:
            self.wait_move(observed)
        message = str(failure.exception)
        for evidence in ('"revision":11', '"revision":12', '"grid":17',
                         '"world_x":8960', '"world_y":6400', '"ap":98',
                         '"ap":97', '"outstanding":1', '"command":1',
                         '"status":3', '"reason":0'):
            self.assertIn(evidence, message)

    def test_one_moving_replica_is_not_success(self):
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "coordinate changes=\\[True, False\\]"):
            self.wait_move([state(moved=True, ap=98), state(ap=98)])

    def test_disagreeing_ap_is_not_convergence(self):
        with self.assertRaisesRegex(movement.MovementFailure, "did not settle"):
            self.wait_move([state(moved=True, ap=98), state(moved=True, ap=97)])

    def test_center_and_coordinates_require_ap_cost_and_command_unlock(self):
        for ap, outstanding in ((100, 0), (-1, 0), (98, 1)):
            with self.subTest(ap=ap, outstanding=outstanding):
                observed = [state(moved=True, ap=ap), state(moved=True, ap=ap)]
                observed[0]["outstanding"] = outstanding
                with self.assertRaisesRegex(movement.MovementFailure, "did not settle"):
                    self.wait_move(observed)

    def test_exact_movement_converges_without_incidental_animation_equality(self):
        observed = [state(moved=True, ap=98, revision=11),
                    state(moved=True, ap=98, revision=12)]
        observed[1]["actors"][0]["animation"] = 37
        observed[1]["actors"][0]["breath"] = 98
        result, evidence = self.wait_move(observed)
        self.assertEqual(result, "applied")
        self.assertEqual(evidence, movement._projection(observed[0]["actors"][0]))
        self.assertNotIn("animation", evidence)

    def test_realtime_movement_accepts_unchanged_or_regenerated_ap(self):
        for ap in (100, 105):
            with self.subTest(ap=ap):
                observed = [state(moved=True, ap=ap, in_combat=False),
                            state(moved=True, ap=ap, in_combat=False)]
                result, evidence = self.wait_move(observed, combat=False)
                self.assertEqual(result, "applied")
                self.assertEqual(evidence["ap"], ap)

    def test_realtime_movement_rejects_negative_decreased_or_divergent_ap(self):
        for left_ap, right_ap in ((-1, -1), (98, 98), (100, 105)):
            with self.subTest(left_ap=left_ap, right_ap=right_ap):
                observed = [state(moved=True, ap=left_ap, in_combat=False),
                            state(moved=True, ap=right_ap, in_combat=False)]
                with self.assertRaisesRegex(movement.MovementFailure, "did not settle"):
                    self.wait_move(observed, combat=False)

    def test_queued_does_not_count_as_applied(self):
        with self.assertRaisesRegex(movement.MovementFailure, "Applied=False"):
            self.wait_move([state(moved=True, ap=98, status=1),
                            state(moved=True, ap=98)])

    def test_rejection_with_spent_ap_cannot_skip_to_another_candidate(self):
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "rejected move changed position or spent AP"):
            self.wait_move([state(ap=98, status=2, reason=12), state(ap=98)])

    def test_truthful_gameplay_rejection_and_revision_race_are_distinct(self):
        for reason, expected in ((12, "rejected"), (5, "stale"), (9, "stale")):
            with self.subTest(reason=reason):
                result, evidence = self.wait_move(
                    [state(status=2, reason=reason), state()])
                self.assertEqual(result, expected)
                self.assertIsNone(evidence)

    def test_baseline_not_ready_rejection_requires_unlock_and_no_side_effects(self):
        for problem in ("spent_ap", "moved", "locked"):
            with self.subTest(problem=problem):
                observed = [state(status=2, reason=9), state()]
                if problem == "spent_ap":
                    observed[0]["actors"][0]["ap"] -= 1
                elif problem == "moved":
                    observed[0]["actors"][0]["world_x"] += 1
                else:
                    observed[0]["outstanding"] = 1
                with self.assertRaises(movement.MovementFailure):
                    self.wait_move(observed)

    def test_retryable_rejection_after_applied_cannot_trigger_resubmission(self):
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        responses = [[state(), state()], [state(status=2, reason=9), state()]]
        with patch.object(movement, "_states", side_effect=responses), \
                patch.object(movement.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(movement.time, "sleep", side_effect=sleep):
            with self.assertRaisesRegex(movement.MovementFailure,
                                        "rejected after an Applied receipt"):
                movement._wait_move(
                    [object(), object()], lambda: None, 0, 1, (0, 1), 18,
                    [state(revision=10), state(revision=10)], 0.8)

    def test_world_identity_change_is_terminal(self):
        observed = [state(), state()]
        observed[1]["world"] = 3
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "changed tactical world identity"):
            self.wait_move(observed)

    def test_candidates_do_not_wrap_rows_or_target_occupied_cells(self):
        snapshot = state()
        snapshot["actors"][0]["grid"] = 6
        occupied = copy.deepcopy(snapshot["actors"][0])
        occupied.update(slot=1, grid=13)
        snapshot["actors"].append(occupied)
        candidates = movement._candidate_moves(snapshot)
        self.assertTrue(candidates)
        self.assertLessEqual(len(candidates), 8)
        self.assertNotIn(((0, 1), 7), candidates)
        self.assertNotIn(((0, 1), 13), candidates)
        self.assertTrue(all(identity == (0, 1) for identity, _grid in candidates))
        snapshot["commands_blocked"] = True
        self.assertEqual(movement._candidate_moves(snapshot), [])

    def test_certification_rejects_shared_or_empty_ownership_before_moving(self):
        for assignments in (([[0, 1]], [[0, 1]]), ([[0, 1]], []),
                            ([], [[1, 1]])):
            with self.subTest(assignments=assignments):
                observed = [state(), state()]
                for snapshot, assigned in zip(observed, assignments):
                    snapshot["assigned"] = assigned
                with patch.object(movement, "_Probe", side_effect=lambda _: object()), \
                        patch.object(movement, "_guard", return_value=lambda: None), \
                        patch.object(movement, "_states", return_value=observed), \
                        patch.object(movement, "_candidate_moves") as candidates:
                    with self.assertRaises(movement.MovementFailure):
                        movement.certify_two_client_movement(
                            [object(), object()], object(), 1.0)
                    candidates.assert_not_called()

    def test_disjoint_nonempty_assignments_are_valid(self):
        observed = [state(), state()]
        observed[1]["assigned"] = [[1, 1], [2, 1]]
        movement._require_two_owners(observed)
        observed[1]["assigned"].append([0, 1])
        with self.assertRaisesRegex(movement.MovementFailure, "disjoint actor ownership"):
            movement._require_two_owners(observed)

    def certification_fixture(self):
        first = state()
        second_actor = copy.deepcopy(first["actors"][0])
        second_actor.update(slot=1, inc=2, grid=18,
                            world_x=4 * movement.CELL_Q8 + movement.CELL_Q8 // 2)
        first["actors"].append(second_actor)
        second = copy.deepcopy(first)
        second["assigned"] = [[1, 2]]
        settled = [movement._projection(actor) for actor in first["actors"]]
        return [first, second], settled

    def test_actual_certification_stops_after_eight_fresh_snapshot_rejections(self):
        observed, _settled = self.certification_fixture()
        probes = [FakeMoveProbe(), FakeMoveProbe()]
        with patch.object(movement, "_Probe", side_effect=probes), \
                patch.object(movement, "_guard", return_value=lambda: None), \
                patch.object(movement, "_states", return_value=observed) as snapshots, \
                patch.object(movement, "_wait_move", return_value=("stale", None)) as wait, \
                patch.object(movement.time, "sleep") as sleep:
            with self.assertRaisesRegex(movement.MovementFailure,
                                        "exceeded 8 fresh-snapshot attempts"):
                movement.certify_two_client_movement([object(), object()], object(), 30.0)
        self.assertEqual(len(probes[0].moves), 8)
        self.assertEqual(len(set(probes[0].moves)), 1)
        self.assertEqual(probes[1].moves, [])
        self.assertEqual(wait.call_count, 8)
        self.assertEqual(snapshots.call_count, 10)  # Startup, owner selection, eight fresh views.
        self.assertEqual([call.args[0] for call in sleep.call_args_list],
                         [0.01, 0.03, 0.05, 0.09, 0.01, 0.03, 0.05])

    def test_actual_certification_accepts_last_bounded_attempt_without_resending_applied(self):
        observed, settled = self.certification_fixture()
        probes = [FakeMoveProbe(), FakeMoveProbe()]
        outcomes = [("stale", None)] * 7 + [("applied", settled[0]), ("applied", settled[1])]
        with patch.object(movement, "_Probe", side_effect=probes), \
                patch.object(movement, "_guard", return_value=lambda: None), \
                patch.object(movement, "_states", return_value=observed), \
                patch.object(movement, "_wait_move", side_effect=outcomes), \
                patch.object(movement.time, "sleep"), patch("builtins.print"):
            evidence = movement.certify_two_client_movement(
                [object(), object()], object(), 30.0)
        self.assertEqual(len(probes[0].moves), 8)
        self.assertEqual(len(probes[1].moves), 1)
        self.assertEqual(evidence["actors"], settled)

    def test_actual_certification_never_resends_an_applied_move_that_did_not_finish(self):
        observed, _settled = self.certification_fixture()
        probes = [FakeMoveProbe(), FakeMoveProbe()]
        with patch.object(movement, "_Probe", side_effect=probes), \
                patch.object(movement, "_guard", return_value=lambda: None), \
                patch.object(movement, "_states", return_value=observed), \
                patch.object(movement, "_wait_move", side_effect=movement.MovementFailure(
                    "move did not settle; Applied=True")) as wait:
            with self.assertRaisesRegex(movement.MovementFailure, "Applied=True"):
                movement.certify_two_client_movement([object(), object()], object(), 30.0)
        self.assertEqual(len(probes[0].moves), 1)
        self.assertEqual(probes[1].moves, [])
        self.assertEqual(wait.call_count, 1)

    def reconnect_fixture(self):
        observed = state(moved=True, ap=98)
        second = copy.deepcopy(observed["actors"][0])
        second.update(slot=1, inc=2, grid=19, ap=97,
                      world_x=5 * movement.CELL_Q8 + movement.CELL_Q8 // 2)
        observed["actors"].append(second)
        expected = {"session": 1, "world": 2,
                    "actors": [movement._projection(actor)
                               for actor in observed["actors"]]}
        return observed, expected

    def verify_reconnect(self, observed, expected, observer=None):
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(movement, "_Probe", side_effect=lambda _: object()), \
                patch.object(movement, "_guard", return_value=lambda: None), \
                patch.object(movement, "_states", return_value=(
                    [observed] if observer is None else [observed, observer])), \
                patch.object(movement.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(movement.time, "sleep", side_effect=sleep):
            return movement.verify_reconnected_movement(
                object(), expected, 0.3,
                observer_client=None if observer is None else object())

    def test_reconnect_preserves_both_completed_movements(self):
        observed, expected = self.reconnect_fixture()
        observed["revision"] += 10
        observed["actors"][0]["animation"] = 37
        observed["actors"][1]["breath"] -= 1
        self.assertIsNone(self.verify_reconnect(observed, expected))

    def test_reconnect_cannot_succeed_with_empty_or_duplicate_evidence(self):
        _observed, expected = self.reconnect_fixture()
        for actors in ([], expected["actors"][:1],
                       [expected["actors"][0], expected["actors"][0]]):
            with self.subTest(actors=actors):
                incomplete = {**expected, "actors": actors}
                with patch.object(movement, "_Probe") as probe:
                    with self.assertRaisesRegex(movement.MovementFailure,
                                                "two different completed actors"):
                        movement.verify_reconnected_movement(object(), incomplete, 1.0)
                    probe.assert_not_called()

    def test_reconnect_cannot_pass_if_one_actor_lost_its_completed_position(self):
        observed, expected = self.reconnect_fixture()
        observed["actors"][1]["grid"] = 20
        observed["actors"][1]["world_x"] += movement.CELL_Q8
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "did not retain both completed movements"):
            self.verify_reconnect(observed, expected)

    def test_reconnect_cannot_pass_a_different_tactical_world(self):
        observed, expected = self.reconnect_fixture()
        observed["world"] += 1
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "changed tactical world identity"):
            self.verify_reconnect(observed, expected)

    def test_realtime_reconnect_ap_refresh_requires_current_replica_agreement(self):
        observed, expected = self.reconnect_fixture()
        expected["ap_checks"] = [
            {"slot": actor["slot"], "inc": actor["inc"],
             "mode": "realtime", "turn_serial": 1}
            for actor in expected["actors"]]
        observed["in_combat"] = False
        for actor in observed["actors"]:
            actor["ap"] = 105
        observer = copy.deepcopy(observed)
        self.assertIsNone(self.verify_reconnect(observed, expected, observer))
        with self.assertRaisesRegex(movement.MovementFailure, "live observer client"):
            self.verify_reconnect(observed, expected)
        observer["actors"][1]["ap"] = 104
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "did not retain both completed movements"):
            self.verify_reconnect(observed, expected, observer)

    def test_reconnect_ap_refresh_does_not_relax_pose_or_allow_negative_ap(self):
        observed, expected = self.reconnect_fixture()
        observed["in_combat"] = False
        for failure in ("negative_ap", "different_grid"):
            candidate = copy.deepcopy(observed)
            if failure == "negative_ap":
                candidate["actors"][1]["ap"] = -1
            else:
                candidate["actors"][1]["grid"] += 1
            with self.subTest(failure=failure):
                with self.assertRaisesRegex(movement.MovementFailure,
                                            "did not retain both completed movements"):
                    self.verify_reconnect(candidate, expected, copy.deepcopy(candidate))

    def test_same_combat_turn_reconnect_still_requires_original_ap(self):
        observed, expected = self.reconnect_fixture()
        expected["ap_checks"] = [
            {"slot": actor["slot"], "inc": actor["inc"],
             "mode": "combat", "turn_serial": 1}
            for actor in expected["actors"]]
        observed["actors"][0]["ap"] += 1
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "did not retain both completed movements"):
            self.verify_reconnect(observed, expected, copy.deepcopy(observed))
        observed["turn_serial"] = 2
        self.assertIsNone(self.verify_reconnect(observed, expected, copy.deepcopy(observed)))

    def advanced_combat_reconnect_fixture(self, initial_mode="realtime"):
        observed, expected = self.reconnect_fixture()
        expected["ap_checks"] = [
            {"slot": actor["slot"], "inc": actor["inc"],
             "mode": initial_mode, "turn_serial": 1}
            for actor in expected["actors"]]
        observed.update(turn_serial=4, revision=5987, in_combat=True)
        observed["actors"][0].update(life=64, stance=2, direction=1, ap=91)
        observed["actors"][1].update(stance=2, ap=96)
        return observed, expected

    def test_reconnect_accepts_matching_attack_and_suppression_after_combat_advances(self):
        for initial_mode in ("realtime", "combat"):
            with self.subTest(initial_mode=initial_mode):
                observed, expected = self.advanced_combat_reconnect_fixture(initial_mode)
                observer = copy.deepcopy(observed)
                observer["revision"] += 6  # Incidental frame publication may differ.
                self.assertIsNone(self.verify_reconnect(observed, expected, observer))

    def test_reconnect_combat_changes_require_explicit_advanced_phase_evidence(self):
        for problem in ("missing", "duplicate", "invalid_mode", "same_turn",
                        "earlier_turn", "realtime_only"):
            with self.subTest(problem=problem):
                observed, expected = self.advanced_combat_reconnect_fixture()
                if problem == "missing":
                    del expected["ap_checks"]
                elif problem == "duplicate":
                    expected["ap_checks"].append(copy.deepcopy(expected["ap_checks"][0]))
                elif problem == "invalid_mode":
                    expected["ap_checks"][0]["mode"] = "unknown"
                elif problem == "same_turn":
                    observed["turn_serial"] = 1
                elif problem == "earlier_turn":
                    observed["turn_serial"] = 0
                else:
                    observed["in_combat"] = False
                with self.assertRaisesRegex(movement.MovementFailure,
                                            "did not retain both completed movements"):
                    self.verify_reconnect(observed, expected, copy.deepcopy(observed))

    def test_reconnect_combat_changes_require_a_live_observer(self):
        observed, expected = self.advanced_combat_reconnect_fixture()
        with self.assertRaisesRegex(movement.MovementFailure, "live observer client"):
            self.verify_reconnect(observed, expected)

    def test_reconnect_same_phase_keeps_historical_life_stance_and_direction_strict(self):
        for field in ("life", "stance", "direction"):
            for with_observer in (False, True):
                with self.subTest(field=field, with_observer=with_observer):
                    observed, expected = self.reconnect_fixture()
                    observed["actors"][0][field] += 1
                    with self.assertRaisesRegex(movement.MovementFailure,
                                                "did not retain both completed movements"):
                        self.verify_reconnect(
                            observed, expected,
                            copy.deepcopy(observed) if with_observer else None)

    def test_reconnect_combat_changes_never_hide_live_actor_disagreement(self):
        for field in movement.STABLE_FIELDS:
            with self.subTest(field=field):
                observed, expected = self.advanced_combat_reconnect_fixture()
                observer = copy.deepcopy(observed)
                observer["actors"][0][field] += 1
                with self.assertRaises(movement.MovementFailure):
                    self.verify_reconnect(observed, expected, observer)

    def test_reconnect_combat_changes_never_hide_lost_historical_positions(self):
        for field in movement.POSITION_FIELDS:
            with self.subTest(field=field):
                observed, expected = self.advanced_combat_reconnect_fixture()
                observed["actors"][0][field] += 1
                with self.assertRaises(movement.MovementFailure):
                    self.verify_reconnect(observed, expected, copy.deepcopy(observed))

    def test_reconnect_combat_changes_never_allow_negative_ap(self):
        observed, expected = self.advanced_combat_reconnect_fixture()
        observed["actors"][0]["ap"] = -1
        with self.assertRaisesRegex(movement.MovementFailure,
                                    "did not retain both completed movements"):
            self.verify_reconnect(observed, expected, copy.deepcopy(observed))

    def test_reconnect_combat_changes_require_matching_current_tactical_phase(self):
        for field in movement.PHASE_FIELDS:
            with self.subTest(field=field):
                observed, expected = self.advanced_combat_reconnect_fixture()
                observer = copy.deepcopy(observed)
                value = observer[field]
                observer[field] = not value if isinstance(value, bool) else value + 1
                with self.assertRaisesRegex(movement.MovementFailure,
                                            "did not retain both completed movements"):
                    self.verify_reconnect(observed, expected, observer)


if __name__ == "__main__":
    unittest.main()
