"""Pack strategy: the whole team climbs from level 2 to level 8 together, on one tile.

Why: an incantation elevates every same-level player on its tile. A team that climbs
by pairs splits into small groups of different levels that never reach the quorum
of the higher levels (4, then 6 players). Here the team never splits:

1. Every player reaches level 2 alone (one linemate, solo incantation).
2. The queen settles on a tile ("home") and never moves again. Every turn she
   broadcasts an encrypted Home message: her level, her food, and the stones still
   missing in her stock for the whole climb to level 8.
3. Workers stock food, collect the missing stones, walk home by following the most
   recent Home sound, and drop their stones there. The queen picks them up, so the
   stock stays in her inventory where no other team can take it.
4. When enough players stand at home and the stock covers the whole climb, the queen
   alone drops the stones of the current level and incants, level after level.
"""
from comm.jammer import Jammer
import secrets

from comm.messages import Here, Home
from comm.secure import SecureChannel
from logic.navigation import DIRECTION_MOVES, Explorer, path_to_tile
from logic.player import Player
from logic.rules import ELEVATION_REQ, MAX_LEVEL, STONES, climb_need
from logic.team import Recruiter
from network.game_api import GameApi


class PackAgent:
    """What the queen and the workers share: inventory refresh, fetching, level 1 -> 2."""

    def __init__(self, api: GameApi, channel: SecureChannel, explorer: Explorer = None):
        self.api = api
        self.channel = channel
        self.player = Player()
        self.explorer = explorer or Explorer()

    def refresh(self):
        self.player.update_inventory(self.api.inventory())
        self.player.reach_level(self.api.announced_level())

    def explore(self):
        self.api.walk(self.explorer.next_moves())

    def fetch(self, wanted: dict) -> bool:
        """Walks to the nearest visible tile holding a wanted item and takes it (and its
        food). False when nothing wanted is in sight."""
        tiles = self.api.look()
        for index, tile in enumerate(tiles):
            useful = [item for item, n in wanted.items() if n > 0 and item in tile]
            if not useful:
                continue
            self.api.walk(path_to_tile(index))
            for item in useful:
                for _ in range(min(tile.count(item), wanted[item])):
                    self.api.take(item)
            if "food" not in useful:
                for _ in range(tile.count("food")):
                    self.api.take("food")
            return True
        return False

    def fetch_or_explore(self, wanted: dict):
        if not wanted or not self.fetch(wanted):
            self.explore()

    def eat(self):
        self.fetch_or_explore({"food": 99})

    def solo_level_up(self):
        """Level 1 -> 2: get one linemate on our tile and incant alone."""
        if self.player.count("linemate") < 1:
            self.fetch_or_explore({"linemate": 1})
            return
        if "linemate" not in self.api.look()[0]:
            self.api.drop("linemate")
        self.player.reach_level(self.api.incant())


class PackQueen(PackAgent):
    SETTLE_FOOD = 40     # food gathered before settling at home
    LEAVE_FOOD = 6       # last resort: the queen leaves home to eat
    GROUP_TARGET = 7     # players at home to start the climb (6 must reach level 8, +1 margin)
    GROUP_MIN = 6
    GROUP_WAIT_TURNS = 40
    ROLL_CALL_TURNS = 3  # a worker counts as present if heard at home within this many turns

    def __init__(self, api: GameApi, channel: SecureChannel, recruiter: Recruiter,
                 jammer: Jammer = None, explorer: Explorer = None):
        super().__init__(api, channel, explorer)
        self.recruiter = recruiter
        self.jammer = jammer or Jammer()
        self.settled = False
        self.ready_turns = 0
        self.turns = 0
        self.present = {}   # worker id -> last turn it said Here from our tile, at our level

    def run(self):
        while True:
            self.turn()

    def turn(self):
        self.turns += 1
        self.recruiter.tick()
        self.refresh()
        self.listen()
        if self.player.level >= MAX_LEVEL:
            self.eat() if self.player.food < 10 else self.api.look()
            return
        if not self.settled:
            if self.player.food < self.SETTLE_FOOD:
                self.eat()
            elif self.player.level < 2:
                self.solo_level_up()
            else:
                self.settled = True
            return
        if self.player.food < self.LEAVE_FOOD:
            self.eat()   # better to move home than to die: workers follow the sound anyway
            return
        deficit = self.deficit()
        self.api.broadcast(self.channel.seal(Home(self.player.level, self.player.food, deficit).encode()))
        self.jam()
        self.collect_home()
        self.maybe_climb(deficit, self.team_at_home())

    def listen(self):
        """Authentic Here messages from our tile make the roll call; messages that do
        not authenticate come from other teams and feed the jammer."""
        for direction, text in self.api.messages():
            plain = self.channel.open(text)
            if plain is None:
                self.jammer.capture(text)
                continue
            here = Here.decode(plain)
            if here and direction == 0 and here.level == self.player.level:
                self.present[here.worker] = self.turns

    def team_at_home(self) -> int:
        """The queen plus the workers that answered the roll call recently."""
        recent = [w for w, turn in self.present.items() if self.turns - turn <= self.ROLL_CALL_TURNS]
        return 1 + len(recent)

    def jam(self):
        decoy = self.jammer.next_decoy()
        if decoy:
            self.api.broadcast(decoy)

    def deficit(self) -> dict:
        return {s: max(0, n - self.player.count(s)) for s, n in climb_need(self.player.level).items()}

    def collect_home(self):
        """Picks up what workers dropped at home."""
        here = self.api.look()[0]
        for item in set(here):
            if item == "food" or item in STONES:
                for _ in range(here.count(item)):
                    self.api.take(item)

    def maybe_climb(self, deficit: dict, players_here: int):
        if any(deficit.values()):
            self.ready_turns = 0
        elif players_here >= self.GROUP_TARGET:
            self.climb_one_level()
        elif players_here >= self.GROUP_MIN:
            self.ready_turns += 1
            if self.ready_turns >= self.GROUP_WAIT_TURNS:
                self.climb_one_level()
        else:
            self.ready_turns = 0

    def climb_one_level(self):
        """Drops exactly the current level's stones and incants; takes them back on failure."""
        req = ELEVATION_REQ[self.player.level]
        dropped = [s for s in STONES for _ in range(req[s])]
        for stone in dropped:
            self.api.drop(stone)
        level = self.api.incant()
        if level:
            self.player.reach_level(level)
            self.present.clear()   # workers answer again at their new level
            return
        for stone in dropped:
            self.api.take(stone)


class PackWorker(PackAgent):
    GO_HOME_FOOD = 30   # food stocked before walking home
    KEEP_FOOD = 20      # food kept when giving some to a hungry queen
    CRITICAL_FOOD = 8   # below this, leave home to eat
    QUEEN_LOW_FOOD = 25

    def __init__(self, api: GameApi, channel: SecureChannel, explorer: Explorer = None):
        super().__init__(api, channel, explorer)
        self.home = None            # last Home message from the queen
        self.home_direction = None  # its direction when heard; None once we moved since
        self.worker_id = secrets.randbits(31)

    def run(self):
        while True:
            self.turn()

    def turn(self):
        self.refresh()
        fresh = self.hear_queen()
        if self.player.level < 2:
            self.eat() if self.player.food < 10 else self.solo_level_up()
            return
        if self.home is None:
            # Queen not heard yet: stock food while waiting.
            self.eat() if self.player.food < self.GO_HOME_FOOD + 10 else self.explore()
            return
        if self.home_direction == 0:
            if fresh:
                self.at_home()
            else:
                # At home but no fresh news (for example right after a level up):
                # stay rather than act on an outdated queen level.
                self.api.look()
            return
        self.away_from_home(fresh)

    def hear_queen(self) -> bool:
        """Keeps the latest authentic Home message. True if one arrived this turn."""
        fresh = False
        for direction, text in self.api.messages():
            plain = self.channel.open(text)
            home = Home.decode(plain) if plain else None
            if home:
                self.home, self.home_direction, fresh = home, direction, True
        return fresh

    def in_group(self) -> bool:
        return self.home.level == self.player.level

    def away_from_home(self, fresh: bool):
        if self.player.food < self.CRITICAL_FOOD:
            self.eat()
        elif not self.in_group():
            self.collect(self.home.deficit)   # too late to join the climb: help and stay alive
        elif self.player.food < self.GO_HOME_FOOD:
            self.eat()
        elif self.home.missing() and not self.player.carries_any(self.home.deficit):
            self.collect(self.home.deficit)
        elif fresh:
            # Walk toward home using only a direction heard after our last move, then
            # forget the sounds heard while walking: they are relative to an old position.
            self.api.walk(DIRECTION_MOVES[self.home_direction])
            self.api.messages()
            self.home_direction = None
        else:
            self.api.look()   # wait for the next Home sound

    def collect(self, deficit: dict):
        wanted = {s: n for s, n in deficit.items() if n > 0}
        if self.player.food < self.GO_HOME_FOOD:
            wanted["food"] = 3
        self.fetch_or_explore(wanted)

    def at_home(self):
        # Hand over every stone: the queen picks them up into her stock.
        for stone, n in self.player.stones().items():
            for _ in range(n):
                self.api.drop(stone)
        if self.home.food < self.QUEEN_LOW_FOOD and self.player.food > self.KEEP_FOOD:
            for _ in range(min(5, self.player.food - self.KEEP_FOOD)):
                self.api.drop("food")
        if self.player.food < self.CRITICAL_FOOD:
            self.eat()
        elif self.in_group() and not self.home.missing():
            # Wait for the climb (frozen during incantations), answering the roll call.
            self.api.broadcast(self.channel.seal(Here(self.worker_id, self.player.level).encode()))
        elif self.in_group() and self.player.food < self.GO_HOME_FOOD:
            self.eat()
        else:
            self.collect(self.home.deficit)
