"""Self-aid probe helpers."""
import unittest

import full_engine_coop_installed_self_aid as self_aid


class SelfAidProbeTests(unittest.TestCase):
    def test_wound_is_read_for_the_right_merc(self):
        text = "[co-op entry test] wounded: actor=1:2; health=56; bleeding=15; kit=201\n"
        self.assertEqual(self_aid.wound(text, (1, 2)), {"health": 56, "bleeding": 15, "kit": 201})
        self.assertIsNone(self_aid.wound(text, (0, 1)))

    def test_medical_view_needs_both_clients_and_a_self_patient(self):
        def state(bleeding, providing, patient):
            return {"actors": [{"slot": 1, "inc": 2, "pose": True, "bleeding": bleeding,
                                "hand_medical_kit_points": 90,
                                "medical_service": {"providing": providing, "patient_slot": patient[0],
                                                    "patient_inc": patient[1], "receiving_count": 1}}]}
        treating = state(15, True, (1, 2))
        self.assertEqual(self_aid.medical_view([treating, treating], (1, 2)), (15, 90, True))
        other = state(15, True, (0, 1))
        self.assertEqual(self_aid.medical_view([other, other], (1, 2)), (15, 90, False))
        self.assertIsNone(self_aid.medical_view([treating, state(14, True, (1, 2))], (1, 2)))


if __name__ == "__main__":
    unittest.main()
