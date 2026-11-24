"""Data-free outcome, privacy and at-most-once checks for inventory probes."""

import copy
from contextlib import ExitStack
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zlib

import full_engine_coop_installed_inventory as inventory
from test_full_engine_coop_installed_fire import primary, states


def initial_states():
    result = states(revision=10)
    for state in result:
        identity = state["assigned"][0]
        slots = [{"slot": index, "item": 0, "count": 0, "condition": 0, "support": 0,
                  "status_kind": 0, "resource_total": 0}
                 for index in range(55)]
        hand = state["actors"][identity[0]]["primary"]
        slots[5].update(item=hand["item"], count=hand["quantity"], condition=hand["condition"], support=1, status_kind=1)
        slots[14].update(item=991, count=1, condition=90, support=1, status_kind=1)
        state["inventories"] = [{"session": 1, "world": 2, "baseline": 1, "revision": 1,
                                  "actor": identity[:], "new_inventory": True, "slots": slots}]
    return result


def held_kit_states():
    """Shape of the real installed loadout: offhand kit and a two-loader stack."""
    result = initial_states()
    for state in result:
        slots = state["inventories"][0]["slots"]
        slots[14].update(item=0, count=0, condition=0, support=0, status_kind=0)
        slots[6].update(item=204, count=1, condition=99, support=1, status_kind=4, resource_total=99)
        slots[7].update(item=1090, count=1, condition=97, support=2)
        slots[8].update(item=1200, count=1, condition=96, support=2)
        slots[25].update(item=77, count=2, condition=6, support=1, status_kind=2, resource_total=12)
    return result


def applied_states(before=None, *, source=14, destination=5, command=2, public_change=True):
    result = copy.deepcopy(before or initial_states())
    current = result[0]["inventories"][0]
    current["slots"] = inventory._swap_slots(current["slots"], source, destination)
    current["revision"] += 1
    for state in result:
        if public_change:
            state["revision"] += 1
            state["actors"][0]["ap"] -= 2
        state["receipt"] = {"command": command, "status": 3, "reason": 0, "revision": state["revision"]}
        if inventory.HAND in (source, destination):
            slot = current["slots"][inventory.HAND]
            state["actors"][0]["primary"] = ({**primary(), "condition": slot["condition"],
                                             "quantity": slot["count"]}
                                            if slot["item"] == primary()["item"] else {
                "item": slot["item"], "quantity": slot["count"], "condition": slot["condition"],
                "ammo_state": False, "ammo_item": 0, "ammo_count": 0,
                "ammo_condition": 0, "chambered": False, "jammed": False})
    return result


class InventoryOutcomeTests(unittest.TestCase):
    def wait(self, observed=None, *, sequence=None, before=None, source=14, destination=5):
        snapshots = sequence or [observed or applied_states()]
        clock, reads = [0.0], [0]

        def observe(*_args):
            sample = snapshots[min(reads[0], len(snapshots) - 1)]
            reads[0] += 1
            return copy.deepcopy(sample)

        with patch.object(inventory, "_states", side_effect=observe), \
                patch.object(inventory.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(inventory.time, "sleep", side_effect=lambda seconds: clock.__setitem__(0, clock[0] + seconds)):
            try:
                return inventory._wait_swap([None, None], lambda: None, 0, 2, (0, 1), source,
                                            destination, before or initial_states(), 1.1)
            finally:
                self.elapsed, self.reads = clock[0], reads[0]

    def test_actual_equip_waits_for_private_revision_both_hands_and_stability(self):
        outcome, evidence = self.wait()
        self.assertEqual(outcome, "applied")
        self.assertEqual(evidence["inventory_revision_after"], 2)
        self.assertEqual(evidence["primary_after"]["item"], 991)
        self.assertGreaterEqual(self.elapsed, 0.5)
        self.assertGreater(self.reads, 1)
        self.assertTrue(evidence["slot_metric_conservation_verified"])

    def test_typed_stack_totals_survive_equip_and_pocket_roundtrips(self):
        before = held_kit_states()
        equipped = applied_states(before, source=6, destination=5)
        outcome, evidence = self.wait(equipped, before=before, source=6, destination=5)
        self.assertEqual(outcome, "applied")
        self.assertEqual((evidence["slots_after"][5]["status_kind"],
                          evidence["slots_after"][5]["resource_total"]), (4, 99))
        pocket = applied_states(before, source=25, destination=26, public_change=False)
        _, evidence = self.wait(pocket, before=before, source=25, destination=26)
        self.assertEqual((evidence["slots_after"][26]["status_kind"],
                          evidence["slots_after"][26]["resource_total"]), (2, 12))

    def test_changed_resource_total_or_kind_cannot_hide_behind_unchanged_first_status(self):
        for change in ({"resource_total": 11}, {"status_kind": 4}):
            with self.subTest(change=change):
                before = held_kit_states()
                observed = applied_states(before, source=25, destination=26, public_change=False)
                observed[0]["inventories"][0]["slots"][26].update(change)
                with self.assertRaises(inventory.InventoryFailure):
                    self.wait(observed, before=before, source=25, destination=26)

    def test_metric_wire_validation_rejects_noncanonical_values(self):
        base = held_kit_states()[0]["inventories"][0]["slots"][25]
        invalid = ({"status_kind": 5}, {"status_kind": True}, {"resource_total": True},
                   {"resource_total": -1}, {"resource_total": 4294967296},
                   {"resource_total": 5}, {"resource_total": 65542},
                   {"status_kind": 0}, {"status_kind": 1}, {"support": 2})
        for change in invalid:
            with self.subTest(change=change):
                slot = {**base, **change}
                self.assertFalse(inventory._valid_slot(slot, 25))
        for key in ("status_kind", "resource_total"):
            slot = copy.deepcopy(base)
            del slot[key]
            self.assertFalse(inventory._valid_slot(slot, 25))

    def test_metric_kind_distinguishes_unsigned_ammo_medical_tool_and_condition(self):
        base = held_kit_states()[0]["inventories"][0]["slots"][25]
        self.assertTrue(inventory._valid_slot({**base, "condition": -1,
                                              "resource_total": 65541}, 25))
        for kind in (3, 4):
            for total, valid in ((99, False), (100, True), (200, True), (201, False)):
                with self.subTest(kind=kind, total=total):
                    self.assertEqual(inventory._valid_slot(
                        {**base, "status_kind": kind, "condition": 100, "resource_total": total}, 25), valid)
        self.assertTrue(inventory._valid_slot({**base, "status_kind": 1,
                                              "condition": 100, "resource_total": 0}, 25))
        self.assertFalse(inventory._valid_slot({**base, "status_kind": 1,
                                               "condition": 101, "resource_total": 0}, 25))

    def test_pocket_only_needs_no_public_delta_or_ap_cost(self):
        observed = applied_states(destination=15, public_change=False)
        outcome, evidence = self.wait(observed, destination=15)
        self.assertEqual(outcome, "applied")
        self.assertEqual(evidence["revisions_before"], evidence["revisions_after"])
        self.assertEqual(evidence["ap_before"], evidence["ap_after"])

    def test_applied_without_changed_inventory_is_not_proof(self):
        observed = initial_states()
        observed[0]["receipt"]["command"] = 2
        with self.assertRaises(inventory.InventoryFailure):
            self.wait(observed)

    def test_no_private_revision_advance_or_only_one_public_replica_is_not_proof(self):
        for kind in ("private-revision", "public-hand", "public-revision"):
            with self.subTest(kind=kind):
                observed = applied_states()
                if kind == "private-revision":
                    observed[0]["inventories"][0]["revision"] = 1
                elif kind == "public-hand":
                    observed[1]["actors"][0]["primary"] = primary()
                else:
                    observed[1]["revision"] = 10
                with self.assertRaises(inventory.InventoryFailure):
                    self.wait(observed)

    def test_unrelated_slot_mutation_or_quantity_loss_does_not_certify(self):
        for slot in (5, 14, 20):
            with self.subTest(slot=slot):
                observed = applied_states()
                observed[0]["inventories"][0]["slots"][slot].update(item=996, count=1, support=1)
                with self.assertRaises(inventory.InventoryFailure):
                    self.wait(observed)

    def test_queued_wrong_receipt_and_locked_or_blocked_states_do_not_certify(self):
        for kind in ("queued", "wrong-command", "lock", "blocked", "other-lock"):
            with self.subTest(kind=kind):
                observed = applied_states()
                if kind == "queued":
                    observed[0]["receipt"]["status"] = 1
                elif kind == "wrong-command":
                    observed[0]["receipt"]["command"] = 1
                else:
                    observed[1 if kind == "other-lock" else 0][
                        "commands_blocked" if kind == "blocked" else "outstanding"] = 2
                with self.assertRaises(inventory.InventoryFailure):
                    self.wait(observed)

    def test_owner_only_cache_rejects_foreign_duplicate_and_wrong_scope(self):
        for kind in ("foreign", "duplicate", "world", "session", "baseline"):
            with self.subTest(kind=kind):
                observed = applied_states()
                if kind == "foreign":
                    observed[1]["inventories"].append(copy.deepcopy(observed[0]["inventories"][0]))
                elif kind == "duplicate":
                    observed[0]["inventories"].append(copy.deepcopy(observed[0]["inventories"][0]))
                else:
                    observed[0]["inventories"][0][kind] += 1
                with self.assertRaises(inventory.InventoryFailure):
                    self.wait(observed)

    def test_gameplay_rejection_never_looks_like_stale_retry(self):
        observed = initial_states()
        observed[0]["receipt"].update(command=2, status=2, reason=12)
        with self.assertRaisesRegex(inventory.InventoryFailure, "reason=12"):
            self.wait(observed)

    def test_only_side_effect_free_stale_rejection_releases_retry(self):
        for reason in (5, 9):
            with self.subTest(reason=reason):
                observed = initial_states()
                observed[0]["receipt"].update(command=2, status=2, reason=reason)
                self.assertEqual(self.wait(observed), ("stale", None))
                self.assertGreaterEqual(self.elapsed, 0.3)

    def test_delayed_rejection_side_effects_and_applied_rejection_conflict_fail(self):
        rejected = initial_states()
        rejected[0]["receipt"].update(command=2, status=2, reason=9)
        changed = copy.deepcopy(rejected)
        changed[0]["inventories"][0]["slots"][14]["condition"] -= 1
        for sequence in ([rejected, changed], [applied_states(), rejected], [rejected, applied_states()]):
            with self.subTest(sequence=sequence), self.assertRaises(inventory.InventoryFailure):
                self.wait(sequence=sequence)

    def test_terminal_outcome_cannot_regress_to_queued_before_conservation(self):
        rejected = initial_states()
        rejected[0]["receipt"].update(command=2, status=2, reason=9)
        for terminal in (applied_states(), rejected):
            queued = copy.deepcopy(terminal)
            queued[0]["receipt"].update(status=1, reason=0)
            with self.subTest(status=terminal[0]["receipt"]["status"]), \
                    self.assertRaises(inventory.InventoryFailure):
                self.wait(sequence=[terminal, queued])

    def test_terminal_receipt_cannot_disappear_or_regress_to_an_older_command(self):
        rejected = initial_states()
        rejected[0]["receipt"].update(command=2, status=2, reason=9)
        for terminal in (applied_states(), rejected):
            for receipt in (None, initial_states()[0]["receipt"]):
                changed = copy.deepcopy(terminal)
                changed[0]["receipt"] = receipt
                with self.subTest(status=terminal[0]["receipt"]["status"], receipt=receipt), \
                        self.assertRaises(inventory.InventoryFailure):
                    self.wait(sequence=[terminal, changed])

    def test_phase_loss_is_sticky_and_does_not_recertify_combat_ap(self):
        changed = applied_states()
        for state in changed:
            state["turn_serial"] = 2
        outcome, evidence = self.wait(sequence=[changed, applied_states()])
        self.assertEqual(outcome, "applied")
        self.assertFalse(evidence["same_combat_ap_phase"])

    def test_candidate_waits_for_both_owner_caches_and_excludes_complex_items(self):
        observed = initial_states()
        self.assertEqual(inventory._candidate(observed), (0, (0, 1), 14, 14, 15))
        observed[1]["inventories"] = []
        self.assertIsNone(inventory._candidate(observed))
        observed = initial_states()
        for state in observed:
            state["inventories"][0]["slots"][14].update(support=2, status_kind=0)
        self.assertIsNone(inventory._candidate(observed))

    def test_candidate_decouples_offhand_equip_from_complete_reserve_stack(self):
        observed = held_kit_states()
        self.assertEqual(inventory._candidate(observed), (0, (0, 1), 6, 25, 26))
        # A pocket item remains a fallback, but the ordinary offhand is preferred.
        observed[0]["inventories"][0]["slots"][14].update(item=991, count=1, condition=90, support=1)
        candidate = inventory._candidate(observed)
        self.assertEqual(candidate[2], 6)

    def test_candidate_never_splits_a_reserve_stack_for_hand_equip(self):
        observed = held_kit_states()
        for state in observed:
            state["inventories"][0]["slots"][6].update(item=0, count=0, condition=0, support=0, status_kind=0, resource_total=0)
        self.assertIsNone(inventory._candidate(observed))

    def test_candidate_does_not_cross_gear_or_use_unreplicated_backpack_access(self):
        for source in (17, 47):
            observed = held_kit_states()
            for state in observed:
                slots = state["inventories"][0]["slots"]
                slots[source].update(item=77, count=2, condition=6, support=1, status_kind=2, resource_total=12)
                slots[25].update(item=0, count=0, condition=0, support=0, status_kind=0, resource_total=0)
            with self.subTest(source=source):
                self.assertIsNone(inventory._candidate(observed))
        observed = held_kit_states()
        for state in observed:
            slots = state["inventories"][0]["slots"]
            slots[25].update(item=0, count=0, condition=0, support=0, status_kind=0, resource_total=0)
            slots[34].update(item=77, count=2, condition=6, support=1, status_kind=2, resource_total=12)
            slots[33].update(item=996, count=1, condition=50, support=2)
        self.assertIsNone(inventory._candidate(observed))


class InventoryDriverTests(unittest.TestCase):
    def certify(self, *, stale=False, reject=False, ambiguous=False, no_mutation=False, held_kit=False,
                readiness_snapshot=None, readiness_error=None, readiness_floor=None, readiness_seconds=0):
        snapshots = held_kit_states() if held_kit else initial_states()
        clock = [0.0]
        self.commands = []
        next_command = [1]
        probes = []

        def readiness(_clients, _server, timeout):
            nonlocal snapshots
            self.assertEqual(probes, [])
            self.assertEqual(self.commands, [])
            self.assertEqual(timeout, 40)
            clock[0] += readiness_seconds
            if readiness_error is not None:
                raise readiness_error
            if readiness_snapshot is not None:
                snapshots = copy.deepcopy(readiness_snapshot)
                next_command[0] = snapshots[0]["receipt"]["command"]
            return {"session": snapshots[0]["session"], "world": snapshots[0]["world"],
                    "interrupt_passes": [{"sender": 2, "actor": [1, 2], "command": 20,
                                          "interrupt_serial": 7}] if readiness_snapshot else [],
                    "submitted_attempts": 1 if readiness_snapshot else 0,
                    "final": {"revisions": [state["revision"] if readiness_floor is None else readiness_floor
                                            for state in snapshots]}}

        class Probe:
            def __init__(probe, _client):
                probe.sender = len(probes)
                probes.append(probe)

            def send(probe, verb, *arguments):
                self.commands.append((probe.sender, verb, *arguments))
                return len(self.commands)

            def receive(probe, request, _kind, _deadline, _guard):
                nonlocal snapshots
                _, _, _slot, _inc, source, destination, revision = self.commands[-1]
                next_command[0] += 1
                command = 2 if ambiguous else next_command[0]
                if stale:
                    snapshots[0]["receipt"].update(command=command, status=2, reason=9)
                elif no_mutation:
                    snapshots[0]["receipt"].update(command=command, status=3, reason=0)
                elif not no_mutation and not reject:
                    snapshots = applied_states(snapshots, source=source, destination=destination,
                                               command=command, public_change=5 in (source, destination))
                return {"submitted": not reject, "command": command if not reject else 0,
                        "inventory_revision": revision}

        with ExitStack() as stack:
            stack.enter_context(patch.object(inventory, "wait_for_two_client_player_control", side_effect=readiness))
            stack.enter_context(patch.object(inventory, "_Probe", Probe))
            stack.enter_context(patch.object(inventory, "_certify_inventory_ui", return_value={"closed": True}))
            stack.enter_context(patch.object(inventory, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(inventory, "_states", side_effect=lambda *_args: copy.deepcopy(snapshots)))
            stack.enter_context(patch.object(inventory.time, "monotonic", side_effect=lambda: clock[0]))
            stack.enter_context(patch.object(inventory.time, "sleep", side_effect=lambda seconds: clock.__setitem__(0, clock[0] + seconds)))
            return inventory.certify_two_client_inventory([object(), object()], object(), 40)

    def test_public_flow_equips_restores_then_moves_pocket_and_restores(self):
        evidence = self.certify()
        self.assertTrue(evidence["restored"])
        self.assertEqual(evidence["submitted_attempts"], 4)
        self.assertEqual([action["action"] for action in evidence["actions"]],
                         ["equip", "restore-hand", "pocket-only", "restore-pocket"])
        self.assertEqual([(command[4], command[5]) for command in self.commands],
                         [(14, 5), (5, 14), (14, 15), (15, 14)])
        self.assertEqual(evidence["actions"][-1]["primary_after"], primary())
        self.assertEqual(evidence["actions"][-1]["slots_after"], initial_states()[0]["inventories"][0]["slots"])

    def test_only_stale_is_bounded_to_eight_fresh_command_ids(self):
        with self.assertRaisesRegex(inventory.InventoryFailure, "bounded"):
            self.certify(stale=True)
        self.assertEqual(len(self.commands), 8)

    def test_public_flow_equips_real_held_kit_then_moves_entire_ammo_stack(self):
        evidence = self.certify(held_kit=True)
        self.assertEqual(evidence["submitted_attempts"], 4)
        self.assertEqual([(command[4], command[5]) for command in self.commands],
                         [(6, 5), (5, 6), (25, 26), (26, 25)])
        self.assertEqual(evidence["actions"][0]["primary_after"]["item"], 204)
        self.assertEqual(evidence["actions"][2]["slots_after"][26]["count"], 2)
        self.assertEqual(evidence["actions"][-1]["slots_after"], held_kit_states()[0]["inventories"][0]["slots"])
        self.assertEqual(evidence["actions"][-1]["primary_after"], primary())

    def test_readiness_resamples_inventory_ap_and_command_cursor_then_records_passes_separately(self):
        refreshed = held_kit_states()
        for state in refreshed:
            state["revision"] = 30
            state["receipt"]["command"] = 21
            state["actors"][0]["ap"] = 63
            state["actors"][0]["primary"]["condition"] = 86
        owner = refreshed[0]["inventories"][0]
        owner["revision"] = 9
        owner["slots"][5]["condition"] = 86
        evidence = self.certify(held_kit=True, readiness_snapshot=refreshed)
        self.assertEqual(evidence["readiness"]["submitted_attempts"], 1)
        self.assertEqual(evidence["readiness"]["interrupt_passes"][0]["sender"], 2)
        self.assertEqual(evidence["submitted_attempts"], 4)
        self.assertEqual(evidence["actions"][0]["inventory_revision_before"], 9)
        self.assertEqual(evidence["actions"][0]["primary_before"]["condition"], 86)
        self.assertEqual(evidence["actions"][0]["ap_before"], 63)
        self.assertEqual(evidence["actions"][0]["command"], 22)
        self.assertEqual(evidence["actions"][-1]["slots_after"], owner["slots"])
        self.assertEqual(evidence["actions"][-1]["primary_after"]["condition"], 86)
        self.assertTrue(all(command[1] == "SWAP" for command in self.commands))

    def test_failed_or_timed_out_readiness_never_sends_a_swap(self):
        with self.assertRaisesRegex(inventory.InventoryFailure, "readiness failed"):
            self.certify(readiness_error=inventory.MovementFailure("readiness failed"))
        self.assertEqual(self.commands, [])
        with self.assertRaises(inventory.InventoryFailure):
            self.certify(readiness_seconds=40)
        self.assertEqual(self.commands, [])

    def test_snapshot_below_readiness_revision_floor_cannot_authorize_a_swap(self):
        with self.assertRaises(inventory.InventoryFailure):
            self.certify(readiness_floor=100)
        self.assertEqual(self.commands, [])

    def test_controller_rejection_is_not_retried(self):
        with self.assertRaisesRegex(inventory.InventoryFailure, "no retry"):
            self.certify(reject=True)
        self.assertEqual(len(self.commands), 1)

    def test_applied_or_ambiguous_command_is_never_retried(self):
        with self.assertRaises(inventory.InventoryFailure):
            self.certify(no_mutation=True)
        self.assertEqual(len(self.commands), 1)
        with self.assertRaisesRegex(inventory.InventoryFailure, "no retry"):
            self.certify(ambiguous=True)
        self.assertEqual(len(self.commands), 2)


class InventoryUiTests(unittest.TestCase):
    def certify(self, *, ready=True, actions=True, reject_action=0, stuck_action=0,
                foreign=False, revision_change=False, source_lost=False,
                no_ordinary=False, no_unsupported=False, mutation=False,
                existing=False, bad_capture_path=False, delayed=False, typed=False):
        snapshots = held_kit_states() if typed else initial_states()
        owner = snapshots[0]["inventories"][0]
        owner["slots"][1].update(item=188, count=1, condition=73, support=1, status_kind=1)
        owner["slots"][7].update(item=1090, count=1, condition=97, support=2, status_kind=0)
        if no_ordinary:
            for slot in owner["slots"]:
                if slot["item"] and slot["slot"] != 1:
                    slot.update(support=2, status_kind=0, resource_total=0)
        if no_unsupported:
            for slot in owner["slots"]:
                if slot["support"] == 2:
                    slot.update(item=0, count=0, condition=0, support=0, status_kind=0, resource_total=0)
        self.commands = []
        self.clock = 0.0
        self.png_waits = []
        identity = owner["actor"][:]
        ui = {"ready": ready, "open": False, "actor": identity[:],
              "session": 1, "world": 2, "baseline": 1, "inventory_revision": 1,
              "source": inventory.NO_SLOT, "inspected": inventory.NO_SLOT,
              "actions_enabled": actions, "width": 640, "height": 480}
        if foreign:
            ui["actor"] = [99, 1]
        pending, observations = [None], [0]

        def apply(action, slot):
            if action == stuck_action:
                return
            if action == 1:
                ui["open"] = True
            elif action == 2:
                ui["inspected"] = slot
                item = owner["slots"][slot]
                if actions and item["support"] == 1 and (slot <= 6 or 14 <= slot < 55):
                    if ui["source"] not in (inventory.NO_SLOT, slot):
                        raise AssertionError("inspection-only control attempted a gameplay destination")
                    ui["source"] = inventory.NO_SLOT if ui["source"] == slot else slot
                elif source_lost:
                    ui["source"] = inventory.NO_SLOT
                if revision_change:
                    ui["inventory_revision"] += 1
            elif action == 4:
                ui.update(open=False, source=inventory.NO_SLOT, inspected=inventory.NO_SLOT)
                if mutation:
                    snapshots[0]["receipt"]["command"] += 1

        class Probe:
            def send(probe, verb, *arguments):
                self.commands.append((verb, *arguments))
                return len(self.commands)

            def receive(probe, request, kind, deadline, guard):
                command = self.commands[request - 1]
                if command[0] == "INVENTORY_UI":
                    self.assertEqual(kind, "inventory_ui")
                    observations[0] += 1
                    if pending[0] is not None and observations[0] % 3 == 0:
                        apply(*pending[0])
                        pending[0] = None
                    return {**copy.deepcopy(ui), "outstanding": snapshots[0]["outstanding"],
                            "receipt": copy.deepcopy(snapshots[0].get("receipt"))}
                self.assertEqual(command[0], "INVENTORY_INPUT")
                self.assertEqual(kind, "inventory_input")
                _, action, actor_slot, actor_inc, slot, revision = command
                self.assertEqual((actor_slot, actor_inc), tuple(identity))
                self.assertEqual(revision, owner["revision"])
                if delayed and action != 3:
                    pending[0] = action, slot
                else:
                    apply(action, slot)
                return {"queued": action != reject_action, "action": action,
                        "screenshot": ("../unsafe.png" if bad_capture_path else inventory.UI_SCREENSHOT)
                        if action == 3 else ""}

        with tempfile.TemporaryDirectory(prefix="ja2-inventory-ui-unit-") as directory, ExitStack() as stack:
            client = SimpleNamespace(log_path=Path(directory) / "client.stdout.log")
            if existing:
                screenshot = Path(directory) / inventory.UI_SCREENSHOT
                screenshot.parent.mkdir()
                screenshot.write_bytes(b"existing artifact must not be replaced")
            stack.enter_context(patch.object(inventory, "_states", side_effect=lambda *_args: copy.deepcopy(snapshots)))
            stack.enter_context(patch.object(inventory.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(inventory.time, "sleep", side_effect=lambda seconds: setattr(self, "clock", self.clock + seconds)))
            stack.enter_context(patch.object(inventory, "_wait_inventory_png", side_effect=lambda *args: self.png_waits.append(args[:2]) or 512))
            return inventory._certify_inventory_ui([Probe(), Probe()], lambda: None,
                                                   [client, client], 0, (1, 2), 20.0)

    def inputs(self):
        return [command for command in self.commands if command[0] == "INVENTORY_INPUT"]

    def test_real_control_sequence_opens_inspects_ordinary_and_unsupported_captures_and_closes(self):
        evidence = self.certify(delayed=True)
        self.assertEqual([command[1] for command in self.inputs()], [1, 2, 2, 3, 4])
        self.assertEqual([command[4] for command in self.inputs()], [65535, 5, 7, 7, 65535])
        self.assertEqual([item["source"] for item in evidence["inspections"]], [5, 5])
        self.assertTrue(evidence["unsupported_inspected"])
        self.assertTrue(evidence["closed"])
        self.assertTrue(evidence["no_gameplay_submission"])
        self.assertEqual(evidence["pixel_size"], [640, 480])
        self.assertEqual(len(self.png_waits), 1)
        self.assertGreater(self.clock, 0)
        self.assertTrue(all(command[0] in ("INVENTORY_UI", "INVENTORY_INPUT") for command in self.commands))

    def test_busy_turn_can_inspect_without_selecting_any_source(self):
        evidence = self.certify(actions=False)
        self.assertEqual([item["source"] for item in evidence["inspections"]], [65535, 65535])

    def test_typed_toolkit_capture_keeps_unsupported_source_proof_and_cancels_same_source(self):
        for actions in (False, True):
            with self.subTest(actions=actions):
                evidence = self.certify(typed=True, delayed=True, actions=actions)
                self.assertEqual([command[1] for command in self.inputs()], [1, 2, 2, 2, 3, 4])
                self.assertEqual([command[4] for command in self.inputs()], [65535, 6, 7, 6, 6, 65535])
                self.assertEqual(evidence["captured_status_kind"], inventory.TOOL_KIT_POINTS)
                self.assertEqual(evidence["captured_resource_total"], 99)
                self.assertTrue(evidence["unsupported_inspected"])
                self.assertTrue(evidence["inspections"][-1]["same_source_cancel"])
                self.assertEqual(evidence["inspections"][-1]["source"], inventory.NO_SLOT)
                self.assertTrue(evidence["no_gameplay_submission"])

    def test_missing_unsupported_item_is_not_invented_or_claimed(self):
        evidence = self.certify(no_unsupported=True)
        self.assertFalse(evidence["unsupported_inspected"])
        self.assertEqual(len(evidence["inspections"]), 1)
        self.assertEqual([command[1] for command in self.inputs()], [1, 2, 3, 4])

    def test_missing_owned_or_visible_state_or_fresh_output_fails_before_input(self):
        for options in ({"ready": False}, {"foreign": True}, {"no_ordinary": True}, {"existing": True}):
            with self.subTest(options=options), self.assertRaises(inventory.InventoryFailure):
                self.certify(**options)
            self.assertEqual(self.inputs(), [])

    def test_rejected_or_ambiguous_inputs_never_replay(self):
        for action in (1, 2, 3, 4):
            with self.subTest(action=action), self.assertRaisesRegex(inventory.InventoryFailure, "no replay"):
                self.certify(reject_action=action)
            self.assertEqual(self.inputs()[-1][1], action)
            self.assertLessEqual(len(self.inputs()), 5)

    def test_open_inspection_or_close_that_never_takes_effect_is_bounded_without_replay(self):
        for action in (1, 2, 4):
            with self.subTest(action=action), self.assertRaisesRegex(inventory.InventoryFailure, "no replay"):
                self.certify(stuck_action=action)
            self.assertEqual(self.inputs()[-1][1], action)
            self.assertLessEqual(len(self.inputs()), 5)
            self.assertLessEqual(len(self.commands), 140)

    def test_replacement_revision_or_source_loss_fails_closed(self):
        for options in ({"revision_change": True}, {"source_lost": True}):
            with self.subTest(options=options), self.assertRaises(inventory.InventoryFailure):
                self.certify(**options)
            self.assertEqual(self.png_waits, [])

    def test_snapshot_submission_or_arbitrary_screenshot_path_is_not_certified(self):
        for options in ({"mutation": True}, {"bad_capture_path": True}):
            with self.subTest(options=options), self.assertRaises(inventory.InventoryFailure):
                self.certify(**options)


class InventoryPngTests(unittest.TestCase):
    @staticmethod
    def png(width=640, height=480):
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
                chunk(b"IDAT", zlib.compress((b"\0" + b"\0\0\0" * width) * height)) + chunk(b"IEND", b""))

    def wait(self, payload, *, dimensions=(640, 480), linked=False):
        clock = [0.0]
        with tempfile.TemporaryDirectory(prefix="ja2-inventory-png-unit-") as directory, ExitStack() as stack:
            path = Path(directory) / "SCREEN00000.png"
            if payload is not None:
                path.write_bytes(payload)
            if linked:
                path.symlink_to(Path(directory) / "missing.png")
            stack.enter_context(patch.object(inventory.time, "monotonic", side_effect=lambda: clock[0]))
            stack.enter_context(patch.object(inventory.time, "sleep", side_effect=lambda seconds: clock.__setitem__(0, clock[0] + seconds)))
            return inventory._wait_inventory_png(path, dimensions, lambda: None, 0.2)

    def test_complete_native_sized_png_is_required(self):
        png = self.png()
        self.assertEqual(self.wait(png), len(png))
        for payload in (None, png[:-12], b"bad!" + png[4:]):
            with self.subTest(payload=payload is None), self.assertRaises(inventory.InventoryFailure):
                self.wait(payload)

    def test_wrong_dimensions_and_redirected_output_fail(self):
        with self.assertRaises(inventory.InventoryFailure):
            self.wait(self.png(), dimensions=(800, 600))
        with self.assertRaises(inventory.InventoryFailure):
            self.wait(None, linked=True)

    def test_oversized_png_is_not_read_unboundedly(self):
        with patch.object(inventory, "MAX_SCREENSHOT_BYTES", 44), self.assertRaises(inventory.InventoryFailure):
            self.wait(self.png())


if __name__ == "__main__":
    unittest.main()
