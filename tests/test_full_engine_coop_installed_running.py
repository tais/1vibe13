"""Running probe helpers."""
import unittest

import full_engine_coop_installed_running as running


def _state():
    actors = [{"slot": 0, "inc": 1, "grid": 10 * 20 + 10, "life": 50, "stance": 1, "pose": True},
              {"slot": 1, "inc": 2, "grid": 10 * 20 + 16, "life": 50, "stance": 2, "pose": True}]
    return {"columns": 20, "rows": 20, "assigned": [[0, 1], [1, 2]], "actors": actors}


class RunningProbeTests(unittest.TestCase):
    def test_destinations_skip_occupied_and_off_map_tiles(self):
        found = running.destinations(_state(), (0, 1))
        self.assertNotIn(10 * 20 + 16, found)
        self.assertEqual(found[0], 16 * 20 + 10)
        self.assertTrue(all(0 <= grid < 400 for grid in found))

    def test_only_a_standing_controlled_merc_runs(self):
        self.assertEqual(running.standing_merc(_state(), 0), (0, 1))
        state = _state(); state["actors"][0]["stance"] = 2
        self.assertIsNone(running.standing_merc(state, 0))


if __name__ == "__main__":
    unittest.main()
