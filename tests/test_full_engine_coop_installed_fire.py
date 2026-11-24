"""Data-free evidence checks for the opt-in installed fire certification."""

from __future__ import annotations

import copy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_fire as fire


def primary(*, ammo=6):
    return {"item": 5, "quantity": 1, "condition": 97, "ammo_state": True,
            "ammo_item": 77, "ammo_count": ammo, "ammo_condition": 100,
            "chambered": True, "jammed": False}


def actor(slot=0, *, hostile=False, ammo=6, ap=100):
    return {
        "slot": slot, "inc": slot + 1, "grid": 17 + slot, "level": 0,
        "direction": 2, "stance": 1, "ap": ap, "life": 90, "breath": 99,
        "animation": 0, "pose": True, "world_x": 1000 + slot * 1000,
        "world_y": 6400, "active": True, "in_sector": True,
        "team": 1 if hostile else 0, "hostile": hostile,
        "interrupt_eligible": False, "primary": primary(ammo=ammo),
    }


def states(*, ammo=6, ap=100, status=3, reason=0, revision=11, in_combat=True):
    first = {
        "ready": True, "session": 1, "world": 2, "revision": revision,
        "columns": 160, "rows": 160, "turn_serial": 1, "team": 0,
        "turn_based": True, "in_combat": in_combat, "commands_blocked": False,
        "interrupt_phase": 0, "interrupt_serial": 0,
        "resynchronizing": False, "outstanding": 0,
        "receipt": {"command": 1, "status": status, "reason": reason,
                    "revision": revision},
        "assigned": [[0, 1]],
        "actors": [actor(ammo=ammo, ap=ap), actor(1, ap=90), actor(2, hostile=True)],
    }
    first["actors"][1]["world_x"] = -10000
    second = copy.deepcopy(first)
    second["assigned"] = [[1, 2]]
    return [first, second]


class FakeFireProbe:
    def __init__(self, *, submitted=True, fixed_command=None):
        self.fires = []
        self.submitted = submitted
        self.fixed_command = fixed_command

    def send(self, verb, *arguments):
        self.fires.append((verb, *arguments))
        return len(self.fires)

    def receive(self, request, _kind, _deadline, _guard):
        return {"submitted": self.submitted, "result": 0 if self.submitted else "controller rejected",
                "command": (self.fixed_command if self.fixed_command is not None else request)
                if self.submitted else 0}


class FireEvidenceTests(unittest.TestCase):
    def wait_fire(self, observed, *, timeout=0.9, combat=True, initial=None):
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        if initial is None:
            initial = states(revision=10, in_combat=combat)
        with patch.object(fire, "_states", return_value=observed), \
                patch.object(fire.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(fire.time, "sleep", side_effect=sleep):
            return fire._wait_fire([object(), object()], lambda: None, 0, 1,
                                   (0, 1), (2, 3), initial, timeout)

    def test_applied_without_actual_loaded_ammo_loss_is_not_a_shot(self):
        with self.assertRaisesRegex(fire.FireFailure, "Applied=True"):
            self.wait_fire(states(ap=95))

    def test_chamber_change_and_target_damage_do_not_replace_loaded_ammo_loss(self):
        observed = states(ap=95)
        for snapshot in observed:
            snapshot["actors"][0]["primary"]["chambered"] = False
            snapshot["actors"][2]["life"] = 30
        with self.assertRaisesRegex(fire.FireFailure, "did not settle"):
            self.wait_fire(observed)

    def test_queued_ammo_loss_is_not_applied(self):
        with self.assertRaisesRegex(fire.FireFailure, "Applied=False"):
            self.wait_fire(states(ammo=5, ap=95, status=1))

    def test_applied_miss_is_proven_without_observing_queued(self):
        result, evidence = self.wait_fire(states(ammo=5, ap=95))
        self.assertEqual(result, "applied")
        self.assertEqual(evidence["ammo_spent"], 1)
        self.assertTrue(evidence["combat_ap_cost_verified"])
        self.assertEqual(evidence["ap_mode"], "combat-cost")
        self.assertEqual(evidence["primary_before"]["ammo_count"], 6)
        self.assertEqual(evidence["primary_after"]["ammo_count"], 5)

    def test_positive_nonunit_drop_and_manual_cycling_are_valid(self):
        observed = states(ammo=3, ap=95)
        for snapshot in observed:
            snapshot["actors"][0]["primary"].update(condition=96, chambered=False)
        result, evidence = self.wait_fire(observed)
        self.assertEqual(result, "applied")
        self.assertEqual(evidence["ammo_spent"], 3)
        self.assertFalse(evidence["primary_after"]["chambered"])

    def test_jam_deducting_ammo_does_not_prove_a_shot(self):
        for fields in ({"jammed": True, "ammo_condition": -100},
                       {"jammed": True}, {"ammo_condition": 0}, {"ammo_condition": -100}):
            with self.subTest(fields=fields):
                observed = states(ammo=5, ap=95)
                for snapshot in observed:
                    snapshot["actors"][0]["primary"].update(fields)
                with self.assertRaisesRegex(fire.FireFailure, "ammunition loss does not prove a shot"):
                    self.wait_fire(observed)

    def test_initial_jammed_gun_cannot_be_certified_by_unjamming(self):
        initial = states(revision=10)
        for snapshot in initial:
            snapshot["actors"][0]["primary"].update(jammed=True, ammo_condition=-100)
        with self.assertRaisesRegex(fire.FireFailure, "started with jammed"):
            self.wait_fire(states(ammo=5, ap=95), initial=initial)

    def test_both_replicas_must_advance_and_agree(self):
        for problem in ("ammo", "ap", "condition", "position", "revision", "phase", "quantity"):
            with self.subTest(problem=problem):
                observed = states(ammo=5, ap=95)
                if problem == "ammo":
                    observed[1]["actors"][0]["primary"]["ammo_count"] = 6
                elif problem == "ap":
                    observed[1]["actors"][0]["ap"] = 94
                elif problem == "condition":
                    observed[1]["actors"][0]["primary"]["condition"] = 96
                elif problem == "position":
                    observed[1]["actors"][0]["world_x"] += 1
                elif problem == "quantity":
                    observed[1]["actors"][0]["primary"]["quantity"] += 1
                elif problem == "revision":
                    observed[1]["revision"] = 10
                else:
                    observed[1]["turn_serial"] = 2
                with self.assertRaisesRegex(fire.FireFailure, "did not settle"):
                    self.wait_fire(observed)

    def test_weapon_or_ammunition_swap_cannot_look_like_a_shot(self):
        for field, value in (("item", 6), ("quantity", 2), ("ammo_item", 88),
                             ("ammo_state", False)):
            with self.subTest(field=field):
                observed = states(ammo=5, ap=95)
                for snapshot in observed:
                    snapshot["actors"][0]["primary"][field] = value
                with self.assertRaisesRegex(fire.FireFailure, "did not settle"):
                    self.wait_fire(observed)

    def test_same_combat_phase_requires_positive_ap_cost_and_unlock(self):
        for ap, outstanding in ((100, 0), (101, 0), (-1, 0), (95, 1)):
            with self.subTest(ap=ap, outstanding=outstanding):
                observed = states(ammo=5, ap=ap)
                observed[0]["outstanding"] = outstanding
                with self.assertRaisesRegex(fire.FireFailure, "did not settle"):
                    self.wait_fire(observed)

    def test_changed_turn_or_interrupt_never_claims_combat_ap_coverage(self):
        for field, value in (("turn_serial", 2), ("team", 1), ("in_combat", False),
                             ("turn_based", False), ("interrupt_phase", 2),
                             ("interrupt_serial", 7)):
            with self.subTest(field=field):
                observed = states(ammo=5, ap=105)
                for snapshot in observed:
                    snapshot[field] = value
                result, evidence = self.wait_fire(observed)
                self.assertEqual(result, "applied")
                self.assertFalse(evidence["combat_ap_cost_verified"])
                self.assertEqual(evidence["ap_mode"], "agreement-only")

    def test_realtime_start_has_only_ap_agreement_coverage(self):
        result, evidence = self.wait_fire(states(ammo=5, ap=100, in_combat=False), combat=False)
        self.assertEqual(result, "applied")
        self.assertFalse(evidence["combat_ap_cost_verified"])

    def test_transient_interrupt_does_not_restore_ap_coverage_when_it_clears(self):
        observed = states(ammo=5, ap=100)
        interrupted = copy.deepcopy(observed)
        for snapshot in interrupted:
            snapshot["interrupt_phase"] = 2
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(fire, "_states", side_effect=[interrupted] + [observed] * 10), \
                patch.object(fire.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(fire.time, "sleep", side_effect=sleep):
            result, evidence = fire._wait_fire([object(), object()], lambda: None, 0, 1,
                                               (0, 1), (2, 3), states(revision=10), 1.0)
        self.assertEqual(result, "applied")
        self.assertFalse(evidence["combat_ap_cost_verified"])

    def test_rejections_require_no_ammo_or_primary_mutation_or_ap_spend(self):
        for reason in (5, 9, 12):
            for problem in ("ammo", "condition", "chamber", "quantity", "ap", "locked"):
                with self.subTest(reason=reason, problem=problem):
                    observed = states(status=2, reason=reason)
                    shooter = observed[0]["actors"][0]
                    if problem == "ammo":
                        shooter["primary"]["ammo_count"] -= 1
                    elif problem == "condition":
                        shooter["primary"]["condition"] -= 1
                    elif problem == "chamber":
                        shooter["primary"]["chambered"] = False
                    elif problem == "quantity":
                        shooter["primary"]["quantity"] += 1
                    elif problem == "ap":
                        shooter["ap"] -= 1
                    else:
                        observed[0]["outstanding"] = 1
                    with self.assertRaises(fire.FireFailure):
                        self.wait_fire(observed)

    def test_retryable_and_gameplay_refusals_are_distinguished(self):
        for reason, expected in ((5, "stale"), (9, "stale"), (12, "rejected")):
            with self.subTest(reason=reason):
                result, evidence = self.wait_fire(states(status=2, reason=reason))
                self.assertEqual(result, expected)
                self.assertIsNone(evidence)

    def test_rejected_after_applied_is_terminal_not_retryable(self):
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(fire, "_states", side_effect=[states(), states(status=2, reason=9)]), \
                patch.object(fire.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(fire.time, "sleep", side_effect=sleep):
            with self.assertRaisesRegex(fire.FireFailure, "rejected after an Applied"):
                fire._wait_fire([object(), object()], lambda: None, 0, 1,
                                (0, 1), (2, 3), states(revision=10), 0.9)

    def test_rejection_side_effect_arriving_on_next_poll_is_not_hidden(self):
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(fire, "_states", side_effect=[states(status=2, reason=12),
                                                       states(status=2, reason=12, ammo=5)]), \
                patch.object(fire.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(fire.time, "sleep", side_effect=sleep):
            with self.assertRaisesRegex(fire.FireFailure, "changed primary"):
                fire._wait_fire([object(), object()], lambda: None, 0, 1,
                                (0, 1), (2, 3), states(revision=10), 0.9)

    def test_wrong_command_receipt_cannot_prove_success(self):
        observed = states(ammo=5, ap=95)
        observed[0]["receipt"]["command"] = 2
        with self.assertRaisesRegex(fire.FireFailure, "Applied=False"):
            self.wait_fire(observed)

    def test_world_change_and_unknown_receipt_are_terminal(self):
        observed = states(ammo=5, ap=95)
        observed[1]["world"] = 3
        with self.assertRaisesRegex(fire.FireFailure, "world identity"):
            self.wait_fire(observed)
        with self.assertRaisesRegex(fire.FireFailure, "status=2, reason=8"):
            self.wait_fire(states(status=2, reason=8))

    def test_timeout_contains_per_client_ammo_pose_phase_and_receipt_evidence(self):
        observed = states(ammo=5, ap=95)
        observed[1]["actors"][0]["primary"]["ammo_count"] = 6
        with self.assertRaises(fire.FireFailure) as failure:
            self.wait_fire(observed)
        for text in ('"ammo_count":5', '"ammo_count":6', '"quantity":1',
                     '"interrupt_phase":0', '"outstanding":0', '"command":1',
                     '"revision":11', '"world_x":1000', 'Applied=True'):
            self.assertIn(text, str(failure.exception))

    def test_candidate_pairs_require_live_owned_shooter_and_coherent_hostile_target(self):
        observed = states()
        self.assertEqual(fire._candidate_pairs(observed)[0], (0, (0, 1), (2, 3)))
        for problem in ("no_hostile", "dead_target", "out_of_sector", "mismatch", "blocked", "jammed"):
            with self.subTest(problem=problem):
                snapshots = copy.deepcopy(observed)
                for snapshot in snapshots:
                    if problem == "no_hostile":
                        snapshot["actors"][2]["hostile"] = False
                    elif problem == "dead_target":
                        snapshot["actors"][2]["life"] = 0
                    elif problem == "out_of_sector":
                        snapshot["actors"][2]["in_sector"] = False
                    elif problem == "blocked":
                        snapshot["commands_blocked"] = True
                    elif problem == "jammed":
                        for shooter in snapshot["actors"][:2]:
                            shooter["primary"].update(jammed=True, ammo_condition=-100)
                if problem == "mismatch":
                    snapshots[1]["actors"][2]["life"] -= 1
                self.assertEqual(fire._candidate_pairs(snapshots), [])

    def test_public_turn_and_interrupt_gates_wait_for_eligible_actor(self):
        observed = states()
        for snapshot in observed:
            snapshot["team"] = 1
        self.assertEqual(fire._candidate_pairs(observed), [])
        for snapshot in observed:
            snapshot.update(interrupt_phase=2, interrupt_serial=8)
            snapshot["actors"][1]["interrupt_eligible"] = True
        self.assertEqual(fire._candidate_pairs(observed), [(1, (1, 2), (2, 3))])
        for snapshot in observed:
            snapshot["interrupt_phase"] = 1
        self.assertEqual(fire._candidate_pairs(observed), [])

    def test_certificate_rejects_shared_or_empty_ownership_before_firing(self):
        for assignment in ([[0, 1]], []):
            observed = states()
            observed[1]["assigned"] = assignment
            with self.subTest(assignment=assignment), \
                    patch.object(fire, "_Probe", side_effect=lambda _: object()), \
                    patch.object(fire, "_guard", return_value=lambda: None), \
                    patch.object(fire, "_states", return_value=observed), \
                    patch.object(fire, "_candidate_pairs") as candidates:
                with self.assertRaises(fire.FireFailure):
                    fire.certify_two_client_fire([object(), object()], object(), 1.0)
                candidates.assert_not_called()

    def certificate(self, outcomes, probes=None, observed=None):
        probes = probes or [FakeFireProbe(), FakeFireProbe()]
        observed = observed or states(revision=10)
        with patch.object(fire, "_Probe", side_effect=probes), \
                patch.object(fire, "_guard", return_value=lambda: None), \
                patch.object(fire, "_states", return_value=observed), \
                patch.object(fire, "_wait_fire", side_effect=outcomes) as wait, \
                patch.object(fire.time, "sleep"), patch("builtins.print"):
            evidence = fire.certify_two_client_fire([object(), object()], object(), 10.0)
        return evidence, probes, wait

    @staticmethod
    def completed():
        return "applied", {"ammo_spent": 1, "ap_mode": "combat-cost",
                           "combat_ap_cost_verified": True}

    def test_exactly_one_accepted_fire_action_is_submitted(self):
        evidence, probes, wait = self.certificate([self.completed()])
        self.assertEqual(probes[0].fires, [("FIRE", 0, 1, 2, 3)])
        self.assertEqual(probes[1].fires, [])
        self.assertEqual(wait.call_count, 1)
        self.assertEqual(evidence["submitted_attempts"], 1)

    def test_only_stale_refusals_retry_fresh_command_ids_up_to_eight(self):
        outcomes = [("stale", None)] * 7 + [self.completed()]
        evidence, probes, wait = self.certificate(outcomes)
        self.assertEqual(len(probes[0].fires), 8)
        self.assertEqual(probes[1].fires, [])
        self.assertEqual([call.args[3] for call in wait.call_args_list], list(range(1, 9)))
        self.assertEqual(evidence["submitted_attempts"], 8)

    def test_eighth_stale_refusal_is_terminal(self):
        probes = [FakeFireProbe(), FakeFireProbe()]
        with self.assertRaisesRegex(fire.FireFailure, "exceeded 8 fresh-snapshot attempts"):
            self.certificate([("stale", None)] * 8, probes)
        self.assertEqual(len(probes[0].fires), 8)
        self.assertEqual(probes[1].fires, [])

    def test_reused_command_identity_cannot_be_called_a_fresh_retry(self):
        probes = [FakeFireProbe(fixed_command=7), FakeFireProbe()]
        with self.assertRaisesRegex(fire.FireFailure, "reused a submitted command"):
            self.certificate([("stale", None)], probes)
        self.assertEqual(len(probes[0].fires), 2)

    def test_gameplay_refusal_advances_candidate_not_repeat_fire(self):
        evidence, probes, wait = self.certificate([("rejected", None), self.completed()])
        self.assertEqual(probes[0].fires, [("FIRE", 0, 1, 2, 3)])
        self.assertEqual(probes[1].fires, [("FIRE", 1, 2, 2, 3)])
        self.assertEqual(wait.call_count, 2)
        self.assertEqual(evidence["refusals"][0]["reason"], 12)

    def test_applied_timeout_is_never_retried_or_switched_to_other_actor(self):
        probes = [FakeFireProbe(), FakeFireProbe()]
        with self.assertRaisesRegex(fire.FireFailure, "Applied=True"):
            self.certificate(fire.FireFailure("fire did not settle; Applied=True"), probes)
        self.assertEqual(len(probes[0].fires), 1)
        self.assertEqual(probes[1].fires, [])

    def test_controller_refusal_is_side_effect_checked_then_advances_candidate(self):
        probes = [FakeFireProbe(submitted=False), FakeFireProbe()]
        with patch.object(fire, "_wait_unsubmitted") as checked:
            evidence, probes, wait = self.certificate([self.completed()], probes)
        self.assertEqual(len(probes[0].fires), 1)
        self.assertEqual(len(probes[1].fires), 1)
        self.assertEqual(wait.call_count, 1)
        self.assertEqual(checked.call_count, 1)
        self.assertEqual(evidence["submitted_attempts"], 1)

    def test_controller_refusal_with_side_effects_stops_without_another_candidate(self):
        probes = [FakeFireProbe(submitted=False), FakeFireProbe()]
        with patch.object(fire, "_wait_unsubmitted", side_effect=fire.FireFailure("spent AP")):
            with self.assertRaisesRegex(fire.FireFailure, "spent AP"):
                self.certificate([], probes)
        self.assertEqual(len(probes[0].fires), 1)
        self.assertEqual(probes[1].fires, [])

    def test_no_hostile_waits_bounded_and_never_false_succeeds(self):
        observed = states()
        for snapshot in observed:
            snapshot["actors"][2]["hostile"] = False
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        probes = [FakeFireProbe(), FakeFireProbe()]
        with patch.object(fire, "_Probe", side_effect=probes), \
                patch.object(fire, "_guard", return_value=lambda: None), \
                patch.object(fire, "_states", return_value=observed), \
                patch.object(fire.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(fire.time, "sleep", side_effect=sleep):
            with self.assertRaisesRegex(fire.FireFailure, "no accepted fire among 0") as failure:
                fire.certify_two_client_fire([object(), object()], object(), 0.5)
        self.assertIn('"hostile":false', str(failure.exception))
        self.assertIn("overall fire deadline exhausted=true", str(failure.exception))
        self.assertIn("last completed public target counts=[0, 0]", str(failure.exception))
        self.assertIn("last eligible candidate pairs=0", str(failure.exception))
        self.assertEqual([probe.fires for probe in probes], [[], []])
        self.assertGreaterEqual(clock[0], 0.5)

    def candidate_observation_failure(self, observations, error, *, failure_time=0.5,
                                      fire_result=None):
        clock = [0.0]
        samples = iter(observations)
        probes = [FakeFireProbe(), FakeFireProbe()]

        def next_states(_probes, deadline, _guard):
            self.assertEqual(deadline, 0.5)
            try:
                return next(samples)
            except StopIteration:
                clock[0] = failure_time
                raise error

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(fire, "_Probe", side_effect=probes), \
                patch.object(fire, "_guard", return_value=lambda: None), \
                patch.object(fire, "_states", side_effect=next_states) as reads, \
                patch.object(fire, "_wait_fire", return_value=fire_result) as shots, \
                patch.object(fire.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(fire.time, "sleep", side_effect=sleep):
            with self.assertRaises(fire.FireFailure) as failure:
                fire.certify_two_client_fire([object(), object()], object(), 0.5)
        self.assertIs(failure.exception.__cause__, error)
        self.assertIn(f"state probe error={error}", str(failure.exception))
        self.assertEqual(reads.call_count, len(observations) + 1)
        self.assertEqual(shots.call_count, 0 if fire_result is None else 1)
        return str(failure.exception), probes

    @staticmethod
    def without_public_hostiles():
        observed = states(revision=5833)
        for snapshot in observed:
            snapshot["actors"][2]["hostile"] = False
        return observed

    def test_last_state_crossing_overall_deadline_preserves_no_target_context(self):
        error = fire.MovementFailure("tactical-client-a timed out answering state request 626")
        message, probes = self.candidate_observation_failure(
            [self.without_public_hostiles()], error)
        self.assertIn("overall fire deadline exhausted=true", message)
        self.assertIn("last completed public target counts=[0, 0]", message)
        self.assertIn("last eligible candidate pairs=0", message)
        self.assertIn("submitted attempts=0", message)
        self.assertIn('"revision":5833', message)
        self.assertEqual([probe.fires for probe in probes], [[], []])

    def test_observation_errors_are_preserved_before_and_at_overall_deadline(self):
        for failure_time in (0.2, 0.5):
            for detail in ("client timed out answering state request 2",
                           "client produced malformed test JSON",
                           "dedicated server exited with status 1"):
                with self.subTest(failure_time=failure_time, detail=detail):
                    message, probes = self.candidate_observation_failure(
                        [self.without_public_hostiles()], fire.MovementFailure(detail),
                        failure_time=failure_time)
                    self.assertIn("overall fire deadline exhausted=" +
                                  ("true" if failure_time == 0.5 else "false"), message)
                    self.assertEqual([probe.fires for probe in probes], [[], []])

    def test_first_probe_failure_does_not_claim_absence_of_public_targets(self):
        message, probes = self.candidate_observation_failure(
            [], fire.MovementFailure("client timed out answering state request 1"))
        self.assertIn("last completed public target counts=null", message)
        self.assertIn("last eligible candidate pairs=null", message)
        self.assertIn("last fire observations=[]", message)
        self.assertNotIn("last completed public target counts=[0, 0]", message)
        self.assertEqual([probe.fires for probe in probes], [[], []])

    def test_not_ready_snapshot_does_not_claim_absence_of_public_targets(self):
        message, probes = self.candidate_observation_failure(
            [[{"ready": False}, {"ready": False}]],
            fire.MovementFailure("client timed out answering state request 2"))
        self.assertIn("last completed public target counts=null", message)
        self.assertIn("last eligible candidate pairs=null", message)
        self.assertEqual([probe.fires for probe in probes], [[], []])

    def test_present_target_without_available_shooter_is_not_reported_absent(self):
        observed = states()
        for snapshot in observed:
            snapshot["commands_blocked"] = True
        message, probes = self.candidate_observation_failure(
            [observed], fire.MovementFailure("client timed out answering state request 2"))
        self.assertIn("last completed public target counts=[1, 1]", message)
        self.assertIn("last eligible candidate pairs=0", message)
        self.assertEqual([probe.fires for probe in probes], [[], []])

    def test_probe_deadline_after_rejection_preserves_submission_and_refusal_counts(self):
        message, probes = self.candidate_observation_failure(
            [states(), self.without_public_hostiles()],
            fire.MovementFailure("client timed out answering state request 3"),
            fire_result=("rejected", None))
        self.assertIn("no accepted fire among 1 exhausted candidate pairs", message)
        self.assertIn("submitted attempts=1", message)
        self.assertIn('"reason": 12', message)
        self.assertIn("last completed public target counts=[0, 0]", message)
        self.assertEqual([probe.fires for probe in probes], [[("FIRE", 0, 1, 2, 3)], []])

    def test_candidate_wait_allows_native_enemy_exposure_before_shot(self):
        initial = states()
        for snapshot in initial:
            snapshot["actors"][2]["hostile"] = False
        probes = [FakeFireProbe(), FakeFireProbe()]
        with patch.object(fire, "_Probe", side_effect=probes), \
                patch.object(fire, "_guard", return_value=lambda: None), \
                patch.object(fire, "_states", side_effect=[initial, states()]), \
                patch.object(fire, "_wait_fire", return_value=self.completed()), \
                patch.object(fire.time, "sleep"), patch("builtins.print"):
            evidence = fire.certify_two_client_fire([object(), object()], object(), 1.0)
        self.assertEqual(evidence["submitted_attempts"], 1)


class NativeFireReadinessTests(unittest.TestCase):
    @staticmethod
    def evidence():
        post_ammo = states(ammo=5, ap=95)
        return {"session": 1, "world": 2,
                "shot": {"sender": 1, "command": 1, "shooter": [0, 1], "target": [2, 3],
                         "revisions_after": [11, 11],
                         "primary_after": primary(ammo=5), "ammo_spent": 1,
                         "actor": fire._projection(post_ammo[0]["actors"][0])},
                "submitted_attempts": 1, "refusals": []}

    def wait_ready(self, observed=None, *, sequence=None, timeout=1.5, evidence=None):
        snapshots = sequence if sequence is not None else [
            observed if observed is not None else states(ammo=5, ap=95)]
        clock = [0.0]
        probes = [FakeFireProbe(), FakeFireProbe()]
        self.observation_times = []

        def next_states(*_arguments):
            index = min(len(self.observation_times), len(snapshots) - 1)
            self.observation_times.append(clock[0])
            sample = snapshots[index]
            if isinstance(sample, Exception):
                raise sample
            return copy.deepcopy(sample)

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(fire, "_Probe", side_effect=probes), \
                patch.object(fire, "_guard", return_value=lambda: None), \
                patch.object(fire, "_states", side_effect=next_states), \
                patch.object(fire, "_wait_fire") as repeat_wait, \
                patch.object(fire, "certify_two_client_fire") as repeat_shot, \
                patch.object(fire.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(fire.time, "sleep", side_effect=sleep), patch("builtins.print"):
            try:
                return fire.wait_for_native_fire_readiness(
                    [object(), object()], object(), evidence or self.evidence(), timeout)
            finally:
                self.elapsed = clock[0]
                self.assertEqual([probe.fires for probe in probes], [[], []])
                repeat_wait.assert_not_called()
                repeat_shot.assert_not_called()

    def test_current_post_ammo_readiness_requires_half_second_fresh_observation(self):
        result = self.wait_ready()
        self.assertIsInstance(result, dict)
        self.assertGreaterEqual(self.elapsed, fire.SETTLED_SECONDS)
        self.assertGreater(len(self.observation_times), 1)

    def test_permanent_native_busy_on_either_peer_times_out_without_repeat_shot(self):
        for peer in (0, 1):
            for field, value in (("commands_blocked", True), ("outstanding", 1)):
                with self.subTest(peer=peer, field=field):
                    observed = states(ammo=5, ap=95)
                    observed[peer][field] = value
                    with self.assertRaises(fire.FireFailure):
                        self.wait_ready(observed, timeout=0.9)
                    self.assertGreaterEqual(self.elapsed, 0.9)

    def test_delayed_native_unlock_starts_stability_after_unlock(self):
        busy = states(ammo=5, ap=95)
        for snapshot in busy:
            snapshot["commands_blocked"] = True
        self.wait_ready(sequence=[busy] * 3 + [states(ammo=5, ap=95)])
        self.assertGreaterEqual(self.elapsed, 0.3 + fire.SETTLED_SECONDS)

    def test_each_peer_must_reach_its_recorded_ammo_evidence_revision(self):
        evidence = self.evidence()
        evidence["shot"]["revisions_after"] = [11, 15]
        for peer in (0, 1):
            with self.subTest(peer=peer):
                observed = states(ammo=5, ap=95)
                observed[1]["revision"] = 15
                observed[peer]["revision"] -= 1
                with self.assertRaises(fire.FireFailure):
                    self.wait_ready(observed, evidence=evidence, timeout=0.9)
        observed = states(ammo=5, ap=95)
        observed[1]["revision"] = 15
        self.assertIsInstance(self.wait_ready(observed, evidence=evidence), dict)

    def test_stale_pre_shot_replicas_do_not_count_toward_stability(self):
        stale = states(revision=10)
        self.wait_ready(sequence=[stale] * 3 + [states(ammo=5, ap=95)])
        self.assertGreaterEqual(self.elapsed, 0.3 + fire.SETTLED_SECONDS)

    def test_peer_phase_disagreement_and_resolving_interrupt_never_certify(self):
        for field, value in (("turn_serial", 2), ("team", 1), ("turn_based", False),
                             ("in_combat", False), ("interrupt_phase", 2),
                             ("interrupt_serial", 9)):
            with self.subTest(field=field):
                observed = states(ammo=5, ap=95)
                observed[1][field] = value
                with self.assertRaises(fire.FireFailure):
                    self.wait_ready(observed, timeout=0.9)
        resolving = states(ammo=5, ap=95)
        for snapshot in resolving:
            snapshot.update(interrupt_phase=1, interrupt_serial=9)
        with self.assertRaises(fire.FireFailure):
            self.wait_ready(resolving, timeout=0.9)

    def test_current_shooter_and_target_must_converge_on_both_replicas(self):
        for identity in (0, 2):
            for field, value in (("life", 89), ("ap", 94), ("grid", 900),
                                 ("world_x", 8000), ("animation", 17)):
                with self.subTest(actor=identity, field=field):
                    observed = states(ammo=5, ap=95)
                    observed[1]["actors"][identity][field] = value
                    with self.assertRaises(fire.FireFailure):
                        self.wait_ready(observed, timeout=0.9)
        observed = states(ammo=5, ap=95)
        observed[1]["actors"][0]["primary"]["ammo_count"] = 4
        with self.assertRaises(fire.FireFailure):
            self.wait_ready(observed, timeout=0.9)

    def test_target_can_die_or_disappear_after_the_certified_shot(self):
        for absent in (False, True):
            with self.subTest(absent=absent):
                observed = states(ammo=5, ap=95)
                for snapshot in observed:
                    if absent:
                        snapshot["actors"] = snapshot["actors"][:2]
                    else:
                        snapshot["actors"][2].update(life=0, active=False)
                self.assertIsInstance(self.wait_ready(observed), dict)

    def test_target_disappearance_on_only_one_replica_does_not_certify(self):
        observed = states(ammo=5, ap=95)
        observed[1]["actors"] = observed[1]["actors"][:2]
        with self.assertRaises(fire.FireFailure):
            self.wait_ready(observed, timeout=0.9)

    def test_shooter_must_remain_present_but_need_not_keep_historical_life_or_ap(self):
        missing = states(ammo=5, ap=95)
        for snapshot in missing:
            snapshot["actors"] = snapshot["actors"][1:]
        with self.assertRaises(fire.FireFailure):
            self.wait_ready(missing, timeout=0.9)
        dead = states(ammo=5, ap=0)
        for snapshot in dead:
            snapshot["actors"][0].update(life=0, active=False)
        self.assertIsInstance(self.wait_ready(dead), dict)

    def test_readiness_does_not_require_original_receipt_to_remain_latest(self):
        for receipt in (None, {"command": 7, "status": 3, "reason": 0, "revision": 11}):
            with self.subTest(receipt=receipt):
                observed = states(ammo=5, ap=95)
                for snapshot in observed:
                    snapshot["receipt"] = receipt
                self.assertIsInstance(self.wait_ready(observed), dict)

    def test_matching_actionable_interrupt_is_not_confused_with_resolving(self):
        observed = states(ammo=5, ap=95)
        for snapshot in observed:
            snapshot.update(interrupt_phase=2, interrupt_serial=9)
            snapshot["actors"][0]["interrupt_eligible"] = True
        self.assertIsInstance(self.wait_ready(observed), dict)

    def test_world_or_session_change_is_terminal(self):
        for field, value in (("world", 3), ("session", 2)):
            with self.subTest(field=field):
                changed = states(ammo=5, ap=95)
                changed[1][field] = value
                with self.assertRaises(fire.FireFailure):
                    self.wait_ready(sequence=[states(ammo=5, ap=95), changed])

    def test_readiness_loss_disconnect_or_resync_restarts_stability(self):
        for field, value in (("ready", False), ("resynchronizing", True),
                             ("commands_blocked", True), ("outstanding", 1),
                             ("revision", 10)):
            for peer in (0, 1):
                with self.subTest(field=field, peer=peer):
                    ready = states(ammo=5, ap=95)
                    lost = copy.deepcopy(ready)
                    lost[peer][field] = value
                    self.wait_ready(sequence=[ready, ready, ready, lost, ready])
                    self.assertGreaterEqual(self.elapsed, 0.4 + fire.SETTLED_SECONDS)

    def test_new_agreed_actor_state_or_phase_restarts_stability(self):
        for change in ("shooter", "target", "phase"):
            with self.subTest(change=change):
                ready = states(ammo=5, ap=95)
                changed = copy.deepcopy(ready)
                for snapshot in changed:
                    if change == "phase":
                        snapshot["turn_serial"] = 2
                    else:
                        snapshot["actors"][0 if change == "shooter" else 2]["life"] -= 1
                self.wait_ready(sequence=[ready] * 3 + [changed])
                self.assertGreaterEqual(self.elapsed, 0.3 + fire.SETTLED_SECONDS)

    def test_transport_failure_is_terminal_without_reissuing_fire(self):
        with self.assertRaises(fire.FireFailure):
            self.wait_ready(sequence=[states(ammo=5, ap=95),
                                      fire.MovementFailure("client disconnected")])


if __name__ == "__main__":
    unittest.main()
