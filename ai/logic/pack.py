"""Pack strategy: the whole team climbs from level 2 to level 8 together, on one tile.

Why: incantations elevate every same-level player standing on the tile. If the team
climbs by pairs it splits into small groups of different levels that can never reach
the quorum of the higher levels (4 then 6 players). Here the team never splits:

1. Every player reaches level 2 alone (one linemate, solo incantation).
2. The queen settles on a tile ("home") and never moves again. She broadcasts
   HOME:<team>:<level>:<food>:<deficit> every turn: her level, her food, and the stones
   still missing in her stock for the whole climb from her level to level 8.
3. Workers stock food, collect the missing stones, walk home by following the most
   recent HOME sound, and drop their stones there. The queen picks them up: the stock
   stays in her inventory, where no other team can take it.
4. When enough players stand at home and the stock covers the whole climb, the queen
   alone drops the stones of the current level and incants, level after level. Workers
   never incant with her, so incantations do not overlap.
"""

from network.broadcast import decode, encode
from network.client import Client
from network.parser import parse_look
from logic.player import Player, ELEVATION_REQ
from logic.fork import TeamManager
from logic.states.survive import survive, _go_to_tile
from logic.explore import explore

STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

# Moves toward a sound direction (1 = front, then clockwise, see the server's
# broadcastDirection). Diagonals take both steps.
DIRECTION_MOVES = {
    1: ["Forward"],
    2: ["Forward", "Right", "Forward"],
    3: ["Right", "Forward"],
    4: ["Right", "Right", "Forward"],
    5: ["Right", "Right", "Forward"],
    6: ["Left", "Left", "Forward"],
    7: ["Left", "Forward"],
    8: ["Forward", "Left", "Forward"],
}

# Players needed at home before the climb starts. 6 must reach level 8 to win;
# one extra gives a margin against a death during the climb.
GROUP_TARGET = 7
GROUP_MIN = 6
# Queen turns to wait at GROUP_MIN before starting without the extra player.
GROUP_WAIT_TURNS = 40

QUEEN_FOOD_SETTLE = 40    # food the queen gathers before settling at home
QUEEN_FOOD_LOW = 25       # below this, workers at home give her food
QUEEN_FOOD_LEAVE = 6      # last resort: the queen leaves home to eat
WORKER_FOOD_GO = 30       # food a worker stocks before walking home
WORKER_FOOD_KEEP = 20     # a worker gives food to the queen only above this
WORKER_FOOD_CRITICAL = 8  # below this, a worker leaves home to eat


def climb_need(level: int) -> dict:
    """Stones needed for every incantation from `level` up to level 8."""
    need = {s: 0 for s in STONES}
    for lvl in range(max(level, 2), 8):
        for s in STONES:
            need[s] += ELEVATION_REQ[lvl][s]
    return need


def _incant(client: Client) -> int:
    """Start an incantation and wait for its end. Returns the new level, or 0 on failure."""
    start = client.send("Incantation", is_incantation=True)
    if start == "ko":
        return 0
    end = start if start.startswith("Current level") else client.receive(is_incantation=True)
    if end.startswith("Current level"):
        try:
            return int(end.split(":")[1])
        except (IndexError, ValueError):
            return 0
    return 0


def _fetch(client: Client, wanted: dict) -> bool:
    """Walk to the nearest visible tile holding a wanted item and take it (and its food).
    Returns False when nothing wanted is in sight (the caller explores)."""
    tiles = parse_look(client.send("Look"))
    for index, tile in enumerate(tiles):
        useful = [r for r in wanted if wanted[r] > 0 and r in tile]
        if not useful:
            continue
        _go_to_tile(client, index)
        for r in useful:
            for _ in range(min(tile.count(r), wanted[r])):
                client.send(f"Take {r}")
        if "food" not in useful:
            for _ in range(tile.count("food")):
                client.send("Take food")
        return True
    return False


class PackBrain:
    def __init__(self, client: Client):
        self.client = client
        self.player = Player()
        self.team = TeamManager(client)

    def _inventory(self):
        self.player.update_inventory(self.client.send("Inventory"))
        lvl = self.client.get_level_update()
        if lvl is not None:
            self.player.level = lvl

    def _solo_level_up(self):
        """Level 1 -> 2: get one linemate on our tile and incant alone."""
        if self.player.inventory["linemate"] < 1:
            if not _fetch(self.client, {"linemate": 1}):
                explore(self.client, self.player)
            return
        tiles = parse_look(self.client.send("Look"))
        if "linemate" not in tiles[0]:
            self.client.send("Set linemate")
        lvl = _incant(self.client)
        if lvl:
            self.player.level = lvl

    def run(self):
        if self.team.is_recruiter:
            self._run_queen()
        else:
            self._run_worker()

    # ----- queen --------------------------------------------------------------------

    def _run_queen(self):
        settled = False
        ready_turns = 0
        while True:
            self.team.tick()
            self._inventory()
            inv = self.player.inventory
            if self.player.level >= 8:
                if inv["food"] < 10:
                    survive(self.client, self.player)
                else:
                    self.client.send("Look")
                continue
            if not settled:
                if inv["food"] < QUEEN_FOOD_SETTLE:
                    survive(self.client, self.player)
                elif self.player.level < 2:
                    self._solo_level_up()
                else:
                    settled = True
                continue
            if inv["food"] < QUEEN_FOOD_LEAVE:
                # Better to move home than to die: the workers follow the sound anyway.
                survive(self.client, self.player)
                continue

            deficit = {s: max(0, n - inv[s]) for s, n in climb_need(self.player.level).items()}
            self.client.send("Broadcast " + encode(
                "HOME", f"{self.client.team}:{self.player.level}:{inv['food']}:"
                + ",".join(str(deficit[s]) for s in STONES)))

            # Pick up everything dropped at home: stones into the stock, food to eat.
            tiles = parse_look(self.client.send("Look"))
            here = tiles[0] if tiles else []
            for item in set(here):
                if item == "food" or item in STONES:
                    for _ in range(here.count(item)):
                        self.client.send(f"Take {item}")
            players_here = here.count("player")

            if any(deficit.values()):
                ready_turns = 0
            elif players_here >= GROUP_TARGET:
                self._climb_one_level()
            elif players_here >= GROUP_MIN:
                ready_turns += 1
                if ready_turns >= GROUP_WAIT_TURNS:
                    self._climb_one_level()
            else:
                ready_turns = 0

    def _climb_one_level(self):
        """Drop exactly the current level's stones, incant, take them back on failure."""
        req = ELEVATION_REQ[self.player.level]
        dropped = []
        for s in STONES:
            for _ in range(req[s]):
                self.client.send(f"Set {s}")
                dropped.append(s)
        lvl = _incant(self.client)
        if lvl:
            self.player.level = lvl
            return
        for s in dropped:
            self.client.send(f"Take {s}")

    # ----- worker -------------------------------------------------------------------

    def _home_signal(self):
        """Latest HOME message of our team: (direction, queen level, queen food, deficit) or None."""
        latest = None
        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if not parsed or parsed[1] != "HOME":
                continue
            parts = parsed[2].split(":")
            if len(parts) != 4 or parts[0] != self.client.team:
                continue
            try:
                deficit = dict(zip(STONES, (int(x) for x in parts[3].split(","))))
                latest = (parsed[0], int(parts[1]), int(parts[2]), deficit)
            except ValueError:
                continue
        return latest

    def _run_worker(self):
        home = None  # last HOME signal heard
        while True:
            self._inventory()
            inv = self.player.inventory
            signal = self._home_signal()
            if signal:
                home = signal
            if self.player.level < 2:
                if inv["food"] < 10:
                    survive(self.client, self.player)
                else:
                    self._solo_level_up()
                continue
            if home is None:
                # Queen not heard yet: stock food while waiting.
                if inv["food"] < WORKER_FOOD_GO + 10:
                    survive(self.client, self.player)
                else:
                    explore(self.client, self.player)
                continue

            direction, queen_level, queen_food, deficit = home
            if direction == 0 and signal is None:
                # At home but no fresh news yet (for example right after a level up):
                # stay put rather than act on an outdated queen level.
                self.client.send("Look")
                continue
            in_group = queen_level == self.player.level

            if direction == 0:
                self._at_home(in_group, queen_food, deficit)
                continue
            if inv["food"] < WORKER_FOOD_CRITICAL:
                survive(self.client, self.player)
                continue
            if not in_group:
                # Too late to join the climb: help with stones and keep alive.
                self._collect_or_eat(deficit)
                continue
            carried = any(inv[s] > 0 and deficit.get(s, 0) > 0 for s in STONES)
            missing = any(n > 0 for n in deficit.values())
            if inv["food"] < WORKER_FOOD_GO:
                survive(self.client, self.player)
            elif missing and not carried:
                self._collect_or_eat(deficit)
            elif signal is not None:
                # Walk toward home using only a FRESH direction (heard after our last move),
                # then drop the sounds heard during the walk: they are relative to an old
                # position and orientation.
                self.client.send_batch(DIRECTION_MOVES[direction])
                self.client.get_broadcasts()
                home = (None, queen_level, queen_food, deficit)
            else:
                self.client.send("Look")  # wait for the next HOME sound

    def _collect_or_eat(self, deficit: dict):
        wanted = {s: n for s, n in deficit.items() if n > 0}
        if self.player.inventory["food"] < WORKER_FOOD_GO:
            wanted["food"] = 3
        if not wanted or not _fetch(self.client, wanted):
            explore(self.client, self.player)

    def _at_home(self, in_group: bool, queen_food: int, deficit: dict):
        inv = self.player.inventory
        # Hand over every stone: the queen picks them up into her stock.
        for s in STONES:
            for _ in range(inv[s]):
                self.client.send(f"Set {s}")
        if queen_food < QUEEN_FOOD_LOW and inv["food"] > WORKER_FOOD_KEEP:
            for _ in range(min(5, inv["food"] - WORKER_FOOD_KEEP)):
                self.client.send("Set food")
        if inv["food"] < WORKER_FOOD_CRITICAL:
            survive(self.client, self.player)
        elif in_group and not any(deficit.values()):
            self.client.send("Look")  # wait for the climb; frozen during incantations
        elif in_group and inv["food"] < WORKER_FOOD_GO:
            survive(self.client, self.player)
        else:
            self._collect_or_eat(deficit)
