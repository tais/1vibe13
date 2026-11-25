"""Exit-grid probe helpers parse exact map facts and publish one trigger per battle."""
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import full_engine_coop_installed_exit_grid as exit_grid


class Managed:
    def __init__(self, lines):
        self.lines = lines


class ExitGridTests(unittest.TestCase):
    def test_exit_grids_filter_by_destination(self):
        server = Managed([
            "[co-op entry test] exit-grid source=12010 destination=10,1,1 grid=7311",
            "[co-op entry test] exit-grid source=100 destination=11,1,0 grid=55",
            "unrelated line",
        ])
        with mock.patch.object(exit_grid, "log_text", lambda managed: "\n".join(managed.lines)):
            self.assertEqual(exit_grid.exit_grids(server, (10, 1, 1)), [(12010, 7311)])
            self.assertEqual(exit_grid.exit_grids(server, (9, 1, 1)), [])

    def test_seed_victory_publishes_exactly_one_world_bound_trigger(self):
        states = [{"ready": True, "world": 3, "turn_serial": 9}, {"ready": True, "world": 3, "turn_serial": 9}]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            record = exit_grid.seed_victory(root, states, 2)
            self.assertEqual((root / "controlled-battle-trigger.txt").read_text(),
                             "JA2_COOP_CONTROLLED_BATTLE_V1 starter-victory 3 9\n")
            self.assertEqual(record, {"world": 3, "turn_serial": 9, "sequence": 2})
            with self.assertRaises(FileExistsError):
                exit_grid.seed_victory(root, states, 2)

    def test_seed_victory_requires_two_clients_on_one_world(self):
        with tempfile.TemporaryDirectory() as directory:
            for states in ([{"ready": True, "world": 3, "turn_serial": 1}],
                           [{"ready": True, "world": 3, "turn_serial": 1}, {"ready": True, "world": 4, "turn_serial": 1}],
                           [{"ready": False, "world": 3, "turn_serial": 1}, {"ready": True, "world": 3, "turn_serial": 1}]):
                with self.subTest(states=states), self.assertRaises(exit_grid.SmokeFailure):
                    exit_grid.seed_victory(Path(directory), states, 1)


if __name__ == "__main__":
    unittest.main()
