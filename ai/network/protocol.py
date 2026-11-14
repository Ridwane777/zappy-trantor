"""Parsing of the server's replies. Pure functions, no I/O."""


def parse_look(raw: str) -> list:
    """`[player food, , linemate]` -> [["player", "food"], [], ["linemate"]].
    Tile 0 is the player's own tile, then rows further and further ahead."""
    raw = raw.strip().strip("[]")
    return [tile.split() for tile in raw.split(",")]


def parse_inventory(raw: str) -> dict:
    """`[food 12, linemate 3]` -> {"food": 12, "linemate": 3}. Malformed items are skipped."""
    items = {}
    for item in raw.strip().strip("[]").split(","):
        parts = item.split()
        if len(parts) == 2 and parts[1].isdigit():
            items[parts[0]] = int(parts[1])
    return items


def parse_message(line: str):
    """`message K, text` -> (K, text), or None if the line is not a broadcast."""
    if not line.startswith("message "):
        return None
    head, sep, text = line[len("message "):].partition(",")
    if not sep or not head.strip().isdigit():
        return None
    return int(head.strip()), text.strip()


def parse_level(line: str):
    """`Current level: K` -> K, or None."""
    if not line.startswith("Current level:"):
        return None
    value = line.split(":", 1)[1].strip()
    return int(value) if value.isdigit() else None
