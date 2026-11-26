"""Stealth probe helpers."""
import unittest

import full_engine_coop_installed_stealth as stealth


def _state(flag):
    return {"actors": [{"slot": 0, "inc": 1, "pose": True, "stealth": flag}]}


class StealthProbeTests(unittest.TestCase):
    def test_shared_flag_only_when_both_clients_agree(self):
        self.assertIs(stealth.shared_stealth([_state(True), _state(True)], (0, 1)), True)
        self.assertIs(stealth.shared_stealth([_state(False), _state(False)], (0, 1)), False)
        self.assertIsNone(stealth.shared_stealth([_state(True), _state(False)], (0, 1)))


if __name__ == "__main__":
    unittest.main()
