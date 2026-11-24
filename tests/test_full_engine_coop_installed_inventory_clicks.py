"""Build-free checks for actual Screen inventory clicks and their native receipts."""

from __future__ import annotations

import copy
from contextlib import ExitStack
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

import full_engine_coop_installed_inventory as inventory
from test_full_engine_coop_installed_inventory import applied_states, held_kit_states


class ClickHarness:
    """Delay queued UI atoms independently from command execution/replication."""

    def __init__(self):
        self.snapshots = held_kit_states()
        # Real worn ordinary armor is swappable, not an inspection-only item.
        self.snapshots[0]["inventories"][0]["slots"][1].update(
            item=188, count=1, condition=97, support=1, status_kind=1)
        self.clock = 0.0
        self.commands = []
        self.identity = (0, 1)
        self.ui = {"ready": True, "open": True, "actor": [0, 1],
                   "session": 1, "world": 2, "baseline": 1, "inventory_revision": 1,
                   "source": inventory.NO_SLOT, "inspected": inventory.NO_SLOT,
                   "actions_enabled": True, "width": 640, "height": 480}
        self.source_delay = 0
        self.source_pending = None
        self.destination_pending = None
        self.destination_reads = 0
        self.destination_delay = 2
        self.click_count = 0
        self.command_increment = 1
        self.outcome = "applied"
        self.reply_change = None
        self.after_source = None
        self.after_click = None
        self.after_applied = None
        self.publish_queued = False
        self.select_pending = None
        self.select_delay = 1
        self.select_count = 0
        self.probes = [self.Probe(self, index) for index in range(2)]

    class Probe:
        def __init__(self, harness, sender):
            self.harness, self.sender = harness, sender

        def send(self, verb, *arguments):
            self.harness.commands.append((self.sender, verb, *arguments))
            return len(self.harness.commands)

        def receive(self, request, kind, deadline, guard):
            return self.harness.receive(self.harness.commands[request - 1], kind)

    def owner(self):
        return next(item for item in self.snapshots[0]["inventories"]
                    if tuple(item["actor"]) == self.identity)

    def receive(self, message, kind):
        sender, verb, *args = message
        if verb == "INVENTORY_UI":
            if self.source_pending is not None:
                if self.source_delay == 0:
                    selected = inventory.NO_SLOT if self.ui["source"] == self.source_pending else self.source_pending
                    self.ui.update(source=selected, inspected=self.source_pending)
                    self.source_pending = None
                    if self.after_source:
                        self.after_source(self)
                else:
                    self.source_delay -= 1
            if self.select_pending is not None:
                if self.select_delay == 0:
                    target = self.select_pending
                    self.ui.update(actor=list(target), source=inventory.NO_SLOT, inspected=inventory.NO_SLOT)
                    owner = next(item for item in self.snapshots[sender]["inventories"]
                                 if tuple(item["actor"]) == target)
                    self.ui.update(baseline=owner["baseline"], inventory_revision=owner["revision"])
                    self.select_pending = None
                else:
                    self.select_delay -= 1
            return {**copy.deepcopy(self.ui), "outstanding": self.snapshots[sender]["outstanding"],
                    "receipt": copy.deepcopy(self.snapshots[sender].get("receipt"))}
        if verb == "INVENTORY_INPUT":
            action, slot, inc, source, revision = args
            if (slot, inc) != self.identity or revision != self.owner()["revision"]:
                raise AssertionError("source click did not bind current owner inventory")
            if action == 1:
                self.ui["open"] = True
            elif action == 2:
                summary = self.owner()["slots"][source]
                if summary["support"] == 1 and (source <= 6 or 14 <= source < 55):
                    if self.ui["source"] not in (inventory.NO_SLOT, source):
                        raise AssertionError("inspection-only control attempted a gameplay destination")
                    self.source_pending = source
                else:
                    self.ui["inspected"] = source
            elif action == 4:
                self.ui.update(open=False, source=inventory.NO_SLOT, inspected=inventory.NO_SLOT)
            elif action != 3:
                raise AssertionError(f"unexpected source-control action {args}")
            return {"queued": True, "action": action,
                    "screenshot": inventory.UI_SCREENSHOT if action == 3 else ""}
        if verb == "INVENTORY_SWAP_CLICK":
            slot, inc, source, destination, session, world, baseline, revision = args
            if self.ui["source"] != source or self.ui["actor"] != [slot, inc]:
                raise AssertionError("destination queued without the exact visible source")
            self.click_count += 1
            previous = self.snapshots[sender].get("receipt")
            previous_command = previous["command"] if previous else 0
            reply = {"queued": True, "actor": [slot, inc], "source": source, "destination": destination,
                     "session": session, "world": world, "baseline": baseline,
                     "inventory_revision": revision, "revision": self.snapshots[sender]["revision"],
                     "outstanding": 0, "previous_command": previous_command,
                     "previous_status": previous["status"] if previous else 0,
                     "previous_reason": previous["reason"] if previous else 0,
                     "attempt": self.click_count, "queued_events": 2}
            self.destination_pending = (source, destination, previous_command + self.command_increment)
            self.destination_reads = 0
            if self.reply_change:
                self.reply_change(reply)
            if self.after_click:
                self.after_click(self)
            return reply
        if verb == "INVENTORY_SELECT_NEXT":
            slot, inc, target_slot, target_inc, session, world, baseline, revision = args
            assigned = self.snapshots[sender]["assigned"]
            current, target = [slot, inc], [target_slot, target_inc]
            expected_next = assigned[(assigned.index(current) + 1) % len(assigned)]
            self.select_pending = tuple(expected_next)
            self.select_count += 1
            reply = {"queued": True, "current": current, "target": target,
                     "expected_next": expected_next, "key_count": self.select_count,
                     "session": session, "world": world, "baseline": baseline,
                     "inventory_revision": revision, "queued_events": 2}
            if self.reply_change:
                self.reply_change(reply)
            return reply
        raise AssertionError(f"UI proof bypassed shipping click controls: {verb}")

    def observe(self, *_args):
        if self.destination_pending is not None:
            self.destination_reads += 1
            source, destination, command = self.destination_pending
            if self.destination_reads > self.destination_delay:
                if self.publish_queued and self.destination_reads == self.destination_delay + 1:
                    self.snapshots[0]["outstanding"] = command
                    self.snapshots[0]["receipt"] = {"command": command, "status": 1, "reason": 0,
                                                   "revision": self.snapshots[0]["revision"]}
                elif self.outcome == "applied":
                    other_receipt = copy.deepcopy(self.snapshots[1].get("receipt"))
                    self.snapshots = applied_states(self.snapshots, source=source,
                                                    destination=destination, command=command)
                    self.snapshots[1]["receipt"] = other_receipt
                    self.snapshots[0]["outstanding"] = 0
                    self.ui.update(inventory_revision=self.owner()["revision"],
                                   source=inventory.NO_SLOT, inspected=inventory.NO_SLOT)
                    self.destination_pending = None
                    if self.after_applied:
                        self.after_applied(self)
                elif self.outcome in ("stale", "gameplay"):
                    self.snapshots[0]["receipt"] = {"command": command, "status": 2,
                                                   "reason": 9 if self.outcome == "stale" else 12,
                                                   "revision": self.snapshots[0]["revision"]}
                    self.snapshots[0]["outstanding"] = 0
                    self.destination_pending = None
                elif self.outcome == "queued":
                    self.snapshots[0]["outstanding"] = command
                    self.snapshots[0]["receipt"] = {"command": command, "status": 1, "reason": 0,
                                                   "revision": self.snapshots[0]["revision"]}
                elif self.outcome != "missing":
                    raise AssertionError(f"unknown test outcome {self.outcome}")
        return copy.deepcopy(self.snapshots)

    def patches(self):
        stack = ExitStack()
        stack.enter_context(patch.object(inventory, "_states", side_effect=self.observe))
        stack.enter_context(patch.object(inventory.time, "monotonic", side_effect=lambda: self.clock))
        stack.enter_context(patch.object(inventory.time, "sleep",
                                        side_effect=lambda seconds: setattr(self, "clock", self.clock + seconds)))
        return stack

    def swap(self, source=6, destination=5, *, deadline=4.0, expected=None):
        expected = copy.deepcopy(self.owner()["slots"]) if expected is None else expected
        with self.patches():
            return inventory._ui_swap(self.probes, lambda: None, 0, self.identity,
                                      source, destination, expected, (1, 2), deadline)

    def mutating(self, verb="INVENTORY_SWAP_CLICK"):
        return [item for item in self.commands if item[1] == verb]


class InventoryClickOutcomeTests(unittest.TestCase):
    def test_delayed_source_and_destination_are_queued_once_before_exact_applied(self):
        harness = ClickHarness()
        harness.source_delay = 2
        evidence = harness.swap()
        self.assertEqual(len(harness.mutating("INVENTORY_INPUT")), 1)
        self.assertEqual(len(harness.mutating()), 1)
        self.assertEqual(evidence["command"], 2)
        self.assertEqual(evidence["primary_after"]["item"], 204)
        self.assertEqual(evidence["inventory_revision_after"], 2)
        self.assertFalse(any(command[1] == "SWAP" for command in harness.commands))

    def test_two_real_clicks_equip_then_restore_exact_original_slots_and_primary(self):
        harness = ClickHarness()
        original = copy.deepcopy(harness.owner()["slots"])
        primary = copy.deepcopy(harness.snapshots[0]["actors"][0]["primary"])
        equipped = harness.swap()
        restored = harness.swap(5, 6, deadline=8.0)
        self.assertEqual([equipped["command"], restored["command"]], [2, 3])
        self.assertEqual(len(harness.mutating()), 2)
        self.assertEqual(restored["slots_after"], original)
        self.assertEqual(restored["primary_after"], primary)
        self.assertTrue(equipped["slot_metric_conservation_verified"])
        self.assertEqual(equipped["slots_after"][5]["status_kind"], inventory.TOOL_KIT_POINTS)
        self.assertEqual(equipped["slots_after"][5]["resource_total"], 99)
        self.assertEqual(restored["slots_after"][25]["resource_total"], 12)

    def test_terminal_rejection_queued_and_missing_outcome_never_replay_ui_click(self):
        for outcome in ("stale", "gameplay", "queued", "missing"):
            harness = ClickHarness()
            harness.outcome = outcome
            with self.subTest(outcome=outcome), self.assertRaises(inventory.InventoryFailure):
                harness.swap(deadline=1.0)
            self.assertEqual(len(harness.mutating()), 1)

    def test_skipped_or_old_command_identity_never_proves_this_click(self):
        for increment in (0, 2, 7):
            harness = ClickHarness()
            harness.command_increment = increment
            with self.subTest(increment=increment), self.assertRaises(inventory.InventoryFailure):
                harness.swap(deadline=1.0)
            self.assertEqual(len(harness.mutating()), 1)

    def test_preexisting_lock_or_nonterminal_receipt_refuses_destination_click(self):
        for problem in ("sender_lock", "other_lock", "blocked", "queued", "rejected"):
            harness = ClickHarness()
            if problem == "sender_lock":
                harness.snapshots[0]["outstanding"] = 1
            elif problem == "other_lock":
                harness.snapshots[1]["outstanding"] = 1
            elif problem == "blocked":
                harness.snapshots[1]["commands_blocked"] = True
            elif problem == "queued":
                harness.snapshots[0]["receipt"]["status"] = 1
            else:
                harness.snapshots[0]["receipt"].update(status=2, reason=9)
            with self.subTest(problem=problem), self.assertRaises(inventory.MovementFailure):
                harness.swap(deadline=1.0)
            self.assertEqual(harness.mutating(), [])

    def test_source_selection_drift_cannot_be_reinterpreted_as_destination_selection(self):
        for problem in ("source", "actor", "baseline", "world", "revision", "closed"):
            harness = ClickHarness()
            def drift(value):
                if problem == "source":
                    value.ui["source"] = 25
                elif problem == "actor":
                    value.ui["actor"] = [1, 2]
                elif problem == "closed":
                    value.ui["open"] = False
                else:
                    key = "inventory_revision" if problem == "revision" else problem
                    value.ui[key] += 1
            harness.after_source = drift
            with self.subTest(problem=problem), self.assertRaises(inventory.InventoryFailure):
                harness.swap(deadline=1.0)
            self.assertEqual(harness.mutating(), [])
            self.assertLessEqual(len(harness.mutating("INVENTORY_INPUT")), 1)

    def test_queued_reply_must_bind_exact_scope_slots_and_previous_terminal(self):
        for field, value in (("queued", False), ("actor", [1, 2]), ("source", 25),
                             ("destination", 26), ("session", 2), ("world", 3), ("baseline", 2),
                             ("inventory_revision", 2), ("outstanding", 1), ("previous_command", 9),
                             ("previous_status", 1), ("previous_reason", 9), ("attempt", 3),
                             ("queued_events", 1), ("queued_events", 4), ("queued_events", True)):
            harness = ClickHarness()
            harness.reply_change = lambda reply: reply.update({field: value})
            with self.subTest(field=field), self.assertRaises(inventory.InventoryFailure):
                harness.swap(deadline=1.0)
            self.assertEqual(len(harness.mutating()), 1)

    def test_changed_owner_slots_never_queue_the_planned_swap(self):
        harness = ClickHarness()
        expected = copy.deepcopy(harness.owner()["slots"])
        harness.owner()["slots"][6]["condition"] -= 1
        with self.assertRaises(inventory.InventoryFailure):
            harness.swap(expected=expected)
        self.assertEqual(harness.mutating(), [])

    def test_world_baseline_and_owner_drift_after_queue_never_allow_replay(self):
        for problem in ("world", "baseline", "owner"):
            harness = ClickHarness()
            def drift(value):
                if problem == "world":
                    for state in value.snapshots:
                        state["world"] += 1
                elif problem == "baseline":
                    value.owner()["baseline"] += 1
                else:
                    value.snapshots[0]["assigned"] = [[1, 2]]
            harness.after_click = drift
            with self.subTest(problem=problem), self.assertRaises(inventory.MovementFailure):
                harness.swap(deadline=1.0)
            self.assertEqual(len(harness.mutating()), 1)

    def test_queued_receipt_then_delayed_applied_uses_one_click(self):
        harness = ClickHarness()
        harness.publish_queued = True
        evidence = harness.swap()
        self.assertEqual(evidence["command"], 2)
        self.assertEqual(evidence["input"], "shipping-source-and-destination-clicks")
        self.assertEqual(len(harness.mutating()), 1)

    def test_other_peer_command_after_queued_receipt_invalidates_isolated_ui_proof(self):
        harness = ClickHarness()
        harness.publish_queued = True
        harness.after_applied = lambda value: value.snapshots[1]["receipt"].update(command=9)
        with self.assertRaises(inventory.InventoryFailure):
            harness.swap()
        self.assertEqual(len(harness.mutating()), 1)

    def test_unrelated_sender_receipt_after_queued_does_not_replace_this_action(self):
        harness = ClickHarness()
        harness.publish_queued = True
        harness.after_applied = lambda value: value.snapshots[0]["receipt"].update(command=9)
        with self.assertRaises(inventory.InventoryFailure):
            harness.swap()
        self.assertEqual(len(harness.mutating()), 1)

    def test_absent_initial_receipt_requires_exact_command_one(self):
        harness = ClickHarness()
        harness.snapshots[0]["receipt"] = None
        self.assertEqual(harness.swap()["command"], 1)


class InventorySelectActorTests(unittest.TestCase):
    def harness(self, count=4):
        harness = ClickHarness()
        harness.ui["open"] = False
        for slot in range(2, count + 1):
            identity = [slot, slot + 1]
            harness.snapshots[0]["assigned"].append(identity)
            owner = copy.deepcopy(harness.snapshots[0]["inventories"][0])
            owner["actor"] = identity[:]
            harness.snapshots[0]["inventories"].append(owner)
            for state in harness.snapshots:
                actor = copy.deepcopy(state["actors"][0])
                actor.update(slot=slot, inc=slot + 1)
                state["actors"] = [current for current in state["actors"] if current["slot"] != slot]
                state["actors"].append(actor)
        return harness

    def select(self, harness, identity, deadline=3.0):
        with harness.patches():
            return inventory._ui_select_actor(harness.probes, lambda: None, 0, identity, (1, 2), deadline)

    def test_existing_selected_owner_never_queues_tab(self):
        harness = self.harness()
        self.select(harness, (0, 1))
        self.assertEqual(harness.mutating("INVENTORY_SELECT_NEXT"), [])

    def test_tab_cycle_waits_each_visible_next_actor_and_stays_within_four_inputs(self):
        harness = self.harness()
        selected = self.select(harness, (4, 5))
        self.assertEqual(selected["actor"], [4, 5])
        self.assertEqual(len(harness.mutating("INVENTORY_SELECT_NEXT")), 3)
        self.assertEqual(harness.mutating(), [])
        self.assertEqual(harness.mutating("INVENTORY_INPUT"), [])

    def test_missing_delayed_tab_never_replays_queued_key(self):
        harness = self.harness()
        harness.select_delay = 10000
        with self.assertRaises(inventory.InventoryFailure):
            self.select(harness, (2, 3), deadline=0.5)
        self.assertEqual(len(harness.mutating("INVENTORY_SELECT_NEXT")), 1)

    def test_four_input_bound_prevents_fifth_cycle_step(self):
        harness = self.harness(count=6)
        with self.assertRaisesRegex(inventory.InventoryFailure, "four"):
            self.select(harness, (6, 7))
        self.assertEqual(len(harness.mutating("INVENTORY_SELECT_NEXT")), 4)

    def test_foreign_target_open_panel_or_stale_scope_never_queues_tab(self):
        for problem in ("foreign", "open", "baseline", "owner", "locked"):
            harness = self.harness()
            target = (2, 3)
            if problem == "foreign":
                target = (1, 2)
            elif problem == "open":
                harness.ui["open"] = True
            elif problem == "baseline":
                harness.ui["baseline"] += 1
            elif problem == "owner":
                harness.ui["actor"] = [1, 2]
            else:
                harness.snapshots[1]["outstanding"] = 1
            with self.subTest(problem=problem), self.assertRaises(inventory.InventoryFailure):
                self.select(harness, target)
            self.assertEqual(harness.mutating("INVENTORY_SELECT_NEXT"), [])

    def test_wrong_next_actor_or_key_count_is_rejected_without_second_tab(self):
        for field, value in (("queued", False), ("current", [1, 2]), ("target", [3, 4]),
                             ("expected_next", [1, 2]), ("key_count", 5), ("session", 2),
                             ("world", 3), ("baseline", 2), ("inventory_revision", 2),
                             ("queued_events", 1), ("queued_events", 3)):
            harness = self.harness()
            harness.reply_change = lambda reply: reply.update({field: value})
            with self.subTest(field=field), self.assertRaises(inventory.InventoryFailure):
                self.select(harness, (2, 3))
            self.assertEqual(len(harness.mutating("INVENTORY_SELECT_NEXT")), 1)


class InventoryClickPostfixTests(unittest.TestCase):
    def readonly_postfix(self, harness):
        harness.ui["open"] = False
        with tempfile.TemporaryDirectory(prefix="ja2-inventory-inspect-unit-") as directory, harness.patches(), \
                patch.object(inventory, "_wait_inventory_png", return_value=512), patch("builtins.print"):
            client = SimpleNamespace(log_path=Path(directory) / "client.stdout.log")
            return inventory._certify_inventory_ui(harness.probes, lambda: None, [client, client],
                                                   0, (1, 2), 20.0)

    def test_ordinary_worn_gear_is_never_an_inspection_only_destination(self):
        harness = ClickHarness()
        for index in range(5):
            harness.owner()["slots"][index].update(item=188 + index, count=1, condition=97,
                                                  support=1, status_kind=1)
        original = copy.deepcopy(harness.snapshots)
        evidence = self.readonly_postfix(harness)
        inspections = [message[5] for message in harness.mutating("INVENTORY_INPUT") if message[2] == 2]
        self.assertEqual(inspections, [6, 7, 6])
        self.assertEqual([item["source"] for item in evidence["inspections"]], [6, 6, inventory.NO_SLOT])
        self.assertTrue(evidence["unsupported_inspected"])
        self.assertEqual(harness.mutating(), [])
        self.assertEqual(harness.snapshots, original)

    def test_niv_prefers_real_worn_lbe_slot_even_when_its_object_summary_is_ordinary(self):
        harness = ClickHarness()
        harness.owner()["slots"][0].update(item=991, count=1, condition=97, support=2)
        harness.owner()["slots"][7].update(support=1, status_kind=1)
        evidence = self.readonly_postfix(harness)
        self.assertEqual(evidence["inspections"][1]["slot"]["slot"], 7)
        self.assertEqual(evidence["inspections"][1]["source"], 6)
        self.assertTrue(evidence["no_gameplay_submission"])

    def test_oiv_inspects_only_a_real_visible_opaque_slot_and_never_hidden_lbe(self):
        for opaque in (False, True):
            harness = ClickHarness()
            harness.owner()["new_inventory"] = False
            if opaque:
                harness.owner()["slots"][25].update(support=2, status_kind=0, resource_total=0)
            with self.subTest(opaque=opaque):
                evidence = self.readonly_postfix(harness)
                inspections = [message[5] for message in harness.mutating("INVENTORY_INPUT") if message[2] == 2]
                self.assertEqual(inspections, [6, 25, 6] if opaque else [6])
                self.assertEqual(evidence["unsupported_inspected"], opaque)
                self.assertEqual(harness.mutating(), [])

    def test_readonly_inspection_command_or_receipt_change_fails_before_any_followup_input(self):
        for change in ("outstanding", "rejected", "applied"):
            harness = ClickHarness()
            original_receive = harness.receive

            def corrupt(message, kind):
                result = original_receive(message, kind)
                if message[1] == "INVENTORY_INPUT" and message[2] == 2 and message[5] == 7:
                    state = harness.snapshots[0]
                    if change == "outstanding":
                        state["outstanding"] = state["receipt"]["command"] + 1
                        harness.ui["actions_enabled"] = False
                    else:
                        state["receipt"]["command"] += 1
                        state["receipt"].update(status=2 if change == "rejected" else 3,
                                                  reason=12 if change == "rejected" else 0)
                return result

            harness.receive = corrupt
            with self.subTest(change=change), self.assertRaisesRegex(inventory.InventoryFailure, "read-only inventory input"):
                self.readonly_postfix(harness)
            inputs = harness.mutating("INVENTORY_INPUT")
            self.assertEqual([(message[2], message[5]) for message in inputs], [(1, inventory.NO_SLOT), (2, 6), (2, 7)])
            self.assertEqual(harness.mutating(), [])

    def test_mutating_postfix_equips_restores_then_keeps_inspection_capture_close_read_only(self):
        harness = ClickHarness()
        harness.ui["open"] = False
        original = copy.deepcopy(harness.owner()["slots"])
        with tempfile.TemporaryDirectory(prefix="ja2-inventory-click-unit-") as directory, harness.patches(), \
                patch.object(inventory, "_wait_inventory_png", return_value=512), patch("builtins.print"):
            client = SimpleNamespace(log_path=Path(directory) / "client.stdout.log")
            evidence = inventory._certify_inventory_ui(harness.probes, lambda: None, [client, client],
                                                      0, (1, 2), 20.0,
                                                      swap_plan=(harness.identity, 6, original))
        self.assertTrue(evidence["closed"])
        self.assertTrue(evidence["read_only_postfix"])
        self.assertFalse(evidence["no_gameplay_submission"])
        self.assertEqual(len(evidence["swaps"]), 2)
        self.assertEqual(len(harness.mutating()), 2)
        self.assertEqual(harness.owner()["slots"], original)
        self.assertEqual(harness.owner()["revision"], 3)
        self.assertEqual(evidence["captured_status_kind"], inventory.TOOL_KIT_POINTS)
        self.assertEqual(evidence["captured_resource_total"], 99)
        self.assertTrue(evidence["unsupported_inspected"])
        self.assertTrue(evidence["inspections"][-1]["same_source_cancel"])
        inputs = [command for command in harness.commands if command[1] == "INVENTORY_INPUT"]
        self.assertEqual(len(inputs), 8)  # Shipping driver cap is unchanged.

    def test_default_postfix_remains_read_only_without_swap_plan(self):
        harness = ClickHarness()
        harness.ui["open"] = False
        original = copy.deepcopy(harness.snapshots)
        with tempfile.TemporaryDirectory(prefix="ja2-inventory-click-unit-") as directory, harness.patches(), \
                patch.object(inventory, "_wait_inventory_png", return_value=512), patch("builtins.print"):
            client = SimpleNamespace(log_path=Path(directory) / "client.stdout.log")
            evidence = inventory._certify_inventory_ui(harness.probes, lambda: None, [client, client],
                                                      0, (1, 2), 20.0)
        self.assertTrue(evidence["no_gameplay_submission"])
        self.assertEqual(evidence["swaps"], [])
        self.assertEqual(harness.mutating(), [])
        self.assertEqual(harness.snapshots, original)
        self.assertEqual(evidence["captured_status_kind"], inventory.TOOL_KIT_POINTS)


if __name__ == "__main__":
    unittest.main()
