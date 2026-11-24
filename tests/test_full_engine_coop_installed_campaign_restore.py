"""Cold-restore evidence must bind real saved state without reusing live handles."""
import contextlib
from copy import deepcopy
import io
from pathlib import Path
import tempfile
import unittest

import full_engine_coop_installed_campaign_restore as restore
from full_engine_coop_installed_campaign_status import EMPTY_ARRIVAL


class InstalledCampaignRestoreTests(unittest.TestCase):
    def snapshot(self, session=2):
        status = {"request": 1, "kind": "campaign", "ready": True, "session": session, "revision": 3,
                  "world_seconds": 112200, "phase": 2, "compression_mode": 0, "paused": True, "pause_locked": False,
                  "compression_active": False, "time_interrupted": False, "leader_revision": 1,
                  "leader_ready": True, "ready_peers": 2, "local_leader": True, "time_control_revision": 3,
                  "leader": [1] + [0] * 15, "arrival": deepcopy(EMPTY_ARRIVAL)}
        group = {"slot": 7, "inc": 8, "x": 9, "y": 1, "z": 0, "next_x": 0, "next_y": 0,
                 "between": False, "vehicle": False, "transport": 1, "destination_x": 0, "destination_y": 0,
                 "arrival_minutes": 0, "traverse_minutes": 0,
                 "members": [{"slot": i, "inc": i + 1, "profile": i + 20, "assignment": 0} for i in range(4)]}
        groups = {"request": 2, "kind": "groups", "ready": True, "session": session, "revision": 2, "available": True, "groups": [group]}
        return {"campaign": [status, dict(deepcopy(status), local_leader=False)], "groups": [groups, deepcopy(groups)]}

    def test_explicit_inputs_and_finite_bounded_timeout(self):
        arguments = ["--source-evidence", "/source/evidence.json", "--server-executable", "/shipping/server",
                     "--client-executable", "/driver/client", "--installed-data-root", "/installed"]
        self.assertEqual(restore.parse_arguments(arguments).timeout_seconds, 180)
        for value in ("nan", "inf", "-inf", "0", "601"):
            with self.subTest(value=value), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                restore.parse_arguments(arguments + [f"--timeout-seconds={value}"])

    def test_fresh_session_may_regenerate_handles_but_must_restore_semantic_roster(self):
        previous, current = self.snapshot(), self.snapshot(3)
        for observation in current["groups"]:
            observation["groups"][0].update(slot=9, inc=99)
            for member in observation["groups"][0]["members"]:
                member["slot"] += 10
                member["inc"] += 200
        before = deepcopy(current)
        restore.require_restored(current, previous)
        self.assertEqual(current, before)
        with self.assertRaises(restore.SmokeFailure):
            restore.require_restored(previous, previous)

    def test_header_count_alone_cannot_prove_live_restored_roster_or_routes(self):
        for area, key, replacement in (("member", "profile", 70), ("member", "assignment", 1),
                                       ("group", "x", 10), ("group", "transport", 2),
                                       ("group", "destination_x", 10)):
            current = self.snapshot(3)
            for observation in current["groups"]:
                group = observation["groups"][0]
                target = group["members"][0] if area == "member" else group
                target[key] = replacement
                if key == "destination_x":
                    group["destination_y"] = 1
            with self.subTest(key=key), self.assertRaises(restore.SmokeFailure):
                restore.require_restored(current, self.snapshot())

    def test_live_restore_requires_both_peers_cold_same_time_and_new_session(self):
        for change in ({"world_seconds": 112260}, {"phase": 3}, {"pause_locked": True}, {"paused": False}):
            current = self.snapshot(3)
            for status in current["campaign"]:
                status.update(change)
            with self.subTest(change=change), self.assertRaises(restore.SmokeFailure):
                restore.require_restored(current, self.snapshot())
        current = self.snapshot(3)
        current["groups"][1]["groups"][0]["members"][0]["inc"] += 1
        with self.assertRaises(restore.SmokeFailure):
            restore.require_restored(current, self.snapshot())

    def test_copied_checkpoint_must_match_committed_bytes_and_generation(self):
        expected = {"slot": "a", "generation": 3, "world_minutes": 1870, "save_size": 99,
                    "save_sha256": "saved", "manifest_sha256": "committed", "save_header": {"mercenary_count": 4}}
        restore.require_checkpoint_match(dict(expected, save_path="/new/private/path"), expected)
        for key, value in (("slot", "b"), ("generation", 2), ("world_minutes", 1860), ("save_size", 98),
                           ("save_sha256", "other"), ("manifest_sha256", "other"), ("save_header", {})):
            with self.subTest(key=key), self.assertRaises(restore.SmokeFailure):
                restore.require_checkpoint_match(dict(expected, **{key: value}), expected)

    def test_private_state_digest_detects_changes_and_refuses_outside_links(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "state"
            root.mkdir()
            value = root / "credential.bin"
            value.write_bytes(b"private test input")
            before = restore.private_state_digest(root)
            value.write_bytes(b"changed test input")
            self.assertNotEqual(restore.private_state_digest(root), before)
            value.write_bytes(b"private test input")
            self.assertEqual(restore.private_state_digest(root), before)
            (root / "external").symlink_to(Path(temporary))
            with self.assertRaises(restore.SmokeFailure):
                restore.private_state_digest(root)


if __name__ == "__main__":
    unittest.main()
