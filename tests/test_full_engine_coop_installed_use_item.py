"""Use-item probe helpers."""
import unittest

import full_engine_coop_installed_use_item as use_item


class UseItemProbeTests(unittest.TestCase):
    def test_seed_is_read_for_the_right_merc(self):
        text = "[co-op entry test] thirsty: actor=1:2; breath=60; canteen=214\n"
        self.assertEqual(use_item.thirsty(text, (1, 2)), {"breath": 60, "canteen": 214})
        self.assertIsNone(use_item.thirsty(text, (0, 1)))

    def test_breath_needs_both_clients_to_agree(self):
        state = lambda breath: {"actors": [{"slot": 1, "inc": 2, "pose": True, "breath": breath}]}
        self.assertEqual(use_item.shared_breath([state(80), state(80)], (1, 2)), 80)
        self.assertIsNone(use_item.shared_breath([state(80), state(60)], (1, 2)))


if __name__ == "__main__":
    unittest.main()
