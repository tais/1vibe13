"""Fire-at-tile probe helpers."""
import unittest

import full_engine_coop_installed_fire_tile as fire_tile


def actor(slot, grid, hostile=False, ammo=6, x=0, y=0):
    return {"slot": slot, "inc": 1, "grid": grid, "active": True, "in_sector": True, "hostile": hostile,
            "life": 50, "world_x": x, "world_y": y, "primary": {"ammo_count": ammo}}


class FireTileProbeTests(unittest.TestCase):
    def test_an_empty_tile_beside_the_enemy_is_chosen(self):
        state = {"assigned": [[1, 1]], "actors": [actor(1, 100), actor(9, 205, hostile=True, x=10)]}
        options = fire_tile.shooters_and_tiles(state, 10)
        self.assertEqual(options[0][1:], ((1, 1), 206))

    def test_occupied_tiles_and_empty_guns_are_skipped(self):
        state = {"assigned": [[1, 1], [2, 1]],
                 "actors": [actor(1, 100, ammo=0), actor(2, 206), actor(9, 205, hostile=True)]}
        options = fire_tile.shooters_and_tiles(state, 10)
        self.assertEqual(options[0][1:], ((2, 1), 204))


if __name__ == "__main__":
    unittest.main()
