"""Attack on the other teams' communications.

The queen hears every broadcast. Messages she cannot authenticate come from other
teams: she keeps the most recent ones and, from time to time, broadcasts a corrupted
copy (same shape, digits replaced). A team that talks in clear text, such as
"NEED:team:4:812", then receives believable but false levels, weights or positions:
followers head to the wrong place or wait for players that do not exist.
Our own messages are authenticated, so this cannot fool our team.
"""
import random
from collections import deque


class Jammer:
    def __init__(self, every: int = 3, memory: int = 20, rng: random.Random = None, enabled: bool = True):
        self._enabled = enabled
        self._captured = deque(maxlen=memory)
        self._every = every
        self._turn = 0
        self._rng = rng or random.Random()

    def capture(self, text: str):
        """Records a message that did not come from our team."""
        if text and self._enabled:
            self._captured.append(text)

    def next_decoy(self):
        """A corrupted enemy message to broadcast this turn, or None."""
        self._turn += 1
        if not self._captured or self._turn % self._every:
            return None
        original = self._rng.choice(self._captured)
        decoy = self.corrupt(original)
        return decoy if decoy != original else None

    def corrupt(self, text: str) -> str:
        return "".join(str(self._rng.randint(0, 9)) if c.isdigit() else c for c in text)
