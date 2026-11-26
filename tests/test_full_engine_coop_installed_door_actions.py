"""Door-action probe helpers."""
import unittest

import full_engine_coop_installed_door_actions as doors


SEED = ("[co-op entry test] locked door seeded: actor=0:1; door=21061; structure=3052; stand=21061; "
        "distance=105; kit=204")


class DoorActionProbeTests(unittest.TestCase):
    def test_parses_the_seed(self):
        self.assertEqual(doors.parse_seed("x\n" + SEED + "\n"),
                         {"actor": (0, 1), "door": 21061, "structure": 3052, "stand": 21061, "distance": 105, "kit": 204})
        self.assertIsNone(doors.parse_seed("[co-op entry test] nothing"))

    def test_lock_states_in_order(self):
        text = ("[co-op entry test] seeded door locked=1\n[x]\n[co-op entry test] seeded door locked=0\n")
        self.assertEqual(doors.lock_states(text), [1, 0])

    def test_owner_must_be_unique(self):
        states = [{"assigned": [[0, 1]]}, {"assigned": [[2, 3]]}]
        self.assertEqual(doors.owner_of(states, (0, 1)), 0)
        with self.assertRaises(doors.SmokeFailure):
            doors.owner_of([{"assigned": [[0, 1]]}, {"assigned": [[0, 1]]}], (0, 1))


if __name__ == "__main__":
    unittest.main()
