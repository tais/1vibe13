"""A secured sector must stay loaded until an explicit, confirmed shared return."""
import unittest
from unittest import mock

import full_engine_coop_installed_peaceful_sector as peaceful


class FakeServer:
    def __init__(self):
        self.lines = ["[dedicated] co-op sector secured; peaceful tactical play (3)"]


def ui(sector=3, armed=False, enabled=True, pending=False, ready=True):
    if not ready:
        return {"kind": "return_map_ui", "ready": False}
    return {"kind": "return_map_ui", "ready": True, "session": 7, "control_revision": 9,
            "groups_revision": 4, "sector": sector, "armed": armed, "enabled": enabled, "pending": pending}


class FakeProbe:
    """Answers RETURN_MAP_UI from a mutable view and records queued keys."""
    def __init__(self, server, on_confirm=True):
        self.server, self.on_confirm = server, on_confirm
        self.view = ui()
        self.keys = []
        self.request = 0

    def send(self, verb, *args):
        self.request += 1
        self.last = (verb, args)
        if verb == "RETURN_MAP_INPUT":
            key = args[-1]
            self.keys.append(key)
            if key == 1:
                self.view = ui(armed=True)
            if key == 2 and self.on_confirm:
                self.server.lines.append(peaceful.RETURNING_MARKER)
        return self.request

    def receive(self, request, kind, deadline, guard):
        verb, args = self.last
        if verb == "RETURN_MAP_UI":
            return dict(self.view, request=request)
        return {"request": request, "kind": "return_map_input", "queued": True, "key": args[-1],
                "queued_events": 2, "input_count": len(self.keys)}


class PeacefulSectorTests(unittest.TestCase):
    def run_return(self, probes, server, **kwargs):
        with mock.patch.object(peaceful, "log_text", lambda managed: "\n".join(managed.lines)):
            return peaceful.return_from_secured_sector(probes, lambda: None, server, 5, hold_seconds=0.3, **kwargs)

    def test_confirmed_return_after_hold(self):
        server = FakeServer()
        probes = [FakeProbe(server), FakeProbe(server)]
        evidence = self.run_return(probes, server)
        self.assertEqual(evidence["secured_sector"], 3)
        self.assertEqual(probes[1].keys, [1, 2])
        self.assertEqual(probes[0].keys, [])
        self.assertGreater(evidence["hold_samples"], 0)
        self.assertIs(evidence["physical_input_verified"], False)
        self.assertIs(evidence["paint_verified"], False)

    def test_unprompted_return_fails_the_hold(self):
        server = FakeServer()
        server.lines.append(peaceful.RETURNING_MARKER)
        with self.assertRaises(peaceful.SmokeFailure):
            self.run_return([FakeProbe(server), FakeProbe(server)], server)

    def test_missing_secured_marker_fails(self):
        server = FakeServer()
        server.lines = []
        with mock.patch.object(peaceful.time, "monotonic", side_effect=[0.0, 0.0, 10.0, 10.0]):
            with self.assertRaises(peaceful.SmokeFailure):
                self.run_return([FakeProbe(server), FakeProbe(server)], server)

    def test_unloaded_sector_during_hold_fails(self):
        server = FakeServer()
        probes = [FakeProbe(server), FakeProbe(server)]
        original = probes[0].receive
        calls = {"count": 0}

        def flaky(request, kind, deadline, guard):
            calls["count"] += 1
            answer = original(request, kind, deadline, guard)
            return answer if calls["count"] < 2 else dict(ui(ready=False), request=request)
        probes[0].receive = flaky
        with self.assertRaises(peaceful.SmokeFailure):
            self.run_return(probes, server)

    def test_confirmation_must_reach_the_server(self):
        server = FakeServer()
        probes = [FakeProbe(server), FakeProbe(server, on_confirm=False)]
        with mock.patch.object(peaceful, "log_text", lambda managed: "\n".join(managed.lines)):
            with self.assertRaises(peaceful.SmokeFailure):
                peaceful.return_from_secured_sector(probes, lambda: None, server, 1.0, hold_seconds=0.1)


if __name__ == "__main__":
    unittest.main()
