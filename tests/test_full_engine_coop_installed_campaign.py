"""Evidence boundary checks for installed campaign input and native receipts."""
import contextlib
from copy import deepcopy
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

import full_engine_coop_installed_campaign as campaign


class InstalledCampaignTests(unittest.TestCase):
    def arguments(self, *extra):
        return campaign.parse_arguments(["--server-executable", "/fixture/server", "--client-executable", "/fixture/client",
                                         "--installed-data-root", "/fixture/data", *extra])

    def ui(self):
        return {"request": 1, "kind": "campaign_ui", "ready": True, "action_pending": False, "time_pending": False,
                "action_result": None, "time_result": None, "session": 2, "control_revision": 3, "groups_revision": 4,
                "decision": 0, "selected": [7, 8], "destination": [10, 1], "retreat_armed": False, "enabled": True}

    def receipt(self):
        return {"request_id": 1, "action": 1, "session": 2, "control_revision": 3, "groups_revision": 4,
                "decision": 0, "group": [7, 8], "destination": [10, 1], "outcome": 1, "native_detail": 0,
                "result_control_revision": 4, "result_groups_revision": 4}

    def test_explicit_executables_and_finite_bounded_timeout(self):
        self.assertEqual(self.arguments().timeout_seconds, 180)
        self.assertEqual(self.arguments().battle_choice, "enter")
        self.assertEqual(self.arguments("--battle-choice=retreat").battle_choice, "retreat")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.arguments("--battle-choice=autoresolve")
        for value in ("1", "1.25", "600"):
            self.assertEqual(self.arguments(f"--timeout-seconds={value}").timeout_seconds, float(value))
        for value in ("0", "-1", "601", "nan", "inf", "-inf"):
            with self.subTest(value=value), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.arguments(f"--timeout-seconds={value}")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            campaign.parse_arguments([])

    def test_queue_acknowledgment_never_counts_as_authoritative_execution(self):
        queued = {"request": 1, "kind": "campaign_input", "queued": True, "key": 7, "queued_events": 2, "input_count": 1}
        with self.assertRaises(campaign.SmokeFailure):
            campaign.require_applied(queued, self.ui(), 1, [7, 8], [10, 1])
        for outcome in range(2, 9):
            with self.subTest(outcome=outcome), self.assertRaises(campaign.SmokeFailure):
                campaign.require_applied(dict(self.receipt(), outcome=outcome), self.ui(), 1, [7, 8], [10, 1])
        campaign.require_applied(self.receipt(), self.ui(), 1, [7, 8], [10, 1])

    def test_applied_receipt_must_bind_exact_selected_group_and_observations(self):
        for field, value in (("session", 3), ("control_revision", 2), ("groups_revision", 3),
                             ("group", [7, 9]), ("destination", [8, 1]), ("decision", 1)):
            with self.subTest(field=field), self.assertRaises(campaign.SmokeFailure):
                campaign.require_applied(dict(self.receipt(), **{field: value}), self.ui(), 1, [7, 8], [10, 1])
        with self.assertRaises(campaign.SmokeFailure):
            campaign.validate_action_result(dict(self.receipt(), result_control_revision=3))

    def test_exact_arrival_identity_cannot_reuse_travel_payload(self):
        arrival = dict(self.receipt(), action=5, decision=9, group=[0, 0], destination=[0, 0])
        campaign.require_applied(arrival, self.ui(), 5, decision=9)
        with self.assertRaises(campaign.SmokeFailure):
            campaign.require_applied(arrival, self.ui(), 5, decision=10)
        for change in ({"decision": 0}, {"group": [7, 8]}, {"destination": [10, 1]}, {"action": 1}):
            with self.subTest(change=change), self.assertRaises(campaign.SmokeFailure):
                campaign.validate_action_result(dict(arrival, **change))

    def test_receipt_is_available_after_screen_leaves_strategic_phase(self):
        value = {key: item for key, item in self.ui().items() if key in campaign.UI_BASE_FIELDS}
        value.update(ready=False, action_result=self.receipt())
        self.assertFalse(campaign.validate_ui(value))
        with self.assertRaises(campaign.SmokeFailure):
            campaign.validate_ui(dict(value, action_pending=True))
        with self.assertRaises(campaign.SmokeFailure):
            campaign.validate_ui(dict(value, selected=[7, 8]))

    def test_untrusted_json_cannot_coerce_bools_floats_or_partial_identities(self):
        for field, value in (("request_id", True), ("action", 1.0), ("session", 0), ("control_revision", -1),
                             ("groups_revision", 2**64), ("decision", False), ("outcome", True),
                             ("native_detail", 65536), ("group", [7, 0]), ("group", [7, True]),
                             ("destination", [17, 1]), ("destination", [10, 0])):
            with self.subTest(field=field, value=value), self.assertRaises(campaign.SmokeFailure):
                campaign.validate_action_result(dict(self.receipt(), **{field: value}))
        for field, value in (("ready", 1), ("request", True), ("action_pending", 0), ("retreat_armed", 0),
                             ("selected", [7, 0]), ("destination", [10, True]), ("decision", -1)):
            with self.subTest(field=field, value=value), self.assertRaises(campaign.SmokeFailure):
                campaign.validate_ui(dict(self.ui(), **{field: value}))

    def test_independent_queued_time_and_action_cannot_both_own_input(self):
        with self.assertRaises(campaign.SmokeFailure):
            campaign.validate_ui(dict(self.ui(), time_pending=True, action_pending=True))
        time = {"request_id": 1, "action": 4, "session": 2, "control_revision": 3, "result_control_revision": 4, "outcome": 1}
        self.assertTrue(campaign.validate_time_result(time))
        with self.assertRaises(campaign.SmokeFailure):
            campaign.validate_time_result(dict(time, result_control_revision=3))
        with self.assertRaises(campaign.SmokeFailure):
            campaign.validate_ui(dict(self.ui(), time_result=time, time_pending=True))

    def test_receipt_validation_preserves_copied_evidence(self):
        value = self.receipt()
        before = deepcopy(value)
        campaign.require_applied(value, self.ui(), 1, [7, 8], [10, 1])
        self.assertEqual(value, before)

    def test_repeated_connection_loss_fails_without_hiding_ordinary_checkpoint_behavior(self):
        marker = "[co-op client] reconnecting to server; reason=connection lost\n"
        alive = Mock()
        with patch.object(campaign, "_guard", return_value=alive), patch.object(campaign, "log_text", return_value=marker):
            campaign.qualification_guard(object(), [object(), object()])()
        alive.assert_called_once()
        with patch.object(campaign, "_guard", return_value=lambda: None), patch.object(campaign, "log_text", return_value=marker * 2):
            with self.assertRaises(campaign.SmokeFailure):
                campaign.qualification_guard(object(), [object(), object()])()

    def test_living_server_cannot_qualify_an_unhandled_native_interaction(self):
        server, first, second = object(), object(), object()
        for diagnostic in (
                "[dedicated] native dialog requires handling: screen=5; flags=4; text=Surrender?",
                "ERROR: [states] Screen handler failed for ID 5 with code 4"):
            with self.subTest(diagnostic=diagnostic):
                logs = {server: "normal progress\n" + diagnostic + "\n", first: "Ready\n", second: "Ready\n"}
                alive = Mock()
                with patch.object(campaign, "_guard", return_value=alive), \
                        patch.object(campaign, "log_text", side_effect=logs.__getitem__):
                    with self.assertRaises(campaign.SmokeFailure) as failure:
                        campaign.qualification_guard(server, [first, second])()
                self.assertIn(diagnostic, str(failure.exception))
                alive.assert_called_once()

    def test_replica_readiness_cannot_certify_failed_native_map_presentation(self):
        replica = "[co-op client] tactical baseline committed; assigned actors ready\n"
        self.assertFalse(campaign.presentation_ready(replica))
        failed = "[co-op client] exact tactical map unavailable: A10.DAT (unexpected trailing map data)\n"
        with self.assertRaises(campaign.SmokeFailure):
            campaign.presentation_ready(replica + failed)
        verified = "[co-op client] exact tactical map verified: A10.DAT\n"
        self.assertFalse(campaign.presentation_ready(replica + verified))
        self.assertTrue(campaign.presentation_ready(replica + verified + campaign.TACTICAL_PRESENTATION_READY_MARKER))

    def return_snapshot(self, moving=True):
        group = {"slot": 7, "inc": 8, "x": 10 if moving else 9, "y": 1, "z": 0,
                 "next_x": 9 if moving else 0, "next_y": 1 if moving else 0,
                 "between": moving, "vehicle": False, "destination_x": 0, "destination_y": 0,
                 "arrival_minutes": 1870 if moving else 0, "traverse_minutes": 5 if moving else 0,
                 "members": [{"slot": i, "inc": i + 1, "profile": i + 10, "assignment": 0} for i in range(4)]}
        status = {"phase": 2, "paused": True, "pause_locked": False, "compression_active": False, "world_seconds": 111900 if moving else 112200,
                  "time_control_revision": 4, "arrival": {"decision": 0}}
        return {"campaign": [status], "groups": [{"revision": 4, "groups": [group]}]}

    def test_return_resume_cannot_reuse_outbound_cached_time_receipt(self):
        receipt = {"request_id": 2, "action": 4, "session": 2, "control_revision": 3, "result_control_revision": 4, "outcome": 1}
        campaign.require_time_applied(receipt, self.ui(), 4, previous_id=1)
        for change in ({"request_id": 1}, {"control_revision": 2}, {"session": 3}, {"outcome": 3}, {"action": 1}):
            with self.subTest(change=change), self.assertRaises(campaign.SmokeFailure):
                campaign.require_time_applied(dict(receipt, **change), self.ui(), 4, previous_id=1)

    def test_autoresolve_availability_does_not_block_permitted_retreat_input(self):
        # Installed A10 advertises both choices. Autoresolve availability is
        # not a requirement to select it; the exact native retreat flag owns R.
        arrived = {"campaign": [{"arrival": {"native_auto": True, "native_retreat": True, "decision": 1}}]}
        probes = [object(), object()]
        stop_at_input = RuntimeError("observed shipping input boundary")
        with patch.object(campaign, "wait_ui", return_value=self.ui()), \
                patch.object(campaign, "scoped_input", side_effect=stop_at_input) as send:
            with self.assertRaisesRegex(RuntimeError, "shipping input boundary"):
                campaign.certify_retreat(probes, lambda: None, [], 0, 1, [7, 8], arrived, 1, 1, 180, {})
            self.assertEqual(send.call_args.args[2], 12)
            send.reset_mock()
            arrived["campaign"][0]["arrival"]["native_retreat"] = False
            with self.assertRaises(campaign.SmokeFailure):
                campaign.certify_retreat(probes, lambda: None, [], 0, 1, [7, 8], arrived, 1, 1, 180, {})
            send.assert_not_called()

    def test_retreat_applied_requires_exact_paused_return_leg_and_four_actor_roster(self):
        returning, arrived = self.return_snapshot(), self.return_snapshot()
        campaign.require_retreat_transit(returning, arrived, [7, 8], self.receipt())
        changes = ({"next_x": 11}, {"destination_x": 9, "destination_y": 1}, {"inc": 9},
                   {"members": returning["groups"][0]["groups"][0]["members"][:-1]}, {"arrival_minutes": 1869})
        for change in changes:
            value = deepcopy(returning)
            value["groups"][0]["groups"][0].update(change)
            with self.subTest(change=change), self.assertRaises(campaign.SmokeFailure):
                campaign.require_retreat_transit(value, arrived, [7, 8], self.receipt())
        for change in ({"paused": False}, {"compression_active": True}, {"arrival": {"decision": 1}},
                       {"world_seconds": 111901}, {"time_control_revision": 3}):
            value = deepcopy(returning)
            value["campaign"][0].update(change)
            with self.subTest(change=change), self.assertRaises(campaign.SmokeFailure):
                campaign.require_retreat_transit(value, arrived, [7, 8], self.receipt())

    def test_peaceful_return_requires_actual_a9_arrival_after_native_eta(self):
        returning, returned = self.return_snapshot(), self.return_snapshot(False)
        campaign.require_return_arrival(returned, returning, [7, 8], self.receipt())
        for area, change in (("campaign", {"world_seconds": 112199}), ("campaign", {"phase": 3}),
                             ("campaign", {"arrival": {"decision": 2}}), ("group", {"x": 10}),
                             ("group", {"between": True}), ("group", {"traverse_minutes": 5})):
            value = deepcopy(returned)
            target = value["campaign"][0] if area == "campaign" else value["groups"][0]["groups"][0]
            target.update(change)
            with self.subTest(change=change), self.assertRaises(campaign.SmokeFailure):
                campaign.require_return_arrival(value, returning, [7, 8], self.receipt())

    def checkpoint_fixture(self, root):
        directory = root / "campaigns" / "campaign-installed-smoke"
        directory.mkdir(parents=True)
        save = bytearray(304)
        save[:4] = (1003).to_bytes(4, "little")
        save[280:284] = (1).to_bytes(4, "little")
        save[284:286] = bytes((7, 10))
        save[291:293] = (4).to_bytes(2, "little")
        save_path = directory / "checkpoint-a.sav"
        save_path.write_bytes(save)
        manifest = bytearray(184)
        manifest[:8] = b"J2DC\x02\x00\x02\x01"
        name = b"installed-smoke"
        manifest[8] = len(name)
        manifest[12:12 + len(name)] = name
        manifest[60] = manifest[80] = 1
        manifest[120:128] = (3).to_bytes(8, "little")
        manifest[128:136] = len(save).to_bytes(8, "little")
        manifest[136:168] = campaign.hashlib.sha256(save).digest()
        manifest[168:176] = (1870).to_bytes(8, "little")
        manifest[180:184] = campaign.zlib.crc32(manifest[:180]).to_bytes(4, "little")
        manifest_path = directory / "manifest-a.bin"
        manifest_path.write_bytes(manifest)
        return manifest_path, save_path

    def test_checkpoint_evidence_requires_committed_hash_time_and_cold_roster_header(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest, save = self.checkpoint_fixture(root)
            record = campaign.checkpoint_record(root)
            self.assertEqual(record["world_minutes"], 1870)
            self.assertEqual(record["save_header"], {"version": 1003, "world_minutes": 1870,
                                                   "mercenary_count": 4, "world_loaded": False})
            seed = {"generation": 2, "save_sha256": "prior"}
            campaign.require_return_checkpoint(record, seed, self.return_snapshot(False), [7, 8])
            # A native arrival quote may already have returned the squad but
            # still own a pause lock; that snapshot cannot certify shutdown.
            for change in ({"pause_locked": True}, {"paused": False}, {"compression_active": True}):
                returned = self.return_snapshot(False)
                returned["campaign"][0].update(change)
                with self.subTest(change=change), self.assertRaises(campaign.SmokeFailure):
                    campaign.require_return_checkpoint(record, seed, returned, [7, 8])
            for change in ({"generation": 2}, {"world_minutes": 1865}, {"save_sha256": "prior"},
                           {"save_header": dict(record["save_header"], world_loaded=True)},
                           {"save_header": dict(record["save_header"], mercenary_count=3)}):
                with self.subTest(change=change), self.assertRaises(campaign.SmokeFailure):
                    campaign.require_return_checkpoint(dict(record, **change), seed, self.return_snapshot(False), [7, 8])
            original = save.read_bytes()
            save.write_bytes(original[:-1] + b"\x01")
            with self.assertRaises(campaign.SmokeFailure):
                campaign.checkpoint_record(root)
            save.write_bytes(original)
            data = bytearray(manifest.read_bytes())
            data[168:176] = (1865).to_bytes(8, "little")
            data[180:184] = campaign.zlib.crc32(data[:180]).to_bytes(4, "little")
            manifest.write_bytes(data)
            with self.assertRaises(campaign.SmokeFailure):
                campaign.checkpoint_record(root)


if __name__ == "__main__":
    unittest.main()
