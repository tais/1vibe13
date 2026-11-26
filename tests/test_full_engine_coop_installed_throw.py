"""Throw probe helpers."""
import unittest

import full_engine_coop_installed_throw as throw


class ThrowProbeTests(unittest.TestCase):
    def test_parses_seed_and_landing(self):
        text = ("[co-op entry test] throw seeded: actor=0:1; item=39; count=3; target=5510\n"
                "[co-op entry test] thrown item landed: grid=5191; distance=2\n")
        self.assertEqual(throw.parse_seed(text), {"actor": (0, 1), "item": 39, "count": 3, "target": 5510})
        self.assertEqual(throw.parse_landing(text), {"grid": 5191, "distance": 2})
        self.assertIsNone(throw.parse_landing("[co-op entry test] nothing"))

    def test_hand_counts_per_client(self):
        actor = {"slot": 0, "inc": 1, "pose": True, "primary": {"item": 39, "quantity": 2}}
        self.assertEqual(throw.hand_counts([{"actors": [actor]}, {"actors": [actor]}], (0, 1)), [(39, 2), (39, 2)])


if __name__ == "__main__":
    unittest.main()
