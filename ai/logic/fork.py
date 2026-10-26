import os
import sys
import time
import subprocess

from network.client import Client

# HARD cap of players per team: the queen never exceeds it.
TEAM_CAP = 9

# Replenishment threshold (hysteresis). The queen only re-forks when the
# headcount drops BELOW this threshold, then rebuilds up to the cap. This avoids
# forking for every single death. 7 is deliberate: 1 queen + 6 workers,
# i.e. the exact quorum of levels 6 and 7 (the most demanding).
REFORK_BELOW = 7

# Short cooldown between two player creations (time for the forked egg to hatch
# and the child to connect, so we do not spam during the Fork latency).
BUILD_FORK_COOLDOWN = 2.0


def _spawn(client: Client) -> subprocess.Popen:
    """Start a new zappy_ai process and return its handle (for supervision).
    The child gets the env variable ZAPPY_CHILD=1: it is only a launch ROLE
    (team reinforcement), not game data. It does NOT recruit (only the
    recruiter manages the headcount); it plays normally. No game info (team
    size, etc.) is passed, so the rule against out-of-game communication is
    respected. On connection, the server gives it a free initial slot, or
    hatches one of the team's eggs (Fork).
    """
    entry = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "zappy_ai")
    env = dict(os.environ, ZAPPY_CHILD="1")
    return subprocess.Popen(
        [
            sys.executable, entry,
            "-p", str(client.port),
            "-n", client.team,
            "-h", client.host,
        ],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


class TeamManager:
    """The QUEEN: builds and MAINTAINS the team headcount (one per team).
    The queen is the first player of the team to connect (the recruiter). She
    does not play, she supervises. She creates children (workers) to reach the
    headcount cap, then supervises them and replaces them when needed."""

    def __init__(self, client: Client):
        self.client = client
        # Recruiter = process started directly (not a _spawn child).
        self.is_recruiter = os.environ.get("ZAPPY_CHILD") is None
        # Handles of teammates created by THIS recruiter.
        self.children = []
        # Initial slots (-c) still free when we connected: that many
        # teammates can be created by simply connecting (no Fork).
        self.free_slots = client.free_slots if self.is_recruiter else 0
        self.slots_used = 0
        self.next_build = 0.0
        # Hysteresis: True = build/rebuild up to the cap. Starts as True
        # (team to build); goes back to True as soon as we drop below the threshold.
        self.rebuilding = True

    def tick(self):
        if not self.is_recruiter:
            return
        # Drop dead children (finished process = dead player).
        self.children = [c for c in self.children if c.poll() is None]
        # Current headcount = the queen (us) + the living workers we created.
        members = 1 + len(self.children)

        # Replenishment hysteresis: (re)build as soon as the headcount drops
        # below REFORK_BELOW, and stop once the TEAM_CAP cap is reached.
        # In between, keep the current state: no fork for every single death,
        # but a real replenishment when the team is too depleted.
        if members < REFORK_BELOW:
            self.rebuilding = True
        elif members >= TEAM_CAP:
            self.rebuilding = False

        # Nothing to do if not rebuilding, or if the cap is already reached.
        if not self.rebuilding or members >= TEAM_CAP:
            return
        if time.time() < self.next_build:
            return

        if self.slots_used < self.free_slots:
            # An initial slot is left: the child can connect to it right away.
            self.slots_used += 1
            self.children.append(_spawn(self.client))
        elif self.client.connect_nbr() > 0:
            # An egg (forked on a previous turn) is ready: start the child that will
            # hatch it. CRUCIAL: only start the child when a slot/egg exists,
            # otherwise the server answers "ko" and the process hangs (leak).
            self.children.append(_spawn(self.client))
        else:
            # Neither slot nor egg: lay an egg now; the child will hatch it
            # on a later tick, once the egg exists (Fork costs 42 ticks).
            self.client.send("Fork")
        self.next_build = time.time() + BUILD_FORK_COOLDOWN
