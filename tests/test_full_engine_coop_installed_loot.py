"""Build-free read-only loot-model evidence tests; no synthetic paint claims."""

import copy
from contextlib import ExitStack
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import full_engine_coop_installed_loot as loot
from test_full_engine_coop_installed_inventory import held_kit_states


class Harness:
    def __init__(self):
        self.states = held_kit_states()
        self.ui = []
        for state in self.states:
            inventory = state["inventories"][0]
            actor = next(actor for actor in state["actors"]
                         if [actor["slot"], actor["inc"]] == inventory["actor"])
            grid, level = actor["grid"], actor["level"]
            inventory["ground"] = {"grid": grid, "level": level, "truncated": False, "items": []}
            inventory["nearby_loot"] = {"truncated": False, "markers": [
                {"grid": grid + 1, "level": level, "medical": False},
                {"grid": grid + 160, "level": level, "medical": True}]}
            self.ui.append({"ready": True, "actor": inventory["actor"][:], "session": 1, "world": 2,
                            "baseline": 1, "inventory_revision": 1, "origin_grid": grid,
                            "origin_level": level, "viewport": [0, 0, 640, 300], "available": 2,
                            "count": 2, "truncated": False, "hovered": -1,
                            "markers": [{**marker, "x": 100 + 20 * index, "y": 100 + 10 * index}
                                        for index, marker in enumerate(inventory["nearby_loot"]["markers"])]})
        self.clients = [SimpleNamespace(name="A"), SimpleNamespace(name="B")]
        self.probes = [self.Probe(self, sender) for sender in range(2)]
        self.commands = []
        self.clock = 0.0
        self.state_reads = self.ui_reads = 0
        self.on_state = self.change_reply = None

    class Probe:
        def __init__(self, harness, sender):
            self.h, self.sender = harness, sender

        def send(self, verb, *arguments):
            if verb not in ("STATE", "LOOT_UI") or arguments:
                raise AssertionError("loot certification attempted gameplay, camera or input mutation")
            self.h.commands.append((self.sender, verb))
            return len(self.h.commands)

        def receive(self, request, kind, deadline, guard):
            sender, verb = self.h.commands[request - 1]
            if sender != self.sender or kind != verb.lower():
                raise AssertionError("probe correlation mismatch")
            if verb == "STATE":
                self.h.state_reads += 1
                if self.h.on_state:
                    self.h.on_state(self.h, sender)
                reply = copy.deepcopy(self.h.states[sender])
            else:
                self.h.ui_reads += 1
                reply = copy.deepcopy(self.h.ui[sender])
            reply.update(request=request, kind=kind)
            if verb == "LOOT_UI" and self.h.change_reply:
                self.h.change_reply(self.h, sender, reply)
            return reply

    def certify(self, timeout=1.0):
        with ExitStack() as stack:
            stack.enter_context(patch.object(loot, "_Probe", side_effect=self.probes))
            stack.enter_context(patch.object(loot, "_guard", return_value=lambda: None))
            stack.enter_context(patch.object(loot.time, "monotonic", side_effect=lambda: self.clock))
            stack.enter_context(patch.object(loot.time, "sleep",
                                       side_effect=lambda value: setattr(self, "clock", self.clock + value)))
            stack.enter_context(patch("builtins.print"))
            return loot.certify_two_client_loot_views(self.clients, object(), timeout)


class LootProbeTests(unittest.TestCase):
    def test_current_two_owner_models_bind_exact_scope_without_gameplay_or_paint_claim(self):
        h = Harness()
        original = copy.deepcopy(h.states)
        evidence = h.certify()
        self.assertEqual(evidence["attempts"], 1)
        self.assertEqual(evidence["clients_with_nonempty_marker_view"], 2)
        self.assertTrue(evidence["scope_and_projection_subset_verified"])
        self.assertTrue(evidence["nonempty_marker_view_verified"])
        self.assertTrue(evidence["no_gameplay_submission"])
        self.assertFalse(evidence["paint_verified"])
        self.assertFalse(evidence["actual_mouse_interaction_verified"])
        self.assertFalse(evidence["pickup_verified"])
        self.assertEqual(h.states, original)
        self.assertEqual(h.commands, [(0, "STATE"), (1, "STATE"), (0, "LOOT_UI"), (1, "LOOT_UI"),
                                      (0, "STATE"), (1, "STATE")])
        for sender, view in enumerate(evidence["views"]):
            self.assertEqual(view["actor"], h.states[sender]["assigned"][0])
            self.assertEqual(view["authorized_markers"], h.states[sender]["inventories"][0]["nearby_loot"]["markers"])

    def test_empty_authority_is_coherent_but_never_positive_nonempty_coverage(self):
        h = Harness()
        for state, ui in zip(h.states, h.ui):
            state["inventories"][0]["nearby_loot"]["markers"] = []
            ui.update(available=0, count=0, markers=[])
        evidence = h.certify()
        self.assertTrue(evidence["scope_and_projection_subset_verified"])
        self.assertFalse(evidence["nonempty_marker_view_verified"])
        self.assertEqual(evidence["clients_with_nonempty_marker_view"], 0)
        self.assertTrue(all(not view["nonempty_marker_view_verified"] for view in evidence["views"]))

    def test_offscreen_subset_and_empty_projection_do_not_overclaim_both_views(self):
        h = Harness()
        h.ui[0]["markers"] = h.ui[0]["markers"][1:]
        h.ui[0]["count"] = 1
        h.ui[0]["hovered"] = 0
        h.ui[1].update(count=0, markers=[])
        evidence = h.certify()
        self.assertEqual(evidence["clients_with_nonempty_marker_view"], 1)
        self.assertFalse(evidence["nonempty_marker_view_verified"])
        self.assertEqual(evidence["views"][0]["hovered_index"], 0)

    def test_read_only_stale_camera_retry_recovers_without_changing_actor_or_camera(self):
        h = Harness()
        def not_ready(h, sender, reply):
            if h.ui_reads <= 2:
                reply.clear()
                reply.update(request=len(h.commands) - (1 - sender), kind="loot_ui", ready=False)
        h.change_reply = not_ready
        self.assertEqual(h.certify()["attempts"], 2)
        self.assertGreaterEqual(h.clock, 0.1)
        self.assertTrue(all(verb in ("STATE", "LOOT_UI") for _, verb in h.commands))

    def test_private_revision_change_between_reads_retries_exact_current_view(self):
        h = Harness()
        def update(h, sender):
            if h.state_reads == 3:
                for state, ui in zip(h.states, h.ui):
                    state["inventories"][0]["revision"] = 2
                    ui["inventory_revision"] = 2
        h.on_state = update
        evidence = h.certify()
        self.assertEqual(evidence["attempts"], 2)
        self.assertTrue(all(view["inventory_revision"] == 2 for view in evidence["views"]))

    def test_permanently_stale_baseline_private_revision_or_pose_never_certifies(self):
        for field, value in (("baseline", 2), ("inventory_revision", 2), ("origin_grid", 19)):
            with self.subTest(field=field):
                h = Harness()
                h.ui[0][field] = value
                with self.assertRaisesRegex(loot.LootFailure, "did not correlate"):
                    h.certify(timeout=0.25)
                self.assertGreaterEqual(h.clock, 0.25)
                self.assertLessEqual(h.ui_reads, 8)
        h = Harness()
        h.states[1]["actors"][0]["grid"] += 1
        with self.assertRaisesRegex(loot.LootFailure, "did not correlate"):
            h.certify(timeout=0.25)

    def test_foreign_actor_or_world_is_not_a_transient_camera_retry(self):
        for field, value in (("actor", [1, 2]), ("session", 2), ("world", 3)):
            with self.subTest(field=field):
                h = Harness()
                h.ui[0][field] = value
                with self.assertRaisesRegex(loot.LootFailure, "foreign"):
                    h.certify()
                self.assertEqual(h.ui_reads, 2)

    def test_reply_canonical_types_counts_viewport_hover_and_capabilities_reject(self):
        invalid = (("ready", 1), ("actor", [False, 1]), ("session", True), ("inventory_revision", 0),
                   ("available", True), ("available", 129), ("available", 1), ("count", True),
                   ("count", 129), ("count", 1), ("truncated", 1), ("truncated", True),
                   ("viewport", [0, 0, 0, 300]), ("viewport", [0, -1, 640, 300]),
                   ("viewport", [0, 0, True, 300]), ("hovered", True), ("hovered", 2),
                   ("hovered", -2), ("pickup_authorized", True))
        for field, value in invalid:
            with self.subTest(field=field, value=value):
                h = Harness()
                h.ui[0][field] = value
                with self.assertRaises(loot.LootFailure):
                    h.certify()

    def test_projected_marker_must_be_typed_authorized_ordered_and_inside_viewport(self):
        for field, value in (("grid", 0), ("grid", True), ("level", 1), ("level", False),
                             ("medical", True), ("medical", 1), ("x", -1), ("x", 640),
                             ("x", True), ("y", 300), ("id", [19, 3])):
            with self.subTest(field=field, value=value):
                h = Harness()
                h.ui[0]["markers"][0][field] = value
                with self.assertRaises(loot.LootFailure):
                    h.certify()
        for duplicate in (False, True):
            h = Harness()
            if duplicate:
                h.ui[0]["markers"][1] = copy.deepcopy(h.ui[0]["markers"][0])
            else:
                h.ui[0]["markers"].reverse()
            with self.assertRaisesRegex(loot.LootFailure, "subsequence"):
                h.certify()

    def test_private_marker_schema_radius_order_and_ground_scope_reject_before_ui(self):
        for problem in ("foreign", "duplicate", "missing", "medical", "id", "floor", "negative", "radius",
                        "row_wrap", "world", "order", "duplicate_marker", "truncated", "absent", "count"):
            with self.subTest(problem=problem):
                h = Harness()
                owner = h.states[0]["inventories"][0]
                markers = owner["nearby_loot"]["markers"]
                if problem == "foreign": owner["actor"] = [1, 2]
                elif problem == "duplicate": h.states[0]["assigned"].append([0, 1])
                elif problem == "missing": del owner["nearby_loot"]
                elif problem == "medical": markers[0]["medical"] = 1
                elif problem == "id": markers[0]["id"] = [19, 3]
                elif problem == "floor": markers[0]["level"] = 1
                elif problem == "negative": markers[0]["grid"] = -1
                elif problem == "radius": markers[0]["grid"] = owner["ground"]["grid"] + 13
                elif problem == "row_wrap": owner["ground"]["grid"] = 159; markers[0]["grid"] = 160
                elif problem == "world": markers[-1]["grid"] = 160 * 160
                elif problem == "order": markers.reverse()
                elif problem == "duplicate_marker": markers[1] = copy.deepcopy(markers[0])
                elif problem == "truncated": owner["nearby_loot"]["truncated"] = True
                elif problem == "absent": owner["ground"]["grid"] = owner["ground"]["level"] = -1
                else: markers.extend(copy.deepcopy(markers[0]) for _ in range(127))
                with self.assertRaises(loot.LootFailure):
                    h.certify()
                self.assertEqual(h.ui_reads, 0)

    def test_incomplete_cache_or_not_ready_is_bounded_read_only_wait(self):
        for missing in (False, True):
            h = Harness()
            if missing:
                h.states[0]["inventories"] = []
            else:
                h.states[0]["ready"] = False
            with self.assertRaisesRegex(loot.LootFailure, "did not correlate"):
                h.certify(timeout=0.25)
            self.assertEqual(h.ui_reads, 0)
            self.assertTrue(all(verb == "STATE" for _, verb in h.commands))

    def test_maximum_canonical_capped_marker_list_is_not_overflow_or_pickup_evidence(self):
        h = Harness()
        for state, ui in zip(h.states, h.ui):
            owner = state["inventories"][0]
            origin = owner["ground"]["grid"]
            rows = range(0, 13)
            columns = range(origin - 12, origin + 13)
            markers = [{"grid": row * 160 + column, "level": 0, "medical": bool(column % 2)}
                       for row in rows for column in columns][:128]
            owner["nearby_loot"] = {"truncated": True, "markers": markers}
            ui.update(available=128, count=128, truncated=True, hovered=127,
                      markers=[{**marker, "x": 100 + (index % 16) * 10, "y": 100 + (index // 16) * 10}
                               for index, marker in enumerate(markers)])
        evidence = h.certify()
        self.assertTrue(all(view["count"] == 128 and view["truncated"] for view in evidence["views"]))
        self.assertFalse(evidence["pickup_verified"])

    def test_timeout_rejects_bool_nonfinite_nonpositive_or_excessive_values_before_probes(self):
        for timeout in (True, False, 0, -1, 301, float("nan"), float("inf"), "1"):
            with self.subTest(timeout=timeout), patch.object(loot, "_Probe") as probe:
                with self.assertRaises(loot.LootFailure):
                    loot.certify_two_client_loot_views([object(), object()], object(), timeout)
                probe.assert_not_called()


if __name__ == "__main__":
    unittest.main()
