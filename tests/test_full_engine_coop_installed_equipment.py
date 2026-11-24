"""Build-free conservation and at-most-once regressions for worn equipment."""

import copy
from contextlib import ExitStack
import unittest
from unittest.mock import patch

import full_engine_coop_installed_equipment as equipment
from full_engine_coop_installed_inventory import _swap_slots
from test_full_engine_coop_installed_drop import before_states


def hand(item=0, quantity=0, condition=0, *, gun=False):
    return {"item": item, "quantity": quantity, "condition": condition, "ammo_state": gun,
            "ammo_item": 77 if gun else 0, "ammo_count": 6 if gun else 0, "ammo_condition": 100 if gun else 0,
            "chambered": gun, "jammed": False}


def initial_states(*, staging=False, gun=False):
    result = before_states()
    for sender, state in enumerate(result):
        slots = state["inventories"][0]["slots"]
        slots[1].update(item=188 + sender, count=1, condition=97, support=1, status_kind=1)
        if staging:
            slots[6].update(item=5 if gun else 204, count=1, condition=94 if gun else 99,
                            support=1, status_kind=1 if gun else 4, resource_total=0 if gun else 99)
    for state in result:
        for actor in state["actors"]:
            actor.update(helmet={"item": 0, "quantity": 0, "condition": 0},
                         legs={"item": 0, "quantity": 0, "condition": 0})
            actor["vest"] = {"item": 188 + actor["slot"], "quantity": 1, "condition": 97}
            actor["secondary"] = hand(5 if gun else 204, 1, 94 if gun else 99, gun=gun) if staging else hand()
    return result


class Harness:
    def __init__(self, *, staging=False, gun=False):
        self.states = initial_states(staging=staging, gun=gun)
        actor = self.states[0]["actors"][0]
        self.objects = {0: hand(), 1: hand(188, 1, 97), 2: hand(),
                        5: copy.deepcopy(actor["primary"]), 6: copy.deepcopy(actor["secondary"]), 21: hand()}
        self.clock = 0.0
        self.messages = []
        self.actions = []
        self.cost = 2
        self.after_action = self.change_reply = self.before_request = None
        self.delay = 0
        self.pending = None
        self.probes = [self.Probe(self, index) for index in range(2)]

    class Probe:
        def __init__(self, harness, sender):
            self.h, self.sender = harness, sender

        def send(self, verb, *arguments):
            if verb not in ("STATE", "SWAP"):
                raise AssertionError(f"equipment attempted an injected turn, placement or other action: {verb}")
            self.h.messages.append((self.sender, verb, arguments))
            return len(self.h.messages)

        def receive(self, request, kind, deadline, guard):
            sender, verb, arguments = self.h.messages[request - 1]
            assert kind == verb.lower()
            if verb == "STATE":
                if sender == 0 and self.h.pending:
                    self.h.pending[2] -= 1
                    if self.h.pending[2] <= 0:
                        source, destination, _ = self.h.pending
                        self.h.pending = None
                        self.h.apply(source, destination)
                if self.h.before_request:
                    self.h.before_request(self.h, sender)
                return copy.deepcopy(self.h.states[sender])
            slot, incarnation, source, destination, revision = arguments
            assert sender == 0 and (slot, incarnation) == (0, 1)
            assert revision == self.h.states[0]["inventories"][0]["revision"]
            self.h.actions.append(arguments)
            reply = {"request": request, "kind": kind, "submitted": True, "result": 0,
                     "command": len(self.h.actions) + 1, "inventory_revision": revision}
            if self.h.delay:
                self.h.pending = [source, destination, self.h.delay]
                self.h.states[0]["outstanding"] = reply["command"]
                self.h.states[0]["receipt"] = {"command": reply["command"], "status": 1,
                                                  "reason": 0, "revision": self.h.states[0]["revision"]}
            else:
                self.h.apply(source, destination)
            if self.h.change_reply:
                self.h.change_reply(self.h, reply)
            return reply

    def apply(self, source, destination):
        inventory = self.states[0]["inventories"][0]
        inventory["slots"] = _swap_slots(inventory["slots"], source, destination)
        inventory["revision"] += 1
        self.objects[source], self.objects[destination] = self.objects[destination], self.objects[source]
        for state in self.states:
            state["revision"] += 1
            actor = state["actors"][0]
            actor["ap"] -= self.cost
            for slot, name in enumerate(("helmet", "vest", "legs")):
                actor[name] = {key: self.objects[slot][key] for key in ("item", "quantity", "condition")}
            actor["primary"] = copy.deepcopy(self.objects[5])
            actor["secondary"] = copy.deepcopy(self.objects[6])
        self.states[0]["outstanding"] = 0
        self.states[0]["receipt"] = {"command": len(self.actions) + 1, "status": 3,
                                      "reason": 0, "revision": self.states[0]["revision"]}
        if self.after_action:
            self.after_action(self)

    def certify(self, timeout=8.0):
        with ExitStack() as stack:
            stack.enter_context(patch.object(equipment, "_Probe", side_effect=self.probes))
            stack.enter_context(patch.object(equipment, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(equipment.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(equipment.time, "sleep",
                                       side_effect=lambda seconds: setattr(self, "clock", self.clock + seconds)))
            return equipment.certify_two_client_equipment([object(), object()], object(), timeout)


class EquipmentTests(unittest.TestCase):
    def test_two_commands_remove_and_restore_worn_armor_into_existing_empty_offhand(self):
        harness = Harness()
        original = copy.deepcopy(harness.states)
        evidence = harness.certify()
        self.assertEqual(harness.actions, [(0, 1, 1, 6, 1), (0, 1, 6, 1, 2)])
        self.assertTrue(evidence["restored"] and evidence["public_armor_and_both_hands_verified"])
        self.assertEqual(evidence["submitted_attempts"], 2)
        self.assertFalse(evidence["offhand_staging"])
        self.assertEqual(harness.states[0]["inventories"][0]["slots"], original[0]["inventories"][0]["slots"])
        for state, before in zip(harness.states, original):
            self.assertEqual(state["actors"][0]["vest"], before["actors"][0]["vest"])
            self.assertEqual(state["actors"][0]["secondary"], before["actors"][0]["secondary"])
        self.assertTrue(evidence["controller_only"])
        self.assertEqual(evidence["turn_commands_submitted"], 0)
        self.assertFalse(evidence["paint_verified"] or evidence["physical_input_verified"] or
                         evidence["full_object_graphs_verified"] or evidence["native_ap_cost_verified"])

    def test_four_commands_stage_real_tool_then_remove_restore_armor_and_exact_original_tool(self):
        harness = Harness(staging=True)
        original = copy.deepcopy(harness.states)
        evidence = harness.certify()
        self.assertEqual([(values[2], values[3]) for values in harness.actions], [(6, 21), (1, 6), (6, 1), (21, 6)])
        self.assertEqual([values[-1] for values in harness.actions], [1, 2, 3, 4])
        self.assertTrue(evidence["offhand_staging"])
        self.assertEqual(evidence["submitted_attempts"], 4)
        self.assertEqual(harness.states[0]["inventories"][0]["slots"], original[0]["inventories"][0]["slots"])
        self.assertEqual(evidence["actions"][0]["loadout_after"]["secondary"], hand())
        self.assertEqual(evidence["actions"][1]["loadout_after"]["secondary"], hand(188, 1, 97))
        self.assertEqual(evidence["actions"][-1]["loadout_after"]["secondary"], hand(204, 1, 99))

    def test_staged_gun_keeps_exact_loaded_ammo_projection_not_just_item_condition(self):
        harness = Harness(staging=True, gun=True)
        evidence = harness.certify()
        self.assertEqual(evidence["actions"][-1]["loadout_after"]["secondary"], hand(5, 1, 94, gun=True))

    def test_candidate_can_use_empty_primary_when_offhand_is_ordinary_and_occupied(self):
        states = initial_states(staging=True)
        for sender, state in enumerate(states):
            slots = state["inventories"][0]["slots"]
            slots[5] = {"slot": 5, "item": 0, "count": 0, "condition": 0,
                        "support": 0, "status_kind": 0, "resource_total": 0}
            for replica in states:
                replica["actors"][sender]["primary"] = hand()
        self.assertEqual(equipment._candidate(states), (0, (0, 1), 1, [(1, 5), (5, 1)]))

    def test_old_inventory_can_use_empty_hand_but_never_niv_only_staging_pocket(self):
        for staging in (False, True):
            harness = Harness(staging=staging)
            for state in harness.states:
                state["inventories"][0]["new_inventory"] = False
            with self.subTest(staging=staging):
                if staging:
                    with self.assertRaisesRegex(equipment.EquipmentFailure, "no SWAP sent"):
                        harness.certify(0.2)
                    self.assertFalse(harness.actions)
                else:
                    evidence = harness.certify()
                    self.assertEqual(evidence["submitted_attempts"], 2)
                    self.assertFalse(evidence["offhand_staging"])

    def test_missing_armor_opaque_offhand_or_occupied_staging_pocket_sends_nothing(self):
        for failure in ("no_armor", "opaque", "occupied"):
            harness = Harness(staging=True)
            for state in harness.states:
                slots = state["inventories"][0]["slots"]
                if failure == "no_armor":
                    slots[1].update(support=2, status_kind=0)
                elif failure == "opaque":
                    slots[6].update(support=2, status_kind=0, resource_total=0)
                else:
                    slots[21] = {**slots[14], "slot": 21}
            with self.subTest(failure=failure), self.assertRaisesRegex(equipment.EquipmentFailure, "no SWAP sent"):
                harness.certify(0.2)
            self.assertFalse(harness.actions)

    def test_private_public_disagreement_before_start_is_not_a_candidate(self):
        states = initial_states()
        for state in states:
            for actor in state["actors"]:
                actor["vest"]["condition"] -= 1
        self.assertIsNone(equipment._candidate(states))

    def test_delayed_queued_swaps_are_sent_once_and_zero_inventory_ap_cost_is_allowed(self):
        harness = Harness(staging=True)
        harness.delay, harness.cost = 3, 0
        evidence = harness.certify()
        self.assertEqual(len(harness.actions), 4)
        self.assertTrue(all(action["ap_before"] == action["ap_after"] for action in evidence["actions"]))

    def test_rejection_or_ambiguous_submission_does_not_try_other_slot_actor_or_retry(self):
        for field, value in (("submitted", False), ("submitted", 1), ("result", True), ("result", 12),
                             ("command", 1), ("command", 3), ("command", True),
                             ("inventory_revision", 99), ("request", True), ("kind", "move")):
            harness = Harness(staging=True)
            harness.change_reply = lambda h, reply: reply.update({field: value})
            with self.subTest(field=field, value=value), self.assertRaisesRegex(equipment.EquipmentFailure, "no replay"):
                harness.certify()
            self.assertEqual(len(harness.actions), 1)

    def test_any_terminal_refusal_after_staging_stops_without_recovery_or_replay(self):
        for status, reason in ((2, 5), (2, 9), (2, 12), (4, 12)):
            harness = Harness(staging=True)
            def reject(h):
                if len(h.actions) == 2:
                    h.states[0]["receipt"].update(status=status, reason=reason)
            harness.after_action = reject
            with self.subTest(status=status, reason=reason), self.assertRaises(equipment.EquipmentFailure):
                harness.certify()
            self.assertEqual(len(harness.actions), 2)

    def test_public_armor_offhand_and_ammo_must_update_exactly_on_both_clients(self):
        for failure in ("old_armor", "old_secondary", "wrong_ammo", "primary_change", "partial"):
            harness = Harness()
            def corrupt(h):
                actor = h.states[1]["actors"][0]
                if failure == "old_armor":
                    actor["vest"] = {"item": 188, "quantity": 1, "condition": 97}
                elif failure == "old_secondary":
                    actor["secondary"] = hand()
                elif failure == "wrong_ammo":
                    actor["secondary"] = hand(188, 1, 97, gun=True)
                elif failure == "primary_change":
                    actor["primary"]["ammo_count"] -= 1
                else:
                    actor["legs"]["condition"] = 1
            harness.after_action = corrupt
            with self.subTest(failure=failure), self.assertRaises(equipment.EquipmentFailure):
                harness.certify(1.0)
            self.assertEqual(len(harness.actions), 1)

    def test_wrong_private_slot_resource_ground_or_unrelated_owner_changes_fail(self):
        for failure in ("source", "resource", "ground", "other_owner", "token", "baseline", "assignment"):
            harness = Harness(staging=True)
            def corrupt(h):
                owner = h.states[0]["inventories"][0]
                if failure == "source":
                    owner["slots"][22] = {**owner["slots"][21], "slot": 22}
                elif failure == "resource":
                    owner["slots"][21]["resource_total"] -= 1
                elif failure == "ground":
                    owner["nearby_loot"]["markers"] = [{"grid": 17, "level": 0, "medical": True}]
                elif failure == "other_owner":
                    h.states[1]["inventories"][0]["revision"] += 1
                elif failure == "token":
                    owner["revision"] -= 1
                elif failure == "baseline":
                    owner["baseline"] += 1
                else:
                    h.states[0]["assigned"] = [[1, 2]]
            harness.after_action = corrupt
            with self.subTest(failure=failure), self.assertRaises(equipment.EquipmentFailure):
                harness.certify()
            self.assertEqual(len(harness.actions), 1)

    def test_applied_without_private_replacement_cannot_certify(self):
        harness = Harness()
        original = copy.deepcopy(harness.states[0]["inventories"])
        harness.after_action = lambda h: h.states[0].update(inventories=copy.deepcopy(original))
        with self.assertRaisesRegex(equipment.EquipmentFailure, "Applied preceded"):
            harness.certify()
        self.assertEqual(len(harness.actions), 1)

    def test_revisions_cannot_rewind_after_an_advanced_observation_even_above_initial(self):
        for private in (False, True):
            harness = Harness()
            observed = 0

            def advance(h):
                if private:
                    h.states[0]["inventories"][0]["revision"] += 1
                else:
                    for state in h.states:
                        state["revision"] += 1

            def rewind(h, sender):
                nonlocal observed
                if sender != 0 or not h.actions:
                    return
                observed += 1
                if observed == 2:
                    if private:
                        h.states[0]["inventories"][0]["revision"] -= 1
                    else:
                        for state in h.states:
                            state["revision"] -= 1

            harness.after_action, harness.before_request = advance, rewind
            with self.subTest(private=private), self.assertRaisesRegex(equipment.EquipmentFailure, "rewound"):
                harness.certify()
            self.assertEqual(len(harness.actions), 1)
            self.assertEqual(observed, 2)

    def test_changed_public_gear_requires_a_fresh_revision_even_with_a_malformed_applied_floor(self):
        harness = Harness()

        def corrupt(h):
            for state in h.states:
                state["revision"] -= 1
            h.states[0]["receipt"]["revision"] -= 1

        harness.after_action = corrupt
        with self.assertRaisesRegex(equipment.EquipmentFailure, "without advancing revision"):
            harness.certify()
        self.assertEqual(len(harness.actions), 1)

    def test_public_ap_or_phase_cannot_change_under_a_previously_seen_revision(self):
        for change in ("ap", "turn_serial", "commands_blocked"):
            harness = Harness()
            observed = 0

            def corrupt(h, sender):
                nonlocal observed
                if sender != 0 or not h.actions:
                    return
                observed += 1
                if observed == 2:
                    for state in h.states:
                        if change == "ap":
                            state["actors"][0]["ap"] -= 1
                        elif change == "turn_serial":
                            state[change] += 1
                        else:
                            state[change] = not state[change]

            harness.before_request = corrupt
            with self.subTest(change=change), self.assertRaisesRegex(equipment.EquipmentFailure, "without advancing revision"):
                harness.certify()
            self.assertEqual(len(harness.actions), 1)

    def test_observed_final_private_or_public_loadout_cannot_revert_under_a_newer_revision(self):
        for private in (False, True):
            harness = Harness()
            original = copy.deepcopy(harness.states)
            observed = 0

            def corrupt(h, sender):
                nonlocal observed
                if sender != 0 or not h.actions:
                    return
                observed += 1
                if observed == 2:
                    if private:
                        inventory = h.states[0]["inventories"][0]
                        inventory["revision"] += 1
                        inventory["slots"] = copy.deepcopy(original[0]["inventories"][0]["slots"])
                    else:
                        for state, before in zip(h.states, original):
                            state["revision"] += 1
                            for field in (*equipment.ARMOR, "primary", "secondary"):
                                state["actors"][0][field] = copy.deepcopy(before["actors"][0][field])

            harness.before_request = corrupt
            with self.subTest(private=private), self.assertRaisesRegex(equipment.EquipmentFailure, "reverted"):
                harness.certify()
            self.assertEqual(len(harness.actions), 1)

    def test_public_revision_or_dimensions_cannot_change_between_settlement_and_next_submission(self):
        original_wait = equipment._wait_swap
        for change in ("revision", "columns", "rows"):
            harness = Harness()

            def corrupt_after_settlement(*arguments):
                result = original_wait(*arguments)
                # The real waiter certified this first command. Corrupt only
                # the next fresh STATE, leaving its returned snapshot intact.
                if change == "revision":
                    harness.states[1][change] -= 1
                else:
                    for state in harness.states:
                        state[change] += 1
                return result

            with self.subTest(change=change), patch.object(equipment, "_wait_swap", side_effect=corrupt_after_settlement), \
                    self.assertRaisesRegex(equipment.EquipmentFailure, "before the next planned action"):
                harness.certify()
            self.assertEqual(len(harness.actions), 1)

    def test_missing_or_foreign_receipt_lock_and_pose_changes_fail_once(self):
        for failure in ("missing", "foreign_receipt", "foreign_lock", "pose", "other_peer", "ap_increase"):
            harness = Harness()
            def corrupt(h):
                state = h.states[0]
                if failure == "missing":
                    state["receipt"] = None
                elif failure == "foreign_receipt":
                    state["receipt"]["command"] += 1
                elif failure == "foreign_lock":
                    state["outstanding"] = 9
                elif failure == "pose":
                    state["actors"][0]["grid"] += 1
                elif failure == "other_peer":
                    h.states[1]["receipt"]["command"] += 1
                else:
                    state["actors"][0]["ap"] += 3
            harness.after_action = corrupt
            with self.subTest(failure=failure), self.assertRaises(equipment.EquipmentFailure):
                harness.certify(1.0)
            self.assertEqual(len(harness.actions), 1)

    def test_missing_secondary_or_boolean_public_metrics_fail_without_submission(self):
        for field, value in (("secondary", None), ("secondary", {**hand(), "quantity": False}),
                             ("vest", {"item": 188, "quantity": True, "condition": 97})):
            harness = Harness()
            harness.states[0]["actors"][0][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(equipment.EquipmentFailure):
                harness.certify()
            self.assertFalse(harness.actions)

    def test_expired_deadline_cannot_submit_a_late_restore(self):
        harness = Harness()
        with patch.object(equipment.time, "monotonic", return_value=5.0), \
                self.assertRaisesRegex(equipment.EquipmentFailure, "deadline expired"):
            equipment._submit(harness.probes[0], lambda: None, (0, 1), 6, 1, harness.states, 0, 4.0)
        self.assertFalse(harness.actions)


if __name__ == "__main__":
    unittest.main()
