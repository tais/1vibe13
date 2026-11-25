"""NPC dialogue probe helpers: stock talk radius, tile choice and fixture parsing."""
import unittest
from unittest import mock

import full_engine_coop_installed_npc_dialogue as dialogue


class Managed:
    def __init__(self, lines):
        self.lines = lines


def state(actors, columns=10, rows=10):
    return {"columns": columns, "rows": rows, "actors": actors}


def actor(grid, level=0, life=50):
    return {"grid": grid, "level": level, "life": life}


class NpcDialogueTests(unittest.TestCase):
    def test_pyth_spaces_matches_stock_integer_distance(self):
        self.assertEqual(dialogue.pyth_spaces(0, 0, 10), 0)
        self.assertEqual(dialogue.pyth_spaces(0, 3, 10), 3)
        self.assertEqual(dialogue.pyth_spaces(0, 33, 10), 4)  # sqrt(18) -> 4
        self.assertEqual(dialogue.pyth_spaces(55, 44, 10), 1)

    def test_talk_tiles_prefer_adjacent_free_in_range_tiles(self):
        npc, merc, bystander = actor(55), actor(11), actor(56)
        tiles = dialogue.talk_tiles(state([npc, merc, bystander]), merc, npc)
        self.assertNotIn(55, tiles)
        self.assertNotIn(56, tiles, "occupied tiles are never chosen")
        self.assertTrue(all(dialogue.pyth_spaces(tile, 55, 10) < dialogue.TALK_RADIUS for tile in tiles))
        self.assertEqual(tiles[0], 45, "adjacent to the NPC first, then nearest to the merc")
        self.assertTrue(all(abs(tile // 10 - 5) <= 1 and abs(tile % 10 - 5) <= 1 for tile in tiles[:7]))

    def test_talk_tiles_stay_inside_the_map(self):
        npc = actor(0)
        tiles = dialogue.talk_tiles(state([npc]), actor(99), npc)
        self.assertTrue(tiles and all(0 <= tile < 100 and tile % 10 <= 2 for tile in tiles))

    def test_fixture_npc_position_uses_the_latest_line(self):
        server = Managed([
            "[co-op entry test] npc profile=101 grid=11291 level=0",
            "[co-op entry test] npc profile=114 grid=11289 level=0",
            "[co-op entry test] npc profile=101 grid=11300 level=1",
        ])
        with mock.patch.object(dialogue, "log_text", lambda managed: "\n".join(managed.lines)):
            self.assertEqual(dialogue.fixture_npc_position(server, 101), {"grid": 11300, "level": 1})
            self.assertEqual(dialogue.fixture_npc_position(server, 114), {"grid": 11289, "level": 0})
            self.assertIsNone(dialogue.fixture_npc_position(server, 57))

    def test_quote_text_decodes_utf16_units(self):
        self.assertEqual(dialogue.quote_text({"quote": [34, 87, 104, 121, 34]}), '"Why"')


if __name__ == "__main__":
    unittest.main()
