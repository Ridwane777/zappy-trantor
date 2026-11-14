"""What the AI knows about its own player."""
from logic.rules import MAX_LEVEL, RESOURCES, STONES


class Player:
    def __init__(self):
        self._level = 1
        self._inventory = {r: 0 for r in RESOURCES}

    @property
    def level(self) -> int:
        return self._level

    def reach_level(self, level: int):
        """Records a level announced by the server (never goes down, never above 8)."""
        if level and self._level < level <= MAX_LEVEL:
            self._level = level

    def count(self, item: str) -> int:
        return self._inventory.get(item, 0)

    @property
    def food(self) -> int:
        return self.count("food")

    def update_inventory(self, items: dict):
        """Replaces the known inventory with the server's answer (the source of truth)."""
        for r in RESOURCES:
            self._inventory[r] = items.get(r, 0)

    def carries_any(self, wanted: dict) -> bool:
        """True if we hold at least one stone that `wanted` asks for."""
        return any(self.count(s) > 0 and wanted.get(s, 0) > 0 for s in STONES)

    def stones(self) -> dict:
        return {s: self.count(s) for s in STONES}
