"""Typed team messages and their text format (before encryption)."""
from dataclasses import dataclass, field

from logic.rules import STONES


@dataclass
class Home:
    """Sent by the queen every turn: where home is (the sound's direction), her level,
    her food, and the stones still missing in her stock for the climb to level 8."""
    level: int
    food: int
    deficit: dict = field(default_factory=dict)

    def encode(self) -> str:
        return "HOME|%d|%d|%s" % (self.level, self.food,
                                  ",".join(str(self.deficit.get(s, 0)) for s in STONES))

    @staticmethod
    def decode(text: str):
        parts = text.split("|")
        if len(parts) != 4 or parts[0] != "HOME":
            return None
        counts = parts[3].split(",")
        if len(counts) != len(STONES) or not all(c.isdigit() for c in counts):
            return None
        if not (parts[1].isdigit() and parts[2].isdigit()):
            return None
        return Home(int(parts[1]), int(parts[2]), dict(zip(STONES, map(int, counts))))

    def missing(self) -> bool:
        return any(n > 0 for n in self.deficit.values())


@dataclass
class Here:
    """Sent by a worker waiting at home: lets the queen count her own players.
    `Look` cannot tell teams apart, so an enemy on our tile would be counted too."""
    worker: int
    level: int

    def encode(self) -> str:
        return "HERE|%d|%d" % (self.worker, self.level)

    @staticmethod
    def decode(text: str):
        parts = text.split("|")
        if len(parts) != 3 or parts[0] != "HERE" or not (parts[1].isdigit() and parts[2].isdigit()):
            return None
        return Here(int(parts[1]), int(parts[2]))
