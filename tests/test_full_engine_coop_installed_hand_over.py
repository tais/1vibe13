"""Hand-over probe helpers."""
import unittest

import full_engine_coop_installed_hand_over as hand_over


class _Fake:
    def __init__(self, answer):
        self.answer, self.sent = answer, []

    def send(self, *args):
        self.sent.append(args)
        return 1

    def receive(self, request, kind, deadline, guard):
        return self.answer


class HandOverProbeTests(unittest.TestCase):
    def test_press_sends_the_exact_visible_selection_and_confirmation(self):
        probe = _Fake({"kind": "hand_over_input", "queued": True})
        ui = {"session": 7, "selected": [3, 4], "armed": [3, 4]}
        hand_over.press(probe, ui, hand_over.KEY_CONFIRM, 0, None)
        self.assertEqual(probe.sent, [("HAND_OVER_INPUT", 7, 3, 4, 3 | (4 << 16), 0, 2)])

    def test_unready_ui_shape_is_not_checked_but_ready_shape_is(self):
        self.assertEqual(hand_over.hand_over_ui(_Fake({"ready": False}), 0, None), {"ready": False})
        with self.assertRaises(Exception):
            hand_over.hand_over_ui(_Fake({"ready": True, "kind": "hand_over_ui"}), 0, None)


if __name__ == "__main__":
    unittest.main()
