"""Build-free strict outcome and no-setup tests for installed medical evidence."""

import copy
from contextlib import ExitStack
import unittest
from unittest.mock import patch

import full_engine_coop_installed_first_aid as aid
from test_full_engine_coop_installed_drop import before_states


def idle_service():
    return {"providing": False, "patient_slot": 65535, "patient_inc": 0, "receiving_count": 0}


def initial_states(*, critical=False):
    result = before_states()
    for state in result:
        for actor in state["actors"]:
            actor.update(bleeding=0, hand_medical_kit_points=0, medical_service=idle_service())
        medic, patient = state["actors"][:2]
        medic["primary"].update(item=202, condition=100, ammo_state=False, ammo_item=0, ammo_count=0,
                                ammo_condition=0, chambered=False, jammed=False)
        medic["hand_medical_kit_points"] = 100
        patient.update(life=10 if critical else 80, bleeding=0 if critical else 10)
    result[0]["inventories"][0]["slots"][5].update(item=202, condition=100, status_kind=3, resource_total=100)
    return result


def treatment_states(before=None, *, outcome=False, ended=False, exhausted=False):
    before = before or initial_states()
    result = copy.deepcopy(before)
    for state in result:
        state["revision"] += 2 if outcome else 1
        medic, patient = state["actors"][:2]
        medic.update(ap=95, stance=2, direction=4, animation=100)
        if not ended:
            medic["medical_service"].update(providing=True, patient_slot=1, patient_inc=2)
            patient["medical_service"]["receiving_count"] = 1
        if outcome:
            points = 0 if exhausted else 98
            medic["hand_medical_kit_points"] = points
            medic["primary"].update(condition=points)
            if exhausted:
                medic["primary"].update(item=0, quantity=0)
            if patient["bleeding"]:
                patient["bleeding"] -= 2
            else:
                patient["life"] += 1
    owner = result[0]["inventories"][0]
    if outcome:
        owner["revision"] += 1
        owner["slots"][5].update(condition=0 if exhausted else 98, resource_total=0 if exhausted else 98)
        if exhausted:
            owner["slots"][5].update(item=0, count=0, support=0, status_kind=0)
    result[0]["receipt"] = {"command": 2, "status": 3, "reason": 0, "revision": before[0]["revision"] + 1}
    return result


class Harness:
    def __init__(self, before=None, samples=None):
        self.before = copy.deepcopy(before or initial_states())
        self.samples = copy.deepcopy(samples if samples is not None else [
            treatment_states(self.before), treatment_states(self.before, outcome=True)])
        self.commands = []
        self.reads = self.index = 0
        self.clock = 0.0
        self.reply_change = None
        self.observe_failure = None
        self.probes = [self.Probe(self, sender) for sender in range(2)]

    class Probe:
        def __init__(self, harness, sender):
            self.h, self.sender = harness, sender

        def send(self, verb, *arguments):
            if verb != "AID":
                raise AssertionError("first aid must not equip, move, advance time, pass interrupts, or shoot")
            self.h.commands.append((self.sender, verb, arguments))
            return len(self.h.commands)

        def receive(self, request, kind, deadline, guard):
            reply = {"request": request, "kind": "aid", "submitted": True, "result": 0, "command": 2}
            assert kind == "aid"
            if self.h.reply_change:
                self.h.reply_change(reply)
            return reply

    def states(self, *_):
        self.reads += 1
        if self.commands:
            if self.observe_failure:
                raise self.observe_failure
            sample = self.samples[min(self.index, len(self.samples) - 1)]
            self.index += 1
            return copy.deepcopy(sample)
        return copy.deepcopy(self.before)

    def certify(self, timeout=0.5):
        with ExitStack() as stack:
            stack.enter_context(patch.object(aid, "_Probe", side_effect=self.probes))
            stack.enter_context(patch.object(aid, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(aid, "_states", side_effect=self.states))
            stack.enter_context(patch.object(aid.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(aid.time, "sleep", side_effect=lambda value: setattr(self, "clock", self.clock + value)))
            return aid.certify_two_client_first_aid([object(), object()], object(), timeout)


class FirstAidTests(unittest.TestCase):
    def test_one_controller_action_proves_service_consumption_and_bleeding_improvement(self):
        h = Harness()
        evidence = h.certify()
        self.assertEqual(h.commands, [(0, "AID", (0, 1, 1, 2))])
        self.assertEqual(evidence["submitted_attempts"], 1)
        self.assertTrue(evidence["initiation_verified"] and evidence["outcome_verified"])
        self.assertEqual(evidence["kit_points_consumed"], 2)
        self.assertEqual(evidence["patient_bleeding_after"], 8)
        self.assertEqual(evidence["service"]["patient"], [1, 2])
        self.assertEqual(evidence["setup_commands"], [])
        self.assertTrue(evidence["controller_only"] and evidence["replica_agreement_verified"])
        self.assertFalse(any(evidence[key] for key in (
            "paint_verified", "physical_input_verified", "full_healing_verified", "full_object_graphs_verified")))

    def test_critical_health_improvement_is_valid_without_initial_bleeding(self):
        before = initial_states(critical=True)
        evidence = Harness(before).certify()
        self.assertEqual((evidence["patient_life_before"], evidence["patient_life_after"]), (10, 11))
        self.assertEqual(evidence["patient_bleeding_after"], 0)

    def test_ended_service_and_fully_consumed_kit_remain_valid_after_observed_relation(self):
        before = initial_states()
        h = Harness(before, [treatment_states(before), treatment_states(before, outcome=True, ended=True, exhausted=True)])
        self.assertEqual(h.certify()["kit_points_consumed"], 100)

    def test_initiation_and_outcome_in_one_coherent_observation_is_valid(self):
        self.assertTrue(Harness(samples=[treatment_states(outcome=True)]).certify()["outcome_verified"])

    def test_healthy_missing_kit_toolkit_unknown_and_empty_resource_do_not_submit(self):
        for problem in ("healthy", "missing", "tool", "unknown", "zero", "empty_first"):
            before = initial_states()
            hand = before[0]["inventories"][0]["slots"][5]
            for state in before:
                medic, patient = state["actors"][:2]
                if problem == "healthy":
                    patient["bleeding"] = 0
                elif problem == "missing":
                    medic["hand_medical_kit_points"] = 0
                elif problem in ("zero", "empty_first"):
                    medic["primary"]["condition"] = 0
                    medic["hand_medical_kit_points"] = 0
            if problem == "tool":
                hand["status_kind"] = 4
            elif problem == "unknown":
                hand.update(status_kind=0, resource_total=0)
            elif problem in ("zero", "empty_first"):
                hand.update(condition=0, resource_total=0)
            h = Harness(before)
            with self.subTest(problem=problem), self.assertRaisesRegex(aid.FirstAidFailure, "no AID submitted"):
                h.certify(0.2)
            self.assertEqual(h.commands, [])

    def test_adjacency_is_cardinal_same_floor_no_row_wrap(self):
        for grid, level in ((18 + 160, 0), (19, 0), (18, 1), (159, 0)):
            before = initial_states()
            for state in before:
                state["actors"][1].update(grid=grid, level=level)
                if grid == 159:
                    state["actors"][0]["grid"] = 160
            h = Harness(before)
            with self.subTest(grid=grid, level=level), self.assertRaisesRegex(aid.FirstAidFailure, "no AID submitted"):
                h.certify(0.2)
            self.assertEqual(h.commands, [])

    def test_readiness_and_interrupts_do_not_trigger_setup_actions(self):
        for problem in ("not_ready", "outstanding", "enemy_turn", "interrupt", "blocked", "existing_service"):
            before = initial_states()
            for state in before:
                if problem == "not_ready":
                    state["ready"] = False
                elif problem == "outstanding":
                    state["outstanding"] = 99
                elif problem == "enemy_turn":
                    state["team"] = 1
                elif problem == "interrupt":
                    state["interrupt_phase"] = 1
                elif problem == "blocked":
                    state["commands_blocked"] = True
                else:
                    state["actors"][1]["medical_service"]["receiving_count"] = 1
            h = Harness(before)
            with self.subTest(problem=problem), self.assertRaisesRegex(aid.FirstAidFailure, "no AID submitted"):
                h.certify(0.2)
            self.assertEqual(h.commands, [])

    def test_public_private_hand_disagreement_does_not_submit(self):
        for problem in ("replica", "private_total", "private_item"):
            before = initial_states()
            if problem == "replica":
                before[1]["actors"][0]["hand_medical_kit_points"] = 99
            elif problem == "private_total":
                before[0]["inventories"][0]["slots"][5].update(condition=99, resource_total=99)
            else:
                before[0]["inventories"][0]["slots"][5]["item"] = 201
            h = Harness(before)
            with self.subTest(problem=problem), self.assertRaisesRegex(aid.FirstAidFailure, "no AID submitted"):
                h.certify(0.2)
            self.assertEqual(h.commands, [])

    def test_malformed_medical_scalars_relations_and_duplicate_actor_fail(self):
        mutations = [lambda actor: actor.update(bleeding=True), lambda actor: actor.update(bleeding=128),
            lambda actor: actor.update(hand_medical_kit_points=-1),
            lambda actor: actor["medical_service"].update(providing=1),
            lambda actor: actor["medical_service"].update(receiving_count=True),
            lambda actor: actor["medical_service"].update(patient_slot=1, patient_inc=2),
            lambda actor: actor["medical_service"].update(patient_slot=65535, patient_inc=1),
            lambda actor: actor["medical_service"].update(patient_slot=1, patient_inc=0, providing=True),
            lambda actor: actor.pop("medical_service"), lambda actor: actor["primary"].update(ammo_state=True)]
        for mutation in mutations:
            before = initial_states()
            mutation(before[0]["actors"][0])
            h = Harness(before, samples=[initial_states()])
            with self.subTest(mutation=mutation), self.assertRaises(aid.FirstAidFailure):
                h.certify()
            self.assertEqual(h.commands, [])
        before = initial_states()
        before[0]["actors"].append(copy.deepcopy(before[0]["actors"][0]))
        with self.assertRaisesRegex(aid.FirstAidFailure, "duplicated"):
            Harness(before).certify()

    def test_foreign_duplicate_and_stale_owner_caches_fail(self):
        for problem in ("foreign", "duplicate", "world", "bool_revision"):
            before = initial_states()
            owner = before[0]["inventories"][0]
            if problem == "foreign":
                owner["actor"] = [1, 2]
            elif problem == "duplicate":
                before[0]["inventories"].append(copy.deepcopy(owner))
            elif problem == "world":
                owner["world"] = 99
            else:
                owner["revision"] = True
            h = Harness(before)
            with self.subTest(problem=problem), self.assertRaises(aid.FirstAidFailure):
                h.certify()
            self.assertEqual(h.commands, [])

    def test_controller_refusal_ambiguous_and_repeated_command_never_retry(self):
        for update in ({"submitted": False, "result": "controller rejected", "command": 0},
                       {"submitted": 1}, {"result": True}, {"command": True}, {"command": 1},
                       {"command": 18446744073709551616}, {"kind": "fire"}, {"request": 99}):
            h = Harness()
            h.reply_change = lambda reply: reply.update(update)
            with self.subTest(update=update), self.assertRaisesRegex(aid.FirstAidFailure, "no replay"):
                h.certify()
            self.assertEqual(len(h.commands), 1)

    def test_rejected_queued_missing_and_changed_receipts_cannot_certify(self):
        for status, reason in ((2, 5), (2, 9), (1, 0), (3, 9)):
            samples = [treatment_states(), treatment_states(outcome=True)]
            for sample in samples:
                sample[0]["receipt"].update(status=status, reason=reason)
            h = Harness(samples=samples)
            with self.subTest(status=status, reason=reason), self.assertRaises(aid.FirstAidFailure):
                h.certify()
            self.assertEqual(len(h.commands), 1)
        for change in (None, {"command": 3, "status": 3, "reason": 0, "revision": 12},
                       {"command": 2, "status": 3, "reason": 0, "revision": 12}):
            after = treatment_states(outcome=True)
            after[0]["receipt"] = change
            with self.subTest(change=change), self.assertRaisesRegex(aid.FirstAidFailure, "receipt"):
                Harness(samples=[treatment_states(), after]).certify()

    def test_service_alone_kit_loss_alone_or_improvement_alone_are_not_outcome(self):
        for problem in ("service_only", "kit_only", "improvement_only", "no_relation"):
            after = treatment_states(outcome=True, ended=problem == "no_relation")
            before = initial_states()
            if problem in ("service_only", "improvement_only"):
                after[0]["inventories"] = copy.deepcopy(before[0]["inventories"])
                for state in after:
                    state["actors"][0]["primary"] = copy.deepcopy(before[0]["actors"][0]["primary"])
                    state["actors"][0]["hand_medical_kit_points"] = 100
            if problem in ("service_only", "kit_only"):
                for state in after:
                    state["actors"][1]["bleeding"] = 10
            h = Harness(samples=[after])
            with self.subTest(problem=problem), self.assertRaisesRegex(aid.FirstAidFailure, "did not demonstrate"):
                h.certify()
            self.assertEqual(len(h.commands), 1)

    def test_private_consumption_requires_revision_and_public_hand_agreement(self):
        for problem in ("revision", "total", "condition", "identity", "count", "unrelated"):
            after = treatment_states(outcome=True)
            owner = after[0]["inventories"][0]
            if problem == "revision":
                owner["revision"] = 1
            elif problem == "total":
                owner["slots"][5].update(condition=97, resource_total=97)
            elif problem == "condition":
                for state in after:
                    state["actors"][0]["primary"]["condition"] = 99
            elif problem == "identity":
                owner["slots"][5]["item"] = 201
            elif problem == "count":
                owner["slots"][5]["count"] = 2
            else:
                owner["slots"][14]["condition"] -= 1
            with self.subTest(problem=problem), self.assertRaises(aid.FirstAidFailure):
                Harness(samples=[treatment_states(), after]).certify()

    def test_baseline_world_ownership_and_revision_boundaries_fail(self):
        for problem in ("baseline", "world", "assignment", "rewind", "resync", "not_ready"):
            after = treatment_states(outcome=True)
            if problem == "baseline":
                after[0]["inventories"][0]["baseline"] += 1
            elif problem == "world":
                after[0]["world"] += 1
            elif problem == "assignment":
                after[0]["assigned"] = [[3, 4]]
            elif problem == "rewind":
                after[0]["revision"] = 9
            elif problem == "resync":
                after[0]["resynchronizing"] = True
            else:
                after[0]["ready"] = False
            h = Harness(samples=[treatment_states(), after])
            with self.subTest(problem=problem), self.assertRaises(aid.FirstAidFailure):
                h.certify()
            self.assertEqual(len(h.commands), 1)

    def test_foreign_unresolved_competing_or_mismatched_services_fail(self):
        for problem in ("foreign", "unresolved", "multiple", "competitor", "wrong_patient_inc"):
            after = treatment_states(outcome=True)
            for state in after:
                medic, patient, other = state["actors"]
                if problem == "foreign":
                    medic["medical_service"].update(patient_slot=2, patient_inc=3)
                elif problem == "unresolved":
                    medic["medical_service"].update(patient_slot=65535, patient_inc=0)
                elif problem == "multiple":
                    patient["medical_service"]["receiving_count"] = 2
                elif problem == "competitor":
                    other["medical_service"].update(providing=True, patient_slot=1, patient_inc=2)
                else:
                    medic["medical_service"]["patient_inc"] = 99
            with self.subTest(problem=problem), self.assertRaisesRegex(aid.FirstAidFailure, "service"):
                Harness(samples=[after]).certify()

    def test_one_sided_service_or_outcome_never_certifies_public_agreement(self):
        for problem in ("service", "outcome", "ap", "phase", "revision"):
            after = treatment_states(outcome=True)
            if problem == "service":
                after[1]["actors"][0]["medical_service"] = idle_service()
                after[1]["actors"][1]["medical_service"] = idle_service()
            elif problem == "outcome":
                after[1]["actors"][1]["bleeding"] = 10
            elif problem == "ap":
                after[1]["actors"][0]["ap"] -= 1
            elif problem == "phase":
                after[1]["turn_serial"] += 1
            else:
                after[1]["revision"] = 10
            with self.subTest(problem=problem), self.assertRaises(aid.FirstAidFailure):
                Harness(samples=[after]).certify()

    def test_stale_public_service_and_private_rewinds_do_not_contribute_evidence(self):
        for problem in ("stale_service", "private_rewind", "same_revision_kit_loss"):
            during, after = treatment_states(), treatment_states(outcome=True)
            if problem == "stale_service":
                for state in during:
                    state["revision"] = 10
            elif problem == "private_rewind":
                during[0]["inventories"][0]["revision"] = 3
            else:
                for state in after:
                    state["revision"] = 11
            with self.subTest(problem=problem), self.assertRaises(aid.FirstAidFailure):
                Harness(samples=[during, after]).certify()

    def test_movement_damage_and_unrelated_commands_invalidate_attribution(self):
        for problem in ("move", "damage", "bleed", "medic_health", "peer_command", "foreign_lock"):
            after = treatment_states(outcome=True)
            if problem == "peer_command":
                after[1]["outstanding"] = 44
            elif problem == "foreign_lock":
                after[0]["outstanding"] = 44
            else:
                for state in after:
                    if problem == "move":
                        state["actors"][1]["grid"] += 1
                    elif problem == "damage":
                        state["actors"][1]["life"] -= 1
                    elif problem == "bleed":
                        state["actors"][1]["bleeding"] = 11
                    else:
                        state["actors"][0]["life"] += 1
            with self.subTest(problem=problem), self.assertRaises(aid.FirstAidFailure):
                Harness(samples=[after]).certify()

    def test_same_replica_damage_after_partial_improvement_fails_while_other_replica_lags(self):
        for critical in (False, True):
            for damage in (False, True):
                before = initial_states(critical=critical)
                during = treatment_states(before)
                lagged = treatment_states(before, outcome=True)
                lagged[0]["actors"][1].update(life=14 if critical else 80, bleeding=0 if critical else 2)
                lagged[1] = copy.deepcopy(during[1])
                caught_up = treatment_states(before, outcome=True)
                for state in caught_up:
                    state["revision"] = 13
                    state["actors"][0]["hand_medical_kit_points"] = 97
                    state["actors"][0]["primary"]["condition"] = 97
                    state["actors"][1].update(life=(12 if damage else 14) if critical else 80,
                                               bleeding=0 if critical else (4 if damage else 2))
                caught_up[0]["inventories"][0]["revision"] = 3
                caught_up[0]["inventories"][0]["slots"][5].update(condition=97, resource_total=97)
                h = Harness(before, [during, lagged, caught_up])
                with self.subTest(critical=critical, damage=damage):
                    if damage:
                        with self.assertRaisesRegex(aid.FirstAidFailure, "concurrent damage"):
                            h.certify()
                    else:
                        evidence = h.certify()
                        self.assertTrue(evidence["outcome_verified"])
                        self.assertEqual(evidence["kit_points_consumed"], 3)
                    self.assertEqual(h.commands, [(0, "AID", (0, 1, 1, 2))])

    def test_noop_and_deadline_failures_preserve_diagnostics_without_retry(self):
        h = Harness(samples=[initial_states()])
        with self.assertRaisesRegex(aid.FirstAidFailure, "Applied=False; service_observed=False"):
            h.certify()
        self.assertEqual(len(h.commands), 1)
        h = Harness()
        h.observe_failure = aid.MovementFailure("client-a timed out answering state request626")
        with self.assertRaisesRegex(aid.FirstAidFailure, "request626.*overall_deadline_exhausted=False.*no replay") as raised:
            h.certify()
        self.assertIs(raised.exception.__cause__, h.observe_failure)
        self.assertEqual(len(h.commands), 1)

    def test_observation_budget_is_finite_even_if_clock_does_not_advance(self):
        h = Harness(samples=[treatment_states()])
        with patch.object(aid, "MAX_OBSERVATIONS", 3):
            with self.assertRaises(aid.FirstAidFailure):
                h.certify(300)
        self.assertEqual(h.index, 3)
        self.assertEqual(len(h.commands), 1)

    def test_timeout_validation_rejects_nonfinite_bool_and_out_of_range(self):
        for timeout in (0, -1, True, float("nan"), float("inf"), 301, "90"):
            with self.subTest(timeout=timeout), self.assertRaises(aid.FirstAidFailure):
                aid.certify_two_client_first_aid([None, None], None, timeout)


if __name__ == "__main__":
    unittest.main()
