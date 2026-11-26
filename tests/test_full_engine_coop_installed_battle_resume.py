"""Battle resume probe helpers."""
import unittest

import full_engine_coop_installed_battle_resume as resume


class BattleResumeProbeTests(unittest.TestCase):
    def test_battle_view_keeps_only_in_sector_actors(self):
        state = {"actors": [{"slot": 0, "grid": 10, "life": 70, "active": True, "in_sector": True},
                            {"slot": 1, "grid": 11, "life": 0, "active": True, "in_sector": False},
                            {"slot": 2, "grid": 12, "life": 40, "active": False, "in_sector": True}]}
        self.assertEqual(resume.battle_view(state), {0: (10, 70)})

    def test_ordered_markers(self):
        text = "\n".join(resume.STOP_MARKERS)
        self.assertTrue(resume.ordered(text, resume.STOP_MARKERS))
        self.assertFalse(resume.ordered("\n".join(reversed(resume.STOP_MARKERS)), resume.STOP_MARKERS))

    def test_kill_probe_builds_on_the_shared_steps(self):
        import full_engine_coop_installed_battle_kill as kill
        for step in ("start_new_battle", "stop_and_save_battle", "resume_battle", "apply_one_move"):
            self.assertIs(getattr(kill, step), getattr(resume, step))


if __name__ == "__main__":
    unittest.main()
