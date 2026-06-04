from network.client import Client
from logic.player import Player
from network.parser import parse_look


def collect(client: Client, player: Player):
    missing = player.missing_for_elevation()
    if not missing:
        return

    raw = client.send("Look")
    tiles = parse_look(raw)

    for resource in missing:
        for i, tile in enumerate(tiles):
            if resource in tile:
                _go_to_tile(client, i, player.level)
                client.send(f"Take {resource}")
                return

    client.send("Forward")


def _go_to_tile(client: Client, index: int, level: int):
    if index == 0:
        return
    row = 0
    count = 0
    for r in range(level + 1):
        if count + (2 * r + 1) > index:
            row = r
            break
        count += 2 * r + 1
    col = index - count - row
    for _ in range(row):
        client.send("Forward")
    if col < 0:
        client.send("Left")
        for _ in range(abs(col)):
            client.send("Forward")
    elif col > 0:
        client.send("Right")
        for _ in range(col):
            client.send("Forward")
