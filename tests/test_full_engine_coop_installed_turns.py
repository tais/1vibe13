"""Build-free native turn-cycle evidence, interruption, and no-replay checks."""

from __future__ import annotations

import copy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_turns as turns
from test_full_engine_coop_installed_fire import states as fire_states


def states(*, turn=1, team=0, phase=0, grant=0, revision=10,
           command=None, status=3, reason=0, sender=0, outstanding=0):
    observed = fire_states(revision=revision)
    for snapshot in observed:
        snapshot.update(turn_serial=turn, team=team, interrupt_phase=phase,
                        interrupt_serial=grant, receipt=None, outstanding=0)
        for actor in snapshot["actors"]:
            actor.update(bleeding=0, hand_medical_kit_points=0,
                         medical_service={"providing": False, "patient_slot": 65535,
                                          "patient_inc": 0, "receiving_count": 0})
    if command is not None:
        observed[sender]["receipt"] = {
            "command": command, "status": status, "reason": reason, "revision": revision}
    observed[sender]["outstanding"] = outstanding
    return observed


def grant_states(*, eligible=(0, 1), **kwargs):
    observed = states(phase=2, grant=7, **kwargs)
    for snapshot in observed:
        for actor in snapshot["actors"]:
            actor["interrupt_eligible"] = actor["slot"] in eligible
    return observed


class FakeTurnProbe:
    def __init__(self, replies=None):
        self.commands = []
        self.replies = replies

    def send(self, verb, *arguments):
        self.commands.append((verb, *arguments))
        return len(self.commands)

    def receive(self, request, kind, deadline, guard):
        if self.replies is not None:
            return copy.deepcopy(self.replies[min(request - 1, len(self.replies) - 1)])
        return {"submitted": True, "result": 0, "command": request}


class _TurnHarness(unittest.TestCase):
    def certify(self, sequence=None, *, timeout=3.0, probes=None, readiness=False):
        sequence = sequence if sequence is not None else [
            states(), states(turn=3, revision=12, command=1)]
        self.probes = probes if probes is not None else [FakeTurnProbe(), FakeTurnProbe()]
        clock = [0.0]
        self.observations = []

        def observe(*_args):
            index = min(len(self.observations), len(sequence) - 1)
            self.observations.append(clock[0])
            sample = sequence[index]
            if isinstance(sample, Exception):
                raise sample
            return copy.deepcopy(sample)

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(turns, "_Probe", side_effect=self.probes), \
                patch.object(turns, "_guard", return_value=lambda: None), \
                patch.object(turns, "_states", side_effect=observe), \
                patch.object(turns.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(turns.time, "sleep", side_effect=sleep), patch("builtins.print"):
            try:
                certify = (turns.wait_for_two_client_player_control if readiness
                           else turns.certify_two_client_turn_cycle)
                return certify([object(), object()], object(), timeout)
            finally:
                self.elapsed = clock[0]


class TurnCycleTests(_TurnHarness):
    def test_one_shared_endturn_proves_cycle_without_sampling_intermediate_ai(self):
        evidence = self.certify()
        self.assertEqual(self.probes[0].commands, [("ENDTURN", 0, 1)])
        self.assertEqual(self.probes[1].commands, [])
        self.assertEqual(evidence["submitted_attempts"], 1)
        self.assertEqual(evidence["sampled_nonplayer_teams"], [])
        self.assertEqual(evidence["initial"]["turn_serial"], 1)
        self.assertEqual(evidence["final"]["phase"]["turn_serial"], 3)
        self.assertGreaterEqual(evidence["final"]["stable_seconds"], 0.5)

    def test_sampled_ai_turn_is_diagnostic_not_an_additional_client_command(self):
        evidence = self.certify([
            states(), states(turn=2, team=1, revision=11, command=1),
            states(turn=2, team=1, revision=12, command=1),
            states(turn=3, revision=13, command=1)])
        self.assertEqual(evidence["sampled_nonplayer_teams"], [1])
        self.assertEqual(evidence["submitted_attempts"], 1)

    def test_s_plus_one_or_same_turn_cannot_prove_full_cycle(self):
        for serial in (1, 2):
            with self.subTest(serial=serial), self.assertRaises(turns.TurnCycleFailure):
                self.certify([states(), states(turn=serial, revision=12, command=1)], timeout=1.0)
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_later_multiple_team_turns_are_valid_without_exact_serial_increment(self):
        evidence = self.certify([states(), states(turn=7, revision=19, command=1)])
        self.assertEqual(evidence["final"]["phase"]["turn_serial"], 7)

    def test_applied_is_required_even_when_final_turn_already_looks_correct(self):
        for status, command in ((1, 1), (3, None), (3, 0)):
            with self.subTest(status=status, command=command), self.assertRaises(turns.TurnCycleFailure):
                self.certify([states(), states(turn=3, revision=12, command=command,
                                              status=status)], timeout=0.8)
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_replaced_unobserved_receipt_is_ambiguous_and_never_retried(self):
        with self.assertRaisesRegex(turns.TurnCycleFailure, "replaced before"):
            self.certify([states(), states(turn=3, revision=12, command=2)])
        self.assertEqual(len(self.probes[0].commands), 1)

    def test_controller_refusal_or_ambiguous_submission_never_retries(self):
        for reply in ({"submitted": False, "result": "controller rejected", "command": 0},
                      {"submitted": True, "result": 0, "command": 0},
                      {"submitted": True, "result": 0, "command": True},
                      {"submitted": True, "result": 4, "command": 1}):
            with self.subTest(reply=reply), self.assertRaises(turns.TurnCycleFailure):
                self.certify(probes=[FakeTurnProbe([reply]), FakeTurnProbe()])
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_gameplay_or_unknown_terminal_rejection_never_retries(self):
        for reason in (8, 12, 15):
            with self.subTest(reason=reason), self.assertRaisesRegex(turns.TurnCycleFailure, "terminated"):
                self.certify([states(), states(revision=11, command=1, status=2, reason=reason)])
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_explicit_stale_rejection_allows_only_fresh_command_retry(self):
        for reason in (5, 9):
            with self.subTest(reason=reason):
                stale = states(revision=11, command=1, status=2, reason=reason)
                evidence = self.certify([
                    states(), *([stale] * 5), states(turn=3, revision=12, command=2)])
                self.assertEqual(evidence["endturn"]["attempts"], 2)
                self.assertEqual(self.probes[0].commands, [("ENDTURN", 0, 1)] * 2)
                self.assertEqual(self.probes[1].commands, [])

    def test_phase_or_actor_change_with_stale_rejection_forbids_retry(self):
        for problem in ("turn", "team", "interrupt", "life", "ap", "ammo", "pose",
                        "bleeding", "hand_medical_kit_points", "medical_service"):
            stale = states(revision=11, command=1, status=2, reason=5)
            for snapshot in stale:
                if problem == "turn":
                    snapshot["turn_serial"] = 2
                elif problem == "team":
                    snapshot["team"] = 1
                elif problem == "interrupt":
                    snapshot.update(interrupt_phase=2, interrupt_serial=7)
                elif problem == "ammo":
                    snapshot["actors"][0]["primary"]["ammo_count"] -= 1
                elif problem == "pose":
                    snapshot["actors"][0]["world_x"] += 1
                elif problem in ("bleeding", "hand_medical_kit_points"):
                    snapshot["actors"][0][problem] += 1
                elif problem == "medical_service":
                    snapshot["actors"][0][problem]["providing"] = True
                else:
                    snapshot["actors"][0][problem] -= 1
            with self.subTest(problem=problem), self.assertRaisesRegex(turns.TurnCycleFailure, "no retry"):
                self.certify([states(), stale])
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_transient_changed_then_restored_phase_does_not_make_stale_retry_safe(self):
        with self.assertRaisesRegex(turns.TurnCycleFailure, "no retry"):
            self.certify([states(), states(turn=2, team=1, revision=11, command=1, status=1),
                          states(revision=12, command=1, status=2, reason=9)])
        self.assertEqual(len(self.probes[0].commands), 1)

    def test_stale_rejection_keeps_actor_and_world_bound_during_fresh_wait(self):
        stale = states(revision=11, command=1, status=2, reason=9)
        with self.assertRaisesRegex(turns.TurnCycleFailure, "phase advanced before"):
            self.certify([states(), *([stale] * 4), states(turn=2, team=1, revision=12)])
        self.assertEqual(len(self.probes[0].commands), 1)

    def test_reused_native_command_id_is_rejected_before_waiting_again(self):
        stale = states(revision=11, command=1, status=2, reason=9)
        repeated = {"submitted": True, "result": 0, "command": 1}
        with self.assertRaisesRegex(turns.TurnCycleFailure, "fresh command identity"):
            self.certify([states(), stale],
                         probes=[FakeTurnProbe([repeated]), FakeTurnProbe()])
        self.assertEqual(len(self.probes[0].commands), 2)

    def test_bounded_stale_attempts_never_submit_an_extra_command(self):
        with patch.object(turns._TurnRun, "wait_command", return_value=(
                "stale", states(), {"command": 1, "status": 2, "reason": 5, "revision": 11})), \
                self.assertRaisesRegex(turns.TurnCycleFailure, "bounded fresh-snapshot"):
            self.certify([states()], timeout=3.0)
        self.assertEqual(len(self.probes[0].commands), 8)

    def test_applied_then_rejected_before_unlock_is_terminal(self):
        with self.assertRaisesRegex(turns.TurnCycleFailure, "rejected after Applied"):
            self.certify([states(), states(revision=11, command=1, outstanding=1),
                          states(revision=12, command=1, status=2, reason=9)])
        self.assertEqual(len(self.probes[0].commands), 1)

    def test_observed_applied_can_be_retained_while_waiting_for_peer_unlock(self):
        evidence = self.certify([
            states(), states(turn=2, team=1, revision=11, command=1, outstanding=1),
            states(turn=3, revision=12)])
        self.assertEqual(evidence["endturn"]["receipt"]["command"], 1)
        self.assertEqual(evidence["submitted_attempts"], 1)

    def test_health_ap_equipment_medical_state_and_actor_deaths_can_legitimately_change(self):
        final = states(turn=3, revision=12, command=1)
        for snapshot in final:
            snapshot["actors"][0].update(life=20, ap=2, breath=25, stance=3, bleeding=70)
            snapshot["actors"][0]["primary"]["condition"] = 40
            snapshot["actors"][1].update(life=0, active=False, ap=0)
            snapshot["actors"] = snapshot["actors"][:2]
        evidence = self.certify([states(), final])
        self.assertEqual(evidence["final"]["actors"][0]["ap"], 2)
        self.assertEqual(evidence["final"]["actors"][0]["bleeding"], 70)
        self.assertEqual(evidence["final"]["actors"][0]["hand_medical_kit_points"], 0)
        self.assertEqual(evidence["final"]["actors"][1]["life"], 0)

    def test_positive_ap_is_not_a_prerequisite_for_ending_shared_team_turn(self):
        initial = states()
        for snapshot in initial:
            snapshot["actors"][0]["ap"] = 0
            snapshot["actors"][1]["ap"] = 0
        evidence = self.certify([initial, states(turn=3, revision=12, command=1)])
        self.assertEqual(evidence["submitted_attempts"], 1)

    def test_final_replicas_must_agree_on_current_phase_and_actor_state(self):
        for problem in ("turn", "team", "grant", "life", "ammo", "membership", "animation",
                        "bleeding", "hand_medical_kit_points"):
            final = states(turn=3, revision=12, command=1)
            if problem == "turn":
                final[1]["turn_serial"] = 4
            elif problem == "team":
                final[1]["team"] = 1
            elif problem == "grant":
                final[1]["interrupt_serial"] = 7
            elif problem == "ammo":
                final[1]["actors"][0]["primary"]["ammo_count"] -= 1
            elif problem == "membership":
                final[1]["actors"].pop()
            else:
                final[1]["actors"][0][problem] += 1
            with self.subTest(problem=problem), self.assertRaises(turns.TurnCycleFailure):
                self.certify([states(), final], timeout=0.9)
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_final_requires_both_revision_advancement_and_released_locks(self):
        for field, value in (("revision", 10), ("outstanding", 1), ("commands_blocked", True)):
            for peer in (0, 1):
                final = states(turn=3, revision=12, command=1)
                final[peer][field] = value
                with self.subTest(field=field, peer=peer), self.assertRaises(turns.TurnCycleFailure):
                    self.certify([states(), final], timeout=0.9)

    def test_medical_integer_boundaries_are_retained_in_final_evidence(self):
        for bleeding, kit_points in ((0, 0), (127, 65535), (23, 412)):
            final = states(turn=3, revision=12, command=1)
            for snapshot in final:
                snapshot["actors"][0].update(
                    bleeding=bleeding, hand_medical_kit_points=kit_points)
            with self.subTest(bleeding=bleeding, kit_points=kit_points):
                evidence = self.certify([states(), final])
                actor = evidence["final"]["actors"][0]
                self.assertEqual(actor["bleeding"], bleeding)
                self.assertEqual(actor["hand_medical_kit_points"], kit_points)

    def test_missing_or_malformed_medical_fields_never_certify_or_replay(self):
        missing = object()
        for field, maximum in (("bleeding", 127), ("hand_medical_kit_points", 65535)):
            for value in (missing, None, False, True, -1, maximum + 1, 0.0, "0", [], {}):
                for peer in (0, 1):
                    for stage in ("initial", "final"):
                        malformed = states() if stage == "initial" else states(
                            turn=3, revision=12, command=1)
                        # Validate every actor, including an unowned enemy.
                        actor = malformed[peer]["actors"][2]
                        if value is missing:
                            del actor[field]
                        else:
                            actor[field] = value
                        sequence = [malformed] if stage == "initial" else [states(), malformed]
                        with self.subTest(field=field, value=value, peer=peer, stage=stage), \
                                self.assertRaisesRegex(turns.TurnCycleFailure, field):
                            self.certify(sequence)
                        self.assertEqual(len(self.probes[0].commands),
                                         0 if stage == "initial" else 1)
                        self.assertEqual(self.probes[1].commands, [])

    def test_peer_revision_must_cover_applied_receipt_not_just_initial_revision(self):
        final = states(turn=3, revision=12, command=1)
        final[1]["revision"] = 11
        with self.assertRaises(turns.TurnCycleFailure):
            self.certify([states(), final], timeout=0.9)
        self.assertEqual(len(self.probes[0].commands), 1)

    def test_active_medical_service_is_retained_without_inventing_completion(self):
        services = (
            {"providing": False, "patient_slot": 65535, "patient_inc": 0,
             "receiving_count": 255},
            {"providing": True, "patient_slot": 65535, "patient_inc": 0,
             "receiving_count": 0},
            {"providing": True, "patient_slot": 0, "patient_inc": 1,
             "receiving_count": 1},
            {"providing": True, "patient_slot": 65534, "patient_inc": 4294967295,
             "receiving_count": 255},
        )
        for service in services:
            final = states(turn=3, revision=12, command=1)
            for snapshot in final:
                snapshot["actors"][0]["medical_service"] = dict(service)
            with self.subTest(service=service):
                evidence = self.certify([states(), final])
                actor = evidence["final"]["actors"][0]
                self.assertEqual(actor["bleeding"], 0)
                self.assertEqual(actor["hand_medical_kit_points"], 0)
                self.assertEqual(actor["medical_service"], service)
                self.assertNotIn("treatment_completed", actor)
                self.assertEqual(self.probes[0].commands, [("ENDTURN", 0, 1)])

    def test_medical_service_projection_owns_a_copy_not_a_mutable_input_alias(self):
        observed = states()[0]
        projected = turns._roster(observed)
        observed["actors"][0]["medical_service"]["receiving_count"] = 2
        self.assertEqual(projected[0]["medical_service"]["receiving_count"], 0)

    def test_missing_or_malformed_service_descriptors_never_certify_or_replay(self):
        missing = object()
        for value in (missing, None, False, True, 0, "none", [], {}):
            for peer in (0, 1):
                for stage in ("initial", "final"):
                    malformed = states() if stage == "initial" else states(
                        turn=3, revision=12, command=1)
                    actor = malformed[peer]["actors"][2]
                    if value is missing:
                        del actor["medical_service"]
                    else:
                        actor["medical_service"] = value
                    sequence = [malformed] if stage == "initial" else [states(), malformed]
                    with self.subTest(value=value, peer=peer, stage=stage), \
                            self.assertRaisesRegex(turns.TurnCycleFailure, "medical_service"):
                        self.certify(sequence)
                    self.assertEqual(len(self.probes[0].commands), 0 if stage == "initial" else 1)
                    self.assertEqual(self.probes[1].commands, [])

    def test_service_fields_require_exact_boolean_and_bounded_integer_types(self):
        missing = object()
        invalid = {
            "providing": (missing, None, 0, 1, 0.0, "false", [], {}),
            "patient_slot": (missing, None, False, True, -1, 65536, 0.0, "0", [], {}),
            "patient_inc": (missing, None, False, True, -1, 4294967296, 0.0, "0", [], {}),
            "receiving_count": (missing, None, False, True, -1, 256, 0.0, "0", [], {}),
        }
        for field, values in invalid.items():
            for value in values:
                for stage in ("initial", "final"):
                    malformed = states() if stage == "initial" else states(
                        turn=3, revision=12, command=1)
                    service = malformed[1]["actors"][2]["medical_service"]
                    if value is missing:
                        del service[field]
                    else:
                        service[field] = value
                    sequence = [malformed] if stage == "initial" else [states(), malformed]
                    with self.subTest(field=field, value=value, stage=stage), \
                            self.assertRaisesRegex(turns.TurnCycleFailure, "medical_service"):
                        self.certify(sequence)
                    self.assertEqual(len(self.probes[0].commands), 0 if stage == "initial" else 1)
                    self.assertEqual(self.probes[1].commands, [])

    def test_noncanonical_patient_identities_and_unlinked_patient_are_rejected(self):
        for providing, slot, incarnation in ((True, 0, 0), (False, 0, 0),
                                             (True, 65535, 1), (False, 65535, 1),
                                             (False, 0, 1), (False, 65534, 4294967295)):
            malformed = states()
            malformed[1]["actors"][2]["medical_service"].update(
                providing=providing, patient_slot=slot, patient_inc=incarnation)
            with self.subTest(providing=providing, slot=slot, incarnation=incarnation), \
                    self.assertRaisesRegex(turns.TurnCycleFailure, "noncanonical.*medical_service"):
                self.certify([malformed])
            self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_medical_service_disagreement_between_clients_cannot_certify(self):
        for field, value in (("providing", False), ("patient_slot", 5),
                             ("patient_inc", 9), ("receiving_count", 1)):
            final = states(turn=3, revision=12, command=1)
            for snapshot in final:
                snapshot["actors"][0]["medical_service"].update(
                    providing=True, patient_slot=1, patient_inc=2)
            changed = final[1]["actors"][0]["medical_service"]
            changed[field] = value
            if field == "providing":
                changed.update(patient_slot=65535, patient_inc=0)
            with self.subTest(field=field), self.assertRaises(turns.TurnCycleFailure):
                self.certify([states(), final], timeout=0.9)
            self.assertEqual(len(self.probes[0].commands), 1)
            self.assertEqual(self.probes[1].commands, [])

    def test_additional_public_fields_do_not_break_known_projection(self):
        final = states(turn=3, revision=12, command=1)
        for snapshot in final:
            snapshot["new_public_metadata"] = {"version": 2}
            for actor in snapshot["actors"]:
                actor["new_actor_metadata"] = "compatible extension"
                actor["primary"]["new_item_metadata"] = "compatible extension"
        self.assertEqual(self.certify([states(), final])["submitted_attempts"], 1)

    def test_current_actor_changes_and_readiness_loss_restart_final_stability(self):
        final = states(turn=3, revision=12, command=1)
        for problem in ("life", "bleeding", "hand_medical_kit_points",
                        "medical_service", "not_ready", "resync", "busy"):
            changed = copy.deepcopy(final)
            for snapshot in changed:
                if problem == "life":
                    snapshot["actors"][0]["life"] -= 1
                elif problem in ("bleeding", "hand_medical_kit_points"):
                    snapshot["actors"][0][problem] += 1
                elif problem == "medical_service":
                    snapshot["actors"][0][problem]["receiving_count"] = 1
                elif problem == "not_ready":
                    snapshot["ready"] = False
                elif problem == "resync":
                    snapshot["resynchronizing"] = True
                else:
                    snapshot["commands_blocked"] = True
            with self.subTest(problem=problem):
                self.certify([states(), final, final, final, changed, final])
                self.assertGreaterEqual(self.elapsed, 0.4 + turns.SETTLED_SECONDS)

    def test_existing_interrupt_requires_distinct_passes_from_both_owners_before_endturn(self):
        active = grant_states()
        after_a = grant_states(eligible=(1,), revision=11, command=1)
        after_b = states(turn=3, grant=7, revision=12, command=1, sender=1)
        final = states(turn=5, grant=7, revision=13, command=2)
        evidence = self.certify([active, after_a, after_a, after_b, after_b, final])
        self.assertEqual(self.probes[0].commands,
                         [("PASS_INTERRUPT", 0, 1, 7), ("ENDTURN", 0, 1)])
        self.assertEqual(self.probes[1].commands, [("PASS_INTERRUPT", 1, 2, 7)])
        self.assertEqual(evidence["initial"]["turn_serial"], 3)
        self.assertEqual(len(evidence["interrupt_passes"]), 2)

    def test_endturn_receipt_is_captured_before_passing_a_new_interrupt(self):
        active_queued = grant_states(eligible=(0,), turn=2, revision=11,
                                     command=1, status=1, outstanding=1)
        active_applied = grant_states(eligible=(0,), turn=2, revision=12, command=1)
        after_pass = states(turn=3, grant=7, revision=13, command=2)
        evidence = self.certify([states(), active_queued, active_applied,
                                 active_applied, after_pass])
        self.assertEqual(self.probes[0].commands,
                         [("ENDTURN", 0, 1), ("PASS_INTERRUPT", 0, 1, 7)])
        self.assertEqual(evidence["endturn"]["command"], 1)
        self.assertEqual(evidence["interrupt_passes"][0]["command"], 2)

    def test_applied_pass_is_never_repeated_if_eligibility_stays_visible(self):
        active = grant_states(eligible=(0,))
        unchanged_eligibility = grant_states(eligible=(0,), revision=11, command=1)
        with self.assertRaises(turns.TurnCycleFailure):
            self.certify([active, unchanged_eligibility], timeout=0.9)
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])
        self.assertEqual(self.probes[1].commands, [])

    def test_resolving_interrupt_waits_without_submitting_a_pass(self):
        resolving = states(phase=1, grant=7)
        for snapshot in resolving:
            snapshot["commands_blocked"] = True
            snapshot["actors"][0]["interrupt_eligible"] = True
        with self.assertRaises(turns.TurnCycleFailure):
            self.certify([resolving], timeout=0.6)
        self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_unowned_dead_or_ineligible_actors_are_not_pass_candidates(self):
        for problem in ("unowned", "dead", "ineligible"):
            active = grant_states(eligible=(0,))
            for snapshot in active:
                if problem == "unowned":
                    snapshot["actors"][0]["interrupt_eligible"] = False
                    snapshot["actors"][2].update(team=0, hostile=False, interrupt_eligible=True)
                elif problem == "dead":
                    snapshot["actors"][0]["life"] = 0
                else:
                    snapshot["actors"][0]["interrupt_eligible"] = False
            with self.subTest(problem=problem), self.assertRaises(turns.TurnCycleFailure):
                self.certify([active], timeout=0.6)
            self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_enemy_active_interrupt_does_not_allow_player_pass(self):
        active = grant_states(team=1)
        with self.assertRaises(turns.TurnCycleFailure):
            self.certify([active], timeout=0.6)
        self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_stale_pass_that_advanced_its_grant_is_never_retried(self):
        active = grant_states(eligible=(0,))
        rejected = grant_states(eligible=(0,), revision=11, command=1, status=2, reason=9)
        for snapshot in rejected:
            snapshot["interrupt_serial"] = 8
        with self.assertRaisesRegex(turns.TurnCycleFailure, "no retry"):
            self.certify([active, rejected])
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])

    def test_explicit_side_effect_free_stale_pass_retries_only_that_exact_grant(self):
        for reason in (5, 9):
            with self.subTest(reason=reason):
                active = grant_states(eligible=(0,))
                stale = grant_states(eligible=(0,), revision=11, command=1,
                                     status=2, reason=reason)
                passed = states(turn=3, grant=7, revision=12, command=2)
                final = states(turn=5, grant=7, revision=13, command=3)
                evidence = self.certify([active, *([stale] * 5), passed, passed, final])
                self.assertEqual(self.probes[0].commands, [
                    ("PASS_INTERRUPT", 0, 1, 7), ("PASS_INTERRUPT", 0, 1, 7),
                    ("ENDTURN", 0, 1)])
                self.assertEqual(evidence["interrupt_passes"][0]["attempts"], 2)
                self.assertEqual(evidence["submitted_attempts"], 3)

    def test_same_actor_can_pass_a_new_grant_without_replaying_old_grant(self):
        active = grant_states(eligible=(0,))
        passed = grant_states(eligible=(0,), revision=11, command=1)
        next_grant = grant_states(eligible=(0,), revision=12, command=1)
        for snapshot in next_grant:
            snapshot["interrupt_serial"] = 8
        ordinary = states(turn=3, grant=8, revision=13, command=2)
        final = states(turn=5, grant=8, revision=14, command=3)
        evidence = self.certify([active, passed, next_grant, ordinary, ordinary, final])
        self.assertEqual(self.probes[0].commands, [
            ("PASS_INTERRUPT", 0, 1, 7), ("PASS_INTERRUPT", 0, 1, 8),
            ("ENDTURN", 0, 1)])
        self.assertEqual([item["interrupt_serial"] for item in evidence["interrupt_passes"]], [7, 8])

    def test_pass_receipt_loss_never_triggers_another_pass_or_endturn(self):
        with self.assertRaisesRegex(turns.TurnCycleFailure, "no replay"):
            self.certify([grant_states(eligible=(0,)), states(turn=3, revision=12)], timeout=0.8)
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])

    def test_interrupt_grants_and_total_submissions_are_bounded(self):
        active = grant_states(eligible=(0,))
        with patch.object(turns, "MAX_INTERRUPT_GRANTS", 0), \
                self.assertRaisesRegex(turns.TurnCycleFailure, "interrupt-grant bound"):
            self.certify([active])
        self.assertEqual([probe.commands for probe in self.probes], [[], []])
        with patch.object(turns, "MAX_SUBMITTED_COMMANDS", 0), \
                self.assertRaisesRegex(turns.TurnCycleFailure, "submitted-command bound"):
            self.certify()
        self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_new_grant_beyond_limit_fails_without_submitting_another_pass(self):
        active = grant_states(eligible=(0,))
        passed = grant_states(eligible=(0,), revision=11, command=1)
        next_grant = grant_states(eligible=(0,), revision=12, command=1)
        for snapshot in next_grant:
            snapshot["interrupt_serial"] = 8
        with patch.object(turns, "MAX_INTERRUPT_GRANTS", 1), \
                self.assertRaisesRegex(turns.TurnCycleFailure, "interrupt-grant bound"):
            self.certify([active, passed, next_grant])
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])

    def test_world_change_combat_end_and_transport_loss_do_not_certify_cycle(self):
        for problem in ("world", "session", "combat", "turn_based", "transport"):
            final = states(turn=3, revision=12, command=1)
            if problem == "transport":
                final = turns.MovementFailure("transport disconnected")
            else:
                for snapshot in final:
                    if problem in ("world", "session"):
                        snapshot[problem] += 1
                    elif problem == "combat":
                        snapshot["in_combat"] = False
                    else:
                        snapshot["turn_based"] = False
            with self.subTest(problem=problem), self.assertRaises(turns.TurnCycleFailure):
                self.certify([states(), final])
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_initial_ownership_world_and_shared_team_must_be_valid(self):
        for problem in ("overlap", "empty", "team", "world"):
            initial = states()
            if problem == "overlap":
                initial[1]["assigned"] = [[0, 1]]
            elif problem == "empty":
                initial[1]["assigned"] = []
            elif problem == "team":
                for snapshot in initial:
                    snapshot["actors"][1]["team"] = 2
            else:
                initial[1]["world"] += 1
            with self.subTest(problem=problem), self.assertRaises(turns.TurnCycleFailure):
                self.certify([initial])
            self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_invalid_timeout_and_client_count_fail_before_creating_probes(self):
        with patch.object(turns, "_Probe") as probe:
            for timeout in (0, -1, 601, float("nan"), float("inf"), True, "120"):
                with self.subTest(timeout=timeout), self.assertRaises(turns.TurnCycleFailure):
                    turns.certify_two_client_turn_cycle([object(), object()], object(), timeout)
            with self.assertRaises(turns.TurnCycleFailure):
                turns.certify_two_client_turn_cycle([object()], object())
            probe.assert_not_called()


class PlayerControlReadinessTests(_TurnHarness):
    def ready(self, sequence, **kwargs):
        return self.certify(sequence, readiness=True, **kwargs)

    def test_already_ordinary_or_realtime_never_submits_any_command(self):
        for mode in ("combat", "noncombat", "realtime"):
            initial = states()
            for snapshot in initial:
                if mode == "noncombat":
                    snapshot["in_combat"] = False
                elif mode == "realtime":
                    snapshot.update(turn_based=False, team=1)
            with self.subTest(mode=mode):
                evidence = self.ready([initial])
                self.assertEqual([probe.commands for probe in self.probes], [[], []])
                self.assertEqual(evidence["submitted_attempts"], 0)
                self.assertEqual(evidence["interrupt_passes"], [])
                self.assertNotIn("endturn", evidence)
                self.assertGreaterEqual(evidence["final"]["stable_seconds"], 0.5)

    def test_only_eligible_other_owner_passes_then_ordinary_inventory_actor_can_act(self):
        active = grant_states(eligible=(1,))
        ordinary = states(turn=3, grant=7, revision=12, command=1, sender=1)
        evidence = self.ready([active, ordinary])
        self.assertEqual(self.probes[0].commands, [])
        self.assertEqual(self.probes[1].commands, [("PASS_INTERRUPT", 1, 2, 7)])
        self.assertEqual(evidence["interrupt_passes"][0]["sender"], 2)
        self.assertEqual(evidence["interrupt_passes"][0]["interrupt_serial"], 7)
        self.assertEqual(evidence["submitted_attempts"], 1)
        self.assertNotIn("endturn", evidence)

    def test_distinct_owners_pass_same_grant_once_each_without_ordinary_endturn(self):
        after_a = grant_states(eligible=(1,), revision=11, command=1)
        ordinary = states(turn=3, grant=7, revision=12, command=1, sender=1)
        evidence = self.ready([grant_states(), after_a, after_a, ordinary])
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])
        self.assertEqual(self.probes[1].commands, [("PASS_INTERRUPT", 1, 2, 7)])
        self.assertEqual(len(evidence["interrupt_passes"]), 2)

    def test_applied_pass_may_legitimately_end_combat_but_still_needs_exact_receipt(self):
        final = states(turn=3, grant=7, revision=12, command=1)
        for snapshot in final:
            snapshot["in_combat"] = False
        evidence = self.ready([grant_states(eligible=(0,)), final])
        self.assertFalse(evidence["final"]["phase"]["in_combat"])
        self.assertEqual(evidence["submitted_attempts"], 1)
        for snapshot in final:
            snapshot["receipt"] = None
        with self.assertRaisesRegex(turns.TurnCycleFailure, "no replay"):
            self.ready([grant_states(eligible=(0,)), final], timeout=0.8)
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])

    def test_waits_through_native_enemy_and_deferred_phases_without_commands(self):
        evidence = self.ready([states(turn=2, team=1), states(turn=2, phase=1, grant=7),
                               states(turn=3, revision=13)])
        self.assertEqual(evidence["submitted_attempts"], 0)
        self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_no_eligible_owned_actor_never_submits_or_invents_a_pass(self):
        for problem in ("none", "unowned", "dead", "enemy", "deferred", "invalid_combat"):
            active = grant_states(eligible=(0,))
            for snapshot in active:
                if problem == "none":
                    snapshot["actors"][0]["interrupt_eligible"] = False
                elif problem == "unowned":
                    snapshot["actors"][0]["interrupt_eligible"] = False
                    snapshot["actors"][2].update(team=0, hostile=False, interrupt_eligible=True)
                elif problem == "dead":
                    snapshot["actors"][0]["life"] = 0
                elif problem == "enemy":
                    snapshot["team"] = 1
                elif problem == "deferred":
                    snapshot["interrupt_phase"] = 1
                else:
                    snapshot["in_combat"] = False
            with self.subTest(problem=problem), self.assertRaises(turns.TurnCycleFailure):
                self.ready([active], timeout=0.8)
            self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_applied_grant_is_not_replayed_while_same_eligibility_stays_visible(self):
        active = grant_states(eligible=(0,))
        applied = grant_states(eligible=(0,), revision=11, command=1)
        with self.assertRaises(turns.TurnCycleFailure):
            self.ready([active, applied], timeout=0.8)
        self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])

    def test_queued_or_missing_receipt_does_not_retry_an_ambiguous_pass(self):
        for status, command in ((1, 1), (3, None)):
            with self.subTest(status=status, command=command), self.assertRaises(turns.TurnCycleFailure):
                self.ready([grant_states(eligible=(0,)), states(turn=3, revision=12,
                            command=command, status=status)], timeout=0.8)
            self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)])

    def test_controller_or_gameplay_rejection_never_retries(self):
        with self.assertRaisesRegex(turns.TurnCycleFailure, "no speculative retry"):
            self.ready([grant_states(eligible=(0,))], probes=[
                FakeTurnProbe([{"submitted": False, "result": 1}]), FakeTurnProbe()])
        self.assertEqual(len(self.probes[0].commands), 1)
        with self.assertRaisesRegex(turns.TurnCycleFailure, "terminated"):
            self.ready([grant_states(eligible=(0,)), grant_states(eligible=(0,), revision=11,
                        command=1, status=2, reason=12)])
        self.assertEqual(len(self.probes[0].commands), 1)

    def test_stale_pass_reuses_exact_actor_and_serial_only_with_fresh_command_ids(self):
        for reason in (5, 9):
            stale = grant_states(eligible=(0,), revision=11, command=1, status=2, reason=reason)
            with self.subTest(reason=reason):
                evidence = self.ready([grant_states(eligible=(0,)), *([stale] * 5),
                                       states(turn=3, grant=7, revision=12, command=2)])
                self.assertEqual(self.probes[0].commands, [("PASS_INTERRUPT", 0, 1, 7)] * 2)
                self.assertEqual(evidence["interrupt_passes"][0]["attempts"], 2)
                self.assertEqual(evidence["submitted_attempts"], 2)

    def test_stale_side_effects_or_changed_serial_forbid_retry(self):
        for problem in ("ap", "serial", "ammo", "combat_end"):
            stale = grant_states(eligible=(0,), revision=11, command=1, status=2, reason=9)
            for snapshot in stale:
                if problem == "ap":
                    snapshot["actors"][0]["ap"] -= 1
                elif problem == "serial":
                    snapshot["interrupt_serial"] += 1
                elif problem == "ammo":
                    snapshot["actors"][0]["primary"]["ammo_count"] -= 1
                else:
                    snapshot["in_combat"] = False
            with self.subTest(problem=problem), self.assertRaisesRegex(turns.TurnCycleFailure, "no retry"):
                self.ready([grant_states(eligible=(0,)), stale])
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_fresh_state_and_all_command_gates_must_cover_applied_pass(self):
        for field, value in (("revision", 10), ("outstanding", 1), ("commands_blocked", True)):
            for peer in (0, 1):
                final = states(turn=3, revision=12, command=1)
                final[peer][field] = value
                with self.subTest(field=field, peer=peer), self.assertRaises(turns.TurnCycleFailure):
                    self.ready([grant_states(eligible=(0,)), final], timeout=0.8)
                self.assertEqual(len(self.probes[0].commands), 1)

    def test_world_change_or_transport_loss_fails_without_replay(self):
        for problem in ("world", "session", "transport"):
            final = states(turn=3, revision=12, command=1)
            if problem == "transport":
                final = turns.MovementFailure("lost connection")
            else:
                for snapshot in final:
                    snapshot[problem] += 1
            with self.subTest(problem=problem), self.assertRaises(turns.TurnCycleFailure):
                self.ready([grant_states(eligible=(0,)), final])
            self.assertEqual(len(self.probes[0].commands), 1)

    def test_grants_and_total_submissions_keep_existing_bounds(self):
        for constant in ("MAX_INTERRUPT_GRANTS", "MAX_SUBMITTED_COMMANDS"):
            with patch.object(turns, constant, 0), self.assertRaises(turns.TurnCycleFailure):
                self.ready([grant_states(eligible=(0,))])
            self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_invalid_arguments_fail_before_creating_probes(self):
        with patch.object(turns, "_Probe") as probe:
            for timeout in (0, -1, 601, float("nan"), float("inf"), True, "120"):
                with self.subTest(timeout=timeout), self.assertRaises(turns.TurnCycleFailure):
                    turns.wait_for_two_client_player_control([object(), object()], object(), timeout)
            with self.assertRaises(turns.TurnCycleFailure):
                turns.wait_for_two_client_player_control([object()], object())
            probe.assert_not_called()


if __name__ == "__main__":
    unittest.main()
