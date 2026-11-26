"""Secured-sector stop probe helpers."""
import unittest

import full_engine_coop_installed_secured_stop as stop


class SecuredStopProbeTests(unittest.TestCase):
    def test_marker_lines(self):
        self.assertEqual(stop.parse_placed("x\n[co-op entry test] sector marker placed: grid=4870; item=201\n"),
                         {"grid": 4870, "item": 201})
        self.assertEqual(stop.parse_found("[co-op entry test] sector marker missing: grid=4870; item=201"),
                         {"found": False, "grid": 4870, "item": 201})
        self.assertIsNone(stop.parse_found("nothing here"))

    def test_stop_sequence_needs_every_step_in_order_and_no_failure(self):
        ordered = "\n".join(stop.STOP_MARKERS)
        self.assertTrue(stop.stop_sequence_ok(ordered))
        self.assertFalse(stop.stop_sequence_ok("\n".join(reversed(stop.STOP_MARKERS))))
        self.assertFalse(stop.stop_sequence_ok(ordered + "\n[dedicated] final co-op checkpoint failed: x"))


if __name__ == "__main__":
    unittest.main()
