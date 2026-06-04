RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

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
    FOOD_THRESHOLD = 20

    def __init__(self):
        self.level = 1
        self.inventory = {r: 0 for r in RESOURCES}

    def update_inventory(self, raw: str):
        raw = raw.strip("[] \n")
        for item in raw.split(","):
            parts = item.strip().split()
            if len(parts) == 2:
                name, qty = parts
                if name in self.inventory:
                    self.inventory[name] = int(qty)

    def missing_for_elevation(self) -> dict:
        if self.level >= 8:
            return {}
        req = ELEVATION_REQ[self.level]
        return {
            r: max(0, req.get(r, 0) - self.inventory.get(r, 0))
            for r in RESOURCES
            if r != "food" and req.get(r, 0) > self.inventory.get(r, 0)
        }

    def needs_food(self) -> bool:
        return self.inventory["food"] < self.FOOD_THRESHOLD
