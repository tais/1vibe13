"""Data-free tests of the two-private-inventory transfer certificate."""

import copy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_transfer as transfer
from test_full_engine_coop_installed_fire import actor
from test_full_engine_coop_installed_inventory import initial_states


def before_states():
    result = initial_states()
    for state in result:
        state["actors"].append(actor(3))
    result[0]["assigned"].append([3, 4])
    inventory = copy.deepcopy(result[0]["inventories"][0])
    inventory["actor"] = [3, 4]
    result[0]["inventories"].append(inventory)
    for index, owner in enumerate(result[0]["inventories"]):
        owner["slots"][25 if index == 0 else 21].update(
            item=77, count=2, condition=6, support=1, status_kind=2, resource_total=12)
    return result


def after_states(before=None):
    before = before or before_states()
    result = copy.deepcopy(before)
    inventories = transfer._expected_caches(before, 0, (0, 1), (3, 4), 25, 26)
    for inventory in inventories[0].values():
        inventory["revision"] += 1
    result[0]["inventories"] = list(inventories[0].values())
    for state in result:
        state["revision"] += 1
        for entry in state["actors"]:
            if entry["slot"] in (0, 3):
                entry["ap"] -= 2
    result[0]["receipt"] = {"command": 2, "status": 3, "reason": 0, "revision": result[0]["revision"]}
    return result


class TransferCertificateTests(unittest.TestCase):
    def wait(self, after=None, before=None, sequence=None):
        observed = iter(sequence or [after or after_states()])
        last = [None]
        clock = [0.0]

        def states(*_):
            last[0] = next(observed, last[0])
            return copy.deepcopy(last[0])

        with patch.object(transfer, "_states", side_effect=states), \
                patch.object(transfer.time, "monotonic", side_effect=lambda: clock[0]), \
                patch.object(transfer.time, "sleep", side_effect=lambda value: clock.__setitem__(0, clock[0] + value)):
            return transfer._wait_transfer([None, None], lambda: None, 0, 2, (0, 1), (3, 4),
                                           25, 26, before or before_states(), 1.1)

    def test_candidate_is_nearby_same_owner_and_does_not_mutate(self):
        initial = before_states()
        original = copy.deepcopy(initial)
        self.assertEqual(transfer._candidate(initial), (0, (0, 1), (3, 4), 25, 26))
        self.assertEqual(initial, original)
        initial[0]["assigned"] = [[0, 1]]
        initial[0]["inventories"].pop()
        self.assertIsNone(transfer._candidate(initial))

    def test_exact_both_inventory_updates_and_ap_costs_certify(self):
        _, evidence = self.wait()
        self.assertEqual(evidence["inventory_revisions_before"], [1, 1])
        self.assertEqual(evidence["inventory_revisions_after"], [2, 2])
        self.assertTrue(evidence["combat_ap_cost_verified"])
        self.assertTrue(evidence["unrelated_owner_inventories_unchanged"])

    def test_missing_unrelated_owner_cache_cannot_certify_privacy_or_conservation(self):
        before = before_states()
        before[1]["inventories"] = []
        self.assertIsNone(transfer._candidate(before))
        with self.assertRaisesRegex(transfer.InventoryFailure, "every assigned"):
            self.wait(after_states(before), before)

    def test_applied_without_recipient_update_never_certifies(self):
        after = after_states()
        after[0]["inventories"][1] = before_states()[0]["inventories"][1]
        with self.assertRaisesRegex(transfer.InventoryFailure, "failed exact"):
            self.wait(after)

    def test_changed_kit_or_ammo_total_is_not_conservation(self):
        for field, value in (("resource_total", 11), ("status_kind", 4), ("count", 1)):
            after = after_states()
            after[0]["inventories"][1]["slots"][26][field] = value
            with self.subTest(field=field), self.assertRaises(transfer.InventoryFailure):
                self.wait(after)

    def test_wrong_ap_cost_for_either_actor_fails(self):
        for slot in (0, 3):
            after = after_states()
            for state in after:
                next(entry for entry in state["actors"] if entry["slot"] == slot)["ap"] += 1
            with self.subTest(slot=slot), self.assertRaisesRegex(transfer.InventoryFailure, "exactly 2 AP"):
                self.wait(after)

    def test_realtime_does_not_claim_combat_ap_cost(self):
        before = before_states()
        for state in before:
            state["in_combat"] = False
        _, evidence = self.wait(after_states(before), before)
        self.assertFalse(evidence["combat_ap_cost_verified"])

    def test_pending_private_delivery_can_settle_later(self):
        partial = after_states()
        partial[0]["inventories"][1] = before_states()[0]["inventories"][1]
        _, evidence = self.wait(sequence=[partial, after_states()])
        self.assertTrue(evidence["slot_metric_conservation_verified"])

    def test_rejected_or_changed_receipt_fails_without_replay(self):
        for status, reason in ((2, 5), (2, 9), (3, 9)):
            after = after_states()
            after[0]["receipt"].update(status=status, reason=reason)
            with self.subTest(status=status, reason=reason), self.assertRaisesRegex(transfer.InventoryFailure, "no replay"):
                self.wait(after)
        replaced = after_states()
        replaced[0]["receipt"]["command"] = 3
        with self.assertRaisesRegex(transfer.InventoryFailure, "replaced"):
            self.wait(replaced)

    def test_terminal_receipt_cannot_change_its_revision_after_applied(self):
        changed = after_states()
        changed[0]["receipt"]["revision"] -= 1
        with self.assertRaisesRegex(transfer.InventoryFailure, "receipt changed"):
            self.wait(sequence=[after_states(), changed])

    def test_unrelated_inventory_change_or_cross_baseline_fails(self):
        for field in ("revision", "baseline"):
            after = after_states()
            after[1]["inventories"][0][field] += 1
            with self.subTest(field=field), self.assertRaises(transfer.InventoryFailure):
                self.wait(after)
        after = after_states()
        after[0]["inventories"][1]["baseline"] += 1
        with self.assertRaisesRegex(transfer.InventoryFailure, "baseline"):
            self.wait(after)

    def test_primary_or_assignment_change_invalidates(self):
        after = after_states()
        for state in after:
            state["actors"][3]["primary"]["condition"] -= 1
        with self.assertRaisesRegex(transfer.InventoryFailure, "primary"):
            self.wait(after)
        after = after_states()
        after[0]["assigned"].reverse()
        with self.assertRaisesRegex(transfer.InventoryFailure, "ownership"):
            self.wait(after)


if __name__ == "__main__":
    unittest.main()
