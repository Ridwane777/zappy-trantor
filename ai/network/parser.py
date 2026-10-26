def parse_look(raw: str) -> list:
    """Turn the answer of the "Look" command into a list of tiles.

    The server returns the field of view as a bracketed list, tiles
    separated by commas and a tile's content separated by spaces. Example:

        [ player, food, , linemate food, deraumere ]
          ^tile0  ^tile1 ^tile2 ^tile3        ^tile4

    We turn it into a list of lists (one sub-list per tile, holding the
    names of the objects present):

        [["player"], ["food"], [], ["linemate", "food"], ["deraumere"]]

    Tile 0 is always the one the player stands on. An empty
    tile gives an empty sub-list.
    """
    # Strip the brackets and surrounding spaces/newlines.
    raw = raw.strip("[] \n")

    tiles = []
    for tile in raw.split(","):
        tile = tile.strip()
        # tile.split() splits on spaces; an empty tile gives [].
        items = tile.split() if tile else []
        tiles.append(items)
    return tiles
