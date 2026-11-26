"""Attach probe helpers."""
import unittest

import full_engine_coop_installed_attach as attach


class AttachProbeTests(unittest.TestCase):
    def test_seed_line_and_slots(self):
        text = "[co-op entry test] attach seeded: actor=0:1; gun=1; attachment=207; pocket=25; hand=1"
        self.assertEqual(attach.parse_seed(text), {"actor": (0, 1), "gun": 1, "attachment": 207, "pocket": 25})
        inventory = {"slots": [{"slot": 0, "item": 176}, {"slot": 5, "item": 1}, {"slot": 25, "item": 0}]}
        self.assertEqual(attach.slot_item(inventory, attach.HAND), 1)
        self.assertEqual(attach.slot_item(inventory, 25), 0)
        self.assertIsNone(attach.slot_item(inventory, 40))

    def test_the_hand_is_the_native_hand_slot_not_the_helmet(self):
        self.assertEqual(attach.HAND, 5)


if __name__ == "__main__":
    unittest.main()
