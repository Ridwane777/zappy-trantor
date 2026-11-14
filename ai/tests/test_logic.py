import unittest

from logic.navigation import DIRECTION_MOVES, path_to_tile
from logic.player import Player
from logic.rules import STONES, climb_need
from network.protocol import parse_inventory, parse_level, parse_look, parse_message


class ProtocolTest(unittest.TestCase):
    def test_look(self):
        self.assertEqual(parse_look("[player food, , linemate linemate]"),
                         [["player", "food"], [], ["linemate", "linemate"]])

    def test_inventory(self):
        self.assertEqual(parse_inventory("[food 12, linemate 3, sibur x]"), {"food": 12, "linemate": 3})

    def test_message(self):
        self.assertEqual(parse_message("message 3, HOME|2|40|0,0,0,0,0,0"), (3, "HOME|2|40|0,0,0,0,0,0"))
        self.assertEqual(parse_message("message 0, a, b"), (0, "a, b"))
        self.assertIsNone(parse_message("ok"))
        self.assertIsNone(parse_message("message x, hi"))

    def test_level(self):
        self.assertEqual(parse_level("Current level: 5"), 5)
        self.assertIsNone(parse_level("ko"))


class NavigationTest(unittest.TestCase):
    def test_path_to_look_tiles(self):
        self.assertEqual(path_to_tile(0), [])
        self.assertEqual(path_to_tile(2), ["Forward"])                     # row 1, center
        self.assertEqual(path_to_tile(1), ["Forward", "Left", "Forward"])  # row 1, left
        self.assertEqual(path_to_tile(3), ["Forward", "Right", "Forward"]) # row 1, right
        self.assertEqual(path_to_tile(4), ["Forward", "Forward", "Left", "Forward", "Forward"])

    def test_every_direction_has_moves(self):
        self.assertEqual(sorted(DIRECTION_MOVES), list(range(9)))
        self.assertEqual(DIRECTION_MOVES[0], [])
        for k in range(1, 9):
            self.assertEqual(DIRECTION_MOVES[k][-1], "Forward")


class RulesAndPlayerTest(unittest.TestCase):
    def test_climb_need_from_level_two(self):
        need = climb_need(2)
        self.assertEqual(need, {"linemate": 8, "deraumere": 8, "sibur": 10,
                                "mendiane": 5, "phiras": 6, "thystame": 1})
        self.assertEqual(climb_need(8), {s: 0 for s in STONES})

    def test_player_level_only_goes_up(self):
        p = Player()
        p.reach_level(3)
        p.reach_level(2)
        p.reach_level(None)
        p.reach_level(9)
        self.assertEqual(p.level, 3)

    def test_player_inventory_comes_from_the_server(self):
        p = Player()
        p.update_inventory({"food": 7, "sibur": 2})
        self.assertEqual(p.food, 7)
        self.assertEqual(p.count("linemate"), 0)
        self.assertTrue(p.carries_any({"sibur": 1}))
        self.assertFalse(p.carries_any({"linemate": 1}))


if __name__ == "__main__":
    unittest.main()
