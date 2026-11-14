"""Strategy tests against a fake game API (no server needed)."""
import unittest

from comm.jammer import Jammer
from comm.messages import Here, Home
from comm.secure import SecureChannel
from logic.pack import PackQueen, PackWorker
from logic.rules import STONES, climb_need


class FakeApi:
    """Records commands; serves a scripted inventory, tile and messages."""

    def __init__(self, inventory=None, tile=None, messages=None, incant_result=0):
        self.team = "red"
        self.free_slots = 0
        self.inv = dict(inventory or {})
        self.tile = list(tile or [])
        self._messages = list(messages or [])
        self.incant_result = incant_result
        self.log = []

    def inventory(self):
        return dict(self.inv)

    def announced_level(self):
        return None

    def look(self):
        return [list(self.tile)]

    def messages(self):
        out, self._messages = self._messages, []
        return out

    def walk(self, moves):
        self.log.extend(moves)

    def take(self, item):
        self.log.append(f"Take {item}")
        if item in self.tile:
            self.tile.remove(item)
            self.inv[item] = self.inv.get(item, 0) + 1
            return True
        return False

    def drop(self, item):
        self.log.append(f"Set {item}")
        if self.inv.get(item, 0) > 0:
            self.inv[item] -= 1
            self.tile.append(item)
            return True
        return False

    def broadcast(self, text):
        self.log.append(f"Broadcast {text}")

    def incant(self):
        self.log.append("Incantation")
        return self.incant_result

    def connect_nbr(self):
        return 0

    def fork(self):
        self.log.append("Fork")


class NoRecruit:
    def tick(self):
        pass


def full_stock(level=2):
    return {s: n for s, n in climb_need(level).items()}


def roll_call(count, level=2, direction=0):
    """`count` authentic Here messages from distinct workers."""
    return [(direction, SecureChannel("red", secret="s", sender_id=100 + i).seal(Here(i, level).encode()))
            for i in range(count)]


class QueenTest(unittest.TestCase):
    def make(self, api):
        queen = PackQueen(api, SecureChannel("red", secret="s", sender_id=1), NoRecruit(), Jammer(every=1))
        queen.player.reach_level(2)
        queen.settled = True
        return queen

    def test_broadcasts_an_encrypted_home(self):
        api = FakeApi(inventory={"food": 50})
        self.make(api).turn()
        broadcasts = [c for c in api.log if c.startswith("Broadcast ")]
        self.assertTrue(broadcasts)
        token = broadcasts[0][len("Broadcast "):]
        home = Home.decode(SecureChannel("red", secret="s", sender_id=2).open(token))
        self.assertEqual(home.level, 2)
        self.assertEqual(home.deficit, climb_need(2))

    def test_waits_while_the_stock_is_incomplete(self):
        api = FakeApi(inventory={"food": 50}, tile=["player"] * 8)
        self.make(api).turn()
        self.assertNotIn("Incantation", api.log)

    def test_enemies_on_the_tile_do_not_count(self):
        api = FakeApi(inventory={"food": 50, **full_stock()}, tile=["player"] * 9)
        self.make(api).turn()
        self.assertNotIn("Incantation", api.log)

    def test_roll_call_ignores_other_tiles_and_levels(self):
        messages = roll_call(3) + roll_call(3, direction=4) + roll_call(3, level=3)
        api = FakeApi(inventory={"food": 50, **full_stock()}, messages=messages)
        queen = self.make(api)
        queen.turn()
        self.assertEqual(queen.team_at_home(), 4)
        self.assertNotIn("Incantation", api.log)

    def test_climbs_when_stock_and_group_are_ready(self):
        stock = full_stock()
        api = FakeApi(inventory={"food": 50, **stock}, tile=["player"] * 7, messages=roll_call(6), incant_result=3)
        queen = self.make(api)
        queen.turn()
        self.assertIn("Incantation", api.log)
        self.assertEqual(queen.player.level, 3)
        dropped = [c[len("Set "):] for c in api.log if c.startswith("Set ")]
        self.assertEqual(sorted(dropped), sorted(["linemate", "deraumere", "sibur"]))

    def test_takes_stones_back_when_the_incantation_fails(self):
        api = FakeApi(inventory={"food": 50, **full_stock()}, tile=["player"] * 7, messages=roll_call(6),
                      incant_result=0)
        queen = self.make(api)
        queen.turn()
        self.assertEqual(queen.player.level, 2)
        self.assertEqual(api.inv["linemate"], full_stock()["linemate"])

    def test_jams_enemy_messages_but_not_its_own_team(self):
        own = SecureChannel("red", secret="s", sender_id=7).seal("HOME|2|40|0,0,0,0,0,0")
        api = FakeApi(inventory={"food": 50}, messages=[(1, "NEED:blue:3:4242"), (2, own)])
        self.make(api).turn()
        decoys = [c for c in api.log if c.startswith("Broadcast NEED:blue:")]
        self.assertEqual(len(decoys), 1)


class WorkerTest(unittest.TestCase):
    def make(self, api):
        worker = PackWorker(api, SecureChannel("red", secret="s", sender_id=2))
        worker.player.reach_level(2)
        return worker

    def home_message(self, level=2, deficit=None, sender=1, food=40):
        home = Home(level, food, deficit if deficit is not None else {s: 0 for s in STONES})
        return SecureChannel("red", secret="s", sender_id=sender).seal(home.encode())

    def test_walks_toward_a_fresh_home_sound(self):
        api = FakeApi(inventory={"food": 40}, messages=[(3, self.home_message())])
        self.make(api).turn()
        self.assertEqual(api.log[:2], ["Right", "Forward"])

    def test_ignores_forged_or_foreign_home_messages(self):
        forged = "HOME|2|40|0,0,0,0,0,0"   # clear text: not from our team
        foreign = SecureChannel("blue", secret="s", sender_id=9).seal("HOME|2|40|0,0,0,0,0,0")
        api = FakeApi(inventory={"food": 50}, messages=[(3, forged), (5, foreign)])
        worker = self.make(api)
        worker.turn()
        self.assertIsNone(worker.home)

    def test_hands_its_stones_over_at_home(self):
        api = FakeApi(inventory={"food": 40, "sibur": 2}, messages=[(0, self.home_message())])
        self.make(api).turn()
        self.assertEqual(api.log.count("Set sibur"), 2)

    def test_answers_the_roll_call_while_waiting_at_home(self):
        api = FakeApi(inventory={"food": 40}, messages=[(0, self.home_message())])
        worker = self.make(api)
        worker.turn()
        calls = [c[len("Broadcast "):] for c in api.log if c.startswith("Broadcast ")]
        self.assertEqual(len(calls), 1)
        here = Here.decode(SecureChannel("red", secret="s", sender_id=1).open(calls[0]))
        self.assertEqual((here.worker, here.level), (worker.worker_id, 2))

    def test_feeds_a_hungry_queen(self):
        api = FakeApi(inventory={"food": 40}, messages=[(0, self.home_message(food=10))])
        self.make(api).turn()
        self.assertEqual(api.log.count("Set food"), 5)


if __name__ == "__main__":
    unittest.main()
