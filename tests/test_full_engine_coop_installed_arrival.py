"""Build-free checks for the opt-in arrival fixture, especially PNG completion."""
import contextlib
import io
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import full_engine_coop_installed_arrival as arrival


class InstalledArrivalTests(unittest.TestCase):
    def arguments(self, *extra):
        return arrival.parse_arguments(["--server-executable", "/fixture/server", "--client-executable", "/fixture/client",
                                        "--installed-data-root", "/fixture/data", *extra])

    def test_explicit_executables_and_bounded_deadlines(self):
        self.assertEqual(self.arguments().timeout_seconds, 180)
        for value in ("1", "1.25", "600"):
            self.assertEqual(self.arguments(f"--timeout-seconds={value}").timeout_seconds, float(value))
        for value in ("0", "-1", "601", "nan", "inf", "-inf"):
            with self.subTest(value=value), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.arguments(f"--timeout-seconds={value}")
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            arrival.parse_arguments([])

    def test_capture_waits_for_native_completion_after_zero_length_file_exists(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = root / "Screenshots/SCREEN00000.png"
            image.parent.mkdir(); image.touch()
            client = SimpleNamespace(log_path=root / "client.stdout.log")
            with patch.object(arrival, "wait_for_marker", side_effect=lambda *_: image.write_bytes(b"completed native PNG")) as wait:
                self.assertEqual(arrival.wait_for_capture(client, self.response(), 2, 3, 5), str(image))
            wait.assert_called_once_with(client, "[video] Saved screenshot: Screenshots/SCREEN00000.png", 5)

    def response(self):
        return {"request": 1, "kind": "campaign_capture", "queued": True, "session": 2, "decision": 3,
                "screenshot": "Screenshots/SCREEN00000.png"}

    def test_capture_rejects_unscoped_response_without_waiting_or_reading_a_path(self):
        for key, value in (("request", True), ("kind", "other"), ("queued", 1), ("session", 3), ("session", 2.0),
                           ("decision", 2), ("decision", 3.0), ("screenshot", "/arbitrary/image.png")):
            with self.subTest(key=key), patch.object(arrival, "wait_for_marker") as wait, self.assertRaises(arrival.SmokeFailure):
                arrival.wait_for_capture(None, dict(self.response(), **{key: value}), 2, 3, 1)
            wait.assert_not_called()

    def test_capture_refuses_missing_empty_or_redirected_native_artifacts(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(arrival, "wait_for_marker"):
            root = Path(directory); client = SimpleNamespace(log_path=root / "client.stdout.log")
            image = root / "Screenshots/SCREEN00000.png"; image.parent.mkdir()
            for create in (lambda: None, image.touch, lambda: (image.unlink(), image.symlink_to(root / "target.png"))):
                create()
                with self.assertRaises(arrival.SmokeFailure):
                    arrival.wait_for_capture(client, self.response(), 2, 3, 1)


if __name__ == "__main__":
    unittest.main()
