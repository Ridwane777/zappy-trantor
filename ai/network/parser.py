def parse_look(raw: str) -> list:
    raw = raw.strip("[] \n")
    tiles = []
    for tile in raw.split(","):
        tile = tile.strip()
        items = tile.split() if tile else []
        tiles.append(items)
    return tiles
