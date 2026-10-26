"""The player's memory."""

import random

"""The 7 game resources."""
RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

"""Elevation requirements table. To go from level N to level N+1, the SAME tile must hold:
the given number of level-N players and the given stone quantities."""
ELEVATION_REQ = {
    1: {"players": 1, "linemate": 1, "deraumere": 0, "sibur": 0, "mendiane": 0, "phiras": 0, "thystame": 0},
    2: {"players": 2, "linemate": 1, "deraumere": 1, "sibur": 1, "mendiane": 0, "phiras": 0, "thystame": 0},
    3: {"players": 2, "linemate": 2, "deraumere": 0, "sibur": 1, "mendiane": 0, "phiras": 2, "thystame": 0},
    4: {"players": 4, "linemate": 1, "deraumere": 1, "sibur": 2, "mendiane": 0, "phiras": 1, "thystame": 0},
    5: {"players": 4, "linemate": 1, "deraumere": 2, "sibur": 1, "mendiane": 3, "phiras": 0, "thystame": 0},
    6: {"players": 6, "linemate": 1, "deraumere": 2, "sibur": 3, "mendiane": 0, "phiras": 1, "thystame": 0},
    7: {"players": 6, "linemate": 2, "deraumere": 2, "sibur": 2, "mendiane": 2, "phiras": 2, "thystame": 1},
}


class Player:
    """Local state of ONE player, as known by the AI.
    This class only stores what the AI needs to decide what to do next
    turn: its level, its inventory, and a few coordination flags.
    """

    """Normal threshold for a player collecting stones.
    Below it, the player goes to find food."""
    FOOD_THRESHOLD = 18
    """Emergency threshold: the player goes for food immediately,
    whatever it is doing."""
    CRITICAL_FOOD = 12
    """Rally threshold: food level to reach before
    gathering for an elevation."""
    FOOD_RALLY_TARGET = 25
    """Max patience of a player locked at the rally point
    before giving up and going back for food."""
    LOCK_TIMEOUT = 60

    def __init__(self):
        # Current level: starts at 1.
        self.level = 1
        # Local inventory (all resources at 0 at start).
        self.inventory = {resource: 0 for resource in RESOURCES}

        # Step counter used to make the player change direction
        self.explore_steps = 0

        """Set to True below CRITICAL_FOOD=12,
        back to False only once FOOD_RALLY_TARGET=25 is reached.
        This keeps a player from leaving to eat every time it loses 1 food."""
        self.need_fuel = True
        
        """Whether the player is the anchor of the current level.
        The anchor is the player of the current level with the highest rally weight."""
        self.is_anchor = False

        """Whether the player is locked on the anchor's tile.
        The lock is released on every level change."""
        self.locked = False

        """Turns spent locked without seeing new players arrive.
        Reset to zero as soon as one more player arrives."""
        self.locked_ticks = 0
    
        """Number of players seen on the tile at the last Look."""
        self.here_count = 0

        """Number of players seen on the tile at the last Look
        that have the same level as the current player."""
        self.rally_seen = 0

        """Random draw used to elect the anchor of the current level."""
        self.rally_weight = random.randint(1, 1_000_000_000)

    def update_inventory(self, raw: str):
        """Update the local inventory from the server's answer.
        The server returns [food 12, linemate 3, deraumere 0, ...]
        Strip the brackets, split on commas; each piece is a
        "name quantity" pair stored in self.inventory."""
        raw = raw.strip("[] \n")
        for item in raw.split(","):
            parts = item.strip().split()
            if len(parts) == 2:
                name, qty = parts
                if name in self.inventory:
                    self.inventory[name] = int(qty)

    def missing_for_elevation(self) -> dict:
        """Compare the inventory with the current level's requirements (ELEVATION_REQ).
        Returns an empty dict when everything is ready."""
        if self.level >= 8:
            return {}
        req = ELEVATION_REQ[self.level]
        return {
            resource: req[resource] - self.inventory[resource]
            for resource in RESOURCES
            if resource != "food" and self.inventory[resource] < req.get(resource, 0)
        }

    def needs_food(self) -> bool:
        """True if the food reserve is below the normal survival threshold."""
        return self.inventory["food"] < self.FOOD_THRESHOLD
