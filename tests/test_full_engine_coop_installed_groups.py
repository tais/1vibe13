import copy
import unittest
from unittest.mock import patch

import full_engine_coop_installed_groups as groups
from full_engine_coop_installed_movement import MovementFailure


def fixture():
    return {"kind": "groups", "request": 1, "ready": True, "session": 7, "revision": 3, "available": True,
            "groups": [{"slot": 2, "inc": 10, "x": 9, "y": 1, "z": 0, "between": False, "vehicle": False,
                        "next_x": 0, "next_y": 0, "transport": 1, "destination_x": 0, "destination_y": 0,
                        "arrival_minutes": 0, "traverse_minutes": 0,
                        "members": [{"slot": i, "inc": i + 1, "profile": 238 + i, "assignment": 0} for i in range(4)]}]}


def tactical_fixture():
    return [{"session": 7, "ready": True, "actors": [{"slot": i, "inc": i + 1, "profile": 238 + i, "team": 0} for i in range(4)],
             "assigned": [[i, i + 1] for i in range(client * 2, client * 2 + 2)]} for client in range(2)]


class GroupsTests(unittest.TestCase):
    def test_live_and_unavailable(self):
        self.assertTrue(groups.validate(fixture()))
        self.assertFalse(groups.validate({"kind": "groups", "request": 1, "ready": False}))
        value = fixture()
        value.update(available=False, groups=[])
        self.assertFalse(groups.validate(value))

    def test_invalid_headers(self):
        for key, bad in (("request", True), ("session", 0), ("revision", False), ("ready", 1),
                         ("available", 1), ("available", False), ("kind", "state"), ("groups", None)):
            with self.subTest(key=key, bad=bad):
                value = fixture()
                value[key] = bad
                with self.assertRaises(MovementFailure):
                    groups.validate(value)
        value = fixture()
        value["ready"] = False
        with self.assertRaises(MovementFailure):
            groups.validate(value)

    def test_invalid_group_fields(self):
        for key, bad in (("slot", 0), ("inc", 0), ("x", 17), ("y", 0), ("z", 4), ("between", 1),
                         ("vehicle", 0), ("next_x", 10), ("arrival_minutes", 7), ("destination_y", 2),
                         ("traverse_minutes", -1), ("transport", 256), ("members", [])):
            value = fixture()
            value["groups"][0][key] = bad
            with self.subTest(key=key), self.assertRaises(MovementFailure):
                groups.validate(value)

    def test_native_moving_group(self):
        value = fixture()
        group = value["groups"][0]
        group.update(between=True, next_x=10, next_y=1, arrival_minutes=2000, traverse_minutes=89,
                     destination_x=10, destination_y=3)
        self.assertTrue(groups.validate(value))
        group["next_y"] = 2
        with self.assertRaises(MovementFailure):
            groups.validate(value)

    def test_members_and_duplicates(self):
        for key, bad in (("slot", 65535), ("inc", 0), ("profile", -1), ("assignment", 128), ("slot", True)):
            value = fixture()
            value["groups"][0]["members"][0][key] = bad
            with self.subTest(key=key), self.assertRaises(MovementFailure):
                groups.validate(value)
        value = fixture()
        value["groups"].append(copy.deepcopy(value["groups"][0]))
        with self.assertRaises(MovementFailure):
            groups.validate(value)  # duplicate group
        value["groups"][1]["slot"] = 3
        with self.assertRaises(MovementFailure):
            groups.validate(value)  # same member in another group

    def test_only_read_only_probe(self):
        class Probe:
            def send(self, *args):
                self.command = args
                return 12
            def receive(self, request, kind, deadline, guard):
                self.received = request, kind
                return fixture()
        probes = [Probe(), Probe()]
        groups.observations(probes, 1, lambda: None)
        self.assertEqual([p.command for p in probes], [("GROUPS",)] * 2)
        self.assertEqual([p.received for p in probes], [(12, "groups")] * 2)

    def test_agreement_requires_full_starter_roster_and_lineage(self):
        with patch.object(groups, "_Probe"), patch.object(groups, "_guard"), patch.object(groups, "observations") as read, \
                patch.object(groups, "_states", return_value=tactical_fixture()):
            read.return_value = [fixture(), fixture()]
            self.assertEqual(groups.wait_shared([1, 2], 3, 1), read.return_value)
            old = fixture()
            old["revision"] += 1
            with self.assertRaises(MovementFailure):
                groups.wait_shared([1, 2], 3, 1, previous=[old, old])
            for value in read.return_value:
                value["groups"][0]["members"].pop()
            with self.assertRaises(MovementFailure):
                groups.wait_shared([1, 2], 3, 1)

    def test_exact_tactical_identity_profile_and_ownership_binding(self):
        groups.verify_tactical_binding(fixture(), tactical_fixture())
        for fault in range(6):
            states = tactical_fixture()
            if fault == 0:
                states[0]["session"] += 1
            if fault == 1:
                states[0]["ready"] = False
            if fault == 2:
                states[0]["actors"][0]["inc"] += 1
            if fault == 3:
                states[1]["actors"][0]["profile"] += 1
            if fault == 4:
                states[1]["assigned"] = states[0]["assigned"]
            if fault == 5:
                states[1]["assigned"].pop()
            with self.subTest(fault=fault), self.assertRaises(MovementFailure):
                groups.verify_tactical_binding(fixture(), states)

    def test_disconnected_cannot_expose_unavailable_stamped_snapshot_either(self):
        with patch.object(groups, "_Probe"), patch.object(groups, "_guard"), patch.object(groups, "observations") as read:
            value = fixture()
            value.update(available=False, groups=[])
            read.return_value = [value, value]
            with self.assertRaises(MovementFailure):
                groups.verify_cleared([1, 2], 1)
            read.return_value = [{"kind": "groups", "request": 1, "ready": False}] * 2
            self.assertEqual(groups.verify_cleared([1, 2], 1), read.return_value)


if __name__ == "__main__":
    unittest.main()
