"""Build-free exact-scope/read-only checks; fake PNGs are not visual evidence."""

import copy
from contextlib import ExitStack
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import full_engine_coop_installed_portraits as portraits
from test_full_engine_coop_installed_inventory import held_kit_states


class Harness:
    def __init__(self, directory):
        self.states = held_kit_states()
        for state in self.states:
            for actor in state["actors"]:
                actor["profile"] = 21 + actor["slot"]
                actor["portrait"] = {"family": 1, "index": 77 + actor["slot"], "camouflage": 0}
        self.clients = []
        self.ui = []
        for sender, state in enumerate(self.states):
            root = Path(directory) / str(sender)
            root.mkdir()
            self.clients.append(SimpleNamespace(log_path=root / "client.log"))
            self.ui.append({"ready": True, "open": False, "actor": state["assigned"][0][:],
                            "session": 1, "world": 2, "baseline": 1, "inventory_revision": 1,
                            "source": 65535, "inspected": 65535, "actions_enabled": False,
                            "width": 640, "height": 480})
        self.commands, self.waits, self.captured = [], [], [False, False]
        self.clock, self.reads = 0.0, 0
        self.reply_change = self.after_capture = self.on_read = None
        self.previous_inventory = [False, False]
        self.probes = [self.Probe(self, sender) for sender in range(2)]

    class Probe:
        def __init__(self, harness, sender):
            self.h, self.sender = harness, sender

        def send(self, verb, *args):
            self.h.commands.append((self.sender, verb, *args))
            return len(self.h.commands)

        def receive(self, request, kind, deadline, guard):
            sender, verb, *args = self.h.commands[request - 1]
            if verb == "INVENTORY_UI":
                return copy.deepcopy(self.h.ui[sender])
            if verb != "ROSTER_CAPTURE":
                raise AssertionError(f"unexpected mutating or unbounded command: {verb}")
            slot, inc, session, world, baseline, revision = args
            ui = self.h.ui[sender]
            if ([slot, inc] != ui["actor"] or (session, world, baseline, revision) !=
                    (ui["session"], ui["world"], ui["baseline"], ui["inventory_revision"])):
                raise AssertionError("capture did not bind the exact current Screen owner")
            relative = portraits.ROSTER_SCREENSHOTS[int(self.h.previous_inventory[sender])]
            path = self.h.clients[sender].log_path.parent / relative
            reply = {"queued": not self.h.captured[sender], "actor": [slot, inc], "session": session,
                     "world": world, "baseline": baseline, "inventory_revision": revision,
                     "width": ui["width"], "height": ui["height"], "screenshot": relative}
            self.h.captured[sender] = True
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b"synthetic screenshot; structural wait is mocked")
            if self.h.after_capture:
                self.h.after_capture(self.h, sender)
            if self.h.reply_change:
                self.h.reply_change(reply)
            return reply

    def observe(self, *_args):
        self.reads += 1
        if self.on_read:
            self.on_read(self)
        return copy.deepcopy(self.states)

    def captures(self):
        return [command for command in self.commands if command[1] == "ROSTER_CAPTURE"]

    def certify(self, *, inventory_evidence=None, timeout=3.0):
        with ExitStack() as stack:
            stack.enter_context(patch.object(portraits, "_Probe", side_effect=self.probes))
            stack.enter_context(patch.object(portraits, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(portraits, "_states", side_effect=self.observe))
            stack.enter_context(patch.object(portraits.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(portraits.time, "sleep",
                                            side_effect=lambda seconds: setattr(self, "clock", self.clock + seconds)))
            stack.enter_context(patch.object(portraits, "_wait_inventory_png",
                                            side_effect=lambda *args: self.waits.append(args[:2]) or 512))
            stack.enter_context(patch("builtins.print"))
            return portraits.certify_two_client_portraits(self.clients, object(), timeout,
                                                          inventory_evidence=inventory_evidence)


class PortraitProbeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="ja2-portrait-unit-")
        self.addCleanup(self.directory.cleanup)
        self.h = Harness(self.directory.name)

    def test_two_owned_rosters_capture_once_with_exact_scope_and_no_gameplay(self):
        original = copy.deepcopy(self.h.states)
        evidence = self.h.certify()
        self.assertEqual(self.h.captures(), [(0, "ROSTER_CAPTURE", 0, 1, 1, 2, 1, 1),
                                             (1, "ROSTER_CAPTURE", 1, 2, 1, 2, 1, 1)])
        self.assertTrue(evidence["no_gameplay_submission"])
        self.assertTrue(evidence["visual_inspection_required"])
        self.assertEqual(len(evidence["descriptors"]), 2)
        self.assertEqual(self.h.states, original)
        self.assertEqual(len(self.h.waits), 2)
        self.assertTrue(all(command[1] in ("INVENTORY_UI", "ROSTER_CAPTURE") for command in self.h.commands))
        self.assertTrue(all(Path(item["screenshot"]).name == "SCREEN00000.png" for item in evidence["screenshots"]))

    def test_known_inventory_capture_uses_second_fixed_path_on_only_that_process(self):
        first = self.h.clients[0].log_path.parent / portraits.ROSTER_SCREENSHOTS[0]
        first.parent.mkdir()
        first.write_bytes(b"previous inventory fixture")
        self.h.previous_inventory[0] = True
        evidence = self.h.certify(inventory_evidence={"ui": {"sender": 1, "closed": True, "screenshot": str(first)}})
        self.assertEqual([Path(item["screenshot"]).name for item in evidence["screenshots"]],
                         ["SCREEN00001.png", "SCREEN00000.png"])
        self.assertEqual(first.read_bytes(), b"previous inventory fixture")

    def test_absent_malformed_descriptor_or_profile_fails_before_any_capture(self):
        original = copy.deepcopy(self.h.states)
        changes = (None, {"family": 0, "index": 0, "camouflage": 0},
                   {"family": 3, "index": 0, "camouflage": 0},
                   {"family": True, "index": 0, "camouflage": 0},
                   {"family": 1, "index": -1, "camouflage": 0},
                   {"family": 1, "index": 256, "camouflage": 0},
                   {"family": 1, "index": 0, "camouflage": 5},
                   {"family": 1, "index": 0},
                   {"family": 1, "index": 0, "camouflage": 0, "file": "elsewhere"})
        for change in changes:
            with self.subTest(change=change):
                self.h.states = copy.deepcopy(original)
                self.h.states[0]["actors"][0]["portrait"] = change
                with self.assertRaises(portraits.PortraitFailure):
                    self.h.certify()
                self.assertEqual(self.h.captures(), [])
        for value in (-1, 200, 255, True, "21"):
            with self.subTest(profile=value):
                self.h.states = copy.deepcopy(original)
                self.h.states[0]["actors"][0]["profile"] = value
                with self.assertRaises(portraits.PortraitFailure):
                    self.h.certify()

    def test_replica_descriptor_disagreement_is_not_visual_agreement(self):
        self.h.states[1]["actors"][0]["portrait"]["index"] += 1
        with self.assertRaisesRegex(portraits.PortraitFailure, "disagree"):
            self.h.certify()
        self.assertEqual(self.h.captures(), [])

    def test_imp_and_camouflage_descriptors_preserve_full_native_index_domain(self):
        for state in self.h.states:
            state["actors"][0]["portrait"] = {"family": 2, "index": 255, "camouflage": 4}
        descriptor = self.h.certify()["descriptors"][0]
        self.assertEqual(descriptor["portrait"], {"family": 2, "index": 255, "camouflage": 4})

    def test_boolean_or_missing_header_scope_cannot_alias_valid_integer_state(self):
        original = copy.deepcopy(self.h.states)
        for field, value in (("session", True), ("world", True), ("outstanding", False),
                             ("commands_blocked", None), ("ready", 1)):
            with self.subTest(field=field):
                self.h.states = copy.deepcopy(original)
                self.h.states[1][field] = value
                with self.assertRaises(portraits.PortraitFailure):
                    self.h.certify()
                self.assertEqual(self.h.captures(), [])

    def test_missing_foreign_duplicate_actor_and_private_scope_fail_before_capture(self):
        original = copy.deepcopy(self.h.states)
        for problem in ("missing", "foreign", "duplicate", "baseline", "ready", "inventory"):
            with self.subTest(problem=problem):
                self.h.states = copy.deepcopy(original)
                if problem == "missing":
                    self.h.states[1]["actors"].pop(0)
                elif problem == "foreign":
                    self.h.states[0]["inventories"][0]["actor"] = [1, 2]
                elif problem == "duplicate":
                    self.h.states[0]["assigned"].append([0, 1])
                elif problem == "baseline":
                    self.h.states[0]["inventories"][0]["baseline"] = 0
                elif problem == "ready":
                    self.h.states[0]["ready"] = False
                else:
                    self.h.states[0]["inventories"] = []
                with self.assertRaises(portraits.PortraitFailure):
                    self.h.certify()
                self.assertEqual(self.h.captures(), [])

    def test_closed_actual_selected_owner_required_not_controller_fixture(self):
        original = copy.deepcopy(self.h.ui)
        for field, value in (("open", True), ("actor", [1, 2]), ("source", 5),
                             ("inspected", 5), ("baseline", 2), ("inventory_revision", 2)):
            with self.subTest(field=field):
                self.h.ui = copy.deepcopy(original)
                self.h.ui[0][field] = value
                with self.assertRaises(portraits.PortraitFailure):
                    self.h.certify()
                self.assertEqual(self.h.captures(), [])

    def test_native_block_gate_is_polled_read_only_until_clear(self):
        for state in self.h.states:
            state["commands_blocked"] = True
        def clear(h):
            if h.reads == 3:
                for state in h.states:
                    state["commands_blocked"] = False
        self.h.on_read = clear
        self.h.certify()
        self.assertGreaterEqual(self.h.clock, 0.2)
        self.assertEqual(len(self.h.captures()), 2)

    def test_uncleared_native_gate_never_sends_capture(self):
        self.h.states[0]["commands_blocked"] = True
        with self.assertRaisesRegex(portraits.PortraitFailure, "did not clear"):
            self.h.certify(timeout=0.3)
        self.assertEqual(self.h.captures(), [])

    def test_position_and_unrelated_ai_changes_do_not_fake_portrait_mutation(self):
        def move(h, sender):
            for state in h.states:
                state["revision"] += 1
                state["actors"][0]["grid"] += 1
                state["actors"][2]["life"] -= 1
        self.h.after_capture = move
        self.assertEqual(len(self.h.certify()["screenshots"]), 2)

    def test_mutated_protected_state_after_capture_fails_without_replaying(self):
        for problem in ("portrait", "profile", "inventory", "receipt", "assignment", "baseline", "world"):
            with self.subTest(problem=problem), tempfile.TemporaryDirectory(prefix="ja2-portrait-mutation-") as directory:
                h = Harness(directory)
                def mutate(h, sender):
                    state = h.states[0]
                    if problem == "portrait":
                        for state in h.states:
                            state["actors"][0]["portrait"]["index"] += 1
                    elif problem == "profile":
                        for state in h.states:
                            state["actors"][0]["profile"] += 1
                    elif problem == "inventory":
                        state["inventories"][0]["slots"][25]["resource_total"] -= 1
                    elif problem == "receipt":
                        state["receipt"]["command"] += 1
                    elif problem == "assignment":
                        state["assigned"] = [[2, 3]]
                    elif problem == "baseline":
                        state["inventories"][0]["baseline"] += 1
                    else:
                        state["world"] += 1
                h.after_capture = mutate
                with self.assertRaises(portraits.PortraitFailure):
                    h.certify()
                self.assertEqual(len(h.captures()), 1)

    def test_rejected_or_foreign_reply_path_and_scope_never_replay_capture(self):
        for field, value in (("queued", False), ("screenshot", "../../outside.png"),
                             ("actor", [9, 10]), ("session", 2), ("world", 3),
                             ("baseline", 2), ("inventory_revision", 2), ("width", 641)):
            with self.subTest(field=field), tempfile.TemporaryDirectory(prefix="ja2-portrait-reply-") as directory:
                h = Harness(directory)
                h.reply_change = lambda reply: reply.update({field: value})
                with self.assertRaisesRegex(portraits.PortraitFailure, "no replay"):
                    h.certify()
                self.assertEqual(len(h.captures()), 1)
                self.assertEqual(h.waits, [])

    def test_preexisting_or_symlink_outputs_refused_before_either_capture(self):
        for problem in ("first", "second", "first-link", "second-link", "directory-link"):
            with self.subTest(problem=problem), tempfile.TemporaryDirectory(prefix="ja2-portrait-path-") as directory:
                h = Harness(directory)
                root = h.clients[1].log_path.parent
                screenshots = root / "Screenshots"
                if problem == "directory-link":
                    screenshots.symlink_to(root, target_is_directory=True)
                else:
                    screenshots.mkdir()
                    path = screenshots / ("SCREEN00000.png" if problem.startswith("first") else "SCREEN00001.png")
                    if problem.endswith("link"):
                        path.symlink_to(root / "absent.png")
                    else:
                        path.write_bytes(b"pre-existing must not be replaced")
                with self.assertRaises(portraits.PortraitFailure):
                    h.certify()
                self.assertEqual(h.captures(), [])

    def test_repeat_probe_never_overwrites_earned_roster_screenshots(self):
        self.h.certify()
        with self.assertRaises(portraits.PortraitFailure):
            self.h.certify()
        self.assertEqual(len(self.h.captures()), 2)


if __name__ == "__main__":
    unittest.main()
