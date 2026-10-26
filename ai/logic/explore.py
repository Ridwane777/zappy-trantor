import random

"""Player exploration module."""

from network.client import Client
from logic.player import Player


def explore(client: Client, player: Player):
    """Exploration move when nothing useful is in sight.
    Goal: cover ground until food or stones show up,
    without crossing the map in a straight line.
    Method: always move forward, and every 3 steps turn left or right at random.
    The resulting zigzag covers the map better than a straight line.
    """
    player.explore_steps += 1
    commands = []
    # Every 3 steps, change heading (left or right at random).
    if player.explore_steps % 3 == 0:
        commands.append(random.choice(["Left", "Right"]))
    commands.append("Forward")
    # Optional turn + forward, sent in a single batch.
    client.send_batch(commands)
