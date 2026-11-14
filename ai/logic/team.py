"""Keeping the team populated: who recruits, and how new players are started.

Rule respected: no game data is passed outside the server. A child process only
gets its launch role (ZAPPY_CHILD=1); free slots come from the server itself
(handshake and Connect_nbr).
"""
import os
import subprocess
import sys
import time

CHILD_ENV = "ZAPPY_CHILD"


def is_child_process() -> bool:
    return os.environ.get(CHILD_ENV) is not None


class ProcessSpawner:
    """Starts one more zappy_ai process for the team, and tracks the living ones."""

    def __init__(self, host: str, port: int, team: str, entry: str = None):
        self._args = ["-p", str(port), "-n", team, "-h", host]
        self._entry = entry or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "zappy_ai")
        self._children = []

    def spawn(self):
        env = dict(os.environ, **{CHILD_ENV: "1"})
        self._children.append(subprocess.Popen([sys.executable, self._entry, *self._args], env=env,
                                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))

    def alive(self) -> int:
        """Living children; a finished process is a dead (or refused) player."""
        self._children = [c for c in self._children if c.poll() is None]
        return len(self._children)


class Recruiter:
    """The queen's headcount policy: grow the team to TEAM_CAP players, and rebuild it
    only when it falls below REFORK_BELOW (no fork for every single death)."""

    TEAM_CAP = 9
    REFORK_BELOW = 7          # 1 queen + 6 workers: the quorum of levels 6 and 7
    COOLDOWN = 2.0            # seconds between two recruitments (an egg takes 42 units)

    def __init__(self, api, spawner: ProcessSpawner, clock=time.time):
        self._api = api
        self._spawner = spawner
        self._clock = clock
        self._free_slots = api.free_slots
        self._slots_used = 0
        self._next = 0.0
        self._rebuilding = True

    def tick(self):
        members = 1 + self._spawner.alive()
        if members < self.REFORK_BELOW:
            self._rebuilding = True
        elif members >= self.TEAM_CAP:
            self._rebuilding = False
        if not self._rebuilding or members >= self.TEAM_CAP or self._clock() < self._next:
            return
        if self._slots_used < self._free_slots:
            self._slots_used += 1
            self._spawner.spawn()
        elif self._api.connect_nbr() > 0:
            # Only start a child when the server has a slot or an egg for it,
            # otherwise it would be refused.
            self._spawner.spawn()
        else:
            self._api.fork()
        self._next = self._clock() + self.COOLDOWN
