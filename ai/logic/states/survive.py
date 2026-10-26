import math

from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.explore import explore


def _go_to_tile(client: Client, index: int):
    """Look numbers tiles in a cone (0=player, row r = r² to (r+1)²-1).
    r = int(√index) -> rows to move forward; col = index - r*(r+1) -> left/right offset."""

    if index == 0:
        return  # already on the target tile
    r = int(math.sqrt(index)) # row number = steps forward
    col = index - r * (r + 1) # signed lateral offset from the center
    commands = ["Forward"] * r
    if col < 0: # tile left of center
        commands.append("Left")
        commands.extend(["Forward"] * abs(col))
    elif col > 0:  # tile right of center
        commands.append("Right")
        commands.extend(["Forward"] * col)

    # Send the whole path at once (the server queues up to 10 commands),
    # faster than waiting for the answer after each step.
    client.send_batch(commands)


def survive(client: Client, player: Player):
    """Find food and pick it up: look around,
    go to the closest food tile and take ALL the food on it.
    If no food is in sight, explore to find some."""
    raw = client.send("Look") # returns a string describing the tiles around the player
    tiles = parse_look(raw)

    # Walk tiles from nearest to farthest and stop at the first one with food.
    for index, tile in enumerate(tiles):
        count = tile.count("food")
        if count:
            _go_to_tile(client, index)
            for _ in range(count):
                client.send("Take food")
            return
    # Nothing to eat in sight: explore other tiles.
    explore(client, player)
