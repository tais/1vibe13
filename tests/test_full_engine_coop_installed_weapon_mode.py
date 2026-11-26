"""Weapon-mode probe helpers."""
import unittest

import full_engine_coop_installed_weapon_mode as weapon_mode


class WeaponModeProbeTests(unittest.TestCase):
    def test_shared_mode_needs_agreement(self):
        states = [{"actors": [{"slot": 0, "inc": 1, "pose": True, "weapon_mode": 1}]}, {"actors": [{"slot": 0, "inc": 1, "pose": True, "weapon_mode": 1}]}]
        self.assertEqual(weapon_mode.shared_mode(states, (0, 1)), 1)
        states[1]["actors"][0]["weapon_mode"] = 0
        self.assertIsNone(weapon_mode.shared_mode(states, (0, 1)))

    def test_fire_modes_must_be_reached_and_left(self):
        self.assertTrue(weapon_mode.reaches_fire_modes({"0:1": [0, 1, 0]}))
        self.assertTrue(weapon_mode.reaches_fire_modes({"0:1": [0, 1, 2, 0]}))
        self.assertFalse(weapon_mode.reaches_fire_modes({"0:1": [0, 1]}))
        self.assertFalse(weapon_mode.reaches_fire_modes({"0:1": [0, 0]}))


if __name__ == "__main__":
    unittest.main()
