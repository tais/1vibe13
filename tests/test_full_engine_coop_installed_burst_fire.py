"""Burst-fire probe helpers."""
import unittest

import full_engine_coop_installed_burst_fire as burst


def _actor(slot, inc, item, mode):
    return {"slot": slot, "inc": inc, "weapon_mode": mode, "primary": {"item": item}}


class BurstFireProbeTests(unittest.TestCase):
    def test_seed_line(self):
        text = "[co-op entry test] combat burst seeded: mercs=4; item=4; burst=3; magazine=15\n"
        self.assertEqual(burst.parse_seed(text), {"mercs": 4, "item": 4, "burst": 3, "magazine": 15})
        self.assertIsNone(burst.parse_seed("nothing"))

    def test_every_controlled_merc_must_show_the_gun_in_burst_on_both_clients(self):
        actors = [_actor(0, 1, 4, 1), _actor(1, 2, 4, 1)]
        states = [{"assigned": [[0, 1]], "actors": actors}, {"assigned": [[1, 2]], "actors": actors}]
        self.assertTrue(burst.armed_for_burst(states, 4))
        single = [_actor(0, 1, 4, 1), _actor(1, 2, 4, 0)]
        self.assertFalse(burst.armed_for_burst([states[0], {"assigned": [[1, 2]], "actors": single}], 4))
        self.assertFalse(burst.armed_for_burst(states, 5))


if __name__ == "__main__":
    unittest.main()
