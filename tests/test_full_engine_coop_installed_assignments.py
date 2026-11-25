"""Assignment probe helpers: Screen key plans and replicated member lookup."""
import unittest

import full_engine_coop_installed_assignments as assignments


class AssignmentProbeTests(unittest.TestCase):
    def test_keys_follow_the_menu_order(self):
        a = assignments
        self.assertEqual(a.keys_for(a.SQUAD, 0), [])
        self.assertEqual(a.keys_for(a.DOCTOR, 0), [a.KEY_DOWN])
        self.assertEqual(a.keys_for(a.TRAIN_SELF, a.STRENGTH), [a.KEY_DOWN] * 4 + [a.KEY_RIGHT] * 3)
        self.assertEqual(a.keys_for(8, 0), [a.KEY_DOWN] * 6)
        with self.assertRaises(ValueError):
            a.keys_for(6, 0)  # militia training is not offered yet

    def test_member_assignment_finds_the_exact_actor(self):
        reply = {"groups": [{"slot": 3, "inc": 4, "members": [{"slot": 0, "inc": 1, "profile": 239, "assignment": 0}]},
                            {"slot": 5, "inc": 9, "members": [{"slot": 1, "inc": 2, "profile": 238, "assignment": 47}]}]}
        self.assertEqual(assignments.member_assignment(reply, [1, 2]), ([5, 9], 47))
        self.assertEqual(assignments.member_assignment(reply, (0, 1)), ([3, 4], 0))
        self.assertEqual(assignments.member_assignment(reply, [1, 3]), (None, None))


if __name__ == "__main__":
    unittest.main()
