"""Data-free outcome and retry checks for the installed reload certificate."""

from __future__ import annotations

import copy
from contextlib import ExitStack
import unittest
from unittest.mock import patch

import full_engine_coop_installed_reload as reload
from test_full_engine_coop_installed_fire import primary, states
from test_full_engine_coop_installed_inventory import held_kit_states


def reload_states(*, ammo=8, ap=90, revision=12, status=3, reason=0):
    observed = states(ammo=ammo, ap=ap, revision=revision,
                      status=status, reason=reason)
    for state in observed:
        state["receipt"]["command"] = 2
    return observed


def initial_states():
    observed = reload_states(ammo=5, ap=95, revision=11)
    for state in observed:
        state["receipt"]["command"] = 1
    return observed


def private_states(observed, *, reserve=12, revision=1):
    result = copy.deepcopy(observed)
    for snapshot, owner in zip(result, held_kit_states()):
        snapshot["inventories"] = copy.deepcopy(owner["inventories"])
    inventory = result[0]["inventories"][0]
    inventory["revision"] = revision
    inventory["slots"][25].update(condition=min(6, reserve), resource_total=reserve)
    # The public convenience fixture duplicates receipts. A real isolated
    # reload changes only its sender's receipt; the other peer retains this one.
    result[1]["receipt"] = copy.deepcopy(initial_states()[1]["receipt"])
    return result


def fire_evidence():
    return {
        "session": 1, "world": 2,
        "shot": {
            "sender": 1, "command": 1, "shooter": [0, 1], "target": [2, 3],
            "primary_before": primary(ammo=6), "primary_after": primary(ammo=5),
            "ammo_spent": 1, "ap_before": 100, "ap_after": 95,
            "revisions_before": [10, 10], "revisions_after": [11, 11],
        },
        "submitted_attempts": 1, "refusals": [],
    }


class FakeReloadProbe:
    def __init__(self, replies=None):
        self.commands = []
        self.replies = replies or []

    def send(self, verb, *arguments):
        self.commands.append((verb, *arguments))
        return len(self.commands)

    def receive(self, request, _kind, _deadline, _guard):
        if self.replies:
            return copy.deepcopy(self.replies[min(request - 1, len(self.replies) - 1)])
        return {"submitted": True, "result": 0, "command": request + 1}


class ReloadOutcomeTests(unittest.TestCase):
    def wait_reload(self, observed=None, *, sequence=None, initial=None, timeout=1.2,
                    verify_reserve_rounds=False):
        snapshots = sequence if sequence is not None else [
            observed if observed is not None else reload_states()]
        initial = initial if initial is not None else initial_states()
        clock = [0.0]
        self.observations = []

        def next_states(*_args):
            index = min(len(self.observations), len(snapshots) - 1)
            self.observations.append(clock[0])
            snapshot = snapshots[index]
            if isinstance(snapshot, Exception):
                raise snapshot
            return copy.deepcopy(snapshot)

        def sleep(seconds):
            clock[0] += seconds

        with patch.object(reload, "_states", side_effect=next_states), \
                patch.object(reload.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(reload.time, "sleep", side_effect=sleep):
            try:
                return reload._wait_reload([object(), object()], lambda: None,
                                           0, 2, (0, 1), initial, timeout,
                                           verify_reserve_rounds=verify_reserve_rounds)
            finally:
                self.elapsed = clock[0]

    def test_applied_reload_requires_actual_ammunition_increase_and_stability(self):
        result, evidence = self.wait_reload()
        self.assertEqual(result, "applied")
        self.assertEqual(evidence["ammo_added"], 3)
        self.assertEqual(evidence["primary_before"]["ammo_count"], 5)
        self.assertEqual(evidence["primary_after"]["ammo_count"], 8)
        self.assertTrue(evidence["combat_ap_cost_verified"])
        self.assertEqual(evidence["ap_mode"], "combat-cost")
        self.assertGreaterEqual(self.elapsed, 0.5)
        self.assertGreater(len(self.observations), 1)
        self.assertNotIn("reserve_rounds", evidence)

    def test_optional_typed_reserve_rounds_exactly_balance_loaded_increase(self):
        outcome, evidence = self.wait_reload(
            private_states(reload_states(), reserve=9, revision=2),
            initial=private_states(initial_states()), verify_reserve_rounds=True)
        self.assertEqual(outcome, "applied")
        reserve = evidence["reserve_rounds"]
        self.assertEqual(reserve["scope"], "typed ordinary reserve-round conservation")
        self.assertEqual((reserve["reserve_rounds_before"], reserve["reserve_rounds_after"],
                          reserve["loaded_rounds_added"]), (12, 9, 3))
        self.assertEqual((reserve["inventory_revision_before"], reserve["inventory_revision_after"]), (1, 2))
        self.assertFalse(reserve["hidden_lbe_or_full_object_conservation"])

    def test_optional_reserve_mismatch_never_certifies_or_replays(self):
        for reserve in (8, 10, 12, 13):
            with self.subTest(reserve=reserve), self.assertRaisesRegex(reload.ReloadFailure, "reserve-round decrease"):
                self.wait_reload(private_states(reload_states(), reserve=reserve, revision=2),
                                 initial=private_states(initial_states()), verify_reserve_rounds=True)

    def test_private_replacement_may_arrive_late_while_queued_but_must_precede_unlock(self):
        before = private_states(initial_states())
        queued = copy.deepcopy(before)
        queued[0].update(outstanding=2, receipt={"command": 2, "status": 1, "reason": 0, "revision": 11})
        after = private_states(reload_states(), reserve=9, revision=2)
        self.assertEqual(self.wait_reload(sequence=[queued, queued, after], initial=before,
                                         verify_reserve_rounds=True)[0], "applied")
        with self.assertRaisesRegex(reload.ReloadFailure, "terminal unlock preceded"):
            self.wait_reload(sequence=[private_states(reload_states()), after], initial=before,
                             verify_reserve_rounds=True)

    def test_missing_wrong_owner_world_and_replaced_baseline_metrics_fail(self):
        for problem in ("missing", "owner", "world", "baseline", "regressed", "unsigned", "kind"):
            with self.subTest(problem=problem):
                after = private_states(reload_states(), reserve=9, revision=2)
                inventory = after[0]["inventories"][0]
                if problem == "missing":
                    after[0]["inventories"] = []
                elif problem == "owner":
                    inventory["actor"] = [1, 2]
                elif problem in ("world", "baseline"):
                    inventory[problem] += 1
                elif problem == "regressed":
                    inventory["revision"] = 0
                elif problem == "unsigned":
                    inventory["slots"][25]["resource_total"] = -1
                else:
                    inventory["slots"][25]["status_kind"] = 0
                with self.assertRaises(reload.ReloadFailure):
                    self.wait_reload(after, initial=private_states(initial_states()), verify_reserve_rounds=True)

    def test_changed_opaque_or_nonammo_reserve_cannot_be_ignored(self):
        for slot in (6, 7, 8):
            with self.subTest(slot=slot):
                after = private_states(reload_states(), reserve=9, revision=2)
                changed = after[0]["inventories"][0]["slots"][slot]
                changed["condition"] -= 1
                if changed["status_kind"] == 4:
                    changed["resource_total"] -= 1
                with self.assertRaisesRegex(reload.ReloadFailure, "opaque or non-ammunition"):
                    self.wait_reload(after, initial=private_states(initial_states()), verify_reserve_rounds=True)

    def test_rejected_reload_must_preserve_private_resources_before_stale_retry(self):
        before = private_states(initial_states())
        rejected = private_states(reload_states(ammo=5, ap=95, status=2, reason=9))
        self.assertEqual(self.wait_reload(rejected, initial=before, verify_reserve_rounds=True)[0], "stale")
        rejected[0]["inventories"][0]["slots"][25]["resource_total"] -= 1
        with self.assertRaisesRegex(reload.ReloadFailure, "rejected reload changed typed private"):
            self.wait_reload(rejected, initial=before, verify_reserve_rounds=True)

    def test_private_reserve_observation_gap_is_ambiguous(self):
        after = private_states(reload_states(), reserve=9, revision=2)
        gap = copy.deepcopy(after)
        gap[1]["ready"] = False
        with self.assertRaisesRegex(reload.ReloadFailure, "world scope"):
            self.wait_reload(sequence=[gap, after], initial=private_states(initial_states()),
                             verify_reserve_rounds=True)

    def test_consumed_stack_removal_and_stowed_magazines_are_counted(self):
        before = private_states(initial_states(), reserve=3)
        before[0]["inventories"][0]["slots"][25].update(count=1)
        after = private_states(reload_states(), reserve=9, revision=2)
        after[0]["inventories"][0]["slots"][25].update(
            item=0, count=0, condition=0, support=0, status_kind=0, resource_total=0)
        self.assertEqual(self.wait_reload(after, initial=before, verify_reserve_rounds=True)[0], "applied")
        before = private_states(initial_states())
        after = private_states(reload_states(), reserve=6, revision=2)
        after[0]["inventories"][0]["slots"][26].update(
            item=88, count=1, condition=3, support=1, status_kind=2, resource_total=3)
        self.assertEqual(self.wait_reload(after, initial=before, verify_reserve_rounds=True)[0], "applied")

    def test_another_peer_command_prevents_isolated_reserve_certificate(self):
        for field in ("outstanding", "receipt"):
            with self.subTest(field=field):
                after = private_states(reload_states(), reserve=9, revision=2)
                after[1][field] = 3 if field == "outstanding" else {
                    "command": 3, "status": 3, "reason": 0, "revision": 12}
                with self.assertRaisesRegex(reload.ReloadFailure, "another peer command"):
                    self.wait_reload(after, initial=private_states(initial_states()), verify_reserve_rounds=True)

    def test_terminal_receipt_cannot_disappear_change_identity_or_regress(self):
        for receipt in (None, {"command": 3, "status": 3, "reason": 0, "revision": 12},
                        {"command": 2, "status": 1, "reason": 0, "revision": 12}):
            with self.subTest(receipt=receipt):
                after = reload_states()
                changed = copy.deepcopy(after)
                changed[0]["receipt"] = receipt
                with self.assertRaises(reload.ReloadFailure):
                    self.wait_reload(sequence=[after, changed])

    def test_applied_without_ammunition_increase_is_not_reload_proof(self):
        for ammo in (5, 4):
            with self.subTest(ammo=ammo), self.assertRaises(reload.ReloadFailure):
                self.wait_reload(reload_states(ammo=ammo))

    def test_chambering_unjamming_and_ap_cost_do_not_replace_ammunition_increase(self):
        observed = reload_states(ammo=5)
        for snapshot in observed:
            snapshot["actors"][0]["primary"].update(chambered=False, condition=96)
        with self.assertRaises(reload.ReloadFailure):
            self.wait_reload(observed)

    def test_new_ammunition_identity_and_condition_are_valid_reload_outcomes(self):
        observed = reload_states()
        for snapshot in observed:
            snapshot["actors"][0]["primary"].update(
                ammo_item=88, ammo_condition=85, chambered=False)
        result, evidence = self.wait_reload(observed)
        self.assertEqual(result, "applied")
        self.assertEqual(evidence["primary_after"]["ammo_item"], 88)

    def test_jammed_or_unusable_ammunition_is_not_certified_as_ready_reload(self):
        for field, value in (("jammed", True), ("ammo_condition", 0),
                             ("ammo_condition", -100)):
            with self.subTest(field=field):
                observed = reload_states()
                for snapshot in observed:
                    snapshot["actors"][0]["primary"][field] = value
                with self.assertRaises(reload.ReloadFailure):
                    self.wait_reload(observed)

    def test_weapon_or_stack_replacement_cannot_look_like_reload(self):
        for field, value in (("item", 6), ("quantity", 2), ("ammo_state", False)):
            with self.subTest(field=field):
                observed = reload_states()
                for snapshot in observed:
                    snapshot["actors"][0]["primary"][field] = value
                with self.assertRaises(reload.ReloadFailure):
                    self.wait_reload(observed)

    def test_queued_receipt_does_not_prove_an_applied_reload(self):
        with self.assertRaises(reload.ReloadFailure):
            self.wait_reload(reload_states(status=1))

    def test_wrong_command_receipt_cannot_certify_reload(self):
        observed = reload_states()
        observed[0]["receipt"]["command"] = 1
        with self.assertRaises(reload.ReloadFailure):
            self.wait_reload(observed)

    def test_sender_lock_and_native_blocking_gate_must_clear(self):
        for peer, field in ((0, "outstanding"), (0, "commands_blocked"),
                            (1, "commands_blocked")):
            with self.subTest(peer=peer, field=field):
                observed = reload_states()
                observed[peer][field] = 2 if field == "outstanding" else True
                with self.assertRaises(reload.ReloadFailure):
                    self.wait_reload(observed)

    def test_both_replicas_require_newer_revisions(self):
        for peer in (0, 1):
            with self.subTest(peer=peer):
                observed = reload_states()
                observed[peer]["revision"] = 11
                with self.assertRaises(reload.ReloadFailure):
                    self.wait_reload(observed)

    def test_current_primary_actor_and_phase_must_agree_on_both_replicas(self):
        mutations = (
            ("primary", "ammo_count", 9), ("primary", "ammo_item", 88),
            ("primary", "condition", 96), ("primary", "chambered", False),
            ("primary", "jammed", True), ("actor", "ap", 89),
            ("actor", "life", 89), ("actor", "world_x", 1001),
            ("phase", "turn_serial", 2), ("phase", "interrupt_serial", 7),
        )
        for scope, field, value in mutations:
            with self.subTest(scope=scope, field=field):
                observed = reload_states()
                subject = (observed[1]["actors"][0]["primary"] if scope == "primary"
                           else observed[1]["actors"][0] if scope == "actor"
                           else observed[1])
                subject[field] = value
                with self.assertRaises(reload.ReloadFailure):
                    self.wait_reload(observed)

    def test_same_combat_phase_requires_positive_ap_cost(self):
        for ap in (95, 96, -1):
            with self.subTest(ap=ap), self.assertRaises(reload.ReloadFailure):
                self.wait_reload(reload_states(ap=ap))

    def test_turn_or_interrupt_change_downgrades_ap_cost_to_agreement(self):
        for field, value in (("turn_serial", 2), ("team", 1),
                             ("turn_based", False), ("in_combat", False),
                             ("interrupt_phase", 2), ("interrupt_serial", 7)):
            with self.subTest(field=field):
                observed = reload_states(ap=100)
                for snapshot in observed:
                    snapshot[field] = value
                result, evidence = self.wait_reload(observed)
                self.assertEqual(result, "applied")
                self.assertFalse(evidence["combat_ap_cost_verified"])
                self.assertEqual(evidence["ap_mode"], "agreement-only")

    def test_realtime_start_never_claims_combat_ap_cost(self):
        before = initial_states()
        after = reload_states(ap=100)
        for snapshot in [*before, *after]:
            snapshot["in_combat"] = False
        result, evidence = self.wait_reload(after, initial=before)
        self.assertEqual(result, "applied")
        self.assertFalse(evidence["combat_ap_cost_verified"])

    def test_transient_interrupt_cannot_restore_combat_ap_coverage(self):
        observed = reload_states(ap=95)
        interrupted = copy.deepcopy(observed)
        for snapshot in interrupted:
            snapshot.update(interrupt_phase=2, interrupt_serial=7)
        result, evidence = self.wait_reload(sequence=[interrupted, observed])
        self.assertEqual(result, "applied")
        self.assertFalse(evidence["combat_ap_cost_verified"])

    def test_gap_in_observation_cannot_restore_combat_ap_coverage(self):
        observed = reload_states(ap=95)
        unavailable = copy.deepcopy(observed)
        unavailable[0]["ready"] = False
        result, evidence = self.wait_reload(sequence=[unavailable, observed])
        self.assertEqual(result, "applied")
        self.assertFalse(evidence["combat_ap_cost_verified"])

    def test_stale_rejections_release_lock_without_equipment_or_ap_side_effects(self):
        for reason in (5, 9):
            with self.subTest(reason=reason):
                result, evidence = self.wait_reload(
                    reload_states(ammo=5, ap=95, status=2, reason=reason))
                self.assertEqual(result, "stale")
                self.assertIsNone(evidence)
        for reason in (5, 9):
            for problem in ("ammo", "condition", "chamber", "ammo_identity", "ap", "lock"):
                with self.subTest(reason=reason, problem=problem):
                    observed = reload_states(ammo=5, ap=95, status=2, reason=reason)
                    actor = observed[0]["actors"][0]
                    if problem == "ammo":
                        actor["primary"]["ammo_count"] += 1
                    elif problem == "condition":
                        actor["primary"]["condition"] -= 1
                    elif problem == "chamber":
                        actor["primary"]["chambered"] = False
                    elif problem == "ammo_identity":
                        actor["primary"]["ammo_item"] = 88
                    elif problem == "ap":
                        actor["ap"] -= 1
                    else:
                        observed[0]["outstanding"] = 2
                    with self.assertRaises(reload.ReloadFailure):
                        self.wait_reload(observed)

    def test_rejection_side_effect_on_later_poll_is_not_hidden_by_retry(self):
        rejected = reload_states(ammo=5, ap=95, status=2, reason=9)
        side_effect = copy.deepcopy(rejected)
        side_effect[1]["actors"][0]["primary"]["ammo_count"] += 1
        with self.assertRaises(reload.ReloadFailure):
            self.wait_reload(sequence=[rejected, side_effect])

    def test_rejected_after_applied_is_terminal(self):
        with self.assertRaises(reload.ReloadFailure):
            self.wait_reload(sequence=[reload_states(),
                                       reload_states(ammo=5, ap=95, status=2, reason=9)])

    def test_gameplay_rejection_missing_reserves_or_other_refusal_cannot_pass(self):
        for status, reason in ((2, 12), (2, 8), (4, 0)):
            with self.subTest(status=status, reason=reason), self.assertRaises(reload.ReloadFailure):
                self.wait_reload(reload_states(ammo=5, ap=95, status=status, reason=reason))

    def test_world_or_session_change_is_terminal(self):
        for field, value in (("world", 3), ("session", 2)):
            with self.subTest(field=field):
                observed = reload_states()
                observed[1][field] = value
                # The private observation seam shares the fire helper's
                # MovementFailure subclasses; the public API normalizes them.
                with self.assertRaises(reload.MovementFailure):
                    self.wait_reload(observed)

    def test_timeout_preserves_ammunition_and_receipt_diagnostics(self):
        observed = reload_states()
        observed[1]["actors"][0]["primary"]["ammo_count"] = 5
        with self.assertRaises(reload.ReloadFailure) as failure:
            self.wait_reload(observed)
        for text in ('"ammo_count":8', '"ammo_count":5', '"command":2',
                     '"revision":12', '"world_x":1000', "Applied=True"):
            self.assertIn(text, str(failure.exception))

    def test_lost_readiness_or_busy_state_restarts_stability(self):
        for field, value in (("ready", False), ("resynchronizing", True),
                             ("commands_blocked", True), ("revision", 11)):
            with self.subTest(field=field):
                ready = reload_states()
                lost = copy.deepcopy(ready)
                lost[1][field] = value
                self.wait_reload(sequence=[ready, ready, ready, lost, ready])
                self.assertGreaterEqual(self.elapsed, 0.4 + 0.5)


class ReloadCoordinatorTests(unittest.TestCase):
    @staticmethod
    def completed(*, reserve=False):
        evidence = {
            "sender": 1, "command": 2, "actor": [0, 1],
            "primary_before": primary(ammo=5), "primary_after": primary(ammo=8),
            "ammo_added": 3, "ap_before": 95, "ap_after": 90,
            "combat_ap_cost_verified": True, "ap_mode": "combat-cost",
            "phase_before": {}, "phase_after": {},
            "revisions_before": [11, 11], "revisions_after": [12, 12],
        }
        if reserve:
            evidence["reserve_rounds"] = reload._reserve_conservation(
                private_states(initial_states())[0]["inventories"][0],
                private_states(reload_states(), reserve=9, revision=2)[0]["inventories"][0], 3)
        return "applied", evidence

    def certificate(self, *, outcomes=None, initial=None, evidence=None,
                    probes=None, timeout=1.2, verify_reserve_rounds=False):
        initial = initial if initial is not None else initial_states()
        evidence = evidence if evidence is not None else fire_evidence()
        probes = probes if probes is not None else [FakeReloadProbe(), FakeReloadProbe()]
        self.probes = probes
        clock = [0.0]

        def sleep(seconds):
            clock[0] += seconds

        with ExitStack() as stack:
            stack.enter_context(patch.object(reload, "_Probe", side_effect=probes))
            stack.enter_context(patch.object(reload, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(reload, "_states", return_value=initial))
            stack.enter_context(patch.object(reload.time, "monotonic", side_effect=lambda: clock[0]))
            stack.enter_context(patch.object(reload.time, "sleep", side_effect=sleep))
            stack.enter_context(patch("builtins.print"))
            self.wait = stack.enter_context(patch.object(
                reload, "_wait_reload", side_effect=outcomes if outcomes is not None
                else [self.completed()]))
            return reload.certify_two_client_reload(
                [object(), object()], object(), evidence, timeout,
                verify_reserve_rounds=verify_reserve_rounds)

    def test_reloads_only_exact_fired_actor_on_its_owner_client(self):
        evidence = self.certificate()
        self.assertEqual(self.probes[0].commands, [("RELOAD", 0, 1)])
        self.assertEqual(self.probes[1].commands, [])
        self.assertEqual(evidence["submitted_attempts"], 1)
        self.assertEqual(evidence["reload"]["ammo_added"], 3)
        self.assertFalse(self.wait.call_args.kwargs["verify_reserve_rounds"])

    def test_optional_private_certificate_is_forwarded_without_another_gameplay_action(self):
        evidence = self.certificate(initial=private_states(initial_states()),
                                    outcomes=[self.completed(reserve=True)], verify_reserve_rounds=True)
        self.assertEqual(self.probes[0].commands, [("RELOAD", 0, 1)])
        self.assertEqual(self.probes[1].commands, [])
        self.assertTrue(self.wait.call_args.kwargs["verify_reserve_rounds"])
        self.assertEqual(evidence["reload"]["reserve_rounds"]["loaded_rounds_added"], 3)

    def test_missing_private_inventory_fails_before_sending_when_requested(self):
        with self.assertRaises(reload.ReloadFailure):
            self.certificate(verify_reserve_rounds=True)
        self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_requested_private_certificate_cannot_be_silently_skipped(self):
        with self.assertRaisesRegex(reload.ReloadFailure, "without its requested"):
            self.certificate(initial=private_states(initial_states()), verify_reserve_rounds=True)
        self.assertEqual(self.probes[0].commands, [("RELOAD", 0, 1)])

    def test_fired_actor_on_client_b_is_not_replaced_by_client_a_candidate(self):
        evidence = fire_evidence()
        evidence["shot"].update(sender=2, shooter=[1, 2])
        initial = initial_states()
        for snapshot in initial:
            snapshot["actors"][1]["primary"] = primary(ammo=5)
        self.certificate(evidence=evidence, initial=initial)
        self.assertEqual(self.probes[0].commands, [])
        self.assertEqual(self.probes[1].commands, [("RELOAD", 1, 2)])

    def test_current_ap_life_ammo_and_condition_need_not_equal_historical_fire_state(self):
        initial = initial_states()
        for snapshot in initial:
            snapshot["actors"][0].update(ap=80, life=75)
            snapshot["actors"][0]["primary"].update(ammo_count=4, condition=96)
        self.certificate(initial=initial)
        supplied_initial = self.wait.call_args.args[5]
        self.assertEqual(supplied_initial[0]["actors"][0]["ap"], 80)
        self.assertEqual(supplied_initial[0]["actors"][0]["primary"]["ammo_count"], 4)

    def test_current_weapon_identity_must_match_certified_shot(self):
        for field, value in (("item", 6), ("quantity", 2)):
            with self.subTest(field=field):
                initial = initial_states()
                for snapshot in initial:
                    snapshot["actors"][0]["primary"][field] = value
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(initial=initial)
                self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_incoherent_current_primary_or_phase_waits_without_sending_reload(self):
        for scope, field, value in (("primary", "ammo_count", 4),
                                    ("actor", "ap", 94), ("phase", "turn_serial", 2)):
            with self.subTest(scope=scope, field=field):
                initial = initial_states()
                subject = (initial[1]["actors"][0]["primary"] if scope == "primary"
                           else initial[1]["actors"][0] if scope == "actor"
                           else initial[1])
                subject[field] = value
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(initial=initial)
                self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_shared_empty_or_wrong_owner_assignments_fail_before_reload(self):
        for problem in ("shared", "empty", "wrong_owner"):
            with self.subTest(problem=problem):
                initial = initial_states()
                if problem == "shared":
                    initial[1]["assigned"] = [[0, 1]]
                elif problem == "empty":
                    initial[0]["assigned"] = []
                else:
                    initial[0]["assigned"], initial[1]["assigned"] = [[1, 2]], [[0, 1]]
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(initial=initial)
                self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_world_identity_and_each_fire_revision_floor_are_required(self):
        for peer, field, value in ((0, "session", 2), (1, "world", 3),
                                   (0, "revision", 10), (1, "revision", 10)):
            with self.subTest(peer=peer, field=field):
                initial = initial_states()
                initial[peer][field] = value
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(initial=initial)
                self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_missing_dead_out_of_sector_or_unrendered_actor_is_not_replaced(self):
        for field, value in (("life", 0), ("active", False),
                             ("in_sector", False), ("pose", False)):
            with self.subTest(field=field):
                initial = initial_states()
                for snapshot in initial:
                    snapshot["actors"][0][field] = value
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(initial=initial)
                self.assertEqual([probe.commands for probe in self.probes], [[], []])
        initial = initial_states()
        for snapshot in initial:
            snapshot["actors"] = snapshot["actors"][1:]
        with self.assertRaises(reload.ReloadFailure):
            self.certificate(initial=initial)

    def test_public_turn_interrupt_and_busy_gates_prevent_submission(self):
        for updates in ({"team": 1}, {"interrupt_phase": 1},
                        {"interrupt_phase": 2, "interrupt_serial": 8},
                        {"commands_blocked": True}, {"outstanding": 9}):
            with self.subTest(updates=updates):
                initial = initial_states()
                for snapshot in initial:
                    snapshot.update(updates)
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(initial=initial)
                self.assertEqual([probe.commands for probe in self.probes], [[], []])

    def test_valid_owned_interrupt_actor_can_submit_reload(self):
        initial = initial_states()
        for snapshot in initial:
            snapshot.update(interrupt_phase=2, interrupt_serial=8)
            snapshot["actors"][0]["interrupt_eligible"] = True
        self.certificate(initial=initial)
        self.assertEqual(self.probes[0].commands, [("RELOAD", 0, 1)])

    def test_stale_refusals_retry_with_fresh_ids_and_exact_same_actor(self):
        evidence = self.certificate(outcomes=[("stale", None), self.completed()])
        self.assertEqual(self.probes[0].commands, [("RELOAD", 0, 1), ("RELOAD", 0, 1)])
        self.assertEqual([call.args[3] for call in self.wait.call_args_list], [2, 3])
        self.assertEqual(evidence["submitted_attempts"], 2)

    def test_stale_attempts_have_hard_eight_command_cap(self):
        with self.assertRaises(reload.ReloadFailure):
            self.certificate(outcomes=[("stale", None)] * 9, timeout=10)
        self.assertEqual(len(self.probes[0].commands), 8)
        self.assertEqual(self.probes[1].commands, [])

    def test_applied_timeout_and_gameplay_failure_never_reissue(self):
        for message in ("Applied reload never converged", "GameplayRejected: no reserves"):
            with self.subTest(message=message):
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(outcomes=[reload.ReloadFailure(message)])
                self.assertEqual(len(self.probes[0].commands), 1)
                self.assertEqual(self.probes[1].commands, [])

    def test_controller_refusal_and_ambiguous_submission_fail_without_retry(self):
        for reply in ({"submitted": False, "command": 0, "result": "controller rejected"},
                      {"submitted": False, "command": 7, "result": "wire failure"},
                      {"submitted": True, "command": 0},
                      {"submitted": True, "command": True},
                      {"submitted": True, "command": 1}):
            with self.subTest(reply=reply):
                probes = [FakeReloadProbe([reply]), FakeReloadProbe()]
                with self.assertRaises(reload.ReloadFailure):
                    self.certificate(probes=probes)
                self.assertEqual(len(probes[0].commands), 1)
                self.assertEqual(probes[1].commands, [])

    def test_stale_retry_cannot_reuse_prior_reload_command_id(self):
        reply = {"submitted": True, "result": 0, "command": 2}
        probes = [FakeReloadProbe([reply, reply]), FakeReloadProbe()]
        with self.assertRaises(reload.ReloadFailure):
            self.certificate(probes=probes, outcomes=[("stale", None), self.completed()])
        self.assertEqual(len(probes[0].commands), 2)
        self.assertEqual(self.wait.call_count, 1)


if __name__ == "__main__":
    unittest.main()
