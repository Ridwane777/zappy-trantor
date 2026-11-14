import random
import unittest

from comm.jammer import Jammer
from comm.messages import Here, Home
from comm.secure import SecureChannel


class SecureChannelTest(unittest.TestCase):
    def setUp(self):
        self.queen = SecureChannel("red", secret="s", sender_id=1)
        self.worker = SecureChannel("red", secret="s", sender_id=2)

    def test_round_trip(self):
        token = self.queen.seal("HOME|3|40|1,2,3,4,5,6")
        self.assertEqual(self.worker.open(token), "HOME|3|40|1,2,3,4,5,6")

    def test_ciphertext_hides_the_content(self):
        token = self.queen.seal("HOME|3|40|1,2,3,4,5,6")
        self.assertNotIn("HOME", token)
        self.assertNotIn("\n", token)

    def test_other_team_cannot_read(self):
        enemy = SecureChannel("blue", secret="s", sender_id=3)
        self.assertIsNone(enemy.open(self.queen.seal("HOME|3|40|0,0,0,0,0,0")))

    def test_other_secret_cannot_read(self):
        stranger = SecureChannel("red", secret="other", sender_id=3)
        self.assertIsNone(stranger.open(self.queen.seal("HOME|3|40|0,0,0,0,0,0")))

    def test_tampered_message_is_rejected(self):
        token = bytearray(self.queen.seal("HOME|3|40|0,0,0,0,0,0").encode())
        token[20] = ord("A") if token[20] != ord("A") else ord("B")
        self.assertIsNone(self.worker.open(token.decode()))

    def test_replay_is_rejected(self):
        token = self.queen.seal("HOME|3|40|0,0,0,0,0,0")
        self.assertIsNotNone(self.worker.open(token))
        self.assertIsNone(self.worker.open(token))

    def test_older_message_is_rejected_after_a_newer_one(self):
        old = self.queen.seal("a")
        new = self.queen.seal("b")
        self.assertEqual(self.worker.open(new), "b")
        self.assertIsNone(self.worker.open(old))

    def test_garbage_is_rejected(self):
        for junk in ["", "hello", "NEED:red:2:42", "!!!", "QUJD"]:
            self.assertIsNone(self.worker.open(junk))


class HomeMessageTest(unittest.TestCase):
    def test_round_trip(self):
        home = Home(4, 33, {"linemate": 1, "deraumere": 0, "sibur": 2, "mendiane": 0, "phiras": 3, "thystame": 1})
        self.assertEqual(Home.decode(home.encode()), home)
        self.assertTrue(home.missing())

    def test_rejects_malformed(self):
        for text in ["", "HOME|1|2", "HOME|x|2|0,0,0,0,0,0", "HOME|1|2|0,0,0", "NEED|1|2|0,0,0,0,0,0"]:
            self.assertIsNone(Home.decode(text))


class HereMessageTest(unittest.TestCase):
    def test_round_trip(self):
        self.assertEqual(Here.decode(Here(123, 4).encode()), Here(123, 4))
        for text in ["", "HERE|1", "HERE|a|2", "HOME|1|2"]:
            self.assertIsNone(Here.decode(text))


class JammerTest(unittest.TestCase):
    def test_decoy_keeps_the_shape_and_changes_digits(self):
        jammer = Jammer(every=1, rng=random.Random(0))
        jammer.capture("NEED:blue:4:812345")
        decoy = jammer.next_decoy()
        self.assertIsNotNone(decoy)
        self.assertTrue(decoy.startswith("NEED:blue:"))
        self.assertNotEqual(decoy, "NEED:blue:4:812345")

    def test_jams_only_every_n_turns(self):
        jammer = Jammer(every=3, rng=random.Random(1))
        jammer.capture("pos 12 34")
        decoys = [jammer.next_decoy() for _ in range(6)]
        self.assertEqual([d is not None for d in decoys], [False, False, True, False, False, True])

    def test_disabled_jammer_never_jams(self):
        jammer = Jammer(every=1, enabled=False)
        jammer.capture("pos 12 34")
        self.assertIsNone(jammer.next_decoy())


if __name__ == "__main__":
    unittest.main()
