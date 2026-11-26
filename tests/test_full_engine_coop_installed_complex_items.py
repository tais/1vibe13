"""Complex-items probe helpers."""
import unittest

import full_engine_coop_installed_complex_items as complex_items

SEED = {"actor": (0, 1), "gun": 1, "attachment": 207, "attached": 1, "key": 271, "grenade": 131, "pocket": 26}


class ComplexItemsProbeTests(unittest.TestCase):
    def test_seed_line(self):
        text = ("[co-op entry test] complex items seeded: actor=0:1; gun=1; attachment=207; attached=1; "
                "key=271; grenade=131; pocket=26")
        self.assertEqual(complex_items.parse_seed(text), SEED)

    def test_support_and_ground_lookups(self):
        inventory = {"actor": [0, 1], "slots": [{"slot": 0, "item": 1, "support": 1},
                                                {"slot": 20, "item": 271, "support": 1},
                                                {"slot": 21, "item": 131, "support": 2},
                                                {"slot": 22, "item": 5, "support": 2}],
                     "ground": {"items": [{"id": [9, 3], "summary": {"item": 131}}]}}
        self.assertEqual(complex_items.loot_support(inventory, SEED), {1: 1, 271: 1, 131: 2})
        self.assertEqual(complex_items.ground_grenade(inventory, 131), ((9, 3), {"item": 131}))
        self.assertEqual(complex_items.ground_grenade(inventory, 999), (None, None))
        self.assertIs(complex_items.inventory_of({"inventories": [inventory]}, (0, 1)), inventory)


if __name__ == "__main__":
    unittest.main()
