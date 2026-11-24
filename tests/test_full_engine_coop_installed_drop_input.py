"""Build-free adversarial checks for the shipping Screen drop input certificate."""

import copy
from contextlib import ExitStack
from pathlib import Path
import unittest
from unittest.mock import patch

import full_engine_coop_installed_drop as drop
import full_engine_coop_installed_drop_input as screen
import full_engine_coop_installed_inventory as inventory
from test_full_engine_coop_installed_drop import before_states, after_states


class Harness:
    def __init__(self):
        self.states = before_states()
        self.clock = 0.0
        self.messages = []
        self.step = 0
        self.pending = None
        self.delay = 2
        self.queued = 0
        self.open = self.dropping = self.picking = False
        self.source = inventory.NO_SLOT
        self.item = screen.INVALID_ITEM[:]
        self.after_process = self.change_reply = self.change_ui = None
        self.never_consume = None
        self.command_offset = 0
        self.probes = [self.Probe(self, sender) for sender in range(2)]

    class Probe:
        def __init__(self, harness, sender):
            self.h, self.sender = harness, sender

        def send(self, verb, *arguments):
            if verb not in ("INVENTORY_UI", "DROP_INPUT_UI", "DROP_INPUT"):
                raise AssertionError(f"Screen certificate bypassed queued input: {verb}")
            self.h.messages.append((self.sender, verb, arguments))
            return len(self.h.messages)

        def receive(self, request, kind, deadline, guard):
            sender, verb, arguments = self.h.messages[request - 1]
            assert sender == self.sender and kind == verb.lower()
            if verb == "INVENTORY_UI":
                return {"ready": True, "open": self.h.open, "actor": [0, 1],
                        "session": 1, "world": 2, "baseline": 1, "inventory_revision": 1,
                        "source": self.h.source, "inspected": self.h.source, "actions_enabled": True,
                        "width": 640, "height": 480}
            if verb == "DROP_INPUT_UI":
                reply = self.h.ui(request)
                if self.h.change_ui:
                    self.h.change_ui(self.h, reply)
                return reply
            step, slot, inc, source, item_slot, item_inc, session, world, baseline, revision = arguments
            assert step == self.h.step + 1 and sender == 0 and (slot, inc, source) == (0, 1, 14)
            assert (session, world, baseline, revision) == (1, 2, 1, self.h.owner()["revision"])
            assert (item_slot, item_inc) == ((99, 1) if step >= 10 else (0, 0))
            receipt = self.h.states[0].get("receipt")
            count = 1 if step in (3, 7, 8) else 3 if step == 5 else 2
            reply = {"request": request, "kind": kind, "queued": True, "step": step,
                     "actor": [slot, inc], "source": source, "item": [item_slot, item_inc],
                     "session": session, "world": world, "baseline": baseline,
                     "inventory_revision": revision, "revision": self.h.states[0]["revision"], "outstanding": 0,
                     "previous_command": receipt["command"] if receipt else 0,
                     "previous_status": receipt["status"] if receipt else 0,
                     "previous_reason": receipt["reason"] if receipt else 0, "queued_events": count}
            self.h.step = step
            self.h.queued = count
            self.h.pending = [step, self.h.delay]
            if self.h.change_reply:
                self.h.change_reply(self.h, reply)
            return reply

    def owner(self):
        return self.states[0]["inventories"][0]

    def ui(self, request):
        owner = self.owner()
        return {"request": request, "kind": "drop_input_ui", "ready": True, "open": self.open,
                "actor": [0, 1], "session": 1, "world": 2, "baseline": owner["baseline"],
                "inventory_revision": owner["revision"], "source": self.source,
                "drop_open": self.dropping, "pickup_open": self.picking, "ground_item": self.item[:],
                "ground_count": 1 if self.picking else 0, "ground_truncated": False,
                "actions_enabled": self.open, "step": self.step, "queued_events": self.queued,
                "revision": self.states[0]["revision"], "outstanding": self.states[0]["outstanding"],
                "receipt": copy.deepcopy(self.states[0].get("receipt"))}

    def observe(self, *_):
        if self.pending and self.pending[0] != self.never_consume:
            self.pending[1] -= 1
            if self.pending[1] <= 0:
                step = self.pending[0]
                self.pending, self.queued = None, 0
                if step == 1:
                    self.open = True
                elif step in (2, 6):
                    self.source = 14
                elif step in (3, 7):
                    self.dropping = True
                elif step == 5:
                    self.dropping, self.source = False, inventory.NO_SLOT
                elif step == 9:
                    self.states = after_states(self.states)
                    self.states[0]["receipt"]["command"] += self.command_offset
                    self.dropping, self.source = False, inventory.NO_SLOT
                elif step == 10:
                    self.picking, self.item = True, [99, 1]
                elif step == 11:
                    self.states = after_states(self.states, pickup=True)
                    self.picking, self.item = False, screen.INVALID_ITEM[:]
                elif step == 12:
                    self.open = False
                if self.after_process:
                    self.after_process(self, step)
        return copy.deepcopy(self.states)

    def inputs(self):
        return [message for message in self.messages if message[1] == "DROP_INPUT"]

    def certify(self, timeout=8.0):
        with ExitStack() as stack:
            stack.enter_context(patch.object(screen, "_Probe", side_effect=self.probes))
            stack.enter_context(patch.object(screen, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(screen, "wait_for_two_client_player_control", return_value={
                "session": 1, "world": 2, "interrupt_passes": [], "submitted_attempts": 0}))
            for module in (screen, drop, inventory):
                stack.enter_context(patch.object(module, "_states", side_effect=self.observe))
            stack.enter_context(patch.object(screen.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(screen.time, "sleep",
                                       side_effect=lambda seconds: setattr(self, "clock", self.clock + seconds)))
            return screen.certify_two_client_drop_input([object(), object()], object(), timeout)


class DropInputTests(unittest.TestCase):
    def test_delayed_shipping_steps_cancel_hold_drop_and_restore_exact_original_slot_once(self):
        harness = Harness()
        harness.delay = 5
        before = copy.deepcopy(harness.owner()["slots"])
        evidence = harness.certify()
        self.assertEqual([message[2][0] for message in harness.inputs()], list(range(1, 13)))
        self.assertEqual(harness.owner()["slots"], before)
        self.assertEqual([action["command"] for action in evidence["actions"]], [2, 3])
        self.assertTrue(evidence["restored"] and evidence["exact_original_slot_restored"])
        self.assertTrue(evidence["screen_queued_input_verified"])
        self.assertTrue(evidence["held_repeat_did_not_confirm"] and evidence["escape_cancel_did_not_drop"])
        self.assertEqual(evidence["queued_input_steps"], 12)
        self.assertEqual(evidence["submitted_attempts"], 2)
        self.assertFalse(evidence["controller_only"] or evidence["physical_input_verified"] or
                         evidence["paint_verified"] or evidence["full_object_graphs_verified"])
        recovery = harness.inputs()[10][2]
        self.assertEqual(recovery, (11, 0, 1, 14, 99, 1, 1, 2, 1, 2))

    def test_natural_candidate_missing_does_not_queue_input_or_inject_items(self):
        harness = Harness()
        for state in harness.states:
            state["inventories"][0]["slots"][14] = drop._empty_slot(14)
        with self.assertRaisesRegex(screen.DropInputFailure, "no Screen DROP input queued"):
            harness.certify(0.2)
        self.assertFalse(harness.inputs())

    def test_arm_repeat_cancel_or_release_must_not_submit_gameplay(self):
        for bad_step in (2, 3, 4, 5, 7, 8):
            harness = Harness()
            def premature(h, step):
                if step == bad_step:
                    h.states = after_states(h.states)
            harness.after_process = premature
            with self.subTest(step=bad_step), self.assertRaises(screen.DropInputFailure):
                harness.certify()
            self.assertEqual(harness.inputs()[-1][2][0], bad_step)
            self.assertEqual(len(harness.inputs()), bad_step)

    def test_arm_and_cancel_require_actual_modal_changes_not_just_queue_acceptance(self):
        for bad_step in (3, 4, 5, 8, 10, 12):
            harness = Harness()
            harness.never_consume = bad_step
            with self.subTest(step=bad_step), self.assertRaisesRegex(screen.DropInputFailure, "not consumed"):
                harness.certify(4.0)
            self.assertEqual(len(harness.inputs()), bad_step)

    def test_repeat_cannot_silently_disarm_or_cancel_retain_source(self):
        for bad_step in (4, 5):
            harness = Harness()
            def broken(h, step):
                if step == bad_step:
                    if step == 4:
                        h.dropping = False
                    else:
                        h.source = 14
            harness.after_process = broken
            with self.subTest(step=bad_step), self.assertRaises(screen.DropInputFailure):
                harness.certify(3.0)
            self.assertEqual(len(harness.inputs()), bad_step)

    def test_stale_private_scope_owner_assignment_or_pose_before_confirmation_cancels_certificate(self):
        changes = (
            lambda h: h.owner().update(revision=99),
            lambda h: h.owner().update(baseline=99),
            lambda h: h.states[0].update(assigned=[[1, 1]]),
            lambda h: h.states[0]["actors"][0].update(ap=1),
            lambda h: h.states[0]["actors"][0].update(grid=200),
            lambda h: h.states[0].update(outstanding=77),
            lambda h: h.states[0].update(commands_blocked=True),
        )
        for index, change in enumerate(changes):
            harness = Harness()
            harness.after_process = lambda h, step: change(h) if step == 8 else None
            with self.subTest(change=index), self.assertRaises(screen.DropInputFailure):
                harness.certify()
            self.assertEqual(len(harness.inputs()), 8)

    def test_queue_receipt_schema_and_exact_scope_reject_ambiguity_without_replay(self):
        changes = (("queued", False), ("queued", 1), ("step", True), ("actor", [False, 1]),
                   ("item", [False, 0]), ("session", True), ("inventory_revision", 99),
                   ("queued_events", 0), ("queued_events", 3), ("previous_command", 77),
                   ("revision", True), ("outstanding", 1), ("request", True))
        for field, value in changes:
            harness = Harness()
            harness.change_reply = lambda h, reply: reply.update({field: value}) if h.step == 3 else None
            with self.subTest(field=field, value=value), self.assertRaisesRegex(screen.DropInputFailure, "ambiguous"):
                harness.certify()
            self.assertEqual(len(harness.inputs()), 3)

    def test_ui_schema_rejects_wrong_mode_identity_types_and_revision(self):
        changes = (("ready", False), ("drop_open", 1), ("ground_item", [False, 0]),
                   ("ground_count", True), ("queued_events", True), ("step", True),
                   ("inventory_revision", 99), ("actor", [False, 1]), ("revision", True))
        for field, value in changes:
            harness = Harness()
            harness.change_ui = lambda h, reply: reply.update({field: value}) if h.step == 4 else None
            with self.subTest(field=field, value=value), self.assertRaises(screen.DropInputFailure):
                harness.certify()
            self.assertEqual(len(harness.inputs()), 4)

    def test_drop_command_skip_rejection_noop_or_missing_receipt_never_retries(self):
        for failure in ("skip", "old", "reject", "queued", "noop", "missing"):
            harness = Harness()
            def broken(h, step):
                if step != 9:
                    return
                if failure == "skip":
                    h.states[0]["receipt"]["command"] += 1
                elif failure == "old":
                    h.states[0]["receipt"]["command"] -= 1
                elif failure == "reject":
                    h.states[0]["receipt"].update(status=2, reason=12)
                elif failure == "queued":
                    h.states[0]["receipt"].update(status=1)
                    h.states[0]["outstanding"] = 2
                elif failure == "noop":
                    h.states[0]["inventories"] = before_states()[0]["inventories"]
                else:
                    h.states[0]["receipt"] = None
            harness.after_process = broken
            with self.subTest(failure=failure), self.assertRaises(screen.DropInputFailure):
                harness.certify(3.0)
            self.assertEqual(len(harness.inputs()), 9)

    def test_pickup_ui_must_select_the_exact_dropped_identity_before_recovery_click(self):
        for wrong in ([99, 2], [100, 1], screen.INVALID_ITEM):
            harness = Harness()
            harness.after_process = lambda h, step: setattr(h, "item", wrong[:]) if step == 10 else None
            with self.subTest(item=wrong), self.assertRaises(screen.DropInputFailure):
                harness.certify(3.0)
            self.assertEqual(len(harness.inputs()), 10)

    def test_recovery_failure_or_wrong_slot_never_submits_another_gameplay_command(self):
        for failure in ("rejected", "wrong_slot", "old_ground", "changed_resource", "extra_ap"):
            harness = Harness()
            def broken(h, step):
                if step != 11:
                    return
                if failure == "rejected":
                    h.states[0]["receipt"].update(status=2, reason=12)
                elif failure == "wrong_slot":
                    slot = h.owner()["slots"][14]
                    h.owner()["slots"][15] = {**slot, "slot": 15}
                    h.owner()["slots"][14] = drop._empty_slot(14)
                elif failure == "old_ground":
                    h.owner()["ground"]["items"] = after_states()[0]["inventories"][0]["ground"]["items"]
                elif failure == "changed_resource":
                    h.owner()["slots"][14]["condition"] -= 1
                else:
                    for state in h.states:
                        state["actors"][0]["ap"] -= 1
            harness.after_process = broken
            with self.subTest(failure=failure), self.assertRaises(screen.DropInputFailure):
                harness.certify()
            self.assertEqual(len(harness.inputs()), 11)

    def test_driver_seam_is_queued_screen_only_and_consumes_before_enqueue(self):
        text = (Path(__file__).parent / "full_engine_coop_installed_client.cpp").read_text()
        body = text.split("void ReplyDropInput(", 1)[1].split("// INVENTORY_SWAP_CLICK", 1)[0]
        self.assertLess(body.index("sequence.step = step"), body.index("QueueEvent("))
        for required in ("BuildFullEngineCoopClientPresentationInventoryModel",
                         "ResolveFullEngineCoopClientPresentationInventoryClick", "KEY_REPEAT",
                         "QueueEvent(KEY_UP, 'x', 0)", "step != sequence.step + 1",
                         "sequence.assignments != presentation.assignedActors"):
            self.assertIn(required, body)
        for forbidden in ("sendIntent(", "inventoryDrop(", "inventoryPickup(", "selectInventorySource(",
                          "gpItemPointer", "OBJECTTYPE", "AddItemToPool", "TryDispatch"):
            self.assertNotIn(forbidden, body)

    def test_deadline_never_queues_a_late_confirmation_or_recovery(self):
        for step in (9, 11):
            harness = Harness()
            with patch.object(screen.time, "monotonic", return_value=5.0), \
                    self.subTest(step=step), self.assertRaisesRegex(screen.DropInputFailure, "deadline expired"):
                screen._queue(harness.probes[0], lambda: None, 0, (0, 1), 14,
                              harness.states, 4.0, step)
            self.assertFalse(harness.inputs())

    def test_extra_queue_atom_is_only_tolerated_for_native_mouse_double_click(self):
        harness = Harness()
        harness.change_reply = lambda h, reply: reply.update(queued_events=3) if h.step in (2, 6, 11) else None
        self.assertTrue(harness.certify()["restored"])

    def test_tab_selection_accepts_nonzero_prior_process_count_without_reset_or_extra_input(self):
        for count in (1, 3, 4, 0, 5, True):
            class Probe:
                calls = []
                def send(self, verb, *args):
                    self.calls.append((verb, args))
                    return 31
                def receive(self, *args):
                    return {"request": 31, "kind": "inventory_select_next", "queued": True,
                            "current": [0, 1], "target": [1, 2], "expected_next": [1, 2], "key_count": count,
                            "session": 1, "world": 2, "baseline": 1, "inventory_revision": 7, "queued_events": 2}
            probe = Probe()
            current = {"actor": [0, 1], "open": False, "baseline": 1, "inventory_revision": 7}
            target = {**current, "actor": [1, 2]}
            cache = {(0, 1): {"baseline": 1, "revision": 7}, (1, 2): {"baseline": 1, "revision": 7}}
            with patch.object(drop, "_caches", return_value=[cache, {}]), \
                    patch.object(screen, "_states", return_value=[]), \
                    patch.object(screen, "_same_read_only"), \
                    patch.object(screen, "_ui_read", side_effect=[current, target, target]), \
                    patch.object(screen.time, "monotonic", return_value=0.0), self.subTest(count=count):
                if type(count) is int and 1 <= count <= 4:
                    selected = screen._select_actor([probe, None], lambda: None, 0, (1, 2),
                                                    [{"session": 1, "world": 2}, {}], 1.0)
                    self.assertEqual(selected[0]["key_count"], count)
                else:
                    with self.assertRaises(screen.DropInputFailure):
                        screen._select_actor([probe, None], lambda: None, 0, (1, 2),
                                             [{"session": 1, "world": 2}, {}], 1.0)
                self.assertEqual(len(probe.calls), 1)


if __name__ == "__main__":
    unittest.main()
