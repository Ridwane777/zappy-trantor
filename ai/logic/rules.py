"""Game rules the AI relies on (mirror of server/include/game/GameRules.hpp)."""

RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]
STONES = RESOURCES[1:]
MAX_LEVEL = 8

# To go from level N to N + 1, one tile must hold `players` players of level N
# and the given stones.
ELEVATION_REQ = {
    1: {"players": 1, "linemate": 1, "deraumere": 0, "sibur": 0, "mendiane": 0, "phiras": 0, "thystame": 0},
    2: {"players": 2, "linemate": 1, "deraumere": 1, "sibur": 1, "mendiane": 0, "phiras": 0, "thystame": 0},
    3: {"players": 2, "linemate": 2, "deraumere": 0, "sibur": 1, "mendiane": 0, "phiras": 2, "thystame": 0},
    4: {"players": 4, "linemate": 1, "deraumere": 1, "sibur": 2, "mendiane": 0, "phiras": 1, "thystame": 0},
    5: {"players": 4, "linemate": 1, "deraumere": 2, "sibur": 1, "mendiane": 3, "phiras": 0, "thystame": 0},
    6: {"players": 6, "linemate": 1, "deraumere": 2, "sibur": 3, "mendiane": 0, "phiras": 1, "thystame": 0},
    7: {"players": 6, "linemate": 2, "deraumere": 2, "sibur": 2, "mendiane": 2, "phiras": 2, "thystame": 1},
}


def climb_need(level: int) -> dict:
    """Stones needed for every incantation from `level` (at least 2) up to level 8."""
    need = {s: 0 for s in STONES}
    for lvl in range(max(level, 2), MAX_LEVEL):
        for s in STONES:
            need[s] += ELEVATION_REQ[lvl][s]
    return need
