def parse_look(raw: str) -> list[list[str]]:
    raw = raw.strip("[] \n")
    tiles = []
    for tile in raw.split(","):
        items = tile.strip().split()
        tiles.append(items)
    return tiles


def parse_inventory(raw: str) -> dict:
    raw = raw.strip("[] \n")
    result = {}
    for item in raw.split(","):
        parts = item.strip().split()
        if len(parts) == 2:
            result[parts[0]] = int(parts[1])
    return result
