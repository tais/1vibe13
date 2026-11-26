"""Grenade probe helpers."""
import unittest

import full_engine_coop_installed_grenade as grenade


def _actor(slot, inc, x, hostile=False, life=50, item=0, quantity=0, grid=0):
    return {"slot": slot, "inc": inc, "world_x": x, "world_y": 0, "hostile": hostile, "life": life,
            "active": True, "in_sector": True, "grid": grid, "primary": {"item": item, "quantity": quantity}}


class GrenadeProbeTests(unittest.TestCase):
    def test_seed_and_explosion_lines(self):
        self.assertEqual(grenade.parse_seed("[co-op entry test] combat grenade seeded: mercs=4; item=134; count=2"),
                         {"mercs": 4, "item": 134, "count": 2})
        self.assertEqual(grenade.parse_explosion("[co-op entry test] grenade exploded: item=134; grid=6132"),
                         {"item": 134, "grid": 6132})

    def test_candidates_pair_controlled_grenadiers_with_living_enemies_nearest_first(self):
        actors = [_actor(0, 1, 0, item=134, quantity=2), _actor(1, 2, 0, item=134, quantity=1),
                  _actor(9, 1, 50, hostile=True, grid=11), _actor(8, 1, 20, hostile=True, grid=12),
                  _actor(7, 1, 5, hostile=True, life=0, grid=13)]
        states = [{"assigned": [[0, 1], [1, 2]], "actors": actors}, {"assigned": [], "actors": actors}]
        found = grenade.candidates(states, 134, 2)
        self.assertEqual([(owner, thrower, target, grid) for _, owner, thrower, target, grid in found],
                         [(0, (0, 1), (8, 1), 12), (0, (0, 1), (9, 1), 11)])


if __name__ == "__main__":
    unittest.main()
