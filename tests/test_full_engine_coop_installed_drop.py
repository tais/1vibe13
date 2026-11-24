"""Build-free, at-most-once tests for real-controller drop/pickup evidence."""

import copy
from contextlib import ExitStack
import unittest
from unittest.mock import patch

import full_engine_coop_installed_drop as drop
from test_full_engine_coop_installed_inventory import initial_states


def before_states():
    result = initial_states()
    for state in result:
        inventory = state["inventories"][0]
        actor = state["actors"][inventory["actor"][0]]
        inventory["ground"] = {"grid": actor["grid"], "level": actor["level"], "truncated": False, "items": []}
        inventory["nearby_loot"] = {"truncated": False, "markers": []}
    return result


def after_states(before=None, *, pickup=False, cost=3):
    before = before or before_states()
    result = copy.deepcopy(before)
    owner = result[0]["inventories"][0]
    if pickup:
        summary = copy.deepcopy(owner["ground"]["items"][0]["summary"])
        owner["slots"][14] = {**summary, "slot": 14}
        owner["ground"]["items"] = []
    else:
        summary = copy.deepcopy(owner["slots"][14])
        owner["slots"][14] = drop._empty_slot(14)
        owner["ground"]["items"] = [{"id": [99, 1], "summary": {**summary, "slot": 0}}]
    for state in result:
        inventory = state["inventories"][0]
        inventory["revision"] += 1
        inventory["nearby_loot"]["markers"] = [] if pickup else [
            {"grid": owner["ground"]["grid"], "level": 0,
             "medical": summary["status_kind"] == 3 and summary["resource_total"] > 0}]
        state["revision"] += 1
        state["actors"][0]["ap"] -= cost
    result[0]["receipt"] = {"command": 3 if pickup else 2, "status": 3,
                             "reason": 0, "revision": result[0]["revision"]}
    return result


class Harness:
    def __init__(self):
        self.states = before_states()
        self.commands = []
        self.clock = 0.0
        self.change_action = self.change_ui = self.change_reply = None
        self.probes = [self.Probe(self, sender) for sender in range(2)]

    class Probe:
        def __init__(self, harness, sender):
            self.h, self.sender = harness, sender

        def send(self, verb, *arguments):
            if verb not in ("STATE", "LOOT_UI", "DROP", "PICKUP"):
                raise AssertionError("unexpected injected gameplay or camera command")
            self.h.commands.append((self.sender, verb, arguments))
            return len(self.h.commands)

        def receive(self, request, kind, deadline, guard):
            sender, verb, arguments = self.h.commands[request - 1]
            assert sender == self.sender and kind == verb.lower()
            if verb == "STATE":
                return copy.deepcopy(self.h.states[sender])
            if verb == "LOOT_UI":
                inventory = self.h.states[sender]["inventories"][0]
                markers = inventory["nearby_loot"]["markers"]
                reply = {"request": request, "kind": kind, "ready": True, "actor": inventory["actor"][:],
                         "session": 1, "world": 2, "baseline": inventory["baseline"],
                         "inventory_revision": inventory["revision"], "origin_grid": inventory["ground"]["grid"],
                         "origin_level": 0, "viewport": [0, 0, 640, 300], "available": len(markers),
                         "count": len(markers), "truncated": False, "hovered": -1,
                         "markers": [{**marker, "x": 100, "y": 100} for marker in markers]}
                if self.h.change_ui:
                    self.h.change_ui(reply)
                return reply
            reply = {"request": request, "kind": kind, "submitted": True, "result": 0,
                     "command": 2 if verb == "DROP" else 3, "inventory_revision": arguments[-1]}
            if self.h.change_reply:
                self.h.change_reply(verb, reply)
            self.h.states = after_states(self.h.states, pickup=verb == "PICKUP")
            if self.h.change_action:
                self.h.change_action(verb, self.h.states)
            return reply

    def certify(self, timeout=3.0):
        with ExitStack() as stack:
            stack.enter_context(patch.object(drop, "_Probe", side_effect=self.probes))
            stack.enter_context(patch.object(drop, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(drop, "wait_for_two_client_player_control", return_value={
                "session": 1, "world": 2, "interrupt_passes": [], "submitted_attempts": 0}))
            stack.enter_context(patch.object(drop.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(drop.time, "sleep",
                                       side_effect=lambda seconds: setattr(self, "clock", self.clock + seconds)))
            return drop.certify_two_client_drop([object(), object()], object(), timeout)

    def actions(self):
        return [command for command in self.commands if command[1] not in ("STATE", "LOOT_UI")]


class DropTests(unittest.TestCase):
    def wait(self, observed=None, *, sequence=None, before=None, pickup=False, expected_cost=None):
        before = before or before_states()
        samples = iter(sequence or [observed or after_states(before, pickup=pickup)])
        clock, last = [0.0], [None]

        def states(*_):
            last[0] = next(samples, last[0])
            return copy.deepcopy(last[0])

        with patch.object(drop, "_states", side_effect=states), \
                patch.object(drop.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(drop.time, "sleep", side_effect=lambda seconds: clock.__setitem__(0, clock[0] + seconds)):
            return drop._wait_action([None, None], lambda: None, 0, 3 if pickup else 2, (0, 1), 14,
                before, 1.1, pickup=pickup, item=(99, 1) if pickup else None,
                summary=before_states()[0]["inventories"][0]["slots"][14] if pickup else None,
                expected_ap_cost=expected_cost)

    def test_roundtrip_uses_exact_controller_arguments_once_and_restores_every_summary(self):
        h = Harness()
        evidence = h.certify()
        self.assertEqual(h.actions(), [(0, "DROP", (0, 1, 14, 1)), (0, "PICKUP", (0, 1, 99, 1, 14, 2))])
        self.assertTrue(evidence["restored"] and evidence["exact_ground_identity_retired"])
        self.assertEqual(evidence["submitted_attempts"], 2)
        self.assertTrue(evidence["equal_combat_ap_costs_verified"])
        self.assertEqual([action["combat_ap_cost"] for action in evidence["actions"]], [3, 3])
        self.assertTrue(evidence["marker_view"]["dropped_location_marker_verified"])
        self.assertTrue(evidence["controller_only"])
        self.assertFalse(evidence["paint_verified"] or evidence["physical_input_verified"] or evidence["full_object_graphs_verified"])

    def test_candidate_does_not_mutate_or_choose_a_hand_or_unknown_source(self):
        before = before_states()
        previous = copy.deepcopy(before)
        self.assertEqual(drop._candidate(before, (1, 2)), (0, (0, 1), 14))
        self.assertEqual(before, previous)
        for state in before:
            state["inventories"][0]["slots"][14]["status_kind"] = 0
        self.assertIsNone(drop._candidate(before, (1, 2)))

    def test_no_candidate_or_missing_cache_times_out_without_gameplay(self):
        for missing in (False, True):
            h = Harness()
            for state in h.states:
                if missing:
                    state["inventories"] = []
                else:
                    state["inventories"][0]["slots"][14] = drop._empty_slot(14)
            with self.subTest(missing=missing), self.assertRaisesRegex(drop.DropFailure, "no DROP submitted"):
                h.certify(0.3)
            self.assertEqual(h.actions(), [])

    def test_existing_ground_and_roof_contexts_are_not_drop_candidates(self):
        for roof in (False, True):
            states = before_states()
            for state in states:
                inventory = state["inventories"][0]
                if roof:
                    inventory["ground"]["level"] = 1
                else:
                    inventory["ground"]["items"] = [{"id": [88, 1], "summary": {
                        **inventory["slots"][14], "slot": 0}}]
            with self.subTest(roof=roof):
                self.assertIsNone(drop._candidate(states, (1, 2)))

    def test_applied_without_private_source_retirement_is_not_success(self):
        after = after_states()
        after[0]["inventories"][0] = before_states()[0]["inventories"][0]
        with self.assertRaises(drop.DropFailure):
            self.wait(after)

    def test_queued_noop_or_missing_receipt_cannot_certify(self):
        for receipt in (None, {"command": 2, "status": 1, "reason": 0, "revision": 11}):
            observed = after_states()
            observed[0]["receipt"] = receipt
            with self.subTest(receipt=receipt), self.assertRaisesRegex(drop.DropFailure, "did not settle"):
                self.wait(observed)

    def test_refusals_and_stale_receipts_fail_without_gameplay_retry(self):
        for reason in (5, 9, 12):
            h = Harness()
            h.change_action = lambda verb, states: states[0]["receipt"].update(status=2, reason=reason)
            with self.subTest(reason=reason), self.assertRaisesRegex(drop.DropFailure, "no replay"):
                h.certify()
            self.assertEqual([command[1] for command in h.actions()], ["DROP"])

    def test_malformed_or_ambiguous_submission_does_not_trigger_recovery_or_repeat(self):
        for field, value in (("command", True), ("command", 1), ("submitted", False),
                             ("result", True), ("result", 1), ("inventory_revision", 999), ("request", True)):
            h = Harness()
            h.change_reply = lambda verb, reply: reply.update({field: value})
            with self.subTest(field=field, value=value), self.assertRaisesRegex(drop.DropFailure, "ambiguous"):
                h.certify()
            self.assertEqual([command[1] for command in h.actions()], ["DROP"])

    def test_pickup_submission_rejection_is_never_repeated(self):
        h = Harness()
        h.change_reply = lambda verb, reply: reply.update(submitted=False) if verb == "PICKUP" else None
        with self.assertRaises(drop.DropFailure):
            h.certify()
        self.assertEqual([command[1] for command in h.actions()], ["DROP", "PICKUP"])

    def test_changed_ground_summary_or_duplicate_stack_is_not_conservation(self):
        for mutation in ("summary", "duplicate", "revision"):
            observed = after_states()
            owner = observed[0]["inventories"][0]
            if mutation == "summary":
                owner["ground"]["items"][0]["summary"]["condition"] -= 1
            elif mutation == "duplicate":
                entry = copy.deepcopy(owner["ground"]["items"][0])
                entry["id"] = [100, 1]
                entry["summary"]["slot"] = 1
                owner["ground"]["items"].append(entry)
            else:
                owner["revision"] = 1
            with self.subTest(mutation=mutation), self.assertRaises(drop.DropFailure):
                self.wait(observed)

    def test_ground_identity_cannot_change_during_settlement(self):
        changed = after_states()
        changed[0]["inventories"][0]["ground"]["items"][0]["id"][1] = 2
        with self.assertRaisesRegex(drop.DropFailure, "identity was reused or changed"):
            self.wait(sequence=[after_states(), changed])

    def test_unrelated_slots_marker_location_and_private_lineage_are_conserved(self):
        for field in ("slot", "marker", "baseline", "revision", "owner"):
            observed = after_states()
            owner = observed[1]["inventories"][0]
            if field == "slot":
                owner["slots"][14]["condition"] -= 1
            elif field == "marker":
                owner["nearby_loot"]["markers"][0]["grid"] += 1
            elif field == "owner":
                owner["actor"] = [0, 1]
            else:
                owner[field] = 0
            with self.subTest(field=field), self.assertRaises(drop.MovementFailure):
                self.wait(observed)

    def test_public_pose_health_hand_and_other_peer_command_changes_fail(self):
        for field in ("grid", "direction", "life", "primary", "other_command", "ap_type"):
            observed = after_states()
            if field == "other_command":
                observed[1]["receipt"]["command"] += 1
            else:
                for state in observed:
                    actor = state["actors"][0]
                    if field == "primary":
                        actor["primary"]["condition"] -= 1
                    elif field == "ap_type":
                        actor["ap"] = True
                    else:
                        actor[field] += 1
            with self.subTest(field=field), self.assertRaises(drop.DropFailure):
                self.wait(observed)

    def test_positive_combat_cost_and_equal_pickup_cost_are_required(self):
        for cost in (0, -1):
            with self.subTest(cost=cost), self.assertRaisesRegex(drop.DropFailure, "positive equal"):
                self.wait(after_states(cost=cost))
        dropped = after_states()
        with self.assertRaisesRegex(drop.DropFailure, "positive equal"):
            self.wait(after_states(dropped, pickup=True, cost=4), before=dropped, pickup=True, expected_cost=3)

    def test_realtime_roundtrip_does_not_claim_combat_ap_costs(self):
        h = Harness()
        for state in h.states:
            state["in_combat"] = False
        evidence = h.certify()
        self.assertFalse(evidence["equal_combat_ap_costs_verified"])
        self.assertEqual([action["combat_ap_cost"] for action in evidence["actions"]], [None, None])

    def test_other_replica_private_delivery_can_follow_owner_applied(self):
        partial = after_states()
        partial[1]["inventories"] = before_states()[1]["inventories"]
        _, evidence = self.wait(sequence=[partial, after_states()])
        self.assertTrue(evidence["exact_slot_ground_metrics_verified"])

    def test_pending_public_ap_disagreement_cannot_false_certify(self):
        observed = after_states()
        observed[1]["actors"][0]["ap"] += 1
        with self.assertRaisesRegex(drop.DropFailure, "did not settle"):
            self.wait(observed)

    def test_optional_empty_or_unready_marker_view_does_not_claim_rendering(self):
        for ready in (False, True):
            h = Harness()
            if ready:
                h.change_ui = lambda reply: reply.update(count=0, markers=[])
            else:
                def unavailable(reply):
                    request = reply["request"]
                    reply.clear()
                    reply.update(request=request, kind="loot_ui", ready=False)
                h.change_ui = unavailable
            with self.subTest(ready=ready):
                evidence = h.certify()
                self.assertFalse(evidence["marker_view"]["dropped_location_marker_verified"])
                self.assertEqual(len(h.actions()), 2)

    def test_foreign_or_malformed_marker_view_stops_before_pickup(self):
        for field, value in (("actor", [1, 2]), ("world", 9), ("count", True)):
            h = Harness()
            h.change_ui = lambda reply: reply.update({field: value})
            with self.subTest(field=field), self.assertRaises(drop.DropFailure):
                h.certify()
            self.assertEqual([command[1] for command in h.actions()], ["DROP"])

    def test_pickup_must_retire_exact_ground_id_and_restore_source(self):
        before = after_states()
        after = after_states(before, pickup=True)
        after[0]["inventories"][0]["ground"] = before[0]["inventories"][0]["ground"]
        with self.assertRaises(drop.DropFailure):
            self.wait(after, before=before, pickup=True, expected_cost=3)


if __name__ == "__main__":
    unittest.main()
